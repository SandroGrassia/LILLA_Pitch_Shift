"""Export exact build bytes and make Teensy Loader select the named root HEX."""
from pathlib import Path
import hashlib

Import("env")


def copy_firmware(source, target, env):
    compiled = Path(env.subst("$BUILD_DIR/${PROGNAME}.hex"))
    exported = Path(env.subst("$PROJECT_DIR/${PROGNAME}.hex"))
    data = compiled.read_bytes()
    exported.write_bytes(data)
    if exported.read_bytes() != data:
        raise RuntimeError("Root HEX differs from compiled firmware")
    print("Verified firmware: " + str(exported))
    print("SHA256: " + hashlib.sha256(data).hexdigest())


env.AddPostAction("buildprog", copy_firmware)
env.AlwaysBuild("buildprog")
env.AddPreAction("upload", copy_firmware)
if env.subst("$UPLOAD_PROTOCOL") == "teensy-gui":
    flags = list(env["UPLOADERFLAGS"])
    flags = ['-path="$PROJECT_DIR"' if str(flag).startswith("-path=") else flag for flag in flags]
    env.Replace(UPLOADERFLAGS=flags)
