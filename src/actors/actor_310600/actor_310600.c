#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// 0x538-byte work block `func_actor_310600_80161E64` allocates with
/// `memCalloc` and hangs off `Task::work`. The display node at `obj` is
/// linked by `Gp_LinkObj` at spawn (its `ctx.recs` points at `rec`, the
/// `GpRec18` table `Gp_InitRec18Table` fills) and unlinked again by the
/// exit callback `func_actor_310600_80162A24`.
///
/// `light` / `color` are the actor's own lighting and colour matrices;
/// `func_actor_310600_80162A58` republishes them onto the model's
/// `TmdObject::lightMtx` / `colorMtx` in place of the shared defaults
/// `Gp_BindDefaultMtx` installs.
///
/// The block is fronted by the animation context `func_actor_310600_8016246C`
/// drives: the `GpAnimCtx` (`func_800B3F84` takes the block address), the
/// twenty `GpAnimSlot`s immediately above it, and the 0x140-byte table
/// `func_800B3F84` also takes at 0x334. `field_474` is the once-only latch the
/// slots are started through, and `field_476` / `field_475` are the animation
/// bank index and the animation id, latched on change and re-read from the
/// block by the loops below them.
typedef struct Actor310600Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0x14];
    /* 0x334 */ byte       field_334[0x140];
    /* 0x474 */ s8         field_474;
    /* 0x475 */ s8         field_475;
    /* 0x476 */ s8         field_476;
    /* 0x477 */ s8         field_477;
    /* 0x478 */ s16        field_478;
    /* 0x47A */ s16        field_47A;
    /* 0x47C */ s16        field_47C;
    /* 0x47E */ u16        field_47E;
    /* 0x480 */ MATRIX     light;
    /* 0x4A0 */ MATRIX     color;
    /* 0x4C0 */ GpObj      obj;
    /* 0x4E0 */ GpRec18    rec;
    /* 0x4F8 */ s32        field_4F8;
    /* 0x4FC */ s32        field_4FC;
    /* 0x500 */ s32        field_500;
    /* 0x504 */ byte       pad_504[0x4];
    /* 0x508 */ VECTOR3    step; // local-space offset `ApplyMatrixLV` rotates into world space
    /* 0x514 */ byte       pad_514[0x4];
    /* 0x518 */ s32        field_518;
    /* 0x51C */ s32        field_51C;
    /* 0x520 */ s32        field_520;
    /* 0x524 */ byte       pad_524[0x4];
    /* 0x528 */ SVECTOR    limit; // per-axis stop threshold; 0x7FFF on all three disables it
    /* 0x530 */ byte       pad_530[0x8];
} Actor310600Work;
STATIC_ASSERT_SIZEOF(Actor310600Work, 0x538);

/// Overlay of the `GsCOORDINATE2` at `TmdObject::coords`, the actor's root
/// part. Offset 0x44 (libgs `param`, `super` at 0x48) holds the Euler angles
/// `func_actor_310600_80162AD8` and `func_actor_310600_80162C18` write and then
/// hand straight to `RotMatrix`.
typedef struct Actor310600Coord {
    /* 0x00 */ s32     flg;
    /* 0x04 */ MATRIX  coord;
    /* 0x24 */ MATRIX  workm;
    /* 0x44 */ SVECTOR rot;
} Actor310600Coord;
STATIC_ASSERT_SIZEOF(Actor310600Coord, 0x4C);

/// The 0x14-byte command block the actor's state handlers build on the stack
/// and hand to `func_actor_310600_8016246C`, which reads it as
/// `{animId, state, path, param}`.
typedef struct Actor310600Cmd {
    /* 0x00 */ s32 animId;
    /* 0x04 */ s32 state;
    /* 0x08 */ s32 path;
    /* 0x0C */ s32 param;
    /* 0x10 */ s32 unk10;
} Actor310600Cmd;

