#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>
#include <clcxx/clcxx.hpp>
#include <complex>
#include <cstring>
#include <iostream>
#include <string>

#define CONFIG_CATCH_MAIN

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
  pack.defun("ref-class", F_PTR(&RefClass));
  pack.defun("lambda1", F_PTR([](A x) { return x.x; }));  // 8
  pack.defun("lambda2", F_PTR([](A x) { return x.x; }));  // 9
  pack.defclass<A, false>("A")
      .defmethod("class-greet", F_PTR(&A::Greet))
      .defmethod("class-operator", F_PTR(&A::operator()))
      .defmethod("class-lambda", F_PTR([](A x) { return x.x; }))
      .constructor<int, int>()
      .member("y", &A::y)
      .defmethod("add", F_PTR(&A::add<int>))
      .defmethod("one", F_PTR(&A::one));
  pack.defun("create-class", F_PTR([]() { return A(1, 4); }));
}

TEST_CASE("clcxx test", "[clcxx]") {
  // // auto d = clcxx::Import([]() { return &A::one; });
  // constexpr auto f = &A::one;
  // // auto k = std::addressof(DoApply<long int, const A>);  //, float, const
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
  clcxx::Package &pack = clcxx::registry().create_package("asd");
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
    A a(1, 2);
    auto f = clcxx::Import([&]() { return &RefClass; });
    std::invoke(reinterpret_cast<decltype(f)>(
                    pack.functions_meta_data().at(7).func_ptr),
                (void *)&a);
    REQUIRE(a.x == 1);
    REQUIRE(a.y == 1000000);
  }
  {
    A a(7, 2);
    auto f = clcxx::Import([]() { return [](A x) { return x.x; }; });
    auto res = std::invoke(reinterpret_cast<decltype(f)>(
                               pack.functions_meta_data().at(8).func_ptr),
                           (void *)&a);
    REQUIRE(res == 7);
  }
  {
    A a(7, 2);
    auto f = clcxx::Import([]() { return &A::Greet; });
    auto res = std::invoke(reinterpret_cast<decltype(f)>(
                               pack.functions_meta_data().at(10).func_ptr),
                           (void *)&a);
    REQUIRE(strcmp(res, "Hello, World") == 0);
  }
  {
    A a(7, 2);
    auto f = clcxx::Import([]() { return &A::operator(); });
    auto res = std::invoke(reinterpret_cast<decltype(f)>(
                               pack.functions_meta_data().at(11).func_ptr),
                           (void *)&a);
    REQUIRE(res == 0.0f);
  }
  {
    A a(7, 2);
    auto f = clcxx::Import([]() { return [](A x) { return x.x; }; });
    auto res = std::invoke(reinterpret_cast<decltype(f)>(
                               pack.functions_meta_data().at(12).func_ptr),
                           (void *)&a);
    REQUIRE(res == 7);
  }
  {
    A a(7, 2);
    auto f = clcxx::Import(
        []() { return &clcxx::detail::CppConstructor<A, int, int>; });
    auto res = clcxx::ToCpp<A>(
        std::invoke(reinterpret_cast<decltype(f)>(
                        pack.functions_meta_data().at(13).func_ptr),
                    a.x, a.y));
    REQUIRE(res.x == a.x);
    REQUIRE(res.y == a.y);
  }
}
