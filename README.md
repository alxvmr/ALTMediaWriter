# ALT Media Writer

ALT Media Writer is a tool that helps users put ALT images on their portable drives such as flash disks.

It is able to automatically download the required image for them and write them in a `dd`-like fashion, extracting compressed images if necessary.

This overwrites the drive's partition layout though so it also provides a way to restore a single-partition layout with a FAT32 partition.

![ALT Media Writer front page](/dist/screenshots/frontpage.png)

## Troubleshooting

If you experience any problem with the application, like crashes or errors when writing to your drives, please open an issue here on Github.

Please don't forget to attach the ALTMediaWriter.log file that will appear in your Documents folder ($HOME/Documents on Linux and Mac, C:\Users\<user>\Documents). It contains some non-sensitive information about your system and the log of all events happening during the runtime.

## Other information

For details about cryptography, see [CRYPTOGRAPHY.md](CRYPTOGRAPHY.md).

Some brief privacy information (regarding User-Agent strings) is in [PRIVACY.md](PRIVACY.md).
