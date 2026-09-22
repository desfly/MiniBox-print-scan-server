#include "wsd.h"
#include <string.h>

static int local_name(const char *p,size_t n,const char *name){
    const char *e,*c;size_t z=strlen(name);
    if(!p||n<z+2||*p!='<')return 0;
    e=memchr(p,'>',n);if(!e)return 0;
    c=memchr(p,':',(size_t)(e-p));
    p=c?c+1:p+1;
    return (size_t)(e-p)>=z&&!memcmp(p,name,z)&&(p[z]=='>'||p[z]==' '||p[z]=='/'||p[z]=='\t'||p[z]=='\r'||p[z]=='\n');
}
static const char *element(const char *s,size_t n,const char *name){
    size_t i;for(i=0;i<n;i++)if(s[i]=='<'&&local_name(s+i,n-i,name))return s+i;return 0;
}
static int text_of(const char *s,size_t n,const char *name,char *out,size_t cap){
    const char *p=element(s,n,name),*gt,*lt;size_t z;
    if(!p)return 1;
    gt=memchr(p,'>',n-(size_t)(p-s));if(!gt)return -1;gt++;
    lt=memchr(gt,'<',n-(size_t)(gt-s));if(!lt)return -1;
    while(gt<lt&&(*gt==' '||*gt=='\t'||*gt=='\r'||*gt=='\n'))gt++;
    while(lt>gt&&(lt[-1]==' '||lt[-1]=='\t'||lt[-1]=='\r'||lt[-1]=='\n'))lt--;
    z=(size_t)(lt-gt);if(!cap||z>=cap)return -2;
    memcpy(out,gt,z);out[z]=0;return 0;
}
static int qname_local_is(const char *s,size_t n,const char *local){
    const char *p=s,*e,*c;size_t z=strlen(local);
    while(p<s+n){
        while(p<s+n&&(*p==' '||*p=='\t'||*p=='\r'||*p=='\n'))p++;
        if(p==s+n)break;
        e=p;while(e<s+n&&*e!=' '&&*e!='\t'&&*e!='\r'&&*e!='\n')e++;
        c=memchr(p,':',(size_t)(e-p));if(c)p=c+1;
        if((size_t)(e-p)==z&&!memcmp(p,local,z))return 1;
        p=e;
    }
    return 0;
}
int mb_wsd_parse(const char *xml,size_t len,struct mb_wsd_request *out){
    const char *body;int r;
    if(!xml||!out||!len||len>65535)return -1;
    memset(out,0,sizeof *out);
    /* Require a SOAP Envelope and Body; do not accept substring-only packets. */
    if(!element(xml,len,"Envelope")||(body=element(xml,len,"Body"))==0)return -2;
    if(element(body,len-(size_t)(body-xml),"Probe"))out->kind=MB_WSD_PROBE;
    else if(element(body,len-(size_t)(body-xml),"Resolve"))out->kind=MB_WSD_RESOLVE;
    else return -3;
    r=text_of(xml,len,"MessageID",out->message_id,sizeof out->message_id);if(r)return -4;
    if(out->kind==MB_WSD_PROBE){r=text_of(body,len-(size_t)(body-xml),"Types",out->types,sizeof out->types);if(r<0)return -5;}
    else {r=text_of(body,len-(size_t)(body-xml),"Address",out->endpoint,sizeof out->endpoint);if(r)return -6;}
    return 0;
}
int mb_wsd_is_print_probe(const struct mb_wsd_request *r){
    if(!r||r->kind!=MB_WSD_PROBE)return 0;
    /* Types is a whitespace-separated list of QNames. Match the local name
     * exactly so values such as NotPrintDeviceType are never false positives. */
    return qname_local_is(r->types,strlen(r->types),"PrintDeviceType");
}
