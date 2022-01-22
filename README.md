# distributed_grpc
Distributed System using grpc

## Brief description
At first tried to implement asynchronous single-thread server, but changed into Synchronous multi-threaded server.
However, some grpc requests(TranslateChunk in Childnode and Supernode) are asynchronous, therefore it can send multiple grpc to child in parellel.
caches are simply implemented using std::vector, replacement policy is LRU.

for client-supernode socket connection, I used assignment1 codes.
for supernode-childnode, there can be two grpc request:
    TranslateChunk that are request for converting fragment of file,
    HandleMiss for handling DB miss case.
for supernode-supernode, same as supernode-childnode, but one more:
    SendInfo for identifying each other

## build order:
mkdir -p cmake/build
pushd ./cmake/build
cmake ../..
make -j
popd

stub files(assign4.grpc.pb.cc, assign4.grpc.pb.h, etc.) should be generated in cmake/build directory to properly included in source files.
binary files are also created in cmake/build directory(probably not necessary).
after process, result.txt should be created at main directory.
