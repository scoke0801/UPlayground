"""Measure PCM sample levels, not perceived loudness/LUFS. Never auto-normalize assets."""
import argparse
from array import array
import json
import math
from pathlib import Path
import sys
import wave
from RunQA import ROOT


def dbfs(value):
    return round(20*math.log10(value), 3) if value > 0 else None


def measure(path):
    with wave.open(str(path), 'rb') as stream:
        width, channels, rate, count = stream.getsampwidth(), stream.getnchannels(), stream.getframerate(), stream.getnframes()
        if width != 2 or stream.getcomptype() != 'NONE':
            raise ValueError(f'{path}: expected uncompressed 16-bit PCM')
        samples = array('h', stream.readframes(count))
    if sys.byteorder != 'little':
        samples.byteswap()
    if not samples or len(samples) != count*channels:
        raise ValueError(f'{path}: empty or truncated audio')
    peak = max(abs(value) for value in samples)/32768
    rms = math.sqrt(sum(value*value for value in samples)/len(samples))/32768
    clipped = sum(value == -32768 or value == 32767 for value in samples)
    block = max(channels, int(rate*.05)*channels)
    block_rms = [math.sqrt(sum(v*v for v in samples[start:start+block])/len(samples[start:start+block]))/32768
                 for start in range(0, len(samples), block)]
    return dict(path=str(path), duration_seconds=round(count/rate, 4), sample_rate=rate, channels=channels,
        peak_dbfs=dbfs(peak), rms_dbfs=dbfs(rms), max_50ms_rms_dbfs=dbfs(max(block_rms)),
        clipped_samples=clipped, sample_count=len(samples), silent=peak == 0)


def analyze(capture_dir=None):
    paths = sorted((ROOT/'Tools/Art/Guardian').glob('S_*.wav'))
    paths += sorted((ROOT/'Tools/Art/CombatCycle').glob('S_*.wav'))
    source = [measure(path) for path in paths]
    captures = [measure(path) for path in sorted(capture_dir.glob('*.wav'))] if capture_dir else []
    errors = [row['path']+': silent or clipped' for row in source+captures if row['silent'] or row['clipped_samples']]
    if len([row for row in source if 'Guard' in Path(row['path']).stem]) != 5:
        errors.append('Expected five guardian source cues')
    if capture_dir and not captures:
        errors.append('No engine master-output WAV captures')
    warnings = [row['path']+': peak above -1 dBFS' for row in captures if row['peak_dbfs'] is not None and row['peak_dbfs'] > -1]
    return dict(status='FAIL' if errors else ('PASS_WITH_WARNINGS' if warnings else 'PASS'), errors=errors,
        warnings=warnings, sources=source, captures=captures,
        limits=['PCM sample peak/RMS only; no LUFS or true-peak claim',
                'Master output contains the whole scene; sources are not isolated in captures',
                'Relative audibility, masking and speaker/headphone volume require human listening'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture-dir', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    report = analyze(args.capture_dir)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2))
    return int(bool(report['errors']))


if __name__ == '__main__':
    raise SystemExit(main())
