#include "common.h"

#include "actors/actor_107000.h"
#include "actors/actors_shared_80136c80.h"
#include "gameplay/3A34.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

/// Per-frame handler of a specimen projectile, entry 1 of
/// `Actor07000_D000E0`. `D_801153F4` mode 1 returns at once and mode 2 hides
/// the model; mode 0 shows it again before the update. The update moves the
/// coordinate by the velocity in the work (mirrored into the first collision
/// record's position), lets the vertical speed grow by 0xA a frame, and tests
/// the second collision record: a hit on an object of the 0x10000 kind or on
/// any occupied slot cues the impact sound, tells the child task how it landed
/// through `spawnArg1` (3, or 2 for a slot hit below -0xC00 in the normal's Y),
/// clears the top bits of the flag word, arms a 0x1E-frame kill countdown and
/// moves on to `Actor07000_Fn068B4`. The collision table is cleared either way.
///
/// The 2/3 pair is written into each arm rather than through a temp: the shared
/// store m2c reads as one variable is `jump.c` cross-jumping the two arms, and
/// a named temp puts the value's live range in front of the comparison that
/// picks it, where it can no longer share `$v0` with the `slti` result.
void Actor07000_Fn04E60(Task* arg0)
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
    rec->point.vx     = -work->field_0;
    rec->point.vy     = -work->field_2;
    rec->point.vz     = -work->field_4;
    coord->coord.t[0] = coord->coord.t[0] + (s16)work->field_0;
    recs              = &work->recs[1];
    coord->coord.t[1] = coord->coord.t[1] + (s16)work->field_2;
    coord->coord.t[2] = coord->coord.t[2] + (s16)work->field_4;
    coord->flg        = 0;
    work->field_2     = work->field_2 + 0xA;
    if (Gp_CountRec18Hi(recs, 0x10000) != 0) {
        SndEvt_EnqueueType6(0x40460007, (s8)Gp_GetObjPan(coord),
                            (s8)gpGetObjDepth(coord));
        if (child != NULL) {
            child->spawnArg1 = 3;
        }
        goto block_16;
    }
    if (Gp_FindRec18(recs, 0) != 0) {
        SndEvt_EnqueueType6(0x40460007, (s8)Gp_GetObjPan(coord),
                            (s8)gpGetObjDepth(coord));
        if (child != NULL) {
            if (hit->at10.normal.vy >= -0xC00) {
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
