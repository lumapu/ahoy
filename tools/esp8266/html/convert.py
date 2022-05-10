import re

def convert2Header(inFile):
    outName  = "h/" + inFile.replace(".", "_") + ".h"
    fileType = inFile.split(".")[1]

    f = open(inFile, "r")
    data = f.read().replace('\n', '')
    f.close()
    if fileType == "html":
        data = re.sub(r"\>\s+\<", '><', data)           # whitespaces between xml tags
        data = re.sub(r"(\;|\}|\>|\{)\s+", r'\1', data) # whitespaces inner javascript
        data = re.sub(r"\"", '\\\"', data)              # escape quotation marks
    else:
        data = re.sub(r"(\;|\}|\:|\{)\s+", r'\1', data) # whitespaces inner css

    define = inFile.split(".")[0].upper()
    define2 = inFile.split(".")[1].upper()
    f = open(outName, "w")
    f.write("#ifndef __{}_{}_H__\n".format(define, define2))
    f.write("#define __{}_{}_H__\n".format(define, define2))
    f.write("const char {}[] PROGMEM = \"{}\";\n".format(inFile.replace(".", "_"), data))
    f.write("#endif /*__{}_{}_H__*/\n".format(define, define2))
    f.close()

convert2Header("index.html")
convert2Header("setup.html")
convert2Header("hoymiles.html")
convert2Header("style.css")
