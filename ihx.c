#include "ihx.h"
#include "stdz.h"

#define MIN_BYTES   5
#define MAX_BYTES   (MIN_BYTES + 255)
#define MIN_LINE    (1 + 2 * MIN_BYTES)
#define MAX_LINE    (1 + 2 * MAX_BYTES)

// parsed record
typedef struct {
    unsigned count;
    size_t address;
    int type;
    uint8_t data[255];
} CHUNK;

static int char2hex(int c)
{
    if (c < '0')
        return -1;
    if (c <= '9')
        return c - '0';
    if (c < 'A')
        return -1;
    if (c <= 'F')
        return c - 'A' + 10;
    if (c < 'a')
        return -1;
    if (c <= 'f')
        return c - 'a' + 10;
    return -1;
}

static unsigned make16(unsigned high, unsigned low)
{
    return (high << 8) + low;
}

static unsigned sum8(unsigned word)
{
    return (word >> 8) + word;
}

// parse one record
// return record type or -1
static int parse_record(CHUNK* pc, const char* line)
{
    // init chunk
    pc->count = 0;
    pc->address = 0;
    pc->type = -1;

    // cut newline character
    unsigned length = strlen(line);
    if (length > 0 && line[length - 1] == '\n')
        --length;
    if (length > 0 && line[length - 1] == '\r')
        --length;

    // allow for empty lines and comments (non-standard feature)
    if (length == 0 || line[0] == ';')
        return 0;   // empty DATA
    // every line must start with colon
    if (line[0] != ':')
        return -1;

    // check number of characters
    if (length < MIN_LINE || length > MAX_LINE || !(length & 1))
        return -1;

    // convert line to byte array
    uint8_t blob[MAX_BYTES];
    size_t bloblen = 0;
    for (size_t i = 1; bloblen < MAX_BYTES; i += 2) {
        int high = char2hex(line[i]);
        if (high < 0)
            break;
        int low = char2hex(line[i + 1]);
        if (low < 0)
            break;
        blob[bloblen++] = (high << 4) | low;
    }
    if (bloblen != (length - 1) / 2)
        return -1;

    // get count, address and type
    unsigned count = blob[0];
    unsigned address = make16(blob[1], blob[2]);
    unsigned type = blob[3];
    if (count != bloblen - MIN_BYTES || address + count > 0x10000)
        return -1;

    // verify checksum
    uint8_t sum = 0;
    for (size_t i = 0; i < bloblen; ++i)
        sum += blob[i];
    if (sum != 0)
        return -1;

    pc->count = count;
    pc->address = address;
    pc->type = type;
    memcpy(pc->data, &blob[4], count);
    return pc->type;
}

