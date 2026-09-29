#include <assert.h>
#include <string.h>
#include "../src/minibox-printerd/http_body.h"

int main(void)
{
    struct minibox_http_body body;
    const char ok[] = "POST / HTTP/1.1\r\nContent-Length: 9\r\ncontent-length: 9\r\n\r\n123";
    const char conflict[] = "POST / HTTP/1.1\r\nContent-Length: 9\r\nContent-Length: 8\r\n\r\n";
    const char invalid[] = "POST / HTTP/1.1\r\nContent-Length: 9x\r\n\r\n";
    const char chunked[] = "POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n4\r\ntest\r\n0\r\n\r\n";
    const char both[] = "POST / HTTP/1.1\r\nContent-Length: 4\r\nTransfer-Encoding: chunked\r\n\r\n";

    assert(minibox_http_parse_body((const unsigned char *)ok, strlen(ok), &body) == 0);
    assert(body.content_length == 9);
    assert(body.buffered_body == 3);
    assert(!body.chunked);
    assert(minibox_http_parse_body((const unsigned char *)chunked, strlen(chunked), &body) == 0);
    assert(body.chunked);
    assert(body.buffered_body > 0);
    assert(minibox_http_parse_body((const unsigned char *)conflict, strlen(conflict), &body) != 0);
    assert(minibox_http_parse_body((const unsigned char *)invalid, strlen(invalid), &body) != 0);
    assert(minibox_http_parse_body((const unsigned char *)both, strlen(both), &body) != 0);
    return 0;
}
