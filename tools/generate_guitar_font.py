#!/usr/bin/env python3
"""Reproduce 16/24 px Chinese UI and annotation fonts with lv_font_conv 1.5.3."""
from pathlib import Path
import hashlib
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'assets/fonts/NotoSansCJKsc-Regular.otf'
assert hashlib.sha256(SOURCE.read_bytes()).hexdigest() == '2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b'
converter = ROOT / '.tools/node_modules/.bin/lv_font_conv'
assert subprocess.check_output([str(converter), '--version'], text=True).strip() == '1.5.3'
for size in (16, 24):
    path = ROOT / f'assets/fonts/guitar_font_{size}.c'
    subprocess.run([
        str(converter), '--font', str(SOURCE),
        '--range', '0x20-0x7e,0x4e00-0x9fef,0x3002,0x300a,0x300b,0xff08,0xff09,0xff0c,0xff01,0xff1f',
        '--size', str(size), '--bpp', '2', '--format', 'lvgl', '--no-compress', '--no-kerning',
        '--lv-font-name', f'guitar_font_{size}', '--lv-include', 'lvgl.h', '--output', str(path)
    ], check=True)
    # Keep generation independent of the local checkout path.
    path.write_text(path.read_text().replace(str(ROOT) + '/', '').rstrip() + '\n')
print('Generated guitar_font_16/24: ASCII, basic CJK, supported title/comment punctuation')
