#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"

#include "main/fs.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/wipsys.h"

#include "rooms/acropolis_forked_road.h"
#include "rooms/room_common.h"

extern s32      D_acropolis_forked_road_80180F3C;
extern TaskDesc D_acropolis_forked_road_80180F44;

/// The camera-target matrix the streamed scene walks along its path table.
extern MATRIX* D_80073B8C;

/// Set to 1 by the fade-out task once the scene has finished.
extern s16 D_80071076;

/// Per-frame path the streamed scene walks `D_80073B8C` along, indexed by
/// `CdCmd_Queue::field_1EA - 1` for the 0x78 frames the ride lasts.
extern SVECTOR D_acropolis_forked_road_80180F80[];

/// The script pair the streamed scene runs.
extern s32 D_acropolis_forked_road_80185058;
extern s32 D_acropolis_forked_road_80185070;

/// The script pair the return ride runs.
extern s32 D_acropolis_forked_road_80185038;
extern s32 D_acropolis_forked_road_80185050;

/// The camera view the room switches to once the return ride is over, and the
/// selector that picks which of the two weapon-id bases the 0x3E8 record uses.
extern u8 D_8007216C;
extern s8 D_8007218A;

void func_acropolis_forked_road_8017D970(void)
{
    if ((D_acropolis_forked_road_80180F3C == 0) && (Game_Session->field_8 == 2)) {
        D_acropolis_forked_road_80180F3C = 1;
        Task_SpawnFromTable(&D_acropolis_forked_road_80180F44, 2, 0, 0);
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_forked_road/acropolis_forked_road_3", func_acropolis_forked_road_8017D9CC);

/// The forked road's streamed-scene task. State 0 allocates the
/// `AfrStreamWork` block, restarts the stream frame counter, cues the stream
/// (slot-6 msg 0xFA4), captures the camera-target matrix and slot 3 in the
/// block and warps slot 3 to the head of the path with a 0x3E9 placement.
/// State 1 sends the same spot again as a 0x3F2. State 2 waits for slot 3 to
/// go idle (msg 0x3F0) and then queues the stream's CD read. State 3 waits for
/// the stream to come up (`CdCmd_Queue::field_1FA`), starts the script pair and
/// reparents this task under it. State 4 drives the ride, moving the camera
/// target to the `field_1EA`th path entry every frame until the pad interrupts
/// it or the path runs out at frame 0x78. State 5 stops the scene, restores
/// the save's room ids, arms the fade-out task and kills this task.
void func_acropolis_forked_road_8017DA24(Task* task)
{
    RoomPlacement  place;
    RoomPlacement  place2;
    u8             slot;
    AfrStreamWork* work;
    AfrStreamWork* blk;
    CdCmdQueue*    queue;

    queue = &CdCmd_Queue;
    work  = (AfrStreamWork*)task->idMap;
    switch (task->state) {
        case 0:
            blk         = Mem_Calloc(0x14, 0);
            task->idMap = (TaskIdMap*)blk;
            if (blk == NULL) {
                Task_Kill(task);
                break;
            }
            queue->field_1EA = 1;
            func_800E9BDC(3, 0x9FF);
            Gp_StateF0.field_4                    = 2;
            ((AfrStreamWork*)task->idMap)->mtx    = D_80073B8C;
            ((AfrStreamWork*)task->idMap)->target = Game_GetPtrSlot(3);
            Gp_DispatchMsg(Game_GetPtrSlot(6), 0xFA4, 0, 0);
            place.rot.vy = 0x400;
            place.rot.vx = 0;
            place.rot.vz = 0;
            place.pos.vx = D_acropolis_forked_road_80180F80[0].vx - 0x654;
            place.pos.vy = D_acropolis_forked_road_80180F80[0].vy;
            place.pos.vz = D_acropolis_forked_road_80180F80[0].vz;
            Gp_DispatchMsg(((AfrStreamWork*)task->idMap)->target, 0x3E9, (s32)&place, 0);
            task->state = task->state + 1;
            break;

        case 1:
            place2.rot.vy = 0x400;
            place2.pos.vx = D_acropolis_forked_road_80180F80[0].vx;
            place2.pos.vy = D_acropolis_forked_road_80180F80[0].vy;
            place2.pos.vz = D_acropolis_forked_road_80180F80[0].vz;
            Gp_DispatchMsg(((AfrStreamWork*)task->idMap)->target, 0x3F2, (s32)&place2, 0);
            task->state = task->state + 1;
            break;

        case 2:
            if (Gp_DispatchMsg(work->target, 0x3F0, 0, 0) == 0) {
                slot = Stream_FindSlot(&Game_Session->field_4, 0, 0);
                CdCmd_Enqueue(0x61, 0, &slot);
                task->state = task->state + 1;
            }
            break;

        case 3:
            if (queue->field_1FA != 0) {
                work->script             = Gp_SpawnScript18((s32)&D_acropolis_forked_road_80185058,
                                                            (s32)&D_acropolis_forked_road_80185070);
                Game_Session->field_13B |= 0x80;
                Task_Reparent(task, work->script);
                task->state = task->state + 1;
            }
            break;

        case 4:
            work->mtx->t[0] = D_acropolis_forked_road_80180F80[queue->field_1EA - 1].vx;
            work->mtx->t[1] = D_acropolis_forked_road_80180F80[queue->field_1EA - 1].vy;
            work->mtx->t[2] = D_acropolis_forked_road_80180F80[queue->field_1EA - 1].vz;
            if ((Pad_CheckFlag800() != 0) || ((queue->field_1EA - 1) >= 0x78)) {
                task->state = task->state + 1;
            }
            break;

        case 5:
            Gp_StateF0.field_4 = 0;
            func_800E9BDC(2, 0x9FF);
            SndEvt_EnqueueType7(0x80000000, 0);
            Mc_SaveData.field_7 = 1;
            Mc_SaveData.field_6 = 0xA;
            Mc_SaveData.field_8 = 4;
            Mc_SaveData.field_5 = 1;
            D_80071076          = 1;
            Task_Spawn(0, 0x11, 0, 0);
            Game_Session->field_13B &= 0x7F;
            Task_Kill(task);
            break;
    }
}

/// The forked road's return ride: the same streamed scene played backwards
/// along `D_acropolis_forked_road_80180F80`, whose entries this one walks from
/// the far end (`0x3B - CdCmd_Queue::field_1EA`).
///
/// State 0 allocates the `AfrStreamWork` block, captures slot 3 and the
/// camera-target matrix (`Wip_SysConfig::field_4`) in it, cues the stream
/// (slot-6 msg 0xFA4) and republishes the player's weapon to slot 3 with a
/// 0x3E8 record. State 1 waits for the stream to come up
/// (`CdCmd_Queue::field_1FA`), moves the camera target to the head of the
/// path, starts the script pair, reparents this task under it and blanks the
/// display. State 2 drives the ride: it un-blanks after two frames, walks the
/// camera target along the path, and lets the pad spawn the skip task. Once
/// that task reports done it warps slot 3 to the path's end with a 0x3E9 and
/// arms the ride's exit; otherwise the ride ends on its own when the path runs
/// down to its last 11 entries, which is sent as a 0x3F2. State 3 waits for
/// slot 3 to go idle (msg 0x3F0), releases it (0x3F1), restores the camera
/// view and the session's ride flag and kills this task.
void func_acropolis_forked_road_8017DD60(Task* task)
{
    GpRec14        rec;
    RoomPlacement  place;
    s32            sp40;
    AfrStreamWork* work;
    AfrStreamWork* blk;
    CdCmdQueue*    queue;
    s32            weaponId;

    queue = &CdCmd_Queue;
    work  = (AfrStreamWork*)task->idMap;
    switch (task->state) {
        case 0:
            blk         = Mem_Calloc(0x14, 0);
            task->idMap = (TaskIdMap*)blk;
            if (blk == NULL) {
                Task_Kill(task);
                break;
            }
            ((AfrStreamWork*)task->idMap)->target = Game_GetPtrSlot(3);
            ((AfrStreamWork*)task->idMap)->mtx    = Wip_SysConfig.field_4;
            Gp_DispatchMsg(Game_GetPtrSlot(6), 0xFA4, 0, 0);
            weaponId     = Wip_SysConfig.field_21;
            rec.field_0  = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
            rec.field_4  = 1;
            rec.field_8  = 0;
            rec.field_C  = 0;
            rec.field_10 = 0;
            Gp_DispatchMsg(((AfrStreamWork*)task->idMap)->target, 0x3E8, (s32)&rec, 0);
            func_800E9BDC(3, 0x9FF);
            Gp_StateF0.field_4 = 2;
            task->state        = task->state + 1;
            break;

        case 1:
            if (queue->field_1FA != 0) {
                work->mtx->t[0]          = D_acropolis_forked_road_80180F80[0x3B - queue->field_1EA].vx;
                work->mtx->t[1]          = D_acropolis_forked_road_80180F80[0x3B - queue->field_1EA].vy;
                work->mtx->t[2]          = D_acropolis_forked_road_80180F80[0x3B - queue->field_1EA].vz;
                work->script             = Gp_SpawnScript18((s32)&D_acropolis_forked_road_80185038,
                                                            (s32)&D_acropolis_forked_road_80185050);
                Game_Session->field_13B |= 0x80;
                Task_Reparent(task, work->script);
                SetDispMask(0);
                task->killCountdown = 0;
                task->state         = task->state + 1;
            }
            break;

        case 2:
            task->killCountdown = task->killCountdown + 1;
            if (task->killCountdown >= 3) {
                SetDispMask(1);
            }
            work->mtx->t[0] = D_acropolis_forked_road_80180F80[0x3B - queue->field_1EA].vx;
            work->mtx->t[1] = D_acropolis_forked_road_80180F80[0x3B - queue->field_1EA].vy;
            work->mtx->t[2] = D_acropolis_forked_road_80180F80[0x3B - queue->field_1EA].vz;
            if (work->skipping != 0) {
                if (Task_PollKill(work->skipper, &sp40) != 0) {
                    place.pos.vx = -0x190;
                    place.pos.vy = 1;
                    place.pos.vz = D_acropolis_forked_road_80180F80[0x3B - queue->field_1EA].vz;
                    place.rot.vz = 0;
                    place.rot.vx = 0;
                    place.rot.vy = 0xC00;
                    Gp_DispatchMsg(((AfrStreamWork*)task->idMap)->target, 0x3E9, (s32)&place, 0);
                    Task_SpawnFromTable(&D_acropolis_forked_road_80180F44, 4, 0, 0);
                    task->state = task->state + 1;
                    break;
                }
            } else if (Pad_CheckFlag800() != 0) {
                work->skipper  = Task_SpawnFromTable(&D_acropolis_forked_road_80180F44, 3, 0, 0);
                work->skipping = 1;
            }
            if ((0x3B - queue->field_1EA) < 0xB) {
                place.pos.vx = -0x190;
                place.pos.vy = 1;
                place.pos.vz = D_acropolis_forked_road_80180F80[0x3B - queue->field_1EA].vz;
                Gp_DispatchMsg(((AfrStreamWork*)task->idMap)->target, 0x3F2, (s32)&place, 0);
                task->state = task->state + 1;
            }
            break;

        case 3:
            if (Gp_DispatchMsg(work->target, 0x3F0, 0, 0) == 0) {
                Gp_DispatchMsg(work->target, 0x3F1, 0, 0);
                D_8007216C = Gp_FindViewIndex(5);
                Gp_DispatchMsg(Game_GetPtrSlot(6), 0xFA5, 0, 0);
                func_800E9BDC(2, 0x9FF);
                Gp_StateF0.field_4       = 0;
                Game_Session->field_13B &= 0x7F;
                Task_Kill(task);
            }
            break;
    }
}
