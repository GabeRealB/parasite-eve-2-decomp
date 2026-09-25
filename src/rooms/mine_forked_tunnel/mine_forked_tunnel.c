#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

/// The enemy's position / rotation path, one `SVECTOR` per step: `pos` and
/// `rot` are the halves `func_mine_forked_tunnel_8017D5E8` and
/// `func_mine_forked_tunnel_8017D8EC` compose into the `RoomPlacement` they
/// hand `func_mine_forked_tunnel_8017DC8C` (entry 0 of each) and that
/// `func_mine_forked_tunnel_8017D724` walks one entry per step of
/// `Task::killCountdown`, which it clamps at 0x6E. Both are 240 entries - the
/// position table starts where the rotation table ends, and the pitch table
/// below starts where the position table ends.
extern SVECTOR D_mine_forked_tunnel_80181244[240];
extern SVECTOR D_mine_forked_tunnel_80180AC4[240];

/// The placement `func_mine_forked_tunnel_8017D5E8` uses instead when the
/// `0x75` game flag is set: a complete `RoomPlacement` sitting in the room's
/// `.data`, offset (0x8CD, 0x3C4, 0x46B) with a half-turn about Y.
extern RoomPlacement D_mine_forked_tunnel_80181BBC;

/// The `RoomPlacement` the tunnel's pitch-animated object adopts: state 0
/// (`func_mine_forked_tunnel_8017DE54`) copies it onto the task's coordinate
/// whole, and state 1 (`func_mine_forked_tunnel_8017DAB8`) then keeps its `pos`
/// while taking the `rot` from the pitch table below. Position
/// (0xB4, -0xEB, -0x30C), rotation zero.
extern RoomPlacement D_mine_forked_tunnel_80181BA4;

/// The pitch curve `func_mine_forked_tunnel_8017DAB8` walks that object
/// through, one `SVECTOR` per step of the counter it runs while
/// `Task::spawnArg1` is 1: entries 0-15 are zero, then `vx` falls to -8 and
/// climbs to 175 before settling at 173 (4096 is a full turn), so the object
/// rises over the sequence and holds. The table is 54 entries, the counter's
/// limit, so the last step lands on the settling value.
extern SVECTOR D_mine_forked_tunnel_801819C4[54];

/// Two-entry `TaskDesc` table `func_mine_forked_tunnel_8017D5E8` spawns the
/// child enemy from; `Task_SpawnFromTable` picks entry 1.
extern TaskDesc D_mine_forked_tunnel_80181B74;

/// The `{id, TaskFunc}` pairs the tunnel's enemy restores through
/// `Task::msgTable` - `0x7D5` maps to `func_mine_forked_tunnel_8017DD08` and
/// `0x7DB` to `func_mine_forked_tunnel_8017D8EC`, the two ids
/// `func_mine_forked_tunnel_8017D5E8` registers.
extern s32 D_mine_forked_tunnel_80181B8C;

/// Per-task work block for the tunnel's enemy: a 0x48-byte `memCalloc`
/// allocation `func_mine_forked_tunnel_8017D5E8` stores at `Task::work`,
/// seeding `field_44` to -1. The two leading matrices are the light and colour
/// matrices `func_mine_forked_tunnel_8017DC70` republishes onto the task's
/// `TmdObject` (`field_1C` / `field_20`) so `Tmd_SetupDraw` picks them up.
/// Same layout as `Actor503500ColorMtx`, whose overlay carries a byte-identical
/// copy of that function.
///
/// `field_40` is the `Task_SpawnFromTable` child the room's `func_mine_forked_tunnel_8017D724`
/// frees; `field_44` is the signed lifetime counter it decrements.
typedef struct MineForkedTunnelWork {
    /* 0x00 */ MATRIX light;
    /* 0x20 */ MATRIX color;
    /* 0x40 */ void*  field_40;
    /* 0x44 */ s32    field_44;
} MineForkedTunnelWork;
STATIC_ASSERT_SIZEOF(MineForkedTunnelWork, 0x48);

