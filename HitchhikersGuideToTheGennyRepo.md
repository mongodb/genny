# 🚀 Hitchhikers' Guide to the Genny Repo 🚀

## 🔰 Intro 🔰

This guide is meant to serve as a readme for anyone working in the Genny
Repo. This is not meant as a guide to use Genny as a tool - that is in the
[README.md][readme] file in this same directory. The Genny Repo has a lot of
different layers 🍰 to it, and before getting started it is useful to know
what part of it to work on. This guide should give you an overview of where
to get started!

[readme]: README.md

## 📂 Where are all the files? 📂

Some of the main files that you will be working on are probably located in
[src/gennylib/include/gennylib][include]. The most common files among those
are [Orchestrator.hpp][orch], [PhaseLoop.hpp][ploop], and [context.hpp][ctx].
The respective .cpp files for [Orchestrator][orchcpp] and [context][ctxcpp]
are in [src/gennylib/src][gennysrc], while PhaseLoop has no corresponding
PhaseLoop.cpp file. To understand what kinds of 📈 metrics Genny outputs,
read through the [metrics.hpp][mtx].

Another main component are the Actors. The Actors all live in
[src/cast_core/src/actors][actorshpp]. We'll go into more detail about them
later.

  [include]: src/gennylib/include/gennylib
  [orch]: src/gennylib/include/gennylib/Orchestrator.hpp
  [ploop]: src/gennylib/include/gennylib/PhaseLoop.hpp
  [ctx]: src/gennylib/include/gennylib/context.hpp
  [orchcpp]: src/gennylib/src/Orchestrator.cpp
  [ctxcpp]: src/gennylib/src/context.cpp
  [gennysrc]: src/gennylib/src
  [mtx]: src/gennylib/include/gennylib/metrics.hpp
  [actorshpp]: src/cast_core/src/actors

## 📜 Walkthrough 📜

### 🚗 Default Driver 🚗

Not to be confused with a "mongo driver", this is the "main" object in
Genny that actually constructs all the relevant pieces and sets the
workload into motion.

The [Default Driver][driver] is what sets everything up. Most of what you
need to worry about is in the [doRunLogic()][dorun] function. The
Orchestrator object is created here. Then a `WorkloadContext` object is
instantiated, the "Cast" exposes all the Actor types available (see below
section), and the [actors are run][runit]. The [runActor()][runActor]
function calls the run() member function for each actor. Finally, the
function writes the metrics out to the specified file and returns 0 or an
error code.

  [driver]: src/driver/src/DefaultDriver.cpp
  [dorun]: src/driver/src/DefaultDriver.cpp#L63
  [runit]: src/driver/src/DefaultDriver.cpp#L103
  [runActor]: src/driver/src/DefaultDriver.cpp#L42

### 🤩 Casts 🤩

Actors belong to a cast. Get it?

The `Cast` keeps a set of `ActorProducer` objects that it exposes to the
`WorkloadContext`. Each `ActorProducer` is a stateful object that knows
a specific pattern for producing single `Actor` instances. It's
perfectly acceptable to have two different `ActorProducer` instances
that only differ in some preset constant value

### 👷 Workload Context 👷

