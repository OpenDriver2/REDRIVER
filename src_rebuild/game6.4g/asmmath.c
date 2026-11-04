/*
 * asmmath.c - FPU control and assembler maths helpers
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "asmmath.asm"; the original is an assembler file,
 * rebuilt here as C with inline asm); partially translated.
 */

#include <string.h>
#include <intrin.h>

#include "asmmath.h"
#include "dr_globals.h"
#include "testmath.h"

// initialised data (0x57c330)
unsigned short FPUStatus = 0;
unsigned short OldFPUStatus = 0;
unsigned short TheFPUStatus = 0;
int FPUHasLamedUp = 0;

// NOTE: the assembler file's own variables (asmmath._T0 ... in IDA, packed without alignment
// between 0x57c342 and 0x57c3de); static here, so their layout is not kept
static float T0;
static float T1;
static float T2;
static float rot_sin;
static float rot_cos;
static float rot_negsin;
static float rot_negcos;
static float px;
static float py;
static int iz;
static LONG value;
static unsigned int TimePoint0;
static unsigned int TimeTaken;

// [0x52a1e0] [D]
// NOTE: assembly in the original (rdtsc)
void TIMESTART(void)
{
	TimePoint0 = (unsigned int)__rdtsc();
}

// [0x52a1f3] [D]
// Cycles since TIMESTART, less the 15 of the measurement itself.
// NOTE: assembly in the original (rdtsc)
int TIMESTOP(void)
{
	TimeTaken = (unsigned int)__rdtsc() - TimePoint0 - 15;
	return TimeTaken;
}

// [0x52a205] [D]
// NOTE: assembly in the original (rep movsd, then rep movsb for the rest); memcpy does the same
void CopyMem(void *dst, void *src, unsigned int size)
{
	memcpy(dst, src, size);
}

// [0x52a232] [D] [A]
// Resets the FPU and sets single precision with truncation (chop) rounding; the old control
// word is kept for FPURestore.
// NOTE: [A] with OPENDRIVER_NO_FPU_CHOP (not defined by premake now) the FPU is left alone and
// float_to_int truncates as fistp does under chop; float results then differ from the original
// now and then. The current CRT parses floats only in round-to-nearest mode (debug builds assert
// "unexpected rounding mode"): the scanf calls with %f switch to it (camera.c, compiler.c).
void FPUInit(void)
{
#ifndef OPENDRIVER_NO_FPU_CHOP
	__asm
	{
		push	eax
		finit
		wait
		fstcw	OldFPUStatus
		wait
		mov		ax, OldFPUStatus
		and		ax, 0FCFFh		; precision control: 24 bit
		or		ax, 0C00h		; rounding control: chop
		mov		FPUStatus, ax
		fldcw	FPUStatus
		wait
		pop		eax
	}
#endif
}

// [0x52a25c] [D] [A]
// Resets the FPU to the mode of FPUInit.
// NOTE: [A] does nothing with OPENDRIVER_NO_FPU_CHOP (FPUStatus is not set then)
void FPUMaintain(void)
{
	FPUHasLamedUp = 0;

#ifndef OPENDRIVER_NO_FPU_CHOP
	__asm
	{
		finit
		wait
		fldcw	FPUStatus
		wait
	}
#endif
}

// [0x52a274] [D]
// Whether something (a driver) changed the rounding mode away from chop.
void FPUStatusCheck(void)
{
	__asm
	{
		fstcw	TheFPUStatus
		wait
	}

	FPUHasLamedUp = (TheFPUStatus & 0xF00) != 0xC00;
}

// [0x52a2a5] [D]
void FPURestore(void)
{
	__asm finit
}

// [A]
// NOTE: not a function in the binary: an inline `fld; fistp INT_FLT; mov eax, INT_FLT` sequence
// (INT_FLT is a global int). Rounds with the FPU mode, which the game sets to chop (FPUInit) for
// its whole run: a truncation, like a C cast.
int float_to_int(float value)
{
#ifdef OPENDRIVER_NO_FPU_CHOP
	return (int)value;
#else
	int result;

	__asm fld value
	__asm fistp result

	return result;
#endif
}

