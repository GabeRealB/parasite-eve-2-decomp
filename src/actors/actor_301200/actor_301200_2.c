#include "common.h"
#include "actors/actor_301200.h"

INCLUDE_ASM("actors/nonmatchings/actor_301200/actor_301200_2", func_actor_301200_8016288C);

/// Restarts the animation when `field_4` is set; otherwise rolls the shared LCG
/// to occasionally advance the substate, then switches to the armed state once
/// the camera target comes within 2000 units in XZ.
void func_actor_301200_80162E60(Actor301200Ctx* arg0, Actor301200* arg1)
{
    Actor301200Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR          delta;
    SVECTOR*         d;
    TmdObject*       obj;

    work = arg1->field_1C;
    if (work->field_4 != 0) {
        obj                 = arg1->field_2C;
        arg0->field_14      = 0;
        obj->field_C        = 0;
        work->field_174     = 5;
        work->field_170     = 1;
        work->field_178     = 0;
        work->obj2C8.flags |= 0x8000;
        work->obj300.flags &= 0x7FFF;
        work->obj338.flags &= 0x7FFF;
        work->obj230.flags |= 0x4000;
        func_actor_301200_80162640(arg1);
        return;
    }
    func_actor_301200_80162640(arg1);
    if ((work->field_58 & 2) && work->field_17C >= 0x19) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 0x10) & 7)) {
            work->field_0 = 3;
        }
    }
    coord    = arg1->field_2C->field_8;
    d        = &delta;
    delta.vx = D_80073B8C->t[0] - coord->coord.t[0];
    d->vy    = D_80073B8C->t[1] - coord->coord.t[1];
    d->vz    = D_80073B8C->t[2] - coord->coord.t[2];
    if (!Actor301200_OutOfRange(d, 2000)) {
        Gp_ArmStateF0(1);
        work->field_0 = 3;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_301200/actor_301200_2", func_actor_301200_80163054);

INCLUDE_ASM("actors/nonmatchings/actor_301200/actor_301200_2", func_actor_301200_801635FC);

INCLUDE_ASM("actors/nonmatchings/actor_301200/actor_301200_2", func_actor_301200_80163DFC);
