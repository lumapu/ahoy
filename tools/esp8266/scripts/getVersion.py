import os
from datetime import date
import subprocess
def get_git_revision_short_hash() -> str:
    return subprocess.check_output(['git', 'rev-parse', '--short', 'HEAD']).decode('ascii').strip()


def readVersion(path, infile):
    f = open(path + infile, "r")
    lines = f.readlines()
    f.close()

    today = date.today()
    search = ["_MAJOR", "_MINOR", "_PATCH"]
    version = today.strftime("%y%m%d") + "_ahoy_"
    for line in lines:
        if(line.find("VERSION_") != -1):
            for s in search:
                p = line.find(s)
                if(p != -1):
                    version += line[p+13:].rstrip() + "."
    
    os.mkdir(path + ".pio/build/out/")
    
    versionout = version[:-1] + "_esp8266_debug_g" + get_git_revision_short_hash() + ".bin"
    src = path + ".pio/build/esp8266-debug/firmware.bin"
    dst = path + ".pio/build/out/" + versionout
    os.rename(src, dst)
    
    versionout = version[:-1] + "_esp8266_release_g" + get_git_revision_short_hash() + ".bin"
    src = path + ".pio/build/esp8266-release/firmware.bin"
    dst = path + ".pio/build/out/" + versionout
    os.rename(src, dst)

readVersion("../", "defines.h")

