#include "common.h"
#include "actors/actor_101500.h"
#include "gameplay/3A34.h"

/// Counts `field_362` up, raising the state-F0 flags at frame 60; from frame 90
/// it switches to pose 7 (8 when `field_36E` is set) and reloads the counter
/// with a random delay.
void ActorsShared801344f8_Fn345D0(Actor101500* actor)
{
    Actor101500Work* work = actor->field_1C;
    s16              pose;
    u32              rnd;
    u16              val;

    if (++work->field_362 == 60) {
        Gp_SetStateF0Byte3(1);
        Gp_SetStateF0Bit(1);
    }
    pose = 7;
    if (work->field_362 >= 90) {
        work->field_35A = 2;
        if (work->field_36E != 0) {
            pose = 8;
        }
        work->field_352 = pose;
        rnd             = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState     = rnd;
        val             = ActorsShared80132ac4Durations[(rnd >> 16) & 0xF];
        work->field_358 = 1;
        work->field_37A = 1;
        work->field_34C = 0x400F0002;
        work->field_380 = 15;
        work->field_362 = val;
    }
}
