#include "common.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/wipsys.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "rooms/acropolis_observatory.h"
#include "rooms/room_common.h"

/// One byte of gameplay state shared with the field actors, latched here when
/// the observatory task first runs during session phase 2 with nibble 0xCA
/// still clear. See `Room_Util31`, which writes the same byte.
extern s8 D_8011540A;

extern GpMsgEntry D_acropolis_observatory_8017E7B8[];
extern s32        D_acropolis_observatory_8017E7D8;
extern TaskDesc   D_acropolis_observatory_8017E7DC;
extern TaskDesc   D_acropolis_observatory_8017FE6C;

/// Per-frame paths the two streamed scenes walk the player's matrix along,
/// indexed by `CdCmd_Queue.field_1EA + 0xA8`, each with the script pair its
/// scene runs.
extern SVECTOR D_acropolis_observatory_8017E80C[];
extern s32     D_acropolis_observatory_80183480;
extern s32     D_acropolis_observatory_80183498;

extern SVECTOR D_acropolis_observatory_8017F16C[];
extern s32     D_acropolis_observatory_801834A0;
extern s32     D_acropolis_observatory_801834B8;

/// The observatory's second streamed-scene task: the same ride as
/// `func_acropolis_observatory_8017DD3C`, walking the player's matrix along
/// `D_acropolis_observatory_8017E80C` instead and ending on view index 2.
/// State 0 allocates the `AobStreamWork` block, cues the stream (slot-6 msg
/// 0xFA4), captures slot 3 and the player's coordinate matrix and republishes
/// the player's weapon to slot 3 with a 0x3E8 record. State 1 waits for the
/// stream (`CdCmd_Queue::field_1FA`), starts the script pair and reparents
/// this task under it. State 2 drives the ride, offering the pad prompt once
/// and warping slot 3 when the prompt task reports back or frame 0xE6 passes.
/// State 3 waits for slot 3 to go idle, releases it and records the view.
/// State 4 stops the stream and kills the task.
void func_acropolis_observatory_8017D9A8(Task* task)
{
    GpRec14        rec;
    RoomPlacement  place;
    s32            killed;
    AobStreamWork* work;
    AobStreamWork* blk;
    AobStreamWork* dest;
    CdCmdQueue*    queue;
    s32            weaponId;

    queue = &CdCmd_Queue;
    work  = (AobStreamWork*)task->idMap;
    switch (task->state) {
        case 0:
            blk         = Mem_Calloc(0x14, 0);
            task->idMap = (TaskIdMap*)blk;
            if (blk == NULL) {
                Task_Kill(task);
                break;
            }
            Gp_DispatchMsg(Game_GetPtrSlot(6), 0xFA4, 0, 0);
            ((AobStreamWork*)task->idMap)->target = Game_GetPtrSlot(3);
            ((AobStreamWork*)task->idMap)->mtx    = Wip_SysConfig.field_4;
            weaponId                              = Wip_SysConfig.field_21;
            rec.field_0                           = (Mc_SaveData.field_22 == 1) ? weaponId + 1 : weaponId + 0x22;
            rec.field_4                           = 1;
            rec.field_8                           = 0;
            rec.field_C                           = 0;
            rec.field_10                          = 0;
            Gp_DispatchMsg(((AobStreamWork*)task->idMap)->target, 0x3E8, (s32)&rec, 0);
            func_800E9BDC(3, 0x9FF);
            Gp_StateF0.field_4 = 1;
            task->state        = task->state + 1;
            break;

        case 1:
            if (queue->field_1FA != 0) {
                work->script             = Gp_SpawnScript18((s32)&D_acropolis_observatory_80183480,
                                                            (s32)&D_acropolis_observatory_80183498);
                Game_Session->field_13B |= 0x80;
                Task_Reparent(task, work->script);
                task->state = task->state + 1;
            }
            break;

        case 2:
            work->mtx->t[0] = D_acropolis_observatory_8017E80C[queue->field_1EA + 0xA8].vx;
            work->mtx->t[1] = D_acropolis_observatory_8017E80C[queue->field_1EA + 0xA8].vy;
            work->mtx->t[2] = D_acropolis_observatory_8017E80C[queue->field_1EA + 0xA8].vz;
            if (work->spawned != 0) {
                if (Task_PollKill(work->child, &killed) != 0) {
                    place.pos.vx = -0x968;
                    place.pos.vy = -0xBAD;
                    place.pos.vz = -0x6D4;
                    place.rot.vz = 0;
                    place.rot.vx = 0;
                    place.rot.vy = 0x400;
                    dest         = (AobStreamWork*)task->idMap;
                    Gp_DispatchMsg(dest->target, 0x3E9, (s32)&place, 0);
                    Task_SpawnFromTable(&D_acropolis_observatory_8017E7DC, 3, 0, 0);
                    task->state = task->state + 1;
                    break;
                }
            } else if (Pad_CheckFlag800() != 0) {
                work->child   = Task_SpawnFromTable(&D_acropolis_observatory_8017E7DC, 2, 0, 0);
                work->spawned = 1;
            }
            if ((queue->field_1EA + 0xA8) >= 0xE6) {
                place.pos.vx = -0x968;
                place.pos.vy = -0xBAD;
                place.pos.vz = -0x6D4;
                dest         = (AobStreamWork*)task->idMap;
                Gp_DispatchMsg(dest->target, 0x3F2, (s32)&place, 0);
                task->state = task->state + 1;
            }
            break;

        case 3:
            if (Gp_DispatchMsg(work->target, 0x3F0, 0, 0) == 0) {
                Gp_DispatchMsg(work->target, 0x3F1, 0, 0);
                Mc_SaveData.field_4 = Gp_FindViewIndex(2);
                task->state         = task->state + 1;
            }
            break;

        case 4:
            Gp_DispatchMsg(Game_GetPtrSlot(6), 0xFA5, 0, 0);
            func_800E9BDC(2, 0x9FF);
            Gp_StateF0.field_4       = 0;
            Game_Session->field_13B &= 0x7F;
            Task_Kill(task);
            break;
    }
}

