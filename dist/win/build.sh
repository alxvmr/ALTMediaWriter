#!/bin/bash

# Usage
# Run this script from the mingw32 shell found in MSYS folder
# ./build.sh - builds and deploys
# ./build.sh install - install necessary packages
# ./build.sh build - compiles the executable and copies necessary dll's
# ./build.sh deploy - creates an installer

# Check that we're running script from it's directory
if [ ! -f build.sh ]
then
	echo "Error: run build.sh from it's directory"
	exit 1
fi

# Determine which step(or all) is executed by checking first arg
opt_install=false
opt_build=false
opt_deploy=false

if [ $# -gt 0 ]
then
    case "$1" in
        install) opt_install=true
            ;;
        build) opt_build=true
            ;;
        deploy) opt_deploy=true
            ;;
    esac
else
	opt_build=true
	opt_deploy=true
fi

# 
# Install
# 
if [ $opt_install = true ]
then
	PACKAGES="base-devel git dos2unix mingw-w64-i686-toolchain mingw-w64-i686-xz mingw-w64-i686-qt5 mingw-w64-i686-nsis"

	pacman -S --needed --noconfirm $PACKAGES
fi

# Setup paths
SCRIPTDIR=$(pwd -P)
ROOTPATH=$(realpath "$SCRIPTDIR/../../")
BUILDPATH="$ROOTPATH/app/release"
MEDIAWRITER="$ROOTPATH/app/release/mediawriter.exe"
HELPER="$ROOTPATH/app/helper.exe"

# 
# Build
# 

if [ $opt_build = true ]
then
	cd "$ROOTPATH"
	qmake -r "CONFIG+=release" mediawriter.pro
	mingw32-make
	windeployqt app/release/mediawriter.exe
	cp dist/win/dlls/* app/release/
fi

# 
# Deploy
# 

# Exit if not deploying
if [ $opt_deploy = false ]
then
	exit
fi

# Check that executables were built
if [ ! -f "$MEDIAWRITER" ] || [ ! -f "$HELPER" ]
then
    echo "$MEDIAWRITER or $HELPER doesn't exist."
    exit 1
fi

cd "$BUILDPATH"

echo "Removing object and MOC files"
rm -f *.cpp
rm -f *.o
rm -f *.h

echo "Copying helper"
cp "$HELPER" .

echo "Copying License"
unix2dos < "$ROOTPATH/LICENSE" > "$BUILDPATH/LICENSE.txt"

echo "Composing installer"

# Get versions with git
VERSION_FULL=$(git describe --tags)
VERSION_STRIPPED=$(sed "s/-.*//" <<< "$VERSION_FULL")
VERSION_MAJOR=$(cut -d. -f1 <<< "$VERSION_STRIPPED")
VERSION_MINOR=$(cut -d. -f2 <<< "$VERSION_STRIPPED")
VERSION_BUILD=$(cut -d. -f3 <<< "$VERSION_STRIPPED")
INSTALLED_SIZE=$(du -k -d0 "$BUILDPATH" | cut -f1)

# Bake versions into tmp .nsi script
cp "$SCRIPTDIR/mediawriter.nsi" "$SCRIPTDIR/mediawriter.tmp.nsi"
sed -i "s/#!define VERSIONMAJOR/!define VERSIONMAJOR $VERSION_MAJOR/" "$SCRIPTDIR/mediawriter.tmp.nsi"
sed -i "s/#!define VERSIONMINOR/!define VERSIONMINOR $VERSION_MINOR/" "$SCRIPTDIR/mediawriter.tmp.nsi"
sed -i "s/#!define VERSIONBUILD/!define VERSIONBUILD $VERSION_BUILD/" "$SCRIPTDIR/mediawriter.tmp.nsi"
sed -i "s/#!define INSTALLSIZE/!define INSTALLSIZE $INSTALLED_SIZE/" "$SCRIPTDIR/mediawriter.tmp.nsi"

# Run .nsi script
makensis "$SCRIPTDIR/mediawriter.tmp.nsi" >/dev/null
rm "$SCRIPTDIR/mediawriter.tmp.nsi"

echo "Composing installer complete"
