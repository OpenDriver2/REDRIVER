/*
 * flat/answerphone.c - motel room answerphone: play the messages of the mission ladder room,
 * accept a mission or go to the car select
 *
 * Rebuilt from Driver_sym2.i64 (IDA folder "motel\answerphone.c=flat\answerphone.c",
 * 0x416a20 - 0x417446)
 */

#include <stdio.h>
#include "dr_keys.h"

#include "dr_types.h"
#include "dr_globals.h"
#include "pres.h"
#include "shell.h"
#include "input.h"
#include "map.h"
#include "sound.h"
#include "flat/carselect.h"
#include "flat/flat.h"
#include "flat/frontend.h"
#include "flat/fudge.h"
#include "flat/prescam.h"
#include "flat/answerphone.h"

void SetupMotelAnswerphone(void);
int CheckMotelAnswerphoneKeys(void);
static void DrawIcons(void);
void SetOptionsMotelAnswerphone(void);
void PlayCurrentMessage(void);

//------------------------------------------------------------------------------------------
// initialised data (0x550370)

int num_messages = 1;
static int icon_flash_value = 50;
static int icon_flash_direction = 20;
// NOTE: 0x55037c - 0x550388, no references
static int unknown_55037C[4] = { 252, 253, 254, 255 };

static FE_TEXT textitems[2] = {
	{ 320, 430, 0, 1, 0, 0, NULL, { 255, 255, 255, 255 } },
	{ -1, -1, 0, 0, 0, 0, NULL, { 255, 255, 255, 255 } },
};

//------------------------------------------------------------------------------------------
// uninitialised data (0x600c60)

int highlighted_icon;			// 0 previous, 1 play, 2 next, 3 accept
int current_message;
// NOTE: dword_600C68; the number of messages not played yet
static int unplayed_messages;
TEX_INFO *FFwdButtonTexture;
TEX_INFO *PlayButtonTexture;
TEX_INFO *AcceptButtonTexture;

// [0x416a20] [D]
// Starts with the "n new messages" sample; returns -1 when left, 1 when a mission was accepted.
int DoMotelAnswerphone(void)
{
	int ret;

	SetupMotelAnswerphone();

	switch (unplayed_messages)
	{
		case 1:
			SimplePlaySound(3, 10, 22050);
			break;
		case 2:
			SimplePlaySound(4, 10, 22050);
			break;
		case 3:
			SimplePlaySound(5, 10, 22050);
			break;
		case 4:
			SimplePlaySound(6, 10, 22050);
			break;
	}

	for (;;)
	{
		HandleWinMessages();
		ReadInput();

		ret = CheckMotelAnswerphoneKeys();

		if (ret != 0)
			break;

		DrawMotel(textitems, DrawIcons);
	}

	StopSound(10);
	return ret;
}

// [0x416ae5] [D]
// Counts the room's messages and the unplayed ones; a car message sets the car to drive.
void SetupMotelAnswerphone(void)
{
	int carpark;

	FFwdButtonTexture = GetTextureDetails("FFWD");
	PlayButtonTexture = GetTextureDetails("PLAY");
	AcceptButtonTexture = GetTextureDetails("ACCEPT8B");

	highlighted_icon = 1;
	unplayed_messages = 0;

	for (num_messages = 0; num_messages < 4 && (room_data->messages[num_messages].type & MSG_END) == 0; num_messages++)
	{
		if ((room_data->messages[num_messages].type & MSG_PLAYED) == 0)
			unplayed_messages++;

		if (room_data->messages[num_messages].type & MSG_CAR)
		{
			if ((unsigned short)room_data->messages[num_messages].data > 2)
				carpark = 2;
			else
				carpark = (unsigned short)room_data->messages[num_messages].data;

			gStoredPlayerCarType = gMotelCarLookup[carpark][0];
		}
	}

	current_message = unplayed_messages % num_messages;

	SetOptionsMotelAnswerphone();
}

// [0x416c74] [D]
// -1: left, 1: the mission was accepted, 0: carry on.
int CheckMotelAnswerphoneKeys(void)
{
	if (CheckKey(DIK_ESCAPE))
	{
		PlayMotelClickSound();
		return -1;
	}

	if (CheckKey(DIK_LEFT))
	{
		PlayMotelClickSound();

		if (--highlighted_icon < 0)
			highlighted_icon = 3;
	}
	else if (CheckKey(DIK_RIGHT))
	{
		PlayMotelClickSound();

		if (++highlighted_icon > 3)
			highlighted_icon = 0;
	}
	else
	{
		if (CheckKey(DIK_RETURN))
		{
			PlayMotelClickSound();

			switch (highlighted_icon)
			{
				case 0:
					if (--current_message < 0)
						current_message = num_messages - 1;

					PlayCurrentMessage();
					return 0;
				case 1:
					PlayCurrentMessage();
					return 0;
				case 2:
					if (++current_message >= num_messages)
						current_message = 0;

					PlayCurrentMessage();
					return 0;
				case 3:
					if (room_data->messages[current_message].type & MSG_MISSION)
					{
						FadeOutMotel(textitems, DrawIcons);
						flat_retcode = current_message;
						return 1;
					}

					if (room_data->messages[current_message].type & MSG_CAR)
					{
						FadeOutMotel(textitems, DrawIcons);
						StopSound(10);

						DoMotelCarSelect((unsigned short)room_data->messages[current_message].data);

						PlaceCameraAt(4);
						MoveCameraTo(9);
						UpdateMotelRegion();

						FadeInMotel(textitems, DrawIcons);
					}
					break;
				default:
					highlighted_icon = 0;
					return 0;
			}
		}

		return 0;
	}

	SetOptionsMotelAnswerphone();
	return 0;
}

