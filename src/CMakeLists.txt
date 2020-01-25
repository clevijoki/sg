cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(sg
	VERSION 0.1
	DESCRIPTION "SQL Game Framework"
	LANGUAGES CXX
)

add_subdirectory(game)
add_subdirectory(external/ads)
add_subdirectory(editor)
