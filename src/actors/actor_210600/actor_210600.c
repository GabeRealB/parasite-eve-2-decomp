#include "actors/actor_210600.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
#include "gameplay/player_actor.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "overlay.h"
#include "../../shared/coord_math.h"
#include "../../shared/actor_messages.h"

/// Values of `_Actor210600Work::animRequest` and `_Actor210600Work::blendRequest`.
///
/// A zero-filled block holds 0, on which the driver only advances the slots.
enum {
    ACTOR_210600_ANIM_REQUEST_BLEND   = 1, // seek the slots to the animation, blending over the frames the transition table gives
    ACTOR_210600_ANIM_REQUEST_RESET   = 2, // restart the slots on the animation
    ACTOR_210600_ANIM_REQUEST_PLAYING = 3  // the request has been applied
};

/// Work block of the actor 210600 task.
///
/// The spawn state allocates it zeroed and keeps it at `Task::work`. It holds
/// the model's animation rig with its driver's request state, the flag the
/// draw-mode message suspends the update state with, the cue index the update
/// state keeps between frames, and storage for the model's matrices.
///
/// The driver state from `animRequest` to `blendRequest` is laid out, and
/// driven, like the same run of `OddStrangerWork`, and the bytes between `rig`
/// and it are where that block keeps a second rig. This actor carries only the
/// part of that driver which applies a request and advances `rig`: nothing
/// stores a blend request of either kind, and no second rig is ever touched.
///
/// Animation ids index the package's animation bank; rates are sixteenths of
/// a frame per tick, `ANIMATION_RATE_ONE` being normal speed.
typedef struct {
    ActorAnimRig19 rig;              // playback of the model's parts; the driver uses slots 1 to 18
    byte           field_43C[0x440]; // never accessed
    s16            animRequest;      // `ACTOR_210600_ANIM_REQUEST_*` for `rig`
    byte           field_87E[0x2];   // never accessed
    s16            appliedAnim;      // animation `rig` was last started on
    s16            animId;           // animation requested of `rig`; only 1 is ever requested
    u16            animFrames;       // ticks since `animRequest` was last applied; never read
    s16            animRate;         // playback rate of `rig`'s slots; 0 until the room command stores `ANIMATION_RATE_ONE`
    byte           field_888[0x2];   // never accessed
    s16            blendRequest;     // a pending `ACTOR_210600_ANIM_REQUEST_RESET` is marked applied by the driver and has no other effect; nothing requests it
    byte           field_88C[0x4];   // never accessed
    s16            suspended;        // 1 while the update state does nothing: set by draw modes 0 and 2, cleared by the other modes and by the room command (0 otherwise)
    byte           field_892[0x4];   // never accessed
    s16            lastCueIndex;     // cue index of slot 0's current pose at the last update, which the cue effect is tested against
    MATRIX         light;            // storage for the model's `TmdObject::lightMtx`
    MATRIX         color;            // storage for the model's `TmdObject::colorMtx`
} _Actor210600Work;
STATIC_ASSERT_SIZEOF(_Actor210600Work, 0x8D8);

/// Step table the seeding body `func_actor_210600_8014B2C0` walks: one 5-byte
/// row per animation in `_Actor210600Work::appliedAnim`, addressed by the
/// requested animation in `_Actor210600Work::animId`. The byte it reads is
/// handed to `animationSeekSlotWithBlend` as the request's fifth argument.
extern s8 D_actor_210600_8015A498[][5];

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).

/// Animation source the spawn body starts the work block's animation context
/// from.
extern u8 D_actor_210600_8015A4B4[];

/// Message table the spawn body publishes as `Task::msgTable`.
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_210600_8015A4CC[4];

/// Integer part of the last movement step `func_actor_210600_8014A9D0`
/// applied.
static SVECTOR ActorContact_ScratchPosition;

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

