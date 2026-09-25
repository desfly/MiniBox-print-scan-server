# MiniBox print + scan server — WORK TRACKER

This file is the canonical work checkpoint for ChatGPT-assisted development.

Finalization audit and per-finding evidence: `docs/FINALIZATION-AUDIT-2026-09-20.md`.

## Mandatory workflow rule

Before any code change:
1. Read this tracker.
2. Verify the current GitHub branch HEAD.
3. Verify relevant GitHub Actions / latest build state.
4. Compare facts with the tracker; do not infer progress from chat memory alone.
5. Only then choose and implement the next step.

After every meaningful code/config/workflow change, hardware test, build, or newly discovered blocker, update this tracker with the new SHA/build/result/NEXT STEP.

Do not re-prove completed layers unless a regression is detected.

## Current checkpoint

- Repository: `desfly/MiniBox-print-scan-server`
- Working branch: `minibox-network-mfp-runtime-v2`
- Current code HEAD before this tracker update: `14905aafa0900def4aed7cefd24dc8c8e0ae2630`
- Latest code change: `scan: add offline SOAPHT transcript analyzer`
- Canonical tracker introduced at: `88800b0b81e7fb5a5bfb4c19959c9bfa21dbe2d2`
- Firmware line: Build-0106 / OpenWrt 25.12.5 / Gainstrong MiniBox V1.0 / AR9330
- Runtime package physically installed on MiniBox: `minibox-mfp-0.3.0-r10.apk`
- r10 source commit: `9cf0a79a539e47c657a240eaf6ec8dcece1e3e1f`
- r10 APK SHA256: `eb761ee0c39d9667c1ce189d75e4f4f3061525e37053f8da438878a1d176ce98`
- r10 artifact run: `35020884433`

## Architecture contract

- MiniBox operates primarily as a Wi-Fi client.
- HP LaserJet M1522n MFP USB VID:PID: `03f0:4517`.
- No CUPS.
- No `usblp`.
- Printer and scanner USB access is userspace/libusb.
- Network print protocol: IPP.
- Network scan protocol: eSCL/AirScan style endpoint.
- Automatic discovery uses mDNS/DNS-SD.

## Proven — do not redo without regression

### Printing

Physical end-to-end network printing has succeeded:

`Windows -> Wi-Fi -> MiniBox -> IPP :631 -> libusb -> HP M1522n`

A real test page printed. Correct PJL/UEL close framing was proven to clear the MFP's lingering `Печать документа` state.

### Scanner network layer

Windows has successfully reached eSCL `ScannerCapabilities` and `ScannerStatus`; status returned `Idle`.

### Scanner USB layer

M1522 scanner transport identified:
- interface 0: SOAPHT `ff/02/01`
- bulk OUT: `0x03`
- bulk IN: `0x83`
- interrupt IN: `0x84`
- printer interface: interface 1, `07/01/02`

Physical `/usr/sbin/minibox-scan-diag --claim` succeeded on r10 and released safely without sending a scan payload.

## Current CI state

Verified source HEAD `ba6bf53aa7ef965f0164b69cc1d968ad42610a9d` had four workflow runs; observed contract workflows were green. Build-0106 was changed there to consume current runtime-v2 source.

Documentation/tool-only commits after that do not change the installed MiniBox runtime yet.

## Current scanner blocker

Real M1522 image acquisition is NOT implemented. Production remains intentionally fail-closed with `minibox_soapht_codec = 0`.

Known old HPLIP wrapper ABI:
- `bb_open`
- `bb_close`
- `bb_get_parameters`
- `bb_is_paper_in_adf`
- `bb_start_scan`
- `bb_get_image_data`
- `bb_end_page`
- `bb_end_scan`

Low-level command framing/bytes still require verified evidence.

## Reverse-engineering tooling

### Capture extractor

`tools/extract_soapht_usbpcap.py`

Filters real capture traffic for `0x03 OUT` / `0x83 IN`, writes chronological TSV and concatenated streams. It intentionally does not decode or replay unknown commands.

### Transcript analyzer — NEW

`tools/analyze_soapht_transcript.py`

Code commit: `14905aafa0900def4aed7cefd24dc8c8e0ae2630`

It consumes the extractor TSV and, without assigning guessed protocol meanings:
- coalesces adjacent same-direction payload transfers into exchanges
- preserves first/last frame and transfer counts
- emits exact per-exchange `.bin` fixtures
- records byte lengths and SHA256
- writes a JSON manifest for regression/codec tests
- prints a short hex preview for inspection

This creates the fixture-first path required before enabling any real SOAPHT command encoder.

