#include "common.h"
#include "actors/actor_102400_fn0208c.h"

extern u32 Gp_LcgState;

/// Shrinks the work block's per-axis scale toward its floor (0x1000 / 0x600 /
/// 0x1000), sets the `field_130` task to state 4 and releases it, and counts
/// `field_140` up with bit 15 of `field_DE` cleared. Past 360 frames it resets
/// `field_13C` / `field_13E`, re-arms the counter at a random 30..61 and sets
/// the bit again.
///
/// Shared by `actor_102400` and `actor_202400`.
void ActorsShared80133d94(Actor02400Scale* arg0)
{
    Actor02400ScaleWork* work = arg0->field_1C;

    work->field_128 -= 0x40;
    if (work->field_128 < 0x1000) {
        work->field_128 = 0x1000;
    }
    work->field_12A -= 0x80;
    if (work->field_12A < 0x600) {
        work->field_12A = 0x600;
    }
    work->field_12C -= 0x40;
    if (work->field_12C < 0x1000) {
        work->field_12C = 0x1000;
    }
    if (work->field_130 != NULL) {
        (*work->field_130)->state = 4;
        work->field_130           = NULL;
    }
    work->field_DE &= 0x7FFF;
    work->field_140++;
    if (work->field_140 > 0x168) {
        work->field_13C = 1;
        work->field_13E = 0;
        Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
        work->field_140 = ((Gp_LcgState >> 16) & 0x1F) + 0x1E;
        work->field_DE |= 0x8000;
    }
}
