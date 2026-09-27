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
#define PRINTER_URI "ipp://minibox.local/ipp/print"
static volatile sig_atomic_t stop;
static unsigned next_job_id=1,current_job_id=0,current_job_state=9;
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
static void ipp_reply(int fd,const struct ipp_request*r,uint16_t status){unsigned char out[256];size_t n=ipp_build_status(out,sizeof out,r,status);reply_data(fd,200,"OK","application/ipp",out,n);}
static void ipp_attributes_reply(int fd,const struct ipp_request*r){unsigned char out[4096];size_t n=ipp_build_printer_attributes(out,sizeof out,r,PRINTER_URI);if(!n){ipp_reply(fd,r,0x0500);return;}reply_data(fd,200,"OK","application/ipp",out,n);}
static void ipp_job_reply(int fd,const struct ipp_request*r,uint16_t status,unsigned job_id,unsigned job_state){unsigned char out[1024];size_t n=ipp_build_job_attributes(out,sizeof out,r,status,PRINTER_URI,job_id,job_state);if(!n){ipp_reply(fd,r,0x0500);return;}reply_data(fd,200,"OK","application/ipp",out,n);}

struct body_reader{int fd,chunked,done;const unsigned char*initial;size_t initial_len,initial_pos,remain,chunk_left,total_out;};
static ssize_t raw_read(struct body_reader*b,void*out,size_t cap){size_t n;if(!cap)return 0;if(b->initial_pos<b->initial_len){n=b->initial_len-b->initial_pos;if(n>cap)n=cap;memcpy(out,b->initial+b->initial_pos,n);b->initial_pos+=n;return(ssize_t)n;}return minibox_recv_retry(b->fd,out,cap);}
static int raw_byte(struct body_reader*b,unsigned char*out){ssize_t n=raw_read(b,out,1);return n==1?0:-1;}
static int chunk_next(struct body_reader*b){char line[64],*end;size_t n=0;unsigned long v;unsigned char c,prev=0;for(;;){if(raw_byte(b,&c))return-1;if(n+1>=sizeof line)return-1;if(prev=='\r'&&c=='\n'){n--;line[n]=0;break;}line[n++]=(char)c;prev=c;}end=strchr(line,';');if(end)*end=0;errno=0;v=strtoul(line,&end,16);if(errno||end==line||*end||v>MAX_PRINT_JOB)return-1;if(v==0){for(;;){n=0;prev=0;for(;;){if(raw_byte(b,&c))return-1;if(n+1>=sizeof line)return-1;if(prev=='\r'&&c=='\n'){n--;break;}line[n++]=(char)c;prev=c;}if(n==0)break;}b->done=1;return 0;}b->chunk_left=(size_t)v;return 0;}
static ssize_t body_read(struct body_reader*b,void*out,size_t cap){unsigned char crlf[2];ssize_t n;if(!cap)return 0;if(!b->chunked){size_t want;if(!b->remain)return 0;want=b->remain<cap?b->remain:cap;n=raw_read(b,out,want);if(n<=0)return-1;b->remain-=(size_t)n;b->total_out+=(size_t)n;return n;}while(!b->done&&!b->chunk_left){if(chunk_next(b))return-1;if(b->done)return 0;}if(b->done)return 0;if(cap>b->chunk_left)cap=b->chunk_left;n=raw_read(b,out,cap);if(n<=0)return-1;b->chunk_left-=(size_t)n;b->total_out+=(size_t)n;if(b->total_out>MAX_PRINT_JOB)return-1;if(!b->chunk_left){if(raw_read(b,crlf,2)!=2||crlf[0]!='\r'||crlf[1]!='\n')return-1;}return n;}
static int drain_body(struct body_reader*b){unsigned char io[IO_CHUNK];ssize_t n;while((n=body_read(b,io,sizeof io))>0){}return n<0?-1:0;}
static int fill_prefix(struct body_reader*b,unsigned char*prefix,size_t*have,size_t need){while(*have<need&&*have<IPP_PREFIX_CAP){ssize_t n=body_read(b,prefix+*have,IPP_PREFIX_CAP-*have);if(n<=0)return-1;*have+=(size_t)n;}return *have>=need?0:-1;}
static int find_document(struct body_reader*b,unsigned char*prefix,size_t*have,size_t*off){int dr;for(;;){dr=ipp_document_offset(prefix,*have,off);if(!dr)return 0;if(*have>=IPP_PREFIX_CAP)return-1;{ssize_t n=body_read(b,prefix+*have,IPP_PREFIX_CAP-*have);if(n<=0)return-1;*have+=(size_t)n;}}}
static void serve_post(int fd,struct body_reader*b){struct ipp_request r;unsigned char prefix[IPP_PREFIX_CAP],io[IO_CHUNK];size_t have=0,off=0;print_session*ps=0;ssize_t n;if(fill_prefix(b,prefix,&have,8)){reply_text(fd,400,"Bad Request","truncated IPP body\n");return;}if(ipp_parse_header(prefix,have,&r)){reply_text(fd,400,"Bad Request","invalid IPP header\n");return;}
 if(r.operation==IPP_OP_PRINT_JOB||r.operation==IPP_OP_SEND_DOCUMENT){if(find_document(b,prefix,&have,&off)){reply_text(fd,400,"Bad Request","truncated IPP attributes\n");return;}if(ps_open(&ps)){ipp_reply(fd,&r,0x0507);return;}current_job_state=5;if(have>off&&ps_write(ps,prefix+off,have-off)){ps_close(ps);current_job_state=6;ipp_reply(fd,&r,0x0507);return;}while((n=body_read(b,io,sizeof io))>0){if(ps_write(ps,io,(size_t)n)){ps_close(ps);current_job_state=6;ipp_reply(fd,&r,0x0507);return;}}ps_close(ps);if(n<0){current_job_state=6;reply_text(fd,400,"Bad Request","truncated print document\n");return;}if(!current_job_id)current_job_id=next_job_id++;current_job_state=9;ipp_job_reply(fd,&r,0,current_job_id,current_job_state);return;}
 if(drain_body(b)){reply_text(fd,400,"Bad Request","truncated IPP body\n");return;}switch(r.operation){case IPP_OP_GET_PRINTER_ATTRIBUTES:ipp_attributes_reply(fd,&r);return;case IPP_OP_VALIDATE_JOB:ipp_reply(fd,&r,0);return;case IPP_OP_CREATE_JOB:current_job_id=next_job_id++;current_job_state=3;ipp_job_reply(fd,&r,0,current_job_id,current_job_state);return;case IPP_OP_GET_JOB_ATTRIBUTES:if(!current_job_id){ipp_reply(fd,&r,0x0406);return;}ipp_job_reply(fd,&r,0,current_job_id,current_job_state);return;case IPP_OP_CANCEL_JOB:if(!current_job_id){ipp_reply(fd,&r,0x0406);return;}current_job_state=7;ipp_job_reply(fd,&r,0,current_job_id,current_job_state);return;case IPP_OP_GET_JOBS:ipp_reply(fd,&r,0);return;default:ipp_reply(fd,&r,0x0501);return;}}
