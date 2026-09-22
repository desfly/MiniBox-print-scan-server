#define _DEFAULT_SOURCE
#include <assert.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
/* Exercise the actual DNS packet encoder/decoder without requiring multicast hardware. */
#include "../src/minibox-discoveryd/mdns.c"

static size_t question(unsigned char *buf, const char *qname, unsigned type) {
    size_t p=12;
    memset(buf,0,512);
    buf[5]=1;
    assert(name(buf,512,&p,qname)==0);
    assert(put16(buf,512,&p,(uint16_t)type)==0);
    assert(put16(buf,512,&p,1)==0);
    return p;
}
static void check(const mb_service_t *services,size_t count,const char *qname,unsigned type,unsigned expected) {
    unsigned char query[512],reply[1500];struct in_addr ip;
    size_t n=question(query,qname,type);
    assert(inet_pton(AF_INET,"192.168.55.250",&ip)==1);
    int len=answer_packet(reply,sizeof reply,services,count,"minibox",query,n,ip);
    if(expected==0){assert(len==0);return;}
    assert(len>=12);
    assert(reply[2]==0x84);
    assert((((unsigned)reply[6]<<8)|reply[7])==expected);
}
int main(void) {
    mb_service_t services[2]={0};
    snprintf(services[0].name,sizeof services[0].name,"HP LaserJet M1522n @ MiniBox");
    snprintf(services[0].type,sizeof services[0].type,"_ipp._tcp");
    snprintf(services[0].txt,sizeof services[0].txt,"rp=ipp/print;ty=HP LaserJet M1522n");
    services[0].port=631;
    snprintf(services[1].name,sizeof services[1].name,"HP Scanner @ MiniBox");
    snprintf(services[1].type,sizeof services[1].type,"_uscan._tcp");
    snprintf(services[1].txt,sizeof services[1].txt,"rs=eSCL");
    services[1].port=8080;
    check(services,2,"_ipp._tcp.local",12,4);
    check(services,2,"_uscan._tcp.local",12,4);
    check(services,2,"HP LaserJet M1522n @ MiniBox._ipp._tcp.local",33,3);
    check(services,2,"HP LaserJet M1522n @ MiniBox._ipp._tcp.local",16,3);
    check(services,2,"_services._dns-sd._udp.local",12,2);
    check(services,2,"minibox.local",1,1);
    check(services,2,"missing.local",1,0);
    puts("mDNS query contract: OK");
    return 0;
}
