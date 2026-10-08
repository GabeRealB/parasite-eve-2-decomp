#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
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
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/dryfield_water_tower.h"
#include "../../shared/limb_shadows.h"
#include "../../shared/actor_messages.h"
#include "../../shared/actor_contacts.h"
#define DESERT_CHASER_BUILD DESERT_CHASER_WATER_TOWER
#include "../../shared/desert_chaser.h"

/// Water Tower behavior selectors used by this package's commands and death states.
enum {
    ACTOR421600_STATE_TRACK_ARENA_ROUTE = 2,
    ACTOR421600_STATE_MOVE_TO_ZONE5     = 6,
    ACTOR421600_STATE_BURST_DEATH       = 8,
    ACTOR421600_STATE_KNOCK_DOWN        = 20,
    ACTOR421600_STATE_WAIT_RESPAWN      = 22,
    ACTOR421600_STATE_WATCH_RUN_PLAYER  = 32,
    ACTOR421600_MESSAGE_IGNORE_2015     = 2015
};

/// Water Tower route destinations and clips used by its private state handlers.
enum {
    ACTOR421600_STATE_ENTRANCE_LUNGE   = 3,
    ACTOR421600_STATE_ARENA_TRANSITION = 7,
    ACTOR421600_STATE_LUNGE_RECOVERY   = 29,
    ACTOR421600_STATE_THROW_PLAYER     = 30,
    ACTOR421600_STATE_CLOSE_CATCH      = 37,
    ACTOR421600_STATE_ROUTE_ROAM       = 39,
    ACTOR421600_CLIP_ARENA_WALK        = 0,
    ACTOR421600_CLIP_WATCH_RUN         = 2,
    ACTOR421600_CLIP_LUNGE             = 3,
    ACTOR421600_CLIP_LEAP_BACK         = 6,
    ACTOR421600_CLIP_WALL_KNOCK_DOWN   = 10,
    ACTOR421600_CLIP_ARENA_TRANSITION  = 17
};

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

/// Scratch-stack block of the Water Tower chaser's entrance lunge: the leap that
/// follows the crouch in which it waits for the room's camera to cut to its
/// view.
///
/// The lunge reserves one block on every tick but the one that enters the
/// state and releases it before it returns. It catches a player its front
/// sphere touches: it turns the player to face the chaser or away from it,
/// takes the damage and starts the player's caught animation. It is the catch
/// of `DesertChaserPursueScratch`'s pursuit with the facing test made on
/// headings instead of turns, and with the arena zone noted. Angles are
/// 4096ths of a turn, and a wrapped one lies in [-0x800, 0x800].
typedef struct {
    SVECTOR offset;        // Lent to the steer round obstacles first, which leaves the displacement it moved the chaser by. Then the player's position minus the root's, world units; `pad` is never written
    s16     playerYaw;     // Heading the caught player faces
    s16     yawFromPlayer; // Bearing from the caught player to the chaser, wrapped: the reverse of `offset`'s. Under a quarter turn from `playerYaw` when the player faces the chaser, so is caught from the front; otherwise half a turn is added. Either way it ends as the heading the caught player is placed at
    s16     turn;          // During a catch, the wrapped turn from the facing to the player; every tick then leaves the wrapped turn from the facing to the player's remembered offset. Never read back
    s16     zone;          // Arena zone the model root stands in, from the package's 4x4 zone table; never read back
    s16     playerBearing; // Wrapped turn from the facing to the player that tick; on a tick without a contact, more than 0x600 abandons the lunge. Never read back
    s16     playerKilled;  // Reply to the damage sent to the caught player: 1 when it took the player's last health. Not written by a chaser that has no health left, which then reads what the scratch stack held
} _Actor421600LungeScratch;
STATIC_ASSERT_SIZEOF(_Actor421600LungeScratch, 0x14);

/// Scratch-stack block of the Water Tower chaser's route roam, in which it
/// walks back and forth between two patrol points while it watches for the
/// player.
///
/// It is `DesertChaserRoamScratch` with one more word. The route roam is that
/// roam with its patrol points read from the package's route table instead of
/// placed to either side of the player: the row is the quadrant of the arena
/// the player stands in, and each row holds one pair of points for the chaser
/// of place index 0 and one for any other. The tick that enters the state
/// reserves a block for that lookup and the bearing to the player, and
/// releases it at once. Every later tick reserves one, turns a limited step
/// toward the current point and walks, and takes a new pair when it arrives
/// or has been pushed about for 21 ticks while facing the point. Angles are
/// 4096ths of a turn, and a wrapped one lies in [-0x800, 0x800].
typedef struct {
    SVECTOR toPatrolPoint;  // Current patrol point's position minus the root's on X and Z, with `vy` zero; then lent to the steer round obstacles, which leaves the displacement it moved the chaser by, and the look takes its bearing from that. On the entering tick: the player's position minus the root's
    SVECTOR toPlayer;       // Player's position minus the root's, world units, refilled once the chaser has moved
    MATRIX  rotation;       // Turn about Y to the player's bearing swung 0x2EE to one side, built as a new pair of points is taken; never read, since the points come from the table
    s16     turn;           // Wrapped turn from the facing to the patrol point; taken the other way round when it is past 0x600 and the player lies on the opposite side, limited to 0x20, then added to the heading: the yaw the root's rotation is rebuilt at. The look reuses it for the bearing of `toPatrolPoint` off the facing
    s16     fullTurn;       // `turn` as first measured, before it is redirected and limited; under 0x20 while the chaser faces its patrol point
    s16     yawFromPlayer;  // Bearing from the player to the chaser, wrapped: the reverse of `toPlayer`'s
    s16     playerYaw;      // Heading the player faces; more than 0x600 from `yawFromPlayer` when the player has its back to the chaser
    s16     playerQuadrant; // Row of the route table for the quarter of the arena the player stands in, split at the origin (0 +X +Z, 1 -X +Z, 2 +X -Z, 3 -X -Z; a coordinate of 0 counts as negative)
} _Actor421600RouteRoamScratch;
STATIC_ASSERT_SIZEOF(_Actor421600RouteRoamScratch, 0x3C);

extern DesertChaserVariant D_actor_421600_8013EF48[];
extern EnemyParams         D_actor_421600_8013EF38;
extern u8                  D_actor_421600_80151028[];
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_421600_80151118[8];

/// The overlay's pose table: 8-byte records of three halfwords at 0x0/0x2/0x4
/// plus padding, i.e. `SVECTOR`s. Indexed by the low signed halfword of the
/// caller's id. `actor_403000` keeps a table of the same shape at 0x80158CE0
/// and reaches it with a body the shared-body index groups with this one; a
/// body that reads its own overlay's data cannot be promoted, so each carrier
/// keeps a plain-C copy -- `src/actors/actor_403000/actor_403000.c` for the
/// other.
extern SVECTOR D_actor_421600_80151158[];

/// Hit-position table `_desertChaserHitEffect` picks one of twelve entries
/// from by relative hit yaw: the same 8-byte `SVECTOR` records as the pose
/// table above, at the 0x801510B8 end of the same trailing data run.

/// 4-byte table indexed by `(arg0 > 0) + ((arg1 < 1) << 1)`.
extern s8 D_actor_421600_801511D0[];

/// Two XZ waypoint pairs for each place-key mode, indexed by zone.
extern s32 D_actor_421600_801511D4[][8];

/// The records closing four of the overlay's model streams, which the
/// death-tick frames point `D_80114B34[5].data.model` at before each `effectSpawn`.
static TmdSource _gActor421600DesertChaserBurstLegRight;
static TmdSource _gActor421600DesertChaserBurstLegLeft;
static TmdSource _gActor421600DesertChaserBurstHead;
static TmdSource _gActor421600DesertChaserBurstTorso;

/// Global effect-model callback slot the spawn helpers read; a one-element
/// array so the store is absolute (see actor 401300's header for the same
/// declaration).

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).

/// 4x4 zone table `_actor421600DownedWaitState` samples with the X and Z
/// buckets of the actor's position, cell `x | z * 4`; the sample is compared
/// against 0xB to pick between the 6 and 0x24 states.
extern s8 D_actor_421600_801511C0[16];

/// Progress counter the same handler compares against 4 / 5 / 2 / 0 to pick
/// the arena corner the actor is dropped into. Written by
/// `_actor421600Spawn` at spawn.
extern s16 D_actor_421600_80151268;

/// Signed transition durations, indexed by old animation * 25 + new animation.
extern s8 gDesertChaserClipStartFrames[25][25];

/// The Water Tower chaser's two catch tables and its hit offsets, three
/// objects stored one after another.
///
/// A catch table lists the animation sets the caught player is sent, indexed
/// by `AnimationPlayRequest::animationId`: 1 to 3 are this package's own, and
/// 4 and 5 are filled in during the catch with the sets of the player's
/// weapon. The regular build (`actor_00100`) gives each table nine words. This
/// one stores five, and the catch handler was not changed: its third stage
/// still writes set 5, ONE PAST THE END of the chosen table, and the player's
/// animation task then reads it back from there. For the front table that
/// word is `gDesertChaserRearAnim[0]`, an entry no request names; for the rear
/// table it is the `vx` and `vy` of `gDesertChaserHitOffsets[0]`.
///
/// The out-of-bounds access is the original program's, kept as it is: the
/// tables have their real five-entry extent, and the handler indexes them the
/// way the regular build does. Whether a rear catch reaches set 5 in play,
/// and with it the overwritten hit offset, is not established.
extern AnimationSet* gDesertChaserFrontAnim[5];   // Sets sent to a player caught facing the chaser
extern AnimationSet* gDesertChaserRearAnim[5];    // Sets sent to a player caught from behind
extern SVECTOR       gDesertChaserHitOffsets[12]; // Offsets of the hit effects from their model part, with the part's index in `pad`

static void _actor421600Exit(Task* task);
static void _actor421600HideState(Task* task);
static void _actor421600LungeRecoveryState(Task* task);
static void _actor421600ResumePursuitState(Task* task);
static void _actor421600RiseState(Task* task);

static AnimationSet _gActor421600Animation19E54;
static AnimationSet _gActor421600Animation1A5F0;
static AnimationSet _gActor421600Animation1ADFC;
static AnimationSet _gActor421600Animation1B610;
static AnimationSet _gActor421600Animation1B8C4;
static AnimationSet _gActor421600Animation1BBE4;
static TmdSource    _gActor421600DesertChaserBody;
static s32          _actor421600ApplyCommand(Task* task, s32 messageId, const ActorCommand* request, s32 unusedExtra);
static s32          _actor421600IsPresent(Task* task, s32 messageId, s32 unusedArg, s32 unusedExtra);
static s32          _actor421600OnRoomEvent(Task* task, s32 messageId, s32 unusedArg, s32 unusedExtra);
static void         _actor421600IgnoreMessage2015(Task* task, s32 messageId, s32 unusedArg, s32 unusedExtra);

DamageAttack D_actor_421600_8013EF24[5] = {
    { 30, 0 },
    { 30, 0 },
    { 18, 0 },
    { 18, 0 },
    { 0xFFFF, 0 },
};

EnemyParams D_actor_421600_8013EF38 = { D_actor_421600_8013EF24, 200, 75, 50, 4, 100, 10, 100, 0 };

DesertChaserVariant D_actor_421600_8013EF48[4] = {
    { 60, 36, 10, 150 },
    { 40, 26, 10, 120 },
    { 20, 18, 10, 120 },
    { 60, 60, 10, 150 },
};

static TmdBone _gActor421600DesertChaserBodySkeleton[18] = {
#include "assets/desert_chaser_body_skeleton.inc"
};

static u32 _gActor421600DesertChaserBodyPartVerts[18] = {
#include "assets/desert_chaser_body_partVerts.inc"
};

static SVECTOR _gActor421600DesertChaserBodyVerts[266] = {
#include "assets/desert_chaser_body_verts.inc"
};

static SVECTOR _gActor421600DesertChaserBodyNormals[324] = {
#include "assets/desert_chaser_body_normals.inc"
};

static u32 _gActor421600DesertChaserBodyStream[3435] = {
#include "assets/desert_chaser_body_stream.inc"
};

static TmdSource _gActor421600DesertChaserBody = {
    0,
    17616,
    5944,
    18,
    _gActor421600DesertChaserBodyPartVerts,
    _gActor421600DesertChaserBodyVerts,
    _gActor421600DesertChaserBodyNormals,
    _gActor421600DesertChaserBodySkeleton,
    _gActor421600DesertChaserBodyStream,
};

static TmdBone _gActor421600DesertChaserBurstLegRightSkeleton[1] = {
#include "assets/desert_chaser_burst_leg_right_skeleton.inc"
};

static u32 _gActor421600DesertChaserBurstLegRightPartVerts[1] = {
#include "assets/desert_chaser_burst_leg_right_partVerts.inc"
};

static SVECTOR _gActor421600DesertChaserBurstLegRightVerts[21] = {
#include "assets/desert_chaser_burst_leg_right_verts.inc"
};

static SVECTOR _gActor421600DesertChaserBurstLegRightNormals[1] = {
#include "assets/desert_chaser_burst_leg_right_normals.inc"
};

static u32 _gActor421600DesertChaserBurstLegRightStream[233] = {
#include "assets/desert_chaser_burst_leg_right_stream.inc"
};

static TmdSource _gActor421600DesertChaserBurstLegRight = {
    0,
    1536,
    0,
    1,
    _gActor421600DesertChaserBurstLegRightPartVerts,
    _gActor421600DesertChaserBurstLegRightVerts,
    _gActor421600DesertChaserBurstLegRightNormals,
    _gActor421600DesertChaserBurstLegRightSkeleton,
    _gActor421600DesertChaserBurstLegRightStream,
};

static TmdBone _gActor421600DesertChaserBurstLegLeftSkeleton[1] = {
#include "assets/desert_chaser_burst_leg_left_skeleton.inc"
};

static u32 _gActor421600DesertChaserBurstLegLeftPartVerts[1] = {
#include "assets/desert_chaser_burst_leg_left_partVerts.inc"
};

static SVECTOR _gActor421600DesertChaserBurstLegLeftVerts[23] = {
#include "assets/desert_chaser_burst_leg_left_verts.inc"
};

static SVECTOR _gActor421600DesertChaserBurstLegLeftNormals[1] = {
#include "assets/desert_chaser_burst_leg_left_normals.inc"
};

static u32 _gActor421600DesertChaserBurstLegLeftStream[242] = {
#include "assets/desert_chaser_burst_leg_left_stream.inc"
};

static TmdSource _gActor421600DesertChaserBurstLegLeft = {
    0,
    1612,
    0,
    1,
    _gActor421600DesertChaserBurstLegLeftPartVerts,
    _gActor421600DesertChaserBurstLegLeftVerts,
    _gActor421600DesertChaserBurstLegLeftNormals,
    _gActor421600DesertChaserBurstLegLeftSkeleton,
    _gActor421600DesertChaserBurstLegLeftStream,
};

static TmdBone _gActor421600DesertChaserBurstHeadSkeleton[1] = {
#include "assets/desert_chaser_burst_head_skeleton.inc"
};

static u32 _gActor421600DesertChaserBurstHeadPartVerts[1] = {
#include "assets/desert_chaser_burst_head_partVerts.inc"
};

static SVECTOR _gActor421600DesertChaserBurstHeadVerts[59] = {
#include "assets/desert_chaser_burst_head_verts.inc"
};

static SVECTOR _gActor421600DesertChaserBurstHeadNormals[81] = {
#include "assets/desert_chaser_burst_head_normals.inc"
};

static u32 _gActor421600DesertChaserBurstHeadStream[556] = {
#include "assets/desert_chaser_burst_head_stream.inc"
};

static TmdSource _gActor421600DesertChaserBurstHead = {
    0,
    3812,
    0,
    1,
    _gActor421600DesertChaserBurstHeadPartVerts,
    _gActor421600DesertChaserBurstHeadVerts,
    _gActor421600DesertChaserBurstHeadNormals,
    _gActor421600DesertChaserBurstHeadSkeleton,
    _gActor421600DesertChaserBurstHeadStream,
};

static TmdBone _gActor421600DesertChaserBurstTorsoSkeleton[1] = {
#include "assets/desert_chaser_burst_torso_skeleton.inc"
};

static u32 _gActor421600DesertChaserBurstTorsoPartVerts[1] = {
#include "assets/desert_chaser_burst_torso_partVerts.inc"
};

static SVECTOR _gActor421600DesertChaserBurstTorsoVerts[19] = {
#include "assets/desert_chaser_burst_torso_verts.inc"
};

static SVECTOR _gActor421600DesertChaserBurstTorsoNormals[31] = {
#include "assets/desert_chaser_burst_torso_normals.inc"
};

static u32 _gActor421600DesertChaserBurstTorsoStream[193] = {
#include "assets/desert_chaser_burst_torso_stream.inc"
};

static TmdSource _gActor421600DesertChaserBurstTorso = {
    0,
    1248,
    0,
    1,
    _gActor421600DesertChaserBurstTorsoPartVerts,
    _gActor421600DesertChaserBurstTorsoVerts,
    _gActor421600DesertChaserBurstTorsoNormals,
    _gActor421600DesertChaserBurstTorsoSkeleton,
    _gActor421600DesertChaserBurstTorsoStream,
};

static AnimationPackedPose _gActor421600Animation13D18Bank1[10] = {
#include "assets/actor_421600_animation_13D18_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation13D18Bank4[110] = {
#include "assets/actor_421600_animation_13D18_bank4.inc"
};

static AnimationRecord _gActor421600Animation13D18Records[175] = {
#include "assets/actor_421600_animation_13D18_records.inc"
};

static u16 _gActor421600Animation13D18Indices[18] = {
#include "assets/actor_421600_animation_13D18_indices.inc"
};

