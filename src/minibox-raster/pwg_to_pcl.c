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
static int cmd(mb_pwg_write_fn fn,void *ctx,const char *fmt,unsigned value){
    char b[64];int n=snprintf(b,sizeof b,fmt,value);
    if(n<0||(size_t)n>=sizeof b)return -1;
    return out(fn,ctx,b,(size_t)n);
}
static int paper_cmd(struct mb_pwg_pcl *s,mb_pwg_write_fn fn,void *ctx){
    unsigned w=s->page_width_points,h=s->page_height_points;
    unsigned code=0;
    if((w>=590&&w<=600&&h>=837&&h<=847)||(h>=590&&h<=600&&w>=837&&w<=847))code=26; /* A4 */
    else if((w>=607&&w<=617&&h>=787&&h<=797)||(h>=607&&h<=617&&w>=787&&w<=797))code=2; /* Letter */
    else if((w>=607&&w<=617&&h>=1003&&h<=1013)||(h>=607&&h<=617&&w>=1003&&w<=1013))code=3; /* Legal */
    return code?cmd(fn,ctx,"\033&l%uA",code):0;
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
static int job_start(struct mb_pwg_pcl *s,mb_pwg_write_fn fn,void *ctx){
    static const char pjl[]="\033%-12345X@PJL JOB NAME=\"MiniBox PWG\"\r\n@PJL ENTER LANGUAGE=PCL\r\n\033E";
    if(s->job_started)return 0;
    if(out(fn,ctx,pjl,sizeof pjl-1))return -1;
    s->job_started=1;return 0;
}
static int page_start(struct mb_pwg_pcl *s,mb_pwg_write_fn fn,void *ctx){
    if(job_start(s,fn,ctx)||paper_cmd(s,fn,ctx)||
       out(fn,ctx,"\033&l0O",5)||
       cmd(fn,ctx,"\033*t%uR",s->xdpi)||
       cmd(fn,ctx,"\033*r%uS",s->width)||
       cmd(fn,ctx,"\033*r%uT",s->height)||
       out(fn,ctx,"\033*b0M\033*r1A",10))return -1;
    return 0;
}
static int page_end(mb_pwg_write_fn fn,void *ctx){
    static const unsigned char end[]={'\033','*','r','B','\f'};
    return out(fn,ctx,end,sizeof end);
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
    s->mono_cap=(s->width+7u)/8u;s->mono=malloc(s->mono_cap);
    if(!s->line||!s->mono){free_page(s);return -3;}
    s->line_cap=s->bytes_per_line;s->line_used=0;s->row=0;s->color_value_bytes=unit;
    if(page_start(s,fn,ctx)){free_page(s);return -4;}
    s->pages++;return 0;
}
static int mono_row(struct mb_pwg_pcl *s){
    unsigned x;
    memset(s->mono,0,s->mono_cap);
    if(s->color_space==3&&s->bits_per_pixel==1){
        memcpy(s->mono,s->line,s->mono_cap);return 0;
    }
    for(x=0;x<s->width;x++){
        int black=0;
        if(s->color_space==18){
            black=s->line[x]<128;
        }else if(s->color_space==19){
            const unsigned char *p=s->line+x*3u;
            unsigned y=(77u*p[0]+150u*p[1]+29u*p[2])>>8;
            black=y<128;
        }else return -1;
        if(black)s->mono[x>>3]|=(unsigned char)(0x80u>>(x&7));
    }
    return 0;
}
static int emit_line(struct mb_pwg_pcl *s,mb_pwg_write_fn fn,void *ctx){
    unsigned i;
    if(mono_row(s))return -1;
    for(i=0;i<s->repeat_lines;i++){
        if(cmd(fn,ctx,"\033*b%uW",(unsigned)s->mono_cap)||
           out(fn,ctx,s->mono,s->mono_cap))return -1;
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
    static const char trailer[]="\033E\033%-12345X";
    if(!s||!fn||s->failed)return -1;
    if(!s->pages||s->phase!=MB_PWG_HEADER||s->header_used!=0)return -2;
    if(out(fn,ctx,trailer,sizeof trailer-1)){s->failed=-3;return -3;}
    return 0;
}
