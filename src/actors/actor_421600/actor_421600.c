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

/// 4x4 zone table `func_actor_421600_8013A404` samples with the X and Z
/// buckets of the actor's position, cell `x | z * 4`; the sample is compared
/// against 0xB to pick between the 6 and 0x24 states.
extern s8 D_actor_421600_801511C0[16];

/// Idle yaw `func_actor_421600_80132A00` stamps onto the enemy's `field_40`
/// on every state message, the same slot actor 00100 keeps at 0x8013EF3C.

/// Progress counter the same handler compares against 4 / 5 / 2 / 0 to pick
/// the arena corner the actor is dropped into. Written by
/// `func_actor_421600_80134AD4` at spawn.
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

static void func_actor_421600_8013E668(Task* task);
static void func_actor_421600_8013E858(Task* arg0);
static void func_actor_421600_8013E9D8(Task* arg0);
static void func_actor_421600_8013EAAC(Task* arg0);
static void func_actor_421600_8013EB7C(Task* arg0);

static AnimationSet _gActor421600Animation19E54;
static AnimationSet _gActor421600Animation1A5F0;
static AnimationSet _gActor421600Animation1ADFC;
static AnimationSet _gActor421600Animation1B610;
static AnimationSet _gActor421600Animation1B8C4;
static AnimationSet _gActor421600Animation1BBE4;
static TmdSource    _gActor421600DesertChaserBody;
s32                 func_actor_421600_80132A00(Task* task, s32 msgId, ActorCommand* request, s32 arg3);
s32                 func_actor_421600_8013E4EC(Task*, s32, s32, s32);
s32                 func_actor_421600_8013E654(Task*, s32, s32, s32);
s32                 func_actor_421600_8013E424(Task*, s32, s32, s32);

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
    { 2015, func_actor_421600_8013E424 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetVisibility },
    { ACTOR_MESSAGE_IS_PRESENT, func_actor_421600_8013E4EC },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceYawFirst },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_421600_80132A00 },
    { ACTOR_MESSAGE_PLAY_ANIMATION, _desertChaserMsgPlayAnim },
    { ROOM_MESSAGE_ACTOR_EVENT, func_actor_421600_8013E654 },
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

static __inline__ s32  Actor421600_HasPlayerContact(WorldCollisionContact* records);
static s32             func_actor_421600_80133334(GfxCoord* arg0);
static void            func_actor_421600_80133444(GfxCoord* coord);
static s32             desertChaserAvoidWalk(GfxCoord* coord, WorldCollisionContact* recs, s16 count, SVECTOR* pos);
static __inline__ void Actor421600_BindMatrices(Task* actor);
static void            func_actor_421600_80134AD4(Enemy* enemy, Task* actor);
static __inline__ s32  Actor421600_FindDamageHit(WorldCollisionContact* records,
                                                 SVECTOR*               pos);
static void            func_actor_421600_801354D8(Task* arg0);
static void            func_actor_421600_80135F6C(Task* arg0);
static __inline__ s16  Actor421600_Zone(GfxCoord* coord);
static void            func_actor_421600_80136138(Task* arg0);
static __inline__ void Actor421600_ShrinkCoord(GfxCoord* coord, s16 y);
static void            func_actor_421600_801366F4(Task* arg0);
static void            func_actor_421600_801369A0(Task* arg0);
static void            func_actor_421600_80138D24(Task* arg0);
static void            func_actor_421600_8013903C(Task* arg0);
static void            func_actor_421600_8013947C(Task* arg0);
static void            func_actor_421600_8013A404(Task* arg0);
static void            func_actor_421600_8013A554(Task* arg0);
static void            func_actor_421600_8013B00C(Task* arg0);
static void            func_actor_421600_8013B4C4(Task* arg0);
static void            func_actor_421600_8013B8E0(Task* arg0);
static __inline__ s32  Actor421600_RouteZone(s32 x, s32 z);
static void            func_actor_421600_8013BA70(Task* arg0);
static void            func_actor_421600_8013C8E0(Task* arg0);
static void            func_actor_421600_8013D658(Enemy* enemy, Task* actor);
static void            func_actor_421600_8013E7F8(SVECTOR* arg0, s32 arg1);
static s8              func_actor_421600_8013E830(s32 arg0, s32 arg1);

static __inline__ s32 Actor421600_HasPlayerContact(WorldCollisionContact* records)
{
    s16 i;
    for (i = 0; i < 12; i++) {
        if (records[i].key.value == 0)
            break;
        if ((records[i].key.value & 0xFFFF0000) == 0x10000)
            return 1;
    }
    return 0;
}

#include "../../shared/actor_contacts_turn_joint.inc.c"

#include "../../shared/actor_contacts_steer.inc.c"

#include "../../shared/actor_contacts_push_contact.inc.c"

/// Message handler. Message 0x109 nudges the state machine by its sub-command
/// (1 copies `chaseHoldoffFrames` into `chaseHoldoff`, 3 moves state 1 on to 2). Any other
/// message has its opcode and the low byte of its sub-command latched into
/// `lastCommand`; for 0x1402 the enemy's hit points are restored and, by
/// sub-command, the placement mode in `placeKey` and the progress counter
/// `D_actor_421600_80151268`, the actor is dropped at a fixed spot with a new
/// state. Returns 1 when the message was handled.
s32 func_actor_421600_80132A00(Task* arg0, s32 arg1, ActorCommand* request, s32 arg3)
{
    DesertChaserWork* work;
    Enemy*            enemy;
    s32               angle;
    s16               mode;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;

    if (request->context.key == 0x109) {
        switch (request->command) {
            case 1:
                work->chaseHoldoff = work->chaseHoldoffFrames;
                break;
            case 2:
                if (work->state == 0x26) {
                    work->state = 0x26;
                }
                break;
            case 3:
                if (work->state == 1) {
                    work->state = 2;
                }
                break;
        }
        return 1;
    }

    work->lastCommand.fields.stage   = request->context.loc.stage;
    work->lastCommand.fields.area    = request->context.loc.area;
    work->lastCommand.fields.command = (u8)request->command;

    if (request->context.key != 0x1402) {
        return 0;
    }

    switch (request->command) {
        case 0:
            enemy->hp = D_actor_421600_8013EF38.hpMax;
            if ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) == 0) {
                work->state = 2;
            }
            return 1;

        case 1:
            mode      = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
            enemy->hp = D_actor_421600_8013EF38.hpMax;
            switch (mode) {
                case 0:
                    if (D_actor_421600_80151268 >= 4) {
                        if (work->state != 0) {
                            break;
                        }
                        arg0->extra.tmd->coords->coord.t[0] = 0x1057;
                        arg0->extra.tmd->coords->coord.t[2] = -0x11A3;
                        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, -0x400, 1);
                        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                        actorRenderComposeCoord(arg0->extra.tmd->coords);
                        work->state     = 0x20;
                        work->prevState = -1;
                        break;
                    }
                    work->state     = 0;
                    work->prevState = -1;
                    break;
                case 1:
                    if (D_actor_421600_80151268 >= 5) {
                        if (work->state != 0) {
                            break;
                        }
                        arg0->extra.tmd->coords->coord.t[0] = 0x1467;
                        arg0->extra.tmd->coords->coord.t[2] = 0x4B9;
                        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0x7BC, 1);
                        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                        actorRenderComposeCoord(arg0->extra.tmd->coords);
                        work->state     = 0x20;
                        work->prevState = -1;
                        break;
                    }
                    work->state     = 0;
                    work->prevState = -1;
                    break;
            }
            worldCoordSetActorColorMode(enemy, ENEMY_COLOR_DEFAULT);
            enemy->reactionFlags = 0;
            enemy->hp            = D_actor_421600_8013EF38.hpMax;
            return 1;

        case 2:
            switch (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) {
                case 0:
                    if (D_actor_421600_80151268 <= 0) {
                        break;
                    }
                    arg0->extra.tmd->coords->coord.t[0] = -0xD40;
                    arg0->extra.tmd->coords->coord.t[2] = 0x104F;
                    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, -0x76C, 1);
                    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(arg0->extra.tmd->coords);
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_DEFAULT);
                    enemy->reactionFlags = 0;
                    enemy->hp            = D_actor_421600_8013EF38.hpMax;
                    work->state          = 6;
                    break;
                case 1:
                    if (D_actor_421600_80151268 < 2) {
                        break;
                    }
                    arg0->extra.tmd->coords->coord.t[0] = 0x138C;
                    arg0->extra.tmd->coords->coord.t[2] = 0x4B2;
                    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0x7BC, 1);
                    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(arg0->extra.tmd->coords);
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_DEFAULT);
                    enemy->reactionFlags = 0;
                    enemy->hp            = D_actor_421600_8013EF38.hpMax;
                    work->state          = 6;
                    break;
            }
            if (D_actor_421600_80151268 == 0) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, 0, 0);
            }
            return 1;

        case 3:
            if (work->playerHeld == 1) {
                work->playerHeld = 0;
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 2, 0);
            }
            if (work->state != 0x14 && work->state != 0x11 && work->state != 0x15 &&
                work->state != 0x16 && work->state != 0 && work->state != 8) {
                work->state     = 5;
                work->prevState = -1;
            }
            return 1;

        case 9:
            work->state     = 0;
            work->prevState = -1;
            return 1;

        default:
            return 0;
    }
}

#include "../../shared/limb_shadows_segment.inc.c"

/// Moves an interior coordinate to the nearest padded X or Z edge.
/// Returns 1 when moved, or 0 when already outside the rectangle.
static s32 func_actor_421600_80133334(GfxCoord* arg0)
{
    s16 dx;
    s16 dz;
    s32 adx;
    s32 adz;
    s32 x;
    s32 z;
    s32 z2;

    x = arg0->coord.t[0];
    if ((x >= -0xC4D) && (x < 0xD16)) {
        z = arg0->coord.t[2];
        if (z < 0xC4E) {
            if (z >= -0xC4D) {
                if ((0xD16 - x) > (x + 0xC4E)) {
                    dx = -((u16)arg0->coord.t[0] + 0xCE4);
                } else {
                    dx = 0xDAC - (u16)arg0->coord.t[0];
                }
                z2 = arg0->coord.t[2];
                if ((0xC4E - z2) > (z2 + 0xC4E)) {
                    dz = -((u16)arg0->coord.t[2] + 0xCE4);
                } else {
                    dz = 0xCE4 - (u16)arg0->coord.t[2];
                }
                adx = ABS(dx);
                adz = ABS(dz);
                if (adz < adx) {
                    arg0->coord.t[2] += dz;
                } else {
                    arg0->coord.t[0] += dx;
                }
                arg0->composeStamp = GRAPHICS_COORD_DIRTY;
                return 1;
            }
        }
    }
    return 0;
}

static void func_actor_421600_80133444(GfxCoord* coord)
{
    SVECTOR              vec;
    SVECTOR*             direction;
    OverlayRangeScratch* rangeScratch;
    OverlayRangeScratch* savedScratchHead;
    s32                  outside;
    u8*                  scratchBase;
    u8*                  scratchRestoreBase;

    if ((u32)(coord->coord.t[0] - 0x1F5) < 0x3E7) {
        if (coord->coord.t[2] < 0x1F4) {
            if (coord->coord.t[2] < -0x1F4) {
                // Reserve squared-distance operands, then release them before the test.
                savedScratchHead                                                       = SCRATCH_STACK_CURSOR(OverlayRangeScratch);
                rangeScratch                                                           = savedScratchHead - 1;
                scratchBase                                                            = PLAYSTATION_SCRATCHPAD_BASE;
                *(OverlayRangeScratch**)(scratchBase + SCRATCH_STACK_HEAD_BYTE_OFFSET) = rangeScratch;
                vec.vx                                                                 = (u16)coord->coord.t[0] - 0x3E8;
                vec.vy                                                                 = 0;
                vec.vz                                                                 = (u16)coord->coord.t[2] + 1;
                rangeScratch->dx                                                       = vec.vx;
                direction                                                              = &vec;
                rangeScratch->dz                                                       = direction->vz;
                rangeScratch->radius                                                   = 0x2D0;
                rangeScratch->dx                                                       = rangeScratch->dx * rangeScratch->dx;
                rangeScratch->dz                                                       = rangeScratch->dz * rangeScratch->dz;
                rangeScratch->radius                                                   = rangeScratch->radius * rangeScratch->radius;
                scratchRestoreBase                                                     = PLAYSTATION_SCRATCHPAD_BASE + (SCRATCH_STACK_HEAD_BYTE_OFFSET - sizeof(void*));
                *(OverlayRangeScratch**)(scratchRestoreBase + sizeof(void*))           = savedScratchHead;
                outside                                                                = rangeScratch->dx + rangeScratch->dz >= rangeScratch->radius;
                if (outside != 0) {
                    return;
                }
                VectorNormalSS(direction, direction);
                gte_lddp(0x2BC);
                gte_ldsv(direction);
                gte_gpf12();
                gte_stsv(direction);
                coord->coord.t[0]   = vec.vx + 0x3E8;
                coord->coord.t[2]   = vec.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
            }
        }
    }
}

static s32 desertChaserAvoidWalk(GfxCoord* coord, WorldCollisionContact* recs, s16 count, SVECTOR* pos)
{
    DesertChaserAvoidScratch* s;
    s16                       diff;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1 || gGameSession->viewReady == 1) {
        return 0;
    }

    s = SCRATCH_STACK_RESERVE_BLOCK(DesertChaserAvoidScratch);

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

