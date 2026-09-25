#include "common.h"

#include <psyq/abs.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

/// Work block allocated by `func_actor_323300_80161E78` (`memCalloc(0x504)`)
/// and parked in that task's `Task::work` slot -- that slot is not a
/// `TaskIdMap` here. `func_actor_323300_801626D0` republishes the two matrices
/// onto `TmdObject::lightMtx` / `field_20`, the light/colour pair
/// `Gp_BindDefaultMtx` otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`.
///
/// The block opens with animation head `anim`: `func_actor_323300_801628B8`
/// hands the block itself to `func_800B3F84` as its `GpAnimCtx`, the slot
/// array inline at 0x14 as the `GpAnimSlot*`, and 0x30C as the pose buffer.
/// The 19 0x28-byte slots run exactly up to that buffer, and every tick loop
/// walks indices 1..0x13, leaving slot 0 alone.
///
/// The display node at +0x480 is the one the exit callback
/// `func_actor_323300_8016269C` hands back to `Gp_UnlinkObj` before tearing
/// the enemy task down. The size is the allocation, and the fields below are
/// the ones the init seeds: the two `sb` bytes at 0x43D/0x43E and the `sh` at
/// 0x502 are set to -1, and 0x500 is set to 1. `rec` is the one-entry `GpRec18`
/// collision table `Gp_InitRec18Table` seeds at 0x4A0; `obj.ctx.recs` addresses
/// it and `Gp_FindNearestSlot` walks it through that pointer.
typedef struct Actor323300Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[19];
    /* 0x30C */ byte       pad_30C[0x130];
    /* 0x43C */ s8         field_43C; // set once the slots have been started
    /* 0x43D */ s8         field_43D; // animation id the slots were seeded with
    /* 0x43E */ s8         field_43E; // animation bank index
    /* 0x43F */ s8         field_43F; // latched to 2 by the 0x7DB placement case
    /* 0x440 */ MATRIX     light;
    /* 0x460 */ MATRIX     color;
    /* 0x480 */ GpObj      obj;
    /* 0x4A0 */ GpRec18    rec;       // seed table `obj.ctx.recs` points at
    /* 0x4B8 */ Task*      field_4B8; // child spawned from `D_actor_323300_8017255C` index 1
    /* 0x4BC */ s32        field_4BC; // placement position, copied verbatim
    /* 0x4C0 */ s32        field_4C0;
    /* 0x4C4 */ s32        field_4C4;
    /* 0x4C8 */ byte       pad_4C8[0x4];
    /* 0x4CC */ s32        field_4CC;
    /* 0x4D0 */ s32        field_4D0;
    /* 0x4D4 */ s32        field_4D4;
    /* 0x4D8 */ byte       pad_4D8[0x1C];
    /* 0x4F4 */ u16        field_4F4; // placement rotation, copied verbatim
    /* 0x4F6 */ u16        field_4F6; // target yaw the turn-to-face body steers toward
    /* 0x4F8 */ u16        field_4F8;
    /* 0x4FA */ byte       pad_4FA[0x2];
    /* 0x4FC */ s16        field_4FC;
    /* 0x4FE */ s16        field_4FE;
    /* 0x500 */ s16        field_500;
    /* 0x502 */ s16        field_502;
} Actor323300Work;
STATIC_ASSERT_SIZEOF(Actor323300Work, 0x504);

/// 0x14-byte animation preset the spawn handlers and the state functions hand
/// `func_actor_323300_801628B8` / `func_actor_323300_80163718`: `field_0` is
/// the animation bank index the helper compares against the block's current
/// bank, `field_4` the animation id it compares against the current id, and
/// `field_8` selects between `func_800B4114` -- which also takes `field_C` --
/// and `Gp_AnimResetSlot`. `field_10` is unread.
typedef struct {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
} Actor323300AnimPreset;
STATIC_ASSERT_SIZEOF(Actor323300AnimPreset, 0x14);

/// Vertex-morph source `func_actor_323300_80162A6C` blends the model with.
/// `field_8` and `field_C` are the key vertex and key normal arrays -- 8-byte
/// `SVECTOR`s, the stride `gteMIMefunc` itself takes -- which that function
/// copies into the model's own vertex and normal arrays starting at part index
/// `field_14`, `field_16` and `field_12` entries apiece; `field_0` is the second
/// key-vertex array it interpolates the model's vertices against, and `field_4`
/// gates the normal pass the way `field_8` gates the vertex one.
typedef struct {
    /* 0x00 */ SVECTOR* field_0;
    /* 0x04 */ s32      field_4;
    /* 0x08 */ SVECTOR* field_8;
    /* 0x0C */ SVECTOR* field_C;
    /* 0x10 */ s16      field_10;
    /* 0x12 */ s16      field_12;
    /* 0x14 */ s16      field_14;
    /* 0x16 */ s16      field_16;
} GpMimeSrc;

