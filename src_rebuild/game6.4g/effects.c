/*
 * effects.c - lighting and weather effects
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "effects.c", 0x4cbca0 - 0x4cfcd0).
 *
 * Night: DoNightSpecificStuff adds the player's headlight lamp, flashing siren lamps and the
 * interview car park lamps; lightning flashes the sky and the light level (DoLightning). Dawn and
 * dusk (DoDawn, DoDusk) step the sky colour (ChangeSkyRGB) and the light source towards the day /
 * night ones. Rain and snow share gRainList (100 drops placed round the camera, a free list of
 * drop numbers); rain also draws splashes on the road and the reflections of lights stored this
 * frame (StoreLightReflection, DisplayLightReflections). The 3D countdown words (3, 2, 1, GO …)
 * are text3d / DrawWord.
 */

#include <math.h>
#include <stdlib.h>

#include "dr_types.h"
#include "effects.h"
#include "checkpoint.h"
#include "dr_globals.h"
#include "asmmath.h"
#include "cops.h"
#include "debris.h"
#include "handling.h"
#include "map.h"
#include "MScript.h"
#include "objanim.h"
#include "pause.h"
#include "shell.h"
#include "sound.h"
#include "testmath.h"
#include "D3DGlide/glide_ptrs.h"
#include "Engine/3dfx/gfxinit.h"
#include "Engine/3dfx/object.h"
#include "Engine/3dfx/polydraw.h"
#include "Engine/3dfx/tset.h"
#include "gsound.h"
#include "ai.h"
#include "camera.h"
#include "glaunch.h"

#define MAX_DROPS			100
#define MAX_REFLECTIONS		100

// a COLLISION_PACKET type CheckForUnderCover looks for (a roof)
#define COLLISION_COVER		4

// light_colour[] types drawn additively in DisplayLightReflections
#define LIGHT_TYPE_ADD1		5
#define LIGHT_TYPE_ADD2		9

// poly types (PolySizes) whose colour ChangeSkyRGB and DrawWord change
#define POLY_SKY_A		4
#define POLY_SKY_B		5
#define POLY_WORD_A			50
#define POLY_WORD_B			51

typedef struct LIGHT_REFLECTION
{
	VECTOR4 position;
	char type;					// light_colour index
	char transparency;
	// screen corners: the light end (near the lamp) and the far end, three points
	XYPAIR field_14;
	XYPAIR field_1C;
	XYPAIR field_24;
	XYPAIR field_2C;
	XYPAIR field_34;
	float field_3C;				// oow of the light end
	float field_40;				// oow of the far end
	struct LIGHT_REFLECTION *field_44;	// next in the draw list
} LIGHT_REFLECTION;

int AddRainDrop(void);
int AddSnowDrop(void);
void RemoveDrop(int idx);
void DoLightning(void);
void SetLightSource(VECTOR4 *pos, float ambient);
void DisplaySplashes(void);

// initialised data (0x561f90)

char gNightRed = 128;
char gNightGreen = 128;
char gNightBlue = 128;

// uninitialised data (@effects.c marks)

int snow_gust_zadd;
LIGHT_REFLECTION gStoredReflections[MAX_REFLECTIONS];
int text3d[2];					// the word (DrawWord) and its frame, 0 none
int snow_gust_xadd;
int gLightFrame;				// siren flash, 0 - 127 in steps of 16
TEX_INFO *gLightReflectTexture;
int PlayerLightsOn;
RAIN_LIST gNormalRainList;
float Ls_Xstep;
int snow_gust_count;
TEX_INFO *gSplashTexture;
char gDuskGreen;
RAIN_LIST gMirroredRainList;
char gDawnRed;
int snow_gust_target_xadd;
int gDawnTimer;
int snow_gust_count_max;
float Ls_Ystep;
RAIN_LIST *gRainList;
int gLightningState;			// 1 while flashing
int snow_gust_target_zadd;
char gDawnBlue;
int gDoPlayerHeadlight;
int gNumLightReflections;
char gDawnGreen;
float DawnParam;				// how far the light source has moved, 0 - 1
int gLampDawnScale;
TEX_INFO *gSnowTexture;
char gDuskRed;
char gDuskBlue;
float Ls_Zstep;
int gLightningTimer;
float Amb_step;

// NOTE: dword_B9F5A0, in the linker's common area, only DrawCheckpoint uses it
static int checkpoint_angle;

// [0x4cbca0] [D]
void InitEffectGlobals(void)
{
	gLightFrame = 0;
	gDoPlayerHeadlight = 1;
	gDawnRed = 50;
	gDawnGreen = 50;
	gDawnBlue = 50;
	gDuskRed = 255;
	gDuskGreen = 255;
	gDuskBlue = 255;
	PlayerLightsOn = 1;
	num_pending_lamps = 0;
	gSplashTexture = NULL;
	gLightReflectTexture = NULL;
	gSnowTexture = NULL;
}

