/*
 * credits.c - the end credits: a scrolling list over the "CREDITS" replay
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "credits.c", 0x4ab010 - 0x4ac0e0).
 *
 * The credits run as a replay (gDoCredits, InitReplay("CREDITS"): credits_choosefunc, the mission
 * ladder's END command). The game draws its picture as usual on the left half of the screen;
 * DrawCredits covers the right half (x 320 - 640) with black, scrolls the credits table up it and
 * draws the Driver logo above. Each table entry is a logo, a text, a language string or a photo;
 * its type also gives the gap to the next entry (margins). The photos share 3 texture sets
 * (PERSON1 - 3) that are overwritten with the next photo as the list scrolls (GetCreditsTexture).
 *
 * [A] On a screen wider than 4:3 the right half reaches the right edge of the screen and the list
 * is centred in it (CREDITS_WIDE, CREDITS_MID); the original covers only x 320 - 640 of the 4:3
 * picture in the middle, and the game's picture showed on the right of it.
 */

#include <stdio.h>

#include "dr_types.h"
#include "dr_globals.h"
#include "credits.h"
#include "D3DGlide/glide_ptrs.h"
#include "Engine/3dfx/gfxinit.h"
#include "Engine/3dfx/tset.h"
#include "frontend/font.h"
#include "map.h"
#include "pres.h"

// credits table entry types. NOTE: names are mine. Text entries hold a string, line entries an
// index into gLanguageStrings; flags is the pen.
#define CRT_LOGO		0	// flags: 1 GT, else Reflections
#define CRT_TEXT		1
#define CRT_LINE		2
#define CRT_TEXT_GAP	3
#define CRT_LINE_GAP	4
#define CRT_TEXT_BIG	5
#define CRT_LINE_BIG	6
#define CRT_PICTURE		7	// flags: index into piccies
#define CRT_END			8

#define NUM_CREDITS_TSETS	3

// [A] how far the right half reaches past x 640 on a screen wider than 4:3, in x of 640 (the
// HUD keeps its proportions, see wide_aspect), and the middle of the right half
#define CREDITS_WIDE	((int)((wide_aspect - 1.0f) * 320.0f))
#define CREDITS_MID		(480 + CREDITS_WIDE / 2)

typedef struct CREDITS_INFO
{
	char type;
	char flags;
	short tset;
	int y_pos;
	union
	{
		int line_num;
		char *text;
	};
} CREDITS_INFO;

typedef struct CREDITS_TSET
{
	short index;
	short tset;
	TEX_INFO *tinfo;
} CREDITS_TSET;

#define CR_LOGO(type)				{ CRT_LOGO, type, 0, 0, { 0 } }
#define CR_TEXT(type, pen, string)	{ type, pen, 0, 0, { .text = string } }
#define CR_LINE(type, pen, line)	{ type, pen, 0, 0, { line } }
#define CR_PICTURE(picture)			{ CRT_PICTURE, picture, 0, 0, { 0 } }
#define CR_END						{ CRT_END, -1, 0, 0, { 0 } }

static void DrawLogo(void);
static void DrawCreditsTextWithTexture(CREDITS_INFO *cred, int y_pos);
static int GetCreditsTexture(CREDITS_INFO *cred);
static void DrawBorderBox(int xpos, int ypos, int width, int height);
static void DrawCreditsText(char *text, int y_pos, int color);
static void DrawCreditsLogo(int type, int y_pos);
static void FlatPoly(int xpos, int ypos, int width, int height, RGB *color);
static void FlatPoly2(int xpos, int ypos, int width, int height, RGB *top, RGB *bottom);
static void TextPoly(int x, int y, int w, int h, TEX_INFO *tinfo, int tset);

