#include "actors/actor_310600.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
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

#include "rooms/acropolis_cafeteria.h"
#include "../../shared/model_placement.h"
#include "../../shared/actor_messages.h"

/// 0x538-byte work block `func_actor_310600_80161E64` allocates with
/// `memCalloc` and hangs off `Task::work`. The display node at `obj` is
/// linked by `Gp_LinkObj` at spawn (its `context.contacts` points at `rec`, the
/// `WorldCollisionContact` table `Gp_InitRec18Table` fills) and unlinked again by the
/// exit callback `func_actor_310600_80162A24`.
///
/// `light` / `color` are the actor's own lighting and colour matrices;
/// `func_actor_310600_80162A58` republishes them onto the model's
/// `TmdObject::lightMtx` / `colorMtx` in place of the shared defaults
/// `Gp_BindDefaultMtx` installs.
///
/// The block is fronted by the animation context `func_actor_310600_8016246C`
/// drives: the `AnimationContext` (`func_800B3F84` takes the block address), the
/// twenty `AnimationSlot`s immediately above it, and the 0x140-byte table
/// `func_800B3F84` also takes at 0x334. `field_474` is the once-only latch the
/// slots are started through, and `field_476` / `field_475` are the animation
/// bank index and the animation id, latched on change and re-read from the
/// block by the loops below them.
typedef struct Actor310600Work {
    /* 0x000 */ ActorAnimRig20        rig;
    /* 0x474 */ s8                    field_474;
    /* 0x475 */ s8                    field_475;
    /* 0x476 */ s8                    field_476;
    /* 0x477 */ s8                    field_477;
    /* 0x478 */ s16                   field_478;
    /* 0x47A */ s16                   field_47A;
    /* 0x47C */ s16                   field_47C;
    /* 0x47E */ u16                   field_47E;
    /* 0x480 */ MATRIX                light;
    /* 0x4A0 */ MATRIX                color;
    /* 0x4C0 */ WorldCollisionBody    obj;
    /* 0x4E0 */ WorldCollisionContact rec;
    /* 0x4F8 */ s32                   field_4F8;
    /* 0x4FC */ s32                   field_4FC;
    /* 0x500 */ s32                   field_500;
    /* 0x504 */ byte                  pad_504[0x4];
    /* 0x508 */ VECTOR3               step; // local-space offset `ApplyMatrixLV` rotates into world space
    /* 0x514 */ byte                  pad_514[0x4];
    /* 0x518 */ s32                   field_518;
    /* 0x51C */ s32                   field_51C;
    /* 0x520 */ s32                   field_520;
    /* 0x524 */ byte                  pad_524[0x4];
    /* 0x528 */ SVECTOR               limit; // per-axis stop threshold; 0x7FFF on all three disables it
    /* 0x530 */ byte                  pad_530[0x8];
} Actor310600Work;
STATIC_ASSERT_SIZEOF(Actor310600Work, 0x538);

/// Spawn table entry 1 is this actor's `Task::state` dispatcher; the type-1
/// setup entry it is spawned from is `func_actor_310600_80161E64`.
extern TaskDesc D_actor_310600_801796A4[];

/// The overlay's `TaskMessageEntry` table, parked in `Task::msgTable`.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
typedef struct {
    s32 id;
    union {
        s32                (*call0)(Task*, s32, AnimationPlayRequest*, s32);
        s32                (*call1)(Task*, s32, ActorTransform*);
        TaskMessageHandler call2;
        void               (*call3)(Task*, s32, VECTOR*);
    } handler;
} Actor310600MsgEntry;
STATIC_ASSERT_SIZEOF(Actor310600MsgEntry, 8);

extern Actor310600MsgEntry D_actor_310600_801796BC[];

/// Per-animation cue lists: `D_actor_310600_80179660[field_475]` is a
/// zero-terminated list of the frames at which that animation fires its effect.
extern s16*    D_actor_310600_80179660[];
extern SVECTOR D_actor_310600_80179694;
extern s32     D_actor_310600_8017969C;
extern s32     D_actor_310600_801796A0;

