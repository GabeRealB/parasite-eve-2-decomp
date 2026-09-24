#include "common.h"

#include "actors/actor_101500.h"

/// Stagger state, entered from a hit reaction or at low hit points: a slow
/// forward drift (`field_360`) with a climb of 0x80 a frame (`field_366`),
/// the second collision object's centre moved to (0, -300, 0).
void Actor01500_Fn0288C(Actor101500* arg0)
{
    Actor101500Work* work = arg0->field_1C;

    work->field_360        = 0x14;
    work->field_366        = 0x80;
    work->field_244.pos.vy = -300;
    work->field_244.pos.vz = 0;
}
