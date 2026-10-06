#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/gtemac.h>
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

/// Scratch-stack block of the state that waits for the chaser to come into
/// view: the projection of the model root's origin, and the yaws between the
/// chaser and the player.
///
/// One block serves one frame. The chaser notices the player once `screen`
/// lies within 0x78 by 0x64 of the projection centre and `turn` is under
/// 0x200. Yaws are 4096 units per turn, and a wrapped one lies in
/// [-0x800, 0x800].
typedef struct {
    SVECTOR delta;           // The root's own origin while it is projected, then the player's position minus the root's
    DVECTOR screen;          // Screen position of the root's origin, relative to the projection centre
    s32     depthCue;        // GTE depth-cue coefficient of the projection; never read
    s32     projectionFlags; // GTE flag word of the projection; never read
    byte    unknown_14[4];   // Reserved with the block and never accessed; role unproven
    s32     orderingDepth;   // Quarter of the origin's view depth; never read
    s16     playerYaw;       // Heading the player faces; never read
    s16     yawFromPlayer;   // Bearing from the player to the chaser, wrapped; never read
    s16     turn;            // Wrapped turn from the chaser's heading to the player
    byte    unknown_22[2];   // Reserved with the block and never accessed; role unproven
} _Actor00100ScreenWatchScratch;
STATIC_ASSERT_SIZEOF(_Actor00100ScreenWatchScratch, 0x24);

/// A place the chaser enters the Mine Mesa at.
///
/// The position is the model root's translation in its parent's space; the
/// root is then turned about Y by `yaw` from the facing it had.
typedef struct {
    s16 x;   // X translation of the root
    s16 y;   // Y translation of the root
    s16 z;   // Z translation of the root
    s16 yaw; // Turn applied to the root's rotation, 4096 units per turn
} _Actor00100MesaPlacement;
STATIC_ASSERT_SIZEOF(_Actor00100MesaPlacement, 0x8);

/// The four places the Mine Mesa's command 1 picks from, by the view the
/// camera is on and a random draw.
///
/// The command handler copies the table to its frame before reading the entry
/// it picked.
typedef struct {
    _Actor00100MesaPlacement placements[4]; // 0 and 1 are the pair on one side of the Mesa, 2 and 3 the pair on the other
} _Actor00100MesaPlacements;
STATIC_ASSERT_SIZEOF(_Actor00100MesaPlacements, 0x20);

extern DesertChaserVariant Actor00100_D0BDB4[4];

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
    scratch->translation.vx = matrix->t[0];
    scratch->translation.vy = matrix->t[1];
    scratch->translation.vz = matrix->t[2];
    gte_lddp(amount);
    vec = &(head - 1)->translation;
    gte_ldsv(vec);
    gte_gpf12();
    gte_stsv(vec);
    matrix->t[0] = scratch->translation.vx;
    matrix->t[1] = scratch->translation.vy;
    matrix->t[2] = scratch->translation.vz;
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

DesertChaserVariant Actor00100_D0BDB4[4] = {
    { 60, 36, 10, 150 },
    { 40, 26, 10, 120 },
    { 20, 18, 10, 120 },
    { 60, 60, 10, 150 },
};

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
    s32 state = work->state;
    if (state == 4 || state == 7 || state == 0x21 || state == 0x14 || state == 0xB || state == 0x11 || (state == 0x24 && work->stateTimer < 10))
        work->state = 7;
    else
        work->state = 0x14;
}

// Transition durations indexed by previous * 25 + next animation.

#include "../../shared/actor_contacts_turn_joint.inc.c"

#include "../../shared/actor_contacts_steer.inc.c"

#include "../../shared/actor_contacts_push_contact.inc.c"

#include "../../shared/player_detection_sight.inc.c"

/// Where the Mine Mesa's command 1 puts the chaser: a pair of places at each
/// of two ends of the Mesa.
static const _Actor00100MesaPlacements Actor00100_D00004 = { {
    { 2247, 0, -7235, 0 },
    { 1247, 0, -6535, 256 },
    { 19000, 0, 4300, -800 },
    { 18700, 0, 5300, -1200 },
} };

/// Message handler for the walking animation. `field_0` is the opcode:
/// 0x109 drives the aim state machine, 0x104 puts the chaser at one of the four places
/// in `Actor00100_D00004`, drawn with the global LCG among those the camera's
/// view allows, 0x1602 plays the step sound and takes the fourth tuning of
/// `Actor00100_D0BDB4`, and 0x202 writes the fixed
/// crouch pose. Every opcode except 0x104/0x1602/0x202 returns 0.
///
/// Each LCG arm keeps its own `value` local: they are separate variables
/// because the arms are separate blocks and one local shared between them
/// changes which register the allocator picks in every arm.
s32 Actor00100_Fn00E58(Task* arg0, s32 arg1, ActorCommand* request, s32 arg3)
{
    _Actor00100MesaPlacements placements;
    _Actor00100MesaPlacement* placement;
    DesertChaserWork*         work;
    Enemy*                    ctx;
    s8                        rnd;
    s32                       view;
    u32                       value2;
    u32                       value4;
    u32                       value5;
    u32                       value38;
    u32                       valueDefault;
    s32                       kind;
    s32                       cmd;
    s32                       sub;
    s32                       req;
    s32                       sound;
    s32                       pan;

    work = arg0->work;
    ctx  = arg0->spawnArg2.pointer;

    if (request->context.key == 0x109) {
        kind = request->command;
        switch (kind) {
            case 1:
                work->chaseHoldoff = 0x5A;
                break;
            case 2:
                if (work->state == 0x26) {
                    work->state = 0x26;
                }
                break;
            case 3:
                work->chaseHoldoff = work->chaseHoldoffFrames;
                break;
            case 4:
                if (work->state == 0x21) {
                    work->state = 0x22;
                }
                if (work->state == 0x18) {
                    work->state = 0x26;
                }
                break;
        }
        return 1;
    } else {
        work->lastCommand.fields.stage   = request->context.loc.stage;
        work->lastCommand.fields.area    = request->context.loc.area;
        work->lastCommand.fields.command = (u8)request->command;
        if (request->context.key == 0x104) {
            placements = Actor00100_D00004;
            cmd        = request->command;
            switch (cmd) {
                case 0:
                    work->state = 0;
                    break;
                case 1:
                    view = viewGetMappedIndex() & 0xFF;
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
                    placement                           = &placements.placements[rnd];
                    arg0->extra.tmd->coords->coord.t[0] = placement->x;
                    arg0->extra.tmd->coords->coord.t[1] = placement->y;
                    arg0->extra.tmd->coords->coord.t[2] = placement->z;
                    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, placement->yaw, 1);
                    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                    work->state                           = 5;
                    break;
            }
        }
        if (request->context.key == 0x1602) {
            sub = request->command;
            switch (sub) {
                case 0:
                    work->state = 0;
                    break;
                case 2:
                    sound = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x52160009;
                    pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                    sndEvtRequestScriptStart(sound, pan,
                                             (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                    work->animId      = sub;
                    work->animRequest = sub;
                    desertChaserAnimTick(arg0);
                    desertChaserAnimTick(arg0);
                    work->state              = 0x1C;
                    work->windupFrames       = Actor00100_D0BDB4[3].windupFrames;
                    work->downFramesBase     = Actor00100_D0BDB4[3].downFramesBase;
                    work->roamLookDelay      = Actor00100_D0BDB4[3].roamLookDelay;
                    work->chaseHoldoffFrames = Actor00100_D0BDB4[3].chaseHoldoffFrames;
                    break;
            }
        }
        if (request->context.key == 0x202) {
            req = request->command;
            switch (req) {
                case 0:
                    work->state = 0;
                    break;
                case 2:
                    work->state                         = 0x26;
                    arg0->extra.tmd->coords->coord.t[0] = -0x896;
                    arg0->extra.tmd->coords->coord.t[1] = 0;
                    arg0->extra.tmd->coords->coord.t[2] = 0x5AF;
                    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, -0x3F4, 1);
                    break;
            }
        }
        return 0;
    }
}

