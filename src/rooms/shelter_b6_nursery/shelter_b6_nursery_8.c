#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/shelter_b6_nursery.h"

#include <psyq/libgpu.h>
#include <psyq/libgs.h>

extern void Room_Draw02(GsCOORDINATE2* coord, s32 arg1, s32 arg2, u8* rgb);
extern void Room_Draw04(GsCOORDINATE2* coord, s32 arg1, u8* rgb);
extern void Room_DrawBillboard(GsCOORDINATE2* coord, s32 arg1, u8* rgb);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_8", func_shelter_b6_nursery_80181314);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_8", func_shelter_b6_nursery_80181820);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_8", func_shelter_b6_nursery_80181EDC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_8", func_shelter_b6_nursery_80182330);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_8", func_shelter_b6_nursery_80182730);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_nursery/shelter_b6_nursery_8", func_shelter_b6_nursery_801829E4);

void func_shelter_b6_nursery_80182D14(s16 arg0, s16 arg1)
{
    D_shelter_b6_nursery_801879F0.field_0 = arg0;
    D_shelter_b6_nursery_801879F0.field_2 = arg1;
}

void func_shelter_b6_nursery_80182D28(Task* task)
{
    RoomEffWork*   work;
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
                rgb[1] = work->field_24 >> 2;
                rgb[2] = work->field_24 >> 1;
                Room_Draw04(coord, (s16)work->field_26, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                Room_Draw04(coord, (s16)(work->field_26 * 2), rgb);
                Room_Draw02(coord, (s16)(0x300 - work->field_26 * 2), 0x80, rgb);
                if (task->spawnArg1 == 0) {
                    work->field_24 = 0xFF;
                    task->state    = 2;
                    rgb[0]         = work->field_24;
                    rgb[1]         = work->field_24 >> 2;
                    rgb[2]         = work->field_24 >> 1;
                    Gp_DrawFadeQuad(rgb, 1);
                }
                break;
            case 2:
                if ((s16)work->field_24 >= 0x11) {
                    rgb[0] = work->field_24;
                    rgb[1] = work->field_24 >> 2;
                    rgb[2] = work->field_24 >> 1;
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
