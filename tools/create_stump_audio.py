"""Original woody eruption: low knocks, creaking bark and a soft leafy tail."""
from pathlib import Path
import math
import random
import struct
import wave

rate = 44100
duration = 0.72
rng = random.Random(0x5354554D)
samples = []
filtered = 0.0
for i in range(round(rate * duration)):
    t = i / rate
    noise = rng.uniform(-1, 1)
    filtered += 0.12 * (noise - filtered)
    attack = min(1, t / 0.008)
    envelope = attack * math.exp(-5 * t) * min(1, (duration - t) / 0.08)
    rumble = 0.12 * math.sin(math.tau * (61 * t - 12 * t * t)) * math.exp(-7 * t)
    bark = 0.15 * filtered * envelope
    creak = 0.045 * math.sin(math.tau * (190 * t + 135 * t * t)) * envelope
    knocks = 0.0
    for when, pitch, gain in [(0, 115, .34), (.066, 142, .22), (.15, 96, .18), (.24, 170, .12)]:
        u = t - when
        if u >= 0:
            knocks += gain * math.exp(-42 * u) * math.sin(math.tau * pitch * u) * min(1, u / .003)
            knocks += .06 * math.exp(-65 * u) * noise
    leaves = 0.028 * (noise - filtered) * math.sin(math.pi * min(1, t / duration)) ** 2 * envelope
    samples.append((rumble + bark + creak + knocks + leaves) * min(1, (duration - t) / .03))

output = Path(__file__).resolve().parents[1] / "assets/audio/stump_roots.wav"
with wave.open(str(output), "wb") as stream:
    stream.setnchannels(1)
    stream.setsampwidth(2)
    stream.setframerate(rate)
    stream.writeframes(b"".join(struct.pack("<h", round(max(-1, min(1, x)) * 32767)) for x in samples))
print(f"{output.name}: {duration}s, original synthesis, peak {max(map(abs, samples)):.3f}")
