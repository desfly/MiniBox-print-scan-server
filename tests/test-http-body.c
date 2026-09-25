#include <assert.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "../src/minibox-printerd/http_body.h"

int main(void)
{
    struct minibox_http_body body;
    const char ok[] = "POST / HTTP/1.1\r\nContent-Length: 9\r\ncontent-length: 9\r\n\r\n123";
    const char conflict[] = "POST / HTTP/1.1\r\nContent-Length: 9\r\nContent-Length: 8\r\n\r\n";
    const char invalid[] = "POST / HTTP/1.1\r\nContent-Length: 9x\r\n\r\n";
    const char chunked[] = "POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n4\r\nWi";
    const char ambiguous[] = "POST / HTTP/1.1\r\nContent-Length: 9\r\nTransfer-Encoding: chunked\r\n\r\n";
    const char unsupported_te[] = "POST / HTTP/1.1\r\nTransfer-Encoding: gzip\r\n\r\n";
    size_t hs;
    int sv[2];
    struct minibox_chunk_reader cr;
    unsigned char decoded[32];
    size_t used=0;
    ssize_t n;

    assert(minibox_http_parse_body((const unsigned char *)ok, strlen(ok), &body) == 0);
    assert(body.content_length == 9);
    assert(body.buffered_body == 3);
    assert(body.chunked == 0);
    assert(minibox_http_parse_body((const unsigned char *)conflict, strlen(conflict), &body) != 0);
    assert(minibox_http_parse_body((const unsigned char *)invalid, strlen(invalid), &body) != 0);
    assert(minibox_http_parse_body((const unsigned char *)ambiguous, strlen(ambiguous), &body) != 0);
    assert(minibox_http_parse_body((const unsigned char *)unsupported_te, strlen(unsupported_te), &body) != 0);

    assert(minibox_http_parse_body((const unsigned char *)chunked, strlen(chunked), &body) == 0);
    assert(body.chunked == 1);
    hs=body.header_bytes;
    assert(body.buffered_body == strlen(chunked)-hs);
    assert(socketpair(AF_UNIX,SOCK_STREAM,0,sv)==0);
    assert(write(sv[1],"ki\r\n5\r\npedia\r\n0\r\nTrailer: ok\r\n\r\n",35)==35);
    close(sv[1]);
    minibox_chunk_reader_init(&cr,sv[0],(const unsigned char *)chunked+hs,body.buffered_body);
    for(;;){
        n=minibox_chunk_read(&cr,decoded+used,3);
        assert(n>=0);
        if(!n)break;
        used+=(size_t)n;
        assert(used<=sizeof decoded);
    }
    close(sv[0]);
    assert(used==9);
    assert(!memcmp(decoded,"Wikipedia",9));
    return 0;
}
