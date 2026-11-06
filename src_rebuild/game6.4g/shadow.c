/*
 * shadow.c - tyre tracks (skid marks) and car shadows
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "shadow.c", 0x51f7c0 - 0x5233db).
 *
 * Each wheel keeps a ring of num_tracks TYRE_TRACKs (track_buffer for the player, other_track_buffer
 * for the other car with tracks, the recorded car of a mission): AddTyreTrack adds a quad from
 * the wheel's last position to the new one; a run of them is drawn as one strip that fades in at
 * its first quad (surface 0) and out at its last (2). DrawShadow lays the CARSHAD texture on the
 * ground under the car: one quad, or (detailed shadows) a grid following the ground height.
 * Data (0x57821c - 0x578238): num_tracks, the tyre track options, the texture names.
 */

#include "dr_types.h"
#include "shadow.h"
#include "dr_globals.h"
#include "D3DGlide/glide_ptrs.h"
#include "asmmath.h"
#include "debris.h"
#include "handling.h"
#include "map.h"
#include "roadbits.h"
#include "shell.h"
#include "testmath.h"
#include "Engine/3dfx/gfxinit.h"
#include "Engine/3dfx/object.h"
#include "Engine/3dfx/polydraw.h"
#include "Engine/3dfx/tset.h"
#include "ai.h"
#include "camera.h"
#include "dr_cars.h"
#include "glaunch.h"

// a quad of a tyre track: p1 p2 the old position (p1 offset across), p3 p4 the new one
typedef struct TYRE_TRACK
{
	char type;					// 1: continues the quad before
	char field_01;
	short surface;				// 0 first quad of a run, 1 middle, 2 last
	u_char shade;				// 1 on grass (brown)
	VECTOR4 p1;
	VECTOR4 p2;
	VECTOR4 p3;
	VECTOR4 p4;
} TYRE_TRACK;

//------------------------------------------------------------------------------------------
// initialised data

int num_tracks = 256;
int gDoTexturedTyreTracks = 1;
int gDoTyreTracks = 1;

//------------------------------------------------------------------------------------------
// uninitialised data

// NOTE: names guessed (IDA stru_BDB998, stru_BDB9D8, stru_BDBA18, stru_BDBA58): the corners of the
// tyre track quad being drawn, the old end (p1, p2) and the new end (p3, p4)
static GrVertex track_v1;
static GrVertex track_v2;
static GrVertex track_v3;
static GrVertex track_v4;
POLYFT4 ShadowPolygon;
int OtherCont[4];
TYRE_TRACK *OtherLastSkid[4];
TYRE_TRACK *LastSkid[4];
IVECTOR tyre_save_positions[4];
VECTOR4 extra_car_tyre_save_positions[4];
TEX_INFO *gShadowTexture;
float shadow0_u;
float shadow0_v;
float shadow3_u;
float shadow3_v;
float shadow1_u;
float shadow1_v;
int Cont[4];
float shadow2_u;
float shadow2_v;
TEX_INFO *gTyreTexture;
IVECTOR tyre_new_positions[4];
TYRE_TRACK track_buffer[4][256];
TYRE_TRACK other_track_buffer[4][256];
float shadow_h;
float shadow_w;

// NOTE: bss globals in the linker's common area (0xc5ec50 - 0xc5ecc0); defined here as shadow.c
// is their only user
int other_tyre_track_offset[4];
int other_num_tyre_tracks[4];
int tyre_track_offset[4];		// the number of tracks in the ring
int num_tyre_tracks[4];			// the first track of the ring

// [0x51f7c0] [D] [A]
// NOTE: [A] the OpenGL device has as many tracks as the board
void InitTyreTracks(void)
{
	int i;

	if (using_glide || gUseGL)
		num_tracks = 256;
	else
		num_tracks = 128;

	for (i = 0; i < 4; i++)
	{
		other_num_tyre_tracks[i] = 0;
		num_tyre_tracks[i] = 0;
		other_tyre_track_offset[i] = 0;
		tyre_track_offset[i] = 0;
	}

	gTyreTexture = GetTextureDetails("SKID");

	if (gTyreTexture)
	{
		// one texel in at the top and the bottom; w and h become the right and bottom edges
		gTyreTexture->v++;
		gTyreTexture->h--;

		gTyreTexture->w += gTyreTexture->u;
		gTyreTexture->h += gTyreTexture->v;
	}
	else
	{
		gDoTexturedTyreTracks = 0;
	}
}

// [0x51f8bb] [D]
// The positions on the ground of the player car's wheels (25 outside the wheels).
void GetTyreTrackPositions(void)
{
	CAR_COSMETICS *car_cos;
	VECTOR4 WheelPos;
	int loop;

	float_body_mat[0].m[0][0] = (float)car_data[0].where.m[0][0] * 0.00024414062f;
	float_body_mat[0].m[1][0] = (float)car_data[0].where.m[1][0] * 0.00024414062f;
	float_body_mat[0].m[2][0] = (float)car_data[0].where.m[2][0] * 0.00024414062f;
	float_body_mat[0].m[0][1] = (float)car_data[0].where.m[0][1] * 0.00024414062f;
	float_body_mat[0].m[1][1] = (float)car_data[0].where.m[1][1] * 0.00024414062f;
	float_body_mat[0].m[2][1] = (float)car_data[0].where.m[2][1] * 0.00024414062f;
	float_body_mat[0].m[0][2] = (float)car_data[0].where.m[0][2] * 0.00024414062f;
	float_body_mat[0].m[1][2] = (float)car_data[0].where.m[1][2] * 0.00024414062f;
	float_body_mat[0].m[2][2] = (float)car_data[0].where.m[2][2] * 0.00024414062f;

	float_body_mat[0].m[0][0] = -float_body_mat[0].m[0][0];
	float_body_mat[0].m[0][1] = -float_body_mat[0].m[0][1];
	float_body_mat[0].m[0][2] = -float_body_mat[0].m[0][2];
	float_body_mat[0].m[2][0] = -float_body_mat[0].m[2][0];
	float_body_mat[0].m[2][1] = -float_body_mat[0].m[2][1];
	float_body_mat[0].m[2][2] = -float_body_mat[0].m[2][2];

	CurrentRotationMatrix = float_body_mat;
	car_cos = &car_cosmetics[car_data[0].model];

	for (loop = 0; loop < 4; loop++)
	{
		if (loop & 2)
			WheelPos.vx = (float)car_cos->wheelDisp[loop].vx + 25.0f;
		else
			WheelPos.vx = (float)car_cos->wheelDisp[loop].vx - 25.0f;

		WheelPos.vy = 0.0f;
		WheelPos.vz = (float)-car_cos->wheelDisp[(loop + 1) & 3].vz;

		MatrixRotateB(&WheelPos);

		WheelPos.vx = WheelPos.vx + (float)car_data[0].location.vx;
		WheelPos.vz = WheelPos.vz + (float)car_data[0].location.vz;

		tyre_new_positions[loop].vx = float_to_int(WheelPos.vx);
		tyre_new_positions[loop].vz = float_to_int(WheelPos.vz);
		tyre_new_positions[loop].vy = MapHeight(&WheelPos);
	}
}

