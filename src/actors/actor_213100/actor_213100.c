#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "../../shared/actor_motion.h"

/// Work block the spawn state `func_actor_213100_8014A118` allocates
/// (`memCalloc(0x488)`) and parks in `Task::work` -- that slot is not a
/// `TaskIdMap` here.
///
/// It opens with the animation context the 0x7D3 handler
/// `actorMotionPlayAnim19` drives: the `AnimationContext` at the block's own
/// address, the 0x13 slots above it and the table at 0x30C, the three
/// arguments that handler hands `func_800B3F84`. `field_43C` latches once the
/// slots have been started, and gates the per-frame tick; `field_43E` and
/// `field_43D` hold the current bank index and animation id, seeded to -1 so
/// the first preset always installs. `light` / `color` are the matrices
/// `func_actor_213100_8014A23C` publishes on the model. `field_480` is the
/// child task the spawn state creates, whose model mirrors this one's
/// visibility; `field_484` is the countdown after which the tick frees the
/// model's buffers, -1 while idle.
typedef struct Actor213100Work {
    /* 0x000 */ ActorAnimRig19 rig;
    /* 0x43C */ s8             field_43C;
    /* 0x43D */ s8             field_43D;
    /* 0x43E */ s8             field_43E;
    /* 0x43F */ byte           pad_43F[0x1];
    /* 0x440 */ MATRIX         light;
    /* 0x460 */ MATRIX         color;
    /* 0x480 */ struct Task*   field_480;
    /* 0x484 */ s32            field_484;
} Actor213100Work;
STATIC_ASSERT_SIZEOF(Actor213100Work, 0x488);

/// Animation bank table the 0x7D3 handler indexes with the preset's
/// `field_0`.
extern AnimationSet*  D_actor_213100_8015217C[10];
extern AnimationSet** gActorMotionAnimBanks19[1];

/// Spawn table the spawn state takes its child from; entry 1 is the child,
/// whose body is `func_actor_213100_80149FE4`.
extern TaskDesc D_actor_213100_801521A8[];

/// Message table the spawn state installs at `Task::msgTable`: 0x7D3 is the
/// animation handler `actorMotionPlayAnim19`, 0x7D4 the placement
/// handler `func_actor_213100_8014A390` and 0x7D5 the display handler
/// `func_actor_213100_8014A40C`.
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*, s32, AnimationPlayRequest*, s32);
        s32 (*call1)(Task*, s32, ActorTransform*);
        s32 (*call2)(Task*, s32, s32);
    } handler;
} Actor213100MessageEntry;
STATIC_ASSERT_SIZEOF(Actor213100MessageEntry, 8);

extern Actor213100MessageEntry D_actor_213100_801521C0[4];

/// Per-view visibility table the tick indexes with the session's current
/// view: nonzero shows the actor and its child, zero hides both.
extern s8 D_actor_213100_801521E0[];

static void func_actor_213100_8014A03C(Task* task);
static void func_actor_213100_8014A0B8(Task* task);
static void func_actor_213100_8014A118(Task* arg0);
static void func_actor_213100_8014A21C(Task* arg0);
static void func_actor_213100_8014A23C(Task* arg0);

extern TmdSource D_actor_213100_801501E4;
extern TmdSource D_actor_213100_801503DC;
s32              func_actor_213100_8014A390(Task*, s32, ActorTransform* args);
s32              func_actor_213100_8014A40C(Task*, s32, s32);
void             func_actor_213100_80149FE4(Task*);
void             func_actor_213100_8014A0C0(Task*);

TmdBone D_actor_213100_8014A500[19] = {
#include "assets/actor_213100_model_063C4_skeleton.inc"
};

u32 D_actor_213100_8014A7AC[19] = {
#include "assets/actor_213100_model_063C4_partVerts.inc"
};

SVECTOR D_actor_213100_8014A7F8[394] = {
#include "assets/actor_213100_model_063C4_verts.inc"
};

SVECTOR D_actor_213100_8014B448[394] = {
#include "assets/actor_213100_model_063C4_normals.inc"
};

u32 D_actor_213100_8014C098[4179] = {
#include "assets/actor_213100_model_063C4_stream.inc"
};

