#include <stddef.h>
#include <string.h>
#include <stdlib.h>

/* Small, allocation-free parser for the HTTP-like framing used on the
 * HP-SOAP-SCAN channel. This does not encode scanner commands. */

struct soapht_http_response {
    int status;
    size_t header_len;
    size_t content_length;
    int chunked;
};

static const unsigned char *find_bytes(const unsigned char *p, size_t n,
                                       const char *needle, size_t nn)
{
    size_t i;
    if (!p || !needle || nn == 0 || n < nn) return 0;
    for (i = 0; i + nn <= n; ++i)
        if (memcmp(p + i, needle, nn) == 0) return p + i;
    return 0;
}

static int ascii_ieq(const unsigned char *a, size_t n, const char *b)
{
    size_t i;
    for (i = 0; i < n && b[i]; ++i) {
        unsigned char c = a[i];
        unsigned char d = (unsigned char)b[i];
        if (c >= 'A' && c <= 'Z') c = (unsigned char)(c + ('a' - 'A'));
        if (d >= 'A' && d <= 'Z') d = (unsigned char)(d + ('a' - 'A'));
        if (c != d) return 0;
    }
    return i == n && b[i] == '\0';
}

int soapht_http_parse_response(const unsigned char *buf, size_t len,
                               struct soapht_http_response *out)
{
    const unsigned char *end, *line, *colon;
    size_t head_len;
    if (!buf || !out) return -1;
    memset(out, 0, sizeof(*out));
    end = find_bytes(buf, len, "\r\n\r\n", 4);
    if (!end) return 1;
    head_len = (size_t)(end - buf) + 4;
    out->header_len = head_len;
    if (len < 12 || memcmp(buf, "HTTP/1.", 7) != 0) return -2;
    line = find_bytes(buf, head_len, " ", 1);
    if (!line || (size_t)(line - buf) + 4 > head_len) return -2;
    if (line[1] < '0' || line[1] > '9' ||
        line[2] < '0' || line[2] > '9' ||
        line[3] < '0' || line[3] > '9') return -2;
    out->status = (line[1]-'0')*100 + (line[2]-'0')*10 + (line[3]-'0');
    if (out->status < 100 || out->status > 599) return -2;

    line = find_bytes(buf, head_len, "\r\n", 2);
    if (!line) return -2;
    line += 2;
    while (line < end) {
        const unsigned char *eol;
        size_t name_len, value_len;
        const unsigned char *value;
        /* For the final header, its terminating CRLF is the first half of
         * the CRLFCRLF delimiter, so search through end+2. */
        eol = find_bytes(line, (size_t)((end + 2) - line), "\r\n", 2);
        if (!eol || eol > end) return -2;
        colon = find_bytes(line, (size_t)(eol-line), ":", 1);
        if (!colon) return -2;
        name_len = (size_t)(colon-line);
        value = colon + 1;
        while (value < eol && (*value == ' ' || *value == '\t')) ++value;
        value_len = (size_t)(eol-value);
        if (ascii_ieq(line, name_len, "Content-Length")) {
            char tmp[32];
            char *ep;
            unsigned long v;
            if (value_len == 0 || value_len >= sizeof(tmp)) return -3;
            memcpy(tmp, value, value_len);
            tmp[value_len] = 0;
            v = strtoul(tmp, &ep, 10);
            if (*ep) return -3;
            out->content_length = (size_t)v;
        } else if (ascii_ieq(line, name_len, "Transfer-Encoding")) {
            if (value_len == 7 && ascii_ieq(value, value_len, "chunked"))
                out->chunked = 1;
        }
        line = eol + 2;
    }
    if (out->chunked && out->content_length) return -4;
    return 0;
}
