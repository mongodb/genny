Documents
=========

These are objects that can be used anywhere a bson object is needed

Currently supported documents
-----------------------------

1. _bson_: These are standard documents represented by their YAML
   representations. They are literal documents with no
   interpretation. They are the default type of documents.
2. _override_: This is a document that wraps another document,
   and can change arbitrary fields in a subdocument. Can currently
   update nested fields except for fields nested in an array. There
   are currently four kinds of overrides:
   1. randomint: Generate a random integer. Accepts min and max
      values.
   2. randomstring: Generate a random string. Accepts a length value
      (default of 10). Will support an alphabet option in the future
   3. increment: Takes a variable and uses it's value and does a post
     increment.
   4. date: The current date

Future documents
----------------

1. _generator_: This document is like an _override_, but is only
   evaluated once, eather than every time it is accessed.

Other Future Work
-----------------

Currently all the transformation of data is embedded in the _override_
document type. This functionality should be pulled out into one or
more helper functions that that return values and/or documents.
