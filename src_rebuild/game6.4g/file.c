/*
 * file.c - file helpers
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "file.c"); partially translated.
 * The original path was D:\Driver\game6.4g\file.c (from a DebugMalloc call).
 */

#include <stdio.h>

/* general.c */
extern void *DebugMalloc(size_t size, const char *filename, int line);
extern char DriverFRead(void *dst, int size, int count, FILE *file);

int FileSize(FILE *fp);

// [0x4d4be0] [D]
int FileSize(FILE *fp)
{
	int size;

	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	return size;
}

// [0x4d4d6e] [D]
// Loads a whole file into a new DebugMalloc block; returns NULL on failure.
void *FileReadGetSize(char *filename, int *size)
{
	void *mem;
	FILE *fp;

	fp = fopen(filename, "rb");

	if (fp)
	{
		*size = FileSize(fp);

		if (*size)
		{
			// NOTE: the binary passes "D:\\Driver\\game6.4g\\file.c", 177 here
			mem = DebugMalloc(*size, __FILE__, __LINE__);

			if (mem && DriverFRead(mem, *size, 1, fp))
			{
				fclose(fp);
				return mem;
			}
		}

		fclose(fp);
	}

	return NULL;
}
