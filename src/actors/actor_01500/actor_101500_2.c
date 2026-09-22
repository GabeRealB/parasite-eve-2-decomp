#include "common.h"
#include "actors/actor_101500.h"

/// Leaves the idle poses once `field_356` frames have run: state 0 switches to
/// pose 7 (8 when `field_36E` is set), state 1 to pose 5 with a random
/// `field_362` delay, state 2 to pose 14.
void ActorsShared801344f8_Fn33528(Actor101500* actor)
{
    Actor101500Work* work = actor->field_1C;
    s16              pose;
    u32              rnd;
    u16              val;
    u16*             tbl;

    switch (work->field_358) {
        case 0:
            pose = 7;
            if ((s16)work->field_356 >= 20) {
                work->field_35A = 0;
                if (work->field_36E != 0) {
                    pose = 8;
                }
                work->field_34C = 0x400F0002;
                work->field_352 = pose;
                work->field_35C = 0;
                work->field_380 = 15;
            }
            break;
        case 1:
            if ((s16)work->field_356 >= 10) {
                tbl             = ActorsShared80132ac4Durations;
                work->field_35A = 3;
                work->field_352 = 5;
                work->field_35C = 0;
                rnd             = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState     = rnd;
                val             = tbl[(rnd >> 16) & 0xF];
                work->field_34C = 0x400F0002;
                work->field_380 = 15;
                work->field_362 = val;
            }
            break;
        case 2:
            if ((s16)work->field_356 > 0) {
                work->field_35A = 5;
                work->field_362 = 0;
                work->field_352 = 14;
                work->field_34C = 0;
            }
            break;
    }
}
