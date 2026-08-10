cmake_minimum_required(VERSION 3.22.1)
project(ia CXX C)

set(IA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../..")
get_filename_component(IA_ROOT "${IA_ROOT}" ABSOLUTE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Match IA's desktop release build regardless of the gradle build type:
# NDEBUG gates IA's TRACE/ASSERT/PANIC paths (debug.hpp) and the render-order
# assertions, and -O2 keeps map generation fast enough to play.
add_compile_options(-O2)
add_compile_definitions(NDEBUG)

# Build paths must not travel in a shared artifact. IA's own asserts are
# compiled out by NDEBUG, but SDL's are not: SDL_assert bakes __FILE__ into
# libSDL2.so, which put the absolute build root - on CI, a path carrying the
# account name - into the APK. Rewrite the source root to a relative path, so
# those strings say "./third_party/SDL/..." wherever the build ran.
add_compile_options("-ffile-prefix-map=${IA_ROOT}=.")

# -----------------------------------------------------------------------------
# SDL2 + SDL2_image + SDL2_mixer, built as shared libs from the sources IA
# already vendors for its desktop static build (third_party/SDL/src).
# SDLActivity loads them by name, so they must be shared (unlike desktop).
# -----------------------------------------------------------------------------
set(SDL_PATH "${IA_ROOT}/third_party/SDL/src/SDL2-2.30.9")
set(SDL_IMAGE_PATH "${IA_ROOT}/third_party/SDL/src/SDL2_image-2.8.2")
set(SDL_MIXER_PATH "${IA_ROOT}/third_party/SDL/src/SDL2_mixer-2.8.0")

set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)

# Debuggable gradle variants compile with CMAKE_BUILD_TYPE=Debug, where the
# SDL libs default to a "d" filename suffix (libSDL2d.so) — which would break
# SDLActivity's loadLibrary("SDL2") at runtime. Force plain names.
set(SDL_CMAKE_DEBUG_POSTFIX "" CACHE STRING "" FORCE)
set(SDL2IMAGE_DEBUG_POSTFIX "" CACHE STRING "" FORCE)
set(SDL2MIXER_DEBUG_POSTFIX "" CACHE STRING "" FORCE)

# Image formats: PNG (decoded by SDL_image's built-in stb_image, so no
# external libpng is needed) and SVG (bundled nanosvg, used for the action
# bar icons). Mirrors cmake/sdl_build_static.cmake.
set(SDL2IMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_VENDORED OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_AVIF OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_BMP OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_GIF OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_JPG OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_JXL OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_LBM OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_PCX OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_PNG ON CACHE BOOL "" FORCE)
set(SDL2IMAGE_PNM OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_QOI OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_SVG ON CACHE BOOL "" FORCE)
set(SDL2IMAGE_TGA OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_TIF OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_WEBP OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_XCF OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_XPM OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_XV OFF CACHE BOOL "" FORCE)

# Audio codecs: all IA content is OGG (decoded by the built-in stb_vorbis).
# Mirrors cmake/sdl_build_static.cmake, plus MIDI/CMD off — timidity needs
# config files that don't exist on Android, and CMD is a unix fork() feature.
set(SDL2MIXER_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_VENDORED OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_CMD OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_OPUS OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MOD_XMP OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MOD OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MIDI OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MIDI_FLUIDSYNTH OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_WAVPACK OFF CACHE BOOL "" FORCE)

add_subdirectory(${SDL_PATH} SDL2-build)
add_subdirectory(${SDL_IMAGE_PATH} SDL2_image-build)
add_subdirectory(${SDL_MIXER_PATH} SDL2_mixer-build)

# -----------------------------------------------------------------------------
# The game, as libmain.so (the name SDLActivity's getLibraries() expects).
# src/*.cpp globs to exactly the source list in the top-level CMakeLists.txt
# (all 174 files, no subdirectories).
# -----------------------------------------------------------------------------
file(GLOB IA_SOURCES CONFIGURE_DEPENDS "${IA_ROOT}/src/*.cpp")

add_library(main SHARED
    ${IA_SOURCES}
    ${IA_ROOT}/third_party/tinyxml2/tinyxml2.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/android_bootstrap.cpp
)

target_include_directories(main PRIVATE
    ${IA_ROOT}/include
    ${IA_ROOT}/third_party/tinyxml2
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(main SYSTEM PRIVATE
    ${IA_ROOT}/third_party/mINI-0.9.17/src/mini
)

# Force-include SDL_main.h so IA's plain main() is renamed to SDL_main() with
# SDLMAIN_DECLSPEC export visibility — SDLActivity resolves it via dlsym.
set_source_files_properties(
    ${IA_ROOT}/src/main.cpp
    PROPERTIES COMPILE_OPTIONS "-include;SDL_main.h"
)

target_link_libraries(main PRIVATE
    SDL2
    SDL2_image
    SDL2_mixer
    log
    android
)
