# Build a stand-alone executable
if(PHASAR_IN_TREE)
  # Build a small test tool to show how phasar may be used
  add_phasar_executable(myphasartool
    myphasartool.cpp
  )
else()
  # Build a small test tool to show how phasar may be used
  add_executable(myphasartool
    myphasartool.cpp
  )
endif()

find_package(Boost COMPONENTS log filesystem program_options graph ${BOOST_THREAD} REQUIRED)
target_link_libraries(myphasartool
  LINK_PUBLIC
  phasar_config
  phasar_controller
  phasar_db
  phasar_experimental
  phasar_controlflow
  phasar_ifdside
  phasar_mono
  phasar_passes
  ${PHASAR_PLUGINS_LIB}
  phasar_pointer
  phasar_typehierarchy
  phasar_phasarllvm_utils
  phasar_utils
  ${Boost_LIBRARIES}
  ${CMAKE_DL_LIBS}
  ${CMAKE_THREAD_LIBS_INIT}
  LINK_PRIVATE
  ${PHASAR_STD_FILESYSTEM}
)

list(TRANSFORM LLVM_LINK_COMPONENTS PREPEND LLVM)
target_link_libraries(myphasartool PUBLIC ${LLVM_LINK_COMPONENTS})

set(LLVM_LINK_COMPONENTS
)

install(TARGETS myphasartool
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