// [0x52a2b1] [D]
// b = the transpose of a (the inverse of a rotation).
// NOTE: assembly in the original, as the rest of this file
void InvertMatrix(MATRIX *a, MATRIX *b)
{
	float m10;
	float m20;
	float m01;
	float m21;
	float m02;
	float m12;

	// read before writing: a and b may be the same matrix
	m10 = a->m[1][0];
	m20 = a->m[2][0];
	m01 = a->m[0][1];

	b->m[0][0] = a->m[0][0];
	b->m[0][1] = m10;
	b->m[0][2] = m20;
	b->m[1][0] = m01;

	m21 = a->m[2][1];
	m02 = a->m[0][2];
	m12 = a->m[1][2];

	b->m[1][1] = a->m[1][1];
	b->m[1][2] = m21;
	b->m[2][0] = m02;
	b->m[2][1] = m12;
	b->m[2][2] = a->m[2][2];
}

// [0x52a2fc] [D]
// c = a * b (rotation parts)
void MulMatrix(MATRIX *a, MATRIX *b, MATRIX *c)
{
	c->m[0][0] = a->m[0][0] * b->m[0][0] + a->m[0][1] * b->m[1][0] + a->m[0][2] * b->m[2][0];
	c->m[1][0] = a->m[1][0] * b->m[0][0] + a->m[1][1] * b->m[1][0] + a->m[1][2] * b->m[2][0];
	c->m[2][0] = a->m[2][0] * b->m[0][0] + a->m[2][1] * b->m[1][0] + a->m[2][2] * b->m[2][0];
	c->m[0][1] = a->m[0][0] * b->m[0][1] + a->m[0][1] * b->m[1][1] + a->m[0][2] * b->m[2][1];
	c->m[1][1] = a->m[1][0] * b->m[0][1] + a->m[1][1] * b->m[1][1] + a->m[1][2] * b->m[2][1];
	c->m[2][1] = a->m[2][0] * b->m[0][1] + a->m[2][1] * b->m[1][1] + a->m[2][2] * b->m[2][1];
	c->m[0][2] = a->m[0][0] * b->m[0][2] + a->m[0][1] * b->m[1][2] + a->m[0][2] * b->m[2][2];
	c->m[1][2] = a->m[1][0] * b->m[0][2] + a->m[1][1] * b->m[1][2] + a->m[1][2] * b->m[2][2];
	c->m[2][2] = a->m[2][0] * b->m[0][2] + a->m[2][1] * b->m[1][2] + a->m[2][2] * b->m[2][2];
}

// [0x52a3ea] [D]
// c = the transpose of a * b
void MulMatrixB(MATRIX *a, MATRIX *b, MATRIX *c)
{
	c->m[0][0] = a->m[0][0] * b->m[0][0] + a->m[0][1] * b->m[1][0] + a->m[0][2] * b->m[2][0];
	c->m[0][1] = a->m[1][0] * b->m[0][0] + a->m[1][1] * b->m[1][0] + a->m[1][2] * b->m[2][0];
	c->m[0][2] = a->m[2][0] * b->m[0][0] + a->m[2][1] * b->m[1][0] + a->m[2][2] * b->m[2][0];
	c->m[1][0] = a->m[0][0] * b->m[0][1] + a->m[0][1] * b->m[1][1] + a->m[0][2] * b->m[2][1];
	c->m[1][1] = a->m[1][0] * b->m[0][1] + a->m[1][1] * b->m[1][1] + a->m[1][2] * b->m[2][1];
	c->m[1][2] = a->m[2][0] * b->m[0][1] + a->m[2][1] * b->m[1][1] + a->m[2][2] * b->m[2][1];
	c->m[2][0] = a->m[0][0] * b->m[0][2] + a->m[0][1] * b->m[1][2] + a->m[0][2] * b->m[2][2];
	c->m[2][1] = a->m[1][0] * b->m[0][2] + a->m[1][1] * b->m[1][2] + a->m[1][2] * b->m[2][2];
	c->m[2][2] = a->m[2][0] * b->m[0][2] + a->m[2][1] * b->m[1][2] + a->m[2][2] * b->m[2][2];
}

// [0x52a4d8] [D]
// m = m * rotation about x (4096 to a turn)
// NOTE: the sine index is not masked, the cosine one is
void RotMatrixX(MATRIX *m, int angle)
{
	rot_cos = fsintab[(angle + 1024) & 0xFFF];
	rot_sin = fsintab[angle];

	rot_negsin = -rot_sin;
	rot_negcos = -rot_cos;

	T0 = m->m[0][1] * rot_cos + m->m[0][2] * rot_sin;
	T1 = m->m[1][1] * rot_cos + m->m[1][2] * rot_sin;
	T2 = m->m[2][1] * rot_cos + m->m[2][2] * rot_sin;

	m->m[0][2] = m->m[0][1] * rot_negsin + m->m[0][2] * rot_cos;
	m->m[1][2] = m->m[1][1] * rot_negsin + m->m[1][2] * rot_cos;
	m->m[2][2] = m->m[2][1] * rot_negsin + m->m[2][2] * rot_cos;

	m->m[0][1] = T0;
	m->m[1][1] = T1;
	m->m[2][1] = T2;
}

