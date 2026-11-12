from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
build = root / ".pio" / "scheduling_test"
build.mkdir(parents=True, exist_ok=True)
# Frozen 0.6.3 reference: validate output independently of the new scheduler.
for source in ("include/FftPitchCore.h", "src/FftPitchCore.cpp"):
    data = subprocess.check_output(["git", "show", "882abb2:" + source], cwd=root)
    data = data.replace(b"FftPitchCore", b"LegacyFftPitchCore")
    (build / ("LegacyFftPitchCore" + Path(source).suffix)).write_bytes(data)
compiler = shutil.which("g++") or "C:/msys64/ucrt64/bin/g++.exe"
exe = build / "scheduling_test.exe"
subprocess.run([compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror", "-static", "-I", str(root / "include"), "-I", str(build), str(root / "src/FftPitchCore.cpp"), str(build / "LegacyFftPitchCore.cpp"), str(root / "tests/scheduling_test.cpp"), "-o", str(exe)], check=True)
subprocess.run([str(exe)], check=True)
