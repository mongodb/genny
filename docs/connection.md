# Connection Strings

This page describes how to use Genny connection strings in general, and in the multi-connection string case.
This case can apply to testing multiple MongoDB clusters at once, or to testing a multitenant system.
For a more broad introduction to usage, see [Using Genny](./using.md).

## Connection Strings and Pools

## Multiple Connection Strings

If multiple different connection strings are needed, such as when testing a multitenant system, we can use an override file. For the above configuration,
we can create the following override:

```
Clients:
  Default:
    URI: "mongodb://localhost:27017"
  Update:
    URI: "mongodb://localhost:27018"
```

Notice the different ports. This can be used at runtime as:

```
./run-genny workload example.yml -o override.yml
```

This will apply the override onto the workload, creating the following result:

```
Clients:
  Default:
    QueryOptions:
      maxPoolSize: 500
    URI: "mongodb://localhost:27017"
  Update:
    QueryOptions:
      maxPoolSize: 500
    URI: "mongodb://localhost:27018"
```

Genny's `evaluate` subcommand can always be used to see the result of complex configurations.

## Default

Since actors generally need a connection and not all workloads need a complicated connection or multiple pools,
simply not setting any connection pools will cause Genny to default to the following:

```
Clients:
  Default:
    QueryOptions:
      maxPoolSize: 500
    URI: "mongodb://localhost:27017"
```
