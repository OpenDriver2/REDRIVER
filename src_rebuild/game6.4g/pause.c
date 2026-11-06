/*
 * pause.c - the in game pause menu: pause, end of mission / game over menus, sound and graphics
 * options, saving a replay, the full screen map
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "pause.c", 0x510a60 - 0x51356f).
 *
 * A menu is a PAUSE_TEXT list ending with an entry with flags 16; l r u d are the entries to go to
 * with the arrow keys, func runs on return. For a slider (flags 8) l and r are entries after the
 * end whose func moves it left and right.
 * Data (0x576a48 - 0x5779c2): the menus, the option text tables, the strings; bss
 * (0xbd96e0 - 0xbd9a40): the replay names, the menu state.
 */

#include <stdio.h>
#include <string.h>
#include "dr_keys.h"

#include "dr_types.h"
#include "pause.h"
#include "dr_globals.h"
#include "D3DGlide/glide_ptrs.h"
#include "asmmath.h"
#include "debug.h"
#include "Director.h"
#include "dr_scores.h"
#include "gsound.h"
#include "input.h"
#include "map.h"
#include "MScript.h"
#include "overlay.h"
#include "pres.h"
#include "replays.h"
#include "shell.h"
#include "sound.h"
#include "eax/cdaudio.h"
#include "frontend/config.h"
#include "frontend/frontend.h"
#include "frontend/gvars.h"
#include "frontend/selrace.h"
#include "Engine/3dfx/gfxinit.h"
#include "Engine/3dfx/tset.h"
#include "debris.h"
#include "denting.h"
#include "shadow.h"
#include "glaunch.h"
#include "mirror.h"

// a menu entry; text is a gLanguageStrings index (flags 1) or a string (flags 2)
typedef struct PAUSE_TEXT
{
	short x;
	short y;
	u_char justification;
	u_char flags;				// 1 language string, 2 string, 4 selectable, 8 slider, 16 end
	u_char pad1;
	u_char pad2;
	int text;
	void (*func)(void);
	u_char l;
	u_char r;
	u_char u;
	u_char d;
} PAUSE_TEXT;

void OpenPauseMenu(void);
void ClosePauseMenu(void);
int HandlePauseMenu(PAUSE_TEXT *menu, int selection);
void DisplayPauseMenu(PAUSE_TEXT *menu);
void sub_51192C(void);
void sub_51195D(void);
void sub_511A4D(void);
void sub_511A72(void);
void sub_511A98(void);
void sub_511AD6(void);
void sub_511B17(void);
void sub_511B50(void);
void sub_511B55(void);
void sub_511B5A(void);
void sub_511B89(void);
void sub_511BB8(void);
void sub_511BE7(void);
void sub_511C16(void);
void sub_511C56(void);
void sub_511D9A(void);
void sub_511FCC(void);
void sub_512028(void);
void sub_512049(void);
void sub_51206A(void);
void sub_51208B(void);
void sub_5120E7(void);
void sub_512108(void);
void sub_512129(void);
void sub_51214A(void);
void sub_51216B(void);
void sub_51218C(void);
void sub_5121AD(void);
void sub_5121CE(void);
void sub_51222A(void);
void sub_5122A8(void);
void sub_5122AD(void);
void sub_5122B2(void);
void sub_5122EB(void);
void sub_512324(void);
void sub_51235D(void);
void sub_512396(void);
void replaysavename_func(void);
void filmdirector_func(void);
void quickreplay_func(void);
void continue_func(void);
void mcontinue_func(void);
void restart_func(void);
void exit_func(void);
void showmap_func(void);
void savereplay_func(void);
void viewagain_func(void);
void tryit_func(void);
void next_func(void);
void yes_func(void);
void no_func(void);
void sound_func(void);
void graphic_func(void);
void enterscore_func(void);
void sub_5128D7(void);
void sub_512933(void);
void StartOfInputFrame(void);
void EndOfInputFrame(void);
void sub_5129FC(float x, float y, TEX_INFO *texture, u_char r, u_char g, u_char b, char flip, char trans);
void sub_512FCC(float x, float y, float fill_x, float fill_y, TEX_INFO *texture, u_char r, u_char g, u_char b, char trans);
void HandleErrorMessage(void);
void error_func(void);

// (bss below; the menus point at them)
static char fps_text[12];
static char replay_names[8][17];

//------------------------------------------------------------------------------------------
// initialised data

PAUSE_TEXT pause1_menu[10] =
{
	{ 320, 120, 1, 1, 0, 0, 150, NULL, 0, 0, 0, 0 },
	{ 320, 170, 1, 5, 0, 0, 161, continue_func, 1, 1, 8, 2 },
	{ 320, 200, 1, 5, 0, 0, 162, restart_func, 2, 2, 1, 3 },
	{ 320, 230, 1, 5, 0, 0, 168, sound_func, 3, 3, 2, 4 },
	{ 320, 260, 1, 5, 0, 0, 169, graphic_func, 4, 4, 3, 5 },
	{ 320, 290, 1, 5, 0, 0, 166, filmdirector_func, 5, 5, 4, 6 },
	{ 320, 320, 1, 5, 0, 0, 165, quickreplay_func, 6, 6, 5, 7 },
	{ 320, 350, 1, 5, 0, 0, 167, showmap_func, 7, 7, 6, 8 },
	{ 320, 380, 1, 5, 0, 0, 163, exit_func, 8, 8, 7, 1 },
	{ -1, -1, 0, 16, 0, 0, -1, NULL, 0, 0, 0, 0 },
};

PAUSE_TEXT pause2_menu[7] =
{
	{ 320, 120, 1, 1, 0, 0, 150, NULL, 0, 0, 0, 0 },
	{ 320, 170, 1, 5, 0, 0, 161, continue_func, 1, 1, 5, 2 },
	{ 320, 200, 1, 5, 0, 0, 162, restart_func, 2, 2, 1, 3 },
	{ 320, 230, 1, 5, 0, 0, 168, sound_func, 3, 3, 2, 4 },
	{ 320, 260, 1, 5, 0, 0, 169, graphic_func, 4, 4, 3, 5 },
	{ 320, 290, 1, 5, 0, 0, 163, exit_func, 5, 5, 4, 1 },
	{ -1, -1, 0, 16, 0, 0, -1, NULL, 0, 0, 0, 0 },
};

PAUSE_TEXT pause3_menu[7] =
{
	{ 320, 120, 1, 1, 0, 0, 150, NULL, 0, 0, 0, 0 },
	{ 320, 170, 1, 5, 0, 0, 161, continue_func, 1, 1, 5, 2 },
	{ 320, 200, 1, 5, 0, 0, 174, tryit_func, 2, 2, 1, 3 },
	{ 320, 230, 1, 5, 0, 0, 168, sound_func, 3, 3, 2, 4 },
	{ 320, 260, 1, 5, 0, 0, 169, graphic_func, 4, 4, 3, 5 },
	{ 320, 290, 1, 5, 0, 0, 163, exit_func, 5, 5, 4, 1 },
	{ -1, -1, 0, 16, 0, 0, -1, NULL, 0, 0, 0, 0 },
};

PAUSE_TEXT pause4_menu[9] =
{
	{ 320, 120, 1, 1, 0, 0, 150, NULL, 0, 0, 0, 0 },
	{ 320, 170, 1, 5, 0, 0, 161, continue_func, 1, 1, 7, 2 },
	{ 320, 200, 1, 5, 0, 0, 162, restart_func, 2, 2, 1, 3 },
	{ 320, 230, 1, 5, 0, 0, 168, sound_func, 3, 3, 2, 4 },
	{ 320, 260, 1, 5, 0, 0, 169, graphic_func, 4, 4, 3, 5 },
	{ 320, 290, 1, 5, 0, 0, 166, filmdirector_func, 5, 5, 4, 6 },
	{ 320, 320, 1, 5, 0, 0, 165, quickreplay_func, 6, 6, 5, 7 },
	{ 320, 350, 1, 5, 0, 0, 163, exit_func, 7, 7, 6, 1 },
	{ -1, -1, 0, 16, 0, 0, -1, NULL, 0, 0, 0, 0 },
};

