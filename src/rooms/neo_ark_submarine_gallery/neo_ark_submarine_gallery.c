#include "common.h"
#include "main/display.h"
#include "gameplay/gameplay.h"
#include "gameplay/D4.h"
#include "gameplay/3FB8.h"
#include "main/task.h"

extern GpMsgEntry D_neo_ark_submarine_gallery_80181884[];
extern TaskDesc   D_neo_ark_submarine_gallery_801818BC[];

extern s32 func_neo_ark_submarine_gallery_8017EC24(u16 arg0, s32 arg1);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_submarine_gallery/neo_ark_submarine_gallery", D_neo_ark_submarine_gallery_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_submarine_gallery/neo_ark_submarine_gallery", func_neo_ark_submarine_gallery_8017D678);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_submarine_gallery/neo_ark_submarine_gallery", func_neo_ark_submarine_gallery_8017E2CC);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_submarine_gallery/neo_ark_submarine_gallery", D_neo_ark_submarine_gallery_8017D614);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_submarine_gallery/neo_ark_submarine_gallery", func_neo_ark_submarine_gallery_8017E86C);

s32 func_neo_ark_submarine_gallery_8017EA04(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_submarine_gallery/neo_ark_submarine_gallery", func_neo_ark_submarine_gallery_8017EA0C);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_submarine_gallery/neo_ark_submarine_gallery", func_neo_ark_submarine_gallery_8017EABC);

s32 func_neo_ark_submarine_gallery_8017EB48(void)
{
    return 0;
}

void func_neo_ark_submarine_gallery_8017EB50(Task* arg0)
{
    arg0->field_24 = D_neo_ark_submarine_gallery_80181884;
    Game_SetPtrSlot(arg0, 7);
    if (Game_Session->field_9 == 4) {
        Task_SpawnFromTable(D_neo_ark_submarine_gallery_801818BC, 0, 0, 0);
    }
    arg0->state = (s32)(arg0->state + 1);
}

void func_neo_ark_submarine_gallery_8017EBC4(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_submarine_gallery/neo_ark_submarine_gallery", func_neo_ark_submarine_gallery_8017EBCC);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_submarine_gallery/neo_ark_submarine_gallery", func_neo_ark_submarine_gallery_8017EC24);

void func_neo_ark_submarine_gallery_8017EED8(Task* arg0)
{
    if (Game_Session->field_9 != 4) {
        arg0->killCountdown = 0;
    } else {
        arg0->killCountdown = 0x780;
    }
    arg0->state = (s32)(arg0->state + 1);
}

void func_neo_ark_submarine_gallery_8017EF14(Task* arg0)
{
    s32 mode;
    if (Gp_ActorSlots[0] != NULL) {
        mode = 4;
        if (Game_Session->field_9 != mode && Game_Session->field_126 != 0) {
            Game_Session->field_9 = mode;
        }
        if (arg0->killCountdown < 0x780) {
            arg0->killCountdown = (s16)((u16)arg0->killCountdown + 0x10);
        }
        func_neo_ark_submarine_gallery_8017EC24((u16)arg0->killCountdown, mode);
    }
}

void func_neo_ark_submarine_gallery_8017EF8C(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_submarine_gallery/neo_ark_submarine_gallery", func_neo_ark_submarine_gallery_8017EF94);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_submarine_gallery/neo_ark_submarine_gallery", func_neo_ark_submarine_gallery_8017EFEC);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_submarine_gallery/neo_ark_submarine_gallery", func_neo_ark_submarine_gallery_8017F288);