static AnimationSet _gActor421600Animation13D18 = {
    _gActor421600Animation13D18Records,
    _gActor421600Animation13D18Indices,
    { NULL, _gActor421600Animation13D18Bank1, NULL, NULL, _gActor421600Animation13D18Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation13FD4Bank1[5] = {
#include "assets/actor_421600_animation_13FD4_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation13FD4Bank4[29] = {
#include "assets/actor_421600_animation_13FD4_bank4.inc"
};

static AnimationRecord _gActor421600Animation13FD4Records[112] = {
#include "assets/actor_421600_animation_13FD4_records.inc"
};

static u16 _gActor421600Animation13FD4Indices[18] = {
#include "assets/actor_421600_animation_13FD4_indices.inc"
};

static AnimationSet _gActor421600Animation13FD4 = {
    _gActor421600Animation13FD4Records,
    _gActor421600Animation13FD4Indices,
    { NULL, _gActor421600Animation13FD4Bank1, NULL, NULL, _gActor421600Animation13FD4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation14608Bank1[13] = {
#include "assets/actor_421600_animation_14608_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation14608Bank4[129] = {
#include "assets/actor_421600_animation_14608_bank4.inc"
};

static AnimationRecord _gActor421600Animation14608Records[210] = {
#include "assets/actor_421600_animation_14608_records.inc"
};

static u16 _gActor421600Animation14608Indices[18] = {
#include "assets/actor_421600_animation_14608_indices.inc"
};

static AnimationSet _gActor421600Animation14608 = {
    _gActor421600Animation14608Records,
    _gActor421600Animation14608Indices,
    { NULL, _gActor421600Animation14608Bank1, NULL, NULL, _gActor421600Animation14608Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation14BE0Bank1[11] = {
#include "assets/actor_421600_animation_14BE0_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation14BE0Bank4[131] = {
#include "assets/actor_421600_animation_14BE0_bank4.inc"
};

static AnimationRecord _gActor421600Animation14BE0Records[191] = {
#include "assets/actor_421600_animation_14BE0_records.inc"
};

static u16 _gActor421600Animation14BE0Indices[18] = {
#include "assets/actor_421600_animation_14BE0_indices.inc"
};

static AnimationSet _gActor421600Animation14BE0 = {
    _gActor421600Animation14BE0Records,
    _gActor421600Animation14BE0Indices,
    { NULL, _gActor421600Animation14BE0Bank1, NULL, NULL, _gActor421600Animation14BE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation15014Bank1[9] = {
#include "assets/actor_421600_animation_15014_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation15014Bank4[83] = {
#include "assets/actor_421600_animation_15014_bank4.inc"
};

static AnimationRecord _gActor421600Animation15014Records[140] = {
#include "assets/actor_421600_animation_15014_records.inc"
};

static u16 _gActor421600Animation15014Indices[18] = {
#include "assets/actor_421600_animation_15014_indices.inc"
};

static AnimationSet _gActor421600Animation15014 = {
    _gActor421600Animation15014Records,
    _gActor421600Animation15014Indices,
    { NULL, _gActor421600Animation15014Bank1, NULL, NULL, _gActor421600Animation15014Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation156ECBank1[19] = {
#include "assets/actor_421600_animation_156EC_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation156ECBank4[138] = {
#include "assets/actor_421600_animation_156EC_bank4.inc"
};

static AnimationRecord _gActor421600Animation156ECRecords[224] = {
#include "assets/actor_421600_animation_156EC_records.inc"
};

static u16 _gActor421600Animation156ECIndices[18] = {
#include "assets/actor_421600_animation_156EC_indices.inc"
};

static AnimationSet _gActor421600Animation156EC = {
    _gActor421600Animation156ECRecords,
    _gActor421600Animation156ECIndices,
    { NULL, _gActor421600Animation156ECBank1, NULL, NULL, _gActor421600Animation156ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation15CECBank1[14] = {
#include "assets/actor_421600_animation_15CEC_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation15CECBank4[99] = {
#include "assets/actor_421600_animation_15CEC_bank4.inc"
};

static AnimationRecord _gActor421600Animation15CECRecords[224] = {
#include "assets/actor_421600_animation_15CEC_records.inc"
};

static u16 _gActor421600Animation15CECIndices[18] = {
#include "assets/actor_421600_animation_15CEC_indices.inc"
};

static AnimationSet _gActor421600Animation15CEC = {
    _gActor421600Animation15CECRecords,
    _gActor421600Animation15CECIndices,
    { NULL, _gActor421600Animation15CECBank1, NULL, NULL, _gActor421600Animation15CECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation15F48Bank1[4] = {
#include "assets/actor_421600_animation_15F48_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation15F48Bank4[34] = {
#include "assets/actor_421600_animation_15F48_bank4.inc"
};

static AnimationRecord _gActor421600Animation15F48Records[86] = {
#include "assets/actor_421600_animation_15F48_records.inc"
};

static u16 _gActor421600Animation15F48Indices[18] = {
#include "assets/actor_421600_animation_15F48_indices.inc"
};

static AnimationSet _gActor421600Animation15F48 = {
    _gActor421600Animation15F48Records,
    _gActor421600Animation15F48Indices,
    { NULL, _gActor421600Animation15F48Bank1, NULL, NULL, _gActor421600Animation15F48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation1656CBank1[12] = {
#include "assets/actor_421600_animation_1656C_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation1656CBank4[147] = {
#include "assets/actor_421600_animation_1656C_bank4.inc"
};

static AnimationRecord _gActor421600Animation1656CRecords[191] = {
#include "assets/actor_421600_animation_1656C_records.inc"
};

static u16 _gActor421600Animation1656CIndices[18] = {
#include "assets/actor_421600_animation_1656C_indices.inc"
};

static AnimationSet _gActor421600Animation1656C = {
    _gActor421600Animation1656CRecords,
    _gActor421600Animation1656CIndices,
    { NULL, _gActor421600Animation1656CBank1, NULL, NULL, _gActor421600Animation1656CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation169BCBank1[9] = {
#include "assets/actor_421600_animation_169BC_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation169BCBank4[73] = {
#include "assets/actor_421600_animation_169BC_bank4.inc"
};

static AnimationRecord _gActor421600Animation169BCRecords[157] = {
#include "assets/actor_421600_animation_169BC_records.inc"
};

static u16 _gActor421600Animation169BCIndices[18] = {
#include "assets/actor_421600_animation_169BC_indices.inc"
};

static AnimationSet _gActor421600Animation169BC = {
    _gActor421600Animation169BCRecords,
    _gActor421600Animation169BCIndices,
    { NULL, _gActor421600Animation169BCBank1, NULL, NULL, _gActor421600Animation169BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation16FA0Bank1[12] = {
#include "assets/actor_421600_animation_16FA0_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation16FA0Bank4[126] = {
#include "assets/actor_421600_animation_16FA0_bank4.inc"
};

static AnimationRecord _gActor421600Animation16FA0Records[196] = {
#include "assets/actor_421600_animation_16FA0_records.inc"
};

static u16 _gActor421600Animation16FA0Indices[18] = {
#include "assets/actor_421600_animation_16FA0_indices.inc"
};

static AnimationSet _gActor421600Animation16FA0 = {
    _gActor421600Animation16FA0Records,
    _gActor421600Animation16FA0Indices,
    { NULL, _gActor421600Animation16FA0Bank1, NULL, NULL, _gActor421600Animation16FA0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation1711CBank1[2] = {
#include "assets/actor_421600_animation_1711C_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation1711CBank4[16] = {
#include "assets/actor_421600_animation_1711C_bank4.inc"
};

static AnimationRecord _gActor421600Animation1711CRecords[54] = {
#include "assets/actor_421600_animation_1711C_records.inc"
};

static u16 _gActor421600Animation1711CIndices[18] = {
#include "assets/actor_421600_animation_1711C_indices.inc"
};

static AnimationSet _gActor421600Animation1711C = {
    _gActor421600Animation1711CRecords,
    _gActor421600Animation1711CIndices,
    { NULL, _gActor421600Animation1711CBank1, NULL, NULL, _gActor421600Animation1711CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation177C0Bank1[13] = {
#include "assets/actor_421600_animation_177C0_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation177C0Bank4[158] = {
#include "assets/actor_421600_animation_177C0_bank4.inc"
};

static AnimationRecord _gActor421600Animation177C0Records[209] = {
#include "assets/actor_421600_animation_177C0_records.inc"
};

static u16 _gActor421600Animation177C0Indices[18] = {
#include "assets/actor_421600_animation_177C0_indices.inc"
};

static AnimationSet _gActor421600Animation177C0 = {
    _gActor421600Animation177C0Records,
    _gActor421600Animation177C0Indices,
    { NULL, _gActor421600Animation177C0Bank1, NULL, NULL, _gActor421600Animation177C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation17A84Bank1[4] = {
#include "assets/actor_421600_animation_17A84_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation17A84Bank4[45] = {
#include "assets/actor_421600_animation_17A84_bank4.inc"
};

static AnimationRecord _gActor421600Animation17A84Records[101] = {
#include "assets/actor_421600_animation_17A84_records.inc"
};

static u16 _gActor421600Animation17A84Indices[18] = {
#include "assets/actor_421600_animation_17A84_indices.inc"
};

static AnimationSet _gActor421600Animation17A84 = {
    _gActor421600Animation17A84Records,
    _gActor421600Animation17A84Indices,
    { NULL, _gActor421600Animation17A84Bank1, NULL, NULL, _gActor421600Animation17A84Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation17F88Bank1[9] = {
#include "assets/actor_421600_animation_17F88_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation17F88Bank4[123] = {
#include "assets/actor_421600_animation_17F88_bank4.inc"
};

static AnimationRecord _gActor421600Animation17F88Records[152] = {
#include "assets/actor_421600_animation_17F88_records.inc"
};

static u16 _gActor421600Animation17F88Indices[18] = {
#include "assets/actor_421600_animation_17F88_indices.inc"
};

static AnimationSet _gActor421600Animation17F88 = {
    _gActor421600Animation17F88Records,
    _gActor421600Animation17F88Indices,
    { NULL, _gActor421600Animation17F88Bank1, NULL, NULL, _gActor421600Animation17F88Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation18508Bank1[13] = {
#include "assets/actor_421600_animation_18508_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation18508Bank4[113] = {
#include "assets/actor_421600_animation_18508_bank4.inc"
};

static AnimationRecord _gActor421600Animation18508Records[181] = {
#include "assets/actor_421600_animation_18508_records.inc"
};

static u16 _gActor421600Animation18508Indices[18] = {
#include "assets/actor_421600_animation_18508_indices.inc"
};

static AnimationSet _gActor421600Animation18508 = {
    _gActor421600Animation18508Records,
    _gActor421600Animation18508Indices,
    { NULL, _gActor421600Animation18508Bank1, NULL, NULL, _gActor421600Animation18508Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation18AB8Bank1[14] = {
#include "assets/actor_421600_animation_18AB8_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation18AB8Bank4[116] = {
#include "assets/actor_421600_animation_18AB8_bank4.inc"
};

static AnimationRecord _gActor421600Animation18AB8Records[187] = {
#include "assets/actor_421600_animation_18AB8_records.inc"
};

static u16 _gActor421600Animation18AB8Indices[18] = {
#include "assets/actor_421600_animation_18AB8_indices.inc"
};

static AnimationSet _gActor421600Animation18AB8 = {
    _gActor421600Animation18AB8Records,
    _gActor421600Animation18AB8Indices,
    { NULL, _gActor421600Animation18AB8Bank1, NULL, NULL, _gActor421600Animation18AB8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation19178Bank1[13] = {
#include "assets/actor_421600_animation_19178_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation19178Bank4[160] = {
#include "assets/actor_421600_animation_19178_bank4.inc"
};

static AnimationRecord _gActor421600Animation19178Records[214] = {
#include "assets/actor_421600_animation_19178_records.inc"
};

static u16 _gActor421600Animation19178Indices[18] = {
#include "assets/actor_421600_animation_19178_indices.inc"
};

static AnimationSet _gActor421600Animation19178 = {
    _gActor421600Animation19178Records,
    _gActor421600Animation19178Indices,
    { NULL, _gActor421600Animation19178Bank1, NULL, NULL, _gActor421600Animation19178Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation1960CBank1[8] = {
#include "assets/actor_421600_animation_1960C_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation1960CBank4[100] = {
#include "assets/actor_421600_animation_1960C_bank4.inc"
};

static AnimationRecord _gActor421600Animation1960CRecords[150] = {
#include "assets/actor_421600_animation_1960C_records.inc"
};

static u16 _gActor421600Animation1960CIndices[18] = {
#include "assets/actor_421600_animation_1960C_indices.inc"
};

static AnimationSet _gActor421600Animation1960C = {
    _gActor421600Animation1960CRecords,
    _gActor421600Animation1960CIndices,
    { NULL, _gActor421600Animation1960CBank1, NULL, NULL, _gActor421600Animation1960CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation19E54Bank1[21] = {
#include "assets/actor_421600_animation_19E54_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation19E54Bank4[193] = {
#include "assets/actor_421600_animation_19E54_bank4.inc"
};

static AnimationRecord _gActor421600Animation19E54Records[254] = {
#include "assets/actor_421600_animation_19E54_records.inc"
};

static u16 _gActor421600Animation19E54Indices[20] = {
#include "assets/actor_421600_animation_19E54_indices.inc"
};

static AnimationSet _gActor421600Animation19E54 = {
    _gActor421600Animation19E54Records,
    _gActor421600Animation19E54Indices,
    { NULL, _gActor421600Animation19E54Bank1, NULL, NULL, _gActor421600Animation19E54Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation1A5F0Bank1[20] = {
#include "assets/actor_421600_animation_1A5F0_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation1A5F0Bank4[172] = {
#include "assets/actor_421600_animation_1A5F0_bank4.inc"
};

static AnimationRecord _gActor421600Animation1A5F0Records[235] = {
#include "assets/actor_421600_animation_1A5F0_records.inc"
};

static u16 _gActor421600Animation1A5F0Indices[20] = {
#include "assets/actor_421600_animation_1A5F0_indices.inc"
};

static AnimationSet _gActor421600Animation1A5F0 = {
    _gActor421600Animation1A5F0Records,
    _gActor421600Animation1A5F0Indices,
    { NULL, _gActor421600Animation1A5F0Bank1, NULL, NULL, _gActor421600Animation1A5F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation1ADFCBank1[15] = {
#include "assets/actor_421600_animation_1ADFC_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation1ADFCBank4[206] = {
#include "assets/actor_421600_animation_1ADFC_bank4.inc"
};

static AnimationRecord _gActor421600Animation1ADFCRecords[244] = {
#include "assets/actor_421600_animation_1ADFC_records.inc"
};

static u16 _gActor421600Animation1ADFCIndices[20] = {
#include "assets/actor_421600_animation_1ADFC_indices.inc"
};

static AnimationSet _gActor421600Animation1ADFC = {
    _gActor421600Animation1ADFCRecords,
    _gActor421600Animation1ADFCIndices,
    { NULL, _gActor421600Animation1ADFCBank1, NULL, NULL, _gActor421600Animation1ADFCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation1B610Bank1[14] = {
#include "assets/actor_421600_animation_1B610_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation1B610Bank4[207] = {
#include "assets/actor_421600_animation_1B610_bank4.inc"
};

static AnimationRecord _gActor421600Animation1B610Records[248] = {
#include "assets/actor_421600_animation_1B610_records.inc"
};

static u16 _gActor421600Animation1B610Indices[20] = {
#include "assets/actor_421600_animation_1B610_indices.inc"
};

static AnimationSet _gActor421600Animation1B610 = {
    _gActor421600Animation1B610Records,
    _gActor421600Animation1B610Indices,
    { NULL, _gActor421600Animation1B610Bank1, NULL, NULL, _gActor421600Animation1B610Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation1B8C4Bank1[5] = {
#include "assets/actor_421600_animation_1B8C4_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation1B8C4Bank4[55] = {
#include "assets/actor_421600_animation_1B8C4_bank4.inc"
};

static AnimationRecord _gActor421600Animation1B8C4Records[83] = {
#include "assets/actor_421600_animation_1B8C4_records.inc"
};

static u16 _gActor421600Animation1B8C4Indices[20] = {
#include "assets/actor_421600_animation_1B8C4_indices.inc"
};

static AnimationSet _gActor421600Animation1B8C4 = {
    _gActor421600Animation1B8C4Records,
    _gActor421600Animation1B8C4Indices,
    { NULL, _gActor421600Animation1B8C4Bank1, NULL, NULL, _gActor421600Animation1B8C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation1BBE4Bank1[6] = {
#include "assets/actor_421600_animation_1BBE4_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation1BBE4Bank4[66] = {
#include "assets/actor_421600_animation_1BBE4_bank4.inc"
};

static AnimationRecord _gActor421600Animation1BBE4Records[96] = {
#include "assets/actor_421600_animation_1BBE4_records.inc"
};

static u16 _gActor421600Animation1BBE4Indices[20] = {
#include "assets/actor_421600_animation_1BBE4_indices.inc"
};

static AnimationSet _gActor421600Animation1BBE4 = {
    _gActor421600Animation1BBE4Records,
    _gActor421600Animation1BBE4Indices,
    { NULL, _gActor421600Animation1BBE4Bank1, NULL, NULL, _gActor421600Animation1BBE4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation1C09CBank1[9] = {
#include "assets/actor_421600_animation_1C09C_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation1C09CBank4[68] = {
#include "assets/actor_421600_animation_1C09C_bank4.inc"
};

static AnimationRecord _gActor421600Animation1C09CRecords[188] = {
#include "assets/actor_421600_animation_1C09C_records.inc"
};

static u16 _gActor421600Animation1C09CIndices[18] = {
#include "assets/actor_421600_animation_1C09C_indices.inc"
};

static AnimationSet _gActor421600Animation1C09C = {
    _gActor421600Animation1C09CRecords,
    _gActor421600Animation1C09CIndices,
    { NULL, _gActor421600Animation1C09CBank1, NULL, NULL, _gActor421600Animation1C09CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation1C6A0Bank1[11] = {
#include "assets/actor_421600_animation_1C6A0_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation1C6A0Bank4[138] = {
#include "assets/actor_421600_animation_1C6A0_bank4.inc"
};

static AnimationRecord _gActor421600Animation1C6A0Records[195] = {
#include "assets/actor_421600_animation_1C6A0_records.inc"
};

static u16 _gActor421600Animation1C6A0Indices[18] = {
#include "assets/actor_421600_animation_1C6A0_indices.inc"
};

static AnimationSet _gActor421600Animation1C6A0 = {
    _gActor421600Animation1C6A0Records,
    _gActor421600Animation1C6A0Indices,
    { NULL, _gActor421600Animation1C6A0Bank1, NULL, NULL, _gActor421600Animation1C6A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation1CE30Bank1[13] = {
#include "assets/actor_421600_animation_1CE30_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation1CE30Bank4[185] = {
#include "assets/actor_421600_animation_1CE30_bank4.inc"
};

static AnimationRecord _gActor421600Animation1CE30Records[241] = {
#include "assets/actor_421600_animation_1CE30_records.inc"
};

static u16 _gActor421600Animation1CE30Indices[18] = {
#include "assets/actor_421600_animation_1CE30_indices.inc"
};

static AnimationSet _gActor421600Animation1CE30 = {
    _gActor421600Animation1CE30Records,
    _gActor421600Animation1CE30Indices,
    { NULL, _gActor421600Animation1CE30Bank1, NULL, NULL, _gActor421600Animation1CE30Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation1D030Bank1[2] = {
#include "assets/actor_421600_animation_1D030_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation1D030Bank4[24] = {
#include "assets/actor_421600_animation_1D030_bank4.inc"
};

static AnimationRecord _gActor421600Animation1D030Records[79] = {
#include "assets/actor_421600_animation_1D030_records.inc"
};

static u16 _gActor421600Animation1D030Indices[18] = {
#include "assets/actor_421600_animation_1D030_indices.inc"
};

static AnimationSet _gActor421600Animation1D030 = {
    _gActor421600Animation1D030Records,
    _gActor421600Animation1D030Indices,
    { NULL, _gActor421600Animation1D030Bank1, NULL, NULL, _gActor421600Animation1D030Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor421600Animation1EF6CBank1[84] = {
#include "assets/actor_421600_animation_1EF6C_bank1.inc"
};

static AnimationPackedRotation _gActor421600Animation1EF6CBank4[736] = {
#include "assets/actor_421600_animation_1EF6C_bank4.inc"
};

static AnimationRecord _gActor421600Animation1EF6CRecords[992] = {
#include "assets/actor_421600_animation_1EF6C_records.inc"
};

static u16 _gActor421600Animation1EF6CIndices[18] = {
#include "assets/actor_421600_animation_1EF6C_indices.inc"
};

static AnimationSet _gActor421600Animation1EF6C = {
    _gActor421600Animation1EF6CRecords,
    _gActor421600Animation1EF6CIndices,
    { NULL, _gActor421600Animation1EF6CBank1, NULL, NULL, _gActor421600Animation1EF6CBank4, NULL, NULL, NULL },
};

s8 gDesertChaserClipStartFrames[25][25] = {
    /*  0 */ { 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  1 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  2 */ { 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  3 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  4 */ { 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  5 */ { 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  6 */ { 5, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  7 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  8 */ { 5, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /*  9 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 10 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 11 */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 12 */ { 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 13 */ { 5, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 14 */ { 5, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    /* 15 */ { 5, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
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

u8 D_actor_421600_80151028[104] = {
    56,
    91,
    20,
    128,
    244,
    93,
    20,
    128,
    40,
    100,
    20,
    128,
    0,
    106,
    20,
    128,
    52,
    110,
    20,
    128,
    12,
    117,
    20,
    128,
    12,
    123,
    20,
    128,
    104,
    125,
    20,
    128,
    140,
    131,
    20,
    128,
    220,
    135,
    20,
    128,
    192,
    141,
    20,
    128,
    60,
    143,
    20,
    128,
    224,
    149,
    20,
    128,
    188,
    222,
    20,
    128,
    192,
    228,
    20,
    128,
    80,
    236,
    20,
    128,
    80,
    238,
    20,
    128,
    140,
    13,
    21,
    128,
    164,
    152,
    20,
    128,
    168,
    157,
    20,
    128,
    40,
    163,
    20,
    128,
    216,
    168,
    20,
    128,
    152,
    175,
    20,
    128,
    44,
    180,
    20,
    128,
    192,
    141,
    20,
    128,
    0,
    0,
    0,
    0,
};

AnimationSet* gDesertChaserFrontAnim[5] = { NULL, &_gActor421600Animation19E54, &_gActor421600Animation1ADFC, &_gActor421600Animation1B8C4, NULL };

AnimationSet* gDesertChaserRearAnim[5] = { NULL, &_gActor421600Animation1A5F0, &_gActor421600Animation1B610, &_gActor421600Animation1BBE4, NULL };

SVECTOR gDesertChaserHitOffsets[12] = { { 60, -12, 30, 2 }, { -50, -130, 29, 2 }, { 20, -70, 25, 2 }, { -30, -65, 25, 2 }, { 60, -120, 30, 2 }, { 20, -20, -5, 2 }, { -15, -50, 0, 2 }, { 2, 10, -15, 2 }, { 14, 0, 0, 7 }, { 25, 0, 0, 2 }, { -14, 0, 0, 9 }, { -25, 0, 0, 2 } };

TaskMessageEntry D_actor_421600_80151118[8] = {
    { ACTOR421600_MESSAGE_IGNORE_2015, _actor421600IgnoreMessage2015 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetVisibility },
    { ACTOR_MESSAGE_IS_PRESENT, _actor421600IsPresent },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceYawFirst },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor421600ApplyCommand },
    { ACTOR_MESSAGE_PLAY_ANIMATION, _desertChaserMsgPlayAnim },
    { ROOM_MESSAGE_ACTOR_EVENT, _actor421600OnRoomEvent },
    { TASK_MESSAGE_TABLE_END, NULL },
};

SVECTOR D_actor_421600_80151158[13] = {
    { 1000, 0, 4500, 0 },
    { 1000, 0, 4500, 0 },
    { -1000, 0, 4500, 0 },
    { -4500, 0, 4500, 0 },
    { -4500, 0, 1500, 0 },
    { -4500, 0, -1500, 0 },
    { -4500, 0, -5000, 0 },
    { -1000, 0, -5300, 0 },
    { 1000, 0, -5500, 0 },
    { 4500, 0, -5300, 0 },
    { 4500, 0, -1500, 0 },
    { 4500, 0, 1500, 0 },
    { 0, 0, 0, 0 },
};

s8 D_actor_421600_801511C0[16] = {
    3,
    2,
    1,
    0,
    4,
    13,
    12,
    11,
    5,
    15,
    14,
    10,
    6,
    7,
    8,
    9,
};

s8 D_actor_421600_801511D0[4] = {
    1,
    0,
    3,
    2,
};

s32 D_actor_421600_801511D4[4][8] = {
    { 0x1F948, 538, 0x1FAC1, 2373, 0x1038C, 0xF78A, 0x108BC, 0xF930 },
    { 0x102D1, 751, 0x101C1, 2219, 0x1022D, 2329, 0x106DC, 0xFB69 },
    { 0x1F948, 538, 0x1FAC1, 2373, 0x10247, 2114, 0x10842, 1445 },
    { 0x1F7BC, 1553, 0x1FBDC, 1606, 0x10459, 0xF66F, 0x106C9, 0xFB85 },
};

TaskDesc D_actor_421600_80151254 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, _desertChaserTask, { .model = &_gActor421600DesertChaserBody } };

SVECTOR ActorContact_ScratchPosition;

s16 D_actor_421600_80151268;

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

static __inline__ s32  _actor421600HasPlayerBodyContact(const WorldCollisionContact* contacts);
static s32             _actor421600PushOutsideArenaCenter(GfxCoord* coord);
static void            _actor421600SnapToSouthArenaArc(GfxCoord* coord);
static __inline__ void _actor421600BindLightingMatrices(Task* actor);
static void            _actor421600Spawn(Enemy* enemy, Task* task);
static __inline__ s32  _actor421600FindAttackContact(const WorldCollisionContact* contacts,
                                                     SVECTOR*                     hitPosition);
static void            _desertChaserDamage(Task* task);
static void            _actor421600ScriptAnimationState(Task* task);
static __inline__ s16  _actor421600GetArenaZone(const GfxCoord* coord);
static void            _actor421600TrackArenaRouteState(Task* task);
static void            _actor421600ShrinkDeathState(Task* task);
static void            _actor421600WaitToRespawnState(Task* task);
static void            _actor421600LeapBackState(Task* task);
static void            _actor421600WatchRunPlayerState(Task* task);
static void            _actor421600WallKnockDownState(Task* task);
static void            _actor421600DownedWaitState(Task* task);
static void            _actor421600EntranceLungeState(Task* task);
static void            _actor421600FleeState(Task* task);
static void            _actor421600MoveToZone5State(Task* task);
static void            _actor421600ArenaTransitionState(Task* task);
static __inline__ s32  _actor421600GetRouteQuadrant(s32 x, s32 z);
static void            _actor421600RouteRoamState(Task* task);
static void            _actor421600BurstDeathState(Task* task);
static void            _actor421600FrameState(Enemy* enemy, Task* task);

/// Returns whether a sphere's contacts include a player or companion body.
///
/// Reads at most `DESERT_CHASER_CONTACTS` entries, stopping at the first zero
/// key. Contact flags and the companion key bit are not tested. The table
/// must remain readable throughout the call; no storage is retained.
static __inline__ s32 _actor421600HasPlayerBodyContact(const WorldCollisionContact* contacts)
{
    s16 contactIndex;
    for (contactIndex = 0; contactIndex < DESERT_CHASER_CONTACTS; contactIndex++) {
        if (contacts[contactIndex].key.value == 0)
            break;
        if ((contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY)
            return 1;
    }
    return 0;
}

#include "../../shared/actor_contacts_turn_joint.inc.c"

#include "../../shared/actor_contacts_steer.inc.c"

#include "../../shared/actor_contacts_push_contact.inc.c"

/// Places the chaser's root XZ and yaw and refreshes its composed transform.
///
/// X/Z use game-coordinate units in the root's parent frame; yaw uses 4096
/// units per turn. Requires a live model root. Y and parent links are preserved;
/// replacing yaw resets the rotation and scale before composition.
static inline void _actor421600PlaceRootXZ(Task* task, s32 x, s32 z, s16 yaw)
{
    task->extra.tmd->coords->coord.t[0] = x;
    task->extra.tmd->coords->coord.t[2] = z;
    gfxRotMatrixY(&task->extra.tmd->coords->coord, yaw, GRAPHICS_ROTATION_REPLACE);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
}

/// Applies chaser-pack commands and the Water Tower room's run/battle commands.
///
/// Borrows a complete `request` through synchronous dispatch. Stage 9/area 1
/// controls pursuit holdoff and route release and always returns 1. Other
/// contexts cache stage, area and the low command byte; only Water Tower
/// commands then restore health, place the chaser, hide it or release a hold.
/// Returns 1 for a recognized room command and 0 for another context or command.
/// The task must own live chaser work, enemy and model objects. The message ID
/// and extra payload are unused; placement preserves the root's Y translation.
static s32 _actor421600ApplyCommand(Task* task, s32 messageId, const ActorCommand* request, s32 unusedExtra)
{
    enum {
        ACTOR421600_PACK_CONTEXT               = 9 | (1 << 8),
        ACTOR421600_ROOM_CONTEXT               = GAME_STAGE_DRYFIELD | (GAME_AREA_DRYFIELD_WATER_TOWER << 8),
        ACTOR421600_PACK_COMMAND_HOLDOFF       = 1,
        ACTOR421600_PACK_COMMAND_KEEP_ROAM     = 2,
        ACTOR421600_PACK_COMMAND_RELEASE_ROUTE = 3
    };
    DesertChaserWork* work;
    Enemy*            enemy;
    s16               placeIndex;

    work  = task->work;
    enemy = task->spawnArg2.pointer;

    if (request->context.key == ACTOR421600_PACK_CONTEXT) {
        switch (request->command) {
            case ACTOR421600_PACK_COMMAND_HOLDOFF:
                work->chaseHoldoff = work->chaseHoldoffFrames;
                break;
            case ACTOR421600_PACK_COMMAND_KEEP_ROAM:
                if (work->state == DESERT_CHASER_STATE_ROAM) {
                    work->state = DESERT_CHASER_STATE_ROAM;
                }
                break;
            case ACTOR421600_PACK_COMMAND_RELEASE_ROUTE:
                if (work->state == DESERT_CHASER_STATE_SCRIPT_ANIMATION) {
                    work->state = ACTOR421600_STATE_TRACK_ARENA_ROUTE;
                }
                break;
        }
        return 1;
    }

    // Cache non-pack commands even when their context is not handled here.
    work->lastCommand.fields.stage   = request->context.loc.stage;
    work->lastCommand.fields.area    = request->context.loc.area;
    work->lastCommand.fields.command = (u8)request->command;

    if (request->context.key != ACTOR421600_ROOM_CONTEXT) {
        return 0;
    }

    switch (request->command) {
        case DRYFIELD_WATER_TOWER_CHASER_COMMAND_START_BATTLE:
            enemy->hp = D_actor_421600_8013EF38.hpMax;
            if ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) == 0) {
                work->state = ACTOR421600_STATE_TRACK_ARENA_ROUTE;
            }
            return 1;

        case DRYFIELD_WATER_TOWER_CHASER_COMMAND_START_RUN:
            placeIndex = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
            enemy->hp  = D_actor_421600_8013EF38.hpMax;
            switch (placeIndex) {
                case 0:
                    if (D_actor_421600_80151268 >= 4) {
                        if (work->state != DESERT_CHASER_STATE_HIDDEN) {
                            break;
                        }
                        _actor421600PlaceRootXZ(task, 0x1057, -0x11A3, -0x400);
                        work->state     = ACTOR421600_STATE_WATCH_RUN_PLAYER;
                        work->prevState = DESERT_CHASER_PREV_STATE_NONE;
                        break;
                    }
                    work->state     = DESERT_CHASER_STATE_HIDDEN;
                    work->prevState = DESERT_CHASER_PREV_STATE_NONE;
                    break;
                case 1:
                    if (D_actor_421600_80151268 >= 5) {
                        if (work->state != DESERT_CHASER_STATE_HIDDEN) {
                            break;
                        }
                        _actor421600PlaceRootXZ(task, 0x1467, 0x4B9, 0x7BC);
                        work->state     = ACTOR421600_STATE_WATCH_RUN_PLAYER;
                        work->prevState = DESERT_CHASER_PREV_STATE_NONE;
                        break;
                    }
                    work->state     = DESERT_CHASER_STATE_HIDDEN;
                    work->prevState = DESERT_CHASER_PREV_STATE_NONE;
                    break;
            }
            worldCoordSetActorColorMode(enemy, ENEMY_COLOR_DEFAULT);
            enemy->reactionFlags = 0;
            enemy->hp            = D_actor_421600_8013EF38.hpMax;
            return 1;

        case DRYFIELD_WATER_TOWER_CHASER_COMMAND_START_FINAL_BATTLE:
            switch (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) {
                case 0:
                    if (D_actor_421600_80151268 <= 0) {
                        break;
                    }
                    _actor421600PlaceRootXZ(task, -0xD40, 0x104F, -0x76C);
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_DEFAULT);
                    enemy->reactionFlags = 0;
                    enemy->hp            = D_actor_421600_8013EF38.hpMax;
                    work->state          = ACTOR421600_STATE_MOVE_TO_ZONE5;
                    break;
                case 1:
                    if (D_actor_421600_80151268 < 2) {
                        break;
                    }
                    _actor421600PlaceRootXZ(task, 0x138C, 0x4B2, 0x7BC);
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_DEFAULT);
                    enemy->reactionFlags = 0;
                    enemy->hp            = D_actor_421600_8013EF38.hpMax;
                    work->state          = ACTOR421600_STATE_MOVE_TO_ZONE5;
                    break;
            }
            if (D_actor_421600_80151268 == 0) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, 0, 0);
            }
            return 1;

        case DRYFIELD_WATER_TOWER_CHASER_COMMAND_END_RUN:
            if (work->playerHeld == 1) {
                work->playerHeld = 0;
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 2, 0);
            }
            if (work->state != ACTOR421600_STATE_KNOCK_DOWN && work->state != DESERT_CHASER_STATE_DOWNED && work->state != DESERT_CHASER_STATE_DEATH &&
                work->state != ACTOR421600_STATE_WAIT_RESPAWN && work->state != DESERT_CHASER_STATE_HIDDEN && work->state != ACTOR421600_STATE_BURST_DEATH) {
                work->state     = DESERT_CHASER_STATE_FLEE;
                work->prevState = DESERT_CHASER_PREV_STATE_NONE;
            }
            return 1;

        case DRYFIELD_WATER_TOWER_CHASER_COMMAND_HIDE:
            work->state     = DESERT_CHASER_STATE_HIDDEN;
            work->prevState = DESERT_CHASER_PREV_STATE_NONE;
            return 1;

        default:
            return 0;
    }
}

#include "../../shared/limb_shadows_segment.inc.c"

/// Pushes a coordinate out of the arena's central exclusion rectangle.
///
/// Acts on X in [-3149, 3349] and Z in [-3149, 3149], in root-parent units.
/// Chooses the nearer X/Z exit, favoring X on a tie, and places it 150 units
/// beyond that face: X -3300/3500 or Z -3300/3300. Returns 1 and marks
/// composition dirty after moving; otherwise returns 0 without changing it.
/// Halfword reads and corrections preserve the original narrowing behavior.
static s32 _actor421600PushOutsideArenaCenter(GfxCoord* coord)
{
    enum { ACTOR421600_CENTER_MIN                     = -3149,
           ACTOR421600_CENTER_X_MAX_EXCLUSIVE         = 3350,
           ACTOR421600_CENTER_Z_MAX_EXCLUSIVE         = 3150,
           ACTOR421600_CENTER_NEGATIVE_FACE_MAGNITUDE = 3150,
           ACTOR421600_CENTER_NEGATIVE_EXIT_MAGNITUDE = 3300,
           ACTOR421600_CENTER_X_POSITIVE_EXIT         = 3500 };
    s16 xCorrection;
    s16 zCorrection;
    s32 xDistance;
    s32 zDistance;
    s32 x;
    s32 z;
    s32 edgeZ;

    x = coord->coord.t[0];
    if ((x >= ACTOR421600_CENTER_MIN) && (x < ACTOR421600_CENTER_X_MAX_EXCLUSIVE)) {
        z = coord->coord.t[2];
        if (z < ACTOR421600_CENTER_Z_MAX_EXCLUSIVE) {
            if (z >= ACTOR421600_CENTER_MIN) {
                if ((ACTOR421600_CENTER_X_MAX_EXCLUSIVE - x) > (x + ACTOR421600_CENTER_NEGATIVE_FACE_MAGNITUDE)) {
                    xCorrection = -((u16)coord->coord.t[0] + ACTOR421600_CENTER_NEGATIVE_EXIT_MAGNITUDE);
                } else {
                    xCorrection = ACTOR421600_CENTER_X_POSITIVE_EXIT - (u16)coord->coord.t[0];
                }
                edgeZ = coord->coord.t[2];
                if ((ACTOR421600_CENTER_Z_MAX_EXCLUSIVE - edgeZ) > (edgeZ + ACTOR421600_CENTER_NEGATIVE_FACE_MAGNITUDE)) {
                    zCorrection = -((u16)coord->coord.t[2] + ACTOR421600_CENTER_NEGATIVE_EXIT_MAGNITUDE);
                } else {
                    zCorrection = ACTOR421600_CENTER_NEGATIVE_EXIT_MAGNITUDE - (u16)coord->coord.t[2];
                }
                xDistance = ABS(xCorrection);
                zDistance = ABS(zCorrection);
                if (zDistance < xDistance) {
                    coord->coord.t[2] += zCorrection;
                } else {
                    coord->coord.t[0] += xCorrection;
                }
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                return 1;
            }
        }
    }
    return 0;
}

/// Snaps qualifying chaser positions onto the arena's southern 700-unit arc.
///
/// Only X in 501..1499 and Z below -500 are considered, in root-parent units.
/// An XZ offset narrowed to halfwords from (1000, -1) must lie strictly inside
/// radius 720. Its normalized direction is scaled to 700 and stored about
/// (1000, 0), preserving the one-unit center difference. Y and rotation stay
/// intact; a correction marks composition dirty. Requires scratch-stack room
/// for one OverlayRangeScratch and changes GTE state.
static void _actor421600SnapToSouthArenaArc(GfxCoord* coord)
{
    enum { ACTOR421600_SOUTH_ARC_X_MIN       = 501,
           ACTOR421600_SOUTH_ARC_X_SPAN      = 999,
           ACTOR421600_SOUTH_ARC_Z_GATE      = 500,
           ACTOR421600_SOUTH_ARC_CENTER_X    = 1000,
           ACTOR421600_SOUTH_ARC_TEST_RADIUS = 720,
           ACTOR421600_SOUTH_ARC_RADIUS      = 700 };
    SVECTOR  radialOffset;
    SVECTOR* radialDirection;
    s32      outsideCircle;

    if ((u32)(coord->coord.t[0] - ACTOR421600_SOUTH_ARC_X_MIN) < ACTOR421600_SOUTH_ARC_X_SPAN) {
        if (coord->coord.t[2] < ACTOR421600_SOUTH_ARC_Z_GATE) {
            if (coord->coord.t[2] < -ACTOR421600_SOUTH_ARC_Z_GATE) {
                radialOffset.vx = (u16)coord->coord.t[0] - ACTOR421600_SOUTH_ARC_CENTER_X;
                radialOffset.vy = 0;
                radialOffset.vz = (u16)coord->coord.t[2] + 1;
                radialDirection = &radialOffset;
                outsideCircle   = _actorRangeOutsideRadiusXZ(radialDirection, ACTOR421600_SOUTH_ARC_TEST_RADIUS);
                if (outsideCircle != 0) {
                    return;
                }
                VectorNormalSS(radialDirection, radialDirection);
                gte_lddp(ACTOR421600_SOUTH_ARC_RADIUS);
                gte_ldsv(radialDirection);
                gte_gpf12();
                gte_stsv(radialDirection);
                coord->coord.t[0]   = radialOffset.vx + ACTOR421600_SOUTH_ARC_CENTER_X;
                coord->coord.t[2]   = radialOffset.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
            }
        }
    }
}

#include "../../shared/desert_chaser_avoid_walk.inc.c"

/// Advances the two animation rigs and mixes secondary rotation into slots 1..10.
///
/// The Water Tower build uses 3/4 main weight on slot 1, 1/2 on slot 2,
/// 1502/4096 on slots 3..5, and 3024/4096 on slots 6..10. All slots 1..17
/// advance the main rig at `animRate - 3`; the blended slots also advance the
/// secondary rig at `blendRate`. Rates use sixteenths of a frame, and signed
/// negative rates are retained. Both rigs and their borrowed clip data must be live.
static void _desertChaserBlendTick(Task* task)
{
    AnimationPose     mainPose;
    AnimationPose     blendPose;
    DesertChaserWork* work;
    s32               mainWeight;
    s32               blendWeight;
    s16               slotIndex;
    s16               nextSlotIndex;

    slotIndex = 1;
    work      = task->work;
    // Leave slot 0 intact; only slots 1..10 mix the secondary rotation.
    do {
        switch (slotIndex) {
            case 1:
                mainWeight = DESERT_CHASER_BLEND_WEIGHT_THREE_QUARTERS;
                break;
            case 2:
                mainWeight = DESERT_CHASER_BLEND_WEIGHT_HALF;
                break;
            case 3:
            case 4:
            case 5:
                mainWeight = DESERT_CHASER_BLEND_WEIGHT_REDUCED;
                break;
            default:
                mainWeight = DESERT_CHASER_BLEND_WEIGHT_DEFAULT;
                break;
        }
        blendWeight = DESERT_CHASER_BLEND_WEIGHT_ONE - mainWeight;
        if (slotIndex < DESERT_CHASER_BLEND_FIRST_UNBLENDED_SLOT) {
            work->blend.slots[slotIndex].rate = work->blendRate;
            work->rig.slots[slotIndex].rate   = (work->animRate - DESERT_CHASER_BLEND_MAIN_RATE_BIAS);
            animationTickSlotPose(&work->rig.anim, slotIndex, &mainPose, NULL);
            animationTickSlotPose(&work->blend.anim, slotIndex, &blendPose, NULL);
            animationApplyPoseWithBlendedRotation(&work->rig.anim, slotIndex, &mainPose, &blendPose, mainWeight, blendWeight);
        } else {
            work->rig.slots[slotIndex].rate = (work->animRate - DESERT_CHASER_BLEND_MAIN_RATE_BIAS);
            animationTickSlot(&work->rig.anim, slotIndex);
        }
        nextSlotIndex = slotIndex + 1;
        slotIndex     = nextSlotIndex;
    } while (nextSlotIndex < (s32)ARRAY_SIZE(work->rig.slots));
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
    SVECTOR effectOffset;
    u32     previousCueIndex;
    s32     resetCueHistory = 1;

    // The low ten record-index bits identify cues, not elapsed animation frames.
    switch (work->animId) {
        case 0:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 9) {
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != 9) {
                    work->lastCueFrames[1] = 9;
                    effectOffset.vz        = 0;
                    effectOffset.vx        = 0;
                    effectOffset.vy        = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(17)) {
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[17], (DESERT_CHASER_CUE_DUST_RECURSIVE | (2 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 640), &effectOffset);
                    }
                    effectOffset.vz = 0;
                    effectOffset.vx = 0;
                    effectOffset.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(9)) {
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[9], (DESERT_CHASER_CUE_DUST_RECURSIVE | (2 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 288), &effectOffset);
                    }
                    return DESERT_CHASER_SOUND_STEP_2;
                }
                work->lastCueFrames[1] = previousCueIndex;
                resetCueHistory        = 0;
            }
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 6) {
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != 6) {
                    work->lastCueFrames[1] = 6;
                    effectOffset.vz        = 0;
                    effectOffset.vx        = 0;
                    effectOffset.vy        = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(14)) {
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[14], (DESERT_CHASER_CUE_DUST_RECURSIVE | (2 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 544), &effectOffset);
                    }
                    effectOffset.vz = 0;
                    effectOffset.vx = 0;
                    effectOffset.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(7)) {
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[7], (DESERT_CHASER_CUE_DUST_RECURSIVE | (2 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 288), &effectOffset);
                    }
                    return DESERT_CHASER_SOUND_STEP_1;
                }
                work->lastCueFrames[1] = previousCueIndex;
                resetCueHistory        = 0;
            }
            break;
        case 10:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 10) {
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != 10) {
                    work->lastCueFrames[1] = 10;
                    effectOffset.vz        = 0;
                    effectOffset.vx        = 0;
                    effectOffset.vy        = 0x0;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(0)) {
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[0], (DESERT_CHASER_CUE_DUST_RECURSIVE | (4 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 2560), &effectOffset);
                    }
                    return DESERT_CHASER_SOUND_CUE_05;
                }
                work->lastCueFrames[1] = previousCueIndex;
                resetCueHistory        = 0;
            }
            break;
        case 3:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 12) {
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != 12) {
                    work->lastCueFrames[1] = 12;
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
                    effectOffset.vz        = 0;
                    effectOffset.vx        = 0;
                    effectOffset.vy        = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(9)) {
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[9], (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512), &effectOffset);
                    }
                    effectOffset.vz = 0;
                    effectOffset.vx = 0;
                    effectOffset.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(7)) {
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[7], (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512), &effectOffset);
                    }
                    return DESERT_CHASER_SOUND_CUE_11;
                }
                work->lastCueFrames[1] = previousCueIndex;
                resetCueHistory        = 0;
            }
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 12) {
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != 12) {
                    work->lastCueFrames[1] = 12;
                    effectOffset.vz        = 0;
                    effectOffset.vx        = 0;
                    effectOffset.vy        = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(17)) {
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[17], (DESERT_CHASER_CUE_DUST_RECURSIVE | (4 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 1152), &effectOffset);
                    }
                    effectOffset.vz = 0;
                    effectOffset.vx = 0;
                    effectOffset.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(14)) {
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[14], (DESERT_CHASER_CUE_DUST_RECURSIVE | (4 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 1152), &effectOffset);
                    }
                    return DESERT_CHASER_SOUND_CUE_11;
                }
                work->lastCueFrames[1] = previousCueIndex;
                resetCueHistory        = 0;
            }
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 13) {
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != 13) {

                    work->lastCueFrames[1] = 13;
                    effectOffset.vz        = 0;
                    effectOffset.vx        = 0;
                    effectOffset.vy        = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(9)) {
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[9], (DESERT_CHASER_CUE_DUST_RECURSIVE | (2 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512), &effectOffset);
                    }
                    effectOffset.vz = 0;
                    effectOffset.vx = 0;
                    effectOffset.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(7)) {
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[7], (DESERT_CHASER_CUE_DUST_RECURSIVE | (2 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 576), &effectOffset);
                    }
                    effectOffset.vz = 0;
                    effectOffset.vx = 0;
                    effectOffset.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(17)) {
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[17], (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 768), &effectOffset);
                    }

                    effectOffset.vz = 0;
                    effectOffset.vx = 0;
                    effectOffset.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(14)) {
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[14], (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 832), &effectOffset);
                    }
                    return DESERT_CHASER_SOUND_CUE_11;
                }
                work->lastCueFrames[1] = previousCueIndex;
                resetCueHistory        = 0;
            }
            break;
        case 21:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 6) {
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != 6) {
                    work->lastCueFrames[1] = 6;
                    effectOffset.vz        = 0;
                    effectOffset.vx        = 0;
                    effectOffset.vy        = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(9)) {
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[9], (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512), &effectOffset);
                    }
                    effectOffset.vz = 0;
                    effectOffset.vx = 0;
                    effectOffset.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(7)) {
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[7], (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512), &effectOffset);
                    }
                    return DESERT_CHASER_SOUND_STEP_1;
                }
                work->lastCueFrames[1] = previousCueIndex;
                resetCueHistory        = 0;
            }
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 9) {
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != 9) {
                    work->lastCueFrames[1] = 9;
                    effectOffset.vz        = 0;
                    effectOffset.vx        = 0;
                    effectOffset.vy        = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(9)) {
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[9], (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512), &effectOffset);
                    }
                    effectOffset.vz = 0;
                    effectOffset.vx = 0;
                    effectOffset.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(17)) {
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[17], (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512), &effectOffset);
                    }
                    return DESERT_CHASER_SOUND_STEP_1;
                }
                work->lastCueFrames[1] = previousCueIndex;
                resetCueHistory        = 0;
            }
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 14) {
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != 14) {
                    work->lastCueFrames[1] = 14;
                    effectOffset.vz        = 0;
                    effectOffset.vx        = 0;
                    effectOffset.vy        = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(14)) {
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[14], (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512), &effectOffset);
                    }
                    effectOffset.vz = 0;
                    effectOffset.vx = 0;
                    effectOffset.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(17)) {
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[17], (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512), &effectOffset);
                    }
                    return DESERT_CHASER_SOUND_STEP_2;
                }
                work->lastCueFrames[1] = previousCueIndex;
                resetCueHistory        = 0;
            }
            break;
        case 20:
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 6) {
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != 6) {
                    work->lastCueFrames[1] = 6;
                    effectOffset.vz        = 0;
                    effectOffset.vx        = 0;
                    effectOffset.vy        = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(9)) {
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[9], (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512), &effectOffset);
                    }
                    effectOffset.vz = 0;
                    effectOffset.vx = 0;
                    effectOffset.vy = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(7)) {
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[7], (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512), &effectOffset);
                    }
                    return DESERT_CHASER_SOUND_STEP_1;
                }
                work->lastCueFrames[1] = previousCueIndex;
                resetCueHistory        = 0;
            }
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 10) {
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != 10) {
                    work->lastCueFrames[1] = 10;
                    effectOffset.vz        = 0;
                    effectOffset.vx        = 0;
                    effectOffset.vy        = 0x2BC;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(7)) {
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[7], (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512), &effectOffset);
                    }
                    effectOffset.vz = 0;
                    effectOffset.vx = 0;
                    effectOffset.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(14)) {
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[14], (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512), &effectOffset);
                    }
                    return DESERT_CHASER_SOUND_STEP_1;
                }
                work->lastCueFrames[1] = previousCueIndex;
                resetCueHistory        = 0;
            }
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 14) {
                previousCueIndex = work->lastCueFrames[1];
                if (previousCueIndex != 14) {
                    work->lastCueFrames[1] = 14;
                    effectOffset.vz        = 0;
                    effectOffset.vx        = 0;
                    effectOffset.vy        = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(14)) {
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[14], (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512), &effectOffset);
                    }
                    effectOffset.vz = 0;
                    effectOffset.vx = 0;
                    effectOffset.vy = 0x258;
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED && _desertChaserPartSupportsDust(17)) {
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[17], (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 512), &effectOffset);
                    }
                    return DESERT_CHASER_SOUND_STEP_2;
                }
                work->lastCueFrames[1] = previousCueIndex;
                resetCueHistory        = 0;
            }
            break;
    }
    // An intervening non-cue record rearms the one-shot cue history.
    if (resetCueHistory == 1) {
        memFillBytes(work->lastCueFrames, 0, sizeof(work->lastCueFrames));
    }
    return 0;
}

