from pathlib import Path

root = Path(__file__).resolve().parents[1]
reference = (root / "Granular_v0_6_8.hex").read_bytes().splitlines()
built = (root / ".pio/build/teensy41/firmware.hex").read_bytes().splitlines()
if reference != built:
    raise SystemExit("FAIL: rebuilt firmware differs from the hardware-tested 0.6.8 HEX")
print("PASS: rebuilt firmware matches every record of the hardware-tested 0.6.8 HEX")
