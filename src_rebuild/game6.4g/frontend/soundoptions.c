/*
 * frontend/soundoptions.c - audio options: sound card, compatibility, EAX, sounds, CD audio,
 * Doppler, mixer format, speakers, effects and music volume; also used in the motel room
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "frontend\soundoptions.c", 0x415110 - 0x415fe1)
 */

#include "dr_types.h"
#include "dr_globals.h"
#include "pres.h"
#include "shell.h"
#include "input.h"
#include "asmmath.h"
#include "map.h"
#include "sound.h"
#include "eax/cdaudio.h"
#include "flat/frontend.h"
#include "frontend/frontend.h"
#include "frontend/soundoptions.h"
#include "flat/fudge.h"

void SetupAudioOptions(void);
static void device_selfunc(int flag);
static void device_unselfunc(int flag);
static int device_choosefunc(void);
static void compatibility_selfunc(int flag);
static void compatibility_unselfunc(int flag);
static int compatibility_choosefunc(void);
static void eax_selfunc(int flag);
static void eax_unselfunc(int flag);
static int eax_choosefunc(void);
static void sounds_selfunc(int flag);
static void sounds_unselfunc(int flag);
static int sounds_choosefunc(void);
static void cdaudio_selfunc(int flag);
static void cdaudio_unselfunc(int flag);
static int cdaudio_choosefunc(void);
static void doppler_selfunc(int flag);
static void doppler_unselfunc(int flag);
static int doppler_choosefunc(void);
static void bits_selfunc(int flag);
static void bits_unselfunc(int flag);
static int bits_choosefunc(void);
static void freq_selfunc(int flag);
static void freq_unselfunc(int flag);
static int freq_choosefunc(void);
static void speakers_selfunc(int flag);
static void speakers_unselfunc(int flag);
static int speakers_choosefunc(void);
static void mastervolume_selfunc(int flag);
static void mastervolume_unselfunc(int flag);
static int mastervolume_choosefunc(void);
static void musicvolume_selfunc(int flag);
static void musicvolume_unselfunc(int flag);
static int musicvolume_choosefunc(void);
static void mastervolume_up(int flag);
static void mastervolume_down(int flag);
static void musicvolume_up(int flag);
static void musicvolume_down(int flag);
static void HighlightMotelItem(void);
static void UnhighlightMotelItem(void);
void DrawMotelAudioOptions(void);

//------------------------------------------------------------------------------------------
// uninitialised data (0x600c3c)

static int selection;
// NOTE: dword_600C40; the (first) text of the item highlighted in the motel room
static int motel_item;
// NOTE: dword_600C44, dword_600C48; the motel room slider textures
static TEX_INFO *sliderbar_tex;
static TEX_INFO *slider_tex;
// NOTE: dword_600C4C, a bss buffer: the text of the end entry of polyitems
static char poly_end_text[4];

//------------------------------------------------------------------------------------------
// initialised data (0x54fc98)

// the effects and music volume sliders; SetupAudioOptions moves the boxes
static FE_POLY polyitems[5] = {
	{ 467, 380, "SLIDER", 1, 0, 0, { -1, -1, -1, -1 }, 0 },
	{ 469, 380, "SLIDEBOX", 1, 0, 0, { -1, -1, -1, -1 }, 0 },
	{ 467, 405, "SLIDER", 1, 0, 0, { -1, -1, -1, -1 }, 0 },
	{ 469, 405, "SLIDEBOX", 1, 0, 0, { -1, -1, -1, -1 }, 0 },
	{ 0, 0, poly_end_text, 4, 0, 0, { 0, 0, 0, 0 }, 0 },
};