PAUSE_TEXT endm1_menu[7] =
{
	{ 320, 120, 1, 1, 0, 0, 151, NULL, 0, 0, 0, 0 },
	{ 320, 170, 1, 5, 0, 0, 166, filmdirector_func, 1, 1, 5, 2 },
	{ 320, 200, 1, 5, 0, 0, 165, quickreplay_func, 2, 2, 1, 3 },
	{ 320, 230, 1, 5, 0, 0, 164, savereplay_func, 3, 3, 2, 4 },
	{ 320, 260, 1, 5, 0, 0, 161, mcontinue_func, 4, 4, 3, 5 },
	{ 320, 290, 1, 5, 0, 0, 163, exit_func, 5, 5, 4, 1 },
	{ -1, -1, 0, 16, 0, 0, -1, NULL, 0, 0, 0, 0 },
};

PAUSE_TEXT endm2_menu[7] =
{
	{ 320, 120, 1, 1, 0, 0, 152, NULL, 0, 0, 0, 0 },
	{ 320, 170, 1, 5, 0, 0, 166, filmdirector_func, 1, 1, 5, 2 },
	{ 320, 200, 1, 5, 0, 0, 165, quickreplay_func, 2, 2, 1, 3 },
	{ 320, 230, 1, 5, 0, 0, 164, savereplay_func, 3, 3, 2, 4 },
	{ 320, 260, 1, 5, 0, 0, 162, restart_func, 4, 4, 3, 5 },
	{ 320, 290, 1, 5, 0, 0, 163, exit_func, 5, 5, 4, 1 },
	{ -1, -1, 0, 16, 0, 0, -1, NULL, 0, 0, 0, 0 },
};

PAUSE_TEXT endm3_menu[7] =
{
	{ 320, 120, 1, 1, 0, 0, 152, NULL, 0, 0, 0, 0 },
	{ 320, 170, 1, 5, 0, 0, 166, filmdirector_func, 1, 1, 5, 2 },
	{ 320, 200, 1, 5, 0, 0, 165, quickreplay_func, 2, 2, 1, 3 },
	{ 320, 230, 1, 5, 0, 0, 164, savereplay_func, 3, 3, 2, 4 },
	{ 320, 260, 1, 5, 0, 0, 161, mcontinue_func, 4, 4, 3, 5 },
	{ 320, 290, 1, 5, 0, 0, 163, exit_func, 5, 5, 4, 1 },
	{ -1, -1, 0, 16, 0, 0, -1, NULL, 0, 0, 0, 0 },
};

PAUSE_TEXT gameover1_menu[7] =
{
	{ 320, 120, 1, 1, 0, 0, 153, NULL, 0, 0, 0, 0 },
	{ 320, 170, 1, 5, 0, 0, 166, filmdirector_func, 1, 1, 5, 2 },
	{ 320, 200, 1, 5, 0, 0, 165, quickreplay_func, 2, 2, 1, 3 },
	{ 320, 230, 1, 5, 0, 0, 164, savereplay_func, 3, 3, 2, 4 },
	{ 320, 260, 1, 5, 0, 0, 162, restart_func, 4, 4, 3, 5 },
	{ 320, 290, 1, 5, 0, 0, 163, exit_func, 5, 5, 4, 1 },
	{ -1, -1, 0, 16, 0, 0, -1, NULL, 0, 0, 0, 0 },
};

PAUSE_TEXT gameover1b_menu[8] =
{
	{ 320, 120, 1, 1, 0, 0, 153, NULL, 0, 0, 0, 0 },
	{ 320, 170, 1, 5, 0, 0, 166, filmdirector_func, 1, 1, 6, 2 },
	{ 320, 200, 1, 5, 0, 0, 165, quickreplay_func, 2, 2, 1, 3 },
	{ 320, 230, 1, 5, 0, 0, 164, savereplay_func, 3, 3, 2, 4 },
	{ 320, 260, 1, 5, 0, 0, 162, restart_func, 4, 4, 3, 5 },
	{ 320, 290, 1, 5, 0, 0, 246, enterscore_func, 5, 5, 4, 6 },
	{ 320, 320, 1, 5, 0, 0, 163, exit_func, 6, 6, 5, 1 },
	{ -1, -1, 0, 16, 0, 0, -1, NULL, 0, 0, 0, 0 },
};

PAUSE_TEXT gameover2_menu[4] =
{
	{ 320, 120, 1, 1, 0, 0, 153, NULL, 0, 0, 0, 0 },
	{ 320, 170, 1, 5, 0, 0, 162, restart_func, 1, 1, 2, 2 },
	{ 320, 200, 1, 5, 0, 0, 163, exit_func, 2, 2, 1, 1 },
	{ -1, -1, 0, 16, 0, 0, -1, NULL, 0, 0, 0, 0 },
};

PAUSE_TEXT demoover1_menu[5] =
{
	{ 320, 120, 1, 1, 0, 0, 159, NULL, 0, 0, 0, 0 },
	{ 320, 170, 1, 5, 0, 0, 173, viewagain_func, 1, 1, 3, 2 },
	{ 320, 200, 1, 5, 0, 0, 174, tryit_func, 2, 2, 1, 3 },
	{ 320, 230, 1, 5, 0, 0, 163, exit_func, 3, 3, 2, 1 },
	{ -1, -1, 0, 16, 0, 0, -1, NULL, 0, 0, 0, 0 },
};

PAUSE_TEXT gameover4_menu[5] =
{
	{ 320, 120, 1, 1, 0, 0, 160, NULL, 0, 0, 0, 0 },
	{ 320, 170, 1, 5, 0, 0, 173, viewagain_func, 1, 1, 3, 2 },
	{ 320, 200, 1, 5, 0, 0, 174, tryit_func, 2, 2, 1, 3 },
	{ 320, 230, 1, 5, 0, 0, 163, exit_func, 3, 3, 2, 1 },
	{ -1, -1, 0, 16, 0, 0, -1, NULL, 0, 0, 0, 0 },
};

PAUSE_TEXT demoover2_menu[6] =
{
	{ 320, 120, 1, 1, 0, 0, 159, NULL, 0, 0, 0, 0 },
	{ 320, 170, 1, 5, 0, 0, 173, viewagain_func, 1, 1, 4, 2 },
	{ 320, 200, 1, 5, 0, 0, 174, tryit_func, 2, 2, 1, 3 },
	{ 320, 230, 1, 5, 0, 0, 26, next_func, 3, 3, 2, 4 },
	{ 320, 260, 1, 5, 0, 0, 163, exit_func, 4, 4, 3, 1 },
	{ -1, -1, 0, 16, 0, 0, -1, NULL, 0, 0, 0, 0 },
};

PAUSE_TEXT gameover3_menu[6] =
{
	{ 320, 120, 1, 1, 0, 0, 160, NULL, 0, 0, 0, 0 },
	{ 320, 170, 1, 5, 0, 0, 173, viewagain_func, 1, 1, 4, 2 },
	{ 320, 200, 1, 5, 0, 0, 174, tryit_func, 2, 2, 1, 3 },
	{ 320, 230, 1, 5, 0, 0, 26, next_func, 3, 3, 2, 4 },
	{ 320, 260, 1, 5, 0, 0, 163, exit_func, 4, 4, 3, 1 },
	{ -1, -1, 0, 16, 0, 0, -1, NULL, 0, 0, 0, 0 },
};

// NOTE: name guessed (IDA stru_5771D8); the menu while watching a replay
static PAUSE_TEXT replay_menu[5] =
{
	{ 320, 120, 1, 1, 0, 0, 295, NULL, 0, 0, 0, 0 },
	{ 320, 170, 1, 5, 0, 0, 166, filmdirector_func, 1, 1, 3, 2 },
	{ 320, 200, 1, 5, 0, 0, 164, savereplay_func, 2, 2, 1, 3 },
	{ 320, 230, 1, 5, 0, 0, 163, exit_func, 3, 3, 2, 1 },
	{ -1, -1, 0, 16, 0, 0, -1, NULL, 0, 0, 0, 0 },
};

