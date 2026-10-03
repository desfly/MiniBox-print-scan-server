/* libusb bulk-transfer error contract; never turn a negative libusb error
 * into a fictitious positive number of bytes sent to the SOAPHT device.
 * Link against real libusb for unused functions, but intercept only bulk I/O.
 */
#define libusb_bulk_transfer mock_libusb_bulk_transfer
#include "../src/minibox-usb/scan_m1522.c"
#undef libusb_bulk_transfer
#include <assert.h>
#include <stdio.h>

static int mock_status;
static int mock_bytes;

int mock_libusb_bulk_transfer(libusb_device_handle *dev, unsigned char ep,
                             unsigned char *data, int len, int *transferred,
                             unsigned int timeout)
{
    (void)dev; (void)ep; (void)data; (void)timeout;
    *transferred = mock_bytes < len ? mock_bytes : len;
    return mock_status;
}

int main(void)
{
    struct m1522_scan_handle h = {0};
    unsigned char buf[16] = {0};
    size_t got = 99;
    h.dev = (libusb_device_handle *)(void *)&h;
    h.bulk_out = 0x03;
    h.bulk_in = 0x83;

    mock_bytes = 5;
    mock_status = LIBUSB_ERROR_TIMEOUT;
    assert(m1522_scan_write(&h, buf, sizeof buf, 5000) == LIBUSB_ERROR_TIMEOUT);
    assert(m1522_scan_read(&h, buf, sizeof buf, &got, 5000) == LIBUSB_ERROR_TIMEOUT);
    assert(got == 0);

    mock_status = LIBUSB_SUCCESS;
    assert(m1522_scan_write(&h, buf, sizeof buf, 5000) == 5);
    assert(m1522_scan_read(&h, buf, sizeof buf, &got, 5000) == 0);
    assert(got == 5);
    puts("M1522 scanner libusb error propagation: OK");
    return 0;
}
