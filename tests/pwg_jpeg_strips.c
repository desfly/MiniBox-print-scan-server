/* Standalone regression for PWG sGray8 -> sequential PCL XL JPEG strips.
 * cc -std=c11 -I src/minibox-raster tests/pwg_jpeg_strips.c \
 *    src/minibox-raster/pwg_to_pcl_v2.c -ljpeg -o /tmp/pwg-strips
 * /tmp/pwg-strips
 */
#include "pwg_to_pcl.h"
#include <stdint.h>
#include <stdio.h>
#include <jpeglib.h>
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
    const unsigned char row[]={0,128}; /* one repeated scanline, then end-of-row white fill */
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
    /* USBPcap M1522-PRINT-REAL(2).pcap: HP UPD uses LF (0a), not CRLF,
     * at the PJL -> PCL XL handoff. Lock down the binary session prefix. */
    {
        static const unsigned char binding[]=
            "@PJL ENTER LANGUAGE=PCLXL\n"
            ") HP-PCL XL;2;1;Comment Copyright(c) 1999 Microsoft Corporation\n";
        static const unsigned char session[]={
            0xd1,0x58,0x02,0x58,0x02,0xf8,0x89,
            0xc0,0x00,0xf8,0x86,0xc0,0x03,0xf8,0x8f,0x41,
            0xc0,0x00,0xf8,0x88,0xc0,0x01,0xf8,0x82,0x48
        };
        size_t j;
        for(j=0;j+sizeof(binding)-1+sizeof(session)<=b.n;j++)
            if(!memcmp(b.p+j,binding,sizeof(binding)-1)&&
               !memcmp(b.p+j+sizeof(binding)-1,session,sizeof(session)))break;
        if(j+sizeof(binding)-1+sizeof(session)>b.n){
            fprintf(stderr,"FAIL: HP capture PJL/PCL XL session prefix mismatch\n");
            goto done;
        }
        /* Invariant HP page preamble from M1522-PRINT-REAL USB capture. */
        {
            static const unsigned char page_prefix[]={
                0xc0,0x01,0xf8,0x26,0xc0,0x00,0xf8,0x34,
                0xc0,0x00,0xf8,0x28,0xc8,0xc0,0x02,'A','4',0xf8,0x25,0x43,
                0xd5,0x00,0x00,0x80,0x3f,0x00,0x00,0x80,0x3f,0xf8,0x2b,0x77,
                0xc0,0x01,0xf8,0x03,0x6a,0xc0,0x00,0xf8,0x2d,0x78,
                0xc0,0x00,0xf8,0x2d,0x7c,0xc0,0xcc,0xf8,0x2c,0x7b
            };
            size_t page=j+sizeof(binding)-1+sizeof(session);
            if(page+sizeof(page_prefix)>b.n ||
               memcmp(b.p+page,page_prefix,sizeof(page_prefix))){
                fprintf(stderr,"FAIL: HP USB capture page-state prefix mismatch\n");
                goto done;
            }
        }
    }
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
        /* Decode each independent JPEG strip; validate its dimensions and
         * pixels rather than trusting marker bytes or framing alone. */
        {
            struct jpeg_decompress_struct d;
            struct jpeg_error_mgr err;
            unsigned char decoded[32*3];
            int valid=1;
            memset(&d,0,sizeof d);
            d.err=jpeg_std_error(&err);
            jpeg_create_decompress(&d);
            jpeg_mem_src(&d,b.p+jpeg,length);
            if(jpeg_read_header(&d,TRUE)!=JPEG_HEADER_OK ||
               d.image_width!=32 || d.image_height!=lines) {
                jpeg_destroy_decompress(&d);
                goto done;
            }
            d.out_color_space=JCS_RGB;
            jpeg_start_decompress(&d);
            if(d.output_width!=32 || d.output_height!=lines ||
               d.output_components!=3) valid=0;
            while(valid && d.output_scanline<d.output_height) {
                JSAMPROW scan=decoded;
                if(jpeg_read_scanlines(&d,&scan,1)!=1){valid=0;break;}
                for(size_t k=0;k<sizeof decoded;k++)
                    if(decoded[k]!=255){valid=0;break;}
            }
            if(valid) jpeg_finish_decompress(&d);
            jpeg_destroy_decompress(&d);
            if(!valid)goto done;
        }
        covered+=lines;blocks++;pos=jpeg+length;
    }
    /* Every emitted JPEG block must be followed by exactly the expected
     * EndImage, EndPage, CloseDataSource, EndSession and PJL job trailer.
     * Without this, a green JPEG test could hide invalid trailing PCL XL. */
    {
        static const unsigned char trailer[]={
            0xb2,0x44,0x49,0x42,
            0x1b,'%','-','1','2','3','4','5','X',
            '@','P','J','L',' ','E','O','J',' ','N','A','M','E','=','"',
            'M','i','n','i','B','o','x',' ','P','W','G','"',0x0d,0x0a,
            0x1b,'%','-','1','2','3','4','5','X'
        };
        if(pos>b.n || b.n-pos!=sizeof(trailer) ||
           memcmp(b.p+pos,trailer,sizeof(trailer))){
            fprintf(stderr,"FAIL: invalid PCL XL/PJL job trailer\\n");
            goto done;
        }
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
