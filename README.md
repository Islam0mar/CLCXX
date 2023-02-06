# CLCXX

This is a C++ library to be used with COMMON-LISP [cl-cxx](https://github.com/Islam0mar/cl-cxx) such as boost.python, PYBIND11, ...

Forked from julia language [libcxxwrap](https://github.com/JuliaInterop/libcxxwrap-julia)

## Example
- function prototype must be `CLCXX_PACKAGE <name>(clcxx::Package &pack)`.
- basic example
```c++
#include <clcxx/clcxx.hpp>
int Int(int x) { return x + 100; }
CLCXX_PACKAGE Test(clcxx::Package &pack) {
  pack.defun("hi", F_PTR(&Hi));}
```
compiled into a shared lib.
- add class with default initializer `pack.defclass<A, true>("A");`
- add class without  default initializer `pack.defclass<A, false>("A");`
- basic class example:
```c++
#include <clcxx/clcxx.hpp>

class A {
 public:
  A(int A, int yy) : y(yy), x(A) {}
  std::string Greet() { return "Hello, World"; }
  float operator()(void) { return 0.0; }
  int y;
  int x;
  template <typename T>
  long int add(T) {
    return 1;
  }
  long int one() const { return 1; }
};

pack.defclass<A, false>("A")
      .defmethod("class-greet", F_PTR(&A::Greet))
      .defmethod("class-operator", F_PTR(&A::operator()))
      .defmethod("class-lambda", F_PTR([](A x) { return x.x; }))
      .constructor<int, int>()
      .member("y", &A::y)
      .defmethod("add", F_PTR(&A::add<int>))
      .defmethod("one", F_PTR(&A::one));
```
- add overloaded function by creating new lambda `.defmethod("m.resize",
                 F_PTR([](Eigen::MatrixXd& m, Eigen::Index i, Eigen::Index j) {
                   return m.resize(i, j);
                 }))`
## Architecture

- `C++` functions/lambda/member_function are converted into an overload function `DoApply` and it's pointer is safed and passed to lisp `cffi`.
- `C++` `fundamental/array/pod_struct` are converted as they are (*copied*) to lisp `cffi` types.
- `C++` `&` are converted to raw pointer `void *` with no allocation.
- `C++` `*` are passed as `void *` with `static_cast`.
- `C++` non-POD `class` are passed as `void *` after allocation with `std::pmr::memory_resource`.
- `C++` `std::strings` are converted to `const char *` after allocation with `std::pmr::memory_resource`.
- `C++` `std::complex` are copied to lisp.

# done
- C++ function, lambda and c functions auto type conversion.
- Classes

# TODO:
- [x] resolve const type template
- [x] reference types
- [ ] support tuples,vectors,...

# compilation

```shell
    mkdir build
    cd build
    cmake ..
    make
    make install
```


