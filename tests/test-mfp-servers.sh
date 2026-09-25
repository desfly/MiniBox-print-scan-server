#!/bin/sh
set -eu
cc -std=c99 -Wall -Wextra -Werror -pedantic -DMINIBOX_TEST_PRINT_SINK src/minibox-printerd/main.c src/minibox-printerd/http_body.c src/minibox-ipp/ipp.c src/minibox-raster/pwg_to_pcl.c -o /tmp/minibox-printerd
cc -std=c99 -Wall -Wextra -Werror -pedantic -DMINIBOX_TEST_SCAN_BACKEND src/minibox-scand/main.c src/minibox-scand/scan_session.c src/minibox-scand/scan_backend.c src/minibox-escl/escl.c src/minibox-printerd/http_body.c -o /tmp/minibox-scand
MINIBOX_TEST_PRINT_FILE=/tmp/printed.bin /tmp/minibox-printerd 18631 >/tmp/printerd.log 2>&1 & P=$!
/tmp/minibox-scand 18080 >/tmp/scand.log 2>&1 & S=$!
trap 'kill $P $S 2>/dev/null || true' EXIT INT TERM
sleep 1
curl -fsS http://127.0.0.1:18631/health | grep -q 'printerd ok'
curl -fsS http://127.0.0.1:18080/health | grep -q 'scand ok'
printf '\002\000\000\013\000\000\000\001\003' >/tmp/ipp.req
curl -fsS -o /tmp/ipp.out -H 'Content-Type: application/ipp' --data-binary @/tmp/ipp.req http://127.0.0.1:18631/ipp/print
python3 - <<'PY'
from pathlib import Path

data = Path('/tmp/ipp.out').read_bytes()
assert len(data) > 9, f'Get-Printer-Attributes response too short: {len(data)}'
assert data[:8] == b'\x02\x00\x00\x00\x00\x00\x00\x01', data[:8].hex()
assert data[8] == 0x01, f'expected mandatory operation-attributes group, got 0x{data[8]:02x}'
# Decode the first two operation attributes and demand their RFC 8011 order.
pos = 9
for expected_tag, expected_name, expected_value in (
    (0x47, b'attributes-charset', b'utf-8'),
    (0x48, b'attributes-natural-language', b'en'),
):
    tag = data[pos]
    pos += 1
    key_len = int.from_bytes(data[pos:pos+2], 'big')
    pos += 2
    key = data[pos:pos+key_len]
    pos += key_len
    value_len = int.from_bytes(data[pos:pos+2], 'big')
    pos += 2
    value = data[pos:pos+value_len]
    pos += value_len
    assert (tag, key, value) == (expected_tag, expected_name, expected_value), (tag, key, value)
assert data[pos] == 0x04, f'missing printer-attributes group at {pos}'
for value in (
    b'printer-name',
    b'HP LaserJet M1522n @ MiniBox',
    b'printer-make-and-model',
    b'HP LaserJet M1522n',
    b'printer-uri-supported',
    b'operations-supported',
    b'ipp-versions-supported',
):
    assert value in data, f'missing IPP attribute/value: {value!r}'
assert b'ipp://' in data and b'.local/ipp/print' in data, 'printer URI not aligned with actual host name'
assert data[-1:] == b'\x03', 'IPP response missing end-of-attributes tag'
PY
# A PDF is NOT supported by this raw USB bridge. Neither Print-Job nor
# Validate-Job may advertise success, and unsupported bytes must not reach USB.
python3 - <<'PY'
from pathlib import Path
def build(op):
    key=b'document-format'
    value=b'application/pdf'
    attrs=(bytes((2,0,0,op,0,0,0,19,1,0x49))+
           len(key).to_bytes(2,'big')+key+
           len(value).to_bytes(2,'big')+value+b'\x03')
    return attrs + (b'%PDF-1.4\nnot a printer language\n' if op == 2 else b'')
