#include "actors/actor_521100.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actor_521100_private.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/fs.h"
#include "main/random.h"
#include "main/gfx.h"
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
#include "../../shared/actor_messages.h"
#include "../../shared/no9_golem.h"

/// Native clips used by the fight's movement and hit reactions.
enum {
    ACTOR_521100_ANIM_IDLE        = 1,
    ACTOR_521100_ANIM_GUARD_START = 14,
    ACTOR_521100_ANIM_GUARD_END   = 15,
    ACTOR_521100_ANIM_STAGGER     = 16,
    ACTOR_521100_ANIM_FLINCH      = 17,
    ACTOR_521100_ANIM_WALK        = 18,
};

/// Initial and resolved responses used by the body's hit handler.
enum {
    ACTOR_521100_HIT_RESPONSE_NORMAL  = 0,
    ACTOR_521100_HIT_RESPONSE_GUARD   = 1,
    ACTOR_521100_HIT_RESPONSE_STAGGER = 2,
};

/// Phases and random-table index shared by the hit reactions.
enum {
    ACTOR_521100_REACTION_BEGIN           = 0,
    ACTOR_521100_REACTION_WAIT            = 1,
    ACTOR_521100_RECOVERY_WAIT_INDEX_MASK = 0xF,
};

/// Resumes approach or the saved route after a hit reaction, then draws its wait.
///
/// `bodyWork` must be a side-effect-free pointer expression to the live work
/// block; it is evaluated repeatedly. `waitFrameTable` is evaluated once and
/// supplies sixteen read-only frame counts. Consumes one LCG draw. Expands
/// to a braced block; use only as a statement inside an existing block.
#define ACTOR_521100_RESUME_AFTER_HIT(bodyWork, waitFrameTable)                                             \
    {                                                                                                       \
        const u16* waitFrames;                                                                              \
        u32        randomState;                                                                             \
        if ((bodyWork)->resumeRoute == 0) {                                                                 \
            (bodyWork)->state    = ACTOR_521100_STATE_APPROACH;                                             \
            (bodyWork)->subState = 0;                                                                       \
        } else {                                                                                            \
            (bodyWork)->state    = ACTOR_521100_STATE_WALK_ROUTE;                                           \
            (bodyWork)->subState = (bodyWork)->resumeRouteLeg;                                              \
        }                                                                                                   \
        (bodyWork)->animationId  = ACTOR_521100_ANIM_IDLE;                                                  \
        waitFrames               = (waitFrameTable);                                                        \
        randomState              = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;          \
        gRandomLcgState          = randomState;                                                             \
        (bodyWork)->stateCounter = waitFrames[(randomState >> 16) & ACTOR_521100_RECOVERY_WAIT_INDEX_MASK]; \
    }

extern TaskMessageEntry D_actor_521100_8015F6FC[8];

s32 func_actor_521100_80135BEC(Task*, s32, s32, s32);

s32 func_actor_521100_80135C14(Task*, s32, AnimationPlayRequest*, s32);

/// Scratch-stack block of the grab, reserved for one tick of that state.
///
/// It holds the payloads of the two messages the grab sends the player - the
/// animation of the package's own player set, and the placement that walks the
/// player to the hold and carries them through the throw - and the vectors the
/// placement is worked out in. The player's task reads each payload while its
/// message is dispatched and keeps neither address. No vector's fourth
/// component is written.
typedef struct {
    VECTOR               worldVector;     // Long vector along the world axes: `localOffset` rotated through the golem's root. The seizure adds the root's position to make it the spot the player is held at, then overwrites it with the unit direction of `toHoldSpot` (4096 = 1.0) while the player is still short of the spot
    VECTOR               toHoldSpot;      // Hold spot minus the player's position on X and Z, `vy` zero; a player further than 0x32 from the spot is placed 0x32 nearer to it, any other on it
    SVECTOR              localOffset;     // Short vector in a model's own frame: the hold spot 0x4E2 ahead of the golem's root, the throw's sideways travel for the frame, or the offset the kill passes with the player's coordinate 4 when it spawns its effect
    AnimationPlayRequest playerAnim;      // Animation of the package's player set the player is told to install and play at each stage of the grab
    ActorTransform       playerPlacement; // Where `GAME_ACTOR_MESSAGE_PLACE` puts the player; of its angles only the yaw is ever nonzero
} _Actor521100GrabScratch;
STATIC_ASSERT_SIZEOF(_Actor521100GrabScratch, 0x54);

extern s16 D_actor_521100_8015F570[];

extern s16 D_actor_521100_8015F684[];

/// The three waypoints the state-6 body `_actor521100WalkRouteState` walks the
/// actor to, one per phase `subState` it switches on: `(-4000, 0, -2000)` for
/// phases 0 and 2, and `(-5250, 0, -1200)` for phase 1. Only `vx` and `vz` are
/// read, and only when the actor is too far from the player for that phase to
/// aim at it; the y of all three is zero, as the positions are on the floor.
extern VECTOR D_actor_521100_8015F654[];

/// Sixteen frames of the burn-out effect the state bodies at `subState == 1`
/// pick between on their last frame, indexed by the 4 bits under the top half
/// of an LCG draw (`(rng >> 16) & 0xF`). The sibling state body
/// `_actor521100FlinchState` reads the table one slot down at 0x8015F5F4.
extern u16 D_actor_521100_8015F634[];

/// The other sixteen-frame burn-out table, read by the state-5 body
/// `_actor521100FlinchState` off the same LCG draw bits the state-3 body
/// `_actor521100StaggerState` indexes `D_actor_521100_8015F634` with.
extern u16 D_actor_521100_8015F5F4[];

/// The burn-out effect table the sequence resets read, one 0x20-byte table
/// below `D_actor_521100_8015F5F4`: the state-1 body
/// `func_actor_521100_801335B4` draws from it the frames its stance is held,
/// into `stateCounter`.
extern u16 D_actor_521100_8015F5D4[];

/// The 4-byte pair `func_actor_521100_801335B4` packs a type-2 record into and
/// copies onto both `weaponAttack` / `forearmAttack` as their `key`, where the
/// sibling overlays' attack bodies put the same pair. Same shape as
/// `D_actor_510900_80167968` and `D_actor_400100_*`.
extern DamageAttack D_actor_521100_8015F550[4];

extern s16 D_actor_521100_8015F57C[16];

/// The attack that follows one chosen twice running while the player is
/// within 0x8FC: a row per `ACTOR_521100_ATTACK_*` that was repeated, a column
/// per random bit. No row offers its own attack, and none the long slash.
///
/// Both choices start as `ACTOR_521100_ATTACK_NONE` and so agree on the first
/// draw, whose row is then the one before the table: the last two entries of
/// `D_actor_521100_8015F57C`, both the stance. Whether the original tables
/// were laid out to be read that way is unproven.
extern s16 D_actor_521100_8015F59C[3][2];

extern s16 D_actor_521100_8015F5A8[16];

/// The same table for a player further away, where the long slash is offered.
/// Its first draw likewise reads the last two entries of
/// `D_actor_521100_8015F5A8`, both the stance.
extern s16 D_actor_521100_8015F5C8[3][2];

extern u16 D_actor_521100_8015F614[];

/// Main-executable global with no module header yet: the remaining-enemy count.

extern EnemyParams D_actor_521100_8015F560;
extern TaskDesc    D_actor_521100_8015F6E4[];

static void func_actor_521100_801322F8(Task* arg0, TmdObject* arg1, s32 arg2);
static void func_actor_521100_80132958(Task* arg0);
static s32  func_actor_521100_80132C70(Task* arg0);
static void func_actor_521100_80132DE8(Task* arg0);
static void func_actor_521100_80133104(Task* arg0);
static void func_actor_521100_8013334C(Task* arg0);
static void func_actor_521100_801335B4(Task* arg0);
static void func_actor_521100_801339B0(Task* arg0);
static void _actor521100GuardState(Task* task);
static void _actor521100WalkRouteState(Task* task);
static void _actor521100TurnTowardTargetYaw(Task* task);
static void _actor521100PlayFootsteps(Task* task);
static void _actor521100ApplyHitTwist(Task* task);
static void _actor521100TickEventFire(Task* task);
static void func_actor_521100_801353CC(Enemy* arg0, Task* arg1);
static void func_actor_521100_80135414(Enemy* arg0, Task* arg1);
static void func_actor_521100_80135478(Enemy* arg0, Task* arg1);
static void func_actor_521100_801355C8(Task* arg0);
static void _actor521100TryStartAttack(Task* task);
static void _actor521100StaggerState(Task* task);
static void _actor521100FlinchState(Task* task);
static void _actor521100StepRootPosition(Task* task);
static void _actor521100TickAnimation(Task* task);
static void _actor521100UpdateLighting(Task* task);
static void _actor521100AttachGunblade(Enemy* unusedEnemy, Task* task);
static void _actor521100UpdateGunbladeDrawMode(Enemy* unusedEnemy, Task* task);

static TmdSource _gActor521100No9GolemDryfieldBody;
static TmdSource _gActor521100No9GolemDryfieldGunblade;
void             func_actor_521100_80135378(Task*);
static void      _actor521100GunbladeTask(Task* task);

static AnimationPackedPose _gActor521100Animation05474Bank1[6] = {
#include "assets/actor_521100_animation_05474_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation05474Bank4[148] = {
#include "assets/actor_521100_animation_05474_bank4.inc"
};

static AnimationRecord _gActor521100Animation05474Records[201] = {
#include "assets/actor_521100_animation_05474_records.inc"
};

static u16 _gActor521100Animation05474Indices[20] = {
#include "assets/actor_521100_animation_05474_indices.inc"
};

