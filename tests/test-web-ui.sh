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
grep -q 'Фізичне сканування потребує окремого тесту' "$root/index.html"
release=$(sed -n 's/^PKG_RELEASE:=//p' package/minibox-mfp/Makefile)
test "${release:-0}" -ge 13
grep -Eq 'DEPENDS:=.*\+uhttpd' package/minibox-mfp/Makefile
grep -q '$(1)/www/index.html' package/minibox-mfp/Makefile
grep -q '$(1)/www/cgi-bin/minibox-status' package/minibox-mfp/Makefile

echo 'embedded web UI contract OK'
