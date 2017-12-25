TARGET = build/vex
LIBS = -lm
CC = gcc
CFLAGS = -g -Wall -I include

.PHONY: default recompile checkdir clean

default: checkdir $(TARGET)
recompile: clean default

OBJECTS = $(subst src/,build/,$(subst .c,.o, $(wildcard src/*.c)))
HEADERS = $(wildcard include/*.h)

build/%.o: src/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) -Wall $(OBJECTS) $(LIBS) -o $@

checkdir:
	@mkdir -p build

clean:
	@rm -rf build/
