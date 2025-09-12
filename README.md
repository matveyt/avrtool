### What is this

AVRISP (aka. STK500v1) serial programmer for AVR/Arduino. Works with standard Arduino
bootloader as well as "Arduino as ISP" programmer.

Notes:

* Input file format (Intel HEX or Binary) is auto-detected
* To save firmware pass `--read` option
* Default serial port is `/dev/ttyUSB0` (`COM3` on Windows)
* Default port speed is 115200 bps (except for `--noreset` it is 19200 bps)
* Automatic chip reset asserts both DTR and RTS
* For "Arduino as ISP" either `--noreset` or `--at89s` option required
* Arduino bootloader (Optiboot) has fake Erase operation
* Fuses are supported only if STK\_UNIVERSAL command works
* AT89Sxx chips are programmable by "Arduino as ISP", see `--at89s` option

### Build

Run `make`.

### Use

```
Usage: avrtool [OPTION]... [FILE]
AVRISP serial programmer. Write HEX/BIN file to AVR/Arduino.

-p, --port=PORT    Select serial device
-b, --baud=BAUD    Transfer baud rate
-x, --erase        Erase chip first
-r, --read         Read Flash Memory to FILE
-n, --noreset      Do not assert DTR or RTS
    --at89s        Target AT89Sxx series
    --lfuse=XX     Set low fuse
    --hfuse=XX     Set high fuse
    --efuse=XX     Set extended fuse
    --lock=XX      Set lock byte
-h, --help         Show this message and exit
```
