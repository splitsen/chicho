# cmake -G "Unix Makefiles" .. -DBOOST_ROOT=/mnt/c/dev/boost_1_65_1 -DASIO_INCLUDE_DIR=/mnt/c/dev/asio-1.10.8/include
# cmake -G "Visual Studio 15 2017" .. -DBOOST_ROOT=C:/dev/boost_1_65_1 -DASIO_INCLUDE_DIR=C:/dev/asio-1.10.8/include
#
# Should add static link multihread
# https://github.com/google/googletest/issues/266
cmake_minimum_required(VERSION 3.4)

project(chicho
        LANGUAGES CXX)

find_package(Boost          REQUIRED)

add_compile_options("-std=c++11")
add_definitions(-DBOOST_ALL_NO_LIB)

add_library(program_options
    ${BOOST_ROOT}/libs/program_options/src/cmdline.cpp
    ${BOOST_ROOT}/libs/program_options/src/config_file.cpp
    ${BOOST_ROOT}/libs/program_options/src/options_description.cpp
    ${BOOST_ROOT}/libs/program_options/src/parsers.cpp
    ${BOOST_ROOT}/libs/program_options/src/variables_map.cpp
    ${BOOST_ROOT}/libs/program_options/src/value_semantic.cpp
    ${BOOST_ROOT}/libs/program_options/src/positional_options.cpp
    ${BOOST_ROOT}/libs/program_options/src/utf8_codecvt_facet.cpp
    ${BOOST_ROOT}/libs/program_options/src/convert.cpp
    ${BOOST_ROOT}/libs/program_options/src/winmain.cpp
    ${BOOST_ROOT}/libs/program_options/src/split.cpp)

add_executable(chicho
    chicho.cpp)

include_directories(AFTER
        ${ASIO_INCLUDE_DIR}
        ${Boost_INCLUDE_DIR})

target_link_libraries(chicho
    program_options)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU") # gcc
    target_link_libraries(chicho
        stdc++fs
        pthread)
endif()
