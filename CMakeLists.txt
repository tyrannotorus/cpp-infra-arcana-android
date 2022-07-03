# ------------------------------------------------------------------------------
# Project
# ------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.0)

project(ia)

SET(CMAKE_COLOR_MAKEFILE OFF)

# ------------------------------------------------------------------------------
# Source files
# ------------------------------------------------------------------------------
set(COMMON_SRC
        include/ability_values.hpp
        include/actor.hpp
        include/actor_act.hpp
        include/actor_cycle_graphics.hpp
        include/actor_data.hpp
        include/actor_death.hpp
        include/actor_eat.hpp
        include/actor_factory.hpp
        include/actor_hear_sound.hpp
        include/actor_hit.hpp
        include/actor_items.hpp
        include/actor_move.hpp
        include/actor_player_state.hpp
        include/actor_see.hpp
        include/actor_sneak.hpp
        include/actor_start_turn.hpp
        include/actor_std_turn.hpp
        include/ai.hpp
        include/array2.hpp
        include/attack.hpp
        include/attack_data.hpp
        include/audio.hpp
        include/audio_data.hpp
        include/bash.hpp
        include/bot.hpp
        include/browser.hpp
        include/bsp.hpp
        include/character_descr.hpp
        include/close.hpp
        include/colors.hpp
        include/common_text.hpp
        include/config.hpp
        include/config_options.hpp
        include/create_character.hpp
        include/debug.hpp
        include/direction.hpp
        include/disarm.hpp
        include/draw_blast.hpp
        include/draw_box.hpp
        include/draw_health_bars.hpp
        include/draw_map.hpp
        include/drop.hpp
        include/explosion.hpp
        include/flood.hpp
        include/fov.hpp
        include/game.hpp
        include/game_commands.hpp
        include/game_over.hpp
        include/game_time.hpp
        include/gfx.hpp
        include/global.hpp
        include/gods.hpp
        include/highscore.hpp
        include/hints.hpp
        include/info_screen_state.hpp
        include/init.hpp
        include/insanity.hpp
        include/inventory.hpp
        include/inventory_handling.hpp
        include/io.hpp
        include/item.hpp
        include/item_artifact.hpp
        include/item_att_property.hpp
        include/item_curio.hpp
        include/item_curse.hpp
        include/item_curse_ids.hpp
        include/item_data.hpp
        include/item_device.hpp
        include/item_factory.hpp
        include/item_potion.hpp
        include/item_rod.hpp
        include/item_scroll.hpp
        include/knockback.hpp
        include/line_calc.hpp
        include/main_menu.hpp
        include/manual.hpp
        include/map.hpp
        include/map_builder.hpp
        include/map_controller.hpp
        include/map_mode_gui.hpp
        include/map_parsing.hpp
        include/map_templates.hpp
        include/map_travel.hpp
        include/mapgen.hpp
        include/marker.hpp
        include/messages.hpp
        include/minimap.hpp
        include/misc.hpp
        include/msg_log.hpp
        include/panel.hpp
        include/pathfind.hpp
        include/paths.hpp
        include/pickup.hpp
        include/player_bon.hpp
        include/player_spells.hpp
        include/populate_items.hpp
        include/populate_monsters.hpp
        include/populate_traps.hpp
        include/popup.hpp
        include/pos.hpp
        include/postmortem.hpp
        include/property.hpp
        include/property_data.hpp
        include/property_factory.hpp
        include/property_handler.hpp
        include/query.hpp
        include/random.hpp
        include/rect.hpp
        include/reload.hpp
        include/room.hpp
        include/saving.hpp
        include/smell.hpp
        include/sound.hpp
        include/spells.hpp
        include/state.hpp
        include/teleport.hpp
        include/terrain.hpp
        include/terrain_data.hpp
        include/terrain_dmg.hpp
        include/terrain_door.hpp
        include/terrain_event.hpp
        include/terrain_factory.hpp
        include/terrain_gong.hpp
        include/terrain_mob.hpp
        include/terrain_monolith.hpp
        include/terrain_pylon.hpp
        include/terrain_trap.hpp
        include/text.hpp
        include/text_format.hpp
        include/throwing.hpp
        include/time.hpp
        include/version.hpp
        include/view.hpp
        include/view_actor_descr.hpp
        include/viewport.hpp
        include/wpn_dmg.hpp
        include/xml.hpp

        src/ability_values.cpp
        src/actor.cpp
        src/actor_act.cpp
        src/actor_cycle_graphics.cpp
        src/actor_data.cpp
        src/actor_death.cpp
        src/actor_eat.cpp
        src/actor_factory.cpp
        src/actor_hear_sound.cpp
        src/actor_hit.cpp
        src/actor_items.cpp
        src/actor_mon.cpp
        src/actor_move.cpp
        src/actor_player.cpp
        src/actor_player_state.cpp
        src/actor_see.cpp
        src/actor_sneak.cpp
        src/actor_start_turn.cpp
        src/actor_std_turn.cpp
        src/ai.cpp
        src/attack.cpp
        src/attack_data.cpp
        src/audio_data.cpp
        src/bash.cpp
        src/bot.cpp
        src/browser.cpp
        src/bsp.cpp
        src/character_descr.cpp
        src/close.cpp
        src/colors.cpp
        src/common_text.cpp
        src/config.cpp
        src/create_character.cpp
        src/debug.cpp
        src/direction.cpp
        src/disarm.cpp
        src/draw_blast.cpp
        src/draw_box.cpp
        src/draw_health_bars.cpp
        src/draw_map.cpp
        src/drop.cpp
        src/explosion.cpp
        src/flood.cpp
        src/fov.cpp
        src/game.cpp
        src/game_commands.cpp
        src/game_over.cpp
        src/game_time.cpp
        src/gfx.cpp
        src/gods.cpp
        src/highscore.cpp
        src/hints.cpp
        src/info_screen_state.cpp
        src/init.cpp
        src/insanity.cpp
        src/inventory.cpp
        src/inventory_handling.cpp
        src/item.cpp
        src/item_artifact.cpp
        src/item_curio.cpp
        src/item_curse.cpp
        src/item_data.cpp
        src/item_device.cpp
        src/item_factory.cpp
        src/item_potion.cpp
        src/item_rod.cpp
        src/item_scroll.cpp
        src/knockback.cpp
        src/line_calc.cpp
        src/main_menu.cpp
        src/manual.cpp
        src/map.cpp
        src/map_builder.cpp
        src/map_builder_special.cpp
        src/map_builder_std.cpp
        src/map_controller.cpp
        src/map_mode_gui.cpp
        src/map_parsing.cpp
        src/map_templates.cpp
        src/map_travel.cpp
        src/mapgen_aux_rooms.cpp
        src/mapgen_bsp_split_rooms.cpp
        src/mapgen_connect_rooms.cpp
        src/mapgen_decorate.cpp
        src/mapgen_doors.cpp
        src/mapgen_merge_regions.cpp
        src/mapgen_monolith.cpp
        src/mapgen_pylon.cpp
        src/mapgen_region.cpp
        src/mapgen_river.cpp
        src/mapgen_room.cpp
        src/mapgen_sub_rooms.cpp
        src/mapgen_utils.cpp
        src/marker.cpp
        src/messages.cpp
        src/minimap.cpp
        src/misc.cpp
        src/msg_log.cpp
        src/panel.cpp
        src/pathfind.cpp
        src/paths.cpp
        src/pickup.cpp
        src/player_bon.cpp
        src/player_spells.cpp
        src/populate_items.cpp
        src/populate_monsters.cpp
        src/populate_traps.cpp
        src/popup.cpp
        src/pos.cpp
        src/postmortem.cpp
        src/property.cpp
        src/property_data.cpp
        src/property_factory.cpp
        src/property_handler.cpp
        src/query.cpp
        src/random.cpp
        src/reload.cpp
        src/room.cpp
        src/saving.cpp
        src/smell.cpp
        src/sound.cpp
        src/spells.cpp
        src/state.cpp
        src/teleport.cpp
        src/terrain.cpp
        src/terrain_data.cpp
        src/terrain_dmg.cpp
        src/terrain_door.cpp
        src/terrain_event.cpp
        src/terrain_factory.cpp
        src/terrain_gong.cpp
        src/terrain_mob.cpp
        src/terrain_monolith.cpp
        src/terrain_pylon.cpp
        src/terrain_trap.cpp
        src/text.cpp
        src/text_format.cpp
        src/throwing.cpp
        src/time.cpp
        src/version.cpp
        src/view.cpp
        src/view_actor_descr.cpp
        src/viewport.cpp
        src/wpn_dmg.cpp
        src/xml.cpp

        xml/tinyxml2/tinyxml2.h
        xml/tinyxml2/tinyxml2.cpp
        )

