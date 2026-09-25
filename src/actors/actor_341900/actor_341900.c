#include "common.h"

#include <psyq/libgte.h>

#include "actors/actor.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"

/// Position and Euler rotation payload: sent to slot 3 as message 0x3E9, and
/// to the child actors as message 0x7D4, whose handler
/// `func_actor_341900_801632A0` copies the longs onto the model's root
/// translation and applies the shorts as Y, X, Z rotations.
typedef struct Actor341900MsgPos {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor341900MsgPos;
STATIC_ASSERT_SIZEOF(Actor341900MsgPos, 0x18);

/// Work block of the overlay's sequence/event task -- the one
/// `D_actor_341900_80164208` points at.
///
/// `func_actor_341900_80162EFC` allocates it with `memCalloc(0x70, 0)`,
/// `Mem_Set`s the same 0x70 bytes over it and stores it in its own task's
/// `Task::work` slot (0x1C), which is not a `TaskIdMap` here, then publishes
/// that task in `D_actor_341900_80164208`. The script callbacks from
/// `func_actor_341900_80163388` on reach the block that way,
/// `(Actor341900Work*)D_actor_341900_80164208->work`; the two dispatchers
/// `func_actor_341900_801628B8` / `func_actor_341900_80162AD4` are handed the
/// same task as their argument and index it identically.
///
/// `field_0` is the `gameGetPtrSlot(3)` task the overlay's messages are aimed
/// at (0x3E8 and 0x3F3), and `field_8` / `field_C` / `field_10` are child tasks
/// the senders null-check first (0x7D5 goes to `field_8`);
/// `func_actor_341900_80163488` disposes of `field_8` by killing it and clearing
/// the slot, and `func_actor_341900_801634D0` does the same for `field_C` and
/// `field_10` in turn.
///
/// `field_5C` and `field_64` are one-shot request states: a dispatcher switches
/// on the state through a jump table and clears it back to 0 on the way out, so
/// writing it runs that state once. `func_actor_341900_80163564` requests state
/// `field_5C`, `func_actor_341900_80163584` state `field_64`, and each also
/// resets the halfword beside it -- `field_5E` / `field_66` -- the step within
/// the state, which the dispatcher compares against 0 and 1 and increments.
/// `field_68` is cleared as that step advances, and `field_6C` is a 0/1 latch
/// shared by `func_actor_341900_801633F8` (sets it, then calls
/// `Gp_KillPlayerEffs`) and `func_actor_341900_80163438` (calls
/// `Gp_SpawnWeaponEff` while it is set, then clears it).
typedef struct Actor341900Work {
    /* 0x00 */ Task*             field_0; // gameGetPtrSlot(3)
    /* 0x04 */ Task*             field_4; // Gp_FindWorkById(session slot)->field_0
    /* 0x08 */ Task*             field_8;
    /* 0x0C */ Task*             field_C;
    /* 0x10 */ Task*             field_10;
    /* 0x14 */ Actor341900MsgPos field_14;
    /* 0x2C */ Actor341900MsgPos field_2C;
    /* 0x44 */ Actor341900MsgPos field_44;
    /* 0x5C */ s16               field_5C;
    /* 0x5E */ s16               field_5E;
    /* 0x60 */ byte              pad_60[0x4];
    /* 0x64 */ s16               field_64;
    /* 0x66 */ s16               field_66;
    /* 0x68 */ s16               field_68;
    /* 0x6A */ byte              pad_6A[0x2];
    /* 0x6C */ u16               field_6C;
    /* 0x6E */ byte              pad_6E[0x2];
} Actor341900Work;
STATIC_ASSERT_SIZEOF(Actor341900Work, 0x70);

/// Session id payload sent to slot 4 as message 0x7DA, asking for the 0x7DB
/// reply. `field_0` takes `GameSession.at4.loc.stage` and `field_1`
/// `at4.loc.area`, the pair `func_actor_341900_80162EFC` also hands
/// `Gp_FindWorkById` to find the session's work object. `field_2` is a
/// selector: that function sends 0, the script callback
/// `func_actor_341900_80163334` sends the script's argument.
typedef struct Actor341900Msg7DA {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ s16 field_2;
} Actor341900Msg7DA;
STATIC_ASSERT_SIZEOF(Actor341900Msg7DA, 0x4);

/// Work block allocated by `func_actor_341900_80162200` (`Mem_Malloc(0x44, 0)`)
/// and parked in that task's `Task::work` slot, which is not a `TaskIdMap`
/// here. The two matrices are the light/colour pair the function republishes
/// onto `TmdObject::lightMtx` / `field_20` -- the pair `Gp_BindDefaultMtx`
/// otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2` -- and `field_40` is
/// the `Task::spawnArg2` spawner, which the same function reparents to the
/// actor.
typedef struct Actor341900ColorMtx {
    /* 0x00 */ MATRIX light;
    /* 0x20 */ MATRIX color;
    /* 0x40 */ Task*  field_40;
} Actor341900ColorMtx;
STATIC_ASSERT_SIZEOF(Actor341900ColorMtx, 0x44);

/// Controller task of this overlay, published by `func_actor_341900_80162EFC`
/// and read by the sequence helpers that hang their work off its `Task::work`.
extern Task* D_actor_341900_80164208;

/// 8-byte record of `D_actor_341900_80163A98`, indexed by `Task::spawnArg1`.
/// `func_actor_341900_801625B4` copies the first three halves onto part 0's
/// `GsCOORDINATE2::coord.t` and hangs that part off entry `field_6` of the
/// spawner model's own coordinate array, so a record is a spawn offset plus the
/// bone the actor is attached to. The first three records are all zero and only
/// `field_6` is under 9 in the rest, which is what sizes a model's part array.
typedef struct Actor341900SpawnPos {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
    /* 0x4 */ s16 field_4;
    /* 0x6 */ s16 field_6;
} Actor341900SpawnPos;
STATIC_ASSERT_SIZEOF(Actor341900SpawnPos, 0x8);

extern Actor341900SpawnPos D_actor_341900_80163A98[6];

/// Work block `func_actor_341900_80162330` allocates with `Mem_Malloc(0x258, 0)`
/// and parks in its own task's `Task::work` slot, which is a `TaskIdMap*` only
/// by type. `field_248` is the task that spawned this actor, copied there from
/// `Task::spawnArg2`; `func_actor_341900_801625B4` walks it to the spawner's
/// model to inherit its spawn position and its colour flag.
///
/// `field_66` is the animation frame, masked to 10 bits, and
/// `func_actor_341900_80162708` acts on two of its values: at 0x12 and 0x18 it
/// reparents the actor to a freshly spawned script and clears its message
/// state, recording each in `field_230` so a frame fires once rather than
/// every tick it is current. That whole check runs behind `field_254`, which
/// is matched against `Task::state` and so gates it to the one state the
/// actor's dispatcher handles it in. `field_24C` and `field_250` are the
/// actor's second and third child tasks, refreshed every tick alongside the
/// model.
typedef struct Actor341900TaskWork {
    /* 0x000 */ byte  pad_0[0x66];
    /* 0x066 */ u16   field_66;
    /* 0x068 */ byte  pad_68[0x1C8];
    /* 0x230 */ s32   field_230;
    /* 0x234 */ byte  pad_234[0x14];
    /* 0x248 */ Task* field_248;
    /* 0x24C */ Task* field_24C;
    /* 0x250 */ Task* field_250;
    /* 0x254 */ u16   field_254;
    /* 0x256 */ byte  pad_256[0x2];
} Actor341900TaskWork;
STATIC_ASSERT_SIZEOF(Actor341900TaskWork, 0x258);

/// The same 0x258-byte block as `Actor341900TaskWork`, seen from
/// `func_actor_341900_80162330`, which fills it: an animation context over
/// eight slots (`func_800B3F84` gets `pad_154` as its scratch area) and the
/// light/colour matrix pair the model draws with.
typedef struct Actor341900AnimWork {
    /* 0x000 */ GpAnimCtx  ctx;
    /* 0x014 */ GpAnimSlot slots[8];
    /* 0x154 */ byte       pad_154[0x80];
    /* 0x1D4 */ MATRIX     light;
    /* 0x1F4 */ MATRIX     color;
    /* 0x214 */ s32        field_214;
    /* 0x218 */ s32        field_218;
    /* 0x21C */ s32        field_21C;
    /* 0x220 */ s32        field_220;
    /* 0x224 */ s32        field_224;
    /* 0x228 */ byte       pad_228[0x20];
    /* 0x248 */ Task*      field_248;
    /* 0x24C */ Task*      field_24C;
    /* 0x250 */ Task*      field_250;
    /* 0x254 */ u16        field_254;
    /* 0x256 */ byte       pad_256[0x2];
} Actor341900AnimWork;
STATIC_ASSERT_SIZEOF(Actor341900AnimWork, 0x258);

/// Animation command `func_actor_341900_80161FD0` copies into
/// `Actor341900AnimWork::field_214..field_224`: `field_4` is the animation id
/// and the low half of `field_C` the blend handed to `func_800B4114` (0 resets
/// the slots instead).
typedef struct Actor341900AnimCmd {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ u16 field_4;
    /* 0x06 */ u16 pad_6;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
} Actor341900AnimCmd;
STATIC_ASSERT_SIZEOF(Actor341900AnimCmd, 0x14);

/// Main-executable globals with no module header yet: `D_80073BA9` is the
/// base weapon id records are numbered from, and `D_8007218A` selects the
/// alternate set -- 1 means the second block, anything else the `+0x22` one.
extern u8 D_80073BA9;
extern s8 D_8007218A;
/// Byte the other actor overlays' one-argument setters write; set to 0xC here
/// beside the stage-3 `D_80062735` mode byte.
extern s8 D_8007272D;

extern void func_80143490(s32 arg0);
extern s32  D_80144A74;
extern s32  D_80144A7C;

/// `func_800B4114` is declared locally with a signed `arg2`; see `gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Parameter record `func_actor_341900_801628B8` sends with message 0x3F4.
extern s32 D_actor_341900_801639A4;
extern u8  D_actor_341900_801639AC[];
extern u8  D_actor_341900_801639B8[];
extern u8  D_actor_341900_801639C4[];
/// Animation id `func_actor_341900_80161E58` hands every slot to
/// `func_800B4114`, indexed by `Actor341900AnimWork::field_218`; a negative
/// entry skips the call.
extern s16 D_actor_341900_801639D0[];
/// Placements sent to the two effect children (`field_C` / `field_10`) as
/// message 0x7D4 (states 3 and 4), and to `field_8` (states 1 and 2).
extern Actor341900MsgPos D_actor_341900_801639D8[2];
extern GpMsgEntry        D_actor_341900_80163A38[];
extern Actor341900MsgPos D_actor_341900_80163A48;
extern Actor341900MsgPos D_actor_341900_80163A60;
extern GpMsgEntry        D_actor_341900_80163A78[];
/// Slot-3 placements and payloads sent by `func_actor_341900_801628B8`;
/// `func_actor_341900_801635A4` also warps slot 3 to the last one.
extern Actor341900MsgPos D_actor_341900_80163AC8;
extern Actor341900MsgPos D_actor_341900_80163AE0;
extern Actor341900MsgPos D_actor_341900_80163AF8;
extern Actor341900MsgPos D_actor_341900_80163B10;
extern Actor341900MsgPos D_actor_341900_80163B28;
/// Opaque script/table blobs in the overlay's `.data`, handed to
/// `func_800E8634` (which forwards them to `Task_Spawn`) as raw addresses.
extern u8       D_actor_341900_80163B48[];
extern u8       D_actor_341900_80163FB0[];
extern TaskDesc D_actor_341900_80164190;

/// Ticks slots `(arg1 == 8)..arg1-1` of the task's animation context (slot 0
/// is skipped for the eight-slot actor). If every one of them then has
/// `field_10` bit 0x100 set, passes them the `D_actor_341900_801639D0` id and
/// returns 1; otherwise returns 0. The gotos reproduce retail's block layout.
s32 func_actor_341900_80161E58(Task* arg0, u16 arg1)
{
    Actor341900AnimWork* work;
    Actor341900AnimWork* ctx;
    u16                  i;
    u16                  done;
    u16                  start;
    u16                  anim;
    s32                  first;

    anim  = arg1 == 8;
    start = anim;
    work  = (Actor341900AnimWork*)arg0->work;
    for (i = start; i < arg1; i++) {
        Gp_AnimTickIndex(&work->ctx, i);
    }
    i    = start;
    done = 1;
    for (; i < arg1; i++) {
        if (!(work->slots[i].flags & 0x100)) {
            goto fail;
        }
    }
check:
    if (done) {
        if (D_actor_341900_801639D0[work->field_218] >= 0) {
            anim  = D_actor_341900_801639D0[work->field_218];
            ctx   = (Actor341900AnimWork*)arg0->work;
            first = arg1 == 8;
            goto loop;
        fail:
            done = 0;
            goto check;
        loop:
            for (i = first; i < arg1; i++) {
                func_800B4114(&ctx->ctx, i, anim, 0, 10);
            }
        }
        return 1;
    }
    return 0;
}

/// Points `n` slots of a task's animation context at `anim`, skipping slot 0
/// on the eight-slot actor: a zero `blend` resets each slot, otherwise
/// `func_800B4114` blends into it.
static inline void Actor341900_SetAnim(Task* task, u16 anim, u16 blend, u16 n)
{
    Actor341900AnimWork* ctx;
    u16                  i;

    ctx = (Actor341900AnimWork*)task->work;
    if (blend == 0) {
        for (i = n == 8; i < n; i++) {
            ctx->slots[i].rate = 0x10;
            Gp_AnimResetSlot(&ctx->ctx, i, anim);
        }
    } else {
        for (i = n == 8; i < n; i++) {
            func_800B4114(&ctx->ctx, i, anim, 0, blend);
        }
    }
}

/// Records an animation command in the work block and applies it to the
/// actor and both of its child tasks.
void func_actor_341900_80161FD0(Task* arg0, s32 arg1, Actor341900AnimCmd* cmd)
{
    Actor341900AnimWork* work;

    work            = (Actor341900AnimWork*)arg0->work;
    work->field_214 = cmd->field_0;
    work->field_218 = work->field_254 = cmd->field_4;
    work->field_21C                   = cmd->field_8;
    work->field_220                   = cmd->field_C;
    work->field_224                   = cmd->field_10;
    Actor341900_SetAnim(arg0, cmd->field_4, cmd->field_C, 8);
    Actor341900_SetAnim(work->field_24C, cmd->field_4, cmd->field_C, 4);
    Actor341900_SetAnim(work->field_250, cmd->field_4, cmd->field_C, 4);
}

/// Turns the model's world translation into the light/colour matrix pair the
/// actor draws with, allocating that pair on the first frame.
void func_actor_341900_80162200(Task* arg0)
{
    TmdObject*           extra;
    TmdObject*           mdl;
    Actor341900ColorMtx* mtx;
    VECTOR               pos;

    if (arg0->state == 0) {
        extra      = (TmdObject*)arg0->extra;
        mtx        = (Actor341900ColorMtx*)Mem_Malloc(0x44, 0);
        arg0->work = (TaskIdMap*)mtx;
        if (mtx == NULL) {
            taskKill(arg0);
        } else {
            Mem_Set(mtx, 0, 0x44);
            mtx->field_40                          = (Task*)arg0->spawnArg2;
            extra->flags                           = 0;
            ((TmdObject*)arg0->extra)->coords->sub = &gGfxViewCoord;
            extra->lightMtx                        = &mtx->light;
            extra->colorMtx                        = &mtx->color;
            extra->otOffset                        = 0x1F;
            arg0->msgTable                         = D_actor_341900_80163A38;
            Task_Reparent(mtx->field_40, arg0);
        }
        arg0->state++;
    }

    mdl    = (TmdObject*)arg0->extra;
    pos.vx = ((TmdObject*)arg0->extra)->coords->workm.t[0];
    pos.vy = ((TmdObject*)arg0->extra)->coords->workm.t[1];
    pos.vz = ((TmdObject*)arg0->extra)->coords->workm.t[2];
    func_800D7A9C(mdl, &pos, 0, 3);
}

/// Shared first tick of the actor's three parts, selected by `spawnArg1`:
/// allocates and clears the `Actor341900AnimWork` block, binds its matrices to
/// the model, applies the area's tpage/clut, sets up the part's animation
/// slots (eight for the body, four for each of the two children, which also
/// register themselves with the spawner) and reparents the spawner to it.
void func_actor_341900_80162330(Task* arg0)
{
    TmdObject*           extra;
    Actor341900AnimWork* work;
    Actor341900AnimWork* ctx;
    Actor341900AnimWork* w;
    GpAreaPlace*         rec;
    u16                  i;

    extra      = (TmdObject*)arg0->extra;
    work       = (Actor341900AnimWork*)Mem_Malloc(0x258, 0);
    arg0->work = (TaskIdMap*)work;
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    w = work;
    Mem_Set(w, 0, 0x258);
    w->field_248    = (Task*)arg0->spawnArg2;
    extra->lightMtx = &w->light;
    extra->colorMtx = &w->color;
    arg0->msgTable  = D_actor_341900_80163A78;
    rec             = ((GpCdAreaRec*)Gp_GetNestedAreaRec((GpAreaKey*)&gGameSession->at4.loc))->field_0;
    for (; rec->entryId != 0xFF; rec++) {
        if (rec->entryId == 0x20) {
            break;
        }
    }
    Gp_SetTmdBytes(extra, (s8)rec->tpage, (s8)rec->clut);
    switch (arg0->spawnArg1) {
        case 0:
            func_800B3F84(&w->ctx, D_actor_341900_801639AC, extra, &w->pad_154, w->slots);
            ctx = (Actor341900AnimWork*)arg0->work;
            for (i = 1; i < 8; i++) {
                ctx->slots[i].rate = 0x10;
                Gp_AnimResetSlot(&ctx->ctx, i, 0);
            }
            break;
            /* The empty loop's notes before `case 1:` make reorg predict the
             * dispatch branch taken and fill its delay slot from that arm. */
            do {
            } while (0);
        case 1:
            ((Actor341900AnimWork*)w->field_248->work)->field_24C = arg0;
            func_800B3F84(&w->ctx, D_actor_341900_801639B8, extra, &w->pad_154, w->slots);
            ctx = (Actor341900AnimWork*)arg0->work;
            for (i = 0; i < 4; i++) {
                ctx->slots[i].rate = 0x10;
                Gp_AnimResetSlot(&ctx->ctx, i, 0);
            }
            break;
        case 2:
            ((Actor341900AnimWork*)w->field_248->work)->field_250 = arg0;
            func_800B3F84(&w->ctx, D_actor_341900_801639C4, extra, &w->pad_154, w->slots);
            ctx = (Actor341900AnimWork*)arg0->work;
            for (i = 0; i < 4; i++) {
                ctx->slots[i].rate = 0x10;
                Gp_AnimResetSlot(&ctx->ctx, i, 0);
            }
            break;
    }
    Task_Reparent(w->field_248, arg0);
}

/// Attaches the actor to the bone its spawn record names, copies that record's
/// offset onto the part's coordinate, inherits the spawner's colour flag and
/// pushes the part's translation through the draw matrix.
void func_actor_341900_801625B4(Task* arg0)
{
    Actor341900TaskWork* work = (Actor341900TaskWork*)arg0->work;
    TmdObject*           extra;
    TmdObject*           mdl;
    GsCOORDINATE2*       coord;
    VECTOR               pos;

    if (arg0->state == 0) {
        func_actor_341900_80162330(arg0);
        work = (Actor341900TaskWork*)arg0->work;

        extra      = (TmdObject*)arg0->extra;
        coord      = extra->coords;
        coord->sub = &((TmdObject*)((Task*)work->field_248)->extra)
                          ->coords[D_actor_341900_80163A98[arg0->spawnArg1].field_6];
        coord->coord.t[0] = D_actor_341900_80163A98[arg0->spawnArg1].field_0;
        coord->coord.t[1] = D_actor_341900_80163A98[arg0->spawnArg1].field_2;
        coord->coord.t[2] = D_actor_341900_80163A98[arg0->spawnArg1].field_4;
        coord->flg        = 0;
        arg0->state++;
    }

    ((TmdObject*)arg0->extra)->flags =
        ((TmdObject*)((Task*)work->field_248)->extra)->flags;

    mdl    = (TmdObject*)arg0->extra;
    pos.vx = ((TmdObject*)arg0->extra)->coords[1].workm.t[0];
    pos.vy = ((TmdObject*)arg0->extra)->coords[1].workm.t[1];
    pos.vz = ((TmdObject*)arg0->extra)->coords[1].workm.t[2];
    func_800D7A9C(mdl, &pos, 0, 3);
}

/// Per-state body of the actor task. State 0 publishes the part's draw
/// matrix, state 1 watches the work block's frame counter for the two frames
/// that respawn the actor's script, and every state but 0 then refreshes the
/// three child tasks and pushes the translation of the model's second
/// coordinate through the draw matrix.
void func_actor_341900_80162708(Task* arg0)
{
    Actor341900TaskWork* work;
    TmdObject*           mdl;
    VECTOR               pos;
    s32                  frame;

    work = (Actor341900TaskWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            func_actor_341900_80162330(arg0);
            ((TmdObject*)arg0->extra)->coords->sub = &gGfxViewCoord;
            arg0->state++;
            return;
        case 1:
            if (work->field_254 == arg0->state) {
                frame = work->field_66 & 0x3FF;
                if ((frame == 0x12) && (work->field_230 != frame)) {
                    Task_Reparent(arg0,
                                  Gp_SpawnScript18((s32)&D_80144A74, (s32)&D_80144A7C));
                    func_80143490(3);
                }
                frame = work->field_66 & 0x3FF;
                if ((frame == 0x18) && (work->field_230 != frame)) {
                    Task_Reparent(arg0,
                                  Gp_SpawnScript18((s32)&D_80144A74, (s32)&D_80144A7C));
                    func_80143490(3);
                }
                work->field_230 = work->field_66 & 0x3FF;
            }
            work = (Actor341900TaskWork*)arg0->work;
            break;
    }

    work = (Actor341900TaskWork*)arg0->work;
    func_actor_341900_80161E58(arg0, 8);
    func_actor_341900_80161E58(work->field_24C, 4);
    func_actor_341900_80161E58(work->field_250, 4);

    mdl    = (TmdObject*)arg0->extra;
    pos.vx = ((TmdObject*)arg0->extra)->coords[1].workm.t[0];
    pos.vy = ((TmdObject*)arg0->extra)->coords[1].workm.t[1];
    pos.vz = ((TmdObject*)arg0->extra)->coords[1].workm.t[2];
    func_800D7A9C(mdl, &pos, 0, 3);
}

/// Runs the one-shot request in `Actor341900Work::field_5C` against the slot-3
/// task after pinging it with message 0x3ED, then clears the request. States 1
/// and 5 install an animation set (message 0x3E8) around a placement (0x3E9),
/// 2 sends 0x3F2, 3 and 4 send 0x3F4 and 6 is a bare placement.
void func_actor_341900_801628B8(Task* arg0)
{
    Actor341900Work* work;
    Actor341900Work* w;
    GpAnimArg        msg;

    work = (Actor341900Work*)arg0->work;
    if (work->field_0 != NULL) {
        Gp_DispatchMsg(work->field_0, 0x3ED, 0, 0);
    }
    switch ((u16)work->field_5C) {
        case 0:
            break;
        case 1:
            Gp_DispatchMsg(work->field_0, 0x3E9, (s32)&D_actor_341900_80163AC8, 0);
            {
                s32 weaponId;
                s32 anim;

                weaponId            = D_80073BA9;
                anim                = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
                msg.animBlock.index = anim;
                msg.field_4         = 1;
                msg.field_8         = 0;
                msg.field_C         = 0;
                msg.field_10        = 0;
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&msg, 0);
            }
            break;
        case 2:
            Gp_DispatchMsg(work->field_0, 0x3F2, (s32)&D_actor_341900_80163AE0, 0);
            break;
        case 3:
            Gp_DispatchMsg(work->field_0, 0x3E9, (s32)&D_actor_341900_80163AF8, 0);
            w = (Actor341900Work*)arg0->work;
            if (w->field_0 != NULL) {
                msg.animBlock.ptr = &D_actor_341900_801639A4;
                msg.field_4       = 0;
                msg.field_8       = 0;
                msg.field_C       = 0;
                msg.field_10      = 0;
                Gp_DispatchMsg(w->field_0, 0x3F4, (s32)&msg, 0);
            }
            break;
        case 4:
            w = (Actor341900Work*)arg0->work;
            if (w->field_0 != NULL) {
                msg.animBlock.ptr = &D_actor_341900_801639A4;
                msg.field_4       = 1;
                msg.field_8       = 1;
                msg.field_C       = 10;
                msg.field_10      = 0;
                Gp_DispatchMsg(w->field_0, 0x3F4, (s32)&msg, 0);
            }
            break;
        case 5: {
            s32 weaponId;
            s32 anim;

            weaponId            = D_80073BA9;
            anim                = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
            msg.animBlock.index = anim;
            msg.field_4         = 9;
            msg.field_8         = 0;
            msg.field_C         = 0;
            msg.field_10        = 0;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&msg, 0);
        }
            Gp_DispatchMsg(work->field_0, 0x3E9, (s32)&D_actor_341900_80163B10, 0);
            break;
        case 6:
            Gp_DispatchMsg(work->field_0, 0x3E9, (s32)&D_actor_341900_80163B28, 0);
            break;
    }
    work->field_5C = 0;
}

