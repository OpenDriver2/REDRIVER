/*
 * input.h - globals and functions of input.c used by other files
 */

#ifndef INPUT_H
#define INPUT_H

#include "dr_types.h"

// globals
extern char InputDevice;
extern int JoyAnalogueX;
extern int JoyAnalogueY;
extern int JoyIsAnalogue;
extern int JoyIsFF;
extern int OldJoyIsAnalogue;
extern int JoyDeadZone;
extern CONTROL_JOY_SET JoySet;
extern char input_device_names[8][32];
extern char KeyMap[256];
extern char PrevKeyMap[256];
extern CONTROL_KEY_SET KeySet;
extern char movedown;
extern char moveup;

// functions
extern BOOL CheckKey(u_char key);
extern void FillPad(int *pad);
extern int GetJoystickDevice(void);
extern int InitDirectInput(void);
extern int IsJoyKeyPressed(u_char key);
extern int ReadInput(void);
extern int SetForceDirection(int x, int y);
extern int SetJoystickDeadZone(void);
extern int SetJoystickDevice(int device);
extern void ShutdownDevices(void);
extern void UpdateForces(void);

// InputDevice
#define INPUTDEVICE_KEYBOARD		0
#define INPUTDEVICE_JOYSTICK		2

#endif // INPUT_H
