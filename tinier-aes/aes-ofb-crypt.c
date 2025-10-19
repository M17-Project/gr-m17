/*-------------------------------------------------------------------------------
 * aes-ofb-crypt.c         Tinier AES
 * Message Encryptor/Decryptor using AES OFB Mode
 *
 * buid with gcc aes-ofb-crypt.c aes.c -o aes-ofb-crypt.o -Wall -Wextra -Wpedantic
 * run with ./aes-ofb-crypt.o
 *-----------------------------------------------------------------------------*/

#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include "aes.h"

//if supplied IV is only 32-bit in value, then expand it to a 128-bit IV using these LFSR taps
void lfsr_32_to_128(uint8_t * iv)
{
  uint64_t lfsr = 0, bit = 0;

  lfsr = ((uint64_t)iv[0] << 24ULL) + ((uint64_t)iv[1] << 16ULL) + ((uint64_t)iv[2] << 8ULL)  + ((uint64_t)iv[3] << 0ULL);

  uint8_t cnt = 0, x = 32;

  for(cnt = 0;cnt < 96; cnt++) 
  {
    //32,22,2,1 (per Xilinx XAPP 052) Table 3: Taps for Maximum-Length LFSR Counters
    bit = ( (lfsr >> 31) ^ (lfsr >> 21) ^ (lfsr >> 1) ^ (lfsr >> 0) ) & 0x1;
    lfsr = (lfsr << 1) | bit;

    //continue packing iv
    iv[x/8] = (iv[x/8] << 1) + bit;

    x++;
  }

  // SECURITY FIX: Removed IV logging - sensitive material must never be printed
  // fprintf (stderr, "\n IV(128): ");
  // for (x = 0; x < 16; x++)
  //   fprintf (stderr, "%02X", iv[x]);
  // fprintf (stderr, "\n");

}

//if supplied IV is only 64-bit in value, then expand it to a 128-bit IV using these LFSR taps
void lfsr_64_to_128(uint8_t * iv)
{
  uint64_t lfsr = 0, bit = 0;

  lfsr = ((uint64_t)iv[0] << 56ULL) + ((uint64_t)iv[1] << 48ULL) + ((uint64_t)iv[2] << 40ULL) + ((uint64_t)iv[3] << 32ULL) + 
         ((uint64_t)iv[4] << 24ULL) + ((uint64_t)iv[5] << 16ULL) + ((uint64_t)iv[6] << 8ULL)  + ((uint64_t)iv[7] << 0ULL);

  uint8_t cnt = 0, x = 64;

  for(cnt = 0;cnt < 64; cnt++) 
  {
    //63,61,45,37,27,14
    // Polynomial is C(x) = x^64 + x^62 + x^46 + x^38 + x^27 + x^15 + 1
    bit = ((lfsr >> 63) ^ (lfsr >> 61) ^ (lfsr >> 45) ^ (lfsr >> 37) ^ (lfsr >> 26) ^ (lfsr >> 14)) & 0x1;
    lfsr = (lfsr << 1) | bit;

    //continue packing iv
    iv[x/8] = (iv[x/8] << 1) + bit;

    x++;
  }

  // SECURITY FIX: Removed IV logging - sensitive material must never be printed
  // fprintf (stderr, "\n IV(128): ");
  // for (x = 0; x < 16; x++)
  //   fprintf (stderr, "%02X", iv[x]);
  // fprintf (stderr, "\n");

}

//convert a user string into a uint8_t array
uint16_t parse_raw_user_string (char * input, uint8_t * output)
{
  //since we want this as octets, get strlen value, then divide by two
  uint16_t len = strlen((const char*)input);
  
  //if zero is returned, just do two
  if (len == 0) len = 2;

  //if odd number, then user didn't pass complete octets, but just add one to len value to make it even
  if (len&1) len++;

  //divide by two to get octet len
  len /= 2;

  char octet_char[3];
  octet_char[2] = 0;
  uint16_t k = 0;
  uint16_t i = 0;

  //debug
  // fprintf (stderr, "\n Raw Len: %d; Raw Octets:", len);
  for (i = 0; i < len; i++)
  {
    strncpy (octet_char, input+k, 2);
    octet_char[2] = 0;
    sscanf (octet_char, "%hhX", &output[i]);

    //debug
    // fprintf (stderr, " (%s)", octet_char);
    // fprintf (stderr, " %02X", super->m17e.raw[i+1]);
    k += 2;
  }
  // fprintf (stderr, "\n");

  return len;
}

