all:
	gcc -o bin/fontstash fontstash.c main.c -I/usr/include/SDL -lSDL -lGL -lm

run: all
	cd bin && ./fontstash
