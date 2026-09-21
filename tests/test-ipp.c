#include "../src/minibox-ipp/ipp.h"
#include <assert.h>
#include <string.h>
static int contains(const unsigned char*b,size_t n,const char*s){size_t z=strlen(s),i;if(z>n)return 0;for(i=0;i+z<=n;i++)if(!memcmp(b+i,s,z))return 1;return 0;}
static int contains_bytes(const unsigned char*b,size_t n,const unsigned char*v,size_t z){size_t i;if(z>n)return 0;for(i=0;i+z<=n;i++)if(!memcmp(b+i,v,z))return 1;return 0;}

/* Check the actual on-wire IPP attribute tags and lengths, not just substrings. */
static int find_attribute(const unsigned char*b,size_t n,const char*key,
                          unsigned char*tag,const unsigned char**value,size_t*length){
    size_t p=9,k=strlen(key);
    if(n<10||b[8]!=0x04)return 0;
    while(p<n&&b[p]!=0x03){
        unsigned char t=b[p++];
        size_t kl,vl;
        if(p+2>n)return 0;
        kl=((size_t)b[p]<<8)|b[p+1];p+=2;
        if(p+kl+2>n)return 0;
        if(kl==k&&!memcmp(b+p,key,k)){
            p+=kl;vl=((size_t)b[p]<<8)|b[p+1];p+=2;
            if(p+vl>n)return 0;
            *tag=t;*value=b+p;*length=vl;return 1;
        }
        p+=kl;vl=((size_t)b[p]<<8)|b[p+1];p+=2;
        if(p+vl>n)return 0;
        p+=vl;
    }
    return 0;
}
static void check_wire_attributes(const unsigned char*b,size_t n){
    unsigned char tag;
    const unsigned char *v;
    size_t len;
    assert(find_attribute(b,n,"printer-name",&tag,&v,&len));
    assert(tag==0x42&&len==strlen("HP LaserJet M1522n @ MiniBox"));
    assert(!memcmp(v,"HP LaserJet M1522n @ MiniBox",len));
    assert(find_attribute(b,n,"printer-make-and-model",&tag,&v,&len));
    assert(tag==0x41&&len==strlen("HP LaserJet M1522n"));
    assert(!memcmp(v,"HP LaserJet M1522n",len));
    assert(find_attribute(b,n,"printer-uri-supported",&tag,&v,&len));
    assert(tag==0x45);
    assert(find_attribute(b,n,"operations-supported",&tag,&v,&len));
    assert(tag==0x23&&len==4);
    assert(find_attribute(b,n,"document-format-default",&tag,&v,&len));
    assert(tag==0x49&&len==strlen("application/octet-stream"));
}

int main(void){const unsigned char q[]={2,0,0,0x0b,0,0,0,7};const unsigned char print_op[]={0,0,0,2};const unsigned char validate_op[]={0,0,0,4};const unsigned char attrs_op[]={0,0,0,0x0b};struct ipp_request r;unsigned char z[2048];size_t n;assert(ipp_parse_header(q,sizeof q,&r)==0);assert(r.major==2&&r.minor==0&&r.operation==IPP_OP_GET_PRINTER_ATTRIBUTES&&r.request_id==7);assert(!strcmp(ipp_operation_name(IPP_OP_PRINT_JOB),"Print-Job"));assert(ipp_parse_header(q,7,&r)==-1);assert(ipp_build_status(z,sizeof z,&r,0)==9);assert(z[0]==2&&z[1]==0&&z[2]==0&&z[3]==0&&z[7]==7&&z[8]==3);n=ipp_build_printer_attributes(z,sizeof z,&r,"ipp://minibox.local/ipp/print");assert(n>9);assert(z[0]==2&&z[1]==0&&z[7]==7);assert(z[8]==0x04);assert(z[n-1]==0x03);check_wire_attributes(z,n);assert(contains(z,n,"printer-name"));assert(contains(z,n,"HP LaserJet M1522n @ MiniBox"));assert(contains(z,n,"printer-uri-supported"));assert(contains(z,n,"ipp://minibox.local/ipp/print"));assert(contains(z,n,"operations-supported"));assert(contains_bytes(z,n,print_op,sizeof print_op));assert(contains_bytes(z,n,validate_op,sizeof validate_op));assert(contains_bytes(z,n,attrs_op,sizeof attrs_op));assert(contains(z,n,"document-format-supported"));return 0;}
