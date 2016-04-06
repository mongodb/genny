Documents
=========

These are objects that can be used anywhere a bson object is needed

Currently supported documents
-----------------------------

### templating document

This is a document with support for templating for value
generation. The documents are represent by YAML, but have special
values that are replaced with generated data. A _templating_ document
supports all of the [value generators](Values.md). In the place for
the generated value should be a yaml document, with key of $typename,
where typename is the type of the value generator. For instance, to
have a field z with value choosen from a random integer between 0 and
10, you would use:

    z: {$randomint: {min: 0, max: 100}}

There is an [example](examples/templating.yml) of operations using
different templating operations in the examples directory.

### bson document

These are standard documents represented by their YAML
representations. They are literal documents with no
interpretation. Use this type of document when you want to disable the
templating.

### override document

This is a document that wraps another document, and can change
arbitrary fields in a subdocument. It is equivalent to the templating
document, but specifies the generated data in a structure outside the
document, unlike the templating document. The override document has two
required fields:

1. _doc_: This is a _bson_ document.
2. _overrides_: This is a yaml map of fields in _doc_ to
      override. Each pair gives the field name (key) to override, and
      the value to use. For example, a: {type: randomint} with replace
      the value for field a, with a random integer. Note that it
      replaces the value associated with the key a. Any
      [value generator](Values.md) may be used for the value.

[override.yml](examples/override.yml) is an example workload with a
number of operations using override documents.
