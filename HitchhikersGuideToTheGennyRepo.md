## 🚀 Hitchhikers' Guide to the Genny Repo 🚀

#### 🔰 Intro 🔰
This guide is meant to serve as a readme for anyone working in the Genny Repo. This is not meant as a guide to use Genny as a tool - that is in the [README.md](README.md) file in this same directory. The Genny Repo has a lot of different layers 🍰 to it, and before getting started it is useful to know what part of it to work on. This guide should give you an overview of where to get started!

### 📂 Where are all the files? 📂
Some of the main files that you will be working on are probably located in [src/gennylib/include/gennylib/](src/gennylib/include/gennylib). The most common files among those are [Orchestrator.hpp](src/gennylib/include/gennylib/Orchestrator.hpp), [PhaseLoop.hpp](src/gennylib/include/gennylib/PhaseLoop.hpp), and [context.hpp](src/gennylib/include/gennylib/context.hpp). The respective .cpp files for [Orchestrator](src/gennylib/src/Orchestrator.cpp) and [context](src/gennylib/src/context.cpp) are in [src/gennylib/src](src/gennylib/src), while PhaseLoop has no corresponding PhaseLoop.cpp file. To understand what kinds of 📈 metrics Genny outputs, read through the [metrics.hpp](src/gennylib/include/gennylib/metrics.hpp).

Another main component are the Actors. The actors all live in [src/gennylib/include/gennylib/actors](src/gennylib/include/gennylib/actors) for the .hpp files and in [src/gennylib/src/actors/HelloWorld.cpp](src/gennylib/src/actors/HelloWorld.cpp) for the .cpp files. We'll go into more detail about them later.

### 📜 Walkthrough 📜

##### 🚗 Default Driver 🚗
The [Default Driver](src/driver/src/DefaultDriver.cpp) is what sets everything up. Most of what you need to worry about is in the [doRunLogic()](src/driver/src/DefaultDriver.cpp#L73-L144) function. The orchestrator object is created here along with all of the [actors](src/driver/src/DefaultDriver.cpp#L85-L93). Then a `WorkloadContext` object is instantiated and the [actors are run](src/driver/src/DefaultDriver.cpp#L125). The [runActor()](src/driver/src/DefaultDriver.cpp#L52-L71) function calls the run() member function for each actor. Finally, the function writes the metrics out to the specified file and returns 0 or an error code. 

##### 👷 Workload Context 👷
The `WorkloadContext` is constructed in [context.hpp](src/gennylib/include/gennylib/context.hpp). The `WorkloadContext` object is the YAML workload file decomposed into different pieces. The `Orchestrator` (which we'll get into later) and the metrics objects are known to the `WorkloadContext`, and the main function is to create a few different contexts. The `WorkloadContext` creates an `ActorContext` that has the YAML node and a mapping of a `phaseNumber` to its respective `PhaseContext`. The `PhaseContext` has the YAML node of the phase itself and knowledge of its `ActorContext`. 

The main member function used in these classes is [get()](src/gennylib/include/gennylib/context.hpp#L284-L286). Look to examples in [context](src/gennylib/include/gennylib/context.hpp#L585) and [other classes](src/gennylib/include/gennylib/PhaseLoop.hpp#L67-L68) for examples. The second argument in `<>` tells whether the arg is required (use `std::optional` if not). 

###### 🌐 HelloWorld.yml example structure 🌐
This example is taken directly from the Workload.yml file.
```
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

##### 💃 General Actor Structure 💃 
We'll look at the [HelloWorld Actor](src/gennylib/src/actors/HelloWorld.cpp) for example. The producer is called in the [DefaultDriver](src/driver/src/DefaultDriver.cpp) before any of the initialization is done. Then when the driver calls [`run()`](src/driver/src/DefaultDriver.cpp#L56) (actually calling [`runActor()`](src/driver/src/DefaultDriver.cpp#L52-L71) which then calls run), the actor iterates over it's PhaseLoop using the PhaseLoop iterator class. The [outer loop](src/gennylib/src/actors/HelloWorld.cpp#L14) splits the `_loop` object into a `PhaseNumber` and an `ActorPhase` (it actually creates an entry into the `_loop` object for each thread that the YAML file specifies). The [inner loop](src/gennylib/src/actors/HelloWorld.cpp#L15) iterates over the config and runs the specified operation for the actor for as many times as repeat or duration specify it to run. If both the repeat and duration (measured in milliseconds) keywords are specified, then it will run for the longer of the two (i.e. duration = 20000, repeat = 2, it will run for 20000).

##### ➰ PhaseLoop and ActorPhase ➰
The `PhaseLoop class` does two main things. It [constructs the `PhaseMap` object](src/gennylib/include/gennylib/PhaseLoop.hpp#L526-L555) (defined [here](src/gennylib/include/gennylib/PhaseLoop.hpp#L314)) and defines the [beginning](src/gennylib/include/gennylib/PhaseLoop.hpp#L516-L518) and [end](src/gennylib/include/gennylib/PhaseLoop.hpp#L520-L522) of the [`PhaseLoop Iterator class`](src/gennylib/include/gennylib/PhaseLoop.hpp#L329-L433). The `PhaseLoop` object is a map that has one entry per thread per phase per actor. So if the YAML was the one in the example above, the PhaseLoop object would have 8 entries in the `PhaseLoop` object (4 / actor, 2 / phase / actor because 2 threads for each actor). 

The `PhaseLoopIterator class` does a few interesting things. In each iteration, it calls `awaitPhaseStart()` and `awaitPhaseEnd()` in the orchestrator. If the thread is blocked, it calls [`awaitPhaseStart()`](src/gennylib/include/gennylib/PhaseLoop.hpp#L344) and stays blocked, but if it is not, then it calls [`awaitPhaseEnd()`](src/gennylib/include/gennylib/PhaseLoop.hpp#L346) and runs. In [`operator++()`](src/gennylib/include/gennylib/PhaseLoop.hpp#L364-L370), [`awaitPhaseEnd()`](src/gennylib/include/gennylib/PhaseLoop.hpp#L369) is also called to signal that the thread is a blocking thread. 

 The [`ActorPhase` object](src/gennylib/include/gennylib/PhaseLoop.hpp#L220-L307) is iterated over in the inner loop of each actor. The object is looped over for the number of times specified by duration or repeat, and then is returned. The [`IterationCompletionCheck` object is passed](src/gennylib/include/gennylib/PhaseLoop.hpp#L252) as an argument to check whether the actor is done iterating, and then the `PhaseLoopIterator object` notifies the orchestrator to step to the next phase as described above. 

##### 🎼 Orchestrator 🎼
The [`Orchestrator class`](src/gennylib/include/gennylib/Orchestrator.hpp) is used to walk all of the actors to the next phase together, and to make sure that blocking threads finish before other threads start their processes. It also outputs the `phaseNumber` to the output file using a [`metrics::Gauge`](src/gennylib/include/gennylib/Orchestrator.hpp#L95) object. The Orchestrator adds a token for every thread that starts, and removes the token once the thread is finished. When the token count is 0, it signals the end of the Phase. 

### 🔚 End 🔚
