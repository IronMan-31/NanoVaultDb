from setuptools import setup
from pybind11.setup_helpers import Pybind11Extension, build_ext

ext_modules = [
    Pybind11Extension(
        "nanodb",
        ["nanodb_python_module/py_bindings.cpp"],
        include_dirs=["."],
        extra_compile_args=["-O3", "-std=c++20", "-fPIC"],
        libraries=["uring", "crypto", "ssl"],
    ),
]

setup(
    name="nanovaultdb",
    version="1.0.1",
    description="High-performance C++ database with Batch writing, web socket support only for high frequency data incoming along with general table storage",
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
    zip_safe=False,
)