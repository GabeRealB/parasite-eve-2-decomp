#include "common.h"

#include "actors/actor_143900.h"
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

/// Message table this handler publishes as the task's `field_24`, the same
/// 8-byte `GpMsgEntry` records the 110300/110800 carriers park there.
extern GpMsgEntry D_actor_143900_801413BC[];

/// Animation bank `func_800B3F84` seeds the work block's slots from, the pair
/// to `D_actor_143900_80149688` the overlay's own variant hands over.
extern u8 D_actor_143900_801413F8[];

/// State 0 of the `ActorsShared80131f9c` dispatcher: allocate the work block,
/// publish it in `ActorsShared80131f9cWork` and on the task's 0x1C slot, point
/// the model's light and color matrices and its animation context at it, and
/// hand the task on to state 1. Every later access to the block goes through
/// the global, which is why the pointer is reloaded at each use instead of
/// staying in a callee-saved register.
///
/// The position it forwards to `func_800D7A9C` is the model root's translation
/// with its Y dropped by 0x320 - the ground offset every carrier of this body
/// applies.
void ActorsShared80131f9cSub0(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor143900Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;

    obj                      = task->extra;
    coord                    = obj->field_8;
    work                     = Mem_Calloc(0x4F0, 0);
    ActorsShared80131f9cWork = work;
    task->idMap              = (TaskIdMap*)work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback       = ActorsShared801366fc;
    coord->sub               = &Gfx_ViewCoord;
    enemy->field_4           = &coord->coord;
    enemy->field_48          = 0;
    enemy->node.field_5      = 0;
    enemy->node.field_4      = 1;
    obj->field_E             = 1;
    obj->field_C             = 0;
    obj->field_1C            = &ActorsShared80131f9cWork->light;
    obj->field_20            = &ActorsShared80131f9cWork->color;
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
    task->field_24                      = D_actor_143900_801413BC;
    func_actor_143900_80131FD4(task);
    task->state += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900", func_actor_143900_80131FD4);

INCLUDE_RODATA("actors/nonmatchings/actor_143900/actor_143900", D_actor_143900_80131E20);
