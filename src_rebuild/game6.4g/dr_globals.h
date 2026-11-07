/*
 * dr_globals.h - [A] globals whose owner file is not known yet, and the switches of our fixes
 *
 * Not an original file: globals in the linker's common area with several users and no IDA
 * @file mark. Move each to its owner's header once known.
 *
 * The OPENDRIVER_ switches of the [A] changes are all here; files that test one include this
 * header. OPENDRIVER_DEBUG comes from premake (debug builds).
 */

#ifndef DR_GLOBALS_H
#define DR_GLOBALS_H

#include "dr_types.h"

// [A] a sun of its own in each city, see ProcessMapLump (map.c); comment out for the original light
//#define OPENDRIVER_CITY_SUN

// [A] the XM songs of the PSX version (Music\MUSIC.BIN) during the game instead of the CD tracks,
// eax/psxmusic.c; comment out for the original music
#define OPENDRIVER_PSX_MUSIC

// [A] leaves the FPU as it is (FPUInit does nothing, float_to_int truncates as under chop), see
// asmmath.c; float results then differ from the original
//#define OPENDRIVER_NO_FPU_CHOP

// [A] the chase camera of Driver 2 (PlaceCameraFollowCarPSX in camera.c), with the head turning
// smoothly; comment out for the original camera
#define OPENDRIVER_D2_FOLLOW_CAMERA

// [A] the in-car camera of Driver 2 (PlaceCameraInCar): the head turns smoothly and looks behind
// from the middle of the car; comment out for the original camera
#define OPENDRIVER_D2_INCAR_CAMERA

// [A] the volume of the player's engine, in 1/100 dB (StartGameSounds, UpdateEngineRevs in
// gsound.c); comment out for the original volume (0)
#define OPENDRIVER_PLAYER_ENGINE_VOL	-4000

// [A] the volume of the player's skids, in 1/100 dB (CheckCarEffects in handling.c); comment out
// for the original volume (0)
#define OPENDRIVER_PLAYER_SKID_VOL		-1800

// [A] the volume of the player's horn and siren, in 1/100 dB (ProcessCarPad in handling.c);
// comment out for the original volume (0)
#define OPENDRIVER_PLAYER_HORN_VOL		-2000

// [A] the volume of the engines of the two closest cops, in 1/100 dB (ControlCopEngineRevs in
// gsound.c); comment out for the original volume (0)
#define OPENDRIVER_COP_ENGINE_VOL		-5000

// [A] fsintab made from int_sintab instead of read from FSIN.DAT (LoadSineTable in testmath.c),
// the same values; comment out to read the file
#define OPENDRIVER_SINE_TABLE

// [A] reckless driving is a felony only after the rear wheels spin or slide for a second
// (CheckPlayerMiscFelonies in felony.c), as in Driver 2, not on a press of the handbrake;
// comment out for the original
#define OPENDRIVER_D2_RECKLESS_FELONY

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
