# HP LaserJet M1522n verified scan transcript

The production codec is based on a USBPcap capture made on 2026-09-16 with
the Windows HP scan driver and an HP LaserJet M1522n (`03f0:4517`).

## Verified transport

- USB vendor-specific scan interface, bulk OUT `0x03`, bulk IN `0x83`.
- HTTP/1.1 messages over the `HP-SOAP-SCAN` channel.
- `Transfer-Encoding: chunked`, `Content-Type: application/soap+xml`.
- Scanner response server string: `gSOAP/2.7`.
- Image response: `Content-Type: application/dime`.

## Verified operation sequence

1. `GetScannerElements`
2. `CreateScanJobRequest`
3. `RetrieveImageRequest`

The captured platen job used RGB24 at 200 dpi.  The scanner returned job ID
`2` and a 1700x2338 JFIF image.  The image was split across 164 DIME records;
therefore the runtime removes each DIME header and padding block while
streaming JPEG bytes to the eSCL client.

The Windows application saved the result as PNG, but the scanner itself sent
`image/jpeg` (`jfif`).
