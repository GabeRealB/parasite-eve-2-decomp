#include "common.h"

#include "main/game.h"

INCLUDE_ASM("main/nonmatchings/2C160", func_8003B960);

INCLUDE_ASM("main/nonmatchings/2C160", func_8003BD34);

INCLUDE_ASM("main/nonmatchings/2C160", func_8003C110);

INCLUDE_ASM("main/nonmatchings/2C160", func_8003C4F0);

INCLUDE_ASM("main/nonmatchings/2C160", func_8003C6D8);

INCLUDE_ASM("main/nonmatchings/2C160", func_8003C728);

void func_8003C748(MATRIX* arg0, volatile SVECTOR* arg1) {
    register short t4 asm("t4");
    register short t5 asm("t5");
    register short t6 asm("t6");

    t4 = arg0->m[0][1];
    t5 = arg0->m[1][1];
    t6 = arg0->m[2][1];
    arg1->vx = t4;
    arg1->vy = t5;
    arg1->vz = t6;
}

INCLUDE_ASM("main/nonmatchings/2C160", func_8003C768);

INCLUDE_ASM("main/nonmatchings/2C160", func_8003C788);

INCLUDE_ASM("main/nonmatchings/2C160", func_8003C98C);

INCLUDE_ASM("main/nonmatchings/2C160", func_8003CB80);

INCLUDE_ASM("main/nonmatchings/2C160", func_8003CD78);

INCLUDE_ASM("main/nonmatchings/2C160", func_8003CEC4);

INCLUDE_ASM("main/nonmatchings/2C160", func_8003D000);