Path('/tmp/unsupported-print.req').write_bytes(build(2))
Path('/tmp/unsupported-validate.req').write_bytes(build(4))
PY
rm -f /tmp/printed.bin
curl -fsS -o /tmp/unsupported-print.out -H 'Content-Type: application/ipp' --data-binary @/tmp/unsupported-print.req http://127.0.0.1:18631/ipp/print
curl -fsS -o /tmp/unsupported-validate.out -H 'Content-Type: application/ipp' --data-binary @/tmp/unsupported-validate.req http://127.0.0.1:18631/ipp/print
python3 - <<'PY'
from pathlib import Path
for label in ('print','validate'):
    wire=Path('/tmp/unsupported-'+label+'.out').read_bytes()
    assert len(wire)>9 and wire[2:4]==b'\x04\x0a', (label,wire.hex())
assert not Path('/tmp/printed.bin').exists(), 'unsupported document was sent to the print sink'
PY
printf '\002\000\000\002\000\000\000\002\003\033EHello MiniBox\014\033E' >/tmp/print.req
printf '\033EHello MiniBox\014\033E' >/tmp/document.expected
curl -fsS -o /tmp/print.out -H 'Content-Type: application/ipp' --data-binary @/tmp/print.req http://127.0.0.1:18631/ipp/print
cmp /tmp/document.expected /tmp/printed.bin
# Android-style driverless path: accept image/pwg-raster and convert it to
# monochrome PCL5 before the USB sink. This is a host-side transport contract,
# not a claim of physical M1522 acceptance until hardware testing.
python3 - <<'PY'
from pathlib import Path
def be32(v): return int(v).to_bytes(4,'big')
h=bytearray(1796)
h[:9]=b'PwgRaster'
h[276:280]=be32(300); h[280:284]=be32(300)
h[352:356]=be32(595); h[356:360]=be32(842)
h[372:376]=be32(8); h[376:380]=be32(2)
h[384:388]=be32(8); h[388:392]=be32(8); h[392:396]=be32(8)
h[396:400]=be32(0); h[400:404]=be32(18); h[420:424]=be32(1)
raster=b'RaS2'+bytes(h)+bytes([1,249,0,255,0,255,0,255,0,255])
key=b'document-format'; value=b'image/pwg-raster'
ipp=(bytes((2,0,0,2,0,0,0,55,1,0x49))+
     len(key).to_bytes(2,'big')+key+
     len(value).to_bytes(2,'big')+value+b'\x03'+raster)
Path('/tmp/pwg-print.req').write_bytes(ipp)
PY
curl -fsS -o /tmp/pwg-print.out -H 'Content-Type: application/ipp' --data-binary @/tmp/pwg-print.req http://127.0.0.1:18631/ipp/print
python3 - <<'PY'
from pathlib import Path
wire=Path('/tmp/pwg-print.out').read_bytes()
assert len(wire)>9 and wire[2:4]==b'\x00\x00', wire.hex()
out=Path('/tmp/printed.bin').read_bytes()
assert b'@PJL ENTER LANGUAGE=PCL' in out
assert b'\x1b*t300R' in out
assert b'\x1b*r8S' in out
assert out.count(b'\x1b*b1W\xaa')==2, out.hex()
PY
python3 tests/test-large-print.py >/tmp/large.size
[ "$(cat /tmp/large.size)" -gt 65536 ]
cmp /tmp/large.expected /tmp/printed.bin
cat >/tmp/scan.xml <<'EOF'
<?xml version="1.0"?><scan:ScanSettings xmlns:scan="http://schemas.hp.com/imaging/escl/2011/05/03"><scan:InputSource>Platen</scan:InputSource><scan:XResolution>300</scan:XResolution><scan:ColorMode>RGB24</scan:ColorMode></scan:ScanSettings>
EOF
python3 - <<'PY'
import socket, time
body = open('/tmp/scan.xml', 'rb').read()
head = (b'POST /eSCL/ScanJobs HTTP/1.1\r\nHost: minibox\r\nContent-Type: text/xml\r\nContent-Length: ' + str(len(body)).encode() + b'\r\nConnection: close\r\n\r\n')
s = socket.create_connection(('127.0.0.1', 18080))
wire = head + body
for offset in range(0, len(wire), 17):
    s.sendall(wire[offset:offset + 17])
    time.sleep(.001)
