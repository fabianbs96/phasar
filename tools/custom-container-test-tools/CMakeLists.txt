
# Build a stand-alone executable
if(PHASAR_IN_TREE)
  # Build a small test tool to show how phasar may be used
  add_phasar_executable(custom-container-test-tool
    custom-container-test-tool.cpp
  )
else()
  # Build a small test tool to show how phasar may be used
  add_executable(custom-container-test-tool
    custom-container-test-tool.cpp
  )
endif()

target_link_libraries(custom-container-test-tool
  PRIVATE
    phasar
    ${PHASAR_STD_FILESYSTEM}
)