/// One of the per-view objects the room's sprite-table record points at, and
/// whose `field_1C` this room's `func_mine_forked_tunnel_8017E48C` flips. Same
/// shape as the `field_C` byte the other rooms carrying this sprite table
/// write (the dryfield water tower's `DwtwSprtViewState`): non-zero leaves the
/// view's sprites out of the ordering table, zero draws them.
typedef struct MineForkedTunnelViewA {
    /* 0x00 */ byte pad_0[0x1C];
    /* 0x1C */ u8   field_1C;
} MineForkedTunnelViewA;
STATIC_ASSERT_SIZEOF(MineForkedTunnelViewA, 0x1D);

/// The second per-view object, written together with and to the same value as
/// `MineForkedTunnelViewA`'s byte.
typedef struct MineForkedTunnelViewB {
    /* 0x00 */ byte pad_0[0x2C];
    /* 0x2C */ u8   field_2C;
} MineForkedTunnelViewB;
STATIC_ASSERT_SIZEOF(MineForkedTunnelViewB, 0x2D);

/// The record `Gp_SprtTables[stage - 1]->field_0[room - 1]` really points at: a
/// room-sized block, far larger than the 0xC-byte `GpSprtRec` the table's
/// element type declares, so the room reaches its tail through a cast (as the
/// water tower's `DwtwSprtRec` and the dumping hole's `SprtBigRec` do). The
/// tail is a run of per-view pointers; this room's pair sits at 0x28 / 0x34.
typedef struct MineForkedTunnelSprtRec {
    /* 0x00 */ byte                   pad_0[0x28];
    /* 0x28 */ MineForkedTunnelViewB* field_28;
    /* 0x2C */ byte                   pad_2C[0x8];
    /* 0x34 */ MineForkedTunnelViewA* field_34;
} MineForkedTunnelSprtRec;
STATIC_ASSERT_SIZEOF(MineForkedTunnelSprtRec, 0x38);

extern GpGridParams D_mine_forked_tunnel_80181C5C;
extern GpGridParams D_mine_forked_tunnel_80183D70;

/// The tunnel's per-view effect anchors, projected by
/// `func_mine_forked_tunnel_8017E78C` with `func_mine_forked_tunnel_8017E504`
/// (half-extent 0x300). Views 2 and 3 share the first anchor, view 4 draws the
/// second and third (the tunnel fork's two arms) and view 5 the fourth.
extern SVECTOR D_mine_forked_tunnel_80183614[];
extern SVECTOR D_mine_forked_tunnel_8018361C[];
extern SVECTOR D_mine_forked_tunnel_8018362C[];

extern TaskDesc   D_mine_forked_tunnel_80183104;
extern GpMsgEntry D_mine_forked_tunnel_80181C80[];
extern s32        D_mine_forked_tunnel_801831AC;
extern s32        D_mine_forked_tunnel_801834F4;
extern u8         D_80062735;

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

void func_mine_forked_tunnel_8017D5E8(Task* arg0);
void func_mine_forked_tunnel_8017D724(Task* arg0);
void func_mine_forked_tunnel_8017DAB8(Task* arg0);
void func_mine_forked_tunnel_8017DC50(Task* arg0);
void func_mine_forked_tunnel_8017DC70(Task* arg0);
s32  func_mine_forked_tunnel_8017DC8C(Task* task, s32 arg1, RoomPlacement* placement, s32 arg3);
s32  func_mine_forked_tunnel_8017DD08(Task* task, s32 arg1, s32 mode, s32 arg3);
void func_mine_forked_tunnel_8017DE54(Task* task);
void func_mine_forked_tunnel_8017DF34(s32 arg0);
void func_mine_forked_tunnel_8017E1E8(Task* arg0);
void func_mine_forked_tunnel_8017E24C(Task* task);
void func_mine_forked_tunnel_8017E48C(s32 arg0);

/// State table of the tunnel's enemy task, indexed by `Task::state`: set-up,
/// the per-frame path walk, and the exit that releases the enemy.
const TaskFuncTable3 D_mine_forked_tunnel_8017D5C4 = {
    { func_mine_forked_tunnel_8017D5E8, func_mine_forked_tunnel_8017D724, func_mine_forked_tunnel_8017DC50 },
};

