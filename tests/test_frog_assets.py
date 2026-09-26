"""Catch stale Chinese subsets before an ESP-IDF or native LVGL build."""
from pathlib import Path
import hashlib
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]

class FrogAssets(unittest.TestCase):
    def test_inventory_matches_all_ui_text(self):
        text = (ROOT / "main/frog_text.h").read_text()
        strings = re.findall(r'^#define TXT_\w+ "([^"]*)"', text, re.M)
        expected = set("".join(strings)) | set(chr(i) for i in range(32, 127))
        inventory = set((ROOT / "assets/fonts/frog-characters.txt").read_text().rstrip("\n"))
        self.assertEqual(expected, inventory)
        header = (ROOT / "main/frog_glyphs.h").read_text()
        self.assertEqual({ord(c) for c in expected}, {int(c, 16) for c in re.findall(r"0x([0-9A-F]+)", header)})
        for size in (14, 20):
            font = (ROOT / f"assets/fonts/frog_font_{size}.c").read_text()
            points = {int(c, 16) for c in re.findall(r"/\* U\+([0-9A-Fa-f]+)", font)}
            self.assertTrue({ord(c) for c in expected} <= points)

    def test_source_font_identity(self):
        font = ROOT / "assets/fonts/NotoSansCJKsc-Regular.otf"
        self.assertEqual(hashlib.sha256(font.read_bytes()).hexdigest(),
                         "2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b")

if __name__ == "__main__":
    unittest.main()