The `WorkloadContext` is constructed in [context.hpp][ctx]. The
`WorkloadContext` object is the YAML workload file decomposed into
different pieces. The `Orchestrator` (which we'll get into later) and
the metrics objects are known to the `WorkloadContext`, and the main
function is to create a few different contexts.

The `WorkloadContext` creates an `ActorContext` that has the YAML node and a
mapping of a `phaseNumber` to its respective `PhaseContext`. The
`PhaseContext` has the YAML node of the phase itself and knowledge of its
`ActorContext`.

The main member function used in these classes is [get()][ctxget]. Look to
examples in [context][ctxeg] and [other classes][ctxother] for examples. The
second argument in `<>` tells whether the arg is required (use
`std::optional` if not).

  [ctxget]: src/gennylib/include/gennylib/context.hpp#L284-L286
  [ctxeg]: src/gennylib/include/gennylib/context.hpp#L585
  [ctxother]: src/gennylib/include/gennylib/PhaseLoop.hpp#L67-L68

### 🌐 HelloWorld.yml example structure 🌐

This example is taken directly from the Workload.yml file.

```yaml
  - Name: HelloTest
    Type: HelloWorld
    Threads: 2
    Phases:
    - Message: Hello Phase 0 🐳
      Duration: 20000
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

We'll look at the [HelloWorld Actor][hi] for example.

The producer is called in the [DefaultDriver][driver] before any of the
initialization is done. Then when the driver calls [`run()`][run] (actually
calling [`runActor()`][runactor] which then calls `run()`), the Actor
iterates over its PhaseLoop using the PhaseLoop iterator class.

The [outer loop][oloop] splits the `_loop` object into a `PhaseNumber` and an
`ActorPhase` (it actually creates an entry into the `_loop` object for each
thread that the YAML file specifies). The [inner loop][iloop] iterates over
the config and runs the specified operation for the actor for as many times
as repeat or duration specify it to run. If both the repeat and duration
(measured in milliseconds) keywords are specified, then it will run for the
longer of the two (i.e. duration = 20000, repeat = 2, it will run for 20000).

  [hi]: src/gennylib/src/actors/HelloWorld.cpp
  [DefaultDriver]: src/driver/src/DefaultDriver.cpp
  [run]: src/driver/src/DefaultDriver.cpp#L56
  [runactor]: src/driver/src/DefaultDriver.cpp#L52-L71
  [oloop]: src/gennylib/src/actors/HelloWorld.cpp#L14
  [iloop]: src/gennylib/src/actors/HelloWorld.cpp#L15

### ➰ PhaseLoop and ActorPhase ➰

The `PhaseLoop` class does two main things. 

1. It [constructs the `PhaseMap` object][cplctor], (defined [here][cplctordef]), and
2. it defines the [beginning][beg] and [end][end] of the [PhaseLoop Iterator class][piter].

The `PhaseLoop` object is a map that has one entry per phase.

The `PhaseLoopIterator class` does a few interesting things. 

- In each iteration, it calls `awaitPhaseStart()` and `awaitPhaseEnd()` in the orchestrator. 
- If the thread is blocked, it calls [`awaitPhaseStart()`][awaitstart] and stays blocked, but if it is not, then it calls [`awaitPhaseEnd()`][awaitend] and runs.
- In [`operator++()`][oppp], `awaitPhaseEnd() is also called to signal that the thread is a blocking thread.

The [`ActorPhase` object][aphase] is iterated over in the inner loop of each
actor. The object is looped over for the number of times specified by
duration or repeat, and then is returned. The [`IterationCompletionCheck`
object is passed][check] as an argument to check whether the actor is done
iterating, and then the `PhaseLoopIterator object` notifies the orchestrator
to step to the next phase as described above.

  [cplctor]: src/gennylib/include/gennylib/PhaseLoop.hpp#L526-L555
  [cplctordef]: src/gennylib/include/gennylib/PhaseLoop.hpp#L314
  [beg]: src/gennylib/include/gennylib/PhaseLoop.hpp#L516-L518
  [end]: src/gennylib/include/gennylib/PhaseLoop.hpp#L520-L522
  [piter]: src/gennylib/include/gennylib/PhaseLoop.hpp#L329-L433
  [awaitstart]: src/gennylib/include/gennylib/PhaseLoop.hpp#L344
  [awaitend]: src/gennylib/include/gennylib/PhaseLoop.hpp#L346
  [oppp]: src/gennylib/include/gennylib/PhaseLoop.hpp#L364-L370
  [aphase]: src/gennylib/include/gennylib/PhaseLoop.hpp#L220-L307
  [check]: src/gennylib/include/gennylib/PhaseLoop.hpp#L252

### 🎼 Orchestrator 🎼

The [`Orchestrator class`][orch] is used to walk all of the actors to the next
phase together, and to make sure that blocking threads finish before
other threads start their processes. It also outputs the `phaseNumber`
to the output file using a [`metrics::Gauge`][gauge] object. The Orchestrator
adds a token for every thread that starts, and removes the token once
the thread is finished. When the token count is 0, it signals the end of
the Phase.

  [gauge]: src/gennylib/include/gennylib/Orchestrator.hpp#L95

### 🔚 End 🔚
