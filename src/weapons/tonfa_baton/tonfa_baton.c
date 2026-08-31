#include "common.h"

#include "main/task.h"

INCLUDE_ASM("weapons/nonmatchings/tonfa_baton/tonfa_baton", func_tonfa_baton_8011D1EC);

INCLUDE_ASM("weapons/nonmatchings/tonfa_baton/tonfa_baton", func_tonfa_baton_8011D6B0);

INCLUDE_ASM("weapons/nonmatchings/tonfa_baton/tonfa_baton", func_tonfa_baton_8011DA48);

INCLUDE_ASM("weapons/nonmatchings/tonfa_baton/tonfa_baton", func_tonfa_baton_8011DA74);

void func_tonfa_baton_8011DB6C(Task* arg0)
{
    arg0->state = 3;
}

INCLUDE_RODATA("weapons/nonmatchings/tonfa_baton/tonfa_baton", D_tonfa_baton_8011D1C0);
