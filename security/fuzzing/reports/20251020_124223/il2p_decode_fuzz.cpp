#include <cstdint>
#include <cstddef>
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size == 0 || size > 8192) return 0;
  uint8_t pn = size ? data[0] : 0;
  for (size_t i = 0; i < size; i++) { volatile uint8_t v = data[i] ^ pn; (void)v; }
  return 0;
}
int main(){return 0;}
