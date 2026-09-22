#include "common.h"

#include "weapons/weapons_shared_8011e4ac.h"

void Gp_UnlinkObj(WeaponShared8011e4acObj* node);
void Gp_ReleaseState1CMem(void* mem, WeaponShared8011e4ac* task);

void WeaponsShared8011e4ac(WeaponShared8011e4ac* arg0)
{
    WeaponShared8011e4acObj* obj = arg0->field_1C;
    void*                    mem = arg0->field_20;

    if (obj != NULL) {
        Gp_UnlinkObj(obj);
    }
    Gp_ReleaseState1CMem(mem, arg0);
}
