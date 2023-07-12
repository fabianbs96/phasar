cmake_minimum_required(VERSION 3.1)
project(phasar-conan-testpackage CXX)

find_package(phasar REQUIRED COMPONENTS phasar-all)

add_executable(myphasartool myphasartool.cpp)
target_link_libraries(myphasartool PUBLIC phasar-all)
