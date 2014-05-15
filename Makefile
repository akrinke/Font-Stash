PROGRAM = fontstash
C_FILES := $(wildcard *.c)
CC = gcc
CFLAGS = -std=c99 -Wall -pedantic -Wno-unused-but-set-variable
CFLAGS += `sdl-config --cflags` `pkg-config --cflags SDL_image`
LDFLAGS = `sdl-config --libs` `pkg-config --libs SDL_image` -lGL -lm

all:
	$(CC) -o bin/$(PROGRAM) $(CFLAGS) $(C_FILES) $(LDFLAGS)

debug:
	$(CC) -o bin/$(PROGRAM) -ggdb $(CFLAGS) $(C_FILES) $(LDFLAGS)

run: all
	cd bin && ./$(PROGRAM)
