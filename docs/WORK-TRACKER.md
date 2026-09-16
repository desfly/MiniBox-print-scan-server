# MiniBox MFP work tracker

## 2026-09-16

- [x] Build-0105 hardware state recorded: HP LaserJet M1522n printing validated.
- [x] Network discovery service supervised by procd.
- [x] USB scanner probe added for HP 03f0:4517 (`minibox-scan-usb`).
- [x] Scanner supervisor service added (`minibox-scand` + `/etc/init.d/minibox-scan`).
- [x] Add libusb transport core: descriptor enumeration, interface claim, bulk read/write primitives.
- [x] Add guarded scan-session state machine; STREAM remains disabled until a real M1522 handshake is verified.
- [x] Add verified-capture parser plus deterministic synthetic replay fixture.
- [x] Add scanner transport CI; run 35092295215 is green on commit 5a3c19a86500e843b0b33349bc7eef9ad6f81910 (compile with -Werror, replay, handshake guard, state transition all pass).
- [ ] Identify the correct M1522 scanner USB interface/protocol from verified descriptors/source/capture; do not assume the first bulk IN+OUT interface.
- [ ] Implement the verified M1522 scanner USB codec/handshake and image payload reader.
- [ ] Feed decoded scan payload into the network/eSCL-facing service.
- [ ] Integrate scanner binaries/scripts into the OpenWrt package and produce the next flashable build.

Rule: continue from the first unchecked item; do not redo validated printing/discovery work unless a regression requires it. Update this tracker after each verified milestone.
