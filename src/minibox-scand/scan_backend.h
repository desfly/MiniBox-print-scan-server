#ifndef MINIBOX_SCAN_BACKEND_H
#define MINIBOX_SCAN_BACKEND_H

#include <stddef.h>
#include "../minibox-escl/escl.h"

struct minibox_scan_backend {
    int (*open)(void *ctx, const struct escl_job *job);
    int (*read)(void *ctx, unsigned char *buf, size_t cap, size_t *got);
    int (*end_page)(void *ctx, int *more_pages);
    void (*close)(void *ctx);
};

struct minibox_scan_stream {
    const struct minibox_scan_backend *backend;
    void *ctx;
    int opened;
};

int minibox_scan_stream_open(struct minibox_scan_stream *s,
                             const struct minibox_scan_backend *backend,
                             void *ctx, const struct escl_job *job);
int minibox_scan_stream_read(struct minibox_scan_stream *s,
                             unsigned char *buf, size_t cap, size_t *got);
int minibox_scan_stream_end_page(struct minibox_scan_stream *s,
                                 int *more_pages);
void minibox_scan_stream_close(struct minibox_scan_stream *s);

#endif
