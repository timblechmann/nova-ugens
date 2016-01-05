###################
# ScPluginInterface


get_filename_component(SOURCEPARENT "${CMAKE_CURRENT_SOURCE_DIR}" PATH)
find_path(SC_PATH NAMES include/plugin_interface/SC_PlugIn.h
	PATHS "${SOURCEPARENT}"
	PATH_SUFFIXES SuperCollider supercollider)

set(SC_FOUND FALSE)
if(IS_DIRECTORY ${SC_PATH})
	set(SC_FOUND TRUE)
endif()

add_library(ScPluginInterface INTERFACE)

target_include_directories(ScPluginInterface INTERFACE
  ${SC_PATH}/include/plugin_interface
  ${SC_PATH}/include/common
  ${SC_PATH}/external_libraries/nova-tt
  ${SC_PATH}/external_libraries/boost)

target_compile_options(ScPluginInterface  INTERFACE  -ffast-math -fno-finite-math-only -ftemplate-backtrace-limit=0)
target_compile_definitions(ScPluginInterface INTERFACE BOOST_NO_AUTO_PTR)


if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  if(APPLE)
    target_compile_options(ScPluginInterface INTERFACE -Wa,-q) # workaround for homebrew's gcc
  endif()
endif()


if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|AppleClang|Clang")

  option( NATIVE "native build" ON )

  if( NATIVE )
    target_compile_options( ScPluginInterface INTERFACE -march=native )
  endif()
endif()


##############
# add_scplugin

function(add_scplugin Name)
  add_library(${Name} MODULE ${ARGN})
  add_library(${Name}_supernova MODULE ${ARGN})
  target_compile_definitions(${Name}_supernova PRIVATE SUPERNOVA)

  target_link_libraries(${Name}           ScPluginInterface)
  target_link_libraries(${Name}_supernova ScPluginInterface)

  set_target_properties(${Name} ${Name}_supernova PROPERTIES
    PREFIX ""
    CXX_STANDARD 14
    VISIBILITY_INLINES_HIDDEN ON)

  if(CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang")
    set_target_properties(${Name} ${Name}_supernova PROPERTIES
      LINKER_FLAGS -stdlib=libc++ )
  endif()

  if(APPLE)
    set_target_properties(${Name} ${Name}_supernova PROPERTIES SUFFIX ".scx")
  endif()

  if(APPLE)
    install(TARGETS ${Name} ${Name}_supernova
            DESTINATION "~/Library/Application Support/SuperCollider/Extensions/plugins/")
  else()
    install(TARGETS ${Name} ${Name}_supernova
            DESTINATION "lib/SuperCollider/plugins/")
  endif()
endfunction()



#############
# add_scclass

function(add_scclass Prefix)
  if(APPLE)
    install(FILES ${ARGN}
            DESTINATION "~/Library/Application Support/SuperCollider/Extensions/${Prefix}/")
  else()
    install(FILES ${ARGN}
            DESTINATION "share/SuperCollider/Extensions/${Prefix}/")
  endif()
endfunction()


##########
# ccache

find_program( CCacheExecutable ccache )
if( CCacheExecutable )
  set( CMAKE_C_COMPILER_LAUNCHER   ${CCacheExecutable} )
  set( CMAKE_CXX_COMPILER_LAUNCHER ${CCacheExecutable} )
endif()
