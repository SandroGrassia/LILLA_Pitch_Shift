"""Cut aligned listening excerpts from complete baseline renders, without restarting DSP."""
import array
import hashlib
import json
from pathlib import Path
import sys
import wave

root = Path(__file__).resolve().parents[1]
source = root / ".pio/offline_compare"
output = root / ".pio/offline_passages"
output.mkdir(parents=True, exist_ok=True)
rate = 44100
tracks = [("originale", "00_original_gain_half.wav"), ("pitch_2", "fft_1024_pitch_2.wav"), ("pitch_3", "fft_1024_pitch_3.wav")]
audio = {}
manifest = {"baseline": "b9cd6a2 / 0.6.8", "target_defect": "unclear consonants and attacks at pitch 3", "gain": "unchanged from full renders; no normalization", "edge_fade_ms": 3, "source_hashes": {}, "passages": []}
for name, filename in tracks:
    path = source / filename
    manifest["source_hashes"][filename] = hashlib.sha256(path.read_bytes()).hexdigest()
    with wave.open(str(path), "rb") as wav:
        assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) == (1, 2, rate)
        samples = array.array("h", wav.readframes(wav.getnframes()))
    if sys.byteorder != "little":
        samples.byteswap()
    audio[name] = samples
length = len(audio["originale"])
assert all(len(samples) == length for samples in audio.values())
bounds = [0, 2 * rate, 4 * rate, 6 * rate, 8 * rate, length]
sections = []
for number, (start, end) in enumerate(zip(bounds, bounds[1:])):
    letter = chr(ord("A") + number)
    sections.append(f"<section><h2>Passaggio {letter}: {start / rate:.2f}–{end / rate:.2f} s</h2>")
    entry = {"id": letter, "start_sample": start, "end_sample": end, "files": []}
    for name, _ in tracks:
        samples = array.array("h", audio[name][start:end])
        fade = min(round(0.003 * rate), len(samples) // 2)
        for i in range(fade):
            samples[i] = round(samples[i] * i / fade)
            samples[-1 - i] = round(samples[-1 - i] * i / fade)
        assert any(samples) and max(map(abs, samples)) < 32767
        if sys.byteorder != "little":
            samples.byteswap()
        filename = f"{letter}_{name}.wav"
        with wave.open(str(output / filename), "wb") as wav:
            wav.setparams((1, 2, rate, len(samples), "NONE", "not compressed"))
            wav.writeframes(samples.tobytes())
        entry["files"].append(filename)
        sections.append(f'<p>{name.replace("_", " ")}</p><audio controls preload="none" src="{filename}"></audio>')
    sections.append("</section>")
    manifest["passages"].append(entry)
page = '<!doctype html><html lang="it"><meta charset="utf-8"><title>109 — consonanti e attacchi</title><style>body{font:18px system-ui;max-width:760px;margin:40px auto;padding:0 20px;background:#141820;color:#eee}section{border-top:1px solid #64748b;margin-top:30px}audio{width:100%}p{margin-bottom:8px}</style><h1>109 — consonanti e attacchi</h1><p>Individua il passaggio in cui una consonante o un attacco chiaro nell’originale si perde a pitch 3. Il pitch 2 serve da controllo.</p><p>Estratti della 0.6.8 già elaborata: nessun riavvio del motore ai tagli, nessun nuovo algoritmo. Stesso guadagno, senza normalizzazione. Solo 3 ms di dissolvenza ai bordi per evitare clic di taglio: non giudicare quei bordi.</p><p>Se il difetto cade su un taglio, indica il tempo nel file completo: prepareremo un estratto centrato su quel punto.</p><label><input id="repeat" type="checkbox"> Ripeti il passaggio in ascolto</label>' + ''.join(sections) + '<script>const tracks=document.querySelectorAll("audio");document.addEventListener("play",e=>{tracks.forEach(a=>{if(a!==e.target){a.pause();}})},true);document.getElementById("repeat").addEventListener("change",e=>{tracks.forEach(a=>{a.loop=e.target.checked;});});</script></html>'
(output / "index.html").write_text(page, encoding="utf-8")
(output / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
for entry in manifest["passages"]:
    for filename in entry["files"]:
        with wave.open(str(output / filename), "rb") as wav:
            assert wav.getnframes() == entry["end_sample"] - entry["start_sample"]
print("PASS: 15 non-silent excerpts, matching time intervals and no full-scale samples.")
print(output / "index.html")
