/*
 * tsd.c - 8 bit software blitters for the frontend's virtual screen (vscreen, 640x480)
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "tsd.c", 0x52af90 - 0x52b1e4).
 *
 * NOTE: the original is hand-written assembly (register juggling, no C frame). These are C
 * equivalents of what it does, not instruction-exact copies. Texture pages are 256x256 bytes;
 * the assembly walks them with byte registers (u in dl, v in dh), so u and v wrap at 256 - kept.
 */

#include "dr_types.h"

#define VSCREEN_WIDTH		640

/* frontend/cpupoly.c */
extern int screen_pitch;
extern u_char chroma_index;
extern int pallookup[256];
extern u_char pallookup_hh[256][256];

// NOTE: tsd.c's own .data (byte_57C410): the assembly saves the start u there in DRAW8TRANS;
// not needed in C

// [0x52af90] [D]
// NOTE: unnamed in IDA (sub_52AF90); name guessed. Copies a w x h part of a texture page at
// (u, v) to the virtual screen, skipping chroma_index pixels.
void DRAW8CHROMA(u_char *tpage, u_char *screen, int w, int h, u_char u, u_char v)
{
	int x;
	u_char pixel;

	while (h > 0)
	{
		h--;

		for (x = 0; x < w; x++)
		{
			pixel = tpage[(v << 8) | (u_char)(u + x)];

			if (pixel != chroma_index)
				screen[x] = pixel;
		}

		v++;
		screen += VSCREEN_WIDTH;
	}
}

// [0x52afe0] [D]
// NOTE: unnamed in IDA (sub_52AFE0); name guessed. Like DRAW8CHROMA, but blends each texel with
// the screen pixel through pallookup_hh (50% translucency).
void DRAW8TRANS(u_char *tpage, u_char *screen, int w, int h, u_char u, u_char v)
{
	int x;

	while (h > 0)
	{
		h--;

		for (x = 0; x < w; x++)
			screen[x] = pallookup_hh[screen[x]][tpage[(v << 8) | (u_char)(u + x)]];

		v++;
		screen += VSCREEN_WIDTH;
	}
}

// [0x52b03d] [D]
// NOTE: unnamed in IDA (sub_52B03D); name guessed. Fills a w x h rectangle of the virtual
// screen with colour, 32 pixels at a time (so w is rounded up to a multiple of 32).
void FILL8(u_char *screen, int w, int h, u_char colour)
{
	int x;
	unsigned int fill;
	unsigned int *p;

	fill = colour | (colour << 8);
	fill |= fill << 16;

	while (h > 0)
	{
		h--;

		p = (unsigned int *)screen;

		for (x = 0; x < w; x += 32)
		{
			p[0] = fill;
			p[1] = fill;
			p[2] = fill;
			p[3] = fill;
			p[4] = fill;
			p[5] = fill;
			p[6] = fill;
			p[7] = fill;
			p += 8;
		}

		screen += VSCREEN_WIDTH;
	}
}

// [0x52b0a3] [D]
// Copies the 8 bit virtual screen to a 16 bit surface through pallookup.
// NOTE: width is not used; rows are always 640 pixels
void COPY8TO16(u_char *src, void *dst, int width, int height)
{
	int x;
	unsigned short *d;

	do
	{
		d = (unsigned short *)dst;

		for (x = 0; x < VSCREEN_WIDTH; x++)
			d[x] = pallookup[src[x]];

		src += VSCREEN_WIDTH;
		dst = (char *)dst + screen_pitch;
	}
	while (--height);
}

// [0x52b121] [D]
// Copies the 8 bit virtual screen to a 32 bit surface through pallookup.
// NOTE: width is not used; rows are always 640 pixels
void COPY8TO32(u_char *src, void *dst, int width, int height)
{
	int x;
	unsigned int *d;

	do
	{
		d = (unsigned int *)dst;

		for (x = 0; x < VSCREEN_WIDTH; x++)
			d[x] = pallookup[src[x]];

		src += VSCREEN_WIDTH;
		dst = (char *)dst + screen_pitch;
	}
	while (--height);
}
