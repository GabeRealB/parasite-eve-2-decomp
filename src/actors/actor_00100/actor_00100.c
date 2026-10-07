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
#include "../../shared/limb_shadows.h"
#include "../../shared/player_detection.h"
#include "../../shared/actor_messages.h"
#include "../../shared/actor_contacts.h"
#define DESERT_CHASER_BUILD DESERT_CHASER_REGULAR
#include "../../shared/desert_chaser.h"

/// Mine Mesa fall gates in root-parent coordinates; yaw uses 4096 units per turn.
enum {
    ACTOR00100_MESA_DROP_MIN_X      = 0x1541,
    ACTOR00100_MESA_DROP_X_EXTENT   = 0x196D,
    ACTOR00100_MESA_DROP_MAX_Z      = 0x5B4,
    ACTOR00100_MESA_DROP_CORNER_X   = 0x2AF9,
    ACTOR00100_MESA_DROP_MIN_YAW    = 0x501,
    ACTOR00100_MESA_DROP_CORNER_YAW = 0x708,
};

/// Package message and hit-sound script identities.
enum {
    ACTOR00100_MESSAGE_IGNORE_2015 = 2015,
    ACTOR00100_SOUND_LIGHT_HIT     = 0x40010007,
    ACTOR00100_SOUND_HEAVY_HIT     = 0x40010008
};

/// Regular-build state selectors used by commands and damage reactions.
enum {
    ACTOR00100_STATE_BURST_DEATH         = 3,
    ACTOR00100_STATE_LEAP_IN             = 5,
    ACTOR00100_STATE_STAGGER             = 7,
    ACTOR00100_STATE_COLLAPSE            = 10,
    ACTOR00100_STATE_DOWNED_HIT          = 11,
    ACTOR00100_STATE_KNOCK_DOWN          = 20,
    ACTOR00100_STATE_APPROACH            = 24,
    ACTOR00100_STATE_PURSUE              = 28,
    ACTOR00100_STATE_WATCH_PLAYER_FACING = 32,
    ACTOR00100_STATE_STEER               = 33,
    ACTOR00100_STATE_RESUME_PURSUIT      = 34,
};

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

/// Returns this carrier's persistent last contact-push correction.
///
/// Components are signed 16.16 corrections shifted right by 16 and narrowed
/// to halfwords. Fractional X/Z add a further unit in the correction's sign;
/// X/Z record the root correction, while Y is only recorded. No grid hit
/// leaves the old value intact. The borrowed vector lives for the overlay's
/// lifetime; `pad` is unused.
static inline SVECTOR* _actorContactGetLastPushStep(void)
{
    return &ActorContact_ScratchPosition;
}

extern AnimationSet* gDesertChaserRearAnim[9];

extern TaskMessageEntry Actor00100_D1BA54[6];

/// Twelve `SVECTOR` hit positions `_desertChaserHitEffect` picks from by relative
/// hit yaw. The fourth halfword (`pad`, unused by the effect) is the model
/// part index the spawned effect anchors to.
extern SVECTOR gDesertChaserHitOffsets[12];

static void _actor00100ResumePursuit(Task* task);

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

static s32 _actor00100ApplyCommand(Task* task, s32 messageId, const ActorCommand* request, s32 unusedExtra);

static s32 _actor00100IgnoreMessage2015(Task* task, s32 messageId, s32 unusedPayload, s32 unusedExtra);

extern s8 gDesertChaserClipStartFrames[25][25];

static const DesertChaserTaskStates gDesertChaserTaskStates;

static void _actor00100HideState(Task* task);

static void _actor00100Rise(Task* task);

static void _actor00100WaitDowned(Task* task);

static void _actor00100DownedHitReaction(Task* task);

static void _actor00100IdleState2(Task* task);

static void _actor00100ReleaseBattleState(Enemy* enemy, Task* task);

static __inline__ s16  _actor00100FacesMesaDrop(GfxCoord* coord);
static __inline__ void _actorScaleMatrix(MATRIX* matrix, s16 scale);
static __inline__ s16  _actor00100IsInMesaDropRegion(Task* actor);
static __inline__ s16  _actor00100MovesTowardMesaDrop(Task* actor, VECTOR* motion);
static __inline__ s32  _actor00100FindAttackContact(WorldCollisionContact* contacts, SVECTOR* hitPosition);
static __inline__ void _actor00100SelectHitReaction(DesertChaserWork* work);
static s32             desertChaserAvoidWalk(GfxCoord* coord, WorldCollisionContact* recs, s16 count, SVECTOR* pos);

static void Actor00100_Fn02C54(Enemy* arg0, Task* arg1);
static void _desertChaserDamage(Task* task);
static void Actor00100_Fn04270(Task* arg0);
static void Actor00100_Fn061FC(Task* arg0);
static void _actor00100LeapBack(Task* task);
static void Actor00100_Fn070DC(Task* arg0);
static void _actor00100KnockDownWithDamage(Task* task);
static void Actor00100_Fn08E7C(Task* arg0);
static void _actor00100BurstDeath(Task* actor);
static void Actor00100_Fn09724(Task* arg0);
static void Actor00100_Fn09CCC(Task* arg0);
static void Actor00100_Fn0A288(Enemy* enemy, Task* actor);
static s16  _actor00100WallProbeTouchesGrid(Task* task);
static void _actor00100SpawnPartDust(Task* task, s16 jointIndex, s16 dustArgs);
static void _actor00100AlignPlayerHeight(Task* task);

/// Tests whether a root faces through the Mine Mesa fall boundary.
///
/// The caller first checks the Mesa room and drop region. Yaw uses 4096 units
/// per turn; the boundary changes direction at root X = 11001.
static __inline__ s16 _actor00100FacesMesaDrop(GfxCoord* coord)
{
    s16 yaw          = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    s32 yawMagnitude = yaw;
    if (yawMagnitude < 0)
        yawMagnitude = -yawMagnitude;
    if (yawMagnitude >= ACTOR00100_MESA_DROP_MIN_YAW) {
        if (coord->coord.t[0] < ACTOR00100_MESA_DROP_CORNER_X)
            return 1;
        if ((s16)ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]) >= ACTOR00100_MESA_DROP_CORNER_YAW)
            return 1;
        if ((s16)ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]) <= 0)
            return 1;
    }
    return 0;
}

/// Uniformly scales an actor matrix's basis and translation by a signed 4.12 factor.
///
/// Translation narrows to signed halfwords before the GTE multiply and returns
/// saturated halfwords. Reserves and releases one scratch block; `matrix` must
/// remain writable through the call. 4096 represents a factor of one.
static __inline__ void _actorScaleMatrix(MATRIX* matrix, s16 scale)
{
    ActorScaleMatrixScratch* scratchHead;
    ActorScaleMatrixScratch* scratch;
    SVECTOR*                 translation;

    scratchHead                                   = SCRATCH_STACK_CURSOR(ActorScaleMatrixScratch);
    scratch                                       = scratchHead - 1;
    SCRATCH_STACK_CURSOR(ActorScaleMatrixScratch) = scratch;
    scratch->scale.vz                             = scale;
    scratch->scale.vy                             = scale;
    scratchHead[-1].scale.vx                      = scale;
    ScaleMatrix(matrix, &scratch->scale);
    scratch->translation.vx = matrix->t[0];
    scratch->translation.vy = matrix->t[1];
    scratch->translation.vz = matrix->t[2];
    gte_lddp(scale);
    translation = &scratch->translation;
    gte_ldsv(translation);
    gte_gpf12();
    gte_stsv(translation);
    matrix->t[0] = scratch->translation.vx;
    matrix->t[1] = scratch->translation.vy;
    matrix->t[2] = scratch->translation.vz;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleMatrixScratch);
}

/// Tests the Mine Mesa drop region in the actor root's parent space.
///
/// X lies in [5441, 11949] and Z is below 1460, with no lower Z bound.
/// The unsigned X subtraction implements both ends of the interval.
static __inline__ s16 _actor00100IsInMesaDropRegion(Task* actor)
{
    GfxCoord* coord = actor->extra.tmd->coords;
    if ((u32)(coord->coord.t[0] - ACTOR00100_MESA_DROP_MIN_X) < (u32)ACTOR00100_MESA_DROP_X_EXTENT) {
        if (coord->coord.t[2] < ACTOR00100_MESA_DROP_MAX_Z)
            return 1;
    }
    return 0;
}

/// Tests whether a displacement heads through the Mine Mesa fall boundary.
///
/// The caller checks the room and drop region first. X/Z displacement and root
/// translation share parent-coordinate units; its bearing uses 4096 per turn.
static __inline__ s16 _actor00100MovesTowardMesaDrop(Task* actor, VECTOR* motion)
{
    GfxCoord* coord = actor->extra.tmd->coords;
    if (abs((s16)ratan2(motion->vx, motion->vz)) >= ACTOR00100_MESA_DROP_MIN_YAW) {
        if (coord->coord.t[0] < ACTOR00100_MESA_DROP_CORNER_X)
            return 1;
        if ((s16)ratan2(motion->vx, motion->vz) >= ACTOR00100_MESA_DROP_CORNER_YAW)
            return 1;
        if ((s16)ratan2(motion->vx, motion->vz) <= 0)
            return 1;
    }
    return 0;
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
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor00100ApplyCommand },
    { ACTOR00100_MESSAGE_IGNORE_2015, _actor00100IgnoreMessage2015 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetVisibility },
    { ACTOR_MESSAGE_IS_PRESENT, actorMsgIsPresent },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceRecordYaw },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc Actor00100_D1BA84 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, _desertChaserTask, { .model = &_gActor00100DesertChaserBody } };

SVECTOR ActorContact_ScratchPosition;