static __inline__ void Actor421600_BindMatrices(Task* actor)
{
    DesertChaserWork* work;
    TmdObject*        obj;
    work          = actor->work;
    obj           = actor->extra.tmd;
    obj->lightMtx = &work->lightMtx;
    obj->colorMtx = &work->colorMtx;
}

static void func_actor_421600_80134AD4(Enemy* enemy, Task* actor)
{
    SVECTOR             dir;
    s32                 kind;
    SVECTOR*            v;
    VECTOR              pos;
    TmdObject*          obj;
    GfxCoord*           root;
    DesertChaserWork*   work;
    WorldCollisionBody* body;
    WorldCollisionBody* head;
    root        = actor->extra.tmd->coords;
    obj         = actor->extra.tmd;
    work        = memCalloc(sizeof(DesertChaserWork), 0);
    actor->work = work;
    if (work == 0) {
        enemyDestroy(enemy, actor);
        return;
    }
    (sceneAcquireBattleRef)(0);
    actor->exitCallback = func_actor_421600_8013E668;
    Actor421600_BindMatrices(actor);
    enemy->field_4    = &actor->extra.tmd->coords[0].coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &actor->extra.tmd->coords[2];
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->reactionFlags          = 0;
    enemy->hp                     = (s16)D_actor_421600_8013EF38.hpMax;
    enemy->param                  = &D_actor_421600_8013EF38;
    enemy->recs                   = work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts;
    animationInitContext(&work->rig.anim, (AnimationSet**)D_actor_421600_80151028, obj, work->rig.poses, work->rig.slots);
    animationInitContext(&work->blend.anim, (AnimationSet**)D_actor_421600_80151028, obj, work->blend.poses, work->blend.slots);
    work->animRequest   = DESERT_CHASER_ANIM_REQUEST_RESET;
    work->blendActive   = 0;
    work->animId        = 1;
    work->lookYaw       = 0;
    work->lookYawTarget = 0;
    work->baseRate      = 0x10;
    work->animRate      = 0x10;
    _desertChaserAnimTick(actor);
    work->spheres[DESERT_CHASER_SPHERE_ROOT].body.context.contacts = work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts;
    work->spheres[DESERT_CHASER_SPHERE_ROOT].body.coord            = root;
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
    work->wallProbe.body.context.capsule                 = &work->wallProbe.shape;
    work->wallProbe.body.coord                           = root;
    work->wallProbe.body.pos.vx                          = 0;
    work->wallProbe.body.pos.vy                          = 0;
    work->wallProbe.body.pos.vz                          = 0;
    work->wallProbe.body.key                             = 0x30001;
    work->wallProbe.body.radius                          = 0;
    work->wallProbe.body.flags                           = WORLD_COLLISION_BODY_CAPSULE;
    work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->wallProbe.body);
    work->wallProbe.body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    worldCollisionInitContacts(work->wallProbe.contacts, ARRAY_SIZE(work->wallProbe.contacts), 0);
    worldCollisionInitContacts(work->spheres[DESERT_CHASER_SPHERE_ROOT].body.context.contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts), 0);
    body                   = &work->spheres[DESERT_CHASER_SPHERE_FRONT].body;
    body->coord            = &actor->extra.tmd->coords[2];
    body->context.contacts = work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts;
    body->pos.vx           = 0;
    body->pos.vy           = 0;
    body->pos.vz           = 0;
    body->key              = 0x30001;
    body->radius           = 0x19C;
    body->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, body);
    body->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(body->context.contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts), 0);
    head                   = &work->spheres[DESERT_CHASER_SPHERE_REAR].body;
    head->coord            = &actor->extra.tmd->coords[10];
    head->context.contacts = work->spheres[DESERT_CHASER_SPHERE_REAR].contacts;
    head->pos.vx           = 0;
    head->pos.vy           = 0;
    head->pos.vz           = 0;
    head->key              = 0x30001;
    head->radius           = 0x100;
    head->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, head);
    head->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(head->context.contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_REAR].contacts), 0);
    work->spheres[DESERT_CHASER_SPHERE_REAR].body.pos.vx = 0;
    work->spheres[DESERT_CHASER_SPHERE_REAR].body.pos.vy = 0;
    work->spheres[DESERT_CHASER_SPHERE_REAR].body.pos.vz = -0x100;
    work->patrolTarget                                   = 0;
    work->patrolPoints[0].x                              = actor->extra.tmd->coords->coord.t[0];
    work->patrolPoints[0].z                              = actor->extra.tmd->coords->coord.t[2];
    gfxReadMatrixZAxis(&actor->extra.tmd->coords->coord, &dir);
    dir.vy = 0;
    v      = &dir;
    VectorNormalSS(v, v);
    gte_lddp(5000);
    gte_ldsv(v);
    gte_gpf12();
    gte_stsv(v);
    work->patrolPoints[1].x               = actor->extra.tmd->coords->coord.t[0] + dir.vx;
    work->patrolPoints[1].z               = actor->extra.tmd->coords->coord.t[2] + dir.vz;
    work->playerAnim.blendFrames          = 3;
    work->playerAnim.source.sets          = 0;
    work->playerAnim.animationId          = 1;
    work->playerAnim.blend                = ANIMATION_BLEND_RESET;
    work->playerAnim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
    actor->msgTable                       = D_actor_421600_80151118;
    root->parent                          = &gGfxViewCoord;
    root->composeStamp                    = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(root);
    pos.vx = root->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = root->workm.t[2];
    worldCoordUpdateActorColor(enemy, &pos, 0, 0);
    kind = actor->spawnArg1.value >> 16;
    switch (kind & 0xF) {
        case 1:
            work->prevState = -1;
            work->state     = 0;
            break;

        case 2:
            work->prevState = -1;
            work->state     = 0x21;
            break;

        case 0:

        default:
            work->prevState = -1;
            work->state     = 0x18;
            tmdAllocPrimitiveBuffer(obj);
            break;
    }

    switch (actor->spawnArg1.value & 0xF) {
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

    gSceneCombatState.battleRefs = 8;
    D_actor_421600_80151268      = 8;
    actor->state++;
}

#include "../../shared/desert_chaser_hit_effect.inc.c"

static __inline__ s32 Actor421600_FindDamageHit(WorldCollisionContact* records,
                                                SVECTOR*               pos)
{
    s16 i;
    for (i = 0; i < 12; i++) {
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

static void func_actor_421600_801354D8(Task* arg0)
{
    s32                        callAngle;
    s32                        debugMode;
    PlayerStatus*              config = &gPlayerStatus;
    s16                        effect;
    s16                        delta;
    s16                        z;
    s32                        state5;
    s16                        damageState;
    s16                        deathState;
    s16                        poisonState;
    s16                        state0;
    s16                        state1;
    s16                        state2;
    s16                        state3;
    s16                        state4;
    s16                        wrapped;
    s16                        hitState;
    GfxCoord*                  objectCoord;
    s32                        tickDamage;
    s32                        dxSquared;
    s32                        dySquared;
    s32                        yaw;
    s32                        deathSound;
    s32                        hurtSound;
    s32                        hitSound;
    s32                        doubleDamage;
    s32                        dx;
    s32                        dy;
    s32                        dz;
    s32                        distance;
    SVECTOR*                   hitPos;
    s32                        soundBase;
    s32                        deathPan;
    s32                        hurtPan;
    s32                        hitPan;
    u16                        totalDamage;
    u32                        kind;
    DesertChaserWork*          work;
    Enemy*                     enemy;
    DesertChaserDamageScratch* scratch;
    DesertChaserDamageScratch* head;
    enemy = arg0->spawnArg2.pointer;
    work  = arg0->work;
    if (enemy->hp > 0) {
        head            = SCRATCH_STACK_CURSOR(DesertChaserDamageScratch);
        scratch         = (SCRATCH_STACK_CURSOR(DesertChaserDamageScratch) = head - 1);
        scratch->hitKey = Actor421600_FindDamageHit(
            work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts, &scratch->hitPos);
        if (scratch->hitKey == 0) {
            hitPos          = &scratch->hitPos;
            scratch->hitKey = Actor421600_FindDamageHit(work->spheres[DESERT_CHASER_SPHERE_REAR].contacts, hitPos);
        }
        if (scratch->hitKey != 0) {
            scratch->criticalEffect = -1;
            work->hitCooldown       = damageGetPlayerAttackHitCooldown(scratch->hitKey);
            kind                    = damageGetPlayerAttackReaction(scratch->hitKey) & 0xFFFF;
            switch (kind) {
                case DAMAGE_PLAYER_REACTION_NONE:
                case DAMAGE_PLAYER_REACTION_EXPLOSION:
                case DAMAGE_PLAYER_REACTION_INCENDIARY:
                case 8:
                case 9:
                    state0 = work->state;
                    if (state0 == 24 || state0 == 38 || state0 == 39 || state0 == 1) {
                        if (work->state == 0x20) {
                            work->state = 3;
                        } else {
                            work->state = 0x1C;
                        }
                    }
                    if (work->state == 0x21) {
                        work->state = 0x22;
                    }
                    if (work->blendActive == 0) {
                        work->recentDamage = 0U;
                    }
                    state1 = work->state;
                    if ((state1 != 0x22) && (state1 != 0x14) && (state1 != 0x11) &&
                        (state1 != 0x15) && (state1 != 0x16) && (state1 != 4) &&
                        (state1 != 0xB) && (state1 != 0x24) && (state1 != 7)) {
                        work->blendActive  = 1;
                        work->blendAnimId  = 9;
                        work->blendRequest = DESERT_CHASER_ANIM_REQUEST_RESET;
                    }
                    break;
                case 4:
                case 5:
                    state2 = work->state;
                    if (state2 == 4 || state2 == 11 || state2 == 20 || state2 == 17) {
                        work->state     = 11;
                        work->prevState = -1;
                    } else if (state2 != 21 && state2 != 7) {
                        work->state = 20;
                    }
                    break;
                case DAMAGE_PLAYER_REACTION_BUILDUP:
                    state3 = work->state;
                    if (state3 == 33 || state3 == 4 || state3 == 11 || state3 == 17) {
                        work->state     = 11;
                        work->prevState = -1;
                    } else if (state3 != 21 && state3 != 7) {
                        work->state = 20;
                    }
                    damageStartEnemyBuildup(enemy, scratch->hitKey, 0);
                    break;
                case DAMAGE_PLAYER_REACTION_POISON:
                    state4 = work->state;
                    if ((state4 == 0x18) || (state4 == 0x26) || (state4 == 1) ||
                        (state4 == 0x20)) {
                        work->state = 0x1C;
                    }
                    if (work->state == 0x21) {
                        work->state = 0x22;
                    }
                    damageTryStartEnemyDamageOverTime(enemy, scratch->hitKey, 0);
                    break;
                case DAMAGE_PLAYER_REACTION_STAGGER:
                    state5 = work->state;
                    if (state5 != 7) {
                        if (state5 == 4 || state5 == 11 || state5 == 20 || state5 == 17 ||
                            (state5 == 36 && work->stateTimer < 10)) {
                            hitState    = 11;
                            work->state = hitState;
                        } else if (state5 != 21 && state5 != 0 && state5 != 22 &&
                                   state5 != 7) {
                            hitState    = 20;
                            work->state = hitState;
                        }
                    }
                    break;
            }
            dx                                    = config->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0];
            dxSquared                             = dx * dx;
            scratch->toPlayer.vx                  = dx;
            dy                                    = config->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1];
            dySquared                             = dy * dy;
            scratch->toPlayer.vy                  = dy;
            dz                                    = config->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2];
            scratch->toPlayer.vz                  = dz;
            distance                              = SquareRoot0(dxSquared + dySquared + (dz * dz));
            scratch->playerDistance               = distance;
            scratch->damage                       = damageComputePlayerAttack(scratch->hitKey, distance, 0, 0);
            arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(arg0->extra.tmd->coords);
            scratch->hitOffset.vx = arg0->extra.tmd->coords->workm.t[0];
            scratch->hitOffset.vy = arg0->extra.tmd->coords->workm.t[1];
            scratch->hitOffset.vz = arg0->extra.tmd->coords->workm.t[2];
            scratch->hitOffset.vx = scratch->hitPos.vx - arg0->extra.tmd->coords->workm.t[0];
            scratch->hitOffset.vy = scratch->hitPos.vy - arg0->extra.tmd->coords->workm.t[1];
            z                     = scratch->hitPos.vz - arg0->extra.tmd->coords->workm.t[2];
            scratch->hitOffset.vz = z;
            yaw                   = ratan2(scratch->hitOffset.vx, z);
            objectCoord           = arg0->extra.tmd->coords;
            delta =
                yaw - ratan2(-objectCoord->workm.m[2][0], objectCoord->workm.m[2][2]);
            wrapped         = delta;
            scratch->hitYaw = delta;
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
            callAngle       = wrapped;
            scratch->hitYaw = callAngle;
            _desertChaserHitEffect(arg0, callAngle, scratch->hitKey);
            work->lookYaw       = 0;
            work->lookYawTarget = 0;
            if (damageRollCriticalHit(enemy, scratch->hitKey, 0) != 0) {
                scratch->criticalEffect = 0;
                scratch->damage         = scratch->damage * 4;
            }
            damageState = work->state;
            if ((damageState == 4) || (damageState == 0xB) || (damageState == 0x11) ||
                (damageState == 0x24)) {
                doubleDamage    = scratch->damage * 2;
                scratch->damage = doubleDamage;
                if (doubleDamage != 0) {
                    scratch->criticalEffect = 3;
                }
            }
            damageAccumulateLifeDrainHp(enemy, scratch->hitKey, scratch->damage, 0);
            effect = scratch->criticalEffect;
            if (effect != -1) {
                effectSpawn(EFFECT_CRITICAL_HIT, arg0->extra.tmd->coords + 2, (s32)(effect), 0);
            }
            scratch->damage = scratch->damage * 2;
            enemy->hp       = (s16)((u16)enemy->hp - (u16)scratch->damage);
            worldTargetAddReadoutAmount(&enemy->node, scratch->damage, 0);
            totalDamage        = work->recentDamage + (u16)scratch->damage;
            work->recentDamage = totalDamage;
            if (enemy->hp <= 0) {
                D_actor_421600_80151268 -= 1;
                if ((damageGetPlayerAttackReaction(scratch->hitKey) & 0xFFFF) == 4) {
                    work->state = 8;
                } else {
                    deathState = work->state;
                    if (deathState == 33 || deathState == 17 || deathState == 11 ||
                        deathState == 4) {
                        work->state     = 11;
                        work->prevState = -1;
                    } else if (deathState == 7) {
                        work->state = 21;
                        deathSound  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40010008;
                        deathPan    = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                        sndEvtRequestScriptStart(deathSound, deathPan,
                                                 (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                    } else {
                        hurtSound = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40010008;
                        hurtPan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                        sndEvtRequestScriptStart(hurtSound, hurtPan,
                                                 (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                        work->state = 20;
                    }
                }
                work->broadcast.context.loc.stage = 9;
                work->broadcast.context.loc.area  = 1;
                work->broadcast.command           = 3;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &work->broadcast, ACTOR_COMMAND_MESSAGE_APPLY);
            } else {
                if (((s16)totalDamage >= 0x47) && (work->state != 0x21) && (work->state != 0x14) &&
                    (work->state != 0x11) && (work->state != 7) &&
                    (work->lastCommand.fields.command != 1)) {
                    soundBase   = 0x40010008;
                    work->state = 0x14;
                } else {
                    soundBase = 0x40010007;
                }
                hitSound = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | soundBase;
                hitPan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                sndEvtRequestScriptStart(hitSound, hitPan,
                                         (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            }
            debugMode = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen;
            if (debugMode == 1) {
                enemy->hp          = 0x64;
                work->blendAnimId  = 9;
                work->blendActive  = (s16)debugMode;
                work->blendRequest = DESERT_CHASER_ANIM_REQUEST_RESET;
            }
        }
        if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            scratch->damage = damageTickEnemyDamageOverTime(enemy);
            if (damageIsEnemyDamageOverTimeExpired(enemy) != 0) {
                enemy->reactionFlags = (u8)(enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR);
            }
            enemy->hp  = (s16)((u16)enemy->hp - (u16)scratch->damage);
            tickDamage = scratch->damage;
            if (tickDamage != 0) {
                worldTargetAddReadoutAmount(&enemy->node, tickDamage, 0);
                if (enemy->hp <= 0) {
                    D_actor_421600_80151268 -= 1;
                    poisonState              = work->state;
                    if ((poisonState != 4) && (poisonState != 0xB) &&
                        (poisonState != 0x11)) {
                        work->state = 0xC;
                    } else {
                        work->state = 0x15;
                    }
                } else {
                    if (work->state == 0x1C) {
                        work->state = 0x26;
                    }
                    work->blendActive  = 1;
                    work->blendAnimId  = 0x12;
                    work->blendRequest = DESERT_CHASER_ANIM_REQUEST_RESET;
                }
            }
        }
        SCRATCH_STACK_RELEASE_BLOCK(DesertChaserDamageScratch);
    }
}

static void func_actor_421600_80135F6C(Task* arg0)
{
    SVECTOR           offset;
    DesertChaserWork* work;
    TmdObject*        obj;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->animRate                                       = 0x10;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        if (work->animId == 0xD) {
            work->animRequest = DESERT_CHASER_ANIM_REQUEST_BLEND;
        } else {
            work->animRequest = DESERT_CHASER_ANIM_REQUEST_RESET;
        }
        work->waistYawTarget = 0;
        work->lookYawTarget  = 0;
        _desertChaserAnimTick(arg0);
        return;
    }
    _desertChaserAnimTick(arg0);
    if ((work->rig.slots[1].status.fields.flags & 2) && (work->animId == 0xD)) {
        work->animId      = 1;
        work->animRequest = DESERT_CHASER_ANIM_REQUEST_BLEND;
    }
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        if (work->animId == 0xF) {
            work->animRequest = DESERT_CHASER_ANIM_REQUEST_RESET;
            work->animId      = 0x10;
        }
        _desertChaserAnimTick(arg0);
    }
    if (work->animId == 0xE) {
        if ((u32)((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) - 8) < 2U) {
            offset.vz = 0;
            offset.vx = 0;
            offset.vy = 0x2BC;
            if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                effectSpawn(EFFECT_DUST_PUFF, arg0->extra.tmd->coords + 7, 0x80002300, &offset);
            }
        }
        if ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) == 8) {
            offset.vz = 0;
            offset.vx = 0;
            offset.vy = 0x2BC;
            if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                effectSpawn(EFFECT_DUST_PUFF, arg0->extra.tmd->coords + 7, 0x80003400, &offset);
            }
        }
    }
}

