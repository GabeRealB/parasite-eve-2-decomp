#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>
#include <psyq/stdio.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/mine_mesa.h"
#include "../../shared/player_detection.h"
#include "../../shared/actor_messages.h"
#include "../../shared/actor_contacts.h"
#define DESERT_CHASER_BUILD DESERT_CHASER_REGULAR
#include "../../shared/desert_chaser.h"

typedef struct Actor00100AngleScratch {
    /* 0x00 */ s16 x;
    /* 0x02 */ s16 y;
    /* 0x04 */ s16 z;
    /* 0x06 */ s16 pad_6;
    /* 0x08 */ s16 yaw;
    /* 0x0A */ s16 targetYaw;
    /* 0x0C */ s16 delta;
    /* 0x0E */ s16 pad_E;
} Actor00100AngleScratch;
STATIC_ASSERT_SIZEOF(Actor00100AngleScratch, 0x10);

typedef struct Actor00100ProjectScratch {
    /* 0x00 */ s16 x;
    /* 0x02 */ s16 y;
    /* 0x04 */ s16 z;
    /* 0x06 */ s16 pad_6;
    /* 0x08 */ s16 screenX;
    /* 0x0A */ s16 screenY;
    /* 0x0C */ s32 dp;
    /* 0x10 */ s32 flag;
    /* 0x14 */ s32 pad_14;
    /* 0x18 */ s32 depth;
    /* 0x1C */ s16 yaw;
    /* 0x1E */ s16 targetYaw;
    /* 0x20 */ s16 delta;
    /* 0x22 */ s16 pad_22;
} Actor00100ProjectScratch;
STATIC_ASSERT_SIZEOF(Actor00100ProjectScratch, 0x24);

/// 0x70-byte scratch from the scratch stack used by `desertChaserAvoidWalk`, the
/// 16-slot variant of the `ActorContact_Steer` walk. `flags` keeps the current
/// record's `field_4` bit 0x80, which gates `blocked` for kind 0x10000.
typedef struct Actor00100AvoidScratch16 {
    /* 0x00 */ MATRIX   m;
    /* 0x20 */ SVECTOR  dir;
    /* 0x28 */ SVECTOR3 eye;
    /* 0x2E */ byte     pad_2E[0x2];
    /* 0x30 */ s32      kind;
    /* 0x34 */ s32      flags;
    /* 0x38 */ s16      angle[16];
    /* 0x58 */ s8       ok[16];
    /* 0x68 */ s16      face;
    /* 0x6A */ s16      diff;
    /* 0x6C */ u8       i;
    /* 0x6D */ u8       j;
    /* 0x6E */ u8       count;
    /* 0x6F */ u8       blocked;
} Actor00100AvoidScratch16;
STATIC_ASSERT_SIZEOF(Actor00100AvoidScratch16, 0x70);

/// One entry of `Actor00100_D00004`: a translation plus the yaw applied after
/// it. The first component is signed, the rest are not (the code sign-extends
/// them at the use site).
typedef struct Actor00100PoseRow {
    /* 0x00 */ s16 vx;
    /* 0x02 */ u16 vy;
    /* 0x04 */ u16 vz;
    /* 0x06 */ u16 yaw;
} Actor00100PoseRow;
STATIC_ASSERT_SIZEOF(Actor00100PoseRow, 0x8);

/// Four poses `Actor00100_Fn00E58` picks between when the 0x104 message arms
/// the actor. Alignment stays 2 so a whole-table copy stays unaligned.
typedef struct Actor00100PoseTable {
    /* 0x00 */ Actor00100PoseRow rows[4];
} Actor00100PoseTable;
STATIC_ASSERT_SIZEOF(Actor00100PoseTable, 0x20);

/// Source of the four halfwords the 0x1602 handler latches into
/// `DesertChaserWork.poseVy..poseYaw`.
typedef struct Actor00100PoseSrcRow {
    /* 0x00 */ u16 vx;
    /* 0x02 */ u16 vy;
    /* 0x04 */ u16 vz;
    /* 0x06 */ u16 yaw;
} Actor00100PoseSrcRow;
STATIC_ASSERT_SIZEOF(Actor00100PoseSrcRow, 0x8);

typedef struct Actor00100PoseSrc {
    /* 0x00 */ Actor00100PoseSrcRow rows[4];
} Actor00100PoseSrc;
STATIC_ASSERT_SIZEOF(Actor00100PoseSrc, 0x20);

typedef struct Actor00100StateTable {
    TaskFunc fn[39];
} Actor00100StateTable;
STATIC_ASSERT_SIZEOF(Actor00100StateTable, 0x9C);

extern Actor00100PoseSrc Actor00100_D0BDB4;

extern EnemyParams Actor00100_D0BDA4;

extern AnimationSet* Actor00100_D1B944[26];

static TmdSource _gActor00100DesertChaserBurstLegRight;

static TmdSource _gActor00100DesertChaserBurstLegLeft;

static TmdSource _gActor00100DesertChaserBurstHead;

static TmdSource _gActor00100DesertChaserBurstTorso;

extern AnimationSet* gDesertChaserFrontAnim[9];

extern SVECTOR ActorContact_ScratchPosition;

/// The contact routines' scratch position.
static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

extern AnimationSet* gDesertChaserRearAnim[9];

extern TaskMessageEntry Actor00100_D1BA54[6];

/// Twelve `SVECTOR` hit positions `desertChaserHitEffect` picks from by damage
/// magnitude. The fourth halfword (`pad`, unused by the effect) is the model
/// part index the spawned effect anchors to.
extern SVECTOR gDesertChaserHitOffsets[12];

typedef struct Actor00100DamageScratch {
    s32 field_0;
    s32 field_4;
    s32 field_8;
    u8  pad_C[0x4];
    s16 field_10;
    s16 field_12;
    s16 field_14;
    u8  pad_16[0x2];
    s16 field_18;
    s16 field_1A;
    s16 field_1C;
    u8  pad_1E[0x2];
    s32 field_20;
    s32 field_24;
    s32 field_28;
    s16 field_2C;
    s16 field_2E;
} Actor00100DamageScratch;

static void Actor00100_Fn0B658(Task* arg0);

static AnimationSet _gActor00100Actor400100Animation129A4;

static AnimationSet _gActor00100Actor400100Animation12C60;

static AnimationSet _gActor00100Actor400100Animation13294;

static AnimationSet _gActor00100Actor400100Animation1386C;

static AnimationSet _gActor00100Actor400100Animation13CA0;

static AnimationSet _gActor00100Actor400100Animation14378;

static AnimationSet _gActor00100Actor400100Animation14978;

static AnimationSet _gActor00100Actor400100Animation14BD4;

static AnimationSet _gActor00100Actor400100Animation151F8;

static AnimationSet _gActor00100Actor400100Animation15648;

static AnimationSet _gActor00100Actor400100Animation15C2C;

static AnimationSet _gActor00100Actor400100Animation15DA8;

static AnimationSet _gActor00100Actor400100Animation1644C;

static AnimationSet _gActor00100Actor400100Animation16E64;

static AnimationSet _gActor00100Actor400100Animation17284;

static AnimationSet _gActor00100Actor400100Animation17548;

static AnimationSet _gActor00100Actor400100Animation17A4C;

static AnimationSet _gActor00100Actor400100Animation17FCC;

static AnimationSet _gActor00100Actor400100Animation1857C;

static AnimationSet _gActor00100Actor400100Animation18C3C;

static AnimationSet _gActor00100Actor400100Animation190D0;

static AnimationSet _gActor00100Actor400100Animation19918;

static AnimationSet _gActor00100Actor400100Animation1A0B4;

static AnimationSet _gActor00100Actor400100Animation1A8C0;

static AnimationSet _gActor00100Actor400100Animation1B0D4;

static AnimationSet _gActor00100Actor400100Animation1B388;

static AnimationSet _gActor00100Actor400100Animation1B6A8;

static TmdSource _gActor00100DesertChaserBody;

s32 Actor00100_Fn00E58(Task* task, s32 msgId, ActorCommand* request, s32 arg3);

s32 Actor00100_Fn0B134(Task*, s32, s32, s32);

extern s8 gDesertChaserClipStartFrames[25][25];

static const DesertChaserTaskStates gDesertChaserTaskStates;

static void Actor00100_Fn0B4D8(Task* arg0);

static void Actor00100_Fn0B730(Task* arg0);

static void Actor00100_Fn0B8D8(Task* arg0);

static void Actor00100_Fn0BB2C(Task* arg0);

static void Actor00100_Fn0BC14(Task* task);

static void Actor00100_Fn0BCBC(Enemy* enemy, Task* task);

static __inline__ s16      Actor00100_FacingAway(GfxCoord* p);
static __inline__ void     Actor00100_ScaleTransform(MATRIX* matrix, s16 amount);
static __inline__ void     Actor00100_PositionDelta(GfxCoord* coord, SVECTOR* pos);
static __inline__ s16      Actor00100_InRegion(Task* actor);
static __inline__ s16      Actor00100_InDirection(Task* actor, VECTOR* motion);
static __inline__ SVECTOR* Actor00100_AllocVector(SVECTOR** head);
static __inline__ s32      Actor00100_FindDamageHit(WorldCollisionContact* records, SVECTOR* pos);
static __inline__ void     Actor00100_SetHitState(DesertChaserWork* work);
static s32                 desertChaserAvoidWalk(GfxCoord* coord, WorldCollisionContact* recs, s16 count, SVECTOR* pos);
static void                Actor00100_Fn01900(Task* actor, s16 firstJoint, s16 secondJoint, s16 width, s16 height, u8 shade);
static void                Actor00100_Fn02C54(Enemy* arg0, Task* arg1);
static void                Actor00100_Fn0375C(Task* arg0);
static void                Actor00100_Fn04270(Task* arg0);
static void                Actor00100_Fn061FC(Task* arg0);
static void                Actor00100_Fn06C10(Task* arg0);
static void                Actor00100_Fn070DC(Task* arg0);
static void                Actor00100_Fn07650(Task* arg0);
static void                Actor00100_Fn08E7C(Task* arg0);
static void                Actor00100_Fn09310(Task* arg0);
static void                Actor00100_Fn09724(Task* arg0);
static void                Actor00100_Fn09CCC(Task* arg0);
static void                Actor00100_Fn0A288(Enemy* enemy, Task* actor);
static s16                 Actor00100_Fn0B13C(Task* arg0);
static void                Actor00100_Fn0B3DC(Task* arg0, s16 arg1, s16 arg2);
static void                Actor00100_Fn0BC1C(Task* arg0);

static __inline__ s16 Actor00100_FacingAway(GfxCoord* p)
{
    s16 angle = ratan2(-p->coord.m[2][0], p->coord.m[2][2]);
    s32 value = angle;
    if (value < 0)
        value = -value;
    if (value >= 0x501) {
        if (p->coord.t[0] < 0x2AF9)
            return 1;
        if ((s16)ratan2(-p->coord.m[2][0], p->coord.m[2][2]) >= 0x708)
            return 1;
        if ((s16)ratan2(-p->coord.m[2][0], p->coord.m[2][2]) <= 0)
            return 1;
    }
    return 0;
}

static __inline__ void Actor00100_ScaleTransform(MATRIX* matrix, s16 amount)
{
    ActorScaleMatrixScratch* head;
    ActorScaleMatrixScratch* scratch;
    SVECTOR*                 vec;

    head                                          = SCRATCH_STACK_CURSOR(ActorScaleMatrixScratch);
    scratch                                       = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleMatrixScratch) = scratch;
    scratch->scale.vz                             = amount;
    scratch->scale.vy                             = amount;
    head[-1].scale.vx                             = amount;
    ScaleMatrix(matrix, &scratch->scale);
    scratch->trans.vx = matrix->t[0];
    scratch->trans.vy = matrix->t[1];
    scratch->trans.vz = matrix->t[2];
    gte_lddp(amount);
    vec = &(head - 1)->trans;
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(vec);
    matrix->t[0] = scratch->trans.vx;
    matrix->t[1] = scratch->trans.vy;
    matrix->t[2] = scratch->trans.vz;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleMatrixScratch);
}

static __inline__ void Actor00100_PositionDelta(GfxCoord* coord, SVECTOR* pos)
{
    pos->vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    pos->vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    pos->vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
}

static __inline__ s16 Actor00100_InRegion(Task* actor)
{
    GfxCoord* coord = actor->extra.tmd->coords;
    if ((u32)(coord->coord.t[0] - 0x1541) < 0x196DU) {
        if (coord->coord.t[2] < 0x5B4)
            return 1;
    }
    return 0;
}

static __inline__ s16 Actor00100_InDirection(Task* actor, VECTOR* motion)
{
    GfxCoord* coord = actor->extra.tmd->coords;
    if (abs((s16)ratan2(motion->vx, motion->vz)) >= 0x501) {
        if (coord->coord.t[0] < 0x2AF9)
            return 1;
        if ((s16)ratan2(motion->vx, motion->vz) >= 0x708)
            return 1;
        if ((s16)ratan2(motion->vx, motion->vz) <= 0)
            return 1;
    }
    return 0;
}

static __inline__ SVECTOR* Actor00100_AllocVector(SVECTOR** head)
{
    SVECTOR* p                     = SCRATCH_HEAD_AT(head, SVECTOR) - 1;
    SCRATCH_HEAD_AT(head, SVECTOR) = p;
    return p;
}

// Each handler receives the argument view used by its message ID.

DamageAttack Actor00100_D0BD90[5] = {
    { 30, 0 },
    { 30, 0 },
    { 18, 0 },
    { 18, 0 },
    { 0xFFFF, 0 },
};

EnemyParams Actor00100_D0BDA4 = { Actor00100_D0BD90, 200, 75, 50, 4, 100, 10, 100, 0 };

Actor00100PoseSrc Actor00100_D0BDB4 = { { { 60, 36, 10, 150 }, { 40, 26, 10, 120 }, { 20, 18, 10, 120 }, { 60, 60, 10, 150 } } };

static TmdBone _gActor00100DesertChaserBodySkeleton[18] = {
#include "assets/desert_chaser_body_skeleton.inc"
};

static u32 _gActor00100DesertChaserBodyPartVerts[18] = {
#include "assets/desert_chaser_body_partVerts.inc"
};

static SVECTOR _gActor00100DesertChaserBodyVerts[266] = {
#include "assets/desert_chaser_body_verts.inc"
};

static SVECTOR _gActor00100DesertChaserBodyNormals[324] = {
#include "assets/desert_chaser_body_normals.inc"
};

static u32 _gActor00100DesertChaserBodyStream[3435] = {
#include "assets/desert_chaser_body_stream.inc"
};

static TmdSource _gActor00100DesertChaserBody = {
    0,
    17616,
    5944,
    18,
    _gActor00100DesertChaserBodyPartVerts,
    _gActor00100DesertChaserBodyVerts,
    _gActor00100DesertChaserBodyNormals,
    _gActor00100DesertChaserBodySkeleton,
    _gActor00100DesertChaserBodyStream,
};

static TmdBone _gActor00100DesertChaserBurstLegRightSkeleton[1] = {
#include "assets/desert_chaser_burst_leg_right_skeleton.inc"
};

static u32 _gActor00100DesertChaserBurstLegRightPartVerts[1] = {
#include "assets/desert_chaser_burst_leg_right_partVerts.inc"
};

static SVECTOR _gActor00100DesertChaserBurstLegRightVerts[21] = {
#include "assets/desert_chaser_burst_leg_right_verts.inc"
};

static SVECTOR _gActor00100DesertChaserBurstLegRightNormals[1] = {
#include "assets/desert_chaser_burst_leg_right_normals.inc"
};

static u32 _gActor00100DesertChaserBurstLegRightStream[233] = {
#include "assets/desert_chaser_burst_leg_right_stream.inc"
};

static TmdSource _gActor00100DesertChaserBurstLegRight = {
    0,
    1536,
    0,
    1,
    _gActor00100DesertChaserBurstLegRightPartVerts,
    _gActor00100DesertChaserBurstLegRightVerts,
    _gActor00100DesertChaserBurstLegRightNormals,
    _gActor00100DesertChaserBurstLegRightSkeleton,
    _gActor00100DesertChaserBurstLegRightStream,
};

static TmdBone _gActor00100DesertChaserBurstLegLeftSkeleton[1] = {
#include "assets/desert_chaser_burst_leg_left_skeleton.inc"
};

static u32 _gActor00100DesertChaserBurstLegLeftPartVerts[1] = {
#include "assets/desert_chaser_burst_leg_left_partVerts.inc"
};

static SVECTOR _gActor00100DesertChaserBurstLegLeftVerts[23] = {
#include "assets/desert_chaser_burst_leg_left_verts.inc"
};

static SVECTOR _gActor00100DesertChaserBurstLegLeftNormals[1] = {
#include "assets/desert_chaser_burst_leg_left_normals.inc"
};

static u32 _gActor00100DesertChaserBurstLegLeftStream[242] = {
#include "assets/desert_chaser_burst_leg_left_stream.inc"
};

