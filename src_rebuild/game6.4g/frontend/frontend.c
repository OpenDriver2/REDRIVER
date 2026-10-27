/*
 * frontend/frontend.c - frontend core: screens, menus, game start
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "frontend\frontend.c"); partially translated.
 */

#define DIRECTINPUT_VERSION 0x0500

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <dinput.h>

#include "dr_types.h"
#include "dr_globals.h"
#include "eax/cdaudio.h"
#include "Engine/3dfx/gfxinit.h"
#include "Engine/3dfx/tset.h"
#include "frontend/config.h"
#include "frontend/demo.h"
#include "frontend/font.h"
#include "frontend/frontend.h"
#include "input.h"
#include "map.h"
#include "shell.h"

#include "../D3DGlide/glide_ptrs.h"
#include "debug.h"
#include "frontend/cpupoly.h"
#include "general.h"
#include "glaunch.h"
#include "sound.h"

// a texture set in a .TSD file
typedef struct TSET_INFO
{
	int length;
	int uv_offset;
	int bitmap_offset;
	short num_textures;
	unsigned short flags;
	unsigned short number;
	short pad;
} TSET_INFO;

typedef struct TSD_UV_INFO
{
	unsigned int id;
	u_char U;
	u_char V;
	short width;
	short height;
	short file_num;		// index of the texture's name in the set's name list
} TSD_UV_INFO;

#pragma pack(push, 2)
typedef struct BMP_HEADER
{
	BITMAPFILEHEADER file;
	BITMAPINFOHEADER info;
} BMP_HEADER;
#pragma pack(pop)

#define SCREEN_IMAGE_WIDTH		640
#define SCREEN_IMAGE_HEIGHT		480

#define TEXT3					4	// POLY_TYPES: textured triangle

#define FE_POLY_TEXTURED		0x1		// FE_POLY.flags: text is a texture name (else a filled rectangle)
#define FE_POLY_END				0x4
#define FE_POLY_HIRES			0x8		// drawn by CPUEndOfFrame instead

#define TSD_PAGE_SIZE			0x10000	// a 256x256 8 bit texture page
#define NUM_PALETTES			50

int SetFullDisplay(int width, int height);
void ClearFullDisplay(int count);
void FE_PlayClickSound(void);
void SetupBackgroundPolys(void);
void DrawPolys(void);
void DrawFrontendTexts(void);
static void DrawFrontendSprite(int x, int y, RGBA *colour, char *name);
int LoadFrontendTextures(char *filename);
void *AllocateTSet(FILE *handle, float version, TSET_INFO *info);
void AddTSet(TSET_INFO *info, void *data, char tset);
static int LoadScreenImage(const char *filename, char *image);
static void UploadScreenImage(char *image);
static unsigned short *GetScreenTile(char *image, int tx, int ty);
static void DrawScreenImage(float brightness);
static void DrawScreenTile(int x, int y, int w, int h, int tset, u_char *colour);

// replay slot used to restart / view the last game
#define TEMP_REPLAY_SLOT	8

//------------------------------------------------------------------------------------------
// initialised data (0x54caf0)

// NOTE: frontend._backcol is not static: main.c's SetupQuit greys it out (IDA mark @frontend\frontend.c)
RGBA backcol = { -1, -1, -1, 0 };
int WantedGameMode = -1;
// NOTE: frontend._delval.0 (0x54caf8, = 10) follows: a function-local static of HandleMenu

//------------------------------------------------------------------------------------------
// uninitialised data (0x5fa0b0)

FE_TEXT *cur_texts;
int draw_overlay;
int overlay_val;
int on_slider;
int exit_flag;
int wanted_exit_flag;
int gInFrondend;
int gbPlayingDemo;
int fe_interp_delay;
FE_POLY *fe_cur_polys;
int fe_key_delay;
int fe_key_pressed;
// NOTE: frontend._delay.0 (0x5fa0e0) follows: a function-local static of HandleMenu

