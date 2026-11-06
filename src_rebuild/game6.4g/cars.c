/*
 * cars.c - drawing the cars: body, wheels, lights and smoke
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "cars.c", 0x4a27d0 - 0x4a3daf)
 */

#include <stdlib.h>
#include <string.h>

#include "dr_types.h"
#include "dr_globals.h"
#include "handling.h"
#include "asmmath.h"
#include "testmath.h"
#include "Engine/3dfx/object.h"
#include "D3DGlide/glide_ptrs.h"
#include "camera.h"
#include "debris.h"
#include "denting.h"
#include "dr_cars.h"
#include "general.h"
#include "map.h"
#include "roadbits.h"
#include "shadow.h"
#include "shell.h"
#include "pause.h"
#include "cops.h"
#include "Engine/3dfx/gfxinit.h"
#include "Engine/3dfx/polydraw.h"
#include "ai.h"
#include "frontend/cheats.h"
#include "glaunch.h"

// the radius of a wheel model's rim vertices (DrawCarWheels)
#define WHEEL_RADIUS	72.0f

//------------------------------------------------------------------------------------------
// uninitialised data

static int front_wheel_rotation[66];		// NOTE: no names in the database (dword_B9EF98, dword_B9F0A0)
static int rear_wheel_rotation[66];

void DrawCarWheels(int id, MATRIX *matrix, char visibility);
int GetCarDamage(int car);

