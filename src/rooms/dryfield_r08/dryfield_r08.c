#include "common.h"

#include "gameplay/D4.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern SVECTOR D_dryfield_r08_8017F464[];
extern SVECTOR D_dryfield_r08_8017F4C4[];
extern s32     D_dryfield_r08_80180C24;

void func_dryfield_r08_8017EB68(SVECTOR* arg0, s32 arg1, s32 arg2);

void func_dryfield_r08_8017D5F8(Task* task)
{
    s32 i;
    u8  view;

    if (task->state == 0) {
        D_dryfield_r08_80180C24 = 0;
        task->state             = task->state + 1;
    }

    view = Gp_GetViewIndex();
    switch (view) {
        case 2: {
            SVECTOR* q;

            q = D_dryfield_r08_8017F4C4;
            Room_Draw31(&q[0], 0x200, 0x444);
            Room_Draw31(&q[2], 0x200, 0x444);
            Room_Draw31(&q[3], 0x200, 0x444);
            Room_Draw31(&q[6], 0x200, 0x444);
            Room_Draw31(&q[14], 0x200, 0x444);
            Room_Draw31(&q[15], 0x200, 0x444);
            Room_Draw31(&q[16], 0x200, 0x444);
            Room_Draw31(&q[17], 0x200, 0x444);
            break;
        }
        case 3:
            for (i = D_dryfield_r08_80180C24; i < 12; i++) {
                func_dryfield_r08_8017EB68(&D_dryfield_r08_8017F464[i], 0xA0, 0x3888);
            }
            break;
        case 4:
            for (i = D_dryfield_r08_80180C24; i < 12; i++) {
                func_dryfield_r08_8017EB68(&D_dryfield_r08_8017F464[i], 0xA0, 0x3888);
            }
            break;
        case 5: {
            SVECTOR* q;

            q = D_dryfield_r08_8017F4C4;
            Room_Draw31(&q[0], 0x200, 0x444);
            Room_Draw31(&q[1], 0x200, 0x444);
            Room_Draw31(&q[2], 0x200, 0x444);
            Room_Draw31(&q[7], 0x200, 0x444);
            Room_Draw31(&q[8], 0x200, 0x444);
            Room_Draw31(&q[21], 0x200, 0x400);
            Room_Draw31(&q[22], 0x200, 0x400);
            Room_Draw31(&q[23], 0x200, 0x400);
            Room_Draw31(&q[29], 0x200, 0x400);
            break;
        }
        case 6: {
            SVECTOR* q;

            q = D_dryfield_r08_8017F4C4;
            Room_Draw31(&q[0], 0x200, 0x433);
            Room_Draw31(&q[2], 0x200, 0x433);
            Room_Draw31(&q[3], 0x200, 0x433);
            Room_Draw31(&q[6], 0x200, 0x433);
            Room_Draw31(&q[14], 0x200, 0x433);
            Room_Draw31(&q[15], 0x200, 0x433);
            Room_Draw31(&q[16], 0x200, 0x433);
            Room_Draw31(&q[17], 0x200, 0x433);
            break;
        }
    }
}

/// The zero word the target keeps between this unit's jump table and
/// `func_dryfield_r08_8017D8B4`'s: the original TU's own tables were preceded
/// by `.align 3`, and this one pads the end of this function's table to the
/// 8-byte boundary the next one started on.
const u32 D_dryfield_r08_8017D5D8 SECTION(".rodata") = 0;

INCLUDE_ASM("rooms/nonmatchings/dryfield_r08/dryfield_r08", func_dryfield_r08_8017D8B4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_r08/dryfield_r08", func_dryfield_r08_8017DEFC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_r08/dryfield_r08", func_dryfield_r08_8017E36C);
