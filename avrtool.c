//
// avrtool
//
// AVRISP (aka. STK500v1) serial programmer
// Write HEX/BIN file to AVR/Arduino
//
// https://github.com/matveyt/avrtool
//

#include "stdz.h"
#include <errno.h>
#include <getopt.h>
#include "ihex.h"
#include "isp.h"
#include "ucomm.h"

const char program_name[] = "avrtool";

// user options
struct {
    char* file;
    char* port;
    unsigned baud;
    bool erase, read, noreset, at89s;
    int fuse_mask;
    uint8_t fuse[4];    // low-high-extended-lock
} opt = {0};

/*noreturn*/
void help(void)
{
    fprintf(stdout,
"Usage: %s [OPTION]... [FILE]\n"
"AVRISP serial programmer. Write HEX/BIN file to AVR/Arduino.\n"
"\n"
"-p, --port=PORT    Select serial device\n"
"-b, --baud=BAUD    Transfer baud rate\n"
"-x, --erase        Erase chip first\n"
"-r, --read         Read flash to FILE\n"
"-n, --noreset      Do not assert DTR or RTS\n"
"    --at89s        Target AT89Sxx series\n"
"    --lfuse=XX     Set low fuse\n"
"    --hfuse=XX     Set high fuse\n"
"    --efuse=XX     Set extended fuse\n"
"    --lock=XX      Set lock byte\n"
"-h, --help         Show this message and exit\n",
        program_name);
    exit(EXIT_SUCCESS);
}

void parse_args(int argc, char* argv[])
{
    static struct option lopts[] = {
        { "port", required_argument, NULL, 'p' },
        { "baud", required_argument, NULL, 'b' },
        { "erase", no_argument, NULL, 'x' },
        { "read", no_argument, NULL, 'r' },
        { "noreset", no_argument, NULL, 'n' },
        { "at89s", no_argument, NULL, 9 },
        { "lfuse", required_argument, NULL, 0 },
        { "hfuse", required_argument, NULL, 1 },
        { "efuse", required_argument, NULL, 2 },
        { "lock", required_argument, NULL, 3 },
        { "help", no_argument, NULL, 'h' },
        {0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "p:b:xrnh", lopts, NULL)) != -1) {
        switch (c) {
        case 'p':
            free(opt.port);
            opt.port = z_strdup(optarg);
        break;
        case 'b':
            opt.baud = strtoul(optarg, NULL, 10);
        break;
        case 'x':
            opt.erase = true;
        break;
        case 'r':
            opt.read = true;
        break;
        case 9:
            opt.at89s = true;
            __attribute__((fallthrough));
            // require also opt.noreset
        case 'n':
            opt.noreset = true;
            if (opt.baud == 0)
                opt.baud = 19200;
        break;
        case 0:
        case 1:
        case 2:
        case 3:
            opt.fuse_mask |= 1 << c;
            opt.fuse[c] = strtoul(optarg, NULL, 16);
        break;
        case 'h':
            help();
        break;
        case '?':
            fprintf(stderr, "Try '%s --help' for more information.\n", program_name);
            exit(EXIT_FAILURE);
        break;
        }
    }

    if (optind == argc - 1)
        opt.file = z_strdup(argv[optind]);
}

// format image as Intel HEX file
void dump_hex(uint8_t* image, size_t sz, FILE* f)
{
    const unsigned wrap = 16;

    for (size_t i = 0; i < sz; i += wrap) {
        unsigned cb = min(wrap, sz - i);
        // : count address type(00)
        fprintf(f, ":%02X%04zX00", cb, i);
        unsigned sum = cb + (i >> 8) + i;
        // data
        for (unsigned j = 0; j < cb; ++j) {
            fprintf(f, "%02X", image[i + j]);
            sum += image[i + j];
        }
        // checksum
        fprintf(f, "%02X\n", (uint8_t)(-sum));
    }

    // EOF record
    fputs(":00000001FF\n", f);
}

// Atmel ID => Flash Size
size_t atmel_flashsize(uint32_t id)
{
    unsigned nib2 = (id >> 8) & 0xf;
    return ((id & 0xf000) == 0x5000) ? (nib2 << 12) : (1024U << nib2);
}

// Atmel Flash Size => Page Size
size_t atmel_pagesize(uint32_t id, size_t fsz)
{
    // AT89Sxx
    if ((id & 0xf000) == 0x5000)
        return 256;
    if (fsz <= 2048)
        return 32;
    if (fsz <= 8192)
        return 64;
    if (fsz <= 32768)
        return 128;
    return 256;
}