/// The larger of the two work blocks this overlay parks in `Task::work`: the
/// `memCalloc(0x6B0)` that `func_actor_323300_80162BE4` allocates, as opposed
/// to the 0x504 `Actor323300Work` `func_actor_323300_80161E78` allocates. The
/// two are different allocations of different sizes, but both carry a
/// light/colour `MATRIX` pair republished onto `TmdObject::lightMtx` /
/// `field_20` by the display path -- here at 0x670/0x690, so the trailing
/// `color` ends flush with the allocation.
///
/// The prefix is the same animation shape the 0x504 block opens with: the
/// `GpAnimCtx` at 0, the `GpAnimSlot` array inline at 0x14 and the
/// pose buffer at 0x30C -- the three addresses
/// `func_actor_323300_80163718` hands `func_800B3F84`. Its animation state
/// sits in the four `s32` words past that buffer rather than in the byte fields
/// `Actor323300Work` uses: `field_440` is the preset bank index, `field_444`
/// the preset animation id (`func_actor_323300_80162BE4` seeds both to -1) and
/// `field_43C` the once-only flag its tick path sets. `field_44C` is the 0x3000
/// that same initialiser stores.
typedef struct Actor323300MtxWork {
    /* 0x000 */ GpAnimCtx     anim;
    /* 0x014 */ GpAnimSlot    slots[19];
    /* 0x30C */ byte          pad_30C[0x130];
    /* 0x43C */ s32           field_43C; // set once the slots have been started
    /* 0x440 */ s32           field_440; // animation bank index the slots were seeded with
    /* 0x444 */ s32           field_444; // animation id the slots were seeded with
    /* 0x448 */ byte          pad_448[0x4];
    /* 0x44C */ s32           field_44C;
    /* 0x450 */ GsCOORDINATE2 shadow[3];   // unsquashed copies of parts 3..5, re-parented onto 4..6
    /* 0x540 */ VECTOR        partPos[19]; // original part translations, before the squash
    /* 0x670 */ ActorMat      light;
    /* 0x690 */ ActorMat      color;
} Actor323300MtxWork;
STATIC_ASSERT_SIZEOF(Actor323300MtxWork, 0x6B0);