/// Placement block `func_actor_310600_80162C18` receives: a world translation
/// followed by the Euler angles handed to `RotMatrix`.
typedef struct Actor310600Placement {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor310600Placement;
STATIC_ASSERT_SIZEOF(Actor310600Placement, 0x18);

/// Spawn table entry 1 is this actor's `Task::state` dispatcher; the type-1
/// setup entry it is spawned from is `func_actor_310600_80161E64`.
extern TaskDesc D_actor_310600_801796A4[];

/// The overlay's `GpMsgEntry` table, parked in `Task::msgTable`.
extern GpMsgEntry D_actor_310600_801796BC[];

/// Per-animation cue lists: `D_actor_310600_80179660[field_475]` is a
/// zero-terminated list of the frames at which that animation fires its effect.
extern s16*    D_actor_310600_80179660[];
extern SVECTOR D_actor_310600_80179694;
extern s32     D_actor_310600_8017969C;
extern s32     D_actor_310600_801796A0;

extern void* D_actor_310600_80179640[]; // animation bank table `work->field_476` indexes
extern s8    D_actor_310600_80179644[]; // extra ticks owed to the animation id in `work->field_475`

/// Spawn table of the follow-up task queued once the cue has fired five times.
extern TaskDesc D_80182AD8[];

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_actor_310600_80161E64(Task* task);
void func_actor_310600_80161FA0(Task* task);
void func_actor_310600_8016231C(Task* arg0);
s32  func_actor_310600_8016246C(Task* task, s32 arg1, Actor310600Cmd* cmd, s32 arg3);
s32  func_actor_310600_801625F0(Task* task, s32 arg1, s32 arg2, s32 arg3);
void func_actor_310600_801627A4(Task* task);
void func_actor_310600_801628B0(Task* task);
void func_actor_310600_80162948(Task* task);
void func_actor_310600_801629C4(Task* task);
void func_actor_310600_80162A24(Task* arg0);
void func_actor_310600_80162A58(Task* arg0);
void func_actor_310600_80162A74(void);
void func_actor_310600_80162A7C(Task* task);
void func_actor_310600_80162AD8(Task* task);
void func_actor_310600_80162B98(Task* task);

/// State handlers of the child part task, which `func_actor_310600_8016274C`
/// runs by `Task::state`: setup, tick and exit.
const TaskFuncTable3 D_actor_310600_80161E24 = { {
    func_actor_310600_801627A4,
    func_actor_310600_801628B0,
    taskKill,
} };

/// A second child handler triple - attach to the parent's part, an empty tick,
/// exit. No dispatcher in this package reads it.
const TaskFuncTable3 D_actor_310600_80161E30 = { {
    func_actor_310600_80162948,
    func_actor_310600_801629C4,
    taskKill,
} };

/// The actor's own state handlers, which `func_actor_310600_801629CC` runs by
/// `Task::state`: spawn/setup, per-frame tick and teardown.
const TaskFuncTable3 D_actor_310600_80161E3C = { {
    func_actor_310600_80161E64,
    func_actor_310600_80161FA0,
    func_actor_310600_80162A24,
} };

/// The actor's movement steps, which `func_actor_310600_80162A7C` runs by
/// `field_47E`: turn to face the target point, start moving, stop on arrival.
const TaskFuncTable3 D_actor_310600_80161E48 = { {
    func_actor_310600_80162AD8,
    func_actor_310600_80162B98,
    func_actor_310600_8016231C,
} };

/// The constant local-space offset `func_actor_310600_80162B98` rotates,
/// `{ 0, 0, 0x200000, 0 }` -- straight ahead along the part's own +Z.
const VECTOR D_actor_310600_80161E54 = { 0, 0, 0x200000, 0 };

void func_actor_310600_80161E64(Task* task)
{
    Actor310600Work* work;
    GpObj*           obj;

    work = memCalloc(0x538, 0);
    if (work == NULL) {
        Gp_EnemyTaskExit(task);
        return;
    }
    task->work      = (TaskIdMap*)work;
    work->field_475 = -1;
    work->field_476 = -1;
    work->field_477 = -1;
    work->field_47C = 0;
    work->field_47E = 0;
    work->field_518 = 0;
    work->field_51C = 0;
    work->field_520 = 0;
    Task_SpawnFromTable(D_actor_310600_801796A4, 1, 8, (s32)task);
    func_actor_310600_80162A58(task);
    obj           = &work->obj;
    obj->coord    = &((TmdObject*)task->extra)->coords[1];
    obj->ctx.recs = &work->rec;
    obj->key      = 0x30000;
    obj->radius   = 0x100;
    obj->pos.vx   = 0;
    obj->pos.vy   = 0;
    obj->pos.vz   = 0;
    obj->flags    = 1;
    Gp_LinkObj(2, obj);
    obj->flags |= 0x8000;
    Gp_InitRec18Table(obj->ctx.recs, 1, 0);
    task->msgTable = D_actor_310600_801796BC;
    func_actor_310600_801625F0(task, 0x7D5, 0, 0);
    task->exitCallback = func_actor_310600_80162A24;
    task->state++;
}

/// The actor's per-frame handler. Runs the entry of its second state table that
/// `field_47C` selects, then advances the root part by `step`: each axis'
/// accumulator carries a 16.16 offset whose whole part is added to the world
/// translation and whose fraction is kept, and clearing `flg` makes
/// `_gpUpdateCoordTree` rebuild the world matrix from it.
///
/// Once the slots have been started (`field_474`) every animation slot is
/// ticked, and the frame counter `field_478` is walked against the cue list
/// `D_actor_310600_80179660[field_475]` -- a zero-terminated list of frames at
/// which the animation currently playing fires an effect. The effect is chosen
/// by the animation id: ids 1 and 2 spawn 0x6006A and ask slot 4 for the
/// follow-up message, but only for the first five of them, after which the
/// other payload is sent and `D_80182AD8` is spawned instead; id 3 spawns
/// 0x6006D. The remaining ids have no cue.
///
/// While the model is visible its ground shadow is drawn at the root part's
/// world position and the occupancy table is cleared, and while the session
/// flag at `field_4D` is set the second part is re-derived and re-lit.
/// `field_477` is the teardown countdown: it frees the model buffers on the
/// tick it reaches zero and then stops at -1.
void func_actor_310600_80161FA0(Task* task)
{
    TmdObject*       ext               = task->extra;
    Actor310600Work* work              = (Actor310600Work*)task->work;
    void             (*funcs[2])(void) = { func_actor_310600_80162A74, (void (*)(void))func_actor_310600_80162A7C };
    VECTOR3          pos;
    GsCOORDINATE2*   coord;
    s16*             cues;
    s16*             cue;
    s32              i;

    funcs[work->field_47C]();
    coord              = ((TmdObject*)task->extra)->coords;
    work->field_518   += work->step.vx;
    work->field_51C   += work->step.vy;
    work->field_520   += work->step.vz;
    coord->coord.t[0] += (s16)(work->field_518 >> 16);
    coord->coord.t[1] += (s16)(work->field_51C >> 16);
    coord->coord.t[2] += (s16)(work->field_520 >> 16);
    coord->flg         = 0;
    work->field_518    = (u16)work->field_518;
    work->field_51C    = (u16)work->field_51C;
    work->field_520    = (u16)work->field_520;
    if (work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
    if (work->field_475 > 0) {
        cues = D_actor_310600_80179660[work->field_475];
        if (cues != NULL) {
            if (*cues != 0) {
                cue = cues;
                do {
                    if (*cue == work->field_478) {
                        coord = &((TmdObject*)task->extra)->coords[8];
                        switch (work->field_475) {
                            case 1:
                            case 2:
                                if ((s16)work->field_47A++ < 5) {
                                    Gp_SpawnEff(0x6006A, coord, 9, NULL);
                                    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7DB, (s32)&D_actor_310600_8017969C, 0);
                                } else {
                                    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7DB, (s32)&D_actor_310600_801796A0, 0);
                                    Task_SpawnFromTable(D_80182AD8, 2, 0, 0);
                                }
                                break;
                            case 3:
                                Gp_SpawnEff(0x6006D, coord, 6, &D_actor_310600_80179694);
                                break;
                        }
                        break;
                    }
                    cue++;
                } while (*cue != 0);
            }
            work->field_478++;
        }
    }
    if (!(ext->flags & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)task->extra)->coords[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x300, Gp_State1C->groundShade);
        }
        Gp_ClearRec18Occupied(&work->rec);
    }
    if (gGameSession->viewReady != 0) {
        ((TmdObject*)task->extra)->coords[1].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[1]);
        func_800D7A9C(ext, (VECTOR*)((TmdObject*)task->extra)->coords[1].workm.t, 0, 3);
    }
    if (work->field_477 >= 0) {
        if (work->field_477 == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->field_477--;
    }
}

