#include "http_body.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

static int lower_eq(const unsigned char *p,const char *s,size_t n){
    size_t i;
    for(i=0;i<n;i++){
        unsigned char a=p[i],b=(unsigned char)s[i];
        if(a>='A'&&a<='Z')a=(unsigned char)(a+32);
        if(b>='A'&&b<='Z')b=(unsigned char)(b+32);
        if(a!=b)return 0;
    }
    return s[n]==0;
}
static int value_eq_ci(const unsigned char *p,size_t n,const char *s){
    size_t z=strlen(s),i;
    while(n&&(*p==' '||*p=='\t')){p++;n--;}
    while(n&&(p[n-1]==' '||p[n-1]=='\t'))n--;
    if(n!=z)return 0;
    for(i=0;i<n;i++){
        unsigned char a=p[i],b=(unsigned char)s[i];
        if(a>='A'&&a<='Z')a=(unsigned char)(a+32);
        if(b>='A'&&b<='Z')b=(unsigned char)(b+32);
        if(a!=b)return 0;
    }
    return 1;
}

int minibox_http_parse_body(const unsigned char *b,size_t n,
                            struct minibox_http_body *o){
    size_t i,hs=0,cl=(size_t)-1;
    int seen_cl=0,seen_te=0,chunked=0;
    if(!b||!o)return -1;
    memset(o,0,sizeof *o);
    for(i=0;i+3<n;i++){
        if(b[i]=='\r'&&b[i+1]=='\n'&&b[i+2]=='\r'&&b[i+3]=='\n'){
            hs=i+4;break;
        }
    }
    if(!hs)return 1;
    i=0;
    while(i+1<hs){
        size_t e=i;
        while(e+1<hs&&(b[e]!='\r'||b[e+1]!='\n'))e++;
        if(e==i)break;
        if(e-i>=15&&lower_eq(b+i,"Content-Length:",15)){
            const unsigned char *p=b+i+15,*end=b+e;
            char tmp[32],*q;unsigned long long v;size_t z;
            while(p<end&&(*p==' '||*p=='\t'))p++;
            z=(size_t)(end-p);
            if(!z||z>=sizeof tmp)return -2;
            memcpy(tmp,p,z);tmp[z]=0;errno=0;v=strtoull(tmp,&q,10);
            if(errno||*q||v>(unsigned long long)SIZE_MAX)return -2;
            if(seen_cl&&cl!=(size_t)v)return -2;
            cl=(size_t)v;seen_cl=1;
        } else if(e-i>=18&&lower_eq(b+i,"Transfer-Encoding:",18)){
            const unsigned char *p=b+i+18;size_t z=(size_t)((b+e)-p);
            if(seen_te)return -2;
            seen_te=1;
            if(!value_eq_ci(p,z,"chunked"))return -4;
            chunked=1;
        }
        i=e+2;
    }
    /* Reject ambiguous framing instead of permitting request smuggling. */
    if(seen_cl&&chunked)return -2;
    if(!seen_cl&&!chunked)return -3;
    o->header_bytes=hs;
    o->chunked=chunked;
    o->content_length=chunked?0:cl;
    o->buffered_body=n>hs?n-hs:0;
    if(!chunked&&o->buffered_body>cl)o->buffered_body=cl;
    return 0;
}

ssize_t minibox_recv_retry(int fd,void *buf,size_t cap){
    ssize_t n;
    do{n=recv(fd,buf,cap,0);}while(n<0&&errno==EINTR);
    return n;
}

void minibox_chunk_reader_init(struct minibox_chunk_reader *r,int fd,
                               const unsigned char *initial,size_t initial_len){
    if(!r)return;
    memset(r,0,sizeof *r);
    r->fd=fd;r->initial=initial;r->initial_len=initial_len;
}
static ssize_t source_read(struct minibox_chunk_reader *r,unsigned char *out,size_t cap){
    size_t n;
    if(r->initial_pos<r->initial_len){
        n=r->initial_len-r->initial_pos;
        if(n>cap)n=cap;
        memcpy(out,r->initial+r->initial_pos,n);
        r->initial_pos+=n;
        return (ssize_t)n;
    }
    return minibox_recv_retry(r->fd,out,cap);
}
static int source_byte(struct minibox_chunk_reader *r,unsigned char *out){
    return source_read(r,out,1)==1?0:-1;
}
static int read_line(struct minibox_chunk_reader *r,char *line,size_t cap){
    size_t n=0;unsigned char c,prev=0;
    if(!cap)return -1;
    for(;;){
        if(source_byte(r,&c))return -1;
        if(prev=='\r'&&c=='\n'){
            if(n)--n;
            line[n]=0;return 0;
        }
        if(n+1>=cap)return -1;
        line[n++]=(char)c;prev=c;
    }
}
static int consume_crlf(struct minibox_chunk_reader *r){
    unsigned char a,b;
    if(source_byte(r,&a)||source_byte(r,&b))return -1;
    return a=='\r'&&b=='\n'?0:-1;
}
static int next_chunk(struct minibox_chunk_reader *r){
    char line[96],*end;unsigned long long v;unsigned trailers=0;
    if(r->need_chunk_crlf){
        if(consume_crlf(r))return -1;
        r->need_chunk_crlf=0;
    }
    if(read_line(r,line,sizeof line))return -1;
    errno=0;v=strtoull(line,&end,16);
    if(errno||end==line||(*end&&*end!=';')||v>(unsigned long long)SIZE_MAX)return -1;
    if(!v){
        /* Consume bounded trailer fields through the final empty line. */
        do{
            if(++trailers>64||read_line(r,line,sizeof line))return -1;
        }while(line[0]);
        r->done=1;return 0;
    }
    r->chunk_left=(size_t)v;
    return 1;
}
ssize_t minibox_chunk_read(struct minibox_chunk_reader *r,
                           unsigned char *out,size_t cap){
    size_t total=0;
    if(!r||!out||!cap)return -1;
    if(r->done)return 0;
    while(total<cap&&!r->done){
        ssize_t n;size_t want;
        if(!r->chunk_left){
            int rc=next_chunk(r);
            if(rc<0)return -1;
            if(!rc)break;
        }
        want=cap-total;
        if(want>r->chunk_left)want=r->chunk_left;
        n=source_read(r,out+total,want);
        if(n<=0)return -1;
        total+=(size_t)n;
        r->chunk_left-=(size_t)n;
        if(!r->chunk_left)r->need_chunk_crlf=1;
    }
    return (ssize_t)total;
}
