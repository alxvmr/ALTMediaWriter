# ALT Media Writer

ALT Media Writer is a tool that helps users put ALT images on their portable drives such as flash disks.

It is able to automatically download the required image for them and write them in a `dd`-like fashion, using either `dd` itself or some other way to access the drive directly.

This overwrites the drive's partition layout though so it also provides a way to restore a single-partition layout with a FAT32 partition.

![ALT Media Writer running, with expanded image list](/dist/screenshots/linux_main.png)

## Troubleshooting

If you experience any problem with the application, like crashes or errors when writing to your drives, please open an issue here on Github.

Please don't forget to attach the ALTMediaWriter.log file that will appear in your Documents folder ($HOME/Documents on Linux and Mac, C:\Users\<user>\Documents). It contains some non-sensitive information about your system and the log of all events happening during the runtime.

## Building

You can build FMW using the default Qt `qmake` build system.

### Linux

You should specify the target directory using the `PREFIX` `qmake` variable. The default prefix path is `/usr/local`

It can be done like this:

`qmake PREFIX=/usr .`

The main binary, `mediawriter`, will be written to `$PREFIX/bin` and the helper binary can be found on the path `$PREFIX/libexec/mediawriter/helper`.

#### Requirements

* `udisks2` or `storaged`
* `liblzma`

### Windows

Building FMW in Windows is just the matter of running `qmake` and `make`.

To create a standalone package, use the `windeployqt` tool, included in your Qt installation. You will probably have to include a bunch of not included DLLs.

#### Requirements

* `xz-libs`

### macOS

Again, you can just run `qmake` and `make`.

To release a standalone package, use `macdeployqt`, supplied with your Qt installation.

#### Requirements

* `xz-libs`

## Translation

If you want to help with translating ALT Media Writer, please visit our [GetALT project page](https://getalt.org/development/writer).

Information about the individual ALT flavors is retrieved from the websites and translated as a separate project.

## Other information

For details about cryptography, see [CRYPTOGRAPHY.md](CRYPTOGRAPHY.md).

Some brief privacy information (regarding User-Agent strings) is in [PRIVACY.md](PRIVACY.md).
