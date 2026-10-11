#include "pwg_to_pcl.h"
#include <stdio.h>
#include <jpeglib.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#define PWG_HEADER_SIZE 1796u
#define PWG_MAX_WIDTH 7200u
#define PWG_MAX_HEIGHT 10800u
#define PWG_MAX_LINE (64u*1024u)

/* Verified from M1522-PRINT-REAL.pcap / HP Universal Printing PCL 6. */
#define M1522_RENDER_DPI 600u
#define M1522_JPEG_QUALITY 95
/* Exact native capture proves 1920x1080 is accepted by the M1522 decoder.
 * Keep width inside the captured envelope.  The 256-row staging band is a
 * renderer memory choice only; it is not treated as a printer protocol rule. */
#define M1522_VERIFIED_JPEG_MAX_WIDTH 1920u
#define M1522_VERIFIED_JPEG_MAX_HEIGHT 1080u
#define M1522_RGB_BAND_ROWS 256u

struct mb_jpeg_error {
    struct jpeg_error_mgr pub;
    jmp_buf jump;
};

struct mb_rgb_band {
    unsigned char *data;
    size_t stride;
    unsigned rows;
    unsigned start_row;
};

static uint32_t be32(const unsigned char *p){
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}
static int out(mb_pwg_write_fn fn,void *ctx,const void *buf,size_t len){
    return fn&&buf&&(!len||fn(ctx,(const unsigned char *)buf,len)==0)?0:-1;
}
static void le16(unsigned char *p,unsigned v){p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);}
static void le32(unsigned char *p,uint32_t v){
    p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);
    p[2]=(unsigned char)(v>>16);p[3]=(unsigned char)(v>>24);
}
static int pxl_u8(mb_pwg_write_fn fn,void *ctx,unsigned v,unsigned attr){
    unsigned char b[4]={0xc0,(unsigned char)v,0xf8,(unsigned char)attr};
    return out(fn,ctx,b,sizeof b);
}
static int pxl_u16(mb_pwg_write_fn fn,void *ctx,unsigned v,unsigned attr){
    unsigned char b[5]={0xc1,0,0,0xf8,(unsigned char)attr};
    le16(b+1,v);return out(fn,ctx,b,sizeof b);
}
static int pxl_xy16(mb_pwg_write_fn fn,void *ctx,unsigned x,unsigned y,unsigned attr){
    unsigned char b[7]={0xd1,0,0,0,0,0xf8,(unsigned char)attr};
    le16(b+1,x);le16(b+3,y);return out(fn,ctx,b,sizeof b);
}
static int pxl_sxy16(mb_pwg_write_fn fn,void *ctx,int x,int y,unsigned attr){
    unsigned char b[7]={0xd3,0,0,0,0,0xf8,(unsigned char)attr};
    le16(b+1,(unsigned)(uint16_t)x);le16(b+3,(unsigned)(uint16_t)y);
    return out(fn,ctx,b,sizeof b);
}
static int pxl_real_xy_1(mb_pwg_write_fn fn,void *ctx,unsigned attr){
    static const unsigned char one_xy[9]={0xd5,0x00,0x00,0x80,0x3f,0x00,0x00,0x80,0x3f};
    unsigned char a[2]={0xf8,(unsigned char)attr};
    return out(fn,ctx,one_xy,sizeof one_xy)||out(fn,ctx,a,sizeof a)?-1:0;
}
static int pxl_u8_array(mb_pwg_write_fn fn,void *ctx,const char *v,unsigned attr){
    size_t n=strlen(v);
    unsigned char pfx[3]={0xc8,0xc0,0};
    unsigned char a[2]={0xf8,(unsigned char)attr};
    if(n>255)return -1;
    pfx[2]=(unsigned char)n;
    return out(fn,ctx,pfx,sizeof pfx)||out(fn,ctx,v,n)||out(fn,ctx,a,sizeof a)?-1:0;
}
static int pxl_op(mb_pwg_write_fn fn,void *ctx,unsigned op){
    unsigned char b=(unsigned char)op;return out(fn,ctx,&b,1);
}

