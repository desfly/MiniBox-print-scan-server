# PWG Raster -> PCL XL mapping for MiniBox M1522

This file is the implementation contract for `src/minibox-raster/pwg_to_pcl.c`.
Do not change the converter by intuition: update this table and its tests first.

## Accepted PWG raster formats

| PWG cupsColorSpace | PWG type exposed by IPP | bits/color | bits/pixel | colors | MiniBox output |
|---:|---|---:|---:|---:|---|
| 3 | black_1 (compatibility only) | 1 | 1 | 1 | PCL XL eGray, e1Bit, eDirectPixel |
| 18 | sgray_8 (advertised) | 8 | 8 | 1 | neutral RGB, then PCL XL eRGB/e8Bit/eDirectPixel JPEG |
| 19 | srgb_8 (compatibility only) | 8 | 24 | 3 | PCL XL eRGB, e8Bit, eDirectPixel JPEG |

Only chunky PWG raster (`cupsColorOrder == 0`) is accepted.

## PCL XL enum values

The values below follow Ghostscript's public PCL XL definitions in
`ArtifexSoftware/ghostpdl/base/gdevpxen.h`:

| PCL XL semantic | Value |
|---|---:|
| e1Bit | 0 |
| e4Bit | 1 |
| e8Bit | 2 |
| eDirectPixel | 0 |
| eIndexedPixel | 1 |
| eGray | 1 |
| eRGB | 2 |
| eSRGB | 6 |
| eNoCompression | 0 |
| eRLECompression | 1 |
| eJPEGCompression | 2 |
| eDeltaRowCompression | 3 |

Ghostscript's PCL XL driver (`devices/vector/gdevpx.c`) sends ordinary
continuous-tone monochrome image data as eGray/e8Bit DirectPixel. 1-bit
images use e1Bit separately. MiniBox follows the same separation.

## Raster row mapping

### black_1

PWG input is packed 1-bit data. The M1522 path tested on physical hardware
requires polarity inversion before sending eGray/e1Bit DirectPixel.
No thresholding or gray conversion is performed.

Output bytes per row:

```
ceil(width / 8)
```

### sgray_8

Each PWG gray sample is preserved without thresholding. To follow the verified
HP UPD image path, MiniBox expands it to neutral RGB (R=G=B=gray) and emits
PCL XL eRGB/e8Bit/eDirectPixel with baseline JPEG quality 95, 4:4:4.

```
0x00 -> 00 00 00
...
0xff -> ff ff ff
```

There is no 128 threshold and no software 1-bit halftoning. Tonal levels stay
continuous so the M1522 performs the final monochrome rendering.

Output source bytes per row before JPEG:

```
width * 3
```

### srgb_8

Each chunky RGB pixel is preserved byte-for-byte as three 8-bit components
and sent as PCL XL eRGB/e8Bit DirectPixel.

This mapping is taken from the verified HP Universal Printing PCL 6 capture:
the image path switches ColorSpace to eRGB and uses e8Bit/eDirectPixel before
BeginImage. MiniBox must not collapse srgb_8 to host-side grayscale, because
that bypasses the printer's own monochrome rendering/halftone path.

Output bytes per row:

```
width * 3
```

## PCL XL ReadImage framing

Continuous-tone sgray_8/srgb_8 uses one capture-derived JPEG ReadImage block
for the current source image/page:

- StartLine = 0
- BlockHeight = SourceHeight
- CompressMode = eJPEGCompression
- ReadImage
- embedded data = baseline JFIF JPEG, quality 95, 4:4:4

The 1-bit compatibility path remains uncompressed. Its row payload is padded
to a 4-byte boundary; for the 2480-pixel M1522 row this is 310 useful bytes
plus 2 zero padding bytes = 312 bytes.

## Regression rules

Tests must fail if any of the following regressions return:

- sgray_8 is thresholded to one bit
- srgb_8 is thresholded to one bit
- srgb_8 is converted to host-side grayscale
- srgb_8 is not emitted as eRGB/e8Bit/eDirectPixel
- e8Bit is replaced by e1Bit for continuous-tone data
- black_1 polarity returns to the pre-hardware-test value
- 2480-pixel 1-bit rows stop being padded from 310 to 312 bytes
- PWG PackBits row-repeat decoding changes without a matching contract test


## Verified PCL XL attribute and operator IDs

These numeric IDs are copied from Ghostscript's public PCL XL tables
(`base/gdevpxat.h`, `base/gdevpxop.h`, `base/gdevpxen.h`) and are the
only IDs allowed in the MiniBox image path.

| Attribute | ID |
|---|---:|
| ColorSpace | 3 |
| MediaSize | 37 |
| MediaSource | 38 |
| Orientation | 40 |
| SimplexPageMode | 52 |
| Point | 76 |
| ColorDepth | 98 |
| BlockHeight | 99 |
| ColorMapping | 100 |
| CompressMode | 101 |
| DestinationSize | 103 |
| SourceHeight | 107 |
| SourceWidth | 108 |
| StartLine | 109 |
| DataOrg | 130 |
| Measure | 134 |
| SourceType | 136 |
| UnitsPerMeasure | 137 |
| ErrorReport | 143 |

| Operator | Hex | Decimal |
|---|---:|---:|
| BeginSession | 0x41 | 65 |
| EndSession | 0x42 | 66 |
| BeginPage | 0x43 | 67 |
| EndPage | 0x44 | 68 |
| OpenDataSource | 0x48 | 72 |
| CloseDataSource | 0x49 | 73 |
| SetColorSpace | 0x6a | 106 |
| SetCursor | 0x6b | 107 |
| BeginImage | 0xb0 | 176 |
| ReadImage | 0xb1 | 177 |
| EndImage | 0xb2 | 178 |

Relevant enum values:

- eInch = 0
- eBinaryHighByteFirst = 0
- eBinaryLowByteFirst = 1
- eDefault data source = 0
- eNoReporting = 0
- eBackChannel = 1
- eErrorPage = 2
- eBackChAndErrPage = 3
