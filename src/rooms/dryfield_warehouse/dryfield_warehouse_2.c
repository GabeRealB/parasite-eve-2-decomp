#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"

#include "main/gameflow.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/dryfield_warehouse.h"

extern u8       D_80071075;
extern u8       D_80073BA9;
extern s8       D_8007218A;
extern s8       D_8007216D;
extern s8       D_80114C12;
extern s32      D_dryfield_warehouse_8017F848;
extern s32      D_dryfield_warehouse_8017F850;
extern s32      D_dryfield_warehouse_8017F868;
extern s16      D_dryfield_warehouse_801821C4;
extern s32      D_dryfield_warehouse_8017F880;
extern s32      D_dryfield_warehouse_8017FA00;
extern TaskDesc D_dryfield_warehouse_8017FB08;

/// Message handler of the warehouse's cutscene task. Message 0 re-opens the
/// room: it kills the screen-fade task still on `D_dryfield_warehouse_801821C0`,
/// turns the display back on and, while `DwhWork::playerEffActive` is up, ends
/// the weapon effect and re-sends the player-weapon record. The owner is then
/// handed that same 0x3E8 record -- `GpRec14` field 0 is the equipped weapon's
/// animation id, `D_80073BA9` plus 1 or 0x22 depending on `D_8007218A`, with 1
/// and 0 padding it out -- followed by the room's placement as msg 0x3E9.
///
/// The session's weapon id is synced to 2 once, and `D_dryfield_warehouse_801821C4`
/// records whether this handler did that: message 1 mirrors the session's view
/// and object tables back onto that flag. The 1 shared by the record and the
/// flag is one callee-saved value because both outlive the dispatches.
void func_dryfield_warehouse_8017DA58(s32 arg0)
{
    DwhWork* work;
    GpRec14  rec;
    s32      weaponId;
    s32      anim;

    switch (arg0) {
        case 0:
            if (D_dryfield_warehouse_801821C0 != 0) {
                taskKill(D_dryfield_warehouse_801821C0);
            }
            SetDispMask(1);
            work = (DwhWork*)D_dryfield_warehouse_801821BC->work;
            if (work->playerEffActive != 0) {
                Gp_SpawnWeaponEff();
                work->playerEffActive = 0;
                Gp_MsgPlayerWeapon(0);
            }
            weaponId     = D_80073BA9;
            anim         = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
            rec.field_0  = anim;
            rec.field_4  = 1;
            rec.field_8  = 0;
            rec.field_C  = 0;
            rec.field_10 = 1;
            Gp_DispatchMsg((Task*)work->owner, 0x3E8, (s32)&rec, 0);
            Gp_DispatchMsg((Task*)work->owner, 0x3E9, (s32)&D_dryfield_warehouse_8017F868, 0);
            if (Mc_SaveData.at4.loc.room != 2) {
                Mc_SaveData.at4.loc.room      = 2;
                gGameSession->at4.loc.room    = 2;
                D_dryfield_warehouse_801821C4 = 1;
                return;
            }
            D_dryfield_warehouse_801821C4 = 0;
            return;
        case 1:
            if (D_dryfield_warehouse_801821C4 != 0) {
                gGameSession->viewDirty     = arg0;
                gGameSession->roomObjsDirty = arg0;
            }
            return;
    }
}

