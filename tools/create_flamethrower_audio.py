"""Create the game's original seamless, soft cartoon flame-jet loop."""
from pathlib import Path
import math
import random
import struct
import wave

root = Path(__file__).resolve().parents[1]
sample_rate = 22050
duration = 2
random_source = random.Random(62041)
partials = [(random_source.randrange(90, 1900), random_source.random() * math.tau,
             random_source.uniform(0.5, 1.0)) for _ in range(96)]
samples = []
for index in range(sample_rate * duration):
    cycle = index / (sample_rate * duration)
    air = sum(math.sin(math.tau * frequency * cycle + phase) * amplitude
              for frequency, phase, amplitude in partials) / 24
    breath = 0.85 + 0.08 * math.sin(math.tau * 3 * cycle) + 0.07 * math.sin(math.tau * 7 * cycle)
    low = 0.05 * math.sin(math.tau * 83 * cycle) + 0.03 * math.sin(math.tau * 117 * cycle)
    samples.append(max(-1.0, min(1.0, (air * breath + low) * 0.8)))

output = root / "assets/audio/flamethrower_loop.wav"
with wave.open(str(output), "wb") as stream:
    stream.setnchannels(1)
    stream.setsampwidth(2)
    stream.setframerate(sample_rate)
    stream.writeframes(b"".join(struct.pack("<h", round(value * 32767)) for value in samples))
print(f"{output.name}: seamless {duration}s, peak {max(map(abs, samples)):.3f}")
