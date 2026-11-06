/*
 * input.c - keyboard and game controllers (with force feedback) through SDL
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "input.c"). The original is DirectInput 5 ([A]);
 * the mouse device is gone, nothing reads it.
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <SDL.h>

#include "dr_types.h"
#include "dr_globals.h"
#include "frontend/gvars.h"
#include "frontend/keymap.h"
#include "handling.h"
#include "input.h"
#include "shell.h"
#include "asmmath.h"
#include "debug.h"
#include "camera.h"
#include "replays.h"
#include "pause.h"
#include "dr_keys.h"

#define MAX_INPUT_DEVICES	8
#define JOY_BUTTONS			32
#define NO_EFFECT			-1		// [A] SDL effect ids start at 0
#define TRIGGER_DOWN		16384	// [A] triggers count as buttons from half way
#define MENU_STICK_DOWN		10000	// [A] about what -31..31 was with the default dead zone

// the length of the force feedback effects
#define DIRECTION_EFFECT_MS		250
#define MAGNITUDE_EFFECT_MS		15

// [A] rumble: forces in the DirectInput scale. Steering only hums, knocks go through in full and fade.
#define RUMBLE_STEER_MAX		2500
#define RUMBLE_JOLT_MIN			500

// joystick "buttons" beyond the real ones (frontend\gvars.c DIJ_ButtonLookup: "Up", "Down", "None")
#define JOY_UP				32
#define JOY_DOWN			33
#define JOY_NONE			34

#define REPLAY_PLAYBACK		2	// ReplayMode: the replay drives the cars

void ShutdownDevices(void);
int SetJoystickDevice(int device);
void UpdateForces(void);
int IsJoyKeyPressed(u_char key);
int ReadKeyboardInput(void);
static void ReadJoystickState(void);
static void ReadMenuButtons(void);
static int OpenController(int index);
static int CreateJoyDirectionEffect(SDL_Haptic *device, int index);
static int CreateJoyMagnitudeEffect(SDL_Haptic *device, int index);
static int ForceLevel(int magnitude);
static void UpdateRumble(void);
static void SetJoyKey(u_char joykey, u_char key);

//------------------------------------------------------------------------------------------
// initialised data (0x564cb0)

char InputDevice = -1;
// NOTE: unnamed in IDA (dword_564CB4); name guessed. Joysticks are only looked for when set.
static int use_joysticks = 1;
// NOTE: 0x564cb8 - 0x564cc8 unnamed, no references; left out

//------------------------------------------------------------------------------------------
// uninitialised data
// NOTE: unmarked bss globals in the linker's common area (0xbb0f38 - 0xbb10f0, 0xd83c10 -
// 0xd83ff4); defined here as this file is their main user

// [A] what the game reads of DIJOYSTATE
typedef struct
{
	int lX;
	int lY;
	u_char rgbButtons[JOY_BUTTONS];		// 0x80 = down
} JOY_STATE;

int magnitude_effects[MAX_INPUT_DEVICES];
int ff_devices[MAX_INPUT_DEVICES];
int direction_effects[MAX_INPUT_DEVICES];
SDL_GameController *input_devices[MAX_INPUT_DEVICES];
SDL_JoystickID device_ids[MAX_INPUT_DEVICES];		// [A] the removed event only gives the id
SDL_Haptic *haptic_devices[MAX_INPUT_DEVICES];		// [A] SDL keeps them apart from the joysticks
int dead_zones[MAX_INPUT_DEVICES];					// [A] was a property of the device, 0 - 10000
JOY_STATE BackupJoyState;
JOY_STATE JoyState;
static int input_started;							// [A] in place of lpDirectInput
static int controller_lost;						// [A] the next controller to arrive takes over
static int rumble_active;							// [A] a rumble is running
int JoyJolt;										// [A] the strongest SetForceDirection since the last rumble
SDL_GameController *JoyDevice;
SDL_Haptic *JoyHaptic;
int JoyDirectionEffect = NO_EFFECT;
int JoyMagnitudeEffect = NO_EFFECT;
int OldJoyAnalogueX;
int OldJoyAnalogueY;
int JoyForce;
int JoyDirectionX;
int JoyDirectionY;
int UseJoyMagnitude;
int UseJoyDirection;
int num_input_devices;
int current_joystick_device;
int JoyAnalogueX;
int JoyAnalogueY;
int JoyIsValid;
int JoyIsAnalogue;
int JoyIsFF;
int OldJoyIsAnalogue;
int OldJoyIsFF;
int JoyDeadZone;

CONTROL_JOY_SET JoySet;
char input_device_names[MAX_INPUT_DEVICES][32];
char KeyMap[256];
char PrevKeyMap[256];
CONTROL_KEY_SET KeySet;

// NOTE: unnamed in IDA, only read and cleared by the unreferenced sub_4E58F0 - sub_4E594D
char byte_BB10FC;
char byte_BB10FD;
char byte_BB10FE;
char byte_BB10FF;

// [A] the positions in rgbButtons: the numbers DirectInput gives an XInput pad come first, as
// DefaultJoySet expects them. The triggers follow as JOY_TRIGGER_LEFT and JOY_TRIGGER_RIGHT.
// frontend\gvars.c DIJ_ButtonLookup has their names in this order.
static const SDL_GameControllerButton JoyButtons[] = {
	SDL_CONTROLLER_BUTTON_A,
	SDL_CONTROLLER_BUTTON_B,
	SDL_CONTROLLER_BUTTON_X,
	SDL_CONTROLLER_BUTTON_Y,
	SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
	SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
	SDL_CONTROLLER_BUTTON_BACK,
	SDL_CONTROLLER_BUTTON_START,
	SDL_CONTROLLER_BUTTON_LEFTSTICK,
	SDL_CONTROLLER_BUTTON_RIGHTSTICK,
	SDL_CONTROLLER_BUTTON_DPAD_UP,
	SDL_CONTROLLER_BUTTON_DPAD_DOWN,
	SDL_CONTROLLER_BUTTON_DPAD_LEFT,
	SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
	SDL_CONTROLLER_BUTTON_GUIDE,
	SDL_CONTROLLER_BUTTON_MISC1,
	SDL_CONTROLLER_BUTTON_PADDLE1,
	SDL_CONTROLLER_BUTTON_PADDLE2,
	SDL_CONTROLLER_BUTTON_PADDLE3,
	SDL_CONTROLLER_BUTTON_PADDLE4,
	SDL_CONTROLLER_BUTTON_TOUCHPAD,
};

#define JOY_TRIGGER_LEFT	(sizeof(JoyButtons) / sizeof(JoyButtons[0]))
#define JOY_TRIGGER_RIGHT	(JOY_TRIGGER_LEFT + 1)

// [A] SDL scancode -> DIK_*. Both are physical key positions, so a layout changes neither.
static const struct
{
	SDL_Scancode scancode;
	u_char key;
} ScancodeToKey[] = {
	{ SDL_SCANCODE_ESCAPE, DIK_ESCAPE },
	{ SDL_SCANCODE_1, DIK_1 },
	{ SDL_SCANCODE_2, DIK_2 },
	{ SDL_SCANCODE_3, DIK_3 },
	{ SDL_SCANCODE_4, DIK_4 },
	{ SDL_SCANCODE_5, DIK_5 },
	{ SDL_SCANCODE_6, DIK_6 },
	{ SDL_SCANCODE_7, DIK_7 },
	{ SDL_SCANCODE_8, DIK_8 },
	{ SDL_SCANCODE_9, DIK_9 },
	{ SDL_SCANCODE_0, DIK_0 },
	{ SDL_SCANCODE_MINUS, DIK_MINUS },
	{ SDL_SCANCODE_EQUALS, DIK_EQUALS },
	{ SDL_SCANCODE_BACKSPACE, DIK_BACK },
	{ SDL_SCANCODE_TAB, DIK_TAB },
	{ SDL_SCANCODE_Q, DIK_Q },
	{ SDL_SCANCODE_W, DIK_W },
	{ SDL_SCANCODE_E, DIK_E },
	{ SDL_SCANCODE_R, DIK_R },
	{ SDL_SCANCODE_T, DIK_T },
	{ SDL_SCANCODE_Y, DIK_Y },
	{ SDL_SCANCODE_U, DIK_U },
	{ SDL_SCANCODE_I, DIK_I },
	{ SDL_SCANCODE_O, DIK_O },
	{ SDL_SCANCODE_P, DIK_P },
	{ SDL_SCANCODE_LEFTBRACKET, DIK_LBRACKET },
	{ SDL_SCANCODE_RIGHTBRACKET, DIK_RBRACKET },
	{ SDL_SCANCODE_RETURN, DIK_RETURN },
	{ SDL_SCANCODE_LCTRL, DIK_LCONTROL },
	{ SDL_SCANCODE_A, DIK_A },
	{ SDL_SCANCODE_S, DIK_S },
	{ SDL_SCANCODE_D, DIK_D },
	{ SDL_SCANCODE_F, DIK_F },
	{ SDL_SCANCODE_G, DIK_G },
	{ SDL_SCANCODE_H, DIK_H },
	{ SDL_SCANCODE_J, DIK_J },
	{ SDL_SCANCODE_K, DIK_K },
	{ SDL_SCANCODE_L, DIK_L },
	{ SDL_SCANCODE_SEMICOLON, DIK_SEMICOLON },
	{ SDL_SCANCODE_APOSTROPHE, DIK_APOSTROPHE },
	{ SDL_SCANCODE_GRAVE, DIK_GRAVE },
	{ SDL_SCANCODE_LSHIFT, DIK_LSHIFT },
	{ SDL_SCANCODE_BACKSLASH, DIK_BACKSLASH },
	{ SDL_SCANCODE_Z, DIK_Z },
	{ SDL_SCANCODE_X, DIK_X },
	{ SDL_SCANCODE_C, DIK_C },
	{ SDL_SCANCODE_V, DIK_V },
	{ SDL_SCANCODE_B, DIK_B },
	{ SDL_SCANCODE_N, DIK_N },
	{ SDL_SCANCODE_M, DIK_M },
	{ SDL_SCANCODE_COMMA, DIK_COMMA },
	{ SDL_SCANCODE_PERIOD, DIK_PERIOD },
	{ SDL_SCANCODE_SLASH, DIK_SLASH },
	{ SDL_SCANCODE_RSHIFT, DIK_RSHIFT },
	{ SDL_SCANCODE_KP_MULTIPLY, DIK_MULTIPLY },
	{ SDL_SCANCODE_LALT, DIK_LMENU },
	{ SDL_SCANCODE_SPACE, DIK_SPACE },
	{ SDL_SCANCODE_CAPSLOCK, DIK_CAPITAL },
	{ SDL_SCANCODE_F1, DIK_F1 },
	{ SDL_SCANCODE_F2, DIK_F2 },
	{ SDL_SCANCODE_F3, DIK_F3 },
	{ SDL_SCANCODE_F4, DIK_F4 },
	{ SDL_SCANCODE_F5, DIK_F5 },
	{ SDL_SCANCODE_F6, DIK_F6 },
	{ SDL_SCANCODE_F7, DIK_F7 },
	{ SDL_SCANCODE_F8, DIK_F8 },
	{ SDL_SCANCODE_F9, DIK_F9 },
	{ SDL_SCANCODE_F10, DIK_F10 },
	{ SDL_SCANCODE_F11, DIK_F11 },
	{ SDL_SCANCODE_F12, DIK_F12 },
	{ SDL_SCANCODE_F13, DIK_F13 },
	{ SDL_SCANCODE_F14, DIK_F14 },
	{ SDL_SCANCODE_F15, DIK_F15 },
	{ SDL_SCANCODE_NUMLOCKCLEAR, DIK_NUMLOCK },
	{ SDL_SCANCODE_SCROLLLOCK, DIK_SCROLL },
	{ SDL_SCANCODE_KP_7, DIK_NUMPAD7 },
	{ SDL_SCANCODE_KP_8, DIK_NUMPAD8 },
	{ SDL_SCANCODE_KP_9, DIK_NUMPAD9 },
	{ SDL_SCANCODE_KP_MINUS, DIK_SUBTRACT },
	{ SDL_SCANCODE_KP_4, DIK_NUMPAD4 },
	{ SDL_SCANCODE_KP_5, DIK_NUMPAD5 },
	{ SDL_SCANCODE_KP_6, DIK_NUMPAD6 },
	{ SDL_SCANCODE_KP_PLUS, DIK_ADD },
	{ SDL_SCANCODE_KP_1, DIK_NUMPAD1 },
	{ SDL_SCANCODE_KP_2, DIK_NUMPAD2 },
	{ SDL_SCANCODE_KP_3, DIK_NUMPAD3 },
	{ SDL_SCANCODE_KP_0, DIK_NUMPAD0 },
	{ SDL_SCANCODE_KP_PERIOD, DIK_DECIMAL },
	{ SDL_SCANCODE_KP_EQUALS, DIK_NUMPADEQUALS },
	{ SDL_SCANCODE_KP_COMMA, DIK_NUMPADCOMMA },
	{ SDL_SCANCODE_KP_ENTER, DIK_NUMPADENTER },
	{ SDL_SCANCODE_KP_DIVIDE, DIK_DIVIDE },
	{ SDL_SCANCODE_NONUSBACKSLASH, DIK_OEM_102 },
	{ SDL_SCANCODE_INTERNATIONAL1, DIK_ABNT_C1 },
	{ SDL_SCANCODE_INTERNATIONAL2, DIK_KANA },
	{ SDL_SCANCODE_INTERNATIONAL3, DIK_YEN },
	{ SDL_SCANCODE_INTERNATIONAL4, DIK_CONVERT },
	{ SDL_SCANCODE_INTERNATIONAL5, DIK_NOCONVERT },
	{ SDL_SCANCODE_RCTRL, DIK_RCONTROL },
	{ SDL_SCANCODE_PRINTSCREEN, DIK_SYSRQ },
	{ SDL_SCANCODE_RALT, DIK_RMENU },
	{ SDL_SCANCODE_PAUSE, DIK_PAUSE },
	{ SDL_SCANCODE_HOME, DIK_HOME },
	{ SDL_SCANCODE_UP, DIK_UP },
	{ SDL_SCANCODE_PAGEUP, DIK_PRIOR },
	{ SDL_SCANCODE_LEFT, DIK_LEFT },
	{ SDL_SCANCODE_RIGHT, DIK_RIGHT },
	{ SDL_SCANCODE_END, DIK_END },
	{ SDL_SCANCODE_DOWN, DIK_DOWN },
	{ SDL_SCANCODE_PAGEDOWN, DIK_NEXT },
	{ SDL_SCANCODE_INSERT, DIK_INSERT },
	{ SDL_SCANCODE_DELETE, DIK_DELETE },
	{ SDL_SCANCODE_LGUI, DIK_LWIN },
	{ SDL_SCANCODE_RGUI, DIK_RWIN },
	{ SDL_SCANCODE_APPLICATION, DIK_APPS },
	{ SDL_SCANCODE_POWER, DIK_POWER },
	{ SDL_SCANCODE_SLEEP, DIK_SLEEP },
};

// [A] the keys controllers stand in for in menus, as bit numbers of ReadMenuButtons
enum
{
	MENU_RETURN,
	MENU_ESCAPE,
	MENU_BACK,
	MENU_LEFT,
	MENU_RIGHT,
	MENU_UP,
	MENU_DOWN,
	MENU_KEYS
};

static const u_char MenuKeys[MENU_KEYS] = {
	DIK_RETURN, DIK_ESCAPE, DIK_BACK, DIK_LEFTARROW, DIK_RIGHTARROW, DIK_UPARROW, DIK_DOWNARROW
};

static const struct
{
	SDL_GameControllerButton button;
	int menukey;
} MenuButtons[] = {
	{ SDL_CONTROLLER_BUTTON_A, MENU_RETURN },
	{ SDL_CONTROLLER_BUTTON_Y, MENU_ESCAPE },
	{ SDL_CONTROLLER_BUTTON_B, MENU_BACK },
	{ SDL_CONTROLLER_BUTTON_DPAD_LEFT, MENU_LEFT },
	{ SDL_CONTROLLER_BUTTON_DPAD_RIGHT, MENU_RIGHT },
	{ SDL_CONTROLLER_BUTTON_DPAD_UP, MENU_UP },
	{ SDL_CONTROLLER_BUTTON_DPAD_DOWN, MENU_DOWN },
};

static int menu_ignored;	// [A] held since before the menu took over; ignored until released

int ControllerDown;			// [A] a button of any controller is down
int ControllerPressed;		// [A] ... and that started this frame

// NOTE: moved here from the generated stubs; no IDA mark, the owner is a guess from the users
// and the neighbouring data
char movedown;
char moveup;

// [0x4e4e40] [D] [A]
// NOTE: IDA shows __fastcall with register junk; it takes no arguments
// NOTE: [A] the keyboard can't fail any more, and no controller subsystem just means no controllers
int InitDirectInput(void)
{
	int i;
	char message[128];

	if (input_started)
		ShutdownDevices();

	InputDevice = INPUTDEVICE_KEYBOARD;
	KeySet = DefaultKeySet;
	memcpy(&JoySet, &DefaultJoySet, sizeof(JoySet));

	JoyIsValid = 0;
	num_input_devices = 0;
	controller_lost = 0;

	for (i = 0; i < MAX_INPUT_DEVICES; i++)
	{
		direction_effects[i] = NO_EFFECT;
		magnitude_effects[i] = NO_EFFECT;
	}

	if (use_joysticks)
	{
		// NOTE: [A] without these PlayStation pads have no rumble over Bluetooth. The pad then stays
		// in the extended mode until it is switched off, which upsets programs on plain DirectInput.
		SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
		SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");

		if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0)
		{
			sprintf(message, "Input: no game controllers: %s", SDL_GetError());
			LogQuick(message);
			return 1;
		}

		input_started = 1;

		// force feedback is optional
		SDL_InitSubSystem(SDL_INIT_HAPTIC);

		// more mappings, if the file is there
		SDL_GameControllerAddMappingsFromFile("gamecontrollerdb.txt");

		// SDL_CONTROLLERDEVICEADDED comes for these as well, ControllerAdded skips them then
		for (i = 0; i < SDL_NumJoysticks(); i++)
			ControllerAdded(i);

		if (num_input_devices > 0)
			SetJoystickDevice(0);
	}

	return 1;
}

// [0x4e5135] [D] [A]
void ShutdownDevices(void)
{
	int i;

	for (i = 0; i < MAX_INPUT_DEVICES; i++)
	{
		direction_effects[i] = NO_EFFECT;
		magnitude_effects[i] = NO_EFFECT;

		// frees the effects as well
		if (haptic_devices[i])
		{
			SDL_HapticClose(haptic_devices[i]);
			haptic_devices[i] = NULL;
		}

		if (input_devices[i])
		{
			SDL_GameControllerClose(input_devices[i]);
			input_devices[i] = NULL;
		}
	}

	JoyDevice = NULL;
	JoyHaptic = NULL;
	JoyDirectionEffect = NO_EFFECT;
	JoyMagnitudeEffect = NO_EFFECT;

	if (input_started)
	{
		SDL_QuitSubSystem(SDL_INIT_HAPTIC | SDL_INIT_GAMECONTROLLER);
		input_started = 0;
	}

	JoyIsValid = 0;
	num_input_devices = 0;
}

// [0x4e52f1] [D] [A]
// Reads the keyboard (and the joystick, mapped onto KeyMap).
int ReadInput(void)
{
	if (!gScreen.active)
		return 0;

	UpdateForces();

	if (InputDevice == INPUTDEVICE_KEYBOARD)
	{
		OldJoyIsAnalogue = 0;
		JoyAnalogueX = 0;
		JoyAnalogueY = 0;

		ReadKeyboardInput();
		ReadMenuButtons();

		return 1;
	}

	if (InputDevice != INPUTDEVICE_JOYSTICK)
		return 0;

	// NOTE: [A] JoyDevice: the frontend can select a controller that has been removed meanwhile
	if (!use_joysticks || !JoyDevice)
	{
		InputDevice = INPUTDEVICE_KEYBOARD;
		return 0;
	}

	BackupJoyState = JoyState;

	OldJoyAnalogueX = JoyAnalogueX;
	OldJoyAnalogueY = JoyAnalogueY;
	OldJoyIsAnalogue = JoyIsAnalogue;
	OldJoyIsFF = JoyIsFF;

	ReadKeyboardInput();
	ReadMenuButtons();

	if (!gInPresentation && ReplayMode == REPLAY_PLAYBACK)
		return 1;

	// NOTE: [A] unplugged; SDL_PumpEvents in ReadKeyboardInput has updated the state
	if (!SDL_GameControllerGetAttached(JoyDevice))
		return 0;

	ReadJoystickState();

	JoyAnalogueX = JoyState.lX;
	JoyAnalogueY = JoyState.lY;

	// the menus are in ReadMenuButtons
	if (!gInPresentation)
	{
		if (JoyAnalogueX < -31)
			KeyMap[KeySet.SteerLeft] |= 0x80;
		else
			KeyMap[KeySet.SteerLeft] = 0;

		if (JoyAnalogueX > 31)
			KeyMap[KeySet.SteerRight] |= 0x80;
		else
			KeyMap[KeySet.SteerRight] = 0;

		SetJoyKey(JoySet.Accelerate, KeySet.Accelerate);
		SetJoyKey(JoySet.Brake, KeySet.Brake);
		SetJoyKey(JoySet.FastSteer, KeySet.FastSteer);
		SetJoyKey(JoySet.Handbrake, KeySet.Handbrake);
		SetJoyKey(JoySet.Burnout, KeySet.Burnout);

		// NOTE: the look buttons are crossed over (as in the binary)
		SetJoyKey(JoySet.LookRight, KeySet.LookLeft);
		SetJoyKey(JoySet.LookBehind, KeySet.LookRight);
		SetJoyKey(JoySet.LookLeft, KeySet.LookBehind);

		SetJoyKey(JoySet.CruiseControl, KeySet.CruiseControl);
		SetJoyKey(JoySet.Horn, KeySet.Horn);
		SetJoyKey(JoySet.ViewToggle, KeySet.ViewToggle);
		SetJoyKey(JoySet.DropCamera, KeySet.DropCamera);

		if (OldJoyIsAnalogue)
		{
			if (JoyAnalogueX > 96)
				KeyMap[KeySet.FastSteer] |= 0x80;

			if (JoyAnalogueX < -96)
				KeyMap[KeySet.FastSteer] |= 0x80;
		}
	}

	return 1;
}

// [0x4e56c9] [D] [A]
int SetJoystickDevice(int device)
{
	if (device >= 0 && device < num_input_devices)
	{
		JoyIsValid = 1;
		JoyIsFF = ff_devices[device];
		JoyDevice = input_devices[device];
		JoyHaptic = haptic_devices[device];
		JoyDirectionEffect = direction_effects[device];
		JoyMagnitudeEffect = magnitude_effects[device];
		current_joystick_device = device;
		return 1;
	}

	JoyIsValid = 0;
	JoyIsFF = 0;
	JoyDevice = NULL;
	JoyHaptic = NULL;
	JoyDirectionEffect = NO_EFFECT;
	JoyMagnitudeEffect = NO_EFFECT;
	current_joystick_device = 0;
	return 0;
}

// [0x4e5775] [D]
int GetJoystickDevice(void)
{
	return current_joystick_device;
}

// [0x4e577f] [D] [A]
// NOTE: IDA shows __fastcall with register junk; it takes no arguments
int SetJoystickDeadZone(void)
{
	if (InputDevice == INPUTDEVICE_KEYBOARD)
		return 0;

	if (JoyDeadZone >= 5)
		JoyDeadZone = 0;

	switch (JoyDeadZone)
	{
		case 0:
			dead_zones[current_joystick_device] = 1000;
			break;
		case 1:
			dead_zones[current_joystick_device] = 2000;
			break;
		case 2:
			dead_zones[current_joystick_device] = 3000;
			break;
		case 3:
			dead_zones[current_joystick_device] = 4000;
			break;
		default:
			dead_zones[current_joystick_device] = 5000;
			break;
	}

	return 1;
}

// [0x4e5876] [D]
// Was the key pressed this frame?
BOOL CheckKey(u_char key)
{
	return KeyMap[key] && !PrevKeyMap[key];
}

// [0x4e58ad] [D]
// Is any key down with the joystick bit (0x80) set?
int CheckJoyKey(void)
{
	int i;

	for (i = 0; i < 256; i++)
	{
		if (KeyMap[i] < 0)
			return 1;
	}

	return 0;
}

// [0x4e58f0] [D]
// NOTE: unreferenced (only its thunk is left); byte_BB10FC - byte_BB10FF are never set anywhere,
// a pressed / previous pair of flags by the look of it
BOOL sub_4E58F0(void)
{
	return byte_BB10FE && !byte_BB10FC;
}

// [0x4e5915] [D]
// NOTE: unreferenced (only its thunk is left)
BOOL sub_4E5915(void)
{
	return byte_BB10FF && !byte_BB10FD;
}

// [0x4e593a] [D]
// NOTE: unreferenced (only its thunk is left)
void sub_4E593A(void)
{
	byte_BB10FC = 0;
	byte_BB10FE = 0;
}

// [0x4e594d] [D]
// NOTE: unreferenced (only its thunk is left)
void sub_4E594D(void)
{
	byte_BB10FD = 0;
	byte_BB10FF = 0;
}

// [0x4e5960] [D]
// Was the joystick button (or stick up / down) pressed this frame?
int IsJoyKeyPressed(u_char key)
{
	// Up
	if (key == JOY_UP)
	{
		if (JoyAnalogueY < -63 && OldJoyAnalogueY >= -63)
			return 1;
	}
	// Down
	else if (key == JOY_DOWN)
	{
		if (JoyAnalogueY > 63 && OldJoyAnalogueY <= 63)
			return 1;
	}
	else if (key != JOY_NONE && JoyState.rgbButtons[key] && !BackupJoyState.rgbButtons[key])
	{
		return 1;
	}

	return 0;
}

// [0x4e59e8] [D]
// The car pad bits from the keys of KeySet (none while a replay plays).
void FillPad(int *pad)
{
	*pad = 0;

	moveup = 0;
	movedown = 0;

	cameraback = 0;
	cameraforward = 0;

	camerarotateleft = 0;
	camerarotateright = 0;

	if (ReplayMode == 2)
		return;

	if (KeyMap[KeySet.SteerLeft])
		*pad |= CAR_PAD_LEFT;

	if (KeyMap[KeySet.SteerRight])
		*pad |= CAR_PAD_RIGHT;

	if (KeyMap[KeySet.Accelerate])
		*pad |= CAR_PAD_ACCEL;

	if (KeyMap[KeySet.Brake])
		*pad |= CAR_PAD_BRAKE;

	if (KeyMap[KeySet.FastSteer])
		*pad |= CAR_PAD_FASTSTEER;

	if (KeyMap[KeySet.Handbrake])
		*pad |= CAR_PAD_HANDBRAKE;

	if (KeyMap[KeySet.Burnout])
		*pad |= CAR_PAD_WHEELSPIN;

	if (KeyMap[KeySet.CruiseControl])
		*pad |= CAR_PAD_CRUISECONTROL;

	if (KeyMap[KeySet.Horn])
		*pad |= CAR_PAD_HORN;
}

// [0x4e5b55] [D] [A]
// Does nothing in the original (the force feedback direction is set from steeringTorque in
// UpdateForces), and force feedback devices still ignore it.
// NOTE: [A] the callers pass the knocks of the player's car, which is what a rumble wants
int SetForceDirection(int x, int y)
{
	int magnitude;

	magnitude = (int)sqrt((double)x * x + (double)y * y);

	if (magnitude > JoyJolt)
		JoyJolt = magnitude;

	return 0;
}

// [0x4e5b5c] [D] [A]
int SetForceMagnitude(int magnitude)
{
	JoyForce = magnitude;
	UseJoyMagnitude = 1;

	return 1;
}

// [0x4e5b78] [D] [A]
// Sends the pending force feedback (steering torque, magnitude) to the joystick.
void UpdateForces(void)
{
	int magnitude;
	SDL_HapticEffect effect;

	if (steeringTorque && !GamePaused)
	{
		JoyDirectionX = steeringTorque;
		UseJoyDirection = 1;
	}

	if (!gForceFeedback || InputDevice != INPUTDEVICE_JOYSTICK)
	{
		JoyJolt = 0;
		return;
	}

	// [A] a pad with motors instead of force feedback
	if (!JoyHaptic)
	{
		if (JoyIsFF)
			UpdateRumble();

		return;
	}

	if (JoyDirectionEffect != NO_EFFECT && UseJoyDirection)
	{
		magnitude = (int)sqrt((double)JoyDirectionY * JoyDirectionY + (double)JoyDirectionX * JoyDirectionX);

		memset(&effect, 0, sizeof(effect));

		effect.type = SDL_HAPTIC_CONSTANT;
		effect.constant.direction.type = SDL_HAPTIC_CARTESIAN;
		effect.constant.direction.dir[0] = JoyDirectionX;
		effect.constant.direction.dir[1] = JoyDirectionY;
		effect.constant.length = DIRECTION_EFFECT_MS;
		effect.constant.level = ForceLevel(magnitude);

		SDL_HapticUpdateEffect(JoyHaptic, JoyDirectionEffect, &effect);
		SDL_HapticRunEffect(JoyHaptic, JoyDirectionEffect, 1);

		UseJoyDirection = 0;

		JoyDirectionX = 0;
		JoyDirectionY = 0;
	}

	if (JoyMagnitudeEffect != NO_EFFECT && UseJoyMagnitude)
	{
		memset(&effect, 0, sizeof(effect));

		effect.type = SDL_HAPTIC_CONSTANT;
		effect.constant.direction.type = SDL_HAPTIC_CARTESIAN;
		effect.constant.direction.dir[1] = 1;
		effect.constant.length = MAGNITUDE_EFFECT_MS;

		// NOTE: [A] the 75% was the effect's dwGain; SDL only has a gain per device
		effect.constant.level = ForceLevel(JoyForce * 3 / 4);

		SDL_HapticUpdateEffect(JoyHaptic, JoyMagnitudeEffect, &effect);
		SDL_HapticRunEffect(JoyHaptic, JoyMagnitudeEffect, 1);

		UseJoyMagnitude = 0;
	}
}

// [A]
// DirectInput force (-10000..10000, it saturates there) -> SDL level.
static int ForceLevel(int magnitude)
{
	if (magnitude > 10000)
		magnitude = 10000;
	else if (magnitude < -10000)
		magnitude = -10000;

	return magnitude * 32767 / 10000;
}

// [A]
// The low motor takes the steering torque and the knocks, the high one only the knocks. The length
// is that of the force feedback effect, so it stops by itself if the updates do.
static void UpdateRumble(void)
{
	int steer;
	int low;

	steer = 0;

	if (UseJoyDirection)
		steer = JoyDirectionX < 0 ? -JoyDirectionX : JoyDirectionX;

	UseJoyDirection = 0;
	UseJoyMagnitude = 0;
	JoyDirectionX = 0;
	JoyDirectionY = 0;

	if (steer > RUMBLE_STEER_MAX)
		steer = RUMBLE_STEER_MAX;

	low = JoyJolt > steer ? JoyJolt : steer;

	if (low || rumble_active)
		SDL_GameControllerRumble(JoyDevice, ForceLevel(low) * 2, ForceLevel(JoyJolt) * 2, DIRECTION_EFFECT_MS);

	rumble_active = low != 0;

	JoyJolt = JoyJolt * 3 / 4;

	if (JoyJolt < RUMBLE_JOLT_MIN)
		JoyJolt = 0;
}

// [A]
// A joystick axis as DirectInput gave it: -127..127, and the dead zone is taken off so that the
// rest of the travel still covers the whole range.
static int ReadJoyAxis(SDL_GameControllerAxis axis)
{
	int value;
	int size;
	int dead;

	value = SDL_GameControllerGetAxis(JoyDevice, axis);
	size = value < 0 ? -value : value;
	dead = dead_zones[current_joystick_device] * 32767 / 10000;

	if (size <= dead)
		return 0;

	if (size > 32767)
		size = 32767;

	size = (size - dead) * 127 / (32767 - dead);

	return value < 0 ? -size : size;
}

// [A]
static void ReadJoystickState(void)
{
	int i;

	JoyState.lX = ReadJoyAxis(SDL_CONTROLLER_AXIS_LEFTX);
	JoyState.lY = ReadJoyAxis(SDL_CONTROLLER_AXIS_LEFTY);

	for (i = 0; i < JOY_TRIGGER_LEFT; i++)
		JoyState.rgbButtons[i] = SDL_GameControllerGetButton(JoyDevice, JoyButtons[i]) ? 0x80 : 0;

	JoyState.rgbButtons[JOY_TRIGGER_LEFT] = SDL_GameControllerGetAxis(JoyDevice, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > TRIGGER_DOWN ? 0x80 : 0;
	JoyState.rgbButtons[JOY_TRIGGER_RIGHT] = SDL_GameControllerGetAxis(JoyDevice, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > TRIGGER_DOWN ? 0x80 : 0;
}

// [A]
// In menus every attached controller works, selected or not: A is Return, Y is Escape, B is
// Backspace, the d-pad and the left stick are the arrow keys. Start stands in for Escape to open
// and close the pause menu. The original only reads the selected joystick, with buttons 0 and 1 as
// Return and Escape, and has nothing to open the pause menu with.
// Any button or trigger at all sets ControllerDown and ControllerPressed, for what waits for a key
// (the splash screens, FMV).
// Whatever is held while the game or a button redefinition has the controller stays ignored until
// it is released: an accelerator button must not choose the first entry of the pause menu.
static void ReadMenuButtons(void)
{
	SDL_GameController *device;
	int start;
	int down;
	int any;
	int i;
	int j;

	start = 0;
	down = 0;
	any = 0;

	for (i = 0; i < num_input_devices; i++)
	{
		device = input_devices[i];

		for (j = 0; j < SDL_CONTROLLER_BUTTON_MAX; j++)
		{
			if (SDL_GameControllerGetButton(device, j))
				any = 1;
		}

		if (SDL_GameControllerGetAxis(device, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > TRIGGER_DOWN
			|| SDL_GameControllerGetAxis(device, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > TRIGGER_DOWN)
		{
			any = 1;
		}

		for (j = 0; j < sizeof(MenuButtons) / sizeof(MenuButtons[0]); j++)
		{
			if (SDL_GameControllerGetButton(device, MenuButtons[j].button))
				down |= 1 << MenuButtons[j].menukey;
		}

		if (SDL_GameControllerGetAxis(device, SDL_CONTROLLER_AXIS_LEFTX) < -MENU_STICK_DOWN)
			down |= 1 << MENU_LEFT;

		if (SDL_GameControllerGetAxis(device, SDL_CONTROLLER_AXIS_LEFTX) > MENU_STICK_DOWN)
			down |= 1 << MENU_RIGHT;

		if (SDL_GameControllerGetAxis(device, SDL_CONTROLLER_AXIS_LEFTY) < -MENU_STICK_DOWN)
			down |= 1 << MENU_UP;

		if (SDL_GameControllerGetAxis(device, SDL_CONTROLLER_AXIS_LEFTY) > MENU_STICK_DOWN)
			down |= 1 << MENU_DOWN;

		if (SDL_GameControllerGetButton(device, SDL_CONTROLLER_BUTTON_START))
			start = 1;
	}

	ControllerPressed = any && !ControllerDown;
	ControllerDown = any;

	if (start && (!gInPresentation || GamePaused))
		KeyMap[DIK_ESCAPE] |= 0x80;

	if (!gInPresentation || SettingJoyKey)
		menu_ignored = down;

	menu_ignored &= down;
	down &= ~menu_ignored;

	// a key is being redefined: any other button would become that key
	if (SettingKey)
		down &= 1 << MENU_ESCAPE;

	for (i = 0; i < MENU_KEYS; i++)
	{
		if (down & (1 << i))
			KeyMap[MenuKeys[i]] |= 0x80;
	}
}

// [0x4e5d1b] [D] [A]
// Sets up an attached controller (dead zone, force feedback). Was the EnumDevices callback
// DirectInput_EnumerateDevice.
static int OpenController(int index)
{
	SDL_GameController *device;
	SDL_Joystick *joystick;
	SDL_Haptic *haptic;
	const char *name;
	int ff;

	ff = 0;
	haptic = NULL;

	device = SDL_GameControllerOpen(index);

	if (!device)
		return 0;

	joystick = SDL_GameControllerGetJoystick(device);

	// 10% dead zone
	dead_zones[num_input_devices] = 1000;

	if (SDL_JoystickIsHaptic(joystick) > 0)
	{
		haptic = SDL_HapticOpenFromJoystick(joystick);

		if (haptic && (SDL_HapticQuery(haptic) & SDL_HAPTIC_CONSTANT))
		{
			if (CreateJoyDirectionEffect(haptic, num_input_devices))
				ff = 1;

			if (CreateJoyMagnitudeEffect(haptic, num_input_devices))
				ff = 1;
		}

		if (haptic && !ff)
		{
			SDL_HapticClose(haptic);
			haptic = NULL;
		}
	}

	// NOTE: [A] a pad that can only rumble counts as force feedback too, see UpdateRumble; the
	// original has none on them
	if (!ff && SDL_GameControllerHasRumble(device))
		ff = 1;

	input_devices[num_input_devices] = device;
	device_ids[num_input_devices] = SDL_JoystickInstanceID(joystick);
	haptic_devices[num_input_devices] = haptic;
	ff_devices[num_input_devices] = ff;

	name = SDL_GameControllerName(device);

	if (!name)
		name = "";

	if (strlen(name) < 32)
	{
		strcpy(input_device_names[num_input_devices], name);
	}
	else
	{
		input_device_names[num_input_devices][0] = 0;
		strncat(input_device_names[num_input_devices], name, 28);
		input_device_names[num_input_devices][28] = '.';
		input_device_names[num_input_devices][29] = '.';
		input_device_names[num_input_devices][30] = '.';
		input_device_names[num_input_devices][31] = 0;
	}

	num_input_devices++;

	return 1;
}

// [A]
// SDL_CONTROLLERDEVICEADDED; index is a device index. A controller stays out of the way of the
// keyboard until it is selected in the menu, unless it replaces the one that was removed.
void ControllerAdded(int index)
{
	SDL_JoystickID id;
	char message[128];
	int i;

	if (!input_started)
		return;

	id = SDL_JoystickGetDeviceInstanceID(index);

	for (i = 0; i < num_input_devices; i++)
	{
		if (device_ids[i] == id)
			return;
	}

	if (!SDL_IsGameController(index))
	{
		sprintf(message, "Input: no controller mapping for %.64s", SDL_JoystickNameForIndex(index));
		LogQuick(message);
		return;
	}

	if (num_input_devices >= MAX_INPUT_DEVICES)
		return;

	if (!OpenController(index))
	{
		sprintf(message, "Input: can't open controller: %.64s", SDL_GetError());
		LogQuick(message);
		return;
	}

	sprintf(message, "Input: controller %d: %s", num_input_devices, input_device_names[num_input_devices - 1]);
	LogQuick(message);

	if (controller_lost)
	{
		controller_lost = 0;

		SetJoystickDevice(num_input_devices - 1);

		InputDevice = INPUTDEVICE_JOYSTICK;
		SetJoystickDeadZone();
	}
}

// [A]
// SDL_CONTROLLERDEVICEREMOVED; id is an instance id. The list stays without holes, as the frontend
// counts through it. Losing the selected controller goes back to the keyboard.
void ControllerRemoved(int id)
{
	char message[128];
	int selected;
	int removed;
	int i;

	for (removed = 0; removed < num_input_devices; removed++)
	{
		if (device_ids[removed] == id)
			break;
	}

	if (removed == num_input_devices)
		return;

	sprintf(message, "Input: controller removed: %s", input_device_names[removed]);
	LogQuick(message);

	selected = JoyIsValid ? current_joystick_device : -1;

	if (haptic_devices[removed])
		SDL_HapticClose(haptic_devices[removed]);

	SDL_GameControllerClose(input_devices[removed]);

	num_input_devices--;

	for (i = removed; i < num_input_devices; i++)
	{
		input_devices[i] = input_devices[i + 1];
		device_ids[i] = device_ids[i + 1];
		haptic_devices[i] = haptic_devices[i + 1];
		dead_zones[i] = dead_zones[i + 1];
		ff_devices[i] = ff_devices[i + 1];
		direction_effects[i] = direction_effects[i + 1];
		magnitude_effects[i] = magnitude_effects[i + 1];
		strcpy(input_device_names[i], input_device_names[i + 1]);
	}

	input_devices[num_input_devices] = NULL;
	haptic_devices[num_input_devices] = NULL;
	direction_effects[num_input_devices] = NO_EFFECT;
	magnitude_effects[num_input_devices] = NO_EFFECT;

	if (selected == removed)
	{
		SetJoystickDevice(-1);

		memset(&JoyState, 0, sizeof(JoyState));
		memset(&BackupJoyState, 0, sizeof(BackupJoyState));
		JoyAnalogueX = 0;
		JoyAnalogueY = 0;

		if (InputDevice == INPUTDEVICE_JOYSTICK)
		{
			InputDevice = INPUTDEVICE_KEYBOARD;
			controller_lost = 1;
		}
	}
	else if (selected > removed)
	{
		SetJoystickDevice(selected - 1);
	}
}

// [0x4e6250] [D] [A]
int ReadKeyboardInput(void)
{
	static int alt_enter;	// NOTE: [A]

	const Uint8 *state;
	int i;

	CopyMem(PrevKeyMap, KeyMap, sizeof(KeyMap));

	// NOTE: [A] DirectInput doesn't need the message pump, and some loops only call ReadInput
	SDL_PumpEvents();

	state = SDL_GetKeyboardState(NULL);

	memset(KeyMap, 0, sizeof(KeyMap));

	for (i = 0; i < sizeof(ScancodeToKey) / sizeof(ScancodeToKey[0]); i++)
	{
		if (state[ScancodeToKey[i].scancode])
			KeyMap[ScancodeToKey[i].key] = 0x80;
	}

	// NOTE: [A] Alt-Enter is for the window, see HandleWinMessages. Enter stays hidden from the
	// game until it is released, Alt may go first.
	if (!state[SDL_SCANCODE_RETURN])
		alt_enter = 0;
	else if (gUseGL && (state[SDL_SCANCODE_LALT] || state[SDL_SCANCODE_RALT]))
		alt_enter = 1;

	if (alt_enter)
		KeyMap[DIK_RETURN] = 0;

	return 1;
}

// [0x4e62a6] [D] [A]
static int CreateJoyDirectionEffect(SDL_Haptic *device, int index)
{
	SDL_HapticEffect effect;

	memset(&effect, 0, sizeof(effect));

	effect.type = SDL_HAPTIC_CONSTANT;
	effect.constant.direction.type = SDL_HAPTIC_CARTESIAN;
	effect.constant.direction.dir[0] = 1;		// NOTE: 0, 0 in the original; replaced on every update
	effect.constant.length = DIRECTION_EFFECT_MS;

	direction_effects[index] = SDL_HapticNewEffect(device, &effect);

	return direction_effects[index] != NO_EFFECT;
}

// [0x4e636c] [D] [A]
// NOTE: IDA shows a __usercall with a garbage ebx argument; the real arguments are (device, index)
static int CreateJoyMagnitudeEffect(SDL_Haptic *device, int index)
{
	SDL_HapticEffect effect;

	memset(&effect, 0, sizeof(effect));

	effect.type = SDL_HAPTIC_CONSTANT;
	effect.constant.direction.type = SDL_HAPTIC_CARTESIAN;
	effect.constant.direction.dir[1] = 1;		// the original's single axis is Y
	effect.constant.length = MAGNITUDE_EFFECT_MS;
	effect.constant.level = ForceLevel(5000 * 3 / 4);

	magnitude_effects[index] = SDL_HapticNewEffect(device, &effect);

	return magnitude_effects[index] != NO_EFFECT;
}

// [0x4e6488] [D]
// NOTE: unnamed in IDA (sub_4E6488); name guessed. Copies a joystick button (or stick up /
// down) onto a key of KeyMap.
static void SetJoyKey(u_char joykey, u_char key)
{
	switch (joykey)
	{
		// Up
		case JOY_UP:
			if (JoyAnalogueY < -63)
				KeyMap[key] = 0x80;
			else
				KeyMap[key] = 0;
			break;

		// Down
		case JOY_DOWN:
			if (JoyAnalogueY > 63)
				KeyMap[key] = 0x80;
			else
				KeyMap[key] = 0;
			break;

		case JOY_NONE:
			break;

		// Button
		default:
			KeyMap[key] = JoyState.rgbButtons[joykey];
			break;
	}
}
