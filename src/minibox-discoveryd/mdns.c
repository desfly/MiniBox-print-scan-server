#include "mdns.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#define MDNS_PORT 5353
#define MDNS_ADDR "224.0.0.251"
#define TTL 120
static int put16(unsigned char*b,size_t c,size_t*p,uint16_t v){if(*p+2>c)return-1;b[(*p)++]=(unsigned char)(v>>8);b[(*p)++]=(unsigned char)v;return 0;}
static int put32(unsigned char*b,size_t c,size_t*p,uint32_t v){return put16(b,c,p,(uint16_t)(v>>16))||put16(b,c,p,(uint16_t)v)?-1:0;}
static int name(unsigned char*b,size_t c,size_t*p,const char*s){const char*q=s,*e;size_t n;while(*q){e=strchr(q,'.');n=e?(size_t)(e-q):strlen(q);if(!n||n>63||*p>c||n>c-*p-(*p<c?1:0))return-1;b[(*p)++]=(unsigned char)n;memcpy(b+*p,q,n);*p+=n;if(!e)break;q=e+1;}if(*p>=c)return-1;b[(*p)++]=0;return 0;}
static int rr_head(unsigned char*b,size_t c,size_t*p,const char*n,uint16_t t,uint16_t rdlen){return name(b,c,p,n)||put16(b,c,p,t)||put16(b,c,p,0x8001)||put32(b,c,p,TTL)||put16(b,c,p,rdlen)?-1:0;}
static int packet(unsigned char*b,size_t c,const mb_service_t*s,const char*host){char type[96],inst[224],target[128];unsigned char r[1024];size_t p=12,rp=0,rdpos;const char*x=s->txt,*semi;size_t n;snprintf(type,sizeof(type),"%s.local",s->type);snprintf(inst,sizeof(inst),"%s.%s",s->name,type);snprintf(target,sizeof(target),"%s.local",host);memset(b,0,12);b[2]=0x84;b[7]=3;rp=0;if(name(r,sizeof(r),&rp,inst))return-1;if(rr_head(b,c,&p,type,12,(uint16_t)rp)||p+rp>c)return-1;memcpy(b+p,r,rp);p+=rp;rp=0;if(put16(r,sizeof(r),&rp,0)||put16(r,sizeof(r),&rp,0)||put16(r,sizeof(r),&rp,s->port)||name(r,sizeof(r),&rp,target))return-1;if(rr_head(b,c,&p,inst,33,(uint16_t)rp)||p+rp>c)return-1;memcpy(b+p,r,rp);p+=rp;if(name(b,c,&p,inst)||put16(b,c,&p,16)||put16(b,c,&p,0x8001)||put32(b,c,&p,TTL))return-1;rdpos=p;if(put16(b,c,&p,0))return-1;rp=0;while(*x){semi=strchr(x,';');n=semi?(size_t)(semi-x):strlen(x);if(n>255||p+n+1>c)return-1;b[p++]=(unsigned char)n;memcpy(b+p,x,n);p+=n;rp+=n+1;if(!semi)break;x=semi+1;}b[rdpos]=(unsigned char)(rp>>8);b[rdpos+1]=(unsigned char)rp;return(int)p;}
int mb_mdns_publish_once(const mb_service_t*services,size_t count,const char*hostname){int fd,rc=0;struct sockaddr_in dst;unsigned char b[1500];size_t i;if(!services||!count||!hostname||!hostname[0])return-EINVAL;fd=socket(AF_INET,SOCK_DGRAM,0);if(fd<0)return-errno;memset(&dst,0,sizeof(dst));dst.sin_family=AF_INET;dst.sin_port=htons(MDNS_PORT);if(inet_pton(AF_INET,MDNS_ADDR,&dst.sin_addr)!=1){close(fd);return-EINVAL;}for(i=0;i<count;i++){int n=packet(b,sizeof(b),&services[i],hostname);if(n<0){rc=-EINVAL;break;}if(sendto(fd,b,(size_t)n,0,(struct sockaddr*)&dst,sizeof(dst))!=n){rc=-errno;break;}}close(fd);return rc;}
