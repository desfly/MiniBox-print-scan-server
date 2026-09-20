#include "../src/minibox-usb/usb_backend.h"
#include <assert.h>
int main(void){struct minibox_usb_device d;assert(minibox_usb_match(0x03f0,0x4517));assert(!minibox_usb_match(0x03f0,0x1234));assert(minibox_usb_parse_lsusb("Bus 001 Device 002: ID 03f0:4517 Hewlett-Packard EWS UPD",&d)==1);assert(d.vid==0x03f0&&d.pid==0x4517&&d.bus==1&&d.address==2);return 0;}