#include "../../shared/desert_chaser_anim_tick.inc.c"

/// Binds the model's borrowed lighting matrices to its chaser work block.
///
/// Both objects must be live; the work block owns the matrix storage and must
/// outlive every model draw that uses these pointers. Matrix contents are
/// initialized by the caller's lighting setup.
static __inline__ void _actor421600BindLightingMatrices(Task* actor)
{
    DesertChaserWork* work;
    TmdObject*        model;
    work            = actor->work;
    model           = actor->extra.tmd;
    model->lightMtx = &work->lightMtx;
    model->colorMtx = &work->colorMtx;
}

/// Creates the Water Tower chaser's work, animation rigs and collision bodies.
///
/// Requires a live enemy and 18-coordinate model. Work owns the lighting and
/// contact storage until teardown; allocation failure destroys the enemy.
/// Spawn argument bits 16..19 select hidden/steer/patrol, and bits 0..3 select
/// tuning. Patrol points use root-parent units; animation rates are sixteenths.
/// Success binds the root to the view, initializes the eight-life pack and
/// advances the task to its frame driver.
static void _actor421600Spawn(Enemy* enemy, Task* task)
{
    enum {
        ACTOR421600_PACK_INITIAL_LIVES          = 8,
        ACTOR421600_SPAWN_MODE_HIDDEN           = 1,
        ACTOR421600_SPAWN_MODE_STEER            = 2,
        ACTOR421600_SPAWN_ARGUMENT_MASK         = 0xF,
        ACTOR421600_SPAWN_MODE_SHIFT            = 16,
        ACTOR421600_INITIAL_CLIP                = 1,
        ACTOR421600_INITIAL_PATROL_DISTANCE     = 5000,
        ACTOR421600_ENEMY_BODY_KEY              = 0x30001,
        ACTOR421600_ROOT_SPHERE_HEIGHT          = 284,
        ACTOR421600_ROOT_SPHERE_RADIUS          = 300,
        ACTOR421600_PROBE_HEIGHT                = 384,
        ACTOR421600_INITIAL_PROBE_REACH         = 700,
        ACTOR421600_REAR_SPHERE_RADIUS          = 256,
        ACTOR421600_REAR_SPHERE_OFFSET          = -256,
        ACTOR421600_INITIAL_PLAYER_BLEND_FRAMES = 3
    };
    SVECTOR             patrolDirection;
    s32                 spawnMode;
    SVECTOR*            direction;
    VECTOR              lightingPosition;
    TmdObject*          model;
    GfxCoord*           rootCoord;
    DesertChaserWork*   work;
    WorldCollisionBody* frontBody;
    WorldCollisionBody* rearBody;
    rootCoord  = task->extra.tmd->coords;
    model      = task->extra.tmd;
    work       = memCalloc(sizeof(*work), 0);
    task->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    sceneAcquireBattleRef(0);
    task->exitCallback = _actor421600Exit;
    _actor421600BindLightingMatrices(task);
    enemy->field_4    = &task->extra.tmd->coords[0].coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &task->extra.tmd->coords[2];
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->reactionFlags          = 0;
    enemy->hp                     = (s16)D_actor_421600_8013EF38.hpMax;
    enemy->param                  = &D_actor_421600_8013EF38;
    enemy->recs                   = work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts;
    // Both rigs borrow the same clip bank and own separate pose storage.
    animationInitContext(&work->rig.anim, (AnimationSet**)D_actor_421600_80151028, model, work->rig.poses, work->rig.slots);
    animationInitContext(&work->blend.anim, (AnimationSet**)D_actor_421600_80151028, model, work->blend.poses, work->blend.slots);
    work->animRequest   = DESERT_CHASER_ANIM_REQUEST_RESET;
    work->blendActive   = 0;
    work->animId        = ACTOR421600_INITIAL_CLIP;
    work->lookYaw       = 0;
    work->lookYawTarget = 0;
    work->baseRate      = ANIMATION_RATE_ONE;
    work->animRate      = ANIMATION_RATE_ONE;
    _desertChaserAnimTick(task);
    // Root sphere and capsule test the room grid; front/rear spheres take pair contacts.
    work->spheres[DESERT_CHASER_SPHERE_ROOT].body.context.contacts = work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts;
    work->spheres[DESERT_CHASER_SPHERE_ROOT].body.coord            = rootCoord;
    work->spheres[DESERT_CHASER_SPHERE_ROOT].body.pos.vx           = 0;
    work->spheres[DESERT_CHASER_SPHERE_ROOT].body.pos.vy           = -ACTOR421600_ROOT_SPHERE_HEIGHT;
    work->spheres[DESERT_CHASER_SPHERE_ROOT].body.pos.vz           = 0;
    work->spheres[DESERT_CHASER_SPHERE_ROOT].body.key              = ACTOR421600_ENEMY_BODY_KEY;
    work->spheres[DESERT_CHASER_SPHERE_ROOT].body.radius           = ACTOR421600_ROOT_SPHERE_RADIUS;
    work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->spheres[DESERT_CHASER_SPHERE_ROOT].body);
    work->wallProbe.shape.ends[0].vx                     = 0;
    work->wallProbe.shape.ends[0].vy                     = -ACTOR421600_PROBE_HEIGHT;
    work->wallProbe.shape.ends[0].vz                     = 0;
    work->wallProbe.shape.ends[1].vx                     = 0;
    work->wallProbe.shape.ends[1].vy                     = -ACTOR421600_PROBE_HEIGHT;
    work->wallProbe.shape.ends[1].vz                     = ACTOR421600_INITIAL_PROBE_REACH;
    work->wallProbe.shape.end0Radius                     = ACTOR421600_ROOT_SPHERE_RADIUS;
    work->wallProbe.shape.end1Radius                     = ACTOR421600_ROOT_SPHERE_RADIUS;
    work->wallProbe.shape.contacts                       = work->wallProbe.contacts;
    work->wallProbe.body.context.capsule                 = &work->wallProbe.shape;
    work->wallProbe.body.coord                           = rootCoord;
    work->wallProbe.body.pos.vx                          = 0;
    work->wallProbe.body.pos.vy                          = 0;
    work->wallProbe.body.pos.vz                          = 0;
    work->wallProbe.body.key                             = ACTOR421600_ENEMY_BODY_KEY;
    work->wallProbe.body.radius                          = 0;
    work->wallProbe.body.flags                           = WORLD_COLLISION_BODY_CAPSULE;
    work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->wallProbe.body);
    work->wallProbe.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    worldCollisionInitContacts(work->wallProbe.contacts, ARRAY_SIZE(work->wallProbe.contacts), 0);
    worldCollisionInitContacts(work->spheres[DESERT_CHASER_SPHERE_ROOT].body.context.contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts), 0);
    frontBody                   = &work->spheres[DESERT_CHASER_SPHERE_FRONT].body;
    frontBody->coord            = &task->extra.tmd->coords[2];
    frontBody->context.contacts = work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts;
    frontBody->pos.vx           = 0;
    frontBody->pos.vy           = 0;
    frontBody->pos.vz           = 0;
    frontBody->key              = ACTOR421600_ENEMY_BODY_KEY;
    frontBody->radius           = DESERT_CHASER_FRONT_RADIUS;
    frontBody->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, frontBody);
    frontBody->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(frontBody->context.contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts), 0);
    rearBody                   = &work->spheres[DESERT_CHASER_SPHERE_REAR].body;
    rearBody->coord            = &task->extra.tmd->coords[10];
    rearBody->context.contacts = work->spheres[DESERT_CHASER_SPHERE_REAR].contacts;
    rearBody->pos.vx           = 0;
    rearBody->pos.vy           = 0;
    rearBody->pos.vz           = 0;
    rearBody->key              = ACTOR421600_ENEMY_BODY_KEY;
    rearBody->radius           = ACTOR421600_REAR_SPHERE_RADIUS;
    rearBody->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, rearBody);
    rearBody->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(rearBody->context.contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_REAR].contacts), 0);
    work->spheres[DESERT_CHASER_SPHERE_REAR].body.pos.vx = 0;
    work->spheres[DESERT_CHASER_SPHERE_REAR].body.pos.vy = 0;
    work->spheres[DESERT_CHASER_SPHERE_REAR].body.pos.vz = ACTOR421600_REAR_SPHERE_OFFSET;
    // Seed the two patrol points at the root and 5000 units along its flattened facing.
    work->patrolTarget      = 0;
    work->patrolPoints[0].x = task->extra.tmd->coords->coord.t[0];
    work->patrolPoints[0].z = task->extra.tmd->coords->coord.t[2];
    gfxReadMatrixZAxis(&task->extra.tmd->coords->coord, &patrolDirection);
    patrolDirection.vy = 0;
    direction          = &patrolDirection;
    VectorNormalSS(direction, direction);
    gte_lddp(ACTOR421600_INITIAL_PATROL_DISTANCE);
    gte_ldsv(direction);
    gte_gpf12();
    gte_stsv(direction);
    work->patrolPoints[1].x               = task->extra.tmd->coords->coord.t[0] + patrolDirection.vx;
    work->patrolPoints[1].z               = task->extra.tmd->coords->coord.t[2] + patrolDirection.vz;
    work->playerAnim.blendFrames          = ACTOR421600_INITIAL_PLAYER_BLEND_FRAMES;
    work->playerAnim.source.sets          = NULL;
    work->playerAnim.animationId          = 1;
    work->playerAnim.blend                = ANIMATION_BLEND_RESET;
    work->playerAnim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
    task->msgTable                        = D_actor_421600_80151118;
    rootCoord->parent                     = &gGfxViewCoord;
    rootCoord->composeStamp               = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    lightingPosition.vx = rootCoord->workm.t[0];
    lightingPosition.vy = rootCoord->workm.t[1];
    lightingPosition.vz = rootCoord->workm.t[2];
    worldCoordUpdateActorColor(enemy, &lightingPosition, 0, 0);
    // Spawn mode chooses the initial state; the low nibble chooses the tuning.
    spawnMode = task->spawnArg1.value >> ACTOR421600_SPAWN_MODE_SHIFT;
    switch (spawnMode & ACTOR421600_SPAWN_ARGUMENT_MASK) {
        case ACTOR421600_SPAWN_MODE_HIDDEN:
            work->prevState = DESERT_CHASER_PREV_STATE_NONE;
            work->state     = DESERT_CHASER_STATE_HIDDEN;
            break;

        case ACTOR421600_SPAWN_MODE_STEER:
            work->prevState = DESERT_CHASER_PREV_STATE_NONE;
            work->state     = DESERT_CHASER_STATE_STEER;
            break;

        case 0:

        default:
            work->prevState = DESERT_CHASER_PREV_STATE_NONE;
            work->state     = DESERT_CHASER_STATE_PATROL;
            tmdAllocPrimitiveBuffer(model);
            break;
    }

    switch (task->spawnArg1.value & ACTOR421600_SPAWN_ARGUMENT_MASK) {
        case 2:
            work->windupFrames       = D_actor_421600_8013EF48[0].windupFrames;
            work->downFramesBase     = D_actor_421600_8013EF48[0].downFramesBase;
            work->roamLookDelay      = D_actor_421600_8013EF48[0].roamLookDelay;
            work->chaseHoldoffFrames = D_actor_421600_8013EF48[0].chaseHoldoffFrames;
            break;

        case 1:
            work->windupFrames       = D_actor_421600_8013EF48[2].windupFrames;
            work->downFramesBase     = D_actor_421600_8013EF48[2].downFramesBase;
            work->roamLookDelay      = D_actor_421600_8013EF48[2].roamLookDelay;
            work->chaseHoldoffFrames = D_actor_421600_8013EF48[2].chaseHoldoffFrames;
            break;

        case 0:

        default:
            work->windupFrames       = D_actor_421600_8013EF48[1].windupFrames;
            work->downFramesBase     = D_actor_421600_8013EF48[1].downFramesBase;
            work->roamLookDelay      = D_actor_421600_8013EF48[1].roamLookDelay;
            work->chaseHoldoffFrames = D_actor_421600_8013EF48[1].chaseHoldoffFrames;
            break;
    }

    gSceneCombatState.battleRefs = ACTOR421600_PACK_INITIAL_LIVES;
    D_actor_421600_80151268      = ACTOR421600_PACK_INITIAL_LIVES;
    task->state++;
}

