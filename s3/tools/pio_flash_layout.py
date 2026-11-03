# PlatformIO extra script (post:): after the firmware is built, write BUILD_DIR/flash-layout.json with what PlatformIO itself
# would hand to esptool to flash this env: the chip, the flash settings, and every image with its offset (bootloader, partition
# table, boot_app0 ... from FLASH_EXTRA_IMAGES, and the app at ESP32_APP_OFFSET). tools/dist.sh reads that file, so no offset is
# guessed anywhere. Used by env:panel-release (platformio.ini: extra_scripts = post:tools/pio_flash_layout.py).
import json
import os

Import("env")  # noqa: F821  (provided by PlatformIO/SCons)


def _write(source, target, env):
    build = env.subst("$BUILD_DIR")
    board = env.BoardConfig()
    images = []
    for off, path in env.get("FLASH_EXTRA_IMAGES", []):
        images.append({"offset": str(off), "file": os.path.realpath(env.subst(path))})
    app_off = env.subst("$ESP32_APP_OFFSET") or "0x10000"
    images.append({"offset": app_off, "file": os.path.realpath(env.subst("$BUILD_DIR/${PROGNAME}.bin"))})
    # the esptool flags PlatformIO builds for upload hold the flash settings of the image
    flags = [env.subst(str(x)) for x in env.get("UPLOADERFLAGS", [])]
    def flag(name):
        return flags[flags.index(name) + 1] if name in flags and flags.index(name) + 1 < len(flags) else None
    layout = {
        "mcu": board.get("build.mcu", ""),
        "flash_mode": flag("--flash-mode") or flag("--flash_mode"),
        "flash_freq": flag("--flash-freq") or flag("--flash_freq"),
        "flash_size": board.get("upload.flash_size", flag("--flash-size") or flag("--flash_size")),
        "partitions_csv": os.path.basename(env.subst("$PARTITIONS_TABLE_CSV")),
        "images": images,
    }
    with open(os.path.join(build, "flash-layout.json"), "w") as f:
        json.dump(layout, f, indent=2)
        f.write("\n")
    print("flash-layout.json written")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", _write)  # noqa: F821
