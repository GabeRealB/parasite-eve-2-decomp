#include "common.h"

#include "actors/actor_123200.h"

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_8013215C);

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_801324A4);

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_801329F0);

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_80132B94);

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_801332E0);

s32 func_actor_123200_80133450(Actor123200Work* arg0)
{
    u16 id;
    s32 v;

    switch (arg0->field_174) {
        case 2:
            id = arg0->field_4A & 0x3FF;
            v  = id;
            if (v != 0x15) {
                goto not15;
            }
        check:
            if (arg0->field_220 == v) {
                goto same;
            }
            arg0->field_220 = id;
            return 0x400C0001;
        not15:
            if (v == 0x11) {
                goto check;
            }
        clear:
            arg0->field_220 = 0;
            break;
        case 3:
            id = arg0->field_4A & 0x3FF;
            v  = id;
            if (v != 0xD && v != 0x12) {
                goto clear;
            }
            goto check;
        same:
            arg0->field_220 = id;
            break;
        case 5:
            if (arg0->field_58 & 2) {
                return 0x400C0005;
            }
            break;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_8013352C);

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_80133820);

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_801339F0);

INCLUDE_RODATA("actors/nonmatchings/actor_123200/actor_123200", D_actor_123200_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_80133BA0);

INCLUDE_RODATA("actors/nonmatchings/actor_123200/actor_123200", ActorsShared80135df4Table);

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_80133E30);

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_80133EDC);
