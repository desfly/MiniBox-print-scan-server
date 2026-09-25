#include "../src/minibox-ipp/ipp.h"
#include <assert.h>
#include <string.h>
static int contains(const unsigned char*b,size_t n,const char*s){size_t z=strlen(s),i;if(z>n)return 0;for(i=0;i+z<=n;i++)if(!memcmp(b+i,s,z))return 1;{const unsigned char raw[]={2,0,0,2,0,0,0,1,1,0x49,0,15,'d','o','c','u','m','e','n','t','-','f','o','r','m','a','t',0,24,'a','p','p','l','i','c','a','t','i','o','n','/','o','c','t','e','t','-','s','t','r','e','a','m',3};unsigned char bad[sizeof raw];memcpy(bad,raw,sizeof raw);assert(ipp_raw_format_supported(raw,sizeof raw)==1);memcpy(bad+29,"application/pdf",15);assert(ipp_raw_format_supported(bad,sizeof bad)==0);}return 0;}
static int contains_bytes(const unsigned char*b,size_t n,const unsigned char*v,size_t z){size_t i;if(z>n)return 0;for(i=0;i+z<=n;i++)if(!memcmp(b+i,v,z))return 1;return 0;}
static int attribute_has_exact_value(const unsigned char *b,size_t n,const char *name,const char *value){
    size_t p=9,expected_name=strlen(name),expected_value=strlen(value);
    while(p<n){size_t nl,vl;unsigned char tag=b[p++];
        if(tag==0x03)return 0;
        if(tag>=0x01&&tag<=0x05)continue;
        if(p+2>n)return 0;
        nl=((size_t)b[p]<<8)|b[p+1];p+=2;
        if(nl>n-p||n-p-nl<2)return 0;
        if(nl==expected_name&&!memcmp(b+p,name,nl)){
            p+=nl;vl=((size_t)b[p]<<8)|b[p+1];p+=2;
            return vl==expected_value&&vl<=n-p&&!memcmp(b+p,value,vl);
        }
        p+=nl;vl=((size_t)b[p]<<8)|b[p+1];p+=2;
        if(vl>n-p)return 0;
        p+=vl;
    }
    return 0;
}

/* Check the actual on-wire IPP attribute tags and lengths, not just substrings. */
static int find_attribute(const unsigned char*b,size_t n,const char*key,
                          unsigned char*tag,const unsigned char**value,size_t*length){
    size_t p=8,k=strlen(key);
    if(n<10||b[8]!=0x01)return 0;
    while(p<n&&b[p]!=0x03){
        unsigned char t=b[p++];
        size_t kl,vl;
        if(t<=0x05)continue; /* Operation/Printer/Unsupported group delimiter */
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
    /* Responses MUST begin with operation group and the two ordered,
     * typed language attributes (RFC 8011 4.1.4). */
    size_t first=9,second;
    assert(b[8]==0x01&&b[first]==0x47);
    assert(b[first+1]==0&&b[first+2]==18); /* attributes-charset */
    assert(!memcmp(b+first+3,"attributes-charset",18));
    first+=1+2+18;
    assert(b[first]==0&&b[first+1]==5);
    assert(!memcmp(b+first+2,"utf-8",5));
    second=first+2+5;
    assert(b[second]==0x48);
    assert(b[second+1]==0&&b[second+2]==27);
    assert(!memcmp(b+second+3,"attributes-natural-language",27));
    assert(find_attribute(b,n,"uri-authentication-supported",&tag,&v,&len));
    assert(tag==0x44&&len==4&&!memcmp(v,"none",4));
    assert(find_attribute(b,n,"uri-security-supported",&tag,&v,&len));
    assert(tag==0x44&&len==4&&!memcmp(v,"none",4));
    assert(find_attribute(b,n,"charset-supported",&tag,&v,&len));
    assert(tag==0x47&&len==5&&!memcmp(v,"utf-8",5));
    assert(find_attribute(b,n,"generated-natural-language-supported",&tag,&v,&len));
    assert(tag==0x48&&len==2&&!memcmp(v,"en",2));
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

int main(void){const unsigned char q[]={2,0,0,0x0b,0,0,0,7};const unsigned char print_op[]={0,0,0,2};const unsigned char validate_op[]={0,0,0,4};const unsigned char attrs_op[]={0,0,0,0x0b};struct ipp_request r;unsigned char z[2048];size_t n;assert(ipp_parse_header(q,sizeof q,&r)==0);assert(r.major==2&&r.minor==0&&r.operation==IPP_OP_GET_PRINTER_ATTRIBUTES&&r.request_id==7);assert(!strcmp(ipp_operation_name(IPP_OP_PRINT_JOB),"Print-Job"));assert(ipp_parse_header(q,7,&r)==-1);n=ipp_build_status(z,sizeof z,&r,0);assert(n>9);assert(z[0]==2&&z[1]==0&&z[2]==0&&z[3]==0&&z[7]==7&&z[8]==1&&z[n-1]==3);n=ipp_build_printer_attributes(z,sizeof z,&r,"ipp://minibox.local/ipp/print");assert(n>9);assert(z[0]==2&&z[1]==0&&z[7]==7);assert(z[8]==0x01);assert(z[n-1]==0x03);check_wire_attributes(z,n);assert(contains(z,n,"printer-name"));assert(contains(z,n,"HP LaserJet M1522n @ MiniBox"));assert(contains(z,n,"printer-uri-supported"));assert(contains(z,n,"ipp://minibox.local/ipp/print"));assert(contains(z,n,"operations-supported"));assert(contains_bytes(z,n,print_op,sizeof print_op));assert(contains_bytes(z,n,validate_op,sizeof validate_op));assert(contains_bytes(z,n,attrs_op,sizeof attrs_op));assert(contains(z,n,"document-format-supported"));assert(attribute_has_exact_value(z,n,"printer-name","HP LaserJet M1522n @ MiniBox"));assert(attribute_has_exact_value(z,n,"printer-make-and-model","HP LaserJet M1522n"));assert(attribute_has_exact_value(z,n,"printer-info","MiniBox network print server"));assert(attribute_has_exact_value(z,n,"document-format-supported","application/octet-stream"));
n=ipp_build_print_job_response(z,sizeof z,&r,42,"ipp://minibox.local/ipp/print/jobs/42");
assert(n>9&&z[8]==0x01&&z[n-1]==0x03);
assert(contains(z,n,"job-uri"));
assert(contains(z,n,"ipp://minibox.local/ipp/print/jobs/42"));
assert(contains(z,n,"job-id"));
assert(contains(z,n,"job-state"));
assert(contains(z,n,"job-state-reasons"));
{ const unsigned char completed[]={0,0,0,9}; assert(contains_bytes(z,n,completed,sizeof completed)); }
return 0;}
