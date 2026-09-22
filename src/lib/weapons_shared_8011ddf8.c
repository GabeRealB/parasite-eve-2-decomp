#include "common.h"
#include "weapons/weapons_shared_8011ddf8.h"
#include "main/task.h"
#include "weapons/weapons_shared_8011de24.h"

void Gp_UnlinkObj(WeaponShared8011de24Obj* node);

void WeaponsShared8011ddf8(WeaponShared8011ddf8* arg0)
{
    WeaponShared8011ddf8Work* work  = arg0->field_1C;
    s32                       timer = work->timer - 1;

    work->timer = timer;
    if (timer <= 0) {
        arg0->state = 3;
    }
}

void WeaponsShared8011de24(Task* task)
{
    WeaponShared8011de24Work* work = task->work;

    Gp_UnlinkObj(&work->field_0);
    Gp_UnlinkObj(&work->field_20);
    taskKill(task);
}
