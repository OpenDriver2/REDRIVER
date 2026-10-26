/*
 * flat/flat.c - motel room ("flat") main loop
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "motel\flat.c=flat\flat.c"); partially translated.
 */

#include <stddef.h>

typedef unsigned char u_char;
typedef unsigned char byte;

typedef struct RGB
{
	byte r;
	byte g;
	byte b;
	byte pad;
} RGB;

typedef struct FE_TEXT
{
	short x;
	short y;
	u_char font;
	u_char justify;
	char field_6;
	char field_7;
	char *text;
	RGB color;
} FE_TEXT;

typedef struct TEX_INFO TEX_INFO;
typedef struct MLADDER_ROOM MLADDER_ROOM;

/* flat/frontend.c (IDA motel\frontend.c) */
extern void InitFlat(void);
extern void QuitFlat(void);
extern void DrawMotel(FE_TEXT *texts, void (*drawicons)(void));
extern void SetMotelIntensity(float intensity);
extern int flat_retcode;			// @motel\frontend.c
extern int motel_font_override;		// @motel\frontend.c
extern float gMotelFadeIntensity;	// @motel\frontend.c

/* flat/prescam.c (IDA motel\path.c) */
extern void PlaceCameraAt(int position);
extern int gCameraOption;			// @motel\path.c
extern int gCameraMoving;			// @motel\path.c

/* frontend/config.c, Engine/3dfx/gfxinit.c, map.c, general.c, shell.c, input.c */
extern int LoadSavedOptions(void);
extern int ResetViewport(int a1, int a2, char a3);
extern TEX_INFO *GetTextureDetails(char *name);
extern int WaitForSync(int delta);
extern void HandleWinMessages(void);
extern int ReadInput(void);

extern float screen_scale;

void FadeInOptionName(void);
void DoMotelCamera(int camera);
int ControlMotel(void);
void DrawIcons(void);	// NOTE: static in the original (flat._DrawIcons); not translated yet, stubbed

//------------------------------------------------------------------------------------------
// initialised data (0x550610)

static FE_TEXT textitems[2] = {
	{ 320, 430, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

//------------------------------------------------------------------------------------------
// uninitialised data (0x600ca8)
// NOTE: the slots right before (0x600ca4) and after (0x600cbc) are unmarked and unreferenced;
// their owner (exit.c / flat.c / frontend.c) is unknown, so they are not declared here

int queuecam;
TEX_INFO *LeftArrowTexture;
TEX_INFO *RightArrowTexture;
int (*queuefunc)(void);
MLADDER_ROOM *room_data;

// [0x418060] [D]
int DoFlat(MLADDER_ROOM *room)
{
	int ret;
	float old_screen_scale;

	LoadSavedOptions();

	room_data = room;
	motel_font_override = 0;

	old_screen_scale = screen_scale;
	screen_scale = 1.0f;

	ResetViewport(0, 0, 0);

	queuefunc = NULL;

	InitFlat();

	LeftArrowTexture = GetTextureDetails("LEFTARRO");
	RightArrowTexture = GetTextureDetails("RIGHTARR");

	gCameraOption = 4;
	PlaceCameraAt(9);

	gMotelFadeIntensity = 0.0f;
	SetMotelIntensity(1.0f);

	FadeInOptionName();
	DoMotelCamera(4);

	flat_retcode = -1;

	WaitForSync(0);

	for (;;)
	{
		HandleWinMessages();
		ReadInput();

		if (ControlMotel() == -1)
			break;

		if (queuefunc && !gCameraMoving)
		{
			ret = queuefunc();

			if (ret == 1)
				break;

			DoMotelCamera(queuecam);
			queuefunc = NULL;
		}

		DrawMotel(textitems, DrawIcons);
	}

	QuitFlat();

	screen_scale = old_screen_scale;
	return flat_retcode;
}