/// Runs the one-shot request in `Actor341900Work::field_64`, stepping through
/// `field_66`. States 1 and 2 hand `field_8` an animation (0x7D3) and a
/// placement (0x7D4); state 1 then slides it along x once `field_68` reaches
/// 0x10, state 2 spawns four effects on its third coordinate after 0x3C ticks.
/// State 3 places both effect children and sends `field_4` 0x7D5; state 4
/// places them and moves them apart along z every tick.
void func_actor_341900_80162AD4(Task* arg0)
{
    Actor341900Work* work;
    GpAnimArg        msg;
    GpAnimArg        msg2;
    SVECTOR          ofs;

    work = (Actor341900Work*)arg0->work;
    switch ((u16)work->field_64) {
        case 1:
            switch ((u16)work->field_66) {
                case 0:
                    msg.field_4         = 1;
                    msg.field_8         = 1;
                    msg.animBlock.index = 0;
                    msg.field_C         = 10;
                    Gp_DispatchMsg(work->field_8, 0x7D3, (s32)&msg, 0);
                    work->field_14.pos.vx = D_actor_341900_80163A48.pos.vx;
                    work->field_14.pos.vy = D_actor_341900_80163A48.pos.vy;
                    work->field_14.pos.vz = D_actor_341900_80163A48.pos.vz;
                    work->field_14.rot.vx = D_actor_341900_80163A48.rot.vx;
                    work->field_14.rot.vy = D_actor_341900_80163A48.rot.vy;
                    work->field_14.rot.vz = D_actor_341900_80163A48.rot.vz;
                    Gp_DispatchMsg(work->field_8, 0x7D4, (s32)&work->field_14, 0);
                    work->field_68 = 0;
                    work->field_66++;
                    break;
                case 1:
                    if ((u16)work->field_68 >= 0x10) {
                        work->field_14.pos.vx += 0x1E;
                        Gp_DispatchMsg(work->field_8, 0x7D4, (s32)&work->field_14, 0);
                    } else {
                        work->field_68++;
                    }
                    break;
            }
            break;
        case 2:
            switch ((u16)work->field_66) {
                case 0:
                    msg2.field_4         = 2;
                    msg2.field_8         = 1;
                    msg2.animBlock.index = 0;
                    msg2.field_C         = 10;
                    Gp_DispatchMsg(work->field_8, 0x7D3, (s32)&msg2, 0);
                    Gp_DispatchMsg(work->field_8, 0x7D4, (s32)&D_actor_341900_80163A60, 0);
                    work->field_68 = 0;
                    work->field_66++;
                    break;
                case 1:
                    work->field_68++;
                    if ((u16)work->field_68 > 0x3C) {
                        ofs.vx = 0;
                        ofs.vy = -0x64;
                        ofs.vz = 0x1194;
                        Gp_SpawnEff(0x60196, &((TmdObject*)work->field_8->extra)->coords[2], 0x04402800, &ofs);
                        ofs.vx = 0xC8;
                        ofs.vy = 0xC8;
                        ofs.vz = 0x1194;
                        Gp_SpawnEff(0x60196, &((TmdObject*)work->field_8->extra)->coords[2], 0x04402800, &ofs);
                        ofs.vx = -0xC8;
                        ofs.vy = 0xC8;
                        ofs.vz = 0x1194;
                        Gp_SpawnEff(0x60196, &((TmdObject*)work->field_8->extra)->coords[2], 0x04402800, &ofs);
                        ofs.vx = 0;
                        ofs.vy = 0xC8;
                        ofs.vz = 0x1194;
                        Gp_SpawnEff(0x60196, &((TmdObject*)work->field_8->extra)->coords[2], 0x04402800, &ofs);
                        work->field_64 = 0;
                    }
                    break;
            }
            break;
        case 3:
            Gp_DispatchMsg(work->field_C, 0x7D4, (s32)&D_actor_341900_801639D8[0], 0);
            Gp_DispatchMsg(work->field_10, 0x7D4, (s32)&D_actor_341900_801639D8[1], 0);
            Gp_DispatchMsg(work->field_4, 0x7D5, 2, 0);
            work->field_64 = 0;
            break;
        case 4:
            switch ((u16)work->field_66) {
                case 0:
                    work->field_2C.pos.vx = D_actor_341900_801639D8[0].pos.vx;
                    work->field_2C.pos.vy = D_actor_341900_801639D8[0].pos.vy;
                    work->field_2C.pos.vz = D_actor_341900_801639D8[0].pos.vz;
                    work->field_2C.rot.vx = D_actor_341900_801639D8[0].rot.vx;
                    work->field_2C.rot.vy = D_actor_341900_801639D8[0].rot.vy;
                    work->field_2C.rot.vz = D_actor_341900_801639D8[0].rot.vz;
                    work->field_44.pos.vx = D_actor_341900_801639D8[0].pos.vx;
                    work->field_44.pos.vy = D_actor_341900_801639D8[0].pos.vy;
                    work->field_44.pos.vz = D_actor_341900_801639D8[0].pos.vz;
                    work->field_44.rot.vx = D_actor_341900_801639D8[0].rot.vx;
                    work->field_44.rot.vy = D_actor_341900_801639D8[0].rot.vy;
                    work->field_44.rot.vz = D_actor_341900_801639D8[0].rot.vz;
                    work->field_66++;
                case 1:
                    work->field_2C.pos.vz -= 0x14;
                    work->field_44.pos.vz += 0x14;
                    Gp_DispatchMsg(work->field_C, 0x7D4, (s32)&work->field_2C, 0);
                    Gp_DispatchMsg(work->field_10, 0x7D4, (s32)&work->field_44, 0);
                    break;
            }
            break;
        case 0:
        default:
            work->field_64 = 0;
            break;
    }
}

