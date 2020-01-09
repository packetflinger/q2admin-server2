-include .config

MY_CFLAGS = $(shell mysql_config --cflags)
MY_LDFLAGS = $(shell mysql_config --libs)

GLIB_CFLAGS = $(shell pkg-config --cflags glib-2.0)
GLIB_LDFLAGS = $(shell pkg-config --libs glib-2.0)

CC ?= gcc
LD ?= ld
RM ?= rm -f
STRIP ?= strip
WINDRES ?= windres

CFLAGS ?= --std=c99 -D_POSIX_C_SOURCE=200112L

HEADERS := server.h

OBJS :=	cmd.o \
		log.o \
		msg.o \
		parse.o \
		server.o \
		teleport.o \
		util.o

CFLAGS += $(MY_CFLAGS) $(GLIB_CFLAGS)
LDFLAGS += $(MY_LDFLAGS) $(GLIB_LDFLAGS)
TARGET ?= q2admin-server
	
all: $(TARGET)

default: all

.PHONY: all default clean strip

# Define V=1 to show command line.
ifdef V
    Q :=
    E := @true
else
    Q := @
    E := @echo
endif

-include $(OBJS:.o=.d)

%.o: %.c $(HEADERS)
	$(E) [CC] $@
	$(Q)$(CC) -c $(CFLAGS) -o $@ $<

%.o: %.rc
	$(E) [RC] $@
	$(Q)$(WINDRES) $(RCFLAGS) -o $@ $<

$(TARGET): $(OBJS)
	$(E) [LD] $@
	$(Q)$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	$(E) [CLEAN]
	$(Q)$(RM) *.o *.d $(TARGET)

strip: $(TARGET)
	$(E) [STRIP]
	$(Q)$(STRIP) $(TARGET)
