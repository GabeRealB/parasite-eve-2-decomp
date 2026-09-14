#include "common.h"

#include "actors/actor_105700.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_105700_80131EC4;

/// Sound id the spawn cue is played against; the low byte comes from the
/// context block's room/channel bits.
extern s32 D_actor_105700_80149058;

INCLUDE_RODATA("actors/nonmatchings/actor_105700/actor_105700_6", D_actor_105700_80131EC4);

/// Spawns the effect burst for the owner's coordinate, hands that coordinate
/// to the pan/depth sound cue, then parks the work block in state 2.
void func_actor_105700_80137130(GpEnemy* arg0, Task* arg1)
{
    Actor105700*     owner;
    Actor105700Obj*  obj;
    Actor105700Obj*  ownerObj;
    Actor105700Work* work;
    GsCOORDINATE2*   coord;
    s16              state;
    s32              snd;
    s32              pan;

    owner        = (Actor105700*)arg1->parent;
    obj          = (Actor105700Obj*)arg1->extra;
    ownerObj     = owner->field_2C;
    work         = owner->field_1C;
    coord        = (GsCOORDINATE2*)obj->field_8;
    obj->field_C = ownerObj->field_C;
    state        = work->field_6D2;

    switch (state) {
        case 0:
            coord->flg = 0;
            return;
        case 1:
            Gp_SpawnEff(0x6005C, coord, 0x10002600, NULL);
            Gp_SpawnEff(0x6005C, coord, 0x01002600, NULL);
            Gp_SpawnEff(0x6005C, coord, 0x01002600, NULL);
            Gp_SpawnEff(0x6005C, coord, 0x02002600, NULL);
            work->field_6D2 = 2;
            snd             = D_actor_105700_80149058 |
                  (((u16)((Actor105700Ctx*)arg1->spawnArg2)->field_8 >> 0xC) << 8);
            pan = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(snd, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
            return;
        case 2:
            arg1->state = state;
            return;
    }
}

void func_actor_105700_80137290(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_105700_80131EC4;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
