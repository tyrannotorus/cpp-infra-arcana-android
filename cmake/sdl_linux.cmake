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

target_link_libraries(ia PUBLIC         ${SDL_LIBS})
target_link_libraries(ia-debug PUBLIC   ${SDL_LIBS})

