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

#ifndef HEADER_00818641_6D7B_4A3D_AFC6_38CC0DBAD99B_INCLUDED
#define HEADER_00818641_6D7B_4A3D_AFC6_38CC0DBAD99B_INCLUDED

#include <queue>
#include <future>
#include <functional>

#include <boost/log/trivial.hpp>

namespace genny {

using ActorId = unsigned int;

/**
 * Owner of a Tasks's future and result. Automatically resolves on access.
 */
template<typename T>
class TaskResult {
public:
    TaskResult(std::shared_ptr<std::future<T>> futureIn, std::shared_ptr<bool> readyIn) :
        resultFuture{futureIn}, ready{readyIn} {}

    TaskResult(TaskResult&&) = default;
    TaskResult& operator=(TaskResult&&) = default;

    TaskResult(const TaskResult&) = delete;
    TaskResult& operator=(const TaskResult&) = delete;

    void resolve() {
        if (!resultValue) {
            resultValue = std::make_unique<T>(std::move(resultFuture->get()));
            *ready = true;
        }
    }

    bool isResolved() {
        return resultValue || *ready; // From a user perspective, a ready future is resolved.
    }

    /**
     * Chained arrow operator that resolves the underlying value
     * before allowing access.
     */
    T& operator->() {
        resolve();
        return *resultValue;
    }

    T& operator*() {
        resolve();
        return *resultValue;
    }

private:

    std::shared_ptr<std::future<T>> resultFuture;
    std::shared_ptr<bool> ready;
    std::unique_ptr<T> resultValue;

};


class TaskQueue {
public:
    /**
     * The return value for tasks that don't need to return anything.
     * (We can't store void in the template.)
     */
    using NoReturn = std::tuple<>;

    TaskQueue() = default; 
    TaskQueue(TaskQueue&&) = default;
    TaskQueue& operator=(TaskQueue&&) = default;

    TaskQueue(const TaskQueue&) = delete;
    TaskQueue& operator=(const TaskQueue&) = delete;

    /**
     * Add a task to the task list.
     *
     * @param t
     *   a function to execute, returning a value to be stored
     * @return a TaskResult containing the result of the task
     */
    template<typename T>
    TaskResult<T> addTask(std::function<T()> t) {
        // We use a sharing so we can resolve on-command.
        // Copy-constructor deletion in the TaskQueue and TaskResult
        // prevents this from proliferating, only the lambda and the
        // result own it.
        std::shared_ptr<std::future<T>> fut = std::make_shared<std::future<T>>(std::async(std::launch::deferred, t));
        std::shared_ptr<bool> ready = std::make_shared<bool>(false);
        _tasks.push([=](){ 
            if (!(*ready)) {
                fut->wait();
                *ready = true;
            }
        });
        return TaskResult<T>(fut, ready);
    }

    /**
     * Overload of addTask for void functions.
     */
    void addTask(std::function<void()> t) {
        addTask<NoReturn>([=]() {
            t();
            return NoReturn();
        });
    }

    /**
     * Run all the tasks in the task list.
     */
    void runAllTasks() {
        while (!_tasks.empty()) {
            _tasks.front()();
            _tasks.pop();
        }
    }

private:
    std::queue<std::function<void()>> _tasks;
};

class ActorContext;

/**
 * An Actor is the base unit of work in genny. An actor is a single-
 * threaded entity.
 *
 * The basic flow is:
 *
 * 1.  Load %YAML File
 * 2.  Construct metrics and other delegate objects
 * 3.  Call all enabled ActorProducers to produce as many Actors as they
 *     wish. Each ActorProducer is given each of the ActorContext objects.
 * 4.  Create a thread for each actor produced.
 * 5.  Call .run() for each actor.
 * 6.  Wait until all actors return from run().
 *
 * When writing a new Actor, there are two steps:
 *
 * 1.  Write the Actor subclass itself. Most actors should consider
 *     having a PhaseLoop member variable that they use for flow-control
 *     logic to collaborate cleanly with other actors.
 *
 * 2.  Write an `ActorProducer` that can produce an ActorVector from an
 *     ActorContext. The ActorProducer is where configuration values can be
 *     read and validated and passed into the Actor implementations.
 *     Typically `ActorProducer`s will simply be a static method on an Actor
 *     subclass.
 *
 * See other Actor implementations as an example. In addition there is the
 * `create-new-actor` script that assists with the boilerplate necessary to
 * create a new Actor instance.
 *
 * Actors may retain a reference to the ActorContext and/or parent
 * WorkloadContext, but it is recommended for performance that they
 * call `context.get(...)` only during their constructors and retain
 * refs or copies of config objects
 */
class Actor {


public:
    /**
     * Construct an Actor.
     *
     * @param context source for configs. The top-level config has `Actors: [{Type: Foo}]`.
     * The ActorContext exposes each of the `{Type: Foo}` maps.
     */
    explicit Actor(ActorContext& context);

    /**
     * Destruct an Actor.
     */
    virtual ~Actor() = default;

    /*
     * A consistent compilation-unit unique name for this actor.
     * This name is mostly intended for metrics and logging purposes.
     */
    // static std::string_view defaultName()

    /**
     * The main method of an actor. Will be run in its own thread.
     * This is only intended to be called by workload drivers.
     */
    virtual void run() = 0;

    /*
     * This should be invoked by the driver right before calling run() but
     * after the context has been constructed to resolve startup-related
     * futures.
     */
    void runStartupTasks();

    /**
     * @return the id for the Actor. Each Actor should
     * have a unique id. This is used for metrics reporting and other purposes.
     * This is obtained from `WorkloadContext.nextActorId()` (see `Actor.cpp`)
     */
    virtual ActorId id() const {
        return _id;
    }
protected:
    TaskQueue _startupTasks;

private:
    ActorId _id;
};


}  // namespace genny

#endif  // HEADER_00818641_6D7B_4A3D_AFC6_38CC0DBAD99B_INCLUDED
