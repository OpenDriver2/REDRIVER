/*
 * debug.c - debug log and on-screen debug info
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "debug.c"); partially translated.
 */

#include <stdio.h>

// initialised data (0x555e8c)
int gDebugLog = 1;

// NOTE: bss global in the linker's common area; defined here as LogQuick is its only user
int log_size;

// [0x4c0a60] [D]
void LogQuick(char *text)
{
	FILE *handle;

	if (!gDebugLog)
		return;

	handle = fopen("debug.log", "r+t");

	if (handle && log_size == 0)
	{
		fclose(handle);
		handle = fopen("debug.log", "wt");
	}
	else
	{
		if (handle)
			fclose(handle);

		handle = fopen("debug.log", "a+t");
	}

	if (handle)
	{
		fprintf(handle, "%s\n", text);
		log_size = ftell(handle);
		fclose(handle);
	}
}
