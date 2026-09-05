#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/rooms_shared_801802cc.h"

#include <psyq/libgs.h>

extern s32 Gp_LcgState;

void Room_Draw02(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* arg3);

void RoomsShared801802cc(Task* task)
{
    GsCOORDINATE2* objCoord;
    GpEffWork*     work;
    u8             rgb[4];

    objCoord = ((TmdObject*)task->extra)->field_8;
    work     = (GpEffWork*)task->spawnArg2;

    if (Gp_State1C->field_4 != 0) {
        if (Gp_State1C->field_4 >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }

    Gp_UpdateCoord(objCoord);
    work->field_22++;

    switch (task->state) {
        case 0:
            Gp_SpawnEff(0x60076, objCoord, 0x400, NULL);
            if (task->spawnArg1 != 0) {
                Gp_SpawnEff(0x60070, objCoord, 0x80004600, NULL);
                task->state = 1;
            } else {
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                work->field_24 = 0x100;
                work->field_26 = 0xC0;
                task->state    = 2;
            }
            break;

        case 1:
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_10 = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_12 = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_14 = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x60070, objCoord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x82003400,
                        (SVECTOR*)&work->field_10);
            if (work->field_22 >= 7) {
                task->state = 3;
            }
            break;

        case 2:
            work->field_26 -= 0x20;
            work->field_24 += 0x30;
            rgb[0]          = work->field_26;
            rgb[1]          = (u16)work->field_26 >> 1;
            rgb[2]          = (u16)work->field_26 >> 2;
            Room_Draw02(objCoord, 0x100, 0x100, rgb);
            Room_Draw02(objCoord, work->field_24, work->field_24, rgb);
            if (work->field_22 >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}