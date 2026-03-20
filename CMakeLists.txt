cmake_minimum_required( VERSION 3.18 FATAL_ERROR )

# use folders for visual studio
set_property( GLOBAL PROPERTY USE_FOLDERS ON )

set( CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo" )

set( CMAKE_POSITION_INDEPENDENT_CODE ON )
set( CMAKE_SHARED_LIBRARY_PREFIX "" )

option( MIMALLOC "Use mimalloc" OFF )

project( media_tag_system )

set( PROJ_OBJ "obj" )
set( PROJ_OUT "${CMAKE_CURRENT_LIST_DIR}/out" )

set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJ_OUT} )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJ_OUT} )
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJ_OUT} )

foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${PROJ_OUT} )
    set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${PROJ_OUT} )
    set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${PROJ_OUT} )
endforeach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )

set_property( GLOBAL PROPERTY PREFIX "" )
set_property( GLOBAL PROPERTY USE_FOLDERS ON )

set( CMAKE_CXX_STANDARD 23 )
set( CMAKE_EXPORT_COMPILE_COMMANDS ON )

# Compiler/Platform specifc options
if ( MSVC )
	# Sets the subsystem to Windows
	# set( CMAKE_WIN32_EXECUTABLE ON )

	# sets main function to int main() instead of WinMain()
	# set( CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup )

	add_compile_definitions(
		_AMD64_ __x86_64 __amd64
		NOMINMAX
		_CRT_SECURE_NO_WARNINGS
		_ALLOW_RUNTIME_LIBRARY_MISMATCH _ALLOW_ITERATOR_DEBUG_LEVEL_MISMATCH _ALLOW_MSC_VER_MISMATCH
		_UNICODE=1
		UNICODE=1
	)
	
	# Use these runtime libraries for both so it doesn't crash smh my head
	# actually this is useless right now because of core.dll, god dammit
	# set_property( GLOBAL PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" )
	
	# set( CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" )
	# set_property( GLOBAL PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" )

	# set( MSVC_VERSION 1939 )

	add_compile_options(
		# "/Wall"             # Show ALL Warnings
		"/W4"               # Warning Level 4
		"/MP"               # multi-threaded compilation
		"/permissive-"      # make MSVC very compliant
		"/Zc:__cplusplus"   # set the __cplusplus macro to be the correct version
		# "/fp:fast"          # fast floating point model
		"/GF"               # string pooling: allows compiler to create a single copy of identical strings in the program image and in memory during execution
		
		# disable stupid warnings
		"/wd4100"  # 'X' unreferenced formal paramter

	#	"/wd4244"  # conversion from 'X' to 'Y', possible loss of data
	#	"/wd4267"  # conversion from 'X' to 'Y', possible loss of data (yes there's 2 idk why)
	#	"/wd4305"  # truncation from 'double' to 'float'
	#	"/wd4464"  # relative include path contains ".."
	#	"/wd4668"  # 'X' is not defined as a preprocessor macro, replacing '0' for '#if/#elif' (floods with win32 stuff)
	#	"/wd4514"  # 'X' unreferenced inline function has been removed
	#	"/wd4505"  # 'X' unreferenced function with internal linkage has been removed
	#	"/wd4201"  # nonstandard extension used: nameless struct/union (used in glm)
	#	"/wd5045"  # Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified
    #
	#	"/wd4706"  # assignment within conditional expression
	#	
	#	# probably temporary
	#	"/wd4365"  # signed/unsigned mismatch
	#	"/wd4061"  # enumerator 'X' in switch of enum 'Y' is not explicitly handled by a case label
	#	
	#	# padding stuff
	#	"/wd4324"  # 'X' structure was padded due to alignment specifier
	#	"/wd4820"  # 'X' bytes padding added after data member 'Y'
    #
	#	# Treat these warnings as errors
    #
	#	# MSVC doesn't care about this one, but this errors on gcc and can help catch a mistake i made unknowingly
	#	"/we4002"  # Too many arguments for function-like macro invocation
	
		# I accidently get this a lot with printing std::string, so I want this to error so I know i made a mistake
		"/we4840"  # non-portable use of class 'type' as an argument to a variadic function
		
	#	# This is self-explanitory
	#	"/we4129"  # 'X' unrecognized character escape sequence
	)

	set( COMPILE_OPTIONS_DEBUG
		"/Od"        # no optimizations
		# "/ZI"        # edit and continue
		"/fp:except" # raise floating point exceptions
		# "/fsanitize=address" # use address sanitizing in Debug (incompatible with /INCREMENTAL and /ZI)
	)
	
	set( COMPILE_OPTIONS_RELEASE
		"/fp:fast"
	)
	
	add_compile_options(
		"$<$<CONFIG:Debug>:${COMPILE_OPTIONS_DEBUG}>"
		"$<$<CONFIG:Release>:${COMPILE_OPTIONS_RELEASE}>"
		"$<$<CONFIG:RelWithDebInfo>:${COMPILE_OPTIONS_RELEASE}>"
	)
	
	set( LINK_OPTIONS_DEBUG
		""
	)
	
	set( LINK_OPTIONS_RELEASE
		""
	)
	
	add_link_options(
		"$<$<CONFIG:Debug>:${LINK_OPTIONS_DEBUG}>"
		"$<$<CONFIG:Release>:${LINK_OPTIONS_RELEASE}>"
		"$<$<CONFIG:RelWithDebInfo>:${LINK_OPTIONS_RELEASE}>"
	)
	
	add_link_options(
		"/SUBSYSTEM:WINDOWS"
		"/ENTRY:mainCRTStartup"
		# "/INCREMENTAL"
	)

	link_libraries( opengl32 )
	
	message( "Copying SDL3" )
	configure_file( "thirdparty/SDL3/lib/x64/SDL3.dll" "${PROJ_OUT}/SDL3.dll" COPYONLY )
	
	message( "Copying MPV" )
	configure_file( "thirdparty/mpv/libmpv-2.dll" "${PROJ_OUT}/libmpv-2.dll" COPYONLY )
	
	message( "Copying SPNG" )
	configure_file( "thirdparty/libspng/build/Release/spng.dll" "${PROJ_OUT}/spng.dll" COPYONLY )
	
	message( "Copying FreeImage" )
	configure_file( "thirdparty/FreeImageRe/bin/FreeImage.dll" "${PROJ_OUT}/FreeImage.dll" COPYONLY )
	configure_file( "thirdparty/FreeImageRe/bin/heif.dll" "${PROJ_OUT}/heif.dll" COPYONLY )
	
	message( "Copying fyaml" )
	configure_file( "thirdparty/libfyaml/build/Release/fyaml.dll" "${PROJ_OUT}/fyaml.dll" COPYONLY )
	
	message( "Copying jxl" )
	configure_file( "thirdparty/libjxl/build/lib/Release/jxl.dll" "${PROJ_OUT}/jxl.dll" COPYONLY )
	configure_file( "thirdparty/libjxl/build/lib/Release/jxl_cms.dll" "${PROJ_OUT}/jxl_cms.dll" COPYONLY )
	configure_file( "thirdparty/libjxl/build/lib/Release/jxl_threads.dll" "${PROJ_OUT}/jxl_threads.dll" COPYONLY )
	
	configure_file( "thirdparty/libjxl/build/third_party/brotli/Release/brotlicommon.dll" "${PROJ_OUT}/brotlicommon.dll" COPYONLY )
	configure_file( "thirdparty/libjxl/build/third_party/brotli/Release/brotlidec.dll" "${PROJ_OUT}/brotlidec.dll" COPYONLY )
	configure_file( "thirdparty/libjxl/build/third_party/brotli/Release/brotlienc.dll" "${PROJ_OUT}/brotlienc.dll" COPYONLY )

	#message( "Copying mimalloc" )
	#configure_file( "thirdparty/mimalloc/out/msvc-x64/Release/mimalloc-redirect.dll" "${PROJ_OUT}/mimalloc-redirect.dll" COPYONLY )
	#configure_file( "thirdparty/mimalloc/out/msvc-x64/Release/mimalloc.dll" "${PROJ_OUT}/mimalloc.dll" COPYONLY )
	
