#include "../src/minibox-raster/pwg_to_pcl.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct sink { unsigned char data[32768]; size_t used; };
static int wr(void *ctx,const unsigned char *p,size_t n){
    struct sink *s=ctx;
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
static int has(const struct sink *s,const unsigned char *needle,size_t n){
    size_t i;if(!n||n>s->used)return 0;
    for(i=0;i+n<=s->used;i++)if(!memcmp(s->data+i,needle,n))return 1;
    return 0;
}
static unsigned count(const struct sink *s,const unsigned char *needle,size_t n){
    size_t i;unsigned c=0;
    for(i=0;i+n<=s->used;i++)if(!memcmp(s->data+i,needle,n))c++;
    return c;
}
static void feed_chunks(struct mb_pwg_pcl *p,const unsigned char *data,size_t n,
                        size_t step,struct sink *s){
    size_t off=0;
    while(off<n){size_t z=n-off<step?n-off:step;assert(mb_pwg_pcl_feed(p,data+off,z,wr,s)==0);off+=z;}
}
int main(void){
    unsigned char h[1796],doc[4+1796+32];size_t n;
    struct mb_pwg_pcl p;struct sink s={0};
    static const unsigned char rowcmd[]={
        0xb1,0xfa,0x08,0x00,0x00,0x00,
        0x00,0x20,0x40,0x60,0x80,0xa0,0xc0,0xff
    };

    header(h,8,2,8,8,8,18,1);
    memcpy(doc,"RaS2",4);memcpy(doc+4,h,1796);n=1800;
    doc[n++]=1;      /* one encoded line repeated twice */
    doc[n++]=249;    /* 8 literal grayscale color values */
    doc[n++]=0;doc[n++]=32;doc[n++]=64;doc[n++]=96;
    doc[n++]=128;doc[n++]=160;doc[n++]=192;doc[n++]=255;
    mb_pwg_pcl_init(&p);feed_chunks(&p,doc,n,7,&s);
    assert(mb_pwg_pcl_finish(&p,wr,&s)==0);
    assert(has(&s,(const unsigned char *)"@PJL ENTER LANGUAGE=PCLXL",25));
    assert(has(&s,(const unsigned char *)") HP-PCL XL;2;1;",16));
    { static const unsigned char gray_setup[]={
        0xc0,0x02,0xf8,0x64, /* ColorDepth=e8Bit */
        0xc0,0x00,0xf8,0x62, /* ColorMapping=eDirectPixel */
        0xc1,0x08,0x00,0xf8,0x6c,
        0xc1,0x02,0x00,0xf8,0x6b
      };
      assert(has(&s,gray_setup,sizeof gray_setup)); }
    assert(count(&s,rowcmd,sizeof rowcmd)==2);
    { static const unsigned char endjob[]="\x49\x42\033%-12345X@PJL EOJ NAME=\"MiniBox PWG\"\r\n\033%-12345X";
      assert(s.used>=sizeof endjob-1);
      assert(!memcmp(s.data+s.used-(sizeof endjob-1),endjob,sizeof endjob-1)); }
    mb_pwg_pcl_reset(&p);

    memset(&s,0,sizeof s);header(h,8,1,8,24,24,19,3);
    memcpy(doc,"RaS2",4);memcpy(doc+4,h,1796);n=1800;
    doc[n++]=0; /* one row */
    doc[n++]=3; doc[n++]=255;doc[n++]=255;doc[n++]=255; /* 4 white */
    doc[n++]=3; doc[n++]=0;doc[n++]=0;doc[n++]=0;       /* 4 black */
    mb_pwg_pcl_init(&p);feed_chunks(&p,doc,n,1,&s);
    assert(mb_pwg_pcl_finish(&p,wr,&s)==0);
    { static const unsigned char rgbrow[]={
        0xb1,0xfa,0x08,0x00,0x00,0x00,
        0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00
      };
      assert(has(&s,rgbrow,sizeof rgbrow)); }
    mb_pwg_pcl_reset(&p);

    memset(&s,0,sizeof s);header(h,8,1,1,1,1,3,1);
    memcpy(doc,"RaS2",4);memcpy(doc+4,h,1796);n=1800;
    doc[n++]=0;doc[n++]=0;doc[n++]=0xa5; /* one repeated byte */
    mb_pwg_pcl_init(&p);feed_chunks(&p,doc,n,13,&s);
    assert(mb_pwg_pcl_finish(&p,wr,&s)==0);
    { static const unsigned char blackrow[]={0xb1,0xfa,0x04,0x00,0x00,0x00,0x5a,0x00,0x00,0x00};
      assert(has(&s,blackrow,sizeof blackrow)); }
    mb_pwg_pcl_reset(&p);

    /* M1522/A4 at 300 dpi: 2480 mono pixels = 310 bytes.
       PCL XL raster rows must be padded to a 4-byte boundary: 312 bytes. */
    memset(&s,0,sizeof s);header(h,2480,1,1,1,310,3,1);
    memcpy(doc,"RaS2",4);memcpy(doc+4,h,1796);n=1800;
    doc[n++]=0;          /* one row */
    doc[n++]=127;doc[n++]=0xa5; /* 128 repeated bytes */
    doc[n++]=127;doc[n++]=0xa5; /* 128 repeated bytes */
    doc[n++]=53; doc[n++]=0xa5; /* 54 repeated bytes = 310 */
    mb_pwg_pcl_init(&p);feed_chunks(&p,doc,n,11,&s);
    assert(mb_pwg_pcl_finish(&p,wr,&s)==0);
    { static const unsigned char m1522_len[]={0xb1,0xfa,0x38,0x01,0x00,0x00};
      assert(has(&s,m1522_len,sizeof m1522_len));
      assert(s.used>=sizeof m1522_len+312);
      { size_t i;
        for(i=0;i+sizeof m1522_len<=s.used;i++)if(!memcmp(s.data+i,m1522_len,sizeof m1522_len))break;
        assert(i+sizeof m1522_len+312<=s.used);
        assert(s.data[i+sizeof m1522_len+309]==0x5a);
        assert(s.data[i+sizeof m1522_len+310]==0x00);
        assert(s.data[i+sizeof m1522_len+311]==0x00);
      }
    }
    mb_pwg_pcl_reset(&p);

    memset(&s,0,sizeof s);header(h,8,1,8,8,8,18,1);
    memcpy(doc,"RaS2",4);memcpy(doc+4,h,1796);n=1800;
    doc[n++]=0;doc[n++]=128; /* reserved PackBits control */
    mb_pwg_pcl_init(&p);
    assert(mb_pwg_pcl_feed(&p,doc,n,wr,&s)<0);
    mb_pwg_pcl_reset(&p);

    puts("PWG Raster -> PCL XL streaming contract OK");
    return 0;
}
