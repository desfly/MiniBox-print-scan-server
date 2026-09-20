#include "print_m1522.h"
#include "../minibox-ipp/print_job.h"
#include <string.h>

static int m1522_sink(void *ctx,const unsigned char *buf,int len,int timeout_ms){return m1522_bulk_write((struct m1522_handle *)ctx,buf,len,timeout_ms);}
int m1522_print_open(struct m1522_print_session *s){int r;if(!s)return-1;memset(s,0,sizeof *s);r=m1522_open(&s->usb);if(r)return r;s->opened=1;return 0;}
int m1522_print_write(struct m1522_print_session *s,const unsigned char *buf,size_t len){if(!s||!s->opened||(!buf&&len))return-1;if(!len)return 0;return minibox_print_document(m1522_sink,&s->usb,buf,len,16384,5000);}
void m1522_print_close(struct m1522_print_session *s){if(!s)return;if(s->opened)m1522_close(&s->usb);memset(s,0,sizeof *s);}
int m1522_print_document(const unsigned char *doc,size_t len){struct m1522_print_session s;int r;if(!doc||!len)return-1;r=m1522_print_open(&s);if(r)return r;r=m1522_print_write(&s,doc,len);m1522_print_close(&s);return r;}
