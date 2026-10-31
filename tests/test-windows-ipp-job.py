#!/usr/bin/env python3
import socket
from pathlib import Path

HOST = ("127.0.0.1", 18631)
SINK = Path("/tmp/printed.bin")

def attr(tag, name, value):
    if isinstance(value, str):
        value = value.encode()
    name = name.encode()
    return bytes([tag]) + len(name).to_bytes(2, "big") + name + len(value).to_bytes(2, "big") + value

def ipp(op, request_id, attrs=b"", document=b""):
    common = (
        b"\x01"
        + attr(0x47, "attributes-charset", "utf-8")
        + attr(0x48, "attributes-natural-language", "en")
    )
    return b"\x02\x00" + op.to_bytes(2, "big") + request_id.to_bytes(4, "big") + common + attrs + b"\x03" + document

def request(body, chunked=False):
    s = socket.create_connection(HOST)
    if chunked:
        head = (
            b"POST /ipp/print HTTP/1.1\r\n"
            b"Host: minibox\r\n"
            b"Content-Type: application/ipp\r\n"
            b"Transfer-Encoding: chunked\r\n"
            b"Connection: close\r\n\r\n"
        )
        s.sendall(head)
        for offset in range(0, len(body), 37):
            chunk = body[offset : offset + 37]
            s.sendall(f"{len(chunk):X}\r\n".encode() + chunk + b"\r\n")
        s.sendall(b"0\r\n\r\n")
    else:
        head = (
            b"POST /ipp/print HTTP/1.1\r\n"
            b"Host: minibox\r\n"
            b"Content-Type: application/ipp\r\n"
            b"Content-Length: " + str(len(body)).encode() + b"\r\n"
            b"Connection: close\r\n\r\n"
        )
        s.sendall(head + body)
    response = b""
    while True:
        part = s.recv(4096)
        if not part:
            break
        response += part
    s.close()
    return response

printer = attr(0x45, "printer-uri", "ipp://127.0.0.1:18631/ipp/print")
create = request(ipp(0x0005, 101, printer))
assert b"200 OK" in create, create
assert b"job-id" in create, create
assert b"job-uri" in create, create

job_id = (
    bytes([0x21])
    + len(b"job-id").to_bytes(2, "big")
    + b"job-id"
    + (4).to_bytes(2, "big")
    + (1).to_bytes(4, "big")
)
document = b"PWG-WINDOWS-CHUNKED-REGRESSION"
send = request(
    ipp(
        0x0006,
        102,
        printer + job_id + attr(0x49, "document-format", "image/pwg-raster"),
        document,
    ),
    chunked=True,
)
assert b"200 OK" in send, send
assert b"job-state" in send, send
assert SINK.read_bytes() == document
print("Windows Create-Job + chunked Send-Document regression OK")
