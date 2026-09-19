#include "common.h"

#include "actors/actor_135400.h"

#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"

/// The three flat lights `func_actor_135400_80132CB0` loads into the model's
/// light / colour matrices: an axis-aligned light on X, Y and Z (`vy` / `vx` /
/// `vz`), each the same mid grey.
extern GsF_LIGHT D_actor_135400_8013F904[3];

/// The actor's animation table: `(anim id, handler)` pairs for 0x7D3 / 0x7D4 /
/// 0x7D5, ended by `0x7FFFFFFF`. `func_actor_135400_80132B60` parks its address
/// in `Task::msgTable` (0x24).
extern s32 D_actor_135400_8013F8E4;

/// Per-step frame counts of the actor's 0x7D3 animation: eight `s16` entries
/// indexed by `Actor135400Work::params.field_4`. `func_actor_135400_801329B0`
/// runs the task's `killCountdown` up and, once it passes the entry for the
/// current step, advances that step -- wrapping at 7 -- and re-issues the
/// animation. The first and last entries are zero, so neither ever expires.
extern s16 D_actor_135400_8013F8C4[];

/// The actor's default animation arguments; defined at the end of this file so
/// its `.rodata` lands after the other units' tables.
extern const GpAnimArg D_actor_135400_80131EA0;

void func_actor_135400_80132D24(Task* task, s32 anim, GpAnimArg* params, s32 arg3);
s32  func_actor_135400_80132EBC(Task* task, s32 anim, s32 arg2, s32 arg3);
void func_actor_135400_80132C90(Task* task);
void func_actor_135400_80132CB0(Task* task);

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400_2", func_actor_135400_80132650);

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400_2", func_actor_135400_8013276C);

