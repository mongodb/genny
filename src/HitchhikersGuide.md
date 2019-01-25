# 🚀 Hitchhikers' Guide to the Genny Repo 🚀 {#mainpage}

## 🔰 Intro 🔰

This guide is meant to serve as a readme for anyone working in the Genny
Repo. This is not meant as a guide to use Genny as a tool - that is in
the README.md file in the parent directory. The Genny Repo has a lot of
different layers 🍰 to it, and before getting started it is useful to
know what part of it to work on. This guide should give you an overview
of where to get started!

## 📂 Where are all the files? 📂

Some of the main files that you will be working on are probably located
in `src/gennylib/include/gennylib`. The most common files among those
are `Orchestrator.hpp`, `PhaseLoop.hpp`, and `context.hpp`. The
respective .cpp files for `Orchestrator` and `context` are in
`src/gennylib/src`, while `PhaseLoop` has no corresponding PhaseLoop.cpp
file. To understand what kinds of 📈 metrics Genny outputs, read through
`metrics.hpp`.

Another main component are the Actors. The Actors all live in
`src/cast_core/include/cast_core` for the `.hpp` files and in
`src/cast_core/src` for the `.cpp` files. We'll go into more detail
about them later.

## 📜 Walkthrough 📜

### 🚗 Default Driver 🚗

The Default Driver (`src/driver/src/DefaultDriver.cpp`) is what sets
everything up. Most of what you need to worry about is in the
`doRunLogic()` function. The Orchestrator object is created here along
with all of the Actors. Then a `WorkloadContext` (from `context.hpp`)
object is instantiated and the actors are run. The `runActor()` function
calls the `run()` member function for each actor. Finally, the function
writes the metrics out to the specified file and returns 0 or an error
code.

### 👷 Workload Context 👷

The WorkloadContext is defined in `context.hpp`. The `WorkloadContext`
object is the YAML workload file decomposed into different pieces. The
`Orchestrator` (which we'll get into later) and the metrics objects are
known to the `WorkloadContext`, and the main function is to create a few
different contexts. The `WorkloadContext` creates an `ActorContext` that
has the YAML node and a mapping of a `phaseNumber` to its respective
`PhaseContext`. The `PhaseContext` has the YAML node of the phase itself
and knowledge of its ActorContext.

The main member function used in these classes is `get()`. Look to
examples in context and other classes for examples. The second argument
in `<>` tells whether the arg is required (use `std::optional` if not).

#### 🌐 HelloWorld.yml example structure 🌐

This example is taken directly from the Workload.yml file.

```yaml
# this whole file is parsed into the WorkloadContext.
SchemaVersion: 2018-07-01 # Required constant
Actors: # each item in Actors becomes an ActorContext
- Name: HelloTest
  Type: HelloWorld
  Threads: 2
  Phases: # each item in Phases becomes a PhaseContext
  - Message: Hello Phase 0 🐳
    Duration: 20000 # Duration/Repeat are handled by PhaseLoop
    Repeat: 2
  - Message: Hello Phase 1 👬
    Repeat: 5
- Name: HelloTest2
  Type: HelloWorld
  Threads: 2
  Phases:
  - Message: Hello Phase 0 🐳
    Duration: 4
  - Message: Hello Phase 1 👬
    Repeat: 4
```

### 💃 General Actor Structure 💃

We'll look at the `HelloWorld` Actor for example. The producer is called
in the `DefaultDriver` before any of the initialization is done. Then
when the driver calls `run()` (actually calling `runActor()` which then
calls run), the actor iterates over its `PhaseLoop` using the
`PhaseLoop` iterator class. The outer loop splits the `_loop` object
into an `ActorPhase` (it actually creates an entry into `the` `_loop`
object for each thread that the YAML file specifies). The inner loop
iterates over the config and runs the specified operation for the actor
for as many times as `Repeat` or `Duration` specify it to run. If both
the `Repeat` and `Duration` keywords are specified, then it will run for
the longer of the two (e.g. `{ Duration: 2 seconds, Repeat: 1 }` the
Duration will win unless the single iteration takes longer than 2
seconds).

### Casts

The `Cast` keeps a set of `ActorProducer` objects that it exposes to the
`WorkloadContext`. Each `ActorProducer` is a stateful object that knows
a specific pattern for producing single `Actor` instances. It's
perfectly acceptable to have two different `ActorProducer` instances
that only differ in some preset constant value.

### ➰ `PhaseLoop` and `ActorPhase` ➰

The `PhaseLoop` class does two main things. It constructs the `PhaseMap`
object and defines the beginning and end of the `PhaseLoop` Iterator
class. The `PhaseLoop` object has a map that has one entry per Phase. So
if the YAML was the one in the example above, each of the 2 HelloWorld
Actor instances would have their own `PhaseLoop` instance which each
contain two `ActorPhase` instances, one for each Phase.

The `PhaseLoopIterator` class does a few interesting things. In each
iteration, it calls `awaitPhaseStart()` and `awaitPhaseEnd()` in the
`Orchestrator`. If the Phase is blocking (a `Repeat` or `Duration` are
specified), it calls `awaitPhaseStart()` and stays blocked, but if it is
not, then it calls `awaitPhaseEnd()` immediately and runs. In
`operator++()`, `awaitPhaseEnd()` is also called to signal that the
thread is a blocking thread.

The `ActorPhase` object is iterated over in the inner loop of each
Actor. The object is looped over for the number of times specified by
`Duration` or `Repeat`, and then is returned. The
`IterationCompletionCheck` object is passed as an argument to check
whether the actor is done iterating, and then the `PhaseLoopIterator`
object notifies the Orchestrator to step to the next Phase as described
above.

### 🎼 The `Orchestrator` 🎼

The `Orchestrator` class is used to walk all of the Actors to the next
Phase together, and to make sure that blocking threads finish before
other threads start their processes. It also outputs the `phaseNumber`
to the output file using a `metrics::Gauge` object. The `Orchestrator`
adds a token for every thread that starts, and removes the token once
the thread is finished. When the token count is 0, it signals the end of
the Phase.

🔚 End 🔚
