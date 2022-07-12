#!/usr/bin/python
import sys
f = open (sys.argv[1], 'rb').read()
for n, c in enumerate(f):
  if n % 16 == 0: print ('        "', end = '')
  print (f"\\x{c:02x}", end = '')
  if n % 16 == 15: print ('" \\')
if n % 16 != 15: print ('"')
