#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"

extern s32 D_actor_260500_8014CBF8;
extern s32 D_actor_260500_8014D630;
extern s32 D_actor_260500_8014D7C8;
extern s32 D_actor_260500_8014D948;
extern s32 D_actor_260500_8014DAB0;
extern s32 D_actor_260500_8014DCC0;

void func_actor_260500_80149EBC(void)
{
    switch (GameFlag_GetNibble(0xE3)) {
        case 0:
            func_800E8634((s32)&D_actor_260500_8014CBF8, 0, (s32)&D_actor_260500_8014D630);
            GameFlag_SetNibble(0xE3, 1);
            break;
        case 1:
            func_800E8614((s32)&D_actor_260500_8014D7C8, 0);
            GameFlag_SetNibble(0xE3, 2);
            break;
        case 2:
            func_800E8614((s32)&D_actor_260500_8014D948, 0);
            GameFlag_SetNibble(0xE3, 3);
            break;
        case 3:
            func_800E8614((s32)&D_actor_260500_8014DAB0, 0);
            GameFlag_SetNibble(0xE3, 4);
            break;
        case 4:
            func_800E8614((s32)&D_actor_260500_8014DCC0, 0);
            break;
    }
}
