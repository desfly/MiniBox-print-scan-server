#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include "mdns.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#define MDNS_PORT 5353
#define MDNS_ADDR "224.0.0.251"
#define TTL 120
static int put16(unsigned char*b,size_t c,size_t*p,uint16_t v){if(*p+2>c)return-1;b[(*p)++]=(unsigned char)(v>>8);b[(*p)++]=(unsigned char)v;return 0;}
static int put32(unsigned char*b,size_t c,size_t*p,uint32_t v){return put16(b,c,p,(uint16_t)(v>>16))||put16(b,c,p,(uint16_t)v)?-1:0;}
static int name(unsigned char*b,size_t c,size_t*p,const char*s){const char*q=s,*e;size_t n;while(*q){e=strchr(q,'.');n=e?(size_t)(e-q):strlen(q);if(!n||n>63||*p>c||n>c-*p-(*p<c?1:0))return-1;b[(*p)++]=(unsigned char)n;memcpy(b+*p,q,n);*p+=n;if(!e)break;q=e+1;}if(*p>=c)return-1;b[(*p)++]=0;return 0;}
static int rr_head(unsigned char*b,size_t c,size_t*p,const char*n,uint16_t t,uint16_t rdlen){return name(b,c,p,n)||put16(b,c,p,t)||put16(b,c,p,t==12?1:0x8001)||put32(b,c,p,TTL)||put16(b,c,p,rdlen)?-1:0;}
static int packet(unsigned char*b,size_t c,const mb_service_t*s,const char*host){char type[96],inst[224],target[128];unsigned char r[1024];size_t p=12,rp=0,rdpos;const char*x=s->txt,*semi;size_t n;snprintf(type,sizeof(type),"%s.local",s->type);snprintf(inst,sizeof(inst),"%s.%s",s->name,type);snprintf(target,sizeof(target),"%s.local",host);memset(b,0,12);b[2]=0x84;b[7]=3;rp=0;if(name(r,sizeof(r),&rp,inst))return-1;if(rr_head(b,c,&p,type,12,(uint16_t)rp)||p+rp>c)return-1;memcpy(b+p,r,rp);p+=rp;rp=0;if(put16(r,sizeof(r),&rp,0)||put16(r,sizeof(r),&rp,0)||put16(r,sizeof(r),&rp,s->port)||name(r,sizeof(r),&rp,target))return-1;if(rr_head(b,c,&p,inst,33,(uint16_t)rp)||p+rp>c)return-1;memcpy(b+p,r,rp);p+=rp;if(name(b,c,&p,inst)||put16(b,c,&p,16)||put16(b,c,&p,0x8001)||put32(b,c,&p,TTL))return-1;rdpos=p;if(put16(b,c,&p,0))return-1;rp=0;while(*x){semi=strchr(x,';');n=semi?(size_t)(semi-x):strlen(x);if(n>255||p+n+1>c)return-1;b[p++]=(unsigned char)n;memcpy(b+p,x,n);p+=n;rp+=n+1;if(!semi)break;x=semi+1;}b[rdpos]=(unsigned char)(rp>>8);b[rdpos+1]=(unsigned char)rp;return(int)p;}
int mb_mdns_publish_once(const mb_service_t*services,size_t count,const char*hostname){int fd,rc=0;struct sockaddr_in dst;unsigned char b[1500];size_t i;if(!services||!count||!hostname||!hostname[0])return-EINVAL;fd=socket(AF_INET,SOCK_DGRAM,0);if(fd<0)return-errno;memset(&dst,0,sizeof(dst));dst.sin_family=AF_INET;dst.sin_port=htons(MDNS_PORT);if(inet_pton(AF_INET,MDNS_ADDR,&dst.sin_addr)!=1){close(fd);return-EINVAL;}for(i=0;i<count;i++){int n=packet(b,sizeof(b),&services[i],hostname);if(n<0){rc=-EINVAL;break;}if(sendto(fd,b,(size_t)n,0,(struct sockaddr*)&dst,sizeof(dst))!=n){rc=-errno;break;}}close(fd);return rc;}