static void jpeg_error_exit(j_common_ptr cinfo){
    struct mb_jpeg_error *e=(struct mb_jpeg_error *)cinfo->err;
    longjmp(e->jump,1);
}
static void rgb_band_free(struct mb_pwg_pcl *s){
    struct mb_rgb_band *b=(struct mb_rgb_band *)s->image_ctx;
    if(!b)return;
    free(b->data);free(b);s->image_ctx=0;
}
static void free_page(struct mb_pwg_pcl *s){
    rgb_band_free(s);
    free(s->line);free(s->mono);s->line=0;s->mono=0;
    s->line_cap=s->mono_cap=0;s->line_used=0;
}
void mb_pwg_pcl_init(struct mb_pwg_pcl *s){
    if(!s)return;
    memset(s,0,sizeof *s);
    s->phase=MB_PWG_MAGIC;
}
void mb_pwg_pcl_reset(struct mb_pwg_pcl *s){
    if(!s)return;
    free_page(s);
    mb_pwg_pcl_init(s);
}

static unsigned pxl_media_enum(const struct mb_pwg_pcl *s){
    unsigned w=s->page_width_points,h=s->page_height_points;
    if((w>=607&&w<=617&&h>=787&&h<=797)||(h>=607&&h<=617&&w>=787&&w<=797))return 0;
    if((w>=607&&w<=617&&h>=1003&&h<=1013)||(h>=607&&h<=617&&w>=1003&&w<=1013))return 1;
    return 2;
}
static int is_a4(const struct mb_pwg_pcl *s){
    unsigned w=s->page_width_points,h=s->page_height_points;
    return (w>=590&&w<=600&&h>=837&&h<=847)||(h>=590&&h<=600&&w>=837&&w<=847);
}
static unsigned scaled_to_600(unsigned pixels,unsigned dpi){
    uint64_t n=(uint64_t)pixels*M1522_RENDER_DPI+(dpi/2u);
    return (unsigned)(n/dpi);
}
static int job_start(struct mb_pwg_pcl *s,mb_pwg_write_fn fn,void *ctx){
    char pjl[256];int n;static const char pxl[]=") HP-PCL XL;2;1;\n";
    if(s->job_started)return 0;
    n=snprintf(pjl,sizeof pjl,
        "\033%%-12345X"
        "@PJL JOB NAME=\"MiniBox PWG\"\r\n"
        "@PJL SET PLANESINUSE=1\r\n"
        "@PJL SET GRAYSCALE=BLACKONLY\r\n"
        "@PJL SET ECONOMODE=OFF\r\n"
        "@PJL SET RESOLUTION=600\r\n"
        "@PJL SET BITSPERPIXEL=1\r\n"
        "@PJL ENTER LANGUAGE=PCLXL\r\n");
    if(n<0||(size_t)n>=sizeof pjl||out(fn,ctx,pjl,(size_t)n)||out(fn,ctx,pxl,sizeof pxl-1))return -1;
    if(pxl_xy16(fn,ctx,M1522_RENDER_DPI,M1522_RENDER_DPI,137)||
       pxl_u8(fn,ctx,0,134)||pxl_u8(fn,ctx,3,143)||pxl_op(fn,ctx,0x41)||
       pxl_u8(fn,ctx,0,136)||pxl_u8(fn,ctx,1,130)||pxl_op(fn,ctx,0x48))return -1;
    s->job_started=1;return 0;
}
static int page_start(struct mb_pwg_pcl *s,mb_pwg_write_fn fn,void *ctx){
    unsigned orientation=s->page_width_points>s->page_height_points?1u:0u;
    unsigned dw=scaled_to_600(s->width,s->xdpi);
    unsigned dh=scaled_to_600(s->height,s->ydpi);
    unsigned is_1bit=((s->color_space==3||s->color_space==18)&&s->bits_per_pixel==1);
    unsigned cs=is_1bit?1u:2u;
    unsigned depth=is_1bit?0u:2u;
    if(dw>65535u||dh>65535u)return -1;
    if(job_start(s,fn,ctx)||
       pxl_u8(fn,ctx,1,38)||pxl_u8(fn,ctx,0,52)||pxl_u8(fn,ctx,orientation,40))return -1;
    if(is_a4(s)){
        if(pxl_u8_array(fn,ctx,"A4",37))return -1;
    }else if(pxl_u8(fn,ctx,pxl_media_enum(s),37))return -1;
    if(pxl_op(fn,ctx,0x43)||
       pxl_real_xy_1(fn,ctx,43)||pxl_op(fn,ctx,0x77)||
       pxl_u8(fn,ctx,1,3)||pxl_op(fn,ctx,0x6a)||
       pxl_u8(fn,ctx,0,45)||pxl_op(fn,ctx,0x78)||
       pxl_u8(fn,ctx,0,45)||pxl_op(fn,ctx,0x7c)||
       pxl_u8(fn,ctx,204,44)||pxl_op(fn,ctx,0x7b)||
       pxl_u8(fn,ctx,cs,3)||pxl_op(fn,ctx,0x6a))return -1;
    if(is_1bit){
        if(pxl_sxy16(fn,ctx,0,0,76)||pxl_op(fn,ctx,0x6b)||
           pxl_u8(fn,ctx,0,100)||pxl_u8(fn,ctx,depth,98)||
           pxl_u16(fn,ctx,s->width,108)||pxl_u16(fn,ctx,s->height,107)||
           pxl_xy16(fn,ctx,dw,dh,103)||pxl_op(fn,ctx,0xb0))return -1;
    }
    return 0;
}
static int page_end(struct mb_pwg_pcl *s,mb_pwg_write_fn fn,void *ctx){
    unsigned is_1bit=((s->color_space==3||s->color_space==18)&&s->bits_per_pixel==1);
    if(is_1bit&&pxl_op(fn,ctx,0xb2))return -1;
    return pxl_op(fn,ctx,0x44)?-1:0;
}


