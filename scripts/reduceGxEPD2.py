import os
import subprocess
import glob
Import("env")

def rmDirWithFiles(path):
    if os.path.isdir(path):
        for f in glob.glob(path + "/*"):
            os.remove(f)
        os.rmdir(path)

def clean(libName):
    # save current wd
    start = os.getcwd()

    if os.path.exists('.pio/libdeps/' + env['PIOENV'] + '/' + libName) == False:
        print("path '" + '.pio/libdeps/' + env['PIOENV'] + '/' + libName + "' does not exist")
        return

    os.chdir('.pio/libdeps/' + env['PIOENV'] + '/' + libName)
    os.chdir('src/')
    types = ('epd/*.h', 'epd/*.cpp') # the tuple of file types
    files = []
    for t in types:
        files.extend(glob.glob(t))

    for f in files:
        if f.count('GxEPD2_150_BN') == 0:
            os.remove(f)

    rmDirWithFiles("epd3c")
    rmDirWithFiles("epd4c")
    rmDirWithFiles("epd7c")
    rmDirWithFiles("gdeq")
    rmDirWithFiles("gdey")
    rmDirWithFiles("it8951")

    os.chdir(start)


clean("GxEPD2")