// 0x554a70
static CREDITS_INFO credits[] = {
	CR_LOGO(0),
	CR_LINE(CRT_LINE_GAP, 1, 178),
	CR_PICTURE(0),
	CR_TEXT(CRT_TEXT_BIG, 0, "Martin Edmondson"),
	CR_LINE(CRT_LINE_GAP, 1, 179),
	CR_LINE(CRT_LINE, 0, 180),
	CR_PICTURE(1),
	CR_TEXT(CRT_TEXT_GAP, 0, "Nathan Whitaker"),
	CR_PICTURE(2),
	CR_TEXT(CRT_TEXT_GAP, 0, "Wayne Coles"),
	CR_TEXT(CRT_TEXT, 2, "..."),
	CR_TEXT(CRT_TEXT, 0, "Russell Lazzari"),
	CR_TEXT(CRT_TEXT_GAP, 2, "..."),
	CR_PICTURE(6),
	CR_TEXT(CRT_TEXT_BIG, 0, "Christopher Phillips"),
	CR_PICTURE(3),
	CR_TEXT(CRT_TEXT_GAP, 0, "Steve Burrows"),
	CR_PICTURE(27),
	CR_TEXT(CRT_TEXT_GAP, 0, "James Hopkin"),
	CR_LINE(CRT_LINE_GAP, 1, 181),
	CR_PICTURE(4),
	CR_TEXT(CRT_TEXT_GAP, 0, "Derrick Owens"),
	CR_PICTURE(5),
	CR_TEXT(CRT_TEXT_GAP, 0, "Tony Oakden"),
	CR_PICTURE(7),
	CR_TEXT(CRT_TEXT_GAP, 0, "Will Musson"),
	CR_PICTURE(8),
	CR_TEXT(CRT_TEXT_GAP, 0, "James Fowlston"),
	CR_PICTURE(9),
	CR_TEXT(CRT_TEXT_BIG, 0, "Jeanette Phillips"),
	CR_LINE(CRT_LINE_GAP, 1, 182),
	CR_PICTURE(10),
	CR_TEXT(CRT_TEXT_GAP, 0, "David Oxford"),
	CR_PICTURE(11),
	CR_TEXT(CRT_TEXT_GAP, 0, "Andrew Sharratt"),
	CR_PICTURE(12),
	CR_TEXT(CRT_TEXT_GAP, 0, "Daniel Oxford"),
	CR_PICTURE(13),
	CR_TEXT(CRT_TEXT_GAP, 0, "Douglas Kalberg"),
	CR_PICTURE(14),
	CR_TEXT(CRT_TEXT_GAP, 0, "Steven Adams"),
	CR_PICTURE(15),
	CR_TEXT(CRT_TEXT_GAP, 0, "Christopher Willacy"),
	CR_PICTURE(16),
	CR_TEXT(CRT_TEXT_GAP, 0, "Andrew Bales"),
	CR_PICTURE(17),
	CR_TEXT(CRT_TEXT_GAP, 0, "David Oxford Jnr"),
	CR_PICTURE(18),
	CR_TEXT(CRT_TEXT_BIG, 0, "Aidan Wilson"),
	CR_LINE(CRT_LINE_GAP, 1, 183),
	CR_PICTURE(19),
	CR_TEXT(CRT_TEXT_GAP, 0, "Simon McKeown"),
	CR_PICTURE(20),
	CR_TEXT(CRT_TEXT_GAP, 0, "Shaun Stephenson"),
	CR_PICTURE(21),
	CR_TEXT(CRT_TEXT_GAP, 0, "Marcus Hardy"),
	CR_PICTURE(22),
	CR_TEXT(CRT_TEXT_GAP, 0, "Marcel Simons"),
	CR_PICTURE(23),
	CR_TEXT(CRT_TEXT_BIG, 0, "Stuart Dobbs"),
	CR_LINE(CRT_LINE_GAP, 1, 184),
	CR_PICTURE(24),
	CR_TEXT(CRT_TEXT_GAP, 0, "Tony M Roberts"),
	CR_PICTURE(25),
	CR_TEXT(CRT_TEXT_BIG, 0, "Andreas Tawn"),
	CR_LINE(CRT_LINE_GAP, 1, 185),
	CR_PICTURE(26),
	CR_TEXT(CRT_TEXT_BIG, 0, "Gareth Edmondson"),
	CR_LINE(CRT_LINE, 1, 304),
	CR_TEXT(CRT_TEXT_GAP, 0, "Tony M Roberts"),
	CR_LINE(CRT_LINE, 1, 186),
	CR_TEXT(CRT_TEXT, 0, "Mark Sample"),
	CR_TEXT(CRT_TEXT_GAP, 0, "Matthew Gibson"),
	CR_LINE(CRT_LINE, 1, 305),
	CR_TEXT(CRT_TEXT, 0, "William Brown"),
	CR_TEXT(CRT_TEXT_GAP, 0, "Andreas Tawn"),
	CR_LINE(CRT_LINE, 1, 187),
	CR_TEXT(CRT_TEXT_GAP, 0, "Maurice Suckling"),
	CR_LINE(CRT_LINE, 1, 189),
	CR_TEXT(CRT_TEXT_GAP, 0, "Will Davis"),
	CR_LINE(CRT_LINE, 1, 190),
	CR_LINE(CRT_LINE, 1, 191),
	CR_TEXT(CRT_TEXT_GAP, 0, "The Sound House"),
	CR_LINE(CRT_LINE, 1, 192),
	CR_TEXT(CRT_TEXT_GAP, 0, "Mike Stewart"),
	CR_LINE(CRT_LINE, 1, 193),
	CR_TEXT(CRT_TEXT_GAP, 0, "Phil Wood"),
	CR_LINE(CRT_LINE, 1, 194),
	CR_LINE(CRT_LINE, 1, 195),
	CR_TEXT(CRT_TEXT_GAP, 0, "Maverick Media"),
	CR_LINE(CRT_LINE, 1, 196),
	CR_TEXT(CRT_TEXT_GAP, 0, "Seamus Masterson"),
	CR_LINE(CRT_LINE, 1, 197),
	CR_TEXT(CRT_TEXT_GAP, 0, "Sarah Heywood"),
	CR_LINE(CRT_LINE, 1, 194),
	CR_LINE(CRT_LINE, 1, 198),
	CR_TEXT(CRT_TEXT, 0, "Brad Lavelle"),
	CR_TEXT(CRT_TEXT, 0, "Enn Reitel"),
	CR_TEXT(CRT_TEXT, 0, "Kenny Andrews"),
	CR_TEXT(CRT_TEXT, 0, "John Sharian"),
	CR_TEXT(CRT_TEXT, 0, "Jonnie Fiori"),
	CR_TEXT(CRT_TEXT, 0, "Dan Russel"),
	CR_TEXT(CRT_TEXT, 0, "Melanie Nicholson"),
	CR_TEXT(CRT_TEXT_GAP, 0, "James Carol Jordan"),
	CR_LOGO(1),
	CR_LINE(CRT_LINE, 1, 199),
	CR_TEXT(CRT_TEXT_GAP, 0, "Pete Hawley"),
	CR_LINE(CRT_LINE, 1, 200),
	CR_TEXT(CRT_TEXT_GAP, 0, "Pete Hawley"),
	CR_LINE(CRT_LINE, 1, 201),
	CR_TEXT(CRT_TEXT_GAP, 0, "Tim Mawson"),
	CR_LINE(CRT_LINE, 1, 202),
	CR_TEXT(CRT_TEXT, 0, "Mark Smikle"),
	CR_TEXT(CRT_TEXT, 0, "Will Carter"),
	CR_TEXT(CRT_TEXT, 0, "Germaine Mendes"),
	CR_TEXT(CRT_TEXT, 0, "Paul McGinniss"),
	CR_TEXT(CRT_TEXT, 0, "Richard Pareja"),
	CR_TEXT(CRT_TEXT, 0, "Dan McNeil"),
	CR_TEXT(CRT_TEXT, 0, "Tim Wileman"),
	CR_TEXT(CRT_TEXT, 0, "Andrew Azorbo"),
	CR_TEXT(CRT_TEXT, 0, "Wayne Gatley"),
	CR_TEXT(CRT_TEXT_GAP, 0, "Andrew Theodoulou"),
	CR_LINE(CRT_LINE, 1, 203),
	CR_LINE(CRT_LINE, 1, 204),
	CR_TEXT(CRT_TEXT_GAP, 0, "Kevin Turner"),
	CR_LINE(CRT_LINE, 1, 205),
	CR_TEXT(CRT_TEXT_GAP, 0, "Graham Axford"),
	CR_LINE(CRT_LINE, 1, 206),
	CR_TEXT(CRT_TEXT_GAP, 0, "Neil McKenna"),
	CR_LINE(CRT_LINE, 1, 207),
	CR_TEXT(CRT_TEXT_GAP, 0, "Graeme Boxall"),
	CR_LINE(CRT_LINE, 1, 208),
	CR_TEXT(CRT_TEXT_GAP, 0, "Marc Swallow"),
	CR_END,
};

