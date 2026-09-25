#include "soapht_codec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SOAPHT_RAW_BUFFER 4096
#define SOAPHT_HEADER_MAX 2048
#define SOAPHT_CONTROL_MAX (128u * 1024u)

struct body_reader {
    struct soapht_session *transport;
    unsigned char raw[SOAPHT_RAW_BUFFER];
    size_t raw_pos, raw_len;
    size_t content_left, chunk_left;
    int chunked, need_chunk_crlf, done, status;
};

struct codec_state {
    int started, retrieve_started, image_done;
    char job_id[32];
    struct body_reader image;
    size_t record_left, record_pad;
    unsigned char record_flags;
    int record_is_image, image_continues;
};

static struct codec_state state;

static unsigned read_be16(const unsigned char *p)
{ return ((unsigned)p[0] << 8) | p[1]; }

static unsigned long read_be32(const unsigned char *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) |
           ((unsigned long)p[2] << 8) | p[3];
}

static size_t pad4(size_t n) { return (n + 3u) & ~(size_t)3u; }

static int raw_fill(struct body_reader *r)
{
    size_t got = 0;
    int rc;
    if (r->raw_pos < r->raw_len) return 0;
    rc = soapht_read(r->transport, r->raw, sizeof(r->raw), &got);
    if (rc || !got) {
        fprintf(stderr, "minibox-scand: stage=soapht-raw-read rc=%d got=%zu\n", rc, got);
        return -1;
    }
    r->raw_pos = 0;
    r->raw_len = got;
    return 0;
}

static int raw_byte(struct body_reader *r, unsigned char *out)
{
    if (raw_fill(r)) return -1;
    *out = r->raw[r->raw_pos++];
    return 0;
}

static int raw_copy(struct body_reader *r, unsigned char *out,
                    size_t cap, size_t *got)
{
    size_t n;
    if (raw_fill(r)) return -1;
    n = r->raw_len - r->raw_pos;
    if (n > cap) n = cap;
    memcpy(out, r->raw + r->raw_pos, n);
    r->raw_pos += n;
    *got = n;
    return 0;
}

static int read_line_raw(struct body_reader *r, char *line, size_t cap)
{
    size_t n = 0;
    unsigned char c, prev = 0;
    if (!cap) return -1;
    for (;;) {
        if (raw_byte(r, &c)) return -1;
        if (prev == '\r' && c == '\n') {
            if (n) --n;
            line[n] = 0;
            return 0;
        }
        if (n + 1 >= cap) return -1;
        line[n++] = (char)c;
        prev = c;
    }
}

static int contains_header(const char *headers, const char *needle)
{
    const char *p = headers;
    size_t nn = strlen(needle);
    while (*p) {
        const char *q = p;
        size_t i;
        for (i = 0; i < nn && q[i]; ++i) {
            char a = q[i], b = needle[i];
            if (a >= 'A' && a <= 'Z') a += 'a' - 'A';
            if (b >= 'A' && b <= 'Z') b += 'a' - 'A';
            if (a != b) break;
        }
        if (i == nn) return 1;
        p = strchr(p, '\n');
        if (!p) break;
        ++p;
    }
    return 0;
}

static int reader_begin(struct body_reader *r, struct soapht_session *transport)
{
    char header[SOAPHT_HEADER_MAX], line[128];
    size_t n = 0;
    unsigned char c;
    memset(r, 0, sizeof(*r));
    r->transport = transport;
    while (n + 1 < sizeof(header)) {
        if (raw_byte(r, &c)) return -1;
        header[n++] = (char)c;
        if (n >= 4 && !memcmp(header + n - 4, "\r\n\r\n", 4)) break;
    }
    if (n < 4 || n + 1 >= sizeof(header)) return -2;
    header[n] = 0;
    if (sscanf(header, "HTTP/1.%*u %d", &r->status) != 1) return -3;
    r->chunked = contains_header(header, "Transfer-Encoding: chunked");
    if (!r->chunked) {
        const char *p = strstr(header, "Content-Length:");
        if (!p) p = strstr(header, "content-length:");
        if (!p) return -4;
        p = strchr(p, ':');
        r->content_left = p ? (size_t)strtoul(p + 1, 0, 10) : 0;
    }
    (void)line;
    return 0;
}

