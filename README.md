# Guide for building Infra Arcana

## Fetching the source code

Clone the IA repository on GitLab:

    https://gitlab.com/martin-tornqvist/ia

## Which branch should I build?
**develop** - If you want to try out new features early (the develop branch should be relatively stable and bug free, feel free to open issues if you encounter bugs or crashes).

**tags (v15.0, v16.0, etc)** - If you want to build one of the official releases (note that the build method may be very different for older versions).

## Building Infra Arcana

Infra Arcana is built with [CMake](https://cmake.org/), which is a build system generator. CMake can generate GNU Makefiles, Code::Blocks projects, Visual Studio solutions, etc for many different platforms. Look for an online tutorial on how to use CMake (some pointers are given below).

The source code of SDL2, SDL2_image and SDL2_mixer are included in the Infra Arcana Git repo. It is built automatically together with the IA build process, and then linked statically with the game - i.e. it is built into the game binary, so there is no need to install any SDL dependencies to play the game.

### Unix/Linux/macOS
You need CMake and build tools (e.g. GNU Make + gcc).

To install dependencies on Debian, Ubuntu and other DEB-based systems, try:

    apt install build-essential cmake

On Fedora and other RPM-based systems, try:

    dnf install g++ make cmake

On macOS, using [Homebrew](https://brew.sh/):

    brew install cmake

Now you can build IA:

    cd <ia directory>
    mkdir build
    cd build
    cmake ../
    make ia

### Windows
You need CMake, and some IDE or build tools of your choice (such as [Code::Blocks](http://www.codeblocks.org), or [Visual Studio](https://www.visualstudio.com/)).

Run CMake. If you use the graphical interface, then for "Where is the source code?" select the root folder of the ia repo (NOT the "src" folder), and for "Where to build the binaries?" select a folder called "build" in the ia repo (the "build" folder may not actually exist yet, but it doesn't matter, just specify this path). Run "Configure" and "Generate".

You may need to set up some system environment variables to fix errors, depending on which type of project you are generating.

After running CMake, if everything went fine, the project (of the type that you selected) should be available in the "build" folder. Open this project and build the "ia" target.

For example, if you generated a Code::Blocks project, then in the drop-down target list (near the top of the screen) select the "ia" target. Build by clicking on the yellow cogwheel, then run the game by clicking on the green arrow.
