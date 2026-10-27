/*
 * frontend/font.c - bitmap fonts (glyphs are textures named <prefix><char code>)
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "frontend\font.c"); partially translated.
 */

#include <stdio.h>

#include "dr_types.h"
#include "dr_globals.h"
#include "Engine/3dfx/tset.h"
#include "frontend/font.h"
#include "frontend/frontend.h"

#include "../D3DGlide/glide_ptrs.h"
#include "Engine/3dfx/gfxinit.h"
#include "frontend/cpupoly.h"
#include "map.h"

typedef struct FONT_GLYPH
{
	short valid;
	short tset;
	u_char u;
	u_char v;
	u_char w;
	u_char h;
} FONT_GLYPH;

typedef struct FONT_DESC
{
	short palette;
	short offset;		// added to each glyph's width
	short spacing;		// advance for characters without a glyph
	FONT_GLYPH glyphs[256];
} FONT_DESC;

#define MAX_FONTS			4

// PrintAt modes
#define PRINT_CENTRE		0x1
#define PRINT_RIGHT			0x2
#define PRINT_ADDITIVE		0x4
#define PRINT_TRANSLUCENT	0x8
#define PRINT_SUBTRACTIVE	0x10
#define PRINT_BOTTOM		0x20	// ypos is the bottom of the text

#define TEXT3				4		// POLY_TYPES: textured triangle

int GetFontHeight(int font);
void DrawFontString(float x, float y, float xscale, float yscale, float u, float v, float w, float h, int tset, u_char *colour);

//------------------------------------------------------------------------------------------
// initialised data (0x54cad4)

float x_scale = 1.0f;
float y_scale = 1.0f;
RGB current_colour = { 255, 255, 255, 0 };
float font_subpixel_adjust = 0.5f;

//------------------------------------------------------------------------------------------
// uninitialised data (0x5d7a18)

FONT_DESC fontdata[MAX_FONTS];
char current_mode;
u_char current_font;
float current_xpos;
float current_ypos;
float start_xpos;
// NOTE: 0x5d9a40 - 0x5da0b0 (1648 bytes) follow without names or references; unclear whether
// they belong to this file, left out

// [0x40aae0] [D]
// Finds the glyph textures <prefix>0 .. <prefix>255; returns the texture set of the last one.
int LoadFont(const char *prefix, u_char font, short offset, short spacing, short palette)
{
	int i;
	int tset;
	char name[16];
	TEX_INFO *info;

	tset = -1;

	for (i = 0; i < 256; i++)
	{
		sprintf(name, "%s%d", prefix, i);

		info = GetTextureDetails(name);

		if (info)
		{
			fontdata[font].glyphs[i].tset = info->tset;
			fontdata[font].glyphs[i].valid = 1;
			fontdata[font].glyphs[i].u = info->u;
			fontdata[font].glyphs[i].v = info->v;

			if (info->w)
				fontdata[font].glyphs[i].w = info->w;
			else
				fontdata[font].glyphs[i].w = 255;

			if (info->h)
				fontdata[font].glyphs[i].h = info->h;
			else
				fontdata[font].glyphs[i].h = 255;

			tset = info->tset;
		}
		else
		{
			fontdata[font].glyphs[i].valid = 0;
		}
	}

	fontdata[font].palette = palette;
	fontdata[font].offset = offset;
	fontdata[font].spacing = spacing;

	return tset;
}

// [0x40ad12] [D]
// Height of the font's first glyph.
int GetFontHeight(int font)
{
	int i;

	for (i = 0; i < 256; i++)
	{
		if (fontdata[font].glyphs[i].valid)
			return fontdata[font].glyphs[i].h;
	}

	return 0;
}

// [0x40ad6a] [D]
void PrintColour(char r, char g, char b)
{
	current_colour.r = r;
	current_colour.g = g;
	current_colour.b = b;
}

// [0x40ad9f] [D]
void PrintAt(float xpos, float ypos, char font, char mode)
{
	start_xpos = xpos;
	current_xpos = xpos;
	current_ypos = ypos;
	current_font = font;
	current_mode = mode;
}

// [0x40addc] [D]
// Prints text at the current position; "^^n" switches to font n.
void PrintText(char *text)
{
	int depth_mask;
	u_char font;
	u_char c;
	char *p;
	float width;
	float y;
	FONT_GLYPH *glyph;

	width = 0.0f;

	if (!gInFrondend)
	{
		SwapPalette(fontdata[current_font].palette);

		depth_mask = EnableDepthMask(0);
	}

	if (current_mode & (PRINT_RIGHT | PRINT_CENTRE))
	{
		// measure the text
		font = current_font;
		p = text;

		while ((c = *p++) != 0)
		{
			if (c == '^' && *p == '^')
			{
				p++;
				font = *p++ - '0';
			}
			else if (fontdata[font].glyphs[c].valid)
			{
				width += (float)(fontdata[font].offset + fontdata[font].glyphs[c].w) * x_scale;
			}
			else
			{
				width += (float)fontdata[font].spacing * x_scale;
			}
		}

		if (current_mode & PRINT_RIGHT)
			current_xpos -= width;
		else
			current_xpos -= width / 2.0f;
	}

	y = current_ypos;

	if (current_mode & PRINT_BOTTOM)
		y -= (float)GetFontHeight(current_font);

	while ((c = *text++) != 0)
	{
		if (c == '^' && *text == '^')
		{
			text++;
			current_font = *text++ - '0';

			if (current_mode & PRINT_BOTTOM)
				y = current_ypos - (float)GetFontHeight(current_font);
		}
		else if (fontdata[current_font].glyphs[c].valid)
		{
			glyph = &fontdata[current_font].glyphs[c];

			DrawFontString(current_xpos, y, x_scale, y_scale, (float)glyph->u, (float)glyph->v, (float)glyph->w, (float)glyph->h,
				glyph->tset, (u_char *)&current_colour);

			current_xpos += (float)(fontdata[current_font].offset + glyph->w) * x_scale;
		}
		else
		{
			current_xpos += (float)fontdata[current_font].spacing * x_scale;
		}
	}

	if (!gInFrondend)
		EnableDepthMask(depth_mask);
}

