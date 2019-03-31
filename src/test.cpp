
#include <string>

#include "clcxx/clcxx.hpp"

class xx {
public:
  int y;
  int r;
  std::string greet() { return "asdHello, World"; }
};

std::string greet() { return "asdHello, World"; }
int Int(int x) { return x + 100; }
float Float(float y) { return y + 100.34; }
auto gr(std::complex<float> x) { return x; }
std::string *hi(char *s) {
  auto x = std::string("hi, " + std::string(s));
  auto y = &x;
  return (y);
}

CLCXX_PACKAGE TEST(clcxx::Package &pack) {
  pack.defun("hi", &hi);
  pack.defun("int", &Int);
  pack.defun("greet", &greet);
  pack.defun("float", &Float);
  pack.defun("complex", &gr);
  pack.defclass<xx>("xx").method("foo", &xx::greet);
}

