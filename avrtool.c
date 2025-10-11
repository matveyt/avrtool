//
// avrtool
//
// AVRISP (aka. STK500v1) serial programmer
// Write HEX/BIN file to AVR/Arduino
//
// https://github.com/matveyt/avrtool
//

#include "stdz.h"
#include "ihx.h"
#include "isp.h"
#include "ucomm.h"

struct isp_device {
    uint32_t sig;       // Signature bytes
    bool cmdV;          // STK_UNIVERSAL supported
    size_t fsz, psz;    // Flash Size and Page Size
};

static bool at89s(uint32_t sig);
static size_t atmel_flashsize(uint32_t sig);
static size_t atmel_pagesize(uint32_t sig, size_t fsz);
static void isp_0(int ch, intptr_t fd);
static uint8_t isp_v(int b1, int b2, int b3, int b4, intptr_t fd);
static uint32_t isp_guess(struct isp_device* d, intptr_t fd);

// user options
static struct {
    char* file;
    char* port;
    unsigned baud;
    int erase;          // >0 erase, <0 no erase, =0 auto
    size_t base, size;  // new image base and size
    bool read, noreset;
    int fuse_mask;
    uint8_t fuse[4];    // low-high-extended-lock
} opt = {0};

/*noreturn*/
static void usage(int status)
{
    if (status != 0)
        fprintf(stderr, "Try '%s --help' for more information.\n", z_getprogname());
    else
        printf(
"Usage: %s [OPTION]... [FILE]\n"
"STK500v1 serial programmer. Write HEX/BIN file to AVR/Arduino.\n"
"\n"
"-p, --port=PORT    Select serial device\n"
"-b, --baud=BAUD    Transfer baud rate\n"
"-x, --erase        Always erase chip\n"
"-X, --noerase      Never erase chip\n"
"-a, --base=ADDR    Flash memory start address\n"
"-z, --size=NUM     Flash memory maximum size\n"
"-r, --read         Read memory to FILE\n"
"-n, --noreset      Do not assert DTR or RTS\n"
"    --lfuse=XX     Set low fuse\n"
"    --hfuse=XX     Set high fuse\n"
"    --efuse=XX     Set extended fuse\n"
"    --lock=XX      Set lock byte\n"
"-h, --help         Show this message and exit\n",
        z_getprogname());
    exit(status);
}

static void parse_args(int argc, char* argv[])
{
    z_setprogname(argv[0]);

    static struct z_option lopts[] = {
        { "port", z_required_argument, NULL, 'p' },
        { "baud", z_required_argument, NULL, 'b' },
        { "erase", z_no_argument, NULL, 'x' },
        { "noerase", z_no_argument, NULL, 'X' },
        { "base", z_required_argument, NULL, 'a' },
        { "size", z_required_argument, NULL, 'z' },
        { "read", z_no_argument, NULL, 'r' },
        { "noreset", z_no_argument, NULL, 'n' },
        { "lfuse", z_required_argument, NULL, 0 },
        { "hfuse", z_required_argument, NULL, 1 },
        { "efuse", z_required_argument, NULL, 2 },
        { "lock", z_required_argument, NULL, 3 },
        { "help", z_no_argument, NULL, 'h' },
        {0}
    };

    int c;
    while ((c = z_getopt_long(argc, argv, "p:b:xXa:z:rnh", lopts, NULL)) != -1) {
        switch (c) {
        case 'p':
            free(opt.port);
            opt.port = z_strdup(z_optarg);
        break;
        case 'b':
            opt.baud = strtoul(z_optarg, NULL, 10);
        break;
        case 'x':
            ++opt.erase;
        break;
        case 'X':
            --opt.erase;
        break;
        case 'a':
            opt.base = strtoul(z_optarg, NULL, 16);
        break;
        case 'z':
            opt.size = strtoul(z_optarg, NULL, 0);
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
            opt.fuse[c] = strtoul(z_optarg, NULL, 16);
        break;
        case 'h':
            usage(EXIT_SUCCESS);
        break;
        case '?':
            usage(EXIT_FAILURE);
        break;
        }
    }

    if (z_optind == argc - 1)
        opt.file = z_strdup(argv[z_optind]);
}

