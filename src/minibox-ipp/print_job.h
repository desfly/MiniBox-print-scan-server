#ifndef MINIBOX_PRINT_JOB_H
#define MINIBOX_PRINT_JOB_H
#include <stddef.h>
typedef int (*print_sink_fn)(void *ctx,const unsigned char *buf,int len,int timeout_ms);
int minibox_print_document(print_sink_fn sink,void *ctx,const unsigned char *doc,size_t len,size_t chunk,int timeout_ms);
#endif
