"""Reject misleading GPU conclusions from incomplete or contaminated evidence."""
import csv
from pathlib import Path
import tempfile
import unittest
from RunToonPerformance import csv_complete, metrics, summarize
from ExportToonCPU import longest_frames


class PerformanceEvidenceTest(unittest.TestCase):
    def test_unfinished_csv_cannot_pass_even_with_enough_frames(self):
        with tempfile.TemporaryDirectory() as folder:
            out, runtime = self.fixture(folder)
            path = Path(runtime['phases'][-1]['csv'])
            path.write_text('\n'.join(path.read_text().splitlines()[:-1]))
            self.assertFalse(csv_complete(path))
            report = summarize(out, runtime, [{'competitors': []}], 0, False)
            self.assertEqual(report['status'], 'FAIL')
            self.assertIsNone(report['comparisons']['no_post']['bracketed_gpu_saving_ms'])

    def test_frame_budget_counts_exclude_invalid_rows_and_use_strict_threshold(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)/'frames.csv'
            path.write_text('FrameTime,GPUTime\n10,5\n20,5\n40,5\n50,5\n101,5\n'
                            '0,5\n-1,5\nnan,5\ninf,5\n[Metadata],invalid\n')
            frame = metrics(path)['FrameTime']
            self.assertEqual(frame['samples'], 5)
            self.assertEqual([r['count'] for r in frame['budget_exceedances']], [4, 3, 1, 1])
            self.assertEqual([r['percent'] for r in frame['budget_exceedances']], [80, 60, 20, 20])
            self.assertEqual(frame['p99'], 101)

    def test_trace_boundary_frames_are_not_runtime_hitches(self):
        def event(name, start, end):
            return dict(TimerName=name, StartTime=str(start), EndTime=str(end), Duration=str(end-start))
        events = [event('FEngineLoop::Tick', 9, 10.5),
                  event('FEngineLoop::Tick', 11, 11.02), event('DirectoryWatch', 11.001, 11.015),
                  event('FEngineLoop::Tick', 19.99, 22), event('CSVShutdown', 20.1, 21.9)]
        result = longest_frames(events, 10, 20)
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0]['frame']['StartTime'], '11')
        self.assertEqual([s['TimerName'] for s in result[0]['scopes']], ['DirectoryWatch'])

    def fixture(self, folder, repeats=(10, 10, 10), gpu=True):
        root = Path(folder)
        out = root/'out'
        out.mkdir()
        phases = []
        for name, value in zip(['full_a', 'no_post', 'full_b', 'full_c'], [repeats[0], 8, repeats[1], repeats[2]]):
            path = root/(name+'.csv')
            with path.open('w', newline='') as stream:
                writer = csv.writer(stream)
                writer.writerow(['FrameTime', 'GPUTime', 'GameThreadTime', 'RenderThreadTime'])
                writer.writerows([[12, value if gpu else 0, 2, 3]]*150)
                writer.writerow(['[HasHeaderRowAtEnd]', '1', '[endtimestamp]', '1',
                                 '[captureduration]', '12', '[csvmaxfilebytesreached]', '0'])
            phases.append(dict(name=name, csv=str(path)))
        return out, dict(status='PASS', phases=phases, phase_plan=phases,
                         motion_checks=[dict(phase=p['name'], bone_motion=True) for p in phases])

    def test_bracketed_difference(self):
        with tempfile.TemporaryDirectory() as folder:
            out, runtime = self.fixture(folder)
            report = summarize(out, runtime, [{'competitors': []}], 0, False)
            self.assertEqual(report['status'], 'CHARACTERIZED')
            self.assertEqual(report['comparisons']['no_post']['bracketed_gpu_saving_ms'], 2)

    def test_drift_suppresses_attribution(self):
        with tempfile.TemporaryDirectory() as folder:
            out, runtime = self.fixture(folder, repeats=(10, 15, 20))
            report = summarize(out, runtime, [{'competitors': []}], 0, False)
            self.assertEqual(report['status'], 'UNSTABLE_PERFORMANCE_SAMPLE')
            self.assertIsNone(report['comparisons']['no_post']['bracketed_gpu_saving_ms'])

    def test_competing_editor_rejects_sample(self):
        with tempfile.TemporaryDirectory() as folder:
            out, runtime = self.fixture(folder)
            report = summarize(out, runtime, [{'competitors': [{'pid': 123}]}], 0, False)
            self.assertEqual(report['status'], 'FAIL')
            self.assertIsNone(report['comparisons']['no_post']['bracketed_gpu_saving_ms'])

    def test_zero_gpu_and_incomplete_sequence_fail(self):
        with tempfile.TemporaryDirectory() as folder:
            out, runtime = self.fixture(folder, gpu=False)
            runtime['phase_plan'] = runtime['phase_plan']+[{'name': 'missing'}]
            report = summarize(out, runtime, [{'competitors': []}], 0, False)
            self.assertEqual(report['status'], 'FAIL')
            self.assertGreaterEqual(len(report['errors']), 2)

    def test_missing_monitor_fails(self):
        with tempfile.TemporaryDirectory() as folder:
            out, runtime = self.fixture(folder)
            self.assertEqual(summarize(out, runtime, [], 0, False)['status'], 'FAIL')

    def test_static_fixture_is_not_an_animated_benchmark(self):
        with tempfile.TemporaryDirectory() as folder:
            out, runtime = self.fixture(folder)
            runtime['motion_checks'] = []
            self.assertEqual(summarize(out, runtime, [{'competitors': []}], 0, False)['status'], 'FAIL')


if __name__ == '__main__':
    unittest.main()
