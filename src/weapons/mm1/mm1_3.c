#include "common.h"

#include "weapons/mm1.h"
#include "weapons/weapons_shared_8011d3a0.h"
#include "weapons/weapons_shared_8011ddf8.h"
#include "weapons/weapons_shared_8011de24.h"

/// Per-frame entry point: runs the weapon task's current state. The table is a
/// local, so GCC copies it from `.rodata` onto the stack every frame.
void func_mm1_8011DBD8(Mm1Task* arg0)
{
    Mm1StateFn states[4] = {
        (Mm1StateFn)WeaponsShared8011d3a0,
        func_mm1_8011D704,
        (Mm1StateFn)WeaponsShared8011ddf8,
        (Mm1StateFn)WeaponsShared8011de24,
    };

    states[arg0->state](arg0);
}
