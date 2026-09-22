#!/bin/sh
set -eu
CC=${CC:-cc}
TMP="${TMPDIR:-/tmp}/minibox-discovery-test.$$"
trap 'rm -rf "$TMP"' EXIT INT TERM
mkdir -p "$TMP"
$CC -std=c11 -Wall -Wextra -Werror -Isrc/minibox-discoveryd \
  src/minibox-discoveryd/main.c src/minibox-discoveryd/service.c src/minibox-discoveryd/mdns.c \
  -o "$TMP/minibox-discoveryd"
cp overlay/etc/minibox/services.d/*.service "$TMP/"
out="$($TMP/minibox-discoveryd --once "$TMP" 2>&1)"
printf '%s\n' "$out"
printf '%s\n' "$out" | grep -F 'published 2 service(s)' >/dev/null

$CC -std=c11 -Wall -Wextra -Werror -pedantic -Isrc/minibox-discoveryd \
  src/minibox-discoveryd/mdns.c src/minibox-discoveryd/service.c \
  tests/test-mdns-query.c -o "$TMP/test-mdns-query"
"$TMP/test-mdns-query"
