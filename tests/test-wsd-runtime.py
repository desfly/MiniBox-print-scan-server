#!/usr/bin/env python3
import os
import socket
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

if len(sys.argv) != 3:
    raise SystemExit("usage: test-wsd-runtime.py <wsdd> <cgi>")

wsdd, cgi = sys.argv[1:]
message_id = "urn:uuid:12345678-1234-4234-8234-123456789abc"
probe = f"""<?xml version="1.0" encoding="utf-8"?>
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope"
 xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing"
 xmlns:d="http://schemas.xmlsoap.org/ws/2005/04/discovery"
 xmlns:dp="http://schemas.xmlsoap.org/ws/2006/02/devprof">
 <s:Header><a:MessageID>{message_id}</a:MessageID></s:Header>
 <s:Body><d:Probe><d:Types>dp:Device</d:Types></d:Probe></s:Body>
</s:Envelope>""".encode()

proc = subprocess.Popen([wsdd], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
try:
    time.sleep(0.35)
    if proc.poll() is not None:
        err = proc.stderr.read().decode(errors="replace")
        raise AssertionError(f"wsdd exited before probe: {proc.returncode}: {err}")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(4.0)
    sock.bind(("127.0.0.1", 0))
    sock.sendto(probe, ("127.0.0.1", 3702))
    reply, _ = sock.recvfrom(8192)
    sock.close()

    root = ET.fromstring(reply)
    ns = {
        "s": "http://www.w3.org/2003/05/soap-envelope",
        "a": "http://schemas.xmlsoap.org/ws/2004/08/addressing",
        "d": "http://schemas.xmlsoap.org/ws/2005/04/discovery",
    }
    relates = root.find("./s:Header/a:RelatesTo", ns)
    assert relates is not None and relates.text == message_id
    match = root.find(".//d:ProbeMatch", ns)
    assert match is not None
    address = match.find("./a:EndpointReference/a:Address", ns)
    xaddrs = match.find("./d:XAddrs", ns)
    types = match.find("./d:Types", ns)
    assert address is not None and address.text.startswith("urn:uuid:")
    assert xaddrs is not None and xaddrs.text.startswith("http://")
    assert xaddrs.text.endswith("/cgi-bin/minibox-wsd")
    assert types is not None and "PrintDeviceType" in types.text and "ScanDeviceType" in types.text
    endpoint = address.text

    get_id = "urn:uuid:abcdefab-cdef-4abc-8def-abcdefabcdef"
    request = f"""<?xml version="1.0" encoding="utf-8"?>
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope"
 xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing">
 <s:Header><a:MessageID>{get_id}</a:MessageID></s:Header>
 <s:Body/>
</s:Envelope>""".encode()
    env = dict(os.environ)
    env["REQUEST_METHOD"] = "POST"
    env["CONTENT_LENGTH"] = str(len(request))
    meta = subprocess.run([cgi], input=request, env=env, capture_output=True, check=True)
    wire = meta.stdout
    assert b"Status: 200 OK\r\n" in wire
    header, body = wire.split(b"\r\n\r\n", 1)
    assert b"Content-Type: application/soap+xml" in header
    mroot = ET.fromstring(body)
    mrel = mroot.find("./s:Header/a:RelatesTo", ns)
    assert mrel is not None and mrel.text == get_id
    assert endpoint.encode() in body
    assert b"HP LaserJet M1522n @ MiniBox" in body
    assert b"MFP Printers Scanners" in body
    print("WSD UDP Probe + HTTP metadata integration: OK")
finally:
    proc.terminate()
    try:
        proc.wait(timeout=3)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=3)
    err = proc.stderr.read().decode(errors="replace")
    if proc.returncode not in (0, -15):
        sys.stderr.write(err)
