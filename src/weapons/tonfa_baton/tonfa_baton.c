#include "common.h"

#include <psyq/libgte.h>

#include "main/session.h"
#include "main/task.h"

void WeaponsShared8011db78(Task* task);

INCLUDE_ASM("weapons/nonmatchings/tonfa_baton/tonfa_baton", func_tonfa_baton_8011D1EC);

INCLUDE_ASM("weapons/nonmatchings/tonfa_baton/tonfa_baton", func_tonfa_baton_8011D6B0);

void func_tonfa_baton_8011DA48(Task* arg0)
{
    GameActorExt* extra;
    s32*          ptr;

    extra              = (GameActorExt*)arg0->extra;
    ptr                = extra->field_8;
    arg0->state        = arg0->state + 1;
    arg0->exitCallback = WeaponsShared8011db78;
    *ptr               = 0;
    extra->field_C     = 0;
}

INCLUDE_ASM("weapons/nonmatchings/tonfa_baton/tonfa_baton", func_tonfa_baton_8011DA74);

void func_tonfa_baton_8011DB6C(Task* arg0)
{
    arg0->state = 3;
}

INCLUDE_RODATA("weapons/nonmatchings/tonfa_baton/tonfa_baton", D_tonfa_baton_8011D1C0);
