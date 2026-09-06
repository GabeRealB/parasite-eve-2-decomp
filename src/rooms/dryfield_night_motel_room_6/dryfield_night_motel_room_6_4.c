#include "common.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

extern s16 D_80071076;

extern TaskDesc D_dryfield_night_motel_room_6_80182EE0;

extern GpAreaApplyRec D_dryfield_night_motel_room_6_80186270[];
extern GpAreaApplyRec D_dryfield_night_motel_room_6_801862B0[];

s32 func_dryfield_night_motel_room_6_80181A9C(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 6) {
        SOFT_TOUCH_REG(arg2);
        if (GameFlag_GetNibble(0x61) != 0) {
            Gp_RunCapCmd1(0x14);
        } else if (GameFlag_GetNibble(0x6C) <= 0) {
            Gp_RunCapCmd1(arg2);
        } else if (GameFlag_GetNibble(0x70) < 2) {
            Task_SpawnFromTable(&D_dryfield_night_motel_room_6_80182EE0, 0, 0x11, 0);
        } else {
            Gp_RunCapCmd1(arg2);
        }
    }
    if (arg2 == 0xD) {
        if (GameFlag_GetNibble(0x61) != 0) {
            Gp_RunCapCmd1(0x13);
        } else {
            Gp_RunCapCmd1(0xD);
        }
    }
    if (arg2 == 0xB) {
        if (GameFlag_GetNibble(0x61) != 0) {
            Gp_RunCapCmd1(0x15);
        } else {
            Gp_RunCapCmd1(0xB);
        }
    }
    return 0;
}

s32 func_dryfield_night_motel_room_6_80181B74(void)
{
    return 0;
}