static TmdSource _gActor00100DesertChaserBurstLegLeft = {
    0,
    1612,
    0,
    1,
    _gActor00100DesertChaserBurstLegLeftPartVerts,
    _gActor00100DesertChaserBurstLegLeftVerts,
    _gActor00100DesertChaserBurstLegLeftNormals,
    _gActor00100DesertChaserBurstLegLeftSkeleton,
    _gActor00100DesertChaserBurstLegLeftStream,
};

static TmdBone _gActor00100DesertChaserBurstHeadSkeleton[1] = {
#include "assets/desert_chaser_burst_head_skeleton.inc"
};

static u32 _gActor00100DesertChaserBurstHeadPartVerts[1] = {
#include "assets/desert_chaser_burst_head_partVerts.inc"
};

static SVECTOR _gActor00100DesertChaserBurstHeadVerts[59] = {
#include "assets/desert_chaser_burst_head_verts.inc"
};

static SVECTOR _gActor00100DesertChaserBurstHeadNormals[81] = {
#include "assets/desert_chaser_burst_head_normals.inc"
};

static u32 _gActor00100DesertChaserBurstHeadStream[556] = {
#include "assets/desert_chaser_burst_head_stream.inc"
};

static TmdSource _gActor00100DesertChaserBurstHead = {
    0,
    3812,
    0,
    1,
    _gActor00100DesertChaserBurstHeadPartVerts,
    _gActor00100DesertChaserBurstHeadVerts,
    _gActor00100DesertChaserBurstHeadNormals,
    _gActor00100DesertChaserBurstHeadSkeleton,
    _gActor00100DesertChaserBurstHeadStream,
};

static TmdBone _gActor00100DesertChaserBurstTorsoSkeleton[1] = {
#include "assets/desert_chaser_burst_torso_skeleton.inc"
};

static u32 _gActor00100DesertChaserBurstTorsoPartVerts[1] = {
#include "assets/desert_chaser_burst_torso_partVerts.inc"
};

static SVECTOR _gActor00100DesertChaserBurstTorsoVerts[19] = {
#include "assets/desert_chaser_burst_torso_verts.inc"
};

static SVECTOR _gActor00100DesertChaserBurstTorsoNormals[31] = {
#include "assets/desert_chaser_burst_torso_normals.inc"
};

static u32 _gActor00100DesertChaserBurstTorsoStream[193] = {
#include "assets/desert_chaser_burst_torso_stream.inc"
};

static TmdSource _gActor00100DesertChaserBurstTorso = {
    0,
    1248,
    0,
    1,
    _gActor00100DesertChaserBurstTorsoPartVerts,
    _gActor00100DesertChaserBurstTorsoVerts,
    _gActor00100DesertChaserBurstTorsoNormals,
    _gActor00100DesertChaserBurstTorsoSkeleton,
    _gActor00100DesertChaserBurstTorsoStream,
};

static AnimationPackedPose _gActor00100Actor400100Animation129A4Bank1[10] = {
#include "assets/actor_400100_animation_129A4_bank1.inc"
};

static AnimationPackedRotation _gActor00100Actor400100Animation129A4Bank4[110] = {
#include "assets/actor_400100_animation_129A4_bank4.inc"
};

static AnimationRecord _gActor00100Actor400100Animation129A4Records[175] = {
#include "assets/actor_400100_animation_129A4_records.inc"
};

static u16 _gActor00100Actor400100Animation129A4Indices[18] = {
#include "assets/actor_400100_animation_129A4_indices.inc"
};

