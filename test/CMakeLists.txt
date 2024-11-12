# Project: supercell-compression cli-tool


# C++ headers and source files
file(GLOB_RECURSE HEADERS source/*.h)
file(GLOB_RECURSE SOURCES source/*.cpp source/*.hpp)

set(TEST_TARGET supercell-compression-test)

add_executable(${TEST_TARGET} ${HEADERS} ${SOURCES})

set_target_properties(${TEST_TARGET} PROPERTIES
    FOLDER SupercellSDK/tools
)

target_include_directories(${TEST_TARGET} PUBLIC source)

target_link_libraries(${TEST_TARGET} PRIVATE supercell::compression)