/// Controller task of the overlay's script sequence, the one published in
/// `D_actor_341900_80164208`. State 0 clears and publishes the work block,
/// points it at the slot-3 task and at the work object of the current session
/// id, hands that id to slot 4 as message 0x7DA, spawns the five child script
/// tasks (table entries 3..7, spawn arguments 1..5) under `field_8` and the
/// two effect actors (entries 8 and 9) under the task itself, then sets the
/// two `GameSession.flowFlags` flags that suppress the bank-load spawn of the
/// ending and area-enter tasks. State 1 arms the stage-3 sound byte and spawns
/// the two blob tasks. State 2 waits for `GameSession.eventState` to clear -- it
/// sets game flag nibble 0x11D and kills the task when it does -- and
/// otherwise runs the two child dispatchers.
void func_actor_341900_80162EFC(Task* arg0)
{
    Actor341900Msg7DA sp10;
    Actor341900Work*  work;
    Actor341900Work*  seqWork;
    u8                sessionIdLo;
    s32               temp_a2;
    u16               var_s0;

    switch (arg0->state) {
        case 0:
            work       = (Actor341900Work*)memCalloc(0x70U, false);
            arg0->work = (TaskIdMap*)work;
            if (work == NULL) {
                taskKill(arg0);
            } else {
                Mem_Set(work, 0U, 0x70U);
                work->field_0           = gameGetPtrSlot(3);
                D_actor_341900_80164208 = arg0;
                work->field_4           = (Task*)Gp_FindWorkById(
                                    gGameSession->at4.loc.area | (gGameSession->at4.loc.stage << 8))
                                    ->field_0;
            }
            sp10.field_0 = gGameSession->at4.loc.stage;
            sessionIdLo  = gGameSession->at4.loc.area;
            sp10.field_2 = 0;
            sp10.field_1 = sessionIdLo;
            Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&sp10, 0x7DB);
            seqWork          = (Actor341900Work*)arg0->work;
            seqWork->field_8 = Task_SpawnFromTable(&D_actor_341900_80164190, 2, 0, (s32)arg0);
            for (var_s0 = 0; (u32)(var_s0 & 0xFFFF) < 5U; var_s0++) {
                temp_a2 = var_s0 & 0xFFFF;
                Task_SpawnFromTable(&D_actor_341900_80164190, temp_a2 + 3, temp_a2 + 1, (s32)seqWork->field_8);
            }
            seqWork->field_C         = Task_SpawnFromTable(&D_actor_341900_80164190, 8, 0, (s32)arg0);
            seqWork->field_10        = Task_SpawnFromTable(&D_actor_341900_80164190, 9, 0, (s32)arg0);
            gGameSession->flowFlags |= 3;
            goto next;
        case 1:
            D_80062735 = 4;
            D_8007272D = 0xC;
            func_800E8634((s32)D_actor_341900_80163B48, 0, (s32)D_actor_341900_80163FB0);
        next:
            arg0->state += 1;
            return;
        case 2:
            if (gGameSession->eventState == 0) {
                GameFlag_SetNibble(0x11D, 2);
                Task_RequestKill(arg0, 0);
                return;
            }
            func_actor_341900_801628B8(arg0);
            func_actor_341900_80162AD4(arg0);
            return;
    }
}