// [0x52a5d8] [D]
// m = m * rotation about y
void RotMatrixY(MATRIX *m, int angle)
{
	rot_cos = fsintab[(angle + 1024) & 0xFFF];
	rot_sin = fsintab[angle];

	rot_negsin = -rot_sin;
	rot_negcos = -rot_cos;

	T0 = m->m[0][0] * rot_cos + m->m[0][2] * rot_negsin;
	T1 = m->m[1][0] * rot_cos + m->m[1][2] * rot_negsin;
	T2 = m->m[2][0] * rot_cos + m->m[2][2] * rot_negsin;

	m->m[0][2] = m->m[0][0] * rot_sin + m->m[0][2] * rot_cos;
	m->m[1][2] = m->m[1][0] * rot_sin + m->m[1][2] * rot_cos;
	m->m[2][2] = m->m[2][0] * rot_sin + m->m[2][2] * rot_cos;

	m->m[0][0] = T0;
	m->m[1][0] = T1;
	m->m[2][0] = T2;
}

// [0x52a6d5] [D]
// m = m * rotation about z
void RotMatrixZ(MATRIX *m, int angle)
{
	rot_cos = fsintab[(angle + 1024) & 0xFFF];
	rot_sin = fsintab[angle];

	rot_negsin = -rot_sin;
	rot_negcos = -rot_cos;

	T0 = m->m[0][0] * rot_cos + m->m[0][1] * rot_sin;
	T1 = m->m[1][0] * rot_cos + m->m[1][1] * rot_sin;
	T2 = m->m[2][0] * rot_cos + m->m[2][1] * rot_sin;

	m->m[0][1] = m->m[0][0] * rot_negsin + m->m[0][1] * rot_cos;
	m->m[1][1] = m->m[1][0] * rot_negsin + m->m[1][1] * rot_cos;
	m->m[2][1] = m->m[2][0] * rot_negsin + m->m[2][1] * rot_cos;

	m->m[0][0] = T0;
	m->m[1][0] = T1;
	m->m[2][0] = T2;
}

// [0x52a7d2] [D]
// v = v * CurrentRotationMatrix (rows)
void MatrixRotate(VECTOR4 *v)
{
	MATRIX *m;
	float x;
	float y;
	float z;

	m = CurrentRotationMatrix;

	x = v->vx;
	y = v->vy;
	z = v->vz;

	v->vy = x * m->m[0][1] + y * m->m[1][1] + z * m->m[2][1];
	v->vx = x * m->m[0][0] + y * m->m[1][0] + z * m->m[2][0];
	v->vz = x * m->m[0][2] + y * m->m[1][2] + z * m->m[2][2];
}

// [0x52a832] [D]
// v = CurrentRotationMatrix * v (columns)
void MatrixRotateB(VECTOR4 *v)
{
	MATRIX *m;
	float x;
	float y;
	float z;

	m = CurrentRotationMatrix;

	x = v->vx;
	y = v->vy;
	z = v->vz;

	v->vy = x * m->m[1][0] + y * m->m[1][1] + z * m->m[1][2];
	v->vx = x * m->m[0][0] + y * m->m[0][1] + z * m->m[0][2];
	v->vz = x * m->m[2][0] + y * m->m[2][1] + z * m->m[2][2];
}

// [0x52a892] [D]
// Rotates (rows) and translates (x_trans ...) a model vertex in place.
void VertexRotate(VERTEX *v)
{
	MATRIX *m;
	float x;
	float y;
	float z;

	m = CurrentRotationMatrix;

	x = v->vx;
	y = v->vy;
	z = v->vz;

	v->vx = x * m->m[0][0] + y * m->m[1][0] + z * m->m[2][0] + x_trans;
	v->vy = x * m->m[0][1] + y * m->m[1][1] + z * m->m[2][1] + y_trans;
	v->vz = x * m->m[0][2] + y * m->m[1][2] + z * m->m[2][2] + z_trans;

	v->computed = current_vertex_computed_value;
}