static __inline__ s16 Actor421600_Zone(GfxCoord* coord)
{
    s32 x, z, ix, iz;
    x = coord->coord.t[0];
    z = coord->coord.t[2];
    if (x >= 0xD49)
        ix = 3;
    else if (x > 0)
        ix = 2;
    else
        ix = x >= -0xC7F;
    iz = 0;
    if (z < 0xBB9) {
        iz = 1;
        if (z <= 0) {
            iz = 3;
            if (z >= -0xBB7)
                iz = 2;
        }
    }
    return D_actor_421600_801511C0[ix | (iz * 4)];
}

static void func_actor_421600_80136138(Task* arg0)
{
    DesertChaserWork* work;
    ActorTurnScratch *head, *turn;
    Enemy*            ctx;
    TmdObject*        obj;
    s16               playerZone, zone;
    s16               nextZone;
    s16               angle;

    work = arg0->work;
    ctx  = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj                         = arg0->extra.tmd;
        ctx->node.state.parts.flags = 0;
        obj->flags                  = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->animRate                                       = 0x10;
        work->animId                                         = 0;
        work->animRequest                                    = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _desertChaserAnimTick(arg0);
        return;
    }
    playerZone = Actor421600_Zone(gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords);
    zone       = Actor421600_Zone(arg0->extra.tmd->coords);
    _actorContactApplyGridPushback(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts));
    if (playerZone == zone) {
        work->state = 0x26;
        return;
    }
    switch ((s16)(playerZone - 1)) {
        case 0:
        case 1:
            if (zone >= 1 && zone <= 3) {
                work->state = 0x26;
                return;
            }
            break;
        case 2:
            if (zone >= 1 && zone <= 6) {
                work->state = 0x26;
                return;
            }
            break;
        case 3:
        case 4:
            if (zone >= 3 && zone <= 6) {
                work->state = 0x26;
                return;
            }
            break;
        case 5:
            if (zone >= 3 && zone <= 9) {
                work->state = 0x26;
                return;
            }
            break;
        case 6:
        case 7:
            if (zone >= 6 && zone <= 9) {
                work->state = 0x26;
                return;
            }
            break;
        case 8:
            if (zone >= 6 && zone <= 11) {
                work->state = 0x26;
                return;
            }
            break;
        case 9:
        case 10:
            if (zone >= 9 && zone <= 11) {
                work->state = 0x26;
                return;
            }
            break;
        case 11:
            if (zone >= 0xB) {
                work->state = 0x26;
                return;
            }
            if (zone >= 0xC) {
                work->state = 0x26;
                return;
            }
            break;
    }
    _desertChaserAnimTick(arg0);
    head = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    turn = head - 1;
    if (zone > playerZone)
        nextZone = zone - 1;
    else
        nextZone = zone + 1;
    head[-1].delta.vx   = D_actor_421600_80151158[nextZone].vx;
    turn->delta.vy      = D_actor_421600_80151158[nextZone].vy;
    turn->delta.vz      = D_actor_421600_80151158[nextZone].vz;
    turn->delta.vx      = turn->delta.vx - (u16)arg0->extra.tmd->coords->coord.t[0];
    turn->delta.vy      = 0;
    turn->delta.vz      = turn->delta.vz - (u16)arg0->extra.tmd->coords->coord.t[2];
    angle               = _actorAngleTurnToOffset(arg0->extra.tmd->coords, turn->delta.vx, turn->delta.vz);
    turn->angle         = angle;
    work->lookYawTarget = angle;
    if (turn->angle >= 0x21)
        turn->angle = 0x20;
    if (turn->angle < -0x20)
        turn->angle = -0x20;
    work->waistYawTarget = turn->angle;
    turn->angle          = turn->angle + ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, turn->angle, 1);
    if (work->blendActive == 0) {
        _actorMovementStepForward(arg0->extra.tmd->coords, 0x14);
    }
    ActorContact_Steer(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts), &turn->delta);
    func_actor_421600_80133334(arg0->extra.tmd->coords);
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Rebuild `coord`'s Y rotation from its current yaw (`ratan2` of
/// `-m[2][0], m[2][2]`), scaled by `y` on Y and left at 1.0 on X and Z, through
/// an `ActorScaleRotScratch` block borrowed from the scratchpad. Marks the coordinate dirty.
static __inline__ void Actor421600_ShrinkCoord(GfxCoord* coord, s16 y)
{
    ActorScaleRotScratch* head;
    ActorScaleRotScratch* blk;
    s16                   ang;
    u16                   m22;

    head                                       = SCRATCH_STACK_CURSOR(ActorScaleRotScratch);
    blk                                        = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleRotScratch) = blk;

    ang      = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->yaw = ang;
    gfxRotMatrixY(&blk->rotation, ang, 1);
    blk->scale.vx = 0x1000;
    blk->scale.vy = y;
    blk->scale.vz = 0x1000;
    ScaleMatrix(&blk->rotation, &blk->scale);

    coord->coord.m[0][0] = (u16)(head - 1)->rotation.m[0][0];
    coord->coord.m[0][1] = (u16)blk->rotation.m[0][1];
    coord->coord.m[0][2] = (u16)blk->rotation.m[0][2];
    coord->coord.m[1][0] = (u16)blk->rotation.m[1][0];
    coord->coord.m[1][1] = (u16)blk->rotation.m[1][1];
    coord->coord.m[1][2] = (u16)blk->rotation.m[1][2];
    coord->coord.m[2][0] = (u16)blk->rotation.m[2][0];
    coord->coord.m[2][1] = (u16)blk->rotation.m[2][1];
    m22                  = (u16)blk->rotation.m[2][2];
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleRotScratch);
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    coord->coord.m[2][2] = m22;
}

/// Shrink tick: on the live-actor edge it drops the model's dirty flag, clears
/// the 0x4000 bit on the 0xB6C node, marks the enemy's list node and resets
/// `stateTimer` / `roomNotified`. Then it counts frames in `stateTimer` and, from frame
/// 0xB on, scales the model's coordinate Y by `0x1000 - (frame - 0xA) * 0x6B`
/// until that factor runs out at 0, through `Actor421600_ShrinkCoord`. The
/// frame counter also drives the light state: 1 sets modes 0 and 1, 20 (and
/// the fall-through from 1) sets mode 2, 38 sets `field_C` 0x80 and the
/// `state` 0x16. Counting stops at 0x401.
static void func_actor_421600_801366F4(Task* arg0)
{
    DesertChaserWork* work;
    Enemy*            ctx;
    TmdObject*        obj;
    s32               t;
    u16               tick;

    work = arg0->work;
    obj  = arg0->extra.tmd;
    ctx  = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj->flags                                           = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        ctx->node.state.parts.flags                          = WORLD_TARGET_NOT_LOCKABLE;
        work->stateTimer                                     = 0;
        work->roomNotified                                   = 0;
    }
    if (work->stateTimer < 0x401) {
        tick             = work->stateTimer + 1;
        work->stateTimer = tick;
        switch ((s16)tick) {
            case 1:
                worldCoordSetActorColorMode(ctx, ENEMY_COLOR_DEFAULT);
                worldCoordSetActorColorMode(ctx, ENEMY_COLOR_WEIGHTED);
                /* fallthrough */
            case 20:
                arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                worldCoordSetActorColorMode(ctx, ENEMY_COLOR_BLACK);
                break;
            case 22:
                break;
            case 38:
                arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                work->state            = 0x16;
                break;
        }
        if (work->stateTimer >= 0xB) {
            t = (work->stateTimer - 10) * 0x6B;
            if (t < 0x1000) {
                Actor421600_ShrinkCoord(arg0->extra.tmd->coords, 0x1000 - t);
            } else {
                Actor421600_ShrinkCoord(arg0->extra.tmd->coords, 0);
            }
        }
    }
}

