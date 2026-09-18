#include "common.h"

#include "actors/actor_335800.h"

#include "gameplay/3CD8.h"

#include "main/session.h"

#include "main/task.h"

#include "main/tmd.h"

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

/// Places the root part 1000 units short of its pose `D_actor_335800_80164F80`
/// and moves it along Z at `killCountdown` (100) per frame; once past the pose
/// height the velocity drops by 6 a frame until it falls below -60, which ends
/// the move. A ground shadow is drawn every frame. The
/// task kills itself once the session's `viewReady` flag is set, or a few
/// frames into state 2.
void func_actor_335800_80161E88(Task* task)
{
    Actor335800Coord* coord;
    VECTOR3           pos;
    SVECTOR*          rot;

    coord = (Actor335800Coord*)((TmdObject*)task->extra)->coords;
    switch (task->state) {
        case 0:
            coord->coord.t[0] = D_actor_335800_80164F80.x;
            coord->coord.t[1] = D_actor_335800_80164F80.y;
            coord->coord.t[2] = D_actor_335800_80164F80.z - 1000;
            rot               = &D_actor_335800_80164F80.rot;
            coord->rot.vx     = rot->vx;
            coord->rot.vy     = rot->vy;
            coord->rot.vz     = rot->vz;
            RotMatrix(&coord->rot, &coord->coord);
            coord->flg          = 0;
            task->killCountdown = 100;
            task->state++;
        case 1:
            if (D_actor_335800_80164F88 < coord->coord.t[2]) {
                task->killCountdown -= 6;
                if (task->killCountdown < -60) {
                    task->killCountdown = 0;
                    task->state++;
                }
            }
            coord->coord.t[2] += task->killCountdown;
            if (gGameSession->viewReady != 0) {
                Task_Kill(task);
            }
            break;
        case 2:
            if (++task->killCountdown < 4) {
                if (gGameSession->viewReady != 0) {
                    Task_Kill(task);
                }
            } else {
                Task_Kill(task);
            }
            break;
        default:
            Task_Kill(task);
            break;
    }
    if (func_800EA1A8((VECTOR3*)((TmdObject*)task->extra)->coords->workm.t, &pos) != 0) {
        Gp_DrawEffGroundQuad(&pos, 0x800, Gp_State1C->field_8);
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_335800/actor_335800", D_actor_335800_80161E20);
