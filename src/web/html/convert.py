import re
import os
import gzip
import glob

from pathlib import Path

def convert2Header(inFile):
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
        Path("h").mkdir(exist_ok=True)

    if fileType == "ico":
        f = open(inFile, "rb")
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

# delete all files in the 'h' dir, but ignore 'favicon_ico_gz.h'
dir = 'h'
if os.getcwd()[-4:] != "html":
    dir = "web/html/" + dir

for f in os.listdir(dir):
    #if not f.startswith('favicon_ico_gz'):
    os.remove(os.path.join(dir, f))

# grab all files with following extensions
if os.getcwd()[-4:] != "html":
    os.chdir('./web/html')
types = ('*.html', '*.css', '*.js', '*.ico') # the tuple of file types
files_grabbed = []
for files in types:
    files_grabbed.extend(glob.glob(files))

# go throw the array
for val in files_grabbed:
    convert2Header(val)
