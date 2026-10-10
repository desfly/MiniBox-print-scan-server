#ifndef MINIBOX_PRINTERD_HTTP_BODY_H
#define MINIBOX_PRINTERD_HTTP_BODY_H
#include <stddef.h>
#include <sys/types.h>

struct minibox_http_body {
    size_t header_bytes;
    size_t content_length;
    size_t buffered_body;
    int chunked;
};

struct minibox_chunk_reader {
    int fd;
    const unsigned char *initial;
    size_t initial_len;
    size_t initial_pos;
    size_t chunk_left;
    int need_chunk_crlf;
    int done;
};

int minibox_http_parse_body(const unsigned char *buf,size_t n,
                            struct minibox_http_body *out);
ssize_t minibox_recv_retry(int fd,void *buf,size_t cap);
void minibox_chunk_reader_init(struct minibox_chunk_reader *r,int fd,
                               const unsigned char *initial,size_t initial_len);
ssize_t minibox_chunk_read(struct minibox_chunk_reader *r,
                           unsigned char *out,size_t cap);

#endif
