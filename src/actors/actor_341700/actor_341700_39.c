#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"

#include "actors/actor_341700.h"
#include "actors/actors_shared_8016bd98.h"

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

/// Death shrink: restores the root matrix saved in `savedRootMtx`, scales it
/// on Y by `field_430` (0x40 smaller each frame), spawns effect 0x600A5 on
/// frame 4, sets the enemy's light mode 2 on frame 16, and after frame 32
/// hides the model and advances the state.
void func_actor_341700_80164E9C(Task* arg0)
{
    Actor341700Work*              work;
    TmdObject*                    obj;
    GsCOORDINATE2*                coord;
    VECTOR                        scale;
    ActorsShared8016bd98Mat       m;
    ActorsShared8016bd98MatWords* ident;
    SVECTOR                       ofs;

    work             = (Actor341700Work*)arg0->work;
    ident            = &m.ident;
    obj              = arg0->extra;
    coord            = obj->coords;
    work->field_430 -= 0x40;
    scale.vx         = 0x1000;
    scale.vy         = (s16)work->field_430;
    scale.vz         = 0x1000;
    coord->coord     = work->savedRootMtx;
    m.ident.m00_m01  = 0x1000;
    m.ident.m02_m10  = 0;
    ident->m11_m12   = 0x1000;
    m.ident.m20_m21  = 0;
    ident->m22       = 0x1000;
    ScaleMatrix(&m.mat, &scale);
    MulMatrix(&coord->coord, &m.mat);
    if ((s16)++work->field_412 == 4) {
        ofs.vx = 0;
        ofs.vy = 0;
        ofs.vz = 0;
        Gp_SpawnEff(0x600A5, coord, 3, &ofs);
    }
    if ((s16)work->field_412 == 0x10) {
        Gp_SetLightMode(arg0->spawnArg2, 2);
    }
    if ((s16)work->field_412 > 0x20) {
        obj->flags |= 0x80;
        work->field_420++;
    }
}

/// Aims at the nearer of the two player actors: saves the root position in
/// `field_60`, stores the offset to that actor in `field_88`..`field_8C` and
/// its horizontal distance in `field_43A`, and its heading relative to
/// `field_7A` in `field_444`. Nothing but the position is updated while
/// player slot 0 is empty.
void func_actor_341700_80165008(Task* arg0)
{
    Actor341700Work* work;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   other;
    GpActorWork*     player;
    SVECTOR          d0;
    SVECTOR          d1;
    s32              dist;
    s32              dist2;

    work              = (Actor341700Work*)arg0->work;
    coord             = ((TmdObject*)arg0->extra)->coords;
    player            = Gp_ActorSlots[0];
    work->field_60.vx = coord->coord.t[0];
    work->field_60.vy = coord->coord.t[1];
    work->field_60.vz = coord->coord.t[2];
    if (player != NULL) {
        other = player->extra->coords;
        d0.vx = other->coord.t[0] - coord->coord.t[0];
        d0.vy = other->coord.t[1] - coord->coord.t[1];
        d0.vz = other->coord.t[2] - coord->coord.t[2];
        dist  = SquareRoot0(d0.vx * d0.vx + d0.vz * d0.vz);
        if (Gp_ActorSlots[1] != NULL) {
            other = Gp_ActorSlots[1]->extra->coords;
            d1.vx = other->coord.t[0] - coord->coord.t[0];
            d1.vy = other->coord.t[1] - coord->coord.t[1];
            d1.vz = other->coord.t[2] - coord->coord.t[2];
            dist2 = SquareRoot0(d1.vx * d1.vx + d1.vz * d1.vz);
            if (dist2 < dist) {
                dist  = dist2;
                d0.vx = d1.vx;
                d0.vy = d1.vy;
                d0.vz = d1.vz;
            }
        }
        // The loop notes keep VectorNormalSS's argument setup below these stores.
        do {
            work->field_88  = d0.vx;
            work->field_8A  = d0.vy;
            work->field_8C  = d0.vz;
            work->field_43A = dist;
        } while (0);
        VectorNormalSS(&d0, &d0);
        work->field_444 = (ratan2(d0.vx, d0.vz) - work->field_7A) & 0xFFF;
    }
}
