# Copyright (C) 2021 Chris Morrison <gnosticist@protonmail.com>
# This file is subject to the license terms in the LICENSE file
# found in the top-level directory of this distribution.

set(TYPE_SAFE_STANDARDESE_DOWNLOAD_DIRECTORY "" CACHE PATH "path to directory with pre-built standardese")

find_package(standardese QUIET)
if(standardese_FOUND)
    standardese_generate(free-software-library CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/standardese.config INCLUDE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../include/ INPUT ${header_list})
endif()