// [0x4cbd15] [D]
// Lamps of the night: the player's headlights, siren lights, the interview car park lamps and
// the pending lamps (AddPendingLamp).
void DoNightSpecificStuff(void)
{
	int i;
	int dx;
	int dz;
	int dist;
	int left;
	int right;
	int level;
	CAR_DATA *cp;
	CIV_STATE *civ;
	VECTOR4 pos;

	civ = civilian_ai_data;

	if (gDoLightning)
		DoLightning();

	if (!GamePaused)
		gLightFrame = (gLightFrame + 16) & 0x7F;

	PlayerLightsOn = 0;

	if (gDoPlayerHeadlight)
	{
		left = car_data[0].damage[0];
		right = car_data[0].damage[1];

		if (gMissionRecording)
		{
			right = 0;
			left = 0;
		}

		// a headlight per undamaged front corner
		if (left < 1000 || right < 1000)
		{
			cp = &car_data[0];
			level = (((right < 1000) + (left < 1000)) << 7) - 1;

			if (gMissionRecording)
				cp = &car_data[65];

			pos.vx = (float)cp->location.vx;
			pos.vy = (float)-cp->location.vy;
			pos.vz = (float)cp->location.vz;
			pos.vx = pos.vx + (float)(3500 * isin(cp->direction) / 4096);
			pos.vz = pos.vz + (float)(3500 * icos(cp->direction) / 4096);

			AddLamp(&pos, 3000, level, level, level);

			PlayerLightsOn = 1;
		}
	}

	// the player's police car siren
	if (gPlayerCarModelType == 10 && gPlayerSirenOn == 1 && car_data[0].where.m[1][1] > 100)
	{
		pos.vx = (float)car_data[0].location.vx;
		pos.vy = (float)-car_data[0].location.vy;
		pos.vz = (float)car_data[0].location.vz;

		dx = float_to_int(camera_position.vx - pos.vx);
		dz = float_to_int(camera_position.vz - pos.vz);

		if (dx < 0)
			dx = -dx;

		if (dz < 0)
			dz = -dz;

		if (dx >= dz)
			dist = dz / 2 + dx;
		else
			dist = dx / 2 + dz;

		if ((float)dist < far_clip)
		{
			if (gLightFrame >= 64)
				AddLamp(&pos, 1500, 0, 0, 4 * gLightFrame);
			else
				AddLamp(&pos, 2000, 4 * gLightFrame, 0, 0);
		}
	}

	if (gRealTimeLights)
	{
		// cop cars in pursuit
		for (i = 0; i < 64; i++, civ++)
		{
			cp = &car_data[i + 1];

			if (civ->IsCopCar && civ->model == 1 && civ->ctrlState == 4 && cp->where.m[1][1] >= 100 &&
				cop_data[(u_char)civ->IsCopCar - 1].health >= 0.01f)
			{
				pos.vx = (float)cp->location.vx;
				pos.vy = (float)-cp->location.vy;
				pos.vz = (float)cp->location.vz;

				dx = float_to_int(camera_position.vx - pos.vx);
				dz = float_to_int(camera_position.vz - pos.vz);

				if (dx < 0)
					dx = -dx;

				if (dz < 0)
					dz = -dz;

				if (dx >= dz)
					dist = dz / 2 + dx;
				else
					dist = dx / 2 + dz;

				if ((float)dist < far_clip)
				{
					if (gLightFrame >= 64)
						AddLamp(&pos, 1500, 0, 0, 4 * gLightFrame);
					else
						AddLamp(&pos, 2000, 4 * gLightFrame, 0, 0);
				}
			}
		}

		// the interview car park lamps, the first one flickering
		if (gDoInterview)
		{
			pos.vx = -15360.0f;
			pos.vy = -700.0f;
			pos.vz = -8192.0f;

			if ((rand() & 3) == 0)
				AddLamp(&pos, (rand() & 0x1FF) + 3000, 255, 220, 15);

			pos.vx = -15360.0f;
			pos.vy = -700.0f;
			pos.vz = -15360.0f;
			AddLamp(&pos, 3000, 255, 220, 15);

			pos.vx = -15360.0f;
			pos.vy = -700.0f;
			pos.vz = -21504.0f;
			AddLamp(&pos, 3000, 255, 220, 15);

			pos.vx = -6144.0f;
			pos.vy = -700.0f;
			pos.vz = -8192.0f;
			AddLamp(&pos, 3000, 255, 220, 15);

			pos.vx = -6144.0f;
			pos.vy = -700.0f;
			pos.vz = -15360.0f;
			AddLamp(&pos, 3000, 255, 220, 15);

			pos.vx = -6144.0f;
			pos.vy = -700.0f;
			pos.vz = -21504.0f;
			AddLamp(&pos, 3000, 255, 220, 15);
		}

		if (num_pending_lamps)
		{
			for (i = 0; i < num_pending_lamps; i++)
			{
				pos.vx = plamps[i].position.vx;
				pos.vy = plamps[i].position.vy;
				pos.vz = plamps[i].position.vz;

				AddLamp(&pos, plamps[i].range, (int)plamps[i].colour.red, (int)plamps[i].colour.green, (int)plamps[i].colour.blue);
			}

			num_pending_lamps = 0;
		}
	}
}

// [0x4cc3e2] [D]
// Draws a checkpoint model, spinning, additively.
// NOTE: the translation part of the matrix is left unset, as in the original
void DrawCheckpoint(CHECKPOINT_DATA *cp)
{
	int clip;
	VECTOR4 pos;
	MODEL *model;
	MATRIX mat;

	if (cp->model >= num_models_in_pack)
		return;

	model = model_pointers[cp->model];

	if (!model)
		return;

	pos.vx = (float)cp->position.vx;
	pos.vy = (float)cp->position.vy;
	pos.vz = (float)cp->position.vz;

	clip = FrustrumCheck(&pos, model->bounding_sphere);

	if (clip)
	{
		pos.vx = pos.vx - camera_position.vx;
		pos.vy = pos.vy - camera_position.vy;
		pos.vz = pos.vz - camera_position.vz;

		CurrentRotationMatrix = &inv_camera_matrix;
		MatrixRotate(&pos);

		mat.m[0][0] = 1.0f;
		mat.m[0][1] = 0.0f;
		mat.m[1][0] = 0.0f;
		mat.m[1][1] = 1.0f;
		mat.m[0][2] = 0.0f;
		mat.m[1][2] = 0.0f;
		mat.m[2][0] = 0.0f;
		mat.m[2][1] = 0.0f;
		mat.m[2][2] = 1.0f;

		checkpoint_angle = (checkpoint_angle + 30) & 0xFFF;
		RotMatrixY(&mat, checkpoint_angle);

		if (last_trans_on != 3)
		{
			grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ZERO);
			last_trans_on = 3;
		}

		last_trans_on = 1;

		if (clip == 1)
			DrawObject(model, &mat, &pos, 0);
		else if (clip == 2)
			DrawObject(model, &mat, &pos, 1);
		else
			DrawObject(model, &mat, &pos, 2);
	}
}

