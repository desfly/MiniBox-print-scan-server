/* MiniBox MFP - HP LaserJet M1522n libusb transport core */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <libusb-1.0/libusb.h>

#define HP_VID 0x03f0
#define M1522_PID 0x4517
#define IO_TIMEOUT_MS 3000

struct m1522_epmap {
    int iface;
    uint8_t bulk_in;
    uint8_t bulk_out;
    uint16_t in_mps;
    uint16_t out_mps;
};

struct m1522_usb {
    libusb_context *ctx;
    libusb_device_handle *h;
    struct m1522_epmap ep;
    int claimed;
};

static int inspect_alt(const struct libusb_interface_descriptor *a,
                       struct m1522_epmap *m)
{
    uint8_t in = 0, out = 0;
    uint16_t im = 0, om = 0;
    int i;

    for (i = 0; i < a->bNumEndpoints; i++) {
        const struct libusb_endpoint_descriptor *e = &a->endpoint[i];

        if ((e->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) != LIBUSB_TRANSFER_TYPE_BULK)
            continue;
        if ((e->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN) {
            in = e->bEndpointAddress;
            im = e->wMaxPacketSize;
        } else {
            out = e->bEndpointAddress;
            om = e->wMaxPacketSize;
        }
    }

    if (!in || !out)
        return 0;

    m->iface = a->bInterfaceNumber;
    m->bulk_in = in;
    m->bulk_out = out;
    m->in_mps = im;
    m->out_mps = om;
    return 1;
}

static int discover(struct m1522_usb *u)
{
    libusb_device *dev;
    struct libusb_config_descriptor *cfg = NULL;
    int rc, i, j;

    u->h = libusb_open_device_with_vid_pid(u->ctx, HP_VID, M1522_PID);
    if (!u->h)
        return LIBUSB_ERROR_NO_DEVICE;

    dev = libusb_get_device(u->h);
    rc = libusb_get_active_config_descriptor(dev, &cfg);
    if (rc < 0)
        return rc;

    u->ep.iface = -1;
    for (i = 0; i < cfg->bNumInterfaces && u->ep.iface < 0; i++) {
        for (j = 0; j < cfg->interface[i].num_altsetting; j++) {
            if (inspect_alt(&cfg->interface[i].altsetting[j], &u->ep))
                break;
        }
    }

    libusb_free_config_descriptor(cfg);
    return u->ep.iface < 0 ? LIBUSB_ERROR_NOT_FOUND : 0;
}

static int open_transport(struct m1522_usb *u)
{
    int rc;

    memset(u, 0, sizeof(*u));
    u->ep.iface = -1;
    rc = libusb_init(&u->ctx);
    if (rc < 0)
        return rc;

    rc = discover(u);
    if (rc < 0)
        return rc;

    if (libusb_kernel_driver_active(u->h, u->ep.iface) == 1) {
        fprintf(stderr, "interface %d has kernel driver; refusing automatic detach\n",
                u->ep.iface);
        return LIBUSB_ERROR_BUSY;
    }

    rc = libusb_claim_interface(u->h, u->ep.iface);
    if (rc < 0)
        return rc;
    u->claimed = 1;
    return 0;
}

static int bulk_write_all(struct m1522_usb *u, const uint8_t *buf,
                          size_t len, unsigned timeout)
{
    size_t off = 0;

    while (off < len) {
        int done = 0;
        int chunk = (len - off) > 16384 ? 16384 : (int)(len - off);
        int rc = libusb_bulk_transfer(u->h, u->ep.bulk_out,
                                     (unsigned char *)buf + off,
                                     chunk, &done, timeout);
        if (rc < 0)
            return rc;
        if (done <= 0)
            return LIBUSB_ERROR_IO;
        off += (size_t)done;
    }
    return 0;
}

static int bulk_read(struct m1522_usb *u, uint8_t *buf, size_t cap,
                     size_t *got, unsigned timeout)
{
    int done = 0;
    int n = cap > 16384 ? 16384 : (int)cap;
    int rc = libusb_bulk_transfer(u->h, u->ep.bulk_in, buf, n, &done, timeout);

    if (got)
        *got = done > 0 ? (size_t)done : 0;
    return rc;
}

static void close_transport(struct m1522_usb *u)
{
    if (u->claimed)
        libusb_release_interface(u->h, u->ep.iface);
    if (u->h)
        libusb_close(u->h);
    if (u->ctx)
        libusb_exit(u->ctx);
    memset(u, 0, sizeof(*u));
}

int main(int argc, char **argv)
{
    struct m1522_usb u;
    int rc = open_transport(&u);

    if (rc < 0) {
        fprintf(stderr, "open transport: %s\n", libusb_error_name(rc));
        close_transport(&u);
        return 2;
    }

    printf("M1522_IFACE=%d\nM1522_BULK_IN=0x%02x\nM1522_BULK_OUT=0x%02x\n"
           "M1522_IN_MPS=%u\nM1522_OUT_MPS=%u\nTRANSPORT=claimed\n",
           u.ep.iface, u.ep.bulk_in, u.ep.bulk_out, u.ep.in_mps, u.ep.out_mps);

    /* Protocol bytes intentionally remain disabled until captured/verified. */
    if (argc > 1 && strcmp(argv[1], "read-once") == 0) {
        uint8_t b[4096];
        size_t got = 0;

        rc = bulk_read(&u, b, sizeof(b), &got, IO_TIMEOUT_MS);
        printf("READ_RC=%d\nREAD_BYTES=%lu\n", rc, (unsigned long)got);
    }

    (void)bulk_write_all;
    close_transport(&u);
    return rc < 0 ? 3 : 0;
}
