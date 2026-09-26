#!/bin/sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
SCRIPT="$ROOT/package/minibox-mfp/files/etc/uci-defaults/90-minibox-recovery-network"
MAKEFILE="$ROOT/package/minibox-mfp/Makefile"

test -s "$SCRIPT"
grep -q "192.168.55.251" "$SCRIPT"
grep -q '""|192.168.1.1)' "$SCRIPT"
grep -q "uci commit network" "$SCRIPT"
grep -q '90-minibox-recovery-network' "$MAKEFILE"

# Recovery defaults must never overwrite an already configured MiniBox LAN
# or inject Wi-Fi credentials/SSID. Normal sysupgrade is responsible for
# preserving the existing Wi-Fi client configuration.
! grep -q "192.168.55.250" "$SCRIPT"
! grep -Eq 'wireless|ssid|key=' "$SCRIPT"

sh -n "$SCRIPT"

echo "MiniBox recovery network contract PASS"
