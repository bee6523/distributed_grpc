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

#include <iostream>
#include <memory>
#include <string>

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
 
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>

#include "cmake/build/assign4.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::ResourceQuota;
using grpc::Channel;
using grpc::ClientContext;
using assign4::Request;
using assign4::Response;
using assign4::ChunkRequest;
using assign4::ChunkResponse;
using assign4::Info;
using assign4::Confirm;
using assign4::Database;
using assign4::Childnode;
using assign4::Supernode;


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
  bool SendInfo(const std::string& ip){
      Info info;
      info.set_ip(ip);

      Confirm reply;
      ClientContext context;
      Status status = stub_->SendInfo(&context, info, &reply);
      if (status.ok()) {
        return reply.checked();
      } else {
        std::cout << status.error_code() << ": " << status.error_message()
                    << std::endl;
        return false;
      }
  }

 private:
  std::unique_ptr<Supernode::Stub> stub_;
};

class ChildnodeClient {
 public:
  ChildnodeClient(){}
  ChildnodeClient(std::shared_ptr<Channel> channel)
      : stub_(Childnode::NewStub(channel)) {}

  // Assembles the client's payload, sends it and presents the response back
  // from the server.
  std::string TranslateChunk(const std::string& user) {
    // Data we are sending to the server.
    Request request;
    request.set_req(user);

    // // Container for the data we expect from the server.
    // Response reply;

    // // Context for the client. It could be used to convey extra information to
    // // the server and/or tweak certain RPC behaviors.
    // ClientContext context;

    // // The actual RPC.
    // Status status = stub_->HandleMiss(&context, request, &reply);

    // // Act upon its status.
    // if (status.ok()) {
    //   return reply.res();
    // } else {
    //   std::cout << status.error_code() << ": " << status.error_message()
    //             << std::endl;
    //   return "RPC failed";
    // }
  }

 private:
  std::unique_ptr<Childnode::Stub> stub_;
};


// Logic and data behind the server's behavior.
class SupernodeServiceImpl final : public Supernode::Service {
  Status SendInfo(ServerContext* context, const Info* request,
                  Confirm* reply) override {
    std::string info = request->ip();
    std::cout << "recieved from supernode " << info << std::endl;
    reply->set_checked(true);
    return Status::OK;
  }
  Status HandleMiss(ServerContext* context, const Request* request,
                  Response* reply) override{
    //TODO

  }
  Status TranslateChunk(ServerContext* context, const ChunkRequest* request,
                  ChunkResponse* reply) override{
    //TODO
  }
};

void RunServer(std::string port) {
  std::string server_address("0.0.0.0:"+port);
  SupernodeServiceImpl service;

  grpc::EnableDefaultHealthCheckService(true);
  //grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
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


class ChildNodeManager{
  public:
  ChildNodeManager(){}
  static std::vector<ChildnodeClient>* instance(){
    static std::vector<ChildnodeClient> inst;
    return &inst;
  }
};

void* grpc_worker(void* argp){
  std::string port = *(std::string *)argp;
  RunServer(port);
}

int main(int argc, char** argv) {
  bool is_secondnode=false;
  std::string port;
  if(argc>2){
    port=argv[1];
    for(int i=2; i<argc;i++){
      std::cout << argv[i] << std::endl;
      if(strcmp(argv[i],"-s")==0){
        std::cout << "thisissecondnode" << std::endl;
        i++;
        is_secondnode=true;
        *SupernodeClient::instance() = SupernodeClient(grpc::CreateChannel(
               argv[i], grpc::InsecureChannelCredentials()));
      }else{
        std::vector<ChildnodeClient> *vec(ChildNodeManager::instance());
        vec->push_back(ChildnodeClient(grpc::CreateChannel(
               argv[i], grpc::InsecureChannelCredentials())));
      }
    }
  }else{
    std::cout << "usage: ./super 12345 [gRPC port] [child1's ip_address]:[child1’s port] [child2’s ip_address]:[child2’s port] [child3’s ip_address]:[child3’s port] ..."
            << std::endl;
    return 0;
  }

  pthread_t tid;
  int rc=pthread_create(&tid,NULL,grpc_worker, (void *)&port);
  if(rc!=0){
      std::cout << "thread creation failed" << std::endl;
      return 0;
  }
  
  //identify current node's ip
  char ip_address[15];
  int fd;
  struct ifreq ifr;
  fd = socket(AF_INET, SOCK_DGRAM, 0);
  ifr.ifr_addr.sa_family = AF_INET;
  memcpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);
  ioctl(fd, SIOCGIFADDR, &ifr);
  close(fd);
  /*Extract IP Address*/
  strcpy(ip_address,inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));

  std::string super_ip(ip_address);
  if(is_secondnode){
      if(!SupernodeClient::instance()->SendInfo(super_ip+":"+port)){
        std::cout << "supernode connection failed" << std::endl;
        goto terminate;
      }
  }
  std::cout << "supernode connection success" <<std::endl;

terminate:
  pthread_join(tid, NULL);
  return 0;
}