// NOTE: bss globals in the linker's common area (0x12f0500, 0x12f0ac0); defined here as
// InitFrontend builds them
int fe_palette2[256];		// hires polys (CPUDrawHiresTexture)
int fe_palette1[256];		// vscreen

// uninitialised data (0x5da0b0, marked @frontend\frontend.c)
// NOTE: unnamed in IDA (word_5DA0B0); name guessed
static unsigned short tile_buffer[256 * 256];

// [0x40c180] [D]
// Sets up the 640x480 frontend: textures, CPU drawing palettes, fonts, sounds.
int InitFrontend(void)
{
	inv_camera_matrix.m[0][0] = 1.0f;
	inv_camera_matrix.m[1][0] = 0.0f;
	inv_camera_matrix.m[2][0] = 0.0f;
	inv_camera_matrix.m[0][1] = 0.0f;
	inv_camera_matrix.m[1][1] = 1.0f;
	inv_camera_matrix.m[2][1] = 0.0f;
	inv_camera_matrix.m[0][2] = 0.0f;
	inv_camera_matrix.m[1][2] = 0.0f;
	inv_camera_matrix.m[2][2] = 1.0f;

	camera_position.vx = 0.0f;
	camera_position.vy = 0.0f;
	camera_position.vz = 0.0f;

	SetListenerPosition(0.0f, 0.0f, 0.0f);

	LoadSavedScores();
	LoadSavedOptions();

	if (display_width != 640 || display_height != 480)
		SetFullDisplay(640, 480);

	ClearFullDisplay(4);

	grClipWindow(0, 48, 640, 432);

	if (!InitCPUPolyInterface())
		DisplayErrorAndExit("Failed to allocate display");

	gInPresentation = 1;
	gInFrondend = 1;

	if (!LoadFrontendTextures("DATA\\FRONTEND.TSD"))
		return 0;

	CreateCPUPaletteLookup((RGB *)gPalette[3], fe_palette2, 113, 255);
	CreateCPUPaletteLookup((RGB *)gPalette[2], fe_palette1, 113, 255);
	SetCPUPalette(fe_palette1);

	LoadFont("F1_", 0, -3, 8, 2);
	LoadFont("F2_", 1, -2, 8, 2);
	LoadFont("F3_", 2, -2, 6, 2);
	LoadFont("F4_", 3, -2, 6, 2);

	font_subpixel_adjust = 0.0f;
	draw_overlay = 0;

	ReleaseSounds();

	LoadSounds("TITLE.DSS", 0);
	SetSoundSystemEnvironment(0);

	if (gUseCDAudio)
		PlayCDTrack(2);

	WaitForSync(0);
	fe_interp_delay = 1;

	return 1;
}

// [0x40c390] [D]
void QuitFrontend(void)
{
	ResetDemoTimer();

	gInPresentation = 0;
	gInFrondend = 0;

	if (gUseCDAudio)
		StopCDTrack();

	ReleaseSounds();

	if (gRamTextureRawDataDump)
	{
		DebugFree((void *)gRamTextureRawDataDump);
		gRamTextureRawDataDump = 0;
		gCurrentRamBytesUsed = 0;
	}

	if (tinfo_block)
	{
		DebugFree(tinfo_block);
		tinfo_block = NULL;
		num_textures = 0;
	}

	font_subpixel_adjust = 0.5f;

	ClearFullDisplay(4);
	UpdateSavedScores();
}

