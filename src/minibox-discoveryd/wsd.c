#include "wsd.h"
#include <stdarg.h>
#include <stdio.h>
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
static const char *element_end(const char *s,size_t n,const char *name){
    size_t i,z=strlen(name);const char *p,*e,*c;
    for(i=0;i+3<n;i++)if(s[i]=='<'&&s[i+1]=='/'){
        p=s+i+2;e=memchr(p,'>',n-i-2);if(!e)return 0;
        c=memchr(p,':',(size_t)(e-p));if(c)p=c+1;
        if((size_t)(e-p)==z&&!memcmp(p,name,z))return e+1;
    }
    return 0;
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
    const char *env,*env_end,*body,*body_end;size_t env_len,body_len;int r;
    if(!xml||!out||!len||len>65535)return -1;
    memset(out,0,sizeof *out);
    env=element(xml,len,"Envelope");if(!env)return -2;
    env_end=element_end(env,len-(size_t)(env-xml),"Envelope");if(!env_end)return -2;
    env_len=(size_t)(env_end-env);
    body=element(env,env_len,"Body");if(!body)return -2;
    body_end=element_end(body,env_len-(size_t)(body-env),"Body");if(!body_end)return -2;
    body_len=(size_t)(body_end-body);
    if(element(body,body_len,"Probe"))out->kind=MB_WSD_PROBE;
    else if(element(body,body_len,"Resolve"))out->kind=MB_WSD_RESOLVE;
    else return -3;
    r=text_of(env,env_len,"MessageID",out->message_id,sizeof out->message_id);if(r)return -4;
    if(out->kind==MB_WSD_PROBE){r=text_of(body,body_len,"Types",out->types,sizeof out->types);if(r<0)return -5;}
    else {r=text_of(body,body_len,"Address",out->endpoint,sizeof out->endpoint);if(r)return -6;}
    return 0;
}
int mb_wsd_is_print_probe(const struct mb_wsd_request *r){
    return r&&r->kind==MB_WSD_PROBE&&qname_local_is(r->types,strlen(r->types),"PrintDeviceType");
}
int mb_wsd_is_scan_probe(const struct mb_wsd_request *r){
    return r&&r->kind==MB_WSD_PROBE&&qname_local_is(r->types,strlen(r->types),"ScanDeviceType");
}
int mb_wsd_is_device_probe(const struct mb_wsd_request *r){
    return r&&r->kind==MB_WSD_PROBE&&qname_local_is(r->types,strlen(r->types),"Device");
}
int mb_wsd_probe_supported(const struct mb_wsd_request *r){
    return mb_wsd_is_device_probe(r)||mb_wsd_is_print_probe(r)||mb_wsd_is_scan_probe(r);
}

static int add(char *out,size_t cap,size_t *p,const char *s){
    size_t n=strlen(s);
    if(*p>cap||n>cap-*p)return -1;
    memcpy(out+*p,s,n);*p+=n;
    return 0;
}
static int addf(char *out,size_t cap,size_t *p,const char *fmt,...){
    va_list ap;int n;
    if(*p>=cap)return -1;
    va_start(ap,fmt);n=vsnprintf(out+*p,cap-*p,fmt,ap);va_end(ap);
    if(n<0||(size_t)n>=cap-*p)return -1;
    *p+=(size_t)n;return 0;
}
static int xml_text(char *out,size_t cap,size_t *p,const char *s){
    const unsigned char *q=(const unsigned char *)s;
    if(!s)return -1;
    for(;*q;q++){
        const char *esc=0;char ch=(char)*q;
        if(*q=='&')esc="&amp;";
        else if(*q=='<')esc="&lt;";
        else if(*q=='>')esc="&gt;";
        else if(*q=='\"')esc="&quot;";
        else if(*q=='\'')esc="&apos;";
        if(esc){if(add(out,cap,p,esc))return -1;}
        else {if(*p>=cap)return -1;out[(*p)++]=ch;}
    }
    return 0;
}
static int uriish(const char *s){
    const unsigned char *p=(const unsigned char *)s;
    if(!s||!*s)return 0;
    for(;*p;p++)if(*p<0x20||*p==0x7f)return 0;
    return 1;
}
static int envelope_start(char *out,size_t cap,size_t *p){
    return add(out,cap,p,
      "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
      "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
      " xmlns:a=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\""
      " xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\""
      " xmlns:dp=\"http://schemas.xmlsoap.org/ws/2006/02/devprof\""
      " xmlns:p=\"http://schemas.microsoft.com/windows/2006/08/wdp/print\""
      " xmlns:scn=\"http://schemas.microsoft.com/windows/2006/08/wdp/scan\">");
}
static int header(char *out,size_t cap,size_t *p,const char *action,
                  const char *response_id,const char *relates,
                  unsigned long instance_id,unsigned long message_number){
    if(add(out,cap,p,"<s:Header><a:Action>")||
       xml_text(out,cap,p,action)||
       add(out,cap,p,"</a:Action><a:MessageID>")||
       xml_text(out,cap,p,response_id)||
       add(out,cap,p,"</a:MessageID><a:RelatesTo>")||
       xml_text(out,cap,p,relates)||
       add(out,cap,p,"</a:RelatesTo>"
                        "<a:To>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</a:To>")||
       addf(out,cap,p,"<d:AppSequence InstanceId=\"%lu\" MessageNumber=\"%lu\"/>",
            instance_id,message_number)||
       add(out,cap,p,"</s:Header>"))return -1;
    return 0;
}
static int match_body(char *out,size_t cap,size_t *p,const char *container,
                      const char *item,const char *endpoint,const char *xaddr){
    if(addf(out,cap,p,"<s:Body><d:%s><d:%s><a:EndpointReference><a:Address>",
            container,item)||
       xml_text(out,cap,p,endpoint)||
       add(out,cap,p,"</a:Address></a:EndpointReference>"
                     "<d:Types>dp:Device p:PrintDeviceType scn:ScanDeviceType</d:Types>"
                     "<d:XAddrs>")||
       xml_text(out,cap,p,xaddr)||
       add(out,cap,p,"</d:XAddrs><d:MetadataVersion>1</d:MetadataVersion>")||
       addf(out,cap,p,"</d:%s></d:%s></s:Body></s:Envelope>",item,container))
        return -1;
    return 0;
}
int mb_wsd_build_match(const struct mb_wsd_request *r,
                       const char *endpoint,const char *xaddr,
                       const char *response_message_id,
                       unsigned long instance_id,unsigned long message_number,
                       char *out,size_t cap){
    const char *action,*container,*item;size_t p=0;
    if(!r||!endpoint||!xaddr||!response_message_id||!out||cap<2)return -1;
    if(!uriish(r->message_id)||!uriish(endpoint)||!uriish(xaddr)||!uriish(response_message_id))return -1;
    if(r->kind==MB_WSD_PROBE){
        if(!mb_wsd_probe_supported(r))return 0;
        action="http://schemas.xmlsoap.org/ws/2005/04/discovery/ProbeMatches";
        container="ProbeMatches";item="ProbeMatch";
    } else if(r->kind==MB_WSD_RESOLVE){
        if(strcmp(r->endpoint,endpoint))return 0;
        action="http://schemas.xmlsoap.org/ws/2005/04/discovery/ResolveMatches";
        container="ResolveMatches";item="ResolveMatch";
    } else return 0;
    if(envelope_start(out,cap,&p)||
       header(out,cap,&p,action,response_message_id,r->message_id,instance_id,message_number)||
       match_body(out,cap,&p,container,item,endpoint,xaddr)||
       p>=cap)return -2;
    out[p]=0;
    return (int)p;
}