static AnimationSet _gActor00100Actor400100Animation129A4 = {
    _gActor00100Actor400100Animation129A4Records,
    _gActor00100Actor400100Animation129A4Indices,
    { NULL, _gActor00100Actor400100Animation129A4Bank1, NULL, NULL, _gActor00100Actor400100Animation129A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00100Actor400100Animation12C60Bank1[5] = {
#include "assets/actor_400100_animation_12C60_bank1.inc"
};

static AnimationPackedRotation _gActor00100Actor400100Animation12C60Bank4[29] = {
#include "assets/actor_400100_animation_12C60_bank4.inc"
};

static AnimationRecord _gActor00100Actor400100Animation12C60Records[112] = {
#include "assets/actor_400100_animation_12C60_records.inc"
};

static u16 _gActor00100Actor400100Animation12C60Indices[18] = {
#include "assets/actor_400100_animation_12C60_indices.inc"
};

static AnimationSet _gActor00100Actor400100Animation12C60 = {
    _gActor00100Actor400100Animation12C60Records,
    _gActor00100Actor400100Animation12C60Indices,
    { NULL, _gActor00100Actor400100Animation12C60Bank1, NULL, NULL, _gActor00100Actor400100Animation12C60Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00100Actor400100Animation13294Bank1[13] = {
#include "assets/actor_400100_animation_13294_bank1.inc"
};

static AnimationPackedRotation _gActor00100Actor400100Animation13294Bank4[129] = {
#include "assets/actor_400100_animation_13294_bank4.inc"
};

static AnimationRecord _gActor00100Actor400100Animation13294Records[210] = {
#include "assets/actor_400100_animation_13294_records.inc"
};

static u16 _gActor00100Actor400100Animation13294Indices[18] = {
#include "assets/actor_400100_animation_13294_indices.inc"
};

static AnimationSet _gActor00100Actor400100Animation13294 = {
    _gActor00100Actor400100Animation13294Records,
    _gActor00100Actor400100Animation13294Indices,
    { NULL, _gActor00100Actor400100Animation13294Bank1, NULL, NULL, _gActor00100Actor400100Animation13294Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00100Actor400100Animation1386CBank1[11] = {
#include "assets/actor_400100_animation_1386C_bank1.inc"
};

static AnimationPackedRotation _gActor00100Actor400100Animation1386CBank4[131] = {
#include "assets/actor_400100_animation_1386C_bank4.inc"
};

static AnimationRecord _gActor00100Actor400100Animation1386CRecords[191] = {
#include "assets/actor_400100_animation_1386C_records.inc"
};

static u16 _gActor00100Actor400100Animation1386CIndices[18] = {
#include "assets/actor_400100_animation_1386C_indices.inc"
};

static AnimationSet _gActor00100Actor400100Animation1386C = {
    _gActor00100Actor400100Animation1386CRecords,
    _gActor00100Actor400100Animation1386CIndices,
    { NULL, _gActor00100Actor400100Animation1386CBank1, NULL, NULL, _gActor00100Actor400100Animation1386CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00100Actor400100Animation13CA0Bank1[9] = {
#include "assets/actor_400100_animation_13CA0_bank1.inc"
};

static AnimationPackedRotation _gActor00100Actor400100Animation13CA0Bank4[83] = {
#include "assets/actor_400100_animation_13CA0_bank4.inc"
};

static AnimationRecord _gActor00100Actor400100Animation13CA0Records[140] = {
#include "assets/actor_400100_animation_13CA0_records.inc"
};

static u16 _gActor00100Actor400100Animation13CA0Indices[18] = {
#include "assets/actor_400100_animation_13CA0_indices.inc"
};

static AnimationSet _gActor00100Actor400100Animation13CA0 = {
    _gActor00100Actor400100Animation13CA0Records,
    _gActor00100Actor400100Animation13CA0Indices,
    { NULL, _gActor00100Actor400100Animation13CA0Bank1, NULL, NULL, _gActor00100Actor400100Animation13CA0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00100Actor400100Animation14378Bank1[19] = {
#include "assets/actor_400100_animation_14378_bank1.inc"
};

static AnimationPackedRotation _gActor00100Actor400100Animation14378Bank4[138] = {
#include "assets/actor_400100_animation_14378_bank4.inc"
};

static AnimationRecord _gActor00100Actor400100Animation14378Records[224] = {
#include "assets/actor_400100_animation_14378_records.inc"
};

static u16 _gActor00100Actor400100Animation14378Indices[18] = {
#include "assets/actor_400100_animation_14378_indices.inc"
};

static AnimationSet _gActor00100Actor400100Animation14378 = {
    _gActor00100Actor400100Animation14378Records,
    _gActor00100Actor400100Animation14378Indices,
    { NULL, _gActor00100Actor400100Animation14378Bank1, NULL, NULL, _gActor00100Actor400100Animation14378Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00100Actor400100Animation14978Bank1[14] = {
#include "assets/actor_400100_animation_14978_bank1.inc"
};

static AnimationPackedRotation _gActor00100Actor400100Animation14978Bank4[99] = {
#include "assets/actor_400100_animation_14978_bank4.inc"
};

static AnimationRecord _gActor00100Actor400100Animation14978Records[224] = {
#include "assets/actor_400100_animation_14978_records.inc"
};

static u16 _gActor00100Actor400100Animation14978Indices[18] = {
#include "assets/actor_400100_animation_14978_indices.inc"
};

static AnimationSet _gActor00100Actor400100Animation14978 = {
    _gActor00100Actor400100Animation14978Records,
    _gActor00100Actor400100Animation14978Indices,
    { NULL, _gActor00100Actor400100Animation14978Bank1, NULL, NULL, _gActor00100Actor400100Animation14978Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00100Actor400100Animation14BD4Bank1[4] = {
#include "assets/actor_400100_animation_14BD4_bank1.inc"
};

static AnimationPackedRotation _gActor00100Actor400100Animation14BD4Bank4[34] = {
#include "assets/actor_400100_animation_14BD4_bank4.inc"
};

static AnimationRecord _gActor00100Actor400100Animation14BD4Records[86] = {
#include "assets/actor_400100_animation_14BD4_records.inc"
};

static u16 _gActor00100Actor400100Animation14BD4Indices[18] = {
#include "assets/actor_400100_animation_14BD4_indices.inc"
};

static AnimationSet _gActor00100Actor400100Animation14BD4 = {
    _gActor00100Actor400100Animation14BD4Records,
    _gActor00100Actor400100Animation14BD4Indices,
    { NULL, _gActor00100Actor400100Animation14BD4Bank1, NULL, NULL, _gActor00100Actor400100Animation14BD4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00100Actor400100Animation151F8Bank1[12] = {
#include "assets/actor_400100_animation_151F8_bank1.inc"
};

static AnimationPackedRotation _gActor00100Actor400100Animation151F8Bank4[147] = {
#include "assets/actor_400100_animation_151F8_bank4.inc"
};

static AnimationRecord _gActor00100Actor400100Animation151F8Records[191] = {
#include "assets/actor_400100_animation_151F8_records.inc"
};

static u16 _gActor00100Actor400100Animation151F8Indices[18] = {
#include "assets/actor_400100_animation_151F8_indices.inc"
};

static AnimationSet _gActor00100Actor400100Animation151F8 = {
    _gActor00100Actor400100Animation151F8Records,
    _gActor00100Actor400100Animation151F8Indices,
    { NULL, _gActor00100Actor400100Animation151F8Bank1, NULL, NULL, _gActor00100Actor400100Animation151F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00100Actor400100Animation15648Bank1[9] = {
#include "assets/actor_400100_animation_15648_bank1.inc"
};

static AnimationPackedRotation _gActor00100Actor400100Animation15648Bank4[73] = {
#include "assets/actor_400100_animation_15648_bank4.inc"
};

static AnimationRecord _gActor00100Actor400100Animation15648Records[157] = {
#include "assets/actor_400100_animation_15648_records.inc"
};

static u16 _gActor00100Actor400100Animation15648Indices[18] = {
#include "assets/actor_400100_animation_15648_indices.inc"
};

static AnimationSet _gActor00100Actor400100Animation15648 = {
    _gActor00100Actor400100Animation15648Records,
    _gActor00100Actor400100Animation15648Indices,
    { NULL, _gActor00100Actor400100Animation15648Bank1, NULL, NULL, _gActor00100Actor400100Animation15648Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00100Actor400100Animation15C2CBank1[12] = {
#include "assets/actor_400100_animation_15C2C_bank1.inc"
};

static AnimationPackedRotation _gActor00100Actor400100Animation15C2CBank4[126] = {
#include "assets/actor_400100_animation_15C2C_bank4.inc"
};

static AnimationRecord _gActor00100Actor400100Animation15C2CRecords[196] = {
#include "assets/actor_400100_animation_15C2C_records.inc"
};

static u16 _gActor00100Actor400100Animation15C2CIndices[18] = {
#include "assets/actor_400100_animation_15C2C_indices.inc"
};

static AnimationSet _gActor00100Actor400100Animation15C2C = {
    _gActor00100Actor400100Animation15C2CRecords,
    _gActor00100Actor400100Animation15C2CIndices,
    { NULL, _gActor00100Actor400100Animation15C2CBank1, NULL, NULL, _gActor00100Actor400100Animation15C2CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00100Actor400100Animation15DA8Bank1[2] = {
#include "assets/actor_400100_animation_15DA8_bank1.inc"
};

static AnimationPackedRotation _gActor00100Actor400100Animation15DA8Bank4[16] = {
#include "assets/actor_400100_animation_15DA8_bank4.inc"
};

static AnimationRecord _gActor00100Actor400100Animation15DA8Records[54] = {
#include "assets/actor_400100_animation_15DA8_records.inc"
};

static u16 _gActor00100Actor400100Animation15DA8Indices[18] = {
#include "assets/actor_400100_animation_15DA8_indices.inc"
};

static AnimationSet _gActor00100Actor400100Animation15DA8 = {
    _gActor00100Actor400100Animation15DA8Records,
    _gActor00100Actor400100Animation15DA8Indices,
    { NULL, _gActor00100Actor400100Animation15DA8Bank1, NULL, NULL, _gActor00100Actor400100Animation15DA8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00100Actor400100Animation1644CBank1[13] = {
#include "assets/actor_400100_animation_1644C_bank1.inc"
};

static AnimationPackedRotation _gActor00100Actor400100Animation1644CBank4[158] = {
#include "assets/actor_400100_animation_1644C_bank4.inc"
};

static AnimationRecord _gActor00100Actor400100Animation1644CRecords[209] = {
#include "assets/actor_400100_animation_1644C_records.inc"
};

static u16 _gActor00100Actor400100Animation1644CIndices[18] = {
#include "assets/actor_400100_animation_1644C_indices.inc"
};

static AnimationSet _gActor00100Actor400100Animation1644C = {
    _gActor00100Actor400100Animation1644CRecords,
    _gActor00100Actor400100Animation1644CIndices,
    { NULL, _gActor00100Actor400100Animation1644CBank1, NULL, NULL, _gActor00100Actor400100Animation1644CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00100Actor400100Animation16E64Bank1[30] = {
#include "assets/actor_400100_animation_16E64_bank1.inc"
};

static AnimationPackedRotation _gActor00100Actor400100Animation16E64Bank4[221] = {
#include "assets/actor_400100_animation_16E64_bank4.inc"
};

static AnimationRecord _gActor00100Actor400100Animation16E64Records[316] = {
#include "assets/actor_400100_animation_16E64_records.inc"
};

static u16 _gActor00100Actor400100Animation16E64Indices[18] = {
#include "assets/actor_400100_animation_16E64_indices.inc"
};

static AnimationSet _gActor00100Actor400100Animation16E64 = {
    _gActor00100Actor400100Animation16E64Records,
    _gActor00100Actor400100Animation16E64Indices,
    { NULL, _gActor00100Actor400100Animation16E64Bank1, NULL, NULL, _gActor00100Actor400100Animation16E64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00100Actor400100Animation17284Bank1[12] = {
#include "assets/actor_400100_animation_17284_bank1.inc"
};

static AnimationPackedRotation _gActor00100Actor400100Animation17284Bank4[77] = {
#include "assets/actor_400100_animation_17284_bank4.inc"
};

static AnimationRecord _gActor00100Actor400100Animation17284Records[132] = {
#include "assets/actor_400100_animation_17284_records.inc"
};

static u16 _gActor00100Actor400100Animation17284Indices[18] = {
#include "assets/actor_400100_animation_17284_indices.inc"
};

static AnimationSet _gActor00100Actor400100Animation17284 = {
    _gActor00100Actor400100Animation17284Records,
    _gActor00100Actor400100Animation17284Indices,
    { NULL, _gActor00100Actor400100Animation17284Bank1, NULL, NULL, _gActor00100Actor400100Animation17284Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00100Actor400100Animation17548Bank1[4] = {
#include "assets/actor_400100_animation_17548_bank1.inc"
};

static AnimationPackedRotation _gActor00100Actor400100Animation17548Bank4[45] = {
#include "assets/actor_400100_animation_17548_bank4.inc"
};

static AnimationRecord _gActor00100Actor400100Animation17548Records[101] = {
#include "assets/actor_400100_animation_17548_records.inc"
};

static u16 _gActor00100Actor400100Animation17548Indices[18] = {
#include "assets/actor_400100_animation_17548_indices.inc"
};

static AnimationSet _gActor00100Actor400100Animation17548 = {
    _gActor00100Actor400100Animation17548Records,
    _gActor00100Actor400100Animation17548Indices,
    { NULL, _gActor00100Actor400100Animation17548Bank1, NULL, NULL, _gActor00100Actor400100Animation17548Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00100Actor400100Animation17A4CBank1[9] = {
#include "assets/actor_400100_animation_17A4C_bank1.inc"
};

static AnimationPackedRotation _gActor00100Actor400100Animation17A4CBank4[123] = {
#include "assets/actor_400100_animation_17A4C_bank4.inc"
};

static AnimationRecord _gActor00100Actor400100Animation17A4CRecords[152] = {
#include "assets/actor_400100_animation_17A4C_records.inc"
};

static u16 _gActor00100Actor400100Animation17A4CIndices[18] = {
#include "assets/actor_400100_animation_17A4C_indices.inc"
};

static AnimationSet _gActor00100Actor400100Animation17A4C = {
    _gActor00100Actor400100Animation17A4CRecords,
    _gActor00100Actor400100Animation17A4CIndices,
    { NULL, _gActor00100Actor400100Animation17A4CBank1, NULL, NULL, _gActor00100Actor400100Animation17A4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00100Actor400100Animation17FCCBank1[13] = {
#include "assets/actor_400100_animation_17FCC_bank1.inc"
};

static AnimationPackedRotation _gActor00100Actor400100Animation17FCCBank4[113] = {
#include "assets/actor_400100_animation_17FCC_bank4.inc"
};

static AnimationRecord _gActor00100Actor400100Animation17FCCRecords[181] = {
#include "assets/actor_400100_animation_17FCC_records.inc"
};

static u16 _gActor00100Actor400100Animation17FCCIndices[18] = {
#include "assets/actor_400100_animation_17FCC_indices.inc"
};

static AnimationSet _gActor00100Actor400100Animation17FCC = {
    _gActor00100Actor400100Animation17FCCRecords,
    _gActor00100Actor400100Animation17FCCIndices,
    { NULL, _gActor00100Actor400100Animation17FCCBank1, NULL, NULL, _gActor00100Actor400100Animation17FCCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00100Actor400100Animation1857CBank1[14] = {
#include "assets/actor_400100_animation_1857C_bank1.inc"
};

static AnimationPackedRotation _gActor00100Actor400100Animation1857CBank4[116] = {
#include "assets/actor_400100_animation_1857C_bank4.inc"
};

static AnimationRecord _gActor00100Actor400100Animation1857CRecords[187] = {
#include "assets/actor_400100_animation_1857C_records.inc"
};

static u16 _gActor00100Actor400100Animation1857CIndices[18] = {
#include "assets/actor_400100_animation_1857C_indices.inc"
};

static AnimationSet _gActor00100Actor400100Animation1857C = {
    _gActor00100Actor400100Animation1857CRecords,
    _gActor00100Actor400100Animation1857CIndices,
    { NULL, _gActor00100Actor400100Animation1857CBank1, NULL, NULL, _gActor00100Actor400100Animation1857CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00100Actor400100Animation18C3CBank1[13] = {
#include "assets/actor_400100_animation_18C3C_bank1.inc"
};

static AnimationPackedRotation _gActor00100Actor400100Animation18C3CBank4[160] = {
#include "assets/actor_400100_animation_18C3C_bank4.inc"
};

static AnimationRecord _gActor00100Actor400100Animation18C3CRecords[214] = {
#include "assets/actor_400100_animation_18C3C_records.inc"
};

static u16 _gActor00100Actor400100Animation18C3CIndices[18] = {
#include "assets/actor_400100_animation_18C3C_indices.inc"
};

static AnimationSet _gActor00100Actor400100Animation18C3C = {
    _gActor00100Actor400100Animation18C3CRecords,
    _gActor00100Actor400100Animation18C3CIndices,
    { NULL, _gActor00100Actor400100Animation18C3CBank1, NULL, NULL, _gActor00100Actor400100Animation18C3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00100Actor400100Animation190D0Bank1[8] = {
#include "assets/actor_400100_animation_190D0_bank1.inc"
};

static AnimationPackedRotation _gActor00100Actor400100Animation190D0Bank4[100] = {
#include "assets/actor_400100_animation_190D0_bank4.inc"
};

static AnimationRecord _gActor00100Actor400100Animation190D0Records[150] = {
#include "assets/actor_400100_animation_190D0_records.inc"
};

static u16 _gActor00100Actor400100Animation190D0Indices[18] = {
#include "assets/actor_400100_animation_190D0_indices.inc"
};

static AnimationSet _gActor00100Actor400100Animation190D0 = {
    _gActor00100Actor400100Animation190D0Records,
    _gActor00100Actor400100Animation190D0Indices,
    { NULL, _gActor00100Actor400100Animation190D0Bank1, NULL, NULL, _gActor00100Actor400100Animation190D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00100Actor400100Animation19918Bank1[21] = {
#include "assets/actor_400100_animation_19918_bank1.inc"
};

static AnimationPackedRotation _gActor00100Actor400100Animation19918Bank4[193] = {
#include "assets/actor_400100_animation_19918_bank4.inc"
};

static AnimationRecord _gActor00100Actor400100Animation19918Records[254] = {
#include "assets/actor_400100_animation_19918_records.inc"
};

static u16 _gActor00100Actor400100Animation19918Indices[20] = {
#include "assets/actor_400100_animation_19918_indices.inc"
};

static AnimationSet _gActor00100Actor400100Animation19918 = {
    _gActor00100Actor400100Animation19918Records,
    _gActor00100Actor400100Animation19918Indices,
    { NULL, _gActor00100Actor400100Animation19918Bank1, NULL, NULL, _gActor00100Actor400100Animation19918Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00100Actor400100Animation1A0B4Bank1[20] = {
#include "assets/actor_400100_animation_1A0B4_bank1.inc"
};

static AnimationPackedRotation _gActor00100Actor400100Animation1A0B4Bank4[172] = {
#include "assets/actor_400100_animation_1A0B4_bank4.inc"
};

static AnimationRecord _gActor00100Actor400100Animation1A0B4Records[235] = {
#include "assets/actor_400100_animation_1A0B4_records.inc"
};

static u16 _gActor00100Actor400100Animation1A0B4Indices[20] = {
#include "assets/actor_400100_animation_1A0B4_indices.inc"
};

static AnimationSet _gActor00100Actor400100Animation1A0B4 = {
    _gActor00100Actor400100Animation1A0B4Records,
    _gActor00100Actor400100Animation1A0B4Indices,
    { NULL, _gActor00100Actor400100Animation1A0B4Bank1, NULL, NULL, _gActor00100Actor400100Animation1A0B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00100Actor400100Animation1A8C0Bank1[15] = {
#include "assets/actor_400100_animation_1A8C0_bank1.inc"
};

static AnimationPackedRotation _gActor00100Actor400100Animation1A8C0Bank4[206] = {
#include "assets/actor_400100_animation_1A8C0_bank4.inc"
};

static AnimationRecord _gActor00100Actor400100Animation1A8C0Records[244] = {
#include "assets/actor_400100_animation_1A8C0_records.inc"
};

static u16 _gActor00100Actor400100Animation1A8C0Indices[20] = {
#include "assets/actor_400100_animation_1A8C0_indices.inc"
};

static AnimationSet _gActor00100Actor400100Animation1A8C0 = {
    _gActor00100Actor400100Animation1A8C0Records,
    _gActor00100Actor400100Animation1A8C0Indices,
    { NULL, _gActor00100Actor400100Animation1A8C0Bank1, NULL, NULL, _gActor00100Actor400100Animation1A8C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00100Actor400100Animation1B0D4Bank1[14] = {
#include "assets/actor_400100_animation_1B0D4_bank1.inc"
};

static AnimationPackedRotation _gActor00100Actor400100Animation1B0D4Bank4[207] = {
#include "assets/actor_400100_animation_1B0D4_bank4.inc"
};

static AnimationRecord _gActor00100Actor400100Animation1B0D4Records[248] = {
#include "assets/actor_400100_animation_1B0D4_records.inc"
};

static u16 _gActor00100Actor400100Animation1B0D4Indices[20] = {
#include "assets/actor_400100_animation_1B0D4_indices.inc"
};

static AnimationSet _gActor00100Actor400100Animation1B0D4 = {
    _gActor00100Actor400100Animation1B0D4Records,
    _gActor00100Actor400100Animation1B0D4Indices,
    { NULL, _gActor00100Actor400100Animation1B0D4Bank1, NULL, NULL, _gActor00100Actor400100Animation1B0D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00100Actor400100Animation1B388Bank1[5] = {
#include "assets/actor_400100_animation_1B388_bank1.inc"
};

static AnimationPackedRotation _gActor00100Actor400100Animation1B388Bank4[55] = {
#include "assets/actor_400100_animation_1B388_bank4.inc"
};

static AnimationRecord _gActor00100Actor400100Animation1B388Records[83] = {
#include "assets/actor_400100_animation_1B388_records.inc"
};

static u16 _gActor00100Actor400100Animation1B388Indices[20] = {
#include "assets/actor_400100_animation_1B388_indices.inc"
};

static AnimationSet _gActor00100Actor400100Animation1B388 = {
    _gActor00100Actor400100Animation1B388Records,
    _gActor00100Actor400100Animation1B388Indices,
    { NULL, _gActor00100Actor400100Animation1B388Bank1, NULL, NULL, _gActor00100Actor400100Animation1B388Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor00100Actor400100Animation1B6A8Bank1[6] = {
#include "assets/actor_400100_animation_1B6A8_bank1.inc"
};

static AnimationPackedRotation _gActor00100Actor400100Animation1B6A8Bank4[66] = {
#include "assets/actor_400100_animation_1B6A8_bank4.inc"
};

static AnimationRecord _gActor00100Actor400100Animation1B6A8Records[96] = {
#include "assets/actor_400100_animation_1B6A8_records.inc"
};

static u16 _gActor00100Actor400100Animation1B6A8Indices[20] = {
#include "assets/actor_400100_animation_1B6A8_indices.inc"

};

static AnimationSet _gActor00100Actor400100Animation1B6A8 = {
    _gActor00100Actor400100Animation1B6A8Records,
    _gActor00100Actor400100Animation1B6A8Indices,
    { NULL, _gActor00100Actor400100Animation1B6A8Bank1, NULL, NULL, _gActor00100Actor400100Animation1B6A8Bank4, NULL, NULL, NULL },
};

/// Blend duration in frames, indexed by [previous animation][next animation].
s8 gDesertChaserClipStartFrames[25][25] = {
    /*  0 */ { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 0, 0, 0, 0, 0, 0 },
    /*  1 */ { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  2 */ { 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  3 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  4 */ { 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  5 */ { 5, 0, 0, 0, 0, 6, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  6 */ { 5, 0, 5, 0, 0, 6, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  7 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  8 */ { 5, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  9 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 10 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 11 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 12 */ { 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 13 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 14 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 15 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 16 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 17 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 18 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 19 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 20 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 21 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 22 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 23 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 24 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AnimationSet* Actor00100_D1B944[26] = {
    &_gActor00100Actor400100Animation129A4,
    &_gActor00100Actor400100Animation12C60,
    &_gActor00100Actor400100Animation13294,
    &_gActor00100Actor400100Animation1386C,
    &_gActor00100Actor400100Animation13CA0,
    &_gActor00100Actor400100Animation14378,
    &_gActor00100Actor400100Animation14978,
    &_gActor00100Actor400100Animation14BD4,
    &_gActor00100Actor400100Animation151F8,
    &_gActor00100Actor400100Animation15648,
    &_gActor00100Actor400100Animation15C2C,
    &_gActor00100Actor400100Animation15DA8,
    &_gActor00100Actor400100Animation1644C,
    &_gActor00100Actor400100Animation16E64,
    &_gActor00100Actor400100Animation17284,
    &_gActor00100Actor400100Animation17548,
    &_gActor00100Actor400100Animation17A4C,
    &_gActor00100Actor400100Animation17FCC,
    &_gActor00100Actor400100Animation1857C,
    &_gActor00100Actor400100Animation18C3C,
    &_gActor00100Actor400100Animation190D0,
    &_gActor00100Actor400100Animation15C2C,
    NULL,
    NULL,
    NULL,
    NULL,
};

AnimationSet* gDesertChaserFrontAnim[9] = { NULL, &_gActor00100Actor400100Animation19918, &_gActor00100Actor400100Animation1A8C0, &_gActor00100Actor400100Animation1B388, NULL, NULL, NULL, NULL, NULL };

AnimationSet* gDesertChaserRearAnim[9] = { NULL, &_gActor00100Actor400100Animation1A0B4, &_gActor00100Actor400100Animation1B0D4, &_gActor00100Actor400100Animation1B6A8, NULL, NULL, NULL, NULL, NULL };

SVECTOR gDesertChaserHitOffsets[12] = {
    { 60, -12, 30, 2 },
    { -50, -130, 29, 2 },
    { 20, -70, 25, 2 },
    { -30, -65, 25, 2 },
    { 60, -120, 30, 2 },
    { 20, -20, -5, 2 },
    { -15, -50, 0, 2 },
    { 2, 10, -15, 2 },
    { 14, 0, 0, 7 },
    { 25, 0, 0, 2 },
    { -14, 0, 0, 9 },
    { -25, 0, 0, 2 },
};

TaskMessageEntry Actor00100_D1BA54[6] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, Actor00100_Fn00E58 },
    { 2015, Actor00100_Fn0B134 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetVisibility },
    { ACTOR_MESSAGE_IS_PRESENT, actorMsgIsPresent },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceRecordYaw },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc Actor00100_D1BA84 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, desertChaserTask, { .model = &_gActor00100DesertChaserBody } };

SVECTOR ActorContact_ScratchPosition;

static __inline__ s32 Actor00100_FindDamageHit(WorldCollisionContact* records, SVECTOR* pos)
{
    s16 i;
    for (i = 0; i < 5; i++) {
        if (!records[i].key.value)
            break;
        if ((records[i].key.value & 0xFFFF0000) == 0x20000) {
            pos->vx = records[i].point.vx;
            pos->vy = records[i].point.vy;
            pos->vz = records[i].point.vz;
            return records[i].key.value;
        }
    }
    return 0;
}

static __inline__ void Actor00100_SetHitState(DesertChaserWork* work)
{
    s32 state = work->field_0;
    if (state == 4 || state == 7 || state == 0x21 || state == 0x14 || state == 0xB || state == 0x11 || (state == 0x24 && work->field_6 < 10))
        work->field_0 = 7;
    else
        work->field_0 = 0x14;
}

// Transition durations indexed by previous * 25 + next animation.

#include "../../shared/actor_contacts_turn_joint.inc.c"

#include "../../shared/actor_contacts_steer.inc.c"

#include "../../shared/actor_contacts_push_contact.inc.c"

#include "../../shared/player_detection_sight.inc.c"

/// The four poses `Actor00100_Fn00E58` picks between for message 0x104.
static const Actor00100PoseTable Actor00100_D00004 = { {
    { 0x08C7, 0, 0xE3BD, 0 },
    { 0x04DF, 0, 0xE679, 0x0100 },
    { 0x4A38, 0, 0x10CC, 0xFCE0 },
    { 0x490C, 0, 0x14B4, 0xFB50 },
} };

/// Message handler for the walking animation. `field_0` is the opcode:
/// 0x109 drives the aim state machine, 0x104 picks one of the four poses in
/// `Actor00100_D00004` with the global LCG, 0x1602 plays the step sound with
/// the pose latched out of `Actor00100_D0BDB4`, and 0x202 writes the fixed
/// crouch pose. Every opcode except 0x104/0x1602/0x202 returns 0.
///
/// Each LCG arm keeps its own `value` local: they are separate variables
/// because the arms are separate blocks and one local shared between them
/// changes which register the allocator picks in every arm.
s32 Actor00100_Fn00E58(Task* arg0, s32 arg1, ActorCommand* request, s32 arg3)
{
    Actor00100PoseTable table;
    Actor00100PoseRow*  row;
    DesertChaserWork*   work;
    Enemy*              ctx;
    s8                  rnd;
    s32                 view;
    u32                 value2;
    u32                 value4;
    u32                 value5;
    u32                 value38;
    u32                 valueDefault;
    s32                 kind;
    s32                 cmd;
    s32                 sub;
    s32                 req;
    s32                 sound;
    s32                 pan;

    work = arg0->work;
    ctx  = arg0->spawnArg2.pointer;

    if (request->context.key == 0x109) {
        kind = request->command;
        switch (kind) {
            case 1:
                work->poseYawPrev = 0x5A;
                break;
            case 2:
                if (work->field_0 == 0x26) {
                    work->field_0 = 0x26;
                }
                break;
            case 3:
                work->poseYawPrev = work->poseYaw;
                break;
            case 4:
                if (work->field_0 == 0x21) {
                    work->field_0 = 0x22;
                }
                if (work->field_0 == 0x18) {
                    work->field_0 = 0x26;
                }
                break;
        }
        return 1;
    } else {
        work->actorId.fields.stage   = request->context.loc.stage;
        work->actorId.fields.area    = request->context.loc.area;
        work->actorId.fields.command = (u8)request->command;
        if (request->context.key == 0x104) {
            table = Actor00100_D00004;
            cmd   = request->command;
            switch (cmd) {
                case 0:
                    work->field_0 = 0;
                    break;
                case 1:
                    view = Gp_GetViewIndex() & 0xFF;
                    switch (view) {
                        case 2:
                            value2          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                            gRandomLcgState = value2;
                            rnd             = ((value2 >> 0x10) % 3) + 1;
                            break;
                        case 4:
                            value4          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                            gRandomLcgState = value4;
                            rnd             = 1;
                            if (((value4 >> 0x10) & 1) == 0) {
                                rnd = 3;
                            }
                            break;
                        case 5:
                            value5          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                            rnd             = (value5 >> 0x10) & 1;
                            gRandomLcgState = value5;
                            break;
                        case 3:
                        case 8:
                            value38         = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                            rnd             = ((value38 >> 0x10) & 1) | 2;
                            gRandomLcgState = value38;
                            break;
                        default:
                            valueDefault    = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                            rnd             = (valueDefault >> 0x10) & 3;
                            gRandomLcgState = valueDefault;
                    }
                    row                                 = &table.rows[rnd];
                    arg0->extra.tmd->coords->coord.t[0] = row->vx;
                    arg0->extra.tmd->coords->coord.t[1] = (s16)row->vy;
                    arg0->extra.tmd->coords->coord.t[2] = (s16)row->vz;
                    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, (s16)row->yaw, 1);
                    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                    work->field_0                         = 5;
                    break;
            }
        }
        if (request->context.key == 0x1602) {
            sub = request->command;
            switch (sub) {
                case 0:
                    work->field_0 = 0;
                    break;
                case 2:
                    sound = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x52160009;
                    pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                    SndEvt_EnqueueType6(sound, pan,
                                        (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                    work->field_82E = sub;
                    work->field_828 = sub;
                    desertChaserAnimTick(arg0);
                    desertChaserAnimTick(arg0);
                    work->field_0 = 0x1C;
                    work->poseVy  = Actor00100_D0BDB4.rows[3].vy;
                    work->poseVx  = Actor00100_D0BDB4.rows[3].vx;
                    work->poseVz  = Actor00100_D0BDB4.rows[3].vz;
                    work->poseYaw = Actor00100_D0BDB4.rows[3].yaw;
                    break;
            }
        }
        if (request->context.key == 0x202) {
            req = request->command;
            switch (req) {
                case 0:
                    work->field_0 = 0;
                block_46:
                    return 0;
                case 2:
                    work->field_0                       = 0x26;
                    arg0->extra.tmd->coords->coord.t[0] = -0x896;
                    arg0->extra.tmd->coords->coord.t[1] = 0;
                    arg0->extra.tmd->coords->coord.t[2] = 0x5AF;
                    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, -0x3F4, 1);
                    goto block_46;
                default:
                    return 0;
            }
        } else {
            return 0;
        }
    }
}

/// Collects bearings from the obstacles in `recs` into a 16-slot scratch and
/// steps `coord` along each survivor. Same walk as `ActorContact_Steer`, but
/// `blocked` is raised only for a kind 0x10000 record whose `key` bit 0x80
/// is clear. The scratch is carved before the early-out, so that path leaks it.
static s32 desertChaserAvoidWalk(GfxCoord* coord, WorldCollisionContact* recs, s16 count, SVECTOR* pos)
{
    u8*                       head;
    Actor00100AvoidScratch16* s;
    s16                       diff;

    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - sizeof(Actor00100AvoidScratch16);
    s                        = SCRATCH_STACK_CURSOR(Actor00100AvoidScratch16);

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1 || gGameSession->viewReady == 1) {
        return 0;
    }

    s->blocked = 0;
    pos->vz    = 0;
    pos->vy    = 0;
    pos->vx    = 0;

    Gfx_MatrixCol1(&coord->workm, (SVECTOR*)(head - 0x50));
    VectorNormalSS((SVECTOR*)(head - 0x50), (SVECTOR*)(head - 0x50));

    if (ABS(s->dir.vz) < 0x818) {
        s->face = ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
    } else {
        s->face = -ratan2(-coord->workm.m[0][2], coord->workm.m[1][2]);
    }

    s->eye.vx = (u16)coord->workm.t[0];
    s->eye.vy = (u16)coord->workm.t[1];
    s->eye.vz = (u16)coord->workm.t[2];
    s->count  = 0;

    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].key.value == 0) {
            break;
        }
        s->kind  = recs[s->i].key.value & 0xFFFF0000;
        s->flags = recs[s->i].key.value & 0x80;
        switch (s->kind) {
            case 0x10000:
                if (s->flags == 0) {
                    s->blocked = 1;
                }
            case 0x30000:
                break;
            default:
                continue;
        }

        if (ABS(s->dir.vz) < 0x818) {
            s->angle[s->count] = overlayBearingXZ((SVECTOR3*)&recs[s->i].point, &s->eye);
        } else {
            s->angle[s->count] = overlayBearingXY((SVECTOR3*)&recs[s->i].point, &s->eye);
        }
        s->ok[s->count] = 1;
        s->count++;
        if (s->count >= 16) {
            break;
        }
    }

    for (s->i = 0; s->i < s->count; s->i++) {
        for (s->j = s->i + 1; s->j < s->count; s->j++) {
            s->diff = actorWrapAngle((u16)s->angle[s->i] - (u16)s->angle[s->j]);
            if (abs(s->diff) > 0x400) {
                s->ok[s->i] = 0;
                s->ok[s->j] = 0;
            }
        }
        if (s->ok[s->i] != 0) {
            diff = ((u16)s->angle[s->i] - (u16)s->face) +
                   ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
            s->diff = diff;
            gfxRotMatrixY(&s->m, diff, 1);
            gfxReadMatrixZAxis(&s->m, &s->dir);
            VectorNormalSS(&s->dir, &s->dir);
            gte_lddp(-10);
            gte_ldsv(&s->dir);
            gte_gpf12();
            gte_stsv(&s->dir);
            pos->vx           += s->dir.vx;
            pos->vz           += s->dir.vz;
            coord->coord.t[0] += s->dir.vx;
            coord->coord.t[2] += s->dir.vz;
        }
    }

    SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor00100AvoidScratch16));
    return s->blocked != 0;
}

/// Draw the beam between parts `firstJoint` and `secondJoint` of the actor's
/// model: project both ends into view space, widen them into a `width`-half
/// quad, and emit it as a `POLY_FT4` tinted `shade` at height `height`.
static void Actor00100_Fn01900(Task* actor, s16 firstJoint, s16 secondJoint, s16 width, s16 height, u8 shade)
{
    ActorBeamScratch* s;
    s16               angle;
    GfxCoord*         secondCoord;
    GfxCoord*         firstCoord;
    s32               offset0;
    s32               offset1;
    s32               offset2;
    s32               offset3;
    s32               halfX;
    s32               halfZ;
    GfxCoord*         coords;
    GfxCoord*         view;
    POLY_FT4*         poly;

    coords      = actor->extra.tmd->coords;
    firstCoord  = coords + firstJoint;
    secondCoord = coords + secondJoint;
    if (firstJoint != secondJoint) {
        s = (ActorBeamScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(ActorBeamScratch));
        Gp_UpdateCoord(firstCoord);
        Gp_UpdateCoord(secondCoord);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &firstCoord->workm, &s->firstMatrix);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &secondCoord->workm, &s->secondMatrix);
        s->first.vy   = height;
        s->second.vy  = height;
        s->first.vx   = s->firstMatrix.t[0];
        s->first.vz   = s->firstMatrix.t[2];
        s->second.vx  = s->secondMatrix.t[0];
        s->second.vz  = s->secondMatrix.t[2];
        angle         = ratan2(s->second.vx - s->first.vx, s->second.vz - s->first.vz);
        halfX         = (s->first.vx - s->second.vx) / 2;
        halfZ         = (s->first.vz - s->second.vz) / 2;
        offset0       = rcos(angle) * width;
        s->corner0.vy = height;
        s->corner0.vx = halfX + (s->first.vx - (offset0 >> 0xC));
        s->corner0.vz = halfZ + (s->first.vz + ((s32)(rsin(angle) * width) >> 0xC));
        offset1       = rcos(angle) * width;
        s->corner1.vy = height;
        s->corner1.vx = halfX + (s->first.vx + (offset1 >> 0xC));
        s->corner1.vz = halfZ + (s->first.vz - ((s32)(rsin(angle) * width) >> 0xC));
        offset2       = rcos(angle) * width;
        s->corner2.vy = height;
        s->corner2.vx = (s->second.vx - (offset2 >> 0xC)) - halfX;
        s->corner2.vz = (s->second.vz + ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
        offset3       = rcos(angle) * width;
        s->corner3.vy = height;
        s->corner3.vx = (s->second.vx + (offset3 >> 0xC)) - halfX;
        s->corner3.vz = (s->second.vz - ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
        /* `gGfxViewCoord`, reached back from its `workm`: the address is built
           from `gGfxViewCoord.workm`, whose high half the GTE loads below share. */
        view               = &gGfxViewCoord;
        view->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(view);
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        s->depth = RotTransPers4(&s->corner0, &s->corner1, &s->corner2, &s->corner3, &s->screen0, &s->screen1,
                                 &s->screen2, &s->screen3, &s->perspective, &s->flags);
        if (s->flags >= 0) {
            poly           = gGpuPrimCursor;
            gGpuPrimCursor = poly + 1;
            setlen(poly, 9);
            poly->code                     = 0x2E;
            GPU_PRIMITIVE_XY_WORD(poly, 0) = s->screen0;
            GPU_PRIMITIVE_XY_WORD(poly, 1) = s->screen1;
            GPU_PRIMITIVE_XY_WORD(poly, 2) = s->screen2;
            GPU_PRIMITIVE_XY_WORD(poly, 3) = s->screen3;
            setUV4(poly, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);
            poly->tpage = 0x48;
            poly->clut  = 0x4283;
            setRGB0(poly, shade, shade, shade);
            addPrim((&gGpuCurrentOt[((((u32)(s->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), poly);
        }
        SCRATCH_STACK_RELEASE_BYTES(sizeof(ActorBeamScratch));
    }
}

void desertChaserBlendTick(Task* arg0)
{
    AnimationPose     pose;
    AnimationPose     otherPose;
    AnimationContext* anim;
    s16               part;
    s16               index;
    s32               blend;
    s32               invBlend;
    s32               offset;
    u8*               work;
    u8*               slotBase;

    work = (u8*)((DesertChaserWork*)arg0->work);
    anim = &((DesertChaserWork*)work)->anim;
    for (index = 1; index < 0x12; index++) {
        part = index - 1;
        switch (part) {
            case 0:
                blend = 0xC00;
                break;
            case 1:
                blend = 0xC00;
                break;
            case 2:
                blend = 0xC00;
                break;
            case 3:
                blend = 0x5DE;
                break;
            case 4:
                blend = 0x5DE;
                break;
            default:
                blend = 0xBD0;
                break;
        }
        invBlend = 0x1000 - blend;
        if (index < 0xB) {
            slotBase                = work + (index * 0x28);
            *(slotBase + 0x43D)     = (u8)((DesertChaserWork*)work)->field_83A;
            *(s8*)(slotBase + 0x39) = (s8)(((DesertChaserWork*)work)->field_832 - 3);
            animationTickSlotPose(anim, (s32)index, &pose, 0);
            animationTickSlotPose(&((DesertChaserWork*)work)->blendAnim, (s32)index, &otherPose, 0);
            Gp_AnimWritePoseCopy(anim, (s32)index, &pose, &otherPose, blend, invBlend);
        } else {
            offset                       = index * 0x28;
            *(s8*)(work + offset + 0x39) = (s8)(((DesertChaserWork*)work)->field_832 - 3);
            animationTickSlot(&((DesertChaserWork*)work)->anim, (s32)index);
        }
    }
}

s32 desertChaserAnimCues(Task* arg0, DesertChaserWork* arg1)
{
    DesertChaserWork* work;
    u32               prev;
    s32               var_a0 = 1;

    switch (arg1->field_82E) {
        case 0:
            if ((arg1->slots[1].currentPose.indices.recordIndex & 0x3FF) == 9) {
                prev = arg1->field_848[1];
                if (prev != 9) {
                    arg1->field_848[1] = 9;
                    work               = arg0->work;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[17], 0x80002280, &work->field_898);
                    }
                    work               = arg0->work;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[9], 0x80002120, &work->field_898);
                    }
                    return 0x40010002;
                }
                arg1->field_848[1] = prev;
                var_a0             = 0;
            }
            if ((arg1->slots[1].currentPose.indices.recordIndex & 0x3FF) == 6) {
                prev = arg1->field_848[1];
                if (prev != 6) {
                    arg1->field_848[1] = 6;
                    work               = arg0->work;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[14], 0x80002220, &work->field_898);
                    }
                    work               = arg0->work;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[7], 0x80002120, &work->field_898);
                    }
                    return 0x40010001;
                }
                arg1->field_848[1] = prev;
                var_a0             = 0;
            }
            break;
        case 0xA:
            if ((arg1->slots[1].currentPose.indices.recordIndex & 0x3FF) == 0xA) {
                prev = arg1->field_848[1];
                if (prev != 0xA) {
                    arg1->field_848[1] = 0xA;
                    work               = arg0->work;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[0], 0x80004A00, &work->field_898);
                    }
                    return 0x40010005;
                }
                arg1->field_848[1] = prev;
                var_a0             = 0;
            }
            break;
        case 3:
            if ((arg1->slots[1].currentPose.indices.recordIndex & 0x3FF) == 0xC) {
                prev = arg1->field_848[1];
                if (prev != 0xC) {
                    arg1->field_848[1] = 0xC;
                    return 0x40010004;
                }
                arg1->field_848[1] = prev;
                var_a0             = 0;
            }
            if ((arg1->slots[1].currentPose.indices.recordIndex & 0x3FF) == 8) {
                prev = arg1->field_848[1];
                if (prev != 8) {
                    arg1->field_848[1] = 8;
                    return 0x40010003;
                }
                arg1->field_848[1] = prev;
                var_a0             = 0;
            }
            break;
        case 6:
            if ((arg1->slots[1].currentPose.indices.recordIndex & 0x3FF) == 6) {
                prev = arg1->field_848[1];
                if (prev != 6) {
                    arg1->field_848[1] = 6;
                    work               = arg0->work;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[9], 0x80003200, &work->field_898);
                    }
                    work               = arg0->work;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[7], 0x80003200, &work->field_898);
                    }
                    return 0x40010004;
                }
                arg1->field_848[1] = prev;
                var_a0             = 0;
            }
            if ((arg1->slots[1].currentPose.indices.recordIndex & 0x3FF) == 0xC) {
                prev = arg1->field_848[1];
                if (prev != 0xC) {
                    arg1->field_848[1] = 0xC;
                    work               = arg0->work;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[17], 0x80004480, &work->field_898);
                    }
                    work               = arg0->work;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[14], 0x80004480, &work->field_898);
                    }
                    return 0x40010011;
                }
                arg1->field_848[1] = prev;
                var_a0             = 0;
            }
            break;
        case 0x12:
            if ((arg1->slots[1].currentPose.indices.recordIndex & 0x3FF) == 6) {
                prev = arg1->field_848[1];
                if (prev != 6) {
                    arg1->field_848[1] = 6;
                    work               = arg0->work;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[9], 0x80003200, &work->field_898);
                    }
                    work               = arg0->work;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[7], 0x80003200, &work->field_898);
                    }
                    return 0x40010001;
                }
                arg1->field_848[1] = prev;
                var_a0             = 0;
            }
            if ((arg1->slots[1].currentPose.indices.recordIndex & 0x3FF) == 9) {
                prev = arg1->field_848[1];
                if (prev != 9) {
                    arg1->field_848[1] = 9;
                    work               = arg0->work;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[9], 0x80003200, &work->field_898);
                    }
                    work               = arg0->work;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[17], 0x80003200, &work->field_898);
                    }
                    return 0x40010001;
                }
                arg1->field_848[1] = prev;
                var_a0             = 0;
            }
            if ((arg1->slots[1].currentPose.indices.recordIndex & 0x3FF) == 0xE) {
                prev = arg1->field_848[1];
                if (prev != 0xE) {
                    arg1->field_848[1] = 0xE;
                    work               = arg0->work;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[14], 0x80003200, &work->field_898);
                    }
                    work               = arg0->work;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[17], 0x80003200, &work->field_898);
                    }
                    return 0x40010002;
                }
                arg1->field_848[1] = prev;
                var_a0             = 0;
            }
            break;
        case 0x11:
            if ((arg1->slots[1].currentPose.indices.recordIndex & 0x3FF) == 6) {
                prev = arg1->field_848[1];
                if (prev != 6) {
                    arg1->field_848[1] = 6;
                    work               = arg0->work;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[9], 0x80003200, &work->field_898);
                    }
                    work               = arg0->work;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[7], 0x80003200, &work->field_898);
                    }
                    return 0x40010001;
                }
                arg1->field_848[1] = prev;
                var_a0             = 0;
            }
            if ((arg1->slots[1].currentPose.indices.recordIndex & 0x3FF) == 0xA) {
                prev = arg1->field_848[1];
                if (prev != 0xA) {
                    arg1->field_848[1] = 0xA;
                    work               = arg0->work;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[7], 0x80003200, &work->field_898);
                    }
                    work               = arg0->work;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[14], 0x80003200, &work->field_898);
                    }
                    return 0x40010001;
                }
                arg1->field_848[1] = prev;
                var_a0             = 0;
            }
            if ((arg1->slots[1].currentPose.indices.recordIndex & 0x3FF) == 0xE) {
                prev = arg1->field_848[1];
                if (prev != 0xE) {
                    arg1->field_848[1] = 0xE;
                    work               = arg0->work;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[14], 0x80003200, &work->field_898);
                    }
                    work               = arg0->work;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[17], 0x80003200, &work->field_898);
                    }
                    return 0x40010002;
                }
                arg1->field_848[1] = prev;
                var_a0             = 0;
            }
            break;
        case 0xD:
            if ((arg1->slots[1].currentPose.indices.recordIndex & 0x3FF) == 0x18) {
                prev = arg1->field_848[1];
                if (prev != 0x18) {
                    arg1->field_848[1] = 0x18;
                    return 0x4001000F;
                }
                arg1->field_848[1] = prev;
                var_a0             = 0;
            }
            break;
    }
    if (var_a0 == 1) {
        memFillBytes(arg1->field_848, 0U, sizeof(arg1->field_848));
    }
    return 0;
}

#include "../../shared/desert_chaser_anim_tick.inc.c"

/// Builds the damage state: allocates the 0xC30 work block, wires the two
/// animation contexts and the four collision objects onto the model, latches
/// the spawn position and the one a fixed step ahead of it, then picks the
/// start state and pose row out of the two nibbles of `spawnArg1`.
static void Actor00100_Fn02C54(Enemy* arg0, Task* arg1)
{
    SVECTOR                 vec;
    VECTOR                  color;
    u8                      cmd30[8];
    u8                      cmd38[8];
    DesertChaserWork*       work;
    TmdObject*              tmd;
    GfxCoord*               coord;
    DesertChaserWork*       mapped;
    TmdObject*              model;
    DesertChaserSphereBody* primary;
    DesertChaserSphereBody* secondary;
    SVECTOR*                dir;
    s32                     kind;
    s32                     sessionMode;

    coord      = arg1->extra.tmd->coords;
    tmd        = arg1->extra.tmd;
    work       = memCalloc(0xC30U, false);
    arg1->work = work;
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    (Gp_IncStateF0Ref)(0);
    arg1->exitCallback = desertChaserExit;
    mapped             = (DesertChaserWork*)arg1->work;
    model              = arg1->extra.tmd;
    model->lightMtx    = &mapped->light;
    model->colorMtx    = &mapped->color;
    arg0->field_4      = &arg1->extra.tmd->coords[0].coord;
    arg0->field_48     = 0;
    arg0->bodyPos.vx   = 0;
    arg0->bodyPos.vy   = 0;
    arg0->bodyPos.vz   = 0;
    arg0->coord        = &arg1->extra.tmd->coords[2];
    Gp_LinkNode(&arg0->node);
    arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    arg0->reactionFlags          = 0;
    arg0->hp                     = Actor00100_D0BDA4.hpMax;
    arg0->param                  = &Actor00100_D0BDA4;
    arg0->recs                   = work->objs[0].contacts;
    animationInitContext(&work->anim, Actor00100_D1B944, tmd, (u8(*)[ANIMATION_POSE_BUFFER_BYTES])work->poses, work->slots);
    animationInitContext(&work->blendAnim, Actor00100_D1B944, tmd, (u8(*)[ANIMATION_POSE_BUFFER_BYTES])work->blendPoses, work->blendSlots);
    work->field_828 = 2;
    work->field_82A = 0;
    work->field_82E = 0;
    work->field_844 = 0;
    work->field_840 = 0;
    if ((arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) & 1) {
        work->field_834 = 0xF;
        work->field_832 = 0xF;
    } else {
        work->field_834 = 0x11;
        work->field_832 = 0x11;
    }
    work->field_83A = 0x10;
    desertChaserAnimTick(arg1);
    work->objs[2].obj.coord            = coord;
    work->objs[2].obj.context.contacts = work->objs[2].contacts;
    work->objs[2].obj.pos.vx           = 0;
    work->objs[2].obj.pos.vy           = -0x11C;
    work->objs[2].obj.pos.vz           = 0;
    work->objs[2].obj.key              = 0x30001;
    work->objs[2].obj.radius           = 0x12C;
    work->objs[2].obj.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->objs[2].obj);
    work->capsuleBody.shape.ends[0].vx     = 0;
    work->capsuleBody.shape.ends[0].vy     = -0x180;
    work->capsuleBody.shape.ends[0].vz     = 0;
    work->capsuleBody.shape.ends[1].vx     = 0;
    work->capsuleBody.shape.ends[1].vy     = -0x180;
    work->capsuleBody.shape.ends[1].vz     = 0x2BC;
    work->capsuleBody.shape.end0Radius     = 0x12C;
    work->capsuleBody.shape.end1Radius     = 0x12C;
    work->capsuleBody.shape.contacts       = work->capsuleBody.contacts;
    work->capsuleBody.body.coord           = coord;
    work->capsuleBody.body.context.capsule = &work->capsuleBody.shape;
    work->capsuleBody.body.pos.vx          = 0;
    work->capsuleBody.body.pos.vy          = 0;
    work->capsuleBody.body.pos.vz          = 0;
    work->capsuleBody.body.key             = 0x30001;
    work->capsuleBody.body.radius          = 1;
    work->capsuleBody.body.flags           = WORLD_COLLISION_BODY_CAPSULE;
    work->objs[2].obj.flags               |= WORLD_COLLISION_BODY_GRID_ENABLED;
    Gp_LinkObj(2, &work->capsuleBody.body);
    work->capsuleBody.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    Gp_InitRec18Table(work->capsuleBody.contacts, ARRAY_SIZE(work->capsuleBody.contacts), 0);
    Gp_InitRec18Table(work->objs[2].obj.context.contacts, 5, 0);
    primary                       = &work->objs[0];
    primary->obj.coord            = &arg1->extra.tmd->coords[2];
    primary->obj.context.contacts = primary->contacts;
    primary->obj.pos.vx           = 0;
    primary->obj.pos.vy           = 0;
    primary->obj.pos.vz           = 0;
    primary->obj.key              = 0x30001;
    primary->obj.radius           = 0x19C;
    primary->obj.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &primary->obj);
    primary->obj.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_InitRec18Table(primary->obj.context.contacts, ARRAY_SIZE(primary->contacts), 0);
    secondary                       = &work->objs[1];
    secondary->obj.coord            = &arg1->extra.tmd->coords[10];
    secondary->obj.context.contacts = secondary->contacts;
    secondary->obj.pos.vx           = 0;
    secondary->obj.pos.vy           = 0;
    secondary->obj.pos.vz           = 0;
    secondary->obj.key              = 0x30001;
    secondary->obj.radius           = 0x100;
    secondary->obj.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &secondary->obj);
    secondary->obj.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_InitRec18Table(secondary->obj.context.contacts, ARRAY_SIZE(secondary->contacts), 0);
    work->objs[1].obj.pos.vx = 0;
    work->objs[1].obj.pos.vy = 0;
    work->objs[1].obj.pos.vz = -0x100;
    work->field_14           = 0;
    work->field_C[0].x       = arg1->extra.tmd->coords[0].coord.t[0];
    work->field_C[0].z       = arg1->extra.tmd->coords[0].coord.t[2];
    gfxReadMatrixZAxis(&arg1->extra.tmd->coords[0].coord, &vec);
    vec.vy = 0;
    dir    = &vec;
    VectorNormalSS(dir, dir);
    gte_lddp(5000);
    gte_ldsv(dir);
    gte_gpf12();
    gte_stsv(dir);
    work->field_C[1].x  = arg1->extra.tmd->coords[0].coord.t[0] + vec.vx;
    work->field_C[1].z  = arg1->extra.tmd->coords[0].coord.t[2] + vec.vz;
    work->animCommand   = NULL;
    work->params[0]     = 1;
    work->params[1]     = 0;
    work->params[2]     = 3;
    work->params[3]     = 1;
    arg1->msgTable      = Actor00100_D1BA54;
    coord->parent       = &gGfxViewCoord;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    color.vx = coord->workm.t[0];
    color.vy = coord->workm.t[1];
    color.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0, &color, 0, 0);
    work->field_890.coord      = &arg1->extra.tmd->coords[1];
    work->field_890.spawnArgLo = 0x100;
    work->field_890.spawnArgHi = 2;
    kind                       = (arg1->spawnArg1.value >> 16) & 0xF;
    if (kind == 1) {
        goto state1;
    }
    if (kind < 2) {
        goto stateStill;
    }
    if (kind == 2) {
        goto state2;
    }
    if (kind == 3) {
        goto state3;
    }
    work->field_2 = -1;
    work->field_0 = 0x18;
    Tmd_AllocBuffers(tmd);
    goto stateEnd;
state1:
    work->field_2 = -1;
    work->field_0 = 0;
    goto stateEnd;
state2:
    work->field_2 = -1;
    work->field_0 = 0x21;
    goto stateEnd;
state3:
    work->field_2 = -1;
    work->field_0 = 5;
    goto stateEnd;
stateStill:
    work->field_2 = -1;
    work->field_0 = 0x18;
    Tmd_AllocBuffers(tmd);
stateEnd:
    kind = arg1->spawnArg1.value & 0xF;
    if (kind == 1) {
        goto pose2;
    }
    if (kind < 2) {
        goto pose1;
    }
    if (kind != 2) {
        goto pose1;
    }
    work->poseVy  = Actor00100_D0BDB4.rows[0].vy;
    work->poseVx  = Actor00100_D0BDB4.rows[0].vx;
    work->poseVz  = Actor00100_D0BDB4.rows[0].vz;
    work->poseYaw = Actor00100_D0BDB4.rows[0].yaw;
    goto poseEnd;
pose2:
    work->poseVy  = Actor00100_D0BDB4.rows[2].vy;
    work->poseVx  = Actor00100_D0BDB4.rows[2].vx;
    work->poseVz  = Actor00100_D0BDB4.rows[2].vz;
    work->poseYaw = Actor00100_D0BDB4.rows[2].yaw;
    goto poseEnd;
pose1:
    work->poseVy  = Actor00100_D0BDB4.rows[1].vy;
    work->poseVx  = Actor00100_D0BDB4.rows[1].vx;
    work->poseVz  = Actor00100_D0BDB4.rows[1].vz;
    work->poseYaw = Actor00100_D0BDB4.rows[1].yaw;
poseEnd:
    sessionMode = gGameSession->location.loc.stage;
    if ((sessionMode - 2) < 2U) {
        if (gGameSession->location.loc.area == 0x18) {
            cmd38[3] = sessionMode;
            cmd38[2] = gGameSession->location.loc.area;
            cmd38[0] = 0x31;
            cmd30[0] = (u8)gGameSession->spriteVariant;
            cmd30[3] = 0;
            cmd30[2] = 0;
            cmd30[1] = 0;
            CdCmd_Enqueue(CD_COMMAND_LOAD_FILE, cmd38, cmd30);
        }
    }
    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 1, 0, 0)) {
        func_mine_mesa_801811C4(0x7D0);
    }
    Gp_ClearRec18Occupied(work->objs[2].contacts);
    Gp_ClearRec18Occupied(work->objs[0].contacts);
    Gp_ClearRec18Occupied(work->objs[1].contacts);
    Gp_ClearRec18Occupied(work->capsuleBody.contacts);
    work->field_C2A = 0;
    arg1->state    += 1;
}

#include "../../shared/desert_chaser_hit_effect.inc.c"

static void Actor00100_Fn0375C(Task* arg0)
{
    PlayerStatus*            config = &gPlayerStatus;
    Enemy*                   ctx;
    GfxCoord*                coord;
    s16                      effect;
    s16                      delta;
    s16                      z;
    s16                      state3;
    s32                      state4;
    s16                      damageState;
    s16                      deathState;
    s16                      hurtState;
    s16                      poisonState;
    s16                      state0;
    s16                      state1;
    s32                      magnitude;
    s16                      nextState;
    s16                      wrapped;
    s32                      yaw;
    s32                      deathSound;
    s32                      hurtSound;
    s32                      hitSound;
    s32                      distance;
    s32                      dx;
    s32                      dy;
    s32                      dz;
    s32                      soundBase;
    s32                      deathPan;
    s32                      hurtPan;
    s32                      hitPan;
    u16                      totalDamage;
    u32                      tickDamage;
    u32                      doubleDamage;
    u32                      kind;
    u8                       sessionMode;
    void*                    hitPos;
    void*                    temp_a2;
    DesertChaserWork*        work;
    Actor00100DamageScratch* scratch;
    Actor00100DamageScratch* head;
    void*                    temp_v1_2;
    void*                    temp_v1_3;

    ctx  = arg0->spawnArg2.pointer;
    work = (DesertChaserWork*)((DesertChaserWork*)arg0->work);
    if (ctx->hp > 0) {
        head              = SCRATCH_STACK_CURSOR(Actor00100DamageScratch);
        scratch           = (SCRATCH_STACK_CURSOR(Actor00100DamageScratch) = head - 1);
        scratch->field_20 = Actor00100_FindDamageHit(work->objs[0].contacts, (SVECTOR*)&scratch->field_18);
        if (scratch->field_20 == 0) {
            hitPos            = &scratch->field_18;
            scratch->field_20 = Actor00100_FindDamageHit(work->objs[1].contacts, (SVECTOR*)hitPos);
        }
        if (scratch->field_20 != 0) {
            work->hitFlag                         = 1;
            scratch->field_2E                     = -1;
            work->hitCooldown                     = Gp_GetIdParam2(scratch->field_20);
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(arg0->extra.tmd->coords);
            scratch->field_10 = (s16)(scratch->field_18 - arg0->extra.tmd->coords->workm.t[0]);
            scratch->field_12 = (s16)(scratch->field_1A - arg0->extra.tmd->coords->workm.t[1]);
            z                 = scratch->field_1C - arg0->extra.tmd->coords->workm.t[2];
            scratch->field_14 = z;
            yaw               = ratan2((s32)scratch->field_10, (s32)z);
            coord             = arg0->extra.tmd->coords;
            delta             = yaw - ratan2((s32)-coord->workm.m[2][0], (s32)coord->workm.m[2][2]);
            wrapped           = delta;
            scratch->field_2C = delta;
            if (delta < 0) {
                while (1) {
                    if (wrapped >= -0x800)
                        break;
                    wrapped += 0x1000;
                }
            } else {
                while (1) {
                    if (wrapped <= 0x800)
                        break;
                    wrapped -= 0x1000;
                }
            }
            scratch->field_2C = wrapped;
            kind              = Gp_GetIdParam0(scratch->field_20) & 0xFFFF;
            switch (kind) {
                case 0:
                case 5:
                case 6:
                case 7:
                    state0 = work->field_0;
                    if ((state0 == 0x18) || (state0 == 0x26) || (state0 == 0x20)) {
                        work->field_0 = 0x1C;
                    }
                    if (work->field_0 == 0x21) {
                        work->field_0 = 0x22;
                    }
                    if (work->field_82A == 0) {
                        work->damageTotal = 0U;
                    }
                    state1 = work->field_0;
                    if (state1 == 4 || state1 == 7 || state1 == 11 || state1 == 17) {
                        work->field_0 = 11;
                        work->field_2 = -1;
                    } else if (state1 != 0x22 && state1 != 0x15 && state1 != 0x14 && state1 != 7 && state1 != 0x24) {
                        work->field_82A = 1;
                        work->field_838 = 15;
                        work->field_836 = 2;
                    }
                    break;
                case 8:
                    sessionMode = gGameSession->location.loc.stage;
                    if ((sessionMode != 2) && (sessionMode != 5)) {
                        magnitude = scratch->field_2C;
                        if (magnitude < 0) {
                            magnitude = -magnitude;
                        }
                        if (magnitude < 0x501) {
                            Actor00100_SetHitState(work);
                        }
                    }
                    break;
                case 9:
                    Actor00100_SetHitState(work);
                    break;
                case 2:
                    Actor00100_SetHitState(work);
                    Gp_SetObjFlag2(ctx, scratch->field_20, 0);
                    break;
                case 3:
                    state3 = work->field_0;
                    if ((state3 == 0x18) || (state3 == 0x26) || (state3 == 0x20)) {
                        work->field_0 = 0x1C;
                    }
                    if (work->field_0 == 0x21) {
                        work->field_0 = 0x22;
                    }
                    Gp_SetObjFlag4(ctx, scratch->field_20, 0);
                    break;
                case 1:
                case 4:
                    state4 = work->field_0;
                    if (state4 == 4 || state4 == 7 || state4 == 0x21 || state4 == 0x14 || state4 == 0xB || state4 == 0x11 || (state4 == 0x24 && work->field_6 < 10)) {
                        nextState     = 7;
                        work->field_0 = nextState;
                    } else if (state4 != 0x15 && state4 != 0) {
                        nextState     = 0x14;
                        work->field_0 = nextState;
                    }
                    break;
            }
            dx                = config->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0];
            scratch->field_0  = dx;
            dy                = config->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1];
            scratch->field_4  = dy;
            dz                = config->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2];
            scratch->field_8  = dz;
            distance          = SquareRoot0((dx * dx) + (dy * dy) + (dz * dz));
            scratch->field_28 = distance;
            scratch->field_24 = Gp_ComputeDamage((u32)scratch->field_20, (u32)distance, 0, 0);
            desertChaserHitEffect(arg0, scratch->field_2C, scratch->field_20);
            work->field_844 = 0;
            work->field_840 = 0;
            if (Gp_RollEnemyChance(ctx, (u32)scratch->field_20, 0) != 0) {
                scratch->field_2E = 0;
                scratch->field_24 = (u32)(scratch->field_24 * 4);
            }
            damageState = work->field_0;
            if (damageState == 4 || (damageState == 0x14 && work->field_6 >= 11) || damageState == 7 || damageState == 0x11 || damageState == 0xB || (damageState == 0x24 && work->field_6 < 10)) {
                doubleDamage      = scratch->field_24 * 2;
                scratch->field_24 = doubleDamage;
                if (doubleDamage != 0) {
                    scratch->field_2E = 3;
                }
            }
            func_800E2C78(ctx, scratch->field_20, (s32)scratch->field_24, 0);
            effect = scratch->field_2E;
            if (effect != -1) {
                Gp_SpawnEff(EFFECT_CRITICAL_HIT, arg0->extra.tmd->coords + 2, (s32)(effect), NULL);
            }
            ctx->hp = ctx->hp - scratch->field_24;
            func_800DA6E8(&ctx->node, (s32)scratch->field_24, 0);
            totalDamage       = work->damageTotal + (u16)scratch->field_24;
            work->damageTotal = totalDamage;
            if (ctx->hp <= 0) {
                work->broadcast.context.loc.stage = 9;
                work->broadcast.context.loc.area  = 1;
                work->broadcast.command           = 4;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &work->broadcast, ACTOR_COMMAND_MESSAGE_APPLY);
                if ((Gp_GetIdParam0(scratch->field_20) & 0xFFFF) == 4) {
                    work->field_0 = 3;
                } else {
                    deathState = work->field_0;
                    if ((deathState == 0x21) || (deathState == 0x11) || (deathState == 0xB) || (deathState == 7) || (deathState == 4)) {
                        soundBase     = 0x40010008;
                        work->field_0 = 7;
                        work->field_2 = -1;
                        goto playHitSound;
                    }
                    deathSound = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40010008;
                    deathPan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                    SndEvt_EnqueueType6(deathSound, (s32)deathPan, (s32)(s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                    work->field_0 = 0x14;
                }
            } else if ((s16)totalDamage >= 0x47) {
                hurtState = work->field_0;
                if ((hurtState != 4) && (hurtState != 0x14) && (hurtState != 7) && (hurtState != 0xB) && (hurtState != 0x11)) {
                    hurtSound = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40010008;
                    hurtPan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                    SndEvt_EnqueueType6(hurtSound, (s32)hurtPan, (s32)(s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                    work->field_0 = 0x14;
                } else {
                    goto normalHitSound;
                }
            } else {
            normalHitSound:
                soundBase = 0x40010007;
            playHitSound:
                hitSound = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | soundBase;
                hitPan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                SndEvt_EnqueueType6(hitSound, (s32)hitPan, (s32)(s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            }
        }
        if (ctx->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            scratch->field_24 = Gp_TickObjFlag4(ctx);
            if (Gp_ObjFlag4Expired(ctx) != 0) {
                ctx->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
            }
            ctx->hp    = ctx->hp - scratch->field_24;
            tickDamage = scratch->field_24;
            if (tickDamage != 0) {
                func_800DA6E8(&ctx->node, (s32)tickDamage, 0);
                if (ctx->hp <= 0) {
                    poisonState = work->field_0;
                    if ((poisonState != 4) && (poisonState != 7) && (poisonState != 0xB) && (poisonState != 0x11)) {
                        work->field_0 = 0xA;
                    } else {
                        work->field_0 = 0x15;
                    }
                } else {
                    if (work->field_0 == 0x1C) {
                        work->field_0 = 0x26;
                    }
                    if (work->field_0 != 4 && work->field_0 != 7 && work->field_0 != 11 && work->field_0 != 17) {
                        work->field_82A = 1;
                        work->field_838 = 15;
                        work->field_836 = 2;
                    } else if (work->field_0 != 4) {
                        work->field_0 = 11;
                    } else {
                        work->field_2 = -1;
                    }
                }
            }
        }
        if (ctx->hp <= 0) {
            work->field_C2A = 1;
        }
        SCRATCH_STACK_RELEASE_BLOCK(Actor00100DamageScratch);
    }
}

static void Actor00100_Fn04270(Task* arg0)
{
    DesertChaserWork* work;
    TmdObject*        obj;
    Enemy*            ctx;
    s32               v;

    work = arg0->work;
    obj  = arg0->extra.tmd;
    ctx  = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        obj->flags                  = 0;
        work->objs[2].obj.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        ctx->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->field_6               = 0;
    }
    if (work->field_6 == 0x3C) {
        Gp_UnlinkObj(&work->objs[0].obj);
        Gp_UnlinkObj(&work->objs[1].obj);
        Gp_UnlinkObj(&work->capsuleBody.body);
        Gp_UnlinkObj(&work->objs[2].obj);
        ctx->recs = 0;
    }
    if (work->field_6 >= 0x3D && work->reported == 0 && Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL && gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 1, 0, 0)) {
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT, 0);
        }
        arg0->state++;
        return;
    }
    switch (++work->field_6) {
        case 1:
            Gp_SetLightMode(ctx, ENEMY_COLOR_DEFAULT);
            Gp_SetLightMode(ctx, ENEMY_COLOR_WEIGHTED);
            /* fallthrough */
        case 0xA:
            arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            Gp_SetLightMode(ctx, ENEMY_COLOR_BLACK);
            break;
        case 0xF:
            work->field_8A8.vx = 0;
            work->field_8A8.vy = 0;
            work->field_8A8.vz = 0;
            work->field_8B0.vx = 0;
            work->field_8B0.vy = 0;
            work->field_8B0.vz = 0;
            actorLocalToView(&arg0->extra.tmd->coords[2], &work->field_8A8);
            Gp_SpawnEff(EFFECT_CORPSE_BURN, &gGfxViewCoord, 2, &work->field_8A8);
            work->field_8A8.vy = arg0->extra.tmd->coords[0].coord.t[1];
            actorLocalToView(&arg0->extra.tmd->coords[9], &work->field_8B0);
            work->field_8B0.vy = arg0->extra.tmd->coords[0].coord.t[1];
            Gp_SpawnEff(EFFECT_CORPSE_BURN, &gGfxViewCoord, 2, &work->field_8B0);
            break;
        case 0x3C:
            arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }

    if (work->field_6 > 0xA) {
        v = (work->field_6 - 0xA) * 107;
        if (v < 0x1000) {
            actorRescaleYawY(arg0->extra.tmd->coords, 0x1000, 0x1000 - v);
        } else {
            actorRescaleYawY(arg0->extra.tmd->coords, 0x1000, 0);
        }
    }
}

#include "../../shared/desert_chaser_approach.inc.c"

#include "../../shared/desert_chaser_pursue.inc.c"

static void Actor00100_Fn061FC(Task* arg0)
{
    s32               radius;
    SVECTOR           delta;
    DesertChaserWork* work;
    TmdObject*        obj;
    s32               outside;
    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->objs[0].obj.radius = 0x19C;
        work->field_828          = 1;
        work->field_82E          = 4;
        work->field_82A          = 0;
        work->hitFlag            = 0;
        work->objs[2].obj.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->field_832          = work->field_834;
        desertChaserAnimTick(arg0);
        return;
    }
    radius = 1000;
    desertChaserAnimTick(arg0);
    Actor00100_PositionDelta(arg0->extra.tmd->coords, &delta);
    if (work->slots[1].status.fields.flags & 0x100) {
        outside       = actorOutsideRadius(&delta, radius);
        work->field_0 = outside == 0 ? 0x1F : 0x26;
    }
}

#include "../../shared/desert_chaser_spawn_aim.inc.c"

#include "../../shared/desert_chaser_strike.inc.c"

static void Actor00100_Fn06C10(Task* arg0)
{
    DesertChaserWork* work;
    TmdObject*        obj;
    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->objs[0].obj.radius = 0x19C;
        work->field_828          = 1;
        work->field_82E          = 6;
        work->field_82A          = 0;
        work->objs[2].obj.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->field_832          = work->field_834;
        desertChaserAnimTick(arg0);
        work->field_6                      = 0;
        work->field_8                      = 0;
        work->capsuleBody.shape.ends[1].vz = -0x2D0;
    }
    work->field_6 += 1;
    desertChaserAnimTick(arg0);
    if (work->slots[1].status.fields.flags & 0x100) {
        work->field_0 = 0x26;
    }
    if (((u32)((work->slots[1].currentPose.indices.recordIndex & 0x3FF) - 6) < 8U) && (work->field_8 < 5)) {
        if (ActorContact_PushContact(arg0->extra.tmd->coords, work->objs[2].contacts, 5) != 0) {
            work->field_8 = (s16)((u16)work->field_8 + 1);
        }
        switch (work->slots[1].currentPose.indices.recordIndex & 0x3FF) {
            case 12:
                actorMoveForward(arg0->extra.tmd->coords, -60);
                break;
            case 13:
                actorMoveForward(arg0->extra.tmd->coords, -30);
                break;
            case 14:
                actorMoveForward(arg0->extra.tmd->coords, -15);
                break;
            default:
                if (desertChaserCapsuleTouchesGrid(arg0)) {
                    actorMoveForward(arg0->extra.tmd->coords, -85);
                } else {
                    actorMoveForward(arg0->extra.tmd->coords, -120);
                }
                break;
        }
    } else {
        ActorContact_PushContact(arg0->extra.tmd->coords, work->objs[2].contacts, 5);
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

static void Actor00100_Fn070DC(Task* arg0)
{
    DesertChaserWork*       work;
    GfxCoord*               coord;
    GfxCoord*               coord2;
    GfxCoord*               facing;
    GfxCoord*               facing2;
    TmdObject*              obj;
    s16                     delta;
    s32                     playerX;
    s16                     delta2;
    s16                     z;
    s16                     targetYaw;
    s16                     wrapped;
    s16                     wrappedYaw;
    s16                     wrapped2;
    s32                     angle;
    s32                     angle2;
    s32                     finalDelta;
    s32                     firstDelta;
    s32                     magnitude;
    Actor00100AngleScratch* head;
    Actor00100AngleScratch* scratch;

    head    = SCRATCH_STACK_CURSOR(Actor00100AngleScratch);
    work    = arg0->work;
    scratch = (SCRATCH_STACK_CURSOR(Actor00100AngleScratch) = head - 1);
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->objs[0].obj.radius = 0x19C;
        work->field_828          = 1;
        work->field_82E          = 2;
        work->field_82A          = 0;
        work->field_83E          = 0;
        work->objs[2].obj.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->field_832          = work->field_834;
        desertChaserAnimTick(arg0);
        work->field_6 = 0;
    }
    ActorContact_PushContact(arg0->extra.tmd->coords, work->objs[2].contacts, 5);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    coord                                 = arg0->extra.tmd->coords;
    head[-1].x                            = (s16)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
    scratch->y                            = (s16)(gPlayerStatus.coordMtx->t[1] - coord->coord.t[1]);
    scratch->z                            = (s16)(gPlayerStatus.coordMtx->t[2] - coord->coord.t[2]);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    desertChaserAnimTick(arg0);
    facing  = arg0->extra.tmd->coords;
    angle   = ratan2((s32)head[-1].x, (s32)scratch->z);
    delta   = angle - ratan2((s32)-facing->coord.m[2][0], (s32)facing->coord.m[2][2]);
    wrapped = delta;

    if (delta < 0) {
    wrapNegative:
        if (wrapped < -0x800) {
            wrapped += 0x1000;
            goto wrapNegative;
        }
    } else {
    wrapPositive:
        if (wrapped >= 0x801) {
            wrapped -= 0x1000;
            goto wrapPositive;
        }
    }
    firstDelta         = wrapped;
    scratch->delta     = firstDelta;
    work->field_840    = firstDelta;
    playerX            = -(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0];
    scratch->yaw       = ratan2((s32)playerX, (s32)(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
    coord2             = arg0->extra.tmd->coords;
    scratch->x         = (s16)(gPlayerStatus.coordMtx->t[0] - coord2->coord.t[0]);
    scratch->y         = (s16)(gPlayerStatus.coordMtx->t[1] - coord2->coord.t[1]);
    z                  = gPlayerStatus.coordMtx->t[2] - coord2->coord.t[2];
    scratch->z         = z;
    targetYaw          = ratan2((s32)scratch->x, (s32)z) + 0x800;
    wrappedYaw         = targetYaw;
    scratch->targetYaw = targetYaw;

    if (targetYaw < 0) {
    wrapYawNegative:
        if (wrappedYaw < -0x800) {
            wrappedYaw += 0x1000;
            goto wrapYawNegative;
        }
    } else {
    wrapYawPositive:
        if (wrappedYaw >= 0x801) {
            wrappedYaw -= 0x1000;
            goto wrapYawPositive;
        }
    }
    scratch->targetYaw = wrappedYaw;
    facing2            = arg0->extra.tmd->coords;
    angle2             = ratan2((s32)scratch->x, (s32)scratch->z);
    delta2             = angle2 - ratan2((s32)-facing2->coord.m[2][0], (s32)facing2->coord.m[2][2]);
    wrapped2           = delta2;

    if (delta2 < 0) {
    wrapFinalNegative:
        if (wrapped2 < -0x800) {
            wrapped2 += 0x1000;
            goto wrapFinalNegative;
        }
    } else {
    wrapFinalPositive:
        if (wrapped2 >= 0x801) {
            wrapped2 -= 0x1000;
            goto wrapFinalPositive;
        }
    }
    finalDelta      = wrapped2;
    scratch->delta  = (s16)finalDelta;
    work->field_840 = (s16)finalDelta;
    magnitude       = abs(scratch->targetYaw - scratch->yaw);
    if (magnitude >= 0x601) {
        Gp_ArmStateF0(1);
        work->field_0 = 0x1C;
    }
    SCRATCH_STACK_RELEASE_BLOCK(Actor00100AngleScratch);
}

#include "../../shared/desert_chaser_steer.inc.c"

static void Actor00100_Fn07650(Task* arg0)
{
    Enemy*            ctx;
    DesertChaserWork* work;
    TmdObject*        obj;
    s32               sound;
    s32               sound2;
    s32               pan;
    s32               pan2;

    work = arg0->work;
    ctx  = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        obj                         = arg0->extra.tmd;
        ctx->node.state.parts.flags = 0;
        obj->flags                  = 0;
        Tmd_AllocBuffers(obj);
        work->objs[0].obj.radius = 0x19C;
        work->field_82E          = 0xA;
        work->field_828          = 1;
        work->field_82A          = 0;
        work->objs[2].obj.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->field_832          = work->field_834;
        desertChaserAnimTick(arg0);
        sound = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40010009;
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        ctx->hp = ctx->hp - 0xF;
        func_800DA6E8(&ctx->node, 0xF, 0);
        if (ctx->hp <= 0) {
            ctx->hp = 1;
        } else {
            sound2 = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40010007;
            pan2   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(sound2, pan2, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        }
    }
    ActorContact_PushContact(arg0->extra.tmd->coords, work->objs[2].contacts, 5);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    desertChaserAnimTick(arg0);
    if (work->slots[1].status.fields.flags & 0x100) {
        if (ctx->hp <= 0) {
            work->field_0 = 0x15;
        } else if (ctx->reactionFlags & ENEMY_REACTION_BUILDUP) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

#include "../../shared/desert_chaser_roam.inc.c"

#include "../../shared/desert_chaser_turn_step.inc.c"

#include "../../shared/desert_chaser_turn_step_probe.inc.c"

static void Actor00100_Fn08E7C(Task* arg0)
{
    DesertChaserWork*         work;
    GfxCoord*                 coord;
    GfxCoord*                 coord2;
    GfxCoord*                 facing;
    GfxCoord*                 facing2;
    TmdObject*                obj;
    s16                       delta;
    s32                       playerX;
    s16                       delta2;
    s16                       z;
    s16                       targetYaw;
    s16                       wrapped;
    s16                       wrappedYaw;
    s16                       wrapped2;
    s32                       angle;
    s32                       angle2;
    s32                       finalDelta;
    s32                       firstDelta;
    Actor00100ProjectScratch* head;
    Actor00100ProjectScratch* scratch;

    head    = SCRATCH_STACK_CURSOR(Actor00100ProjectScratch);
    work    = arg0->work;
    scratch = (SCRATCH_STACK_CURSOR(Actor00100ProjectScratch) = head - 1);
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->objs[0].obj.radius = 0x19C;
        work->field_828          = 1;
        work->field_82E          = 2;
        work->field_82A          = 0;
        work->field_83E          = 0;
        work->objs[2].obj.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->field_832          = work->field_834;
        desertChaserAnimTick(arg0);
        work->field_6 = 0;
    }
    Gp_UpdateCoord(arg0->extra.tmd->coords);
    gte_SetTransMatrix(&arg0->extra.tmd->coords->workm);
    gte_SetRotMatrix(&arg0->extra.tmd->coords->workm);
    head[-1].x = 0;
    scratch->y = 0;
    scratch->z = 0;
    gte_ldv0(scratch);
    gte_rtps();
    gte_stsxy(&head[-1].screenX);
    gte_stdp(&head[-1].dp);
    gte_stflg(&head[-1].flag);
    gte_stszotz(&head[-1].depth);
    ActorContact_PushContact(arg0->extra.tmd->coords, work->objs[2].contacts, 5);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    coord                                 = arg0->extra.tmd->coords;
    head[-1].x                            = (s16)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
    scratch->y                            = (s16)(gPlayerStatus.coordMtx->t[1] - coord->coord.t[1]);
    scratch->z                            = (s16)(gPlayerStatus.coordMtx->t[2] - coord->coord.t[2]);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    desertChaserAnimTick(arg0);
    facing  = arg0->extra.tmd->coords;
    angle   = ratan2((s32)head[-1].x, (s32)scratch->z);
    delta   = angle - ratan2((s32)-facing->coord.m[2][0], (s32)facing->coord.m[2][2]);
    wrapped = delta;

    if (delta < 0) {
    wrapNegative:
        if (wrapped < -0x800) {
            wrapped += 0x1000;
            goto wrapNegative;
        }
    } else {
    wrapPositive:
        if (wrapped >= 0x801) {
            wrapped -= 0x1000;
            goto wrapPositive;
        }
    }
    firstDelta         = wrapped;
    scratch->delta     = firstDelta;
    work->field_840    = firstDelta;
    playerX            = -(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0];
    scratch->yaw       = ratan2((s32)playerX, (s32)(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
    coord2             = arg0->extra.tmd->coords;
    scratch->x         = (s16)(gPlayerStatus.coordMtx->t[0] - coord2->coord.t[0]);
    scratch->y         = (s16)(gPlayerStatus.coordMtx->t[1] - coord2->coord.t[1]);
    z                  = gPlayerStatus.coordMtx->t[2] - coord2->coord.t[2];
    scratch->z         = z;
    targetYaw          = ratan2((s32)scratch->x, (s32)z) + 0x800;
    wrappedYaw         = targetYaw;
    scratch->targetYaw = targetYaw;

    if (targetYaw < 0) {
    wrapYawNegative:
        if (wrappedYaw < -0x800) {
            wrappedYaw += 0x1000;
            goto wrapYawNegative;
        }
    } else {
    wrapYawPositive:
        if (wrappedYaw >= 0x801) {
            wrappedYaw -= 0x1000;
            goto wrapYawPositive;
        }
    }
    scratch->targetYaw = wrappedYaw;
    facing2            = arg0->extra.tmd->coords;
    angle2             = ratan2((s32)scratch->x, (s32)scratch->z);
    delta2             = angle2 - ratan2((s32)-facing2->coord.m[2][0], (s32)facing2->coord.m[2][2]);
    wrapped2           = delta2;

    if (delta2 < 0) {
    wrapFinalNegative:
        if (wrapped2 < -0x800) {
            wrapped2 += 0x1000;
            goto wrapFinalNegative;
        }
    } else {
    wrapFinalPositive:
        if (wrapped2 >= 0x801) {
            wrapped2 -= 0x1000;
            goto wrapFinalPositive;
        }
    }
    finalDelta      = wrapped2;
    scratch->delta  = (s16)finalDelta;
    work->field_840 = (s16)finalDelta;
    if (abs(scratch->screenX) < 0x78 && abs(scratch->screenY) < 0x64 && abs(scratch->delta) < 0x200) {
        Gp_ArmStateF0(1);
        work->field_0 = 0x1C;
    }
    SCRATCH_STACK_RELEASE_BLOCK(Actor00100ProjectScratch);
}

static void Actor00100_Fn09310(Task* arg0)
{
    SVECTOR           vector;
    EffectWork*       effect;
    EffectWork*       effect2;
    EffectWork*       effect3;
    EffectWork*       effect4;
    Task*             task;
    Task*             task2;
    Task*             task3;
    Task*             task4;
    TmdObject*        obj;
    u16               next;
    TmdObject*        effectObj;
    TmdObject*        effectObj2;
    TmdObject*        effectObj3;
    TmdObject*        effectObj4;
    DesertChaserWork* work;
    Enemy*            ctx;

    work = arg0->work;
    ctx  = arg0->spawnArg2.pointer;
    obj  = arg0->extra.tmd;
    if (work->field_4 != 0) {
        work->hitFlag               = 0;
        obj->flags                  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->objs[0].obj.radius    = 0x19C;
        work->objs[2].obj.flags     = (u16)(work->objs[2].obj.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        ctx->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->field_844             = 0;
        work->field_840             = 0;
        work->field_83E             = 0;
        work->field_6               = 0U;
        vector.vx                   = 0x64;
        vector.vz                   = 0;
        vector.vy                   = 0;
    }
    next          = work->field_6 + 1;
    work->field_6 = next;
    if ((s16)next == 2) {
        obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        Tmd_FreeBuffers(obj);
        D_80114B34[5].data.model = &_gActor00100DesertChaserBurstLegRight;
        vector.vz                = 0x64;
        vector.vy                = 0;
        vector.vx                = 0;
        effect                   = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, &arg0->extra.tmd->coords[9], 0x200, &vector);
        if (effect != NULL) {
            task                               = effect->task;
            task->extra.tmd->texturePageOffset = (u8)arg0->extra.tmd->texturePageOffset;
            task->extra.tmd->clutRowOffset     = (u8)arg0->extra.tmd->clutRowOffset;
            effectObj                          = task->extra.tmd;
            if (effectObj->buffer != 0) {
                tmdProcessStream(effectObj);
                tmdProcessStream(task->extra.tmd);
            }
        }
        if (work->field_6 == 2) {
            D_80114B34[5].data.model = &_gActor00100DesertChaserBurstLegLeft;
            vector.vy                = 0;
            vector.vx                = 0;
            effect2                  = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, &arg0->extra.tmd->coords[12], 0x200, &vector);
            if (effect2 != NULL) {
                task2                               = effect2->task;
                task2->extra.tmd->texturePageOffset = (u8)arg0->extra.tmd->texturePageOffset;
                task2->extra.tmd->clutRowOffset     = (u8)arg0->extra.tmd->clutRowOffset;
                effectObj2                          = task2->extra.tmd;
                if (effectObj2->buffer != 0) {
                    tmdProcessStream(effectObj2);
                    tmdProcessStream(task2->extra.tmd);
                }
            }
        }
    }
    if (work->field_6 == 4) {
        D_80114B34[5].data.model = &_gActor00100DesertChaserBurstTorso;
        effect3                  = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, &arg0->extra.tmd->coords[1], 0x200, NULL);
        if (effect3 != NULL) {
            task3                               = effect3->task;
            task3->extra.tmd->texturePageOffset = (u8)arg0->extra.tmd->texturePageOffset;
            task3->extra.tmd->clutRowOffset     = (u8)arg0->extra.tmd->clutRowOffset;
            effectObj3                          = task3->extra.tmd;
            if (effectObj3->buffer != 0) {
                tmdProcessStream(effectObj3);
                tmdProcessStream(task3->extra.tmd);
            }
        }
    }
    if (work->field_6 == 5) {
        D_80114B34[5].data.model = &_gActor00100DesertChaserBurstHead;
        effect4                  = Gp_SpawnEff(0xA0000 | 5, &arg0->extra.tmd->coords[3], 0x200, NULL);
        if (effect4 != NULL) {
            task4                               = effect4->task;
            task4->extra.tmd->texturePageOffset = (u8)arg0->extra.tmd->texturePageOffset;
            task4->extra.tmd->clutRowOffset     = (u8)arg0->extra.tmd->clutRowOffset;
            effectObj4                          = task4->extra.tmd;
            if (effectObj4->buffer != 0) {
                tmdProcessStream(effectObj4);
                tmdProcessStream(task4->extra.tmd);
            }
        }
    }
    if (work->field_6 == 0x1E) {
        ctx->hp = 0;
        Gp_UnlinkObj(&work->objs[0].obj);
        Gp_UnlinkObj(&work->objs[1].obj);
        Gp_UnlinkObj(&work->capsuleBody.body);
        Gp_UnlinkObj(&work->objs[2].obj);
        ctx->recs = 0;
    }
    if ((work->field_6 >= 0x1F) && (work->reported == 0) && (Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 1, 0, 0)) {
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, (s32)(ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT), 0);
        }
        arg0->state++;
    }
}

static void Actor00100_Fn09724(Task* arg0)
{
    Enemy*            ctx;
    DesertChaserWork* work;
    TmdObject*        obj;
    s16               timer;
    s16               state;
    s32               sound;
    s32               pan;

    work = arg0->work;
    SCRATCH_STACK_RESERVE_BYTES(0x10);
    ctx = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        obj                         = arg0->extra.tmd;
        ctx->node.state.parts.flags = 0;
        obj->flags                  = 0;
        Tmd_AllocBuffers(obj);
        work->objs[0].obj.radius = 0x19C;
        work->field_828          = 1;
        work->field_82E          = 3;
        work->field_82A          = 0;
        work->field_6            = 0;
        work->field_840          = 0;
        work->field_844          = 0;
        work->field_83E          = 0;
        work->field_842          = 0;
        work->objs[2].obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        work->field_832          = work->field_834;
        func_mine_mesa_801811C4(0x7D0);
    }
    work->field_6 += 1;
    desertChaserAnimTick(arg0);
    state = work->field_82E;
    switch (state) {
        case 3:
            timer = work->field_6;
            if (timer < 0x1E) {
                Actor00100_ScaleTransform(&work->color, (timer << 12) / 30);
            } else {
                work->field_82E = 0xD;
                work->field_828 = 1;
                work->field_6   = 0;
                sound           = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40010010;
                pan             = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                SndEvt_EnqueueType6(sound, (s32)pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            }
            actorMoveForward(arg0->extra.tmd->coords, 200);
            break;
        case 13:
            if (work->field_6 <= ((s16)work->field_834 * 17) / 16) {
                actorMoveForwardNonzero(arg0->extra.tmd->coords, ((s16)work->field_834 * 2000) / 272);
            } else if (work->field_6 <= ((s16)work->field_834 * 25) / 16) {
                actorMoveForwardNonzero(arg0->extra.tmd->coords, ((s16)work->field_834 * 1000) / 192);
            }
            if (work->slots[1].status.fields.flags & 0x100) {
                work->field_0 = 0x26;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

static void Actor00100_Fn09CCC(Task* arg0)
{
    Enemy*            ctx;
    DesertChaserWork* work;
    GfxCoord*         coord;
    TmdObject*        obj;
    TmdObject*        hiddenObj;
    TmdObject*        finishedObj;
    s32               timer;
    s32               y;
    s32               sound;
    s32               sound2;
    s32               depth;
    s32               pan2;
    s32               pan;

    work = arg0->work;
    SCRATCH_STACK_RESERVE_BYTES(0x14);
    ctx = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        obj                         = arg0->extra.tmd;
        ctx->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                  = 0;
        Tmd_AllocBuffers(obj);
        work->objs[0].obj.radius = 0x19C;
        work->field_828          = 1;
        work->field_82E          = 3;
        work->field_82A          = 0;
        work->field_6            = 0;
        work->field_840          = 0;
        work->field_844          = 0;
        work->field_83E          = 0;
        work->field_842          = 0;
        work->objs[2].obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        work->field_832          = work->field_834;
    }
    work->field_6 += 1;
    desertChaserAnimTick(arg0);
    switch (work->field_82E) {
        case 3:
            actorMoveForwardNonzero(arg0->extra.tmd->coords, ((s16)work->field_834 * 1000) / 192);
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            if (work->field_6 >= 0xD) {
                work->field_82E = 0xE;
                work->field_828 = 1;
                work->field_6   = 0;
            }
            break;
        case 14:
            actorMoveForwardNonzero(arg0->extra.tmd->coords, ((s16)work->field_834 * 1300) / 192);
            timer = work->field_6;
            if (timer == 0xF) {
                if ((Gp_GetViewIndex() & 0xFF) == 8) {
                    sound = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54010005;
                    pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                    depth = worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords);
                    SndEvt_EnqueueType6(sound, (s8)pan, (s8)(depth + abs(worldCoordGetOriginAudioPan(arg0->extra.tmd->coords)) / 2));
                } else {
                    sound2 = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54010005;
                    pan2   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                    SndEvt_EnqueueType6(sound2, (s8)pan2, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                }
                timer = work->field_6;
            }
            if (timer >= 4) {
                coord = arg0->extra.tmd->coords;
                y     = coord->coord.t[1];
                if (y < 0x2EE0) {
                    coord->coord.t[1] = y + ((timer - 3) * 0x21);
                }
            }
            if (work->field_6 == 0x64) {
                Gp_UnlinkObj(&work->objs[0].obj);
                Gp_UnlinkObj(&work->objs[1].obj);
                Gp_UnlinkObj(&work->capsuleBody.body);
                Gp_UnlinkObj(&work->objs[2].obj);
                ctx->recs         = 0;
                ctx->hp           = 0;
                hiddenObj         = arg0->extra.tmd;
                hiddenObj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            if (work->field_6 == 0x65) {
                finishedObj         = arg0->extra.tmd;
                finishedObj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            }
            if ((work->field_6 >= 0x79) && (work->reported != 1) && (Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL) && gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, (s32)(ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT), 0);
                work->field_C2A = 1;
                arg0->state++;
            }

            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x14);
}

/// Per-state handlers the per-frame update calls, indexed by the work block's
/// `field_0`, called unconditionally; a null entry is a state with no handler.
static const Actor00100StateTable Actor00100_D000F0 = { {
    Actor00100_Fn0B4D8,
    Actor00100_Fn08E7C,
    Actor00100_Fn0BC14,
    Actor00100_Fn09310,
    desertChaserStunned,
    Actor00100_Fn09724,
    Actor00100_Fn09CCC,
    desertChaserStagger,
    desertChaserTurnStep,
    desertChaserTurnStepProbe,
    desertChaserCollapse,
    Actor00100_Fn0BB2C,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    Actor00100_Fn0B8D8,
    NULL,
    NULL,
    desertChaserFlinch,
    Actor00100_Fn04270,
    NULL,
    NULL,
    desertChaserApproach,
    NULL,
    NULL,
    NULL,
    desertChaserPursue,
    Actor00100_Fn061FC,
    desertChaserSpawnAim,
    Actor00100_Fn06C10,
    Actor00100_Fn070DC,
    desertChaserSteer,
    Actor00100_Fn0B658,
    Actor00100_Fn07650,
    Actor00100_Fn0B730,
    desertChaserStrike,
    desertChaserRoam,
} };

static void Actor00100_Fn0A288(Enemy* enemy, Task* actor)
{
    PlayerStatus*        config;
    s32                  excludedState;
    VECTOR               pos;
    Actor00100StateTable states;
    GfxCoord*            actorcoord;

    SVECTOR**         scratchHead;
    SVECTOR*          scratch;
    s32               state;
    s16               modeState;
    s16               height;
    s16               modeHeight;
    s16               initialState;
    s16               finalState;
    s16               i;
    GfxCoord*         playerCoord;
    s32               sound;
    s32               sound2;
    s32               depth;
    s32               result;
    s32               action;
    s32               nextAction;
    s32               pan2;
    s32               pan;
    AnimationSet**    nextPlayerSets;
    DesertChaserWork* actorWork;
    Task*             playerSlot;
    DesertChaserWork* work;
    Task*             player;
    AnimationSet**    playerSets;
    Task*             slot;
    GfxCoord*         coord;
    void*             message;
    void*             nextMessage;

    work                                   = actor->work;
    player                                 = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    config                                 = &gPlayerStatus;
    states                                 = Actor00100_D000F0;
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(actor->extra.tmd->coords);
    pos.vx = actor->extra.tmd->coords->workm.t[0];
    pos.vy = actor->extra.tmd->coords->workm.t[1];
    pos.vz = actor->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            initialState = work->field_0;
            if (initialState != 21 && initialState != 0 && initialState != 6 && initialState != 3) {
                actor->extra.tmd->flags = 0;
                height                  = actor->extra.tmd->coords->coord.t[1];
                Actor00100_Fn01900(actor, 1, 3, 0x12C, (s32)height, 0xFF);
                Actor00100_Fn01900(actor, 3, 4, 0xC8, (s32)height, 0xFF);
                Actor00100_Fn01900(actor, 1, 0xB, 0xFA, (s32)height, 0xFF);
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            modeState = work->field_0;
            if ((modeState != 0x15) && (modeState != 0) && (modeState != 6) && (modeState != 3)) {
                actor->extra.tmd->flags = 0;
                modeHeight              = actor->extra.tmd->coords->coord.t[1];
                Actor00100_Fn01900(actor, 1, 3, 0x12C, (s32)modeHeight, 0xFF);
                Actor00100_Fn01900(actor, 3, 4, 0xC8, (s32)modeHeight, 0xFF);
                Actor00100_Fn01900(actor, 1, 0xB, 0xFA, (s32)modeHeight, 0xFF);
            }
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            actor->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }
    scratchHead = (SVECTOR**)SCRATCH_HEAD_ADDR;
    scratch     = Actor00100_AllocVector(scratchHead);
    if (work->hitCooldown > 0) {
        work->hitCooldown = (s16)((u16)work->hitCooldown - 1);
    } else if (work->field_0 != 5) {
        Actor00100_Fn0375C(actor);
    }
    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = (s16)(u16)work->field_0;
    excludedState = 21;
    state         = work->field_0;
    if (work->reported == 1) {
        if ((state != excludedState) && (state != 0) && (state != 6) && (state != 3)) {
            actorWork = actor->work;
            slot      = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            if ((slot != NULL) && (actorWork->playerMove.collisionRequests == GAME_ACTOR_COLLISION_REQUEST_MASK)) {
                playerCoord = slot->extra.tmd->coords;
                actorcoord  = actor->extra.tmd->coords;
                if (abs(playerCoord->coord.t[1] - actorcoord->coord.t[1]) >= 0x321) {
                    playerCoord->coord.t[1]               = actorcoord->coord.t[1];
                    slot->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                }
            }
        }
        action          = work->params[0];
        work->field_C28 = (u16)(work->field_C28 + 1);
        switch (action) {
            case 4:
                break;
            case 1:
                if (work->playerMove.collisionRequests == (GAME_ACTOR_COLLISION_REQUEST_MASK << GAME_ACTOR_COLLISION_DISABLE_REQUEST_SHIFT)) {
                    TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_MOVE_BY, &work->playerMove, 0);
                    if (work->field_C28 == 0xF) {
                        if ((Gp_GetViewIndex() & 0xFF) == 8) {
                            sound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54010004;
                            pan   = (s8)worldCoordGetOriginAudioPan(actor->extra.tmd->coords);
                            depth = worldCoordGetOriginAudioDepth(actor->extra.tmd->coords);
                            SndEvt_EnqueueType6(sound, (s8)pan, (s8)(depth + abs(worldCoordGetOriginAudioPan(actor->extra.tmd->coords)) / 2));
                        } else {
                            sound2 = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54010004;
                            pan2   = (s8)worldCoordGetOriginAudioPan(actor->extra.tmd->coords);
                            SndEvt_EnqueueType6(sound2, pan2, (s8)worldCoordGetOriginAudioDepth(actor->extra.tmd->coords));
                        }
                    }
                    if (work->field_C28 >= 0xF) {
                        coord = player->extra.tmd->coords;
                        if (coord->coord.t[1] < 0x1770) {
                            work->playerMove.displacement.vy += 0x21;
                        } else {
                            coord->coord.t[1]                = 0x1770;
                            work->playerMove.displacement.vx = 0;
                            work->playerMove.displacement.vy = 0;
                            work->playerMove.displacement.vz = 0;
                        }
                    }
                    if (player->extra.tmd->coords->coord.t[1] >= 0x1770) {
                        if (config->hp > 0) {
                            for (i = 0; i < 10; i++) {
                                gGameSession->deathSoundCountdown = GAME_SESSION_DEATH_SOUND_HOLD;
                                playerSlot                        = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                                if (taskMessageDispatch(playerSlot, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackObjPair(enemy, 4), 0) == 1)
                                    break;
                            }
                        }
                    }
                } else if (work->playerMove.displacement.vx == 0) {
                    if (work->playerMove.displacement.vz != 0) {
                        goto dispatchMotion;
                    }
                } else {
                dispatchMotion:
                    if ((s16)TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_MOVE_BY, &work->playerMove, 0) != 1) {
                        if (work->field_C28 >= 0xF) {
                            work->playerMove.displacement.vx >>= 1;
                            work->playerMove.displacement.vy >>= 1;
                            work->playerMove.displacement.vz >>= 1;
                        }
                    } else if (((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 1, 0, 0)) && (work->playerMove.displacement.vx != 0) && (work->playerMove.displacement.vz != 0) && (work->field_C28 < 6)) {
                        if (Actor00100_InRegion(player)) {
                            if (Actor00100_InDirection(player, &work->playerMove.displacement)) {
                                work->playerMove.collisionRequests = (GAME_ACTOR_COLLISION_REQUEST_MASK << GAME_ACTOR_COLLISION_DISABLE_REQUEST_SHIFT);
                                Gp_StateC08.flags                  = (u8)(Gp_StateC08.flags | ATTACHMENT_FLAG_EVENT_LOCK);
                                Gp_PulseState1C();
                                scratch->vx = (u16)work->playerMove.displacement.vx;
                                scratch->vy = 0;
                                scratch->vz = (u16)work->playerMove.displacement.vz;
                                VectorNormalSS(scratch, scratch);
                                gte_lddp(250);
                                gte_ldsv(scratch);
                                gte_gpf12();
                                gte_stsv(scratch);
                                work->playerMove.displacement.vx = (s16)scratch->vx;
                                work->playerMove.displacement.vy = 0x3C;
                                work->playerMove.displacement.vz = scratch->vz;
                            } else {
                                goto clearMotion;
                            }
                        } else {
                            goto clearMotion;
                        }
                    } else {
                    clearMotion:
                        work->playerMove.displacement.vx = 0;
                        work->playerMove.displacement.vy = 0;
                        work->playerMove.displacement.vz = 0;
                    }
                }
                break;
            case 2:
                playerSets = work->animCommand;
                if (playerSets == gDesertChaserRearAnim) {
                    if ((config->hp > 0) && (work->field_C28 >= 0x17)) {
                        message         = &work->animCommand;
                        playerSets[4]   = Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon]->table.sets[7];
                        work->params[0] = 4;
                        work->params[1] = 1;
                        work->params[2] = 3;
                        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, message, 0);
                        work->field_C28 = 0U;
                    }
                } else if ((config->hp > 0) && (work->field_C28 >= 0x22)) {
                    message                   = &work->animCommand;
                    gDesertChaserFrontAnim[4] = Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon]->table.sets[7];
                    work->params[0]           = 4;
                    work->params[1]           = 1;
                    work->params[2]           = 3;
                    TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, message, 0);
                    work->field_C28 = 0U;
                }
                break;
            case 3:
                if ((work->field_C28 < 6) && (config->hp > 0) && ((work->playerMove.displacement.vx != 0) || (work->playerMove.displacement.vz != 0))) {
                    result = TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_MOVE_BY, &work->playerMove, 0);
                    if (result == 1) {
                        work->playerMove.displacement.vx = 0;
                        work->playerMove.displacement.vy = 0;
                        work->playerMove.displacement.vz = 0;
                        work->playerMove.keepControl     = result;
                    }
                }
                break;
            case 5:
                if ((config->hp > 0) && (work->field_C28 >= 7)) {
                    taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 2, 0);
                    work->reported = 0;
                }
                break;
        }
        if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
            nextAction = work->params[0];
            switch (nextAction) {
                case 1:
                    if ((((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(4, 1, 0, 0)) || (work->playerMove.collisionRequests != (GAME_ACTOR_COLLISION_REQUEST_MASK << GAME_ACTOR_COLLISION_DISABLE_REQUEST_SHIFT))) && (config->hp > 0)) {
                        nextMessage     = &work->animCommand;
                        work->params[1] = 0;
                        work->params[2] = 0;
                        work->params[0] = 2;
                        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, nextMessage, 0);
                        work->field_C28 = 0U;
                    }
                    break;
                case 3:
                    if (config->hp > 0) {
                        work->params[1] = 1;
                        work->params[2] = 6;
                        work->params[0] = 5;
                        nextPlayerSets  = work->animCommand;
                        if (nextPlayerSets == gDesertChaserRearAnim) {
                            nextPlayerSets[5] = Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon]->table.sets[9];
                        } else {
                            gDesertChaserFrontAnim[5] = Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon]->table.sets[9];
                        }
                        nextMessage = &work->animCommand;
                        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, nextMessage, 0);
                        work->field_C28 = 0U;
                    }
                    break;
                case 4:
                case 7:
                    if (config->hp > 0) {
                        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 2, 0);
                        work->reported = 0;
                    }
                    break;
            }
        }
    }
    states.fn[work->field_0](actor);
    finalState = work->field_0;
    if ((finalState != 0x15) && (finalState != 0) && (finalState != 6) && (finalState != 5) && (finalState != 3)) {
        work->objs[0].obj.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->objs[1].obj.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->objs[0].obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->objs[1].obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    Gp_ClearRec18Occupied(work->objs[2].contacts);
    Gp_ClearRec18Occupied(work->objs[0].contacts);
    Gp_ClearRec18Occupied(work->objs[1].contacts);
    Gp_ClearRec18Occupied(work->capsuleBody.contacts);
    if ((work->field_C2A == 1) && (work->reported == 0)) {
        work->field_C2A = 0;
        Gp_ReleaseStateF0Add(actor, 1);
    }
    if (work->poseYawPrev > 0) {
        work->poseYawPrev = (s16)((u16)work->poseYawPrev - 1);
    }
    scratch->vx = 0;
    scratch->vy = 0;
    scratch->vz = 0;
    actorTransformToView(actor->extra.tmd->coords + 2, scratch);
    enemy->bodyPos.vx = (s32)(s16)scratch->vx;
    enemy->bodyPos.vy = (s32)scratch->vy;
    enemy->bodyPos.vz = (s32)scratch->vz;
    enemy->coord      = &gGfxViewCoord;
    SCRATCH_STACK_RELEASE_BYTES(8);
}