static int consume_chunk_crlf(struct body_reader *r)
{
    unsigned char a, b;
    if (raw_byte(r, &a) || raw_byte(r, &b)) return -1;
    return (a == '\r' && b == '\n') ? 0 : -1;
}

static int body_read(struct body_reader *r, unsigned char *out,
                     size_t cap, size_t *got)
{
    size_t n;
    *got = 0;
    if (r->done || !cap) return 0;
    if (!r->chunked) {
        if (!r->content_left) { r->done = 1; return 0; }
        n = cap < r->content_left ? cap : r->content_left;
        if (raw_copy(r, out, n, got)) return -1;
        r->content_left -= *got;
        if (!r->content_left) r->done = 1;
        return 0;
    }
    while (!r->chunk_left) {
        char line[64], *end;
        unsigned long v;
        if (r->need_chunk_crlf) {
            if (consume_chunk_crlf(r)) return -2;
            r->need_chunk_crlf = 0;
        }
        if (read_line_raw(r, line, sizeof(line))) return -3;
        v = strtoul(line, &end, 16);
        if (end == line || (*end && *end != ';')) return -4;
        if (!v) {
            do { if (read_line_raw(r, line, sizeof(line))) return -5; }
            while (line[0]);
            r->done = 1;
            return 0;
        }
        r->chunk_left = (size_t)v;
    }
    n = cap < r->chunk_left ? cap : r->chunk_left;
    if (raw_copy(r, out, n, got)) return -6;
    r->chunk_left -= *got;
    if (!r->chunk_left) r->need_chunk_crlf = 1;
    return 0;
}

static int body_exact(struct body_reader *r, unsigned char *out, size_t len)
{
    size_t off = 0, got;
    while (off < len) {
        if (body_read(r, out + off, len - off, &got) || !got) return -1;
        off += got;
    }
    return 0;
}

static int body_skip(struct body_reader *r, size_t len)
{
    unsigned char tmp[128];
    size_t got, n;
    while (len) {
        n = len < sizeof(tmp) ? len : sizeof(tmp);
        if (body_read(r, tmp, n, &got) || !got) return -1;
        len -= got;
    }
    return 0;
}

static int send_request(struct soapht_session *transport, const char *xml)
{
    char header[320], chunk[40];
    size_t n = strlen(xml);
    int hn = snprintf(header, sizeof(header),
        "POST / HTTP/1.1\r\n"
        "Host: http:0\r\n"
        "User-Agent: gSOAP/2.7\r\n"
        "Content-Type: application/soap+xml; charset=utf-8\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Connection: close\r\n\r\n");
    int cn = snprintf(chunk, sizeof(chunk), "%lX\r\n", (unsigned long)n);
    if (hn <= 0 || cn <= 0) return -1;
    if (soapht_write(transport, (unsigned char *)header, (size_t)hn) ||
        soapht_write(transport, (unsigned char *)chunk, (size_t)cn) ||
        soapht_write(transport, (const unsigned char *)xml, n) ||
        soapht_write(transport, (const unsigned char *)"\r\n0\r\n\r\n", 7))
        return -1;
    return 0;
}

static int control_request(struct soapht_session *transport, const char *xml,
                           char **body_out)
{
    struct body_reader r;
    unsigned char *body;
    size_t len = 0, got;
    int rc = send_request(transport, xml);
    if (rc) {
        fprintf(stderr, "minibox-scand: stage=soapht-control-write rc=%d\n", rc);
        return -1;
    }
    rc = reader_begin(&r, transport);
    if (rc) {
        fprintf(stderr, "minibox-scand: stage=soapht-control-headers rc=%d\n", rc);
        return -1;
    }
    if (r.status < 200 || r.status >= 300) {
        fprintf(stderr, "minibox-scand: stage=soapht-control-http status=%d\n", r.status);
        return -2;
    }
    body = malloc(SOAPHT_CONTROL_MAX + 1);
    if (!body) return -3;
    while (!r.done) {
        if (len == SOAPHT_CONTROL_MAX) {
            fprintf(stderr, "minibox-scand: stage=soapht-control-body rc=-5 status=%d received=%zu limit=%u\n",
                    r.status, len, (unsigned)SOAPHT_CONTROL_MAX);
            free(body);
            return -4;
        }
        rc = body_read(&r, body + len, SOAPHT_CONTROL_MAX - len, &got);
        if (rc || (!got && !r.done)) {
            fprintf(stderr, "minibox-scand: stage=soapht-control-body rc=%d status=%d received=%zu got=%zu content_left=%zu chunk_left=%zu chunked=%d raw_pending=%zu\n",
                    rc, r.status, len, got, r.content_left, r.chunk_left,
                    r.chunked, r.raw_len - r.raw_pos);
            free(body);
            return -4;
        }
        len += got;
    }
    body[len] = 0;
    if (body_out) *body_out = (char *)body; else free(body);
    return 0;
}

