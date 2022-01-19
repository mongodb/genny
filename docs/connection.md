# Connection Strings

This page describes how to use Genny connection strings in general, and in the multi-connection string case.
This case can apply to testing multiple MongoDB clusters at once, or to testing a multitenant system.
For a more broad introduction to usage, see [Using Genny](./using.md).

## Connection Strings and Pools
Genny creates connections using one or more C++ driver pools. These pools can be configured in a workload like so:

```
Clients:
  Default:
    QueryOptions:
      maxPoolSize: 500
    URI: "mongodb://localhost:27017"
  Update:
    QueryOptions:
      maxPoolSize: 500
    URI: "mongodb://localhost:27017"
```

This will configure two pools, one named `Default` and one named `Update`. The `QueryOptions` can contain
any supported [connection string options](https://docs.mongodb.com/manual/reference/connection-string/), and these will
be spliced into the final URI used to connect. Genny constructs pools lazily, so these pools will not actually be
created until a workload actor requests them.

The `Default` pool is what actors connect to unless their .cpp class determines otherwise. Some actors (the Loader, CrudActor, and RunCommand actors)
have a `ClientName` field for specifying which pool to request.

Genny's preprocessor operates on this configuration to make using it easier. The `-u` CLI option can be used to set the default URI. During preprocessing,
any pool that does not have the `URI` key set will be given this value. The default value of the default URI is `"mongodb://localhost:27017"`. Since URI
is typically not known until runtime, this means that most workloads should have a configuration more like the following:

```
Clients:
  Default:
    QueryOptions:
      maxPoolSize: 500
  Update:
    QueryOptions:
      maxPoolSize: 500
```

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
