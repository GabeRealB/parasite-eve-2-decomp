#include "common.h"
#include "actors/actors_shared_80133240.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "main/wipsys.h"

/// State-0 handler of `ActorsShared80134cfc`: counts `field_140` down,
/// re-arming it from `Gp_LcgState`, and wakes the actor (pose 1/2, unit scale,
/// arm state 1) when the player is within 1500 units on XZ or either global
/// trigger is set.
void ActorsShared80133240(ActorsShared80133240Actor* arg0)
{
    ActorsShared80133240Work* work;
    GsCOORDINATE2*            coord;
    s32                       flag;
    s32                       dx;
    s32                       dz;
    u32                       random;
    VECTOR*                   delta;
    VECTOR*                   scratchEnd;

    scratchEnd                         = *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC);
    delta                              = scratchEnd - 1;
    *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) = delta;
    work                               = arg0->field_1C;
    coord                              = arg0->field_2C->field_8;
    flag                               = 0;
    if (--work->field_140 < 0) {
        random          = (Gp_LcgState * 5) + 0x71357911;
        work->field_140 = (random >> 0x10) & 0xF;
        Gp_LcgState     = random;
        if (D_801153F2[0] & 2) {
            flag = 1;
        }
    }
    scratchEnd[-1].vx = (s32)(Player_Status.coordMtx->t[0] - coord->coord.t[0]);
    delta->vy         = 0;
    dz                = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    delta->vz         = dz;
    dx                = scratchEnd[-1].vx;
    if (SquareRoot0((dx * dx) + (dz * dz)) < 0x5DC) {
        flag = 1;
    }
    if (D_8011540B != 0) {
        flag = 1;
    }
    if (flag != 0) {
        work->field_13C = 1;
        work->field_13E = 2;
        work->field_128 = 0x1000;
        work->field_12A = 0x1000;
        work->field_12C = 0x1000;
        work->field_140 = 0;
        Gp_ArmStateF0(1);
    }
    *(VECTOR**)PSX_SCRATCH_ADDR(0x3FC) += 1;
}
