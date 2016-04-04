Value Generation
================

The tool supports a number of generic value generators and
accessors. These generators are used throughout the tool in
overrideDocuments, setVariable, workloadNode currently, and will be
expanded to most fields. All the generators create a bson value of
some sort and can be included as a value in a bson
document. Additionally, where appropriate, they can be accessed as a
number or string. It will return a runtime error if you access a value
generator as a number and is not a number.

Each value generator has a _type_ field. They are listed by _type_
below.

add
---

Adds a list of values and returns the result as a bson object. It
takes a list of _addends_. The result is a double, but can be used
where an integer is required.

1. _addends_: A yaml list of value generators or constants to add
   together

choose
------

A choose generator takes one field: _choices_, which should be a
list. The entries are converted to bson values, and can be of
arbitrary bson values.

concatenate
-----------

A concatenate generator takes a list named _parts_ of other value
generators. It will call each generator in turn to generate a string,
and will concatenate them into a single string. The concatenate will
work with string generators and number generators (the number will be
converted to a string representation). Other types will create a
runtime error.

date
----

The date generator has no arguments. It generates a bson value with
the current date.

Note: This will be extended to include picking dates in specified
range, or with an offset from the current date.

increment
---------

The increment generator accesses a variable and does a
post-increment. It has the following arguments:

* _variable_ with the name of the variable to increment.
* _increment_: Takes a value generator. The amount to increment
  by. Defaults to 1.
* _maximum_: Takes a value generator. The maximum value to increment
  to. Will wrap around to _minimum_ when it gets to _maximum_. Default
  value is max int.
* _minimum_: Takes a value generator. The value to wrap to when
  _maximum_ is reached. Default value is min int.

multiply
--------

Multiplies a list of values and returns the result as a bson object. It
takes a list of _factors_. The result is a double, but can be used
where an integer is required.

1. _factors_: A yaml list of value generators or constants to multiple
   together

randomint
---------

Generates a uniform random integer. Arguments:

1. _min_ (optional): minimum value (default = 0)
2. _max_ (optional): maximum value (default = 100)

randomstring
------------

Generates a random string. Arguments:

1. _length_ (optional): Integer length of the string (default = 10)
2. _alphabet_ (optional): String with possible characters to use in the random
   string. (default alphanumerics plus + and /)

useresult
---------

Uses the result from the last operation that saved a result, such as
the count returned from a count operation. Doesn't take any
arguments.

useval
------

Useval wraps a static bson value defined in the _value_ field.

usevar
------

Accesses a variable as is. It has one argument _variable_ to specify
the variable to access. In addition to variables defined in the
workload variables and thread variables, you can also access the
current database name (_variable_: DBName) and collection name
(_variable_: CollectionName) as strings.
