#include "common.h"

#include "actors/actors_shared_80133d68.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// Turns the attach coordinate towards the target position in the work block:
/// `field_244` is the heading to the target, and `field_246` steps from the
/// matrix's current heading towards it by at most `field_254`, taking the short
/// way round. The result is written back as a Y rotation.
///
/// Carried by two actor slots - `actor_103700` and `actor_203700`; the shared
/// span is in `configs/USA/overlays.toml`.
void ActorsShared80133d68(ActorShared80133d68* arg0)
{
    ActorShared80133d68Work* work;
    GsCOORDINATE2*           coord;
    SVECTOR*                 rot;
    u16                      want;
    s16                      ang;
    s16                      diff;
    s32                      adiff;
    s32                      step;
    s32                      cur;
    s32                      next;
    s32                      wrapStep;

    coord           = arg0->field_2C->field_8;
    work            = arg0->field_1C;
    rot             = (SVECTOR*)(SCRATCH_SP -= 8);
    rot->vx         = work->field_23C - coord->coord.t[0];
    rot->vy         = 0;
    rot->vz         = work->field_240 - coord->coord.t[2];
    work->field_244 = ratan2(rot->vx, rot->vz) & 0xFFF;
    ang             = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]);
    want            = work->field_244;
    ang            &= 0xFFF;
    diff            = want - ang;
    adiff           = diff >= 0 ? diff : -diff;

    work->field_246 = ang;
    if (adiff < 0x800) {
        step = work->field_254;
        if (step >= adiff) {
            work->field_246 = want;
        } else {
            next = ang;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            goto store;
        }
    } else {
        step = work->field_254;
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
        work->field_246 = work->field_244;
        goto done;
    turn:
        wrapStep = work->field_254;
        cur      = work->field_246;
        if (diff > 0) {
            next = cur - wrapStep;
        } else {
            next = cur + wrapStep;
        }
    store:
        work->field_246 = next;
    }
done:
    rot->vx = 0;
    rot->vy = work->field_246;
    rot->vz = 0;
    RotMatrix(rot, &coord->coord);
    SCRATCH_SP += 8;
}
