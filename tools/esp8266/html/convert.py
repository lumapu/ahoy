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
    f = open(outName, "w")
    f.write("#ifndef __{}_H__\n".format(define))
    f.write("#define __{}_H__\n".format(define))
    f.write("const char {}[] PROGMEM = \"{}\";\n".format(inFile.replace(".", "_"), data))
    f.write("#endif /*__{}_H__*/\n".format(define))
    f.close()

convert2Header("index.html")
convert2Header("setup.html")
convert2Header("hoymiles.html")
convert2Header("style.css")
