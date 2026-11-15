from pathlib import Path

Import("env")

root = Path(env.subst("$PROJECT_DIR")) / "lilla" / "lib"
env.Append(CPPPATH=[str(path) for path in sorted(root.iterdir()) if path.is_dir()])
framework = Path(env.PioPlatform().get_package_dir("framework-arduinoteensy")) / "libraries"
env.Append(CPPPATH=[str(framework / name) for name in ("Wire", "SPI", "Audio", "SD", "SdFat", "SerialFlash")])
dependencies = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV")
if dependencies.is_dir():
    env.Append(CPPPATH=[str(path) for library in dependencies.iterdir() if library.is_dir() for path in (library, library / "src") if path.is_dir()])
