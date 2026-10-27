"""Generate the original short mechanical shutter cue (no external samples)."""
from pathlib import Path
import math
import random
import struct
import wave

random_source = random.Random(1816)
rate = 48000
samples = []
previous = 0.0
for index in range(int(rate * 0.19)):
    time = index / rate
    noise = random_source.uniform(-1, 1)
    high = noise - previous * 0.7
    previous = noise
    value = 0.0
    for start, amplitude, decay, pitch in [(0, 0.55, 0.013, 1650), (0.045, 0.42, 0.018, 1050)]:
        age = time - start
        if age >= 0:
            envelope = math.exp(-age / decay) * min(1, age / 0.0007)
            value += amplitude * envelope * (0.65 * high + 0.35 * math.sin(age * pitch * math.tau))
    value += noise * 0.055 * math.exp(-time / 0.045)
    samples.append(struct.pack('<h', round(max(-0.95, min(0.95, value)) * 32767)))
path = Path(__file__).resolve().parents[1] / 'assets/audio/feedback_shutter.wav'
with wave.open(str(path), 'wb') as sound:
    sound.setnchannels(1)
    sound.setsampwidth(2)
    sound.setframerate(rate)
    sound.writeframes(b''.join(samples))
print(path)
