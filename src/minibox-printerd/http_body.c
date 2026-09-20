#include "http_body.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

static int lower_eq(const unsigned char *p,const char *s,size_t n){size_t i;for(i=0;i<n;i++){unsigned char a=p[i],b=(unsigned char)s[i];if(a>='A'&&a<='Z')a=(unsigned char)(a+32);if(b>='A'&&b<='Z')b=(unsigned char)(b+32);if(a!=b)return 0;}return s[n]==0;}
int minibox_http_parse_body(const unsigned char *b,size_t n,struct minibox_http_body *o){size_t i,hs=0,cl=(size_t)-1;int seen_cl=0;if(!b||!o)return -1;for(i=0;i+3<n;i++){if(b[i]=='\r'&&b[i+1]=='\n'&&b[i+2]=='\r'&&b[i+3]=='\n'){hs=i+4;break;}}if(!hs)return 1;i=0;while(i+1<hs){size_t e=i;while(e+1<hs&&(b[e]!='\r'||b[e+1]!='\n'))e++;if(e==i)break;if(e-i>=15&&lower_eq(b+i,"Content-Length:",15)){const unsigned char *p=b+i+15,*end=b+e;char tmp[32],*q;unsigned long long v;size_t z;while(p<end&&(*p==' '||*p=='\t'))p++;z=(size_t)(end-p);if(!z||z>=sizeof tmp)return -2;memcpy(tmp,p,z);tmp[z]=0;errno=0;v=strtoull(tmp,&q,10);if(errno||*q||v>(unsigned long long)SIZE_MAX)return -2;if(seen_cl&&cl!=(size_t)v)return -2;cl=(size_t)v;seen_cl=1;}i=e+2;}if(!seen_cl)return -3;o->header_bytes=hs;o->content_length=cl;o->buffered_body=n>hs?n-hs:0;if(o->buffered_body>cl)o->buffered_body=cl;return 0;}
ssize_t minibox_recv_retry(int fd,void *buf,size_t cap){ssize_t n;do{n=recv(fd,buf,cap,0);}while(n<0&&errno==EINTR);return n;}
