#include "soapht_transport.h"
int soapht_open(struct soapht_session*s,const struct soapht_io*io,void*ctx){if(!s||!io||!io->open||!io->write||!io->read||!io->close)return-1;s->io=io;s->ctx=ctx;s->opened=0;if(io->open(ctx,MINIBOX_SOAPHT_CHANNEL))return-2;s->opened=1;return 0;}
int soapht_write(struct soapht_session*s,const unsigned char*b,size_t n){if(!s||!s->opened||(!b&&n))return-1;return s->io->write(s->ctx,b,n);}
int soapht_read(struct soapht_session*s,unsigned char*b,size_t cap,size_t*got){if(!s||!s->opened||!b||!cap||!got)return-1;*got=0;return s->io->read(s->ctx,b,cap,got);}
void soapht_close(struct soapht_session*s){if(!s||!s->opened)return;s->io->close(s->ctx);s->opened=0;}
