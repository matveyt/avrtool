#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// load Intel HEX or Binary file
// return binary image pointer or NULL
// note: 8-bit format only, may rewind(), caller must free()
uint8_t* ihex_load8(size_t* sz, size_t* base, size_t* entry, FILE* f);
// size_t sz, base, entry;
// uint8_t* image = ihex_load8(&sz, &base, &entry, f);
// if (image != NULL) {
//     assert(sz > 0);
//     assert(base <= entry && entry < base + sz);
// }

// convert hex string to byte array
// return number of bytes converted
size_t ihex_blob(uint8_t* blob, size_t sz, const char* str);
// uint8_t blob[100];
// size_t bloblen = ihex_blob(blob, sizeof(blob), "deadbeef");
// assert(bloblen == 4);
