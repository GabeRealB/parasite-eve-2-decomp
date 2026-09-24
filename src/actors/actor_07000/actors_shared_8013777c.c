#include "common.h"

#include "actors/actors_shared_8013777c.h"
#include "gameplay/3A34.h"
#include "main/tmd.h"

#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// Collision response of the specimen's second form: node 2's collision table
/// is run through `func_800E0C10` with a 0x38-byte scratch. Response 1 adds
/// the returned X and Z offsets to the root; only the first one (while
/// `field_39A` is clear) also adds Y, latches the response in `field_38C` and
/// `field_39A`, arms `field_2E4` to 0x400, sets the fall speed `field_398` to
/// -0x50 and takes a quarter off the step length `field_378`. Response 2 puts
/// the root back at the translation saved in `field_33C`. Both collision
/// tables are released either way.
void Actor07000_Fn0595C(Task* arg0)
{
    ActorsShared8013777cScratch* scratch;
    ActorsShared8013777cWork*    work;
    GsCOORDINATE2*               coord;
    s32                          movement;

    work     = (ActorsShared8013777cWork*)arg0->work;
    scratch  = (ActorsShared8013777cScratch*)(SCRATCH_SP -= 0x38);
    coord    = ((TmdObject*)arg0->extra)->coords;
    movement = func_800E0C10(&work->field_24C[0], &scratch->delta, 4, NULL);
    switch (movement) {
        case 0:
            break;
        case 1:
            if (work->field_39A == 0) {
                work->field_38C    = movement;
                work->field_2E4    = 0x400;
                coord->coord.t[1] += scratch->delta.vy.h.hi;
                work->field_398    = -0x50;
                work->field_378    = work->field_378 - (s16)work->field_378 / 4;
                work->field_39A    = movement;
            }
            coord->coord.t[0] += scratch->delta.vx.h.hi;
            coord->coord.t[2] += scratch->delta.vz.h.hi;
            break;
        case 2:
            coord->coord.t[0] = work->field_33C.vx;
            coord->coord.t[1] = work->field_33C.vy;
            coord->coord.t[2] = work->field_33C.vz;
            break;
    }
    Gp_ClearRec18Occupied(&work->field_24C[0]);
    Gp_ClearRec18Occupied(&work->field_2CC);
    SCRATCH_SP += 0x38;
}