/// Arrival handler of the actor's second state table (`field_47E`), reached once
/// `func_actor_310600_80162B98` has laid down the per-frame `step` offset: takes
/// each horizontal axis' gap between the target point `field_4F8` / `field_500`
/// and the root part's world translation -- the low 16 bits of the signed
/// difference, as in `func_actor_310600_80162AD8` -- and compares it against the
/// axis' stop threshold in `limit`, which starts at 0x7FFF. Both gaps past their
/// threshold means the actor has stopped closing in: the arrival preset of
/// message 0x7D3 is queued (animation bank 0, id 0xD, path 1, param 0xA), but
/// only while `field_475` still holds 0xC, and then `step` and the two counters
/// are cleared and the handler returns without re-arming. Otherwise each
/// threshold is pulled down to the gap just measured, so the next tick that
/// fails to shrink it is the one that fires.
void func_actor_310600_8016231C(Task* arg0)
{
    Actor310600Work*  work;
    Actor310600Coord* coord;
    SVECTOR           d;
    s32               dx;
    s32               dz;
    Actor310600Cmd    cmd;

    work  = (Actor310600Work*)arg0->work;
    coord = (Actor310600Coord*)((TmdObject*)arg0->extra)->coords;
    if (work->field_4F8 - coord->coord.t[0] >= 0) {
        dx = (u16)work->field_4F8 - (u16)coord->coord.t[0];
    } else {
        dx = (u16)coord->coord.t[0] - (u16)work->field_4F8;
    }
    d.vx = dx;
    if (work->field_500 - coord->coord.t[2] >= 0) {
        dz = (u16)work->field_500 - (u16)coord->coord.t[2];
    } else {
        dz = (u16)coord->coord.t[2] - (u16)work->field_500;
    }
    d.vz = dz;
    if (d.vx >= work->limit.vx && d.vz >= work->limit.vz) {
        if (work->field_475 == 0xC) {
            cmd.animId = 0;
            cmd.state  = 0xD;
            cmd.path   = 1;
            cmd.param  = 0xA;
            cmd.unk10  = 0;
            func_actor_310600_8016246C(arg0, 0x7D3, &cmd, 0);
        }
        work->step.vx   = 0;
        work->step.vy   = 0;
        work->step.vz   = 0;
        work->field_47C = 0;
        work->field_47E = 0;
        return;
    }
    work->limit.vx = d.vx < 0 ? -d.vx : d.vx;
    work->limit.vz = d.vz < 0 ? -d.vz : d.vz;
}