/// Payload the sender of message 0x7DB passes as `Gp_DispatchMsg`'s `arg2`;
/// the halfword at 0x2 is the only part `func_actor_323300_80162360` reads.
typedef struct Actor323300Msg7DB {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor323300Msg7DB;
STATIC_ASSERT_SIZEOF(Actor323300Msg7DB, 0x4);

/// Message table `func_actor_323300_80161E78` parks in `Task::msgTable`:
/// `Gp_DispatchMsg` matches an incoming id against these and calls the handler.
/// Ids 0x7D3/0x7D4/0x7D5/0x7DB reach `func_actor_323300_801628B8`,
/// `func_actor_323300_801629F0`, `func_actor_323300_80162208` and
/// `func_actor_323300_80162360`; the 0x7FFFFFFF terminator ends the walk.
extern GpMsgEntry D_actor_323300_80172574[];

/// Animation source table `func_actor_323300_80162360` and
/// `func_actor_323300_801628B8` index by the 0x504 block's bank byte.
extern void* D_actor_323300_80172558[];

/// Descriptor table the 0x7DB handler spawns its child from; index 1 is the
/// task parked in `Actor323300Work::field_4B8`.
extern TaskDesc D_actor_323300_8017255C[];

/// Placement `func_actor_323300_80161E78` hands `func_actor_323300_801629F0`.
extern GpPlaceArg D_actor_323300_8017259C;

/// Animation presets the spawn handler, the 0x7DB handler and the two states
/// hand `func_actor_323300_801628B8`.
extern Actor323300AnimPreset D_actor_323300_801725B4;
extern Actor323300AnimPreset D_actor_323300_801725C8;
extern Actor323300AnimPreset D_actor_323300_801725DC;

/// Animation source table `func_actor_323300_80163718` indexes by the 0x6B0
/// block's bank index, one `void*` per bank. `func_actor_323300_80162BE4`
/// applies the preset `D_actor_323300_80174A74` through it and places the
/// actor at `D_actor_323300_80174AB0`.
extern void*                 D_actor_323300_80174A70[];
extern Actor323300AnimPreset D_actor_323300_80174A74;
extern GpPlaceArg            D_actor_323300_80174AB0;

/// Vertex-morph source `func_actor_323300_80162DF0` re-blends every frame off
/// the 0x6B0 block's squash ramp. Absolute, so it lives outside the overlay.
extern GpMimeSrc D_801865D0;

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// `gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).
void func_8004BFF8(s16 angle, MATRIX* matrix);

void func_actor_323300_80161E78(Task* arg0);
void func_actor_323300_80161FE8(Task* arg0);
s32  func_actor_323300_80162208(Task* arg0, s32 arg1, s32 mode, s32 arg3);
void func_actor_323300_8016269C(Task* arg0);
void func_actor_323300_801626D0(Task* arg0);
void func_actor_323300_801626EC(Task* arg0);
void func_actor_323300_801626F4(Task* arg0);
void func_actor_323300_80162748(Task* arg0);
void func_actor_323300_801627B4(Task* arg0);
s32  func_actor_323300_801628B8(Task* arg0, s32 arg1, Actor323300AnimPreset* arg2, s32 arg3);
s32  func_actor_323300_801629F0(Task* arg0, s32 arg1, GpPlaceArg* arg2, s32 arg3);
void func_actor_323300_801634B0(Task* arg0);
void func_actor_323300_80163510(Task* arg0);
void func_actor_323300_8016359C(Task* arg0, s16 arg1);
s32  func_actor_323300_8016369C(Task* arg0, s32 arg1, GpPlaceArg* arg2, s32 arg3);
s32  func_actor_323300_80163718(Task* arg0, s32 arg1, Actor323300AnimPreset* arg2, s32 arg3);

/// State table `func_actor_323300_80162630` copies onto the stack and indexes
/// by `Task::state`: spawn, per-frame runner and exit of the 0x504 block.
const TaskFuncTable3 D_actor_323300_80161E24 = { {
    func_actor_323300_80161E78,
    func_actor_323300_80161FE8,
    func_actor_323300_8016269C,
} };

/// Allocates the 0x504 `Actor323300Work` this actor's whole lifetime runs on,
/// seeds the `GpRec18` collision table and the display node at +0x480, then
/// binds the three message handlers and the animation presets the state
/// functions drive. Bails out through `Gp_EnemyTaskExit` when the room flag
/// 0x60 is already set (the actor already spawned) or the allocation fails.
void func_actor_323300_80161E78(Task* arg0)
{
    Actor323300Work* work;
    TmdObject*       extra;
    GpObj*           obj;

    if (GameFlag_GetNibble(0x60) != 0 || (work = memCalloc(0x504, 0)) == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }
    arg0->work      = (TaskIdMap*)work;
    work->field_43D = -1;
    work->field_43E = -1;
    work->field_500 = 1;
    work->field_502 = -1;
    func_actor_323300_801626D0(arg0);
    extra         = arg0->extra;
    obj           = &work->obj;
    obj->coord    = extra->coords + 1;
    obj->ctx.recs = &work->rec;
    obj->key      = 0x30000;
    obj->pos.vx   = 0;
    obj->pos.vy   = 0;
    obj->pos.vz   = 0;
    obj->radius   = 0x100;
    obj->flags    = 1;
    Gp_LinkObj(2, obj);
    obj->flags |= 0x8000;
    Gp_InitRec18Table(obj->ctx.recs, 1, 0);
    arg0->msgTable = &D_actor_323300_80172574;
    func_actor_323300_80162208(arg0, 0x7D5, 0, 0);
    func_actor_323300_801629F0(arg0, 0x7D3, &D_actor_323300_8017259C, 0);
    func_actor_323300_801628B8(arg0, 0x7D3, &D_actor_323300_801725B4, 0);
    SndEvt_EnqueueType6(0x52100006, 0, 0x28);
    arg0->exitCallback = func_actor_323300_8016269C;
    arg0->state       += 1;
}

/// Per-frame runner for the `Actor323300Work` block: dispatches on
/// `field_4FC` through the two-entry handler table it builds on the stack,
/// walks the 18 animation slots and, while `field_500` is set, posts one of the
/// two sound cues -- the pan/depth pair the session's `at4.loc.view` picks between
/// is built twice so the two calls cross-jump into a shared `jal`. Then, unless
/// `TmdObject::flags` says the model is hidden, draws the ground shadow under
/// coordinate 1, refreshes that coordinate's matrix and colour, and ticks the
/// `field_502` countdown that frees the model's buffers when it reaches zero.
void func_actor_323300_80161FE8(Task* arg0)
{
    TmdObject*       extra               = (TmdObject*)arg0->extra;
    Actor323300Work* work                = (Actor323300Work*)arg0->work;
    void             (*states[2])(Task*) = {
        func_actor_323300_801626EC,
        func_actor_323300_801626F4,
    };
    VECTOR vec;
    s32    i;

    states[(s16)work->field_4FC](arg0);
    if (work->field_43C != 0) {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
        }
        if (work->field_500 != 0) {
            if (work->slots[1].flags & 2) {
                if (gGameSession->at4.loc.view == 2) {
                    SndEvt_EnqueueType6(0x52100006, 0, 0x28);
                } else {
                    SndEvt_EnqueueType6(0x52100006, 0, 0);
                }
            } else if (gGameSession->viewReady != 0) {
                if (gGameSession->at4.loc.view == 2) {
                    SndEvt_EnqueueTypeA(0x52100006, 0, 0x28);
                } else {
                    SndEvt_EnqueueTypeA(0x52100006, 0, 0);
                }
            }
        }
    }
    if (!(extra->flags & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)arg0->extra)->coords[1].workm.t, (VECTOR3*)&vec) != 0) {
            Gp_DrawEffGroundQuad((VECTOR3*)&vec, 0x200, Gp_State1C->groundShade);
        }
        Gp_ClearRec18Occupied(&work->rec);
        ((TmdObject*)arg0->extra)->coords[1].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[1]);
        func_800D7A9C(extra, (VECTOR*)((TmdObject*)arg0->extra)->coords[1].workm.t, 0, 3);
    }
    if (work->field_502 >= 0) {
        if (work->field_502 == 0) {
            Tmd_FreeBuffers(extra);
        }
        work->field_502--;
    }
}

