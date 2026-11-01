#!/usr/bin/env python3
from pathlib import Path
import re

root=Path(__file__).resolve().parents[1]
raw=bytes.fromhex(''.join((root/'tests/reference/m1522-soapht-2026-09-16/SOAPHT-OUT.hex').read_text().split()))
src=(root/'src/minibox-scan/soapht_codec.c').read_text()

parts=raw.split(b'POST / HTTP/1.1\r\n')
assert len(parts)-1==3, f'capture request count={len(parts)-1}'
reqs=[b'POST / HTTP/1.1\r\n'+p for p in parts[1:]]
expected_header=(b'POST / HTTP/1.1\r\nHost: http:0\r\nUser-Agent: gSOAP/2.7\r\n'
 b'Content-Type: application/soap+xml; charset=utf-8\r\nTransfer-Encoding: chunked\r\n'
 b'Connection: close\r\n\r\n')
for r in reqs: assert r.startswith(expected_header)

def chunk(r):
 p=len(expected_header); e=r.index(b'\r\n',p); n=int(r[p:e],16); s=e+2
 return n,r[s:s+n].decode()
chunks=[chunk(r) for r in reqs]
assert [x[0] for x in chunks]==[0x19e,0x5d4,0x1f2]
assert 'GetScannerElements' in chunks[0][1]
create=chunks[1][1]; retrieve=chunks[2][1]
for x in ('<ImagesToTransfer>0</ImagesToTransfer>','<InputSource>Platen</InputSource>',
 '<InputMediaSize><Width>8500</Width><Height>11690</Height></InputMediaSize>',
 '<ScanRegionWidth>8500</ScanRegionWidth><ScanRegionHeight>11690</ScanRegionHeight>',
 '<ColorProcessing>RGB24</ColorProcessing>','<Resolution><Width>200</Width><Height>200</Height></Resolution>',
 '<RetrieveImageTimeout>300</RetrieveImageTimeout>','<DisableImageProcessing>false</DisableImageProcessing>'):
 assert x in create, x
for x in ('<JobId>2</JobId>','<JobToken></JobToken>','<DocumentDescription></DocumentDescription>'):
 assert x in retrieve, x

# Lock production literals/semantics to the captured protocol.
for x in ('Host: http:0','User-Agent: gSOAP/2.7','Transfer-Encoding: chunked',
 '<ImagesToTransfer>0</ImagesToTransfer>','<RetrieveImageTimeout>300</RetrieveImageTimeout>',
 '<JobToken></JobToken>','<DocumentDescription></DocumentDescription>'):
 assert x in src, x
assert 'escl300_to_soapht1000' in src
print('verified production SOAPHT semantics against captured M1522 OUT stream: OK')
