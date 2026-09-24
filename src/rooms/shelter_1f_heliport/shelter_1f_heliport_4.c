#include "common.h"

#include "gameplay/3CD8.h"
#include "main/session.h"

extern u8 D_8007216C;
extern u8 D_shelter_1f_heliport_801811D4[][4];

void func_shelter_1f_heliport_801807C0(void)
{
    s32 i;
    s32 idx = D_8007216C;

    if (gGameSession->at4.loc.place < 3 && idx < 12) {
        if (D_shelter_1f_heliport_801811D4[idx][0] != 0) {
            for (i = 0; i < 4; i++) {
                Gp_MsgSlot4Chain(i, D_shelter_1f_heliport_801811D4[idx][i]);
            }
        }
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_heliport/shelter_1f_heliport_4", func_shelter_1f_heliport_8018085C);

void func_shelter_1f_heliport_80180B4C(void)
{
}
