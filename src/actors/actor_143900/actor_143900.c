#include "common.h"

#include "actors/actor_143900.h"
#include "actors/actor_461800_move.h"
#include "actors/actors_shared_801326b4.h"
#include "actors/actors_shared_801366fc.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

/// Spawn table this overlay hands to `Task_SpawnFromTable`. It sits at an
/// absolute address outside the actor slot - offset 0x440 into the loaded room
/// overlay, whose base is 0x8017D5C0 - so splat cannot name it and it keeps its
/// raw `D_` form, as `D_80147E48` does in `actor_206100`. `shelter_r49` spawns
/// from a table at this same address (`D_shelter_r49_8017DA00`).
extern TaskDesc D_8017DA00;

/// Arms the `field_5C5` scene event and starts the table's task, unless
/// `field_23` is 9 - the `Task_Spawn` bank the attract-demo prompts
/// (`Gp_StrDemoWait` / `Gp_StrDemoPause`) key off, so this story trigger is
/// skipped while the demo plays. Same shape as `func_actor_450800_80132080`.
void func_actor_143900_80131E24(void)
{
    if (Mc_SaveData.field_23 != 9) {
        Mc_SaveData.field_5C5 = 0x14;
        Task_SpawnFromTable(&D_8017DA00, 0, 0, 0);
    }
}

/// State 0 of the `ActorsShared80131f9c` dispatcher: allocate the work block,
/// publish it in `ActorsShared80131f9cWork` and on the task's 0x1C slot, point
/// the model's light and color matrices and its animation context at it, the
/// same way the step-1 handler is handed its slot array.
///
/// Every access to the block goes through `ActorsShared80131f9cWork` rather
/// than the `Mem_Calloc` result, which is why the pointer is reloaded at each
/// use instead of staying in a callee-saved register; the same two loads
/// publish the block's matrices, which go to the object's `field_1C` /
/// `field_20`. `task->msgTable` takes the message table
/// `D_actor_143900_801413BC`.
///
/// The position it forwards to `func_800D7A9C` is the model root's translation
/// with its Y dropped by 0x320 - the standing height the light solve is cast
/// from, the ground offset every carrier of this body applies; the other two
/// components are the raw ones.
void ActorsShared80131f9cSub0(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor143900Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;

    obj                      = task->extra;
    coord                    = obj->coords;
    work                     = Mem_Calloc(0x4F0, 0);
    ActorsShared80131f9cWork = work;
    task->work               = (TaskIdMap*)work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback       = ActorsShared801366fc;
    coord->sub               = &gGfxViewCoord;
    enemy->field_4           = &coord->coord;
    enemy->field_48          = 0;
    enemy->node.field_5      = 0;
    enemy->node.field_4      = 1;
    obj->otOffset            = 1;
    obj->flags               = 0;
    obj->lightMtx            = &ActorsShared80131f9cWork->light;
    obj->colorMtx            = &ActorsShared80131f9cWork->color;
    vec.vx                   = coord->workm.t[0];
    vec.vy                   = coord->workm.t[1] - 0x320;
    ActorsShared801326b4Task = task;
    vec.vz                   = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&ActorsShared80131f9cWork->anim, D_actor_143900_801413F8, (GpAnimObj*)obj,
                  &ActorsShared80131f9cWork->pad_374, ActorsShared80131f9cWork->slots);
    ActorsShared80131f9cWork->field_4B8 = 1;
    ActorsShared80131f9cWork->field_4B4 = 2;
    ActorsShared80131f9cWork->field_4EA = 0;
    ActorsShared80131f9cWork->field_4EC = 0;
    task->msgTable                      = D_actor_143900_801413BC;
    func_actor_143900_80131FD4(task);
    task->state += 1;
}

void ActorsShared80132538(void);

extern s16 D_actor_143900_801413B8;
extern s16 D_actor_143900_801496C0;

/// Per-frame update of the shared body: modes 1 and 2 run their one-shot
/// reseed and switch to mode 3; mode 3 walks the model while `field_4EA`
/// counts down (distance picked by `D_actor_143900_801496C0`), turns it while
/// `field_4EC` counts down in animation 3, then ticks the animation. Same body
/// as `func_actor_461800_80132660`.
void func_actor_143900_80131FD4(Task* task)
{
    GsCOORDINATE2*   coord = ((TmdObject*)task->extra)->coords;
    Actor143900Work* work  = (Actor143900Work*)task->work;

    if (ActorsShared80131f9cWork->field_4B4 == 1) {
        func_actor_143900_801325A4();
        ActorsShared80131f9cWork->field_4B4 = 3;
    } else if (ActorsShared80131f9cWork->field_4B4 == 2) {
        ActorsShared80132538();
        ActorsShared80131f9cWork->field_4B4 = 3;
    } else if (ActorsShared80131f9cWork->field_4B4 == 3) {
        if (work->field_4B8 == 0xE || work->field_4B8 == 2 || work->field_4B8 == 0xF) {
            if (work->field_4EA != 0) {
                switch (D_actor_143900_801496C0) {
                    case 0:
                        Actor461800_MoveForward(task, 0x3C);
                        break;
                    case 1:
                        Actor461800_MoveForward(task, -0xF);
                        break;
                    case 2:
                        Actor461800_MoveForward(task, 0x19);
                        break;
                }
                if (--work->field_4EA == 0) {
                    work->field_4B4         = 1;
                    D_actor_143900_801413B8 = 10;
                    work->field_4B8         = 0xD;
                }
            }
        }
        if (work->field_4B8 == 3 && work->field_4EC != 0) {
            work->yaw += 0x33;
            Gfx_RotMatrixY(&coord->coord, (s16)work->yaw, 1);
            coord->flg = 0;
            work->field_4EC--;
        }
        func_actor_143900_801324C8();
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_143900/actor_143900", D_actor_143900_80131E20);
