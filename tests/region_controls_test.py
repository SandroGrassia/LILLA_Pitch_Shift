"""Run production FFT transport/region regressions without the retired prototype."""
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
build = root / ".pio/region_tests"
build.mkdir(parents=True, exist_ok=True)
compiler = shutil.which("g++") or "C:/msys64/ucrt64/bin/g++.exe"
common = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror", "-static",
          "-I", str(root / "include"), str(root / "src/FftPitchCore.cpp"),
          str(root / "src/FftSpectralFrame.cpp")]
for name in ("ping_pong", "loop_reverse", "once_reverse", "once_forward", "cross_mix", "region_controls", "fft"):
    exe = build / (name + ".exe")
    subprocess.run(common + [str(root / f"tests/{name}_test.cpp"), "-o", str(exe)], cwd=root, check=True)
    subprocess.run([str(exe)], cwd=root, check=True)
