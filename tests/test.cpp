#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>
#include <clcxx/clcxx.hpp>
#include <complex>
#include <cstring>
#include <iostream>
#include <string>

#define CONFIG_CATCH_MAIN

class Pod {
 public:
  int x;
  float y;
  long int one() const { return 1; }
};

const Pod &ReturnPodRef() {
  static auto x = Pod{1, 3.5};
  return x;
}

const Pod ReturnPod() { return Pod{1, 3.5}; }
Pod ManipulatePod(Pod a) {
  a.x += 10;
  a.y += 10;
  return a;
}

double FuncPtrDummy(double x) { return x + 10; }
double FuncStd(
    const std::function<double(double)> &f) {  // FIXME: don't try this
  return f(1.5);
}
double FuncPtr(double (*f)(double)) { return f(1.5); }

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

std::string Greet() { return "Hello, World"; }
int Int(int x) { return x + 100; }
float Float(const float y) { return y + 100.34; }
auto ComplexReal(std::complex<float> x) { return real(x); }
auto ComplexImag(std::complex<float> x) { return imag(x); }
std::string Hi(const char *s) { return std::string("hi, " + std::string(s)); }

void RefInt(int &x) { x += 30; }
void RefClass(A &x) { x.y = 1000000; }

CLCXX_PACKAGE Test(clcxx::Package &pack) {
  pack.defun("hi", F_PTR(&Hi));
  pack.defun("test-int", F_PTR(&Int));
  pack.defun("greet", F_PTR(&Greet));
  pack.defun("test-float", F_PTR(&Float));
  pack.defun("test-complex1", F_PTR(&ComplexReal));
  pack.defun("test-complex2", F_PTR(&ComplexImag));
  pack.defun("ref-int", F_PTR(&RefInt));
  pack.defclass<A, false>("A")
      .defmethod("class-greet", F_PTR(&A::Greet))          // 7
      .defmethod("class-operator", F_PTR(&A::operator()))  // 8
      .defmethod("class-lambda", F_PTR([](A x) { return x.x; }))
      .constructor<int, int>()
      .member("y", &A::y)
      .defmethod("add", F_PTR(&A::add<int>))
      .defmethod("one", F_PTR(&A::one));
  pack.defun("ref-class", F_PTR(&RefClass));                    // 15
  pack.defun("lambda1", F_PTR([](A x) { return x.x; }));        // 16
  pack.defun("create-class", F_PTR([]() { return A(1, 4); }));  // 17
  pack.defun("dummy", F_PTR(&ReturnPodRef));                    // 18
  pack.defcstruct<Pod>("Pod").member("x", &Pod::x).member("y", &Pod::y);

  pack.defun("create-pod", F_PTR(&ReturnPod));      // 19
  pack.defun("create-pod", F_PTR(&ManipulatePod));  // 20
  pack.defun("func-ptr", F_PTR(&FuncPtr));          // 21
}

CLCXX_PACKAGE Test2(clcxx::Package &pack) {
  pack.defun("create-pod", F_PTR(&ReturnPod));
}

