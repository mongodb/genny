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

#include <cstdlib>

#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include <poplarlib/collector.grpc.pb.h>
#include <metrics/operation.hpp>

/**
 * @namespace genny::metrics::internals::v2 this namespace is private and only intended to be used by genny's
 * own internals. No types from the genny::metrics::v2 namespace should ever be typed directly into
 * the implementation of an actor.
 */
namespace genny::metrics::internals::v2 {

// TODO: Add gRPC call deadline constant.

/**
 * Manages the collector stub and its interactions.
 */
class CollectorStubInterface {
public:

    CollectorStubInterface() {
        if (!_stub) {
            auto channel = grpc::CreateChannel("localhost:2288", grpc::InsecureChannelCredentials());
            _stub = poplar::PoplarEventCollector::NewStub(channel);
        }
    }

    poplar::PoplarEventCollector::StubInterface* operator->() {
        return _stub.get();
    }

private:
    static std::unique_ptr<poplar::PoplarEventCollector::StubInterface> _stub;
};


/**
 * Manages the stream of poplar EventMetrics.
 */
class StreamInterface {
public:
    StreamInterface(CollectorStubInterface stub) :
        _stub{stub},
        _options{},
        _response{},
        _context{},
        _stream{_stub->StreamEvents(&_context, &_response)} {
            _options.set_no_compression().set_buffer_hint();
        }

    void write(const poplar::EventMetrics& event) {
        auto success = _stream->Write(event, _options);

        // TODO: Better error handling.
        if (!success) {
            std::cout << "Couldn't write: stream was closed";
            throw std::bad_function_call();
        }
    }

    ~StreamInterface() {
        // TODO: Better debug logs.
        if (!_stream) {
            std::cout << "No _stream." << std::endl;
            return;
        }
        if (!_stream->WritesDone()) {
            // TODO: barf
            std::cout << "Errors in doing the writes?" << std::endl;
        }
        auto status = _stream->Finish();
        if (!status.ok()) {
            std::cout << "Problem closing the stream:\n"
                      << _context.debug_error_string() << std::endl;
        }
    }

private:
    CollectorStubInterface _stub;
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

    explicit Collector(CollectorStubInterface& stub, std::string name)
        : _name{std::move(name)}, _stub{stub}, _id{} {
        _id.set_name(_name);

        //std::cout << "DEBUG: Creating collector with name " << _name << "\n";
        if (_name == "") {
            //std::cout << "DEBUG: Name is empty" << "\n";
        }

        grpc::ClientContext context;
        poplar::PoplarResponse response;
        poplar::CreateOptions options = createOptions(_name);
        auto status = _stub->CreateCollector(&context, options, &response);

        // TODO: Better error handling.
        if (!status.ok()) {
            std::cout << "Status not okay\n" << status.error_message() << "\n";
            throw std::bad_function_call();
        }
    }

    ~Collector() {
        grpc::ClientContext context;
        poplar::PoplarResponse response;
        // TODO: Better error handling
        auto status = _stub->CloseCollector(&context, _id, &response);
        if (!status.ok()) {
            std::cout << "Couldn't close collector: " << status.error_message();
        }
    }

    CollectorStubInterface& _stub;

private:
    // TODO: Add path prefix
    static auto createPath(const std::string& name) {
        std::stringstream str;
        str << name << ".ftdc";
        return str.str();
    }

    static poplar::CreateOptions createOptions(const std::string& name) {
        poplar::CreateOptions options;
        options.set_name(name);
        options.set_events(poplar::CreateOptions_EventsCollectorType_BASIC);
        // end in .ftdc -- each stream should have a different path -- unique id for the stream
        options.set_path(createPath(name));
        // how many events between compression and write events
        options.set_chunksize(1000);  // probably not less than 10k maybe more - play with it; less
        // means more time in cpu&io to compress
        // flush to disk intermittently
        options.set_streaming(true);
        // dynamic means shape changes over time
        options.set_dynamic(false);
        // may not matter but shrug it works
        options.set_recorder(poplar::CreateOptions_RecorderType_PERF);
        options.set_events(poplar::CreateOptions_EventsCollectorType_BASIC);
        return options;
    }

    std::string _name;
    // _id should always be the same as the name in the options to createcollector
    poplar::PoplarID _id;

};

template <typename Clocksource>
class OperationEventT;

/**
 * Primary point of interaction between v2 poplar internals and the metrics system.
 */
template <typename ClockSource>
class EventStream {
    using duration = typename ClockSource::duration;
    using OptionalPhaseNumber = std::optional<genny::PhaseNumber>;
public:
    explicit EventStream(const ActorId& actorId, std::string actor_name, std::string op_name, OptionalPhaseNumber phase) 
        : 
            _stub{}, 
            _name{std::move(createName(actorId, actor_name, op_name, phase))},
            _collector{_stub, _name}, 
            _stream{_stub}, 
            _phase{phase} {
                _metrics.set_name(_name);
                this->_reset();
                //std::cout << "DEBUG: Constructing EventStream" << "\n";
            }

    // TODO: Add phase / state, maybe ID?
    void add(const OperationEventT<ClockSource>& event) {
        _metrics.mutable_counters()->set_number(event.iters);
        _metrics.mutable_counters()->set_ops(event.ops);
        _metrics.mutable_counters()->set_size(event.size);
        _metrics.mutable_counters()->set_errors(event.errors);
        _metrics.mutable_timers()->mutable_duration()->set_seconds(
                secondsCount(static_cast<duration>(event.duration)));
        _metrics.mutable_timers()->mutable_duration()->set_nanos(
                nanosecondsCount(static_cast<duration>(event.duration)));
        _metrics.mutable_gauges()->set_failed(event.isFailure());
        _stream.write(_metrics);
        _reset();
    }
    
private:
    void _reset() {
        _metrics.mutable_timers()->mutable_duration()->set_nanos(0);
        _metrics.mutable_timers()->mutable_duration()->set_seconds(0);
        _metrics.mutable_counters()->set_errors(0);
        _metrics.mutable_counters()->set_number(0);
        _metrics.mutable_counters()->set_ops(0);
        _metrics.mutable_counters()->set_size(0);
        _metrics.mutable_gauges()->set_state(0);
        _metrics.mutable_gauges()->set_workers(0);
        _metrics.mutable_gauges()->set_failed(false);
        _metrics.mutable_time()->set_seconds(0);
        _metrics.mutable_time()->set_nanos(0);
    }

    std::string createName(ActorId actor_id, std::string actor_name, std::string op_name, OptionalPhaseNumber phase) {
        std::stringstream str;
        std::cout << "DEBUG: Creating name. \n\tactor_name: " << actor_name << "\n\tactor_id: " << actor_id << " \n\top_name: " << op_name << " \n\tphase: " << *phase << "\n";
        str << actor_name << '.' << actor_id << '.' << op_name;
        if (phase) {
            str << '.' << *phase;
        }
        return str.str();
    }

    
private:
    
    CollectorStubInterface _stub;
    std::string _name;
    Collector _collector;
    StreamInterface _stream;
    poplar::EventMetrics _metrics;
    std::optional<genny::PhaseNumber> _phase;
};


} // namespace genny::metrics::v2

#endif // HEADER_960919A5_5455_4DD2_BC68_EFBAEB228BB0_INCLUDED