static FE_TEXT textitems[21] = {
	{ 45, 152, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 595, 152, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 45, 177, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 595, 177, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 45, 202, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 595, 202, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 45, 227, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 595, 227, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 45, 252, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 595, 252, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 45, 277, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 595, 277, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 45, 302, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 595, 302, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 45, 327, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 595, 327, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 45, 352, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 595, 352, 2, 2, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 45, 377, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ 45, 402, 2, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

// [11] - [14]: left / right on the volume sliders (HandleMenu calls their selfunc)
static FE_BUTTON menufunc[15] = {
	{ device_selfunc, device_unselfunc, device_choosefunc, 0, 0, 10, 1 },
	{ compatibility_selfunc, compatibility_unselfunc, compatibility_choosefunc, 1, 1, 0, 2 },
	{ eax_selfunc, eax_unselfunc, eax_choosefunc, 2, 2, 1, 3 },
	{ sounds_selfunc, sounds_unselfunc, sounds_choosefunc, 3, 3, 2, 4 },
	{ cdaudio_selfunc, cdaudio_unselfunc, cdaudio_choosefunc, 4, 4, 3, 5 },
	{ doppler_selfunc, doppler_unselfunc, doppler_choosefunc, 5, 5, 4, 6 },
	{ bits_selfunc, bits_unselfunc, bits_choosefunc, 6, 6, 5, 7 },
	{ freq_selfunc, freq_unselfunc, freq_choosefunc, 7, 7, 6, 8 },
	{ speakers_selfunc, speakers_unselfunc, speakers_choosefunc, 8, 8, 7, 9 },
	{ mastervolume_selfunc, mastervolume_unselfunc, mastervolume_choosefunc, 11, 12, 8, 10 },
	{ musicvolume_selfunc, musicvolume_unselfunc, musicvolume_choosefunc, 13, 14, 9, 0 },
	{ mastervolume_down, NULL, NULL, 0, 0, 0, 0 },
	{ mastervolume_up, NULL, NULL, 0, 0, 0, 0 },
	{ musicvolume_down, NULL, NULL, 0, 0, 0, 0 },
	{ musicvolume_up, NULL, NULL, 0, 0, 0, 0 },
};

// gLanguageStrings indices: off, on
static int onoff[2] = { 2, 1 };
// NOTE: dword_54FF48; the second text of the item highlighted in the motel room, -1: none
static int motel_item2 = -1;
static char *bits_names[2] = { "8bit", "16bit" };
static char *freq_names[2] = { "22050Hz", "44100Hz" };
// gLanguageStrings indices of gSoundCompatibility and gAudioSpeakerMode
static int compatibility_names[3] = { 301, 302, 303 };
static int speaker_names[5] = { 236, 237, 238, 239, 240 };

// [0x415110] [D]
// In the motel room the menu is drawn over the room and leaving it returns 0.
int SelectAudioOptions(void)
{
	int ret;

	SetupAudioOptions();

	selection = 0;
	motel_item = 6;
	motel_item2 = 7;

	ResetMenu(menufunc, 0);

	if (gDoMotelRoom == 0)
		OpenFrontendScreen(polyitems, textitems);

	for (;;)
	{
		HandleWinMessages();
		ReadInput();

		if (gDoMotelRoom)
		{
			selection = MotelMenuKeyControl(menufunc, selection);

			if (selection == -1)
			{
				ret = 0;
				break;
			}

			HighlightMotelItem();
			DrawMotel(textitems, DrawMotelAudioOptions);
		}
		else
		{
			ret = HandleMenu(menufunc, &selection);

			if (ret != 0)
				break;

			DrawFrontend();
		}
	}

	if (ret != -1)
		return ret;

	if (gDoMotelRoom == 0)
		CloseFrontendScreen();

	return 0;
}

// [0x4151ff] [D] [A]
// Sets the texts and slider boxes from the options and applies both volumes.
void SetupAudioOptions(void)
{
	int bits16;
	int freq44;

	bits16 = gAudioMixerBits != 8;
	freq44 = gAudioMixerFreq != 22050;

	if (gDoMotelRoom)
	{
		slider_tex = GetTextureDetails("SLIDER");
		sliderbar_tex = GetTextureDetails("SLIDERBA");
	}

	textitems[0].text = gLanguageStrings[272];
	textitems[2].text = gLanguageStrings[300];
	textitems[4].text = gLanguageStrings[293];
	textitems[6].text = gLanguageStrings[80];
	textitems[8].text = gLanguageStrings[81];
	textitems[10].text = gLanguageStrings[210];
	textitems[12].text = gLanguageStrings[233];
	textitems[14].text = gLanguageStrings[234];
	textitems[16].text = gLanguageStrings[235];
	textitems[18].text = gLanguageStrings[82];
	textitems[19].text = gLanguageStrings[83];

	// NOTE: [A] the original adds 0x34 bytes per card instead of sizeof(DSOUND_CARD_INFO) (0x38) from 4
	// bytes before the name (the end of the GUID), so that only the first card shows at all
	if (num_soundcards)
		textitems[1].text = SoundDevices[gSoundHardware].name;
	else
		textitems[1].text = gLanguageStrings[12];

	textitems[3].text = gLanguageStrings[compatibility_names[gSoundCompatibility]];
	textitems[5].text = gLanguageStrings[onoff[gUseEAX]];
	textitems[7].text = gLanguageStrings[onoff[gAllowSounds]];
	textitems[9].text = gLanguageStrings[onoff[gUseCDAudio]];
	textitems[11].text = gLanguageStrings[onoff[gUseDoppler]];
	textitems[13].text = bits_names[bits16];
	textitems[15].text = freq_names[freq44];
	textitems[17].text = gLanguageStrings[speaker_names[gAudioSpeakerMode]];

	polyitems[1].x = float_to_int(gMasterVolume * 116.0f) + 469;
	polyitems[3].x = float_to_int(gMusicVolume * 116.0f) + 469;

	SetEffectsVolume(gMasterVolume);
	SetCDVolume(float_to_int(gMusicVolume * 65535.0f));
}

// [0x41547d] [D]
// flag 0 / 1: from HandleMenu; 2: from the motel room (the item is highlighted by colour)
static void device_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[0].font = 3;
		textitems[1].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 0;
		motel_item2 = 1;
	}
}

