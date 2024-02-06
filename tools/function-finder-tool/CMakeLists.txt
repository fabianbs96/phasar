# Build a stand-alone executable
if(PHASAR_IN_TREE)
  add_phasar_executable(functionfindertool
    functionfindertool.cpp
  )
else()
  add_executable(functionfindertool
    functionfindertool.cpp
  )
endif()

target_link_libraries(functionfindertool
  PRIVATE
    phasar
    ${PHASAR_STD_FILESYSTEM}
)

install(TARGETS functionfindertool
  RUNTIME DESTINATION bin
)
