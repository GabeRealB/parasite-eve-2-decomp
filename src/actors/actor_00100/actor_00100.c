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

extern TmdSource Actor00100_D10D60;

extern TmdSource Actor00100_D11234;

extern TmdSource Actor00100_D11F90;

extern TmdSource Actor00100_D12470;

extern DesertChaserAnimCommand gDesertChaserFrontAnim;

extern SVECTOR ActorContact_ScratchPosition;

/// The contact routines' scratch position.
static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

extern DesertChaserAnimCommand gDesertChaserRearAnim;

s32 Actor00100_Fn0B1A4(Task* arg0, s32 arg1, s32 arg2);

typedef struct {
    s32 id;
    union {
        s32  (*command)(Task*, s32, ActorCommand* request);
        void (*reset)(void);
        s32  (*value)(Task*, s32, s32);
        s32  (*task)(Task*);
        s32  (*placement)(Task*, s32, ActorTransform*);
    } handler;
} Actor00100MessageEntry;
STATIC_ASSERT_SIZEOF(Actor00100MessageEntry, 8);

extern Actor00100MessageEntry Actor00100_D1BA54[6];

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

static void desertChaserArmedAnimTick(Task* arg0);

static void Actor00100_Fn0B658(Task* arg0);

extern AnimationSet Actor00100_D129A4;

extern AnimationSet Actor00100_D12C60;

extern AnimationSet Actor00100_D13294;

extern AnimationSet Actor00100_D1386C;

extern AnimationSet Actor00100_D13CA0;

extern AnimationSet Actor00100_D14378;

extern AnimationSet Actor00100_D14978;

extern AnimationSet Actor00100_D14BD4;

extern AnimationSet Actor00100_D151F8;

extern AnimationSet Actor00100_D15648;

extern AnimationSet Actor00100_D15C2C;

extern AnimationSet Actor00100_D15DA8;

extern AnimationSet Actor00100_D1644C;

extern AnimationSet Actor00100_D16E64;

extern AnimationSet Actor00100_D17284;

extern AnimationSet Actor00100_D17548;

extern AnimationSet Actor00100_D17A4C;

extern AnimationSet Actor00100_D17FCC;

extern AnimationSet Actor00100_D1857C;

extern AnimationSet Actor00100_D18C3C;

extern AnimationSet Actor00100_D190D0;

extern AnimationSet Actor00100_D19918;

extern AnimationSet Actor00100_D1A0B4;

extern AnimationSet Actor00100_D1A8C0;

extern AnimationSet Actor00100_D1B0D4;

extern AnimationSet Actor00100_D1B388;

extern AnimationSet Actor00100_D1B6A8;

extern TmdSource Actor00100_D108C0;

s32 Actor00100_Fn00E58(Task*, s32, ActorCommand* request);

void Actor00100_Fn0B134(void);

void Actor00100_Fn0BD28(Task*);

extern s8 Actor00100_D1B6D0[25][25];

static const GpEnemyTaskFuncTable4 Actor00100_D001A0;

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
static void                Actor00100_Fn001FC(GfxCoord* coord, s16 yaw);
static s32                 desertChaserAvoidWalk(GfxCoord* coord, WorldCollisionContact* recs, s16 count, SVECTOR* pos);
static void                Actor00100_Fn01900(Task* actor, s16 firstJoint, s16 secondJoint, s16 width, s16 height, u8 shade);
static void                Actor00100_Fn01D74(Task* arg0);
static s32                 Actor00100_Fn01EEC(Task* arg0, DesertChaserWork* arg1);
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

TmdBone Actor00100_D0BDD4[18] = {
#include "assets/actor_400100_model_108C0_skeleton.inc"
};

u32 Actor00100_D0C05C[18] = {
#include "assets/actor_400100_model_108C0_partVerts.inc"
};

SVECTOR Actor00100_D0C0A4[266] = {
#include "assets/actor_400100_model_108C0_verts.inc"
};

SVECTOR Actor00100_D0C8F4[324] = {
#include "assets/actor_400100_model_108C0_normals.inc"
};

u32 Actor00100_D0D314[3435] = {
#include "assets/actor_400100_model_108C0_stream.inc"
};

TmdSource Actor00100_D108C0 = {
    0,
    17616,
    5944,
    18,
    Actor00100_D0C05C,
    Actor00100_D0C0A4,
    Actor00100_D0C8F4,
    Actor00100_D0BDD4,
    Actor00100_D0D314,
};

TmdBone Actor00100_D108E4[1] = {
#include "assets/actor_400100_model_10D60_skeleton.inc"
};

u32 Actor00100_D10908[1] = {
#include "assets/actor_400100_model_10D60_partVerts.inc"
};

SVECTOR Actor00100_D1090C[21] = {
#include "assets/actor_400100_model_10D60_verts.inc"
};

SVECTOR Actor00100_D109B4[1] = {
#include "assets/actor_400100_model_10D60_normals.inc"
};

u32 Actor00100_D109BC[233] = {
#include "assets/actor_400100_model_10D60_stream.inc"
};

TmdSource Actor00100_D10D60 = {
    0,
    1536,
    0,
    1,
    Actor00100_D10908,
    Actor00100_D1090C,
    Actor00100_D109B4,
    Actor00100_D108E4,
    Actor00100_D109BC,
};

TmdBone Actor00100_D10D84[1] = {
#include "assets/actor_400100_model_11234_skeleton.inc"
};

u32 Actor00100_D10DA8[1] = {
#include "assets/actor_400100_model_11234_partVerts.inc"
};

SVECTOR Actor00100_D10DAC[23] = {
#include "assets/actor_400100_model_11234_verts.inc"
};

SVECTOR Actor00100_D10E64[1] = {
#include "assets/actor_400100_model_11234_normals.inc"
};

u32 Actor00100_D10E6C[242] = {
#include "assets/actor_400100_model_11234_stream.inc"
};

TmdSource Actor00100_D11234 = {
    0,
    1612,
    0,
    1,
    Actor00100_D10DA8,
    Actor00100_D10DAC,
    Actor00100_D10E64,
    Actor00100_D10D84,
    Actor00100_D10E6C,
};

TmdBone Actor00100_D11258[1] = {
#include "assets/actor_400100_model_11F90_skeleton.inc"
};

u32 Actor00100_D1127C[1] = {
#include "assets/actor_400100_model_11F90_partVerts.inc"
};

SVECTOR Actor00100_D11280[59] = {
#include "assets/actor_400100_model_11F90_verts.inc"
};

SVECTOR Actor00100_D11458[81] = {
#include "assets/actor_400100_model_11F90_normals.inc"
};

u32 Actor00100_D116E0[556] = {
#include "assets/actor_400100_model_11F90_stream.inc"
};

TmdSource Actor00100_D11F90 = {
    0,
    3812,
    0,
    1,
    Actor00100_D1127C,
    Actor00100_D11280,
    Actor00100_D11458,
    Actor00100_D11258,
    Actor00100_D116E0,
};

