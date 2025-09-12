//
// uComm
// A very minimal cross-platform serial port library
//
// https://github.com/matveyt/ucomm
//

#include "ucomm.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__unix__)
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#if !defined(O_CLOEXEC)
#define O_CLOEXEC   0
#endif // O_CLOEXEC
#endif

static unsigned baudrate(unsigned baud)
{
#if defined(_WIN32)
    return baud ? baud : 115200;
#elif defined(__unix__)
    static const unsigned ubr[] = {
#define B(n)    (B##n), (n)
        B(50), B(75), B(110), B(134), B(150), B(200), B(300), B(600), B(1200), B(1800),
        B(2400), B(4800), B(9600), B(19200), B(38400), B(57600), B(115200),
        /*B(128000),*/ B(230400), /*B(256000),*/ B(460800), B(500000), B(576000),
        B(921600), B(1000000), B(1152000), B(1500000), B(2000000), B(2500000),
        B(3000000), /*B(3500000),*/ /*B(4000000),*/
#undef B
    };
    for (ssize_t i = sizeof(ubr) / sizeof(ubr[0]) - 1; i > 0; i -= 2)
        if (baud >= ubr[i])
            return ubr[i - 1];
    return B115200;
#endif
}

intptr_t ucomm_open(const char* port, unsigned baud, unsigned config)
{
    // config 0x801 => 8-N-1
    unsigned databits = (config >> 8) & 0x0f;   // 5..8
    unsigned parity = (config >> 4) & 0x0f;     // 0..2
    unsigned stopbits = (config) & 0x0f;        // 1..2

    if (databits < 5 || databits > 8)
        databits = 8;
    if (parity > 2)
        parity = 0;
    if (stopbits != 2)
        stopbits = 1;

#if defined(_WIN32)
    char fullname[11];  // \\\\.\\COMxxx
    if (port == NULL) {
        // arbitrary value
        port = "\\\\.\\COM3";
    } else if (lstrlenA(port) <= 6) {
        // COMx => \\\\.\\COMx
        lstrcpyA(fullname, "\\\\.\\");
        port = lstrcatA(fullname, port);
    }

    HANDLE fd = CreateFileA(port, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
        0, NULL);
    if (fd != INVALID_HANDLE_VALUE) {
        DCB dcb = {
            .DCBlength = sizeof(DCB),
            .BaudRate = baudrate(baud),
            .fBinary = 1,
            .fParity = !!parity,
            .fDtrControl = DTR_CONTROL_DISABLE,
            .fRtsControl = RTS_CONTROL_DISABLE,
            .ByteSize = databits,
            .Parity = (parity == 0) ? NOPARITY : (parity == 1) ? ODDPARITY : EVENPARITY,
            .StopBits = (stopbits == 1) ? ONESTOPBIT : TWOSTOPBITS,
            .XonLim = 256,
            .XoffLim = 256,
        };
        SetupComm(fd, 1024, 1024);
        SetCommState(fd, &dcb);
        ucomm_timeout((intptr_t)fd, 300);
    }
#elif defined(__unix__)
    // arbitrary value
    if (port == NULL)
        port = "/dev/ttyUSB0";

    int fd = open(port, O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (fd != -1) {
        struct termios tio;
        tcgetattr(fd, &tio);

        //cfmakeraw(&tio);
        tio.c_iflag &= ~(IGNBRK | BRKINT | IGNPAR | INPCK | ISTRIP | INLCR | IGNCR |
            ICRNL | IXON | PARMRK);
        tio.c_oflag &= ~(OPOST);
        tio.c_cflag &= ~(CSIZE | CSTOPB | PARENB | PARODD);
        tio.c_lflag &= ~(ISIG | ICANON | ECHO | ECHONL | IEXTEN);
        switch (databits) {
        case 5: tio.c_cflag |= CS5; break;
        case 6: tio.c_cflag |= CS6; break;
        case 7: tio.c_cflag |= CS7; break;
        case 8: tio.c_cflag |= CS8; break;
        }
        if (parity != 0) {
            tio.c_iflag |= INPCK;
            tio.c_cflag |= (parity == 1) ? (PARENB | PARODD) : PARENB;
        }
        tio.c_cflag |= (stopbits == 2) ? CSTOPB : 0;
        tio.c_cflag |= (CREAD | CLOCAL);
        tio.c_cc[VMIN] = 0;
        tio.c_cc[VTIME] = 3;    // 300 ms
        speed_t ubr = baudrate(baud);
        cfsetispeed(&tio, ubr);
        cfsetospeed(&tio, ubr);
        tcsetattr(fd, TCSANOW, &tio);
    }
#endif

    return (intptr_t)fd;
}

int ucomm_close(intptr_t fd)
{
#if defined(_WIN32)
    return CloseHandle((HANDLE)fd) ? 0 : -1;
#elif defined(__unix__)
    return close(fd);
#endif
}

int ucomm_timeout(intptr_t fd, unsigned ms)
{
#if defined(_WIN32)
    COMMTIMEOUTS timeouts = {
        .ReadIntervalTimeout = ms ? ms : MAXDWORD,
        .ReadTotalTimeoutConstant = ms,
        .ReadTotalTimeoutMultiplier = 0,
        .WriteTotalTimeoutConstant = ms,
        .WriteTotalTimeoutMultiplier = 0,
    };
    return SetCommTimeouts((HANDLE)fd, &timeouts) ? 0 : -1;
#elif defined(__unix__)
    struct termios tio;
    tcgetattr(fd, &tio);
    tio.c_cc[VTIME] = (ms / 100) + !!(ms % 100);
    return tcsetattr(fd, TCSANOW, &tio);
#endif
}

int ucomm_purge(intptr_t fd)
{
#if defined(_WIN32)
    return PurgeComm((HANDLE)fd, PURGE_RXCLEAR | PURGE_TXCLEAR) ? 0 : -1;
#elif defined(__unix__)
    return tcflush(fd, TCIOFLUSH);
#endif
}

int ucomm_dtr(intptr_t fd, int pulldown)
{
#if defined(_WIN32)
    return EscapeCommFunction((HANDLE)fd, pulldown ? SETDTR : CLRDTR) ? 0 : - 1;
#elif defined(__unix__)
    return ioctl(fd, pulldown ? TIOCMBIS : TIOCMBIC, &(int){TIOCM_DTR});
#endif
}

int ucomm_rts(intptr_t fd, int pulldown)
{
#if defined(_WIN32)
    return EscapeCommFunction((HANDLE)fd, pulldown ? SETRTS : CLRRTS) ? 0 : -1;
#elif defined(__unix__)
    return ioctl(fd, pulldown ? TIOCMBIS : TIOCMBIC, &(int){TIOCM_RTS});
#endif
}

ssize_t ucomm_available(intptr_t fd)
{
#if defined(_WIN32)
    COMSTAT stat;
    return ClearCommError((HANDLE)fd, NULL, &stat) ? (LONG)stat.cbInQue : -1;
#elif defined(__unix__)
    int available = -1;
    ioctl(fd, TIOCINQ, &available);
    return available;
#endif
}

int ucomm_getc(intptr_t fd)
{
    uint8_t b;
#if defined(_WIN32)
    DWORD part;
    ReadFile((HANDLE)fd, &b, sizeof(b), &part, NULL);
#elif defined(__unix__)
    ssize_t part = read(fd, &b, sizeof(b));
#endif
    return (part == sizeof(b)) ? (int)b : -1;
}

int ucomm_putc(intptr_t fd, int ch)
{
    uint8_t b = (uint8_t)ch;
#if defined(_WIN32)
    DWORD part;
    WriteFile((HANDLE)fd, &b, sizeof(b), &part, NULL);
#elif defined(__unix__)
    ssize_t part = write(fd, &b, sizeof(b));
#endif
    return (part == sizeof(b)) ? (int)b : -1;
}

ssize_t ucomm_read(intptr_t fd, void* buffer, size_t length)
{
    ssize_t sz = 0;
    while (sz < (ssize_t)length) {
#if defined(_WIN32)
        DWORD part;
        BOOL success = ReadFile((HANDLE)fd, (uint8_t*)buffer + sz, length - sz, &part,
            NULL);
#elif defined(__unix__)
        ssize_t part = read(fd, (uint8_t*)buffer + sz, length - sz);
        int success = (part >= 0);
#endif
        if (!success && sz <= 0)
            return -1;
        if (part <= 0)
            break;
        sz += part;
    }
    return sz;
}

ssize_t ucomm_write(intptr_t fd, const void* buffer, size_t length)
{
    ssize_t sz = 0;
    while (sz < (ssize_t)length) {
#if defined(_WIN32)
        DWORD part;
        BOOL success = WriteFile((HANDLE)fd, (uint8_t*)buffer + sz, length - sz, &part,
            NULL);
#elif defined(__unix__)
        ssize_t part = write(fd, (uint8_t*)buffer + sz, length - sz);
        int success = (part >= 0);
#endif
        if (!success && sz <= 0)
            return -1;
        if (part <= 0)
            break;
        sz += part;
    }
    return sz;
}
