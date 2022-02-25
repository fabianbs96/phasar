# Build a stand-alone executable
if(PHASAR_IN_TREE)
  add_phasar_executable(phasar-clang
    phasar-clang.cpp
  )
else()
  add_executable(phasar-clang
    phasar-clang.cpp
  )
endif()

find_package(Boost COMPONENTS log filesystem program_options graph ${BOOST_THREAD} REQUIRED)
list(TRANSFORM LLVM_LINK_COMPONENTS PREPEND LLVM)
target_link_libraries(phasar-clang PUBLIC ${LLVM_LINK_COMPONENTS})
target_link_libraries(phasar-clang
  LINK_PUBLIC
  phasar_config
  phasar_controller
  phasar_controlflow
  phasar_phasarllvm_utils
  phasar_analysis_strategy
  phasar_ifdside
  phasar_utils
  phasar_mono
  phasar_db
  phasar_experimental
  # phasar_clang
  phasar_passes
  # FIXME: cmake variable ${PHASAR_PLUGINS_LIB} is empty although it should contain phasar_plugins
  phasar_plugins
  # ${PHASAR_PLUGINS_LIB}
  phasar_pointer
  phasar_typehierarchy
  ${SQLite3_LIBRARIES}
  ${Boost_LIBRARIES}
  ${CMAKE_DL_LIBS}
  ${CMAKE_THREAD_LIBS_INIT}
  # ${CLANG_LIBRARY}
  LINK_PRIVATE
  ${PHASAR_STD_FILESYSTEM}
)

set(LLVM_LINK_COMPONENTS
)
