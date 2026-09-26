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

### Sonic Key Radar fonts

[Noto Sans CJK SC Regular](https://github.com/notofonts/noto-cjk/tree/f8d157532fbfaeda587e826d4cd5b21a49186f7c/Sans)
is vendored as [`fonts/NotoSansCJKsc-Regular.otf`](fonts/NotoSansCJKsc-Regular.otf)
under the [SIL Open Font License](fonts/OFL.txt). Source SHA-256:
`2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b`.

`fonts/key_font_14.c` and `fonts/key_font_20.c` are uncompressed 4 bpp subsets,
compiled by `main/CMakeLists.txt`; the OTF is for reproducibility and is not embedded.
`main/key_text.h` owns fixed strings; `fonts/key-characters.txt` and
`main/key_glyphs.h` record required glyphs plus printable ASCII. Regenerate with:

```sh
npm install --prefix .tools --no-audit --no-fund lv_font_conv@1.5.3
python3 tools/generate_key_fonts.py
```

The generator verifies converter version 1.5.3, selects sizes 14/20 and passes
`--symbols`, `--bpp 4 --format lvgl --no-compress --no-kerning --lv-include lvgl.h`.
Source hash/inventory tests run in the static gate. The real LVGL renderer checks
all glyph descriptors, a missing-glyph negative case, actual widget bindings and
text bounds. Physical Chinese output remains unverified until device testing.
The application's radar geometry is code-native in `main/key_ui.c`, under the
project MIT license. See [Sonic Key Radar](../docs/key-radar.md).

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
