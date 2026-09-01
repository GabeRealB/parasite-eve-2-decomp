#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/shelter_b6_nursery.h"

#include <psyq/libgpu.h>
#include <psyq/libgs.h>

extern s32  Gp_LcgState;
extern void Room_Draw02(GsCOORDINATE2* coord, s32 arg1, s32 arg2, u8* rgb);

void func_shelter_b6_nursery_80184074(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    u8             rgb[3];

    coord = ((TmdObject*)task->extra)->field_8;
    work  = task->spawnArg2;
    if (Gp_State1C->field_4 != 0) {
        if (Gp_State1C->field_4 >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->field_22++;
        switch (task->state) {
            case 0:
                Gp_SpawnEff(0x60076, coord, 0x400, NULL);
                if (task->spawnArg1 != 0) {
                    Gp_SpawnEff(0x60070, coord, 0x80004600, NULL);
                    task->state = 1;
                } else {
                    Gp_SpawnEff(0x6007C, coord, 0x100, NULL);
                    Gp_SpawnEff(0x6007C, coord, 0x100, NULL);
                    work->field_24 = 0x100;
                    work->field_26 = 0xC0;
                    task->state    = 2;
                }
                break;
            case 1:
                Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                work->field_10.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                work->field_10.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                work->field_10.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x60070, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x82003400,
                            &work->field_10);
                if ((s16)work->field_22 >= 7) {
                    task->state = 3;
                }
                break;
            case 2:
                work->field_26 -= 0x20;
                work->field_24 += 0x30;
                rgb[0]          = work->field_26;
                rgb[1]          = work->field_26 >> 1;
                rgb[2]          = work->field_26 >> 2;
                Room_Draw02(coord, 0x100, 0x100, rgb);
                Room_Draw02(coord, (s16)work->field_24, (s16)work->field_24, rgb);
                if ((s16)work->field_22 >= 7) {
                    task->state = 3;
                }
                break;
            case 3:
                Gp_ReleaseState1CMem(work, task);
                break;
        }
    }
}
