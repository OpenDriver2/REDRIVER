/*
 * flat/detailoptions.c - motel room detail options: frontend/details.c with slider icons
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "motel\detailoptions.c=flat\detailoptions.c",
 * 0x417b00 - 0x417da5)
 */

#include "dr_types.h"
#include "dr_globals.h"
#include "map.h"
#include "frontend/details.h"
#include "flat/fudge.h"
#include "flat/detailoptions.h"

//------------------------------------------------------------------------------------------
// uninitialised data (0x600c90)

static TEX_INFO *sliderbar_tex;
static TEX_INFO *slider_tex;

// [0x417b00] [D]
int DoMotelDetailOptions(void)
{
	slider_tex = GetTextureDetails("SLIDER");
	sliderbar_tex = GetTextureDetails("SLIDERBA");

	SelectDetailOptions();
	return -1;
}

// [0x417b31] [D]
// The five sliders of the detail menu; selected (0 - 4) is drawn red, -1: none.
void DrawMotelDetailOptionsIcons(float peds, float view, float mirror, float traffic, float night, int selected)
{
	u_char mirror_g = 255, mirror_b = 255;
	u_char view_g = 255, view_b = 255;
	u_char peds_g = 255, peds_b = 255;
	u_char traffic_g = 255, traffic_b = 255;
	u_char night_g = 255, night_b = 255;

	if (selected == 0)
	{
		mirror_g = 0;
		mirror_b = 0;
	}

	if (selected == 1)
	{
		view_g = 0;
		view_b = 0;
	}

	if (selected == 2)
	{
		peds_g = 0;
		peds_b = 0;
	}

	if (selected == 3)
	{
		traffic_g = 0;
		traffic_b = 0;
	}

	if (selected == 4)
	{
		night_g = 0;
		night_b = 0;
	}

	if (slider_tex)
	{
		MyDrawIcon(467.0f, 310.0f, slider_tex, 255, mirror_g, mirror_b, 0, 0);
		MyDrawIcon(467.0f, 334.0f, slider_tex, 255, view_g, view_b, 0, 0);
		MyDrawIcon(467.0f, 358.0f, slider_tex, 255, peds_g, peds_b, 0, 0);
		MyDrawIcon(467.0f, 382.0f, slider_tex, 255, traffic_g, traffic_b, 0, 0);
		MyDrawIcon(467.0f, 406.0f, slider_tex, 255, night_g, night_b, 0, 0);
	}

	if (sliderbar_tex)
	{
		MyDrawSlider(467.0f, 310.0f, mirror, 1.0f, sliderbar_tex, 255, mirror_g, mirror_b, 0);
		MyDrawSlider(467.0f, 334.0f, view, 1.0f, sliderbar_tex, 255, view_g, view_b, 0);
		MyDrawSlider(467.0f, 358.0f, peds, 1.0f, sliderbar_tex, 255, peds_g, peds_b, 0);
		MyDrawSlider(467.0f, 382.0f, traffic, 1.0f, sliderbar_tex, 255, traffic_g, traffic_b, 0);
		MyDrawSlider(467.0f, 406.0f, night, 1.0f, sliderbar_tex, 255, night_g, night_b, 0);
	}
}
