# MiniBox MFP work tracker

## 2026-09-16

- [x] Build-0105 hardware state recorded: HP LaserJet M1522n printing validated.
- [x] Network discovery service supervised by procd.
- [x] USB scanner probe added for HP 03f0:4517 (`minibox-scan-usb`).
- [x] Scanner supervisor service added (`minibox-scand` + `/etc/init.d/minibox-scan`).
- [ ] Implement the M1522 scanner USB codec/handshake and image payload reader.
- [ ] Feed decoded scan payload into the network/eSCL-facing service.
- [ ] Add scanner transport tests to CI and produce next flashable build.

Rule: continue from the first unchecked item; do not redo validated printing/discovery work unless a regression requires it.
