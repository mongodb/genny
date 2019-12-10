## Jasper gRPC Client for Genny

### Overview

The `/src/third_party/jasper` directory contains files related to the
[jasper](https://github.com/mongodb/jasper) process management service C++
gRPC client that Genny uses.

There are three categories of files here:


1. `jasper.proto`: The proto file copied verbatim from the jasper repository.
2. `jasper.pb.*`: The generated protobuf C++ header and source which contain
   the definition for the protobuf interface structure.
3. `jasper.grpc.*` The generated gRPC client C++ header and source which
   contain the RPC functions to jasper.

### Versioning
`jasper.proto` should correspond to the version used in the `curator` binary
(which contains jasper, among other things). The curator version used in DSI
will be updated concurrently with the `jasper.proto` file in the genny
repository. The best way to find the actual version is to ssh into a DSI
machine and run `curator --version`, or grep `curator --version` in the DSI
repository.

The exact version of `curator` and `jasper.proto` will be stored in the DSI
repo, there is no separate version recorded in the genny repo. This allows
the source of truth to be kept in a single location.

If you need to update the version of `jasper.proto` in genny manually:

1. replace `jasper.proto` with the newer version.
2. from this directory, call:

```bash
protoc --cpp_out=. \
       --grpc_out=. \
       --plugin=protoc-gen-grpc=/data/mci/gennytoolchain/installed/x64-osx-static/tools/grpc/grpc_cpp_plugin \
       ./jasper.proto
```

You will need the `protoc` and `grpc_cpp_plugin` binaries, which are part of
the `protobuf` and `grpc++` projects respectively.
