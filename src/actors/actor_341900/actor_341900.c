#include "common.h"

#include "main/gameflag.h"
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

#include "actors/actor_341900.h"

extern GpMsgEntry D_actor_341900_80163A38[];

extern void func_80143490(s32 arg0);
extern s32  D_80144A74;
extern s32  D_80144A7C;

extern TaskDesc D_actor_341900_80164190;
/// Opaque script/table blobs in the overlay's `.data`, handed to
/// `func_800E8634` (which forwards them to `Task_Spawn`) as raw addresses.
extern u8 D_actor_341900_80163B48[];
extern u8 D_actor_341900_80163FB0[];
/// Byte the other actor overlays' one-argument setters write; set to 0xC here
/// beside the stage-3 `D_80062735` mode byte.
extern s8 D_8007272D;

/// `func_800B4114` is declared locally with a signed `arg2`; see `gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Animation id `func_actor_341900_80161E58` hands every slot to
/// `func_800B4114`, indexed by `Actor341900AnimWork::field_218`; a negative
/// entry skips the call.
extern s16 D_actor_341900_801639D0[];

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

extern u8         D_actor_341900_801639AC[];
extern u8         D_actor_341900_801639B8[];
extern u8         D_actor_341900_801639C4[];
extern GpMsgEntry D_actor_341900_80163A78[];

void func_actor_341900_80162330(Task* arg0)
{
    TmdObject*           extra;
    Actor341900AnimWork* work;
    Actor341900AnimWork* ctx;
    Actor341900AnimWork* w;
    GpCdRec10*           rec;
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
    for (; rec->field_0 != 0xFF; rec++) {
        if (rec->field_0 == 0x20) {
            break;
        }
    }
    Gp_SetTmdBytes(extra, (s8)rec->field_D, (s8)rec->field_E);
    switch (arg0->spawnArg1) {
        case 0:
            func_800B3F84(&w->ctx, D_actor_341900_801639AC, (GpAnimObj*)extra, &w->pad_154, w->slots);
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
            func_800B3F84(&w->ctx, D_actor_341900_801639B8, (GpAnimObj*)extra, &w->pad_154, w->slots);
            ctx = (Actor341900AnimWork*)arg0->work;
            for (i = 0; i < 4; i++) {
                ctx->slots[i].rate = 0x10;
                Gp_AnimResetSlot(&ctx->ctx, i, 0);
            }
            break;
        case 2:
            ((Actor341900AnimWork*)w->field_248->work)->field_250 = arg0;
            func_800B3F84(&w->ctx, D_actor_341900_801639C4, (GpAnimObj*)extra, &w->pad_154, w->slots);
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
/// three child tasks and pushes the model's third coordinate, the actor's own
/// world position, through the draw matrix.
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

extern u8 D_80073BA9;
extern s8 D_8007218A;
/// Parameter record `func_actor_341900_801628B8` sends with message 0x3F4.
extern s32 D_actor_341900_801639A4;
/// Slot-3 placements and payloads sent by `func_actor_341900_801628B8`.
extern Actor341900MsgPos D_actor_341900_80163AC8;
extern Actor341900MsgPos D_actor_341900_80163AE0;
extern Actor341900MsgPos D_actor_341900_80163AF8;
extern Actor341900MsgPos D_actor_341900_80163B10;
extern Actor341900MsgPos D_actor_341900_80163B28;

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

                weaponId     = D_80073BA9;
                anim         = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
                msg.field_0  = (void*)anim;
                msg.field_4  = 1;
                msg.field_8  = 0;
                msg.field_C  = 0;
                msg.field_10 = 0;
                Gp_DispatchMsg((Task*)Game_GetPtrSlot(3), 0x3E8, (s32)&msg, 0);
            }
            break;
        case 2:
            Gp_DispatchMsg(work->field_0, 0x3F2, (s32)&D_actor_341900_80163AE0, 0);
            break;
        case 3:
            Gp_DispatchMsg(work->field_0, 0x3E9, (s32)&D_actor_341900_80163AF8, 0);
            w = (Actor341900Work*)arg0->work;
            if (w->field_0 != NULL) {
                msg.field_0  = &D_actor_341900_801639A4;
                msg.field_4  = 0;
                msg.field_8  = 0;
                msg.field_C  = 0;
                msg.field_10 = 0;
                Gp_DispatchMsg(w->field_0, 0x3F4, (s32)&msg, 0);
            }
            break;
        case 4:
            w = (Actor341900Work*)arg0->work;
            if (w->field_0 != NULL) {
                msg.field_0  = &D_actor_341900_801639A4;
                msg.field_4  = 1;
                msg.field_8  = 1;
                msg.field_C  = 10;
                msg.field_10 = 0;
                Gp_DispatchMsg(w->field_0, 0x3F4, (s32)&msg, 0);
            }
            break;
        case 5: {
            s32 weaponId;
            s32 anim;

            weaponId     = D_80073BA9;
            anim         = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
            msg.field_0  = (void*)anim;
            msg.field_4  = 9;
            msg.field_8  = 0;
            msg.field_C  = 0;
            msg.field_10 = 0;
            Gp_DispatchMsg((Task*)Game_GetPtrSlot(3), 0x3E8, (s32)&msg, 0);
        }
            Gp_DispatchMsg(work->field_0, 0x3E9, (s32)&D_actor_341900_80163B10, 0);
            break;
        case 6:
            Gp_DispatchMsg(work->field_0, 0x3E9, (s32)&D_actor_341900_80163B28, 0);
            break;
    }
    work->field_5C = 0;
}

/// Placements sent to the two effect children (`field_C` / `field_10`) as
/// message 0x7D4 (states 3 and 4), and to `field_8` (states 1 and 2).
extern Actor341900MsgPos D_actor_341900_801639D8[2];
extern Actor341900MsgPos D_actor_341900_80163A48;
extern Actor341900MsgPos D_actor_341900_80163A60;

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
                    msg.field_4 = 1;
                    msg.field_8 = 1;
                    msg.field_0 = 0;
                    msg.field_C = 10;
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
                    msg2.field_4 = 2;
                    msg2.field_8 = 1;
                    msg2.field_0 = 0;
                    msg2.field_C = 10;
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
                work->field_0           = (Task*)Game_GetPtrSlot(3);
                D_actor_341900_80164208 = arg0;
                work->field_4           = (Task*)Gp_FindWorkById(
                                    gGameSession->at4.loc.area | (gGameSession->at4.loc.stage << 8))
                                    ->field_0;
            }
            sp10.field_0 = gGameSession->at4.loc.stage;
            sessionIdLo  = gGameSession->at4.loc.area;
            sp10.field_2 = 0;
            sp10.field_1 = sessionIdLo;
            Gp_DispatchMsg((Task*)Game_GetPtrSlot(4), 0x7DA, (s32)&sp10, 0x7DB);
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
