#ifndef MINIBOX_SOAPHT_TRANSPORT_H
#define MINIBOX_SOAPHT_TRANSPORT_H
#include <stddef.h>
#define MINIBOX_SOAPHT_CHANNEL "HP-SOAP-SCAN"
struct soapht_io {
    int (*open)(void *ctx,const char *channel);
    int (*write)(void *ctx,const unsigned char *buf,size_t len);
    int (*read)(void *ctx,unsigned char *buf,size_t cap,size_t *got);
    void (*close)(void *ctx);
};
struct soapht_session { const struct soapht_io *io; void *ctx; int opened; };
int soapht_open(struct soapht_session *s,const struct soapht_io *io,void *ctx);
int soapht_write(struct soapht_session *s,const unsigned char *buf,size_t len);
int soapht_read(struct soapht_session *s,unsigned char *buf,size_t cap,size_t *got);
void soapht_close(struct soapht_session *s);
#endif
