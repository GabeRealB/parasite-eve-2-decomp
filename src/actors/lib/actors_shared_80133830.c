#include "common.h"
#include "actors/actors_shared_80133830.h"

/// Two-step sub-state machine for the actor: sub-state 0 raises `field_134`
/// for up to 7 frames (cut short by `field_14C`), sub-state 1 lowers it and,
/// after 7 frames, either resets on `field_14C` or re-arms with a random
/// 30-61 frame wait. Every frame `field_12A` oscillates between 0xF00 and
/// 0x1100.
///
/// Shared by `actor_102400` and `actor_202400`.
void ActorsShared80133830(ActorsShared80133830Actor* arg0)
{
    ActorsShared80133830Work* work;
    s32                       state;

    work  = arg0->field_1C;
    state = work->field_13E;
    switch (state) {
        case 0:
            work->field_134 = 1;
            work->field_140++;
            if (work->field_140 >= 7) {
                work->field_13E = 1;
                work->field_140 = 0;
            }
            if (work->field_14C != 0) {
                work->field_13E = 1;
                work->field_140 = 0;
            }
            break;
        case 1:
            work->field_134 = 0;
            work->field_140++;
            if (work->field_140 >= 7) {
                if (work->field_14C != 0) {
                    work->field_14C = 0;
                    work->field_13C = 3;
                    work->field_13E = 0;
                    work->field_140 = 0;
                    work->field_142 = 0;
                    work->field_128 = work->field_12A;
                    work->field_12C = work->field_12A;
                } else {
                    work->field_13C = state;
                    work->field_13E = 0;
                    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                    work->field_140 = ((Gp_LcgState >> 16) & 0x1F) + 0x1E;
                    work->field_DE |= 0x8000;
                }
            }
            break;
    }
    if (work->field_142 == 0) {
        work->field_12A += 0x80;
        if (work->field_12A > 0x1100) {
            work->field_142 = 1;
        }
    } else {
        work->field_12A -= 0x80;
        if (work->field_12A < 0xF00) {
            work->field_142 = 0;
        }
    }
}
