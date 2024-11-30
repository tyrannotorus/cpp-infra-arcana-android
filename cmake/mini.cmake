# Configure mINI (.ini file parser).

set(MINI_BASE_PATH third_party/mINI-0.9.17)

set(MINI_SRC_PATH ${MINI_BASE_PATH}/src)

target_include_directories(ia PUBLIC ${MINI_SRC_PATH})
target_include_directories(ia-debug PUBLIC ${MINI_SRC_PATH})
target_include_directories(ia-test PUBLIC ${MINI_SRC_PATH})

configure_file(
    ${CMAKE_SOURCE_DIR}/${MINI_BASE_PATH}/LICENSE
    ${CMAKE_BINARY_DIR}/LICENSE-mINI.txt
    COPYONLY
)
