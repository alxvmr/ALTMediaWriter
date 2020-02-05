#!/bin/bash

# Before running this script
# 
# Get liblzma(compiled for mingw)
#   copy to Qt/(version)/(mingw+version) AND Qt/Tools/(mingw+version)
# 
# Launch Qt commandline for mingw
# (In root of the app)
# qmake mediawriter.pro
# mingw32-make
# windeployqt --release app/release/mediawriter.exe
# 
# Copy all files inside dist/win/dlls into app/release
# 
# Launch MSYS2's mingw64 shell
# Install git, dos2unix and nsis with
#     pacman -S git dos2unix mingw-w64-x86_64-nsis
# Run build.sh from within it's folder


SCRIPTDIR=$(pwd -P)
ROOTPATH=$(realpath "$SCRIPTDIR/../../")
BUILDPATH="$ROOTPATH/app/release"
MEDIAWRITER="$ROOTPATH/app/release/mediawriter.exe"
HELPER="$ROOTPATH/app/helper.exe"

if [ ! -f "$MEDIAWRITER" ] || [ ! -f "$HELPER" ]; then
    echo "$MEDIAWRITER or $HELPER doesn't exist."
    exit 1
fi

cd $BUILDPATH

echo "=== Removing object and MOC files"
rm -f *.cpp
rm -f *.o
rm -f *.h

echo "=== Copying helper"
cp "$HELPER" .

echo "=== Copying License"
unix2dos < "$ROOTPATH/LICENSE" > "$BUILDPATH/LICENSE.txt"

echo "=== Composing installer"

# Get versions with git
VERSION_FULL=$(git describe --tags)
VERSION_STRIPPED=$(sed "s/-.*//" <<< "${VERSION_FULL}")
VERSION_MAJOR=$(cut -d. -f1 <<< "${VERSION_STRIPPED}")
VERSION_MINOR=$(cut -d. -f2 <<< "${VERSION_STRIPPED}")
VERSION_BUILD=$(cut -d. -f3 <<< "${VERSION_STRIPPED}")
INSTALLED_SIZE=$(du -k -d0 "$BUILDPATH" | cut -f1)

# Bake versions into tmp .nsi script
cp "$SCRIPTDIR/mediawriter.nsi" "$SCRIPTDIR/mediawriter.tmp.nsi"
sed -i "s/#!define VERSIONMAJOR/!define VERSIONMAJOR ${VERSION_MAJOR}/" "$SCRIPTDIR/mediawriter.tmp.nsi"
sed -i "s/#!define VERSIONMINOR/!define VERSIONMINOR ${VERSION_MINOR}/" "$SCRIPTDIR/mediawriter.tmp.nsi"
sed -i "s/#!define VERSIONBUILD/!define VERSIONBUILD ${VERSION_BUILD}/" "$SCRIPTDIR/mediawriter.tmp.nsi"
sed -i "s/#!define INSTALLSIZE/!define INSTALLSIZE ${INSTALLED_SIZE}/" "$SCRIPTDIR/mediawriter.tmp.nsi"

# Run .nsi script
makensis "$SCRIPTDIR/mediawriter.tmp.nsi" >/dev/null
rm "$SCRIPTDIR/mediawriter.tmp.nsi"

echo "=== Composing installer complete"
