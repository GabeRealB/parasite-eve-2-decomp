#include "common.h"

#include "actors/actor_141000.h"

#include "main/task.h"
#include "main/tmd.h"

/// State-1 handler of the actor's main-body table `D_actor_141000_80131E58`,
/// the step after the shared turn-to-face body. Rotates the constant
/// local-space offset `D_actor_141000_80131E68` through the root part's matrix
/// into `work->step`, halving it first while `field_4C8` is clear -- the
/// variant `func_actor_141000_80133F6C` latches through message 0x7DB -- then
/// opens the per-axis stop threshold to 0x7FFF, which disables it for the
/// update loop, and advances the state.
void func_actor_141000_80133B28(Task* arg0)
{
    Actor141000Work* work;
    GsCOORDINATE2*   coord;
    VECTOR           vec;

    coord = ((TmdObject*)arg0->extra)->field_8;
    work  = (Actor141000Work*)arg0->idMap;

    vec = D_actor_141000_80131E68;
    if (work->field_4C8 == 0) {
        vec.vx >>= 1;
        vec.vy >>= 1;
        vec.vz >>= 1;
    }
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->step);
    work->limit.vx  = 0x7FFF;
    work->limit.vy  = 0x7FFF;
    work->limit.vz  = 0x7FFF;
    work->field_4C2 = work->field_4C2 + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_4", func_actor_141000_80133BD8);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_4", func_actor_141000_80133CD8);
