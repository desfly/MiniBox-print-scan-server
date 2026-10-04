# PWG Raster -> PCL XL mapping for MiniBox M1522

This file is the implementation contract for `src/minibox-raster/pwg_to_pcl.c`.
Do not change the converter by intuition: update this table and its tests first.

## Accepted PWG raster formats

| PWG cupsColorSpace | PWG type exposed by IPP | bits/color | bits/pixel | colors | MiniBox output |
|---:|---|---:|---:|---:|---|
| 3 | black_1 | 1 | 1 | 1 | PCL XL eGray, e1Bit, eDirectPixel |
| 18 | sgray_8 | 8 | 8 | 1 | PCL XL eGray, e8Bit, eDirectPixel |
| 19 | srgb_8 | 8 | 24 | 3 | convert sRGB to 8-bit gray, then PCL XL eGray, e8Bit, eDirectPixel |

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

Each PWG gray sample is preserved as one PCL XL eGray/e8Bit sample.

```
0x00 = black
...
0xff = white
```

There is no 128 threshold and no software 1-bit halftoning.

Output bytes per row:

```
width
```

### srgb_8

Each chunky RGB pixel is converted to one 8-bit gray sample using fixed
integer Rec.601 luma weights:

```
gray = (77*R + 150*G + 29*B + 128) >> 8
```

The resulting 8-bit gray samples are sent as PCL XL eGray/e8Bit DirectPixel.
They are not thresholded to 1 bit.

Output bytes per row:

```
width
```

## PCL XL ReadImage framing

For each emitted row MiniBox currently uses:

- StartLine = physical row index
- BlockHeight = 1
- CompressMode = eNoCompression
- ReadImage
- embedded data length = row payload rounded up to a 4-byte boundary
- zero-valued alignment bytes after the useful row payload

The 4-byte row alignment follows the behavior used by Ghostscript's PCL XL
writer and is already proven on the physical M1522.

## Regression rules

Tests must fail if any of the following regressions return:

- sgray_8 is thresholded to one bit
- srgb_8 is thresholded to one bit
- e8Bit is replaced by e1Bit for continuous-tone data
- black_1 polarity returns to the pre-hardware-test value
- 2480-pixel 1-bit rows stop being padded from 310 to 312 bytes
- PWG PackBits row-repeat decoding changes without a matching contract test
