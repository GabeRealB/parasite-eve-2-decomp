#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

/// Teleport the actor's model: drop `pos` straight into the root part's coord
/// translation and invalidate the cached world matrix so `Gp_UpdateCoord`
/// rebuilds it. The part index is accepted but unused - the sibling helper
/// `func_actor_341700_80168370` is the one that offsets by a part.
void ActorsShared8016833c(Task* task, s16 part, VECTOR3* pos)
{
    GsCOORDINATE2* coord;

    coord             = ((TmdObject*)task->extra)->field_8;
    coord->coord.t[0] = pos->vx;
    coord->coord.t[1] = pos->vy;
    coord->coord.t[2] = pos->vz;
    coord->flg        = 0;
}