// convert Intel HEX to Binary image
int ihx_load(IHX* ihx, unsigned filler, FILE* f)
{
    size_t segment = 0, blocksize = 0x10000;    // 64 KB
    size_t start = SIZE_MAX, end = 0, eip = 0;

    ihx->image = (uint8_t*)memset(z_malloc(blocksize), min(filler, 255), blocksize);
    ihx->sz = ihx->base = ihx->entry = 0;

    bool found_eof = false;
    do {
        char line[MAX_LINE + 3];    // CR+LF+NUL
        if (fgets(line, sizeof(line), f) == NULL)
            break;

        CHUNK chunk;
        switch (parse_record(&chunk, line)) {
        case 0: /* DATA */
            if (chunk.count > 0) {
                // parse_record() guarantees never getting past 64 KB
                memcpy(ihx->image + segment + chunk.address, chunk.data, chunk.count);
                start = min(start, segment + chunk.address);
                end = max(end, segment + chunk.address + chunk.count);
            }
        break;
        case 1: /* EOF */
            found_eof = (chunk.count == 0);
        break;
        case 2: /* CS */
        case 4: /* HIWORD(ADDRESS32) */
            if (chunk.count == 2) {
                segment = make16(chunk.data[0], chunk.data[1]);
                segment <<= (chunk.type == 2) ? 4 : 16;
                // grow image if less than 64 KB remaining
                if (segment + 0x10000 > blocksize) {
                    size_t newsize = segment + 0x100000; // +1 MB
                    ihx->image = (uint8_t*)z_realloc(ihx->image, newsize);
                    memset(ihx->image + blocksize, min(filler, 255),
                        newsize - blocksize);
                    blocksize = newsize;
                }
            }
        break;
        case 3: /* CS:IP */
        case 5: /* EIP */
            if (chunk.count == 4) {
                eip = make16(chunk.data[0], chunk.data[1]);
                eip <<= (chunk.type == 3) ? 4 : 16;
                eip += make16(chunk.data[2], chunk.data[3]);
            }
        break;
        case -1:
        default:
            // assume Binary file
            if (fseek(f, 0, SEEK_END) == 0) {
                long t = ftell(f);
                if (t > 0) {
                    fseek(f, 0, SEEK_SET);
                    ihx->image = (uint8_t*)z_realloc(ihx->image, t);
                    ihx->sz = fread(ihx->image, 1, t, f);
                    return 'b';
                }
            }
            free(ihx->image);
            ihx->image = NULL;
            return -1;
        break;
        }
    } while (!found_eof);

    if (start < end) {
        // rebase image
        if (start > 0)
            memmove(ihx->image, ihx->image + start, end - start);
        ihx->sz = end - start;
        ihx->base = start;
        ihx->entry = (start <= eip && eip < end) ? eip : start;
    }

    // shrink memory block
    ihx->image = (uint8_t*)z_realloc(ihx->image, ihx->sz);
    return 'x';
}

// format output as Intel HEX file
void ihx_dump(IHX* ihx, unsigned filler, unsigned wrap, FILE* f)
{
    size_t segment = ihx->base & 0xffff0000;    // over 64 KB
    bool use32 = (ihx->sz > 0x100000);          // size > 1 MB

    if (wrap == 0)
        wrap = 16;

    for (size_t i = 0; i < ihx->sz; ) {
        // segment overrun
        if (segment <= ihx->base + i) {
            // address output
            if (segment > 0) {
                unsigned type, high;
                if (use32) {
                    type = 4;   // HIWORD(ADDRESS32)
                    high = segment >> 16;
                } else {
                    type = 2;   // CS
                    high = segment >> 4;
                }
                int sum = 2 + type + sum8(high);
                fprintf(f, ":020000%02X%04X%02X\n", type, high, (uint8_t)(-sum));
            }
            segment += 0x10000; // +64 KB
        }

        // max number of bytes on line
        unsigned cb_max = segment - ihx->base - i;
        cb_max = min(cb_max, ihx->sz - i);
        cb_max = min(cb_max, wrap);

        // skip trailing bytes
        unsigned cb_line = cb_max;
        if (filler <= 255)
            for (; cb_line > 0; --cb_line)
                if (ihx->image[i + cb_line - 1] != filler)
                    break;

        if (cb_line > 0) {
            // : count address type(00)
            fprintf(f, ":%02X%04X00", cb_line, (uint16_t)(ihx->base + i));
            int sum = cb_line + sum8(ihx->base + i);
            // data
            for (unsigned j = 0; j < cb_line; ++j) {
                fprintf(f, "%02X", ihx->image[i + j]);
                sum += ihx->image[i + j];
            }
            // checksum
            fprintf(f, "%02X\n", (uint8_t)(-sum));
        }

        // advance index
        i += cb_max;
    }

    // start address
    if (ihx->entry > 0) {
        unsigned type, high;
        if (use32) {
            type = 5;
            high = ihx->entry >> 16;
        } else {
            type = 3;
            high = (ihx->entry & 0xf0000) >> 4;
        }
        int sum = 4 + type + sum8(high + ihx->entry);
        fprintf(f, ":040000%02X%04X%04X%02X\n", type, high, (uint16_t)ihx->entry,
            (uint8_t)(-sum));
    }

    // EOF record
    fputs(":00000001FF\n", f);
}
