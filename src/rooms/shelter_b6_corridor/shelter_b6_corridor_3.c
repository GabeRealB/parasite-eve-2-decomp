#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "rooms/shelter_b6_corridor.h"

extern s32 Gp_GetViewIndex(void);

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
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F834[0], 0x140, 0x442);
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F834[2], 0x140, 0x442);
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F834[8], 0x140, 0x442);
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F834[10], 0x140, 0x442);
            break;
        case 3:
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F844[0], 0x140, 0x442);
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F844[2], 0x140, 0x442);
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F844[4], 0x140, 0x442);
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F844[8], 0x140, 0x442);
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F844[10], 0x140, 0x442);
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F844[12], 0x140, 0x442);
            break;
        case 4:
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F874[0], 0x140, 0x442);
            break;
    }
}
