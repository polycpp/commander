cmake_minimum_required(VERSION 3.20)
project(polycpp_commander VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(POLYCPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    polycpp
    GIT_REPOSITORY git@github.com:enricohuang/polycpp.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(polycpp)

add_library(polycpp_commander
    src/commander.cpp
)
target_link_libraries(polycpp_commander PUBLIC polycpp)
target_include_directories(polycpp_commander PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

add_library(polycpp::commander ALIAS polycpp_commander)

option(POLYCPP_COMMANDER_BUILD_TESTS "Build tests" ON)
if(POLYCPP_COMMANDER_BUILD_TESTS)
    enable_testing()
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY git@github.com:google/googletest.git
        GIT_TAG        v1.15.2
    )
    FetchContent_MakeAvailable(googletest)

    add_executable(test_error tests/test_error.cpp)
    target_link_libraries(test_error PRIVATE polycpp_commander GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(test_error)
endif()
