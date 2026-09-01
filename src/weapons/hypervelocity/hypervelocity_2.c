#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/session.h"
#include "main/task.h"

void WeaponsShared8011db78(Task* task);
void func_hypervelocity_8011EC1C(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, u8* arg3);

void func_hypervelocity_8011F168(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s16            val;
    u8             rgb[3];

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }

    Gp_UpdateCoord(coord);
    mem->field_22++;
    if (arg0->state == 0) {
        mem->field_24 = 0xF0;
        mem->field_26 = 0x100;
        arg0->state   = 1;
    }
    rgb[0] = mem->field_24 >> 1;
    rgb[1] = mem->field_24 >> 1;
    rgb[2] = mem->field_24;
    Gp_DrawBand(coord, mem->field_26, rgb);
    mem->field_26 += 0x40;
    val            = mem->field_24 - 0x10;
    mem->field_24  = val;
    if (val < 0x10) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

void func_hypervelocity_8011F270(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s16            val;
    u8             rgb[3];

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }

    Gp_UpdateCoord(coord);
    mem->field_22++;
    if (arg0->state == 0) {
        mem->field_24 = 0x80;
        mem->field_26 = 0x200;
        arg0->state   = 1;
    }
    rgb[0] = mem->field_24;
    rgb[1] = mem->field_24;
    rgb[2] = mem->field_24;
    func_hypervelocity_8011EC1C(coord, mem->field_22, mem->field_26, rgb);
    mem->field_26 += 0x60;
    val            = mem->field_24 - 8;
    mem->field_24  = val;
    if (val < 6) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

INCLUDE_ASM("weapons/nonmatchings/hypervelocity/hypervelocity_2", func_hypervelocity_8011F374);

void func_hypervelocity_8011F570(Task* arg0)
{
    Task*          child;
    TmdObject*     childExtra;
    TmdObject*     extra;
    GsCOORDINATE2* coord;

    extra               = (TmdObject*)arg0->extra;
    coord               = (GsCOORDINATE2*)extra->field_8;
    arg0->state        += 1;
    arg0->exitCallback  = WeaponsShared8011db78;
    arg0->killCountdown = 0;
    coord->flg          = 0;
    extra->field_C      = 0;
    if (!(arg0->spawnArg1 & 0xF)) {
        child = Task_Spawn(7, 0x70, 1, 0);
        if (child != NULL) {
            ((GsCOORDINATE2*)((TmdObject*)child->extra)->field_8)->sub = coord;
            childExtra                                                 = (TmdObject*)child->extra;
            childExtra->field_20                                       = extra->field_20;
            childExtra->field_1C                                       = extra->field_1C;
            Task_Reparent(arg0, child);
        }
        child = Task_Spawn(7, 0x74, 2, 0);
        if (child != NULL) {
            ((GsCOORDINATE2*)((TmdObject*)child->extra)->field_8)->sub = coord;
            childExtra                                                 = (TmdObject*)child->extra;
            childExtra->field_20                                       = extra->field_20;
            childExtra->field_1C                                       = extra->field_1C;
            Task_Reparent(arg0, child);
            coord->coord.t[0] = -6;
            coord->coord.t[1] = -0x3C;
            coord->coord.t[2] = -0x16;
        }
    }
}

void func_hypervelocity_8011F694(Task* arg0)
{
    arg0->state = 3;
}
