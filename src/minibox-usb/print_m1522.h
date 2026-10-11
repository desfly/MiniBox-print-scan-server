#ifndef MINIBOX_PRINT_M1522_H
#define MINIBOX_PRINT_M1522_H
#include <stddef.h>
#include "libusb_m1522.h"

#define M1522_CAPTURED_USB_OUT_URB 262144u

struct m1522_print_session {
    struct m1522_handle usb;
    unsigned char *out_buf;
    size_t out_used;
    int opened;
    int failed;
};

int m1522_print_open(struct m1522_print_session *s);
int m1522_print_write(struct m1522_print_session *s,const unsigned char *buf,size_t len);
int m1522_print_finish(struct m1522_print_session *s);
void m1522_print_close(struct m1522_print_session *s);
int m1522_print_document(const unsigned char *doc,size_t len);
#endif
