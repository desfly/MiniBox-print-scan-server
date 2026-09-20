#ifndef MINIBOX_USB_STREAM_H
#define MINIBOX_USB_STREAM_H
#include <stddef.h>
typedef int (*minibox_write_fn)(void *ctx,const unsigned char *buf,int len,int timeout_ms);
int minibox_stream_write(minibox_write_fn fn,void *ctx,const unsigned char *data,size_t len,size_t chunk,int timeout_ms);
#endif