// [0x4cc56d] [D]
// Draws countdown word (0 THREE, 1 TWO, 2 ONE, 3 GO, 4 CHECKPOINT, 5 RACE OVER) at its frame:
// it flies in from the distance, tips over and fades.
// NOTE: the model is uninitialised for other words in the original; NULL here
void DrawWord(int word, int frame)
{
	int num_polys;
	int angle;
	VECTOR4 pos;
	u_char *poly;
	MODEL *model;
	MATRIX mat;
	u_char level;

	model = NULL;

	switch (word)
	{
		case 0:
			model = gThreeModel;
			break;
		case 1:
			model = gTwoModel;
			break;
		case 2:
			model = gOneModel;
			break;
		case 3:
			model = gGoModel;
			break;
		case 4:
			model = gCheckPointModel;
			break;
		case 5:
			model = gRaceOverModel;
			break;
	}

	if (!model)
		return;

	pos.vx = 0.0f;
	pos.vy = 0.0f;

	if (frame >= 30)
		pos.vz = 520.0f;
	else
		pos.vz = (float)(1000 - 16 * frame);

	if (frame < 5)
		level = 50 * frame;
	else if (frame <= 20)
		level = 255;
	else
		level = 255 - 25 * (frame - 20);

	mat = inv_camera_matrix;
	angle = ((frame << 6) - 1024) & 0xFFF;

	if (word == 5)
	{
		pos.vz = (float)(frame * frame + 520);
		level = 255;
		angle = (frame * frame + 3471) & 0xFFF;

		if (frame < 25)
		{
			angle = 0;
			level = 5 * frame;
		}

		if (frame > 30)
			level = 255 - (4 * frame - 120);

		if (frame > 93)
			level = 0;
	}

	RotMatrixX(&mat, angle);

	// the word's colour
	// NOTE: a poly of another type stops the walk (it is not stepped over), as in the original
	poly = (u_char *)model->poly_block;
	num_polys = model->num_polys;

	while (num_polys--)
	{
		if (*poly == POLY_WORD_A)
		{
			((float *)poly)[8] = (float)level;
			((float *)poly)[9] = (float)level;
			((float *)poly)[10] = (float)level;
			poly += PolySizes[*poly];
		}
		else if (*poly == POLY_WORD_B)
		{
			((float *)poly)[10] = (float)level;
			((float *)poly)[11] = (float)level;
			((float *)poly)[12] = (float)level;
			poly += PolySizes[*poly];
		}
	}

	if (last_trans_on != 3)
	{
		grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ZERO);
		last_trans_on = 3;
	}

	last_trans_on = 1;

	grDepthBufferMode(GR_DEPTHBUFFER_DISABLE);
	DrawObject(model, &mat, &pos, 1);
	grDepthBufferMode(GR_DEPTHBUFFER_WBUFFER);
}

// [0x4cc866] [D]
// Steps the countdown: 30 frames a word with a beep, then GO.
void Handle3dText(void)
{
	if (++text3d[1] == 2)
	{
		if (text3d[0] < 3)
			SimplePlaySound(16, 16, -1);
		else if (text3d[0] == 3)
			SimplePlaySound(17, 16, -1);
	}

	if (text3d[1] == 30)
	{
		if (text3d[0] < 3)
		{
			text3d[1] = 1;

			if (++text3d[0] == 3)
				mission.objective = 0x7FFF;
		}
		else if (text3d[0] == 3)
		{
			text3d[1] = 0;
		}
	}
}

// [0x4cc904] [D]
void Draw3dText(void)
{
	if (text3d[1])
		DrawWord(text3d[0], text3d[1]);
}

// [0x4cc927] [D]
void SetupLightning(void)
{
	if (gDoDawn)
	{
		gLampDawnScale = 255;
	}
	else if (gDoDusk)
	{
		gLampDawnScale = 0;
	}
	else if (gNight)
	{
		gLampDawnScale = 128;
	}
	else
	{
		gLightningTimer = ((u_char)rand() + 90) & 0xFE;
		gLightningState = 0;

		ChangeSkyRGB(50, 50, 20);
	}
}

// [0x4cc995] [D]
// Waits, then flashes the sky and the light for 30 frames with thunder (not under cover).
void DoLightning(void)
{
	int restore_sky;
	int under_cover;
	float x;
	float y;
	float z;
	u_char red;
	u_char green;
	u_char blue;

	restore_sky = 1;

	if (GamePaused)
		return;

	x = (float)car_data[0].location.vx;
	y = (float)car_data[0].location.vy;
	z = (float)car_data[0].location.vz;

	under_cover = CheckForUnderCover(camera_position.vx, camera_position.vy, camera_position.vz) ||
		CheckForUnderCover(x, y, z);

	if (gLightningState)
	{
		if (gLightningTimer)
		{
			gLightningTimer--;

			if (under_cover || (rand() & 7) != 1)
			{
				light_source = gLightSourceNight;
				ambient_light_level = gAmbientLightNight;
				ChangeLightLevel();
			}
			else
			{
				blue = gNightBlue + (rand() & 0x1F) + 96;
				green = gNightGreen + (rand() & 0x1F) + 96;
				red = gNightRed + (rand() & 0x1F) + 96;
				ChangeSkyRGB(red, green, blue);

				restore_sky = 0;

				if (rand() & 1)
				{
					light_source = gLightSourceDay;
					ambient_light_level = gAmbientLightDay;
					ChangeLightLevel();
				}
			}
		}
		else
		{
			if (!under_cover)
			{
				light_source = gLightSourceNight;
				ambient_light_level = gAmbientLightNight;
				ChangeLightLevel();
			}

			gLightningState = 0;
			gLightningTimer = ((unsigned short)rand() + 512) & 0x1FF;
		}

		if (restore_sky && gNight)
			ChangeSkyRGB(gNightRed, gNightGreen, gNightBlue);
	}
	else if (gLightningTimer)
	{
		gLightningTimer--;
	}
	else
	{
		gLightningState = 1;
		gLightningTimer = 30;

		TriggerThunder(30);
	}
}

// [0x4ccc77] [D]
// Moves the light source towards pos and the ambient level towards ambient over 500 steps.
void SetLightSource(VECTOR4 *pos, float ambient)
{
	DawnParam = 0.0f;

	Ls_Xstep = (pos->vx - light_source.vx) * 0.002f;
	Ls_Ystep = (pos->vy - light_source.vy) * 0.002f;
	Ls_Zstep = (pos->vz - light_source.vz) * 0.002f;

	Amb_step = (ambient - ambient_light_level) * 0.002f;
}

// [0x4ccce2] [D]
// Night to day, a step every 8 frames: the sky brightens, the light moves to the day one, then
// the lamps go off.
void DoDawn(void)
{
	int stage;

	stage = gDawnTimer / 30;

	if (!GamePaused && (game_frame & 7) == 0)
	{
		gDawnTimer++;

		if (stage < 1)
		{
			gDoLightFlare = 1;
			gNight = 1;
			SetStreetLampPolys(1);
			LightBillBoards(gNight);
			SetLightSource(&gLightSourceDay, gAmbientLightDay);
			return;
		}

		if (stage > 12 && gLampDawnScale > 2)
			gLampDawnScale -= 2;

		if (stage <= 18)
		{
			if (stage > 1 && gDawnRed != 255)
				gDawnRed++;

			if (stage > 3 && gDawnGreen != 255)
				gDawnGreen++;

			if (stage > 5 && gDawnBlue != 255)
				gDawnBlue++;

			ChangeSkyRGB(gDawnRed, gDawnGreen, gDawnBlue);

			if (stage > 7)
			{
				if (DawnParam < 1.0f)
				{
					light_source.vx = light_source.vx + Ls_Xstep;
					light_source.vy = light_source.vy + Ls_Ystep;
					light_source.vz = light_source.vz + Ls_Zstep;
					ambient_light_level = ambient_light_level + Amb_step;
					DawnParam = DawnParam + 0.002f;
				}

				ChangeLightLevel();
			}
		}
		else
		{
			gDoLightFlare = 0;
			gNight = 0;
			SetStreetLampPolys(0);
			LightBillBoards(gNight);
		}
	}
}

