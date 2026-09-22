#include "common.h"

#include "actors/actors_shared_80134084.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// Turns the actor towards `field_146` by at most `field_13A` per frame and
/// rebuilds the part coordinate's matrix from the result, staging the angle in
/// a scratchpad `SVECTOR`. The wanted yaw wraps at 0x1000: when the remaining
/// turn would overshoot through the wrap the actor snaps to it instead.
void ActorsShared80134084(ActorShared80134084* arg0)
{
    ActorShared80134084Work*       work;
    GsCOORDINATE2*                 coord;
    ActorShared80134084RotScratch* sc;
    s32                            ang;
    u16                            want;
    s16                            diff;
    s32                            adiff;
    s32                            step;
    s32                            cur;
    s32                            next;
    s32                            wrapStep;

    sc    = (ActorShared80134084RotScratch*)(SCRATCH_SP -= 0x18);
    coord = arg0->field_2C->field_8;
    work  = arg0->field_1C;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->field_146;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_144 = ang;
    if (adiff < 0x800) {
        step = work->field_13A;
        if (step >= adiff) {
            work->field_144 = want;
        } else {
            next = work->field_144;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->field_144 = next;
        }
    } else {
        step = work->field_13A;
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
        work->field_144 = work->field_146;
        goto done;
    turn:
        wrapStep = work->field_13A;
        cur      = work->field_144;
        if (diff > 0) {
            work->field_144 = cur - wrapStep;
        } else {
            work->field_144 = cur + wrapStep;
        }
    }
done:
    sc->rot.vx = 0;
    sc->rot.vy = work->field_144;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_SP += 0x18;
}
