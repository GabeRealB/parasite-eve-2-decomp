#include "common.h"
#include "actors/actor_101500.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/tmd.h"

/// Once the pose has run 30 frames, re-aims `field_374` along the coordinate's
/// facing and walks the actor 40 units back along it (state 1 also rises, faster
/// early on); from frame 59 it queues pose 5 with two `Gp_LcgState` draws.
void ActorsShared80132de4(Actor101500* actor)
{
    Actor101500Work* work;
    GsCOORDINATE2*   coord;
    s16              angle;
    u32              rnd;
    u32              rnd2;

    work  = actor->field_1C;
    coord = actor->field_2C->field_8;
    if ((s16)work->field_356 >= 0x1E) {
        work->field_374 = angle = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
        switch (work->field_36E) {
            case 0:
                coord->coord.t[0] += -(rsin(angle) * 40) >> 12;
                coord->coord.t[2] += -(rcos(work->field_374) * 40) >> 12;
                break;
            case 1:
                coord->coord.t[0] += -(rsin(angle) * 40) >> 12;
                coord->coord.t[2] += -(rcos(work->field_374) * 40) >> 12;
                if ((s16)work->field_356 < 0x24) {
                    coord->coord.t[1] += 0x6E;
                } else if ((s16)work->field_356 < 0x2E) {
                    coord->coord.t[1] += 0x23;
                } else {
                    coord->coord.t[1] += 0xF;
                }
                break;
        }
        if ((s16)work->field_356 >= 0x3B) {
            rnd             = Gp_LcgState * 5 + 0x71357911;
            work->field_35A = 3;
            work->field_352 = 5;
            work->field_35C = 0;
            work->field_34C = 0x400F0002;
            work->field_380 = 0xF;
            Gp_LcgState     = rnd;
            work->field_362 = ((rnd >> 16) & 0x3F) + 0x3C;
            rnd2            = rnd * 5 + 0x71357911;
            Gp_LcgState     = rnd2;
            work->field_364 = (rnd2 >> 16) & 0x1FF;
        }
    }
}
