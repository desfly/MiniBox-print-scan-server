# MiniBox print + scan runtime implementation

Target: HP LaserJet M1522n USB `03f0:4517`, exposed automatically over Wi-Fi as an IPP printer and eSCL scanner. Product rules: no CUPS, no usblp; USB ownership stays in userspace/libusb.

## Implemented in runtime-v2

- dependency-free mDNS/DNS-SD discovery daemon
- `_ipp._tcp` service on port 631, resource `/ipp/print`
- `_uscan._tcp` service on port 8080, eSCL root `/eSCL`
- real TCP listeners for printerd and scand
- scanner capability/status endpoints
- deterministic HP M1522 USB VID/PID identity layer
- CI transport and USB identity contract tests

## Next implementation slices

1. IPP binary request decoder: Get-Printer-Attributes, Validate-Job, Print-Job.
2. libusb device enumeration/open/claim for `03f0:4517`; discover bulk interfaces/endpoints from descriptors instead of hard-coding endpoint numbers.
3. Print data path with bounded streaming buffers, timeout/cancel/error propagation and serialized jobs.
4. M1522 scan backend over libusb; map platen/ADF acquisition into eSCL ScanJobs/NextDocument.
5. Generate eSCL capabilities from actual backend abilities; do not advertise unsupported color/duplex/formats.
6. procd services, UCI configuration, firewall bindings limited to LAN/Wi-Fi zone, health/status API.
7. Cross-compile/package into ath79 firmware and hardware-test on Build-0105 validated MiniBox/M1522.

Important: current server endpoints deliberately return 501 for actual Print-Job and ScanJobs. This prevents a green transport test from being mistaken for working physical print/scan.
