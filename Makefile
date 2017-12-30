TARGET_SERVER = build/vex-server
TARGET_CLIENT = build/vex
CC = clang
CFLAGS = -Wall -I include -lm -pthread
DEPS = build/utils.o build/http.o

.PHONY: all client server checkdir clean image

all: checkdir client server
recompile: clean checkdir client server

client: utils.o http.o
	$(CC) $(CFLAGS) $(DEPS) -o ${TARGET_CLIENT} src/client.c

server: utils.o http.o
	$(CC) $(CFLAGS) $(DEPS) -o ${TARGET_SERVER} src/server.c

utils.o: checkdir
	$(CC) -Wall -I include -c src/utils.c -o build/utils.o

http.o: checkdir
	$(CC) -Wall -I include -c src/http.c -o build/http.o

checkdir:
	@mkdir -p build

clean:
	@rm -rf build/

image:
	docker build -t vex .
