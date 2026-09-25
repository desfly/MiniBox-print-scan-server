#define _POSIX_C_SOURCE 200809L
#include "wsd.h"
#include "wsd_identity.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define BODY_MAX 8192
#define RESPONSE_MAX 8192

static void error_response(const char *status,const char *text){
    size_t n=strlen(text);
    printf("Status: %s\r\nContent-Type: text/plain; charset=utf-8\r\n"
           "Content-Length: %zu\r\nCache-Control: no-store\r\n\r\n%s",
           status,n,text);
}
static int read_body(char *buf,size_t cap,size_t *len){
    const char *s=getenv("CONTENT_LENGTH");char *end=0;unsigned long want;size_t got;
    if(!buf||cap<2||!len)return -1;
    if(!s||!*s)return -1;
    errno=0;want=strtoul(s,&end,10);
    if(errno||!end||*end||want==0||want>=cap)return -1;
    got=fread(buf,1,(size_t)want,stdin);
    if(got!=(size_t)want)return -1;
    buf[got]=0;*len=got;return 0;
}
static void response_id(char *out,size_t cap,const char *serial){
    unsigned long t=(unsigned long)time(NULL);
    unsigned long tail=((t&0xffffffffUL)<<16)^((unsigned long)getpid()&0xffffUL);
    (void)snprintf(out,cap,"urn:uuid:4d424d45-%04lx-4000-8000-%012lx",
                   (t^(unsigned long)getpid())&0xffffUL,tail&0xffffffffffffUL);
    (void)serial;
}
int main(void){
    char body[BODY_MAX],out[RESPONSE_MAX],request_id[192],reply_id[96];
    size_t body_len;int n;struct mb_wsd_identity id;const char *method=getenv("REQUEST_METHOD");
    if(!method||strcmp(method,"POST")){
        error_response("405 Method Not Allowed","WSD metadata requires HTTP POST\n");return 0;
    }
    if(read_body(body,sizeof body,&body_len)){
        error_response("400 Bad Request","Invalid WSD metadata request\n");return 0;
    }
    if(mb_wsd_extract_message_id(body,body_len,request_id,sizeof request_id)){
        error_response("400 Bad Request","Missing WSD MessageID\n");return 0;
    }
    if(mb_wsd_get_identity(&id)){
        error_response("503 Service Unavailable","MiniBox network identity unavailable\n");return 0;
    }
    response_id(reply_id,sizeof reply_id,id.serial);
    n=mb_wsd_build_metadata_response(request_id,id.endpoint,id.xaddr,reply_id,
                                     id.serial,id.presentation,out,sizeof out);
    if(n<=0){
        error_response("500 Internal Server Error","WSD metadata response failed\n");return 0;
    }
    printf("Status: 200 OK\r\nContent-Type: application/soap+xml; charset=utf-8\r\n"
           "Content-Length: %d\r\nCache-Control: no-store\r\n\r\n",n);
    fwrite(out,1,(size_t)n,stdout);
    return 0;
}
