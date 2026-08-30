#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/task.h"

extern s32 D_actor_146300_80137AAC;
extern s32 D_actor_146300_80137B38;
extern s32 D_actor_146300_80137B60;
extern s32 D_actor_146300_80137C10;

INCLUDE_RODATA("actors/nonmatchings/actor_146300/actor_146300", D_actor_146300_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_146300/actor_146300", func_actor_146300_80131ECC);

void func_actor_146300_8013224C(void)
{
    switch (GameFlag_GetNibble(0x7B)) {
        case 2:
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137B38, 0);
            break;
        case 3:
            if (Gp_HasCollectedBit(0x119) == 0) {
                if (Gp_GetCurBit2Flag(0x1F) == 1) {
                    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137AAC, 0);
                } else {
                    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137B38, 0);
                }
            } else {
                Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137B38, 0);
            }
            break;
        case 4:
            if (Gp_HasCollectedBit(0x119) == 0) {
                if (Gp_GetCurBit2Flag(0x20) == 1) {
                    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137AAC, 0);
                } else {
                    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137B38, 0);
                }
                break;
            }
            /* fallthrough */
        case 5:
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D4, (s32)&D_actor_146300_80137C10, 0);
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_146300_80137B60, 0);
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_146300/actor_146300", func_actor_146300_801323E0);

INCLUDE_ASM("actors/nonmatchings/actor_146300/actor_146300", func_actor_146300_80132418);

INCLUDE_ASM("actors/nonmatchings/actor_146300/actor_146300", func_actor_146300_801324AC);

INCLUDE_ASM("actors/nonmatchings/actor_146300/actor_146300", func_actor_146300_801326CC);

INCLUDE_ASM("actors/nonmatchings/actor_146300/actor_146300", func_actor_146300_80132728);

INCLUDE_ASM("actors/nonmatchings/actor_146300/actor_146300", func_actor_146300_801327A4);

INCLUDE_ASM("actors/nonmatchings/actor_146300/actor_146300", func_actor_146300_801327CC);

INCLUDE_ASM("actors/nonmatchings/actor_146300/actor_146300", func_actor_146300_80132840);

INCLUDE_ASM("actors/nonmatchings/actor_146300/actor_146300", func_actor_146300_8013288C);

INCLUDE_ASM("actors/nonmatchings/actor_146300/actor_146300", func_actor_146300_8013291C);

INCLUDE_ASM("actors/nonmatchings/actor_146300/actor_146300", func_actor_146300_8013299C);

INCLUDE_ASM("actors/nonmatchings/actor_146300/actor_146300", func_actor_146300_80132A2C);

INCLUDE_ASM("actors/nonmatchings/actor_146300/actor_146300", func_actor_146300_80132A98);

INCLUDE_ASM("actors/nonmatchings/actor_146300/actor_146300", func_actor_146300_80132B14);

INCLUDE_ASM("actors/nonmatchings/actor_146300/actor_146300", func_actor_146300_80132B1C);
