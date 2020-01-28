cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

project(editor)

if (CMAKE_VERSION VERSION_LESS "3.7.0")
	set (CMAKE_INCLUDE_CURRENT_DIR ON)
endif()

set(GTEST_ROOT "${CMAKE_SOURCE_DIR}/dependencies/googletest")

find_package(Qt5 COMPONENTS Widgets Sql REQUIRED)

add_executable(editor WIN32
	main.cpp
	CodeGenerator.cpp
	ComponentEditor.cpp
	ComponentList.cpp
	ComponentSelector.cpp
	ConnectDialog.cpp
	Connection.cpp
	Controller.cpp
	EntityEditor.cpp
	EntityList.cpp
	EntitySelector.cpp
	EntityGraphicsScene.cpp
	InitialSetup.cpp
	MainWindow.cpp
	MessageBox.cpp
	Result.cpp
	resources.qrc
)

if (UNIX)
	set(EDITOR_PLATFORM_LIBRARIES "pthread")
endif()

target_link_libraries(editor Qt5::Widgets Qt5::Sql gtest ${EDITOR_PLATFORM_LIBRARIES})

get_target_property(_qmake_executable Qt5::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")

if (WIN32)

	if (NOT DEFINED ENV{POSTGRES_BIN_DIR})
		message(FATAL_ERROR "POSTGRES_BIN_DIR is not set. This is require under windows to postcopy DLLs")
	endif()

	# this copies all of the Qt DLLs 
	add_custom_command(TARGET editor POST_BUILD
	    COMMAND "${CMAKE_COMMAND}" -E
	        env PATH="${_qt_bin_dir}" "${WINDEPLOYQT_EXECUTABLE}"
	            "$<TARGET_FILE:editor>"
	    COMMENT "Running windeployqt..."
	)

	# this copies all of the postgres DLLs, if this fails specify POSTGRES_BIN_DIR on the enviornment

	foreach (file_i "libcrypto-1_1-x64.dll" "libiconv-2.dll" "libintl-8.dll" "libssl-1_1-x64.dll" "libpq.dll")
	add_custom_command(TARGET editor POST_BUILD
	    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
	    		"$ENV{POSTGRES_BIN_DIR}/${file_i}"
	            "$<TARGET_FILE_DIR:editor>"
	    COMMENT "Copying postgres dlls..."
	)

	endforeach(file_i)

endif()