#include "common.h"

#include "weapons/m4a1_grenade.h"
#include "weapons/weapons_shared_8011ddf8.h"
#include "weapons/weapons_shared_8011de24.h"

/// Per-frame entry point: runs the weapon task's current state. The table is a
/// local, so GCC copies it from `.rodata` onto the stack every frame.
void func_m4a1_grenade_8011DE68(Task* arg0)
{
    M4a1GrenadeStateFn states[4] = {
        func_m4a1_grenade_8011D654,
        func_m4a1_grenade_8011D994,
        (M4a1GrenadeStateFn)WeaponsShared8011ddf8,
        (M4a1GrenadeStateFn)WeaponsShared8011de24,
    };

    states[arg0->state](arg0);
}
