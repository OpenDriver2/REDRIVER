/*
 * general.c - memory debugging, frame sync, misc helpers
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "general.c"); partially translated.
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct SCREEN
{
	HWND handle;
	HINSTANCE module;
	int fullscreen;
	int active;
} SCREEN;

typedef struct MEM_RECORD
{
	unsigned long mem_addr;
	unsigned long size;
	char *filename;
} MEM_RECORD;

#define MAX_MEM_PACKETS		4096	// NOTE: known constant

#define FRAME_TIME			20	// ms per game frame (50 fps)
#define MAX_FRAMES_PER_SYNC	10

/* shell.c, ai.c */
extern void HandleWinMessages(void);
extern void ClearMem(void *mem, size_t size);

void UpdateLoadingBar(void);

/* input.c */
extern int ReadInput(void);
extern char KeyMap[256];

extern SCREEN gScreen;
extern int gDoAutoInterpolation;
extern int gDoInterpolation;
extern int max_frame_skip;
extern int FrameCount;

// NOTE: bss globals in the linker's common area, owner by layout unknown; defined here next to
// their only users
MEM_RECORD mem_info[MAX_MEM_PACKETS];
long mem_allocated, mem_released, current_mem_usage, peak_mem_usage;
int LastFrameCount;
int gTotalBytesRead;
int gNextUpdate;
int gDoLoadingBar;

// [0x4d94c3] [D]
// Waits for the next 20 ms frame tick; returns how many frames passed (at most 10).
int WaitForSync(int delta)
{
	DWORD frames;

	frames = 0;

	HandleWinMessages();

	if (gDoAutoInterpolation)
	{
		while (frames == 0)
			frames = (timeGetTime() - LastFrameCount) / FRAME_TIME;

		LastFrameCount += FRAME_TIME * frames;

		if (frames > MAX_FRAMES_PER_SYNC)
			frames = MAX_FRAMES_PER_SYNC;

		FrameCount += frames;
		return frames;
	}
	else if (gDoInterpolation)
	{
		while (frames < (DWORD)max_frame_skip)
			frames = (timeGetTime() - LastFrameCount) / FRAME_TIME;

		LastFrameCount += FRAME_TIME * frames;

		if (frames > MAX_FRAMES_PER_SYNC)
			frames = MAX_FRAMES_PER_SYNC;

		FrameCount += frames;
		return frames;
	}

	FrameCount++;
	return 1;
}

// [0x4dab08] [D]
void InitMemDebug(void)
{
	int loop;

	for (loop = 0; loop < MAX_MEM_PACKETS; loop++)
		mem_info[loop].mem_addr = 0;

	mem_allocated = 0;
	current_mem_usage = 0;
	mem_released = 0;
	peak_mem_usage = 0;
}

// [0x4dab65] [D]
void CloseMemDebug(void)
{
	if (mem_allocated != mem_released)
	{
		int loop;
		char text[256];

		for (loop = 0; loop < MAX_MEM_PACKETS; loop++)
		{
			if (mem_info[loop].mem_addr)
			{
				if (mem_info[loop].filename)
				{
					sprintf(text, "%d bytes of memory were not released from address $%x!\n"
									"This is due to poor use of malloc / free - resolve immediately!\n"
									"Memory Log: %s",
										mem_info[loop].size, mem_info[loop].mem_addr, mem_info[loop].filename);

					free(mem_info[loop].filename);
					mem_info[loop].filename = NULL;
				}
				else
				{
					sprintf(text, "%d bytes of memory were not released from address $%x!\n"
									"This is due to poor use of malloc / free - resolve immediately!",
										mem_info[loop].size, mem_info[loop].mem_addr);
				}

				MessageBox(gScreen.handle, text, "MEMORY FAULT", MB_OK | MB_ICONEXCLAMATION);

				free((void *)mem_info[loop].mem_addr);
				mem_info[loop].mem_addr = 0;
			}
		}
	}
}

// [0x4dac99] [D]
// malloc with a 4 byte "NAIF" guard after the block and an allocation log entry.
void *DebugMalloc(size_t size, const char *filename, int line)
{
	char *mem;
	char text[128];
	int i;

	sprintf(text, "File: %s (Line: %d)", filename, line);

	mem = (char *)malloc(size + 4);

	if (size)
		ClearMem(mem, size);

	memcpy(&mem[size], "NAIF", 4);

	if (mem)
	{
		for (i = 0; i < MAX_MEM_PACKETS; i++)
		{
			if (mem_info[i].mem_addr == 0)
			{
				mem_info[i].mem_addr = (unsigned long)mem;
				mem_info[i].size = size;

				mem_info[i].filename = (char *)malloc(strlen(text) + 1);

				if (mem_info[i].filename)
					strcpy(mem_info[i].filename, text);

				break;
			}
		}

		mem_allocated += size;
		current_mem_usage += size;

		if (current_mem_usage > peak_mem_usage)
			peak_mem_usage = current_mem_usage;
	}

	return mem;
}

// [0x4dae15] [D]
void DebugFree(void *mem)
{
	int loop;

	if (mem)
	{
		for (loop = 0; loop < MAX_MEM_PACKETS && mem_info[loop].mem_addr != (unsigned long)mem; loop++)
			;

		// NOTE: the original also computes mem + size here into an unused local; dropped

		if (mem_info[loop].filename)
			free(mem_info[loop].filename);

		mem_info[loop].filename = NULL;

		mem_released += mem_info[loop].size;
		current_mem_usage -= mem_info[loop].size;

		mem_info[loop].mem_addr = 0;

		free(mem);
	}
}

// [0x4db47b] [D]
char DriverFRead(void *dst, int size, int count, FILE *file)
{
	char *ptr;

	ptr = (char *)dst;

	if (count == 1)
	{
		// split up big reads into many smaller ones
		while (size > 65535)
		{
			fread(ptr, 65535, 1, file);

			ptr += 65535;
			size -= 65535;
			gTotalBytesRead += 65535;
		}

		fread(ptr, size, 1, file);
		gTotalBytesRead += size;
	}
	else
	{
		fread(dst, size, count, file);
		gTotalBytesRead += count * size;
	}

	if (gNextUpdate < gTotalBytesRead && gDoLoadingBar)
		UpdateLoadingBar();

	return 1;
}

// [0x4db6c8] [D]
int AnyKeyPressed(void)
{
	int i;

	ReadInput();

	i = 255;

	while (i--)
	{
		if (KeyMap[i])
			return 1;
	}

	return 0;
}
