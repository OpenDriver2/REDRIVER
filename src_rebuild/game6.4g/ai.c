/*
 * ai.c - car AI
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "ai.c"); partially translated.
 */

#include <string.h>

// [0x48e865] [D]
// Clears memory a dword at a time, then the remaining bytes.
void ClearMem(void *mem, size_t size)
{
	char *ptr;

	ptr = (char *)mem;

	if (size >> 2)
	{
		memset(mem, 0, (size >> 2) * 4);
		ptr = (char *)mem + (size >> 2) * 4;
	}

	if (size & 3)
		memset(ptr, 0, size & 3);
}
