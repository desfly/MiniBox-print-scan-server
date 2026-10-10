#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include "wsd_identity.h"
#include "../minibox-identity/m1522_identity.h"
#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <string.h>
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

int mb_wsd_get_identity(struct mb_wsd_identity *out){
    char ip[INET_ADDRSTRLEN];int n;
    if(!out)return -EINVAL;
    memset(out,0,sizeof *out);
    if(preferred_ipv4(out->ifname,sizeof out->ifname,&out->ipv4))return -EADDRNOTAVAIL;
    n=snprintf(out->serial,sizeof out->serial,"%s",MINIBOX_MFP_SERIAL);
    if(n!=(int)strlen(MINIBOX_MFP_SERIAL))return -EINVAL;
    n=snprintf(out->endpoint,sizeof out->endpoint,"%s",MINIBOX_MFP_URN_UUID);
    if(n<0||(size_t)n>=sizeof out->endpoint)return -EINVAL;
    if(!inet_ntop(AF_INET,&out->ipv4,ip,sizeof ip))return -errno;
    n=snprintf(out->xaddr,sizeof out->xaddr,"http://%s/StableWSDiscoveryEndpoint/schemas-xmlsoap-org_ws_2005_04_discovery",ip);
    if(n<0||(size_t)n>=sizeof out->xaddr)return -EINVAL;
    n=snprintf(out->presentation,sizeof out->presentation,"http://%s/",ip);
    if(n<0||(size_t)n>=sizeof out->presentation)return -EINVAL;
    return 0;
}

