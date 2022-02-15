file(GLOB_RECURSE PLUGINS_SRC *.h *.cpp)
#file(GLOB_RECURSE PLUGINS_SO *.cxx)

set(PHASAR_LINK_LIBS
  phasar_ifdside
  phasar_mono
  phasar_phasarllvm_utils
  phasar_utils
)

set(LLVM_LINK_COMPONENTS
  Core
  Support
)

# Handle the library files
if(BUILD_SHARED_LIBS)
  add_phasar_library(phasar_plugins
    SHARED
    ${PLUGINS_SRC}
  )
else()
  add_phasar_library(phasar_plugins
    STATIC
    ${PLUGINS_SRC}
  )
endif()


find_package(Boost COMPONENTS log filesystem program_options REQUIRED)
target_link_libraries(phasar_plugins
  LINK_PUBLIC
  ${Boost_LIBRARIES}
  ${CMAKE_DL_LIBS}
)

set_target_properties(phasar_plugins
  PROPERTIES
  LINKER_LANGUAGE CXX
  PREFIX "lib"
)


# Handle all plugins
list(TRANSFORM LLVM_LINK_COMPONENTS PREPEND LLVM)
foreach(plugin ${PLUGINS_SO})
  get_filename_component(plugin_name ${plugin} NAME_WE)
  add_library(${plugin_name} SHARED ${plugin})
  target_link_libraries(${plugin_name} PUBLIC ${LLVM_LINK_COMPONENTS})
  set_target_properties(${plugin_name} PROPERTIES PREFIX "")
  target_link_libraries(${plugin_name}
    LINK_PRIVATE
    phasar_plugins
    phasar_config
    phasar_utils
    phasar_ifdside
    phasar_mono
    )
endforeach()

