/*
 * flat/fudge.c - motel room icons and sliders (Glide quads scaled from 640 x 480), depth switches
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "motel\fudge.c=flat\fudge.c", 0x419340 - 0x419d07)
 */

#include "dr_types.h"
#include "dr_globals.h"
#include "Engine/3dfx/gfxinit.h"
#include "Engine/3dfx/tset.h"
#include "../D3DGlide/glide_ptrs.h"
#include "flat/frontend.h"
#include "flat/fudge.h"

//------------------------------------------------------------------------------------------
// initialised data (0x550680)

int bOldMotelDepthMask = 1;

// [0x419340] [D] [A]
// An icon at (x, y) of the 640 x 480 screen, coloured and faded with the room; mirror flips it.
// NOTE: [A] wider than 4:3 the HUD keeps its proportions, see wide_aspect
void MyDrawIcon(float x, float y, TEX_INFO *tinfo, u_char r, u_char g, u_char b, char mirror, char blend)
{
	float oow;
	float red;
	float green;
	float blue;
	float u0;
	float v0;
	float w0;
	float h0;
	float sx;
	float sy;
	float sw;
	float sh;
	GrVertex verts[4];

	if (using_glide)
		oow = 1.0f;
	else
		oow = 1.0f / 64.0f;

	red = (float)r * gMotelFadeIntensity;
	green = (float)g * gMotelFadeIntensity;
	blue = (float)b * gMotelFadeIntensity;

	u0 = (float)tinfo->u + 0.5f;
	v0 = (float)tinfo->v + 0.5f;
	w0 = (float)tinfo->w - 0.5f;
	h0 = (float)tinfo->h - 0.5f;

	sx = x * motel_xscale + motel_xoffset;
	sy = y * motel_yscale;
	sw = w0 * motel_xscale;
	sh = h0 * motel_yscale;

	if (last_state != TEXT3)
	{
		grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE);
		last_state = TEXT3;
	}

	// 0 none, 1 multiply by inverse alpha, 2 subtractive, 3 additive
	switch (blend)
	{
		case 0:
			if (last_trans_on != 0)
			{
				grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
				last_trans_on = 0;
			}
			break;
		case 1:
			if (last_trans_on != 1)
			{
				grAlphaBlendFunction(GR_BLEND_ZERO, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO);
				last_trans_on = 1;
			}
			break;
		case 2:
			if (last_trans_on != 2)
			{
				grAlphaBlendFunction(GR_BLEND_ZERO, GR_BLEND_ONE_MINUS_SRC_COLOR, GR_BLEND_ONE, GR_BLEND_ZERO);
				last_trans_on = 2;
			}
			break;
		case 3:
			if (last_trans_on != 3)
			{
				grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ZERO);
				last_trans_on = 3;
			}
			break;
	}

	if (tinfo->tset != last_tset)
	{
		last_tset = tinfo->tset;
		grTexSource(GR_TMU0, TextureSlots[tinfo->tset].texture_ram_address, GR_MIPMAPLEVELMASK_BOTH, TextureSlots[tinfo->tset].TInfo);
	}

	verts[0].x = sx;
	verts[0].y = sy;
	verts[0].oow = oow;
	verts[0].r = red;
	verts[0].g = green;
	verts[0].b = blue;
	verts[0].a = 255.0f;

	verts[1].x = sx + sw;
	verts[1].y = sy;
	verts[1].oow = oow;
	verts[1].r = red;
	verts[1].g = green;
	verts[1].b = blue;
	verts[1].a = 255.0f;

	verts[2].x = verts[1].x;
	verts[2].y = sy + sh;
	verts[2].oow = oow;
	verts[2].r = red;
	verts[2].g = green;
	verts[2].b = blue;
	verts[2].a = 255.0f;

	verts[3].x = sx;
	verts[3].y = verts[2].y;
	verts[3].oow = oow;
	verts[3].r = red;
	verts[3].g = green;
	verts[3].b = blue;
	verts[3].a = 255.0f;

	if (mirror)
	{
		verts[0].tmuvtx[0].sow = (u0 + w0) * oow;
		verts[0].tmuvtx[0].tow = v0 * oow;
		verts[1].tmuvtx[0].sow = u0 * oow;
	}
	else
	{
		verts[0].tmuvtx[0].sow = u0 * oow;
		verts[0].tmuvtx[0].tow = v0 * oow;
		verts[1].tmuvtx[0].sow = (u0 + w0) * oow;
	}

	verts[1].tmuvtx[0].tow = verts[0].tmuvtx[0].tow;
	verts[2].tmuvtx[0].sow = verts[1].tmuvtx[0].sow;
	verts[2].tmuvtx[0].tow = (v0 + h0) * oow;
	verts[3].tmuvtx[0].sow = verts[0].tmuvtx[0].sow;
	verts[3].tmuvtx[0].tow = verts[2].tmuvtx[0].tow;

	grDrawPolygonVertexList(4, verts);
}

