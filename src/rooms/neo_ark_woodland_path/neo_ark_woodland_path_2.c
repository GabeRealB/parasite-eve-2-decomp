#include "rooms/neo_ark_woodland_path.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/stdio.h>

#include "common.h"
#include "gte.h"

#include "neo_ark_woodland_path_private.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/collision.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/enemy_params.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
/// Empty presence flag so `water_effects.h` declares the shared
/// `waterDrawSpinU16` and `waterDrawTileU16`. This file includes their bodies.
#define WATER_SHARED_U16_DRAWERS
#include "../../shared/water_effects.h"
#include "../../shared/falling_leaves.h"

#define ABS_DIFF(a, b) ((a) - (b) >= 0 ? (a) - (b) : (b) - (a))

/// The distance between `a` and `b`, spelled as a conditional subtraction.

/// The room's frame countdown at `D_neo_ark_woodland_path_8018498E`. Signed,
/// although the arithmetic reads compile as `lhu` (`func_...8018154C` adds to
/// it, `func_...80180DDC` counts it down): a load whose result is truncated by
/// the following `sh` only has to supply the low half, so GCC picks the
/// unsigned form by itself, while the `lh` comparisons and the -1 the 0x7DB
/// handler stores need the signed declaration. A union offering both views
/// compiles the same instructions but marks every access `in_struct`, and that
/// flag decides the scheduler's dependence analysis - it pinned a load after a
/// store in `func_neo_ark_woodland_path_80180C6C`. See
/// `DECOMPILATION_LEARNINGS.md`, "A union that only names a view costs
/// `in_struct`".
extern s16 D_neo_ark_woodland_path_8018498E;

extern ActorCommand D_neo_ark_woodland_path_80184A5C;

/// The room's five spawn slots: `func_neo_ark_woodland_path_8018046C` fills the
/// first free one with a countdown and `func_neo_ark_woodland_path_80180B18`
/// hands slot 0 to the spawn it triggers and clears it. Read as `lhu` by the
/// handler and as `lh` by the slot filler, so each site names the view it uses
/// (`[0]` here, an `s16*` cast there).
extern u16 D_neo_ark_woodland_path_80184A60[5];

/// Ceiling `func_neo_ark_woodland_path_8018046C` clamps a spawn slot to
/// (0x1A4, 420 frames). Only the first halfword is this unit's; the run
/// continues into the room's parameter block, so the extent is splat's.

/// The same run reached through its leading label, which is how
/// `func_neo_ark_woodland_path_80180C6C` reads the ceiling: element 2 is
/// `D_...8494C[0]`, 420 frames. splat names both addresses because the compiled
/// code names both, and the two are different code - an index keeps this
/// symbol in a register and takes the offset as the load's displacement, while
/// naming `D_...8494C` addresses it directly.
extern EnemyParams  D_neo_ark_woodland_path_80184948;
extern DamageAttack D_neo_ark_woodland_path_80184930[6];

/// The room's arming count, packed into game flag 0x10A as a nibble:
/// `func_neo_ark_woodland_path_80180C6C` adds the slot's spawn count to it and
/// then caps it at 5, the number of slots `D_...84A60` has. Signed, though the
/// add reads it as `lhu` - the result is truncated by the following `sh`, so
/// only the low half matters and GCC picks the unsigned load by itself.
extern s16 D_neo_ark_woodland_path_80184990;

/// How many spawns each slot arms, indexed by `gGameSession->location.loc.variant` (the
/// slot the session is in): the byte `func_...80180C6C` adds to
/// `D_...80184990`, and the gate `func_...80180DDC` tests against zero.
extern u8 D_neo_ark_woodland_path_80184970[];

/// The message-handler table `func_neo_ark_woodland_path_80180C6C` parks in
/// `Task::msgTable`: a placement request (0x13EF,
/// `func_neo_ark_woodland_path_80181568`), a countdown bump (0x13F4) and the
/// 0x7DB command handler `func_neo_ark_woodland_path_80180B18`.
extern GpMsgEntry D_neo_ark_woodland_path_801849F4[];

/// The same gate for the arm-state one step earlier: `func_...80180568` tests
/// it against zero and `func_...801806D8` reads the slot's count from it. One
/// byte per session slot, indexed by `gGameSession->location.loc.variant`, like
/// `D_...84970` above.
extern u8 D_neo_ark_woodland_path_80184980[];

/// `func_...80180568`'s own message-handler table, parked in `Task::msgTable`
/// as `D_...849F4` is by `func_...80180C6C`: the same three ids, answered by
/// the placement request `func_neo_ark_woodland_path_8018147C`, the spawn-slot
/// filler `func_neo_ark_woodland_path_8018046C` and a 0x7DB handler that
/// ignores the message.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
typedef struct {
    s32 id;
    union {
        s32  (*call0)(Task*, s32, TaskMessageArg, TaskMessageArg);
        s32  (*call1)(Task*, s32, u8*, TaskMessageArg);
        void (*call2)(Task*, s32, s32);
    } handler;
} NeoArkWoodlandPath2MsgEntry;
STATIC_ASSERT_SIZEOF(NeoArkWoodlandPath2MsgEntry, 8);

