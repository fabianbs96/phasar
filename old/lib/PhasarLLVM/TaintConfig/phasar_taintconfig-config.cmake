set(PHASAR_taintconfig_COMPONENT_FOUND 1)

list(APPEND
  PHASAR_TAINTCONFIG_DEPS
  utils
  db
  phasarllvm_utils
)

foreach(dep ${PHASAR_TAINTCONFIG_DEPS})
  list(APPEND
    PHASAR_NEEDED_LIBS
    phasar::phasar_${dep}
  )
  if(NOT (${PHASAR_${dep}_COMPONENT_FOUND}))
    find_package(phasar COMPONENTS ${dep})
  endif()
endforeach()

find_package(nlohmann_json_schema_validator REQUIRED)

list(APPEND
  PHASAR_NEEDED_LIBS
  phasar::phasar_taintconfig
  nlohmann_json_schema_validator
)
