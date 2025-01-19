# Guide for building Infra Arcana

## Fetching the source code

Clone the IA repository on GitLab:

    https://gitlab.com/martin-tornqvist/ia

## Which branch should I build?
**develop** - If you want to try out new features early (the develop branch should be relatively stable and bug free, feel free to open issues if you encounter bugs or crashes).

**tags (v15.0, v16.0, etc)** - If you want to build one of the official releases (note that the build method may be very different for older versions).

## Building Infra Arcana

Infra Arcana is built with [CMake](https://cmake.org/), which is a build system generator. CMake can generate GNU Makefiles, Code::Blocks projects, Visual Studio solutions, etc for many different platforms. Look for an online tutorial on how to use CMake (some pointers are given below).

This game uses SDL2, SDL2_image, and SDL_mixer. There are two ways to include these dependencies:
* Use the system installation of SDL on Linux, or use the bundled prebuilt version in Windows
* Build the bundled SDL source code and link it statically into the game binary.

The first method is the default behavior, it will be used if nothing else is specified. Buiding SDL from source and linking it statically is enabled via the CMake option "IA_BUILD_STATIC_SDL" (use -DIA_BUILD_STATIC_SDL=ON when running cmake to configure this mode). The "official" releases of the game are built with SDL statically linked, so there is no need to install any SDL dependencies to play those releases.

### Unix/Linux/macOS
You need CMake and build tools (e.g. GNU Make + gcc). Also you need dependencies to SDL2, SDL2-image, and SDL2-mixer (unless you want to use the option to build the bundled SDL source code).

To install dependencies on Debian, Ubuntu and other DEB-based systems, try:

    apt install build-essential cmake
    
    # Also, if using system installation of SDL (default method):
    apt install libsdl2-2.0-0 libsdl2-image-2.0-0 libsdl2-mixer-2.0-0

(Or whatever versions of the SDL2 libraries are available.)

On Fedora and other RPM-based systems, try:

    dnf install g++ make cmake
    
    # Also, if using system installation of SDL (default method):
    dnf install SDL2 SDL2_image SDL2_mixer

On macOS, using [Homebrew](https://brew.sh/):

    brew install cmake
    
    # Also, if using system installation of SDL (default method):
    brew install sdl2 sdl2_image sdl2_mixer

Now you can build IA (stand in the root of the IA repo and run these commands):

    mkdir -p build && cd build && cmake .. && make ia

Alternatively, you can build the bundled SDL source code and link it statically:

    mkdir build && cd build && cmake -DIA_BUILD_STATIC_SDL=ON .. && make ia

### Windows
You need CMake, and some IDE or build tools of your choice (such as [Code::Blocks](http://www.codeblocks.org), or [Visual Studio](https://www.visualstudio.com/)).

Run CMake. If you use the graphical interface, then for "Where is the source code?" select the root folder of the ia repo (NOT the "src" folder), and for "Where to build the binaries?" select a folder called "build" in the ia repo (the "build" folder may not actually exist yet, but it doesn't matter, just specify this path). Run "Configure" and "Generate".

You may need to set up some system environment variables to fix errors, depending on which type of project you are generating.

After running CMake, if everything went fine, the project (of the type that you selected) should be available in the "build" folder. Open this project and build the "ia" target.

For example, if you generated a Code::Blocks project, then in the drop-down target list (near the top of the screen) select the "ia" target. Build by clicking on the yellow cogwheel, then run the game by clicking on the green arrow.
