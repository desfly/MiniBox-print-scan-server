#include "wsd_mtom.h"
#include <string.h>
#include <stdio.h>
#define MB_MIME_LINE_MAX 1024
static const unsigned char *locate(const unsigned char *p,size_t n,const char *s,size_t sl){
    size_t i;if(!sl||sl>n)return 0;
    for(i=0;i<=n-sl;i++)if(p[i]==(unsigned char)s[0]&&!memcmp(p+i,s,sl))return p+i;
    return 0;
}
int mb_wsd_mtom_attachment(const unsigned char *buf,size_t len,
                           const char *boundary,const char *content_id,
                           const unsigned char **out,size_t *out_len){
    char marker[160],cid[256];size_t bl,ml,cl,at=0;int found=0;
    if(!buf||!boundary||!content_id||!out||!out_len)return -1;
    *out=0;*out_len=0;
    bl=strlen(boundary);cl=strlen(content_id);
    if(!bl||bl>100||cl>200||strchr(boundary,'\r')||strchr(boundary,'\n'))return -1;
    if(snprintf(marker,sizeof marker,"--%s",boundary)<0)return -1;
    if(snprintf(cid,sizeof cid,"Content-ID: <%s>",content_id)<0)return -1;
    ml=strlen(marker);
    while(at<len){
        const unsigned char *m=locate(buf+at,len-at,marker,ml),*p,*hdr_end,*next;
        size_t pos,hlen,remain;
        if(!m)return found?0:-2;
        pos=(size_t)(m-buf);
        if(pos && (pos<2||buf[pos-2]!='\r'||buf[pos-1]!='\n')){at=pos+ml;continue;}
        p=m+ml;
        if((size_t)(buf+len-p)>=2&&p[0]=='-'&&p[1]=='-')return found?0:-2;
        if((size_t)(buf+len-p)<2||p[0]!='\r'||p[1]!='\n')return -3;
        p+=2;remain=(size_t)(buf+len-p);
        hdr_end=locate(p,remain,"\r\n\r\n",4);
        if(!hdr_end)return -3;
        hlen=(size_t)(hdr_end-p);
        if(hlen>8192)return -3;
        p=hdr_end+4;
        /* A MIME part is selected only by its explicit Content-ID header. */
        {
            const unsigned char *line=m+ml+2;
            int matches=0;
            while(line<hdr_end){
                const unsigned char *e=locate(line,(size_t)(hdr_end-line),"\r\n",2);
                size_t ll=e?(size_t)(e-line):(size_t)(hdr_end-line);
                if(ll>MB_MIME_LINE_MAX)return -3;
                if(ll==strlen(cid)&&!memcmp(line,cid,ll))matches=1;
                line=e?e+2:hdr_end;
            }
            if(!matches){at=(size_t)(p-buf);continue;}
        }
        {
            char delim[164];size_t dl;
            snprintf(delim,sizeof delim,"\r\n--%s",boundary);dl=strlen(delim);
            next=locate(p,(size_t)(buf+len-p),delim,dl);
            if(!next)return -3;
            if(found)return -4;
            *out=p;*out_len=(size_t)(next-p);found=1;
            at=(size_t)(next-buf)+2;
        }
    }
    return found?0:-2;
}