## Hardware evidence still needed

Capture one known-good direct USB scan from Windows with USBPcap/Wireshark:
- M1522 directly connected to Windows laptop by USB
- platen
- one page
- 300 dpi
- color or grayscale
- capture starts before scan and stops immediately after scan
- save `.pcapng`

Only verified protocol evidence may be used to implement/enable the SOAPHT codec.

## Truth table

- MiniBox sees M1522 USB: YES
- Physical network print: YES
- IPP endpoint: YES
- Correct PJL/UEL close: YES
- Persistent mDNS code: YES
- eSCL capabilities/status from Windows: YES
- SOAPHT interface/endpoints identified: YES
- libusb scanner claim/release on hardware: YES
- Capture extractor: YES
- Offline transcript -> fixture analyzer: YES
- Real scan movement/image: NO
- Verified SOAPHT codec: NO
- Automatic Windows printer discovery: NOT YET PHYSICALLY CONFIRMED
- Automatic Windows scanner discovery: NOT YET PHYSICALLY CONFIRMED

## NEXT STEP

1. Add deterministic tests for `analyze_soapht_transcript.py` using synthetic TSV data only; this tests tooling, not protocol assumptions.
2. Wire that test into the existing contract CI so the reverse-engineering pipeline cannot silently regress.
3. Keep `minibox_soapht_codec = 0`.
4. Obtain real M1522 `.pcapng`, run extractor + analyzer, then commit only sanitized protocol fixtures/manifest needed for codec tests.
5. Derive and implement `start/read_image/end_page/end_scan` only after real transcript evidence.
6. Update this tracker after each step.

## 2026-09-23 — integrated web UI recovery

- Repository name verified on GitHub: `desfly/MiniBox-print-scan-server`; default branch remains `main`.
- Root cause confirmed from the artifact of run `35775205560` / commit `d24d92e`: its manifest contains `uhttpd` and `uhttpd-mod-ubus` from the base build-kit configuration, but `minibox-mfp` packaged no `/www` UI files and LuCI is absent. The green build therefore had a web server without the intended management page.
- Decision for the 16 MB flash / 64 MB RAM target: embed the previously agreed minimal HTML UI and use `uhttpd`; do not add full LuCI unless the minimal UI proves insufficient. This keeps the UI explicit and avoids the substantially larger LuCI dependency set.
- Branch: `fix/integrated-web-ui-20260923` based on the exact commit used by yesterday's successful integration build.
- Added `/www/index.html`, `/www/minibox/index.html`, `/www/minibox/app.js`, read-only `/www/cgi-bin/minibox-status`, and a hard package dependency on `uhttpd`.
- Added source contract checks plus post-build squashfs inspection for the UI files, `uhttpd` binary/config/init script and enabled `/etc/rc.d/S??uhttpd` link. Factory image size remains gated at 16,580,608 bytes.
- Physical baseline rechecked read-only on the running MiniBox before installing any candidate: OpenWrt 25.12.5 / kernel 6.12.94, `minibox-mfp-0.3.0-r12`, `uhttpd` and `uhttpd-mod-ubus` are installed; LuCI is absent. `uhttpd`, printerd, scand and discoveryd are running and their boot links exist (`S50`, `S91`, `S91`, `S92`). HTTP 80, IPP 631, eSCL 8080 and UDP 5353 listen; RAW 9100 does not.
- The physical `/www` contains no UI files and `/www/cgi-bin` does not exist, while `uhttpd` serves a directory index. This independently confirms the failure mechanism on hardware: the server starts, but the image/package has no web payload. No reboot, package install or flash write was performed during this check.
- Resource baseline: 55,676 KiB total RAM with 15,876 KiB available at idle, and about 9.0 MiB free overlay. This reinforces the lightweight static UI decision; it is not a hardware-compatibility result for the new image.
- Read-only MTD inspection confirms the physical `firmware` partition is exactly 16,580,608 bytes, matching the CI factory-image ceiling. `u-boot` is a separate 131,072-byte partition and `art` is a separate final 65,536-byte partition; neither was read or modified. The current TP-Link firmware parser exposes kernel/rootfs splits and the boot cmdline is `console=ttyATH0,115200 rootfstype=squashfs,jffs2`. This is layout evidence only, not proof that UART recovery or a new image is safe.
- Safety state: CI output is TEST-ONLY. Do not flash until initramfs/RAM boot and board-specific Ethernet, Wi-Fi, ART/calibration, USB, UI and recovery checks pass.

### NEXT STEP

