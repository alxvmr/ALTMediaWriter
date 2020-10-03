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
    esac
else
	opt_build=true
	opt_deploy=true
fi

# 
# Install
# 
if [ $opt_dependencies = true ]
then
	PACKAGES="base-devel mingw-w64-i686-toolchain mingw-w64-i686-qt5 mingw-w64-i686-xz  mingw-w64-i686-nsis mingw-w64-i686-mesa git dos2unix mingw-w64-i686-yaml-cpp"

	pacman -S --needed --noconfirm $PACKAGES
fi

# Setup paths
SCRIPTDIR=$(pwd -P)
ROOTPATH=$(realpath "$SCRIPTDIR/../../")
BUILDPATH="$ROOTPATH/build"

# 
# Build
# 

if [ $opt_build = true ]
then
    # if [ command -v cloc ] 
    if [ ! "$(command -v qmake)" ] || [ ! "$(command -v mingw32-make)" ]
    then
        echo "You need to run \"./build.sh dependencies\" to install required dependencies before building"
        exit 1
    fi

	echo "Building"

    # Clean everything so that version gets embedded correctly in makefiles
    find "$ROOTPATH" -name Makefile -delete
    find "$ROOTPATH" -name mediawriter.exe -delete

	cd "$ROOTPATH"
	qmake
    make clean
	make -j 8

	# Clear previous build results
	if [ -d $BUILDPATH ]
	then
		rm -r $BUILDPATH
	fi

	mkdir $BUILDPATH

	echo "Copying executables to build folder"
	cp $ROOTPATH/app/mediawriter.exe $BUILDPATH 
	cp $ROOTPATH/app/helper.exe $BUILDPATH 
	
	echo "Copying DLL's to build folder"

	windeployqt --compiler-runtime --qmldir app "$BUILDPATH/mediawriter.exe"
	
	DLLS="libssl-1_1 libcrypto-1_1 libbz2-1 libdouble-conversion libfreetype-6 libglib-2.0-0 libgraphite2 libharfbuzz-0 libiconv-2 libicudt65 libicuin65 libicuio65 libicutest65 libicutu65 libicuuc65 libpcre-1 libpcre2-16-0 libpcre16-0 libpng16-16 libzstd zlib1 libintl-8 opengl32 libglapi yaml-cpp"

	for dll in $DLLS
	do
		dll_file="/mingw32/bin/${dll}.dll"
		if [ ! -f $dll_file ]
		then
			echo "Error: couldn't find $dll_file, ensure you have installed all required dependencies"
			exit 1
		fi

		cp $dll_file $BUILDPATH
	done
fi

# 
# Deploy
# 

if [ $opt_deploy = true ]
then
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

	# Clean up
	rm "$SCRIPTDIR/mediawriter.tmp.nsi"
	rm "$ROOTPATH/tempinstaller.exe"

	echo "Composing installer complete"
fi