int main (void)
{

  uint8_t i = 0;

  uint8_t key[32];
  // SECURITY FIX: Use secure memory clearing for sensitive data
  explicit_bzero(key, 32*sizeof(uint8_t));

  uint8_t iv[16];
  // SECURITY FIX: Use secure memory clearing for sensitive data
  explicit_bzero(iv, 16*sizeof(uint8_t));

  uint8_t input_bytes[129*18];
  memset (input_bytes, 0, 129*18*sizeof(uint8_t));

  uint8_t keystream_bytes[129*18];
  // SECURITY FIX: Use secure memory clearing for keystream data
  explicit_bzero(keystream_bytes, 129*18*sizeof(uint8_t));

  uint8_t output_bytes[129*18];
  memset (output_bytes, 0, 129*18*sizeof(uint8_t));

  char input_string[3000];

  uint16_t len = 0;
  uint16_t type = 256;  //AES256
  uint16_t nblocks = 1; //number of rounds needed

  fprintf (stderr, "\n----------------Tinier-AES OFB Message Cipher----------------");
  fprintf (stderr, "\n");

  fprintf (stderr, " Enter AES Key Len / Type (128/192/256):  ");
  scanf("%hu", &type);

  //echo input and internally change AES type to work in the internal function
  if (type == 128)
  {
    fprintf (stderr, " AES 128 ");
    type = 0;
  }
  else if (type == 192)
  {
    fprintf (stderr, " AES 192 ");
    type = 1;
  }
  else if (type == 256)
  {
    fprintf (stderr, " AES 256 ");
    type = 2;
  }
  else
  {
    fprintf (stderr, " %d Not Recognized, defaulting to AES 256 ", type);
    type = 2;
  }
  fprintf (stderr, "\n");

  fprintf (stderr, "\n Include any leading zeroes in key values, IV, and Input Message!");
  fprintf (stderr, "\n");

  memset (input_string, 0, 2048*sizeof(char));
  fprintf (stderr, " Enter Key: ");
  scanf("%s", input_string); //no white space allowed
  input_string[2999] = '\0'; //terminate string
  len = parse_raw_user_string(input_string, key);

  // SECURITY FIX: Removed key logging - sensitive material must never be printed
  // fprintf (stderr, "\n Key: ");
  // for (i = 0; i < len; i++)
  //     fprintf (stderr, "%02X", key[i]);

  memset (input_string, 0, 2048*sizeof(char));
  fprintf (stderr, "\n Enter IV (4, 8, or 16 octets): ");
  scanf("%s", input_string); //no white space allowed
  input_string[2999] = '\0'; //terminate string
  len = parse_raw_user_string(input_string, iv);

  //print input IV len in octets
  fprintf (stderr, "\n  IV Len: %02d Octets;", len);

  //if supplied IV len is 4 octets (32-bit)
  if (len == 4)
  {
    // SECURITY FIX: Removed IV logging - sensitive material must never be printed
    // fprintf (stderr, "\n  IV(32): ");
    // for (i = 0; i < len; i++)
    //     fprintf (stderr, "%02X", iv[i]);

    //LFSR to expand a 32-bit IV into a 128-bit IV
    lfsr_32_to_128(iv);
  }

  //if supplied IV len is 8 octets (64-bit)
  else if (len == 8)
  {
    // SECURITY FIX: Removed IV logging - sensitive material must never be printed
    // fprintf (stderr, "\n  IV(64): ");
    // for (i = 0; i < len; i++)
    //     fprintf (stderr, "%02X", iv[i]);

    //LFSR to expand a 64-bit IV into a 128-bit IV
    lfsr_64_to_128(iv);
  }

  //if supplied IV len is 16 octets (128-bit)
  else if (len == 16)
  {
    // SECURITY FIX: Removed IV logging - sensitive material must never be printed
    // fprintf (stderr, "\n IV(128): ");
    // for (i = 0; i < len; i++)
    //   fprintf (stderr, "%02X", iv[i]);
    fprintf (stderr, "\n");
  }

  else
  {
    fprintf (stderr, "\n Abnormal IV len %d octets; Please enter 32-bit(8 hex char/4 octets),\n 64-bit (16 hex char/8 octets), or 128-bit (32 hex char/16 octet) IV; ", len);
    return 0;
  }

  memset (input_string, 0, 2048*sizeof(char));
  fprintf (stderr, "\n Enter Input Message (Hex Octets): ");
  scanf("%s", input_string); //no white space allowed
  input_string[2999] = '\0'; //terminate string
  len = parse_raw_user_string(input_string, input_bytes);

  //calculate nblocks needed based on returned len value here
  nblocks = len / 16;
  if (len % 16) nblocks += 1;
  nblocks++; //add one more for the OFB discard round

  //print input
  fprintf (stderr, "\n  Input: ");
  for (i = 0; i < len; i++)
    fprintf (stderr, "%02X", input_bytes[i]);

  //byte-wise output of AES OFB Keystream
  aes_ofb_keystream_output(iv, key, keystream_bytes, type, nblocks);

  //xor keystream vs input to get output (+16 for discard round on keystream)
  for (i = 0; i < 16*(nblocks-1); i++)
    output_bytes[i] = input_bytes[i] ^ keystream_bytes[i+16];

  //print output
  fprintf (stderr, "\n\n Output: ");
  for (i = 0; i < 16*(nblocks-1); i++)
  {
    if ((i != 0) && ((i%8) == 0))
      fprintf (stderr, " ");
    fprintf (stderr, "%02X", output_bytes[i]);
  }

  //print output (as string)
  // fprintf (stderr, "\n Output: %s", output_bytes);

  //ending line break
  fprintf (stderr, "\n ");

  return 0;
}
