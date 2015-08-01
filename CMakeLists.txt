cmake_minimum_required (VERSION 3.0)
project (nova-ugens)

enable_testing()

GET_FILENAME_COMPONENT(SOURCEPARENT "${CMAKE_CURRENT_SOURCE_DIR}" PATH)
find_path(SC_PATH NAMES include/plugin_interface/SC_PlugIn.h
	PATHS "${SOURCEPARENT}"
	PATH_SUFFIXES SuperCollider)

set(SC_FOUND FALSE)
if(IS_DIRECTORY ${SC_PATH})
	set(SC_FOUND TRUE)
endif()

include_directories(${SC_PATH}/include/plugin_interface)
include_directories(${SC_PATH}/include/common)

include_directories(${SC_PATH}/external_libraries/nova-tt)

if( CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES Clang)
  set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -fvisibility-inlines-hidden")
  set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y")
  set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native") # boom
endif()

if( CMAKE_CXX_COMPILER_ID MATCHES Clang)
  set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()


SET(CMAKE_SHARED_LIBRARY_PREFIX "")

set (sources
  ESInterleaver.cpp
  FeedbackAM.cpp
  NovaDiskIO.cpp
  NovaFB.cpp
  NovaFilters.cpp
  NovaGendy.cpp
  NovaOsc.cpp)

foreach(src ${sources})
  string(REPLACE .cpp "" libname ${src} )

  add_library(${libname} SHARED ${src} TLSF-2.4.6/src/tlsf.c)

  set (snlib ${libname}_supernova)

  add_library(${snlib} SHARED ${src} TLSF-2.4.6/src/tlsf.c)

  set_property(TARGET ${snlib}
    APPEND PROPERTY COMPILE_DEFINITIONS SUPERNOVA)

  set(libs ${libname} ${snlib} ${libs})
endforeach()

target_link_libraries( NovaDiskIO sndfile)
target_link_libraries( NovaDiskIO_supernova sndfile)

target_include_directories( NovaDiskIO           PUBLIC boost_sync/include ${SC_PATH}/external_libraries/boost)
target_include_directories( NovaDiskIO_supernova PUBLIC boost_sync/include ${SC_PATH}/external_libraries/boost)

target_compile_definitions( NovaDiskIO           PUBLIC BOOST_SYNC_USE_STD_SYSTEM_ERROR)
target_compile_definitions( NovaDiskIO_supernova PUBLIC BOOST_SYNC_USE_STD_SYSTEM_ERROR)

install(TARGETS ${libs}
        DESTINATION "lib/SuperCollider/plugins/")

install(FILES sc/NovaUGens.sc sc/NovaDiskIOUGens.sc
        DESTINATION "share/SuperCollider/Extensions/Nova/")
