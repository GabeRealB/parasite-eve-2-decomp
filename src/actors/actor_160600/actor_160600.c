#include "common.h"

void func_actor_160600_80131E24(void)
{
    if (Game_GetPtrSlot(0xA) != NULL) {
        Task_CallExit(Game_GetPtrSlot(0xA));
        Game_SetPtrSlot(NULL, 0xA);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_160600/actor_160600", func_actor_160600_80131E68);

INCLUDE_ASM("actors/nonmatchings/actor_160600/actor_160600", func_actor_160600_80131FFC);

INCLUDE_ASM("actors/nonmatchings/actor_160600/actor_160600", func_actor_160600_801321B4);

INCLUDE_ASM("actors/nonmatchings/actor_160600/actor_160600", func_actor_160600_80132208);

INCLUDE_RODATA("actors/nonmatchings/actor_160600/actor_160600", D_actor_160600_80131E20);
