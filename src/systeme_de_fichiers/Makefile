EXEC=led_control
SRCS=$(wildcard *.c)

target=nano

TOOLCHAIN_PATH=/buildroot/output/host/usr/bin/
TOOLCHAIN=$(TOOLCHAIN_PATH)aarch64-linux-
CFLAGS=-Wall -Wextra -g -c -O0 -MD -std=gnu11
CFLAGS+=-mcpu=cortex-a53 -funwind-tables
##CFLAGS+=-O2 -fno-omit-frame-pointer
OBJDIR=.obj/nano

CC=$(TOOLCHAIN)gcc
LD=$(TOOLCHAIN)gcc
AR=$(TOOLCHAIN)ar
STRIP=$(TOOLCHAIN)strip

OBJDIR=.obj/$(target)
OBJS= $(addprefix $(OBJDIR)/, $(SRCS:.c=.o))

$(OBJDIR)/%o: %c
	$(CC) $(CFLAGS) $< -o $@

all: $(OBJDIR)/ $(EXEC)
	echo $(EXEC)
	echo $(SRCS)

$(EXEC): $(OBJS)
	$(LD) $(OBJS) -o $@

$(OBJDIR)/:
	mkdir -p $(OBJDIR)

clean:
	rm -Rf $(OBJDIR) $(EXEC) $(EXEC)_s .obj *~

clean_all: clean

.PHONY: all clean clean_all
