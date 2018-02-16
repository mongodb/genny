Operation Nodes
===============

These are nodes that execute basic operations, such as a
find_one. These operations *do* something. The largest class of
operations are crud operations. The goal is to support all CRUD spec
operations.

CRUD Operations
---------------

### Supported Operations

* Collection operations
  * count: Takes a document in the _filter_ field
  * create\_index: document _keys_ and index options _options_
  * delete\_many: document _filter_ and delete options _options_
  * delete\_one: document _filter_ and delete options _options_
  * distinct: document _filter_ and distinct options _options_
  * drop: drops current collection
  * find: document _filter_ and find options _options_
  * find\_one: document _filter_ and find options _options_
  * find\_one\_and\_delete: document _filter_ and find and delete options _options_
  * find\_one\_and\_replace: document _filter_, replacement document
    _replace_,  and find and replace options _options_
  * find\_one\_and\_update: document _filter_, update document
    _update_,  and find and update options _options_
  * insert\_many: This operates two ways. Both have insert options _options_
    * field _container_ with a list of documents to insert
    * field _doc_ with one document, and field _times_ for the number
      of times to insert the document. This is a convenience option
      for building the container of documents.
  * insert\_one: document _doc_ to insert and insert options _options_
  * list\_indexes: Calls list indexes. Doesn't take any options
  * name: Gets the collection name. Doesn't take any options
  * read\_preference: set the default read preference
    _read\_preference_ on the collection
  * replace\_one: document _filter_, replacement document
    _replacement_,  and update options _options_
  * update\_many: document _filter_, update document
    _replacement_,  and update options _options_
  * update\_one: document _filter_, update document
    _replacement_,  and update options _options_
  * write\_concern: set the default _write\_concern_ on the collection
* Database operations
  * command (needs to be renamed run\_command). Takes an arbitrary document _command_ and executes it
  * create\_collection: Create collection with string name
    _collection\_name_ and create collection options _options_

Helper operations
-----------------

* set\_variable: Set a workload or thread variable. If an existing
  thread or workload variable exists, it will udpate that value. If
  there is no existing variable, it will be added to the thread
  variables (tvariables). The variable can be set from a _value_ or an
  explicit _operation_, which can be any
  [value generator](Values.md). Database name and collection name can
  also be set.  Fields:
      * _target_: string value name of the variable to update (or
        database or collection)
      * _value_: A json/yaml value to set the variable to
      * _operation_: A map specifying how to generate the
        value. The operations are the generic [value generators](Values.md).
* load\_file: This loads documents into the database. It takes a file
  containing a sequence of json documents, such as those produced by
  mongo-export.
  Fields:
      * _path_ (optional): The directory path to the file. See example
        [load\_file.yml](examples/load_file.yml) for common usage of
        this option. A top level _path_ node in the yaml will be
        replaced with the path of the current yaml file.
      * _file\_name_: The name of the file to load. This is combined
        with the _path_ if it exists.
