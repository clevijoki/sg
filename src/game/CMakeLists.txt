cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories("../dependencies/SDL/include")

add_executable(game
	main
	SgCodeGen)

if (MSVC)
	list (APPEND EXTRA_LIBS
		debug ucrtd
		debug vcruntimed
		debug msvcrtd
		debug msvcprtd
		optimized ucrt
		optimized vcruntime
		optimized msvcrt
		optimized msvcprt
	)
endif()

target_link_libraries(game SDL2-static SDL2main ${EXTRA_LIBS})

