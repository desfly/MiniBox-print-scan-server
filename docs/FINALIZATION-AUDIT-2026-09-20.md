# MiniBox print/scan server finalization audit — 2026-09-20

This is the append-only evidence tracker for the finalization branch. Hardware-dependent
items remain `PENDING/MANUAL`; no flash write is authorized by this work.

## Repository baseline

- Canonical repository: `desfly/MiniBox-print-scan-server`
- Canonical URL: <https://github.com/desfly/MiniBox-print-scan-server>
- Repository ID: `1314138414`
- Default branch baseline: `main` at `90226c8b905530951353283bf0a240932d0094ed`
- Runtime branch baseline: `minibox-network-mfp-runtime-v2` at `d20119ec39b0ea5042ef660cb40d74a9df50ea7c`
- Common integration base: `minibox-network-mfp-discovery` at `9578b2589adcab778366d3ae7929ddb3992cdcb0`
- Initial divergence: main-only 28 commits; runtime-v2-only 145 commits
- Open PRs at audit start: none
- Open Issues at audit start: none
- Finalization branch: `audit/minibox-finalize-20260920`, based on runtime-v2

## Findings and resolutions

| ID | Area | Finding | Resolution/test | Status |
|---|---|---|---|---|
| F-001 | Integration | `main` and runtime-v2 independently changed package, discovery, scanner diagnostics and CI. | Merge both histories; retain native runtime and add safe diagnostic binaries. | RESOLVED |
| F-002 | Packaging | Add/add conflict in `package/minibox-mfp/Makefile`; main package was diagnostics-only while runtime-v2 contained production daemons. | Combined targets; release r12; retained libusb and truthful Avahi dependency. | RESOLVED |
| F-003 | Discovery | Legacy main script advertised raw port 9100, HTTP port 80, PDF/JPEG/PWG printing and PDF scanning without matching runtime support. | Excluded duplicate legacy publisher/init scripts; keep native `_ipp._tcp` and `_uscan._tcp` contracts. | RESOLVED |
| F-004 | HTTP security | Conflicting duplicate `Content-Length` values were accepted. | Reject conflicting values; added unit regression. | RESOLVED |
| F-005 | Resource limits | Printer accepted arbitrarily large declared bodies and clients could block a single-thread daemon indefinitely. | 128 MiB print-job limit and 15-second receive/send socket deadlines. | RESOLVED |
| F-006 | eSCL compatibility | Scanner parsed only one `recv()`, so fragmented Windows/Android HTTP POSTs could fail. | Bounded complete-request reader with Content-Length validation; 17-byte fragment integration test. | RESOLVED |
| F-007 | HTTP correctness | Scanner could emit `503` after already starting a `200 image/jpeg` response. | Distinguish pre-header and post-header stream failures; close connection after partial stream. | RESOLVED |
| F-008 | Concurrency | Print and scan daemons serialize clients; this avoids USB/session races but one slow client could monopolize a daemon. | Bounded by socket and USB timeouts. Multi-client concurrency remains intentionally serialized for 64 MiB target. | ACCEPTED |
| F-009 | Hardware | USB interface selection, SOAPHT command behavior, mDNS visibility and end-to-end clients require the MiniBox/M1522. | Exact manual checklist included in the test-only artifact. | PENDING/MANUAL |
| F-010 | Artifact safety | Previous workflow names/readme could be read as hardware-verified and checksums lacked source/rollback/SBOM evidence. | Build-0108 and r12 artifacts are marked TEST-ONLY, include source SHA, checksums, rollback/manual checklist and SPDX metadata; initramfs is preferred when produced. | RESOLVED |

## Test log

- `tests/test-mfp-core.sh`: PASS
- `tests/test-mfp-servers.sh`: PASS, including fragmented eSCL request
- `tests/test-procd-runtime.sh`: PASS
- `tests/test-discovery-runtime.sh`: PASS
- discovery service C contract: PASS
- SOAPHT HTTP framing C contract: PASS
- `tests/test_soapht_transcript.py`: PASS
- `tests/test_scan_contract.sh`: PASS
- main-branch M1522 capture replay: PASS after building its parser as CI does
- strict libusb host compilation: pending CI because the local runner lacks `pkg-config` and libusb development headers

## Hardware validation gate

Do not flash from this audit. A user-controlled RAM-boot or explicitly approved test install must verify:

1. Boot and confirm board identity, ART/Wi-Fi calibration and stable Wi-Fi-client reconnect.
2. Confirm `03f0:4517`, no `usblp`, and exclusive libusb claim/release for printer interface 1 and SOAPHT interface 0.
3. Windows: automatic printer discovery, attributes, test page, repeated/large job, cancel/error recovery.
4. Android: automatic printer discovery through the installed print service and a real document print.
5. Windows and Android: `_uscan._tcp` discovery, capabilities/status, platen and ADF scans at supported settings.
6. Disconnect/reconnect USB and Wi-Fi during idle and active operations; verify bounded failure and recovery.
7. Reboot twice and verify services, discovery and configuration persistence.
8. Only after all checks pass may hardware readiness be claimed.