// NOTE: moved here from the generated stubs; no IDA mark, the owner is a guess from the users
// and the neighbouring data
CAR_COSMETICS player_car_cosmetics[18] = {
	{
		{ { -145.4f, 13.3f, -518.8f, 0.0f }, { 145.4f, 13.3f, -518.8f, 0.0f } },		// headLight
		{ { -151.1f, 75.9f, -516.4f, 0.0f }, { 151.1f, 75.9f, -516.4f, 0.0f } },		// frontInd
		{ { -157.3f, -1.3f, 531.2f, 0.0f }, { 157.3f, -1.3f, 531.2f, 0.0f } },		// backInd
		{ { -137.9f, -0.7f, 538.7f, 0.0f }, { 137.9f, -0.7f, 538.7f, 0.0f } },		// brakeLight
		{ 138.8f, 0.0f, 540.8f, 0.0f },		// revLight
		{ { -50.0f, 0.0f, 0.0f, 0.0f }, { 50.0f, 0.0f, 0.0f, 0.0f } },		// policeLight
		{ 100.0f, 79.8f, 503.8f, 0.0f },		// exhaust
		{ 0.0f, 70.0f, -423.0f, 0.0f },		// smoke
		{ 0.0f, 10.0f, -451.2f, 0.0f },		// fire
		{ { 187, -163, 299, 0 }, { 187, -163, -278, 0 }, { -187, -163, 299, 0 }, { -187, -163, -278, 0 } },		// wheelDisp
		7000, -30712,		// powerRatio, extraInfo
		{		// cPoints
			{ 185, -107, -528, 0 },
			{ -185, -107, -528, 0 },
			{ 185, -107, 542, 0 },
			{ -185, -107, 542, 0 },
			{ 185, 29, -546, 0 },
			{ -185, 29, -546, 0 },
			{ 150, 130, -180, 0 },
			{ -150, 130, -180, 0 },
			{ 150, 130, 31, 0 },
			{ -150, 130, 31, 0 },
			{ 185, 9, 540, 0 },
			{ -185, 9, 540, 0 },
		},
		{ 189, 132, 548, 0 }		// colBox
	},
	{
		{ { -148.1f, 5.9f, -512.8f, 0.0f }, { 148.1f, 5.9f, -512.8f, 0.0f } },		// headLight
		{ { -164.6f, 33.5f, -509.1f, 0.0f }, { 164.6f, 33.5f, -509.1f, 0.0f } },		// frontInd
		{ { -181.2f, 39.2f, 503.4f, 0.0f }, { 181.2f, 39.2f, 503.4f, 0.0f } },		// backInd
		{ { -181.4f, 11.3f, 510.9f, 0.0f }, { 181.4f, 11.3f, 510.9f, 0.0f } },		// brakeLight
		{ 174.2f, 39.7f, 517.9f, 0.0f },		// revLight
		{ { -50.0f, 0.0f, 0.0f, 0.0f }, { 50.0f, 0.0f, 0.0f, 0.0f } },		// policeLight
		{ -168.5f, 112.2f, 473.1f, 0.0f },		// exhaust
		{ 0.0f, -15.1f, -442.082f, 0.0f },		// smoke
		{ 0.0f, -22.4f, -451.2f, 0.0f },		// fire
		{ { 190, -200, 344, 0 }, { 190, -200, -249, 0 }, { -190, -200, 344, 0 }, { -190, -200, -249, 0 } },		// wheelDisp
		7000, -29688,		// powerRatio, extraInfo
		{		// cPoints
			{ 201, -105, -520, 0 },
			{ -201, -105, -520, 0 },
			{ 189, -105, 520, 0 },
			{ -189, -105, 520, 0 },
			{ 185, 31, -507, 0 },
			{ -185, 31, -507, 0 },
			{ 150, 130, -107, 0 },
			{ -150, 130, -107, 0 },
			{ 141, 125, 130, 0 },
			{ -141, 125, 130, 0 },
			{ 185, 23, 515, 0 },
			{ -185, 23, 515, 0 },
		},
		{ 192, 132, 522, 0 }		// colBox
	},
	{
		{ { -127.8f, 34.3f, -500.7f, 0.0f }, { 127.8f, 34.3f, -500.7f, 0.0f } },		// headLight
		{ { -173.3f, 33.5f, -510.3f, 0.0f }, { 173.3f, 33.5f, -510.3f, 0.0f } },		// frontInd
		{ { -152.8f, 38.6f, 515.0f, 0.0f }, { 152.8f, 38.6f, 515.0f, 0.0f } },		// backInd
		{ { -88.2f, 39.7f, 515.0f, 0.0f }, { 88.2f, 39.7f, 515.0f, 0.0f } },		// brakeLight
		{ 85.1f, 39.7f, 522.0f, 0.0f },		// revLight
		{ { -50.0f, 0.0f, 0.0f, 0.0f }, { 50.0f, 0.0f, 0.0f, 0.0f } },		// policeLight
		{ 122.6f, 116.2f, 491.1f, 0.0f },		// exhaust
		{ -5.8f, 13.9f, -420.838f, 0.0f },		// smoke
		{ -11.6f, 5.9f, -422.34198f, 0.0f },		// fire
		{ { 190, -185, 340, 0 }, { 190, -185, -292, 0 }, { -190, -185, 340, 0 }, { -190, -185, -292, 0 } },		// wheelDisp
		7000, 17160,		// powerRatio, extraInfo
		{		// cPoints
			{ 185, -118, -544, 0 },
			{ -185, -118, -544, 0 },
			{ 185, -110, 520, 0 },
			{ -185, -110, 520, 0 },
			{ 185, 15, -495, 0 },
			{ -185, 15, -495, 0 },
			{ 150, 113, -240, 0 },
			{ -150, 113, -240, 0 },
			{ 150, 121, 57, 0 },
			{ -150, 121, 57, 0 },
			{ 185, -8, 503, 0 },
			{ -185, -8, 503, 0 },
		},
		{ 186, 122, 546, 0 }		// colBox
	},
	{
		{ { -164.3f, 30.3f, -520.9f, 0.0f }, { 164.3f, 30.3f, -520.9f, 0.0f } },		// headLight
		{ { -165.2f, 86.2f, -530.0f, 0.0f }, { 165.2f, 86.2f, -530.0f, 0.0f } },		// frontInd
		{ { -173.1f, -1.9f, 535.3f, 0.0f }, { 173.1f, -1.9f, 535.3f, 0.0f } },		// backInd
		{ { -177.3f, 23.5f, 531.2f, 0.0f }, { 177.3f, 23.5f, 531.2f, 0.0f } },		// brakeLight
		{ 170.1f, 11.3f, 534.2f, 0.0f },		// revLight
		{ { -50.0f, 0.0f, 0.0f, 0.0f }, { 50.0f, 0.0f, 0.0f, 0.0f } },		// policeLight
		{ 117.4f, 95.9f, 496.3f, 0.0f },		// exhaust
		{ -8.1f, 5.2f, -481.186f, 0.0f },		// smoke
		{ -5.8f, -2.2f, -482.78397f, 0.0f },		// fire
		{ { 195, -194, 368, 0 }, { 195, -194, -270, 0 }, { -195, -194, 368, 0 }, { -195, -194, -270, 0 } },		// wheelDisp
		7000, -22520,		// powerRatio, extraInfo
		{		// cPoints
			{ 185, -114, -532, 0 },
			{ -185, -114, -532, 0 },
			{ 185, -118, 528, 0 },
			{ -185, -118, 528, 0 },
			{ 185, 7, -532, 0 },
			{ -185, 7, -532, 0 },
			{ 150, 117, -179, 0 },
			{ -150, 117, -179, 0 },
			{ 150, 125, 102, 0 },
			{ -150, 125, 102, 0 },
			{ 185, 15, 524, 0 },
			{ -185, 15, 524, 0 },
		},
		{ 196, 127, 534, 0 }		// colBox
	},
	{
		{ { -115.7f, 30.3f, -508.8f, 0.0f }, { 115.7f, 30.3f, -508.8f, 0.0f } },		// headLight
		{ { -148.9f, 29.5f, -514.3f, 0.0f }, { 148.9f, 29.5f, -514.3f, 0.0f } },		// frontInd
		{ { -148.7f, 18.4f, 510.9f, 0.0f }, { 148.7f, 18.4f, 510.9f, 0.0f } },		// backInd
		{ { -128.7f, 19.4f, 515.0f, 0.0f }, { 128.7f, 19.4f, 515.0f, 0.0f } },		// brakeLight
		{ 109.4f, 19.4f, 522.0f, 0.0f },		// revLight
		{ { -50.0f, 0.0f, 0.0f, 0.0f }, { 50.0f, 0.0f, 0.0f, 0.0f } },		// policeLight
		{ 105.8f, 95.9f, 480.7f, 0.0f },		// exhaust
		{ 0.0f, 5.2f, -445.84198f, 0.0f },		// smoke
		{ 0.0f, 1.9f, -458.814f, 0.0f },		// fire
		{ { 183, -169, 311, 0 }, { 183, -169, -264, 0 }, { -183, -169, 311, 0 }, { -183, -169, -264, 0 } },		// wheelDisp
		9100, 16904,		// powerRatio, extraInfo
		{		// cPoints
			{ 185, -110, -520, 0 },
			{ -185, -110, -520, 0 },
			{ 185, -110, 511, 0 },
			{ -185, -110, 511, 0 },
			{ 185, 23, -507, 0 },
			{ -185, 23, -507, 0 },
			{ 150, 105, -216, 0 },
			{ -150, 105, -216, 0 },
			{ 150, 105, 65, 0 },
			{ -150, 105, 65, 0 },
			{ 185, 11, 507, 0 },
			{ -185, 11, 507, 0 },
		},
		{ 187, 112, 522, 0 }		// colBox
	},
	{
		{ { -144.1f, 34.3f, -520.9f, 0.0f }, { 144.1f, 34.3f, -520.9f, 0.0f } },		// headLight
		{ { -185.4f, 29.5f, -534.6f, 0.0f }, { 185.4f, 29.5f, -534.6f, 0.0f } },		// frontInd
		{ { -185.2f, 29.9f, 531.2f, 0.0f }, { 185.2f, 29.9f, 531.2f, 0.0f } },		// backInd
		{ { -161.1f, 27.5f, 531.2f, 0.0f }, { 161.1f, 27.5f, 531.2f, 0.0f } },		// brakeLight
		{ 137.7f, 31.6f, 534.2f, 0.0f },		// revLight
		{ { -50.0f, 0.0f, 0.0f, 0.0f }, { 50.0f, 0.0f, 0.0f, 0.0f } },		// policeLight
		{ 88.4f, 100.0f, 537.9f, 0.0f },		// exhaust
		{ 0.0f, 5.2f, -464.924f, 0.0f },		// smoke
		{ 0.0f, 1.9f, -466.428f, 0.0f },		// fire
		{ { 202, -177, 340, 0 }, { 202, -177, -292, 0 }, { -202, -177, 340, 0 }, { -202, -177, -292, 0 } },		// wheelDisp
		7000, 8,		// powerRatio, extraInfo
		{		// cPoints
			{ 190, -101, -560, 0 },
			{ -190, -101, -560, 0 },
			{ 202, -97, 560, 0 },
			{ -202, -97, 560, 0 },
			{ 185, 19, -507, 0 },
			{ -185, 19, -507, 0 },
			{ 150, 105, -248, 0 },
			{ -150, 105, -248, 0 },
			{ 150, 117, 37, 0 },
			{ -150, 117, 37, 0 },
			{ 185, 0, 524, 0 },
			{ -185, 0, 524, 0 },
		},
		{ 192, 119, 562, 0 }		// colBox
	},
	{
		{ { -147.5f, 42.4f, -500.7f, 0.0f }, { 147.5f, 42.4f, -500.7f, 0.0f } },		// headLight
		{ { -177.3f, 37.6f, -510.3f, 0.0f }, { 177.3f, 37.6f, -510.3f, 0.0f } },		// frontInd
		{ { -169.0f, 1.6f, 502.8f, 0.0f }, { 169.0f, 1.6f, 502.8f, 0.0f } },		// backInd
		{ { -89.9f, -0.8f, 499.4f, 0.0f }, { 89.9f, -0.8f, 499.4f, 0.0f } },		// brakeLight
		{ 125.6f, 3.2f, 504.6f, 0.0f },		// revLight
		{ { -50.0f, 0.0f, 0.0f, 0.0f }, { 50.0f, 0.0f, 0.0f, 0.0f } },		// policeLight
		{ -129.2f, 71.6f, 486.4f, 0.0f },		// exhaust
		{ 0.0f, 9.2f, -445.84198f, 0.0f },		// smoke
		{ 0.0f, 10.0f, -451.2f, 0.0f },		// fire
		{ { 194, -198, 352, 0 }, { 194, -198, -272, 0 }, { -194, -198, 352, 0 }, { -194, -198, -272, 0 } },		// wheelDisp
		7000, 8,		// powerRatio, extraInfo
		{		// cPoints
			{ 185, -110, -520, 0 },
			{ -185, -110, -520, 0 },
			{ 185, -114, 507, 0 },
			{ -185, -114, 507, 0 },
			{ 185, 23, -491, 0 },
			{ -185, 23, -491, 0 },
			{ 150, 109, -159, 0 },
			{ -150, 109, -159, 0 },
			{ 140, 117, 90, 0 },
			{ -140, 117, 90, 0 },
			{ 185, -8, 507, 0 },
			{ -185, -8, 507, 0 },
		},
		{ 196, 119, 522, 0 }		// colBox
	},
	{
		{ { -119.4f, 41.6f, -529.1f, 0.0f }, { 119.4f, 41.6f, -529.1f, 0.0f } },		// headLight
		{ { -160.7f, 38.4f, -530.5f, 0.0f }, { 160.7f, 38.4f, -530.5f, 0.0f } },		// frontInd
		{ { -173.2f, 66.4f, 575.4f, 0.0f }, { 173.2f, 66.4f, 575.4f, 0.0f } },		// backInd
		{ { -173.3f, 35.6f, 571.9f, 0.0f }, { 173.3f, 35.6f, 571.9f, 0.0f } },		// brakeLight
		{ 48.6f, 31.6f, 536.2f, 0.0f },		// revLight
		{ { -50.0f, 0.0f, 0.0f, 0.0f }, { 50.0f, 0.0f, 0.0f, 0.0f } },		// policeLight
		{ 117.4f, 95.9f, 569.2f, 0.0f },		// exhaust
		{ 0.0f, 9.2f, -457.31f, 0.0f },		// smoke
		{ 0.0f, 10.0f, -451.2f, 0.0f },		// fire
		{ { 190, -177, 348, 0 }, { 190, -177, -276, 0 }, { -190, -177, 348, 0 }, { -190, -177, -276, 0 } },		// wheelDisp
		7000, 20744,		// powerRatio, extraInfo
		{		// cPoints
			{ 185, -109, -554, 0 },
			{ -185, -109, -554, 0 },
			{ 185, -110, 552, 0 },
			{ -185, -110, 552, 0 },
			{ 185, 11, -554, 0 },
			{ -185, 11, -554, 0 },
			{ 150, 109, -277, 0 },
			{ -150, 109, -277, 0 },
			{ 150, 109, 78, 0 },
			{ -150, 109, 78, 0 },
			{ 185, -4, 536, 0 },
			{ -185, -4, 536, 0 },
		},
		{ 192, 112, 556, 0 }		// colBox
	},
	{
		{ { -135.9f, 30.3f, -549.3f, 0.0f }, { 135.9f, 30.3f, -549.3f, 0.0f } },		// headLight
		{ { -173.3f, 29.5f, -562.0f, 0.0f }, { 173.3f, 29.5f, -562.0f, 0.0f } },		// frontInd
		{ { -173.1f, 18.4f, 574.8f, 0.0f }, { 173.1f, 18.4f, 574.8f, 0.0f } },		// backInd
		{ { -144.9f, 22.9f, 574.8f, 0.0f }, { 144.9f, 22.9f, 574.8f, 0.0f } },		// brakeLight
		{ 99.5f, 20.0f, 582.2f, 0.0f },		// revLight
		{ { -50.0f, 0.0f, 0.0f, 0.0f }, { 50.0f, 0.0f, 0.0f, 0.0f } },		// policeLight
		{ 120.8f, 71.6f, 550.7f, 0.0f },		// exhaust
		{ -5.8f, -2.9f, -498.10602f, 0.0f },		// smoke
		{ -8.1f, -6.2f, -503.934f, 0.0f },		// fire
		{ { 196, -190, 382, 0 }, { 196, -190, -260, 0 }, { -196, -190, 382, 0 }, { -196, -190, -260, 0 } },		// wheelDisp
		7000, 8,		// powerRatio, extraInfo
		{		// cPoints
			{ 185, -118, -561, 0 },
			{ -185, -118, -561, 0 },
			{ 185, -101, 564, 0 },
			{ -185, -101, 564, 0 },
			{ 198, 23, -554, 0 },
			{ -198, 23, -554, 0 },
			{ 150, 105, -212, 0 },
			{ -150, 105, -212, 0 },
			{ 150, 113, 98, 0 },
			{ -150, 113, 98, 0 },
			{ 185, 11, 548, 0 },
			{ -185, 11, 548, 0 },
		},
		{ 198, 121, 572, 0 }		// colBox
	},
	{
		{ { -144.9f, 38.6f, -494.2f, 0.0f }, { 144.9f, 38.6f, -494.2f, 0.0f } },		// headLight
		{ { -169.1f, 36.2f, -490.5f, 0.0f }, { 169.1f, 36.2f, -490.5f, 0.0f } },		// frontInd
		{ { -159.9f, 25.8f, 508.2f, 0.0f }, { 159.9f, 25.8f, 508.2f, 0.0f } },		// backInd
		{ { -140.5f, 24.6f, 510.3f, 0.0f }, { 140.5f, 24.6f, 510.3f, 0.0f } },		// brakeLight
		{ 127.3f, 24.6f, 516.9f, 0.0f },		// revLight
		{ { -50.0f, 0.0f, 0.0f, 0.0f }, { 50.0f, 0.0f, 0.0f, 0.0f } },		// policeLight
		{ 109.3f, 63.5f, 478.9f, 0.0f },		// exhaust
		{ 0.0f, 9.2f, -464.924f, 0.0f },		// smoke
		{ 4.1f, 3.2f, -465.77f, 0.0f },		// fire
		{ { 180, -179, 346, 0 }, { 180, -179, -260, 0 }, { -180, -179, 346, 0 }, { -180, -179, -260, 0 } },		// wheelDisp
		7000, -7672,		// powerRatio, extraInfo
		{		// cPoints
			{ 185, -110, -520, 0 },
			{ -185, -110, -520, 0 },
			{ 185, -110, 520, 0 },
			{ -185, -110, 520, 0 },
			{ 185, 23, -524, 0 },
			{ -185, 23, -524, 0 },
			{ 150, 113, -171, 0 },
			{ -150, 113, -171, 0 },
			{ 150, 109, 98, 0 },
			{ -150, 109, 98, 0 },
			{ 185, 0, 520, 0 },
			{ -185, 0, 520, 0 },
		},
		{ 186, 115, 526, 0 }		// colBox
	},
	{
		{ { -127.8f, 42.4f, -504.7f, 0.0f }, { 127.8f, 42.4f, -504.7f, 0.0f } },		// headLight
		{ { -169.2f, 41.6f, -510.3f, 0.0f }, { 169.2f, 41.6f, -510.3f, 0.0f } },		// frontInd
		{ { -156.8f, 50.8f, 515.0f, 0.0f }, { 156.8f, 50.8f, 515.0f, 0.0f } },		// backInd
		{ { -128.7f, 47.8f, 523.7f, 0.0f }, { 128.7f, 47.8f, 523.7f, 0.0f } },		// brakeLight
		{ 93.2f, 51.8f, 530.1f, 0.0f },		// revLight
		{ { -78.4f, -121.5f, 20.3f, 0.0f }, { 78.4f, -121.5f, 20.3f, 0.0f } },		// policeLight
		{ 103.5f, 115.6f, 525.2f, 0.0f },		// exhaust
		{ 0.0f, 21.4f, -442.082f, 0.0f },		// smoke
		{ -1.7f, 18.1f, -445.74802f, 0.0f },		// fire
		{ { 180, -194, 311, 0 }, { 180, -194, -308, 0 }, { -180, -194, 311, 0 }, { -180, -194, -308, 0 } },		// wheelDisp
		7000, -29688,		// powerRatio, extraInfo
		{		// cPoints
			{ 185, -110, -540, 0 },
			{ -185, -110, -540, 0 },
			{ 176, -141, 536, 0 },
			{ -176, -141, 536, 0 },
			{ 167, 0, -515, 0 },
			{ -167, 0, -515, 0 },
			{ 132, 101, -218, 0 },
			{ -132, 101, -218, 0 },
			{ 124, 105, 78, 0 },
			{ -124, 105, 78, 0 },
			{ 173, -12, 504, 0 },
			{ -173, -12, 504, 0 },
		},
		{ 186, 114, 542, 0 }		// colBox
	},
	{
		{ { -176.5f, 38.9f, -541.2f, 0.0f }, { 176.5f, 38.9f, -541.2f, 0.0f } },		// headLight
		{ { -132.7f, 45.7f, -567.0f, 0.0f }, { 132.7f, 45.7f, -567.0f, 0.0f } },		// frontInd
		{ { -173.1f, 29.9f, 530.0f, 0.0f }, { 173.1f, 29.9f, 530.0f, 0.0f } },		// backInd
		{ { -157.1f, 31.6f, 540.1f, 0.0f }, { 157.1f, 31.6f, 540.1f, 0.0f } },		// brakeLight
		{ 133.7f, 31.6f, 540.3f, 0.0f },		// revLight
		{ { -50.0f, 0.0f, 0.0f, 0.0f }, { 50.0f, 0.0f, 0.0f, 0.0f } },		// policeLight
		{ 128.9f, 104.1f, 510.2f, 0.0f },		// exhaust
		{ 0.0f, 9.2f, -479.02402f, 0.0f },		// smoke
		{ 4.6f, 10.0f, -483.81802f, 0.0f },		// fire
		{ { 190, -190, 340, 0 }, { 190, -190, -284, 0 }, { -190, -190, 340, 0 }, { -190, -190, -284, 0 } },		// wheelDisp
		7000, -22520,		// powerRatio, extraInfo
		{		// cPoints
			{ 185, -114, -540, 0 },
			{ -185, -114, -540, 0 },
			{ 185, -114, 544, 0 },
			{ -185, -114, 544, 0 },
			{ 185, 27, -528, 0 },
			{ -185, 27, -528, 0 },
			{ 150, 113, -200, 0 },
			{ -150, 113, -200, 0 },
			{ 150, 121, 53, 0 },
			{ -150, 121, 53, 0 },
			{ 196, -4, 545, 0 },
			{ -196, -4, 545, 0 },
		},
		{ 195, 122, 546, 0 }		// colBox
	},
	{
		{ { -148.1f, 18.1f, -495.0f, 0.0f }, { 148.1f, 18.1f, -495.0f, 0.0f } },		// headLight
		{ { -104.4f, 25.4f, -500.0f, 0.0f }, { 104.4f, 25.4f, -500.0f, 0.0f } },		// frontInd
		{ { -96.1f, 30.5f, 510.0f, 0.0f }, { 96.1f, 30.5f, 510.0f, 0.0f } },		// backInd
		{ { -140.8f, 27.5f, 508.0f, 0.0f }, { 140.8f, 27.5f, 508.0f, 0.0f } },		// brakeLight
		{ 93.2f, 19.4f, 505.0f, 0.0f },		// revLight
		{ { -50.0f, 0.0f, 0.0f, 0.0f }, { 50.0f, 0.0f, 0.0f, 0.0f } },		// policeLight
		{ 117.4f, 83.8f, 504.4f, 0.0f },		// exhaust
		{ 0.0f, 5.2f, -438.228f, 0.0f },		// smoke
		{ 0.0f, 1.9f, -447.346f, 0.0f },		// fire
		{ { 190, -169, 307, 0 }, { 190, -169, -288, 0 }, { -190, -169, 307, 0 }, { -190, -169, -288, 0 } },		// wheelDisp
		7000, -23544,		// powerRatio, extraInfo
		{		// cPoints
			{ 185, -110, -520, 0 },
			{ -185, -110, -520, 0 },
			{ 185, -110, 520, 0 },
			{ -185, -110, 520, 0 },
			{ 185, 3, -503, 0 },
			{ -185, 3, -503, 0 },
			{ 150, 105, -195, 0 },
			{ -150, 105, -195, 0 },
			{ 150, 109, 33, 0 },
			{ -150, 109, 33, 0 },
			{ 185, 11, 499, 0 },
			{ -185, 11, 499, 0 },
		},
		{ 192, 110, 522, 0 }		// colBox
	},
	{
		{ { -127.8f, 54.6f, -545.3f, 0.0f }, { 127.8f, 54.6f, -545.3f, 0.0f } },		// headLight
		{ { -161.1f, 53.8f, -550.8f, 0.0f }, { 161.1f, 53.8f, -550.8f, 0.0f } },		// frontInd
		{ { -169.0f, 58.9f, 559.6f, 0.0f }, { 169.0f, 58.9f, 559.6f, 0.0f } },		// backInd
		{ { -154.7f, 56.4f, 561.3f, 0.0f }, { 154.7f, 56.4f, 561.3f, 0.0f } },		// brakeLight
		{ 125.6f, 55.9f, 562.5f, 0.0f },		// revLight
		{ { -50.0f, 0.0f, 0.0f, 0.0f }, { 50.0f, 0.0f, 0.0f, 0.0f } },		// policeLight
		{ -111.8f, 128.4f, 544.9f, 0.0f },		// exhaust
		{ 0.0f, 25.4f, -483.912f, 0.0f },		// smoke
		{ 0.0f, 26.2f, -489.27f, 0.0f },		// fire
		{ { 181, -206, 329, 0 }, { 181, -206, -300, 0 }, { -181, -206, 329, 0 }, { -181, -206, -300, 0 } },		// wheelDisp
		7000, 8,		// powerRatio, extraInfo
		{		// cPoints
			{ 167, -114, -562, 0 },
			{ -167, -114, -562, 0 },
			{ 167, -126, 554, 0 },
			{ -167, -126, 554, 0 },
			{ 167, -20, -552, 0 },
			{ -167, -20, -552, 0 },
			{ 138, 93, -199, 0 },
			{ -138, 93, -199, 0 },
			{ 132, 101, 88, 0 },
			{ -132, 101, 88, 0 },
			{ 156, -20, 534, 0 },
			{ -156, -20, 534, 0 },
		},
		{ 186, 126, 563, 0 }		// colBox
	},
	{
		{ { -119.4f, 41.6f, -529.1f, 0.0f }, { 119.4f, 41.6f, -529.1f, 0.0f } },		// headLight
		{ { -160.7f, 38.4f, -530.5f, 0.0f }, { 160.7f, 38.4f, -530.5f, 0.0f } },		// frontInd
		{ { -173.2f, 66.4f, 575.4f, 0.0f }, { 173.2f, 66.4f, 575.4f, 0.0f } },		// backInd
		{ { -173.3f, 35.6f, 571.9f, 0.0f }, { 173.3f, 35.6f, 571.9f, 0.0f } },		// brakeLight
		{ 48.6f, 31.6f, 536.2f, 0.0f },		// revLight
		{ { -50.0f, 0.0f, 0.0f, 0.0f }, { 50.0f, 0.0f, 0.0f, 0.0f } },		// policeLight
		{ 117.4f, 95.9f, 569.2f, 0.0f },		// exhaust
		{ 0.0f, 9.2f, -457.31f, 0.0f },		// smoke
		{ 0.0f, 10.0f, -451.2f, 0.0f },		// fire
		{ { 190, -177, 348, 0 }, { 190, -177, -276, 0 }, { -190, -177, 348, 0 }, { -190, -177, -276, 0 } },		// wheelDisp
		7000, 20744,		// powerRatio, extraInfo
		{		// cPoints
			{ 185, -109, -576, 0 },
			{ -185, -109, -576, 0 },
			{ 185, -110, 552, 0 },
			{ -185, -110, 552, 0 },
			{ 185, 11, -560, 0 },
			{ -185, 11, -560, 0 },
			{ 150, 109, -277, 0 },
			{ -150, 109, -277, 0 },
			{ 150, 109, 78, 0 },
			{ -150, 109, 78, 0 },
			{ 185, -4, 536, 0 },
			{ -185, -4, 536, 0 },
		},
		{ 192, 112, 577, 0 }		// colBox
	},
	{
		{ { -143.2f, 29.4f, -505.8f, 0.0f }, { 143.2f, 29.4f, -505.8f, 0.0f } },		// headLight
		{ { -143.3f, 53.8f, -508.7f, 0.0f }, { 143.3f, 53.8f, -508.7f, 0.0f } },		// frontInd
		{ { -123.6f, 35.4f, 502.0f, 0.0f }, { 123.6f, 35.4f, 502.0f, 0.0f } },		// backInd
		{ { -120.6f, 19.4f, 498.8f, 0.0f }, { 120.6f, 19.4f, 498.8f, 0.0f } },		// brakeLight
		{ 94.0f, 42.1f, 499.3f, 0.0f },		// revLight
		{ { -50.0f, 0.0f, 0.0f, 0.0f }, { 50.0f, 0.0f, 0.0f, 0.0f } },		// policeLight
		{ 88.0f, 99.5f, 486.2f, 0.0f },		// exhaust
		{ 0.0f, 14.9f, -462.574f, 0.0f },		// smoke
		{ 0.0f, 13.2f, -469.436f, 0.0f },		// fire
		{ { 177, -180, 320, 0 }, { 177, -180, -257, 0 }, { -177, -180, 320, 0 }, { -177, -180, -257, 0 } },		// wheelDisp
		7000, 8,		// powerRatio, extraInfo
		{		// cPoints
			{ 172, -106, -502, 0 },
			{ -172, -106, -502, 0 },
			{ 171, -110, 501, 0 },
			{ -171, -110, 501, 0 },
			{ 138, 10, -497, 0 },
			{ -138, 10, -497, 0 },
			{ 126, 113, -194, 0 },
			{ -126, 113, -194, 0 },
			{ 140, 117, 83, 0 },
			{ -140, 117, 83, 0 },
			{ 157, -5, 493, 0 },
			{ -157, -5, 493, 0 },
		},
		{ 186, 119, 513, 0 }		// colBox
	},
	{
		{ { -126.9f, 13.2f, -465.9f, 0.0f }, { 126.9f, 13.2f, -465.9f, 0.0f } },		// headLight
		{ { -137.6f, 57.4f, -471.7f, 0.0f }, { 137.6f, 57.4f, -471.7f, 0.0f } },		// frontInd
		{ { -110.8f, 21.6f, 476.2f, 0.0f }, { 110.8f, 21.6f, 476.2f, 0.0f } },		// backInd
		{ { -115.1f, 50.2f, 483.4f, 0.0f }, { 115.1f, 50.2f, 483.4f, 0.0f } },		// brakeLight
		{ 116.9f, 34.4f, 480.9f, 0.0f },		// revLight
		{ { -50.0f, 0.0f, 0.0f, 0.0f }, { 50.0f, 0.0f, 0.0f, 0.0f } },		// policeLight
		{ 97.3f, 115.4f, 448.2f, 0.0f },		// exhaust
		{ -1.8f, 13.1f, -397.15f, 0.0f },		// smoke
		{ -4.5f, 13.2f, -400.25198f, 0.0f },		// fire
		{ { 178, -174, 317, 0 }, { 178, -174, -262, 0 }, { -178, -174, 317, 0 }, { -178, -174, -262, 0 } },		// wheelDisp
		7000, 8,		// powerRatio, extraInfo
		{		// cPoints
			{ 185, -113, -469, 0 },
			{ -185, -113, -469, 0 },
			{ 185, -116, 472, 0 },
			{ -185, -116, 472, 0 },
			{ 162, 14, -461, 0 },
			{ -162, 14, -461, 0 },
			{ 136, 114, -224, 0 },
			{ -136, 114, -224, 0 },
			{ 122, 120, 2, 0 },
			{ -122, 120, 2, 0 },
			{ 153, 14, 446, 0 },
			{ -153, 14, 446, 0 },
		},
		{ 186, 122, 485, 0 }		// colBox
	},
	{
		{ { -147.8f, 15.2f, -498.5f, 0.0f }, { 147.8f, 15.2f, -498.5f, 0.0f } },		// headLight
		{ { -150.4f, 93.5f, -520.1f, 0.0f }, { 150.4f, 93.5f, -520.1f, 0.0f } },		// frontInd
		{ { -161.2f, 55.2f, 494.1f, 0.0f }, { 161.2f, 55.2f, 494.1f, 0.0f } },		// backInd
		{ { -129.2f, 70.4f, 496.8f, 0.0f }, { 129.2f, 70.4f, 496.8f, 0.0f } },		// brakeLight
		{ 159.0f, 54.8f, 495.9f, 0.0f },		// revLight
		{ { -50.0f, 0.0f, 0.0f, 0.0f }, { 50.0f, 0.0f, 0.0f, 0.0f } },		// policeLight
		{ -121.2f, 123.1f, 499.1f, 0.0f },		// exhaust
		{ 0.0f, -5.2f, -305.406f, 0.0f },		// smoke
		{ -3.7f, -5.6f, -305.21802f, 0.0f },		// fire
		{ { 179, -179, 329, 0 }, { 179, -179, -276, 0 }, { -179, -179, 329, 0 }, { -179, -179, -276, 0 } },		// wheelDisp
		7000, 8,		// powerRatio, extraInfo
		{		// cPoints
			{ 185, -117, -517, 0 },
			{ -185, -117, -517, 0 },
			{ 185, -110, 512, 0 },
			{ -185, -110, 512, 0 },
			{ 177, 8, -500, 0 },
			{ -177, 8, -500, 0 },
			{ 135, 116, -150, 0 },
			{ -135, 116, -150, 0 },
			{ 120, 116, 117, 0 },
			{ -120, 116, 117, 0 },
			{ 164, 11, 489, 0 },
			{ -164, 11, 489, 0 },
		},
		{ 186, 123, 519, 0 }		// colBox
	}
};
MATRIX float_body_mat[66];
char gang_car_vis[964];
int player_damage_factor;

