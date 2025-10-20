#include <cstdint>
#include <cmath>
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size==0) return 0;
  size_t n = (data[0] % 64) + 1; float in[64]; unsigned char out[64];
  for (size_t i=0;i<n;i++){ in[i] = ((data[i%size]/255.0f)*2.0f)-1.0f; out[i] = in[i]>0 ? 1 : 0; }
  return 0;
}
int main(){return 0;}
