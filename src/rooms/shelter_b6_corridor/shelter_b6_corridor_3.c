#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"

extern s32 Gp_GetViewIndex(void);
void       Room_Draw01(SVECTOR* v, s32 arg1, s32 arg2);

extern SVECTOR D_shelter_b6_corridor_8017F834[];
extern SVECTOR D_shelter_b6_corridor_8017F844[];
extern SVECTOR D_shelter_b6_corridor_8017F874[];
extern s32     D_shelter_b6_corridor_801851B8;

void func_shelter_b6_corridor_8017E238(Task* task)
{
    u8 view;

    if (task->state == 0) {
        D_shelter_b6_corridor_801851B8 = 0;
        task->state                    = 1;
    }

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
            Room_Draw01(&D_shelter_b6_corridor_8017F834[0], 0x140, 0x442);
            Room_Draw01(&D_shelter_b6_corridor_8017F834[2], 0x140, 0x442);
            Room_Draw01(&D_shelter_b6_corridor_8017F834[8], 0x140, 0x442);
            Room_Draw01(&D_shelter_b6_corridor_8017F834[10], 0x140, 0x442);
            break;
        case 3:
            Room_Draw01(&D_shelter_b6_corridor_8017F844[0], 0x140, 0x442);
            Room_Draw01(&D_shelter_b6_corridor_8017F844[2], 0x140, 0x442);
            Room_Draw01(&D_shelter_b6_corridor_8017F844[4], 0x140, 0x442);
            Room_Draw01(&D_shelter_b6_corridor_8017F844[8], 0x140, 0x442);
            Room_Draw01(&D_shelter_b6_corridor_8017F844[10], 0x140, 0x442);
            Room_Draw01(&D_shelter_b6_corridor_8017F844[12], 0x140, 0x442);
            break;
        case 4:
            Room_Draw01(&D_shelter_b6_corridor_8017F874[0], 0x140, 0x442);
            break;
    }
}
