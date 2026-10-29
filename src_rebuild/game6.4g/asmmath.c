/*
 * asmmath.c - FPU control and assembler maths helpers
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "asmmath.asm"; the original is an assembler file,
 * rebuilt here as C with inline asm); partially translated.
 */

#include <string.h>

#include "asmmath.h"

// initialised data (0x57c330)
unsigned short FPUStatus = 0;
unsigned short OldFPUStatus = 0;

// [0x52a205] [D]
// NOTE: assembly in the original (rep movsd, then rep movsb for the rest); memcpy does the same
void CopyMem(void *dst, void *src, unsigned int size)
{
	memcpy(dst, src, size);
}

// [0x52a232] [D] [A]
// Resets the FPU and sets single precision with truncation (chop) rounding; the old control
// word is kept for FPURestore.
// NOTE: [A] with OPENDRIVER_NO_FPU_CHOP (premake, all builds for now) the FPU is left alone: the
// current CRT parses and prints floats only in round-to-nearest mode (debug builds assert
// "unexpected rounding mode"). C float code is SSE2 and does not see the x87 mode; float_to_int
// truncates as fistp does under chop.
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