// [0x41985a] [D] [A]
// The texture cut to value / max of its width (and height scaled by max), for the volume bars.
// NOTE: [A] wider than 4:3 the HUD keeps its proportions, see wide_aspect
void MyDrawSlider(float x, float y, float value, float max, TEX_INFO *tinfo, u_char r, u_char g, u_char b, char blend)
{
	float oow;
	float red;
	float green;
	float blue;
	float u0;
	float v0;
	float w0;
	float h0;
	float sx;
	float sy;
	float sw;
	float sh;
	GrVertex verts[4];

	if (using_glide)
		oow = 1.0f;
	else
		oow = 1.0f / 64.0f;

	red = (float)r * gMotelFadeIntensity;
	green = (float)g * gMotelFadeIntensity;
	blue = (float)b * gMotelFadeIntensity;

	u0 = (float)tinfo->u + 0.5f;
	v0 = (float)tinfo->v + 0.5f;
	w0 = (float)tinfo->w * value - 0.5f;
	h0 = (float)tinfo->h * max - 0.5f;

	sx = x * motel_xscale + motel_xoffset;
	sy = y * motel_yscale;
	sw = w0 * motel_xscale;
	sh = h0 * motel_yscale;

	if (last_state != TEXT3)
	{
		grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE);
		last_state = TEXT3;
	}

	// 0 none, 1 multiply by inverse alpha, 2 subtractive, 3 additive
	switch (blend)
	{
		case 0:
			if (last_trans_on != 0)
			{
				grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
				last_trans_on = 0;
			}
			break;
		case 1:
			if (last_trans_on != 1)
			{
				grAlphaBlendFunction(GR_BLEND_ZERO, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO);
				last_trans_on = 1;
			}
			break;
		case 2:
			if (last_trans_on != 2)
			{
				grAlphaBlendFunction(GR_BLEND_ZERO, GR_BLEND_ONE_MINUS_SRC_COLOR, GR_BLEND_ONE, GR_BLEND_ZERO);
				last_trans_on = 2;
			}
			break;
		case 3:
			if (last_trans_on != 3)
			{
				grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ZERO);
				last_trans_on = 3;
			}
			break;
	}

	if (tinfo->tset != last_tset)
	{
		last_tset = tinfo->tset;
		grTexSource(GR_TMU0, TextureSlots[tinfo->tset].texture_ram_address, GR_MIPMAPLEVELMASK_BOTH, TextureSlots[tinfo->tset].TInfo);
	}

	verts[0].x = sx;
	verts[0].y = sy;
	verts[0].oow = oow;
	verts[0].r = red;
	verts[0].g = green;
	verts[0].b = blue;
	verts[0].a = 255.0f;
	verts[0].tmuvtx[0].sow = u0 * oow;
	verts[0].tmuvtx[0].tow = v0 * oow;

	verts[1].x = sx + sw;
	verts[1].y = sy;
	verts[1].oow = oow;
	verts[1].r = red;
	verts[1].g = green;
	verts[1].b = blue;
	verts[1].a = 255.0f;
	verts[1].tmuvtx[0].sow = (u0 + w0) * oow;
	verts[1].tmuvtx[0].tow = verts[0].tmuvtx[0].tow;

	verts[2].x = verts[1].x;
	verts[2].y = sy + sh;
	verts[2].oow = oow;
	verts[2].r = red;
	verts[2].g = green;
	verts[2].b = blue;
	verts[2].a = 255.0f;
	verts[2].tmuvtx[0].sow = verts[1].tmuvtx[0].sow;
	verts[2].tmuvtx[0].tow = (v0 + h0) * oow;

	verts[3].x = sx;
	verts[3].y = verts[2].y;
	verts[3].oow = oow;
	verts[3].r = red;
	verts[3].g = green;
	verts[3].b = blue;
	verts[3].a = 255.0f;
	verts[3].tmuvtx[0].sow = verts[0].tmuvtx[0].sow;
	verts[3].tmuvtx[0].tow = verts[2].tmuvtx[0].tow;

	grDrawPolygonVertexList(4, verts);
}

// [0x419cc8] [D]
void MotelDisableZ(void)
{
	bOldMotelDepthMask = EnableDepthMask(0);
	grDepthBufferMode(GR_DEPTHBUFFER_DISABLE);
}

// [0x419ce4] [D]
void MotelEnableZ(void)
{
	grDepthBufferMode(GR_DEPTHBUFFER_WBUFFER);
	EnableDepthMask(bOldMotelDepthMask);
	grDepthBufferFunction(GR_CMP_LEQUAL);
}
