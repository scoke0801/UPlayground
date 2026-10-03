"""Original UI sound design, reproducible with Python's standard library only.

48 kHz stereo PCM16. No recordings, external samples or third-party music.
The D/A/E motif connects a glass/metal reveal, a tactile latch and a resolved
victory phrase. Short early reflections supply width without phase inversion.
"""
import array
import hashlib
import json
import math
from pathlib import Path
import random
import sys
import wave

RATE = 48000
OUT = Path(__file__).resolve().parent
TAU = math.tau


def stereo(seconds):
    return [[0.0] * round(seconds * RATE) for _ in range(2)]


def layer(dst, mono, start=0, gain=1, pan=0):
    offset = round(start * RATE)
    gains = (math.sqrt((1-pan)/2)*gain, math.sqrt((1+pan)/2)*gain)
    for channel in range(2):
        for i, value in enumerate(mono[:len(dst[channel])-offset]):
            dst[channel][offset+i] += value * gains[channel]


def bell(frequency, duration, decay, brightness=.3):
    # Damped, slightly inharmonic metal modes with a softer pitched body.
    modes = ((1, 1, 1), (2.003, brightness, .57), (3.97, brightness*.36, .28),
             (5.43, brightness*.13, .17))
    values = []
    for i in range(round(duration*RATE)):
        t = i/RATE
        attack = min(1, t/.004)**2
        end = min(1, (duration-t)/.07)
        values.append(attack*end*sum(a*math.sin(TAU*frequency*r*t)*math.exp(-t/(decay*d))
                                     for r, a, d in modes))
    return values


def air(duration, seed, swell=False):
    rng = random.Random(seed)
    values, low, lower = [], 0.0, 0.0
    for i in range(round(duration*RATE)):
        t = i/RATE
        low += .24*(rng.uniform(-1, 1)-low)
        lower += .025*(low-lower)
        envelope = math.sin(math.pi*t/duration)**2 if swell else min(1,t/.002)*math.exp(-t/.014)
        values.append((low-lower)*envelope)
    return values


def body(duration, frequency, fall):
    return [math.sin(TAU*(frequency*i/RATE + fall*.018*(1-math.exp(-i/RATE/.018)))) *
            min(1, i/RATE/.003) * math.exp(-i/RATE/.045) * min(1, (duration-i/RATE)/.015)
            for i in range(round(duration*RATE))]


def reflections(dst, strength):
    dry = [list(c) for c in dst]
    for delay, gain in ((.029,.42),(.047,.31),(.079,.22),(.113,.16),(.173,.09)):
        offset = round(delay*RATE)
        for ch in range(2):
            for i in range(len(dst[ch])-offset):
                dst[ch][i+offset] += dry[1-ch][i]*gain*strength


def master(dst, peak_db):
    # DC blocking and gentle low-pass; explicit endpoint fades prevent clicks.
    for channel in dst:
        previous, hp, lp = 0.0, 0.0, 0.0
        for i, value in enumerate(channel):
            hp = value-previous + .995*hp
            previous = value
            lp += .66*(hp-lp)
            fade_in = min(1, i/(RATE*.002))
            fade_out = min(1, (len(channel)-1-i)/(RATE*.055))
            channel[i] = lp*fade_in*fade_out
    peak = max(abs(v) for channel in dst for v in channel)
    scale = 10**(peak_db/20)/peak
    return [[round(v*scale*32767) for v in channel] for channel in dst]


def write(name, channels):
    samples = array.array('h', (v for frame in zip(*channels) for v in frame))
    if sys.byteorder != 'little':
        samples.byteswap()
    path = OUT / (name+'.wav')
    with wave.open(str(path), 'wb') as file:
        file.setparams((2, 2, RATE, len(channels[0]), 'NONE', 'not compressed'))
        file.writeframes(samples.tobytes())
    return path


