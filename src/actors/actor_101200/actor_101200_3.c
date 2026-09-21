#include "common.h"
#include "actors/actor_101200.h"
#include "actors/actor_101200_motion.h"
#include "main/gfx.h"
#include "main/mem.h"

INCLUDE_ASM("actors/nonmatchings/actor_101200/actor_101200_3", func_actor_101200_80134A08);

/// Walk back toward the spawn point: turn at most 0x10 toward it, step 8 units,
/// and hand over to state 7 once within 0x50 or after 0xDD frames (state 6 when
/// `func_actor_101200_80131F50` reports 1).
void func_actor_101200_801350B4(Actor101200Ctx* arg0, Actor101200* arg1)
{
    Actor101200Work*        work;
    GsCOORDINATE2*          coord;
    GsCOORDINATE2*          facing;
    Actor101200Obj2C*       obj;
    Actor101200TurnScratch* head;
    Actor101200TurnScratch* s;

    work = arg1->field_1C;
    if (work->field_4 != 0) {
        obj              = arg1->field_2C;
        arg0->field_14   = 0;
        obj->field_C     = 0;
        work->field_174  = 2;
        work->field_170  = 1;
        work->field_178  = 0;
        work->field_2E6 |= 0x8000;
        work->field_31E &= 0x7FFF;
        work->field_356 &= 0x7FFF;
        work->field_24E |= 0x4000;
        func_actor_101200_80132640(arg1);
        work->field_3DC = 0;
        work->field_6   = 0;
        return;
    }
    head                                      = *(Actor101200TurnScratch**)G_SCRATCH_HEAD;
    *(Actor101200TurnScratch**)G_SCRATCH_HEAD = head - 1;
    s                                         = head - 1;
    func_actor_101200_80132640(arg1);
    arg1->field_2C->field_8->flg = 0;
    head[-1].d.vx                = work->origin.vx - arg1->field_2C->field_8->coord.t[0];
    s->d.vy                      = 0;
    s->d.vz                      = work->origin.vz - arg1->field_2C->field_8->coord.t[2];
    coord                        = arg1->field_2C->field_8;
    s->angle                     = Actor101200_NormalizeYaw(ratan2(head[-1].d.vx, s->d.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    if (s->angle > 0x10) {
        s->angle = 0x10;
    }
    if (s->angle < -0x10) {
        s->angle = -0x10;
    }
    facing    = arg1->field_2C->field_8;
    s->angle += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    Gfx_RotMatrixY(&arg1->field_2C->field_8->coord, s->angle, 1);
    Actor101200_StepForward(arg1->field_2C->field_8, 8);
    func_actor_101200_8013249C(arg1->field_2C->field_8, &work->rec1B8, 5);
    work->field_6++;
    if (!Actor101200_OutOfRange(&s->d, 0x50) || work->field_6 >= 0xDD) {
        work->field_0 = 7;
    }
    if (func_actor_101200_80131F50(arg1->field_2C->field_8, &work->rec250, 5, &s->d) == 1) {
        work->field_0 = 6;
    }
    *(Actor101200TurnScratch**)G_SCRATCH_HEAD += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_101200/actor_101200_3", func_actor_101200_801354D0);
