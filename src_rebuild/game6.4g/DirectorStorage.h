/*
 * DirectorStorage.h - globals and functions of DirectorStorage.c used by other files
 */

#ifndef DIRECTORSTORAGE_H
#define DIRECTORSTORAGE_H

#include "dr_types.h"

// functions
extern void DeleteWaypoints(void);
extern int LoadDirector(char *filename);
extern int LoadWaypoints(void);
extern int SaveDirector(char *filename);
extern void SaveDirectorCameras(char *filename);
extern void SaveWaypoints(int frame);

#endif // DIRECTORSTORAGE_H
