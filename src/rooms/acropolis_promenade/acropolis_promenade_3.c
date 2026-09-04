#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

#include "rooms/acropolis_promenade.h"
#include "rooms/room_common.h"

extern TaskDesc D_acropolis_promenade_80181148;

/// Per-frame path the promenade's streamed scene walks the player's matrix
/// along, indexed backwards by `0x45 - CdCmd_Queue.field_1EA`, plus the script
/// pair the scene runs.
extern SVECTOR D_acropolis_promenade_80181184[];
extern s32     D_acropolis_promenade_80186224;
extern s32     D_acropolis_promenade_8018623C;

void func_acropolis_promenade_8017DB48(Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;

    obj   = (TmdObject*)task->extra;
    coord = obj->field_8;
    if (Gp_GetViewIndex() == 5) {
        obj->field_C = 0x80;
    } else {
        obj->field_C = 0;
    }
    coord->flg = 0;
}

/// The promenade's streamed-scene task. State 0 allocates the `ApmStreamWork`
/// block, cues the stream (slot-6 msg 0xFA4), captures slot 3 and the player's
/// coordinate matrix in the block, and republishes the player's weapon to slot
/// 3 with a 0x3E8 record. State 1 waits for the stream to come up
/// (`CdCmd_Queue::field_1FA`), then starts the script pair and reparents this
/// task under it. State 2 drives the ride: every frame it moves the player's
/// matrix to the path entry the stream's countdown selects, offers the pad
/// prompt once (`Pad_CheckFlag800`, entry 3 of the room's task table) and, when
/// the prompt task reports back, warps slot 3 with a 0x3E9 placement and spawns
/// entry 4 instead; once the countdown is within 6 frames of the end it sends
/// the same placement as a 0x3F2 and moves on either way. State 3 waits for
/// slot 3 to go idle (msg 0x3F0), releases it (0x3F1), stops the stream
/// (0xFA5), records the room in the save and kills the task.
void func_acropolis_promenade_8017DB9C(Task* task)
{
    GpRec14        rec;
    RoomPlacement  place;
    s32            killed;
    ApmStreamWork* work;
    ApmStreamWork* blk;
    ApmStreamWork* dest;
    CdCmdQueue*    queue;
    s32            weaponId;

    queue = &CdCmd_Queue;
    work  = (ApmStreamWork*)task->idMap;
    switch (task->state) {
        case 0:
            blk         = Mem_Calloc(0x14, 0);
            task->idMap = (TaskIdMap*)blk;
            if (blk == NULL) {
                Task_Kill(task);
                break;
            }
            Gp_DispatchMsg(Game_GetPtrSlot(6), 0xFA4, 0, 0);
            ((ApmStreamWork*)task->idMap)->target = Game_GetPtrSlot(3);
            ((ApmStreamWork*)task->idMap)->mtx    = Wip_SysConfig.field_4;
            weaponId                              = Wip_SysConfig.field_21;
            rec.field_0                           = (Mc_SaveData.field_22 == 1) ? weaponId + 1 : weaponId + 0x22;
            rec.field_4                           = 1;
            rec.field_8                           = 0;
            rec.field_C                           = 0;
            rec.field_10                          = 0;
            Gp_DispatchMsg(((ApmStreamWork*)task->idMap)->target, 0x3E8, (s32)&rec, 0);
            func_800E9BDC(3, 0x9FF);
            Gp_StateF0.field_4 = 1;
            task->state        = task->state + 1;
            break;

        case 1:
            if (queue->field_1FA != 0) {
                work->script             = Gp_SpawnScript18((s32)&D_acropolis_promenade_80186224,
                                                            (s32)&D_acropolis_promenade_8018623C);
                Game_Session->field_13B |= 0x80;
                Task_Reparent(task, work->script);
                task->state = task->state + 1;
            }
            break;

        case 2:
            work->mtx->t[0] = D_acropolis_promenade_80181184[0x45 - queue->field_1EA].vx;
            work->mtx->t[1] = D_acropolis_promenade_80181184[0x45 - queue->field_1EA].vy;
            work->mtx->t[2] = D_acropolis_promenade_80181184[0x45 - queue->field_1EA].vz - 0xC8;
            if (work->spawned != 0) {
                if (Task_PollKill(work->child, &killed) != 0) {
                    place.pos.vx = 0x282;
                    place.pos.vy = 0x29;
                    place.pos.vz = D_acropolis_promenade_80181184[0x45 - queue->field_1EA].vz - 0xC8;
                    place.rot.vz = 0;
                    place.rot.vx = 0;
                    place.rot.vy = 0xC00;
                    dest         = (ApmStreamWork*)task->idMap;
                    Gp_DispatchMsg(dest->target, 0x3E9, (s32)&place, 0);
                    Task_SpawnFromTable(&D_acropolis_promenade_80181148, 4, 0, 0);
                    task->state = task->state + 1;
                    break;
                }
            } else if (Pad_CheckFlag800() != 0) {
                work->child   = Task_SpawnFromTable(&D_acropolis_promenade_80181148, 3, 0, 0);
                work->spawned = 1;
            }
            if ((0x45 - queue->field_1EA) < 6) {
                place.pos.vx = 0x282;
                place.pos.vy = 0x29;
                place.pos.vz = D_acropolis_promenade_80181184[0x45 - queue->field_1EA].vz - 0xC8;
                dest         = (ApmStreamWork*)task->idMap;
                Gp_DispatchMsg(dest->target, 0x3F2, (s32)&place, 0);
                task->state = task->state + 1;
            }
            break;

        case 3:
            if (Gp_DispatchMsg(work->target, 0x3F0, 0, 0) == 0) {
                Gp_DispatchMsg(work->target, 0x3F1, 0, 0);
                Gp_DispatchMsg(Game_GetPtrSlot(6), 0xFA5, 0, 0);
                Mc_SaveData.field_4 = 2;
                func_800E9BDC(2, 0x9FF);
                Gp_StateF0.field_4       = 0;
                Game_Session->field_13B &= 0x7F;
                Task_Kill(task);
            }
            break;
    }
}