#include "../../shared/desert_chaser_hit_effect.inc.c"

/// Returns the first attack contact key and copies its world-space hit position.
///
/// Reads a sphere's `DESERT_CHASER_CONTACTS` entries up to the first zero key.
/// Returns zero without changing `hitPosition` when no attack is found.
/// Only XYZ are copied; the vector's fourth halfword is untouched. Inputs
/// and writable output must remain live through the call.
static __inline__ s32 _actor421600FindAttackContact(const WorldCollisionContact* contacts,
                                                    SVECTOR*                     hitPosition)
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

/// Plays a hit EVT with this chaser's placement index and narrowed spatial audio.
///
/// The script word's placement byte is ORed with the enemy index; pan and
/// depth use the signed low byte of the root's audio queries. Inputs are
/// borrowed for this call and queries retain their order.
static inline void _actor421600PlayPlacedHitSound(Task* task, Enemy* enemy, s32 soundBase)
{
    s32 sound = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | soundBase;
    s32 pan   = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
}

/// Applies the Water Tower chaser's direct hits, reactions and damage over time.
///
/// Requires live enemy/work/model and an initialized scratch stack. An enemy
/// with nonpositive HP is skipped. Direct hits prefer the front sphere, set
/// the attack's cooldown and use player distance in root-parent units. Hit yaw
/// uses the composed-root frame, narrowed before wrapping in 4096-unit turns.
/// Critical and vulnerable multipliers precede an extra doubling of direct HP
/// loss. Direct defeat decrements the pack counter and broadcasts command 3;
/// damage-over-time defeat decrements without broadcasting. HP and recent
/// damage retain halfword wrapping. The caller gates this entire step by cooldown.
static void _desertChaserDamage(Task* task)
{
    enum {
        ACTOR421600_STATE_STAGGER                = 11,
        ACTOR421600_STATE_COLLAPSE               = 12,
        ACTOR421600_PACK_STAGE                   = 9,
        ACTOR421600_PACK_AREA                    = 1,
        ACTOR421600_PACK_COMMAND_MEMBER_DEFEATED = 3,
        ACTOR421600_SOUND_LIGHT_HIT              = 0x40010007,
        ACTOR421600_SOUND_HEAVY_HIT              = 0x40010008,
        ACTOR421600_REACTION_BODY_BURST          = 4,
        ACTOR421600_REACTION_KNOCK_DOWN          = 5,
        ACTOR421600_REACTION_BLEND_HIT_8         = 8,
        ACTOR421600_REACTION_BLEND_HIT_9         = 9,
        ACTOR421600_RISE_HIT_WINDOW_TICKS        = 10,
        ACTOR421600_KNOCK_DOWN_DAMAGE            = 71,
        ACTOR421600_CRITICAL_EFFECT_NONE         = -1,
        ACTOR421600_CRITICAL_EFFECT_ROLL         = 0,
        ACTOR421600_CRITICAL_EFFECT_VULNERABLE   = 3,
        ACTOR421600_FROZEN_HEALTH                = 100,
        ACTOR421600_CLIP_LIGHT_FLINCH            = 9,
        ACTOR421600_CLIP_POISON_FLINCH           = 18,
    };
    s32                        wrappedHitBearing;
    s32                        actorsFrozen;
    PlayerStatus*              playerStatus = &gPlayerStatus;
    s16                        criticalStyle;
    s16                        relativeHitYaw;
    s16                        hitOffsetZ;
    s32                        staggerState;
    s16                        vulnerableState;
    s16                        fatalHitState;
    s16                        fatalTickState;
    s16                        lightHitWakeState;
    s16                        lightHitState;
    s16                        knockDownState;
    s16                        buildupState;
    s16                        poisonWakeState;
    s16                        wrappedHitYaw;
    s16                        reactionState;
    GfxCoord*                  rootCoord;
    s32                        damageOverTime;
    s32                        playerDeltaXSquared;
    s32                        playerDeltaYSquared;
    s32                        hitBearing;
    s32                        doubledDamage;
    s32                        playerDeltaX;
    s32                        playerDeltaY;
    s32                        playerDeltaZ;
    s32                        playerDistance;
    SVECTOR*                   hitPosition;
    s32                        hitSoundBase;
    u16                        accumulatedDamage;
    u32                        reaction;
    DesertChaserWork*          work;
    Enemy*                     enemy;
    DesertChaserDamageScratch* scratch;
    enemy = task->spawnArg2.pointer;
    work  = task->work;
    if (enemy->hp > 0) {
        scratch = SCRATCH_STACK_RESERVE_BLOCK(DesertChaserDamageScratch);
        // Take at most one direct hit, preferring the front sphere.
        scratch->hitKey = _actor421600FindAttackContact(
            work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts, &scratch->hitPos);
        if (scratch->hitKey == 0) {
            hitPosition     = &scratch->hitPos;
            scratch->hitKey = _actor421600FindAttackContact(work->spheres[DESERT_CHASER_SPHERE_REAR].contacts, hitPosition);
        }
        if (scratch->hitKey != 0) {
            scratch->criticalEffect = ACTOR421600_CRITICAL_EFFECT_NONE;
            work->hitCooldown       = damageGetPlayerAttackHitCooldown(scratch->hitKey);
            reaction                = damageGetPlayerAttackReaction(scratch->hitKey) & 0xFFFF;
            switch (reaction) {
                case DAMAGE_PLAYER_REACTION_NONE:
                case DAMAGE_PLAYER_REACTION_EXPLOSION:
                case DAMAGE_PLAYER_REACTION_INCENDIARY:
                case ACTOR421600_REACTION_BLEND_HIT_8:
                case ACTOR421600_REACTION_BLEND_HIT_9:
                    lightHitWakeState = work->state;
                    if (lightHitWakeState == DESERT_CHASER_STATE_PATROL || lightHitWakeState == DESERT_CHASER_STATE_ROAM || lightHitWakeState == ACTOR421600_STATE_ROUTE_ROAM || lightHitWakeState == DESERT_CHASER_STATE_SCRIPT_ANIMATION) {
                        if (work->state == ACTOR421600_STATE_WATCH_RUN_PLAYER) {
                            work->state = ACTOR421600_STATE_ENTRANCE_LUNGE;
                        } else {
                            work->state = DESERT_CHASER_STATE_PURSUE;
                        }
                    }
                    if (work->state == DESERT_CHASER_STATE_STEER) {
                        work->state = DESERT_CHASER_STATE_RESUME_PURSUIT;
                    }
                    if (work->blendActive == 0) {
                        work->recentDamage = 0U;
                    }
                    lightHitState = work->state;
                    if ((lightHitState != DESERT_CHASER_STATE_RESUME_PURSUIT) && (lightHitState != ACTOR421600_STATE_KNOCK_DOWN) && (lightHitState != DESERT_CHASER_STATE_DOWNED) &&
                        (lightHitState != DESERT_CHASER_STATE_DEATH) && (lightHitState != ACTOR421600_STATE_WAIT_RESPAWN) && (lightHitState != DESERT_CHASER_STATE_STUNNED) &&
                        (lightHitState != ACTOR421600_STATE_STAGGER) && (lightHitState != DESERT_CHASER_STATE_RISE) && (lightHitState != ACTOR421600_STATE_ARENA_TRANSITION)) {
                        work->blendActive  = 1;
                        work->blendAnimId  = ACTOR421600_CLIP_LIGHT_FLINCH;
                        work->blendRequest = DESERT_CHASER_ANIM_REQUEST_RESET;
                    }
                    break;
                case ACTOR421600_REACTION_BODY_BURST:
                case ACTOR421600_REACTION_KNOCK_DOWN:
                    knockDownState = work->state;
                    if (knockDownState == DESERT_CHASER_STATE_STUNNED || knockDownState == ACTOR421600_STATE_STAGGER || knockDownState == ACTOR421600_STATE_KNOCK_DOWN || knockDownState == DESERT_CHASER_STATE_DOWNED) {
                        work->state     = ACTOR421600_STATE_STAGGER;
                        work->prevState = DESERT_CHASER_PREV_STATE_NONE;
                    } else if (knockDownState != DESERT_CHASER_STATE_DEATH && knockDownState != ACTOR421600_STATE_ARENA_TRANSITION) {
                        work->state = ACTOR421600_STATE_KNOCK_DOWN;
                    }
                    break;
                case DAMAGE_PLAYER_REACTION_BUILDUP:
                    buildupState = work->state;
                    if (buildupState == DESERT_CHASER_STATE_STEER || buildupState == DESERT_CHASER_STATE_STUNNED || buildupState == ACTOR421600_STATE_STAGGER || buildupState == DESERT_CHASER_STATE_DOWNED) {
                        work->state     = ACTOR421600_STATE_STAGGER;
                        work->prevState = DESERT_CHASER_PREV_STATE_NONE;
                    } else if (buildupState != DESERT_CHASER_STATE_DEATH && buildupState != ACTOR421600_STATE_ARENA_TRANSITION) {
                        work->state = ACTOR421600_STATE_KNOCK_DOWN;
                    }
                    damageStartEnemyBuildup(enemy, scratch->hitKey, 0);
                    break;
                case DAMAGE_PLAYER_REACTION_POISON:
                    poisonWakeState = work->state;
                    if ((poisonWakeState == DESERT_CHASER_STATE_PATROL) || (poisonWakeState == DESERT_CHASER_STATE_ROAM) || (poisonWakeState == DESERT_CHASER_STATE_SCRIPT_ANIMATION) ||
                        (poisonWakeState == ACTOR421600_STATE_WATCH_RUN_PLAYER)) {
                        work->state = DESERT_CHASER_STATE_PURSUE;
                    }
                    if (work->state == DESERT_CHASER_STATE_STEER) {
                        work->state = DESERT_CHASER_STATE_RESUME_PURSUIT;
                    }
                    damageTryStartEnemyDamageOverTime(enemy, scratch->hitKey, 0);
                    break;
                case DAMAGE_PLAYER_REACTION_STAGGER:
                    staggerState = work->state;
                    if (staggerState != ACTOR421600_STATE_ARENA_TRANSITION) {
                        if (staggerState == DESERT_CHASER_STATE_STUNNED || staggerState == ACTOR421600_STATE_STAGGER || staggerState == ACTOR421600_STATE_KNOCK_DOWN || staggerState == DESERT_CHASER_STATE_DOWNED ||
                            (staggerState == DESERT_CHASER_STATE_RISE && work->stateTimer < ACTOR421600_RISE_HIT_WINDOW_TICKS)) {
                            reactionState = ACTOR421600_STATE_STAGGER;
                            work->state   = reactionState;
                        } else if (staggerState != DESERT_CHASER_STATE_DEATH && staggerState != DESERT_CHASER_STATE_HIDDEN && staggerState != ACTOR421600_STATE_WAIT_RESPAWN &&
                                   staggerState != ACTOR421600_STATE_ARENA_TRANSITION) {
                            reactionState = ACTOR421600_STATE_KNOCK_DOWN;
                            work->state   = reactionState;
                        }
                    }
                    break;
            }
            playerDeltaX                          = playerStatus->coordMtx->t[0] - task->extra.tmd->coords->coord.t[0];
            playerDeltaXSquared                   = playerDeltaX * playerDeltaX;
            scratch->toPlayer.vx                  = playerDeltaX;
            playerDeltaY                          = playerStatus->coordMtx->t[1] - task->extra.tmd->coords->coord.t[1];
            playerDeltaYSquared                   = playerDeltaY * playerDeltaY;
            scratch->toPlayer.vy                  = playerDeltaY;
            playerDeltaZ                          = playerStatus->coordMtx->t[2] - task->extra.tmd->coords->coord.t[2];
            scratch->toPlayer.vz                  = playerDeltaZ;
            playerDistance                        = SquareRoot0(playerDeltaXSquared + playerDeltaYSquared + (playerDeltaZ * playerDeltaZ));
            scratch->playerDistance               = playerDistance;
            scratch->damage                       = damageComputePlayerAttack(scratch->hitKey, playerDistance, 0, 0);
            task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(task->extra.tmd->coords);
            // Preserve the original root-position stores before replacing them with hit offsets.
            scratch->hitOffset.vx = task->extra.tmd->coords->workm.t[0];
            scratch->hitOffset.vy = task->extra.tmd->coords->workm.t[1];
            scratch->hitOffset.vz = task->extra.tmd->coords->workm.t[2];
            scratch->hitOffset.vx = scratch->hitPos.vx - task->extra.tmd->coords->workm.t[0];
            scratch->hitOffset.vy = scratch->hitPos.vy - task->extra.tmd->coords->workm.t[1];
            hitOffsetZ            = scratch->hitPos.vz - task->extra.tmd->coords->workm.t[2];
            scratch->hitOffset.vz = hitOffsetZ;
            hitBearing            = ratan2(scratch->hitOffset.vx, hitOffsetZ);
            rootCoord             = task->extra.tmd->coords;
            relativeHitYaw =
                hitBearing - ratan2(-rootCoord->workm.m[2][0], rootCoord->workm.m[2][2]);
            scratch->hitYaw   = relativeHitYaw;
            wrappedHitYaw     = _actorAngleNormalizeYaw(relativeHitYaw);
            wrappedHitBearing = wrappedHitYaw;
            scratch->hitYaw   = wrappedHitBearing;
            _desertChaserHitEffect(task, wrappedHitBearing, scratch->hitKey);
            work->lookYaw       = 0;
            work->lookYawTarget = 0;
            if (damageRollCriticalHit(enemy, scratch->hitKey, 0) != 0) {
                scratch->criticalEffect = ACTOR421600_CRITICAL_EFFECT_ROLL;
                scratch->damage         = scratch->damage * 4;
            }
            vulnerableState = work->state;
            if ((vulnerableState == DESERT_CHASER_STATE_STUNNED) || (vulnerableState == ACTOR421600_STATE_STAGGER) || (vulnerableState == DESERT_CHASER_STATE_DOWNED) ||
                (vulnerableState == DESERT_CHASER_STATE_RISE)) {
                doubledDamage   = scratch->damage * 2;
                scratch->damage = doubledDamage;
                if (doubledDamage != 0) {
                    scratch->criticalEffect = ACTOR421600_CRITICAL_EFFECT_VULNERABLE;
                }
            }
            damageAccumulateLifeDrainHp(enemy, scratch->hitKey, scratch->damage, 0);
            criticalStyle = scratch->criticalEffect;
            if (criticalStyle != ACTOR421600_CRITICAL_EFFECT_NONE) {
                effectSpawn(EFFECT_CRITICAL_HIT, task->extra.tmd->coords + 2, (s32)criticalStyle, 0);
            }
            // Water Tower doubles direct HP loss after critical effects and Life Drain accounting.
            scratch->damage = scratch->damage * 2;
            enemy->hp       = (s16)((u16)enemy->hp - (u16)scratch->damage);
            worldTargetAddReadoutAmount(&enemy->node, scratch->damage, 0);
            accumulatedDamage  = work->recentDamage + (u16)scratch->damage;
            work->recentDamage = accumulatedDamage;
            if (enemy->hp <= 0) {
                D_actor_421600_80151268 -= 1;
                if ((damageGetPlayerAttackReaction(scratch->hitKey) & 0xFFFF) == ACTOR421600_REACTION_BODY_BURST) {
                    work->state = ACTOR421600_STATE_BURST_DEATH;
                } else {
                    fatalHitState = work->state;
                    if (fatalHitState == DESERT_CHASER_STATE_STEER || fatalHitState == DESERT_CHASER_STATE_DOWNED || fatalHitState == ACTOR421600_STATE_STAGGER ||
                        fatalHitState == DESERT_CHASER_STATE_STUNNED) {
                        work->state     = ACTOR421600_STATE_STAGGER;
                        work->prevState = DESERT_CHASER_PREV_STATE_NONE;
                    } else if (fatalHitState == ACTOR421600_STATE_ARENA_TRANSITION) {
                        work->state = DESERT_CHASER_STATE_DEATH;
                        _actor421600PlayPlacedHitSound(task, enemy, ACTOR421600_SOUND_HEAVY_HIT);
                    } else {
                        _actor421600PlayPlacedHitSound(task, enemy, ACTOR421600_SOUND_HEAVY_HIT);
                        work->state = ACTOR421600_STATE_KNOCK_DOWN;
                    }
                }
                work->broadcast.context.loc.stage = ACTOR421600_PACK_STAGE;
                work->broadcast.context.loc.area  = ACTOR421600_PACK_AREA;
                work->broadcast.command           = ACTOR421600_PACK_COMMAND_MEMBER_DEFEATED;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &work->broadcast, ACTOR_COMMAND_MESSAGE_APPLY);
            } else {
                if (((s16)accumulatedDamage >= ACTOR421600_KNOCK_DOWN_DAMAGE) && (work->state != DESERT_CHASER_STATE_STEER) && (work->state != ACTOR421600_STATE_KNOCK_DOWN) &&
                    (work->state != DESERT_CHASER_STATE_DOWNED) && (work->state != ACTOR421600_STATE_ARENA_TRANSITION) &&
                    (work->lastCommand.fields.command != DRYFIELD_WATER_TOWER_CHASER_COMMAND_START_RUN)) {
                    hitSoundBase = ACTOR421600_SOUND_HEAVY_HIT;
                    work->state  = ACTOR421600_STATE_KNOCK_DOWN;
                } else {
                    hitSoundBase = ACTOR421600_SOUND_LIGHT_HIT;
                }
                _actor421600PlayPlacedHitSound(task, enemy, hitSoundBase);
            }
            actorsFrozen = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen;
            if (actorsFrozen == 1) {
                enemy->hp          = ACTOR421600_FROZEN_HEALTH;
                work->blendAnimId  = ACTOR421600_CLIP_LIGHT_FLINCH;
                work->blendActive  = (s16)actorsFrozen;
                work->blendRequest = DESERT_CHASER_ANIM_REQUEST_RESET;
            }
        }
        // Damage over time advances even when no direct contact was found.
        if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            scratch->damage = damageTickEnemyDamageOverTime(enemy);
            if (damageIsEnemyDamageOverTimeExpired(enemy) != 0) {
                enemy->reactionFlags = (u8)(enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR);
            }
            enemy->hp      = (s16)((u16)enemy->hp - (u16)scratch->damage);
            damageOverTime = scratch->damage;
            if (damageOverTime != 0) {
                worldTargetAddReadoutAmount(&enemy->node, damageOverTime, 0);
                if (enemy->hp <= 0) {
                    D_actor_421600_80151268 -= 1;
                    fatalTickState           = work->state;
                    if ((fatalTickState != DESERT_CHASER_STATE_STUNNED) && (fatalTickState != ACTOR421600_STATE_STAGGER) &&
                        (fatalTickState != DESERT_CHASER_STATE_DOWNED)) {
                        work->state = ACTOR421600_STATE_COLLAPSE;
                    } else {
                        work->state = DESERT_CHASER_STATE_DEATH;
                    }
                } else {
                    if (work->state == DESERT_CHASER_STATE_PURSUE) {
                        work->state = DESERT_CHASER_STATE_ROAM;
                    }
                    work->blendActive  = 1;
                    work->blendAnimId  = ACTOR421600_CLIP_POISON_FLINCH;
                    work->blendRequest = DESERT_CHASER_ANIM_REQUEST_RESET;
                }
            }
        }
        SCRATCH_STACK_RELEASE_BLOCK(DesertChaserDamageScratch);
    }
}

