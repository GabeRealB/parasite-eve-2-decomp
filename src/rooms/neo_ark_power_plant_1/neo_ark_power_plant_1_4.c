#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/task.h"

extern s32 Gp_LcgState;

/// Steam / spark emitter positions the power plant's view-specific ambient
/// effects are played at, indexed by `func_neo_ark_power_plant_1_8017DA18`.
extern SVECTOR D_neo_ark_power_plant_1_8017F020[52];
extern SVECTOR D_neo_ark_power_plant_1_8017F1C0;

void Room_Draw13(SVECTOR* pos, s32 arg1, s32 arg2);

void func_neo_ark_power_plant_1_8017DA18(void)
{
    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[30], 0x200, 0x344);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[32], 0x200, 0x344);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[35], 0x200, 0x344);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[36], 0x200, 0x344);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[37], 0x200, 0x344);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[38], 0x200, 0x344);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[39], 0x200, 0x344);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[40], 0x200, 0x344);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[41], 0x200, 0x344);
            break;
        case 3:
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[51], 0x300, 0x334);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[26], 0x200, 0x344);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[27], 0x200, 0x344);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[28], 0x200, 0x344);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[29], 0x200, 0x344);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[30], 0x200, 0x233);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[31], 0x200, 0x233);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[32], 0x200, 0x233);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[33], 0x200, 0x233);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[34], 0x200, 0x122);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[35], 0x200, 0x122);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[36], 0x200, 0x122);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[37], 0x200, 0x122);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[39], 0x200, 0x122);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[41], 0x200, 0x122);
            break;
        case 4:
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[42], 0x300, 0x334);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[45], 0x300, 0x334);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[46], 0x300, 0x334);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[48], 0x300, 0x334);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[49], 0x300, 0x334);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[16], 0x200, 0x122);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[17], 0x200, 0x122);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[18], 0x200, 0x122);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[19], 0x200, 0x122);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[20], 0x200, 0x122);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[21], 0x200, 0x122);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[22], 0x200, 0x122);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[23], 0x200, 0x122);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[24], 0x200, 0x233);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[25], 0x200, 0x233);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[26], 0x200, 0x233);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[27], 0x200, 0x233);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[28], 0x200, 0x344);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[29], 0x200, 0x344);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[30], 0x200, 0x344);
            break;
        case 5:
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[48], 0x300, 0x334);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[49], 0x300, 0x334);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[12], 0x200, 0x344);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[13], 0x200, 0x344);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[14], 0x200, 0x344);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[15], 0x200, 0x344);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[16], 0x200, 0x233);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[17], 0x200, 0x233);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[18], 0x200, 0x233);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[19], 0x200, 0x233);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[20], 0x200, 0x122);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[21], 0x200, 0x122);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[22], 0x200, 0x122);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[23], 0x200, 0x122);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[24], 0x200, 0x122);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[25], 0x200, 0x122);
            break;
        case 6:
            if (GameFlag_GetNibble(0x148) != 0) {
                if (Gp_State1C->field_4 == 0 && GameFlag_GetNibble(0xDE) == 0) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if ((((u32)Gp_LcgState >> 16) & 7) == 0) {
                        Gp_SpawnEff(0x600E0, NULL, 0x400, &D_neo_ark_power_plant_1_8017F1C0);
                    }
                }
            } else {
                Room_Draw13(&D_neo_ark_power_plant_1_8017F1C0, 0x300, 0x334);
            }
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[42], 0x300, 0x334);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[43], 0x300, 0x334);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[0], 0x200, 0x11);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[1], 0x200, 0x11);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[2], 0x200, 0x11);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[3], 0x200, 0x11);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[4], 0x200, 0x122);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[5], 0x200, 0x122);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[6], 0x200, 0x122);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[7], 0x200, 0x122);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[8], 0x200, 0x233);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[9], 0x200, 0x233);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[10], 0x200, 0x233);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[11], 0x200, 0x233);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[12], 0x200, 0x344);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[13], 0x200, 0x344);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[14], 0x200, 0x344);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[15], 0x200, 0x344);
            break;
        case 7:
            if (GameFlag_GetNibble(0x148) != 0) {
                if (Gp_State1C->field_4 == 0 && GameFlag_GetNibble(0xDE) == 0) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if ((((u32)Gp_LcgState >> 16) & 7) == 0) {
                        Gp_SpawnEff(0x600E0, NULL, 0x400, &D_neo_ark_power_plant_1_8017F1C0);
                    }
                }
            } else {
                Room_Draw13(&D_neo_ark_power_plant_1_8017F1C0, 0x300, 0x334);
            }
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[0], 0x200, 0x344);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[1], 0x200, 0x344);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[2], 0x200, 0x344);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[3], 0x200, 0x344);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[4], 0x200, 0x344);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[5], 0x200, 0x344);
            break;
        case 8:
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[42], 0x300, 0x223);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[43], 0x300, 0x223);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[44], 0x300, 0x223);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[45], 0x300, 0x334);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[46], 0x300, 0x334);
            Room_Draw13(&D_neo_ark_power_plant_1_8017F020[47], 0x300, 0x334);
            break;
    }
}
