#include "ihex.h"
#include "stdz.h"

#define MAX_IMAGE   (UINT16_MAX + 1)
#define MIN_BYTES   5
#define MAX_BYTES   (MIN_BYTES + UINT8_MAX)
#define MIN_LINE    (1 + 2 * MIN_BYTES)
#define MAX_LINE    (1 + 2 * MAX_BYTES)

// parsed record
typedef struct {
    uint8_t data[UINT8_MAX];
    unsigned length;
    size_t address;
} CHUNK;

// print annoying message
static void warn(unsigned lineno, const char* what)
{
    fprintf(stderr, "ihex: %s on line %u\n", what, lineno);
}

// parse one record
// return record type or -1
static int parse_record(CHUNK* pc, char* line)
{
    // init chunk
    pc->length = 0;
    pc->address = 0;

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
    size_t bloblen = ihex_blob(blob, MAX_BYTES, &line[1]);
    if (bloblen != (length - 1) / 2)
        return -1;

    // get count, address and type
    unsigned count = blob[0];
    unsigned address = (blob[1] << 8) + (blob[2]);
    unsigned type = blob[3];
    if (count != bloblen - MIN_BYTES || address + count > MAX_IMAGE)
        return -1;

    // verify checksum
    uint8_t sum = 0;
    for (size_t i = 0; i < bloblen; ++i)
        sum += blob[i];
    if (sum != 0)
        return -1;

    memcpy(pc->data, &blob[4], count);
    pc->length = count;
    pc->address = address;
    return (int)type;
}

// convert Intel HEX to Binary
uint8_t* ihex_load8(size_t* sz, size_t* base, size_t* entry, FILE* f)
{
    *sz = *base = *entry = 0;

    uint8_t* image = memset(z_malloc(MAX_IMAGE), UINT8_MAX, MAX_IMAGE);
    size_t start = MAX_IMAGE, end = 0, ip = 0;

    bool found_eof = false;
    for (unsigned lineno = 1; !found_eof; ++lineno) {
        char line[MAX_LINE + 3];    // CR+LF+NUL
        if (fgets(line, sizeof(line), f) == NULL) {
            warn(lineno, "no EOF record");
            break;
        }

        CHUNK chunk;
        switch (parse_record(&chunk, line)) {
        case 0: /* DATA */
            if (chunk.length > 0) {
                // parse_record() guarantees never getting past MAX_IMAGE
                memcpy(image + chunk.address, chunk.data, chunk.length);
                start = min(start, chunk.address);
                end = max(end, chunk.address + chunk.length);
            }
        break;
        case 1: /* EOF */
            if (chunk.length > 0)
                warn(lineno, "wrong record length");
            else
                found_eof = true;
        break;
        case 3: /* CS:IP */
        case 5: /* EIP */
            if (chunk.length != 4) {
                warn(lineno, "wrong record length");
            } else {
                if (chunk.data[0] != 0 || chunk.data[1] != 0)
                    warn(lineno, "invalid entry point");
                else
                    ip = (chunk.data[2] << 8) + (chunk.data[3]);
            }
        break;
        case 2: /* CS */
        case 4: /* HIWORD(ADDRESS32) */
            warn(lineno, "unsupported record type");
        break;
        case -1:
        default:
            // assume BIN file
            start = end = ip = 0;
            long t = fseek(f, 0, SEEK_END) ? -1L : ftell(f);
            if (t > 0) {
                rewind(f);
                end = fread(image, 1, min(t, MAX_IMAGE), f);
            }
            found_eof = true;
        break;
        }
    }

    if (start < end) {
        // rebase image
        if (start > 0)
            memmove(image, image + start, end - start);
        *sz = end - start;
        *base = start;
        if (start <= ip && ip < end)
            *entry = ip;
        else
            *entry = start;
    }

    // shrink memory block
    return z_realloc(image, *sz);
}

// convert char to hex number
static int char2hex(int ch)
{
    if (ch < '0')
        return -1;
    if (ch <= '9')
        return ch - '0';
    if (ch < 'A')
        return -1;
    if (ch <= 'F')
        return ch - 'A' + 10;
    if (ch < 'a')
        return -1;
    if (ch <= 'f')
        return ch - 'a' + 10;
    return -1;
}

// convert hex string to byte array
// return number of bytes converted
size_t ihex_blob(uint8_t* blob, size_t sz, const char* str)
{
    for (size_t i = 0, j = 0; i < sz; ++i, j += 2) {
        int hi = char2hex(str[j]);
        int lo = char2hex(str[j + 1]);
        if (hi < 0 || lo < 0)
            return i;
        blob[i] = (hi << 4) | lo;
    }
    return sz;
}
