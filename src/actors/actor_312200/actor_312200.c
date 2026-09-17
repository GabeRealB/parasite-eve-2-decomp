#include "common.h"

#include "actors/actor_312200.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/tmd.h"

extern u8 D_actor_312200_80169F44[];
extern u8 D_actor_312200_80169F5C[];

INCLUDE_ASM("actors/nonmatchings/actor_312200/actor_312200", func_actor_312200_80161E30);

INCLUDE_ASM("actors/nonmatchings/actor_312200/actor_312200", func_actor_312200_80162178);

INCLUDE_ASM("actors/nonmatchings/actor_312200/actor_312200", func_actor_312200_801626C4);

INCLUDE_ASM("actors/nonmatchings/actor_312200/actor_312200", func_actor_312200_80162868);

INCLUDE_ASM("actors/nonmatchings/actor_312200/actor_312200", func_actor_312200_80162FB4);

/// Spawn body: allocates the actor's 0x984-byte `Actor312200Work`, stores it in
/// `Task::idMap` and seeds the enemy object, the model's root coordinate and the
/// animation context from the `TmdObject` in `Task::extra`. The enemy takes the
/// root coordinate's matrix as `field_4` and the model's third part coordinate
/// as `field_18`; the display node the body builds in place points its `field_C`
/// at the block's three-entry `GpRec18` table and its `field_8` at the model's
/// fourth part coordinate. The model coordinate is parented to `Gfx_ViewCoord`
/// and rebuilt once before the three matrix translations are copied to
/// `func_800D7A9C` (start 0, count 3).
void func_actor_312200_80163178(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;
    TmdObject*       tmd;
    Actor312200Work* mem;
    Actor312200Work* work;
    GpObj*           node;

    obj         = (TmdObject*)task->extra;
    coord       = obj->field_8;
    mem         = (Actor312200Work*)Mem_Calloc(sizeof(Actor312200Work), 0);
    work        = mem;
    task->idMap = (TaskIdMap*)mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    tmd                = (TmdObject*)task->extra;
    tmd->field_1C      = &work->light;
    tmd->field_20      = &work->color;
    enemy->field_4     = &coord->coord;
    enemy->field_48    = 0;
    enemy->field_1C.vx = 0;
    enemy->field_1C.vy = 0;
    enemy->field_1C.vz = 0;
    enemy->field_18    = &((TmdObject*)task->extra)->field_8[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.field_4 = 1;
    enemy->field_4D     = 0;
    enemy->field_4C     = 0;
    enemy->field_4D     = 0;
    func_800B3F84(&work->anim, D_actor_312200_80169F44, (GpAnimObj*)obj, work->poses, work->slots);
    work->field_88C = 2;
    work->field_892 = 1;
    work->field_896 = 0x10;
    func_actor_312200_80162FB4(task);
    node           = &work->field_8BC;
    node->field_8  = &((TmdObject*)task->extra)->field_8[3];
    node->field_C  = work->recs;
    node->field_10 = 0;
    node->field_12 = 0;
    node->field_14 = 0;
    node->field_18 = 0x3000A;
    node->field_1C = 0x180;
    node->flags    = 1;
    Gp_LinkObj(2, node);
    node->flags |= 0x8000;
    Gp_InitRec18Table(node->field_C, 3, 0);
    task->field_24  = D_actor_312200_80169F5C;
    work->field_8AC = 0;
    work->field_8AD = 1;
    coord->sub      = &Gfx_ViewCoord;
    coord->flg      = 0;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    func_800D7A9C((TmdObject*)task->extra, &vec, 0, 3);
    work->field_0 = 0;
    work->field_2 = -1;
    task->state++;
}

/// Per-tick handler and state dispatcher, called with the task second. The
/// handler table is built in place - the show handler at index 0, the tick
/// handler at index 1 - and `field_0` selects from it, unless the global
/// `D_801153F4` holds the actor. `field_4` records whether the state moved
/// before it is re-latched into `field_2`. The tail clears the display node's
/// `GpRec18` record while occupied, re-propagates the root coordinate's
/// translation over the model's three part coordinates while `field_8AD` is
/// set, and then refreshes `field_8AD` from that coordinate's `flg` - so the
/// propagation runs on the frame after the coordinate is dirtied. That same
/// coordinate is rebuilt (`flg` dropped) and the 0x51030008 loop queued while
/// the room is live, from view 0x10 with the 0x7DB action `field_8B8` at 1.
///
/// The `SOFT_BARRIER` is a matching aid, not the original's: without it the
/// scheduler pulls the handler table's first `lui` in front of the `idMap`
/// load. The trailing `vec` is the original's own - three dead stores, but the
/// frame and the rest of the schedule are built around them.
void func_actor_312200_80163370(GpEnemy* enemy, Task* task)
{
    TmdObject*       obj;
    Actor312200Work* work;
    VECTOR           vec;
    void             (*states[2])(Task*);
    s32              pan;

    work = (Actor312200Work*)task->idMap;
    SOFT_BARRIER();
    states[0] = func_actor_312200_80163778;
    states[1] = func_actor_312200_801637CC;
    if (D_801153F4 == 0) {
        if (work->field_2 != work->field_0) {
            work->field_4 = 1;
        } else {
            work->field_4 = 0;
        }
        work->field_2 = work->field_0;
        states[work->field_0](task);
        if (work->recs[0].field_4 != 0) {
            Gp_ClearRec18Occupied(work->recs);
        }
        if (work->field_8AD != 0) {
            obj = (TmdObject*)task->extra;
            func_800D7A9C(obj, (VECTOR*)obj->field_8->workm.t, 0, 3);
        }
        if (Game_Session->field_4D != 0) {
            ((TmdObject*)task->extra)->field_8->flg = 0;
            if ((Gp_GetViewIndex() == 0x10) && (work->field_8B8 == 1)) {
                pan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)task->extra)->field_8);
                SndEvt_EnqueueType6(0x51030008, pan,
                                    (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)task->extra)->field_8));
            }
        }
        if (((TmdObject*)task->extra)->field_8->flg == 0) {
            work->field_8AD = 1;
        } else {
            work->field_8AD = 0;
        }
        vec.vz = 0;
        vec.vy = 0;
        vec.vx = 0;
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_312200/actor_312200", D_actor_312200_80161E20);

INCLUDE_RODATA("actors/nonmatchings/actor_312200/actor_312200", ActorsShared80135df4Table);
