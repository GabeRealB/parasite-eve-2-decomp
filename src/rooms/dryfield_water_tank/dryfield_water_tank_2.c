#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

extern TaskDesc       D_dryfield_water_tank_8017F34C;
extern s32            D_dryfield_water_tank_8017F114;
extern s32            D_dryfield_water_tank_8017F21C;
extern s32            D_dryfield_water_tank_80184E0C;
extern s32            D_dryfield_water_tank_801859DC;
extern GpAreaApplyRec D_dryfield_water_tank_80188D1C[];
extern GpMsgEntry     D_dryfield_water_tank_8017F324[];
extern TaskDesc       D_dryfield_water_tank_801868A4[];

void func_dryfield_water_tank_8017DB48(void);

s32 func_dryfield_water_tank_8017D7EC(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    if (arg2->field_2 == 1) {
        if (GameFlag_GetNibble(0x36) == 0) {
            GameFlag_SetNibble(0x36, 1);
            Task_SpawnFromTable(&D_dryfield_water_tank_8017F34C, 0, 0, 0);
            GameFlag_SetNibble(0x56, 1);
        }
    }
    if ((arg2->field_2 == 2) && (GameFlag_GetNibble(0x33) == 0)) {
        GameFlag_SetNibble(0x33, 1);
        func_800E3FAC(0xA2, 0xE);
        GameFlag_SetNibble(3, 0);
        GameFlag_SetNibble(0x155, 3);
        Gp_ApplyAreaRecs(D_dryfield_water_tank_80188D1C);
        Gp_MsgPlayerWeapon(0);
        func_800E8634((s32)&D_dryfield_water_tank_80184E0C, 0, (s32)&D_dryfield_water_tank_801859DC);
    }
    if (arg2->field_2 == 3) {
        func_800E8614((s32)&D_dryfield_water_tank_8017F114, 0);
    }
    if (arg2->field_2 == 4) {
        func_800E8614((s32)&D_dryfield_water_tank_8017F21C, 0);
    }
    return 1;
}

/// Room message handler: on message `0xE` spawn the second entry of
/// `D_dryfield_water_tank_8017F34C`, the same table `func_dryfield_water_tank_8017D7EC`
/// takes entry 0 from.
s32 func_dryfield_water_tank_8017D910(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0xE) {
        Task_SpawnFromTable(&D_dryfield_water_tank_8017F34C, 1, 0, 0);
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_2", func_dryfield_water_tank_8017D948);

/// State 0 of the room's event task: publish the message table the room's
/// handlers hang off (`0x13EE`–`0x13F1`), take pointer slot 7, spawn the
/// cutscene task from `D_dryfield_water_tank_801868A4`, queue sound event
/// `0x52150009`, run the game-flag `0x55` dispatch in
/// `func_dryfield_water_tank_8017DB48`, then advance.
void func_dryfield_water_tank_8017D9D4(Task* task)
{
    task->field_24 = D_dryfield_water_tank_8017F324;
    Game_SetPtrSlot(task, 7);
    Task_SpawnFromTable(D_dryfield_water_tank_801868A4, 0, 0, 0);
    SndEvt_EnqueueType6(0x52150009, 0, 0);
    func_dryfield_water_tank_8017DB48();
    task->state = (s32)(task->state + 1);
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_2", func_dryfield_water_tank_8017DA4C);
