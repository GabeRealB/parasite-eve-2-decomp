#include "common.h"

#include "gameplay/3CD8.h"
#include "main/task.h"
#include "main/gameflag.h"

extern s32 D_actor_160700_80135664;
extern s32 D_actor_160700_80135ACC;
extern s32 D_actor_160700_80135BD4;
extern s32 D_actor_160700_801362F4;
extern s32 D_actor_160700_80136414;

INCLUDE_ASM("actors/nonmatchings/actor_160700/actor_160700", func_actor_160700_80131E24);

void func_actor_160700_80131E70(void)
{
    switch (GameFlag_GetNibble(0x113)) {
        case 0:
            func_800E8634((s32)&D_actor_160700_80135664, 0, (s32)&D_actor_160700_80135ACC);
            GameFlag_SetNibble(0x113, 1);
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 0xC);
            break;
        case 1:
            func_800E8634((s32)&D_actor_160700_80135BD4, 0, (s32)&D_actor_160700_80135ACC);
            GameFlag_SetNibble(0x113, 2);
            break;
        case 2:
            func_800E8614((s32)&D_actor_160700_801362F4, 0);
            GameFlag_SetNibble(0x113, 3);
            break;
        case 3:
            func_800E8614((s32)&D_actor_160700_80136414, 0);
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_160700/actor_160700", func_actor_160700_80131F70);

INCLUDE_ASM("actors/nonmatchings/actor_160700/actor_160700", func_actor_160700_80132184);

INCLUDE_ASM("actors/nonmatchings/actor_160700/actor_160700", func_actor_160700_8013233C);

INCLUDE_ASM("actors/nonmatchings/actor_160700/actor_160700", func_actor_160700_80132390);

INCLUDE_RODATA("actors/nonmatchings/actor_160700/actor_160700", D_actor_160700_80131E20);