/// State table of the enemy's pitch-animated child, indexed by `Task::state`:
/// attach to the enemy, walk the pitch curve, and `taskKill`.
const TaskFuncTable3 D_mine_forked_tunnel_8017D5D0 = {
    { func_mine_forked_tunnel_8017DE54, func_mine_forked_tunnel_8017DAB8, taskKill },
};

/// State table of the room's message-driven task, indexed by `Task::state`:
/// set-up, an idle state, and `taskKill`.
const TaskFuncTable3 D_mine_forked_tunnel_8017D5DC = {
    { func_mine_forked_tunnel_8017E1E8, func_mine_forked_tunnel_8017E24C, taskKill },
};

void func_mine_forked_tunnel_8017D5E8(Task* arg0)
{
    MineForkedTunnelWork* work;
    RoomPlacement         placement;

    work = memCalloc(0x48, 0);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }

    arg0->work     = work;
    work->field_44 = -1;

    if (GameFlag_GetNibble(0x75) == 0) {
        placement.pos.vx = D_mine_forked_tunnel_80181244[0].vx;
        placement.pos.vy = D_mine_forked_tunnel_80181244[0].vy;
        placement.pos.vz = D_mine_forked_tunnel_80181244[0].vz;
        placement.rot.vx = D_mine_forked_tunnel_80180AC4[0].vx;
        placement.rot.vy = D_mine_forked_tunnel_80180AC4[0].vy;
        placement.rot.vz = D_mine_forked_tunnel_80180AC4[0].vz;
        func_mine_forked_tunnel_8017DC8C(arg0, 0x7D4, &placement, 0);
    } else {
        func_mine_forked_tunnel_8017DC8C(arg0, 0x7D4, &D_mine_forked_tunnel_80181BBC, 0);
    }

    func_mine_forked_tunnel_8017DD08(arg0, 0x7D5, 1, 0);
    func_mine_forked_tunnel_8017DC70(arg0);
    work->field_40 = Task_SpawnFromTable(&D_mine_forked_tunnel_80181B74, 1, 0, (s32)arg0);
    arg0->msgTable = &D_mine_forked_tunnel_80181B8C;
    func_mine_forked_tunnel_8017DF34(GameFlag_GetNibble(0x75));
    arg0->exitCallback = func_mine_forked_tunnel_8017DC50;
    arg0->state++;
}

void func_mine_forked_tunnel_8017D724(Task* arg0)
{
    TmdObject*    ext;
    RoomPlacement placement;
    VECTOR3       vec;

    ext = arg0->extra;

    if (arg0->spawnArg1 == 1 && arg0->killCountdown < 0x6E) {
        placement.pos.vx = D_mine_forked_tunnel_80181244[arg0->killCountdown].vx;
        placement.pos.vy = D_mine_forked_tunnel_80181244[arg0->killCountdown].vy;
        placement.pos.vz = D_mine_forked_tunnel_80181244[arg0->killCountdown].vz;
        placement.rot.vx = D_mine_forked_tunnel_80180AC4[arg0->killCountdown].vx;
        placement.rot.vy = D_mine_forked_tunnel_80180AC4[arg0->killCountdown].vy;
        placement.rot.vz = D_mine_forked_tunnel_80180AC4[arg0->killCountdown].vz;

        func_mine_forked_tunnel_8017DC8C(arg0, 0x7D4, &placement, 0);
        arg0->killCountdown++;
    }

    if (!(ext->flags & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)arg0->extra)->coords->workm.t, &vec) != 0) {
            Gp_DrawEffGroundQuad(&vec, 0x200, Gp_State1C->groundShade);
        }
        Gp_UpdateCoord(((TmdObject*)arg0->extra)->coords);
        func_800D7A9C(ext, (VECTOR*)((TmdObject*)arg0->extra)->coords->workm.t, 0, 3);
    }

    if (((MineForkedTunnelWork*)arg0->work)->field_44 >= 0) {
        if (((MineForkedTunnelWork*)arg0->work)->field_44 == 0) {
            Tmd_FreeBuffers(ext);
        }
        ((MineForkedTunnelWork*)arg0->work)->field_44--;
    }
}