/// Fade task, entry 1 of the overlay's task table: its first tick allocates
/// the channel block and seeds every channel at 0xFF; each tick then draws the
/// full-screen fade overlay and steps the channels down by `spawnArg1`,
/// killing the task once `r` has gone negative.
void func_actor_341900_80163148(Task* arg0)
{
    ActorFadeWork* fade;
    ActorFadeWork* alloc;

    fade = (ActorFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (ActorFadeWork*)memCalloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            fade         = alloc;
            fade->b      = 0xFF;
            fade->g      = 0xFF;
            fade->r      = 0xFF;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->r, 2);
            fade->r -= (u16)arg0->spawnArg1;
            fade->g -= (u16)arg0->spawnArg1;
            fade->b -= (u16)arg0->spawnArg1;
            if (fade->r < 0) {
                taskKill(arg0);
            }
            break;
    }
}

/// Message 0x7D5 handler of both of the overlay's message tables: sets the
/// draw bits of the task's `TmdObject` from the mode in `arg2`. Mode 0 sets
/// 0x80 and clears 0x4, mode 1 clears both, mode 2 sets both.
void func_actor_341900_80163224(Task* arg0, s32 arg1, s32 arg2)
{
    TmdObject* extra;

    extra = (TmdObject*)arg0->extra;
    switch (arg2) {
        case 0:
            extra->flags = (extra->flags | 0x80) & 0xFFFB;
            return;
        case 1:
            extra->flags = extra->flags & 0xFF7B;
            return;
        case 2:
            extra->flags = extra->flags | 0x84;
            return;
    }
}

