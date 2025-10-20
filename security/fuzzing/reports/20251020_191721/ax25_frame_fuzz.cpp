#include <cstdint>
#include <cstring>
#include <cstdlib>
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size < 16 || size > 4096) return 0;
  char src[7] = {0}, dst[7] = {0};
  for (int i = 0; i < 6 && 1 + i < (int)size; i++) dst[i] = (char)(data[1+i] >> 1);
  for (int i = 0; i < 6 && 8 + i < (int)size; i++) src[i] = (char)(data[8+i] >> 1);
  uint8_t ctrl = size > 14 ? data[14] : 0; (void)ctrl;
  uint8_t pid  = size > 15 ? data[15] : 0; (void)pid;
  uint8_t info[330]; size_t n = size > sizeof(info) ? sizeof(info) : size; memcpy(info, data, n);
  return 0;
}
int main(){return 0;}