/// Plays room-requested chaser clips and their script-specific transitions and dust.
///
/// Entry restores targeting, primitive buffers and root-grid collision at
/// normal animation rate. Clip 13 follows a control jump into clip 1; settled
/// clip 15 resets into clip 16. Clip 14 emits part-7 dust on cue records 8/9,
/// with a second puff on record 8. The state remains active until another
/// command or the frame driver releases it. Requires live work, enemy and model.
static void _actor421600ScriptAnimationState(Task* task)
{
    enum { ACTOR421600_SCRIPT_CLIP_BRANCH           = 13,
           ACTOR421600_SCRIPT_CLIP_AFTER_BRANCH     = 1,
           ACTOR421600_SCRIPT_CLIP_DUST             = 14,
           ACTOR421600_SCRIPT_CLIP_TRANSITION       = 15,
           ACTOR421600_SCRIPT_CLIP_AFTER_TRANSITION = 16 };
    SVECTOR           dustOffset;
    DesertChaserWork* work;
    TmdObject*        model;

    work = task->work;
    if (work->stateEntered != 0) {
        model                                                     = task->extra.tmd;
        ((Enemy*)task->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                              = 0;
        tmdAllocPrimitiveBuffer(model);
        work->animRate                                       = ANIMATION_RATE_ONE;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        if (work->animId == ACTOR421600_SCRIPT_CLIP_BRANCH) {
            work->animRequest = DESERT_CHASER_ANIM_REQUEST_BLEND;
        } else {
            work->animRequest = DESERT_CHASER_ANIM_REQUEST_RESET;
        }
        work->waistYawTarget = 0;
        work->lookYawTarget  = 0;
        _desertChaserAnimTick(task);
        return;
    }
    _desertChaserAnimTick(task);
    if ((work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP) && (work->animId == ACTOR421600_SCRIPT_CLIP_BRANCH)) {
        work->animId      = ACTOR421600_SCRIPT_CLIP_AFTER_BRANCH;
        work->animRequest = DESERT_CHASER_ANIM_REQUEST_BLEND;
    }
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        if (work->animId == ACTOR421600_SCRIPT_CLIP_TRANSITION) {
            work->animRequest = DESERT_CHASER_ANIM_REQUEST_RESET;
            work->animId      = ACTOR421600_SCRIPT_CLIP_AFTER_TRANSITION;
        }
        _desertChaserAnimTick(task);
    }
    if (work->animId == ACTOR421600_SCRIPT_CLIP_DUST) {
        if ((u32)((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) - 8) < 2U) {
            dustOffset.vz = 0;
            dustOffset.vx = 0;
            dustOffset.vy = 0x2BC;
            if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                effectSpawn(EFFECT_DUST_PUFF, task->extra.tmd->coords + 7, (DESERT_CHASER_CUE_DUST_RECURSIVE | (2 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 0x300), &dustOffset);
            }
        }
        if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 8) {
            dustOffset.vz = 0;
            dustOffset.vx = 0;
            dustOffset.vy = 0x2BC;
            if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                effectSpawn(EFFECT_DUST_PUFF, task->extra.tmd->coords + 7, (DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 0x400), &dustOffset);
            }
        }
    }
}

/// Returns the arena-zone value for a coordinate's local XZ position.
///
/// X buckets split at -3199, 0 and 3401; Z buckets descend through 3001, 0
/// and -2999. The resulting cell `xBucket | zBucket * 4` is always in 0..15,
/// and the package's table maps it to a zone in 0..15. Translation uses
/// whole units in the arena root's parent frame; no composition is performed.
static __inline__ s16 _actor421600GetArenaZone(const GfxCoord* coord)
{
    enum { ACTOR421600_ZONE_POSITIVE_X_SPLIT = 3401,
           ACTOR421600_ZONE_NEGATIVE_X_SPLIT = -3199,
           ACTOR421600_ZONE_POSITIVE_Z_SPLIT = 3001,
           ACTOR421600_ZONE_NEGATIVE_Z_SPLIT = -2999 };
    s32 x, z, xBucket, zBucket;
    x = coord->coord.t[0];
    z = coord->coord.t[2];
    if (x >= ACTOR421600_ZONE_POSITIVE_X_SPLIT)
        xBucket = 3;
    else if (x > 0)
        xBucket = 2;
    else
        xBucket = x >= ACTOR421600_ZONE_NEGATIVE_X_SPLIT;
    zBucket = 0;
    if (z < ACTOR421600_ZONE_POSITIVE_Z_SPLIT) {
        zBucket = 1;
        if (z <= 0) {
            zBucket = 3;
            if (z >= ACTOR421600_ZONE_NEGATIVE_Z_SPLIT)
                zBucket = 2;
        }
    }
    return D_actor_421600_801511C0[xBucket | (zBucket * 4)];
}

/// Walks the arena perimeter toward the player's zone until close enough to roam.
///
/// Entry selects walking at normal animation rate. Later ticks compare arena
/// zones and follow the adjacent waypoint with a maximum yaw step of 32/4096
/// turn and a 20-unit forward step while no reaction is blended. Grid and
/// body contacts correct movement, then the center exclusion is applied.
/// Waypoint lookup requires the adjacent index to lie in 0..12; inner-cell
/// zones 13..15 do not establish that bound. Requires live roots and scratch.
static void _actor421600TrackArenaRouteState(Task* task)
{
    enum { ACTOR421600_ROUTE_MAX_TURN = 32,
           ACTOR421600_ROUTE_STEP     = 20 };
    DesertChaserWork* work;
    ActorTurnScratch *savedCursor, *routeTurn;
    Enemy*            enemy;
    TmdObject*        model;
    s16               playerZone, actorZone;
    s16               nextZone;
    s16               waypointTurn;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->animRate                                       = ANIMATION_RATE_ONE;
        work->animId                                         = ACTOR421600_CLIP_ARENA_WALK;
        work->animRequest                                    = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _desertChaserAnimTick(task);
        return;
    }
    playerZone = _actor421600GetArenaZone(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords);
    actorZone  = _actor421600GetArenaZone(task->extra.tmd->coords);
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts));
    if (playerZone == actorZone) {
        work->state = DESERT_CHASER_STATE_ROAM;
        return;
    }
    switch ((s16)(playerZone - 1)) {
        case 0:
        case 1:
            if (actorZone >= 1 && actorZone <= 3) {
                work->state = DESERT_CHASER_STATE_ROAM;
                return;
            }
            break;
        case 2:
            if (actorZone >= 1 && actorZone <= 6) {
                work->state = DESERT_CHASER_STATE_ROAM;
                return;
            }
            break;
        case 3:
        case 4:
            if (actorZone >= 3 && actorZone <= 6) {
                work->state = DESERT_CHASER_STATE_ROAM;
                return;
            }
            break;
        case 5:
            if (actorZone >= 3 && actorZone <= 9) {
                work->state = DESERT_CHASER_STATE_ROAM;
                return;
            }
            break;
        case 6:
        case 7:
            if (actorZone >= 6 && actorZone <= 9) {
                work->state = DESERT_CHASER_STATE_ROAM;
                return;
            }
            break;
        case 8:
            if (actorZone >= 6 && actorZone <= 11) {
                work->state = DESERT_CHASER_STATE_ROAM;
                return;
            }
            break;
        case 9:
        case 10:
            if (actorZone >= 9 && actorZone <= 11) {
                work->state = DESERT_CHASER_STATE_ROAM;
                return;
            }
            break;
        case 11:
            if (actorZone >= 0xB) {
                work->state = DESERT_CHASER_STATE_ROAM;
                return;
            }
            if (actorZone >= 0xC) {
                work->state = DESERT_CHASER_STATE_ROAM;
                return;
            }
            break;
    }
    _desertChaserAnimTick(task);
    savedCursor = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    routeTurn = savedCursor - 1;
    if (actorZone > playerZone)
        nextZone = actorZone - 1;
    else
        nextZone = actorZone + 1;
    savedCursor[-1].delta.vx = D_actor_421600_80151158[nextZone].vx;
    routeTurn->delta.vy      = D_actor_421600_80151158[nextZone].vy;
    routeTurn->delta.vz      = D_actor_421600_80151158[nextZone].vz;
    routeTurn->delta.vx      = routeTurn->delta.vx - (u16)task->extra.tmd->coords->coord.t[0];
    routeTurn->delta.vy      = 0;
    routeTurn->delta.vz      = routeTurn->delta.vz - (u16)task->extra.tmd->coords->coord.t[2];
    waypointTurn             = _actorAngleTurnToOffset(task->extra.tmd->coords, routeTurn->delta.vx, routeTurn->delta.vz);
    routeTurn->angle         = waypointTurn;
    work->lookYawTarget      = waypointTurn;
    if (routeTurn->angle >= ACTOR421600_ROUTE_MAX_TURN + 1)
        routeTurn->angle = ACTOR421600_ROUTE_MAX_TURN;
    if (routeTurn->angle < -ACTOR421600_ROUTE_MAX_TURN)
        routeTurn->angle = -ACTOR421600_ROUTE_MAX_TURN;
    work->waistYawTarget = routeTurn->angle;
    routeTurn->angle     = routeTurn->angle + ratan2(-task->extra.tmd->coords->coord.m[2][0], task->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, routeTurn->angle, GRAPHICS_ROTATION_REPLACE);
    if (work->blendActive == 0) {
        _actorMovementStepForward(task->extra.tmd->coords, ACTOR421600_ROUTE_STEP);
    }
    _actorContactApplyAvoidancePushback(task->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts), &routeTurn->delta);
    _actor421600PushOutsideArenaCenter(task->extra.tmd->coords);
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Flattens and hides a dead chaser before it waits to respawn.
///
/// Entry disables root-grid collision and lock-on and resets death bookkeeping.
/// Frame 1 changes color modes and falls through to the translucency/black
/// setup also run at frame 20. From frame 11, Y scale loses 107/4096 per tick;
/// frame 38 hides the model and selects the respawn-wait state. X/Z scale
/// remains unity. Requires live chaser work, enemy, model and scratch stack.
static void _actor421600ShrinkDeathState(Task* task)
{
    enum { ACTOR421600_DEATH_TIMER_LIMIT  = 1025,
           ACTOR421600_SHRINK_START_FRAME = 11,
           ACTOR421600_SHRINK_SCALE_LOSS  = 107,
           ACTOR421600_DEATH_BLACK_FRAME  = 20,
           ACTOR421600_DEATH_NOOP_FRAME   = 22,
           ACTOR421600_DEATH_HIDE_FRAME   = 38 };
    DesertChaserWork* work;
    Enemy*            enemy;
    TmdObject*        model;
    s32               scaleLoss;
    u16               nextFrame;

    work  = task->work;
    model = task->extra.tmd;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model->flags                                         = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags                        = WORLD_TARGET_NOT_LOCKABLE;
        work->stateTimer                                     = 0;
        work->roomNotified                                   = 0;
    }
    if (work->stateTimer < ACTOR421600_DEATH_TIMER_LIMIT) {
        nextFrame        = work->stateTimer + 1;
        work->stateTimer = nextFrame;
        switch ((s16)nextFrame) {
            case 1:
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_DEFAULT);
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                /* fallthrough */
            case ACTOR421600_DEATH_BLACK_FRAME:
                task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                break;
            case ACTOR421600_DEATH_NOOP_FRAME:
                break;
            case ACTOR421600_DEATH_HIDE_FRAME:
                task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                work->state            = ACTOR421600_STATE_WAIT_RESPAWN;
                break;
        }
        // Rebuild from yaw each tick so Y scale replaces, rather than compounds, the old scale.
        if (work->stateTimer >= ACTOR421600_SHRINK_START_FRAME) {
            scaleLoss = (work->stateTimer - (ACTOR421600_SHRINK_START_FRAME - 1)) * ACTOR421600_SHRINK_SCALE_LOSS;
            if (scaleLoss < ONE) {
                _actorRenderRescaleYawY(task->extra.tmd->coords, ONE, ONE - scaleLoss);
            } else {
                _actorRenderRescaleYawY(task->extra.tmd->coords, ONE, 0);
            }
        }
    }
}

/// Restores health and default color at a final-battle entrance and heads for zone 5.
///
/// X/Z use game-coordinate units in the root's parent frame; yaw uses 4096
/// units per turn. Task, work and enemy must describe the same live chaser.
/// Y is preserved. Composition precedes the yaw replacement, so callers must
/// invalidate and recompose before consuming the new cached orientation.
static inline void _actor421600RestoreFinalBattleChaser(Task* task, DesertChaserWork* work, Enemy* enemy, s32 x, s32 z, s16 yaw)
{
    task->extra.tmd->coords->coord.t[0]   = x;
    task->extra.tmd->coords->coord.t[2]   = z;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, yaw, GRAPHICS_ROTATION_REPLACE);
    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_DEFAULT);
    enemy->reactionFlags = 0;
    enemy->hp            = D_actor_421600_8013EF38.hpMax;
    work->state          = ACTOR421600_STATE_MOVE_TO_ZONE5;
}

/// Settles a dead chaser's battle reference and returns it for the final battle.
///
/// A held player defers entry by forcing another state-entry tick. Otherwise
/// rewards are released only while at least two battle references remain.
/// An exhausted eight-life pack sends the room its completion event and hides.
/// Only START_FINAL_BATTLE keeps the wait active; the other placed chaser's
/// health and the remaining-life count decide whether this one can return.
/// Respawn restores health/color and enters the route toward arena zone 5.
/// Frame and room-event counters saturate/count down but do not gate respawn.
/// The task, scene, room/player slots and paired enemy records must be live.
static void _actor421600WaitToRespawnState(Task* task)
{
    enum { ACTOR421600_RESPAWN_TIMER_LIMIT = 128 };
    DesertChaserWork* work;
    Enemy*            enemy;
    Enemy*            otherChaser;
    s32               stageBits;
    s32               otherPlaceAreaBits;
    s32               firstPlaceKey;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        if (work->playerHeld == 1) {
            work->prevState  = DESERT_CHASER_PREV_STATE_NONE;
            work->stateTimer = 0;
            return;
        }
        work->stateTimer = 0;
        if (gSceneCombatState.battleRefs >= 2U) {
            sceneReleaseBattleRefWithRewards(task, 1);
        }
        if (D_actor_421600_80151268 <= 0) {
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, 0, 0);
            work->roomNotified = 1;
            work->state        = DESERT_CHASER_STATE_HIDDEN;
            return;
        }
    }
    if (work->stateTimer < ACTOR421600_RESPAWN_TIMER_LIMIT) {
        work->stateTimer = work->stateTimer + 1;
    }
    if (work->roomEventCountdown > 0) {
        work->roomEventCountdown = work->roomEventCountdown - 1;
    }
    if (work->lastCommand.fields.command != DRYFIELD_WATER_TOWER_CHASER_COMMAND_START_FINAL_BATTLE) {
        work->state = DESERT_CHASER_STATE_HIDDEN;
        return;
    }
    // Place keys pack stage above area, unlike ActorCommand context keys.
    otherChaser = NULL;
    switch (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) {
        case 0:
            stageBits          = gGameSession->location.loc.stage << ENEMY_PLACE_STAGE_SHIFT;
            otherPlaceAreaBits = gGameSession->location.loc.area | (1 << ENEMY_PLACE_INDEX_SHIFT);
            otherChaser        = sceneFindEnemyByPlaceKey(otherPlaceAreaBits | stageBits);
            break;
        case 1:
            firstPlaceKey = (gGameSession->location.loc.stage << ENEMY_PLACE_STAGE_SHIFT) | gGameSession->location.loc.area;
            otherChaser   = sceneFindEnemyByPlaceKey(firstPlaceKey);
            break;
    }
    // Keep the last remaining life on the live partner; otherwise replace this chaser.
    if (otherChaser != NULL) {
        if (otherChaser->hp > 0) {
            if (D_actor_421600_80151268 == 1) {
                work->state = DESERT_CHASER_STATE_HIDDEN;
            }
        }
        if ((D_actor_421600_80151268 >= 2) || ((otherChaser->hp <= 0) && (D_actor_421600_80151268 == 1))) {
            switch (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) {
                case 0:
                    _actor421600RestoreFinalBattleChaser(task, work, enemy, -0xD40, 0x104F, -0x76C);
                    break;
                case 1:
                    _actor421600RestoreFinalBattleChaser(task, work, enemy, 0x138C, 0x4B2, 0x7BC);
                    break;
            }
        }
    }
}

#include "../../shared/desert_chaser_approach.inc.c"

#include "../../shared/desert_chaser_pursue.inc.c"

#include "../../shared/desert_chaser_spawn_aim.inc.c"

#include "../../shared/desert_chaser_strike.inc.c"

/// Leaps backward after a catch, then resumes arena-route tracking.
///
/// Entry selects the backward clip and places the capsule probe 800 units
/// behind the root. Frames 9..24 retreat 200 root-parent units per tick, or
/// 85 while the capsule touches the grid, until five root-grid corrections
/// have been counted. A settled clip selects route tracking. Requires live
/// work, enemy, model and collision contacts.
static void _actor421600LeapBackState(Task* task)
{
    enum { ACTOR421600_LEAP_BACK_FIRST_TICK      = 9,
           ACTOR421600_LEAP_BACK_TICKS           = 16,
           ACTOR421600_LEAP_BACK_MAX_GRID_PUSHES = 5,
           ACTOR421600_LEAP_BACK_PROBE_DISTANCE  = 800,
           ACTOR421600_LEAP_BACK_STEP            = 200,
           ACTOR421600_LEAP_BACK_BLOCKED_STEP    = 85 };
    DesertChaserWork* work;
    Enemy*            enemy;
    TmdObject*        model;
    s16               wallContact;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy                         = task->spawnArg2.pointer;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = DESERT_CHASER_FRONT_RADIUS;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animRate                                        = ANIMATION_RATE_ONE;
        work->blendActive                                     = 0;
        work->animId                                          = ACTOR421600_CLIP_LEAP_BACK;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _desertChaserAnimTick(task);
        work->stateTimer                 = 0;
        work->stateCounter               = 0;
        work->wallProbe.shape.ends[1].vz = -ACTOR421600_LEAP_BACK_PROBE_DISTANCE;
    }
    work->stateTimer++;
    _desertChaserAnimTick(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        work->state = ACTOR421600_STATE_TRACK_ARENA_ROUTE;
    }
    if (((u32)((u16)work->stateTimer - ACTOR421600_LEAP_BACK_FIRST_TICK) < ACTOR421600_LEAP_BACK_TICKS) && (work->stateCounter < ACTOR421600_LEAP_BACK_MAX_GRID_PUSHES)) {
        if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts)) != 0) {
            work->stateCounter++;
        }
        wallContact = _desertChaserCapsuleTouchesGrid(task);
        if (wallContact != 0) {
            _actorMovementStepForward(task->extra.tmd->coords, -ACTOR421600_LEAP_BACK_BLOCKED_STEP);
        } else {
            _actorMovementStepForward(task->extra.tmd->coords, -ACTOR421600_LEAP_BACK_STEP);
        }
    } else {
        _actorContactApplyGridPushback(task->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts));
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Faces the player while waiting for the placed chaser's entrance camera view.
///
/// Entry selects the waiting clip; each tick applies root-grid correction,
/// tracks the player's narrowed XZ offset and points the root at that bearing.
/// Place 0 starts the entrance lunge in mapped view 3; place 1 does so in
/// view 8. Borrows live player/model roots and reserves two SVECTORs of
/// scratch storage, of which only the first vector's XYZ is accessed.
static void _actor421600WatchRunPlayerState(Task* task)
{
    DesertChaserWork* work;
    Enemy*            enemy;
    TmdObject*        model;
    SVECTOR*          savedCursor;
    SVECTOR*          toPlayer;
    GfxCoord*         positionCoord;
    GfxCoord*         headingCoord;
    s16               playerTurn;
    s32               mappedView;

    savedCursor = SCRATCH_STACK_CURSOR(SVECTOR);
    SCRATCH_STACK_RESERVE_BYTES(2 * sizeof(SVECTOR));
    toPlayer = savedCursor - 2;
    work     = task->work;
    enemy    = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = DESERT_CHASER_FRONT_RADIUS;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animRate                                        = ANIMATION_RATE_ONE;
        work->blendActive                                     = 0;
        work->animId                                          = ACTOR421600_CLIP_WATCH_RUN;
        work->waistYawTarget                                  = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _desertChaserAnimTick(task);
        work->stateTimer = 0;
    }
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts));
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    positionCoord                         = task->extra.tmd->coords;
    savedCursor[-2].vx                    = (u16)gPlayerStatus.coordMtx->t[0] - (u16)positionCoord->coord.t[0];
    toPlayer->vy                          = (u16)gPlayerStatus.coordMtx->t[1] - (u16)positionCoord->coord.t[1];
    toPlayer->vz                          = (u16)gPlayerStatus.coordMtx->t[2] - (u16)positionCoord->coord.t[2];
    headingCoord                          = task->extra.tmd->coords;
    playerTurn                            = ratan2(savedCursor[-2].vx, toPlayer->vz) - ratan2(-headingCoord->coord.m[2][0], headingCoord->coord.m[2][2]);
    playerTurn                            = _actorAngleNormalizeYaw(playerTurn);
    work->lookYawTarget                   = playerTurn;
    gfxRotMatrixY(&task->extra.tmd->coords->coord, (s16)ratan2(toPlayer->vx, toPlayer->vz), GRAPHICS_ROTATION_REPLACE);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _desertChaserAnimTick(task);
    if (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) == 0) {
        mappedView = viewGetMappedIndex() & 0xFF;
        if (mappedView == 3) {
            work->state = mappedView;
        }
    }
    if ((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) == 1) && ((viewGetMappedIndex() & 0xFF) == 8)) {
        work->state = ACTOR421600_STATE_ENTRANCE_LUNGE;
    }
    SCRATCH_STACK_RELEASE_BYTES(2 * sizeof(SVECTOR));
}

#include "../../shared/desert_chaser_steer.inc.c"

