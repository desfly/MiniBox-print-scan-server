#!/bin/sh
set -eu
for f in overlay/etc/init.d/minibox-printerd overlay/etc/init.d/minibox-scand overlay/etc/init.d/minibox-discoveryd overlay/etc/init.d/minibox-wsdd; do
  grep -q 'USE_PROCD=1' "$f"
  grep -q 'procd_set_param command' "$f"
done
grep -q ' 631' overlay/etc/init.d/minibox-printerd
grep -q ' 8080' overlay/etc/init.d/minibox-scand
grep -q '/usr/sbin/minibox-wsdd' overlay/etc/init.d/minibox-wsdd
echo 'procd runtime contract OK'
