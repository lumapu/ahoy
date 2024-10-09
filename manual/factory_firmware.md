# Generate factory firmware (ESP32)

If the firmware should already contain predefined settings this guide will help you to compile these into a single binary file.

## Generate default settings

First install on the requested platform the standard firmware and configure everything to your needs. Once you did all changes store them and export them to a `json` file.

## Further prepare default settings

First create a directory `data` inside the following project path: `src/`.

As the export removes all your passwords you need to add them again to the `json` file. Open the `json` file with a text editor and search for all the `"pwd":""` sections. Between the second bunch of quotation marks you have to place the password.

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

## build firmware

Choose your prefered environment and build firmware as usual. Once the process is finished you should find along with the standard `firmware.bin` an additional file called `firmware.factory.bin`. Both files are located here: `src/.pio/build/[ENVIRONMENT]/`

## Upload to device

Navigate to the firmware output directory `src/.pio/build/[ENVIRONMENT]/` and open a terminal or vice versa.

Python:
`esptool.py -b 921600 write_flash --flash_mode dio --flash_size detect 0x0 firmware.factory.bin`

Windows:
`esptool.exe -b 921600 write_flash --flash_mode dio --flash_size detect 0x0 firmware.factory.bin`

The upload should be finished within one minute.

## Testing

Reboot your ESP an check if all your settings are present.

## Get updated with 'Mainline'

From time to time a new version of AhoyDTU will be published. To get the changes into your already prepared factory binary generation environment you have to do only a few steps:

1. pull new changes from remote: `git pull`
2. check if the `data` folder is still there and contains the `settings.json`
3. build and upload
4. enjoy