// [0x40c431] [D]
// NOTE: Hex-Rays fails on the exit_flag switch; translated from the disassembly
void StartGame(int do_credits)
{
	CHEATS cheat_flags;
	int error;

	cheat_flags = gCheatFlags;

	if (wanted_display_width != display_width || wanted_display_height != display_height)
		SetFullDisplay(wanted_display_width, wanted_display_height);

	HandleWinMessages();

	if (do_credits)
	{
		exit_flag = 4;
		gDisableConfig = 1;
	}
	else
	{
		exit_flag = 0;
		gDisableConfig = 0;
	}

	WantedGameMode = -1;

	error = 0;

	while (!error)
	{
		wanted_exit_flag = 1;
		quit = 1;
		GamePaused = 0;

		GameInit();

		if (exit_flag == 4 || exit_flag == 5)
			CamerasChanged[3] = 1;

		while (quit)
		{
			while (!gScreen.active)
				HandleWinMessages();

			if (glide_active)
				GameLoop();

			HandleWinMessages();
		}

		GameExit();

		exit_flag = wanted_exit_flag;

		switch (exit_flag)
		{
			case 2:
				error = 1;
				break;

			case 3:
				exit_flag = 0;
				break;

			case 4:
			case 5:
				if (!UpdateSavedReplay(TEMP_REPLAY_SLOT, "TEMP"))
				{
					LogQuick("ERROR: Can't save replay");
					error = 1;
				}
				else if (!LoadSavedReplay(TEMP_REPLAY_SLOT))
				{
					LogQuick("ERROR: Can't load replay");
					error = 1;
				}
				break;

			case 6:
				if (WantedGameMode != -1)
				{
					exit_flag = 0;
					GameMode = WantedGameMode;
				}
				else
				{
					LogQuick("ERROR: Missing training script number");
					error = 1;
				}
				break;

			default:
				error = 1;
				break;
		}
	}

	gDoInterview = 0;
	gDoTraining = 0;
	gNight = 0;
	gDoRain = 0;
	gDoSnow = 0;

	remove("Scripts\\Waypoint.tmp");

	gCheatFlags = cheat_flags;

	gDisableConfig = 0;

	LoadSavedOptions();
	UpdateSavedOptions();
}

// [0x40c68b] [D]
void StartGameFromFrontend(int do_gamemode, int do_credits)
{
	QuitFrontend();

	if (!do_gamemode)
		GameMode = 0;

	StartGame(do_credits);
	InitFrontend();
}

// [0x40c769] [D]
// Reopens the 3D display at another resolution (falls back to the old one).
int SetFullDisplay(int width, int height)
{
	int old_width;
	int old_height;

	old_width = display_width;
	old_height = display_height;

	ShutdownDrawingSystem();

	display_width = width;
	display_height = height;

	if (!InitDrawingSystem(gScreen.handle, gScreen.fullscreen, 0))
	{
		display_width = old_width;
		display_height = old_height;

		if (!InitDrawingSystem(gScreen.handle, gScreen.fullscreen, 0))
			DisplayErrorAndExit("FATAL: Can't allocate 3D hardware");
	}

	return 1;
}

// [0x40c7f4] [D]
// Clears both buffers (count swaps).
void ClearFullDisplay(int count)
{
	int i;

	grClipWindow(0, 0, display_width, display_height);

	for (i = 0; i < count; i++)
	{
		grBufferClear(0, 0, GR_WDEPTHVALUE_FARTHEST);
		grBufferSwap(0);
	}
}

// [0x40c8f1] [D]
// Shows a 640x480 24 bit BMP: fades in, waits `seconds` (or a key), fades out.
void FadeInScreen(const char *filename, int seconds)
{
	char *image;
	float i;

	grClipWindow(0, 0, display_width, display_height);

	// NOTE: the binary passes "D:\\Driver\\game6.4g\\frontend\\frontend.c", 501 here
	image = (char *)DebugMalloc(SCREEN_IMAGE_WIDTH * SCREEN_IMAGE_HEIGHT * 4, __FILE__, __LINE__);

	if (image)
	{
		if (LoadScreenImage(filename, image))
		{
			UploadScreenImage(image);

			for (i = 0.0f; i < 1.0f; i += 0.05f)
				DrawScreenImage(i);

			DrawScreenImage(1.0f);
			DrawScreenImage(1.0f);
			DrawScreenImage(1.0f);
			DrawScreenImage(1.0f);

			StartDemoTimer();

			while (demo_timer_count < seconds && !AnyKeyPressed())
				;

			ResetDemoTimer();

			for (i = 1.0f; i > 0.0f; i -= 0.05f)
				DrawScreenImage(i);

			DrawScreenImage(0.0f);
			DrawScreenImage(0.0f);
			DrawScreenImage(0.0f);
			DrawScreenImage(0.0f);
		}

		DebugFree(image);
	}
}