int main(int argc, char* argv[])
{
    opt.base = SIZE_MAX;    // not used
    opt.size = SIZE_MAX;
    parse_args(argc, argv);

    // ISP connection
    intptr_t isp = ucomm_open(opt.port, opt.baud, 0x801/*8-N-1*/);
    if (isp < 0) {
        if (opt.port != NULL)
            z_error(EXIT_FAILURE, errno, "ucomm_open(\"%s\")", opt.port);
        z_warnx(1, "missing port name");
        usage(EXIT_FAILURE);
    }
    free(opt.port);

    if (!opt.noreset) {
        // assert RTS then DTR (aka nodemcu reset)
        ucomm_rts(isp, 1);
        ucomm_dtr(isp, 1);
        ucomm_rts(isp, 0);
        ucomm_dtr(isp, 0);
    }

    // Wait for connect
    puts("Wait for connection...");
    do {
        (void)ucomm_getc(isp);  // delay
        // STK_GET_SYNC
    } while (isp_command('0', isp) != STK_OK);
    ucomm_purge(isp);

    // test if anything is attached
    struct isp_device d;
    if (isp_guess(&d, isp) == 0)
        z_error(EXIT_FAILURE, ENODEV, "isp_guess");

    isp_set_device(at89s(d.sig) ? 0xe1 : 0x86, d.fsz, d.psz, isp);
    isp_0('P', isp);

    printf("Device ID: 0x%x\n", d.sig);
    printf("Flash Memory: %zuKB,%zup,x%zu\n", d.fsz / 1024, d.fsz / d.psz, d.psz);
    printf("STK_UNIVERSAL: %s\n", d.cmdV ? "yes" : "no");

    // Show fuses
    if (d.cmdV) {
        if (at89s(d.sig)) {
            uint8_t lock = isp_v(0x24, 0, 0, 0, isp);
            printf("Lock=%x\n", lock);
        } else {
            uint8_t lfuse = isp_v(0x50, 0, 0, 0, isp);
            uint8_t hfuse = isp_v(0x58, 8, 0, 0, isp);
            uint8_t efuse = isp_v(0x50, 8, 0, 0, isp);
            uint8_t lock = isp_v(0x58, 0, 0, 0, isp);
            printf("Fuse=%x:%x:%x Lock=%x\n", lfuse, hfuse, efuse, lock);
        }
    }

    // Erase
    if (opt.erase > 0 || (opt.erase == 0 && opt.file != NULL && !opt.read)) {
        puts("Erase Chip");
        if (d.cmdV)
            isp_v(0xac, 0x80, 0, 0, isp);
        else
            isp_0('R', isp);
        (void)ucomm_getc(isp);  // delay >= 500 ms (AT89S)
        (void)ucomm_getc(isp);
    }

    // Read/Write
    if (opt.file != NULL) {
        // page align
        if (opt.base < d.fsz)
            opt.base &= ~(d.psz - 1);
        if (opt.size < d.fsz) {
            opt.size += d.psz - 1;
            opt.size &= ~(d.psz - 1);
        }

        FILE* f = z_fopen(opt.file, opt.read ? "w" : "rb");
        uint8_t* image;
        size_t sz, base;
        if (opt.read) {
            // Read Flash
            base = (opt.base < d.fsz) ? opt.base : 0;
            sz = min(opt.size, d.fsz - base);
            image = z_malloc(sz);
            printf("Read Flash[%zu] ", sz);
            for (size_t cnt = 0; cnt < sz; cnt += d.psz) {
                if (at89s(d.sig)) {
                    // reading AT89S in slow byte mode
                    for (size_t i = 0; i < d.psz; ++i) {
                        uint16_t addr = base + cnt + i;
                        image[cnt + i] = isp_v(0x20, addr >> 8, addr, 0, isp);
                    }
                } else {
                    // invoke STK_READ_PAGE
                    isp_load_address(base + cnt, isp);
                    if (isp_read_page(&image[cnt], d.psz, isp) != STK_OK)
                        z_error(EXIT_FAILURE, -1, "READ_PAGE 0x%zx", base + cnt);
                }
                putchar('#');
            }
            ihx_dump(image, sz, base, base, 0xff, 0, f);
        } else {
            // Write Flash
            if (ihx_load(&image, &sz, &base, &(size_t){0}, f) < 0)
                z_error(EXIT_FAILURE, errno, "ihx_load");
            // overwrite image base and size
            if (opt.base < d.fsz)
                base = opt.base;
            sz = min(sz, opt.size);
            if (base + sz > d.fsz)
                z_error(EXIT_FAILURE, EFBIG, "ihx_load");

            printf("Write Flash[%zu] ", sz);
            for (size_t cnt = 0; cnt < sz; cnt += d.psz) {
                size_t rest = min(d.psz, sz - cnt);
                if (at89s(d.sig)) {
                    // writing AT89S in slow byte mode
                    for (size_t i = 0; i < rest; ++i) {
                        uint16_t addr = base + cnt + i;
                        isp_v(0x40, addr >> 8, addr, image[cnt + i], isp);
                    }
                } else {
                    // invoke STK_PROG_PAGE
                    isp_load_address(base + cnt, isp);
                    if (isp_prog_page(&image[cnt], rest, isp) != STK_OK)
                        z_error(EXIT_FAILURE, -1, "PROG_PAGE 0x%zx", base + cnt);
                }
                putchar('#');
            }
        }
        putchar('\n');
        free(image);
        fclose(f);
    }

    if (opt.fuse_mask != 0) {
        if (!d.cmdV || at89s(d.sig))
            z_error(EXIT_FAILURE, -1, "Fuse write not supported");

        puts("Program Fuse");
        // low fuse
        if (opt.fuse_mask & 1)
            isp_v(0xac, 0xa0, 0, opt.fuse[0], isp);
        // high fuse
        if (opt.fuse_mask & 2)
            isp_v(0xac, 0xa8, 0, opt.fuse[1], isp);
        // extended fuse
        if (opt.fuse_mask & 4)
            isp_v(0xac, 0xa4, 0, opt.fuse[2], isp);
        // lock
        if (opt.fuse_mask & 8)
            isp_v(0xac, 0xe0, 0, opt.fuse[3], isp);
    }

    isp_0('Q', isp);
    ucomm_close(isp);
    exit(EXIT_SUCCESS);
}

