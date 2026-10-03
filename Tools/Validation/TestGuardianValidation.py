"""Evidence parsers must reject missing/silent captures and exclude CSV metadata."""
from array import array
import math
from pathlib import Path
import tempfile
import unittest
import wave
from AnalyzeGuardianAudio import measure
from RunGuardianSoak import summarize_csv


class GuardianEvidenceTests(unittest.TestCase):
    def test_csv_footer_and_invalid_frames_are_excluded(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'capture.csv'
            path.write_text('FrameTime,GPUTime,Memory/PhysicalUsedMB\n10,4,1024\n20,6,1040\nNaN,9,9000\n[Metadata],,\n', encoding='utf-8')
            result = summarize_csv(path)
            self.assertEqual(result['frames'], 2)
            self.assertEqual(result['metrics']['FrameTime']['p95'], 20)
            self.assertEqual(result['metrics']['Memory/PhysicalUsedMB']['unit'], 'MB')
            self.assertEqual(result['metrics']['GPUTime']['mean'], 5)

    def test_csv_with_no_frame_evidence_stays_empty(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'capture.csv'
            path.write_text('FrameTime,GPUTime\n[Metadata],0\ninf,0\n', encoding='utf-8')
            self.assertEqual(summarize_csv(path), dict(frames=0, metrics={}))

    def test_ue58_physical_memory_column_has_megabyte_units(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'capture.csv'
            path.write_text('FrameTime,PhysicalUsedMB,GPUMem/LocalUsedMB\n16,2048,1024\n', encoding='utf-8')
            metrics = summarize_csv(path)['metrics']
            self.assertEqual(metrics['Memory/PhysicalUsedMB']['mean'], 2048)
            self.assertEqual(metrics['GPUMem/LocalUsedMB']['unit'], 'MB')

    def audio(self, values):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'capture.wav'
            with wave.open(str(path), 'wb') as stream:
                stream.setnchannels(1)
                stream.setsampwidth(2)
                stream.setframerate(44100)
                stream.writeframes(array('h', values).tobytes())
            return measure(path)

    def test_silence_is_not_passing_audio_evidence(self):
        report = self.audio([0]*100)
        self.assertTrue(report['silent'])
        self.assertIsNone(report['peak_dbfs'])

    def test_both_pcm_rails_count_as_clipping(self):
        report = self.audio([-32768, 32767, 0])
        self.assertEqual(report['clipped_samples'], 2)
        self.assertEqual(report['peak_dbfs'], 0)

    def test_known_half_scale_level(self):
        report = self.audio([16384, -16384]*100)
        self.assertAlmostEqual(report['peak_dbfs'], 20*math.log10(.5), places=3)
        self.assertEqual(report['clipped_samples'], 0)


if __name__ == '__main__':
    unittest.main()
