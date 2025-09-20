CFLAGS := -O2 -std=c99 -Wall -Wextra -Wpedantic -Werror
LDFLAGS := -s

.PHONY : clean

avrtool : ihx.o isp.o stdz.o ucomm.o
avrtool.o : ihx.h isp.h stdz.h ucomm.h
ihx.o : ihx.h stdz.h
isp.o : isp.h
stdz.o : stdz.h
ucomm.o : ucomm.h
clean : ;-rm -f avrtool *.o
