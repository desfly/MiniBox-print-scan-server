# MiniBox MFP work tracker

## 2026-09-16

- [x] Build-0105 hardware state recorded: HP LaserJet M1522n printing validated.
- [x] Network discovery service supervised by procd.
- [x] USB scanner probe added for HP 03f0:4517 (`minibox-scan-usb`).
- [x] Scanner supervisor service added (`minibox-scand` + `/etc/init.d/minibox-scan`).
- [x] Add libusb transport core: descriptor enumeration, interface claim, bulk read/write primitives.
- [x] Add guarded scan-session state machine; STREAM remains disabled until a real M1522 handshake is verified.
- [x] Add verified-capture parser plus deterministic synthetic replay fixture.
- [x] Add scanner transport CI; run 35092295215 green on 5a3c19a86500e843b0b33349bc7eef9ad6f81910.
- [x] Remove unsafe first-bulk-interface selection; commit 4ba60d3c16e127b5dbcafac3fa082eeb094a0696 requires explicit interface selection and run 35092679780 is green.
- [x] Audit repository packaging state: package Makefile was missing; package integration is now being built and validated.
- [x] UX contract locked: expose one logical `HP LaserJet M1522n (MiniBox)` MFP, with separate DNS-SD services for printing and scanning. Windows printer addition must be automatic from Settings without manual IP/port; scanner must be discoverable by compatible scan software. Android printing must appear in the system print flow via IPP discovery; Android scanning must support eSCL/AirScan clients and a MiniBox web UI scan fallback (PDF/JPEG, DPI, color) so basic scanning does not require a dedicated app.
- [x] Restore and package the read-only `minibox-scan-usb` probe required by `minibox-scand`; package release bumped to 0.3.0-r2.
- [ ] Capture descriptor map from the physical HP M1522n 03f0:4517 and identify the correct scanner interface/channel; do not guess it.
- [ ] Implement the verified M1522 scanner USB codec/handshake and image payload reader.
- [ ] Feed decoded scan payload into the network/eSCL-facing service.
- [ ] Implement IPP + DNS-SD printer advertisement for Windows/Android automatic discovery; no manual IP/port in the normal flow.
- [ ] Implement eSCL/AirScan + DNS-SD scanner advertisement and web scan fallback for Windows/Android.
- [ ] Verify OpenWrt package dependencies/executable installation and produce the next flashable build.

## Change / logic trace

- Scanner safety rule: never guess the HP M1522n scanner USB interface or send vendor protocol bytes until a physical descriptor/capture identifies the correct channel. Descriptor inspection is read-only; explicit interface selection is required for any claim/read test.
- Printing architecture remains userspace/libusb: no CUPS and no `usblp`. Build-0105 is the physical printing baseline and must not be regressed while scanner/network work continues.
- Device UX is one logical `HP LaserJet M1522n (MiniBox)` MFP with separate print and scan services. Discovery records must describe only capabilities that have a real listening backend; do not leave phantom IPP/eSCL services or unverified PDL claims enabled in a release build.
- Package integration repair: `minibox-mfp` now includes the scanner probe required by `minibox-scand`; package also needs the mDNS publisher dependency used by `minibox-discovery`.
- Firmware CI repair: run 35097361421 (Build #12) failed at `Build firmware` after package/dependency changes. Commit c37ee92d18d4f13848de515d7ddeed5dca987bf3 changed the workflow to enable/install OpenWrt package feeds before configuring/building the live package and prevents stale queued builds from blocking newer work.
- Verification of that repair: run 35109584650 (Build #13) has successfully completed dependency installation, Build-0104 extraction, OpenWrt clone, package-feed enablement, live package injection, OpenWrt configuration, cache restore, and source download. `Build firmware` is currently in progress; no firmware artifact is claimed until collection/upload succeeds.
- Do not push unrelated package/workflow changes while Build #13 is compiling because package-path commits trigger another full firmware build. Documentation-only tracker updates do not match the firmware workflow package-path trigger.

## Current next step

1. Let Build #13 finish the current `Build firmware` step without invalidating it.
2. If green: verify `Collect firmware and package`, artifact upload, exact BIN/APK contents/sizes, then record the build as the next test candidate.
3. If red: retrieve the exact failing job/log, fix only the verified root cause, and rerun.
4. Physical scanner descriptor capture remains the first hardware-dependent unchecked milestone; protocol implementation stays blocked until that evidence exists.

Rule: before coding or continuing, read this tracker first and compare it with live repository/CI state. Continue from the first unchecked actionable item; do not redo validated work unless a regression requires it. After every verified change, record the change, reason/logic, commit or CI evidence, next step, and known risk/blocker here.