TmdBone Actor00100_D11FB4[1] = {
#include "assets/actor_400100_model_12470_skeleton.inc"
};

u32 Actor00100_D11FD8[1] = {
#include "assets/actor_400100_model_12470_partVerts.inc"
};

SVECTOR Actor00100_D11FDC[19] = {
#include "assets/actor_400100_model_12470_verts.inc"
};

SVECTOR Actor00100_D12074[31] = {
#include "assets/actor_400100_model_12470_normals.inc"
};

u32 Actor00100_D1216C[193] = {
#include "assets/actor_400100_model_12470_stream.inc"
};

TmdSource Actor00100_D12470 = {
    0,
    1248,
    0,
    1,
    Actor00100_D11FD8,
    Actor00100_D11FDC,
    Actor00100_D12074,
    Actor00100_D11FB4,
    Actor00100_D1216C,
};

AnimationPackedPose Actor00100_D12494[10] = {
#include "assets/actor_400100_animation_129A4_bank1.inc"
};

AnimationPackedRotation Actor00100_D1250C[110] = {
#include "assets/actor_400100_animation_129A4_bank4.inc"
};

AnimationRecord Actor00100_D126C4[175] = {
#include "assets/actor_400100_animation_129A4_records.inc"
};

u16 Actor00100_D12980[18] = {
#include "assets/actor_400100_animation_129A4_indices.inc"
};

AnimationSet Actor00100_D129A4 = {
    Actor00100_D126C4,
    Actor00100_D12980,
    { NULL, Actor00100_D12494, NULL, NULL, Actor00100_D1250C, NULL, NULL, NULL },
};

AnimationPackedPose Actor00100_D129CC[5] = {
#include "assets/actor_400100_animation_12C60_bank1.inc"
};

AnimationPackedRotation Actor00100_D12A08[29] = {
#include "assets/actor_400100_animation_12C60_bank4.inc"
};

AnimationRecord Actor00100_D12A7C[112] = {
#include "assets/actor_400100_animation_12C60_records.inc"
};

u16 Actor00100_D12C3C[18] = {
#include "assets/actor_400100_animation_12C60_indices.inc"
};

AnimationSet Actor00100_D12C60 = {
    Actor00100_D12A7C,
    Actor00100_D12C3C,
    { NULL, Actor00100_D129CC, NULL, NULL, Actor00100_D12A08, NULL, NULL, NULL },
};

AnimationPackedPose Actor00100_D12C88[13] = {
#include "assets/actor_400100_animation_13294_bank1.inc"
};

AnimationPackedRotation Actor00100_D12D24[129] = {
#include "assets/actor_400100_animation_13294_bank4.inc"
};

AnimationRecord Actor00100_D12F28[210] = {
#include "assets/actor_400100_animation_13294_records.inc"
};

u16 Actor00100_D13270[18] = {
#include "assets/actor_400100_animation_13294_indices.inc"
};

AnimationSet Actor00100_D13294 = {
    Actor00100_D12F28,
    Actor00100_D13270,
    { NULL, Actor00100_D12C88, NULL, NULL, Actor00100_D12D24, NULL, NULL, NULL },
};

AnimationPackedPose Actor00100_D132BC[11] = {
#include "assets/actor_400100_animation_1386C_bank1.inc"
};

AnimationPackedRotation Actor00100_D13340[131] = {
#include "assets/actor_400100_animation_1386C_bank4.inc"
};

AnimationRecord Actor00100_D1354C[191] = {
#include "assets/actor_400100_animation_1386C_records.inc"
};

u16 Actor00100_D13848[18] = {
#include "assets/actor_400100_animation_1386C_indices.inc"
};

AnimationSet Actor00100_D1386C = {
    Actor00100_D1354C,
    Actor00100_D13848,
    { NULL, Actor00100_D132BC, NULL, NULL, Actor00100_D13340, NULL, NULL, NULL },
};

AnimationPackedPose Actor00100_D13894[9] = {
#include "assets/actor_400100_animation_13CA0_bank1.inc"
};

AnimationPackedRotation Actor00100_D13900[83] = {
#include "assets/actor_400100_animation_13CA0_bank4.inc"
};

AnimationRecord Actor00100_D13A4C[140] = {
#include "assets/actor_400100_animation_13CA0_records.inc"
};

u16 Actor00100_D13C7C[18] = {
#include "assets/actor_400100_animation_13CA0_indices.inc"
};

AnimationSet Actor00100_D13CA0 = {
    Actor00100_D13A4C,
    Actor00100_D13C7C,
    { NULL, Actor00100_D13894, NULL, NULL, Actor00100_D13900, NULL, NULL, NULL },
};

AnimationPackedPose Actor00100_D13CC8[19] = {
#include "assets/actor_400100_animation_14378_bank1.inc"
};

AnimationPackedRotation Actor00100_D13DAC[138] = {
#include "assets/actor_400100_animation_14378_bank4.inc"
};

AnimationRecord Actor00100_D13FD4[224] = {
#include "assets/actor_400100_animation_14378_records.inc"
};

u16 Actor00100_D14354[18] = {
#include "assets/actor_400100_animation_14378_indices.inc"
};

AnimationSet Actor00100_D14378 = {
    Actor00100_D13FD4,
    Actor00100_D14354,
    { NULL, Actor00100_D13CC8, NULL, NULL, Actor00100_D13DAC, NULL, NULL, NULL },
};

AnimationPackedPose Actor00100_D143A0[14] = {
#include "assets/actor_400100_animation_14978_bank1.inc"
};

AnimationPackedRotation Actor00100_D14448[99] = {
#include "assets/actor_400100_animation_14978_bank4.inc"
};

AnimationRecord Actor00100_D145D4[224] = {
#include "assets/actor_400100_animation_14978_records.inc"
};

u16 Actor00100_D14954[18] = {
#include "assets/actor_400100_animation_14978_indices.inc"
};

AnimationSet Actor00100_D14978 = {
    Actor00100_D145D4,
    Actor00100_D14954,
    { NULL, Actor00100_D143A0, NULL, NULL, Actor00100_D14448, NULL, NULL, NULL },
};

AnimationPackedPose Actor00100_D149A0[4] = {
#include "assets/actor_400100_animation_14BD4_bank1.inc"
};

AnimationPackedRotation Actor00100_D149D0[34] = {
#include "assets/actor_400100_animation_14BD4_bank4.inc"
};

AnimationRecord Actor00100_D14A58[86] = {
#include "assets/actor_400100_animation_14BD4_records.inc"
};

u16 Actor00100_D14BB0[18] = {
#include "assets/actor_400100_animation_14BD4_indices.inc"
};

AnimationSet Actor00100_D14BD4 = {
    Actor00100_D14A58,
    Actor00100_D14BB0,
    { NULL, Actor00100_D149A0, NULL, NULL, Actor00100_D149D0, NULL, NULL, NULL },
};

AnimationPackedPose Actor00100_D14BFC[12] = {
#include "assets/actor_400100_animation_151F8_bank1.inc"
};

