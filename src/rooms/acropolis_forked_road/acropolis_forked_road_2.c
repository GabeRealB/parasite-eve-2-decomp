#include "common.h"
#include "main/fs.h"
#include "gameplay/gameplay.h"
extern s32      D_acropolis_forked_road_80180F3C;
extern TaskDesc D_acropolis_forked_road_80180F44;

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

INCLUDE_ASM("rooms/nonmatchings/acropolis_forked_road/acropolis_forked_road_2", func_acropolis_forked_road_8017D8A8);

INCLUDE_ASM("rooms/nonmatchings/acropolis_forked_road/acropolis_forked_road_2", func_acropolis_forked_road_8017D92C);

void func_acropolis_forked_road_8017D970(void)
{
    if ((D_acropolis_forked_road_80180F3C == 0) && (Game_Session->field_8 == 2)) {
        D_acropolis_forked_road_80180F3C = 1;
        Task_SpawnFromTable(&D_acropolis_forked_road_80180F44, 2, 0, 0);
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_forked_road/acropolis_forked_road_2", func_acropolis_forked_road_8017D9CC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_forked_road/acropolis_forked_road_2", func_acropolis_forked_road_8017DA24);

INCLUDE_ASM("rooms/nonmatchings/acropolis_forked_road/acropolis_forked_road_2", func_acropolis_forked_road_8017DD60);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_forked_road/acropolis_forked_road_2", D_acropolis_forked_road_8017D5E8);
