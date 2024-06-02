import os
import subprocess
import shutil
from SCons.Script import DefaultEnvironment
Import("env")


def build_littlefs():
    if os.path.isfile('data/settings.json') == False:
        return # nothing to do

    result = subprocess.run(["pio", "run", "--target", "buildfs", "--environment", env['PIOENV']])
    if result.returncode != 0:
        print("Error building LittleFS:")
        exit(1)
    else:
        print("LittleFS build successful")

def merge_bins():
    if os.path.isfile('data/settings.json') == False:
        return # nothing to do

    BOOTLOADER_OFFSET = 0x0000
    PARTITIONS_OFFSET = 0x8000
    FIRMWARE_OFFSET   = 0x10000

    if env['PIOENV'][:13] == "esp32-wroom32":
        BOOTLOADER_OFFSET = 0x1000

    flash_size = int(env.BoardConfig().get("upload.maximum_size", "1310720")) # 0x140000
    app0_offset = 0x10000
    if env['PIOENV'][:7] == "esp8266":
        app0_offset = 0
    elif env['PIOENV'][:7] == "esp8285":
        app0_offset = 0

    littlefs_offset = 0x290000
    if flash_size == 0x330000:
        littlefs_offset = 0x670000
    elif flash_size == 0x640000:
        littlefs_offset = 0xc90000

    # save current wd
    start = os.getcwd()
    os.chdir('.pio/build/' + env['PIOENV'] + '/')

    with open("bootloader.bin", "rb") as bootloader_file:
        bootloader_data = bootloader_file.read()

    with open("partitions.bin", "rb") as partitions_file:
        partitions_data = partitions_file.read()

    with open("firmware.bin", "rb") as firmware_file:
        firmware_data = firmware_file.read()

    with open("littlefs.bin", "rb") as littlefs_file:
        littlefs_data = littlefs_file.read()

    with open("firmware.factory.bin", "wb") as merged_file:
        merged_file.write(b'\xFF' * BOOTLOADER_OFFSET)
        merged_file.write(bootloader_data)

        merged_file.write(b'\xFF' * (PARTITIONS_OFFSET - (BOOTLOADER_OFFSET + len(bootloader_data))))
        merged_file.write(partitions_data)

        merged_file.write(b'\xFF' * (FIRMWARE_OFFSET - (PARTITIONS_OFFSET + len(partitions_data))))
        merged_file.write(firmware_data)

        merged_file.write(b'\xFF' * (littlefs_offset - (FIRMWARE_OFFSET + len(firmware_data))))
        merged_file.write(littlefs_data)

    os.chdir(start)

def main(target, source, env):
    build_littlefs()
    merge_bins()

# ensure that script is called once firmeware was compiled
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", main)