// [0x4cced5] [D]
// Day to night, a step every 4 frames: the light moves to the night one, the lamps come on and
// the sky darkens.
void DoDusk(void)
{
	int stage;

	stage = gDawnTimer / 30;

	if (!GamePaused && (game_frame & 3) == 0)
	{
		gDawnTimer++;

		if (stage < 1)
		{
			gDoLightFlare = 0;
			gNight = 0;
			SetStreetLampPolys(0);
			LightBillBoards(gNight);
			SetLightSource(&gLightSourceNight, gAmbientLightNight);
			return;
		}

		if (stage > 16)
		{
			gNight = 1;
			SetStreetLampPolys(1);
			LightBillBoards(gNight);

			if (gLampDawnScale < 253)
				gLampDawnScale += 2;
		}

		if (stage > 16)
			gDoLightFlare = 1;

		if (stage > 22 && gDuskRed > 50)
			gDuskRed--;

		if (stage > 25 && gDuskRed > 50)
			gDuskRed--;

		if (stage > 19 && gDuskGreen > 50)
			gDuskGreen--;

		if (stage > 16 && gDuskBlue > 50)
			gDuskBlue--;

		ChangeSkyRGB(gDuskRed, gDuskGreen, gDuskBlue);

		if (stage > 1)
		{
			if (DawnParam < 1.0f)
			{
				light_source.vx = light_source.vx + Ls_Xstep;
				light_source.vy = light_source.vy + Ls_Ystep;
				light_source.vz = light_source.vz + Ls_Zstep;
				ambient_light_level = ambient_light_level + Amb_step;
				DawnParam = DawnParam + 0.002f;
			}

			ChangeLightLevel();
		}
	}
}

// [0x4cd0df] [D]
// Sets the colour of the sky polys of the four sky models.
void ChangeSkyRGB(u_char red, u_char green, u_char blue)
{
	int num_polys;
	int i;
	MODEL *model;
	u_char *poly;

	for (i = 0; i < 4; i++)
	{
		model = model_pointers[i];
		num_polys = model->num_polys;

		for (poly = (u_char *)model->poly_block; num_polys--; poly += PolySizes[*poly])
		{
			if (*poly == POLY_SKY_B)
			{
				((float *)poly)[14] = (float)red;
				((float *)poly)[15] = (float)green;
				((float *)poly)[16] = (float)blue;
			}

			if (*poly == POLY_SKY_A)
			{
				((float *)poly)[11] = (float)red;
				((float *)poly)[12] = (float)green;
				((float *)poly)[13] = (float)blue;
			}
		}
	}
}

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

// [0x4cd437] [D]
// Empties a drop list and finds the splash and reflection textures (used one texel in).
void SetupRain(RAIN_LIST *rain)
{
	int i;

	for (i = 0; i < MAX_DROPS; i++)
	{
		rain->field_04[i] = i;
		rain->drops[i].index = i;
	}

	rain->count = 0;

	gSplashTexture = GetTextureDetails("SPLASH");

	if (gSplashTexture)
	{
		gSplashTexture->u++;
		gSplashTexture->v++;

		gSplashTexture->w += gSplashTexture->u - 1;
		gSplashTexture->h += gSplashTexture->v - 1;
	}

	gLightReflectTexture = GetTextureDetails("RFLECT");

	if (gLightReflectTexture)
	{
		gLightReflectTexture->w += gLightReflectTexture->u;
		gLightReflectTexture->h += gLightReflectTexture->v;

		gNumLightReflections = 0;
	}
}

// [0x4cd562] [D]
// A free drop number, -1 if all are used.
int AddRainDrop(void)
{
	if (gRainList->count >= MAX_DROPS)
		return -1;

	return gRainList->field_04[gRainList->count++];
}

// [0x4cd59a] [D]
int AddSnowDrop(void)
{
	if (gRainList->count >= MAX_DROPS)
		return -1;

	return gRainList->field_04[gRainList->count++];
}

// [0x4cd5d2] [D]
void RemoveDrop(int idx)
{
	gRainList->field_04[--gRainList->count] = idx;
}

// [0x4cd5fd] [D]
// Rain falls 50 a frame; a drop below the camera's view or off the ground goes. field_10 / 14
// keep the last screen position, so the drop is drawn as a streak.
void HandleRainDrops(void)
{
	int count;
	float ooz;
	float scale;
	VECTOR4 pos;
	RAIN_DROP *drop;

	if (GamePaused || gRainList->count == 0)
		return;

	CurrentRotationMatrix = &inv_camera_matrix;

	drop = gRainList->drops;
	count = gRainList->count;

	while (count--)
	{
		drop->position[1] += 50;

		if (drop->position[1] > 50)
		{
			RemoveDrop(drop->index);
		}
		else
		{
			pos.vx = (float)drop->position[0] - camera_position.vx;
			pos.vy = (float)drop->position[1] - camera_position.vy;
			pos.vz = (float)drop->position[2] - camera_position.vz;

			MatrixRotate(&pos);

			if (pos.vz < 50.0f || pos.vz >= 1536.0f)
			{
				RemoveDrop(drop->index);
			}
			else
			{
				if (drop->field_30 != 2)
					drop->field_30++;

				drop->field_10 = float_to_int(drop->field_20.vx);
				drop->field_14 = float_to_int(drop->field_20.vy);

				ooz = ooz_table[(int)pos.vz];
				scale = DIST * ooz;

				drop->field_20.vx = pos.vx * scale + screen_xmid - VERTEX_SNAPPER;
				drop->field_20.vy = pos.vy * scale + screen_ymid - VERTEX_SNAPPER;
				drop->field_20.vz = ooz;

				drop->field_31 = rand() & 0x1F;
			}
		}

		drop++;
	}
}

