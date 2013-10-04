cmake_minimum_required (VERSION 2.6)
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

if (${UNIX})
  add_definitions(-fvisibility-inlines-hidden)
  add_definitions(-fvisibility=hidden)
  add_definitions(-std=c++11)
endif(${UNIX})

SET(CMAKE_SHARED_LIBRARY_PREFIX "")

set (sources FeedbackAM.cpp
  NovaDiskIO.cpp
  NovaFB.cpp
  NovaFilters.cpp
  NovaGendy.cpp
  NovaOsc.cpp)

foreach(src ${sources})
  string(REPLACE .cpp "" libname ${src} )

  add_library(${libname} SHARED ${src})

  set (snlib ${libname}_supernova)

  add_library(${snlib} SHARED ${src})

  set_property(TARGET ${snlib}
    APPEND PROPERTY COMPILE_DEFINITIONS SUPERNOVA)

  set(libs ${libname} ${snlib} ${libs})
endforeach()

install(TARGETS ${libs}
        DESTINATION "lib/SuperCollider/plugins/")

install(FILES sc/NovaUGens.sc
        DESTINATION "share/SuperCollider/Extensions/Nova/")