/// Message 0x7DB handler for the tunnel's enemy (`D_mine_forked_tunnel_80181B8C`
/// routes the id here). Command 0 rewinds the enemy and its spawned child
/// (`spawnArg1` and `killCountdown` cleared on both) and drops it back on the
/// placement `func_mine_forked_tunnel_8017D5E8` uses while flag 0x75 is clear;
/// 1 starts only the child's pitch walk, 2 starts the enemy's own, and 3 puts
/// the enemy on `D_mine_forked_tunnel_80181BBC` - the placement
/// `func_mine_forked_tunnel_8017D5E8` uses while the flag is set - then
/// refreshes the flag-dependent state through
/// `func_mine_forked_tunnel_8017DF34` and rewinds the enemy again. `arg1` is
/// the message id, which nothing here reads.
///
/// The `do { } while (0)` around the last command is an allocator lever, not
/// logic (the `break` leaves it for the switch's own tail, so the two are
/// equivalent): `flow` weights each reference by the loop depth, and
/// local-alloc's quantity rank is built from those counts, so the wrapper -
/// and only the wrapper - lifts the six placement reads above the placement
/// pointer and gives `$v0` to the values instead of the address.
s32 func_mine_forked_tunnel_8017D8EC(Task* task, s32 arg1, RoomActorMsg* msg)
{
    RoomPlacement         placement;
    RoomPlacement*        place;
    RoomPlacement*        src;
    RoomCoord*            coord;
    MineForkedTunnelWork* work;

    switch (msg->command) {
        case 0:
            work                = task->work;
            task->spawnArg1     = 0;
            task->killCountdown = 0;
            if (work->field_40 != 0) {
                ((Task*)work->field_40)->spawnArg1     = 0;
                ((Task*)work->field_40)->killCountdown = 0;
            }
            placement.pos.vx = D_mine_forked_tunnel_80181244[0].vx;
            placement.pos.vy = D_mine_forked_tunnel_80181244[0].vy;
            placement.pos.vz = D_mine_forked_tunnel_80181244[0].vz;
            placement.rot.vx = D_mine_forked_tunnel_80180AC4[0].vx;
            placement.rot.vy = D_mine_forked_tunnel_80180AC4[0].vy;
            placement.rot.vz = D_mine_forked_tunnel_80180AC4[0].vz;

            place             = &placement;
            coord             = (RoomCoord*)((TmdObject*)task->extra)->coords;
            coord->coord.t[0] = place->pos.vx;
            coord->coord.t[1] = place->pos.vy;
            coord->coord.t[2] = place->pos.vz;
            coord->rot.vx     = place->rot.vx;
            coord->rot.vy     = place->rot.vy;
            coord->rot.vz     = place->rot.vz;
            RotMatrixZYX(&coord->rot, &coord->coord);
            coord->flg = 0;
            break;
        case 1:
            work = task->work;
            if (work->field_40 != 0) {
                ((Task*)work->field_40)->spawnArg1 = 1;
            }
            break;
        case 2:
            task->spawnArg1 = 1;
            break;
            do {
                case 3:
                    src               = &D_mine_forked_tunnel_80181BBC;
                    coord             = (RoomCoord*)((TmdObject*)task->extra)->coords;
                    coord->coord.t[0] = src->pos.vx;
                    coord->coord.t[1] = src->pos.vy;
                    coord->coord.t[2] = src->pos.vz;
                    coord->rot.vx     = src->rot.vx;
                    coord->rot.vy     = src->rot.vy;
                    coord->rot.vz     = src->rot.vz;
                    RotMatrixZYX(&coord->rot, &coord->coord);
                    coord->flg = 0;

                    func_mine_forked_tunnel_8017DF34(GameFlag_GetNibble(0x75));
                    task->spawnArg1 = 0;
                    break;
            } while (0);
    }
    return 0;
}