/// Returns the first attack contact key and copies its world-space hit point.
///
/// `contacts` has DESERT_CHASER_CONTACTS entries, as each regular-build sphere
/// provides. A zero key ends the scan. Returns zero without writing
/// `hitPosition` when there is no attack; coordinates narrow to signed halfwords
/// and the output vector's fourth halfword stays untouched.
static __inline__ s32 _actor00100FindAttackContact(WorldCollisionContact* contacts, SVECTOR* hitPosition)
{
    s16 contactIndex;
    for (contactIndex = 0; contactIndex < DESERT_CHASER_CONTACTS; contactIndex++) {
        if (!contacts[contactIndex].key.value)
            break;
        if ((contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_ATTACK) {
            hitPosition->vx = contacts[contactIndex].point.vx;
            hitPosition->vy = contacts[contactIndex].point.vy;
            hitPosition->vz = contacts[contactIndex].point.vz;
            return contacts[contactIndex].key.value;
        }
    }
    return 0;
}

/// Chooses the stagger or flinch state for a new hit.
///
/// Buildup, stagger, steering, flinch, rising, down, and recovery with a state
/// timer below ten take the stagger reaction; other states take the flinch reaction.
static __inline__ void _actor00100SelectHitReaction(DesertChaserWork* work)
{
    enum {
        ACTOR00100_STATE_BUILDUP             = 4,
        ACTOR00100_STATE_STAGGER             = 7,
        ACTOR00100_STATE_RISE                = 11,
        ACTOR00100_STATE_DOWN                = 17,
        ACTOR00100_STATE_FLINCH              = 20,
        ACTOR00100_STATE_STEER               = 33,
        ACTOR00100_STATE_RECOVER             = 36,
        ACTOR00100_RECOVER_HIT_WINDOW_FRAMES = 10,
    };

    s32 state = work->state;
    if (state == ACTOR00100_STATE_BUILDUP || state == ACTOR00100_STATE_STAGGER || state == ACTOR00100_STATE_STEER || state == ACTOR00100_STATE_FLINCH || state == ACTOR00100_STATE_RISE || state == ACTOR00100_STATE_DOWN || (state == ACTOR00100_STATE_RECOVER && work->stateTimer < ACTOR00100_RECOVER_HIT_WINDOW_FRAMES))
        work->state = ACTOR00100_STATE_STAGGER;
    else
        work->state = ACTOR00100_STATE_FLINCH;
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

/// Applies pack coordination and room commands to the regular Desert Chaser.
///
/// Borrows `request` through synchronous dispatch. Stage 9/area 1 commands
/// control pursuit holdoff and steering and return 1, including unknown commands.
/// Other contexts cache stage, area and the low command byte and return 0:
/// Mine Mesa can hide or place a leap-in, the Dryfield breezeway can hide or
/// start pursuit with the fourth tuning, and Main Street can hide or place a
/// roaming chaser. The message ID and extra word are unused.
static s32 _actor00100ApplyCommand(Task* task, s32 messageId, const ActorCommand* request, s32 unusedExtra)
{
    enum {
        ACTOR00100_PACK_CONTEXT                 = 9 | (1 << 8),
        ACTOR00100_MESA_CONTEXT                 = GAME_STAGE_MINE_SHELTER | (GAME_AREA_MINE_MESA << 8),
        ACTOR00100_BREEZEWAY_CONTEXT            = GAME_STAGE_DRYFIELD | (GAME_AREA_DRYFIELD_BREEZEWAY << 8),
        ACTOR00100_MAIN_STREET_CONTEXT          = GAME_STAGE_DRYFIELD | (GAME_AREA_DRYFIELD_MAIN_STREET << 8),
        ACTOR00100_PACK_COMMAND_HOLDOFF         = 1,
        ACTOR00100_PACK_COMMAND_KEEP_ROAM       = 2,
        ACTOR00100_PACK_COMMAND_RESTART_HOLDOFF = 3,
        ACTOR00100_PACK_COMMAND_BREAK_OFF       = 4,
        ACTOR00100_ROOM_COMMAND_HIDE            = 0,
        ACTOR00100_MESA_COMMAND_LEAP_IN         = 1,
        ACTOR00100_ROOM_COMMAND_START           = 2,
        ACTOR00100_PACK_HOLDOFF_FRAMES          = 90,
        ACTOR00100_BREEZEWAY_VARIANT            = 3,
        ACTOR00100_SOUND_BREEZEWAY_WAKE         = 0x52160009,
    };
    _Actor00100MesaPlacements placements;
    _Actor00100MesaPlacement* placement;
    DesertChaserWork*         work;
    Enemy*                    enemy;
    s8                        placementIndex;
    s32                       viewIndex;
    u32                       randomView2;
    u32                       randomView4;
    u32                       randomView5;
    u32                       randomViews3And8;
    u32                       randomOtherView;
    s32                       packCommand;
    s32                       mesaCommand;
    s32                       breezewayCommand;
    s32                       mainStreetCommand;
    s32                       wakeSound;
    s32                       audioPan;

    work  = task->work;
    enemy = task->spawnArg2.pointer;

    if (request->context.key == ACTOR00100_PACK_CONTEXT) {
        packCommand = request->command;
        switch (packCommand) {
            case ACTOR00100_PACK_COMMAND_HOLDOFF:
                work->chaseHoldoff = ACTOR00100_PACK_HOLDOFF_FRAMES;
                break;
            case ACTOR00100_PACK_COMMAND_KEEP_ROAM:
                if (work->state == DESERT_CHASER_STATE_ROAM) {
                    work->state = DESERT_CHASER_STATE_ROAM;
                }
                break;
            case ACTOR00100_PACK_COMMAND_RESTART_HOLDOFF:
                work->chaseHoldoff = work->chaseHoldoffFrames;
                break;
            case ACTOR00100_PACK_COMMAND_BREAK_OFF:
                if (work->state == ACTOR00100_STATE_STEER) {
                    work->state = ACTOR00100_STATE_RESUME_PURSUIT;
                }
                if (work->state == ACTOR00100_STATE_APPROACH) {
                    work->state = DESERT_CHASER_STATE_ROAM;
                }
                break;
        }
        return 1;
    } else {
        // Room commands are remembered by value; only the low command byte is retained.
        work->lastCommand.fields.stage   = request->context.loc.stage;
        work->lastCommand.fields.area    = request->context.loc.area;
        work->lastCommand.fields.command = (u8)request->command;
        if (request->context.key == ACTOR00100_MESA_CONTEXT) {
            placements  = Actor00100_D00004;
            mesaCommand = request->command;
            switch (mesaCommand) {
                case ACTOR00100_ROOM_COMMAND_HIDE:
                    work->state = DESERT_CHASER_STATE_HIDDEN;
                    break;
                case ACTOR00100_MESA_COMMAND_LEAP_IN:
                    viewIndex = viewGetMappedIndex() & 0xFF;
                    // Keep each camera branch's random draw independent.
                    switch (viewIndex) {
                        case 2:
                            randomView2     = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                            gRandomLcgState = randomView2;
                            placementIndex  = ((randomView2 >> 0x10) % 3) + 1;
                            break;
                        case 4:
                            randomView4     = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                            gRandomLcgState = randomView4;
                            placementIndex  = 1;
                            if (((randomView4 >> 0x10) & 1) == 0) {
                                placementIndex = 3;
                            }
                            break;
                        case 5:
                            randomView5     = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                            placementIndex  = (randomView5 >> 0x10) & 1;
                            gRandomLcgState = randomView5;
                            break;
                        case 3:
                        case 8:
                            randomViews3And8 = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                            placementIndex   = ((randomViews3And8 >> 0x10) & 1) | 2;
                            gRandomLcgState  = randomViews3And8;
                            break;
                        default:
                            randomOtherView = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                            placementIndex  = (randomOtherView >> 0x10) & 3;
                            gRandomLcgState = randomOtherView;
                    }
                    placement                           = &placements.placements[placementIndex];
                    task->extra.tmd->coords->coord.t[0] = placement->x;
                    task->extra.tmd->coords->coord.t[1] = placement->y;
                    task->extra.tmd->coords->coord.t[2] = placement->z;
                    gfxRotMatrixY(&task->extra.tmd->coords->coord, placement->yaw, 1);
                    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                    work->state                           = ACTOR00100_STATE_LEAP_IN;
                    break;
            }
        }
        if (request->context.key == ACTOR00100_BREEZEWAY_CONTEXT) {
            breezewayCommand = request->command;
            switch (breezewayCommand) {
                case ACTOR00100_ROOM_COMMAND_HIDE:
                    work->state = DESERT_CHASER_STATE_HIDDEN;
                    break;
                case ACTOR00100_ROOM_COMMAND_START:
                    wakeSound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR00100_SOUND_BREEZEWAY_WAKE;
                    audioPan  = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
                    sndEvtRequestScriptStart(wakeSound, audioPan,
                                             (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
                    work->animId      = breezewayCommand;
                    work->animRequest = breezewayCommand;
                    _desertChaserAnimTick(task);
                    _desertChaserAnimTick(task);
                    work->state              = ACTOR00100_STATE_PURSUE;
                    work->windupFrames       = Actor00100_D0BDB4[ACTOR00100_BREEZEWAY_VARIANT].windupFrames;
                    work->downFramesBase     = Actor00100_D0BDB4[ACTOR00100_BREEZEWAY_VARIANT].downFramesBase;
                    work->roamLookDelay      = Actor00100_D0BDB4[ACTOR00100_BREEZEWAY_VARIANT].roamLookDelay;
                    work->chaseHoldoffFrames = Actor00100_D0BDB4[ACTOR00100_BREEZEWAY_VARIANT].chaseHoldoffFrames;
                    break;
            }
        }
        if (request->context.key == ACTOR00100_MAIN_STREET_CONTEXT) {
            mainStreetCommand = request->command;
            switch (mainStreetCommand) {
                case ACTOR00100_ROOM_COMMAND_HIDE:
                    work->state = DESERT_CHASER_STATE_HIDDEN;
                    break;
                case ACTOR00100_ROOM_COMMAND_START:
                    work->state                         = DESERT_CHASER_STATE_ROAM;
                    task->extra.tmd->coords->coord.t[0] = -0x896;
                    task->extra.tmd->coords->coord.t[1] = 0;
                    task->extra.tmd->coords->coord.t[2] = 0x5AF;
                    gfxRotMatrixY(&task->extra.tmd->coords->coord, -0x3F4, 1);
                    break;
            }
        }
        return 0;
    }
}

/// Collects bearings from the obstacles in `recs` into a
/// `DesertChaserAvoidScratch` and steps `coord` along each survivor. Same walk
/// as `_actorContactApplyAvoidancePushback`, but `blocked` is raised only for a kind 0x10000
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
            s->bearing[s->count] = _actorAngleBearingXZ(&recs[s->i].point, &s->origin);
        } else {
            s->bearing[s->count] = _actorAngleBearingXY(&recs[s->i].point, &s->origin);
        }
        s->kept[s->count] = 1;
        s->count++;
        if (s->count >= ARRAY_SIZE(s->bearing)) {
            break;
        }
    }

    for (s->i = 0; s->i < s->count; s->i++) {
        for (s->j = s->i + 1; s->j < s->count; s->j++) {
            s->diff = _actorAngleNormalizeYaw(s->bearing[s->i] - s->bearing[s->j]);
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

#include "../../shared/limb_shadows_segment.inc.c"

/// Advances the two animation rigs and mixes secondary rotation into slots 1..10.
///
/// The regular build uses 3/4 main weight on slots 1..3, 1502/4096 on
/// slots 4..5, and 3024/4096 on slots 6..10. All slots 1..17
/// advance the main rig at `animRate - 3`; the blended slots also advance the
/// secondary rig at `blendRate`. Rates use sixteenths of a frame, and signed
/// negative rates are retained. Both rigs and their borrowed clip data must be live.
static void _desertChaserBlendTick(Task* task)
{
    AnimationPose     mainPose;
    AnimationPose     blendPose;
    AnimationContext* anim;
    s16               partIndex;
    s16               slotIndex;
    s32               mainWeight;
    s32               blendWeight;
    DesertChaserWork* work;

    work = task->work;
    anim = &work->rig.anim;
    // Leave slot 0 intact; only slots 1..10 mix the secondary rotation.
    for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
        partIndex = slotIndex - 1;
        switch (partIndex) {
            case 0:
                mainWeight = DESERT_CHASER_BLEND_WEIGHT_THREE_QUARTERS;
                break;
            case 1:
                mainWeight = DESERT_CHASER_BLEND_WEIGHT_THREE_QUARTERS;
                break;
            case 2:
                mainWeight = DESERT_CHASER_BLEND_WEIGHT_THREE_QUARTERS;
                break;
            case 3:
                mainWeight = DESERT_CHASER_BLEND_WEIGHT_REDUCED;
                break;
            case 4:
                mainWeight = DESERT_CHASER_BLEND_WEIGHT_REDUCED;
                break;
            default:
                mainWeight = DESERT_CHASER_BLEND_WEIGHT_DEFAULT;
                break;
        }
        blendWeight = DESERT_CHASER_BLEND_WEIGHT_ONE - mainWeight;
        if (slotIndex < DESERT_CHASER_BLEND_FIRST_UNBLENDED_SLOT) {
            work->blend.slots[slotIndex].rate = work->blendRate;
            work->rig.slots[slotIndex].rate   = work->animRate - DESERT_CHASER_BLEND_MAIN_RATE_BIAS;
            animationTickSlotPose(anim, (s32)slotIndex, &mainPose, NULL);
            animationTickSlotPose(&work->blend.anim, (s32)slotIndex, &blendPose, NULL);
            animationApplyPoseWithBlendedRotation(anim, (s32)slotIndex, &mainPose, &blendPose, mainWeight, blendWeight);
        } else {
            work->rig.slots[slotIndex].rate = work->animRate - DESERT_CHASER_BLEND_MAIN_RATE_BIAS;
            animationTickSlot(&work->rig.anim, (s32)slotIndex);
        }
    }
}

/// Stores a dust origin offset along a model part's local Y axis.
///
/// Requires a live regular-chaser work block. `yOffset` uses model-part
/// coordinate units and narrows to signed 16 bits. X and Z are cleared;
/// the vector's unused `pad` halfword is retained. Callers stage the offset
/// even when room effects are disabled; effect placement copies it at spawn.
static inline void _actor00100StageDustOffset(DesertChaserWork* work, s32 yOffset)
{
    work->effectOffset.vz = 0;
    work->effectOffset.vx = 0;
    work->effectOffset.vy = yOffset;
}

/// Stages a local vertical offset and emits one animation dust cue at a model part.
///
/// Requires a live regular-chaser task and a part index in 0..17. `height`
/// uses model-part coordinate units and narrows to a signed halfword. `dustArgs`
/// packs size in bits 0..11, frame period in 12..15 (zero means one), and child
/// recursion in bit 31. The offset is staged even when room effects are disabled;
/// placement copies it during spawning and dust playback does not read it again.
static inline void _actor00100SpawnCueDust(Task* task, s32 jointIndex, s32 height, s32 dustArgs)
{
    DesertChaserWork* work = task->work;

    _actor00100StageDustOffset(work, height);
    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[jointIndex], dustArgs, &work->effectOffset);
    }
}

/// Emits one-shot animation dust cues and returns an EVT sound script, or zero.
///
/// The armed builds test slot 1. The low ten pose record-index bits select a
/// cue; per-slot history suppresses repeats until a non-cue clears the history.
/// The task and work must belong to the same live chaser, with slots and model
/// parts 0..17 available. Dust offsets use model-part units and are borrowed
/// for placement; dust playback does not follow the retained offset pointer.
/// Sound placement and panning belong to the caller.
static s32 _desertChaserAnimCues(Task* task, DesertChaserWork* work)
{
    u32 previousCueIndex;
    s32 resetCueHistory = 1;

    // The low ten record-index bits identify cues, not elapsed animation frames.
    switch (work->animId) {
        case 0:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 9) {
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != 9) {
                    work->lastCueFrames[1] = 9;
                    _actor00100SpawnCueDust(task, 17, 0x258, (DESERT_CHASER_CUE_DUST_RECURSIVE | (2 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 640));
                    _actor00100SpawnCueDust(task, 9, 0x2BC, (DESERT_CHASER_CUE_DUST_RECURSIVE | (2 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 288));
                    return DESERT_CHASER_SOUND_STEP_2;
                }
                work->lastCueFrames[1] = previousCueIndex;
                resetCueHistory        = 0;
            }
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 6) {
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != 6) {
                    work->lastCueFrames[1] = 6;
                    _actor00100SpawnCueDust(task, 14, 0x258, (DESERT_CHASER_CUE_DUST_RECURSIVE | (2 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 544));
                    _actor00100SpawnCueDust(task, 7, 0x2BC, (DESERT_CHASER_CUE_DUST_RECURSIVE | (2 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 288));
                    return DESERT_CHASER_SOUND_STEP_1;
                }
                work->lastCueFrames[1] = previousCueIndex;
                resetCueHistory        = 0;
            }
            break;
        case 0xA:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0xA) {
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != 0xA) {
                    work->lastCueFrames[1] = 0xA;
                    _actor00100SpawnCueDust(task, 0, 0, (DESERT_CHASER_CUE_DUST_RECURSIVE | (4 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 2560));
                    return DESERT_CHASER_SOUND_CUE_05;
                }
                work->lastCueFrames[1] = previousCueIndex;
                resetCueHistory        = 0;
            }
            break;
        case 3:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0xC) {
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != 0xC) {
                    work->lastCueFrames[1] = 0xC;
                    return DESERT_CHASER_SOUND_CUE_04;
                }
                work->lastCueFrames[1] = previousCueIndex;
                resetCueHistory        = 0;
            }
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 8) {
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != 8) {
                    work->lastCueFrames[1] = 8;
                    return DESERT_CHASER_SOUND_CUE_03;
                }
                work->lastCueFrames[1] = previousCueIndex;
                resetCueHistory        = 0;
            }
            break;
        case 6:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 6) {
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != 6) {
                    work->lastCueFrames[1] = 6;
                    _actor00100SpawnCueDust(task, 9, 0x2BC, (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512));
                    _actor00100SpawnCueDust(task, 7, 0x2BC, (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512));
                    return DESERT_CHASER_SOUND_CUE_04;
                }
                work->lastCueFrames[1] = previousCueIndex;
                resetCueHistory        = 0;
            }
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0xC) {
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != 0xC) {
                    work->lastCueFrames[1] = 0xC;
                    _actor00100SpawnCueDust(task, 17, 0x258, (DESERT_CHASER_CUE_DUST_RECURSIVE | (4 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 1152));
                    _actor00100SpawnCueDust(task, 14, 0x258, (DESERT_CHASER_CUE_DUST_RECURSIVE | (4 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 1152));
                    return DESERT_CHASER_SOUND_CUE_11;
                }
                work->lastCueFrames[1] = previousCueIndex;
                resetCueHistory        = 0;
            }
            break;
        case 0x12:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 6) {
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != 6) {
                    work->lastCueFrames[1] = 6;
                    _actor00100SpawnCueDust(task, 9, 0x2BC, (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512));
                    _actor00100SpawnCueDust(task, 7, 0x2BC, (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512));
                    return DESERT_CHASER_SOUND_STEP_1;
                }
                work->lastCueFrames[1] = previousCueIndex;
                resetCueHistory        = 0;
            }
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 9) {
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != 9) {
                    work->lastCueFrames[1] = 9;
                    _actor00100SpawnCueDust(task, 9, 0x2BC, (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512));
                    _actor00100SpawnCueDust(task, 17, 0x258, (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512));
                    return DESERT_CHASER_SOUND_STEP_1;
                }
                work->lastCueFrames[1] = previousCueIndex;
                resetCueHistory        = 0;
            }
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0xE) {
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != 0xE) {
                    work->lastCueFrames[1] = 0xE;
                    _actor00100SpawnCueDust(task, 14, 0x258, (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512));
                    _actor00100SpawnCueDust(task, 17, 0x258, (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512));
                    return DESERT_CHASER_SOUND_STEP_2;
                }
                work->lastCueFrames[1] = previousCueIndex;
                resetCueHistory        = 0;
            }
            break;
        case 0x11:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 6) {
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != 6) {
                    work->lastCueFrames[1] = 6;
                    _actor00100SpawnCueDust(task, 9, 0x2BC, (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512));
                    _actor00100SpawnCueDust(task, 7, 0x2BC, (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512));
                    return DESERT_CHASER_SOUND_STEP_1;
                }
                work->lastCueFrames[1] = previousCueIndex;
                resetCueHistory        = 0;
            }
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0xA) {
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != 0xA) {
                    work->lastCueFrames[1] = 0xA;
                    _actor00100SpawnCueDust(task, 7, 0x2BC, (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512));
                    _actor00100SpawnCueDust(task, 14, 0x258, (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512));
                    return DESERT_CHASER_SOUND_STEP_1;
                }
                work->lastCueFrames[1] = previousCueIndex;
                resetCueHistory        = 0;
            }
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0xE) {
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != 0xE) {
                    work->lastCueFrames[1] = 0xE;
                    _actor00100SpawnCueDust(task, 14, 0x258, (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512));
                    _actor00100SpawnCueDust(task, 17, 0x258, (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512));
                    return DESERT_CHASER_SOUND_STEP_2;
                }
                work->lastCueFrames[1] = previousCueIndex;
                resetCueHistory        = 0;
            }
            break;
        case 0xD:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x18) {
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != 0x18) {
                    work->lastCueFrames[1] = 0x18;
                    return DESERT_CHASER_SOUND_CUE_0F;
                }
                work->lastCueFrames[1] = previousCueIndex;
                resetCueHistory        = 0;
            }
            break;
    }
    // An intervening non-cue record rearms the one-shot cue history.
    if (resetCueHistory == 1) {
        memFillBytes(work->lastCueFrames, 0U, sizeof(work->lastCueFrames));
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
    arg1->exitCallback = _desertChaserExit;
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
    _desertChaserAnimTick(arg1);
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
    worldCoordUpdateActorColor(arg0, &color, 0, 0);
    work->effectArg.coord      = &arg1->extra.tmd->coords[1];
    work->effectArg.spawnArgLo = 0x100;
    work->effectArg.spawnArgHi = 2;
    kind                       = (arg1->spawnArg1.value >> 16) & 0xF;
    switch (kind) {
        case 1:
            work->prevState = -1;
            work->state     = 0;
            break;
        case 2:
            work->prevState = -1;
            work->state     = 0x21;
            break;
        case 3:
            work->prevState = -1;
            work->state     = 5;
            break;
        case 0:
        default:
            work->prevState = -1;
            work->state     = 0x18;
            tmdAllocPrimitiveBuffer(tmd);
            break;
    }
    kind = arg1->spawnArg1.value & 0xF;
    switch (kind) {
        case 2:
            work->windupFrames       = Actor00100_D0BDB4[0].windupFrames;
            work->downFramesBase     = Actor00100_D0BDB4[0].downFramesBase;
            work->roamLookDelay      = Actor00100_D0BDB4[0].roamLookDelay;
            work->chaseHoldoffFrames = Actor00100_D0BDB4[0].chaseHoldoffFrames;
            break;
        case 1:
            work->windupFrames       = Actor00100_D0BDB4[2].windupFrames;
            work->downFramesBase     = Actor00100_D0BDB4[2].downFramesBase;
            work->roamLookDelay      = Actor00100_D0BDB4[2].roamLookDelay;
            work->chaseHoldoffFrames = Actor00100_D0BDB4[2].chaseHoldoffFrames;
            break;
        case 0:
        default:
            work->windupFrames       = Actor00100_D0BDB4[1].windupFrames;
            work->downFramesBase     = Actor00100_D0BDB4[1].downFramesBase;
            work->roamLookDelay      = Actor00100_D0BDB4[1].roamLookDelay;
            work->chaseHoldoffFrames = Actor00100_D0BDB4[1].chaseHoldoffFrames;
            break;
    }
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

/// Starts a sound script for this enemy placement at the model root.
///
/// `script` is an EVT script word with placement bits 8..15 clear. The enemy
/// placement index supplies those bits; pan and depth narrow to signed bytes.
static inline void _actor00100PlayPlacedSound(Task* task, Enemy* enemy, s32 script)
{
    s32 soundId;
    s32 audioPan;

    soundId  = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | script;
    audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
}

/// Requests a hit reaction blended over the chaser's current primary clip.
///
/// Requires a live regular-chaser work block. The next animation update
/// restarts secondary clip 15, including when a hit blend is already active.
/// Only slots 1..10 mix its rotation; secondary slot 1 settling ends the blend.
static inline void _actor00100RequestHitBlend(DesertChaserWork* work)
{
    enum { ACTOR00100_CLIP_BLEND_HIT = 15 };
    work->blendActive  = 1;
    work->blendAnimId  = ACTOR00100_CLIP_BLEND_HIT;
    work->blendRequest = DESERT_CHASER_ANIM_REQUEST_RESET;
}

/// Takes one front/rear attack contact and advances damage over time for a living chaser.
///
/// The caller enforces hit cooldown. Contact keys end at zero within each
/// bounded sphere array. Hit yaw uses 4096 units per turn; range uses root
/// coordinate units. Critical and vulnerable-state multipliers compose, while
/// accumulated damage narrows to 16 bits and is tested as signed. A fatal
/// direct hit broadcasts pack break-off; any fatal damage marks battle release
/// pending. The enemy, model and work block must remain live for the call.
static void _desertChaserDamage(Task* task)
{
    enum {
        ACTOR00100_REACTION_BODY_BURST          = 4,
        ACTOR00100_REACTION_BLEND_HIT           = 5,
        ACTOR00100_REACTION_FRONT_STAGGER       = 8,
        ACTOR00100_REACTION_FORCE_STAGGER       = 9,
        ACTOR00100_FRONT_HIT_LIMIT              = 0x501,
        ACTOR00100_RISE_HIT_WINDOW_FRAMES       = 10,
        ACTOR00100_KNOCK_DOWN_BONUS_FIRST_FRAME = 11,
        ACTOR00100_KNOCK_DOWN_DAMAGE            = 71,
        ACTOR00100_CRITICAL_EFFECT_NONE         = -1,
        ACTOR00100_CRITICAL_EFFECT_ROLL         = 0,
        ACTOR00100_CRITICAL_EFFECT_VULNERABLE   = 3,
        ACTOR00100_PACK_STAGE                   = 9,
        ACTOR00100_PACK_AREA                    = 1,
        ACTOR00100_PACK_COMMAND_BREAK_OFF       = 4,
    };
    PlayerStatus*              playerStatus = &gPlayerStatus;
    Enemy*                     enemy;
    GfxCoord*                  rootCoord;
    s16                        criticalStyle;
    s16                        relativeHitYaw;
    s16                        hitOffsetZ;
    s16                        poisonWakeState;
    s32                        staggerHitState;
    s16                        damageMultiplierState;
    s16                        fatalHitState;
    s16                        accumulatedHitState;
    s16                        fatalTickState;
    s16                        hitWakeState;
    s16                        lightHitState;
    s32                        hitYawMagnitude;
    s16                        reactionState;
    s16                        wrappedHitYaw;
    s32                        hitBearing;
    s32                        playerDistance;
    s32                        playerDeltaX;
    s32                        playerDeltaY;
    s32                        playerDeltaZ;
    u16                        accumulatedDamage;
    u32                        damageOverTime;
    u32                        doubledDamage;
    u32                        reaction;
    u8                         stage;
    SVECTOR*                   hitPosition;
    DesertChaserWork*          work;
    DesertChaserDamageScratch* scratch;
    DesertChaserDamageScratch* scratchHead;

    enemy = task->spawnArg2.pointer;
    work  = task->work;
    if (enemy->hp > 0) {
        scratchHead = SCRATCH_STACK_CURSOR(DesertChaserDamageScratch);
        scratch     = (SCRATCH_STACK_CURSOR(DesertChaserDamageScratch) = scratchHead - 1);
        // A frame takes at most one direct hit, preferring the front sphere.
        scratch->hitKey = _actor00100FindAttackContact(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts, &scratch->hitPos);
        if (scratch->hitKey == 0) {
            hitPosition     = &scratch->hitPos;
            scratch->hitKey = _actor00100FindAttackContact(work->spheres[DESERT_CHASER_SPHERE_REAR].contacts, hitPosition);
        }
        if (scratch->hitKey != 0) {
            work->hitFlag                         = 1;
            scratch->criticalEffect               = ACTOR00100_CRITICAL_EFFECT_NONE;
            work->hitCooldown                     = damageGetPlayerAttackHitCooldown(scratch->hitKey);
            task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(task->extra.tmd->coords);
            scratch->hitOffset.vx = scratch->hitPos.vx - task->extra.tmd->coords->workm.t[0];
            scratch->hitOffset.vy = scratch->hitPos.vy - task->extra.tmd->coords->workm.t[1];
            hitOffsetZ            = scratch->hitPos.vz - task->extra.tmd->coords->workm.t[2];
            scratch->hitOffset.vz = hitOffsetZ;
            hitBearing            = ratan2(scratch->hitOffset.vx, hitOffsetZ);
            rootCoord             = task->extra.tmd->coords;
            relativeHitYaw        = hitBearing - ratan2((s32)-rootCoord->workm.m[2][0], (s32)rootCoord->workm.m[2][2]);
            wrappedHitYaw         = relativeHitYaw;
            scratch->hitYaw       = relativeHitYaw;
            wrappedHitYaw         = _actorAngleNormalizeYaw(relativeHitYaw);
            scratch->hitYaw       = wrappedHitYaw;
            reaction              = damageGetPlayerAttackReaction(scratch->hitKey) & 0xFFFF;
            switch (reaction) {
                case DAMAGE_PLAYER_REACTION_NONE:
                case ACTOR00100_REACTION_BLEND_HIT:
                case DAMAGE_PLAYER_REACTION_EXPLOSION:
                case DAMAGE_PLAYER_REACTION_INCENDIARY:
                    hitWakeState = work->state;
                    if ((hitWakeState == ACTOR00100_STATE_APPROACH) || (hitWakeState == DESERT_CHASER_STATE_ROAM) || (hitWakeState == ACTOR00100_STATE_WATCH_PLAYER_FACING)) {
                        work->state = ACTOR00100_STATE_PURSUE;
                    }
                    if (work->state == ACTOR00100_STATE_STEER) {
                        work->state = ACTOR00100_STATE_RESUME_PURSUIT;
                    }
                    if (work->blendActive == 0) {
                        work->recentDamage = 0U;
                    }
                    lightHitState = work->state;
                    if (lightHitState == DESERT_CHASER_STATE_STUNNED || lightHitState == ACTOR00100_STATE_STAGGER || lightHitState == ACTOR00100_STATE_DOWNED_HIT || lightHitState == DESERT_CHASER_STATE_DOWNED) {
                        work->state     = ACTOR00100_STATE_DOWNED_HIT;
                        work->prevState = DESERT_CHASER_PREV_STATE_NONE;
                    } else if (lightHitState != ACTOR00100_STATE_RESUME_PURSUIT && lightHitState != DESERT_CHASER_STATE_DEATH && lightHitState != ACTOR00100_STATE_KNOCK_DOWN && lightHitState != ACTOR00100_STATE_STAGGER && lightHitState != DESERT_CHASER_STATE_RISE) {
                        _actor00100RequestHitBlend(work);
                    }
                    break;
                case ACTOR00100_REACTION_FRONT_STAGGER:
                    stage = gGameSession->location.loc.stage;
                    if ((stage != GAME_STAGE_DRYFIELD) && (stage != GAME_STAGE_SHELTER_NEO_ARK)) {
                        hitYawMagnitude = scratch->hitYaw;
                        if (hitYawMagnitude < 0) {
                            hitYawMagnitude = -hitYawMagnitude;
                        }
                        if (hitYawMagnitude < ACTOR00100_FRONT_HIT_LIMIT) {
                            _actor00100SelectHitReaction(work);
                        }
                    }
                    break;
                case ACTOR00100_REACTION_FORCE_STAGGER:
                    _actor00100SelectHitReaction(work);
                    break;
                case DAMAGE_PLAYER_REACTION_BUILDUP:
                    _actor00100SelectHitReaction(work);
                    damageStartEnemyBuildup(enemy, scratch->hitKey, 0);
                    break;
                case DAMAGE_PLAYER_REACTION_POISON:
                    poisonWakeState = work->state;
                    if ((poisonWakeState == ACTOR00100_STATE_APPROACH) || (poisonWakeState == DESERT_CHASER_STATE_ROAM) || (poisonWakeState == ACTOR00100_STATE_WATCH_PLAYER_FACING)) {
                        work->state = ACTOR00100_STATE_PURSUE;
                    }
                    if (work->state == ACTOR00100_STATE_STEER) {
                        work->state = ACTOR00100_STATE_RESUME_PURSUIT;
                    }
                    damageTryStartEnemyDamageOverTime(enemy, scratch->hitKey, 0);
                    break;
                case DAMAGE_PLAYER_REACTION_STAGGER:
                case ACTOR00100_REACTION_BODY_BURST:
                    staggerHitState = work->state;
                    if (staggerHitState == DESERT_CHASER_STATE_STUNNED || staggerHitState == ACTOR00100_STATE_STAGGER || staggerHitState == ACTOR00100_STATE_STEER || staggerHitState == ACTOR00100_STATE_KNOCK_DOWN || staggerHitState == ACTOR00100_STATE_DOWNED_HIT || staggerHitState == DESERT_CHASER_STATE_DOWNED || (staggerHitState == DESERT_CHASER_STATE_RISE && work->stateTimer < ACTOR00100_RISE_HIT_WINDOW_FRAMES)) {
                        reactionState = ACTOR00100_STATE_STAGGER;
                        work->state   = reactionState;
                    } else if (staggerHitState != DESERT_CHASER_STATE_DEATH && staggerHitState != DESERT_CHASER_STATE_HIDDEN) {
                        reactionState = ACTOR00100_STATE_KNOCK_DOWN;
                        work->state   = reactionState;
                    }
                    break;
            }
            // Distance damage, critical damage and vulnerable-state damage compose.
            playerDeltaX            = playerStatus->coordMtx->t[0] - task->extra.tmd->coords->coord.t[0];
            scratch->toPlayer.vx    = playerDeltaX;
            playerDeltaY            = playerStatus->coordMtx->t[1] - task->extra.tmd->coords->coord.t[1];
            scratch->toPlayer.vy    = playerDeltaY;
            playerDeltaZ            = playerStatus->coordMtx->t[2] - task->extra.tmd->coords->coord.t[2];
            scratch->toPlayer.vz    = playerDeltaZ;
            playerDistance          = SquareRoot0((playerDeltaX * playerDeltaX) + (playerDeltaY * playerDeltaY) + (playerDeltaZ * playerDeltaZ));
            scratch->playerDistance = playerDistance;
            scratch->damage         = damageComputePlayerAttack(scratch->hitKey, playerDistance, 0, 0);
            _desertChaserHitEffect(task, scratch->hitYaw, scratch->hitKey);
            work->lookYaw       = 0;
            work->lookYawTarget = 0;
            if (damageRollCriticalHit(enemy, scratch->hitKey, 0) != 0) {
                scratch->criticalEffect = ACTOR00100_CRITICAL_EFFECT_ROLL;
                scratch->damage         = scratch->damage * 4;
            }
            damageMultiplierState = work->state;
            if (damageMultiplierState == DESERT_CHASER_STATE_STUNNED || (damageMultiplierState == ACTOR00100_STATE_KNOCK_DOWN && work->stateTimer >= ACTOR00100_KNOCK_DOWN_BONUS_FIRST_FRAME) || damageMultiplierState == ACTOR00100_STATE_STAGGER || damageMultiplierState == DESERT_CHASER_STATE_DOWNED || damageMultiplierState == ACTOR00100_STATE_DOWNED_HIT || (damageMultiplierState == DESERT_CHASER_STATE_RISE && work->stateTimer < ACTOR00100_RISE_HIT_WINDOW_FRAMES)) {
                doubledDamage   = scratch->damage * 2;
                scratch->damage = doubledDamage;
                if (doubledDamage != 0) {
                    scratch->criticalEffect = ACTOR00100_CRITICAL_EFFECT_VULNERABLE;
                }
            }
            damageAccumulateLifeDrainHp(enemy, scratch->hitKey, scratch->damage, 0);
            criticalStyle = scratch->criticalEffect;
            if (criticalStyle != ACTOR00100_CRITICAL_EFFECT_NONE) {
                effectSpawn(EFFECT_CRITICAL_HIT, task->extra.tmd->coords + 2, (s32)(criticalStyle), NULL);
            }
            enemy->hp = enemy->hp - scratch->damage;
            worldTargetAddReadoutAmount(&enemy->node, scratch->damage, 0);
            accumulatedDamage  = work->recentDamage + (u16)scratch->damage;
            work->recentDamage = accumulatedDamage;
            if (enemy->hp <= 0) {
                work->broadcast.context.loc.stage = ACTOR00100_PACK_STAGE;
                work->broadcast.context.loc.area  = ACTOR00100_PACK_AREA;
                work->broadcast.command           = ACTOR00100_PACK_COMMAND_BREAK_OFF;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &work->broadcast, ACTOR_COMMAND_MESSAGE_APPLY);
                if ((damageGetPlayerAttackReaction(scratch->hitKey) & 0xFFFF) == ACTOR00100_REACTION_BODY_BURST) {
                    work->state = ACTOR00100_STATE_BURST_DEATH;
                } else {
                    fatalHitState = work->state;
                    if ((fatalHitState == ACTOR00100_STATE_STEER) || (fatalHitState == DESERT_CHASER_STATE_DOWNED) || (fatalHitState == ACTOR00100_STATE_DOWNED_HIT) || (fatalHitState == ACTOR00100_STATE_STAGGER) || (fatalHitState == DESERT_CHASER_STATE_STUNNED)) {
                        work->state     = ACTOR00100_STATE_STAGGER;
                        work->prevState = DESERT_CHASER_PREV_STATE_NONE;
                        _actor00100PlayPlacedSound(task, enemy, ACTOR00100_SOUND_HEAVY_HIT);
                    } else {
                        _actor00100PlayPlacedSound(task, enemy, ACTOR00100_SOUND_HEAVY_HIT);
                        work->state = ACTOR00100_STATE_KNOCK_DOWN;
                    }
                }
            } else if ((s16)accumulatedDamage >= ACTOR00100_KNOCK_DOWN_DAMAGE) {
                accumulatedHitState = work->state;
                if ((accumulatedHitState != DESERT_CHASER_STATE_STUNNED) && (accumulatedHitState != ACTOR00100_STATE_KNOCK_DOWN) && (accumulatedHitState != ACTOR00100_STATE_STAGGER) && (accumulatedHitState != ACTOR00100_STATE_DOWNED_HIT) && (accumulatedHitState != DESERT_CHASER_STATE_DOWNED)) {
                    _actor00100PlayPlacedSound(task, enemy, ACTOR00100_SOUND_HEAVY_HIT);
                    work->state = ACTOR00100_STATE_KNOCK_DOWN;
                } else {
                    _actor00100PlayPlacedSound(task, enemy, ACTOR00100_SOUND_LIGHT_HIT);
                }
            } else {
                _actor00100PlayPlacedSound(task, enemy, ACTOR00100_SOUND_LIGHT_HIT);
            }
        }
        // Damage over time still advances on frames with no direct hit.
        if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            scratch->damage = damageTickEnemyDamageOverTime(enemy);
            if (damageIsEnemyDamageOverTimeExpired(enemy) != 0) {
                enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
            }
            enemy->hp      = enemy->hp - scratch->damage;
            damageOverTime = scratch->damage;
            if (damageOverTime != 0) {
                worldTargetAddReadoutAmount(&enemy->node, (s32)damageOverTime, 0);
                if (enemy->hp <= 0) {
                    fatalTickState = work->state;
                    if ((fatalTickState != DESERT_CHASER_STATE_STUNNED) && (fatalTickState != ACTOR00100_STATE_STAGGER) && (fatalTickState != ACTOR00100_STATE_DOWNED_HIT) && (fatalTickState != DESERT_CHASER_STATE_DOWNED)) {
                        work->state = ACTOR00100_STATE_COLLAPSE;
                    } else {
                        work->state = DESERT_CHASER_STATE_DEATH;
                    }
                } else {
                    if (work->state == ACTOR00100_STATE_PURSUE) {
                        work->state = DESERT_CHASER_STATE_ROAM;
                    }
                    if (work->state != DESERT_CHASER_STATE_STUNNED && work->state != ACTOR00100_STATE_STAGGER && work->state != ACTOR00100_STATE_DOWNED_HIT && work->state != DESERT_CHASER_STATE_DOWNED) {
                        _actor00100RequestHitBlend(work);
                    } else if (work->state != DESERT_CHASER_STATE_STUNNED) {
                        work->state = ACTOR00100_STATE_DOWNED_HIT;
                    } else {
                        work->prevState = DESERT_CHASER_PREV_STATE_NONE;
                    }
                }
            }
        }
        if (enemy->hp <= 0) {
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
            worldCoordSetActorColorMode(ctx, ENEMY_COLOR_DEFAULT);
            worldCoordSetActorColorMode(ctx, ENEMY_COLOR_WEIGHTED);
            /* fallthrough */
        case 0xA:
            arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            worldCoordSetActorColorMode(ctx, ENEMY_COLOR_BLACK);
            break;
        case 0xF:
            work->burnPosFront.vx   = 0;
            work->burnPosFront.vy   = 0;
            work->burnPosFront.vz   = 0;
            work->burnPosForeleg.vx = 0;
            work->burnPosForeleg.vy = 0;
            work->burnPosForeleg.vz = 0;
            _actorRenderTransformLocalPointToWorld(&arg0->extra.tmd->coords[2], &work->burnPosFront);
            effectSpawn(EFFECT_CORPSE_BURN, &gGfxViewCoord, 2, &work->burnPosFront);
            work->burnPosFront.vy = arg0->extra.tmd->coords[0].coord.t[1];
            _actorRenderTransformLocalPointToWorld(&arg0->extra.tmd->coords[9], &work->burnPosForeleg);
            work->burnPosForeleg.vy = arg0->extra.tmd->coords[0].coord.t[1];
            effectSpawn(EFFECT_CORPSE_BURN, &gGfxViewCoord, 2, &work->burnPosForeleg);
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
        _desertChaserAnimTick(arg0);
        return;
    }
    radius = 1000;
    _desertChaserAnimTick(arg0);
    _actorPositionDeltaToPlayer(&gPlayerStatus, arg0->extra.tmd->coords, &delta);
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        outside     = actorOutsideRadius(&delta, radius);
        work->state = outside == 0 ? 0x1F : 0x26;
    }
}

