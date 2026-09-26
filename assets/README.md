<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Assets

This directory stores reusable fonts, images, music, and sound effects, organized by asset type.

Keep each asset in the matching subdirectory and document its destination, naming, integration method, and source/license. Do not mix binary assets with Markdown documentation.

## Fonts

Store reusable font files and generated font sources in `fonts/`.

- Use descriptive names that include the family, weight, size, and format when relevant.
- Document the source, license, character range, conversion command, and expected destination.
- Check Flash and internal-RAM impact before adding a font; the ESP32-C3 has no PSRAM.
- Do not commit fonts whose license does not permit redistribution.

### GREEN ROOM Chinese font

The existing local project's licensed [Noto Sans CJK SC Regular](https://github.com/notofonts/noto-cjk/tree/f8d157532fbfaeda587e826d4cd5b21a49186f7c/Sans)
is reused at [`fonts/NotoSansCJKsc-Regular.otf`](fonts/NotoSansCJKsc-Regular.otf),
under the [SIL Open Font License](fonts/OFL.txt). Source SHA-256:
`2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b`.

[`fonts/guitar_font_16.c`](fonts/guitar_font_16.c) and
[`fonts/guitar_font_24.c`](fonts/guitar_font_24.c) are 16/24 px, uncompressed 2 bpp
fonts compiled by `main/CMakeLists.txt`. The source OTF is not embedded. It covers
ASCII U+0020–U+007E, 20,976 CJK characters U+4E00–U+9FEF and eight punctuation
marks specified in [the application guide](../docs/guitar-score.md). The
generated bitmap exceeds 1 MiB, so `CONFIG_LV_FONT_FMT_TXT_LARGE=y` is required;
font data stays in Flash. User song titles use the 16 px font; large inline annotations
use the 24 px font as the chord label fallback. Host rendering verifies descriptors, a missing-glyph negative
case, layout and the 24 KiB LVGL pool. Physical rendering still needs a device.

Reproduce with the pinned converter (no global installation):

```sh
npm install --prefix .tools --no-audit --no-fund lv_font_conv@1.5.3
python3 tools/generate_guitar_font.py
```

The generator verifies source hash and converter version, then passes the exact
range, `--size 16` / `--size 24`, `--bpp 2 --format lvgl --no-compress --no-kerning`. Latin chord
names and numbers use LVGL's Montserrat fonts. Chord diagrams and the rest of
the interface are code-native assets under the project license.

## Images

Store reusable source images and generated display assets in `images/`.

| File | Dimensions and format | Use and source |
| --- | --- | --- |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160, JPEG | Product hero image embedded in both project README files to foreground AI Passport and its open, maker-oriented identity. |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724, PNG RGBA | Optional technical infographic retained as a reference asset; it is no longer used as the homepage hero. Generated for this repository with the built-in image generation tool on 2026-09-17; the six labels and values were checked against the documented hardware contract. |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336, PNG RGBA | Transparent black wordmark extracted from the repository's original `images/logo.png`; embedded in both project README files for light backgrounds. |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336, PNG RGBA | White version of the extracted wordmark, used by the README `<picture>` element when GitHub is in dark mode. |

- Use descriptive names and document dimensions, pixel format, conversion steps, and destination.
- Prefer formats suitable for the 240 × 320 RGB565 display and account for Flash and internal RAM.
- Preserve editable sources where licensing permits, and record the source and license.
- Never commit device QR secrets, credentials, or personal data in images.

## Music and sound effects

Store reusable music and sound-effect sources in `music/`.

- Document the source, license, sample rate, bit depth, channels, conversion command, and destination.
- Prefer 16 kHz, 16-bit mono PCM when it matches the current BSP audio path.
- Check Flash and internal-RAM cost before embedding audio; stream or chunk long recordings.
- Do not commit media without redistribution permission.
