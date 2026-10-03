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
- Working branch: `integration/minibox-r21-20260925`
- Current HEAD: `16a14ea10631c3fb7d0a44b077514f7d592faf2e`
- Firmware line: OpenWrt 25.12.5 / Gainstrong MiniBox V1.0 / AR9330
- Physically flashed baseline before today's scanner-discovery fix: `ade0c6541f23393a95e1deb0d2f0883fb138bf10`
- Baseline flashed image SHA256: `496b018dd4a04f99efed2de585b4e53d3ffa79802e6f4014291291b315b81db8`
- Baseline image file: `minibox-v1-ade0c654-sysupgrade.bin`
- Baseline `sysupgrade -T`: PASS
- Device returned after flash at Wi-Fi IP `192.168.55.250`.

## Architecture contract

- MiniBox operates primarily as a Wi-Fi client.
- HP LaserJet M1522n MFP USB VID:PID: `03f0:4517`.
- No CUPS.
- No `usblp`.
- Printer and scanner USB access is userspace/libusb.
- Network print protocol: IPP.
- Network scan protocol: eSCL/AirScan style endpoint.
- Automatic discovery uses mDNS/DNS-SD plus Windows WSD where needed.

## 2026-09-25 — physical firmware validation and Windows discovery work

### Firmware and runtime after flashing ade0c654

All three MiniBox daemons are running:
- `minibox-printerd` — running
- `minibox-scand` — running
- `minibox-discoveryd` — running

Health endpoints:
- `http://192.168.55.250:631/health` -> `minibox-printerd ok`
- `http://192.168.55.250:8080/health` -> `minibox-scand ok`

eSCL:
- `/eSCL/ScannerCapabilities` -> HTTP 200
- `/eSCL/ScannerStatus` -> HTTP 200 / Idle
- Capabilities advertise HP LaserJet M1522n @ MiniBox, Platen, ADF, RGB24, Grayscale8, JPEG, 300x300.

### Windows command-environment repair

Bad/stub binaries in `C:\Users\75` were shadowing system tools:
- `powershell`
- `powershell.exe`
- `ssh.exe`
- `curl.exe`

They were moved to `C:\Users\75\CLI_BAD_BACKUP`.

Verified correct tools:
- PowerShell 5.1.19041.6456
- OpenSSH_for_Windows_9.5p1
- curl 8.13.0

Use `scp -O` because MiniBox has no `/usr/libexec/sftp-server`.

### mDNS proven on hardware/network

MiniBox UDP 5353 is listening.

Windows direct multicast capture received MiniBox advertisements from `192.168.55.250:5353` for both:
- `_ipp._tcp.local`
- `_uscan._tcp.local`

IPP TXT observed:
- `rp=ipp/print`
- `ty=HP LaserJet M1522n`
- `product=(HP LaserJet M1522n)`
- `pdl=application/octet-stream,image/pwg-raster`
- `Duplex=F`
- `Scan=T`
- `note=MiniBox MFP`

Scanner TXT observed:
- `is=platen,adf`
- `cs=grayscale,color`
- `pdl=image/jpeg`
- `duplex=F`
- `note=MiniBox MFP`

Conclusion: multicast delivery to Windows is physically proven. Do not repeat basic mDNS reachability unless a regression appears.

### Windows WS-Discovery proven manually

MiniBox UDP 3702 is listening through `minibox-wsdd`.

Direct unicast Windows WS Probe to `192.168.55.250:3702` returned valid SOAP `ProbeMatches` with:
- endpoint `urn:uuid:4d424f58-0000-4000-8000-0cefafcfc53d`
- types `dp:Device p:PrintDeviceType scn:ScanDeviceType`
- XAddr `http://192.168.55.250/cgi-bin/minibox-wsd`

HTTP GET to the WSD endpoint returned expected 405 because metadata requires POST.

