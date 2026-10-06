#include "actor_510900_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

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
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
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
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
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

#include "rooms/acropolis_helicopter_landing_pad.h"
#include "../../shared/actor_messages.h"
#include "../../shared/no9_golem.h"

s32 func_actor_510900_801391B8(Task*, s32, s32, s32);

s32 func_actor_510900_8013BD5C(Task*, s32, s32, s32);

s32 func_actor_510900_8013BD84(Task*, s32, AnimationPlayRequest*, s32);

s32 func_actor_510900_8013BE64(Task*, s32, s32, s32);

s32 func_actor_510900_8013BE84(Task*, s32, s32, s32);

// Only the leading view ID is read; retain the following halfwords.
extern u16 D_actor_510900_80167CE4[4];

/// Scratch block of the step that carries the golem's three wall faces in the
/// room's collision grid to where the golem stands.
///
/// The faces are stored as twelve corners around an origin of their own, which
/// lies at a fixed offset from the golem in the golem's axes. The step turns
/// that offset by the golem's facing to place the origin in the world, then
/// turns each corner the same way and adds the origin to it. Reserve one
/// complete block and release it when the step is done. The fourth halfword of
/// either vector is never written.
typedef struct {
    SVECTOR origin;  // The faces' origin: first its offset from the golem in the golem's axes, then its world position, held within 0x1770 of the world origin in X and Z
    SVECTOR rotated; // Latest vector turned by the golem's facing: the origin's offset, then each corner in turn
} _Actor510900GridFacesScratch;
STATIC_ASSERT_SIZEOF(_Actor510900GridFacesScratch, 0x10);

/// Values of `_Actor510900HelipadLightWork::state`. Light 2 also copies its
/// state to `Actor510900Work::light2State` every frame.
enum {
    ACTOR_510900_HELIPAD_LIGHT_INTACT   = 0, // Whole: a shot or the golem's slash breaks it
    ACTOR_510900_HELIPAD_LIGHT_SPARKING = 1, // Broken and throwing sparks; its blast can strike once
    ACTOR_510900_HELIPAD_LIGHT_DONE     = 2, // The sparks have run out, or the golem is gone
};

/// Values of `_Actor510900HelipadLightWork::status`.
enum {
    ACTOR_510900_HELIPAD_LIGHT_STATUS_INTACT   = 0, // Not broken yet
    ACTOR_510900_HELIPAD_LIGHT_STATUS_SPARKING = 1, // Broken, sparks still running
    ACTOR_510900_HELIPAD_LIGHT_STATUS_SPENT    = 2, // Broken, sparks over
    ACTOR_510900_HELIPAD_LIGHT_STATUS_STOPPED  = 3, // Never broken: the golem was gone first
};

/// Work block of one of the three helipad lights, kept at `Task::work` of the
/// light model's task.
///
/// A light stands intact until the player shoots it or the golem's slash
/// reaches it. It then breaks: the model changes to its second animation, a
/// spark effect starts, and for as long as the sparks last a blast sphere
/// around the light can strike the golem, which drives it back in a shower of
/// sparks. The blast is switched off at its first contact. A light runs only
/// while the camera is in one of its three views.
///
/// The block is cleared at allocation.
typedef struct {
    AnimationContext      anim;                                   // Context bound to `slots`, `poses` and the model's part coordinates
    AnimationSlot         slots[11];                              // Playback state of the model part at the same index; slots 1..10 are driven
    u8                    poses[11][ANIMATION_POSE_BUFFER_BYTES]; // Encoded transition pose of the slot at the same index
    MATRIX                colorMtx;                               // Colour matrix lent to the light's model
    MATRIX                lightMtx;                               // Light matrix lent to the same model
    WorldCollisionBody    body;                                   // Sphere of radius 0xC8 on model part 10, on list 2, receiving attacks; enabled only while the light is intact and in view
    WorldCollisionContact bodyContacts[1];                        // Contact of `body`, read and cleared every frame the light is intact
    WorldCollisionBody    blast;                                  // Sphere of radius 0x15E on the same part, on list 8, keyed 0x50002; enabled from the break until its first contact or the end of the sparks
    WorldCollisionContact blastContacts[1];                       // Contact of `blast`
    Task*                 sparksTask;                             // Spark effect started at the break and adopted as a child of the light's task; `NULL` before the break, if it did not spawn, or once it has been told to stop
    s16                   state;                                  // `ACTOR_510900_HELIPAD_LIGHT_*`
    s16                   sparkFrames;                            // Frames of sparks left, 0x78 at the break; a frame the camera spends in another view also takes one off
    s16                   lightIndex;                             // Which of the three lights this is (0..2), from the spawn argument; picks its place, facing and views
    s16                   status;                                 // `ACTOR_510900_HELIPAD_LIGHT_STATUS_*`, published every frame in view: by lights 0 and 1 to game-flag nibbles 0xB and 0xC, by light 2 to `Actor510900Work::light2Status`
} _Actor510900HelipadLightWork;
STATIC_ASSERT_SIZEOF(_Actor510900HelipadLightWork, 0x338);

/// Values of `_Actor510900GrenadeWork::phase` while the grenade flies.
enum {
    ACTOR_510900_GRENADE_FLIGHT_BELOW_PEAK = 0, // Has not been above height 0x514 yet
    ACTOR_510900_GRENADE_FLIGHT_PEAKED     = 1, // Has been above it: dropping back below bursts the grenade
};

/// Values of `_Actor510900GrenadeWork::phase` once the grenade has burst.
enum {
    ACTOR_510900_GRENADE_BURST_SPREADING = 0, // 0x10 frames with the attack sphere off, then it widens to radius 0x258
    ACTOR_510900_GRENADE_BURST_CATCHING  = 1, // Up to 0x1F frames in which the widened sphere can catch the player
    ACTOR_510900_GRENADE_BURST_HOLDING   = 2, // The player is caught: `holdStep` runs
    ACTOR_510900_GRENADE_BURST_ENDING    = 3, // 0x1F frames, then the grenade's task ends
};

/// Values of `_Actor510900GrenadeWork::holdStep`.
enum {
    ACTOR_510900_GRENADE_HOLD_REQUEST = 0, // Asks the player for the hold; on acceptance deals the damage and starts the stunned animation
    ACTOR_510900_GRENADE_HOLD_STUNNED = 1, // Until the player has escaped, 0x3C frames have passed or the golem is gone; then starts the recovery animation
    ACTOR_510900_GRENADE_HOLD_RECOVER = 2, // 0x14 frames, then waits for the animation to end and releases the player
};

/// Work block of the golem's stun grenade, kept at `Task::work` of the
/// grenade model's task.
///
/// The grenade leaves the golem's chest pitching forward a fixed step every
/// frame while it moves along its own axis, so the step sets how far the arc
/// carries; it is chosen from the distance to the player at the throw. It
/// bursts when it comes back down through height 0x514, touches the player or
/// meets the room's collision grid. The burst then spreads into a wider sphere
/// that holds a player it catches until they have pressed enough buttons, the
/// hold times out or the golem is gone.
///
/// The block is cleared at allocation.
typedef struct {
    MATRIX                colorMtx;             // Colour matrix lent to the grenade's model
    MATRIX                lightMtx;             // Light matrix lent to the same model
    WorldCollisionBody    attack;               // Sphere on list 3 at the grenade: radius 0xC8 in flight, 0x258 once the burst has spread; a player contact bursts the grenade or starts the hold
    WorldCollisionContact attackContacts[1];    // Contact of `attack`
    WorldCollisionBody    gridProbe;            // Capsule on list 3 tested against the room's collision grid only; unlinked at the burst
    WorldCollisionCapsule gridProbeCapsule;     // Shape of `gridProbe`: 0x1F4 along the grenade's Y axis from its origin, radius 1
    WorldCollisionContact gridProbeContacts[1]; // Contact of `gridProbe`; any contact bursts the grenade
    u16                   frames;               // Frames counted in the current phase; in flight, frames since the last smoke puff of the trail (one every third frame)
    s16                   phase;                // `ACTOR_510900_GRENADE_FLIGHT_*` in flight, `ACTOR_510900_GRENADE_BURST_*` after the burst
    s16                   holdStep;             // `ACTOR_510900_GRENADE_HOLD_*`
    s16                   phaseCounter;         // In flight, the pitch added each frame (4096 a turn); during the hold, frames spent in `holdStep`
} _Actor510900GrenadeWork;
STATIC_ASSERT_SIZEOF(_Actor510900GrenadeWork, 0xD0);

/// Animation-set table handed to the player as the 0x3FF payload's `source.sets`.
extern AnimationSet* D_actor_510900_80167B2C[];

/// Spawn position of the child, indexed by its `Task::spawnArg1`.
extern SVECTOR D_actor_510900_80167CB8[];
/// Spawn rotation about Y, indexed the same way.
extern u16 D_actor_510900_80167CD0[];
/// Animation set table `animationInitContext` installs in the context above.
extern AnimationSet* D_actor_510900_80167CAC[];

/// `_Actor510900GrenadeWork::phaseCounter` of the flight, the pitch added each
/// frame, per 1000 units of distance between the grenade and the player,
/// clamped to the last entry.
extern u16 D_actor_510900_80167C94[12];

/// Values of `_Actor510900BlastSourceWork::state`.
enum {
    ACTOR_510900_BLAST_SOURCE_DORMANT = 0, // Waiting for helipad light 2 to break
    ACTOR_510900_BLAST_SOURCE_ARMED   = 1, // Can be shot
    ACTOR_510900_BLAST_SOURCE_SHOT    = 2, // The frame after the shot: the flare is given its starting state
    ACTOR_510900_BLAST_SOURCE_FLARING = 3, // `flareFrames` runs down
    ACTOR_510900_BLAST_SOURCE_SPENT   = 4, // The flare has been told to end
    ACTOR_510900_BLAST_SOURCE_STOPPED = 5, // The golem is gone and the flare released
};

/// Work block of the blast source beside helipad light 2, kept at
/// `Task::work` of a task that has a coordinate and no model.
///
/// The source lies dormant until light 2 breaks. A shot then sets it off: a
/// flare starts, and for as long as it burns a blast sphere can strike the
/// golem once - stunning it in a shower of sparks while light 2 is still
/// sparking, driving it back along the lap once the light is spent. The
/// source runs in one view only and gives off embers there the whole time.
/// What the source is in the scene is unproven.
///
/// The block is cleared at allocation. No access to `pad_7A` has been
/// observed; whether it is a member or tail padding is unproven.
typedef struct {
    WorldCollisionBody    body;             // Sphere of radius 0x12C at the source on list 2, receiving attacks; enabled only while armed and in view
    WorldCollisionContact bodyContacts[1];  // Contact of `body`, read and cleared every frame the source is armed
    WorldCollisionBody    blast;            // Sphere of radius 0x200 raised 0x200 above the source, on list 8; keyed 0x50003 or 0x50004 by the shot and enabled from it until its first contact or the end of the flare
    WorldCollisionContact blastContacts[1]; // Contact of `blast`
    Task*                 flareTask;        // Flare effect started by the shot and adopted as a child of the source's task; `NULL` before the shot, if it did not spawn, or once it has been released
    s16                   state;            // `ACTOR_510900_BLAST_SOURCE_*`
    s16                   flareFrames;      // Frames the flare still burns, 0x3C at the shot; a frame the camera spends in another view also takes one off
    s16                   flareTaskState;   // `Task::state` the flare starts in, chosen at the shot (1 if light 2 was already spent, 0 if it was still sparking)
    byte                  pad_7A[0x2];
} _Actor510900BlastSourceWork;
STATIC_ASSERT_SIZEOF(_Actor510900BlastSourceWork, 0x7C);

/// Table the state 1 handler below picks `stateCounter` from; a 4-bit
/// `gRandomLcgState` draw indexes at least sixteen `u16` entries.
extern u16 D_actor_510900_801679F0[];

/// Table the 0x14-animation state picks `stateCounter` from, indexed by a 4-bit
/// `gRandomLcgState` draw the same way `D_actor_510900_801679F0` is.
extern u16 D_actor_510900_801679D0[];

/// Per-cycle threshold the patrol walk in `func_actor_510900_80136B70` compares
/// a 4-bit `gRandomLcgState` draw against, indexed by the `stateCounter` cycle counter;
/// a draw above the entry ends the walk.
extern s16 D_actor_510900_80167A10[];

/// Per-animation-id value `func_actor_510900_8013BB20` hands `animationSeekSlotWithBlend`
/// as its fifth argument when it restarts animation slots 1..18.
extern s16 D_actor_510900_80167B38[];

/// One corner of the square lap the golem walks, in world X and Z. A side of
/// the lap starts at its corner.
typedef struct {
    s16 x; // World X of the corner
    s16 z; // World Z of the corner
} _Actor510900LapCorner;
STATIC_ASSERT_SIZEOF(_Actor510900LapCorner, 0x4);

/// The direction one side of the lap runs in, from its corner to the next:
/// the world X and Z the golem covers per unit of distance along the side.
/// Every side is parallel to an axis, so one member is 1 or -1 and the other 0.
typedef struct {
    s8 x; // World X covered per unit of distance along the side
    s8 z; // World Z covered per unit of distance along the side
} _Actor510900LapDirection;
STATIC_ASSERT_SIZEOF(_Actor510900LapDirection, 0x2);

/// The four corners of the lap, indexed by `Actor510900Work::lapSide`.
extern _Actor510900LapCorner D_actor_510900_80167B84[4];

/// The direction of each of its four sides, indexed the same way.
extern _Actor510900LapDirection D_actor_510900_80167B94[4];

/// `func_800FDB18` argument record the state-3 handler refreshes every sixth
/// frame from the player's model coordinates.
extern EffectSpawnArg D_actor_510900_80167B7C;

/// The yaws `func_actor_510900_801387F4` turns the actor's coordinate
/// towards, indexed by `Actor510900Work::lapSide` (one entry further on
/// while `sideRemaining` is below 0x3E8).
extern u16 D_actor_510900_80167B9C[];

/// The strip of the landing pad that runs along one side of the lap, as a
/// world-space rectangle; all four edges are exclusive.
///
/// The golem finds the side the player is on by testing the player's position
/// against the four strips in order. The strips overlap at the pad's corners,
/// where the first one that holds the player counts.
typedef struct {
    s16 minX; // Edge of the strip towards negative X
    s16 maxX; // Edge towards positive X
    s16 minZ; // Edge towards negative Z
    s16 maxZ; // Edge towards positive Z
} _Actor510900LapStrip;
STATIC_ASSERT_SIZEOF(_Actor510900LapStrip, 0x8);

/// The strip along each side of the lap, in the same order as the corners.
extern _Actor510900LapStrip D_actor_510900_80167BA4[4];

/// The three face normals `func_actor_510900_8013B524` copies into
/// `Gp_GridParams->normals`, restoring the collision grid this actor edited.
static SVECTOR _gActor510900Collision35DA4[3];

/// The twelve face corners `func_actor_510900_8013B524` copies into
/// `Gp_GridParams->vertices`.
static SVECTOR _gActor510900Collision35DBC[12];

/// The three `WorldCollisionGridFace` records `func_actor_510900_8013B524` copies into
/// `Gp_GridParams->faces`.
static WorldCollisionGridFace _gActor510900Collision35E1C[3];

/// The extra face normal `func_actor_510900_8013B424` installs as
/// `Gp_GridParams->normals[3]` while the actor's own face is in the grid.
extern SVECTOR D_actor_510900_80167C60;

/// The four face corners that face uses, copied into
/// `Gp_GridParams->vertices[12..15]`.
extern SVECTOR D_actor_510900_80167C68[4];

/// The `WorldCollisionGridFace` record for that face, copied into
/// `Gp_GridParams->faces[3]`.
extern WorldCollisionGridFace D_actor_510900_80167C88;

static s32  func_actor_510900_8013691C(Task* arg0);
static void func_actor_510900_8013864C(Task* arg0);
static void func_actor_510900_801387F4(Task* arg0);
static void func_actor_510900_80138978(Task* arg0);
static void func_actor_510900_80138A9C(Task* arg0);
static void func_actor_510900_80138D38(Task* arg0);
static void func_actor_510900_80138F44(Task* arg0);
static void func_actor_510900_8013B6A0(Enemy* arg0, Task* arg1);
static void func_actor_510900_8013B804(Task* arg0);
static void func_actor_510900_8013B870(Task* arg0);
static void func_actor_510900_8013B988(Task* arg0);
static void func_actor_510900_8013BA58(Task* arg0);
static void func_actor_510900_8013BB20(Task* arg0);
static void func_actor_510900_8013BC80(Task* arg0);

static void func_actor_510900_8013C380(Task* arg0);
static void func_actor_510900_8013C430(Task* arg0);

/// The script block pair `Gp_SpawnScript18` is handed at animation frame 0x58; both live
/// in the room overlay, not here.

/// `stateCounter` reload tables, indexed by four bits of `gRandomLcgState`.
extern s16 D_actor_510900_80167990[];
extern s16 D_actor_510900_801679B0[];

void func_actor_510900_8013B3D0(Task*);
void func_actor_510900_8013BE98(Task*);
void func_actor_510900_8013BF90(Task*);
void func_actor_510900_8013C090(Task*);
void func_actor_510900_8013C190(Task*);
void func_actor_510900_8013C1EC(Task*);
void func_actor_510900_8013C3DC(Task*);

DamageAttack D_actor_510900_8016796C[5] = {
    { 24, 6 },
    { 20, 7 },
    { 999, 6 },
    { 0, 2 },
    { 1, 6 },
};

