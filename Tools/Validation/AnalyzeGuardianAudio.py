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
        if width not in (2,3) or stream.getcomptype() != 'NONE':
            raise ValueError(f'{path}: expected uncompressed 16/24-bit PCM')
        raw = stream.readframes(count)
        samples = array('h', raw) if width==2 else [int.from_bytes(raw[i:i+3], 'little', signed=True) for i in range(0,len(raw),3)]
        full_scale = 1 << (8*width-1)
    if width==2 and sys.byteorder != 'little':
        samples.byteswap()
    if not samples or len(samples) != count*channels:
        raise ValueError(f'{path}: empty or truncated audio')
    peak = max(abs(value) for value in samples)/full_scale
    rms = math.sqrt(sum(value*value for value in samples)/len(samples))/full_scale
    clipped = sum(value == -full_scale or value == full_scale-1 for value in samples)
    block = max(channels, int(rate*.05)*channels)
    block_rms = [math.sqrt(sum(v*v for v in samples[start:start+block])/len(samples[start:start+block]))/full_scale
                 for start in range(0, len(samples), block)]
    return dict(path=str(path), duration_seconds=round(count/rate, 4), sample_rate=rate, channels=channels,
        peak_dbfs=dbfs(peak), rms_dbfs=dbfs(rms), max_50ms_rms_dbfs=dbfs(max(block_rms)),
        clipped_samples=clipped, sample_count=len(samples), silent=peak == 0)


def analyze(capture_dir=None):
    paths = sorted((ROOT/'Content/ExternalAssets/Sound/SFX').glob('*.wav'))
    source = [measure(path) for path in paths]
    captures = [measure(path) for path in sorted(capture_dir.glob('*.wav'))] if capture_dir else []
    errors = [row['path']+': silent or clipped' for row in source+captures if row['silent'] or row['clipped_samples']]
    if len(source) != 51:
        errors.append('Expected 51 supplied SFX sources')
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
