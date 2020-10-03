# Linux
Build using standard ATLinux build process.

# Windows
Windows build requries MSYS2 to be installed, so install it from the official website. Make sure to complete all of the installation steps listed on the website!

Open mingw32 shell, it can be found in the MSYS2 folder.

Run build.sh found in "dist/win" folder. It has the following steps:
* build.sh dependencies - install required dependencies
* build.sh build - compiles the executable and copies necessary dll's
* build.sh deploy - creates an installer
* build.sh - do all steps