// [0x4a27d0] [D]
// The player's car cosmetics (wheel positions, body points, power ...) from the chosen car.
void InitCarCosmetics(void)
{
	memcpy(car_cosmetics, &player_car_cosmetics[gPlayerCarModelType], sizeof(CAR_COSMETICS));
}

// [0x4a27f7] [D]
// Draws a car (slot -1: the player's, else a civilian slot) with its shadow, wheels, lights,
// smoke and water splash; the player's car sinks in deep water.
void DrawCar(int id, int slot)
{
	char old_clipping;
	MODEL *car_model;
	VECTOR4 position;
	int visibility;
	int cop_slot;
	int poly_block;
	float shine;
	CAR_DATA *cp;
	int normals;
	float fade_dist;
	CIV_STATE *ai;
	int civ;
	int model;
	int vertices;
	int WheelSpeed;

	// not from inside the player's car, nor when the head is turned round
	if (slot == -1 && cameraview == 8)
		return;

	if (FlipX && slot == -1)
		return;

	if (slot == -1 && cameraview == 2 && abs(gHeadTurn >> 16) > 1024)
		return;

	cp = &car_data[id];

	position.vx = (float)cp->location.vx;
	position.vy = -(float)cp->location.vy;
	position.vz = (float)cp->location.vz;

	if (cameraview != 6 && !PositionVisible(&position, CurrentPVS, camera.current_cell_x, camera.current_cell_z))
		return;

	cop_slot = 0;

	if (slot == -1)
	{
		civ = 0;
		model = 0;
	}
	else
	{
		civ = slot + 1;
		cop_slot = (unsigned char)civilian_ai_data[slot].IsCopCar;
		model = (unsigned char)civilian_ai_data[slot].model;
	}

	car_model = gCarCleanModelPtr[model];

	// the headlight pools on the road at night
	if (gNight && id != 0)
	{
		ai = &civilian_ai_data[id - 1];

		if (ai->ctrlState != CIVSTATE_PARKED
			&& car_data[id].damage[0] < 1000
			&& car_data[id].damage[1] < 1000
			&& FrustrumCheck(&position, car_model->bounding_sphere * 2.5f))
		{
			gCarPools[gCarPoolsToDraw++] = id;
		}
	}

	visibility = FrustrumCheck(&position, car_model->bounding_sphere);

	if (!visibility)
		return;

	// the body matrix in float, x and z flipped
	float_body_mat[id].m[0][0] = (float)car_data[id].where.m[0][0] * (1.0f / 4096.0f);
	float_body_mat[id].m[1][0] = (float)car_data[id].where.m[1][0] * (1.0f / 4096.0f);
	float_body_mat[id].m[2][0] = (float)car_data[id].where.m[2][0] * (1.0f / 4096.0f);
	float_body_mat[id].m[0][1] = (float)car_data[id].where.m[0][1] * (1.0f / 4096.0f);
	float_body_mat[id].m[1][1] = (float)car_data[id].where.m[1][1] * (1.0f / 4096.0f);
	float_body_mat[id].m[2][1] = (float)car_data[id].where.m[2][1] * (1.0f / 4096.0f);
	float_body_mat[id].m[0][2] = (float)car_data[id].where.m[0][2] * (1.0f / 4096.0f);
	float_body_mat[id].m[1][2] = (float)car_data[id].where.m[1][2] * (1.0f / 4096.0f);
	float_body_mat[id].m[2][2] = (float)car_data[id].where.m[2][2] * (1.0f / 4096.0f);

	float_body_mat[id].m[0][0] = -float_body_mat[id].m[0][0];
	float_body_mat[id].m[0][1] = -float_body_mat[id].m[0][1];
	float_body_mat[id].m[0][2] = -float_body_mat[id].m[0][2];
	float_body_mat[id].m[2][0] = -float_body_mat[id].m[2][0];
	float_body_mat[id].m[2][1] = -float_body_mat[id].m[2][1];
	float_body_mat[id].m[2][2] = -float_body_mat[id].m[2][2];

	position.vx = position.vx - camera_position.vx;
	position.vy = position.vy - camera_position.vy;
	position.vz = position.vz - camera_position.vz;

	CurrentRotationMatrix = &inv_camera_matrix;
	MatrixRotate(&position);

	// the car shine fades out with distance
	if (do_fogging)
		fade_dist = 1500.0f;
	else
		fade_dist = 5500.0f;

	if (position.vz > fade_dist)
		shine = 0.0f;
	else if (position.vz >= 1000.0f)
		shine = 1.0f - (position.vz - 1000.0f) / (fade_dist - 1000.0f);
	else
		shine = 1.0f;

	if (far_clip - 1000.0f < position.vz)
		return;

	// far away: the low detail model only
	if (position.vz > 15000.0f && !CamPerspectiveMode)
	{
		DrawCarModel(gCarLowModelPtr[model], &float_body_mat[id], &position, 0, id, model, gCarMaxDamage[civ], shine);
		return;
	}

	if (!gSinking || slot != -1)
	{
		old_clipping = do_clipping;
		DrawShadow(id);
		do_clipping = old_clipping;

		AddHeadLights(id);
	}

	DrawCarWheels(id, &float_body_mat[id], visibility);

	if (id == PlayerCar && !gSinking)
	{
		if (cp->wheel_speed > 0 && cp->thrust < 0)
			AddSideLights(id);
		else
			AddBrakeLight(id);

		if (cp->wheel_speed < 0 && cp->thrust < 0)
			AddReverseLight(id);

		if (cp->wheel_speed < 307200 && cp->wheel_speed > 0 && cp->wheel_angle > 200)
			AddIndicatorLight(id, 0);
		else if (cp->wheel_speed < 307200 && cp->wheel_speed > 0 && cp->wheel_angle < -200)
			AddIndicatorLight(id, 1);
	}

	// NOTE: WheelSpeed is not set while the player's car sinks (gSinking), as in the binary
	WheelSpeed = 0;

	if (!gSinking)
	{
		WheelSpeed = cp->wheel_speed;

		if (cp->damage[0] > 4000
			|| cp->damage[1] > 4000
			|| (cop_slot && cop_data[cop_slot - 1].totalDamage > gCopCarWipedOutDamage))
		{
			if (WheelSpeed < 400000 && WheelSpeed > -200000)
			{
				AddSmokingEngine(id, 1, WheelSpeed);

				if (WheelSpeed < 30000 && WheelSpeed > -30000)
					AddFlamingEngine(id);
			}
		}
		else if ((cp->damage[0] > 3000 || cp->damage[1] > 3000) && WheelSpeed < 400000 && WheelSpeed > -200000)
		{
			AddSmokingEngine(id, 0, WheelSpeed);
		}
	}

	if (WheelSpeed > 80000 || WheelSpeed < -40000)
		SwirlLeaves(&cp->location);

	if (((!gSinking && WheelSpeed < 60000 && WheelSpeed > -60000) || (gCheatFlags.SuperFastCars && id == 0))
		&& (id == 0 || civilian_ai_data[id - 1].ctrlState != CIVSTATE_PARKED))
	{
		AddExhaustSmoke(id, WheelSpeed);
	}

	// the player's car in deep water: splash, slow down, then sink
	if (gDoSmoke
		&& id == 0
		&& ROADS_GetRoadInfo(cp->location.vx, cp->location.vz)
		&& model_pointers[RouteData.type]
		&& (model_pointers[RouteData.type]->shape_flags & SHAPE_FLAG_WATER)
		&& RouteData.height + cp->location.vy < 0)
	{
		if (gTimeInWater)
		{
			AddWater(0, RouteData.height);

			car_data[0].thrust = -7000;
			car_data[0].st.n.linearVelocity[0] >>= 1;
			car_data[0].st.n.linearVelocity[1] >>= 1;
			car_data[0].st.n.linearVelocity[2] >>= 1;

			gTimeInWater--;
		}
		else
		{
			car_data[0].thrust = -7000;
			car_data[0].st.n.linearVelocity[0] >>= 1;
			car_data[0].st.n.linearVelocity[1] >>= 1;
			car_data[0].st.n.linearVelocity[2] >>= 1;

			gSinking = 1;
		}
	}

	if (id == 0 && gPlayerCarModelType == 10 && gPlayerSirenOn)
		AddCopCarLight(0, &gPlayerSirenClock, 1.0f);

	// draw with the slot's own (dented) copy of the vertices, normals and, for civilians, polygons
	vertices = car_model->vertices;
	normals = car_model->normals;

	if (civ)
		poly_block = car_model->poly_block;

	car_model->vertices = (int)gTempCarVertDump[civ];
	car_model->normals = (int)gTempCarNormalDump[civ];

	if (civ)
		car_model->poly_block = (int)gTempCarPolyDump[civ];

	if (visibility == 1)
		DrawCarModel(car_model, &float_body_mat[id], &position, 0, id, model, gCarMaxDamage[civ], shine);
	else if (visibility == 2)
		DrawCarModel(car_model, &float_body_mat[id], &position, 1, id, model, gCarMaxDamage[civ], shine);

	car_model->vertices = vertices;
	car_model->normals = normals;

	if (civ)
		car_model->poly_block = poly_block;
}