AnimationPackedRotation Actor00100_D14C8C[147] = {
#include "assets/actor_400100_animation_151F8_bank4.inc"
};

AnimationRecord Actor00100_D14ED8[191] = {
#include "assets/actor_400100_animation_151F8_records.inc"
};

u16 Actor00100_D151D4[18] = {
#include "assets/actor_400100_animation_151F8_indices.inc"
};

AnimationSet Actor00100_D151F8 = {
    Actor00100_D14ED8,
    Actor00100_D151D4,
    { NULL, Actor00100_D14BFC, NULL, NULL, Actor00100_D14C8C, NULL, NULL, NULL },
};

AnimationPackedPose Actor00100_D15220[9] = {
#include "assets/actor_400100_animation_15648_bank1.inc"
};

AnimationPackedRotation Actor00100_D1528C[73] = {
#include "assets/actor_400100_animation_15648_bank4.inc"
};

AnimationRecord Actor00100_D153B0[157] = {
#include "assets/actor_400100_animation_15648_records.inc"
};

u16 Actor00100_D15624[18] = {
#include "assets/actor_400100_animation_15648_indices.inc"
};

AnimationSet Actor00100_D15648 = {
    Actor00100_D153B0,
    Actor00100_D15624,
    { NULL, Actor00100_D15220, NULL, NULL, Actor00100_D1528C, NULL, NULL, NULL },
};

AnimationPackedPose Actor00100_D15670[12] = {
#include "assets/actor_400100_animation_15C2C_bank1.inc"
};

AnimationPackedRotation Actor00100_D15700[126] = {
#include "assets/actor_400100_animation_15C2C_bank4.inc"
};

AnimationRecord Actor00100_D158F8[196] = {
#include "assets/actor_400100_animation_15C2C_records.inc"
};

u16 Actor00100_D15C08[18] = {
#include "assets/actor_400100_animation_15C2C_indices.inc"
};

AnimationSet Actor00100_D15C2C = {
    Actor00100_D158F8,
    Actor00100_D15C08,
    { NULL, Actor00100_D15670, NULL, NULL, Actor00100_D15700, NULL, NULL, NULL },
};

AnimationPackedPose Actor00100_D15C54[2] = {
#include "assets/actor_400100_animation_15DA8_bank1.inc"
};

AnimationPackedRotation Actor00100_D15C6C[16] = {
#include "assets/actor_400100_animation_15DA8_bank4.inc"
};

AnimationRecord Actor00100_D15CAC[54] = {
#include "assets/actor_400100_animation_15DA8_records.inc"
};

u16 Actor00100_D15D84[18] = {
#include "assets/actor_400100_animation_15DA8_indices.inc"
};

AnimationSet Actor00100_D15DA8 = {
    Actor00100_D15CAC,
    Actor00100_D15D84,
    { NULL, Actor00100_D15C54, NULL, NULL, Actor00100_D15C6C, NULL, NULL, NULL },
};

AnimationPackedPose Actor00100_D15DD0[13] = {
#include "assets/actor_400100_animation_1644C_bank1.inc"
};

AnimationPackedRotation Actor00100_D15E6C[158] = {
#include "assets/actor_400100_animation_1644C_bank4.inc"
};

AnimationRecord Actor00100_D160E4[209] = {
#include "assets/actor_400100_animation_1644C_records.inc"
};

u16 Actor00100_D16428[18] = {
#include "assets/actor_400100_animation_1644C_indices.inc"
};

AnimationSet Actor00100_D1644C = {
    Actor00100_D160E4,
    Actor00100_D16428,
    { NULL, Actor00100_D15DD0, NULL, NULL, Actor00100_D15E6C, NULL, NULL, NULL },
};

AnimationPackedPose Actor00100_D16474[30] = {
#include "assets/actor_400100_animation_16E64_bank1.inc"
};

AnimationPackedRotation Actor00100_D165DC[221] = {
#include "assets/actor_400100_animation_16E64_bank4.inc"
};

AnimationRecord Actor00100_D16950[316] = {
#include "assets/actor_400100_animation_16E64_records.inc"
};

u16 Actor00100_D16E40[18] = {
#include "assets/actor_400100_animation_16E64_indices.inc"
};

AnimationSet Actor00100_D16E64 = {
    Actor00100_D16950,
    Actor00100_D16E40,
    { NULL, Actor00100_D16474, NULL, NULL, Actor00100_D165DC, NULL, NULL, NULL },
};

AnimationPackedPose Actor00100_D16E8C[12] = {
#include "assets/actor_400100_animation_17284_bank1.inc"
};

AnimationPackedRotation Actor00100_D16F1C[77] = {
#include "assets/actor_400100_animation_17284_bank4.inc"
};

AnimationRecord Actor00100_D17050[132] = {
#include "assets/actor_400100_animation_17284_records.inc"
};

u16 Actor00100_D17260[18] = {
#include "assets/actor_400100_animation_17284_indices.inc"
};

AnimationSet Actor00100_D17284 = {
    Actor00100_D17050,
    Actor00100_D17260,
    { NULL, Actor00100_D16E8C, NULL, NULL, Actor00100_D16F1C, NULL, NULL, NULL },
};

AnimationPackedPose Actor00100_D172AC[4] = {
#include "assets/actor_400100_animation_17548_bank1.inc"
};

AnimationPackedRotation Actor00100_D172DC[45] = {
#include "assets/actor_400100_animation_17548_bank4.inc"
};

AnimationRecord Actor00100_D17390[101] = {
#include "assets/actor_400100_animation_17548_records.inc"
};

u16 Actor00100_D17524[18] = {
#include "assets/actor_400100_animation_17548_indices.inc"
};

AnimationSet Actor00100_D17548 = {
    Actor00100_D17390,
    Actor00100_D17524,
    { NULL, Actor00100_D172AC, NULL, NULL, Actor00100_D172DC, NULL, NULL, NULL },
};

AnimationPackedPose Actor00100_D17570[9] = {
#include "assets/actor_400100_animation_17A4C_bank1.inc"
};

AnimationPackedRotation Actor00100_D175DC[123] = {
#include "assets/actor_400100_animation_17A4C_bank4.inc"
};

AnimationRecord Actor00100_D177C8[152] = {
#include "assets/actor_400100_animation_17A4C_records.inc"
};

u16 Actor00100_D17A28[18] = {
#include "assets/actor_400100_animation_17A4C_indices.inc"
};

AnimationSet Actor00100_D17A4C = {
    Actor00100_D177C8,
    Actor00100_D17A28,
    { NULL, Actor00100_D17570, NULL, NULL, Actor00100_D175DC, NULL, NULL, NULL },
};

AnimationPackedPose Actor00100_D17A74[13] = {
#include "assets/actor_400100_animation_17FCC_bank1.inc"
};

AnimationPackedRotation Actor00100_D17B10[113] = {
#include "assets/actor_400100_animation_17FCC_bank4.inc"
};

AnimationRecord Actor00100_D17CD4[181] = {
#include "assets/actor_400100_animation_17FCC_records.inc"
};

