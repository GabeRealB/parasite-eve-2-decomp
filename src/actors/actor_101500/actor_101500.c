#include "common.h"
#include "actors/actor_101500.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/sound.h"
#include "main/tmd.h"

INCLUDE_ASM("actors/nonmatchings/actor_101500/actor_101500", func_actor_101500_80131EB4);

INCLUDE_RODATA("actors/nonmatchings/actor_101500/actor_101500", D_actor_101500_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_101500/actor_101500", ActorsShared80135df4Table);

INCLUDE_ASM("actors/nonmatchings/actor_101500/actor_101500", func_actor_101500_8013230C);

void func_actor_101500_8013291C(Actor101500* actor, s32 damage)
{
    GpEnemy*         enemy;
    Actor101500Work* work;
    GsCOORDINATE2*   coord;
    s32              id;

    enemy            = actor->field_20;
    work             = actor->field_1C;
    coord            = actor->field_2C->field_8;
    enemy->field_40 -= damage;
    if (enemy->field_40 <= 0) {
        work->field_378 = 1;
    }
    id = ((actor->field_20->field_8 >> 12) << 8) | 0x400F0004;
    SndEvt_EnqueueType6(id, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
    if (enemy->field_40 <= (D_actor_101500_8013BDDC * 60) / 100) {
        work->field_358 = 2;
        if (work->field_35A != 5) {
            work->field_35A          = 4;
            work->field_244.field_12 = -300;
            work->field_244.field_14 = 0;
        }
    } else {
        work->field_35A = 7;
        switch (work->field_358) {
            case 0:
                if (work->field_36E == 0) {
                    work->field_352 = 11;
                    work->field_354 = 1;
                } else {
                    work->field_352 = 12;
                    work->field_354 = 2;
                }
                break;
            case 1:
                work->field_352 = 13;
                work->field_354 = 6;
                break;
            case 2:
                work->field_352 = 13;
                work->field_354 = 14;
                break;
        }
        work->field_34C = 0;
        work->field_356 = 0;
    }
    Gp_SetStateF0Byte3(2);
}

INCLUDE_ASM("actors/nonmatchings/actor_101500/actor_101500", func_actor_101500_80132AC4);