/// Pulls an outlying arena coordinate 150 units inside the crossed boundary.
///
/// The accepted X range is [-2850, 3050] and Z range [-2850, 2850], in
/// the root's parent-space units. Correcting X returns immediately, leaving
/// Z for a later call. Rotation and Y are untouched; the caller must mark
/// composition dirty after a correction.
static __inline__ void _actor421600ClampInsideArena(GfxCoord* coord)
{
    enum { ACTOR421600_ARENA_MIN             = -2850,
           ACTOR421600_ARENA_X_MAX_EXCLUSIVE = 3051,
           ACTOR421600_ARENA_Z_MAX_EXCLUSIVE = 2851,
           ACTOR421600_ARENA_MIN_INSET       = -2700,
           ACTOR421600_ARENA_X_MAX_INSET     = 2900,
           ACTOR421600_ARENA_Z_MAX_INSET     = 2700 };
    s32 x;
    s32 z;

    x = coord->coord.t[0];
    if (x > 0) {
        if (x >= ACTOR421600_ARENA_X_MAX_EXCLUSIVE) {
            coord->coord.t[0] = ACTOR421600_ARENA_X_MAX_INSET;
            return;
        }
    } else if (x < ACTOR421600_ARENA_MIN) {
        coord->coord.t[0] = ACTOR421600_ARENA_MIN_INSET;
        return;
    }
    z = coord->coord.t[2];
    if (z > 0) {
        if (z >= ACTOR421600_ARENA_Z_MAX_EXCLUSIVE) {
            coord->coord.t[2] = ACTOR421600_ARENA_Z_MAX_INSET;
        }
    } else if (z < ACTOR421600_ARENA_MIN) {
        coord->coord.t[2] = ACTOR421600_ARENA_MIN_INSET;
    }
}

/// Plays a wall-impact knockdown, charges 15 HP, and waits downed or starts death.
///
/// Entry plays the impact and hurt sounds, floors the HP charge at 1, and
/// enables the front sphere's grid test as well as the root's. The front grid
/// bit remains enabled in later states. Final-battle commands clamp the root
/// inside the arena; other commands apply the center exclusion. On clip end,
/// living chasers wait downed or stay stunned by buildup; dead ones shrink.
/// Requires live work, enemy, model, contacts and positional-audio queries.
static void _actor421600WallKnockDownState(Task* task)
{
    enum { ACTOR421600_WALL_KNOCK_DOWN_HP_COST = 15,
           ACTOR421600_SOUND_WALL_KNOCK_DOWN   = 0x40010009,
           ACTOR421600_SOUND_HURT              = 0x40010007 };
    DesertChaserWork* work;
    Enemy*            enemy;
    TmdObject*        model;
    s32               knockdownSound;
    s32               knockdownPan;
    s32               hurtSound;
    s32               hurtPan;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = DESERT_CHASER_FRONT_RADIUS;
        work->animRate                                        = ANIMATION_RATE_ONE;
        work->animId                                          = ACTOR421600_CLIP_WALL_KNOCK_DOWN;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->blendActive                                     = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _desertChaserAnimTick(task);
        knockdownSound = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR421600_SOUND_WALL_KNOCK_DOWN;
        knockdownPan   = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(knockdownSound, knockdownPan, (s32)(s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        // The impact costs health but cannot itself empty the pool.
        enemy->hp -= ACTOR421600_WALL_KNOCK_DOWN_HP_COST;
        worldTargetAddReadoutAmount(&enemy->node, ACTOR421600_WALL_KNOCK_DOWN_HP_COST, 0);
        if (enemy->hp <= 0) {
            enemy->hp = 1;
        }
        hurtSound = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR421600_SOUND_HURT;
        hurtPan   = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(hurtSound, hurtPan,
                                 (s32)(s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts));
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts));
    if (work->lastCommand.fields.command == DRYFIELD_WATER_TOWER_CHASER_COMMAND_START_FINAL_BATTLE) {
        _actor421600ClampInsideArena(task->extra.tmd->coords);
    } else {
        _actor421600PushOutsideArenaCenter(task->extra.tmd->coords);
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _desertChaserAnimTick(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        if (enemy->hp > 0) {
            if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
                work->state = DESERT_CHASER_STATE_STUNNED;
            } else {
                work->state = DESERT_CHASER_STATE_DOWNED;
            }
        } else {
            work->state = DESERT_CHASER_STATE_DEATH;
        }
    }
}

#include "../../shared/desert_chaser_roam.inc.c"

/// Waits downFramesBase plus 0..15 random ticks before a command-selected recovery.
///
/// Entry advances the shared LCG once. The unsigned timer decrements and its
/// signed halfword view expires below zero; animation continues during the wait.
/// START_RUN and END_RUN flee. START_FINAL_BATTLE rises in zone 11 or above,
/// otherwise routes toward zone 5; other commands rise. Requires live work/model.
static void _actor421600DownedWaitState(Task* task)
{
    DesertChaserWork* work;
    u32               randomState;
    u8                roomCommand;

    work = task->work;
    if (work->stateEntered != 0) {
        randomState      = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState  = randomState;
        work->stateTimer = work->downFramesBase + ((randomState >> 0x10) & 0xF);
    }
    work->stateTimer -= 1;
    _desertChaserAnimTick(task);
    if ((s16)work->stateTimer < 0) {
        roomCommand = work->lastCommand.fields.command;
        if ((roomCommand == DRYFIELD_WATER_TOWER_CHASER_COMMAND_START_RUN) || (roomCommand == DRYFIELD_WATER_TOWER_CHASER_COMMAND_END_RUN)) {
            work->state = DESERT_CHASER_STATE_FLEE;
        } else if (roomCommand == DRYFIELD_WATER_TOWER_CHASER_COMMAND_START_FINAL_BATTLE) {
            if (_actor421600GetArenaZone(task->extra.tmd->coords) >= 0xB) {
                work->state = DESERT_CHASER_STATE_RISE;
            } else {
                work->state = ACTOR421600_STATE_MOVE_TO_ZONE5;
            }
        } else {
            work->state = DESERT_CHASER_STATE_RISE;
        }
    }
}

/// Lunges from the camera-triggered entrance and catches a contacted player.
///
/// Entry engages battle and starts the lunge clip. Later ticks move 200
/// root-parent units, choose front/rear catch animations from an unwrapped
/// absolute heading difference, and select attacks 2/3 before 1000 accumulated
/// units or 0/1 thereafter. Damage replies select player death behavior.
/// Requires live work, enemy, player/model roots and a scratch lunge block.
/// The damage reply is only initialized while enemy HP is positive; the
/// retained dead-enemy path reads the previous scratch contents.
static void _actor421600EntranceLungeState(Task* task)
{
    enum { ACTOR421600_LUNGE_STEP              = 200,
           ACTOR421600_CLOSE_CATCH_DISTANCE    = 1000,
           ACTOR421600_CATCH_BUTTON_PRESSES    = 8,
           ACTOR421600_LUNGE_GRID_FLEE_TICK    = 11,
           ACTOR421600_LUNGE_ABANDON_TURN      = 1536,
           ACTOR421600_PLAYER_DEATH_STATE      = 10,
           ACTOR421600_ATTACK_THROW_FRONT      = 0,
           ACTOR421600_ATTACK_THROW_REAR       = 1,
           ACTOR421600_ATTACK_CLOSE_FRONT      = 2,
           ACTOR421600_ATTACK_CLOSE_REAR       = 3,
           ACTOR421600_PLAYER_CLIP_THROW       = 1,
           ACTOR421600_PLAYER_CLIP_CLOSE_CATCH = 3,
           ACTOR421600_SOUND_ENTRANCE_LUNGE    = 0x40010006 };
    SVECTOR                   dustOffset;
    s32                       rememberedBearing;
    s16                       rememberedTurn;
    s32                       playerFacingX;
    s32                       catchBearing;
    s16                       catchTurn;
    s16                       catchOffsetZ;
    s16                       reverseBearing;
    s16                       nextState;
    GfxCoord*                 catchPositionCoord;
    GfxCoord*                 rememberedHeadingCoord;
    GfxCoord*                 catchHeadingCoord;
    GfxCoord*                 stepCoord;
    GfxCoord*                 zoneCoord;
    s32                       lungeSound;
    s32                       emitDust;
    s32                       dustFlags;
    s32                       dustPart;
    s32                       freeTurnMagnitude;
    s32                       catchYawDifference;
    s32                       closeCatchYawDifference;
    s32                       farCatchYawDifference;
    s32                       lungePan;
    TmdObject*                model;
    Enemy*                    entryEnemy;
    Task*                     player;
    DesertChaserWork*         work;
    Enemy*                    enemy;
    _Actor421600LungeScratch* scratch;

    /// Starts a catch clip and a stopped collision-enabled scripted move.
    ///
    /// Work/player arguments must be live, side-effect-free pointer expressions:
    /// each is evaluated repeatedly. `clipId` is evaluated once. Invoke within
    /// an already braced statement block. Dispatch consumes the borrowed
    /// animation request synchronously; no value is returned.
#define ACTOR421600_BEGIN_PLAYER_CATCH(chaserWork, caughtPlayer, clipId)              \
    (chaserWork)->playerAnim.animationId         = (clipId);                          \
    (chaserWork)->playerAnim.blend               = ANIMATION_BLEND_RESET;             \
    (chaserWork)->playerAnim.blendFrames         = 0;                                 \
    (chaserWork)->playerMove.displacement.vx     = 0;                                 \
    (chaserWork)->playerMove.displacement.vy     = 0;                                 \
    (chaserWork)->playerMove.displacement.vz     = 0;                                 \
    (chaserWork)->playerMove.collisionRequests   = GAME_ACTOR_COLLISION_REQUEST_MASK; \
    (chaserWork)->playerMove.keepControl         = 1;                                 \
    (chaserWork)->playerHeld                     = 1;                                 \
    (chaserWork)->lastCommand.fields.catchFrames = 0;                                 \
    TASK_MESSAGE_DISPATCH_POINTER((caughtPlayer), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &(chaserWork)->playerAnim, 0)

    work   = task->work;
    enemy  = task->spawnArg2.pointer;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (work->stateEntered != 0) {
        model                              = task->extra.tmd;
        entryEnemy                         = task->spawnArg2.pointer;
        entryEnemy->node.state.parts.flags = 0;
        sceneEngageBattle(1);
        model->flags = 0;
        tmdAllocPrimitiveBuffer(model);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = DESERT_CHASER_FRONT_RADIUS;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animRate                                        = ANIMATION_RATE_ONE;
        work->blendActive                                     = 0;
        work->animId                                          = ACTOR421600_CLIP_LUNGE;
        work->waistYawTarget                                  = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags   = (u16)(work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        _desertChaserAnimTick(task);
        work->stateTimer    = 0;
        work->stateCounter  = 0;
        work->lungeDistance = 0;
        lungeSound          = (((u16)entryEnemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR421600_SOUND_ENTRANCE_LUNGE;
        lungePan            = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(lungeSound, lungePan, (s32)(s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        return;
    }
    // The entry returns above; later ticks alone reserve catch workspace.
    scratch       = SCRATCH_STACK_RESERVE_BLOCK(_Actor421600LungeScratch);
    zoneCoord     = task->extra.tmd->coords;
    scratch->zone = _actor421600GetArenaZone(zoneCoord);
    if ((_actorContactApplyGridPushback(task->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts)) != 0) && (work->stateTimer >= ACTOR421600_LUNGE_GRID_FLEE_TICK)) {
        work->state = DESERT_CHASER_STATE_FLEE;
    }
    if (((s16)_actorContactApplyAvoidancePushback(task->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts), &scratch->offset)) != 0 && work->animId == ACTOR421600_CLIP_LUNGE) {
        work->playerButtonHold.pressCount = ACTOR421600_CATCH_BUTTON_PRESSES;
        if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &work->playerButtonHold, 0) == 0) {
            playerFacingX          = -gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords->coord.m[2][0];
            scratch->playerYaw     = ratan2(playerFacingX, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords->coord.m[2][2]);
            catchPositionCoord     = task->extra.tmd->coords;
            scratch->offset.vx     = (s16)(gPlayerStatus.coordMtx->t[0] - catchPositionCoord->coord.t[0]);
            scratch->offset.vy     = (s16)(gPlayerStatus.coordMtx->t[1] - catchPositionCoord->coord.t[1]);
            catchOffsetZ           = gPlayerStatus.coordMtx->t[2] - catchPositionCoord->coord.t[2];
            scratch->offset.vz     = catchOffsetZ;
            reverseBearing         = ratan2(scratch->offset.vx, catchOffsetZ) + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
            scratch->yawFromPlayer = reverseBearing;
            scratch->yawFromPlayer = _actorAngleNormalizeYaw(reverseBearing);
            catchHeadingCoord      = task->extra.tmd->coords;
            catchBearing           = ratan2(scratch->offset.vx, scratch->offset.vz);
            catchTurn              = catchBearing - ratan2(-catchHeadingCoord->coord.m[2][0], catchHeadingCoord->coord.m[2][2]);
            scratch->turn          = _actorAngleNormalizeYaw(catchTurn);
            catchYawDifference     = scratch->yawFromPlayer - scratch->playerYaw;
            catchYawDifference     = abs(catchYawDifference);
            if (catchYawDifference < (ACTOR_TRANSFORM_ANGLE_TURN / 4)) {
                work->playerAnim.source.sets = gDesertChaserFrontAnim;
            } else {
                work->playerAnim.source.sets = gDesertChaserRearAnim;
                scratch->yawFromPlayer       = (s16)((u16)scratch->yawFromPlayer + ACTOR_TRANSFORM_ANGLE_HALF_TURN);
            }
            work->playerPlacement.rot.vx = 0;
            work->playerPlacement.rot.vy = (u16)scratch->yawFromPlayer;
            work->playerPlacement.rot.vz = 0;
            work->playerPlacement.pos.vx = (s32)player->extra.tmd->coords->coord.t[0];
            work->playerPlacement.pos.vy = (s32)player->extra.tmd->coords->coord.t[1];
            work->playerPlacement.pos.vz = (s32)player->extra.tmd->coords->coord.t[2];
            TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_PLACE, &work->playerPlacement, 0);
            if (work->lungeDistance < ACTOR421600_CLOSE_CATCH_DISTANCE) {
                if (enemy->hp > 0) {
                    closeCatchYawDifference = scratch->yawFromPlayer - scratch->playerYaw;
                    closeCatchYawDifference = abs(closeCatchYawDifference);
                    if (closeCatchYawDifference < (ACTOR_TRANSFORM_ANGLE_TURN / 4)) {
                        scratch->playerKilled = _damageApplyEnemyAttackToPlayer(enemy, ACTOR421600_ATTACK_CLOSE_FRONT);
                    } else {
                        scratch->playerKilled = _damageApplyEnemyAttackToPlayer(enemy, ACTOR421600_ATTACK_CLOSE_REAR);
                    }
                }
                if (scratch->playerKilled != 1) {
                    ACTOR421600_BEGIN_PLAYER_CATCH(work, player, ACTOR421600_PLAYER_CLIP_CLOSE_CATCH);
                }
                nextState = ACTOR421600_STATE_CLOSE_CATCH;
            } else {
                if (enemy->hp > 0) {
                    farCatchYawDifference = scratch->yawFromPlayer - scratch->playerYaw;
                    farCatchYawDifference = abs(farCatchYawDifference);
                    if (farCatchYawDifference < (ACTOR_TRANSFORM_ANGLE_TURN / 4)) {
                        scratch->playerKilled = _damageApplyEnemyAttackToPlayer(enemy, ACTOR421600_ATTACK_THROW_FRONT);
                    } else {
                        scratch->playerKilled = _damageApplyEnemyAttackToPlayer(enemy, ACTOR421600_ATTACK_THROW_REAR);
                    }
                }
                if (scratch->playerKilled == 1) {
                    ((GameActor*)player->work)->state = ACTOR421600_PLAYER_DEATH_STATE;
                }
                ACTOR421600_BEGIN_PLAYER_CATCH(work, player, ACTOR421600_PLAYER_CLIP_THROW);
                nextState = ACTOR421600_STATE_THROW_PLAYER;
            }
            work->state = nextState;
        }
        scratch->playerBearing = _actorAngleTurnToPlayer(task, &scratch->offset, &gPlayerStatus);
    } else {
        freeTurnMagnitude      = _actorAngleTurnToPlayer(task, &scratch->offset, &gPlayerStatus);
        scratch->playerBearing = (s16)freeTurnMagnitude;
        freeTurnMagnitude      = abs(freeTurnMagnitude);
        if (freeTurnMagnitude >= ACTOR421600_LUNGE_ABANDON_TURN + 1) {
            work->state = ACTOR421600_STATE_LUNGE_RECOVERY;
        }
    }
    // Keep moving after a catch or a requested state change on this tick.
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    rememberedHeadingCoord                = task->extra.tmd->coords;
    rememberedBearing                     = ratan2(work->playerDelta.vx, work->playerDelta.vz);
    rememberedTurn                        = rememberedBearing - ratan2(-rememberedHeadingCoord->coord.m[2][0], rememberedHeadingCoord->coord.m[2][2]);
    scratch->turn                         = _actorAngleNormalizeYaw(rememberedTurn);
    _desertChaserAnimTick(task);
    stepCoord = task->extra.tmd->coords;
    _actorMovementStepForward(stepCoord, ACTOR421600_LUNGE_STEP);
    work->lungeDistance = (s16)((u16)work->lungeDistance + ACTOR421600_LUNGE_STEP);
    if (work->animId == ACTOR421600_CLIP_LUNGE) {
        switch (work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) {
            case 5:
                emitDust      = 1;
                dustPart      = 7;
                dustFlags     = (4 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 0x300;
                dustOffset.vz = 0;
                dustOffset.vx = 0;
                dustOffset.vy = 700;
                break;
            case 8:
                emitDust      = 1;
                dustPart      = 9;
                dustFlags     = (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 0x500;
                dustOffset.vz = 0;
                dustOffset.vx = 0;
                dustOffset.vy = 700;
                break;
            case 10:
                emitDust      = 1;
                dustPart      = 14;
                dustFlags     = (5 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 0xA00;
                dustOffset.vz = 0;
                dustOffset.vx = 0;
                dustOffset.vy = 600;
                break;
            case 13:
                emitDust      = 1;
                dustPart      = 17;
                dustFlags     = (4 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 0x800;
                dustOffset.vz = 0;
                dustOffset.vx = 0;
                dustOffset.vy = 600;
                break;
            default:
                dustFlags = 0;
                emitDust  = 0;
                dustPart  = 0;
                break;
        }
        if ((gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) && (emitDust == 1)) {
            effectSpawn(EFFECT_DUST_PUFF, task->extra.tmd->coords + dustPart, dustFlags | DESERT_CHASER_CUE_DUST_RECURSIVE, &dustOffset);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor421600LungeScratch);
#undef ACTOR421600_BEGIN_PLAYER_CATCH
}

/// Runs along adjacent arena waypoints to the end farthest from the player's zone.
///
/// Entry chooses destination zone 1 for player zones 7..15, otherwise zone 11.
/// Arrival hides the chaser. Each moving tick turns at most 128/4096 turn and
/// steps 200 root-parent units while no reaction is blended, then applies
/// body avoidance. The adjacent waypoint index must lie in 0..12; inner-cell
/// zones 13..15 do not establish that bound. Requires live roots and scratch.
static void _actor421600FleeState(Task* task)
{
    enum { ACTOR421600_ROUTE_MAX_TURN = 128,
           ACTOR421600_ROUTE_STEP     = 200 };
    DesertChaserWork* work;
    ActorTurnScratch* savedCursor;
    ActorTurnScratch* routeTurn;
    Enemy*            enemy;
    TmdObject*        model;
    s32               actorZone;
    Task*             player;
    s16               waypointTurn;

    work   = task->work;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy                         = task->spawnArg2.pointer;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->animRate                                       = ANIMATION_RATE_ONE;
        work->animId                                         = ACTOR421600_CLIP_LUNGE;
        work->animRequest                                    = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _desertChaserAnimTick(task);
        if (_actor421600GetArenaZone(player->extra.tmd->coords) >= 7) {
            work->fleeZone = 1;
            return;
        }
        work->fleeZone = 0xB;
        return;
    }
    actorZone = _actor421600GetArenaZone(task->extra.tmd->coords);
    if ((s16)actorZone == work->fleeZone) {
        work->state = DESERT_CHASER_STATE_HIDDEN;
        return;
    }
    savedCursor = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    routeTurn = savedCursor - 1;
    if ((s16)actorZone > work->fleeZone) {
        savedCursor[-1].delta.vx = D_actor_421600_80151158[actorZone - 1].vx;
        routeTurn->delta.vy      = D_actor_421600_80151158[actorZone - 1].vy;
        routeTurn->delta.vz      = D_actor_421600_80151158[actorZone - 1].vz;
    } else {
        savedCursor[-1].delta.vx = D_actor_421600_80151158[actorZone + 1].vx;
        routeTurn->delta.vy      = D_actor_421600_80151158[actorZone + 1].vy;
        routeTurn->delta.vz      = D_actor_421600_80151158[actorZone + 1].vz;
    }
    routeTurn->delta.vx = routeTurn->delta.vx - (u16)task->extra.tmd->coords->coord.t[0];
    routeTurn->delta.vy = 0;
    routeTurn->delta.vz = routeTurn->delta.vz - (u16)task->extra.tmd->coords->coord.t[2];
    _desertChaserAnimTick(task);
    waypointTurn        = _actorAngleTurnToOffset(task->extra.tmd->coords, routeTurn->delta.vx, routeTurn->delta.vz);
    routeTurn->angle    = waypointTurn;
    work->lookYawTarget = waypointTurn;
    if (routeTurn->angle >= ACTOR421600_ROUTE_MAX_TURN + 1) {
        routeTurn->angle = ACTOR421600_ROUTE_MAX_TURN;
    }
    if (routeTurn->angle < -ACTOR421600_ROUTE_MAX_TURN) {
        routeTurn->angle = -ACTOR421600_ROUTE_MAX_TURN;
    }
    work->waistYawTarget = routeTurn->angle;
    routeTurn->angle     = routeTurn->angle + ratan2(-task->extra.tmd->coords->coord.m[2][0], task->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, routeTurn->angle, GRAPHICS_ROTATION_REPLACE);
    if (work->blendActive == 0) {
        _actorMovementStepForward(task->extra.tmd->coords, ACTOR421600_ROUTE_STEP);
    }
    _actorContactApplyAvoidancePushback(task->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts), &routeTurn->delta);
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Runs the final-battle entrance route to arena zone 5 without accepting lock-on.
///
/// Arrival starts the arena-transition animation. Other ticks follow adjacent
/// waypoints with a maximum yaw step of 128/4096 turn and a 200-unit forward
/// step while no reaction is blended. The adjacent waypoint index must lie
/// in 0..12; inner-cell zones 13..15 do not establish that bound. Requires
/// live work, enemy/model roots, body contacts and scratch storage.
static void _actor421600MoveToZone5State(Task* task)
{
    enum { ACTOR421600_ROUTE_MAX_TURN = 128,
           ACTOR421600_ROUTE_STEP     = 200 };
    DesertChaserWork* work;
    ActorTurnScratch* savedCursor;
    ActorTurnScratch* routeTurn;
    Enemy*            enemy;
    TmdObject*        model;
    GfxCoord*         rotationCoord;
    GfxCoord*         stepCoord;
    s32               actorZone;
    s8                destinationZone = 5;
    s16               waypointTurn;
    s32               wrappedTurn;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy                         = task->spawnArg2.pointer;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->animRate                                       = ANIMATION_RATE_ONE;
        work->animId                                         = ACTOR421600_CLIP_LUNGE;
        work->animRequest                                    = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _desertChaserAnimTick(task);
        return;
    }
    actorZone = _actor421600GetArenaZone(task->extra.tmd->coords);
    if ((s8)actorZone == destinationZone) {
        work->state = ACTOR421600_STATE_ARENA_TRANSITION;
        return;
    }
    savedCursor = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    routeTurn = savedCursor - 1;
    if ((s8)actorZone > destinationZone) {
        savedCursor[-1].delta.vx = D_actor_421600_80151158[actorZone - 1].vx;
        routeTurn->delta.vy      = D_actor_421600_80151158[actorZone - 1].vy;
        routeTurn->delta.vz      = D_actor_421600_80151158[actorZone - 1].vz;
    } else {
        savedCursor[-1].delta.vx = D_actor_421600_80151158[actorZone + 1].vx;
        routeTurn->delta.vy      = D_actor_421600_80151158[actorZone + 1].vy;
        routeTurn->delta.vz      = D_actor_421600_80151158[actorZone + 1].vz;
    }
    routeTurn->delta.vx = routeTurn->delta.vx - (u16)task->extra.tmd->coords->coord.t[0];
    routeTurn->delta.vy = 0;
    routeTurn->delta.vz = routeTurn->delta.vz - (u16)task->extra.tmd->coords->coord.t[2];
    _desertChaserAnimTick(task);
    waypointTurn        = _actorAngleTurnToOffset(task->extra.tmd->coords, routeTurn->delta.vx, routeTurn->delta.vz);
    wrappedTurn         = waypointTurn;
    routeTurn->angle    = wrappedTurn;
    work->lookYawTarget = wrappedTurn;
    if (routeTurn->angle >= ACTOR421600_ROUTE_MAX_TURN + 1) {
        routeTurn->angle = ACTOR421600_ROUTE_MAX_TURN;
    }
    if (routeTurn->angle < -ACTOR421600_ROUTE_MAX_TURN) {
        routeTurn->angle = -ACTOR421600_ROUTE_MAX_TURN;
    }
    work->waistYawTarget = routeTurn->angle;
    rotationCoord        = task->extra.tmd->coords;
    routeTurn->angle     = routeTurn->angle + ratan2(-rotationCoord->coord.m[2][0], rotationCoord->coord.m[2][2]);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, routeTurn->angle, GRAPHICS_ROTATION_REPLACE);
    if (work->blendActive == 0) {
        stepCoord = task->extra.tmd->coords;
        _actorMovementStepForward(stepCoord, ACTOR421600_ROUTE_STEP);
    }
    _actorContactApplyAvoidancePushback(task->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts), &routeTurn->delta);
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Plays the arena transition at the origin, then places the chaser on its patrol route.
///
/// Entry resets clip 17, root XYZ and yaw. A settled clip places XYZ at
/// (-820, 0, -1220), faces +X, resets the walking clip and enters route roam.
/// Entry ticks animation twice; completion ticks the reset walking clip twice
/// more. Coordinates use root-parent units and yaw uses 4096 units per turn.
/// Requires live work, enemy and model.
static void _actor421600ArenaTransitionState(Task* task)
{
    DesertChaserWork* work;
    TmdObject*        model;

    work = task->work;
    if (work->stateEntered != 0) {
        model                                                     = task->extra.tmd;
        ((Enemy*)task->spawnArg2.pointer)->node.state.parts.flags = 0;
        model->flags                                              = 0;
        tmdAllocPrimitiveBuffer(model);
        work->animRate                                       = ANIMATION_RATE_ONE;
        work->animId                                         = ACTOR421600_CLIP_ARENA_TRANSITION;
        work->animRequest                                    = DESERT_CHASER_ANIM_REQUEST_RESET;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        task->extra.tmd->coords->coord.t[0]                  = 0;
        task->extra.tmd->coords->coord.t[1]                  = 0;
        task->extra.tmd->coords->coord.t[2]                  = 0;
        task->extra.tmd->coords->composeStamp                = GRAPHICS_COORD_DIRTY;
        gfxRotMatrixY(&task->extra.tmd->coords->coord, 0, GRAPHICS_ROTATION_REPLACE);
        _desertChaserAnimTick(task);
    }
    _desertChaserAnimTick(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        task->extra.tmd->coords->coord.t[0]   = -0x334;
        task->extra.tmd->coords->coord.t[1]   = 0;
        task->extra.tmd->coords->coord.t[2]   = -0x4C4;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        gfxRotMatrixY(&task->extra.tmd->coords->coord, 0x400, GRAPHICS_ROTATION_REPLACE);
        work->animRequest = DESERT_CHASER_ANIM_REQUEST_RESET;
        work->animId      = ACTOR421600_CLIP_ARENA_WALK;
        _desertChaserAnimTick(task);
        _desertChaserAnimTick(task);
        work->state = ACTOR421600_STATE_ROUTE_ROAM;
    }
}

/// Returns the patrol-route quadrant for an arena XZ position.
///
/// The route-table row is 0 for +X/+Z, 1 for nonpositive X/+Z, 2 for
/// +X/nonpositive Z and 3 for nonpositive X/Z. Zero belongs to the
/// nonpositive side of each axis; the four-entry lookup is always in bounds.
static __inline__ s32 _actor421600GetRouteQuadrant(s32 x, s32 z)
{
    s32 positiveX    = x > 0;
    s32 nonPositiveZ = z < 1;
    return D_actor_421600_801511D0[positiveX + (nonPositiveZ * 2)];
}

/// Loads both patrol points for one quadrant and this enemy's placement class.
///
/// `scratch->playerQuadrant` must be 0..3. Placement zero uses columns 0..3; every other
/// index uses columns 4..7 of the route table. Source words narrow to the
/// patrol points' signed halfwords in root-parent coordinate units.
static inline void _actor421600SelectPatrolRoute(DesertChaserWork* work, Enemy* enemy, _Actor421600RouteRoamScratch* scratch)
{
    if ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) == 0) {
        work->patrolPoints[0].x = D_actor_421600_801511D4[scratch->playerQuadrant][0];
        work->patrolPoints[0].z = D_actor_421600_801511D4[scratch->playerQuadrant][1];
        work->patrolPoints[1].x = D_actor_421600_801511D4[scratch->playerQuadrant][2];
        work->patrolPoints[1].z = D_actor_421600_801511D4[scratch->playerQuadrant][3];
    } else {
        work->patrolPoints[0].x = D_actor_421600_801511D4[scratch->playerQuadrant][4];
        work->patrolPoints[0].z = D_actor_421600_801511D4[scratch->playerQuadrant][5];
        work->patrolPoints[1].x = D_actor_421600_801511D4[scratch->playerQuadrant][6];
        work->patrolPoints[1].z = D_actor_421600_801511D4[scratch->playerQuadrant][7];
    }
}

