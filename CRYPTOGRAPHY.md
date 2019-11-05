# Cryptographic features of ALT Media Writer

There are two separate checksum validation processes integrated in the ALT Media Writer.

## SHA1 hash

All ALT images have a SHA1 hash assigned when created. This hash is included in the release metadata that's included in ALT Media Writer and also in the releases.json file that's provided as a part of ALT Websites and served over HTTPS.

ALT Media Writer then computes SHA1 hash of the ISO data being downloaded to check if the image is counterfeit or not.

## Integrated MD5 ISO checksum

All ALT ISO images have an integrated MD5 checksum for integrity purposes.

ALT Media Writer verifies this checksum right after the image is downloaded and also after the image has been written to a flash drive to verify if it has been written correctly.
