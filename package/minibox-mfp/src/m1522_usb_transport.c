/* MiniBox MFP - HP LaserJet M1522n libusb transport core
 * Discovers 03f0:4517 and enumerates interfaces/endpoints without guessing
 * undocumented vendor commands. This is the base for the scanner codec.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <libusb-1.0/libusb.h>

#define HP_VID 0x03f0
#define M1522_PID 0x4517

struct m1522_epmap {
    int iface;
    uint8_t bulk_in;
    uint8_t bulk_out;
    uint16_t in_mps;
    uint16_t out_mps;
};

static int inspect_alt(const struct libusb_interface_descriptor *a,
                       struct m1522_epmap *m)
{
    uint8_t in = 0, out = 0;
    uint16_t in_mps = 0, out_mps = 0;
    int i;
    for (i = 0; i < a->bNumEndpoints; i++) {
        const struct libusb_endpoint_descriptor *e = &a->endpoint[i];
        if ((e->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) != LIBUSB_TRANSFER_TYPE_BULK)
            continue;
        if ((e->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN) {
            in = e->bEndpointAddress; in_mps = e->wMaxPacketSize;
        } else {
            out = e->bEndpointAddress; out_mps = e->wMaxPacketSize;
        }
    }
    if (!in || !out) return 0;
    m->iface = a->bInterfaceNumber;
    m->bulk_in = in; m->bulk_out = out;
    m->in_mps = in_mps; m->out_mps = out_mps;
    return 1;
}

int main(void)
{
    libusb_context *ctx = NULL;
    libusb_device_handle *h = NULL;
    libusb_device *dev;
    struct libusb_config_descriptor *cfg = NULL;
    struct m1522_epmap map = { .iface = -1 };
    int rc, i, j;

    rc = libusb_init(&ctx);
    if (rc < 0) { fprintf(stderr, "libusb_init: %s\n", libusb_error_name(rc)); return 2; }
    h = libusb_open_device_with_vid_pid(ctx, HP_VID, M1522_PID);
    if (!h) { fprintf(stderr, "M1522 03f0:4517 not found\n"); libusb_exit(ctx); return 3; }
    dev = libusb_get_device(h);
    rc = libusb_get_active_config_descriptor(dev, &cfg);
    if (rc < 0) { fprintf(stderr, "config: %s\n", libusb_error_name(rc)); libusb_close(h); libusb_exit(ctx); return 4; }

    for (i = 0; i < cfg->bNumInterfaces && map.iface < 0; i++) {
        const struct libusb_interface *it = &cfg->interface[i];
        for (j = 0; j < it->num_altsetting; j++) {
            const struct libusb_interface_descriptor *a = &it->altsetting[j];
            if (inspect_alt(a, &map)) break;
        }
    }
    if (map.iface < 0) {
        fprintf(stderr, "no bulk IN/OUT interface found\n"); rc = 5;
    } else {
        printf("M1522_IFACE=%d\nM1522_BULK_IN=0x%02x\nM1522_BULK_OUT=0x%02x\nM1522_IN_MPS=%u\nM1522_OUT_MPS=%u\n",
               map.iface, map.bulk_in, map.bulk_out, map.in_mps, map.out_mps);
        rc = 0;
    }
    libusb_free_config_descriptor(cfg);
    libusb_close(h);
    libusb_exit(ctx);
    return rc;
}
