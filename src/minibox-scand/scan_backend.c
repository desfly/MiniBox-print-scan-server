#include "scan_backend.h"
#include <string.h>

int minibox_scan_stream_open(struct minibox_scan_stream *s,
                             const struct minibox_scan_backend *backend,
                             void *ctx, const struct escl_job *job)
{
    if (!s || !backend || !backend->open || !backend->read ||
        !backend->end_page || !backend->close || !job)
        return -1;
    memset(s, 0, sizeof(*s));
    s->backend = backend;
    s->ctx = ctx;
    if (backend->open(ctx, job) != 0) {
        s->backend = 0;
        s->ctx = 0;
        return -2;
    }
    s->opened = 1;
    return 0;
}

int minibox_scan_stream_read(struct minibox_scan_stream *s,
                             unsigned char *buf, size_t cap, size_t *got)
{
    if (!s || !s->opened || !s->backend || !buf || !cap || !got)
        return -1;
    *got = 0;
    return s->backend->read(s->ctx, buf, cap, got);
}

int minibox_scan_stream_end_page(struct minibox_scan_stream *s,
                                 int *more_pages)
{
    if (!s || !s->opened || !s->backend || !more_pages) return -1;
    *more_pages = 0;
    return s->backend->end_page(s->ctx, more_pages);
}

void minibox_scan_stream_close(struct minibox_scan_stream *s)
{
    if (!s) return;
    if (s->opened && s->backend && s->backend->close)
        s->backend->close(s->ctx);
    memset(s, 0, sizeof(*s));
}
