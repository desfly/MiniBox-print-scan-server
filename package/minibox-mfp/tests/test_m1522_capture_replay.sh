#!/bin/sh
# Deterministic parser/session fixture. Synthetic bytes test framing only;
# they are explicitly NOT an HP protocol capture.
set -eu
PARSER=${PARSER:-./m1522_capture_parser}
TMP=${TMPDIR:-/tmp}/m1522-replay.$$
trap 'rm -f "$TMP" "$TMP.out"' EXIT INT TERM
cat >"$TMP" <<'EOF'
# synthetic framing fixture, not device protocol
OUT 00 01 02 03
IN  10 11 12 13
OUT aa bb
IN  cc dd ee
EOF
"$PARSER" "$TMP" >"$TMP.out"
grep -q '^CAPTURE=valid$' "$TMP.out"
grep -q '^FRAMES=4$' "$TMP.out"
grep -q '^IN_FRAMES=2$' "$TMP.out"
grep -q '^OUT_FRAMES=2$' "$TMP.out"
grep -q '^BYTES=13$' "$TMP.out"
echo 'm1522 capture replay: PASS'