extern AnimationSet*  D_actor_310600_8017962C[5];
extern AnimationSet** D_actor_310600_80179640[1]; // animation bank table `work->field_476` indexes
extern s8             D_actor_310600_80179644[];  // extra ticks owed to the animation id in `work->field_475`

/// Spawn table of the follow-up task queued once the cue has fired five times.

static void func_actor_310600_80161E64(Task* task);
static void func_actor_310600_80161FA0(Task* task);
static void func_actor_310600_8016231C(Task* arg0);
s32         func_actor_310600_8016246C(Task* task, s32 arg1, AnimationPlayRequest* cmd, s32 arg3);
s32         func_actor_310600_801625F0(Task* task, s32 arg1, s32 arg2, s32 arg3);
static void func_actor_310600_801629C4(Task* task);
static void func_actor_310600_80162A24(Task* arg0);
static void func_actor_310600_80162A58(Task* arg0);
static void func_actor_310600_80162A74(Task* task);
static void func_actor_310600_80162A7C(Task* task);
static void func_actor_310600_80162AD8(Task* task);
static void func_actor_310600_80162B98(Task* task);

/// State handlers of the child part task, which `func_actor_310600_8016274C`
/// runs by `Task::state`: setup, tick and exit.
static const TaskFuncTable3 D_actor_310600_80161E24 = { {
    modelPlacementAttachChild,
    modelPlacementMirrorParent,
    taskKill,
} };

/// A second child handler triple - attach to the parent's part, an empty tick,
/// exit. No dispatcher in this package reads it.
static const TaskFuncTable3 D_actor_310600_80161E30 = { {
    modelPlacementAttachPart,
    func_actor_310600_801629C4,
    taskKill,
} };

/// The actor's own state handlers, which `func_actor_310600_801629CC` runs by
/// `Task::state`: spawn/setup, per-frame tick and teardown.
static const TaskFuncTable3 D_actor_310600_80161E3C = { {
    func_actor_310600_80161E64,
    func_actor_310600_80161FA0,
    func_actor_310600_80162A24,
} };

/// The actor's movement steps, which `func_actor_310600_80162A7C` runs by
/// `field_47E`: turn to face the target point, start moving, stop on arrival.
static const TaskFuncTable3 D_actor_310600_80161E48 = { {
    func_actor_310600_80162AD8,
    func_actor_310600_80162B98,
    func_actor_310600_8016231C,
} };

/// The constant local-space offset `func_actor_310600_80162B98` rotates,
/// `{ 0, 0, 0x200000, 0 }` -- straight ahead along the part's own +Z.
static const VECTOR D_actor_310600_80161E54 = { 0, 0, 0x200000, 0 };

extern TmdSource D_actor_310600_8016C7F8;
extern TmdSource D_actor_310600_8016CD50;
void             func_actor_310600_8016274C(Task*);
void             func_actor_310600_801629CC(Task*);

s32  func_actor_310600_8016246C(Task*, s32, AnimationPlayRequest*, s32);
s32  func_actor_310600_801625F0(Task*, s32, s32, s32);
void func_actor_310600_80162C94(Task*, s32, VECTOR*);

AnimationPackedPose D_actor_310600_80162CF8[102] = {
#include "assets/actor_310600_animation_04ADC_bank1.inc"
};

AnimationPackedRotation D_actor_310600_801631C0[1555] = {
#include "assets/actor_310600_animation_04ADC_bank4.inc"
};

AnimationRecord D_actor_310600_80164A0C[1970] = {
#include "assets/actor_310600_animation_04ADC_records.inc"
};

u16 D_actor_310600_801668D4[20] = {
#include "assets/actor_310600_animation_04ADC_indices.inc"
};

AnimationSet D_actor_310600_801668FC = {
    D_actor_310600_80164A0C,
    D_actor_310600_801668D4,
    { NULL, D_actor_310600_80162CF8, NULL, NULL, D_actor_310600_801631C0, NULL, NULL, NULL },
};

