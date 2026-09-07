#include "common.h"
#include "actors/actor_403200.h"
extern s32 D_actor_403200_80141C54;

extern s16 D_actor_403200_80141C5A;

extern Task* D_actor_403200_8015F8F0;

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_2", func_actor_403200_80141A94);

s32 func_actor_403200_80141B30(void)
{
    D_actor_403200_80141C54 = 0;
    return 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_2", func_actor_403200_80141B40);