/// The task's handlers, indexed by `Task::state`: set-up, the per-frame state
/// dispatch, a wait for the pending release before advancing, and teardown.
static const DesertChaserTaskStates gDesertChaserTaskStates = { {
    Actor00100_Fn02C54,
    Actor00100_Fn0A288,
    Actor00100_Fn0BCBC,
    enemyDestroy,
} };

s32 Actor00100_Fn0B134(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
}

static s16 Actor00100_Fn0B13C(Task* arg0)
{
    DesertChaserWork* work  = (DesertChaserWork*)arg0->work;
    s16               found = 0;
    s16               i;
    s32               value;

    for (i = 0; i < 5; i++) {
        value = work->capsuleBody.contacts[i].key.value;
        if (value == 0) {
            break;
        }
        if ((value & 0xFFFF0000) == 0x100000) {
            found = 1;
        }
    }
    return found;
}

#include "../../shared/actor_messages_visibility.inc.c"

#include "../../shared/actor_messages_is_present.inc.c"

#include "../../shared/actor_messages_place_yaw.inc.c"

#include "../../shared/desert_chaser_exit.inc.c"

static void Actor00100_Fn0B3DC(Task* arg0, s16 arg1, s16 arg2)
{
    DesertChaserWork* work = arg0->work;
    s32               spawn;

    switch (arg1) {
        case 0:
        case 1:
            spawn              = 1;
            work->field_898.vz = 0;
            work->field_898.vx = 0;
            work->field_898.vy = 0;
            break;
        case 9:
            spawn              = 1;
            work->field_898.vz = 0;
            work->field_898.vx = 0;
            work->field_898.vy = 0x2BC;
            break;
        case 7:
            spawn              = 1;
            work->field_898.vz = 0;
            work->field_898.vx = 0;
            work->field_898.vy = 0x2BC;
            break;
        case 14:
        case 17:
            spawn              = 1;
            work->field_898.vz = 0;
            work->field_898.vx = 0;
            work->field_898.vy = 0x258;
            break;
        default:
            spawn = 0;
            break;
    }

    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && spawn == 1) {
        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[arg1], arg2 | 0x80000000, &work->field_898);
    }
}