/// Collects bearings from the obstacles in `recs` into a
/// `DesertChaserAvoidScratch` and steps `coord` along each survivor. Same walk
/// as `ActorContact_Steer`, but `blocked` is raised only for a kind 0x10000
/// record whose `key` bit 0x80 is clear. The scratch is carved before the
/// early-out, so that path leaks it.
static s32 desertChaserAvoidWalk(GfxCoord* coord, WorldCollisionContact* recs, s16 count, SVECTOR* pos)
{
    DesertChaserAvoidScratch* s;
    s16                       diff;

    s = SCRATCH_STACK_RESERVE_BLOCK(DesertChaserAvoidScratch);

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1 || gGameSession->viewReady == 1) {
        return 0;
    }

    s->blocked = 0;
    pos->vz    = 0;
    pos->vy    = 0;
    pos->vx    = 0;

    gfxReadMatrixYAxis(&coord->workm, &s->dir);
    VectorNormalSS(&s->dir, &s->dir);

    if (ABS(s->dir.vz) < 0x818) {
        s->heading = ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
    } else {
        s->heading = -ratan2(-coord->workm.m[0][2], coord->workm.m[1][2]);
    }

    s->origin.vx = (u16)coord->workm.t[0];
    s->origin.vy = (u16)coord->workm.t[1];
    s->origin.vz = (u16)coord->workm.t[2];
    s->count     = 0;

    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].key.value == 0) {
            break;
        }
        s->kind        = recs[s->i].key.value & WORLD_COLLISION_CONTACT_KIND_MASK;
        s->nonBlocking = recs[s->i].key.value & 0x80;
        switch (s->kind) {
            case 0x10000:
                if (s->nonBlocking == 0) {
                    s->blocked = 1;
                }
            case 0x30000:
                break;
            default:
                continue;
        }

        if (ABS(s->dir.vz) < 0x818) {
            s->bearing[s->count] = overlayBearingXZ((SVECTOR3*)&recs[s->i].point, &s->origin);
        } else {
            s->bearing[s->count] = overlayBearingXY((SVECTOR3*)&recs[s->i].point, &s->origin);
        }
        s->kept[s->count] = 1;
        s->count++;
        if (s->count >= ARRAY_SIZE(s->bearing)) {
            break;
        }
    }

    for (s->i = 0; s->i < s->count; s->i++) {
        for (s->j = s->i + 1; s->j < s->count; s->j++) {
            s->diff = actorWrapAngle((u16)s->bearing[s->i] - (u16)s->bearing[s->j]);
            if (abs(s->diff) > 0x400) {
                s->kept[s->i] = 0;
                s->kept[s->j] = 0;
            }
        }
        if (s->kept[s->i] != 0) {
            diff = ((u16)s->bearing[s->i] - (u16)s->heading) +
                   ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
            s->diff = diff;
            gfxRotMatrixY(&s->rot, diff, 1);
            gfxReadMatrixZAxis(&s->rot, &s->dir);
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

    SCRATCH_STACK_RELEASE_BLOCK(DesertChaserAvoidScratch);
    return s->blocked != 0;
}

/// Draw the limb shadow under the segment between parts `firstJoint` and
/// `secondJoint` of the actor's model: take both ends in world space, widen
/// them into a `width`-half quad at height `height`, and emit it as a
/// subtractive `POLY_FT4` tinted `shade`.
static void Actor00100_Fn01900(Task* actor, s16 firstJoint, s16 secondJoint, s16 width, s16 height, u8 shade)
{
    ActorLimbShadowScratch* s;
    s16                     angle;
    GfxCoord*               secondCoord;
    GfxCoord*               firstCoord;
    s32                     offset0;
    s32                     offset1;
    s32                     offset2;
    s32                     offset3;
    s32                     halfX;
    s32                     halfZ;
    GfxCoord*               coords;
    GfxCoord*               view;
    POLY_FT4*               poly;

    coords      = actor->extra.tmd->coords;
    firstCoord  = coords + firstJoint;
    secondCoord = coords + secondJoint;
    if (firstJoint != secondJoint) {
        s = SCRATCH_STACK_RESERVE_BLOCK(ActorLimbShadowScratch);
        actorRenderComposeCoord(firstCoord);
        actorRenderComposeCoord(secondCoord);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &firstCoord->workm, &s->firstMatrix);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &secondCoord->workm, &s->secondMatrix);
        s->firstPos.vy   = height;
        s->secondPos.vy  = height;
        s->firstPos.vx   = s->firstMatrix.t[0];
        s->firstPos.vz   = s->firstMatrix.t[2];
        s->secondPos.vx  = s->secondMatrix.t[0];
        s->secondPos.vz  = s->secondMatrix.t[2];
        angle            = ratan2(s->secondPos.vx - s->firstPos.vx, s->secondPos.vz - s->firstPos.vz);
        halfX            = (s->firstPos.vx - s->secondPos.vx) / 2;
        halfZ            = (s->firstPos.vz - s->secondPos.vz) / 2;
        offset0          = rcos(angle) * width;
        s->corners[0].vy = height;
        s->corners[0].vx = halfX + (s->firstPos.vx - (offset0 >> 0xC));
        s->corners[0].vz = halfZ + (s->firstPos.vz + ((s32)(rsin(angle) * width) >> 0xC));
        offset1          = rcos(angle) * width;
        s->corners[1].vy = height;
        s->corners[1].vx = halfX + (s->firstPos.vx + (offset1 >> 0xC));
        s->corners[1].vz = halfZ + (s->firstPos.vz - ((s32)(rsin(angle) * width) >> 0xC));
        offset2          = rcos(angle) * width;
        s->corners[2].vy = height;
        s->corners[2].vx = (s->secondPos.vx - (offset2 >> 0xC)) - halfX;
        s->corners[2].vz = (s->secondPos.vz + ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
        offset3          = rcos(angle) * width;
        s->corners[3].vy = height;
        s->corners[3].vx = (s->secondPos.vx + (offset3 >> 0xC)) - halfX;
        s->corners[3].vz = (s->secondPos.vz - ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
        /* `gGfxViewCoord`, reached back from its `workm`: the address is built
           from `gGfxViewCoord.workm`, whose high half the GTE loads below share. */
        view               = &gGfxViewCoord;
        view->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(view);
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        s->depth = RotTransPers4(&s->corners[0], &s->corners[1], &s->corners[2], &s->corners[3], &s->screenCorners[0], &s->screenCorners[1],
                                 &s->screenCorners[2], &s->screenCorners[3], &s->depthCue, &s->flag);
        if (s->flag >= 0) {
            poly           = gGpuPrimCursor;
            gGpuPrimCursor = poly + 1;
            setlen(poly, 9);
            poly->code                     = 0x2E;
            GPU_PRIMITIVE_XY_WORD(poly, 0) = s->screenCorners[0];
            GPU_PRIMITIVE_XY_WORD(poly, 1) = s->screenCorners[1];
            GPU_PRIMITIVE_XY_WORD(poly, 2) = s->screenCorners[2];
            GPU_PRIMITIVE_XY_WORD(poly, 3) = s->screenCorners[3];
            setUV4(poly, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);
            poly->tpage = 0x48;
            poly->clut  = 0x4283;
            setRGB0(poly, shade, shade, shade);
            addPrim((&gGpuCurrentOt[((((u32)(s->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), poly);
        }
        SCRATCH_STACK_RELEASE_BLOCK(ActorLimbShadowScratch);
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
    DesertChaserWork* work;

    work = arg0->work;
    anim = &work->rig.anim;
    for (index = 1; index < ARRAY_SIZE(work->rig.slots); index++) {
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
            work->blend.slots[index].rate = work->blendRate;
            work->rig.slots[index].rate   = work->animRate - 3;
            animationTickSlotPose(anim, (s32)index, &pose, 0);
            animationTickSlotPose(&work->blend.anim, (s32)index, &otherPose, 0);
            animationApplyPoseWithBlendedRotation(anim, (s32)index, &pose, &otherPose, blend, invBlend);
        } else {
            work->rig.slots[index].rate = work->animRate - 3;
            animationTickSlot(&work->rig.anim, (s32)index);
        }
    }
}

s32 desertChaserAnimCues(Task* arg0, DesertChaserWork* arg1)
{
    DesertChaserWork* work;
    u32               prev;
    s32               var_a0 = 1;

    switch (arg1->animId) {
        case 0:
            if ((arg1->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 9) {
                prev = arg1->lastCueFrames[1];
                if (prev != 9) {
                    arg1->lastCueFrames[1] = 9;
                    work                   = arg0->work;
                    work->effectOffset.vz  = 0;
                    work->effectOffset.vx  = 0;
                    work->effectOffset.vy  = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[17], 0x80002280, &work->effectOffset);
                    }
                    work                  = arg0->work;
                    work->effectOffset.vz = 0;
                    work->effectOffset.vx = 0;
                    work->effectOffset.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[9], 0x80002120, &work->effectOffset);
                    }
                    return 0x40010002;
                }
                arg1->lastCueFrames[1] = prev;
                var_a0                 = 0;
            }
            if ((arg1->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 6) {
                prev = arg1->lastCueFrames[1];
                if (prev != 6) {
                    arg1->lastCueFrames[1] = 6;
                    work                   = arg0->work;
                    work->effectOffset.vz  = 0;
                    work->effectOffset.vx  = 0;
                    work->effectOffset.vy  = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[14], 0x80002220, &work->effectOffset);
                    }
                    work                  = arg0->work;
                    work->effectOffset.vz = 0;
                    work->effectOffset.vx = 0;
                    work->effectOffset.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[7], 0x80002120, &work->effectOffset);
                    }
                    return 0x40010001;
                }
                arg1->lastCueFrames[1] = prev;
                var_a0                 = 0;
            }
            break;
        case 0xA:
            if ((arg1->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0xA) {
                prev = arg1->lastCueFrames[1];
                if (prev != 0xA) {
                    arg1->lastCueFrames[1] = 0xA;
                    work                   = arg0->work;
                    work->effectOffset.vz  = 0;
                    work->effectOffset.vx  = 0;
                    work->effectOffset.vy  = 0;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[0], 0x80004A00, &work->effectOffset);
                    }
                    return 0x40010005;
                }
                arg1->lastCueFrames[1] = prev;
                var_a0                 = 0;
            }
            break;
        case 3:
            if ((arg1->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0xC) {
                prev = arg1->lastCueFrames[1];
                if (prev != 0xC) {
                    arg1->lastCueFrames[1] = 0xC;
                    return 0x40010004;
                }
                arg1->lastCueFrames[1] = prev;
                var_a0                 = 0;
            }
            if ((arg1->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 8) {
                prev = arg1->lastCueFrames[1];
                if (prev != 8) {
                    arg1->lastCueFrames[1] = 8;
                    return 0x40010003;
                }
                arg1->lastCueFrames[1] = prev;
                var_a0                 = 0;
            }
            break;
        case 6:
            if ((arg1->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 6) {
                prev = arg1->lastCueFrames[1];
                if (prev != 6) {
                    arg1->lastCueFrames[1] = 6;
                    work                   = arg0->work;
                    work->effectOffset.vz  = 0;
                    work->effectOffset.vx  = 0;
                    work->effectOffset.vy  = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[9], 0x80003200, &work->effectOffset);
                    }
                    work                  = arg0->work;
                    work->effectOffset.vz = 0;
                    work->effectOffset.vx = 0;
                    work->effectOffset.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[7], 0x80003200, &work->effectOffset);
                    }
                    return 0x40010004;
                }
                arg1->lastCueFrames[1] = prev;
                var_a0                 = 0;
            }
            if ((arg1->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0xC) {
                prev = arg1->lastCueFrames[1];
                if (prev != 0xC) {
                    arg1->lastCueFrames[1] = 0xC;
                    work                   = arg0->work;
                    work->effectOffset.vz  = 0;
                    work->effectOffset.vx  = 0;
                    work->effectOffset.vy  = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[17], 0x80004480, &work->effectOffset);
                    }
                    work                  = arg0->work;
                    work->effectOffset.vz = 0;
                    work->effectOffset.vx = 0;
                    work->effectOffset.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[14], 0x80004480, &work->effectOffset);
                    }
                    return 0x40010011;
                }
                arg1->lastCueFrames[1] = prev;
                var_a0                 = 0;
            }
            break;
        case 0x12:
            if ((arg1->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 6) {
                prev = arg1->lastCueFrames[1];
                if (prev != 6) {
                    arg1->lastCueFrames[1] = 6;
                    work                   = arg0->work;
                    work->effectOffset.vz  = 0;
                    work->effectOffset.vx  = 0;
                    work->effectOffset.vy  = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[9], 0x80003200, &work->effectOffset);
                    }
                    work                  = arg0->work;
                    work->effectOffset.vz = 0;
                    work->effectOffset.vx = 0;
                    work->effectOffset.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[7], 0x80003200, &work->effectOffset);
                    }
                    return 0x40010001;
                }
                arg1->lastCueFrames[1] = prev;
                var_a0                 = 0;
            }
            if ((arg1->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 9) {
                prev = arg1->lastCueFrames[1];
                if (prev != 9) {
                    arg1->lastCueFrames[1] = 9;
                    work                   = arg0->work;
                    work->effectOffset.vz  = 0;
                    work->effectOffset.vx  = 0;
                    work->effectOffset.vy  = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[9], 0x80003200, &work->effectOffset);
                    }
                    work                  = arg0->work;
                    work->effectOffset.vz = 0;
                    work->effectOffset.vx = 0;
                    work->effectOffset.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[17], 0x80003200, &work->effectOffset);
                    }
                    return 0x40010001;
                }
                arg1->lastCueFrames[1] = prev;
                var_a0                 = 0;
            }
            if ((arg1->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0xE) {
                prev = arg1->lastCueFrames[1];
                if (prev != 0xE) {
                    arg1->lastCueFrames[1] = 0xE;
                    work                   = arg0->work;
                    work->effectOffset.vz  = 0;
                    work->effectOffset.vx  = 0;
                    work->effectOffset.vy  = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[14], 0x80003200, &work->effectOffset);
                    }
                    work                  = arg0->work;
                    work->effectOffset.vz = 0;
                    work->effectOffset.vx = 0;
                    work->effectOffset.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[17], 0x80003200, &work->effectOffset);
                    }
                    return 0x40010002;
                }
                arg1->lastCueFrames[1] = prev;
                var_a0                 = 0;
            }
            break;
        case 0x11:
            if ((arg1->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 6) {
                prev = arg1->lastCueFrames[1];
                if (prev != 6) {
                    arg1->lastCueFrames[1] = 6;
                    work                   = arg0->work;
                    work->effectOffset.vz  = 0;
                    work->effectOffset.vx  = 0;
                    work->effectOffset.vy  = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[9], 0x80003200, &work->effectOffset);
                    }
                    work                  = arg0->work;
                    work->effectOffset.vz = 0;
                    work->effectOffset.vx = 0;
                    work->effectOffset.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[7], 0x80003200, &work->effectOffset);
                    }
                    return 0x40010001;
                }
                arg1->lastCueFrames[1] = prev;
                var_a0                 = 0;
            }
            if ((arg1->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0xA) {
                prev = arg1->lastCueFrames[1];
                if (prev != 0xA) {
                    arg1->lastCueFrames[1] = 0xA;
                    work                   = arg0->work;
                    work->effectOffset.vz  = 0;
                    work->effectOffset.vx  = 0;
                    work->effectOffset.vy  = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[7], 0x80003200, &work->effectOffset);
                    }
                    work                  = arg0->work;
                    work->effectOffset.vz = 0;
                    work->effectOffset.vx = 0;
                    work->effectOffset.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[14], 0x80003200, &work->effectOffset);
                    }
                    return 0x40010001;
                }
                arg1->lastCueFrames[1] = prev;
                var_a0                 = 0;
            }
            if ((arg1->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0xE) {
                prev = arg1->lastCueFrames[1];
                if (prev != 0xE) {
                    arg1->lastCueFrames[1] = 0xE;
                    work                   = arg0->work;
                    work->effectOffset.vz  = 0;
                    work->effectOffset.vx  = 0;
                    work->effectOffset.vy  = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[14], 0x80003200, &work->effectOffset);
                    }
                    work                  = arg0->work;
                    work->effectOffset.vz = 0;
                    work->effectOffset.vx = 0;
                    work->effectOffset.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[17], 0x80003200, &work->effectOffset);
                    }
                    return 0x40010002;
                }
                arg1->lastCueFrames[1] = prev;
                var_a0                 = 0;
            }
            break;
        case 0xD:
            if ((arg1->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 0x18) {
                prev = arg1->lastCueFrames[1];
                if (prev != 0x18) {
                    arg1->lastCueFrames[1] = 0x18;
                    return 0x4001000F;
                }
                arg1->lastCueFrames[1] = prev;
                var_a0                 = 0;
            }
            break;
    }
    if (var_a0 == 1) {
        memFillBytes(arg1->lastCueFrames, 0U, sizeof(arg1->lastCueFrames));
    }
    return 0;
}

