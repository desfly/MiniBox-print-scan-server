#!/bin/sh
set -eu
pkg-config --exists libusb-1.0
CFLAGS="-std=c99 -Wall -Wextra -Werror -pedantic $(pkg-config --cflags libusb-1.0)"
LIBS="$(pkg-config --libs libusb-1.0)"
cc $CFLAGS -c src/minibox-usb/libusb_m1522.c -o /tmp/libusb_m1522.o
cc $CFLAGS tests/test-printer-interface.c /tmp/libusb_m1522.o $LIBS -o /tmp/test-printer-interface
/tmp/test-printer-interface
cc $CFLAGS -c src/minibox-usb/scan_m1522.c -o /tmp/scan_m1522.o
cc $CFLAGS tests/test-scan-usb-errors.c $LIBS -o /tmp/test-scan-usb-errors
/tmp/test-scan-usb-errors
cc $CFLAGS -c src/minibox-scan/soapht_transport.c -o /tmp/soapht_transport.o
cc $CFLAGS -c src/minibox-scan/soapht_m1522_io.c -o /tmp/soapht_m1522_io.o
cc $CFLAGS -c src/minibox-scan/soapht_codec.c -o /tmp/soapht_codec.o
cc $CFLAGS -c src/minibox-scand/scan_backend.c -o /tmp/scan_backend.o
cc $CFLAGS -c src/minibox-scand/m1522_backend.c -o /tmp/m1522_backend.o
cc $CFLAGS -c src/minibox-usb/scan_diag.c -o /tmp/scan_diag.o
cat >/tmp/scan_chain_main.c <<'EOF'
#include "src/minibox-scand/m1522_backend.h"
int main(void){return minibox_m1522_scan_backend.open == 0;}
EOF
cc $CFLAGS -I. /tmp/scan_chain_main.c \
  /tmp/m1522_backend.o /tmp/scan_backend.o /tmp/soapht_codec.o \
  /tmp/soapht_transport.o /tmp/soapht_m1522_io.o /tmp/scan_m1522.o \
  $LIBS -o /tmp/minibox-scan-chain
/tmp/minibox-scan-chain
echo 'M1522 production scan chain strict-link OK'
