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
#define M1522_JPEG_BLOCK_ROWS 1080u

struct mb_jpeg_error {
    struct jpeg_error_mgr pub;
    jmp_buf jump;
};

struct mb_jpeg_block {
    struct jpeg_compress_struct cinfo;
    struct mb_jpeg_error err;
    unsigned char *data;
    unsigned long size;
    unsigned start_line;
    unsigned rows_target;
    unsigned rows_written;
    int created;
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
static void jpeg_block_free(struct mb_pwg_pcl *s){
    struct mb_jpeg_block *b=(struct mb_jpeg_block *)s->image_ctx;
    if(!b)return;
    if(b->created)jpeg_destroy_compress(&b->cinfo);
    free(b->data);
    free(b);
    s->image_ctx=0;
}
static void free_page(struct mb_pwg_pcl *s){
    jpeg_block_free(s);
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
    unsigned cs=s->color_space==19?2u:1u;
    unsigned depth=(s->color_space==3&&s->bits_per_pixel==1)?0u:2u;
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
       pxl_sxy16(fn,ctx,0,0,76)||pxl_op(fn,ctx,0x6b)||
       pxl_u8(fn,ctx,cs,3)||pxl_op(fn,ctx,0x6a)||
       pxl_u8(fn,ctx,0,100)||
       pxl_u8(fn,ctx,depth,98)||
       pxl_u16(fn,ctx,s->width,108)||pxl_u16(fn,ctx,s->height,107)||
       pxl_xy16(fn,ctx,dw,dh,103)||pxl_op(fn,ctx,0xb0))return -1;
    return 0;
}
static int page_end(mb_pwg_write_fn fn,void *ctx){
    return pxl_op(fn,ctx,0xb2)||pxl_op(fn,ctx,0x44)?-1:0;
}