/// Animation preset handler of message 0x7D3: re-seeds the slot array off bank
/// table
/// `D_actor_310600_80179640` when the preset's bank index changes -- clearing
/// the latched id to -1 so the state below is re-applied -- then restarts or
/// resets every slot and ticks them, repeating the tick pass `1 +
/// D_actor_310600_80179644[state]` times.
///
/// The two byte stores must stay in this order. The second one is a QImode
/// store to a varying address, so cse treats it as aliasing everything and
/// drops the equivalence the first one recorded; that is what keeps
/// `work->field_476` a reload instead of the register `cmd->animId` arrived in.
s32 func_actor_310600_8016246C(Task* task, s32 arg1, Actor310600Cmd* cmd, s32 arg3)
{
    Actor310600Work* work;
    TmdObject*       ext;
    s32              i;
    s32              j;

    work = (Actor310600Work*)task->work;
    ext  = task->extra;
    if (cmd->animId != work->field_476) {
        work->field_476 = cmd->animId;
        work->field_475 = -1;
        func_800B3F84(&work->anim, D_actor_310600_80179640[work->field_476], ext, work->field_334,
                      work->slots);
    }
    if (cmd->state != work->field_475) {
        work->field_475 = cmd->state;
        if (cmd->path != 0) {
            for (i = 1; i < 0x14; i++) {
                func_800B4114(&work->anim, i, work->field_475, 0, cmd->param);
            }
        } else {
            for (i = 1; i < 0x14; i++) {
                Gp_AnimResetSlot(&work->anim, i, work->field_475);
            }
        }
        for (j = 0; j <= D_actor_310600_80179644[work->field_475]; j++) {
            for (i = 1; i < 0x14; i++) {
                Gp_AnimTickIndex(&work->anim, i);
            }
        }
        work->field_474 = 1;
        work->field_478 = 0;
        work->field_47A = 0;
    }
    return 0;
}