static void func_actor_421600_801369A0(Task* arg0)
{
    DesertChaserWork* work;
    Enemy*            ctx;
    Enemy*            found;
    s32               hi;
    s32               id;
    s32               stageAreaId;

    work = arg0->work;
    ctx  = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        if (work->playerHeld == 1) {
            work->prevState  = -1;
            work->stateTimer = 0;
            return;
        }
        work->stateTimer = 0;
        do {
        } while (0);
        if (gSceneCombatState.battleRefs >= 2U) {
            sceneReleaseBattleRefWithRewards(arg0, 1);
        }
        if (D_actor_421600_80151268 <= 0) {
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, 0, 0);
            work->roomNotified = 1;
            work->state        = 0;
            return;
        }
    }
    if (work->stateTimer < 0x80) {
        work->stateTimer = work->stateTimer + 1;
    }
    if (work->roomEventCountdown > 0) {
        work->roomEventCountdown = work->roomEventCountdown - 1;
    }
    if (work->lastCommand.fields.command != 2) {
        work->state = 0;
        return;
    }
    found = NULL;
    switch (ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) {
        case 0:
            hi    = gGameSession->location.loc.stage << 8;
            id    = gGameSession->location.loc.area | 0x1000;
            found = sceneFindEnemyByPlaceKey(id | hi);
            break;
        case 1:
            stageAreaId = (gGameSession->location.loc.stage << 8) | gGameSession->location.loc.area;
            found       = sceneFindEnemyByPlaceKey(stageAreaId);
            break;
    }
    if (found != NULL) {
        if (found->hp > 0) {
            if (D_actor_421600_80151268 == 1) {
                work->state = 0;
            }
        }
        if ((D_actor_421600_80151268 >= 2) || ((found->hp <= 0) && (D_actor_421600_80151268 == 1))) {
            switch (ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) {
                case 0:
                    arg0->extra.tmd->coords->coord.t[0]   = -0xD40;
                    arg0->extra.tmd->coords->coord.t[2]   = 0x104F;
                    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(arg0->extra.tmd->coords);
                    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, -0x76C, 1);
                    worldCoordSetActorColorMode(ctx, ENEMY_COLOR_DEFAULT);
                    ctx->reactionFlags = 0;
                    ctx->hp            = D_actor_421600_8013EF38.hpMax;
                    work->state        = 6;
                    break;
                case 1:
                    arg0->extra.tmd->coords->coord.t[0]   = 0x138C;
                    arg0->extra.tmd->coords->coord.t[2]   = 0x4B2;
                    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(arg0->extra.tmd->coords);
                    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0x7BC, 1);
                    worldCoordSetActorColorMode(ctx, ENEMY_COLOR_DEFAULT);
                    ctx->reactionFlags = 0;
                    ctx->hp            = D_actor_421600_8013EF38.hpMax;
                    work->state        = 6;
                    break;
            }
        }
    }
}

#include "../../shared/desert_chaser_approach.inc.c"

#include "../../shared/desert_chaser_pursue.inc.c"

#include "../../shared/desert_chaser_spawn_aim.inc.c"

#include "../../shared/desert_chaser_strike.inc.c"

/// Aim tick: on the live-actor edge it re-arms the model the way
/// `_desertChaserThrowPlayer` does -- buffers reallocated, clip 0x10,
/// `animId` 6, the 0xB6C node's 0x4000 flag up -- with `stateCounter` and the
/// 0xCD8 offset it owns reseeded, then, while `stateTimer` is inside 9..0x18 and
/// `stateCounter` below 5, walks the 0xB8C table and counts a retry for every hit.
/// The 0xCE4 records decide which way the model is aimed: one carrying the
/// 0x100000 kind turns it by `-0x55`, none by `-0xC8`, through
/// `_actorMovementStepForward`. Outside that frame window, and in both aim arms,
/// the 0xB8C walk is what runs.
static void func_actor_421600_80138D24(Task* arg0)
{
    DesertChaserWork* work;
    Enemy*            ctx;
    TmdObject*        obj;
    s16               found;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                         = arg0->extra.tmd;
        ctx                         = arg0->spawnArg2.pointer;
        ctx->node.state.parts.flags = 0;
        obj->flags                  = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animRate                                        = 0x10;
        work->blendActive                                     = 0;
        work->animId                                          = 6;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _desertChaserAnimTick(arg0);
        work->stateTimer                 = 0;
        work->stateCounter               = 0;
        work->wallProbe.shape.ends[1].vz = -0x320;
    }
    work->stateTimer++;
    _desertChaserAnimTick(arg0);
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        work->state = 2;
    }
    if (((u32)((u16)work->stateTimer - 9) < 0x10) && (work->stateCounter < 5)) {
        if (_actorContactApplyGridPushback(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts)) != 0) {
            work->stateCounter++;
        }
        found = _desertChaserCapsuleTouchesGrid(arg0);
        if (found != 0) {
            _actorMovementStepForward(arg0->extra.tmd->coords, -0x55);
        } else {
            _actorMovementStepForward(arg0->extra.tmd->coords, -0xC8);
        }
    } else {
        _actorContactApplyGridPushback(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts));
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Re-arms the model the way `_desertChaserThrowPlayer` does -- buffers
/// reallocated, clip 0x10, `animId` 2, the 0xB6C node's 0x4000 flag up --
/// then applies horizontal grid pushback from the root sphere's contact table
/// through `_actorContactApplyGridPushback`.
/// Takes two `SVECTOR`s off the scratch stack and fills the XZ offset of the
/// model coordinate from `gPlayerStatus.coordMtx` (the player's coordinate matrix),
/// forms the yaw difference against the model's own facing (row 2 of its
/// matrix), wraps it into `[-0x800, 0x800]` into `lookYawTarget` and re-aims the
/// coordinate with `gfxRotMatrixY`. Ends by writing the view index into
/// `state` on the two view transitions.
///
/// The coordinate is read twice into two locals: `coord` only feeds the offset
/// and dies before the first `ratan2`, while `coord2` is live across it, so GCC
/// 2.8.1 keeps them in a caller-saved and a callee-saved register respectively.
/// One local assigned twice is one pseudo with one live range and costs a sixth
/// saved register.
static void func_actor_421600_8013903C(Task* arg0)
{
    DesertChaserWork* work;
    Enemy*            ctx;
    TmdObject*        obj;
    SVECTOR*          head;
    SVECTOR*          vec;
    GfxCoord*         coord;
    GfxCoord*         coord2;
    s16               angle;
    s32               view;

    head                           = SCRATCH_STACK_CURSOR(SVECTOR);
    SCRATCH_STACK_CURSOR(SVECTOR) -= 2;
    vec                            = head - 2;
    work                           = arg0->work;
    ctx                            = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj                         = arg0->extra.tmd;
        ctx->node.state.parts.flags = 0;
        obj->flags                  = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animRate                                        = 0x10;
        work->blendActive                                     = 0;
        work->animId                                          = 2;
        work->waistYawTarget                                  = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _desertChaserAnimTick(arg0);
        work->stateTimer = 0;
    }
    _actorContactApplyGridPushback(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts));
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    coord                                 = arg0->extra.tmd->coords;
    head[-2].vx                           = (u16)gPlayerStatus.coordMtx->t[0] - (u16)coord->coord.t[0];
    vec->vy                               = (u16)gPlayerStatus.coordMtx->t[1] - (u16)coord->coord.t[1];
    vec->vz                               = (u16)gPlayerStatus.coordMtx->t[2] - (u16)coord->coord.t[2];
    coord2                                = arg0->extra.tmd->coords;
    angle                                 = ratan2(head[-2].vx, vec->vz) - ratan2(-coord2->coord.m[2][0], coord2->coord.m[2][2]);
    angle                                 = _actorAngleNormalizeYaw(angle);
    work->lookYawTarget                   = angle;
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, (s16)ratan2(vec->vx, vec->vz), 1);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _desertChaserAnimTick(arg0);
    if (((u16)ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) == 0) {
        view = viewGetMappedIndex() & 0xFF;
        if (view == 3) {
            work->state = view;
        }
    }
    if ((((u16)ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) == 1) && ((viewGetMappedIndex() & 0xFF) == 8)) {
        work->state = 3;
    }
    SCRATCH_STACK_CURSOR(SVECTOR) += 2;
}

#include "../../shared/desert_chaser_steer.inc.c"

/// Pulls a position that left the arena back inside: X first, and Z only when
/// X was in range.
static __inline__ void Actor421600_ClampToArena(GfxCoord* coord)
{
    s32 x;
    s32 z;

    x = coord->coord.t[0];
    if (x > 0) {
        if (x >= 0xBEB) {
            coord->coord.t[0] = 0xB54;
            return;
        }
    } else if (x < -0xB22) {
        coord->coord.t[0] = -0xA8C;
        return;
    }
    z = coord->coord.t[2];
    if (z > 0) {
        if (z >= 0xB23) {
            coord->coord.t[2] = 0xA8C;
        }
    } else if (z < -0xB22) {
        coord->coord.t[2] = -0xA8C;
    }
}