/// `Gp_DispatchMsg` handler for message 0x7D5, run against the `TmdObject`
/// parked in `Task::extra` -- the same four-way model switch
/// `func_actor_135400_80132EBC` performs. Mode 0 shows the model (bit 0x80) and
/// clears the 4 flag, 1 hides it, frees the aux buffers and clears the flag, 2
/// shows it, frees those buffers and sets the flag, and 3 hides it while
/// setting the flag. Anything else returns 1 and leaves the object alone; the
/// handled modes return 0. Either way the resulting `field_C` is copied onto
/// the part task's model (`Actor135400MainWork::field_4B8`), keeping the pair in
/// step.
s32 func_actor_135400_801327E8(Task* task, s32 msgId, s32 mode, s32 arg3)
{
    TmdObject* obj;
    TmdObject* other;
    s32        ret;

    obj   = (TmdObject*)task->extra;
    other = (TmdObject*)((Actor135400MainWork*)task->work)->field_4B8->extra;
    ret   = 0;
    switch (mode) {
        case 0:
            obj->flags |= 0x80;
            obj->flags &= ~4;
            break;
        case 1:
            obj->flags &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~4;
            break;
        case 2:
            obj->flags |= 0x80;
            Tmd_FreeBuffers(obj);
            obj->flags |= 4;
            break;
        case 3:
            obj->flags &= ~0x80;
            obj->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    other->flags = obj->flags;
    return ret;
}

s32 func_actor_135400_801328DC(Task* task, s32 msgId, Actor135400Msg7DB* msg, s32 arg3)
{
    Actor135400MainWork* work;
    TmdObject*           model;

    work = (Actor135400MainWork*)task->work;
    switch (msg->field_2) {
        case 0:
            if (work->field_4BC != NULL) {
                model         = (TmdObject*)work->field_4BC->extra;
                model->flags &= 0xFF7F;
            }
            break;
        case 1:
            if (work->field_4BC != NULL) {
                model         = (TmdObject*)work->field_4BC->extra;
                model->flags |= 0x80;
            }
            break;
        case 2:
            work->headAim = 1;
            break;
        case 3:
            work->headAim = 0;
            break;
        case 4:
            if (work->field_4BC != NULL) {
                work->field_4BC->spawnArg1 = 1;
            }
            break;
        case 5:
            if (work->field_4BC != NULL) {
                work->field_4BC->spawnArg1 = 3;
                model                      = (TmdObject*)work->field_4BC->extra;
                model->flags              &= 0xFF7F;
            }
            break;
    }
    return 0;
}

/// The actor's per-frame tick. Once `func_actor_135400_80132D24` has raised
/// `field_43C` it ticks the work block's twenty animation slots through
/// `Gp_AnimTickIndex`; while the model is drawn (bit 0x80 of
/// `TmdObject::flags` clear) it draws the ground shadow under the model's
/// root part, as `func_actor_135400_801322A8` does for the main task. It then
/// steps the 0x7D3 animation on the `D_actor_135400_8013F8C4` frame counts, and
/// finally runs `field_494` down -- at zero the model's aux buffers are freed
/// and the countdown carries on to -1, so that free happens once.
void func_actor_135400_801329B0(Task* task)
{
    Actor135400Work* work;
    TmdObject*       ext;
    VECTOR3          pos;
    s32              i;
    s32              step;
    u16              count;

    work = (Actor135400Work*)task->work;
    ext  = task->extra;
    if (work->field_43C != 0) {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
    if (!(ext->flags & 0x80) && (func_800EA1A8((VECTOR3*)((TmdObject*)task->extra)->coords[0].workm.t, &pos) != 0)) {
        Gp_DrawEffGroundQuad(&pos, 0x180, Gp_State1C->groundShade);
    }
    count               = task->killCountdown + 1;
    task->killCountdown = count;
    if (D_actor_135400_8013F8C4[work->params.field_4] < (s16)count) {
        work->params.field_4 = work->params.field_4 + 1;
        if (work->params.field_4 >= 7) {
            work->params.field_4 = 1;
        }
        func_actor_135400_80132D24(task, 0x7D3, &work->params, 0);
        task->killCountdown = 0;
    }
    step = work->field_494;
    if (step >= 0) {
        if (step == 0) {
            Tmd_FreeBuffers(ext);
            step = work->field_494;
        }
        step           -= 1;
        work->field_494 = step;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400_2", func_actor_135400_80132AF4);

void func_actor_135400_80132B60(Task* arg0)
{
    Actor135400Work* work;
    GpAnimArg        params;
    GpAnimArg        spawn;

    memset(&params, 0, sizeof(params));
    params.field_4 = 1;
    spawn          = D_actor_135400_80131EA0;
    if ((GameFlag_GetNibble(0x6C) > 0) || ((work = memCalloc(0x498, 0)) == NULL)) {
        Gp_EnemyTaskExit(arg0);
        return;
    }
    arg0->work      = (TaskIdMap*)work;
    work->field_43D = -1;
    work->field_43E = -1;
    work->field_494 = -1;
    work->params    = spawn;
    func_actor_135400_80132D24(arg0, 0x7D3, &params, 0);
    func_actor_135400_80132EBC(arg0, 0x7D5, 1, 0);
    func_actor_135400_80132CB0(arg0);
    arg0->msgTable     = &D_actor_135400_8013F8E4;
    arg0->exitCallback = func_actor_135400_80132C90;
    arg0->state       += 1;
}

void func_actor_135400_80132C90(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

void func_actor_135400_80132CB0(Task* task)
{
    Actor135400Work* work = (Actor135400Work*)task->work;
    TmdObject*       obj  = (TmdObject*)task->extra;
    GsF_LIGHT*       light;
    s32              i;

    obj->lightMtx = &work->lightMtx;
    obj->colorMtx = &work->colorMtx;
    for (i = 0, light = D_actor_135400_8013F904; i < 3; i++, light++) {
        Gfx_SetFlatLight(i, light, &work->lightMtx, &work->colorMtx);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400_2", func_actor_135400_80132D24);

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400_2", func_actor_135400_80132E40);

/// `Gp_DispatchMsg` handler for the actor's 4-way model-mode switch, run
/// against the `TmdObject` parked in `Task::extra`. Mode 0 shows the model and
/// clears the 4 flag, 1 hides it, frees the aux buffers and clears the flag, 2
/// does both plus latching the mode into the work block's `field_494`, and 3
/// hides it while setting the flag. Anything else returns 1 and leaves the
/// object alone; the handled modes return 0. The same body shape as
/// `func_actor_141000_80133E8C`.
s32 func_actor_135400_80132EBC(Task* task, s32 anim, s32 arg2, s32 arg3)
{
    TmdObject* obj;
    s32        ret;

    obj = task->extra;
    ret = 0;
    switch (arg2) {
        case 0:
            obj->flags |= 0x80;
            obj->flags &= ~4;
            break;
        case 1:
            obj->flags &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~4;
            break;
        case 2:
            obj->flags                               |= 0x80;
            ((Actor135400Work*)task->work)->field_494 = arg2;
            obj->flags                               |= 4;
            break;
        case 3:
            obj->flags &= ~0x80;
            obj->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

/* The animation arguments `func_actor_135400_80132B60` copies into
   `Actor135400Work::params` when the actor is created. It closes this unit's
   `.rodata` -- `func_actor_135400_801328DC`'s jump table (0x5C) and
   `D_actor_135400_80131E94` (0x74) precede it -- which is why it is defined
   here rather than beside the other declarations: `.rodata` follows source
   order. */
const GpAnimArg D_actor_135400_80131EA0 = { 0, 2, 1, 10, 0 };
