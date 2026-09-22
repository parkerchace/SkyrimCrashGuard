# Copyright (C) 2024-2026 Parker Chace
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of Skyrim Crash Guard. See LICENSE for details.
#
# Populates CommonLibSSE NG's extern/openvr submodule without cloning the whole
# ValveSoftware/openvr history (~1 GB, several minutes). Only the headers and the
# x64 import library are needed, so this fetches the exact commit CommonLibSSE
# pins, blobless and sparse - a couple of seconds and well under a megabyte.
#
# Run as a script: cmake -D COMMONLIB_SOURCE_DIR=<dir> -P FetchOpenVR.cmake

if(NOT DEFINED COMMONLIB_SOURCE_DIR)
    message(FATAL_ERROR "FetchOpenVR.cmake: COMMONLIB_SOURCE_DIR is required")
endif()

set(_openvr_dir "${COMMONLIB_SOURCE_DIR}/extern/openvr")

if(EXISTS "${_openvr_dir}/headers/openvr.h" AND EXISTS "${_openvr_dir}/lib/win64/openvr_api.lib")
    message(STATUS "openvr: already populated at ${_openvr_dir}")
    return()
endif()

find_package(Git REQUIRED)

# The commit to fetch is whatever the pinned CommonLibSSE tag records for the
# submodule, read straight out of the checkout so the two can never drift.
execute_process(
    COMMAND "${GIT_EXECUTABLE}" ls-tree HEAD extern/openvr
    WORKING_DIRECTORY "${COMMONLIB_SOURCE_DIR}"
    OUTPUT_VARIABLE _ls_tree
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _ls_tree_result
)
if(NOT _ls_tree_result EQUAL 0 OR NOT _ls_tree MATCHES "commit[ \t]+([0-9a-f]+)")
    message(FATAL_ERROR "openvr: could not read the pinned submodule commit from ${COMMONLIB_SOURCE_DIR}")
endif()
set(_openvr_commit "${CMAKE_MATCH_1}")
message(STATUS "openvr: fetching pinned commit ${_openvr_commit}")

file(REMOVE_RECURSE "${_openvr_dir}")
file(MAKE_DIRECTORY "${_openvr_dir}")

macro(_openvr_git)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" ${ARGN}
        WORKING_DIRECTORY "${_openvr_dir}"
        RESULT_VARIABLE _git_result
        OUTPUT_QUIET
    )
    if(NOT _git_result EQUAL 0)
        message(FATAL_ERROR "openvr: 'git ${ARGN}' failed (${_git_result})")
    endif()
endmacro()

_openvr_git(init --quiet .)
_openvr_git(remote add origin https://github.com/ValveSoftware/openvr.git)
_openvr_git(config core.sparseCheckout true)
_openvr_git(sparse-checkout set --no-cone headers lib/win64)
_openvr_git(fetch --quiet --depth 1 --filter=blob:none origin "${_openvr_commit}")
_openvr_git(checkout --quiet FETCH_HEAD)

if(NOT EXISTS "${_openvr_dir}/headers/openvr.h" OR NOT EXISTS "${_openvr_dir}/lib/win64/openvr_api.lib")
    message(FATAL_ERROR "openvr: fetch completed but headers/lib are missing in ${_openvr_dir}")
endif()

message(STATUS "openvr: populated ${_openvr_dir}")
