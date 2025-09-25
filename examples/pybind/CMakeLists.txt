project(simple)

# Find Python and pybind11
find_package(Python COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 REQUIRED)

# Create the Python module
pybind11_add_module(introspection_demo automatic_binding_example.cpp)

add_executable(${PROJECT_NAME} main.cxx)