// [0x40ca9d] [D]
void FE_PlayClickSound(void)
{
	SimplePlaySound(0, 9, -1);
}

// [0x40cab0] [D]
void FE_PlaySelectSound(void)
{
	SimplePlaySound(1, 9, -1);
}

// [0x40cac3] [D]
void FE_PlayErrorSound(void)
{
	SimplePlaySound(2, 9, -1);
}

// [0x40cad6] [D]
void ResetMenu(FE_BUTTON *buttons, int selection)
{
	int i;

	for (i = 0; buttons[i].choosefunc; i++)
		buttons[i].unselfunc(0);

	buttons[selection].selfunc(0);
}

// [0x40cb27] [D]
// Moves the selection with the arrow keys, flashes it, runs choosefunc on Return.
// Returns -1 on Escape, else choosefunc's result (0 when nothing was chosen).
int HandleMenu(FE_BUTTON *buttons, int *selection)
{
	static int delay;
	static int delval = 10;
	int ret;
	int cursel;

	ret = 0;
	cursel = *selection;

	if (CheckKey(DIK_ESCAPE))
	{
		buttons[cursel].unselfunc(0);
		return -1;
	}

	if (--fe_key_delay <= 0)
	{
		fe_key_delay = 20;
		fe_key_pressed ^= 1;
	}

	if (fe_key_pressed)
		buttons[*selection].selfunc(1);
	else
		buttons[*selection].unselfunc(1);

	if (on_slider)
	{
		// left / right auto-repeat: 40 frames, then every frame
		if (!((KeyMap[DIK_RIGHTARROW] | KeyMap[DIK_LEFTARROW]) & 0x80))
		{
			delay = 0;
			delval = 40;
		}

		if (delay)
		{
			delay--;
		}
		else if (KeyMap[DIK_LEFTARROW] & 0x80)
		{
			buttons[buttons[*selection].l].selfunc(0);

			delay = delval;
			delval = 1;

			FE_PlayClickSound();
		}
		else if (KeyMap[DIK_RIGHTARROW] & 0x80)
		{
			buttons[buttons[*selection].r].selfunc(0);

			delay = delval;
			delval = 1;

			FE_PlayClickSound();
		}
	}
	else if (CheckKey(DIK_LEFT))
	{
		*selection = buttons[*selection].l;
		FE_PlayClickSound();
	}
	else if (CheckKey(DIK_RIGHT))
	{
		*selection = buttons[*selection].r;
		FE_PlayClickSound();
	}

	if (CheckKey(DIK_UP))
	{
		*selection = buttons[*selection].u;
		FE_PlayClickSound();
	}
	else if (CheckKey(DIK_DOWN))
	{
		*selection = buttons[*selection].d;
		FE_PlayClickSound();
	}
	else if (CheckKey(DIK_RETURN))
	{
		FE_PlayClickSound();
		ret = buttons[*selection].choosefunc();
	}

	if (cursel != *selection)
	{
		buttons[cursel].unselfunc(0);
		buttons[*selection].selfunc(0);
	}

	return ret;
}

// [0x40cdd5] [D]
// Draws a frontend frame (when the frame interpolation delay has run out).
void DrawFrontend(void)
{
	int h;
	RGBA black;

	while (!gScreen.active)
		HandleWinMessages();

	if (--fe_interp_delay)
		return;

	if (gUseCDAudio && !gMusicPlaying)
		PlayCDTrack(2);

	UpdateSound();
	ReallyUpdateSound();

	StartOfFrame();
	CPUStartOfFrame();

	SetupBackgroundPolys();
	DrawPolys();
	DrawFrontendTexts();

	// screen transition: black bands closing in from the top and bottom
	if (draw_overlay)
	{
		black.r = 0;
		black.g = 0;
		black.b = 0;
		black.a = 0;

		h = (int)((float)overlay_val * 38.4f);

		CPUFillRect(0, 48, &black, 640, h);
		CPUFillRect(0, 432 - h, &black, 640, h + 1);
	}

	CPUDisplayDebugStats(0, 255);
	CPUEndOfFrame();

	EndOfFrame();

	fe_interp_delay = WaitForSync(0);
}

