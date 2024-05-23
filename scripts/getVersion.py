import os
import shutil
import gzip
from datetime import date
import sys

def genOtaBin(path):
    arr = []
    arr.append(1)
    arr.append(0)
    arr.append(0)
    arr.append(0)
    for x in range(24):
        arr.append(255)
    arr.append(154)
    arr.append(152)
    arr.append(67)
    arr.append(71)
    for x in range(4064):
        arr.append(255)
    arr.append(0)
    arr.append(0)
    arr.append(0)
    arr.append(0)
    for x in range(4092):
        arr.append(255)
    with open(path + "ota.bin", "wb") as f:
        f.write(bytearray(arr))

# write gzip firmware file
def gzip_bin(bin_file, gzip_file):
    with open(bin_file,"rb") as fp:
        with gzip.open(gzip_file, "wb", compresslevel = 9) as f:
            shutil.copyfileobj(fp, f)

def getVersion(path_define):
    f = open(path_define, "r")
    lines = f.readlines()
    f.close()

    today = date.today()
    search = ["_MAJOR", "_MINOR", "_PATCH"]
    version = today.strftime("%y%m%d") + "_ahoy_"
    versionnumber = "ahoy_v"
    for line in lines:
        if(line.find("VERSION_") != -1):
            for s in search:
                p = line.find(s)
                if(p != -1):
                    version += line[p+13:].rstrip() + "."
                    versionnumber += line[p+13:].rstrip() + "."

    return [version, versionnumber]

def renameFw(path_define, env):
    version = getVersion(path_define)[0]
    
    os.mkdir("firmware/")
    fwDir = ""
    if env[:7] == "esp8266":
        fwDir = "ESP8266/"
    elif env[:7] == "esp8285":
        fwDir = "ESP8285/"
    elif env[:7] == "esp32-w":
        fwDir = "ESP32/"
    elif env[:8] == "esp32-s2":
        fwDir = "ESP32-S2/"
    elif env[:4] == "open":
        fwDir = "ESP32-S3/"
    elif env[:8] == "esp32-c3":
        fwDir = "ESP32-C3/"
    os.mkdir("firmware/" + fwDir)
    sha = os.getenv("SHA",default="sha")

    dst = "firmware/" + fwDir
    fname = version[:-1] + "_" + sha + "_" + env + ".bin"

    os.rename("src/.pio/build/" + env + "/firmware.bin", dst + fname)

    if env[:5] == "esp32":
        os.rename("src/.pio/build/" + env + "/bootloader.bin", dst + "bootloader.bin")
        os.rename("src/.pio/build/" + env + "/partitions.bin", dst + "partitions.bin")
        os.rename("src/.pio/build/" + env + "/firmware.elf.7z", dst + fname[:-3] + "elf.7z")
        genOtaBin(dst)

    if env[:7] == "esp8285":
        gzip_bin(dst + fname, dst + fname[:-4] + ".gz")

if len(sys.argv) == 1:
    print("name=" + getVersion("src/defines.h")[1][:-1])
else:
    # arg1: environment
    renameFw("src/defines.h", sys.argv[1])
