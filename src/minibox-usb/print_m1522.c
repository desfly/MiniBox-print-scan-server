#include "print_m1522.h"
#include <stdlib.h>
#include <string.h>

#define M1522_USB_TIMEOUT_MS 5000

static int flush_out(struct m1522_print_session *s)
{
    int w;
    if(!s||!s->opened||s->failed)return -1;
    if(!s->out_used)return 0;
    if(s->out_used>0x7fffffffU){s->failed=-2;return s->failed;}

    /*
     * M1522-PRINT-REAL.pcap: HP UPD sends the print stream as a 262144-byte
     * bulk OUT URB, followed by the final shorter URB.  Keep PCL XL byte
     * content unchanged; only reproduce that captured USB transfer framing.
     */
    w=m1522_bulk_write(&s->usb,s->out_buf,(int)s->out_used,M1522_USB_TIMEOUT_MS);
    if(w!=(int)s->out_used){s->failed=(w<0?w:-3);return s->failed;}
    s->out_used=0;
    return 0;
}

int m1522_print_open(struct m1522_print_session *s)
{
    int r;
    if(!s)return -1;
    memset(s,0,sizeof *s);
    r=m1522_open(&s->usb);
    if(r)return r;
    s->out_buf=(unsigned char *)malloc(M1522_CAPTURED_USB_OUT_URB);
    if(!s->out_buf){m1522_close(&s->usb);memset(s,0,sizeof *s);return -6;}
    s->opened=1;
    return 0;
}

int m1522_print_write(struct m1522_print_session *s,const unsigned char *buf,size_t len)
{
    if(!s||!s->opened||s->failed||(!buf&&len))return -1;
    while(len){
        size_t room=M1522_CAPTURED_USB_OUT_URB-s->out_used;
        size_t take=len<room?len:room;
        memcpy(s->out_buf+s->out_used,buf,take);
        s->out_used+=take;
        buf+=take;
        len-=take;
        if(s->out_used==M1522_CAPTURED_USB_OUT_URB&&flush_out(s))return s->failed;
    }
    return 0;
}

int m1522_print_finish(struct m1522_print_session *s)
{
    if(!s||!s->opened||s->failed)return -1;
    return flush_out(s);
}

void m1522_print_close(struct m1522_print_session *s)
{
    if(!s)return;
    free(s->out_buf);
    s->out_buf=0;
    if(s->opened)m1522_close(&s->usb);
    memset(s,0,sizeof *s);
}

int m1522_print_document(const unsigned char *doc,size_t len)
{
    struct m1522_print_session s;
    int r;
    if(!doc||!len)return -1;
    r=m1522_print_open(&s);
    if(r)return r;
    r=m1522_print_write(&s,doc,len);
    if(!r)r=m1522_print_finish(&s);
    m1522_print_close(&s);
    return r;
}
