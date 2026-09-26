#!/usr/bin/env python3
"""Encode real LVGL PPM renders as PNG; generate a self-contained review page."""
import base64
from pathlib import Path
import struct
import zlib

root = Path(__file__).resolve().parents[1] / "build/preview"
def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

cards = []
names = ["ready", "listening", "thinking", "jump-0", "jump-50", "jump-100", "result", "error-5", "error-6", "error-7", "error-8"]
for name in names:
    raw = (root / f"{name}.ppm").read_bytes().split(b"\n", 3)[3]
    data = b"".join(b"\0" + raw[y * 720:(y + 1) * 720] for y in range(320))
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 240, 320, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(data)) + chunk(b"IEND", b"")
    (root / f"{name}.png").write_bytes(png)
    encoded = base64.b64encode(png).decode()
    cards.append(f'<figure><img src="data:image/png;base64,{encoded}"><figcaption>{name}</figcaption></figure>')
(root / "index.html").write_text('''<!doctype html><html lang="zh-CN"><meta charset="utf-8">
<title>蛙声跳跳 · LVGL 实际渲染预览</title><style>
body{background:#e8eddf;color:#254d38;font:16px system-ui;padding:24px}main{display:flex;flex-wrap:wrap;gap:24px}
figure{margin:0}img{width:240px;border-radius:30px;box-shadow:0 8px 25px #254d3820}figcaption{text-align:center;padding:12px}
</style><h1>蛙声跳跳</h1><p>同一份应用代码与 LVGL 9.5 渲染；主机模拟画面，尚未验证实机。</p><main>'''
    + "".join(cards) + "</main></html>")
print(f"UI review: {root / 'index.html'}")
