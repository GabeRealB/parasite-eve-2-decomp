#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

/// Cutscene script blob arguments of `func_800E8634`.
extern s32 D_dryfield_night_dilapidated_house_801868F4;
extern s32 D_dryfield_night_dilapidated_house_80187134;

/// The room task's message table: `{id, handler}` pairs terminated by
/// `0x7FFFFFFF`.
extern GpMsgEntry D_dryfield_night_dilapidated_house_8017E700[];

s32 func_dryfield_night_dilapidated_house_8017D8D4(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_dilapidated_house/dryfield_night_dilapidated_house", func_dryfield_night_dilapidated_house_8017D8DC);

s32 func_dryfield_night_dilapidated_house_8017D960(void)
{
    return 0;
}

s32 func_dryfield_night_dilapidated_house_8017D968(void)
{
    return 0;
}

void func_dryfield_night_dilapidated_house_8017D970(Task* arg0)
{
    arg0->field_24 = D_dryfield_night_dilapidated_house_8017E700;
    Game_SetPtrSlot(arg0, 7);
    arg0->state = (s32)(arg0->state + 1);
    if (GameFlag_GetNibble(0x92) == 0) {
        if (Game_GetPtrSlot(0xA) != 0) {
            func_800E8634((s32)&D_dryfield_night_dilapidated_house_801868F4, 0,
                          (s32)&D_dryfield_night_dilapidated_house_80187134);
        }
        GameFlag_SetNibble(0x92, 1);
        GameFlag_SetNibble(0x7A, 3);
        func_800E3FAC(0xA2, 0x11);
    }
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_dilapidated_house/dryfield_night_dilapidated_house", RoomsShared8017d878Table);
