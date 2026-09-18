#include "common.h"

#include "actors/actors_shared_80133cd0.h"
#include "gameplay/3A34.h"
#include "main/mem.h"
#include "main/tmd.h"

#define SCRATCH_SP (*(u32*)G_SCRATCH_HEAD)

/// The specimen's slide step, the twin of the enemy walk in
/// `ActorsShared8013777c`: node 2's collision table is run through
/// `func_800E0C10` with a 0x48-byte scratch, and the response's mode decides
/// what the step does. The two are one body per carrier - the walk reaches a
/// larger work block at 0x24C and wipes a second table, this one reads the
/// table at 0x154 and the root motion `ActorsShared8013454c` saved.
///
/// Mode 1 is the sliding one. Only the first slide arms it - `field_2E0`
/// latches the mode, `field_2DE` is re-armed to -0x64 and the step length
/// `field_2BE` sheds a quarter of itself - and the response's Y translation is
/// added onto the model coordinate inside that guard, while X and Z are added
/// either way. Mode 2 is a snap back that writes the saved root translation
/// instead. Either way the collision table is wiped before the scratch is
/// handed back.
///
/// Carried by four slots - `actor_104600`, `actor_107000`, `actor_204600` and
/// `actor_207000` - which all reach the block through `Task::work`, so the
/// body takes the `Task` rather than any carrier's own context type; the
/// shared span is in `configs/USA/overlays.toml`.
void ActorsShared80133cd0(Task* arg0)
{
    ActorsShared80133cd0Scratch* scratch;
    ActorsShared80133cd0Work*    work;
    GsCOORDINATE2*               coord;
    s32                          movement;

    work     = (ActorsShared80133cd0Work*)arg0->work;
    scratch  = (ActorsShared80133cd0Scratch*)(SCRATCH_SP -= 0x48);
    coord    = ((TmdObject*)arg0->extra)->coords;
    movement = func_800E0C10(&work->field_154[0], &scratch->delta, 4, NULL);
    switch (movement) {
        case 0:
            break;
        case 1:
            if (work->field_2E0 == 0) {
                coord->coord.t[1] += scratch->delta.vy.h.hi;
                work->field_2DE    = -0x64;
                work->field_2BE    = (u16)work->field_2BE - work->field_2BE / 4;
                work->field_2E0    = movement;
            }
            coord->coord.t[0] += scratch->delta.vx.h.hi;
            coord->coord.t[2] += scratch->delta.vz.h.hi;
            break;
        case 2:
            coord->coord.t[0] = work->field_274;
            coord->coord.t[1] = work->field_278;
            coord->coord.t[2] = work->field_27C;
            break;
    }
    Gp_ClearRec18Occupied(&work->field_154[0]);
    SCRATCH_SP += 0x48;
}