// [0x40cee7] [D]
// Shows a new screen (polys and texts) with the opening transition.
void OpenFrontendScreen(FE_POLY *polys, FE_TEXT *texts)
{
	int i;

	fe_cur_polys = polys;
	cur_texts = texts;

	draw_overlay = 1;

	for (i = 5; i >= 0; i--)
	{
		HandleWinMessages();
		ReadInput();

		overlay_val = i;
		DrawFrontend();
	}

	draw_overlay = 0;

	StartDemoTimer();
}

// [0x40cf4a] [D]
// Closes the current screen with the closing transition.
void CloseFrontendScreen(void)
{
	int i;

	ResetDemoTimer();
	FE_PlaySelectSound();

	draw_overlay = 1;

	for (i = 0; i <= 5; i++)
	{
		HandleWinMessages();
		ReadInput();

		overlay_val = i;
		DrawFrontend();
	}

	draw_overlay = 0;
}

// [0x40cfa2] [D]
void DrawPolys(void)
{
	FE_POLY *poly;

	poly = fe_cur_polys;

	if (!poly)
		return;

	while (!(poly->flags & FE_POLY_END))
	{
		if (!(poly->flags & FE_POLY_HIRES))
		{
			if (poly->flags & FE_POLY_TEXTURED)
				DrawFrontendSprite(poly->x, poly->y, &poly->color, poly->text);
			else
				CPUFillRect(poly->x, poly->y, &poly->color, poly->field_0A, poly->field_0C);
		}

		poly++;
	}
}

// [0x40d064] [D]
void DrawFrontendTexts(void)
{
	FE_TEXT *fet;

	fet = cur_texts;

	if (!fet)
		return;

	PrintReset();

	while (fet->text)
	{
		PrintColour(fet->color.r, fet->color.g, fet->color.b);

		if (fet->font >= 2)
			PrintAt((float)fet->x, (float)fet->y - 3.0f, fet->font, fet->justify);
		else
			PrintAt((float)fet->x, (float)fet->y - 1.0f, fet->font, fet->justify);

		PrintText(fet->text);

		fet++;
	}
}

// [0x40d168] [D]
void SetupBackgroundPolys(void)
{
	DrawFrontendSprite(0, 48, &backcol, "BKGRND1A");
	DrawFrontendSprite(256, 48, &backcol, "BKGRND1B");
	DrawFrontendSprite(512, 48, &backcol, "BKGRND1C");
	DrawFrontendSprite(0, 304, &backcol, "BKGRND1D");
	DrawFrontendSprite(256, 304, &backcol, "BKGRND1E");
	DrawFrontendSprite(512, 304, &backcol, "BKGRND1F");
}

// [0x40d212] [D]
// NOTE: unnamed in IDA (sub_40D212); name guessed. Draws a frontend texture; the colour is
// not used.
static void DrawFrontendSprite(int x, int y, RGBA *colour, char *name)
{
	int w;
	int h;
	TEX_INFO *info;

	info = GetTextureDetails(name);

	if (!info)
		return;

	if (info->w)
		w = info->w;
	else
		w = 256;

	if (info->h)
		h = info->h;
	else
		h = 256;

	CPUDrawTexture(x, y, info->tset, info->u, info->v, w, h);
}

