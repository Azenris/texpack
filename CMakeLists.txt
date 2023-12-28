
cmake_minimum_required( VERSION 3.26.3 )

project(
	app
	VERSION 1.0
	LANGUAGES CXX
)

set( APP_NAME texpack )

add_executable( app src/main.cpp )

option( BUILD_CRT_STATIC "CRT static link." ON )

target_compile_definitions( app PRIVATE C_PLUS_PLUS )
target_compile_definitions( app PRIVATE "$<$<CONFIG:Debug>:DEBUG>$<$<CONFIG:Release>:NDEBUG>" )
target_compile_definitions( app PRIVATE -DBUILD_TYPE="$<$<CONFIG:Debug>:DEBUG>$<$<CONFIG:Release>:RELEASE>" )
target_compile_definitions( app PRIVATE LITTLE_ENDIAN )
target_compile_definitions( app PRIVATE UNITY_BUILD )

target_compile_features( app PRIVATE cxx_std_20 )

target_include_directories( app PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/" )
target_include_directories( app PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/" )

if ( MSVC )
	target_compile_definitions( app PRIVATE _CRT_SECURE_NO_WARNINGS )
	target_compile_options( app PRIVATE -WX -W4 -wd4100 -wd4201 -wd4324 -wd4189 -Zc:preprocessor -Zc:strictStrings -GR- -EHsc )
	target_compile_options( app PRIVATE $<$<CONFIG:Debug>:-Z7 -FC> )
	target_compile_options( app PRIVATE $<$<CONFIG:Release>:-O2 -Ot -GF> )

	if ( BUILD_CRT_STATIC )
		target_compile_options( app PRIVATE $<$<CONFIG:Release>:-MT>$<$<CONFIG:Debug>:-MTd> )
	else()
		target_compile_options( app PRIVATE $<$<CONFIG:Release>:-MD>$<$<CONFIG:Debug>:-MDd> )
	endif()
endif()

if ( CMAKE_COMPILER_IS_GNUCC )
	target_compile_options( app PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-uninitialized -Wno-non-virtual-dtor -fno-rtti )
	target_compile_options( app PRIVATE $<$<CONFIG:Debug>:-OO -g> )
	target_compile_options( app PRIVATE $<$<CONFIG:Release>:-O2> )
endif()

if ( CMAKE_SYSTEM_NAME STREQUAL "Windows" )
	set( platform "PLATFORM_WINDOWS" )
endif()

if ( CMAKE_SYSTEM_NAME STREQUAL "Linux" )
	set( platform "PLATFORM_LINUX" )
endif()

if ( CMAKE_SYSTEM_NAME STREQUAL "Darwin" )
	set( platform "PLATFORM_MAC" )
endif()

target_compile_definitions( app PRIVATE ${platform} -DPLATFORM_ENGINE="${platform}" )

set_target_properties(
	app
	PROPERTIES
	OUTPUT_NAME                      "${APP_NAME}"
	RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/builds/debug"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/builds/release"
)