TmdBone D_actor_310600_80166924[20] = {
#include "assets/actor_310600_model_0A9D8_skeleton.inc"
};

u32 D_actor_310600_80166BF4[20] = {
#include "assets/actor_310600_model_0A9D8_partVerts.inc"
};

SVECTOR D_actor_310600_80166C44[386] = {
#include "assets/actor_310600_model_0A9D8_verts.inc"
};

SVECTOR D_actor_310600_80167854[385] = {
#include "assets/actor_310600_model_0A9D8_normals.inc"
};

u32 D_actor_310600_8016845C[4327] = {
#include "assets/actor_310600_model_0A9D8_stream.inc"
};

TmdSource D_actor_310600_8016C7F8 = {
    0,
    23980,
    6012,
    20,
    D_actor_310600_80166BF4,
    D_actor_310600_80166C44,
    D_actor_310600_80167854,
    D_actor_310600_80166924,
    D_actor_310600_8016845C,
};

TmdBone D_actor_310600_8016C81C[1] = {
#include "assets/actor_310600_model_0AF30_skeleton.inc"
};

u32 D_actor_310600_8016C840[1] = {
#include "assets/actor_310600_model_0AF30_partVerts.inc"
};

SVECTOR D_actor_310600_8016C844[28] = {
#include "assets/actor_310600_model_0AF30_verts.inc"
};

SVECTOR D_actor_310600_8016C924[28] = {
#include "assets/actor_310600_model_0AF30_normals.inc"
};

u32 D_actor_310600_8016CA04[211] = {
#include "assets/actor_310600_model_0AF30_stream.inc"
};

TmdSource D_actor_310600_8016CD50 = {
    0,
    1464,
    0,
    1,
    D_actor_310600_8016C840,
    D_actor_310600_8016C844,
    D_actor_310600_8016C924,
    D_actor_310600_8016C81C,
    D_actor_310600_8016CA04,
};

AnimationPackedPose D_actor_310600_8016CD74[73] = {
#include "assets/actor_310600_animation_0E6B0_bank1.inc"
};

AnimationPackedRotation D_actor_310600_8016D0E0[1085] = {
#include "assets/actor_310600_animation_0E6B0_bank4.inc"
};

AnimationRecord D_actor_310600_8016E1D4[2229] = {
#include "assets/actor_310600_animation_0E6B0_records.inc"
};

u16 D_actor_310600_801704A8[20] = {
#include "assets/actor_310600_animation_0E6B0_indices.inc"
};

