/*
 * input.c - DirectInput keyboard, mouse and joystick (with force feedback)
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "input.c"); partially translated.
 */

#define DIRECTINPUT_VERSION 0x0500

#include <windows.h>
#include <string.h>
#include <math.h>
#include <dinput.h>

#include "dr_types.h"
#include "dr_globals.h"
#include "frontend/gvars.h"
#include "handling.h"
#include "input.h"
#include "shell.h"
#include "asmmath.h"
#include "debug.h"
#include "camera.h"
#include "replays.h"

#define MAX_INPUT_DEVICES	8

// DIAcquire / DIReAcquire device types
#define DI_KEYBOARD			0
#define DI_MOUSE			1
#define DI_JOYSTICK			2

// joystick "buttons" beyond the real ones (frontend\gvars.c DIJ_ButtonLookup: "Up", "Down", "None")
#define JOY_UP				32
#define JOY_DOWN			33
#define JOY_NONE			34

#define REPLAY_PLAYBACK		2	// ReplayMode: the replay drives the cars

void ShutdownDevices(void);
int SetJoystickDevice(int device);
void UpdateForces(void);
int IsJoyKeyPressed(u_char key);
BOOL DIAcquire(int type);
BOOL DIReAcquire(int type);
int ReadKeyboardInput(void);
BOOL CALLBACK DirectInput_EnumerateDevice(LPCDIDEVICEINSTANCEA lpddi, LPVOID pvRef);
BOOL CreateJoyDirectionEffect(LPDIRECTINPUTDEVICE2A device, int index);
BOOL CreateJoyMagnitudeEffect(LPDIRECTINPUTDEVICE2A device, int index);
BOOL CALLBACK EnumInputEffect(LPCDIEFFECTINFOA pdei, LPVOID pvRef);
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

LPDIRECTINPUTEFFECT magnitude_effects[MAX_INPUT_DEVICES];
int ff_devices[MAX_INPUT_DEVICES];
LPDIRECTINPUTEFFECT direction_effects[MAX_INPUT_DEVICES];
LPDIRECTINPUTDEVICE2A input_devices[MAX_INPUT_DEVICES];
DIJOYSTATE BackupJoyState;
int valid_devices[MAX_INPUT_DEVICES];
DIJOYSTATE JoyState;
LPDIRECTINPUTA lpDirectInput;
LPDIRECTINPUTDEVICEA keyboard_device;
LPDIRECTINPUTDEVICEA mouse_device;
LPDIRECTINPUTDEVICE2A JoyDevice;
LPDIRECTINPUTEFFECT JoyDirectionEffect;
LPDIRECTINPUTEFFECT JoyMagnitudeEffect;
HANDLE MouseEvent;
int OldJoyAnalogueX;
int OldJoyAnalogueY;
DICONSTANTFORCE JoyForce;
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

// [A]
// Current Windows SDKs have no dinput.lib (the DirectInput 5 import library): take
// DirectInputCreateA from dinput.dll at run time.
HRESULT WINAPI DirectInputCreateA(HINSTANCE hinst, DWORD dwVersion, LPDIRECTINPUTA *ppDI, LPUNKNOWN punkOuter)
{
	static HRESULT (WINAPI *create)(HINSTANCE, DWORD, LPDIRECTINPUTA *, LPUNKNOWN);
	HMODULE dll;

	if (!create)
	{
		dll = LoadLibrary("dinput.dll");

		if (dll)
			create = (HRESULT (WINAPI *)(HINSTANCE, DWORD, LPDIRECTINPUTA *, LPUNKNOWN))GetProcAddress(dll, "DirectInputCreateA");

		if (!create)
			return DIERR_GENERIC;
	}

	return create(hinst, dwVersion, ppDI, punkOuter);
}

