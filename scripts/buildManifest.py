import os
from datetime import date
import json

def readVersion(path, infile):
    f = open(path + infile, "r")
    lines = f.readlines()
    f.close()

    today = date.today()
    search = ["_MAJOR", "_MINOR", "_PATCH"]
    version = today.strftime("%y%m%d") + "_ahoy_"
    versionnumber = ""# "ahoy_v"
    for line in lines:
        if(line.find("VERSION_") != -1):
            for s in search:
                p = line.find(s)
                if(p != -1):
                    version += line[p+13:].rstrip() + "."
                    versionnumber += line[p+13:].rstrip() + "."

    return [versionnumber[:-1], version[:-1]]

def buildManifest(path, infile, outfile):
    version = readVersion(path, infile)
    sha = os.getenv("SHA",default="sha")
    data = {}
    data["name"] = "AhoyDTU - Development"
    data["version"] = version[0]
    data["new_install_prompt_erase"] = 1
    data["builds"] = []

    esp32 = {}
    esp32["chipFamily"] = "ESP32"
    esp32["parts"] = []
    esp32["parts"].append({"path": "bootloader.bin", "offset": 4096})
    esp32["parts"].append({"path": "partitions.bin", "offset": 32768})
    esp32["parts"].append({"path": "ota.bin", "offset": 57344})
    esp32["parts"].append({"path": version[1] + "_" + sha + "_esp32.bin", "offset": 65536})
    data["builds"].append(esp32)

    esp8266 = {}
    esp8266["chipFamily"] = "ESP8266"
    esp8266["parts"] = []
    esp8266["parts"].append({"path": version[1] + "_" + sha + "_esp8266.bin", "offset": 0})
    data["builds"].append(esp8266)

    jsonString = json.dumps(data, indent=2)

    fp = open(path + "firmware/" + outfile, "w")
    fp.write(jsonString)
    fp.close()

    
buildManifest("", "defines.h", "manifest.json")
