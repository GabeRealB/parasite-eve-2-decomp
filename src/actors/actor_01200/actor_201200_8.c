#include "common.h"
#include "actors/actor_201200.h"
#include "main/gfx.h"
#include "main/mem.h"

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

/// Rebuilds `coord`'s rotation as its current yaw alone, uniformly scaled by
/// `scale`, working in a block borrowed from the scratch stack, and marks the
/// coordinate dirty.
void Actor01200_Fn03C40(GsCOORDINATE2* coord, s16 scale)
{
    void**                  scratch;
    void*                   head;
    Actor201200FaceScratch* blk;
    s16                     ang;
    u16                     m22;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (Actor201200FaceScratch*)head - 1;
    *scratch = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = scale;
    blk->scale.vy = scale;
    blk->scale.vx = scale;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] = *(u16*)&((Actor201200FaceScratch*)head - 1)->m.m[0][0];
    coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
    m22                  = *(u16*)&blk->m.m[2][2];
    *scratch             = (u8*)*scratch + sizeof(Actor201200FaceScratch);
    coord->flg           = 0;
    coord->coord.m[2][2] = m22;
}
