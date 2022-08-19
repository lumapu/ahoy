import os
from datetime import date

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
    
    os.mkdir(path + ".pio/build/out/")
    sha = os.getenv("SHA",default="sha")
    versionout = version[:-1] + "_esp8266_" + sha + ".bin"
    src = path + ".pio/build/esp8266-release/firmware.bin"
    dst = path + ".pio/build/out/" + versionout
    os.rename(src, dst)
    print("::set-output name=name::" + versionnumber[:-1] )
    
    
readVersion("../", "defines.h")
