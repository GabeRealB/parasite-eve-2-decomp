import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from gen_overlay_configs import subsegments


class SharedTextHeaderTests(unittest.TestCase):
    def layout(self, spans):
        return subsegments(
            'actor_test', bytes(0x40), (0x10, 0x40), spans,
            [{'start': '0x4', 'unit': 'first'},
             {'start': '0x8', 'unit': 'actor_test_header_2'}],
            None, [], [],
        )

    def test_single_shared_span_keeps_header_in_asm(self):
        result = self.layout([{'start': '0x10', 'end': '0x40', 'unit': 'first'}])
        self.assertIn('[0x0, rodata, actor_test_header]', result)
        self.assertIn('[0x4, .rodata, lib/first]', result)
        self.assertIn('[0x8, rodata, actor_test_header_2]', result)

    def test_adjacent_shared_spans_keep_header_in_asm(self):
        result = self.layout([
            {'start': '0x10', 'end': '0x20', 'unit': 'first'},
            {'start': '0x20', 'end': '0x30', 'unit': 'middle'},
            {'start': '0x30', 'end': '0x40', 'unit': 'last'},
        ])
        self.assertIn('[0x0, rodata, actor_test_header]', result)
        self.assertIn('[0x4, .rodata, lib/first]', result)
        self.assertIn('[0x8, rodata, actor_test_header_2]', result)
        self.assertNotIn('c, actor_test/', result)

    def test_gap_retains_overlay_local_code_and_rodata(self):
        result = self.layout([
            {'start': '0x10', 'end': '0x20', 'unit': 'first'},
            {'start': '0x30', 'end': '0x40', 'unit': 'last'},
        ])
        self.assertIn('[0x0, .rodata, actor_test/actor_test]', result)
        self.assertIn('[0x20, c, actor_test/actor_test]', result)


if __name__ == '__main__':
    unittest.main()
