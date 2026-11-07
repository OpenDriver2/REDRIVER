/*
 * file.h - globals and functions of file.c used by other files
 */

#ifndef FILE_H
#define FILE_H

#include "dr_types.h"

// functions
extern int FileFindOnDisk(char *filename, char *drive);
extern int FileLoad(char *filename, void *buffer);
extern void *FileRead(char *filename);
extern void *FileReadGetSize(char *filename, int *size);
extern int FileSize(FILE *fp);

#endif // FILE_H
