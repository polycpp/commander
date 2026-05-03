cmake_minimum_required(VERSION 3.20)
project(polycpp_commander VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# Fetch polycpp (disable its tests when building as a dependency).
# Allow a local checkout via POLYCPP_SOURCE_DIR for in-tree validation;
# fall back to the master branch on GitHub for external builds.
set(POLYCPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(POLYCPP_SOURCE_DIR "" CACHE PATH "Path to a local polycpp checkout")

if(POLYCPP_SOURCE_DIR AND EXISTS "${POLYCPP_SOURCE_DIR}/CMakeLists.txt")
    FetchContent_Declare(polycpp SOURCE_DIR "${POLYCPP_SOURCE_DIR}")
else()
    FetchContent_Declare(
        polycpp
        GIT_REPOSITORY git@github.com:enricohuang/polycpp.git
        GIT_TAG        master
        GIT_SHALLOW    TRUE
    )
endif()
FetchContent_MakeAvailable(polycpp)

# Library target
add_library(polycpp_commander
    src/commander.cpp
)
target_link_libraries(polycpp_commander PUBLIC polycpp)
target_include_directories(polycpp_commander PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
if(MSVC)
    target_compile_options(polycpp_commander PRIVATE /bigobj)
endif()

# Alias for consistent naming
add_library(polycpp::commander ALIAS polycpp_commander)

# Tests are on by default for standalone builds and off when consumed via
# FetchContent / add_subdirectory, so embedding a polycpp_commander pulls
# only the library target.
set(POLYCPP_COMMANDER_BUILD_TESTS_DEFAULT OFF)
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(POLYCPP_COMMANDER_BUILD_TESTS_DEFAULT ON)
endif()
option(POLYCPP_COMMANDER_BUILD_TESTS "Build tests"
       ${POLYCPP_COMMANDER_BUILD_TESTS_DEFAULT})

if(POLYCPP_COMMANDER_BUILD_TESTS)
    enable_testing()
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.15.2
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(googletest)

    include(GoogleTest)

    foreach(area IN ITEMS error argument option suggest_similar help command integration)
        set(test_target "polycpp_commander_test_${area}")
        add_executable(${test_target} tests/test_${area}.cpp)
        target_link_libraries(${test_target} PRIVATE polycpp::commander GTest::gtest_main)
        if(MSVC)
            target_compile_options(${test_target} PRIVATE /bigobj)
        endif()
        gtest_discover_tests(${test_target})
    endforeach()

    # Stand-alone executable subcommand tests need real on-disk programs to
    # spawn. Build small fixture binaries (NOT linked against polycpp_commander
    # — they run in a separate process) and pass the fixture target output
    # directory in via a compile definition so multi-config generators work.
    set(POLYCPP_COMMANDER_EXECUTABLE_FIXTURES echo_argv exit_with_code)
    if(NOT WIN32)
        list(APPEND POLYCPP_COMMANDER_EXECUTABLE_FIXTURES signal_writer)
    endif()

    foreach(fixture IN LISTS POLYCPP_COMMANDER_EXECUTABLE_FIXTURES)
        set(fixture_target "polycpp_commander_test_executable_fixture_${fixture}")
        add_executable(${fixture_target} tests/fixtures/${fixture}.cpp)
        set_target_properties(${fixture_target} PROPERTIES
            OUTPUT_NAME ${fixture}
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test_fixtures")
    endforeach()

    add_executable(polycpp_commander_test_executable tests/test_executable.cpp)
    target_link_libraries(polycpp_commander_test_executable PRIVATE
        polycpp::commander GTest::gtest_main)
    if(MSVC)
        target_compile_options(polycpp_commander_test_executable PRIVATE /bigobj)
    endif()
    target_compile_definitions(polycpp_commander_test_executable PRIVATE
        POLYCPP_COMMANDER_TEST_FIXTURE_DIR="$<TARGET_FILE_DIR:polycpp_commander_test_executable_fixture_echo_argv>"
        POLYCPP_COMMANDER_TEST_EXE_SUFFIX="${CMAKE_EXECUTABLE_SUFFIX}")
    foreach(fixture IN LISTS POLYCPP_COMMANDER_EXECUTABLE_FIXTURES)
        add_dependencies(polycpp_commander_test_executable
            "polycpp_commander_test_executable_fixture_${fixture}")
    endforeach()
    gtest_discover_tests(polycpp_commander_test_executable)
endif()

# Examples are off by default; turn on locally to build the runnable demos
# referenced by the docs site.
set(POLYCPP_COMMANDER_BUILD_EXAMPLES_DEFAULT OFF)
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(POLYCPP_COMMANDER_BUILD_EXAMPLES_DEFAULT ON)
endif()
option(POLYCPP_COMMANDER_BUILD_EXAMPLES "Build examples"
       ${POLYCPP_COMMANDER_BUILD_EXAMPLES_DEFAULT})

if(POLYCPP_COMMANDER_BUILD_EXAMPLES)
    file(GLOB POLYCPP_COMMANDER_EXAMPLES CONFIGURE_DEPENDS examples/*.cpp)
    foreach(example_source IN LISTS POLYCPP_COMMANDER_EXAMPLES)
        get_filename_component(example_name "${example_source}" NAME_WE)
        set(example_target "polycpp_commander_example_${example_name}")
        add_executable("${example_target}" "${example_source}")
        target_link_libraries("${example_target}" PRIVATE polycpp::commander)
        if(MSVC)
            target_compile_options("${example_target}" PRIVATE /bigobj)
        endif()
        set_target_properties("${example_target}" PROPERTIES
            OUTPUT_NAME "${example_name}"
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples"
        )
    endforeach()
endif()
