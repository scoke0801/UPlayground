"""Regression cases for the missing irises and opaque highlight overlays."""
import tempfile
import unittest
from pathlib import Path
from ToonMaterialSource import parse_material


class ToonMaterialSourceTest(unittest.TestCase):
    def parse(self, shader, alpha, cutoff=.5, extra=''):
        guid = 'a' * 32
        text = f'''  m_Name: neutral_name
  m_Shader: {{fileID: 4800000, guid: {guid}, type: 3}}
  m_CustomRenderQueue: -1
    - _Color: {{r: 1, g: 1, b: 1, a: {alpha}}}
    - _Cutoff: {cutoff}
{extra}
'''
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'material.mat'
            path.write_text(text, encoding='utf-8')
            return parse_material(path, {}, {guid: {'name': shader, 'path': 'source.shader'}})

    def test_opaque_irises_ignore_color_alpha_zero(self):
        result = self.parse('lilToon', 0)
        self.assertEqual((result['render_mode'], result['main_opacity'], result['use_base_alpha']), ('opaque', 1, 0))

    def test_transparent_highlights_with_default_queue_and_negative_cutoff(self):
        result = self.parse('Hidden/lilToonTransparent', 1, -.001)
        self.assertTrue(result['transparent'])
        self.assertEqual(result['use_base_alpha'], 1)

    def test_cutout_retains_authored_opacity(self):
        result = self.parse('Hidden/lilToonCutoutOutline', .45, .25)
        self.assertEqual((result['render_mode'], result['main_opacity'], result['cutoff']), ('masked', .45, .25))

    def test_transparent_tears_keep_partial_alpha(self):
        self.assertAlmostEqual(self.parse('Hidden/lilToonTransparent', .1098)['main_opacity'], .1098)


if __name__ == '__main__':
    unittest.main()