#include "../../shared/desert_chaser_spawn_aim.inc.c"

#include "../../shared/desert_chaser_strike.inc.c"

/// Plays the backward leap after a strike or throw, then returns to roam.
///
/// The rearward wall probe shortens steps. Movement is limited to the travel
/// cue window and stops after five root-grid pushes; step lengths use root
/// parent-coordinate units. Requires the regular build's live enemy and model.
static void _actor00100LeapBack(Task* task)
{
    enum { ACTOR00100_CLIP_LEAP_BACK  = 6,
           ACTOR00100_LEAP_FIRST_CUE  = 6,
           ACTOR00100_LEAP_CUE_COUNT  = 8,
           ACTOR00100_LEAP_MAX_PUSHES = 5 };
    DesertChaserWork* work;
    TmdObject*        model;
    Enemy*            enemy;
    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy                         = task->spawnArg2.pointer;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = DESERT_CHASER_FRONT_RADIUS;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animId                                          = ACTOR00100_CLIP_LEAP_BACK;
        work->blendActive                                     = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->animRate                                        = work->baseRate;
        _desertChaserAnimTick(task);
        work->stateTimer                 = 0;
        work->stateCounter               = 0;
        work->wallProbe.shape.ends[1].vz = -0x2D0;
    }
    work->stateTimer += 1;
    _desertChaserAnimTick(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        work->state = DESERT_CHASER_STATE_ROAM;
    }
    // Only the leap's travel cues move backward; grid pushes limit further travel.
    if (((u32)((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) - ACTOR00100_LEAP_FIRST_CUE) < (u32)ACTOR00100_LEAP_CUE_COUNT) && (work->stateCounter < ACTOR00100_LEAP_MAX_PUSHES)) {
        if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts)) != 0) {
            work->stateCounter = (s16)((u16)work->stateCounter + 1);
        }
        switch (work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) {
            case 12:
                _actorMovementStepForward(task->extra.tmd->coords, -60);
                break;
            case 13:
                _actorMovementStepForward(task->extra.tmd->coords, -30);
                break;
            case 14:
                _actorMovementStepForward(task->extra.tmd->coords, -15);
                break;
            default:
                if (_desertChaserCapsuleTouchesGrid(task)) {
                    _actorMovementStepForward(task->extra.tmd->coords, -85);
                } else {
                    _actorMovementStepForward(task->extra.tmd->coords, -120);
                }
                break;
        }
    } else {
        _actorContactApplyGridPushback(task->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts));
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