#include "../../shared/desert_chaser_anim_tick.inc.c"

/// Builds the damage state: allocates the work block, wires the two
/// animation contexts and the four collision objects onto the model, latches
/// the spawn position and the one a fixed step ahead of it, then picks the
/// start state and `DesertChaserVariant` out of the two nibbles of `spawnArg1`.
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
    work       = memCalloc(sizeof(DesertChaserWork), false);
    arg1->work = work;
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    (sceneAcquireBattleRef)(0);
    arg1->exitCallback = desertChaserExit;
    mapped             = arg1->work;
    model              = arg1->extra.tmd;
    model->lightMtx    = &mapped->lightMtx;
    model->colorMtx    = &mapped->colorMtx;
    arg0->field_4      = &arg1->extra.tmd->coords[0].coord;
    arg0->field_48     = 0;
    arg0->bodyPos.vx   = 0;
    arg0->bodyPos.vy   = 0;
    arg0->bodyPos.vz   = 0;
    arg0->coord        = &arg1->extra.tmd->coords[2];
    worldTargetLinkNode(&arg0->node);
    arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    arg0->reactionFlags          = 0;
    arg0->hp                     = Actor00100_D0BDA4.hpMax;
    arg0->param                  = &Actor00100_D0BDA4;
    arg0->recs                   = work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts;
    animationInitContext(&work->rig.anim, Actor00100_D1B944, tmd, work->rig.poses, work->rig.slots);
    animationInitContext(&work->blend.anim, Actor00100_D1B944, tmd, work->blend.poses, work->blend.slots);
    work->animRequest   = DESERT_CHASER_ANIM_REQUEST_RESET;
    work->blendActive   = 0;
    work->animId        = 0;
    work->lookYaw       = 0;
    work->lookYawTarget = 0;
    if ((arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) & 1) {
        work->baseRate = 0xF;
        work->animRate = 0xF;
    } else {
        work->baseRate = 0x11;
        work->animRate = 0x11;
    }
    work->blendRate = 0x10;
    desertChaserAnimTick(arg1);
    work->spheres[DESERT_CHASER_SPHERE_ROOT].body.coord            = coord;
    work->spheres[DESERT_CHASER_SPHERE_ROOT].body.context.contacts = work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts;
    work->spheres[DESERT_CHASER_SPHERE_ROOT].body.pos.vx           = 0;
    work->spheres[DESERT_CHASER_SPHERE_ROOT].body.pos.vy           = -0x11C;
    work->spheres[DESERT_CHASER_SPHERE_ROOT].body.pos.vz           = 0;
    work->spheres[DESERT_CHASER_SPHERE_ROOT].body.key              = 0x30001;
    work->spheres[DESERT_CHASER_SPHERE_ROOT].body.radius           = 0x12C;
    work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->spheres[DESERT_CHASER_SPHERE_ROOT].body);
    work->wallProbe.shape.ends[0].vx                     = 0;
    work->wallProbe.shape.ends[0].vy                     = -0x180;
    work->wallProbe.shape.ends[0].vz                     = 0;
    work->wallProbe.shape.ends[1].vx                     = 0;
    work->wallProbe.shape.ends[1].vy                     = -0x180;
    work->wallProbe.shape.ends[1].vz                     = 0x2BC;
    work->wallProbe.shape.end0Radius                     = 0x12C;
    work->wallProbe.shape.end1Radius                     = 0x12C;
    work->wallProbe.shape.contacts                       = work->wallProbe.contacts;
    work->wallProbe.body.coord                           = coord;
    work->wallProbe.body.context.capsule                 = &work->wallProbe.shape;
    work->wallProbe.body.pos.vx                          = 0;
    work->wallProbe.body.pos.vy                          = 0;
    work->wallProbe.body.pos.vz                          = 0;
    work->wallProbe.body.key                             = 0x30001;
    work->wallProbe.body.radius                          = 1;
    work->wallProbe.body.flags                           = WORLD_COLLISION_BODY_CAPSULE;
    work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->wallProbe.body);
    work->wallProbe.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    worldCollisionInitContacts(work->wallProbe.contacts, ARRAY_SIZE(work->wallProbe.contacts), 0);
    worldCollisionInitContacts(work->spheres[DESERT_CHASER_SPHERE_ROOT].body.context.contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts), 0);
    primary                        = &work->spheres[DESERT_CHASER_SPHERE_FRONT];
    primary->body.coord            = &arg1->extra.tmd->coords[2];
    primary->body.context.contacts = primary->contacts;
    primary->body.pos.vx           = 0;
    primary->body.pos.vy           = 0;
    primary->body.pos.vz           = 0;
    primary->body.key              = 0x30001;
    primary->body.radius           = 0x19C;
    primary->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &primary->body);
    primary->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(primary->body.context.contacts, ARRAY_SIZE(primary->contacts), 0);
    secondary                        = &work->spheres[DESERT_CHASER_SPHERE_REAR];
    secondary->body.coord            = &arg1->extra.tmd->coords[10];
    secondary->body.context.contacts = secondary->contacts;
    secondary->body.pos.vx           = 0;
    secondary->body.pos.vy           = 0;
    secondary->body.pos.vz           = 0;
    secondary->body.key              = 0x30001;
    secondary->body.radius           = 0x100;
    secondary->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &secondary->body);
    secondary->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(secondary->body.context.contacts, ARRAY_SIZE(secondary->contacts), 0);
    work->spheres[DESERT_CHASER_SPHERE_REAR].body.pos.vx = 0;
    work->spheres[DESERT_CHASER_SPHERE_REAR].body.pos.vy = 0;
    work->spheres[DESERT_CHASER_SPHERE_REAR].body.pos.vz = -0x100;
    work->patrolTarget                                   = 0;
    work->patrolPoints[0].x                              = arg1->extra.tmd->coords[0].coord.t[0];
    work->patrolPoints[0].z                              = arg1->extra.tmd->coords[0].coord.t[2];
    gfxReadMatrixZAxis(&arg1->extra.tmd->coords[0].coord, &vec);
    vec.vy = 0;
    dir    = &vec;
    VectorNormalSS(dir, dir);
    gte_lddp(5000);
    gte_ldsv(dir);
    gte_gpf12();
    gte_stsv(dir);
    work->patrolPoints[1].x               = arg1->extra.tmd->coords[0].coord.t[0] + vec.vx;
    work->patrolPoints[1].z               = arg1->extra.tmd->coords[0].coord.t[2] + vec.vz;
    work->playerAnim.source.sets          = NULL;
    work->playerAnim.animationId          = 1;
    work->playerAnim.blend                = ANIMATION_BLEND_RESET;
    work->playerAnim.blendFrames          = 3;
    work->playerAnim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
    arg1->msgTable                        = Actor00100_D1BA54;
    coord->parent                         = &gGfxViewCoord;
    coord->composeStamp                   = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    color.vx = coord->workm.t[0];
    color.vy = coord->workm.t[1];
    color.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0, &color, 0, 0);
    work->effectArg.coord      = &arg1->extra.tmd->coords[1];
    work->effectArg.spawnArgLo = 0x100;
    work->effectArg.spawnArgHi = 2;
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
    work->prevState = -1;
    work->state     = 0x18;
    tmdAllocPrimitiveBuffer(tmd);
    goto stateEnd;
