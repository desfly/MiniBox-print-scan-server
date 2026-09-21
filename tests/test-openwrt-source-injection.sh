#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM

OPENWRT="$TMP/openwrt-src"
PACKAGE="$OPENWRT/package/minibox-mfp"
mkdir -p "$OPENWRT/package"
cp -a "$ROOT/package/minibox-mfp" "$PACKAGE"

# The package is relocated below openwrt-src/package, while the repository's
# shared daemon sources must be injected at openwrt-src/src. This is the path
# consumed by $(CURDIR)/../../src in package/minibox-mfp/Makefile.
test ! -e "$PACKAGE/../../src"
cp -a "$ROOT/src" "$OPENWRT/src"
test "$(realpath "$PACKAGE/../../src")" = "$(realpath "$OPENWRT/src")"

required_sources='minibox-printerd/main.c
minibox-printerd/http_body.c
minibox-ipp/ipp.c
minibox-ipp/print_job.c
minibox-usb/stream.c
minibox-usb/libusb_m1522.c
minibox-usb/print_m1522.c
minibox-scand/main.c
minibox-scand/scan_session.c
minibox-scand/scan_backend.c
minibox-scand/m1522_backend.c
minibox-escl/escl.c
minibox-scan/soapht_transport.c
minibox-scan/soapht_m1522_io.c
minibox-scan/soapht_codec.c
minibox-usb/scan_m1522.c
minibox-discoveryd/main.c
minibox-discoveryd/service.c
minibox-discoveryd/mdns.c'

printf '%s\n' "$required_sources" | while IFS= read -r source; do
    test -f "$PACKAGE/../../src/$source" || {
        printf 'missing OpenWrt package source: %s\n' "$source" >&2
        exit 1
    }
done

test -f "$PACKAGE/src/m1522_usb_transport.c"
test -f "$PACKAGE/src/m1522_capture_parser.c"
test -f "$PACKAGE/src/m1522_scan_session.c"

printf '%s\n' 'OpenWrt package source injection contract: PASS'
