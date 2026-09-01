#include "common.h"

#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/task.h"

s32 func_800E3FCC(s32 arg0);

/// Task table spawned by `func_shelter_b2_breeding_room_8017D6A4` once the
/// breeding-room script has run.
extern TaskDesc D_shelter_b2_breeding_room_80180444[];

/// Handler for msg `0x16`: the first entry into the breeding room. Runs the
/// scripted scene once, then replays cap script `0x16` on later visits.
s32 func_shelter_b2_breeding_room_8017D6A4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 0x16) {
        if (GameFlag_GetNibble(0x120) != 0) {
            Gp_RunCapCmd1(0x16);
        } else {
            GameFlag_SetNibble(0x120, 1);
            if (func_800E3FCC(0xA2) == 0x1E) {
                func_800E3FAC(0xA2, 0x1F);
            }
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x140, 0x100);
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(1);
            Task_SpawnFromTable(D_shelter_b2_breeding_room_80180444, 0, 0, 0);
        }
    }
    return 0;
}

s32 func_shelter_b2_breeding_room_8017D750(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_breeding_room/shelter_b2_breeding_room_2", func_shelter_b2_breeding_room_8017D758);

void func_shelter_b2_breeding_room_8017D7A8(Task* arg0)
{
    if (Gp_CapBusy() == 0) {
        Gp_ResetCap();
        Gp_MsgPlayerWeapon(1);
        Task_Kill(arg0);
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_breeding_room/shelter_b2_breeding_room_2", func_shelter_b2_breeding_room_8017D7EC);

void func_shelter_b2_breeding_room_8017D838(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_breeding_room/shelter_b2_breeding_room_2", func_shelter_b2_breeding_room_8017D840);

INCLUDE_ASM("rooms/nonmatchings/shelter_b2_breeding_room/shelter_b2_breeding_room_2", func_shelter_b2_breeding_room_8017D898);