/// Mode handler for the actor's display object, called by the setup path
/// (`func_actor_310600_80161E64` with message 0x7D5 and mode 0) with the mode in
/// `arg2`. Modes 0 and 2 hide the model: bit 0x80 of `TmdObject.flags` goes on,
/// the 0x8000 flag comes off the actor's own object, and 0x4 is cleared. Modes 1
/// and 3 show it: 0x80 comes off, 0x8000 goes on, the buffers are reinstated
/// through `Tmd_AllocBuffers`, and 0x4 is set. Mode 2 additionally latches
/// `field_477` to 2. Returns 1 for a mode outside 0..3.
///
/// `work` and `w` are the same block on purpose. cse turns the second load of
/// `task->work` into a copy of the first and keeps the copy's register for the
/// mode 0..2 walks, because the only later use of the first load's register is
/// the `obj` assignment in the entry block -- so mode 3's walk reads the first
/// load's register and the other three read the copy's, the split the target
/// has. Writing `&work->obj` inside case 3 instead leaves cse canonicalizing the
/// walks the other way, and the overlay comes out three instructions short.
s32 func_actor_310600_801625F0(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    Actor310600Work* work;
    Actor310600Work* w;
    TmdObject*       ext;
    GpObj*           p;
    GpObj*           obj;
    s32              i;
    s32              ret;

    work = (Actor310600Work*)task->work;
    ext  = task->extra;
    w    = (Actor310600Work*)task->work;
    obj  = &work->obj;
    ret  = 0;
    switch (arg2) {
        case 0:
            ext->flags |= 0x80;
            p           = &w->obj;
            for (i = 0; i <= 0; i++) {
                p->flags &= 0x7FFF;
                p++;
            }
            ext->flags &= ~4;
            break;
        case 1:
            ext->flags &= ~0x80;
            p           = &w->obj;
            for (i = 0; i <= 0; i++) {
                p->flags |= 0x8000;
                p++;
            }
            Tmd_AllocBuffers(ext);
            ext->flags &= ~4;
            break;
        case 2:
            ext->flags |= 0x80;
            p           = &w->obj;
            for (i = 0; i <= 0; i++) {
                p->flags &= 0x7FFF;
                p++;
            }
            w->field_477 = 2;
            ext->flags  |= 4;
            break;
        case 3:
            ext->flags &= ~0x80;
            p           = obj;
            for (i = 0; i <= 0; i++) {
                p->flags |= 0x8000;
                p++;
            }
            ext->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

void func_actor_310600_8016274C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_310600_80161E24;
    sp.funcs[task->state](task);
}

/// Setup state of the child task whose handlers open
/// `D_actor_310600_80161E24`: hides the child's model, then mirrors the
/// parent's (`spawnArg2`) model flag bits 0x80 and 0x4 as the tick state
/// `func_actor_310600_801628B0` does. It draws the model at order-table offset
/// -2, hangs the child's root coordinate off the parent's part `spawnArg1`,
/// shares the parent's light and colour matrices, reparents the task under the
/// parent and steps to the next state.
void func_actor_310600_801627A4(Task* task)
{
    Task*          parent;
    TmdObject*     obj;
    TmdObject*     parentObj;
    GsCOORDINATE2* coords;
    GsCOORDINATE2* root;

    parent      = task->spawnArg2;
    obj         = task->extra;
    parentObj   = parent->extra;
    coords      = parentObj->coords;
    obj->flags |= 0x80;
    root        = obj->coords;
    if (!(parentObj->flags & 0x80)) {
        obj->flags &= 0xFF7F;
    }
    if (!(parentObj->flags & 4)) {
        obj->flags &= 0xFFFB;
        Tmd_AllocBuffers(obj);
    } else {
        obj->flags |= 4;
    }
    obj->otOffset = -2;
    coords       += task->spawnArg1;
    root->flg     = 0;
    root->sub     = coords;
    obj->lightMtx = parentObj->lightMtx;
    obj->colorMtx = parentObj->colorMtx;
    Task_Reparent(parent, task);
    task->state++;
}

/// Keeps a child model's visibility in step with its parent's: bits 0x80
/// (hidden) and 0x4 (buffers released) of the parent task's `TmdObject` - the
/// task named by `spawnArg2` - are copied onto the calling task's own object.
/// When bit 0x4 comes off, the child's draw buffers are rebuilt through
/// `Tmd_AllocBuffers`.
void func_actor_310600_801628B0(Task* task)
{
    TmdObject* parentObject;
    TmdObject* object;

    parentObject = (TmdObject*)((Task*)task->spawnArg2)->extra;
    object       = (TmdObject*)task->extra;

    if (!(parentObject->flags & 0x80)) {
        object->flags &= 0xFF7F;
    } else {
        object->flags |= 0x80;
    }
    if (!(parentObject->flags & 4)) {
        object->flags &= 0xFFFB;
        Tmd_AllocBuffers(object);
        return;
    }
    object->flags |= 4;
}

/// Attaches a child model to the part of its parent's skeleton named by the
/// spawn arguments (`spawnArg2` the parent task, `spawnArg1` the part): the
/// child's root coordinate is chained under that part's coordinate, the
/// parent's light and colour matrices are shared, and the task is reparented
/// so it is updated with the parent.
void func_actor_310600_80162948(Task* task)
{
    Task*          parent;
    s32            part;
    TmdObject*     extra;
    TmdObject*     parentExtra;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* dest;

    parent          = (Task*)task->spawnArg2;
    part            = task->spawnArg1;
    extra           = (TmdObject*)task->extra;
    parentExtra     = (TmdObject*)parent->extra;
    coord           = extra->coords;
    dest            = &parentExtra->coords[part];
    coord->flg      = 0;
    coord->sub      = dest;
    extra->lightMtx = parentExtra->lightMtx;
    extra->colorMtx = parentExtra->colorMtx;
    Task_Reparent(parent, task);
    task->state += 1;
}

/// Tick state of the second child handler triple `D_actor_310600_80161E30`:
/// does nothing.
void func_actor_310600_801629C4(Task* task)
{
}

void func_actor_310600_801629CC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_310600_80161E3C;
    sp.funcs[task->state](task);
}