// [0x4154c3] [D]
static void device_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[0].font = 2;
		textitems[1].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x4154f4] [D]
// Moves on to the next sound card that starts, then reloads the sounds.
static int device_choosefunc(void)
{
	int old;
	int i;

	if (num_soundcards == 0)
		return 0;

	ReleaseSounds();

	if (num_soundcards == 1)
	{
		gSoundHardware = 0;
		ShutdownSoundSystem();
		InitSoundSystem(gSoundHardware, gSoundCompatibility, gUseEAX);
	}
	else
	{
		old = gSoundHardware;

		for (i = gSoundHardware + 1; i != old; i++)
		{
			if (i >= num_soundcards)
				i = 0;

			ShutdownSoundSystem();

			if (InitSoundSystem(i, gSoundCompatibility, gUseEAX))
				break;
		}

		gSoundHardware = i;
	}

	if (gDoMotelRoom)
	{
		LoadSounds("MOTEL.DSS", 0);
		SetSoundSystemEnvironment(5);
	}
	else
	{
		LoadSounds("TITLE.DSS", 0);
		SetSoundSystemEnvironment(0);
	}

	SetupAudioOptions();
	return 0;
}

// [0x4155f1] [D]
static void compatibility_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[2].font = 3;
		textitems[3].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 2;
		motel_item2 = 3;
	}
}

// [0x415637] [D]
static void compatibility_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[2].font = 2;
		textitems[3].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x415668] [D]
// Cycles the compatibility mode 0 -> 1 -> 2 -> 0 and restarts the sound system.
static int compatibility_choosefunc(void)
{
	ReleaseSounds();

	if (gSoundCompatibility == 1)
		gSoundCompatibility = 2;
	else
		gSoundCompatibility = gSoundCompatibility != 2;

	ShutdownSoundSystem();
	InitSoundSystem(gSoundHardware, gSoundCompatibility, gUseEAX);

	if (gDoMotelRoom)
	{
		LoadSounds("MOTEL.DSS", 0);
		SetSoundSystemEnvironment(5);
	}
	else
	{
		LoadSounds("TITLE.DSS", 0);
		SetSoundSystemEnvironment(0);
	}

	SetupAudioOptions();
	return 0;
}

