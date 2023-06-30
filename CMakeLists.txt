cmake_minimum_required(VERSION 2.6)
project(spotiamb)

set(SPOTIAMB_SRCS
	com_ptr.h
	commands.h
	dialogs.h
	dialogs_win32.cpp
	resource.h
	spotifyamp.cpp
	spotifyamp.h
	spotifyamp.rc
	stdafx.h
	targetver.h
	window.cpp
	window.h
	window_sdl.cpp
	window_sdl.h
	window_win32.cpp
	window_win32.h
	vis.cpp
	visplugin.cpp
	visplugin.h
	zipfile.cpp
	zipfile.h
	audio_win32.c
	audio_eq.c
	audio_eq.h
	fpu_util.h
	tinfl.c
	tinfl.h
	shoutcast.h
	mp3.h
	build_config.h
	config.h
	types.h
	util.h
	appkey.h
	tiny_spotify/tiny_spotify.h
	tiny_spotify/tiny_spotify.obj)
  
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "/D_CRT_NONSTDC_NO_WARNINGS")
set(CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE} "/O1 /Ob2 /Oi /Os /Oy /GF /Gy /MT /fp:fast /QIfist")

STRING(REGEX REPLACE "/EHsc" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS}) # Disable C++ exceptions
STRING(REGEX REPLACE "/GR" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS}) # Disable RTTI

include_directories(tiny_spotify)
add_executable(spotiamb ${SPOTIAMB_SRCS})
add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_UNICODE -DUNICODE)

set_property(TARGET spotiamb APPEND PROPERTY LINK_FLAGS "/NXCOMPAT:NO  /SAFESEH:NO")