// [0x52a918] [D]
// VertexRotate into tx / ty / vz, then the perspective projection into vx / vy (Glide: the
// screen position plus VERTEX_SNAPPER, taken off here again; see ResetViewport).
void VertexRotatePers(VERTEX *v)
{
	MATRIX *m;
	float x;
	float y;
	float z;
	float scale;

	m = CurrentRotationMatrix;

	x = v->vx;
	y = v->vy;
	z = v->vz;

	v->tx = x * m->m[0][0] + y * m->m[1][0] + z * m->m[2][0] + x_trans;
	v->ty = x * m->m[0][1] + y * m->m[1][1] + z * m->m[2][1] + y_trans;
	z = x * m->m[0][2] + y * m->m[1][2] + z * m->m[2][2] + z_trans;

	iz = (int)z;
	v->vz = z;
	v->computed = current_vertex_computed_value;

	v->oow = ooz_table[iz];

	scale = v->oow * DIST;
	px = v->tx * scale;

	v->vx = px + screen_xmid - VERTEX_SNAPPER;
	v->vy = scale * v->ty + screen_ymid - VERTEX_SNAPPER;
}

// [0x52aa07] [D]
// VertexRotatePers for Direct3D (d3d_screen_xmid / ymid, no snapper).
void VertexRotatePers_D3D(VERTEX *v)
{
	MATRIX *m;
	float x;
	float y;
	float z;
	float scale;

	m = CurrentRotationMatrix;

	x = v->vx;
	y = v->vy;
	z = v->vz;

	v->tx = x * m->m[0][0] + y * m->m[1][0] + z * m->m[2][0] + x_trans;
	v->ty = x * m->m[0][1] + y * m->m[1][1] + z * m->m[2][1] + y_trans;
	z = x * m->m[0][2] + y * m->m[1][2] + z * m->m[2][2] + z_trans;

	iz = (int)z;
	v->vz = z;
	v->computed = current_vertex_computed_value;

	v->oow = ooz_table[iz];

	scale = v->oow * DIST;
	px = v->tx * scale;

	v->vx = px + d3d_screen_xmid;
	v->vy = scale * v->ty + d3d_screen_ymid;
}

// [0x52aae1] [D]
// VertexRotate with the matrix columns.
void VertexRotate_D3D(VERTEX *v)
{
	MATRIX *m;
	float x;
	float y;
	float z;

	m = CurrentRotationMatrix;

	x = v->vx;
	y = v->vy;
	z = v->vz;

	v->vx = x * m->m[0][0] + y * m->m[0][1] + z * m->m[0][2] + x_trans;
	v->vy = x * m->m[1][0] + y * m->m[1][1] + z * m->m[1][2] + y_trans;
	v->vz = x * m->m[2][0] + y * m->m[2][1] + z * m->m[2][2] + z_trans;

	v->computed = current_vertex_computed_value;
}

// [0x52ab67] [D]
// VertexRotatePers with the matrix columns; tx / ty are not kept.
void VertexRotatePersB(VERTEX *v)
{
	MATRIX *m;
	float x;
	float y;
	float z;
	float scale;

	m = CurrentRotationMatrix;

	x = v->vx;
	y = v->vy;
	z = v->vz;

	px = x * m->m[0][0] + y * m->m[0][1] + z * m->m[0][2] + x_trans;
	py = x * m->m[1][0] + y * m->m[1][1] + z * m->m[1][2] + y_trans;
	z = x * m->m[2][0] + y * m->m[2][1] + z * m->m[2][2] + z_trans;

	iz = (int)z;
	v->vz = z;
	v->computed = current_vertex_computed_value;

	v->oow = ooz_table[iz];

	scale = v->oow * DIST;
	px = px * scale;

	v->vx = px + screen_xmid - VERTEX_SNAPPER;
	v->vy = scale * py + screen_ymid - VERTEX_SNAPPER;
}

// [0x52ac62] [D]
// Integer square root: a binary search between powers of two around the root.
LONG SquareRoot0(LONG a)
{
	LONG root;
	LONG lo;
	LONG hi;
	unsigned long bit;

	value = a;
	lo = 0;
	hi = 0;

	if (a <= 3)
		return 1;

	_BitScanReverse(&bit, a);

	if (bit < 4)
	{
		hi = a;
		root = a;
	}
	else
	{
		bit >>= 1;

		lo = 1 << bit;
		hi = 1 << (bit + 1);
		root = (hi + lo) >> 1;
	}

	for (;;)
	{
		root >>= 1;

		if (lo == root)
			return root;

		if (hi == root)
			return root - 1;

		if (value == root * root)
			return root;

		if (value > root * root)
		{
			lo = root;
			root += hi;
		}
		else
		{
			hi = root;
			root += lo;
		}
	}
}
