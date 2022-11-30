import os
import shutil
import gzip
from datetime import date

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

def readVersion(path, infile):
    f = open(path + infile, "r")
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
    
    os.mkdir(path + "firmware/")
    sha = os.getenv("SHA",default="sha")
    versionout = version[:-1] + "_esp8266_" + sha + ".bin"
    src = path + ".pio/build/esp8266-release/firmware.bin"
    dst = path + "firmware/" + versionout
    os.rename(src, dst)
    gzip_bin(dst, dst + ".gz")
    
    versionout = version[:-1] + "_esp8266_1m_" + sha + ".bin"
    src = path + ".pio/build/esp8285-release/firmware.bin"
    dst = path + "firmware/" + versionout
    os.rename(src, dst)
    gzip_bin(dst, dst + ".gz")

    versionout = version[:-1] + "_esp32_" + sha + ".bin"
    src = path + ".pio/build/esp32-wroom32-release/firmware.bin"
    dst = path + "firmware/" + versionout
    os.rename(src, dst)
    gzip_bin(dst, dst + ".gz")

    # other ESP32 bin files
    src = path + ".pio/build/esp32-wroom32-release/"
    dst = path + "firmware/"
    os.rename(src + "bootloader.bin", dst + "bootloader.bin")
    os.rename(src + "partitions.bin", dst + "partitions.bin")
    genOtaBin(path + "firmware/")
    os.rename("../scripts/gh-action-dev-build-flash.html", path + "install.html")

    print("name=" + versionnumber[:-1] )
    
    
readVersion("", "defines.h")
