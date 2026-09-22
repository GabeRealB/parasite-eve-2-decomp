#include "common.h"

#include "main/task.h"
#include "weapons/weapons_shared_8011de24.h"

void Gp_UnlinkObj(WeaponShared8011de24Obj* node);

void WeaponsShared8011de24(Task* task)
{
    WeaponShared8011de24Work* work = task->work;

    Gp_UnlinkObj(&work->field_0);
    Gp_UnlinkObj(&work->field_20);
    taskKill(task);
}
