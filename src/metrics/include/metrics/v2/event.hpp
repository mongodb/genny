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
#include <cstdlib>

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

class PoplarRequestError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};


/**
 * Manages the collector stub and its interactions.
 */
class CollectorStubInterface {
public:
    CollectorStubInterface() {
        getStub();
    }

    poplar::PoplarEventCollector::StubInterface* operator->() {
        return _stub.get();
    }

private:
    // We should only have one channel across all threads.
    static std::unique_ptr<poplar::PoplarEventCollector::StubInterface> _stub;

    auto createStub() {
        grpc::ChannelArguments args;
        // The BDP estimator overwhelms the server with pings on a heavy workload.
        args.SetInt("GRPC_ARG_HTTP2_BDP_PROBE", 0);
        auto channel =
            grpc::CreateCustomChannel("localhost:2288", grpc::InsecureChannelCredentials(), args);
        _stub = poplar::PoplarEventCollector::NewStub(channel);
        return true;
    }

    void getStub() {
        static bool stub_created = createStub();
        return;
    }
};


/**
 * Manages the stream of poplar EventMetrics.
 */
class StreamInterface {
public:
    StreamInterface(CollectorStubInterface& stub)
        : _options{}, _response{}, _context{}, _stream{stub->StreamEvents(&_context, &_response)} {
        _options.set_no_compression().set_buffer_hint();
    }

    void write(const poplar::EventMetrics& event) {
        auto success = _stream->Write(event, _options);

        if (!success) {
            BOOST_THROW_EXCEPTION(PoplarRequestError("Failed to write to stream."));
        }
    }

    ~StreamInterface() {
        if (!_stream) {
            BOOST_LOG_TRIVIAL(error) << "Tried to close grpc stream, but none existed.";
            return;
        }
        if (!_stream->WritesDone()) {
            BOOST_LOG_TRIVIAL(warning) << "Closing grpc stream, but not all writes completed.";
        }
        auto status = _stream->Finish();
        if (!status.ok()) {
            BOOST_LOG_TRIVIAL(error)
                << "Problem closing grpc stream: " << _context.debug_error_string();
        }
    } 
private:
    grpc::WriteOptions _options;
    poplar::PoplarResponse _response;
    grpc::ClientContext _context;
    std::unique_ptr<grpc::ClientWriterInterface<poplar::EventMetrics>> _stream;
};


/**
 * Manages the gRPC-side collector for each operation.
 * Exists for construction / destruction resource management only.
 */
class Collector {
public:
    Collector(const Collector&) = delete;

    explicit Collector(const std::string& name,
                       const std::string& path_prefix)
        : _name{name}, _id{} {
        _id.set_name(_name);

        grpc::ClientContext context;
        poplar::PoplarResponse response;
        poplar::CreateOptions options = createOptions(_name, path_prefix);
        auto status = _stub->CreateCollector(&context, options, &response);

        if (!status.ok()) {
            std::ostringstream os;
            os << "Collector status not okay: " << status.error_message();
            BOOST_THROW_EXCEPTION(PoplarRequestError(os.str()));
        }
    }

    ~Collector() {
        grpc::ClientContext context;
        poplar::PoplarResponse response;
        auto status = _stub->CloseCollector(&context, _id, &response);
        if (!status.ok()) {
            BOOST_LOG_TRIVIAL(error) << "Couldn't close collector: " << status.error_message();
        }
    }


private:
    static auto createPath(const std::string& name, const std::string& path_prefix) {
        std::stringstream str;
        str << path_prefix << '/';
        str << name << ".ftdc";
        return str.str();
    }

    static poplar::CreateOptions createOptions(const std::string& name,
                                               const std::string& path_prefix) {
        poplar::CreateOptions options;
        options.set_name(name);
        options.set_events(poplar::CreateOptions_EventsCollectorType_BASIC);
        options.set_path(createPath(name, path_prefix));
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

/**
 * Primary point of interaction between v2 poplar internals and the metrics system.
 */
template <typename ClockSource>
class EventStream {
    using duration = typename ClockSource::duration;
    using OptionalPhaseNumber = std::optional<genny::PhaseNumber>;
public:
    explicit EventStream(const ActorId& actorId,
                         const std::string& name,
                         const OptionalPhaseNumber& phase,
                         const std::string& path_prefix)
        : _stub{},
          _name{name},
          _stream{_stub},
          _phase{phase},
          _last_finish{ClockSource::now()} {
        _metrics.set_name(_name);
        _metrics.set_id(actorId);
        this->_reset();
    }

    void addAt(const typename ClockSource::time_point& finish, const OperationEventT<ClockSource>& event, size_t workerCount) {
        _metrics.mutable_timers()->mutable_duration()->set_nanos(event.duration.getSecondsCount());
        _metrics.mutable_timers()->mutable_duration()->set_nanos(event.duration.getNanosecondsCount());
        _metrics.mutable_timers()->mutable_total()->set_nanos(Period<ClockSource>(finish - _last_finish).getNanosecondsCount());
        _metrics.mutable_timers()->mutable_total()->set_nanos(Period<ClockSource>(finish - _last_finish).getSecondsCount());

        _metrics.mutable_counters()->set_number(event.iters);
        _metrics.mutable_counters()->set_ops(event.ops);
        _metrics.mutable_counters()->set_size(event.size);
        _metrics.mutable_counters()->set_errors(event.errors);

        _metrics.mutable_gauges()->set_failed(event.isFailure());
        _metrics.mutable_gauges()->set_workers(workerCount);
        if (_phase) {
            _metrics.mutable_gauges()->set_state(*_phase);
        }
        _stream.write(_metrics);
        _last_finish = finish;
    }

private:
    void _reset() {
        _metrics.mutable_timers()->mutable_duration()->set_nanos(0);
        _metrics.mutable_timers()->mutable_total()->set_nanos(0);
        _metrics.mutable_counters()->set_errors(0);
        _metrics.mutable_counters()->set_number(0);
        _metrics.mutable_counters()->set_ops(0);
        _metrics.mutable_counters()->set_size(0);
        _metrics.mutable_gauges()->set_state(0);
        _metrics.mutable_gauges()->set_workers(0);
        _metrics.mutable_gauges()->set_failed(false);
        _metrics.mutable_time()->set_nanos(0);
    }

private:
    CollectorStubInterface _stub;
    std::string _name;
    StreamInterface _stream;
    poplar::EventMetrics _metrics;
    std::optional<genny::PhaseNumber> _phase;
    typename ClockSource::time_point _last_finish;
};


}  // namespace internals::v2

}  // namespace genny::metrics

#endif  // HEADER_960919A5_5455_4DD2_BC68_EFBAEB228BB0_INCLUDED