/// Message 0x7D4 handler of both of the overlay's message tables: copies the
/// placement onto the model's root coordinate, the three longs as its
/// translation and the three angles as its rotation (Y, then X, then Z), and
/// marks the coordinate dirty.
void func_actor_341900_801632A0(Task* task, s32 arg1, Actor341900MsgPos* placement)
{
    GsCOORDINATE2* coord;
    MATRIX*        mtx;

    coord             = ((TmdObject*)task->extra)->coords;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    mtx               = &coord->coord;
    coord->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixY(mtx, placement->rot.vy, 1);
    Gfx_RotMatrixX(mtx, placement->rot.vx, 0);
    Gfx_RotMatrixZ(mtx, placement->rot.vz, 0);
    coord->flg = 0;
}

/// Message 0x7DB handler of the actor's second message table; ignores it.
void func_actor_341900_8016332C(void)
{
}

/// Script callback: sends message 0x7DA to the slot-4 task, tagged with the
/// current session's two id bytes and the script's selector, asking for the
/// 0x7DB reply.
void func_actor_341900_80163334(s16 arg0)
{
    Actor341900Msg7DA msg;

    msg.field_0 = gGameSession->at4.loc.stage;
    msg.field_1 = gGameSession->at4.loc.area;
    msg.field_2 = arg0;
    Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
}

