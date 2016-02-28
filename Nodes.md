Nodes
=====

These are nodes in a transition graph and define the order of
execution of operations. Unless otherwise noted, every node has

* A _name_
* A _next_ node
* Optional text field _print_ which is printed out when the node executes.

There are different classes of nodes. Some nodes primarily control the
flow of execution, while others execute operations against the server,
or update the saved state of the workload.

Control Flow Nodes
------------------

These nodes effect the flow of control for the workload, including
starting new threads.

1. ForN: This node wraps another node or set of nodes. It executes the
   underlying node N times. It executes the child node, it's next node,
   etc, until the thread of execution reaches a finish node, at which
   point it repeats the process until achieving the count of N.
   * _N_: number of times to execute the node
   * _node_: The name of the node to execute multiple times. Note this can be a
      workload node.
2. randomChoice: This node has a map of other nodes and weights
   (converted to probabilities) in the _next_ subdocument. It uses the
   pseudo-random number generator to select the next node to execute.
4. Finish: This represents an absorbing state in the graph. The
   workload is finished when the Finish node executes. There is always
   an implicit Finish state included in the workload.
5. doAll/join: These give fork/join semantics. The doAll executes all
   of it's children nodes from the sequence _childNodes_, spawning a
   thread for each child node. The doAll also uses the _next_ field
   and it should point to a join node. Parent and child threads
   (nodes) continue until reaching a
   join node. The parent waits for all children threads to complete at the
   join node, before continuing to the join node's _next_
   node. Optionally you may also define a sequence of
   _backgroundNodes_. They behave similarly to _childNodes_, except
   the join node does not wait for them. Instead the _backgroundNodes_
   are stopped once all _childNodes_ have finished.
6. workloadNode: This node wraps a _workload_. When it is executed,
   the wrapped workload is executed. When the workload finishes, the
   node continues on to its _next_ node. Parameters of the embedded
   workload can be changed using _overrides_. The _overrides_ is an optional map
   with optional fields (_threads_, _database_, _collection_,
   _runLengthMs_, _name_) that match their definition in the
   workload. The value in the override will be used in place of the
   matching value in the workload definition. Overrides may be a value
   (string or number as appropriate), a variable, or may increment a
   variable (for _threads_ and _runLengthMs_). For now, a workloadNode
   should only be called once at a time.
7. sleep: This node just introduces a delay into the execution of the
   graph. _sleepMs_ specifies the sleep time in milliseconds.
8. ifNode: This node represents a logical choice. It performs a
   logical comparison on the result from the previous node. Currently
   only the count operation saves a result, and it saves its
   count. This will be expanded. The ifNode has the following fields (note:
   fields and tests subject to change)
       * _comparison_ : The comparison test to use. This is a map and
         contains sub fields:
         * _value_ : This is a bson value to compare to the result.
         * _test_ : the test to use. Options are equals, greater,
           less, greater\_or\_equal, and less\_or\_equal. If not
           present, defaults to equals.
       * _ifNode_ : Execute this node next if the comparison evaluates
         to true
       * _elseNode_ : Execute this node next if the comparison evaluates to false.
9. spawn: This node spawns a (or multiple) new thread of execution
   that continues on independently of the existing thread of
   operation. It differs from the doAll, in that there is no
   expectation of joining back together. The parent thread continues
   on to its next node as normal after spawning the other
   thread(s). The spawn node can be used to model an arrival process
   by combining it in a loop with a sleep of the appropriate length.
       * _spawn_: This is either the name of one node to execute with
         a new thread, or a list of nodes. If it is a list, a new
         thread is created for each node name in the list.

Operation Nodes
---------------

These nodes do something, such as database commands, or manipulating
variables. They are described in [Operations.md](operations.md)

Future Nodes
------------

1. Phase Marker: Make a transition externally visible. For instance,
   to mark the end of the warmup phsae of a workload.
2. External trigger: A node that waits for an external trigger before
   executing it's next node. For multi machine client systems, this could be
   used with the phase marker to synchronize multiple client
   machines. For example, all the clients can have a phase marker node
   to indicate the end of warmup, and then immediately wait on an
   external trigger. External software could track when all the
   clients have finished their warmups, and then indicate for all of
   the clients to continue.

Future Extensions
-----------------

1. The sleep node currently sleeps for a constant amount of time. In
   the future it will have an option to sleep for a random period of
   time, and the distribution and parameters will be specifiable. An
   exponential sleep, combined wiht the spawn node would allow
   modeling a Poisson arrival process.
