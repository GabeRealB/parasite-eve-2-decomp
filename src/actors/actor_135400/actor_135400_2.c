#include "common.h"

#include "actors/actor_135400.h"

#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"

/// The three flat lights `func_actor_135400_80132CB0` loads into the model's
/// light / colour matrices: an axis-aligned light on X, Y and Z (`vy` / `vx` /
/// `vz`), each the same mid grey.
extern GsF_LIGHT D_actor_135400_8013F904[3];

/// The actor's animation table: `(anim id, handler)` pairs for 0x7D3 / 0x7D4 /
/// 0x7D5, ended by `0x7FFFFFFF`. `func_actor_135400_80132B60` parks its address
/// in `Task::field_24` (0x24).
extern s32 D_actor_135400_8013F8E4;

/// The actor's default animation arguments; defined at the end of this file so
/// its `.rodata` lands after the other units' tables.
extern const GpAnimArg D_actor_135400_80131EA0;

void func_actor_135400_80132D24(Task* task, s32 anim, GpAnimArg* params, s32 arg3);
void func_actor_135400_80132EBC(Task* task, s32 anim, s32 arg2, s32 arg3);
void func_actor_135400_80132C90(Task* task);
void func_actor_135400_80132CB0(Task* task);

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400_2", func_actor_135400_80132650);

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400_2", func_actor_135400_8013276C);

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400_2", func_actor_135400_801327E8);

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400_2", func_actor_135400_801328DC);

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400_2", func_actor_135400_801329B0);

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400_2", func_actor_135400_80132AF4);

void func_actor_135400_80132B60(Task* arg0)
{
    Actor135400Work* work;
    GpAnimArg        params;
    GpAnimArg        spawn;

    memset(&params, 0, sizeof(params));
    params.field_4 = 1;
    spawn          = D_actor_135400_80131EA0;
    if ((GameFlag_GetNibble(0x6C) > 0) || ((work = Mem_Calloc(0x498, 0)) == NULL)) {
        Gp_EnemyTaskExit(arg0);
        return;
    }
    arg0->idMap     = (TaskIdMap*)work;
    work->field_43D = -1;
    work->field_43E = -1;
    work->field_494 = -1;
    work->params    = spawn;
    func_actor_135400_80132D24(arg0, 0x7D3, &params, 0);
    func_actor_135400_80132EBC(arg0, 0x7D5, 1, 0);
    func_actor_135400_80132CB0(arg0);
    arg0->field_24     = &D_actor_135400_8013F8E4;
    arg0->exitCallback = func_actor_135400_80132C90;
    arg0->state       += 1;
}

void func_actor_135400_80132C90(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

void func_actor_135400_80132CB0(Task* task)
{
    Actor135400Work* work = (Actor135400Work*)task->idMap;
    TmdObject*       obj  = (TmdObject*)task->extra;
    GsF_LIGHT*       light;
    s32              i;

    obj->field_1C = &work->lightMtx;
    obj->field_20 = &work->colorMtx;
    for (i = 0, light = D_actor_135400_8013F904; i < 3; i++, light++) {
        Gfx_SetFlatLight(i, light, &work->lightMtx, &work->colorMtx);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400_2", func_actor_135400_80132D24);

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400_2", func_actor_135400_80132E40);

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400_2", func_actor_135400_80132EBC);

/* The animation arguments `func_actor_135400_80132B60` copies into
   `Actor135400Work::params` when the actor is created. It closes this unit's
   `.rodata` -- `func_actor_135400_801328DC`'s jump table (0x5C) and
   `D_actor_135400_80131E94` (0x74) precede it -- which is why it is defined
   here rather than beside the other declarations: `.rodata` follows source
   order. */
const GpAnimArg D_actor_135400_80131EA0 = { 0, 2, 1, 10, 0 };