// the values ([2] [4] ...) are set by sub_51195D; 13 and 14 are the volume sliders
PAUSE_TEXT soundoptions_menu[19] =
{
	{ 320, 150, 1, 1, 0, 0, 156, NULL, 0, 0, 0, 0 },
	{ 122, 200, 0, 5, 0, 0, 80, sub_51192C, 1, 1, 14, 3 },
	{ 528, 200, 2, 1, 0, 0, -1, NULL, 0, 0, 0, 0 },
	{ 122, 225, 0, 5, 0, 0, 81, sub_511A4D, 3, 3, 1, 5 },
	{ 528, 225, 2, 1, 0, 0, -1, NULL, 0, 0, 0, 0 },
	{ 122, 250, 0, 5, 0, 0, 210, sub_511A72, 5, 5, 3, 7 },
	{ 528, 250, 2, 1, 0, 0, -1, NULL, 0, 0, 0, 0 },
	{ 122, 275, 0, 5, 0, 0, 233, sub_511A98, 7, 7, 5, 9 },
	{ 528, 275, 2, 2, 0, 0, -1, NULL, 0, 0, 0, 0 },
	{ 122, 300, 0, 5, 0, 0, 234, sub_511AD6, 9, 9, 7, 11 },
	{ 528, 300, 2, 2, 0, 0, -1, NULL, 0, 0, 0, 0 },
	{ 122, 325, 0, 5, 0, 0, 235, sub_511B17, 11, 11, 9, 13 },
	{ 528, 325, 2, 1, 0, 0, -1, NULL, 0, 0, 0, 0 },
	{ 122, 350, 0, 13, 0, 0, 82, sub_511B50, 15, 16, 11, 14 },
	{ 122, 375, 0, 13, 0, 0, 83, sub_511B55, 17, 18, 13, 1 },
	{ -1, -1, 0, 16, 0, 0, -1, sub_511B89, 0, 0, 0, 0 },
	{ -1, -1, 0, 16, 0, 0, -1, sub_511B5A, 0, 0, 0, 0 },
	{ -1, -1, 0, 16, 0, 0, -1, sub_511BE7, 0, 0, 0, 0 },
	{ -1, -1, 0, 16, 0, 0, -1, sub_511BB8, 0, 0, 0, 0 },
};

// the values ([2] [4] ...) are set by sub_511D9A; 31 and 32 are the draw distance sliders
PAUSE_TEXT graphics_menu[37] =
{
	{ 320, 40, 1, 1, 0, 0, 157, NULL, 0, 0, 0, 0 },
	{ 92, 70, 0, 5, 0, 0, 228, sub_511C16, 1, 1, 32, 3 },
	{ 548, 70, 2, 1, 0, 0, -1, NULL, 0, 0, 0, 0 },
	{ 92, 98, 0, 5, 0, 0, 43, sub_511FCC, 3, 3, 1, 5 },
	{ 548, 98, 2, 1, 0, 0, -1, NULL, 0, 0, 0, 0 },
	{ 92, 121, 0, 5, 0, 0, 44, sub_512028, 5, 5, 3, 7 },
	{ 548, 121, 2, 1, 0, 0, -1, NULL, 0, 0, 0, 0 },
	{ 92, 144, 0, 5, 0, 0, 45, sub_512049, 7, 7, 5, 9 },
	{ 548, 144, 2, 1, 0, 0, -1, NULL, 0, 0, 0, 0 },
	{ 92, 167, 0, 5, 0, 0, 49, sub_512129, 9, 9, 7, 11 },
	{ 548, 167, 2, 1, 0, 0, -1, NULL, 0, 0, 0, 0 },
	{ 92, 190, 0, 5, 0, 0, 229, sub_51214A, 11, 11, 9, 13 },
	{ 548, 190, 2, 1, 0, 0, -1, NULL, 0, 0, 0, 0 },
	{ 92, 213, 0, 5, 0, 0, 48, sub_512108, 13, 13, 11, 15 },
	{ 548, 213, 2, 1, 0, 0, -1, NULL, 0, 0, 0, 0 },
	{ 92, 236, 0, 5, 0, 0, 57, sub_5120E7, 15, 15, 13, 17 },
	{ 548, 236, 2, 1, 0, 0, -1, NULL, 0, 0, 0, 0 },
	{ 92, 259, 0, 5, 0, 0, 47, sub_51208B, 17, 17, 15, 19 },
	{ 548, 259, 2, 1, 0, 0, -1, NULL, 0, 0, 0, 0 },
	{ 92, 282, 0, 5, 0, 0, 52, sub_5121CE, 19, 19, 17, 21 },
	{ 548, 282, 2, 1, 0, 0, -1, NULL, 0, 0, 0, 0 },
	{ 92, 305, 0, 5, 0, 0, 51, sub_5121AD, 21, 21, 19, 23 },
	{ 548, 305, 2, 1, 0, 0, -1, NULL, 0, 0, 0, 0 },
	{ 92, 328, 0, 5, 0, 0, 55, sub_51218C, 23, 23, 21, 25 },
	{ 548, 328, 2, 1, 0, 0, -1, NULL, 0, 0, 0, 0 },
	{ 92, 351, 0, 5, 0, 0, 50, sub_51216B, 25, 25, 23, 27 },
	{ 548, 351, 2, 1, 0, 0, -1, NULL, 0, 0, 0, 0 },
	{ 92, 374, 0, 5, 0, 0, 87, sub_51222A, 27, 27, 25, 29 },
	{ 548, 374, 2, 2, 0, 0, (int)fps_text, NULL, 0, 0, 0, 0 },
	{ 92, 397, 0, 5, 0, 0, 46, sub_51206A, 29, 29, 27, 31 },
	{ 548, 397, 2, 1, 0, 0, -1, NULL, 0, 0, 0, 0 },
	{ 92, 420, 0, 13, 0, 0, 56, sub_5122AD, 33, 34, 29, 32 },
	{ 92, 443, 0, 13, 0, 0, 54, sub_5122A8, 35, 36, 31, 1 },
	{ -1, -1, 0, 16, 0, 0, -1, sub_51235D, 0, 0, 0, 0 },
	{ -1, -1, 0, 16, 0, 0, -1, sub_512324, 0, 0, 0, 0 },
	{ -1, -1, 0, 16, 0, 0, -1, sub_5122EB, 0, 0, 0, 0 },
	{ -1, -1, 0, 16, 0, 0, -1, sub_5122B2, 0, 0, 0, 0 },
};

// NOTE: name guessed (IDA stru_5776A8); the eight replay slots: number and name
static PAUSE_TEXT savereplay_menu[18] =
{
	{ 320, 110, 1, 1, 0, 0, 158, NULL, 0, 0, 0, 0 },
	{ 190, 160, 0, 6, 0, 0, (int)"1.", replaysavename_func, 1, 1, 15, 3 },
	{ 230, 160, 0, 2, 0, 0, (int)replay_names[0], NULL, 0, 0, 0, 0 },
	{ 190, 185, 0, 6, 0, 0, (int)"2.", replaysavename_func, 3, 3, 1, 5 },
	{ 230, 185, 0, 2, 0, 0, (int)replay_names[1], NULL, 0, 0, 0, 0 },
	{ 190, 210, 0, 6, 0, 0, (int)"3.", replaysavename_func, 5, 5, 3, 7 },
	{ 230, 210, 0, 2, 0, 0, (int)replay_names[2], NULL, 0, 0, 0, 0 },
	{ 190, 235, 0, 6, 0, 0, (int)"4.", replaysavename_func, 7, 7, 5, 9 },
	{ 230, 235, 0, 2, 0, 0, (int)replay_names[3], NULL, 0, 0, 0, 0 },
	{ 190, 260, 0, 6, 0, 0, (int)"5.", replaysavename_func, 9, 9, 7, 11 },
	{ 230, 260, 0, 2, 0, 0, (int)replay_names[4], NULL, 0, 0, 0, 0 },
	{ 190, 285, 0, 6, 0, 0, (int)"6.", replaysavename_func, 11, 11, 9, 13 },
	{ 230, 285, 0, 2, 0, 0, (int)replay_names[5], NULL, 0, 0, 0, 0 },
	{ 190, 310, 0, 6, 0, 0, (int)"7.", replaysavename_func, 13, 13, 11, 15 },
	{ 230, 310, 0, 2, 0, 0, (int)replay_names[6], NULL, 0, 0, 0, 0 },
	{ 190, 335, 0, 6, 0, 0, (int)"8.", replaysavename_func, 15, 15, 13, 1 },
	{ 230, 335, 0, 2, 0, 0, (int)replay_names[7], NULL, 0, 0, 0, 0 },
	{ -1, -1, 0, 16, 0, 0, 0, NULL, 0, 0, 0, 0 },
};

PAUSE_TEXT areyousure_menu[4] =
{
	{ 320, 150, 1, 1, 0, 0, 170, NULL, 0, 0, 0, 0 },
	{ 310, 200, 2, 5, 0, 0, 171, yes_func, 2, 2, 1, 1 },
	{ 330, 200, 0, 5, 0, 0, 172, no_func, 1, 1, 2, 2 },
	{ -1, -1, 0, 16, 0, 0, 0, NULL, 0, 0, 0, 0 },
};

PAUSE_TEXT error_menu[2] =
{
	{ 320, 200, 1, 5, 0, 0, 0, error_func, 0, 0, 0, 0 },
	{ -1, -1, 0, 16, 0, 0, 0, NULL, 0, 0, 0, 0 },
};

