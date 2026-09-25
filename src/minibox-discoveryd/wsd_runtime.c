#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include "wsd_runtime.h"
#include "wsd.h"
#include "wsd_identity.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define WSD_PORT 3702
#define WSD_ADDR "239.255.255.250"

static int multicast_addr(struct in_addr *out){
    return inet_pton(AF_INET,WSD_ADDR,out)==1?0:-EINVAL;
}
static int join_group(int fd,const struct in_addr *group_addr,
                      const struct in_addr *interface_addr){
    struct ip_mreq group;
    memset(&group,0,sizeof group);
    group.imr_multiaddr=*group_addr;group.imr_interface=*interface_addr;
    if(setsockopt(fd,IPPROTO_IP,IP_ADD_MEMBERSHIP,&group,sizeof group))return -errno;
    (void)setsockopt(fd,IPPROTO_IP,IP_MULTICAST_IF,interface_addr,sizeof *interface_addr);
    return 0;
}
static void drop_group(int fd,const struct in_addr *group_addr,
                       const struct in_addr *interface_addr){
    struct ip_mreq group;
    memset(&group,0,sizeof group);
    group.imr_multiaddr=*group_addr;group.imr_interface=*interface_addr;
    (void)setsockopt(fd,IPPROTO_IP,IP_DROP_MEMBERSHIP,&group,sizeof group);
}
static void response_id(char *out,size_t cap,unsigned long sequence){
    unsigned long now=(unsigned long)time(NULL);
    unsigned long a=(now^(unsigned long)getpid())&0xffffUL;
    unsigned long tail=((now&0xffffffffUL)<<16)^(sequence&0xffffUL);
    (void)snprintf(out,cap,"urn:uuid:4d425744-%04lx-4000-8000-%012lx",
                   a,tail&0xffffffffffffUL);
}
static void discovery_delay(unsigned long sequence){
    struct timespec ts;unsigned long ms;
    ms=((unsigned long)time(NULL)^(sequence*1103515245UL)^(unsigned long)getpid())%501UL;
    ts.tv_sec=0;ts.tv_nsec=(long)ms*1000000L;
    while(nanosleep(&ts,&ts)<0&&errno==EINTR){}
}
int mb_wsdd_run(volatile sig_atomic_t *stop){
    int fd,one=1,rc;unsigned long instance_id=(unsigned long)time(NULL),message_number=0;
    struct sockaddr_in bind_addr,peer;struct in_addr multicast;
    struct mb_wsd_identity identity;
    if(!stop||multicast_addr(&multicast))return -EINVAL;
    rc=mb_wsd_get_identity(&identity);if(rc)return rc;
    fd=socket(AF_INET,SOCK_DGRAM,0);if(fd<0)return -errno;
    (void)setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&one,sizeof one);
    memset(&bind_addr,0,sizeof bind_addr);bind_addr.sin_family=AF_INET;
    bind_addr.sin_addr.s_addr=htonl(INADDR_ANY);bind_addr.sin_port=htons(WSD_PORT);
    if(bind(fd,(struct sockaddr *)&bind_addr,sizeof bind_addr)){
        rc=-errno;close(fd);return rc;
    }
    rc=join_group(fd,&multicast,&identity.ipv4);
    if(rc){close(fd);return rc;}
    fprintf(stderr,"minibox-wsdd: listening udp/%d on %s, endpoint=%s\n",
            WSD_PORT,identity.ifname,identity.endpoint);
    while(!*stop){
        fd_set fds;struct timeval tv={1,0};struct mb_wsd_identity latest;
        FD_ZERO(&fds);FD_SET(fd,&fds);
        rc=select(fd+1,&fds,0,0,&tv);
        if(rc<0){if(errno==EINTR){rc=0;continue;}rc=-errno;break;}
        if(!mb_wsd_get_identity(&latest)&&
           latest.ipv4.s_addr!=identity.ipv4.s_addr){
            if(!join_group(fd,&multicast,&latest.ipv4)){
                drop_group(fd,&multicast,&identity.ipv4);
                identity=latest;
                fprintf(stderr,"minibox-wsdd: IPv4/interface changed to %s\n",
                        identity.ifname);
            }
        }
        if(rc==0||!FD_ISSET(fd,&fds))continue;
        {
            char in[8193],out[4096],mid[96];socklen_t peer_len=sizeof peer;
            ssize_t got=recvfrom(fd,in,sizeof in-1,0,(struct sockaddr *)&peer,&peer_len);
            struct mb_wsd_request req;int n;
            if(got<=0)continue;
            in[got]=0;
            if(mb_wsd_parse(in,(size_t)got,&req))continue;
            response_id(mid,sizeof mid,++message_number);
            n=mb_wsd_build_match(&req,identity.endpoint,identity.xaddr,mid,
                                 instance_id,message_number,out,sizeof out);
            if(n<=0)continue;
            discovery_delay(message_number);
            if(sendto(fd,out,(size_t)n,0,(struct sockaddr *)&peer,peer_len)!=n)
                fprintf(stderr,"minibox-wsdd: response send failed: %d\n",errno);
        }
    }
    drop_group(fd,&multicast,&identity.ipv4);
    close(fd);return rc<0?rc:0;
}
