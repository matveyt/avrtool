PROG = avrtool
OBJS = avrtool.o ihx.o isp.o stdz.o ucomm.o

$(PROG) : $(OBJS)
avrtool.o : ihx.h isp.h stdz.h ucomm.h getopt.h
ihx.o : ihx.h stdz.h
isp.o : isp.h ucomm.h
stdz.o : stdz.h getopt.h getopt.c
ucomm.o : ucomm.h

CFLAGS += -O2 -std=c99
CFLAGS += -Wall -Wextra -Wpedantic -Werror
LDFLAGS += -s
MAKEFLAGS += -r

$(PROG) :
	$(CC) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $@
%.o : %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
clean :
	-rm -f $(PROG) *.o
.PHONY : clean
