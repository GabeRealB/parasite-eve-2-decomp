#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"

#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/dryfield_warehouse.h"
#include "rooms/rooms_shared_80180b2c.h"

extern u8  D_80071075;
extern u8  D_80073BA9;
extern s8  D_8007218A;
extern s8  D_80114C12;
extern s32 D_dryfield_warehouse_8017F868;
extern s16 D_dryfield_warehouse_801821C4;
extern s32 D_dryfield_warehouse_8017F880;
extern s32 D_dryfield_warehouse_8017FA00;

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
                Task_Kill(D_dryfield_warehouse_801821C0);
            }
            SetDispMask(1);
            work = (DwhWork*)RoomsShared80180b2cTask->idMap;
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
            if (Mc_SaveData.field_5 != 2) {
                Mc_SaveData.field_5           = 2;
                Game_Session->field_5         = 2;
                D_dryfield_warehouse_801821C4 = 1;
                return;
            }
            D_dryfield_warehouse_801821C4 = 0;
            return;
        case 1:
            if (D_dryfield_warehouse_801821C4 != 0) {
                Game_Session->field_52 = arg0;
                Game_Session->field_76 = arg0;
            }
            return;
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_warehouse/dryfield_warehouse_2", func_dryfield_warehouse_8017DBB0);

/// Main loop of the warehouse's cutscene task, the owner of the 0x10-byte
/// `DwhWork` block. State 0 arms the script once: a `D_80114C12` of 1 or a live
/// `D_80071075` both mean the cutscene is already up, so it does nothing.
/// Otherwise it parks the zeroed work block in `Task::idMap` -- a failed
/// `Mem_Malloc` kills the task, but the record below is dispatched either way --
/// fills `owner` from pointer slot 3 and republishes this task as
/// `RoomsShared80180b2cTask` so the room's script helpers reach that block.
///
/// The 0x3E8 record is rebuilt here rather than taken from its owner: `GpRec14`
/// field 0 is the equipped weapon's animation id, `D_80073BA9` plus 1 or 0x22
/// depending on `D_8007218A`, and 1 and 0 pad it out. It is dispatched to a
/// freshly fetched slot 3, not to the work block's owner.
///
/// State 0 then falls into state 1, which only steps the machine, so a task
/// entering at 1 runs the step alone. State 2 kills the task once the session
/// has torn down (`Game_Session->field_1`), otherwise runs the script.
void func_dryfield_warehouse_8017E090(Task* arg0)
{
    DwhWork* work;
    GpRec14  rec;
    s32      weaponId;
    s32      anim;

    switch (arg0->state) {
        case 0:
            if ((D_80114C12 != 1) && (D_80071075 == 0)) {
                work        = Mem_Malloc(0x10, false);
                arg0->idMap = (TaskIdMap*)work;
                if (work == NULL) {
                    Task_Kill(arg0);
                } else {
                    Mem_Set(work, 0, 0x10);
                    work->owner             = Game_GetPtrSlot(3);
                    RoomsShared80180b2cTask = arg0;
                }
                weaponId     = D_80073BA9;
                anim         = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
                rec.field_0  = anim;
                rec.field_4  = 1;
                rec.field_8  = 0;
                rec.field_C  = 0;
                rec.field_10 = 0;
                Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E8, (s32)&rec, 0);
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
            if (Game_Session->field_1 == 0) {
                Task_RequestKill(arg0, 0);
                return;
            }
            func_dryfield_warehouse_8017DBB0(arg0);
            break;
    }
}
