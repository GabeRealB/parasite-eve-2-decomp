#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include <psyq/libgs.h>

extern s8  D_8007106B;
extern s32 D_8011572C;
extern s32 D_80115750;
extern s32 D_80115758;

extern GpMsgEntry D_acropolis_cafeteria_80184CEC[];

void            Room_Draw02(GsCOORDINATE2* coord, s32 arg1, s32 arg2, u8* rgb);
void            Room_Draw04(GsCOORDINATE2* coord, s32 arg1, u8* rgb);
void            Room_DrawBillboard(GsCOORDINATE2* coord, s32 arg1, u8* rgb);
extern TaskDesc D_acropolis_cafeteria_80184178;
extern s32      D_acropolis_cafeteria_80184CFC;

void func_acropolis_cafeteria_8017E6B8(Task* arg0)
{
    Display_SpawnWithOt(&D_acropolis_cafeteria_80184178, 2, 0, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    Task_Kill(arg0);
}

void func_acropolis_cafeteria_8017E708(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    SVECTOR*       vec;

    work  = (GpEffWork*)task->spawnArg2;
    coord = ((TmdObject*)task->extra)->field_8;
    if (task->state != 0) {
        return;
    }
    task->field_24 = D_acropolis_cafeteria_80184CEC;
    Game_SetPtrSlot(task, 5);
    vec                            = (SVECTOR*)&work->field_10;
    D_acropolis_cafeteria_80184CFC = 0;
    work->field_10                 = 0x220;
    work->field_12                 = -0x12C;
    work->field_14                 = -0x6A0;
    Gp_SpawnEff(0x60064, coord, 0, vec);
    work->field_10 = 0x400;
    work->field_12 = -0x12C;
    work->field_14 = -0x260;
    Gp_SpawnEff(0x60064, coord, 0, vec);
    work->field_10 = 0x370;
    work->field_12 = -0x12C;
    work->field_14 = -0x860;
    Gp_SpawnEff(0x60064, coord, 0, vec);
    task->state    = task->state + 1;
    work->field_10 = 0xBB8;
    work->field_12 = -0x834;
    work->field_14 = -0x7D0;
    Gp_SpawnEff(0x60064, coord, 1, vec);
    work->field_10 = 0xB22;
    work->field_12 = -0x834;
    work->field_14 = -0x900;
    Gp_SpawnEff(0x60064, coord, 1, vec);
    D_80115758 = 0x6028D;
    D_8011572C = 0x6028E;
    D_80115750 = 0x6028F;
}
INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_6", func_acropolis_cafeteria_8017E89C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_6", func_acropolis_cafeteria_8017EA90);

INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_6", func_acropolis_cafeteria_8017F390);

s32 func_acropolis_cafeteria_8017F908(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    GsCOORDINATE2* coord;

    coord                          = ((TmdObject*)task->extra)->field_8;
    D_acropolis_cafeteria_80184CFC = arg2;
    if (arg2 != 0) {
        Gp_SpawnEff(0x6009D, coord, 0, NULL);
    }
    return 0;
}
void func_acropolis_cafeteria_8017F948(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    u8             rgb[3];

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->field_8;
    if (Gp_State1C->field_4 != 0) {
        if (Gp_State1C->field_4 >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->field_22++;
        switch (task->state) {
            case 0:
                work->field_24 = 0;
                work->field_26 = 0x80;
                work->field_2A = 0x100 / task->spawnArg1;
                task->state    = 1;
                break;
            case 1:
                work->field_24 += work->field_2A;
                work->field_26 += work->field_2A;
                task->spawnArg1--;
                rgb[0] = work->field_24;
                rgb[1] = (u16)work->field_24 >> 2;
                rgb[2] = (u16)work->field_24 >> 1;
                Room_Draw04(coord, (s16)work->field_26, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                Room_Draw04(coord, (s16)((u16)work->field_26 * 2), rgb);
                Room_Draw02(coord, (s16)(0x300 - (u16)work->field_26 * 2), 0x80, rgb);
                if (task->spawnArg1 == 0) {
                    work->field_24 = 0xFF;
                    task->state    = 2;
                    rgb[0]         = work->field_24;
                    rgb[1]         = (u16)work->field_24 >> 2;
                    rgb[2]         = (u16)work->field_24 >> 1;
                    Gp_DrawFadeQuad(rgb, 1);
                }
                break;
            case 2:
                if ((s16)work->field_24 >= 0x11) {
                    rgb[0] = work->field_24;
                    rgb[1] = (u16)work->field_24 >> 2;
                    rgb[2] = (u16)work->field_24 >> 1;
                    Room_DrawBillboard(coord, (s16)((s16)work->field_26 * 3), rgb);
                    work->field_24 -= 0x10;
                    work->field_26 -= 8;
                    break;
                }
                /* fallthrough */
            case 3:
                Gp_ReleaseState1CMem(work, task);
                break;
        }
    }
}
INCLUDE_RODATA("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_6", D_acropolis_cafeteria_8017D69C);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_6", D_acropolis_cafeteria_8017D6AC);
