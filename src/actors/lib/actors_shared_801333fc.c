#include "common.h"

#include "actors/actor_101500.h"
#include "main/mem.h"
#include "main/wipsys.h"

/// Faces the actor toward the player on the XZ plane and raises `field_378`
/// once the player leaves the vertical band (500 above, 1800 below) or
/// `field_362` counts past 1800 frames.
void ActorsShared801333fc(Actor101500* actor)
{
    Actor101500Work* work;
    GsCOORDINATE2*   coord;
    VECTOR*          head;
    VECTOR*          blk;

    work                      = actor->field_1C;
    coord                     = actor->field_2C->field_8;
    work->field_352           = 0xE;
    work->field_360           = 5;
    work->field_376           = 5;
    work->field_34C           = 0;
    work->field_366           = 0x80;
    head                      = *(VECTOR**)G_SCRATCH_HEAD;
    blk                       = head - 1;
    head[-1].vx               = coord->coord.t[0] - Player_Status.coordMtx->t[0];
    blk->vy                   = 0;
    blk->vz                   = coord->coord.t[2] - Player_Status.coordMtx->t[2];
    *(VECTOR**)G_SCRATCH_HEAD = blk;
    work->field_372           = ratan2((s16)head[-1].vx, (s16)blk->vz) & 0xFFF;
    if (coord->coord.t[1] > Player_Status.coordMtx->t[1] + 500 ||
        coord->coord.t[1] < Player_Status.coordMtx->t[1] - 1800 ||
        ++work->field_362 > 1800) {
        work->field_378 = 1;
    }
    *(VECTOR**)G_SCRATCH_HEAD += 1;
}