EnemyParams D_actor_510900_80167980 = { &D_actor_510900_80167968, 1600, 500, 800, 30, 50, 3, 0, 0 };

s16 D_actor_510900_80167990[16] = {
    2000,
    2100,
    2200,
    2300,
    2400,
    2500,
    2600,
    2700,
    2800,
    2900,
    3000,
    3200,
    3400,
    3600,
    3800,
    4000,
};

s16 D_actor_510900_801679B0[16] = {
    14,
    16,
    18,
    20,
    22,
    24,
    26,
    28,
    30,
    32,
    35,
    40,
    45,
    50,
    55,
    60,
};

u16 D_actor_510900_801679D0[16] = {
    5,
    5,
    5,
    5,
    10,
    10,
    10,
    10,
    15,
    15,
    15,
    15,
    20,
    20,
    30,
    30,
};

u16 D_actor_510900_801679F0[16] = {
    15,
    15,
    15,
    15,
    25,
    25,
    25,
    25,
    35,
    35,
    35,
    35,
    45,
    45,
    45,
    45,
};

s16 D_actor_510900_80167A10[4] = {
    14,
    12,
    10,
    0,
};

TaskDesc D_actor_510900_80167A18[7] = {
    { { { TASK_BODY_TMD, 96 } }, func_actor_510900_8013B3D0, { .model = &gActor510900No9GolemAkropolisBody } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_510900_8013BE98, { .model = &gActor510900No9GolemAkropolisProp } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_510900_8013BF90, { .model = &gActor510900Model0FE60 } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_510900_8013C090, { .model = &gActor510900Model10468 } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_510900_8013C190, { .model = &gActor510900GolemGrenade } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_510900_8013C1EC, { .model = &gActor510900Model10C8C } },
    { { { TASK_BODY_COORD, 96 } }, func_actor_510900_8013C3DC, { .value = 0 } },
};