/// Death / respawn tick: re-arms the model buffers and the 0x828 motion block,
/// fires the 0x40010009 spawn sound and the 0x40010007 tick sound (draining
/// `hp` by 0xF and flooring it at 1), then walks the two `WorldCollisionContact`
/// movement tables. While the last command received is 2 the actor is held
/// in the arena by clamping X -- and Z only when X was already inside -- and
/// otherwise `func_actor_421600_80133334` drags it back. Picks the state
/// `state` from `hp` and the buildup bit of `reactionFlags`.
static void func_actor_421600_8013947C(Task* arg0)
{
    DesertChaserWork* work;
    Enemy*            ctx;
    TmdObject*        obj;
    s32               sound;
    s32               pan;
    s32               eventSound;
    s32               eventPan;

    work = arg0->work;
    ctx  = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj                         = arg0->extra.tmd;
        ctx->node.state.parts.flags = 0;
        obj->flags                  = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->animRate                                        = 0x10;
        work->animId                                          = 0xA;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->blendActive                                     = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _desertChaserAnimTick(arg0);
        sound = (((u16)ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40010009;
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(sound, pan, (s32)(s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        ctx->hp -= 0xF;
        worldTargetAddReadoutAmount(&ctx->node, 0xF, 0);
        if (ctx->hp <= 0) {
            ctx->hp = 1;
        }
        eventSound = (((u16)ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40010007;
        eventPan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(eventSound, eventPan,
                                 (s32)(s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    _actorContactApplyGridPushback(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts));
    _actorContactApplyGridPushback(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts));
    if (work->lastCommand.fields.command == 2) {
        Actor421600_ClampToArena(arg0->extra.tmd->coords);
    } else {
        func_actor_421600_80133334(arg0->extra.tmd->coords);
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _desertChaserAnimTick(arg0);
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        if (ctx->hp > 0) {
            if (ctx->reactionFlags & ENEMY_REACTION_BUILDUP) {
                work->state = 4;
            } else {
                work->state = 0x11;
            }
        } else {
            work->state = 0x15;
        }
    }
}

#include "../../shared/desert_chaser_roam.inc.c"

static void func_actor_421600_8013A404(Task* arg0)
{
    DesertChaserWork* temp_s0;
    GfxCoord*         temp_v0_2;
    s32               temp_a0;
    s32               temp_a1;
    s32               var_a0;
    s32               var_v1;
    u32               temp_v0;
    u8                temp_v1;

    temp_s0 = arg0->work;
    if (temp_s0->stateEntered != 0) {
        temp_v0             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState     = temp_v0;
        temp_s0->stateTimer = temp_s0->downFramesBase + ((temp_v0 >> 0x10) & 0xF);
    }
    temp_s0->stateTimer -= 1;
    _desertChaserAnimTick(arg0);
    if ((s16)temp_s0->stateTimer < 0) {
        temp_v1 = temp_s0->lastCommand.fields.command;
        if ((temp_v1 == 1) || (temp_v1 == 3)) {
            temp_s0->state = 5;
        } else if (temp_v1 == 2) {
            temp_v0_2 = arg0->extra.tmd->coords;
            temp_a0   = temp_v0_2->coord.t[0];
            temp_a1   = temp_v0_2->coord.t[2];
            if (temp_a0 >= 0xD49) {
                var_a0 = 3;
            } else if (temp_a0 > 0) {
                var_a0 = 2;
            } else {
                var_a0 = temp_a0 >= -0xC7F;
            }
            var_v1 = 0;
            if (temp_a1 < 0xBB9) {
                var_v1 = 1;
                if (temp_a1 <= 0) {
                    var_v1 = 3;
                    if (temp_a1 >= -0xBB7) {
                        var_v1 = 2;
                    }
                }
            }
            if (D_actor_421600_801511C0[var_a0 | (var_v1 * 4)] >= 0xB) {
                temp_s0->state = 0x24;
            } else {
                temp_s0->state = 6;
            }
        } else {
            temp_s0->state = 0x24;
        }
    }
}

static void func_actor_421600_8013A554(Task* arg0)
{
    SVECTOR                   effect;
    s16                       aimZ;
    s16                       fallbackZ;
    s32                       fallbackAngle;
    s16                       fallbackDelta;
    s32                       moveAngle;
    s16                       moveDelta;
    s32                       playerX;
    s32                       facingAngle;
    s16                       facingDelta;
    s32                       aimAngle;
    s16                       aimDelta;
    s16                       targetZ;
    s16                       yaw;
    s16                       nextState;
    GfxCoord*                 targetCoord;
    GfxCoord*                 aimCoord;
    GfxCoord*                 fallbackCoord;
    GfxCoord*                 fallbackFacing;
    GfxCoord*                 moveCoord;
    GfxCoord*                 facingCoord;
    GfxCoord*                 aimFacing;
    GfxCoord*                 stepCoord;
    GfxCoord*                 coord;
    s32                       sound;
    s32                       spawnEffect;
    s32                       effectFlags;
    s32                       part;
    s32                       fallbackYaw;
    s32                       distance;
    s32                       closeDistance;
    s32                       farDistance;
    s32                       pan;
    TmdObject*                obj;
    Enemy*                    ctx;
    Task*                     player;
    DesertChaserWork*         work;
    Enemy*                    enemy;
    _Actor421600LungeScratch* scratch;

    work   = arg0->work;
    enemy  = arg0->spawnArg2.pointer;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (work->stateEntered != 0) {
        obj                         = arg0->extra.tmd;
        ctx                         = arg0->spawnArg2.pointer;
        ctx->node.state.parts.flags = 0;
        sceneEngageBattle(1);
        obj->flags = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animRate                                        = 0x10;
        work->blendActive                                     = 0;
        work->animId                                          = 3;
        work->waistYawTarget                                  = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags   = (u16)(work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        _desertChaserAnimTick(arg0);
        work->stateTimer    = 0;
        work->stateCounter  = 0;
        work->lungeDistance = 0;
        sound               = (((u16)ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40010006;
        pan                 = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(sound, pan, (s32)(s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        return;
    }
    scratch       = SCRATCH_STACK_RESERVE_BLOCK(_Actor421600LungeScratch);
    coord         = arg0->extra.tmd->coords;
    scratch->zone = Actor421600_Zone(coord);
    if ((_actorContactApplyGridPushback(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts)) != 0) && (work->stateTimer >= 0xB)) {
        work->state = 5;
    }
    if ((ActorContact_Steer(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts), &scratch->offset) << 0x10) != 0 && work->animId == 3) {
        work->playerButtonHold.pressCount = 8;
        if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &work->playerButtonHold, 0) == 0) {
            playerX                = -gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords->coord.m[2][0];
            scratch->playerYaw     = ratan2(playerX, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords->coord.m[2][2]);
            targetCoord            = arg0->extra.tmd->coords;
            scratch->offset.vx     = (s16)(gPlayerStatus.coordMtx->t[0] - targetCoord->coord.t[0]);
            scratch->offset.vy     = (s16)(gPlayerStatus.coordMtx->t[1] - targetCoord->coord.t[1]);
            targetZ                = gPlayerStatus.coordMtx->t[2] - targetCoord->coord.t[2];
            scratch->offset.vz     = targetZ;
            yaw                    = ratan2(scratch->offset.vx, targetZ) + 0x800;
            scratch->yawFromPlayer = yaw;
            scratch->yawFromPlayer = _actorAngleNormalizeYaw(yaw);
            facingCoord            = arg0->extra.tmd->coords;
            facingAngle            = ratan2(scratch->offset.vx, scratch->offset.vz);
            facingDelta            = facingAngle - ratan2(-facingCoord->coord.m[2][0], facingCoord->coord.m[2][2]);
            scratch->turn          = _actorAngleNormalizeYaw(facingDelta);
            distance               = scratch->yawFromPlayer - scratch->playerYaw;
            distance               = abs(distance);
            if (distance < 0x400) {
                work->playerAnim.source.sets = gDesertChaserFrontAnim;
            } else {
                work->playerAnim.source.sets = gDesertChaserRearAnim;
                scratch->yawFromPlayer       = (s16)((u16)scratch->yawFromPlayer + 0x800);
            }
            work->playerPlacement.rot.vx = 0;
            work->playerPlacement.rot.vy = (u16)scratch->yawFromPlayer;
            work->playerPlacement.rot.vz = 0;
            work->playerPlacement.pos.vx = (s32)player->extra.tmd->coords->coord.t[0];
            work->playerPlacement.pos.vy = (s32)player->extra.tmd->coords->coord.t[1];
            work->playerPlacement.pos.vz = (s32)player->extra.tmd->coords->coord.t[2];
            TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_PLACE, &work->playerPlacement, 0);
            if (work->lungeDistance < 0x3E8) {
                if (enemy->hp > 0) {
                    closeDistance = scratch->yawFromPlayer - scratch->playerYaw;
                    closeDistance = abs(closeDistance);
                    if (closeDistance < 0x400) {
                        scratch->playerKilled = taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, 2), 0);
                    } else {
                        scratch->playerKilled = taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, 3), 0);
                    }
                }
                if (scratch->playerKilled != 1) {
                    work->playerAnim.animationId         = 3;
                    work->playerAnim.blend               = ANIMATION_BLEND_RESET;
                    work->playerAnim.blendFrames         = 0;
                    work->playerMove.displacement.vx     = 0;
                    work->playerMove.displacement.vy     = 0;
                    work->playerMove.displacement.vz     = 0;
                    work->playerMove.collisionRequests   = GAME_ACTOR_COLLISION_REQUEST_MASK;
                    work->playerMove.keepControl         = 1;
                    work->playerHeld                     = 1;
                    work->lastCommand.fields.catchFrames = 0;
                    TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
                }
                nextState = 0x25;
            } else {
                if (enemy->hp > 0) {
                    farDistance = scratch->yawFromPlayer - scratch->playerYaw;
                    farDistance = abs(farDistance);
                    if (farDistance < 0x400) {
                        scratch->playerKilled = taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, 0), 0);
                    } else {
                        scratch->playerKilled = taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, 1), 0);
                    }
                }
                if (scratch->playerKilled == 1) {
                    ((GameActor*)player->work)->state = 0xA;
                }
                work->playerAnim.animationId         = 1;
                work->playerAnim.blend               = ANIMATION_BLEND_RESET;
                work->playerAnim.blendFrames         = 0;
                work->playerMove.displacement.vx     = 0;
                work->playerMove.displacement.vy     = 0;
                work->playerMove.displacement.vz     = 0;
                work->playerMove.collisionRequests   = GAME_ACTOR_COLLISION_REQUEST_MASK;
                work->playerMove.keepControl         = 1;
                work->playerHeld                     = 1;
                work->lastCommand.fields.catchFrames = 0;
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
                nextState = 0x1E;
            }
            work->state = nextState;
        }
        aimCoord               = arg0->extra.tmd->coords;
        scratch->offset.vx     = (s16)(gPlayerStatus.coordMtx->t[0] - aimCoord->coord.t[0]);
        scratch->offset.vy     = (s16)(gPlayerStatus.coordMtx->t[1] - aimCoord->coord.t[1]);
        aimZ                   = gPlayerStatus.coordMtx->t[2] - aimCoord->coord.t[2];
        scratch->offset.vz     = aimZ;
        aimFacing              = arg0->extra.tmd->coords;
        aimAngle               = ratan2(scratch->offset.vx, aimZ);
        aimDelta               = aimAngle - ratan2(-aimFacing->coord.m[2][0], aimFacing->coord.m[2][2]);
        scratch->playerBearing = _actorAngleNormalizeYaw(aimDelta);
    } else {
        fallbackCoord          = arg0->extra.tmd->coords;
        scratch->offset.vx     = (s16)(gPlayerStatus.coordMtx->t[0] - fallbackCoord->coord.t[0]);
        scratch->offset.vy     = (s16)(gPlayerStatus.coordMtx->t[1] - fallbackCoord->coord.t[1]);
        fallbackZ              = gPlayerStatus.coordMtx->t[2] - fallbackCoord->coord.t[2];
        scratch->offset.vz     = fallbackZ;
        fallbackFacing         = arg0->extra.tmd->coords;
        fallbackAngle          = ratan2(scratch->offset.vx, fallbackZ);
        fallbackDelta          = fallbackAngle - ratan2(-fallbackFacing->coord.m[2][0], fallbackFacing->coord.m[2][2]);
        fallbackYaw            = _actorAngleNormalizeYaw(fallbackDelta);
        scratch->playerBearing = (s16)fallbackYaw;
        fallbackYaw            = abs(fallbackYaw);
        if (fallbackYaw >= 0x601) {
            work->state = 0x1D;
        }
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    moveCoord                             = arg0->extra.tmd->coords;
    moveAngle                             = ratan2(work->playerDelta.vx, work->playerDelta.vz);
    moveDelta                             = moveAngle - ratan2(-moveCoord->coord.m[2][0], moveCoord->coord.m[2][2]);
    scratch->turn                         = _actorAngleNormalizeYaw(moveDelta);
    _desertChaserAnimTick(arg0);
    stepCoord = arg0->extra.tmd->coords;
    _actorMovementStepForward(stepCoord, 200);
    work->lungeDistance = (s16)((u16)work->lungeDistance + 0xC8);
    if (work->animId == 3) {
        switch (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) {
            case 5:
                spawnEffect = 1;
                part        = 7;
                effectFlags = 0x4300;
                effect.vz   = 0;
                effect.vx   = 0;
                effect.vy   = 700;
                break;
            case 8:
                spawnEffect = 1;
                part        = 9;
                effectFlags = 0x3500;
                effect.vz   = 0;
                effect.vx   = 0;
                effect.vy   = 700;
                break;
            case 10:
                spawnEffect = 1;
                part        = 14;
                effectFlags = 0x5A00;
                effect.vz   = 0;
                effect.vx   = 0;
                effect.vy   = 600;
                break;
            case 13:
                spawnEffect = 1;
                part        = 17;
                effectFlags = 0x4800;
                effect.vz   = 0;
                effect.vx   = 0;
                effect.vy   = 600;
                break;
            default:
                effectFlags = 0;
                spawnEffect = 0;
                part        = 0;
                break;
        }
        if ((gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) && (spawnEffect == 1)) {
            effectSpawn(EFFECT_DUST_PUFF, arg0->extra.tmd->coords + part, effectFlags | 0x80000000, &effect);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor421600LungeScratch);
}

static void func_actor_421600_8013B00C(Task* arg0)
{
    DesertChaserWork* work;
    ActorTurnScratch* head;
    ActorTurnScratch* turn;
    Enemy*            ctx;
    TmdObject*        obj;
    GfxCoord*         coord;
    s32               zone;
    s32               x_entry;
    s32               z_entry;
    s32               var_a0_entry;
    s32               var_v1_entry;
    Task*             task;
    GfxCoord*         playerCoord;
    s32               x;
    s32               z;
    s16               angle;
    s32               var_a0;
    s32               var_v1;

    work = arg0->work;
    task = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (work->stateEntered != 0) {
        obj                         = arg0->extra.tmd;
        ctx                         = arg0->spawnArg2.pointer;
        ctx->node.state.parts.flags = 0;
        obj->flags                  = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->animRate                                       = 0x10;
        work->animId                                         = 3;
        work->animRequest                                    = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _desertChaserAnimTick(arg0);
        playerCoord = task->extra.tmd->coords;
        x_entry     = playerCoord->coord.t[0];
        z_entry     = playerCoord->coord.t[2];
        if (x_entry >= 0xD49) {
            var_a0_entry = 3;
        } else if (x_entry > 0) {
            var_a0_entry = 2;
        } else {
            var_a0_entry = x_entry >= -0xC7F;
        }
        var_v1_entry = 0;
        if (z_entry < 0xBB9) {
            var_v1_entry = 1;
            if (z_entry <= 0) {
                var_v1_entry = 3;
                if (z_entry >= -0xBB7) {
                    var_v1_entry = 2;
                }
            }
        }
        if (D_actor_421600_801511C0[var_a0_entry | (var_v1_entry * 4)] >= 7) {
            work->fleeZone = 1;
            return;
        }
        work->fleeZone = 0xB;
        return;
    }
    coord = arg0->extra.tmd->coords;
    x     = coord->coord.t[0];
    z     = coord->coord.t[2];
    if (x >= 0xD49) {
        var_a0 = 3;
    } else if (x > 0) {
        var_a0 = 2;
    } else {
        var_a0 = x >= -0xC7F;
    }
    var_v1 = 0;
    if (z < 0xBB9) {
        var_v1 = 1;
        if (z <= 0) {
            var_v1 = 3;
            if (z >= -0xBB7) {
                var_v1 = 2;
            }
        }
    }
    zone = D_actor_421600_801511C0[var_a0 | (var_v1 * 4)];
    if ((s16)zone == work->fleeZone) {
        work->state = 0;
        return;
    }
    head = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    turn = head - 1;
    if ((s16)zone > work->fleeZone) {
        head[-1].delta.vx = D_actor_421600_80151158[zone - 1].vx;
        turn->delta.vy    = D_actor_421600_80151158[zone - 1].vy;
        turn->delta.vz    = D_actor_421600_80151158[zone - 1].vz;
    } else {
        head[-1].delta.vx = D_actor_421600_80151158[zone + 1].vx;
        turn->delta.vy    = D_actor_421600_80151158[zone + 1].vy;
        turn->delta.vz    = D_actor_421600_80151158[zone + 1].vz;
    }
    turn->delta.vx = turn->delta.vx - (u16)arg0->extra.tmd->coords->coord.t[0];
    turn->delta.vy = 0;
    turn->delta.vz = turn->delta.vz - (u16)arg0->extra.tmd->coords->coord.t[2];
    _desertChaserAnimTick(arg0);
    angle               = _actorAngleTurnToOffset(arg0->extra.tmd->coords, turn->delta.vx, turn->delta.vz);
    turn->angle         = angle;
    work->lookYawTarget = angle;
    if (turn->angle >= 0x81) {
        turn->angle = 0x80;
    }
    if (turn->angle < -0x80) {
        turn->angle = -0x80;
    }
    work->waistYawTarget = turn->angle;
    turn->angle          = turn->angle + ratan2(-arg0->extra.tmd->coords->coord.m[2][0], arg0->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, turn->angle, 1);
    if (work->blendActive == 0) {
        _actorMovementStepForward(arg0->extra.tmd->coords, 0xC8);
    }
    ActorContact_Steer(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts), &turn->delta);
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Zone-aim tick: the live-actor edge re-arms the model the way
/// `func_actor_421600_80138D24` does -- buffers reallocated, clip 0x10, pose 3,
/// motion 1, the 0xB6C node's 0x4000 flag up.
///
/// Otherwise the X and Z of the actor's coordinate are bucketed into the 4x4
/// zone table `D_actor_421600_801511C0` exactly as `func_actor_421600_8013A404`
/// does, and zone 5 abandons the tick into state 7. Any other zone picks the
/// neighbouring entry of the 8-byte pose table `D_actor_421600_80151158` --
/// `zone - 1` above the table's midpoint `mode`, `zone + 1` at or below it --
/// and copies all three halfwords into a 0xC block taken off the scratch stack,
/// which becomes the XZ direction from the actor to that pose.
///
/// `mode` and the `(s8)` casts on `zone` are load-bearing, and so is the
/// `turn->delta.vy = 0` between the two coordinate subtractions. A plain `5`
/// literal lets expand fold `zone > 5` into `zone < 6`, which drops the two
/// register copies and the `slt` the ROM has; keeping the limit in a
/// declaration-initialised `s8` leaves it a register operand so the fold never
/// runs. The midpoint store then lands in the load-delay slot the subtractions
/// leave open.
static void func_actor_421600_8013B4C4(Task* arg0)
{
    DesertChaserWork* work;
    ActorTurnScratch* head;
    ActorTurnScratch* turn;
    Enemy*            ctx;
    TmdObject*        obj;
    GfxCoord*         coord;
    GfxCoord*         coord2;
    GfxCoord*         coord3;
    GfxCoord*         coord4;
    s32               zone;
    s8                mode = 5;
    s32               v;
    s32               x;
    s32               z;
    s16               angle;
    s32               wrapped;
    s32               var_a0;
    s32               var_v1;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                         = arg0->extra.tmd;
        ctx                         = arg0->spawnArg2.pointer;
        ctx->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                  = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->animRate                                       = 0x10;
        work->animId                                         = 3;
        work->animRequest                                    = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _desertChaserAnimTick(arg0);
        return;
    }
    coord = arg0->extra.tmd->coords;
    x     = coord->coord.t[0];
    z     = coord->coord.t[2];
    if (x >= 0xD49) {
        var_a0 = 3;
    } else if (x > 0) {
        var_a0 = 2;
    } else {
        var_a0 = x >= -0xC7F;
    }
    var_v1 = 0;
    if (z < 0xBB9) {
        var_v1 = 1;
        if (z <= 0) {
            var_v1 = 3;
            if (z >= -0xBB7) {
                var_v1 = 2;
            }
        }
    }
    zone = D_actor_421600_801511C0[var_a0 | (var_v1 * 4)];
    if ((s8)zone == mode) {
        work->state = 7;
        return;
    }
    head = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    turn = head - 1;
    if ((s8)zone > mode) {
        head[-1].delta.vx = D_actor_421600_80151158[zone - 1].vx;
        turn->delta.vy    = D_actor_421600_80151158[zone - 1].vy;
        turn->delta.vz    = D_actor_421600_80151158[zone - 1].vz;
    } else {
        head[-1].delta.vx = D_actor_421600_80151158[zone + 1].vx;
        turn->delta.vy    = D_actor_421600_80151158[zone + 1].vy;
        turn->delta.vz    = D_actor_421600_80151158[zone + 1].vz;
    }
    turn->delta.vx = turn->delta.vx - (u16)arg0->extra.tmd->coords->coord.t[0];
    turn->delta.vy = 0;
    turn->delta.vz = turn->delta.vz - (u16)arg0->extra.tmd->coords->coord.t[2];
    _desertChaserAnimTick(arg0);
    coord2 = arg0->extra.tmd->coords;
    angle  = ratan2(turn->delta.vx, turn->delta.vz) - ratan2(-coord2->coord.m[2][0], coord2->coord.m[2][2]);
    if (angle < 0) {
    loop_neg:
        if (angle < -0x800) {
            angle += 0x1000;
            goto loop_neg;
        }
    } else {
    loop_pos:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto loop_pos;
        }
    }
    wrapped             = angle;
    turn->angle         = wrapped;
    work->lookYawTarget = wrapped;
    if (turn->angle >= 0x81) {
        turn->angle = 0x80;
    }
    if (turn->angle < -0x80) {
        turn->angle = -0x80;
    }
    work->waistYawTarget = turn->angle;
    coord3               = arg0->extra.tmd->coords;
    turn->angle          = turn->angle + ratan2(-coord3->coord.m[2][0], coord3->coord.m[2][2]);
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, turn->angle, 1);
    if (work->blendActive == 0) {
        coord4 = arg0->extra.tmd->coords;
        _actorMovementStepForward(coord4, 0xC8);
    }
    ActorContact_Steer(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts), &turn->delta);
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