void func_actor_310600_80162A24(Task* arg0)
{
    Gp_UnlinkObj(&((Actor310600Work*)arg0->work)->obj);
    Gp_EnemyTaskExit(arg0);
}

void func_actor_310600_80162A58(Task* arg0)
{
    TmdObject*       ext;
    Actor310600Work* work;

    work          = (Actor310600Work*)arg0->work;
    ext           = arg0->extra;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}

/// Entry 0 of the two-entry stack table `func_actor_310600_80161FA0` dispatches
/// through by `field_47C`: the idle handler, which does nothing. Entry 1 is
/// `func_actor_310600_80162A7C`; both are called with no argument.
void func_actor_310600_80162A74(void)
{
}

/// Runs the entry of the actor's second state table that `field_47E` selects -
/// the counter `func_actor_310600_80162AD8` and `func_actor_310600_80162B98`
/// bump as they finish, so the table steps through the handlers in turn. Copies
/// the table onto the stack first, the same dispatch `func_actor_310600_801629CC`
/// performs over `state`.
void func_actor_310600_80162A7C(Task* task)
{
    Actor310600Work* work;
    TaskFuncTable3   fns;

    work = (Actor310600Work*)task->work;
    fns  = D_actor_310600_80161E48;
    fns.funcs[(s16)work->field_47E](task);
}