/// Message-0x7D5 handler: the four-way visibility/mode switch on the message's
/// mode word, plus the display-node toggles the 0x504 work block's own `GpObj`
/// needs.
///
/// Mode 0 hides the model -- `TmdObject::flags` bit 0x80, the bit
/// `func_actor_323300_80161FE8` tests before drawing the ground shadow -- and
/// clears bit 4 so the buffers get reallocated; 1 shows it, puts the node back
/// in the pair walk and allocates the aux buffers; 2 hides it and arms the
/// `field_502` countdown that same per-frame runner frees the buffers with; 3
/// shows it while keeping them. Modes 2 and 3 set bit 4, so the update path
/// skips the realloc for the two frames the countdown runs. Anything else
/// returns 1 and leaves the object alone; the handled modes return 0.
///
/// The node's `GpObj::flags` halfword is the induction variable, strided by one
/// `GpObj` per step: the block owns a single node, so the walk covers one
/// element, but retail keeps the array shape. Bit 0x8000 is the one
/// `Gp_RunPairHandler` tests before pairing the node up, so this switch is what
/// takes the node in and out of the pair walk.
s32 func_actor_323300_80162208(Task* arg0, s32 arg1, s32 mode, s32 arg3)
{
    Actor323300Work* work;
    TmdObject*       extra;
    u16*             flags;
    s32              i;
    s32              ret;

    extra = arg0->extra;
    work  = (Actor323300Work*)arg0->work;
    ret   = 0;

    switch (mode) {
        case 0:
            extra->flags |= 0x80;
            flags         = &work->obj.flags;
            for (i = 0; i < 1; i++) {
                flags[i * (sizeof(GpObj) / sizeof(*flags))] &= 0x7FFF;
            }
            extra->flags &= ~4;
            break;
        case 1:
            extra->flags &= ~0x80;
            flags         = &work->obj.flags;
            for (i = 0; i < 1; i++) {
                flags[i * (sizeof(GpObj) / sizeof(*flags))] |= 0x8000;
            }
            Tmd_AllocBuffers(extra);
            extra->flags &= ~4;
            break;
        case 2:
            extra->flags |= 0x80;
            flags         = &work->obj.flags;
            for (i = 0; i < 1; i++) {
                flags[i * (sizeof(GpObj) / sizeof(*flags))] &= 0x7FFF;
            }
            work->field_502 = 2;
            extra->flags   |= 4;
            break;
        case 3:
            extra->flags &= ~0x80;
            flags         = &work->obj.flags;
            for (i = 0; i < 1; i++) {
                flags[i * (sizeof(GpObj) / sizeof(*flags))] |= 0x8000;
            }
            extra->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }

    return ret;
}

/// Message 0x7DB handler, listed in `D_actor_323300_80172574` after the 0x7D3 /
/// 0x7D4 / 0x7D5 ones. The payload halfword selects one of five actions: 0
/// shows the model through the 0x7D5 visibility switch, 10/11 spawn and kill
/// the child at `field_4B8`, 12 latches a placement and starts preset
/// `D_actor_323300_801725C8` (inlining the 0x7D3 preset body of
/// `func_actor_323300_801628B8`), 13 posts effect 0x600A2 on part 6.
s32 func_actor_323300_80162360(Task* arg0, s32 arg1, Actor323300Msg7DB* msg, GpPlaceArg* place)
{
    Actor323300Work*       w;
    Actor323300Work*       work;
    Actor323300AnimPreset* preset;
    TmdObject*             extra;
    Task*                  spawned;
    GsCOORDINATE2*         src;
    GsCOORDINATE2*         dst;
    SVECTOR                vec;
    s32                    i;

    w = (Actor323300Work*)arg0->work;
    switch (msg->field_2) {
        case 0:
            func_actor_323300_80162208(arg0, 0x7D5, 1, 0);
            break;
        case 10:
            spawned      = Task_SpawnFromTable(D_actor_323300_8017255C, 1, 0, 0);
            w->field_4B8 = spawned;
            if (spawned != NULL) {
                src        = ((TmdObject*)arg0->extra)->coords;
                dst        = ((TmdObject*)spawned->extra)->coords;
                dst->coord = src->coord;
            }
            w->field_500 = 0;
            break;
        case 11:
            if (w->field_4B8 != NULL) {
                taskKill(w->field_4B8);
            }
            w->field_500 = 0;
            SndEvt_EnqueueType7(0x52100006, 1);
            break;
        case 12:
            w->field_4FC = 1;
            w->field_4FE = 0;
            w->field_4BC = place->pos.vx;
            w->field_4C0 = place->pos.vy;
            w->field_4C4 = place->pos.vz;
            w->field_4F4 = place->rot.vx;
            w->field_4F6 = place->rot.vy;
            w->field_4F8 = place->rot.vz;
            w->field_43F = 2;

            preset = &D_actor_323300_801725C8;
            work   = (Actor323300Work*)arg0->work;
            extra  = arg0->extra;
            if (preset->field_0 != work->field_43E) {
                work->field_43E = preset->field_0;
                work->field_43D = -1;
                func_800B3F84(&work->anim, D_actor_323300_80172558[work->field_43E], extra,
                              work->pad_30C, work->slots);
            }
            if (preset->field_4 != work->field_43D) {
                work->field_43D = preset->field_4;
                if (preset->field_8 != 0 && work->field_43C != 0) {
                    for (i = 1; i < 0x13; i++) {
                        func_800B4114(&work->anim, i, work->field_43D, 0, preset->field_C);
                    }
                } else {
                    for (i = 1; i < 0x13; i++) {
                        Gp_AnimResetSlot(&work->anim, i, work->field_43D);
                    }
                }
                for (i = 1; i < 0x13; i++) {
                    Gp_AnimTickIndex(&work->anim, i);
                }
                work->field_43C = 1;
            }
            break;
        case 13:
            vec.vy = 0x3C;
            vec.vx = 0;
            vec.vz = 0xC8;
            Gp_SpawnEff(0x600A2, &((TmdObject*)arg0->extra)->coords[6], 0xA, &vec);
            break;
    }
    return 0;
}

/// Per-frame dispatcher of the 0x504-block actor: runs its spawn, tick or exit
/// state from `D_actor_323300_80161E24` by `Task::state`, skipping the frame
/// while the global freeze byte is set.
void func_actor_323300_80162630(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_323300_80161E24;
    if (Gp_StateF0.field_4 == 0) {
        sp.funcs[task->state](task);
    }
}

void func_actor_323300_8016269C(Task* arg0)
{
    Gp_UnlinkObj(&((Actor323300Work*)arg0->work)->obj);
    Gp_EnemyTaskExit(arg0);
}

