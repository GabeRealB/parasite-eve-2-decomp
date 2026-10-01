#include "rooms/neo_ark_garden.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/rand.h>

#include "gte.h"
#include "types.h"

#include "neo_ark_garden_private.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/scene_combat.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

#include "overlay.h"

s32     rcos(s32);
s32     rsin(s32);
MATRIX* TransposeMatrix(MATRIX*, MATRIX*);

extern GpAreaApplyRec D_neo_ark_garden_80182BF8[];

extern s32 D_801334EC;
extern s32 D_80133954;

extern GpAreaTmdRec D_neo_ark_garden_80182AE8[2];
extern GpAreaTmdRec D_neo_ark_garden_80182B00[2];
extern GpAreaTmdRec D_neo_ark_garden_80182B18[3];
extern GpAreaTmdRec D_neo_ark_garden_80182B3C[2];

GpAreaTmdRec D_neo_ark_garden_80182AE8[2] = {
    { 38, 38, 0, 0, { 0, 0 }, D_80137D74 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_garden_80182B00[2] = {
    { 132, 510, 0, 0, { 0, 0 }, D_8013D2E0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_garden_80182B18[3] = {
    { 38, 38, 0, 0, { 0, 0 }, D_80137D74 },
    { 49, 49, 1, 0, { 0, 0 }, D_8015F400 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_garden_80182B3C[2] = {
    { 24, 24, 0, 0, { 0, 0 }, D_8013647C },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_neo_ark_garden_80182B54[13] = {
    { NULL, NULL },
    { D_map_neo_ark_8017B630, D_neo_ark_garden_80182AE8 },
    { D_map_neo_ark_8017B6E0, D_neo_ark_garden_80182B00 },
    { D_map_neo_ark_8017B700, D_neo_ark_garden_80182B18 },
    { D_map_neo_ark_8017B780, D_neo_ark_garden_80182B3C },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

WorldCollisionFootstepSounds D_neo_ark_garden_80182BBC = {
    0x1000000D,
    0x1000000F,
    0x1000000D,
};

WorldCollisionSurfaceProperties D_neo_ark_garden_80182BC8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_neo_ark_garden_80182BD0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_garden_80182BBC },
};

WorldCollisionSurfaceProperties* D_neo_ark_garden_80182BD8[8] = {
    D_neo_ark_garden_80182BC8,
    D_neo_ark_garden_80182BC8,
    D_neo_ark_garden_80182BC8,
    D_neo_ark_garden_80182BC8,
    D_neo_ark_garden_80182BD0,
    D_neo_ark_garden_80182BC8,
    D_neo_ark_garden_80182BC8,
    D_neo_ark_garden_80182BC8,
};

GpAreaApplyRec D_neo_ark_garden_80182BF8[3] = {
    { 5, 11, 2, 1 },
    { 5, 27, 2, 1 },
    { 255, 0, 0, 0 },
};

static void func_neo_ark_garden_8017E9B4(Task* arg0);
static void func_neo_ark_garden_8017EA34(Task* task);

#include "../../shared/water_refraction_task.inc.c"

#include "../../shared/water_distort_band_task.inc.c"

s32 func_neo_ark_garden_8017E840(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_neo_ark_garden_8017E848(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_neo_ark_80179B14(in, out);
    if (in->areaId != 0x21) {
        return 1;
    }
    if (GameFlag_GetNibble(0xDC) != 0) {
        return 1;
    }
    if (in->queryOnly != ROOM_EVENT_EXECUTE) {
        return 0;
    }
    Gp_SetNibbleIf(in->flagId, 2);
    Gp_RunCapCmd1(1);
    return 0;
}

s32 func_neo_ark_garden_8017E8DC(Task* arg0, s32 arg1, s32 arg2, TaskMessageArg arg3)
{
    if (arg2 == 4) {
        Gp_RunCapCmd1(GameFlag_GetNibble(0x141) != 0 ? 6 : 4);
        if ((GameFlag_GetNibble(0xFA) == 0) && (GameFlag_GetNibble(0xDC) == 0)) {
            GameFlag_SetNibble(0xFA, 1);
            Gp_ApplyAreaRecs(D_neo_ark_garden_80182BF8);
        }
    }
    if (arg2 == 7) {
        Gp_SpawnIfCapIdle(GameFlag_GetNibble(0xC7) != 0 ? 9 : 7, 0);
    }
    if (arg2 == 5) {
        Gp_SpawnIfCapIdle(GameFlag_GetNibble(0xC7) != 0 ? 0xA : 5, 0);
    }
    return 0;
}

s32 func_neo_ark_garden_8017E9AC(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

static void func_neo_ark_garden_8017E9B4(Task* arg0)
{
    arg0->msgTable = D_neo_ark_garden_801813B0;
    Game_SetPtrSlot(arg0, 7);
    // Match arrival warp 3 and placement variant 2 as one halfword.
    if (*(u16*)&gGameSession->location.loc.warp == ((2 << 8) | 3)) {
        func_800E8634(&D_801334EC, 0, &D_80133954);
        func_800E3FAC(0xA2, 0x34);
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// Idle state of the room's entry task: does nothing. The unused 0x10-byte
/// local reproduces the stack frame the retail code reserves.
static void func_neo_ark_garden_8017EA34(Task* task)
{
    char pad[0x10];
}

/// State handlers of the room's entry task: set-up, idle, then kill.
static const TaskFuncTable3 D_neo_ark_garden_8017D614 = {
    { func_neo_ark_garden_8017E9B4, func_neo_ark_garden_8017EA34, taskKill }
};

/// Tick of the room's entry task: copies its state table to the stack and
/// calls the handler for the task's state.
void func_neo_ark_garden_8017EA44(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_garden_8017D614;
    sp.funcs[task->state](task);
}