else()  # linux
	link_libraries( GL )
	
	add_compile_definitions(
		_FILE_OFFSET_BITS=64
		_TIME_BITS=64
	)

	message( "Copying SPNG" )
	configure_file( "thirdparty/libspng/build/libspng.so" "${PROJ_OUT}/libspng.so" COPYONLY )

	message( "Copying FreeImage" )
	configure_file( "thirdparty/FreeImageRe/lib/libFreeImage.so" "${PROJ_OUT}/libFreeImage.so" COPYONLY )
	configure_file( "thirdparty/FreeImageRe/lib/libheif.so" "${PROJ_OUT}/libheif.so" COPYONLY )

	message( "Copying fyaml" )
	configure_file( "thirdparty/libfyaml/build/libfyaml.so" "${PROJ_OUT}/libfyaml.so" COPYONLY )

	message( "Copying jxl" )
	configure_file( "thirdparty/libjxl/build/lib/libjxl.so" "${PROJ_OUT}/libjxl.so" COPYONLY )
	configure_file( "thirdparty/libjxl/build/lib/libjxl_cms.so" "${PROJ_OUT}/libjxl_cms.so" COPYONLY )
	configure_file( "thirdparty/libjxl/build/lib/libjxl_threads.so" "${PROJ_OUT}/libjxl_threads.so" COPYONLY )

	configure_file( "thirdparty/libjxl/build/third_party/brotli/libbrotlicommon.so" "${PROJ_OUT}/libbrotlicommon.so" COPYONLY )
	configure_file( "thirdparty/libjxl/build/third_party/brotli/libbrotlidec.so" "${PROJ_OUT}/libbrotlidec.so" COPYONLY )
	configure_file( "thirdparty/libjxl/build/third_party/brotli/libbrotlienc.so" "${PROJ_OUT}/libbrotlienc.so" COPYONLY )