static void func_actor_421600_8013B8E0(Task* arg0)
{
    DesertChaserWork* temp_s1;
    TmdObject*        temp_a0;

    temp_s1 = arg0->work;
    if (temp_s1->stateEntered != 0) {
        temp_a0                                                   = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        temp_a0->flags                                            = 0;
        tmdAllocPrimitiveBuffer(temp_a0);
        temp_s1->animRate                                       = 0x10;
        temp_s1->animId                                         = 0x11;
        temp_s1->animRequest                                    = DESERT_CHASER_ANIM_REQUEST_RESET;
        temp_s1->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        arg0->extra.tmd->coords->coord.t[0]                     = 0;
        arg0->extra.tmd->coords->coord.t[1]                     = 0;
        arg0->extra.tmd->coords->coord.t[2]                     = 0;
        arg0->extra.tmd->coords->composeStamp                   = GRAPHICS_COORD_DIRTY;
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0, 1);
        _desertChaserAnimTick(arg0);
    }
    _desertChaserAnimTick(arg0);
    if (temp_s1->rig.slots[1].status.fields.flags & 0x100) {
        arg0->extra.tmd->coords->coord.t[0]   = -0x334;
        arg0->extra.tmd->coords->coord.t[1]   = 0;
        arg0->extra.tmd->coords->coord.t[2]   = -0x4C4;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0x400, 1);
        temp_s1->animRequest = DESERT_CHASER_ANIM_REQUEST_RESET;
        temp_s1->animId      = 0;
        _desertChaserAnimTick(arg0);
        _desertChaserAnimTick(arg0);
        temp_s1->state = 0x27;
    }
}

static __inline__ s32 Actor421600_RouteZone(s32 x, s32 z)
{
    s32 ix = x > 0;
    s32 iz = z < 1;
    return D_actor_421600_801511D0[ix + (iz * 2)];
}

