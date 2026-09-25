#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "actors/actor_102300.h"

/// Sound id of the child's cue, ORed with the enemy's id nibble.
extern s32 Actor02300_D15B08;

void Actor02300_Fn028AC(Task* arg0);
void Actor02300_Fn02EA0(Task* arg0);
void Actor02300_Fn01A20(Task* arg0);

/// The enemy's three state handlers - spawn/setup, per-frame tick
/// and teardown - dispatched through by state.
const GpEnemyTaskFuncTable3 Actor02300_D00078 = {
    Actor02300_Fn028AC,
    Actor02300_Fn02EA0,
    Actor02300_Fn01A20,
};

/// Per-frame state of the child task `Actor02300_Fn03D44` sets up at part 11
/// of the enemy's model. It mirrors the enemy's model flags onto its own and
/// runs on the enemy's `field_6D2`: 0 keeps its coordinate refreshed, 1 spawns
/// four effects on it, plays the cue and moves to 2, and 2 hands the task over
/// to state 2.
void Actor02300_Fn03D88(GpEnemy* arg0, Task* arg1)
{
    Actor102300*     owner;
    TmdObject*       obj;
    TmdObject*       ownerObj;
    Actor102300Work* work;
    GsCOORDINATE2*   coord;
    s16              state;
    s32              snd;
    s32              pan;

    owner      = (Actor102300*)arg1->parent;
    obj        = (TmdObject*)arg1->extra;
    ownerObj   = owner->field_2C;
    work       = owner->field_1C;
    coord      = obj->coords;
    obj->flags = ownerObj->flags;
    state      = work->field_6D2;

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
            snd             = Actor02300_D15B08 | (((u16)((GpEnemy*)arg1->spawnArg2)->placeKey >> 0xC) << 8);
            pan             = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
            return;
        case 2:
            arg1->state = state;
            return;
    }
}

void Actor02300_Fn03EE8(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor02300_D00078;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
