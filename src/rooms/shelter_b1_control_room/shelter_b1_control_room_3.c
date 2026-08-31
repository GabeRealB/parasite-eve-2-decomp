#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "main/task.h"

extern s32     D_80115730;
extern s32     D_80115734;
extern s32     D_80115754;
extern SVECTOR D_shelter_b1_control_room_80181BD4[];
extern SVECTOR D_shelter_b1_control_room_80181C3C[];

void Room_Draw01(SVECTOR* v, s32 arg1, s32 arg2);
void Room_Draw13(SVECTOR* v, s32 arg1, s32 arg2);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_control_room/shelter_b1_control_room_3", func_shelter_b1_control_room_8017F100);

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
            Room_Draw01(&p[0], 0x100, 0x243);
            Room_Draw01(&p[2], 0x100, 0x243);
            Room_Draw01(&p[4], 0x100, 0x243);
            Room_Draw01(&p[6], 0x100, 0x243);
            Room_Draw01(&p[8], 0x100, 0x243);
            Room_Draw01(&p[10], 0x100, 0x243);
            Room_Draw13(&p[12], 0x180, 0x421);
            break;
        case 3:
            p = D_shelter_b1_control_room_80181BD4;
            SOFT_TOUCH_REG(p);
            Room_Draw01(&p[0], 0x100, 0x243);
            Room_Draw01(&p[2], 0x100, 0x243);
            Room_Draw01(&p[4], 0x100, 0x243);
            Room_Draw01(&p[6], 0x100, 0x243);
            Room_Draw01(&p[8], 0x100, 0x243);
            Room_Draw01(&p[10], 0x100, 0x243);
            Room_Draw13(&p[12], 0x180, 0x421);
            Room_Draw13(&p[14], 0x200, 0x23);
            Room_Draw13(&p[15], 0x200, 0x23);
            Room_Draw13(&p[16], 0x200, 0x23);
            Room_Draw13(&p[17], 0x200, 0x23);
            break;
        case 4:
            p = D_shelter_b1_control_room_80181C3C;
            SOFT_TOUCH_REG(p);
            Room_Draw13(&p[0], 0x200, 0x23);
            Room_Draw13(&p[1], 0x200, 0x23);
            Room_Draw13(&p[2], 0x200, 0x23);
            break;
        case 6:
            p = D_shelter_b1_control_room_80181C3C;
            SOFT_TOUCH_REG(p);
            Room_Draw13(&p[0], 0x200, 0x23);
            Room_Draw13(&p[1], 0x200, 0x23);
            Room_Draw13(&p[2], 0x200, 0x23);
            Room_Draw13(&p[3], 0x200, 0x23);
            break;
    }
}
