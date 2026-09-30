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

/// Dual-width view of the animation rate in the work block. The message
/// handler `func_actor_210600_8014B770` arms it as one halfword, while the
/// seeding body copies the low byte into every slot's `AnimationSlot.rate`.
typedef union Actor210600Rate {
    /* 0x0 */ u16 half;
    /* 0x0 */ u8  byte;
} Actor210600Rate;
STATIC_ASSERT_SIZEOF(Actor210600Rate, 0x2);

/// The actor's work block. The spawn body allocates it zeroed with
/// `memCalloc(0x8D8, false)` and keeps it in `Task::work`, which an enemy
/// actor uses for its own state rather than a `TaskIdMap`. It holds the
/// animation context and slots at the front, the animation request state, and
/// the light / colour matrices the task's `TmdObject` is pointed at.
typedef struct Actor210600Work {
    /// Animation context the spawn body starts through `func_800B3F84`, with
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
/// to `func_800B4114` as the request's fifth argument.
extern s8 D_actor_210600_8015A498[][5];

/// Stack record the state dispatcher copies the state table into before the
/// indirect call. Only `table` is written; the dispatcher's frame is larger
/// than the table alone, which the two trailing words account for.
typedef struct Actor210600DispatchCtx {
    /* 0x00 */ GpEnemyTaskFuncTable3 table;
    /* 0x0C */ s32                   field_C;
    /* 0x10 */ s32                   field_10;
} Actor210600DispatchCtx;
STATIC_ASSERT_SIZEOF(Actor210600DispatchCtx, 0x14);

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).

/// Animation source the spawn body starts the work block's animation context
/// from.
extern u8 D_actor_210600_8015A4B4[];

/// Message table the spawn body publishes as `Task::msgTable`.
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*, s32, ActorCommand* request);
        s32 (*call1)(Task*, s32, GpXformArg*);
        s32 (*call2)(Task*, s32, s32);
    } handler;
} Actor210600MessageEntry;
STATIC_ASSERT_SIZEOF(Actor210600MessageEntry, 8);

extern Actor210600MessageEntry D_actor_210600_8015A4CC[4];

/// Integer part of the last movement step `func_actor_210600_8014A9D0`
/// applied.
static SVECTOR ActorContact_ScratchPosition;

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

extern TmdSource D_actor_210600_801594D8;
s32              func_actor_210600_8014B5F4(Task*, s32, s32);
s32              func_actor_210600_8014B6A0(Task*, s32, GpXformArg*);
s32              func_actor_210600_8014B770(Task*, s32, ActorCommand* msg);
void             func_actor_210600_8014BA3C(Task*);

#include "../../shared/actor_contacts.h"

AnimationPackedPose D_actor_210600_8014CF28[112] = {
#include "assets/actor_210600_animation_07EF4_bank1.inc"
};

AnimationPackedRotation D_actor_210600_8014D468[1614] = {
#include "assets/actor_210600_animation_07EF4_bank4.inc"
};

AnimationRecord D_actor_210600_8014EDA0[3027] = {
#include "assets/actor_210600_animation_07EF4_records.inc"
};

u16 D_actor_210600_80151CEC[20] = {
#include "assets/actor_210600_animation_07EF4_indices.inc"
};

