#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>      // define structures like hostent
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#define DATA_SIZE 1000
#define HEAD_SIZE 16
#define MAXPKT_SIZE DATA_SIZE+HEAD_SIZE


typedef struct{
  int seq;
  int ack;
  int length;
  short flg;
  short rwnd;
  char data[DATA_SIZE]; 
}packet;
//function prototype
void sendURL(int,char*,struct sockaddr*,socklen_t);
int cal_corruption(float pc);
int cal_loss(float pl);
int createACK(int sockfd,struct sockaddr* serv_addr,socklen_t serverlen,int wantseq,packet* recvpkt,int recvpktlen);

void error(char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[])
{
    int sockfd; //Socket descriptor
    int portno,n,recvpktlen;
    float pl,pc;//probability
    socklen_t serverlen;
    FILE* fp;
    char buffer[MAXPKT_SIZE];
    char rbuf[4*MAXPKT_SIZE];
    int wantseq=0,newwantseq=0,ack=0;
    short flg=0,rwnd=4*MAXPKT_SIZE;
    struct sockaddr_in serv_addr;
    
    fd_set rd_fds;
    FD_ZERO(&rd_fds);
    int newsock;
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    
    struct hostent *server; //contains tons of information, including the server's IP address
    if (argc < 7) {
       fprintf(stderr,"format: name, hostname, port number, request filename, save filename, pl, pc\n");
       exit(0);
    }
    
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_DGRAM, 0); //create a new socket
    if (sockfd < 0) 
        error("ERROR opening socket");
    
    server = gethostbyname(argv[1]); //takes a string like "www.yahoo.com", and returns a struct hostent which contains information, as IP address, address type, the length of the addresses...
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; //initialize server's address
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    serverlen=sizeof(serv_addr);

    //send URL
    sendURL(sockfd,argv[3],(struct sockaddr*)&serv_addr,serverlen);
    //printf("here");
    //open file with same URL
    fp=fopen(argv[4],"w+");
    if(fp==NULL) printf("error open file");
    
    pl = atof(argv[5]);
    pc = atof(argv[6]);
    //create recvpkt
    packet* recvpkt;
    srand((unsigned)time(NULL));//random seed

    //receive pkt loop
    while(1){
       bzero(buffer,MAXPKT_SIZE);
       //read from socket
    recvpktlen = recvfrom(sockfd,buffer,MAXPKT_SIZE,0,(struct sockaddr*)&serv_addr,&serverlen);
    //printf("recvpktlen:%d",recvpktlen);
    if (recvpktlen < 0) error("ERROR receiving to socket");
    recvpkt=(packet*)buffer;
    //file not found
    if(recvpkt->flg == 404)
    {
    printf("request file %s not found\n",argv[3]);
    break;
    }
    //emulate corruption and lost
    if(!cal_loss(pl))//drop pkt upon loss
    {
        printf("seq:%d packet was lost\n",recvpkt->seq);
        continue;
    }
    else if(!cal_corruption(pc)) //drop pkt upon corruption
    
    {
        printf("seq:%d packet was corrupted\n",recvpkt->seq);
         continue;
    }
    
       //createACK
       newwantseq=createACK(sockfd,(struct sockaddr*)&serv_addr,serverlen,wantseq,recvpkt,recvpktlen);
       //write to file
       if(wantseq!=newwantseq)//directly write to file
    {
        //printf("data:%s\n",buffer+HEAD_SIZE);
        
        n = fwrite(buffer+HEAD_SIZE,1,recvpkt->length,fp);
        //printf("write: %d\n",n);
          if(n < 0) error("ERROR writing to file");
    }
       else//drop for GBN
       {   
               printf("seq:%d packet was manually dropped for GO_BACK_N\n",recvpkt->seq);
       }
       wantseq=newwantseq;//update wantseq for next loop
       //FIN=1 
       if((recvpkt->flg==1)&&((recvpkt->seq)<wantseq))
       {
           FD_SET (sockfd, &rd_fds);
           newsock = select(sockfd+1,&rd_fds,NULL,NULL,&tv);
           if(newsock)continue;
           else break;
       }
    }//end of while

    close(sockfd); //close socket
    fclose(fp);//close file
    return 0;
}

//sendURL initial sendto 
void sendURL(int sockfd,char* URL,struct sockaddr* serv_addr,socklen_t serverlen)
{
    int n;
    packet URLpacket;
    URLpacket.seq=0;
    URLpacket.ack=0;
    URLpacket.flg=2;
    URLpacket.length=sizeof(URL);
    URLpacket.rwnd= 4*MAXPKT_SIZE;
    bzero(URLpacket.data,DATA_SIZE);
    strncpy(URLpacket.data,URL,DATA_SIZE);
    
    n = sendto(sockfd,&URLpacket,HEAD_SIZE+strlen((char*)(&URLpacket)+HEAD_SIZE),0,serv_addr,serverlen);
    if (n < 0) error("ERROR sending to socket");
}

//calculate probablity of corruption pkt
int cal_corruption(float pc)
{
 float randpc;
  //srand((unsigned)time(NULL));//random seed
  randpc=((float)rand())/(float)(RAND_MAX/1.0);
   // printf("randpc: %f\n",randpc);
  if(randpc<=pc)
  return 0;
  else return 1;
}

//calculate probablity of dropped pkt
int cal_loss(float pl)
{
  float randpl;
  //srand((unsigned)time(NULL));//random seed
  randpl=((float)rand())/(float)(RAND_MAX/1.0);
    //printf("randpl: %f\n",randpl);
  if(randpl<=pl)
  return 0;
  else return 1;
}

//get seq# from pkt then construct ACK packet (using cumulative ACK)
int createACK(int sockfd,struct sockaddr* serv_addr,socklen_t serverlen,int wantseq,packet* recvpkt,int recvpktlen)
{
    int n;
    recvpktlen=recvpktlen-HEAD_SIZE;
    packet ackpkt;
    ackpkt.length=0;
    if(recvpkt->seq == wantseq)
    {
    wantseq+=recvpkt->length;
    ackpkt.ack=wantseq;//new ACK
    ackpkt.seq+=1;
    ackpkt.flg=4;
    }
    else
    {    
    ackpkt.ack=wantseq;//cumulative ACK
    ackpkt.flg=4;
    }
    if((recvpkt->flg ==1)&&(recvpkt->seq<=wantseq))
    {
        ackpkt.flg=5;
    }
    n = sendto(sockfd,&ackpkt,HEAD_SIZE,0,serv_addr,serverlen);//no data send HEAD_SIZE
    if (n < 0) error("ERROR sending to socket");
    printf("ACK:%d was sent by client\n",ackpkt.ack);
    return wantseq;//return changed wantseq
}