/// Patrols a quadrant-selected pair of arena points, resuming pursuit when eligible.
///
/// Requires live work/enemy/model/player and an initialized scratch stack.
/// The player's sign quadrant selects a route row; placement zero uses its
/// first pair and every other placement its second. XZ are root-parent units,
/// narrowed in offset vectors; angles use 4096 units per turn. The two points
/// alternate on arrival or after 21 facing/grid pushes, with a 32-unit turn
/// limit and 20-unit walk step. Detection waits for windupFrames and holdoff;
/// near range, the delayed avoidance bearing, player facing or loss of lock
/// can resume pursuit. Both scratch reservations are released before return.
static void _actor421600RouteRoamState(Task* task)
{
    enum {
        ACTOR421600_ROUTE_NEAR_RADIUS         = 1500,
        ACTOR421600_ROUTE_FAR_RADIUS          = 8000,
        ACTOR421600_ROUTE_FAR_LOOK_TICKS      = 451,
        ACTOR421600_ROUTE_UNUSED_ROTATION_YAW = 750,
        ACTOR421600_ROUTE_TURN_LIMIT          = 0x20,
        ACTOR421600_ROUTE_LONG_TURN_THRESHOLD = 0x600,
        ACTOR421600_ROUTE_LOOK_TURN_THRESHOLD = 0x100,
        ACTOR421600_ROUTE_AVOIDANCE_YAW_LIMIT = 0x300,
        ACTOR421600_ROUTE_PLAYER_FACING_LIMIT = 0x600,
        ACTOR421600_ROUTE_WAIST_TURN_SCALE    = 16
    };
    s32                           chaseRadius = ACTOR421600_ROUTE_NEAR_RADIUS;
    DesertChaserWork*             work;
    Enemy*                        enemy;
    GfxCoord*                     playerRoot;
    WorldCollisionContact*        frontContacts;
    GfxCoord*                     entryCoord;
    GfxCoord*                     patrolCoord;
    GfxCoord*                     detectionCoord;
    GfxCoord*                     turnCoord;
    _Actor421600RouteRoamScratch* scratch;
    SVECTOR*                      playerOffset;
    SVECTOR*                      detectionOffset;
    _Actor421600RouteRoamScratch* frameCursor;
    TmdObject*                    model;
    s16                           lookTurn;
    s16                           patrolTurn;
    s16                           nextYaw;
    s32                           playerHeadingX;
    s16                           bearingFromPlayer;
    s16                           playerOffsetZ;
    s32                           patrolTurnMagnitude;
    s32                           lookTurnMagnitude;
    s16                           longWayTurn;
    s32                           fullTurnMagnitude;
    s16                           wrappedPlayerBearing;
    s32                           playerBearing;
    s32                           turnDelta;
    s32                           avoidanceTurn;
    s32                           playerFacingDifference;
    u16                           unsignedPatrolTurn;
    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        scratch                       = SCRATCH_STACK_RESERVE_BLOCK(_Actor421600RouteRoamScratch);
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = DESERT_CHASER_FRONT_RADIUS;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animRate                                        = ANIMATION_RATE_ONE;
        work->blendActive                                     = 0;
        work->animId                                          = DESERT_CHASER_CLIP_PATROL;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;

        _desertChaserAnimTick(task);
        _desertChaserAnimTick(task);
        work->stateTimer          = 0;
        work->stateCounter        = 0;
        playerRoot                = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
        scratch->playerQuadrant   = _actor421600GetRouteQuadrant(playerRoot->coord.t[0], playerRoot->coord.t[2]);
        entryCoord                = task->extra.tmd->coords;
        scratch->toPatrolPoint.vx = (s16)(gPlayerStatus.coordMtx->t[0] - entryCoord->coord.t[0]);
        scratch->toPatrolPoint.vy = gPlayerStatus.coordMtx->t[1] - entryCoord->coord.t[1];
        playerOffsetZ             = gPlayerStatus.coordMtx->t[2] - entryCoord->coord.t[2];
        scratch->toPatrolPoint.vz = playerOffsetZ;
        work->lookYawTarget       = _actorAngleTurnToOffset(task->extra.tmd->coords, scratch->toPatrolPoint.vx, playerOffsetZ);
        work->patrolTarget        = 0;
        // Each quadrant row has two points for placement zero and two for other placements.
        _actor421600SelectPatrolRoute(work, enemy, scratch);
        SCRATCH_STACK_RELEASE_BLOCK(_Actor421600RouteRoamScratch);
        work->wallProbe.shape.ends[1].vz = DESERT_CHASER_PATROL_PROBE_REACH;
        return;
    }
    work->stateCounter += 1;
    // The saved cursor and reserved pointer address the same block; retain both access forms.
    frameCursor                      = SCRATCH_STACK_CURSOR(_Actor421600RouteRoamScratch);
    scratch                          = SCRATCH_STACK_RESERVE_BLOCK(_Actor421600RouteRoamScratch);
    frameCursor[-1].toPatrolPoint.vx = (s16)(work->patrolPoints[work->patrolTarget].x - task->extra.tmd->coords->coord.t[0]);
    scratch->toPatrolPoint.vy        = 0;
    scratch->toPatrolPoint.vz        = work->patrolPoints[work->patrolTarget].z - task->extra.tmd->coords->coord.t[2];
    patrolCoord                      = task->extra.tmd->coords;
    frameCursor[-1].toPlayer.vx      = (s16)(gPlayerStatus.coordMtx->t[0] - patrolCoord->coord.t[0]);
    playerOffset                     = &frameCursor[-1].toPlayer;
    playerOffset->vy                 = gPlayerStatus.coordMtx->t[1] - patrolCoord->coord.t[1];
    playerOffset->vz                 = gPlayerStatus.coordMtx->t[2] - patrolCoord->coord.t[2];
    if (!actorOutsideRadius(&scratch->toPatrolPoint, DESERT_CHASER_WAYPOINT_RADIUS) || work->stateTimer >= DESERT_CHASER_STUCK_TICKS) {
        work->lookYawTarget = _actorAngleTurnToOffset(task->extra.tmd->coords, frameCursor[-1].toPlayer.vx, playerOffset->vz);
        // Retain the original rotation calculation even though table waypoints do not read it.
        if (work->patrolTarget == 0) {
            gfxRotMatrixY(&scratch->rotation, (s16)ratan2((s32)scratch->toPlayer.vx, (s32)scratch->toPlayer.vz) - ACTOR421600_ROUTE_UNUSED_ROTATION_YAW, 1);
            work->patrolTarget = 1;
        } else {
            gfxRotMatrixY(&scratch->rotation, (s16)ratan2((s32)scratch->toPlayer.vx, (s32)scratch->toPlayer.vz) + ACTOR421600_ROUTE_UNUSED_ROTATION_YAW, 1);
            work->patrolTarget = 0;
        }
        playerRoot              = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
        scratch->playerQuadrant = _actor421600GetRouteQuadrant(playerRoot->coord.t[0], playerRoot->coord.t[2]);
        _actor421600SelectPatrolRoute(work, enemy, scratch);
        work->stateTimer = 0;
    }
    _desertChaserAnimTick(task);
    work->lookYawTarget = _actorAngleTurnToOffset(task->extra.tmd->coords, scratch->toPlayer.vx, scratch->toPlayer.vz);
    turnDelta           = _actorAngleTurnToOffset(task->extra.tmd->coords, scratch->toPatrolPoint.vx, scratch->toPatrolPoint.vz);
    scratch->fullTurn   = (scratch->turn = (s16)turnDelta);
    patrolTurn          = scratch->turn;
    unsignedPatrolTurn  = (u16)scratch->turn;
    patrolTurnMagnitude = abs(scratch->turn);
    if (patrolTurnMagnitude >= ACTOR421600_ROUTE_LONG_TURN_THRESHOLD + 1) {
        lookTurn          = work->lookYawTarget;
        lookTurnMagnitude = abs(lookTurn);
        if ((lookTurnMagnitude >= ACTOR421600_ROUTE_LOOK_TURN_THRESHOLD + 1) && ((lookTurn * patrolTurn) < 0)) {
            longWayTurn = unsignedPatrolTurn - ACTOR_TRANSFORM_ANGLE_TURN;
            if (patrolTurn < 0) {
                longWayTurn = unsignedPatrolTurn + ACTOR_TRANSFORM_ANGLE_TURN;
            }
            scratch->turn = longWayTurn;
        }
    }
    if (scratch->turn >= ACTOR421600_ROUTE_TURN_LIMIT + 1) {
        scratch->turn = ACTOR421600_ROUTE_TURN_LIMIT;
    }
    if (scratch->turn < -ACTOR421600_ROUTE_TURN_LIMIT) {
        scratch->turn = -ACTOR421600_ROUTE_TURN_LIMIT;
    }
    work->waistYawTarget = scratch->turn * ACTOR421600_ROUTE_WAIST_TURN_SCALE;
    turnCoord            = task->extra.tmd->coords;
    nextYaw              = (u16)scratch->turn + ratan2((s32)-turnCoord->coord.m[2][0], (s32)turnCoord->coord.m[2][2]);
    scratch->turn        = nextYaw;
    gfxRotMatrixY(&task->extra.tmd->coords->coord, (s32)nextYaw, 1);
    frontContacts = work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts;
    if (work->blendActive == 0) {
        if (_desertChaserCapsuleTouchesGrid(task)) {
            _actorMovementStepForward(task->extra.tmd->coords, DESERT_CHASER_PATROL_STEP);
        } else {
            _actorMovementStepForward(task->extra.tmd->coords, DESERT_CHASER_PATROL_STEP);
        }
        frontContacts = work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts;
    }
    // Avoidance overwrites toPatrolPoint with the displacement actually applied.
    _actorContactApplyAvoidancePushback(task->extra.tmd->coords, frontContacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts), &scratch->toPatrolPoint);
    if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts)) == 1) {
        fullTurnMagnitude = abs(scratch->fullTurn);
        if (fullTurnMagnitude < ACTOR421600_ROUTE_TURN_LIMIT) {
            work->stateTimer += 1;
        }
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    detectionCoord                        = task->extra.tmd->coords;
    scratch->toPlayer.vx                  = (s16)(gPlayerStatus.coordMtx->t[0] - detectionCoord->coord.t[0]);
    detectionOffset                       = &scratch->toPlayer;
    detectionOffset->vy                   = gPlayerStatus.coordMtx->t[1] - detectionCoord->coord.t[1];
    detectionOffset->vz                   = gPlayerStatus.coordMtx->t[2] - detectionCoord->coord.t[2];
    if (work->stateCounter > work->windupFrames) {
        if (work->chaseHoldoff <= 0) {

            if (actorOutsideRadius(&scratch->toPlayer, chaseRadius)) {
                if (!actorOutsideRadius(&scratch->toPlayer, ACTOR421600_ROUTE_FAR_RADIUS) && work->stateCounter >= ACTOR421600_ROUTE_FAR_LOOK_TICKS) {
                    avoidanceTurn = _actorAngleTurnToOffset(task->extra.tmd->coords, scratch->toPatrolPoint.vx, scratch->toPatrolPoint.vz);
                    scratch->turn = (s16)avoidanceTurn;
                    avoidanceTurn = abs(avoidanceTurn);
                    if (avoidanceTurn < ACTOR421600_ROUTE_AVOIDANCE_YAW_LIMIT) {
                        work->state = DESERT_CHASER_STATE_PURSUE;
                    }
                }
            } else {
                work->state = DESERT_CHASER_STATE_PURSUE;
            }
            playerHeadingX         = -(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0];
            scratch->playerYaw     = ratan2((s32)playerHeadingX, (s32)(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
            bearingFromPlayer      = ratan2((s32)scratch->toPlayer.vx, (s32)scratch->toPlayer.vz) + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
            scratch->yawFromPlayer = bearingFromPlayer;
            wrappedPlayerBearing   = _actorAngleNormalizeYaw(bearingFromPlayer);
            playerBearing          = wrappedPlayerBearing;
            scratch->yawFromPlayer = (s16)playerBearing;
            playerFacingDifference = playerBearing - scratch->playerYaw;
            if (playerFacingDifference < 0) {
                playerFacingDifference = -playerFacingDifference;
            }
            if (playerFacingDifference >= ACTOR421600_ROUTE_PLAYER_FACING_LIMIT + 1 || worldTargetGetActorLockMask(&enemy->node) == 0) {
                work->state = DESERT_CHASER_STATE_PURSUE;
            }
        } else {
            work->chaseHoldoff -= 1;
        }
    }
    _actor421600ClampInsideArena(task->extra.tmd->coords);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor421600RouteRoamScratch);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Hides the body and emits its four detached models before waiting to respawn.
///
/// Requires live enemy/work/model. Entry disables lock-on and root-grid tests
/// and emits a part-1 gravity particle. Tick 2 frees primitive buffers; ticks 3/5/7/8
/// detach right leg, left leg, torso and head, with the enemy's placement texture
/// offsets. Tick 10 selects the respawn wait. The left leg's X/Y are zeroed
/// but Z is reused from the uninitialized stack vector; this behavior is retained.
static void _actor421600BurstDeathState(Task* task)
{
    enum {
        ACTOR421600_BURST_FREE_BUFFERS_TICK = 2,
        ACTOR421600_BURST_RIGHT_LEG_TICK    = 3,
        ACTOR421600_BURST_LEFT_LEG_TICK     = 5,
        ACTOR421600_BURST_TORSO_TICK        = 7,
        ACTOR421600_BURST_HEAD_TICK         = 8,
        ACTOR421600_BURST_RESPAWN_TICK      = 10,
        ACTOR421600_BURST_TIMER_LIMIT       = 1024,
        ACTOR421600_BURST_OFFSET_DISTANCE   = 100,
        ACTOR421600_BURST_PARTICLE_ARGUMENT = 0x10300,
        ACTOR421600_BURST_PART_ARGUMENT     = 0x200
    };
    DesertChaserWork* work;
    Enemy*            enemy;
    TmdObject*        model;
    SVECTOR           spawnOffset;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    model = task->extra.tmd;
    if (work->stateEntered != 0) {
        model->flags                                          = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = DESERT_CHASER_FRONT_RADIUS;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags                         = WORLD_TARGET_NOT_LOCKABLE;
        work->lookYaw                                         = 0;
        work->lookYawTarget                                   = 0;
        work->waistYawTarget                                  = 0;
        work->stateTimer                                      = 0;
        spawnOffset.vx                                        = ACTOR421600_BURST_OFFSET_DISTANCE;
        spawnOffset.vz                                        = 0;
        spawnOffset.vy                                        = 0;
        effectSpawn(EFFECT_030, task->extra.tmd->coords + 1, ACTOR421600_BURST_PARTICLE_ARGUMENT, &spawnOffset);
    }
    if (work->stateTimer == ACTOR421600_BURST_FREE_BUFFERS_TICK) {
        model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        tmdFreePrimitiveBuffer(model);
    }
    // Select each detached model before spawning and applying this enemy's texture placement.
    if (work->stateTimer == ACTOR421600_BURST_RIGHT_LEG_TICK) {
        D_80114B34[5].data.model = &_gActor421600DesertChaserBurstLegRight;
        spawnOffset.vz           = ACTOR421600_BURST_OFFSET_DISTANCE;
        spawnOffset.vy           = 0;
        spawnOffset.vx           = 0;
        _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, task->extra.tmd->coords + 9, ACTOR421600_BURST_PART_ARGUMENT, &spawnOffset), enemy);
    }
    if (work->stateTimer == ACTOR421600_BURST_LEFT_LEG_TICK) {
        D_80114B34[5].data.model = &_gActor421600DesertChaserBurstLegLeft;
        spawnOffset.vy           = 0;
        spawnOffset.vx           = 0;
        _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, task->extra.tmd->coords + 12, ACTOR421600_BURST_PART_ARGUMENT, &spawnOffset), enemy);
    }
    if (work->stateTimer == ACTOR421600_BURST_TORSO_TICK) {
        D_80114B34[5].data.model = &_gActor421600DesertChaserBurstTorso;
        _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, task->extra.tmd->coords + 1, ACTOR421600_BURST_PART_ARGUMENT, NULL), enemy);
    }
    if (work->stateTimer == ACTOR421600_BURST_HEAD_TICK) {
        D_80114B34[5].data.model = &_gActor421600DesertChaserBurstHead;
        _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, task->extra.tmd->coords + 3, ACTOR421600_BURST_PART_ARGUMENT, NULL), enemy);
    }
    if (work->stateTimer == ACTOR421600_BURST_RESPAWN_TICK) {
        work->state = ACTOR421600_STATE_WAIT_RESPAWN;
    }
    if (work->stateTimer < ACTOR421600_BURST_TIMER_LIMIT) {
        work->stateTimer++;
    }
}

#include "../../shared/desert_chaser_turn_step.inc.c"

#include "../../shared/desert_chaser_turn_step_probe.inc.c"

static const DesertChaserStateTable D_actor_421600_80131EFC = { { _actor421600HideState,
                                                                  _actor421600ScriptAnimationState,
                                                                  _actor421600TrackArenaRouteState,
                                                                  _actor421600EntranceLungeState,
                                                                  _desertChaserStunned,
                                                                  _actor421600FleeState,
                                                                  _actor421600MoveToZone5State,
                                                                  _actor421600ArenaTransitionState,
                                                                  _actor421600BurstDeathState,
                                                                  _desertChaserTurnRightState,
                                                                  _desertChaserTurnLeftState,
                                                                  _desertChaserStagger,
                                                                  _desertChaserCollapse,
                                                                  NULL,
                                                                  NULL,
                                                                  NULL,
                                                                  NULL,
                                                                  _actor421600DownedWaitState,
                                                                  NULL,
                                                                  NULL,
                                                                  _desertChaserKnockDown,
                                                                  _actor421600ShrinkDeathState,
                                                                  _actor421600WaitToRespawnState,
                                                                  NULL,
                                                                  _desertChaserPatrol,
                                                                  NULL,
                                                                  NULL,
                                                                  NULL,
                                                                  _desertChaserPursue,
                                                                  _actor421600LungeRecoveryState,
                                                                  _desertChaserThrowPlayer,
                                                                  _actor421600LeapBackState,
                                                                  _actor421600WatchRunPlayerState,
                                                                  _desertChaserSteer,
                                                                  _actor421600ResumePursuitState,
                                                                  _actor421600WallKnockDownState,
                                                                  _actor421600RiseState,
                                                                  _desertChaserCloseCatchState,
                                                                  _desertChaserRoam,
                                                                  _actor421600RouteRoamState } };
/// Keeps a collision-enabled held player within 300 vertical units of the chaser.
///
/// Requires live chaser work/model. A missing player is skipped; an enabled
/// move request with a greater height difference snaps Y and dirties the root.
/// The frame driver deliberately calls this twice on its eligible states.
static inline void _actor421600AlignHeldPlayerHeight(Task* task)
{
    enum { ACTOR421600_HELD_HEIGHT_LIMIT = 300 };
    DesertChaserWork* work   = task->work;
    Task*             player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (player != NULL && work->playerMove.collisionRequests == GAME_ACTOR_COLLISION_REQUEST_MASK) {
        GfxCoord* playerRoot = player->extra.tmd->coords;
        GfxCoord* chaserRoot = task->extra.tmd->coords;
        if (abs(playerRoot->coord.t[1] - chaserRoot->coord.t[1]) >= ACTOR421600_HELD_HEIGHT_LIMIT + 1) {
            playerRoot->coord.t[1]                  = chaserRoot->coord.t[1];
            player->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        }
    }
}