// [0x51fbb0] [D]
void SetTyreTrackOldPositions(void)
{
	tyre_save_positions[0] = tyre_new_positions[0];
	tyre_save_positions[1] = tyre_new_positions[1];
	tyre_save_positions[2] = tyre_new_positions[2];
	tyre_save_positions[3] = tyre_new_positions[3];
}

// [0x51fc69] [D]
void SetOtherTyreTrackOldPositions(VECTOR4 *pos)
{
	extra_car_tyre_save_positions[0] = pos[0];
	extra_car_tyre_save_positions[1] = pos[1];
	extra_car_tyre_save_positions[2] = pos[2];
	extra_car_tyre_save_positions[3] = pos[3];
}

// [0x51fd1d] [D]
// GetTyreTrackPositions for any car, into pos[4].
void GetCarTyreTrackPositions(CAR_DATA *cp, VECTOR4 *pos)
{
	CAR_COSMETICS *car_cos;
	VECTOR4 WheelPos;
	int i;

	float_body_mat[0].m[0][0] = (float)cp->where.m[0][0] * 0.00024414062f;
	float_body_mat[0].m[1][0] = (float)cp->where.m[1][0] * 0.00024414062f;
	float_body_mat[0].m[2][0] = (float)cp->where.m[2][0] * 0.00024414062f;
	float_body_mat[0].m[0][1] = (float)cp->where.m[0][1] * 0.00024414062f;
	float_body_mat[0].m[1][1] = (float)cp->where.m[1][1] * 0.00024414062f;
	float_body_mat[0].m[2][1] = (float)cp->where.m[2][1] * 0.00024414062f;
	float_body_mat[0].m[0][2] = (float)cp->where.m[0][2] * 0.00024414062f;
	float_body_mat[0].m[1][2] = (float)cp->where.m[1][2] * 0.00024414062f;
	float_body_mat[0].m[2][2] = (float)cp->where.m[2][2] * 0.00024414062f;

	float_body_mat[0].m[0][0] = -float_body_mat[0].m[0][0];
	float_body_mat[0].m[0][1] = -float_body_mat[0].m[0][1];
	float_body_mat[0].m[0][2] = -float_body_mat[0].m[0][2];
	float_body_mat[0].m[2][0] = -float_body_mat[0].m[2][0];
	float_body_mat[0].m[2][1] = -float_body_mat[0].m[2][1];
	float_body_mat[0].m[2][2] = -float_body_mat[0].m[2][2];

	CurrentRotationMatrix = float_body_mat;

	car_cos = &car_cosmetics[cp->model];

	for (i = 0; i < 4; i++, pos++)
	{
		if (i & 2)
			WheelPos.vx = (float)car_cos->wheelDisp[i].vx + 25.0f;
		else
			WheelPos.vx = (float)car_cos->wheelDisp[i].vx - 25.0f;

		WheelPos.vy = 0.0f;
		WheelPos.vz = (float)-car_cos->wheelDisp[(i + 1) & 3].vz;

		MatrixRotateB(&WheelPos);

		WheelPos.vx = WheelPos.vx + (float)cp->location.vx;
		WheelPos.vz = WheelPos.vz + (float)cp->location.vz;

		pos->vx = WheelPos.vx;
		pos->vz = WheelPos.vz;
		pos->vy = (float)MapHeight(&WheelPos);
	}
}

// [0x51ffb7] [D]
// Grass thrown up by the spinning driven wheels on grass (no skid sound there).
void CheckAddGrass(void)
{
	IVECTOR pos;
	IVECTOR speed;
	int i;
	int num;

	num = 0;

	if (car_data[0].wheelspin)
		num = 16;

	if (!num)
		return;

	for (i = 0; i < 4; i++)
	{
		if (i == 0 || i == 2)
		{
			if (car_data[0].wheel[(i - 1) & 3].onGrass)
			{
				AllowSkidSound = 0;

				pos = tyre_new_positions[i];
				pos.vy -= 50;

				speed.vx = car_data[0].st.n.linearVelocity[0] / 4096;
				speed.vy = -634 * num;
				speed.vz = car_data[0].st.n.linearVelocity[2] / 4096;

				Setup_Sparks(&pos, &speed, num, 2);
			}
			else
			{
				AllowSkidSound = 1;
			}
		}
	}
}

