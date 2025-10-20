#include <cstdint>
#include <cstring>
static inline bool looks_m17(const uint8_t* d,size_t n){
  const uint8_t sync[8]={0x55,0xF7,0x7F,0xD7,0x7F,0x55,0xF7,0x7F};
  return n>=8 && memcmp(d,sync,8)==0;
}
static inline bool looks_ax25(const uint8_t* d,size_t n){return n>=16 && (d[14]&0x01)==0x01;}
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size<1||size>8192) return 0;
  if (looks_m17(data,size)) {
  } else if (looks_ax25(data,size)) {
  }
  uint8_t buf[1024]; size_t c=size>sizeof(buf)?sizeof(buf):size; memcpy(buf,data,c);
  return 0;
}
int main(){return 0;}