// [0x40b2d4] [D]
void PrintReset(void)
{
	current_mode = 0;
	current_font = 0;

	current_colour.r = 255;
	current_colour.g = 255;
	current_colour.b = 255;

	current_xpos = 0.0f;
	current_ypos = 0.0f;
	start_xpos = 0.0f;

	x_scale = 1.0f;
	y_scale = 1.0f;
}

// [0x40b4b6] [D]
// Draws one glyph: into the frontend's virtual screen, or as a textured quad in game.
void DrawFontString(float x, float y, float xscale, float yscale, float u, float v, float w, float h, int tset, u_char *colour)
{
	float oow;
	float u0;
	float v0;
	float w0;
	float h0;
	GrVertex verts[4];

	if (gInFrondend)
	{
		if (current_mode & PRINT_TRANSLUCENT)
			CPUDrawTransTexture((int)x, (int)y, tset, (u_char)(int)u, (u_char)(int)v, (int)w, (int)h);
		else
			CPUDrawTexture((int)x, (int)y, tset, (u_char)(int)u, (u_char)(int)v, (int)w, (int)h);

		return;
	}

	// the D3D emulation takes texture coords and z from oow
	if (using_glide)
		oow = 1.0f;
	else
		oow = 1.0f / 64.0f;

	u0 = u + font_subpixel_adjust;
	v0 = v + font_subpixel_adjust;
	w0 = w - font_subpixel_adjust;
	h0 = h - font_subpixel_adjust;

	verts[0].x = x;
	verts[0].y = y;
	verts[0].oow = oow;
	verts[0].r = (float)colour[0];
	verts[0].g = (float)colour[1];
	verts[0].b = (float)colour[2];
	verts[0].a = 255.0f;
	verts[0].tmuvtx[0].sow = u0 * oow;
	verts[0].tmuvtx[0].tow = v0 * oow;

	verts[1].x = w0 * xscale + x;
	verts[1].y = y;
	verts[1].oow = oow;
	verts[1].r = (float)colour[0];
	verts[1].g = (float)colour[1];
	verts[1].b = (float)colour[2];
	verts[1].a = 255.0f;
	verts[1].tmuvtx[0].sow = (u0 + w0) * oow;
	verts[1].tmuvtx[0].tow = verts[0].tmuvtx[0].tow;

	verts[2].x = verts[1].x;
	verts[2].y = h0 * yscale + y;
	verts[2].oow = oow;
	verts[2].r = (float)colour[0];
	verts[2].g = (float)colour[1];
	verts[2].b = (float)colour[2];
	verts[2].a = 255.0f;
	verts[2].tmuvtx[0].sow = verts[1].tmuvtx[0].sow;
	verts[2].tmuvtx[0].tow = (v0 + h0) * oow;

	verts[3].x = x;
	verts[3].y = verts[2].y;
	verts[3].oow = oow;
	verts[3].r = (float)colour[0];
	verts[3].g = (float)colour[1];
	verts[3].b = (float)colour[2];
	verts[3].a = 255.0f;
	verts[3].tmuvtx[0].sow = verts[0].tmuvtx[0].sow;
	verts[3].tmuvtx[0].tow = verts[2].tmuvtx[0].tow;

	if (tset != last_tset)
	{
		last_tset = tset;
		grTexSource(GR_TMU0, TextureSlots[tset].texture_ram_address, GR_MIPMAPLEVELMASK_BOTH, TextureSlots[tset].TInfo);
	}

	if (last_state != TEXT3)
	{
		grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE);
		last_state = TEXT3;
	}

	if (current_mode & PRINT_ADDITIVE)
	{
		if (last_trans_on != 3)
		{
			grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ZERO);
			last_trans_on = 3;
		}
	}
	else if (current_mode & PRINT_TRANSLUCENT)
	{
		// NOTE: only the first vertex gets alpha 128 (as in the binary)
		verts[0].a = 128.0f;

		grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO);
		grAlphaCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXFALSE);

		gPreviousAlpha = 128;
		last_trans_on = 0x80;
	}
	else if (current_mode & PRINT_SUBTRACTIVE)
	{
		if (last_trans_on != 2)
		{
			grAlphaBlendFunction(GR_BLEND_ZERO, GR_BLEND_ONE_MINUS_SRC_COLOR, GR_BLEND_ONE, GR_BLEND_ZERO);
			last_trans_on = 2;
		}
	}
	else if (last_trans_on)
	{
		grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
		last_trans_on = 0;
	}

	grDrawPolygonVertexList(4, verts);
}
