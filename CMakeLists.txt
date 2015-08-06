cmake_minimum_required (VERSION 3.0)
project (nova-ugens)

enable_testing()

include( sc-plugin-builder.cmake )

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

  add_scplugin( ${libname} ${src} TLSF-2.4.6/src/tlsf.c )

  target_compile_definitions( ${libname}           PUBLIC BOOST_SYNC_USE_STD_SYSTEM_ERROR )
  target_compile_definitions( ${libname}_supernova PUBLIC BOOST_SYNC_USE_STD_SYSTEM_ERROR )

  target_include_directories( ${libname}           PUBLIC boost_sync/include ${SC_PATH}/external_libraries/boost)
  target_include_directories( ${libname}_supernova PUBLIC boost_sync/include ${SC_PATH}/external_libraries/boost)

endforeach()

target_link_libraries( NovaDiskIO           sndfile )
target_link_libraries( NovaDiskIO_supernova sndfile )
