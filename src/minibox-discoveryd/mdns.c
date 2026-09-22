#define _DEFAULT_SOURCE
#include "mdns.h"
#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define MDNS_PORT 5353
#define MDNS_ADDR "224.0.0.251"
#define TTL 120

static int put16(unsigned char *b,size_t cap,size_t *p,uint16_t v) {
    if(*p>cap||cap-*p<2)return -1;
    b[(*p)++]=(unsigned char)(v>>8); b[(*p)++]=(unsigned char)v; return 0;
}
static int put32(unsigned char *b,size_t cap,size_t *p,uint32_t v) {
    return put16(b,cap,p,(uint16_t)(v>>16))||put16(b,cap,p,(uint16_t)v)?-1:0;
}
static int name(unsigned char *b,size_t cap,size_t *p,const char *s) {
    const char *q=s,*end;
    size_t n;
    while(*q) {
        end=strchr(q,'.'); n=end?(size_t)(end-q):strlen(q);
        if(!n||n>63||*p>cap||cap-*p<n+1)return -1;
        b[(*p)++]=(unsigned char)n; memcpy(b+*p,q,n); *p+=n;
        if(!end)break;
        q=end+1;
    }
    if(*p>=cap)return -1;
    b[(*p)++]=0;
    return 0;
}
static int rr_head(unsigned char *b,size_t cap,size_t *p,const char *owner,
                   uint16_t type,uint16_t klass,uint16_t rdlen) {
    return name(b,cap,p,owner)||put16(b,cap,p,type)||
           put16(b,cap,p,klass)||put32(b,cap,p,TTL)||
           put16(b,cap,p,rdlen)?-1:0;
}
static int label(char *out,size_t cap,const char *prefix,const char *suffix) {
    int n=snprintf(out,cap,"%s%s",prefix,suffix);
    return n<0||(size_t)n>=cap?-1:0;
}
/* Each reply supplies PTR, SRV, TXT and a resolvable A record. Never encode a
 * shared PTR record with the unique-record cache-flush bit set. */
