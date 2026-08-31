#include "common.h"

#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

extern s32 D_80139964;
extern s32 D_8013A33C;
extern s32 D_8013A84C;
extern s32 D_8013A8DC;
extern s32 D_shelter_b6_nursery_8018500C;

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_6", func_shelter_b6_nursery_8017FDD4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_6", func_shelter_b6_nursery_8017FE3C);

void func_shelter_b6_nursery_8017FEC4(Task* arg0)
{
    arg0->field_24 = &D_shelter_b6_nursery_8018500C;
    Game_SetPtrSlot(arg0, 7);
    Gp_FillAllyHp();
    if (GameFlag_GetNibble(0xC7) == 0) {
        GameFlag_SetNibble(0xC7, 1);
        func_800E8634((s32)&D_80139964, 0, (s32)&D_8013A33C);
        GameFlag_SetNibble(3, 0);
        func_800E3FAC(0xA2, 0x30);
    } else if (GameFlag_GetNibble(0xC7) == 1) {
        func_800E8614((s32)&D_8013A84C, 1);
    } else {
        func_800E8614((s32)&D_8013A8DC, 1);
    }
    arg0->state++;
}