// [0x5200ba] [D]
// Adds a quad (50 wide) from the wheel's last position to the new one to the player's tracks,
// with tyre smoke (grass on grass).
void AddTyreTrack(int wheel)
{
	IVECTOR *savetp;
	IVECTOR *newtp;
	TYRE_TRACK *tt_p;
	VECTOR4 SmokePosition;
	VECTOR4 SmokeDrift;
	VECTOR4 New1, New2, New3, New4;
	int track;
	int _dx, _dz;
	int dir;
	int canSmoke;
	float x, z;

	if (!gDoTyreTracks)
		return;

	savetp = &tyre_save_positions[wheel];
	newtp = &tyre_new_positions[wheel];

	_dx = savetp->vx - newtp->vx;
	_dz = savetp->vz - newtp->vz;

	dir = Arc_Tan(_dz, _dx);

	x = fsintab[(-dir & 0xFFF) + 1024] * 50.0f;
	z = fsintab[-dir & 0xFFF] * 50.0f;

	New1.vx = (float)savetp->vx;
	New2.vx = New1.vx;
	New1.vy = (float)-savetp->vy - 20.0f;
	New2.vy = New1.vy;
	New1.vz = (float)savetp->vz;
	New2.vz = New1.vz;

	New3.vx = (float)newtp->vx;
	New4.vx = New3.vx;
	New3.vy = (float)-newtp->vy - 20.0f;
	New4.vy = New3.vy;
	New3.vz = (float)newtp->vz;
	New4.vz = New3.vz;

	New1.vx = New1.vx + z;
	New1.vz = New1.vz + x;

	New3.vx = New3.vx + z;
	New3.vz = New3.vz + x;

	track = tyre_track_offset[wheel] + num_tyre_tracks[wheel];

	if (track >= num_tracks)
		track -= num_tracks;

	tt_p = &track_buffer[wheel][track];

	// a full ring drops its first track
	if (tyre_track_offset[wheel] == num_tracks)
	{
		if (++num_tyre_tracks[wheel] >= num_tracks)
			num_tyre_tracks[wheel] -= num_tracks;
	}
	else
	{
		tyre_track_offset[wheel]++;
	}

	if (Cont[wheel] == 1 && continuous_track == 1)
	{
		tt_p->type = continuous_track;
		tt_p->surface = 2;

		if (LastSkid[wheel] && LastSkid[wheel]->surface == 2)
			LastSkid[wheel]->surface = 1;
	}
	else
	{
		tt_p->type = 0;
		tt_p->surface = 0;
	}

	tt_p->p1 = New1;
	tt_p->p2 = New2;
	tt_p->p3 = New3;
	tt_p->p4 = New4;

	if (gDoSmoke)
	{
		GetSmokeDrift(&SmokeDrift, 0);
		MatrixRotateB(&SmokeDrift);
	}

	SmokePosition.vx = (float)tyre_new_positions[wheel].vx;
	SmokePosition.vy = (float)-tyre_new_positions[wheel].vy;
	SmokePosition.vz = (float)tyre_new_positions[wheel].vz;

	canSmoke = game_frame & 2;

	if (ROADS_GetRoadInfo(tyre_new_positions[wheel].vx, tyre_new_positions[wheel].vz) && model_pointers[RouteData.type] && (model_pointers[RouteData.type]->flags2 & MODEL_FLAG_GRASS))
	{
		if (!gDoRain && canSmoke)
			Setup_Smoke(&SmokePosition, 100, 500, 3, 0, &SmokeDrift, 0);

		tt_p->shade = 1;
		AllowSkidSound = 0;
	}
	else
	{
		tt_p->shade = 0;

		if (!gDoRain && canSmoke)
			Setup_Smoke(&SmokePosition, 100, 500, 2, 0, &SmokeDrift, 0);

		AllowSkidSound = 1;
	}

	Cont[wheel] = 1;
	LastSkid[wheel] = tt_p;
}

// [0x52055d] [D]
// AddTyreTrack for the other car (positions from GetCarTyreTrackPositions).
void AddOtherTyreTrack(int wheel, VECTOR4 *pos)
{
	VECTOR4 *savetp;
	TYRE_TRACK *tt_p;
	VECTOR4 SmokePosition;
	VECTOR4 SmokeDrift;
	VECTOR4 New1, New2, New3, New4;
	int track;
	int dir;
	int canSmoke;
	float x, z;

	if (!gDoTyreTracks)
		return;

	savetp = &extra_car_tyre_save_positions[wheel];

	dir = Arc_Tan(float_to_int(savetp->vz - pos->vz), float_to_int(savetp->vx - pos->vx));

	x = fsintab[(-dir & 0xFFF) + 1024] * 50.0f;
	z = fsintab[-dir & 0xFFF] * 50.0f;

	New1.vx = savetp->vx;
	New2.vx = New1.vx;
	New1.vy = -savetp->vy - 20.0f;
	New2.vy = New1.vy;
	New1.vz = savetp->vz;
	New2.vz = New1.vz;

	New3.vx = pos->vx;
	New4.vx = New3.vx;
	New3.vy = -pos->vy - 20.0f;
	New4.vy = New3.vy;
	New4.vz = pos->vz;

	New1.vx = New1.vx + z;
	New1.vz = New1.vz + x;

	New3.vx = New3.vx + z;
	New3.vz = New4.vz + x;

	track = other_tyre_track_offset[wheel] + other_num_tyre_tracks[wheel];

	if (track >= num_tracks)
		track -= num_tracks;

	tt_p = &other_track_buffer[wheel][track];

	if (other_tyre_track_offset[wheel] == num_tracks)
	{
		if (++other_num_tyre_tracks[wheel] >= num_tracks)
			other_num_tyre_tracks[wheel] -= num_tracks;
	}
	else
	{
		other_tyre_track_offset[wheel]++;
	}

	if (OtherCont[wheel] == 1 && other_continous_track == 1)
	{
		tt_p->type = other_continous_track;
		tt_p->surface = 2;

		if (OtherLastSkid[wheel] && OtherLastSkid[wheel]->surface == 2)
			OtherLastSkid[wheel]->surface = 1;
	}
	else
	{
		tt_p->type = 0;
		tt_p->surface = 0;
	}

	tt_p->p1 = New1;
	tt_p->p2 = New2;
	tt_p->p3 = New3;
	tt_p->p4 = New4;

	if (gDoSmoke)
	{
		GetSmokeDrift(&SmokeDrift, 0);
		MatrixRotateB(&SmokeDrift);
	}

	SmokePosition.vx = pos->vx;
	SmokePosition.vy = -pos->vy;
	SmokePosition.vz = pos->vz;

	canSmoke = game_frame & 2;

	if (ROADS_GetRoadInfo(float_to_int(pos->vx), float_to_int(pos->vz)) && model_pointers[RouteData.type] && (model_pointers[RouteData.type]->flags2 & MODEL_FLAG_GRASS))
	{
		if (!gDoRain && canSmoke)
			Setup_Smoke(&SmokePosition, 100, 500, 3, 0, &SmokeDrift, 0);

		tt_p->shade = 1;
	}
	else
	{
		tt_p->shade = 0;

		if (!gDoRain && canSmoke)
			Setup_Smoke(&SmokePosition, 100, 500, 2, 0, &SmokeDrift, 0);
	}

	OtherCont[wheel] = 1;
	OtherLastSkid[wheel] = tt_p;
}

