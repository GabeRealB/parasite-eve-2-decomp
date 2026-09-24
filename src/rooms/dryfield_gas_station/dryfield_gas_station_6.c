#include "common.h"

#include "main/display.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "rooms/dryfield_gas_station.h"

extern s32          D_dryfield_gas_station_80182E30;
extern DgsPlacement D_dryfield_gas_station_80182E44[];
extern DgsPlacement D_dryfield_gas_station_80182E5C;
extern DgsPlacement D_dryfield_gas_station_80182E74;
extern s32          D_dryfield_gas_station_80182E8C;
extern s32          D_dryfield_gas_station_8018303C;
extern u8           D_80071075;
extern s8           D_80114C12;
extern TaskDesc     D_dryfield_gas_station_8018312C[];

/// Carries out the script command in `DgsWork::field_4`, then clears it (the
/// multi-frame commands return early until they finish). 1 places the owner at
/// the first of three 0x3E9 placements and plays two sounds; 2 and 3 hand the
/// owner a `D_dryfield_gas_station_80182E30` script record as msg 0x3F4, 2 also
/// sending msg 0x3FD and 3 placing the owner at the second placement first.
/// 4 walks the owner from the first placement to the third over 30 frames with
/// msg 0x3FE before its closing 0x3F4; 5 is `func_dryfield_gas_station_80180A60`
/// written out again; 6 spawns entry 1 of `D_dryfield_gas_station_8018312C`,
/// waits a frame and turns the display back on.
void func_dryfield_gas_station_801803C0(Task* task)
{
    DgsWork* work;
    DgsWork* cur;
    DgsWork* eff;
    Task*    shared;
    union {
        GpRec14   rec;
        DgsMsg3FE move;
    } msg;
    GpRec14  script;
    GpRec14* rec;
    u16      step;

    work = (DgsWork*)task->work;
    switch (work->field_4) {
        case 0:
            break;
        case 1:
            Gp_DispatchMsg((Task*)work->owner, 0x3E9, (s32)&D_dryfield_gas_station_80182E44[0], 0);
            SndEvt_EnqueueType6(0x52010011, 0, 0);
            SndEvt_EnqueueType6(0x52010012, 0, 0);
            break;
        case 2:
            cur = (DgsWork*)task->work;
            if (cur->owner != NULL) {
                msg.rec.field_0  = (s32)&D_dryfield_gas_station_80182E30;
                msg.rec.field_4  = 1;
                msg.rec.field_8  = 0;
                msg.rec.field_C  = 0;
                msg.rec.field_10 = 0;
                Gp_DispatchMsg((Task*)cur->owner, 0x3F4, (s32)&msg.rec, 0);
            }
            Gp_DispatchMsg((Task*)work->owner, 0x3FD, 8, 0);
            break;
        case 3:
            SndEvt_EnqueueType6(0x52010013, 0, 0);
            Gp_DispatchMsg((Task*)work->owner, 0x3E9, (s32)&D_dryfield_gas_station_80182E5C, 0);
            cur = (DgsWork*)task->work;
            if (cur->owner != NULL) {
                msg.rec.field_0  = (s32)&D_dryfield_gas_station_80182E30;
                msg.rec.field_4  = 2;
                msg.rec.field_8  = 1;
                msg.rec.field_C  = 0x1E;
                msg.rec.field_10 = 0;
                Gp_DispatchMsg((Task*)cur->owner, 0x3F4, (s32)&msg.rec, 0);
            }
            break;
        case 4:
            step = work->field_6;
            switch (step) {
                case 0:
                    cur = (DgsWork*)task->work;
                    if (cur->owner != NULL) {
                        msg.rec.field_0  = (s32)&D_dryfield_gas_station_80182E30;
                        msg.rec.field_4  = 3;
                        msg.rec.field_8  = 0;
                        msg.rec.field_C  = 0;
                        msg.rec.field_10 = 0;
                        Gp_DispatchMsg((Task*)cur->owner, 0x3F4, (s32)&msg.rec, 0);
                    }
                    Gp_DispatchMsg((Task*)work->owner, 0x3FD, 8, 0);
                    Gp_DispatchMsg((Task*)work->owner, 0x3FC, 0, 0);
                    work->field_8 = 0;
                    work->field_6++;
                    return;
                case 1:
                    msg.move.x        = (D_dryfield_gas_station_80182E44[2].pos.vx - D_dryfield_gas_station_80182E44[0].pos.vx) / 30;
                    msg.move.y        = 0;
                    msg.move.z        = (D_dryfield_gas_station_80182E44[2].pos.vz - D_dryfield_gas_station_80182E44[0].pos.vz) / 30;
                    msg.move.field_10 = 0;
                    Gp_DispatchMsg((Task*)work->owner, 0x3FE, (s32)&msg.move, 0);
                    work->field_8++;
                    if (work->field_8 < 31) {
                        return;
                    }
                    // Taken before the owner check, the record's address is in
                    // $a2 early enough that the two register-valued fields are
                    // stored through it; the constant ones still go off $sp.
                    rec = &script;
                    cur = (DgsWork*)task->work;
                    if (cur->owner != NULL) {
                        script.field_0  = (s32)&D_dryfield_gas_station_80182E30;
                        script.field_4  = 0;
                        rec->field_8    = step;
                        rec->field_C    = 0xF;
                        script.field_10 = 0;
                        Gp_DispatchMsg((Task*)cur->owner, 0x3F4, (s32)rec, 0);
                    }
                    break;
                default:
                    return;
            }
            break;
        case 5:
            shared = D_dryfield_gas_station_80184BD4;
            eff    = (DgsWork*)shared->work;
            if (eff->playerEffActive != 0) {
                Gp_SpawnWeaponEff();
                eff->playerEffActive = 0;
                Gp_MsgPlayerWeapon(0);
            }
            Gp_DispatchMsg((Task*)eff->owner, 0x3E9, (s32)&D_dryfield_gas_station_80182E74, 0);
            cur = (DgsWork*)shared->work;
            if (cur->owner != NULL) {
                msg.rec.field_0  = (s32)&D_dryfield_gas_station_80182E30;
                msg.rec.field_4  = 0;
                msg.rec.field_8  = 0;
                msg.rec.field_C  = 0;
                msg.rec.field_10 = 0;
                Gp_DispatchMsg((Task*)cur->owner, 0x3F4, (s32)&msg.rec, 0);
            }
            SndEvt_EnqueueType7(0x52010011, 0x3C);
            SetDispMask(1);
            break;
        case 6:
            switch (work->field_6) {
                case 0:
                    Task_SpawnFromTable(D_dryfield_gas_station_8018312C, 1, 0x1E, 0);
                case 1:
                    work->field_6++;
                    return;
                case 2:
                    SetDispMask(1);
                    break;
            }
            break;
    }
    work->field_4 = 0;
}

