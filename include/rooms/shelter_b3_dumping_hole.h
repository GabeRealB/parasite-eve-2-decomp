#ifndef INCLUDE_ROOMS_SHELTER_B3_DUMPING_HOLE_H
#define INCLUDE_ROOMS_SHELTER_B3_DUMPING_HOLE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"
#include "overlay.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

extern OverlayEncounterSpot D_shelter_b3_dumping_hole_8018B74C[12];

extern TmdSource gShelterB3DumpingHoleAcropolisSanctuaryModel090F0;

extern TaskDesc D_shelter_b3_dumping_hole_8018B83C[4];

extern TaskDesc D_shelter_b3_dumping_hole_8018B57C[1];

extern AreaVariant D_shelter_b3_dumping_hole_8018EC3C[13];

// shelter_b3_dumping_hole
extern WorldCollisionRoomResources D_shelter_b3_dumping_hole_8018B678[];

extern u8* D_shelter_b3_dumping_hole_8018B698[];

extern ViewCount D_shelter_b3_dumping_hole_8018B6A0[];

extern DirectionWarpEntry D_shelter_b3_dumping_hole_8018B6A4[];

extern ViewCamera D_shelter_b3_dumping_hole_8018C410[];

extern SpriteView D_shelter_b3_dumping_hole_8018E050[];

extern WorldCoordRoomLights D_shelter_b3_dumping_hole_8018E3DC;

extern WorldCoordRoomLights D_shelter_b3_dumping_hole_8018E874;

extern WorldCoordRoomAmbientEntry D_shelter_b3_dumping_hole_8018F1FC[];

extern WorldCoordRoomAmbientEntry D_shelter_b3_dumping_hole_8018F32C[];

extern WorldCollisionSurfaceProperties* D_shelter_b3_dumping_hole_8018F480[];

void func_shelter_b3_dumping_hole_8017D9A8(Task* task);

void func_shelter_b3_dumping_hole_8017FCF4(GfxCoord* arg0, SVECTOR* arg1);

void func_shelter_b3_dumping_hole_80183F84(Task* task);

void shelterB3DumpingHoleEffectSpriteDriftTaskAimed(Task* task);

void func_shelter_b3_dumping_hole_80186218(Task* task);

void func_shelter_b3_dumping_hole_80186D4C(Task* arg0);

#endif // INCLUDE_ROOMS_SHELTER_B3_DUMPING_HOLE_H