// [0x4a32ed] [D]
// Picks a moving civilian car out of the player's sight and heading away as the gang car.
int SelectGangCar(void)
{
	int gang;
	VECTOR4 pos;
	int ccz;
	int ccx;
	int angle;
	CIV_STATE *ai;
	VECTOR4 player_pos;
	int i;
	CAR_DATA *cp;

	player_pos.vx = (float)car_data[0].location.vx;
	player_pos.vz = (float)car_data[0].location.vz;

	GetVisSetAtPosition(&player_pos, gang_car_vis, &ccx, &ccz);

	ai = civilian_ai_data;
	gang = -1;

	for (i = 0; i < 64; i++, ai++)
	{
		if (ai->isGangCar)
			return i;

		if (ai->IsCopCar || ai->model == 1 || ai->ctrlState != CIVSTATE_MOVING || gang != -1 || GetCarDamage(i))
			continue;

		cp = &car_data[i + 1];

		pos.vx = (float)cp->location.vx;
		pos.vz = (float)cp->location.vz;

		if (PositionVisible(&pos, gang_car_vis, ccx, ccz))
			continue;

		if (abs(((car_data[0].direction - cp->direction + 2048) & 0xFFF) - 2048) < 1024)
			continue;

		angle = Arc_Tan(car_data[0].location.vx - float_to_int(pos.vx), car_data[0].location.vz - float_to_int(pos.vz));
		angle = abs(((cp->direction - angle + 2048) & 0xFFF) - 2048);

		if (angle <= 712)
			gang = i;
	}

	if (gang != -1)
		civilian_ai_data[gang].isGangCar = 1;

	return gang;
}

