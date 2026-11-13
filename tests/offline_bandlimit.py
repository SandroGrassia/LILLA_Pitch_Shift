"""Compare source bandwidth reduction with the existing pitch-3 render."""
import array
import hashlib
import json
import math
from pathlib import Path
import shutil
import sys
import wave

root = Path(__file__).resolve().parents[1]
source = root / ".pio/offline_compare"
out = root / ".pio/offline_bandlimit"
out.mkdir(parents=True, exist_ok=True)
assert sys.byteorder == "little"
rate = 44100
cutoff = 7350.0
taps = 255
half = taps // 2
with wave.open(str(source / "00_original_gain_half.wav"), "rb") as wav:
    assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) == (1, 2, rate)
    original = array.array("h", wav.readframes(wav.getnframes()))

# Symmetric windowed-sinc FIR. Centered convolution compensates group delay.
# Reflection at the two file edges avoids introducing a hard zero-padding step.
coefficients = []
for i in range(taps):
    offset = i - half
    ideal = 2 * cutoff / rate if offset == 0 else math.sin(2 * math.pi * cutoff / rate * offset) / (math.pi * offset)
    window = 0.42 - 0.5 * math.cos(2 * math.pi * i / (taps - 1)) + 0.08 * math.cos(4 * math.pi * i / (taps - 1))
    coefficients.append(ideal * window)
total = sum(coefficients)
coefficients = [x / total for x in coefficients]

def response(frequency):
    value = sum(coefficients[i] * math.cos(2 * math.pi * frequency * (i - half) / rate) for i in range(taps))
    return 20 * math.log10(max(abs(value), 1e-12))

assert abs(response(1000)) < 0.01 and abs(response(6000)) < 0.1
assert response(9000) < -60
assert abs(response(cutoff) + 6.0206) < 0.1
values = list(original)
padded = values[1:half + 1][::-1] + values + values[-half - 1:-1][::-1]
filtered = array.array("h")
for index in range(len(values)):
    center = index + half
    sample = coefficients[half] * padded[center]
    for offset in range(1, half + 1):
        sample += coefficients[half + offset] * (padded[center - offset] + padded[center + offset])
    assert -32768 < sample < 32767
    filtered.append(round(sample))
assert filtered != original
shutil.copyfile(source / "00_original_gain_half.wav", out / "A_originale.wav")
shutil.copyfile(source / "fft_1024_pitch_3.wav", out / "C_FFT_068_pitch_3.wav")
with wave.open(str(out / "B_originale_filtrato_7350.wav"), "wb") as wav:
    wav.setparams((1, 2, rate, len(filtered), "NONE", "not compressed"))
    wav.writeframes(filtered.tobytes())
manifest = {"filter": "255-tap Blackman-windowed sinc, symmetric convolution with delay compensation and reflected edges", "cutoff_hz": cutoff, "cutoff_definition": "approximately -6 dB, finite transition band", "gain": "source gain 0.5, no per-file normalization", "response_db": {str(f): response(f) for f in (1000, 6000, 7000, 7350, 7700, 8000, 9000)}, "files": []}
for name in ("A_originale.wav", "B_originale_filtrato_7350.wav", "C_FFT_068_pitch_3.wav"):
    with wave.open(str(out / name), "rb") as wav:
        assert wav.getnframes() == len(original) and wav.getframerate() == rate
        samples = array.array("h", wav.readframes(wav.getnframes()))
    peak = max(map(abs, samples))
    assert 0 < peak < 32767
    manifest["files"].append({"name": name, "samples": len(samples), "peak": peak, "rms": math.sqrt(sum(s * s for s in samples) / len(samples)), "sha256": hashlib.sha256((out / name).read_bytes()).hexdigest()})
rows = []
for name, label in [("A_originale.wav", "A — Originale integro"), ("B_originale_filtrato_7350.wav", "B — Originale filtrato a 7,35 kHz, senza trasposizione"), ("C_FFT_068_pitch_3.wav", "C — FFT 0.6.8 a pitch 3")]:
    rows.append(f'<h2>{label}</h2><audio controls preload="none" src="{name}"></audio>')
page = '<!doctype html><html lang="it"><meta charset="utf-8"><title>109 — perdita di banda</title><style>body{font:18px system-ui;max-width:760px;margin:40px auto;padding:0 20px;background:#141820;color:#eee}audio{width:100%}</style><h1>109 — verifica della perdita di banda</h1><p>Ascolta prima A e B: le P, T e C rimangono riconoscibili nel file filtrato? Poi confronta con C, il riferimento a pitch 3.</p><p>B mantiene pitch e durata originali. Il filtro FIR ha una transizione graduale intorno a 7,35 kHz (circa −6 dB al taglio); non è un taglio ideale. Il ritardo del filtro è compensato. Un filtro può comunque modificare i transienti: questa è una verifica diagnostica, non una riproduzione esatta della perdita spettrale della FFT.</p><p>File interi, stesso guadagno iniziale 0,5, nessuna normalizzazione individuale. Firmware invariato: 0.6.8.</p><label><input id="repeat" type="checkbox"> Ripeti</label>' + ''.join(rows) + '<script>const tracks=document.querySelectorAll("audio");document.addEventListener("play",e=>{tracks.forEach(a=>{if(a!==e.target){a.pause();}})},true);document.getElementById("repeat").addEventListener("change",e=>{tracks.forEach(a=>{a.loop=e.target.checked;});});</script></html>'
(out / "index.html").write_text(page, encoding="utf-8")
(out / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
print(json.dumps(manifest, indent=2))
print(out / "index.html")
