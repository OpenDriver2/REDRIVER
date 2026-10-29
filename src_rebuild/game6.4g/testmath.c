/*
 * testmath.c - maths helpers and lookup tables
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "testmath.c"); partially translated.
 */

#include <math.h>

#include "dr_globals.h"
#include "asmmath.h"
#include "file.h"
#include "Engine/3dfx/object.h"
#include "testmath.h"

#define OOZ_TABLE_SIZE		56250

// NOTE: bss global in the linker's common area; defined here as testmath.c fills it
float ooz_table[OOZ_TABLE_SIZE];
float fsintab[5120];

// [0x527ca0] [D]
// FSIN.DAT: 4096 sines of a full turn; the table holds 1.25 turns so cosines can index it too
void LoadSineTable(void)
{
	FileLoad("FSIN.DAT", fsintab);
	CopyMem(&fsintab[4096], fsintab, 4096);
}

// [0x527ffd] [D]
// Rotation by ang (x, then y, then z; 4096 to a turn).
void CalcMatrixXYZ(MATRIX *matrix, SVECTOR *ang)
{
	float yc;
	float ys;
	float xc;
	float xs;
	float zc;
	float zs;

	xc = fsintab[ang->vx + 1024];
	xs = fsintab[ang->vx];
	yc = fsintab[ang->vy + 1024];
	ys = fsintab[ang->vy];
	zc = fsintab[ang->vz + 1024];
	zs = fsintab[ang->vz];

	matrix->m[0][0] = yc * zc;
	matrix->m[0][1] = -zs * yc;
	matrix->m[0][2] = ys;
	matrix->m[1][0] = xs * ys * zc + xc * zs;
	matrix->m[1][1] = xs * ys * -zs + xc * zc;
	matrix->m[1][2] = -xs * yc;
	matrix->m[2][0] = -ys * xc * zc + xs * zs;
	matrix->m[2][1] = -ys * xc * -zs + xs * zc;
	matrix->m[2][2] = xc * yc;
}

// [0x528277] [D]
// ooz_table[z] = screen_scale / z (perspective divide lookup).
void PreCalculateOOZ(void)
{
	int i;

	for (i = 0; i < OOZ_TABLE_SIZE; i++)
	{
		if (i)
			ooz_table[i] = screen_scale / (float)i;
		else
			ooz_table[0] = 0.0f;
	}
}

// [0x528371] [D]
void SetLightVector(float x, float y, float z)
{
	float len;
	float scale;

	// Normalized vector
	len = (float)sqrt(x * x + y * y + z * z);
	scale = 1.0f / len;

	light_source.vx = x * scale;
	light_source.vy = y * scale;
	light_source.vz = z * scale;
}