static int headers_complete(const unsigned char*b,size_t n){size_t i;for(i=0;i+3<n;i++)if(b[i]=='\r'&&b[i+1]=='\n'&&b[i+2]=='\r'&&b[i+3]=='\n')return 1;return 0;}
static void serve(int fd){unsigned char head[HTTP_HEAD_CAP];size_t used=0;struct minibox_http_body hb;struct body_reader br;int pr;char method[16],path[256];while(!headers_complete(head,used)&&used<sizeof head){ssize_t n=minibox_recv_retry(fd,head+used,sizeof head-used);if(n<=0)return;used+=(size_t)n;}if(!headers_complete(head,used)){reply_text(fd,431,"Request Header Fields Too Large","headers too large\n");return;}if(sscanf((const char*)head,"%15s %255s",method,path)!=2){reply_text(fd,400,"Bad Request","bad request\n");return;}if(!strcmp(method,"GET")&&!strcmp(path,"/health")){reply_text(fd,200,"OK","minibox-printerd ok\n");return;}if(strcmp(path,"/ipp/print")){reply_text(fd,404,"Not Found","not found\n");return;}if(strcmp(method,"POST")){reply_text(fd,405,"Method Not Allowed","POST required\n");return;}pr=minibox_http_parse_body(head,used,&hb);if(pr){reply_text(fd,411,"Length Required","Content-Length or chunked body required\n");return;}if(!hb.chunked&&hb.content_length>MAX_PRINT_JOB){reply_text(fd,413,"Payload Too Large","print job too large\n");return;}memset(&br,0,sizeof br);br.fd=fd;br.chunked=hb.chunked;br.initial=head+hb.header_bytes;br.initial_len=hb.buffered_body;br.remain=hb.content_length;serve_post(fd,&br);}
int main(int argc,char**argv){int port=argc>1?atoi(argv[1]):631;int s=socket(AF_INET,SOCK_STREAM,0);if(s<0){perror("socket");return 1;}int one=1;setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&one,sizeof one);struct sockaddr_in a;memset(&a,0,sizeof a);a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_ANY);a.sin_port=htons((unsigned short)port);if(bind(s,(struct sockaddr*)&a,sizeof a)||listen(s,8)){perror("bind/listen");close(s);return 1;}signal(SIGINT,on_signal);signal(SIGTERM,on_signal);fprintf(stderr,"minibox-printerd: listening on %d\n",port);while(!stop){int c=accept(s,NULL,NULL);struct timeval tv={CLIENT_TIMEOUT_SEC,0};if(c<0){if(errno==EINTR)continue;perror("accept");break;}(void)setsockopt(c,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof tv);(void)setsockopt(c,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof tv);serve(c);close(c);}close(s);return 0;}
