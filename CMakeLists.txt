cmake_minimum_required(VERSION 3.17)
project(cpp_introspection)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

include_directories(include)

add_subdirectory(examples/simple)
add_subdirectory(examples/http)
add_subdirectory(examples/socket)