/* Answer DNS-SD questions as well as sending periodic unsolicited announcements. */
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/select.h>
#include <time.h>
static int read_dns_name(const unsigned char *b,size_t n,size_t *p,char *out,size_t cap){
    size_t i=*p,w=0;unsigned hops=0;int jumped=0;
    while(i<n&&++hops<128){unsigned len=b[i++];
        if(!len){if(!jumped)*p=i;if(w>=cap)return -1;out[w]=0;return 0;}
        if((len&0xc0)==0xc0){unsigned ptr;if(i>=n)return -1;ptr=((len&0x3f)<<8)|b[i++];if(ptr>=n)return -1;if(!jumped)*p=i;jumped=1;i=ptr;continue;}
        if(len&0xc0||len>63||len>n-i||w+len+1>=cap)return -1;
        if(w) { out[w++]='.'; }
        memcpy(out+w,b+i,len);w+=len;i+=len;
    }return -1;
}
static int dns_equal(const char*a,const char*b){
    while(*a&&*b){unsigned char x=(unsigned char)*a++,y=(unsigned char)*b++;
        if(x>='A'&&x<='Z')x=(unsigned char)(x+32);
        if(y>='A'&&y<='Z')y=(unsigned char)(y+32);
        if(x!=y)return 0;
    }return !*a&&!*b;
}
/* Select the source address used by the kernel route to the mDNS group.
 * Do not advertise an arbitrary Ethernet address on a Wi-Fi-only bridge. */
