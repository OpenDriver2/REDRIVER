/*
 * file.c - file helpers
 *
 * Rebuilt from Driver_sym2_94.i64 (IDA folder "file.c").
 * The original path was D:\Driver\game6.4g\file.c (from a DebugMalloc call).
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "file.h"
#include "general.h"

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

// [0x4d4c1a] [D]
int FileLoad(char *filename, void *buffer)
{
	int size;
	FILE *fp;

	fp = fopen(filename, "rb");

	if (!fp)
		return 0;

	size = FileSize(fp);

	// NOTE: the file is left open when the read fails, as in the original
	if (!DriverFRead(buffer, size, 1, fp))
		return 0;

	fclose(fp);

	return size;
}

// [0x4d4c83] [D]
// NOTE: unreferenced (only its thunk is left); the file is not closed when fwrite fails
int FileSave(char *filename, void *buffer, int size)
{
	FILE *fp;

	fp = fopen(filename, "wb");

	if (!fp)
		return 0;

	if (!fwrite(buffer, size, 1, fp))
		return 0;

	fclose(fp);

	return 1;
}

// [0x4d4cd8] [D]
// Loads a whole file into a new DebugMalloc block (not 0-terminated); returns NULL on failure.
void *FileRead(char *filename)
{
	void *mem;
	int size;
	FILE *fp;

	fp = fopen(filename, "rb");

	if (fp)
	{
		size = FileSize(fp);

		if (size)
		{
			// NOTE: the binary passes "D:\\Driver\\game6.4g\\file.c", 134 here
			mem = DebugMalloc(size, __FILE__, __LINE__);

			if (mem && DriverFRead(mem, size, 1, fp))
			{
				fclose(fp);
				return mem;
			}
		}

		fclose(fp);
	}

	return NULL;
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

// [0x4d4e0c] [D]
// Looks for filename in the root of every CD-ROM drive; on success copies the drive root
// ("D:\") to drive.
int FileFindOnDisk(char *filename, char *drive)
{
	FILE *fp;
	char path[260];
	char drives[260];
	char *root;

	if (GetLogicalDriveStrings(256, drives))
	{
		for (root = drives; *root; root += strlen(root) + 1)
		{
			if (GetDriveType(root) == DRIVE_CDROM)
			{
				sprintf(path, "%s%s", root, filename);

				fp = fopen(path, "rb");

				if (fp)
				{
					fclose(fp);

					sprintf(drive, root);

					return 1;
				}
			}
		}
	}

	return 0;
}
