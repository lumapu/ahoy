import re
import os
import gzip
import glob
import shutil
import json
from datetime import date
from pathlib import Path
import subprocess
import configparser
Import("env")
build_flags = []

import htmlPreprocessorDefines as prepro

def getFlagsOfEnv(env):
    config = configparser.ConfigParser()
    config.read('platformio.ini')
    global build_flags
    flags = config[env]['build_flags'].split('\n')

    for i in range(len(flags)):
        if flags[i][:2] == "-D" or flags[i][:2] == "${":
            flags[i] = flags[i][2:]
        if flags[i][-13:-1] == ".build_flags":
            getFlagsOfEnv(flags[i].split(".build_flags")[0])
        elif len(flags[i]) > 0:
            build_flags = build_flags + [flags[i]]


def get_build_flags():
    getFlagsOfEnv("env:" + env['PIOENV'])
    config = configparser.ConfigParser()
    config.read('platformio.ini')

    # translate board
    board = config["env:" + env['PIOENV']]['board']
    if board == "esp12e" or board == "esp8285":
        build_flags.append("ESP8266")
    elif board == "lolin_d32":
        build_flags.append("ESP32")
    elif board == "lolin_s2_mini":
        build_flags.append("ESP32")
        build_flags.append("ESP32-S2")
    elif board == "lolin_c3_mini":
        build_flags.append("ESP32")
        build_flags.append("ESP32-C3")
    elif board == "esp32-s3-devkitc-1":
        build_flags.append("ESP32")
        build_flags.append("ESP32-S3")

def get_git_sha():
    try:
        return subprocess.check_output(['git', 'rev-parse', '--short', 'HEAD']).decode('ascii').strip()
    except:
        return "0000000"

def readVersion(path):
    f = open(path, "r")
    lines = f.readlines()
    f.close()

    today = date.today()
    search = ["_MAJOR", "_MINOR", "_PATCH"]
    ver = ""
    for line in lines:
        if(line.find("VERSION_") != -1):
            for s in search:
                p = line.find(s)
                if(p != -1):
                    ver += line[p+13:].rstrip() + "."
    return ver[:-1]

def readVersionFull(path):
    f = open(path, "r")
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
    version = version[:-1] + "_" + get_git_sha()
    return version

def htmlParts(file, header, nav, footer, versionPath, lang):
    f = open(file, "r")
    lines = f.readlines()
    f.close();

    f = open(header, "r")
    h = f.readlines()
    f.close()

    f = open(nav, "r")
    n = f.readlines()
    f.close()

    f = open(footer, "r")
    fo = f.readlines()
    f.close()

    linesExt = []
    for line in lines:
        if line.find("{#HTML_HEADER}") != -1:
            linesExt.extend(h)
        elif line.find("{#HTML_NAV}") != -1:
            linesExt.extend(n)
        elif line.find("{#HTML_FOOTER}") != -1:
            linesExt.extend(fo)
        else:
            linesExt.append(line)

    linesMod = prepro.conv(linesExt, build_flags)

    #placeholders
    version = readVersion(versionPath);
    link = '<a target="_blank" href="https://github.com/lumapu/ahoy/commits/' + get_git_sha() + '">GIT SHA: ' + get_git_sha() + ' :: ' + version + '</a>'
    p = ""
    for line in linesMod:
        p += line

    p = p.replace("{#VERSION}", version)
    p = p.replace("{#VERSION_FULL}", readVersionFull(versionPath))
    p = p.replace("{#VERSION_GIT}", link)

    p = translate(file, p, lang)
    p = translate("general", p, lang) # menu / header / footer

    f = open("tmp/" + file, "w")
    f.write(p);
    f.close();
    return p

def findLang(file):
    with open('../lang.json') as j:
        lang = json.load(j)

        for l in lang["files"]:
            if l["name"] == file:
                return l

    return None

def translate(file, data, lang="de"):
    json = findLang(file)

    if None != json:
        matches = re.findall(r'\{\#([A-Z0-9_]+)\}', data)
        for x in matches:
            for e in json["list"]:
                if x == e["token"]:
                    #print("replace " + "{#" + x + "}" + " with " + e[lang])
                    data = data.replace("{#" + x + "}", e[lang])
    return data


def convert2Header(inFile, versionPath, lang):
    fileType      = inFile.split(".")[1]
    define        = inFile.split(".")[0].upper()
    define2       = inFile.split(".")[1].upper()
    inFileVarName = inFile.replace(".", "_")

    if os.getcwd()[-4:] != "html":
        outName = "html/" + "h/" + inFileVarName + ".h"
        inFile  = "html/" + inFile
        Path("html/h").mkdir(exist_ok=True)
    else:
        outName = "h/" + inFileVarName + ".h"

    data = ""
    if fileType == "ico":
        f = open(inFile, "rb")
        data = f.read()
        f.close()
    else:
        if fileType == "html":
            data = htmlParts(inFile, "includes/header.html", "includes/nav.html", "includes/footer.html", versionPath, lang)
        else:
            f = open(inFile, "r")
            data = f.read()
            f.close()

    if fileType == "css":
        data = data.replace('\n', '')
        data = re.sub(r"(\;|\}|\:|\{)\s+", r'\1', data) # whitespaces inner css

    length = len(data)

    f = open(outName, "w")
    f.write("#ifndef __{}_{}_H__\n".format(define, define2))
    f.write("#define __{}_{}_H__\n".format(define, define2))

    if fileType == "ico":
        zipped = gzip.compress(bytes(data))
    else:
        zipped = gzip.compress(bytes(data, 'utf-8'))
    zippedStr = ""
    for i in range(len(zipped)):
        zippedStr += "0x{:02x}".format(zipped[i]) #hex(zipped[i])
        if (i + 1) != len(zipped):
            zippedStr += ", "
        if (i + 1) % 16 == 0 and i != 0:
            zippedStr += "\n"
    f.write("#define {}_len {}\n".format(inFileVarName, len(zipped)))
    f.write("const uint8_t {}[] PROGMEM = {{\n{}}};\n".format(inFileVarName, zippedStr))
    f.write("#endif /*__{}_{}_H__*/\n".format(define, define2))
    f.close()


def main():
    get_build_flags()

    # delete all files in the 'h' dir
    wd = 'web/html/h'

    if os.path.exists(wd):
        for f in os.listdir(wd):
            os.remove(os.path.join(wd, f))
    wd += "/tmp"
    if os.path.exists(wd):
        for f in os.listdir(wd):
            os.remove(os.path.join(wd, f))

    # grab all files with following extensions
    os.chdir('./web/html')
    types = ('*.html', '*.css', '*.js', '*.ico', '*.json') # the tuple of file types
    files_grabbed = []
    for files in types:
        files_grabbed.extend(glob.glob(files))

    Path("h").mkdir(exist_ok=True)
    Path("tmp").mkdir(exist_ok=True) # created to check if webpages are valid with all replacements
    shutil.copyfile("style.css", "tmp/style.css")

    # get language from environment
    lang = "en"
    if env['PIOENV'][-3:] == "-de":
        lang = "de"


    # go throw the array
    for val in files_grabbed:
        convert2Header(val, "../../defines.h", lang)


main()
