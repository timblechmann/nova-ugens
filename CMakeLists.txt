cmake_minimum_required (VERSION 3.0)
project (nova-ugens)

enable_testing()

include( sc-plugin-builder.cmake )

set(sources
  ESInterleaver.cpp
  NovaDiskIO.cpp
  NovaFB.cpp
  NovaGendy.cpp
  NovaOsc.cpp
  NovaPeakMeter.cpp
  NovaRMFilters.cpp
  NovaTanhShaper.cpp

# obsolete:
# FeedbackAM.cpp
# NovaFilters.cpp

)

add_library( tlsf TLSF-2.4.6/src/tlsf.c )
set_target_properties( tlsf PROPERTIES POSITION_INDEPENDENT_CODE ON )

foreach(src ${sources})
  string(REPLACE .cpp "" libname ${src} )

  add_scplugin( ${libname} ${src} )

  target_compile_definitions( ${libname}           PUBLIC BOOST_SYNC_USE_STD_SYSTEM_ERROR )
  target_compile_definitions( ${libname}_supernova PUBLIC BOOST_SYNC_USE_STD_SYSTEM_ERROR )

  target_include_directories( ${libname}           PUBLIC boost_sync/include ${SC_PATH}/external_libraries/boost)
  target_include_directories( ${libname}_supernova PUBLIC boost_sync/include ${SC_PATH}/external_libraries/boost)

endforeach()

target_link_libraries( NovaDiskIO           sndfile tlsf )
target_link_libraries( NovaDiskIO_supernova sndfile tlsf )


target_include_directories( NovaRMFilters           PUBLIC nova-dsp )
target_include_directories( NovaRMFilters_supernova PUBLIC nova-dsp )


target_include_directories( NovaTanhShaper           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} )
target_include_directories( NovaTanhShaper_supernova PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} )


add_scclass( NovaUGens sc/NovaDiskIOUGens.sc )
add_scclass( NovaUGens sc/NovaUGens.sc       )
add_scclass( NovaUGens sc/NovaRMFilters.sc   )
add_scclass( NovaUGens sc/NovaTanhShaper.sc  )
add_scclass( NovaUGens sc/NovaPeakMeter.sc  )
