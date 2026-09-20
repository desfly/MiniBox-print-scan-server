#ifndef MINIBOX_USB_BACKEND_H
#define MINIBOX_USB_BACKEND_H
#include <stddef.h>
#define MINIBOX_HP_VID 0x03f0u
#define MINIBOX_M1522_PID 0x4517u
struct minibox_usb_device { unsigned vid, pid; int bus, address; };
int minibox_usb_match(unsigned vid,unsigned pid);
int minibox_usb_parse_lsusb(const char *line, struct minibox_usb_device *out);
#endif