int cursel = 1;
int lastsel = 1;

// gLanguageStrings indices of the option values
// NOTE: names guessed (IDA dword_577890 ... dword_5778E0)
static int onoff_text[2] = { 2, 1 };					// off, on
static int tyretracks_text[3] = { 2, 6, 7 };
static int lights_text[3] = { 8, 7, 232 };
static int shadows_text[3] = { 2, 5, 7 };
static int detail_text[5] = { 5, 10, 226, 232, 227 };
static char *mixerbits_text[2] = { "8bit", "16bit" };
static char *mixerfreq_text[2] = { "22050KHz", "44100KHz" };
static int speakermode_text[5] = { 236, 237, 238, 239, 240 };

int key_delay = 10;
static int cursor_timer = 10;		// NOTE: name guessed (IDA dword_5778F8); blinks the name cursor
int move_fast_delay = 10;
static int depth_mask = 1;			// NOTE: name guessed (IDA dword_577900)

//------------------------------------------------------------------------------------------
// uninitialised data

// NOTE: names of the unnamed ones guessed (IDA dword_BD96E0, byte_BD9744, byte_BD9960,
// flt_BD9A00 ... dword_BD9A18)
static int replay_saved[8];				// the slot has a replay (else its name is "empty")
SCORE_DATA backup_score;
static char fps_text[12];				// graphics_menu[28]
MISSION backup_mission;
static char replay_names[8][17];
PAUSE_TEXT *curmenu;
PAUSE_TEXT *lastmenu;
PAUSE_TEXT *errormenu;
int errorsel;
TEX_INFO *SliderSelTexture;
TEX_INFO *SliderTexture;
static float far_clip_slider;			// far_clip, 0 .. 1
static float mirror_clip_slider;		// mirror_far_clip, 0 .. 1
static int edit_slot;					// the replay slot being named
static int editing_name;
static int name_entered;
static int edit_pos;					// the cursor in the name
static int cursor_on;
int disable_escape;
int bGrayQuickReplay;
int bGrayShowMap;
static int bWantPauseGame;
static int bForcePause;
int bErrorMessageOn;
int GamePaused;
int gbFullscreenMap;
int move_delay;

// [0x510a60] [D]
// Asks for the pause menu to open (1) or close (0) at the next CheckPause; asking for the state
// already asked for forces it.
void PauseGame(int pause)
{
	if (pause == bWantPauseGame)
		bForcePause = 1;

	bWantPauseGame = pause;
}

// [0x510a83] [D]
void CheckPause(void)
{
	if (bWantPauseGame)
	{
		if (!GamePaused || bForcePause)
			OpenPauseMenu();
	}
	else if (GamePaused || bForcePause)
	{
		ClosePauseMenu();
	}

	bForcePause = 0;
}

// [0x510acb] [D]
void PauseKeyControl(void)
{
	if (!curmenu || Director.Mode)
		return;

	if (gbFullscreenMap)
	{
		if (CheckKey(DIK_ESCAPE) || CheckKey(DIK_RETURN))
			gbFullscreenMap = 0;
	}
	else if (editing_name)
	{
		sub_512396();
	}
	else
	{
		cursel = HandlePauseMenu(curmenu, cursel);

		if (cursel == -1)
		{
			// back
			if (bErrorMessageOn)
			{
				cursel = errorsel;
				curmenu = errormenu;
				bErrorMessageOn = 0;
			}
			else
			{
				cursel = lastsel;
				curmenu = lastmenu;

				if (!lastmenu)
				{
					if (gbGameDemo)
						SetDirectorMode(15, 0, 0);
					else
						PauseGame(0);
				}

				lastmenu = NULL;
			}
		}
	}
}

// [0x510bca] [D]
// Draws the pause menu (or the full screen map); the sliders of the sound and graphics options,
// the cursor of the replay name being typed.
void RenderInput(void)
{
	u_char g1, b1;
	u_char g2, b2;

	if (!curmenu || Director.Mode)
		return;

	StartOfInputFrame();

	if (gbFullscreenMap)
	{
		DrawFullscreenMap();
		SwapPalette(24);
	}
	else
	{
		SwapPalette(24);

		// the selected slider is red
		if (curmenu == soundoptions_menu)
		{
			g2 = 255;
			g1 = 255;
			b2 = 255;
			b1 = 255;

			if (cursel == 13)
			{
				g1 = 0;
				b1 = 0;
			}

			if (cursel == 14)
			{
				g2 = 0;
				b2 = 0;
			}

			if (SliderTexture)
			{
				sub_5129FC(401.0f, 351.0f, SliderTexture, 255, g1, b1, 0, 0);
				sub_5129FC(401.0f, 376.0f, SliderTexture, 255, g2, b2, 0, 0);
			}

			if (SliderSelTexture)
			{
				sub_512FCC(401.0f, 351.0f, gMasterVolume, 1.0f, SliderSelTexture, 255, g1, b1, 0);
				sub_512FCC(401.0f, 376.0f, gMusicVolume, 1.0f, SliderSelTexture, 255, g2, b2, 0);
			}
		}

		if (curmenu == graphics_menu)
		{
			g2 = 255;
			g1 = 255;
			b2 = 255;
			b1 = 255;

			if (cursel == 32)
			{
				g1 = 0;
				b1 = 0;
			}

			if (cursel == 31)
			{
				g2 = 0;
				b2 = 0;
			}

			if (SliderTexture)
			{
				sub_5129FC(421.0f, 421.0f, SliderTexture, 255, g2, b2, 0, 0);
				sub_5129FC(421.0f, 444.0f, SliderTexture, 255, g1, b1, 0, 0);
			}

			if (SliderSelTexture)
			{
				sub_512FCC(421.0f, 421.0f, mirror_clip_slider, 1.0f, SliderSelTexture, 255, g2, b2, 0);
				sub_512FCC(421.0f, 444.0f, far_clip_slider, 1.0f, SliderSelTexture, 255, g1, b1, 0);
			}
		}

		if (curmenu == savereplay_menu)
		{
			if (--cursor_timer < 0)
			{
				cursor_timer = 10;
				cursor_on ^= 1;
			}

			if (editing_name && cursor_on)
			{
				replay_names[edit_slot][edit_pos] = '_';
				replay_names[edit_slot][edit_pos + 1] = 0;

				DisplayPauseMenu(curmenu);

				replay_names[edit_slot][edit_pos] = 0;
			}
			else
			{
				DisplayPauseMenu(curmenu);
			}
		}
		else
		{
			DisplayPauseMenu(curmenu);
		}
	}

	EndOfInputFrame();
}

