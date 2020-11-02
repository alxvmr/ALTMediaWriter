# Linux
Build using standard ALTLinux build process.

# Windows
Windows build requries MSYS2 to be installed, install it from the official website. Make sure to complete all of the installation steps listed on the website!

Run mingw32.exe found in the MSYS2 folder.

Run build.sh found in "altmediawriter/dist/win". Possible run modes are:
* "./build.sh" - run all steps in order
* "./build.sh dependencies" - install required dependencies
* "./build.sh build" - build executables
* "./build.sh deploy" - construct an installer from executables
* "./build.sh build_and_deploy"
At the end you should be left with the installer file located in "deploy" folder.

To build without using build.sh, use "/mingw32/qt5-static/bin/qmake", NOT regular qmake because windows build is only setup for static build.

# Debugging
To show debug messages from the app, set the qt environment variable like this:

    export QT_LOGGING_RULES="*.debug=true;qt.*.debug=false"

To also show debug messages from Qt:

    export QT_LOGGING_RULES="*.debug=true"

On windows using an MSYS2/mingw console this shouldn't be needed.
