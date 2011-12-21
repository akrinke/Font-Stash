//
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

#include <OpenGL/gl.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_malloc(x,u)    malloc(x)
#define STBTT_free(x,u)      free(x)
#include "stb_truetype.h"

#define HASH_LUT_SIZE 256
#define MAX_ROWS 128
#define MAX_FONTS 4
#define VERT_COUNT (6*128)
#define VERT_STRIDE (sizeof(float)*4)

static unsigned int hashint(unsigned int a)
{
	a += ~(a<<15);
	a ^=  (a>>10);
	a +=  (a<<3);
	a ^=  (a>>6);
	a += ~(a<<11);
	a ^=  (a>>16);
	return a;
}


struct sth_quad
{
	float x0,y0,s0,t0;
	float x1,y1,s1,t1;
};

struct sth_row
{
	short x,y,h;	
};

struct sth_glyph
{
	unsigned int codepoint;
	short size;
	int x0,y0,x1,y1;
	float xadv,xoff,yoff;
	int next;
};

struct sth_font
{
	stbtt_fontinfo font;
	unsigned char* data;
	int datasize;
	struct sth_glyph* glyphs;
	int lut[HASH_LUT_SIZE];
	int nglyphs;
	float ascender;
	float descender;
	float lineh;
};

struct sth_stash
{
	int tw,th;
	float itw,ith;
	GLuint tex;
	struct sth_row rows[MAX_ROWS];
	int nrows;
	struct sth_font fonts[MAX_FONTS];
	float verts[4*VERT_COUNT];
	int nverts;
	int drawing;
};



// Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.

#define UTF8_ACCEPT 0
#define UTF8_REJECT 1

static const unsigned char utf8d[] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 00..1f
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 20..3f
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 40..5f
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 60..7f
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, // 80..9f
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, // a0..bf
	8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, // c0..df
	0xa,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x4,0x3,0x3, // e0..ef
	0xb,0x6,0x6,0x6,0x5,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8, // f0..ff
	0x0,0x1,0x2,0x3,0x5,0x8,0x7,0x1,0x1,0x1,0x4,0x6,0x1,0x1,0x1,0x1, // s0..s0
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,0,1,0,1,1,1,1,1,1, // s1..s2
	1,2,1,1,1,1,1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1, // s3..s4
	1,2,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,3,1,1,1,1,1,1, // s5..s6
	1,3,1,1,1,1,1,3,1,3,1,1,1,1,1,1,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // s7..s8
};

static unsigned int decutf8(unsigned int* state, unsigned int* codep, unsigned int byte)
{
	unsigned int type = utf8d[byte];
	*codep = (*state != UTF8_ACCEPT) ?
		(byte & 0x3fu) | (*codep << 6) :
		(0xff >> type) & (byte);
	*state = utf8d[256 + *state*16 + type];
	return *state;
}



struct sth_stash* sth_create(int cachew, int cacheh)
{
	struct sth_stash* stash;

	// Allocate memory for the font stash.
	stash = (struct sth_stash*)malloc(sizeof(struct sth_stash));
	if (stash == NULL) goto error;
	memset(stash,0,sizeof(struct sth_stash));

	// Create texture for the cache.
	stash->tw = cachew;
	stash->th = cacheh;
	stash->itw = 1.0f/cachew;
	stash->ith = 1.0f/cacheh;
	glGenTextures(1, &stash->tex);
	if (!stash->tex) goto error;
	glBindTexture(GL_TEXTURE_2D, stash->tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, stash->tw,stash->th, 0, GL_ALPHA, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	return stash;
	
error:
	if (stash != NULL)
		free(stash);
	return NULL;
}

int sth_add_font(struct sth_stash* stash, int idx, const char* path)
{
	FILE* fp = 0;
	int i, ascent, descent, fh, lineGap;
	struct sth_font* fnt;
	
	if (idx < 0 || idx >= MAX_FONTS) return 0;
	
	fnt = &stash->fonts[idx];
	if (fnt->data)
		free(fnt->data);
	if (fnt->glyphs)
		free(fnt->glyphs);
	memset(fnt,0,sizeof(struct sth_font));
	
	// Init hash lookup.
	for (i = 0; i < HASH_LUT_SIZE; ++i) fnt->lut[i] = -1;
	
	// Read in the font data.
	fp = fopen(path, "rb");
	if (!fp) goto error;
	fseek(fp,0,SEEK_END);
	fnt->datasize = (int)ftell(fp);
	fseek(fp,0,SEEK_SET);
	fnt->data = (unsigned char*)malloc(fnt->datasize);
	if (fnt->data == NULL) goto error;
	fread(fnt->data, 1, fnt->datasize, fp);
	fclose(fp);
	fp = 0;
	
	// Init stb_truetype
	if (!stbtt_InitFont(&fnt->font, fnt->data, 0)) goto error;
	
	// Store normalized line height. The real line height is got
	// by multiplying the lineh by font size.
	stbtt_GetFontVMetrics(&fnt->font, &ascent, &descent, &lineGap);
	fh = ascent - descent;
	fnt->ascender = (float)ascent / (float)fh;
	fnt->descender = (float)descent / (float)fh;
	fnt->lineh = (float)(fh + lineGap) / (float)fh;
	
	return 1;
	
error:
	if (fnt->data) free(fnt->data);
	if (fnt->glyphs) free(fnt->glyphs);
	memset(fnt,0,sizeof(struct sth_font));
	if (fp) fclose(fp);
	return 0;
}

static struct sth_glyph* get_glyph(struct sth_stash* stash, struct sth_font* fnt, unsigned int codepoint, short isize)
{
	int i,g,advance,lsb,x0,y0,x1,y1,gw,gh;
	float scale;
	struct sth_glyph* glyph;
	unsigned char* bmp;
	unsigned int h;
	float size = isize/10.0f;
	int rh;
	struct sth_row* br;

