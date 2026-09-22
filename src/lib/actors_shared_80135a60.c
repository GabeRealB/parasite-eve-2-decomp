#include "common.h"

#include "actors/actors_shared_80135a60.h"
#include "main/gfx.h"
#include "main/mem.h"

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

/// Rebuilds `coord`'s Y rotation from its current yaw (`ratan2` of
/// `-m[2][0], m[2][2]`), uniformly scaled by `scale`, through a 0x34-byte
/// block borrowed from the scratchpad. Marks the coordinate dirty.
void ActorsShared80135a60(GsCOORDINATE2* coord, s16 scale)
{
    void**                      scratch;
    void*                       head;
    ActorShared80135a60Scratch* blk;
    s16                         ang;
    u16                         m22;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (ActorShared80135a60Scratch*)((u8*)head - 0x34);
    *scratch = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = scale;
    blk->scale.vy = scale;
    blk->scale.vx = scale;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] = *(u16*)&((ActorShared80135a60Scratch*)((u8*)head - 0x34))->m.m[0][0];
    coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
    m22                  = *(u16*)&blk->m.m[2][2];
    *scratch             = (u8*)*scratch + 0x34;
    coord->flg           = 0;
    coord->coord.m[2][2] = m22;
}
