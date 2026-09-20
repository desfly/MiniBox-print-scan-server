#include <assert.h>
#include <stddef.h>

struct soapht_http_response {
    int status;
    size_t header_len;
    size_t content_length;
    int chunked;
};
int soapht_http_parse_response(const unsigned char *, size_t,
                               struct soapht_http_response *);

int main(void)
{
    static const unsigned char ok[] =
        "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello";
    static const unsigned char partial[] = "HTTP/1.1 200 OK\r\n";
    static const unsigned char chunked[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n";
    static const unsigned char bad[] =
        "HTTP/1.1 200 OK\r\nContent-Length: x\r\n\r\n";
    struct soapht_http_response r;

    assert(soapht_http_parse_response(ok, sizeof(ok)-1, &r) == 0);
    assert(r.status == 200);
    assert(r.content_length == 5);
    assert(r.chunked == 0);
    assert(soapht_http_parse_response(partial, sizeof(partial)-1, &r) == 1);
    assert(soapht_http_parse_response(chunked, sizeof(chunked)-1, &r) == 0);
    assert(r.chunked == 1);
    assert(soapht_http_parse_response(bad, sizeof(bad)-1, &r) == -3);
    return 0;
}