static void func_actor_421600_8013BA70(Task* arg0)
{
    s32                           radius = 0x5DC;
    DesertChaserWork*             work;
    Enemy*                        enemy;
    GfxCoord*                     zoneCoord;
    WorldCollisionContact*        record;
    GfxCoord*                     coord;
    GfxCoord*                     coord2;
    GfxCoord*                     coord3;
    GfxCoord*                     facing3;
    GfxCoord*                     facing4;
    GfxCoord*                     facing5;
    GfxCoord*                     facing;
    GfxCoord*                     facing2;
    GfxCoord*                     turnCoord;
    _Actor421600RouteRoamScratch* scratch;
    SVECTOR*                      target;
    SVECTOR*                      target2;
    _Actor421600RouteRoamScratch* head;
    _Actor421600RouteRoamScratch* head2;
    TmdObject*                    obj;
    s16                           targetDelta;
    s16                           delta;
    s16                           yaw;
    s16                           delta3;
    s16                           delta4;
    s16                           delta5;
    s32                           playerX;
    s16                           delta1;
    s16                           delta2;
    s16                           targetYaw;
    s16                           z;
    s32                           magnitude;
    s32                           targetMagnitude;
    s16                           adjustedDelta;
    s32                           originalMagnitude;
    s16                           wrapped;
    s16                           wrapped2;
    s16                           wrapped3;
    s16                           wrapped4;
    s16                           wrapped5;
    s16                           wrappedYaw;
    s32                           angle3;
    s32                           angle4;
    s32                           angle5;
    s32                           angle;
    s32                           angle2;
    s32                           finalYaw;
    s32                           turnDelta;
    s32                           finalDelta;
    s32                           yawDifference;
    u16                           unsignedDelta;
    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        head                          = SCRATCH_STACK_CURSOR(_Actor421600RouteRoamScratch);
        obj                           = arg0->extra.tmd;
        scratch                       = (SCRATCH_STACK_CURSOR(_Actor421600RouteRoamScratch) = head - 1);
        enemy->node.state.parts.flags = 0;
        obj->flags                    = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animRate                                        = 0x10;
        work->blendActive                                     = 0;
        work->animId                                          = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;

        _desertChaserAnimTick(arg0);
        _desertChaserAnimTick(arg0);
        work->stateTimer          = 0;
        work->stateCounter        = 0;
        zoneCoord                 = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
        scratch->playerQuadrant   = Actor421600_RouteZone(zoneCoord->coord.t[0], zoneCoord->coord.t[2]);
        coord                     = arg0->extra.tmd->coords;
        head[-1].toPatrolPoint.vx = (s16)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
        scratch->toPatrolPoint.vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
        z                         = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
        scratch->toPatrolPoint.vz = z;
        facing                    = arg0->extra.tmd->coords;
        angle                     = ratan2((s32)head[-1].toPatrolPoint.vx, (s32)z);
        delta1                    = angle - ratan2((s32)-facing->coord.m[2][0], (s32)facing->coord.m[2][2]);
        wrapped                   = delta1;
        if (delta1 < 0) {
            while (1) {
                if (wrapped < -0x800) {
                    wrapped += 0x1000;
                    continue;
                }
                break;
            }
        } else {
            while (1) {
                if (wrapped >= 0x801) {
                    wrapped -= 0x1000;
                    continue;
                }
                break;
            }
        }
        work->lookYawTarget = wrapped;
        work->patrolTarget  = 0;
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
        SCRATCH_STACK_RELEASE_BLOCK(_Actor421600RouteRoamScratch);
        work->wallProbe.shape.ends[1].vz = 0x26C;
        return;
    }
    work->stateCounter        += 1;
    head2                      = SCRATCH_STACK_CURSOR(_Actor421600RouteRoamScratch);
    scratch                    = (SCRATCH_STACK_CURSOR(_Actor421600RouteRoamScratch) = head2 - 1);
    head2[-1].toPatrolPoint.vx = (s16)(work->patrolPoints[work->patrolTarget].x - arg0->extra.tmd->coords->coord.t[0]);
    scratch->toPatrolPoint.vy  = 0;
    scratch->toPatrolPoint.vz  = work->patrolPoints[work->patrolTarget].z - arg0->extra.tmd->coords->coord.t[2];
    coord2                     = arg0->extra.tmd->coords;
    head2[-1].toPlayer.vx      = (s16)(gPlayerStatus.coordMtx->t[0] - coord2->coord.t[0]);
    target                     = &head2[-1].toPlayer;
    target->vy                 = gPlayerStatus.coordMtx->t[1] - coord2->coord.t[1];
    target->vz                 = gPlayerStatus.coordMtx->t[2] - coord2->coord.t[2];
    if (!actorOutsideRadius(&scratch->toPatrolPoint, 0xA0) || work->stateTimer >= 0x15) {
        facing2  = arg0->extra.tmd->coords;
        angle2   = ratan2((s32)head2[-1].toPlayer.vx, (s32)target->vz);
        delta2   = angle2 - ratan2((s32)-facing2->coord.m[2][0], (s32)facing2->coord.m[2][2]);
        wrapped2 = delta2;
        if (delta2 < 0) {
            while (1) {
                if (wrapped2 < -0x800) {
                    wrapped2 += 0x1000;
                    continue;
                }
                break;
            }
        } else {
            while (1) {
                if (wrapped2 >= 0x801) {
                    wrapped2 -= 0x1000;
                    continue;
                }
                break;
            }
        }
        work->lookYawTarget = wrapped2;
        if (work->patrolTarget == 0) {
            gfxRotMatrixY(&scratch->rotation, (s16)ratan2((s32)scratch->toPlayer.vx, (s32)scratch->toPlayer.vz) - 0x2EE, 1);
            work->patrolTarget = 1;
        } else {
            gfxRotMatrixY(&scratch->rotation, (s16)ratan2((s32)scratch->toPlayer.vx, (s32)scratch->toPlayer.vz) + 0x2EE, 1);
            work->patrolTarget = 0;
        }
        zoneCoord               = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
        scratch->playerQuadrant = Actor421600_RouteZone(zoneCoord->coord.t[0], zoneCoord->coord.t[2]);
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
        work->stateTimer = 0;
    }
    _desertChaserAnimTick(arg0);
    facing3  = arg0->extra.tmd->coords;
    angle3   = ratan2((s32)scratch->toPlayer.vx, (s32)scratch->toPlayer.vz);
    delta3   = angle3 - ratan2((s32)-facing3->coord.m[2][0], (s32)facing3->coord.m[2][2]);
    wrapped3 = delta3;
    if (delta3 < 0) {
        while (1) {
            if (wrapped3 < -0x800) {
                wrapped3 += 0x1000;
                continue;
            }
            break;
        }
    } else {
        while (1) {
            if (wrapped3 >= 0x801) {
                wrapped3 -= 0x1000;
                continue;
            }
            break;
        }
    }
    work->lookYawTarget = wrapped3;
    facing4             = arg0->extra.tmd->coords;
    angle4              = ratan2((s32)scratch->toPatrolPoint.vx, (s32)scratch->toPatrolPoint.vz);
    delta4              = angle4 - ratan2((s32)-facing4->coord.m[2][0], (s32)facing4->coord.m[2][2]);
    wrapped4            = delta4;
    if (delta4 < 0) {
        while (1) {
            if (wrapped4 < -0x800) {
                wrapped4 += 0x1000;
                continue;
            }
            break;
        }
    } else {
        while (1) {
            if (wrapped4 >= 0x801) {
                wrapped4 -= 0x1000;
                continue;
            }
            break;
        }
    }
    turnDelta         = wrapped4;
    scratch->fullTurn = (scratch->turn = (s16)turnDelta);
    delta             = scratch->turn;
    unsignedDelta     = (u16)scratch->turn;
    magnitude         = abs(scratch->turn);
    if (magnitude >= 0x601) {
        targetDelta     = work->lookYawTarget;
        targetMagnitude = abs(targetDelta);
        if ((targetMagnitude >= 0x101) && ((targetDelta * delta) < 0)) {
            adjustedDelta = unsignedDelta - 0x1000;
            if (delta < 0) {
                adjustedDelta = unsignedDelta + 0x1000;
            }
            scratch->turn = adjustedDelta;
        }
    }
    if (scratch->turn >= 0x21) {
        scratch->turn = 0x20;
    }
    if (scratch->turn < -0x20) {
        scratch->turn = -0x20;
    }
    work->waistYawTarget = scratch->turn * 0x10;
    turnCoord            = arg0->extra.tmd->coords;
    yaw                  = (u16)scratch->turn + ratan2((s32)-turnCoord->coord.m[2][0], (s32)turnCoord->coord.m[2][2]);
    scratch->turn        = yaw;
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, (s32)yaw, 1);
    record = work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts;
    if (work->blendActive == 0) {
        if (_desertChaserCapsuleTouchesGrid(arg0)) {
            _actorMovementStepForward(arg0->extra.tmd->coords, 20);
        } else {
            _actorMovementStepForward(arg0->extra.tmd->coords, 20);
        }
        record = work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts;
    }
    ActorContact_Steer(arg0->extra.tmd->coords, record, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts), &scratch->toPatrolPoint);
    if (_actorContactApplyGridPushback(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts)) == 1) {
        originalMagnitude = abs(scratch->fullTurn);
        if (originalMagnitude < 0x20) {
            work->stateTimer += 1;
        }
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    coord3                                = arg0->extra.tmd->coords;
    scratch->toPlayer.vx                  = (s16)(gPlayerStatus.coordMtx->t[0] - coord3->coord.t[0]);
    target2                               = &scratch->toPlayer;
    target2->vy                           = gPlayerStatus.coordMtx->t[1] - coord3->coord.t[1];
    target2->vz                           = gPlayerStatus.coordMtx->t[2] - coord3->coord.t[2];
    if (work->stateCounter > work->windupFrames) {
        if (work->chaseHoldoff <= 0) {

            if (actorOutsideRadius(&scratch->toPlayer, radius)) {
                if (!actorOutsideRadius(&scratch->toPlayer, 0x1F40) && work->stateCounter >= 0x1C3) {
                    facing5  = arg0->extra.tmd->coords;
                    angle5   = ratan2((s32)scratch->toPatrolPoint.vx, (s32)scratch->toPatrolPoint.vz);
                    delta5   = angle5 - ratan2((s32)-facing5->coord.m[2][0], (s32)facing5->coord.m[2][2]);
                    wrapped5 = delta5;
                    if (delta5 < 0) {
                        while (1) {
                            if (wrapped5 < -0x800) {
                                wrapped5 += 0x1000;
                                continue;
                            }
                            break;
                        }
                    } else {
                        while (1) {
                            if (wrapped5 >= 0x801) {
                                wrapped5 -= 0x1000;
                                continue;
                            }
                            break;
                        }
                    }
                    finalDelta    = wrapped5;
                    scratch->turn = (s16)finalDelta;
                    finalDelta    = abs(finalDelta);
                    if (finalDelta < 0x300) {
                        work->state = 0x1C;
                    }
                }
            } else {
                work->state = 0x1C;
            }
            playerX                = -(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0];
            scratch->playerYaw     = ratan2((s32)playerX, (s32)(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
            targetYaw              = ratan2((s32)scratch->toPlayer.vx, (s32)scratch->toPlayer.vz) + 0x800;
            wrappedYaw             = targetYaw;
            scratch->yawFromPlayer = targetYaw;
            if (targetYaw < 0) {
                while (1) {
                    if (wrappedYaw < -0x800) {
                        wrappedYaw += 0x1000;
                        continue;
                    }
                    break;
                }
            } else {
                while (1) {
                    if (wrappedYaw >= 0x801) {
                        wrappedYaw -= 0x1000;
                        continue;
                    }
                    break;
                }
            }
            finalYaw               = wrappedYaw;
            scratch->yawFromPlayer = (s16)finalYaw;
            yawDifference          = finalYaw - scratch->playerYaw;
            if (yawDifference < 0) {
                yawDifference = -yawDifference;
            }
            if (yawDifference >= 0x601 || worldTargetGetActorLockMask(&enemy->node) == 0) {
                work->state = 0x1C;
            }
        } else {
            work->chaseHoldoff -= 1;
        }
    }
    Actor421600_ClampToArena(arg0->extra.tmd->coords);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor421600RouteRoamScratch);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Death tick: the live-actor edge arms the model (dirty 0x80, clip 0x19C, the
/// 0xB6C node's 0x4000 flag down, the enemy's list node marked, the 0x83E /
/// 0x840 / 0x844 triple and `stateTimer` cleared) and spawns the 0x60030 effect on
/// the second coordinate. Frames 2, 3, 5, 7 and 8 then free the model buffers
/// and spawn one effect each -- 0xA0005 on coordinate 9, 12, 1 and 3 -- whose
/// model is tinted from the enemy's area record (`field_24` / `field_25`) and
/// re-streamed. Frame 0xA writes the 0x16 state. The counter stops at 0x400.
static void func_actor_421600_8013C8E0(Task* arg0)
{
    DesertChaserWork* work;
    Enemy*            ctx;
    TmdObject*        obj;
    SVECTOR           vec;

    work = arg0->work;
    ctx  = arg0->spawnArg2.pointer;
    obj  = arg0->extra.tmd;
    if (work->stateEntered != 0) {
        obj->flags                                            = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
        ctx->node.state.parts.flags                           = WORLD_TARGET_NOT_LOCKABLE;
        work->lookYaw                                         = 0;
        work->lookYawTarget                                   = 0;
        work->waistYawTarget                                  = 0;
        work->stateTimer                                      = 0;
        vec.vx                                                = 0x64;
        vec.vz                                                = 0;
        vec.vy                                                = 0;
        effectSpawn(EFFECT_030, arg0->extra.tmd->coords + 1, 0x10300, &vec);
    }
    if (work->stateTimer == 2) {
        obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        tmdFreePrimitiveBuffer(obj);
    }
    if (work->stateTimer == 3) {
        D_80114B34[5].data.model = &_gActor421600DesertChaserBurstLegRight;
        vec.vz                   = 0x64;
        vec.vy                   = 0;
        vec.vx                   = 0;
        actorTintEffect(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 9, 0x200, &vec), ctx);
    }
    if (work->stateTimer == 5) {
        D_80114B34[5].data.model = &_gActor421600DesertChaserBurstLegLeft;
        vec.vy                   = 0;
        vec.vx                   = 0;
        actorTintEffect(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 12, 0x200, &vec), ctx);
    }
    if (work->stateTimer == 7) {
        D_80114B34[5].data.model = &_gActor421600DesertChaserBurstTorso;
        actorTintEffect(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 1, 0x200, NULL), ctx);
    }
    if (work->stateTimer == 8) {
        D_80114B34[5].data.model = &_gActor421600DesertChaserBurstHead;
        actorTintEffect(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 3, 0x200, NULL), ctx);
    }
    if (work->stateTimer == 0xA) {
        work->state = 0x16;
    }
    if (work->stateTimer < 0x400) {
        work->stateTimer++;
    }
}

#include "../../shared/desert_chaser_turn_step.inc.c"

#include "../../shared/desert_chaser_turn_step_probe.inc.c"

static const DesertChaserStateTable D_actor_421600_80131EFC = { { func_actor_421600_8013E858,
                                                                  func_actor_421600_80135F6C,
                                                                  func_actor_421600_80136138,
                                                                  func_actor_421600_8013A554,
                                                                  _desertChaserStunned,
                                                                  func_actor_421600_8013B00C,
                                                                  func_actor_421600_8013B4C4,
                                                                  func_actor_421600_8013B8E0,
                                                                  func_actor_421600_8013C8E0,
                                                                  _desertChaserTurnRightState,
                                                                  _desertChaserTurnLeftState,
                                                                  _desertChaserStagger,
                                                                  _desertChaserCollapse,
                                                                  NULL,
                                                                  NULL,
                                                                  NULL,
                                                                  NULL,
                                                                  func_actor_421600_8013A404,
                                                                  NULL,
                                                                  NULL,
                                                                  _desertChaserKnockDown,
                                                                  func_actor_421600_801366F4,
                                                                  func_actor_421600_801369A0,
                                                                  NULL,
                                                                  desertChaserApproach,
                                                                  NULL,
                                                                  NULL,
                                                                  NULL,
                                                                  desertChaserPursue,
                                                                  func_actor_421600_8013E9D8,
                                                                  _desertChaserThrowPlayer,
                                                                  func_actor_421600_80138D24,
                                                                  func_actor_421600_8013903C,
                                                                  desertChaserSteer,
                                                                  func_actor_421600_8013EAAC,
                                                                  func_actor_421600_8013947C,
                                                                  func_actor_421600_8013EB7C,
                                                                  desertChaserStrike,
                                                                  desertChaserRoam,
                                                                  func_actor_421600_8013BA70 } };
