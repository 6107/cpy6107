---
applyTo: "bindings/**/*.cpp,bindings/**/*.hpp"
---
# nanobind Wrapper Rules
* Include `<nanobind/nanobind.h>` and `<nanobind/stl/string.h>` / `<nanobind/ndarray.h>` as needed.
* Alias `namespace nb = nanobind;` inside function scopes.
* Use `nb::ndarray<nb::numpy, const double, nb::shape<-1, -1>>` for zero-copy 2D matrix inputs from NumPy.
* Always attach docstrings to every `nb::class_`, `.def()`, and `.def_rw()` call so `nanobind_add_stub` generates complete `.pyi` type hints.
* Release the GIL (`nb::call_guard<nb::gil_scoped_release>()`) on any bound C++ function that runs heavy optimization, trajectory generation, or NetworkTables blocking I/O.
