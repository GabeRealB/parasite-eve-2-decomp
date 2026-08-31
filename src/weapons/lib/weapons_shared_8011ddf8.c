#include "common.h"

#include "weapons/weapons_shared_8011ddf8.h"

void WeaponsShared8011ddf8(WeaponShared8011ddf8* arg0)
{
    WeaponShared8011ddf8Work* work  = arg0->field_1C;
    s32                       timer = work->timer - 1;

    work->timer = timer;
    if (timer <= 0) {
        arg0->state = 3;
    }
}
