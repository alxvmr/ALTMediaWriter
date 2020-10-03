#!/bin/bash

# See BUILDING.md for instructions

# Check that we're running script from it's directory
if [ ! -f build.sh ]
then
	echo "Error: run build.sh from it's directory"
	exit 1
fi

# Determine which step(or all) is executed by checking first arg
opt_dependencies=false
opt_build=false
opt_deploy=false

if [ $# -gt 0 ]
then
    case "$1" in
        dependencies) opt_dependencies=true
            ;;
        build) opt_build=true
            ;;
        deploy) opt_deploy=true
            ;;
        build_and_deploy)
            opt_build=true
            opt_deploy=true
            ;;
    esac
else
    opt_dependencies=true
	opt_build=true
	opt_deploy=true
fi

# 
# Install
# 
if [ $opt_dependencies = true ]
then
	PACKAGES="base-devel mingw-w64-i686-toolchain git mingw-w64-i686-qt5-static mingw-w64-i686-xz mingw-w64-i686-nsis mingw-w64-i686-mesa dos2unix mingw-w64-i686-yaml-cpp"

	pacman -S --needed --noconfirm $PACKAGES
fi

# Setup paths
SCRIPTDIR=$(pwd -P)
ROOTPATH=$(realpath "$SCRIPTDIR/../../")
BUILDPATH="$ROOTPATH/build"
DEPLOYPATH="$ROOTPATH/deploy"

# 
# Build
# 

if [ $opt_build = true ]
then
    cd "$ROOTPATH"
	
    echo "Building"

    # Remake build dir
    if [ -d $BUILDPATH ]
    then
        rm -r $BUILDPATH
    fi
    mkdir $BUILDPATH

	cd "$BUILDPATH"
	/mingw32/qt5-static/bin/qmake ..
	make -j12
fi

# 
# Deploy
# 

if [ $opt_deploy = true ]
then
    cd "$ROOTPATH"
    
    # Remake deploy dir
    if [ -d $DEPLOYPATH ]
    then
        rm -r $DEPLOYPATH
    fi
    mkdir $DEPLOYPATH

    echo "Copying files to deploy folder"

    echo "Copying executables"
    cp $BUILDPATH/app/release/mediawriter.exe $DEPLOYPATH 
    cp $BUILDPATH/helper/win/release/helper.exe $DEPLOYPATH 

	echo "Copying License"
	unix2dos < "$ROOTPATH/LICENSE" > "$DEPLOYPATH/LICENSE.txt"

	echo "Composing installer"

	# Get versions with git
	VERSION_FULL=$(git describe --tags)
	VERSION_STRIPPED=$(sed "s/-.*//" <<< "$VERSION_FULL")
	VERSION_MAJOR=$(cut -d. -f1 <<< "$VERSION_STRIPPED")
	VERSION_MINOR=$(cut -d. -f2 <<< "$VERSION_STRIPPED")
	VERSION_BUILD=$(cut -d. -f3 <<< "$VERSION_STRIPPED")
	INSTALLED_SIZE=$(du -k -d0 "$DEPLOYPATH" | cut -f1)

	# Bake versions into tmp .nsi script
	cp "$SCRIPTDIR/mediawriter.nsi" "$SCRIPTDIR/mediawriter.tmp.nsi"
	sed -i "s/#!define VERSIONMAJOR/!define VERSIONMAJOR $VERSION_MAJOR/" "$SCRIPTDIR/mediawriter.tmp.nsi"
	sed -i "s/#!define VERSIONMINOR/!define VERSIONMINOR $VERSION_MINOR/" "$SCRIPTDIR/mediawriter.tmp.nsi"
	sed -i "s/#!define VERSIONBUILD/!define VERSIONBUILD $VERSION_BUILD/" "$SCRIPTDIR/mediawriter.tmp.nsi"
	sed -i "s/#!define INSTALLSIZE/!define INSTALLSIZE $INSTALLED_SIZE/" "$SCRIPTDIR/mediawriter.tmp.nsi"

	# Run .nsi script
	makensis "$SCRIPTDIR/mediawriter.tmp.nsi" >/dev/null

	# Clean up
	rm "$SCRIPTDIR/mediawriter.tmp.nsi"
	rm "$ROOTPATH/tempinstaller.exe"

	echo "Composing installer complete"
fi