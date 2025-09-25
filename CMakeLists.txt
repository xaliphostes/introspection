cmake_minimum_required(VERSION 3.17)
project(cpp_introspection)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

include_directories(include)

# Add the executable
add_executable(introspection_demo main.cxx)

# Optional: Set output directory
set_target_properties(introspection_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../bin
)