// [0x520a0a] [D]
// Draws the player car tyre tracks within 6000 of the camera.
void DrawTyreTracks(void)
{
	MATRIX mat;
	TYRE_TRACK *tt_p;
	VECTOR4 v1, v2, v3, v4;
	float u, v, w, h;
	float ooz;
	float scale;
	int last_surface;
	int last_shade;
	int depth_mask;
	int broken;
	int count;
	int i;

	u = (float)gTyreTexture->u;
	v = (float)gTyreTexture->v;
	w = (float)gTyreTexture->w;
	h = (float)gTyreTexture->h;

	if (!gDoTyreTracks)
		return;

	SwapPalette(0);

	if (FlipX)
	{
		mat = inv_camera_matrix;

		mat.m[0][0] = -inv_camera_matrix.m[0][0];
		mat.m[1][0] = -inv_camera_matrix.m[1][0];
		mat.m[2][0] = -inv_camera_matrix.m[2][0];

		CurrentRotationMatrix = &mat;
	}
	else
	{
		CurrentRotationMatrix = &inv_camera_matrix;
	}

	depth_mask = EnableDepthMask(0);

	if (gDoTexturedTyreTracks)
	{
		if (last_state != TEXT3)
		{
			grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE);
			last_state = TEXT3;
		}

		if (gTyreTexture->tset != last_tset)
		{
			last_tset = gTyreTexture->tset;

			grTexSource(GR_TMU0, TextureSlots[gTyreTexture->tset].texture_ram_address, GR_MIPMAPLEVELMASK_BOTH, TextureSlots[gTyreTexture->tset].TInfo);
		}

		if (last_trans_on != 2)
		{
			grAlphaBlendFunction(GR_BLEND_ZERO, GR_BLEND_ONE_MINUS_SRC_COLOR, GR_BLEND_ONE, GR_BLEND_ZERO);
			last_trans_on = 2;
		}
	}
	else
	{
		if (last_state)
		{
			grColorCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_ZERO, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXFALSE);
			last_state = FLAT3;
		}

		if (last_trans_on != 2)
		{
			grAlphaBlendFunction(GR_BLEND_ZERO, GR_BLEND_ONE_MINUS_SRC_COLOR, GR_BLEND_ONE, GR_BLEND_ZERO);
			last_trans_on = 2;
		}
	}

	last_surface = -1;
	last_shade = -1;

	for (i = 0; i < 4; i++)
	{
		if (!tyre_track_offset[i])
			continue;

		// broken: the quad before was not drawn, so a continuing quad can not reuse its end
		broken = 1;

		tt_p = &track_buffer[i][num_tyre_tracks[i]];

		for (count = 0; count < tyre_track_offset[i]; count++, tt_p++)
		{
			if (tt_p == &track_buffer[i][num_tracks])
				tt_p = track_buffer[i];

			if (tt_p->type && !broken)
			{
				// continues the quad before: its end is this one's start
				broken = 1;

				v1 = v3;
				v2 = v4;

				track_v1 = track_v3;
				track_v2 = track_v4;

				v3.vx = tt_p->p3.vx - camera_position.vx;

				if (v3.vx > 6000.0f || v3.vx < -6000.0f)
					continue;

				v3.vz = tt_p->p3.vz - camera_position.vz;

				if (v3.vz > 6000.0f || v3.vz < -6000.0f)
					continue;

				v3.vy = tt_p->p3.vy - camera_position.vy;

				MatrixRotate(&v3);

				if (v3.vz <= 50.0f)
					continue;

				v4.vx = tt_p->p4.vx - camera_position.vx;
				v4.vy = tt_p->p4.vy - camera_position.vy;
				v4.vz = tt_p->p4.vz - camera_position.vz;

				MatrixRotate(&v4);

				if (v4.vz <= 50.0f)
					continue;

				broken = 0;

				ooz = ooz_table[(int)v3.vz];
				scale = ooz * DIST;

				track_v3.x = v3.vx * scale + screen_xmid - VERTEX_SNAPPER;
				track_v3.y = v3.vy * scale + screen_ymid - VERTEX_SNAPPER;
				track_v3.oow = ooz;

				ooz = ooz_table[(int)v4.vz];
				scale = ooz * DIST;

				track_v4.x = v4.vx * scale + screen_xmid - VERTEX_SNAPPER;
				track_v4.y = v4.vy * scale + screen_ymid - VERTEX_SNAPPER;
				track_v4.oow = ooz;
			}
			else
			{
				broken = 1;

				v1.vx = tt_p->p1.vx - camera_position.vx;

				if (v1.vx > 6000.0f || v1.vx < -6000.0f)
					continue;

				v1.vz = tt_p->p1.vz - camera_position.vz;

				if (v1.vz > 6000.0f || v1.vz < -6000.0f)
					continue;

				v1.vy = tt_p->p1.vy - camera_position.vy;

				MatrixRotate(&v1);

				if (v1.vz <= 50.0f || v1.vz > 45000.0f)
					continue;

				v3.vx = tt_p->p3.vx - camera_position.vx;

				if (v3.vx > 6000.0f || v3.vx < -6000.0f)
					continue;

				v3.vz = tt_p->p3.vz - camera_position.vz;

				if (v3.vz > 6000.0f || v3.vz < -6000.0f)
					continue;

				v3.vy = tt_p->p3.vy - camera_position.vy;

				MatrixRotate(&v3);

				if (v3.vz <= 50.0f || v3.vz > 45000.0f)
					continue;

				v2.vx = tt_p->p2.vx - camera_position.vx;
				v2.vy = tt_p->p2.vy - camera_position.vy;
				v2.vz = tt_p->p2.vz - camera_position.vz;

				MatrixRotate(&v2);

				if (v2.vz <= 50.0f || v2.vz > 45000.0f)
					continue;

				v4.vx = tt_p->p4.vx - camera_position.vx;
				v4.vy = tt_p->p4.vy - camera_position.vy;
				v4.vz = tt_p->p4.vz - camera_position.vz;

				MatrixRotate(&v4);

				if (v4.vz <= 50.0f || v4.vz > 45000.0f)
					continue;

				broken = 0;

				ooz = ooz_table[(int)v1.vz];
				scale = ooz * DIST;

				track_v1.x = v1.vx * scale + screen_xmid - VERTEX_SNAPPER;
				track_v1.y = v1.vy * scale + screen_ymid - VERTEX_SNAPPER;
				track_v1.oow = ooz;

				ooz = ooz_table[(int)v2.vz];
				scale = ooz * DIST;

				track_v2.x = v2.vx * scale + screen_xmid - VERTEX_SNAPPER;
				track_v2.y = v2.vy * scale + screen_ymid - VERTEX_SNAPPER;
				track_v2.oow = ooz;

				ooz = ooz_table[(int)v3.vz];
				scale = ooz * DIST;

				track_v3.x = v3.vx * scale + screen_xmid - VERTEX_SNAPPER;
				track_v3.y = v3.vy * scale + screen_ymid - VERTEX_SNAPPER;
				track_v3.oow = ooz;

				ooz = ooz_table[(int)v4.vz];
				scale = ooz * DIST;

				track_v4.x = v4.vx * scale + screen_xmid - VERTEX_SNAPPER;
				track_v4.y = v4.vy * scale + screen_ymid - VERTEX_SNAPPER;
				track_v4.oow = ooz;
			}

			// the colours (the blend darkens by them): grey, brown on grass; black at the open
			// end of a run
			if (tt_p->surface == 0)
			{
				if (last_surface != 0 || tt_p->shade != last_shade)
				{
					last_surface = 0;

					track_v1.r = track_v1.g = track_v1.b = 0.0f;
					track_v2.r = track_v2.g = track_v2.b = 0.0f;

					if (tt_p->shade)
					{
						track_v3.r = track_v4.r = 54.0f;
						track_v3.g = track_v4.g = 76.0f;
						track_v3.b = track_v4.b = 93.0f;
					}
					else
					{
						track_v3.r = track_v3.g = track_v3.b = 25.0f;
						track_v4.r = track_v4.g = track_v4.b = 25.0f;
					}

					last_shade = tt_p->shade;
				}
			}
			else if (tt_p->surface == 1)
			{
				if (last_surface != 1 || tt_p->shade != last_shade)
				{
					last_surface = 1;

					if (tt_p->shade)
					{
						track_v4.r = track_v3.r = track_v2.r = track_v1.r = 54.0f;
						track_v4.g = track_v3.g = track_v2.g = track_v1.g = 76.0f;
						track_v4.b = track_v3.b = track_v2.b = track_v1.b = 93.0f;
					}
					else
					{
						track_v4.r = track_v4.g = track_v4.b = 25.0f;
						track_v3.r = track_v3.g = track_v3.b = 25.0f;
						track_v2.r = track_v2.g = track_v2.b = 25.0f;
						track_v1.r = track_v1.g = track_v1.b = 25.0f;
					}

					last_shade = tt_p->shade;
				}
			}
			else if (last_surface != 2 || tt_p->shade != last_shade)
			{
				last_surface = 2;

				if (tt_p->shade)
				{
					track_v2.b = track_v1.b = 93.0f;
					track_v2.g = track_v1.g = 76.0f;
					track_v2.r = track_v1.r = 54.0f;
				}
				else
				{
					track_v2.r = track_v2.g = track_v2.b = 25.0f;
					track_v1.r = track_v1.g = track_v1.b = 25.0f;
				}

				track_v4.r = track_v4.g = track_v4.b = 0.0f;
				track_v3.r = track_v3.g = track_v3.b = 0.0f;

				last_shade = tt_p->shade;
			}

			track_v4.tmuvtx[0].sow = u * track_v4.oow;
			track_v4.tmuvtx[0].tow = v * track_v4.oow;
			track_v3.tmuvtx[0].sow = w * track_v3.oow;
			track_v3.tmuvtx[0].tow = v * track_v3.oow;
			track_v1.tmuvtx[0].sow = w * track_v1.oow;
			track_v1.tmuvtx[0].tow = h * track_v1.oow;
			track_v2.tmuvtx[0].sow = u * track_v2.oow;
			track_v2.tmuvtx[0].tow = h * track_v2.oow;

			guDrawTriangleWithClip(&track_v1, &track_v2, &track_v3);
			guDrawTriangleWithClip(&track_v2, &track_v3, &track_v4);
		}
	}

	EnableDepthMask(depth_mask);
}