TaskMessageEntry D_actor_510900_80167A6C[7] = {
    { 2014, func_actor_510900_8013BD5C },
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_510900_8013BD84 },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceRotMatrix },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_510900_8013BE64 },
    { 2007, func_actor_510900_801391B8 },
    { ACTOR_MESSAGE_IS_PRESENT, func_actor_510900_8013BE84 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

u8 D_actor_510900_80167AA4[136] = {
    0,
    0,
    0,
    0,
    224,
    58,
    20,
    128,
    216,
    61,
    20,
    128,
    208,
    71,
    20,
    128,
    164,
    86,
    20,
    128,
    192,
    96,
    20,
    128,
    108,
    108,
    20,
    128,
    252,
    144,
    20,
    128,
    216,
    154,
    20,
    128,
    0,
    165,
    20,
    128,
    124,
    169,
    20,
    128,
    112,
    192,
    20,
    128,
    180,
    199,
    20,
    128,
    200,
    221,
    20,
    128,
    200,
    233,
    20,
    128,
    136,
    246,
    20,
    128,
    140,
    3,
    21,
    128,
    156,
    13,
    21,
    128,
    124,
    33,
    21,
    128,
    92,
    39,
    21,
    128,
    4,
    50,
    21,
    128,
    196,
    53,
    21,
    128,
    148,
    62,
    21,
    128,
    24,
    66,
    21,
    128,
    72,
    95,
    21,
    128,
    8,
    110,
    21,
    128,
    200,
    133,
    21,
    128,
    216,
    145,
    21,
    128,
    136,
    190,
    21,
    128,
    32,
    214,
    21,
    128,
    252,
    7,
    22,
    128,
    36,
    218,
    21,
    128,
    224,
    66,
    22,
    128,
    204,
    113,
    22,
    128,
};

AnimationSet* D_actor_510900_80167B2C[3] = {
    NULL,
    &gActor510900Animation27994,
    &gActor510900Animation27FDC,
};

s16 D_actor_510900_80167B38[34] = {
    0,
    8,
    8,
    4,
    4,
    0,
    8,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    4,
    4,
    3,
    3,
    3,
    3,
    0,
    8,
    4,
    4,
    8,
    8,
    8,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

EffectSpawnArg D_actor_510900_80167B7C = { NULL, 300, 1 };

_Actor510900LapCorner D_actor_510900_80167B84[4] = {
    { -6600, -6600 },
    { 6600, -6600 },
    { 6600, 6600 },
    { -6600, 6600 },
};

_Actor510900LapDirection D_actor_510900_80167B94[4] = {
    { 1, 0 },
    { 0, 1 },
    { -1, 0 },
    { 0, -1 },
};

u16 D_actor_510900_80167B9C[4] = {
    1024,
    0,
    3072,
    2048,
};

_Actor510900LapStrip D_actor_510900_80167BA4[4] = {
    { -7200, 7200, -7200, -6000 },
    { 6000, 7200, -6000, 7200 },
    { -7200, 6000, 6000, 7200 },
    { -7200, -4608, -6000, 6000 },
};

static SVECTOR _gActor510900Collision35DA4[3] = {
#include "assets/actor_510900_collision_35DA4.inc"
};

static SVECTOR _gActor510900Collision35DBC[12] = {
#include "assets/actor_510900_collision_35DBC.inc"
};

static WorldCollisionGridFace _gActor510900Collision35E1C[3] = {
#include "assets/actor_510900_collision_35E1C.inc"
};

SVECTOR D_actor_510900_80167C60 = { -4096, 0, 0, 0 };

SVECTOR D_actor_510900_80167C68[4] = {
    { -5952, -200, -1248, 0 },
    { -5952, -200, -2500, 0 },
    { -5952, 0, -1248, 0 },
    { -5952, 0, -2500, 0 },
};

WorldCollisionGridFace D_actor_510900_80167C88 = { { 12, 13, 14, 15 }, 3, 0 };

u16 D_actor_510900_80167C94[12] = {
    25,
    20,
    15,
    12,
    10,
    9,
    8,
    7,
    6,
    5,
    5,
    5,
};

AnimationSet* D_actor_510900_80167CAC[3] = {
    NULL,
    &gActor510900Animation35474,
    &gActor510900Animation35B20,
};

SVECTOR D_actor_510900_80167CB8[3] = {
    { 6458, -2658, 535, 0 },
    { -1336, -2658, 6552, 0 },
    { -6441, -2658, -100, 0 },
};

u16 D_actor_510900_80167CD0[4] = {
    2048,
    1024,
    0,
    0,
};

u16 D_actor_510900_80167CD8[2][3] = {
    { 5, 6, 7 },
    { 7, 8, 9 },
};

u16 D_actor_510900_80167CE4[4] = {
    10,
    10,
    10,
    0,
};

u16 D_actor_510900_80167CEC[7][4] = {
    { 0, 1, 2, 6 },
    { 0, 1, 2, 6 },
    { 0, 4, 4, 6 },
    { 0, 4, 4, 6 },
    { 0, 5, 5, 6 },
    { 0, 5, 5, 6 },
    { 0, 0, 0, 0 },
};

static void func_actor_510900_8013A310(Task* task);

static void func_actor_510900_8013A9BC(Task* task);

static s32 func_actor_510900_8013C240(Task* task);

static void func_actor_510900_8013C338(Task* arg0, GfxCoord* arg1);

static void func_actor_510900_8013B0D8(Task* arg0);

/// View index the child keeps running in; any other view parks it.
/// Game-flag nibble 0xD values, indexed by the blast source's
/// `_Actor510900BlastSourceWork::state` and the parent work's `light2Status`.
extern u16 D_actor_510900_80167CEC[][4];

static void func_actor_510900_8013B658(Enemy* arg0, Task* arg1);

static void func_actor_510900_8013BEEC(Enemy* enemy, Task* task);

static void func_actor_510900_8013BFE4(Enemy* enemy, Task* task);

static void func_actor_510900_8013C034(Enemy* enemy, Task* task);

static void func_actor_510900_8013C0E4(Enemy* enemy, Task* task);

static void func_actor_510900_8013C134(Enemy* enemy, Task* task);

/// The three views a helipad light is visible in, indexed by its
/// `_Actor510900HelipadLightWork::lightIndex`.
extern u16 D_actor_510900_80167CD8[][3];

static void func_actor_510900_80135744(Task* arg0);
static void func_actor_510900_80135E90(Task* arg0);
static void func_actor_510900_80136184(Task* arg0);
static void func_actor_510900_80136B70(Task* arg0);
static void func_actor_510900_80137008(Task* arg0);
static void func_actor_510900_801373B8(Task* arg0);
static void func_actor_510900_801375D8(Task* arg0);
static void func_actor_510900_80137868(Task* arg0);
static void func_actor_510900_80137E20(Task* arg0);
static void func_actor_510900_80137FBC(Task* arg0);
static void func_actor_510900_80138250(Task* arg0);
static void func_actor_510900_801384C4(Task* arg0);
static void func_actor_510900_801395AC(Enemy* enemy, Task* task);
static void func_actor_510900_801397F0(Enemy* arg0, Task* arg1);
static void func_actor_510900_80139C10(Enemy* enemy, Task* task);
static void func_actor_510900_8013A100(Enemy* enemy, Task* task);
static void func_actor_510900_8013A5B8(Enemy* enemy, Task* task);
static void func_actor_510900_8013A85C(Enemy* arg0, Task* arg1);
static void func_actor_510900_8013AD90(Enemy* enemy, Task* task);
static void func_actor_510900_8013AF38(Enemy* arg0, Task* arg1);

/// Applies this frame's hits from the three `bodyContacts` collision records. A
/// type-2 id lands only while the `hitCooldown` cooldown is clear: its damage
/// is halved for 0x8000 ids, otherwise scaled by the player's distance and
/// doubled/quadrupled by the id's class and `damageRollCriticalHit`, and may pick
/// a flinch (`reaction`) that sets the next handler. Type-5 ids apply the
/// `Gp_LookupIdField` table damage directly.
static void func_actor_510900_80135744(Task* arg0)
{
    s32              lastId;
    VECTOR*          d;
    Actor510900Work* work;
    Enemy*           enemy;
    GfxCoord*        coord;
    s32              reaction;
    s32              param;
    s32              i;
    s16              dmg;
    s16              hit;
    s32              full;
    s16              loss;
    s32              sound;
    s32              pan;
    s16              wait;

    SCRATCH_STACK_RESERVE_BYTES(sizeof(VECTOR));
    d        = SCRATCH_STACK_CURSOR(VECTOR);
    coord    = arg0->extra.tmd->coords;
    work     = arg0->work;
    enemy    = arg0->spawnArg2.pointer;
    reaction = 0;
    lastId   = 0;
    if (work->hitCooldown != 0) {
        if (--work->hitCooldown <= 0) {
            work->hitCooldown = 0;
        }
    }
    for (i = 0; i < ARRAY_SIZE(work->bodyContacts); i++) {
        switch ((u16)(work->bodyContacts[i].key.value >> 16)) {
            case 0:
            case 1:
            case 3:
            case 4:
                break;
            case 2:
                if (work->hitCooldown != 0) {
                    break;
                }
                param = Gp_GetIdParam0(work->bodyContacts[i].key.value);
                if (work->bodyContacts[i].key.value & 0x8000) {
                    if ((u8)work->bodyContacts[i].key.value - 1 < 6U) {
                        reaction = 2;
                    }
                    dmg = (s16)Gp_ComputeDamage(work->bodyContacts[i].key.value, 0, 0, 0) >> 1;
                    damageAccumulateLifeDrainHp(arg0->spawnArg2.pointer, work->bodyContacts[i].key.value, dmg, 0);
                } else {
                    d->vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
                    d->vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
                    d->vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
                    full  = Gp_ComputeDamage(work->bodyContacts[i].key.value, SquareRoot0(d->vx * d->vx + d->vy * d->vy + d->vz * d->vz), 0, 0);
                    dmg   = full;
                    if ((u16)param == 5) {
                        dmg = full * 2;
                        Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[3], 2, NULL);
                    }
                    if (damageRollCriticalHit(enemy, work->bodyContacts[i].key.value, 0) != 0) {
                        dmg *= 4;
                        if ((u16)param != 5) {
                            Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[3], 0, NULL);
                        }
                        if (work->buildupStunned == 0) {
                            reaction = 1;
                        }
                    }
                }
                switch ((u16)param) {
                    case 0:
                    case 5:
                    case 6:
                    case 7:
                    case 8:
                    case 9:
                        break;
                    case 1:
                        if (work->buildupStunned == 0) {
                            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                            if ((gRandomLcgState >> 16) & 1) {
                                reaction = 1;
                            }
                        }
                        break;
                    case 2:
                        if (work->lethalAttackPhase == 0) {
                            Gp_SetObjFlag2(enemy, work->bodyContacts[i].key.value, 0);
                        }
                        break;
                    case 3:
                        if (work->buildupStunned == 0) {
                            reaction = 2;
                        }
                        break;
                    case 4:
                        dmg = dmg * 75 / 100;
                        break;
                }
                enemy->hp -= dmg;
                worldTargetAddReadoutAmount(&enemy->node, dmg, 0);
                if (enemy->hp <= 0) {
                    if (work->lethalAttackPhase < 2) {
                        work->lethalAttackPhase = 0;
                    } else {
                        enemy->hp = 1;
                    }
                }
                if (reaction != 0) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    if (!((gRandomLcgState >> 16) & 1)) {
                        sound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780004;
                    } else {
                        sound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780005;
                    }
                    pan = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(coord));
                }
                if (work->sparkSound != 0) {
                    sndEvtRequestScriptStop(work->sparkSound, SOUND_SCRIPT_STOP_NO_FADE);
                    work->sparkSound = 0;
                }
                if (work->lethalAttackPhase == 1) {
                    reaction = 0;
                }
                switch (reaction) {
                    case 0: {
                        u32 rng;
                        u32 r;
                        s32 v;
                        s32 r2;
                        s32 v2;

                        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        r               = rng >> 16;
                        v               = (r & 0x7F) + 0x40;
                        gRandomLcgState = rng;
                        if (!(r & 1)) {
                            v = -v;
                        }
                        work->hitTwist.vx = v;
                        r2                = (s16)r >> 8;
                        v2                = (r2 & 0x7F) + 0x40;
                        if (!(r2 & 1)) {
                            v2 = -v2;
                        }
                        work->hitTwist.vy    = v2;
                        work->hitTwistActive = 1;
                        if (enemy->hp <= 0) {
                            work->state       = ACTOR_510900_STATE_DEATH;
                            work->subState    = 0;
                            work->animationId = 0x18;
                            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        }
                        break;
                    }
                    case 1:
                        work->state                = ACTOR_510900_STATE_FLINCH;
                        work->subState             = 0;
                        work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        if (enemy->hp <= 0) {
                            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        }
                        break;
                    case 2:
                        work->state                = ACTOR_510900_STATE_RECOIL;
                        work->subState             = 0;
                        work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        if (enemy->hp <= 0) {
                            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        }
                        break;
                }
                if (lastId != work->bodyContacts[i].key.value) {
                    lastId = work->bodyContacts[i].key.value;
                    func_800FDB18(Gp_GetIdParam1(lastId) & 0xFFFF, &arg0->extra.tmd->coords[3], NULL, &work->hitEffectArg);
                }
                wait = Gp_GetIdParam2(work->bodyContacts[i].key.value);
                if (wait > 0) {
                    work->hitCooldown = wait;
                }
                break;
            case 5:
                hit = 0;
                switch ((u32)(u16)work->bodyContacts[i].key.value) {
                    case 2:
                        hit            = 1;
                        work->state    = ACTOR_510900_STATE_SPARK_RECOIL;
                        work->subState = 0;
                        break;
                    case 3:
                        hit            = 1;
                        work->state    = ACTOR_510900_STATE_RECOIL;
                        work->subState = 0;
                        break;
                    case 4:
                        hit            = 1;
                        work->state    = ACTOR_510900_STATE_SPARK_STUN;
                        work->subState = 0;
                        break;
                }
                if (hit) {
                    work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    loss                       = Gp_LookupIdField((u16)work->bodyContacts[i].key.value, 1);
                    enemy->hp                 -= loss;
                    worldTargetAddReadoutAmount(&enemy->node, loss, 0);
                    if (enemy->hp <= 0) {
                        if (work->lethalAttackPhase < 2) {
                            work->lethalAttackPhase = 0;
                            work->body.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        } else {
                            enemy->hp = 1;
                        }
                    }
                }
                break;
        }
    }
    worldCollisionClearContacts(work->bodyContacts);
    if (work->attackContacts[0].flags & 1) {
        if ((work->attackContacts[0].key.value & 0xFFFF0000) == 0x10000) {
            work->attackLanded         = 1;
            work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        }
        worldCollisionClearContacts(work->attackContacts);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

/// State 0 (animations 0xA/8/0x19): the idle-to-walk start. Sub-state 0 queues
/// the step cue at animation frame 0xA and past 0x64 hands sub-state 1 the animation 0xA;
/// sub-state 1 enables both attack spheres with the
/// animation 8 past frame 0x14. Sub-states 2 and 3 are the two walk cycles:
/// each feeds `lapSpeed` 0x38 over a 0x13-frame window, mutes it while
/// `attackLanded` is latched, queues the footfall cue, and at the end of the cycle
/// either hands on to the next one or - when `attackLanded` is set - disables both
/// attack spheres and restarts the cycle with the animation 0x19. Sub-state 4
/// leaves for state 3 with the animation 0xF.
static void func_actor_510900_80135E90(Task* arg0)
{
    Actor510900Work* work;
    GfxCoord*        coord;
    s32              snd;
    s32              pair;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (work->subState) {
        case 0:
            if (work->animationFrame == 0xA) {
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780006;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animationFrame >= 0x64) {
                work->subState    = 1;
                work->animationId = 0xA;
            }
            break;
        case 1:
            if (work->animationFrame >= 0x14) {
                work->subState             = 2;
                work->animationId          = 8;
                work->weaponAttack.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->forearmAttack.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                pair                       = damagePackAttackKey(&D_actor_510900_80167968, 2);
                work->weaponAttack.key     = pair;
                work->forearmAttack.key    = pair;
            }
            break;
        case 2:
            work->lapSpeed = ((u32)((u16)work->animationFrame - 9) < 0x13U) ? 0x38 : 0;
            if (work->attackLanded != 0) {
                work->lapSpeed = 0;
            }
            if (work->animationFrame == 0xA) {
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4078000B;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animationFrame >= 0x1D) {
                if (work->attackLanded != 0) {
                    work->state                = ACTOR_510900_STATE_SLASH;
                    work->subState             = 3;
                    work->animationId          = 0x19;
                    work->attackLanded         = 0;
                    work->flameFrames          = 0;
                    work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                } else {
                    work->subState    = 3;
                    work->animationId = 9;
                }
            }
            break;
        case 3:
            work->lapSpeed = ((u32)((u16)work->animationFrame - 5) < 0x13U) ? 0x38 : 0;
            if (work->attackLanded != 0) {
                work->lapSpeed = 0;
            }
            if (work->animationFrame == 6) {
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4078000B;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animationFrame >= 0x27) {
                if (work->attackLanded != 0) {
                    work->state                = ACTOR_510900_STATE_SLASH;
                    work->subState             = 3;
                    work->animationId          = 0x19;
                    work->attackLanded         = 0;
                    work->flameFrames          = 0;
                    work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                } else {
                    work->subState = 4;
                }
            }
            break;
        case 4:
            work->animationId = 0xF;
            work->state       = ACTOR_510900_STATE_FLAME_LUNGE;
            work->subState    = 1;
            break;
    }
}

/// Sub-state machine on `subState`. Off the corner (`lapSide` != `playerSide`)
/// a player range `sideRemaining` of 0xBB8 or more forces sub-state 4, except from
/// 8. Sub-state 0 counts `stateCounter` down and, unless
/// `func_actor_510900_8013691C` hands over, picks 1 or 5 from that range;
/// 2, 3 and 6 drain `stateCounter` by 0x1D a frame (3 also spawns an effect every
/// 0x28 frames through `shotCountdown`). Each reload of `stateCounter` is a 4-bit
/// `gRandomLcgState` draw from `D_actor_510900_80167990` or `D_actor_510900_801679B0`.
static void func_actor_510900_80136184(Task* arg0)
{
    Actor510900Work* work;
    GfxCoord*        coord;
    EffectWork*      eff;
    s32              snd;
    s32              rng;
    s16              speed;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->playerSide != work->lapSide && work->subState != 8 && work->sideRemaining >= 0xBB8) {
        work->subState = 4;
    }
    switch (work->subState) {
        case 0:
            work->lapSpeed     = 0;
            work->attackLanded = 0;
            if (--work->stateCounter <= 0) {
                if (work->flameMode == ACTOR_510900_FLAME_BLAST) {
                    work->flameMode = ACTOR_510900_FLAME_BURNING;
                }
                if (func_actor_510900_8013691C(arg0) == 0) {
                    if (work->sideRemaining < 0x7D0) {
                        work->subState     = 1;
                        work->animationId  = 2;
                        work->stateCounter = 0;
                    } else if (((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.targeted == 1 && (u32)(gPlayerStatus.weaponSlotItem - 0xA) < 3U) {
                        work->subState    = 5;
                        work->animationId = 0x15;
                        snd               = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780003;
                        sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                                 (s8)worldCoordGetOriginAudioDepth(coord));
                    } else {
                        work->subState     = 1;
                        work->animationId  = 2;
                        work->stateCounter = 1;
                    }
                }
            }
            break;
        case 1:
            work->lapSpeed = 0;
            if (work->animationFrame >= 0xD) {
                if (work->stateCounter == 0) {
                    work->subState    = 2;
                    work->animationId = 3;
                } else {
                    work->subState      = 3;
                    work->animationId   = 4;
                    work->shotCountdown = 0x1D;
                }
                work->stateCounter = D_actor_510900_80167990[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
            }
            break;
        case 2:
            work->lapSpeed      = 0x1D;
            work->stateCounter -= 0x1D;
            if (work->stateCounter <= 0 || work->lapDistance > 0xB478U) {
                work->animationId  = 1;
                work->subState     = 0;
                work->stateCounter = D_actor_510900_801679B0[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
            }
            if (work->sideRemaining >= 0x7D0 && work->playerDistance < 0x3E8 && work->lapDistance <= 0xAE9CU &&
                work->lapDistance > 0xB477U) {
                work->subState     = 0;
                work->animationId  = 1;
                work->stateCounter = 0;
            }
            break;
        case 3:
            work->lapSpeed      = 0x1D;
            work->stateCounter -= 0x1D;
            if (--work->shotCountdown == 0) {
                eff = Gp_SpawnEff((EFFECT_NO9_MUZZLE_FLASH | EFFECT_SPAWN_UNLIMITED), &arg0->extra.tmd->coords[8], 0, NULL);
                if (eff != NULL) {
                    taskReparent(arg0, eff->task);
                }
                work->shotCountdown = 0x28;
                snd                 = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4078000E;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.targeted == 1 && (u32)(gPlayerStatus.weaponSlotItem - 0xA) < 3U) {
                work->subState    = 6;
                work->animationId = 0x15;
                snd               = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780003;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->stateCounter <= 0 || work->lapDistance > 0xB478U) {
                work->animationId  = 1;
                work->subState     = 0;
                work->stateCounter = D_actor_510900_801679B0[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
            }
            if (work->sideRemaining >= 0x7D0 && work->playerDistance < 0x3E8 && work->lapDistance <= 0xAE9CU &&
                work->lapDistance > 0xB477U) {
                work->subState     = 0;
                work->animationId  = 1;
                work->stateCounter = 0;
            }
            break;
        case 4:
            speed             = 0;
            work->animationId = 6;
            if (work->animationFrame >= 8) {
                speed = 0xA0;
            }
            work->lapSpeed = speed;
            if (work->sideRemaining < 0x5DC) {
                work->subState     = 0;
                work->animationId  = 1;
                work->stateCounter = 0;
            }
            if (work->sideRemaining >= 0x7D0 && work->playerDistance < 0x3E8 && work->lapDistance <= 0xAE9CU &&
                work->lapDistance > 0xB477U) {
                work->subState     = 0;
                work->animationId  = 1;
                work->stateCounter = 0;
            }
            break;
        case 5:
            if (work->animationFrame >= 0xF) {
                work->subState     = 6;
                work->animationId  = 0x16;
                work->stateCounter = D_actor_510900_80167990[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
            }
            break;
        case 6:
            work->lapSpeed      = 0x1D;
            work->stateCounter -= 0x1D;
            if (work->stateCounter <= 0 || work->lapDistance > 0xB478U ||
                (work->sideRemaining >= 0x7D0 && work->playerDistance < 0x3E8 && work->lapDistance <= 0xAE9CU)) {
                work->subState    = 7;
                work->animationId = 0x17;
            }
            break;
        case 7:
            work->lapSpeed = 0;
            if (work->animationFrame >= 0xD) {
                work->subState    = 0;
                work->animationId = 1;
                if (work->sideRemaining >= 0x7D0 && work->playerDistance < 0x3E8 && work->lapDistance <= 0xAE9CU &&
                    work->lapDistance > 0xB477U) {
                    work->stateCounter = 0;
                } else {
                    work->stateCounter = D_actor_510900_801679B0[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
                }
            }
            break;
        case 8:
            if (work->sideTravelled < 0xC8 || work->playerSide != work->lapSide || work->animationFrame < 0xB ||
                work->animationFrame >= 0x18) {
                work->lapSpeed = 0;
            } else {
                work->lapSpeed = -0xA7;
            }
            if (work->animationFrame == 0x19) {
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780008;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animationFrame >= 0x32) {
                work->state        = ACTOR_510900_STATE_PATROL;
                work->subState     = 0;
                work->animationId  = 1;
                work->stateCounter = 0;
            }
            break;
    }
}

/// Picks the handler the patrol hands over to, from the distance `lapDistance`
/// walked around the patrol square and the player's facing. Two spots on the
/// lap - 0x4B00 and 0x7F00, each with a 0x120 window - fire once apiece through
/// the `slashedLight` latch and send the actor into state 2; past 0xB477 the lap is
/// over and state 6 takes it. Otherwise, only on the frame the corner has been
/// reached (`lapSide` == `playerSide`), the player's range `playerDistance` and
/// whether they face the actor choose between states 2..5, each with the
/// animation the handler starts on. Returns 1 when a handler was selected.
static s32 func_actor_510900_8013691C(Task* arg0)
{
    Actor510900Work* work;
    GfxCoord*        coord;
    s32              diff;
    s32              ret;
    s16              dist;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->slashedLight == 0) {
        diff = work->lapDistance - 0x4B00;
        if (diff < 0) {
            diff = -diff;
        }
        if (diff < 0x120) {
            work->slashedLight = 1;
            work->state        = ACTOR_510900_STATE_SLASH;
            work->subState     = 0;
            work->animationId  = 0xA;
            return 1;
        }
        diff = work->lapDistance - 0x7F00;
        if (diff < 0) {
            diff = -diff;
        }
        if (diff < 0x120) {
            work->slashedLight = 2;
            work->state        = ACTOR_510900_STATE_SLASH;
            work->subState     = 0;
            work->animationId  = 0xA;
            return 1;
        }
    }
    if (work->lapDistance > 0xB477U) {
        work->state       = ACTOR_510900_STATE_LETHAL_ATTACK;
        work->subState    = 0;
        work->animationId = 0xE;
        return 1;
    }
    if (work->lapDistance > 0xAE9CU) {
        return 0;
    }
    if (work->lapSide != work->playerSide) {
        return 0;
    }
    if (gPlayerStatus.coordMtx->m[0][2] * coord->coord.m[0][2] +
            gPlayerStatus.coordMtx->m[2][2] * coord->coord.m[2][2] <
        0) {
        dist = work->playerDistance;
        if (dist < 0xAF0) {
            ret               = 1;
            work->state       = ACTOR_510900_STATE_SLASH;
            work->subState    = 0;
            work->animationId = 0xA;
        } else if (dist < 0xED8) {
            ret               = 1;
            work->state       = ACTOR_510900_STATE_FLAME_LUNGE;
            work->subState    = 0;
            work->animationId = 0xE;
        } else if (dist < 0x12C0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 0x10) & 1) {
                work->state       = ACTOR_510900_STATE_FLAME_LUNGE;
                work->subState    = 0;
                work->animationId = 0xE;
            } else {
                work->state       = ACTOR_510900_STATE_GRENADE;
                work->subState    = 0;
                work->animationId = 0x1B;
            }
            ret = 1;
        } else {
            ret = 0;
            if (work->grenadeLive == 0) {
                ret               = 1;
                work->state       = ACTOR_510900_STATE_GRENADE;
                work->subState    = 0;
                work->animationId = 0x1B;
            }
        }
    } else {
        if (work->playerDistance < 0xAF0) {
            work->state       = ACTOR_510900_STATE_SLASH;
            work->subState    = 0;
            work->animationId = 0xA;
        } else {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (((gRandomLcgState >> 0x10) & 0xF) < 0xAU) {
                work->state       = ACTOR_510900_STATE_GRENADE;
                work->subState    = 0;
                work->animationId = 0x1B;
            } else {
                work->state       = ACTOR_510900_STATE_DASH;
                work->subState    = 0;
                work->animationId = 0xB;
            }
        }
        ret = 1;
    }
    return ret;
}

/// State 2 (sub-states 0..3): the patrol walk. Sub-state 0 stops the actor and
/// hands sub-state 1 the animation 8 past animation frame 0x14. Sub-states 1 and 2 are
/// the two walk cycles: each feeds `lapSpeed` 0x38 over a 0x13-frame window,
/// mutes it while `attackLanded` is latched and queues the footfall cue; sub-state
/// 1 also sets `lightSlashStruck` when `slashedLight` names a light and enables both
/// attack spheres at frame 9. At the end of a cycle a
/// 4-bit `gRandomLcgState` draw against `D_actor_510900_80167A10[stateCounter]`
/// decides whether to walk another cycle - bumping `stateCounter`, which sub-state
/// 2 caps at three - or to leave for state 1 with a fresh `stateCounter` from
/// `D_actor_510900_801679D0`. A latched `attackLanded` instead sends sub-state 3,
/// the turn, with the animation 0x19; it queues its cue at frame 0x2D and past
/// 0x5A leaves the same way.
static void func_actor_510900_80136B70(Task* arg0)
{
    Actor510900Work* work;
    GfxCoord*        coord;
    s32              snd;
    s32              pair;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (work->subState) {
        case 0:
            work->lapSpeed = 0;
            if (work->animationFrame >= 0x14) {
                work->subState     = 1;
                work->animationId  = 8;
                work->stateCounter = 0;
            }
            break;
        case 1:
            work->lapSpeed = ((u32)((u16)work->animationFrame - 9) < 0x13U) ? 0x38 : 0;
            if (work->attackLanded != 0) {
                work->lapSpeed = 0;
            }
            if (work->animationFrame == 0xA) {
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4078000B;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if ((work->slashedLight > 0) && (work->animationFrame == 8)) {
                work->lightSlashStruck = 1;
            }
            if (work->animationFrame == 9) {
                work->weaponAttack.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->forearmAttack.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                pair                       = damagePackAttackKey(&D_actor_510900_80167968, 2);
                work->weaponAttack.key     = pair;
                work->forearmAttack.key    = pair;
            }
            if (work->animationFrame >= 0x1D) {
                if (work->attackLanded != 0) {
                    work->subState             = 3;
                    work->animationId          = 0x19;
                    work->attackLanded         = 0;
                    work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                } else {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    if (D_actor_510900_80167A10[work->stateCounter] < (s32)((gRandomLcgState >> 0x10) & 0xF)) {
                        u16* tbl                   = D_actor_510900_801679D0;
                        work->state                = ACTOR_510900_STATE_PATROL;
                        work->animationId          = 1;
                        work->subState             = 0;
                        gRandomLcgState            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->stateCounter         = tbl[(gRandomLcgState >> 0x10) & 0xF];
                        work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    } else {
                        work->subState    = 2;
                        work->animationId = 9;
                        work->stateCounter++;
                    }
                }
            }
            break;
        case 2:
            work->lapSpeed = ((u32)((u16)work->animationFrame - 5) < 0x13U) ? 0x38 : 0;
            if (work->attackLanded != 0) {
                work->lapSpeed = 0;
            }
            if (work->animationFrame == 6) {
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4078000B;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animationFrame >= 0x27) {
                if (work->attackLanded != 0) {
                    work->subState             = 3;
                    work->animationId          = 0x19;
                    work->attackLanded         = 0;
                    work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                } else {
                    if ((D_actor_510900_80167A10[work->stateCounter] <
                         (s32)(((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 0x10) & 0xF)) ||
                        (work->stateCounter >= 3)) {
                        u16* tbl                   = D_actor_510900_801679D0;
                        work->state                = ACTOR_510900_STATE_PATROL;
                        work->subState             = 0;
                        work->animationId          = 1;
                        gRandomLcgState            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->stateCounter         = tbl[(gRandomLcgState >> 0x10) & 0xF];
                        work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    } else {
                        work->stateCounter++;
                        work->subState    = 1;
                        work->animationId = 8;
                    }
                }
            }
            break;
        case 3:
            if (work->animationFrame == 0x2D) {
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4078000A;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animationFrame >= 0x5A) {
                u16* tbl           = D_actor_510900_801679D0;
                work->state        = ACTOR_510900_STATE_PATROL;
                work->subState     = 0;
                work->animationId  = 1;
                gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->stateCounter = tbl[(gRandomLcgState >> 0x10) & 0xF];
            }
            break;
    }
}

/// State 3 (sub-state 0/1): the wind-up. Sub-state 0 queues the two-part
/// charge sound at animation frame 0x39 - setting `flameMode`/`flameFrames` and enabling
/// both attack spheres - a second cue at 0x41, and past
/// 0x5A hands sub-state 1 the animation 0xF. Sub-state 1 converts `playerDistance`
/// into the `stateCounter` speed at frame 0xE, feeds `lapSpeed` from it over
/// frames 0x17..0x2F, queues two more cues, and past 0x46 leaves for either
/// state 8 (when `attackLanded` is set) or state 1 with a fresh `stateCounter`,
/// disabling both attack spheres on the way out.
static void func_actor_510900_80137008(Task* arg0)
{
    Actor510900Work* work;
    GfxCoord*        coord;
    s32              snd;
    s32              pair;
    s32              val;
    s32              cur;
    u32              rng;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (work->subState) {
        case 0:
            work->lapSpeed = 0;
            if (work->animationFrame == 0x39) {
                work->flameMode   = ACTOR_510900_FLAME_BURNING;
                work->flameFrames = 0x55;
                snd               = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4078000E;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
                work->flameSound = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4078000D;
                sndEvtRequestScriptStart(work->flameSound, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
                work->weaponAttack.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->forearmAttack.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                pair                       = damagePackAttackKey(&D_actor_510900_80167968, 5);
                work->weaponAttack.key     = pair;
                work->forearmAttack.key    = pair;
            }
            if (work->animationFrame == 0x41) {
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780006;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animationFrame >= 0x5A) {
                work->animationId = 0xF;
                work->subState    = 1;
            }
            break;
        case 1:
            if (work->animationFrame == 0xE) {
                cur = work->playerDistance;
                val = 0x1388;
                if (cur < 0x1389) {
                    val = cur;
                }
                work->stateCounter = (val - 0x4B0) / 24;
            }
            work->lapSpeed = ((u32)((u16)work->animationFrame - 0x17) < 0x19U) ? work->stateCounter : 0;
            if (work->animationFrame == 0x19) {
                work->weaponAttack.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->forearmAttack.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                pair                       = damagePackAttackKey(&D_actor_510900_80167968, 1);
                work->weaponAttack.key     = pair;
                work->forearmAttack.key    = pair;
                snd                        = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780007;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animationFrame == 0x2F) {
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780008;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animationFrame >= 0x46) {
                if (work->attackLanded != 0) {
                    work->state        = ACTOR_510900_STATE_PATROL;
                    work->subState     = 8;
                    work->attackLanded = 0;
                    work->animationId  = 5;
                } else {
                    work->state        = ACTOR_510900_STATE_PATROL;
                    work->animationId  = 1;
                    work->subState     = 0;
                    work->stateCounter = D_actor_510900_801679D0[((u32)(rng = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 0x10) & 0xF];
                    gRandomLcgState    = rng;
                }
                work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            break;
    }
}

/// Handler that spawns the actor's companion enemy at frame 0x14 of its
/// animation (`animationFrame` == 0x14): the spawned task's model takes its texture
/// page and CLUT row from the current area record, indexed by the enemy's own
/// bank nibble, and a sound is queued from the actor's attach coordinate. Past
/// frame 0x46 the handler leaves for either state 5 (animation 0xB) or, on a
/// failed `gRandomLcgState` roll, state 1 with a fresh `stateCounter`.
static void func_actor_510900_801373B8(Task* arg0)
{
    Actor510900Work* work;
    Enemy*           enemy;
    GfxCoord*        coord;
    GameLocationKey* sessionKey;
    TmdObject*       model;
    AreaVariant*     layout;
    AreaPlacement*   entry;
    GameLocationKey  key;
    s32              idx;
    s32              snd;
    s32              pan;
    s32              roll;
    u32              rng;

    roll  = 0;
    coord = arg0->extra.tmd->coords;
    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;

    work->lapSpeed = 0;
    if (work->animationFrame == 0x14) {
        work->grenadeLive = 1;
        model             = Gp_SpawnEnemyFromTable(D_actor_510900_80167A18, 4, roll, enemy)->task->extra.tmd;
        idx               = (u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        sessionKey        = &gGameSession->location.loc;
        key.stage         = sessionKey->stage;
        key.area          = sessionKey->area;
        key.room          = sessionKey->room;
        key.view          = gGameSession->location.loc.view;
        areaSyncLocationVariant(&key);
        layout = Gp_GetNestedAreaRec(&key);
        /* offset + base, not `&layout->placements[idx]`: the ROM adds the scaled
           index onto the table (`addu s0, s0, v0`). */
        entry                    = gpAreaPlaceAt(layout->placements, idx);
        model->texturePageOffset = entry->texturePageOffset;
        model->clutRowOffset     = entry->clutRowOffset;
        if (model->buffer != NULL) {
            tmdBuildBufferHalf(model);
            tmdBuildBufferHalf(model);
        }
        snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780012;
        pan = (s8)worldCoordGetOriginAudioPan(coord);
        sndEvtRequestScriptStart(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
    }
    if (work->animationFrame >= 0x46) {
        if (work->playerDistance >= 0xEA7) {
            /* Reading the global back is what keeps the store ahead of the
               shift: written as a local, the store sinks into the branch
               delay slot and the draw lands in a different register. */
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (((gRandomLcgState >> 0x10) & 0xF) < 0xCU) {
                roll = 1;
            }
        }
        if (roll == 0) {
            work->state        = ACTOR_510900_STATE_PATROL;
            work->animationId  = 1;
            work->subState     = 0;
            work->stateCounter = D_actor_510900_801679D0[((u32)(rng = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 0x10) & 0xF];
            gRandomLcgState    = rng;
            return;
        }
        work->state       = ACTOR_510900_STATE_DASH;
        work->subState    = 0;
        work->animationId = 0xB;
    }
}

/// State 5 (animation 0xB): the actor rears up, holds, then either drops back
/// to state 1 or commits. Sub-state 0 ramps `lapSpeed` at animation frame 0x47, queues
/// the rear-up sound at 0x4A and hands sub-state 1 the animation 0xC at 0x52.
/// Sub-state 1 enables both attack spheres while
/// `playerDistance` is short; otherwise it bleeds `stateCounter` down by 0x84 a frame
/// and, once that runs out (or `sideRemaining` drops below 0x384), returns to
/// state 1 with a fresh `gRandomLcgState` draw. Sub-state 2 queues the landing
/// sound at frame 0xE and leaves for sub-state 8 of state 1 past 0x3B.
static void func_actor_510900_801375D8(Task* arg0)
{
    Actor510900Work* work;
    GfxCoord*        coord;
    s32              snd;
    s32              pair;
    s32              val;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (work->subState) {
        case 0:
            work->lapSpeed = (work->animationFrame < 0x47) ? 0 : 0x84;
            if (work->animationFrame == 0x4A) {
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4078000F;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animationFrame >= 0x52) {
                work->animationId  = 0xC;
                work->subState     = 1;
                work->stateCounter = 0xEA6;
            }
            break;
        case 1:
            work->lapSpeed = 0x84;
            if (work->playerDistance < 0x514) {
                work->subState             = 2;
                work->animationId          = 0xD;
                work->weaponAttack.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->forearmAttack.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                pair                       = damagePackAttackKey(&D_actor_510900_80167968, 0);
                work->weaponAttack.key     = pair;
                work->forearmAttack.key    = pair;
            } else {
                work->stateCounter -= 0x84;
                if (work->stateCounter < 0 || work->sideRemaining < 0x384) {
                    work->state                = ACTOR_510900_STATE_PATROL;
                    work->subState             = 0;
                    work->animationId          = 1;
                    gRandomLcgState            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    val                        = D_actor_510900_801679D0[(gRandomLcgState >> 16) & 0xF];
                    work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->stateCounter         = val;
                }
            }
            break;
        case 2:
            work->lapSpeed = 0;
            if (work->animationFrame == 0xE) {
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4078000C;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animationFrame >= 0x3B) {
                work->subState             = 8;
                work->animationId          = 5;
                work->state                = ACTOR_510900_STATE_PATROL;
                work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            break;
    }
}

/// Handler for the scripted sequence the actor plays out through `subState`:
/// state 0 opens with the two 0x4078 cues at animation frame 0x39 and hands over at 0x5A,
/// state 1 plays one more cue at frame 0xA and hands over at 0x64, state 2 runs
/// the long beat - a pair swap and a screen fade at 0x50, a cue at 0x53 whose id
/// depends on `flameMode`, a script spawn at 0x58, the pair blanked at 0x60 -
/// and either enters state 3 when `attackLanded` is latched or, past frame 0xB3,
/// drops back to `state` state 1 with a fresh `stateCounter`. State 3 keeps the
/// walk speed inside its two frame windows and, every sixth frame, restarts the
/// effect on the player's model; its `deathSoundLoadStep` sub-state queues file 0x1E and
/// plays the arrival cue once the drive goes idle.
///
/// `blend` is one temp on purpose: the `lh` of `animationId` leaves its high bits
/// unknown to combine, which is what keeps the `andi 0xFFFF` on the second
/// window test of each chain.
static void func_actor_510900_80137868(Task* arg0)
{
    Actor510900Work* work;
    GfxCoord*        coord;
    s32              snd;
    s32              pair;
    s32              blend;
    u32              rng;

    coord = arg0->extra.tmd->coords;
    work  = arg0->work;

    SCRATCH_STACK_RESERVE_BYTES(0x10);

    switch (work->subState) {
        case 0:
            work->lethalAttackPhase = 1;
            work->lapSpeed          = 0;
            if (work->animationFrame == 0x39) {
                work->flameMode   = ACTOR_510900_FLAME_BURNING;
                work->flameFrames = 0x10E;
                snd               = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4078000E;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
                work->flameSound = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4078000D;
                sndEvtRequestScriptStart(work->flameSound, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
                work->weaponAttack.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->forearmAttack.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                pair                       = damagePackAttackKey(&D_actor_510900_80167968, 5);
                work->weaponAttack.key     = pair;
                work->forearmAttack.key    = pair;
            }
            if (work->animationFrame >= 0x5A) {
                work->animationId = 0x1A;
                work->subState    = 1;
            }
            break;
        case 1:
            if (work->animationFrame == 0xA) {
                snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780006;
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animationFrame >= 0x64) {
                work->animationId = 7;
                work->subState    = 2;
            }
            break;
        case 2:
            if (work->animationFrame == 0x50) {
                work->lethalAttackPhase    = 2;
                work->weaponAttack.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->forearmAttack.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                pair                       = damagePackAttackKey(&D_actor_510900_80167968, 3);
                work->weaponAttack.key     = pair;
                work->forearmAttack.key    = pair;

                gGameSession->deathRestartDelay   = 0x80;
                gGameSession->deathSoundCountdown = GAME_SESSION_DEATH_SOUND_HOLD;
            }
            if (work->animationFrame == 0x53) {
                if (work->flameMode == ACTOR_510900_FLAME_BURNING) {
                    snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780010;
                    sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                             (s8)worldCoordGetOriginAudioDepth(coord));
                } else {
                    snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4078000C;
                    sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord),
                                             (s8)worldCoordGetOriginAudioDepth(coord));
                }
            }
            if (work->animationFrame == 0x58) {
                Gp_SpawnScript18(D_acropolis_helicopter_landing_pad_80187D34, &D_acropolis_helicopter_landing_pad_80187D3C);
            }
            if (work->animationFrame == 0x60) {
                work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            blend = (u16)work->animationFrame;
            if (blend - 0x4B < 0x10U) {
                work->lapSpeed = 0x3C;
            } else if (((blend - 0x8F) & 0xFFFF) < 0xFU) {
                work->lapSpeed = -0x40;
            } else {
                work->lapSpeed = 0;
            }
            if (work->animationFrame >= 0x51) {
                if (work->attackLanded == 1) {
                    work->subState           = 3;
                    work->attackLanded       = 0;
                    work->deathSoundLoadStep = 1;
                    gPlayerStatus.hp         = 0;
                    Gp_PulseState1C80();
                }
            } else {
                work->attackLanded = 0;
            }
            if (work->animationFrame >= 0xB3) {
                work->state        = ACTOR_510900_STATE_PATROL;
                work->animationId  = 1;
                work->subState     = 0;
                work->stateCounter = D_actor_510900_801679D0[((u32)(rng = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 0x10) & 0xF];
                gRandomLcgState    = rng;
            }
            break;
        case 3:
            blend = work->animationId;
            if (blend == 7) {
                blend = (u16)work->animationFrame;
                if (blend - 0x4B < 0x10U) {
                    work->lapSpeed = 0x3C;
                } else if (((blend - 0x8F) & 0xFFFF) < 0xFU) {
                    work->lapSpeed = -0x10;
                } else {
                    work->lapSpeed = 0;
                }
                if (work->animationFrame >= 0xAA) {
                    work->animationId = 0x19;
                }
            }
            work->stateCounter++;
            if (work->stateCounter >= 6) {
                work->stateCounter            = 0;
                D_actor_510900_80167B7C.coord = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
                func_800FDB18(5, &(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords[4], NULL,
                              &D_actor_510900_80167B7C);
            }
            switch (work->deathSoundLoadStep) {
                case 0:
                    break;
                case 1:
                    CdCmd_EnqueueLoadFile(9, 0x1E, 3);
                    work->deathSoundLoadStep = 2;
                    break;
                case 2:
                    if (cdCmdIsIdle() == 1) {
                        coord = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
                        sndEvtRequestScriptStart(SOUND_PLAYER_DEATH, (s8)worldCoordGetOriginAudioPan(coord),
                                                 (s8)worldCoordGetOriginAudioDepth(coord));
                        work->deathSoundLoadStep = 0;
                    }
                    break;
            }
            break;
    }

    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

static void func_actor_510900_80137E20(Task* arg0)
{
    Actor510900Work* work;
    Enemy*           enemy;
    GfxCoord*        coord;
    s32              snd;
    s32              pan;
    s32              rng;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    coord = arg0->extra.tmd->coords;
    switch (work->subState) {
        case 0:
            work->animationId = 0x10;
            work->lapSpeed    = 0;
            work->subState    = 1;
            snd               = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780005;
            pan               = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            break;
        case 1:
            if (work->sideTravelled >= 0xC8 && work->playerSide == work->lapSide) {
                if ((u16)work->animationFrame >= 3 && (u16)work->animationFrame < 13) {
                    work->lapSpeed = -0x2C;
                } else if ((u16)work->animationFrame >= 0x12 && (u16)work->animationFrame < 0x27) {
                    work->lapSpeed = -0x1E;
                } else {
                    work->lapSpeed = 0;
                }
            } else {
                work->lapSpeed = 0;
            }
            if (work->animationFrame >= 0x53) {
                if (enemy->hp <= 0) {
                    work->state       = ACTOR_510900_STATE_DEATH;
                    work->subState    = 0;
                    work->animationId = 0x18;
                } else {
                    work->state       = ACTOR_510900_STATE_PATROL;
                    work->animationId = 1;
                    work->subState    = 0;
                    work->stateCounter =
                        D_actor_510900_801679F0[((u32)(rng = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
                    gRandomLcgState = rng;
                }
            }
            break;
    }
}

static void func_actor_510900_80137FBC(Task* arg0)
{
    Actor510900Work* work;
    GfxCoord*        coord;
    Enemy*           enemy;
    SVECTOR*         rot;
    SVECTOR*         head;
    s32              startPan;
    s32              loopPan;
    s16              step;
    u32              rng;

    head                          = SCRATCH_STACK_CURSOR(SVECTOR);
    SCRATCH_STACK_CURSOR(SVECTOR) = head - 1;
    work                          = arg0->work;
    enemy                         = arg0->spawnArg2.pointer;
    coord                         = arg0->extra.tmd->coords;
    switch (work->subState) {
        case 0:
            work->animationId  = 0x12;
            work->lapSpeed     = 0;
            work->subState     = 1;
            work->stateCounter = 0;
            work->sparkSound   = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780009;
            startPan           = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(work->sparkSound, startPan, (s8)worldCoordGetOriginAudioDepth(coord));
            break;
        case 1:
            if (work->animationFrame & 1) {
                rot             = head - 1;
                rot->vx         = 0;
                rot->vz         = 0;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                rot->vy         = ((gRandomLcgState >> 0x10) & 0x2FF) - 0x680;
                Gp_SpawnEff(EFFECT_FLASH_BURST, coord, 0x100, rot);
            }
            work->stateCounter++;
            if (work->stateCounter >= 3) {
                loopPan = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptMix(work->sparkSound, loopPan, (s8)worldCoordGetOriginAudioDepth(coord));
                work->stateCounter = 0;
            }
            if ((work->sideTravelled < 0xC8) || (work->playerSide != work->lapSide)) {
                work->lapSpeed = 0;
            } else {
                step = 0;
                if ((u32)((u16)work->animationFrame - 0x47) < 7) {
                    step = -0x53;
                }
                work->lapSpeed = step;
            }
            if (work->animationFrame >= 0x51) {
                if (work->sparkSound != 0) {
                    sndEvtRequestScriptStop(work->sparkSound, SOUND_SCRIPT_STOP_NO_FADE);
                    work->sparkSound = 0;
                }
                if (enemy->hp <= 0) {
                    work->state       = ACTOR_510900_STATE_DEATH;
                    work->subState    = 0;
                    work->animationId = 0x18;
                } else {
                    work->state       = ACTOR_510900_STATE_PATROL;
                    work->animationId = 1;
                    work->subState    = 0;
                    work->stateCounter =
                        D_actor_510900_801679F0[((u32)(rng = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 0x10) & 0xF];
                    gRandomLcgState = rng;
                }
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

static void func_actor_510900_80138250(Task* arg0)
{
    Actor510900Work* work;
    GfxCoord*        coord;
    Enemy*           enemy;
    SVECTOR*         rot;
    SVECTOR*         head;
    s32              startPan;
    s32              loopPan;
    u32              rng;

    head                          = SCRATCH_STACK_CURSOR(SVECTOR);
    SCRATCH_STACK_CURSOR(SVECTOR) = head - 1;
    work                          = arg0->work;
    enemy                         = arg0->spawnArg2.pointer;
    coord                         = arg0->extra.tmd->coords;
    switch (work->subState) {
        case 0:
            work->lapSpeed     = 0;
            work->animationId  = 0x13;
            work->subState     = 1;
            work->stateCounter = 0;
            work->sparkSound   = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780009;
            startPan           = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(work->sparkSound, startPan, (s8)worldCoordGetOriginAudioDepth(coord));
            break;
        case 1:
            if (work->animationFrame & 1) {
                rot             = head - 1;
                rot->vx         = 0;
                rot->vz         = 0;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                rot->vy         = ((gRandomLcgState >> 0x10) & 0x2FF) - 0x680;
                Gp_SpawnEff(EFFECT_FLASH_BURST, coord, 0x100, rot);
            }
            work->stateCounter++;
            if (work->stateCounter >= 3) {
                loopPan = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptMix(work->sparkSound, loopPan, (s8)worldCoordGetOriginAudioDepth(coord));
                work->stateCounter = 0;
            }
            if (work->animationFrame >= 0x1E) {
                if (work->sparkSound != 0) {
                    sndEvtRequestScriptStop(work->sparkSound, SOUND_SCRIPT_STOP_NO_FADE);
                    work->sparkSound = 0;
                }
                work->animationId = 0x14;
                work->subState    = 2;
            }
            break;
        case 2:
            if (work->animationFrame >= 0x3B) {
                if (enemy->hp <= 0) {
                    work->state       = ACTOR_510900_STATE_DEATH;
                    work->subState    = 0;
                    work->animationId = 0x18;
                } else {
                    work->state       = ACTOR_510900_STATE_PATROL;
                    work->animationId = 1;
                    work->subState    = 0;
                    work->stateCounter =
                        D_actor_510900_801679F0[((u32)(rng = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 0x10) & 0xF];
                    gRandomLcgState = rng;
                }
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

static void func_actor_510900_801384C4(Task* arg0)
{
    Actor510900Work* work;
    GfxCoord*        coord;
    s32              snd;
    s32              pan;
    u32              rng;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    if (gPlayerStatus.hp <= 0) {
        ((Enemy*)arg0->spawnArg2.pointer)->hp = 1;
        work->state                           = ACTOR_510900_STATE_PATROL;
        work->animationId                     = 1;
        work->subState                        = 0;
        work->stateCounter                    = D_actor_510900_801679F0[((u32)(rng = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
        gRandomLcgState                       = rng;
        return;
    }
    if (work->flameMode != ACTOR_510900_FLAME_OFF) {
        work->flameMode = ACTOR_510900_FLAME_DYING;
    }
    work->flameFrames = 0;
    if (work->sparkSound != 0) {
        sndEvtRequestScriptStop(work->sparkSound, SOUND_SCRIPT_STOP_NO_FADE);
        work->sparkSound = 0;
    }
    if (work->flameSound != 0) {
        sndEvtRequestScriptStop(work->flameSound, SOUND_SCRIPT_STOP_NO_FADE);
        work->flameSound = 0;
    }
    if (work->eventFlameSound != 0) {
        sndEvtRequestScriptStop(work->eventFlameSound, SOUND_SCRIPT_STOP_NO_FADE);
        work->eventFlameSound = 0;
    }
    ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    if (work->animationFrame == 0x70) {
        snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780007;
        pan = (s8)worldCoordGetOriginAudioPan(coord);
        sndEvtRequestScriptStart(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
    }
    Gp_StateC08.flags &= ATTACHMENT_FLAG_EVENT_LOCK;
    work->present      = 0;
}

/// Latches which of the four `D_actor_510900_80167BA4` boxes the player stands
/// in into `playerSide`, records how far `lapDistance` is from the near and far
/// ends of the current patrol side, and measures the straight-line distance
/// from the actor's attach coordinate to the player into `playerDistance`.
static void func_actor_510900_8013864C(Task* arg0)
{
    Actor510900Work* work;
    GfxCoord*        coord;
    VECTOR*          delta;
    VECTOR*          head;
    s32              i;
    s32              dx;
    s32              dz;

    work                         = arg0->work;
    head                         = SCRATCH_STACK_CURSOR(VECTOR);
    coord                        = arg0->extra.tmd->coords;
    SCRATCH_STACK_CURSOR(VECTOR) = head - 1;
    delta                        = head - 1;

    for (i = 0; i < 4; i++) {
        if (D_actor_510900_80167BA4[i].minX < gPlayerStatus.coordMtx->t[0] &&
            gPlayerStatus.coordMtx->t[0] < D_actor_510900_80167BA4[i].maxX &&
            D_actor_510900_80167BA4[i].minZ < gPlayerStatus.coordMtx->t[2] &&
            gPlayerStatus.coordMtx->t[2] < D_actor_510900_80167BA4[i].maxZ) {
            work->playerSide = i;
            break;
        }
    }

    work->sideTravelled = __builtin_abs(work->lapSide * ACTOR_510900_LAP_SIDE - work->lapDistance);
    work->sideRemaining = __builtin_abs((work->lapSide + 1) * ACTOR_510900_LAP_SIDE - work->lapDistance);

    dx                   = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    delta->vx            = dx;
    delta->vy            = 0;
    dz                   = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    delta->vz            = dz;
    work->playerDistance = SquareRoot0(dx * dx + dz * dz);
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

static void func_actor_510900_801387F4(Task* arg0)
{
    Actor510900Work* work;
    GfxCoord*        coord;
    SVECTOR*         rot;
    SVECTOR*         head;
    u16              target;
    u16              cur;
    s32              d;
    s16              diff;
    s32              sdiff;
    s32              mag;
    s32              prev;
    s32              prev2;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->lapSpeed != 0) {
        head                          = SCRATCH_STACK_CURSOR(SVECTOR);
        SCRATCH_STACK_CURSOR(SVECTOR) = head - 1;
        rot                           = head - 1;
        if (work->sideRemaining < 0x3E8) {
            target = D_actor_510900_80167B9C[work->lapSide + 1];
        } else {
            target = D_actor_510900_80167B9C[work->lapSide];
        }
        cur       = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
        d         = target - cur;
        diff      = d;
        sdiff     = (s16)d;
        mag       = __builtin_abs(sdiff);
        work->yaw = cur;
        if (mag < 0x800) {
            if (mag < 0x1F) {
                work->yaw = target;
            } else {
                prev = work->yaw;
                if (sdiff > 0) {
                    work->yaw = prev + 0x1E;
                } else {
                    work->yaw = prev - 0x1E;
                }
            }
        } else if (sdiff > 0 ? (0x1000 - sdiff) < 0x1F : (sdiff + 0x1000) < 0x1F) {
            work->yaw = target;
        } else {
            prev2 = work->yaw;
            if (diff > 0) {
                work->yaw = prev2 - 0x1E;
            } else {
                work->yaw = prev2 + 0x1E;
            }
        }
        rot->vx = 0;
        rot->vy = work->yaw;
        rot->vz = 0;
        RotMatrix(rot, &coord->coord);
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    }
}

/// Walks the actor once around a fixed square patrol path: `lapDistance` is the
/// distance travelled, advanced by `lapSpeed` and clamped, and its quotient by
/// the side length selects the corner (also latched into `lapSide` for the
/// turn handler) while the remainder is the offset along that side.
static void func_actor_510900_80138978(Task* arg0)
{
    Actor510900Work* work;
    GfxCoord*        coord;
    s16              side;
    s16              along;
    u16              dist;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;

    dist              = work->lapDistance + (u16)work->lapSpeed;
    work->lapDistance = dist;
    if (dist < 0xC8) {
        work->lapDistance = 0xC8;
    } else if (dist > 0xB66C) {
        work->lapDistance = 0xB66C;
    }

    side          = work->lapDistance / ACTOR_510900_LAP_SIDE;
    work->lapSide = side;
    along         = work->lapDistance % ACTOR_510900_LAP_SIDE;

    coord->coord.t[0] = D_actor_510900_80167B84[side].x + (along * D_actor_510900_80167B94[side].x);
    coord->coord.t[1] = 0;
    coord->coord.t[2] =
        D_actor_510900_80167B84[work->lapSide].z + (along * D_actor_510900_80167B94[work->lapSide].z);
}

/// Fires the actor's step sounds: while the current animation record carries
/// `flags` cue bit 0x20 or 0x10, a sound is queued on the frame that bit has just
/// dropped from `Actor510900Work::lastCueFlags`, panned and depth-attenuated from
/// the actor's attach coordinate. The record's two bits are latched for the
/// next frame at the end.
static void func_actor_510900_80138A9C(Task* arg0)
{
    s32                    snd;
    s32                    pan;
    s32                    pan2;
    Actor510900Work*       work;
    GfxCoord*              coord;
    const AnimationRecord* rec;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    rec   = animationGetCurrentRecord(&work->rig.anim, &work->rig.slots[1]);
    if (rec != NULL) {
        if (!(rec->flags & ANIMATION_RECORD_CUE_2) && (work->lastCueFlags & ANIMATION_RECORD_CUE_2)) {
            snd = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780001;
            pan = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
        }
        if (!(rec->flags & ANIMATION_RECORD_CUE_1) && (work->lastCueFlags & ANIMATION_RECORD_CUE_1)) {
            snd  = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40780002;
            pan2 = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(snd, pan2, (s8)worldCoordGetOriginAudioDepth(coord));
        }
        work->lastCueFlags = (u16)(rec->flags & ANIMATION_RECORD_CUE_MASK);
    }
}

#include "../../shared/no9_golem_aim_head.inc.c"

/// Rotates the chest coordinate (`coords[3]`) by the residual rotation in
/// `Actor510900Work::hitTwist` and then walks that residual 0x20 back towards
/// zero on each axis, snapping to zero inside the last step. `hitTwistActive` is
/// cleared on the frame both axes have come to rest.
static void func_actor_510900_80138D38(Task* arg0)
{
    Actor510900Work* work;
    GfxCoord*        coord;
    MATRIX*          matrix;
    s32              angleX;
    s32              angleY;
    s32              absX;
    s32              nextX;
    s32              absY;
    s32              nextY;
    s32              active;

    SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    matrix = SCRATCH_STACK_CURSOR(MATRIX);
    active = 0;
    work   = arg0->work;
    coord  = arg0->extra.tmd->coords;
    RotMatrix(&work->hitTwist, matrix);
    gte_SetRotMatrix(&coord[3].coord);
    gte_ldclmv(matrix);
    gte_rtir();
    gte_stclmv(&coord[3].coord);
    gte_ldclmv(&matrix->m[0][1]);
    gte_rtir();
    gte_stclmv(&coord[3].coord.m[0][1]);
    gte_ldclmv(&matrix->m[0][2]);
    gte_rtir();
    gte_stclmv(&coord[3].coord.m[0][2]);

    angleX = work->hitTwist.vx;
    if (angleX != 0) {
        absX = __builtin_abs(angleX);
        if (absX < 0x21) {
            work->hitTwist.vx = 0;
        } else {
            nextX = angleX - 0x20;
            if (angleX <= 0) {
                nextX = angleX + 0x20;
            }
            work->hitTwist.vx = nextX;
            active            = 1;
        }
    }

    angleY = work->hitTwist.vy;
    if (angleY != 0) {
        absY = __builtin_abs(angleY);
        if (absY < 0x21) {
            work->hitTwist.vy = 0;
        } else {
            nextY = angleY - 0x20;
            if (angleY <= 0) {
                nextY = angleY + 0x20;
            }
            work->hitTwist.vy = nextY;
            active            = 1;
        }
    }

    if (active == 0) {
        work->hitTwistActive = 0;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x20);
}

/// Rebuilds the three collision faces this actor occupies in the grid, at the
/// body's current position and facing. `origin` is the faces' fixed offset from
/// the body rotated into world space, translated by the coordinate and clamped to
/// the grid extent; the twelve corners in `_gActor510900Collision35DBC` are rotated
/// and offset from it into `Gp_GridParams->vertices`, and the three face normals
/// in `_gActor510900Collision35DA4` are rotated in place into `field_4`.
static void func_actor_510900_80138F44(Task* arg0)
{
    _Actor510900GridFacesScratch* scratch;
    GfxCoord*                     coord;
    SVECTOR*                      normals;
    SVECTOR*                      corners;
    s32                           i;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor510900GridFacesScratch);
    coord   = arg0->extra.tmd->coords;
    normals = Gp_GridParams->normals;
    corners = Gp_GridParams->vertices;

    scratch->origin.vx = -0x258;
    scratch->origin.vy = 0;
    scratch->origin.vz = 0x280;

    gte_SetRotMatrix(&coord->coord);
    gte_ldv0(&scratch->origin);
    gte_rtv0();
    gte_stsv(&scratch->rotated);

    scratch->origin.vx = coord->coord.t[0] + scratch->rotated.vx;
    scratch->origin.vy = coord->coord.t[1] + scratch->rotated.vy;
    scratch->origin.vz = coord->coord.t[2] + scratch->rotated.vz;

    if (scratch->origin.vx > 0x1770) {
        scratch->origin.vx = 0x1770;
    } else if (scratch->origin.vx < -0x1770) {
        scratch->origin.vx = -0x1770;
    }
    if (scratch->origin.vz > 0x1770) {
        scratch->origin.vz = 0x1770;
    } else if (scratch->origin.vz < -0x1770) {
        scratch->origin.vz = -0x1770;
    }

    for (i = 0; i < 12; i++) {
        gte_SetRotMatrix(&coord->coord);
        gte_ldv0(&_gActor510900Collision35DBC[i]);
        gte_rtv0();
        gte_stsv(&scratch->rotated);
        corners[i].vx = scratch->rotated.vx + scratch->origin.vx;
        corners[i].vy = scratch->rotated.vy + scratch->origin.vy;
        corners[i].vz = scratch->rotated.vz + scratch->origin.vz;
    }

    for (i = 0; i < 3; i++) {
        gte_SetRotMatrix(&coord->coord);
        gte_ldv0(&_gActor510900Collision35DA4[i]);
        gte_rtv0();
        gte_stsv(&normals[i]);
    }

    SCRATCH_STACK_RELEASE_BLOCK(_Actor510900GridFacesScratch);
}

/// Message 0x7D7 handler (entry in `D_actor_510900_80167A6C`). `arg2` picks
/// between three visibility/liveness states of the boss and its two companion
/// enemies, whose models are reached through `weaponTask` / `chestModelTask`.
///
/// 0 parks the actor: both models and its own have their draw flags cleared and `activation`
/// becomes 1, after taking a `gSceneCombatState` reference (release id 0x1B).
/// 1 wakes it: the same flag reset, then the lap is restarted from
/// distance 0x11F8 with animation 0x1A and yaw 0x400, the root coordinate is
/// rebuilt from that yaw with its translation cleared, and slots 1..18 are
/// restarted. The first wake (`flameMode` still off) also lights the flame, enables both
/// attack spheres and queues the flame sound.
/// 2 puts it away: the models stop being drawn, the flame jet's task is handed
/// `ACTOR_510900_FLAME_RELEASED` and dropped, all three collision spheres are
/// disabled, and the collision grid this actor
/// edited is restored - the three faces `func_actor_510900_8013B524` copies back
/// plus the fourth `func_actor_510900_8013B424` zeroes, both written out inline.
///
/// `vec` is one `SVECTOR*` serving two unrelated roles - the scratch rotation
/// state 1 builds the root matrix from, and the extra face normal state 2
/// clears - which is what puts it in `$a0` in both. The `do`/`while (0)` cuts
/// the basic block so the second `Gp_GridParams->normals` read is scheduled on
/// its own; without it the corner pointer and its walking copy coalesce.
s32 func_actor_510900_801391B8(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    Actor510900Work*        work;
    TmdObject*              obj;
    GfxCoord*               coord;
    Enemy*                  enemy;
    SVECTOR*                rot;
    void*                   head;
    SVECTOR*                normals;
    SVECTOR*                verts;
    WorldCollisionGridFace* faces;
    s32                     pair;
    s32                     i;
    s32                     j;
    s32                     k;
    SVECTOR*                vec;
    SVECTOR*                corners;

    head                       = SCRATCH_STACK_CURSOR(void);
    SCRATCH_STACK_CURSOR(void) = (u8*)head - sizeof(SVECTOR);
    rot                        = SCRATCH_STACK_CURSOR(SVECTOR);
    obj                        = arg0->extra.tmd;
    work                       = arg0->work;
    enemy                      = arg0->spawnArg2.pointer;
    coord                      = obj->coords;

    switch (arg2) {
        case 0:
            (sceneAcquireBattleRef)(0x1B);
            obj->flags                             = 0;
            work->weaponTask->extra.tmd->flags     = 0;
            work->chestModelTask->extra.tmd->flags = 0;
            work->activation                       = 1;
            break;
        case 1:
            obj->flags                             = 0;
            work->weaponTask->extra.tmd->flags     = 0;
            work->chestModelTask->extra.tmd->flags = 0;
            work->activation                       = 2;
            work->lapDistance                      = 0x11F8;
            vec                                    = rot;
            work->animationId                      = 0x1A;
            work->seededAnimationId                = 0x1A;
            work->yaw                              = 0x400;
            work->state                            = ACTOR_510900_STATE_OPENING;
            work->subState                         = 0;
            work->lapSpeed                         = 0;
            work->animationFrame                   = 0;
            work->body.flags                      |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            enemy->node.state.parts.flags          = WORLD_TARGET_HIDE_HP;
            vec->vx                                = 0;
            vec->vy                                = work->yaw;
            vec->vz                                = 0;
            RotMatrix(vec, &coord->coord);
            coord->coord.t[0] = 0;
            coord->coord.t[1] = 0;
            coord->coord.t[2] = 0;
            for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
                animationResetSlot(&work->rig.anim, i, work->animationId);
            }
            if (work->flameMode == ACTOR_510900_FLAME_OFF) {
                work->flameMode = ACTOR_510900_FLAME_BURNING;
                if (work->flameJetTask != NULL) {
                    work->flameJetTask->spawnArg1.value = ACTOR_510900_FLAME_BURNING;
                }
                work->flameFrames          = 0xF0;
                work->weaponAttack.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->forearmAttack.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                pair                       = damagePackAttackKey(&D_actor_510900_80167968, 5);
                work->weaponAttack.key     = pair;
                work->forearmAttack.key    = pair;
                work->flameSound           = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4078000D;
                sndEvtRequestScriptStart(work->flameSound, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
            }
            break;
        case 2:
            obj->flags                             = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->weaponTask->extra.tmd->flags     = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->chestModelTask->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->activation                       = 0;
            if (work->flameJetTask != NULL) {
                work->flameJetTask->spawnArg1.value = ACTOR_510900_FLAME_RELEASED;
            }
            work->flameJetTask            = NULL;
            work->body.flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->weaponAttack.flags     &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->forearmAttack.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;

            normals = Gp_GridParams->normals;
            verts   = Gp_GridParams->vertices;
            faces   = Gp_GridParams->faces;
            for (j = 0; j < 12; j++) {
                verts[j] = _gActor510900Collision35DBC[j];
            }
            for (j = 0; j < 3; j++) {
                normals[j] = _gActor510900Collision35DA4[j];
                faces[j]   = _gActor510900Collision35E1C[j];
            }

            do {
                vec = Gp_GridParams->normals;
            } while (0);
            corners   = Gp_GridParams->vertices;
            vec[3].vx = 0;
            vec[3].vy = 0;
            vec[3].vz = 0;
            for (k = 0; k < 4; k++) {
                corners[12 + k].vx = 0;
                corners[12 + k].vy = 0;
                corners[12 + k].vz = 0;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(SVECTOR));
    return 0;
}

/// Frame handler of the muzzle-flash child task, dispatched from
/// `func_actor_510900_8013BE98` once the spawn has run. The parent's animation
/// id (`animationId`) selects the behaviour: below 0x1C the model is hidden,
/// 0x1C/0x1D hold, 0x1E unhides it once `animationFrame` reaches 0xC8, and 0x1F
/// drives the coordinate. There the frame's position within its 40-frame cycle
/// picks between an identity frame parented to the body's coordinate 12 (with
/// frame 0xE storing that coordinate's world transform in `propTossMtx`) and that
/// matrix parented to the view, lifted along y by `3*(n - 0xC)^2 - 0x1B0`.
/// Frame 0x52 restores the parented identity frame.
static void func_actor_510900_801395AC(Enemy* enemy, Task* task)
{
    TmdObject*        obj;
    Actor510900Work*  work;
    GfxCoord*         coord;
    GfxCoord*         parentCoord;
    GfxRotationWords* mat;
    GfxRotationWords* mat2;
    s16               blend;
    s16               r;
    s32               dy;

    work  = task->parent->work;
    obj   = task->extra.tmd;
    coord = obj->coords;
    if (work->animationId < 0x1C) {
        obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        return;
    }
    SCRATCH_STACK_RESERVE_BYTES(0x20);
    switch (work->animationId) {
        case 0x1C:
        case 0x1D:
            break;

        case 0x1E:
            if (work->animationFrame == 0xC8) {
                obj->flags = 0;
            }
            break;

        case 0x1F:
            parentCoord = &task->parent->extra.tmd->coords[12];
            blend       = work->animationFrame;
            if (blend < 0x50) {
                r = blend % 40;
                if (r < 0xF) {
                    mat               = (GfxRotationWords*)&coord->coord;
                    mat->m00M01       = ONE;
                    mat->m11M12       = ONE;
                    mat->m22          = ONE;
                    mat->m02M10       = 0;
                    mat->m20M21       = 0;
                    coord->coord.t[0] = 0;
                    coord->coord.t[1] = 0;
                    coord->coord.t[2] = 0;
                    coord->parent     = parentCoord;
                    if (r == 0xE) {
                        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &parentCoord->workm, &work->propTossMtx);
                    }
                } else {
                    r                  = r - 0xF;
                    dy                 = ((r - 0xC) * (r - 0xC) * 3) - 0x1B0;
                    coord->coord       = work->propTossMtx;
                    coord->parent      = &gGfxViewCoord;
                    coord->coord.t[1] += dy;
                }
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
            } else if (blend == 0x52) {
                mat2                = (GfxRotationWords*)&coord->coord;
                mat2->m00M01        = ONE;
                mat2->m02M10        = 0;
                mat2->m11M12        = ONE;
                mat2->m20M21        = 0;
                mat2->m22           = ONE;
                coord->coord.t[0]   = 0;
                coord->coord.t[1]   = 0;
                coord->coord.t[2]   = 0;
                coord->parent       = parentCoord;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x20);
}

/// Spawn state of the child effect task: allocates its `_Actor510900GrenadeWork`
/// work block, places the child on the parent's fourth coordinate offset by a
/// fixed local vector and yawed -0x160, and links its two collision objects.
/// `phaseCounter`, the pitch step of the flight, comes from
/// `D_actor_510900_80167C94` indexed by the horizontal distance to the player
/// in units of 1000, clamped to the last entry.
static void func_actor_510900_801397F0(Enemy* arg0, Task* arg1)
{
    _Actor510900GrenadeWork* work;
    ActorChildPlaceScratch*  scratch;
    TmdObject*               tmd;
    GfxCoord*                coord;
    GfxCoord*                parentCoords;
    GfxCoord*                parentCoord;
    s32                      dx;
    s32                      dz;
    s32                      idx;

    tmd          = arg1->extra.tmd;
    coord        = tmd->coords;
    parentCoords = arg1->parent->extra.tmd->coords;
    parentCoord  = &parentCoords[3];
    work         = memCalloc(sizeof(_Actor510900GrenadeWork), false);
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->work    = work;
    tmd->flags    = 0;
    scratch       = SCRATCH_STACK_RESERVE_BLOCK(ActorChildPlaceScratch);
    tmd->lightMtx = &work->lightMtx;
    tmd->colorMtx = &work->colorMtx;

    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&gGfxViewCoord);
    parentCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(parentCoord);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &parentCoord->workm, &coord->coord);

    scratch->operand.vx = -0xA5;
    scratch->operand.vy = -0x235;
    scratch->operand.vz = 0xA0;
    gte_SetRotMatrix(&coord->coord);
    gte_ldv0(&scratch->operand);
    gte_rtv0();
    gte_stlvnl(&scratch->offset);
    coord->parent      = &gGfxViewCoord;
    coord->coord.t[0] += scratch->offset.vx;
    coord->coord.t[1] += scratch->offset.vy;
    coord->coord.t[2] += scratch->offset.vz;

    scratch->operand.vx = -0x160;
    scratch->operand.vy = 0;
    scratch->operand.vz = 0;
    RotMatrix(&scratch->operand, &scratch->rotation);
    gte_SetRotMatrix(&coord->coord);
    gte_ldclmv(&scratch->rotation);
    gte_rtir();
    gte_stclmv(&coord->coord);
    gte_ldclmv(&scratch->rotation.m[0][1]);
    gte_rtir();
    gte_stclmv(&coord->coord.m[0][1]);
    gte_ldclmv(&scratch->rotation.m[0][2]);
    gte_rtir();
    gte_stclmv(&coord->coord.m[0][2]);

    dx                 = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    scratch->offset.vy = 0;
    scratch->offset.vx = dx;
    dz                 = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    scratch->offset.vz = dz;
    idx                = SquareRoot0((dx * dx) + (dz * dz)) / 1000;
    if (idx >= 0xC) {
        idx = 0xB;
    }

    work->phaseCounter            = D_actor_510900_80167C94[idx];
    work->attack.coord            = coord;
    work->attack.context.contacts = work->attackContacts;
    work->attack.pos.vx           = 0;
    work->attack.pos.vy           = 0;
    work->attack.pos.vz           = 0;
    work->attack.key              = 0;
    work->attack.radius           = 0xC8;
    work->attack.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->attack);
    worldCollisionInitContacts(work->attackContacts, ARRAY_SIZE(work->attackContacts), 0);

    work->gridProbeCapsule.ends[0].vx = 0;
    work->gridProbeCapsule.ends[0].vy = 0;
    work->gridProbeCapsule.ends[0].vz = 0;
    work->gridProbeCapsule.ends[1].vx = 0;
    work->gridProbeCapsule.ends[1].vy = 0x1F4;
    work->gridProbeCapsule.ends[1].vz = 0;
    work->gridProbeCapsule.end0Radius = 1;
    work->gridProbeCapsule.end1Radius = 1;
    work->gridProbeCapsule.contacts   = work->gridProbeContacts;
    work->gridProbe.context.capsule   = &work->gridProbeCapsule;
    work->gridProbe.coord             = coord;
    work->gridProbe.pos.vx            = 0;
    work->gridProbe.pos.vy            = 0;
    work->gridProbe.pos.vz            = 0;
    work->gridProbe.key               = 0;
    work->gridProbe.radius            = 0;
    work->gridProbe.flags             = WORLD_COLLISION_BODY_CAPSULE;
    work->attack.flags               |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->gridProbe);
    worldCollisionInitContacts(work->gridProbeContacts, ARRAY_SIZE(work->gridProbeContacts), 0);
    work->gridProbe.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;

    arg1->state = 1;
    SCRATCH_STACK_RELEASE_BLOCK(ActorChildPlaceScratch);
}

/// Per-frame handler of the effect child while it is alive: spins the object by
/// `phaseCounter` about X, drags it 150 units down its own Y axis and drips a trail
/// effect every third frame. Once it has fallen past -0x514 and come back up,
/// or either `WorldCollisionContact` table reports a hit, it fires the impact effects,
/// reparents the task under the spawned one and hands the actor to state 2.
/// A parent that has stopped (`present` == 0) tears the object down the same
/// way. `gSceneCombatState.actorControl` 1 only refreshes the colour and 2 only hides the model.
static void func_actor_510900_80139C10(Enemy* enemy, Task* task)
{
    VECTOR                   pos;
    EffectWork*              eff;
    GfxCoord*                coord;
    ActorEulerTurnScratch*   scratch;
    TmdObject*               tmd;
    _Actor510900GrenadeWork* work;
    Actor510900Work*         parent;
    s32                      angle;
    s32                      snd;
    s32                      done;
    u16                      tick;

    tmd    = task->extra.tmd;
    work   = task->work;
    coord  = tmd->coords;
    parent = task->parent->work;
    done   = 0;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            tmd->flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            pos.vx = coord->workm.t[0];
            pos.vy = coord->workm.t[1];
            pos.vz = coord->workm.t[2];
            worldCoordUpdateActorColor(task->spawnArg2.pointer, &pos, 0, 0);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }

    scratch            = SCRATCH_STACK_RESERVE_BLOCK(ActorEulerTurnScratch);
    angle              = -work->phaseCounter;
    scratch->angles.vx = angle;
    scratch->angles.vy = 0;
    scratch->angles.vz = 0;
    RotMatrix(&scratch->angles, &scratch->rotation);
    gte_SetRotMatrix(&coord->coord);
    gte_ldclmv(&scratch->rotation);
    gte_rtir();
    gte_stclmv(&coord->coord);
    gte_ldclmv(&scratch->rotation.m[0][1]);
    gte_rtir();
    gte_stclmv(&coord->coord.m[0][1]);
    gte_ldclmv(&scratch->rotation.m[0][2]);
    gte_rtir();
    gte_stclmv(&coord->coord.m[0][2]);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[0]  += -(coord->coord.m[0][1] * 0x96) >> 12;
    coord->coord.t[1]  += -(coord->coord.m[1][1] * 0x96) >> 12;
    coord->coord.t[2]  += -(coord->coord.m[2][1] * 0x96) >> 12;
    tick                = work->frames + 1;
    work->frames        = tick;
    if (tick >= 3) {
        scratch->angles.vx = 0;
        scratch->angles.vy = 0x64;
        scratch->angles.vz = 0;
        Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, 0x01001600, &scratch->angles);
        work->frames = 0;
    }
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    worldCoordUpdateActorColor(task->spawnArg2.pointer, &pos, 0, 0);
    if (coord->coord.t[1] < -0x514) {
        work->phase = ACTOR_510900_GRENADE_FLIGHT_PEAKED;
    }
    if (work->phase != ACTOR_510900_GRENADE_FLIGHT_BELOW_PEAK && coord->coord.t[1] >= -0x513) {
        done = 1;
    }
    if (done != 0 || (work->attackContacts[0].key.value & 0xFFFF0000) == 0x10000 || work->gridProbeContacts[0].key.value != 0) {
        Gp_SpawnEff(EFFECT_EXPLOSION, coord, 0x10002200, NULL);
        Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, 0xC1001200, NULL);
        eff = Gp_SpawnEff((EFFECT_ACTOR_510900_IMPACT_BURST | EFFECT_SPAWN_UNLIMITED), coord, 0, NULL);
        if (eff != NULL) {
            taskReparent(task, eff->task);
        }
        if (work->attackContacts[0].key.value != 0) {
            work->phase = ACTOR_510900_GRENADE_BURST_HOLDING;
        } else {
            work->phase = ACTOR_510900_GRENADE_BURST_SPREADING;
        }
        work->attack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        worldCollisionClearContacts(work->attackContacts);
        worldCollisionUnlinkBody(&work->gridProbe);
        work->frames           = 0;
        task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        snd                    = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x51100009;
        sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
        task->state = 2;
    }
    worldCollisionClearContacts(work->attackContacts);
    if (parent->present == 0) {
        work->attack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        worldCollisionUnlinkBody(&work->gridProbe);
        work->frames           = 0;
        task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        task->state            = 2;
        work->phase            = ACTOR_510900_GRENADE_BURST_ENDING;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorEulerTurnScratch);
}

/// Frame handler of the effect child task: state 0 fades the object in over
/// 0x10 frames, state 1 holds it until its `WorldCollisionContact` reports a hit or 0x1F
/// frames pass, state 2 runs the hit handler, and state 3 unlinks the object
/// and destroys the enemy.
static void func_actor_510900_8013A100(Enemy* enemy, Task* task)
{
    _Actor510900GrenadeWork* work;
    Actor510900Work*         parent;
    u16                      tick;

    work   = task->work;
    parent = task->parent->work;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        switch (work->phase) {
            case ACTOR_510900_GRENADE_BURST_SPREADING:
                tick         = work->frames + 1;
                work->frames = tick;
                if (tick >= 0x10) {
                    work->attack.radius = 0x258;
                    work->phase         = ACTOR_510900_GRENADE_BURST_CATCHING;
                    work->frames        = 0;
                    work->attack.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                    return;
                }
                if (parent->present == 0) {
                    work->attack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    worldCollisionUnlinkBody(&work->gridProbe);
                    work->frames           = 0;
                    task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    task->state            = 2;
                    work->phase            = ACTOR_510900_GRENADE_BURST_ENDING;
                    return;
                }
                break;
            case ACTOR_510900_GRENADE_BURST_CATCHING:
                if ((work->attackContacts[0].key.value & 0xFFFF0000) == 0x10000) {
                    work->phase         = ACTOR_510900_GRENADE_BURST_HOLDING;
                    work->holdStep      = ACTOR_510900_GRENADE_HOLD_REQUEST;
                    work->attack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                } else {
                    tick         = work->frames + 1;
                    work->frames = tick;
                    if (tick >= 0x1F) {
                        work->phase  = ACTOR_510900_GRENADE_BURST_ENDING;
                        work->frames = 0;
                    } else if (parent->present == 0) {
                        work->attack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        worldCollisionUnlinkBody(&work->gridProbe);
                        work->frames           = 0;
                        task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                        task->state            = 2;
                        work->phase            = ACTOR_510900_GRENADE_BURST_ENDING;
                    }
                }
                worldCollisionClearContacts(work->attackContacts);
                return;
            case ACTOR_510900_GRENADE_BURST_HOLDING:
                func_actor_510900_8013A310(task);
                return;
            case ACTOR_510900_GRENADE_BURST_ENDING:
                tick         = work->frames + 1;
                work->frames = tick;
                if (tick >= 0x1F) {
                    parent->grenadeLive = 0;
                    worldCollisionUnlinkBody(&work->attack);
                    enemyDestroy(enemy, task);
                }
                break;
        }
    }
}

/// Runs the player-hold sequence the effect's state 2 drives, on a 0x2C-byte
/// scratch block: `holdStep` 0 asks the player for the hold (message 0x3F8) and
/// on success starts the grab animation and its sound, 1 holds until the parent
/// reports the hit or 0x3C frames pass and then switches to the second
/// animation, and 2 waits 0x14 frames before releasing the player. Any refused
/// message leaves the effect in state 3 so the caller tears it down.
static void func_actor_510900_8013A310(Task* task)
{
    _Actor510900GrenadeWork* work;
    Actor510900Work*         parent;
    Task*                    player;
    ActorPlayerHoldScratch*  scratch;
    GfxCoord*                obj;
    u16                      tick;
    s32                      snd;
    s32                      pan;

    work   = task->work;
    parent = task->parent->work;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    SCRATCH_STACK_RESERVE_BLOCK(ActorPlayerHoldScratch);
    scratch = SCRATCH_STACK_CURSOR(ActorPlayerHoldScratch);

    switch (work->holdStep) {
        case ACTOR_510900_GRENADE_HOLD_REQUEST:
            if (((GameActor*)player->work)->mode != GAME_ACTOR_MODE_SCRIPTED) {
                scratch->buttonPressHold.pressCount = 0xC;
                if (TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &scratch->buttonPressHold, 0) != 0) {
                    work->phase = ACTOR_510900_GRENADE_BURST_ENDING;
                    break;
                }
                taskMessageDispatch(player, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackAttackKey(&D_actor_510900_80167968, 4), 0);
                scratch->playerAnim.source.sets          = D_actor_510900_80167B2C;
                scratch->playerAnim.animationId          = 1;
                scratch->playerAnim.blend                = ANIMATION_BLEND_RESET;
                scratch->playerAnim.blendFrames          = 0;
                scratch->playerAnim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &scratch->playerAnim, 0);
                work->holdStep     = ACTOR_510900_GRENADE_HOLD_STUNNED;
                work->phaseCounter = 0;
                obj                = player->extra.tmd->coords;
                snd                = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x5110000A;
                pan                = (s8)worldCoordGetOriginAudioPan(obj);
                sndEvtRequestScriptStart(snd, pan, (s8)worldCoordGetOriginAudioDepth(obj));
            }
            break;
        case ACTOR_510900_GRENADE_HOLD_STUNNED:
            tick               = work->phaseCounter + 1;
            work->phaseCounter = tick;
            if ((s16)tick < 0x3D && parent->playerEscaped != 1 && parent->present != 0) {
                break;
            }
            parent->playerEscaped                    = 0;
            scratch->playerAnim.source.sets          = D_actor_510900_80167B2C;
            scratch->playerAnim.animationId          = 2;
            scratch->playerAnim.blend                = ANIMATION_BLEND_RESET;
            scratch->playerAnim.blendFrames          = 0;
            scratch->playerAnim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
            TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &scratch->playerAnim, 0);
            work->holdStep     = ACTOR_510900_GRENADE_HOLD_RECOVER;
            work->phaseCounter = 0;
            break;
        case ACTOR_510900_GRENADE_HOLD_RECOVER:
            tick               = work->phaseCounter + 1;
            work->phaseCounter = tick;
            if ((s16)tick >= 0x15) {
                if (taskMessageDispatch(player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                    taskMessageDispatch(player, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                    work->phase = ACTOR_510900_GRENADE_BURST_ENDING;
                }
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorPlayerHoldScratch);
}

/// Spawn handler of the child task: allocates the animation work block, seeds
/// the model's root coordinate from the spawn-index tables, resets animation
/// slots 1..10 and links the two render objects.
static void func_actor_510900_8013A5B8(Enemy* enemy, Task* task)
{
    TmdObject*                    tmd;
    GfxCoord*                     coords;
    _Actor510900HelipadLightWork* work;
    GfxCoord*                     coord;
    SVECTOR*                      rot;
    void*                         head;
    s32                           i;

    tmd    = task->extra.tmd;
    coords = tmd->coords;
    work   = memCalloc(sizeof(_Actor510900HelipadLightWork), 0);
    coord  = &coords[10];
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work                 = work;
    tmd->flags                 = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    coords->composeStamp       = GRAPHICS_COORD_DIRTY;
    tmd->lightMtx              = &work->lightMtx;
    tmd->colorMtx              = &work->colorMtx;
    head                       = SCRATCH_STACK_CURSOR(void);
    enemy->field_4             = &coords->coord;
    rot                        = (SVECTOR*)(head - 8);
    SCRATCH_STACK_CURSOR(void) = head - 8;
    enemy->field_48            = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->coord                  = coord;
    enemy->bodyPos.vx             = -0xC8;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    work->lightIndex              = task->spawnArg1.value;
    ((SVECTOR*)(head - 8))->vx    = 0;
    rot->vy                       = D_actor_510900_80167CD0[work->lightIndex];
    rot->vz                       = 0;
    RotMatrix(rot, &coords->coord);
    i                  = 1;
    coords->coord.t[0] = D_actor_510900_80167CB8[work->lightIndex].vx;
    coords->coord.t[1] = D_actor_510900_80167CB8[work->lightIndex].vy;
    coords->coord.t[2] = D_actor_510900_80167CB8[work->lightIndex].vz;
    coords->parent     = &gGfxViewCoord;
    animationInitContext(&work->anim, D_actor_510900_80167CAC, tmd, work->poses, work->slots);
    do {
        animationResetSlot(&work->anim, i, 1);
        i++;
    } while (i < 0xB);
    work->body.pos.vx           = -0xC8;
    work->body.coord            = coord;
    work->body.pos.vy           = 0;
    work->body.pos.vz           = 0;
    work->body.context.contacts = work->bodyContacts;
    work->body.key              = 0;
    work->body.radius           = 0xC8;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(work->bodyContacts, ARRAY_SIZE(work->bodyContacts), 0);
    work->blast.key              = 0x50002;
    work->blast.coord            = coord;
    work->blast.pos.vx           = 0;
    work->blast.pos.vy           = 0;
    work->blast.pos.vz           = 0;
    work->blast.context.contacts = work->blastContacts;
    work->blast.radius           = 0x15E;
    work->blast.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->body.flags            &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    worldCollisionLinkBody(WORLD_COLLISION_LIST_BLASTS, &work->blast);
    worldCollisionInitContacts(work->blastContacts, ARRAY_SIZE(work->blastContacts), 0);
    work->blast.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    task->exitCallback = func_actor_510900_8013C380;
    task->state        = 1;
    SCRATCH_STACK_RELEASE_BYTES(8);
}

/// Frame handler (state 1) of the child task. Mode 1 of `gSceneCombatState.actorControl` only
/// redraws, mode 2 hides the model and flags the context, and mode 0 ticks the
/// animation until `func_actor_510900_8013C240` reports ready before falling
/// into the normal body.
static void func_actor_510900_8013A85C(Enemy* arg0, Task* arg1)
{
    TmdObject*                    obj;
    _Actor510900HelipadLightWork* work;
    GfxCoord*                     coord;
    Actor510900Work*              parent;
    s32                           i;

    obj    = arg1->extra.tmd;
    work   = arg1->work;
    coord  = obj->coords;
    parent = arg1->parent->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (func_actor_510900_8013C240(arg1) == 0) {
                i = 1;
                do {
                    animationTickSlot(&work->anim, i);
                    i++;
                } while (i < 0xB);
                return;
            }
            arg1->extra.tmd->flags       = 0;
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            func_actor_510900_8013C338(arg1, coord);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags                   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    func_actor_510900_8013A9BC(arg1);
    if (work->lightIndex < 2) {
        gameFlagSetNibble(work->lightIndex + 0xB, work->status);
    } else {
        parent->light2Status = work->status;
    }
    i = 1;
    do {
        animationTickSlot(&work->anim, i);
        i++;
    } while (i < 0xB);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    func_actor_510900_8013C338(arg1, coord);
}

/// Grab state machine of the child task, run from the frame handler above.
/// State 0 waits for the grab: the player has to be inside the `bodyContacts` node
/// (and survive `Gp_ComputeDamage`) or the parent has to request this child by
/// number through `slashedLight`; on a hit it resets the animation slots, spawns
/// the grab effect and its sound and starts the `sparkFrames` countdown. State 1
/// runs that countdown, keeping the four trailing part coordinates updated, and
/// hands the held effect task its exit state once the timer runs out or the
/// camera cuts away. State 2 only releases the held task. `lightIndex` 2 mirrors
/// the state back to the parent's `light2State`.
static void func_actor_510900_8013A9BC(Task* task)
{
    _Actor510900HelipadLightWork* work;
    Actor510900Work*              parent;
    Enemy*                        ctx;
    ActorFaceScratch*             scratch;
    ActorFaceScratch*             head;
    GfxCoord*                     coord;
    EffectWork*                   eff;
    Task*                         spawned;
    s16                           next;
    s32                           grabbed;
    s32                           i;
    s32                           snd;
    s32                           pan;
    s32                           dmg;
    s16                           state;

    grabbed                                = 0;
    head                                   = SCRATCH_STACK_CURSOR(ActorFaceScratch);
    coord                                  = &task->extra.tmd->coords[10];
    SCRATCH_STACK_CURSOR(ActorFaceScratch) = head - 1;
    scratch                                = head - 1;
    work                                   = task->work;
    ctx                                    = task->spawnArg2.pointer;
    state                                  = work->state;
    parent                                 = task->parent->work;
    switch (state) {
        case 0:
            if (parent->present == 0) {
                work->status = ACTOR_510900_HELIPAD_LIGHT_STATUS_STOPPED;
                work->state  = ACTOR_510900_HELIPAD_LIGHT_DONE;
                break;
            }
            ctx->node.state.parts.flags = gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_ENGAGED;
            dmg                         = work->bodyContacts[0].key.value;
            work->body.flags           |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            if ((dmg & 0xFFFF8000) == 0x20000 && ctx->node.state.parts.targeted == 1 &&
                Gp_ComputeDamage(dmg, 0x3E8, 0, 0) != 0) {
                grabbed = 1;
            }
            if (parent->slashedLight == work->lightIndex + 1 && parent->lightSlashStruck == 1) {
                grabbed              = 1;
                parent->slashedLight = -1;
            }
            if (grabbed == 1) {
                work->status = grabbed;
                work->state  = grabbed;
                i            = 1;
                do {
                    animationSeekSlotWithBlend(&work->anim, i, 2, 0, 0);
                    i++;
                } while (i < 0xB);
                scratch->rot.vx = 0;
                scratch->rot.vy = 0x80;
                scratch->rot.vz = 0;
                eff             = Gp_SpawnEff((EFFECT_HELIPAD_LIGHT_SPARKS | EFFECT_SPAWN_UNLIMITED), coord, 0, &scratch->rot);
                if (eff != NULL) {
                    spawned          = eff->task;
                    work->sparksTask = spawned;
                    taskReparent(task, spawned);
                }
                Gp_SpawnEff(EFFECT_EXPLOSION, coord, 0x200, &scratch->rot);
                work->sparkFrames  = 0x78;
                work->body.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->blast.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                snd                = (((u16)ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x51100004;
                pan                = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            worldCollisionClearContacts(work->bodyContacts);
            break;
        case 1:
            if (worldCollisionFindContactIndex(work->blastContacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
                work->blast.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            worldCollisionClearContacts(work->blastContacts);
            acropolisHelicopterLandingPadDrawLowerSparkLine(&task->extra.tmd->coords[9]);
            acropolisHelicopterLandingPadDrawLowerSparkLine(&task->extra.tmd->coords[8]);
            acropolisHelicopterLandingPadDrawLowerSparkLine(&task->extra.tmd->coords[7]);
            acropolisHelicopterLandingPadDrawLowerSparkLine(&task->extra.tmd->coords[6]);
            work->sparkFrames--;
            next = 2;
            if (work->sparkFrames <= 0) {
                Task* held;

                work->status       = next;
                work->blast.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                held               = work->sparksTask;
                if (held != NULL) {
                    held->state = state;
                }
            } else {
                Task* held;

                if (parent->present != 0) {
                    break;
                }
                work->status       = next;
                work->blast.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                held               = work->sparksTask;
                if (held != NULL) {
                    held->state      = 2;
                    work->sparksTask = NULL;
                }
            }
            work->state = next;
            break;
        case 2:
            if (parent->present == 0) {
                Task* held;

                held = work->sparksTask;
                if (held != NULL) {
                    held->state      = state;
                    work->sparksTask = NULL;
                }
            }
            break;
    }
    if (work->lightIndex == 2) {
        parent->light2State = work->state;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}

static void func_actor_510900_8013AD90(Enemy* enemy, Task* task)
{
    GfxCoord*                    coord;
    GfxRotationWords*            mat;
    _Actor510900BlastSourceWork* work;

    coord = task->extra.tmd->coords;
    work  = memCalloc(sizeof(_Actor510900BlastSourceWork), false);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    mat                 = (GfxRotationWords*)&coord->coord;
    task->work          = work;
    mat->m00M01         = ONE;
    mat->m11M12         = ONE;
    mat->m22            = ONE;
    mat->m02M10         = 0;
    mat->m20M21         = 0;
    coord->coord.t[0]   = -0x17D4;
    coord->coord.t[1]   = -0x456;
    coord->coord.t[2]   = 0x17C;
    coord->parent       = &gGfxViewCoord;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    enemy->field_4      = &coord->coord;
    enemy->field_48     = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord                  = coord;
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    work->body.coord              = coord;
    work->body.pos.vx             = 0;
    work->body.pos.vy             = 0;
    work->body.pos.vz             = 0;
    work->body.context.contacts   = work->bodyContacts;
    work->body.key                = 0;
    work->body.radius             = 0x12C;
    work->body.flags              = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(work->bodyContacts, ARRAY_SIZE(work->bodyContacts), 0);
    work->blast.pos.vy           = -0x200;
    work->blast.coord            = coord;
    work->blast.pos.vx           = 0;
    work->blast.pos.vz           = 0;
    work->blast.context.contacts = work->blastContacts;
    work->blast.key              = 0;
    work->blast.radius           = 0x200;
    work->blast.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->body.flags            &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    worldCollisionLinkBody(WORLD_COLLISION_LIST_BLASTS, &work->blast);
    worldCollisionInitContacts(work->blastContacts, ARRAY_SIZE(work->blastContacts), 0);
    work->blast.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    task->exitCallback = func_actor_510900_8013C430;
    task->state        = 1;
}

static void func_actor_510900_8013AF38(Enemy* arg0, Task* arg1)
{
    _Actor510900BlastSourceWork* work;
    Actor510900Work*             parent;
    u32                          random;
    Task*                        child;

    work   = arg1->work;
    parent = arg1->parent->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if ((viewGetMappedIndex() & 0xFF) != D_actor_510900_80167CE4[0]) {
                arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
                work->body.flags            &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->blast.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                if (work->flareFrames != 0) {
                    work->flareFrames--;
                }
                child = work->flareTask;
                if (child != NULL) {
                    child->state    = 3;
                    work->flareTask = NULL;
                }
                return;
            }
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    func_actor_510900_8013B0D8(arg1);
    gameFlagSetNibble(GAME_FLAG_00D, D_actor_510900_80167CEC[work->state][parent->light2Status]);
    random          = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gRandomLcgState = random;
    if ((u16)((random >> 16) % 3) == 0) {
        Gp_SpawnEff(EFFECT_ACROPOLIS_HELIPAD_EMBER, arg1->extra.tmd->coords, 0, NULL);
    }
}

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - indexed by `Task::state`.
static const EnemyTaskFuncTable3 D_actor_510900_80131ECC = {
    { func_actor_510900_801397F0, func_actor_510900_80139C10, func_actor_510900_8013A100 },
};

/// `func_actor_510900_8013AF38`.
/// State 1 watches the parent's `light2State` phase and its own collision record
/// for a 0x20000 hit; landing one spawns the grab effects, reparents this task
/// under the effect's task and hands the victim the state in `flareTaskState`. The
/// timer in `flareFrames` then runs the hold out (state 3), and states 4 and 5
/// finish or release the victim.
static void func_actor_510900_8013B0D8(Task* arg0)
{
    _Actor510900BlastSourceWork* work;
    GfxCoord*                    coord;
    Actor510900Work*             parent;
    Enemy*                       ctx;
    EffectWork*                  eff;
    Task*                        held;
    Task*                        ending;
    Task*                        dropped;
    Task*                        released;
    Task*                        spawned;
    s32                          hit;
    s32                          tag;
    s32                          snd;
    s32                          pan;
    u16                          left;

    work   = arg0->work;
    coord  = arg0->extra.tmd->coords;
    parent = arg0->parent->work;
    ctx    = arg0->spawnArg2.pointer;

    switch (work->state) {
        case ACTOR_510900_BLAST_SOURCE_DORMANT:
            if (parent->light2State == ACTOR_510900_HELIPAD_LIGHT_SPARKING) {
                work->state = ACTOR_510900_BLAST_SOURCE_ARMED;
            }
            break;
        case ACTOR_510900_BLAST_SOURCE_ARMED:
            ctx->node.state.parts.flags = gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_ENGAGED;
            hit                         = work->bodyContacts[0].key.value;
            work->body.flags           |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            if ((hit & ~0x7FFF) == 0x20000) {
                tag = ctx->node.state.parts.targeted;
                if (tag == 1 && Gp_ComputeDamage(hit, 0x3E8, 0, 0) != 0) {
                    work->state       = ACTOR_510900_BLAST_SOURCE_SHOT;
                    work->flareFrames = 0x3C;
                    Gp_SpawnEff(EFFECT_IMPACT_SPARK, coord, 0, NULL);
                    eff = Gp_SpawnEff((EFFECT_05F | EFFECT_SPAWN_UNLIMITED), coord, 0, NULL);
                    if (eff != NULL) {
                        spawned         = eff->task;
                        work->flareTask = spawned;
                        taskReparent(arg0, spawned);
                    }
                    work->body.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->blast.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                    if (parent->light2State == ACTOR_510900_HELIPAD_LIGHT_DONE) {
                        work->flareTaskState = tag;
                        work->blast.key      = 0x50003;
                    } else {
                        work->flareTaskState = 0;
                        work->blast.key      = 0x50004;
                    }
                    snd = (((u16)ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x51100002;
                    pan = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
                }
            }
            worldCollisionClearContacts(work->bodyContacts);
            break;
        case ACTOR_510900_BLAST_SOURCE_SHOT:
            held        = work->flareTask;
            work->state = ACTOR_510900_BLAST_SOURCE_FLARING;
            if (held != NULL) {
                held->state = work->flareTaskState;
            }
            if (worldCollisionFindContactIndex(work->blastContacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
                work->blast.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            worldCollisionClearContacts(work->blastContacts);
            break;
        case ACTOR_510900_BLAST_SOURCE_FLARING:
            if (worldCollisionFindContactIndex(work->blastContacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
                work->blast.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            worldCollisionClearContacts(work->blastContacts);
            left              = work->flareFrames - 1;
            work->flareFrames = left;
            if ((s16)left <= 0) {
                work->state        = ACTOR_510900_BLAST_SOURCE_SPENT;
                work->blast.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                ending             = work->flareTask;
                if (ending != NULL) {
                    ending->state = 2;
                }
                break;
            }
            if (parent->present == 0) {
                work->state        = ACTOR_510900_BLAST_SOURCE_STOPPED;
                work->blast.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                dropped            = work->flareTask;
                if (dropped != NULL) {
                    dropped->state  = 3;
                    work->flareTask = NULL;
                }
            }
            break;
        case ACTOR_510900_BLAST_SOURCE_SPENT:
            if (parent->present == 0) {
                work->state = ACTOR_510900_BLAST_SOURCE_STOPPED;
                released    = work->flareTask;
                if (released != NULL) {
                    released->state = 3;
                    work->flareTask = NULL;
                }
            }
            break;
        case ACTOR_510900_BLAST_SOURCE_STOPPED:
            break;
    }
}

void func_actor_510900_8013B3D0(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = { func_actor_510900_801350F8, func_actor_510900_8013B658 };

    fns[task->state](task->spawnArg2.pointer, task);
}

/// Adds (arg0 == 1) or clears the extra collision-grid face this actor edits
/// in, extending the three faces `func_actor_510900_8013B524` restores with a
/// fourth. Clearing zeroes only the vertices and normal; the face record stays.
void func_actor_510900_8013B424(s32 arg0)
{
    s32                     i;
    SVECTOR*                normals = Gp_GridParams->normals;
    SVECTOR*                verts   = Gp_GridParams->vertices;
    WorldCollisionGridFace* faces   = Gp_GridParams->faces;

    if (arg0 == 1) {
        for (i = 0; i < 4; i++) {
            verts[12 + i] = D_actor_510900_80167C68[i];
        }
        normals[3] = D_actor_510900_80167C60;
        faces[3]   = D_actor_510900_80167C88;
    } else {
        normals[3].vx = 0;
        normals[3].vy = 0;
        normals[3].vz = 0;
        for (i = 0; i < 4; i++) {
            verts[12 + i].vx = 0;
            verts[12 + i].vy = 0;
            verts[12 + i].vz = 0;
        }
    }
}

/// Restores the collision-grid faces this actor edited. The spawn handler
/// passes its task, which this never reads; the parameter is kept because the
/// call site materialises it.
void func_actor_510900_8013B524(Task* arg0)
{
    s32                     i;
    SVECTOR*                normals = Gp_GridParams->normals;
    SVECTOR*                verts   = Gp_GridParams->vertices;
    WorldCollisionGridFace* faces   = Gp_GridParams->faces;

    for (i = 0; i < 12; i++) {
        verts[i] = _gActor510900Collision35DBC[i];
    }

    for (i = 0; i < 3; i++) {
        normals[i] = _gActor510900Collision35DA4[i];
        faces[i]   = _gActor510900Collision35E1C[i];
    }
}

void func_actor_510900_8013B608(Task* arg0)
{
    Actor510900Work* work = arg0->work;

    worldCollisionUnlinkBody(&work->body);
    worldCollisionUnlinkBody(&work->weaponAttack);
    worldCollisionUnlinkBody(&work->forearmAttack);
    enemyDestroy(arg0->spawnArg2.pointer, arg0);
}

static void func_actor_510900_8013B658(Enemy* arg0, Task* arg1)
{
    if (gGameSession->eventState != 0) {
        func_actor_510900_801355B4(arg0, arg1);
        return;
    }
    func_actor_510900_8013B6A0(arg0, arg1);
}

static void func_actor_510900_8013B6A0(Enemy* arg0, Task* arg1)
{
    GfxCoord*        temp_s1;
    TmdObject*       temp_a1;
    Actor510900Work* temp_s2;

    temp_s2 = arg1->work;
    temp_a1 = arg1->extra.tmd;
    temp_s1 = temp_a1->coords;
    if (temp_s2->activation != 0) {
        switch (gSceneCombatState.actorControl) {
            case SCENE_COMBAT_ACTORS_RUNNING:
                temp_a1->flags               = 0;
                arg0->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
                break;
            case SCENE_COMBAT_ACTORS_PAUSED:
                no9GolemDrawShadow(arg1);
                func_actor_510900_8013BC38(arg1, temp_s1);
                return;
            case SCENE_COMBAT_ACTORS_HIDDEN:
                temp_a1->flags               = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
                return;
        }
        if (arg0->reactionFlags != 0) {
            func_actor_510900_8013B804(arg1);
        }
        func_actor_510900_80135744(arg1);
        func_actor_510900_8013864C(arg1);
        func_actor_510900_8013B870(arg1);
        func_actor_510900_801387F4(arg1);
        func_actor_510900_80138978(arg1);
        func_actor_510900_80138A9C(arg1);
        func_actor_510900_8013BB20(arg1);
        no9GolemAimHead(arg1);
        if (temp_s2->hitTwistActive != 0) {
            func_actor_510900_80138D38(arg1);
        }
        temp_s1->composeStamp                   = GRAPHICS_COORD_DIRTY;
        arg1->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(temp_s1);
        no9GolemDrawShadow(arg1);
        func_actor_510900_8013BC38(arg1, temp_s1);
        func_actor_510900_80138F44(arg1);
        func_actor_510900_8013BC80(arg1);
    }
}

static void func_actor_510900_8013B804(Task* arg0)
{
    Actor510900Work* work;
    Enemy*           enemy;
    u8               flags;

    enemy = arg0->spawnArg2.pointer;
    flags = enemy->reactionFlags;
    work  = arg0->work;
    if (flags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags = flags & ENEMY_REACTION_STAGGER_CLEAR;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->state           = ACTOR_510900_STATE_BUILDUP_STUN;
        work->subState        = 0;
        work->buildupStunned  = 1;
    }
    flags = enemy->reactionFlags;
    if (flags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        enemy->reactionFlags = flags & ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
    }
}

static void func_actor_510900_8013B870(Task* arg0)
{
    s16 temp_v1;

    temp_v1 = ((Actor510900Work*)arg0->work)->state;
    switch (temp_v1) {
        case ACTOR_510900_STATE_OPENING:
            func_actor_510900_80135E90(arg0);
            return;
        case ACTOR_510900_STATE_PATROL:
            func_actor_510900_80136184(arg0);
            return;
        case ACTOR_510900_STATE_SLASH:
            func_actor_510900_80136B70(arg0);
            return;
        case ACTOR_510900_STATE_FLAME_LUNGE:
            func_actor_510900_80137008(arg0);
            return;
        case ACTOR_510900_STATE_GRENADE:
            func_actor_510900_801373B8(arg0);
            return;
        case ACTOR_510900_STATE_DASH:
            func_actor_510900_801375D8(arg0);
            return;
        case ACTOR_510900_STATE_LETHAL_ATTACK:
            func_actor_510900_80137868(arg0);
            return;
        case ACTOR_510900_STATE_BUILDUP_STUN:
            func_actor_510900_8013B988(arg0);
            return;
        case ACTOR_510900_STATE_FLINCH:
            func_actor_510900_8013BA58(arg0);
            return;
        case ACTOR_510900_STATE_RECOIL:
            func_actor_510900_80137E20(arg0);
            return;
        case ACTOR_510900_STATE_SPARK_RECOIL:
            func_actor_510900_80137FBC(arg0);
            return;
        case ACTOR_510900_STATE_SPARK_STUN:
            func_actor_510900_80138250(arg0);
            return;
        case ACTOR_510900_STATE_DEATH:
            func_actor_510900_801384C4(arg0);
        default:
            return;
    }
}

static void func_actor_510900_8013B988(Task* arg0)
{
    Actor510900Work* work;
    s32              state;
    s32              rng;

    work  = arg0->work;
    state = work->subState;
    switch (state) {
        case 0:
            work->lapSpeed    = 0;
            work->animationId = 0x13;
            if (Gp_TickObjFlag2(arg0->spawnArg2.pointer) != 0) {
                work->animationId    = 0x14;
                work->subState       = 1;
                work->buildupStunned = 0;
            }
            break;
        case 1:
            if (work->animationFrame >= 0x3B) {
                work->state       = state;
                work->subState    = 0;
                work->animationId = state;
                work->stateCounter =
                    D_actor_510900_801679F0[((u32)(rng = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
                gRandomLcgState = rng;
            }
            break;
    }
}

static void func_actor_510900_8013BA58(Task* arg0)
{
    Actor510900Work* work;
    s32              state;
    Enemy*           enemy;
    s32              rng;

    work  = arg0->work;
    state = work->subState;
    enemy = arg0->spawnArg2.pointer;
    switch (state) {
        case 0:
            work->animationId = 0x11;
            work->lapSpeed    = 0;
            work->subState    = 1;
            break;
        case 1:
            if (work->animationFrame >= 0x50) {
                if (enemy->hp <= 0) {
                    work->state       = ACTOR_510900_STATE_DEATH;
                    work->subState    = 0;
                    work->animationId = 0x18;
                } else {
                    work->state       = state;
                    work->subState    = 0;
                    work->animationId = state;
                    work->stateCounter =
                        D_actor_510900_801679F0[((u32)(rng = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
                    gRandomLcgState = rng;
                }
            }
            break;
    }
}

static void func_actor_510900_8013BB20(Task* arg0)
{
    Actor510900Work* work;
    s32              i;
    s32              value;

    work = arg0->work;
    if (work->animationId != work->seededAnimationId) {
        work->seededAnimationId = work->animationId;
        work->animationFrame    = 0;
        value                   = D_actor_510900_80167B38[work->animationId];
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationSeekSlotWithBlend(&work->rig.anim, i, work->animationId, 0, value);
        }
    } else {
        work->animationFrame++;
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
}

#include "../../shared/no9_golem_draw_shadow.inc.c"

void func_actor_510900_8013BC38(Task* arg0, GfxCoord* arg1)
{
    VECTOR pos;

    pos.vx = arg1->workm.t[0];
    pos.vy = arg1->workm.t[1];
    pos.vz = arg1->workm.t[2];
    worldCoordUpdateActorColor(arg0->spawnArg2.pointer, &pos, 0, 0);
}

static void func_actor_510900_8013BC80(Task* arg0)
{
    Actor510900Work* work = arg0->work;

    if ((u16)(work->flameMode - ACTOR_510900_FLAME_BURNING) < 2) {
        if (--work->flameFrames <= 0) {
            work->flameMode = ACTOR_510900_FLAME_DYING;
            if (work->flameSound != 0) {
                sndEvtRequestScriptStop(work->flameSound, SOUND_SCRIPT_STOP_NO_FADE);
                work->flameSound = 0;
            }
            if (work->eventFlameSound != 0) {
                sndEvtRequestScriptStop(work->eventFlameSound, SOUND_SCRIPT_STOP_NO_FADE);
                work->eventFlameSound = 0;
            }
            work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        }
    }
    if (work->flameMode != work->sentFlameMode) {
        if (work->flameJetTask != NULL) {
            work->flameJetTask->spawnArg1.value = work->flameMode;
        }
        work->sentFlameMode = work->flameMode;
    }
}

s32 func_actor_510900_8013BD5C(Task* arg0, s32 msgId, s32 arg2, s32 arg3)
{
    if (gPlayerStatus.hp > 0) {
        ((Actor510900Work*)arg0->work)->playerEscaped = 1;
    }
    return 0;
}

s32 func_actor_510900_8013BD84(Task* arg0, s32 arg1, AnimationPlayRequest* arg2, s32 arg3)
{
    Actor510900Work* work;
    s32              blend;
    s32              i;

    blend             = (arg2->blend != ANIMATION_BLEND_RESET) * 8;
    work              = arg0->work;
    work->animationId = arg2->animationId + 0x1B;
    for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
        animationSeekSlotWithBlend(&work->rig.anim, i, work->animationId, 0, blend);
    }
    work->animationFrame = 0;
    return 0;
}

#include "../../shared/actor_messages_place_rot_matrix.inc.c"

/// Message 0x7D5 handler: switches the model's 0x80 flag: set when `arg2` is 0,
/// cleared for any other value. The message id itself is unused.
s32 func_actor_510900_8013BE64(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    TmdObject* tmd;

    tmd = task->extra.tmd;
    if (arg2 == 0) {
        tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        tmd->flags = 0;
    }
    return 0;
}

s32 func_actor_510900_8013BE84(Task* arg0, s32 msgId, s32 arg2, s32 arg3)
{
    return ((Actor510900Work*)arg0->work)->present;
}

void func_actor_510900_8013BE98(Task* task)
{
    EnemyTaskFunc fns[2] = { func_actor_510900_8013BEEC, func_actor_510900_801395AC };

    fns[task->state](task->spawnArg2.pointer, task);
}

static void func_actor_510900_8013BEEC(Enemy* enemy, Task* task)
{
    TmdObject*       obj;
    Actor510900Work* work;
    GfxCoord*        coord;

    obj                 = task->extra.tmd;
    work                = task->parent->work;
    coord               = obj->coords;
    obj->clutRowOffset += 2;
    tmdBuildBufferHalf(obj);
    tmdBuildBufferHalf(obj);
    coord->parent       = &task->parent->extra.tmd->coords[12];
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->flags          = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    obj->lightMtx       = &work->light;
    obj->colorMtx       = &work->color;
    task->state         = 1;
}

void func_actor_510900_8013BF90(Task* task)
{
    EnemyTaskFunc fns[2] = { func_actor_510900_8013BFE4, func_actor_510900_8013C034 };

    fns[task->state](task->spawnArg2.pointer, task);
}

static void func_actor_510900_8013BFE4(Enemy* enemy, Task* task)
{
    TmdObject*       obj;
    Actor510900Work* work;

    obj                 = task->extra.tmd;
    work                = task->parent->work;
    obj->flags          = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    obj->coords->parent = &task->parent->extra.tmd->coords[8];
    obj->lightMtx       = &work->light;
    obj->colorMtx       = &work->color;
    task->state         = 1;
}

static void func_actor_510900_8013C034(Enemy* enemy, Task* task)
{
    task->extra.tmd->flags                = task->parent->extra.tmd->flags;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
}

void func_actor_510900_8013C090(Task* task)
{
    EnemyTaskFunc fns[2] = { func_actor_510900_8013C0E4, func_actor_510900_8013C134 };

    fns[task->state](task->spawnArg2.pointer, task);
}

static void func_actor_510900_8013C0E4(Enemy* enemy, Task* task)
{
    TmdObject*       obj;
    Actor510900Work* work;

    obj                 = task->extra.tmd;
    work                = task->parent->work;
    obj->flags          = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    obj->coords->parent = &task->parent->extra.tmd->coords[3];
    obj->lightMtx       = &work->light;
    obj->colorMtx       = &work->color;
    task->state         = 1;
}

static void func_actor_510900_8013C134(Enemy* enemy, Task* task)
{
    task->extra.tmd->flags                = task->parent->extra.tmd->flags;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
}

/// Runs the enemy's current state handler, copying the table onto the stack
/// before the call.
void func_actor_510900_8013C190(Task* task)
{
    EnemyTaskFuncTable3 sp;

    sp = D_actor_510900_80131ECC;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}

void func_actor_510900_8013C1EC(Task* task)
{
    EnemyTaskFunc fns[2] = {
        func_actor_510900_8013A5B8,
        func_actor_510900_8013A85C,
    };

    fns[task->state](task->spawnArg2.pointer, task);
}

/// Reports whether the camera has cut away from every view this child runs in.
/// Until then it returns 1 and the caller keeps ticking the animation; on the
/// frame all three views miss it hides the model, releases the task it holds
/// and returns 0.
static s32 func_actor_510900_8013C240(Task* task)
{
    TmdObject*                    obj;
    _Actor510900HelipadLightWork* work;
    Enemy*                        ctx;
    s32                           i;
    u8                            misses;
    u8                            view;

    obj    = task->extra.tmd;
    work   = task->work;
    ctx    = task->spawnArg2.pointer;
    misses = 0;
    view   = viewGetMappedIndex();
    for (i = 0; i < 3; i++) {
        if (view != D_actor_510900_80167CD8[work->lightIndex][i]) {
            misses++;
        }
    }

    if (misses != 3) {
        return 1;
    }

    obj->flags                  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->body.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->blast.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    ctx->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    if (work->sparksTask != NULL) {
        work->sparksTask->state = 2;
        work->sparksTask        = NULL;
    }
    if (work->sparkFrames != 0) {
        work->sparkFrames--;
    }
    return 0;
}

static void func_actor_510900_8013C338(Task* arg0, GfxCoord* arg1)
{
    VECTOR pos;

    pos.vx = arg1->workm.t[0];
    pos.vy = arg1->workm.t[1];
    pos.vz = arg1->workm.t[2];
    worldCoordSetModelLighting(arg0->extra.tmd, &pos, 0, 3);
}

static void func_actor_510900_8013C380(Task* arg0)
{
    Enemy*                        enemy = arg0->spawnArg2.pointer;
    _Actor510900HelipadLightWork* work  = arg0->work;

    worldTargetUnlinkNode(&enemy->node);
    worldCollisionUnlinkBody(&work->body);
    worldCollisionUnlinkBody(&work->blast);
    enemyDestroy(enemy, arg0);
}

void func_actor_510900_8013C3DC(Task* task)
{
    EnemyTaskFunc fns[2] = {
        func_actor_510900_8013AD90,
        func_actor_510900_8013AF38,
    };

    fns[task->state](task->spawnArg2.pointer, task);
}

static void func_actor_510900_8013C430(Task* arg0)
{
    Enemy*                       enemy = arg0->spawnArg2.pointer;
    _Actor510900BlastSourceWork* work  = arg0->work;

    worldTargetUnlinkNode(&enemy->node);
    worldCollisionUnlinkBody(&work->body);
    worldCollisionUnlinkBody(&work->blast);
    enemyDestroy(enemy, arg0);
}