AnimationSet D_actor_210600_80151D14 = {
    D_actor_210600_8014EDA0,
    D_actor_210600_80151CEC,
    { NULL, D_actor_210600_8014CF28, NULL, NULL, D_actor_210600_8014D468, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_210600_80151D3C[106] = {
#include "assets/actor_210600_animation_0A050_bank1.inc"
};

AnimationPackedRotation D_actor_210600_80152234[687] = {
#include "assets/actor_210600_animation_0A050_bank4.inc"
};

AnimationRecord D_actor_210600_80152CF0[1110] = {
#include "assets/actor_210600_animation_0A050_records.inc"
};

u16 D_actor_210600_80153E48[20] = {
#include "assets/actor_210600_animation_0A050_indices.inc"
};

AnimationSet D_actor_210600_80153E70 = {
    D_actor_210600_80152CF0,
    D_actor_210600_80153E48,
    { NULL, D_actor_210600_80151D3C, NULL, NULL, D_actor_210600_80152234, NULL, NULL, NULL },
};

TmdBone D_actor_210600_80153E98[19] = {
#include "assets/actor_210600_model_0F6B8_skeleton.inc"
};

u32 D_actor_210600_80154144[19] = {
#include "assets/actor_210600_model_0F6B8_partVerts.inc"
};

SVECTOR D_actor_210600_80154190[306] = {
#include "assets/actor_210600_model_0F6B8_verts.inc"
};

SVECTOR D_actor_210600_80154B20[365] = {
#include "assets/actor_210600_model_0F6B8_normals.inc"
};

u32 D_actor_210600_80155688[3988] = {
#include "assets/actor_210600_model_0F6B8_stream.inc"
};

TmdSource D_actor_210600_801594D8 = {
    0,
    20032,
    7672,
    19,
    D_actor_210600_80154144,
    D_actor_210600_80154190,
    D_actor_210600_80154B20,
    D_actor_210600_80153E98,
    D_actor_210600_80155688,
};

AnimationPackedPose D_actor_210600_801594FC[31] = {
#include "assets/actor_210600_animation_10650_bank1.inc"
};

AnimationPackedRotation D_actor_210600_80159670[374] = {
#include "assets/actor_210600_animation_10650_bank4.inc"
};

AnimationRecord D_actor_210600_80159C48[512] = {
#include "assets/actor_210600_animation_10650_records.inc"
};

u16 D_actor_210600_8015A448[20] = {
#include "assets/actor_210600_animation_10650_indices.inc"
};

AnimationSet D_actor_210600_8015A470 = {
    D_actor_210600_80159C48,
    D_actor_210600_8015A448,
    { NULL, D_actor_210600_801594FC, NULL, NULL, D_actor_210600_80159670, NULL, NULL, NULL },
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

Actor210600MessageEntry D_actor_210600_8015A4CC[4] = {
    { 2005, { .call2 = func_actor_210600_8014B5F4 } },
    { 2004, { .call1 = func_actor_210600_8014B6A0 } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call0 = func_actor_210600_8014B770 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_210600_8015A4EC = { 257, 96, func_actor_210600_8014BA3C, { .model = &D_actor_210600_801594D8 } };

AnimationPackedPose D_actor_210600_8015A4F8[37] = {
#include "assets/actor_210600_animation_11F5C_bank1.inc"
};

AnimationPackedRotation D_actor_210600_8015A6B4[461] = {
#include "assets/actor_210600_animation_11F5C_bank4.inc"
};

AnimationRecord D_actor_210600_8015ADE8[987] = {
#include "assets/actor_210600_animation_11F5C_records.inc"
};

u16 D_actor_210600_8015BD54[20] = {
#include "assets/actor_210600_animation_11F5C_indices.inc"
};

AnimationSet D_actor_210600_8015BD7C = {
    D_actor_210600_8015ADE8,
    D_actor_210600_8015BD54,
    { NULL, D_actor_210600_8015A4F8, NULL, NULL, D_actor_210600_8015A6B4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_210600_8015BDA4[5] = {
#include "assets/actor_210600_animation_12244_bank1.inc"
};

AnimationPackedRotation D_actor_210600_8015BDE0[52] = {
#include "assets/actor_210600_animation_12244_bank4.inc"
};

AnimationRecord D_actor_210600_8015BEB0[99] = {
#include "assets/actor_210600_animation_12244_records.inc"
};

u16 D_actor_210600_8015C03C[20] = {
#include "assets/actor_210600_animation_12244_indices.inc"
};

AnimationSet D_actor_210600_8015C064 = {
    D_actor_210600_8015BEB0,
    D_actor_210600_8015C03C,
    { NULL, D_actor_210600_8015BDA4, NULL, NULL, D_actor_210600_8015BDE0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_210600_8015C08C[16] = {
#include "assets/actor_210600_animation_12B30_bank1.inc"
};

AnimationPackedRotation D_actor_210600_8015C14C[221] = {
#include "assets/actor_210600_animation_12B30_bank4.inc"
};

AnimationRecord D_actor_210600_8015C4C0[282] = {
#include "assets/actor_210600_animation_12B30_records.inc"
};

u16 D_actor_210600_8015C928[20] = {
#include "assets/actor_210600_animation_12B30_indices.inc"
};

AnimationSet D_actor_210600_8015C950 = {
    D_actor_210600_8015C4C0,
    D_actor_210600_8015C928,
    { NULL, D_actor_210600_8015C08C, NULL, NULL, D_actor_210600_8015C14C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_210600_8015C978[8] = {
#include "assets/actor_210600_animation_1310C_bank1.inc"
};

AnimationPackedRotation D_actor_210600_8015C9D8[150] = {
#include "assets/actor_210600_animation_1310C_bank4.inc"
};

AnimationRecord D_actor_210600_8015CC30[181] = {
#include "assets/actor_210600_animation_1310C_records.inc"
};

u16 D_actor_210600_8015CF04[20] = {
#include "assets/actor_210600_animation_1310C_indices.inc"
};

AnimationSet D_actor_210600_8015CF2C = {
    D_actor_210600_8015CC30,
    D_actor_210600_8015CF04,
    { NULL, D_actor_210600_8015C978, NULL, NULL, D_actor_210600_8015C9D8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_210600_8015CF54[7] = {
#include "assets/actor_210600_animation_134C8_bank1.inc"
};

AnimationPackedRotation D_actor_210600_8015CFA8[72] = {
#include "assets/actor_210600_animation_134C8_bank4.inc"
};

AnimationRecord D_actor_210600_8015D0C8[126] = {
#include "assets/actor_210600_animation_134C8_records.inc"
};

u16 D_actor_210600_8015D2C0[20] = {
#include "assets/actor_210600_animation_134C8_indices.inc"
};

AnimationSet D_actor_210600_8015D2E8 = {
    D_actor_210600_8015D0C8,
    D_actor_210600_8015D2C0,
    { NULL, D_actor_210600_8015CF54, NULL, NULL, D_actor_210600_8015CFA8, NULL, NULL, NULL },
};

static SVECTOR ActorContact_ScratchPosition;

static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

static void            func_actor_210600_8014B2C0(Task* task);
static __inline__ void Actor210600_ScaleRotation(Task* task, s16 scale);
static void            func_actor_210600_8014B434(GpEnemy* enemy, Task* task);
static void            func_actor_210600_8014B7B0(GfxCoord* coord, s16 scale);
static void            func_actor_210600_8014B8C8(GpEnemy* enemy, Task* task);

#include "../../shared/actor_contacts.inc.c"

/// Animation request handler: step 1 of the work block's `field_87C` seeks
/// every slot 1..18 to the clip in `field_882` through `func_800B4114`,
/// passing `field_886`'s rate byte into the slot and the step `field_880`'s row
/// of `D_actor_210600_8015A498` as the request's fifth argument, then latches
/// the clip into `field_880`; step 2 does the same through
/// `Gp_AnimResetSlot`. Both settle on step 3 and clear the frame counter at
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
            func_800B4114(&start->rig.anim, i, (s16)start->field_882, 0,
                          D_actor_210600_8015A498[start->field_880][(s16)start->field_882]);
        }
        start->field_880 = start->field_882;
        goto advance;
    }
    if (work->field_87C == 2) {
        reset = (Actor210600Work*)task->work;
        for (j = 1; j < 0x13; j++) {
            reset->rig.slots[j].rate = reset->field_886.byte;
            Gp_AnimResetSlot(&reset->rig.anim, j, (s16)reset->field_882);
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
        Gp_AnimTickIndex(&tick->rig.anim, k);
    }
}

/// Rebuilds the model's root part rotation around the yaw it already faces and
/// rescales it uniformly through a 0x34-byte block borrowed from
/// the scratch stack, which is handed back once the rotation has been copied
/// onto the coordinate. The same code as `func_actor_210600_8014B7B0`,
/// expanded in place where the update body calls it.
static __inline__ void Actor210600_ScaleRotation(Task* task, s16 scale)
{
    ActorScaleRotScratch* blk;
    GfxCoord*             coord;
    u8*                   head;
    s16                   ang;
    u16                   m22;

    head                               = SCRATCH_HEAD(u8);
    coord                              = task->extra.tmd->coords;
    blk                                = (ActorScaleRotScratch*)(head - 0x34);
    SCRATCH_HEAD(ActorScaleRotScratch) = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = scale;
    blk->scale.vy = scale;
    blk->scale.vx = scale;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] = (u16)((ActorScaleRotScratch*)(head - 0x34))->m.m[0][0];
    coord->coord.m[0][1] = (u16)blk->m.m[0][1];
    coord->coord.m[0][2] = (u16)blk->m.m[0][2];
    coord->coord.m[1][0] = (u16)blk->m.m[1][0];
    coord->coord.m[1][1] = (u16)blk->m.m[1][1];
    coord->coord.m[1][2] = (u16)blk->m.m[1][2];
    coord->coord.m[2][0] = (u16)blk->m.m[2][0];
    coord->coord.m[2][1] = (u16)blk->m.m[2][1];
    m22                  = (u16)blk->m.m[2][2];
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    coord->coord.m[2][2] = m22;
    SCRATCH_POP_BYTES(0x34);
}

/// Update state of the actor. While `Actor210600Work::field_890` is clear it
/// runs the animation pass, rebuilds the model root's rotation around its yaw
/// at 0.75 scale, and when animation slot 1 holds clip 7 while slot 0 did not
/// on the previous update, spawns the effect `Gp_GetIdParam1(0x1001)` on the
/// model's second part. `enemy` is unused.
static void func_actor_210600_8014B434(GpEnemy* enemy, Task* task)
{
    Actor210600Work* work;
    SVECTOR          vec;
    GpEffArg         eff;
    s32              id;

    work = (Actor210600Work*)task->work;
    if (work->field_890 == 0) {
        func_actor_210600_8014B2C0(task);
        Actor210600_ScaleRotation(task, 0xC00);

        id = work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF;
        if (id == 7 && work->field_896 != id) {
            memset(&vec, 0, 8);
            eff.coord      = task->extra.tmd->coords;
            eff.spawnArgLo = 0x100;
            eff.spawnArgHi = 2;
            func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, task->extra.tmd->coords + 1, &vec, &eff);
        }
        work->field_896 = work->rig.slots[0].currentPose.indices.recordIndex & 0x3FF;
    }
}

/// Message 0x7D5 handler, listed in `D_actor_210600_8015A4CC`: `arg2` selects
/// the display mode. 0 hides the model (`TmdObject::flags` = 0x80) and 1 shows
/// it (flags cleared), both reallocating its buffers through
/// `Tmd_AllocBuffers`; 2 adds bit 0x4 to the flags and any other value sets
/// them to 0x4 alone. Modes 0 and 2 set `Actor210600Work::field_890`, which
/// stops the update state, and the other two clear it. `arg1` is unused.
s32 func_actor_210600_8014B5F4(Task* task, s32 arg1, s32 arg2)
{
    TmdObject*       obj;
    Actor210600Work* work;

    obj  = task->extra.tmd;
    work = (Actor210600Work*)task->work;
    switch (arg2) {
        case 0:
            obj->flags = TMD_OBJECT_HIDDEN;
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

/// Message 0x7D4 handler, listed in `D_actor_210600_8015A4CC`: places the
/// model root at `placement`. The three longs become the coordinate's
/// translation, the X, Y and Z angles are then applied in that order through
/// `Gfx_RotMatrixX` / `Y` / `Z`, and the coordinate is marked dirty. `msgId`
/// is unused; the handler always reports the message handled.
s32 func_actor_210600_8014B6A0(Task* task, s32 msgId, GpXformArg* placement)
{
    task->extra.tmd->coords->coord.t[0] = placement->pos.vx;
    task->extra.tmd->coords->coord.t[1] = placement->pos.vy;
    task->extra.tmd->coords->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixX(&task->extra.tmd->coords->coord, placement->rot.vx, 1);
    Gfx_RotMatrixY(&task->extra.tmd->coords->coord, placement->rot.vy, 0);
    Gfx_RotMatrixZ(&task->extra.tmd->coords->coord, placement->rot.vz, 0);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    return 1;
}

/// Message 0x7DB handler, listed in `D_actor_210600_8015A4CC`. When the payload
/// comes from sender 0x401 with selector 1, it requests clip 1 through the
/// reset step at rate 0x10 and clears `Actor210600Work::field_890` so the
/// update state runs. Always reports the message handled.
s32 func_actor_210600_8014B770(Task* task, s32 msgId, ActorCommand* msg)
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

/// Rebuilds `coord`'s rotation as a pure Y rotation by the yaw it currently
/// faces (`ratan2` of `-m[2][0], m[2][2]`), uniformly scaled by `scale`,
/// through a 0x34-byte block borrowed from the scratch stack and handed back
/// once the matrix is copied. Marks the coordinate dirty. Nothing in the
/// overlay calls it: the update body carries the same code inline.
static void func_actor_210600_8014B7B0(GfxCoord* coord, s16 scale)
{
    void**                scratch;
    ActorScaleRotScratch* head;
    ActorScaleRotScratch* blk;
    s16                   ang;
    u16                   m22;

    scratch                                        = SCRATCH_HEAD_ADDR;
    head                                           = SCRATCH_HEAD_AT(scratch, ActorScaleRotScratch);
    blk                                            = head - 1;
    SCRATCH_HEAD_AT(scratch, ActorScaleRotScratch) = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = scale;
    blk->scale.vy = scale;
    blk->scale.vx = scale;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] = (u16)(head - 1)->m.m[0][0];
    coord->coord.m[0][1] = (u16)blk->m.m[0][1];
    coord->coord.m[0][2] = (u16)blk->m.m[0][2];
    coord->coord.m[1][0] = (u16)blk->m.m[1][0];
    coord->coord.m[1][1] = (u16)blk->m.m[1][1];
    coord->coord.m[1][2] = (u16)blk->m.m[1][2];
    coord->coord.m[2][0] = (u16)blk->m.m[2][0];
    coord->coord.m[2][1] = (u16)blk->m.m[2][1];
    m22                  = (u16)blk->m.m[2][2];
    SCRATCH_POP_AT(scratch, ActorScaleRotScratch);
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    coord->coord.m[2][2] = m22;
}

/// Spawn state of the actor: allocates its `Actor210600Work`, destroying the
/// enemy if that fails, and points the task's `TmdObject` at the block's
/// light / colour matrices. The enemy takes the model root's matrix and its
/// third part coordinate, with its body offset zeroed, and is linked in. The
/// animation context is started from `D_actor_210600_8015A4B4` and reset to
/// clip 1, the message table is installed, and the model root is parented to
/// `gGfxViewCoord` and rebuilt once before its world position is handed to
/// `func_800D7A9C`. Advances the task to the next state.
static void func_actor_210600_8014B8C8(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    GfxCoord*        coord;
    TmdObject*       obj;
    Actor210600Work* work;
    Actor210600Work* mem;
    TmdObject*       tmd;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    mem        = (Actor210600Work*)memCalloc(0x8D8, false);
    work       = mem;
    task->work = mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(enemy, task);
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
    enemy->node.state.b.flags = 1;
    enemy->field_4D           = 0;
    enemy->reactionFlags      = 0;
    enemy->field_4D           = 0;
    func_800B3F84(&work->rig.anim, D_actor_210600_8015A4B4, obj, work->rig.poses, work->rig.slots);
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
static const GpEnemyTaskFuncTable3 D_actor_210600_80149E24 = {
    {
        func_actor_210600_8014B8C8,
        func_actor_210600_8014B434,
        Gp_DestroyEnemy,
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
