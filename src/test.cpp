
#include <string>

#include "clcxx/clcxx.hpp"

class xx {
 public:
  xx(int xx, int yy) : y(yy), x(xx) {}
  std::string greet() { return "Hello, World"; }

  int y;
  int x;
};

std::string greet() { return "Hello, World"; }
int Int(int x) { return x + 100; }
float Float(float y) { return y + 100.34; }
auto gr(std::complex<float> x) { return x; }
std::string hi(char *s) { return std::string("hi, " + std::string(s)); }

void ref_int(int& x) { x += 30; }
void ref_class(xx& x) { x.y = 1000000; }


CLCXX_PACKAGE TEST(clcxx::Package &pack) {
  pack.defun("hi", &hi);
  pack.defun("int", &Int);
  pack.defun("greet", &greet);
  pack.defun("float", &Float);
  pack.defun("complex", &gr);
  pack.defun("ref-int", &ref_int);
  pack.defun("ref-class", &ref_class);
  pack.defclass<xx, false>("xx")
      .defmethod("foo", &xx::greet)
      .constructor<int, int>();
}
