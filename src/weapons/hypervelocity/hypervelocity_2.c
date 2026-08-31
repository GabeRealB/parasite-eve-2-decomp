#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/session.h"
#include "main/task.h"

void WeaponsShared8011db78(Task* task);

INCLUDE_ASM("weapons/nonmatchings/hypervelocity/hypervelocity_2", func_hypervelocity_8011F168);

INCLUDE_ASM("weapons/nonmatchings/hypervelocity/hypervelocity_2", func_hypervelocity_8011F270);

INCLUDE_ASM("weapons/nonmatchings/hypervelocity/hypervelocity_2", func_hypervelocity_8011F374);

void func_hypervelocity_8011F570(Task* arg0)
{
    Task*          child;
    GameActorExt*  childExtra;
    GameActorExt*  extra;
    GsCOORDINATE2* coord;

    extra               = (GameActorExt*)arg0->extra;
    coord               = (GsCOORDINATE2*)extra->field_8;
    arg0->state        += 1;
    arg0->exitCallback  = WeaponsShared8011db78;
    arg0->killCountdown = 0;
    coord->flg          = 0;
    extra->field_C      = 0;
    if (!(arg0->spawnArg1 & 0xF)) {
        child = Task_Spawn(7, 0x70, 1, 0);
        if (child != NULL) {
            ((GsCOORDINATE2*)((GameActorExt*)child->extra)->field_8)->sub = coord;
            childExtra                                                    = (GameActorExt*)child->extra;
            childExtra->field_20                                          = extra->field_20;
            childExtra->field_1C                                          = extra->field_1C;
            Task_Reparent(arg0, child);
        }
        child = Task_Spawn(7, 0x74, 2, 0);
        if (child != NULL) {
            ((GsCOORDINATE2*)((GameActorExt*)child->extra)->field_8)->sub = coord;
            childExtra                                                    = (GameActorExt*)child->extra;
            childExtra->field_20                                          = extra->field_20;
            childExtra->field_1C                                          = extra->field_1C;
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
