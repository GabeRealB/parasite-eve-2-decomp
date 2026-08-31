#include "common.h"

#include "main/task.h"

void func_tonfa_baton_8011DA48(Task* arg0);
void func_tonfa_baton_8011DA74(Task* arg0);
void func_tonfa_baton_8011DB6C(Task* arg0);
void WeaponsShared8011db78(Task* arg0);

/// Per-frame entry point: runs the weapon task's current state. The table is a
/// local, so GCC copies it from `.rodata` onto the stack every frame.
void func_tonfa_baton_8011DB98(Task* arg0)
{
    TaskFunc states[4] = {
        func_tonfa_baton_8011DA48,
        func_tonfa_baton_8011DA74,
        func_tonfa_baton_8011DB6C,
        WeaponsShared8011db78,
    };

    states[arg0->state](arg0);
}

INCLUDE_ASM("weapons/nonmatchings/tonfa_baton/tonfa_baton_2", func_tonfa_baton_8011DBFC);