void func_mine_forked_tunnel_8017DAB8(Task* arg0)
{
    RoomPlacement  placement;
    RoomPlacement* place;
    RoomCoord*     coord;

    if (arg0->spawnArg1 == 1 && arg0->killCountdown < 0x36) {
        placement.pos.vx = D_mine_forked_tunnel_80181BA4.pos.vx;
        placement.pos.vy = D_mine_forked_tunnel_80181BA4.pos.vy;
        placement.pos.vz = D_mine_forked_tunnel_80181BA4.pos.vz;
        placement.rot.vx = D_mine_forked_tunnel_801819C4[arg0->killCountdown].vx;
        placement.rot.vy = D_mine_forked_tunnel_801819C4[arg0->killCountdown].vy;
        placement.rot.vz = D_mine_forked_tunnel_801819C4[arg0->killCountdown].vz;

        place             = &placement;
        coord             = (RoomCoord*)((TmdObject*)arg0->extra)->coords;
        coord->coord.t[0] = place->pos.vx;
        coord->coord.t[1] = place->pos.vy;
        coord->coord.t[2] = place->pos.vz;
        coord->rot.vx     = place->rot.vx;
        coord->rot.vy     = place->rot.vy;
        coord->rot.vz     = place->rot.vz;
        RotMatrixZYX(&coord->rot, &coord->coord);
        coord->flg = 0;

        arg0->killCountdown++;
    }
}

/// Dispatches the tunnel's enemy task through its three-state table (set-up,
/// path walk, exit), copied onto the stack first; nothing runs while
/// `Gp_StateF0.field_4` is non-zero.
void func_mine_forked_tunnel_8017DBE4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mine_forked_tunnel_8017D5C4;
    if (Gp_StateF0.field_4 == 0) {
        sp.funcs[task->state](task);
    }
}