state1:
    work->prevState = -1;
    work->state     = 0;
    goto stateEnd;
state2:
    work->prevState = -1;
    work->state     = 0x21;
    goto stateEnd;
state3:
    work->prevState = -1;
    work->state     = 5;
    goto stateEnd;
stateStill:
    work->prevState = -1;
    work->state     = 0x18;
    tmdAllocPrimitiveBuffer(tmd);
stateEnd:
    kind = arg1->spawnArg1.value & 0xF;
    if (kind == 1) {
        goto variant2;
    }
    if (kind < 2) {
        goto variant1;
    }
    if (kind != 2) {
        goto variant1;
    }
    work->windupFrames       = Actor00100_D0BDB4[0].windupFrames;
    work->downFramesBase     = Actor00100_D0BDB4[0].downFramesBase;
    work->roamLookDelay      = Actor00100_D0BDB4[0].roamLookDelay;
    work->chaseHoldoffFrames = Actor00100_D0BDB4[0].chaseHoldoffFrames;
    goto variantEnd;
variant2:
    work->windupFrames       = Actor00100_D0BDB4[2].windupFrames;
    work->downFramesBase     = Actor00100_D0BDB4[2].downFramesBase;
    work->roamLookDelay      = Actor00100_D0BDB4[2].roamLookDelay;
    work->chaseHoldoffFrames = Actor00100_D0BDB4[2].chaseHoldoffFrames;
    goto variantEnd;
variant1:
    work->windupFrames       = Actor00100_D0BDB4[1].windupFrames;
    work->downFramesBase     = Actor00100_D0BDB4[1].downFramesBase;
    work->roamLookDelay      = Actor00100_D0BDB4[1].roamLookDelay;
    work->chaseHoldoffFrames = Actor00100_D0BDB4[1].chaseHoldoffFrames;
variantEnd:
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
            cdCmdEnqueue(CD_COMMAND_LOAD_FILE, cmd38, cmd30);
        }
    }
    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 1, 0, 0)) {
        func_mine_mesa_801811C4(0x7D0);
    }
    worldCollisionClearContacts(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts);
    worldCollisionClearContacts(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts);
    worldCollisionClearContacts(work->spheres[DESERT_CHASER_SPHERE_REAR].contacts);
    worldCollisionClearContacts(work->wallProbe.contacts);
    work->deathPending = 0;
    arg1->state       += 1;
}

#include "../../shared/desert_chaser_hit_effect.inc.c"

