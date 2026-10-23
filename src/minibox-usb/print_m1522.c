#include "print_m1522.h"
#include "../minibox-ipp/print_job.h"
#include <string.h>
#include <stdio.h>

static int m1522_sink(void *ctx,const unsigned char *buf,int len,int timeout_ms){
    struct m1522_print_session *s=ctx;
    int n=m1522_bulk_write(&s->usb,buf,len,timeout_ms);
    s->usb_calls++;
    if(n>0) s->usb_bytes+=(size_t)n;
    else if(!s->first_error) s->first_error=n?n:-1;
    return n;
}
int m1522_print_open(struct m1522_print_session *s){int r;if(!s)return-1;memset(s,0,sizeof *s);r=m1522_open(&s->usb);if(r)return r;s->opened=1;return 0;}
int m1522_print_write(struct m1522_print_session *s,const unsigned char *buf,size_t len){if(!s||!s->opened||(!buf&&len))return-1;if(!len)return 0;return minibox_print_document(m1522_sink,s,buf,len,16384,5000);}
void m1522_print_close(struct m1522_print_session *s){if(!s)return;if(s->opened){
    fprintf(stderr,"minibox-usb: bulk_out_bytes=%zu calls=%u first_error=%d\n",
            s->usb_bytes,s->usb_calls,s->first_error);
    m1522_close(&s->usb);
}
memset(s,0,sizeof *s);}
int m1522_print_document(const unsigned char *doc,size_t len){struct m1522_print_session s;int r;if(!doc||!len)return-1;r=m1522_print_open(&s);if(r)return r;r=m1522_print_write(&s,doc,len);m1522_print_close(&s);return r;}
