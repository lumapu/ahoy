import re
import os
import queue

def error(msg):
    print("ERROR: " + msg)
    exit()

def check(inp, lst, pattern):
    q = queue.LifoQueue()
    out = []
    keep = True
    for line in inp:
        x = re.findall(pattern, line)
        if len(x) > 0:
            if line.find("ENDIF_") != -1:
                if q.empty():
                    error("missing open statement!")
                if q.get() != x[0]:
                    error("wrong close statement!")
                keep = True
            elif line.find("IF_") != -1:
                q.put(x[0])
                if keep is True:
                    keep = x[0] in lst
            elif line.find("E") != -1:
                if q.empty():
                    error("missing open statement!")
                keep = not keep
        else:
            if keep is True:
                out.append(line)

    return out

def conv(inp, lst):
    print(lst)
    out = check(inp, lst, r'\/\*(?:IF_|ELS|ENDIF_)([A-Z0-9\-_]+)?\*\/')
    return check(out, lst, r'\<\!\-\-(?:IF_|ELS|ENDIF_)([A-Z0-9\-_]+)?\-\-\>')
