#include "../src/minibox-raster/pwg_to_pcl.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct sink { unsigned char data[65536]; size_t used; };
static int wr(void *ctx,const unsigned char *p,size_t n){
    struct sink *s=(struct sink *)ctx;
    if(n>sizeof s->data-s->used)return -1;
    memcpy(s->data+s->used,p,n);s->used+=n;return 0;
}
static void u32(unsigned char *p,uint32_t v){
    p[0]=(unsigned char)(v>>24);p[1]=(unsigned char)(v>>16);
    p[2]=(unsigned char)(v>>8);p[3]=(unsigned char)v;
}
static void header(unsigned char h[1796],unsigned w,unsigned ht,
                   unsigned bpc,unsigned bpp,unsigned bpl,unsigned cs,unsigned nc){
    memset(h,0,1796);memcpy(h,"PwgRaster",9);
    u32(h+276,300);u32(h+280,300);
    u32(h+352,595);u32(h+356,842);
    u32(h+372,w);u32(h+376,ht);u32(h+384,bpc);u32(h+388,bpp);
    u32(h+392,bpl);u32(h+396,0);u32(h+400,cs);u32(h+420,nc);
}
static size_t find_bytes(const struct sink *s,const unsigned char *needle,size_t n){
    size_t i;if(!n||n>s->used)return (size_t)-1;
    for(i=0;i+n<=s->used;i++)if(!memcmp(s->data+i,needle,n))return i;
    return (size_t)-1;
}
static int has(const struct sink *s,const unsigned char *needle,size_t n){
    return find_bytes(s,needle,n)!=(size_t)-1;
}
static int has_buf(const unsigned char *buf,size_t len,const unsigned char *needle,size_t n){
    size_t i;if(!n||n>len)return 0;
    for(i=0;i+n<=len;i++)if(!memcmp(buf+i,needle,n))return 1;
    return 0;
}
static void feed_chunks(struct mb_pwg_pcl *p,const unsigned char *data,size_t n,
                        size_t step,struct sink *s){
    size_t off=0;
    while(off<n){
        size_t z=n-off<step?n-off:step;
        assert(mb_pwg_pcl_feed(p,data+off,z,wr,s)==0);
        off+=z;
    }
}
static void assert_jpeg_after_readimage(const struct sink *s,unsigned block_height,int rgb){
    size_t i;
    unsigned char cmd[16]={
        0xc1,0x00,0x00,0xf8,0x6d,
        0xc1,(unsigned char)block_height,(unsigned char)(block_height>>8),0xf8,0x63,
        0xc0,0x02,0xf8,0x65,0xb1,0xfa
    };
    i=find_bytes(s,cmd,sizeof cmd);assert(i!=(size_t)-1);
    i+=sizeof cmd;
    assert(i+4<s->used);
    {
        uint32_t n=(uint32_t)s->data[i]|((uint32_t)s->data[i+1]<<8)|
                   ((uint32_t)s->data[i+2]<<16)|((uint32_t)s->data[i+3]<<24);
        i+=4;assert(n>100&&i+n<=s->used);
        assert(s->data[i]==0xff&&s->data[i+1]==0xd8);
        assert(s->data[i+2]==0xff&&s->data[i+3]==0xe0);
        assert(!memcmp(s->data+i+6,"JFIF",4));
        /* HP UPD capture uses the standard libjpeg quality-95 luma table. */
        {
            static const unsigned char q95[]={
                0xff,0xdb,0x00,0x43,0x00,
                0x02,0x01,0x01,0x01,0x01,0x01,0x02,0x01,0x01,0x01,0x02,0x02,0x02,0x02,0x02,0x04
            };
            assert(has_buf(s->data+i,n,q95,sizeof q95));
        }
        if(rgb){
            /* Baseline SOF0, 3 components, 1x1 sampling for Y/Cb/Cr: exact UPD mode. */
            static const unsigned char sof_components[]={
                0x03,0x01,0x11,0x00,0x02,0x11,0x01,0x03,0x11,0x01
            };
            assert(has_buf(s->data+i,n,sof_components,sizeof sof_components));
        }
        assert(s->data[i+n-2]==0xff&&s->data[i+n-1]==0xd9);
    }
}
int main(void){
    unsigned char h[1796],doc[4+1796+64];size_t n;
    struct mb_pwg_pcl p;struct sink s={0};

    header(h,8,2,8,8,8,18,1);
    memcpy(doc,"RaS2",4);memcpy(doc+4,h,1796);n=1800;
    doc[n++]=1;      /* one encoded line repeated twice */
    doc[n++]=249;    /* 8 literal grayscale values */
    doc[n++]=0;doc[n++]=32;doc[n++]=64;doc[n++]=96;
    doc[n++]=128;doc[n++]=160;doc[n++]=192;doc[n++]=255;
    mb_pwg_pcl_init(&p);feed_chunks(&p,doc,n,7,&s);
    assert(mb_pwg_pcl_finish(&p,wr,&s)==0);
    assert(has(&s,(const unsigned char *)"@PJL SET RESOLUTION=600",23));
    assert(has(&s,(const unsigned char *)"@PJL SET BITSPERPIXEL=1",26));
    assert(has(&s,(const unsigned char *)"@PJL ENTER LANGUAGE=PCLXL",25));
    assert(has(&s,(const unsigned char *)") HP-PCL XL;2;1;",16));
    {
        static const unsigned char session_setup[]={
            0xd1,0x58,0x02,0x58,0x02,0xf8,0x89, /* UnitsPerMeasure=600,600 */
            0xc0,0x00,0xf8,0x86,                 /* Measure=eInch */
            0xc0,0x03,0xf8,0x8f,                 /* ErrorReport=eBackChAndErrPage */
            0x41,
            0xc0,0x00,0xf8,0x88,
            0xc0,0x01,0xf8,0x82,
            0x48
        };
        assert(has(&s,session_setup,sizeof session_setup));
    }
    {
        static const unsigned char a4[]={
            0xc0,0x01,0xf8,0x26,
            0xc0,0x00,0xf8,0x34,
            0xc0,0x00,0xf8,0x28,
            0xc8,0xc0,0x02,'A','4',0xf8,0x25,
            0x43
        };
        assert(has(&s,a4,sizeof a4));
    }
    {
        static const unsigned char gray_image[]={
            0xc0,0x01,0xf8,0x03,0x6a,           /* eGray */
            0xc0,0x00,0xf8,0x64,                /* eDirectPixel */
            0xc0,0x02,0xf8,0x62,                /* e8Bit */
            0xc1,0x08,0x00,0xf8,0x6c,
            0xc1,0x02,0x00,0xf8,0x6b,
            0xd1,0x10,0x00,0x04,0x00,0xf8,0x67, /* 300dpi -> 600-unit destination */
            0xb0
        };
        assert(has(&s,gray_image,sizeof gray_image));
    }
    assert_jpeg_after_readimage(&s,2,0);
    mb_pwg_pcl_reset(&p);

    memset(&s,0,sizeof s);header(h,8,2,8,24,24,19,3);
    memcpy(doc,"RaS2",4);memcpy(doc+4,h,1796);n=1800;
    doc[n++]=1; /* one RGB row repeated twice */
    doc[n++]=3;doc[n++]=255;doc[n++]=255;doc[n++]=255;
    doc[n++]=3;doc[n++]=0;doc[n++]=0;doc[n++]=0;
    mb_pwg_pcl_init(&p);feed_chunks(&p,doc,n,1,&s);
    assert(mb_pwg_pcl_finish(&p,wr,&s)==0);
    {
        static const unsigned char rgb_image[]={
            0xc0,0x02,0xf8,0x03,0x6a, /* ColorSpace=eRGB */
            0xc0,0x00,0xf8,0x64,
            0xc0,0x02,0xf8,0x62
        };
        assert(has(&s,rgb_image,sizeof rgb_image));
    }
    assert_jpeg_after_readimage(&s,2,1);
    mb_pwg_pcl_reset(&p);

    /* True black_1 remains the documented packed 1-bit path. */
    memset(&s,0,sizeof s);header(h,2480,1,1,1,310,3,1);
    memcpy(doc,"RaS2",4);memcpy(doc+4,h,1796);n=1800;
    doc[n++]=0;
    doc[n++]=127;doc[n++]=0xa5;
    doc[n++]=127;doc[n++]=0xa5;
    doc[n++]=53;doc[n++]=0xa5;
    mb_pwg_pcl_init(&p);feed_chunks(&p,doc,n,11,&s);
    assert(mb_pwg_pcl_finish(&p,wr,&s)==0);
    {
        static const unsigned char m1522_len[]={0xb1,0xfa,0x38,0x01,0x00,0x00};
        size_t i=find_bytes(&s,m1522_len,sizeof m1522_len);
        assert(i!=(size_t)-1);
        i+=sizeof m1522_len;
        assert(i+312<=s.used);
        assert(s.data[i+309]==0x5a);
        assert(s.data[i+310]==0x00);
        assert(s.data[i+311]==0x00);
    }
    mb_pwg_pcl_reset(&p);

    memset(&s,0,sizeof s);header(h,8,1,8,8,8,18,1);
    memcpy(doc,"RaS2",4);memcpy(doc+4,h,1796);n=1800;
    doc[n++]=0;doc[n++]=128;
    mb_pwg_pcl_init(&p);
    assert(mb_pwg_pcl_feed(&p,doc,n,wr,&s)<0);
    mb_pwg_pcl_reset(&p);

    puts("PWG Raster -> capture-derived M1522 PCL XL contract OK");
    return 0;
}
