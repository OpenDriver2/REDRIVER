/*
 * dr_globals.h - [A] globals whose owner file is not known yet
 *
 * Not an original file: globals in the linker's common area with several users and no IDA
 * @file mark. Move each to its owner's header once known.
 */

#ifndef DR_GLOBALS_H
#define DR_GLOBALS_H

#include "dr_types.h"

// [A] the lamps (headlights, sirens, AddLamp in object.c) reach LAMP_RANGE_SCALE times as far and
// fall off as the inverse square of the distance, 1 / (1 + LAMP_INV_SQUARE * x), x = d^2 / R^2 with
// R the new range, times (1 - x^2)^2 to reach 0 at R (AddLampLight in polydraw.c, the shader in
// glide_gl.c), instead of 1 - d^2 / r^2; comment out for the original falloff and range
#define OPENDRIVER_SMOOTH_LAMPS
#define LAMP_RANGE_SCALE	2		// the range of every lamp times this
#define LAMP_INV_SQUARE		8.0	// 16 / 2^2: half the light at a half of the original range

// [A] the lamps light each pixel in the OpenGL device's shader (glide_gl.c) instead of each vertex
// (AddLampLight in polydraw.c), with the falloff of OPENDRIVER_SMOOTH_LAMPS; comment out for the
// light per vertex
#define OPENDRIVER_PIXEL_LIGHTS

#endif // DR_GLOBALS_H
