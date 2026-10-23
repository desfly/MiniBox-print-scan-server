#!/usr/bin/env python3
"""Independent byte-for-byte reference from successful HP M1522 USB capture #003.

Only the PCL XL binding and 2338x1700 first image preamble are fixed here.
Do not compare JPEG bytes: the image is generated from different input pixels.
Reference: M1522-FULL-USB-003(1).pcap, USB OUT payload, captured 2026-10-22.
"""
import pathlib
import sys

REFERENCE = bytes.fromhex(
    "292048502d50434c20584c3b323b313b436f6d6d656e7420436f707972696768742863292031393939204d6963726f736f667420436f72706f726174696f6e0ad158025802f889c000f886c003f88f41c000f888c001f88248c001f826c000f834c001f828c8c0024134f82543c204107068f891c001f893c001f894c001f89546d50000803f0000803ff82b77c001f8036ac000f82d78c000f82d7cc0ccf82c7bd364006400f82a756169d37d01b300f84c6bc000f82d78c000f82d7cc0ccf82c7bc002f8036ac000f864c002f862c12209f86cc1a406f86bd1a3173011f867b0c10000f86dc1a406f863c002f865b1fa"
)

def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: check_capture_003_prefix.py generated-pclxl.bin")
    stream = pathlib.Path(sys.argv[1]).read_bytes()
    start = stream.find(b") HP-PCL XL;2;1;")
    if start < 0:
        raise SystemExit("FAIL: missing HP PCL XL binding")
    actual = stream[start:start + len(REFERENCE)]
    if len(actual) != len(REFERENCE):
        raise SystemExit("FAIL: truncated PCL XL preamble")
    if actual != REFERENCE:
        offset = next(i for i, (a, b) in enumerate(zip(actual, REFERENCE)) if a != b)
        raise SystemExit(
            f"FAIL: HP capture #003 preamble mismatch at byte {offset}: "
            f"generated=0x{actual[offset]:02x} expected=0x{REFERENCE[offset]:02x}"
        )
    print(f"PASS: exact {len(REFERENCE)}-byte HP capture #003 PCL XL preamble")

if __name__ == "__main__":
    main()
