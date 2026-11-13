"""Full-file WSOLA listening comparison, isolated from the active FFT firmware."""
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
out = root / ".pio/offline_wsola"
build = out / "build"
build.mkdir(parents=True, exist_ok=True)
data = args.source.read_bytes()
assert sys.byteorder == "little" and len(data) >= 4 and len(data) % 2 == 0
reference = json.loads((root / ".pio/offline_compare/manifest.json").read_text())
assert hashlib.sha256(data).hexdigest() == reference["sha256"], "Different source from previous comparisons"
manifest = {"source_sha256": reference["sha256"], "source_commit": "b9cd6a2", "gain": 0.5, "normalization": "none", "correlation": "all overlap samples; coarse candidates every 8 samples, fine candidates every sample", "filter": "existing 4-pole WSOLA antialias before linear resampling", "files": [], "renders": []}
for path in ("include/WsolaCore.h", "src/WsolaCore.cpp", "include/WsolaAntialias.h", "src/WsolaAntialias.cpp"):
    content = subprocess.check_output(["git", "show", "b9cd6a2:" + path], cwd=root).decode()
    if path.endswith("WsolaCore.cpp"):
        for stride in (64, 128):
            expression = f"std::max<uint32_t>(1u, overlap / {stride}u)"
            assert expression in content
            content = content.replace(expression, "1u")
    (build / Path(path).name).write_text(content)
compiler = shutil.which("g++") or "C:/msys64/ucrt64/bin/g++.exe"
exe = build / "render.exe"
subprocess.run([compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror", "-static", "-I", str(build), str(build / "WsolaCore.cpp"), str(build / "WsolaAntialias.cpp"), str(root / "tests/offline_wsola_render.cpp"), "-o", str(exe)], check=True)

def write(name, samples):
    assert len(samples) * 2 == len(data)
    peak = max(map(abs, samples))
    assert 0 < peak < 32767
    with wave.open(str(out / name), "wb") as wav:
        wav.setparams((1, 2, 44100, len(samples), "NONE", "not compressed"))
        wav.writeframes(samples.tobytes())
    manifest["files"].append({"name": name, "samples": len(samples), "peak": peak, "rms": math.sqrt(sum(x * x for x in samples) / len(samples)), "sha256": hashlib.sha256(samples.tobytes()).hexdigest()})

write("originale.wav", array.array("h", (int(x * 0.5) for x in array.array("h", data))))
rows = []
for pitch in (3, 2):
    rows.append(f"<h2>Pitch {pitch}</h2>")
    with wave.open(str(root / f".pio/offline_compare/fft_1024_pitch_{pitch}.wav"), "rb") as wav:
        samples = array.array("h", wav.readframes(wav.getnframes()))
    name = f"pitch_{pitch}_A_FFT_068.wav"
    write(name, samples)
    rows.append(f'<p>A — FFT 0.6.8 di riferimento</p><audio controls preload="none" src="{name}"></audio>')
    for label, window, overlap, search in [("B", 882, 50, 441), ("C", 1764, 50, 882), ("D", 882, 75, 441)]:
        name = f"pitch_{pitch}_{label}_WSOLA_{window}_{overlap}.wav"
        raw = build / (name + ".raw")
        command = [str(exe), str(args.source.resolve()), str(raw), str(pitch), str(window), str(overlap), str(search)]
        result = subprocess.run(command, check=True, capture_output=True, text=True)
        write(name, array.array("h", raw.read_bytes()))
        manifest["renders"].append({"file": name, "window": window, "overlap_percent": overlap, "search_radius": search, "pitch": pitch, "diagnostic": result.stdout.strip()})
        rows.append(f'<p>{label} — WSOLA, {window} campioni, overlap {overlap}%</p><audio controls preload="none" src="{name}"></audio>')
        print(name, result.stdout.strip(), flush=True)
page = '<!doctype html><html lang="it"><meta charset="utf-8"><title>109 — WSOLA, file intero</title><style>body{font:18px system-ui;max-width:760px;margin:40px auto;padding:0 20px;background:#141820;color:#eee}audio{width:100%}h2{border-top:1px solid #64748b;padding-top:24px}</style><h1>109 — confronto WSOLA</h1><p>Solo file interi. Prima pitch 3, poi pitch 2 come controllo. A: FFT 0.6.8. B: WSOLA 882/50%. C: WSOLA 1764/50%. D: WSOLA 882/75%.</p><p>WSOLA allunga il segnale, poi lo ricampiona con filtro antialias per recuperare la durata originale. La correlazione confronta tutti i campioni dell’overlap, mantenendo la ricerca grossolana/raffinata del motore esistente. Tutti i suoni restano trasposti.</p><p>Stessa durata e guadagno iniziale 0,5; nessuna normalizzazione individuale. I livelli risultanti possono differire. Ascolta P, T e C, eventuali attacchi ripetuti e stabilità delle vocali. Questa prova non garantisce miglioramenti né prestazioni sul Teensy.</p><p>Firmware invariato: 0.6.8.</p><label><input type="checkbox" id="repeat"> Ripeti il file</label><h2>Originale</h2><audio controls src="originale.wav"></audio>' + ''.join(rows) + '<script>const tracks=document.querySelectorAll("audio");document.addEventListener("play",e=>{tracks.forEach(a=>{if(a!==e.target){a.pause();}})},true);document.getElementById("repeat").addEventListener("change",e=>{tracks.forEach(a=>{a.loop=e.target.checked;});});</script></html>'
(out / "index.html").write_text(page, encoding="utf-8")
(out / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
assert len({f["sha256"] for f in manifest["files"]}) == len(manifest["files"])
for entry in manifest["files"]:
    with wave.open(str(out / entry["name"]), "rb") as wav:
        assert wav.getnframes() == len(data) // 2 and wav.getframerate() == 44100
print("PASS: 9 distinct non-silent full-length WAVs, equal duration, no full-scale samples.")
print(out / "index.html")
