TARGET_SERVER = build/vex-server
TARGET_CLIENT = build/vex
CC = cc
CFLAGS = -O2 -std=c99 -Wall -I include -lm -pthread
DEPS = build/utils.o

.PHONY: all client server checkdir clean

all: checkdir client server
recompile: clean checkdir client server

client: utils.o
	$(CC) $(CFLAGS) $(DEPS) -o ${TARGET_CLIENT} src/client.c

server: utils.o
	$(CC) $(CFLAGS) $(DEPS) -o ${TARGET_SERVER} src/server.c

utils.o: checkdir
	$(CC) -Wall -I include -c src/utils.c -o build/utils.o

checkdir:
	@mkdir -p build

clean:
	@rm -rf build/
