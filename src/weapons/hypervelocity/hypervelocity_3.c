#include "common.h"

#include "main/task.h"

void func_hypervelocity_8011F374(Task* arg0);
void func_hypervelocity_8011F570(Task* arg0);
void func_hypervelocity_8011F694(Task* arg0);
void WeaponsShared8011db78(Task* arg0);

/// Per-frame entry point: runs the weapon task's current state. The table is a
/// local, so GCC copies it from `.rodata` onto the stack every frame.
void func_hypervelocity_8011F6C0(Task* arg0)
{
    TaskFunc states[4] = {
        func_hypervelocity_8011F570,
        func_hypervelocity_8011F374,
        func_hypervelocity_8011F694,
        WeaponsShared8011db78,
    };

    states[arg0->state](arg0);
}

INCLUDE_ASM("weapons/nonmatchings/hypervelocity/hypervelocity_3", func_hypervelocity_8011F724);
