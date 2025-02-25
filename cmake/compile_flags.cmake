# GNU gcc and Clang specific compiler flags
if(
                CMAKE_COMPILER_IS_GNUCXX
                OR(CMAKE_C_COMPILER_ID MATCHES "Clang")
                OR(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
                )

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
                -Wnull-dereference
                -g
                # NOTE: For some reason, -O0 breaks the "ar" command when building
                # with mingw (the command seems to freeze forever), but -Og works.
                # -Og seems to be the preferable setting overall anyway.
                -Og
                # Uncomment to generate gmon.out for gprof (must also be done for linker
                # flags, see below)
                # -pg
                )

        set(TEST_COMPILE_FLAGS
                -Wnull-dereference
                -g
                -Og
                -Wno-unused-value
                )

        if(IA_DEBUG_SANITIZE)
                list(APPEND DEBUG_COMPILE_FLAGS
                        -fsanitize=address
                        -fsanitize=undefined
                        # -fsanitize=leak
                        -fsanitize=vptr
                        -fno-sanitize-recover
                        # Needed for full stack trace when running gcc sanitizers
                        -fno-omit-frame-pointer
                        )

                list(APPEND TEST_COMPILE_FLAGS
                        -fsanitize=undefined
                        # Needed for full stack trace when running gcc sanitizers
                        -fno-omit-frame-pointer
                        )
        endif()

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

list(APPEND RELEASE_COMPILE_FLAGS
        -DNDEBUG
        )

list(APPEND TEST_COMPILE_FLAGS
        -DIA_TEST
        )

target_compile_options(ia PUBLIC ${COMMON_COMPILE_FLAGS} ${RELEASE_COMPILE_FLAGS})
target_compile_options(ia-debug PUBLIC ${COMMON_COMPILE_FLAGS} ${DEBUG_COMPILE_FLAGS})
target_compile_options(ia-test PUBLIC ${COMMON_COMPILE_FLAGS} ${TEST_COMPILE_FLAGS})

set(COMMON_INCLUDE_DIRS include third_party/tinyxml2)

target_include_directories(ia PUBLIC ${COMMON_INCLUDE_DIRS})

target_include_directories(ia-debug PUBLIC ${COMMON_INCLUDE_DIRS})

target_include_directories(ia-test PUBLIC
        ${COMMON_INCLUDE_DIRS}
        test/include
        test/Catch2-v2.13.9/include
        )