TmdSource D_actor_213100_801501E4 = {
    0,
    22872,
    6804,
    19,
    D_actor_213100_8014A7AC,
    D_actor_213100_8014A7F8,
    D_actor_213100_8014B448,
    D_actor_213100_8014A500,
    D_actor_213100_8014C098,
};

TmdBone D_actor_213100_80150208[1] = {
#include "assets/actor_213100_model_065BC_skeleton.inc"
};

u32 D_actor_213100_8015022C[1] = {
#include "assets/actor_213100_model_065BC_partVerts.inc"
};

SVECTOR D_actor_213100_80150230[14] = {
#include "assets/actor_213100_model_065BC_verts.inc"
};

u32 D_actor_213100_801502A0[79] = {
#include "assets/actor_213100_model_065BC_stream.inc"
};

TmdSource D_actor_213100_801503DC = {
    0,
    528,
    0,
    1,
    D_actor_213100_8015022C,
    D_actor_213100_80150230,
    &D_actor_213100_80150230[14],
    D_actor_213100_80150208,
    D_actor_213100_801502A0,
};

AnimationPackedPose D_actor_213100_80150400[6] = {
#include "assets/actor_213100_animation_068BC_bank1.inc"
};

AnimationPackedRotation D_actor_213100_80150448[46] = {
#include "assets/actor_213100_animation_068BC_bank4.inc"
};

AnimationRecord D_actor_213100_80150500[109] = {
#include "assets/actor_213100_animation_068BC_records.inc"
};

u16 D_actor_213100_801506B4[20] = {
#include "assets/actor_213100_animation_068BC_indices.inc"
};

