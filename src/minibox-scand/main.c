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
#include "../minibox-escl/escl.h"
#include "../minibox-printerd/http_body.h"
#include "scan_session.h"
#include "scan_backend.h"
#ifndef MINIBOX_TEST_SCAN_BACKEND
#include "m1522_backend.h"
#endif
static volatile sig_atomic_t stop; static void on_signal(int s){(void)s;stop=1;}
static unsigned next_job=1; static struct minibox_scan_session scan;
#define SCAN_REQUEST_MAX 65536
#define CLIENT_TIMEOUT_SEC 15
static int send_all(int f,const void*p,size_t n){const unsigned char*q=p;while(n){ssize_t w=send(f,q,n,0);if(w<0){if(errno==EINTR)continue;return-1;}q+=w;n-=(size_t)w;}return 0;}
static void outx(int f,int code,const char*reason,const char*type,const char*extra,const char*body){char h[768];size_t n=strlen(body);int m=snprintf(h,sizeof h,"HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n%sConnection: close\r\n\r\n",code,reason,type,n,extra?extra:"");if(m>0){send_all(f,h,(size_t)m);send_all(f,body,n);}}
static const char *reason_for(int code){switch(code){case 200:return "OK";case 201:return "Created";case 400:return "Bad Request";case 404:return "Not Found";case 409:return "Conflict";case 503:return "Service Unavailable";default:return "Error";}}
static void out(int f,int code,const char*type,const char*body){outx(f,code,reason_for(code),type,NULL,body);}
static int headers_complete(const char*b,size_t n){size_t i;for(i=0;i+3<n;i++)if(b[i]=='\r'&&b[i+1]=='\n'&&b[i+2]=='\r'&&b[i+3]=='\n')return 1;return 0;}
static int receive_request(int f,char*b,size_t cap,size_t*used){struct minibox_http_body hb;size_t need;*used=0;while(!headers_complete(b,*used)){ssize_t n;if(*used>=cap-1)return-2;n=minibox_recv_retry(f,b+*used,cap-1-*used);if(n<=0)return-1;*used+=(size_t)n;}b[*used]=0;if(strncmp(b,"POST ",5))return 0;if(minibox_http_parse_body((const unsigned char*)b,*used,&hb))return-3;if(hb.header_bytes>=cap||hb.content_length>=cap-hb.header_bytes)return-4;need=hb.header_bytes+hb.content_length;while(*used<need){ssize_t n=minibox_recv_retry(f,b+*used,need-*used);if(n<=0)return-1;*used+=(size_t)n;}b[*used]=0;return 0;}
static const char*body_of(char*b,size_t n,size_t*len){size_t i;for(i=0;i+3<n;i++)if(b[i]=='\r'&&b[i+1]=='\n'&&b[i+2]=='\r'&&b[i+3]=='\n'){*len=n-i-4;return b+i+4;}*len=0;return NULL;}
static int next_document_id(const char *p,unsigned *id){char tail;return sscanf(p,"/eSCL/ScanJobs/%u/NextDocument%c",id,&tail)==1?0:-1;}
#ifdef MINIBOX_TEST_SCAN_BACKEND
struct test_scan_ctx{size_t off;}; static struct test_scan_ctx test_ctx; static const unsigned char test_jpeg[]={0xff,0xd8,'M','I','N','I','B','O','X',0xff,0xd9};
static int tb_open(void*v,const struct escl_job*j){struct test_scan_ctx*c=v;(void)j;c->off=0;return 0;} static int tb_read(void*v,unsigned char*b,size_t cap,size_t*got){struct test_scan_ctx*c=v;if(getenv("MINIBOX_TEST_SCAN_FAIL_FIRST") && c->off==0){*got=0;return -1;}size_t left=sizeof(test_jpeg)-c->off,n=left<cap?left:cap;if(n)memcpy(b,test_jpeg+c->off,n);c->off+=n;*got=n;return 0;} static int tb_end(void*v,int*more){(void)v;*more=0;return 0;} static void tb_close(void*v){(void)v;} static const struct minibox_scan_backend scan_backend_storage={tb_open,tb_read,tb_end,tb_close}; static const struct minibox_scan_backend *scan_backend=&scan_backend_storage; static void*scan_backend_ctx=&test_ctx;
#else
static const struct minibox_scan_backend *scan_backend=&minibox_m1522_scan_backend; static void *scan_backend_ctx;
#endif
static int stream_document(int f){
 struct minibox_scan_stream s={0}; unsigned char buf[16384]; size_t got; int more=0,started=0;
 const char*h="HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nConnection: close\r\n\r\n";
#ifndef MINIBOX_TEST_SCAN_BACKEND
 scan_backend_ctx=minibox_m1522_scan_backend_ctx;
#endif
 if(!scan_backend||minibox_scan_stream_open(&s,scan_backend,scan_backend_ctx,&scan.settings)) return -1;
 /* Do not commit HTTP 200 before the backend yields image data. */
 if(minibox_scan_stream_read(&s,buf,sizeof buf,&got) || !got) goto fail;
 if(send_all(f,h,strlen(h))) goto fail;
 started=1;
 if(send_all(f,buf,got)) goto fail;
 for(;;){
  if(minibox_scan_stream_read(&s,buf,sizeof buf,&got)) goto fail;
  if(!got) break;
  if(send_all(f,buf,got)) goto fail;
 }
 if(minibox_scan_stream_end_page(&s,&more)) goto fail;
 minibox_scan_stream_close(&s);
 return minibox_scan_session_end_page(&scan,more);
fail:
 minibox_scan_stream_close(&s);
 minibox_scan_session_fail(&scan);
 return started?-2:-1;
}
static void serve(int f){char b[SCAN_REQUEST_MAX+1],m[16],p[256];size_t n=0;int rr=receive_request(f,b,SCAN_REQUEST_MAX,&n);if(rr){out(f,rr==-4?413:400,"text/plain",rr==-4?"scan request too large\n":"invalid or incomplete request\n");return;}if(sscanf(b,"%15s %255s",m,p)!=2){out(f,400,"text/plain","bad request\n");return;}if(!strcmp(m,"GET")&&!strcmp(p,"/health")){out(f,200,"text/plain","minibox-scand ok\n");return;}if(!strcmp(m,"GET")&&!strcmp(p,"/eSCL/ScannerCapabilities")){out(f,200,"text/xml","<?xml version=\"1.0\"?><scan:ScannerCapabilities xmlns:scan=\"http://schemas.hp.com/imaging/escl/2011/05/03\"><scan:MakeAndModel>HP LaserJet M1522n @ MiniBox</scan:MakeAndModel><scan:Platen/><scan:Adf/></scan:ScannerCapabilities>\n");return;}if(!strcmp(m,"GET")&&!strcmp(p,"/eSCL/ScannerStatus")){out(f,200,"text/xml",minibox_scan_session_busy(&scan)?"<?xml version=\"1.0\"?><scan:ScannerStatus xmlns:scan=\"http://schemas.hp.com/imaging/escl/2011/05/03\"><scan:State>Processing</scan:State></scan:ScannerStatus>\n":"<?xml version=\"1.0\"?><scan:ScannerStatus xmlns:scan=\"http://schemas.hp.com/imaging/escl/2011/05/03\"><scan:State>Idle</scan:State></scan:ScannerStatus>\n");return;}if(!strcmp(m,"POST")&&!strcmp(p,"/eSCL/ScanJobs")){size_t z=0;const char*x=body_of(b,n,&z);char extra[256];struct escl_job settings;unsigned id;if(minibox_scan_session_busy(&scan)){out(f,409,"text/plain","scan job already active\n");return;}if(!x||escl_parse_scan_settings(x,z,&settings)){out(f,400,"text/plain","invalid scan settings\n");return;}id=next_job++;if(!id)id=next_job++;if(minibox_scan_session_create(&scan,id,&settings)){out(f,503,"text/plain","cannot create scan job\n");return;}snprintf(extra,sizeof extra,"Location: /eSCL/ScanJobs/%u\r\n",id);outx(f,201,"Created","text/plain",extra,"");return;}if(!strcmp(m,"GET")){unsigned id;if(next_document_id(p,&id)==0){int sr;if(minibox_scan_session_begin_page(&scan,id)){out(f,404,"text/plain","no matching scan job\n");return;}sr=stream_document(f);if(sr==-1)out(f,503,"text/plain","M1522 scan backend unavailable\n");return;}}out(f,404,"text/plain","not found\n");}
int main(int argc,char**argv){int port=argc>1?atoi(argv[1]):8080,s=socket(AF_INET,SOCK_STREAM,0),one=1;if(s<0){perror("socket");return 1;}setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&one,sizeof one);struct sockaddr_in a;memset(&a,0,sizeof a);a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_ANY);a.sin_port=htons((unsigned short)port);if(bind(s,(struct sockaddr*)&a,sizeof a)||listen(s,8)){perror("bind/listen");close(s);return 1;}signal(SIGINT,on_signal);signal(SIGTERM,on_signal);fprintf(stderr,"minibox-scand: listening on %d\n",port);while(!stop){int c=accept(s,NULL,NULL);struct timeval tv={CLIENT_TIMEOUT_SEC,0};if(c<0){if(errno==EINTR)continue;break;}(void)setsockopt(c,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof tv);(void)setsockopt(c,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof tv);serve(c);close(c);}close(s);return 0;}
