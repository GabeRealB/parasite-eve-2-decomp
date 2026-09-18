#include "common.h"

#include "actors/actors_shared_8013777c.h"
#include "gameplay/3A34.h"
#include "main/tmd.h"

#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// The step comes out of `func_800E0C10` as the collision response's own mode:
/// 0 is a free step, 1 a sliding one and 2 a snap back to the position the work
/// saved in `field_33C`. A sliding step arms the walk - the mode latches into
/// `field_38C` and `field_39A`, the vertical speed is set to -0x50 and the
/// forward speed loses a quarter - but only the first one does, since
/// `field_39A` stays non-zero afterwards. Either way the x and z translation
/// the walk produced is added onto the model coordinate; a snap back writes the
/// saved position instead. The two collision tables are wiped before the
/// scratch is handed back.
///
/// Carried by two enemy slots - `actor_107000` and `actor_207000` - which both
/// reach the block through `Task::work`, so the body takes the `Task` rather
/// than either carrier's own context type; the shared span is in
/// `configs/USA/overlays.toml`.
void ActorsShared8013777c(Task* arg0)
{
    ActorsShared8013777cScratch* scratch;
    ActorsShared8013777cWork*    work;
    GsCOORDINATE2*               coord;
    s32                          movement;

    work     = (ActorsShared8013777cWork*)arg0->work;
    scratch  = (ActorsShared8013777cScratch*)(SCRATCH_SP -= 0x38);
    coord    = ((TmdObject*)arg0->extra)->field_8;
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
