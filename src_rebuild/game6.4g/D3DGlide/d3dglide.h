/*
 * D3DGlide/d3dglide.h - [A] what the devices share
 *
 * Not an original file.
 */

#ifndef D3DGLIDE_H
#define D3DGLIDE_H

#define FX_GLIDE_NO_FUNC_PROTO
#include <glide.h>

#include "dr_types.h"

// grLfbLock's info: a GrLfbInfo_t followed by the frame buffer's pixel format
// NOTE: not a named type in IDA (InitCPUPolyInterface passes a 28 byte buffer); name guessed
typedef struct LFB_INFO
{
	GrLfbInfo_t info;
	unsigned char depth;
	unsigned char rbits;		// GetBits() of the colour masks
	unsigned char gbits;
	unsigned char bbits;
	unsigned char rshift;		// GetShift() of the colour masks
	unsigned char gshift;
	unsigned char bshift;
	unsigned char pad;
} LFB_INFO;

#endif
