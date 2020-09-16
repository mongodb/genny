// Copyright 2019-present MongoDB Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#ifndef HEADER_960919A5_5455_4DD2_BC68_EFBAEB228BB0_INCLUDED
#define HEADER_960919A5_5455_4DD2_BC68_EFBAEB228BB0_INCLUDED

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <queue>
#include <set>
#include <thread>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>

#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include <metrics/operation.hpp>
#include <poplarlib/collector.grpc.pb.h>

/**
 * @namespace genny::metrics::internals::v2 this namespace is private and only intended to be used
 * by genny's own internals. No types from the genny::metrics::v2 namespace should ever be typed
 * directly into the implementation of an actor.
 */

namespace genny::metrics {

template <typename Clocksource>
class OperationEventT;

namespace internals::v2 {

const int NUM_CHANNELS = 4;
const int BUFFER_SIZE = 1000;
const int CLIENT_THREADS = 3;
const int GRPC_THREAD_SLEEP_MS = 50;
const double SWAP_BUFFER_PERCENT = .25;

class PoplarRequestError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class MetricsError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};


/**
 * Wraps the channel-owning gRPC stub.
 *
 * RAII class that exists for construction / destruction resource management during
 * setup/teardown execution phase only so efficiency isn't as much of a concern as correctness.
 */
class CollectorStubInterface {
public:
    CollectorStubInterface() {
        setStub();
    }

    poplar::PoplarEventCollector::StubInterface* operator->() {
        return _stub.get();
    }

private:

    std::unique_ptr<poplar::PoplarEventCollector::StubInterface> _stub;

    // Should only be called by setStub(), is statically guarded
    // so only one thread will execute.
    auto createChannels() {
        std::vector<std::shared_ptr<grpc::Channel>> _channels;

        grpc::ChannelArguments args;
        // The BDP estimator overwhelms the server with pings on a heavy workload.
        args.SetInt(GRPC_ARG_HTTP2_BDP_PROBE, 0);
        // Maximum buffer size grpc will allow.
        args.SetInt(GRPC_ARG_HTTP2_WRITE_BUFFER_SIZE, 67108864);
        // Local subchannels prohibit global sharing and forces multiple TCP connections.
        args.SetInt(GRPC_ARG_USE_LOCAL_SUBCHANNEL_POOL, 1);
        for (int i = 0; i < NUM_CHANNELS; i++) {
            _channels.push_back(grpc::CreateCustomChannel(
                "localhost:2288", grpc::InsecureChannelCredentials(), args));
        }

        return _channels;
    }

    void setStub() {
        static auto _channels = createChannels();
        static std::atomic<size_t> _curChannel = 0;

        _stub = poplar::PoplarEventCollector::NewStub(_channels[_curChannel++ % _channels.size()]);
        return;
    }
};


/**
 * Manages the stream of poplar EventMetrics.
 *
 * RAII class that exists for construction / destruction resource management during
 * setup/teardown execution phase only so efficiency isn't as much of a concern as correctness.
 */
class StreamInterfaceImpl {
public:
    StreamInterfaceImpl(const std::string& name, const ActorId& actorId)
        : _name{name},
          _actorId{actorId},
          _inFlight{true},
          _options{},
          _response{},
          _context{},
          _cq{},
          // This is used by the gRPC system to distinguish calls.
          // We only ever have 1 message in flight at a time, so it doesn't matter to us.
          _grpcTag{(void*)1},
          _stream{_stub->AsyncStreamEvents(&_context, &_response, &_cq, _grpcTag)} {
        _options.set_no_compression().set_buffer_hint();
        finishCall();  // We expect a response from the initial construction.
    }

    void write(const poplar::EventMetrics& event) {
        if (!finishCall()) {
            std::ostringstream os;
            os << "Failed to write to stream for operation name " << _name << " and actor ID "
               << _actorId << ". EventMetrics object: " << event.ShortDebugString();

            BOOST_THROW_EXCEPTION(PoplarRequestError(os.str()));
        }

        _stream->Write(event, _options, _grpcTag);
        _inFlight = true;
    }

