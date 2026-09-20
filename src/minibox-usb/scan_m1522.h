#ifndef MINIBOX_SCAN_M1522_H
#define MINIBOX_SCAN_M1522_H
#include <stddef.h>
#include <libusb-1.0/libusb.h>
#define M1522_SOAPHT_CLASS 0xffu
#define M1522_SOAPHT_SUBCLASS 0x02u
#define M1522_SOAPHT_PROTOCOL 0x01u
struct m1522_scan_handle {
    libusb_context *ctx;
    libusb_device_handle *dev;
    int iface;
    unsigned char bulk_out;
    unsigned char bulk_in;
};
int m1522_scan_open(struct m1522_scan_handle *h);
void m1522_scan_close(struct m1522_scan_handle *h);
int m1522_scan_write(struct m1522_scan_handle *h,const unsigned char *buf,size_t len,int timeout_ms);
int m1522_scan_read(struct m1522_scan_handle *h,unsigned char *buf,size_t cap,size_t *got,int timeout_ms);
#endif
