#ifndef MINIBOX_PRINTERD_HTTP_BODY_H
#define MINIBOX_PRINTERD_HTTP_BODY_H
#include <stddef.h>
#include <sys/types.h>
struct minibox_http_body {
    size_t header_bytes;
    size_t content_length;
    size_t buffered_body;
};
int minibox_http_parse_body(const unsigned char *buf,size_t n,struct minibox_http_body *out);
ssize_t minibox_recv_retry(int fd,void *buf,size_t cap);
#endif
