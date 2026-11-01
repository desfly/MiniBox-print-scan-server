/* Standalone regression for PWG sGray8 -> one full-height HP-style JPEG ReadImage.
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
static int check(unsigned height, unsigned width) {
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
    put32(h+352,width>height?842:595);
    put32(h+356,width>height?595:842);
    put32(h+372,width);put32(h+376,height);
    put32(h+384,8);put32(h+388,8);put32(h+392,width);
    put32(h+400,18);put32(h+420,1);
    if(mb_pwg_pcl_feed(&s,magic,sizeof magic,write_bytes,&b) ||
       mb_pwg_pcl_feed(&s,h,sizeof h,write_bytes,&b)) goto done;
    for(unsigned y=0;y<height;y++)
        if(mb_pwg_pcl_feed(&s,row,sizeof row,write_bytes,&b))goto done;
    if(mb_pwg_pcl_finish(&s,write_bytes,&b))goto done;
    /* Preserve the actual full-width MiniBox output for an independent
     * parser; do not reconstruct it from expected byte patterns. */
    if(height==1700 && width==2338){
        FILE *fp=fopen("/tmp/minibox-generated-pclxl.bin","wb");
        if(!fp){fprintf(stderr,"FAIL: cannot open PCL XL export\\n");goto done;}
        size_t count=fwrite(b.p,1,b.n,fp);
        int close_rc=fclose(fp);
        if(count!=b.n || close_rc!=0){
            fprintf(stderr,"FAIL: incomplete PCL XL export\\n");goto done;
        }
    }
    /* PJL options observed in capture #003 are separated by additional
     * driver metadata. Verify their relative order in MiniBox's filtered PJL,
     * without pretending they are adjacent in the reference capture. */
    {
        static const char *const hp_pjl[]={
            "@PJL SET RET=OFF\n",
            "@PJL SET PLANESINUSE=1\n",
            "@PJL SET GRAYSCALE=BLACKONLY\n",
            "@PJL SET SEPARATORPAGE=OFF\n",
            "@PJL SET EDGETOEDGE=NO\n",
            "@PJL SET ECONOMODE=OFF\n",
            "@PJL SET RESOLUTION=600\n",
            "@PJL SET BITSPERPIXEL=1\n",
            "@PJL ENTER LANGUAGE=PCLXL\n"
        };
        size_t off=0;
        for(size_t opt=0;opt<sizeof hp_pjl/sizeof hp_pjl[0];opt++){
            size_t len=strlen(hp_pjl[opt]);
            while(off+len<=b.n && memcmp(b.p+off,hp_pjl[opt],len))off++;
            if(off+len>b.n){
                fprintf(stderr,"FAIL: HP capture #003 PJL option/order mismatch\\n");
                goto done;
            }
            off+=len;
        }
    }
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
            const unsigned char page_prefix[]={
                0xc0,0x01,0xf8,0x26,0xc0,0x00,0xf8,0x34,
                0xc0,(unsigned char)(width>height?1:0),0xf8,0x28,0xc8,0xc0,0x02,'A','4',0xf8,0x25,0x43,
                /* VendorUnique page setup from successful full HP capture. */
                0xc2,0x04,0x10,0x70,0x68,0xf8,0x91,
                0xc0,0x01,0xf8,0x93,0xc0,0x01,0xf8,0x94,
                0xc0,0x01,0xf8,0x95,0x46,
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
    if(width>height){
        static const unsigned char landscape[]={
            0xc0,0x01,0xf8,0x28, /* HP captured landscape orientation */
            0xc8,0xc0,0x02,'A','4',0xf8,0x25,0x43
        };
        int found=0;
        for(size_t k=0;k+sizeof landscape<=b.n;k++)
            if(!memcmp(b.p+k,landscape,sizeof landscape)){found=1;break;}
        if(!found){fprintf(stderr,"FAIL: landscape page setup absent\\n");goto done;}
    }
    /* Ground exact capture #003 coordinates for its matching raster only.
     * PageOrigin=(100,100), Cursor=(381,179) in the real USB stream. */
    if(width==2338 && height==1700){
        static const unsigned char coords[]={
            0xd3,0x64,0x00,0x64,0x00,0xf8,0x2a,0x75,
            0x61,0x69,
            0xd3,0x7d,0x01,0xb3,0x00,0xf8,0x4c,0x6b
        };
        int found=0;
        for(size_t k=0;k+sizeof coords<=b.n;k++)
            if(!memcmp(b.p+k,coords,sizeof coords)){found=1;break;}
        if(!found){
            fprintf(stderr,"FAIL: capture #003 PageOrigin/Cursor bytes mismatch\\n");
            goto done;
        }
    }
    /* PCL XL geometry must describe the generated PWG page, not copy the
     * photo placement coordinates from the successful HP USB capture. */
    if(width==2338 && height==1700){
        const unsigned char geometry[]={
            0xc0,0x02,0xf8,0x03,0x6a, /* RGB */
            0xc0,0x00,0xf8,0x64,      /* direct pixels */
            0xc0,0x02,0xf8,0x62,      /* 8 bits/component */
            0xc1,0x22,0x09,0xf8,0x6c,/* source width 2338 */
            0xc1,0xa4,0x06,0xf8,0x6b,/* source height 1700 */
            0xd1,0xa3,0x17,0x30,0x11,0xf8,0x67,0xb0 /* captured DestSize=6051x4400 */
        };
        int found=0;
        for(size_t k=0;k+sizeof geometry<=b.n;k++)
            if(!memcmp(b.p+k,geometry,sizeof geometry)){found=1;break;}
        if(!found){fprintf(stderr,"FAIL: 2338x1700 landscape PCL XL image geometry mismatch\\n");goto done;}
    }
    /* Strict framing: JPEG must begin immediately after the complete
     * contiguous HP-style PCL XL image descriptor.  A loose byte search
     * anywhere in the job could otherwise match arbitrary JPEG payload. */
    {
        unsigned char desc[]={
            0xc0,0x02,0xf8,0x03,0x6a,
            0xc0,0x00,0xf8,0x64,
            0xc0,0x02,0xf8,0x62,
            0xc1,0,0,0xf8,0x6c,
            0xc1,0,0,0xf8,0x6b,
            0xd1,0,0,0,0,0xf8,0x67,0xb0,
            0xc1,0,0,0xf8,0x6d,
            0xc1,0,0,0xf8,0x63,
            0xc0,0x02,0xf8,0x65,0xb1,0xfa
        };
        const unsigned x=width,y=height;
        size_t at=0,matches=0;
        desc[14]=(unsigned char)x;desc[15]=(unsigned char)(x>>8);
        desc[19]=(unsigned char)y;desc[20]=(unsigned char)(y>>8);
        const unsigned dw=(width==2338 && height==1700)?6051u:x;
        const unsigned dh=(width==2338 && height==1700)?4400u:y;
        desc[24]=(unsigned char)dw;desc[25]=(unsigned char)(dw>>8);
        desc[26]=(unsigned char)dh;desc[27]=(unsigned char)(dh>>8);
        desc[37]=(unsigned char)y;desc[38]=(unsigned char)(y>>8);
        for(size_t k=0;k+sizeof desc<=b.n;k++)
            if(!memcmp(b.p+k,desc,sizeof desc)){at=k;matches++;}
        if(matches!=1){
            fprintf(stderr,"FAIL: expected one contiguous PCL XL image descriptor, found %zu\\n",matches);
            goto done;
        }
        (void)at;
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
        if(start!=covered || !lines || lines!=height || blocks!=0 ||
           lines>height-covered || length<4 || length>b.n-jpeg)goto done;
        if(b.p[jpeg]!=0xff || b.p[jpeg+1]!=0xd8 ||
           b.p[jpeg+length-2]!=0xff || b.p[jpeg+length-1]!=0xd9)goto done;
        /* Decode each independent JPEG strip; validate its dimensions and
         * pixels rather than trusting marker bytes or framing alone. */
        {
            struct jpeg_decompress_struct d;
            struct jpeg_error_mgr err;
            unsigned char *decoded=malloc((size_t)width*3u);
            int valid=decoded!=NULL;
            memset(&d,0,sizeof d);
            d.err=jpeg_std_error(&err);
            jpeg_create_decompress(&d);
            jpeg_mem_src(&d,b.p+jpeg,length);
            if(jpeg_read_header(&d,TRUE)!=JPEG_HEADER_OK ||
               d.image_width!=width || d.image_height!=lines) {
                jpeg_destroy_decompress(&d);
                free(decoded);
                goto done;
            }
            d.out_color_space=JCS_RGB;
            jpeg_start_decompress(&d);
            if(d.output_width!=width || d.output_height!=lines ||
               d.output_components!=3) valid=0;
            while(valid && d.output_scanline<d.output_height) {
                JSAMPROW scan=decoded;
                if(jpeg_read_scanlines(&d,&scan,1)!=1){valid=0;break;}
                for(size_t k=0;k<(size_t)width*3u;k++)
                    if(decoded[k]!=255){valid=0;break;}
            }
            if(valid) jpeg_finish_decompress(&d);
            jpeg_destroy_decompress(&d);
            free(decoded);
            if(!valid)goto done;
        }
        covered+=lines;blocks++;pos=jpeg+length;
    }
    /* Every emitted JPEG block must be followed by exactly the expected
     * EndImage, EndPage, CloseDataSource, EndSession and PJL job trailer.
     * Without this, a green JPEG test could hide invalid trailing PCL XL. */
    {
        static const unsigned char trailer[]={
            0xb2,0x60,0xc1,0x01,0x00,0xf8,0x31,0x44,0x49,0x42,
            0x1b,'%','-','1','2','3','4','5','X',
            '@','P','J','L',' ','E','O','J',0x0a,
            0x1b,'%','-','1','2','3','4','5','X'
        };
        if(pos>b.n || b.n-pos!=sizeof(trailer) ||
           memcmp(b.p+pos,trailer,sizeof(trailer))){
            fprintf(stderr,"FAIL: invalid PCL XL/PJL job trailer\\n");
            goto done;
        }
    }
    ok=(blocks==1 && covered==height);
done:
    if(!ok)fprintf(stderr,"FAIL height=%u blocks=%u covered=%u bytes=%zu\n",
                   height,blocks,covered,b.n);
    mb_pwg_pcl_reset(&s);free(b.p);
    return ok?0:1;
}