static void Actor00100_Fn070DC(Task* arg0)
{
    DesertChaserWork*  work;
    GfxCoord*          coord;
    GfxCoord*          coord2;
    TmdObject*         obj;
    s32                playerX;
    s16                z;
    s16                targetYaw;
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
        _desertChaserAnimTick(arg0);
        work->stateTimer = 0;
    }
    _actorContactApplyGridPushback(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts));
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    coord                                 = arg0->extra.tmd->coords;
    head[-1].delta.vx                     = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    scratch->delta.vy                     = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    scratch->delta.vz                     = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _desertChaserAnimTick(arg0);
    firstDelta             = _actorAngleTurnToOffset(arg0->extra.tmd->coords, head[-1].delta.vx, scratch->delta.vz);
    scratch->turn          = firstDelta;
    work->lookYawTarget    = firstDelta;
    playerX                = -(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0];
    scratch->playerYaw     = ratan2((s32)playerX, (s32)(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
    coord2                 = arg0->extra.tmd->coords;
    scratch->delta.vx      = gPlayerStatus.coordMtx->t[0] - coord2->coord.t[0];
    scratch->delta.vy      = gPlayerStatus.coordMtx->t[1] - coord2->coord.t[1];
    z                      = gPlayerStatus.coordMtx->t[2] - coord2->coord.t[2];
    scratch->delta.vz      = z;
    targetYaw              = ratan2(scratch->delta.vx, (s32)z) + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
    scratch->yawFromPlayer = targetYaw;
    scratch->yawFromPlayer = _actorAngleNormalizeYaw(targetYaw);
    finalDelta             = _actorAngleTurnToOffset(arg0->extra.tmd->coords, scratch->delta.vx, scratch->delta.vz);
    scratch->turn          = (s16)finalDelta;
    work->lookYawTarget    = (s16)finalDelta;
    magnitude              = abs(scratch->yawFromPlayer - scratch->playerYaw);
    if (magnitude >= 0x601) {
        sceneEngageBattle(1);
        work->state = 0x1C;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorChaseScratch);
}

#include "../../shared/desert_chaser_steer.inc.c"

/// Knocks the chaser down after a blocked lunge and charges 15 HP on entry.
///
/// The entry charge is clamped to leave at least one HP, including its readout
/// of all 15 points. Once the clip settles, later damage may select death;
/// otherwise buildup selects stun and the remaining path waits downed.
static void _actor00100KnockDownWithDamage(Task* task)
{
    enum { ACTOR00100_CLIP_KNOCK_DOWN  = 10,
           ACTOR00100_COLLISION_DAMAGE = 15,
           ACTOR00100_SOUND_KNOCK_DOWN = 0x40010009 };
    Enemy*            enemy;
    DesertChaserWork* work;
    TmdObject*        model;
    s32               knockDownSound;
    s32               hitSound;
    s32               knockDownPan;
    s32               hitPan;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = DESERT_CHASER_FRONT_RADIUS;
        work->animId                                          = ACTOR00100_CLIP_KNOCK_DOWN;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->blendActive                                     = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->animRate                                        = work->baseRate;
        _desertChaserAnimTick(task);
        knockDownSound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR00100_SOUND_KNOCK_DOWN;
        knockDownPan   = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(knockDownSound, knockDownPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        // A blocked lunge charges damage, but this entry charge cannot kill.
        enemy->hp = enemy->hp - ACTOR00100_COLLISION_DAMAGE;
        worldTargetAddReadoutAmount(&enemy->node, ACTOR00100_COLLISION_DAMAGE, 0);
        if (enemy->hp <= 0) {
            enemy->hp = 1;
        } else {
            hitSound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR00100_SOUND_LIGHT_HIT;
            hitPan   = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(hitSound, hitPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        }
    }
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts));
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _desertChaserAnimTick(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        if (enemy->hp <= 0) {
            work->state = DESERT_CHASER_STATE_DEATH;
        } else if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            work->state = DESERT_CHASER_STATE_STUNNED;
        } else {
            work->state = DESERT_CHASER_STATE_DOWNED;
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
    TmdObject*                     obj;
    s32                            playerX;
    s16                            z;
    s16                            targetYaw;
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
        _desertChaserAnimTick(arg0);
        work->stateTimer = 0;
    }
    actorRenderComposeCoord(arg0->extra.tmd->coords);
    gte_SetTransMatrix(&arg0->extra.tmd->coords->workm);
    gte_SetRotMatrix(&arg0->extra.tmd->coords->workm);
    head[-1].delta.vx = 0;
    scratch->delta.vy = 0;
    scratch->delta.vz = 0;
    gte_RotTransPers(&scratch->delta, &head[-1].screen, &head[-1].depthCue, &head[-1].projectionFlags, &head[-1].orderingDepth);
    _actorContactApplyGridPushback(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts));
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    coord                                 = arg0->extra.tmd->coords;
    head[-1].delta.vx                     = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    scratch->delta.vy                     = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    scratch->delta.vz                     = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _desertChaserAnimTick(arg0);
    firstDelta             = _actorAngleTurnToOffset(arg0->extra.tmd->coords, head[-1].delta.vx, scratch->delta.vz);
    scratch->turn          = firstDelta;
    work->lookYawTarget    = firstDelta;
    playerX                = -(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0];
    scratch->playerYaw     = ratan2((s32)playerX, (s32)(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
    coord2                 = arg0->extra.tmd->coords;
    scratch->delta.vx      = gPlayerStatus.coordMtx->t[0] - coord2->coord.t[0];
    scratch->delta.vy      = gPlayerStatus.coordMtx->t[1] - coord2->coord.t[1];
    z                      = gPlayerStatus.coordMtx->t[2] - coord2->coord.t[2];
    scratch->delta.vz      = z;
    targetYaw              = ratan2(scratch->delta.vx, (s32)z) + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
    scratch->yawFromPlayer = targetYaw;
    scratch->yawFromPlayer = _actorAngleNormalizeYaw(targetYaw);
    finalDelta             = _actorAngleTurnToOffset(arg0->extra.tmd->coords, scratch->delta.vx, scratch->delta.vz);
    scratch->turn          = (s16)finalDelta;
    work->lookYawTarget    = (s16)finalDelta;
    if (abs(scratch->screen.vx) < 0x78 && abs(scratch->screen.vy) < 0x64 && abs(scratch->turn) < 0x200) {
        sceneEngageBattle(1);
        work->state = 0x1C;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor00100ScreenWatchScratch);
}

/// Applies the actor's texture placement to a spawned body-part model.
///
/// A null `burstEffect` leaves `sourceActor` unread. Otherwise both tasks must
/// have live TMD models. Encoded texture-page and CLUT-row displacements are
/// signed bytes. An existing primitive buffer must meet `tmdBuildBufferHalf`'s
/// contract and has both halves rebuilt; a bufferless model retains the offsets
/// for its later build. The task and effect records are borrowed.
static inline void _actor00100ApplyBurstTexturePlacement(const Task* sourceActor, const EffectWork* burstEffect)
{
    Task*      burstTask;
    TmdObject* burstModel;
    if (burstEffect != NULL) {
        burstTask                               = burstEffect->task;
        burstTask->extra.tmd->texturePageOffset = sourceActor->extra.tmd->texturePageOffset;
        burstTask->extra.tmd->clutRowOffset     = sourceActor->extra.tmd->clutRowOffset;
        burstModel                              = burstTask->extra.tmd;
        if (burstModel->buffer != NULL) {
            // Each build toggles the selected half; reload the model for the second.
            tmdBuildBufferHalf(burstModel);
            tmdBuildBufferHalf(burstTask->extra.tmd);
        }
    }
}

/// Breaks the body into model parts, unlinks collision, then advances toward teardown.
///
/// Legs spawn on tick 2, torso on 4 and head on 5, with texture offsets copied
/// from the live actor. Tick 30 unlinks all four bodies. From tick 31 teardown
/// waits for player release, the wheel overlay and pending display changes;
/// Mine Mesa receives the placement index before the task state advances.
static void _actor00100BurstDeath(Task* actor)
{
    enum {
        ACTOR00100_BURST_LEGS_TICK          = 2,
        ACTOR00100_BURST_TORSO_TICK         = 4,
        ACTOR00100_BURST_HEAD_TICK          = 5,
        ACTOR00100_BURST_UNLINK_TICK        = 30,
        ACTOR00100_BURST_RELEASE_FIRST_TICK = 31,
        ACTOR00100_BURST_PUFF_SIZE          = 512,
        ACTOR00100_BURST_TASK_TYPE          = (u16)EFFECT_BURST_BODY_PART_BANK10,
        ACTOR00100_BURST_PART_RIGHT_LEG     = 9,
        ACTOR00100_BURST_PART_LEFT_LEG      = 12,
        ACTOR00100_BURST_PART_TORSO         = 1,
        ACTOR00100_BURST_PART_HEAD          = 3,
    };
    SVECTOR           burstOffset;
    EffectWork*       rightLegEffect;
    EffectWork*       leftLegEffect;
    EffectWork*       torsoEffect;
    EffectWork*       headEffect;
    TmdObject*        model;
    u16               elapsedFrames;
    DesertChaserWork* work;
    Enemy*            enemy;

    work  = actor->work;
    enemy = actor->spawnArg2.pointer;
    model = actor->extra.tmd;
    if (work->stateEntered != 0) {
        work->hitFlag                                         = 0;
        model->flags                                          = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = DESERT_CHASER_FRONT_RADIUS;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags   = (u16)(work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        enemy->node.state.parts.flags                         = WORLD_TARGET_NOT_LOCKABLE;
        work->lookYaw                                         = 0;
        work->lookYawTarget                                   = 0;
        work->waistYawTarget                                  = 0;
        work->stateTimer                                      = 0U;
        // Entry stages this offset even though later part-spawn cues overwrite it.
        burstOffset.vx = 100;
        burstOffset.vz = 0;
        burstOffset.vy = 0;
    }
    elapsedFrames    = work->stateTimer + 1;
    work->stateTimer = elapsedFrames;
    // Select the descriptor before each spawn; the new effect snapshots its model.
    if ((s16)elapsedFrames == ACTOR00100_BURST_LEGS_TICK) {
        model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        tmdFreePrimitiveBuffer(model);
        D_80114B34[ACTOR00100_BURST_TASK_TYPE].data.model = &_gActor00100DesertChaserBurstLegRight;
        burstOffset.vz                                    = 0x64;
        burstOffset.vy                                    = 0;
        burstOffset.vx                                    = 0;
        rightLegEffect                                    = effectSpawn(EFFECT_BURST_BODY_PART_BANK10, &actor->extra.tmd->coords[ACTOR00100_BURST_PART_RIGHT_LEG], ACTOR00100_BURST_PUFF_SIZE, &burstOffset);
        _actor00100ApplyBurstTexturePlacement(actor, rightLegEffect);
        if (work->stateTimer == ACTOR00100_BURST_LEGS_TICK) {
            D_80114B34[ACTOR00100_BURST_TASK_TYPE].data.model = &_gActor00100DesertChaserBurstLegLeft;
            burstOffset.vy                                    = 0;
            burstOffset.vx                                    = 0;
            leftLegEffect                                     = effectSpawn(EFFECT_BURST_BODY_PART_BANK10, &actor->extra.tmd->coords[ACTOR00100_BURST_PART_LEFT_LEG], ACTOR00100_BURST_PUFF_SIZE, &burstOffset);
            _actor00100ApplyBurstTexturePlacement(actor, leftLegEffect);
        }
    }
    if (work->stateTimer == ACTOR00100_BURST_TORSO_TICK) {
        D_80114B34[ACTOR00100_BURST_TASK_TYPE].data.model = &_gActor00100DesertChaserBurstTorso;
        torsoEffect                                       = effectSpawn(EFFECT_BURST_BODY_PART_BANK10, &actor->extra.tmd->coords[ACTOR00100_BURST_PART_TORSO], ACTOR00100_BURST_PUFF_SIZE, NULL);
        _actor00100ApplyBurstTexturePlacement(actor, torsoEffect);
    }
    if (work->stateTimer == ACTOR00100_BURST_HEAD_TICK) {
        D_80114B34[ACTOR00100_BURST_TASK_TYPE].data.model = &_gActor00100DesertChaserBurstHead;
        headEffect                                        = effectSpawn(EFFECT_BURST_BODY_PART_BANK10, &actor->extra.tmd->coords[ACTOR00100_BURST_PART_HEAD], ACTOR00100_BURST_PUFF_SIZE, NULL);
        _actor00100ApplyBurstTexturePlacement(actor, headEffect);
    }
    // Collision ends before release is allowed to advance the task.
    if (work->stateTimer == ACTOR00100_BURST_UNLINK_TICK) {
        enemy->hp = 0;
        worldCollisionUnlinkBody(&work->spheres[DESERT_CHASER_SPHERE_FRONT].body);
        worldCollisionUnlinkBody(&work->spheres[DESERT_CHASER_SPHERE_REAR].body);
        worldCollisionUnlinkBody(&work->wallProbe.body);
        worldCollisionUnlinkBody(&work->spheres[DESERT_CHASER_SPHERE_ROOT].body);
        enemy->recs = 0;
    }
    if ((work->stateTimer >= ACTOR00100_BURST_RELEASE_FIRST_TICK) && (work->playerHeld == 0) && (Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_MESA, 0, 0)) {
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, (s32)(enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT), 0);
        }
        actor->state++;
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
    _desertChaserAnimTick(arg0);
    state = work->animId;
    switch (state) {
        case 3:
            timer = work->stateTimer;
            if (timer < 0x1E) {
                _actorScaleMatrix(&work->colorMtx, (timer << 12) / 30);
            } else {
                work->animId      = 0xD;
                work->animRequest = DESERT_CHASER_ANIM_REQUEST_BLEND;
                work->stateTimer  = 0;
                sound             = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40010010;
                pan               = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                sndEvtRequestScriptStart(sound, (s32)pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            }
            _actorMovementStepForward(arg0->extra.tmd->coords, 200);
            break;
        case 13:
            if (work->stateTimer <= ((s16)work->baseRate * 17) / 16) {
                _actorMovementTranslateForwardNonzero(arg0->extra.tmd->coords, ((s16)work->baseRate * 2000) / 272);
            } else if (work->stateTimer <= ((s16)work->baseRate * 25) / 16) {
                _actorMovementTranslateForwardNonzero(arg0->extra.tmd->coords, ((s16)work->baseRate * 1000) / 192);
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
    _desertChaserAnimTick(arg0);
    switch (work->animId) {
        case 3:
            _actorMovementTranslateForwardNonzero(arg0->extra.tmd->coords, ((s16)work->baseRate * 1000) / 192);
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            if (work->stateTimer >= 0xD) {
                work->animId      = 0xE;
                work->animRequest = DESERT_CHASER_ANIM_REQUEST_BLEND;
                work->stateTimer  = 0;
            }
            break;
        case 14:
            _actorMovementTranslateForwardNonzero(arg0->extra.tmd->coords, ((s16)work->baseRate * 1300) / 192);
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
    _actor00100HideState,
    Actor00100_Fn08E7C,
    _actor00100IdleState2,
    _actor00100BurstDeath,
    _desertChaserStunned,
    Actor00100_Fn09724,
    Actor00100_Fn09CCC,
    _desertChaserStagger,
    _desertChaserTurnRightState,
    _desertChaserTurnLeftState,
    _desertChaserCollapse,
    _actor00100DownedHitReaction,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    _actor00100WaitDowned,
    NULL,
    NULL,
    _desertChaserKnockDown,
    Actor00100_Fn04270,
    NULL,
    NULL,
    desertChaserApproach,
    NULL,
    NULL,
    NULL,
    desertChaserPursue,
    Actor00100_Fn061FC,
    _desertChaserThrowPlayer,
    _actor00100LeapBack,
    Actor00100_Fn070DC,
    desertChaserSteer,
    _actor00100ResumePursuit,
    _actor00100KnockDownWithDamage,
    _actor00100Rise,
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
    worldCoordUpdateActorColor(enemy, &pos, 0, 0);
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            initialState = work->state;
            if (initialState != 21 && initialState != 0 && initialState != 6 && initialState != 3) {
                actor->extra.tmd->flags = 0;
                height                  = actor->extra.tmd->coords->coord.t[1];
                _limbShadowDrawSegment(actor, 1, 3, 0x12C, height, 0xFF);
                _limbShadowDrawSegment(actor, 3, 4, 0xC8, height, 0xFF);
                _limbShadowDrawSegment(actor, 1, 0xB, 0xFA, height, 0xFF);
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            modeState = work->state;
            if ((modeState != 0x15) && (modeState != 0) && (modeState != 6) && (modeState != 3)) {
                actor->extra.tmd->flags = 0;
                modeHeight              = actor->extra.tmd->coords->coord.t[1];
                _limbShadowDrawSegment(actor, 1, 3, 0x12C, modeHeight, 0xFF);
                _limbShadowDrawSegment(actor, 3, 4, 0xC8, modeHeight, 0xFF);
                _limbShadowDrawSegment(actor, 1, 0xB, 0xFA, modeHeight, 0xFF);
            }
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            actor->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }
    scratch = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    if (work->hitCooldown > 0) {
        work->hitCooldown = (s16)((u16)work->hitCooldown - 1);
    } else if (work->state != 5) {
        _desertChaserDamage(actor);
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
                                if (taskMessageDispatch(playerSlot, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, 4), 0) == 1)
                                    break;
                            }
                        }
                    }
                } else if ((work->playerMove.displacement.vx != 0) || (work->playerMove.displacement.vz != 0)) {
                    if ((s16)TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_MOVE_BY, &work->playerMove, 0) != 1) {
                        if (work->playerAnimFrames >= 0xF) {
                            work->playerMove.displacement.vx >>= 1;
                            work->playerMove.displacement.vy >>= 1;
                            work->playerMove.displacement.vz >>= 1;
                        }
                    } else if (((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 1, 0, 0)) && (work->playerMove.displacement.vx != 0) && (work->playerMove.displacement.vz != 0) && (work->playerAnimFrames < 6) && _actor00100IsInMesaDropRegion(player) && _actor00100MovesTowardMesaDrop(player, &work->playerMove.displacement)) {
                        work->playerMove.collisionRequests = (GAME_ACTOR_COLLISION_REQUEST_MASK << GAME_ACTOR_COLLISION_DISABLE_REQUEST_SHIFT);
                        Gp_StateC08.flags                  = (u8)(Gp_StateC08.flags | ATTACHMENT_FLAG_EVENT_LOCK);
                        roomEffectRequestCancelAll();
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
        sceneReleaseBattleRefWithRewards(actor, 1);
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
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

/// The task's handlers, indexed by `Task::state`: set-up, the per-frame state
/// dispatch, a wait for the pending release before advancing, and teardown.
static const DesertChaserTaskStates gDesertChaserTaskStates = { {
    Actor00100_Fn02C54,
    Actor00100_Fn0A288,
    _actor00100ReleaseBattleState,
    enemyDestroy,
} };

/// Empty handler for message 2015; ignores the task and both payload words.
///
/// The binary leaves the reply register unspecified. The message callback
/// signature is retained without manufacturing a return value.
static s32 _actor00100IgnoreMessage2015(Task* task, s32 messageId, s32 unusedPayload, s32 unusedExtra)
{
}

/// Returns whether the regular chaser's wall probe has a room-grid contact.
///
/// Scans its five-entry contact array up to the first zero key; the task must
/// own a live regular-build work block.
static s16 _actor00100WallProbeTouchesGrid(Task* task)
{
    return _desertChaserCapsuleTouchesGrid(task);
}

#include "../../shared/actor_messages_visibility.inc.c"

#include "../../shared/actor_messages_is_present.inc.c"

#include "../../shared/actor_messages_place_yaw.inc.c"

#include "../../shared/desert_chaser_exit.inc.c"

/// Emits recursive dust at regular-chaser parts 0, 1, 7, 9, 14 and 17.
///
/// Other part indices do nothing. Supported parts stage a fixed local vertical
/// offset even while room effects are disabled. `dustArgs` is sign-extended
/// from a halfword before setting recursion: bits 0..11 are size and 12..15
/// are frame period (zero selects one). The effect copies the offset during
/// dispatch; the live model must provide all 18 coordinates.
static void _actor00100SpawnPartDust(Task* task, s16 jointIndex, s16 dustArgs)
{
    DesertChaserWork* work = task->work;
    s32               supportedJoint;

    switch (jointIndex) {
        case 0:
        case 1:
            supportedJoint = 1;
            _actor00100StageDustOffset(work, 0);
            break;
        case 9:
            supportedJoint = 1;
            _actor00100StageDustOffset(work, 700);
            break;
        case 7:
            supportedJoint = 1;
            _actor00100StageDustOffset(work, 700);
            break;
        case 14:
        case 17:
            supportedJoint = 1;
            _actor00100StageDustOffset(work, 600);
            break;
        default:
            supportedJoint = 0;
            break;
    }

    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && supportedJoint == 1) {
        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[jointIndex], dustArgs | DESERT_CHASER_CUE_DUST_RECURSIVE, &work->effectOffset);
    }
}

/// Hides the chaser and disables root grid collision on entry to work state 0.
///
/// The enemy cannot be locked onto while hidden. Pair collision participation
/// is managed by the per-frame driver.
static void _actor00100HideState(Task* task)
{
    TmdObject*        obj;
    Enemy*            enemy;
    DesertChaserWork* work;

    work = task->work;
    if (work->stateEntered != 0) {
        obj                                                  = task->extra.tmd;
        enemy                                                = task->spawnArg2.pointer;
        enemy->node.state.parts.flags                        = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                                          |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
    }
}

#include "../../shared/desert_chaser_stunned.inc.c"

/// Leaves steering with clip 8 and resumes pursuit once the clip settles.
///
/// Entry restores targeting, drawing and root-grid collision and engages
/// battle. Root contacts push the chaser out of the grid every tick.
static void _actor00100ResumePursuit(Task* task)
{
    enum { ACTOR00100_CLIP_RESUME_PURSUIT = 8 };
    TmdObject*        model;
    Enemy*            enemy;
    DesertChaserWork* work;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy                         = task->spawnArg2.pointer;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = DESERT_CHASER_FRONT_RADIUS;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animId                                          = ACTOR00100_CLIP_RESUME_PURSUIT;
        work->blendActive                                     = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->animRate                                        = work->baseRate;
        _desertChaserAnimTick(task);
        sceneEngageBattle(1);
    }
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts));
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _desertChaserAnimTick(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        work->state = ACTOR00100_STATE_PURSUE;
    }
}