// [0x4a34f1] [D]
int GetCarDamage(int car)
{
	return civ_ai_data_pt[car].totalDamage;
}

// [0x4a350c] [D]
// Draws the four wheels, turned by the steering and spun by the wheel speed (fast spinning
// wheels use the blurred model, a lost hubcap the damaged one).
void DrawCarWheels(int id, MATRIX *matrix, char visibility)
{
	MODEL *front_model;
	MODEL *rear_model;
	MODEL *wheel_model;
	CAR_COSMETICS *car_cos;
	float *verts;
	MATRIX steer_mat;
	MATRIX turned;
	VECTOR4 pos;
	CAR_DATA *cp;
	float c;
	float s;
	float f0, f1, f2, f3, f4, f5;		// the front rim corners
	float r0, r1, r2, r3, r4, r5;		// the rear rim corners
	float dx;
	float dy;
	float dz;
	int front_speed;
	int rear_speed;
	int i;

	cp = &car_data[id];

	front_speed = cp->wheel_speed / 256;
	rear_speed = cp->wheel_speed / 256;

	if (cp->wheelspin)
		rear_speed = -700;

	if (cp->wheel[0].locked)
		front_speed = 0;

	if (cp->wheel[3].locked)
		rear_speed = 0;

	if (!GamePaused)
	{
		front_wheel_rotation[id] = (front_speed + front_wheel_rotation[id]) & 0xFFF;
		rear_wheel_rotation[id] = (rear_speed + rear_wheel_rotation[id]) & 0xFFF;
	}

	if (front_speed <= 500 && front_speed >= -500)
		front_model = gCleanWheelModelPtr;
	else
		front_model = gFastWheelModelPtr;

	if (rear_speed <= 500 && rear_speed >= -500)
		rear_model = gCleanWheelModelPtr;
	else
		rear_model = gFastWheelModelPtr;

	steer_mat.m[0][0] = 1.0f;
	steer_mat.m[0][1] = 0.0f;
	steer_mat.m[0][2] = 0.0f;
	steer_mat.m[1][0] = 0.0f;
	steer_mat.m[1][1] = 1.0f;
	steer_mat.m[1][2] = 0.0f;
	steer_mat.m[2][0] = 0.0f;
	steer_mat.m[2][1] = 0.0f;
	steer_mat.m[2][2] = 1.0f;

	RotMatrixY(&steer_mat, cp->wheel_angle & 0xFFF);
	MulMatrix(&steer_mat, matrix, &turned);

	c = fcos(front_wheel_rotation[id]) * WHEEL_RADIUS;
	s = fsin(front_wheel_rotation[id]) * WHEEL_RADIUS;

	f0 = c - s;
	f1 = s + c;
	f2 = c + s;
	f3 = s - c;
	f4 = -s - c;
	f5 = -c - s;

	c = fcos(rear_wheel_rotation[id]) * WHEEL_RADIUS;
	s = fsin(rear_wheel_rotation[id]) * WHEEL_RADIUS;

	r0 = c - s;
	r1 = s + c;
	r2 = c + s;
	r3 = s - c;
	r4 = -s - c;
	r5 = -c - s;

	if (gUseAlphaTests && gPreviousAlpha != 1)
	{
		grAlphaCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXTRUE);
		grAlphaTestFunction(GR_CMP_GREATER);
		gPreviousAlpha = 1;
	}

	dx = (float)cp->location.vx - camera_position.vx;
	dy = (float)-cp->location.vy - camera_position.vy;
	dz = (float)cp->location.vz - camera_position.vz;

	car_cos = &car_cosmetics[(unsigned char)cp->model];

	wheel_model = NULL;

	for (i = 0; i < 4; i++)
	{
		if (id == 0 && !gHubcap.Present[3 - i])
			wheel_model = gDamWheelModelPtr;
		else if (i & 1)
			wheel_model = rear_model;
		else
			wheel_model = front_model;

		if (i & 2)
			pos.vx = (float)(-car_cos->wheelDisp[i].vx - 25);
		else
			pos.vx = (float)(25 - car_cos->wheelDisp[i].vx);

		pos.vy = (float)(-car_cos->wheelDisp[i].vy - 70);
		pos.vy = pos.vy + (float)(20 - (unsigned char)cp->wheel[i].susCompression);
		pos.vz = (float)-car_cos->wheelDisp[i].vz;

		CurrentRotationMatrix = matrix;
		MatrixRotateB(&pos);

		pos.vx = pos.vx + dx;
		pos.vy = pos.vy + dy;
		pos.vz = pos.vz + dz;

		CurrentRotationMatrix = &inv_camera_matrix;
		MatrixRotate(&pos);

		// spin the rim: the y / z of vertices 8 - 15 (VECTOR4 vertices)
		verts = (float *)wheel_model->vertices;

		if (i & 1)
		{
			verts[33] = r0;
			verts[49] = r0;
			verts[34] = r1;
			verts[50] = r1;
			verts[37] = r5;
			verts[53] = r5;
			verts[38] = r0;
			verts[54] = r0;
			verts[41] = r3;
			verts[57] = r3;
			verts[42] = r4;
			verts[58] = r4;
			verts[45] = r2;
			verts[61] = r2;
			verts[46] = r3;
			verts[62] = r3;

			if (gDoTraining && id == 65)
				sub_42BD3B(wheel_model, matrix, (VECTOR *)&pos, visibility - 1);
			else
				DrawObject(wheel_model, matrix, &pos, visibility - 1);
		}
		else
		{
			verts[33] = f0;
			verts[49] = f0;
			verts[34] = f1;
			verts[50] = f1;
			verts[37] = f5;
			verts[53] = f5;
			verts[38] = f0;
			verts[54] = f0;
			verts[41] = f3;
			verts[57] = f3;
			verts[42] = f4;
			verts[58] = f4;
			verts[45] = f2;
			verts[61] = f2;
			verts[46] = f3;
			verts[62] = f3;

			if (gDoTraining && id == 65)
				sub_42BD3B(wheel_model, &turned, (VECTOR *)&pos, visibility - 1);
			else
				DrawObject(wheel_model, &turned, &pos, visibility - 1);
		}
	}

	if (gUseAlphaTests && gPreviousAlpha != 0)
	{
		grAlphaCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_ONE, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_CONSTANT, FXTRUE);
		grAlphaTestFunction(GR_CMP_ALWAYS);
		gPreviousAlpha = 0;
	}
}