// [0x52172e] [D]
// DrawTyreTracks for the other car tracks.
void DrawOtherTyreTracks(void)
{
	MATRIX mat;
	TYRE_TRACK *tt_p;
	VECTOR4 v1, v2, v3, v4;
	float u, v, w, h;
	float ooz;
	float scale;
	int last_surface;
	int last_shade;
	int depth_mask;
	int broken;
	int count;
	int i;

	u = (float)gTyreTexture->u;
	v = (float)gTyreTexture->v;
	w = (float)gTyreTexture->w;
	h = (float)gTyreTexture->h;

	if (!gDoTyreTracks)
		return;

	SwapPalette(0);

	if (FlipX)
	{
		mat = inv_camera_matrix;

		mat.m[0][0] = -inv_camera_matrix.m[0][0];
		mat.m[1][0] = -inv_camera_matrix.m[1][0];
		mat.m[2][0] = -inv_camera_matrix.m[2][0];

		CurrentRotationMatrix = &mat;
	}
	else
	{
		CurrentRotationMatrix = &inv_camera_matrix;
	}

	depth_mask = EnableDepthMask(0);

	if (gDoTexturedTyreTracks)
	{
		if (last_state != TEXT3)
		{
			grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE);
			last_state = TEXT3;
		}

		if (gTyreTexture->tset != last_tset)
		{
			last_tset = gTyreTexture->tset;

			grTexSource(GR_TMU0, TextureSlots[gTyreTexture->tset].texture_ram_address, GR_MIPMAPLEVELMASK_BOTH, TextureSlots[gTyreTexture->tset].TInfo);
		}

		if (last_trans_on != 2)
		{
			grAlphaBlendFunction(GR_BLEND_ZERO, GR_BLEND_ONE_MINUS_SRC_COLOR, GR_BLEND_ONE, GR_BLEND_ZERO);
			last_trans_on = 2;
		}
	}
	else
	{
		if (last_state)
		{
			grColorCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_ZERO, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXFALSE);
			last_state = FLAT3;
		}

		if (last_trans_on != 2)
		{
			grAlphaBlendFunction(GR_BLEND_ZERO, GR_BLEND_ONE_MINUS_SRC_COLOR, GR_BLEND_ONE, GR_BLEND_ZERO);
			last_trans_on = 2;
		}
	}

	last_surface = -1;
	last_shade = -1;

	for (i = 0; i < 4; i++)
	{
		if (!other_tyre_track_offset[i])
			continue;

		// broken: the quad before was not drawn, so a continuing quad can not reuse its end
		broken = 1;

		tt_p = &other_track_buffer[i][other_num_tyre_tracks[i]];

		for (count = 0; count < other_tyre_track_offset[i]; count++, tt_p++)
		{
			if (tt_p == &other_track_buffer[i][num_tracks])
				tt_p = other_track_buffer[i];

			if (tt_p->type && !broken)
			{
				// continues the quad before: its end is this one's start
				broken = 1;

				v1 = v3;
				v2 = v4;

				track_v1 = track_v3;
				track_v2 = track_v4;

				v3.vx = tt_p->p3.vx - camera_position.vx;

				if (v3.vx > 6000.0f || v3.vx < -6000.0f)
					continue;

				v3.vz = tt_p->p3.vz - camera_position.vz;

				if (v3.vz > 6000.0f || v3.vz < -6000.0f)
					continue;

				v3.vy = tt_p->p3.vy - camera_position.vy;

				MatrixRotate(&v3);

				if (v3.vz <= 50.0f)
					continue;

				v4.vx = tt_p->p4.vx - camera_position.vx;
				v4.vy = tt_p->p4.vy - camera_position.vy;
				v4.vz = tt_p->p4.vz - camera_position.vz;

				MatrixRotate(&v4);

				if (v4.vz <= 50.0f)
					continue;

				broken = 0;

				ooz = ooz_table[(int)v3.vz];
				scale = ooz * DIST;

				track_v3.x = v3.vx * scale + screen_xmid - VERTEX_SNAPPER;
				track_v3.y = v3.vy * scale + screen_ymid - VERTEX_SNAPPER;
				track_v3.oow = ooz;

				ooz = ooz_table[(int)v4.vz];
				scale = ooz * DIST;

				track_v4.x = v4.vx * scale + screen_xmid - VERTEX_SNAPPER;
				track_v4.y = v4.vy * scale + screen_ymid - VERTEX_SNAPPER;
				track_v4.oow = ooz;
			}
			else
			{
				broken = 1;

				v1.vx = tt_p->p1.vx - camera_position.vx;

				if (v1.vx > 6000.0f || v1.vx < -6000.0f)
					continue;

				v1.vz = tt_p->p1.vz - camera_position.vz;

				if (v1.vz > 6000.0f || v1.vz < -6000.0f)
					continue;

				v1.vy = tt_p->p1.vy - camera_position.vy;

				MatrixRotate(&v1);

				if (v1.vz <= 50.0f || v1.vz > 45000.0f)
					continue;

				v3.vx = tt_p->p3.vx - camera_position.vx;

				if (v3.vx > 6000.0f || v3.vx < -6000.0f)
					continue;

				v3.vz = tt_p->p3.vz - camera_position.vz;

				if (v3.vz > 6000.0f || v3.vz < -6000.0f)
					continue;

				v3.vy = tt_p->p3.vy - camera_position.vy;

				MatrixRotate(&v3);

				if (v3.vz <= 50.0f || v3.vz > 45000.0f)
					continue;

				v2.vx = tt_p->p2.vx - camera_position.vx;
				v2.vy = tt_p->p2.vy - camera_position.vy;
				v2.vz = tt_p->p2.vz - camera_position.vz;

				MatrixRotate(&v2);

				if (v2.vz <= 50.0f || v2.vz > 45000.0f)
					continue;

				v4.vx = tt_p->p4.vx - camera_position.vx;
				v4.vy = tt_p->p4.vy - camera_position.vy;
				v4.vz = tt_p->p4.vz - camera_position.vz;

				MatrixRotate(&v4);

				if (v4.vz <= 50.0f || v4.vz > 45000.0f)
					continue;

				broken = 0;

				ooz = ooz_table[(int)v1.vz];
				scale = ooz * DIST;

				track_v1.x = v1.vx * scale + screen_xmid - VERTEX_SNAPPER;
				track_v1.y = v1.vy * scale + screen_ymid - VERTEX_SNAPPER;
				track_v1.oow = ooz;

				ooz = ooz_table[(int)v2.vz];
				scale = ooz * DIST;

				track_v2.x = v2.vx * scale + screen_xmid - VERTEX_SNAPPER;
				track_v2.y = v2.vy * scale + screen_ymid - VERTEX_SNAPPER;
				track_v2.oow = ooz;

				ooz = ooz_table[(int)v3.vz];
				scale = ooz * DIST;

				track_v3.x = v3.vx * scale + screen_xmid - VERTEX_SNAPPER;
				track_v3.y = v3.vy * scale + screen_ymid - VERTEX_SNAPPER;
				track_v3.oow = ooz;

				ooz = ooz_table[(int)v4.vz];
				scale = ooz * DIST;

				track_v4.x = v4.vx * scale + screen_xmid - VERTEX_SNAPPER;
				track_v4.y = v4.vy * scale + screen_ymid - VERTEX_SNAPPER;
				track_v4.oow = ooz;
			}

			// the colours (the blend darkens by them): grey, brown on grass; black at the open
			// end of a run
			if (tt_p->surface == 0)
			{
				if (last_surface != 0 || tt_p->shade != last_shade)
				{
					last_surface = 0;

					track_v1.r = track_v1.g = track_v1.b = 0.0f;
					track_v2.r = track_v2.g = track_v2.b = 0.0f;

					if (tt_p->shade)
					{
						track_v3.r = track_v4.r = 54.0f;
						track_v3.g = track_v4.g = 76.0f;
						track_v3.b = track_v4.b = 93.0f;
					}
					else
					{
						track_v3.r = track_v3.g = track_v3.b = 25.0f;
						track_v4.r = track_v4.g = track_v4.b = 25.0f;
					}

					last_shade = tt_p->shade;
				}
			}
			else if (tt_p->surface == 1)
			{
				if (last_surface != 1 || tt_p->shade != last_shade)
				{
					last_surface = 1;

					if (tt_p->shade)
					{
						track_v4.r = track_v3.r = track_v2.r = track_v1.r = 54.0f;
						track_v4.g = track_v3.g = track_v2.g = track_v1.g = 76.0f;
						track_v4.b = track_v3.b = track_v2.b = track_v1.b = 93.0f;
					}
					else
					{
						track_v4.r = track_v4.g = track_v4.b = 25.0f;
						track_v3.r = track_v3.g = track_v3.b = 25.0f;
						track_v2.r = track_v2.g = track_v2.b = 25.0f;
						track_v1.r = track_v1.g = track_v1.b = 25.0f;
					}

					last_shade = tt_p->shade;
				}
			}
			else if (last_surface != 2 || tt_p->shade != last_shade)
			{
				last_surface = 2;

				if (tt_p->shade)
				{
					track_v2.b = track_v1.b = 93.0f;
					track_v2.g = track_v1.g = 76.0f;
					track_v2.r = track_v1.r = 54.0f;
				}
				else
				{
					track_v2.r = track_v2.g = track_v2.b = 25.0f;
					track_v1.r = track_v1.g = track_v1.b = 25.0f;
				}

				track_v4.r = track_v4.g = track_v4.b = 0.0f;
				track_v3.r = track_v3.g = track_v3.b = 0.0f;

				last_shade = tt_p->shade;
			}

			track_v4.tmuvtx[0].sow = u * track_v4.oow;
			track_v4.tmuvtx[0].tow = v * track_v4.oow;
			track_v3.tmuvtx[0].sow = w * track_v3.oow;
			track_v3.tmuvtx[0].tow = v * track_v3.oow;
			track_v1.tmuvtx[0].sow = w * track_v1.oow;
			track_v1.tmuvtx[0].tow = h * track_v1.oow;
			track_v2.tmuvtx[0].sow = u * track_v2.oow;
			track_v2.tmuvtx[0].tow = h * track_v2.oow;

			guDrawTriangleWithClip(&track_v1, &track_v2, &track_v3);
			guDrawTriangleWithClip(&track_v2, &track_v3, &track_v4);
		}
	}

	EnableDepthMask(depth_mask);
}

