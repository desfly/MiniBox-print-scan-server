#include "../src/minibox-ipp/print_job.h"
#include <assert.h>
#include <string.h>
struct sink{unsigned char b[512];size_t n;};
static int write_sink(void*c,const unsigned char*b,int n,int t){
    struct sink*s=c;(void)t;
    assert(s->n+(size_t)n<=sizeof s->b);
    memcpy(s->b+s->n,b,(size_t)n);s->n+=(size_t)n;return n;
}
static int short_write_sink(void*c,const unsigned char*b,int n,int t){
    return write_sink(c,b,n>3?3:n,t);
}
int main(void){
    static const unsigned char pcl[]={0x1b,'E','H','e','l','l','o',0x0c,0x1b,'E'};
    /* Original HP UPD PCL XL syntax: MiniBox is a byte-preserving bridge,
     * not a renderer, when the caller submits a complete raw job. */
    static const unsigned char hp_pclxl[]={
        0x1b,'%','-','1','2','3','4','5','X',
        '@','P','J','L',' ','E','N','T','E','R',' ','L','A','N','G','U','A','G','E','=','P','C','L','X','L','\n',
        ')',' ','H','P','-','P','C','L',' ','X','L',';','2',';','1',';','C','o','m','m','e','n','t','\n',
        0xd1,0x58,0x02,0x58,0x02,0xf8,0x89,0xc0,0x00,0xf8,0x86,0xc0,0x03,0xf8,0x8f,0x41,
        0x42,0x1b,'%','-','1','2','3','4','5','X',
        '@','P','J','L',' ','E','O','J','\n',0x1b,'%','-','1','2','3','4','5','X'
    };
    struct sink s={{0},0};
    assert(minibox_print_document(write_sink,&s,pcl,sizeof pcl,4,1000)==0);
    assert(s.n==sizeof pcl&&!memcmp(s.b,pcl,sizeof pcl));
    memset(&s,0,sizeof s);
    assert(minibox_print_document(short_write_sink,&s,hp_pclxl,sizeof hp_pclxl,11,1000)==0);
    assert(s.n==sizeof hp_pclxl&&!memcmp(s.b,hp_pclxl,sizeof hp_pclxl));
    return 0;
}
