#include "../src/minibox-usb/libusb_m1522.h"
#include "../src/minibox-usb/stream.h"
#include <assert.h>
#include <string.h>
#include <libusb-1.0/libusb.h>

/* Simulate timeout with three bytes accepted by the USB controller.
 * The next transfer must contain only the untransmitted suffix. */
static const unsigned char expected[] = "01234567";
static unsigned char received[sizeof expected - 1];
static int calls;
static int mode;
int libusb_bulk_transfer(libusb_device_handle *dev, unsigned char endpoint,
                         unsigned char *data, int length, int *transferred,
                         unsigned int timeout) {
    (void)dev; (void)timeout;
    assert(endpoint == 0x02);
    if(mode==2){
        /* A zero-progress timeout must terminate this write; do not replay. */
        *transferred=0;
        calls++;
        return LIBUSB_ERROR_TIMEOUT;
    }
    if(mode==1){
        /* Disconnect is not a timeout: partial progress must NOT mask it. */
        *transferred=3;
        calls++;
        return LIBUSB_ERROR_NO_DEVICE;
    }
    if (calls == 0) {
        assert(length == 8);
        assert(!memcmp(data, expected, 3));
        memcpy(received, data, 3);
        *transferred = 3;
        calls++;
        return LIBUSB_ERROR_TIMEOUT;
    }
    assert(calls == 1);
    assert(length == 5);
    assert(!memcmp(data, expected + 3, 5));
    memcpy(received + 3, data, 5);
    *transferred = 5;
    calls++;
    return 0;
}
static int sink(void *ctx,const unsigned char *buf,int n,int timeout) {
    return m1522_bulk_write((struct m1522_handle *)ctx,buf,n,timeout);
}
int main(void) {
    struct m1522_handle h;
    memset(&h,0,sizeof h);
    h.dev = (libusb_device_handle *)&h; /* non-null sentinel; mock never dereferences */
    h.bulk_out = 0x02;
    assert(minibox_stream_write(sink,&h,expected,sizeof expected - 1,8,5000)==0);
    assert(calls==2);
    assert(!memcmp(received,expected,sizeof received));
    mode=1;calls=0;
    assert(minibox_stream_write(sink,&h,expected,sizeof expected - 1,8,5000)!=0);
    assert(calls==1);
    mode=2;calls=0;
    assert(minibox_stream_write(sink,&h,expected,sizeof expected - 1,8,5000)!=0);
    assert(calls==1);
    return 0;
}