static void Actor00100_Fn0B4D8(Task* arg0)
{
    TmdObject*        obj;
    DesertChaserWork* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                                               |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->objs[2].obj.flags                                  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
    }
}

#include "../../shared/desert_chaser_stunned.inc.c"

static void Actor00100_Fn0B658(Task* arg0)
{
    TmdObject*        obj;
    DesertChaserWork* work;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->objs[0].obj.radius = 0x19C;
        work->field_828          = 1;
        work->field_82E          = 8;
        work->field_82A          = 0;
        work->objs[2].obj.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->field_832          = work->field_834;
        desertChaserAnimTick(arg0);
        Gp_ArmStateF0(1);
    }
    ActorContact_PushContact(arg0->extra.tmd->coords, work->objs[2].contacts, 5);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    desertChaserAnimTick(arg0);
    if (work->slots[1].status.fields.flags & 0x100) {
        work->field_0 = 0x1C;
    }
}

static void Actor00100_Fn0B730(Task* arg0)
{
    DesertChaserWork* work;
    TmdObject*        obj;

    work = arg0->work;
    if (work->field_4 != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->objs[0].obj.radius = 0x19C;
        work->field_828          = 1;
        work->field_82E          = 0xC;
        work->field_82A          = 0;
        work->objs[2].obj.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->field_832          = work->field_834;
        desertChaserAnimTick(arg0);
    }
    desertChaserAnimTick(arg0);
    if (work->slots[1].status.fields.flags & 0x100) {
        work->field_0 = 0x26;
    }
}

