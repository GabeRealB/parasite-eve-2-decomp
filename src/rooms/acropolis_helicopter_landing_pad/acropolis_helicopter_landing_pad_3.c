#include "common.h"
#include "main/task.h"

extern Task* D_acropolis_helicopter_landing_pad_80187F80;

void func_acropolis_helicopter_landing_pad_8017E75C(s32 arg0)
{
    D_acropolis_helicopter_landing_pad_80187F80->state = arg0;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_3", func_acropolis_helicopter_landing_pad_8017E76C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_3", func_acropolis_helicopter_landing_pad_8017E81C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_3", func_acropolis_helicopter_landing_pad_8017E974);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_3", func_acropolis_helicopter_landing_pad_8017EA6C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad_3", func_acropolis_helicopter_landing_pad_8017EB00);
