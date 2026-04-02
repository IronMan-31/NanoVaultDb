#include <pybind11/pybind11.h>
#include "main_db_class.hpp"

namespace py = pybind11;

PYBIND11_MODULE(nanodb, m) {
    py::class_<NanoVaultDB>(m, "NanoVaultDB")
        .def(py::init<>())
        .def("init", &NanoVaultDB::init)
        .def("execute", &NanoVaultDB::execute)
        .def("enter_shell", &NanoVaultDB::enter_shell);
}