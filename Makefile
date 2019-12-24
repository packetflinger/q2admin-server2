MY_CFLAGS = $(shell mysql_config --cflags)
MY_LDFLAGS = $(shell mysql_config --libs)

GLIB_CFLAGS = $(shell pkg-config --cflags glib-2.0)
GLIB_LDFLAGS = $(shell pkg-config --libs glib-2.0)

all:
	gcc --std=c99 -D_POSIX_C_SOURCE=200112L -o q2admin-server server.c msg.c cmd.c log.c teleport.c $(MY_CFLAGS) $(MY_LDFLAGS) $(GLIB_CFLAGS) $(GLIB_LDFLAGS)

clean:
	rm -f q2admin-server