extern NeoArkWoodlandPath2MsgEntry D_neo_ark_woodland_path_80184998[];

/// Set once a spawn slot has been armed, read by the room's other states.
extern s16 D_neo_ark_woodland_path_80184996;

/// Spawn point requested by the room's 0x13EF handlers (the message's third
/// byte, taken only while `D_...8498E` has run out and the byte differs from
/// the previous request). One-based index into the placement table of the
/// running sequence; zero means no request, and the per-frame states clear it
/// every frame whether or not they placed a spawn.
extern s16 D_neo_ark_woodland_path_80184992;

/// The byte the last 0x13EF message carried, kept so that a repeated request
/// is dropped rather than placed again.
extern s16 D_neo_ark_woodland_path_80184994;

/// A placement `func_...801806D8` puts a spawned task at: the x and z it writes
/// into the task's coordinate translation (y is always zero) and the Y
/// rotation it hands `Gfx_RotMatrixY`. The halfword after `x` is not read.
typedef struct NeoArkWoodlandPathSpawnPos {
    s16 x;
    s16 pad_2;
    s16 z;
    s16 rotY;
} NeoArkWoodlandPathSpawnPos;

/// The room's spawn placements, indexed by `D_...80184992 - 1`.
extern NeoArkWoodlandPathSpawnPos D_neo_ark_woodland_path_801849B8[];

/// The second arming sequence's spawn placements, which
/// `func_...80180DDC` picks from by `D_...80184992 - 1`. Five of them; any
/// request past the fourth takes the last.
extern NeoArkWoodlandPathSpawnPos D_neo_ark_woodland_path_80184A14[5];

/// `Gp_StateF0.field_6` as `func_...801806D8` saw it on the previous frame, so
/// that it can tell the reference count was non-zero before the frame began.
extern s16 D_neo_ark_woodland_path_801849F0;

/// The object `Task::spawnArg2` holds for the task that runs
/// `func_neo_ark_woodland_path_8017EA08`. Only the halfword at 0x26 is known:
/// the chance, out of 0x200, of spawning an effect this frame. It is recomputed
/// from how far the tracked model parts moved. Nothing yet shows whether this
/// is the same object as `GpEnemy`.
typedef struct NeoArkWoodlandPathTrailObj {
    /* 0x00 */ byte pad_0[0x26];
    /* 0x26 */ s16  chance;
} NeoArkWoodlandPathTrailObj;

s32  func_neo_ark_woodland_path_80180B18(Task*, s32, TaskMessageArg, TaskMessageArg);
s32  func_neo_ark_woodland_path_80181474(Task*, s32, TaskMessageArg, TaskMessageArg);
s32  func_neo_ark_woodland_path_8018147C(Task*, s32, u8*, TaskMessageArg);
s32  func_neo_ark_woodland_path_8018154C(Task*, s32, TaskMessageArg, TaskMessageArg);
s32  func_neo_ark_woodland_path_80181568(Task*, s32, u8*, TaskMessageArg);
void func_neo_ark_woodland_path_8018046C(Task*, s32, s32);

void func_neo_ark_woodland_path_801814E8(Task*);
void func_neo_ark_woodland_path_801815D4(Task*);

DamageAttack D_neo_ark_woodland_path_80184930[6] = {
    { 30, 7 },
    { 30, 7 },
    { 50, 7 },
    { 50, 7 },
    { 40, 0 },
    { 40, 0 },
};

EnemyParams D_neo_ark_woodland_path_80184948 = { D_neo_ark_woodland_path_80184930, 420, 115, 200, 5, 100, 10, 100, 10 };

// Retained numeric records following the enemy parameters.
u16 D_neo_ark_woodland_path_80184958[3][4] = {
    { 0, 900, 3, 0 },
    { 0, 800, 5, 0 },
    { 0, 500, 7, 0 },
};

