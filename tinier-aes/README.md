# Tinier AES

Very Simple and Easy To Use AES Functions based off of [Tiny-AES-c](https://github.com/kokke/tiny-AES-c "Tiny-AES-c"), all slimmed down into one file with a configurable Nb, Nk, and Nr value, with some included bit and byte utilities, convenience wrapper functions for ECB, OFB, CFB(128), CBC, CBC_MAC, and CTR mode, and a very easy to follow demos (demo.c and aes-ofb-crypt.c) to help you write your own code for it.

Just copy and paste the aes.c and aes.h file into any folder you want and be sure to `#include "aes.h"` in your .c file or .h file.