void func_actor_341900_80163388(s32 arg0)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->work;

    Gp_DispatchMsg(work->field_8, 0x7D5, arg0, 0);
}

void func_actor_341900_801633C0(s32 arg0)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->work;

    Gp_DispatchMsg(work->field_0, 0x3F3, arg0, 0);
}

void func_actor_341900_801633F8(void)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->work;

    if (work->field_6C == 0) {
        work->field_6C = 1;
        Gp_KillPlayerEffs();
    }
}

void func_actor_341900_80163438(void)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->work;

    if (work->field_6C != 0) {
        Gp_SpawnWeaponEff();
        work->field_6C = 0;
        Gp_MsgPlayerWeapon(0);
    }
}

void func_actor_341900_80163488(void)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->work;

    if (work->field_8 != NULL) {
        taskKill(work->field_8);
        work->field_8 = NULL;
    }
}

void func_actor_341900_801634D0(void)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->work;

    if (work->field_C != NULL) {
        taskKill(work->field_C);
        work->field_C = NULL;
    }
    if (work->field_10 != NULL) {
        taskKill(work->field_10);
        work->field_10 = NULL;
    }
}

void func_actor_341900_80163534(void)
{
    Task_SpawnFromTable(&D_actor_341900_80164190, 1, 9, 0);
}

