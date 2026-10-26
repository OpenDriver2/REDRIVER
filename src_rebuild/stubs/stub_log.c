/*
 * stub_log.c - logging for not yet translated functions (see stubs.h); not part of the original game
 */

#include <windows.h>
#include <stdio.h>

#include "stubs.h"

// [A]
void StubLog(const char *name)
{
	static int first = 1;
	FILE *fp;

	fp = fopen("stubs.log", first ? "wt" : "at");
	first = 0;

	if (fp)
	{
		fprintf(fp, "STUB: %s\n", name);
		fclose(fp);
	}

	OutputDebugString("STUB: ");
	OutputDebugString(name);
	OutputDebugString("\n");
}
