#include "pwg_to_pcl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PWG_HEADER_SIZE 1796u
#define PWG_MAX_WIDTH 7200u
#define PWG_MAX_HEIGHT 10800u
#define PWG_MAX_LINE (64u*1024u)

static uint32_t be32(const unsigned char *p){
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}
static int out(mb_pwg_write_fn fn,void *ctx,const void *buf,size_t len){
    return fn&&buf&&(!len||fn(ctx,(const unsigned char *)buf,len)==0)?0:-1;
}
static void free_page(struct mb_pwg_pcl *s){
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
static void le16(unsigned char *p,unsigned v){p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);}
static void le32(unsigned char *p,uint32_t v){p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);p[2]=(unsigned char)(v>>16);p[3]=(unsigned char)(v>>24);}
static int pxl_u8(mb_pwg_write_fn fn,void *ctx,unsigned v,unsigned attr){
    unsigned char b[4]={0xc0,(unsigned char)v,0xf8,(unsigned char)attr};return out(fn,ctx,b,sizeof b);
}
static int pxl_u16(mb_pwg_write_fn fn,void *ctx,unsigned v,unsigned attr){
    unsigned char b[5]={0xc1,0,0,0xf8,(unsigned char)attr};le16(b+1,v);return out(fn,ctx,b,sizeof b);
}
static int pxl_xy16(mb_pwg_write_fn fn,void *ctx,unsigned x,unsigned y,unsigned attr){
    unsigned char b[7]={0xd1,0,0,0,0,0xf8,(unsigned char)attr};le16(b+1,x);le16(b+3,y);return out(fn,ctx,b,sizeof b);
}
static int pxl_op(mb_pwg_write_fn fn,void *ctx,unsigned op){unsigned char b=(unsigned char)op;return out(fn,ctx,&b,1);}
static unsigned pxl_media(const struct mb_pwg_pcl *s){
    unsigned w=s->page_width_points,h=s->page_height_points;
    if((w>=590&&w<=600&&h>=837&&h<=847)||(h>=590&&h<=600&&w>=837&&w<=847))return 2; /* A4 */
    if((w>=607&&w<=617&&h>=787&&h<=797)||(h>=607&&h<=617&&w>=787&&w<=797))return 0; /* Letter */
    if((w>=607&&w<=617&&h>=1003&&h<=1013)||(h>=607&&h<=617&&w>=1003&&w<=1013))return 1; /* Legal */
    return 2;
}
static int job_start(struct mb_pwg_pcl *s,mb_pwg_write_fn fn,void *ctx){
    char pjl[192];int n;static const char pxl[]=") HP-PCL XL;2;1;\n";
    if(s->job_started)return 0;
    n=snprintf(pjl,sizeof pjl,"\033%%-12345X@PJL JOB NAME=\"MiniBox PWG\"\r\n@PJL SET RESOLUTION=%u\r\n@PJL SET BITSPERPIXEL=1\r\n@PJL SET GRAYSCALE=BLACKONLY\r\n@PJL ENTER LANGUAGE=PCLXL\r\n",s->xdpi);
    if(n<0||(size_t)n>=sizeof pjl||out(fn,ctx,pjl,(size_t)n)||out(fn,ctx,pxl,sizeof pxl-1))return -1;
    if(pxl_xy16(fn,ctx,s->xdpi,s->ydpi,137)||pxl_u8(fn,ctx,0,134)||pxl_u8(fn,ctx,3,143)||pxl_op(fn,ctx,0x41)||
       pxl_u8(fn,ctx,0,136)||pxl_u8(fn,ctx,1,130)||pxl_op(fn,ctx,0x48))return -1;
    s->job_started=1;return 0;
}
static int page_start(struct mb_pwg_pcl *s,mb_pwg_write_fn fn,void *ctx){
    if(job_start(s,fn,ctx)||
       pxl_u8(fn,ctx,1,38)||pxl_u8(fn,ctx,0,52)||pxl_u8(fn,ctx,0,40)||pxl_u8(fn,ctx,pxl_media(s),37)||pxl_op(fn,ctx,0x43)||
       pxl_u8(fn,ctx,s->color_space==19?2:1,3)|| /* ColorSpace: srgb_8 -> eRGB, mono/sgray -> eGray */
       pxl_op(fn,ctx,0x6a)||
       pxl_xy16(fn,ctx,0,0,76)||pxl_op(fn,ctx,0x6b)||
       pxl_u8(fn,ctx,(s->color_space==3&&s->bits_per_pixel==1)?0:2,98)|| /* ColorDepth: e1Bit/e8Bit */
       pxl_u8(fn,ctx,0,100)|| /* ColorMapping: eDirectPixel */
       pxl_u16(fn,ctx,s->width,108)||pxl_u16(fn,ctx,s->height,107)||
       pxl_xy16(fn,ctx,s->width,s->height,103)||pxl_op(fn,ctx,0xb0))return -1;
    return 0;
}
static int page_end(mb_pwg_write_fn fn,void *ctx){
    return pxl_op(fn,ctx,0xb2)||pxl_op(fn,ctx,0x44)?-1:0;
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
    s->line=malloc(s->bytes_per_line);
    if(s->color_space==3&&s->bits_per_pixel==1)s->mono_cap=(s->width+7u)/8u;
    else if(s->color_space==19)s->mono_cap=(size_t)s->width*3u;
    else s->mono_cap=s->width;
    s->mono=malloc(s->mono_cap);
    if(!s->line||!s->mono){free_page(s);return -3;}
    s->line_cap=s->bytes_per_line;s->line_used=0;s->row=0;s->color_value_bytes=unit;
    if(page_start(s,fn,ctx)){free_page(s);return -4;}
    s->pages++;return 0;
}
static int mono_row(struct mb_pwg_pcl *s){
    unsigned x;
    if(s->color_space==3&&s->bits_per_pixel==1){
        /*
         * PWG black_1 has 1 bits for black; the M1522 PCL XL
         * eGray/e1Bit DirectPixel path proven on hardware uses 0 for black.
         */
        for(x=0;x<s->mono_cap;x++)s->mono[x]=(unsigned char)~s->line[x];
        return 0;
    }
    if(s->color_space==18){
        /* PWG sgray_8 maps directly to PCL XL eGray/e8Bit DirectPixel. */
        memcpy(s->mono,s->line,s->width);
        return 0;
    }
    if(s->color_space==19){
        /*
         * PWG srgb_8 maps to PCL XL eRGB/e8Bit DirectPixel.
         * Preserve all three 8-bit components and let the printer's
         * documented PCL XL raster pipeline perform monochrome rendering.
         */
        memcpy(s->mono,s->line,(size_t)s->width*3u);
        return 0;
    }
    return -1;
}
static int emit_line(struct mb_pwg_pcl *s,mb_pwg_write_fn fn,void *ctx){
    unsigned i;
    if(mono_row(s))return -1;
    for(i=0;i<s->repeat_lines;i++){
        unsigned char emb[5]={0xfa,0,0,0,0};
        if(pxl_u16(fn,ctx,s->row,109)||pxl_u16(fn,ctx,1,99)||pxl_u8(fn,ctx,0,101)||pxl_op(fn,ctx,0xb1))return -1;
        {
            static const unsigned char pad[3]={0,0,0};
            size_t padded=(s->mono_cap+3u)&~3u;
            size_t padding=padded-s->mono_cap;
            le32(emb+1,(uint32_t)padded);
            if(out(fn,ctx,emb,sizeof emb)||out(fn,ctx,s->mono,s->mono_cap)||
               (padding&&out(fn,ctx,pad,padding)))return -1;
        }
        if(++s->row>s->height)return -1;
    }
    if(s->row==s->height){
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
    static const unsigned char trailer[] = {0x49,0x42};
    static const char pjl_end[] = "\033%-12345X@PJL EOJ NAME=\"MiniBox PWG\"\r\n\033%-12345X";
    if(!s||!fn||s->failed)return -1;
    if(!s->pages||s->phase!=MB_PWG_HEADER||s->header_used!=0)return -2;
    if(out(fn,ctx,trailer,sizeof trailer)||out(fn,ctx,pjl_end,sizeof pjl_end-1)){s->failed=-3;return -3;}
    return 0;
}
