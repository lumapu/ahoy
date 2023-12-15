import os
import subprocess
Import("env")

def applyPatch(libName, patchFile):
    # save current wd
    start = os.getcwd()

    if os.path.exists('.pio/libdeps/' + env['PIOENV'] + '/' + libName) == False:
        print("path '" + '.pio/libdeps/' + env['PIOENV'] + '/' + libName + "' does not exist")
        return

    os.chdir('.pio/libdeps/' + env['PIOENV'] + '/' + libName)

    process = subprocess.run(['git', 'apply', '--reverse', '--check', '../../../../' + patchFile], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if (process.returncode == 0):
        print('\'' + patchFile + '\' already applied')
    else:
        process = subprocess.run(['git', 'apply', '../../../../' + patchFile])
        if (process.returncode == 0):
            print('\'' + patchFile + '\' applied')
        else:
            print('applying \'' + patchFile + '\' failed')

    os.chdir(start)


# list of patches to apply (relative to /src)
if env['PIOENV'][:22] != "opendtufusion-ethernet":
    applyPatch("ESP Async WebServer", "../patches/AsyncWeb_Prometheus.patch")

if env['PIOENV'][:13] == "opendtufusion":
    applyPatch("GxEPD2", "../patches/GxEPD2_SW_SPI.patch")
if env['PIOENV'][:22] == "opendtufusion-ethernet":
    applyPatch("RF24", "../patches/RF24_Hal.patch")
