//
// Copyright (c) 2011 Andreas Krinke andreas.krinke@gmx.de
// Copyright (c) 2009 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "SDL.h"

#ifdef __MACOSX__
#include "SDL_Opengl.h"
#else
#include "SDL_opengl.h"
#endif

#include "fontstash.h"

int main(int argc, char *argv[])
{
	int done;
	SDL_Event event;
	SDL_Surface* screen;
	int width, height;
	struct sth_stash* stash = 0;
	FILE* fp = 0;
	int datasize;
	unsigned char* data;

	float sx,sy,dx,dy,lh;

	int droidRegular, droidItalic, droidBold, droidJapanese;

	if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
	{
		fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
		return -1;
	}

	// Init OpenGL
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

	width = 800;
	height = 600;	
	screen = SDL_SetVideoMode(width, height, 0, SDL_OPENGL);
	if (!screen)
	{
		fprintf(stderr, "Could not initialise SDL opengl\n");
		return -1;
	}
	
	SDL_WM_SetCaption("FontStash Demo", 0);

	stash = sth_create(512,512);
	if (!stash)
	{
		fprintf(stderr, "Could not create stash.\n");
		return -1;
	}

	// Load the first font from memory.
	fp = fopen("DroidSerif-Regular.ttf", "rb");
	if (!fp) goto error_add_font;
	fseek(fp, 0, SEEK_END);
	datasize = (int)ftell(fp);
	fseek(fp, 0, SEEK_SET);
	data = (unsigned char*)malloc(datasize);
	if (data == NULL) goto error_add_font;
	fread(data, 1, datasize, fp);
	fclose(fp);
	fp = 0;
	
	if (!(droidRegular = sth_add_font_from_memory(stash, data, datasize)))
		goto error_add_font;

	// Load the remaining fonts directly.
	if (!(droidItalic = sth_add_font(stash,"DroidSerif-Italic.ttf")))
		goto error_add_font;
	if (!(droidBold = sth_add_font(stash,"DroidSerif-Bold.ttf")))
		goto error_add_font;
	if (!(droidJapanese = sth_add_font(stash,"DroidSansJapanese.ttf")))
		goto error_add_font;
	
	done = 0;
	while (!done)
	{
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
				case SDL_MOUSEMOTION:
					break;
				case SDL_MOUSEBUTTONDOWN:
					break;
				case SDL_KEYDOWN:
					if (event.key.keysym.sym == SDLK_ESCAPE)
						done = 1;
					break;
				case SDL_QUIT:
					done = 1;
					break;
				default:
					break;
			}
		}
		
		// Update and render
		glViewport(0, 0, width, height);
		glClearColor(0.3f, 0.3f, 0.32f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_TEXTURE_2D);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0,width,0,height,-1,1);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glDisable(GL_DEPTH_TEST);
		glColor4ub(255,255,255,255);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

		sx = 100; sy = 250;
		
		sth_begin_draw(stash);
		
		dx = sx; dy = sy;
		sth_draw_text(stash, droidRegular, 24.0f, dx, dy, "The quick ", &dx);
		sth_draw_text(stash, droidItalic, 48.0f, dx, dy, "brown ", &dx);
		sth_draw_text(stash, droidRegular, 24.0f, dx, dy, "fox ", &dx);
		sth_vmetrics(stash, droidItalic, 24, NULL, NULL, &lh);
		dx = sx;
		dy -= lh*1.2f;
		sth_draw_text(stash, droidItalic, 24.0f, dx, dy, "jumps over ", &dx);
		sth_draw_text(stash, droidBold, 24.0f, dx, dy, "the lazy ", &dx);
		sth_draw_text(stash, droidRegular, 24.0f, dx, dy, "dog.", &dx);
		dx = sx;
		dy -= lh*1.2f;
		sth_draw_text(stash, droidRegular, 12.0f, dx, dy, "Now is the time for all good men to come to the aid of the party.", &dx);
		sth_vmetrics(stash, droidItalic, 12, NULL, NULL, &lh);
		dx = sx;
		dy -= lh*1.2f*2;
		sth_draw_text(stash, droidItalic, 18.0f, dx, dy, "Ég get etið gler án þess að meiða mig.", &dx);
		sth_vmetrics(stash, droidItalic, 18, NULL, NULL, &lh);
		dx = sx;
		dy -= lh*1.2f;
		sth_draw_text(stash, droidJapanese, 18.0f, dx, dy, "私はガラスを食べられます。それは私を傷つけません。", &dx);

		sth_end_draw(stash);
		
		glEnable(GL_DEPTH_TEST);
		SDL_GL_SwapBuffers();
	}
	
	sth_delete(stash);
	free(data);
	SDL_Quit();
	return 0;

error_add_font:
	fprintf(stderr, "Could not add font.\n");
	return -1;
}
