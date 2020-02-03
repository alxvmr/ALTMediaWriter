# ALT Media Writer

ALT Media Writer is a tool that helps users put ALT images on their portable drives such as flash disks.

It is able to automatically download the required image for them and write them in a `dd`-like fashion, using either `dd` itself or some other way to access the drive directly.

This overwrites the drive's partition layout though so it also provides a way to restore a single-partition layout with a FAT32 partition.

![ALT Media Writer front page](/dist/screenshots/frontpage.png)

## Troubleshooting

If you experience any problem with the application, like crashes or errors when writing to your drives, please open an issue here on Github.

Please don't forget to attach the ALTMediaWriter.log file that will appear in your Documents folder ($HOME/Documents on Linux and Mac, C:\Users\<user>\Documents). It contains some non-sensitive information about your system and the log of all events happening during the runtime.

## Building

### ALT Linux

First run `qmake-qt5 mediawriter.pro`. Then run `make` from the root of the project.

#### Requirements

* `qt5`
* `udisks2` or `storaged`
* `liblzma`

### Windows

Open Qt-mingw cmd shortcut. First run `qmake mediawriter.pro`. Then run `mingw32-make` from the root of the project. Note that the resulting release will miss many dll's and the app won't work completely. To get missing dll's run `windeployqt` and also copy contents of dist/win/dlls into app/releases folder.

#### Requirements

* `qt5`
* `mingw`
* `xz-libs` (compiled for mingw)

### macOS

First run `qmake mediawriter.pro`. Then run `make` from the root of the project.

#### Requirements

* `qt5`
* `xz-libs`

## Deploying

### ALT Linux

Run `gear-rpm -ba -v` from the root of the project.

### Windows

You need msys to deploy on windows. Follow directions written inside `dist/win/build.sh`, then run `dist/win/build.sh` in msys.

### macOS

Run `dist/mac/build.sh`.

## Other information

For details about cryptography, see [CRYPTOGRAPHY.md](CRYPTOGRAPHY.md).

Some brief privacy information (regarding User-Agent strings) is in [PRIVACY.md](PRIVACY.md).