void func_actor_323300_801626D0(Task* arg0)
{
    TmdObject*       ext;
    Actor323300Work* work;

    ext           = arg0->extra;
    work          = (Actor323300Work*)arg0->work;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}

/// Index 0 of the two-entry table `func_actor_323300_80161FE8` builds on its
/// stack: the empty "hold" state.
void func_actor_323300_801626EC(Task* arg0)
{
}

/// Index 1 of that table: re-dispatches on `field_4FE` through a second
/// two-entry table, the preset start and the turn-to-face step.
void func_actor_323300_801626F4(Task* arg0)
{
    Actor323300Work* work                = (Actor323300Work*)arg0->work;
    void             (*states[2])(Task*) = {
        func_actor_323300_80162748,
        func_actor_323300_801627B4,
    };

    states[(s16)work->field_4FE](arg0);
}

void func_actor_323300_80162748(Task* arg0)
{
    Actor323300Work* work;
    s32              i;

    work = (Actor323300Work*)arg0->work;
    func_actor_323300_801628B8(arg0, 0x7D3, &D_actor_323300_801725C8, 0);
    for (i = 1; i < 0x13; i++) {
        work->slots[i].rate = 8;
    }
    work->field_4CC = 0;
    work->field_4D0 = 0;
    work->field_4D4 = 0;
    work->field_4FE++;
}

/// State handler at index 1 of the two-entry table `func_actor_323300_801626F4`
/// dispatches, the turn-to-face body. Euler-extracts the root coordinate into `vec`
/// and, while the yaw gap to the target `work->field_4F6` stays under 0x41,
/// snaps `vec.vy` to that target, plays anim 0x7D3 through
/// `func_actor_323300_801628B8` and parks all 18 animation slots at 0x16 --
/// `field_4FC` and `field_4FE` go back to zero, so the handler re-runs. A wider
/// gap steps `vec.vy` toward the target by 0x40 instead. Either way the root
/// coordinate is rebuilt as the identity matrix rotated by `vec`, with `flg`
/// cleared so the next `Gp_UpdateCoord` recomputes it.
void func_actor_323300_801627B4(Task* arg0)
{
    Actor323300Work* work;
    ActorMat*        words;
    GsCOORDINATE2*   coord;
    SVECTOR          vec;
    s16              diff;
    s32              vy;
    s32              i;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor323300Work*)arg0->work;

    Gp_ExtractEuler(&vec, &coord->coord);
    diff = (u16)work->field_4F6 - (u16)vec.vy;
    if (ABS(diff) >= 0x41) {
        vy = vec.vy;
        if (diff < 0) {
            vec.vy = vy - 0x40;
        } else {
            vec.vy = vy + 0x40;
        }
    } else {
        vec.vy = work->field_4F6;
        func_actor_323300_801628B8(arg0, 0x7D3, &D_actor_323300_801725DC, 0);
        for (i = 1; i < 0x13; i++) {
            work->slots[i].rate = 0x16;
        }
        work->field_4FC = 0;
        work->field_4FE = 0;
    }

    words                = (ActorMat*)&coord->coord;
    words->ident.m00_m01 = 0x1000;
    words->ident.m02_m10 = 0;
    words->ident.m11_m12 = 0x1000;
    words->ident.m20_m21 = 0;
    words->ident.m22     = 0x1000;
    RotMatrix(&vec, &coord->coord);
    coord->flg = 0;
}

/// Message-0x7D3 handler, also called directly by the spawn handler and the
/// two states of `func_actor_323300_801626F4` with a preset of their own. A
/// changed bank index re-seeds the whole animation slot array through
/// `func_800B3F84` from `D_actor_323300_80172558` and forgets the current
/// animation id. A changed animation id is then stored and installed on every
/// slot 1..0x12 - through `func_800B4114` when the preset's `field_8` is set
/// and the slots have already been started, through `Gp_AnimResetSlot`
/// otherwise - after which every slot is ticked once and `field_43C` latches.
/// An unchanged id skips all of that. Returns 0.
s32 func_actor_323300_801628B8(Task* task, s32 arg1, Actor323300AnimPreset* msg, s32 arg3)
{
    Actor323300Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor323300Work*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_43E) {
        work->field_43E = msg->field_0;
        work->field_43D = -1;
        func_800B3F84(&work->anim, D_actor_323300_80172558[work->field_43E], ext, work->pad_30C, work->slots);
    }
    if (msg->field_4 != work->field_43D) {
        work->field_43D = msg->field_4;
        if (msg->field_8 != 0 && work->field_43C != 0) {
            for (i = 1; i < 0x13; i++) {
                func_800B4114(&work->anim, i, work->field_43D, 0, msg->field_C);
            }
        } else {
            for (i = 1; i < 0x13; i++) {
                Gp_AnimResetSlot(&work->anim, i, work->field_43D);
            }
        }
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
        work->field_43C = 1;
    }
    return 0;
}

/// Message-0x7D4 handler: places the actor at `args`. The translation goes
/// straight into the root part's local matrix, the Euler angles into the
/// coordinate's `rot` slot, from which `RotMatrix` rebuilds the rotation;
/// clearing `flg` makes the world matrix be recomputed.
s32 func_actor_323300_801629F0(Task* task, s32 msgId, GpPlaceArg* args, s32 arg3)
{
    GpCoordExt* coord;

    coord               = (GpCoordExt*)((TmdObject*)task->extra)->coords;
    coord->coord.t[0]   = args->pos.vx;
    coord->coord.t[1]   = args->pos.vy;
    coord->coord.t[2]   = args->pos.vz;
    coord->param.rot.vx = args->rot.vx;
    coord->param.rot.vy = args->rot.vy;
    coord->param.rot.vz = args->rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->flg = 0;
    return 0;
}

