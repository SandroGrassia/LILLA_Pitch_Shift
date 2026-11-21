"""Derive build artifact names from the active firmware identity."""
from pathlib import Path
import re

Import("env")

config = Path(env.subst("$PROJECT_DIR")) / "lib/config/config.h"
text = config.read_text(encoding="utf-8")
match = re.search(r'FIRMWARE_VERSION\[\]\s*=\s*"LILLA_PitchShift v(\d+_\d+_\d+)( DIAG)?"', text)
if match is None:
    raise RuntimeError("Unsupported LILLA_PitchShift firmware identity")
name = "LIlla_PitchShift_v_" + match.group(1)
if match.group(2):
    name += "_diagnostic"
env.Replace(PROGNAME=name)
