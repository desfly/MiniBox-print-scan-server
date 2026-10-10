#!/bin/sh
set -eu

root=package/minibox-mfp/files/www
test -s "$root/index.html"
test -s "$root/minibox/app.js"
test -s "$root/cgi-bin/minibox-status"
grep -q 'MiniBox print + scan server' "$root/index.html"
grep -q '/cgi-bin/minibox-status' "$root/minibox/app.js"
# Do not present a nonexistent TCP/9100 listener or claim physical scan success.
grep -q 'RAW/JetDirect (порт 9100): не реалізовано' "$root/index.html"
grep -q '/cgi-bin/luci/' "$root/index.html"
grep -q 'Стан фізичного USB-зв’язку з МФУ' "$root/index.html"
grep -q 'МФУ HP LaserJet M1522n' "$root/cgi-bin/minibox-status"
grep -q '03f0' "$root/cgi-bin/minibox-status"
grep -q '4517' "$root/cgi-bin/minibox-status"
version=$(sed -n 's/^PKG_VERSION:=//p' package/minibox-mfp/Makefile)
release=$(sed -n 's/^PKG_RELEASE:=//p' package/minibox-mfp/Makefile)
case "$version" in
  0.3.0) test "${release:-0}" -ge 13 ;;
  0.4.0) test "${release:-0}" -ge 1 ;;
  *) echo "unexpected minibox-mfp version: $version" >&2; exit 1 ;;
esac
grep -Eq 'DEPENDS:=.*\+uhttpd' package/minibox-mfp/Makefile
grep -q '$(1)/www/index.html' package/minibox-mfp/Makefile
grep -q '$(1)/www/cgi-bin/minibox-status' package/minibox-mfp/Makefile

echo 'embedded web UI contract OK'
