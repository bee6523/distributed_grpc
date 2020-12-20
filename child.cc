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
using grpc::ResourceQuota;
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
    request.set_from_super(false);

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


class ChildnodeServiceImpl final : public Childnode::Service{
 public:
  Status HandleMiss(ServerContext* context, const Request* request,
                  Response* reply) override{
    std::cout << "miss handler arrived" << std::endl;
    std::string keyword(request->req());
    std::string value = DatabaseClient::instance()->AccessDb(keyword);
    reply->set_res(value);
    return Status::OK;
  }
  Status TranslateChunk(ServerContext* context, const ChunkRequest* request,
                  ChunkResponse* reply) override{
    std::cout<<"recieved request"<<std::endl;
    std::string chunk(request->chunk());
    int index = -1;
    std::cout<<"chunk: "<<chunk<<std::endl;

    std::cout<<"start processing"<<std::endl;
    while(true){
      do{
        index++;
        if(index==chunk.size()){
          std::cout << "translate complete: " << chunk << std::endl;
          reply->set_chunk(chunk);
          return Status::OK;
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
      if(value.at(0)=='\0'){
        std::cout<<"miss happened" <<std::endl;
        value = SupernodeClient::instance()->HandleMiss(keyword);
      }
      std::cout<<"found "<<value<< value.length() << std::endl;

      //TODO:: cache add

      chunk.replace(start,len,value);
      index += value.length() - len -1;

      // And we are done! Let the gRPC runtime know we've finished, using the
      // memory address of this instance as the uniquely identifying tag for
      // the event.
      if(index==chunk.size()){
        std::cout << "translate complete: " << chunk << std::endl;
        reply->set_chunk(chunk);
        return Status::OK;
      }
    }
  }
};
// There is no shutdown handling in this code.
void RunServer(std::string& port) {
  std::string server_address("0.0.0.0:"+port);
  ChildnodeServiceImpl service;

  grpc::EnableDefaultHealthCheckService(true);
  //grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  ResourceQuota quota;
  quota.SetMaxThreads(8);    //set maximum number of threads grpc server use
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.SetResourceQuota(quota);
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}


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

  RunServer(port);

  return 0;
}