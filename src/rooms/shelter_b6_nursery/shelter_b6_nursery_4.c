#include "common.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"
extern TaskDesc       D_shelter_b6_nursery_80185000;
extern s32            D_shelter_b6_nursery_8018797C;
extern TaskFuncTable3 RoomsShared8017d878Table;

void func_shelter_b6_nursery_8017FFF4(void)
{
    if (D_shelter_b6_nursery_8018797C == 0) {
        D_shelter_b6_nursery_8018797C = 1;
        Task_SpawnFromTable(&D_shelter_b6_nursery_80185000, 0, 0, 0);
    }
}

/// Sets the second sprite command's skip-link flag in view 13 for the current
/// room in the first stage table. Only low-byte values 0 and 1 change the flag.
void func_shelter_b6_nursery_80180038(s32 arg0)
{
    GameSessionFrom4* sess = (GameSessionFrom4*)&Game_Session->field_4;
    GpSprtCmd*        cmd;
    s32               mode;

    cmd  = Gp_SprtTables[sess->field_3 - 1][0].field_0[sess->field_2 - 1][12].field_4;
    mode = arg0 & 0xFF;
    if (mode == 0) {
        cmd[1].field_4 = 0;
    } else if (mode == 1) {
        cmd[1].field_4 = 1;
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_4", func_shelter_b6_nursery_801800A0);
