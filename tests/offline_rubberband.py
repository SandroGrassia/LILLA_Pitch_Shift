"""Compare frozen 0.6.8 with Rubber Band R3, without changing firmware."""
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
parser.add_argument("--input", type=Path, default=Path(".pio/offline_compare"))
parser.add_argument("--output", type=Path, default=Path(".pio/offline_rubberband"))
parser.add_argument("--exe", type=Path, default=Path(".pio/tools/rubberband/rubberband-4.0.0-gpl-executable-windows/rubberband.exe"))
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
exe = str(args.exe.resolve())
version = subprocess.run([exe, "--version"], capture_output=True, text=True, check=True)
original = args.input / "00_original_gain_half.wav"
manifest = {"version": (version.stdout + version.stderr).strip(), "engine": "R3 offline", "input_sha256": hashlib.sha256(original.read_bytes()).hexdigest(), "input_gain": 0.5, "normalization": "none; automatic clipping gain reduction disabled", "commands": [], "files": []}

def inspect(path):
    with wave.open(str(path), "rb") as wav:
        assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) == (1, 2, 44100)
        samples = array.array("h", wav.readframes(wav.getnframes()))
    if sys.byteorder != "little":
        samples.byteswap()
    assert len(samples) == 474769, "Duration differs from the reference"
    peak = max(map(abs, samples))
    assert 0 < peak < 32767, "Silent or potentially clipped output"
    manifest["files"].append({"file": path.name, "samples": len(samples), "peak": peak, "rms": math.sqrt(sum(s * s for s in samples) / len(samples)), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})

shutil.copyfile(original, args.output / "original.wav")
inspect(args.output / "original.wav")
rows = []
for pitch in (2, 3):
    baseline = args.output / f"pitch_{pitch}_A_068.wav"
    shutil.copyfile(args.input / f"fft_1024_pitch_{pitch}.wav", baseline)
    inspect(baseline)
    rows.append(f"<h2>Pitch {pitch}</h2>")
    files = [(baseline, "A — 0.6.8 di riferimento")]
    for suffix, flags, label in [("B_R3", [], "B — Rubber Band R3"), ("C_R3_formanti", ["--formant"], "C — Rubber Band R3, formanti preservate")]:
        target = args.output / f"pitch_{pitch}_{suffix}.wav"
        command = [exe, "-3", "--time", "1", "--frequency", str(pitch), "--ignore-clipping", "--quiet"] + flags + [str(original.resolve()), str(target.resolve())]
        result = subprocess.run(command, capture_output=True, text=True, check=True)
        (args.output / f"pitch_{pitch}_{suffix}.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        manifest["commands"].append(command)
        inspect(target)
        files.append((target, label))
    for path, label in files:
        rows.append(f'<p>{label}</p><audio controls preload="none" src="{path.name}"></audio>')

page = '<!doctype html><html lang="it"><meta charset="utf-8"><title>109 — confronto Rubber Band</title><style>body{font:18px system-ui;max-width:720px;margin:40px auto;padding:0 20px;background:#141820;color:#eee}audio{width:100%}a{color:#93c5fd}</style><h1>109 — confronto Rubber Band</h1><p>A: 0.6.8. B: Rubber Band R3 standard. C: R3 con preservazione delle formanti, per limitare il cambiamento del timbro vocale.</p><p>Durata invariata, 10,77 secondi. Stesso guadagno iniziale 0,5, senza normalizzazione individuale; i livelli risultanti possono differire. Ascoltare consonanti, attacchi e suoni sostenuti.</p><p>Questa è una prova offline su PC, non una versione per Teensy. Firmware sempre 0.6.8.</p><p><a href="https://breakfastquay.com/rubberband/">Utility ufficiale Rubber Band 4.0.0</a></p><h2>Originale</h2><audio controls src="original.wav"></audio>' + ''.join(rows) + '<script>document.addEventListener("play",e=>{document.querySelectorAll("audio").forEach(a=>{if(a!==e.target){a.pause();}})},true);</script></html>'
(args.output / "index.html").write_text(page, encoding="utf-8")
(args.output / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
for item in manifest["files"]:
    print(item["file"], "peak", item["peak"], "RMS", round(item["rms"], 1))
print(args.output.resolve())
