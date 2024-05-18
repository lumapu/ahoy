# Generate factory firmware (ESP32)

If the firmware should already contain predefined settings this guide will help you to compile these into a single binary file.

## Generate default settings

First install on the requested platform the standard firmware and configure everything to your needs. Once you did all changes store them and export them to a `json` file.

## Further prepare default settings

First create a directory `data` inside the following project path: `src/`.

As the export removes all your password you need to add them again to the `json` file. Open the `json` file with a text editor and search for all the `"pwd": ""`. Between the second bunch of quotation marks you have to place the password.

*Note: It's recommended to keep all information in one line to save space on the ESP littlefs partition*

Next rename your export file to `settings.json` and move it to the new created directory. It should be look similar to this:

```
ahoy
  |-- src
        |-- data
              |-- settings.json
        |-- config
        |-- network
        ...
```

## modify platform.ini to build factory binary

Open the file `src/platformio.ini` and uncomment the following line `#post:../scripts/add_littlefs_binary.py` (remove the `#`)

## build firmware

Choose your prefered environment and build firmware as usual. Once the process is finished you should find along with the standard `firmware.bin` an additional file called `firmware.factory.bin`. Both files are located here: `src/.pio/build/[ENVIRONMENT]/`

## Upload to device

Navigate to the firmware output directory `src/.pio/build/[ENVIRONMENT]/` and open a terminal.

### ESP32

Python:
`esptool.py -b 921600 write_flash --flash_mode dio --flash_size detect 0x1000 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.factory.bin`

Windows:
`esptool.exe -b 921600 write_flash --flash_mode dio --flash_size detect 0x1000 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.factory.bin`

### ESP32-S3 (OpenDTU Fusion Board)

Python:
`esptool.py -b 921600 write_flash --flash_mode dio --flash_size detect 0x10000 firmware.factory.bin`

Windows:
`esptool.exe -b 921600 write_flash --flash_mode dio --flash_size detect 0x10000 firmware.factory.bin`

For a 4MB flash size the upload should be finished within 22 seconds.

## Testing

Reboot your ESP an check if all your settings are present.

## Keep updated with 'Mainline'

From time to time a new version of AhoyDTU will be published. To get this changes into your alread prepared factory binary generation environment you have to do only a few steps:

1. revert the changes of `platformio.ini` by executing from repository root: `git checkout src/platformio.ini`
2. pull new changes from remote: `git pull`
3. modify the `platformio.ini` again as you can read above (remove comment)
4. build and upload
5. enjoy