AnimationSet D_actor_213100_801506DC = {
    D_actor_213100_80150500,
    D_actor_213100_801506B4,
    { NULL, D_actor_213100_80150400, NULL, NULL, D_actor_213100_80150448, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_213100_80150704[8] = {
#include "assets/actor_213100_animation_06C90_bank1.inc"
};

AnimationPackedRotation D_actor_213100_80150764[84] = {
#include "assets/actor_213100_animation_06C90_bank4.inc"
};

AnimationRecord D_actor_213100_801508B4[117] = {
#include "assets/actor_213100_animation_06C90_records.inc"
};

u16 D_actor_213100_80150A88[20] = {
#include "assets/actor_213100_animation_06C90_indices.inc"
};

AnimationSet D_actor_213100_80150AB0 = {
    D_actor_213100_801508B4,
    D_actor_213100_80150A88,
    { NULL, D_actor_213100_80150704, NULL, NULL, D_actor_213100_80150764, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_213100_80150AD8[7] = {
#include "assets/actor_213100_animation_07080_bank1.inc"
};

AnimationPackedRotation D_actor_213100_80150B2C[62] = {
#include "assets/actor_213100_animation_07080_bank4.inc"
};

AnimationRecord D_actor_213100_80150C24[149] = {
#include "assets/actor_213100_animation_07080_records.inc"
};

u16 D_actor_213100_80150E78[20] = {
#include "assets/actor_213100_animation_07080_indices.inc"
};

AnimationSet D_actor_213100_80150EA0 = {
    D_actor_213100_80150C24,
    D_actor_213100_80150E78,
    { NULL, D_actor_213100_80150AD8, NULL, NULL, D_actor_213100_80150B2C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_213100_80150EC8[7] = {
#include "assets/actor_213100_animation_073EC_bank1.inc"
};

AnimationPackedRotation D_actor_213100_80150F1C[74] = {
#include "assets/actor_213100_animation_073EC_bank4.inc"
};

AnimationRecord D_actor_213100_80151044[104] = {
#include "assets/actor_213100_animation_073EC_records.inc"
};

u16 D_actor_213100_801511E4[20] = {
#include "assets/actor_213100_animation_073EC_indices.inc"
};

AnimationSet D_actor_213100_8015120C = {
    D_actor_213100_80151044,
    D_actor_213100_801511E4,
    { NULL, D_actor_213100_80150EC8, NULL, NULL, D_actor_213100_80150F1C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_213100_80151234[9] = {
#include "assets/actor_213100_animation_0793C_bank1.inc"
};

AnimationPackedRotation D_actor_213100_801512A0[100] = {
#include "assets/actor_213100_animation_0793C_bank4.inc"
};

AnimationRecord D_actor_213100_80151430[193] = {
#include "assets/actor_213100_animation_0793C_records.inc"
};

u16 D_actor_213100_80151734[20] = {
#include "assets/actor_213100_animation_0793C_indices.inc"
};

AnimationSet D_actor_213100_8015175C = {
    D_actor_213100_80151430,
    D_actor_213100_80151734,
    { NULL, D_actor_213100_80151234, NULL, NULL, D_actor_213100_801512A0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_213100_80151784[7] = {
#include "assets/actor_213100_animation_07CEC_bank1.inc"
};

AnimationPackedRotation D_actor_213100_801517D8[81] = {
#include "assets/actor_213100_animation_07CEC_bank4.inc"
};

AnimationRecord D_actor_213100_8015191C[114] = {
#include "assets/actor_213100_animation_07CEC_records.inc"
};

u16 D_actor_213100_80151AE4[20] = {
#include "assets/actor_213100_animation_07CEC_indices.inc"
};

AnimationSet D_actor_213100_80151B0C = {
    D_actor_213100_8015191C,
    D_actor_213100_80151AE4,
    { NULL, D_actor_213100_80151784, NULL, NULL, D_actor_213100_801517D8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_213100_80151B34[3] = {
#include "assets/actor_213100_animation_07EB0_bank1.inc"
};

AnimationPackedRotation D_actor_213100_80151B58[27] = {
#include "assets/actor_213100_animation_07EB0_bank4.inc"
};

AnimationRecord D_actor_213100_80151BC4[57] = {
#include "assets/actor_213100_animation_07EB0_records.inc"
};

u16 D_actor_213100_80151CA8[20] = {
#include "assets/actor_213100_animation_07EB0_indices.inc"
};

AnimationSet D_actor_213100_80151CD0 = {
    D_actor_213100_80151BC4,
    D_actor_213100_80151CA8,
    { NULL, D_actor_213100_80151B34, NULL, NULL, D_actor_213100_80151B58, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_213100_80151CF8[4] = {
#include "assets/actor_213100_animation_08170_bank1.inc"
};

AnimationPackedRotation D_actor_213100_80151D28[40] = {
#include "assets/actor_213100_animation_08170_bank4.inc"
};

AnimationRecord D_actor_213100_80151DC8[104] = {
#include "assets/actor_213100_animation_08170_records.inc"
};

u16 D_actor_213100_80151F68[20] = {
#include "assets/actor_213100_animation_08170_indices.inc"
};

AnimationSet D_actor_213100_80151F90 = {
    D_actor_213100_80151DC8,
    D_actor_213100_80151F68,
    { NULL, D_actor_213100_80151CF8, NULL, NULL, D_actor_213100_80151D28, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_213100_80151FB8[3] = {
#include "assets/actor_213100_animation_08334_bank1.inc"
};

AnimationPackedRotation D_actor_213100_80151FDC[27] = {
#include "assets/actor_213100_animation_08334_bank4.inc"
};

AnimationRecord D_actor_213100_80152048[57] = {
#include "assets/actor_213100_animation_08334_records.inc"
};

u16 D_actor_213100_8015212C[20] = {
#include "assets/actor_213100_animation_08334_indices.inc"
};

AnimationSet D_actor_213100_80152154 = {
    D_actor_213100_80152048,
    D_actor_213100_8015212C,
    { NULL, D_actor_213100_80151FB8, NULL, NULL, D_actor_213100_80151FDC, NULL, NULL, NULL },
};

AnimationSet* D_actor_213100_8015217C[10] = {
    NULL,
    &D_actor_213100_801506DC,
    &D_actor_213100_80150AB0,
    &D_actor_213100_80150EA0,
    &D_actor_213100_8015120C,
    &D_actor_213100_8015175C,
    &D_actor_213100_80151B0C,
    &D_actor_213100_80151CD0,
    &D_actor_213100_80151F90,
    &D_actor_213100_80152154,
};

AnimationSet** gActorMotionAnimBanks19[1] = {
    D_actor_213100_8015217C,
};

TaskDesc D_actor_213100_801521A8[2] = {
    { TASK_BODY_TMD, 192, func_actor_213100_8014A0C0, { .model = &D_actor_213100_801501E4 } },
    { TASK_BODY_TMD, 192, func_actor_213100_80149FE4, { .model = &D_actor_213100_801503DC } },
};

Actor213100MessageEntry D_actor_213100_801521C0[4] = {
    { 2003, { .call0 = actorMotionPlayAnim19 } },
    { 2004, { .call1 = func_actor_213100_8014A390 } },
    { 2005, { .call2 = func_actor_213100_8014A40C } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

s8 D_actor_213100_801521E0[24] = {
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

static void func_actor_213100_80149E3C(Task* task);

/// Per-frame tick: ticks the work block's animation slots once they have been
/// started, and while the model is shown samples the child part's
/// translation through `func_800EA1A8` and draws the ground shadow where it
/// hits. Once the view is ready, rebuilds that part's world matrix, hands its
/// translation to `func_800D7A9C`, and shows or hides this model and the
/// child's together from the per-view table. The work block's countdown then
/// frees the model's buffers as it reaches zero.
static void func_actor_213100_80149E3C(Task* task)
{
    Actor213100Work* work;
    TmdObject*       extra;
    TmdObject*       child;
    VECTOR3          pos;
    s32              i;

    work  = (Actor213100Work*)task->work;
    extra = task->extra.tmd;
    if (work->field_43C != 0) {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
    }
    if (!(extra->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (func_800EA1A8(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x300, Gp_State1C->groundShadowShade);
        }
    }
    if (gGameSession->viewReady != 0) {
        task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&task->extra.tmd->coords[1]);
        func_800D7A9C(extra, (VECTOR*)task->extra.tmd->coords[1].workm.t, 0, 3);
        child = work->field_480->extra.tmd;
        if (D_actor_213100_801521E0[gGameSession->location.loc.view] != 0) {
            extra->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            child->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        } else {
            extra->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            child->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        }
    }
    if (work->field_484 >= 0) {
        if (work->field_484 == 0) {
            Tmd_FreeBuffers(extra);
        }
        work->field_484--;
    }
}

/// State table of the child the spawn state creates: attach to the parent,
/// idle, kill.
static const TaskFuncTable3 D_actor_213100_80149E24 = {
    {
        func_actor_213100_8014A03C,
        func_actor_213100_8014A0B8,
        taskKill,
    },
};

/// Body of the child task: dispatches on its state through
/// `D_actor_213100_80149E24`.
void func_actor_213100_80149FE4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213100_80149E24;
    sp.funcs[task->state](task);
}

/// State 0 of the child: chains the child's root coordinate under the
/// parent's skeleton part named by the spawn arguments (the parent task and
/// the part index it was spawned with), inherits the parent's light and colour
/// matrices, reparents the task so it runs with the parent, and advances to
/// the idle state.
static void func_actor_213100_8014A03C(Task* task)
{
    Task*      parent;
    s32        part;
    TmdObject* extra;
    TmdObject* parentExtra;
    GfxCoord*  coord;
    GfxCoord*  dest;

    parent              = (Task*)task->spawnArg2.pointer;
    part                = task->spawnArg1.value;
    extra               = task->extra.tmd;
    parentExtra         = parent->extra.tmd;
    coord               = extra->coords;
    dest                = &parentExtra->coords[part];
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->parent       = dest;
    extra->lightMtx     = parentExtra->lightMtx;
    extra->colorMtx     = parentExtra->colorMtx;
    Task_Reparent(parent, task);
    task->state += 1;
}

/// The child's idle state: does nothing.
static void func_actor_213100_8014A0B8(Task* task)
{
}

/// The actor's three states: spawn, per-frame tick and teardown.
static const TaskFuncTable3 D_actor_213100_80149E30 = {
    {
        func_actor_213100_8014A118,
        func_actor_213100_80149E3C,
        func_actor_213100_8014A21C,
    },
};

/// Body of the actor's task: dispatches on its state through
/// `D_actor_213100_80149E30`.
void func_actor_213100_8014A0C0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213100_80149E30;
    sp.funcs[task->state](task);
}

/// Spawn state: allocates the work block into `Task::work` and seeds its
/// animation bytes and countdown to -1, then spawns the child from entry 1 of
/// the spawn table, attached to part 8 of this actor's skeleton. Either
/// allocation failing exits the task instead. Both models start hidden and
/// take the work block's matrices; the actor starts its animation by calling
/// the 0x7D3 handler directly with the preset `{ 0, 5, 0, 0, 0 }`, then
/// installs its message table and exit callback and advances to the tick.
static void func_actor_213100_8014A118(Task* arg0)
{
    Actor213100Work*     work;
    AnimationPlayRequest preset;
    TmdObject*           ext;
    Task*                child;

    work = (Actor213100Work*)memCalloc(0x488, 0);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }
    arg0->work      = work;
    work->field_43D = -1;
    work->field_43E = -1;
    work->field_484 = -1;
    child           = Task_SpawnFromTable(D_actor_213100_801521A8, 1, 8, arg0);
    work->field_480 = child;
    if (child == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }
    func_actor_213100_8014A23C(arg0);
    ext                         = arg0->extra.tmd;
    ext->flags                 |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    ext                         = work->field_480->extra.tmd;
    ext->flags                 |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    preset.source.index         = 0;
    preset.animationId          = 5;
    preset.blend                = ANIMATION_BLEND_RESET;
    preset.blendFrames          = 0;
    preset.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    actorMotionPlayAnim19(arg0, 0, &preset, 0);
    arg0->msgTable     = D_actor_213100_801521C0;
    arg0->exitCallback = func_actor_213100_8014A21C;
    arg0->state++;
}

/// The actor's teardown state and its `Task::exitCallback`: hands the task to
/// `Gp_EnemyTaskExit`.
static void func_actor_213100_8014A21C(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

/// Points the model's light and colour matrices at the work block's own pair.
static void func_actor_213100_8014A23C(Task* arg0)
{
    TmdObject*       ext;
    Actor213100Work* work;

    ext           = arg0->extra.tmd;
    work          = (Actor213100Work*)arg0->work;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}

#include "../../shared/actor_motion_play19.inc.c"

/// Message-0x7D4 handler: places the actor at the message's arguments -
/// the translation goes straight into the root coordinate's local matrix, the
/// Euler angles into the coordinate's `rot` slot, from which the rotation is
/// rebuilt. Clearing `composeStamp` has the world matrix recomputed. Returns 0.
s32 func_actor_213100_8014A390(Task* task, s32 arg1, ActorTransform* args)
{
    GfxCoord* coord;

    coord               = task->extra.tmd->coords;
    coord->coord.t[0]   = args->pos.vx;
    coord->coord.t[1]   = args->pos.vy;
    coord->coord.t[2]   = args->pos.vz;
    coord->param.rot.vx = args->rot.vx;
    coord->param.rot.vy = args->rot.vy;
    coord->param.rot.vz = args->rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}

/// Message-0x7D5 display handler: switches on the message's mode word, then
/// copies the model's flags onto the child's model. Mode 0 hides the model
/// and clears flag 0x4; 1 shows it, reallocates its buffers through
/// `Tmd_AllocBuffers` and clears 0x4; 2 hides it, sets 0x4 and starts the
/// work block's countdown at 2, after which the tick frees the buffers; 3
/// shows it and sets 0x4. The handled modes return 0; any other mode changes
/// nothing on this model and returns 1.
s32 func_actor_213100_8014A40C(Task* task, s32 arg1, s32 mode)
{
    TmdObject*       obj;
    TmdObject*       other;
    Actor213100Work* work;
    s32              ret;

    obj   = task->extra.tmd;
    work  = (Actor213100Work*)task->work;
    other = work->field_480->extra.tmd;
    ret   = 0;
    switch (mode) {
        case 0:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 1:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 2:
            obj->flags     |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->field_484 = mode;
            obj->flags     |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 3:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            ret = 1;
            break;
    }
    other->flags = obj->flags;
    return ret;
}