// [0x4e4e40] [D] [A]
// NOTE: IDA shows __fastcall with register junk; it takes no arguments
int InitDirectInput(void)
{
	HRESULT res;
	DIPROPDWORD dipdw;

	if (lpDirectInput)
		ShutdownDevices();

	if (DirectInputCreateA(gScreen.module, DIRECTINPUT_VERSION, &lpDirectInput, NULL) != DI_OK)
		return 0;

	if (lpDirectInput->lpVtbl->CreateDevice(lpDirectInput, &GUID_SysKeyboard, &keyboard_device, NULL) != DI_OK)
		goto fail;

	if (lpDirectInput->lpVtbl->CreateDevice(lpDirectInput, &GUID_SysMouse, &mouse_device, NULL) != DI_OK)
		goto fail;

	if (keyboard_device->lpVtbl->SetDataFormat(keyboard_device, &c_dfDIKeyboard) != DI_OK)
		goto fail;

	if (mouse_device->lpVtbl->SetDataFormat(mouse_device, &c_dfDIMouse) != DI_OK)
		goto fail;

	if (keyboard_device->lpVtbl->SetCooperativeLevel(keyboard_device, gScreen.handle, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND) != DI_OK)
		goto fail;

	if (gShowMouse)
		res = mouse_device->lpVtbl->SetCooperativeLevel(mouse_device, gScreen.handle, DISCL_EXCLUSIVE | DISCL_FOREGROUND);
	else
		res = mouse_device->lpVtbl->SetCooperativeLevel(mouse_device, gScreen.handle, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);

	if (res != DI_OK)
		goto fail;

	// NOTE: the original fails here (and frees the devices) when the window is not in the
	// foreground, then ReadKeyboardInput crashes on a NULL keyboard_device; ReadKeyboardInput
	// re-acquires it once the window gets focus
	DIAcquire(DI_KEYBOARD);

	InputDevice = INPUTDEVICE_KEYBOARD;
	KeySet = DefaultKeySet;
	memcpy(&JoySet, &DefaultJoySet, sizeof(JoySet));

	MouseEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	if (!MouseEvent)
		goto fail;

	if (mouse_device->lpVtbl->SetEventNotification(mouse_device, MouseEvent) != DI_OK)
		goto fail;

	dipdw.diph.dwSize = sizeof(DIPROPDWORD);
	dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	dipdw.diph.dwObj = 0;
	dipdw.diph.dwHow = DIPH_DEVICE;
	dipdw.dwData = 32;

	if (mouse_device->lpVtbl->SetProperty(mouse_device, DIPROP_BUFFERSIZE, &dipdw.diph) != DI_OK)
		goto fail;

	// NOTE: not fatal, same as the keyboard above
	mouse_device->lpVtbl->Acquire(mouse_device);

	JoyIsValid = 0;
	num_input_devices = 0;

	if (use_joysticks)
	{
		lpDirectInput->lpVtbl->EnumDevices(lpDirectInput, DIDEVTYPE_JOYSTICK, DirectInput_EnumerateDevice, lpDirectInput, DIEDFL_ATTACHEDONLY);

		if (num_input_devices > 0)
			SetJoystickDevice(0);
	}

	return 1;

fail:
	ShutdownDevices();
	return 0;
}

// [0x4e5135] [D]
void ShutdownDevices(void)
{
	int i;

	if (MouseEvent)
	{
		CloseHandle(MouseEvent);
		MouseEvent = NULL;
	}

	if (keyboard_device)
	{
		keyboard_device->lpVtbl->Unacquire(keyboard_device);
		keyboard_device->lpVtbl->Release(keyboard_device);
		keyboard_device = NULL;
	}

	if (mouse_device)
	{
		mouse_device->lpVtbl->Unacquire(mouse_device);
		mouse_device->lpVtbl->Release(mouse_device);
		mouse_device = NULL;
	}

	for (i = 0; i < MAX_INPUT_DEVICES; i++)
	{
		if (direction_effects[i])
		{
			direction_effects[i]->lpVtbl->Release(direction_effects[i]);
			direction_effects[i] = NULL;
		}

		if (magnitude_effects[i])
		{
			magnitude_effects[i]->lpVtbl->Release(magnitude_effects[i]);
			magnitude_effects[i] = NULL;
		}

		if (input_devices[i])
		{
			input_devices[i]->lpVtbl->Unacquire(input_devices[i]);
			input_devices[i]->lpVtbl->Release(input_devices[i]);
			input_devices[i] = NULL;
		}
	}

	JoyDevice = NULL;
	JoyDirectionEffect = NULL;
	JoyMagnitudeEffect = NULL;

	if (lpDirectInput)
	{
		lpDirectInput->lpVtbl->Release(lpDirectInput);
		lpDirectInput = NULL;
	}

	JoyIsValid = 0;
}

