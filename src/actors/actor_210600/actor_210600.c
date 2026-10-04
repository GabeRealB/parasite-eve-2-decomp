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

/// Dual-width view of the animation rate in the work block. The message
/// handler `func_actor_210600_8014B770` arms it as one halfword, while the
/// seeding body copies the low byte into every slot's `AnimationSlot.rate`.
typedef union Actor210600Rate {
    /* 0x0 */ u16 half;
    /* 0x0 */ u8  byte;
} Actor210600Rate;
STATIC_ASSERT_SIZEOF(Actor210600Rate, 0x2);

/// The actor's work block. The spawn body allocates it zeroed with
/// `memCalloc(0x8D8, false)` and keeps it in `Task::work`. It holds the
/// animation context and slots at the front, the animation request state, and
/// the light / colour matrices the task's `TmdObject` is pointed at.
typedef struct Actor210600Work {
    /// Animation context the spawn body starts through `animationInitContext`, with
    /// its 19 slots directly behind it and the pose buffer after them.
    /* 0x000 */ ActorAnimRig19 rig;
    /* 0x43C */ byte           pad_43C[0x440];
    /// Animation request state. `field_87C` is the step the seeding body
    /// `func_actor_210600_8014B2C0` dispatches on -- 1 seeks every slot to
    /// `field_882`, 2 resets them, and both settle on 3 and clear the frame
    /// counter at `field_884`, which the running step then counts in.
    /// `field_882` is the requested clip, `field_880` the clip the previous
    /// request latched (the row `D_actor_210600_8015A498` is indexed with);
    /// `field_88A` steps 2 to 3 on the first update that sees it at 2.
    /* 0x87C */ s16             field_87C;
    /* 0x87E */ byte            pad_87E[0x2];
    /* 0x880 */ s16             field_880;
    /* 0x882 */ u16             field_882;
    /* 0x884 */ u16             field_884;
    /* 0x886 */ Actor210600Rate field_886;
    /* 0x888 */ byte            pad_888[0x2];
    /* 0x88A */ s16             field_88A;
    /* 0x88C */ byte            pad_88C[0x4];
    /* 0x890 */ s16             field_890;
    /* 0x892 */ byte            pad_892[0x4];
    /// Clip id (low 10 bits of `currentPose.indices.recordIndex`) slot 0 held on the last update, kept
    /// so the once-per-clip effect is not respawned while the clip is held.
    /* 0x896 */ s16 field_896;
    /// The light / colour matrices the spawn body points the task's
    /// `TmdObject::lightMtx` / `colorMtx` at.
    /* 0x898 */ MATRIX light;
    /* 0x8B8 */ MATRIX color;
} Actor210600Work;
STATIC_ASSERT_SIZEOF(Actor210600Work, 0x8D8);

/// Step table the seeding body `func_actor_210600_8014B2C0` walks: one 5-byte
/// row per clip the previous request latched in `Actor210600Work::field_880`,
/// addressed by the requested clip in `field_882`. The byte it reads is handed
/// to `animationSeekSlotWithBlend` as the request's fifth argument.
extern s8 D_actor_210600_8015A498[][5];

/// Stack record the state dispatcher copies the state table into before the
/// indirect call. Only `table` is written; the dispatcher's frame is larger
/// than the table alone, which the two trailing words account for.
typedef struct Actor210600DispatchCtx {
    /* 0x00 */ EnemyTaskFuncTable3 table;
    /* 0x0C */ s32                 field_C;
    /* 0x10 */ s32                 field_10;
} Actor210600DispatchCtx;
STATIC_ASSERT_SIZEOF(Actor210600DispatchCtx, 0x14);

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

