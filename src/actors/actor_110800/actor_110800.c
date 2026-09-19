#include "common.h"

#include "actors/actor_110800.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

/// Step 0 of the `ActorsShared80131f9c` dispatcher: allocate the work block,
/// publish it, and hand the model's animation context its slot array.
///
/// Every access to the block goes through `ActorsShared80131f9cWork` rather
/// than the `Mem_Calloc` result, which is why the pointer is reloaded at each
/// use instead of staying in a callee-saved register. `task->msgTable` takes
/// the message table the step-1 handler leaves behind.
///
/// Instruction-for-instruction the `actor_110300` copy of this body; only the
/// data symbols it relocates against are this overlay's own.
void ActorsShared80131f9cSub0(GpEnemy* enemy, Task* task)
{
    VECTOR         vec;
    void*          work;
    TmdObject*     obj;
    GsCOORDINATE2* coord;

    obj                      = task->extra;
    coord                    = obj->coords;
    work                     = Mem_Calloc(0x55C, 0);
    ActorsShared80131f9cWork = work;
    task->work               = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback      = func_actor_110800_8013232C;
    coord->sub              = &gGfxViewCoord;
    enemy->field_4          = &coord->coord;
    enemy->node.field_4     = 1;
    enemy->field_48         = 0;
    enemy->node.field_5     = 0;
    obj->otOffset           = 0;
    coord->flg              = 0;
    D_actor_110800_80139F14 = (GpActorWork*)task;
    D_actor_110800_80139F18 = Task_SpawnFromTable(D_actor_110800_80139EDC, 1, 0, 0);
    func_800B3F84(&ActorsShared80131f9cWork->anim, D_actor_110800_80139EF4, (GpAnimObj*)obj,
                  ActorsShared80131f9cWork->aux, ActorsShared80131f9cWork->slots);
    ActorsShared80131f9cWork->animId    = 1;
    ActorsShared80131f9cWork->field_474 = 2;
    func_actor_110800_80132368(task);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    ActorsShared80131f9cWork->field_47A++;
    task->msgTable = D_actor_110800_80139EC4;
    task->state++;
}

/// Step 1 of the `ActorsShared80131f9c` dispatcher, the walk/run footstep cue:
/// run the body the actor's step selects, cue the sound the running
/// animation's frame table asks for, then refresh the model root as step 0 did
/// by feeding its world translation to `func_800D7A9C` (the light solve)
/// against the model object itself.
///
/// The two animation ids this actor plays carry a frame table each: id 4
/// watches slot 19 alone, id 5 watches slot 19 and then slot 16. An entry
/// latches the frame it fired for in `field_47C`, so a frame that is held over
/// several calls only cues once; the mask is the frame index of the slot's
/// halfword.
///
/// The body reaches the task through the second argument, so the incoming `$a1`
/// is copied into `$a0` (the first, unused, is the `GpEnemy*`), and the model
/// and its coordinate are read through that copy. `task->extra` is written
/// twice with the coordinate taken through the first read: that leaves cse's
/// load in a temporary and copies it into `obj`, which is the `move` between
/// the two loads the target has.
///
/// The switch reads `animId` signed. The field is unsigned, so the cast is
/// load-bearing: without it the halfword load is `lhu` where the target has
/// `lh`.
void ActorsShared80131f9cSub1(GpEnemy* enemy, Task* task)
{
    GsCOORDINATE2* coord;
    TmdObject*     obj;
    VECTOR         vec;

    coord = ((TmdObject*)task->extra)->coords;
    obj   = (TmdObject*)task->extra;
    func_actor_110800_80132368(task);
    switch ((s16)ActorsShared80131f9cWork->animId) {
        case 4:
            if ((ActorsShared80131f9cWork->slots[19].field_2 & 0x3FF) == 0xC8) {
                if (ActorsShared80131f9cWork->field_47C != (ActorsShared80131f9cWork->slots[19].field_2 & 0x3FF)) {
                    SndEvt_EnqueueType6(0x510D0011, 0, 0);
                }
                ActorsShared80131f9cWork->field_47C = ActorsShared80131f9cWork->slots[19].field_2 & 0x3FF;
            }
            if ((ActorsShared80131f9cWork->slots[19].field_2 & 0x3FF) == 0xCA) {
                if (ActorsShared80131f9cWork->field_47C != (ActorsShared80131f9cWork->slots[19].field_2 & 0x3FF)) {
                    SndEvt_EnqueueType6(0x510D000D, 0, 0);
                }
                ActorsShared80131f9cWork->field_47C = ActorsShared80131f9cWork->slots[19].field_2 & 0x3FF;
            }
            if ((ActorsShared80131f9cWork->slots[19].field_2 & 0x3FF) == 0xCD) {
                if (ActorsShared80131f9cWork->field_47C != (ActorsShared80131f9cWork->slots[19].field_2 & 0x3FF)) {
                    SndEvt_EnqueueType6(0x510D000E, 0, 0);
                }
                ActorsShared80131f9cWork->field_47C = ActorsShared80131f9cWork->slots[19].field_2 & 0x3FF;
            }
            break;
        case 5:
            if ((ActorsShared80131f9cWork->slots[19].field_2 & 0x3FF) == 0x115) {
                if (ActorsShared80131f9cWork->field_47C != (ActorsShared80131f9cWork->slots[19].field_2 & 0x3FF)) {
                    SndEvt_EnqueueType6(0x510D000F, 0, 0);
                }
                ActorsShared80131f9cWork->field_47C = ActorsShared80131f9cWork->slots[19].field_2 & 0x3FF;
            }
            if ((ActorsShared80131f9cWork->slots[19].field_2 & 0x3FF) == 0x11F) {
                if (ActorsShared80131f9cWork->field_47C != (ActorsShared80131f9cWork->slots[19].field_2 & 0x3FF)) {
                    SndEvt_EnqueueType6(0x510D000F, 0, 0);
                }
                ActorsShared80131f9cWork->field_47C = ActorsShared80131f9cWork->slots[19].field_2 & 0x3FF;
            }
            if ((ActorsShared80131f9cWork->slots[16].field_2 & 0x3FF) == 0xCE) {
                if (ActorsShared80131f9cWork->field_47C != (ActorsShared80131f9cWork->slots[16].field_2 & 0x3FF)) {
                    SndEvt_EnqueueType6(0x510D0010, 0, 0);
                }
                ActorsShared80131f9cWork->field_47C = ActorsShared80131f9cWork->slots[16].field_2 & 0x3FF;
            }
            if ((ActorsShared80131f9cWork->slots[16].field_2 & 0x3FF) == 0xD8) {
                if (ActorsShared80131f9cWork->field_47C != (ActorsShared80131f9cWork->slots[16].field_2 & 0x3FF)) {
                    SndEvt_EnqueueType6(0x510D0010, 0, 0);
                }
                ActorsShared80131f9cWork->field_47C = ActorsShared80131f9cWork->slots[16].field_2 & 0x3FF;
            }
            break;
    }
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
}

INCLUDE_RODATA("actors/nonmatchings/actor_110800/actor_110800", D_actor_110800_80131E20);
