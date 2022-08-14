import re
import sys
import os

def convert2Header(inFile):
    fileType      = inFile.split(".")[1]
    define        = inFile.split(".")[0].upper()
    define2       = inFile.split(".")[1].upper()
    inFileVarName = inFile.replace(".", "_")

    if os.getcwd()[-4:] != "html":
        print("ok")
        outName = "html/" + "h/" + inFileVarName + ".h"
        inFile  = "html/" + inFile
    else:
        outName = "h/" + inFileVarName + ".h"

    f = open(inFile, "r")
    data = f.read().replace('\n', '')
    f.close()
    if fileType == "html":
        data = re.sub(r"\>\s+\<", '><', data)           # whitespaces between xml tags
        data = re.sub(r"(\;|\}|\>|\{)\s+", r'\1', data) # whitespaces inner javascript
        data = re.sub(r"\"", '\\\"', data)              # escape quotation marks
    else:
        data = re.sub(r"(\;|\}|\:|\{)\s+", r'\1', data) # whitespaces inner css

    f = open(outName, "w")
    f.write("#ifndef __{}_{}_H__\n".format(define, define2))
    f.write("#define __{}_{}_H__\n".format(define, define2))
    f.write("const char {}[] PROGMEM = \"{}\";\n".format(inFileVarName, data))
    f.write("#endif /*__{}_{}_H__*/\n".format(define, define2))
    f.close()

convert2Header("index.html")
convert2Header("setup.html")
convert2Header("visualization.html")
convert2Header("style.css")
