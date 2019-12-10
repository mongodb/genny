```bash
protoc --cpp_out=poplarlib \
       --grpc_out=poplarlib \
       --plugin=protoc-gen-grpc=/data/mci/gennytoolchain/installed/x64-osx-static/tools/grpc/grpc_cpp_plugin \
       ./*.proto
```
