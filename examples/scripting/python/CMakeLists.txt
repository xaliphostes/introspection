project(pyintro)

include(FetchContent)

FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG main
)
FetchContent_MakeAvailable(pybind11)
add_subdirectory(pybind11)

## Find Python and pybind11
#find_package(Python COMPONENTS Interpreter Development REQUIRED)
#find_package(pybind11 REQUIRED)

# Create the Python module
pybind11_add_module(pyintro binding.cxx)