AnimationSet D_actor_310600_801704D0 = {
    D_actor_310600_8016E1D4,
    D_actor_310600_801704A8,
    { NULL, D_actor_310600_8016CD74, NULL, NULL, D_actor_310600_8016D0E0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_310600_801704F8[8] = {
#include "assets/actor_310600_animation_0EB84_bank1.inc"
};

AnimationPackedRotation D_actor_310600_80170558[115] = {
#include "assets/actor_310600_animation_0EB84_bank4.inc"
};

AnimationRecord D_actor_310600_80170724[150] = {
#include "assets/actor_310600_animation_0EB84_records.inc"
};

u16 D_actor_310600_8017097C[20] = {
#include "assets/actor_310600_animation_0EB84_indices.inc"
};

AnimationSet D_actor_310600_801709A4 = {
    D_actor_310600_80170724,
    D_actor_310600_8017097C,
    { NULL, D_actor_310600_801704F8, NULL, NULL, D_actor_310600_80170558, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_310600_801709CC[127] = {
#include "assets/actor_310600_animation_15668_bank1.inc"
};

AnimationPackedRotation D_actor_310600_80170FC0[2329] = {
#include "assets/actor_310600_animation_15668_bank4.inc"
};

AnimationRecord D_actor_310600_80173424[4111] = {
#include "assets/actor_310600_animation_15668_records.inc"
};

u16 D_actor_310600_80177460[20] = {
#include "assets/actor_310600_animation_15668_indices.inc"
};

AnimationSet D_actor_310600_80177488 = {
    D_actor_310600_80173424,
    D_actor_310600_80177460,
    { NULL, D_actor_310600_801709CC, NULL, NULL, D_actor_310600_80170FC0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_310600_801774B0[68] = {
#include "assets/actor_310600_animation_177E4_bank1.inc"
};

AnimationPackedRotation D_actor_310600_801777E0[781] = {
#include "assets/actor_310600_animation_177E4_bank4.inc"
};

AnimationRecord D_actor_310600_80178414[1138] = {
#include "assets/actor_310600_animation_177E4_records.inc"
};

u16 D_actor_310600_801795DC[20] = {
#include "assets/actor_310600_animation_177E4_indices.inc"
};

AnimationSet D_actor_310600_80179604 = {
    D_actor_310600_80178414,
    D_actor_310600_801795DC,
    { NULL, D_actor_310600_801774B0, NULL, NULL, D_actor_310600_801777E0, NULL, NULL, NULL },
};

AnimationSet* D_actor_310600_8017962C[5] = {
    NULL,
    &D_actor_310600_801704D0,
    &D_actor_310600_801709A4,
    &D_actor_310600_80177488,
    &D_actor_310600_80179604,
};

AnimationSet** D_actor_310600_80179640[1] = {
    D_actor_310600_8017962C,
};

// Cafeteria's scripts select bank 0 animations 1..4. The retained movement
// handler requests 12/13, but has no caller in those scripts or companion actors.
s8 D_actor_310600_80179644[8] = {
    0,
    2,
    0,
    0,
    0,
    0,
    0,
    0,
};

s16 D_actor_310600_8017964C[7] = {
    74,
    95,
    117,
    137,
    159,
    223,
    0,
};

s16 D_actor_310600_8017965C[2] = {
    20,
    0,
};

s16* D_actor_310600_80179660[11] = {
    NULL,
    D_actor_310600_8017964C,
    NULL,
    D_actor_310600_8017965C,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

SVECTOR D_actor_310600_8017968C = { -30, 450, 140, 0 };

SVECTOR D_actor_310600_80179694 = { -78, 80, 83, 0 };

s32 D_actor_310600_8017969C = 0x60401;

s32 D_actor_310600_801796A0 = 0x70401;

TaskDesc D_actor_310600_801796A4[2] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_310600_801629CC, { .model = &D_actor_310600_8016C7F8 } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_310600_8016274C, { .model = &D_actor_310600_8016CD50 } },
};

Actor310600MsgEntry D_actor_310600_801796BC[5] = {
    { 2003, { .call0 = func_actor_310600_8016246C } },
    { 2004, { .call1 = actorMsgPlaceEuler } },
    { 2005, { .call2 = func_actor_310600_801625F0 } },
    { 2013, { .call3 = func_actor_310600_80162C94 } },
    { TASK_MESSAGE_TABLE_END, { .call0 = NULL } },
};

static void func_actor_310600_80161E64(Task* task)
{
    Actor310600Work*    work;
    WorldCollisionBody* obj;

    work = memCalloc(0x538, 0);
    if (work == NULL) {
        Gp_EnemyTaskExit(task);
        return;
    }
    task->work      = work;
    work->field_475 = -1;
    work->field_476 = -1;
    work->field_477 = -1;
    work->field_47C = 0;
    work->field_47E = 0;
    work->field_518 = 0;
    work->field_51C = 0;
    work->field_520 = 0;
    Task_SpawnFromTable(D_actor_310600_801796A4, 1, 8, task);
    func_actor_310600_80162A58(task);
    obj                   = &work->obj;
    obj->coord            = &task->extra.tmd->coords[1];
    obj->context.contacts = &work->rec;
    obj->key              = 0x30000;
    obj->radius           = 0x100;
    obj->pos.vx           = 0;
    obj->pos.vy           = 0;
    obj->pos.vz           = 0;
    obj->flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, obj);
    obj->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_InitRec18Table(obj->context.contacts, 1, 0);
    task->msgTable = D_actor_310600_801796BC;
    func_actor_310600_801625F0(task, 0x7D5, 0, 0);
    task->exitCallback = func_actor_310600_80162A24;
    task->state++;
}

/// The actor's per-frame handler. Runs the entry of its second state table that
/// `field_47C` selects, then advances the root part by `step`: each axis'
/// accumulator carries a 16.16 offset whose whole part is added to the world
/// translation and whose fraction is kept, and clearing `composeStamp` makes
/// `actorRenderComposeCoordChain` rebuild the composed matrix from it.
///
/// Once the slots have been started (`field_474`) every animation slot is
/// ticked, and the frame counter `field_478` is walked against the cue list
/// `D_actor_310600_80179660[field_475]` -- a zero-terminated list of frames at
/// which the animation currently playing fires an effect. The effect is chosen
/// by the animation id: ids 1 and 2 spawn 0x6006A and ask slot 4 for the
/// follow-up message, but only for the first five of them, after which the
/// other payload is sent and `D_acropolis_cafeteria_80182AD8` is spawned instead; id 3 spawns
/// 0x6006D. The remaining ids have no cue.
///
/// While the model is visible its ground shadow is drawn at the root part's
/// world position and the occupancy table is cleared, and while the session
/// flag at `field_4D` is set the second part is re-derived and re-lit.
/// `field_477` is the teardown countdown: it frees the model buffers on the
/// tick it reaches zero and then stops at -1.
static void func_actor_310600_80161FA0(Task* task)
{
    TmdObject*       ext      = task->extra.tmd;
    Actor310600Work* work     = (Actor310600Work*)task->work;
    TaskFunc         funcs[2] = { func_actor_310600_80162A74, func_actor_310600_80162A7C };
    VECTOR3          pos;
    GfxCoord*        coord;
    s16*             cues;
    s16*             cue;
    s32              i;

    funcs[work->field_47C](task);
    coord               = task->extra.tmd->coords;
    work->field_518    += work->step.vx;
    work->field_51C    += work->step.vy;
    work->field_520    += work->step.vz;
    coord->coord.t[0]  += (s16)(work->field_518 >> 16);
    coord->coord.t[1]  += (s16)(work->field_51C >> 16);
    coord->coord.t[2]  += (s16)(work->field_520 >> 16);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->field_518     = (u16)work->field_518;
    work->field_51C     = (u16)work->field_51C;
    work->field_520     = (u16)work->field_520;
    if (work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
    }
    if (work->field_475 > 0) {
        cues = D_actor_310600_80179660[work->field_475];
        if (cues != NULL) {
            if (*cues != 0) {
                cue = cues;
                do {
                    if (*cue == work->field_478) {
                        coord = &task->extra.tmd->coords[8];
                        switch (work->field_475) {
                            case 1:
                            case 2:
                                if ((s16)work->field_47A++ < 5) {
                                    Gp_SpawnEff(0x6006A, coord, 9, NULL);
                                    TASK_MESSAGE_DISPATCH_POINTER(Gp_LookupSlot4(0), ACTOR_COMMAND_MESSAGE_APPLY, &D_actor_310600_8017969C, 0);
                                } else {
                                    TASK_MESSAGE_DISPATCH_POINTER(Gp_LookupSlot4(0), ACTOR_COMMAND_MESSAGE_APPLY, &D_actor_310600_801796A0, 0);
                                    Task_SpawnFromTable(D_acropolis_cafeteria_80182AD8, 2, 0, 0);
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
    if (!(ext->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (func_800EA1A8(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x300, gRoomEffectState->groundShadowShade);
        }
        Gp_ClearRec18Occupied(&work->rec);
    }
    if (gGameSession->viewReady != 0) {
        task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&task->extra.tmd->coords[1]);
        func_800D7A9C(ext, (VECTOR*)task->extra.tmd->coords[1].workm.t, 0, 3);
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
static void func_actor_310600_8016231C(Task* arg0)
{
    Actor310600Work*     work;
    GfxCoord*            coord;
    SVECTOR              d;
    s32                  dx;
    s32                  dz;
    AnimationPlayRequest cmd;

    work  = (Actor310600Work*)arg0->work;
    coord = (arg0->extra.tmd)->coords;
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
            cmd.source.index         = 0;
            cmd.animationId          = 0xD;
            cmd.blend                = ANIMATION_BLEND_INTERPOLATE;
            cmd.blendFrames          = 0xA;
            cmd.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
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
/// `work->field_476` a reload instead of the register `cmd->source.index` arrived in.
s32 func_actor_310600_8016246C(Task* task, s32 arg1, AnimationPlayRequest* cmd, s32 arg3)
{
    Actor310600Work* work;
    TmdObject*       ext;
    s32              i;
    s32              j;

    work = (Actor310600Work*)task->work;
    ext  = task->extra.tmd;
    if (cmd->source.index != work->field_476) {
        work->field_476 = cmd->source.index;
        work->field_475 = -1;
        func_800B3F84(&work->rig.anim, D_actor_310600_80179640[work->field_476], ext, work->rig.poses,
                      work->rig.slots);
    }
    if (cmd->animationId != work->field_475) {
        work->field_475 = cmd->animationId;
        if (cmd->blend != ANIMATION_BLEND_RESET) {
            for (i = 1; i < 0x14; i++) {
                func_800B4114(&work->rig.anim, i, work->field_475, 0, cmd->blendFrames);
            }
        } else {
            for (i = 1; i < 0x14; i++) {
                Gp_AnimResetSlot(&work->rig.anim, i, work->field_475);
            }
        }
        for (j = 0; j <= D_actor_310600_80179644[work->field_475]; j++) {
            for (i = 1; i < 0x14; i++) {
                Gp_AnimTickIndex(&work->rig.anim, i);
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
    Actor310600Work*    work;
    Actor310600Work*    w;
    TmdObject*          ext;
    WorldCollisionBody* p;
    WorldCollisionBody* obj;
    s32                 i;
    s32                 ret;

    work = (Actor310600Work*)task->work;
    ext  = task->extra.tmd;
    w    = (Actor310600Work*)task->work;
    obj  = &work->obj;
    ret  = 0;
    switch (arg2) {
        case 0:
            ext->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            p           = &w->obj;
            for (i = 0; i <= 0; i++) {
                p->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                p++;
            }
            ext->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 1:
            ext->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            p           = &w->obj;
            for (i = 0; i <= 0; i++) {
                p->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                p++;
            }
            Tmd_AllocBuffers(ext);
            ext->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 2:
            ext->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            p           = &w->obj;
            for (i = 0; i <= 0; i++) {
                p->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                p++;
            }
            w->field_477 = 2;
            ext->flags  |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 3:
            ext->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            p           = obj;
            for (i = 0; i <= 0; i++) {
                p->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                p++;
            }
            ext->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
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

#include "../../shared/model_placement_attach.inc.c"

#include "../../shared/model_placement_mirror_parent.inc.c"

#include "../../shared/model_placement_attach_part.inc.c"

/// Tick state of the second child handler triple `D_actor_310600_80161E30`:
/// does nothing.
static void func_actor_310600_801629C4(Task* task)
{
}

void func_actor_310600_801629CC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_310600_80161E3C;
    sp.funcs[task->state](task);
}

static void func_actor_310600_80162A24(Task* arg0)
{
    Gp_UnlinkObj(&((Actor310600Work*)arg0->work)->obj);
    Gp_EnemyTaskExit(arg0);
}

static void func_actor_310600_80162A58(Task* arg0)
{
    TmdObject*       ext;
    Actor310600Work* work;

    work          = (Actor310600Work*)arg0->work;
    ext           = arg0->extra.tmd;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}

/// Entry 0 of the two-entry stack table `func_actor_310600_80161FA0` dispatches
/// through by `field_47C`: the idle handler, which does nothing. Entry 1 is
/// `func_actor_310600_80162A7C`; both receive the current task.
static void func_actor_310600_80162A74(Task* task)
{
}

/// Runs the entry of the actor's second state table that `field_47E` selects -
/// the counter `func_actor_310600_80162AD8` and `func_actor_310600_80162B98`
/// bump as they finish, so the table steps through the handlers in turn. Copies
/// the table onto the stack first, the same dispatch `func_actor_310600_801629CC`
/// performs over `state`.
static void func_actor_310600_80162A7C(Task* task)
{
    Actor310600Work* work;
    TaskFuncTable3   fns;

    work = (Actor310600Work*)task->work;
    fns  = D_actor_310600_80161E48;
    fns.funcs[(s16)work->field_47E](task);
}

/// Turns the actor's root part to face the work block's stored point: normalises
/// the offset from the part's own translation, takes its yaw with `ratan2`, and
/// rebuilds the local matrix from that yaw alone. Clearing `composeStamp` makes
/// `actorRenderComposeCoordChain` recompute the composed matrix from it, and bumping
/// `field_47E` moves the actor on to the next handler of its state table.
static void func_actor_310600_80162AD8(Task* task)
{
    Actor310600Work* work;
    GfxCoord*        coord;
    VECTOR           delta;
    SVECTOR          dir;
    SVECTOR          rot;

    work  = (Actor310600Work*)task->work;
    coord = task->extra.tmd->coords;

    delta.vx = work->field_4F8 - coord->coord.t[0];
    delta.vy = work->field_4FC - coord->coord.t[1];
    delta.vz = work->field_500 - coord->coord.t[2];
    VectorNormalS(&delta, &dir);

    rot.vx = 0;
    rot.vy = ratan2(dir.vx, dir.vz);
    rot.vz = 0;

    coord->param.rot.vx = rot.vx;
    coord->param.rot.vy = rot.vy;
    coord->param.rot.vz = rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->field_47E++;
}

/// State handler reached by the `field_47E` advance `func_actor_310600_80162AD8`
/// ends with: rotates the constant local-space offset
/// `D_actor_310600_80161E54` through the root part's matrix into `work->step`,
/// opens the per-axis stop threshold to 0x7FFF, which disables it for the update
/// loop, and advances `field_47E` again so the dispatcher runs the next handler.
static void func_actor_310600_80162B98(Task* task)
{
    Actor310600Work* work;
    GfxCoord*        coord;
    VECTOR           vec;

    coord = task->extra.tmd->coords;
    work  = (Actor310600Work*)task->work;

    vec = D_actor_310600_80161E54;
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->step);
    work->limit.vx = 0x7FFF;
    work->limit.vy = 0x7FFF;
    work->limit.vz = 0x7FFF;
    work->field_47E++;
}

#include "../../shared/actor_messages_place_euler.inc.c"

/// Sends the actor walking to the point `arg2`: stores it as the target the
/// movement steps of `D_actor_310600_80161E48` turn toward and close in on,
/// switches the tick onto those steps (`field_47C`), and starts animation 0xC
/// of bank 0 through `func_actor_310600_8016246C`. `arg1` is unused.
void func_actor_310600_80162C94(Task* arg0, s32 arg1, VECTOR* arg2)
{
    Actor310600Work*     work;
    AnimationPlayRequest cmd;

    work = (Actor310600Work*)arg0->work;

    work->field_47C = 1;
    work->field_4F8 = arg2->vx;
    work->field_4FC = arg2->vy;
    work->field_500 = arg2->vz;

    cmd.source.index         = 0;
    cmd.animationId          = 0xC;
    cmd.blend                = ANIMATION_BLEND_RESET;
    cmd.blendFrames          = 0;
    cmd.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;

    func_actor_310600_8016246C(arg0, 0x7D3, &cmd, 0);
}
