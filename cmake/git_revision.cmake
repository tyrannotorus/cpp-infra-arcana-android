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