static void Actor00100_Fn0375C(Task* arg0)
{
    PlayerStatus*              config = &gPlayerStatus;
    Enemy*                     ctx;
    GfxCoord*                  coord;
    s16                        effect;
    s16                        delta;
    s16                        z;
    s16                        state3;
    s32                        state4;
    s16                        damageState;
    s16                        deathState;
    s16                        hurtState;
    s16                        poisonState;
    s16                        state0;
    s16                        state1;
    s32                        magnitude;
    s16                        nextState;
    s16                        wrapped;
    s32                        yaw;
    s32                        deathSound;
    s32                        hurtSound;
    s32                        hitSound;
    s32                        distance;
    s32                        dx;
    s32                        dy;
    s32                        dz;
    s32                        soundBase;
    s32                        deathPan;
    s32                        hurtPan;
    s32                        hitPan;
    u16                        totalDamage;
    u32                        tickDamage;
    u32                        doubleDamage;
    u32                        kind;
    u8                         sessionMode;
    SVECTOR*                   hitPos;
    void*                      temp_a2;
    DesertChaserWork*          work;
    DesertChaserDamageScratch* scratch;
    DesertChaserDamageScratch* head;
    void*                      temp_v1_2;
    void*                      temp_v1_3;

    ctx  = arg0->spawnArg2.pointer;
    work = arg0->work;
    if (ctx->hp > 0) {
        head            = SCRATCH_STACK_CURSOR(DesertChaserDamageScratch);
        scratch         = (SCRATCH_STACK_CURSOR(DesertChaserDamageScratch) = head - 1);
        scratch->hitKey = Actor00100_FindDamageHit(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts, &scratch->hitPos);
        if (scratch->hitKey == 0) {
            hitPos          = &scratch->hitPos;
            scratch->hitKey = Actor00100_FindDamageHit(work->spheres[DESERT_CHASER_SPHERE_REAR].contacts, hitPos);
        }
        if (scratch->hitKey != 0) {
            work->hitFlag                         = 1;
            scratch->criticalEffect               = -1;
            work->hitCooldown                     = Gp_GetIdParam2(scratch->hitKey);
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(arg0->extra.tmd->coords);
            scratch->hitOffset.vx = scratch->hitPos.vx - arg0->extra.tmd->coords->workm.t[0];
            scratch->hitOffset.vy = scratch->hitPos.vy - arg0->extra.tmd->coords->workm.t[1];
            z                     = scratch->hitPos.vz - arg0->extra.tmd->coords->workm.t[2];
            scratch->hitOffset.vz = z;
            yaw                   = ratan2(scratch->hitOffset.vx, z);
            coord                 = arg0->extra.tmd->coords;
            delta                 = yaw - ratan2((s32)-coord->workm.m[2][0], (s32)coord->workm.m[2][2]);
            wrapped               = delta;
            scratch->hitYaw       = delta;
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
            scratch->hitYaw = wrapped;
            kind            = Gp_GetIdParam0(scratch->hitKey) & 0xFFFF;
            switch (kind) {
                case 0:
                case 5:
                case 6:
                case 7:
                    state0 = work->state;
                    if ((state0 == 0x18) || (state0 == 0x26) || (state0 == 0x20)) {
                        work->state = 0x1C;
                    }
                    if (work->state == 0x21) {
                        work->state = 0x22;
                    }
                    if (work->blendActive == 0) {
                        work->recentDamage = 0U;
                    }
                    state1 = work->state;
                    if (state1 == 4 || state1 == 7 || state1 == 11 || state1 == 17) {
                        work->state     = 11;
                        work->prevState = -1;
                    } else if (state1 != 0x22 && state1 != 0x15 && state1 != 0x14 && state1 != 7 && state1 != 0x24) {
                        work->blendActive  = 1;
                        work->blendAnimId  = 15;
                        work->blendRequest = DESERT_CHASER_ANIM_REQUEST_RESET;
                    }
                    break;
                case 8:
                    sessionMode = gGameSession->location.loc.stage;
                    if ((sessionMode != 2) && (sessionMode != 5)) {
                        magnitude = scratch->hitYaw;
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
                    Gp_SetObjFlag2(ctx, scratch->hitKey, 0);
                    break;
                case 3:
                    state3 = work->state;
                    if ((state3 == 0x18) || (state3 == 0x26) || (state3 == 0x20)) {
                        work->state = 0x1C;
                    }
                    if (work->state == 0x21) {
                        work->state = 0x22;
                    }
                    Gp_SetObjFlag4(ctx, scratch->hitKey, 0);
                    break;
                case 1:
                case 4:
                    state4 = work->state;
                    if (state4 == 4 || state4 == 7 || state4 == 0x21 || state4 == 0x14 || state4 == 0xB || state4 == 0x11 || (state4 == 0x24 && work->stateTimer < 10)) {
                        nextState   = 7;
                        work->state = nextState;
                    } else if (state4 != 0x15 && state4 != 0) {
                        nextState   = 0x14;
                        work->state = nextState;
                    }
                    break;
            }
            dx                      = config->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0];
            scratch->toPlayer.vx    = dx;
            dy                      = config->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1];
            scratch->toPlayer.vy    = dy;
            dz                      = config->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2];
            scratch->toPlayer.vz    = dz;
            distance                = SquareRoot0((dx * dx) + (dy * dy) + (dz * dz));
            scratch->playerDistance = distance;
            scratch->damage         = Gp_ComputeDamage(scratch->hitKey, distance, 0, 0);
            desertChaserHitEffect(arg0, scratch->hitYaw, scratch->hitKey);
            work->lookYaw       = 0;
            work->lookYawTarget = 0;
            if (Gp_RollEnemyChance(ctx, scratch->hitKey, 0) != 0) {
                scratch->criticalEffect = 0;
                scratch->damage         = scratch->damage * 4;
            }
            damageState = work->state;
            if (damageState == 4 || (damageState == 0x14 && work->stateTimer >= 11) || damageState == 7 || damageState == 0x11 || damageState == 0xB || (damageState == 0x24 && work->stateTimer < 10)) {
                doubleDamage    = scratch->damage * 2;
                scratch->damage = doubleDamage;
                if (doubleDamage != 0) {
                    scratch->criticalEffect = 3;
                }
            }
            func_800E2C78(ctx, scratch->hitKey, scratch->damage, 0);
            effect = scratch->criticalEffect;
            if (effect != -1) {
                Gp_SpawnEff(EFFECT_CRITICAL_HIT, arg0->extra.tmd->coords + 2, (s32)(effect), NULL);
            }
            ctx->hp = ctx->hp - scratch->damage;
            func_800DA6E8(&ctx->node, scratch->damage, 0);
            totalDamage        = work->recentDamage + (u16)scratch->damage;
            work->recentDamage = totalDamage;
            if (ctx->hp <= 0) {
                work->broadcast.context.loc.stage = 9;
                work->broadcast.context.loc.area  = 1;
                work->broadcast.command           = 4;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &work->broadcast, ACTOR_COMMAND_MESSAGE_APPLY);
                if ((Gp_GetIdParam0(scratch->hitKey) & 0xFFFF) == 4) {
                    work->state = 3;
                } else {
                    deathState = work->state;
                    if ((deathState == 0x21) || (deathState == 0x11) || (deathState == 0xB) || (deathState == 7) || (deathState == 4)) {
                        soundBase       = 0x40010008;
                        work->state     = 7;
                        work->prevState = -1;
                        goto playHitSound;
                    }
                    deathSound = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40010008;
                    deathPan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                    sndEvtRequestScriptStart(deathSound, (s32)deathPan, (s32)(s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                    work->state = 0x14;
                }
            } else if ((s16)totalDamage >= 0x47) {
                hurtState = work->state;
                if ((hurtState != 4) && (hurtState != 0x14) && (hurtState != 7) && (hurtState != 0xB) && (hurtState != 0x11)) {
                    hurtSound = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40010008;
                    hurtPan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                    sndEvtRequestScriptStart(hurtSound, (s32)hurtPan, (s32)(s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                    work->state = 0x14;
                } else {
                    goto normalHitSound;
                }
            } else {
            normalHitSound:
                soundBase = 0x40010007;
            playHitSound:
                hitSound = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | soundBase;
                hitPan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                sndEvtRequestScriptStart(hitSound, (s32)hitPan, (s32)(s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            }
        }
        if (ctx->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            scratch->damage = Gp_TickObjFlag4(ctx);
            if (Gp_ObjFlag4Expired(ctx) != 0) {
                ctx->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
            }
            ctx->hp    = ctx->hp - scratch->damage;
            tickDamage = scratch->damage;
            if (tickDamage != 0) {
                func_800DA6E8(&ctx->node, (s32)tickDamage, 0);
                if (ctx->hp <= 0) {
                    poisonState = work->state;
                    if ((poisonState != 4) && (poisonState != 7) && (poisonState != 0xB) && (poisonState != 0x11)) {
                        work->state = 0xA;
                    } else {
                        work->state = 0x15;
                    }
                } else {
                    if (work->state == 0x1C) {
                        work->state = 0x26;
                    }
                    if (work->state != 4 && work->state != 7 && work->state != 11 && work->state != 17) {
                        work->blendActive  = 1;
                        work->blendAnimId  = 15;
                        work->blendRequest = DESERT_CHASER_ANIM_REQUEST_RESET;
                    } else if (work->state != 4) {
                        work->state = 11;
                    } else {
                        work->prevState = -1;
                    }
                }
            }
        }
        if (ctx->hp <= 0) {
            work->deathPending = 1;
        }
        SCRATCH_STACK_RELEASE_BLOCK(DesertChaserDamageScratch);
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
    if (work->stateEntered != 0) {
        obj->flags                                           = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        ctx->node.state.parts.flags                          = WORLD_TARGET_NOT_LOCKABLE;
        work->stateTimer                                     = 0;
    }
    if (work->stateTimer == 0x3C) {
        worldCollisionUnlinkBody(&work->spheres[DESERT_CHASER_SPHERE_FRONT].body);
        worldCollisionUnlinkBody(&work->spheres[DESERT_CHASER_SPHERE_REAR].body);
        worldCollisionUnlinkBody(&work->wallProbe.body);
        worldCollisionUnlinkBody(&work->spheres[DESERT_CHASER_SPHERE_ROOT].body);
        ctx->recs = 0;
    }
    if (work->stateTimer >= 0x3D && work->playerHeld == 0 && Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL && gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 1, 0, 0)) {
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT, 0);
        }
        arg0->state++;
        return;
    }
    switch (++work->stateTimer) {
        case 1:
            Gp_SetLightMode(ctx, ENEMY_COLOR_DEFAULT);
            Gp_SetLightMode(ctx, ENEMY_COLOR_WEIGHTED);
            /* fallthrough */
        case 0xA:
            arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            Gp_SetLightMode(ctx, ENEMY_COLOR_BLACK);
            break;
        case 0xF:
            work->burnPosFront.vx   = 0;
            work->burnPosFront.vy   = 0;
            work->burnPosFront.vz   = 0;
            work->burnPosForeleg.vx = 0;
            work->burnPosForeleg.vy = 0;
            work->burnPosForeleg.vz = 0;
            actorLocalToView(&arg0->extra.tmd->coords[2], &work->burnPosFront);
            Gp_SpawnEff(EFFECT_CORPSE_BURN, &gGfxViewCoord, 2, &work->burnPosFront);
            work->burnPosFront.vy = arg0->extra.tmd->coords[0].coord.t[1];
            actorLocalToView(&arg0->extra.tmd->coords[9], &work->burnPosForeleg);
            work->burnPosForeleg.vy = arg0->extra.tmd->coords[0].coord.t[1];
            Gp_SpawnEff(EFFECT_CORPSE_BURN, &gGfxViewCoord, 2, &work->burnPosForeleg);
            break;
        case 0x3C:
            arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }

    if (work->stateTimer > 0xA) {
        v = (work->stateTimer - 0xA) * 107;
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
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animId                                          = 4;
        work->blendActive                                     = 0;
        work->hitFlag                                         = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->animRate                                        = work->baseRate;
        desertChaserAnimTick(arg0);
        return;
    }
    radius = 1000;
    desertChaserAnimTick(arg0);
    Actor00100_PositionDelta(arg0->extra.tmd->coords, &delta);
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        outside     = actorOutsideRadius(&delta, radius);
        work->state = outside == 0 ? 0x1F : 0x26;
    }
}

#include "../../shared/desert_chaser_spawn_aim.inc.c"

#include "../../shared/desert_chaser_strike.inc.c"

