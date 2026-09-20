#ifndef MINIBOX_LIBUSB_M1522_H
#define MINIBOX_LIBUSB_M1522_H
#include <libusb-1.0/libusb.h>
struct m1522_handle { libusb_context *ctx; libusb_device_handle *dev; int iface; unsigned char bulk_out; unsigned char bulk_in; };
int m1522_printer_interface_rank(unsigned char cls,unsigned char sub,unsigned char proto,unsigned char bulk_out);
int m1522_open(struct m1522_handle *h);
void m1522_close(struct m1522_handle *h);
int m1522_bulk_write(struct m1522_handle *h,const unsigned char *buf,int len,int timeout_ms);
#endif
