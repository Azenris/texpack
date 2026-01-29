cmake_minimum_required( VERSION 3.26.3 )

project( app VERSION 1.0 LANGUAGES CXX )

set( APP_NAME texpack )

option( BUILD_CRT_STATIC "CRT static link." ON )

if ( BUILD_CRT_STATIC )
	set( CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" )
else()
	set( CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" )
endif()

add_executable( app src/main.cpp )

target_compile_features( app PRIVATE cxx_std_23 )

set_target_properties(
	app
	PROPERTIES
	OUTPUT_NAME "${APP_NAME}"
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF
	RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin/debug"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin/release"
)

target_include_directories( app PRIVATE src/ third_party/ )

target_compile_definitions( app PRIVATE 
	C_PLUS_PLUS
	LITTLE_ENDIAN
	UNITY_BUILD
	$<$<CONFIG:Debug>:DEBUG>
	$<$<CONFIG:Release>:NDEBUG>
	BUILD_TYPE="$<$<CONFIG:Debug>:DEBUG>$<$<CONFIG:Release>:RELEASE>"
	PLATFORM_WINDOWS=$<$<PLATFORM_ID:Windows>:1>
	PLATFORM_LINUX=$<$<PLATFORM_ID:Linux>:1>
	PLATFORM_MAC=$<$<PLATFORM_ID:Darwin>:1>
)

if ( MSVC )
	target_compile_definitions( app PRIVATE _CRT_SECURE_NO_WARNINGS )
	target_compile_options( app PRIVATE 
		-WX -W4 
		-wd4100 -wd4201 -wd4324 -wd4189 
		-Zc:preprocessor -Zc:strictStrings 
		-GR- -EHsc
		$<$<CONFIG:Debug>:-Z7 -FC>
		$<$<CONFIG:Release>:-O2 -Ot -GF>
	)
else()
	target_compile_options( app PRIVATE 
		-Wall -Wextra -Wpedantic -Werror 
		-Wno-uninitialized -Wno-non-virtual-dtor 
		-fno-rtti
		$<$<CONFIG:Debug>:-O0 -g>
		$<$<CONFIG:Release>:-O2>
	)
endif()