// [0x415710] [D]
static void eax_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[4].font = 3;
		textitems[5].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 4;
		motel_item2 = 5;
	}
}

// [0x415756] [D]
static void eax_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[4].font = 2;
		textitems[5].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x415787] [D]
static int eax_choosefunc(void)
{
	gUseEAX ^= 1;

	ReleaseSounds();
	ShutdownSoundSystem();
	InitSoundSystem(gSoundHardware, gSoundCompatibility, gUseEAX);

	if (gDoMotelRoom)
	{
		LoadSounds("MOTEL.DSS", 0);
		SetSoundSystemEnvironment(5);
	}
	else
	{
		LoadSounds("TITLE.DSS", 0);
		SetSoundSystemEnvironment(0);
	}

	SetupAudioOptions();
	return 0;
}

// [0x415803] [D]
static void sounds_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[6].font = 3;
		textitems[7].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 6;
		motel_item2 = 7;
	}
}

// [0x415849] [D]
static void sounds_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[6].font = 2;
		textitems[7].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x41587a] [D]
static int sounds_choosefunc(void)
{
	gAllowSounds ^= 1;

	SetupAudioOptions();
	return 0;
}

// [0x415893] [D]
static void cdaudio_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[8].font = 3;
		textitems[9].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 8;
		motel_item2 = 9;
	}
}

// [0x4158d9] [D]
static void cdaudio_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[8].font = 2;
		textitems[9].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x41590a] [D]
static int cdaudio_choosefunc(void)
{
	gUseCDAudio ^= 1;

	if (gUseCDAudio == 0)
		StopCDTrack();

	SetupAudioOptions();
	return 0;
}

// [0x415931] [D]
static void doppler_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[10].font = 3;
		textitems[11].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 10;
		motel_item2 = 11;
	}
}

// [0x415977] [D]
static void doppler_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[10].font = 2;
		textitems[11].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x4159a8] [D]
static int doppler_choosefunc(void)
{
	gUseDoppler ^= 1;
	SetSoundSystemDoppler(gUseDoppler);

	SetupAudioOptions();
	return 0;
}

// [0x4159d0] [D]
static void bits_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[12].font = 3;
		textitems[13].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 12;
		motel_item2 = 13;
	}
}

// [0x415a16] [D]
static void bits_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[12].font = 2;
		textitems[13].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x415a47] [D]
static int bits_choosefunc(void)
{
	if (gAudioMixerBits == 8)
		gAudioMixerBits = 16;
	else
		gAudioMixerBits = 8;

	SetSoundSystemFormat(gAudioMixerBits, gAudioMixerFreq);

	SetupAudioOptions();
	return 0;
}

// [0x415a87] [D]
static void freq_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[14].font = 3;
		textitems[15].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 14;
		motel_item2 = 15;
	}
}

// [0x415acd] [D]
static void freq_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[14].font = 2;
		textitems[15].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x415afe] [D]
static int freq_choosefunc(void)
{
	if (gAudioMixerFreq == 22050)
		gAudioMixerFreq = 44100;
	else
		gAudioMixerFreq = 22050;

	SetSoundSystemFormat(gAudioMixerBits, gAudioMixerFreq);

	SetupAudioOptions();
	return 0;
}

// [0x415b41] [D]
static void speakers_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[16].font = 3;
		textitems[17].font = 3;
	}
	else if (flag == 2)
	{
		motel_item = 16;
		motel_item2 = 17;
	}
}

// [0x415b87] [D]
static void speakers_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[16].font = 2;
		textitems[17].font = 2;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
	}
}

// [0x415bb8] [D]
static int speakers_choosefunc(void)
{
	if (++gAudioSpeakerMode == 5)
		gAudioSpeakerMode = 0;

	SetSoundSystemMode(gAudioSpeakerMode);

	SetupAudioOptions();
	return 0;
}

// [0x415bf3] [D]
static void mastervolume_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[18].font = 3;
		on_slider = 1;
	}
	else if (flag == 2)
	{
		motel_item = 18;
		motel_item2 = -1;
		on_slider = 1;
	}
}

