#include "../src/minibox-usb/stream.h"
#include <assert.h>
#include <string.h>
struct sink{unsigned char b[64];size_t n;int calls;};
static int put(void*c,const unsigned char*b,int n,int t){struct sink*s=c;(void)t;memcpy(s->b+s->n,b,(size_t)n);s->n+=(size_t)n;s->calls++;return n;}
static int fail(void*c,const unsigned char*b,int n,int t){
    struct sink*s=c;(void)b;(void)n;(void)t;s->calls++;
    return -7; /* real libusb errors must remain negative */
}
int main(void){struct sink s={{0},0,0};const unsigned char x[]="0123456789abcdef";assert(minibox_stream_write(put,&s,x,16,5,1000)==0);assert(s.n==16&&s.calls==4&&!memcmp(s.b,x,16));s.calls=0;assert(minibox_stream_write(fail,&s,x,16,5,1000)!=0);assert(s.calls==1);return 0;}
