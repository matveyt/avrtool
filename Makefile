CFLAGS := -O2 -std=c99 -Wall -Wextra -Wpedantic -Werror
LDFLAGS := -s

.PHONY : clean

avrtool : ihex.o isp.o stdz.o ucomm.o
avrtool.o : ihex.h isp.h stdz.h ucomm.h
ihex.o : ihex.h stdz.h
isp.o : isp.h
stdz.o : stdz.h
ucomm.o : ucomm.h
clean : ;-rm -f avrtool *.o