    ~StreamInterfaceImpl() {
        if (!_stream) {
            BOOST_LOG_TRIVIAL(error) << "Tried to close gRPC stream for operation name " << _name
                                     << " and actor ID " << _actorId << ", but no stream existed.";
            return;
        }
        if (!finishCall()) {
            BOOST_LOG_TRIVIAL(warning)
                << "Closing gRPC stream for operation name " << _name << " and actor ID "
                << _actorId << ", but not all writes completed.";
        }

        _stream->WritesDone(_grpcTag);
        if (!finishCall()) {
            BOOST_LOG_TRIVIAL(warning) << "Failed to write to stream for operation name " << _name
                                       << " and actor ID " << _actorId << ".";
        }

        grpc::Status status;
        _stream->Finish(&status, _grpcTag);
        if (!finishCall()) {
            BOOST_LOG_TRIVIAL(error) << "Failed to finish writes to stream for operation name "
                                     << _name << " and actor ID " << _actorId << ".";
            return;
        }
        if (!status.ok()) {
            BOOST_LOG_TRIVIAL(error)
                << "Problem closing grpc stream for operation name " << _name << " and actor ID "
                << _actorId << ": " << _context.debug_error_string();
        }

        shutdownQueue();
    }

private:
    bool finishCall() {
        if (_inFlight) {
            void* gotTag;
            bool ok = false;
            _cq.Next(&gotTag, &ok);

            _inFlight = false;
            // Basic sanity check that the returned tag is expected.
            // (and ok status).
            return gotTag == _grpcTag && ok;
        }
        return true;
    }

    void shutdownQueue() {
        _cq.Shutdown();
        void* gotTag;
        bool ok = false;

        // Flush the queue.
        while (_cq.Next(&gotTag, &ok)) {
        };
    }

    std::string _name;
    ActorId _actorId;
    bool _inFlight;
    CollectorStubInterface _stub;
    grpc::WriteOptions _options;
    poplar::PoplarResponse _response;
    grpc::ClientContext _context;
    grpc::CompletionQueue _cq;
    void* _grpcTag;
    std::unique_ptr<grpc::ClientAsyncWriterInterface<poplar::EventMetrics>> _stream;
};


/**
 * Manages the gRPC-side collector for each operation.
 *
 * RAII class that exists for construction / destruction resource management during
 * setup/teardown execution phase only so efficiency isn't as much of a concern as correctness.
 */
class Collector {
public:
    Collector(const Collector&) = delete;

    explicit Collector(const std::string& name, const boost::filesystem::path& pathPrefix)
        : _name{name}, _id{} {
        _id.set_name(_name);

        grpc::ClientContext context;
        poplar::PoplarResponse response;
        poplar::CreateOptions options = createOptions(_name, pathPrefix.string());
        auto status = _stub->CreateCollector(&context, options, &response);

        if (!status.ok()) {
            std::ostringstream os;
            os << "Collector " << _name << " status not okay: " << status.error_message();
            BOOST_THROW_EXCEPTION(PoplarRequestError(os.str()));
        }
    }

    ~Collector() {
        grpc::ClientContext context;
        poplar::PoplarResponse response;
        auto status = _stub->CloseCollector(&context, _id, &response);
        if (!status.ok()) {
            BOOST_LOG_TRIVIAL(error)
                << "Couldn't close collector " << _name << ": " << status.error_message();
        }
    }


private:
    static auto createPath(const std::string& name, const boost::filesystem::path& pathPrefix) {
        std::stringstream str;
        str << name << ".ftdc";
        return (pathPrefix / boost::filesystem::path(str.str())).string();
    }

    static poplar::CreateOptions createOptions(const std::string& name,
                                               const boost::filesystem::path& pathPrefix) {
        poplar::CreateOptions options;
        options.set_name(name);
        options.set_events(poplar::CreateOptions_EventsCollectorType_BASIC);
        options.set_path(createPath(name, pathPrefix));
        options.set_chunksize(1000);
        options.set_streaming(true);
        options.set_dynamic(false);
        options.set_recorder(poplar::CreateOptions_RecorderType_PERF);
        options.set_events(poplar::CreateOptions_EventsCollectorType_BASIC);
        return options;
    }

    std::string _name;
    poplar::PoplarID _id;
    CollectorStubInterface _stub;
};

template <typename Clocksource, typename StreamInterface>
class EventStream;

// Manages a thread of grpc client execution .
template <typename ClockSource, typename StreamInterface>
class GrpcThread {
public:
    GrpcThread() : _thread{&GrpcThread::run, this} {}

    typedef EventStream<ClockSource, StreamInterface>* StreamPtr;

    void registerStream(StreamPtr stream) {
        const std::lock_guard<std::mutex> lock(_streamsMutex);
        streams.insert(stream);
    }

