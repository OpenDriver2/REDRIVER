/*
 * map.h - globals and functions of map.c used by other files
 */

#ifndef MAP_H
#define MAP_H

#include "dr_types.h"

// globals
extern int cells_across;
extern int cells_down;
extern int cell_size;
extern char *level_files[7];
extern CELL_OBJECT **ground_debris_ptr;
extern TEX_INFO *tinfo_block;
extern int num_textures;
extern char *model_names;
// most references: map.c 29, frontend/frontend.c 5
extern int gCurrentRamBytesUsed;
extern char *PackedPVSRegions;
extern int NumRoads;
extern int NumJunctions;
extern int *MapData;
extern int MapInfo;
extern int SurfaceData;
extern int ModelActive[1632];
extern int OrigSurfaceData;
extern int RoadMapXMid;
extern int RoadMapUnitWidth;
extern int RoadMapUnitHeight;
extern ROAD Roads[2048];
extern int RoadMapUnitZMid;
extern ROADBOUNDS RoadBounds[2048];
extern float road_region_size_recip;
extern int RoadMapUnitXMid;
extern int RoadMapHeight;
extern int road_region_size;
extern float road_recip;
extern int *RoadMapInfoRegions[4];
extern int RoadMapZMid;
extern XYPAIR JunctionPositions[1024];
extern int RoadMapWidth;
extern JUNCTION Junctions[1024];
extern SURFACEINFO *SurfacePtrs[16384];
extern int RoadMapRegions[8];
extern short *RoadMapDataRegions[4];
extern CAMERA camera;
extern MOVEABLE_OBJECT *moveable_object_mem_originals;
extern int region_size;
extern float gAmbientLightDay;
extern int regions_down;
extern int cells_across_halved;
// most references: mirror.c 4, glaunch.c 3, map.c 1
extern short view_arc[256][64];
extern CELL_DATA *cells;
extern int units_across_halved;
extern int units_down_halved;
extern int horizon_angle;
// most references: glaunch.c 3, mirror.c 2, map.c 1
extern short *arc_view;
extern int num_models_in_pack;
extern int cells_down_halved;
extern char CurrentPVS[10004];
extern int num_regions;
extern CELL_OBJECT *ground_debris[16];
extern MODEL *model_pointers[1152];
extern CAMERA player;
extern VECTOR4 gLightSourceDay;
extern int num_moveable_objects;
extern CELL_OBJECT *cell_objects;
extern MOVEABLE_OBJECT *moveable_object_mem;
extern int regions_unpacked[4];
extern VECTOR4 gLightSourceNight;
extern int regions_across;
// NOTE: a pointer kept as int
// most references: map.c 11, frontend/frontend.c 7, glaunch.c 6
extern char *gRamTextureRawDataDump;
extern float gAmbientLightNight;
extern int *cell_ptrs;

// functions
extern void DrawHorizon(void);
extern void DrawMap(int update_cells);
extern void DrawTransparentObjects(void);
extern void DrawWater(void);
extern MODEL *FindModelPtrWithName(char *name);
extern int FindModelWithName(char *name);
extern void FreePackedRegionMem(void);
extern void FreePaletteData(void);
extern TEX_INFO *GetTextureDetails(char *tex_name);
extern void GetVisSetAtPosition(VECTOR4 *pos, char *vis, int *p_ccx, int *p_ccz);
extern void InitHorizon(void);
extern void InitMap(void);
extern void InitPackedRegionMem(void);
extern void KillCellData(void);
extern void KillModelMem(void);
extern void LoadCosmeticsAndDentData(void);
extern void LoadPaletteData(char *filename);
extern int LoadViewArc(void);
extern int PositionVisible(VECTOR4 *pos, char *vis, int ccx, int ccz);
extern int ReadLevelFile(void);
extern void ResetMap(void);

#endif // MAP_H