void func_mine_forked_tunnel_8017DC50(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

void func_mine_forked_tunnel_8017DC70(Task* arg0)
{
    TmdObject*            ext;
    MineForkedTunnelWork* work;

    ext           = arg0->extra;
    work          = (MineForkedTunnelWork*)arg0->work;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}

/// Puts the task's model at `placement`: the position becomes the translation
/// of the `TmdObject`'s coordinate frame and the angles its rotation, rebuilt
/// with `RotMatrixZYX` and marked dirty. Shaped as a message handler; the room
/// calls it directly with id 0x7D4 in `arg1`, which it does not read.
s32 func_mine_forked_tunnel_8017DC8C(Task* task, s32 arg1, RoomPlacement* placement, s32 arg3)
{
    RoomCoord* coord;

    coord             = (RoomCoord*)((TmdObject*)task->extra)->coords;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->rot.vx     = placement->rot.vx;
    coord->rot.vy     = placement->rot.vy;
    coord->rot.vz     = placement->rot.vz;
    RotMatrixZYX(&coord->rot, &coord->coord);
    coord->flg = 0;
    return 0;
}

/// `Task::msgTable` handler for message id 0x7D5: switches the draw and
/// buffer-alloc bits of the task's `TmdObject` extra. Modes 0 and 1 set and
/// clear bit 0x80 - hiding and showing the model - and leave bit 0x4 clear so
/// the model keeps its buffers, mode 1 reinstating them through
/// `Tmd_AllocBuffers` first. Modes 2 and 3 set 0x4 instead, skipping that
/// allocation; mode 2 also stores itself in the work block's lifetime counter,
/// `MineForkedTunnelWork::field_44`, which `func_mine_forked_tunnel_8017D724`
/// counts down before freeing the child. Any other mode touches nothing and
/// reports 1.
s32 func_mine_forked_tunnel_8017DD08(Task* task, s32 arg1, s32 mode, s32 arg3)
{
    TmdObject* ext;
    s32        ret;

    ext = (TmdObject*)task->extra;
    ret = 0;
    switch (mode) {
        case 0:
            ext->flags |= 0x80;
            ext->flags &= ~4;
            break;
        case 1:
            ext->flags &= ~0x80;
            Tmd_AllocBuffers(ext);
            ext->flags &= ~4;
            break;
        case 2:
            ext->flags                                   |= 0x80;
            ((MineForkedTunnelWork*)task->work)->field_44 = mode;
            ext->flags                                   |= 4;
            break;
        case 3:
            ext->flags &= ~0x80;
            ext->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

/// Dispatches the enemy's pitch-animated child through its three-state table
/// (attach, pitch walk, `taskKill`), copied onto the stack first; nothing runs
/// while `Gp_StateF0.field_4` is non-zero.
void func_mine_forked_tunnel_8017DDE8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mine_forked_tunnel_8017D5D0;
    if (Gp_StateF0.field_4 == 0) {
        sp.funcs[task->state](task);
    }
}

/// Spawn state 0: adopt the parent task's model lighting - the light and colour
/// matrix pointers off the parent's `TmdObject` plus its coordinate as the
/// frame's parent link - then reparent onto that task, drop `field_C` bit 7 and
/// place the object at this room's `RoomPlacement`, rebuilding `coord` with
/// `RotMatrixZYX`.
void func_mine_forked_tunnel_8017DE54(Task* task)
{
    Task*          parent;
    TmdObject*     ext;
    TmdObject*     parentExt;
    GsCOORDINATE2* parentCoord;
    RoomCoord*     coord;
    RoomCoord*     dst;

    parent      = task->spawnArg2;
    ext         = task->extra;
    parentExt   = parent->extra;
    coord       = (RoomCoord*)ext->coords;
    parentCoord = parentExt->coords;

    coord->flg    = 0;
    coord->sub    = parentCoord;
    ext->lightMtx = parentExt->lightMtx;
    ext->colorMtx = parentExt->colorMtx;
    ext->otOffset = -1;
    Task_Reparent(parent, task);
    ext->flags = ext->flags & 0xFF7F;

    dst             = (RoomCoord*)((TmdObject*)task->extra)->coords;
    dst->coord.t[0] = D_mine_forked_tunnel_80181BA4.pos.vx;
    dst->coord.t[1] = D_mine_forked_tunnel_80181BA4.pos.vy;
    dst->coord.t[2] = D_mine_forked_tunnel_80181BA4.pos.vz;
    dst->rot.vx     = D_mine_forked_tunnel_80181BA4.rot.vx;
    dst->rot.vy     = D_mine_forked_tunnel_80181BA4.rot.vy;
    dst->rot.vz     = D_mine_forked_tunnel_80181BA4.rot.vz;
    RotMatrixZYX(&dst->rot, &dst->coord);
    dst->flg    = 0;
    task->state = task->state + 1;
}

/// Restores the room's layout lists from their template, then offsets the eight
/// `field_8` coordinates by (0, 0, -0xC8), or by (0, -0xBB8, -0xC8) when `arg0`
/// is non-zero. The callers pass game-flag nibble 0x75.
void func_mine_forked_tunnel_8017DF34(s32 arg0)
{
    GpGridParams* dst;
    GpGridParams* src;
    SVECTOR       d;
    s32           i;

    dst = &D_mine_forked_tunnel_80183D70;
    src = &D_mine_forked_tunnel_80181C5C;

    for (i = 0; i < 3; i++) {
        dst->field_4[i].vx = src->field_4[i].vx;
        dst->field_4[i].vy = src->field_4[i].vy;
        dst->field_4[i].vz = src->field_4[i].vz;
        dst->field_C[i]    = src->field_C[i];
    }

    for (i = 0; i < 8; i++) {
        dst->field_8[i].vx = src->field_8[i].vx;
        dst->field_8[i].vy = src->field_8[i].vy;
        dst->field_8[i].vz = src->field_8[i].vz;
    }

    if (arg0 == 0) {
        d.vx = 0;
        d.vy = 0;
        d.vz = -0xC8;
    } else {
        d.vy = -0xBB8;
        d.vx = 0;
        d.vz = -0xC8;
    }

    for (i = 0; i < 8; i++) {
        dst->field_8[i].vx += d.vx;
        dst->field_8[i].vy += d.vy;
        dst->field_8[i].vz += d.vz;
    }
}

s32 func_mine_forked_tunnel_8017E0E8(void)
{
    return 0;
}

/// Message handler that copies the incoming record onto the outgoing one and
/// forwards both to `func_80179A04`, returning 1.
s32 func_mine_forked_tunnel_8017E0F0(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    return 1;
}

s32 func_mine_forked_tunnel_8017E134(s32 arg0, s32 arg1, s32 arg2)
{
    if ((arg2 == 2) && (Gp_GetCurBit2Flag(1) == 1)) {
        if (GameFlag_GetNibble(0x152) == 0) {
            Gp_RunCapCmd1(5);
        } else {
            Task_SpawnFromTable(&D_mine_forked_tunnel_80183104, 1, 0, 0);
        }
    }
    return 0;
}

/// Message 1 handler: spawn the room's `Task_SpawnFromTable` entry when the
/// tunnel switch flag is still clear.
s32 func_mine_forked_tunnel_8017E19C(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    if ((arg2->field_2 == 1) && (GameFlag_GetNibble(0x75) == 0)) {
        Task_SpawnFromTable(&D_mine_forked_tunnel_80183104, 0, 0, 0);
    }
    return 0;
}

/// State 0 of the room's message-driven task family: park the room's
/// `GpMsgEntry` table in `Task::msgTable`, publish the task in pointer slot 7,
/// arm the message flag, then hand off to `func_mine_forked_tunnel_8017E48C`.
void func_mine_forked_tunnel_8017E1E8(Task* arg0)
{
    arg0->msgTable = D_mine_forked_tunnel_80181C80;
    Game_SetPtrSlot(arg0, 7);
    D_80062735 = 1;
    func_mine_forked_tunnel_8017E48C(Gp_GetCurBit2Flag(1) == 2);
    arg0->state = (s32)(arg0->state + 1);
}

/// State 1 of the room's message-driven task: does nothing.
void func_mine_forked_tunnel_8017E24C(Task* task)
{
    char pad[0x10];
}

/// Dispatches the room's message-driven task through its three-state table,
/// copied onto the stack before the call.
void func_mine_forked_tunnel_8017E25C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mine_forked_tunnel_8017D5DC;
    sp.funcs[task->state](task);
}

void func_mine_forked_tunnel_8017E2B4(void)
{
    SndEvt_EnqueueTypeA(0x54070005, 0, 0);
}

void func_mine_forked_tunnel_8017E2E0(Task* arg0)
{
    s32 state;

    state = arg0->state;
    switch (state) {
        case 0:
            Gp_RunCapCmd1(1);
            arg0->state = arg0->state + 1;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (Gp_GetCapEventKey() == state) {
                    func_800E8634((s32)&D_mine_forked_tunnel_801831AC, 0, (s32)&D_mine_forked_tunnel_801834F4);
                    GameFlag_SetNibble(0x75, 1);
                }
                taskKill(arg0);
            }
            break;
    }
}

