# Copyright (c) 2017 jean Davy (jean.davy.77@gmail.com)
# Distributed under the Boost Software License, Version 1.0. (http://www.boost.org/LICENSE_1_0.txt)
#
# chicho program is a file tree generator
# boost::program_options is required (http://www.boost.org/users/download/)
# Standalone asio is required (http://think-async.com/Asio)
# std::filesystem is required (implies a C++17 compiler VS2017, gcc7.2)
# cmake step by step:
# cmake -G "Unix Makefiles" .. -DBOOST_ROOT=/mnt/c/dev/boost_1_65_1 -DASIO_INCLUDE_DIR=/mnt/c/dev/asio-1.10.8/include
# cmake -G "Visual Studio 15 2017 Win64" .. -DBOOST_ROOT=C:/dev/boost_1_65_1 -DASIO_INCLUDE_DIR=C:/dev/asio-1.10.8/include
# cmake --build . --config Release

cmake_minimum_required(VERSION 3.4)

project(chicho
        LANGUAGES CXX)

find_package(Boost          REQUIRED)

IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Release)
ENDIF(NOT CMAKE_BUILD_TYPE)

add_definitions(
    -DBOOST_ALL_NO_LIB
    -DBUILD_SHARED_LIBS=OFF)

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

set_target_properties(chicho PROPERTIES
    CXX_STANDARD_REQUIRED 17)

include_directories(AFTER
    ${ASIO_INCLUDE_DIR}
    ${Boost_INCLUDE_DIR})

target_link_libraries(chicho
    program_options)

if (NOT NOSTATIC_RUNTIME)
    # using static runtime library in order to avoid install runtime shared lib
    if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        # https://cmake.org/Wiki/CMake_FAQ#How_can_I_build_my_MSVC_application_with_a_static_runtime.3F
        foreach(flag_var
                CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
                CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
           if(${flag_var} MATCHES "/MD")
              string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
           endif(${flag_var} MATCHES "/MD")
        endforeach(flag_var)
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU") # gcc
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++")
        target_link_libraries(chicho
            stdc++fs
            pthread)
    endif()
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU") # gcc
    target_link_libraries(chicho
        stdc++fs
        pthread)
endif()
