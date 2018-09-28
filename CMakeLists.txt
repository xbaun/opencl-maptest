cmake_minimum_required(VERSION 3.8)

project(MapTest)

find_package(OpenCL REQUIRED)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_VERBOSE_MAKEFILE on )
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

add_executable(MapTest main.cpp)

target_link_libraries(MapTest OpenCL::OpenCL)

configure_file("Kernel.cl" "Kernel.cl" COPYONLY)