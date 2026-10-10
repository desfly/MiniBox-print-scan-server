# HP LaserJet M1522n print protocol reference

This file is the source of truth for the MiniBox print renderer.

Do not change numeric PCL XL values, JPEG parameters, or PJL rendering
parameters by intuition. A change must be justified by either the verified
M1522 capture or a public PCL XL reference implementation.

## Verified capture

Source: `M1522-PRINT-REAL.pcap`

Producer reported by PJL:

`HP Universal Printing PCL 6 (0.3.1584.24923)`

The capture contains a real USB bulk OUT print job to the M1522.

## PJL rendering parameters observed

The verified job contains:

- `PLANESINUSE=1`
- `GRAYSCALE=BLACKONLY`
- `ECONOMODE=OFF`
- `RESOLUTION=600`
- `BITSPERPIXEL=1`
- `ENTER LANGUAGE=PCLXL`

The PCL XL binding line is:

`) HP-PCL XL;2;1;`

Job-accounting and application-identification PJL comments are metadata and
are not part of the raster rendering contract.

## PCL XL session sequence observed

| Meaning | Attribute/operator | Numeric value |
|---|---|---:|
| Units per measure | UnitsPerMeasure | 600 x 600 |
| Measure | Measure | 0 = eInch |
| Error reporting | ErrorReport | 3 = eBackChAndErrPage |
| Begin session | BeginSession | 0x41 |
| Source type | SourceType | 0 = eDefault |
| Data organization | DataOrg | 1 = eBinaryLowByteFirst |
| Open source | OpenDataSource | 0x48 |

These IDs and enum meanings match Ghostscript's public PCL XL tables.

## Verified page/image example from HP UPD

The captured image object is:

| Field | Verified value |
|---|---:|
| MediaSource | 1 = eAutoSelect |
| SimplexPageMode | 0 |
| Orientation | 1 = eLandscape |
| MediaSize | UByteArray `A4` |
| PageScale | 1.0 x 1.0 |
| ColorSpace before image | 2 = eRGB |
| ColorMapping | 0 = eDirectPixel |
| ColorDepth | 2 = e8Bit |
| SourceWidth | 1920 |
| SourceHeight | 1080 |
| DestinationSize | 6454 x 3630 |
| StartLine | 0 |
| BlockHeight | 1080 |
| CompressMode | 2 = eJPEGCompression |
| ReadImage | 0xb1 |
| Embedded length | 506871 bytes |

The embedded payload begins with a normal baseline JFIF JPEG stream.

## JPEG parameters decoded from the capture

The captured JPEG has:

- baseline SOF0
- 1920 x 1080 pixels
- 3 components
- 1x1 sampling for all three components (4:4:4)
- standard IJG/libjpeg quantization tables at quality **95**
- standard Huffman tables
- no progressive coding
- no restart interval
- JFIF density unit 0, X density 1, Y density 1

The quantization tables match libjpeg quality 95 exactly.

Therefore the MiniBox continuous-tone renderer uses libjpeg-compatible
baseline JPEG, quality 95, and 4:4:4 sampling.

## PWG Raster mapping

The network side remains driverless PWG Raster. Mapping into the M1522
PCL XL path is:

| PWG type | PCL XL color space | Depth | Mapping | Data path |
|---|---|---|---|---|
| black_1 | eGray | e1Bit | eDirectPixel | packed 1-bit |
| sgray_8 | accepted for compatibility, not advertised | e8Bit | — | neutral RGB fallback only |
| srgb_8 | eRGB | e8Bit | eDirectPixel | baseline JPEG 4:4:4 |

There is no host-side thresholding or RGB-to-gray conversion for srgb_8.

## Resolution and physical size

The M1522 rendering session is 600 units/inch, matching the verified HP UPD
job. PWG input at either 300 or 600 dpi is accepted.

PCL XL DestinationSize is computed from the PWG physical size:

`destination_pixels = source_pixels * 600 / source_dpi`

This preserves the requested physical page/image size while rendering in the
same 600-unit coordinate system as the HP driver.

## JPEG ReadImage block handling

The verified capture contains one 1080-line JPEG block because the source
image is 1080 lines high.

PCL XL permits ReadImage to cover subsets of the source through StartLine and
BlockHeight, and Ghostscript's reference driver emits JPEG per image block.
MiniBox uses one JPEG ReadImage block for the complete continuous-tone image, with StartLine=0 and BlockHeight=SourceHeight. The value 1080 is not a protocol constant; it is only the SourceHeight of the captured job.

## Explicitly prohibited regressions

Do not reintroduce any of these:

- host-side ordered dithering
- Floyd-Steinberg or another invented halftone
- hard thresholding of continuous-tone data
- srgb_8 to host-side gray conversion
- 300-unit PCL XL session for the M1522 continuous-tone path
- eNoCompression for continuous-tone srgb_8/sgray_8
- arbitrary PCL XL attribute numbers or enum values
- a SetHalftoneMethod operator unless a verified driver stream requires it


## Driverless advertisement policy

The printer is advertised consistently as monochrome across IPP, DNS-SD and
WSD. Its PWG raster types are black_1 and sgray_8. srgb_8 remains accepted by
the renderer for compatibility/testing but is not advertised as a printer
capability. The advertised PWG raster resolution is 600 x 600 dpi.

The human-visible DNS-SD/WSD service name is exactly "M1522n NET". Per-device
uniqueness is carried by the UUID/endpoint identity and must not be appended to
the UI name.

IPP advertises both image/pwg-raster and application/octet-stream, with
image/pwg-raster as the default. DNS-SD pdl mirrors those same formats. IPP and
WSD expose the same IEEE-1284 identity:
MFG:HP;MDL:HP LaserJet M1522n MFP;CMD:PCLXL,PCL;CLS:PRINTER;.
