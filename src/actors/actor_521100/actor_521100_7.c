#include "common.h"

#include "actors/actor_521100.h"
#include "actors/actors_shared_80132074.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

extern TaskDesc D_actor_521100_8016A388;
extern Task*    D_actor_521100_8016A3E0;
extern Task*    D_actor_521100_8016A3E4;

/// Message 0x7D4 handler in `D_actor_521100_8016A358`, placing the actor: only
/// the yaw of the argument block's angles is used, cached in the work block's
/// `field_48C.yaw` and applied with `Gfx_RotMatrixY`, then the position becomes
/// the root coordinate's translation and `flg` is cleared.
s32 func_actor_521100_80136A64(Task* task, s32 arg1, ActorsShared80132074Args* placement)
{
    GsCOORDINATE2* coord;
    u16            yaw;

    coord                                  = ((TmdObject*)task->extra)->coords;
    D_actor_521100_8016A3D8->field_48C.yaw = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

s32 func_actor_521100_80136AE0(Task* task, s32 arg1, Actor521100Msg* msg)
{
    switch (msg->field_2) {
        case 1:
            D_actor_521100_8016A3E0 = Task_SpawnFromTable(&D_actor_521100_8016A388, 1, 0, (s32)task);
            break;

        case 2:
            D_actor_521100_8016A3D8->field_484 = 0;
            task->state                        = 2;
            break;

        case 3:
            if (D_actor_521100_8016A3E0 != NULL) {
                taskKill(D_actor_521100_8016A3E0);
                D_actor_521100_8016A3E0 = NULL;
            }
            if (D_actor_521100_8016A3E4 != NULL) {
                taskKill(D_actor_521100_8016A3E4);
                D_actor_521100_8016A3E4 = NULL;
            }
            /* fall through -- the jump table's index 0 lands on the same store */

        case 0:
            task->state = 1;
            break;

        case 4:
            D_actor_521100_8016A3E4 = Task_SpawnFromTable(&D_actor_521100_8016A388, 1, 1, (s32)task);
            break;
    }
    return 0;
}
s32 func_actor_521100_80136BE8(Task* task, s32 arg1, Actor521100Target* target)
{
    GsCOORDINATE2* coord;
    s32            dx;
    s32            dz;
    u16            yaw;

    coord                                  = ((TmdObject*)task->extra)->coords;
    dx                                     = target->pos.vx - coord->coord.t[0];
    dz                                     = target->pos.vz - coord->coord.t[2];
    yaw                                    = ratan2(dx, dz);
    D_actor_521100_8016A3D8->field_48C.yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    D_actor_521100_8016A3D8->field_48C.travel = SquareRoot0(dx * dx + dz * dz) / 20;
    return 0;
}