	// Find code point and size.
	h = hashint(codepoint) & (HASH_LUT_SIZE-1);
	i = fnt->lut[h];
	while (i != -1)
	{
		if (fnt->glyphs[i].codepoint == codepoint && fnt->glyphs[i].size == isize)
			return &fnt->glyphs[i];
		i = fnt->glyphs[i].next;
	}
	
	// Could not find glyph, create it.
	scale = stbtt_ScaleForPixelHeight(&fnt->font, size);
	g = stbtt_FindGlyphIndex(&fnt->font, codepoint);
	stbtt_GetGlyphHMetrics(&fnt->font, g, &advance, &lsb);
	stbtt_GetGlyphBitmapBox(&fnt->font, g, scale,scale, &x0,&y0,&x1,&y1);
	gw = x1-x0;
	gh = y1-y0;


	// Find row where the glyph can be fit.
	br = NULL;
	rh = (gh+7) & ~7;
	for (i = 0; i < stash->nrows; ++i)
	{
		if (stash->rows[i].h == rh && stash->rows[i].x+gw+1 <= stash->tw)
			br = &stash->rows[i];
	}
	
	// If no row found, add new.
	if (br == NULL)
	{
		short py = 0;
		// Check that there is enough space.
		if (stash->nrows)
		{
			py = stash->rows[stash->nrows-1].y + stash->rows[stash->nrows-1].h+1;
			if (py+rh > stash->th)
				return 0;
		}
		// Init and add row
		br = &stash->rows[stash->nrows];
		br->x = 0;
		br->y = py;
		br->h = rh;
		stash->nrows++;
	}
	
	// Alloc space for new glyph.
	fnt->nglyphs++;
	fnt->glyphs = realloc(fnt->glyphs, fnt->nglyphs*sizeof(struct sth_glyph));
	if (!fnt->glyphs) return 0;

	// Init glyph.
	glyph = &fnt->glyphs[fnt->nglyphs-1];
	memset(glyph, 0, sizeof(struct sth_glyph));
	glyph->codepoint = codepoint;
	glyph->size = isize;
	glyph->x0 = br->x;
	glyph->y0 = br->y;
	glyph->x1 = glyph->x0+gw;
	glyph->y1 = glyph->y0+gh;
	glyph->xadv = scale * advance;
	glyph->xoff = (float)x0;
	glyph->yoff = (float)y0;
	glyph->next = 0;

	// Advance row location.
	br->x += gw+1;
	
	// Insert char to hash lookup.
	glyph->next = fnt->lut[h];
	fnt->lut[h] = fnt->nglyphs-1;

	// Rasterize
	bmp = (unsigned char*)malloc(gw*gh);
	if (bmp)
	{
		stbtt_MakeGlyphBitmap(&fnt->font, bmp, gw,gh,gw, scale,scale, g);
		// Update texture
		glPixelStorei(GL_UNPACK_ALIGNMENT,1);
		glTexSubImage2D(GL_TEXTURE_2D, 0, glyph->x0,glyph->y0, gw,gh, GL_ALPHA,GL_UNSIGNED_BYTE,bmp); 
		free(bmp);
	}
	