/// Animation request handler: step 1 of the work block's `field_87C` seeks
/// every slot 1..18 to the clip in `field_882` through `animationSeekSlotWithBlend`,
/// passing `field_886`'s rate byte into the slot and the step `field_880`'s row
/// of `D_actor_210600_8015A498` as the request's fifth argument, then latches
/// the clip into `field_880`; step 2 does the same through
/// `animationResetSlot`. Both settle on step 3 and clear the frame counter at
/// `field_884`, which is counted from here on while every slot is ticked.
static void func_actor_210600_8014B2C0(Task* task)
{
    Actor210600Work* work;
    Actor210600Work* start;
    Actor210600Work* reset;
    Actor210600Work* tick;
    s32              i;
    s32              j;
    s32              k;

    work = (Actor210600Work*)task->work;
    if (work->field_87C == 1) {
        start = (Actor210600Work*)task->work;
        for (i = 1; i < 0x13; i++) {
            start->rig.slots[i].rate = start->field_886.byte;
            animationSeekSlotWithBlend(&start->rig.anim, i, (s16)start->field_882, 0,
                                       D_actor_210600_8015A498[start->field_880][(s16)start->field_882]);
        }
        start->field_880 = start->field_882;
        goto advance;
    }
    if (work->field_87C == 2) {
        reset = (Actor210600Work*)task->work;
        for (j = 1; j < 0x13; j++) {
            reset->rig.slots[j].rate = reset->field_886.byte;
            animationResetSlot(&reset->rig.anim, j, (s16)reset->field_882);
        }
        reset->field_880 = reset->field_882;
    advance:
        work->field_87C = 3;
        work->field_884 = 0;
    }
    if (work->field_88A == 2) {
        work->field_88A = 3;
    }
    work->field_884++;
    tick = (Actor210600Work*)task->work;
    for (k = 1; k < 0x13; k++) {
        tick->rig.slots[k].rate = tick->field_886.byte;
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

/// Update state of the actor. While `Actor210600Work::field_890` is clear it
/// runs the animation pass, rebuilds the model root's rotation around its yaw
/// at 0.75 scale, and when animation slot 1 holds clip 7 while slot 0 did not
/// on the previous update, spawns the effect `Gp_GetIdParam1(0x1001)` on the
/// model's second part. `enemy` is unused.
static void func_actor_210600_8014B434(Enemy* enemy, Task* task)
{
    Actor210600Work* work;
    SVECTOR          vec;
    EffectSpawnArg   eff;
    s32              id;

    work = (Actor210600Work*)task->work;
    if (work->field_890 == 0) {
        func_actor_210600_8014B2C0(task);
        Actor210600_ScaleRotation(task, 0xC00);

        id = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (id == 7 && work->field_896 != id) {
            memset(&vec, 0, 8);
            eff.coord      = task->extra.tmd->coords;
            eff.spawnArgLo = 0x100;
            eff.spawnArgHi = 2;
            func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, task->extra.tmd->coords + 1, &vec, &eff);
        }
        work->field_896 = work->rig.slots[0].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    }
}

/// Message 0x7D5 handler, listed in `D_actor_210600_8015A4CC`: `arg2` selects
/// the display mode. 0 hides the model (`TmdObject::flags` = 0x80) and 1 shows
/// it (flags cleared), both reallocating its buffers through
/// `Tmd_AllocBuffers`; 2 adds `TMD_OBJECT_SKIP_AUTO_BUFFER` to the flags and any other value sets
/// them to `TMD_OBJECT_SKIP_AUTO_BUFFER` alone. Modes 0 and 2 set `Actor210600Work::field_890`, which
/// stops the update state, and the other two clear it. `arg1` is unused.
s32 func_actor_210600_8014B5F4(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    TmdObject*       obj;
    Actor210600Work* work;

    obj  = task->extra.tmd;
    work = (Actor210600Work*)task->work;
    switch (arg2) {
        case 0:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Tmd_AllocBuffers(obj);
            work->field_890 = 1;
            break;
        case 1:
            obj->flags = 0;
            Tmd_AllocBuffers(obj);
            work->field_890 = 0;
            break;
        case 2:
            obj->flags     |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->field_890 = 1;
            break;
        default:
            obj->flags      = TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->field_890 = 0;
            break;
    }
    return 0;
}

#include "../../shared/actor_messages_place.inc.c"

/// Message 0x7DB handler, listed in `D_actor_210600_8015A4CC`. When the payload
/// comes from sender 0x401 with selector 1, it requests clip 1 through the
/// reset step at rate 0x10 and clears `Actor210600Work::field_890` so the
/// update state runs. Always reports the message handled.
s32 func_actor_210600_8014B770(Task* task, s32 msgId, ActorCommand* msg, s32 arg3)
{
    Actor210600Work* work;
    u16              selector;

    work = (Actor210600Work*)task->work;
    if (msg->context.key == 0x401) {
        selector = msg->command;
        if (selector == 1) {
            work->field_886.half = 0x10;
            work->field_882      = selector;
            work->field_890      = 0;
            work->field_87C      = 2;
        }
    }
    return 1;
}

#include "../../shared/coord_math_yaw_scale.inc.c"

/// Spawn state of the actor: allocates its `Actor210600Work`, destroying the
/// enemy if that fails, and points the task's `TmdObject` at the block's
/// light / colour matrices. The enemy takes the model root's matrix and its
/// third part coordinate, with its body offset zeroed, and is linked in. The
/// animation context is started from `D_actor_210600_8015A4B4` and reset to
/// clip 1, the message table is installed, and the model root is parented to
/// `gGfxViewCoord` and rebuilt once before its world position is handed to
/// `func_800D7A9C`. Advances the task to the next state.
static void func_actor_210600_8014B8C8(Enemy* enemy, Task* task)
{
    VECTOR           vec;
    GfxCoord*        coord;
    TmdObject*       obj;
    Actor210600Work* work;
    Actor210600Work* mem;
    TmdObject*       tmd;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    mem        = memCalloc(0x8D8, false);
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
    work->field_87C = 2;
    work->field_882 = 1;
    func_actor_210600_8014B2C0(task);
    task->msgTable      = D_actor_210600_8015A4CC;
    coord->parent       = &gGfxViewCoord;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    func_800D7A9C(task->extra.tmd, &vec, 0, 3);
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
    Actor210600DispatchCtx sp;

    sp.table = D_actor_210600_80149E24;
    sp.table.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}
