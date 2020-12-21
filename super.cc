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
 
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <net/if.h>


#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>

#include "cmake/build/assign4.grpc.pb.h"

#define BACKLOG 10
#define PACKET_SIZE 1000
#define CACHE_SIZE 30720

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::StatusCode;
using grpc::ResourceQuota;
using grpc::Channel;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::ClientAsyncResponseReader;
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
  SupernodeClient(){
    cq_ = new CompletionQueue;
  }
  SupernodeClient(std::shared_ptr<Channel> channel)
      : stub_(Supernode::NewStub(channel)) {
        cq_ = new CompletionQueue;
      }

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
    request.set_from_super(true);

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
  void TranslateChunk(const std::string& chunk){
    ChunkRequest request;
    request.set_chunk(chunk);

    AsyncClientCall* call = new AsyncClientCall;
    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    call->response_reader = stub_->AsyncTranslateChunk(&call->context, request, cq_);
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
    // std::cout << "rpc send to supernode" << std::endl;
  }
  std::string CompleteTranslateChunk(){
    void* got_tag;
    bool ok = false;
    // std::cout << "rpc recieved from supernode" << std::endl;
    GPR_ASSERT(cq_->Next(&got_tag, &ok));
    AsyncClientCall* call = static_cast<AsyncClientCall*>(got_tag);
    GPR_ASSERT(ok);
    if (call->status.ok()) {
      return call->reply.chunk();
    } else {
      std::cout << call->status.error_code() << ": " << call->status.error_message()
                << std::endl;
      return "RPC failed";
    }
  }
 private:
  struct AsyncClientCall {
    // Container for the data we expect from the server.
    ChunkResponse reply;
    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;
    // Storage for the status of the RPC upon completion.
    Status status;
    std::unique_ptr<ClientAsyncResponseReader<ChunkResponse>> response_reader;
  };
  std::unique_ptr<Supernode::Stub> stub_;
  CompletionQueue* cq_;
};

class ChildnodeClient {
 public:
  ChildnodeClient():stub_(){
    cq_=new CompletionQueue();
  }
  ChildnodeClient(std::shared_ptr<Channel> channel)
      : stub_(Childnode::NewStub(channel)) {
        cq_=new CompletionQueue();
      }

  std::string HandleMiss(const std::string& keyword){
    Request request;
    request.set_req(keyword);
    Response reply;
    ClientContext context;
    Status status=stub_->HandleMiss(&context,request,&reply);
    if (status.ok()) {
      return reply.res();
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return "RPC failed";
    }
  }
  // Assembles the client's payload, sends it and presents the response back
  // from the server.
  void TranslateChunk(const std::string& user) {
    // Data we are sending to the server.
    ChunkRequest request;
    request.set_chunk(user);

    AsyncClientCall* call = new AsyncClientCall;
    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    call->response_reader = stub_->AsyncTranslateChunk(&call->context, request, cq_);
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
    // The actual RPC.
    //Status status = stub_->TranslateChunk(&context, request, &reply);
    // Act upon its status.
  }
  std::string CompleteTranslateChunk(){
    void* got_tag;
    bool ok = false;
    GPR_ASSERT(cq_->Next(&got_tag, &ok));
    AsyncClientCall* call = static_cast<AsyncClientCall*>(got_tag);
    GPR_ASSERT(ok);
    if (call->status.ok()) {
      return call->reply.chunk();
    } else {
      std::cout << call->status.error_code() << ": " << call->status.error_message()
                << std::endl;
      return "RPC failed";
    }
  }

 private:
  struct AsyncClientCall {
    // Container for the data we expect from the server.
    ChunkResponse reply;
    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;
    // Storage for the status of the RPC upon completion.
    Status status;
    std::unique_ptr<ClientAsyncResponseReader<ChunkResponse>> response_reader;
  };
  std::unique_ptr<Childnode::Stub> stub_;
  CompletionQueue* cq_;
};

class ChildNodeManager{
  public:
  ChildNodeManager(){}
  static std::vector<ChildnodeClient>* instance(){
    static std::vector<ChildnodeClient> inst;
    return &inst;
  }
};

