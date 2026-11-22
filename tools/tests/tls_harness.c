#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include "include/types.h"
#include "net/tls.h"
#include "crypto/crypto.h"

static int fd; static int corrupt_after = -1; static int bytes_seen = 0;
static int t_send(const u8* d,u32 n){ return write(fd,d,n)==(ssize_t)n?0:-1; }
static int t_recv(u8* b,u32 max,u32 ticks){ struct pollfd p={fd,POLLIN,0}; int r=poll(&p,1,ticks*10); if(r<=0) return -1; ssize_t n=read(fd,b,max); if(n>0 && corrupt_after>=0){ for(ssize_t i=0;i<n;i++){ if(bytes_seen+i==corrupt_after) b[i]^=1; } } if(n>0) bytes_seen+=n; return n>0?(int)n:0; }
static int unhex(const char* h,u8* o){int n=0; for(;h[0]&&h[1];h+=2){unsigned v; sscanf(h,"%2x",&v); o[n++]=v;} return n;}
int main(int argc,char**argv){
  int port=atoi(argv[1]); const char* pinhex = argc>2 ? argv[2] : NULL; if(argc>3) corrupt_after=atoi(argv[3]);
  fd=socket(AF_INET,SOCK_STREAM,0); struct sockaddr_in a={0}; a.sin_family=AF_INET; a.sin_port=htons(port); inet_pton(AF_INET,"127.0.0.1",&a.sin_addr);
  if(connect(fd,(struct sockaddr*)&a,sizeof a)){perror("connect");return 9;}
  tls_transport_t t={t_send,t_recv}; u8 pin[32], want[32]; int strong=0;
  const u8* wp=NULL; if(pinhex && strcmp(pinhex,"-")){ unhex(pinhex,want); wp=want; }
  int r=tls_connect(&t,"localhost",wp,pin,&strong);
  printf("tls_connect=%d (%s) rdrand=%d\n",r,tls_error_text(r),strong);
  if(r!=0){ printf("pin="); for(int i=0;i<32;i++)printf("%02x",pin[i]); printf("\n"); return 1; }
  printf("pin="); for(int i=0;i<32;i++)printf("%02x",pin[i]); printf("\n");
  const char* req="GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"; tls_write((const u8*)req,strlen(req));
  u8 buf[4096]; int total=0; int n; while((n=tls_read(buf,sizeof buf-1,300))>0){ if(total==0){ buf[n>60?60:n]=0; printf("debut reponse: %.40s\n",buf);} total+=n; }
  printf("octets recus=%d, fin=%d\n",total,n); tls_close(); return total>0?0:2; }