// [0x522452] [D]
void InitShadow(void)
{
	gShadowTexture = GetTextureDetails("CARSHAD");

	if (!gShadowTexture)
		return;

	ShadowPolygon.v0 = 0;
	ShadowPolygon.v1 = 1;
	ShadowPolygon.v2 = 2;
	ShadowPolygon.v3 = 3;
	ShadowPolygon.texture_set = gShadowTexture->tset;

	shadow0_u = (float)gShadowTexture->u + 1.5f;
	shadow0_v = (float)gShadowTexture->v + 1.5f;
	shadow1_u = (float)(gShadowTexture->w + gShadowTexture->u) - 0.5f;
	shadow1_v = (float)gShadowTexture->v + 1.5f;
	shadow2_u = (float)(gShadowTexture->w + gShadowTexture->u) - 0.5f;
	shadow2_v = (float)(gShadowTexture->h + gShadowTexture->v) - 0.5f;
	shadow3_u = (float)gShadowTexture->u + 1.5f;
	shadow3_v = (float)(gShadowTexture->h + gShadowTexture->v) - 0.5f;

	shadow_w = shadow1_u - shadow0_u;
	shadow_h = shadow2_v - shadow0_v;

	ShadowPolygon.r = 255.0f;
	ShadowPolygon.g = 255.0f;
	ShadowPolygon.b = 255.0f;
}