1. Push the branch, open a PR and run GitHub Actions from the PR head.
2. Record the real run URL, artifact name/digest and generated image sizes here.
3. RAM-boot the initramfs candidate and verify `http://<MiniBox-IP>/` before any flash write.

## 2026-09-24 — physical test #101 and follow-up PR: eSCL 503 recovery/diagnostics

- [x] Hardware-tested image documented in PR #8 issue comment 5818989553: run 36014351722, image commit 133d6113ee6823607a50242c0ba0ffd97d9aebf6; web UI PASS and manual IPP/PCL6 paper print PASS; automatic Windows/Android discovery FAIL; eSCL NextDocument HTTP 503 FAIL.
- [x] On actual M1522, SOAPHT interface 0 (ff/02/01), bulk OUT 0x03 and bulk IN 0x83, with claim/release success. Repeating USB-descriptor/claim tests without a new regression is not the next step.
- [x] Code audit: the generic 503 corresponds to scan stream/backend OPEN failing, but the previously asserted exact SOAPHT-handshake failure was **not proven**; the m1522_scan_session test harness is separate from the production eSCL path. Production SOAPHT codec exists but has not yielded a physical image.
- [x] Branch `fix/scan-503-recovery-and-stage-logs-20260924` created from PR #8 head `d7bb6e913ad7d1722152756727dd331a53785330`. Fixes eSCL session stuck in READING after backend-open 503, adds stage/error logs at backend, SOAPHT and libusb boundaries, and a regression in `tests/test-mfp-servers.sh` that requires the next ScanJobs POST to return 201 rather than 409. No new USB command bytes or printer path changes.
- [ ] Obtain green mfp-server-contract CI on final PR head; a source patch is not hardware validation. Do not claim scanning works until actual JPEG bytes are retrieved from physical M1522.
- [ ] Use new server logs on a hardware run to distinguish libusb open/claim, SOAPHT GetScannerElements and CreateScanJob failure. If protocol mismatch remains, obtain a known-good direct USB capture; do not invent vendor commands.
- [ ] Complete platform discovery separately. Windows 10 legacy wizard requires WSD investigation; Android print service requires genuinely supported document formats/conversion. mDNS visibility and manual PCL6 print do not prove auto-install or driverless output.
- [ ] Preserve current physically printing firmware as rollback; no flash/merge to production based only on this diagnostics PR.

## 2026-09-24 — physical r13 HTTP 201 / scanner GetScannerElements 503

- Hardware: on the physically installed `minibox-mfp-0.3.0-r13`, MiniBox loopback eSCL POST `/eSCL/ScanJobs` with platen 300 dpi RGB24 returned HTTP 201 and `Location: /eSCL/ScanJobs/1`. GET `/eSCL/ScanJobs/1/NextDocument` returned HTTP 503, **no JPEG**.
- Device log, 2026-09-24 20:10:42: `stage=soapht-get-elements rc=-4`, then `stage=soapht-start rc=-2`, then `scan job=1 stage=backend-open rc=-2`. The first SOAPHT control response is not fully parsed/read. This does **not** establish a USB disconnect, an invalid vendor command, or a printer-side fault. The outer -4 is an internal control-body failure code; prior r13 diagnostics did not preserve the underlying USB result or expected/received body length.
- Earlier physical tests established the USB SOAPHT interface/endpoints and claim/release; do not redo these in place of first-response diagnosis. Current physical print/web operation is independent of this scan failure.
- Diagnostics-only branch `fix/soapht-503-hardware-error-detail-20260924`, based on PR #12 head `830ab8b322769e4f5e4c156badd334944d179620`: log failed USB bulk IN/OUT return code and byte count, SOAPHT control response header/status/body-stage failure and framing counters without dumping image/USB payload. Add a truncated-first-response codec regression and assert log stages. Bump runtime package release to `0.3.0-r14`. No unverified USB commands or protocol retries introduced, and **no hardware scan success claimed**.
- NEXT: get green PR CI and r14 APK; install APK *only after checking its source/artifact identity*, then repeat one known-good platen request and collect **new** log stages (`libusb-bulk-in`, `soapht-raw-read`, `soapht-control-body`) to distinguish USB timeout/short reply, incomplete chunk framing and non-2xx response. If protocol mismatch remains, capture direct-Windows known-good SOAPHT USB transcript before altering the vendor command encoding. Keep r13 printing firmware as rollback, no full BIN flash based solely on green CI.

## 2026-09-24 — automatic discovery Wi-Fi-interface defect and staged fix

