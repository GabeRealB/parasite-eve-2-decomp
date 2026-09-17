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

INCLUDE_ASM("actors/nonmatchings/actor_341900/actor_341900", func_actor_341900_80161E58);

INCLUDE_ASM("actors/nonmatchings/actor_341900/actor_341900", func_actor_341900_80161FD0);

/// Turns the model's world translation into the light/colour matrix pair the
/// actor draws with, allocating that pair on the first frame.
void func_actor_341900_80162200(Task* arg0)
{
    TmdObject*           extra;
    TmdObject*           mdl;
    Actor341900ColorMtx* mtx;
    VECTOR               pos;

    if (arg0->state == 0) {
        extra       = (TmdObject*)arg0->extra;
        mtx         = (Actor341900ColorMtx*)Mem_Malloc(0x44, 0);
        arg0->idMap = (TaskIdMap*)mtx;
        if (mtx == NULL) {
            Task_Kill(arg0);
        } else {
            Mem_Set(mtx, 0, 0x44);
            mtx->field_40                           = (Task*)arg0->spawnArg2;
            extra->field_C                          = 0;
            ((TmdObject*)arg0->extra)->field_8->sub = &Gfx_ViewCoord;
            extra->field_1C                         = &mtx->light;
            extra->field_20                         = &mtx->color;
            extra->field_E                          = 0x1F;
            arg0->field_24                          = D_actor_341900_80163A38;
            Task_Reparent(mtx->field_40, arg0);
        }
        arg0->state++;
    }

    mdl    = (TmdObject*)arg0->extra;
    pos.vx = ((TmdObject*)arg0->extra)->field_8->workm.t[0];
    pos.vy = ((TmdObject*)arg0->extra)->field_8->workm.t[1];
    pos.vz = ((TmdObject*)arg0->extra)->field_8->workm.t[2];
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

    extra       = (TmdObject*)arg0->extra;
    work        = (Actor341900AnimWork*)Mem_Malloc(0x258, 0);
    arg0->idMap = (TaskIdMap*)work;
    if (work == NULL) {
        Task_Kill(arg0);
        return;
    }
    w = work;
    Mem_Set(w, 0, 0x258);
    w->field_248    = (Task*)arg0->spawnArg2;
    extra->field_1C = &w->light;
    extra->field_20 = &w->color;
    arg0->field_24  = D_actor_341900_80163A78;
    rec             = ((GpCdAreaRec*)Gp_GetNestedAreaRec((GpAreaKey*)&Game_Session->field_4))->field_0;
    for (; rec->field_0 != 0xFF; rec++) {
        if (rec->field_0 == 0x20) {
            break;
        }
    }
    Gp_SetTmdBytes(extra, (s8)rec->field_D, (s8)rec->field_E);
    switch (arg0->spawnArg1) {
        case 0:
            func_800B3F84(&w->ctx, D_actor_341900_801639AC, (GpAnimObj*)extra, &w->pad_154, w->slots);
            ctx = (Actor341900AnimWork*)arg0->idMap;
            for (i = 1; i < 8; i++) {
                ctx->slots[i].field_9 = 0x10;
                Gp_AnimResetSlot(&ctx->ctx, i, 0);
            }
            break;
            /* The empty loop's notes before `case 1:` make reorg predict the
             * dispatch branch taken and fill its delay slot from that arm. */
            do {
            } while (0);
        case 1:
            ((Actor341900AnimWork*)w->field_248->idMap)->field_24C = arg0;
            func_800B3F84(&w->ctx, D_actor_341900_801639B8, (GpAnimObj*)extra, &w->pad_154, w->slots);
            ctx = (Actor341900AnimWork*)arg0->idMap;
            for (i = 0; i < 4; i++) {
                ctx->slots[i].field_9 = 0x10;
                Gp_AnimResetSlot(&ctx->ctx, i, 0);
            }
            break;
        case 2:
            ((Actor341900AnimWork*)w->field_248->idMap)->field_250 = arg0;
            func_800B3F84(&w->ctx, D_actor_341900_801639C4, (GpAnimObj*)extra, &w->pad_154, w->slots);
            ctx = (Actor341900AnimWork*)arg0->idMap;
            for (i = 0; i < 4; i++) {
                ctx->slots[i].field_9 = 0x10;
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
    Actor341900TaskWork* work = (Actor341900TaskWork*)arg0->idMap;
    TmdObject*           extra;
    TmdObject*           mdl;
    GsCOORDINATE2*       coord;
    VECTOR               pos;

    if (arg0->state == 0) {
        func_actor_341900_80162330(arg0);
        work = (Actor341900TaskWork*)arg0->idMap;

        extra      = (TmdObject*)arg0->extra;
        coord      = extra->field_8;
        coord->sub = &((TmdObject*)((Task*)work->field_248)->extra)
                          ->field_8[D_actor_341900_80163A98[arg0->spawnArg1].field_6];
        coord->coord.t[0] = D_actor_341900_80163A98[arg0->spawnArg1].field_0;
        coord->coord.t[1] = D_actor_341900_80163A98[arg0->spawnArg1].field_2;
        coord->coord.t[2] = D_actor_341900_80163A98[arg0->spawnArg1].field_4;
        coord->flg        = 0;
        arg0->state++;
    }

    ((TmdObject*)arg0->extra)->field_C =
        ((TmdObject*)((Task*)work->field_248)->extra)->field_C;

    mdl    = (TmdObject*)arg0->extra;
    pos.vx = ((TmdObject*)arg0->extra)->field_8[1].workm.t[0];
    pos.vy = ((TmdObject*)arg0->extra)->field_8[1].workm.t[1];
    pos.vz = ((TmdObject*)arg0->extra)->field_8[1].workm.t[2];
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

    work = (Actor341900TaskWork*)arg0->idMap;
    switch (arg0->state) {
        case 0:
            func_actor_341900_80162330(arg0);
            ((TmdObject*)arg0->extra)->field_8->sub = &Gfx_ViewCoord;
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
            work = (Actor341900TaskWork*)arg0->idMap;
            break;
    }

    work = (Actor341900TaskWork*)arg0->idMap;
    func_actor_341900_80161E58(arg0, 8);
    func_actor_341900_80161E58(work->field_24C, 4);
    func_actor_341900_80161E58(work->field_250, 4);

    mdl    = (TmdObject*)arg0->extra;
    pos.vx = ((TmdObject*)arg0->extra)->field_8[1].workm.t[0];
    pos.vy = ((TmdObject*)arg0->extra)->field_8[1].workm.t[1];
    pos.vz = ((TmdObject*)arg0->extra)->field_8[1].workm.t[2];
    func_800D7A9C(mdl, &pos, 0, 3);
}

INCLUDE_RODATA("actors/nonmatchings/actor_341900/actor_341900", D_actor_341900_80161E20);

INCLUDE_ASM("actors/nonmatchings/actor_341900/actor_341900", func_actor_341900_801628B8);

INCLUDE_ASM("actors/nonmatchings/actor_341900/actor_341900", func_actor_341900_80162AD4);

/// Controller task of the overlay's script sequence, the one published in
/// `D_actor_341900_80164208`. State 0 clears and publishes the work block,
/// points it at the slot-3 task and at the work object of the current session
/// id, hands that id to slot 4 as message 0x7DA, spawns the five child script
/// tasks (table entries 3..7, spawn arguments 1..5) under `field_8` and the
/// two effect actors (entries 8 and 9) under the task itself, then sets the
/// two `GameSession.field_69` flags that suppress the bank-load spawn of the
/// ending and area-enter tasks. State 1 arms the stage-3 sound byte and spawns
/// the two blob tasks. State 2 waits for `GameSession.field_1` to clear -- it
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
            work        = (Actor341900Work*)Mem_Calloc(0x70U, false);
            arg0->idMap = (TaskIdMap*)work;
            if (work == NULL) {
                Task_Kill(arg0);
            } else {
                Mem_Set(work, 0U, 0x70U);
                work->field_0           = (Task*)Game_GetPtrSlot(3);
                D_actor_341900_80164208 = arg0;
                work->field_4           = (Task*)Gp_FindWorkById(
                                    Game_Session->field_6 | (Game_Session->field_7 << 8))
                                    ->field_0;
            }
            sp10.field_0 = Game_Session->field_7;
            sessionIdLo  = Game_Session->field_6;
            sp10.field_2 = 0;
            sp10.field_1 = sessionIdLo;
            Gp_DispatchMsg((Task*)Game_GetPtrSlot(4), 0x7DA, (s32)&sp10, 0x7DB);
            seqWork          = (Actor341900Work*)arg0->idMap;
            seqWork->field_8 = Task_SpawnFromTable(&D_actor_341900_80164190, 2, 0, (s32)arg0);
            for (var_s0 = 0; (u32)(var_s0 & 0xFFFF) < 5U; var_s0++) {
                temp_a2 = var_s0 & 0xFFFF;
                Task_SpawnFromTable(&D_actor_341900_80164190, temp_a2 + 3, temp_a2 + 1, (s32)seqWork->field_8);
            }
            seqWork->field_C        = Task_SpawnFromTable(&D_actor_341900_80164190, 8, 0, (s32)arg0);
            seqWork->field_10       = Task_SpawnFromTable(&D_actor_341900_80164190, 9, 0, (s32)arg0);
            Game_Session->field_69 |= 3;
            goto next;
        case 1:
            D_80062735 = 4;
            D_8007272D = 0xC;
            func_800E8634((s32)D_actor_341900_80163B48, 0, (s32)D_actor_341900_80163FB0);
        next:
            arg0->state += 1;
            return;
        case 2:
            if (Game_Session->field_1 == 0) {
                GameFlag_SetNibble(0x11D, 2);
                Task_RequestKill(arg0, 0);
                return;
            }
            func_actor_341900_801628B8(arg0);
            func_actor_341900_80162AD4(arg0);
            return;
    }
}
