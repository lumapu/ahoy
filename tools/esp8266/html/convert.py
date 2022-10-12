import re
import os
import gzip
import glob

from pathlib import Path

def convert2Header(inFile, compress):
    fileType = inFile.split(".")[1]
    define        = inFile.split(".")[0].upper()
    define2       = inFile.split(".")[1].upper()
    inFileVarName = inFile.replace(".", "_")
    print(inFile + ", compress: " + str(compress))

    if os.getcwd()[-4:] != "html":
        outName = "html/" + "h/" + inFileVarName + ".h"
        inFile  = "html/" + inFile
        Path("html/h").mkdir(exist_ok=True)
    else:
        outName = "h/" + inFileVarName + ".h"
        Path("h").mkdir(exist_ok=True)

    f = open(inFile, "r")
    data = f.read()
    f.close()

    if fileType == "html":
        if False == compress:
            data = data.replace('\n', '')
            data = re.sub(r"\>\s+\<", '><', data)           # whitespaces between xml tags
            data = re.sub(r"(\r\n|\r|\n)(\s+|\s?)", '', data)     # whitespaces inner javascript
        length = len(data)                              # get unescaped length
        if False == compress:
            data = re.sub(r"\"", '\\\"', data)          # escape quotation marks
    elif fileType == "js":
        #data = re.sub(r"(\r\n|\r|\n)(\s+|\s?)", '', data)     # whitespaces inner javascript
        #data = re.sub(r"\s?(\=|\!\=|\{|,)+\s?", r'\1', data) # whitespaces inner javascript
        length = len(data)                              # get unescaped length
        if False == compress:
            data = re.sub(r"\"", '\\\"', data)          # escape quotation marks
    else:
        data = data.replace('\n', '')
        data = re.sub(r"(\;|\}|\:|\{)\s+", r'\1', data) # whitespaces inner css
        length = len(data)                              # get unescaped length                           # get unescaped length

    f = open(outName, "w")
    f.write("#ifndef __{}_{}_H__\n".format(define, define2))
    f.write("#define __{}_{}_H__\n".format(define, define2))
    if compress:
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
    else:
        f.write("const char {}[] PROGMEM = \"{}\";\n".format(inFileVarName, data))
        f.write("const uint32_t {}_len = {};\n".format(inFileVarName, length))
    f.write("#endif /*__{}_{}_H__*/\n".format(define, define2))
    f.close()

# Todo: delete all, but ignore 'favicon_ico_gz.h'
# delete all files in the 'h' dir
#dir = './html/h'
#for f in os.listdir(dir):
#    os.remove(os.path.join(dir, f))

# grab all files with following extensions
os.chdir('./html')
types = ('*.html', '*.css', '*.js') # the tuple of file types
files_grabbed = []
for files in types:
    files_grabbed.extend(glob.glob(files))

# go throw the array
for val in files_grabbed:
    convert2Header(val, True)