// [0x510f20] [D]
// Pauses the game and picks the menu: pause, end of mission (passed / failed), game over, demo
// over, replay.
void OpenPauseMenu(void)
{
	int scorepos;

	if (Director.Attract)
	{
		DestroyCameras();
		DestroyWaypoints();
		wanted_exit_flag = 1;
		quit = 0;
		return;
	}

	GamePaused = 1;
	gInPresentation = 1;
	bErrorMessageOn = 0;

	if (Director.Mode)
		return;

	bGrayShowMap = 0;
	bGrayQuickReplay = 0;

	// back from a replay of the game: the game's mission state
	if (!gbGameDemo && (exit_flag == 4 || exit_flag == 5))
	{
		mission = backup_mission;
		global_score = backup_score;

		if (exit_flag == 5)
			bGrayQuickReplay = 1;
	}

	if (GameLevel == 6)
		bGrayShowMap = 1;

	PauseSounds();

	if (gUseCDAudio)
		PauseCDTrack();

	key_delay = 10;
	SliderSelTexture = GetTextureDetails("SLIDERB+");
	SliderTexture = GetTextureDetails("SLIDERBR");
	lastmenu = NULL;
	cursel = 1;

	// failed, but the driving games end so
	if (mission.objective == 0xFF0)
	{
		if (global_score.type == GAME_TRAILBLAZER)
		{
			global_score.complete = 1;
			mission.objective = 0x8000;
		}
		else if (global_score.type == GAME_SURVIVAL || global_score.type == GAME_CARNAGE)
		{
			global_score.lap_info = *mission.timer;
			global_score.complete = 1;
			mission.objective = 0x8000;
		}
	}

	if (Director.DoingReplay)
	{
		curmenu = replay_menu;
		disable_escape = 1;
	}
	else if (mission.objective == 0xFF0)
	{
		switch (global_score.type)
		{
			case GAME_DEFAULT:
			case GAME_TRAILBLAZER:
			case GAME_CHECKPOINT:
			case GAME_SURVIVAL:
			case GAME_PURSUIT:
			case GAME_GETAWAY:
			case GAME_CARNAGE:
			case GAME_TAKEADRIVE:
			case GAME_TIMETRIAL:
				curmenu = gameover1_menu;
				break;
			case GAME_DIRTTRACK:
				curmenu = gameover2_menu;
				break;
			case GAME_MISSION:
				if (mission.canBeFailed)
					curmenu = endm3_menu;
				else
					curmenu = endm2_menu;
				break;
			case GAME_CARPARK:
				if (GameMode == giDemoScript)
				{
					if (gNextScript == -1)
						curmenu = demoover1_menu;
					else
						curmenu = demoover2_menu;
				}
				else if (gNextScript == -1)
				{
					curmenu = gameover4_menu;
				}
				else
				{
					curmenu = gameover3_menu;
				}
				break;
			default:
				curmenu = gameover1_menu;
				break;
		}

		disable_escape = 1;
	}
	else if (mission.objective & 0x8000)
	{
		switch (global_score.type)
		{
			case GAME_DEFAULT:
			case GAME_MISSION:
				curmenu = endm1_menu;

				if (!exit_flag)
					MNext = MLast;
				break;
			case GAME_TRAILBLAZER:
			case GAME_CHECKPOINT:
			case GAME_SURVIVAL:
			case GAME_PURSUIT:
			case GAME_GETAWAY:
			case GAME_CARNAGE:
			case GAME_TIMETRIAL:
				if (CheckForRecord(&scorepos, 1) || CheckForRecord(&scorepos, 0))
					curmenu = gameover1b_menu;
				else
					curmenu = gameover1_menu;
				break;
			case GAME_DIRTTRACK:
				curmenu = gameover1_menu;
				break;
			case GAME_CARPARK:
				if (GameMode == giDemoScript)
				{
					if (gNextScript == -1)
						curmenu = demoover1_menu;
					else
						curmenu = demoover2_menu;
				}
				else if (gNextScript == -1)
				{
					curmenu = gameover4_menu;
				}
				else
				{
					curmenu = gameover3_menu;
				}
				break;
			default:
				LogQuick("Pause: Invalid game type, defaulting...");
				curmenu = gameover1_menu;
				break;
		}

		disable_escape = 1;
	}
	else
	{
		if (!gDoInterview || gbGameDemo || GameMode)
		{
			switch (global_score.type)
			{
				case GAME_DEFAULT:
				case GAME_TRAILBLAZER:
				case GAME_CHECKPOINT:
				case GAME_SURVIVAL:
				case GAME_PURSUIT:
				case GAME_GETAWAY:
				case GAME_CARNAGE:
				case GAME_TAKEADRIVE:
				case GAME_MISSION:
					curmenu = pause1_menu;
					break;
				case GAME_DIRTTRACK:
				case GAME_TIMETRIAL:
					curmenu = pause2_menu;
					break;
				case GAME_CARPARK:
					curmenu = pause3_menu;
					break;
				default:
					LogQuick("Pause: Invalid game type, defaulting...");
					curmenu = pause1_menu;
					break;
			}
		}
		else
		{
			curmenu = pause4_menu;
		}

		disable_escape = 0;
	}
}

// [0x5113c6] [D]
void ClosePauseMenu(void)
{
	GamePaused = 0;
	gInPresentation = 0;

	if (Director.Mode)
		return;

	curmenu = NULL;

	if (quit)
	{
		UnPauseSounds();

		if (gUseCDAudio)
			UnPauseCDTrack();
	}
	else
	{
		StopSounds();

		if (gUseCDAudio)
			StopCDTrack();
	}

	if (gDoFrontend && !exit_flag)
		UpdateSavedOptions();
}

// [0x51143c] [D]
// Moves the selection with the arrow keys and runs the entry's func on return; returns the
// selection, -1 on escape.
int HandlePauseMenu(PAUSE_TEXT *menu, int selection)
{
	if (key_delay)
	{
		key_delay--;
		return selection;
	}

	if (CheckKey(DIK_ESCAPE) && (!disable_escape || lastmenu))
		return -1;

	if (menu[selection].flags & 8)
	{
		// a slider: held left / right repeats, faster after the first step
		if (!((KeyMap[DIK_RIGHTARROW] | KeyMap[DIK_LEFTARROW]) & 0x80))
		{
			move_delay = 0;
			move_fast_delay = 40;
		}

		if (move_delay)
		{
			move_delay--;
		}
		else if (KeyMap[DIK_LEFTARROW] & 0x80)
		{
			menu[menu[selection].l].func();
			move_delay = move_fast_delay;
			move_fast_delay = 3;
		}
		else if (KeyMap[DIK_RIGHTARROW] & 0x80)
		{
			menu[menu[selection].r].func();
			move_delay = move_fast_delay;
			move_fast_delay = 3;
		}
	}
	else if (CheckKey(DIK_LEFT))
	{
		selection = menu[selection].l;
	}
	else if (CheckKey(DIK_RIGHT))
	{
		selection = menu[selection].r;
	}

	// the greyed out entries are skipped
	if (CheckKey(DIK_UP))
	{
		selection = menu[selection].u;

		if (bGrayQuickReplay && menu[selection].text == 165)
			selection = menu[selection].u;

		if (bGrayShowMap && menu[selection].text == 167)
			selection = menu[selection].u;
	}
	else if (CheckKey(DIK_DOWN))
	{
		selection = menu[selection].d;

		if (bGrayQuickReplay && menu[selection].text == 165)
			selection = menu[selection].d;

		if (bGrayShowMap && menu[selection].text == 167)
			selection = menu[selection].d;
	}
	else if (CheckKey(DIK_RETURN))
	{
		cursel = selection;
		menu[selection].func();
		selection = cursel;
	}

	return selection;
}

// [0x5116fd] [D]
// Draws the menu: the title (the first entry) yellow, the selected entry red, the greyed out ones
// grey.
void DisplayPauseMenu(PAUSE_TEXT *menu)
{
	char *text;
	int i;

	for (i = 0; !(menu->flags & 16); menu++, i++)
	{
		if (menu->text == -1)
			continue;

		if (menu->flags & 1)
			text = gLanguageStrings[menu->text];
		else if (menu->flags & 2)
			text = (char *)menu->text;
		else
			text = NULL;

		if (!text)
			continue;

		SetTextColour(255, 255, 255);

		WriteScaledStringWithMode((float)menu->x, (float)menu->y, text, 0, menu->justification | 0x10);
		WriteScaledStringWithMode((float)menu->x + 2.0f, (float)menu->y + 2.0f, text, 0, menu->justification | 0x10);

		// (an entry that is not selectable is the value of the one before)
		if (i)
		{
			if (i == cursel || (i == cursel + 1 && !(menu->flags & 4)))
				SetTextColour(255, 0, 0);
			else if ((bGrayQuickReplay && menu->text == 165) || (bGrayShowMap && menu->text == 167))
				SetTextColour(127, 127, 127);
			else
				SetTextColour(255, 255, 255);
		}
		else
		{
			SetTextColour(255, 255, 128);
		}

		WriteScaledStringWithMode((float)menu->x, (float)menu->y, text, 0, menu->justification | 4);
	}
}

// [0x51192c] [D]
// Sound: on / off.
void sub_51192C(void)
{
	gAllowSounds ^= 1;

	if (gAllowSounds)
	{
		StartGameSounds();
		PauseSounds();
	}
	else
	{
		StopGameSounds();
	}

	sub_51195D();
}

// [0x51195d] [D]
// Applies the volumes and shows the sound options.
void sub_51195D(void)
{
	int bits16;
	int freq44;

	bits16 = gAudioMixerBits != 8;
	freq44 = gAudioMixerFreq != 22050;

	SetEffectsVolume(gMasterVolume);
	SetCDVolume(float_to_int(gMusicVolume * 65535.0f));

	soundoptions_menu[2].text = onoff_text[gAllowSounds];
	soundoptions_menu[4].text = onoff_text[gUseCDAudio];
	soundoptions_menu[6].text = onoff_text[gUseDoppler];
	soundoptions_menu[8].text = (int)mixerbits_text[bits16];
	soundoptions_menu[10].text = (int)mixerfreq_text[freq44];
	soundoptions_menu[12].text = speakermode_text[gAudioSpeakerMode];
}

// [0x511a4d] [D]
// CD music: on / off.
void sub_511A4D(void)
{
	gUseCDAudio ^= 1;

	if (!gUseCDAudio)
		StopCDTrack();

	sub_51195D();
}

