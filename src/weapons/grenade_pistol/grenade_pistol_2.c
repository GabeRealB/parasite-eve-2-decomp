#include "common.h"

#include "main/task.h"

void func_grenade_pistol_8011D3A0(Task* arg0);
void func_grenade_pistol_8011D6FC(Task* arg0);
void WeaponsShared8011ddf8(Task* arg0);
void WeaponsShared8011de24(Task* arg0);

const TaskFuncTable4 D_grenade_pistol_8011D1C4 = { {
    func_grenade_pistol_8011D3A0,
    func_grenade_pistol_8011D6FC,
    WeaponsShared8011ddf8,
    WeaponsShared8011de24,
} };

void func_grenade_pistol_8011DBD0(Task* arg0)
{
    TaskFuncTable4 sp;

    sp = D_grenade_pistol_8011D1C4;
    sp.funcs[arg0->state](arg0);
}