// [0x4cd7fe] [D]
// Draws the rain streaks (with the reflection texture) after the splashes.
// NOTE: gLightReflectTexture is not checked, as in the original
void DrawRainDrops(void)
{
	int count;
	int width;
	int oldDepthMask;
	float ooz;
	RAIN_DROP *drop;
	GrVertex v0;
	GrVertex v1;
	GrVertex v2;
	GrVertex v3;
	u_char level;

	if (gRainList->count == 0)
		return;

	width = display_width >> 8;
	oldDepthMask = EnableDepthMask(0);

	DisplaySplashes();

	if (last_trans_on != 3)
	{
		grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ZERO);
		last_trans_on = 3;
	}

	if (last_state != TEXT3)
	{
		grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE);
		last_state = TEXT3;
	}

	if (gLightReflectTexture->tset != last_tset)
	{
		last_tset = gLightReflectTexture->tset;
		grTexSource(GR_TMU0, TextureSlots[gLightReflectTexture->tset].texture_ram_address, GR_MIPMAPLEVELMASK_BOTH, TextureSlots[gLightReflectTexture->tset].TInfo);
	}

	drop = gRainList->drops;
	count = gRainList->count;

	while (count--)
	{
		if (drop->field_30 == 2)
		{
			ooz = drop->field_20.vz;

			// v0 v1: the drop now, v3 v2: where it was
			v0.x = drop->field_20.vx;
			v0.y = drop->field_20.vy;
			v1.oow = ooz;
			v0.oow = ooz;
			v1.x = v0.x + (float)width;
			v1.y = v0.y;

			v3.x = (float)drop->field_10;
			v3.y = (float)drop->field_14;
			v3.oow = ooz;
			v2.oow = ooz;
			v2.x = v3.x + (float)width;
			v2.y = v3.y;

			level = drop->field_31 + 40;

			v3.r = v3.g = v3.b = (float)(level / 2);
			v2.r = v2.g = v2.b = (float)(level / 2);
			v1.r = v1.g = v1.b = (float)level;
			v0.r = v0.g = v0.b = (float)level;

			v0.tmuvtx[0].sow = (float)gLightReflectTexture->u * ooz;
			v0.tmuvtx[0].tow = (float)gLightReflectTexture->v * ooz;
			v1.tmuvtx[0].tow = v0.tmuvtx[0].tow;
			v1.tmuvtx[0].sow = (float)gLightReflectTexture->w * ooz;
			v2.tmuvtx[0].sow = (float)gLightReflectTexture->w * ooz;
			v3.tmuvtx[0].sow = (float)gLightReflectTexture->u * ooz;
			v3.tmuvtx[0].tow = (float)gLightReflectTexture->h * ooz;
			v2.tmuvtx[0].tow = v3.tmuvtx[0].tow;

			guDrawTriangleWithClip(&v0, &v1, &v2);
			guDrawTriangleWithClip(&v0, &v2, &v3);
		}

		drop++;
	}

	EnableDepthMask(oldDepthMask);
}

// [0x4cdbe8] [D]
// Fills the free drops with rain round the camera (not under cover).
void AddRainDrops(void)
{
	int count;
	int idx;
	VECTOR4 pos;
	RAIN_DROP *drop;

	if (GamePaused)
		return;

	count = MAX_DROPS;

	CurrentRotationMatrix = &camera_matrix;

	while (count--)
	{
		idx = AddRainDrop();

		if (idx < 0)
			break;

		drop = &gRainList->drops[idx];

		pos.vx = (float)((rand() & 0x3FF) - 512);
		pos.vy = (float)(512 - (rand() & 0x3FF));
		pos.vz = (float)((rand() & 0xFFF) - 1024);

		MatrixRotate(&pos);

		pos.vx = pos.vx + camera_position.vx;
		pos.vy = pos.vy + camera_position.vy;
		pos.vz = pos.vz + camera_position.vz;

		if (CheckForUnderCover(pos.vx, pos.vy, pos.vz))
		{
			RemoveDrop(idx);
		}
		else
		{
			drop->position[0] = float_to_int(pos.vx);
			drop->position[1] = float_to_int(pos.vy);
			drop->position[2] = float_to_int(pos.vz);

			drop->field_30 = 0;
			drop->index = idx;
		}
	}
}

// [0x4cdd82] [D]
// 50 rain splashes a frame on the road round the camera.
void DisplaySplashes(void)
{
	int count;
	int size;
	float ooz;
	float scale;
	VECTOR4 pos;
	VECTOR4 world;
	GrVertex v0;
	GrVertex v1;
	GrVertex v2;
	GrVertex v3;
	u_char level;

	if (!gSplashTexture || GamePaused)
		return;

	SwapPalette(0);

	if (last_state != TEXT3)
	{
		grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE);
		last_state = TEXT3;
	}

	if (gSplashTexture->tset != last_tset)
	{
		last_tset = gSplashTexture->tset;
		grTexSource(GR_TMU0, TextureSlots[gSplashTexture->tset].texture_ram_address, GR_MIPMAPLEVELMASK_BOTH, TextureSlots[gSplashTexture->tset].TInfo);
	}

	if (last_trans_on != 3)
	{
		grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ZERO);
		last_trans_on = 3;
	}

	count = 50;

	while (count--)
	{
		CurrentRotationMatrix = &inv_camera_matrix;

		pos.vx = (float)((rand() & 0xFFF) - 2048);
		pos.vz = (float)((rand() & 0xFFF) - 2048);

		world.vx = pos.vx + camera_position.vx;
		world.vz = pos.vz + camera_position.vz;

		pos.vy = (float)-MapHeight(&world) - camera_position.vy - 50.0f;

		MatrixRotate(&pos);

		if (pos.vz >= 50.0f && pos.vz < 1536.0f)
		{
			ooz = ooz_table[(int)pos.vz];
			scale = DIST * ooz;

			v0.x = pos.vx * scale + screen_xmid - VERTEX_SNAPPER;
			v0.y = pos.vy * scale + screen_ymid - VERTEX_SNAPPER;
			v0.oow = ooz;

			if (v0.x > 0.0f && v0.x < 640.0f && v0.y > 0.0f && v0.y < 480.0f)
			{
				size = float_to_int(32.0f * scale);

				// v0 top left, v1 top right, v2 bottom right, v3 bottom left
				v1.x = v0.x + (float)size;
				v1.y = v0.y;
				v1.oow = ooz;
				v3.x = v0.x;
				v3.y = v0.y + (float)size;
				v3.oow = ooz;
				v2.x = v1.x;
				v2.y = v3.y;
				v2.oow = ooz;

				level = (rand() & 0x1F) + 40;

				v0.r = v0.g = v0.b = (float)level;
				v1.r = v1.g = v1.b = (float)level;
				v2.r = v2.g = v2.b = (float)level;
				v3.r = v3.g = v3.b = (float)level;

				// every other one mirrored
				if (count & 1)
				{
					v3.tmuvtx[0].sow = (float)gSplashTexture->u * ooz;
					v0.tmuvtx[0].sow = v3.tmuvtx[0].sow;
					v1.tmuvtx[0].tow = (float)gSplashTexture->v * ooz;
					v0.tmuvtx[0].tow = v1.tmuvtx[0].tow;
					v2.tmuvtx[0].sow = (float)gSplashTexture->w * ooz;
					v1.tmuvtx[0].sow = v2.tmuvtx[0].sow;
					v3.tmuvtx[0].tow = (float)gSplashTexture->h * ooz;
					v2.tmuvtx[0].tow = v3.tmuvtx[0].tow;
				}
				else
				{
					v1.tmuvtx[0].sow = (float)gSplashTexture->u * ooz;
					v2.tmuvtx[0].sow = v1.tmuvtx[0].sow;
					v0.tmuvtx[0].tow = (float)gSplashTexture->v * ooz;
					v1.tmuvtx[0].tow = v0.tmuvtx[0].tow;
					v3.tmuvtx[0].sow = (float)gSplashTexture->w * ooz;
					v0.tmuvtx[0].sow = v3.tmuvtx[0].sow;
					v2.tmuvtx[0].tow = (float)gSplashTexture->h * ooz;
					v3.tmuvtx[0].tow = v2.tmuvtx[0].tow;
				}

				guDrawTriangleWithClip(&v0, &v1, &v2);
				guDrawTriangleWithClip(&v0, &v2, &v3);
			}
		}
	}
}

