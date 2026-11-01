/*
 * effects.c - lighting and weather effects
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "effects.c"); partially translated.
 */

#include <math.h>

#include "dr_types.h"
#include "effects.h"
#include "dr_globals.h"
#include "Engine/3dfx/object.h"

// [0x4cd1f9] [D]
// The light intensity of each world axis from light_source and ambient_light_level, for the
// axis aligned models (DrawObjectOSUseAxisChecks).
void ChangeLightLevel(void)
{
	if (light_source.vx >= 0.0f)
	{
		axis_light_intensity_right = (float)fabs(light_source.vx) + ambient_light_level;

		if (axis_light_intensity_right > 1.0f)
			axis_light_intensity_right = 1.0f;

		axis_light_intensity_left = ambient_light_level;
	}
	else
	{
		axis_light_intensity_left = (float)fabs(light_source.vx) + ambient_light_level;

		if (axis_light_intensity_left > 1.0f)
			axis_light_intensity_left = 1.0f;

		axis_light_intensity_right = ambient_light_level;
	}

	if (light_source.vy >= 0.0f)
	{
		axis_light_intensity_down = (float)fabs(light_source.vy) + ambient_light_level;

		if (axis_light_intensity_down > 1.0f)
			axis_light_intensity_down = 1.0f;

		axis_light_intensity_up = ambient_light_level;
	}
	else
	{
		axis_light_intensity_up = (float)fabs(light_source.vy) + ambient_light_level;

		if (axis_light_intensity_up > 1.0f)
			axis_light_intensity_up = 1.0f;

		axis_light_intensity_down = ambient_light_level;
	}

	if (light_source.vz >= 0.0f)
	{
		axis_light_intensity_front = (float)fabs(light_source.vz) + ambient_light_level;

		if (axis_light_intensity_front > 1.0f)
			axis_light_intensity_front = 1.0f;

		axis_light_intensity_back = ambient_light_level;
	}
	else
	{
		axis_light_intensity_back = (float)fabs(light_source.vz) + ambient_light_level;

		if (axis_light_intensity_back > 1.0f)
			axis_light_intensity_back = 1.0f;

		axis_light_intensity_front = ambient_light_level;
	}
}
