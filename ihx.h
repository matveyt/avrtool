#if !defined(IHX_H)
#define IHX_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#if defined(__cplusplus)
extern "C" {
#endif

// load Intel HEX or Binary file
// note: may fseek(f), caller must free(image)
int ihx_load(uint8_t** image, size_t* sz, size_t* base, size_t* entry, FILE* f);
// uint8_t* image;
// size_t sz, base, entry;
// int fmt = ihx_load(&image, &sz, &base, &entry, f);
// if (fmt < 0) {
//     assert(fmt == -1);
//     assert(image == NULL);
//     assert(sz == 0);
//     assert(base == 0 && entry == 0);
// } else {
//     assert(fmt == 'x' || fmt == 'b');
//     assert(image != NULL);
//     assert(sz > 0);
//     assert(base <= entry && entry < base + sz);
// }

// format output as Intel HEX file
// if filler <= UINT8_MAX then may skip consecutive "filler" bytes
// if wrap == 0 then use default value (16)
void ihx_dump(uint8_t* image, size_t sz, size_t base, size_t entry, unsigned filler,
    unsigned wrap, FILE* f);

#if defined(__cplusplus)
}
#endif

#endif // IHX_H