// [0x4ce364] [D]
// Keeps a light for its reflection on the wet road this frame.
void StoreLightReflection(VECTOR4 *pos, char type, char transparency)
{
	if (gNumLightReflections != MAX_REFLECTIONS)
	{
		gStoredReflections[gNumLightReflections].position.vx = pos->vx;
		gStoredReflections[gNumLightReflections].position.vy = pos->vy;
		gStoredReflections[gNumLightReflections].position.vz = pos->vz;
		gStoredReflections[gNumLightReflections].type = type;
		gStoredReflections[gNumLightReflections++].transparency = transparency;
	}
}

// [0x4ce3e1] [D]
// Draws the stored light reflections: a streak on the road from below the light towards the
// camera, the light end wider; sorted by depth of the far end.
// NOTE: a reflection nearer than the list head is still put after it, as in the original
void DisplayLightReflections(void)
{
	int n;
	int height;
	int size;
	int dx;
	int dz;
	int oldDepthMask;
	float ooz;
	float ooz2;
	float scale;
	float fade;
	VECTOR4 pos;
	short transparency;
	LIGHT_REFLECTION *list;
	LIGHT_REFLECTION *ref;
	LIGHT_REFLECTION *at;
	LIGHT_INFO *light;
	GrVertex v0;
	GrVertex v1;
	GrVertex v2;
	GrVertex v3;
	GrVertex v4;

	if (!gLightReflectTexture)
		return;

	CurrentRotationMatrix = &inv_camera_matrix;

	list = NULL;
	n = gNumLightReflections;

	while (n--)
	{
		ref = &gStoredReflections[n];

		pos.vx = ref->position.vx;
		pos.vz = ref->position.vz;
		pos.vy = (float)(-MapHeight(&ref->position) - 28);

		height = float_to_int(-(ref->position.vy - pos.vy));

		pos.vx = pos.vx - camera_position.vx;
		pos.vy = pos.vy - camera_position.vy;
		pos.vz = pos.vz - camera_position.vz;

		MatrixRotate(&pos);

		if (pos.vz < 50.0f || pos.vz >= far_clip)
			continue;

		// the light end
		ooz = ooz_table[(int)pos.vz];
		scale = DIST * ooz;

		size = float_to_int((float)((height >> 3) + 80) * scale);

		fade = 0.7f - pos.vz / 8000.0f;

		if (fade < 0.3f)
			fade = 0.3f;

		ref->field_14.x = float_to_int(pos.vx * scale + screen_xmid - (float)(size >> 2) - VERTEX_SNAPPER);
		ref->field_14.y = float_to_int(pos.vy * scale + screen_ymid - VERTEX_SNAPPER);
		ref->field_3C = ooz;
		ref->field_1C.x = float_to_int((float)((size >> 1) + ref->field_14.x));
		ref->field_1C.y = float_to_int((float)ref->field_14.y);

		if (ref->field_14.x <= 0 || ref->field_14.x >= display_width || ref->field_14.y <= 0 || ref->field_14.y >= display_height)
			continue;

		// the far end, towards the camera
		dx = float_to_int((ref->position.vx - camera_position.vx) * fade);
		dz = float_to_int((ref->position.vz - camera_position.vz) * fade);

		pos.vx = (float)dx + camera_position.vx;
		pos.vz = (float)dz + camera_position.vz;
		pos.vy = (float)-MapHeight(&pos) - 28.0f;

		pos.vx = pos.vx - camera_position.vx;
		pos.vy = pos.vy - camera_position.vy;
		pos.vz = pos.vz - camera_position.vz;

		MatrixRotate(&pos);

		if (pos.vz < 50.0f || pos.vz >= far_clip)
			continue;

		ooz2 = ooz_table[(int)pos.vz];
		scale = DIST * ooz2;

		size = float_to_int((float)((height >> 3) + 20) * scale);

		ref->field_2C.x = float_to_int(pos.vx * scale + screen_xmid - (float)(size >> 3) - VERTEX_SNAPPER);
		ref->field_2C.y = float_to_int(pos.vy * scale + screen_ymid - VERTEX_SNAPPER);
		ref->field_24.x = (size >> 2) + ref->field_2C.x;
		ref->field_34.x = (size >> 3) + ref->field_2C.x;
		ref->field_34.y = ref->field_2C.y;
		ref->field_24.y = ref->field_34.y;
		ref->field_40 = ooz2;

		if (list)
		{
			for (at = list; at->field_44 && at->field_40 <= ref->field_40; at = at->field_44)
				;

			ref->field_44 = at->field_44;
			at->field_44 = ref;
		}
		else
		{
			list = ref;
			list->field_44 = NULL;
		}
	}

	if (!list)
		return;

	SwapPalette(0);

	if (last_state != TEXT3)
	{
		grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE);
		last_state = TEXT3;
	}

	if (gLightReflectTexture->tset != last_tset)
	{
		last_tset = gLightReflectTexture->tset;
		grTexSource(GR_TMU0, TextureSlots[gLightReflectTexture->tset].texture_ram_address, GR_MIPMAPLEVELMASK_BOTH, TextureSlots[gLightReflectTexture->tset].TInfo);
	}

	oldDepthMask = EnableDepthMask(0);

	while (list)
	{
		if (list->type == LIGHT_TYPE_ADD1 || list->type == LIGHT_TYPE_ADD2)
		{
			if (last_trans_on != 3)
			{
				grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ZERO);
				last_trans_on = 3;
			}
		}
		else if (last_trans_on != 5)
		{
			grAlphaBlendFunction(GR_BLEND_SRC_COLOR, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ZERO);
			last_trans_on = 5;
		}

		light = &light_colour[(u_char)list->type];
		transparency = (u_char)list->transparency;

		v4.r = v3.r = v2.r = v1.r = v0.r = (float)((transparency * light->red) >> 9);
		v4.g = v3.g = v2.g = v1.g = v0.g = (float)((transparency * light->green) >> 9);
		v4.b = v3.b = v2.b = v1.b = v0.b = (float)((transparency * light->blue) >> 9);

		// v0 v1: the light end, v2 v3 v4: the far end (v4 its middle)
		v0.x = (float)list->field_14.x;
		v0.y = (float)list->field_14.y;
		v0.oow = list->field_3C;
		v1.x = (float)list->field_1C.x;
		v1.y = (float)list->field_1C.y;
		v1.oow = list->field_3C;
		v2.x = (float)list->field_24.x;
		v2.y = (float)list->field_24.y;
		v2.oow = list->field_40;
		v3.x = (float)list->field_2C.x;
		v3.y = (float)list->field_2C.y;
		v3.oow = list->field_40;
		v4.x = (float)list->field_34.x;
		v4.y = (float)list->field_34.y;
		v4.oow = list->field_40;

		ooz = list->field_3C;
		ooz2 = list->field_40;

		v0.tmuvtx[0].sow = (float)gLightReflectTexture->u * ooz;
		v0.tmuvtx[0].tow = (float)gLightReflectTexture->v * ooz;
		v1.tmuvtx[0].tow = v0.tmuvtx[0].tow;
		v1.tmuvtx[0].sow = (float)gLightReflectTexture->w * ooz;
		v2.tmuvtx[0].sow = (float)gLightReflectTexture->w * ooz2;
		v3.tmuvtx[0].sow = (float)gLightReflectTexture->u * ooz2;
		v4.tmuvtx[0].tow = (float)gLightReflectTexture->h * ooz2;
		v3.tmuvtx[0].tow = v4.tmuvtx[0].tow;
		v2.tmuvtx[0].tow = v4.tmuvtx[0].tow;
		v4.tmuvtx[0].sow = (v2.tmuvtx[0].sow + v3.tmuvtx[0].sow) * 0.5f;

		guDrawTriangleWithClip(&v4, &v1, &v2);
		guDrawTriangleWithClip(&v0, &v4, &v3);
		guDrawTriangleWithClip(&v0, &v4, &v1);

		list = list->field_44;
	}

	EnableDepthMask(oldDepthMask);
}