static void Actor00100_Fn06C10(Task* arg0)
{
    DesertChaserWork* work;
    TmdObject*        obj;
    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animId                                          = 6;
        work->blendActive                                     = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->animRate                                        = work->baseRate;
        desertChaserAnimTick(arg0);
        work->stateTimer                 = 0;
        work->stateCounter               = 0;
        work->wallProbe.shape.ends[1].vz = -0x2D0;
    }
    work->stateTimer += 1;
    desertChaserAnimTick(arg0);
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        work->state = 0x26;
    }
    if (((u32)((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) - 6) < 8U) && (work->stateCounter < 5)) {
        if (ActorContact_PushContact(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts)) != 0) {
            work->stateCounter = (s16)((u16)work->stateCounter + 1);
        }
        switch (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) {
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
        ActorContact_PushContact(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts));
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

static void Actor00100_Fn070DC(Task* arg0)
{
    DesertChaserWork*  work;
    GfxCoord*          coord;
    GfxCoord*          coord2;
    GfxCoord*          facing;
    GfxCoord*          facing2;
    TmdObject*         obj;
    s16                delta;
    s32                playerX;
    s16                delta2;
    s16                z;
    s16                targetYaw;
    s16                wrapped;
    s16                wrappedYaw;
    s16                wrapped2;
    s32                angle;
    s32                angle2;
    s32                finalDelta;
    s32                firstDelta;
    s32                magnitude;
    ActorChaseScratch* head;
    ActorChaseScratch* scratch;

    head    = SCRATCH_STACK_CURSOR(ActorChaseScratch);
    work    = arg0->work;
    scratch = (SCRATCH_STACK_CURSOR(ActorChaseScratch) = head - 1);
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animId                                          = 2;
        work->blendActive                                     = 0;
        work->waistYawTarget                                  = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->animRate                                        = work->baseRate;
        desertChaserAnimTick(arg0);
        work->stateTimer = 0;
    }
    ActorContact_PushContact(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts));
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    coord                                 = arg0->extra.tmd->coords;
    head[-1].delta.vx                     = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    scratch->delta.vy                     = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    scratch->delta.vz                     = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    desertChaserAnimTick(arg0);
    facing  = arg0->extra.tmd->coords;
    angle   = ratan2(head[-1].delta.vx, scratch->delta.vz);
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
    firstDelta             = wrapped;
    scratch->turn          = firstDelta;
    work->lookYawTarget    = firstDelta;
    playerX                = -(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0];
    scratch->playerYaw     = ratan2((s32)playerX, (s32)(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
    coord2                 = arg0->extra.tmd->coords;
    scratch->delta.vx      = gPlayerStatus.coordMtx->t[0] - coord2->coord.t[0];
    scratch->delta.vy      = gPlayerStatus.coordMtx->t[1] - coord2->coord.t[1];
    z                      = gPlayerStatus.coordMtx->t[2] - coord2->coord.t[2];
    scratch->delta.vz      = z;
    targetYaw              = ratan2(scratch->delta.vx, (s32)z) + 0x800;
    wrappedYaw             = targetYaw;
    scratch->yawFromPlayer = targetYaw;

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
    scratch->yawFromPlayer = wrappedYaw;
    facing2                = arg0->extra.tmd->coords;
    angle2                 = ratan2(scratch->delta.vx, scratch->delta.vz);
    delta2                 = angle2 - ratan2((s32)-facing2->coord.m[2][0], (s32)facing2->coord.m[2][2]);
    wrapped2               = delta2;

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
    finalDelta          = wrapped2;
    scratch->turn       = (s16)finalDelta;
    work->lookYawTarget = (s16)finalDelta;
    magnitude           = abs(scratch->yawFromPlayer - scratch->playerYaw);
    if (magnitude >= 0x601) {
        Gp_ArmStateF0(1);
        work->state = 0x1C;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
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
    if (work->stateEntered != 0) {
        obj                         = arg0->extra.tmd;
        ctx->node.state.parts.flags = 0;
        obj->flags                  = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->animId                                          = 0xA;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->blendActive                                     = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->animRate                                        = work->baseRate;
        desertChaserAnimTick(arg0);
        sound = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40010009;
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        ctx->hp = ctx->hp - 0xF;
        func_800DA6E8(&ctx->node, 0xF, 0);
        if (ctx->hp <= 0) {
            ctx->hp = 1;
        } else {
            sound2 = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40010007;
            pan2   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            sndEvtRequestScriptStart(sound2, pan2, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        }
    }
    ActorContact_PushContact(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts));
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    desertChaserAnimTick(arg0);
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        if (ctx->hp <= 0) {
            work->state = 0x15;
        } else if (ctx->reactionFlags & ENEMY_REACTION_BUILDUP) {
            work->state = 4;
        } else {
            work->state = 0x11;
        }
    }
}

#include "../../shared/desert_chaser_roam.inc.c"

#include "../../shared/desert_chaser_turn_step.inc.c"

#include "../../shared/desert_chaser_turn_step_probe.inc.c"

static void Actor00100_Fn08E7C(Task* arg0)
{
    DesertChaserWork*              work;
    GfxCoord*                      coord;
    GfxCoord*                      coord2;
    GfxCoord*                      facing;
    GfxCoord*                      facing2;
    TmdObject*                     obj;
    s16                            delta;
    s32                            playerX;
    s16                            delta2;
    s16                            z;
    s16                            targetYaw;
    s16                            wrapped;
    s16                            wrappedYaw;
    s16                            wrapped2;
    s32                            angle;
    s32                            angle2;
    s32                            finalDelta;
    s32                            firstDelta;
    _Actor00100ScreenWatchScratch* head;
    _Actor00100ScreenWatchScratch* scratch;

    head    = SCRATCH_STACK_CURSOR(_Actor00100ScreenWatchScratch);
    work    = arg0->work;
    scratch = (SCRATCH_STACK_CURSOR(_Actor00100ScreenWatchScratch) = head - 1);
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animId                                          = 2;
        work->blendActive                                     = 0;
        work->waistYawTarget                                  = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->animRate                                        = work->baseRate;
        desertChaserAnimTick(arg0);
        work->stateTimer = 0;
    }
    actorRenderComposeCoord(arg0->extra.tmd->coords);
    gte_SetTransMatrix(&arg0->extra.tmd->coords->workm);
    gte_SetRotMatrix(&arg0->extra.tmd->coords->workm);
    head[-1].delta.vx = 0;
    scratch->delta.vy = 0;
    scratch->delta.vz = 0;
    gte_RotTransPers(&scratch->delta, &head[-1].screen, &head[-1].depthCue, &head[-1].projectionFlags, &head[-1].orderingDepth);
    ActorContact_PushContact(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts));
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    coord                                 = arg0->extra.tmd->coords;
    head[-1].delta.vx                     = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    scratch->delta.vy                     = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    scratch->delta.vz                     = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    desertChaserAnimTick(arg0);
    facing  = arg0->extra.tmd->coords;
    angle   = ratan2(head[-1].delta.vx, scratch->delta.vz);
    delta   = angle - ratan2((s32)-facing->coord.m[2][0], (s32)facing->coord.m[2][2]);
    wrapped = delta;

    if (delta < 0) {
        for (;;) {
            if (wrapped < -0x800) {
                wrapped += 0x1000;
            } else {
                break;
            }
        }
    } else {
        for (;;) {
            if (wrapped >= 0x801) {
                wrapped -= 0x1000;
            } else {
                break;
            }
        }
    }
    firstDelta             = wrapped;
    scratch->turn          = firstDelta;
    work->lookYawTarget    = firstDelta;
    playerX                = -(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0];
    scratch->playerYaw     = ratan2((s32)playerX, (s32)(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
    coord2                 = arg0->extra.tmd->coords;
    scratch->delta.vx      = gPlayerStatus.coordMtx->t[0] - coord2->coord.t[0];
    scratch->delta.vy      = gPlayerStatus.coordMtx->t[1] - coord2->coord.t[1];
    z                      = gPlayerStatus.coordMtx->t[2] - coord2->coord.t[2];
    scratch->delta.vz      = z;
    targetYaw              = ratan2(scratch->delta.vx, (s32)z) + 0x800;
    wrappedYaw             = targetYaw;
    scratch->yawFromPlayer = targetYaw;

    if (targetYaw < 0) {
        for (;;) {
            if (wrappedYaw < -0x800) {
                wrappedYaw += 0x1000;
            } else {
                break;
            }
        }
    } else {
        for (;;) {
            if (wrappedYaw >= 0x801) {
                wrappedYaw -= 0x1000;
            } else {
                break;
            }
        }
    }
    scratch->yawFromPlayer = wrappedYaw;
    facing2                = arg0->extra.tmd->coords;
    angle2                 = ratan2(scratch->delta.vx, scratch->delta.vz);
    delta2                 = angle2 - ratan2((s32)-facing2->coord.m[2][0], (s32)facing2->coord.m[2][2]);
    wrapped2               = delta2;

    if (delta2 < 0) {
        for (;;) {
            if (wrapped2 < -0x800) {
                wrapped2 += 0x1000;
            } else {
                break;
            }
        }
    } else {
        for (;;) {
            if (wrapped2 >= 0x801) {
                wrapped2 -= 0x1000;
            } else {
                break;
            }
        }
    }
    finalDelta          = wrapped2;
    scratch->turn       = (s16)finalDelta;
    work->lookYawTarget = (s16)finalDelta;
    if (abs(scratch->screen.vx) < 0x78 && abs(scratch->screen.vy) < 0x64 && abs(scratch->turn) < 0x200) {
        Gp_ArmStateF0(1);
        work->state = 0x1C;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor00100ScreenWatchScratch);
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
    if (work->stateEntered != 0) {
        work->hitFlag                                         = 0;
        obj->flags                                            = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags   = (u16)(work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        ctx->node.state.parts.flags                           = WORLD_TARGET_NOT_LOCKABLE;
        work->lookYaw                                         = 0;
        work->lookYawTarget                                   = 0;
        work->waistYawTarget                                  = 0;
        work->stateTimer                                      = 0U;
        vector.vx                                             = 0x64;
        vector.vz                                             = 0;
        vector.vy                                             = 0;
    }
    next             = work->stateTimer + 1;
    work->stateTimer = next;
    if ((s16)next == 2) {
        obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        tmdFreePrimitiveBuffer(obj);
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
                tmdBuildBufferHalf(effectObj);
                tmdBuildBufferHalf(task->extra.tmd);
            }
        }
        if (work->stateTimer == 2) {
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
                    tmdBuildBufferHalf(effectObj2);
                    tmdBuildBufferHalf(task2->extra.tmd);
                }
            }
        }
    }
    if (work->stateTimer == 4) {
        D_80114B34[5].data.model = &_gActor00100DesertChaserBurstTorso;
        effect3                  = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, &arg0->extra.tmd->coords[1], 0x200, NULL);
        if (effect3 != NULL) {
            task3                               = effect3->task;
            task3->extra.tmd->texturePageOffset = (u8)arg0->extra.tmd->texturePageOffset;
            task3->extra.tmd->clutRowOffset     = (u8)arg0->extra.tmd->clutRowOffset;
            effectObj3                          = task3->extra.tmd;
            if (effectObj3->buffer != 0) {
                tmdBuildBufferHalf(effectObj3);
                tmdBuildBufferHalf(task3->extra.tmd);
            }
        }
    }
    if (work->stateTimer == 5) {
        D_80114B34[5].data.model = &_gActor00100DesertChaserBurstHead;
        effect4                  = Gp_SpawnEff(0xA0000 | 5, &arg0->extra.tmd->coords[3], 0x200, NULL);
        if (effect4 != NULL) {
            task4                               = effect4->task;
            task4->extra.tmd->texturePageOffset = (u8)arg0->extra.tmd->texturePageOffset;
            task4->extra.tmd->clutRowOffset     = (u8)arg0->extra.tmd->clutRowOffset;
            effectObj4                          = task4->extra.tmd;
            if (effectObj4->buffer != 0) {
                tmdBuildBufferHalf(effectObj4);
                tmdBuildBufferHalf(task4->extra.tmd);
            }
        }
    }
    if (work->stateTimer == 0x1E) {
        ctx->hp = 0;
        worldCollisionUnlinkBody(&work->spheres[DESERT_CHASER_SPHERE_FRONT].body);
        worldCollisionUnlinkBody(&work->spheres[DESERT_CHASER_SPHERE_REAR].body);
        worldCollisionUnlinkBody(&work->wallProbe.body);
        worldCollisionUnlinkBody(&work->spheres[DESERT_CHASER_SPHERE_ROOT].body);
        ctx->recs = 0;
    }
    if ((work->stateTimer >= 0x1F) && (work->playerHeld == 0) && (Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
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
    if (work->stateEntered != 0) {
        obj                         = arg0->extra.tmd;
        ctx->node.state.parts.flags = 0;
        obj->flags                  = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animId                                          = 3;
        work->blendActive                                     = 0;
        work->stateTimer                                      = 0;
        work->lookYawTarget                                   = 0;
        work->lookYaw                                         = 0;
        work->waistYawTarget                                  = 0;
        work->waistYaw                                        = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        work->animRate                                        = work->baseRate;
        func_mine_mesa_801811C4(0x7D0);
    }
    work->stateTimer += 1;
    desertChaserAnimTick(arg0);
    state = work->animId;
    switch (state) {
        case 3:
            timer = work->stateTimer;
            if (timer < 0x1E) {
                Actor00100_ScaleTransform(&work->colorMtx, (timer << 12) / 30);
            } else {
                work->animId      = 0xD;
                work->animRequest = DESERT_CHASER_ANIM_REQUEST_BLEND;
                work->stateTimer  = 0;
                sound             = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40010010;
                pan               = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                sndEvtRequestScriptStart(sound, (s32)pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            }
            actorMoveForward(arg0->extra.tmd->coords, 200);
            break;
        case 13:
            if (work->stateTimer <= ((s16)work->baseRate * 17) / 16) {
                actorMoveForwardNonzero(arg0->extra.tmd->coords, ((s16)work->baseRate * 2000) / 272);
            } else if (work->stateTimer <= ((s16)work->baseRate * 25) / 16) {
                actorMoveForwardNonzero(arg0->extra.tmd->coords, ((s16)work->baseRate * 1000) / 192);
            }
            if (work->rig.slots[1].status.fields.flags & 0x100) {
                work->state = 0x26;
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
    if (work->stateEntered != 0) {
        obj                         = arg0->extra.tmd;
        ctx->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                  = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animId                                          = 3;
        work->blendActive                                     = 0;
        work->stateTimer                                      = 0;
        work->lookYawTarget                                   = 0;
        work->lookYaw                                         = 0;
        work->waistYawTarget                                  = 0;
        work->waistYaw                                        = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        work->animRate                                        = work->baseRate;
    }
    work->stateTimer += 1;
    desertChaserAnimTick(arg0);
    switch (work->animId) {
        case 3:
            actorMoveForwardNonzero(arg0->extra.tmd->coords, ((s16)work->baseRate * 1000) / 192);
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            if (work->stateTimer >= 0xD) {
                work->animId      = 0xE;
                work->animRequest = DESERT_CHASER_ANIM_REQUEST_BLEND;
                work->stateTimer  = 0;
            }
            break;
        case 14:
            actorMoveForwardNonzero(arg0->extra.tmd->coords, ((s16)work->baseRate * 1300) / 192);
            timer = work->stateTimer;
            if (timer == 0xF) {
                if ((viewGetMappedIndex() & 0xFF) == 8) {
                    sound = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54010005;
                    pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                    depth = worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords);
                    sndEvtRequestScriptStart(sound, (s8)pan, (s8)(depth + abs(worldCoordGetOriginAudioPan(arg0->extra.tmd->coords)) / 2));
                } else {
                    sound2 = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54010005;
                    pan2   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                    sndEvtRequestScriptStart(sound2, (s8)pan2, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                }
                timer = work->stateTimer;
            }
            if (timer >= 4) {
                coord = arg0->extra.tmd->coords;
                y     = coord->coord.t[1];
                if (y < 0x2EE0) {
                    coord->coord.t[1] = y + ((timer - 3) * 0x21);
                }
            }
            if (work->stateTimer == 0x64) {
                worldCollisionUnlinkBody(&work->spheres[DESERT_CHASER_SPHERE_FRONT].body);
                worldCollisionUnlinkBody(&work->spheres[DESERT_CHASER_SPHERE_REAR].body);
                worldCollisionUnlinkBody(&work->wallProbe.body);
                worldCollisionUnlinkBody(&work->spheres[DESERT_CHASER_SPHERE_ROOT].body);
                ctx->recs         = 0;
                ctx->hp           = 0;
                hiddenObj         = arg0->extra.tmd;
                hiddenObj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            if (work->stateTimer == 0x65) {
                finishedObj         = arg0->extra.tmd;
                finishedObj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            }
            if ((work->stateTimer >= 0x79) && (work->playerHeld != 1) && (Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL) && gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, (s32)(ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT), 0);
                work->deathPending = 1;
                arg0->state++;
            }

            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x14);
}

/// Per-state handlers the per-frame update calls, indexed by the work block's
/// `state`, called unconditionally; a null entry is a state with no handler.
static const DesertChaserStateTable Actor00100_D000F0 = { {
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
    PlayerStatus*          config;
    s32                    excludedState;
    VECTOR                 pos;
    DesertChaserStateTable states;
    GfxCoord*              actorcoord;

    SVECTOR**             scratchHead;
    SVECTOR*              scratch;
    s32                   state;
    s16                   modeState;
    s16                   height;
    s16                   modeHeight;
    s16                   initialState;
    s16                   finalState;
    s16                   i;
    GfxCoord*             playerCoord;
    s32                   sound;
    s32                   sound2;
    s32                   depth;
    s32                   result;
    s32                   action;
    s32                   nextAction;
    s32                   pan2;
    s32                   pan;
    AnimationSet**        nextPlayerSets;
    DesertChaserWork*     actorWork;
    Task*                 playerSlot;
    DesertChaserWork*     work;
    Task*                 player;
    AnimationSet**        playerSets;
    Task*                 slot;
    GfxCoord*             coord;
    AnimationPlayRequest* message;
    AnimationPlayRequest* nextMessage;

    work                                   = actor->work;
    player                                 = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    config                                 = &gPlayerStatus;
    states                                 = Actor00100_D000F0;
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(actor->extra.tmd->coords);
    pos.vx = actor->extra.tmd->coords->workm.t[0];
    pos.vy = actor->extra.tmd->coords->workm.t[1];
    pos.vz = actor->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            initialState = work->state;
            if (initialState != 21 && initialState != 0 && initialState != 6 && initialState != 3) {
                actor->extra.tmd->flags = 0;
                height                  = actor->extra.tmd->coords->coord.t[1];
                Actor00100_Fn01900(actor, 1, 3, 0x12C, (s32)height, 0xFF);
                Actor00100_Fn01900(actor, 3, 4, 0xC8, (s32)height, 0xFF);
                Actor00100_Fn01900(actor, 1, 0xB, 0xFA, (s32)height, 0xFF);
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            modeState = work->state;
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
    } else if (work->state != 5) {
        Actor00100_Fn0375C(actor);
    }
    if (work->prevState != work->state) {
        work->stateEntered = 1;
    } else {
        work->stateEntered = 0;
    }
    work->prevState = (s16)(u16)work->state;
    excludedState   = 21;
    state           = work->state;
    if (work->playerHeld == 1) {
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
        action                 = work->playerAnim.animationId;
        work->playerAnimFrames = (u16)(work->playerAnimFrames + 1);
        switch (action) {
            case 4:
                break;
            case 1:
                if (work->playerMove.collisionRequests == (GAME_ACTOR_COLLISION_REQUEST_MASK << GAME_ACTOR_COLLISION_DISABLE_REQUEST_SHIFT)) {
                    TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_MOVE_BY, &work->playerMove, 0);
                    if (work->playerAnimFrames == 0xF) {
                        if ((viewGetMappedIndex() & 0xFF) == 8) {
                            sound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54010004;
                            pan   = (s8)worldCoordGetOriginAudioPan(actor->extra.tmd->coords);
                            depth = worldCoordGetOriginAudioDepth(actor->extra.tmd->coords);
                            sndEvtRequestScriptStart(sound, (s8)pan, (s8)(depth + abs(worldCoordGetOriginAudioPan(actor->extra.tmd->coords)) / 2));
                        } else {
                            sound2 = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54010004;
                            pan2   = (s8)worldCoordGetOriginAudioPan(actor->extra.tmd->coords);
                            sndEvtRequestScriptStart(sound2, pan2, (s8)worldCoordGetOriginAudioDepth(actor->extra.tmd->coords));
                        }
                    }
                    if (work->playerAnimFrames >= 0xF) {
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
                        if (work->playerAnimFrames >= 0xF) {
                            work->playerMove.displacement.vx >>= 1;
                            work->playerMove.displacement.vy >>= 1;
                            work->playerMove.displacement.vz >>= 1;
                        }
                    } else if (((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 1, 0, 0)) && (work->playerMove.displacement.vx != 0) && (work->playerMove.displacement.vz != 0) && (work->playerAnimFrames < 6)) {
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
                playerSets = work->playerAnim.source.sets;
                if (playerSets == gDesertChaserRearAnim) {
                    if ((config->hp > 0) && (work->playerAnimFrames >= 0x17)) {
                        message                      = &work->playerAnim;
                        playerSets[4]                = Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon]->table.sets[7];
                        work->playerAnim.animationId = 4;
                        work->playerAnim.blend       = ANIMATION_BLEND_INTERPOLATE;
                        work->playerAnim.blendFrames = 3;
                        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, message, 0);
                        work->playerAnimFrames = 0U;
                    }
                } else if ((config->hp > 0) && (work->playerAnimFrames >= 0x22)) {
                    message                      = &work->playerAnim;
                    gDesertChaserFrontAnim[4]    = Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon]->table.sets[7];
                    work->playerAnim.animationId = 4;
                    work->playerAnim.blend       = ANIMATION_BLEND_INTERPOLATE;
                    work->playerAnim.blendFrames = 3;
                    TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, message, 0);
                    work->playerAnimFrames = 0U;
                }
                break;
            case 3:
                if ((work->playerAnimFrames < 6) && (config->hp > 0) && ((work->playerMove.displacement.vx != 0) || (work->playerMove.displacement.vz != 0))) {
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
                if ((config->hp > 0) && (work->playerAnimFrames >= 7)) {
                    taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 2, 0);
                    work->playerHeld = 0;
                }
                break;
        }
        if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
            nextAction = work->playerAnim.animationId;
            switch (nextAction) {
                case 1:
                    if ((((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(4, 1, 0, 0)) || (work->playerMove.collisionRequests != (GAME_ACTOR_COLLISION_REQUEST_MASK << GAME_ACTOR_COLLISION_DISABLE_REQUEST_SHIFT))) && (config->hp > 0)) {
                        nextMessage                  = &work->playerAnim;
                        work->playerAnim.blend       = ANIMATION_BLEND_RESET;
                        work->playerAnim.blendFrames = 0;
                        work->playerAnim.animationId = 2;
                        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, nextMessage, 0);
                        work->playerAnimFrames = 0U;
                    }
                    break;
                case 3:
                    if (config->hp > 0) {
                        work->playerAnim.blend       = ANIMATION_BLEND_INTERPOLATE;
                        work->playerAnim.blendFrames = 6;
                        work->playerAnim.animationId = 5;
                        nextPlayerSets               = work->playerAnim.source.sets;
                        if (nextPlayerSets == gDesertChaserRearAnim) {
                            nextPlayerSets[5] = Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon]->table.sets[9];
                        } else {
                            gDesertChaserFrontAnim[5] = Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon]->table.sets[9];
                        }
                        nextMessage = &work->playerAnim;
                        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, nextMessage, 0);
                        work->playerAnimFrames = 0U;
                    }
                    break;
                case 4:
                case 7:
                    if (config->hp > 0) {
                        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 2, 0);
                        work->playerHeld = 0;
                    }
                    break;
            }
        }
    }
    states.handlers[work->state](actor);
    finalState = work->state;
    if ((finalState != 0x15) && (finalState != 0) && (finalState != 6) && (finalState != 5) && (finalState != 3)) {
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->spheres[DESERT_CHASER_SPHERE_REAR].body.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->spheres[DESERT_CHASER_SPHERE_REAR].body.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    worldCollisionClearContacts(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts);
    worldCollisionClearContacts(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts);
    worldCollisionClearContacts(work->spheres[DESERT_CHASER_SPHERE_REAR].contacts);
    worldCollisionClearContacts(work->wallProbe.contacts);
    if ((work->deathPending == 1) && (work->playerHeld == 0)) {
        work->deathPending = 0;
        Gp_ReleaseStateF0Add(actor, 1);
    }
    if (work->chaseHoldoff > 0) {
        work->chaseHoldoff = (s16)((u16)work->chaseHoldoff - 1);
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
    DesertChaserWork* work  = arg0->work;
    s16               found = 0;
    s16               i;
    s32               value;

    for (i = 0; i < 5; i++) {
        value = work->wallProbe.contacts[i].key.value;
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
            spawn                 = 1;
            work->effectOffset.vz = 0;
            work->effectOffset.vx = 0;
            work->effectOffset.vy = 0;
            break;
        case 9:
            spawn                 = 1;
            work->effectOffset.vz = 0;
            work->effectOffset.vx = 0;
            work->effectOffset.vy = 0x2BC;
            break;
        case 7:
            spawn                 = 1;
            work->effectOffset.vz = 0;
            work->effectOffset.vx = 0;
            work->effectOffset.vy = 0x2BC;
            break;
        case 14:
        case 17:
            spawn                 = 1;
            work->effectOffset.vz = 0;
            work->effectOffset.vx = 0;
            work->effectOffset.vy = 0x258;
            break;
        default:
            spawn = 0;
            break;
    }

    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && spawn == 1) {
        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[arg1], arg2 | 0x80000000, &work->effectOffset);
    }
}

