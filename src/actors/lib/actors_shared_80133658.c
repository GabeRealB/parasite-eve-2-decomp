#include "common.h"

#include "actors/actor_101500.h"
#include "main/tmd.h"

#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// Turns the actor toward `field_372` by at most `field_376` per call, taking
/// the short way round the 0x1000 circle, then rebuilds its rotation matrix.
void ActorsShared80133658(Actor101500* arg0)
{
    Actor101500Work*       work;
    GsCOORDINATE2*         coord;
    Actor101500RotScratch* sc;
    s32                    ang;
    u16                    want;
    s16                    diff;
    s32                    adiff;
    s32                    step;
    s32                    cur;
    s32                    next;
    s32                    wrapStep;

    sc    = (Actor101500RotScratch*)(SCRATCH_SP -= 0x18);
    coord = arg0->field_2C->field_8;
    work  = arg0->field_1C;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->field_372;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_374 = ang;
    if (adiff < 0x800) {
        step = work->field_376;
        if (step >= adiff) {
            work->field_374 = want;
        } else {
            next = work->field_374;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->field_374 = next;
        }
    } else {
        step = work->field_376;
        if (diff > 0) {
            if (step >= 0x1000 - diff) {
                goto snap;
            } else {
                goto turn;
            }
        } else if (step >= 0x1000 + diff) {
            goto snap;
        } else {
            goto turn;
        }
    snap:
        work->field_374 = work->field_372;
        goto done;
    turn:
        wrapStep = work->field_376;
        cur      = work->field_374;
        if (diff > 0) {
            work->field_374 = cur - wrapStep;
        } else {
            work->field_374 = cur + wrapStep;
        }
    }
done:
    sc->rot.vx = 0;
    sc->rot.vy = work->field_374;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_SP += 0x18;
}
