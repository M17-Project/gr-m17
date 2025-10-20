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