/// Blends `arg1`'s key vertex and normal arrays into the model parts starting at
/// `arg1->field_14`, `arg2` being the 0..0x1000 ramp: the vertex pass runs
/// through `gteMIMefunc` against `arg1->field_0`, the normal pass only while
/// `arg1->field_4` is set.
void func_actor_323300_80162A6C(Task* arg0, GpMimeSrc* arg1, s32 arg2)
{
    s32        i;
    s32        count;
    s32        off;
    TmdSource* src;
    u16*       dst;
    u16*       from;
    u16*       dstMid;
    u16*       fromMid;
    SVECTOR*   nrm;
    SVECTOR*   nrmA;
    SVECTOR*   nrmB;
    SVECTOR*   nrmDst;
    s32        blend;
    s32        inv;
    u16        vx;
    u16        vz;

    i     = 0;
    count = arg1->field_16;
    src   = ((TmdObject*)arg0->extra)->source;
    off   = arg1->field_14 * 8;
    from  = (u16*)((u8*)arg1->field_8 + off);
    nrm   = src->normals;
    dst   = (u16*)((u8*)src->verts + off);
    if (count > 0) {
        fromMid = from + 2;
        dstMid  = dst + 2;
        do {
            vx         = *from;
            from      += 4;
            i         += 1;
            *dst       = vx;
            dst       += 4;
            dstMid[-1] = fromMid[-1];
            vz         = fromMid[0];
            fromMid   += 4;
            dstMid[0]  = vz;
            dstMid    += 4;
        } while (i < count);
    }
    blend = arg2;
    inv   = 0x1000 - blend;
    gteMIMefunc(src->verts + arg1->field_14, arg1->field_0, arg1->field_16, blend);
    nrmA = (SVECTOR*)arg1->field_4;
    if (nrmA != NULL) {
        count = arg1->field_12;
        nrmB  = arg1->field_C;
        i     = 0;
        if (count > 0) {
            do {
                gte_lddp(blend);
                gte_ldsv(nrmA);
                gte_gpf12();
                nrmDst = nrm + i;
                gte_lddp(inv);
                gte_ldsv(nrmB);
                gte_gpl12();
                nrmB++;
                i++;
                nrmA++;
                gte_stsv(nrmDst);
            } while (i < count);
        }
    }
}

void func_actor_323300_80162BE4(Task* arg0)
{
    Actor323300MtxWork* work;
    TmdObject*          extra;
    TmdObject*          model;
    TmdSource*          src;
    GsCOORDINATE2*      coords;
    SVECTOR*            dst;
    SVECTOR*            from;
    GpMimeSrc*          ctl;
    SVECTOR*            nrm;
    long*               translation;
    s32                 i;
    s32                 part;

    extra              = arg0->extra;
    arg0->exitCallback = func_actor_323300_801634B0;
    work               = (Actor323300MtxWork*)memCalloc(0x6B0, 0);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    arg0->work         = work;
    work->field_444    = -1;
    work->field_440    = -1;
    work->field_44C    = 0x3000;
    extra->clut        = 2;
    extra->tpageOffset = 2;
    extra->clutOffset  = 4;
    extra->tpage       = 0;
    extra->lightLevel  = 0xFFF;
    extra->flags      &= 0xFF7F;
    tmdProcessStream(extra);
    tmdProcessStream(extra);
    func_actor_323300_80163718(arg0, 0x7D3, &D_actor_323300_80174A74, 0);
    func_actor_323300_8016369C(arg0, 0x7D3, &D_actor_323300_80174AB0, 0);
    model = arg0->extra;
    ctl   = &D_801865D0;
    SCHED_BARRIER();
    dst = ctl->field_8;
    nrm = ctl->field_C;
    SCHED_BARRIER();
    src  = model->source;
    from = (SVECTOR*)src->verts;
    for (i = 0; i < ctl->field_10; i++) {
        dst[i].vx = (u16)from[i].vx;
        dst[i].vy = (u16)from[i].vy;
        dst[i].vz = (u16)from[i].vz;
    }
    if (ctl->field_4 != 0) {
        from = (SVECTOR*)src->normals;
        i    = 0;
        if (ctl->field_12 > 0) {
            dst = nrm;
            do {
                dst[i].vx = (u16)from[i].vx;
                dst[i].vy = (u16)from[i].vy;
                dst[i].vz = (u16)from[i].vz;
                i++;
            } while (i < ctl->field_12);
        }
    }
    func_actor_323300_80163510(arg0);
    for (part = 1; part < 0x13; part++) {
        coords                     = ((TmdObject*)arg0->extra)->coords;
        translation                = coords[part].coord.t;
        work->partPos[part].vx     = translation[0];
        (work->partPos + part)->vy = coords[part].coord.t[1];
        (work->partPos + part)->vz = coords[part].coord.t[2];
    }
    DEF_REG(model);
    arg0->state += 1;
}

