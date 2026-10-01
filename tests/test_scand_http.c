#define _POSIX_C_SOURCE 200809L
#define MINIBOX_TEST_SCAN_BACKEND 1
#define main minibox_scand_daemon_main
#include "../src/minibox-scand/main.c"
#undef main
#include <assert.h>
#include <sys/socket.h>

int minibox_m1522_present(void){ return 1; }
int mb_wsd_get_identity(struct mb_wsd_identity *id){ (void)id; return -1; }

static void request(const char *req,char *out,size_t cap){
 int sv[2]; ssize_t n; size_t used=0;
 assert(socketpair(AF_UNIX,SOCK_STREAM,0,sv)==0);
 assert(send(sv[0],req,strlen(req),0)==(ssize_t)strlen(req));
 shutdown(sv[0],SHUT_WR);
 serve(sv[1]); close(sv[1]);
 while((n=recv(sv[0],out+used,cap-1-used,0))>0) used+=(size_t)n;
 out[used]=0; close(sv[0]);
}
int main(void){
 char out[4096],req[4096],loc[128]; unsigned id; const char *body=
 "<scan:ScanSettings><scan:InputSource>Platen</scan:InputSource>"
 "<scan:XResolution>300</scan:XResolution><scan:YResolution>300</scan:YResolution>"
 "<scan:XOffset>0</scan:XOffset><scan:YOffset>0</scan:YOffset>"
 "<scan:Width>2550</scan:Width><scan:Height>3507</scan:Height>"
 "<scan:ColorMode>RGB24</scan:ColorMode></scan:ScanSettings>";
 minibox_scan_session_reset(&scan); next_job=1;
 snprintf(req,sizeof req,"POST /eSCL/ScanJobs HTTP/1.1\r\nHost: localhost\r\nContent-Length: %zu\r\n\r\n%s",strlen(body),body);
 request(req,out,sizeof out);
 assert(strstr(out,"HTTP/1.1 201 Created\r\n"));
 assert(sscanf(strstr(out,"Location:"),"Location: /eSCL/ScanJobs/%u",&id)==1);
 snprintf(loc,sizeof loc,"GET /eSCL/ScanJobs/%u/NextDocument HTTP/1.1\r\nHost: localhost\r\n\r\n",id);
 request(loc,out,sizeof out);
 assert(strstr(out,"HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\n"));
 { const char *p=strstr(out,"\r\n\r\n"); assert(p); p+=4; assert((unsigned char)p[0]==0xff&&(unsigned char)p[1]==0xd8); assert(!memcmp(p+2,"MINIBOX",7)); }
 minibox_scan_session_reset(&scan); next_job=1; test_pages_done=0; test_opens=0; test_closes=0; test_ends=0; setenv("MINIBOX_TEST_SCAN_MORE_PAGES","2",1);
 body="<scan:ScanSettings><scan:InputSource>Adf</scan:InputSource><scan:XResolution>300</scan:XResolution><scan:YResolution>300</scan:YResolution><scan:ColorMode>RGB24</scan:ColorMode></scan:ScanSettings>";
 snprintf(req,sizeof req,"POST /eSCL/ScanJobs HTTP/1.1\r\nHost: localhost\r\nContent-Length: %zu\r\n\r\n%s",strlen(body),body);
 request(req,out,sizeof out); assert(strstr(out,"HTTP/1.1 201 Created\r\n")); assert(sscanf(strstr(out,"Location:"),"Location: /eSCL/ScanJobs/%u",&id)==1);
 snprintf(loc,sizeof loc,"GET /eSCL/ScanJobs/%u/NextDocument HTTP/1.1\r\nHost: localhost\r\n\r\n",id); request(loc,out,sizeof out); assert(strstr(out,"HTTP/1.1 200 OK")); assert(scan.state==MINIBOX_SCAN_PAGE_DONE&&scan.page==1); assert(test_opens==1&&test_ends==1&&test_closes==0);
 request(loc,out,sizeof out); assert(strstr(out,"HTTP/1.1 200 OK")); assert(scan.state==MINIBOX_SCAN_DONE&&scan.page==2); assert(scan.id==id); assert(test_opens==1&&test_ends==2&&test_closes==1); unsetenv("MINIBOX_TEST_SCAN_MORE_PAGES");
 minibox_scan_session_reset(&scan); memset(&scan_stream,0,sizeof(scan_stream)); next_job=1; test_pages_done=0; test_opens=0; test_closes=0; test_ends=0; setenv("MINIBOX_TEST_SCAN_MORE_PAGES","2",1);
 snprintf(req,sizeof req,"POST /eSCL/ScanJobs HTTP/1.1\r\nHost: localhost\r\nContent-Length: %zu\r\n\r\n%s",strlen(body),body); request(req,out,sizeof out); assert(sscanf(strstr(out,"Location:"),"Location: /eSCL/ScanJobs/%u",&id)==1);
 snprintf(loc,sizeof loc,"GET /eSCL/ScanJobs/%u/NextDocument HTTP/1.1\r\nHost: localhost\r\n\r\n",id); request(loc,out,sizeof out); assert(scan.state==MINIBOX_SCAN_PAGE_DONE); assert(test_opens==1&&test_closes==0);
 setenv("MINIBOX_TEST_SCAN_READ_FAIL","1",1); request(loc,out,sizeof out); unsetenv("MINIBOX_TEST_SCAN_READ_FAIL"); unsetenv("MINIBOX_TEST_SCAN_MORE_PAGES"); assert(scan.state==MINIBOX_SCAN_FAILED); assert(test_opens==1&&test_closes==1);
 minibox_scan_session_reset(&scan); memset(&scan_stream,0,sizeof(scan_stream)); next_job=1; test_pages_done=0; test_opens=0; test_closes=0; test_ends=0;
 snprintf(req,sizeof req,"POST /eSCL/ScanJobs HTTP/1.1\r\nHost: localhost\r\nContent-Length: %zu\r\n\r\n%s",strlen(body),body); request(req,out,sizeof out); assert(sscanf(strstr(out,"Location:"),"Location: /eSCL/ScanJobs/%u",&id)==1);
 snprintf(loc,sizeof loc,"GET /eSCL/ScanJobs/%u/NextDocument HTTP/1.1\r\nHost: localhost\r\n\r\n",id); setenv("MINIBOX_TEST_SCAN_READ_FAIL","1",1); request(loc,out,sizeof out); unsetenv("MINIBOX_TEST_SCAN_READ_FAIL"); assert(strstr(out,"HTTP/1.1 503 Service Unavailable")); assert(!strstr(out,"HTTP/1.1 200 OK")); assert(scan.state==MINIBOX_SCAN_FAILED); assert(test_opens==1&&test_closes==1&&test_ends==0);
 minibox_scan_session_reset(&scan); memset(&scan_stream,0,sizeof(scan_stream)); next_job=1; test_pages_done=0; test_opens=0; test_closes=0; test_ends=0;
 setenv("MINIBOX_TEST_SCAN_LATE_READ_FAIL","1",1); snprintf(req,sizeof req,"POST /eSCL/ScanJobs HTTP/1.1\r\nHost: localhost\r\nContent-Length: %zu\r\n\r\n%s",strlen(body),body); request(req,out,sizeof out); assert(sscanf(strstr(out,"Location:"),"Location: /eSCL/ScanJobs/%u",&id)==1); snprintf(loc,sizeof loc,"GET /eSCL/ScanJobs/%u/NextDocument HTTP/1.1\r\nHost: localhost\r\n\r\n",id); request(loc,out,sizeof out); unsetenv("MINIBOX_TEST_SCAN_LATE_READ_FAIL"); assert(strstr(out,"HTTP/1.1 200 OK")); assert(!strstr(out,"HTTP/1.1 503 Service Unavailable")); assert(scan.state==MINIBOX_SCAN_FAILED); assert(test_opens==1&&test_closes==1&&test_ends==0);
 minibox_scan_session_reset(&scan); memset(&scan_stream,0,sizeof(scan_stream)); next_job=1; test_pages_done=0; test_opens=0; test_closes=0; test_ends=0;
 setenv("MINIBOX_TEST_SCAN_BAD_PREFIX","1",1); snprintf(req,sizeof req,"POST /eSCL/ScanJobs HTTP/1.1\r\nHost: localhost\r\nContent-Length: %zu\r\n\r\n%s",strlen(body),body); request(req,out,sizeof out); assert(sscanf(strstr(out,"Location:"),"Location: /eSCL/ScanJobs/%u",&id)==1); snprintf(loc,sizeof loc,"GET /eSCL/ScanJobs/%u/NextDocument HTTP/1.1\r\nHost: localhost\r\n\r\n",id); request(loc,out,sizeof out); unsetenv("MINIBOX_TEST_SCAN_BAD_PREFIX"); assert(strstr(out,"HTTP/1.1 503 Service Unavailable")); assert(!strstr(out,"HTTP/1.1 200 OK")); assert(scan.state==MINIBOX_SCAN_FAILED); assert(test_opens==1&&test_closes==1&&test_ends==0);
 minibox_scan_session_reset(&scan); memset(&scan_stream,0,sizeof(scan_stream)); next_job=1; test_pages_done=0; test_opens=0; test_closes=0; test_ends=0;
 setenv("MINIBOX_TEST_SCAN_OPEN_FAIL","1",1); snprintf(req,sizeof req,"POST /eSCL/ScanJobs HTTP/1.1\r\nHost: localhost\r\nContent-Length: %zu\r\n\r\n%s",strlen(body),body); request(req,out,sizeof out); assert(sscanf(strstr(out,"Location:"),"Location: /eSCL/ScanJobs/%u",&id)==1); snprintf(loc,sizeof loc,"GET /eSCL/ScanJobs/%u/NextDocument HTTP/1.1\r\nHost: localhost\r\n\r\n",id); request(loc,out,sizeof out); unsetenv("MINIBOX_TEST_SCAN_OPEN_FAIL"); assert(strstr(out,"HTTP/1.1 503 Service Unavailable")); assert(scan.state==MINIBOX_SCAN_FAILED); assert(test_opens==0&&test_closes==0&&test_ends==0); request(loc,out,sizeof out); assert(strstr(out,"HTTP/1.1 404 Not Found")); assert(scan.state==MINIBOX_SCAN_FAILED); assert(test_opens==0&&test_closes==0&&test_ends==0);
 puts("verified eSCL ScanJobs -> NextDocument -> JPEG HTTP path, ADF lifetime, and backend failures: OK");
 return 0;
}
