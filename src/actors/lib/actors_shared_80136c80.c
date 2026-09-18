#include "common.h"

#include "actors/actors_shared_80136c80.h"
#include "gameplay/3A34.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

/// Per-frame tick of the specimen's movement cycle, gated by the mode in
/// `D_801153F4`: a mode of 1 returns at once, 2 hides the model part
/// (`field_C = 0x80`) and returns, and 0 zeroes that word and falls through
/// into the tick.
///
/// The tick folds the work's velocity onto the model - negated into the first
/// collision record's position, and added to the coordinate's own translation,
/// which is marked dirty - and walks `field_2` on by 0xA a frame. The second
/// collision record is then read as the hit test: a record of the 0x10000 kind
/// or any occupied slot cues the specimen's pan/depth sound, arming the child
/// task's `spawnArg1` to 3 - or, for the occupied-slot arm, to 3 or 2 picked by
/// whether that record sits at or above the floor cutoff -0xC00. Either arm
/// then trims the work's flag word to 0x3FFF, arms the task's kill countdown to
/// 0x1E and advances the state; both end by clearing the collision table.
///
/// The 2/3 pair is written into each arm rather than through a temp: the shared
/// store m2c reads as one variable is `jump.c` cross-jumping the two arms, and
/// a named temp puts the value's live range in front of the comparison that
/// picks it, where it can no longer share `$v0` with the `slti` result.
void ActorsShared80136c80(Task* arg0)
{
    ActorsShared80136c80Work* work;
    TmdObject*                part;
    Task*                     child;
    GsCOORDINATE2*            coord;
    GpRec18*                  rec;
    GpRec18*                  hit;
    GpRec18*                  recs;
    s32                       state;
    s32                       one;

    work  = (ActorsShared80136c80Work*)arg0->work;
    part  = (TmdObject*)arg0->extra;
    state = D_801153F4;
    child = arg0->firstChild;
    rec   = &work->recs[0];
    coord = part->coords;
    hit   = &work->recs[1];
    one   = 1;

    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    part->flags = 0;
    goto default_body;
case1:
    return;
case2:
    part->flags = 0x80;
    return;
default_body:
    rec->field_8      = -work->field_0;
    rec->field_A      = -work->field_2;
    rec->field_C      = -work->field_4;
    coord->coord.t[0] = coord->coord.t[0] + (s16)work->field_0;
    recs              = &work->recs[1];
    coord->coord.t[1] = coord->coord.t[1] + (s16)work->field_2;
    coord->coord.t[2] = coord->coord.t[2] + (s16)work->field_4;
    coord->flg        = 0;
    work->field_2     = work->field_2 + 0xA;
    if (Gp_CountRec18Hi(recs, 0x10000) != 0) {
        SndEvt_EnqueueType6(0x40460007, (s8)Gp_GetObjPan((GpObj38*)coord),
                            (s8)Gp_GetObjDepth((GpObj38*)coord));
        if (child != NULL) {
            child->spawnArg1 = 3;
        }
        goto block_16;
    }
    if (Gp_FindRec18(recs, 0) != 0) {
        SndEvt_EnqueueType6(0x40460007, (s8)Gp_GetObjPan((GpObj38*)coord),
                            (s8)Gp_GetObjDepth((GpObj38*)coord));
        if (child != NULL) {
            if (hit->field_12 >= -0xC00) {
                child->spawnArg1 = 3;
            } else {
                child->spawnArg1 = 2;
            }
        }
    block_16:
        work->field_26      = work->field_26 & 0x3FFF;
        arg0->killCountdown = 0x1E;
        arg0->state         = arg0->state + 1;
    }
    Gp_ClearRec18Occupied(&work->recs[1]);
}