/// Per-frame squash driver for the 0x6B0 `Actor323300MtxWork` block, and the
/// runner the model-display path calls once the block's animation has been
/// started: it ticks the 18 slots like `func_actor_323300_80163718` does, folds
/// `field_44C` -- the 0x3000 countdown `func_actor_323300_80162BE4` seeds, 0x40
/// per frame -- into the 0..0xFFF ramp `func_actor_323300_80162A6C` blends the
/// model's vertices with, and republishes that ramp onto `TmdObject::lightLevel`,
/// the intensity the shading path scales its RGB by. While the countdown is
/// still above 0x1000 the turn angle handed to `func_actor_323300_8016359C` is
/// `(0x1000 - field_44C) / 4`, i.e. the ramp read the other way round.
///
/// The three coordinate nodes at parts 3..5 are then flattened: each is copied
/// off into `shadow[0..2]` first, then squashed in place through
/// `ScaleMatrix` -- parts 3 and 4 to 0.2 on Y, part 5 to identity -- and the
/// *copies* become the parents of parts 4, 5 and 6, so the squash does not
/// compound down the part chain. The Y translation the squash removes from
/// parts 4 and 5 is folded out of their own `coord.t[1]` by the same 0.8 and
/// the same ramp. Part 6's shading is rebound to the third copy's translation
/// before the countdown drops, so the whole ramp runs out exactly when it
/// reaches zero.
void func_actor_323300_80162DF0(Task* arg0)
{
    Actor323300MtxWork* work;
    TmdObject*          extra;
    GsCOORDINATE2*      coord;
    VECTOR              vec;
    s32                 blend;
    s32                 i;

    work  = (Actor323300MtxWork*)arg0->work;
    extra = (TmdObject*)arg0->extra;

    if (work->field_43C != 0) {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
        }
    }

    blend = work->field_44C;
    if (blend >= 0x2000) {
        blend = 0xFFF;
    } else if (blend > 0x1000) {
        blend -= 0x1000;
    } else {
        blend = 0;
    }

    func_actor_323300_80162A6C(arg0, &D_801865D0, blend);
    extra->lightLevel = blend;

    if (work->field_44C < 0x1000) {
        func_actor_323300_8016359C(arg0, (s16)(((0x1000 - work->field_44C) << 14) >> 16));
    }

    coord           = &((TmdObject*)arg0->extra)->coords[3];
    work->shadow[0] = *coord;
    vec.vx          = 0x1000;
    vec.vy          = 0x333;
    vec.vz          = 0x1000;
    ScaleMatrix(&coord->coord, &vec);

    coord           = &((TmdObject*)arg0->extra)->coords[4];
    work->shadow[1] = *coord;
    coord->sub      = &work->shadow[0];
    vec.vx          = 0x1000;
    vec.vy          = 0x333;
    vec.vz          = 0x1000;
    ScaleMatrix(&coord->coord, &vec);
    coord->coord.t[1] = work->partPos[4].vy - work->partPos[4].vy * 0.8 * blend / 4096.0;

    coord           = &((TmdObject*)arg0->extra)->coords[5];
    work->shadow[2] = *coord;
    coord->sub      = &work->shadow[1];
    vec.vx          = 0x1000;
    vec.vy          = 0x1000;
    vec.vz          = 0x1000;
    ScaleMatrix(&coord->coord, &vec);
    coord->coord.t[1] = work->partPos[5].vy - work->partPos[5].vy * 0.8 * blend / 4096.0;

    coord      = &((TmdObject*)arg0->extra)->coords[6];
    coord->sub = &work->shadow[2];
    func_800D7A9C(extra, (VECTOR*)coord->workm.t, 0, 3);

    work->field_44C -= 0x40;
    if (work->field_44C < 0) {
        work->field_44C = 0;
    }
}

/// Turns joint `coord` by `angle` about the world Y axis and then by half of it
/// about X, so the joint is pitched as well as turned: builds its world
/// rotation in a matrix carved off the scratchpad head, applies both turns,
/// converts the result back into the parent's frame, writes the 3x3 into the
/// joint and refreshes it.
void func_actor_323300_80163188(GsCOORDINATE2* coord, s16 angle)
{
    MATRIX*        rotation;
    GsCOORDINATE2* out;

    *(MATRIX**)G_SCRATCH_HEAD -= 1;
    rotation                   = *(MATRIX**)G_SCRATCH_HEAD;
    actorAccumulateRotation(coord, rotation, &gGfxViewCoord);
    func_8004BFF8(angle, rotation);
    RotMatrixX(angle / 2, rotation);
    out = actorLocalizeRotation(coord, rotation);
    __builtin_memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->flg = 0;
    Gp_UpdateCoord(out);
    *(MATRIX**)G_SCRATCH_HEAD += 1;
}

void func_actor_323300_801634B0(Task* arg0)
{
    GsCOORDINATE2* base;
    GsCOORDINATE2* node;
    GsCOORDINATE2* sub;

    do {
        base      = ((TmdObject*)arg0->extra)->coords;
        sub       = base + 3;
        node      = base + 4;
        node->sub = sub;
    } while (0);
    sub                                      = ((TmdObject*)arg0->extra)->coords + 5;
    sub->sub                                 = node;
    ((TmdObject*)arg0->extra)->coords[6].sub = sub;
    taskKill(arg0);
}

