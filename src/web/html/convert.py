import re
import os
import gzip
import glob
import shutil
import pkg_resources
from datetime import date
from pathlib import Path

required_pkgs = {'dulwich'}
installed_pkgs = {pkg.key for pkg in pkg_resources.working_set}
missing_pkgs = required_pkgs - installed_pkgs

if missing_pkgs:
    env.Execute('"$PYTHONEXE" -m pip install dulwich')

from dulwich import porcelain

def get_git_sha():
    try:
        build_version = porcelain.describe('../../../')  # refers to the repository root dir
    except:
        build_version = "g0000000"

    build_flag = "-D AUTO_GIT_HASH=\\\"" + build_version[1:] + "\\\""
    #print ("Firmware Revision: " + build_version)
    return (build_flag)

def readVersion(path):
    f = open(path, "r")
    lines = f.readlines()
    f.close()

    today = date.today()
    search = ["_MAJOR", "_MINOR", "_PATCH"]
    version = today.strftime("%y%m%d") + "_ahoy_"
    ver = ""
    for line in lines:
        if(line.find("VERSION_") != -1):
            for s in search:
                p = line.find(s)
                if(p != -1):
                    version += line[p+13:].rstrip() + "."
                    ver += line[p+13:].rstrip() + "."
    return ver[:-1]

def htmlParts(file, header, nav, footer, version):
    p = "";
    f = open(file, "r")
    lines = f.readlines()
    f.close();

    f = open(header, "r")
    h = f.read().strip()
    f.close()

    f = open(nav, "r")
    n = f.read().strip()
    f.close()

    f = open(footer, "r")
    fo = f.read().strip()
    f.close()

    for line in lines:
        line = line.replace("{#HTML_HEADER}", h)
        line = line.replace("{#HTML_NAV}", n)
        line = line.replace("{#HTML_FOOTER}", fo)
        p += line

    #placeholders
    link = '<a target="_blank" href="https://github.com/lumapu/ahoy/commits/' + get_git_sha() + '">GIT SHA: ' + get_git_sha() + ' :: ' + version + '</a>'
    p = p.replace("{#VERSION}", version)
    p = p.replace("{#VERSION_GIT}", link)
    f = open("tmp/" + file, "w")
    f.write(p);
    f.close();
    return p

def convert2Header(inFile, version):
    fileType = inFile.split(".")[1]
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
            data = htmlParts(inFile, "includes/header.html", "includes/nav.html", "includes/footer.html", version)
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

# delete all files in the 'h' dir
wd = 'h'
if os.getcwd()[-4:] != "html":
    wd = "web/html/" + wd

if os.path.exists(wd):
    for f in os.listdir(wd):
        os.remove(os.path.join(wd, f))
wd += "/tmp"
if os.path.exists(wd):
    for f in os.listdir(wd):
        os.remove(os.path.join(wd, f))

# grab all files with following extensions
if os.getcwd()[-4:] != "html":
    os.chdir('./web/html')
types = ('*.html', '*.css', '*.js', '*.ico') # the tuple of file types
files_grabbed = []
for files in types:
    files_grabbed.extend(glob.glob(files))

Path("h").mkdir(exist_ok=True)
Path("tmp").mkdir(exist_ok=True) # created to check if webpages are valid with all replacements
shutil.copyfile("style.css", "tmp/style.css")
version = readVersion("../../defines.h")

# go throw the array
for val in files_grabbed:
    convert2Header(val, version)