static int packet(unsigned char *out,size_t cap,const mb_service_t *s,
                  const char *host,const unsigned char ip[4]) {
    char type[96],instance[224],target[128];
    unsigned char rdata[512];
    const char *part=s->txt,*semi;
    size_t p=12,rp=0,rdlen_pos,n;
    if(!s||!host||!ip||cap<12)return -1;
    if(label(type,sizeof type,s->type,".local")||
       snprintf(instance,sizeof instance,"%s.%s",s->name,type)<0||
       strlen(s->name)+1+strlen(type)>=sizeof instance||
       label(target,sizeof target,host,".local"))return -1;
    memset(out,0,12);
    out[2]=0x84; out[3]=0x00; out[7]=4;
    if(name(rdata,sizeof rdata,&rp,instance)||
       rr_head(out,cap,&p,type,12,1,(uint16_t)rp)||
       rp>cap-p)return -1;
    memcpy(out+p,rdata,rp); p+=rp;
    rp=0;
    if(put16(rdata,sizeof rdata,&rp,0)||put16(rdata,sizeof rdata,&rp,0)||
       put16(rdata,sizeof rdata,&rp,s->port)||
       name(rdata,sizeof rdata,&rp,target)||
       rr_head(out,cap,&p,instance,33,0x8001,(uint16_t)rp)||
       rp>cap-p)return -1;
    memcpy(out+p,rdata,rp); p+=rp;
    if(name(out,cap,&p,instance)||put16(out,cap,&p,16)||
       put16(out,cap,&p,0x8001)||put32(out,cap,&p,TTL))return -1;
    rdlen_pos=p;
    if(put16(out,cap,&p,0))return -1;
    rp=0;
    while(*part) {
        semi=strchr(part,';'); n=semi?(size_t)(semi-part):strlen(part);
        if(n>255||p>cap||cap-p<n+1||rp>65535-n-1)return -1;
        out[p++]=(unsigned char)n; memcpy(out+p,part,n);
        p+=n; rp+=n+1;
        if(!semi)break;
        part=semi+1;
    }
    out[rdlen_pos]=(unsigned char)(rp>>8);
    out[rdlen_pos+1]=(unsigned char)rp;
    if(rr_head(out,cap,&p,target,1,0x8001,4)||cap-p<4)return -1;
    memcpy(out+p,ip,4); p+=4;
    return (int)p;
}
/* Resolve a DNS question safely, including compressed QNAME pointers. */
static int read_name(const unsigned char *q,size_t len,size_t *offset,
                     char *out,size_t cap) {
    size_t p=*offset,w=0; unsigned hops=0; int jumped=0;
    for(;;) {
        unsigned char n;
        if(p>=len||hops++>64)return -1;
        n=q[p++];
        if((n&0xc0)==0xc0) {
            size_t target;
            if(p>=len)return -1;
            target=((size_t)(n&0x3f)<<8)|q[p++];
            if(target>=len)return -1;
            if(!jumped){*offset=p;jumped=1;}
            p=target;continue;
        }
        if(n&0xc0)return -1;
        if(!n) {
            if(!jumped)*offset=p;
            if(w>=cap)return -1;
            out[w]=0;return 0;
        }
        if(n>63||p+n>len||w+n+1>=cap)return -1;
        if(w)out[w++]='.';
        memcpy(out+w,q+p,n);w+=n;p+=n;
    }
}
int mb_mdns_build_reply(const unsigned char *q,size_t len,
                        const mb_service_t *services,size_t count,
                        const char *hostname,const unsigned char ip[4],
                        unsigned char *out,size_t cap) {
    size_t p=12,i,j,questions;char question[256],type[96],inst[224],target[128];
    if(!q||len<12||!services||!count||!hostname||!ip||!out)return -1;
    if(q[2]&0x80)return 0; /* Never reply to another answer or announcement. */
    questions=((size_t)q[4]<<8)|q[5];
    if(!questions||questions>64)return 0;
    if(label(target,sizeof target,hostname,".local"))return -1;
    for(i=0;i<questions;i++) {
        uint16_t kind,klass;
        if(read_name(q,len,&p,question,sizeof question)||p>len||len-p<4)return -1;
        kind=(uint16_t)((q[p]<<8)|q[p+1]);
        klass=(uint16_t)((q[p+2]<<8)|q[p+3]);p+=4;
        if((klass&0x7fff)!=1)continue;
        for(j=0;j<count;j++) {
            const mb_service_t *s=&services[j];
            int match;
            if(label(type,sizeof type,s->type,".local")||
               snprintf(inst,sizeof inst,"%s.%s",s->name,type)<0||
               strlen(s->name)+1+strlen(type)>=sizeof inst)return -1;
            match=((kind==12||kind==255)&&!strcasecmp(question,type))||
                  ((kind==33||kind==16||kind==255)&&!strcasecmp(question,inst))||
                  ((kind==1||kind==255)&&!strcasecmp(question,target));
            if(match)return packet(out,cap,s,hostname,ip);
        }
    }
    return 0;
}
static int wifi_ipv4(unsigned char ip[4],struct in_addr *addr) {
    struct ifaddrs *ifs,*p;int best=-1;
    if(getifaddrs(&ifs))return -errno;
    for(p=ifs;p;p=p->ifa_next) {
        const struct sockaddr_in *sin;
        int score;
        if(!p->ifa_addr||p->ifa_addr->sa_family!=AF_INET||
           !(p->ifa_flags&IFF_UP)||(p->ifa_flags&IFF_LOOPBACK))continue;
        score=1;
        if(strstr(p->ifa_name,"sta")||strstr(p->ifa_name,"wlan")||
           strstr(p->ifa_name,"wifi")||strstr(p->ifa_name,"wl"))score=2;
        if(score<=best)continue;
        sin=(const struct sockaddr_in *)p->ifa_addr;
        *addr=sin->sin_addr;memcpy(ip,&sin->sin_addr,4);best=score;
    }
    freeifaddrs(ifs);
    return best<0?-EADDRNOTAVAIL:0;
}
static int announce(int fd,const struct sockaddr_in *dst,
                    const mb_service_t *services,size_t count,
                    const char *host,const unsigned char ip[4]) {
    unsigned char out[1500];size_t i;
    for(i=0;i<count;i++) {
        int n=packet(out,sizeof out,&services[i],host,ip);
        if(n<0)return -EINVAL;
        if(sendto(fd,out,(size_t)n,0,(const struct sockaddr*)dst,sizeof *dst)!=n)
            return -errno;
    }
    return 0;
}
static int destination(struct sockaddr_in *dst) {
    memset(dst,0,sizeof *dst);dst->sin_family=AF_INET;
    dst->sin_port=htons(MDNS_PORT);
    return inet_pton(AF_INET,MDNS_ADDR,&dst->sin_addr)==1?0:-EINVAL;
}
int mb_mdns_publish_once(const mb_service_t *services,size_t count,const char *host) {
    int fd,rc;unsigned char ip[4];struct in_addr interface_ip;
    struct sockaddr_in dst;
    if(!services||!count||!host||!host[0])return -EINVAL;
    if((rc=wifi_ipv4(ip,&interface_ip))||destination(&dst))return rc?rc:-EINVAL;
    fd=socket(AF_INET,SOCK_DGRAM,0);if(fd<0)return -errno;
    (void)setsockopt(fd,IPPROTO_IP,IP_MULTICAST_IF,&interface_ip,sizeof interface_ip);
    rc=announce(fd,&dst,services,count,host,ip);
    close(fd);return rc;
}
int mb_mdns_run(const mb_service_t *services,size_t count,
                const char *host,volatile sig_atomic_t *stop) {
    int fd,rc,one=1;time_t next=0;struct sockaddr_in bind_addr,dst;
    struct ip_mreq group;
    unsigned char ip[4],in[1500],out[1500];struct in_addr interface_ip;
    if(!services||!count||!host||!stop||destination(&dst))return -EINVAL;
    fd=socket(AF_INET,SOCK_DGRAM,0);if(fd<0)return -errno;
    (void)setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&one,sizeof one);
    memset(&bind_addr,0,sizeof bind_addr);
    bind_addr.sin_family=AF_INET;bind_addr.sin_port=htons(MDNS_PORT);
    if(bind(fd,(struct sockaddr*)&bind_addr,sizeof bind_addr)) {
        rc=-errno;close(fd);return rc;
    }
    if((rc=wifi_ipv4(ip,&interface_ip))) {close(fd);return rc;}
    memset(&group,0,sizeof group);
    group.imr_multiaddr=dst.sin_addr;group.imr_interface=interface_ip;
    if(setsockopt(fd,IPPROTO_IP,IP_ADD_MEMBERSHIP,&group,sizeof group)) {
        rc=-errno;close(fd);return rc;
    }
    (void)setsockopt(fd,IPPROTO_IP,IP_MULTICAST_IF,&interface_ip,sizeof interface_ip);
    while(!*stop) {
        fd_set fds;struct timeval tv={1,0};time_t now=time(NULL);
        if(now>=next) {
            rc=wifi_ipv4(ip,&interface_ip);
            if(!rc)rc=announce(fd,&dst,services,count,host,ip);
            if(rc)fprintf(stderr,"minibox-discoveryd: mDNS publish failed: %d\n",rc);
            else {
                printf("minibox-discoveryd: published %zu service(s) as %s.local\n",count,host);
                fflush(stdout);
            }
            next=now+60;
        }
        FD_ZERO(&fds);FD_SET(fd,&fds);
        rc=select(fd+1,&fds,NULL,NULL,&tv);
        if(rc<0){if(errno==EINTR)continue;rc=-errno;close(fd);return rc;}
        if(rc>0) {
            struct sockaddr_in peer;socklen_t plen=sizeof peer;ssize_t n;
            n=recvfrom(fd,in,sizeof in,0,(struct sockaddr*)&peer,&plen);
            if(n<=0)continue;
            rc=mb_mdns_build_reply(in,(size_t)n,services,count,host,ip,out,sizeof out);
            if(rc>0) {
                /* QU questions request unicast. Sending multicast for normal
                   questions keeps other DNS-SD browsers' caches coherent. */
                size_t qoff=12;char ignored[256];int qu=0;
                if(in[4]==0&&in[5]==1&&
                   !read_name(in,(size_t)n,&qoff,ignored,sizeof ignored)&&
                   qoff+4<=(size_t)n)qu=(in[qoff+2]&0x80)!=0;
                (void)sendto(fd,out,(size_t)rc,0,
                             (const struct sockaddr*)(qu?&peer:&dst),
                             sizeof dst);
            }
        }
    }
    close(fd);return 0;
}
