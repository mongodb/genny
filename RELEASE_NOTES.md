Release Notes
-------------

0.4.0
=====

- Generalized overrides into value generators, and added a bunch of
  generators. Generators can be recursively used in places. New
  generators:
  - Add
  - multiply taking a general list
  - concatenate
  - increment with min, max, and increment step
  - choose generator
- Default workload name
- load file to load a list of json documents in a text file
- Add quickstart and documentation updates
- ifnode against any variable, not just previous result
- Added system node for making arbitrary system calls
- Added support for including other yaml files within a yaml file

0.3.0
=====

- Githash included in compiled binary with version info
- Changed semantics of ForN node. It now wraps a normal node (not a
  workload). Flow of control continues from that node until reaching
  the Finish node. Once the Finish node is reached, the next iteration
  of the ForN starts.
- More command line options to control execution (number of threads,
  workload, etc)
- Added spawn node, which starts a new thread of execution.
- Ability to override workload settings in workloadNode
- Added ifNode for conditional
- Internally working with bsoncxx::document::values rather than
  passing stream builders.
- Cpack support for building packages
- Added assert ability to count operation.

0.2.0
=====

- Added noop operation
- Periodic stats
- stats

First release. Basic functionality plus list above. 
