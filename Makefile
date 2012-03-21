all:
	gcc -o bin/fontstash fontstash.c main.c `sdl-config --cflags --libs` `pkg-config --cflags --libs SDL_image` -lGL -lm

debug:
	gcc -o bin/fontstash -ggdb fontstash.c main.c `sdl-config --cflags --libs` `pkg-config --cflags --libs SDL_image` -lGL -lm

run: all
	cd bin && ./fontstash