static int jpeg_block_start(struct mb_pwg_pcl *s){
    struct mb_jpeg_block *b;
    unsigned remaining=s->height-s->row;
    unsigned target=remaining>M1522_JPEG_BLOCK_ROWS?M1522_JPEG_BLOCK_ROWS:remaining;
    b=(struct mb_jpeg_block *)calloc(1,sizeof *b);
    if(!b||!target){free(b);return -1;}
    b->start_line=s->row;b->rows_target=target;
    b->cinfo.err=jpeg_std_error(&b->err.pub);
    b->err.pub.error_exit=jpeg_error_exit;
    if(setjmp(b->err.jump)){
        if(b->created)jpeg_destroy_compress(&b->cinfo);
        free(b->data);free(b);return -1;
    }
    jpeg_create_compress(&b->cinfo);b->created=1;
    jpeg_mem_dest(&b->cinfo,&b->data,&b->size);
    b->cinfo.image_width=s->width;
    b->cinfo.image_height=target;
    b->cinfo.input_components=s->color_space==19?3:1;
    b->cinfo.in_color_space=s->color_space==19?JCS_RGB:JCS_GRAYSCALE;
    jpeg_set_defaults(&b->cinfo);
    jpeg_set_quality(&b->cinfo,M1522_JPEG_QUALITY,TRUE);
    if(s->color_space==19&&b->cinfo.num_components==3){
        int i;
        for(i=0;i<3;i++){
            b->cinfo.comp_info[i].h_samp_factor=1;
            b->cinfo.comp_info[i].v_samp_factor=1;
        }
    }
    b->cinfo.optimize_coding=FALSE;
    b->cinfo.write_JFIF_header=TRUE;
    b->cinfo.density_unit=0;
    b->cinfo.X_density=1;b->cinfo.Y_density=1;
    jpeg_start_compress(&b->cinfo,TRUE);
    s->image_ctx=b;
    return 0;
}
static int jpeg_block_write_row(struct mb_pwg_pcl *s,const unsigned char *row){
    struct mb_jpeg_block *b=(struct mb_jpeg_block *)s->image_ctx;
    JSAMPROW scan[1];
    if(!b||b->rows_written>=b->rows_target)return -1;
    if(setjmp(b->err.jump))return -1;
    scan[0]=(JSAMPROW)row;
    if(jpeg_write_scanlines(&b->cinfo,scan,1)!=1)return -1;
    b->rows_written++;
    return 0;
}
static int jpeg_block_finish(struct mb_pwg_pcl *s,mb_pwg_write_fn fn,void *ctx){
    struct mb_jpeg_block *b=(struct mb_jpeg_block *)s->image_ctx;
    unsigned char emb[5]={0xfa,0,0,0,0};
    if(!b||b->rows_written!=b->rows_target)return -1;
    if(setjmp(b->err.jump)){jpeg_block_free(s);return -1;}
    jpeg_finish_compress(&b->cinfo);
    if(!b->data||!b->size||b->size>0xffffffffUL){jpeg_block_free(s);return -1;}
    if(pxl_u16(fn,ctx,b->start_line,109)||
       pxl_u16(fn,ctx,b->rows_target,99)||
       pxl_u8(fn,ctx,2,101)||pxl_op(fn,ctx,0xb1)){
        jpeg_block_free(s);return -1;
    }
    le32(emb+1,(uint32_t)b->size);
    if(out(fn,ctx,emb,sizeof emb)||out(fn,ctx,b->data,(size_t)b->size)){
        jpeg_block_free(s);return -1;
    }
    jpeg_block_free(s);
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
    if(!s->width||!s->height||s->width>PWG_MAX_WIDTH||s->height>PWG_MAX_HEIGHT||
       !s->bytes_per_line||s->bytes_per_line>PWG_MAX_LINE||
       !s->xdpi||s->xdpi!=s->ydpi||(s->xdpi!=300&&s->xdpi!=600)||
       s->color_order!=0)return -1;
    if(s->color_space==18&&s->bits_per_color==8&&s->bits_per_pixel==8&&s->num_colors==1)unit=1;
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
    }
    if(!s->line||((s->color_space==3&&s->bits_per_pixel==1)&&!s->mono)){
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
static int emit_black_line(struct mb_pwg_pcl *s,mb_pwg_write_fn fn,void *ctx){
    unsigned i;
    if(black_row(s))return -1;
    for(i=0;i<s->repeat_lines;i++){
        unsigned char emb[5]={0xfa,0,0,0,0};
        static const unsigned char pad[3]={0,0,0};
        size_t padded=(s->mono_cap+3u)&~3u;
        size_t padding=padded-s->mono_cap;
        if(pxl_u16(fn,ctx,s->row,109)||pxl_u16(fn,ctx,1,99)||
           pxl_u8(fn,ctx,0,101)||pxl_op(fn,ctx,0xb1))return -1;
        le32(emb+1,(uint32_t)padded);
        if(out(fn,ctx,emb,sizeof emb)||out(fn,ctx,s->mono,s->mono_cap)||
           (padding&&out(fn,ctx,pad,padding)))return -1;
        if(++s->row>s->height)return -1;
    }
    return 0;
}
static int emit_continuous_line(struct mb_pwg_pcl *s,mb_pwg_write_fn fn,void *ctx){
    unsigned i;
    const unsigned char *row=s->line;
    for(i=0;i<s->repeat_lines;i++){
        struct mb_jpeg_block *b;
        if(!s->image_ctx&&jpeg_block_start(s))return -1;
        b=(struct mb_jpeg_block *)s->image_ctx;
        if(jpeg_block_write_row(s,row))return -1;
        if(++s->row>s->height)return -1;
        if(b->rows_written==b->rows_target&&jpeg_block_finish(s,fn,ctx))return -1;
    }
    return 0;
}
static int emit_line(struct mb_pwg_pcl *s,mb_pwg_write_fn fn,void *ctx){
    int rc=(s->color_space==3&&s->bits_per_pixel==1)?
        emit_black_line(s,fn,ctx):emit_continuous_line(s,fn,ctx);
    if(rc)return rc;
    if(s->row==s->height){
        if(s->image_ctx&&jpeg_block_finish(s,fn,ctx))return -1;
        if(page_end(fn,ctx))return -1;
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
        if(ch==128)return -1;
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
