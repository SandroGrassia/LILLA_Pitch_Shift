"""Render isolated 0.6.8 transient-threshold experiments; leave firmware untouched."""
import argparse
import array
import hashlib
import json
import math
from pathlib import Path
import shutil
import subprocess
import sys
import wave

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("source", type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
out = root / ".pio/offline_phase_resets"
out.mkdir(parents=True, exist_ok=True)
data = args.source.read_bytes()
assert sys.byteorder == "little" and len(data) >= 4 and len(data) % 2 == 0
old_manifest = json.loads((root / ".pio/offline_compare/manifest.json").read_text())
assert hashlib.sha256(data).hexdigest() == old_manifest["sha256"], "Source differs from previous listening tests"
header = subprocess.check_output(["git", "show", "b9cd6a2:include/FftPitchCore.h"], cwd=root).decode()
baseline = subprocess.check_output(["git", "show", "b9cd6a2:src/FftPitchCore.cpp"], cwd=root).decode()
compiler = shutil.which("g++") or "C:/msys64/ucrt64/bin/g++.exe"
manifest = {"baseline": "b9cd6a2 / 0.6.8", "source_sha256": old_manifest["sha256"], "gain": 0.5, "normalization": "none", "fft": 1024, "hop": 256, "edge_fade_ms": 3, "variants": [], "files": []}
audio = {"originale": array.array("h", (int(x * 0.5) for x in array.array("h", data)))}

def write(name, samples):
    assert any(samples) and max(map(abs, samples)) < 32767
    with wave.open(str(out / name), "wb") as wav:
        wav.setparams((1, 2, 44100, len(samples), "NONE", "not compressed"))
        wav.writeframes(samples.tobytes())
    manifest["files"].append({"name": name, "samples": len(samples), "peak": max(map(abs, samples)), "rms": math.sqrt(sum(x * x for x in samples) / len(samples))})

for name, threshold, holdoff in [("riferimento", "0.35", 4), ("moderato", "0.22", 2), ("intenso", "0.12", 1)]:
    build = out / ("build_" + name)
    build.mkdir(exist_ok=True)
    assert baseline.count("positive_flux > 0.35f * spectral_sum") == 1
    assert baseline.count("transient_holdoff = 4;") == 1
    core = baseline.replace("positive_flux > 0.35f * spectral_sum", f"positive_flux > {threshold}f * spectral_sum").replace("transient_holdoff = 4;", f"transient_holdoff = {holdoff};")
    (build / "FftPitchCore.h").write_text(header)
    (build / "FftPitchCore.cpp").write_text(core)
    exe = build / "render.exe"
    subprocess.run([compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror", "-static", "-I", str(build), str(build / "FftPitchCore.cpp"), str(root / "tests/offline_render.cpp"), "-o", str(exe)], check=True)
    entry = {"name": name, "flux_threshold": float(threshold), "holdoff_hops": holdoff, "renders": []}
    for pitch in (2, 3):
        raw = build / f"pitch_{pitch}.raw"
        result = subprocess.run([str(exe), str(args.source.resolve()), str(raw), str(pitch)], check=True, capture_output=True, text=True)
        samples = array.array("h", raw.read_bytes())
        assert len(samples) * 2 == len(data)
        if name == "riferimento":
            with wave.open(str(root / f".pio/offline_compare/fft_1024_pitch_{pitch}.wav"), "rb") as wav:
                assert samples.tobytes() == wav.readframes(wav.getnframes()), "Baseline output changed"
        write(f"completo_pitch_{pitch}_{name}.wav", samples)
        entry["renders"].append({"pitch": pitch, "diagnostic": result.stdout.strip()})
        print(name, pitch, result.stdout.strip(), flush=True)
        if pitch == 3:
            audio[name] = samples
    manifest["variants"].append(entry)
assert audio["moderato"] != audio["riferimento"] and audio["intenso"] != audio["moderato"]

bounds = [0, 88200, 176400, 264600, 352800, len(audio["originale"])]
rows = []
for number, (start, end) in enumerate(zip(bounds, bounds[1:])):
    letter = chr(65 + number)
    rows.append(f"<section><h2>Passaggio {letter}: {start / 44100:.2f}–{end / 44100:.2f} s</h2>")
    for name, full in audio.items():
        samples = full[start:end]
        for i in range(132):
            samples[i] = round(samples[i] * i / 132)
            samples[-1 - i] = round(samples[-1 - i] * i / 132)
        filename = f"{letter}_{name}.wav"
        write(filename, samples)
        rows.append(f'<p>{name}</p><audio controls preload="none" src="{filename}"></audio>')
    rows.append("</section>")
rows.append("<h2>File completi — controllo a pitch 2</h2>")
for name in ("riferimento", "moderato", "intenso"):
    rows.append(f'<p>{name}</p><audio controls preload="none" src="completo_pitch_2_{name}.wav"></audio>')
page = '<!doctype html><html lang="it"><meta charset="utf-8"><title>109 — reset di fase</title><style>body{font:18px system-ui;max-width:760px;margin:40px auto;padding:0 20px;background:#141820;color:#eee}audio{width:100%}section{border-top:1px solid #64748b;margin-top:25px}</style><h1>109 — reset di fase, pitch 3</h1><p>Riferimento: 0.6.8, soglia 35%, intervallo minimo 4 hop (23,2 ms). Moderato: 22%, 2 hop (11,6 ms). Intenso: 12%, 1 hop (5,8 ms).</p><p>FFT 1024, hop 256 e tutto il resto invariati. Nessun reinserimento degli attacchi originali: le tre elaborazioni sono interamente trasposte. Stesso guadagno, nessuna normalizzazione individuale.</p><p>Confronta P, T e C, ma controlla anche tremolio, crepitii e instabilità delle vocali. Gli estratti vengono tagliati dopo la lavorazione completa; solo i primi e ultimi 3 ms hanno una dissolvenza per evitare clic di taglio.</p><p>La traccia originale serve solo come riferimento. Firmware invariato: 0.6.8.</p><label><input type="checkbox" id="repeat"> Ripeti</label>' + ''.join(rows) + '<script>const tracks=document.querySelectorAll("audio");document.addEventListener("play",e=>{tracks.forEach(a=>{if(a!==e.target){a.pause();}})},true);document.getElementById("repeat").addEventListener("change",e=>{tracks.forEach(a=>{a.loop=e.target.checked;});});</script></html>'
(out / "index.html").write_text(page, encoding="utf-8")
(out / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
for entry in manifest["files"]:
    with wave.open(str(out / entry["name"]), "rb") as wav:
        assert wav.getnframes() == entry["samples"] and wav.getframerate() == 44100
print(f"PASS: {len(manifest['files'])} non-silent WAVs; baseline matches earlier renders; variants differ; no full-scale samples.")
print(out / "index.html")