// [0x4e52f1] [D]
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

		return ReadKeyboardInput();
	}

	if (InputDevice != INPUTDEVICE_JOYSTICK)
		return 0;

	if (!use_joysticks)
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

	if (!gInPresentation && ReplayMode == REPLAY_PLAYBACK)
		return 1;

	if ((unsigned int)JoyDevice->lpVtbl->Poll(JoyDevice) > DI_NOEFFECT ||
		JoyDevice->lpVtbl->GetDeviceState(JoyDevice, sizeof(DIJOYSTATE), &JoyState) != DI_OK)
	{
		DIReAcquire(DI_JOYSTICK);
		return 0;
	}

	JoyAnalogueX = JoyState.lX;
	JoyAnalogueY = JoyState.lY;

	if (gInPresentation)
	{
		// menus: buttons 0 and 1 are Return and Escape, the stick works as the arrow keys
		if (!SettingJoyKey)
		{
			if (IsJoyKeyPressed(0))
				KeyMap[DIK_RETURN] |= 0x80;

			if (IsJoyKeyPressed(1))
				KeyMap[DIK_ESCAPE] |= 0x80;

			if (JoyAnalogueX < -31)
				KeyMap[DIK_LEFTARROW] |= 0x80;

			if (JoyAnalogueX > 31)
				KeyMap[DIK_RIGHTARROW] |= 0x80;

			if (JoyAnalogueY < -31)
				KeyMap[DIK_UPARROW] |= 0x80;

			if (JoyAnalogueY > 31)
				KeyMap[DIK_DOWNARROW] |= 0x80;
		}
	}
	else
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

// [0x4e56c9] [D]
int SetJoystickDevice(int device)
{
	if (device >= 0 && device < num_input_devices)
	{
		JoyIsValid = 1;
		JoyIsFF = ff_devices[device];
		JoyDevice = input_devices[device];
		JoyDirectionEffect = direction_effects[device];
		JoyMagnitudeEffect = magnitude_effects[device];
		current_joystick_device = device;
		return 1;
	}

	JoyIsValid = 0;
	JoyIsFF = 0;
	JoyDevice = NULL;
	JoyDirectionEffect = NULL;
	JoyMagnitudeEffect = NULL;
	current_joystick_device = 0;
	return 0;
}

// [0x4e5775] [D]
int GetJoystickDevice(void)
{
	return current_joystick_device;
}