    void closeStream(StreamPtr stream) {
        const std::lock_guard<std::mutex> lock(_streamsMutex);
        streams.erase(stream);
    }

    ~GrpcThread() {
        done = true;
        _thread.join();
    }

private:
    void run() {
        while (!done || !streams.empty()) {
            reapActors();
            std::this_thread::sleep_for(std::chrono::milliseconds(GRPC_THREAD_SLEEP_MS));
        }
    }

    void reapActors() {
        bool stillReaping = false;
        do {
            stillReaping = false;
            const std::lock_guard<std::mutex> lock(_streamsMutex);
            for (const auto stream : streams) {
                if (stream->sendOne())
                    stillReaping = true;
            }
        } while (stillReaping);
    }

    std::atomic<bool> done = false;
    std::thread _thread;
    std::set<StreamPtr> streams;
    std::mutex _streamsMutex;
};

// Manages all the grpc threads. Divides the workload evenly between them.
template <typename ClockSource, typename StreamInterface>
class GrpcClient {
public:
    GrpcClient() : _threads{CLIENT_THREADS} {}

    // EventStream users manage their own memory and should deregister themselves before
    // destructing, so we (carefully) use naked pointers.
    typedef EventStream<ClockSource, StreamInterface>* StreamPtr;

    void registerStream(StreamPtr stream) {
        _threads[latest_thread++ % _threads.size()].registerStream(stream);
    }

    void closeStream(StreamPtr stream) {
        for (int i = 0; i < _threads.size(); i++) {
            _threads[i].closeStream(stream);
        }
    }

private:
    std::vector<GrpcThread<ClockSource, StreamInterface>> _threads;
    std::atomic<size_t> latest_thread = 0;
};

template <typename ClockSource>
struct MetricsArgs {
    MetricsArgs(const typename ClockSource::time_point& finish,
                OperationEventT<ClockSource> event,
                size_t workerCount)
        : finish{finish}, event{std::move(event)}, workerCount{workerCount} {}
    typename ClockSource::time_point finish;
    OperationEventT<ClockSource> event;
    size_t workerCount;
};

// Efficient buffer that should rarely be blocked on insertions.
template <typename ClockSource>
class MetricsBuffer {
public:
    explicit MetricsBuffer(size_t size, const std::string& name)
        : size{size},
          name{name},
          _loading{std::make_unique<std::vector<MetricsArgs<ClockSource>>>()},
          _draining{std::make_unique<std::vector<MetricsArgs<ClockSource>>>()} {
        _draining->reserve(size);
        _loading->reserve(size);
    }

    // Thread-safe.
    void addAt(const typename ClockSource::time_point& finish,
               OperationEventT<ClockSource> event,
               size_t workerCount) {
        const std::lock_guard<std::mutex> lock(_loadingMutex);
        _loading->emplace_back(finish, std::move(event), workerCount);
    }

    // Not thread-safe.
    std::optional<MetricsArgs<ClockSource>> pop(bool force) {
        refresh(force);
        if (_draining->empty()) {
            return std::nullopt;
        } else {
            auto ret = std::move(_draining->back());
            _draining->pop_back();
            return ret;
        }
    }

    const std::string name;
    const size_t size;

private:
    void refresh(bool force) {
        if (_draining->empty()) {
            const std::lock_guard<std::mutex> lock(_loadingMutex);
            if (force || _loading->size() >= size * SWAP_BUFFER_PERCENT) {
                _draining.swap(_loading);
            }
        }

        // Maybe a bit nuclear, but this draws a box around the entire grpc system
        // and errors if it ever backs up enough to slow down an actor thread.
        if (_draining->size() > size) {
            std::ostringstream os;
            os << "Metrics buffer for operation name " << name << " exceeded pre-allocated space"
               << ". Expected size: " << size << ". Actual size: " << _draining->size()
               << ". This may affect recorded performance.";

            BOOST_THROW_EXCEPTION(MetricsError(os.str()));
        }
    }

