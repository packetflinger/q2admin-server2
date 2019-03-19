all:
	gcc --std=c99 -o q2admin-server server.c

clean:
	rm -f q2admin-server
