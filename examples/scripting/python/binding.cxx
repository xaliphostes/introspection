#include "PythonBindingGenerator.h"
#include "demo.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

// pybind11 module definition using automatic binding
PYBIND11_MODULE(introspection_demo, m) {
    m.doc() = "Automatic Python bindings using C++ introspection";

    PythonBindingGenerator generator(m);
    generator.bind_class<Person, Vehicle>();
}
