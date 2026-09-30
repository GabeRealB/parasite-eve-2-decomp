#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>

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
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

/// Optional start animation the placement handler takes: the preset's
/// `field_4` and the `model.nextAnimId` byte. Absent, the defaults are anim 3 (or 2
/// once `field_4C4` is set) and 1.
typedef GpSpawnAnimArg Actor350500SpawnAnim;

/// Animation bank table the preset's bank index selects from.
extern AnimationSet*  D_actor_350500_80168E8C[5];
extern AnimationSet** D_actor_350500_80168EA0[1];

/// `Gp_DispatchMsg` handler table installed at `Task::msgTable` by
/// `func_actor_350500_801623CC`; terminator id 0x7FFFFFFF.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*, s32, AnimationPlayRequest*, s32);
        s32 (*call1)(Task*, s32, ActorCommand* request);
        s32 (*call2)(Task*, s32, GpXformArg*);
        s32 (*call3)(Task*, s32, GpXformArg*, Actor350500SpawnAnim*);
        s32 (*call4)(Task*, s32, s32);
    } handler;
} Actor350500MsgEntry;
STATIC_ASSERT_SIZEOF(Actor350500MsgEntry, 8);

extern Actor350500MsgEntry D_actor_350500_80168EB0[];

static void func_actor_350500_80161E50(Task* arg0);
static void func_actor_350500_80162038(Task* arg0);
static void func_actor_350500_801623CC(Task* arg0);
static void func_actor_350500_8016245C(Task* arg0);
static void func_actor_350500_8016247C(Task* arg0);
static void func_actor_350500_80162498(Task* arg0);
static void func_actor_350500_801624A0(Task* arg0);
static void func_actor_350500_80162508(Task* task);
static void func_actor_350500_801625E4(Task* arg0);
static void func_actor_350500_8016272C(Task* arg0);
s32         func_actor_350500_80162828(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3);

/// Spawn, tick and exit handlers, dispatched by `func_actor_350500_80162360`.
static const TaskFuncTable3 D_actor_350500_80161E24 = { {
    func_actor_350500_801623CC,
    func_actor_350500_80161E50,
    func_actor_350500_8016245C,
} };

/// The walk steps, indexed by `Actor350500Work::walk.motionStep`: turn to face
/// `target`, start moving, approach until arrival, then turn to the placement
/// yaw.
static const TaskFuncTable4 D_actor_350500_80161E30 = { {
    func_actor_350500_80162508,
    func_actor_350500_801625E4,
    func_actor_350500_80162038,
    func_actor_350500_8016272C,
} };

/// Local-space offset the start-moving step rotates: straight ahead along
/// the part's own +Z.
static const VECTOR D_actor_350500_80161E40 = { 0, 0, 0x200000, 0 };

extern TmdSource D_actor_350500_8016785C;
s32              func_actor_350500_8016217C(Task*, s32, GpXformArg*, Actor350500SpawnAnim*);
s32              func_actor_350500_80162828(Task*, s32, AnimationPlayRequest*, s32);
s32              func_actor_350500_80162960(Task*, s32, GpXformArg*);
s32              func_actor_350500_801629DC(Task*, s32, s32);
s32              func_actor_350500_80162ABC(Task*, s32, ActorCommand* msg);
void             func_actor_350500_80162360(Task*);

TmdBone D_actor_350500_80162AF8[19] = {
#include "assets/actor_350500_model_05A3C_skeleton.inc"
};

u32 D_actor_350500_80162DA4[19] = {
#include "assets/actor_350500_model_05A3C_partVerts.inc"
};

SVECTOR D_actor_350500_80162DF0[312] = {
#include "assets/actor_350500_model_05A3C_verts.inc"
};

SVECTOR D_actor_350500_801637B0[338] = {
#include "assets/actor_350500_model_05A3C_normals.inc"
};

u32 D_actor_350500_80164240[3463] = {
#include "assets/actor_350500_model_05A3C_stream.inc"
};

TmdSource D_actor_350500_8016785C = {
    0,
    18392,
    6232,
    19,
    D_actor_350500_80162DA4,
    D_actor_350500_80162DF0,
    D_actor_350500_801637B0,
    D_actor_350500_80162AF8,
    D_actor_350500_80164240,
};

