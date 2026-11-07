/*
 * asmmath.h - globals and functions of asmmath.c used by other files
 */

#ifndef ASMMATH_H
#define ASMMATH_H

#include "dr_types.h"

// functions
extern void CopyMem(void *dst, void *src, unsigned int size);
extern void FPUInit(void);
extern void FPUMaintain(void);
extern void FPURestore(void);
extern void InvertMatrix(MATRIX *a, MATRIX *b);
extern void MatrixRotate(VECTOR4 *v);
extern void MatrixRotateB(VECTOR4 *v);
extern void MulMatrix(MATRIX *a, MATRIX *b, MATRIX *c);
extern void MulMatrixB(MATRIX *a, MATRIX *b, MATRIX *c);
extern void RotMatrixX(MATRIX *m, int angle);
extern void RotMatrixY(MATRIX *m, int angle);
extern void RotMatrixZ(MATRIX *m, int angle);
extern LONG SquareRoot0(LONG a);
extern void VertexRotate(VERTEX *v);
extern void VertexRotatePers(VERTEX *v);
extern void VertexRotatePers_D3D(VERTEX *v);
extern int float_to_int(float value);

#endif // ASMMATH_H
