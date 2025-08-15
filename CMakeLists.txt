cmake_minimum_required(VERSION 3.0...3.10)
project(oggvorbis2fsb5)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include_directories(
	include
	external/ogg/include
	external/vorbis/include
)

add_executable(oggvorbis2fsb5
	oggvorbis2fsb5.c
	external/ogg/src/bitwise.c
	external/ogg/src/framing.c
	external/vorbis/lib/bitrate.c
	external/vorbis/lib/block.c
	external/vorbis/lib/codebook.c
	external/vorbis/lib/envelope.c
	external/vorbis/lib/floor0.c
	external/vorbis/lib/floor1.c
	external/vorbis/lib/info.c
	external/vorbis/lib/lpc.c
	external/vorbis/lib/lsp.c
	external/vorbis/lib/mapping0.c
	external/vorbis/lib/mdct.c
	external/vorbis/lib/psy.c
	external/vorbis/lib/registry.c
	external/vorbis/lib/res0.c
	external/vorbis/lib/sharedbook.c
	external/vorbis/lib/smallft.c
	external/vorbis/lib/synthesis.c
	external/vorbis/lib/vorbisfile.c
	external/vorbis/lib/window.c
)

set_target_properties(oggvorbis2fsb5 PROPERTIES C_STANDARD 11)

if(WIN32)
	target_compile_definitions(oggvorbis2fsb5 PUBLIC UNICODE _UNICODE)
	target_link_options(oggvorbis2fsb5 PUBLIC -static -static-libgcc -municode)
endif()
if((${CMAKE_SYSTEM_NAME} MATCHES "Linux") OR (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD"))
	target_link_libraries(oggvorbis2fsb5 m)
endif()
