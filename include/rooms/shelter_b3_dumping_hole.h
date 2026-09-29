#ifndef INCLUDE_ROOMS_SHELTER_B3_DUMPING_HOLE_H
#define INCLUDE_ROOMS_SHELTER_B3_DUMPING_HOLE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actors_shared_801673f8.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "shared/cap_caption_types.h"

extern ActorsShared801673f8Spot D_shelter_b3_dumping_hole_8018B74C[12];

extern TmdSource D_shelter_b3_dumping_hole_80187550;

extern TaskDesc D_shelter_b3_dumping_hole_8018B83C[4];

extern CapCaptionTaskTable D_shelter_b3_dumping_hole_8018B57C;

extern GpAreaVariant D_shelter_b3_dumping_hole_8018EC3C[13];

// shelter_b3_dumping_hole
extern GpRoomObjRec D_shelter_b3_dumping_hole_8018B678[];

extern u8* D_shelter_b3_dumping_hole_8018B698[];

extern GpViewCountRec D_shelter_b3_dumping_hole_8018B6A0[];

extern GpWarpRec D_shelter_b3_dumping_hole_8018B6A4[];

extern GpViewRec D_shelter_b3_dumping_hole_8018C410[];

extern GpSprtRec D_shelter_b3_dumping_hole_8018E050[];

extern GpRoomCoordSet D_shelter_b3_dumping_hole_8018E3DC;

extern GpRoomCoordSet D_shelter_b3_dumping_hole_8018E874;

extern GpRoomBoundVec D_shelter_b3_dumping_hole_8018F1FC[];

extern GpRoomBoundVec D_shelter_b3_dumping_hole_8018F32C[];

extern GpRoomParamRec* D_shelter_b3_dumping_hole_8018F480[];

void func_shelter_b3_dumping_hole_8017D9A8(Task* task);

void func_shelter_b3_dumping_hole_8017FCF4(GpCoord* arg0, SVECTOR* arg1);

void func_shelter_b3_dumping_hole_80183F84(Task* task);

void func_shelter_b3_dumping_hole_8018521C(Task* task);

void func_shelter_b3_dumping_hole_80186218(Task* task);

void func_shelter_b3_dumping_hole_80186D4C(Task* arg0);

#endif // INCLUDE_ROOMS_SHELTER_B3_DUMPING_HOLE_H