SOAP Transfer/Get POST returned HTTP 200 metadata containing:
- FriendlyName `HP LaserJet M1522n @ MiniBox`
- Manufacturer `HP`
- ModelName `HP LaserJet M1522n @ MiniBox`
- ModelNumber `M1522n`
- SerialNumber `0cefafcfc53d`
- PresentationUrl `http://192.168.55.250/`
- DeviceCategory `MFP Printers Scanners`

Conclusion: manual WSD Probe + metadata chain works. This does not itself prove Windows GUI auto-registration.

### Windows print path — physically confirmed

Windows printer queue:
- Name: `\\http://192.168.55.250:631\HP LaserJet M1522n @ MiniBox`
- Driver: `HP Universal Printing PCL 6`
- Port: `http://192.168.55.250:631/ipp/print`

`Win32_Printer.PrintTestPage` returned `ReturnValue 0`, and the user confirmed the physical page printed.

Physical end-to-end path is therefore confirmed:
`Windows -> Wi-Fi -> MiniBox -> IPP :631 -> libusb -> HP M1522n -> paper`.

### Windows scanner registration — current blocker

`Get-PnpDevice -Class Image` shows only:
- old USB HP LJ M1522n Scan entry, status Unknown
- `ROOT\IMAGE\0000`, status OK

WIA COM enumeration also returns only:
- `HP LJ M1522n Scan`

There is no separately registered MiniBox network scanner in Windows.

Important distinction:
- eSCL HTTP works
- mDNS `_uscan._tcp` reaches Windows
- WSD Probe/metadata manually works
- Windows still does not provision/register the network scanner.

### eSCL identity fix — PR #22

Branch:
`fix/windows-escl-identity-20260925`

PR:
`#22 Fix Windows eSCL discovery identity`

Change:
- add stable eSCL DNS-SD `UUID`, derived from the existing WSD identity
- add `adminurl=http://<hostname>.local/`
- only enrich `_uscan._tcp` / `_uscans._tcp`
- preserve truthful existing scan capabilities; no false Mopria certification claim
- add regression coverage.

PR head:
`a755c49841fdd8b65f758250a78d931423e50f14`

All PR checks passed:
- scan-contract — success
- discovery-contract — success
- server-contract — success
- OpenWrt 25.12.5 / MiniBox V1 — success

PR #22 was merged into `integration/minibox-r21-20260925`.

Merge commit / current integration HEAD:
`16a14ea10631c3fb7d0a44b077514f7d592faf2e`

### New firmware build for tomorrow

Existing integration PR #21 automatically picked up the new head and launched CI for `16a14ea...`.

Current status when stopping work tonight:
- scan-contract — success
- discovery-contract — success
- server-contract — success
- OpenWrt 25.12.5 / MiniBox V1 — IN PROGRESS
- full firmware workflow run: `36184892544`

Do not flash tonight. Tomorrow first verify that full OpenWrt build completed successfully and that the artifact's `SOURCE-COMMIT.txt` / provenance matches exactly `16a14ea10631c3fb7d0a44b077514f7d592faf2e`.

## Tomorrow start sequence

1. Check run `36184892544`.
2. If green, fetch artifact and verify exact source commit = `16a14ea10631c3fb7d0a44b077514f7d592faf2e`.
3. Verify artifact checksums and identify the new `squashfs-sysupgrade.bin` / renamed TEST sysupgrade image.
4. Download to Windows.
5. Verify SHA256 on Windows.
6. Copy with:
   `scp -O <new-image> root@192.168.55.250:/tmp/`
7. On MiniBox verify file size and SHA256.
8. Run:
   `sysupgrade -T /tmp/<new-image>`
9. Only if test passes, flash with `sysupgrade /tmp/<new-image>`.
10. After reboot, verify health endpoints, then immediately retest Windows scanner registration before changing any other subsystem.

## Priority order remains

1. Printing reliable end-to-end — CURRENTLY PHYSICALLY PASS.
2. Scanning returns a real image reliably end-to-end.
3. Automatic discovery/addition works on Windows and Android.
4. Only after the three above: factory AP / Wi-Fi setup UI and reset provisioning.

Do not let AP/UI work preempt scanner and OS auto-discovery work.
