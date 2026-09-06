#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/task.h"

/// Cutscene script blob argument of `func_800E8614`.
extern void     func_800E8614(s32 arg0, s32 arg1);
extern TaskDesc D_dryfield_night_saloon_g_r_8017F940[];
extern s32      D_dryfield_night_saloon_g_r_801848DC;
extern s32      D_dryfield_night_saloon_g_r_80184B34;
extern s32      D_dryfield_night_saloon_g_r_80184D2C;

s32 func_dryfield_night_saloon_g_r_8017DD84(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 4:
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(D_dryfield_night_saloon_g_r_8017F940, 0, 0, 0);
            break;
        case 8:
            if (GameFlag_GetNibble(0x5A) == 0) {
                func_800E8614((s32)&D_dryfield_night_saloon_g_r_801848DC, 0);
                GameFlag_SetNibble(0x5A, 1);
            } else if (GameFlag_GetNibble(0x5A) == 1) {
                func_800E8614((s32)&D_dryfield_night_saloon_g_r_80184B34, 0);
            }
            break;
        case 10:
            if (GameFlag_GetNibble(0x5A) < 2) {
                func_800E8614((s32)&D_dryfield_night_saloon_g_r_80184D2C, 0);
            }
            break;
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_saloon_g_r/dryfield_night_saloon_g_r_2", func_dryfield_night_saloon_g_r_8017DE68);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_saloon_g_r/dryfield_night_saloon_g_r_2", func_dryfield_night_saloon_g_r_8017DF90);
