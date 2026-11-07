/*
 * ai.h - globals and functions of ai.c used by other files
 */

#ifndef AI_H
#define AI_H

#include "dr_types.h"

// globals
extern ROUTE_DATA RouteData;
extern ROAD_LANE_INFO RoadLaneInfo[2048];
extern CAR_DATA *civ_ai_data_pt;
extern int pre_rotated_boxes[1024][4][2];
extern int LeadCarHit;
extern int LeadCarLost;
extern int LeadCarFinished;
extern PING_POINT path_ping_points[240];
extern int num_path_ping_points;
extern int RequestCopCar;
extern short PlayerLastRoad;
extern CIV_STATE civilian_ai_data[65];

// functions
extern int AllocateAndPlaceCarAtPosition(int x, int y, int z, int dir, int blocking);
extern void BuildJunctionPingInfo(void);
extern void CheckCivilianScenaryCollisions(void);
extern void CheckCopsWithPedestrians(void);
extern int CheckRedLights(int road);
extern void ClearMem(void *mem, size_t size);
extern void ControlAI(void);
extern void ControlRoadBlock(void);
extern void DrawCivilianCars(void);
extern void HandleCopSirens(void);
extern void InitCopState(CAR_DATA *cp, COP *cop, int id);
extern void InitPlayerCarPosition(void);
extern void InitialiseAI(void);
extern void InstantCivilianCar(char index, int model, char palette, int dir, int xpos, int zpos);
extern void InstantCopCar(int dir, int x, int z);
extern int IsMissionCarLost(int targetId, int targetRange);
extern int IsMissionCarWrecked(int targetId, int maxDamage, IVECTOR *pos);
extern void NotifyAIOfCollision(int c1, int c2, int howHard);
extern void NotifyAIRunAFewFrames(int car);
extern int RequestRoadBlock(unsigned char dir);

#endif // AI_H
