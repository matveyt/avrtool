#if !defined(IHX_H)
#define IHX_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct {
    uint8_t* image;
    size_t sz, base, entry;
} IHX;

// load Intel HEX or Binary file
// note: may fseek(f), caller must free(image)
int ihx_load(IHX* ihx, unsigned filler, FILE* f);
// IHX ihx;
// int fmt = ihx_load(&ihx, 0xff, f);
// if (fmt < 0) {
//     assert(fmt == -1);
//     assert(ihx.image == NULL);
//     assert(ihx.sz == 0);
//     assert(ihx.base == 0 && ihx.entry == 0);
// } else {
//     assert(fmt == 'x' || fmt == 'b');
//     assert(ihx.image != NULL);
//     assert(ihx.sz > 0);
//     assert(ihx.base <= ihx.entry && ihx.entry < ihx.base + ihx.sz);
// }

// format output as Intel HEX file
// if filler <= 255 then may skip consecutive "filler" bytes
// if wrap == 0 then use default value (16)
void ihx_dump(IHX* ihx, unsigned filler, unsigned wrap, FILE* f);

#if defined(__cplusplus)
}
#endif

#endif // IHX_H