// [0x511a72] [D]
// Doppler: on / off.
void sub_511A72(void)
{
	gUseDoppler ^= 1;
	SetSoundSystemDoppler(gUseDoppler);

	sub_51195D();
}

// [0x511a98] [D]
// Mixer: 8 / 16 bit.
void sub_511A98(void)
{
	if (gAudioMixerBits == 8)
		gAudioMixerBits = 16;
	else
		gAudioMixerBits = 8;

	SetSoundSystemFormat(gAudioMixerBits, gAudioMixerFreq);

	sub_51195D();
}

// [0x511ad6] [D]
// Mixer: 22050 / 44100 Hz.
void sub_511AD6(void)
{
	if (gAudioMixerFreq == 22050)
		gAudioMixerFreq = 44100;
	else
		gAudioMixerFreq = 22050;

	SetSoundSystemFormat(gAudioMixerBits, gAudioMixerFreq);

	sub_51195D();
}

// [0x511b17] [D]
// Speakers: the next of the five modes.
void sub_511B17(void)
{
	if (++gAudioSpeakerMode == 5)
		gAudioSpeakerMode = 0;

	SetSoundSystemMode(gAudioSpeakerMode);

	sub_51195D();
}

// [0x511b50] [D]
// (return on the effects volume slider)
void sub_511B50(void)
{
}

// [0x511b55] [D]
// (return on the music volume slider)
void sub_511B55(void)
{
}

// [0x511b5a] [D]
void sub_511B5A(void)
{
	if (gMasterVolume < 1.0f)
	{
		gMasterVolume = gMasterVolume + 0.015625f;
		sub_51195D();
	}
}

// [0x511b89] [D]
void sub_511B89(void)
{
	if (gMasterVolume > 0.0f)
	{
		gMasterVolume = gMasterVolume - 0.015625f;
		sub_51195D();
	}
}

// [0x511bb8] [D]
void sub_511BB8(void)
{
	if (gMusicVolume < 1.0f)
	{
		gMusicVolume = gMusicVolume + 0.015625f;
		sub_51195D();
	}
}

// [0x511be7] [D]
void sub_511BE7(void)
{
	if (gMusicVolume > 0.0f)
	{
		gMusicVolume = gMusicVolume - 0.015625f;
		sub_51195D();
	}
}

// [0x511c16] [D]
// Detail level: the next of the presets.
void sub_511C16(void)
{
	if (++gDetailLevel > 3)
		gDetailLevel = 0;

	SetGraphicsLevel(gDetailLevel, 1);

	sub_511C56();
	sub_511D9A();
}

// [0x511c56] [D]
// Clamps the draw distances (15000 .. 42000) and the frame skip (1 .. 10), shows the graphics
// options.
void sub_511C56(void)
{
	float clip;
	int skip;

	clip = far_clip <= 42000.0f ? far_clip : 42000.0f;
	far_clip = clip >= 15000.0f ? clip : 15000.0f;

	clip = mirror_far_clip <= 42000.0f ? mirror_far_clip : 42000.0f;
	mirror_far_clip = clip >= 15000.0f ? clip : 15000.0f;

	skip = max_frame_skip <= 10 ? max_frame_skip : 10;
	max_frame_skip = skip >= 1 ? skip : 1;

	sub_511D9A();
}

// [0x511d9a] [D]
// Shows the graphics options.
void sub_511D9A(void)
{
	int lights;
	int tyretracks;
	int shadows;

	if (gRealTimeLights)
	{
		if (gUltraLamps)
			lights = 2;
		else
			lights = 1;
	}
	else
	{
		lights = 0;
	}

	if (gDoTyreTracks)
	{
		if (gDoTexturedTyreTracks)
			tyretracks = 2;
		else
			tyretracks = 1;
	}
	else
	{
		tyretracks = 0;
	}

	if (gDrawShadows)
	{
		if (gDoDetailedShadows)
			shadows = 2;
		else
			shadows = 1;
	}
	else
	{
		shadows = 0;
	}

	if (gDoInterpolation)
	{
		if (gDoAutoInterpolation)
			sprintf(fps_text, "%s", gLanguageStrings[213]);
		else
			sprintf(fps_text, "%dfps", 50 / max_frame_skip);
	}
	else
	{
		sprintf(fps_text, "%s", gLanguageStrings[2]);
	}

	far_clip_slider = far_clip - 15000.0f;
	far_clip_slider = far_clip_slider / 27000.0f;
	mirror_clip_slider = mirror_far_clip - 15000.0f;
	mirror_clip_slider = mirror_clip_slider / 27000.0f;

	graphics_menu[2].text = detail_text[gDetailLevel];
	graphics_menu[4].text = shadows_text[shadows];
	graphics_menu[6].text = onoff_text[gDoSmoke];
	graphics_menu[8].text = onoff_text[gDoDebris];
	graphics_menu[10].text = onoff_text[gDoLensFlare];
	graphics_menu[12].text = onoff_text[gDoLightFlare];
	graphics_menu[14].text = onoff_text[gDoShinyCars];
	graphics_menu[16].text = onoff_text[gDoMultiColouredCars];
	graphics_menu[18].text = tyretracks_text[tyretracks];
	graphics_menu[20].text = lights_text[lights];
	graphics_menu[22].text = onoff_text[gDrawMap];
	graphics_menu[24].text = onoff_text[gFogAllowed];
	graphics_menu[26].text = onoff_text[DrawSubmergedObjects];
	graphics_menu[30].text = onoff_text[gDoRearViewMirror];
}

// [0x511fcc] [D]
// Shadows: off / on / detailed (a custom detail level from now on, as for all options below).
void sub_511FCC(void)
{
	if (gDrawShadows)
	{
		if (gDoDetailedShadows)
		{
			gDrawShadows = 0;
			gDoDetailedShadows = 0;
		}
		else
		{
			gDoDetailedShadows = 1;
		}
	}
	else
	{
		gDrawShadows = 1;
		gDoDetailedShadows = 0;
	}

	gDetailLevel = 4;
	sub_511D9A();
}

// [0x512028] [D]
void sub_512028(void)
{
	gDoSmoke ^= 1;
	gDetailLevel = 4;
	sub_511D9A();
}

// [0x512049] [D]
void sub_512049(void)
{
	gDoDebris ^= 1;
	gDetailLevel = 4;
	sub_511D9A();
}

// [0x51206a] [D]
void sub_51206A(void)
{
	gDoRearViewMirror ^= 1;
	gDetailLevel = 4;
	sub_511D9A();
}

// [0x51208b] [D]
// Tyre tracks: off / on / textured.
void sub_51208B(void)
{
	if (gDoTyreTracks)
	{
		if (gDoTexturedTyreTracks)
		{
			gDoTyreTracks = 0;
			gDoTexturedTyreTracks = 0;
		}
		else
		{
			gDoTexturedTyreTracks = 1;
		}
	}
	else
	{
		gDoTyreTracks = 1;
		gDoTexturedTyreTracks = 0;
	}

	gDetailLevel = 4;
	sub_511D9A();
}

// [0x5120e7] [D]
void sub_5120E7(void)
{
	gDoMultiColouredCars ^= 1;
	gDetailLevel = 4;
	sub_511D9A();
}

// [0x512108] [D]
void sub_512108(void)
{
	gDoShinyCars ^= 1;
	gDetailLevel = 4;
	sub_511D9A();
}

// [0x512129] [D]
void sub_512129(void)
{
	gDoLensFlare ^= 1;
	gDetailLevel = 4;
	sub_511D9A();
}

// [0x51214a] [D]
void sub_51214A(void)
{
	gDoLightFlare ^= 1;
	gDetailLevel = 4;
	sub_511D9A();
}

// [0x51216b] [D]
void sub_51216B(void)
{
	DrawSubmergedObjects ^= 1;
	gDetailLevel = 4;
	sub_511D9A();
}

// [0x51218c] [D]
void sub_51218C(void)
{
	gFogAllowed ^= 1;
	gDetailLevel = 4;
	sub_511D9A();
}

// [0x5121ad] [D]
void sub_5121AD(void)
{
	gDrawMap ^= 1;
	gDetailLevel = 4;
	sub_511D9A();
}

// [0x5121ce] [D]
// Lights: off / real time / ultra.
void sub_5121CE(void)
{
	if (gRealTimeLights)
	{
		if (gUltraLamps)
		{
			gRealTimeLights = 0;
			gUltraLamps = 0;
		}
		else
		{
			gUltraLamps = 1;
		}
	}
	else
	{
		gRealTimeLights = 1;
		gUltraLamps = 0;
	}

	gDetailLevel = 4;
	sub_511D9A();
}