    std::unique_ptr<std::vector<MetricsArgs<ClockSource>>> _loading;
    std::unique_ptr<std::vector<MetricsArgs<ClockSource>>> _draining;
    std::mutex _loadingMutex;
};
/**
 * Primary point of interaction between v2 poplar internals and the metrics system.
 */
template <typename ClockSource, typename StreamInterface>
class EventStream {
    using duration = typename ClockSource::duration;
    using OptionalPhaseNumber = std::optional<genny::PhaseNumber>;
    using ClientPtrRef = std::unique_ptr<GrpcClient<ClockSource, StreamInterface>>&;

public:
    explicit EventStream(const ActorId& actorId,
                         const std::string& name,
                         const OptionalPhaseNumber& phase,
                         const boost::filesystem::path& pathPrefix,
                         ClientPtrRef grpcClient)
        : _name{name},
          _stream{name, actorId},
          _phase{phase},
          _lastFinish{ClockSource::now()},
          _buffer(std::make_unique<MetricsBuffer<ClockSource>>(BUFFER_SIZE, _name)),
          _grpcClient(grpcClient) {
        _metrics.set_name(_name);
        _metrics.set_id(actorId);
        registerSelf();
    }

    // Record a metrics event to the loading buffer.
    void addAt(const typename ClockSource::time_point& finish,
               OperationEventT<ClockSource> event,
               size_t workerCount) {
        _buffer->addAt(finish, std::move(event), workerCount);
    }

    // Send one event from the draining buffer to the grpc api.
    // Returns true if there are more events to send.
    bool sendOne(bool force = false) {
        auto metricsArgsOptional = _buffer->pop(force);
        if (!metricsArgsOptional)
            return false;
        auto metricsArgs = *metricsArgsOptional;

        _metrics.mutable_time()->set_seconds(
            Period<ClockSource>(metricsArgs.finish.time_since_epoch()).getSecondsCount());
        _metrics.mutable_time()->set_nanos(
            Period<ClockSource>(metricsArgs.finish.time_since_epoch()).getNanosecondsCount());

        _metrics.mutable_timers()->mutable_duration()->set_seconds(
            metricsArgs.event.duration.getSecondsCount());
        _metrics.mutable_timers()->mutable_duration()->set_nanos(
            metricsArgs.event.duration.getNanosecondsCount());

        // If the EventStream was constructed after the end time was recorded.
        if (metricsArgs.finish < _lastFinish) {
            _metrics.mutable_timers()->mutable_total()->set_seconds(
                metricsArgs.event.duration.getSecondsCount());
            _metrics.mutable_timers()->mutable_total()->set_nanos(
                metricsArgs.event.duration.getNanosecondsCount());
        } else {
            _metrics.mutable_timers()->mutable_total()->set_seconds(
                Period<ClockSource>(metricsArgs.finish - _lastFinish).getSecondsCount());
            _metrics.mutable_timers()->mutable_total()->set_nanos(
                Period<ClockSource>(metricsArgs.finish - _lastFinish).getNanosecondsCount());
        }

        _metrics.mutable_counters()->set_number(metricsArgs.event.number);
        _metrics.mutable_counters()->set_ops(metricsArgs.event.ops);
        _metrics.mutable_counters()->set_size(metricsArgs.event.size);
        _metrics.mutable_counters()->set_errors(metricsArgs.event.errors);

        _metrics.mutable_gauges()->set_failed(metricsArgs.event.isFailure());
        _metrics.mutable_gauges()->set_workers(metricsArgs.workerCount);
        if (_phase) {
            _metrics.mutable_gauges()->set_state(*_phase);
        }
        _stream.write(_metrics);
        _lastFinish = metricsArgs.finish;

        return true;
    }

    EventStream(const EventStream<ClockSource, StreamInterface>&) = delete;
    EventStream<ClockSource, StreamInterface>& operator=(
        const EventStream<ClockSource, StreamInterface>&) = delete;

    ~EventStream() {
        try {
            if (_grpcClient) {
                _grpcClient->closeStream(this);
            }
            // Drain the buffer.
            while (sendOne(true)) {
            };
        } catch (...) {
            BOOST_LOG_TRIVIAL(error)
                << "Error while closing gRPC stream for operation name " << _name;
        }
    }

private:
    void registerSelf() {
        if (_grpcClient) {
            _grpcClient->registerStream(this);
        }
    }

    std::string _name;
    StreamInterface _stream;
    poplar::EventMetrics _metrics;
    std::optional<genny::PhaseNumber> _phase;
    typename ClockSource::time_point _lastFinish;
    std::unique_ptr<MetricsBuffer<ClockSource>> _buffer;
    ClientPtrRef _grpcClient;
};


}  // namespace internals::v2

}  // namespace genny::metrics

#endif  // HEADER_960919A5_5455_4DD2_BC68_EFBAEB228BB0_INCLUDED
