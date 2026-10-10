# MiniBox MFP — 24-hour execution plan (2026-09-21)

## Non-negotiable product contract
A single HP LaserJet M1522n connected by USB to the MiniBox Wi-Fi *client* must be automatically discoverable for printing on both Windows and Android, without entering MiniBox IP, RAW/9100 port or IPP URL. Windows and Android print paths must print a real page. Compatible scanner apps must discover the eSCL scanner and retrieve a **physical** scan. Userspace/libusb only; no CUPS, usblp or Wi-Fi AP mode.

## Starting evidence and blockers
- Hardware MiniBox has r12 installed; 0.3.0-r12 post-upgrade succeeded, printerd and discoveryd restarted, UDP/5353 listening. Firmware BIN has NOT been written; UART recovery remains nonfunctional.
- Windows PowerShell received a 391-byte DNS-SD response from 192.168.55.250 with M1522n, _ipp._tcp.local and rp=ipp/print, but Windows 10's legacy Add Printer wizard did not list the device. Do **not** repeat generic Wi-Fi/router/IGMP testing.
- PR #4 already corrects malformed IPP response operation attributes; CI validates transport, **not** platform compatibility. No physical print on this r12 IPP path yet.
- HP confirms M1522n supports PCL 5, PCL 6 and PostScript Level 3 emulation, **not native** PDF, PCLm or PWG Raster. Android Mopria's Android discovery path expects IPP with supported print data (PDF, PCLm, PWG Raster, depending on client). Current pdl=application/octet-stream is not sufficient; do not fake PDF/PWG capability. Sources: https://support.hp.com/us-en/product/product-specs/hp-laserjet-m1500-multifunction-printer-series/model/3442751 ; https://www.pwg.org/ipp/ippguide.html ; https://android.googlesource.com/platform/prebuilts/fullsdk/sources/+/88c7ff1cd72d6305ec59f97aadc2198cc2dc3592/android-34/com/android/printservice/recommendation/plugin/samsung/PrinterFilterMopria.java
- Microsoft documents that Windows' legacy Add Printer Wizard uses UDP WS-Discovery and HTTP metadata: https://learn.microsoft.com/en-us/windows/win32/wsdapi/troubleshooting-the-add-printer-wizard . A _ipp._tcp answer alone must not be reported as successful automatic Windows installation.

## Work blocks and verifiable deliverables
| Window from start | Work | Evidence to record |
|---|---|---|
| T+0–2h | Freeze facts/r12 baseline. Audit remaining IPP response framing, Print-Job / Validate-Job semantics, DNS-SD and Windows/Android client requirements. | Commit issue matrix and specific failing wire-level regression(s). |
| T+2–6h | Fix demonstrable IPP protocol and job negotiation defects without changing production hardware. | Dedicated unit/integration tests + green server CI on exact SHA. |
| T+6–10h | Android path: identify real PDF/PCLm/PWG -> PCL/PS conversion options that fit 16 MiB flash / 64 MiB RAM; prototype bounded streaming conversion and test with sample documents if feasible. Never advertise unsupported formats. | Accepted format table, tested conversion proof or explicit blocker with measured memory/build size. |
| T+10–14h | Windows auto-discovery path: identify whether the tested Windows 10 build needs WS-Discovery/WSD printer functionality; implement only a functional, standards-correct slice (Probe + required HTTP metadata/print integration), not a dummy advertisement. | Captured/protocol test with Windows-style discovery request, tests for every advertised endpoint. |
| T+14–18h | eSCL physical backend and Android scanning: audit available code/captures; test real-image output on available hardware only if user supplies it; do not equate status XML with a completed scan. | Scanner contract/codec regressions, clear hardware-dependent items. |
| T+18–22h | Consolidate work safely in draft PR(s); run full host/unit, print/scan/discovery contracts and package build; confirm artifact's commit SHA; avoid duplicate firmware builds from documentation-only updates. | Actual CI URLs and artifact SHA, failed steps fixed. |
| T+22–24h | Report hard evidence separately for Windows discovery, Windows print, Android discovery, Android print, Windows/Android scan. Propose package-only field test if suitable. | Checklist with PASS / NOT VERIFIED / BLOCKED, rollback/size/installation plan. |

## Execution policy
- It is a **schedule of work and checkpoints**, not a claim that both operating systems will be fully driverless-ready in 24h; that depends on real format conversion, Windows WSD transport, available hardware and tested clients.
- Current functional r12 stays installed. No full-firmware flash: UART recovery does not work. Any package update is a separate explicit on-device test with a rollback option.
- Every future work pass reads this plan and docs/WORK-TRACKER.md first; reports only concrete new code/CI/hardware evidence and updates the tracker.