endif()


# ========================================================================================================

set(
    SRC_FILES
    src/main.cpp
    src/main.h
    src/thumbnail_loader.cpp
    src/thumbnail_jxl.cpp
    src/mpv_interface.cpp
    src/mpv_interface.h
    src/image_loader.cpp
    src/memory.cpp
    src/config.cpp

    src/view_gallery.cpp
    src/view_gallery_sidebar.cpp

    src/view_media.cpp
	
    src/util.h
    src/util.cpp
    src/util_filesystem.cpp

    src/args.cpp
    src/args.h
    src/handles.h

    src/codecs/loader_jpeg.cpp
    src/codecs/loader_png.cpp
    # src/codecs/format_gif.cpp
    src/codecs/loader_freeimage.cpp
    src/codecs/loader_jpegxl.cpp

    src/system/system.h

    src/imgui/imconfig.h
    src/imgui/imgui.cpp
    src/imgui/imgui_demo.cpp
    src/imgui/imgui_draw.cpp
    src/imgui/imgui_impl_opengl3.cpp
    src/imgui/imgui_impl_sdl3.cpp
    src/imgui/imgui_tables.cpp
    src/imgui/imgui_widgets.cpp

	src/glad/glad.c
	src/glad/glad.h
	src/glad/khrplatform.h
)


if ( MSVC )
	set(
		SYSTEM_FILES
		src/system/win32/sys_win32.h
		src/system/win32/sys_win32.cpp
		src/system/win32/sys_win32_internal.cpp
		src/system/win32/sys_win32_dragdrop.cpp
	)
else()
	set(
		SYSTEM_FILES
		src/system/linux/sys_linux.cpp
	)
endif()

set( FREETYPE_DIR "thirdparty/freetype" )

set(
	FREETYPE_IMGUI_SRC
	src/imgui/imgui_freetype.cpp
	src/imgui/imgui_freetype.h
)

include_directories( "${FREETYPE_DIR}/include" )
add_compile_definitions( IMGUI_ENABLE_FREETYPE=1 )