/// Spawns the gas station's cutscene owner. State 0 refuses to run twice (a
/// `D_80114C12` of 1 and a live `D_80071075` both mean the cutscene is already
/// up), otherwise it parks the freshly zeroed 0x10-byte `DgsWork` block in
/// `Task::work`, fills `owner` from pointer slot 3 and republishes this task as
/// `D_dryfield_gas_station_80184BD4` so the room's script helpers can reach that block.
/// Two kills: a failed `Mem_Malloc` kills the task outright, and state 1 kills
/// it once the session has torn down (`gGameSession->eventState`). Between the two
/// it hands slot 3 the `D_dryfield_gas_station_80182E30` script record as msg
/// 0x3F4 -- only when a previous state 0 already found an owner, since the
/// reloaded `work` is dereferenced unconditionally.
void func_dryfield_gas_station_801807E0(Task* task)
{
    DgsWork* work;
    DgsWork* work2;
    GpRec14  script;

    switch (task->state) {
        case 0:
            if ((D_80114C12 != 1) && (D_80071075 == 0)) {
                work       = Mem_Malloc(0x10, false);
                task->work = (TaskIdMap*)work;
                if (work == NULL) {
                    taskKill(task);
                } else {
                    Mem_Set(work, 0, 0x10);
                    work->owner                     = gameGetPtrSlot(3);
                    D_dryfield_gas_station_80184BD4 = task;
                }
                work2 = (DgsWork*)task->work;
                if (work2->owner != 0) {
                    script.field_0  = (s32)&D_dryfield_gas_station_80182E30;
                    script.field_4  = 0;
                    script.field_8  = 0;
                    script.field_C  = 0;
                    script.field_10 = 0;
                    Gp_DispatchMsg((Task*)work2->owner, 0x3F4, (s32)&script, 0);
                }
                func_800E3FAC(0xA2, 9);
                func_800E8634((s32)&D_dryfield_gas_station_80182E8C, 0,
                              (s32)&D_dryfield_gas_station_8018303C);
                task->state = task->state + 1;
                return;
            }
            return;

        case 1:
            if (gGameSession->eventState == 0) {
                Task_RequestKill(task, 0);
                return;
            }
            func_dryfield_gas_station_801803C0(task);
            break;
    }
}

/// Latches the player-effect flag and kills the effects once. The 1 is loaded
/// before the branch and stored in the `jal` delay slot.
void func_dryfield_gas_station_80180944(void)
{
    DgsWork* work = (DgsWork*)D_dryfield_gas_station_80184BD4->work;
    if (work->playerEffActive == 0) {
        work->playerEffActive = 1;
        Gp_KillPlayerEffs();
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_gas_station/dryfield_gas_station_6", func_dryfield_gas_station_80180984);

/// Tells slot 3 that the cutscene is opening: it ends the weapon effect the
/// player may still be carrying (flag at `DgsWork::playerEffActive`), echoes the
/// equipped weapon back with msg 0x3E9 and, once the cutscene task has an owner,
/// hands that owner the `D_dryfield_gas_station_80182E30` script record as msg
/// 0x3F4. The record is a `GpRec14` built on the stack, only its first field
/// (the script pointer) set.
void func_dryfield_gas_station_80180A60(void)
{
    Task*    task;
    DgsWork* work;
    DgsWork* work2;
    GpRec14  script;

    task = D_dryfield_gas_station_80184BD4;
    work = (DgsWork*)task->work;
    if (work->playerEffActive != 0) {
        Gp_SpawnWeaponEff();
        work->playerEffActive = 0;
        Gp_MsgPlayerWeapon(0);
    }
    Gp_DispatchMsg((Task*)work->owner, 0x3E9, (s32)&D_dryfield_gas_station_80182E74, 0);
    work2 = (DgsWork*)task->work;
    if (work2->owner != 0) {
        script.field_0  = (s32)&D_dryfield_gas_station_80182E30;
        script.field_4  = 0;
        script.field_8  = 0;
        script.field_C  = 0;
        script.field_10 = 0;
        Gp_DispatchMsg((Task*)work2->owner, 0x3F4, (s32)&script, 0);
    }
    SndEvt_EnqueueType7(0x52010011, 0x3C);
    SetDispMask(1);
}