u16 Actor00100_D17FA8[18] = {
#include "assets/actor_400100_animation_17FCC_indices.inc"
};

AnimationSet Actor00100_D17FCC = {
    Actor00100_D17CD4,
    Actor00100_D17FA8,
    { NULL, Actor00100_D17A74, NULL, NULL, Actor00100_D17B10, NULL, NULL, NULL },
};

AnimationPackedPose Actor00100_D17FF4[14] = {
#include "assets/actor_400100_animation_1857C_bank1.inc"
};

AnimationPackedRotation Actor00100_D1809C[116] = {
#include "assets/actor_400100_animation_1857C_bank4.inc"
};

AnimationRecord Actor00100_D1826C[187] = {
#include "assets/actor_400100_animation_1857C_records.inc"
};

u16 Actor00100_D18558[18] = {
#include "assets/actor_400100_animation_1857C_indices.inc"
};

AnimationSet Actor00100_D1857C = {
    Actor00100_D1826C,
    Actor00100_D18558,
    { NULL, Actor00100_D17FF4, NULL, NULL, Actor00100_D1809C, NULL, NULL, NULL },
};

AnimationPackedPose Actor00100_D185A4[13] = {
#include "assets/actor_400100_animation_18C3C_bank1.inc"
};

AnimationPackedRotation Actor00100_D18640[160] = {
#include "assets/actor_400100_animation_18C3C_bank4.inc"
};

AnimationRecord Actor00100_D188C0[214] = {
#include "assets/actor_400100_animation_18C3C_records.inc"
};

u16 Actor00100_D18C18[18] = {
#include "assets/actor_400100_animation_18C3C_indices.inc"
};

AnimationSet Actor00100_D18C3C = {
    Actor00100_D188C0,
    Actor00100_D18C18,
    { NULL, Actor00100_D185A4, NULL, NULL, Actor00100_D18640, NULL, NULL, NULL },
};

AnimationPackedPose Actor00100_D18C64[8] = {
#include "assets/actor_400100_animation_190D0_bank1.inc"
};

AnimationPackedRotation Actor00100_D18CC4[100] = {
#include "assets/actor_400100_animation_190D0_bank4.inc"
};

AnimationRecord Actor00100_D18E54[150] = {
#include "assets/actor_400100_animation_190D0_records.inc"
};

u16 Actor00100_D190AC[18] = {
#include "assets/actor_400100_animation_190D0_indices.inc"
};

AnimationSet Actor00100_D190D0 = {
    Actor00100_D18E54,
    Actor00100_D190AC,
    { NULL, Actor00100_D18C64, NULL, NULL, Actor00100_D18CC4, NULL, NULL, NULL },
};

AnimationPackedPose Actor00100_D190F8[21] = {
#include "assets/actor_400100_animation_19918_bank1.inc"
};

AnimationPackedRotation Actor00100_D191F4[193] = {
#include "assets/actor_400100_animation_19918_bank4.inc"
};

AnimationRecord Actor00100_D194F8[254] = {
#include "assets/actor_400100_animation_19918_records.inc"
};

u16 Actor00100_D198F0[20] = {
#include "assets/actor_400100_animation_19918_indices.inc"
};

AnimationSet Actor00100_D19918 = {
    Actor00100_D194F8,
    Actor00100_D198F0,
    { NULL, Actor00100_D190F8, NULL, NULL, Actor00100_D191F4, NULL, NULL, NULL },
};

AnimationPackedPose Actor00100_D19940[20] = {
#include "assets/actor_400100_animation_1A0B4_bank1.inc"
};

AnimationPackedRotation Actor00100_D19A30[172] = {
#include "assets/actor_400100_animation_1A0B4_bank4.inc"
};

AnimationRecord Actor00100_D19CE0[235] = {
#include "assets/actor_400100_animation_1A0B4_records.inc"
};

u16 Actor00100_D1A08C[20] = {
#include "assets/actor_400100_animation_1A0B4_indices.inc"
};

AnimationSet Actor00100_D1A0B4 = {
    Actor00100_D19CE0,
    Actor00100_D1A08C,
    { NULL, Actor00100_D19940, NULL, NULL, Actor00100_D19A30, NULL, NULL, NULL },
};

AnimationPackedPose Actor00100_D1A0DC[15] = {
#include "assets/actor_400100_animation_1A8C0_bank1.inc"
};

AnimationPackedRotation Actor00100_D1A190[206] = {
#include "assets/actor_400100_animation_1A8C0_bank4.inc"
};

AnimationRecord Actor00100_D1A4C8[244] = {
#include "assets/actor_400100_animation_1A8C0_records.inc"
};

u16 Actor00100_D1A898[20] = {
#include "assets/actor_400100_animation_1A8C0_indices.inc"
};

AnimationSet Actor00100_D1A8C0 = {
    Actor00100_D1A4C8,
    Actor00100_D1A898,
    { NULL, Actor00100_D1A0DC, NULL, NULL, Actor00100_D1A190, NULL, NULL, NULL },
};

AnimationPackedPose Actor00100_D1A8E8[14] = {
#include "assets/actor_400100_animation_1B0D4_bank1.inc"
};

AnimationPackedRotation Actor00100_D1A990[207] = {
#include "assets/actor_400100_animation_1B0D4_bank4.inc"
};

AnimationRecord Actor00100_D1ACCC[248] = {
#include "assets/actor_400100_animation_1B0D4_records.inc"
};

u16 Actor00100_D1B0AC[20] = {
#include "assets/actor_400100_animation_1B0D4_indices.inc"
};

AnimationSet Actor00100_D1B0D4 = {
    Actor00100_D1ACCC,
    Actor00100_D1B0AC,
    { NULL, Actor00100_D1A8E8, NULL, NULL, Actor00100_D1A990, NULL, NULL, NULL },
};

AnimationPackedPose Actor00100_D1B0FC[5] = {
#include "assets/actor_400100_animation_1B388_bank1.inc"
};

AnimationPackedRotation Actor00100_D1B138[55] = {
#include "assets/actor_400100_animation_1B388_bank4.inc"
};

AnimationRecord Actor00100_D1B214[83] = {
#include "assets/actor_400100_animation_1B388_records.inc"
};

u16 Actor00100_D1B360[20] = {
#include "assets/actor_400100_animation_1B388_indices.inc"
};

AnimationSet Actor00100_D1B388 = {
    Actor00100_D1B214,
    Actor00100_D1B360,
    { NULL, Actor00100_D1B0FC, NULL, NULL, Actor00100_D1B138, NULL, NULL, NULL },
};

AnimationPackedPose Actor00100_D1B3B0[6] = {
#include "assets/actor_400100_animation_1B6A8_bank1.inc"
};

AnimationPackedRotation Actor00100_D1B3F8[66] = {
#include "assets/actor_400100_animation_1B6A8_bank4.inc"
};

AnimationRecord Actor00100_D1B500[96] = {
#include "assets/actor_400100_animation_1B6A8_records.inc"
};

u16 Actor00100_D1B680[20] = {
#include "assets/actor_400100_animation_1B6A8_indices.inc"

};