// 0x5550b8
// the gap below an entry, by type
static int margins[9] = { 74, 27, 27, 54, 54, 78, 78, 140, -1 };

// 0x5550dc
static RGB pens[3] = {
	{ 255, 255, 255, 0 },
	{ 255, 204, 0, 0 },
	{ 80, 80, 80, 0 },
};

// 0x5550e8
static RGB white = { 255, 255, 255, 0 };

// 0x5550ec
static char *piccies[28] = {
	"MARTIN", "NAIF", "WAYNE", "STEVEB", "DES", "TONYO", "CHRIST",
	"WILL", "JAMIE", "JEAN", "DAVE", "ANDYS", "DAN", "DOUG",
	"STEVEA", "CHRIS", "ANDYB", "DAVID", "AIDAN", "SIMON", "SHAUN",
	"MARCUS", "MARCEL", "STUART", "TONYR", "ANDREAS", "GARETH", "JAMES",
};

// 0xb9f258
static CREDITS_TSET tsets[NUM_CREDITS_TSETS];
// 0xb9f270
static int current_tset;
// 0xb9f274
static int scrollpos;
// 0xb9f278
static RGB black;

// [0x4ab010] [D]
void InitCredits(void)
{
	TEX_INFO *texture;
	int y_pos;
	char name[12];
	int i;

	y_pos = 480;

	for (i = 0; credits[i].type != CRT_END; i++)
	{
		credits[i].y_pos = y_pos;
		y_pos += margins[(u_char)credits[i].type];
	}

	for (i = 0; i < NUM_CREDITS_TSETS; i++)
	{
		tsets[i].index = -1;
		tsets[i].tset = -1;
		tsets[i].tinfo = NULL;

		sprintf(name, "PERSON%d", i + 1);

		texture = GetTextureDetails(name);

		if (texture)
			tsets[i].tset = texture->tset;
	}

	current_tset = 0;
	scrollpos = 0;
}