/* Regression: verify actual halftones survive PWG sGray8 -> HP JPEG.
 * A white-only test misses inversion, contouring and lost gray levels.
 * This is pixel validation; it never changes the captured HP protocol. */
static int check_gray_ramp(void) {
    enum { W=128, H=16 };
    unsigned char header[1796]={0};
    const unsigned char magic[]={'R','a','S','2'};
    unsigned char encoded[2+W];
    struct mb_pwg_pcl state;
    struct buffer buf={0};
    struct jpeg_decompress_struct d;
    struct jpeg_error_mgr err;
    size_t start=SIZE_MAX, count=0;
    int ok=0;
    mb_pwg_pcl_init(&state);
    put32(header+276,600);put32(header+280,600);
    put32(header+352,595);put32(header+356,842);
    put32(header+372,W);put32(header+376,H);
    put32(header+384,8);put32(header+388,8);
    put32(header+392,W);put32(header+400,18);put32(header+420,1);
    encoded[0]=0;encoded[1]=129; /* one row, 128 literal gray pixels */
    for(unsigned x=0;x<W;x++)encoded[x+2]=(unsigned char)(x*2);
    if(mb_pwg_pcl_feed(&state,magic,sizeof magic,write_bytes,&buf) ||
       mb_pwg_pcl_feed(&state,header,sizeof header,write_bytes,&buf))goto finish;
    for(unsigned y=0;y<H;y++)
        if(mb_pwg_pcl_feed(&state,encoded,sizeof encoded,write_bytes,&buf))
            goto finish;
    if(mb_pwg_pcl_finish(&state,write_bytes,&buf))goto finish;
    for(size_t i=0;i+6<buf.n;i++){
        if(buf.p[i]==0xb1 && buf.p[i+1]==0xfa){
            uint32_t len=get32(buf.p+i+2);
            if(len<4 || len>buf.n-i-6)goto finish;
            if(buf.p[i+6]!=0xff || buf.p[i+7]!=0xd8)goto finish;
            start=i+6;count=len;break;
        }
    }
    if(start==SIZE_MAX)goto finish;
    memset(&d,0,sizeof d);
    d.err=jpeg_std_error(&err);
    jpeg_create_decompress(&d);
    jpeg_mem_src(&d,buf.p+start,count);
    if(jpeg_read_header(&d,TRUE)!=JPEG_HEADER_OK ||
       d.image_width!=W || d.image_height!=H){
        jpeg_destroy_decompress(&d);goto finish;
    }
    d.out_color_space=JCS_RGB;
    jpeg_start_decompress(&d);
    {
        unsigned char pixels[W*3];
        JSAMPROW row=pixels;
        int valid=1;
        while(d.output_scanline<d.output_height){
            unsigned y=d.output_scanline;
            if(jpeg_read_scanlines(&d,&row,1)!=1){valid=0;break;}
            for(unsigned x=0;x<W;x++){
                int expected=(int)x*2;
                for(unsigned c=0;c<3;c++){
                    int got=pixels[x*3+c];
                    if(got<expected-4 || got>expected+4){valid=0;break;}
                }
                if(!valid)break;
            }
            if(!valid){fprintf(stderr,"FAIL: sGray8 halftone loss at decoded row %u\\n",y);break;}
        }
        if(valid)jpeg_finish_decompress(&d);
        ok=valid;
    }
    jpeg_destroy_decompress(&d);
finish:
    if(!ok)fprintf(stderr,"FAIL: PWG sGray8 -> HP JPEG ramp\\n");
    mb_pwg_pcl_reset(&state);
    free(buf.p);
    return ok?0:1;
}

int main(void) {
    if(check_gray_ramp())return 1;
    const unsigned heights[]={1,127,128,129,255,256,257,1080,3508,4960};
    for(size_t i=0;i<sizeof heights/sizeof heights[0];i++)
        if(check(heights[i],32))return 1;
    /* Full-size landscape source captured in successful HP UPD USB trace. */
    if(check(1700,2338))return 1;
    puts("PASS: single full-height JPEG ReadImage, including 2338x1700 landscape");
    return 0;
}
