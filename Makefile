-include .config

GLIB_CFLAGS = $(shell pkg-config --cflags glib-2.0)
GLIB_LDFLAGS = $(shell pkg-config --libs glib-2.0)

CC ?= gcc
LD ?= ld
RM ?= rm -f
STRIP ?= strip
WINDRES ?= windres

CFLAGS ?= -g --std=c99 -D_POSIX_C_SOURCE=200112L -I/usr/local/jannson/include

HEADERS := \
		server.h \
		list.h \
		threadpool.h 

OBJS :=	\
		cmd.o \
		crypto.o \
		database.o \
		log.o \
		msg.o \
		parse.o \
		server.o \
		teleport.o \
		threadpool.o \
		util.o

CFLAGS += $(GLIB_CFLAGS)
LDFLAGS += $(GLIB_LDFLAGS) -lcrypto -lsqlite3 -lpthread
TARGET ?= q2admind
	
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