response = b''
while True:
    chunk = s.recv(4096)
    if not chunk: break
    response += chunk
s.close()
assert b'201 Created' in response, response
open('/tmp/scan.headers', 'wb').write(response.split(b'\r\n\r\n', 1)[0] + b'\r\n')
PY
grep -qi '^Location: /eSCL/ScanJobs/1' /tmp/scan.headers
code=$(curl -sS -D /tmp/next.headers -o /tmp/next.out -w '%{http_code}' http://127.0.0.1:18080/eSCL/ScanJobs/1/NextDocument); [ "$code" = 200 ]
grep -qi '^Content-Type: image/jpeg' /tmp/next.headers
printf '\377\330MINIBOX\377\331' >/tmp/next.expected
cmp /tmp/next.expected /tmp/next.out
# A real backend-open failure must return 503 AND release the eSCL job state,
# so the next valid ScanJobs POST does not become permanently stuck at 409.
MINIBOX_TEST_SCAN_OPEN_FAIL=1 /tmp/minibox-scand 18081 >/tmp/scand-open-fail.log 2>&1 & F=$!
trap 'kill $P $S $F 2>/dev/null || true' EXIT INT TERM
sleep 1
code=$(curl -sS -D /tmp/failed-job1.headers -o /tmp/failed-job1.out -w '%{http_code}' -H 'Content-Type: text/xml' --data-binary @/tmp/scan.xml http://127.0.0.1:18081/eSCL/ScanJobs)
[ "$code" = 201 ]
grep -qi '^Location: /eSCL/ScanJobs/1' /tmp/failed-job1.headers
code=$(curl -sS -o /tmp/failed-next.out -w '%{http_code}' http://127.0.0.1:18081/eSCL/ScanJobs/1/NextDocument)
[ "$code" = 503 ]
grep -q 'M1522 scan backend unavailable' /tmp/failed-next.out
code=$(curl -sS -D /tmp/failed-job2.headers -o /tmp/failed-job2.out -w '%{http_code}' -H 'Content-Type: text/xml' --data-binary @/tmp/scan.xml http://127.0.0.1:18081/eSCL/ScanJobs)
[ "$code" = 201 ]
grep -qi '^Location: /eSCL/ScanJobs/2' /tmp/failed-job2.headers
grep -q 'stage=backend-open' /tmp/scand-open-fail.log
# Failure on the *first image read* must not produce HTTP 200 or a fake JPEG.
MINIBOX_TEST_SCAN_READ_FAIL=1 /tmp/minibox-scand 18082 >/tmp/scand-read-fail.log 2>&1 & R=$!
trap 'kill $P $S $F $R 2>/dev/null || true' EXIT INT TERM
sleep 1
code=$(curl -sS -D /tmp/read-job1.headers -o /tmp/read-job1.out -w '%{http_code}' -H 'Content-Type: text/xml' --data-binary @/tmp/scan.xml http://127.0.0.1:18082/eSCL/ScanJobs)
[ "$code" = 201 ]
code=$(curl -sS -D /tmp/read-next.headers -o /tmp/read-next.out -w '%{http_code}' http://127.0.0.1:18082/eSCL/ScanJobs/1/NextDocument)
[ "$code" = 503 ]
grep -qi '^Content-Type: text/plain' /tmp/read-next.headers
grep -q 'M1522 scan backend unavailable' /tmp/read-next.out
grep -q 'stage=first-image-read' /tmp/scand-read-fail.log
code=$(curl -sS -D /tmp/read-job2.headers -o /tmp/read-job2.out -w '%{http_code}' -H 'Content-Type: text/xml' --data-binary @/tmp/scan.xml http://127.0.0.1:18082/eSCL/ScanJobs)
[ "$code" = 201 ]
grep -qi '^Location: /eSCL/ScanJobs/2' /tmp/read-job2.headers
echo 'MFP server transport contract OK'