// Logic and data behind the server's behavior.
class SupernodeServiceImpl final : public Supernode::Service {
  public:
  Status SendInfo(ServerContext* context, const Info* request,
                  Confirm* reply) override {
    std::string info = request->ip();
    // std::cout << "recieved from supernode " << std::endl;
    *SupernodeClient::instance() = SupernodeClient(grpc::CreateChannel(
               info, grpc::InsecureChannelCredentials()));
    reply->set_checked(true);
    return Status::OK;
  }
  Status HandleMiss(ServerContext* context, const Request* request,
                  Response* reply) override{
    std::vector<ChildnodeClient>* vec=ChildNodeManager::instance();
    std::string keyword(request->req());
    std::string value;
//    std::cout << "handle keyword " << keyword << std::endl;
    gpr_mu_lock(&cache_lock);
    bool cache_hit=false;;
    for(std::vector<struct node>::iterator it = cache.begin(); it != cache.end(); it++){
      if( it->keyword == keyword){
        cache_hit = true;
        value=it->value;
        cache.erase(it);
        cache.push_back({keyword,value});

        break;
      }
    }
    gpr_mu_unlock(&cache_lock);
    if(!cache_hit){
      for(std::vector<ChildnodeClient>::iterator it=vec->begin(); it != vec->end(); it++){
        value = it->HandleMiss(keyword);
        if(value.at(0) != '\0'){
          break;
        }
      }
      if(value.at(0) == '\0' && !request->from_super()){
        value = SupernodeClient::instance()->HandleMiss(keyword);
      }
      if(value.at(0) != '\0'){
        gpr_mu_lock(&cache_lock);
        while((keyword.length()+value.length()+cache_size) > CACHE_SIZE){ //replacement: LRU
          std::vector<struct node>::iterator n = cache.begin();
          cache_size -= n->keyword.length() + n->value.length();
          cache.erase(n);
        }
        cache.push_back({keyword,value});
        gpr_mu_unlock(&cache_lock);
      }
      
    }
    if(value.at(0) != '\0'){
      reply->set_res(value);
      return Status::OK;
    }
    return Status(StatusCode::NOT_FOUND,"other child nodes also failed to find it");

  }
  Status TranslateChunk(ServerContext* context, const ChunkRequest* request,
                  ChunkResponse* reply) override{
    std::vector<ChildnodeClient>* vec=ChildNodeManager::instance();
    std::vector<ChildnodeClient>::iterator it;
    // std::cout << "recieved rpc" << std::endl;
    int num_child = vec->size();
    std::string message = request->chunk();
    int msglen = message.length();
    int len_per_child = msglen/num_child+1;
    int start=0;
    int index=-1;
    for(it=vec->begin(); it != vec->end(); it++){
      do{
        index++;
      }while(index-start < len_per_child || isalnum(message[index]) );
      it->TranslateChunk(message.substr(start,index-start));
      start=index;
    }
    std::string ret;
    for(it=vec->begin(); it != vec->end(); it++){
      ret+=it->CompleteTranslateChunk();
    }
    reply->set_chunk(ret);
    return Status::OK;
  }

  void InitCacheLock(){
    gpr_mu_init(&cache_lock);
  }

  private:
    struct node{
      std::string keyword;
      std::string value;
    };
    std::vector<struct node> cache;
    gpr_mu cache_lock;
    int cache_size=0;
};

void RunServer(std::string port) {
  std::string server_address("0.0.0.0:"+port);
  SupernodeServiceImpl service;
  service.InitCacheLock();

  grpc::EnableDefaultHealthCheckService(true);
  //grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  ResourceQuota quota;
  quota.SetMaxThreads(16);    //set maximum number of threads grpc server use
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



void* grpc_worker(void* argp){
  std::string port = *(std::string *)argp;
  RunServer(port);
  return NULL;
}

//connect_server: open connection to recieve client
int connect_server(const char *port){
    struct addrinfo hints;
    struct addrinfo *res, *p;
    int status, sockfd;
    int yes=1;

    memset(&hints,0,sizeof(hints));//initializing addrinfo
    hints.ai_family=AF_INET;
    hints.ai_socktype=SOCK_STREAM;
    hints.ai_flags=AI_PASSIVE;	
    
    if((status=getaddrinfo(NULL,port,&hints,&res))!=0){//getaddrinfo
        fprintf(stderr, "getaddrinfo: %s\n",gai_strerror(status));
        return -1;
    }
    for(p=res;p!=NULL;p=p->ai_next){//opens listening socket
        if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol))==-1){
            perror("server: socket");
            continue;
        }
        if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))==-1){
            perror("setsockopt");
            continue;
        }
        if(bind(sockfd, p->ai_addr, p->ai_addrlen)==-1){
            close(sockfd);
            perror("server: connect");
            continue;
    	}
        break;
    }    
        freeaddrinfo(res);

    if(p==NULL){
        fprintf(stderr, "client: failed to connect\n");
        return -1;
    }
    return sockfd;
}