static const char get_elements_xml[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
    "xmlns:SOAP-ENC=\"http://www.w3.org/2003/05/soap-encoding\" "
    "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
    "xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
    "xmlns:wscn=\"http://tempuri.org/wscn.xsd\"><SOAP-ENV:Body>"
    "<wscn:GetScannerElements></wscn:GetScannerElements>"
    "</SOAP-ENV:Body></SOAP-ENV:Envelope>";

static int make_create_xml(char *out, size_t cap, const struct escl_job *job)
{
    const char *source = job->source == ESCL_SOURCE_ADF ? "ADF" : "Platen";
    const char *color = job->color ? "RGB24" : "GrayScale8";
    unsigned dpi = job->dpi >= 75 && job->dpi <= 1200 ? job->dpi : 300;
    int n = snprintf(out, cap,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
        "xmlns:SOAP-ENC=\"http://www.w3.org/2003/05/soap-encoding\" "
        "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
        "xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
        "xmlns:wscn=\"http://tempuri.org/wscn.xsd\"><SOAP-ENV:Body>"
        "<wscn:CreateScanJobRequest><ScanIdentifier></ScanIdentifier><ScanTicket>"
        "<JobDescription></JobDescription><DocumentParameters><Format>jfif</Format>"
        "<CompressionQualityFactor>0</CompressionQualityFactor><ImagesToTransfer>0</ImagesToTransfer>"
        "<InputSource>%s</InputSource><ContentType>Auto</ContentType><InputSize>"
        "<InputMediaSize><Width>8500</Width><Height>11690</Height></InputMediaSize>"
        "<DocumentSizeAutoDetect>false</DocumentSizeAutoDetect></InputSize><Exposure>"
        "<AutoExposure>false</AutoExposure><ExposureSettings><Contrast>0</Contrast>"
        "</ExposureSettings></Exposure><MediaSides><MediaFront><ScanRegion>"
        "<ScanRegionXOffset>0</ScanRegionXOffset><ScanRegionYOffset>0</ScanRegionYOffset>"
        "<ScanRegionWidth>8500</ScanRegionWidth><ScanRegionHeight>11690</ScanRegionHeight>"
        "</ScanRegion><ColorProcessing>%s</ColorProcessing><Resolution>"
        "<Width>%u</Width><Height>%u</Height></Resolution></MediaFront></MediaSides>"
        "</DocumentParameters><RetrieveImageTimeout>300</RetrieveImageTimeout>"
        "<ScanManufacturingParameters><DisableImageProcessing>false</DisableImageProcessing>"
        "</ScanManufacturingParameters></ScanTicket></wscn:CreateScanJobRequest>"
        "</SOAP-ENV:Body></SOAP-ENV:Envelope>", source, color, dpi, dpi);
    return n > 0 && (size_t)n < cap ? 0 : -1;
}

static int make_retrieve_xml(char *out, size_t cap, const char *job_id)
{
    int n = snprintf(out, cap,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
        "xmlns:SOAP-ENC=\"http://www.w3.org/2003/05/soap-encoding\" "
        "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
        "xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
        "xmlns:wscn=\"http://tempuri.org/wscn.xsd\"><SOAP-ENV:Body>"
        "<wscn:RetrieveImageRequest><JobId>%s</JobId><JobToken></JobToken>"
        "<DocumentDescription></DocumentDescription></wscn:RetrieveImageRequest>"
        "</SOAP-ENV:Body></SOAP-ENV:Envelope>", job_id);
    return n > 0 && (size_t)n < cap ? 0 : -1;
}

static int codec_start(struct soapht_session *transport,
                       const struct escl_job *job)
{
    char xml[4096], *response = 0, *a, *b;
    size_t n;
    memset(&state, 0, sizeof(state));
    if (!transport || !job) return -1;
    {
        int rc = control_request(transport, get_elements_xml, 0);
        if (rc) {
            fprintf(stderr, "minibox-scand: stage=soapht-get-elements rc=%d\n", rc);
            return -2;
        }
    }
    if (make_create_xml(xml, sizeof(xml), job)) {
        fprintf(stderr, "minibox-scand: stage=soapht-create-xml rc=-1\n");
        return -3;
    }
    {
        int rc = control_request(transport, xml, &response);
        if (rc) {
            fprintf(stderr, "minibox-scand: stage=soapht-create-job rc=%d\n", rc);
            return -3;
        }
    }
    a = strstr(response, "<JobId>");
    b = a ? strstr(a + 7, "</JobId>") : 0;
    if (!a || !b || b == a + 7 || (n = (size_t)(b - (a + 7))) >= sizeof(state.job_id)) {
        fprintf(stderr, "minibox-scand: stage=soapht-job-id rc=-4\n");
        free(response); return -4;
    }
    memcpy(state.job_id, a + 7, n);
    state.job_id[n] = 0;
    free(response);
    state.started = 1;
    return 0;
}

static int next_dime_record(void)
{
    unsigned char h[12], type[64];
    size_t options_len, id_len, type_len, data_len;
    if (body_exact(&state.image, h, sizeof(h))) return -1;
    if ((h[0] & 0xf8u) != 0x08u) return -2;
    options_len = read_be16(h + 2);
    id_len = read_be16(h + 4);
    type_len = read_be16(h + 6);
    data_len = (size_t)read_be32(h + 8);
    if (options_len > 4096 || id_len > 4096 || type_len >= sizeof(type)) return -3;
    if (body_skip(&state.image, pad4(options_len)) ||
        body_skip(&state.image, pad4(id_len))) return -4;
    memset(type, 0, sizeof(type));
    if (type_len && body_exact(&state.image, type, type_len)) return -5;
    if (pad4(type_len) > type_len &&
        body_skip(&state.image, pad4(type_len) - type_len)) return -5;
    state.record_flags = h[0];
    state.record_left = data_len;
    state.record_pad = pad4(data_len) - data_len;
    state.record_is_image =
        (type_len == 10 && !memcmp(type, "image/jpeg", 10)) ||
        (type_len == 0 && state.image_continues);
    state.image_continues = state.record_is_image && (h[0] & 0x01u);
    return 0;
}

static int begin_retrieve(struct soapht_session *transport)
{
    char xml[2048];
    if (make_retrieve_xml(xml, sizeof(xml), state.job_id) ||
        send_request(transport, xml) || reader_begin(&state.image, transport)) return -1;
    if (state.image.status != 200) return -2;
    state.retrieve_started = 1;
    return 0;
}

static int codec_read_image(struct soapht_session *transport,
                            unsigned char *out, size_t cap, size_t *got)
{
    unsigned char scratch[256];
    size_t total = 0, n, want;
    if (!transport || !out || !cap || !got || !state.started) return -1;
    *got = 0;
    if (state.image_done) return 0;
    if (!state.retrieve_started && begin_retrieve(transport)) return -2;
    while (total < cap && !state.image_done) {
        if (!state.record_left) {
            if (state.record_pad && body_skip(&state.image, state.record_pad)) return -3;
            state.record_pad = 0;
            if (state.record_is_image && !(state.record_flags & 0x01u)) {
                state.image_done = 1;
                break;
            }
            if (next_dime_record()) return -4;
            if (!state.record_left) continue;
        }
        if (state.record_is_image) {
            want = cap - total;
            if (want > state.record_left) want = state.record_left;
            if (body_read(&state.image, out + total, want, &n) || !n) return -5;
            total += n;
        } else {
            want = state.record_left < sizeof(scratch) ? state.record_left : sizeof(scratch);
            if (body_read(&state.image, scratch, want, &n) || !n) return -6;
        }
        state.record_left -= n;
    }
    *got = total;
    return 0;
}

static int codec_end_page(struct soapht_session *transport, int *more_pages)
{
    (void)transport;
    if (!more_pages || !state.image_done) return -1;
    *more_pages = 0;
    return 0;
}

static int codec_finish(struct soapht_session *transport)
{
    (void)transport;
    memset(&state, 0, sizeof(state));
    return 0;
}

static const struct soapht_codec verified_m1522_codec = {
    codec_start, codec_read_image, codec_end_page, codec_finish
};

const struct soapht_codec *minibox_soapht_codec = &verified_m1522_codec;