int main(int argc, char* argv[])
{
    parse_args(argc, argv);

    // UART connection
    uint8_t buffer[4];
    intptr_t isp = ucomm_open(opt.port, opt.baud, 0x801/*8-N-1*/);
    free(opt.port);
    if (isp < 0)
        z_die("ucomm_open");

    if (!opt.noreset) {
        // assert RTS then DTR (aka. nodemcu reset)
        ucomm_rts(isp, 1);
        ucomm_dtr(isp, 1);
        ucomm_rts(isp, 0);
        ucomm_dtr(isp, 0);
    }

    // Wait for connect
    puts("Wait for connection...");
    do {
        // STK_GET_SYNC
    } while (isp_command('0', NULL, 0, isp) != STK_OK);
    ucomm_purge(isp);

    // fake AT89S52 -- let programmer know Reset is active high
    if (opt.at89s)
        isp_set_device(0xe1, 256, 0, 8192, isp);

    // Chip Info
    uint32_t did;
    size_t fsz, psz;
    bool cmd_V;

#define ISP(code, ch, cb)                                           \
    do {                                                            \
        if (isp_command(ch, buffer, cb, isp) != STK_OK)             \
            z_die(#code);                                           \
    } while (0)
#define ISP_V(b1, b2, b3, b4)                                       \
    do {                                                            \
        if (isp_universal(b1, b2, b3, b4, buffer, isp) != STK_OK)   \
            z_die("UNIVERSAL");                                     \
    } while (0)

    ISP(ENTER_PROGMODE, 'P', 0);

    ISP(READ_SIGN, 'u', 3);
    if (buffer[0] == 0 || buffer[0] == UINT8_MAX) {
        errno = ENODEV;
        z_die("READ_SIGN");
    }
    did = (buffer[0] << 16) | (buffer[1] << 8) | (buffer[2]);
    fsz = atmel_flashsize(did);
    psz = atmel_pagesize(did, fsz);

    ISP_V(0x30, 0, 0, 0);               // READ_SIGN[0]
    cmd_V = (buffer[0] == (did >> 16)); // 0x1e Atmel

    printf("Device ID: 0x%x\n", did);
    printf("Flash Memory: %zuKB,%zup,x%zu\n", fsz / 1024, fsz / psz, psz);
    printf("STK_UNIVERSAL: %s\n", cmd_V ? "yes" : "no");

    // Show fuses
    if (cmd_V && !opt.at89s) {
        uint8_t lfuse, hfuse, efuse, lock;
        ISP_V(0x50, 0, 0, 0);
        lfuse = buffer[0];
        ISP_V(0x58, 8, 0, 0);
        hfuse = buffer[0];
        ISP_V(0x50, 8, 0, 0);
        efuse = buffer[0];
        ISP_V(0x58, 0, 0, 0);
        lock = buffer[0];
        printf("Fuse=%x:%x:%x Lock=%x\n", lfuse, hfuse, efuse, lock);
    }

    // Erase
    if (opt.erase) {
        ucomm_timeout(isp, 3000);
        puts("Erase Chip");
        if (cmd_V)
            ISP_V(0xac, 0x80, 0, 0);
        else
            ISP(CHIP_ERASE, 'R', 0);
        ucomm_timeout(isp, 300);
    }

    // Read/Write
    if (opt.file != NULL) {
        FILE* f = z_fopen(opt.file, opt.read ? "w" : "rb");
        uint8_t* image;
        if (opt.read) {
            image = z_malloc(fsz);
            printf("Read Flash[%zu]\n", fsz);
            for (size_t cnt = 0; cnt < fsz; cnt += psz) {
                isp_load_address(cnt, isp);
                if (isp_read_page(&image[cnt], min(psz, fsz - cnt), isp) != STK_OK)
                    z_die("READ_PAGE");
            }
            dump_hex(image, fsz, f);
        } else {
            size_t sz, base, entry;
            image = ihex_load8(&sz, &base, &entry, f);
            if (image == NULL) {
                errno = ENOEXEC;
                z_die("ihex");
            }
            if (base + sz > fsz) {
                errno = EFBIG;
                z_die("ihex");
            }

            printf("Write Flash[%zu]\n", sz);
            for (size_t cnt = 0; cnt < sz; cnt += psz) {
                isp_load_address(base + cnt, isp);
                if (isp_prog_page(&image[cnt], min(psz, sz - cnt), isp) != STK_OK)
                    z_die("PROG_PAGE");
            }
        }
        free(image);
        fclose(f);
    }

    if (opt.fuse_mask != 0) {
        if (!cmd_V || opt.at89s) {
            errno = ENOTSUP;
            z_die("FUSE");
        }
        // low fuse
        if (opt.fuse_mask & 1)
            ISP_V(0xac, 0xa0, 0, opt.fuse[0]);
        // high fuse
        if (opt.fuse_mask & 2)
            ISP_V(0xac, 0xa8, 0, opt.fuse[1]);
        // extended fuse
        if (opt.fuse_mask & 4)
            ISP_V(0xac, 0xa4, 0, opt.fuse[2]);
        // lock
        if (opt.fuse_mask & 8)
            ISP_V(0xac, 0xe0, 0, opt.fuse[3]);
    }

    ISP(LEAVE_PROGMODE, 'Q', 0);
    ucomm_close(isp);
    exit(EXIT_SUCCESS);
}