/// Rises with clip 12 at the chaser's base rate, then returns to roam.
///
/// Entry restores drawing, targeting and root-grid collision. Requires the
/// regular build's live work block, enemy and model.
static void _actor00100Rise(Task* task)
{
    enum { ACTOR00100_CLIP_RISE = 12 };
    DesertChaserWork* work;
    TmdObject*        model;
    Enemy*            enemy;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy                         = task->spawnArg2.pointer;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = DESERT_CHASER_FRONT_RADIUS;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animId                                          = ACTOR00100_CLIP_RISE;
        work->blendActive                                     = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->animRate                                        = work->baseRate;
        _desertChaserAnimTick(task);
    }
    _desertChaserAnimTick(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        work->state = DESERT_CHASER_STATE_ROAM;
    }
}

#include "../../shared/desert_chaser_flinch.inc.c"

/// Waits downed for the variant's base frames plus a random 0..15 ticks.
///
/// The halfword countdown is tested as signed after decrement, including on
/// entry. Expiry selects rise while HP is positive and death otherwise.
static void _actor00100WaitDowned(Task* task)
{
    enum { ACTOR00100_DOWN_DELAY_MASK = 15 };
    DesertChaserWork* work;
    u16               framesLeft;
    u32               randomDelay;
    Enemy*            enemy;

    work = task->work;
    if (work->stateEntered != 0) {
        randomDelay      = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState  = randomDelay;
        work->stateTimer = work->downFramesBase + ((randomDelay >> 0x10) & ACTOR00100_DOWN_DELAY_MASK);
    }
    _desertChaserAnimTick(task);
    framesLeft       = work->stateTimer - 1;
    work->stateTimer = framesLeft;
    if ((s16)framesLeft < 0) {
        enemy = task->spawnArg2.pointer;
        if (enemy->hp > 0) {
            work->state = DESERT_CHASER_STATE_RISE;
        } else {
            work->state = DESERT_CHASER_STATE_DEATH;
        }
    }
}

