# START HERE TOMORROW — MiniBox print + scan server

Checkpoint saved: 2026-09-15 23:51 (Kyiv, UTC+3)
Repository: `desfly/MiniBox-print-scan-server`
Working branch: `minibox-network-mfp-runtime-v2`

## 1. Hardware / target

- MiniBox V1.0 / AR9330 / OpenWrt 25.12.5
- HP LaserJet M1522n MFP
- USB VID:PID: `03f0:4517`
- MiniBox Wi-Fi IP: `192.168.55.250`
- No CUPS
- No usblp
- Printing and scanning via libusb
- Network services over Wi-Fi

## 2. Proven printing state

Physical network printing is confirmed end-to-end:

`Windows -> Wi-Fi -> MiniBox -> IPP :631 -> libusb -> HP M1522n`

Test page physically printed:

`MINIBOX M1522 NETWORK TEST`

IPP endpoint:

`ipp://192.168.55.250/ipp/print`

The first primitive raw PCL test left the MFP display on `Печать документа`.
A proper PJL/UEL close sequence cleared it immediately, proving the host USB path was fine and the issue was print-job framing.

Important remaining print work:

- ensure normal jobs are properly framed/finished
- improve Windows IPP compatibility / required attributes
- verify real automatic printer discovery after persistent mDNS fix
- current IPP code is still minimal
- discovery TXT currently advertises PDF while the raw backend does not yet provide PDF conversion; this mismatch must be fixed

## 3. Discovery state

`discoveryd` originally exited after one mDNS announcement. This was fixed so it stays alive and re-announces periodically.

Key commits:

- `9a6f33f3263eb15130e1f19bafbc6fe3af4d18da` — persistent discovery loop
- `9a5d9cd592249fe43af6eeb34c2c080a577f81c5` — r8 bump
- test contract was updated because the old test expected discoveryd to exit

By r10 all 4 GitHub Actions were green.

Real Windows auto-discovery is NOT yet physically confirmed. That must still be tested.

## 4. r10 runtime installed on MiniBox

Installed package:

`minibox-mfp-0.3.0-r10.apk`

Installed successfully over r7:

`0.3.0-r7 -> 0.3.0-r10`

r10 build commit:

`9cf0a79a539e47c657a240eaf6ec8dcece1e3e1f`

APK details:

- size: `15758 bytes`
- SHA256: `eb761ee0c39d9667c1ce189d75e4f4f3061525e37053f8da438878a1d176ce98`

Artifact run:

`35020884433`

Artifact name:

`minibox-mfp-runtime-v2-9cf0a79a539e47c657a240eaf6ec8dcece1e3e1f`

## 5. Proven scanner network state

From Windows browser, MiniBox eSCL works:

`http://192.168.55.250:8080/eSCL/ScannerCapabilities`

Returned:

- `HP LaserJet M1522n @ MiniBox`
- `Platen`
- `Adf`

Scanner status endpoint:

`http://192.168.55.250:8080/eSCL/ScannerStatus`

Returned:

`<scan:State>Idle</scan:State>`

Therefore this path is confirmed:

`Windows -> Wi-Fi -> MiniBox -> eSCL :8080`

## 6. Proven scanner USB state

USB enumeration on MiniBox confirmed:

`1-1 03f0:4517 Hewlett-Packard HP LaserJet M1522n MFP`

`minibox-scan-diag` identified:

- interface 0 = SOAPHT `ff/02/01`
- OUT bulk = `0x03`
- IN bulk = `0x83`
- interrupt IN = `0x84`
- printer interface = interface 1, class `07/01/02`

With r10 installed, this real hardware test passed:

`/usr/sbin/minibox-scan-diag --claim`

Result:

`SOAPHT claim OK: if=0 bulk_out=0x03 bulk_in=0x83`

`SOAPHT release OK; no scan command or payload was sent.`

This proves MiniBox/libusb can claim and release the physical scanner interface safely.

## 7. Main remaining scanner blocker

Real image acquisition is NOT implemented yet.

Current code intentionally has:

`minibox_soapht_codec = 0`

so production scanning fails closed before sending unverified commands.

The old open HPLIP source confirms M1522 uses:

`SOAPHT / HorseThief`

but the low-level implementation was in restricted `bb_soapht.so`.

Known ABI from old HPLIP wrapper:

- `bb_open`
- `bb_close`
- `bb_get_parameters`
- `bb_is_paper_in_adf`
- `bb_start_scan`
- `bb_get_image_data`
- `bb_end_page`
- `bb_end_scan`

Important HPLIP note for M1522 ADF:

If multiple sheets are loaded, all pages must be scanned as one complete scan job; stopping early may jam the ADF.

## 8. USBPcap plan for real SOAPHT reverse engineering

Best evidence source: one USB capture of a real scan with M1522 connected directly to Windows.

Need capture:

- Platen
- one page
- 300 dpi
- Color or Grayscale
- start USBPcap/Wireshark before scan
- stop capture immediately after scan
- save as `.pcapng`

Example filename:

`m1522-scan-300dpi.pcapng`

Goal is to extract USB traffic on:

- `0x03 OUT`
- `0x83 IN`

and recover:

`start_scan -> settings -> image stream -> end_page -> end_scan`

The MFP is in another room, so do NOT move it. If capture becomes necessary, bring a laptop to the MFP for about 10 minutes and connect directly by USB there.

## 9. USBPcap parser already added

Added to repo:

`tools/extract_soapht_usbpcap.py`

Commit:

`c1af75f6f54bbb9fde29de7ba2b8a734f21867e0`

Purpose:

- filter SOAPHT traffic
- extract `0x03 OUT` and `0x83 IN`
- produce chronological TSV
- produce `soapht-out.bin`
- produce `soapht-in.bin`

As soon as a `.pcapng` is available, analyze it with this tool first.

## 10. Windows command notes

Windows CMD prompt looks like:

`C:\Users\75>`

MiniBox shell looks like:

`root@OpenWrt:~#`

Do not mix commands between them.

Legacy SCP is required because MiniBox has no sftp-server:

`scp -O ... root@192.168.55.250:/tmp/`

If normal `ssh` on Windows behaves oddly, use:

`C:\Windows\System32\OpenSSH\ssh.exe root@192.168.55.250`

MiniBox does not have Python or lsusb installed by default.

## 11. First tasks tomorrow

1. Check branch `minibox-network-mfp-runtime-v2` and open this file first.
2. Verify r10 services on MiniBox still running.
3. Test actual Windows automatic printer discovery now that persistent mDNS is installed.
4. Test actual Windows automatic scanner discovery if visible.
5. Continue reverse engineering SOAPHT from open HPLIP sources.
6. If low-level command bytes remain unknown, capture one Windows USB scan with USBPcap and feed the `.pcapng` to `tools/extract_soapht_usbpcap.py`.
7. Implement real `start/read_image/end_page/finish` codec only from verified protocol evidence.

## 12. Current truth table

- MiniBox sees M1522 USB: YES
- Physical network print: YES
- IPP endpoint: YES
- Proper print job close: proven with PJL/UEL test
- eSCL capabilities from Windows: YES
- eSCL status from Windows: YES (`Idle`)
- SOAPHT interface identification: YES
- libusb scanner claim/release: YES
- Real scan movement/image from M1522: NOT YET
- Automatic Windows printer discovery: NOT YET CONFIRMED
- Automatic Windows scanner discovery: NOT YET CONFIRMED

Start from here. Do not re-prove already confirmed layers unless a regression appears.
