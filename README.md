# CLCXX

Forked from julia language [libcxxwrap](https://github.com/JuliaInterop/libcxxwrap-julia)
This is a C++ library to be used with COMMON-LISP such as boost.python, PYBIND11, ...


# done
- C++ function, lambda and c functions auto type conversion.
- Classes

# TODO:
- [ ] resolve const type template
- [ ] convert reference to fundamental types to mapped-types not pointers
- [ ] support tuples

# compilation

```shell
    mkdir build
    cd build
    cmake ..
    make
    make install
```