// [0x416e8f] [D]
// The previous (mirrored next), play, next and accept buttons; the highlighted one pulses.
static void DrawIcons(void)
{
	u_char value;

	icon_flash_value += icon_flash_direction;

	if (icon_flash_value > 255)
	{
		icon_flash_value = 255;
		icon_flash_direction = -icon_flash_direction;
	}

	if (icon_flash_value < 0)
	{
		icon_flash_value = 0;
		icon_flash_direction = -icon_flash_direction;
	}

	value = icon_flash_value;

	if (FFwdButtonTexture)
	{
		if (highlighted_icon != 0)
		{
			MyDrawIcon(144.0f, 352.0f, FFwdButtonTexture, 255, 255, 255, 1, 2);
			MyDrawIcon(142.0f, 350.0f, FFwdButtonTexture, 255, 255, 255, 1, 3);
		}
		else
		{
			MyDrawIcon(144.0f, 352.0f, FFwdButtonTexture, value, value, value, 1, 2);
			MyDrawIcon(142.0f, 350.0f, FFwdButtonTexture, value, value, value, 1, 3);
		}
	}

	if (PlayButtonTexture)
	{
		if (highlighted_icon == 1)
		{
			MyDrawIcon(244.0f, 352.0f, PlayButtonTexture, value, value, value, 0, 2);
			MyDrawIcon(242.0f, 350.0f, PlayButtonTexture, value, value, value, 0, 3);
		}
		else
		{
			MyDrawIcon(244.0f, 352.0f, PlayButtonTexture, 255, 255, 255, 0, 2);
			MyDrawIcon(242.0f, 350.0f, PlayButtonTexture, 255, 255, 255, 0, 3);
		}
	}

	if (FFwdButtonTexture)
	{
		if (highlighted_icon == 2)
		{
			MyDrawIcon(344.0f, 352.0f, FFwdButtonTexture, value, value, value, 0, 2);
			MyDrawIcon(342.0f, 350.0f, FFwdButtonTexture, value, value, value, 0, 3);
		}
		else
		{
			MyDrawIcon(344.0f, 352.0f, FFwdButtonTexture, 255, 255, 255, 0, 2);
			MyDrawIcon(342.0f, 350.0f, FFwdButtonTexture, 255, 255, 255, 0, 3);
		}
	}

	if (AcceptButtonTexture)
	{
		if (highlighted_icon == 3)
		{
			MyDrawIcon(444.0f, 352.0f, AcceptButtonTexture, value, value, value, 0, 2);
			MyDrawIcon(442.0f, 350.0f, AcceptButtonTexture, value, value, value, 0, 3);
		}
		else
		{
			MyDrawIcon(444.0f, 352.0f, AcceptButtonTexture, 255, 255, 255, 0, 2);
			MyDrawIcon(442.0f, 350.0f, AcceptButtonTexture, 255, 255, 255, 0, 3);
		}
	}
}

// [0x4171f7] [D]
void SetOptionsMotelAnswerphone(void)
{
	switch (highlighted_icon)
	{
		case 1:
			textitems[0].text = gLanguageStrings[139];
			break;
		case 2:
			textitems[0].text = gLanguageStrings[140];
			break;
		case 3:
			textitems[0].text = gLanguageStrings[141];
			break;
		default:
			textitems[0].text = gLanguageStrings[138];
			break;
	}
}

// [0x41724d] [D]
// Marks the message played and plays its sample: a joke, a car park or an answerphone message.
void PlayCurrentMessage(void)
{
	char filename[32];
	int number;

	StopSound(10);

	room_data->messages[current_message].type |= MSG_PLAYED;

	if (room_data->messages[current_message].type & MSG_JOKE)
	{
		if ((unsigned short)room_data->messages[current_message].data > 3)
			number = 3;
		else
			number = (unsigned short)room_data->messages[current_message].data;

		sprintf(filename, "AUDIO\\MOTEL\\JOKE%03d.WAV", number);
	}
	else if (room_data->messages[current_message].type & MSG_CAR)
	{
		if ((unsigned short)room_data->messages[current_message].data > 2)
			number = 2;
		else
			number = (unsigned short)room_data->messages[current_message].data;

		sprintf(filename, "AUDIO\\MOTEL\\CAR%03d.WAV", number + 1);
	}
	else
	{
		number = (unsigned short)room_data->messages[current_message].data;
		sprintf(filename, "AUDIO\\MOTEL\\ANS%03d.WAV", number);
	}

	if (ChangeSample(filename, 7))
		SimplePlaySound(7, 10, 22050);
}
