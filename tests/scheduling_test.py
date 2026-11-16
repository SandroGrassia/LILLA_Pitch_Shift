from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
build = root / ".pio" / "scheduling_test"
build.mkdir(parents=True, exist_ok=True)
# Use the same region engine with synchronous analysis as the scheduling reference.
for source in ("include/FftPitchCore.h", "src/FftPitchCore.cpp"):
    data = (root / source).read_bytes().replace(b"FftPitchCore", b"LegacyFftPitchCore")
    data = data.replace(b"else if (until_frame == Hop / 2)", b"else if (false)")
    (build / ("LegacyFftPitchCore" + Path(source).suffix)).write_bytes(data)
compiler = shutil.which("g++") or "C:/msys64/ucrt64/bin/g++.exe"
exe = build / "scheduling_test.exe"
subprocess.run([compiler, "-std=c++20", "-DFFT_DUAL_BAND=0", "-O2", "-Wall", "-Wextra", "-Werror", "-static", "-I", str(root / "include"), "-I", str(build), str(root / "src/FftSpectralFrame.cpp"), str(root / "src/FftPitchCore.cpp"), str(build / "LegacyFftPitchCore.cpp"), str(root / "tests/scheduling_test.cpp"), "-o", str(exe)], check=True)
subprocess.run([str(exe)], check=True)