static void Actor00100_Fn0B4D8(Task* arg0)
{
    TmdObject*        obj;
    DesertChaserWork* work;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                                               |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags      &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
    }
}

#include "../../shared/desert_chaser_stunned.inc.c"

static void Actor00100_Fn0B658(Task* arg0)
{
    TmdObject*        obj;
    DesertChaserWork* work;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animId                                          = 8;
        work->blendActive                                     = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->animRate                                        = work->baseRate;
        desertChaserAnimTick(arg0);
        Gp_ArmStateF0(1);
    }
    ActorContact_PushContact(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts));
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    desertChaserAnimTick(arg0);
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        work->state = 0x1C;
    }
}

static void Actor00100_Fn0B730(Task* arg0)
{
    DesertChaserWork* work;
    TmdObject*        obj;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animId                                          = 0xC;
        work->blendActive                                     = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->animRate                                        = work->baseRate;
        desertChaserAnimTick(arg0);
    }
    desertChaserAnimTick(arg0);
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        work->state = 0x26;
    }
}

#include "../../shared/desert_chaser_flinch.inc.c"

static void Actor00100_Fn0B8D8(Task* arg0)
{
    DesertChaserWork* work;
    u16               timer;
    u32               random;

    work = arg0->work;
    if (work->stateEntered != 0) {
        random           = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState  = random;
        work->stateTimer = work->downFramesBase + ((random >> 0x10) & 0xF);
    }
    desertChaserAnimTick(arg0);
    timer            = work->stateTimer - 1;
    work->stateTimer = timer;
    if ((s16)timer < 0) {
        if (((Enemy*)arg0->spawnArg2.pointer)->hp > 0) {
            work->state = 0x24;
        } else {
            work->state = 0x15;
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
    if (work->stateEntered != 0) {
        obj                                                   = arg0->extra.tmd;
        work->hitFlag                                         = 0;
        obj->flags                                            = 0;
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        ctx->node.state.parts.flags                           = 0;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_RESET;
        work->animId                                          = 0x14;
        work->animRate                                        = 0x10;
        work->lookYawTarget                                   = 0;
        work->waistYawTarget                                  = 0;
        if (ctx->hp <= 0) {
            sceneSetEnemyAlert(1);
        }
    }
    desertChaserAnimTick(arg0);
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        if (ctx->hp > 0) {
            if (ctx->reactionFlags & ENEMY_REACTION_BUILDUP) {
                work->state = 4;
            } else {
                work->state = 0x24;
            }
        } else {
            work->state = 0x15;
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

    work = task->work;
    if (work->deathPending == 1) {
        work->deathPending = 0;
        Gp_ReleaseStateF0Add(task, 1);
    }
    if (work->deathPending == 0) {
        task->state++;
    }
}

#include "../../shared/desert_chaser_task.inc.c"
