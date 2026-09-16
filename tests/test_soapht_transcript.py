#!/usr/bin/env python3
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "tools" / "analyze_soapht_transcript.py"

TSV = """frame\ttime_epoch\tdevice\tendpoint\tdirection\treported_len\tpayload_len\tpayload_hex
10\t1000.000\t2\t0x03\tOUT\t2\t2\t0102
11\t1000.005\t2\t0x03\tOUT\t2\t2\t0304
12\t1000.010\t2\t0x83\tIN\t3\t3\taabbcc
13\t1000.100\t2\t0x83\tIN\t1\t1\tdd
14\t1000.105\t2\t0x03\tOUT\t0\t0\t
15\t1000.110\t2\t0x03\tOUT\t1\t1\tff
"""


def main() -> int:
    with tempfile.TemporaryDirectory() as td:
        td = Path(td)
        tsv = td / "synthetic.tsv"
        out = td / "fixture"
        tsv.write_text(TSV, encoding="utf-8")
        p = subprocess.run([sys.executable, str(TOOL), str(tsv), "-o", str(out)],
                           text=True, capture_output=True)
        assert p.returncode == 0, p.stderr + p.stdout
        manifest = json.loads((out / "manifest.json").read_text(encoding="utf-8"))
        xs = manifest["exchanges"]
        assert len(xs) == 4, xs
        assert [x["direction"] for x in xs] == ["OUT", "IN", "IN", "OUT"]
        assert [x["bytes"] for x in xs] == [4, 3, 1, 1]
        assert xs[0]["first_frame"] == 10 and xs[0]["last_frame"] == 11
        assert xs[0]["transfers"] == 2
        assert (out / xs[0]["fixture"]).read_bytes() == bytes.fromhex("01020304")
        assert (out / xs[1]["fixture"]).read_bytes() == bytes.fromhex("aabbcc")
        assert (out / xs[2]["fixture"]).read_bytes() == bytes.fromhex("dd")
        assert (out / xs[3]["fixture"]).read_bytes() == bytes.fromhex("ff")
        assert xs[0]["preview_hex"] == "01020304"
        assert len(xs[0]["sha256"]) == 64
    print("SOAPHT transcript analyzer synthetic contract: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
