#include "common.h"

#include "actors/actor_521100.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

extern TaskDesc D_actor_521100_8016A388;
extern Task*    D_actor_521100_8016A3E0;
extern Task*    D_actor_521100_8016A3E4;

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_7", func_actor_521100_80136A64);

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

    coord                                      = ((TmdObject*)task->extra)->coords;
    dx                                         = target->pos.vx - coord->coord.t[0];
    dz                                         = target->pos.vz - coord->coord.t[2];
    yaw                                        = ratan2(dx, dz);
    D_actor_521100_8016A3D8->field_48C.rot.yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    D_actor_521100_8016A3D8->field_48C.rot.travel = SquareRoot0(dx * dx + dz * dz) / 20;
    return 0;
}