// [0x4e577f] [D]
// NOTE: IDA shows __fastcall with register junk; it takes no arguments
int SetJoystickDeadZone(void)
{
	DIPROPDWORD dipdw;

	if (InputDevice == INPUTDEVICE_KEYBOARD)
		return 0;

	if (JoyDeadZone >= 5)
		JoyDeadZone = 0;

	dipdw.diph.dwSize = sizeof(DIPROPDWORD);
	dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	dipdw.diph.dwObj = DIJOFS_X;
	dipdw.diph.dwHow = DIPH_BYOFFSET;

	switch (JoyDeadZone)
	{
		case 0:
			dipdw.dwData = 1000;
			break;
		case 1:
			dipdw.dwData = 2000;
			break;
		case 2:
			dipdw.dwData = 3000;
			break;
		case 3:
			dipdw.dwData = 4000;
			break;
		default:
			dipdw.dwData = 5000;
			break;
	}

	if (JoyDevice->lpVtbl->SetProperty(JoyDevice, DIPROP_DEADZONE, &dipdw.diph) != DI_OK)
		return 0;

	dipdw.diph.dwObj = DIJOFS_Y;

	if (JoyDevice->lpVtbl->SetProperty(JoyDevice, DIPROP_DEADZONE, &dipdw.diph) != DI_OK)
		return 0;

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

// [0x4e5b55] [D]
// Does nothing (the force feedback direction is set from steeringTorque in UpdateForces).
int SetForceDirection(int x, int y)
{
	return 0;
}

// [0x4e5b5c] [D]
int SetForceMagnitude(LONG magnitude)
{
	JoyForce.lMagnitude = magnitude;
	UseJoyMagnitude = 1;

	return 1;
}

// [0x4e5b78] [D]
// Sends the pending force feedback (steering torque, magnitude) to the joystick.
void UpdateForces(void)
{
	LONG direction[2];
	LONG magnitude;
	DWORD axis;
	DIEFFECT effect;

	if (steeringTorque && !GamePaused)
	{
		JoyDirectionX = steeringTorque;
		UseJoyDirection = 1;
	}

	if (!gForceFeedback || InputDevice != INPUTDEVICE_JOYSTICK)
		return;

	if (JoyDirectionEffect && UseJoyDirection)
	{
		direction[0] = JoyDirectionX;
		direction[1] = JoyDirectionY;

		magnitude = (LONG)sqrt((double)(JoyDirectionY * JoyDirectionY + JoyDirectionX * JoyDirectionX));

		memset(&effect, 0, sizeof(effect));

		effect.dwSize = sizeof(DIEFFECT);
		effect.dwFlags = DIEFF_OBJECTOFFSETS | DIEFF_CARTESIAN;
		effect.cAxes = 2;
		effect.rglDirection = direction;
		effect.lpEnvelope = NULL;
		effect.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
		effect.lpvTypeSpecificParams = &magnitude;

		JoyDirectionEffect->lpVtbl->SetParameters(JoyDirectionEffect, &effect, DIEP_DIRECTION | DIEP_TYPESPECIFICPARAMS | DIEP_START);

		UseJoyDirection = 0;

		JoyDirectionX = 0;
		JoyDirectionY = 0;
	}

	if (JoyMagnitudeEffect && UseJoyMagnitude)
	{
		magnitude = JoyForce.lMagnitude;
		axis = DIJOFS_Y;

		memset(&effect, 0, sizeof(effect));

		effect.dwSize = sizeof(DIEFFECT);
		effect.rgdwAxes = &axis;
		effect.dwFlags = DIEFF_OBJECTOFFSETS | DIEFF_CARTESIAN;
		effect.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
		effect.lpvTypeSpecificParams = &magnitude;

		JoyMagnitudeEffect->lpVtbl->SetParameters(JoyMagnitudeEffect, &effect, DIEP_TYPESPECIFICPARAMS | DIEP_START);

		UseJoyMagnitude = 0;
	}
}

// [0x4e5d1b] [D]
// EnumDevices callback: sets up each attached joystick (range, dead zone, force feedback).
// NOTE: IDA shows __fastcall with register junk; it is a plain DirectInput callback
BOOL CALLBACK DirectInput_EnumerateDevice(LPCDIDEVICEINSTANCEA lpddi, LPVOID pvRef)
{
	LPDIRECTINPUTA di;
	LPDIRECTINPUTDEVICEA device1;
	LPDIRECTINPUTDEVICE2A device;
	DIPROPRANGE diprg;
	DIPROPDWORD dipdw;
	DIDEVCAPS caps;
	DWORD subtype;
	BOOL valid;
	int ff;

	di = (LPDIRECTINPUTA)pvRef;

	valid = FALSE;
	ff = 0;

	if (di->lpVtbl->CreateDevice(di, &lpddi->guidInstance, &device1, NULL) != DI_OK)
		return DIENUM_CONTINUE;

	if (device1->lpVtbl->QueryInterface(device1, &IID_IDirectInputDevice2A, (LPVOID *)&device) != DI_OK)
	{
		device1->lpVtbl->Release(device1);
		return DIENUM_CONTINUE;
	}

	device1->lpVtbl->Release(device1);
	device1 = NULL;

	if (device->lpVtbl->SetDataFormat(device, &c_dfDIJoystick) != DI_OK)
	{
		device->lpVtbl->Release(device);
		return DIENUM_CONTINUE;
	}

	if (device->lpVtbl->SetCooperativeLevel(device, gScreen.handle, DISCL_EXCLUSIVE | DISCL_FOREGROUND) != DI_OK)
	{
		device->lpVtbl->Release(device);
		return DIENUM_CONTINUE;
	}

	// axes range -127..127
	diprg.diph.dwSize = sizeof(DIPROPRANGE);
	diprg.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	diprg.diph.dwObj = DIJOFS_X;
	diprg.diph.dwHow = DIPH_BYOFFSET;
	diprg.lMin = -127;
	diprg.lMax = 127;

	if (device->lpVtbl->SetProperty(device, DIPROP_RANGE, &diprg.diph) != DI_OK)
	{
		device->lpVtbl->Release(device);
		return DIENUM_CONTINUE;
	}

	diprg.diph.dwObj = DIJOFS_Y;

	if (device->lpVtbl->SetProperty(device, DIPROP_RANGE, &diprg.diph) != DI_OK)
	{
		device->lpVtbl->Release(device);
		return DIENUM_CONTINUE;
	}

	// 10% dead zone
	dipdw.diph.dwSize = sizeof(DIPROPDWORD);
	dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	dipdw.diph.dwObj = DIJOFS_X;
	dipdw.diph.dwHow = DIPH_BYOFFSET;
	dipdw.dwData = 1000;

	if (device->lpVtbl->SetProperty(device, DIPROP_DEADZONE, &dipdw.diph) != DI_OK)
	{
		device->lpVtbl->Release(device);
		return DIENUM_CONTINUE;
	}

	dipdw.diph.dwObj = DIJOFS_Y;

	if (device->lpVtbl->SetProperty(device, DIPROP_DEADZONE, &dipdw.diph) != DI_OK)
	{
		device->lpVtbl->Release(device);
		return DIENUM_CONTINUE;
	}

	JoyDevice = device;

	if (!DIAcquire(DI_JOYSTICK))
	{
		device->lpVtbl->Release(device);
		return DIENUM_CONTINUE;
	}

	subtype = (lpddi->dwDevType & 0xFF00) >> 8;
	valid = (subtype != DIDEVTYPEJOYSTICK_GAMEPAD);

	memset(&caps, 0, sizeof(caps));
	caps.dwSize = sizeof(DIDEVCAPS);

	if (device->lpVtbl->GetCapabilities(device, &caps) != DI_OK)
	{
		device->lpVtbl->Release(device);
		return DIENUM_CONTINUE;
	}

	if (caps.dwFlags & DIDC_FORCEFEEDBACK)
	{
		if (CreateJoyDirectionEffect(device, num_input_devices))
			ff = 1;

		if (CreateJoyMagnitudeEffect(device, num_input_devices))
			ff = 1;
	}

	input_devices[num_input_devices] = device;
	valid_devices[num_input_devices] = valid;
	ff_devices[num_input_devices] = ff;

	if (strlen(lpddi->tszInstanceName) < 32)
	{
		strcpy(input_device_names[num_input_devices], lpddi->tszInstanceName);
	}
	else
	{
		strncat(input_device_names[num_input_devices], lpddi->tszInstanceName, 28);
		input_device_names[num_input_devices][28] = '.';
		input_device_names[num_input_devices][29] = '.';
		input_device_names[num_input_devices][30] = '.';
		input_device_names[num_input_devices][31] = 0;
	}

	return ++num_input_devices < MAX_INPUT_DEVICES;
}

// [0x4e60cf] [D]
BOOL DIAcquire(int type)
{
	HRESULT res;

	if (type == DI_KEYBOARD)
		res = keyboard_device->lpVtbl->Acquire(keyboard_device);
	else if (type == DI_MOUSE)
		res = mouse_device->lpVtbl->Acquire(mouse_device);
	else if (type == DI_JOYSTICK)
		res = JoyDevice->lpVtbl->Acquire(JoyDevice);
	else
		return FALSE;

	return res == DI_OK;
}

// [0x4e6147] [D]
BOOL DIReAcquire(int type)
{
	HRESULT res;

	if (type == DI_KEYBOARD)
	{
		if (keyboard_device->lpVtbl->SetDataFormat(keyboard_device, &c_dfDIKeyboard) != DI_OK)
			LogQuick("DIReAcquire: SetDataFormat failed");

		res = keyboard_device->lpVtbl->Acquire(keyboard_device);
	}
	else if (type == DI_MOUSE)
	{
		if (mouse_device->lpVtbl->SetDataFormat(mouse_device, &c_dfDIMouse) != DI_OK)
			LogQuick("DIReAcquire: SetDataFormat failed");

		res = mouse_device->lpVtbl->Acquire(mouse_device);
	}
	else if (type == DI_JOYSTICK)
	{
		if (JoyDevice->lpVtbl->SetDataFormat(JoyDevice, &c_dfDIJoystick) != DI_OK)
			LogQuick("DIReAcquire: SetDataFormat failed");

		res = JoyDevice->lpVtbl->Acquire(JoyDevice);
	}
	else
	{
		return FALSE;
	}

	return res == DI_OK;
}

// [0x4e6250] [D] [A]
int ReadKeyboardInput(void)
{
	CopyMem(PrevKeyMap, KeyMap, sizeof(KeyMap));

	// NOTE: no device when InitDirectInput failed; the original crashes here
	if (!keyboard_device)
		return 0;

	if (keyboard_device->lpVtbl->GetDeviceState(keyboard_device, sizeof(KeyMap), KeyMap) == DI_OK)
		return 1;

	DIReAcquire(DI_KEYBOARD);
	return 0;
}

// [0x4e62a6] [D]
BOOL CreateJoyDirectionEffect(LPDIRECTINPUTDEVICE2A device, int index)
{
	DWORD axes[2];
	LONG direction[2];
	DICONSTANTFORCE force;
	DIEFFECT effect;

	axes[0] = DIJOFS_X;
	axes[1] = DIJOFS_Y;
	direction[0] = 0;
	direction[1] = 0;
	force.lMagnitude = 0;

	if (JoyDirectionEffect)
		return TRUE;

	effect.dwSize = sizeof(DIEFFECT);
	effect.dwFlags = DIEFF_OBJECTOFFSETS | DIEFF_CARTESIAN;
	effect.dwDuration = 250000;
	effect.dwSamplePeriod = 0;
	effect.dwGain = DI_FFNOMINALMAX;
	effect.dwTriggerButton = DIEB_NOTRIGGER;
	effect.dwTriggerRepeatInterval = 0;
	effect.cAxes = 2;
	effect.rgdwAxes = axes;
	effect.rglDirection = direction;
	effect.lpEnvelope = NULL;
	effect.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
	effect.lpvTypeSpecificParams = &force;

	return device->lpVtbl->CreateEffect(device, &GUID_ConstantForce, &effect, &direction_effects[index], NULL) == DI_OK;
}

// [0x4e636c] [D]
// NOTE: IDA shows a __usercall with a garbage ebx argument; the real arguments are (device, index)
BOOL CreateJoyMagnitudeEffect(LPDIRECTINPUTDEVICE2A device, int index)
{
	GUID guid;
	LONG direction;
	DWORD axis;
	DICONSTANTFORCE force;
	DIEFFECT effect;

	if (JoyMagnitudeEffect)
		return TRUE;

	if (device->lpVtbl->EnumEffects(device, EnumInputEffect, &guid, DIEFT_CONSTANTFORCE) != DI_OK)
		return FALSE;

	force.lMagnitude = 5000;
	axis = DIJOFS_Y;
	direction = 1;

	effect.dwSize = sizeof(DIEFFECT);
	effect.dwSamplePeriod = 0;
	effect.dwTriggerButton = DIEB_NOTRIGGER;
	effect.dwTriggerRepeatInterval = 0;
	effect.rgdwAxes = &axis;
	effect.rglDirection = &direction;
	effect.dwGain = 7500;
	effect.dwFlags = DIEFF_OBJECTOFFSETS | DIEFF_CARTESIAN;
	effect.dwDuration = 15000;
	effect.cAxes = 1;
	effect.lpEnvelope = NULL;
	effect.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
	effect.lpvTypeSpecificParams = &force;

	return device->lpVtbl->CreateEffect(device, &guid, &effect, &magnitude_effects[index], NULL) == DI_OK;
}

// [0x4e644a] [D]
// EnumEffects callback: takes the first effect's GUID.
BOOL CALLBACK EnumInputEffect(LPCDIEFFECTINFOA pdei, LPVOID pvRef)
{
	if (pvRef)
		*(GUID *)pvRef = pdei->guid;

	return DIENUM_STOP;
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
