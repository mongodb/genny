```bash
protoc --cpp_out=. \
       --grpc_out=. \
       --plugin=protoc-gen-grpc=/data/mci/gennytoolchain/installed/x64-osx-static/tools/grpc/grpc_cpp_plugin \
       ./*.proto
```
