Documents
=========

These are objects that can be used anywhere a bson object is needed

Currently supported documents
-----------------------------

1. _bson_: These are standard documents represented by their YAML
   representations. They are literal documents with no
   interpretation. They are the default type of documents.
2. _override_: This is a document that wraps another document,
   and can change arbitrary fields in a subdocument. The override
   document has two required fields:
   1. _doc_: This is a _bson_ document.
   2. _overrides_: This is a yaml map of fields in _doc_ to
      override. Each pair gives the field name (key) to override, and
      the value to use. For example, a: {type: randomint} with replace
      the value for field a, with a random integer. Note that it replaces
      the value associated with the key a. Any [value generator](Values.md) may be
      used for the value.