// [0x52263c] [D]
// The shadow of a car: a quad at its shadowPoints, or with detailed shadows (always for the
// player, a truck, or on uneven ground) a grid of quads over the ground height.
void DrawShadow(int id)
{
	CAR_DATA *cp;
	VERTEX *vert;
	VECTOR4 pos;
	MATRIX mat;
	float x0, z0;
	float dx, dz_x, dx_z, dz;
	float step_x, step_z;
	float fx, fz;
	float su, tv;
	int cols, rows, stride;
	int i, j;
	int index;
	int depth_mask;

	if (!gDrawShadows || !gShadowTexture)
	{
		current_vertex_computed_value++;
		return;
	}

	cp = &car_data[id];

	// none on water
	if (ROADS_GetRoadInfo(cp->location.vx, cp->location.vz) && model_pointers[RouteData.type] && (model_pointers[RouteData.type]->shape_flags & SHAPE_FLAG_WATER))
	{
		current_vertex_computed_value++;
		return;
	}

	if (gDoTraining)
	{
		if (id == 1)
			return;

		ShadowPolygon.b = 255.0f;
		ShadowPolygon.g = 255.0f;
		ShadowPolygon.r = 255.0f;
	}

	if (do_fogging)
	{
		ShadowPolygon.r = 128.0f;
		ShadowPolygon.g = 128.0f;
		ShadowPolygon.b = 128.0f;
	}

	if (gDoDetailedShadows && (id == 0 || gDoTraining || cp->model == 1 ||
		abs(cp->shadowPoints[0].vy - cp->shadowPoints[1].vy) >= 10 ||
		abs(cp->shadowPoints[1].vy - cp->shadowPoints[2].vy) >= 10 ||
		abs(cp->shadowPoints[2].vy - cp->shadowPoints[3].vy) >= 10))
	{
		if (!gDoTraining && id != 0)
		{
			cols = 5;
			rows = 6;
			stride = 6;
		}
		else
		{
			cols = 8;
			rows = 10;
			stride = 9;
		}

		if (FlipX)
		{
			mat = inv_camera_matrix;
			mat.m[0][0] = -inv_camera_matrix.m[0][0];
			mat.m[1][0] = -inv_camera_matrix.m[1][0];
			mat.m[2][0] = -inv_camera_matrix.m[2][0];
		}

		// the grid spans shadowPoints 0 -> 1 (across) and 0 -> 2 (along)
		x0 = (float)cp->shadowPoints[0].vx;
		z0 = (float)cp->shadowPoints[0].vz;
		dz_x = (float)cp->shadowPoints[1].vz - z0;
		dx_z = (float)cp->shadowPoints[2].vx - x0;
		dz = (float)cp->shadowPoints[2].vz - z0;

		step_x = 1.0f / (float)cols;
		step_z = 1.0f / (float)rows;

		fz = 0.0f;
		last_shadowx = 0x7FFFFF;
		last_shadowz = 0x7FFFFF;

		vert = conv_points;

		if (FlipX)
			CurrentRotationMatrix = &mat;
		else
			CurrentRotationMatrix = &inv_camera_matrix;

		for (j = 0; j <= rows; j++)
		{
			fx = 0.0f;

			for (i = 0; i <= cols; i++)
			{
				dx = (float)cp->shadowPoints[1].vx - x0;

				pos.vx = dx * fx + x0 + dx_z * fz;
				pos.vz = dz_x * fx + z0 + dz * fz;
				pos.vy = MapHeightShadow(&pos);

				vert->vx = pos.vx - camera_position.vx;
				vert->vy = -10.0f - camera_position.vy - pos.vy;
				vert->vz = pos.vz - camera_position.vz;
				vert->computed = current_vertex_computed_value;

				MatrixRotate((VECTOR4 *)&vert->vx);

				if (vert->vz > 45000.0f)
				{
					current_vertex_computed_value++;
					return;
				}

				if (vert->vz >= 50.0f)
					VertexPers(vert);

				fx = fx + step_x;
				vert++;
			}

			fz = fz + step_z;
		}

		su = shadow_w * step_x;
		tv = shadow_h * step_z;

		ShadowPolygon.t0 = shadow0_v;
		ShadowPolygon.t1 = shadow0_v;
		ShadowPolygon.t2 = shadow0_v + tv;
		ShadowPolygon.t3 = ShadowPolygon.t2;

		if (gPreviousAlpha)
		{
			grAlphaCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXTRUE);
			grAlphaTestFunction(GR_CMP_ALWAYS);
			gPreviousAlpha = 0;
		}

		if (do_fogging)
			grFogMode(GR_FOG_DISABLE);

		vert = conv_points;

		grDepthBiasLevel(-768);
		depth_mask = EnableDepthMask(0);
		do_clipping = 1;

		index = 0;

		for (j = 0; j < rows; j++)
		{
			ShadowPolygon.s0 = shadow0_u;
			ShadowPolygon.s1 = shadow0_u + su;
			ShadowPolygon.s2 = ShadowPolygon.s1;
			ShadowPolygon.s3 = shadow0_u;

			for (i = 0; i < cols; i++)
			{
				ShadowPolygon.v0 = index;
				ShadowPolygon.v1 = index + 1;
				ShadowPolygon.v2 = index + stride + 1;
				ShadowPolygon.v3 = stride + index;

				if (vert[0].vz > 50.0f && vert[1].vz > 50.0f && vert[stride + 1].vz > 50.0f && vert[stride].vz > 50.0f)
				{
					polyblock = (int)&ShadowPolygon;
					DrawPOLYFT4(2);
				}

				ShadowPolygon.s0 = ShadowPolygon.s0 + su;
				ShadowPolygon.s1 = ShadowPolygon.s1 + su;
				ShadowPolygon.s2 = ShadowPolygon.s2 + su;
				ShadowPolygon.s3 = ShadowPolygon.s3 + su;

				vert++;
				index++;
			}

			index++;
			vert++;

			ShadowPolygon.t0 = ShadowPolygon.t0 + tv;
			ShadowPolygon.t1 = ShadowPolygon.t1 + tv;
			ShadowPolygon.t2 = ShadowPolygon.t2 + tv;
			ShadowPolygon.t3 = ShadowPolygon.t3 + tv;
		}

		grDepthBiasLevel(0);
		EnableDepthMask(depth_mask);

		if (do_fogging)
			grFogMode(GR_FOG_WITH_TABLE);
	}
	else
	{
		ShadowPolygon.s0 = shadow0_u;
		ShadowPolygon.t0 = shadow0_v;
		ShadowPolygon.s1 = shadow1_u;
		ShadowPolygon.t1 = shadow1_v;
		ShadowPolygon.s2 = shadow2_u;
		ShadowPolygon.t2 = shadow2_v;
		ShadowPolygon.s3 = shadow3_u;
		ShadowPolygon.t3 = shadow3_v;

		ShadowPolygon.v0 = 0;
		ShadowPolygon.v1 = 1;
		ShadowPolygon.v2 = 2;
		ShadowPolygon.v3 = 3;

		// the corners 0 1 3 2 (around the quad), 10 above the ground
		conv_points[0].vx = (float)cp->shadowPoints[0].vx - camera_position.vx;
		conv_points[0].vy = -10.0f - camera_position.vy - (float)cp->shadowPoints[0].vy;
		conv_points[0].vz = (float)cp->shadowPoints[0].vz - camera_position.vz;
		conv_points[0].computed = current_vertex_computed_value;

		conv_points[1].vx = (float)cp->shadowPoints[1].vx - camera_position.vx;
		conv_points[1].vy = -10.0f - camera_position.vy - (float)cp->shadowPoints[1].vy;
		conv_points[1].vz = (float)cp->shadowPoints[1].vz - camera_position.vz;
		conv_points[1].computed = current_vertex_computed_value;

		conv_points[2].vx = (float)cp->shadowPoints[3].vx - camera_position.vx;
		conv_points[2].vy = -10.0f - camera_position.vy - (float)cp->shadowPoints[3].vy;
		conv_points[2].vz = (float)cp->shadowPoints[3].vz - camera_position.vz;
		conv_points[2].computed = current_vertex_computed_value;

		conv_points[3].vx = (float)cp->shadowPoints[2].vx - camera_position.vx;
		conv_points[3].vy = -10.0f - camera_position.vy - (float)cp->shadowPoints[2].vy;
		conv_points[3].vz = (float)cp->shadowPoints[2].vz - camera_position.vz;
		conv_points[3].computed = current_vertex_computed_value;

		if (FlipX)
		{
			mat = inv_camera_matrix;
			mat.m[0][0] = -inv_camera_matrix.m[0][0];
			mat.m[1][0] = -inv_camera_matrix.m[1][0];
			mat.m[2][0] = -inv_camera_matrix.m[2][0];

			CurrentRotationMatrix = &mat;
		}
		else
		{
			CurrentRotationMatrix = &inv_camera_matrix;
		}

		for (i = 0; i < 4; i++)
		{
			MatrixRotate((VECTOR4 *)&conv_points[i].vx);

			if (conv_points[i].vz > 45000.0f)
			{
				current_vertex_computed_value++;
				return;
			}
		}

		if (gPreviousAlpha)
		{
			grAlphaCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXTRUE);
			grAlphaTestFunction(GR_CMP_ALWAYS);
			gPreviousAlpha = 0;
		}

		polyblock = (int)&ShadowPolygon;

		if (do_fogging)
			grFogMode(GR_FOG_DISABLE);

		depth_mask = EnableDepthMask(0);

		if (conv_points[0].vz >= 50.0f && conv_points[1].vz >= 50.0f && conv_points[2].vz >= 50.0f && conv_points[3].vz >= 50.0f)
		{
			do_clipping = 1;

			VertexPers(&conv_points[0]);
			VertexPers(&conv_points[1]);
			VertexPers(&conv_points[2]);
			VertexPers(&conv_points[3]);

			DrawPOLYFT4(2);
		}
		else
		{
			DrawClippedPOLYFT4(2);
		}

		EnableDepthMask(depth_mask);

		if (do_fogging)
			grFogMode(GR_FOG_WITH_TABLE);
	}

	current_vertex_computed_value++;
}