SET(SRC
        ${COMMON_SRC}

        src/audio.cpp
        src/io.cpp
        src/io_animation.cpp
        src/io_coordinates.cpp
        src/io_draw_obj.cpp
        src/io_input.cpp
        src/io_primitives.cpp
        src/io_px_manip.cpp
        src/io_text.cpp
        src/io_window.cpp

        src/main.cpp
        )

# Adding tests should be convenient - making an exception to use globbing here
file(GLOB TEST_SRC
        ${COMMON_SRC}
        test/src/*.cpp
        test/test_cases/src/*.cpp
        test/include/*.hpp
        )

# ------------------------------------------------------------------------------
# Icon (on Windows)
# ------------------------------------------------------------------------------
set(RC_FILE "")

if(WIN32)
        set(RC_FILE ${CMAKE_SOURCE_DIR}/icon/icon.rc)
endif()

# ------------------------------------------------------------------------------
# Target definitions
# ------------------------------------------------------------------------------
add_executable(ia ${SRC} ${RC_FILE})
set_target_properties(ia PROPERTIES OUTPUT_NAME ia)

add_executable(ia-debug ${SRC} ${RC_FILE})

if(${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} GREATER_EQUAL 3.20)
        message(STATUS "cmake version >= 3.20, setting EXPORT_COMPILE_COMMANDS" for target ia-debug)

        set_target_properties(ia-debug PROPERTIES OUTPUT_NAME ia-debug EXPORT_COMPILE_COMMANDS 1)
else()
        message(STATUS "cmake version < 3.20 (will not set EXPORT_COMPILE_COMMANDS for debug target")

        set_target_properties(ia-debug PROPERTIES OUTPUT_NAME ia-debug)
endif()

enable_testing()

add_executable(ia-test ${TEST_SRC})
set_target_properties(ia-test PROPERTIES OUTPUT_NAME ia-test)
add_test(IA-test ia-test)

# GNU gcc and Clang specific compiler flags
if(CMAKE_COMPILER_IS_GNUCXX OR(CMAKE_C_COMPILER_ID MATCHES "Clang") OR(CMAKE_CXX_COMPILER_ID MATCHES "Clang"))

        set(COMMON_COMPILE_FLAGS
                -std=c++17
                -Wpedantic
                # Disabled for the same reason as -Werror, see comment below.
                # -pedantic-errors
                -Wall
                -Wextra
                # NOTE: While warnings should not be accepted, -Werror can cause
                # problems when building the game with different compilers or
                # compiler versions than it was developed on.
                # -Werror
                -Wmissing-declarations
                -Wunused
                -Wuninitialized
                -Wshadow
                )

        set(DEBUG_COMPILE_FLAGS
                -Wunused
                -Wnull-dereference
                # NOTE: For some reason, -O0 breaks the "ar" command when building
                # with mingw (the command seems to freeze forever), but -Og works.
                # -Og is also recommended by the gcc manual for debug builds.
                -Og
                -g
                -fsanitize=address
                -fsanitize=undefined
                -fno-sanitize-recover
                # Needed for full stack trace when running gcc sanitizers
                -fno-omit-frame-pointer
                #
                # Uncomment to generate gmon.out for gprof (must also be done for linker
                # flags, see below)
                # -pg
                )

        set(RELEASE_COMPILE_FLAGS
                -O2
                -Wno-unused-value
                )
endif()

# GNU gcc specific compiler flags
if(CMAKE_COMPILER_IS_GNUCXX)

        list(APPEND COMMON_COMPILE_FLAGS
                -Wuninitialized
                )
endif()

set(RELEASE_COMPILE_FLAGS
        ${RELEASE_COMPILE_FLAGS}
        -DNDEBUG
        )

set(TEST_COMPILE_FLAGS
        ${DEBUG_COMPILE_FLAGS}
        -fno-sanitize=all
        -fsanitize=undefined
        -DTRACE_LVL=0
        )
 
target_compile_options(ia PUBLIC
        ${COMMON_COMPILE_FLAGS}
        ${RELEASE_COMPILE_FLAGS}
        )

target_compile_options(ia-debug PUBLIC
        ${COMMON_COMPILE_FLAGS}
        ${DEBUG_COMPILE_FLAGS}
        )

target_compile_options(ia-test PUBLIC
        ${COMMON_COMPILE_FLAGS}
        ${TEST_COMPILE_FLAGS}
        )

set(COMMON_INCLUDE_DIRS
        include
        xml/tinyxml2
        )

target_include_directories(ia PUBLIC
        ${COMMON_INCLUDE_DIRS}
        )

target_include_directories(ia-debug PUBLIC
        ${COMMON_INCLUDE_DIRS}
        )

target_include_directories(ia-test PUBLIC
        ${COMMON_INCLUDE_DIRS}
        test/include
        test/Catch2-v2.13.9/include
        )

# On Windows releases, remove the console window
if(WIN32)
        # TODO: This solution only works with gcc/clang - not with MSVC
        if(CMAKE_COMPILER_IS_GNUCXX OR(CMAKE_C_COMPILER_ID MATCHES "Clang") OR(CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
                target_link_libraries(ia -mwindows)
        endif()

        if(CMAKE_COMPILER_IS_GNUCXX)
                target_link_libraries(ia -static-libgcc -static-libstdc++)
                target_link_libraries(ia-debug -static-libgcc -static-libstdc++)
        endif()
endif()

# Copy installed files to the build directory.
file(GLOB INSTALLED_FILES "installed_files/*")
file(COPY ${INSTALLED_FILES} DESTINATION .)

# ------------------------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------------------------
find_package(Git)

if(WIN32)

        if (NOT DEFINED ARCH)

                if("${CMAKE_SIZEOF_VOID_P}" EQUAL "4")
                        message(STATUS "Assuming 32 bit architecture")
                        set(ARCH 32bit)
                elseif("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
                        message(STATUS "Assuming 64 bit architecture")
                        set(ARCH 64bit)
                endif()

        endif()

        if (NOT DEFINED ARCH OR ((NOT "${ARCH}" EQUAL "32bit") AND (NOT "${ARCH}" EQUAL "64bit")))
                message(FATAL_ERROR "Unknown architecture")
        endif()

        set(SDL_BASE_DIR ${CMAKE_SOURCE_DIR}/SDL)

        if(MSVC)

                if("${ARCH}" EQUAL "32bit")
                        set(SDL_ARCH_DIR x86)
                else()
                        set(SDL_ARCH_DIR x64)
                endif()

                set(SDL2_PATH       ${SDL_BASE_DIR}/msvc/SDL2-2.0.9)
                set(SDL2_IMAGE_PATH ${SDL_BASE_DIR}/msvc/SDL2_image-2.0.5)
                set(SDL2_MIXER_PATH ${SDL_BASE_DIR}/msvc/SDL2_mixer-2.0.4)

                set(SDL_INCLUDE_DIRS
                        ${SDL2_PATH}/include
                        ${SDL2_IMAGE_PATH}/include
                        ${SDL2_MIXER_PATH}/include
                        )

                set(SDL2_LIBS_PATH          ${SDL2_PATH}/lib/${SDL_ARCH_DIR})
                set(SDL2_IMAGE_LIBS_PATH    ${SDL2_IMAGE_PATH}/lib/${SDL_ARCH_DIR})
                set(SDL2_MIXER_LIBS_PATH    ${SDL2_MIXER_PATH}/lib/${SDL_ARCH_DIR})

                set(SDL2_BINS_PATH          ${SDL2_PATH}/lib/${SDL_ARCH_DIR})
                set(SDL2_IMAGE_BINS_PATH    ${SDL2_IMAGE_PATH}/lib/${SDL_ARCH_DIR})
                set(SDL2_MIXER_BINS_PATH    ${SDL2_MIXER_PATH}/lib/${SDL_ARCH_DIR})

        else()

                # Not MSVC - e.g. gcc

                if("${ARCH}" EQUAL "32bit")
                        set(SDL_ARCH_DIR i686-w64-mingw32)
                else()
                        set(SDL_ARCH_DIR x86_64-w64-mingw32)
                endif()

                set(SDL2_PATH       ${SDL_BASE_DIR}/mingw/SDL2-2.0.9/${SDL_ARCH_DIR})
                set(SDL2_IMAGE_PATH ${SDL_BASE_DIR}/mingw/SDL2_image-2.0.5/${SDL_ARCH_DIR})
                set(SDL2_MIXER_PATH ${SDL_BASE_DIR}/mingw/SDL2_mixer-2.0.4/${SDL_ARCH_DIR})

                set(SDL_INCLUDE_DIRS
                        ${SDL2_PATH}/include/SDL2
                        ${SDL2_IMAGE_PATH}/include/SDL2
                        ${SDL2_MIXER_PATH}/include/SDL2
                        )

                set(SDL2_LIBS_PATH          ${SDL2_PATH}/lib)
                set(SDL2_IMAGE_LIBS_PATH    ${SDL2_IMAGE_PATH}/lib)
                set(SDL2_MIXER_LIBS_PATH    ${SDL2_MIXER_PATH}/lib)

                set(SDL2_BINS_PATH          ${SDL2_PATH}/bin)
                set(SDL2_IMAGE_BINS_PATH    ${SDL2_IMAGE_PATH}/bin)
                set(SDL2_MIXER_BINS_PATH    ${SDL2_MIXER_PATH}/bin)

                target_link_libraries(ia        mingw32)
                target_link_libraries(ia-debug  mingw32)

        endif()

        message(STATUS "SDL_INCLUDE_DIRS: " ${SDL_INCLUDE_DIRS})

        target_include_directories(ia       PUBLIC ${SDL_INCLUDE_DIRS})
        target_include_directories(ia-debug PUBLIC ${SDL_INCLUDE_DIRS})
        target_include_directories(ia-test  PUBLIC ${SDL_INCLUDE_DIRS})

        message(STATUS "SDL2_LIBS_PATH: "        ${SDL2_LIBS_PATH})
        message(STATUS "SDL2_IMAGE_LIBS_PATH: "  ${SDL2_IMAGE_LIBS_PATH})
        message(STATUS "SDL2_MIXER_LIBS_PATH: "  ${SDL2_MIXER_LIBS_PATH})

        find_library(SDL2_LIB_PATH          SDL2        PATHS ${SDL2_LIBS_PATH})
        find_library(SDL2_MAIN_LIB_PATH     SDL2main    PATHS ${SDL2_LIBS_PATH})
        find_library(SDL2_IMAGE_LIB_PATH    SDL2_image  PATHS ${SDL2_IMAGE_LIBS_PATH})
        find_library(SDL2_MIXER_LIB_PATH    SDL2_mixer  PATHS ${SDL2_MIXER_LIBS_PATH})

        message(STATUS "SDL2_LIB_PATH: "        ${SDL2_LIB_PATH})
        message(STATUS "SDL2_MAIN_LIB_PATH: "   ${SDL2_MAIN_LIB_PATH})
        message(STATUS "SDL2_IMAGE_LIB_PATH: "  ${SDL2_IMAGE_LIB_PATH})
        message(STATUS "SDL2_MIXER_LIB_PATH: "  ${SDL2_MIXER_LIB_PATH})

        set(SDL_LIBS
                ${SDL2_LIB_PATH}
                ${SDL2_MAIN_LIB_PATH}
                ${SDL2_IMAGE_LIB_PATH}
                ${SDL2_MIXER_LIB_PATH}
                )

        target_link_libraries(ia        ${SDL_LIBS})
        target_link_libraries(ia-debug  ${SDL_LIBS})

        # SDL dll files and licenses
        set(SDL_DISTR_FILES
                ${SDL2_BINS_PATH}/SDL2.dll
                ${SDL2_IMAGE_BINS_PATH}/SDL2_image.dll
                ${SDL2_IMAGE_BINS_PATH}/zlib1.dll
                ${SDL2_IMAGE_BINS_PATH}/libpng16-16.dll
                ${SDL2_IMAGE_BINS_PATH}/LICENSE.zlib.txt
                ${SDL2_IMAGE_BINS_PATH}/LICENSE.png.txt
                ${SDL2_MIXER_BINS_PATH}/SDL2_mixer.dll
                ${SDL2_MIXER_BINS_PATH}/libogg-0.dll
                ${SDL2_MIXER_BINS_PATH}/libvorbis-0.dll
                ${SDL2_MIXER_BINS_PATH}/libvorbisfile-3.dll
                ${SDL2_MIXER_BINS_PATH}/LICENSE.ogg-vorbis.txt
                )

        file(COPY ${SDL_DISTR_FILES} DESTINATION .)

        install(FILES ${SDL_DISTR_FILES} DESTINATION ia)
        install(FILES ${SDL_DISTR_FILES} DESTINATION ia-debug)

else()

        # Not windows (e.g. Unix)

        set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

        find_package(SDL2       REQUIRED)
        find_package(SDL2_image REQUIRED)
        find_package(SDL2_mixer REQUIRED)

        set(SDL_INCLUDE_DIRS
                ${SDL2_INCLUDE_DIR}
                ${SDL2_IMAGE_INCLUDE_DIR}
                ${SDL2_MIXER_INCLUDE_DIR}
                )

        target_include_directories(ia       PUBLIC ${SDL_INCLUDE_DIRS})
        target_include_directories(ia-debug PUBLIC ${SDL_INCLUDE_DIRS})
        target_include_directories(ia-test  PUBLIC ${SDL_INCLUDE_DIRS})

        set(SDL_LIBS
                ${SDL2_LIBRARY}
                ${SDL2_IMAGE_LIBRARIES}
                ${SDL2_MIXER_LIBRARIES}
                )

        target_link_libraries(ia-debug PUBLIC   -lasan -lubsan)
        target_link_libraries(ia-test PUBLIC    -lubsan)

        target_link_libraries(ia PUBLIC         ${SDL_LIBS})
        target_link_libraries(ia-debug PUBLIC   ${SDL_LIBS})

        # Uncomment to generate gmon.out for gprof (must also be done for compile
        # flags, see above)
        # NOTE: "-no-pie" is needed due to "a bug in gcc", see:
        # https://stackoverflow.com/questions/42620074
        # https://stackoverflow.com/questions/39827412
        # target_link_libraries(ia-debug PUBLIC -pg -no-pie)

endif()

# ------------------------------------------------------------------------------
# Retrieve abbreviated git commit
# ------------------------------------------------------------------------------
set(GENERATED_GIT_SHA1_PATH ${CMAKE_BINARY_DIR}/data/git-sha1.txt)

if(GIT_FOUND)
        message("Running revision command with Git executable: '${GIT_EXECUTABLE}'")

        message("Revision output path: '${GENERATED_GIT_SHA1_PATH}'")

        execute_process(
                COMMAND ${GIT_EXECUTABLE} log -1 --format=%h
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_STRIP_TRAILING_WHITESPACE
                OUTPUT_FILE ${GENERATED_GIT_SHA1_PATH}
                RESULT_VARIABLE REVISION_COMMAND_RESULT
                ERROR_VARIABLE REVISION_COMMAND_ERRORS
                )

        message("Revision command result: '${REVISION_COMMAND_RESULT}'")

        message("Revision command errors: '${REVISION_COMMAND_ERRORS}'")
else()
        message("Could not find Git - will not write revision")
endif()

# ------------------------------------------------------------------------------
# Packaging
# ------------------------------------------------------------------------------
set(CMAKE_INSTALL_PREFIX target)

install(TARGETS ia DESTINATION ia)
install(TARGETS ia-debug DESTINATION ia-debug)

install(DIRECTORY installed_files/ DESTINATION ia)
install(DIRECTORY installed_files/ DESTINATION ia-debug)

install(FILES ${GENERATED_GIT_SHA1_PATH} DESTINATION ia/data OPTIONAL)
install(FILES ${GENERATED_GIT_SHA1_PATH} DESTINATION ia-debug/data OPTIONAL)