AnimationPackedPose D_actor_350500_80167880[2] = {
#include "assets/actor_350500_animation_05C4C_bank1.inc"
};

AnimationPackedRotation D_actor_350500_80167898[23] = {
#include "assets/actor_350500_animation_05C4C_bank4.inc"
};

AnimationRecord D_actor_350500_801678F4[84] = {
#include "assets/actor_350500_animation_05C4C_records.inc"
};

u16 D_actor_350500_80167A44[20] = {
#include "assets/actor_350500_animation_05C4C_indices.inc"
};

AnimationSet D_actor_350500_80167A6C = {
    D_actor_350500_801678F4,
    D_actor_350500_80167A44,
    { NULL, D_actor_350500_80167880, NULL, NULL, D_actor_350500_80167898, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_350500_80167A94[22] = {
#include "assets/actor_350500_animation_06830_bank1.inc"
};

AnimationPackedRotation D_actor_350500_80167B9C[298] = {
#include "assets/actor_350500_animation_06830_bank4.inc"
};

AnimationRecord D_actor_350500_80168044[377] = {
#include "assets/actor_350500_animation_06830_records.inc"
};

u16 D_actor_350500_80168628[20] = {
#include "assets/actor_350500_animation_06830_indices.inc"
};

AnimationSet D_actor_350500_80168650 = {
    D_actor_350500_80168044,
    D_actor_350500_80168628,
    { NULL, D_actor_350500_80167A94, NULL, NULL, D_actor_350500_80167B9C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_350500_80168678[5] = {
#include "assets/actor_350500_animation_06C38_bank1.inc"
};

AnimationPackedRotation D_actor_350500_801686B4[76] = {
#include "assets/actor_350500_animation_06C38_bank4.inc"
};

AnimationRecord D_actor_350500_801687E4[147] = {
#include "assets/actor_350500_animation_06C38_records.inc"
};

u16 D_actor_350500_80168A30[20] = {
#include "assets/actor_350500_animation_06C38_indices.inc"
};

AnimationSet D_actor_350500_80168A58 = {
    D_actor_350500_801687E4,
    D_actor_350500_80168A30,
    { NULL, D_actor_350500_80168678, NULL, NULL, D_actor_350500_801686B4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_350500_80168A80[5] = {
#include "assets/actor_350500_animation_07044_bank1.inc"
};

AnimationPackedRotation D_actor_350500_80168ABC[89] = {
#include "assets/actor_350500_animation_07044_bank4.inc"
};

AnimationRecord D_actor_350500_80168C20[135] = {
#include "assets/actor_350500_animation_07044_records.inc"
};

u16 D_actor_350500_80168E3C[20] = {
#include "assets/actor_350500_animation_07044_indices.inc"
};

AnimationSet D_actor_350500_80168E64 = {
    D_actor_350500_80168C20,
    D_actor_350500_80168E3C,
    { NULL, D_actor_350500_80168A80, NULL, NULL, D_actor_350500_80168ABC, NULL, NULL, NULL },
};

AnimationSet* D_actor_350500_80168E8C[5] = {
    NULL,
    &D_actor_350500_80167A6C,
    &D_actor_350500_80168650,
    &D_actor_350500_80168A58,
    &D_actor_350500_80168E64,
};

AnimationSet** D_actor_350500_80168EA0[1] = {
    D_actor_350500_80168E8C,
};

TaskDesc D_actor_350500_80168EA4 = { 257, 192, func_actor_350500_80162360, { .model = &D_actor_350500_8016785C } };

Actor350500MsgEntry D_actor_350500_80168EB0[6] = {
    { 2003, { .call0 = func_actor_350500_80162828 } },
    { 2004, { .call2 = func_actor_350500_80162960 } },
    { 2005, { .call4 = func_actor_350500_801629DC } },
    { 2013, { .call3 = func_actor_350500_8016217C } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call1 = func_actor_350500_80162ABC } },
    { 0x7FFFFFFF, { .call0 = NULL } },
}; /// Per-frame tick: runs the idle or the walk handler `walk.motion` selects,
/// then integrates the world-space `step` into the 16.16 accumulators at
/// `walk.acc`, adds their high halves to the root coordinate's translation
/// and truncates them back to 16 bits. Ticks the animation slots while
/// `model.ticking` is set, and -- unless the model is hidden -- draws the
/// ground-shadow quad under the second part, clears that part's `composeStamp` and
/// rebuilds its coordinate. The `freeCountdown` countdown then runs while it is
/// non-negative, freeing the model buffers on the frame it reaches zero; the
/// init's -1 disables it.
static void func_actor_350500_80161E50(Task* arg0)
{
    TmdObject*       ext      = arg0->extra.tmd;
    Actor350500Work* work     = (Actor350500Work*)arg0->work;
    TaskFunc         funcs[2] = { func_actor_350500_80162498, func_actor_350500_801624A0 };
    VECTOR3          pos;
    GfxCoord*        coord;
    s32              i;

    funcs[(s16)work->walk.motion](arg0);
    coord                = arg0->extra.tmd->coords;
    work->walk.acc[0].w += work->walk.step.vx;
    work->walk.acc[1].w += work->walk.step.vy;
    work->walk.acc[2].w += work->walk.step.vz;
    coord->coord.t[0]   += (s16)(work->walk.acc[0].w >> 16);
    coord->coord.t[1]   += (s16)(work->walk.acc[1].w >> 16);
    coord->coord.t[2]   += (s16)(work->walk.acc[2].w >> 16);
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    work->walk.acc[0].w  = (u16)work->walk.acc[0].w;
    work->walk.acc[1].w  = (u16)work->walk.acc[1].w;
    work->walk.acc[2].w  = (u16)work->walk.acc[2].w;
    if (work->model.ticking != 0) {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
    }
    if (!(ext->flags & TMD_OBJECT_HIDDEN)) {
        if (func_800EA1A8(MATRIX_TRANS(&arg0->extra.tmd->coords[1].workm), &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x200, Gp_State1C->groundShadowShade);
        }
        arg0->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&arg0->extra.tmd->coords[1]);
        func_800D7A9C(ext, (VECTOR*)arg0->extra.tmd->coords[1].workm.t, 0, 3);
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->freeCountdown--;
    }
}

/// Walk step 2, the approach test. Once the X/Z distances from the root
/// coordinate to `target` stop shrinking below `limit`, plays the `model.nextAnimId`
/// animation, clears `step` and advances `walk.motionStep`; otherwise records the
/// distances as the new `limit`.
static void func_actor_350500_80162038(Task* arg0)
{
    Actor350500Work*     work;
    GfxCoord*            coord;
    SVECTOR              d;
    s32                  dx;
    s32                  dz;
    AnimationPlayRequest preset;

    work  = (Actor350500Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->walk.target.vx - coord->coord.t[0] >= 0) {
        dx = (u16)work->walk.target.vx - (u16)coord->coord.t[0];
    } else {
        dx = (u16)coord->coord.t[0] - (u16)work->walk.target.vx;
    }
    d.vx = dx;
    if (work->walk.target.vz - coord->coord.t[2] >= 0) {
        dz = (u16)work->walk.target.vz - (u16)coord->coord.t[2];
    } else {
        dz = (u16)coord->coord.t[2] - (u16)work->walk.target.vz;
    }
    d.vz = dz;
    if (d.vx >= work->walk.limit.vx && d.vz >= work->walk.limit.vz) {
        preset.source.index         = 0;
        preset.animationId          = work->model.nextAnimId;
        preset.blend                = ANIMATION_BLEND_INTERPOLATE;
        preset.blendFrames          = 5;
        preset.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        func_actor_350500_80162828(arg0, 0x7D3, &preset, 0);
        work->walk.step.vx = 0;
        work->walk.step.vy = 0;
        work->walk.step.vz = 0;
        work->walk.motionStep++;
        return;
    }
    work->walk.limit.vx = d.vx < 0 ? -d.vx : d.vx;
    work->walk.limit.vz = d.vz < 0 ? -d.vz : d.vz;
}

/// Placement message handler: seeds the work block's target and placement
/// rotation from `place`, starts the walk sequence, and picks the start
/// animation from `anim` (or anim 3, 2 once `field_4C4` is set), installing
/// it with the body of `func_actor_350500_80162828` written out inline.
/// Returns 0.
s32 func_actor_350500_8016217C(Task* task, s32 arg1, GpXformArg* place, Actor350500SpawnAnim* anim)
{
    Actor350500Work*      work;
    Actor350500Work*      w;
    AnimationPlayRequest  preset;
    AnimationPlayRequest* msg;
    s32                   i;
    TmdObject*            ext;

    w                   = (Actor350500Work*)task->work;
    w->walk.motion      = 1;
    w->walk.motionStep  = 0;
    w->walk.target.vx   = place->pos.vx;
    w->walk.target.vy   = place->pos.vy;
    w->walk.target.vz   = place->pos.vz;
    w->walk.rotX        = place->rot.vx;
    w->walk.rotY        = place->rot.vy;
    w->walk.rotZ        = place->rot.vz;
    preset.source.index = 0;
    if (anim != NULL) {
        preset.animationId  = anim->field_0;
        w->model.nextAnimId = anim->field_4;
    } else {
        if (w->field_4C4 != 0) {
            preset.animationId = 2;
        } else {
            preset.animationId = 3;
        }
        w->model.nextAnimId = 1;
    }
    preset.blend                = ANIMATION_BLEND_INTERPOLATE;
    preset.blendFrames          = 5;
    preset.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;

    msg  = &preset;
    work = (Actor350500Work*)task->work;
    ext  = task->extra.tmd;
    if (msg->source.index != work->model.bank) {
        work->model.bank   = msg->source.index;
        work->model.animId = -1;
        func_800B3F84(&work->rig.anim, D_actor_350500_80168EA0[work->model.bank], ext, work->rig.poses,
                      work->rig.slots);
    }
    if (msg->animationId != work->model.animId) {
        work->model.animId = msg->animationId;
        if (msg->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
            for (i = 1; i < 0x13; i++) {
                func_800B4114(&work->rig.anim, i, work->model.animId, 0, msg->blendFrames);
            }
        } else {
            for (i = 1; i < 0x13; i++) {
                Gp_AnimResetSlot(&work->rig.anim, i, work->model.animId);
            }
        }
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
        work->model.ticking = 1;
    }
    return 0;
}

/// Per-frame dispatcher: runs the spawn, tick or exit state from
/// `D_actor_350500_80161E24`, skipping the frame while the global freeze
/// byte is set.
void func_actor_350500_80162360(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_350500_80161E24;
    if (Gp_StateF0.field_4 == 0) {
        sp.funcs[task->state](task);
    }
}

/// Spawn state of the enemy actor: allocates the 0x4C8-byte work block that
/// every later handler reads through `Task::work`, seeds the three -1 bytes
/// and the three cleared words the work's own init expects, republishes the
/// light and colour matrices onto the display object, then installs the
/// message table and the exit handler. An allocation failure ends the
/// task instead of leaving a half-built actor behind.
static void func_actor_350500_801623CC(Task* arg0)
{
    Actor350500Work* work;

    work = memCalloc(sizeof(Actor350500Work), false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }

    arg0->work          = work;
    work->model.animId  = -1;
    work->model.bank    = -1;
    work->freeCountdown = -1;
    work->walk.acc[0].w = 0;
    work->walk.acc[1].w = 0;
    work->walk.acc[2].w = 0;

    func_actor_350500_8016247C(arg0);

    arg0->msgTable     = D_actor_350500_80168EB0;
    arg0->exitCallback = func_actor_350500_8016245C;
    arg0->state        = arg0->state + 1;
}

/// Exit callback `func_actor_350500_801623CC` installs; tears the task down.
static void func_actor_350500_8016245C(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

/// Republishes the work block's two matrices onto `TmdObject::lightMtx` /
/// `colorMtx`, so the actor draws with its own lighting.
static void func_actor_350500_8016247C(Task* arg0)
{
    TmdObject*       ext;
    Actor350500Work* work;

    ext           = arg0->extra.tmd;
    work          = (Actor350500Work*)arg0->work;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
}

/// Idle tick handler, selected while `walk.motion` is clear.
static void func_actor_350500_80162498(Task* arg0)
{
}

/// Walk tick handler: runs the step of `D_actor_350500_80161E30` that
/// `walk.motionStep` selects.
static void func_actor_350500_801624A0(Task* arg0)
{
    TaskFuncTable4   sp;
    Actor350500Work* work;

    work = (Actor350500Work*)arg0->work;
    sp   = D_actor_350500_80161E30;
    sp.funcs[(s16)work->walk.motionStep](arg0);
}

/// Walk step 0: turns the root part toward `work->target`, taking the yaw of
/// the normalised offset from the part's own translation with `ratan2` --
/// turned half a revolution away while `field_4C4` is clear -- and rebuilding
/// the local matrix from that yaw alone. Clearing `composeStamp` makes the coordinate
/// tree recompute the world matrix, and bumping `walk.motionStep` moves on to the
/// next step.
static void func_actor_350500_80162508(Task* task)
{
    Actor350500Work* work;
    GfxCoord*        coord;
    VECTOR           delta;
    SVECTOR          dir;
    SVECTOR          rot;

    work  = (Actor350500Work*)task->work;
    coord = (task->extra.tmd)->coords;

    delta.vx = work->walk.target.vx - coord->coord.t[0];
    delta.vy = work->walk.target.vy - coord->coord.t[1];
    delta.vz = work->walk.target.vz - coord->coord.t[2];
    VectorNormalS(&delta, &dir);

    rot.vx = 0;
    rot.vy = ratan2(dir.vx, dir.vz);
    rot.vz = 0;
    if (work->field_4C4 == 0) {
        rot.vy += 0x7FF;
    }

    coord->param.rot.vx = rot.vx;
    coord->param.rot.vy = rot.vy;
    coord->param.rot.vz = rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->walk.motionStep++;
}

/// Walk step 1: rotates `D_actor_350500_80161E40` -- shrunk to -0.4 of its
/// length, a slower backward walk, while `field_4C4` is clear -- through the
/// root part's matrix into `work->step`, opens the per-axis stop threshold to
/// 0x7FFF, which disables it for the approach test, and advances `walk.motionStep`.
static void func_actor_350500_801625E4(Task* arg0)
{
    Actor350500Work* work;
    GfxCoord*        coord;
    VECTOR           vec;

    coord = arg0->extra.tmd->coords;
    work  = (Actor350500Work*)arg0->work;

    vec = D_actor_350500_80161E40;
    if (work->field_4C4 == 0) {
        vec.vx = vec.vx * -0.4;
        vec.vy = vec.vy * -0.4;
        vec.vz = vec.vz * -0.4;
    }
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->walk.step);
    work->walk.limit.vx = 0x7FFF;
    work->walk.limit.vy = 0x7FFF;
    work->walk.limit.vz = 0x7FFF;
    work->walk.motionStep++;
}

/// Walk step 3, the final turn. Euler-extracts the root coordinate into
/// `vec`, and while the yaw gap to the placement yaw `walk.rotY` is at least
/// 0x61 steps `vec.vy` toward it by 0x60; otherwise snaps the yaw to it,
/// plays anim 1 and clears `walk.motion` and `walk.motionStep`, which returns the
/// tick to idle. Either way the root coordinate is rebuilt as the identity
/// rotated by `vec` and its `composeStamp` cleared.
static void func_actor_350500_8016272C(Task* arg0)
{
    Actor350500Work*     work;
    GpMtxWords*          words;
    GfxCoord*            coord;
    SVECTOR              vec;
    AnimationPlayRequest preset;
    s32                  vy;
    s16                  diff;

    coord = arg0->extra.tmd->coords;
    work  = (Actor350500Work*)arg0->work;

    Gp_ExtractEuler(&vec, &coord->coord);
    diff = (u16)work->walk.rotY - (u16)vec.vy;
    if (ABS(diff) >= 0x61) {
        vy = vec.vy;
        if (diff < 0) {
            vec.vy = vy - 0x60;
        } else {
            vec.vy = vy + 0x60;
        }
    } else {
        vec.vy                      = work->walk.rotY;
        preset.source.index         = 0;
        preset.animationId          = 1;
        preset.blend                = ANIMATION_BLEND_INTERPOLATE;
        preset.blendFrames          = 4;
        preset.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        func_actor_350500_80162828(arg0, 0x7D3, &preset, 0);
        work->walk.motion     = 0;
        work->walk.motionStep = 0;
    }

    words          = (GpMtxWords*)&coord->coord;
    words->m00_m01 = ONE;
    words->m02_m10 = 0;
    words->m11_m12 = ONE;
    words->m20_m21 = 0;
    words->m22     = ONE;
    RotMatrix(&vec, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Applies the requested animation bank and clip to this actor's rig.
///
/// A changed bank installs its set table. An unchanged clip skips playback setup.
/// Blends an already ticking rig when requested, using a whole-frame duration;
/// otherwise resets the slots before ticking them.
s32 func_actor_350500_80162828(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3)
{
    Actor350500Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor350500Work*)task->work;
    ext  = task->extra.tmd;
    if (msg->source.index != work->model.bank) {
        work->model.bank   = msg->source.index;
        work->model.animId = -1;
        func_800B3F84(&work->rig.anim, D_actor_350500_80168EA0[work->model.bank], ext, work->rig.poses, work->rig.slots);
    }
    if (msg->animationId != work->model.animId) {
        work->model.animId = msg->animationId;
        if (msg->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
            for (i = 1; i < 0x13; i++) {
                func_800B4114(&work->rig.anim, i, work->model.animId, 0, msg->blendFrames);
            }
        } else {
            for (i = 1; i < 0x13; i++) {
                Gp_AnimResetSlot(&work->rig.anim, i, work->model.animId);
            }
        }
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
        work->model.ticking = 1;
    }
    return 0;
}

/// Message-0x7D4 handler: places the root part at `args`. The translation
/// goes straight into the local matrix, the Euler angles into the
/// coordinate's `rot` slot, from which `RotMatrix` rebuilds the rotation;
/// clearing `composeStamp` makes the world matrix be recomputed. Returns 0.
s32 func_actor_350500_80162960(Task* task, s32 msgId, GpXformArg* args)
{
    GfxCoord* coord;

    coord               = (task->extra.tmd)->coords;
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

/// `Gp_DispatchMsg` handler: the four-way visibility/mode switch on the
/// message's mode word, run against the `TmdObject` parked in `Task::extra`.
/// Mode 0 hides the model (flag 0x80, under which the tick also skips the
/// shadow and the part update) and clears the 4 flag; 1 shows it, allocates
/// the model buffers and clears 4; 2 hides it, sets 4 and latches the mode
/// into the `freeCountdown` countdown, which frees the buffers when it runs out;
/// 3 shows it and sets 4. Anything else returns 1 and leaves the object
/// alone; the handled modes return 0.
s32 func_actor_350500_801629DC(Task* task, s32 arg1, s32 mode)
{
    TmdObject* obj;
    s32        ret;

    obj = task->extra.tmd;
    ret = 0;
    switch (mode) {
        case 0:
            obj->flags |= TMD_OBJECT_HIDDEN;
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 1:
            obj->flags &= ~TMD_OBJECT_HIDDEN;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 2:
            obj->flags                                   |= TMD_OBJECT_HIDDEN;
            ((Actor350500Work*)task->work)->freeCountdown = mode;
            obj->flags                                   |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 3:
            obj->flags &= ~TMD_OBJECT_HIDDEN;
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

/// `Gp_DispatchMsg` handler: latches the variant the message's halfword at
/// 0x2 selects into `field_4C4` -- 1 clears it, 2 sets it, anything else
/// leaves it. Always returns 0.
s32 func_actor_350500_80162ABC(Task* task, s32 arg1, ActorCommand* msg)
{
    Actor350500Work* work;

    work = (Actor350500Work*)task->work;
    switch (msg->command) {
        case 1:
            work->field_4C4 = 0;
            break;
        case 2:
            work->field_4C4 = 1;
            break;
    }
    return 0;
}
