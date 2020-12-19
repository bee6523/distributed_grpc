/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <memory>
#include <iostream>
#include <string>
#include <thread>

#include <grpcpp/grpcpp.h>
#include <grpc/support/log.h>

#include "cmake/build/assign4.grpc.pb.h"

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerCompletionQueue;
using grpc::Status;
using grpc::Channel;
using grpc::ClientContext;
using assign4::Request;
using assign4::Response;
using assign4::ChunkRequest;
using assign4::ChunkResponse;
using assign4::Database;
using assign4::Childnode;
using assign4::Supernode;



class DatabaseClient {
 public:
  DatabaseClient(){}
  DatabaseClient(std::shared_ptr<Channel> channel)
      : stub_(Database::NewStub(channel)) {}

  static DatabaseClient* instance(){
    static DatabaseClient database_cli;
    return &database_cli;
  }

  // Assembles the client's payload, sends it and presents the response back
  // from the server.
  std::string AccessDb(const std::string& user) {
    // Data we are sending to the server.
    Request request;
    request.set_req(user);

    // Container for the data we expect from the server.
    Response reply;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // The actual RPC.
    Status status = stub_->AccessDB(&context, request, &reply);

    // Act upon its status.
    if (status.ok()) {
      return reply.res();
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return "RPC failed";
    }
  }

 private:
  std::unique_ptr<Database::Stub> stub_;
};

class SupernodeClient {
 public:
  SupernodeClient(){}
  SupernodeClient(std::shared_ptr<Channel> channel)
      : stub_(Supernode::NewStub(channel)) {}

  static SupernodeClient* instance(){
    static SupernodeClient supernode_cli;
    return &supernode_cli;
  }
  // Assembles the client's payload, sends it and presents the response back
  // from the server.
  std::string HandleMiss(const std::string& user) {
    // Data we are sending to the server.
    Request request;
    request.set_req(user);

    // Container for the data we expect from the server.
    Response reply;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // The actual RPC.
    Status status = stub_->HandleMiss(&context, request, &reply);

    // Act upon its status.
    if (status.ok()) {
      return reply.res();
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return "RPC failed";
    }
  }

 private:
  std::unique_ptr<Supernode::Stub> stub_;
};


class ServerImpl final {
 public:
  ~ServerImpl() {
    server_->Shutdown();
    // Always shutdown the completion queue after the server.
    cq_->Shutdown();
  }

  // There is no shutdown handling in this code.
  void Run(std::string& target) {
    std::string server_address("0.0.0.0:"+target);

    ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    // Register "service_" as the instance through which we'll communicate with
    // clients. In this case it corresponds to an *asynchronous* service.
    builder.RegisterService(&service_);
    // Get hold of the completion queue used for the asynchronous communication
    // with the gRPC runtime.
    cq_ = builder.AddCompletionQueue();
    // Finally assemble the server.
    server_ = builder.BuildAndStart();
    std::cout << "Server listening on " << server_address << std::endl;

    // Proceed to the server's main loop.
    HandleRpcs();
  }

 private:
  // Class encompasing the state and logic needed to serve a request.
  class CallData {
   public:
    // Take in the "service" instance (in this case representing an asynchronous
    // server) and the completion queue "cq" used for asynchronous communication
    // with the gRPC runtime.
    CallData(Childnode::AsyncService* service, ServerCompletionQueue* cq)
        : service_(service), cq_(cq), responder_(&ctx_), status_(CREATE) {
      // Invoke the serving logic right away.
      Proceed();
    }

