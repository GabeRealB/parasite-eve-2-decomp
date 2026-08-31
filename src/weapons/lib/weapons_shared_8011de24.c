#include "common.h"

#include "weapons/weapons_shared_8011de24.h"

void Gp_UnlinkObj(WeaponShared8011de24Obj* node);
void Task_Kill(WeaponShared8011de24* task);

void WeaponsShared8011de24(WeaponShared8011de24* arg0)
{
    WeaponShared8011de24Work* work = arg0->field_1C;

    Gp_UnlinkObj(&work->field_0);
    Gp_UnlinkObj(&work->field_20);
    Task_Kill(arg0);
}
