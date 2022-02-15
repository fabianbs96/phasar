if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
  message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
  file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/v0.16.1/conan.cmake"
                "${CMAKE_BINARY_DIR}/conan.cmake"
                EXPECTED_HASH SHA256=396e16d0f5eabdc6a14afddbcfff62a54a7ee75c6da23f32f7a31bc85db23484
                TLS_VERIFY ON)
endif()
include(${CMAKE_BINARY_DIR}/conan.cmake)
conan_cmake_run(
    BASIC_SETUP
    CONANFILE "${CMAKE_SOURCE_DIR}/conanfile.txt" 
    BUILD missing)

set(CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR};${CMAKE_MODULE_PATH}"  )

# Threads
find_package(Threads REQUIRED)

# Boost
find_package(Boost 1.72.0 COMPONENTS filesystem graph system program_options log ${BOOST_THREAD} REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

# JSON library
option(JSON_BuildTests OFF)

find_package(nlohmann_json REQUIRED)
include_directories(${nlohmann_json_INCLUDE_DIR})

# JSON schema validator library
find_package(nlohmann_json_schema_validator REQUIRED)
include_directories(${nlohmann_json_schema_validator_INCLUDE_DIR})


# Googletest
find_package(GTest REQUIRED)
include_directories(${GTest_INCLUDE_DIR})

# SQL
find_package(SQLite3 REQUIRED)
include_directories(${SQLite3_INCLUDE_DIRS})

# Curl
find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIRS})
link_libraries(curl)

# LLVM
find_package(llvm-core 12 REQUIRED)
include_directories(${llvm-core_INCLUDE_DIRS})
link_directories(${llvm-core_LIBRARY_DIRS})

# set(LLVM_LINK_COMPONENTS
#   coverage
#   coroutines
#   demangle
#   libdriver
#   lto
#   support
#   analysis
#   bitwriter
#   core
#   ipo
#   irreader
#   instcombine
#   instrumentation
#   linker
#   objcarcopts
#   scalaropts
#   transformutils
#   codegen
#   vectorize
# )