file(
	GLOB_RECURSE THIRDPARTY_FILES CONFIGURE_DEPENDS
	thirdparty/*.h
	thirdparty/*.hpp
)

include_directories(
	"src/"
	"src/imgui"
	"src/glad"
	"thirdparty"
	"thirdparty/nativefiledialog/src/include"
	"thirdparty/SDL3/include"
	"thirdparty/mozjpeg"
	"thirdparty/libspng/spng"
	"thirdparty/mpv/include"
	"thirdparty/gif_load"
	"thirdparty/FreeImageRe/include"
	"thirdparty/stb"
	"thirdparty/libfyaml/include"
	"thirdparty/libjxl/lib/include"
	"thirdparty/libjxl/build/lib/include"
)

if( LINUX )
	include_directories(
		"/usr/include/SDL3"
	)
endif()

if ( MIMALLOC )
	add_compile_definitions( USE_MIMALLOC=1 )
	include_directories( "thirdparty/mimalloc/include" )
else()
	add_compile_definitions( USE_MIMALLOC=0 )
endif()

link_libraries(
	freetype
	nfd
	SDL3
	spng
	fyaml
	jxl
	jxl_cms
	jxl_threads

	# must be below jxl, or my program will link the jxl functions to freeimage instead
	FreeImage
)

if ( MIMALLOC )
	link_libraries( mimalloc )
	link_directories( "thirdparty/mimalloc/out/msvc-x64/Release" )
endif()

link_directories(
	"thirdparty/nativefiledialog/build/src"
	"thirdparty/mozjpeg/build"
	"thirdparty/libspng/build"
	"thirdparty/FreeImageRe/lib"
	"thirdparty/libfyaml/build"
)

if ( WIN32 )
	link_directories(
		"thirdparty/libspng/build/Release"
		"thirdparty/libfyaml/build/Release"
		"thirdparty/libjxl/build/lib/Release"
		"thirdparty/SDL3/lib/x64"
	)
	link_libraries(
		turbojpeg-static
	)
elseif( LINUX )
	link_directories(
	)
	link_libraries(
		turbojpeg
		stdc++exp
	)
endif()

foreach( OUTPUT_CONFIG ${CMAKE_CONFIGURATION_TYPES} )
	link_directories(
		"${FREETYPE_DIR}/objs/x64/${OUTPUT_CONFIG} Static"
		"thirdparty/nativefiledialog/build/src/${OUTPUT_CONFIG}"
		"thirdparty/mozjpeg/build/${OUTPUT_CONFIG}"
		# "thirdparty/libfyaml/build/${OUTPUT_CONFIG}"
		# "thirdparty/libspng/build/${OUTPUT_CONFIG}"
	)
endforeach( OUTPUT_CONFIG ${CMAKE_CONFIGURATION_TYPES} )

add_executable( media_tag_system ${SRC_FILES} ${SYSTEM_FILES} ${FREETYPE_IMGUI_SRC} ${THIRDPARTY_FILES} )

if ( WIN32 )
	add_executable( media_tag_system_con ${SRC_FILES} ${SYSTEM_FILES} ${FREETYPE_IMGUI_SRC} ${THIRDPARTY_FILES} )

	target_link_options(
		media_tag_system_con PRIVATE
		"/SUBSYSTEM:CONSOLE"
	)

	set_target_properties(
		media_tag_system_con PROPERTIES
		SUFFIX ".com"
		RUNTIME_OUTPUT_NAME "media_tag_system"
		PDB_NAME "media_tag_system_con"
	)
endif()

source_group(
	TREE ${CMAKE_CURRENT_LIST_DIR}/src
	PREFIX "Source Files"
	FILES ${SRC_FILES} ${SYSTEM_FILES} ${FREETYPE_IMGUI_SRC}
)

source_group(
	TREE ${CMAKE_CURRENT_LIST_DIR}/thirdparty
	PREFIX "Thirdparty Files"
	FILES ${THIRDPARTY_FILES}
)

