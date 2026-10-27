/*
 * frontend/cpupoly.c - software (CPU) drawing of the frontend into an 8 bit virtual screen
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "frontend\cpupoly.c"); partially translated.
 *
 * The frontend draws 8 bit texture pages into vscreen (640x480) with the tsd.c blitters; at the
 * end of the frame vscreen is converted through pallookup into the locked back buffer.
 */

#include <string.h>
#include <stdlib.h>
#include <limits.h>

#include "dr_types.h"

#include "../D3DGlide/d3dglide.h"

#define FE_POLY_END			0x4		// FE_POLY.flags: end of the list
#define FE_POLY_HIRES		0x8		// FE_POLY.flags: drawn at double size straight into the frame buffer

#define VSCREEN_WIDTH		640
#define VSCREEN_HEIGHT		480

/* D3DGlide/system.c */
extern FxBool grLfbLock(GrBuffer_t buffer, LFB_INFO *info);
extern FxBool grLfbUnlock(GrBuffer_t buffer);

/* tsd.c */
extern void COPY8TO16(u_char *src, void *dst, int width, int height);
extern void COPY8TO32(u_char *src, void *dst, int width, int height);
extern void DRAW8CHROMA(u_char *tpage, u_char *screen, int w, int h, u_char u, u_char v);
extern void DRAW8TRANS(u_char *tpage, u_char *screen, int w, int h, u_char u, u_char v);
extern void FILL8(u_char *screen, int w, int h, u_char colour);

/* asmmath.asm, map.c, debug.c */
extern void CopyMem(void *dst, void *src, unsigned int size);
extern TEX_INFO *GetTextureDetails(char *name);
extern void DisplayDebugStats(char *screen, char x, char y);

/* frontend/frontend.c */
extern FE_POLY *fe_cur_polys;
extern int fe_palette2[256];

extern TextureSet TextureSlots[];
extern int gDisableDebug;

void CPUDrawHiresPolys(char *lfb);
void CPUDrawHiresTexture(char *lfb, int x, int y, char *name);

//------------------------------------------------------------------------------------------
// data (0x57d9b0 - 0x5c89dd, this file's block)

void (*CPUCopy)(u_char *src, void *dst, int width, int height);
char vscreen[VSCREEN_HEIGHT][VSCREEN_WIDTH];
static char *lfb_ptr;			// NOTE: unnamed in IDA (dword_5C89B4); name guessed
int screen_depth;
int rbits;
int gbits;
int bbits;
int rshift;
int gshift;
int bshift;
u_char black_index;
int screen_pitch;
u_char chroma_index;

// NOTE: bss globals in the linker's common area; defined here as this file builds them
int pallookup[256];
u_char pallookup_hh[256][256];

// [0x407b00] [D]
// Reads the back buffer's pixel format and picks the matching vscreen copy routine.
int InitCPUPolyInterface(void)
{
	LFB_INFO info;

	memset(&info, 0, sizeof(info));

	info.info.size = sizeof(GrLfbInfo_t);

	if (!grLfbLock(GR_BUFFER_BACKBUFFER, &info))
		return 0;

	grLfbUnlock(GR_BUFFER_BACKBUFFER);

	if (info.depth == 16)
		CPUCopy = COPY8TO16;
	else
		CPUCopy = COPY8TO32;

	screen_pitch = info.info.strideInBytes;
	screen_depth = info.depth;
	rbits = info.rbits;
	gbits = info.gbits;
	bbits = info.bbits;
	rshift = info.rshift;
	gshift = info.gshift;
	bshift = info.bshift;

	return 1;
}

// [0x407bda] [D]
// Builds the screen colour table for a 256 colour palette, and the 50% blend table
// (pallookup_hh[screen][texel]); clears vscreen to black.
// NOTE: red and blue are swapped in the colour table (r uses the b bits/shift and vice versa),
// as in the binary
void CreateCPUPaletteLookup(RGB *pal, int *table, u_char chroma, u_char black)
{
	unsigned int i;
	unsigned int n;
	int j;
	int k;
	int r;
	int g;
	int b;
	int dist;
	int best_dist;
	u_char best;

	for (i = 0; i < 256; i++)
		table[i] = (pal[i].r >> bbits << bshift) | (pal[i].g >> gbits << gshift) | (pal[i].b >> rbits << rshift);

	for (j = 0; j < 256; j++)
	{
		for (k = 0; k < 256; k++)
		{
			if (k == chroma)
			{
				best = j;
			}
			else
			{
				// find the palette colour closest to the average of the two
				b = (pal[k].b >> 1) + (pal[j].b >> 1);
				g = (pal[k].g >> 1) + (pal[j].g >> 1);
				r = (pal[k].r >> 1) + (pal[j].r >> 1);

				best = 0;
				best_dist = INT_MAX;

				for (n = 0; n < 256; n++)
				{
					dist = 16 * abs(pal[n].r - r) + 16 * abs(pal[n].g - g) + 16 * abs(pal[n].b - b);

					if (dist < best_dist)
					{
						best_dist = dist;
						best = n;

						if (dist < 100)
							break;
					}
				}
			}

			pallookup_hh[j][k] = best;
		}
	}

	chroma_index = chroma;
	black_index = black;

	memset(vscreen, black, sizeof(vscreen));
}