AnimationSet Actor00100_D1B6A8 = {
    Actor00100_D1B500,
    Actor00100_D1B680,
    { NULL, Actor00100_D1B3B0, NULL, NULL, Actor00100_D1B3F8, NULL, NULL, NULL },
};

/// Blend duration in frames, indexed by [previous animation][next animation].
s8 Actor00100_D1B6D0[25][25] = {
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
    &Actor00100_D129A4,
    &Actor00100_D12C60,
    &Actor00100_D13294,
    &Actor00100_D1386C,
    &Actor00100_D13CA0,
    &Actor00100_D14378,
    &Actor00100_D14978,
    &Actor00100_D14BD4,
    &Actor00100_D151F8,
    &Actor00100_D15648,
    &Actor00100_D15C2C,
    &Actor00100_D15DA8,
    &Actor00100_D1644C,
    &Actor00100_D16E64,
    &Actor00100_D17284,
    &Actor00100_D17548,
    &Actor00100_D17A4C,
    &Actor00100_D17FCC,
    &Actor00100_D1857C,
    &Actor00100_D18C3C,
    &Actor00100_D190D0,
    &Actor00100_D15C2C,
    NULL,
    NULL,
    NULL,
    NULL,
};

DesertChaserAnimCommand gDesertChaserFrontAnim = { { NULL, &Actor00100_D19918, &Actor00100_D1A8C0, &Actor00100_D1B388, NULL, NULL, NULL, NULL, NULL } };

DesertChaserAnimCommand gDesertChaserRearAnim = { { NULL, &Actor00100_D1A0B4, &Actor00100_D1B0D4, &Actor00100_D1B6A8, NULL, NULL, NULL, NULL, NULL } };

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

Actor00100MessageEntry Actor00100_D1BA54[6] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, { .command = Actor00100_Fn00E58 } },
    { 2015, { .reset = Actor00100_Fn0B134 } },
    { 2005, { .value = Actor00100_Fn0B1A4 } },
    { 2006, { .task = actorMsgIsPresent } },
    { 2004, { .placement = actorMsgPlaceRecordYaw } },
    { TASK_MESSAGE_TABLE_END, { .command = NULL } },
};

TaskDesc Actor00100_D1BA84 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, Actor00100_Fn0BD28, { .model = &Actor00100_D108C0 } };

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

/// Turns joint `coord` by `yaw` about the world Y axis: builds its world
/// rotation in a matrix carved off the scratchpad head, applies the turn,
/// converts the result back into the parent's frame, writes the 3x3 into the
/// joint and refreshes it.
static void Actor00100_Fn001FC(GfxCoord* coord, s16 yaw)
{
    MATRIX*   rotation;
    GfxCoord* out;

    SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    rotation = SCRATCH_STACK_CURSOR(MATRIX);
    actorAccumulateRotation(coord, rotation, &gGfxViewCoord);
    RotMatrixY(yaw, rotation);
    out = actorLocalizeRotation(coord, rotation);
    memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(out);
    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
}

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
s32 Actor00100_Fn00E58(Task* arg0, s32 arg1, ActorCommand* request)
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
        work->actorId.bytes[0] = request->context.loc.stage;
        work->actorId.bytes[1] = request->context.loc.area;
        work->actorId.bytes[2] = (u8)request->command;
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
                    desertChaserArmedAnimTick(arg0);
                    desertChaserArmedAnimTick(arg0);
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
            Gfx_MatrixCol2(&s->m, &s->dir);
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

static void Actor00100_Fn01D74(Task* arg0)
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
            Gp_AnimTickIndex(&((DesertChaserWork*)work)->anim, (s32)index);
        }
    }
}

static s32 Actor00100_Fn01EEC(Task* arg0, DesertChaserWork* arg1)
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
                        Gp_SpawnEff(0x60054, &arg0->extra.tmd->coords[17], 0x80002280, &work->field_898);
                    }
                    work               = arg0->work;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(0x60054, &arg0->extra.tmd->coords[9], 0x80002120, &work->field_898);
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
                        Gp_SpawnEff(0x60054, &arg0->extra.tmd->coords[14], 0x80002220, &work->field_898);
                    }
                    work               = arg0->work;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(0x60054, &arg0->extra.tmd->coords[7], 0x80002120, &work->field_898);
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
                        Gp_SpawnEff(0x60054, &arg0->extra.tmd->coords[0], 0x80004A00, &work->field_898);
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
                        Gp_SpawnEff(0x60054, &arg0->extra.tmd->coords[9], 0x80003200, &work->field_898);
                    }
                    work               = arg0->work;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(0x60054, &arg0->extra.tmd->coords[7], 0x80003200, &work->field_898);
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
                        Gp_SpawnEff(0x60054, &arg0->extra.tmd->coords[17], 0x80004480, &work->field_898);
                    }
                    work               = arg0->work;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(0x60054, &arg0->extra.tmd->coords[14], 0x80004480, &work->field_898);
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
                        Gp_SpawnEff(0x60054, &arg0->extra.tmd->coords[9], 0x80003200, &work->field_898);
                    }
                    work               = arg0->work;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(0x60054, &arg0->extra.tmd->coords[7], 0x80003200, &work->field_898);
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
                        Gp_SpawnEff(0x60054, &arg0->extra.tmd->coords[9], 0x80003200, &work->field_898);
                    }
                    work               = arg0->work;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(0x60054, &arg0->extra.tmd->coords[17], 0x80003200, &work->field_898);
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
                        Gp_SpawnEff(0x60054, &arg0->extra.tmd->coords[14], 0x80003200, &work->field_898);
                    }
                    work               = arg0->work;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(0x60054, &arg0->extra.tmd->coords[17], 0x80003200, &work->field_898);
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
                        Gp_SpawnEff(0x60054, &arg0->extra.tmd->coords[9], 0x80003200, &work->field_898);
                    }
                    work               = arg0->work;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(0x60054, &arg0->extra.tmd->coords[7], 0x80003200, &work->field_898);
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
                        Gp_SpawnEff(0x60054, &arg0->extra.tmd->coords[7], 0x80003200, &work->field_898);
                    }
                    work               = arg0->work;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(0x60054, &arg0->extra.tmd->coords[14], 0x80003200, &work->field_898);
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
                        Gp_SpawnEff(0x60054, &arg0->extra.tmd->coords[14], 0x80003200, &work->field_898);
                    }
                    work               = arg0->work;
                    work->field_898.vz = 0;
                    work->field_898.vx = 0;
                    work->field_898.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(0x60054, &arg0->extra.tmd->coords[17], 0x80003200, &work->field_898);
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
        Mem_Set(arg1->field_848, 0U, 0x48U);
    }
    return 0;
}