// test if AT89S or AVR chip
bool at89s(uint32_t sig)
{
    return (sig & 0xf000) == 0x5000 || (sig & 0xf000) == 0x7000;
}

// Atmel Signature => Flash Size
size_t atmel_flashsize(uint32_t sig)
{
    unsigned nib2 = (sig >> 8) & 0xf;
    return at89s(sig) ? (nib2 << 12) : (1024U << nib2);
}

// Atmel Flash Size => Page Size
size_t atmel_pagesize(uint32_t sig, size_t fsz)
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
void isp_0(int ch, intptr_t fd)
{
    int resp = isp_command(ch, fd);
    if (resp != STK_OK)
        z_error(EXIT_FAILURE, -1, "For '%c' got response %d", ch, resp);
}

// AVRISP: universal command
uint8_t isp_v(int b1, int b2, int b3, int b4, intptr_t fd)
{
    uint8_t b_out;
    int resp = isp_universal(b1, b2, b3, b4, &b_out, fd);
    if (resp != STK_OK)
        z_error(EXIT_FAILURE, -1, "For 'V 0x%x 0x%x 0x%x 0x%x' got response %d",
            b1, b2, b3, b4, resp);
    return b_out;
}

// AVRISP: guess device parameters
uint32_t isp_guess(struct isp_device* d, intptr_t fd)
{
    d->sig = 0;
    isp_set_device(0x86, 32768, 128, fd);       // fake ATmega328P

    isp_0('P', fd);
    if (isp_read_sign(&d->sig, fd) == STK_OK) {
        // AVR chip found
        d->cmdV = (isp_v(0x30, 0, 0, 0, fd) == 0x1e);
    } else {
        // test for AT89S
        isp_0('Q', fd);
        isp_set_device(0xe1, 8192, 256, fd);    // fake AT89S52
        isp_0('P', fd);
        d->cmdV = (isp_v(0x28, 0, 0, 0, fd) == 0x1e);
        if (d->cmdV) {
            // read signature for AT89S directly
            // (e.g., "Arduino as ISP" can do STK_READ_SIGN for AVR only)
            uint8_t sig1 = isp_v(0x28, 1, 0, 0, fd);
            uint8_t sig2 = isp_v(0x28, 2, 0, 0, fd);
            d->sig = (0x1e << 16) | (sig1 << 8) | sig2;
        }
    }

    // don't leave from bootloader's progmode (or it'd go reboot)
    if (opt.noreset)
        isp_0('Q', fd);
    if ((d->sig >> 16) != 0x1e)
        return 0;
    d->fsz = atmel_flashsize(d->sig);
    d->psz = atmel_pagesize(d->sig, d->fsz);
    return d->sig;
}
