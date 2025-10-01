#include <introspection/PyGenerator.h>
#include "../demo.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

// pybind11 module definition using automatic binding
PYBIND11_MODULE(pyintrospection, m)
{
    m.doc() = "Automatic Python bindings using C++ introspection";

    PyGenerator generator(m);
    generator.bind_classes<Person, Vehicle>();
}
