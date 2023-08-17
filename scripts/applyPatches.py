import os
import subprocess
Import("env")

def applyPatch(libName, patchFile):
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


# list of patches to apply (relative to /src)
applyPatch("ESP Async WebServer", "../patches/AsyncWeb_Prometheus.patch")