struct hdr{
    uint8_t version;
    uint8_t userID;
    uint16_t seq;
    uint16_t length;
    uint16_t cmd;
};

int main(int argc, char** argv) {
  bool is_secondnode=false;
  std::string port;
  std::string clientport;
  if(argc>2){
    clientport=argv[1];
    port=argv[2];
    for(int i=3; i<argc;i++){
      if(strcmp(argv[i],"-s")==0){
        // std::cout << "this is second supernode" << std::endl;
        i++;
        is_secondnode=true;
        *SupernodeClient::instance() = SupernodeClient(grpc::CreateChannel(
               argv[i], grpc::InsecureChannelCredentials()));
      }else{
        std::vector<ChildnodeClient>* vec(ChildNodeManager::instance());
        vec->push_back(ChildnodeClient(grpc::CreateChannel(
               argv[i], grpc::InsecureChannelCredentials())));
      }
    }
  }else{
    std::cout << "usage: ./super [port for client] [gRPC port] [child1's ip_address]:[child1’s port] [child2’s ip_address]:[child2’s port] [child3’s ip_address]:[child3’s port] ..."
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
  memcpy(ifr.ifr_name, "enp0s31f6", IFNAMSIZ-1);    //set interface name
  ioctl(fd, SIOCGIFADDR, &ifr);
  close(fd);
  /*Extract IP Address*/
  strcpy(ip_address,inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));

  std::string super_ip(ip_address);
  if(is_secondnode){
      if(!SupernodeClient::instance()->SendInfo(super_ip+":"+port)){
        std::cout << "supernode connection failed" << std::endl;
        exit(1);
      }
      std::cout << "supernode connection success" <<std::endl;
  }

  //opening socket server
  int sockfd = connect_server(clientport.c_str());
  if(listen(sockfd, BACKLOG)==-1){
      perror("listen");
      exit(1);
  }
  struct sockaddr_storage their_addr;
  socklen_t sin_size = sizeof(their_addr);
  int newfd;

  while(true){
    newfd=accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
    if(newfd==-1){
        perror("accept");
        exit(1);
    }
    // std::cout << "client connected" << std::endl;
    char *filebuf;
    struct hdr *rcv_hdr=(struct hdr *)malloc(sizeof(struct hdr));
    struct hdr *snd_hdr;
    std::string message;
    std::string tmp;
    int filesize;
    int numbytes;

    //get filesize from client    
    numbytes=recv(newfd,rcv_hdr,8,0);
    GPR_ASSERT(ntohs(rcv_hdr->cmd)==0x0002);
    filebuf = (char *)malloc(ntohs(rcv_hdr->length)-8);
    if(recv(newfd,filebuf,ntohs(rcv_hdr->length)-8,0)<0){
        fprintf(stderr, "recieving content failed\n");
        return -1;
    }
    filesize = atoi(filebuf);
    // std::cout << "filesize: " << filebuf << std::endl;
    free(filebuf);

    //retrieve half of information that should be passed into other supernode
    while((numbytes=recv(newfd,rcv_hdr,8,0))!=0){
      if(numbytes==-1)
          perror("recv error");
      filebuf = (char *)malloc(ntohs(rcv_hdr->length)-7);
      memset(filebuf,0,ntohs(rcv_hdr->length)-7);
      if(recv(newfd,filebuf,ntohs(rcv_hdr->length)-8,0)<0){
          fprintf(stderr, "recieving content failed\n");
          return -1;
      }
      message += filebuf;
      free(filebuf);

      if(message.length() > filesize/2){
        // std::cout << "packet for supernode collected" << std::endl;
        int index = filesize/2;
        while(isalnum(message[index])){
          index--;
        }
        tmp = message.substr(index);
        message.resize(index);
        SupernodeClient::instance()->TranslateChunk(message);
        filesize-=index;
        message = tmp;
        break;
      }
    }
    // std::cout << "message left:" << message << std::endl;

    //get the rest of informations
    while((numbytes=recv(newfd,rcv_hdr,8,0))!=0){
      if(numbytes==-1)
          perror("recv error");
      filebuf = (char *)malloc(ntohs(rcv_hdr->length)-7);
      memset(filebuf,0,ntohs(rcv_hdr->length)-7);
      if(recv(newfd,filebuf,ntohs(rcv_hdr->length)-8,0)<0){
          fprintf(stderr, "recieving content failed\n");
          return -1;
      }
      if(ntohs(rcv_hdr->cmd) == 0x0004){
        // std::cout << "file read finished" << std::endl;
        free(filebuf);
        break;
      }else{
        message += filebuf;
        free(filebuf);
      }
    }
    free(rcv_hdr);
    //send rest of information to child nodes
    std::vector<ChildnodeClient>* vec=ChildNodeManager::instance();
    std::vector<ChildnodeClient>::iterator it;
    // std::cout << "string for child node collected. send it to childs" << std::endl;
    int num_child = vec->size();
    int msglen = message.length();
    int len_per_child = msglen/num_child+1;
    int start=0;
    int index=-1;
    for(it=vec->begin(); it != vec->end(); it++){
      do{
        index++;
      }while(index-start < len_per_child || isalnum(message[index]) );
      if(index > msglen){
        index=msglen;
      }
      it->TranslateChunk(message.substr(start,index-start));
      start=index;
    }
    message = "";
    //send supernode result
    // std::cout << "send supernode result" << std::endl;
    std::string ret = SupernodeClient::instance()->CompleteTranslateChunk();
    filesize = ret.length();
    index=0;
    snd_hdr = (struct hdr *) malloc(sizeof(struct hdr));
    while(filesize){
        int p_size = (filesize>(PACKET_SIZE-8))?(PACKET_SIZE-8):filesize;
        memset(snd_hdr,0,8);
        filesize -= p_size;
        snd_hdr->version=0x04;
        snd_hdr->userID=0x08;
        snd_hdr->seq=0;
        snd_hdr->length=htons(p_size+8);
        snd_hdr->cmd=htons(0x0003);
        // strcpy(filebuf,ret.substr(index,p_size).c_str());
        if(send(newfd, (void *)snd_hdr,8,0)<0){
            perror("sending file failed\n");
        }
        if(send(newfd, (void *)ret.substr(index,p_size).c_str(),p_size,0)<0){
            perror("sending file failed\n");
        }
        index+=p_size;
    }
    
    //send childnode results
    // std::cout << "send childnode results" << std::endl;
    ret = "";
    for(it=vec->begin(); it != vec->end(); it++){
      ret+=it->CompleteTranslateChunk();
    }
    filesize = ret.length();
    index=0;
    while(filesize){
        int p_size = (filesize>(PACKET_SIZE-8))?(PACKET_SIZE-8):filesize;
        filesize -= p_size;
        memset(snd_hdr,0,8);
        snd_hdr->version=0x04;
        snd_hdr->userID=0x08;
        snd_hdr->seq=0;
        snd_hdr->length=htons(p_size+8);
        snd_hdr->cmd=htons(0x0003);
        // strcpy(filebuf,ret.substr(index,p_size).c_str());
        if(send(newfd, (void *)snd_hdr,8,0)<0){
            perror("sending file failed\n");
        }
        if(send(newfd, (void *)ret.substr(index,p_size).c_str(),p_size,0)<0){
            perror("sending file failed\n");
        }
        index+=p_size;
    }
    // std::cout << "send completion message" << std::endl;
    //send completion message
    memset(snd_hdr,0,sizeof(struct hdr));
    snd_hdr->version=0x04;
    snd_hdr->userID=0x08;
    snd_hdr->seq=0;
    snd_hdr->length=htons(8);
    snd_hdr->cmd=htons(0x0004);
    if(send(newfd, (void *)snd_hdr, 8,0)<0){
        perror("sending filename failed\n");
    }
    free(snd_hdr);
    close(newfd);
  }


  pthread_join(tid, NULL);
  return 0;
}