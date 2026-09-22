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
- [x] Verify OpenWrt package dependencies/executable installation and produce Build #13 firmware/package artifacts.
- [ ] Capture descriptor map from the physical HP M1522n 03f0:4517 and identify the correct scanner interface/channel; do not guess it.
- [ ] Implement the verified M1522 scanner USB codec/handshake and image payload reader.
- [ ] Feed decoded scan payload into the network/eSCL-facing service.
- [ ] Implement IPP + DNS-SD printer advertisement for Windows/Android automatic discovery; no manual IP/port in the normal flow.
- [ ] Implement eSCL/AirScan + DNS-SD scanner advertisement and web scan fallback for Windows/Android.
- [ ] Add separate Web UI update flows for full firmware `.bin` and the `minibox-mfp` `.apk` module; validate file type/version before install and keep firmware flashing guarded.

## Change / logic trace

- Scanner safety rule: never guess the HP M1522n scanner USB interface or send vendor protocol bytes until a physical descriptor/capture identifies the correct channel. Descriptor inspection is read-only; explicit interface selection is required for any claim/read test.
- Printing architecture remains userspace/libusb: no CUPS and no `usblp`. Build-0105 is the physical printing baseline and must not be regressed while scanner/network work continues.
- Device UX is one logical `HP LaserJet M1522n (MiniBox)` MFP with separate print and scan services. Discovery records must describe only capabilities that have a real listening backend; do not leave phantom IPP/eSCL services or unverified PDL claims enabled in a release build.
- Package integration repair: `minibox-mfp` includes the scanner probe required by `minibox-scand` and the mDNS publisher dependency used by `minibox-discovery`.
- Firmware CI repair: run 35097361421 (Build #12) failed at `Build firmware` after package/dependency changes. Commit c37ee92d18d4f13848de515d7ddeed5dca987bf3 enabled OpenWrt package feeds before configuring/building the live package.
- Verification: run 35109584650 (Build #13), head c37ee92d18d4f13848de515d7ddeed5dca987bf3, completed successfully including firmware build, collection and artifact upload.
- Build #13 artifact `minibox-v1-openwrt-25.12.5-live-package-13`: 20,634,466-byte ZIP, GitHub artifact digest sha256:5d2bbf40fd62ebe992e5b711c5c4b7b53eed0fa06a9fe13c389d35357167a2b8.
- Verified contents: initramfs-kernel.bin 6,649,255 bytes sha256 4f889bb26b345566c5ea6a7ffe5db2f2a335164cb74c236ab05aeecce49baf14; squashfs-factory.bin 16,252,928 bytes sha256 a90d7bcf9a4ccbd75b24870bc4bda637cb8d51b8b295b97aef3cc9660dbb7590; squashfs-sysupgrade.bin 7,012,648 bytes sha256 433af260af45fae43e82738ee6873e41acea0cf2208cb81948061653293b938f; minibox-mfp-0.3.0-r3.apk 9,289 bytes sha256 39be883a219888797dab0e1ef3a63611c8f883641639b39ea14c9ad7218311ea.
- Build #13 is a test candidate, not yet a flash-safe release. Preserve the established safety rule: RAM-boot/test initramfs first; do not infer flash safety solely from a green CI build.
- Web UI update contract added to backlog: `.bin` is full firmware update; `.apk` updates only the MiniBox print/scan module. They must be separate, clearly labelled actions with validation.

## Current next step

1. RAM-boot the verified Build #13 initramfs and confirm Ethernet/Wi-Fi/ART, USB enumeration and the already validated print path are not regressed.
2. On the physical HP M1522n, run the read-only descriptor helper and capture the descriptor map for 03f0:4517; identify the scanner interface from evidence, never by guess.
3. Only after that descriptor evidence, implement and test the M1522 scanner handshake/image reader.
4. Keep network advertisements truthful: do not release phantom IPP/eSCL services before their actual listening backends exist.

Rule: before coding or continuing, read this tracker first and compare it with live repository/CI state. Continue from the first unchecked actionable item; do not redo validated work unless a regression requires it. After every verified change, record the change, reason/logic, commit or CI evidence, next step, and known risk/blocker here.


## 2026-09-21 — Windows IPP hardware finding and remediation (PR #4)

- [x] Hardware: OpenWrt 25.12.5 boots on MiniBox; USB 03f0:4517 detected, usblp unloaded, printerd /health on 631 and scand /health and eSCL metadata/status on 8080 respond.
- [x] Windows: existing HP Standard TCP/IP printer targeted 192.168.55.155 RAW:9100, NOT MiniBox 192.168.55.250:631; failed jobs on that old Windows queue do not prove a new IPP USB-print regression.
- [x] Windows: manual IPP install at http://192.168.55.250:631/ipp/print failed. Automatic discovery and end-to-end printing NOT VALIDATED.
- [x] Code audit: IPP printer-name and printer-make-and-model carried URI value tag 0x45; model lost final byte; operations-supported sent integer 0x21 instead of enum 0x23. Source: src/minibox-ipp/ipp.c main d7b8094543e4f29a2f2ca16618624d2c316acbc6.
- [x] Draft PR #4 on fix/windows-ipp-discovery-20260921: correct IPP attribute tags and lengths, test binary wire fields. Package/firmware needs new build AFTER successful CI. This is NOT yet a validated Windows installer or flash-ready fix.
- [ ] Implement query-response mDNS/DNS-SD including address resolution: current discoveryd emits unsolicited PTR/SRV/TXT once per minute, does not listen/respond to client queries, and does not publish an address record. A log line 'published 2 service(s)' is NOT proof of Windows discovery.
- [ ] Confirm exact IPP capability negotiation and Windows printer driver/data format. application/octet-stream does NOT establish driverless PDF/AirPrint capability.
- [ ] Obtain green PR CI and artifact, review flash recovery requirements, then validate from Windows Add Printer with NO manual IP or port and physically print on HP M1522n.
- [ ] Separate end-to-end scan test: eSCL status/capabilities do NOT confirm USB image capture.

Do not repeat USB/port/health tests without new firmware or a regression. Next actionable code task: mDNS query-answering/address record and tests on PR #4.


### 2026-09-21 continued — PR #4 code and CI evidence
- [x] IPP attributes corrected on the wire: nameWithoutLanguage/textWithoutLanguage/enum tags; complete printer model string, default format and conservative IPP version advertisement. Regression in tests/test-ipp.c.
- [x] IPP printer-uri-supported now uses the current hostname, consistent with discovery target, rather than hard-coded minibox.local. Server contract updated.
- [x] Discovery daemon now binds UDP 5353, joins multicast and handles bounded DNS-SD queries, advertises PTR/SRV/TXT plus a Wi-Fi IPv4 A record. Bounded query-response test added; production hardware verification is STILL REQUIRED.
- [x] Network MFP CI for commit b952ca8b069d04f75c7f269a2fa453636c935809: success, run 35637166400 (includes DNS-SD regression); scan contract run 35637166343 success.
- [ ] Verify all CI checks on final PR #4 HEAD; build artifacts generated from final HEAD, not an older cancelled run.
- [ ] Review service discovery on actual Windows; query-based discovery, device IP/hostname, printer installation and real PCL/PCL6 job on M1522 all remain unverified.
- [ ] If Windows IPP class driver requires an actual driverless PDL, implement truthful format support and conversion, not fake PDF/URF claims. Keep using native libusb; no CUPS/usblp.
- [ ] RAM-boot/rollback gate before any full firmware flash; existing image on device has unverified recovery path and must not be overwritten just because CI passed.


### 2026-09-21 late — field diagnosis after installing r12 on the actual MiniBox
- [x] Hardware: r12 installed with apk, printerd/discoveryd restarted; UDP/5353 listening. Wi-Fi phy0-sta0 192.168.55.250/24; Ethernet br-lan 192.168.55.251/24; Wi-Fi preferred in route; /proc/net/igmp confirms multicast membership E00000FB on phy0-sta0.
- [x] Windows PowerShell sent DNS-SD QU query for _ipp._tcp.local to 224.0.0.251:5353; 391-byte answer arrived from 192.168.55.250. The answer contains HP LaserJet M1522n @ MiniBox, rp=ipp/print, and OpenWrt.local. Thus generic LAN routing/mDNS reachability is no longer the first blocker.
- [ ] On Windows 10 19045 legacy Add Printer wizard, MiniBox still does NOT show up. Microsoft WSDAPI documentation says the legacy Add Printer Wizard uses UDP WS-Discovery for device discovery: https://learn.microsoft.com/en-us/windows/win32/wsdapi/troubleshooting-the-add-printer-wizard . DNS-SD alone cannot be assumed sufficient for this Windows UX. Investigate proper WS-Discovery + WSD printer metadata/transport (or document a demonstrably supported Windows 10 DNS-SD setup) instead of claiming fixed auto-install from mDNS response alone.
- [x] Review RFC 8011 sections 4.1.4 and 5.4: initial operation attributes charset and language are REQUIRED in every IPP response; previous r12 returned printer group directly and bare 9-byte status replies. Corrected all IPP replies, added uri-authentication-supported/uri-security-supported, charset-supported and generated-natural-language-supported, queued-job-count, and strict response layout tests. Code commits 6cc1608aa22766aa9f506db421ac35448f861134, 4d8990a4ed6c34506039c6576fbf72e2e5553aad and 2279887484505748a5e84ce2bf8233f11558a02f. Server-contract CI run 35648800492 success, network contract run 35648800266 success, scan contract 35648800297 success. Firmware build 35648800283 initially pending.
- [ ] Current r12 on device DOES NOT include the new IPP response framing fix. Do not repeat the same Windows discovery wizard or misattribute that to a full test of the new code.
- [ ] Protocol completion remains: IPP operations-required, actual PCL/PCL6 payload acceptance, accurate advertised document formats, Windows 10 auto discovery and physical print, and actual USB scan image.
- [ ] Do NOT full-flash just to deploy these changes: UART remains nonfunctional and full-firmware recovery is unverified; if available, test a new verified APK on the current system, preserving rollback.


### 2026-09-21 — Android acceptance gate (user requirement reaffirmed)
- [ ] Android MUST automatically discover "HP LaserJet M1522n @ MiniBox" over the same Wi-Fi via a compatible Android print service (e.g., Mopria), with no manually entered MiniBox IP address, port, or printer URL. Confirm device and Android print-service version during hardware test. A DNS-SD response alone is insufficient proof of Android discovery.
- [ ] Android MUST print a real document end-to-end to the HP M1522n through IPP and userspace/libusb. Do not falsely advertise PDF, PWG Raster, Apple URF, JPEG, or driverless IPP Everywhere support unless actual format handling/conversion into M1522-accepted printer data is implemented and tested. Current application/octet-stream only is NOT Android driverless printing readiness.
- [ ] Android scanning: discover the _uscan._tcp/eSCL scanner automatically in a compatible scanner app, complete a physical platen scan and retrieve real JPEG/PDF output. As Android has no guaranteed universal built-in scanning flow, provide MiniBox Web scan fallback without manual IP in normal UX (discoverable name/link or a companion app if required).
- [ ] Windows and Android discovery are distinct compatibility paths: implement/test Windows WS-Discovery as needed, while retaining DNS-SD/IPP for Android; do not treat passing one platform as passing the other.
- [ ] Test same-Wi-Fi Android discovery, print/scan, reconnect after MiniBox reboot and changed DHCP address; record observed hardware results separately from simulator/CI contracts.

### 24-hour execution schedule
- [x] Detailed 24-hour sequence, Windows/Android separate gates and explicit acceptance evidence recorded in docs/24H-WINDOWS-ANDROID-MFP-PLAN-2026-09-21.md (commit 8299ad3d699294a4a1744a426baf168033cfe804). Work begins from IPP and platform print PDL, not generic repeated LAN tests.
- [ ] Resume first executable code task: implement and test actual accepted job formats/IPP negotiation; retain accurate Android unsupported state until proven raster or PDF conversion to M1522 PCL/PS exists.
- [ ] Windows WSD full metadata and print integration; do not just advertise a UDP Hello without functioning HTTP endpoints.
