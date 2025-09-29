CFLAGS += -O2 -std=c99
CFLAGS += -Wall -Wextra -Wpedantic -Werror
LDFLAGS += -s

avrtool : avrtool.o ihx.o isp.o stdz.o ucomm.o ya_getopt.o
avrtool.o : avrtool.c ihx.h isp.h stdz.h ucomm.h ya_getopt.h
ihx.o : ihx.c ihx.h stdz.h ucomm.h
isp.o : isp.c isp.h
stdz.o : stdz.c stdz.h
ucomm.o : ucomm.c ucomm.h
ya_getopt.o : ya_getopt.c ya_getopt.h
clean : ;-rm -f avrtool *.o
.PHONY : clean
