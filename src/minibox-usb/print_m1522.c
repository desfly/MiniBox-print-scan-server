#include "print_m1522.h"
#include "../minibox-ipp/print_job.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>

/* FNV-1a diagnostics only: no buffering, no change to USB payload. */
#define M1522_FNV_OFFSET UINT64_C(14695981039346656037)
#define M1522_FNV_PRIME UINT64_C(1099511628211)
static uint64_t m1522_hash_bytes(uint64_t hash,const unsigned char *buf,size_t len){
    for(size_t i=0;i<len;i++){hash^=buf[i];hash*=M1522_FNV_PRIME;}
    return hash;
}
static int m1522_sink(void *ctx,const unsigned char *buf,int len,int timeout_ms){
    struct m1522_print_session *s=ctx;
    int n=m1522_bulk_write(&s->usb,buf,len,timeout_ms);
    s->usb_calls++;
    if(n>0){s->usb_bytes+=(size_t)n;s->accepted_hash=m1522_hash_bytes(s->accepted_hash,buf,(size_t)n);}
    else if(!s->first_error) s->first_error=n?n:-1;
    return n;
}
int m1522_print_open(struct m1522_print_session *s){int r;if(!s)return-1;memset(s,0,sizeof *s);r=m1522_open(&s->usb);if(r)return r;s->opened=1;s->submitted_hash=M1522_FNV_OFFSET;s->accepted_hash=M1522_FNV_OFFSET;return 0;}
int m1522_print_write(struct m1522_print_session *s,const unsigned char *buf,size_t len){if(!s||!s->opened||(!buf&&len))return-1;if(!len)return 0;
if(len>SIZE_MAX-s->requested_bytes)return -2;
s->requested_bytes+=len;
s->submitted_hash=m1522_hash_bytes(s->submitted_hash,buf,len);
return minibox_print_document(m1522_sink,s,buf,len,16384,5000);}
void m1522_print_close(struct m1522_print_session *s){if(!s)return;if(s->opened){
    fprintf(stderr,"minibox-usb: requested_bytes=%zu bulk_out_bytes=%zu calls=%u first_error=%d complete=%s\n",
            s->requested_bytes,s->usb_bytes,s->usb_calls,s->first_error,
            s->requested_bytes==s->usb_bytes && !s->first_error?"yes":"no");
    fprintf(stderr,"minibox-usb: submitted_fnv64=%016llx accepted_fnv64=%016llx match=%s\n",
            (unsigned long long)s->submitted_hash,(unsigned long long)s->accepted_hash,
            s->requested_bytes==s->usb_bytes && s->submitted_hash==s->accepted_hash && !s->first_error?"yes":"no");
    m1522_close(&s->usb);
}
memset(s,0,sizeof *s);}
int m1522_print_document(const unsigned char *doc,size_t len){struct m1522_print_session s;int r;if(!doc||!len)return-1;r=m1522_print_open(&s);if(r)return r;r=m1522_print_write(&s,doc,len);m1522_print_close(&s);return r;}