AnimationSet gActor521100Animation05474 = {
    _gActor521100Animation05474Records,
    _gActor521100Animation05474Indices,
    { NULL, _gActor521100Animation05474Bank1, NULL, NULL, _gActor521100Animation05474Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation059C4Bank1[9] = {
#include "assets/actor_521100_animation_059C4_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation059C4Bank4[131] = {
#include "assets/actor_521100_animation_059C4_bank4.inc"
};

static AnimationRecord _gActor521100Animation059C4Records[162] = {
#include "assets/actor_521100_animation_059C4_records.inc"
};

static u16 _gActor521100Animation059C4Indices[20] = {
#include "assets/actor_521100_animation_059C4_indices.inc"
};

AnimationSet gActor521100Animation059C4 = {
    _gActor521100Animation059C4Records,
    _gActor521100Animation059C4Indices,
    { NULL, _gActor521100Animation059C4Bank1, NULL, NULL, _gActor521100Animation059C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation05F98Bank1[8] = {
#include "assets/actor_521100_animation_05F98_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation05F98Bank4[117] = {
#include "assets/actor_521100_animation_05F98_bank4.inc"
};

static AnimationRecord _gActor521100Animation05F98Records[212] = {
#include "assets/actor_521100_animation_05F98_records.inc"
};

static u16 _gActor521100Animation05F98Indices[20] = {
#include "assets/actor_521100_animation_05F98_indices.inc"
};

AnimationSet gActor521100Animation05F98 = {
    _gActor521100Animation05F98Records,
    _gActor521100Animation05F98Indices,
    { NULL, _gActor521100Animation05F98Bank1, NULL, NULL, _gActor521100Animation05F98Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation0623CBank1[5] = {
#include "assets/actor_521100_animation_0623C_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation0623CBank4[32] = {
#include "assets/actor_521100_animation_0623C_bank4.inc"
};

static AnimationRecord _gActor521100Animation0623CRecords[102] = {
#include "assets/actor_521100_animation_0623C_records.inc"
};

static u16 _gActor521100Animation0623CIndices[20] = {
#include "assets/actor_521100_animation_0623C_indices.inc"
};

AnimationSet gActor521100Animation0623C = {
    _gActor521100Animation0623CRecords,
    _gActor521100Animation0623CIndices,
    { NULL, _gActor521100Animation0623CBank1, NULL, NULL, _gActor521100Animation0623CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation07168Bank1[47] = {
#include "assets/actor_521100_animation_07168_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation07168Bank4[243] = {
#include "assets/actor_521100_animation_07168_bank4.inc"
};

static AnimationRecord _gActor521100Animation07168Records[567] = {
#include "assets/actor_521100_animation_07168_records.inc"
};

static u16 _gActor521100Animation07168Indices[20] = {
#include "assets/actor_521100_animation_07168_indices.inc"
};

AnimationSet gActor521100Animation07168 = {
    _gActor521100Animation07168Records,
    _gActor521100Animation07168Indices,
    { NULL, _gActor521100Animation07168Bank1, NULL, NULL, _gActor521100Animation07168Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation07BA4Bank1[16] = {
#include "assets/actor_521100_animation_07BA4_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation07BA4Bank4[246] = {
#include "assets/actor_521100_animation_07BA4_bank4.inc"
};

static AnimationRecord _gActor521100Animation07BA4Records[341] = {
#include "assets/actor_521100_animation_07BA4_records.inc"
};

static u16 _gActor521100Animation07BA4Indices[20] = {
#include "assets/actor_521100_animation_07BA4_indices.inc"
};

AnimationSet gActor521100Animation07BA4 = {
    _gActor521100Animation07BA4Records,
    _gActor521100Animation07BA4Indices,
    { NULL, _gActor521100Animation07BA4Bank1, NULL, NULL, _gActor521100Animation07BA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation0820CBank1[14] = {
#include "assets/actor_521100_animation_0820C_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation0820CBank4[148] = {
#include "assets/actor_521100_animation_0820C_bank4.inc"
};

static AnimationRecord _gActor521100Animation0820CRecords[200] = {
#include "assets/actor_521100_animation_0820C_records.inc"
};

static u16 _gActor521100Animation0820CIndices[20] = {
#include "assets/actor_521100_animation_0820C_indices.inc"
};

AnimationSet gActor521100Animation0820C = {
    _gActor521100Animation0820CRecords,
    _gActor521100Animation0820CIndices,
    { NULL, _gActor521100Animation0820CBank1, NULL, NULL, _gActor521100Animation0820CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation09030Bank1[25] = {
#include "assets/actor_521100_animation_09030_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation09030Bank4[369] = {
#include "assets/actor_521100_animation_09030_bank4.inc"
};

static AnimationRecord _gActor521100Animation09030Records[441] = {
#include "assets/actor_521100_animation_09030_records.inc"
};

static u16 _gActor521100Animation09030Indices[20] = {
#include "assets/actor_521100_animation_09030_indices.inc"
};

AnimationSet gActor521100Animation09030 = {
    _gActor521100Animation09030Records,
    _gActor521100Animation09030Indices,
    { NULL, _gActor521100Animation09030Bank1, NULL, NULL, _gActor521100Animation09030Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation091C0Bank1[2] = {
#include "assets/actor_521100_animation_091C0_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation091C0Bank4[17] = {
#include "assets/actor_521100_animation_091C0_bank4.inc"
};

static AnimationRecord _gActor521100Animation091C0Records[57] = {
#include "assets/actor_521100_animation_091C0_records.inc"
};

static u16 _gActor521100Animation091C0Indices[20] = {
#include "assets/actor_521100_animation_091C0_indices.inc"
};

AnimationSet gActor521100Animation091C0 = {
    _gActor521100Animation091C0Records,
    _gActor521100Animation091C0Indices,
    { NULL, _gActor521100Animation091C0Bank1, NULL, NULL, _gActor521100Animation091C0Bank4, NULL, NULL, NULL },
};

static TmdBone _gActor521100No9GolemDryfieldBodySkeleton[19] = {
#include "assets/no9_golem_dryfield_body_skeleton.inc"
};

static u32 _gActor521100No9GolemDryfieldBodyPartVerts[19] = {
#include "assets/no9_golem_dryfield_body_partVerts.inc"
};

static SVECTOR _gActor521100No9GolemDryfieldBodyVerts[432] = {
#include "assets/no9_golem_dryfield_body_verts.inc"
};

static SVECTOR _gActor521100No9GolemDryfieldBodyNormals[444] = {
#include "assets/no9_golem_dryfield_body_normals.inc"
};

static u32 _gActor521100No9GolemDryfieldBodyStream[4749] = {
#include "assets/no9_golem_dryfield_body_stream.inc"
};

static TmdSource _gActor521100No9GolemDryfieldBody = {
    0,
    26564,
    6624,
    19,
    _gActor521100No9GolemDryfieldBodyPartVerts,
    _gActor521100No9GolemDryfieldBodyVerts,
    _gActor521100No9GolemDryfieldBodyNormals,
    _gActor521100No9GolemDryfieldBodySkeleton,
    _gActor521100No9GolemDryfieldBodyStream,
};

static TmdBone _gActor521100No9GolemDryfieldGunbladeSkeleton[1] = {
#include "assets/no9_golem_dryfield_gunblade_skeleton.inc"
};

static u32 _gActor521100No9GolemDryfieldGunbladePartVerts[1] = {
#include "assets/no9_golem_dryfield_gunblade_partVerts.inc"
};

static SVECTOR _gActor521100No9GolemDryfieldGunbladeVerts[43] = {
#include "assets/no9_golem_dryfield_gunblade_verts.inc"
};

static SVECTOR _gActor521100No9GolemDryfieldGunbladeNormals[41] = {
#include "assets/no9_golem_dryfield_gunblade_normals.inc"
};

static u32 _gActor521100No9GolemDryfieldGunbladeStream[326] = {
#include "assets/no9_golem_dryfield_gunblade_stream.inc"
};

static TmdSource _gActor521100No9GolemDryfieldGunblade = {
    0,
    2300,
    0,
    1,
    _gActor521100No9GolemDryfieldGunbladePartVerts,
    _gActor521100No9GolemDryfieldGunbladeVerts,
    _gActor521100No9GolemDryfieldGunbladeNormals,
    _gActor521100No9GolemDryfieldGunbladeSkeleton,
    _gActor521100No9GolemDryfieldGunbladeStream,
};

static AnimationPackedPose _gActor521100Animation10EACBank1[21] = {
#include "assets/actor_521100_animation_10EAC_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation10EACBank4[317] = {
#include "assets/actor_521100_animation_10EAC_bank4.inc"
};

static AnimationRecord _gActor521100Animation10EACRecords[382] = {
#include "assets/actor_521100_animation_10EAC_records.inc"
};

static u16 _gActor521100Animation10EACIndices[20] = {
#include "assets/actor_521100_animation_10EAC_indices.inc"
};

AnimationSet gActor521100Animation10EAC = {
    _gActor521100Animation10EACRecords,
    _gActor521100Animation10EACIndices,
    { NULL, _gActor521100Animation10EACBank1, NULL, NULL, _gActor521100Animation10EACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation11614Bank1[12] = {
#include "assets/actor_521100_animation_11614_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation11614Bank4[187] = {
#include "assets/actor_521100_animation_11614_bank4.inc"
};

static AnimationRecord _gActor521100Animation11614Records[231] = {
#include "assets/actor_521100_animation_11614_records.inc"
};

static u16 _gActor521100Animation11614Indices[20] = {
#include "assets/actor_521100_animation_11614_indices.inc"
};

AnimationSet gActor521100Animation11614 = {
    _gActor521100Animation11614Records,
    _gActor521100Animation11614Indices,
    { NULL, _gActor521100Animation11614Bank1, NULL, NULL, _gActor521100Animation11614Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation12204Bank1[20] = {
#include "assets/actor_521100_animation_12204_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation12204Bank4[321] = {
#include "assets/actor_521100_animation_12204_bank4.inc"
};

static AnimationRecord _gActor521100Animation12204Records[363] = {
#include "assets/actor_521100_animation_12204_records.inc"
};

static u16 _gActor521100Animation12204Indices[20] = {
#include "assets/actor_521100_animation_12204_indices.inc"
};

AnimationSet gActor521100Animation12204 = {
    _gActor521100Animation12204Records,
    _gActor521100Animation12204Indices,
    { NULL, _gActor521100Animation12204Bank1, NULL, NULL, _gActor521100Animation12204Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation1271CBank1[5] = {
#include "assets/actor_521100_animation_1271C_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation1271CBank4[99] = {
#include "assets/actor_521100_animation_1271C_bank4.inc"
};

static AnimationRecord _gActor521100Animation1271CRecords[192] = {
#include "assets/actor_521100_animation_1271C_records.inc"
};

static u16 _gActor521100Animation1271CIndices[20] = {
#include "assets/actor_521100_animation_1271C_indices.inc"
};

AnimationSet gActor521100Animation1271C = {
    _gActor521100Animation1271CRecords,
    _gActor521100Animation1271CIndices,
    { NULL, _gActor521100Animation1271CBank1, NULL, NULL, _gActor521100Animation1271CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation13454Bank1[21] = {
#include "assets/actor_521100_animation_13454_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation13454Bank4[354] = {
#include "assets/actor_521100_animation_13454_bank4.inc"
};

static AnimationRecord _gActor521100Animation13454Records[409] = {
#include "assets/actor_521100_animation_13454_records.inc"
};

static u16 _gActor521100Animation13454Indices[20] = {
#include "assets/actor_521100_animation_13454_indices.inc"
};

AnimationSet gActor521100Animation13454 = {
    _gActor521100Animation13454Records,
    _gActor521100Animation13454Indices,
    { NULL, _gActor521100Animation13454Bank1, NULL, NULL, _gActor521100Animation13454Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation13B8CBank1[16] = {
#include "assets/actor_521100_animation_13B8C_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation13B8CBank4[177] = {
#include "assets/actor_521100_animation_13B8C_bank4.inc"
};

static AnimationRecord _gActor521100Animation13B8CRecords[217] = {
#include "assets/actor_521100_animation_13B8C_records.inc"
};

static u16 _gActor521100Animation13B8CIndices[20] = {
#include "assets/actor_521100_animation_13B8C_indices.inc"
};

AnimationSet gActor521100Animation13B8C = {
    _gActor521100Animation13B8CRecords,
    _gActor521100Animation13B8CIndices,
    { NULL, _gActor521100Animation13B8CBank1, NULL, NULL, _gActor521100Animation13B8CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation143F0Bank1[14] = {
#include "assets/actor_521100_animation_143F0_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation143F0Bank4[220] = {
#include "assets/actor_521100_animation_143F0_bank4.inc"
};

static AnimationRecord _gActor521100Animation143F0Records[255] = {
#include "assets/actor_521100_animation_143F0_records.inc"
};

static u16 _gActor521100Animation143F0Indices[20] = {
#include "assets/actor_521100_animation_143F0_indices.inc"
};

AnimationSet gActor521100Animation143F0 = {
    _gActor521100Animation143F0Records,
    _gActor521100Animation143F0Indices,
    { NULL, _gActor521100Animation143F0Bank1, NULL, NULL, _gActor521100Animation143F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation14918Bank1[9] = {
#include "assets/actor_521100_animation_14918_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation14918Bank4[124] = {
#include "assets/actor_521100_animation_14918_bank4.inc"
};

static AnimationRecord _gActor521100Animation14918Records[159] = {
#include "assets/actor_521100_animation_14918_records.inc"
};

static u16 _gActor521100Animation14918Indices[20] = {
#include "assets/actor_521100_animation_14918_indices.inc"
};

AnimationSet gActor521100Animation14918 = {
    _gActor521100Animation14918Records,
    _gActor521100Animation14918Indices,
    { NULL, _gActor521100Animation14918Bank1, NULL, NULL, _gActor521100Animation14918Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation14C38Bank1[5] = {
#include "assets/actor_521100_animation_14C38_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation14C38Bank4[55] = {
#include "assets/actor_521100_animation_14C38_bank4.inc"
};

static AnimationRecord _gActor521100Animation14C38Records[110] = {
#include "assets/actor_521100_animation_14C38_records.inc"
};

static u16 _gActor521100Animation14C38Indices[20] = {
#include "assets/actor_521100_animation_14C38_indices.inc"
};

AnimationSet gActor521100Animation14C38 = {
    _gActor521100Animation14C38Records,
    _gActor521100Animation14C38Indices,
    { NULL, _gActor521100Animation14C38Bank1, NULL, NULL, _gActor521100Animation14C38Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation15168Bank1[10] = {
#include "assets/actor_521100_animation_15168_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation15168Bank4[121] = {
#include "assets/actor_521100_animation_15168_bank4.inc"
};

static AnimationRecord _gActor521100Animation15168Records[161] = {
#include "assets/actor_521100_animation_15168_records.inc"
};

static u16 _gActor521100Animation15168Indices[20] = {
#include "assets/actor_521100_animation_15168_indices.inc"
};

AnimationSet gActor521100Animation15168 = {
    _gActor521100Animation15168Records,
    _gActor521100Animation15168Indices,
    { NULL, _gActor521100Animation15168Bank1, NULL, NULL, _gActor521100Animation15168Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation152F8Bank1[2] = {
#include "assets/actor_521100_animation_152F8_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation152F8Bank4[17] = {
#include "assets/actor_521100_animation_152F8_bank4.inc"
};

static AnimationRecord _gActor521100Animation152F8Records[57] = {
#include "assets/actor_521100_animation_152F8_records.inc"
};

static u16 _gActor521100Animation152F8Indices[20] = {
#include "assets/actor_521100_animation_152F8_indices.inc"
};

AnimationSet gActor521100Animation152F8 = {
    _gActor521100Animation152F8Records,
    _gActor521100Animation152F8Indices,
    { NULL, _gActor521100Animation152F8Bank1, NULL, NULL, _gActor521100Animation152F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation15904Bank1[9] = {
#include "assets/actor_521100_animation_15904_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation15904Bank4[151] = {
#include "assets/actor_521100_animation_15904_bank4.inc"
};

static AnimationRecord _gActor521100Animation15904Records[189] = {
#include "assets/actor_521100_animation_15904_records.inc"
};

static u16 _gActor521100Animation15904Indices[20] = {
#include "assets/actor_521100_animation_15904_indices.inc"
};

AnimationSet gActor521100Animation15904 = {
    _gActor521100Animation15904Records,
    _gActor521100Animation15904Indices,
    { NULL, _gActor521100Animation15904Bank1, NULL, NULL, _gActor521100Animation15904Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation15E28Bank1[8] = {
#include "assets/actor_521100_animation_15E28_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation15E28Bank4[116] = {
#include "assets/actor_521100_animation_15E28_bank4.inc"
};

static AnimationRecord _gActor521100Animation15E28Records[169] = {
#include "assets/actor_521100_animation_15E28_records.inc"
};

static u16 _gActor521100Animation15E28Indices[20] = {
#include "assets/actor_521100_animation_15E28_indices.inc"
};

AnimationSet gActor521100Animation15E28 = {
    _gActor521100Animation15E28Records,
    _gActor521100Animation15E28Indices,
    { NULL, _gActor521100Animation15E28Bank1, NULL, NULL, _gActor521100Animation15E28Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation16628Bank1[10] = {
#include "assets/actor_521100_animation_16628_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation16628Bank4[195] = {
#include "assets/actor_521100_animation_16628_bank4.inc"
};

static AnimationRecord _gActor521100Animation16628Records[267] = {
#include "assets/actor_521100_animation_16628_records.inc"
};

static u16 _gActor521100Animation16628Indices[20] = {
#include "assets/actor_521100_animation_16628_indices.inc"
};

AnimationSet gActor521100Animation16628 = {
    _gActor521100Animation16628Records,
    _gActor521100Animation16628Indices,
    { NULL, _gActor521100Animation16628Bank1, NULL, NULL, _gActor521100Animation16628Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation16F90Bank1[16] = {
#include "assets/actor_521100_animation_16F90_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation16F90Bank4[237] = {
#include "assets/actor_521100_animation_16F90_bank4.inc"
};

static AnimationRecord _gActor521100Animation16F90Records[297] = {
#include "assets/actor_521100_animation_16F90_records.inc"
};

static u16 _gActor521100Animation16F90Indices[20] = {
#include "assets/actor_521100_animation_16F90_indices.inc"
};

AnimationSet gActor521100Animation16F90 = {
    _gActor521100Animation16F90Records,
    _gActor521100Animation16F90Indices,
    { NULL, _gActor521100Animation16F90Bank1, NULL, NULL, _gActor521100Animation16F90Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation18B20Bank1[53] = {
#include "assets/actor_521100_animation_18B20_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation18B20Bank4[738] = {
#include "assets/actor_521100_animation_18B20_bank4.inc"
};

static AnimationRecord _gActor521100Animation18B20Records[847] = {
#include "assets/actor_521100_animation_18B20_records.inc"
};

static u16 _gActor521100Animation18B20Indices[20] = {
#include "assets/actor_521100_animation_18B20_indices.inc"
};

AnimationSet gActor521100Animation18B20 = {
    _gActor521100Animation18B20Records,
    _gActor521100Animation18B20Indices,
    { NULL, _gActor521100Animation18B20Bank1, NULL, NULL, _gActor521100Animation18B20Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation1A8E4Bank1[50] = {
#include "assets/actor_521100_animation_1A8E4_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation1A8E4Bank4[812] = {
#include "assets/actor_521100_animation_1A8E4_bank4.inc"
};

static AnimationRecord _gActor521100Animation1A8E4Records[923] = {
#include "assets/actor_521100_animation_1A8E4_records.inc"
};

static u16 _gActor521100Animation1A8E4Indices[20] = {
#include "assets/actor_521100_animation_1A8E4_indices.inc"
};

AnimationSet gActor521100Animation1A8E4 = {
    _gActor521100Animation1A8E4Records,
    _gActor521100Animation1A8E4Indices,
    { NULL, _gActor521100Animation1A8E4Bank1, NULL, NULL, _gActor521100Animation1A8E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation1B8F8Bank1[26] = {
#include "assets/actor_521100_animation_1B8F8_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation1B8F8Bank4[434] = {
#include "assets/actor_521100_animation_1B8F8_bank4.inc"
};

static AnimationRecord _gActor521100Animation1B8F8Records[497] = {
#include "assets/actor_521100_animation_1B8F8_records.inc"
};

static u16 _gActor521100Animation1B8F8Indices[20] = {
#include "assets/actor_521100_animation_1B8F8_indices.inc"
};

AnimationSet gActor521100Animation1B8F8 = {
    _gActor521100Animation1B8F8Records,
    _gActor521100Animation1B8F8Indices,
    { NULL, _gActor521100Animation1B8F8Bank1, NULL, NULL, _gActor521100Animation1B8F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation1C104Bank1[15] = {
#include "assets/actor_521100_animation_1C104_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation1C104Bank4[197] = {
#include "assets/actor_521100_animation_1C104_bank4.inc"
};

static AnimationRecord _gActor521100Animation1C104Records[253] = {
#include "assets/actor_521100_animation_1C104_records.inc"
};

static u16 _gActor521100Animation1C104Indices[20] = {
#include "assets/actor_521100_animation_1C104_indices.inc"
};

AnimationSet gActor521100Animation1C104 = {
    _gActor521100Animation1C104Records,
    _gActor521100Animation1C104Indices,
    { NULL, _gActor521100Animation1C104Bank1, NULL, NULL, _gActor521100Animation1C104Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation1C58CBank1[5] = {
#include "assets/actor_521100_animation_1C58C_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation1C58CBank4[87] = {
#include "assets/actor_521100_animation_1C58C_bank4.inc"
};

static AnimationRecord _gActor521100Animation1C58CRecords[168] = {
#include "assets/actor_521100_animation_1C58C_records.inc"
};

static u16 _gActor521100Animation1C58CIndices[20] = {
#include "assets/actor_521100_animation_1C58C_indices.inc"
};

AnimationSet gActor521100Animation1C58C = {
    _gActor521100Animation1C58CRecords,
    _gActor521100Animation1C58CIndices,
    { NULL, _gActor521100Animation1C58CBank1, NULL, NULL, _gActor521100Animation1C58CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation20880Bank1[147] = {
#include "assets/actor_521100_animation_20880_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation20880Bank4[1395] = {
#include "assets/actor_521100_animation_20880_bank4.inc"
};

static AnimationRecord _gActor521100Animation20880Records[2429] = {
#include "assets/actor_521100_animation_20880_records.inc"
};

static u16 _gActor521100Animation20880Indices[20] = {
#include "assets/actor_521100_animation_20880_indices.inc"
};

AnimationSet gActor521100Animation20880 = {
    _gActor521100Animation20880Records,
    _gActor521100Animation20880Indices,
    { NULL, _gActor521100Animation20880Bank1, NULL, NULL, _gActor521100Animation20880Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation20F94Bank1[11] = {
#include "assets/actor_521100_animation_20F94_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation20F94Bank4[162] = {
#include "assets/actor_521100_animation_20F94_bank4.inc"
};

static AnimationRecord _gActor521100Animation20F94Records[238] = {
#include "assets/actor_521100_animation_20F94_records.inc"
};

static u16 _gActor521100Animation20F94Indices[20] = {
#include "assets/actor_521100_animation_20F94_indices.inc"
};

AnimationSet gActor521100Animation20F94 = {
    _gActor521100Animation20F94Records,
    _gActor521100Animation20F94Indices,
    { NULL, _gActor521100Animation20F94Bank1, NULL, NULL, _gActor521100Animation20F94Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation2177CBank1[14] = {
#include "assets/actor_521100_animation_2177C_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation2177CBank4[170] = {
#include "assets/actor_521100_animation_2177C_bank4.inc"
};

static AnimationRecord _gActor521100Animation2177CRecords[274] = {
#include "assets/actor_521100_animation_2177C_records.inc"
};

static u16 _gActor521100Animation2177CIndices[20] = {
#include "assets/actor_521100_animation_2177C_indices.inc"
};

AnimationSet gActor521100Animation2177C = {
    _gActor521100Animation2177CRecords,
    _gActor521100Animation2177CIndices,
    { NULL, _gActor521100Animation2177CBank1, NULL, NULL, _gActor521100Animation2177CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation220B8Bank1[13] = {
#include "assets/actor_521100_animation_220B8_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation220B8Bank4[206] = {
#include "assets/actor_521100_animation_220B8_bank4.inc"
};

static AnimationRecord _gActor521100Animation220B8Records[326] = {
#include "assets/actor_521100_animation_220B8_records.inc"
};

static u16 _gActor521100Animation220B8Indices[20] = {
#include "assets/actor_521100_animation_220B8_indices.inc"
};

AnimationSet gActor521100Animation220B8 = {
    _gActor521100Animation220B8Records,
    _gActor521100Animation220B8Indices,
    { NULL, _gActor521100Animation220B8Bank1, NULL, NULL, _gActor521100Animation220B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation229A0Bank1[12] = {
#include "assets/actor_521100_animation_229A0_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation229A0Bank4[194] = {
#include "assets/actor_521100_animation_229A0_bank4.inc"
};

static AnimationRecord _gActor521100Animation229A0Records[320] = {
#include "assets/actor_521100_animation_229A0_records.inc"
};

static u16 _gActor521100Animation229A0Indices[20] = {
#include "assets/actor_521100_animation_229A0_indices.inc"
};

AnimationSet gActor521100Animation229A0 = {
    _gActor521100Animation229A0Records,
    _gActor521100Animation229A0Indices,
    { NULL, _gActor521100Animation229A0Bank1, NULL, NULL, _gActor521100Animation229A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation2318CBank1[10] = {
#include "assets/actor_521100_animation_2318C_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation2318CBank4[175] = {
#include "assets/actor_521100_animation_2318C_bank4.inc"
};

static AnimationRecord _gActor521100Animation2318CRecords[282] = {
#include "assets/actor_521100_animation_2318C_records.inc"
};

static u16 _gActor521100Animation2318CIndices[20] = {
#include "assets/actor_521100_animation_2318C_indices.inc"
};

AnimationSet gActor521100Animation2318C = {
    _gActor521100Animation2318CRecords,
    _gActor521100Animation2318CIndices,
    { NULL, _gActor521100Animation2318CBank1, NULL, NULL, _gActor521100Animation2318CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation239C0Bank1[12] = {
#include "assets/actor_521100_animation_239C0_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation239C0Bank4[179] = {
#include "assets/actor_521100_animation_239C0_bank4.inc"
};

static AnimationRecord _gActor521100Animation239C0Records[290] = {
#include "assets/actor_521100_animation_239C0_records.inc"
};

static u16 _gActor521100Animation239C0Indices[20] = {
#include "assets/actor_521100_animation_239C0_indices.inc"
};

AnimationSet gActor521100Animation239C0 = {
    _gActor521100Animation239C0Records,
    _gActor521100Animation239C0Indices,
    { NULL, _gActor521100Animation239C0Bank1, NULL, NULL, _gActor521100Animation239C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation24F5CBank1[48] = {
#include "assets/actor_521100_animation_24F5C_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation24F5CBank4[554] = {
#include "assets/actor_521100_animation_24F5C_bank4.inc"
};

static AnimationRecord _gActor521100Animation24F5CRecords[665] = {
#include "assets/actor_521100_animation_24F5C_records.inc"
};

static u16 _gActor521100Animation24F5CIndices[20] = {
#include "assets/actor_521100_animation_24F5C_indices.inc"
};

AnimationSet gActor521100Animation24F5C = {
    _gActor521100Animation24F5CRecords,
    _gActor521100Animation24F5CIndices,
    { NULL, _gActor521100Animation24F5CBank1, NULL, NULL, _gActor521100Animation24F5CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation25ACCBank1[13] = {
#include "assets/actor_521100_animation_25ACC_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation25ACCBank4[259] = {
#include "assets/actor_521100_animation_25ACC_bank4.inc"
};

static AnimationRecord _gActor521100Animation25ACCRecords[414] = {
#include "assets/actor_521100_animation_25ACC_records.inc"
};

static u16 _gActor521100Animation25ACCIndices[20] = {
#include "assets/actor_521100_animation_25ACC_indices.inc"
};

AnimationSet gActor521100Animation25ACC = {
    _gActor521100Animation25ACCRecords,
    _gActor521100Animation25ACCIndices,
    { NULL, _gActor521100Animation25ACCBank1, NULL, NULL, _gActor521100Animation25ACCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation26024Bank1[8] = {
#include "assets/actor_521100_animation_26024_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation26024Bank4[117] = {
#include "assets/actor_521100_animation_26024_bank4.inc"
};

static AnimationRecord _gActor521100Animation26024Records[181] = {
#include "assets/actor_521100_animation_26024_records.inc"
};

static u16 _gActor521100Animation26024Indices[20] = {
#include "assets/actor_521100_animation_26024_indices.inc"
};

AnimationSet gActor521100Animation26024 = {
    _gActor521100Animation26024Records,
    _gActor521100Animation26024Indices,
    { NULL, _gActor521100Animation26024Bank1, NULL, NULL, _gActor521100Animation26024Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation26E3CBank1[25] = {
#include "assets/actor_521100_animation_26E3C_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation26E3CBank4[372] = {
#include "assets/actor_521100_animation_26E3C_bank4.inc"
};

static AnimationRecord _gActor521100Animation26E3CRecords[435] = {
#include "assets/actor_521100_animation_26E3C_records.inc"
};

static u16 _gActor521100Animation26E3CIndices[20] = {
#include "assets/actor_521100_animation_26E3C_indices.inc"
};

AnimationSet gActor521100Animation26E3C = {
    _gActor521100Animation26E3CRecords,
    _gActor521100Animation26E3CIndices,
    { NULL, _gActor521100Animation26E3CBank1, NULL, NULL, _gActor521100Animation26E3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation26FCCBank1[2] = {
#include "assets/actor_521100_animation_26FCC_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation26FCCBank4[17] = {
#include "assets/actor_521100_animation_26FCC_bank4.inc"
};

static AnimationRecord _gActor521100Animation26FCCRecords[57] = {
#include "assets/actor_521100_animation_26FCC_records.inc"
};

static u16 _gActor521100Animation26FCCIndices[20] = {
#include "assets/actor_521100_animation_26FCC_indices.inc"
};

AnimationSet gActor521100Animation26FCC = {
    _gActor521100Animation26FCCRecords,
    _gActor521100Animation26FCCIndices,
    { NULL, _gActor521100Animation26FCCBank1, NULL, NULL, _gActor521100Animation26FCCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation271A8Bank1[2] = {
#include "assets/actor_521100_animation_271A8_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation271A8Bank4[17] = {
#include "assets/actor_521100_animation_271A8_bank4.inc"
};

static AnimationRecord _gActor521100Animation271A8Records[76] = {
#include "assets/actor_521100_animation_271A8_records.inc"
};

static u16 _gActor521100Animation271A8Indices[20] = {
#include "assets/actor_521100_animation_271A8_indices.inc"
};

AnimationSet gActor521100Animation271A8 = {
    _gActor521100Animation271A8Records,
    _gActor521100Animation271A8Indices,
    { NULL, _gActor521100Animation271A8Bank1, NULL, NULL, _gActor521100Animation271A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation27A78Bank1[19] = {
#include "assets/actor_521100_animation_27A78_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation27A78Bank4[211] = {
#include "assets/actor_521100_animation_27A78_bank4.inc"
};

static AnimationRecord _gActor521100Animation27A78Records[276] = {
#include "assets/actor_521100_animation_27A78_records.inc"
};

static u16 _gActor521100Animation27A78Indices[20] = {
#include "assets/actor_521100_animation_27A78_indices.inc"
};

AnimationSet gActor521100Animation27A78 = {
    _gActor521100Animation27A78Records,
    _gActor521100Animation27A78Indices,
    { NULL, _gActor521100Animation27A78Bank1, NULL, NULL, _gActor521100Animation27A78Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation27DB0Bank1[6] = {
#include "assets/actor_521100_animation_27DB0_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation27DB0Bank4[63] = {
#include "assets/actor_521100_animation_27DB0_bank4.inc"
};

static AnimationRecord _gActor521100Animation27DB0Records[105] = {
#include "assets/actor_521100_animation_27DB0_records.inc"
};

static u16 _gActor521100Animation27DB0Indices[20] = {
#include "assets/actor_521100_animation_27DB0_indices.inc"
};

AnimationSet gActor521100Animation27DB0 = {
    _gActor521100Animation27DB0Records,
    _gActor521100Animation27DB0Indices,
    { NULL, _gActor521100Animation27DB0Bank1, NULL, NULL, _gActor521100Animation27DB0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation28380Bank1[11] = {
#include "assets/actor_521100_animation_28380_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation28380Bank4[131] = {
#include "assets/actor_521100_animation_28380_bank4.inc"
};

static AnimationRecord _gActor521100Animation28380Records[188] = {
#include "assets/actor_521100_animation_28380_records.inc"
};

static u16 _gActor521100Animation28380Indices[20] = {
#include "assets/actor_521100_animation_28380_indices.inc"
};

AnimationSet gActor521100Animation28380 = {
    _gActor521100Animation28380Records,
    _gActor521100Animation28380Indices,
    { NULL, _gActor521100Animation28380Bank1, NULL, NULL, _gActor521100Animation28380Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation28DD8Bank1[17] = {
#include "assets/actor_521100_animation_28DD8_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation28DD8Bank4[212] = {
#include "assets/actor_521100_animation_28DD8_bank4.inc"
};

static AnimationRecord _gActor521100Animation28DD8Records[379] = {
#include "assets/actor_521100_animation_28DD8_records.inc"
};

static u16 _gActor521100Animation28DD8Indices[20] = {
#include "assets/actor_521100_animation_28DD8_indices.inc"
};

AnimationSet gActor521100Animation28DD8 = {
    _gActor521100Animation28DD8Records,
    _gActor521100Animation28DD8Indices,
    { NULL, _gActor521100Animation28DD8Bank1, NULL, NULL, _gActor521100Animation28DD8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation291F8Bank1[8] = {
#include "assets/actor_521100_animation_291F8_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation291F8Bank4[93] = {
#include "assets/actor_521100_animation_291F8_bank4.inc"
};

static AnimationRecord _gActor521100Animation291F8Records[127] = {
#include "assets/actor_521100_animation_291F8_records.inc"
};

static u16 _gActor521100Animation291F8Indices[20] = {
#include "assets/actor_521100_animation_291F8_indices.inc"
};

AnimationSet gActor521100Animation291F8 = {
    _gActor521100Animation291F8Records,
    _gActor521100Animation291F8Indices,
    { NULL, _gActor521100Animation291F8Bank1, NULL, NULL, _gActor521100Animation291F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation29534Bank1[6] = {
#include "assets/actor_521100_animation_29534_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation29534Bank4[64] = {
#include "assets/actor_521100_animation_29534_bank4.inc"
};

static AnimationRecord _gActor521100Animation29534Records[105] = {
#include "assets/actor_521100_animation_29534_records.inc"
};

static u16 _gActor521100Animation29534Indices[20] = {
#include "assets/actor_521100_animation_29534_indices.inc"
};

AnimationSet gActor521100Animation29534 = {
    _gActor521100Animation29534Records,
    _gActor521100Animation29534Indices,
    { NULL, _gActor521100Animation29534Bank1, NULL, NULL, _gActor521100Animation29534Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation29A88Bank1[10] = {
#include "assets/actor_521100_animation_29A88_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation29A88Bank4[117] = {
#include "assets/actor_521100_animation_29A88_bank4.inc"
};

static AnimationRecord _gActor521100Animation29A88Records[174] = {
#include "assets/actor_521100_animation_29A88_records.inc"
};

static u16 _gActor521100Animation29A88Indices[20] = {
#include "assets/actor_521100_animation_29A88_indices.inc"
};

AnimationSet gActor521100Animation29A88 = {
    _gActor521100Animation29A88Records,
    _gActor521100Animation29A88Indices,
    { NULL, _gActor521100Animation29A88Bank1, NULL, NULL, _gActor521100Animation29A88Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation2A998Bank1[28] = {
#include "assets/actor_521100_animation_2A998_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation2A998Bank4[400] = {
#include "assets/actor_521100_animation_2A998_bank4.inc"
};

static AnimationRecord _gActor521100Animation2A998Records[460] = {
#include "assets/actor_521100_animation_2A998_records.inc"
};

static u16 _gActor521100Animation2A998Indices[20] = {
#include "assets/actor_521100_animation_2A998_indices.inc"
};

AnimationSet gActor521100Animation2A998 = {
    _gActor521100Animation2A998Records,
    _gActor521100Animation2A998Indices,
    { NULL, _gActor521100Animation2A998Bank1, NULL, NULL, _gActor521100Animation2A998Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation2B89CBank1[33] = {
#include "assets/actor_521100_animation_2B89C_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation2B89CBank4[385] = {
#include "assets/actor_521100_animation_2B89C_bank4.inc"
};

static AnimationRecord _gActor521100Animation2B89CRecords[457] = {
#include "assets/actor_521100_animation_2B89C_records.inc"
};

static u16 _gActor521100Animation2B89CIndices[20] = {
#include "assets/actor_521100_animation_2B89C_indices.inc"
};

AnimationSet gActor521100Animation2B89C = {
    _gActor521100Animation2B89CRecords,
    _gActor521100Animation2B89CIndices,
    { NULL, _gActor521100Animation2B89CBank1, NULL, NULL, _gActor521100Animation2B89CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation2C378Bank1[19] = {
#include "assets/actor_521100_animation_2C378_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation2C378Bank4[289] = {
#include "assets/actor_521100_animation_2C378_bank4.inc"
};

static AnimationRecord _gActor521100Animation2C378Records[329] = {
#include "assets/actor_521100_animation_2C378_records.inc"
};

static u16 _gActor521100Animation2C378Indices[20] = {
#include "assets/actor_521100_animation_2C378_indices.inc"
};

AnimationSet gActor521100Animation2C378 = {
    _gActor521100Animation2C378Records,
    _gActor521100Animation2C378Indices,
    { NULL, _gActor521100Animation2C378Bank1, NULL, NULL, _gActor521100Animation2C378Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation2CCA8Bank1[17] = {
#include "assets/actor_521100_animation_2CCA8_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation2CCA8Bank4[234] = {
#include "assets/actor_521100_animation_2CCA8_bank4.inc"
};

static AnimationRecord _gActor521100Animation2CCA8Records[283] = {
#include "assets/actor_521100_animation_2CCA8_records.inc"
};

static u16 _gActor521100Animation2CCA8Indices[20] = {
#include "assets/actor_521100_animation_2CCA8_indices.inc"
};

AnimationSet gActor521100Animation2CCA8 = {
    _gActor521100Animation2CCA8Records,
    _gActor521100Animation2CCA8Indices,
    { NULL, _gActor521100Animation2CCA8Bank1, NULL, NULL, _gActor521100Animation2CCA8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor521100Animation2D708Bank1[21] = {
#include "assets/actor_521100_animation_2D708_bank1.inc"
};

static AnimationPackedRotation _gActor521100Animation2D708Bank4[261] = {
#include "assets/actor_521100_animation_2D708_bank4.inc"
};

static AnimationRecord _gActor521100Animation2D708Records[320] = {
#include "assets/actor_521100_animation_2D708_records.inc"
};

static u16 _gActor521100Animation2D708Indices[20] = {
#include "assets/actor_521100_animation_2D708_indices.inc"
};

AnimationSet gActor521100Animation2D708 = {
    _gActor521100Animation2D708Records,
    _gActor521100Animation2D708Indices,
    { NULL, _gActor521100Animation2D708Bank1, NULL, NULL, _gActor521100Animation2D708Bank4, NULL, NULL, NULL },
};

DamageAttack D_actor_521100_8015F550[4] = {
    { 15, 7 },
    { 25, 7 },
    { 40, 7 },
    { 10, 0 },
};

EnemyParams D_actor_521100_8015F560 = { D_actor_521100_8015F550, 1100, 800, 300, 50, 0, 0, 0, 0 };

s16 D_actor_521100_8015F570[6] = {
    6,
    11,
    16,
    16,
    4,
    0,
};

s16 D_actor_521100_8015F57C[16] = {
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
    2,
    2,
    2,
    2,
    2,
    2,
};

s16 D_actor_521100_8015F59C[3][2] = {
    { ACTOR_521100_ATTACK_STANCE, ACTOR_521100_ATTACK_STANCE },
    { ACTOR_521100_ATTACK_SLASH, ACTOR_521100_ATTACK_STANCE },
    { ACTOR_521100_ATTACK_SLASH, ACTOR_521100_ATTACK_SLASH },
};

s16 D_actor_521100_8015F5A8[16] = {
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
};

s16 D_actor_521100_8015F5C8[3][2] = {
    { ACTOR_521100_ATTACK_LONG_SLASH, ACTOR_521100_ATTACK_STANCE },
    { ACTOR_521100_ATTACK_SLASH, ACTOR_521100_ATTACK_STANCE },
    { ACTOR_521100_ATTACK_SLASH, ACTOR_521100_ATTACK_LONG_SLASH },
};

u16 D_actor_521100_8015F5D4[16] = { 0 };

u16 D_actor_521100_8015F5F4[16] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    15,
    15,
    15,
    15,
    30,
    30,
    30,
    30,
};

u16 D_actor_521100_8015F614[16] = {
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
};

u16 D_actor_521100_8015F634[16] = { 0 };

VECTOR D_actor_521100_8015F654[3] = {
    { -4000, 0, -2000, 0 },
    { -5250, 0, -1200, 0 },
    { -4000, 0, -2000, 0 },
};

s16 D_actor_521100_8015F684[48] = {
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
    1,
    1,
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

TaskDesc D_actor_521100_8015F6E4[2] = {
    { { { TASK_BODY_TMD, 96 } }, func_actor_521100_80135378, { .model = &_gActor521100No9GolemDryfieldBody } },
    { { { TASK_BODY_TMD, 96 } }, _actor521100GunbladeTask, { .model = &_gActor521100No9GolemDryfieldGunblade } },
};

TaskMessageEntry D_actor_521100_8015F6FC[8] = {
    { 2014, func_actor_521100_80135BEC },
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_521100_80135C14 },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceRotMatrix },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actor521100SetModelDrawFlags },
    { ACTOR_COMMAND_MESSAGE_APPLY, actor521100ApplyCommand },
    { ACTOR_521100_MESSAGE_ACTIVATE, actor521100Activate },
    { ACTOR_MESSAGE_IS_PRESENT, actor521100IsPresent },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static void           func_actor_521100_80131E8C(Enemy* enemy, Task* task);
static __inline__ s32 _actor521100GetBaseHitResponse(s32 attackKey);

/// Spawn state of the actor: allocates its 0x6C0 work block, registers the
/// enemy on the lock-on list with its parameter record, contact table and body
/// coordinate (the model's fourth part), and starts the animation on clip 0x15.
/// It then links the actor's collision bodies, spawns a second enemy from
/// `D_actor_521100_8015F6E4` with this one as its parent, dresses that enemy's
/// model with the texture page and CLUT of this enemy's placement, and links
/// two more pairs of bodies, one of them placed on the second enemy's model.
///
/// Every body's coordinate is assigned first in its block: the model pointer is
/// reloaded from the task each time, and that load has to precede the stores
/// into the work block, which it cannot be scheduled across.
static void func_actor_521100_80131E8C(Enemy* enemy, Task* task)
{
    GameLocationKey  key;
    TmdObject*       obj;
    GfxCoord*        coord;
    Actor521100Work* work;
    Enemy*           spawned;
    TmdObject*       model;
    GameLocationKey* sessionKey;
    AreaPlacement*   place;
    s32              idx;
    u32              raw;
    s32              i;

    obj   = task->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(sizeof(Actor521100Work), false);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work          = work;
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->light;
    obj->colorMtx       = &work->color;
    enemy->field_4      = &coord->coord;
    enemy->field_48     = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord                  = &task->extra.tmd->coords[3];
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->param                  = &D_actor_521100_8015F560;
    enemy->recs                   = work->bodyContacts;
    enemy->hp                     = D_actor_521100_8015F560.hpMax;
    work->hitEffectArg.coord      = &task->extra.tmd->coords[3];
    work->hitEffectArg.spawnArgLo = 0x400;
    work->hitEffectArg.spawnArgHi = 3;
    animationInitContext(&work->rig.anim, D_actor_521100_8015F73C, obj, work->rig.poses, work->rig.slots);
    work->animationId       = 0x15;
    work->seededAnimationId = 0x15;
    i                       = 1;
    do {
        animationResetSlot(&work->rig.anim, i, work->animationId);
        i++;
    } while (i < 0x13);
    work->present          = 1;
    work->attackChoice     = ACTOR_521100_ATTACK_NONE;
    work->prevAttackChoice = ACTOR_521100_ATTACK_NONE;

    work->groundBody.coord            = task->extra.tmd->coords;
    work->groundBody.context.contacts = work->groundContacts;
    work->groundBody.pos.vx           = 0;
    work->groundBody.pos.vy           = -0x190;
    work->groundBody.pos.vz           = 0;
    work->groundBody.key              = 0x30022;
    work->groundBody.radius           = 0x190;
    work->groundBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->groundBody);
    worldCollisionInitContacts(work->groundContacts, ARRAY_SIZE(work->groundContacts), 0);
    work->groundBody.flags |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);

    work->body.coord            = &task->extra.tmd->coords[3];
    work->body.context.contacts = work->bodyContacts;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = 0;
    work->body.pos.vz           = 0;
    work->body.key              = 0x30022;
    work->body.radius           = 0x190;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(work->bodyContacts, ARRAY_SIZE(work->bodyContacts), 0);
    work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

    spawned    = enemySpawnFromTable(D_actor_521100_8015F6E4, 1, 0, enemy);
    model      = spawned->task->extra.tmd;
    raw        = enemy->placeKey;
    sessionKey = &gGameSession->location.loc;
    key.stage  = sessionKey->stage;
    key.area   = sessionKey->area;
    key.room   = sessionKey->room;
    idx        = raw >> 12;
    key.view   = sessionKey->view;
    areaSyncLocationVariant(&key);
    /* offset + base, as in the sibling spawn bodies: the ROM adds the scaled
       index onto the table. */
    place                    = gpAreaPlaceAt(areaGetVariant(&key)->placements, idx);
    model->texturePageOffset = place->texturePageOffset;
    model->clutRowOffset     = place->clutRowOffset;
    if (model->buffer != NULL) {
        tmdBuildBufferHalf(model);
        tmdBuildBufferHalf(model);
    }
    work->weaponTask = spawned->task;

    work->weaponAttack.coord            = spawned->task->extra.tmd->coords;
    work->weaponAttack.pos.vx           = -0x226;
    work->weaponAttack.context.contacts = work->attackContacts;
    work->weaponAttack.pos.vy           = 0x64;
    work->weaponAttack.pos.vz           = 0;
    work->weaponAttack.key              = 0;
    work->weaponAttack.radius           = 0x1C2;
    work->weaponAttack.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->weaponAttack);
    worldCollisionInitContacts(work->attackContacts, ARRAY_SIZE(work->attackContacts), 0);
    work->weaponAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    work->forearmAttack.coord            = &task->extra.tmd->coords[7];
    work->forearmAttack.context.contacts = work->attackContacts;
    work->forearmAttack.pos.vx           = 0;
    work->forearmAttack.pos.vy           = 0;
    work->forearmAttack.pos.vz           = 0;
    work->forearmAttack.key              = 0;
    work->forearmAttack.radius           = 0x1C2;
    work->forearmAttack.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->forearmAttack);

    work->grabPathCapsule.ends[0].vz = 0x5DC;
    work->grabPathCapsule.ends[0].vx = 0;
    work->grabPathCapsule.ends[0].vy = 0;
    work->grabPathCapsule.ends[1].vx = 0;
    work->grabPathCapsule.ends[1].vy = 0;
    work->grabPathCapsule.ends[1].vz = 0;
    work->grabPathCapsule.end0Radius = 1;
    work->grabPathCapsule.end1Radius = 1;
    work->grabPathCapsule.contacts   = work->grabProbeContacts;
    work->forearmAttack.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    work->grabPathProbe.coord           = task->extra.tmd->coords;
    work->grabPathProbe.context.capsule = &work->grabPathCapsule;
    work->grabPathProbe.pos.vy          = -0x1F4;
    work->grabPathProbe.pos.vx          = 0;
    work->grabPathProbe.pos.vz          = 0;
    work->grabPathProbe.key             = 0;
    work->grabPathProbe.radius          = 0;
    work->grabPathProbe.flags           = WORLD_COLLISION_BODY_CAPSULE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->grabPathProbe);
    worldCollisionInitContacts(work->grabProbeContacts, ARRAY_SIZE(work->grabProbeContacts), 0);
    work->grabPathProbe.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;

    work->grabSpotProbe.coord            = task->extra.tmd->coords;
    work->grabSpotProbe.pos.vy           = -0x320;
    work->grabSpotProbe.context.contacts = work->grabProbeContacts;
    work->grabSpotProbe.pos.vx           = 0;
    work->grabSpotProbe.pos.vz           = 0x4E2;
    work->grabSpotProbe.key              = 0;
    work->grabSpotProbe.radius           = 0x1C2;
    work->grabSpotProbe.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->grabSpotProbe);
    work->grabSpotProbe.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;

    task->msgTable = D_actor_521100_8015F6FC;
    task->state    = 1;
}

/// Returns the golem's initial response to a player attack: normal or guard.
///
/// Attachment keys (bit 15) always request a guard; weapon keys select the
/// 48-entry response table with their low six bits, which must be in 0..47.
/// Category and other bits are ignored. The hit handler may turn a guard into
/// a normal hit during a grab or a stagger when struck off-axis or attacking.
static __inline__ s32 _actor521100GetBaseHitResponse(s32 attackKey)
{
    enum {
        ACTOR_521100_ATTACK_KEY_ATTACHMENT = 0x8000,
        ACTOR_521100_ATTACK_KEY_ROW_MASK   = 0x3F,
    };
    if (attackKey & ACTOR_521100_ATTACK_KEY_ATTACHMENT) {
        return ACTOR_521100_HIT_RESPONSE_GUARD;
    }
    return D_actor_521100_8015F684[attackKey & ACTOR_521100_ATTACK_KEY_ROW_MASK];
}

static void func_actor_521100_801322F8(Task* arg0, TmdObject* arg1, s32 arg2)
{
    ActorContactDeltaWideScratch* scratch;
    Actor521100Work*              work;
    Enemy*                        enemy;
    GfxCoord*                     coord;
    u32                           lastId;
    u32                           sound;
    u32                           kind;
    u32                           damage;
    u32                           rng;
    u32                           rng2;
    u32                           r;
    s32                           result;
    s32                           dx;
    s32                           coordX;
    s32                           dz;
    s32                           absDiff;
    s32                           r2;
    s32                           angle;
    s32                           angle2;
    s32                           hitResponse;
    s32                           i;
    s16                           diff;
    s16                           wrap;
    s16                           cooldown;
    s32                           pan;
    s32                           pan1;
    s32                           pan2;
    s32                           depth;
    s16                           wait;

    lastId  = 0;
    work    = arg0->work;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(ActorContactDeltaWideScratch);
    coord   = arg0->extra.tmd->coords;
    enemy   = arg0->spawnArg2.pointer;
    result  = worldCollisionResolvePushback(work->groundContacts, &scratch->delta, ARRAY_SIZE(work->groundContacts), NULL);
    switch (result) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += scratch->delta.fixed.vx.halves.integer;
            coord->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
            coord->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
            break;
        case 2:
            coord->coord.t[0] = work->prevRootPos.vx;
            coord->coord.t[1] = work->prevRootPos.vy;
            coord->coord.t[2] = work->prevRootPos.vz;
            break;
    }
    worldCollisionClearContacts(work->groundContacts);
    if (work->hitCooldown != 0) {
        cooldown          = (u16)work->hitCooldown - 1;
        work->hitCooldown = cooldown;
        if ((cooldown << 0x10) <= 0) {
            work->hitCooldown = 0;
        }
    }
    if (work->flinchCooldown != 0) {
        work->flinchCooldown = (u16)work->flinchCooldown - 1;
    }
    for (i = 0; i < ARRAY_SIZE(work->bodyContacts); i++) {
        kind = (u16)(work->bodyContacts[i].key.value >> 0x10);
        if (kind < 2) {
            continue;
        }
        if (kind != 2) {
            continue;
        }
        if (work->hitCooldown != 0) {
            continue;
        }
        coordX                   = coord->coord.t[0];
        dx                       = gPlayerStatus.coordMtx->t[0] - coordX;
        scratch->delta.vector.vx = dx;
        scratch->delta.vector.vy = 0;
        dz                       = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
        scratch->delta.vector.vz = dz;
        damage                   = damageComputePlayerAttack(work->bodyContacts[i].key.value, SquareRoot0(dx * dx + dz * dz), 0, 0);
        hitResponse              = _actor521100GetBaseHitResponse(work->bodyContacts[i].key.value);
        if (hitResponse == ACTOR_521100_HIT_RESPONSE_GUARD) {
            if (work->state == ACTOR_521100_STATE_GRAB) {
                hitResponse = ACTOR_521100_HIT_RESPONSE_NORMAL;
            } else {
                diff    = work->yaw - (ratan2((s16)scratch->delta.vector.vx, (s16)scratch->delta.vector.vz) & 0xFFF);
                absDiff = abs(diff);
                if (absDiff < 0x800) {
                    wrap = absDiff;
                } else if (diff > 0) {
                    wrap = 0x1000 - diff;
                } else {
                    wrap = diff + 0x1000;
                }
                if ((wrap >= 0x301) || (work->attackLive == 1)) {
                    hitResponse = ACTOR_521100_HIT_RESPONSE_STAGGER;
                }
            }
        }
        switch (hitResponse) {
            case ACTOR_521100_HIT_RESPONSE_NORMAL:
                rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                r               = rng >> 0x10;
                angle           = (r & 0x7F) + 0x40;
                gRandomLcgState = rng;
                if (!(r & 1)) {
                    angle = -angle;
                }
                work->hitTwist.vx = angle;
                r2                = (s16)r >> 8;
                angle2            = (r2 & 0x7F) + 0x40;
                if (!(r2 & 1)) {
                    angle2 = -angle2;
                }
                work->hitTwist.vy    = angle2;
                work->hitTwistActive = 1;
                damage             >>= 1;
                if ((damageGetPlayerAttackReaction(work->bodyContacts[i].key.value) & 0xFFFF) == 5) {
                    damage *= 2;
                    effectSpawn(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[3], 2, NULL);
                }
                if ((work->state == ACTOR_521100_STATE_APPROACH) && (work->flinchCooldown <= 0)) {
                    work->state          = ACTOR_521100_STATE_FLINCH;
                    work->subState       = 0;
                    rng2                 = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->flinchCooldown = ((rng2 >> 0x10) & 0xFF) + 0x96;
                    gRandomLcgState      = rng2;
                    sound                = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401C0006;
                    pan                  = (s8)worldCoordGetOriginAudioPan(coord);
                    depth                = (s8)worldCoordGetOriginAudioDepth(coord);
                    sndEvtRequestScriptStart((s32)sound, pan, depth);
                }
                break;
            case ACTOR_521100_HIT_RESPONSE_GUARD:
                work->state    = ACTOR_521100_STATE_GUARD;
                work->subState = 0;
                if (work->bodyContacts[i].key.value & 0x8000) {
                    damage >>= 2;
                } else {
                    damage >>= 3;
                }
                sound = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401C0003;
                pan1  = (s8)worldCoordGetOriginAudioPan(coord);
                depth = (s8)worldCoordGetOriginAudioDepth(coord);
                sndEvtRequestScriptStart((s32)sound, pan1, depth);
                break;
            case ACTOR_521100_HIT_RESPONSE_STAGGER:
                work->state      = ACTOR_521100_STATE_STAGGER;
                work->subState   = 0;
                work->attackLive = 0;
                if (work->bodyContacts[i].key.value & 0x8000) {
                    damage *= 2;
                } else {
                    damage >>= 1;
                }
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if ((gRandomLcgState >> 16) & 1) {
                    sound = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401C0004;
                } else {
                    sound = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401C0005;
                }
                pan2  = (s8)worldCoordGetOriginAudioPan(coord);
                depth = (s8)worldCoordGetOriginAudioDepth(coord);
                sndEvtRequestScriptStart((s32)sound, pan2, depth);
                break;
        }
        damageAccumulateLifeDrainHp(enemy, work->bodyContacts[i].key.value, (s32)damage, 0);
        worldTargetAddReadoutAmount(&enemy->node, (s32)damage, 0);
        enemy->hp = (u16)enemy->hp - damage;
        if (lastId != work->bodyContacts[i].key.value) {
            lastId = work->bodyContacts[i].key.value;
            effectSpawnHit(damageGetPlayerAttackEffectId(work->bodyContacts[i].key.value), &arg0->extra.tmd->coords[3], NULL, &work->hitEffectArg);
        }
        wait = damageGetPlayerAttackHitCooldown(work->bodyContacts[i].key.value);
        if (wait > 0) {
            work->hitCooldown = wait;
        }
    }
    worldCollisionClearContacts(work->bodyContacts);
    if (work->attackContacts[0].flags & 1) {
        work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        worldCollisionClearContacts(work->attackContacts);
        work->attackLanded = 1;
    }
    work->grabBlocked = 0;
    if (work->grabProbeContacts[0].flags & 1) {
        work->grabBlocked = 1;
        worldCollisionClearContacts(work->grabProbeContacts);
    }
    if (enemy->hp <= 0) {
        if (gPlayerStatus.hp > 0) {
            work->present = 0;
        } else {
            enemy->hp = 1;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorContactDeltaWideScratch);
}

static void func_actor_521100_80132958(Task* arg0)
{
    Actor521100Work* work;
    GfxCoord*        coord;
    VECTOR*          scratchEnd;
    VECTOR*          vec;
    u16*             tbl;
    u16*             tbl1;
    u16*             tbl2;
    u32              rng;
    u32              rng1;
    u32              rng2;
    s16              state;
    s16              delta;
    s16              angle;
    s16              timer;
    s16              wrapped;
    s32              magnitude;

    coord                        = arg0->extra.tmd->coords;
    work                         = arg0->work;
    scratchEnd                   = SCRATCH_STACK_CURSOR(VECTOR);
    vec                          = scratchEnd - 1;
    SCRATCH_STACK_CURSOR(VECTOR) = vec;
    scratchEnd[-1].vx            = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    vec->vy                      = 0;
    vec->vz                      = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    work->playerDistance         = SquareRoot0(scratchEnd[-1].vx * scratchEnd[-1].vx + vec->vz * vec->vz);
    angle                        = ratan2((s16)scratchEnd[-1].vx, (s16)vec->vz) & 0xFFF;
    work->targetYaw              = angle;
    if (gGameSession->location.loc.view == 2) {
        work->state    = ACTOR_521100_STATE_WALK_ROUTE;
        work->subState = 0;
    } else {
        state = work->subState;
        switch (state) {
            case 0:
                work->forwardSpeed = 0;
                work->turnSpeed    = 0;
                timer              = (u16)work->stateCounter - 1;
                work->stateCounter = timer;
                if (timer < 0) {
                    work->subState     = 1;
                    work->animationId  = 0x12;
                    tbl                = D_actor_521100_8015F614;
                    rng                = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    gRandomLcgState    = rng;
                    work->stateCounter = tbl[(rng >> 16) & 0xF];
                } else if (func_actor_521100_80132C70(arg0) == 0) {
                    _actor521100TryStartAttack(arg0);
                }
                break;
            case 1:
                if ((work->grabBlocked == state) && (work->playerDistance < 0x7D0)) {
                    delta     = angle - work->yaw;
                    magnitude = abs(delta);
                    if (magnitude < 0x800) {
                        wrapped = magnitude;
                    } else {
                        if (delta > 0) {
                            wrapped = 0x1000 - delta;
                        } else {
                            wrapped = delta + 0x1000;
                        }
                    }
                    if (wrapped < 0x100) {
                        work->stateCounter = 0;
                    }
                }
                timer              = (u16)work->stateCounter - 1;
                work->stateCounter = timer;
                if (timer <= 0) {
                    work->turnSpeed    = 0x78;
                    work->forwardSpeed = 0;
                    tbl1               = D_actor_521100_8015F5F4;
                    rng1               = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    gRandomLcgState    = rng1;
                    timer              = tbl1[(rng1 >> 16) & 0xF];
                    work->stateCounter = timer;
                    if (timer == 0) {
                        _actor521100TryStartAttack(arg0);
                        if (work->state == ACTOR_521100_STATE_APPROACH) {
                            tbl2               = D_actor_521100_8015F614;
                            rng2               = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                            gRandomLcgState    = rng2;
                            work->stateCounter = tbl2[(rng2 >> 16) & 0xF];
                        }
                    } else {
                        work->subState    = 0;
                        work->animationId = 1;
                    }
                } else {
                    work->forwardSpeed = 0x14;
                    work->turnSpeed    = 0x78;
                    func_actor_521100_80132C70(arg0);
                }
                break;
        }
    }
    work->attackLive = 0;
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

/// Asks the player to await 0x19 button presses once the actor has
/// swung its heading to within 0x20 of the slot-3 task's own and is lined up
/// to latch on. The heading error is the 12-bit difference between the work
/// block's `targetYaw` and `yaw`, wrapped into [-0x800, 0x800]. While it is
/// under 0x400 with the player within 0x4E2 (`playerDistance`), neither grab
/// probe touching the room (`grabBlocked`) and `gPlayerStatus.hp` (the player's
/// current HP) positive, `turnSpeed` is raised to 0x50; the request goes out
/// once the error is also under 0x20 and the player's own `GameActor::mode` is
/// not scripted. On acceptance the body enters `ACTOR_521100_STATE_GRAB`
/// (animation 0xA into `animationId`, both speeds zeroed, the 0xA/0xFF/0x80
/// pad lerp) and returns 1. The payload is a `GameActorButtonPressHold`
/// reserved on the scratch stack.
static s32 func_actor_521100_80132C70(Task* arg0)
{
    Actor521100Work*          work;
    Task*                     player;
    GameActorButtonPressHold* msg;
    s16                       diff;
    s32                       adiff;
    s16                       wrap;
    s32                       ret;

    work   = arg0->work;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    msg    = SCRATCH_STACK_RESERVE_BLOCK(GameActorButtonPressHold);

    diff  = work->targetYaw - work->yaw;
    adiff = diff >= 0 ? diff : -diff;
    ret   = 0;
    if (adiff < 0x800) {
        wrap = adiff;
    } else if (diff > 0) {
        wrap = 0x1000 - diff;
    } else {
        wrap = diff + 0x1000;
    }
    if ((wrap < 0x400) && (work->playerDistance < 0x4E2) && (work->grabBlocked == 0) && (gPlayerStatus.hp > 0) && (work->turnSpeed = 0x50, (wrap < 0x20)) && (((GameActor*)player->work)->mode != GAME_ACTOR_MODE_SCRIPTED)) {
        msg->pressCount = 0x19;
        if (TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, msg, 0) == 0) {
            ret                 = 1;
            work->playerEscaped = 0;
            work->state         = ACTOR_521100_STATE_GRAB;
            work->subState      = 0;
            work->attackStep    = 0;
            work->animationId   = 0xA;
            work->forwardSpeed  = 0;
            work->turnSpeed     = 0;
            padScriptSpawnVariableMotorRamp(0xA, 0xFF, 0x80);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(GameActorButtonPressHold);
    return ret;
}
static void func_actor_521100_80132DE8(Task* arg0)
{
    Actor521100Work* work;
    GfxCoord*        coord;
    VECTOR*          head;
    VECTOR*          vec;
    s16*             flatNear;
    s16*             flatFar;
    u16              prev;
    u32              rngFN;
    u32              rngFF;
    s32              packed;
    s32              next;

    coord = arg0->extra.tmd->coords;
    work  = arg0->work;
    head  = SCRATCH_STACK_CURSOR(VECTOR);
    vec   = head - 1;

    SCRATCH_STACK_CURSOR(VECTOR) = vec;
    head[-1].vx                  = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    vec->vy                      = 0;
    vec->vz                      = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];

    work->playerDistance = SquareRoot0(head[-1].vx * head[-1].vx + vec->vz * vec->vz);
    work->targetYaw      = ratan2((s16)head[-1].vx, (s16)vec->vz) & 0xFFF;

    switch (work->subState) {
        case 0:
            if (((u32)((u8)Gp_StateC08.mode - ATTACHMENT_MODE_ARMED) >= 2U) && (gGameSession->location.loc.view != 2)) {
                if (work->playerDistance < 0x8FC) {
                    if (work->prevAttackChoice == work->attackChoice) {
                        next = D_actor_521100_8015F59C[work->attackChoice][((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 1];
                    } else {
                        flatNear        = D_actor_521100_8015F57C;
                        rngFN           = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        next            = flatNear[(rngFN >> 16) & 0xF];
                        gRandomLcgState = rngFN;
                    }
                } else {
                    if (work->prevAttackChoice == work->attackChoice) {
                        next = D_actor_521100_8015F5C8[work->attackChoice][((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 1];
                    } else {
                        flatFar         = D_actor_521100_8015F5A8;
                        rngFF           = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        next            = flatFar[(rngFF >> 16) & 0xF];
                        gRandomLcgState = rngFF;
                    }
                }
            } else {
                next = ACTOR_521100_ATTACK_STANCE;
            }

            prev                   = (u16)work->attackChoice;
            work->attackChoice     = next;
            work->prevAttackChoice = prev;

            switch (next) {
                case ACTOR_521100_ATTACK_SLASH:
                    work->subState          = 1;
                    work->animationId       = 5;
                    packed                  = damagePackAttackKey(D_actor_521100_8015F550, ACTOR_521100_ATTACK_SLASH);
                    work->weaponAttack.key  = packed;
                    work->forearmAttack.key = packed;
                    break;
                case ACTOR_521100_ATTACK_LONG_SLASH:
                    work->subState          = 2;
                    work->animationId       = 6;
                    packed                  = damagePackAttackKey(D_actor_521100_8015F550, ACTOR_521100_ATTACK_LONG_SLASH);
                    work->weaponAttack.key  = packed;
                    work->forearmAttack.key = packed;
                    break;
                case ACTOR_521100_ATTACK_STANCE:
                    work->subState          = 3;
                    work->attackStep        = 0;
                    work->animationId       = 3;
                    packed                  = damagePackAttackKey(D_actor_521100_8015F550, ACTOR_521100_ATTACK_STANCE);
                    work->weaponAttack.key  = packed;
                    work->forearmAttack.key = packed;
                    break;
            }
            break;
        case 1:
            func_actor_521100_80133104(arg0);
            break;
        case 2:
            func_actor_521100_8013334C(arg0);
            break;
        case 3:
            func_actor_521100_801335B4(arg0);
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

/// The effect's coordinate is read from the display object again rather than
/// taken from `coord`: the scratch reservation between the two reads is a
/// store, so the second one is a load until after register allocation.
static void func_actor_521100_80133104(Task* arg0)
{
    Actor521100Work* work;
    GfxCoord*        coord;
    SVECTOR*         head;
    SVECTOR*         vec;
    u16*             tbl;
    s16              clip;
    s16              speed;
    s16              frame;
    s32              snd;

    work                          = arg0->work;
    coord                         = arg0->extra.tmd->coords;
    head                          = SCRATCH_STACK_CURSOR(SVECTOR);
    vec                           = head - 1;
    SCRATCH_STACK_CURSOR(SVECTOR) = vec;
    clip                          = D_actor_521100_8015F894[work->animationId];

    frame = work->animationFrame;
    if (frame == clip + 0x1A) {
        effectSpawn(EFFECT_NO9_GOLEM_SWING_TRAIL, arg0->extra.tmd->coords + 8, 0xC, NULL);
        padScriptSpawnVariableMotorRamp(0xA, 0x40, 0xFF);
    } else if (frame == clip + 0x1E) {
        vec->vx = -0x320;
        vec->vy = 0x64;
        vec->vz = 0;
        effectSpawn(EFFECT_CRITICAL_HIT, work->weaponTask->extra.tmd->coords, 0, vec);
    }

    frame = work->animationFrame;
    if (frame == clip + 0x1C) {
        work->attackLive           = 1;
        work->weaponAttack.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->forearmAttack.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        snd                        = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401C0008;
        sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
    } else if (frame == clip + 0x28) {
        work->attackLanded         = 0;
        work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }

    speed = 0;
    frame = work->animationFrame;
    if (frame >= clip + 0x1C && frame <= clip + 0x1E) {
        speed = 0x64;
    }
    work->forwardSpeed = speed;
    if (work->animationFrame >= clip + 0x7A) {
        work->animationId  = 1;
        work->state        = ACTOR_521100_STATE_APPROACH;
        work->subState     = 0;
        tbl                = D_actor_521100_8015F5F4;
        gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->stateCounter = tbl[(gRandomLcgState >> 16) & 0xF];
        work->attackLive   = 0;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

/// Runs one frame of the burn-out sequence timed off the clip the slots are
/// playing: `D_actor_521100_8015F894[animationId]` is the clip's own length, read
/// signed and again unsigned because the cue frames below need it both ways,
/// and `animationFrame` is the frame counter the blend in
/// `_actor521100TickAnimation` ticks. The counter is re-read at each cue
/// rather than carried, so the effects spawned in between cannot leave a stale
/// copy behind.
///
/// The cues, all offsets from that length: under +0x28 the turn limit
/// `turnSpeed` is held at 0x50; at +0x23 effect 0x60188 drops onto the attach
/// coordinate eight slots along and the 0xA/0x40/0xFF pad lerp starts; at
/// +0x27 the 8-byte scratch `SVECTOR` is thrown to (-0x320, 0x64, 0) and handed
/// to effect 0x6009C on the coordinate `weaponTask`'s own display object
/// carries; and +0x23 again, this time against the unsigned length, sets
/// `attackLive` and enables pair tests on `weaponAttack` and `forearmAttack`
/// together, then cues `sndEvtRequestScriptStart` with the actor's pan and depth
/// narrowed to bytes. +0x2D disables both spheres again and clears
/// `attackLanded`.
///
/// The two ends are the motion: `forwardSpeed` is held at 0x88 of forward speed
/// while the counter is between +0x20 and +0x2A of the length, and is zero
/// everywhere else, and past +0x90 the actor returns to the approach - clip 1,
/// a fresh wait out of `D_actor_521100_8015F5F4` (the top four bits of an LCG
/// draw) into `stateCounter`, and `state`, `subState` and `attackLive` all
/// cleared.
static void func_actor_521100_8013334C(Task* arg0)
{
    Actor521100Work* work;
    GfxCoord*        coord;
    SVECTOR*         head;
    SVECTOR*         vec;
    u16*             tbl;
    u32              rng;
    s16              clip;
    u16              clipId;
    s16              turn;
    s16              speed;
    s16              frame;
    s16              frame2;
    s32              frame3;
    s32              snd;
    s32              pan;

    work                          = arg0->work;
    head                          = SCRATCH_STACK_CURSOR(SVECTOR);
    vec                           = head - 1;
    SCRATCH_STACK_CURSOR(SVECTOR) = vec;
    clip                          = D_actor_521100_8015F894[work->animationId];
    clipId                        = D_actor_521100_8015F894[work->animationId];
    coord                         = arg0->extra.tmd->coords;

    turn = 0;
    if (work->animationFrame < clip + 0x28) {
        turn = 0x50;
    }
    work->turnSpeed = turn;

    frame = work->animationFrame;
    if (frame == clip + 0x23) {
        effectSpawn(EFFECT_NO9_GOLEM_SWING_TRAIL, arg0->extra.tmd->coords + 8, 0xC, NULL);
        padScriptSpawnVariableMotorRamp(0xA, 0x40, 0xFF);
    } else if (frame == clip + 0x27) {
        vec->vx = -0x320;
        vec->vy = 0x64;
        vec->vz = 0;
        effectSpawn(EFFECT_CRITICAL_HIT, work->weaponTask->extra.tmd->coords, 0, vec);
    }

    frame2 = work->animationFrame;
    if (frame2 == (s16)clipId + 0x23) {
        work->attackLive           = 1;
        work->weaponAttack.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->forearmAttack.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        snd                        = (((u32)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401C0008;
        pan                        = (s8)worldCoordGetOriginAudioPan(coord);
        sndEvtRequestScriptStart(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
        speed = 0;
    } else {
        speed = 0;
        if (frame2 == (s16)clipId + 0x2D) {
            work->attackLanded         = 0;
            work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        }
    }

    frame3 = work->animationFrame;
    if ((s16)clipId + 0x20 < frame3) {
        if ((s16)clipId + 0x2A >= frame3) {
            speed = 0x88;
        }
    }
    work->forwardSpeed = speed;
    if (work->animationFrame >= (s16)clipId + 0x90) {
        work->animationId  = 1;
        work->state        = ACTOR_521100_STATE_APPROACH;
        work->subState     = 0;
        tbl                = D_actor_521100_8015F5F4;
        rng                = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState    = rng;
        work->stateCounter = tbl[(rng >> 16) & 0xF];
        work->attackLive   = 0;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}
/// Step-0 body of the burn-out sequence: the transition into it and the two
/// respawn draws. `attackStep` is a four-phase latch. Phase 0 waits out the clip
/// long enough for the actor to commit (`D_actor_521100_8015F894[animationId]`
/// plus 0x38) and then latches clip 4 and hands phase 1 the frames to wait out
/// of `D_actor_521100_8015F5D4`, cueing the 0x401C0007 sound with the actor's
/// own pan and depth. Phase 1 counts `stateCounter` down (stepped as an unsigned
/// halfword, tested as a signed one) and, when it runs out, either keys the two
/// attack spheres with the type-2 pair and asks for clip 8, or - with
/// `playerDistance` at 0xDAC or more - drops the actor back to the approach
/// with a wait out of `D_actor_521100_8015F5F4`.
///
/// Phase 2 walks the turn limit `turnSpeed` 0x3C up while the clip is young,
/// fires the 0x401C0009 cue, the effect 0x60188 on the eighth coordinate and
/// the 0xA/0x40/0xFF pad lerp together on the clip's 0x20th frame, holds the
/// forward speed at 0x64 across the 0x22..0x26 window, and at 0x27 latches
/// phase 3 and disables both attack spheres. Phase 3 waits 0x5E frames and then
/// picks the finish off `coord->coord.t[0]`: under -0xFA0 the actor goes on to
/// `ACTOR_521100_STATE_WALK_ROUTE` (leg 2, or leg 1 in the location's view 2,
/// where it takes the route's leg 0 from the other side of that line as well);
/// otherwise it returns to the approach with the clip-1 draw.
static void func_actor_521100_801335B4(Task* arg0)
{
    Actor521100Work* work;
    GfxCoord*        coord;
    u32              rng;
    u16              timer;
    s16              turn;
    s32              snd;
    s32              pair;

    SCRATCH_STACK_RESERVE_BYTES(0x18);
    work  = arg0->work;
    coord = arg0->extra.tmd->coords;

    switch (work->attackStep) {
        case 0:
            if (work->animationFrame >= D_actor_521100_8015F894[work->animationId] + 0x38) {
                u16* tbl           = D_actor_521100_8015F5D4;
                work->animationId  = 4;
                work->attackStep   = 1;
                gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->stateCounter = tbl[(gRandomLcgState >> 16) & 0xF];
                snd                = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401C0007;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
            }
            break;
        case 1:
            timer              = work->stateCounter - 1;
            work->stateCounter = timer;
            if ((s16)timer > 0) {
                break;
            }
            if (work->playerDistance >= 0xDAC) {
                u16* tbl           = D_actor_521100_8015F5F4;
                work->state        = ACTOR_521100_STATE_APPROACH;
                work->subState     = 0;
                work->attackStep   = 0;
                work->animationId  = 1;
                gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->stateCounter = tbl[(gRandomLcgState >> 16) & 0xF];
            } else {
                work->attackStep        = 2;
                work->animationId       = 8;
                pair                    = damagePackAttackKey(D_actor_521100_8015F550, ACTOR_521100_ATTACK_STANCE);
                work->weaponAttack.key  = pair;
                work->forearmAttack.key = pair;
            }
            break;
        case 2:
            turn = 0;
            if (work->animationFrame < 0x20) {
                turn = 0x3C;
            }
            work->turnSpeed = turn;
            if (work->animationFrame == 0x20) {
                work->attackLive          = 1;
                work->weaponAttack.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                snd                       = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401C0009;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
                effectSpawn(EFFECT_NO9_GOLEM_SWING_TRAIL, arg0->extra.tmd->coords + 8, 8, NULL);
                padScriptSpawnVariableMotorRamp(0xA, 0x40, 0xFF);
            }
            if ((u32)((u16)work->animationFrame - 0x22) < 5) {
                work->forwardSpeed = 0x64;
            } else {
                work->forwardSpeed = 0;
            }
            if (work->animationFrame >= 0x27) {
                work->attackStep           = 3;
                work->animationId          = 7;
                work->attackLanded         = 0;
                work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            break;
        case 3:
            if (work->animationFrame < 0x5E) {
                break;
            }
            if (gGameSession->location.loc.view == 2) {
                work->state = ACTOR_521100_STATE_WALK_ROUTE;
                if (coord->coord.t[0] < -0xFA0) {
                    work->subState = 1;
                } else {
                    work->subState = 0;
                }
            } else if (coord->coord.t[0] < -0xFA0) {
                work->state    = ACTOR_521100_STATE_WALK_ROUTE;
                work->subState = 2;
            } else {
                u16* tbl           = D_actor_521100_8015F5F4;
                work->state        = ACTOR_521100_STATE_APPROACH;
                work->subState     = 0;
                work->animationId  = 1;
                gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->stateCounter = tbl[(gRandomLcgState >> 16) & 0xF];
            }
            work->attackLive = 0;
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x18);
}

static void func_actor_521100_801339B0(Task* arg0)
{
    Actor521100Work*         work;
    GfxCoord*                coord;
    GfxCoord*                pcoord;
    Task*                    player;
    _Actor521100GrabScratch* scratch;
    s32                      flag;
    s32                      i;
    s32                      snd;
    s32                      absDiff;
    s32                      angle;
    s16                      state;
    s16                      turn;
    u16                      timer;
    u32                      rng;
    u16*                     tbl;

    work   = arg0->work;
    coord  = arg0->extra.tmd->coords;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    SCRATCH_STACK_RESERVE_BLOCK(_Actor521100GrabScratch);
    scratch = SCRATCH_STACK_CURSOR(_Actor521100GrabScratch);

    switch (work->subState) {
        case 0:
            if (work->animationFrame == 0xA) {
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 6;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
            } else if (work->animationFrame == 0xC) {
                flag                                     = (gPlayerStatus.coordMtx->m[0][2] * coord->coord.m[0][2] + gPlayerStatus.coordMtx->m[1][2] * coord->coord.m[1][2] + gPlayerStatus.coordMtx->m[2][2] * coord->coord.m[2][2]);
                work->grabFromFront                      = (u32)flag >> 31;
                scratch->playerAnim.source.sets          = D_actor_521100_8015F7CC;
                scratch->playerAnim.animationId          = work->grabFromFront ? 2 : 6;
                scratch->playerAnim.blend                = ANIMATION_BLEND_RESET;
                scratch->playerAnim.blendFrames          = 0;
                scratch->playerAnim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &scratch->playerAnim, 0);
            } else if (work->animationFrame >= 0x2B) {
                work->subState                           = 1;
                work->animationId                        = 0xB;
                work->stateCounter                       = 0;
                work->stateElapsed                       = 0;
                scratch->playerAnim.source.sets          = D_actor_521100_8015F7CC;
                scratch->playerAnim.animationId          = work->grabFromFront ? 3 : 7;
                scratch->playerAnim.blend                = ANIMATION_BLEND_RESET;
                scratch->playerAnim.blendFrames          = 0;
                scratch->playerAnim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &scratch->playerAnim, 0);
            }
            if ((work->animationFrame >= 4) && (work->animationFrame <= 0xC)) {
                scratch->localOffset.vz = 0x4E2;
                scratch->localOffset.vx = 0;
                scratch->localOffset.vy = 0;
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&scratch->localOffset);
                gte_rtv0();
                gte_stlvnl(&scratch->worldVector);
                scratch->worldVector.vx = coord->coord.t[0] + scratch->worldVector.vx;
                scratch->worldVector.vy = coord->coord.t[1] + scratch->worldVector.vy;
                scratch->worldVector.vz = coord->coord.t[2] + scratch->worldVector.vz;
                pcoord                  = player->extra.tmd->coords;
                scratch->toHoldSpot.vx  = scratch->worldVector.vx - pcoord->coord.t[0];
                scratch->toHoldSpot.vy  = 0;
                scratch->toHoldSpot.vz  = scratch->worldVector.vz - pcoord->coord.t[2];
                if ((SquareRoot0((scratch->toHoldSpot.vx * scratch->toHoldSpot.vx) + (scratch->toHoldSpot.vz * scratch->toHoldSpot.vz)) < 0x32) || (work->animationFrame == 0xC)) {
                    scratch->playerPlacement.pos.vx = scratch->worldVector.vx;
                    scratch->playerPlacement.pos.vy = scratch->worldVector.vy;
                    scratch->playerPlacement.pos.vz = scratch->worldVector.vz;
                } else {
                    VectorNormal(&scratch->toHoldSpot, &scratch->worldVector);
                    scratch->playerPlacement.pos.vx = pcoord->coord.t[0] + ((scratch->worldVector.vx * 0x32) >> 12);
                    scratch->playerPlacement.pos.vy = pcoord->coord.t[1] + ((scratch->worldVector.vy * 0x32) >> 12);
                    scratch->playerPlacement.pos.vz = pcoord->coord.t[2] + ((scratch->worldVector.vz * 0x32) >> 12);
                }
                angle                           = ratan2(pcoord->coord.m[0][2], pcoord->coord.m[2][2]) & 0xFFF;
                flag                            = work->yaw - angle;
                scratch->playerPlacement.rot.vx = 0;
                scratch->playerPlacement.rot.vz = 0;
                if (work->animationFrame == 0xC) {
                    scratch->playerPlacement.rot.vy = work->yaw;
                } else {
                    absDiff = flag >= 0 ? flag : -flag;
                    if ((u32)(absDiff - 0x400) >= 0x801U) {
                        if (absDiff < 0x65) {
                            scratch->playerPlacement.rot.vy = work->yaw;
                        } else if (flag > 0) {
                            scratch->playerPlacement.rot.vy = angle + 0x64;
                        } else {
                            scratch->playerPlacement.rot.vy = angle - 0x64;
                        }
                    } else {
                        if (absDiff < 0x65) {
                            scratch->playerPlacement.rot.vy = (work->yaw + ACTOR_TRANSFORM_ANGLE_HALF_TURN) & ACTOR_TRANSFORM_ANGLE_MASK;
                        } else if (flag > 0) {
                            scratch->playerPlacement.rot.vy = angle - 0x64;
                        } else {
                            scratch->playerPlacement.rot.vy = angle + 0x64;
                        }
                    }
                }
                TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_PLACE, &scratch->playerPlacement, 0);
            }
            break;
        case 1:
            flag = 0;
            if (work->stateCounter == 2) {
                padScriptSpawnVariableMotorRamp(5, 0xC0, 0x80);
            }
            timer              = work->stateCounter - 1;
            work->stateCounter = timer;
            if ((s16)timer <= 0) {
                if (gPlayerStatus.hp <= D_actor_521100_8015F570[gSceneCombatState.difficulty]) {
                    work->animationId                        = 0x14;
                    work->subState                           = 5;
                    scratch->playerAnim.source.sets          = D_actor_521100_8015F7CC;
                    scratch->playerAnim.animationId          = work->grabFromFront ? 0xC : 0xD;
                    scratch->playerAnim.blend                = ANIMATION_BLEND_RESET;
                    scratch->playerAnim.blendFrames          = 0;
                    scratch->playerAnim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                    TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &scratch->playerAnim, 0);
                    flag = 1;
                } else {
                    work->stateCounter = 0x20;
                    taskMessageDispatch(player, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackAttackKey(D_actor_521100_8015F550, 3), 0);
                }
            }
            if (flag != 1) {
                flag = 0;
                if ((work->playerEscaped == 1) && (gPlayerStatus.hp < 0x3D) && (((D_actor_521100_8015F560.hpMax / 3) & 0xFFFF) >= ((Enemy*)arg0->spawnArg2.pointer)->hp)) {
                    flag = work->grabFromFront == 1;
                }
                if (flag != 0) {
                    work->subState                           = 3;
                    work->playerEscaped                      = 0;
                    work->animationFrame                     = 0;
                    scratch->playerAnim.source.sets          = D_actor_521100_8015F7CC;
                    scratch->playerAnim.animationId          = 5;
                    scratch->playerAnim.blend                = ANIMATION_BLEND_RESET;
                    scratch->playerAnim.blendFrames          = 0;
                    scratch->playerAnim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                    TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &scratch->playerAnim, 0);
                } else {
                    if (work->playerEscaped == 0) {
                        timer              = work->stateElapsed + 1;
                        work->stateElapsed = timer;
                        if ((s16)timer < 0x97) {
                            break;
                        }
                    }
                    work->subState                           = 2;
                    work->animationId                        = 0x13;
                    work->playerEscaped                      = 0;
                    scratch->playerAnim.source.sets          = D_actor_521100_8015F7CC;
                    scratch->playerAnim.animationId          = work->grabFromFront ? 9 : 0xA;
                    scratch->playerAnim.blend                = ANIMATION_BLEND_RESET;
                    scratch->playerAnim.blendFrames          = 0;
                    scratch->playerAnim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                    TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &scratch->playerAnim, 0);
                }
            }
            break;
        case 2:
            if (work->animationFrame == 0x22) {
                padScriptSpawnVariableMotorRamp(0xF, 0xFF, 0x80);
            }
            if (work->animationFrame == 0x25) {
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401C000F;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animationFrame < 0x45) {
                // Carry the player by the step of the span this frame of the throw falls in.
                for (i = 0; i < ARRAY_SIZE(D_actor_521100_8015F80C[0]); i++) {
                    if (work->animationFrame < D_actor_521100_8015F80C[work->grabFromFront][i].endFrame) {
                        scratch->localOffset.vx = D_actor_521100_8015F80C[work->grabFromFront][i].sidewaysStep;
                        break;
                    }
                }
                scratch->localOffset.vy = 0;
                scratch->localOffset.vz = 0;
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&scratch->localOffset);
                gte_rtv0();
                gte_stlvnl(&scratch->worldVector);
                pcoord                          = player->extra.tmd->coords;
                scratch->playerPlacement.pos.vx = pcoord->coord.t[0] + scratch->worldVector.vx;
                scratch->playerPlacement.pos.vy = pcoord->coord.t[1] + scratch->worldVector.vy;
                scratch->playerPlacement.pos.vz = pcoord->coord.t[2] + scratch->worldVector.vz;
                scratch->playerPlacement.rot.vx = 0;
                scratch->playerPlacement.rot.vy = work->yaw;
                scratch->playerPlacement.rot.vz = 0;
                TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_PLACE, &scratch->playerPlacement, 0);
            }
            if (work->animationFrame == 0x23) {
                effectSpawn(EFFECT_DUST_PUFF, player->extra.tmd->coords + 3, 0x80003400, NULL);
                effectSpawn(EFFECT_DUST_PUFF, player->extra.tmd->coords + 3, 0x80003400, NULL);
                effectSpawn(EFFECT_DUST_PUFF, player->extra.tmd->coords + 3, 0x80003400, NULL);
            }
            if (work->animationFrame == 0x45) {
                scratch->playerAnim.source.sets          = D_actor_521100_8015F7CC;
                scratch->playerAnim.animationId          = 0xB;
                scratch->playerAnim.blend                = ANIMATION_BLEND_RESET;
                scratch->playerAnim.blendFrames          = 0;
                scratch->playerAnim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &scratch->playerAnim, 0);
            }
            turn = 0;
            if (work->animationFrame < 0x5F) {
                turn = -0x10;
            }
            work->forwardSpeed = turn;
            if (work->animationFrame == 0x6F) {
                coord                           = player->extra.tmd->coords;
                scratch->playerPlacement.pos.vx = coord->coord.t[0];
                scratch->playerPlacement.pos.vy = coord->coord.t[1];
                scratch->playerPlacement.pos.vz = coord->coord.t[2];
                scratch->playerPlacement.rot.vx = 0;
                scratch->playerPlacement.rot.vy = (work->yaw + ACTOR_TRANSFORM_ANGLE_HALF_TURN) & ACTOR_TRANSFORM_ANGLE_MASK;
                scratch->playerPlacement.rot.vz = 0;
                TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_PLACE, &scratch->playerPlacement, 0);
            }
            if ((work->animationFrame >= 0x6F) && (taskMessageDispatch(player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0)) {
                taskMessageDispatch(player, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            }
            if (work->animationFrame >= 0xA4) {
                tbl                = D_actor_521100_8015F5F4;
                work->animationId  = 1;
                work->state        = ACTOR_521100_STATE_APPROACH;
                work->subState     = 0;
                rng                = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState    = rng;
                work->stateCounter = tbl[(rng >> 16) & 0xF];
            }
            break;
        case 3:
            if (work->animationFrame == 0x20) {
                effectSpawn(EFFECT_DILAPIDATED_HOUSE_FIRE_BLAST, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords + 0xC, 0, NULL);
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401C000E;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
                work->subState                           = 4;
                work->animationId                        = 0xD;
                scratch->playerAnim.source.sets          = D_actor_521100_8015F7CC;
                scratch->playerAnim.animationId          = 4;
                scratch->playerAnim.blend                = ANIMATION_BLEND_RESET;
                scratch->playerAnim.blendFrames          = 0;
                scratch->playerAnim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &scratch->playerAnim, 0);
            }
            break;
        case 4:
            if (work->animationFrame == 0xF) {
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401C000C;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animationFrame == 0x64) {
                ((Enemy*)arg0->spawnArg2.pointer)->hp = 0;
            }
            break;
        case 5:
            if (work->animationFrame == 0x1A) {
                ((GameActor*)player->work)->state = 0xA;
                work->subState                    = 6;
                work->stateCounter                = 0;
                gGameSession->deathRestartDelay   = 0x5A;
                gGameSession->deathSoundCountdown = GAME_SESSION_DEATH_SOUND_HOLD;
                scratch->localOffset.vx           = 0;
                scratch->localOffset.vy           = -0x96;
                scratch->localOffset.vz           = 0xC8;
                effectSpawnHit(EFFECT_HIT_KIND_WEAPON_PUFF, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords + 4, &scratch->localOffset,
                               &D_actor_521100_8015F804);
                padScriptSpawnVariableMotorRamp(0xA, 0xFF, 8);
                taskMessageDispatch(player, 0x400, 0, 0);
                gPlayerStatus.hp = 0;
            }
            break;
        case 6:
            state = work->stateCounter;
            if (state != 1) {
                if (state < 2) {
                    if (state == 0) {
                        cdCmdEnqueueDisplayResource(9, 0x1E, CD_COMMAND_DISPLAY_LOAD_DEFAULT);
                        work->stateCounter = 1;
                    }
                }
            } else if ((cdCmdIsIdle() & 0xFFFF) == state) {
                coord = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
                sndEvtRequestScriptStart(SOUND_PLAYER_DEATH, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
                work->stateCounter = 2;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor521100GrabScratch);
}

/// Runs the guard reaction, then resumes approach or the interrupted route.
///
/// Requires the body's live work block. Entry stops movement and disables both
/// attack spheres; the two guard clips run for 5 and 38 animation ticks.
/// Completion restores idle and draws the next approach wait in frames.
static void _actor521100GuardState(Task* task)
{
    enum {
        ACTOR_521100_GUARD_WAIT_END     = 2,
        ACTOR_521100_GUARD_START_FRAMES = 5,
        ACTOR_521100_GUARD_END_FRAMES   = 38,
    };
    Actor521100Work* work;

    work = task->work;
    switch (work->subState) {
        case ACTOR_521100_REACTION_BEGIN:
            work->animationId          = ACTOR_521100_ANIM_GUARD_START;
            work->subState             = ACTOR_521100_REACTION_WAIT;
            work->forwardSpeed         = 0;
            work->turnSpeed            = 0;
            work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            return;
        case ACTOR_521100_REACTION_WAIT:
            if (work->animationFrame >= ACTOR_521100_GUARD_START_FRAMES) {
                work->animationId = ACTOR_521100_ANIM_GUARD_END;
                work->subState    = ACTOR_521100_GUARD_WAIT_END;
            }
            return;
        case ACTOR_521100_GUARD_WAIT_END:
            if (work->animationFrame >= ACTOR_521100_GUARD_END_FRAMES) {
                ACTOR_521100_RESUME_AFTER_HIT(work, D_actor_521100_8015F634);
            }
            return;
    }
}
/// Walks the Dryfield fight's three route legs and turns to attack a nearby player.
///
/// Requires the body's live root coordinate and work block. Targets use the
/// root's parent frame and integer game units; yaw uses 4096 units per turn.
/// View 2 retains a route leg for resumption after an attack or hit reaction.
/// Reserves and releases one ActorFaceScratch block for the target delta.
static void _actor521100WalkRouteState(Task* task)
{
    enum {
        ACTOR_521100_ROUTE_FIRST           = 0,
        ACTOR_521100_ROUTE_SECOND          = 1,
        ACTOR_521100_ROUTE_RETURN          = 2,
        ACTOR_521100_ROUTE_VIEW            = 2,
        ACTOR_521100_ROUTE_WALK_SPEED      = 20,
        ACTOR_521100_ROUTE_TURN_SPEED      = 120,
        ACTOR_521100_ROUTE_ATTACK_DISTANCE = 2000,
        ACTOR_521100_ROUTE_WAYPOINT_RADIUS = 60,
        ACTOR_521100_ROUTE_PLAYER_Z_LIMIT  = -1500,
        ACTOR_521100_ROUTE_RETURN_X        = -4000,
    };
    Actor521100Work*  work;
    GfxCoord*         rootCoord;
    ActorFaceScratch* scratch;
    s16               routeLeg;

    scratch   = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    routeLeg  = work->subState;
    // Attack near the player; otherwise follow the current room waypoint.
    switch (routeLeg) {
        case ACTOR_521100_ROUTE_FIRST:
            work->animationId  = ACTOR_521100_ANIM_WALK;
            work->forwardSpeed = ACTOR_521100_ROUTE_WALK_SPEED;
            work->turnSpeed    = ACTOR_521100_ROUTE_TURN_SPEED;
            scratch->delta.vx  = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
            scratch->delta.vy  = 0;
            scratch->delta.vz  = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
            if ((SquareRoot0((scratch->delta.vx * scratch->delta.vx) + (scratch->delta.vz * scratch->delta.vz)) < ACTOR_521100_ROUTE_ATTACK_DISTANCE) && (gPlayerStatus.coordMtx->t[2] < ACTOR_521100_ROUTE_PLAYER_Z_LIMIT)) {
                work->targetYaw    = ratan2((s16)scratch->delta.vx, (s16)scratch->delta.vz) & ACTOR_TRANSFORM_ANGLE_MASK;
                work->forwardSpeed = 0;
                work->turnSpeed    = ACTOR_521100_ROUTE_TURN_SPEED;
                work->state        = ACTOR_521100_STATE_ATTACK;
                work->subState     = 0;
            } else {
                scratch->delta.vx = D_actor_521100_8015F654[ACTOR_521100_ROUTE_FIRST].vx - rootCoord->coord.t[0];
                scratch->delta.vy = 0;
                scratch->delta.vz = D_actor_521100_8015F654[ACTOR_521100_ROUTE_FIRST].vz - rootCoord->coord.t[2];
                work->targetYaw   = ratan2((s16)scratch->delta.vx, (s16)scratch->delta.vz) & ACTOR_TRANSFORM_ANGLE_MASK;
                if (SquareRoot0((scratch->delta.vx * scratch->delta.vx) + (scratch->delta.vz * scratch->delta.vz)) < ACTOR_521100_ROUTE_WAYPOINT_RADIUS) {
                    work->subState = ACTOR_521100_ROUTE_SECOND;
                } else {
                    if (gGameSession->location.loc.view != ACTOR_521100_ROUTE_VIEW) {
                        work->state       = ACTOR_521100_STATE_APPROACH;
                        work->subState    = 0;
                        work->resumeRoute = 0;
                    } else {
                        work->resumeRoute = 1;
                    }
                    work->resumeRouteLeg = 0;
                }
            }
            break;
        case ACTOR_521100_ROUTE_SECOND:
            work->animationId  = ACTOR_521100_ANIM_WALK;
            work->forwardSpeed = ACTOR_521100_ROUTE_WALK_SPEED;
            work->turnSpeed    = ACTOR_521100_ROUTE_TURN_SPEED;
            scratch->delta.vx  = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
            scratch->delta.vy  = 0;
            scratch->delta.vz  = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
            if (SquareRoot0((scratch->delta.vx * scratch->delta.vx) + (scratch->delta.vz * scratch->delta.vz)) < ACTOR_521100_ROUTE_ATTACK_DISTANCE) {
                work->targetYaw    = ratan2((s16)scratch->delta.vx, (s16)scratch->delta.vz) & ACTOR_TRANSFORM_ANGLE_MASK;
                work->forwardSpeed = 0;
                work->turnSpeed    = ACTOR_521100_ROUTE_TURN_SPEED;
                work->state        = ACTOR_521100_STATE_ATTACK;
                work->subState     = 0;
            } else {
                scratch->delta.vx = D_actor_521100_8015F654[ACTOR_521100_ROUTE_SECOND].vx - rootCoord->coord.t[0];
                scratch->delta.vy = 0;
                scratch->delta.vz = D_actor_521100_8015F654[ACTOR_521100_ROUTE_SECOND].vz - rootCoord->coord.t[2];
                work->targetYaw   = ratan2((s16)scratch->delta.vx, (s16)scratch->delta.vz) & ACTOR_TRANSFORM_ANGLE_MASK;
                if (SquareRoot0((scratch->delta.vx * scratch->delta.vx) + (scratch->delta.vz * scratch->delta.vz)) < ACTOR_521100_ROUTE_WAYPOINT_RADIUS) {
                    scratch->delta.vx  = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
                    scratch->delta.vy  = 0;
                    scratch->delta.vz  = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
                    work->targetYaw    = ratan2((s16)scratch->delta.vx, (s16)scratch->delta.vz) & ACTOR_TRANSFORM_ANGLE_MASK;
                    work->forwardSpeed = 0;
                    work->turnSpeed    = ACTOR_521100_ROUTE_TURN_SPEED;
                    work->state        = ACTOR_521100_STATE_ATTACK;
                    work->subState     = 0;
                } else if (gGameSession->location.loc.view != ACTOR_521100_ROUTE_VIEW) {
                    work->subState       = ACTOR_521100_ROUTE_RETURN;
                    work->resumeRoute    = 0;
                    work->resumeRouteLeg = 0;
                } else {
                    work->resumeRoute    = 1;
                    work->resumeRouteLeg = ACTOR_521100_ROUTE_SECOND;
                }
            }
            break;
        case ACTOR_521100_ROUTE_RETURN:
            work->animationId  = ACTOR_521100_ANIM_WALK;
            work->forwardSpeed = ACTOR_521100_ROUTE_WALK_SPEED;
            work->turnSpeed    = ACTOR_521100_ROUTE_TURN_SPEED;
            scratch->delta.vx  = D_actor_521100_8015F654[ACTOR_521100_ROUTE_RETURN].vx - rootCoord->coord.t[0];
            scratch->delta.vy  = 0;
            scratch->delta.vz  = D_actor_521100_8015F654[ACTOR_521100_ROUTE_RETURN].vz - rootCoord->coord.t[2];
            work->targetYaw    = ratan2((s16)scratch->delta.vx, (s16)scratch->delta.vz) & ACTOR_TRANSFORM_ANGLE_MASK;
            if (rootCoord->coord.t[0] < ACTOR_521100_ROUTE_RETURN_X) {
                if (SquareRoot0((scratch->delta.vx * scratch->delta.vx) + (scratch->delta.vz * scratch->delta.vz)) < ACTOR_521100_ROUTE_WAYPOINT_RADIUS) {
                    work->state    = ACTOR_521100_STATE_APPROACH;
                    work->subState = 0;
                } else if (gGameSession->location.loc.view == routeLeg) {
                    work->subState = 0;
                }
            } else {
                if (gGameSession->location.loc.view != routeLeg) {
                    work->state = ACTOR_521100_STATE_APPROACH;
                }
                work->subState = 0;
            }
            work->resumeRoute    = 0;
            work->resumeRouteLeg = 0;
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}
/// Turns the body toward targetYaw by at most turnSpeed angle units this tick.
///
/// Requires a live root and work block with targetYaw in 0..4095 and a
/// nonnegative turnSpeed. Chooses the shorter arc and replaces the root's
/// rotation with pure yaw at unit scale, retaining its translation. Reserves
/// and releases one ActorFaceScratch block for the rotation vector.
static void _actor521100TurnTowardTargetYaw(Task* task)
{
    Actor521100Work*  work;
    GfxCoord*         rootCoord;
    ActorFaceScratch* scratch;
    s32               currentYaw;
    u16               targetYaw;
    s16               signedDifference;
    s32               differenceMagnitude;
    s32               turnStep;
    s32               currentStepYaw;
    s32               nextYaw;
    s32               wrappedTurnStep;

    scratch             = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    rootCoord           = task->extra.tmd->coords;
    work                = task->work;
    currentYaw          = ratan2(rootCoord->coord.m[0][2], rootCoord->coord.m[2][2]) & ACTOR_TRANSFORM_ANGLE_MASK;
    targetYaw           = work->targetYaw;
    signedDifference    = targetYaw - currentYaw;
    differenceMagnitude = signedDifference >= 0 ? signedDifference : -signedDifference;

    // Choose the shorter arc, including when the headings straddle zero.
    work->yaw = currentYaw;
    if (differenceMagnitude < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        turnStep = work->turnSpeed;
        if (turnStep >= differenceMagnitude) {
            work->yaw = targetYaw;
        } else {
            nextYaw = work->yaw;
            if (signedDifference <= 0) {
                nextYaw -= turnStep;
            } else {
                nextYaw += turnStep;
            }
            work->yaw = nextYaw;
        }
    } else {
        turnStep = work->turnSpeed;
        if (signedDifference > 0 ? turnStep >= ACTOR_TRANSFORM_ANGLE_TURN - signedDifference : turnStep >= ACTOR_TRANSFORM_ANGLE_TURN + signedDifference) {
            work->yaw = work->targetYaw;
        } else {
            wrappedTurnStep = work->turnSpeed;
            currentStepYaw  = work->yaw;
            if (signedDifference > 0) {
                work->yaw = currentStepYaw - wrappedTurnStep;
            } else {
                work->yaw = currentStepYaw + wrappedTurnStep;
            }
        }
    }
    scratch->rot.vx = 0;
    scratch->rot.vy = work->yaw;
    scratch->rot.vz = 0;
    RotMatrix(&scratch->rot, &rootCoord->coord);
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}
/// Plays the body's two footstep cues when their slot-1 record bits fall.
///
/// Requires a live Enemy in spawnArg2 and an initialized body animation rig.
/// Cue 2 plays character-bank entry 1 and cue 1 entry 2, with the placement
/// index identifying the sound instance. Pan and depth are narrowed to signed
/// bytes. A missing current record leaves the previous cue bits unchanged.
static void _actor521100PlayFootsteps(Task* task)
{
    enum {
        ACTOR_521100_FOOTSTEP_CUE_2 = SOUND_CHARACTER(0x1C, 1),
        ACTOR_521100_FOOTSTEP_CUE_1 = SOUND_CHARACTER(0x1C, 2),
    };
    Enemy*                 enemy;
    s32                    soundScript;
    s32                    cue2Pan;
    s32                    cue1Pan;
    Actor521100Work*       work;
    GfxCoord*              rootCoord;
    const AnimationRecord* record;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    record    = animationGetCurrentRecord(&work->rig.anim, &work->rig.slots[1]);
    if (record != NULL) {
        if (!(record->flags & ANIMATION_RECORD_CUE_2) && (work->lastCueFlags & ANIMATION_RECORD_CUE_2)) {
            enemy       = task->spawnArg2.pointer;
            soundScript = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_521100_FOOTSTEP_CUE_2;
            cue2Pan     = (s8)worldCoordGetOriginAudioPan(rootCoord);
            sndEvtRequestScriptStart(soundScript, cue2Pan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
        }
        if (!(record->flags & ANIMATION_RECORD_CUE_1) && (work->lastCueFlags & ANIMATION_RECORD_CUE_1)) {
            enemy       = task->spawnArg2.pointer;
            soundScript = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_521100_FOOTSTEP_CUE_1;
            cue1Pan     = (s8)worldCoordGetOriginAudioPan(rootCoord);
            sndEvtRequestScriptStart(soundScript, cue1Pan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
        }
        work->lastCueFlags = (u16)(record->flags & ANIMATION_RECORD_CUE_MASK);
    }
}

#include "../../shared/no9_golem_aim_head.inc.c"
/// Composes a hit rotation into the chest's local rotation; changes GTE state.
///
/// Both matrices must remain live, with Q12 coefficients; the chest is writable.
/// Translation is untouched. The chest supplies the GTE matrix and the hit
/// rotation supplies its three columns; each result overwrites the chest.
static __inline__ void _actor521100ComposeHitRotation(MATRIX* chestRotation, const MATRIX* hitRotation)
{
    gte_SetRotMatrix(chestRotation);
    gte_ldclmv(hitRotation);
    gte_rtir();
    gte_stclmv(chestRotation);
    gte_ldclmv(&hitRotation->m[0][1]);
    gte_rtir();
    gte_stclmv(&chestRotation->m[0][1]);
    gte_ldclmv(&hitRotation->m[0][2]);
    gte_rtir();
    gte_stclmv(&chestRotation->m[0][2]);
}

/// Applies the residual hit rotation to the chest and decays it toward zero.
///
/// Requires the body's live model and work block. X and Y angles use 4096 units
/// per turn and decay by 32 units per tick; hitTwistActive clears when both
/// reach zero. Reserves and releases one MATRIX block and changes GTE state.
static void _actor521100ApplyHitTwist(Task* task)
{
    enum { ACTOR_521100_HIT_TWIST_DECAY_STEP = 32 };
    Actor521100Work* work;
    GfxCoord*        bodyCoords;
    MATRIX*          twistMatrix;
    s32              angleX;
    s32              angleY;
    s32              absAngleX;
    s32              nextAngleX;
    s32              absAngleY;
    s32              nextAngleY;
    s32              decayActive;

    SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    twistMatrix = SCRATCH_STACK_CURSOR(MATRIX);
    decayActive = 0;
    work        = task->work;
    bodyCoords  = task->extra.tmd->coords;
    RotMatrix(&work->hitTwist, twistMatrix);
    // Compose the hit rotation into the chest before decaying its two angles.
    _actor521100ComposeHitRotation(&bodyCoords[3].coord, twistMatrix);
    angleX = work->hitTwist.vx;
    if (angleX != 0) {
        absAngleX = __builtin_abs(angleX);
        if (absAngleX < ACTOR_521100_HIT_TWIST_DECAY_STEP + 1) {
            work->hitTwist.vx = 0;
        } else {
            nextAngleX = angleX - ACTOR_521100_HIT_TWIST_DECAY_STEP;
            if (angleX <= 0) {
                nextAngleX = angleX + ACTOR_521100_HIT_TWIST_DECAY_STEP;
            }
            work->hitTwist.vx = nextAngleX;
            decayActive       = 1;
        }
    }
    angleY = work->hitTwist.vy;
    if (angleY != 0) {
        absAngleY = __builtin_abs(angleY);
        if (absAngleY < ACTOR_521100_HIT_TWIST_DECAY_STEP + 1) {
            work->hitTwist.vy = 0;
        } else {
            nextAngleY = angleY - ACTOR_521100_HIT_TWIST_DECAY_STEP;
            if (angleY <= 0) {
                nextAngleY = angleY + ACTOR_521100_HIT_TWIST_DECAY_STEP;
            }
            work->hitTwist.vy = nextAngleY;
            decayActive       = 1;
        }
    }
    if (decayActive == 0) {
        work->hitTwistActive = 0;
    }
    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
}
/// Spawns and fades the event's body fire over 420 ticks.
///
/// Call once per event frame with eventBurnStage in 1..3. Bursts occur every
/// 7, 14 or 28 ticks at the chest; stage 1 also picks one of eight body parts.
/// At ticks 240 and 330 the stage advances; tick 420 disables further bursts.
/// The signed-halfword timer comparisons and increments retain their wraps.
static void _actor521100TickEventFire(Task* task)
{
    enum {
        ACTOR_521100_FIRE_NONE       = 0,
        ACTOR_521100_FIRE_FULL       = 1,
        ACTOR_521100_FIRE_FADING     = 2,
        ACTOR_521100_FIRE_LAST       = 3,
        ACTOR_521100_FIRE_FADE_FRAME = 240,
        ACTOR_521100_FIRE_LAST_FRAME = 330,
        ACTOR_521100_FIRE_END_FRAME  = 420,
    };
    Actor521100Work* work;
    u16              burstFrames;
    u16              elapsedFrames;
    const s16*       fireParts;
    s16              partIndex;

    work               = task->work;
    burstFrames        = work->stateCounter + 1;
    work->stateCounter = burstFrames;
    if ((s16)burstFrames >= D_actor_521100_8015F8CC[work->eventBurnStage]) {
        work->stateCounter = 0U;
        effectSpawnHit(EFFECT_HIT_KIND_BLAST, &task->extra.tmd->coords[3], NULL, &work->hitEffectArg);
        if (work->eventBurnStage == ACTOR_521100_FIRE_FULL) {
            fireParts       = D_actor_521100_8015F8BC;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            partIndex       = fireParts[(gRandomLcgState >> 16) & (ARRAY_SIZE(D_actor_521100_8015F8BC) - 1)];
            effectSpawnHit(EFFECT_HIT_KIND_BLAST, &task->extra.tmd->coords[partIndex], NULL, &work->hitEffectArg);
        }
    }
    // The overall fire clock advances independently of the burst interval.
    elapsedFrames      = work->stateElapsed + 1;
    work->stateElapsed = elapsedFrames;
    if ((s16)elapsedFrames == ACTOR_521100_FIRE_FADE_FRAME) {
        work->eventBurnStage = ACTOR_521100_FIRE_FADING;
    }
    if (work->stateElapsed == ACTOR_521100_FIRE_LAST_FRAME) {
        work->eventBurnStage = ACTOR_521100_FIRE_LAST;
    }
    if (work->stateElapsed >= ACTOR_521100_FIRE_END_FRAME) {
        work->eventBurnStage = ACTOR_521100_FIRE_NONE;
    }
}

/// State handlers of the actor's second part, which `_actor521100GunbladeTask`
/// dispatches through: the setup `_actor521100AttachGunblade`, the per-frame
/// tick `_actor521100UpdateGunbladeDrawMode` and `enemyDestroy`.
static const EnemyTaskFuncTable3 D_actor_521100_80131E40 = { {
    _actor521100AttachGunblade,
    _actor521100UpdateGunbladeDrawMode,
    enemyDestroy,
} };

/// The actor's task body: runs the handler for `Task::state` out of a two-entry
/// table built on the stack - the spawn state `func_actor_521100_80131E8C`,
/// then the per-frame state `func_actor_521100_801353CC` - passing the task's
/// `Enemy` along with the task.
void func_actor_521100_80135378(Task* task)
{
    EnemyTaskFunc fns[2] = {
        func_actor_521100_80131E8C,
        func_actor_521100_801353CC,
    };

    fns[task->state](task->spawnArg2.pointer, task);
}

static void func_actor_521100_801353CC(Enemy* arg0, Task* arg1)
{
    Actor521100Work* work;

    work = arg1->work;
    if (gGameSession->eventState != 0) {
        work->inEvent = 1;
        func_actor_521100_80135414(arg0, arg1);
        return;
    }
    work->inEvent = 0;
    func_actor_521100_80135478(arg0, arg1);
}

static void func_actor_521100_80135414(Enemy* arg0, Task* arg1)
{
    Actor521100Work* temp_s0;

    temp_s0                      = arg1->work;
    arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    _actor521100TickAnimation(arg1);
    _actor521100UpdateLighting(arg1);
    no9GolemDrawShadow(arg1);
    if (temp_s0->eventBurnStage != 0) {
        _actor521100TickEventFire(arg1);
    }
}

static void func_actor_521100_80135478(Enemy* arg0, Task* arg1)
{
    GfxCoord*        temp_s2;
    TmdObject*       temp_a1;
    Actor521100Work* temp_s1;
    s32              state;
    s32              one;

    temp_a1 = arg1->extra.tmd;
    state   = gSceneCombatState.actorControl;
    temp_s1 = arg1->work;
    temp_s2 = temp_a1->coords;
    one     = 1;
    switch (state) {
        case 1:
            _actor521100UpdateLighting(arg1);
            no9GolemDrawShadow(arg1);
            return;
        case 0:
            temp_a1->flags                        = 0;
            temp_s1->weaponTask->extra.tmd->flags = 0;
            arg0->node.state.parts.flags          = WORLD_TARGET_HIDE_HP;
            break;
        case 2:
            temp_a1->flags                        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            temp_s1->weaponTask->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags          = one;
            return;
    }
    if (temp_s1->activated == 0) {
        arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        return;
    }
    func_actor_521100_801322F8(arg1, temp_a1, one);
    func_actor_521100_801355C8(arg1);
    _actor521100TurnTowardTargetYaw(arg1);
    _actor521100StepRootPosition(arg1);
    _actor521100PlayFootsteps(arg1);
    _actor521100TickAnimation(arg1);
    no9GolemAimHead(arg1);
    if (temp_s1->hitTwistActive != 0) {
        _actor521100ApplyHitTwist(arg1);
    }
    temp_s2->composeStamp                   = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(temp_s2);
    _actor521100UpdateLighting(arg1);
    no9GolemDrawShadow(arg1);
}

static void func_actor_521100_801355C8(Task* arg0)
{
    s16 temp_v1;

    temp_v1 = ((Actor521100Work*)arg0->work)->state;
    switch (temp_v1) {
        case ACTOR_521100_STATE_APPROACH:
            func_actor_521100_80132958(arg0);
            return;
        case ACTOR_521100_STATE_ATTACK:
            func_actor_521100_80132DE8(arg0);
            return;
        case ACTOR_521100_STATE_GRAB:
            func_actor_521100_801339B0(arg0);
            return;
        case ACTOR_521100_STATE_STAGGER:
            _actor521100StaggerState(arg0);
            return;
        case ACTOR_521100_STATE_GUARD:
            _actor521100GuardState(arg0);
            return;
        case ACTOR_521100_STATE_FLINCH:
            _actor521100FlinchState(arg0);
            return;
        case ACTOR_521100_STATE_WALK_ROUTE:
            _actor521100WalkRouteState(arg0);
        default:
            return;
    }
}

/// Starts an attack when the player is within 3500 units and 45 degrees of facing.
///
/// Requires the approach state's current playerDistance, yaw and targetYaw.
/// Both limits are strict. On entry to attack, clears subState and both speeds;
/// otherwise leaves the work block unchanged. Headings use 4096 units per turn.
static void _actor521100TryStartAttack(Task* task)
{
    enum {
        ACTOR_521100_ATTACK_FACING_LIMIT = ACTOR_TRANSFORM_ANGLE_TURN / 8,
        ACTOR_521100_ATTACK_DISTANCE     = 3500,
    };
    Actor521100Work* work;
    s16              signedDifference;
    s16              facingError;
    s16              wrappedDifference;
    s32              differenceMagnitude;

    work                = task->work;
    signedDifference    = work->targetYaw - work->yaw;
    differenceMagnitude = abs(signedDifference);
    if (differenceMagnitude < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        facingError = differenceMagnitude;
    } else {
        if (signedDifference > 0) {
            wrappedDifference = ACTOR_TRANSFORM_ANGLE_TURN - signedDifference;
        } else {
            wrappedDifference = signedDifference + ACTOR_TRANSFORM_ANGLE_TURN;
        }
        facingError = wrappedDifference;
    }
    if ((facingError < ACTOR_521100_ATTACK_FACING_LIMIT) && (work->playerDistance < ACTOR_521100_ATTACK_DISTANCE)) {
        work->state        = ACTOR_521100_STATE_ATTACK;
        work->subState     = 0;
        work->forwardSpeed = 0;
        work->turnSpeed    = 0;
    }
}

/// Runs the stagger reaction and resumes approach or the interrupted route.
///
/// Entry stops movement and disables both attack spheres. After 55 animation
/// ticks, restores idle and draws the next approach wait in frames.
static void _actor521100StaggerState(Task* task)
{
    enum { ACTOR_521100_STAGGER_FRAMES = 0x37 };
    Actor521100Work* work;

    work = task->work;
    switch (work->subState) {
        case ACTOR_521100_REACTION_BEGIN:
            work->animationId          = ACTOR_521100_ANIM_STAGGER;
            work->subState             = ACTOR_521100_REACTION_WAIT;
            work->forwardSpeed         = 0;
            work->turnSpeed            = 0;
            work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            return;
        case ACTOR_521100_REACTION_WAIT:
            if (work->animationFrame >= ACTOR_521100_STAGGER_FRAMES) {
                ACTOR_521100_RESUME_AFTER_HIT(work, D_actor_521100_8015F634);
            }
            return;
    }
}
/// Runs the plain-hit flinch and resumes approach or the interrupted route.
///
/// Entry stops movement and disables both attack spheres. After 72 animation
/// ticks, restores idle and draws the next approach wait in frames.
static void _actor521100FlinchState(Task* task)
{
    enum { ACTOR_521100_FLINCH_FRAMES = 0x48 };
    Actor521100Work* work;

    work = task->work;
    switch (work->subState) {
        case ACTOR_521100_REACTION_BEGIN:
            work->animationId          = ACTOR_521100_ANIM_FLINCH;
            work->subState             = ACTOR_521100_REACTION_WAIT;
            work->forwardSpeed         = 0;
            work->turnSpeed            = 0;
            work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            return;
        case ACTOR_521100_REACTION_WAIT:
            if (work->animationFrame >= ACTOR_521100_FLINCH_FRAMES) {
                ACTOR_521100_RESUME_AFTER_HIT(work, D_actor_521100_8015F5F4);
            }
            return;
    }
}
#undef ACTOR_521100_RESUME_AFTER_HIT

/// Saves the root's pre-move position and applies this tick's translation.
///
/// Requires the body's live root and work block. X and Z move forwardSpeed
/// game units along the root's Q12 facing axis; Y advances by 128 units.
/// The saved translation narrows to signed halfwords for collision rollback;
/// the snapshot's fourth component is untouched. The caller dirties the root.
static void _actor521100StepRootPosition(Task* task)
{
    enum {
        ACTOR_521100_MOVEMENT_FRACTION_BITS = 12,
        ACTOR_521100_ROOT_VERTICAL_STEP     = 128,
    };
    GfxCoord*        rootCoord;
    Actor521100Work* work;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;

    work->prevRootPos.vx   = rootCoord->coord.t[0];
    work->prevRootPos.vy   = rootCoord->coord.t[1];
    work->prevRootPos.vz   = rootCoord->coord.t[2];
    rootCoord->coord.t[0] += (rootCoord->coord.m[0][2] * work->forwardSpeed) >> ACTOR_521100_MOVEMENT_FRACTION_BITS;
    rootCoord->coord.t[1] += ACTOR_521100_ROOT_VERTICAL_STEP;
    rootCoord->coord.t[2] += (rootCoord->coord.m[2][2] * work->forwardSpeed) >> ACTOR_521100_MOVEMENT_FRACTION_BITS;
}

/// Starts a changed body clip with its blend, or ticks all driven slots.
///
/// Requires a valid nonnegative animationId and the initialized nineteen-slot
/// body rig. Drives slots 1..18, leaving the root slot alone. A new clip resets
/// animationFrame; an unchanged one increments it with halfword wrap before
/// ticking. Native clip ids below 21 use the blend table; other clips use zero.
static void _actor521100TickAnimation(Task* task)
{
    enum { ACTOR_521100_NATIVE_BLEND_LIMIT = 21 };
    Actor521100Work* work;
    s32              slotIndex;
    s32              blendFrames;

    work        = task->work;
    blendFrames = 0;
    if (work->animationId != work->seededAnimationId) {
        work->seededAnimationId = work->animationId;
        work->animationFrame    = 0;
        if (work->animationId < ACTOR_521100_NATIVE_BLEND_LIMIT) {
            blendFrames = D_actor_521100_8015F894[work->animationId];
        }
        slotIndex = 1;
        do {
            animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->animationId, 0, blendFrames);
            slotIndex++;
        } while (slotIndex < ARRAY_SIZE(work->rig.slots));
        return;
    }
    slotIndex             = 1;
    work->animationFrame += slotIndex;
    do {
        animationTickSlot(&work->rig.anim, slotIndex);
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(work->rig.slots));
}

/// Updates body lighting and colour from model coordinate 1's cached translation.
///
/// Requires a live body Enemy/model and a valid coordinate-1 cache. The lighting
/// query borrows only the three initialized position words during the
/// call; the resulting matrices are also used by the child gunblade model.
static void _actor521100UpdateLighting(Task* task)
{
    GfxCoord* sampleCoord;
    VECTOR3   samplePosition;

    sampleCoord       = &task->extra.tmd->coords[1];
    samplePosition.vx = sampleCoord->workm.t[0];
    samplePosition.vy = sampleCoord->workm.t[1];
    samplePosition.vz = sampleCoord->workm.t[2];
    worldCoordUpdateActorColor(task->spawnArg2.pointer, &samplePosition, 0, 0);
}

#include "../../shared/no9_golem_draw_shadow.inc.c"

/// Dispatches the child gunblade's attach, draw-update or destroy task state.
///
/// Requires a live child task and Enemy with Task.state in 0..2. The callback
/// table is copied by value; the destroy state releases the enemy and task.
static void _actor521100GunbladeTask(Task* task)
{
    EnemyTaskFuncTable3 handlers;

    handlers = D_actor_521100_80131E40;
    handlers.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Attaches the gunblade model to the body's weapon hand and starts its draw update.
///
/// Requires the live parent body task, its nineteen-part model and work block.
/// Borrows parent coordinate 8 and the body's lighting matrices; the task
/// hierarchy keeps that parent alive for the child's lifetime. unusedEnemy
/// is retained for the EnemyTaskFunc callback signature.
static void _actor521100AttachGunblade(Enemy* unusedEnemy, Task* task)
{
    enum {
        ACTOR_521100_GUNBLADE_HAND_COORD = 8,
        ACTOR_521100_GUNBLADE_UPDATE     = 1,
    };
    Task*            bodyTask;
    TmdObject*       gunbladeModel;
    Actor521100Work* bodyWork;
    GfxCoord*        gunbladeRoot;
    GfxCoord*        bodyCoords;

    bodyTask      = task->parent;
    gunbladeModel = task->extra.tmd;
    bodyCoords    = bodyTask->extra.tmd->coords;
    gunbladeRoot  = gunbladeModel->coords;
    bodyWork      = bodyTask->work;

    gunbladeRoot->parent    = &bodyCoords[ACTOR_521100_GUNBLADE_HAND_COORD];
    gunbladeModel->lightMtx = &bodyWork->light;
    gunbladeModel->flags    = 0;
    gunbladeModel->colorMtx = &bodyWork->color;
    task->state             = ACTOR_521100_GUNBLADE_UPDATE;
}

/// Makes the gunblade follow the body's requested draw flags during events.
///
/// Requires the live parent work block and child model. Bit 0 permits active
/// drawing and bit 1 suppresses automatic buffer allocation. weaponHidden
/// overrides both with active-draw exclusion alone. Outside events the flags
/// are unchanged. unusedEnemy is retained for the callback signature.
static void _actor521100UpdateGunbladeDrawMode(Enemy* unusedEnemy, Task* task)
{
    TmdObject*       gunbladeModel;
    Actor521100Work* bodyWork;
    s16              drawFlags;

    bodyWork      = task->parent->work;
    gunbladeModel = task->extra.tmd;
    if (bodyWork->inEvent != 0) {
        drawFlags            = ((bodyWork->modelDrawFlags & ACTOR_MESSAGE_PAIR_SHOW) == 0) * TMD_OBJECT_SKIP_ACTIVE_DRAW;
        gunbladeModel->flags = drawFlags;
        if (bodyWork->modelDrawFlags & ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER) {
            gunbladeModel->flags = drawFlags | TMD_OBJECT_SKIP_AUTO_BUFFER;
        }
        if (bodyWork->weaponHidden != 0) {
            gunbladeModel->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        }
    }
}

s32 func_actor_521100_80135BEC(Task* arg0, s32 msgId, s32 arg2, s32 arg3)
{
    if (gPlayerStatus.hp > 0) {
        ((Actor521100Work*)arg0->work)->playerEscaped = 1;
    }
    return 0;
}

s32 func_actor_521100_80135C14(Task* arg0, s32 arg1, AnimationPlayRequest* args, s32 arg3)
{
    Actor521100Work* work;
    s32              i;
    s32              frames;
    s16              clip;
    s16              base;

    frames = 0;
    work   = arg0->work;
    base   = 0x1D;
    if (args->source.index == 0) {
        base = 0x14;
    }
    clip                    = args->animationId + base;
    work->animationId       = clip;
    work->seededAnimationId = clip;
    if (args->blend != ANIMATION_BLEND_RESET) {
        frames = args->blendFrames;
    }
    for (i = 1; i < 0x13; i++) {
        animationSeekSlotWithBlend(&work->rig.anim, i, work->animationId, 0, frames);
    }
    return 0;
}

#include "../../shared/actor_messages_place_rot_matrix.inc.c"
