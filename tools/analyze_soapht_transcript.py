#!/usr/bin/env python3
"""Analyze an extracted M1522 SOAPHT TSV without guessing protocol semantics.

Input is the chronological TSV produced by extract_soapht_usbpcap.py.  This tool
coalesces adjacent transfers in the same direction into exchanges, reports
sizes/timing, emits exact binary fixtures, and writes a JSON manifest suitable
for regression tests.  It deliberately does NOT label bytes as start/end/image
until protocol evidence proves those meanings.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
from pathlib import Path


def load_rows(path: Path):
    rows = []
    with path.open("r", encoding="utf-8", newline="") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            direction = r["direction"].strip().upper()
            if direction not in ("OUT", "IN"):
                continue
            payload = bytes.fromhex(r["payload_hex"].strip()) if r["payload_hex"].strip() else b""
            rows.append({
                "frame": int(r["frame"]),
                "time": float(r["time_epoch"]),
                "endpoint": r["endpoint"],
                "direction": direction,
                "payload": payload,
            })
    return rows


def coalesce(rows, gap_ms: float):
    exchanges = []
    for r in rows:
        if not r["payload"]:
            continue
        join = False
        if exchanges and exchanges[-1]["direction"] == r["direction"]:
            gap = (r["time"] - exchanges[-1]["last_time"]) * 1000.0
            join = gap <= gap_ms
        if join:
            e = exchanges[-1]
            e["payload"] += r["payload"]
            e["last_frame"] = r["frame"]
            e["last_time"] = r["time"]
            e["transfers"] += 1
        else:
            exchanges.append({
                "direction": r["direction"],
                "endpoint": r["endpoint"],
                "first_frame": r["frame"],
                "last_frame": r["frame"],
                "first_time": r["time"],
                "last_time": r["time"],
                "transfers": 1,
                "payload": r["payload"],
            })
    return exchanges


def main() -> int:
    ap = argparse.ArgumentParser(description="Analyze extracted M1522 SOAPHT transcript")
    ap.add_argument("tsv", type=Path)
    ap.add_argument("-o", "--output-dir", type=Path, default=Path("soapht-fixture"))
    ap.add_argument("--same-direction-gap-ms", type=float, default=20.0,
                    help="coalesce adjacent same-direction transfers within this gap")
    args = ap.parse_args()
    if not args.tsv.is_file():
        raise SystemExit(f"input not found: {args.tsv}")

    rows = load_rows(args.tsv)
    exchanges = coalesce(rows, args.same_direction_gap_ms)
    args.output_dir.mkdir(parents=True, exist_ok=True)

    manifest = {
        "format": 1,
        "source": args.tsv.name,
        "coalesce_gap_ms": args.same_direction_gap_ms,
        "exchanges": [],
    }
    for i, e in enumerate(exchanges):
        name = f"{i:04d}-{e['direction'].lower()}.bin"
        data = e["payload"]
        (args.output_dir / name).write_bytes(data)
        item = {
            "index": i,
            "direction": e["direction"],
            "endpoint": e["endpoint"],
            "first_frame": e["first_frame"],
            "last_frame": e["last_frame"],
            "transfers": e["transfers"],
            "bytes": len(data),
            "sha256": hashlib.sha256(data).hexdigest(),
            "fixture": name,
            "preview_hex": data[:32].hex(),
        }
        manifest["exchanges"].append(item)
        print(f"{i:04d} {e['direction']:3s} frames {e['first_frame']}-{e['last_frame']} "
              f"xfer={e['transfers']} bytes={len(data)} sha256={item['sha256'][:16]} "
              f"preview={item['preview_hex']}")

    out = args.output_dir / "manifest.json"
    out.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"exchanges: {len(exchanges)}")
    print(f"manifest: {out}")
    return 0 if exchanges else 5


if __name__ == "__main__":
    raise SystemExit(main())
