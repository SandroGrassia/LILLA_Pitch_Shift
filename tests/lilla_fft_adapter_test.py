from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
build = root / ".pio/lilla_adapter_test"
build.mkdir(parents=True, exist_ok=True)
source = (root / "lilla/lib/AudioPlayer/AudioPlayer.cpp").read_text()
names = ["Fft_mode", "Read_fft_source", "Configure_fft_live_region", "Prepare_fft_live", "Start_fft", "Update_fft"]
functions = []
for name in names:
    marker = "AudioPlayer::" + name + "("
    begin = source.rfind("\n", 0, source.index(marker)) + 1
    opening = source.index("{", begin)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    functions.append(source[begin:end])
fixture = (root / "tests/lilla_fft_adapter_fixture.h").read_text()
tests = (root / "tests/lilla_fft_adapter_test.cpp").read_text()
unit = build / "adapter.cpp"
unit.write_text(fixture + "\nuint32_t AudioPlayer::fft_first_frame_cycle = UINT32_MAX;\n" + "\n".join(functions) + "\n" + tests)
exe = build / "adapter.exe"
subprocess.run(["C:/msys64/ucrt64/bin/g++.exe", "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror", "-static", "-I", str(root / "include"), str(root / "src/FftPitchCore.cpp"), str(root / "src/FftSpectralFrame.cpp"), str(root / "src/FftAttackCache.cpp"), str(unit), "-o", str(exe)], check=True)
subprocess.run([str(exe)], check=True)