void func_actor_341900_80163564(s16 arg0)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->work;

    work->field_5C = arg0;
    work->field_5E = 0;
}

void func_actor_341900_80163584(s16 arg0)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->work;

    work->field_64 = arg0;
    work->field_66 = 0;
}

/// Installs one animation set on slot 3 (message 0x3E8) and then warps it to
/// the overlay's fixed placement (message 0x3E9), cancelling any pending CD
/// command replacement on the way out. The set is `D_80073BA9 + 1` for the
/// alternate weapon block and `D_80073BA9 + 0x22` for the base one; its
/// `field_4` is 9, the rest of the frame is zero.
void func_actor_341900_801635A4(void)
{
    Actor341900Work* work;
    GpAnimArg        msg;
    s32              weaponId;
    s32              anim;

    work                = (Actor341900Work*)D_actor_341900_80164208->work;
    weaponId            = D_80073BA9;
    anim                = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
    msg.animBlock.index = anim;
    msg.field_4         = 9;
    msg.field_8         = 0;
    msg.field_C         = 0;
    msg.field_10        = 0;
    Gp_DispatchMsg(work->field_0, 0x3E8, (s32)&msg, 0);
    Gp_DispatchMsg(work->field_0, 0x3E9, (s32)&D_actor_341900_80163B28, 0);
    CdCmd_CancelReplaceAndActivate();
}

/// Script callback: queues the replacement overlay load.
void func_actor_341900_80163638(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

/// Script callback: queues the overlay load.
void func_actor_341900_80163658(void)
{
    CdCmd_EnqueueOverlay81();
}

/// Script callback: restores the stream random state, then cancels the
/// pending overlay replacement and activates the loaded one.
void func_actor_341900_80163678(void)
{
    Gp_RestoreStreamRng();
    CdCmd_CancelReplaceAndActivate();
}