static int jpeg_encode_tile(const unsigned char *base,size_t stride,
                            unsigned x,unsigned w,unsigned h,
                            unsigned char **data,unsigned long *size){
    struct jpeg_compress_struct ci;
    struct mb_jpeg_error err;
    JSAMPROW scan[1];
    unsigned y;
    int created=0;
    memset(&ci,0,sizeof ci);memset(&err,0,sizeof err);
    *data=0;*size=0;
    ci.err=jpeg_std_error(&err.pub);err.pub.error_exit=jpeg_error_exit;
    if(setjmp(err.jump)){
        if(created)jpeg_destroy_compress(&ci);
        free(*data);*data=0;*size=0;return -1;
    }
    jpeg_create_compress(&ci);created=1;
    jpeg_mem_dest(&ci,data,size);
    ci.image_width=w;ci.image_height=h;ci.input_components=3;ci.in_color_space=JCS_RGB;
    jpeg_set_defaults(&ci);jpeg_set_quality(&ci,M1522_JPEG_QUALITY,TRUE);
    {
        int i;for(i=0;i<3;i++){ci.comp_info[i].h_samp_factor=1;ci.comp_info[i].v_samp_factor=1;}
    }
    ci.optimize_coding=FALSE;ci.write_JFIF_header=TRUE;
    ci.density_unit=0;ci.X_density=1;ci.Y_density=1;
    jpeg_start_compress(&ci,TRUE);
    for(y=0;y<h;y++){
        scan[0]=(JSAMPROW)(base+(size_t)y*stride+(size_t)x*3u);
        if(jpeg_write_scanlines(&ci,scan,1)!=1){jpeg_destroy_compress(&ci);free(*data);*data=0;return -1;}
    }
    jpeg_finish_compress(&ci);jpeg_destroy_compress(&ci);
    return (*data&&*size&&*size<=0xffffffffUL)?0:-1;
}
static int emit_rgb_tile(struct mb_pwg_pcl *s,mb_pwg_write_fn fn,void *ctx,
                         const struct mb_rgb_band *band,
                         unsigned x,unsigned w){
    unsigned char *jpeg=0,emb[5]={0xfa,0,0,0,0};
    unsigned long jpeg_size=0;
    unsigned dx=scaled_to_600(x,s->xdpi);
    unsigned dy=scaled_to_600(band->start_row,s->ydpi);
    unsigned dw=scaled_to_600(w,s->xdpi);
    unsigned dh=scaled_to_600(band->rows,s->ydpi);
    if(!w||!band->rows||w>M1522_VERIFIED_JPEG_MAX_WIDTH||
       band->rows>M1522_VERIFIED_JPEG_MAX_HEIGHT||
       dx>32767u||dy>32767u||dw>65535u||dh>65535u)return -1;
    if(jpeg_encode_tile(band->data,band->stride,x,w,band->rows,&jpeg,&jpeg_size))return -1;
    if(pxl_sxy16(fn,ctx,(int)dx,(int)dy,76)||pxl_op(fn,ctx,0x6b)||
       pxl_u8(fn,ctx,0,100)||pxl_u8(fn,ctx,2,98)||
       pxl_u16(fn,ctx,w,108)||pxl_u16(fn,ctx,band->rows,107)||
       pxl_xy16(fn,ctx,dw,dh,103)||pxl_op(fn,ctx,0xb0)||
       pxl_u16(fn,ctx,0,109)||pxl_u16(fn,ctx,band->rows,99)||
       pxl_u8(fn,ctx,2,101)||pxl_op(fn,ctx,0xb1)){
        free(jpeg);return -1;
    }
    le32(emb+1,(uint32_t)jpeg_size);
    if(out(fn,ctx,emb,sizeof emb)||out(fn,ctx,jpeg,(size_t)jpeg_size)||
       pxl_op(fn,ctx,0xb2)){free(jpeg);return -1;}
    free(jpeg);return 0;
}
static int flush_rgb_band(struct mb_pwg_pcl *s,mb_pwg_write_fn fn,void *ctx){
    struct mb_rgb_band *band=(struct mb_rgb_band *)s->image_ctx;
    unsigned x;
    if(!band||!band->rows)return 0;
    for(x=0;x<s->width;){
        unsigned w=s->width-x;
        if(w>M1522_VERIFIED_JPEG_MAX_WIDTH)w=M1522_VERIFIED_JPEG_MAX_WIDTH;
        if(emit_rgb_tile(s,fn,ctx,band,x,w))return -1;
        x+=w;
    }
    band->start_row+=band->rows;band->rows=0;
    return 0;
}

