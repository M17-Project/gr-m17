#include <cstdint>
#include <cstring>
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size < 2) return 0;
  uint16_t type = (data[0] << 8) | data[1]; (void)type;
  uint8_t meta[14] = {0}; size_t m = size >= 16 ? 14 : (size > 2 ? size-2 : 0);
  if (m) memcpy(meta, data+2, m);
  uint8_t tmp[28] = {0}; size_t c = size > sizeof(tmp) ? sizeof(tmp) : size; memcpy(tmp, data, c);
  return 0;
}
int main(){return 0;}
