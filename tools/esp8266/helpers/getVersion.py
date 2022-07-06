import os
from datetime import date

def readVersion(infile):
    f = open(infile, "r")
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

    version = version[:-1] + "_esp8266.bin"
    src = "../.pio/build/d1_mini/firmware.bin"
    dst = "../.pio/build/d1_mini/out/" + version
    os.mkdir("../.pio/build/d1_mini/out/")
    os.rename(src, dst)

readVersion("../defines.h")
