#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "main/task.h"

extern s32     D_shelter_b3_dumping_hole_8018F4D8;
extern SVECTOR D_shelter_b3_dumping_hole_8018B86C[];
extern SVECTOR D_shelter_b3_dumping_hole_8018B8BC[];
extern SVECTOR D_shelter_b3_dumping_hole_8018B92C[];
extern SVECTOR D_shelter_b3_dumping_hole_8018B93C[];
extern SVECTOR D_shelter_b3_dumping_hole_8018B94C[];
extern SVECTOR D_shelter_b3_dumping_hole_8018B95C[];
extern SVECTOR D_shelter_b3_dumping_hole_8018B96C[];
extern SVECTOR D_shelter_b3_dumping_hole_8018B974[];
extern SVECTOR D_shelter_b3_dumping_hole_8018B98C[];

void Room_Draw13(SVECTOR* v, s32 arg1, s32 arg2);
void Room_Draw01(SVECTOR* v, s32 arg1, s32 arg2);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_5", func_shelter_b3_dumping_hole_80183CA0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_5", func_shelter_b3_dumping_hole_80183D34);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_5", func_shelter_b3_dumping_hole_80183E08);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_5", func_shelter_b3_dumping_hole_80183E6C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_5", func_shelter_b3_dumping_hole_80183F04);

void func_shelter_b3_dumping_hole_80183F84(Task* task)
{
    u8 view;

    if (task->state == 0) {
        D_shelter_b3_dumping_hole_8018F4D8 = 0;
        task->state                        = 1;
    }

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[0], 0x300, 0x100);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[1], 0x300, 0x200);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[2], 0x300, 0x300);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[3], 0x300, 0x400);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[4], 0x300, 0x400);
            break;
        case 3:
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[0], 0x300, 0x200);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[1], 0x300, 0x300);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[2], 0x300, 0x400);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[3], 0x300, 0x400);
            break;
        case 4:
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B94C[0], 0x280, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B94C[1], 0x280, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B94C[9], 0x300, 0x400);
            break;
        case 7:
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B974[0], 0x300, 0x400);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B974[1], 0x300, 0x400);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B974[2], 0x300, 0x400);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B974[3], 0x300, 0x400);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B974[4], 0x300, 0x400);
            break;
        case 14:
            if (D_shelter_b3_dumping_hole_8018F4D8 != 0) {
                Room_Draw13(&D_shelter_b3_dumping_hole_8018B95C[0], 0x280, 0x44);
                Room_Draw13(&D_shelter_b3_dumping_hole_8018B95C[1], 0x280, 0x40);
            }
            break;
        case 15:
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B86C[0], 0x200, 0x444);
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B86C[2], 0x200, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[0x20], 0x300, 0x100);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[0x21], 0x300, 0x200);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[0x22], 0x300, 0x300);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[0x23], 0x300, 0x400);
            break;
        case 17:
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B98C[0], 0x300, 0x400);
            break;
        case 18:
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B8BC[0], 0x200, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B8BC[0x12], 0x280, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B8BC[0x13], 0x280, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B8BC[0x19], 0x300, 0x400);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B8BC[0x1A], 0x300, 0x300);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B8BC[0x1B], 0x300, 0x200);
            break;
        case 19:
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[0], 0x300, 0x400);
            break;
        case 21:
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B92C[0], 0x400, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B92C[1], 0x400, 0x444);
            break;
        case 22:
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B95C[0], 0x280, 0x44);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B95C[1], 0x280, 0x40);
            break;
        case 23:
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B86C[0], 0x200, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[0x20], 0x300, 0x400);
            break;
        case 26:
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B93C[0], 0x300, 0x400);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B93C[1], 0x300, 0x400);
            break;
        case 29:
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B94C[0], 0x280, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B94C[1], 0x280, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B94C[2], 0x280, 0x44);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B94C[3], 0x280, 0x40);
            break;
        case 30:
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B94C[0], 0x280, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B94C[1], 0x280, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B94C[2], 0x280, 0x44);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B94C[3], 0x280, 0x40);
            break;
        case 31:
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B86C[0], 0x200, 0x444);
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B86C[2], 0x200, 0x444);
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B86C[4], 0x200, 0x444);
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B86C[12], 0x200, 0x444);
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B86C[14], 0x200, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[24], 0x400, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[26], 0x400, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[32], 0x300, 0x200);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[33], 0x300, 0x300);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[34], 0x300, 0x400);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[38], 0x300, 0x200);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[39], 0x300, 0x300);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[40], 0x300, 0x400);
            break;
        case 34:
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[0], 0x300, 0x200);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[1], 0x300, 0x200);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[2], 0x300, 0x300);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[3], 0x300, 0x300);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[4], 0x300, 0x400);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[5], 0x300, 0x400);
            break;
        case 35:
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[0], 0x300, 0x200);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[1], 0x300, 0x400);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[6], 0x300, 0x200);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B96C[7], 0x300, 0x400);
            break;
        case 13:
        case 37:
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B86C[0], 0x200, 0x444);
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B86C[2], 0x200, 0x444);
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B86C[4], 0x200, 0x444);
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B86C[6], 0x200, 0x444);
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B86C[12], 0x200, 0x444);
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B86C[14], 0x200, 0x444);
            Room_Draw01(&D_shelter_b3_dumping_hole_8018B86C[16], 0x200, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[24], 0x400, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[25], 0x400, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[26], 0x400, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[27], 0x400, 0x444);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[32], 0x300, 0x100);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[33], 0x300, 0x200);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[34], 0x300, 0x300);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[35], 0x300, 0x400);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[38], 0x300, 0x100);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[39], 0x300, 0x200);
            Room_Draw13(&D_shelter_b3_dumping_hole_8018B86C[40], 0x300, 0x300);
            break;
    }
}