/// The observatory's streamed-scene task. State 0 allocates the
/// `AobStreamWork` block, cues the stream (slot-6 msg 0xFA4), captures slot 3
/// and the player's coordinate matrix in the block, and republishes the
/// player's weapon to slot 3 with a 0x3E8 record. State 1 waits for the stream
/// to come up (`CdCmd_Queue::field_1FA`), then starts the script pair and
/// reparents this task under it. State 2 drives the ride: every frame it moves
/// the player's matrix to the `field_1EA`th entry of the path table, offers the
/// pad prompt once (`Pad_CheckFlag800`, entry 2 of the room's task table) and,
/// when the prompt task reports back, warps slot 3 with a 0x3E9 placement and
/// spawns entry 3 instead; past frame 0xE6 it sends the same placement as a
/// 0x3F2 and moves on either way. State 3 waits for slot 3 to go idle (msg
/// 0x3F0), releases it (0x3F1) and records the view in the save. State 4 stops
/// the stream (0xFA5), clears the scene flags and kills the task.
void func_acropolis_observatory_8017DD3C(Task* task)
{
    GpRec14        rec;
    RoomPlacement  place;
    s32            killed;
    AobStreamWork* work;
    AobStreamWork* blk;
    AobStreamWork* dest;
    CdCmdQueue*    queue;
    s32            weaponId;

    queue = &CdCmd_Queue;
    work  = (AobStreamWork*)task->idMap;
    switch (task->state) {
        case 0:
            blk         = Mem_Calloc(0x14, 0);
            task->idMap = (TaskIdMap*)blk;
            if (blk == NULL) {
                Task_Kill(task);
                break;
            }
            Gp_DispatchMsg(Game_GetPtrSlot(6), 0xFA4, 0, 0);
            ((AobStreamWork*)task->idMap)->target = Game_GetPtrSlot(3);
            ((AobStreamWork*)task->idMap)->mtx    = Wip_SysConfig.field_4;
            weaponId                              = Wip_SysConfig.field_21;
            rec.field_0                           = (Mc_SaveData.field_22 == 1) ? weaponId + 1 : weaponId + 0x22;
            rec.field_4                           = 1;
            rec.field_8                           = 0;
            rec.field_C                           = 0;
            rec.field_10                          = 0;
            Gp_DispatchMsg(((AobStreamWork*)task->idMap)->target, 0x3E8, (s32)&rec, 0);
            func_800E9BDC(3, 0x9FF);
            Gp_StateF0.field_4 = 1;
            task->state        = task->state + 1;
            break;

        case 1:
            if (queue->field_1FA != 0) {
                work->script             = Gp_SpawnScript18((s32)&D_acropolis_observatory_801834A0,
                                                            (s32)&D_acropolis_observatory_801834B8);
                Game_Session->field_13B |= 0x80;
                Task_Reparent(task, work->script);
                task->state = task->state + 1;
            }
            break;

        case 2:
            work->mtx->t[0] = D_acropolis_observatory_8017F16C[queue->field_1EA + 0xA8].vx;
            work->mtx->t[1] = D_acropolis_observatory_8017F16C[queue->field_1EA + 0xA8].vy;
            work->mtx->t[2] = D_acropolis_observatory_8017F16C[queue->field_1EA + 0xA8].vz + 0xC8;
            if (work->spawned != 0) {
                if (Task_PollKill(work->child, &killed) != 0) {
                    place.pos.vx = -0x8F8;
                    place.pos.vy = -0xBAD;
                    place.pos.vz = -0x2936;
                    place.rot.vz = 0;
                    place.rot.vx = 0;
                    place.rot.vy = 0x400;
                    dest         = (AobStreamWork*)task->idMap;
                    Gp_DispatchMsg(dest->target, 0x3E9, (s32)&place, 0);
                    Task_SpawnFromTable(&D_acropolis_observatory_8017E7DC, 3, 0, 0);
                    task->state = task->state + 1;
                    break;
                }
            } else if (Pad_CheckFlag800() != 0) {
                work->child   = Task_SpawnFromTable(&D_acropolis_observatory_8017E7DC, 2, 0, 0);
                work->spawned = 1;
            }
            if ((queue->field_1EA + 0xA8) >= 0xE6) {
                place.pos.vx = -0x8F8;
                place.pos.vy = -0xBAD;
                place.pos.vz = -0x2936;
                dest         = (AobStreamWork*)task->idMap;
                Gp_DispatchMsg(dest->target, 0x3F2, (s32)&place, 0);
                task->state = task->state + 1;
            }
            break;

        case 3:
            if (Gp_DispatchMsg(work->target, 0x3F0, 0, 0) == 0) {
                Gp_DispatchMsg(work->target, 0x3F1, 0, 0);
                Mc_SaveData.field_4 = Gp_FindViewIndex(4);
                task->state         = task->state + 1;
            }
            break;

        case 4:
            Gp_DispatchMsg(Game_GetPtrSlot(6), 0xFA5, 0, 0);
            func_800E9BDC(2, 0x9FF);
            Gp_StateF0.field_4       = 0;
            Game_Session->field_13B &= 0x7F;
            Task_Kill(task);
            break;
    }
}