- Physical evidence: Windows received an mDNS IPP announcement and manual IPP/PCL6 printing succeeded, but Windows and Android automatic printer/scanner installation did not. eSCL actual image still fails 503. mDNS DNS-SD visibility alone is not end-to-end OS discovery or driverless print/scan.
- Source audit (PR #12/#13): `minibox-discoveryd` chooses its advertised IPv4 from `getifaddrs` using substring checks for `sta/wlan/wifi/wl`, **excluding the OpenWrt Wi-Fi client L3 interface name `wwan`**. If `br-lan` or Ethernet appears before `wwan`, discovery announces the management Ethernet IP instead of the Wi-Fi client IP. This is a source-level potential failure: the live interface names and advertised A record must be confirmed on hardware.
- Another source defect: the mDNS listener joins one multicast interface only at process start. On Wi-Fi reconnect/address change it may announce an updated A record without rejoining the corresponding multicast group, causing browse queries to be missed.
- Separate stacked draft PR on PR #13: `fix/discovery-wifi-client-advertisement-20260924`, diagnostic package `0.3.0-r15`. Prefer OpenWrt `wwan`/wlan wireless IPv4 over Ethernet, rejoin mDNS group on address change, add deterministic interface-ranking tests. Preserve no-CUPS/no-usblp and existing print/scan paths; do not advertise nonexistent RAW 9100, WSD, IPP Everywhere or a functioning physical scanner.
- **Remaining platform blockers**: the repository contains a WSD XML parser but the production discovery daemon has no UDP/3702 WSD service or HTTP metadata endpoint. The raw PCL6 bridge lacks IPP Everywhere PWG Raster and PDF conversion; Android automatic driverless printing cannot be claimed. eSCL scanner capabilities/status reachability does not prove Android scanner enrollment or image transfer. Physical confirmation must separately check Windows IPP/manual driver, Windows Add Printer/Scanner browse and Android service behavior.
- NEXT: await CI on r14 diagnostics and r15 Wi-Fi discovery branch independently. Inspect actual Wi-Fi name/IP and mDNS answers on hardware before attributing auto-detection failure solely to the interface selector. Do not flash full BIN. Subsequent platform work requires a truthful WSD/driverless path, not misleading service announcements.

## 2026-09-25 — mandatory Wi-Fi firstboot / factory-reset setup contract

- User requirement is now recorded in `docs/WIFI-FIRST-BOOT-CONTRACT.md` (canonical detailed spec). On first boot / after factory reset, boot as a secure provisioning AP, expose a working local web UI with a Wi-Fi settings tab, scan nearby networks, choose SSID and enter password. Trial STA association and DHCP must leave a recovery AP path if joining fails.
- Following successful join, the **router-assigned Wi-Fi DHCP address** is the MiniBox MFP network identity for web UI, IPP and eSCL; update DNS-SD on lease/reconnection without hard-coded `192.168.55.250`. The HP USB device itself does not obtain a separate IP. Wi-Fi settings must remain editable after setup.
- This must be implemented and verified in the **full firmware image/factory reset defaults**; r13–r15 APK updates and the existing status-only web page do not meet it. The image must bake AP wireless/network/firewall/DHCP/uhttpd setup, test recoverability on AR9330, and avoid accidental lockout or unprotected credential-changing CGI. Do not factory-reset or full-flash the currently working hardware until a recovery path is verified.
- Hardware evidence: current STA `phy0-sta0` is `192.168.55.250`, Ethernet `br-lan` is `192.168.55.251`; this shows the old `sta` matcher already had a Wi-Fi interface to prefer and **does not prove** the r15 wwan fix resolves actual Windows/Android auto-discovery. Preserve separate auto-discovery and scanner 503 tasks.
- NEXT PROGRAMMING TASK: implement minimal authenticated Wi-Fi onboarding state machine and tab, safety/fallback tests and full-image embedded AP defaults. Then validate firstboot/reset, correct/wrong credentials, DHCP IP migration and OS discovery on actual hardware; keep current r13 untouched meanwhile.

## 2026-09-25 — priority order clarified

**Do not implement factory AP / Wi-Fi setup UI yet.** This work is explicitly deferred until all three core MFP goals below are solved and physically verified:

1. Printing works reliably end-to-end.
2. Scanning returns a real image reliably end-to-end.
3. Automatic discovery/addition works on both Windows and Android for the intended printer/scanner flows.

Only after those three are complete may work begin on the previously specified factory-reset AP provisioning flow, Wi-Fi scan/select/password UI, DHCP handoff and recovery behavior.

The detailed future provisioning specification remains in `docs/WIFI-FIRST-BOOT-CONTRACT.md`, but it is **not the current implementation priority**. Do not let AP/UI work delay scanner 503 diagnosis, Windows discovery, Android discovery or print-path completion.