static void                         func_actor_421600_8013D658(Enemy* enemy, Task* actor)
{
    PlayerStatus*          config;
    VECTOR                 pos;
    DesertChaserStateTable states;

    s16                       view;
    s16                       height;
    s16                       state;
    s16                       activeState;
    s16                       finalState;
    GfxCoord*                 playerCoord;
    GfxCoord*                 actorCoord;
    GfxCoord*                 actorCoord2;
    GfxCoord*                 playerCoord2;
    s32                       action;
    s32                       nextAction;
    s32                       result;
    u8                        kind;
    s32                       contactKind;
    AnimationSet**            nextPlayerSets;
    DesertChaserWork*         actorWork;
    DesertChaserWork*         actorWork2;
    DesertChaserWork*         work;
    Task*                     player;
    AnimationSet**            playerSets;
    Task*                     slot;
    Task*                     slot2;
    DesertChaserFrameScratch* scratch;
    AnimationPlayRequest*     message;
    AnimationPlayRequest*     nextMessage;

    work                                   = actor->work;
    player                                 = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    config                                 = &gPlayerStatus;
    view                                   = viewGetMappedIndex() & 0xFF;
    states                                 = D_actor_421600_80131EFC;
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(actor->extra.tmd->coords);
    pos.vx = actor->extra.tmd->coords->workm.t[0];
    pos.vy = actor->extra.tmd->coords->workm.t[1];
    pos.vz = actor->extra.tmd->coords->workm.t[2];
    worldCoordUpdateActorColor(enemy, &pos, 0, 0);
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->state != 0x15 && work->state != 0 && work->state != 0x16 && work->state != 7 && work->state != 8) {
                height = actor->extra.tmd->coords->coord.t[1];
                _limbShadowDrawSegment(actor, 1, 3, 0x12C, height, 0xFF);
                _limbShadowDrawSegment(actor, 3, 4, 0xC8, height, 0xFF);
                _limbShadowDrawSegment(actor, 1, 0xB, 0xFA, height, 0xFF);
                if ((viewGetMappedIndex() & 0xFF) == 0x13) {
                    actor->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                } else {
                    actor->extra.tmd->flags = 0;
                }
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            if (work->state != 0x15 && work->state != 0 && work->state != 0x16 && work->state != 7 && work->state != 8) {
                height = actor->extra.tmd->coords->coord.t[1];
                _limbShadowDrawSegment(actor, 1, 3, 0x12C, height, 0xFF);
                _limbShadowDrawSegment(actor, 3, 4, 0xC8, height, 0xFF);
                _limbShadowDrawSegment(actor, 1, 0xB, 0xFA, height, 0xFF);
                if ((viewGetMappedIndex() & 0xFF) == 0x13) {
                    actor->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                } else {
                    actor->extra.tmd->flags = 0;
                }
            }
            worldCollisionClearContacts(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts);
            worldCollisionClearContacts(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts);
            worldCollisionClearContacts(work->spheres[DESERT_CHASER_SPHERE_REAR].contacts);
            worldCollisionClearContacts(work->wallProbe.contacts);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            actor->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            worldCollisionClearContacts(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts);
            worldCollisionClearContacts(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts);
            worldCollisionClearContacts(work->spheres[DESERT_CHASER_SPHERE_REAR].contacts);
            worldCollisionClearContacts(work->wallProbe.contacts);
            return;
    }
    scratch = SCRATCH_STACK_RESERVE_BLOCK(DesertChaserFrameScratch);
    if (work->hitCooldown > 0) {
        work->hitCooldown = (s16)((u16)work->hitCooldown - 1);
    } else if (config->hp > 0) {
        func_actor_421600_801354D8(actor);
    }
    kind = work->lastCommand.fields.command;
    if ((kind == 2) && ((state = work->state, (state == 0x26)) || (state == kind))) {
        work->state = 0x27;
    }
    if (work->prevState != work->state) {
        work->stateEntered = 1;
    } else {
        work->stateEntered = 0;
    }
    work->prevState = (s16)(u16)work->state;
    scratch->zone   = Actor421600_Zone(actor->extra.tmd->coords);
    if (work->playerHeld == 1) {
        activeState = work->state;
        if ((activeState != 0x15) && (activeState != 0) && (activeState != 8)) {
            actorWork = actor->work;
            slot      = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            if ((slot != NULL) && (actorWork->playerMove.collisionRequests == GAME_ACTOR_COLLISION_REQUEST_MASK)) {
                playerCoord = slot->extra.tmd->coords;
                actorCoord  = actor->extra.tmd->coords;
                if (abs(playerCoord->coord.t[1] - actorCoord->coord.t[1]) >= 0x12D) {
                    playerCoord->coord.t[1]               = actorCoord->coord.t[1];
                    slot->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                }
            }
        }
        actorWork2 = actor->work;
        slot2      = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        if ((slot2 != NULL) && (actorWork2->playerMove.collisionRequests == GAME_ACTOR_COLLISION_REQUEST_MASK)) {
            playerCoord2 = slot2->extra.tmd->coords;
            actorCoord2  = actor->extra.tmd->coords;
            if (abs(playerCoord2->coord.t[1] - actorCoord2->coord.t[1]) >= 0x12D) {
                playerCoord2->coord.t[1]               = actorCoord2->coord.t[1];
                slot2->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            }
        }
        contactKind = work->lastCommand.fields.command;
        work->lastCommand.fields.catchFrames++;
        if (contactKind == 1) {
            if (D_dryfield_water_tower_801876AA == (D_dryfield_water_tower_801876A8 + 1)) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, NULL, 0);
                work->playerHeld = 0;
            }
            if ((work->lastCommand.fields.command == contactKind) && ((s32)D_dryfield_water_tower_801876AA < (D_dryfield_water_tower_801876A8 + 3))) {
                work->playerMove.displacement.vx = 0;
                work->playerMove.displacement.vy = 0;
                work->playerMove.displacement.vz = 0;
            }
        }
        action = work->playerAnim.animationId;
        switch (action) {
            case 4:
                break;
            case 1:
                if (TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_MOVE_BY, &work->playerMove, 0) == 1) {
                    work->playerMove.displacement.vx = 0;
                    work->playerMove.displacement.vy = 0;
                    work->playerMove.displacement.vz = 0;
                }
                if ((work->lastCommand.fields.catchFrames >= 0xFU) && (work->playerMove.collisionRequests == GAME_ACTOR_COLLISION_REQUEST_MASK)) {
                    work->playerMove.displacement.vx >>= 1;
                    work->playerMove.displacement.vy >>= 1;
                    work->playerMove.displacement.vz >>= 1;
                }
                break;
            case 2:
                playerSets = work->playerAnim.source.sets;
                if (playerSets == gDesertChaserRearAnim) {
                    if ((config->hp > 0) && (work->lastCommand.fields.catchFrames >= 0x17U)) {
                        message                      = &work->playerAnim;
                        playerSets[4]                = (Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon])->table.sets[7];
                        work->playerAnim.animationId = 4;
                        work->playerAnim.blend       = ANIMATION_BLEND_INTERPOLATE;
                        work->playerAnim.blendFrames = 3;
                        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, message, 0);
                        work->lastCommand.fields.catchFrames = 0U;
                    }
                } else if ((config->hp > 0) && (work->lastCommand.fields.catchFrames >= 0x22U)) {
                    message                   = &work->playerAnim;
                    gDesertChaserFrontAnim[4] = (Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon])->table.sets[7];

                    work->playerAnim.animationId = 4;
                    work->playerAnim.blend       = ANIMATION_BLEND_INTERPOLATE;
                    work->playerAnim.blendFrames = 3;
                    TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, message, 0);
                    work->lastCommand.fields.catchFrames = 0U;
                }
                break;
            case 3:
                if ((work->lastCommand.fields.catchFrames < 6U) && (config->hp > 0)) {
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
                if ((config->hp > 0) && (work->lastCommand.fields.catchFrames >= 7U)) {
                    taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 2, 0);
                    work->playerHeld = 0;
                }
                break;
        }
        if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, NULL, 0) == 0) {
            nextAction = work->playerAnim.animationId;
            switch (nextAction) {
                case 1:
                    if (((((GAME_LOCATION_WORD(gGameSession->location.loc)) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(4, 1, 0, 0)) || (work->playerMove.collisionRequests != (GAME_ACTOR_COLLISION_REQUEST_MASK << GAME_ACTOR_COLLISION_DISABLE_REQUEST_SHIFT))) && (config->hp > 0)) {
                        nextMessage                  = &work->playerAnim;
                        work->playerAnim.blend       = ANIMATION_BLEND_RESET;
                        work->playerAnim.blendFrames = 0;
                        work->playerAnim.animationId = 2;

                        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, nextMessage, 0);
                        work->lastCommand.fields.catchFrames = 0U;
                    }
                    break;
                case 3:
                    if (config->hp > 0) {
                        work->playerAnim.blend       = ANIMATION_BLEND_INTERPOLATE;
                        work->playerAnim.blendFrames = 6;
                        work->playerAnim.animationId = 5;
                        nextPlayerSets               = work->playerAnim.source.sets;
                        if (nextPlayerSets == gDesertChaserRearAnim) {
                            nextPlayerSets[5] = (Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon])->table.sets[9];
                        } else {
                            gDesertChaserFrontAnim[5] = (Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon])->table.sets[9];
                        }
                        nextMessage = &work->playerAnim;
                        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, nextMessage, 0);
                        work->lastCommand.fields.catchFrames = 0U;
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
    if (work->state == 1 && work->lastCommand.fields.command == 0) {
        if ((viewGetMappedIndex() & 0xFF) == 5)
            work->state = 2;
    }
    if (Actor421600_HasPlayerContact(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts)) {
        switch (view) {
            case 9:
            case 0x12:
                Actor421600_ClampToArena(player->extra.tmd->coords);
                break;
            case 0xE:
                break;
            default:
                func_actor_421600_80133334(player->extra.tmd->coords);
                break;
        }
    }
    finalState = work->state;
    if ((finalState != 0x15) && (finalState != 0) && (finalState != 5) && (finalState != 0x16) && (finalState != 8)) {
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
    scratch->bodyPos.vx = 0U;
    scratch->bodyPos.vy = 0;
    scratch->bodyPos.vz = 0;
    actorTransformToView(actor->extra.tmd->coords + 2, &scratch->bodyPos);
    enemy->bodyPos.vx = (s32)(s16)scratch->bodyPos.vx;
    enemy->bodyPos.vy = (s32)scratch->bodyPos.vy;
    enemy->bodyPos.vz = (s32)scratch->bodyPos.vz;
    enemy->coord      = &gGfxViewCoord;
    SCRATCH_STACK_RELEASE_BLOCK(DesertChaserFrameScratch);
    func_actor_421600_80133444(actor->extra.tmd->coords);
}

s32 func_actor_421600_8013E424(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
}

/// The enemy task's state handlers - spawn, per-frame tick and teardown - run by
/// `_desertChaserTask`.
static const DesertChaserTaskStates gDesertChaserTaskStates = {
    {
        func_actor_421600_80134AD4,
        func_actor_421600_8013D658,
        enemyDestroy,
    },
};

#include "../../shared/actor_messages_visibility.inc.c"

/// Handler for message 0x7D6: returns 1 while the enemy still has hit points
/// or its model is shown (flag 0x80 clear), 0 once it is dead and hidden.
s32 func_actor_421600_8013E4EC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (((Enemy*)task->spawnArg2.pointer)->hp <= 0 && (task->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) != 0) {
        return 0;
    }
    return 1;
}

#include "../../shared/actor_messages_place_yaw_first.inc.c"

#include "../../shared/desert_chaser_play_anim.inc.c"

s32 func_actor_421600_8013E654(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    DesertChaserWork* work = task->work;

    work->roomEventCountdown = 0x1E;
    return 1;
}

/// `Task::exitCallback` teardown: kill the two helper tasks, unlink the three
/// display nodes, clear the enemy's `recs`, then `enemyDestroy`.
static void func_actor_421600_8013E668(Task* task)
{
    DesertChaserWork* work;
    Enemy*            enemy;

    work  = task->work;
    enemy = (Enemy*)task->spawnArg2.pointer;
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
        enemy->recs = 0;
    }
    enemyDestroy(enemy, task);
}

#include "../../shared/desert_chaser_part_effect.inc.c"

/// Copy the `vx`/`vy`/`vz` of entry `arg1` of the pose table into `arg0`.
static void func_actor_421600_8013E7F8(SVECTOR* arg0, s32 arg1)
{
    arg0->vx = D_actor_421600_80151158[(s16)arg1].vx;
    arg0->vy = D_actor_421600_80151158[(s16)arg1].vy;
    arg0->vz = D_actor_421600_80151158[(s16)arg1].vz;
}

static s8 func_actor_421600_8013E830(s32 arg0, s32 arg1)
{
    s8* p;
    s32 a;
    s32 b;

    p = D_actor_421600_801511D0;
    a = arg0 > 0;
    b = arg1 < 1;
    return p[a + (b << 1)];
}

static void func_actor_421600_8013E858(Task* arg0)
{
    TmdObject*        obj;
    DesertChaserWork* work;
    Enemy*            enemy;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                  = arg0->extra.tmd;
        enemy                                                = arg0->spawnArg2.pointer;
        enemy->node.state.parts.flags                        = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                                          |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->hp                                            = 0;
    }
}

#include "../../shared/desert_chaser_stunned.inc.c"

/// State handler: on the live-actor edge (`stateEntered` set) show the model, start
/// animation 4 and step it once; afterwards step the animation and, once its
/// flag 0x100 is up, go to state 5 when the last command received is
/// `DESERT_CHASER_COMMAND_WATER_TOWER_1`, else 2.
static void func_actor_421600_8013E9D8(Task* arg0)
{
    TmdObject*        obj;
    DesertChaserWork* work;
    s32               state;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animRate                                        = 0x10;
        work->blendActive                                     = 0;
        work->animId                                          = 4;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _desertChaserAnimTick(arg0);
        return;
    }
    _desertChaserAnimTick(arg0);
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        state = work->lastCommand.word & DESERT_CHASER_COMMAND_MASK;
        if (state == DESERT_CHASER_COMMAND_WATER_TOWER_1) {
            state = 5;
        } else {
            state = 2;
        }
        work->state = state;
    }
}

static void func_actor_421600_8013EAAC(Task* arg0)
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
        work->animRate                                        = 0x10;
        work->blendActive                                     = 0;
        work->animId                                          = 8;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _desertChaserAnimTick(arg0);
    }
    _actorContactApplyGridPushback(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts));
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _desertChaserAnimTick(arg0);
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        work->state = 0x1C;
    }
}

static void func_actor_421600_8013EB7C(Task* arg0)
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
        work->animRate                                        = 0x10;
        work->blendActive                                     = 0;
        work->animId                                          = 0xC;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _desertChaserAnimTick(arg0);
    }
    _desertChaserAnimTick(arg0);
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        work->state = 2;
    }
}

#include "../../shared/desert_chaser_flinch.inc.c"

#include "../../shared/desert_chaser_stagger.inc.c"

#include "../../shared/desert_chaser_collapse.inc.c"

#include "../../shared/desert_chaser_task.inc.c"
