#define _POSIX_C_SOURCE 200809L
#include "print_m1522.h"
#include "../minibox-ipp/print_job.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <libusb-1.0/libusb.h>
static unsigned long long print_clock_ms(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return 0;return (unsigned long long)t.tv_sec*1000ULL+(unsigned long long)t.tv_nsec/1000000ULL;}


static int m1522_sink(void *ctx,const unsigned char *buf,int len,int timeout_ms){
    struct m1522_print_session *s=ctx;
    unsigned long long begun=print_clock_ms();
    int n=m1522_bulk_write(&s->usb,buf,len,timeout_ms);
    unsigned long long elapsed=print_clock_ms()-begun;
    if(n<0)fprintf(stderr,"minibox-usb: bulk error=%d (%s) request=%d timeout_ms=%d elapsed_ms=%llu accepted_total=%zu\n",n,libusb_error_name(n),len,timeout_ms,elapsed,s->usb_bytes);
    else if(elapsed>=1000)fprintf(stderr,"minibox-usb: bulk slow request=%d returned=%d elapsed_ms=%llu accepted_total=%zu\n",len,n,elapsed,s->usb_bytes);
    s->usb_calls++;
    if(n>0){s->usb_bytes+=(size_t)n;}
    else if(!s->first_error) s->first_error=n?n:-1;
    return n;
}
int m1522_print_open(struct m1522_print_session *s){int r;if(!s)return-1;memset(s,0,sizeof *s);r=m1522_open(&s->usb);if(r)return r;s->opened=1;return 0;}
int m1522_print_write(struct m1522_print_session *s,const unsigned char *buf,size_t len){if(!s||!s->opened||(!buf&&len))return-1;if(!len)return 0;
if(len>SIZE_MAX-s->requested_bytes)return -2;
s->requested_bytes+=len;
/* Printer back-pressure exceeded 5s in the observed USB trace. Keep
 * a bounded wait; the stream layer advances only by reported progress. */
return minibox_print_document(m1522_sink,s,buf,len,16384,30000);}
void m1522_print_close(struct m1522_print_session *s){if(!s)return;if(s->opened){
    fprintf(stderr,"minibox-usb: requested_bytes=%zu bulk_out_bytes=%zu calls=%u first_error=%d complete=%s\n",
            s->requested_bytes,s->usb_bytes,s->usb_calls,s->first_error,
            s->requested_bytes==s->usb_bytes && !s->first_error?"yes":"no");
    m1522_close(&s->usb);
}
memset(s,0,sizeof *s);}
int m1522_print_document(const unsigned char *doc,size_t len){struct m1522_print_session s;int r;if(!doc||!len)return-1;r=m1522_print_open(&s);if(r)return r;r=m1522_print_write(&s,doc,len);m1522_print_close(&s);return r;}