// [0x4ab10a] [D]
void ControlCredits(void)
{
	scrollpos++;
}

// [0x4ab11c] [D] [A]
// NOTE: [A] the black and the fades reach the right edge of a screen wider than 4:3, CREDITS_WIDE
void DrawCredits(void)
{
	int line_num;
	int loop;
	int y_pos;
	CREDITS_INFO *cred;

	loop = 0;

	FlatPoly(320, 96, 320 + CREDITS_WIDE, 360, &black);

	while (credits[loop].type != CRT_END)
	{
		cred = &credits[loop++];

		y_pos = cred->y_pos - scrollpos;

		if (y_pos >= 456)
			break;

		switch (cred->type)
		{
			case CRT_LOGO:
				DrawCreditsLogo((u_char)cred->flags, y_pos);
				break;
			case CRT_TEXT:
			case CRT_TEXT_GAP:
			case CRT_TEXT_BIG:
				if (cred->text)
					DrawCreditsText(cred->text, y_pos, (u_char)cred->flags);
				break;
			case CRT_LINE:
			case CRT_LINE_GAP:
			case CRT_LINE_BIG:
				line_num = cred->line_num;

				if (line_num != -1)
					DrawCreditsText(gLanguageStrings[line_num], y_pos, (u_char)cred->flags);
				break;
			case CRT_PICTURE:
				DrawCreditsTextWithTexture(cred, y_pos);
				break;
			default:
				continue;	// start over if invalid
		}
	}

	FlatPoly(320, 0, 320 + CREDITS_WIDE, 96, &black);

	DrawLogo();

	FlatPoly2(320, 96, 320 + CREDITS_WIDE, 32, &white, &black);
	FlatPoly2(320, 424, 320 + CREDITS_WIDE, 32, &black, &white);
	FlatPoly(320, 456, 320 + CREDITS_WIDE, 24, &black);
}

// [0x4ab2da] [D] [A]
// NOTE: [A] centred at CREDITS_MID, the middle of the right half on a screen wider than 4:3
static void DrawLogo(void)
{
	int h;
	int w;
	TEX_INFO *tinfo;

	tinfo = GetTextureDetails("DLOGO");

	if (tinfo)
	{
		w = tinfo->w ? tinfo->w : 256;
		h = tinfo->h ? tinfo->h : 256;

		TextPoly(CREDITS_MID - w / 2, 10, w, h, tinfo, -1);
	}
}

