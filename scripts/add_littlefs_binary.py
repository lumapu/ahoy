import os
import subprocess
import shutil
Import("env")


def build_littlefs():
    result = subprocess.run(["pio", "run", "--target", "buildfs", "--environment", env['PIOENV']])
    if result.returncode != 0:
        print("Error building LittleFS:")
        exit(1)
    else:
        print("LittleFS build successful")

def merge_bins():
    flash_size = int(env.BoardConfig().get("upload.maximum_size", "4194304"))
    app0_offset = 0x10000
    if env['PIOENV'][:7] == "esp8266":
        app0_offset = 0
    elif env['PIOENV'][:7] == "esp8285":
        app0_offset = 0

    print(flash_size)
    littlefs_offset = 0x290000
    if flash_size == 0x330000:
        littlefs_offset = 0x670000
    elif flash_size == 0x640000:
        littlefs_offset = 0xc90000

    # save current wd
    start = os.getcwd()
    os.chdir('.pio/build/' + env['PIOENV'] + '/')

    with open("firmware.bin", "rb") as firmware_file:
        firmware_data = firmware_file.read()

    with open("littlefs.bin", "rb") as littlefs_file:
        littlefs_data = littlefs_file.read()

    with open("firmware.factory.bin", "wb") as merged_file:
        # fill gap with 0xff
        merged_file.write(firmware_data)
        if len(firmware_data) < (littlefs_offset - app0_offset):
            merged_file.write(b'\xFF' * ((littlefs_offset - app0_offset) - len(firmware_data)))

        merged_file.write(littlefs_data)

    os.chdir(start)

def main(target, source, env):
    build_littlefs()
    merge_bins()


# ensure that script is called once firmeware was compiled
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", main)