// [0x415c46] [D]
static void mastervolume_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[18].font = 2;
		on_slider = 0;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
		on_slider = 0;
	}
}

// [0x415c84] [D]
static int mastervolume_choosefunc(void)
{
	return 0;
}

// [0x415c8b] [D]
static void musicvolume_selfunc(int flag)
{
	if (flag == 0 || flag == 1)
	{
		textitems[19].font = 3;
		on_slider = 1;
	}
	else if (flag == 2)
	{
		motel_item = 19;
		motel_item2 = -1;
		on_slider = 1;
	}
}

// [0x415cde] [D]
static void musicvolume_unselfunc(int flag)
{
	if (flag == 0)
	{
		textitems[19].font = 2;
		on_slider = 0;
	}
	else if (flag == 2)
	{
		UnhighlightMotelItem();
		on_slider = 0;
	}
}

// [0x415d1c] [D]
static int musicvolume_choosefunc(void)
{
	return 0;
}

// [0x415d23] [D]
static void mastervolume_up(int flag)
{
	if (gMasterVolume < 1.0f)
	{
		gMasterVolume += 1.0f / 64.0f;
		SetupAudioOptions();
	}
}

// [0x415d52] [D]
static void mastervolume_down(int flag)
{
	if (gMasterVolume > 0.0f)
	{
		gMasterVolume -= 1.0f / 64.0f;
		SetupAudioOptions();
	}
}

// [0x415d81] [D]
static void musicvolume_up(int flag)
{
	if (gMusicVolume < 1.0f)
	{
		gMusicVolume += 1.0f / 64.0f;
		SetupAudioOptions();
	}
}

// [0x415db0] [D]
static void musicvolume_down(int flag)
{
	if (gMusicVolume > 0.0f)
	{
		gMusicVolume -= 1.0f / 64.0f;
		SetupAudioOptions();
	}
}

// [0x415ddf] [D]
// NOTE: sub_415DDF, the name is a guess
// Draws the texts of the motel room item in red.
static void HighlightMotelItem(void)
{
	if (selection == -1)
		return;

	textitems[motel_item].color.r = 255;
	textitems[motel_item].color.g = 0;
	textitems[motel_item].color.b = 0;

	if (motel_item2 != -1)
	{
		textitems[motel_item2].color.r = 255;
		textitems[motel_item2].color.g = 0;
		textitems[motel_item2].color.b = 0;
	}
}

// [0x415e54] [D]
// NOTE: sub_415E54, the name is a guess
static void UnhighlightMotelItem(void)
{
	textitems[motel_item].color.r = 255;
	textitems[motel_item].color.g = 255;
	textitems[motel_item].color.b = 255;

	if (motel_item2 != -1)
	{
		textitems[motel_item2].color.r = 255;
		textitems[motel_item2].color.g = 255;
		textitems[motel_item2].color.b = 255;
	}
}

// [0x415ec0] [D]
// The volume sliders in the motel room; the one highlighted is drawn red.
void DrawMotelAudioOptions(void)
{
	u_char master_g;
	u_char master_b;
	u_char music_g;
	u_char music_b;

	master_g = 255;
	master_b = 255;
	music_g = 255;
	music_b = 255;

	if (gDoMotelRoom == 0)
		return;

	if (motel_item == 18)
	{
		master_g = 0;
		master_b = 0;
	}

	if (motel_item == 19)
	{
		music_g = 0;
		music_b = 0;
	}

	if (slider_tex)
	{
		MyDrawIcon(467.0f, 381.0f, slider_tex, 255, master_g, master_b, 0, 0);
		MyDrawIcon(467.0f, 406.0f, slider_tex, 255, music_g, music_b, 0, 0);
	}

	if (sliderbar_tex)
	{
		MyDrawSlider(467.0f, 381.0f, gMasterVolume, 1.0f, sliderbar_tex, 255, master_g, master_b, 0);
		MyDrawSlider(467.0f, 406.0f, gMusicVolume, 1.0f, sliderbar_tex, 255, music_g, music_b, 0);
	}
}
