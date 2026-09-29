#ifndef INCLUDE_ROOMS_ACROPOLIS_HELICOPTER_LANDING_PAD_H
#define INCLUDE_ROOMS_ACROPOLIS_HELICOPTER_LANDING_PAD_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/pad_script.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

extern GpScriptCmd D_acropolis_helicopter_landing_pad_80187D34[2];

extern GpScriptRec D_acropolis_helicopter_landing_pad_80187D3C;

extern GpAreaVariant D_acropolis_helicopter_landing_pad_801861E8[13];

extern TmdSource D_acropolis_helicopter_landing_pad_801836EC;

extern TmdSource D_acropolis_helicopter_landing_pad_80187F50;

// acropolis_helicopter_landing_pad
extern GpRoomObjRec D_acropolis_helicopter_landing_pad_80184F10[];

extern u8* D_acropolis_helicopter_landing_pad_80184F3C[];

extern GpViewCountRec D_acropolis_helicopter_landing_pad_80184F40[];

extern GpRoomCoordRec D_acropolis_helicopter_landing_pad_80184F44[];

extern GpWarpRec D_acropolis_helicopter_landing_pad_80184F4C[];

extern GpSprtRec D_acropolis_helicopter_landing_pad_80187824[];

extern GpViewRec D_acropolis_helicopter_landing_pad_80187968[];

extern GpRoomParamRec* D_acropolis_helicopter_landing_pad_80187DC8[];

void func_acropolis_helicopter_landing_pad_80180A64(GpCoord* coord);

void func_acropolis_helicopter_landing_pad_801818F0(Task* arg0);

void func_acropolis_helicopter_landing_pad_8017FA30(Task* arg0);

void func_acropolis_helicopter_landing_pad_80181064(Task* arg0);

void func_acropolis_helicopter_landing_pad_80180E40(Task* arg0);

void func_acropolis_helicopter_landing_pad_8017EF60(s32 unused0, s32 unused1);

void func_acropolis_helicopter_landing_pad_801802E0(Task* arg0);

void func_acropolis_helicopter_landing_pad_8017EF8C(Task* arg0);

void func_acropolis_helicopter_landing_pad_8017D964(Task* task);

void func_acropolis_helicopter_landing_pad_8017EB00(Task* task);

void func_acropolis_helicopter_landing_pad_801822B0(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_HELICOPTER_LANDING_PAD_H
