# How to update assets to match getalt

## Copy files

Copy assets from getalt to appropriate locations in assets folder. The directory structure is similar to the one in getalt.

Required files: logos from "i/logos", sections file from "_data/sections" directory, images files from "_data/images" directory.

## Update assets.qrc

Add/update/remove entries in the qrc file so that all needed files are present there. In general, file aliases must match filenames, but logo filenames must match the "img" attribute of release in the sections files. This *could* be different from the filename!

## Troubleshooting
Run the app. Check for any errors or warnings to make sure that assets are setup correctly.
