/*
 * event.c - scripted events (moving level objects: boats, trains, barriers ...)
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "event.c", 0x4d0ce0 - 0x4d3669).
 *
 * compiler.c builds the EVENT list from the level's event script. An event starts (flags bit 1)
 * when the player gets within its radius, enters its cell (bit 2) or another event triggers its
 * id (bit 3). While running, each of its models executes its command list (ExecuteEvent, one
 * handler per script command) until a handler returns 0: the handler that waits leaves commands
 * on itself and the model goes on from it next frame (commands_end).
 *
 * EVENT_MODEL flags: 1 move to direction, 2 target the player, 4 destroyed, 0x10 turn to the
 * target, 0x20 tilt along the ground, 0x40 move along rotation.vy at velocity, 0x80 target
 * reached, 0x100 speed curve / turn started, 0x200 collisions off.
 * EVENT flags: 2 running, 4 cell trigger, 8 triggered by id, 0x10 has clones, 0x20 a clone.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "dr_types.h"
#include "event.h"
#include "ai.h"
#include "asmmath.h"
#include "debris.h"
#include "dr_globals.h"
#include "general.h"
#include "handling.h"
#include "map.h"
#include "pause.h"
#include "shell.h"
#include "testmath.h"
#include "Engine/3dfx/gfxinit.h"
#include "Engine/3dfx/object.h"
#include "Engine/3dfx/polydraw.h"
#include "camera.h"
#include "glaunch.h"

void DoEvent(void);
void NextEvent(void);
void RotateEventModel(EVENT_MODEL *em, IVECTOR *dir);
int EventNull(void);
int EventSetModel(void);
int EventSetRotation(void);
int EventModelTarget(void);
int EventPlayerTarget(void);
int EventModelMoveto(void);
int EventModelMove(void);
int EventModelStop(void);
int EventModelVelocity(void);
int EventModelSlow(void);
int EventModelSpeed(void);
int EventModelWait(void);
int EventModelDestroy(void);
int EventTerminate(void);
int EventSendMessage(void);
int EventAddModel(void);
int EventLoop(void);
int EventLoopWithCount(void);
int EventTrigger(void);
int EventPlaySound(void);
int EventStopBox(void);
int EventTurnToTarget(void);
int EventWaitForTarget(void);
int EventTriggerNext(void);
int EventModelTurn(void);
int EventModelSpeedCurve(void);
int EventModelRotate(void);
int EventCollisionsOff(void);

// [0x561fb0] the command handlers by script command number; 0 stops the model for this frame
// NOTE: no @file mark; DoEvent is its only user
int (*ExecuteEvent[33])(void) = {
	EventNull,
	EventNull,
	EventNull,
	EventNull,
	EventSetModel,
	EventSetRotation,
	EventModelTarget,
	EventPlayerTarget,
	EventModelMoveto,
	EventModelMove,
	EventModelStop,
	EventModelVelocity,
	EventModelSlow,
	EventModelSpeed,
	EventModelWait,
	EventModelDestroy,
	EventTerminate,
	EventSendMessage,
	EventAddModel,
	EventNull,
	EventLoop,
	EventLoopWithCount,
	EventTrigger,
	EventPlaySound,
	EventStopBox,
	EventNull,
	EventTurnToTarget,
	EventWaitForTarget,
	EventTriggerNext,
	EventModelTurn,
	EventModelSpeedCurve,
	EventModelRotate,
	EventCollisionsOff
};

MODEL *event_model_list[512];
EVENT *FirstEvent;
EVENT_MODEL *main_model;
EVENT *main_event;
short *commands;
int num_event_models;
int num_events;

// [0x4d0ce0] [D]
void InitEventGlobals(void)
{
	FirstEvent = NULL;
	num_event_models = 0;
	num_events = 0;
}

// [0x4d0d03] [D]
// Runs the running events and starts the ones the player triggers.
void EventHandler(void)
{
	CAR_DATA *cp;
	float dx;
	float dz;

	cp = &car_data[PlayerCar];

	for (main_event = FirstEvent; main_event; main_event = main_event->next)
	{
		if (main_event->flags & 2)
		{
			DoEvent();
		}
		else if (main_event->flags & 4)
		{
			if (cells_across_halved + cp->location.vx / cell_size +
				cells_across * (cells_down_halved + cp->location.vz / cell_size) == EVENT_TRIGGER_CELL(main_event))
			{
				NextEvent();
				DoEvent();
			}
		}
		else if ((main_event->flags & 8) == 0)
		{
			dx = (float)cp->location.vx - EVENT_TRIGGER_X(main_event);
			dz = (float)cp->location.vz - EVENT_TRIGGER_Z(main_event);

			if (dx < 0.0f)
				dx = -dx;

			if (dz < 0.0f)
				dz = -dz;

			if (dx * dx + dz * dz <= EVENT_RADIUS_SQR(main_event))
			{
				NextEvent();
				DoEvent();
			}
		}
	}
}

// [0x4d0eaf] [D]
// Runs main_event's models for a frame; stops the event when the player leaves its trigger.
void DoEvent(void)
{
	CAR_DATA *cp;
	IVECTOR dir;
	float velocity;
	int dx;
	int dz;
	int cell;
	int run;

	if (GamePaused)
		return;

	cp = &car_data[PlayerCar];

	if (main_event->flags & 4)
	{
		cell = cells_across_halved + cp->location.vx / cell_size +
			cells_across * (cells_down_halved + cp->location.vz / cell_size);

		if (EVENT_TRIGGER_CELL(main_event) != cell)		// what the fuck?
		{
			main_event->flags &= ~2;
			return;
		}
	}
	else if ((main_event->flags & 8) == 0)
	{
		dx = cp->location.vx - float_to_int(EVENT_TRIGGER_X(main_event));
		dz = cp->location.vz - float_to_int(EVENT_TRIGGER_Z(main_event));

		if (dx < 0)
			dx = -dx;

		if (dz < 0)
			dz = -dz;

		if ((float)(dz * dz + dx * dx) > EVENT_RADIUS_SQR(main_event))
		{
			main_event->flags &= ~2;
			return;
		}
	}

	for (main_model = main_event->model; main_model; main_model = main_model->next)
	{
		if (main_model->flags & 4)
			continue;

		commands = main_model->commands_end;

		run = 1;

		while (run)
			run = ExecuteEvent[(unsigned short)*commands]();

		if ((main_event->flags & 2) == 0)
			return;

		if (main_model->flags & 4)
			continue;

		if ((main_model->flags & 0x10) && (main_model->flags & 0x80) == 0)
		{
			dir.vx = float_to_int(main_model->direction.vx - main_model->position.vx);
			dir.vy = float_to_int(main_model->direction.vy - main_model->position.vy);
			dir.vz = float_to_int(main_model->direction.vz - main_model->position.vz);

			RotateEventModel(main_model, &dir);
		}

		if (main_model->velocity != 0.0f && (main_model->flags & 0x40))
		{
			velocity = main_model->velocity;

			main_model->position.vx = fcos(main_model->rotation.vy) * velocity + main_model->position.vx;
			main_model->position.vz = fsin(main_model->rotation.vy) * velocity + main_model->position.vz;
		}

		main_model->commands_end = commands;
	}
}

// [0x4d1292] [D]
// Copies the events and their models to ptr (commands_end as an offset); returns the size.
int ReadEventData(char *ptr)
{
	EVENT *ev;
	EVENT_MODEL *em;
	int size;

	size = 0;

	for (ev = FirstEvent; ev; ev = ev->next)
	{
		memcpy(ptr, ev, sizeof(EVENT));

		ptr += sizeof(EVENT);
		size += sizeof(EVENT);

		for (em = ev->model; em; em = em->next)
		{
			em->commands_end = (short *)((char *)em->commands_end - (unsigned int)em->commands_begin);

			memcpy(ptr, em, sizeof(EVENT_MODEL));

			ptr += sizeof(EVENT_MODEL);
			size += sizeof(EVENT_MODEL);

			em->commands_end = (short *)((char *)em->commands_end + (unsigned int)em->commands_begin);
		}
	}

	return size;
}

// [0x4d1352] [D]
// Restores the events from ReadEventData's copy, keeping the links and command lists.
void WriteEventData(char *ptr)
{
	EVENT *ev;
	EVENT *next;
	EVENT_MODEL *em;
	EVENT_MODEL *model;
	EVENT_MODEL *next_model;
	short *commands_begin;

	for (ev = FirstEvent; ev; ev = ev->next)
	{
		model = ev->model;
		next = ev->next;

		memcpy(ev, ptr, sizeof(EVENT));

		ev->next = next;
		ev->model = model;

		ptr += sizeof(EVENT);

		for (em = ev->model; em; em = em->next)
		{
			commands_begin = em->commands_begin;
			next_model = em->next;

			*em = *(EVENT_MODEL *)ptr;
			ptr += sizeof(EVENT_MODEL);

			em->next = next_model;
			em->commands_begin = commands_begin;
			em->commands_end = (short *)((char *)em->commands_end + (unsigned int)em->commands_begin);
		}
	}
}

// [0x4d142c] [D]
void DisplayEvents(void)
{
	VECTOR4 offset;
	SVECTOR angle;
	int size;
	VECTOR4 left;
	IVECTOR right_pos;
	IVECTOR left_pos;
	VECTOR4 right;
	int tilt;
	VECTOR4 pos;
	int visible;
	VECTOR4 drift;
	VECTOR4 view;
	MATRIX smoke_mat;
	int palette;
	MODEL *mdl;
	MATRIX mat;

	memset(&drift, 0, sizeof(drift));

	palette = -1;

	for (main_event = FirstEvent; main_event; main_event = main_event->next)
	{
		if ((main_event->flags & 2) == 0)
			continue;

		for (main_model = main_event->model; main_model; main_model = main_model->next)
		{
			if (main_model->flags & 4)
				continue;

			main_model->rotation.vy = -main_model->rotation.vy & 0xfff;
			CalcMatrixXYZ(&mat, &main_model->rotation);
			main_model->rotation.vy = -main_model->rotation.vy & 0xfff;

			mdl = event_model_list[(unsigned short)main_model->model_id];

			visible = FrustrumCheckFar(&main_model->position, mdl->bounding_sphere);

			if (!visible)
				continue;

			if (main_model->flags & 0x20)
			{
				// tilt along the ground between the model's ends
				left.vx = -mdl->bounding_sphere;
				left.vy = 0.0f;
				left.vz = 0.0f;
				right.vx = mdl->bounding_sphere;
				right.vy = 0.0f;
				right.vz = 0.0f;

				CurrentRotationMatrix = &mat;

				MatrixRotate(&left);
				MatrixRotate(&right);

				left.vx = left.vx + main_model->position.vx;
				left.vy = left.vy + main_model->position.vy;
				left.vz = left.vz + main_model->position.vz;
				right.vx = right.vx + main_model->position.vx;
				right.vy = right.vy + main_model->position.vy;
				right.vz = right.vz + main_model->position.vz;

				left_pos.vx = float_to_int(left.vx);
				left_pos.vz = float_to_int(left.vz);
				right_pos.vx = float_to_int(right.vx);
				right_pos.vz = float_to_int(right.vz);

				left_pos.vy = MapHeightInt(&left_pos);
				right_pos.vy = MapHeightInt(&right_pos);

				tilt = Arc_Tan(right_pos.vx - left_pos.vx, right_pos.vy - left_pos.vy);

				RotMatrixZ(&mat, tilt);
			}

			// draw boat on water in miami
			if (GameLevel == 0 && main_model->model_id == 3)
			{
				size = (rand() & 0x1f) + 70;

				angle.vx = 0;
				angle.vy = main_model->rotation.vy & 0xfff;
				angle.vz = 0;
				angle.pad = 0;

				CalcMatrixXYZ(&smoke_mat, &angle);

				CurrentRotationMatrix = &smoke_mat;

				offset.vx = -1400.0f;
				offset.vy = 140.0f;
				offset.vz = 20.0f;

				MatrixRotate(&offset);

				pos.vx = offset.vx + main_model->position.vx;
				pos.vy = offset.vy + main_model->position.vy;
				pos.vz = offset.vz + main_model->position.vz;

				Setup_SlowSmoke(&pos, &drift, size * 5, 120, 8, 120, 2, 0.2f, 0);

				offset.vx = -1340.0f;
				offset.vy = 140.0f;
				offset.vz = -20.0f;

				MatrixRotate(&offset);

				size = (rand() & 0x1f) + 70;

				pos.vx = offset.vx + main_model->position.vx;
				pos.vy = offset.vy + main_model->position.vy;
				pos.vz = offset.vz + main_model->position.vz;

				Setup_SlowSmoke(&pos, &drift, size * 5, 120, 8, 120, 2, 0.2f, 0);
			}

			CurrentRotationMatrix = &inv_camera_matrix;

			view.vx = main_model->position.vx - camera_position.vx;
			view.vy = main_model->position.vy - camera_position.vy;
			view.vz = main_model->position.vz - camera_position.vz;

			MatrixRotate(&view);

			if (main_model->palette != palette && main_model->palette != -1)
			{
				palette = main_model->palette;
				SwapPalette(palette);
			}

			if (visible == 1)
				DrawObject(mdl, &mat, &view, 0);
			else if (visible == 2)
				DrawObject(mdl, &mat, &view, 1);
			else
				DrawObject(mdl, &mat, &view, 2);
		}
	}
}

// [0x4d1a49] [D]
// Starts the waiting events with the trigger value id.
void TriggerEvent(short id)
{
	EVENT *main_event_top;
	EVENT_MODEL *main_model_top;
	short *commands_top;

	main_event_top = main_event;
	main_model_top = main_model;
	commands_top = commands;

	for (main_event = FirstEvent; main_event; main_event = main_event->next)
	{
		if ((main_event->flags & 8) == 0 || (main_event->flags & 2))
			continue;

		if (main_event->id == id)
		{
			NextEvent();
			DoEvent();
		}
	}

	commands = commands_top;
	main_model = main_model_top;
	main_event = main_event_top;
}

// [0x4d1b00] [D]
void KillEvent(int id)
{
	EVENT *ev;

	for (ev = FirstEvent; ev; ev = ev->next)
	{
		if ((ev->flags & 2) && (ev->flags & 8) && (unsigned short)ev->id == id)
			ev->flags &= ~2;
	}
}

// [0x4d1b5e] [D]
// Starts main_event: its models start over from their first command.
void NextEvent(void)
{
	EVENT_MODEL *em;

	main_event->flags |= 2;

	for (em = main_event->model; em; em = em->next)
	{
		em->position.vz = 0.0f;
		em->position.vy = 0.0f;
		em->position.vx = 0.0f;

		em->field_30 = 0;
		em->field_2C = 0;
		em->field_28 = 0;

		em->rotation.vz = 0;
		em->rotation.vy = 0;
		em->rotation.vx = 0;

		em->flags = 0;

		em->commands_end = em->commands_begin;
	}
}

// [0x4d1c01] [D]
void StopEvents(void)
{
	for (main_event = FirstEvent; main_event; main_event = main_event->next)
	{
		if (main_event->flags & 2)
			main_event->flags &= ~2;
	}
}

// [0x4d1c51] [D]
// Turns the model's yaw towards dir by up to 32 a frame.
void RotateEventModel(EVENT_MODEL *em, IVECTOR *dir)
{
	short angle;
	int diff;

	angle = Arc_Tan(dir->vz, dir->vx);
	diff = ((angle - em->rotation.vy + 2048) & 0xfff) - 2048;

	if (abs(diff) >= 32)
	{
		if (diff >= 0)
			em->rotation.vy += 32;
		else
			em->rotation.vy -= 32;
	}
	else
	{
		em->rotation.vy = angle;
	}
}

// [0x4d1ce2] [D]
void DestroyEvents(void)
{
	EVENT *ev;
	EVENT *next;
	EVENT_MODEL *em;
	EVENT_MODEL *next_model;
	int i;

	ev = FirstEvent;

	FirstEvent = NULL;

	while (ev)
	{
		next = ev->next;

		for (em = ev->model; em; em = next_model)
		{
			next_model = em->next;

			// clones share their parent's commands
			if ((ev->flags & 0x20) == 0)
			{
				DebugFree(em->commands_begin);
				em->commands_begin = NULL;
			}

			DebugFree(em);
		}

		DebugFree(ev);
		ev = next;
	}

	if (num_event_models)
	{
		for (i = 0; i < num_event_models; i++)
		{
			DebugFree(event_model_list[i]);

			// REALLY make sure it's null, lmao
			event_model_list[i] = NULL;
			event_model_list[i] = NULL;
		}

		num_event_models = 0;
	}
}

// [0x4d1deb] [D]
// SET_MODEL model, x, y, z
int EventSetModel(void)
{
	commands++;

	if ((main_event->flags & 0x30) == 0)
		main_model->model_id = *commands;

	commands++;

	main_model->position.vx = *(float *)commands;
	commands += 2;

	main_model->position.vy = *(float *)commands;
	commands += 2;

	main_model->position.vz = *(float *)commands;
	commands += 2;

	main_model->flags &= 4;

	return 1;
}

// [0x4d1ea6] [D]
// SET_ROTATION x, y, z
int EventSetRotation(void)
{
	main_model->rotation.vx = *++commands;
	main_model->rotation.vy = *++commands;
	main_model->rotation.vz = *++commands;

	commands++;

	return 1;
}

// [0x4d1f1f] [D]
// MODEL_TARGET x, y, z
int EventModelTarget(void)
{
	commands++;

	main_model->direction.vx = *(float *)commands;
	commands += 2;

	main_model->direction.vy = *(float *)commands;
	commands += 2;

	main_model->direction.vz = *(float *)commands;
	commands += 2;

	main_model->flags = (main_model->flags & ~0x83) | 1;

	return 1;
}

// [0x4d1fb0] [D]
// COLLISIONSOFF
int EventCollisionsOff(void)
{
	commands++;

	main_model->flags |= 0x200;

	return 1;
}

// [0x4d1fdd] [D]
int EventNull(void)
{
	return 0;
}

// [0x4d1fe4] [D]
// TARGET_PLAYER
int EventPlayerTarget(void)
{
	commands++;

	main_model->flags = (main_model->flags & ~0x83) | 2;

	return 1;
}

// [0x4d2019] [D] [A]
// A car (the player or an active civilian) touching the box ahead of x, z in the direction
// (one of the four axes): half width, length.
// NOTE: returns car_data (the first car) for any car found, as in the original.
// NOTE: [A] the box is 0 for a direction off the four axes (uninitialised in the original).
CAR_DATA *CarBlockingEvent(int x, int z, int direction, int half_width, int length)
{
	CAR_DATA *cp;
	int min_x;
	int max_x;
	int min_z;
	int max_z;
	int cx;
	int cz;
	int (*box)[2];
	int dir;
	int i;
	int k;
	int hit;

	min_x = max_x = min_z = max_z = 0;

	if (direction > 4064 || direction < 32)
	{
		min_x = x - half_width;
		max_x = x + half_width;
		max_z = z + length;
		min_z = z;
	}
	else if (direction > 2016 && direction < 2080)
	{
		min_x = x - half_width;
		max_x = x + half_width;
		max_z = z;
		min_z = z - length;
	}
	else if (direction > 3040 && direction < 3104)
	{
		min_x = x;
		max_x = x + length;
		max_z = z + half_width;
		min_z = z - half_width;
	}
	else if (direction > 992 && direction < 1056)
	{
		min_x = x - length;
		max_x = x;
		max_z = z + half_width;
		min_z = z - half_width;
	}

	cp = car_data;

	for (i = 0; i < 65; i++, cp++)
	{
		if (i != 0 && civilian_ai_data[i - 1].ctrlState == 0)
			continue;

		cx = cp->location.vx;
		cz = cp->location.vz;

		if (abs(cx - x) >= length || abs(cz - z) >= length)
			continue;

		dir = -cp->direction & 0xfff;
		box = pre_rotated_boxes[dir / 4];

		// some corner on each side of every box edge
		hit = 0;

		for (k = 0; k < 4; k++)
		{
			if (cx + box[k][0] >= min_x)
				hit = 1;
		}

		if (!hit)
			continue;

		hit = 0;

		for (k = 0; k < 4; k++)
		{
			if (cx + box[k][0] <= max_x)
				hit = 1;
		}

		if (!hit)
			continue;

		hit = 0;

		for (k = 0; k < 4; k++)
		{
			if (cz + box[k][1] <= max_z)
				hit = 1;
		}

		if (!hit)
			continue;

		for (k = 0; k < 4; k++)
		{
			if (cz + box[k][1] >= min_z)
				return car_data;
		}
	}

	return NULL;
}

// [0x4d2314] [D]
// MODEL_SPEEDCURVE ends, x, z, start_velocity, top_velocity
// Moves to x, z, the speed following a sine from start_velocity: ends bit 0 speeds up over
// the first quarter of the way, bit 1 slows down over the last.
int EventModelSpeedCurve(void)
{
	short *command;
	short ends;
	float pos_x;
	float pos_z;
	float start_velocity;
	float top_velocity;
	float dx;
	float dz;
	float dist;
	float phase;
	float velocity;

	command = commands++;

	ends = *commands++;

	pos_x = *(float *)commands;
	commands += 2;

	pos_z = *(float *)commands;
	commands += 2;

	start_velocity = *(float *)commands;
	commands += 2;

	top_velocity = *(float *)commands;
	commands += 2;

	main_model->flags &= ~0x80;
	main_model->direction.vx = pos_x;
	main_model->direction.vz = pos_z;

	dx = pos_x - main_model->position.vx;
	dz = pos_z - main_model->position.vz;

	dist = sqrt(dx * dx + dz * dz);

	if ((main_model->flags & 0x100) == 0)
	{
		// 1 / the whole way
		main_model->field_5C = 1.0f / dist;
		main_model->flags |= 0x100;
	}

	phase = 2048.0f - 2048.0f * dist * main_model->field_5C;

	if (phase < 512.0f && (ends & 1))
		phase = phase + phase;
	else if (phase > 1536.0f && (ends & 2))
		phase = phase - 1536.0f + phase - 1536.0f + 1024.0f;
	else
		phase = 1024.0f;

	main_model->velocity = (top_velocity - start_velocity) * fsintab[float_to_int(phase)] + start_velocity;		// GG, IDA

	velocity = main_model->velocity;

	if (dist != 0.0f)
	{
		dx = dx / dist;
		dz = dz / dist;
	}

	if (dist >= velocity)
	{
		main_model->position.vx = dx * velocity + main_model->position.vx;
		main_model->position.vz = dz * velocity + main_model->position.vz;

		commands = command;

		return 0;
	}

	main_model->position.vx = pos_x;
	main_model->position.vz = pos_z;

	main_model->flags |= 0x80;
	main_model->flags &= ~0x100;

	return 1;
}

// [0x4d25dd] [D]
// MODEL_MOVETO x, y, z
// Moves at velocity; slows down when a car is in the stop box ahead.
int EventModelMoveto(void)
{
	short *command;
	float velocity;
	float target_x;
	float target_y;
	float target_z;
	float dx;
	float dy;
	float dz;
	float dist;
	int angle;
	int car_dx;
	int car_dz;

	command = commands++;

	velocity = main_model->velocity;

	target_x = *(float *)commands;
	commands += 2;

	target_y = *(float *)commands;
	commands += 2;

	target_z = *(float *)commands;
	commands += 2;

	main_model->flags &= ~0x80;

	main_model->direction.vx = target_x;
	main_model->direction.vy = target_y;
	main_model->direction.vz = target_z;

	if (velocity == 0.0f)
		return 1;

	dx = target_x - main_model->position.vx;
	dy = target_y - main_model->position.vy;
	dz = target_z - main_model->position.vz;

	if (main_model->field_3C && main_model->field_3e)
	{
		angle = Arc_Tan((int)dx, (int)dz);

		if (CarBlockingEvent(float_to_int(main_model->position.vx), float_to_int(main_model->position.vz),
			-angle & 0xfff, main_model->field_3C / 2, main_model->field_3e / 2))
		{
			// NOTE: the original takes the z distance from the player's x too
			car_dx = float_to_int(main_model->position.vx) - car_data[0].location.vx;
			car_dz = float_to_int(main_model->position.vz) - car_data[0].location.vx;

			dist = sqrt((double)(car_dz * car_dz + car_dx * car_dx));

			if (dist < (float)(main_model->field_3e * main_model->field_3e))
			{
				dist = dist / (float)main_model->field_3e;

				if (dist >= 0.3f)
					velocity = velocity * dist;
				else
					velocity = 0.0f;
			}
		}
	}

	dist = sqrt(dx * dx + dy * dy + dz * dz);

	if (dist != 0.0f)
	{
		dx = dx / dist;
		dy = dy / dist;
		dz = dz / dist;
	}

	if (dist >= velocity)
	{
		main_model->position.vx = dx * velocity + main_model->position.vx;
		main_model->position.vy = dy * velocity + main_model->position.vy;
		main_model->position.vz = dz * velocity + main_model->position.vz;

		commands = command;

		return 0;
	}

	main_model->position.vx = target_x;
	main_model->position.vy = target_y;
	main_model->position.vz = target_z;

	main_model->flags |= 0x80;

	return 1;
}

// [0x4d295c] [D]
// MODEL_TURN radius, quarter
// Turns a quarter circle of radius at velocity from the quarter's start angle (quarter & 3)
// clockwise, or anticlockwise when quarter >= 4.
int EventModelTurn(void)
{
	short *command;
	float radius;
	unsigned short quarter;
	unsigned short step;
	unsigned short left;

	command = commands++;

	radius = *(float *)commands;
	commands += 2;

	quarter = *commands++;

	step = (int)(1024.0f * main_model->velocity / (radius * 3.1415927f / 2.0f));

	if ((main_model->flags & 0x100) == 0)
		main_model->rotation.vy = (quarter & 3) << 10;

	main_model->flags |= 0x1c0;

	if (quarter >= 4)
	{
		main_model->rotation.vy += 4096 - step;
		main_model->rotation.vy &= 0xfff;

		left = main_model->rotation.vy - (((quarter + 3) & 3) << 10);
	}
	else
	{
		main_model->rotation.vy += step;

		left = (((quarter & 3) + 1) << 10) - main_model->rotation.vy;
	}

	if (left >= step)
	{
		commands = command;
		return 0;
	}

	main_model->flags &= ~0x140;

	return 1;
}

// [0x4d2b08] [D]
// MODEL_ROTATE axis, step, angle
int EventModelRotate(void)
{
	short *command;
	short axis;
	unsigned short step;
	unsigned short angle;
	short *which;
	int min_step;

	command = commands++;

	axis = *commands++;
	step = *commands++;
	angle = *commands++;

	// NOTE: an axis other than 0 - 2 leaves which uninitialised in the original; z here
	if (axis == 0)
		which = &main_model->rotation.vx;
	else if (axis == 1)
		which = &main_model->rotation.vy;
	else
		which = &main_model->rotation.vz;

	if (step <= 2047)
		min_step = step;
	else
		min_step = 4096 - step;

	if (abs(angle - *which) >= min_step)
	{
		*which += step;
		*which &= 0xfff;

		commands = command;

		return 0;
	}

	*which = angle;

	return 1;
}

// [0x4d2c5e] [D]
// MODEL_MOVE velocity, x, y, z (the direction; normalised by the compiler)
// not seen in scripts
int EventModelMove(void)
{
	commands++;

	main_model->velocity = *(float *)commands;
	commands += 2;

	main_model->field_28 = *(int *)commands;
	commands += 2;

	main_model->field_2C = *(int *)commands;
	commands += 2;

	main_model->field_30 = *(int *)commands;
	commands += 2;

	return 1;
}

// [0x4d2cf1] [D]
// MODEL_STOP; not seen in scripts
int EventModelStop(void)
{
	commands++;

	main_model->velocity = 0.0f;

	return 1;
}

// [0x4d2d15] [D]
// MODEL_VELOCITY velocity
int EventModelVelocity(void)
{
	commands++;

	main_model->velocity = *(float *)commands;
	commands += 2;

	return 1;
}

// [0x4d2d4c] [D]
// MODEL_SLOW min_velocity, step
int EventModelSlow(void)
{
	short *command;
	float min_velocity;
	float step;

	command = commands;

	main_model->flags |= 0x40;

	commands++;

	min_velocity = *(float *)commands;
	commands += 2;

	step = *(float *)commands;
	commands += 2;

	main_model->velocity = main_model->velocity - step;

	if (main_model->velocity <= min_velocity)
	{
		main_model->velocity = min_velocity;
		return 1;
	}

	commands = command;

	return 0;
}

// [0x4d2dfb] [D]
// MODEL_SPEED max_velocity, step
int EventModelSpeed(void)
{
	short *command;
	float max_velocity;
	float step;

	command = commands;

	main_model->flags |= 0x40;

	commands++;

	max_velocity = *(float *)commands;
	commands += 2;

	step = *(float *)commands;
	commands += 2;

	main_model->velocity = step + main_model->velocity;

	if (main_model->velocity >= max_velocity)
	{
		main_model->velocity = max_velocity;
		return 1;
	}

	commands = command;

	return 0;
}

// [0x4d2eaa] [D]
// STOPBOX width, length (the compiler does not store it)
int EventStopBox(void)
{
	commands++;

	main_model->field_38 = 1;
	main_model->field_3C = *commands++;
	main_model->field_3e = *commands++;

	return 1;
}

// [0x4d2f1c] [D]
// MODEL_WAIT frames (stored with its counter)
int EventModelWait(void)
{
	short *command;
	short frames;

	command = commands++;

	frames = *commands++;

	if (*commands)
	{
		(*commands)--;
		commands = command;
		return 0;
	}

	*commands++ = frames;

	return 1;
}

// [0x4d2fb8] [D]
// MODEL_DESTROY
int EventModelDestroy(void)
{
	commands++;

	main_model->flags |= 4;

	return 0;
}

// [0x4d2fe2] [D]
// TERMINATE_EVENT
int EventTerminate(void)
{
	commands++;

	main_event->flags &= ~2;

	return 0;
}

// [0x4d300c] [D]
// SEND_MESSAGE what, value: 1 tilt along the ground on / off, 2 palette
int EventSendMessage(void)
{
	short what;
	short value;

	what = *++commands;
	value = *++commands;

	commands++;

	if (what == 1)
	{
		if (value)
			main_model->flags |= 0x20;
		else
			main_model->flags &= ~0x20;
	}
	else if (what == 2)
	{
		main_model->palette = value;
	}

	return 1;
}

// [0x4d30b8] [D]
// ADD_MODEL model, x, y, z; does nothing
int EventAddModel(void)
{
	commands++;
	commands++;

	commands += 2;
	commands += 2;
	commands += 2;

	return 1;
}

// [0x4d3109] [D]
// TRIGGER id
int EventTrigger(void)
{
	short id;

	id = *++commands;

	commands++;

	TriggerEvent(id);

	return 1;
}

// [0x4d314a] [D]
// TRIGNEXT id: starts the next clone of a MULTIEVENT, else the event id (if not 0)
int EventTriggerNext(void)
{
	short id;

	id = *++commands;

	commands++;

	if (main_event->next && (main_event->next->flags & 0x20))
		TriggerEvent(main_event->id + 1);
	else if (id)
		TriggerEvent(id);

	return 1;
}

// [0x4d31e1] [D]
// PLAY_SOUND id; does nothing
int EventPlaySound(void)
{
	commands++;
	commands++;

	return 1;
}

// [0x4d3215] [D]
// LOOP: back to the first command after DO (the parameter is the byte offset)
int EventLoop(void)
{
	commands++;

	commands = (short *)((char *)commands - (unsigned short)*commands);

	return 1;
}

// [0x4d3252] [D]
// LOOP offset, count, counter
// NOTE: unreachable (the compiler never emits it); it reads the count as the counter and
// jumps back by the counter, as in the original
int EventLoopWithCount(void)
{
	short offset;

	offset = *++commands;

	if (*++commands)
	{
		(*commands)--;
		commands++;
		commands = (short *)((char *)commands - (unsigned short)*commands);
	}
	else
	{
		*commands++ = offset;
		commands++;
	}

	return 1;
}

// [0x4d3317] [D]
// TURNTOTARGET: toggles turning towards the target
int EventTurnToTarget(void)
{
	commands++;

	main_model->flags ^= 0x10;

	return 1;
}

// [0x4d3345] [D]
// WAITFORTARGET: waits until the model would reach its target this frame, then puts it there
int EventWaitForTarget(void)
{
	short *command;
	float velocity;
	float dx;
	float dy;
	float dz;
	float dist;
	int angle;
	int car_dx;
	int car_dz;

	command = commands++;

	velocity = main_model->velocity;

	main_model->flags |= 0x40;
	main_model->flags &= ~0x80;

	if (velocity != 0.0f)
	{
		dx = main_model->direction.vx - main_model->position.vx;
		dy = main_model->direction.vy - main_model->position.vy;
		dz = main_model->direction.vz - main_model->position.vz;

		if (main_model->field_3C && main_model->field_3e)
		{
			angle = Arc_Tan((int)dx, (int)dz);

			if (CarBlockingEvent(float_to_int(main_model->position.vx), float_to_int(main_model->position.vz),
				-angle & 0xfff, main_model->field_3C / 2, main_model->field_3e / 2))
			{
				// NOTE: the original takes the z distance from the player's x too
				car_dx = float_to_int(main_model->position.vx) - car_data[0].location.vx;
				car_dz = float_to_int(main_model->position.vz) - car_data[0].location.vx;

				dist = sqrt((double)(car_dz * car_dz + car_dx * car_dx));

				if (dist < (float)(main_model->field_3e * main_model->field_3e))
				{
					dist = dist / (float)main_model->field_3e;

					if (dist >= 0.3f)
						velocity = velocity * dist;
					else
						velocity = 0.0f;
				}
			}
		}

		dist = sqrt(dx * dx + dy * dy + dz * dz);

		if (dist >= velocity)
		{
			commands = command;
			return 0;
		}

		main_model->position.vx = main_model->direction.vx;
		main_model->position.vy = main_model->direction.vy;
		main_model->position.vz = main_model->direction.vz;

		main_model->flags |= 0x80;
	}

	main_model->flags &= ~0x40;

	return 1;
}

// [0x4d3664] [D]
void ResetBoxes(void)
{
}

// [0x4d3669] [D]
void DrawMoveableObjects(void)
{
}
