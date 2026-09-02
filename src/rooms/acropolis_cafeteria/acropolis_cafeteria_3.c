#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/task.h"
#include "main/tmd.h"

#include <psyq/libgs.h>

extern s32 D_acropolis_cafeteria_8018D6A0;
extern s32 D_acropolis_cafeteria_8018D6A4;
extern s32 D_acropolis_cafeteria_8018D6A8;
extern s32 D_acropolis_cafeteria_80182D28;
extern s32 D_acropolis_cafeteria_80182DB8;

void func_acropolis_cafeteria_8017DF68(Task* task)
{
    GsCOORDINATE2* coord;

    coord = ((TmdObject*)((Task*)Gp_LookupSlot4(0))->extra)->field_8;
    switch (task->state) {
        case 0:
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D4, (s32)&D_acropolis_cafeteria_80182D28, 0);
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7DB, (s32)&D_acropolis_cafeteria_80182DB8, 0);
            D_acropolis_cafeteria_8018D6A0 = 0;
            D_acropolis_cafeteria_8018D6A4 = -0x14;
            D_acropolis_cafeteria_8018D6A8 = -0x14;
            task->state                    = task->state + 1;
            break;

        case 1:
            coord->coord.t[0] += D_acropolis_cafeteria_8018D6A0;
            coord->coord.t[1] += D_acropolis_cafeteria_8018D6A4;
            if (coord->coord.t[1] > -0x12C) {
                coord->coord.t[1] = -0x12C;
            }
            coord->coord.t[2]              += D_acropolis_cafeteria_8018D6A8;
            coord->flg                      = 0;
            D_acropolis_cafeteria_8018D6A0  = D_acropolis_cafeteria_8018D6A0 / 2;
            D_acropolis_cafeteria_8018D6A4 += 5;
            if (D_acropolis_cafeteria_8018D6A4 > 0x14) {
                D_acropolis_cafeteria_8018D6A4 = 0x14;
            }
            D_acropolis_cafeteria_8018D6A8 = D_acropolis_cafeteria_8018D6A8 / 2;
            if (D_acropolis_cafeteria_8018D6A0 == 0 && D_acropolis_cafeteria_8018D6A8 == 0 &&
                coord->coord.t[1] == -0x12C) {
                Task_Kill(task);
            }
            break;
    }
}