/// Per-frame script step of the warehouse's cutscene task, dispatched on
/// `DwhWork::field_4` with `field_6` as the sub-step. States 1 and 5 advance a
/// frame counter in `field_E` and play a sound every 60 frames; 3 re-sends the
/// weapon record and spawns five staggered effects until `field_8` reaches 36;
/// 4 and 5 draw a white fade. State 2 spawns entry 1 of the room task table into
/// `D_dryfield_warehouse_801821C0`; it, state 0 and any unknown state reset
/// `field_4` to 0, as does state 3 once its timer runs out.
void func_dryfield_warehouse_8017DBB0(Task* arg0)
{
    DwhWork* work;
    DwhWork* shared;
    DwhWork* cur;
    union {
        GpRec14 rec;
        SVECTOR pos;
    } msg;
    s32 weaponId;
    s32 anim;

    work = (DwhWork*)arg0->work;
    switch (work->field_4) {
        case 0:
            break;
        case 1:
            switch (work->field_6) {
                case 0:
                    SetDispMask(1);
                    Task_SpawnFromTable(&D_dryfield_warehouse_8017FB08, 2, 8, 0);
                    Gp_KillPlayerEffs();
                    work->playerEffActive = 1;
                    cur                   = (DwhWork*)arg0->work;
                    if (cur->owner != NULL) {
                        msg.rec.field_0  = (s32)&D_dryfield_warehouse_8017F848;
                        msg.rec.field_4  = 1;
                        msg.rec.field_8  = 0;
                        msg.rec.field_C  = 0;
                        msg.rec.field_10 = 0;
                        Gp_DispatchMsg((Task*)cur->owner, 0x3F4, (s32)&msg.rec, 0);
                    }
                    Gp_DispatchMsg((Task*)work->owner, 0x3E9, (s32)&D_dryfield_warehouse_8017F850, 0);
                    work->field_8 = 0;
                    work->field_6++;
                    break;
                case 1:
                    if ((work->field_E % 60) == 0) {
                        SndEvt_EnqueueType6(0x52070003, 0, 0);
                    }
                    break;
            }
            work->field_E++;
            return;
        case 2:
            D_dryfield_warehouse_801821C0 = Task_SpawnFromTable(&D_dryfield_warehouse_8017FB08, 1, 8, 0);
            break;
        case 3:
            shared = (DwhWork*)D_dryfield_warehouse_801821BC->work;
            if (shared->playerEffActive != 0) {
                Gp_SpawnWeaponEff();
                shared->playerEffActive = 0;
                Gp_MsgPlayerWeapon(0);
            }
            weaponId         = D_80073BA9;
            anim             = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
            msg.rec.field_0  = anim;
            msg.rec.field_4  = 1;
            msg.rec.field_8  = 0;
            msg.rec.field_C  = 0;
            msg.rec.field_10 = 1;
            Gp_DispatchMsg((Task*)shared->owner, 0x3E8, (s32)&msg.rec, 0);
            Gp_DispatchMsg((Task*)shared->owner, 0x3E9, (s32)&D_dryfield_warehouse_8017F868, 0);
            switch (work->field_6) {
                case 0:
                    Task_SpawnFromTable(&D_dryfield_warehouse_8017FB08, 2, 8, 0);
                    work->field_8 = 0;
                    work->field_6++;
                    return;
                case 1:
                    work->field_8++;
                    shared     = (DwhWork*)arg0->work;
                    msg.pos.vx = 0x1644;
                    msg.pos.vy = 0;
                    if (!(shared->field_8 & 7)) {
                        msg.pos.vz = -500;
                        Gp_SpawnEff(0x60054, NULL, 0x80002300, &msg.pos);
                    }
                    if (!((shared->field_8 + 1) & 7)) {
                        msg.pos.vz = -700;
                        Gp_SpawnEff(0x60054, NULL, 0x80002300, &msg.pos);
                    }
                    if (!((shared->field_8 + 2) & 7)) {
                        msg.pos.vz = -900;
                        Gp_SpawnEff(0x60054, NULL, 0x80002300, &msg.pos);
                    }
                    if (!((shared->field_8 + 3) & 7)) {
                        msg.pos.vz = -1100;
                        Gp_SpawnEff(0x60054, NULL, 0x80002300, &msg.pos);
                    }
                    if (!((shared->field_8 + 4) & 7)) {
                        msg.pos.vz = -1300;
                        Gp_SpawnEff(0x60054, NULL, 0x80002300, &msg.pos);
                    }
                    if (work->field_8 >= 36) {
                        work->field_4 = 0;
                    }
                    SetDispMask(1);
                    return;
            }
            break;
        case 4:
            Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
            switch (work->field_6) {
                case 0:
                    D_8007216D                 = 2;
                    gGameSession->at4.loc.room = 2;
                    work->field_8              = 0;
                    work->field_6++;
                    break;
                case 1:
                    gGameSession->viewDirty     = 1;
                    gGameSession->roomObjsDirty = 1;
                    work->field_6++;
                    break;
                case 2:
                    break;
            }
            if (work->field_8 == 10) {
                SndEvt_EnqueueType6(0x52070004, 0, 0);
            }
            work->field_8++;
            return;
        case 5:
            Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
            switch (work->field_6) {
                case 0:
                    D_80115768    = 0;
                    work->field_E = 0;
                    work->field_6++;
                    break;
                case 1:
                    if ((work->field_E % 60) == 0) {
                        SndEvt_EnqueueType6(0x52070003, 0, 0);
                    }
                    break;
            }
            work->field_E++;
            return;
    }
    work->field_4 = 0;
}

/// Main loop of the warehouse's cutscene task, the owner of the 0x10-byte
/// `DwhWork` block. State 0 arms the script once: a `D_80114C12` of 1 or a live
/// `D_80071075` both mean the cutscene is already up, so it does nothing.
/// Otherwise it parks the zeroed work block in `Task::work` -- a failed
/// `Mem_Malloc` kills the task, but the record below is dispatched either way --
/// fills `owner` from pointer slot 3 and republishes this task as
/// `D_dryfield_warehouse_801821BC` so the room's script helpers reach that block.
///
/// The 0x3E8 record is rebuilt here rather than taken from its owner: `GpRec14`
/// field 0 is the equipped weapon's animation id, `D_80073BA9` plus 1 or 0x22
/// depending on `D_8007218A`, and 1 and 0 pad it out. It is dispatched to a
/// freshly fetched slot 3, not to the work block's owner.
///
/// State 0 then falls into state 1, which only steps the machine, so a task
/// entering at 1 runs the step alone. State 2 kills the task once the session
/// has torn down (`gGameSession->eventState`), otherwise runs the script.
void func_dryfield_warehouse_8017E090(Task* arg0)
{
    DwhWork* work;
    GpRec14  rec;
    s32      weaponId;
    s32      anim;

    switch (arg0->state) {
        case 0:
            if ((D_80114C12 != 1) && (D_80071075 == 0)) {
                work       = Mem_Malloc(0x10, false);
                arg0->work = (TaskIdMap*)work;
                if (work == NULL) {
                    taskKill(arg0);
                } else {
                    Mem_Set(work, 0, 0x10);
                    work->owner                   = gameGetPtrSlot(3);
                    D_dryfield_warehouse_801821BC = arg0;
                }
                weaponId     = D_80073BA9;
                anim         = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
                rec.field_0  = anim;
                rec.field_4  = 1;
                rec.field_8  = 0;
                rec.field_C  = 0;
                rec.field_10 = 0;
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&rec, 0);
                D_dryfield_warehouse_801821C0 = NULL;
                D_80115768                    = 1;
                arg0->state                   = arg0->state + 1;
                case 1:
                    func_800E8634((s32)&D_dryfield_warehouse_8017F880, 0,
                                  (s32)&D_dryfield_warehouse_8017FA00);
                    arg0->state = arg0->state + 1;
                    return;
            }
            return;
        case 2:
            if (gGameSession->eventState == 0) {
                Task_RequestKill(arg0, 0);
                return;
            }
            func_dryfield_warehouse_8017DBB0(arg0);
            break;
    }
}
