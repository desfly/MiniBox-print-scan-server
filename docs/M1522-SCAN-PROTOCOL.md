# HP LaserJet M1522n scan transport

## Confirmed protocol family

HPLIP identifies the LaserJet M1522 family as `HPMUD_SCANTYPE_SOAPHT` (HorseThief) and exposes the scanner transport through the `HP-SOAP-SCAN` channel.

This is important for MiniBox: the USB scan backend must not guess a generic bulk scanner protocol. The implementation target is the HPLIP SOAPHT/HorseThief transaction model for the M1522 family, adapted to our libusb-only runtime.

## MiniBox constraints

- no CUPS
- no `usblp`
- no full HPLIP runtime on the target
- eSCL remains the network-facing API
- USB implementation stays small and device-specific
- do not advertise a completed scan until image bytes have actually been received

## Runtime path

`phone/PC -> eSCL -> minibox-scand -> SOAPHT adapter -> libusb -> 03f0:4517`

The current eSCL server already accepts `ScanJobs`, parses platen/ADF, DPI and color settings, allocates a job URL and exposes `NextDocument`. `NextDocument` must remain unavailable until the SOAPHT adapter can start a hardware scan and return image bytes.

## Next implementation checkpoints

1. Extract the SOAPHT request/response sequence used by HPLIP for `ljm1522`.
2. Map the HPMUD `HP-SOAP-SCAN` logical channel to the M1522 USB interface/endpoints rather than selecting the first bulk endpoint.
3. Implement open/start/read/close as a narrow scan transport API.
4. Add a transcript/mock transport test before enabling physical USB access in `minibox-scand`.
5. Return the actual scanner image MIME type from eSCL `NextDocument`.

## Evidence

Public HPLIP `hpmud.h` documents `HPMUD_SCANTYPE_SOAPHT = 5` with the comment `HorseThief (ie: ljm1522)` and defines `HPMUD_S_SOAP_SCAN` as `HP-SOAP-SCAN`. OpenWrt's HPLIP packaging also shows that hpaio scanning depends on HPLIP transport libraries, reinforcing why MiniBox needs either that transport or a compatible small implementation rather than a guessed raw USB command stream.
