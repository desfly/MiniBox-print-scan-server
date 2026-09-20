#include "scan_backend.h"
#include "../minibox-scan/soapht_codec.h"
#include "../minibox-scan/soapht_m1522_io.h"
#include "../minibox-usb/scan_m1522.h"

struct m1522_backend_ctx {
    struct m1522_scan_handle usb;
    struct soapht_session transport;
};

static struct m1522_backend_ctx ctx;

static int backend_open(void *v, const struct escl_job *job)
{
    struct m1522_backend_ctx *c = v;
    if (!c || !job || !minibox_soapht_codec) return -1;
    if (soapht_open(&c->transport, &minibox_m1522_soapht_io, &c->usb)) return -2;
    if (minibox_soapht_codec->start(&c->transport, job)) {
        soapht_close(&c->transport);
        return -3;
    }
    return 0;
}

static int backend_read(void *v, unsigned char *buf, size_t cap, size_t *got)
{
    struct m1522_backend_ctx *c = v;
    if (!c || !minibox_soapht_codec) return -1;
    return minibox_soapht_codec->read_image(&c->transport, buf, cap, got);
}

static int backend_end_page(void *v, int *more_pages)
{
    struct m1522_backend_ctx *c = v;
    if (!c || !minibox_soapht_codec) return -1;
    return minibox_soapht_codec->end_page(&c->transport, more_pages);
}

static void backend_close(void *v)
{
    struct m1522_backend_ctx *c = v;
    if (!c) return;
    if (minibox_soapht_codec) (void)minibox_soapht_codec->finish(&c->transport);
    soapht_close(&c->transport);
}

const struct minibox_scan_backend minibox_m1522_scan_backend = {
    backend_open, backend_read, backend_end_page, backend_close
};
void *minibox_m1522_scan_backend_ctx = &ctx;
