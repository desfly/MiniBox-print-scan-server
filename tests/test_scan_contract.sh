#!/bin/sh
set -eu

i=package/minibox-mfp/files/etc/init.d/minibox-scand
p=package/minibox-mfp/files/usr/libexec/minibox-mfp/scan-policy
m=package/minibox-mfp/Makefile
d=src/minibox-usb/scan_diag.c
c=src/minibox-scan/soapht_codec.c

# Runtime-v2 uses the native C daemon directly. Legacy socat/scanimage wrappers
# and duplicate shell SOAPHT transports must never return to the active path.
test -f "$i"
test -f "$p"
grep -q '/usr/sbin/minibox-scand 8080' "$i"
! grep -q 'TCP-LISTEN:9290' "$i"
grep -q 'scanner-transport=userspace-libusb' "$p"
grep -q 'network-protocol=escl' "$p"
test ! -e package/minibox-mfp/files/usr/libexec/minibox-mfp/soapht-usb

grep -q 'minibox_m1522_scan_backend' src/minibox-scand/m1522_backend.c
grep -q 'soapht_m1522_io.c' "$m"
grep -q 'scan_m1522.c' "$m"
grep -q 'minibox-scan-diag' "$m"
grep -q 'libusb_bulk_transfer' src/minibox-usb/scan_m1522.c

# Until a verified HorseThief codec exists, production scanning must fail closed
# before an unverified command can be sent to the physical M1522n.
grep -Eq 'minibox_soapht_codec[[:space:]]*=[[:space:]]*0' "$c"

grep -q 'M1522_SOAPHT_CLASS' "$d"
grep -q 'LIBUSB_TRANSFER_TYPE_BULK' "$d"
grep -q 'bulk_in&&bulk_out' "$d"
grep -q 'no scan command or payload was sent' "$d"

grep -q 'HP-SOAP-SCAN' docs/M1522-SCAN-PROTOCOL.md
grep -q '03f0:4517' docs/M1522-SCAN-PROTOCOL.md

echo 'scan contract: canonical eSCL/SOAPHT/libusb path locked; codec fail-closed; safe USB diagnostics present'
