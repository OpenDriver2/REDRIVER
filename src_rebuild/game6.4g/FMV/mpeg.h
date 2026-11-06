/*
 * FMV/mpeg.h - [A] the videos (FMV/mpeg.c)
 */

#ifndef FMV_MPEG_H
#define FMV_MPEG_H

int MpegStartup(void);
void MpegShutdown(void);

int MpegOpen(const char *filename);
void MpegClose(void);
int MpegStep(void);

int MpegOpenSound(const char *filename, double *duration);
void MpegCloseSound(void);
int MpegPlaySound(void);
void MpegStopSound(void);

#endif // FMV_MPEG_H
