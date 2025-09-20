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
#include "ihx.h"
#include "isp.h"
#include "ucomm.h"

static const char program_name[] = "avrtool";

// user options
static struct {
    char* file;
    char* port;
    unsigned baud;
    int erase;          // >0 erase, <0 no erase, =0 auto
    size_t base;        // new image base
    bool read, noreset;
    int fuse_mask;
    uint8_t fuse[4];    // low-high-extended-lock
} opt = {0};

/*noreturn*/
static void help(void)
{
    fprintf(stdout,
"Usage: %s [OPTION]... [FILE]\n"
"STK500v1 serial programmer. Write HEX/BIN file to AVR/Arduino.\n"
"\n"
"-p, --port=PORT    Select serial device\n"
"-b, --baud=BAUD    Transfer baud rate\n"
"-x, --erase        Always erase chip\n"
"-X, --noerase      Never erase chip\n"
"-a, --base=ADDR    First page address to read/write\n"
"-r, --read         Read memory to FILE\n"
"-n, --noreset      Do not assert DTR or RTS\n"
"    --lfuse=XX     Set low fuse\n"
"    --hfuse=XX     Set high fuse\n"
"    --efuse=XX     Set extended fuse\n"
"    --lock=XX      Set lock byte\n"
"-h, --help         Show this message and exit\n",
        program_name);
    exit(EXIT_SUCCESS);
}