// [0x4ab370] [D] [A]
// A photo in a white box.
// NOTE: [A] centred at CREDITS_MID, the middle of the right half on a screen wider than 4:3
static void DrawCreditsTextWithTexture(CREDITS_INFO *cred, int y_pos)
{
	int h;
	int w;
	CREDITS_TSET *tset;

	if (y_pos >= -32 && GetCreditsTexture(cred))
	{
		tset = &tsets[(u_short)cred->tset];

		w = tset->tinfo->w ? tset->tinfo->w : 256;
		h = tset->tinfo->h ? tset->tinfo->h : 256;

		DrawBorderBox(CREDITS_MID - w / 2, y_pos, w, h);

		SwapPalette(31);

		TextPoly(CREDITS_MID - w / 2, y_pos, w, h, tset->tinfo, (u_short)tset->tset);
	}
}

// [0x4ab490] [D]
// Puts the entry's photo in the next of the shared texture sets, unless it is in one already.
static int GetCreditsTexture(CREDITS_INFO *cred)
{
	int i;

	for (i = 0; i < NUM_CREDITS_TSETS; i++)
	{
		if ((u_char)cred->flags == (u_short)tsets[i].index)
			return 1;
	}

	current_tset = (current_tset + 1) % NUM_CREDITS_TSETS;

	// NOTE: never true (0xFFFF against -1, as the original compares): a missing PERSON<n> texture
	// is not caught here
	if ((u_short)tsets[current_tset].tset == -1)
		return 0;

	tsets[current_tset].index = (u_char)cred->flags;
	tsets[current_tset].tinfo = GetTextureDetails(piccies[(u_char)cred->flags]);

	if (tsets[current_tset].tinfo == NULL)
		return 0;

	ReplaceTextureSet((u_short)tsets[current_tset].tset, TextureSlots[tsets[current_tset].tinfo->tset].data);

	cred->tset = current_tset;

	return 1;
}

// [0x4ab59c] [D]
static void DrawBorderBox(int xpos, int ypos, int width, int height)
{
	FlatPoly(xpos - 6, ypos - 6, width + 12, height + 12, &white);
}

// [0x4ab5ca] [D] [A]
// NOTE: [A] centred at CREDITS_MID, the middle of the right half on a screen wider than 4:3
static void DrawCreditsText(char *text, int y_pos, int color)
{
	if (y_pos >= 96 - GetFontHeight(0))
	{
		SetTextColour(pens[color].r, pens[color].g, pens[color].b);
		WriteScaledStringWithMode((float)CREDITS_MID, (float)y_pos, text, 0, 1);
	}
}

// [0x4ab648] [D] [A]
// NOTE: [A] centred at CREDITS_MID, the middle of the right half on a screen wider than 4:3
static void DrawCreditsLogo(int type, int y_pos)
{
	int height;
	int width;
	TEX_INFO *tinfo;

	if (y_pos + 64 >= 96)
	{
		if (type == 1)
			tinfo = GetTextureDetails("GT");
		else
			tinfo = GetTextureDetails("REFLEC");

		if (tinfo)
		{
			width = tinfo->w ? tinfo->w : 256;
			height = tinfo->h ? tinfo->h : 256;

			TextPoly(CREDITS_MID - width / 2, y_pos, width, height, tinfo, -1);
		}
	}
}

// [0x4ab705] [D]
// A flat rectangle in 640 x 480 coordinates.
static void FlatPoly(int xpos, int ypos, int width, int height, RGB *color)
{
	float x0, y0, x1, y1;
	float oow;
	GrVertex v[4];
	int i;

	oow = using_glide ? 1.0f : 0.015625f;

	if (ypos >= -512)
	{
		x0 = GetScaledXCoord((float)xpos);
		y0 = GetScaledYCoord((float)ypos);
		x1 = GetScaledXCoord((float)(width + xpos));
		y1 = GetScaledYCoord((float)(height + ypos));

		v[0].x = x0;
		v[0].y = y0;
		v[1].x = x1;
		v[1].y = y0;
		v[2].x = x1;
		v[2].y = y1;
		v[3].x = x0;
		v[3].y = y1;

		for (i = 0; i < 4; i++)
		{
			v[i].oow = oow;
			v[i].r = (float)color->r;
			v[i].g = (float)color->g;
			v[i].b = (float)color->b;
			v[i].a = 255.0f;
		}

		if (last_state != FLAT3)
		{
			grColorCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_ZERO, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXFALSE);
			last_state = FLAT3;
		}

		if (last_trans_on)
		{
			grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
			last_trans_on = 0;
		}

		grDrawPolygonVertexList(4, v);
	}
}