static void desertChaserArmedAnimTick(Task* arg0)
{
    s32               index;
    u32               table;
    DesertChaserWork* seekWork;
    DesertChaserWork* resetWork;
    DesertChaserWork* turnWork;
    DesertChaserWork* secondaryWork;
    DesertChaserWork* tickWork;
    DesertChaserWork* work;
    s32               targetAngle;
    s32               animation;
    s32               updatedTurn;
    s16               currentTurn;
    s16               thirdAngle;
    s16               state;
    s32               currentAngle;
    s16               angle;
    s32               seekSlotIndex;
    s32               resetSlotIndex;
    s32               secondarySlotIndex;
    s32               tickSlotIndex;
    s32               signedTurn;
    s32               soundId;
    s32               sound;
    s32               resetIndex;
    s32               secondaryIndex;
    s32               tickIndex;
    s32               seekIndex;
    s32               delta;
    s8*               tickSlot;
    s8*               seekSlot;
    s8*               resetSlot;
    s8*               secondarySlot;
    s32               pan;
    s32               currentAngleBits;
    u16               originalTurn;
    s32               targetAngleBits;
    u16               updatedTurnBits;
    s32               clampedAngle;
    s32               targetTurn;

    work  = arg0->work;
    state = (s16)work->field_828;
    if (state == 1) {
        if (work->field_82C != work->field_82E) {
            seekWork  = work;
            seekIndex = 1;
            table     = (u32)&Actor00100_D1B6D0;
            seekSlot  = (s8*)&work->anim.slots;
            do {
                seekSlotIndex  = seekIndex;
                seekSlot[0x39] = (u8)seekWork->field_832;
                animation      = seekWork->field_82E;
                seekSlot      += sizeof(AnimationSlot);
                index          = seekWork->field_82C * 0x19;
                func_800B4114(&seekWork->anim, seekSlotIndex, (s16)(animation), 0, (s32) * (s8*)((animation + index) + table));
                seekIndex += 1;
            } while (seekIndex < 0x12);
            seekWork->field_82C = seekWork->field_82E;
        }
        work->field_828 = 3;
        work->field_830 = 0;
        Mem_Set(work->field_848, 0U, 0x48U);
    } else if (state == 2) {
        resetWork  = work;
        resetIndex = 1;
        resetSlot  = (s8*)&work->anim.slots;
        do {
            resetSlotIndex  = resetIndex;
            resetSlot[0x39] = (u8)resetWork->field_832;
            resetSlot      += sizeof(AnimationSlot);
            Gp_AnimResetSlot(&resetWork->anim, resetSlotIndex, (s32)resetWork->field_82E);
            resetIndex += 1;
        } while (resetIndex < 0x12);
        resetWork->field_82C = resetWork->field_82E;
        work->field_828      = 3;
        work->field_830      = 0U;
        Mem_Set(work->field_848, 0U, 0x48U);
    }
    if (work->field_836 == 2) {
        secondaryWork  = arg0->work;
        secondaryIndex = 1;
        secondarySlot  = (s8*)&secondaryWork->anim.slots;
        do {
            secondarySlotIndex  = secondaryIndex;
            secondarySlot[0x39] = (u8)secondaryWork->field_83A;
            secondarySlot      += sizeof(AnimationSlot);
            Gp_AnimResetSlot(&secondaryWork->blendAnim, secondarySlotIndex, (s32)secondaryWork->field_838);
            secondaryIndex += 1;
        } while (secondaryIndex < 0x12);
        work->field_836 = 3;
    }
    work->field_830 = (u16)(work->field_830 + 1);
    if (work->field_82A == 0) {
        tickWork  = arg0->work;
        tickIndex = 1;
        tickSlot  = (s8*)&tickWork->anim.slots;
        do {
            tickSlotIndex  = tickIndex;
            tickSlot[0x39] = (u8)tickWork->field_832;
            Gp_AnimTickIndex(&tickWork->anim, tickSlotIndex);
            tickSlot  += sizeof(AnimationSlot);
            tickIndex += 1;
        } while (tickIndex < 0x12);
    } else {
        Actor00100_Fn01D74(arg0);
        if (work->blendSlots[1].flags & 0x100) {
            work->field_82A = 0;
        }
    }
    targetAngle      = (s16)work->field_840;
    currentAngle     = (s16)work->field_844;
    targetAngleBits  = work->field_840;
    currentAngleBits = work->field_844;
    if (currentAngle < targetAngle) {
        if ((targetAngle - currentAngle) >= 0x72) {
            work->field_844 = currentAngleBits + 0x71;
        } else {
            goto block_26;
        }
    } else if ((currentAngle - targetAngle) >= 0x72) {
        work->field_844 = currentAngleBits - 0x71;
    } else {
    block_26:
        work->field_844 = targetAngleBits;
    }
    angle        = (s16)work->field_844;
    clampedAngle = work->field_844;
    if (angle != 0) {
        if (angle >= 0x501) {
            clampedAngle = 0x500;
        }
        if (angle < -0x500) {
            clampedAngle = -0x500;
        }
        thirdAngle = (s16)clampedAngle / 3;
        Actor00100_Fn001FC(&arg0->extra.tmd->coords[2], thirdAngle);
        arg0->extra.tmd->coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
        Actor00100_Fn001FC(&arg0->extra.tmd->coords[3], thirdAngle);
        arg0->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
        Actor00100_Fn001FC(&arg0->extra.tmd->coords[4], (s16)clampedAngle / 2);
        arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if ((work->field_82E == 0) && (work->field_0 == 0x26)) {
        Gfx_RotMatrixX(&arg0->extra.tmd->coords[4].coord, 0x280, 0);
        arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&arg0->extra.tmd->coords[4]);
    }
    turnWork     = arg0->work;
    targetTurn   = turnWork->field_83E;
    originalTurn = targetTurn;
    if ((s16)targetTurn >= 0x201) {
        targetTurn = 0x200;
    }
    if ((s16)originalTurn < -0x200) {
        targetTurn = -0x200;
    }
    signedTurn  = (s16)targetTurn;
    currentTurn = turnWork->field_842;
    if (currentTurn < signedTurn) {
        if ((signedTurn - currentTurn) >= 0xD) {
            turnWork->field_842 = (s16)((u16)turnWork->field_842 + 0xC);
        } else {
            turnWork->field_842 = (s16)targetTurn;
        }
    }
    updatedTurn     = turnWork->field_842;
    updatedTurnBits = (u16)turnWork->field_842;
    if ((s16)targetTurn < updatedTurn) {
        delta = updatedTurn - (s16)targetTurn;
        if (delta < 0) {
            delta = -delta;
        }
        if (delta >= 0xD) {
            turnWork->field_842 = (s16)(updatedTurnBits - 0xC);
        } else {
            turnWork->field_842 = (s16)targetTurn;
        }
    }
    Actor00100_Fn001FC(&arg0->extra.tmd->coords[10], (s16)((s32)(u16)turnWork->field_842 * -1));
    arg0->extra.tmd->coords[10].composeStamp = GRAPHICS_COORD_DIRTY;
    sound                                    = Actor00100_Fn01EEC(arg0, work);
    if (sound != 0) {
        soundId = sound | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, (s32)pan, (s32)(s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
}

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
        Gp_DestroyEnemy(arg0, arg1);
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
    func_800B3F84(&work->anim, Actor00100_D1B944, tmd, work->poses, work->slots);
    func_800B3F84(&work->blendAnim, Actor00100_D1B944, tmd, work->blendPoses, work->blendSlots);
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
    desertChaserArmedAnimTick(arg1);
    work->objs[2].obj.coord            = coord;
    work->objs[2].obj.context.contacts = work->objs[2].contacts;
    work->objs[2].obj.pos.vx           = 0;
    work->objs[2].obj.pos.vy           = -0x11C;
    work->objs[2].obj.pos.vz           = 0;
    work->objs[2].obj.key              = 0x30001;
    work->objs[2].obj.radius           = 0x12C;
    work->objs[2].obj.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->objs[2].obj);
    work->capsuleBody.shape.ends[0].vx    = 0;
    work->capsuleBody.shape.ends[0].vy    = -0x180;
    work->capsuleBody.shape.ends[0].vz    = 0;
    work->capsuleBody.shape.ends[1].vx    = 0;
    work->capsuleBody.shape.ends[1].vy    = -0x180;
    work->capsuleBody.shape.ends[1].vz    = 0x2BC;
    work->capsuleBody.shape.end0Radius    = 0x12C;
    work->capsuleBody.shape.end1Radius    = 0x12C;
    work->capsuleBody.shape.contacts      = work->capsuleBody.contacts;
    work->capsuleBody.obj.coord           = coord;
    work->capsuleBody.obj.context.capsule = &work->capsuleBody.shape;
    work->capsuleBody.obj.pos.vx          = 0;
    work->capsuleBody.obj.pos.vy          = 0;
    work->capsuleBody.obj.pos.vz          = 0;
    work->capsuleBody.obj.key             = 0x30001;
    work->capsuleBody.obj.radius          = 1;
    work->capsuleBody.obj.flags           = WORLD_COLLISION_BODY_CAPSULE;
    work->objs[2].obj.flags              |= WORLD_COLLISION_BODY_GRID_ENABLED;
    Gp_LinkObj(2, &work->capsuleBody.obj);
    work->capsuleBody.obj.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
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
    Gfx_MatrixCol2(&arg1->extra.tmd->coords[0].coord, &vec);
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
            CdCmd_Enqueue(0x21, cmd38, cmd30);
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
                Gp_SpawnEff(0x6009C, arg0->extra.tmd->coords + 2, (s32)(effect), NULL);
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
        Gp_UnlinkObj(&work->capsuleBody.obj);
        Gp_UnlinkObj(&work->objs[2].obj);
        ctx->recs = 0;
    }
    if (work->field_6 >= 0x3D && work->reported == 0 && Gp_StateC08.field_A != 1 && gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 1, 0, 0)) {
            Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), 0x13F4, ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT, 0);
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
            Gp_SpawnEff(0x600A5, &gGfxViewCoord, 2, &work->field_8A8);
            work->field_8A8.vy = arg0->extra.tmd->coords[0].coord.t[1];
            actorLocalToView(&arg0->extra.tmd->coords[9], &work->field_8B0);
            work->field_8B0.vy = arg0->extra.tmd->coords[0].coord.t[1];
            Gp_SpawnEff(0x600A5, &gGfxViewCoord, 2, &work->field_8B0);
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
        desertChaserArmedAnimTick(arg0);
        return;
    }
    radius = 1000;
    desertChaserArmedAnimTick(arg0);
    Actor00100_PositionDelta(arg0->extra.tmd->coords, &delta);
    if (work->slots[1].flags & 0x100) {
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
        desertChaserArmedAnimTick(arg0);
        work->field_6                      = 0;
        work->field_8                      = 0;
        work->capsuleBody.shape.ends[1].vz = -0x2D0;
    }
    work->field_6 += 1;
    desertChaserArmedAnimTick(arg0);
    if (work->slots[1].flags & 0x100) {
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
        desertChaserArmedAnimTick(arg0);
        work->field_6 = 0;
    }
    ActorContact_PushContact(arg0->extra.tmd->coords, work->objs[2].contacts, 5);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    coord                                 = arg0->extra.tmd->coords;
    head[-1].x                            = (s16)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
    scratch->y                            = (s16)(gPlayerStatus.coordMtx->t[1] - coord->coord.t[1]);
    scratch->z                            = (s16)(gPlayerStatus.coordMtx->t[2] - coord->coord.t[2]);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    desertChaserArmedAnimTick(arg0);
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
        desertChaserArmedAnimTick(arg0);
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
    desertChaserArmedAnimTick(arg0);
    if (work->slots[1].flags & 0x100) {
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
        desertChaserArmedAnimTick(arg0);
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
    desertChaserArmedAnimTick(arg0);
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
        D_80114B34[5].data.model = &Actor00100_D10D60;
        vector.vz                = 0x64;
        vector.vy                = 0;
        vector.vx                = 0;
        effect                   = Gp_SpawnEff(0xA0005, &arg0->extra.tmd->coords[9], 0x200, &vector);
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
            D_80114B34[5].data.model = &Actor00100_D11234;
            vector.vy                = 0;
            vector.vx                = 0;
            effect2                  = Gp_SpawnEff(0xA0005, &arg0->extra.tmd->coords[12], 0x200, &vector);
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
        D_80114B34[5].data.model = &Actor00100_D12470;
        effect3                  = Gp_SpawnEff(0xA0005, &arg0->extra.tmd->coords[1], 0x200, NULL);
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
        D_80114B34[5].data.model = &Actor00100_D11F90;
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
        Gp_UnlinkObj(&work->capsuleBody.obj);
        Gp_UnlinkObj(&work->objs[2].obj);
        ctx->recs = 0;
    }
    if ((work->field_6 >= 0x1F) && (work->reported == 0) && (Gp_StateC08.field_A != 1) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 1, 0, 0)) {
            Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), 0x13F4, (s32)(ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT), 0);
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
    desertChaserArmedAnimTick(arg0);
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
            if (work->slots[1].flags & 0x100) {
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
    desertChaserArmedAnimTick(arg0);
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
                Gp_UnlinkObj(&work->capsuleBody.obj);
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
            if ((work->field_6 >= 0x79) && (work->reported != 1) && (Gp_StateC08.field_A != 1) && gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), 0x13F4, (s32)(ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT), 0);
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

    SVECTOR**                scratchHead;
    SVECTOR*                 scratch;
    s32                      state;
    s16                      modeState;
    s16                      height;
    s16                      modeHeight;
    s16                      initialState;
    s16                      finalState;
    s16                      i;
    GfxCoord*                playerCoord;
    s32                      sound;
    s32                      sound2;
    s32                      depth;
    s32                      result;
    s32                      action;
    s32                      nextAction;
    s32                      pan2;
    s32                      pan;
    DesertChaserAnimCommand* command2;
    DesertChaserWork*        actorWork;
    Task*                    playerSlot;
    DesertChaserWork*        work;
    Task*                    player;
    DesertChaserAnimCommand* command;
    Task*                    slot;
    GfxCoord*                coord;
    void*                    message;
    void*                    nextMessage;

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
            if ((slot != NULL) && (actorWork->poseId == 7)) {
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
                if (work->poseId == 0x38) {
                    TASK_MESSAGE_DISPATCH_POINTER(player, 0x3FE, (&work->routePos.vx), 0);
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
                            work->routePos.vy = (s32)(work->routePos.vy + 0x21);
                        } else {
                            coord->coord.t[1] = 0x1770;
                            work->routePos.vx = 0;
                            work->routePos.vy = 0;
                            work->routePos.vz = 0;
                        }
                    }
                    if (player->extra.tmd->coords->coord.t[1] >= 0x1770) {
                        if (config->hp > 0) {
                            for (i = 0; i < 10; i++) {
                                gGameSession->deathSoundCountdown = GAME_SESSION_DEATH_SOUND_HOLD;
                                playerSlot                        = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                                if (Gp_DispatchMsg(playerSlot, 0x3F9, Gp_PackObjPair(enemy, 4), 0) == 1)
                                    break;
                            }
                        }
                    }
                } else if (work->routePos.vx == 0) {
                    if (work->routePos.vz != 0) {
                        goto dispatchMotion;
                    }
                } else {
                dispatchMotion:
                    if ((s16)TASK_MESSAGE_DISPATCH_POINTER(player, 0x3FE, (&work->routePos.vx), 0) != 1) {
                        if (work->field_C28 >= 0xF) {
                            work->routePos.vx = (s32)((s32)work->routePos.vx >> 1);
                            work->routePos.vy = (s32)((s32)work->routePos.vy >> 1);
                            work->routePos.vz = (s32)((s32)work->routePos.vz >> 1);
                        }
                    } else if (((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 1, 0, 0)) && (work->routePos.vx != 0) && (work->routePos.vz != 0) && (work->field_C28 < 6)) {
                        if (Actor00100_InRegion(player)) {
                            if (Actor00100_InDirection(player, (VECTOR*)&work->routePos.vx)) {
                                work->poseId        = 0x38;
                                Gp_StateC08.field_6 = (u8)(Gp_StateC08.field_6 | 1);
                                Gp_PulseState1C();
                                scratch->vx = (u16)work->routePos.vx;
                                scratch->vy = 0;
                                scratch->vz = (u16)work->routePos.vz;
                                VectorNormalSS(scratch, scratch);
                                gte_lddp(250);
                                gte_ldsv(scratch);
                                gte_gpf12();
                                gte_stsv(scratch);
                                work->routePos.vx = (s32)(s16)scratch->vx;
                                work->routePos.vy = 0x3C;
                                work->routePos.vz = (s32)scratch->vz;
                            } else {
                                goto clearMotion;
                            }
                        } else {
                            goto clearMotion;
                        }
                    } else {
                    clearMotion:
                        work->routePos.vx = 0;
                        work->routePos.vy = 0;
                        work->routePos.vz = 0;
                    }
                }
                break;
            case 2:
                command = work->animCommand;
                if (command == &gDesertChaserRearAnim) {
                    if ((config->hp > 0) && (work->field_C28 >= 0x17)) {
                        message          = &work->animCommand;
                        command->sets[4] = Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon]->table.sets[7];
                        work->params[0]  = 4;
                        work->params[1]  = 1;
                        work->params[2]  = 3;
                        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, message, 0);
                        work->field_C28 = 0U;
                    }
                } else if ((config->hp > 0) && (work->field_C28 >= 0x22)) {
                    message                        = &work->animCommand;
                    gDesertChaserFrontAnim.sets[4] = Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon]->table.sets[7];
                    work->params[0]                = 4;
                    work->params[1]                = 1;
                    work->params[2]                = 3;
                    TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, message, 0);
                    work->field_C28 = 0U;
                }
                break;
            case 3:
                if ((work->field_C28 < 6) && (config->hp > 0) && ((work->routePos.vx != 0) || (work->routePos.vz != 0))) {
                    result = TASK_MESSAGE_DISPATCH_POINTER(player, 0x3FE, (&work->routePos.vx), 0);
                    if (result == 1) {
                        work->routePos.vx = 0;
                        work->routePos.vy = 0;
                        work->routePos.vz = 0;
                        work->poseBlend   = (s8)result;
                    }
                }
                break;
            case 5:
                if ((config->hp > 0) && (work->field_C28 >= 7)) {
                    Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3F1, 2, 0);
                    work->reported = 0;
                }
                break;
        }
        if (Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3ED, 0, 0) == 0) {
            nextAction = work->params[0];
            switch (nextAction) {
                case 1:
                    if ((((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(4, 1, 0, 0)) || (work->poseId != 0x38)) && (config->hp > 0)) {
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
                        command2        = work->animCommand;
                        if (command2 == &gDesertChaserRearAnim) {
                            command2->sets[5] = Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon]->table.sets[9];
                        } else {
                            gDesertChaserFrontAnim.sets[5] = Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon]->table.sets[9];
                        }
                        nextMessage = &work->animCommand;
                        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, nextMessage, 0);
                        work->field_C28 = 0U;
                    }
                    break;
                case 4:
                case 7:
                    if (config->hp > 0) {
                        Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3F1, 2, 0);
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
static const GpEnemyTaskFuncTable4 Actor00100_D001A0 = { {
    Actor00100_Fn02C54,
    Actor00100_Fn0A288,
    Actor00100_Fn0BCBC,
    Gp_DestroyEnemy,
} };

