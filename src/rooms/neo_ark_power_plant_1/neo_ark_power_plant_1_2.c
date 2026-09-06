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

s32 func_neo_ark_power_plant_1_8017D7F8(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    s32 cmd;

    switch (arg2) {
        case 2:
            if (GameFlag_GetNibble(0xDE) == 0) {
                cmd = 2;
            } else {
                cmd = 5;
            }
            Gp_RunCapCmd1(cmd);
            break;
        case 3:
            if (Gp_StateF0.field_0 == 2) {
                cmd = 7;
            } else if (GameFlag_GetNibble(0x148) != 0) {
                cmd = 6;
            } else {
                cmd = 3;
            }
            Gp_RunCapCmd1(cmd);
            break;
        case 9:
            if (GameFlag_GetNibble(0xDF) == 0) {
                cmd = 9;
            } else {
                cmd = 0xB;
            }
            Gp_RunCapCmd1(cmd);
            break;
        case 12:
            if (GameFlag_GetNibble(0xDF) != 0) {
                cmd = 0xA;
            } else {
                cmd = 0xC;
            }
            Gp_RunCapCmd1(cmd);
            break;
    }
    return 0;
}

s32 func_neo_ark_power_plant_1_8017D8C8(void)
{
    return 0;
}