    void Proceed() {
      if (status_ == CREATE) {
        // Make this instance progress to the PROCESS state.
        status_ = PROCESS;

        // As part of the initial CREATE state, we *request* that the system
        // start processing SayHello requests. In this request, "this" acts are
        // the tag uniquely identifying the request (so that different CallData
        // instances can serve different requests concurrently), in this case
        // the memory address of this CallData instance.
        service_->RequestTranslateChunk(&ctx_, &request_, &responder_, cq_, cq_,
                                  this);
      }else if(status_ == START){
        // Spawn a new CallData instance to serve new clients while we process
        // the one for this CallData. The instance will deallocate itself as
        // part of its FINISH state.
        new CallData(service_, cq_);
        chunk = request_.chunk();
        index = 0;
        status_ = PROCESS;
      } else if (status_ == PROCESS) {
        
        // The actual processing.
        //slice one keyword
        do{
          index++;
          if(index==chunk.size()){
            status_ = FINISH;
            responder_.Finish(reply_, Status::OK, this);
          }
        }while(!isalnum(chunk[index]));
        int start = index;

        do{
          index++;
        }while(index<chunk.size() && isalnum(chunk[index]));
        int len=index-start;

        std::string keyword(chunk.substr(start,len));

        //TODO:: cache search

        std::string value = DatabaseClient::instance()->AccessDb(keyword);
        if(value == "\0"){
          value = SupernodeClient::instance()->HandleMiss(keyword);
        }

        //TODO:: cache add

        chunk.replace(start,len,value);

        // And we are done! Let the gRPC runtime know we've finished, using the
        // memory address of this instance as the uniquely identifying tag for
        // the event.
        if(index==chunk.size()){
          status_ = FINISH;
          reply_.set_chunk(chunk);
          responder_.Finish(reply_, Status::OK, this);
        }
      } else {
        GPR_ASSERT(status_ == FINISH);
        // Once in the FINISH state, deallocate ourselves (CallData).
        delete this;
      }
    }

   private:
    // The means of communication with the gRPC runtime for an asynchronous
    // server.
    Childnode::AsyncService* service_;
    // The producer-consumer queue where for asynchronous server notifications.
    ServerCompletionQueue* cq_;
    // Context for the rpc, allowing to tweak aspects of it such as the use
    // of compression, authentication, as well as to send metadata back to the
    // client.
    ServerContext ctx_;

    // What we get from the client.
    ChunkRequest request_;
    // What we send back to the client.
    std::string chunk;
    int index;
    ChunkResponse reply_;

    // The means to get back to the client.
    ServerAsyncResponseWriter<ChunkResponse> responder_;

    // Let's implement a tiny state machine with the following states.
    enum CallStatus { CREATE, START, PROCESS, FINISH };

    CallStatus status_;  // The current serving state.
  };

  // This can be run in multiple threads if needed.
  void HandleRpcs() {
    // Spawn a new CallData instance to serve new clients.
    new CallData(&service_, cq_.get());
    void* tag;  // uniquely identifies a request.
    bool ok;
    while (true) {
      // Block waiting to read the next event from the completion queue. The
      // event is uniquely identified by its tag, which in this case is the
      // memory address of a CallData instance.
      // The return value of Next should always be checked. This return value
      // tells us whether there is any kind of event or cq_ is shutting down.
      GPR_ASSERT(cq_->Next(&tag, &ok));
      GPR_ASSERT(ok);
      static_cast<CallData*>(tag)->Proceed();
    }
  }

  std::unique_ptr<ServerCompletionQueue> cq_;
  Childnode::AsyncService service_;
  std::unique_ptr<Server> server_;
};


int main(int argc, char** argv) {
  std::string port;
  std::string supernode;
  std::string dbserver;
  if (argc > 3) {
    port = argv[1];
    supernode = argv[2];
    dbserver = argv[3];
  } else {
    std::cout << "usage: ./child [grpc port] [super node’s ip_address]:[super node’s gRPC port] [DB server’s ip_address]:[DB server’s port] "
                << std::endl;
    return 0;
  }
  *DatabaseClient::instance() = DatabaseClient(grpc::CreateChannel(
      dbserver, grpc::InsecureChannelCredentials()));
  *SupernodeClient::instance() = SupernodeClient(grpc::CreateChannel(
      supernode, grpc::InsecureChannelCredentials()));

  ServerImpl server;
  server.Run(port);

  return 0;
}