void Actor00100_Fn0B134(void)
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

s32 Actor00100_Fn0B1A4(Task* arg0, s32 arg1, s32 arg2)
{
    TmdObject*        obj  = arg0->extra.tmd;
    DesertChaserWork* work = arg0->work;

    switch (arg2) {
        case 0:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0;
            break;
        case 1:
            obj->flags = 0;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0x18;
            break;
        case 2:
            obj->flags   |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->field_0 = 0;
            break;
        case 3:
            obj->flags    = 0;
            work->field_0 = 0;
            obj->flags   |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
    }
    return 0;
}

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
        Gp_SpawnEff(0x60054, &arg0->extra.tmd->coords[arg1], arg2 | 0x80000000, &work->field_898);
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
        desertChaserArmedAnimTick(arg0);
        Gp_ArmStateF0(1);
    }
    ActorContact_PushContact(arg0->extra.tmd->coords, work->objs[2].contacts, 5);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    desertChaserArmedAnimTick(arg0);
    if (work->slots[1].flags & 0x100) {
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
        desertChaserArmedAnimTick(arg0);
    }
    desertChaserArmedAnimTick(arg0);
    if (work->slots[1].flags & 0x100) {
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
    desertChaserArmedAnimTick(arg0);
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
    desertChaserArmedAnimTick(arg0);
    if (work->slots[1].flags & 0x100) {
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
    if ((task != NULL) && (work->poseId == 7)) {
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

void Actor00100_Fn0BD28(Task* arg0)
{
    GpEnemyTaskFuncTable4 sp;

    sp = Actor00100_D001A0;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}
