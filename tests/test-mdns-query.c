#include "../src/minibox-discoveryd/mdns.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static size_t query(unsigned char *q,const char *s,unsigned kind,unsigned klass){
    size_t p=12;
    const char *part=s,*dot;
    memset(q,0,512);
    q[5]=1; /* one question */
    while(*part){
        size_t len;
        dot=strchr(part,'.');
        len=dot?(size_t)(dot-part):strlen(part);
        assert(len>0&&len<=63&&p+len+1<512);
        q[p++]=(unsigned char)len;
        memcpy(q+p,part,len);p+=len;
        if(!dot)break;
        part=dot+1;
    }
    q[p++]=0;q[p++]=(unsigned char)(kind>>8);q[p++]=(unsigned char)kind;
    q[p++]=(unsigned char)(klass>>8);q[p++]=(unsigned char)klass;
    return p;
}
static int contains(const unsigned char *b,size_t n,const char *text){
    size_t i,z=strlen(text);
    for(i=0;i+z<=n;i++)if(!memcmp(b+i,text,z))return 1;
    return 0;
}
int main(void){
    mb_service_t svc[2];
    unsigned char q[512],a[1500],ip[4]={192,168,55,250};
    size_t n;int len;
    assert(!mb_service_load("overlay/etc/minibox/services.d/ipp-printer.service",&svc[0]));
    assert(!mb_service_load("overlay/etc/minibox/services.d/scanner.service",&svc[1]));
    n=query(q,"_ipp._tcp.local",12,1);
    len=mb_mdns_build_reply(q,n,svc,2,"OpenWrt",ip,a,sizeof a);
    assert(len>0&&a[2]==0x84&&a[7]==4);
    assert(contains(a,(size_t)len,"HP LaserJet M1522n"));
    assert(memcmp(a+len-4,ip,4)==0); /* target host A record, not a phantom host */
    n=query(q,"_uscan._tcp.local",12,1);
    len=mb_mdns_build_reply(q,n,svc,2,"OpenWrt",ip,a,sizeof a);
    assert(len>0&&a[7]==4&&contains(a,(size_t)len,"HP LaserJet M1522n"));
    n=query(q,"OpenWrt.local",1,1);
    len=mb_mdns_build_reply(q,n,svc,2,"OpenWrt",ip,a,sizeof a);
    assert(len>0&&memcmp(a+len-4,ip,4)==0);
    n=query(q,"unknown._tcp.local",12,1);
    assert(mb_mdns_build_reply(q,n,svc,2,"OpenWrt",ip,a,sizeof a)==0);
    n=query(q,"_ipp._tcp.local",12,1);
    q[n-1]=0; /* class becomes 0 (unsupported), no answer */
    q[n-2]=0;
    assert(mb_mdns_build_reply(q,n,svc,2,"OpenWrt",ip,a,sizeof a)==0);
    n=query(q,"_ipp._tcp.local",12,1);
    assert(mb_mdns_build_reply(q,n-1,svc,2,"OpenWrt",ip,a,sizeof a)<0);
    q[12]=0xc0;q[13]=0xff; /* out-of-bounds compression pointer */
    assert(mb_mdns_build_reply(q,n,svc,2,"OpenWrt",ip,a,sizeof a)<0);
    puts("mDNS DNS-SD query/response contract OK");
    return 0;
}
