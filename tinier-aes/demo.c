/*-------------------------------------------------------------------------------
 * demo.c         Tinier AES
 * Quick Work Flow Demonstation for Various AES Modes
 *
 * buid with gcc demo.c aes.c -o demo.o -Wall -Wextra -Wpedantic
 * run with ./demo.o
 *-----------------------------------------------------------------------------*/

#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include "aes.h"

int main (void)
{

  // SECURITY FIX: Use cryptographically secure random number generation
  int i = 0;

  //AES Type/Key Len
  int type = 2; // 0 - 128, 1 - 192, 2 - 256

  //number of rounds of OFB Keystream Blocks (16-byte blocks) to produce
  int nblocks = 2; //2 or more, first round is usually always discarded in OFB applications

  uint8_t key[32];
  // SECURITY FIX: Use /dev/urandom for secure key generation
  FILE *urandom = fopen("/dev/urandom", "rb");
  if (urandom == NULL) {
    fprintf(stderr, "ERROR: Cannot access secure random number generator\n");
    return -1;
  }
  if (fread(key, 1, 32, urandom) != 32) {
    fclose(urandom);
    fprintf(stderr, "ERROR: Failed to read secure random bytes for key\n");
    return -1;
  }
  fclose(urandom);

  uint8_t iv[16];
  // SECURITY FIX: Use /dev/urandom for secure IV generation
  urandom = fopen("/dev/urandom", "rb");
  if (urandom == NULL) {
    fprintf(stderr, "ERROR: Cannot access secure random number generator\n");
    return -1;
  }
  if (fread(iv, 1, 16, urandom) != 16) {
    fclose(urandom);
    fprintf(stderr, "ERROR: Failed to read secure random bytes for IV\n");
    return -1;
  }
  fclose(urandom);

  uint8_t input_bytes[16];
  // SECURITY FIX: Use /dev/urandom for secure input generation
  urandom = fopen("/dev/urandom", "rb");
  if (urandom == NULL) {
    fprintf(stderr, "ERROR: Cannot access secure random number generator\n");
    return -1;
  }
  if (fread(input_bytes, 1, 16, urandom) != 16) {
    fclose(urandom);
    fprintf(stderr, "ERROR: Failed to read secure random bytes for input\n");
    return -1;
  }
  fclose(urandom);

  uint8_t output_bytes[32];
  // SECURITY FIX: Use secure memory clearing for sensitive data
  explicit_bzero(output_bytes, 32*sizeof(uint8_t));

  fprintf (stderr, "\n---------------Tinier-AES Workflow Demo----------------");

  // SECURITY FIX: Removed key logging - sensitive material must never be printed
  // fprintf (stderr, "\nKey:   ");
  // for (i = 0; i < 32; i++)
  // {
  //   if (i == 16)
  //     fprintf (stderr, "\n       ");
  //   fprintf (stderr, " %02X", key[i]);
  // }

  // SECURITY FIX: Removed IV logging - sensitive material must never be printed
  // fprintf (stderr, "\nIV:    ");
  // for (i = 0; i < 16; i++)
  //   fprintf (stderr, " %02X", iv[i]);

  //print input
  fprintf (stderr, "\nInput: ");
  for (i = 0; i < 16; i++)
    fprintf (stderr, " %02X", input_bytes[i]);

  // //execute aes_ctr with a 256-bit key (type 2)
  // aes_ctr_bytewise_payload_crypt (iv, key, input_bytes, type);

  // //print output
  // fprintf (stderr, "\nOutput:");
  // for (i = 0; i < 16; i++)
  //   fprintf (stderr, " %02X", input_bytes[i]);

  // //or run a simple ECB mode without using an initialization vector
  // int de = 1; //1 for encrypt, 0 for decrypt
  // aes_ecb_bytewise_payload_crypt (input_bytes, key, output_bytes, type, de);

  // //print output
  // fprintf (stderr, "\nOutput:");
  // for (i = 0; i < 16; i++)
  //   fprintf (stderr, " %02X", output_bytes[i]);

  //or run OFB mode, and create a keystream to XOR on input_bytes
  aes_ofb_keystream_output (iv, key, output_bytes, type, nblocks);

  // SECURITY FIX: Removed keystream logging - sensitive material must never be printed
  // fprintf (stderr, "\nKS:    ");
  // for (i = 0; i < 16; i++)
  //   fprintf (stderr, " %02X", output_bytes[i+16]);

  //xor ofb input_bytes against output_bytes (keystream) to cipher/decipher input payload
  for (i = 0; i < 16*(nblocks-1); i++)
    input_bytes[i] ^= output_bytes[i+16]; //+16 is the offset for the first round OFB discard

  //print ciphered input
  fprintf (stderr, "\nOutput:");
  for (i = 0; i < 16; i++)
    fprintf (stderr, " %02X", input_bytes[i]);

  fprintf (stderr, "\n");

  return 0;
}