/// Splats an identity light/colour pair into the `memCalloc(0x6B0)` work block
/// `func_actor_323300_80162BE4` parked in `Task::work`, republishes them onto
/// `TmdObject::lightMtx` / `field_20`, then re-derives model part 1's world
/// matrix -- clearing its dirty flag, rebuilding it from its parent and
/// rebinding the actor's shading to the part's translation.
void func_actor_323300_80163510(Task* arg0)
{
    Actor323300MtxWork* work;
    ActorMat*           light;
    ActorMat*           color;
    GsCOORDINATE2*      coords;
    TmdObject*          extra;

    extra  = arg0->extra;
    work   = (Actor323300MtxWork*)arg0->work;
    coords = extra->coords;

    work->light.ident.m00_m01 = 0x1000;
    light                     = &work->light;
    light->ident.m02_m10      = 0;
    light->ident.m11_m12      = 0x1000;
    light->ident.m20_m21      = 0;
    light->ident.m22          = 0x1000;

    work->color.ident.m00_m01 = 0x1000;
    color                     = &work->color;
    color->ident.m02_m10      = 0;
    color->ident.m11_m12      = 0x1000;
    color->ident.m20_m21      = 0;
    color->ident.m22          = 0x1000;

    extra->lightMtx = &light->mat;
    extra->colorMtx = &color->mat;

    coords[1].flg = 0;
    Gp_UpdateCoord(&coords[1]);
    func_800D7A9C(extra, (VECTOR*)coords[1].workm.t, 0, 3);
}

/// Re-aims the per-part coordinate nodes at index 5 and index 2 from one turn
/// angle: the angle is clamped to +-0x400 -- a quarter turn either way -- then
/// `func_actor_323300_80163188` rebuilds node 5 from two thirds of it and node
/// 2 from half, and nodes 5 down to 2 have their dirty flag cleared so the next
/// `Gp_UpdateCoord` re-derives them. The lower clamp tests `arg1` rather than
/// the clamped copy; that is the same test, because the upper clamp has already
/// pinned the copy to 0x400 whenever the angle was out of range upwards.
void func_actor_323300_8016359C(Task* arg0, s16 arg1)
{
    s16 var;

    var = arg1;
    if (var > 0x400) {
        var = 0x400;
    }
    if (arg1 < -0x400) {
        var = -0x400;
    }

    func_actor_323300_80163188(&((TmdObject*)arg0->extra)->coords[5], (var * 2) / 3);
    func_actor_323300_80163188(&((TmdObject*)arg0->extra)->coords[2], var / 2);

    ((TmdObject*)arg0->extra)->coords[5].flg = 0;
    ((TmdObject*)arg0->extra)->coords[4].flg = 0;
    ((TmdObject*)arg0->extra)->coords[3].flg = 0;
    ((TmdObject*)arg0->extra)->coords[2].flg = 0;
}

/// The 0x6B0 block's placement handler, the same body as
/// `func_actor_323300_801629F0`: copies `args`' translation into the root
/// part's local matrix and its Euler angles into the coordinate's `rot` slot,
/// rebuilds the rotation from them and clears `flg`.
s32 func_actor_323300_8016369C(Task* task, s32 msgId, GpPlaceArg* args, s32 arg3)
{
    GpCoordExt* coord;

    coord               = (GpCoordExt*)((TmdObject*)task->extra)->coords;
    coord->coord.t[0]   = args->pos.vx;
    coord->coord.t[1]   = args->pos.vy;
    coord->coord.t[2]   = args->pos.vz;
    coord->param.rot.vx = args->rot.vx;
    coord->param.rot.vy = args->rot.vy;
    coord->param.rot.vz = args->rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->flg = 0;
    return 0;
}

/// Start-preset handler for the 0x6B0 `Actor323300MtxWork` block
/// `func_actor_323300_80162BE4` parks in `Task::work`, and the twin of
/// `func_actor_323300_801628B8` (which drives the 0x504 block the same way).
/// A preset bank the block is not already on re-seeds it: the animation id is
/// reset to -1, the bank is stored and the bank's animation source goes to
/// `func_800B3F84` with the block's context, slots and matrix table. A
/// different animation id then restarts every slot 1..0x12 -- through
/// `func_800B4114` when the preset asks for it and the block has been started
/// before, through `Gp_AnimResetSlot` otherwise -- ticks them once and latches
/// `field_43C` so the next preset takes the first branch.
s32 func_actor_323300_80163718(Task* arg0, s32 arg1, Actor323300AnimPreset* arg2, s32 arg3)
{
    Actor323300MtxWork* work;
    TmdObject*          ext;
    s32                 i;

    work = (Actor323300MtxWork*)arg0->work;
    ext  = arg0->extra;
    if (arg2->field_0 != work->field_440) {
        work->field_440 = arg2->field_0;
        work->field_444 = -1;
        func_800B3F84(&work->anim, D_actor_323300_80174A70[work->field_440], ext,
                      work->pad_30C, work->slots);
    }
    if (arg2->field_4 != work->field_444) {
        work->field_444 = arg2->field_4;
        if (arg2->field_8 != 0 && work->field_43C != 0) {
            for (i = 1; i < 0x13; i++) {
                func_800B4114(&work->anim, i, work->field_444, 0, arg2->field_C);
            }
        } else {
            for (i = 1; i < 0x13; i++) {
                Gp_AnimResetSlot(&work->anim, i, work->field_444);
            }
        }
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
        work->field_43C = 1;
    }
    return 0;
}

const TaskFuncTable3 D_actor_323300_80161E6C = { {
    func_actor_323300_80162BE4,
    func_actor_323300_80162DF0,
    func_actor_323300_801634B0,
} };

/// Per-frame dispatcher of the 0x6B0-block actor: runs its spawn, tick or exit
/// state from `D_actor_323300_80161E6C` by `Task::state`, skipping the frame
/// while the global freeze byte is set.
void func_actor_323300_80163840(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_323300_80161E6C;
    if (Gp_StateF0.field_4 == 0) {
        sp.funcs[task->state](task);
    }
}
