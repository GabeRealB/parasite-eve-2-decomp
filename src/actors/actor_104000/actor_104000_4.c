#include "common.h"

#include "actors/actor_104000.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"

extern TaskFunc D_actor_104000_8013E50C[];

INCLUDE_ASM("actors/nonmatchings/actor_104000/actor_104000_4", func_actor_104000_801388E4);

/// Restart handler variant: when the work block's field_4 latch is set, clears
/// the caller's flag and the display object, rearms the motion state (4/1/0)
/// and flips the gate bits on four records of the 0x38-stride flag table before
/// running the shared per-frame step. Otherwise it runs that step first and
/// only advances the work state when bit 0 of field_58 is set.
void func_actor_104000_801389E8(Actor104000Ctx* arg0, Actor104000* arg1)
{
    Actor104000Work*  work;
    Actor104000Obj2C* obj;

    work = arg1->field_1C;
    if (work->field_4 != 0) {
        obj              = arg1->field_2C;
        arg0->field_14   = 0;
        obj->field_C     = 0;
        work->field_174  = 4;
        work->field_170  = 1;
        work->field_178  = 0;
        work->field_36E |= 0x8000;
        work->field_3A6 &= 0x7FFF;
        work->field_3DE &= 0x7FFF;
        work->field_28E |= 0x4000;
        func_actor_104000_80132C8C(arg1);
    } else {
        func_actor_104000_80132C8C(arg1);
        if (work->field_58 & 1) {
            work->field_0 = 2;
        }
    }
}

/// Restart handler: when the work block's field_4 latch is set, clears the
/// caller's flag and the display object, rearms the motion state (6/1/0) and
/// flips the gate bits on four records of the 0x38-stride flag table before
/// running the shared per-frame step. Otherwise it runs that step first and
/// only advances the work state when bit 0 of field_58 is set.
void func_actor_104000_80138AA0(Actor104000Ctx* arg0, Actor104000* arg1)
{
    Actor104000Work*  work;
    Actor104000Obj2C* obj;

    work = arg1->field_1C;
    if (work->field_4 != 0) {
        obj              = arg1->field_2C;
        arg0->field_14   = 0;
        obj->field_C     = 0;
        work->field_174  = 6;
        work->field_170  = 1;
        work->field_178  = 0;
        work->field_36E |= 0x8000;
        work->field_3A6 &= 0x7FFF;
        work->field_3DE &= 0x7FFF;
        work->field_28E |= 0x4000;
        func_actor_104000_80132C8C(arg1);
    } else {
        func_actor_104000_80132C8C(arg1);
        if (work->field_58 & 1) {
            work->field_0 = 7;
        }
    }
}

/// Restart handler variant: when the work block's field_4 latch is set, clears
/// the display object, sets the caller's flag to 5, rearms the motion state
/// (1/2/0), flips the gate bits on four records of the 0x38-stride flag table
/// and, after the shared per-frame step, rebuilds the display coordinate's
/// matrix as a half-turn roll plus the yaw its own third column already points
/// at. Otherwise it just runs the shared step.
void func_actor_104000_80138B58(Actor104000Ctx* arg0, Actor104000* arg1)
{
    Actor104000Work* work;
    GsCOORDINATE2*   coord;
    s32              yaw;

    work = arg1->field_1C;
    if (work->field_4 != 0) {
        arg1->field_2C->field_C = 0;
        arg0->field_14          = 5;
        work->field_174         = 1;
        work->field_170         = 2;
        work->field_178         = 0;
        work->field_36E        |= 0x8000;
        work->field_3A6        &= 0x7FFF;
        work->field_3DE        &= 0x7FFF;
        work->field_28E        |= 0x4000;
        func_actor_104000_80132C8C(arg1);
        coord = arg1->field_2C->field_8;
        yaw   = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        Gfx_RotMatrixZ(&arg1->field_2C->field_8->coord, 0x800, 1);
        Gfx_RotMatrixY(&arg1->field_2C->field_8->coord, (s16)yaw, 0);
        arg1->field_2C->field_8->flg = 0;
        return;
    }
    func_actor_104000_80132C8C(arg1);
}