// [0x4ab9d7] [D]
// A rectangle with a vertical gradient that darkens what is behind it (white: to black): the
// fades at the top and bottom of the scrolling list.
static void FlatPoly2(int xpos, int ypos, int width, int height, RGB *top, RGB *bottom)
{
	float x0, y0, x1, y1;
	float oow;
	GrVertex v[4];
	int i;

	oow = using_glide ? 1.0f : 0.015625f;

	if (ypos >= -512)
	{
		x0 = GetScaledXCoord((float)xpos);
		y0 = GetScaledYCoord((float)ypos);
		x1 = GetScaledXCoord((float)(width + xpos));
		y1 = GetScaledYCoord((float)(height + ypos));

		v[0].x = x0;
		v[0].y = y0;
		v[1].x = x1;
		v[1].y = y0;
		v[2].x = x1;
		v[2].y = y1;
		v[3].x = x0;
		v[3].y = y1;

		for (i = 0; i < 4; i++)
		{
			RGB *color = i < 2 ? top : bottom;

			v[i].oow = oow;
			v[i].r = (float)color->r;
			v[i].g = (float)color->g;
			v[i].b = (float)color->b;
			v[i].a = 255.0f;
		}

		if (last_state != FLAT3)
		{
			grColorCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_ZERO, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXFALSE);
			last_state = FLAT3;
		}

		if (last_trans_on != 2)
		{
			grAlphaBlendFunction(GR_BLEND_ZERO, GR_BLEND_ONE_MINUS_SRC_COLOR, GR_BLEND_ONE, GR_BLEND_ZERO);
			last_trans_on = 2;
		}

		grDrawPolygonVertexList(4, v);
	}
}

// [0x4abcaa] [D]
// A texture in 640 x 480 coordinates; tset -1: the texture's own set.
static void TextPoly(int x, int y, int w, int h, TEX_INFO *tinfo, int tset)
{
	float u0, v0, u1, v1;
	float size_u, size_v;
	float x0, y0, x1, y1;
	float oow;
	GrVertex v[4];
	int i;

	if (y >= -512)
	{
		u0 = (float)tinfo->u + 0.5f;
		v0 = (float)tinfo->v + 0.5f;

		size_u = tinfo->w ? (float)tinfo->w - 0.5f : 255.5f;
		size_v = tinfo->h ? (float)tinfo->h - 0.5f : 255.5f;

		oow = using_glide ? 1.0f : 0.015625f;

		x0 = GetScaledXCoord((float)x);
		y0 = GetScaledYCoord((float)y);
		x1 = GetScaledXCoord((float)(w + x));
		y1 = GetScaledYCoord((float)(h + y));

		u1 = (u0 + size_u) * oow;
		v1 = (v0 + size_v) * oow;
		u0 *= oow;
		v0 *= oow;

		v[0].x = x0;
		v[0].y = y0;
		v[0].tmuvtx[0].sow = u0;
		v[0].tmuvtx[0].tow = v0;
		v[1].x = x1;
		v[1].y = y0;
		v[1].tmuvtx[0].sow = u1;
		v[1].tmuvtx[0].tow = v0;
		v[2].x = x1;
		v[2].y = y1;
		v[2].tmuvtx[0].sow = u1;
		v[2].tmuvtx[0].tow = v1;
		v[3].x = x0;
		v[3].y = y1;
		v[3].tmuvtx[0].sow = u0;
		v[3].tmuvtx[0].tow = v1;

		for (i = 0; i < 4; i++)
		{
			v[i].oow = oow;
			v[i].r = 255.0f;
			v[i].g = 255.0f;
			v[i].b = 255.0f;
			v[i].a = 255.0f;
		}

		if (last_state != TEXT3)
		{
			grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE);
			last_state = TEXT3;
		}

		if (tset == -1)
		{
			if (tinfo->tset != last_tset)
			{
				last_tset = tinfo->tset;

				grTexSource(GR_TMU0, TextureSlots[tinfo->tset].texture_ram_address, GR_MIPMAPLEVELMASK_BOTH, TextureSlots[tinfo->tset].TInfo);
			}
		}
		else if (tset != last_tset)
		{
			last_tset = tset;

			grTexSource(GR_TMU0, TextureSlots[tset].texture_ram_address, GR_MIPMAPLEVELMASK_BOTH, TextureSlots[tset].TInfo);
		}

		if (last_trans_on)
		{
			grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
			last_trans_on = 0;
		}

		grDrawPolygonVertexList(4, v);
	}
}
