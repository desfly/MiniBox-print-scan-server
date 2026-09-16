/* MiniBox MFP - HP LaserJet M1522n libusb transport core */
#include <errno.h>
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
    int alt;
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
    m->alt = a->bAlternateSetting;
    m->bulk_in = in;
    m->bulk_out = out;
    m->in_mps = im;
    m->out_mps = om;
    return 1;
}

static void print_alt(const struct libusb_interface_descriptor *a)
{
    int i;
    printf("IFACE=%u ALT=%u CLASS=0x%02x SUBCLASS=0x%02x PROTOCOL=0x%02x EPS=%u\n",
           a->bInterfaceNumber, a->bAlternateSetting, a->bInterfaceClass,
           a->bInterfaceSubClass, a->bInterfaceProtocol, a->bNumEndpoints);
    for (i = 0; i < a->bNumEndpoints; i++) {
        const struct libusb_endpoint_descriptor *e = &a->endpoint[i];
        printf("  EP=0x%02x TYPE=%u DIR=%s MPS=%u\n", e->bEndpointAddress,
               e->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK,
               (e->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN ? "IN" : "OUT",
               e->wMaxPacketSize);
    }
}

static int open_device(struct m1522_usb *u)
{
    int rc = libusb_init(&u->ctx);
    if (rc < 0)
        return rc;
    u->h = libusb_open_device_with_vid_pid(u->ctx, HP_VID, M1522_PID);
    return u->h ? 0 : LIBUSB_ERROR_NO_DEVICE;
}

static int descriptors(struct m1522_usb *u)
{
    struct libusb_config_descriptor *cfg = NULL;
    libusb_device *dev = libusb_get_device(u->h);
    int rc, i, j;

    rc = libusb_get_active_config_descriptor(dev, &cfg);
    if (rc < 0)
        return rc;
    for (i = 0; i < cfg->bNumInterfaces; i++)
        for (j = 0; j < cfg->interface[i].num_altsetting; j++)
            print_alt(&cfg->interface[i].altsetting[j]);
    libusb_free_config_descriptor(cfg);
    return 0;
}

static int select_interface(struct m1522_usb *u, int wanted)
{
    struct libusb_config_descriptor *cfg = NULL;
    libusb_device *dev = libusb_get_device(u->h);
    int rc, i, j;

    rc = libusb_get_active_config_descriptor(dev, &cfg);
    if (rc < 0)
        return rc;
    u->ep.iface = -1;
    for (i = 0; i < cfg->bNumInterfaces; i++) {
        for (j = 0; j < cfg->interface[i].num_altsetting; j++) {
            const struct libusb_interface_descriptor *a = &cfg->interface[i].altsetting[j];
            if (a->bInterfaceNumber == wanted && inspect_alt(a, &u->ep))
                break;
        }
        if (u->ep.iface >= 0)
            break;
    }
    libusb_free_config_descriptor(cfg);
    return u->ep.iface < 0 ? LIBUSB_ERROR_NOT_FOUND : 0;
}

static int claim_selected(struct m1522_usb *u, int iface)
{
    int rc = select_interface(u, iface);
    int kd;
    if (rc < 0)
        return rc;
    kd = libusb_kernel_driver_active(u->h, u->ep.iface);
    if (kd < 0 && kd != LIBUSB_ERROR_NOT_SUPPORTED)
        return kd;
    if (kd == 1) {
        fprintf(stderr, "interface %d has kernel driver; refusing automatic detach\n", u->ep.iface);
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

static int parse_iface(const char *s, int *iface)
{
    char *end = NULL;
    long n;
    errno = 0;
    n = strtol(s, &end, 0);
    if (errno || !end || *end || n < 0 || n > 255)
        return -1;
    *iface = (int)n;
    return 0;
}

int main(int argc, char **argv)
{
    struct m1522_usb u;
    int rc, iface;
    memset(&u, 0, sizeof(u));
    u.ep.iface = -1;

    rc = open_device(&u);
    if (rc < 0) {
        fprintf(stderr, "open device: %s\n", libusb_error_name(rc));
        close_transport(&u);
        return 2;
    }

    if (argc == 2 && strcmp(argv[1], "descriptors") == 0) {
        rc = descriptors(&u);
        close_transport(&u);
        return rc < 0 ? 3 : 0;
    }

    if (argc < 3 || strcmp(argv[1], "--interface") != 0 || parse_iface(argv[2], &iface)) {
        fprintf(stderr, "usage: %s descriptors | --interface N [read-once]\n", argv[0]);
        fprintf(stderr, "refusing automatic interface selection until M1522 scan interface is verified\n");
        close_transport(&u);
        return 64;
    }

    rc = claim_selected(&u, iface);
    if (rc < 0) {
        fprintf(stderr, "claim interface %d: %s\n", iface, libusb_error_name(rc));
        close_transport(&u);
        return 4;
    }

    printf("M1522_IFACE=%d\nM1522_ALT=%d\nM1522_BULK_IN=0x%02x\nM1522_BULK_OUT=0x%02x\n"
           "M1522_IN_MPS=%u\nM1522_OUT_MPS=%u\nTRANSPORT=claimed\n",
           u.ep.iface, u.ep.alt, u.ep.bulk_in, u.ep.bulk_out, u.ep.in_mps, u.ep.out_mps);

    if (argc > 3 && strcmp(argv[3], "read-once") == 0) {
        uint8_t b[4096];
        size_t got = 0;
        rc = bulk_read(&u, b, sizeof(b), &got, IO_TIMEOUT_MS);
        printf("READ_RC=%d\nREAD_BYTES=%lu\n", rc, (unsigned long)got);
    }

    (void)bulk_write_all;
    close_transport(&u);
    return rc < 0 ? 5 : 0;
}