// [0x40d2ae] [D]
// Loads a .TSD file: palettes, then 8 bit texture pages (aligned to 64K, as the tsd.c blitters
// need) and the texture names / positions in tinfo_block.
int LoadFrontendTextures(char *filename)
{
	FILE *handle;
	char id[4];
	float version;
	short num_palettes;
	short palette;
	short skip;
	int i;
	int tset;
	int start;
	int total_size;
	int total_textures;
	char *data;
	char *tset_data;
	TSET_INFO info;
	// NOTE: the 8 bytes after the version; meaning of the first 4 unknown, names guessed
	struct
	{
		int unknown;
		short page_size;
		short num_tsets;
	} header;

	// Reset PalettesPresent
	for (i = 0; i < NUM_PALETTES; i++)
		gPalettePresent[i] = 0;

	handle = fopen(filename, "rb");

	if (!handle)
		return 0;

	fread(id, 1, 4, handle);

	if (id[0] != 'T' || id[1] != 'S' || id[2] != 'D' || id[3] != ' ')
	{
		fclose(handle);
		return 0;
	}

	fread(&version, 1, 4, handle);
	fread(&header, 1, 8, handle);

	if (header.page_size != 256)
	{
		fclose(handle);
		return 0;
	}

	if (version == 1.0f)
	{
		fread(gPalette[1], 1, 1024, handle);
		gPalettePresent[1] = 1;

		fread(gPalette[0], 1, 1024, handle);
		gPalettePresent[0] = 1;
	}
	else
	{
		fread(&num_palettes, 1, 2, handle);

		for (i = 0; i < num_palettes; i++)
		{
			fread(&palette, 1, 2, handle);
			fread(&palette, 1, 2, handle);

			fread(gPalette[palette], 1, 1024, handle);
			gPalettePresent[palette] = 1;
		}
	}

	// count the texture sets and textures
	start = ftell(handle);
	total_size = 0;
	total_textures = 0;

	for (tset = 0; tset < header.num_tsets; tset++)
	{
		if (version > 1.0f)
			fread(&skip, 1, 2, handle);

		fread(&info, 1, sizeof(TSET_INFO), handle);

		info.length -= sizeof(TSET_INFO);
		info.uv_offset -= sizeof(TSET_INFO);
		info.bitmap_offset -= sizeof(TSET_INFO);

		total_textures += info.num_textures;
		total_size += TSD_PAGE_SIZE;

		fseek(handle, info.length, SEEK_CUR);
	}

	if (total_size)
	{
		// NOTE: the binary passes "D:\\Driver\\game6.4g\\frontend\\frontend.c", 943 here
		gRamTextureRawDataDump = (int)DebugMalloc(total_size + 0xFFFF, __FILE__, __LINE__);

		if (!gRamTextureRawDataDump)
		{
			fclose(handle);
			return 0;
		}
	}

	// start the pages on a 64K boundary
	gCurrentRamBytesUsed = (unsigned short)-(short)gRamTextureRawDataDump;

	// NOTE: the binary passes "D:\\Driver\\game6.4g\\frontend\\frontend.c", 953 here
	tinfo_block = (TEX_INFO *)DebugMalloc(sizeof(TEX_INFO) * total_textures, __FILE__, __LINE__);

	if (!tinfo_block)
	{
		fclose(handle);
		return 0;
	}

	fseek(handle, start, SEEK_SET);

	num_textures = 0;

	for (tset = 0; tset < header.num_tsets; tset++)
	{
		tset_data = AllocateTSet(handle, version, &info);

		AddTSet(&info, tset_data, tset);
		data = tset_data + info.bitmap_offset;

		TextureSlots[tset].data = (void *)(gCurrentRamBytesUsed + gRamTextureRawDataDump);
		memcpy(TextureSlots[tset].data, data, TSD_PAGE_SIZE);
		gCurrentRamBytesUsed += TSD_PAGE_SIZE;

		DebugFree(tset_data);
	}

	fclose(handle);
	return 1;
}

// [0x40d6e6] [D]
// Reads the next texture set of a .TSD file into a new buffer.
void *AllocateTSet(FILE *handle, float version, TSET_INFO *info)
{
	short skip;
	void *data;

	if (version > 1.0f)
		fread(&skip, 1, 2, handle);

	fread(info, 1, sizeof(TSET_INFO), handle);

	info->length -= sizeof(TSET_INFO);
	info->uv_offset -= sizeof(TSET_INFO);
	info->bitmap_offset -= sizeof(TSET_INFO);

	// NOTE: the binary passes "D:\\Driver\\game6.4g\\frontend\\frontend.c", 998 here
	data = DebugMalloc(info->length, __FILE__, __LINE__);

	if (!data)
	{
		fclose(handle);
		return NULL;
	}

	fread(data, 1, info->length, handle);
	return data;
}

