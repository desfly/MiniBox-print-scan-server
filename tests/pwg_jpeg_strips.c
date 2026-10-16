/* Standalone regression for PWG sGray8 -> sequential PCL XL JPEG strips.
 * cc -std=c11 -I src/minibox-raster tests/pwg_jpeg_strips.c \
 *    src/minibox-raster/pwg_to_pcl_v2.c -ljpeg -o /tmp/pwg-strips
 * /tmp/pwg-strips
 */
#include "pwg_to_pcl.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct buffer { unsigned char *p; size_t n, cap; };
static int write_bytes(void *arg, const unsigned char *s, size_t n) {
    struct buffer *b=arg;
    if(n>SIZE_MAX-b->n) return -1;
    if(n+b->n>b->cap) {
        size_t cap=b->cap?b->cap:4096;
        while(cap<n+b->n) { if(cap>SIZE_MAX/2)return -1;cap*=2; }
        unsigned char *p=realloc(b->p,cap);
        if(!p)return -1;
        b->p=p;b->cap=cap;
    }
    memcpy(b->p+b->n,s,n);b->n+=n;return 0;
}
static void put32(unsigned char *p,unsigned v) {
    p[0]=(unsigned char)(v>>24);p[1]=(unsigned char)(v>>16);
    p[2]=(unsigned char)(v>>8);p[3]=(unsigned char)v;
}
static unsigned get16(const unsigned char *p) {return (unsigned)p[0]|((unsigned)p[1]<<8);}
static uint32_t get32(const unsigned char *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static int check(unsigned height) {
    unsigned char h[1796]={0};
    const unsigned char magic[]={'R','a','S','2'};
    const unsigned char row[]={128}; /* PWG end-of-row fill: white for sGray8 */
    struct mb_pwg_pcl s;
    struct buffer b={0};
    unsigned covered=0,blocks=0;
    size_t pos=0;
    int ok=0;
    mb_pwg_pcl_init(&s);
    put32(h+276,600);put32(h+280,600);
    put32(h+352,595);put32(h+356,842);
    put32(h+372,32);put32(h+376,height);
    put32(h+384,8);put32(h+388,8);put32(h+392,32);
    put32(h+400,18);put32(h+420,1);
    if(mb_pwg_pcl_feed(&s,magic,sizeof magic,write_bytes,&b) ||
       mb_pwg_pcl_feed(&s,h,sizeof h,write_bytes,&b)) goto done;
    for(unsigned y=0;y<height;y++)
        if(mb_pwg_pcl_feed(&s,row,sizeof row,write_bytes,&b))goto done;
    if(mb_pwg_pcl_finish(&s,write_bytes,&b))goto done;
    while(covered<height) {
        size_t i;
        /* c1,start16,f8,6d,c1,height16,f8,63,c0,02,f8,65,b1,fa,len32 */
        for(i=pos;i+20<=b.n;i++) {
            const unsigned char *p=b.p+i;
            if(p[0]==0xc1 && p[3]==0xf8 && p[4]==0x6d &&
               p[5]==0xc1 && p[8]==0xf8 && p[9]==0x63 &&
               p[10]==0xc0 && p[11]==2 && p[12]==0xf8 &&
               p[13]==0x65 && p[14]==0xb1 && p[15]==0xfa) break;
        }
        if(i+20>b.n)goto done;
        const unsigned char *p=b.p+i;
        unsigned start=get16(p+1),lines=get16(p+6);
        uint32_t length=get32(p+16);
        size_t jpeg=i+20;
        if(start!=covered || !lines || lines>128 ||
           lines>height-covered || length<4 || length>b.n-jpeg)goto done;
        if(b.p[jpeg]!=0xff || b.p[jpeg+1]!=0xd8 ||
           b.p[jpeg+length-2]!=0xff || b.p[jpeg+length-1]!=0xd9)goto done;
        covered+=lines;blocks++;pos=jpeg+length;
    }
    ok=(blocks==(height+127)/128);
done:
    if(!ok)fprintf(stderr,"FAIL height=%u blocks=%u covered=%u bytes=%zu\n",
                   height,blocks,covered,b.n);
    mb_pwg_pcl_reset(&s);free(b.p);
    return ok?0:1;
}
int main(void) {
    const unsigned heights[]={1,127,128,129,255,256,257};
    for(size_t i=0;i<sizeof heights/sizeof heights[0];i++)
        if(check(heights[i]))return 1;
    puts("PASS: JPEG ReadImage strips cover every row without gaps");
    return 0;
}
