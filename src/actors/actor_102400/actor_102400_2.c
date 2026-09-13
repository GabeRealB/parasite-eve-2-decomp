#include "common.h"

#include "actors/actor_102400.h"

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400_2", func_actor_102400_80134DB4);

/// Resets the two part coordinates and walks their local Z either apart or
/// back together, depending on the work block's direction flag. Entry Z is
/// parked at -0x5F and its travel is bounded below by 0; the follower's travel
/// is bounded below by 0x1E instead, so the pair never collapses.
void func_actor_102400_80134EB8(Actor102400* arg0)
{
    Actor102400Work* work;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   c2;
    GsCOORDINATE2*   c3;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    c2    = coord + 2;
    c3    = coord + 3;

    c2->coord.t[0] = 0;
    c2->coord.t[1] = -0x5F;
    if (work->field_134 != 0) {
        c2->coord.t[2] += 0x14;
    } else {
        c2->coord.t[2] -= 0x28;
        if (c2->coord.t[2] < 0) {
            c2->coord.t[2] = 0;
        }
    }
    c2->flg        = 0;
    c3->coord.t[0] = 0;
    c3->coord.t[1] = 0;
    if (work->field_134 != 0) {
        c3->coord.t[2] += 0x50;
    } else {
        c3->coord.t[2] -= 0xA0;
        if (c3->coord.t[2] < 0x1E) {
            c3->coord.t[2] = 0x1E;
        }
    }
    c3->flg = 0;
}
