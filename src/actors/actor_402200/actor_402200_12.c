#include "common.h"

#include "actors/actor_402200.h"

/// Sequence 8, the low-HP turn: state 0 picks animation 0xE (and state 1) when
/// `field_6F0` is 1, otherwise 0x12 and state 2; states 1 and 2 wait for the
/// frame counter to reach 0x10 or 0x16, then switch to animation 0x10 or 0x14,
/// enter sequence 7 at state 3 and arm the `field_6D4` countdown from the
/// `Gp_LcgState` LCG (0..0x3F).
void func_actor_402200_8013592C(Actor402200* arg0)
{
    Actor402200Work* work;
    s16              state;
    s32              next;

    work  = arg0->field_1C;
    state = work->field_6CE;
    switch (state) {
        case 0:
            next = work->field_6F0;
            if (next == 1) {
                work->field_6C0 = 0xE;
                work->field_6CE = next;
            } else {
                work->field_6C0 = 0x12;
                work->field_6CE = 2;
            }
            break;
        case 1:
            if (work->field_6C4 >= 0x10) {
                work->field_6C0 = 0x10;
                work->field_6CC = 7;
                work->field_6CE = 3;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_6D4 = ((u32)Gp_LcgState >> 16) & 0x3F;
            }
            break;
        case 2:
            if (work->field_6C4 >= 0x16) {
                work->field_6C0 = 0x14;
                work->field_6CC = 7;
                work->field_6CE = 3;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_6D4 = ((u32)Gp_LcgState >> 16) & 0x3F;
            }
            break;
    }
}
