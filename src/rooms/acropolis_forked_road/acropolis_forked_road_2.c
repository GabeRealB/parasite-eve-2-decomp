#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"

#include "main/fs.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

extern s32 D_acropolis_forked_road_801820B8;

s32 func_acropolis_forked_road_8017D858(s32 arg0, s32 arg1, s32 arg2)
{
    s32 cmd;

    if (arg2 == 3) {
        if ((Gp_GetCurBit2Flag(0x18) == 0) || (Gp_GetCurBit2Flag(0x18) == 1)) {
            cmd = 3;
        } else {
            cmd = 4;
        }
        Gp_RunCapCmd1(cmd);
    }
    return 0;
}

s32 func_acropolis_forked_road_8017D8A8(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    u8 temp;

    if (arg2->field_2 == 1 && (GameFlag_GetNibble(9) & 2)) {
        temp = Game_Session->field_9;
        if (((temp == 4) || (temp == 8)) && (GameFlag_GetNibble(0xCC) == 0)) {
            func_800E8614((s32)&D_acropolis_forked_road_801820B8, 1);
            GameFlag_SetNibble(0xCC, 1);
        }
    }
    return 1;
}

INCLUDE_RODATA("rooms/nonmatchings/acropolis_forked_road/acropolis_forked_road_2", jtbl_acropolis_forked_road_8017D5D0);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_forked_road/acropolis_forked_road_2", D_acropolis_forked_road_8017D5E8);
