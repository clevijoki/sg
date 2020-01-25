cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

project(editor)

if (CMAKE_VERSION VERSION_LESS "3.7.0")
	set (CMAKE_INCLUDE_CURRENT_DIR ON)
endif()

include_directories(../external)

find_package(Qt5 COMPONENTS Widgets Sql REQUIRED)
find_package(GTest)

add_executable(editor
	main.cpp
	CodeGenerator.cpp
	ComponentMetaEditor.cpp
	ComponentPropertyMetaEditor.cpp
	ConnectDialog.cpp
	Connection.cpp
	Controller.cpp
	EntityEditor.cpp
	EntityPropertyEditor.cpp
	MainWindow.cpp
	MessageBox.cpp
	Result.cpp
	resources.qrc
)

target_link_libraries(editor Qt5::Widgets Qt5::Sql gtest pthread)
