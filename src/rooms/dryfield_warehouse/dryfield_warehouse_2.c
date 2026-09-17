#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"

#include "main/mc.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/dryfield_warehouse.h"
#include "rooms/rooms_shared_80180b2c.h"

extern u8  D_80073BA9;
extern s8  D_8007218A;
extern s32 D_dryfield_warehouse_8017F868;
extern s16 D_dryfield_warehouse_801821C4;

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

INCLUDE_ASM("rooms/nonmatchings/dryfield_warehouse/dryfield_warehouse_2", func_dryfield_warehouse_8017E090);