// [0x4cefdb] [D]
// 1 if x y z is under a roof: inside a cover box of an indoor model in its cell.
int CheckForUnderCover(float x, float y, float z)
{
	int cellx;
	int cellz;
	int cell;
	int radius;
	int dx;
	int dz;
	int i;
	float rx;
	float rz;
	float tx;
	float bx;
	float bz;
	float xsize;
	float zsize;
	MATRIX *mat;
	COLLISION_PACKET *packet;
	CELL_DATA *cdp;
	CELL_OBJECT *cop;
	MODEL *model;

	cellx = float_to_int((x + (float)units_across_halved) * cell_recip);
	cellz = float_to_int((z + (float)units_down_halved) * cell_recip);

	for (cell = cell_ptrs[region_size * (cellz % region_size) + cellx % region_size +
			region_size * region_size * (((cellx / region_size) & 1) + 2 * ((cellz / region_size) & 1))];
		cell != -1; cell = cdp->next)
	{
		cdp = &cells[cell];
		cop = &cell_objects[cdp->num];
		model = model_pointers[cop->type];

		if ((model->flags2 & MODEL_FLAG_INDOORS) && model->collision_block)
		{
			radius = float_to_int(model->bounding_sphere2d);
			radius = radius * radius;

			dx = float_to_int((float)fabs(cop->pos.vx - x));
			dz = float_to_int((float)fabs(cop->pos.vz - z));

			if (radius > dz * dz + dx * dx)
			{
				packet = (COLLISION_PACKET *)(model->collision_block + 4);

				for (i = 0; i < *(int *)model->collision_block; i++, packet++)
				{
					if (packet->type != COLLISION_COVER)
						continue;

					// into the box's space
					rx = x - cop->pos.vx;
					rz = z - cop->pos.vz;
					mat = &(*matrix_mem)[cop->yang & 0xFFF];

					bx = rx * mat->m[0][0] + rz * mat->m[2][0] - (float)packet->xpos;
					bz = rx * mat->m[0][2] + rz * mat->m[2][2] - (float)packet->zpos;

					if (packet->yang)
					{
						mat = &(*matrix_mem)[(unsigned short)packet->yang & 0xFFF];
						tx = bx;
						bx = bx * mat->m[0][0] + bz * mat->m[2][0];
						bz = tx * mat->m[0][2] + bz * mat->m[2][2];
					}

					xsize = (float)(packet->xsize >> 1);
					zsize = (float)(packet->zsize >> 1);

					if (bx > -xsize && bx < xsize && bz > -zsize && bz < zsize)
						return 1;
				}
			}
		}
	}

	return 0;
}

// [0x4cf455] [D]
// Empties a drop list and finds the snow texture (no snow without it).
void SetupSnow(RAIN_LIST *snow)
{
	int i;

	snow_gust_count = 0;
	snow_gust_xadd = 0;
	snow_gust_zadd = 0;
	snow_gust_target_xadd = 0;
	snow_gust_target_zadd = 0;

	snow_gust_count_max = (u_char)rand();

	for (i = 0; i < MAX_DROPS; i++)
	{
		snow->field_04[i] = i;
		snow->drops[i].index = i;
	}

	snow->count = 0;

	gSnowTexture = GetTextureDetails("SNOW");

	if (gSnowTexture)
	{
		gSnowTexture->u++;
		gSnowTexture->v++;

		gSnowTexture->w += gSnowTexture->u - 1;
		gSnowTexture->h += gSnowTexture->v - 1;
	}
	else
	{
		gDoSnow = 0;
	}
}