/// Drives the Water Tower chaser's combat state and any player held by its catch.
///
/// Requires live enemy/work/model/player and an initialized scratch stack.
/// Pause/hide controls clear contacts and return before damage, catches or
/// state dispatch. Otherwise ticks cooldown, applies damage while the player
/// lives, drives catch requests, dispatches a non-NULL state-table entry and
/// updates collision eligibility. Contacts are consumed then cleared; part 2's
/// world-space origin is published to the enemy before the final arena snap.
/// Catch release preserves the original index-5 write beyond its five-entry set table.
static void _actor421600FrameState(Enemy* enemy, Task* task)
{
    enum {
        ACTOR421600_VIEW_RESUME_ROUTE         = 5,
        ACTOR421600_VIEW_CLAMP_ARENA_9        = 9,
        ACTOR421600_VIEW_KEEP_PLAYER_POSITION = 14,
        ACTOR421600_VIEW_CLAMP_ARENA_18       = 18,
        ACTOR421600_VIEW_HIDE_CHASER          = 19,
        ACTOR421600_CATCH_START               = 1,
        ACTOR421600_CATCH_HOLD                = 2,
        ACTOR421600_CATCH_THROW               = 3,
        ACTOR421600_CATCH_RECOVER             = 4,
        ACTOR421600_CATCH_RELEASE             = 5,
        ACTOR421600_CATCH_FINISHED_7          = 7,
        ACTOR421600_CATCH_DECELERATE_TICKS    = 15,
        ACTOR421600_REAR_RECOVER_TICKS        = 23,
        ACTOR421600_FRONT_RECOVER_TICKS       = 34,
        ACTOR421600_THROW_MOVE_TICKS          = 6,
        ACTOR421600_RELEASE_TICKS             = 7,
        ACTOR421600_PLAYER_RECOVERY_SET       = 7,
        ACTOR421600_PLAYER_RELEASE_SET        = 9,
        ACTOR421600_RECOVERY_BLEND_FRAMES     = 3,
        ACTOR421600_RELEASE_BLEND_FRAMES      = 6,
    };
    PlayerStatus*          playerStatus;
    VECTOR                 lightingPosition;
    DesertChaserStateTable states;

    s16                       frameView;
    s16                       shadowHeight;
    s16                       routeState;
    s16                       heldState;
    s16                       collisionState;
    s32                       catchStage;
    s32                       settledCatchStage;
    s32                       moveReply;
    u8                        lastCommand;
    s32                       catchCommand;
    AnimationSet**            releaseSets;
    DesertChaserWork*         work;
    Task*                     player;
    AnimationSet**            recoverySets;
    DesertChaserFrameScratch* scratch;

    work         = task->work;
    player       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    playerStatus = &gPlayerStatus;
    frameView    = viewGetMappedIndex() & 0xFF;
    states       = D_actor_421600_80131EFC;
    // Relight from the composed root before honoring pause or hide controls.
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
    lightingPosition.vx = task->extra.tmd->coords->workm.t[0];
    lightingPosition.vy = task->extra.tmd->coords->workm.t[1];
    lightingPosition.vz = task->extra.tmd->coords->workm.t[2];
    worldCoordUpdateActorColor(enemy, &lightingPosition, 0, 0);
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->state != DESERT_CHASER_STATE_DEATH && work->state != DESERT_CHASER_STATE_HIDDEN && work->state != ACTOR421600_STATE_WAIT_RESPAWN && work->state != ACTOR421600_STATE_ARENA_TRANSITION && work->state != ACTOR421600_STATE_BURST_DEATH) {
                shadowHeight = task->extra.tmd->coords->coord.t[1];
                _limbShadowDrawSegment(task, 1, 3, 0x12C, shadowHeight, 0xFF);
                _limbShadowDrawSegment(task, 3, 4, 0xC8, shadowHeight, 0xFF);
                _limbShadowDrawSegment(task, 1, 0xB, 0xFA, shadowHeight, 0xFF);
                if ((viewGetMappedIndex() & 0xFF) == ACTOR421600_VIEW_HIDE_CHASER) {
                    task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                } else {
                    task->extra.tmd->flags = 0;
                }
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            if (work->state != DESERT_CHASER_STATE_DEATH && work->state != DESERT_CHASER_STATE_HIDDEN && work->state != ACTOR421600_STATE_WAIT_RESPAWN && work->state != ACTOR421600_STATE_ARENA_TRANSITION && work->state != ACTOR421600_STATE_BURST_DEATH) {
                shadowHeight = task->extra.tmd->coords->coord.t[1];
                _limbShadowDrawSegment(task, 1, 3, 0x12C, shadowHeight, 0xFF);
                _limbShadowDrawSegment(task, 3, 4, 0xC8, shadowHeight, 0xFF);
                _limbShadowDrawSegment(task, 1, 0xB, 0xFA, shadowHeight, 0xFF);
                if ((viewGetMappedIndex() & 0xFF) == ACTOR421600_VIEW_HIDE_CHASER) {
                    task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                } else {
                    task->extra.tmd->flags = 0;
                }
            }
            worldCollisionClearContacts(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts);
            worldCollisionClearContacts(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts);
            worldCollisionClearContacts(work->spheres[DESERT_CHASER_SPHERE_REAR].contacts);
            worldCollisionClearContacts(work->wallProbe.contacts);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            worldCollisionClearContacts(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts);
            worldCollisionClearContacts(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts);
            worldCollisionClearContacts(work->spheres[DESERT_CHASER_SPHERE_REAR].contacts);
            worldCollisionClearContacts(work->wallProbe.contacts);
            return;
    }
    scratch = SCRATCH_STACK_RESERVE_BLOCK(DesertChaserFrameScratch);
    if (work->hitCooldown > 0) {
        work->hitCooldown = (s16)((u16)work->hitCooldown - 1);
    } else if (playerStatus->hp > 0) {
        _desertChaserDamage(task);
    }
    lastCommand = work->lastCommand.fields.command;
    if ((lastCommand == DRYFIELD_WATER_TOWER_CHASER_COMMAND_START_FINAL_BATTLE) && ((routeState = work->state, (routeState == DESERT_CHASER_STATE_ROAM)) || (routeState == lastCommand))) {
        work->state = ACTOR421600_STATE_ROUTE_ROAM;
    }
    if (work->prevState != work->state) {
        work->stateEntered = 1;
    } else {
        work->stateEntered = 0;
    }
    work->prevState = (s16)(u16)work->state;
    scratch->zone   = _actor421600GetArenaZone(task->extra.tmd->coords);
    // Drive the held player before the chaser state; message replies can stop the push.
    if (work->playerHeld == 1) {
        heldState = work->state;
        if ((heldState != DESERT_CHASER_STATE_DEATH) && (heldState != DESERT_CHASER_STATE_HIDDEN) && (heldState != ACTOR421600_STATE_BURST_DEATH)) {
            _actor421600AlignHeldPlayerHeight(task);
        }
        _actor421600AlignHeldPlayerHeight(task);
        catchCommand = work->lastCommand.fields.command;
        work->lastCommand.fields.catchFrames++;
        if (catchCommand == DRYFIELD_WATER_TOWER_CHASER_COMMAND_START_RUN) {
            if (D_dryfield_water_tower_801876AA == (D_dryfield_water_tower_801876A8 + 1)) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, PLAYER_ACTOR_END_SCRIPTED_DEFAULT, 0);
                work->playerHeld = 0;
            }
            if ((work->lastCommand.fields.command == catchCommand) && ((s32)D_dryfield_water_tower_801876AA < (D_dryfield_water_tower_801876A8 + 3))) {
                work->playerMove.displacement.vx = 0;
                work->playerMove.displacement.vy = 0;
                work->playerMove.displacement.vz = 0;
            }
        }
        catchStage = work->playerAnim.animationId;
        switch (catchStage) {
            case ACTOR421600_CATCH_RECOVER:
                break;
            case ACTOR421600_CATCH_START:
                if (TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_MOVE_BY, &work->playerMove, 0) == 1) {
                    work->playerMove.displacement.vx = 0;
                    work->playerMove.displacement.vy = 0;
                    work->playerMove.displacement.vz = 0;
                }
                if ((work->lastCommand.fields.catchFrames >= ACTOR421600_CATCH_DECELERATE_TICKS) && (work->playerMove.collisionRequests == GAME_ACTOR_COLLISION_REQUEST_MASK)) {
                    work->playerMove.displacement.vx >>= 1;
                    work->playerMove.displacement.vy >>= 1;
                    work->playerMove.displacement.vz >>= 1;
                }
                break;
            case ACTOR421600_CATCH_HOLD:
                recoverySets = work->playerAnim.source.sets;
                if (recoverySets == gDesertChaserRearAnim) {
                    if ((playerStatus->hp > 0) && (work->lastCommand.fields.catchFrames >= ACTOR421600_REAR_RECOVER_TICKS)) {
                        recoverySets[ACTOR421600_CATCH_RECOVER] = (Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon])->table.sets[ACTOR421600_PLAYER_RECOVERY_SET];
                        work->playerAnim.animationId            = ACTOR421600_CATCH_RECOVER;
                        work->playerAnim.blend                  = ANIMATION_BLEND_INTERPOLATE;
                        work->playerAnim.blendFrames            = ACTOR421600_RECOVERY_BLEND_FRAMES;
                        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
                        work->lastCommand.fields.catchFrames = 0U;
                    }
                } else if ((playerStatus->hp > 0) && (work->lastCommand.fields.catchFrames >= ACTOR421600_FRONT_RECOVER_TICKS)) {
                    gDesertChaserFrontAnim[ACTOR421600_CATCH_RECOVER] = (Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon])->table.sets[ACTOR421600_PLAYER_RECOVERY_SET];

                    work->playerAnim.animationId = ACTOR421600_CATCH_RECOVER;
                    work->playerAnim.blend       = ANIMATION_BLEND_INTERPOLATE;
                    work->playerAnim.blendFrames = ACTOR421600_RECOVERY_BLEND_FRAMES;
                    TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
                    work->lastCommand.fields.catchFrames = 0U;
                }
                break;
            case ACTOR421600_CATCH_THROW:
                if ((work->lastCommand.fields.catchFrames < ACTOR421600_THROW_MOVE_TICKS) && (playerStatus->hp > 0)) {
                    moveReply = TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_MOVE_BY, &work->playerMove, 0);
                    if (moveReply == 1) {
                        work->playerMove.displacement.vx = 0;
                        work->playerMove.displacement.vy = 0;
                        work->playerMove.displacement.vz = 0;
                        work->playerMove.keepControl     = moveReply;
                    }
                }
                break;
            case ACTOR421600_CATCH_RELEASE:
                if ((playerStatus->hp > 0) && (work->lastCommand.fields.catchFrames >= ACTOR421600_RELEASE_TICKS)) {
                    taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET, 0);
                    work->playerHeld = 0;
                }
                break;
        }
        if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, NULL, 0) == 0) {
            settledCatchStage = work->playerAnim.animationId;
            switch (settledCatchStage) {
                case ACTOR421600_CATCH_START:
                    if (((((GAME_LOCATION_WORD(gGameSession->location.loc)) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_MESA, 0, 0)) || (work->playerMove.collisionRequests != (GAME_ACTOR_COLLISION_REQUEST_MASK << GAME_ACTOR_COLLISION_DISABLE_REQUEST_SHIFT))) && (playerStatus->hp > 0)) {
                        work->playerAnim.blend       = ANIMATION_BLEND_RESET;
                        work->playerAnim.blendFrames = 0;
                        work->playerAnim.animationId = ACTOR421600_CATCH_HOLD;

                        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
                        work->lastCommand.fields.catchFrames = 0U;
                    }
                    break;
                case ACTOR421600_CATCH_THROW:
                    if (playerStatus->hp > 0) {
                        work->playerAnim.blend       = ANIMATION_BLEND_INTERPOLATE;
                        work->playerAnim.blendFrames = ACTOR421600_RELEASE_BLEND_FRAMES;
                        work->playerAnim.animationId = ACTOR421600_CATCH_RELEASE;
                        releaseSets                  = work->playerAnim.source.sets;
                        if (releaseSets == gDesertChaserRearAnim) {
                            releaseSets[ACTOR421600_CATCH_RELEASE] = (Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon])->table.sets[ACTOR421600_PLAYER_RELEASE_SET];
                        } else {
                            gDesertChaserFrontAnim[ACTOR421600_CATCH_RELEASE] = (Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon])->table.sets[ACTOR421600_PLAYER_RELEASE_SET];
                        }
                        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
                        work->lastCommand.fields.catchFrames = 0U;
                    }
                    break;
                case ACTOR421600_CATCH_RECOVER:
                case ACTOR421600_CATCH_FINISHED_7:
                    if (playerStatus->hp > 0) {
                        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET, 0);
                        work->playerHeld = 0;
                    }
                    break;
            }
        }
    }
    // State handlers consume this frame's contacts before they are cleared.
    states.handlers[work->state](task);
    if (work->state == DESERT_CHASER_STATE_SCRIPT_ANIMATION && work->lastCommand.fields.command == DRYFIELD_WATER_TOWER_CHASER_COMMAND_START_BATTLE) {
        if ((viewGetMappedIndex() & 0xFF) == ACTOR421600_VIEW_RESUME_ROUTE)
            work->state = ACTOR421600_STATE_TRACK_ARENA_ROUTE;
    }
    if (_actor421600HasPlayerBodyContact(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts)) {
        switch (frameView) {
            case ACTOR421600_VIEW_CLAMP_ARENA_9:
            case ACTOR421600_VIEW_CLAMP_ARENA_18:
                _actor421600ClampInsideArena(player->extra.tmd->coords);
                break;
            case ACTOR421600_VIEW_KEEP_PLAYER_POSITION:
                break;
            default:
                _actor421600PushOutsideArenaCenter(player->extra.tmd->coords);
                break;
        }
    }
    collisionState = work->state;
    if ((collisionState != DESERT_CHASER_STATE_DEATH) && (collisionState != DESERT_CHASER_STATE_HIDDEN) && (collisionState != DESERT_CHASER_STATE_FLEE) && (collisionState != ACTOR421600_STATE_WAIT_RESPAWN) && (collisionState != ACTOR421600_STATE_BURST_DEATH)) {
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
    // Publish part 2's origin in world coordinates, excluding the view transform.
    scratch->bodyPos.vx = 0U;
    scratch->bodyPos.vy = 0;
    scratch->bodyPos.vz = 0;
    _actorRenderTransformToWorld(task->extra.tmd->coords + 2, &scratch->bodyPos);
    enemy->bodyPos.vx = (s32)(s16)scratch->bodyPos.vx;
    enemy->bodyPos.vy = (s32)scratch->bodyPos.vy;
    enemy->bodyPos.vz = (s32)scratch->bodyPos.vz;
    enemy->coord      = &gGfxViewCoord;
    SCRATCH_STACK_RELEASE_BLOCK(DesertChaserFrameScratch);
    _actor421600SnapToSouthArenaArc(task->extra.tmd->coords);
}

/// Ignores message 2015 without accessing the receiver or either payload.
///
/// This callback does not supply a reply value; senders must ignore the
/// dispatch result. The message's wider sender-side purpose is unproven.
static void _actor421600IgnoreMessage2015(Task* task, s32 messageId, s32 unusedArg, s32 unusedExtra)
{
}

/// The enemy task's state handlers - spawn, per-frame tick and teardown - run by
/// `_desertChaserTask`.
static const DesertChaserTaskStates gDesertChaserTaskStates = {
    {
        _actor421600Spawn,
        _actor421600FrameState,
        enemyDestroy,
    },
};

#include "../../shared/actor_messages_visibility.inc.c"

/// Returns 1 while the chaser is alive or its model is visible, otherwise 0.
///
/// Handles `ACTOR_MESSAGE_IS_PRESENT` with no payload. The task must carry a
/// live enemy and model; a dead but still visible death animation counts as present.
static s32 _actor421600IsPresent(Task* task, s32 messageId, s32 unusedArg, s32 unusedExtra)
{
    Enemy* enemy = task->spawnArg2.pointer;

    if (enemy->hp <= 0 && (task->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) != 0) {
        return 0;
    }
    return 1;
}

#include "../../shared/actor_messages_place_yaw_first.inc.c"

#include "../../shared/desert_chaser_play_anim.inc.c"

/// Restarts the dead chaser's room-event countdown at thirty ticks and replies 1.
///
/// Handles `ROOM_MESSAGE_ACTOR_EVENT`; both payload words are unused. The
/// task must own live chaser work. The respawn wait decrements this counter,
/// but does not use it to delay a return.
static s32 _actor421600OnRoomEvent(Task* task, s32 messageId, s32 unusedArg, s32 unusedExtra)
{
    enum { ACTOR421600_ROOM_EVENT_COUNTDOWN_FRAMES = 30 };
    DesertChaserWork* work = task->work;

    work->roomEventCountdown = ACTOR421600_ROOM_EVENT_COUNTDOWN_FRAMES;
    return 1;
}

/// Releases the chaser's child tasks and collision links before destroying its enemy task.
///
/// A missing work block skips child/link cleanup. Otherwise the front, rear
/// and root spheres are unlinked and the enemy's borrowed contact pointer is
/// cleared before enemyDestroy releases the work/model/task. The capsule's
/// separate grid link is not unlinked here. Requires a live enemy record;
/// child-task fields may be NULL and are not populated by this package.
static void _actor421600Exit(Task* task)
{
    DesertChaserWork* work;
    Enemy*            enemy;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work != NULL) {
        if (work->childTask0 != NULL) {
            taskKill(work->childTask0);
        }
        if (work->childTask1 != NULL) {
            taskKill(work->childTask1);
        }
        worldCollisionUnlinkBody(&work->spheres[DESERT_CHASER_SPHERE_FRONT].body);
        worldCollisionUnlinkBody(&work->spheres[DESERT_CHASER_SPHERE_REAR].body);
        worldCollisionUnlinkBody(&work->spheres[DESERT_CHASER_SPHERE_ROOT].body);
        enemy->recs = NULL;
    }
    enemyDestroy(enemy, task);
}

#include "../../shared/desert_chaser_part_effect.inc.c"

/// Copies an arena waypoint's XYZ into a borrowed vector, preserving pad.
///
/// The low signed halfword of zone must index the thirteen-entry waypoint
/// table (0..12). Components use root-parent game-coordinate units. Both
/// storage objects must remain live during the call; no pointer is retained.
/// This standalone body has no known C caller.
static void _actor421600CopyArenaWaypoint(SVECTOR* position, s32 zone)
{
    position->vx = D_actor_421600_80151158[(s16)zone].vx;
    position->vy = D_actor_421600_80151158[(s16)zone].vy;
    position->vz = D_actor_421600_80151158[(s16)zone].vz;
}

/// Returns the arena patrol quadrant for an XZ position.
///
/// Returns 0 for +X/+Z, 1 for nonpositive X/+Z, 2 for +X/nonpositive Z,
/// and 3 for nonpositive X/Z. Zero lies on the nonpositive side of each axis.
/// The four-entry lookup accepts all signed 32-bit inputs; coordinates use
/// root-parent units. This standalone body has no known C caller.
static s8 _actor421600LookupRouteQuadrant(s32 x, s32 z)
{
    const s8* quadrantMap;
    s32       positiveX;
    s32       nonPositiveZ;

    quadrantMap  = D_actor_421600_801511D0;
    positiveX    = x > 0;
    nonPositiveZ = z < 1;
    return quadrantMap[positiveX + (nonPositiveZ << 1)];
}

/// Enters the hidden state with zero health, no lock-on and no root-grid test.
///
/// Hides the live model only on the state-entry tick. The frame driver disables
/// the two pair-test spheres for this state; this handler preserves their flags
/// and leaves allocated model buffers available for a later return.
static void _actor421600HideState(Task* task)
{
    TmdObject*        model;
    DesertChaserWork* work;
    Enemy*            enemy;

    work = task->work;
    if (work->stateEntered != 0) {
        model                                                = task->extra.tmd;
        enemy                                                = task->spawnArg2.pointer;
        enemy->node.state.parts.flags                        = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                                        |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->hp                                            = 0;
    }
}

#include "../../shared/desert_chaser_stunned.inc.c"

/// Restores a live chaser's targeting and drawing and allocates primitive buffers.
///
/// Borrows the task's enemy and model for this call; work and collision flags
/// are configured separately by the state. Existing model buffers are reused.
static inline void _actor421600RearmPresentation(Task* task, TmdObject* model)
{
    ((Enemy*)task->spawnArg2.pointer)->node.state.parts.flags = 0;
    model->flags                                              = 0;
    tmdAllocPrimitiveBuffer(model);
}

/// Blends through lunge recovery, then tracks the arena route or flees.
///
/// Entry restores targeting, drawing and root-grid collision and ticks clip 4
/// once at normal rate. Later ticks wait for slot 1 to settle; Water Tower
/// command 1 chooses flee, otherwise arena-route tracking. Requires live
/// work, enemy and model.
static void _actor421600LungeRecoveryState(Task* task)
{
    enum { ACTOR421600_CLIP_LUNGE_RECOVERY = 4 };
    TmdObject*        model;
    DesertChaserWork* work;
    s32               nextState;

    work = task->work;
    if (work->stateEntered != 0) {
        model = task->extra.tmd;
        _actor421600RearmPresentation(task, model);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = DESERT_CHASER_FRONT_RADIUS;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animRate                                        = ANIMATION_RATE_ONE;
        work->blendActive                                     = 0;
        work->animId                                          = ACTOR421600_CLIP_LUNGE_RECOVERY;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _desertChaserAnimTick(task);
        return;
    }
    _desertChaserAnimTick(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        nextState = work->lastCommand.word & DESERT_CHASER_COMMAND_MASK;
        if (nextState == DESERT_CHASER_COMMAND_WATER_TOWER_1) {
            nextState = DESERT_CHASER_STATE_FLEE;
        } else {
            nextState = ACTOR421600_STATE_TRACK_ARENA_ROUTE;
        }
        work->state = nextState;
    }
}

/// Leaves steering through clip 8 and resumes pursuit after the clip settles.
///
/// Entry restores targeting, drawing and root-grid collision at normal rate.
/// Root contacts push the chaser out of the grid every tick. Entry advances
/// animation twice; later ticks advance it once. Requires live work/enemy/model.
static void _actor421600ResumePursuitState(Task* task)
{
    enum { ACTOR421600_CLIP_RESUME_PURSUIT = 8 };
    TmdObject*        model;
    DesertChaserWork* work;

    work = task->work;
    if (work->stateEntered != 0) {
        model = task->extra.tmd;
        _actor421600RearmPresentation(task, model);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = DESERT_CHASER_FRONT_RADIUS;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animRate                                        = ANIMATION_RATE_ONE;
        work->blendActive                                     = 0;
        work->animId                                          = ACTOR421600_CLIP_RESUME_PURSUIT;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _desertChaserAnimTick(task);
    }
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts));
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _desertChaserAnimTick(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        work->state = DESERT_CHASER_STATE_PURSUE;
    }
}

/// Rises through clip 12, then returns to arena-route tracking.
///
/// Entry restores targeting, drawing and root-grid collision at normal rate
/// and advances animation twice; later ticks advance it once. Slot 1 settling
/// completes the rise. Requires live work, enemy and model.
static void _actor421600RiseState(Task* task)
{
    enum { ACTOR421600_CLIP_RISE = 12 };
    TmdObject*        model;
    DesertChaserWork* work;

    work = task->work;
    if (work->stateEntered != 0) {
        model = task->extra.tmd;
        _actor421600RearmPresentation(task, model);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = DESERT_CHASER_FRONT_RADIUS;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animRate                                        = ANIMATION_RATE_ONE;
        work->blendActive                                     = 0;
        work->animId                                          = ACTOR421600_CLIP_RISE;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _desertChaserAnimTick(task);
    }
    _desertChaserAnimTick(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        work->state = ACTOR421600_STATE_TRACK_ARENA_ROUTE;
    }
}

#include "../../shared/desert_chaser_flinch.inc.c"

#include "../../shared/desert_chaser_stagger.inc.c"

#include "../../shared/desert_chaser_collapse.inc.c"

#include "../../shared/desert_chaser_task.inc.c"