// [0x40d79f] [D]
// Adds the textures of a set to tinfo_block (names: upper case, up to 8 chars, no extension).
void AddTSet(TSET_INFO *info, void *data, char tset)
{
	int i;
	int j;
	int file_num;
	u_char *name;
	TSD_UV_INFO *tex;

	tex = (TSD_UV_INFO *)((char *)data + info->uv_offset);

	for (i = 0; i < info->num_textures; i++)
	{
		// the set starts with its texture file names, one after another
		file_num = tex->file_num;
		name = (u_char *)data;

		while (file_num--)
		{
			while (*name++)
				;
		}

		for (j = 0; j < 8; j++)
		{
			if (name[j] == 0 || name[j] == '.')
			{
				tinfo_block[num_textures].name[j] = 0;
				break;
			}

			tinfo_block[num_textures].name[j] = toupper(name[j]);
		}

		tinfo_block[num_textures].tset = tset;
		tinfo_block[num_textures].u = tex->U;
		tinfo_block[num_textures].v = tex->V;
		tinfo_block[num_textures].w = (u_char)tex->width;
		tinfo_block[num_textures].h = (u_char)tex->height;

		tex++;
		num_textures++;
	}
}

// [0x40d91b] [D]
// NOTE: unnamed in IDA (sub_40D91B); name guessed
// Loads a 640x480 uncompressed 24 bit BMP into a 32 bit RGBx image.
static int LoadScreenImage(const char *filename, char *image)
{
	FILE *fp;
	int ok;
	int y;
	int x;
	BMP_HEADER header;

	ok = 0;

	fp = fopen(filename, "rb");

	if (fp)
	{
		fread(&header, 1, sizeof(BMP_HEADER), fp);

		if (header.file.bfType == 'MB' && header.info.biWidth == SCREEN_IMAGE_WIDTH && header.info.biHeight == SCREEN_IMAGE_HEIGHT &&
			header.info.biBitCount == 24 && header.info.biCompression == BI_RGB)
		{
			// rows are stored bottom up, pixels as B, G, R
			for (y = header.info.biHeight - 1; y >= 0; y--)
			{
				for (x = 0; x < header.info.biWidth; x++)
				{
					image[4 * header.info.biWidth * y + 4 * x + 2] = fgetc(fp);
					image[4 * header.info.biWidth * y + 4 * x + 1] = fgetc(fp);
					image[4 * header.info.biWidth * y + 4 * x + 0] = fgetc(fp);
				}
			}

			ok = 1;
		}

		fclose(fp);
	}

	return ok;
}

// [0x40dc89] [D]
// NOTE: unnamed in IDA (sub_40DC89); name guessed
// Cuts the image into 256x256 tiles and loads them as texture sets 0..5.
static void UploadScreenImage(char *image)
{
	int y;
	int x;
	unsigned short *tile;
	int tset;

	InitTextureSpace();

	tset = 0;

	for (y = 0; y < SCREEN_IMAGE_HEIGHT; y += 256)
	{
		for (x = 0; x < SCREEN_IMAGE_WIDTH; x += 256)
		{
			tile = GetScreenTile(image, x, y);
			AddTextureSet16(tset++, tile);
		}
	}
}

// [0x40dd14] [D]
// NOTE: unnamed in IDA (sub_40DD14); name guessed
// Converts a 256x256 part of the image to ARGB1555 (alpha 0) in tile_buffer.
static unsigned short *GetScreenTile(char *image, int tx, int ty)
{
	int x;
	int y;
	u_char *pixel;

	for (y = 0; y < 256; y++)
	{
		if (y + ty < SCREEN_IMAGE_HEIGHT)
		{
			for (x = 0; x < 256; x++)
			{
				if (x + tx < SCREEN_IMAGE_WIDTH)
				{
					pixel = (u_char *)&image[SCREEN_IMAGE_WIDTH * 4 * (ty + y) + 4 * (tx + x)];

					tile_buffer[256 * y + x] = (pixel[2] >> 3) | ((pixel[1] >> 3) << 5) | ((pixel[0] >> 3) << 10);
				}
			}
		}
	}

	return tile_buffer;
}

