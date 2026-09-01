#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3FB8.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/task.h"
#include "weapons/tonfa_baton.h"

void WeaponsShared8011db78(Task* task);

INCLUDE_ASM("weapons/nonmatchings/tonfa_baton/tonfa_baton", func_tonfa_baton_8011D1EC);

INCLUDE_ASM("weapons/nonmatchings/tonfa_baton/tonfa_baton", func_tonfa_baton_8011D6B0);

void func_tonfa_baton_8011DA48(Task* arg0)
{
    TmdObject* extra;
    s32*       ptr;

    extra              = (TmdObject*)arg0->extra;
    ptr                = extra->field_8;
    arg0->state        = arg0->state + 1;
    arg0->exitCallback = WeaponsShared8011db78;
    *ptr               = 0;
    extra->field_C     = 0;
}

void func_tonfa_baton_8011DA74(Task* arg0)
{
    TmdObject*  extra;
    TonfaCoord* coord;
    GameActor*  actor;
    s32         mode;

    extra          = (TmdObject*)arg0->extra;
    coord          = (TonfaCoord*)extra->field_8;
    actor          = ((GpActorWork*)Game_GetPtrSlot(3))->actor;
    coord->flg     = 0;
    extra->field_C = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_C;

    coord->coord.t[0] = 0;
    coord->coord.t[1] = 0x60;
    coord->coord.t[2] = 0;

    if (*(u32*)&actor->field_954 != 0x40000) {
        arg0->spawnArg1 = 0;
    }

    mode = arg0->spawnArg1 & 0xF;
    switch (mode) {
        case 0:
            if (coord->angle > 0) {
                coord->angle = coord->angle - 0x100;
            }
            break;
        case 1:
            if (coord->angle < 0x800) {
                coord->angle = coord->angle + 0x1C0;
            }
            break;
    }
    Gfx_RotMatrixZ(&coord->coord, coord->angle, 1);
}

void func_tonfa_baton_8011DB6C(Task* arg0)
{
    arg0->state = 3;
}

INCLUDE_RODATA("weapons/nonmatchings/tonfa_baton/tonfa_baton", D_tonfa_baton_8011D1C0);
