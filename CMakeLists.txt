# Copyright (C) Chris Morrison <gnosticist@protonmail.com>
# This file is subject to the license terms in the LICENSE file
# found in the top-level directory of this distribution.

cmake_minimum_required(VERSION 3.20)

add_library(free-software-library INTERFACE)
target_include_directories(free-software-library INTERFACE include/)

file(GLOB header_list ${CMAKE_CURRENT_SOURCE_DIR}/include/fsl/*.hpp)
target_sources(free-software-library INTERFACE "$<BUILD_INTERFACE:${header_list}>)
target_include_directories(free-software-library INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>)
target_include_directories(free-software-library SYSTEM INTERFACE $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include>)

# Install target
install(DIRECTORY include/fsl DESTINATION include)

option(FSL_BUILD_DOC "generate documentation" OFF)
if(FSL_BUILD_DOC)
    add_subdirectory(doc/)
endif()