static void parse_args(int argc, char* argv[])
{
    static struct option lopts[] = {
        { "port", required_argument, NULL, 'p' },
        { "baud", required_argument, NULL, 'b' },
        { "erase", no_argument, NULL, 'x' },
        { "noerase", no_argument, NULL, 'X' },
        { "base", required_argument, NULL, 'a' },
        { "read", no_argument, NULL, 'r' },
        { "noreset", no_argument, NULL, 'n' },
        { "lfuse", required_argument, NULL, 0 },
        { "hfuse", required_argument, NULL, 1 },
        { "efuse", required_argument, NULL, 2 },
        { "lock", required_argument, NULL, 3 },
        { "help", no_argument, NULL, 'h' },
        {0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "p:b:xXa:rnh", lopts, NULL)) != -1) {
        switch (c) {
        case 'p':
            free(opt.port);
            opt.port = z_strdup(optarg);
        break;
        case 'b':
            opt.baud = strtoul(optarg, NULL, 10);
        break;
        case 'x':
            ++opt.erase;
        break;
        case 'X':
            --opt.erase;
        break;
        case 'a':
            opt.base = strtoul(optarg, NULL, 16);
        break;
        case 'r':
            opt.read = true;
        break;
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

// test if AT89S or AVR chip
static bool at89s(uint32_t sig)
{
    return (sig & 0xf000) == 0x5000 || (sig & 0xf000) == 0x7000;
}

// Atmel Signature => Flash Size
static size_t atmel_flashsize(uint32_t sig)
{
    unsigned nib2 = (sig >> 8) & 0xf;
    return at89s(sig) ? (nib2 << 12) : (1024U << nib2);
}

// Atmel Flash Size => Page Size
static size_t atmel_pagesize(uint32_t sig, size_t fsz)
{
    if ((sig & 0xf000) == 0x5000)
        return 256;
    if ((sig & 0xf000) == 0x7000)
        return 64;
    if (fsz <= 2048)
        return 32;
    if (fsz <= 8192)
        return 64;
    if (fsz <= 32768)
        return 128;
    return 256;
}

// AVRISP: simple command
static void isp_0(const char* what, int ch, intptr_t fd)
{
    if (isp_command(ch, fd) != STK_OK)
        z_die(what);
}

// AVRISP: universal command
static uint8_t isp_v(const char* what, int b1, int b2, int b3, int b4, intptr_t fd)
{
    uint8_t b_out;
    if (isp_universal(b1, b2, b3, b4, &b_out, fd) != STK_OK)
        z_die(what);
    return b_out;
}

// AVRISP: guess device parameters
struct device {
    uint32_t sig;       // Signature bytes
    bool cmdV;          // STK_UNIVERSAL supported
    size_t fsz, psz;    // Flash Size and Page Size
};
static void isp_guess(struct device* d, intptr_t fd)
{
    d->sig = 0;
    isp_set_device(0x86, 32768, 128, fd);       // fake ATmega328P

    isp_0("ENTER_PROGMODE", 'P', fd);
    if (isp_read_sign(&d->sig, fd) == STK_OK) {
        // AVR chip found
        d->cmdV = (isp_v("V_READ_SIGN", 0x30, 0, 0, 0, fd) == 0x1e);
    } else {
        // test for AT89S
        isp_0("LEAVE_PROGMODE", 'Q', fd);
        isp_set_device(0xe1, 8192, 256, fd);    // fake AT89S52
        isp_0("ENTER_PROGMODE", 'P', fd);
        d->cmdV = (isp_v("V_READ_SIGN", 0x28, 0, 0, 0, fd) == 0x1e);
        if (d->cmdV) {
            // read signature for AT89S directly
            // (e.g., "Arduino as ISP" can do STK_READ_SIGN for AVR only)
            uint8_t sig1 = isp_v("V_READ_SIGN", 0x28, 1, 0, 0, fd);
            uint8_t sig2 = isp_v("V_READ_SIGN", 0x28, 2, 0, 0, fd);
            d->sig = (0x1e << 16) | (sig1 << 8) | sig2;
        }
    }

    // don't leave from bootloader's progmode (or it'd go reboot)
    if (opt.noreset)
        isp_0("LEAVE_PROGMODE", 'Q', fd);

    if (d->sig != 0) {
        d->fsz = atmel_flashsize(d->sig);
        d->psz = atmel_pagesize(d->sig, d->fsz);
    }
}

int main(int argc, char* argv[])
{
    opt.base = SIZE_MAX;    // invalid value
    parse_args(argc, argv);

    // ISP connection
    intptr_t isp = ucomm_open(opt.port, opt.baud, 0x801/*8-N-1*/);
    if (isp < 0) {
        if (opt.port == NULL)
            help();
        z_die("ucomm_open");
    }
    free(opt.port);

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
    } while (isp_command('0', isp) != STK_OK);
    ucomm_purge(isp);

    // test if anything is attached
    struct device d;
    isp_guess(&d, isp);
    if (d.sig == 0) {
        errno = ENODEV;
        z_die("isp_guess");
    }

    isp_set_device(at89s(d.sig) ? 0xe1 : 0x86, d.fsz, d.psz, isp);
    isp_0("ENTER_PROGMODE", 'P', isp);

    printf("Device ID: 0x%x\n", d.sig);
    printf("Flash Memory: %zuKB,%zup,x%zu\n", d.fsz / 1024, d.fsz / d.psz, d.psz);
    printf("STK_UNIVERSAL: %s\n", d.cmdV ? "yes" : "no");

    // Show fuses
    if (d.cmdV) {
        if (at89s(d.sig)) {
            uint8_t lock = isp_v("V_FUSE", 0x24, 0, 0, 0, isp);
            printf("Lock=%x\n", lock);
        } else {
            uint8_t lfuse = isp_v("V_FUSE", 0x50, 0, 0, 0, isp);
            uint8_t hfuse = isp_v("V_FUSE", 0x58, 8, 0, 0, isp);
            uint8_t efuse = isp_v("V_FUSE", 0x50, 8, 0, 0, isp);
            uint8_t lock = isp_v("V_FUSE", 0x58, 0, 0, 0, isp);
            printf("Fuse=%x:%x:%x Lock=%x\n", lfuse, hfuse, efuse, lock);
        }
    }

    // Erase
    if (opt.erase > 0 || (opt.erase == 0 && opt.file != NULL && !opt.read)) {
        puts("Erase Chip");
        if (d.cmdV)
            isp_v("V_CHIP_ERASE", 0xac, 0x80, 0, 0, isp);
        else
            isp_0("CHIP_ERASE", 'R', isp);
        (void)ucomm_getc(isp);  // delay >= 500 ms (AT89S)
        (void)ucomm_getc(isp);
    }

    // Read/Write
    if (opt.file != NULL) {
        opt.base &= ~(d.psz - 1); // page align

        FILE* f = z_fopen(opt.file, opt.read ? "w" : "rb");
        uint8_t* image;
        size_t sz, base;
        if (opt.read) {
            // Read Flash
            base = (opt.base < d.fsz) ? opt.base : 0;
            sz = d.fsz - base;
            image = z_malloc(sz);
            printf("Read Flash[%zu] ", sz);
            for (size_t cnt = 0; cnt < sz; cnt += d.psz) {
                if (at89s(d.sig)) {
                    // reading AT89S in slow byte mode
                    for (size_t i = 0; i < d.psz; ++i) {
                        uint16_t addr = base + cnt + i;
                        image[cnt + i] = isp_v("V_READ_FLASH", 0x20, addr >> 8, addr, 0,
                            isp);
                    }
                } else {
                    // invoke STK_READ_PAGE
                    isp_load_address(base + cnt, isp);
                    if (isp_read_page(&image[cnt], d.psz, isp) != STK_OK)
                        z_die("READ_PAGE");
                }
                putchar('#');
            }
            ihx_dump(image, sz, base, base, 0xff, 0, f);
        } else {
            // Write Flash
            if (ihx_load(&image, &sz, &base, &(size_t){0}, f) < 0) {
                errno = ENOEXEC;
                z_die("ihx_load");
            }
            // overwrite image base
            if (opt.base < d.fsz)
                base = opt.base;
            if (base + sz > d.fsz) {
                errno = EFBIG;
                z_die("ihx_load");
            }

            printf("Write Flash[%zu] ", sz);
            for (size_t cnt = 0; cnt < sz; cnt += d.psz) {
                size_t rest = min(d.psz, sz - cnt);
                if (at89s(d.sig)) {
                    // writing AT89S in slow byte mode
                    for (size_t i = 0; i < rest; ++i) {
                        uint16_t addr = base + cnt + i;
                        isp_v("V_PROG_FLASH", 0x40, addr >> 8, addr, image[cnt + i],
                            isp);
                    }
                } else {
                    // invoke STK_PROG_PAGE
                    isp_load_address(base + cnt, isp);
                    if (isp_prog_page(&image[cnt], rest, isp) != STK_OK)
                        z_die("PROG_PAGE");
                }
                putchar('#');
            }
        }
        putchar('\n');
        free(image);
        fclose(f);
    }

    if (opt.fuse_mask != 0) {
        if (!d.cmdV || at89s(d.sig)) {
            errno = ENOSYS;
            z_die("V_FUSE");
        }

        puts("Program Fuse");
        // low fuse
        if (opt.fuse_mask & 1)
            isp_v("V_FUSE", 0xac, 0xa0, 0, opt.fuse[0], isp);
        // high fuse
        if (opt.fuse_mask & 2)
            isp_v("V_FUSE", 0xac, 0xa8, 0, opt.fuse[1], isp);
        // extended fuse
        if (opt.fuse_mask & 4)
            isp_v("V_FUSE", 0xac, 0xa4, 0, opt.fuse[2], isp);
        // lock
        if (opt.fuse_mask & 8)
            isp_v("V_FUSE", 0xac, 0xe0, 0, opt.fuse[3], isp);
    }

    isp_0("LEAVE_PROGMODE", 'Q', isp);
    ucomm_close(isp);
    exit(EXIT_SUCCESS);
}