TEST_CASE("clcxx test", "[clcxx]") {
  // // auto d = clcxx::Import([]() { return &A::one; });
  // constexpr auto f = &A::one;
  // // auto k = std::addressof(DoApply<long int, const A>);  //, float,  const
  // // int>;
  // using S = const A;
  // // auto k = &DoApply<long int, S>;  //, float, const int>;
  // std::cout << clcxx::TypeName<decltype(f)>() << std::endl;
  // std::cout << clcxx::TypeName<clcxx::ToLisp_t<const int>>() << std::endl;
  // std::cout << clcxx::TypeName<clcxx::ToLisp_t<const A> &&>() << std::endl;
  // std::cout << clcxx::TypeName<clcxx::ToLisp_t<const A &>>() << std::endl;
  // std::cout << clcxx::TypeName<clcxx::ToLisp_t<const A &> &&>() << std::endl;
  // std::cout << clcxx::TypeName<decltype(k)>() << std::endl;
  // std::cout << clcxx::TypeName<decltype(d)>() << std::endl;
  clcxx::Package &pack = clcxx::registry().create_package("test");
  Test(pack);

  REQUIRE(clcxx::registry().has_current_package());
  REQUIRE(pack.functions_meta_data()[0].func_ptr !=
          pack.functions_meta_data()[1].func_ptr);
  REQUIRE(pack.functions_meta_data()[8].func_ptr !=
          pack.functions_meta_data()[9].func_ptr);

  {
    auto f = clcxx::Import([&]() { return &Hi; });
    auto x = "bro.";
    auto res = std::invoke(reinterpret_cast<decltype(f)>(
                               pack.functions_meta_data().at(0).func_ptr),
                           std::forward<const char *>(x));
    REQUIRE(strcmp(res, "hi, bro.") == 0);
  }
  {
    auto f = clcxx::Import([&]() { return &Int; });
    auto res = std::invoke(reinterpret_cast<decltype(f)>(
                               pack.functions_meta_data().at(1).func_ptr),
                           (int)7);
    REQUIRE(res == 107);
  }
  {
    auto f = clcxx::Import([&]() { return &Greet; });
    auto res = std::invoke(reinterpret_cast<decltype(f)>(
        pack.functions_meta_data().at(2).func_ptr));
    REQUIRE(strcmp(res, "Hello, World") == 0);
  }
  {
    auto f = clcxx::Import([&]() { return &Float; });
    auto res = std::invoke(reinterpret_cast<decltype(f)>(
                               pack.functions_meta_data().at(3).func_ptr),
                           6.66f);
    REQUIRE(res == 107.0f);
  }
  {
    auto f = clcxx::Import([&]() { return &ComplexReal; });
    auto res = std::invoke(reinterpret_cast<decltype(f)>(
                               pack.functions_meta_data().at(4).func_ptr),
                           clcxx::LispComplex{{7.1f}, {5.1f}});
    REQUIRE(res == 7.1f);
  }
  {
    auto f = clcxx::Import([&]() { return &ComplexImag; });
    auto res = std::invoke(reinterpret_cast<decltype(f)>(
                               pack.functions_meta_data().at(5).func_ptr),
                           clcxx::LispComplex{{7.1f}, {5.1f}});
    REQUIRE(res == 5.1f);
  }
  {
    int res = 20;
    auto f = clcxx::Import([&]() { return &RefInt; });
    std::invoke(reinterpret_cast<decltype(f)>(
                    pack.functions_meta_data().at(6).func_ptr),
                (int *)&res);
    REQUIRE(res == 50);
  }
  {
    A a(7, 2);
    auto f = clcxx::Import([]() { return &A::Greet; });
    auto res = std::invoke(reinterpret_cast<decltype(f)>(
                               pack.functions_meta_data().at(7).func_ptr),
                           (void *)&a);
    REQUIRE(strcmp(res, "Hello, World") == 0);
  }
  {
    A a(7, 2);
    auto f = clcxx::Import([]() { return &A::operator(); });
    auto res = std::invoke(reinterpret_cast<decltype(f)>(
                               pack.functions_meta_data().at(8).func_ptr),
                           (void *)&a);
    REQUIRE(res == 0.0f);
  }
  {
    A a(7, 2);
    auto f = clcxx::Import([]() { return [](A x) { return x.x; }; });
    auto res = std::invoke(reinterpret_cast<decltype(f)>(
                               pack.functions_meta_data().at(9).func_ptr),
                           (void *)&a);
    REQUIRE(res == 7);
  }
  {
    A a(7, 2);
    auto f = clcxx::Import(
        []() { return &clcxx::detail::CppConstructor<A, int, int>; });
    auto res = clcxx::ToCpp<A>(
        std::invoke(reinterpret_cast<decltype(f)>(
                        pack.functions_meta_data().at(10).func_ptr),
                    a.x, a.y));
    REQUIRE(res.x == a.x);
    REQUIRE(res.y == a.y);
  }
  {
    A a(1, 2);
    auto f = clcxx::Import([&]() { return &RefClass; });
    std::invoke(reinterpret_cast<decltype(f)>(
                    pack.functions_meta_data().at(15).func_ptr),
                (void *)&a);
    REQUIRE(a.x == 1);
    REQUIRE(a.y == 1000000);
  }
  {
    A a(7, 2);
    auto f = clcxx::Import([]() { return [](A x) { return x.x; }; });
    auto res = std::invoke(reinterpret_cast<decltype(f)>(
                               pack.functions_meta_data().at(16).func_ptr),
                           (void *)&a);
    REQUIRE(res == 7);
  }
  {
    Pod a{1, 3.5};
    auto f = clcxx::Import([]() { return &ReturnPod; });
    auto res = std::invoke(reinterpret_cast<decltype(f)>(
        pack.functions_meta_data().at(19).func_ptr));
    REQUIRE(res.x == a.x);
    REQUIRE(res.y == a.y);
  }
  {
    Pod a{1, 3.5};
    auto f = clcxx::Import([]() { return &ManipulatePod; });
    auto res = std::invoke(reinterpret_cast<decltype(f)>(
                               pack.functions_meta_data().at(20).func_ptr),
                           a);
    REQUIRE(res.x == a.x + 10);
    REQUIRE(res.y == a.y + 10);
  }
  {
    auto f = clcxx::Import([]() { return &FuncPtr; });
    auto res = std::invoke(reinterpret_cast<decltype(f)>(
                               pack.functions_meta_data().at(21).func_ptr),
                           (void (*)())FuncPtrDummy);
    REQUIRE(res == FuncPtr(FuncPtrDummy));
  }

  REQUIRE_NOTHROW(clcxx::registry().remove_package("test"));
  // REQUIRE_THROWS(clcxx::registry().remove_package("test"));

  // REQUIRE_THROWS(Test2(pack));
}
