#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include "../minibox-ipp/ipp.h"
#include "http_body.h"
#ifndef MINIBOX_TEST_PRINT_SINK
#include "../minibox-usb/print_m1522.h"
#endif
#define HTTP_HEAD_CAP 16384
#define IPP_PREFIX_CAP 16384
#define IO_CHUNK 16384
#define MAX_PRINT_JOB (128u * 1024u * 1024u)
#define CLIENT_TIMEOUT_SEC 15
static volatile sig_atomic_t stop;
static void on_signal(int sig){(void)sig;stop=1;}
static int send_all(int fd,const void*vp,size_t n){const unsigned char*p=vp;while(n){ssize_t w=send(fd,p,n,0);if(w<0){if(errno==EINTR)continue;return-1;}p+=w;n-=(size_t)w;}return 0;}
static void reply_data(int fd,int code,const char*reason,const char*type,const void*body,size_t n){char h[512];int m=snprintf(h,sizeof h,"HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",code,reason,type,n);if(m>0){(void)send_all(fd,h,(size_t)m);if(n)(void)send_all(fd,body,n);}}
static void reply_text(int fd,int code,const char*reason,const char*body){reply_data(fd,code,reason,"text/plain",body,strlen(body));}
#ifdef MINIBOX_TEST_PRINT_SINK
typedef FILE print_session;
static int ps_open(print_session **s){const char*p=getenv("MINIBOX_TEST_PRINT_FILE");if(!p)return-90;*s=fopen(p,"wb");return *s?0:-91;}
static int ps_write(print_session*s,const unsigned char*b,size_t n){return fwrite(b,1,n,s)==n?0:-92;}
static void ps_close(print_session*s){if(s)(void)fclose(s);}
#else
typedef struct m1522_print_session print_session;
static int ps_open(print_session **s){static print_session x;*s=&x;return m1522_print_open(*s);}
static int ps_write(print_session*s,const unsigned char*b,size_t n){return m1522_print_write(s,b,n);}
static void ps_close(print_session*s){m1522_print_close(s);}
#endif
static void ipp_reply(int fd,const struct ipp_request*r,uint16_t status){unsigned char out[64];size_t n=ipp_build_status(out,sizeof out,r,status);reply_data(fd,200,"OK","application/ipp",out,n);}
static void ipp_attributes_reply(int fd,const struct ipp_request*r){unsigned char out[2048];size_t n=ipp_build_printer_attributes(out,sizeof out,r,"ipp://minibox.local/ipp/print");if(!n){ipp_reply(fd,r,0x0500);return;}reply_data(fd,200,"OK","application/ipp",out,n);}
static int read_more(int fd,unsigned char*b,size_t*used,size_t cap,size_t total){ssize_t n;size_t want;if(*used>=cap||*used>=total)return-1;want=cap-*used;if(want>total-*used)want=total-*used;n=minibox_recv_retry(fd,b+*used,want);if(n<=0)return-1;*used+=(size_t)n;return 0;}
static void serve_post(int fd,unsigned char*body,size_t have,size_t total){struct ipp_request r;size_t off=0;int dr;print_session*ps=0;unsigned char io[IO_CHUNK];size_t consumed,remain;if(have<8){while(have<8&&have<total)if(read_more(fd,body,&have,IPP_PREFIX_CAP,total)){reply_text(fd,400,"Bad Request","truncated IPP body\n");return;}}if(ipp_parse_header(body,have,&r)){reply_text(fd,400,"Bad Request","invalid IPP header\n");return;}if(r.operation!=IPP_OP_PRINT_JOB){while(have<total){size_t want=total-have>sizeof io?sizeof io:total-have;ssize_t n=minibox_recv_retry(fd,io,want);if(n<=0){reply_text(fd,400,"Bad Request","truncated IPP body\n");return;}have+=(size_t)n;}if(r.operation==IPP_OP_GET_PRINTER_ATTRIBUTES){ipp_attributes_reply(fd,&r);return;}ipp_reply(fd,&r,r.operation==IPP_OP_VALIDATE_JOB?0:0x0501);return;}for(;;){dr=ipp_document_offset(body,have,&off);if(!dr)break;if(have>=total||have>=IPP_PREFIX_CAP){ipp_reply(fd,&r,0x0400);return;}if(read_more(fd,body,&have,IPP_PREFIX_CAP,total)){reply_text(fd,400,"Bad Request","truncated IPP attributes\n");return;}}if(off>=total){ipp_reply(fd,&r,0x0400);return;}if(ps_open(&ps)){ipp_reply(fd,&r,0x0507);return;}consumed=have;if(have>off&&ps_write(ps,body+off,have-off)){ps_close(ps);ipp_reply(fd,&r,0x0507);return;}remain=total-consumed;while(remain){size_t want=remain>sizeof io?sizeof io:remain;ssize_t n=minibox_recv_retry(fd,io,want);if(n<=0){ps_close(ps);reply_text(fd,400,"Bad Request","truncated print document\n");return;}if(ps_write(ps,io,(size_t)n)){ps_close(ps);ipp_reply(fd,&r,0x0507);return;}remain-=(size_t)n;}ps_close(ps);ipp_reply(fd,&r,0);}
static int headers_complete(const unsigned char*b,size_t n){size_t i;for(i=0;i+3<n;i++)if(b[i]=='\r'&&b[i+1]=='\n'&&b[i+2]=='\r'&&b[i+3]=='\n')return 1;return 0;}
static void serve(int fd){unsigned char head[HTTP_HEAD_CAP],prefix[IPP_PREFIX_CAP];size_t used=0,initial;struct minibox_http_body hb;int pr;char method[16],path[256];while(!headers_complete(head,used)&&used<sizeof head){ssize_t n=minibox_recv_retry(fd,head+used,sizeof head-used);if(n<=0)return;used+=(size_t)n;}if(!headers_complete(head,used)){reply_text(fd,431,"Request Header Fields Too Large","headers too large\n");return;}if(sscanf((const char*)head,"%15s %255s",method,path)!=2){reply_text(fd,400,"Bad Request","bad request\n");return;}if(!strcmp(method,"GET")&&!strcmp(path,"/health")){reply_text(fd,200,"OK","minibox-printerd ok\n");return;}if(strcmp(path,"/ipp/print")){reply_text(fd,404,"Not Found","not found\n");return;}if(strcmp(method,"POST")){reply_text(fd,405,"Method Not Allowed","POST required\n");return;}pr=minibox_http_parse_body(head,used,&hb);if(pr){reply_text(fd,411,"Length Required","valid Content-Length required\n");return;}if(hb.content_length>MAX_PRINT_JOB){reply_text(fd,413,"Payload Too Large","print job too large\n");return;}initial=hb.buffered_body;if(initial>sizeof prefix)initial=sizeof prefix;if(initial)memcpy(prefix,head+hb.header_bytes,initial);if(hb.content_length<8){reply_text(fd,400,"Bad Request","IPP header too short\n");return;}serve_post(fd,prefix,initial,hb.content_length);}
int main(int argc,char**argv){int port=argc>1?atoi(argv[1]):631;int s=socket(AF_INET,SOCK_STREAM,0);if(s<0){perror("socket");return 1;}int one=1;setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&one,sizeof one);struct sockaddr_in a;memset(&a,0,sizeof a);a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_ANY);a.sin_port=htons((unsigned short)port);if(bind(s,(struct sockaddr*)&a,sizeof a)||listen(s,8)){perror("bind/listen");close(s);return 1;}signal(SIGINT,on_signal);signal(SIGTERM,on_signal);fprintf(stderr,"minibox-printerd: listening on %d\n",port);while(!stop){int c=accept(s,NULL,NULL);struct timeval tv={CLIENT_TIMEOUT_SEC,0};if(c<0){if(errno==EINTR)continue;perror("accept");break;}(void)setsockopt(c,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof tv);(void)setsockopt(c,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof tv);serve(c);close(c);}close(s);return 0;}