// [0x51222a] [D]
// Frame rate: off / 50 / 25 / ... 10 fps / auto.
void sub_51222A(void)
{
	if (gDoInterpolation)
	{
		if (gDoAutoInterpolation == 1)
		{
			gDoAutoInterpolation = 0;
			gDoInterpolation = 0;
		}
		else if (max_frame_skip == 5)
		{
			gDoAutoInterpolation = 1;
		}
		else
		{
			max_frame_skip++;
		}
	}
	else
	{
		gDoInterpolation = 1;
		gDoAutoInterpolation = 0;
		max_frame_skip = 1;
	}

	gDetailLevel = 4;
	sub_511D9A();
}

// [0x5122a8] [D]
// (return on the draw distance slider)
void sub_5122A8(void)
{
}

// [0x5122ad] [D]
// (return on the mirror draw distance slider)
void sub_5122AD(void)
{
}

// [0x5122b2] [D]
void sub_5122B2(void)
{
	if (far_clip < 42000.0f)
	{
		far_clip = far_clip + 3000.0f;
		gDetailLevel = 4;
		sub_511D9A();
	}
}

// [0x5122eb] [D]
void sub_5122EB(void)
{
	if (far_clip > 15000.0f)
	{
		far_clip = far_clip - 3000.0f;
		gDetailLevel = 4;
		sub_511D9A();
	}
}

// [0x512324] [D]
void sub_512324(void)
{
	if (mirror_far_clip < 42000.0f)
	{
		mirror_far_clip = mirror_far_clip + 3000.0f;
		gDetailLevel = 4;
		sub_511D9A();
	}
}

// [0x51235d] [D]
void sub_51235D(void)
{
	if (mirror_far_clip > 15000.0f)
	{
		mirror_far_clip = mirror_far_clip - 3000.0f;
		gDetailLevel = 4;
		sub_511D9A();
	}
}

// [0x512396] [D]
// Typing the name of the replay to save (up to 15 characters).
void sub_512396(void)
{
	int key;

	key = GetASCIIInput();

	switch (key)
	{
		case -3:
			// ESCAPE
			editing_name = 0;
			name_entered = 0;
			sub_512933();
			break;
		case -2:
			// RETURN
			if (edit_pos)
			{
				name_entered = 1;
				sub_5128D7();
			}
			break;
		case -1:
			// BACK
			if (edit_pos > 0)
				replay_names[edit_slot][--edit_pos] = 0;
			break;
		case 0:
			break;
		default:
			if (edit_pos < 15)
			{
				replay_names[edit_slot][edit_pos++] = key;
				replay_names[edit_slot][edit_pos] = 0;
			}
			break;
	}
}

// [0x512486] [D]
// Starts typing the name of the selected slot (keeping the name of a saved replay).
void replaysavename_func(void)
{
	editing_name = 1;
	edit_slot = (cursel - 1) / 2;

	if (replay_saved[(cursel - 1) / 2])
	{
		edit_pos = strlen(replay_names[edit_slot]);
	}
	else
	{
		edit_pos = 0;
		replay_names[edit_slot][0] = 0;
	}
}

// [0x5124ef] [D]
void filmdirector_func(void)
{
	lastmenu = curmenu;
	lastsel = cursel;
	curmenu = areyousure_menu;
	cursel = 2;
	wanted_exit_flag = 5;
}

// [0x512528] [D]
void quickreplay_func(void)
{
	if (bGrayQuickReplay)
		return;

	lastmenu = curmenu;
	lastsel = cursel;
	curmenu = areyousure_menu;
	cursel = 2;
	wanted_exit_flag = 4;
}

// [0x51256a] [D]
void continue_func(void)
{
	if (gbGameDemo)
		SetDirectorMode(15, 0, 0);
	else
		PauseGame(0);
}

// [0x512592] [D]
void mcontinue_func(void)
{
	wanted_exit_flag = 2;
	quit = 0;
	PauseGame(0);
}

// [0x5125b5] [D]
void restart_func(void)
{
	lastmenu = curmenu;
	lastsel = cursel;
	curmenu = areyousure_menu;
	cursel = 2;
	wanted_exit_flag = 3;
}

// [0x5125ee] [D]
void exit_func(void)
{
	if (!disable_escape)
		global_score.complete = 0;

	lastmenu = curmenu;
	lastsel = cursel;
	curmenu = areyousure_menu;
	cursel = 2;
	wanted_exit_flag = 1;
	want_score = 0;
}

// [0x512644] [D]
void showmap_func(void)
{
	if (!bGrayShowMap && map_bmp_height)
		gbFullscreenMap ^= 1;
}

// [0x512668] [D]
void savereplay_func(void)
{
	lastmenu = curmenu;
	lastsel = cursel;
	curmenu = savereplay_menu;
	cursel = 1;

	sub_512933();
}

// [0x51269c] [D]
void viewagain_func(void)
{
	lastmenu = curmenu;
	lastsel = cursel;
	curmenu = areyousure_menu;
	cursel = 2;
	wanted_exit_flag = 6;

	if (!gbGameDemo || GameMode)
		WantedGameMode = giDemoScript;
	else
		WantedGameMode = 200;
}

// [0x5126ff] [D]
void tryit_func(void)
{
	lastmenu = curmenu;
	lastsel = cursel;
	curmenu = areyousure_menu;
	cursel = 2;
	wanted_exit_flag = 6;
	WantedGameMode = gTryItScript;
}

// [0x512744] [D]
void next_func(void)
{
	lastmenu = curmenu;
	lastsel = cursel;
	curmenu = areyousure_menu;
	cursel = 2;
	wanted_exit_flag = 6;
	WantedGameMode = gNextScript;
}

// [0x512789] [D]
// Confirms: for a replay the mission state is kept to come back to (OpenPauseMenu).
void yes_func(void)
{
	if (wanted_exit_flag == 4 || wanted_exit_flag == 5)
	{
		if (!exit_flag)
			Director.NumFrames = game_frame - 4;

		if (!(mission.objective & 0x8000))
			mission.objective = 0xFF0;

		backup_mission = mission;
		backup_score = global_score;
	}

	quit = 0;
	PauseGame(0);
}

// [0x51280a] [D]
void no_func(void)
{
	cursel = -1;
}

// [0x512819] [D]
void sound_func(void)
{
	lastmenu = curmenu;
	lastsel = cursel;
	curmenu = soundoptions_menu;
	cursel = 1;

	sub_51195D();
}

// [0x51284d] [D]
void graphic_func(void)
{
	lastmenu = curmenu;
	lastsel = cursel;
	curmenu = graphics_menu;
	cursel = 1;

	sub_511C56();
}

// [0x512881] [D]
void enterscore_func(void)
{
	if (!disable_escape)
		global_score.complete = 0;

	lastmenu = curmenu;
	lastsel = cursel;
	curmenu = areyousure_menu;
	cursel = 2;
	wanted_exit_flag = 1;
	want_score = 1;
}

// [0x5128d7] [D]
// Saves the replay under the name typed.
void sub_5128D7(void)
{
	editing_name = 0;
	name_entered = 0;
	edit_pos = 0;

	if (!exit_flag)
		DeInitPadPlayRec();

	if (!UpdateSavedReplay(edit_slot, replay_names[edit_slot]))
		HandleErrorMessage();

	sub_512933();
}

// [0x512933] [D]
// Reads the names of the saved replays.
void sub_512933(void)
{
	char name[16];
	int i;

	for (i = 0; i < 8; i++)
	{
		if (CheckReplay(i, name))
		{
			strcpy(replay_names[i], name);
			replay_saved[i] = 1;
		}
		else
		{
			strcpy(replay_names[i], gLanguageStrings[13]);
			replay_saved[i] = 0;
		}
	}
}

// [0x5129bd] [D]
void StartOfInputFrame(void)
{
	depth_mask = EnableDepthMask(0);
	grDepthBufferMode(GR_DEPTHBUFFER_DISABLE);
}

// [0x5129d9] [D]
void EndOfInputFrame(void)
{
	grDepthBufferMode(GR_DEPTHBUFFER_WBUFFER);
	EnableDepthMask(depth_mask);
	grDepthBufferFunction(GR_CMP_LEQUAL);
}

