#include "common.h"
#include "main/display.h"
#include "gameplay/gameplay.h"

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

INCLUDE_ASM("rooms/nonmatchings/neo_ark_submarine_gallery/neo_ark_submarine_gallery", func_neo_ark_submarine_gallery_8017EB50);

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

INCLUDE_ASM("rooms/nonmatchings/neo_ark_submarine_gallery/neo_ark_submarine_gallery", func_neo_ark_submarine_gallery_8017EF14);

void func_neo_ark_submarine_gallery_8017EF8C(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_submarine_gallery/neo_ark_submarine_gallery", func_neo_ark_submarine_gallery_8017EF94);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_submarine_gallery/neo_ark_submarine_gallery", func_neo_ark_submarine_gallery_8017EFEC);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_submarine_gallery/neo_ark_submarine_gallery", func_neo_ark_submarine_gallery_8017F288);
