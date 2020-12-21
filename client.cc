#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <time.h>

#define MAX_DOMAIN_LEN 100
#define PACKET_SIZE 1000
#define MAX_PORT_LEN 6
#define INIT_SEQ_NUM rand()

struct hdr{
    uint8_t version;
    uint8_t userID;
    uint16_t seq;
    uint16_t length;
    uint16_t cmd;
};

int send_error(int sockfd, int seq);

int main(int argc, char * const argv[]){
    int c,status,sockfd;
    const char *host, *port,*filename;
    FILE * snd_file;
    char buf[PACKET_SIZE];
    struct addrinfo hints;
    struct addrinfo *res, *p;

    srand(0);
    
    if(argc != 4){
        fprintf(stderr, "usage: ./client <filename> <ip_addr> <port>\n");
        exit(-1);
    }
    filename = argv[1];
    host = argv[2];
    port = argv[3];
    if((snd_file = fopen(filename,"r")) == NULL){
        fprintf(stderr, "file open failed\n");
        exit(-1);
    }

    if(atoi(port)<1000){ //check if port is appropriate
    	fprintf(stderr, "appropriate port required\n");
        exit(-1);
    }
    if(host[0]=='\0'){	//check if hostname entered
	    fprintf(stderr, "host name required\n");
	    exit(-1);
    }
	
    memset(&hints,0,sizeof(hints));//addrinfo initializing
    hints.ai_family=AF_INET;
    hints.ai_socktype=SOCK_STREAM;
	
    if((status=getaddrinfo(host,port,&hints,&res))!=0){ //getaddrinfo
    	fprintf(stderr, "getaddrinfo: %s\n",gai_strerror(status));
	    return 1;
    }
    for(p=res;p!=NULL;p=p->ai_next){ //connecting to server
	    if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol))==-1){
	        perror("client: socket");
	        continue;
	    }
	    if(connect(sockfd, p->ai_addr, p->ai_addrlen)==-1){
	        close(sockfd);
	        perror("client: connect");
	        continue;
	    }
	    break;
    }
    if(p==NULL){
	    fprintf(stderr, "client: failed to connect\n");
	    return 2;
    }

    freeaddrinfo(res);
    
    unsigned short seq=INIT_SEQ_NUM;
    struct hdr *snd_hdr = (struct hdr *) malloc(sizeof(struct hdr));
    struct hdr *rcv_hdr;
    

    //sending data to server
    unsigned int file_size = 0;
    char filechar[100]="0";
    char *file_content;
    fseek(snd_file,0,SEEK_END);
    file_size=ftell(snd_file);
    fseek(snd_file,0,SEEK_SET);
    //file size(0x0004) sending
    sprintf(filechar,"%d",file_size);
    snd_hdr=(struct hdr *)malloc(sizeof(struct hdr)+strlen(filechar)+1);
    seq++;
    snd_hdr->version=0x04;
    snd_hdr->userID=0x08;
    snd_hdr->seq=htons(seq);
    snd_hdr->length=htons(strlen(filechar)+9);
    snd_hdr->cmd=htons(0x0002);
    file_content=((char *)snd_hdr)+8;
    strcpy(file_content,filechar);
    if(send(sockfd, (void *)snd_hdr, strlen(filechar)+9,0)<0){
        perror("sending filename failed\n");
    }
    free(snd_hdr);

    while(file_size){
        int p_size = (file_size>(PACKET_SIZE-8))?(PACKET_SIZE-8):file_size;
        file_size -= p_size;
        seq+=1;
        snd_hdr = (struct hdr *) malloc(sizeof(struct hdr)+p_size);
        snd_hdr->version=0x04;
        snd_hdr->userID=0x08;
        snd_hdr->seq=htons(seq);
        snd_hdr->length=htons(p_size+8);
        snd_hdr->cmd=htons(0x0003);
        file_content = ((char *)snd_hdr)+8;
        if(fread(file_content,p_size,1,snd_file)<=0){
            send_error(sockfd, seq+1);
            perror("file read failed\n");
        }
        if(send(sockfd, (void *)snd_hdr,p_size+8,0)<0){
            perror("sending file failed\n");
        }
        free(snd_hdr);
    }

    //date store packet(0x0004) sending
    snd_hdr=(struct hdr *)malloc(sizeof(struct hdr)+strlen(filename)+1);
    seq++;
    snd_hdr->version=0x04;
    snd_hdr->userID=0x08;
    snd_hdr->seq=htons(seq);
    snd_hdr->length=htons(strlen(filename)+9);
    snd_hdr->cmd=htons(0x0004);
    file_content=((char *)snd_hdr)+8;
    strcpy(file_content,filename);
    if(send(sockfd, (void *)snd_hdr, strlen(filename)+9,0)<0){
        perror("sending filename failed\n");
    }
    free(snd_hdr);

    FILE *tf;
    if((tf=fopen("result.txt","w+"))==NULL){
        fprintf(stderr,"creating result file failed\n");
        return -1;
    }
    int numbytes;
    char *filebuf;
    rcv_hdr=(struct hdr *)buf;
    while((numbytes=recv(sockfd,buf,8,0))!=0){
        if(numbytes==-1)
            perror("recv error");
        switch(ntohs(rcv_hdr->cmd)){
            case 0x0003:
                filebuf = (char *)malloc(ntohs(rcv_hdr->length)-8);
                if(recv(sockfd,filebuf,ntohs(rcv_hdr->length)-8,0)<0){
                    fprintf(stderr, "recieving content failed\n");
                    return -1;
                }
                fwrite(filebuf,ntohs(rcv_hdr->length)-8,1,tf);
                free(filebuf);
                break;
            case 0x0004:
                fclose(tf);
                close(sockfd);
                exit(0);
                break;
            case 0x0005:
                fprintf(stdout, "error recieved from client. session end\n");
                return -1;
            default:
                fprintf(stderr, "something went wrong. info: %d\n", ntohs(rcv_hdr->cmd));
                return -1;
        }
    }



    close(sockfd);
    return 0;
}
int send_error(int sockfd, int seq){
    struct hdr *snd_hdr = (struct hdr *) malloc(sizeof(struct hdr));
    snd_hdr->version=0x04;
    snd_hdr->userID=0x08;
    snd_hdr->seq=htons(seq);
    snd_hdr->length=htons(8);
    snd_hdr->cmd=htons(0x0005);
    if(send(sockfd,(const void *)snd_hdr,8,0)<0){
        free(snd_hdr);
        return -1;
    }
    free(snd_hdr);
    return 0;
}
