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

Rule: continue from the first unchecked item; do not redo validated printing/discovery work unless a regression requires it. Update this tracker after each verified milestone.