u8 D_neo_ark_woodland_path_80184970[16] = {
    0,
    1,
    3,
    2,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

u8 D_neo_ark_woodland_path_80184980[14] = {
    0,
    3,
    2,
    4,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

s16 D_neo_ark_woodland_path_8018498E = 30;

s16 D_neo_ark_woodland_path_80184990 = 0;

s16 D_neo_ark_woodland_path_80184992 = 0;

s16 D_neo_ark_woodland_path_80184994 = 0;

s16 D_neo_ark_woodland_path_80184996 = 0;

NeoArkWoodlandPath2MsgEntry D_neo_ark_woodland_path_80184998[4] = {
    { 5103, { .call0 = func_neo_ark_woodland_path_8018147C } },
    { 5108, { .call2 = func_neo_ark_woodland_path_8018046C } },
    { 2011, { .call0 = func_neo_ark_woodland_path_80181474 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

NeoArkWoodlandPathSpawnPos D_neo_ark_woodland_path_801849B8[7] = {
    { -2000, 0, 8977, -1024 },
    { 379, 0, 7700, 2048 },
    { 7950, 0, 4650, -1024 },
    { 7950, 0, -4650, -1024 },
    { -633, 0, -1000, 2048 },
    { -2280, 0, -6378, -1024 },
    { 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF },
};

s16 D_neo_ark_woodland_path_801849F0 = 0;

GpMsgEntry D_neo_ark_woodland_path_801849F4[4] = {
    { 5103, func_neo_ark_woodland_path_80181568 },
    { 5108, func_neo_ark_woodland_path_8018154C },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_neo_ark_woodland_path_80180B18 },
    { 0x7FFFFFFF, NULL },
};

NeoArkWoodlandPathSpawnPos D_neo_ark_woodland_path_80184A14[5] = {
    { 8884, 0, 2200, 2048 },
    { 0, 0, -2000, 200 },
    { -4200, 0, -3000, 0 },
    { -4100, 0, 2000, 1900 },
    { -6500, 0, 2000, 2200 },
};

s16 D_neo_ark_woodland_path_80184A3C[4] = {
    0x7FFF,
    0x7FFF,
    0x7FFF,
    0x7FFF,
};

TaskDesc D_neo_ark_woodland_path_80184A44[2] = {
    { 0, 32, func_neo_ark_woodland_path_801815D4, { .model = NULL } },
    { 0, 32, func_neo_ark_woodland_path_801814E8, { .model = NULL } },
};

ActorCommand D_neo_ark_woodland_path_80184A5C = { { .loc = { 0, 0 } }, 0 };

u16 D_neo_ark_woodland_path_80184A60[5];

static void func_neo_ark_woodland_path_801814D4(Task* arg0);

static void func_neo_ark_woodland_path_801815C0(Task* arg0);

static void func_neo_ark_woodland_path_80180568(Task* task);
static void func_neo_ark_woodland_path_801806D8(Task* task);
static void func_neo_ark_woodland_path_80180C6C(Task* task);
static void func_neo_ark_woodland_path_80180DDC(Task* task);

/// Scatters effects around the slot-3 task's model while its root coordinate
/// is at a y of 0x12C or more (y grows downward) and no event is running.
/// Once per frame, the spawn chance is set from how far model parts 15 and 18
/// moved since the previous frame. Two effects are rolled at the model's x and
/// z with y fixed at 0xC8: effect `D_8011574C` against that chance, then effect
/// `D_80115738` against the chance less 0x20. The same function also sets the
/// room effect mode to 2 while the root y is below 0x11. On its first run it
/// stores the two effect ids and the starting part positions.
void func_neo_ark_woodland_path_8017EA08(Task* task)
{
    NeoArkWoodlandPathTrailObj* obj;
    Task*                       owner;
    GfxCoord*                   root;
    GfxCoord*                   part;
    GfxCoord                    coord;
    s32                         i;

    obj   = task->spawnArg2.pointer;
    owner = gameGetPtrSlot(3);
    root  = owner->extra.tmd->coords;
    if (task->state == 0) {
        D_8011574C  = 0x60058;
        D_80115738  = 0x60187;
        task->state = 1;
        for (i = 0; i < 2; i++) {
            part                                   = &owner->extra.tmd->coords[i * 3 + 15];
            D_neo_ark_woodland_path_80181684[i].vx = part->workm.t[0];
            D_neo_ark_woodland_path_80181684[i].vy = part->workm.t[1];
            D_neo_ark_woodland_path_80181684[i].vz = part->workm.t[2];
        }
    }
    gRoomEffectState->roomEffectMode = (root->coord.t[1] < 0x11) * ROOM_EFFECT_VIEW_ENABLED;
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING && root->coord.t[1] >= 0x12C) {
        for (i = 0; i < 2; i++) {
            part               = &owner->extra.tmd->coords[i * 3 + 15];
            obj->chance        = ABS_DIFF(D_neo_ark_woodland_path_80181684[i].vx, part->workm.t[0]) + ABS_DIFF(D_neo_ark_woodland_path_80181684[i].vy, part->workm.t[1]) + ABS_DIFF(D_neo_ark_woodland_path_80181684[i].vz, part->workm.t[2]) + 0x20;
            coord.parent       = root->parent;
            coord.coord        = root->coord;
            coord.coord.t[0]   = root->coord.t[0];
            coord.coord.t[1]   = 0xC8;
            coord.coord.t[2]   = root->coord.t[2];
            coord.composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(&coord);
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((s32)((Gp_LcgState >> 16) & 0x1FF) < obj->chance) {
                Gp_SpawnEff(D_8011574C, &coord, 0x40, 0);
            }
            obj->chance -= 0x20;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            if ((s32)((Gp_LcgState >> 16) & 0x1FF) < obj->chance) {
                Gp_SpawnEff(D_80115738, &coord, 0x1202180, 0);
            }
            D_neo_ark_woodland_path_80181684[i].vx = part->workm.t[0];
            D_neo_ark_woodland_path_80181684[i].vy = part->workm.t[1];
            D_neo_ark_woodland_path_80181684[i].vz = part->workm.t[2];
        }
    }
}

#include "../../shared/falling_leaves_task.inc.c"

/// The room's falling-leaf task, named by gameplay's effect table.
void func_neo_ark_woodland_path_8017ED00(Task* task)
{
    leafFallTask(task);
}

/// Draws one mote of the room's ambient effect: the unit quad `D_80111E38`
/// scaled by `arg1`, rotated by the mote's own coordinate and offset by its
/// world translation, then projected through `GsWSMATRIX` (the first corner
/// with `rtps`, the other three with `rtpt`) in a 0x38-byte scratch stack
/// block. When the GTE flag is non-negative it queues one `POLY_FT4` (tpage
/// 0x2B, clut 0x4390, an 8x8 texel tile at 0,0x28). `arg2` is the fade level:
/// zero draws the raw texture, otherwise the quad is semi-transparent and
/// modulated by the grey `(arg2, arg2, arg2)`.
void leafDraw(GfxCoord* arg0, s32 arg1, s16 arg2)
{
    GpQuadScratch* block;
    s32            i;
    POLY_FT4*      prim;

    block = SCRATCH_STACK_RESERVE_BLOCK(GpQuadScratch);
    for (i = 0; i < 4; i++) {
        block->vec[i].vx = D_80111E38[i].x * arg1;
        block->vec[i].vy = 0;
        block->vec[i].vz = D_80111E38[i].y * arg1;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->vec[i]);
        gte_rtv0();
        gte_stsv(&block->vec[i]);
        block->vec[i].vx += arg0->workm.t[0];
        block->vec[i].vy += arg0->workm.t[1];
        block->vec[i].vz += arg0->workm.t[2];
    }

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps();
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        if (arg2 != 0) {
            setSemiTrans(prim, 1);
            setRGB0(prim, arg2, arg2, arg2);
        } else {
            setShadeTex(prim, 1);
        }
        prim->tpage = 0x2B;
        prim->clut  = 0x4390;
        prim->v0    = 0x28;
        prim->v1    = 0x28;
        prim->u0    = 0;
        prim->u1    = 7;
        prim->u2    = 0;
        prim->v2    = 0x2F;
        prim->u3    = 7;
        prim->v3    = 0x2F;
        prim->x0    = block->sxy0.vx;
        prim->y0    = block->sxy0.vy;
        prim->x1    = block->sxy1.vx;
        prim->y1    = block->sxy1.vy;
        prim->x2    = block->sxy2.vx;
        prim->y2    = block->sxy2.vy;
        prim->x3    = block->sxy3.vx;
        prim->y3    = block->sxy3.vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GpQuadScratch);
}

#include "../../shared/water_ripple_task.inc.c"

void func_neo_ark_woodland_path_8017F4A0(Task* task)
{
    waterRippleTask(task);
}

#include "../../shared/water_splash.inc.c"

#include "../../shared/water_drift_task_u16.inc.c"

void func_neo_ark_woodland_path_8017F928(Task* task)
{
    waterDriftTaskU16(task);
}

#include "../../shared/water_spin_u16.inc.c"

#include "../../shared/water_tile_u16.inc.c"

void func_neo_ark_woodland_path_8018046C(Task* task, s32 arg1, s32 arg2)
{
    s16 i;
    s16 v;

    if (arg2 > 0) {
        for (i = 0; i < 5; i++) {
            if (((s16*)D_neo_ark_woodland_path_80184A60)[i] == 0) {
                v                                           = arg2 * 0x6E / 100;
                ((s16*)D_neo_ark_woodland_path_80184A60)[i] = v;
                if (D_neo_ark_woodland_path_80184948.hpMax < v) {
                    ((s16*)D_neo_ark_woodland_path_80184A60)[i] = D_neo_ark_woodland_path_80184948.hpMax;
                }
                if (Gp_StateF0.field_6 >= 2) {
                    Gp_ReleaseStateF0(task, 0xD);
                } else {
                    D_neo_ark_woodland_path_80184996 = 1;
                }
                D_neo_ark_woodland_path_8018498E += 0x5A;
                return;
            }
        }
        return;
    }
    D_neo_ark_woodland_path_8018498E += 0x5A;
}

/// Arming state, the sibling of `func_neo_ark_woodland_path_80180C6C` one step
/// earlier in the sequence: it parks its own 0x7DB handler table in the task,
/// folds the slot's spawn count into game flag 0x10C (remembering the slot in
/// 0x10D) and fills the five spawn slots with the room's ceiling - or zero.
/// Same shape as its sibling; only the flags, the slot-count array and the
/// handler table differ.
static void func_neo_ark_woodland_path_80180568(Task* task)
{
    s16 i;
    s16 nib;

    if (D_neo_ark_woodland_path_80184980[gGameSession->location.loc.variant] == 0) {
        task->msgTable = NULL;
        task->state    = task->state + 1;
        return;
    }
    task->msgTable                   = D_neo_ark_woodland_path_80184998;
    D_neo_ark_woodland_path_80184990 = GameFlag_GetNibble(0x10C);
    nib                              = GameFlag_GetNibble(0x10D);
    if (gGameSession->location.loc.variant != nib) {
        D_neo_ark_woodland_path_80184990 = D_neo_ark_woodland_path_80184990 + D_neo_ark_woodland_path_80184980[gGameSession->location.loc.variant];
        GameFlag_SetNibble(0x10C, D_neo_ark_woodland_path_80184990);
        GameFlag_SetNibble(0x10D, gGameSession->location.loc.variant);
    }
    if (D_neo_ark_woodland_path_80184990 >= 6) {
        D_neo_ark_woodland_path_80184990 = 5;
    }
    for (i = 0; i < 5; i++) {
        if (i < D_neo_ark_woodland_path_80184990) {
            D_neo_ark_woodland_path_80184A60[i] = D_neo_ark_woodland_path_80184948.hpMax;
        } else {
            D_neo_ark_woodland_path_80184A60[i] = 0;
        }
    }
    D_neo_ark_woodland_path_8018498E = 0x5A;
    task->state                      = task->state + 1;
}

/// Per-frame state of the arming sequence `func_...80180568` sets up: counts
/// the room's countdown down, and once the reference count on `Gp_StateF0`
/// has dropped to zero folds the still-pending spawn slots back into game
/// flags 0x168 and 0x10C. When a spawn point has been requested it hands the
/// first pending slot to a waiting slot-4 task (one whose enemy `hp` still reads -999),
/// sends it the 0x7DB message and places it at that point.
static void func_neo_ark_woodland_path_801806D8(Task* task)
{
    s16      i;
    s16      count;
    s32      a;
    s32      b;
    GpEnemy* obj;
    s16      j;
    s16      k;

    gameGetPtrSlot(3);
    if (D_neo_ark_woodland_path_80184980[gGameSession->location.loc.variant] == 0) {
        return;
    }
    if (D_neo_ark_woodland_path_8018498E > 0) {
        D_neo_ark_woodland_path_8018498E--;
    }
    if (D_neo_ark_woodland_path_80184996 == 1 && Gp_StateF0.field_6 >= 2) {
        D_neo_ark_woodland_path_80184996 = 0;
        Gp_ReleaseStateF0(task, 0xD);
    }
    if (Gp_StateF0.field_6 == 0 && D_neo_ark_woodland_path_801849F0 > 0) {
        D_neo_ark_woodland_path_8018498E = 0x96;
        a                                = GameFlag_GetNibble(0x168);
        b                                = GameFlag_GetNibble(0x10C);
        count                            = 0;
        for (k = 0; k < 5; k++) {
            if (((s16*)D_neo_ark_woodland_path_80184A60)[k] > 0) {
                count++;
            }
        }
        GameFlag_SetNibble(0x168, a + (b - count));
        count = 0;
        for (k = 0; k < 5; k++) {
            if (((s16*)D_neo_ark_woodland_path_80184A60)[k] > 0) {
                count++;
            }
        }
        GameFlag_SetNibble(0x10C, count);
        areaSyncLocationVariant(&gGameSession->location.loc);
    }
    D_neo_ark_woodland_path_801849F0 = Gp_StateF0.field_6;
    if (gGameSession->battleResetPending == 1 && D_neo_ark_woodland_path_8018498E == 0) {
        Gp_StateF0.prefix.bytes.field_0  = 0;
        Gp_StateF0.field_5               = 0;
        Gp_StateF0.field_6               = 0;
        Gp_StateF0.field_8               = 0;
        Gp_StateF0.field_C               = 0;
        Gp_StateF0.field_10              = 0;
        gGameSession->battleResetPending = 0;
    }
    if (Gp_StateF0.prefix.bytes.field_0 != 2 && D_neo_ark_woodland_path_80184992 != 0) {
        D_neo_ark_woodland_path_80184A5C.context.loc.stage = 5;
        D_neo_ark_woodland_path_80184A5C.context.loc.area  = 0x1D;
        D_neo_ark_woodland_path_80184A5C.command           = 0xB;
        for (i = 0; i < 2; i++) {
            if (Gp_LookupSlot4(i) == 0) {
                break;
            }
            obj = Gp_LookupSlot4(i)->spawnArg2.pointer;
            if (obj == NULL) {
                break;
            }
            if (obj->hp == -999) {
                for (j = 0; j < D_neo_ark_woodland_path_80184990; j++) {
                    if (((s16*)D_neo_ark_woodland_path_80184A60)[j] > 0) {
                        obj->hp                             = D_neo_ark_woodland_path_80184A60[j];
                        obj->reactionFlags                  = 0;
                        D_neo_ark_woodland_path_80184A60[j] = 0;
                        break;
                    }
                }
                if (obj->hp > 0) {
                    Gp_IncStateF0Ref(0);
                    D_neo_ark_woodland_path_8018498E += 0x5A;
                    Gp_DispatchMsgPtr(Gp_LookupSlot4(i), ACTOR_COMMAND_MESSAGE_APPLY, &D_neo_ark_woodland_path_80184A5C, 0);
                    Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[0]   = D_neo_ark_woodland_path_801849B8[D_neo_ark_woodland_path_80184992 - 1].x;
                    Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[1]   = 0;
                    Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[2]   = D_neo_ark_woodland_path_801849B8[D_neo_ark_woodland_path_80184992 - 1].z;
                    Gp_LookupSlot4(i)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gfx_RotMatrixY(&Gp_LookupSlot4(i)->extra.tmd->coords->coord,
                                   D_neo_ark_woodland_path_801849B8[D_neo_ark_woodland_path_80184992 - 1].rotY, 1);
                }
                break;
            }
        }
    }
    D_neo_ark_woodland_path_80184992 = 0;
}

/// State handlers of the first arming sequence's entry task
/// `func_neo_ark_woodland_path_801814E8`: arm, run, advance, then kill.
static const TaskFuncTable4 D_neo_ark_woodland_path_8017D638 = {
    { func_neo_ark_woodland_path_80180568, func_neo_ark_woodland_path_801806D8,
      func_neo_ark_woodland_path_801814D4, taskKill }
};

s32 func_neo_ark_woodland_path_80180B18(Task* task, s32 arg1, TaskMessageArg msg, TaskMessageArg arg3)
{
    s32      result;
    u16      cmd;
    GpEnemy* obj;

    result = 0;
    if (msg.command->context.key == 0xB05) {
        cmd = msg.command->command;
        switch (cmd) {
            case 0:
                D_neo_ark_woodland_path_8018498E = -1;
                result                           = 0;
                return result;
            case 2:
                D_neo_ark_woodland_path_80184A5C.context.loc.stage = 5;
                D_neo_ark_woodland_path_80184A5C.context.loc.area  = 0xB;
                D_neo_ark_woodland_path_80184A5C.command           = 0xC;
                result                                             = 1;
                if (Gp_LookupSlot4(0) != 0) {
                    Gp_DispatchMsgPtr(Gp_LookupSlot4(0), ACTOR_COMMAND_MESSAGE_APPLY,
                                      &D_neo_ark_woodland_path_80184A5C, 0);
                    obj                                              = Gp_LookupSlot4(0)->spawnArg2.pointer;
                    Gp_LookupSlot4(0)->extra.tmd->coords->coord.t[0] = 5;
                    Gp_LookupSlot4(0)->extra.tmd->coords->coord.t[1] = 0;
                    Gp_LookupSlot4(0)->extra.tmd->coords->coord.t[2] = -0x320;
                    if (obj != 0) {
                        obj->hp                             = D_neo_ark_woodland_path_80184A60[0];
                        D_neo_ark_woodland_path_80184A60[0] = 0;
                        obj->reactionFlags                  = 0;
                    }
                    Gfx_RotMatrixY(&Gp_LookupSlot4(0)->extra.tmd->coords->coord,
                                   0x400, 1);
                    D_neo_ark_woodland_path_8018498E = 0x5A;
                }
                return result;
            default:
                return 0;
        }
    } else {
        return result;
    }
}

/// Arming state: with no spawns to arm for the session's slot it only advances;
/// otherwise it parks this room's 0x7DB handler table in the task, folds the
/// slot's spawn count into game flag 0x10A (remembering the slot in 0x10B), and
/// fills the five spawn slots with the room's ceiling - or zero.
static void func_neo_ark_woodland_path_80180C6C(Task* task)
{
    s16 i;
    s16 nib;

    if (D_neo_ark_woodland_path_80184970[gGameSession->location.loc.variant] == 0) {
        task->msgTable = NULL;
        task->state    = task->state + 1;
        return;
    }
    task->msgTable                   = D_neo_ark_woodland_path_801849F4;
    D_neo_ark_woodland_path_80184990 = GameFlag_GetNibble(0x10A);
    nib                              = GameFlag_GetNibble(0x10B);
    if (gGameSession->location.loc.variant != nib) {
        D_neo_ark_woodland_path_80184990 = D_neo_ark_woodland_path_80184990 + D_neo_ark_woodland_path_80184970[gGameSession->location.loc.variant];
        GameFlag_SetNibble(0x10A, D_neo_ark_woodland_path_80184990);
        GameFlag_SetNibble(0x10B, gGameSession->location.loc.variant);
    }
    if (D_neo_ark_woodland_path_80184990 >= 6) {
        D_neo_ark_woodland_path_80184990 = 5;
    }
    for (i = 0; i < 5; i++) {
        if (i < D_neo_ark_woodland_path_80184990) {
            D_neo_ark_woodland_path_80184A60[i] = D_neo_ark_woodland_path_80184948.hpMax;
        } else {
            D_neo_ark_woodland_path_80184A60[i] = 0;
        }
    }
    D_neo_ark_woodland_path_8018498E = 0x5A;
    task->state                      = task->state + 1;
}

/// Per-frame state of the arming sequence `func_...80180C6C` sets up, the
/// sibling of `func_...801806D8`: counts the room's countdown down, and once the
/// reference count on `Gp_StateF0` has dropped to zero folds the still-pending
/// spawn slots back into game flags 0x167 and 0x10A. When a spawn point has
/// been requested it hands the first pending slot to a waiting slot-4 task,
/// sends it the 0x7DB message and places it at one of five fixed points.
static void func_neo_ark_woodland_path_80180DDC(Task* task)
{
    s16      i;
    s16      count;
    s32      a;
    s32      b;
    GpEnemy* obj;
    s16      j;
    s16      k;

    gameGetPtrSlot(3);
    if (D_neo_ark_woodland_path_80184970[gGameSession->location.loc.variant] == 0) {
        return;
    }
    if (D_neo_ark_woodland_path_8018498E > 0) {
        D_neo_ark_woodland_path_8018498E--;
    }
    if (Gp_StateF0.field_6 == 0 && D_neo_ark_woodland_path_801849F0 > 0) {
        b     = GameFlag_GetNibble(0x10A);
        count = 0;
        for (k = 0; k < 5; k++) {
            if (((s16*)D_neo_ark_woodland_path_80184A60)[k] > 0) {
                count++;
            }
        }
        printf("(get_flag(266)-get_total()) = %d\n", b - count);
        a     = GameFlag_GetNibble(0x167);
        b     = GameFlag_GetNibble(0x10A);
        count = 0;
        for (k = 0; k < 5; k++) {
            if (((s16*)D_neo_ark_woodland_path_80184A60)[k] > 0) {
                count++;
            }
        }
        GameFlag_SetNibble(0x167, a + (b - count));
        count = 0;
        for (k = 0; k < 5; k++) {
            if (((s16*)D_neo_ark_woodland_path_80184A60)[k] > 0) {
                count++;
            }
        }
        GameFlag_SetNibble(0x10A, count);
        areaSyncLocationVariant(&gGameSession->location.loc);
        D_neo_ark_woodland_path_8018498E = 0x96;
    }
    D_neo_ark_woodland_path_801849F0 = Gp_StateF0.field_6;
    if (gGameSession->battleResetPending == 1 && D_neo_ark_woodland_path_8018498E == 0) {
        Gp_StateF0.prefix.bytes.field_0  = 0;
        Gp_StateF0.field_5               = 0;
        Gp_StateF0.field_6               = 0;
        Gp_StateF0.field_8               = 0;
        Gp_StateF0.field_C               = 0;
        Gp_StateF0.field_10              = 0;
        gGameSession->battleResetPending = 0;
    }
    if (Gp_StateF0.prefix.bytes.field_0 != 2 && D_neo_ark_woodland_path_80184992 != 0) {
        D_neo_ark_woodland_path_80184A5C.context.loc.stage = 5;
        D_neo_ark_woodland_path_80184A5C.context.loc.area  = 0xB;
        D_neo_ark_woodland_path_80184A5C.command           = 0xB;
        for (i = 0; i < 2; i++) {
            if (Gp_LookupSlot4(i) == 0) {
                break;
            }
            obj = Gp_LookupSlot4(i)->spawnArg2.pointer;
            if (obj == NULL) {
                break;
            }
            if (obj->hp == -999) {
                for (j = 0; j < D_neo_ark_woodland_path_80184990; j++) {
                    if (((s16*)D_neo_ark_woodland_path_80184A60)[j] > 0) {
                        obj->hp                             = D_neo_ark_woodland_path_80184A60[j];
                        obj->reactionFlags                  = 0;
                        D_neo_ark_woodland_path_80184A60[j] = 0;
                        break;
                    }
                }
                if (obj->hp > 0) {
                    Gp_IncStateF0Ref(0);
                    D_neo_ark_woodland_path_8018498E += 0x5A;
                    Gp_DispatchMsgPtr(Gp_LookupSlot4(i), ACTOR_COMMAND_MESSAGE_APPLY, &D_neo_ark_woodland_path_80184A5C, 0);
                    switch ((s16)(D_neo_ark_woodland_path_80184992 - 1)) {
                        case 0:
                            Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[0]   = D_neo_ark_woodland_path_80184A14[0].x;
                            Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[1]   = 0;
                            Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[2]   = D_neo_ark_woodland_path_80184A14[0].z;
                            Gp_LookupSlot4(i)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                            Gfx_RotMatrixY(&Gp_LookupSlot4(i)->extra.tmd->coords->coord,
                                           D_neo_ark_woodland_path_80184A14[0].rotY, 1);
                            break;
                        case 1:
                            Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[0]   = D_neo_ark_woodland_path_80184A14[1].x;
                            Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[1]   = 0;
                            Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[2]   = D_neo_ark_woodland_path_80184A14[1].z;
                            Gp_LookupSlot4(i)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                            Gfx_RotMatrixY(&Gp_LookupSlot4(i)->extra.tmd->coords->coord,
                                           D_neo_ark_woodland_path_80184A14[1].rotY, 1);
                            break;
                        case 2:
                            Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[0] = D_neo_ark_woodland_path_80184A14[2].x;
                            Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[1] = 0;
                            Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[2] = D_neo_ark_woodland_path_80184A14[2].z;
                            Gfx_RotMatrixY(&Gp_LookupSlot4(i)->extra.tmd->coords->coord,
                                           D_neo_ark_woodland_path_80184A14[2].rotY, 1);
                            Gp_LookupSlot4(i)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                            break;
                        case 3:
                            Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[0] = D_neo_ark_woodland_path_80184A14[3].x;
                            Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[1] = 0;
                            Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[2] = D_neo_ark_woodland_path_80184A14[3].z;
                            Gfx_RotMatrixY(&Gp_LookupSlot4(i)->extra.tmd->coords->coord,
                                           D_neo_ark_woodland_path_80184A14[3].rotY, 1);
                            Gp_LookupSlot4(i)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                            break;
                        case 4:
                        default:
                            Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[0] = D_neo_ark_woodland_path_80184A14[4].x;
                            Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[1] = 0;
                            Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[2] = D_neo_ark_woodland_path_80184A14[4].z;
                            Gfx_RotMatrixY(&Gp_LookupSlot4(i)->extra.tmd->coords->coord,
                                           D_neo_ark_woodland_path_80184A14[4].rotY, 1);
                            Gp_LookupSlot4(i)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                            break;
                    }
                }
                break;
            }
        }
    }
    D_neo_ark_woodland_path_80184992 = 0;
}

/// State handlers of the second arming sequence's entry task
/// `func_neo_ark_woodland_path_801815D4`: arm, run, advance, then kill.
static const TaskFuncTable4 D_neo_ark_woodland_path_8017D684 = {
    { func_neo_ark_woodland_path_80180C6C, func_neo_ark_woodland_path_80180DDC,
      func_neo_ark_woodland_path_801815C0, taskKill }
};

s32 func_neo_ark_woodland_path_80181474(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// 0x13EF handler of the first sequence: records the message's third byte as the
/// requested spawn point, unless it repeats the previous request or the
/// room's countdown `D_neo_ark_woodland_path_8018498E` is still running, in
/// which case any pending request is cleared. Always answers 1.
s32 func_neo_ark_woodland_path_8018147C(Task* task, s32 msgId, u8* msg, TaskMessageArg arg3)
{
    s16 counter;

    if (msg[2] != D_neo_ark_woodland_path_80184994) {
        counter = D_neo_ark_woodland_path_8018498E;
        if (counter == 0) {
            D_neo_ark_woodland_path_80184992 = msg[2];
        } else {
            goto L_clear;
        }
    } else {
    L_clear:
        D_neo_ark_woodland_path_80184992 = 0;
    }
    D_neo_ark_woodland_path_80184994 = msg[2];
    return 1;
}

static void func_neo_ark_woodland_path_801814D4(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

/// Entry task of the first arming sequence: runs the state handler
/// `D_neo_ark_woodland_path_8017D638` holds for the task's state, copying the table
/// to the stack first.
void func_neo_ark_woodland_path_801814E8(Task* task)
{
    TaskFuncTable4 sp;

    sp = D_neo_ark_woodland_path_8017D638;
    sp.funcs[task->state](task);
}

s32 func_neo_ark_woodland_path_8018154C(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    D_neo_ark_woodland_path_8018498E += 0x5A;
    return 1;
}

/// 0x13EF handler of the second sequence: records the message's third byte as the
/// requested spawn point, unless it repeats the previous request or the
/// room's countdown `D_neo_ark_woodland_path_8018498E` is still running, in
/// which case any pending request is cleared. Always answers 1.
s32 func_neo_ark_woodland_path_80181568(Task* task, s32 msgId, u8* msg, TaskMessageArg arg3)
{
    s16 counter;

    if (msg[2] != D_neo_ark_woodland_path_80184994) {
        counter = D_neo_ark_woodland_path_8018498E;
        if (counter == 0) {
            D_neo_ark_woodland_path_80184992 = msg[2];
        } else {
            goto L_clear;
        }
    } else {
    L_clear:
        D_neo_ark_woodland_path_80184992 = 0;
    }
    D_neo_ark_woodland_path_80184994 = msg[2];
    return 1;
}

static void func_neo_ark_woodland_path_801815C0(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

/// Entry task of the second arming sequence: runs the state handler
/// `D_neo_ark_woodland_path_8017D684` holds for the task's state, copying the table
/// to the stack first.
void func_neo_ark_woodland_path_801815D4(Task* task)
{
    TaskFuncTable4 sp;

    sp = D_neo_ark_woodland_path_8017D684;
    sp.funcs[task->state](task);
}
