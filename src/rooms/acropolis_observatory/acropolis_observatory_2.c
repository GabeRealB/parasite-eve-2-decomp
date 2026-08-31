#include "common.h"
#include "main/fs.h"
#include "gameplay/gameplay.h"
extern s32      D_acropolis_observatory_8017E7D8;
extern TaskDesc D_acropolis_observatory_8017E7DC;

INCLUDE_ASM("rooms/nonmatchings/acropolis_observatory/acropolis_observatory_2", func_acropolis_observatory_8017D7C4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_observatory/acropolis_observatory_2", func_acropolis_observatory_8017D834);

void func_acropolis_observatory_8017D8AC(void)
{
    if ((D_acropolis_observatory_8017E7D8 == 0) && (Game_Session->field_8 == 3)) {
        D_acropolis_observatory_8017E7D8 = 1;
        Task_SpawnFromTable(&D_acropolis_observatory_8017E7DC, 1, 0, 0);
    }
    if ((D_acropolis_observatory_8017E7D8 == 0) && (Game_Session->field_8 == 4)) {
        D_acropolis_observatory_8017E7D8 = 1;
        Task_SpawnFromTable(&D_acropolis_observatory_8017E7DC, 0, 0, 0);
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_observatory/acropolis_observatory_2", func_acropolis_observatory_8017D950);

INCLUDE_ASM("rooms/nonmatchings/acropolis_observatory/acropolis_observatory_2", func_acropolis_observatory_8017D9A8);

INCLUDE_ASM("rooms/nonmatchings/acropolis_observatory/acropolis_observatory_2", func_acropolis_observatory_8017DD3C);