def metrics(channels):
    count = len(channels[0])
    energy = sum(v*v for c in channels for v in c)/(2*count)
    mono_energy = sum(((l+r)/2)**2 for l,r in zip(*channels))/count
    cross = sum(l*r for l,r in zip(*channels))
    correlation = cross / math.sqrt(sum(v*v for v in channels[0])*sum(v*v for v in channels[1]))
    return dict(duration=count/RATE, peak_dbfs=round(20*math.log10(max(abs(v) for c in channels for v in c)/32768),3),
                rms_dbfs=round(10*math.log10(energy/32768**2),3),
                dc=max(abs(sum(c)/count/32768) for c in channels),
                stereo_correlation=round(correlation,4), mono_loss_db=round(10*math.log10(mono_energy/energy),3),
                clipped_samples=sum(abs(v)>=32767 for c in channels for v in c),
                endpoints=[c[i] for c in channels for i in (0,-1)])


def main():
    sounds = []
    reveal = stereo(.82)
    layer(reveal, air(.24, 8101, True), gain=.24)
    for f, start, gain, pan in ((587.33,0,.43,-.22),(880,.07,.28,.18),(1174.66,.14,.24,.05)):
        layer(reveal, bell(f,.65,.16,.22), start, gain, pan)
    layer(reveal, body(.18,160,95), gain=.09)
    reflections(reveal,.38)
    sounds.append(('S_PG_UI_RewardOpen', reveal, -8, .85, 'Soft ascending D5-A5-D6 glass reveal; 70 ms steps match card stagger.'))

    confirm = stereo(.32)
    layer(confirm, body(.2,230,190), gain=.48)
    layer(confirm, air(.09,8102), gain=.6)
    layer(confirm, bell(1174.66,.3,.055,.2), gain=.24, pan=-.06)
    layer(confirm, bell(1760,.26,.045,.12), .014, .12, .09)
    reflections(confirm,.16)
    sounds.append(('S_PG_UI_RewardConfirm', confirm, -8, .9, 'Dry tactile latch, immediate attack; acknowledges selection, not successful saving.'))

    victory = stereo(2.4)
    layer(victory, body(.25,98,105), gain=.15)
    layer(victory, air(.5,8103,True), gain=.1)
    for f, start, gain, pan in ((293.665,0,.42,0),(440,.085,.3,-.2),(587.33,.17,.36,.2),
                              (739.99,.255,.24,-.1),(880,.34,.28,.17),(1174.66,.34,.14,-.2)):
        layer(victory, bell(f,2.05,.43,.18), start, gain, pan)
    reflections(victory,.65)
    sounds.append(('S_PG_UI_Victory', victory, -6, .85, 'D major rising resolution, warm low body and restrained crystalline tail.'))

    manifest = dict(schema=1, sample_rate=RATE, channels=2, bits=16, seed_family=8100,
                    provenance='Original procedural synthesis. No external samples or music.',
                    sound_class_volume=.8, preview_platform_headroom_db=-3.0,
                    asset_directory='/Game/DataCenter/Audio/UI', sounds=[])
    preview = [[], []]
    for name, raw, peak, volume, description in sounds:
        pcm = master(raw,peak)
        path = write(name,pcm)
        result = metrics(pcm)
        assert result['clipped_samples']==0 and result['dc']<.0001
        assert result['mono_loss_db']>-.6 and result['endpoints']==[0]*4
        manifest['sounds'].append(dict(name=name, filename=path.name, volume=volume, description=description,
                                       sha256=hashlib.sha256(path.read_bytes()).hexdigest(), **result))
        for ch in range(2):
            preview[ch].extend([0]*round(.45*RATE))
            preview[ch].extend(round(v*volume*manifest['sound_class_volume']*10**(manifest['preview_platform_headroom_db']/20)) for v in pcm[ch])
            preview[ch].extend([0]*round(.75*RATE))
    write('PG_UI_SFX_Preview',preview)
    manifest['preview_order']=[s['name'] for s in manifest['sounds']]
    (OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(manifest,indent=2))


if __name__ == '__main__':
    main()