// [0x5129fc] [D]
// Draws a texture at x y of 640x480 (flip: mirrored); trans as last_trans_on.
void sub_5129FC(float x, float y, TEX_INFO *texture, u_char r, u_char g, u_char b, char flip, char trans)
{
	float xscale;
	float yscale;
	float half_width;
	float half_height;
	float u, v;
	float tw, th;
	float w, h;
	GrVertex verts[4];

	xscale = (float)display_width / 640.0f;
	yscale = (float)display_height / 480.0f;

	x = x * xscale;
	y = y * yscale;

	half_width = (float)(display_width >> 1);
	x = x - half_width;
	half_height = (float)(display_height >> 1);
	y = y - half_height;

	x = x * screen_scale;
	y = y * screen_scale;

	x = x + half_width;
	y = y + half_height;

	u = (float)texture->u;
	v = (float)texture->v;
	tw = (float)texture->w - 0.5f;
	th = (float)texture->h - 0.5f;

	w = tw * xscale * screen_scale;
	h = th * yscale * screen_scale;

	if (last_state != TEXT3)
	{
		grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE);
		last_state = TEXT3;
	}

	switch (trans)
	{
		case 0:
			if (last_trans_on)
			{
				grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
				last_trans_on = 0;
			}
			break;
		case 1:
			if (last_trans_on != 1)
			{
				grAlphaBlendFunction(GR_BLEND_ZERO, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO);
				last_trans_on = 1;
			}
			break;
		case 2:
			if (last_trans_on != 2)
			{
				grAlphaBlendFunction(GR_BLEND_ZERO, GR_BLEND_ONE_MINUS_SRC_COLOR, GR_BLEND_ONE, GR_BLEND_ZERO);
				last_trans_on = 2;
			}
			break;
		case 3:
			if (last_trans_on != 3)
			{
				grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ZERO);
				last_trans_on = 3;
			}
			break;
	}

	if (texture->tset != last_tset)
	{
		last_tset = texture->tset;
		grTexSource(GR_TMU0, TextureSlots[texture->tset].texture_ram_address, GR_MIPMAPLEVELMASK_BOTH, TextureSlots[texture->tset].TInfo);
	}

	verts[0].x = x;
	verts[0].y = y;
	verts[0].oow = 1.0f;
	verts[0].r = (float)r;
	verts[0].g = (float)g;
	verts[0].b = (float)b;
	verts[0].a = 255.0f;

	verts[1].x = x + w;
	verts[1].y = y;
	verts[1].oow = 1.0f;
	verts[1].r = verts[0].r;
	verts[1].g = verts[0].g;
	verts[1].b = verts[0].b;
	verts[1].a = 255.0f;

	verts[2].x = verts[1].x;
	verts[2].y = y + h;
	verts[2].oow = 1.0f;
	verts[2].r = verts[0].r;
	verts[2].g = verts[0].g;
	verts[2].b = verts[0].b;
	verts[2].a = 255.0f;

	verts[3].x = x;
	verts[3].y = verts[2].y;
	verts[3].oow = 1.0f;
	verts[3].r = verts[0].r;
	verts[3].g = verts[0].g;
	verts[3].b = verts[0].b;
	verts[3].a = 255.0f;

	if (flip)
	{
		verts[0].tmuvtx[0].sow = u + 0.5f + tw;
		verts[0].tmuvtx[0].tow = v + 0.5f;
		verts[1].tmuvtx[0].sow = u + 0.5f;
	}
	else
	{
		verts[0].tmuvtx[0].sow = u + 0.5f;
		verts[0].tmuvtx[0].tow = v + 0.5f;
		verts[1].tmuvtx[0].sow = u + 0.5f + tw;
	}

	verts[1].tmuvtx[0].tow = verts[0].tmuvtx[0].tow;
	verts[2].tmuvtx[0].sow = verts[1].tmuvtx[0].sow;
	verts[2].tmuvtx[0].tow = v + 0.5f + th;
	verts[3].tmuvtx[0].sow = verts[0].tmuvtx[0].sow;
	verts[3].tmuvtx[0].tow = verts[2].tmuvtx[0].tow;

	grDrawPolygonVertexList(4, verts);
}

// [0x512fcc] [D]
// Draws the part fill_x x fill_y (0 .. 1) of a texture at x y of 640x480 (a slider's fill).
void sub_512FCC(float x, float y, float fill_x, float fill_y, TEX_INFO *texture, u_char r, u_char g, u_char b, char trans)
{
	float xscale;
	float yscale;
	float half_width;
	float half_height;
	float u, v;
	float tw, th;
	float w, h;
	GrVertex verts[4];

	xscale = (float)display_width / 640.0f;
	yscale = (float)display_height / 480.0f;

	x = x * xscale;
	y = y * yscale;

	half_width = (float)(display_width >> 1);
	x = x - half_width;
	half_height = (float)(display_height >> 1);
	y = y - half_height;

	x = x * screen_scale;
	y = y * screen_scale;

	x = x + half_width;
	y = y + half_height;

	u = (float)texture->u;
	v = (float)texture->v;
	tw = (float)texture->w * fill_x - 0.5f;
	th = (float)texture->h * fill_y - 0.5f;

	w = tw * xscale * screen_scale;
	h = th * yscale * screen_scale;

	if (last_state != TEXT3)
	{
		grColorCombine(GR_COMBINE_FUNCTION_SCALE_OTHER, GR_COMBINE_FACTOR_LOCAL, GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_TEXTURE, FXFALSE);
		last_state = TEXT3;
	}

	switch (trans)
	{
		case 0:
			if (last_trans_on)
			{
				grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
				last_trans_on = 0;
			}
			break;
		case 1:
			if (last_trans_on != 1)
			{
				grAlphaBlendFunction(GR_BLEND_ZERO, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO);
				last_trans_on = 1;
			}
			break;
		case 2:
			if (last_trans_on != 2)
			{
				grAlphaBlendFunction(GR_BLEND_ZERO, GR_BLEND_ONE_MINUS_SRC_COLOR, GR_BLEND_ONE, GR_BLEND_ZERO);
				last_trans_on = 2;
			}
			break;
		case 3:
			if (last_trans_on != 3)
			{
				grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ZERO);
				last_trans_on = 3;
			}
			break;
	}

	if (texture->tset != last_tset)
	{
		last_tset = texture->tset;
		grTexSource(GR_TMU0, TextureSlots[texture->tset].texture_ram_address, GR_MIPMAPLEVELMASK_BOTH, TextureSlots[texture->tset].TInfo);
	}

	verts[0].x = x;
	verts[0].y = y;
	verts[0].oow = 1.0f;
	verts[0].r = (float)r;
	verts[0].g = (float)g;
	verts[0].b = (float)b;
	verts[0].a = 255.0f;
	verts[0].tmuvtx[0].sow = u + 0.5f;
	verts[0].tmuvtx[0].tow = v + 0.5f;

	verts[1].x = x + w;
	verts[1].y = y;
	verts[1].oow = 1.0f;
	verts[1].r = verts[0].r;
	verts[1].g = verts[0].g;
	verts[1].b = verts[0].b;
	verts[1].a = 255.0f;
	verts[1].tmuvtx[0].sow = verts[0].tmuvtx[0].sow + tw;
	verts[1].tmuvtx[0].tow = verts[0].tmuvtx[0].tow;

	verts[2].x = verts[1].x;
	verts[2].y = y + h;
	verts[2].oow = 1.0f;
	verts[2].r = verts[0].r;
	verts[2].g = verts[0].g;
	verts[2].b = verts[0].b;
	verts[2].a = 255.0f;
	verts[2].tmuvtx[0].sow = verts[1].tmuvtx[0].sow;
	verts[2].tmuvtx[0].tow = verts[0].tmuvtx[0].tow + th;

	verts[3].x = x;
	verts[3].y = verts[2].y;
	verts[3].oow = 1.0f;
	verts[3].r = verts[0].r;
	verts[3].g = verts[0].g;
	verts[3].b = verts[0].b;
	verts[3].a = 255.0f;
	verts[3].tmuvtx[0].sow = verts[0].tmuvtx[0].sow;
	verts[3].tmuvtx[0].tow = verts[2].tmuvtx[0].tow;

	grDrawPolygonVertexList(4, verts);
}

// [0x513527] [D]
// Shows the error message (gLanguageStrings[0]); escape or return goes back to the menu.
void HandleErrorMessage(void)
{
	bErrorMessageOn = 1;
	errorsel = cursel;
	errormenu = curmenu;
	curmenu = error_menu;
	cursel = 0;
}

// [0x513560] [D]
void error_func(void)
{
	cursel = -1;
}