// [0x4cf574] [D]
// Fills the free drops with snow flakes round the camera.
// NOTE: no snow is added anywhere while the camera is under cover (rain checks each drop), as
// in the original
void AddSnow(void)
{
	int count;
	int idx;
	VECTOR4 pos;
	RAIN_DROP *drop;

	if (GamePaused || CheckForUnderCover(camera_position.vx, camera_position.vy, camera_position.vz))
		return;

	count = MAX_DROPS;

	CurrentRotationMatrix = &camera_matrix;

	while (count--)
	{
		idx = AddSnowDrop();

		if (idx < 0)
			break;

		drop = &gRainList->drops[idx];

		pos.vx = (float)((rand() & 0x7FF) - 1024);
		pos.vy = (float)((rand() & 0x7FF) - 1024);
		pos.vz = (float)((rand() & 0x1FFF) - 1024);

		MatrixRotate(&pos);

		pos.vx = pos.vx + camera_position.vx;
		pos.vy = pos.vy + camera_position.vy;
		pos.vz = pos.vz + camera_position.vz;

		drop->position[0] = float_to_int(pos.vx);
		drop->position[1] = float_to_int(pos.vy);
		drop->position[2] = float_to_int(pos.vz);

		drop->index = idx;
	}
}

// [0x4cf705] [D]
// Snow falls 10 a frame, blown by gusts that change every 0 - 255 frames.
void HandleSnow(void)
{
	int count;
	float ooz;
	float scale;
	VECTOR4 pos;
	RAIN_DROP *drop;

	if (GamePaused)
		return;

	if (++snow_gust_count > snow_gust_count_max)
	{
		snow_gust_count_max = (u_char)rand();

		snow_gust_count = 0;
		snow_gust_target_xadd = (rand() & 0xF) - 8;
		snow_gust_target_zadd = (rand() & 0xF) - 8;
	}

	if ((game_frame & 3) == 0)
	{
		if (snow_gust_xadd < snow_gust_target_xadd)
			snow_gust_xadd++;
		else if (snow_gust_xadd > snow_gust_target_xadd)
			snow_gust_xadd--;

		if (snow_gust_zadd < snow_gust_target_zadd)
			snow_gust_zadd++;
		else if (snow_gust_zadd > snow_gust_target_zadd)
			snow_gust_zadd--;
	}

	if (gRainList->count == 0)
		return;

	CurrentRotationMatrix = &inv_camera_matrix;

	drop = gRainList->drops;
	count = gRainList->count;

	while (count--)
	{
		drop->position[0] += snow_gust_xadd;
		drop->position[1] += 10;
		drop->position[2] += snow_gust_zadd;

		if (drop->position[1] <= 50)
		{
			pos.vx = (float)drop->position[0] - camera_position.vx;
			pos.vy = (float)drop->position[1] - camera_position.vy;
			pos.vz = (float)drop->position[2] - camera_position.vz;

			MatrixRotate(&pos);

			if (pos.vz < 50.0f || pos.vz >= 4096.0f)
			{
				RemoveDrop(drop->index);
			}
			else
			{
				ooz = ooz_table[(int)pos.vz];
				scale = DIST * ooz;

				drop->field_20.vx = pos.vx * scale + screen_xmid - VERTEX_SNAPPER;
				drop->field_20.vy = pos.vy * scale + screen_ymid - VERTEX_SNAPPER;
				drop->field_20.vz = ooz;
			}
		}
		else
		{
			RemoveDrop(drop->index);
		}

		drop++;
	}
}

// [0x4cf986] [D]
// NOTE: gSnowTexture is not checked (SetupSnow turns snow off without it)
void DrawSnow(void)
{
	int count;
	int oldDepthMask;
	float ooz;
	float size;
	RAIN_DROP *drop;
	GrVertex v0;
	GrVertex v1;
	GrVertex v2;
	GrVertex v3;

	if (gRainList->count == 0)
		return;

	SwapPalette(0);

	if (last_state != TEXT3)
	{
		grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE);
		last_state = TEXT3;
	}

	if (gSnowTexture->tset != last_tset)
	{
		last_tset = gSnowTexture->tset;
		grTexSource(GR_TMU0, TextureSlots[gSnowTexture->tset].texture_ram_address, GR_MIPMAPLEVELMASK_BOTH, TextureSlots[gSnowTexture->tset].TInfo);
	}

	if (last_trans_on != 3)
	{
		grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ZERO);
		last_trans_on = 3;
	}

	oldDepthMask = EnableDepthMask(0);

	v3.b = v2.b = v1.b = v0.b = 255.0f;
	v3.g = v2.g = v1.g = v0.g = 255.0f;
	v3.r = v2.r = v1.r = v0.r = 255.0f;

	drop = gRainList->drops;
	count = gRainList->count;

	while (count--)
	{
		ooz = drop->field_20.vz;
		size = 16.0f * (DIST * ooz);

		// v0 top left, v1 top right, v2 bottom right, v3 bottom left
		v0.x = drop->field_20.vx;
		v0.y = drop->field_20.vy;
		v0.tmuvtx[0].sow = (float)gSnowTexture->u * ooz;
		v0.tmuvtx[0].tow = (float)gSnowTexture->v * ooz;

		v1.x = v0.x + size;
		v1.y = v0.y;
		v1.tmuvtx[0].sow = (float)gSnowTexture->w * ooz;
		v1.tmuvtx[0].tow = v0.tmuvtx[0].tow;

		v2.x = v1.x;
		v2.y = v0.y + size;
		v2.tmuvtx[0].sow = v1.tmuvtx[0].sow;
		v2.tmuvtx[0].tow = (float)gSnowTexture->h * ooz;

		v3.x = v0.x;
		v3.y = v2.y;
		v3.tmuvtx[0].sow = v0.tmuvtx[0].sow;
		v3.tmuvtx[0].tow = v2.tmuvtx[0].tow;

		v3.oow = ooz;
		v2.oow = ooz;
		v1.oow = ooz;
		v0.oow = ooz;

		guDrawTriangleWithClip(&v0, &v1, &v2);
		guDrawTriangleWithClip(&v0, &v2, &v3);

		drop++;
	}

	EnableDepthMask(oldDepthMask);
}