#include "../../shared/desert_chaser_flinch.inc.c"

static void Actor00100_Fn0B8D8(Task* arg0)
{
    DesertChaserWork* work;
    u16               timer;
    u32               random;

    work = arg0->work;
    if (work->field_4 != 0) {
        random          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState = random;
        work->field_6   = work->poseVx + ((random >> 0x10) & 0xF);
    }
    desertChaserAnimTick(arg0);
    timer         = work->field_6 - 1;
    work->field_6 = timer;
    if ((s16)timer < 0) {
        if (((Enemy*)arg0->spawnArg2.pointer)->hp > 0) {
            work->field_0 = 0x24;
        } else {
            work->field_0 = 0x15;
        }
    }
}

#include "../../shared/desert_chaser_stagger.inc.c"

#include "../../shared/desert_chaser_collapse.inc.c"

static void Actor00100_Fn0BB2C(Task* arg0)
{
    Enemy*            ctx;
    DesertChaserWork* work;
    TmdObject*        obj;

    work = arg0->work;
    ctx  = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        obj                         = arg0->extra.tmd;
        work->hitFlag               = 0;
        obj->flags                  = 0;
        work->objs[0].obj.radius    = 0x19C;
        work->objs[2].obj.flags    |= WORLD_COLLISION_BODY_GRID_ENABLED;
        ctx->node.state.parts.flags = 0;
        work->field_828             = 2;
        work->field_82E             = 0x14;
        work->field_832             = 0x10;
        work->field_840             = 0;
        work->field_83E             = 0;
        if (ctx->hp <= 0) {
            Gp_SetStateF0Byte3(1);
        }
    }
    desertChaserAnimTick(arg0);
    if (work->slots[1].status.fields.flags & 0x100) {
        if (ctx->hp > 0) {
            if (ctx->reactionFlags & ENEMY_REACTION_BUILDUP) {
                work->field_0 = 4;
            } else {
                work->field_0 = 0x24;
            }
        } else {
            work->field_0 = 0x15;
        }
    }
}

static void Actor00100_Fn0BC14(Task* task)
{
}

static void Actor00100_Fn0BC1C(Task* arg0)
{
    DesertChaserWork* work;
    Task*             task;

    work = arg0->work;
    task = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if ((task != NULL) && (work->playerMove.collisionRequests == GAME_ACTOR_COLLISION_REQUEST_MASK)) {
        if (abs(task->extra.tmd->coords->coord.t[1] - arg0->extra.tmd->coords->coord.t[1]) > 800) {
            task->extra.tmd->coords->coord.t[1]   = arg0->extra.tmd->coords->coord.t[1];
            task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        }
    }
}

static void Actor00100_Fn0BCBC(Enemy* enemy, Task* task)
{
    DesertChaserWork* work;

    work = (DesertChaserWork*)task->work;
    if (work->field_C2A == 1) {
        work->field_C2A = 0;
        Gp_ReleaseStateF0Add(task, 1);
    }
    if (work->field_C2A == 0) {
        task->state++;
    }
}

#include "../../shared/desert_chaser_task.inc.c"
