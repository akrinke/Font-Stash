all:
	gcc -o bin/fontstash fontstash.c main.c `sdl-config --cflags --libs` -lGL -lm

run: all
	cd bin && ./fontstash