static int mdns_source_ipv4(struct in_addr *addr){
    int fd;struct sockaddr_in dst,local;socklen_t len=sizeof local;
    fd=socket(AF_INET,SOCK_DGRAM,0);if(fd<0)return -1;
    memset(&dst,0,sizeof dst);dst.sin_family=AF_INET;dst.sin_port=htons(MDNS_PORT);
    if(inet_pton(AF_INET,MDNS_ADDR,&dst.sin_addr)!=1||
       connect(fd,(struct sockaddr*)&dst,sizeof dst)!=0||
       getsockname(fd,(struct sockaddr*)&local,&len)!=0){close(fd);return -1;}
    close(fd);
    if(local.sin_family!=AF_INET||local.sin_addr.s_addr==htonl(INADDR_ANY))return -1;
    *addr=local.sin_addr;return 0;
}
static int append_a(unsigned char*b,size_t cap,size_t *p,const char*host,struct in_addr addr){
    unsigned char ip[4];memcpy(ip,&addr.s_addr,4);
    if(rr_head(b,cap,p,host,1,4)||*p>cap||cap-*p<4)return -1;
    memcpy(b+*p,ip,4);*p+=4;return 0;
}
static int answer_packet(unsigned char *out,size_t cap,const mb_service_t *services,size_t count,
                         const char *hostname,const unsigned char *query,size_t qlen,struct in_addr addr){
    size_t qp=12,p=12;unsigned questions,answers=0,i,j;char qname[256],type[96],inst[224],target[128];
    if(qlen<12||(query[2]&0x80))return 0;
    questions=((unsigned)query[4]<<8)|query[5];if(questions>32)return 0;
    memset(out,0,12);out[2]=0x84;out[3]=0;
    snprintf(target,sizeof target,"%s.local",hostname);
    for(j=0;j<questions;j++){
        unsigned qt,qclass;int want_a;
        if(read_dns_name(query,qlen,&qp,qname,sizeof qname)||qlen-qp<4)return 0;
        qt=((unsigned)query[qp]<<8)|query[qp+1];qclass=((unsigned)query[qp+2]<<8)|query[qp+3];qp+=4;
        if((qclass&0x7fff)!=1)continue;
        want_a=dns_equal(qname,target)&&(qt==1||qt==255);
        if(want_a){if(append_a(out,cap,&p,target,addr))return 0;answers++;continue;}
        for(i=0;i<count;i++){
            unsigned char record[1500];int n;
            snprintf(type,sizeof type,"%s.local",services[i].type);
            snprintf(inst,sizeof inst,"%s.%s",services[i].name,type);
            if(!((qt==12||qt==255)&&dns_equal(qname,type))&&
               !((qt==33||qt==16||qt==255)&&dns_equal(qname,inst))&&
               !((qt==12||qt==255)&&dns_equal(qname,"_services._dns-sd._udp.local")))continue;
            n=packet(record,sizeof record,&services[i],hostname);
            if(n<12||p>cap||(size_t)(n-12)>cap-p)return 0;
            memcpy(out+p,record+12,(size_t)n-12);p+=(size_t)n-12;answers+=3;
        }
    }
    if(!answers)return 0;
    out[6]=(unsigned char)(answers>>8);out[7]=(unsigned char)answers;
    return (int)p;
}
int mb_mdns_serve(const mb_service_t *services,size_t count,const char *hostname,
                  volatile sig_atomic_t *stop){
    int fd,one=1;struct sockaddr_in bindaddr,dst;struct ip_mreq membership;struct in_addr source;
    unsigned char in[1500],out[1500];time_t last=0;
    if(!services||!count||!hostname||!stop)return -EINVAL;
    if(mdns_source_ipv4(&source))return -ENETUNREACH;
    fd=socket(AF_INET,SOCK_DGRAM,0);if(fd<0)return -errno;
    setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&one,sizeof one);
    memset(&bindaddr,0,sizeof bindaddr);bindaddr.sin_family=AF_INET;
    bindaddr.sin_port=htons(MDNS_PORT);bindaddr.sin_addr.s_addr=htonl(INADDR_ANY);
    if(bind(fd,(struct sockaddr*)&bindaddr,sizeof bindaddr)){int e=-errno;close(fd);return e;}
    memset(&membership,0,sizeof membership);
    inet_pton(AF_INET,MDNS_ADDR,&membership.imr_multiaddr);
    membership.imr_interface=source;
    if(setsockopt(fd,IPPROTO_IP,IP_ADD_MEMBERSHIP,&membership,sizeof membership)){int e=-errno;close(fd);return e;}
    memset(&dst,0,sizeof dst);dst.sin_family=AF_INET;dst.sin_port=htons(MDNS_PORT);
    inet_pton(AF_INET,MDNS_ADDR,&dst.sin_addr);
    if(setsockopt(fd,IPPROTO_IP,IP_MULTICAST_IF,&source,sizeof source)){int e=-errno;close(fd);return e;}
    while(!*stop){
        fd_set fds;struct timeval timeout={1,0};int ready;time_t now=time(NULL);
        if(now-last>=60){size_t i;for(i=0;i<count;i++){int n=packet(out,sizeof out,&services[i],hostname);
            if(n>0)sendto(fd,out,(size_t)n,0,(struct sockaddr*)&dst,sizeof dst);}
            last=now;
        }
        FD_ZERO(&fds);FD_SET(fd,&fds);ready=select(fd+1,&fds,NULL,NULL,&timeout);
        if(ready>0&&FD_ISSET(fd,&fds)){
            struct sockaddr_in peer;socklen_t peerlen=sizeof peer;
            ssize_t n=recvfrom(fd,in,sizeof in,0,(struct sockaddr*)&peer,&peerlen);
            if(n>0){
                int m=answer_packet(out,sizeof out,services,count,hostname,in,(size_t)n,source);
                if(m>0)sendto(fd,out,(size_t)m,0,(struct sockaddr*)&dst,sizeof dst);
            }
        }else if(ready<0&&errno!=EINTR){int e=-errno;close(fd);return e;}
    }
    close(fd);return 0;
}
