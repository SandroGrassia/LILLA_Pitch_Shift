"""Render frozen 0.6.8 and larger-window variants without changing firmware."""
import argparse
import array
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import wave

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("source", type=Path, help="Mono signed 16-bit little-endian RAW, 44100 Hz")
parser.add_argument("--output", type=Path, default=Path(".pio/offline_compare"))
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
source = args.source.resolve()
data = source.read_bytes()
if len(data) < 4 or len(data) % 2:
    parser.error("RAW size must be even and contain at least two samples")
if sys.byteorder != "little":
    parser.error("The C++ renderer requires a little-endian host")
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
compiler = shutil.which("g++") or "C:/msys64/ucrt64/bin/g++.exe"
header = subprocess.check_output(["git", "show", "b9cd6a2:include/FftPitchCore.h"], cwd=root).decode()
core = subprocess.check_output(["git", "show", "b9cd6a2:src/FftPitchCore.cpp"], cwd=root).decode()
manifest = {"source": str(source), "sha256": hashlib.sha256(data).hexdigest(), "rate": 44100, "samples": len(data) // 2, "gain": 0.5, "baseline": "b9cd6a2", "files": []}

def write_wav(name, pcm):
    with wave.open(str(out / name), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(44100)
        wav.writeframes(pcm)
    samples = array.array("h", pcm)
    rms = (sum(x * x for x in samples) / len(samples)) ** 0.5
    manifest["files"].append({"file": name, "samples": len(samples), "peak": max(abs(x) for x in samples), "rms": rms, "full_scale_samples": sum(abs(x) >= 32767 for x in samples)})

original = array.array("h", data)
write_wav("00_original_gain_half.wav", array.array("h", (int(x * 0.5) for x in original)).tobytes())
for size in (1024, 2048, 4096):
    build = out / f"build_{size}"
    build.mkdir(exist_ok=True)
    (build / "FftPitchCore.h").write_text(header.replace("Size = 1024", f"Size = {size}").replace("Hop = 256", f"Hop = {size // 4}"))
    (build / "FftPitchCore.cpp").write_text(core)
    exe = build / "render.exe"
    subprocess.run([compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror", "-static", "-I", str(build), str(build / "FftPitchCore.cpp"), str(root / "tests/offline_render.cpp"), "-o", str(exe)], check=True)
    for pitch in (1, 2, 3):
        raw = build / f"pitch_{pitch}.raw"
        subprocess.run([str(exe), str(source), str(raw), str(pitch)], check=True)
        pcm = raw.read_bytes()
        assert len(pcm) == len(data)
        write_wav(f"fft_{size}_pitch_{pitch}.wav", pcm)
        print(f"Rendered FFT {size}, pitch {pitch}", flush=True)
(out / "manifest.json").write_text(json.dumps(manifest, indent=2))
(out / "LISTEN.txt").write_text("Compare pitch 2 files first, then pitch 3, at the same playback volume.\nAll files use gain 0.5 with no individual normalization. Original duration is preserved.\n1024 is the frozen 0.6.8 baseline; 2048 and 4096 are offline-only variants, with 75% overlap.\nLarger windows also increase startup buildup and transient holdoff duration; listen to attacks.\nThese variants are not the rejected 0.6.9 (2048 with hop 256). Here hop scales with FFT size.\nPitch 1 uses direct RAW output with anticlick, serving as a control.\nPortable host FFT is used; this does not measure Teensy CPU or guarantee sample-exact CMSIS output.\nNo claim of quality improvement is made; the listening comparison determines that.\n")
print(out)
rows = []
for pitch in (2, 3):
    rows.append(f"<h2>Pitch {pitch}</h2>")
    for size in (1024, 2048, 4096):
        label = "0.6.8 di riferimento" if size == 1024 else "variante offline"
        rows.append(f'<p>FFT {size} — {label}</p><audio controls preload="none" src="fft_{size}_pitch_{pitch}.wav"></audio>')
page = '<!doctype html><html lang="it"><meta charset="utf-8"><title>109.raw — confronto offline</title><style>body{font:18px system-ui;max-width:720px;margin:40px auto;padding:0 20px;background:#141820;color:#eee}audio{width:100%}p{margin-bottom:8px}</style><h1>109.raw — confronto offline</h1><p>Stessa durata e guadagno 0,5 per tutte le versioni; nessuna normalizzazione individuale. Il volume risultante può differire. Ascoltare una traccia alla volta, confrontando timbro, consonanti e attacchi.</p><p>FFT maggiori: più risoluzione in frequenza, ma finestre temporali più lunghe. Overlap 75%. Firmware invariato.</p><h2>Originale</h2><audio controls src="00_original_gain_half.wav"></audio>' + ''.join(rows) + '<script>document.addEventListener("play",e=>{document.querySelectorAll("audio").forEach(a=>{if(a!==e.target){a.pause();}})},true);</script></html>'
(out / "index.html").write_text(page, encoding="utf-8")
