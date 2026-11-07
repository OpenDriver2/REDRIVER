/*
 * Engine/3dfx/tset.h - globals and functions of Engine/3dfx/tset.c used by other files
 */

#ifndef ENGINE_3DFX_TSET_H
#define ENGINE_3DFX_TSET_H

#include "dr_types.h"

// globals
extern TextureSet TextureSlots[];
extern char gUsedTSets[1024][70];

// functions
extern void AddTextureSet16(int tset, void *data);
extern void AddTextureSet8(int Parent, char *ParentData, char *ChildData);
extern void CheckUnpackRegionTSets(void);
extern void InitTextureSpace(void);
extern void ReplaceTextureSet(int num, void *tmem);

#endif // ENGINE_3DFX_TSET_H