// [0x40de21] [D]
// NOTE: unnamed in IDA (sub_40DE21); name guessed
// Draws the uploaded image at the given brightness (0..1) and shows it.
static void DrawScreenImage(float brightness)
{
	int y;
	int x;
	int h;
	int w;
	int tset;
	u_char colour[4];

	colour[0] = (u_char)(int)(brightness * 255.0f);
	colour[1] = (u_char)(int)(brightness * 255.0f);
	colour[2] = (u_char)(int)(brightness * 255.0f);

	tset = 0;

	for (y = 0; y < SCREEN_IMAGE_HEIGHT; y += 256)
	{
		h = 256;

		if (y + 256 > SCREEN_IMAGE_HEIGHT)
			h = SCREEN_IMAGE_HEIGHT - 256;

		for (x = 0; x < SCREEN_IMAGE_WIDTH; x += 256)
		{
			w = 256;

			if (x + 256 > SCREEN_IMAGE_WIDTH)
				w = SCREEN_IMAGE_WIDTH - 512;

			DrawScreenTile(x, y, w, h, tset++, colour);
		}
	}

	grBufferSwap(1);
}

// [0x40df13] [D]
// NOTE: unnamed in IDA (sub_40DF13); name guessed
// Draws one tile as a textured quad shaded with colour.
static void DrawScreenTile(int x, int y, int w, int h, int tset, u_char *colour)
{
	float oow;
	float tw;
	float th;
	GrVertex v[4];

	// the D3D emulation takes texture coords and z from oow
	if (using_glide)
		oow = 1.0f;
	else
		oow = 1.0f / 64.0f;

	tw = (float)w - 0.5f;
	th = (float)h - 0.5f;

	v[0].x = (float)x;
	v[0].y = (float)y;
	v[0].oow = oow;
	v[0].r = (float)colour[0];
	v[0].g = (float)colour[1];
	v[0].b = (float)colour[2];
	v[0].a = 255.0f;
	v[0].tmuvtx[0].sow = 0.5f * oow;
	v[0].tmuvtx[0].tow = v[0].tmuvtx[0].sow;

	v[1].x = (float)(x + w);
	v[1].y = v[0].y;
	v[1].oow = oow;
	v[1].r = (float)colour[0];
	v[1].g = (float)colour[1];
	v[1].b = (float)colour[2];
	v[1].a = 255.0f;
	v[1].tmuvtx[0].sow = tw * oow;
	v[1].tmuvtx[0].tow = v[0].tmuvtx[0].sow;

	v[2].x = v[1].x;
	v[2].y = (float)(y + h);
	v[2].oow = oow;
	v[2].r = (float)colour[0];
	v[2].g = (float)colour[1];
	v[2].b = (float)colour[2];
	v[2].a = 255.0f;
	v[2].tmuvtx[0].sow = v[1].tmuvtx[0].sow;
	v[2].tmuvtx[0].tow = th * oow;

	v[3].x = v[0].x;
	v[3].y = v[2].y;
	v[3].oow = oow;
	v[3].r = (float)colour[0];
	v[3].g = (float)colour[1];
	v[3].b = (float)colour[2];
	v[3].a = 255.0f;
	v[3].tmuvtx[0].sow = v[0].tmuvtx[0].sow;
	v[3].tmuvtx[0].tow = v[2].tmuvtx[0].tow;

	if (tset != last_tset)
	{
		last_tset = tset;
		grTexSource(GR_TMU0, TextureSlots[tset].texture_ram_address, GR_MIPMAPLEVELMASK_BOTH, TextureSlots[tset].TInfo);
	}

	if (last_state != TEXT3)
	{
		grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE);
		last_state = TEXT3;
	}

	if (last_trans_on)
	{
		grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
		last_trans_on = 0;
	}

	grDrawPolygonVertexList(4, v);
}
