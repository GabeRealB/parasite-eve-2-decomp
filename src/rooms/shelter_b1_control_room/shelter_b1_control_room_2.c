#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/task.h"
#include "rooms/shelter_b1_control_room.h"

extern s8       D_8007106B;
extern s32      D_80115730;
extern s32      D_80115734;
extern s32      D_80115754;
extern TaskDesc D_shelter_b1_control_room_80181BBC;
extern SVECTOR  D_shelter_b1_control_room_80181BD4[];
extern SVECTOR  D_shelter_b1_control_room_80181C3C[];

void func_shelter_b1_control_room_8017F100(Task* arg0)
{
    Display_SpawnWithOt(&D_shelter_b1_control_room_80181BBC, 1, 0, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}

void func_shelter_b1_control_room_8017F150(Task* task)
{
    u8       view;
    SVECTOR* p;

    if (task->state == 0) {
        D_80115734  = 0x60276;
        D_80115730  = 0x60277;
        D_80115754  = 0x60278;
        task->state = 1;
    }

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
            p = D_shelter_b1_control_room_80181BD4;
            SOFT_TOUCH_REG(p);
            func_shelter_b1_control_room_8017F39C(&p[0], 0x100, 0x243);
            func_shelter_b1_control_room_8017F39C(&p[2], 0x100, 0x243);
            func_shelter_b1_control_room_8017F39C(&p[4], 0x100, 0x243);
            func_shelter_b1_control_room_8017F39C(&p[6], 0x100, 0x243);
            func_shelter_b1_control_room_8017F39C(&p[8], 0x100, 0x243);
            func_shelter_b1_control_room_8017F39C(&p[10], 0x100, 0x243);
            func_shelter_b1_control_room_8017FBE0(&p[12], 0x180, 0x421);
            break;
        case 3:
            p = D_shelter_b1_control_room_80181BD4;
            SOFT_TOUCH_REG(p);
            func_shelter_b1_control_room_8017F39C(&p[0], 0x100, 0x243);
            func_shelter_b1_control_room_8017F39C(&p[2], 0x100, 0x243);
            func_shelter_b1_control_room_8017F39C(&p[4], 0x100, 0x243);
            func_shelter_b1_control_room_8017F39C(&p[6], 0x100, 0x243);
            func_shelter_b1_control_room_8017F39C(&p[8], 0x100, 0x243);
            func_shelter_b1_control_room_8017F39C(&p[10], 0x100, 0x243);
            func_shelter_b1_control_room_8017FBE0(&p[12], 0x180, 0x421);
            func_shelter_b1_control_room_8017FBE0(&p[14], 0x200, 0x23);
            func_shelter_b1_control_room_8017FBE0(&p[15], 0x200, 0x23);
            func_shelter_b1_control_room_8017FBE0(&p[16], 0x200, 0x23);
            func_shelter_b1_control_room_8017FBE0(&p[17], 0x200, 0x23);
            break;
        case 4:
            p = D_shelter_b1_control_room_80181C3C;
            SOFT_TOUCH_REG(p);
            func_shelter_b1_control_room_8017FBE0(&p[0], 0x200, 0x23);
            func_shelter_b1_control_room_8017FBE0(&p[1], 0x200, 0x23);
            func_shelter_b1_control_room_8017FBE0(&p[2], 0x200, 0x23);
            break;
        case 6:
            p = D_shelter_b1_control_room_80181C3C;
            SOFT_TOUCH_REG(p);
            func_shelter_b1_control_room_8017FBE0(&p[0], 0x200, 0x23);
            func_shelter_b1_control_room_8017FBE0(&p[1], 0x200, 0x23);
            func_shelter_b1_control_room_8017FBE0(&p[2], 0x200, 0x23);
            func_shelter_b1_control_room_8017FBE0(&p[3], 0x200, 0x23);
            break;
    }
}
