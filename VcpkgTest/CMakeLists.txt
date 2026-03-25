cmake_minimum_required(VERSION 3.31)
set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic" CACHE STRING "vcpkg triplet")
project(VcpkgTest)

set(CMAKE_CXX_STANDARD 20)

add_executable(VcpkgTest main.cpp)

find_package(glog CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE glog::glog)