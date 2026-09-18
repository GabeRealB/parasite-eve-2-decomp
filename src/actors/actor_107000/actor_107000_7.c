#include "common.h"

#include "actors/actor_107000.h"
#include "actors/actors_shared_80136614.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/task.h"
#include "main/tmd.h"

// actor_207000 carries the same body as func_actor_207000_8014E4D8; the two
// were kept apart only because the body's last call was this overlay's own
// func_actor_107000_80136614, now the shared ActorsShared80136614 - the same
// objection ActorsShared80136288 carried before promotion.

/// Both `Gp_LcgState` draws fold `(state >> 16) % 100` down to under 11 for a
/// hit, and each is taken through its own local so the two divides stay separate
/// objects: the compiler keeps the second draw's quotient alive across the
/// scaling chain, and `SCHED_BARRIER` holds the reaction branch's assignment on
/// the far side of it. The two barriers are load-bearing, not decoration —
/// dropping either one re-schedules the `branch` write to before the chain and
/// the whole block's allocation follows it.
void func_actor_107000_801364D8(Task* arg0)
{
    u32              sp10;
    Actor107000Work* work;
    u32              rng;
    u32              rng2;
    u32              hi;
    u32              quotient;
    u32              roll;
    s32              hit;
    s16              branch;

    work = (Actor107000Work*)arg0->work;
    ActorsShared80136614(((TmdObject*)arg0->extra)->field_8, &sp10);
    if (Gp_CountRec18Hi(&work->field_214, 0x10000) == 0 || sp10 >= 0xBB8U) {
        work->field_382 = 0;
        work->field_36E = 0;
    } else {
        rng         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rng;
        if ((u16)((rng >> 0x10) % 100U) < 0xBU) {
            branch = 3;
        } else if (sp10 >= 0x9C4U) {
            branch = 2;
        } else {
            rng2        = rng * 5 + 0x71357911;
            Gp_LcgState = rng2;
            hi          = rng2 >> 0x10;
            quotient    = hi / 100U;
            SCHED_BARRIER();
            roll = (u16)(hi - quotient * 100U);
            SCHED_BARRIER();
            hit    = (roll < 0xBU);
            branch = 2;
            if (!hit) {
                branch = 1;
            }
        }
        work->field_382 = branch;
        work->field_374 = 0;
        work->field_372 = 0;
    }
    Gp_ClearRec18Occupied(&work->field_214);
}
