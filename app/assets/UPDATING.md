# How to update assets to match getalt

## Copy files

Copy assets from getalt to appropriate locations in assets folder. The directory structure is similar to the one in getalt.

Required files: logos from "i/logos", sections file from "_data/sections" directory, images files from "_data/images" directory.

## Update assets.qrc

Add/update/remove entries in the qrc file so that all needed files are present there. Note that file alias must match filenames.

## Update metadata.yml.
Update "release_names" if needed.
Make sure "getalt_images_location" is still valid.

## Troubleshooting
Run the app. Check for any errors or warnings to make sure that assets are setup correctly.

Each release name must have an accompanying images file.

Logo filenames must match the "img" attribute of release in the sections files. This *could* be different from the filename!
