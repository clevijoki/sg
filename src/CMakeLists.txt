cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(sg
	VERSION 0.1
	DESCRIPTION "SQL Game Framework"
	LANGUAGES CXX
)

function(message)
    if (NOT MESSAGE_QUIET)
        _message(${ARGN})
    endif()
endfunction()

# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(dependencies/googletest EXCLUDE_FROM_ALL)

set(MESSAGE_QUIET ON) # silence SDL
set(SDL_STATIC ON)
set(SDL_SHARED OFF)
set(FORCE_STATIC_VCRT ON)

add_subdirectory(dependencies/SDL)
set(MESSAGE_QUIET OFF)

add_subdirectory(editor)
add_subdirectory(game)