#include "../../shared/desert_chaser_stagger.inc.c"

#include "../../shared/desert_chaser_collapse.inc.c"

/// Plays the hit reaction selected while the chaser is stunned, staggered or downed.
///
/// Restarts clip 20 at one frame per tick. On settling, a living enemy rises
/// or returns to buildup stun; an empty HP pool starts the death sequence.
static void _actor00100DownedHitReaction(Task* task)
{
    enum { ACTOR00100_CLIP_DOWNED_HIT = 20 };
    Enemy*            enemy;
    DesertChaserWork* work;
    TmdObject*        model;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model                                                 = task->extra.tmd;
        work->hitFlag                                         = 0;
        model->flags                                          = 0;
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = DESERT_CHASER_FRONT_RADIUS;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags                         = 0;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_RESET;
        work->animId                                          = ACTOR00100_CLIP_DOWNED_HIT;
        work->animRate                                        = ANIMATION_RATE_ONE;
        work->lookYawTarget                                   = 0;
        work->waistYawTarget                                  = 0;
        if (enemy->hp <= 0) {
            sceneSetEnemyAlert(1);
        }
    }
    _desertChaserAnimTick(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        if (enemy->hp > 0) {
            if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
                work->state = DESERT_CHASER_STATE_STUNNED;
            } else {
                work->state = DESERT_CHASER_STATE_RISE;
            }
        } else {
            work->state = DESERT_CHASER_STATE_DEATH;
        }
    }
}

