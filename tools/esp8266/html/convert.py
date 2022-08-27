import re
import os
import gzip

def convert2Header(inFile, compress):
    fileType = inFile.split(".")[1]
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
    if False == compress:
        if fileType == "html":
            data = re.sub(r"\>\s+\<", '><', data)           # whitespaces between xml tags
            data = re.sub(r"(\;|\}|\>|\{)\s+", r'\1', data) # whitespaces inner javascript
            length = len(data)                              # get unescaped length
            data = re.sub(r"\"", '\\\"', data)              # escape quotation marks
        else:
            data = re.sub(r"(\;|\}|\:|\{)\s+", r'\1', data) # whitespaces inner css
            length = len(data)                              # get unescaped length
    else:
        data = re.sub(r"\>\s+\<", '><', data)           # whitespaces between xml tags
        data = re.sub(r"(\;|\}|\>|\{)\s+", r'\1', data) # whitespaces inner javascript
        length = len(data)                              # get unescaped length

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

convert2Header("index.html", False)
convert2Header("setup.html", True)
convert2Header("visualization.html", False)
convert2Header("update.html", False)
convert2Header("style.css", False)
