# MiniBox print + scan server — WORK TRACKER

This file is the canonical work checkpoint for ChatGPT-assisted development.

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
- Verified source HEAD before tracker creation: `ba6bf53aa7ef965f0164b69cc1d968ad42610a9d`
- HEAD message: `build: make Build-0106 consume current runtime-v2 source`
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

From Windows, these endpoints have returned valid responses:

- `http://192.168.55.250:8080/eSCL/ScannerCapabilities`
- `http://192.168.55.250:8080/eSCL/ScannerStatus`

Scanner status returned `Idle`.

Therefore this layer is proven:

`Windows -> Wi-Fi -> MiniBox -> eSCL :8080`

### Scanner USB layer

M1522 scanner transport identified:

- interface 0: SOAPHT `ff/02/01`
- bulk OUT: `0x03`
- bulk IN: `0x83`
- interrupt IN: `0x84`
- printer interface: interface 1, `07/01/02`

Physical command `/usr/sbin/minibox-scan-diag --claim` succeeded on r10:

- SOAPHT claim OK
- SOAPHT release OK
- no scan command/payload sent

Thus userspace libusb can safely own the physical scanner interface.

## Current CI state

For verified HEAD `ba6bf53aa7ef965f0164b69cc1d968ad42610a9d`, GitHub reported four workflow runs. Contract workflows observed include:

- `MiniBox network MFP contract` — success
- `MiniBox print scan server contract` — success

Build-0106 workflow was changed at `ba6bf53` to consume the current `runtime-v2` package/src instead of stale package source.

## Current scanner blocker

Real M1522 image acquisition is NOT implemented.

The production code intentionally keeps:

`minibox_soapht_codec = 0`

This is fail-closed: no guessed/unverified HorseThief/SOAPHT scan command may be sent to the physical MFP.

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

## Existing reverse-engineering tool

`tools/extract_soapht_usbpcap.py`

Existing commit: `c1af75f6f54bbb9fde29de7ba2b8a734f21867e0`

Purpose: parse a real Windows USBPcap capture, filter SOAPHT endpoints `0x03 OUT` / `0x83 IN`, preserve chronology, and extract input/output streams.

## Hardware evidence still needed

Capture one known-good direct USB scan from Windows with USBPcap/Wireshark:

- M1522 directly connected to Windows laptop by USB
- platen
- one page
- 300 dpi
- color or grayscale
- capture starts before scan and stops immediately after scan
- save `.pcapng`

Then run the existing extractor and identify verified:

`start_scan -> settings -> image stream -> end_page -> end_scan`

Only verified protocol evidence may be used to implement/enable the SOAPHT codec.

## Remaining print/discovery verification

Not yet physically confirmed:

- Windows automatic printer discovery after persistent mDNS fix
- Windows automatic scanner discovery

Still to improve:

- normal IPP job framing/finish robustness
- Windows IPP required attributes/compatibility
- discovery TXT must not advertise PDF unless runtime can actually convert/accept it

## Truth table

- MiniBox sees M1522 USB: YES
- Physical network print: YES
- IPP endpoint: YES
- Correct PJL/UEL close: YES
- Persistent mDNS code: YES
- eSCL capabilities from Windows: YES
- eSCL status from Windows: YES (`Idle`)
- SOAPHT interface/endpoints identified: YES
- libusb scanner claim/release on hardware: YES
- Real scan movement/image: NO
- Verified SOAPHT codec: NO
- Automatic Windows printer discovery: NOT YET PHYSICALLY CONFIRMED
- Automatic Windows scanner discovery: NOT YET PHYSICALLY CONFIRMED

## NEXT STEP

1. Treat this file + current GitHub HEAD/CI as the starting point for every session.
2. Do not create another scanner transport or redo eSCL/libusb claim work.
3. Obtain or locate a real M1522 USB scan capture (`.pcapng`).
4. Analyze it with `tools/extract_soapht_usbpcap.py`.
5. Derive the SOAPHT/HorseThief codec only from captured/verified protocol evidence.
6. Add codec tests/fixtures first, then implement `start/read_image/end_page/end_scan`.
7. Keep fail-closed until those tests and Build CI are green.
8. Update this tracker immediately after each completed step.