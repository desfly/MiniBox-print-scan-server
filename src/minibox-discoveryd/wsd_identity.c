#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include "wsd_identity.h"
#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

static int wifi_name_score(const char *ifname){
    if(!ifname)return 0;
    if(!strncmp(ifname,"wlan",4)||!strncmp(ifname,"wwan",4)||
       !strncmp(ifname,"wifi",4)||!strncmp(ifname,"wlp",3)||
       !strncmp(ifname,"wlx",3)||!strncmp(ifname,"sta",3)||
       !strncmp(ifname,"phy",3))return 3;
    return 1;
}

static int preferred_ipv4(char *ifname,size_t ifcap,struct in_addr *addr){
    struct ifaddrs *ifs,*p;int best=-1,rc=-EADDRNOTAVAIL;
    if(!ifname||!ifcap||!addr)return -EINVAL;
    if(getifaddrs(&ifs))return -errno;
    for(p=ifs;p;p=p->ifa_next){
        const struct sockaddr_in *sin;int score;
        if(!p->ifa_addr||p->ifa_addr->sa_family!=AF_INET||
           !(p->ifa_flags&IFF_UP)||(p->ifa_flags&IFF_LOOPBACK))continue;
        score=wifi_name_score(p->ifa_name);
        if(score<=best)continue;
        sin=(const struct sockaddr_in *)p->ifa_addr;
        if(strlen(p->ifa_name)>=ifcap)continue;
        strcpy(ifname,p->ifa_name);
        *addr=sin->sin_addr;best=score;rc=0;
    }
    freeifaddrs(ifs);
    return rc;
}

static int interface_mac(const char *ifname,unsigned char mac[6]){
    int fd,rc=0;struct ifreq ifr;
    if(!ifname||!mac)return -EINVAL;
    fd=socket(AF_INET,SOCK_DGRAM,0);if(fd<0)return -errno;
    memset(&ifr,0,sizeof ifr);
    if(strlen(ifname)>=sizeof ifr.ifr_name){close(fd);return -EINVAL;}
    strcpy(ifr.ifr_name,ifname);
    if(ioctl(fd,SIOCGIFHWADDR,&ifr)<0)rc=-errno;
    else memcpy(mac,ifr.ifr_hwaddr.sa_data,6);
    close(fd);return rc;
}

int mb_wsd_get_identity(struct mb_wsd_identity *out){
    unsigned char mac[6];char ip[INET_ADDRSTRLEN];int n,i,nonzero=0;
    if(!out)return -EINVAL;
    memset(out,0,sizeof *out);
    if(preferred_ipv4(out->ifname,sizeof out->ifname,&out->ipv4))return -EADDRNOTAVAIL;
    if(interface_mac(out->ifname,mac))return -ENODEV;
    for(i=0;i<6;i++)if(mac[i])nonzero=1;
    if(!nonzero)return -ENODEV;
    n=snprintf(out->serial,sizeof out->serial,"%02x%02x%02x%02x%02x%02x",
               mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    if(n!=12)return -EINVAL;
    n=snprintf(out->endpoint,sizeof out->endpoint,
               "urn:uuid:4d424f58-0000-4000-8000-%s",out->serial);
    if(n<0||(size_t)n>=sizeof out->endpoint)return -EINVAL;
    if(!inet_ntop(AF_INET,&out->ipv4,ip,sizeof ip))return -errno;
    n=snprintf(out->xaddr,sizeof out->xaddr,"http://%s/cgi-bin/minibox-wsd",ip);
    if(n<0||(size_t)n>=sizeof out->xaddr)return -EINVAL;
    n=snprintf(out->presentation,sizeof out->presentation,"http://%s/",ip);
    if(n<0||(size_t)n>=sizeof out->presentation)return -EINVAL;
    return 0;
}
