#include <rapidcheck.h>

int main(int, char **) {
  const bool ok = rc::check("Addition is commutative", [] {
    const int a = *rc::gen::arbitrary<int>();
    const int b = *rc::gen::arbitrary<int>();
    RC_ASSERT(a + b == b + a);
  });
  return ok ? 0 : 1;
}


