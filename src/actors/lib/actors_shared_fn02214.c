#include "common.h"
#include "actors/actor_105500.h"

#define SCRATCH_SP (*(u32*)0x1F8003FC)

void ActorsSharedFn02214(Actor105500* arg0)
{
    Actor105500Work*       work;
    GsCOORDINATE2*         coord;
    Actor105500RotScratch* sc;
    s32                    ang;
    u16                    want;
    s16                    diff;
    s32                    adiff;
    s32                    step;
    s32                    cur;
    s32                    next;
    s32                    wrapStep;

    sc    = (Actor105500RotScratch*)(SCRATCH_SP -= 0x18);
    coord = arg0->field_2C->field_8;
    work  = arg0->field_1C;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->field_3A4;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_3A2 = ang;
    if (adiff < 0x800) {
        step = work->field_3A6;
        if (step >= adiff) {
            work->field_3A2 = want;
        } else {
            next = work->field_3A2;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->field_3A2 = next;
        }
    } else {
        step = work->field_3A6;
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
        work->field_3A2 = work->field_3A4;
        goto done;
    turn:
        wrapStep = work->field_3A6;
        cur      = work->field_3A2;
        if (diff > 0) {
            work->field_3A2 = cur - wrapStep;
        } else {
            work->field_3A2 = cur + wrapStep;
        }
    }
done:
    sc->rot.vx = 0;
    sc->rot.vy = work->field_3A2;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_SP += 0x18;
}

#undef SCRATCH_SP
