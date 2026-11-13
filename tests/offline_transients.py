"""Listening probe: crossfade original attacks into frozen 0.6.8 renders.

This is a time-domain transient bypass, not harmonic/percussive separation.
It deliberately leaves the injected attacks untransposed. No firmware is changed.
"""
import argparse
import array
import hashlib
import json
import math
from pathlib import Path
import sys
import wave

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--input", type=Path, default=Path(".pio/offline_compare"))
parser.add_argument("--output", type=Path, default=Path(".pio/offline_transients"))
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)

def read(name):
    with wave.open(str(args.input / name), "rb") as wav:
        assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) == (1, 2, 44100)
        samples = array.array("h", wav.readframes(wav.getnframes()))
    if sys.byteorder != "little":
        samples.byteswap()
    return samples

original = read("00_original_gain_half.wav")
rate = 44100
block = 128
# Detect rapid rises in total energy OR high-frequency (first-difference) energy.
# Floors suppress background noise; 35 ms refractory time avoids repeated triggers.
energy_history = 0.0
high_history = 0.0
last_onset = -rate
onsets = []
for start in range(0, len(original), block):
    stop = min(start + block, len(original))
    energy = sum(original[i] ** 2 for i in range(start, stop)) / (stop - start)
    high = sum((original[i] - original[max(0, i - 1)]) ** 2 for i in range(start, stop)) / (stop - start)
    rising = energy > max(120.0 ** 2, energy_history * 3.0) or high > max(100.0 ** 2, high_history * 3.0)
    if rising and start - last_onset >= int(0.035 * rate):
        # Locate the strongest rise inside the detector block, then pre-fade offline.
        point = max(range(start, stop), key=lambda i: abs(original[i] - original[max(0, i - 1)]))
        onsets.append(point)
        last_onset = point
    energy_history += (energy - energy_history) * 0.12
    high_history += (high - high_history) * 0.12

def envelope(hold_ms, release_ms):
    result = [0.0] * len(original)
    attack = int(0.002 * rate)
    hold = int(hold_ms * rate / 1000)
    release = int(release_ms * rate / 1000)
    for onset in onsets:
        for i in range(max(0, onset - attack), min(len(result), onset + hold + release)):
            if i < onset:
                value = 0.5 - 0.5 * math.cos(math.pi * (i - onset + attack) / attack)
            elif i < onset + hold:
                value = 1.0
            else:
                value = 0.5 + 0.5 * math.cos(math.pi * (i - onset - hold) / release)
            result[i] = max(result[i], value)
    return result

manifest = {"method": "untransposed original transient bypass with cosine crossfades", "original_gain": 0.5, "normalization": "none", "onsets_seconds": [i / rate for i in onsets], "files": []}

def write(name, samples):
    assert len(samples) == len(original)
    raw = array.array("h", samples)
    if sys.byteorder != "little":
        raw.byteswap()
    with wave.open(str(args.output / name), "wb") as wav:
        wav.setparams((1, 2, rate, len(samples), "NONE", "not compressed"))
        wav.writeframes(raw.tobytes())
    manifest["files"].append({"name": name, "samples": len(samples), "peak": max(map(abs, samples)), "rms": math.sqrt(sum(x * x for x in samples) / len(samples)), "sha256": hashlib.sha256(raw.tobytes()).hexdigest()})

write("original.wav", original)
rows = []
for pitch in (2, 3):
    baseline = read(f"fft_1024_pitch_{pitch}.wav")
    assert len(baseline) == len(original)
    rows.append(f"<h2>Pitch {pitch}</h2>")
    versions = [("A_riferimento_068", baseline)]
    for name, strength, hold, release in [("B_attacchi_leggeri", 0.5, 5, 10), ("C_attacchi_decisi", 1.0, 8, 18)]:
        weights = envelope(hold, release)
        mixed = [round(baseline[i] * (1.0 - strength * weights[i]) + original[i] * strength * weights[i]) for i in range(len(original))]
        assert mixed != list(baseline), "No transient changes detected"
        versions.append((name, mixed))
        manifest[f"pitch_{pitch}_{name}"] = {"strength": strength, "hold_ms": hold, "release_ms": release, "affected_percent": 100 * sum(w > 0 for w in weights) / len(weights)}
    for label, samples in versions:
        name = f"pitch_{pitch}_{label}.wav"
        write(name, samples)
        rows.append(f'<p>{label.replace("_", " ")}</p><audio controls preload="none" src="{name}"></audio>')

(args.output / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
page = '<!doctype html><html lang="it"><meta charset="utf-8"><title>109 — prova attacchi</title><style>body{font:18px system-ui;max-width:720px;margin:40px auto;padding:0 20px;background:#141820;color:#eee}audio{width:100%}</style><h1>109 — prova attacchi</h1><p>A: 0.6.8. B: reinserimento leggero degli attacchi originali. C: reinserimento completo e più lungo. Gli attacchi reinseriti restano al pitch originale: ascoltare anche eventuali cambi di timbro o doppia intonazione.</p><p>Stessa durata, guadagno sorgente 0,5; nessuna normalizzazione. FFT sempre 1024. È un bypass temporale sperimentale, non una separazione armonico/percussiva. Nessuna modifica al firmware.</p><h2>Originale</h2><audio controls src="original.wav"></audio>' + ''.join(rows) + '<script>document.addEventListener("play",e=>{document.querySelectorAll("audio").forEach(a=>{if(a!==e.target){a.pause();}})},true);</script></html>'
(args.output / "index.html").write_text(page, encoding="utf-8")
print(f"Detected {len(onsets)} attacks; wrote {len(manifest['files'])} WAVs to {args.output.resolve()}")
for entry in manifest["files"]:
    print(entry["name"], "peak", entry["peak"], "RMS", round(entry["rms"], 1))