// [0x407e57] [D]
void SetCPUPalette(int *table)
{
	CopyMem(pallookup, table, sizeof(pallookup));
}

// [0x407e72] [D]
// NOTE: unnamed in IDA (sub_407E72); name guessed. Nothing to do at the start of a frame.
int CPUStartOfFrame(void)
{
	return 1;
}

// [0x407e7c] [D]
// NOTE: unnamed in IDA (sub_407E7C); name guessed. Copies vscreen into the back buffer and
// draws the hires polys over it.
int CPUEndOfFrame(void)
{
	LFB_INFO info;

	info.info.size = sizeof(LFB_INFO);

	if (!grLfbLock(GR_BUFFER_BACKBUFFER, &info))
		return 0;

	lfb_ptr = info.info.lfbPtr;

	CPUCopy((u_char *)vscreen, info.info.lfbPtr, VSCREEN_WIDTH, VSCREEN_HEIGHT);
	CPUDrawHiresPolys(lfb_ptr);

	grLfbUnlock(GR_BUFFER_BACKBUFFER);
	return 1;
}

// [0x407ee8] [D]
// NOTE: unnamed in IDA (sub_407EE8); name guessed
void CPUDrawTexture(int x, int y, int tset, u_char u, u_char v, int w, int h)
{
	DRAW8CHROMA(TextureSlots[tset].data, (u_char *)&vscreen[y][x], w, h, u, v);
}

// [0x407f37] [D]
// NOTE: unnamed in IDA (sub_407F37); name guessed
void CPUDrawTransTexture(int x, int y, int tset, u_char u, u_char v, int w, int h)
{
	DRAW8TRANS(TextureSlots[tset].data, (u_char *)&vscreen[y][x], w, h, u, v);
}

// [0x407f86] [D]
// NOTE: unnamed in IDA (sub_407F86); name guessed. Fills with black whatever the colour.
void CPUFillRect(int x, int y, RGBA *colour, int w, int h)
{
	FILL8((u_char *)&vscreen[y][x], w & ~3, h, black_index);
}

// [0x407fc7] [D]
// NOTE: unnamed in IDA (sub_407FC7); name guessed
void CPUDisplayDebugStats(char x, char y)
{
	if (!gDisableDebug)
		DisplayDebugStats(vscreen[0], x, y);
}

// [0x407fea] [D]
// NOTE: unnamed in IDA (sub_407FEA); name guessed
void CPUDrawHiresPolys(char *lfb)
{
	FE_POLY *poly;

	poly = fe_cur_polys;

	if (poly)
	{
		while (!(poly->flags & FE_POLY_END))
		{
			if (poly->flags & FE_POLY_HIRES)
				CPUDrawHiresTexture(lfb, poly->x, poly->y, poly->text);

			poly++;
		}
	}
}

// [0x40805f] [D]
// NOTE: unnamed in IDA (sub_40805F); name guessed. Draws a texture at double size, centred
// on x, straight into the locked frame buffer (palette fe_palette2).
void CPUDrawHiresTexture(char *lfb, int x, int y, char *name)
{
	TEX_INFO *info;
	int w;
	int h;
	int i;
	int j;
	u_char *src;
	u_char *row;
	unsigned int *dst16;
	unsigned int *dst32;
	char *line;

	info = GetTextureDetails(name);

	if (!info)
		return;

	if (info->w)
		w = info->w;
	else
		w = 256;

	if (info->h)
		h = info->h;
	else
		h = 256;

	row = (u_char *)TextureSlots[info->tset].data + 256 * info->v + info->u;

	if (screen_depth == 16)
	{
		line = lfb + screen_pitch * y + 2 * (x - (w >> 1));

		for (i = 0; i < 2 * h; i++)
		{
			src = row;
			dst16 = (unsigned int *)line;

			for (j = 0; j < w; j++)
			{
				*dst16++ = fe_palette2[*src] | (fe_palette2[*src] << 16);
				src++;
			}

			if (i & 1)
				row += 256;

			line += screen_pitch;
		}
	}
	else
	{
		line = lfb + screen_pitch * y + 4 * (x - (w >> 1));

		for (i = 0; i < 2 * h; i++)
		{
			src = row;
			dst32 = (unsigned int *)line;

			for (j = 0; j < w; j++)
			{
				dst32[0] = fe_palette2[*src];
				dst32[1] = fe_palette2[*src];
				dst32 += 2;
				src++;
			}

			if (i & 1)
				row += 256;

			line += screen_pitch;
		}
	}
}
