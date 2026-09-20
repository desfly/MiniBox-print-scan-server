#!/usr/bin/env python3
"""Extract HP LaserJet M1522 SOAPHT bulk traffic from a USBPcap/Wireshark capture.

The M1522 SOAPHT interface observed on real hardware uses bulk OUT endpoint 0x03
and bulk IN endpoint 0x83.  This tool intentionally does not decode or replay
commands; it only extracts captured payload bytes for offline analysis.

Usage:
  python tools/extract_soapht_usbpcap.py capture.pcapng -o soapht

Outputs:
  soapht.tsv       chronological transfer list
  soapht-out.bin   concatenated payload bytes sent to endpoint 0x03
  soapht-in.bin    concatenated payload bytes received from endpoint 0x83

Requires Wireshark/tshark on Windows.  The script searches PATH and the usual
Program Files installation directory.
"""

from __future__ import annotations

import argparse
import csv
import os
from pathlib import Path
import shutil
import subprocess
import sys

OUT_EP = "0x03"
IN_EP = "0x83"


def find_tshark(explicit: str | None) -> str:
    candidates = []
    if explicit:
        candidates.append(explicit)
    p = shutil.which("tshark")
    if p:
        candidates.append(p)
    pf = os.environ.get("ProgramFiles")
    if pf:
        candidates.append(str(Path(pf) / "Wireshark" / "tshark.exe"))
    pfx86 = os.environ.get("ProgramFiles(x86)")
    if pfx86:
        candidates.append(str(Path(pfx86) / "Wireshark" / "tshark.exe"))
    for c in candidates:
        if c and Path(c).is_file():
            return c
    raise FileNotFoundError(
        "tshark not found. Install Wireshark with TShark or pass --tshark PATH."
    )


def clean_hex(value: str) -> str:
    # tshark byte fields are normally colon-separated; multiple occurrences may
    # be comma-separated.  Keep hex digits only so both forms are accepted.
    return "".join(ch for ch in value if ch in "0123456789abcdefABCDEF")


def payload_from_fields(capdata: str, fragment: str) -> bytes:
    raw = capdata.strip() or fragment.strip()
    if not raw:
        return b""
    h = clean_hex(raw)
    if len(h) & 1:
        raise ValueError(f"odd-length hex payload: {raw!r}")
    return bytes.fromhex(h)


def normalize_ep(raw: str) -> str:
    raw = raw.strip()
    if not raw:
        return ""
    try:
        return f"0x{int(raw, 0):02x}"
    except ValueError:
        # Some tshark builds render endpoint_address as hex without 0x.
        try:
            return f"0x{int(raw, 16):02x}"
        except ValueError:
            return raw.lower()


def run_tshark(tshark: str, capture: Path) -> list[list[str]]:
    display_filter = "usb.endpoint_address == 0x03 || usb.endpoint_address == 0x83"
    cmd = [
        tshark,
        "-r", str(capture),
        "-Y", display_filter,
        "-T", "fields",
        "-E", "separator=\\t",
        "-E", "quote=n",
        "-E", "occurrence=f",
        "-e", "frame.number",
        "-e", "frame.time_epoch",
        "-e", "usb.device_address",
        "-e", "usb.endpoint_address",
        "-e", "usb.data_len",
        "-e", "usb.capdata",
        "-e", "usb.data_fragment",
    ]
    proc = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if proc.returncode != 0:
        raise RuntimeError(f"tshark failed ({proc.returncode}):\n{proc.stderr.strip()}")
    rows: list[list[str]] = []
    for line in proc.stdout.splitlines():
        cols = line.split("\t")
        cols += [""] * (7 - len(cols))
        rows.append(cols[:7])
    return rows


def main() -> int:
    ap = argparse.ArgumentParser(description="Extract M1522 SOAPHT 0x03/0x83 USB payloads")
    ap.add_argument("capture", type=Path, help="USBPcap/Wireshark .pcapng/.pcap capture")
    ap.add_argument("-o", "--output-prefix", default="soapht", help="output prefix (default: soapht)")
    ap.add_argument("--tshark", help="explicit path to tshark.exe")
    args = ap.parse_args()

    if not args.capture.is_file():
        print(f"capture not found: {args.capture}", file=sys.stderr)
        return 2

    try:
        tshark = find_tshark(args.tshark)
        rows = run_tshark(tshark, args.capture)
    except (FileNotFoundError, RuntimeError) as e:
        print(e, file=sys.stderr)
        return 3

    prefix = Path(args.output_prefix)
    tsv_path = prefix.with_name(prefix.name + ".tsv")
    out_path = prefix.with_name(prefix.name + "-out.bin")
    in_path = prefix.with_name(prefix.name + "-in.bin")

    transfers = 0
    out_bytes = 0
    in_bytes = 0
    with tsv_path.open("w", newline="", encoding="utf-8") as tf, \
         out_path.open("wb") as of, in_path.open("wb") as inf:
        writer = csv.writer(tf, delimiter="\t", lineterminator="\n")
        writer.writerow(["frame", "time_epoch", "device", "endpoint", "direction", "reported_len", "payload_len", "payload_hex"])
        for frame, ts, dev, ep_raw, data_len, capdata, fragment in rows:
            ep = normalize_ep(ep_raw)
            if ep not in (OUT_EP, IN_EP):
                continue
            try:
                payload = payload_from_fields(capdata, fragment)
            except ValueError as e:
                print(f"frame {frame}: {e}", file=sys.stderr)
                return 4
            direction = "OUT" if ep == OUT_EP else "IN"
            writer.writerow([frame, ts, dev, ep, direction, data_len, len(payload), payload.hex()])
            if payload:
                if direction == "OUT":
                    of.write(payload)
                    out_bytes += len(payload)
                else:
                    inf.write(payload)
                    in_bytes += len(payload)
            transfers += 1

    print(f"tshark: {tshark}")
    print(f"SOAPHT transfers: {transfers}")
    print(f"OUT 0x03 payload: {out_bytes} bytes -> {out_path}")
    print(f"IN  0x83 payload: {in_bytes} bytes -> {in_path}")
    print(f"chronology: {tsv_path}")
    if transfers == 0:
        print("warning: no 0x03/0x83 traffic found; verify the capture includes the scan job", file=sys.stderr)
        return 5
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
