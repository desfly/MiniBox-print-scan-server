#!/bin/sh
set -eu
CC=${CC:-cc}
TMP="${TMPDIR:-/tmp}/minibox-discovery-test.$$"
trap 'rm -rf "$TMP"' EXIT INT TERM
mkdir -p "$TMP"
$CC -std=c11 -Wall -Wextra -Werror -Isrc/minibox-discoveryd \
  src/minibox-discoveryd/main.c src/minibox-discoveryd/service.c \
  src/minibox-discoveryd/mdns.c src/minibox-discoveryd/wsd_identity.c \
  -o "$TMP/minibox-discoveryd"
cp overlay/etc/minibox/services.d/*.service "$TMP/"
out="$($TMP/minibox-discoveryd --once "$TMP" 2>&1)"
printf '%s\n' "$out"
printf '%s\n' "$out" | grep -F 'published 2 service(s)' >/dev/null

$CC -std=c11 -Wall -Wextra -Werror -pedantic -Isrc/minibox-discoveryd \
  src/minibox-discoveryd/mdns.c src/minibox-discoveryd/service.c \
  tests/test-mdns-query.c -o "$TMP/test-mdns-query"
"$TMP/test-mdns-query"

$CC -std=c11 -Wall -Wextra -Werror -pedantic -Isrc/minibox-discoveryd \
  src/minibox-discoveryd/wsd_identity.c tests/test-discovery-identity.c \
  -o "$TMP/test-discovery-identity"
"$TMP/test-discovery-identity"

$CC -std=c11 -Wall -Wextra -Werror -pedantic -Isrc/minibox-discoveryd \
  src/minibox-discoveryd/wsd_main.c \
  src/minibox-discoveryd/wsd_runtime.c \
  src/minibox-discoveryd/wsd_identity.c \
  src/minibox-discoveryd/wsd.c \
  -o "$TMP/minibox-wsdd"

$CC -std=c11 -Wall -Wextra -Werror -pedantic -Isrc/minibox-discoveryd \
  src/minibox-discoveryd/wsd_metadata.c \
  src/minibox-discoveryd/wsd_identity.c \
  src/minibox-discoveryd/wsd.c \
  -o "$TMP/minibox-wsd-cgi"

grep -q 'minibox-wsdd' package/minibox-mfp/Makefile
grep -q 'minibox-wsd-cgi' package/minibox-mfp/Makefile
python3 tests/test-wsd-runtime.py "$TMP/minibox-wsdd" "$TMP/minibox-wsd-cgi"
printf '%s\n' 'WSD runtime build/integration contract: OK'
