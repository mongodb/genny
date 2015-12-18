Nodes
=====

These are nodes in a transition graph and define the order of
execution of operations. Unless otherwise noted, every node has

* A _name_
* A _next_ node 
* Optional text field _print_ which is printed out when the node
executes.

### Currently supported nodes

1. opNode: This is the default node. It executes its associated
   [operation](Operations.yml), and then moves execution to its next
   node.
   * _op_ : the operation. Or if not set will look for operation
     fields at the top level of the node definition. 
2. forN: This node wraps a node. It executes the underlying node N
   times. It ignores the child nodes next node, and does not execute
   it.
   * _N_: number of times to execute the node
   * _node_: The node to execute multiple times. Note this can be a
      workload node. 
3. randomChoice: This node has a map of other nodes and weights
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
   node continues on to its _next_ node.
7. sleep: This node just introduces a delay into the execution of the
   graph. _sleep_ specifies the sleep time in milliseconds.

### Future Nodes

1. arrival: This represents an arrival process. Will take parameters
   for various arrival processes (deterministic, random -- poisson,
   etc...). Each arrival represents a new thread of execution. Threads
   continue until they reach a finish node. We may also want to allow
   capping the total number of threads in the system from a given
   arrival node, or cummulative.
2. if: Make a choice of next node based on thread state.
3. Phase Marker: Make a transition externally visible. For instance,
   to mark the end of the warmup phsae of a workload.
4. External trigger: A node that waits for an external trigger before
   executing it's next node. For multi machine client systems, this could be
   used with the phase marker to synchronize multiple client
   machines. For example, all the clients can have a phase marker node
   to indicate the end of warmup, and then immediately wait on an
   external trigger. External software could track when all the
   clients have finished their warmups, and then indicate for all of
   the clients to continue. 
5. Extension to workloadNode: Add a field _overrides_ that is a
   sequence of settings in the underlying workload that should be
   overriden. Things that can be overridden include:
   1. variables
   2. Database name
   3. Collection name
   4. runLength
   5. number of threads
   6. workload name

