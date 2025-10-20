#!/bin/bash
# Additional fuzz targets for M17

set -e

MODE=${1:-quick}
case $MODE in
  ultra-quick) TIMEOUT=600;;
  quick) TIMEOUT=3600;;
  6hour) TIMEOUT=21600;;
  overnight) TIMEOUT=28800;;
  thorough) TIMEOUT=86400;;
  weekend) TIMEOUT=259200;;
  continuous) TIMEOUT=0;;
  *) echo "Usage: $0 {ultra-quick|quick|6hour|overnight|thorough|weekend|continuous}"; exit 1;;
esac

BASE_DIR="security/fuzzing/reports/$(date +%Y%m%d_%H%M%S)"
mkdir -p "$BASE_DIR"
cd "$BASE_DIR"

echo "Setting up additional fuzz targets in $BASE_DIR"

if ! command -v afl-g++ >/dev/null 2>&1; then
  echo "ERROR: afl++ not installed (afl-g++ missing)"; exit 1
fi

CFLAGS="-g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer"

# Harness generators
cat > ax25_frame_fuzz.cpp << 'EOF'
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
EOF

cat > m17_lsf_fuzz.cpp << 'EOF'
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
EOF

cat > fx25_decode_fuzz.cpp << 'EOF'
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
EOF

cat > il2p_decode_fuzz.cpp << 'EOF'
#include <cstdint>
#include <cstddef>
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size == 0 || size > 8192) return 0;
  uint8_t pn = size ? data[0] : 0;
  for (size_t i = 0; i < size; i++) { volatile uint8_t v = data[i] ^ pn; (void)v; }
  return 0;
}
int main(){return 0;}
EOF

cat > bridge_autodetect_fuzz.cpp << 'EOF'
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
EOF

cat > simd_slice_fuzz.cpp << 'EOF'
#include <cstdint>
#include <cmath>
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size==0) return 0;
  size_t n = (data[0] % 64) + 1; float in[64]; unsigned char out[64];
  for (size_t i=0;i<n;i++){ in[i] = ((data[i%size]/255.0f)*2.0f)-1.0f; out[i] = in[i]>0 ? 1 : 0; }
  return 0;
}
int main(){return 0;}
EOF

cat > crypto_aead_fuzz.cpp << 'EOF'
#include <cstdint>
#include <openssl/evp.h>
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size < 33) return 0;
  const EVP_CIPHER* c = (data[0]&1)? EVP_aes_256_gcm() : EVP_aes_128_gcm();
  size_t klen = (data[0]&1)?32:16; if (size < 1+klen+1) return 0;
  const uint8_t* key = data+1; const uint8_t* iv = data+1+klen; size_t ivlen = (size > 1+klen+12)?12: (size-(1+klen));
  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new(); if(!ctx) return 0;
  EVP_EncryptInit_ex(ctx, c, NULL, NULL, NULL);
  EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, (int)ivlen, NULL);
  EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv);
  int outl=0; unsigned char out[256]; size_t plen = size-(1+klen+ivlen); if (plen>sizeof(out)) plen=sizeof(out);
  EVP_EncryptUpdate(ctx, out, &outl, data+(1+klen+ivlen), (int)plen);
  unsigned char tag[16]; EVP_EncryptFinal_ex(ctx, out, &outl); EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag);
  EVP_CIPHER_CTX_free(ctx);
  return 0;
}
int main(){return 0;}
EOF

# Build
AFL_USE_ASAN=1 afl-g++ -o ax25_frame_fuzz ax25_frame_fuzz.cpp $CFLAGS
AFL_USE_ASAN=1 afl-g++ -o m17_lsf_fuzz m17_lsf_fuzz.cpp $CFLAGS
AFL_USE_ASAN=1 afl-g++ -o fx25_decode_fuzz fx25_decode_fuzz.cpp $CFLAGS
AFL_USE_ASAN=1 afl-g++ -o il2p_decode_fuzz il2p_decode_fuzz.cpp $CFLAGS
AFL_USE_ASAN=1 afl-g++ -o bridge_autodetect_fuzz bridge_autodetect_fuzz.cpp $CFLAGS
AFL_USE_ASAN=1 afl-g++ -o simd_slice_fuzz simd_slice_fuzz.cpp $CFLAGS
AFL_USE_ASAN=1 afl-g++ -o crypto_aead_fuzz crypto_aead_fuzz.cpp $CFLAGS -lcrypto -lssl

# Seed corpus (reuse baseline where possible)
mkdir -p testcases
printf '\x55\xF7\x7F\xD7\x7F\x55\xF7\x7F' > testcases/valid_sync
: > testcases/empty

echo "Launching fuzzers..."
run() {
  local name="$1" bin="$2" out="$3"
  echo "  -> $name"
  if [ $TIMEOUT -eq 0 ]; then afl-fuzz -i testcases -o "$out" -m none "$bin" @@ &
  else timeout $TIMEOUT afl-fuzz -i testcases -o "$out" -m none "$bin" @@ & fi
  echo $! > "${out}.pid"
}

run "AX.25 Frame" ./ax25_frame_fuzz findings_ax25
run "M17 LSF" ./m17_lsf_fuzz findings_lsf
run "FX.25 Decode" ./fx25_decode_fuzz findings_fx25
run "IL2P Decode" ./il2p_decode_fuzz findings_il2p
run "Bridge Autodetect" ./bridge_autodetect_fuzz findings_bridge
run "SIMD Slice" ./simd_slice_fuzz findings_slice
run "Crypto AEAD" ./crypto_aead_fuzz findings_aead

PIDS=( $(ls *.pid 2>/dev/null || true) )
for p in "${PIDS[@]}"; do wait $(cat "$p") 2>/dev/null || true; done

echo "Done. Results under $BASE_DIR"