	return glyph;
}

static int get_quad(struct sth_stash* stash, struct sth_font* fnt, unsigned int codepoint, short isize, float* x, float* y, struct sth_quad* q)
{
	int rx,ry;
	struct sth_glyph* glyph = get_glyph(stash, fnt, codepoint, isize);
	if (!glyph) return 0;
	
	rx = floorf(*x + glyph->xoff);
	ry = floorf(*y - glyph->yoff);
	
	q->x0 = rx;
	q->y0 = ry;
	q->x1 = rx + glyph->x1 - glyph->x0;
	q->y1 = ry - glyph->y1 + glyph->y0;
	
	q->s0 = (glyph->x0) * stash->itw;
	q->t0 = (glyph->y0) * stash->ith;
	q->s1 = (glyph->x1) * stash->itw;
	q->t1 = (glyph->y1) * stash->ith;
	
	*x += glyph->xadv;
	
	return 1;
}

static float* setv(float* v, float x, float y, float s, float t)
{
	v[0] = x;
	v[1] = y;
	v[2] = s;
	v[3] = t;
	return v+4;
}

static void flush_draw(struct sth_stash* stash)
{
	if (stash->nverts == 0)
		return;
		
	glBindTexture(GL_TEXTURE_2D, stash->tex);
	glEnable(GL_TEXTURE_2D);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glVertexPointer(2, GL_FLOAT, VERT_STRIDE, stash->verts);
	glTexCoordPointer(2, GL_FLOAT, VERT_STRIDE, stash->verts+2);
	glDrawArrays(GL_TRIANGLES, 0, stash->nverts);
	glDisable(GL_TEXTURE_2D);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	stash->nverts = 0;
}

void sth_begin_draw(struct sth_stash* stash)
{
	if (stash == NULL) return;
	if (stash->drawing)
		flush_draw(stash);
	stash->drawing = 1;
}

void sth_end_draw(struct sth_stash* stash)
{
	if (stash == NULL) return;
	if (!stash->drawing) return;

/*
	// Debug dump.
	if (stash->nverts+6 < VERT_COUNT)
	{
		float x = 500, y = 100;
		float* v = &stash->verts[stash->nverts*4];

		v = setv(v, x, y, 0, 0);
		v = setv(v, x+stash->tw, y, 1, 0);
		v = setv(v, x+stash->tw, y+stash->th, 1, 1);

		v = setv(v, x, y, 0, 0);
		v = setv(v, x+stash->tw, y+stash->th, 1, 1);
		v = setv(v, x, y+stash->th, 0, 1);

		stash->nverts += 6;
	}
*/
	
	flush_draw(stash);
	stash->drawing = 0;
}

void sth_draw_text(struct sth_stash* stash,
				   int idx, float size,
				   float x, float y,
				   const char* s, float* dx)
{
	unsigned int codepoint;
	unsigned int state = 0;
	struct sth_quad q;
	short isize = (short)(size*10.0f);
	float* v;
	struct sth_font* fnt;
	
	if (stash == NULL) return;
	if (!stash->tex) return;
	if (idx < 0 || idx >= MAX_FONTS) return;
	fnt = &stash->fonts[idx];
	if (!fnt->data) return;
	
	for (; *s; ++s)
	{
		if (decutf8(&state, &codepoint, *(unsigned char*)s)) continue;

		if (stash->nverts+6 >= VERT_COUNT)
			flush_draw(stash);
		
		if (!get_quad(stash, fnt, codepoint, isize, &x, &y, &q)) continue;
		
		v = &stash->verts[stash->nverts*4];
		
		v = setv(v, q.x0, q.y0, q.s0, q.t0);
		v = setv(v, q.x1, q.y0, q.s1, q.t0);
		v = setv(v, q.x1, q.y1, q.s1, q.t1);

		v = setv(v, q.x0, q.y0, q.s0, q.t0);
		v = setv(v, q.x1, q.y1, q.s1, q.t1);
		v = setv(v, q.x0, q.y1, q.s0, q.t1);
		
		stash->nverts += 6;		
	}
	
	if (dx) *dx = x;
}

void sth_dim_text(struct sth_stash* stash,
				  int idx, float size,
				  const char* s,
				  float* minx, float* miny, float* maxx, float* maxy)
{
	unsigned int codepoint;
	unsigned int state = 0;
	struct sth_quad q;
	short isize = (short)(size*10.0f);
	struct sth_font* fnt;
	float x = 0, y = 0;
	
	if (stash == NULL) return;
	if (!stash->tex) return;
	if (idx < 0 || idx >= MAX_FONTS) return;
	fnt = &stash->fonts[idx];
	if (!fnt->data) return;
	
	*minx = *maxx = x;
	*miny = *maxy = y;

	for (; *s; ++s)
	{
		if (decutf8(&state, &codepoint, *(unsigned char*)s)) continue;
		if (!get_quad(stash, fnt, codepoint, isize, &x, &y, &q)) continue;
		if (q.x0 < *minx) *minx = q.x0;
		if (q.x1 > *maxx) *maxx = q.x1;
		if (q.y1 < *miny) *miny = q.y1;
		if (q.y0 > *maxy) *maxy = q.y0;
	}
}

void sth_vmetrics(struct sth_stash* stash,
				  int idx, float size,
				  float* ascender, float* descender, float* lineh)
{
	if (stash == NULL) return;
	if (!stash->tex) return;
	if (idx < 0 || idx >= MAX_FONTS) return;
	if (!stash->fonts[idx].data) return;
	if (ascender)
		*ascender = stash->fonts[idx].ascender*size;
	if (descender)
		*descender = stash->fonts[idx].descender*size;
	if (lineh)
		*lineh = stash->fonts[idx].lineh*size;
}

void sth_delete(struct sth_stash* stash)
{
	int i;
	if (!stash) return;
	if (stash->tex) glDeleteTextures(1,&stash->tex);
	for (i = 0; i < MAX_FONTS; ++i)
	{
		if (stash->fonts[i].glyphs)
			free(stash->fonts[i].glyphs);
		if (stash->fonts[i].data)
			free(stash->fonts[i].data);
	}
	free(stash);
}
