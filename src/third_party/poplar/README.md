/data/mci/gennytoolchain/installed/x64-osx/tools/protobuf/protoc

```bash
/data/mci/gennytoolchain/installed/x64-osx/tools/protobuf/protoc \
       --plugin=protoc-gen-grpc="/data/mci/gennytoolchain/installed/x64-osx/tools/grpc/grpc_cpp_plugin" \
       --cpp_out=poplarlib \
       --grpc_out=poplarlib \
       ./*.proto
```
