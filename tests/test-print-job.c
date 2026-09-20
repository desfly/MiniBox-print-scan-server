#include "../src/minibox-ipp/print_job.h"
#include <assert.h>
#include <string.h>
struct sink{unsigned char b[128];size_t n;};
static int write_sink(void*c,const unsigned char*b,int n,int t){struct sink*s=c;(void)t;memcpy(s->b+s->n,b,(size_t)n);s->n+=(size_t)n;return n;}
int main(void){static const unsigned char pcl[]={0x1b,'E','H','e','l','l','o',0x0c,0x1b,'E'};struct sink s={{0},0};assert(minibox_print_document(write_sink,&s,pcl,sizeof pcl,4,1000)==0);assert(s.n==sizeof pcl&&!memcmp(s.b,pcl,sizeof pcl));return 0;}
