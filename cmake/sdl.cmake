set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

add_subdirectory(SDL/src/SDL2-2.30.9)
add_subdirectory(SDL/src/SDL2_image-2.8.2)
add_subdirectory(SDL/src/SDL2_mixer-2.8.0)

set(SDL_LIBRARIES SDL2::SDL2-static SDL2_image::SDL2_image-static SDL2_mixer::SDL2_mixer-static)

target_link_libraries(ia        ${SDL_LIBRARIES})
target_link_libraries(ia-debug  ${SDL_LIBRARIES})
   
# TODO: Copy license files
# configure_file(${SDL_PATH}/LICENSE.txt          LICENSE-SDL.txt         COPYONLY)
# configure_file(${SDL_IMAGE_PATH}/LICENSE.txt    LICENSE-SDL_image.txt   COPYONLY)
# configure_file(${SDL_MIXER_PATH}/LICENSE.txt    LICENSE-SDL_mixer.txt   COPYONLY)

# install(FILES ${SDL_DISTR_FILES} DESTINATION ia)
# install(FILES ${SDL_DISTR_FILES} DESTINATION ia-debug)