static int parse_header(struct mb_pwg_pcl *s,mb_pwg_write_fn fn,void *ctx){
    unsigned unit;
    s->xdpi=be32(s->header+276);s->ydpi=be32(s->header+280);
    s->page_width_points=be32(s->header+352);s->page_height_points=be32(s->header+356);
    s->width=be32(s->header+372);s->height=be32(s->header+376);
    s->bits_per_color=be32(s->header+384);s->bits_per_pixel=be32(s->header+388);
    s->bytes_per_line=be32(s->header+392);s->color_order=be32(s->header+396);
    s->color_space=be32(s->header+400);s->num_colors=be32(s->header+420);
    fprintf(stderr,
            "minibox-pwg: dpi=%ux%u size=%ux%u bpc=%u bpp=%u bpl=%u order=%u cs=%u colors=%u\\n",
            s->xdpi,s->ydpi,s->width,s->height,s->bits_per_color,s->bits_per_pixel,
            s->bytes_per_line,s->color_order,s->color_space,s->num_colors);
    if(!s->width||!s->height||s->width>PWG_MAX_WIDTH||s->height>PWG_MAX_HEIGHT||
       !s->bytes_per_line||s->bytes_per_line>PWG_MAX_LINE||
       !s->xdpi||s->xdpi!=s->ydpi||(s->xdpi!=300&&s->xdpi!=600)||
       s->color_order!=0)return -1;
    if(s->color_space==18&&s->bits_per_color==1&&s->bits_per_pixel==1&&s->num_colors==1)unit=1;
    else if(s->color_space==18&&s->bits_per_color==8&&s->bits_per_pixel==8&&s->num_colors==1)unit=1;
    else if(s->color_space==19&&s->bits_per_color==8&&s->bits_per_pixel==24&&s->num_colors==3)unit=3;
    else if(s->color_space==3&&s->bits_per_color==1&&s->bits_per_pixel==1&&s->num_colors==1)unit=1;
    else return -2;
    if(s->bits_per_pixel==24&&s->bytes_per_line<s->width*3u)return -1;
    if(s->bits_per_pixel==8&&s->bytes_per_line<s->width)return -1;
    if(s->bits_per_pixel==1&&s->bytes_per_line<(s->width+7u)/8u)return -1;
    s->line=(unsigned char *)malloc(s->bytes_per_line);
    if(s->color_space==3&&s->bits_per_pixel==1){
        s->mono_cap=(s->width+7u)/8u;
        s->mono=(unsigned char *)malloc(s->mono_cap);
    }else if(s->color_space==18&&s->bits_per_pixel==8){
        s->mono_cap=(size_t)s->width*3u;
        s->mono=(unsigned char *)malloc(s->mono_cap);
    }
    if(s->bits_per_pixel==8||s->bits_per_pixel==24){
        struct mb_rgb_band *band=(struct mb_rgb_band *)calloc(1,sizeof *band);
        size_t stride=(size_t)s->width*3u;
        if(band){
            band->stride=stride;band->start_row=0;
            band->data=(unsigned char *)malloc(stride*M1522_RGB_BAND_ROWS);
            if(!band->data){free(band);band=0;}
        }
        s->image_ctx=band;
    }
    if(!s->line||
       (((s->color_space==3&&s->bits_per_pixel==1)||
         (s->color_space==18&&s->bits_per_pixel==8))&&!s->mono)||
       ((s->bits_per_pixel==8||s->bits_per_pixel==24)&&!s->image_ctx)){
        free_page(s);return -3;
    }
    s->line_cap=s->bytes_per_line;s->line_used=0;s->row=0;s->color_value_bytes=unit;
    if(page_start(s,fn,ctx)){free_page(s);return -4;}
    s->pages++;return 0;
}
static int black_row(struct mb_pwg_pcl *s){
    unsigned x;
    if(!(s->color_space==3&&s->bits_per_pixel==1))return -1;
    for(x=0;x<s->mono_cap;x++)s->mono[x]=(unsigned char)~s->line[x];
    return 0;
}
static int emit_1bit_line(struct mb_pwg_pcl *s,mb_pwg_write_fn fn,void *ctx){
    unsigned i;
    const unsigned char *row=s->line;
    size_t row_bytes=(s->width+7u)/8u;
    if(s->color_space==3){
        if(black_row(s))return -1;
        row=s->mono;
    }else if(s->color_space!=18)return -1;
    for(i=0;i<s->repeat_lines;i++){
        unsigned char emb[5]={0xfa,0,0,0,0};
        static const unsigned char pad[3]={0,0,0};
        size_t padded=(row_bytes+3u)&~3u;
        size_t padding=padded-row_bytes;
        if(pxl_u16(fn,ctx,s->row,109)||pxl_u16(fn,ctx,1,99)||
           pxl_u8(fn,ctx,0,101)||pxl_op(fn,ctx,0xb1))return -1;
        le32(emb+1,(uint32_t)padded);
        if(out(fn,ctx,emb,sizeof emb)||out(fn,ctx,row,row_bytes)||
           (padding&&out(fn,ctx,pad,padding)))return -1;
        if(++s->row>s->height)return -1;
    }
    return 0;
}
static int emit_continuous_line(struct mb_pwg_pcl *s,mb_pwg_write_fn fn,void *ctx){
    unsigned i;
    const unsigned char *row=s->line;
    struct mb_rgb_band *band=(struct mb_rgb_band *)s->image_ctx;
    if(!band||!band->data)return -1;
    if(s->color_space==18&&s->bits_per_pixel==8){
        unsigned x;
        if(!s->mono||s->mono_cap<(size_t)s->width*3u)return -1;
        for(x=0;x<s->width;x++){
            unsigned char g=s->line[x];
            uint64_t add=(uint64_t)s->gray_hist[g]+s->repeat_lines;
            s->gray_hist[g]=(uint32_t)(add>0xffffffffu?0xffffffffu:add);
            s->mono[(size_t)x*3u+0u]=g;
            s->mono[(size_t)x*3u+1u]=g;
            s->mono[(size_t)x*3u+2u]=g;
        }
        {
            uint64_t add=(uint64_t)s->gray_samples+(uint64_t)s->width*s->repeat_lines;
            s->gray_samples=(uint32_t)(add>0xffffffffu?0xffffffffu:add);
        }
        row=s->mono;
    }
    for(i=0;i<s->repeat_lines;i++){
        if(band->rows==M1522_RGB_BAND_ROWS&&flush_rgb_band(s,fn,ctx))return -1;
        memcpy(band->data+(size_t)band->rows*band->stride,row,band->stride);
        band->rows++;
        if(++s->row>s->height)return -1;
    }
    if((band->rows==M1522_RGB_BAND_ROWS||s->row==s->height)&&flush_rgb_band(s,fn,ctx))return -1;
    return 0;
}
static int emit_line(struct mb_pwg_pcl *s,mb_pwg_write_fn fn,void *ctx){
    int rc=((s->color_space==3||s->color_space==18)&&s->bits_per_pixel==1)?
        emit_1bit_line(s,fn,ctx):emit_continuous_line(s,fn,ctx);
    if(rc)return rc;
    if(s->row==s->height){
        if(s->color_space==18&&s->bits_per_pixel==8&&s->gray_samples){
            unsigned v,unique=0,minv=255,maxv=0;
            uint64_t mid=0,black=s->gray_hist[0],white=s->gray_hist[255];
            for(v=0;v<256;v++)if(s->gray_hist[v]){
                if(!unique)minv=v;
                maxv=v;unique++;
                if(v>0&&v<255)mid+=s->gray_hist[v];
            }
            fprintf(stderr,
                    "minibox-pwg-gray: samples=%u unique=%u min=%u max=%u black=%lu white=%lu mid=%lu\\n",
                    s->gray_samples,unique,minv,maxv,
                    (unsigned long)black,(unsigned long)white,(unsigned long)mid);
        }
        if(page_end(s,fn,ctx))return -1;
        free_page(s);s->header_used=0;s->phase=MB_PWG_HEADER;
    }else s->phase=MB_PWG_REPEAT;
    return 0;
}
static int append_repeat_value(struct mb_pwg_pcl *s){
    unsigned i;
    if(s->token_value_used!=s->color_value_bytes)return -1;
    if(s->token_units>s->line_cap-s->line_used)return -1;
    if(s->color_value_bytes>1&&s->token_units>s->line_cap/s->color_value_bytes)return -1;
    for(i=0;i<s->token_units;i++){
        if(s->line_used+s->color_value_bytes>s->line_cap)return -1;
        memcpy(s->line+s->line_used,s->token_value,s->color_value_bytes);
        s->line_used+=s->color_value_bytes;
    }
    return 0;
}
static int consume_byte(struct mb_pwg_pcl *s,unsigned char ch,
                        mb_pwg_write_fn fn,void *ctx){
    int rc;
    switch(s->phase){
    case MB_PWG_MAGIC:
        s->magic[s->magic_used++]=ch;
        if(s->magic_used==4){
            if(memcmp(s->magic,"RaS2",4))return -1;
            s->phase=MB_PWG_HEADER;s->header_used=0;
        }
        return 0;
    case MB_PWG_HEADER:
        s->header[s->header_used++]=ch;
        if(s->header_used==PWG_HEADER_SIZE){
            rc=parse_header(s,fn,ctx);if(rc)return rc;
            s->phase=MB_PWG_REPEAT;
        }
        return 0;
    case MB_PWG_REPEAT:
        s->repeat_lines=(unsigned)ch+1u;
        if(s->row+s->repeat_lines>s->height)return -1;
        s->line_used=0;s->phase=MB_PWG_CONTROL;return 0;
    case MB_PWG_CONTROL:
        if(ch==128){
            unsigned char fill=(s->color_space==18||s->color_space==19)?0xffu:0x00u;
            if(s->line_used>s->bytes_per_line)return -1;
            memset(s->line+s->line_used,fill,s->bytes_per_line-s->line_used);
            s->line_used=s->bytes_per_line;
            return emit_line(s,fn,ctx);
        }
        s->token_value_used=0;
        if(ch<=127){
            s->token_units=(unsigned)ch+1u;s->phase=MB_PWG_REPEAT_VALUE;
        }else{
            s->token_units=257u-(unsigned)ch;
            s->token_bytes_left=s->token_units*s->color_value_bytes;
            if(s->token_bytes_left>s->line_cap-s->line_used)return -1;
            s->phase=MB_PWG_LITERAL;
        }
        return 0;
    case MB_PWG_REPEAT_VALUE:
        if(s->token_value_used>=sizeof s->token_value)return -1;
        s->token_value[s->token_value_used++]=ch;
        if(s->token_value_used==s->color_value_bytes){
            if(append_repeat_value(s))return -1;
            if(s->line_used==s->bytes_per_line)return emit_line(s,fn,ctx);
            s->phase=MB_PWG_CONTROL;
        }
        return 0;
    case MB_PWG_LITERAL:
        if(s->line_used>=s->line_cap||!s->token_bytes_left)return -1;
        s->line[s->line_used++]=ch;s->token_bytes_left--;
        if(!s->token_bytes_left){
            if(s->line_used==s->bytes_per_line)return emit_line(s,fn,ctx);
            s->phase=MB_PWG_CONTROL;
        }
        return 0;
    default:return -1;
    }
}
int mb_pwg_pcl_feed(struct mb_pwg_pcl *s,const unsigned char *data,size_t len,
                    mb_pwg_write_fn fn,void *ctx){
    size_t i;int rc;
    if(!s||(!data&&len)||!fn||s->failed)return -1;
    for(i=0;i<len;i++){
        rc=consume_byte(s,data[i],fn,ctx);
        if(rc){s->failed=rc;return rc;}
    }
    return 0;
}
int mb_pwg_pcl_finish(struct mb_pwg_pcl *s,mb_pwg_write_fn fn,void *ctx){
    static const unsigned char trailer[]={0x49,0x42};
    static const char pjl_end[]="\033%-12345X@PJL EOJ NAME=\"MiniBox PWG\"\r\n\033%-12345X";
    if(!s||!fn||s->failed)return -1;
    if(!s->pages||s->phase!=MB_PWG_HEADER||s->header_used!=0)return -2;
    if(out(fn,ctx,trailer,sizeof trailer)||out(fn,ctx,pjl_end,sizeof pjl_end-1)){
        s->failed=-3;return -3;
    }
    return 0;
}
