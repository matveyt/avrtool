TARGET = avrtool
OBJECTS = avrtool.o stdz.o ihx.o isp.o ucomm.o ucomm_ports.o

CFLAGS += -O2 -std=c99
CFLAGS += -Wall -Wextra -Wpedantic -Werror
LDFLAGS += -s
MAKEFLAGS += -r

$(TARGET) : $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o $@
%.o : %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
clean :
	-rm -f $(TARGET) $(OBJECTS)
.PHONY : clean

avrtool.o : stdz.h getopt.h ihx.h isp.h ucomm.h
stdz.o : stdz.h getopt.h getopt.c
ihx.o : stdz.h ihx.h
isp.o : isp.h ucomm.h
ucomm.o ucomm_ports.o : ucomm.h
