#include "common.h"

#include "main/game.h"

INCLUDE_ASM("main/nonmatchings/18E64", func_80028664);

void func_80028718(void)
{
    D_80071210      = 0;
    D_800711B8.next = NULL;
    D_800711B8.prev = &D_800711B8;
    D_800711C0.next = NULL;
    D_800711C0.prev = &D_800711C0;
}
