CFLAGS=-std=c99 -O2
CPPFLAGS=-O2
all: file_server client
file.o: file.c file.h
	gcc $(CFLAGS) file.c -c -o file.o
poll.o: poll.c poll.h
	gcc $(CFLAGS) poll.c -c -o poll.o
coroutine.o: coroutine.c coroutine.h switch.S
	gcc $(CFLAGS) coroutine.c -c -o coroutine.o
switch.o: switch.S
	gcc $(CFLAGS) switch.S -c -o switch.o
main.o: main.c
	gcc $(CFLAGS) main.c -c -o main.o
listen_server.o: listen_server.c listen_server.h
	gcc $(CFLAGS) listen_server.c -c -o listen_server.o
sftp.o: sftp.c
	gcc $(CFLAGS) sftp.c -c -o sftp.o
file_server: file.o poll.o coroutine.o sftp.o switch.o main.o listen_server.o
	gcc $(CFLAGS) file.o poll.o coroutine.o sftp.o switch.o listen_server.o main.o -o file_server
client: client.cpp
	g++ $(CPPFLAGS) client.cpp -o client
.PHONY: clean
clean:
	rm *.o file_server client
