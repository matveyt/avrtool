### What is this

Avrtool is a serial programmer for AVR/Arduino using STK500v1 protocol. It works with
standard Arduino bootloader as well as "Arduino as ISP" programmer.

So it is sort of "replacement" for AVRDUDE with (too) little features yet easier to use.

Notes:

* Input file format (Intel HEX or Binary) is auto-detected
* To save firmware pass `--read` option
* Default serial port is `/dev/ttyUSB0` (`COM3` on Windows)
* Default port speed is 115200 bps (except for `--noreset`, it is 19200 bps)
* If MCU does not respond try manual baud setting (e.g., 57600 bps for LGT8F series)
* Automatic chip reset asserts both DTR and RTS
* Avrtool waits for connection indefinitely; press Ctrl-C to exit
* For "Arduino as ISP" `--noreset` option is required
* While reading chip any empty byte sequence (i.e., 0xff) may be removed from output
* Passing `--size` option may significantly speed up read operation
* Bootloaders may fake some commands (STK\_CHIP\_ERASE is no-op, STK\_READ\_SIGN returns
  arbitrary value, etc.)
* Bootloader may overwrite itself and become non-functional; use `--size` option to set
  upper memory limit and prevent this
* Fuses are supported only if STK\_UNIVERSAL command works
* AT89S chips are programmable by "Arduino as ISP"

### Build

If using GCC then simply run `make`. Otherwise, you may need to setup different compile
flags. The source code itself is thought to be C99 portable.

### Use

```
Usage: avrtool [OPTION]... [FILE]
STK500v1 serial programmer. Write HEX/BIN file to AVR/Arduino.

-p, --port=PORT    Select serial device
-b, --baud=BAUD    Transfer baud rate
-x, --erase        Always erase chip
-X, --noerase      Never erase chip
-a, --base=ADDR    Flash memory start address
-z, --size=NUM     Flash memory maximum size
-r, --read         Read memory to FILE
-n, --noreset      Do not assert DTR or RTS
    --lfuse=X      Set low fuse
    --hfuse=X      Set high fuse
    --efuse=X      Set extended fuse
    --lock=X       Set lock byte
-l, --list-ports   List available ports only
-h, --help         Show this message and exit
```
