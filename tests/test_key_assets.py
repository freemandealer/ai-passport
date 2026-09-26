"""Check the licensed font source and complete fixed-text inventory."""
import hashlib
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]

class KeyAssets(unittest.TestCase):
    def test_source(self):
        self.assertEqual(hashlib.sha256((ROOT / 'assets/fonts/NotoSansCJKsc-Regular.otf').read_bytes()).hexdigest(),
                         '2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b')
        self.assertIn('SIL OPEN FONT LICENSE', (ROOT / 'assets/fonts/OFL.txt').read_text())

    def test_inventory(self):
        text = (ROOT / 'main/key_text.h').read_text()
        strings = re.findall(r'^#define TXT_\w+ "([^"]*)"', text, re.M)
        expected = set(''.join(strings)) | set(map(chr, range(32, 127)))
        self.assertEqual(set((ROOT / 'assets/fonts/key-characters.txt').read_text().rstrip('\n')), expected)
        exported = set(int(v,16) for v in re.findall(r'0x([0-9A-F]+)', (ROOT / 'main/key_glyphs.h').read_text()))
        self.assertEqual(exported, set(map(ord,expected)))
        self.assertNotIn(0x9F98, exported)  # Runtime negative probe must really be missing.

if __name__ == '__main__':
    unittest.main()