static TmdSource _gActor210600GrinningStrangerBody;
s32              func_actor_210600_8014B5F4(Task*, s32, s32, s32);
s32              func_actor_210600_8014B770(Task* task, s32 msgId, ActorCommand* msg, s32 arg3);
void             func_actor_210600_8014BA3C(Task*);

#include "../../shared/actor_contacts.h"

static AnimationPackedPose _gActor210600Animation07EF4Bank1[112] = {
#include "assets/actor_210600_animation_07EF4_bank1.inc"
};

static AnimationPackedRotation _gActor210600Animation07EF4Bank4[1614] = {
#include "assets/actor_210600_animation_07EF4_bank4.inc"
};

static AnimationRecord _gActor210600Animation07EF4Records[3027] = {
#include "assets/actor_210600_animation_07EF4_records.inc"
};

static u16 _gActor210600Animation07EF4Indices[20] = {
#include "assets/actor_210600_animation_07EF4_indices.inc"
};

AnimationSet gActor210600Animation07EF4 = {
    _gActor210600Animation07EF4Records,
    _gActor210600Animation07EF4Indices,
    { NULL, _gActor210600Animation07EF4Bank1, NULL, NULL, _gActor210600Animation07EF4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor210600Animation0A050Bank1[106] = {
#include "assets/actor_210600_animation_0A050_bank1.inc"
};

static AnimationPackedRotation _gActor210600Animation0A050Bank4[687] = {
#include "assets/actor_210600_animation_0A050_bank4.inc"
};

static AnimationRecord _gActor210600Animation0A050Records[1110] = {
#include "assets/actor_210600_animation_0A050_records.inc"
};

static u16 _gActor210600Animation0A050Indices[20] = {
#include "assets/actor_210600_animation_0A050_indices.inc"
};

AnimationSet gActor210600Animation0A050 = {
    _gActor210600Animation0A050Records,
    _gActor210600Animation0A050Indices,
    { NULL, _gActor210600Animation0A050Bank1, NULL, NULL, _gActor210600Animation0A050Bank4, NULL, NULL, NULL },
};

static TmdBone _gActor210600GrinningStrangerBodySkeleton[19] = {
#include "assets/grinning_stranger_body_skeleton.inc"
};

static u32 _gActor210600GrinningStrangerBodyPartVerts[19] = {
#include "assets/grinning_stranger_body_partVerts.inc"
};

static SVECTOR _gActor210600GrinningStrangerBodyVerts[306] = {
#include "assets/grinning_stranger_body_verts.inc"
};

static SVECTOR _gActor210600GrinningStrangerBodyNormals[365] = {
#include "assets/grinning_stranger_body_normals.inc"
};

static u32 _gActor210600GrinningStrangerBodyStream[3988] = {
#include "assets/grinning_stranger_body_stream.inc"
};

static TmdSource _gActor210600GrinningStrangerBody = {
    0,
    20032,
    7672,
    19,
    _gActor210600GrinningStrangerBodyPartVerts,
    _gActor210600GrinningStrangerBodyVerts,
    _gActor210600GrinningStrangerBodyNormals,
    _gActor210600GrinningStrangerBodySkeleton,
    _gActor210600GrinningStrangerBodyStream,
};

static AnimationPackedPose _gActor210600Animation10650Bank1[31] = {
#include "assets/actor_210600_animation_10650_bank1.inc"
};

static AnimationPackedRotation _gActor210600Animation10650Bank4[374] = {
#include "assets/actor_210600_animation_10650_bank4.inc"
};

static AnimationRecord _gActor210600Animation10650Records[512] = {
#include "assets/actor_210600_animation_10650_records.inc"
};

static u16 _gActor210600Animation10650Indices[20] = {
#include "assets/actor_210600_animation_10650_indices.inc"
};

static AnimationSet _gActor210600Animation10650 = {
    _gActor210600Animation10650Records,
    _gActor210600Animation10650Indices,
    { NULL, _gActor210600Animation10650Bank1, NULL, NULL, _gActor210600Animation10650Bank4, NULL, NULL, NULL },
};

s8 D_actor_210600_8015A498[5][5] = { 0 };

u8 D_actor_210600_8015A4B4[24] = {
    0,
    0,
    0,
    0,
    112,
    164,
    21,
    128,
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

TaskMessageEntry D_actor_210600_8015A4CC[4] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_210600_8014B5F4 },
    { ACTOR_MESSAGE_PLACE, actorMsgPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_210600_8014B770 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_210600_8015A4EC = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, func_actor_210600_8014BA3C, { .model = &_gActor210600GrinningStrangerBody } };

static AnimationPackedPose _gActor210600Animation11F5CBank1[37] = {
#include "assets/actor_210600_animation_11F5C_bank1.inc"
};

static AnimationPackedRotation _gActor210600Animation11F5CBank4[461] = {
#include "assets/actor_210600_animation_11F5C_bank4.inc"
};

static AnimationRecord _gActor210600Animation11F5CRecords[987] = {
#include "assets/actor_210600_animation_11F5C_records.inc"
};

static u16 _gActor210600Animation11F5CIndices[20] = {
#include "assets/actor_210600_animation_11F5C_indices.inc"
};

AnimationSet gActor210600Animation11F5C = {
    _gActor210600Animation11F5CRecords,
    _gActor210600Animation11F5CIndices,
    { NULL, _gActor210600Animation11F5CBank1, NULL, NULL, _gActor210600Animation11F5CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor210600Animation12244Bank1[5] = {
#include "assets/actor_210600_animation_12244_bank1.inc"
};

static AnimationPackedRotation _gActor210600Animation12244Bank4[52] = {
#include "assets/actor_210600_animation_12244_bank4.inc"
};

static AnimationRecord _gActor210600Animation12244Records[99] = {
#include "assets/actor_210600_animation_12244_records.inc"
};

static u16 _gActor210600Animation12244Indices[20] = {
#include "assets/actor_210600_animation_12244_indices.inc"
};

AnimationSet gActor210600Animation12244 = {
    _gActor210600Animation12244Records,
    _gActor210600Animation12244Indices,
    { NULL, _gActor210600Animation12244Bank1, NULL, NULL, _gActor210600Animation12244Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor210600Animation12B30Bank1[16] = {
#include "assets/actor_210600_animation_12B30_bank1.inc"
};

static AnimationPackedRotation _gActor210600Animation12B30Bank4[221] = {
#include "assets/actor_210600_animation_12B30_bank4.inc"
};

static AnimationRecord _gActor210600Animation12B30Records[282] = {
#include "assets/actor_210600_animation_12B30_records.inc"
};

static u16 _gActor210600Animation12B30Indices[20] = {
#include "assets/actor_210600_animation_12B30_indices.inc"
};

AnimationSet gActor210600Animation12B30 = {
    _gActor210600Animation12B30Records,
    _gActor210600Animation12B30Indices,
    { NULL, _gActor210600Animation12B30Bank1, NULL, NULL, _gActor210600Animation12B30Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor210600Animation1310CBank1[8] = {
#include "assets/actor_210600_animation_1310C_bank1.inc"
};

static AnimationPackedRotation _gActor210600Animation1310CBank4[150] = {
#include "assets/actor_210600_animation_1310C_bank4.inc"
};

static AnimationRecord _gActor210600Animation1310CRecords[181] = {
#include "assets/actor_210600_animation_1310C_records.inc"
};

static u16 _gActor210600Animation1310CIndices[20] = {
#include "assets/actor_210600_animation_1310C_indices.inc"
};

AnimationSet gActor210600Animation1310C = {
    _gActor210600Animation1310CRecords,
    _gActor210600Animation1310CIndices,
    { NULL, _gActor210600Animation1310CBank1, NULL, NULL, _gActor210600Animation1310CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor210600Animation134C8Bank1[7] = {
#include "assets/actor_210600_animation_134C8_bank1.inc"
};

static AnimationPackedRotation _gActor210600Animation134C8Bank4[72] = {
#include "assets/actor_210600_animation_134C8_bank4.inc"
};

static AnimationRecord _gActor210600Animation134C8Records[126] = {
#include "assets/actor_210600_animation_134C8_records.inc"
};

static u16 _gActor210600Animation134C8Indices[20] = {
#include "assets/actor_210600_animation_134C8_indices.inc"
};

AnimationSet gActor210600Animation134C8 = {
    _gActor210600Animation134C8Records,
    _gActor210600Animation134C8Indices,
    { NULL, _gActor210600Animation134C8Bank1, NULL, NULL, _gActor210600Animation134C8Bank4, NULL, NULL, NULL },
};

static SVECTOR ActorContact_ScratchPosition;

static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

static void            func_actor_210600_8014B2C0(Task* task);
static __inline__ void Actor210600_ScaleRotation(Task* task, s16 scale);
static void            func_actor_210600_8014B434(Enemy* enemy, Task* task);
static void            func_actor_210600_8014B8C8(Enemy* enemy, Task* task);

#include "../../shared/actor_contacts.inc.c"

/// Animation driver, run once per update: an `animRequest` of
/// `ACTOR_210600_ANIM_REQUEST_BLEND` seeks slots 1 to 18 of `rig` to `animId`
/// through the step table `D_actor_210600_8015A498`,
/// `ACTOR_210600_ANIM_REQUEST_RESET` restarts them on it, and both latch the
/// animation into `appliedAnim`, settle on `ACTOR_210600_ANIM_REQUEST_PLAYING`
/// and clear `animFrames`. A reset in `blendRequest` is only marked applied.
/// The tail counts a frame and advances every driven slot at `animRate`.
static void func_actor_210600_8014B2C0(Task* task)
{
    _Actor210600Work* work;
    _Actor210600Work* start;
    _Actor210600Work* reset;
    _Actor210600Work* tick;
    s32               i;
    s32               j;
    s32               k;

    work = task->work;
    if (work->animRequest == ACTOR_210600_ANIM_REQUEST_BLEND) {
        start = task->work;
        for (i = 1; i < ARRAY_SIZE(start->rig.slots); i++) {
            start->rig.slots[i].rate = start->animRate;
            animationSeekSlotWithBlend(&start->rig.anim, i, start->animId, 0,
                                       D_actor_210600_8015A498[start->appliedAnim][start->animId]);
        }
        start->appliedAnim = start->animId;
        goto advance;
    }
    if (work->animRequest == ACTOR_210600_ANIM_REQUEST_RESET) {
        reset = task->work;
        for (j = 1; j < ARRAY_SIZE(reset->rig.slots); j++) {
            reset->rig.slots[j].rate = reset->animRate;
            animationResetSlot(&reset->rig.anim, j, reset->animId);
        }
        reset->appliedAnim = reset->animId;
    advance:
        work->animRequest = ACTOR_210600_ANIM_REQUEST_PLAYING;
        work->animFrames  = 0;
    }
    if (work->blendRequest == ACTOR_210600_ANIM_REQUEST_RESET) {
        work->blendRequest = ACTOR_210600_ANIM_REQUEST_PLAYING;
    }
    work->animFrames++;
    tick = task->work;
    for (k = 1; k < ARRAY_SIZE(tick->rig.slots); k++) {
        tick->rig.slots[k].rate = tick->animRate;
        animationTickSlot(&tick->rig.anim, k);
    }
}

/// Rebuilds the model's root part rotation around the yaw it already faces and
/// rescales it uniformly through an `ActorScaleRotScratch` block borrowed from
/// the scratch stack, which is handed back once the rotation has been copied
/// onto the coordinate. The same code as `coordSetYawScale`,
/// expanded in place where the update body calls it.
static __inline__ void Actor210600_ScaleRotation(Task* task, s16 scale)
{
    ActorScaleRotScratch* blk;
    GfxCoord*             coord;
    u8*                   head;
    s16                   ang;
    u16                   m22;

    head                                       = SCRATCH_STACK_CURSOR(u8);
    coord                                      = task->extra.tmd->coords;
    blk                                        = (ActorScaleRotScratch*)(head - sizeof(ActorScaleRotScratch));
    SCRATCH_STACK_CURSOR(ActorScaleRotScratch) = blk;

    ang      = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->yaw = ang;
    gfxRotMatrixY(&blk->rotation, ang, 1);
    blk->scale.vz = scale;
    blk->scale.vy = scale;
    blk->scale.vx = scale;
    ScaleMatrix(&blk->rotation, &blk->scale);

    coord->coord.m[0][0] = (u16)((ActorScaleRotScratch*)(head - sizeof(ActorScaleRotScratch)))->rotation.m[0][0];
    coord->coord.m[0][1] = (u16)blk->rotation.m[0][1];
    coord->coord.m[0][2] = (u16)blk->rotation.m[0][2];
    coord->coord.m[1][0] = (u16)blk->rotation.m[1][0];
    coord->coord.m[1][1] = (u16)blk->rotation.m[1][1];
    coord->coord.m[1][2] = (u16)blk->rotation.m[1][2];
    coord->coord.m[2][0] = (u16)blk->rotation.m[2][0];
    coord->coord.m[2][1] = (u16)blk->rotation.m[2][1];
    m22                  = (u16)blk->rotation.m[2][2];
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    coord->coord.m[2][2] = m22;
    SCRATCH_STACK_RELEASE_BYTES(sizeof(ActorScaleRotScratch));
}

/// Update state of the actor. While `_Actor210600Work::suspended` is clear it
/// runs the animation driver, rebuilds the model root's rotation around its
/// yaw at 0.75 scale, and when the current pose of animation slot 1 is at cue
/// index 7 while `lastCueIndex` is not, spawns the effect
/// `Gp_GetIdParam1(0x1001)` on the model's second part. `lastCueIndex` is then
/// taken from slot 0, not from the slot just tested. `enemy` is unused.
static void func_actor_210600_8014B434(Enemy* enemy, Task* task)
{
    _Actor210600Work* work;
    SVECTOR           vec;
    EffectSpawnArg    eff;
    s32               id;

    work = task->work;
    if (work->suspended == 0) {
        func_actor_210600_8014B2C0(task);
        Actor210600_ScaleRotation(task, 0xC00);

        id = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (id == 7 && work->lastCueIndex != id) {
            memset(&vec, 0, 8);
            eff.coord      = task->extra.tmd->coords;
            eff.spawnArgLo = 0x100;
            eff.spawnArgHi = 2;
            func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, task->extra.tmd->coords + 1, &vec, &eff);
        }
        work->lastCueIndex = work->rig.slots[0].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    }
}

/// Message 0x7D5 handler, listed in `D_actor_210600_8015A4CC`: `arg2` selects
/// the display mode. 0 hides the model (`TmdObject::flags` = 0x80) and 1 shows
/// it (flags cleared), both reallocating its buffers through
/// `tmdAllocPrimitiveBuffer`; 2 adds `TMD_OBJECT_SKIP_AUTO_BUFFER` to the flags and any other value sets
/// them to `TMD_OBJECT_SKIP_AUTO_BUFFER` alone. Modes 0 and 2 set `_Actor210600Work::suspended`, which
/// stops the update state, and the other two clear it. `arg1` is unused.
s32 func_actor_210600_8014B5F4(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    TmdObject*        obj;
    _Actor210600Work* work;

    obj  = task->extra.tmd;
    work = task->work;
    switch (arg2) {
        case 0:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(obj);
            work->suspended = 1;
            break;
        case 1:
            obj->flags = 0;
            tmdAllocPrimitiveBuffer(obj);
            work->suspended = 0;
            break;
        case 2:
            obj->flags     |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->suspended = 1;
            break;
        default:
            obj->flags      = TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->suspended = 0;
            break;
    }
    return 0;
}

#include "../../shared/actor_messages_place.inc.c"

/// Message 0x7DB handler, listed in `D_actor_210600_8015A4CC`. When the payload
/// comes from sender 0x401 with selector 1, it requests a restart on animation
/// 1 at normal speed and clears `_Actor210600Work::suspended` so the update
/// state runs. Always reports the message handled.
s32 func_actor_210600_8014B770(Task* task, s32 msgId, ActorCommand* msg, s32 arg3)
{
    _Actor210600Work* work;
    u16               selector;

    work = task->work;
    if (msg->context.key == 0x401) {
        selector = msg->command;
        if (selector == 1) {
            work->animRate    = ANIMATION_RATE_ONE;
            work->animId      = selector;
            work->suspended   = 0;
            work->animRequest = ACTOR_210600_ANIM_REQUEST_RESET;
        }
    }
    return 1;
}

#include "../../shared/coord_math_yaw_scale.inc.c"

/// Spawn state of the actor: allocates its `_Actor210600Work`, destroying the
/// enemy if that fails, and points the task's `TmdObject` at the block's
/// light / colour matrices. The enemy takes the model root's matrix and its
/// third part coordinate, with its body offset zeroed, and is linked in. The
/// animation context is started from `D_actor_210600_8015A4B4` and restarted
/// on animation 1, the message table is installed, and the model root is parented to
/// `gGfxViewCoord` and rebuilt once before its world position is handed to
/// `worldCoordSetModelLighting`. Advances the task to the next state.
static void func_actor_210600_8014B8C8(Enemy* enemy, Task* task)
{
    VECTOR            vec;
    GfxCoord*         coord;
    TmdObject*        obj;
    _Actor210600Work* work;
    _Actor210600Work* mem;
    TmdObject*        tmd;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    mem        = memCalloc(sizeof(_Actor210600Work), false);
    work       = mem;
    task->work = mem;
    if (mem == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    tmd               = task->extra.tmd;
    tmd->lightMtx     = &work->light;
    tmd->colorMtx     = &work->color;
    enemy->field_4    = &coord->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &task->extra.tmd->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->field_4D               = 0;
    enemy->reactionFlags          = 0;
    enemy->field_4D               = 0;
    animationInitContext(&work->rig.anim, (AnimationSet**)D_actor_210600_8015A4B4, obj, work->rig.poses, work->rig.slots);
    work->animRequest = ACTOR_210600_ANIM_REQUEST_RESET;
    work->animId      = 1;
    func_actor_210600_8014B2C0(task);
    task->msgTable      = D_actor_210600_8015A4CC;
    coord->parent       = &gGfxViewCoord;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    worldCoordSetModelLighting(task->extra.tmd, &vec, 0, 3);
    task->state++;
}

/// The actor's three task states - spawn, update and teardown - which
/// `func_actor_210600_8014BA3C` runs by `Task::state`.
static const EnemyTaskFuncTable3 D_actor_210600_80149E24 = {
    {
        func_actor_210600_8014B8C8,
        func_actor_210600_8014B434,
        enemyDestroy,
    },
};

/// State dispatcher: copies the state table onto the stack and calls the entry
/// `Task::state` selects with the task's enemy and the task itself.
void func_actor_210600_8014BA3C(Task* arg0)
{
    EnemyTaskFuncTable3 sp;
    // Nothing reads or writes these bytes, but the frame is eight bytes larger
    // than the table copy alone needs. Their original declaration is unknown.
    byte unused[8];

    sp = D_actor_210600_80149E24;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}