void func_mine_forked_tunnel_8017E38C(Task* arg0)
{
    s32 state;
    s16 temp;

    state = arg0->state;
    switch (state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_StartCapSlot(2, 0, 0);
            arg0->state = arg0->state + 1;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                arg0->state = arg0->state + 1;
            }
            break;
        case 2:
            temp                = arg0->killCountdown + 1;
            arg0->killCountdown = temp;
            if (temp >= 0xB) {
                if (Gp_GetCapEventKey() == state) {
                    Gp_StartCapSlot(2, 0, 1);
                    func_mine_forked_tunnel_8017E48C(1);
                }
                Gp_MsgPlayerWeapon(1);
                taskKill(arg0);
            }
            break;
    }
}

void func_mine_forked_tunnel_8017E48C(s32 arg0)
{
    GpAreaKey*               sess;
    MineForkedTunnelSprtRec* rec;
    MineForkedTunnelViewB*   v28;
    MineForkedTunnelViewA*   v34;

    sess = &gGameSession->at4.loc;
    rec  = (MineForkedTunnelSprtRec*)Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1];

    if (!(arg0 & 0xFF)) {
        v28           = rec->field_28;
        v28->field_2C = 0;
        v34           = rec->field_34;
        v34->field_1C = 0;
        return;
    }
    v28           = rec->field_28;
    v28->field_2C = 1;
    v34           = rec->field_34;
    v34->field_1C = 1;
}

