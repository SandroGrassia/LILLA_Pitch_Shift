from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
build = root / ".pio/region_tests"
build.mkdir(parents=True, exist_ok=True)
for source in ("include/FftPitchCore.h", "src/FftPitchCore.cpp"):
    data = subprocess.check_output(["git", "show", "b9cd6a2:" + source], cwd=root)
    (build / ("BaselineFftPitchCore" + Path(source).suffix)).write_bytes(data.replace(b"FftPitchCore", b"BaselineFftPitchCore"))
compiler = shutil.which("g++") or "C:/msys64/ucrt64/bin/g++.exe"
common = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror", "-static", "-I", str(root / "tests/stubs"), "-I", str(root / "include"), "-I", str(build)]
cases = {
    "once_forward": ["src/FftPitchCore.cpp", "src/PlaybackRegion.cpp", "src/GranularAudio.cpp", "src/NoteVoices.cpp", "src/ExperimentConfig.cpp", "tests/once_forward_test.cpp"],
    "cross_mix": ["src/FftPitchCore.cpp", "src/PlaybackRegion.cpp", "src/GranularAudio.cpp", "src/NoteVoices.cpp", "src/ExperimentConfig.cpp", "tests/cross_mix_test.cpp"],
    "region_baseline": ["src/FftPitchCore.cpp", str(build / "BaselineFftPitchCore.cpp"), "tests/region_baseline_test.cpp"],
    "region_controls": ["src/FftPitchCore.cpp", "src/PlaybackRegion.cpp", "src/GranularAudio.cpp", "src/NoteVoices.cpp", "src/ExperimentConfig.cpp", "tests/region_controls_test.cpp"],
    "midi_voices": ["src/FftPitchCore.cpp", "src/GranularAudio.cpp", "src/NoteVoices.cpp", "src/ExperimentConfig.cpp", "tests/midi_voices_test.cpp"],
    "fft": ["src/FftPitchCore.cpp", "tests/fft_test.cpp"],
}
for name, sources in cases.items():
    exe = build / (name + ".exe")
    subprocess.run(common + [str(root / path) for path in sources] + ["-o", str(exe)], cwd=root, check=True)
    subprocess.run([str(exe)], cwd=root, check=True)
