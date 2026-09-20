#include "../src/minibox-scand/scan_backend.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

struct mock_ctx { size_t off; int opened; int closed; };
static const unsigned char jpeg[] = {0xff,0xd8,'M','B','O','X',0xff,0xd9};

static int mock_open(void *v,const struct escl_job *job){struct mock_ctx*c=v;if(!job||job->dpi!=300)return-1;c->opened=1;c->off=0;return 0;}
static int mock_read(void *v,unsigned char *buf,size_t cap,size_t *got){struct mock_ctx*c=v;size_t left=sizeof(jpeg)-c->off;size_t n=left<cap?left:cap;if(n)memcpy(buf,jpeg+c->off,n);c->off+=n;*got=n;return 0;}
static int mock_end(void *v,int *more){struct mock_ctx*c=v;if(c->off!=sizeof(jpeg))return-1;*more=0;return 0;}
static void mock_close(void *v){((struct mock_ctx*)v)->closed=1;}
static const struct minibox_scan_backend backend={mock_open,mock_read,mock_end,mock_close};

int main(void){
 struct minibox_scan_stream s={0}; struct mock_ctx c={0}; struct escl_job j={ESCL_SOURCE_PLATEN,300,1};
 unsigned char out[sizeof(jpeg)]={0},buf[3]; size_t total=0,got=0; int more=-1;
 assert(minibox_scan_stream_open(&s,&backend,&c,&j)==0); assert(c.opened);
 while(total<sizeof(out)){assert(minibox_scan_stream_read(&s,buf,sizeof(buf),&got)==0);assert(got>0);memcpy(out+total,buf,got);total+=got;}
 assert(memcmp(out,jpeg,sizeof(jpeg))==0); assert(minibox_scan_stream_end_page(&s,&more)==0); assert(more==0);
 minibox_scan_stream_close(&s); assert(c.closed); assert(!s.opened);
 puts("scan backend tests: OK"); return 0;
}
