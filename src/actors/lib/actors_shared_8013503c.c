#include "common.h"

#include "actors/actors_shared_8013503c.h"

/// Remembers the attach coordinate's position in the work block, then steps it
/// along the matrix's third column scaled by `field_252`, and moves its height
/// 30 units toward `field_23E`.
///
/// Carried by two actor slots - `actor_103700` and `actor_203700`; the shared
/// span is in `configs/USA/overlays.toml`.
void ActorsShared8013503c(ActorShared8013503c* arg0)
{
    GsCOORDINATE2*           coord;
    ActorShared8013503cWork* work;
    s32                      y;

    coord = arg0->field_2C->field_8;
    work  = arg0->field_1C;

    work->field_22C    = coord->coord.t[0];
    work->field_22E    = coord->coord.t[1];
    work->field_230    = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_252) >> 12;
    y                  = coord->coord.t[1];
    coord->coord.t[1]  = (work->field_23E - y > 0) ? y + 30 : y - 30;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_252) >> 12;
}