/// Draws one glow sprite at the world-space point `arg0`: projects it through
/// `Gfx_ViewWorldMtx` and, when `gte_stflg` is non-negative, queues one
/// semi-transparent `POLY_FT4` (tpage 0x2B, clut `(arg1 & 0x3F) | 0x4380`).
/// `arg1` selects the 40-texel UV column `(s16)arg1 * 40` at v=0..0x27. `arg2`
/// is a signed half-extent; the on-screen radius is `(s16)arg2 * 39 / otz`. RGB
/// is the frame-counter blend byte `((animFrame & 1) * 16) + 0x20` on all three
/// channels, so the sprite flickers between two brightnesses.
void func_mine_forked_tunnel_8017E504(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    u8*                tmp;
    RoomDraw13Scratch* block;
    POLY_FT4*          prim;
    DisplayState*      ds;
    s32                tex;
    s32                idx;
    s32                u0;
    s32                u1;
    register s32       sarg asm("v1");
    s32                t;
    s32                field8;
    s32                blend;
    s32                v;
    u8                 code;
    s16                xy;

    tex = arg1;
    CLOBBER_REG(a1);
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    tmp     = head - 0x10;
    block   = (RoomDraw13Scratch*)tmp;
    SOFT_TOUCH_REG(block);
    *scratch = tmp;

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (((RoomDraw13Scratch*)tmp)->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        ds             = &gDisplayState;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        idx         = (s16)tex;
        field8      = (u8)ds->animFrame;
        prim->tpage = 0x2B;
        prim->clut  = (idx & 0x3F) | 0x4380;
        u0          = idx * 40;
        u1          = u0 + 0x27;
        prim->u0    = u0;
        prim->u2    = u0;
        SOFT_USE_REG(u0);
        v        = 0x27;
        prim->u1 = u1;
        prim->u3 = u1;
        SOFT_USE_REG(u1);
        sarg     = arg2 << 16;
        prim->v2 = v;
        prim->v3 = v;
        SCHED_BARRIER();
        code     = prim->code;
        sarg     = sarg >> 16;
        prim->v0 = 0;
        prim->v1 = 0;
        blend    = ((field8 & 1) * 16) + 0x20;
        COMPILER_BARRIER();
        prim->code = code | 2;
        t          = sarg * 40;
        setRGB0(prim, blend, blend, blend);
        ((RoomDraw13Scratch*)tmp)->radius =
            (t - sarg) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        xy       = *(u16*)&((RoomDraw13Scratch*)tmp)->sx - *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->x2 = xy;
        prim->x0 = xy;
        xy       = *(u16*)&((RoomDraw13Scratch*)tmp)->sx + *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->x3 = xy;
        prim->x1 = xy;
        xy       = *(u16*)&((RoomDraw13Scratch*)tmp)->sy - *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->y1 = xy;
        prim->y0 = xy;
        xy       = *(u16*)&((RoomDraw13Scratch*)tmp)->sy + *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->y3 = xy;
        prim->y2 = xy;
        addPrim((u_long*)(((((u32)((RoomDraw13Scratch*)(head - 0x10))->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x10;
}

/// Room effect tick. Marks the effect state (`field_A` = 2, the value
/// `actor_400100_text` and `Gp_EffCtlTaskAC` test) and projects the light
/// anchor belonging to the camera's view index, so the fork's light follows
/// whichever branch the player is looking down.
void func_mine_forked_tunnel_8017E78C(void)
{
    s32 idx;

    Gp_State1C->roomEffectMode = 2;
    idx                        = Gp_GetViewIndex() & 0xFF;

    switch (idx) {
        case 2:
        case 3:
            func_mine_forked_tunnel_8017E504(D_mine_forked_tunnel_80183614, 1, 0x300);
            break;
        case 4:
            func_mine_forked_tunnel_8017E504(&D_mine_forked_tunnel_8018361C[0], 1, 0x300);
            func_mine_forked_tunnel_8017E504(&D_mine_forked_tunnel_8018361C[1], 1, 0x300);
            break;
        case 5:
            func_mine_forked_tunnel_8017E504(D_mine_forked_tunnel_8018362C, 1, 0x300);
            break;
        default:
            return;
    }
}