/// Turns the actor's root part to face the work block's stored point: normalises
/// the offset from the part's own translation, takes its yaw with `ratan2`, and
/// rebuilds the local matrix from that yaw alone. Clearing `flg` makes
/// `_gpUpdateCoordTree` recompute the world matrix from it, and bumping
/// `field_47E` moves the actor on to the next handler of its state table.
void func_actor_310600_80162AD8(Task* task)
{
    Actor310600Work*  work;
    Actor310600Coord* coord;
    VECTOR            delta;
    SVECTOR           dir;
    SVECTOR           rot;

    work  = (Actor310600Work*)task->work;
    coord = (Actor310600Coord*)((TmdObject*)task->extra)->coords;

    delta.vx = work->field_4F8 - coord->coord.t[0];
    delta.vy = work->field_4FC - coord->coord.t[1];
    delta.vz = work->field_500 - coord->coord.t[2];
    VectorNormalS(&delta, &dir);

    rot.vx = 0;
    rot.vy = ratan2(dir.vx, dir.vz);
    rot.vz = 0;

    coord->rot.vx = rot.vx;
    coord->rot.vy = rot.vy;
    coord->rot.vz = rot.vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg = 0;
    work->field_47E++;
}

/// State handler reached by the `field_47E` advance `func_actor_310600_80162AD8`
/// ends with: rotates the constant local-space offset
/// `D_actor_310600_80161E54` through the root part's matrix into `work->step`,
/// opens the per-axis stop threshold to 0x7FFF, which disables it for the update
/// loop, and advances `field_47E` again so the dispatcher runs the next handler.
void func_actor_310600_80162B98(Task* task)
{
    Actor310600Work* work;
    GsCOORDINATE2*   coord;
    VECTOR           vec;

    coord = ((TmdObject*)task->extra)->coords;
    work  = (Actor310600Work*)task->work;

    vec = D_actor_310600_80161E54;
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->step);
    work->limit.vx = 0x7FFF;
    work->limit.vy = 0x7FFF;
    work->limit.vz = 0x7FFF;
    work->field_47E++;
}

/// Places the actor at `args`: the translation goes straight into the root
/// part's local matrix, the Euler angles into the coordinate's `rot` slot, and
/// the rotation is rebuilt from them. Clearing `flg` makes `_gpUpdateCoordTree`
/// recompute the world matrix. `arg1` is unused.
s32 func_actor_310600_80162C18(Task* task, s32 arg1, Actor310600Placement* args)
{
    Actor310600Coord* coord;

    coord             = (Actor310600Coord*)((TmdObject*)task->extra)->coords;
    coord->coord.t[0] = args->pos.vx;
    coord->coord.t[1] = args->pos.vy;
    coord->coord.t[2] = args->pos.vz;
    coord->rot.vx     = args->rot.vx;
    coord->rot.vy     = args->rot.vy;
    coord->rot.vz     = args->rot.vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg = 0;
    return 0;
}

/// Sends the actor walking to the point `arg2`: stores it as the target the
/// movement steps of `D_actor_310600_80161E48` turn toward and close in on,
/// switches the tick onto those steps (`field_47C`), and starts animation 0xC
/// of bank 0 through `func_actor_310600_8016246C`. `arg1` is unused.
void func_actor_310600_80162C94(Task* arg0, s32 arg1, VECTOR* arg2)
{
    Actor310600Work* work;
    Actor310600Cmd   cmd;

    work = (Actor310600Work*)arg0->work;

    work->field_47C = 1;
    work->field_4F8 = arg2->vx;
    work->field_4FC = arg2->vy;
    work->field_500 = arg2->vz;

    cmd.animId = 0;
    cmd.state  = 0xC;
    cmd.path   = 0;
    cmd.param  = 0;
    cmd.unk10  = 0;

    func_actor_310600_8016246C(arg0, 0x7D3, &cmd, 0);
}
