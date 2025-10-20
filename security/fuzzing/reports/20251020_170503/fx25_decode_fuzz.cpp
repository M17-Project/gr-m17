#include <cstdint>
#include <cstring>
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size == 0 || size > 8192) return 0;
  const size_t sym = 1 + (data[0] & 31);
  for (size_t i = 0; i + sym <= size && i < 2048; i += sym) {
    volatile uint8_t acc = 0; for (size_t j=0;j<sym;j++) acc ^= data[i+j];
  }
  return 0;
}
int main(){return 0;}