/// Idle handler for work state 2; leaves the actor and state unchanged.
static void _actor00100IdleState2(Task* task)
{
}

/// Snaps the caught player's root height to the chaser when separation exceeds 800 units.
///
/// Acts only with a live player slot and all player collision requests enabled.
/// Coordinates share root-parent units. It does not test the hold flag itself;
/// the frame driver performs the equivalent operation inside its hold phase.
static void _actor00100AlignPlayerHeight(Task* task)
{
    enum { ACTOR00100_PLAYER_HEIGHT_LIMIT = 800 };
    DesertChaserWork* work;
    Task*             player;

    work   = task->work;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if ((player != NULL) && (work->playerMove.collisionRequests == GAME_ACTOR_COLLISION_REQUEST_MASK)) {
        if (abs(player->extra.tmd->coords->coord.t[1] - task->extra.tmd->coords->coord.t[1]) > ACTOR00100_PLAYER_HEIGHT_LIMIT) {
            player->extra.tmd->coords->coord.t[1]   = task->extra.tmd->coords->coord.t[1];
            player->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        }
    }
}

/// Settles a pending battle reference release, then advances toward teardown.
///
/// The enemy argument is unused. Clears the pending marker before releasing
/// with rewards; task state advances once the marker is zero.
static void _actor00100ReleaseBattleState(Enemy* enemy, Task* task)
{
    DesertChaserWork* work;

    work = task->work;
    if (work->deathPending == 1) {
        work->deathPending = 0;
        sceneReleaseBattleRefWithRewards(task, 1);
    }
    if (work->deathPending == 0) {
        task->state++;
    }
}

#include "../../shared/desert_chaser_task.inc.c"
