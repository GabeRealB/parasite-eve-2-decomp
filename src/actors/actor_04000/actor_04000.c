#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
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

#include "main/areas.h"
#include "main/coord.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#include "../../shared/actor_messages.h"
#include "../../shared/anim_driver.h"
#include "../../shared/actor_contacts.h"

/// Values of `_Actor04000Work::state`: the index of the handler the per-frame
/// tick runs.
///
/// Animation numbers are indices into the package's animation-set table. The
/// three burst states end the same way: the model swells and its colour fades
/// while `burstAttackBody` and `burstWaveBody` are switched on for a few
/// ticks, and the actor goes to `HIDDEN`. The two that are not a kill redden
/// the model first.
enum {
    ACTOR_04000_STATE_HIDDEN          = 0x00, // not drawn, not lockable, all four bodies off
    ACTOR_04000_STATE_SETTLE          = 0x01, // plays animation 4 to its boundary, then idles
    ACTOR_04000_STATE_IDLE            = 0x02, // loops animation 5; rouses at random after 25 loops, or when the player comes within 2000
    ACTOR_04000_STATE_ROUSE           = 0x03, // plays animation 6 to its boundary, then patrols
    ACTOR_04000_STATE_CHASE           = 0x04, // runs at the player on animation 3; lunges when close and facing, returns after 240 ticks out of reach
    ACTOR_04000_STATE_SELF_BURST      = 0x05, // bursts of its own accord from tick 0x28, releasing a held player, and ends with no hit points
    ACTOR_04000_STATE_DEATH_BURST     = 0x06, // entered when the hit points run out: plays animation 10 and bursts from tick 13
    ACTOR_04000_STATE_PATROL          = 0x07, // walks between the two `patrolPoints` on animation 2 until it notices the player
    ACTOR_04000_STATE_RETURN          = 0x08, // walks back to `spawnPos` on animation 2, then settles
    ACTOR_04000_STATE_AWAIT_BATTLE    = 0x09, // holds animation 1, shown only at placements 6 and 7, until the battle is engaged; then patrols
    ACTOR_04000_STATE_LUNGE           = 0x0A, // plays animation 13 and jumps forward; on tick 9 latches onto a player in reach, else recovers
    ACTOR_04000_STATE_LATCHED         = 0x0B, // holds the player, damaging every 8 ticks; after 41 ticks `SELF_BURST` if the player accepts a further hold, else `RELEASE_BURST`
    ACTOR_04000_STATE_LUNGE_RECOVER   = 0x0C, // plays animation 15 after a missed lunge, then chases; the fourth miss bursts instead
    ACTOR_04000_STATE_RELEASE_BURST   = 0x0D, // follows `LATCHED`: starts the player's second grab animation, drifts sideways on animation 14 and bursts from tick 0x5B
    ACTOR_04000_STATE_HANG            = 0x0E, // hangs upside down on animation 1, not lockable, until the controller task picks it to drop
    ACTOR_04000_STATE_DROP            = 0x0F, // waits 50 ticks, then falls to the floor turning upright, and rouses
    ACTOR_04000_STATE_SCRIPTED_THRASH = 0x10, // jerks back and forth on animation 3 at a fixed spot of stage 3, area 18; not lockable in view 5
    ACTOR_04000_STATE_SCRIPTED_FALL   = 0x11, // falls to the floor, plays animations 12, 16 and 17 with scripted steps, then chases
    ACTOR_04000_STATE_SCRIPTED_LEAP   = 0x12, // plays animation 11 while stepping forward and down for 49 ticks, then `SCRIPTED_FALL`
    ACTOR_04000_STATE_COUNT                   // number of states, and of the handlers in `_Actor04000StateTable`
};

/// Stage/area command namespaces; stage occupies the low byte, unlike a location key.
enum {
    ACTOR_04000_COMMAND_CONTEXT_HANGING_POOL  = (GAME_AREA_DRYFIELD_NIGHT_TOILET << 8) | GAME_STAGE_DRYFIELD_NIGHT,
    ACTOR_04000_COMMAND_CONTEXT_SALOON        = (GAME_AREA_DRYFIELD_NIGHT_SALOON_G_R << 8) | GAME_STAGE_DRYFIELD_NIGHT,
    ACTOR_04000_COMMAND_CONTEXT_GENERAL_STORE = (GAME_AREA_DRYFIELD_GENERAL_STORE << 8) | GAME_STAGE_DRYFIELD,
};

/// Commands interpreted only within their respective room namespaces.
enum {
    ACTOR_04000_POOL_COMMAND_HANG          = 1,
    ACTOR_04000_SALOON_COMMAND_HIDE        = 0,
    ACTOR_04000_SALOON_COMMAND_START       = 1,
    ACTOR_04000_STORE_COMMAND_AWAIT_BATTLE = 0,
    ACTOR_04000_STORE_COMMAND_ENTER        = 1,
    ACTOR_04000_STORE_COMMAND_HIDE         = 9,
};

/// The hanging encounter uses six candidates from the eight-entry placement table.
enum { ACTOR_04000_HANGING_POOL_COUNT = 6 };

/// Set-table indices selected by this actor's behavior states.
enum {
    ACTOR_04000_ANIMATION_HOLD           = 1,
    ACTOR_04000_ANIMATION_SETTLE         = 4,
    ACTOR_04000_ANIMATION_ROUSE          = 6,
    ACTOR_04000_ANIMATION_DEATH_BURST    = 10,
    ACTOR_04000_ANIMATION_RELEASE_BURST  = 14,
    ACTOR_04000_ANIMATION_LUNGE_RECOVER  = 15,
    ACTOR_04000_ANIMATION_WALK           = 2,
    ACTOR_04000_ANIMATION_RUN            = 3,
    ACTOR_04000_ANIMATION_IDLE           = 5,
    ACTOR_04000_ANIMATION_SCRIPTED_LEAP  = 11,
    ACTOR_04000_ANIMATION_SCRIPTED_FALL  = 12,
    ACTOR_04000_ANIMATION_LUNGE          = 13,
    ACTOR_04000_ANIMATION_SCRIPTED_PAUSE = 16,
    ACTOR_04000_ANIMATION_SCRIPTED_SLIDE = 17,
};

/// Contact identity and geometric limits shared by the three burst states.
///
/// Radii and the wave's upward offset use game-coordinate units. The wave is
/// an attack-category contact in the blast list, separate from enemy attack 1.
/// Sound keys leave bits 8..15 free for the placement's script instance.
enum {
    ACTOR_04000_BURST_WAVE_CONTACT_KEY      = WORLD_COLLISION_CONTACT_ATTACK | 0x2222,
    ACTOR_04000_BURST_ATTACK_INDEX          = 1,
    ACTOR_04000_BURST_ATTACK_RADIUS         = 1000,
    ACTOR_04000_BURST_WAVE_START_RADIUS     = 250,
    ACTOR_04000_BURST_WAVE_MIDDLE_RADIUS    = 500,
    ACTOR_04000_BURST_WAVE_END_RADIUS       = 1000,
    ACTOR_04000_BURST_WAVE_HEIGHT           = 500,
    ACTOR_04000_SOUND_BURST                 = 0x40280004,
    ACTOR_04000_BURST_TIMER_LIMIT           = 1024,
    ACTOR_04000_BURST_COLOR_START_Q12       = 3000,
    ACTOR_04000_BURST_COLOR_ZERO_CUTOFF_Q12 = 1200,
};

/// Initial scripted playback rate in sixteenths of a frame per tick.
enum { ACTOR_04000_ANIMATION_PRIME_RATE = 1 };

/// Work block of the package's enemy task.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`. It
/// holds the state machine, the animation rig and its driver's state, four
/// collision bodies with their contact records, the points the actor walks
/// between, and storage for the model's matrices.
///
/// Everything up to and including `driver` is laid out as `AnimDriverWork`,
/// through which the shared animation driver reaches the block: its
/// `carrierState` bytes are the six words ahead of `rig` here. Angles are
/// 4096ths of a turn. A cue index is the low ten bits of the record a slot's
/// current pose names (`ANIMATION_POSE_CUE_INDEX_MASK`).
typedef struct {
    s16           state;                             // `ACTOR_04000_STATE_*`
    s16           prevState;                         // `state` the tick last ran; -1 from spawn, so the first tick enters `state` afresh
    s16           stateEntered;                      // 1 on the first tick of a state, when the handler sets itself up (0 otherwise)
    s16           stateFrame;                        // tick counter of the running state, cleared on entry by the states that time themselves; `PATROL` counts its blocked ticks in it
    s16           field_8;                           // cleared on entering `SCRIPTED_FALL` and `SCRIPTED_LEAP` and never read; role unproven
    s16           field_A;                           // cleared at spawn and never accessed again; role unproven
    ActorAnimRig6 rig;                               // playback of the model's parts; slot 1's status and cue index time the states
    struct {
        s16 state;                                   // `ANIM_DRIVER_STATE_*` (0 idle, 1 or 2 restart requested, 3 playing)
        s16 playingSet;                              // animation the slots were last restarted on
        s16 requestedSet;                            // animation the next restart plays; also what the sound cues are keyed on
        s16 rate;                                    // playback rate in sixteenths of a frame per tick; from spawn 16 plus the placement index (odd placements) or minus half of it (even), until a state sets its own
        s16 rateBias;                                // added to `rate`; `CHASE` runs with 16, every other state with 0
        s16 tickCount;                               // advancing ticks since the last restart
        s16 jumpCount;                               // of those, ticks on which slot 1 followed a control jump: loops of a looping animation
    } driver;                                        // the animation driver's state, member for member `AnimDriverWork`'s
    s16                      field_17E;              // cleared at spawn and never accessed again; role unproven
    s32                      field_180;              // set to 0 at spawn and never accessed again; role unproven
    s32                      field_184;              // set to 1 at spawn and never accessed again; role unproven
    s32                      field_188;              // set to 0 at spawn and never accessed again; role unproven
    s32                      field_18C;              // set to 3 at spawn and never accessed again; role unproven
    s32                      field_190;              // set to 1 at spawn and never accessed again; role unproven
    u16                      lastCommandContext;     // stage and area of an actor command as one word; never stored here, so the burst states' test for the pool command (stage 3, area 16) never passes
    byte                     pad_196[2];             // never accessed
    s16                      fallSpeed;              // downward step per tick of a fall, 0 when it begins
    s16                      fallAccel;              // added to `fallSpeed` each tick of a fall; starts at 10 and itself grows by 4 a tick in `DROP`, by 2 in `SCRIPTED_FALL`
    s16                      fallRoll;               // roll about Z `DROP` still has to undo: half a turn when it begins, unwound by at most 0x92 a tick
    byte                     pad_19E[2];             // never accessed
    s16                      field_1A0;              // 5 at spawn, offset by the placement index the way `driver.rate` is; never accessed again; role unproven
    s16                      field_1A2;              // 20 at spawn, offset the same way; never accessed again; role unproven
    byte                     pad_1A4[0xC];           // never accessed
    WorldCollisionContact    gridContacts[8];        // contacts of `gridBody`, which push the root out of what it walks into
    WorldCollisionBody       gridBody;               // sphere of radius 400 on part 1 that the room grid tests; off in `HIDDEN`, `DEATH_BURST`, `AWAIT_BATTLE`, `SCRIPTED_FALL` and `SCRIPTED_LEAP`
    WorldCollisionContact    hitContacts[8];         // contacts of `hitBody`; also the enemy's hit records, which damage and steering read
    WorldCollisionBody       hitBody;                // sphere of radius 360 on part 2 that takes the hits; held at a fixed world point during `SCRIPTED_THRASH`
    WorldCollisionContact    burstAttackContacts[1]; // contact of `burstAttackBody`
    WorldCollisionBody       burstAttackBody;        // world-space sphere at the root carrying the key of attack 1; on, with radius 1000, only for the first ticks of a burst
    WorldCollisionContact    burstWaveContacts[1];   // contact of `burstWaveBody`; never cleared by the tick
    WorldCollisionBody       burstWaveBody;          // world-space sphere 500 above the root with key 0x22222 that grows from 250 to 1000 through a burst
    EffectSpawnArg           hitEffectArg;           // argument record of the effect a hit spawns; names the part the effect hangs off
    SVECTOR                  hitEffectOffset;        // offset of that effect from its part, picked at random by the side the hit came from; `pad` holds the part index
    SVECTOR                  spawnPos;               // root position at spawn; `RETURN` walks back to it
    SVECTOR                  slideDir;               // column 0 of the root's rotation on entering `SCRIPTED_FALL`, normalised; the actor slides along it during animation 17
    SVECTOR                  patrolPoints[2];        // `spawnPos` plus (0) and minus (1) 1000 along the facing at spawn; only X and Z are used
    s16                      patrolIndex;            // `patrolPoints` entry `PATROL` walks toward; swapped on arrival or after 97 blocked ticks
    byte                     pad_412[2];             // never accessed
    MATRIX                   lightMtx;               // storage for the model's `TmdObject::lightMtx`
    MATRIX                   colorMtx;               // storage for the model's `TmdObject::colorMtx`
    MATRIX                   savedColorMtx;          // `colorMtx` as a burst state found it; each tick reddens and scales a fresh copy
    u16                      lastSoundCueIndex;      // slot 1's cue index when the walk or run cue sound last fired, so a held cue sounds once; 0 on any other cue
    byte                     pad_476[3];             // never accessed
    s8                       airborne;               // 1 from the start of a drop or a scripted state until the actor is back on the floor; a burst in the air leaves no ground glow
    s8                       missedLunges;           // times `LUNGE_RECOVER` has been entered; at 4 the actor bursts
    byte                     pad_47B[1];             // never accessed
    GameActorButtonPressHold playerButtonHold;       // hold sent to the player by a lunge (12 presses) and by `LATCHED` (9999); only `pressCount` is filled in
    s16                      chaseFarFrames;         // consecutive `CHASE` ticks with the player farther than 1000; past 240 the actor returns
    s16                      holdingPlayer;          // 1 from the latch until the actor has ended the player's scripted hold
} _Actor04000Work;
STATIC_ASSERT_SIZEOF(_Actor04000Work, 0x498);
STATIC_ASSERT(OFFSET_OF(_Actor04000Work, rig) == OFFSET_OF(AnimDriverWork, rig), Actor04000Work_rig);
STATIC_ASSERT(OFFSET_OF(_Actor04000Work, driver) == OFFSET_OF(AnimDriverWork, state), Actor04000Work_driver);

/// The scratch-stack block of the lunge at the player.
///
/// `ACTOR_04000_STATE_LUNGE` reserves one on the tick that decides whether
/// the lunge caught the player and releases it before returning; nothing
/// carries over to another tick. Every such tick fills `delta` and `turn`
/// to test the player's range and bearing. A catch reuses both to pick a
/// side of the player by the sign of `turn`, and then to set the actor down
/// 540 units to that side, facing the player. Yaws are 4096 units per turn,
/// and a wrapped one lies in [-0x800, 0x800].
typedef struct {
    SVECTOR delta;          // Player's position minus the actor's, low 16 bits of each axis. On a catch it is replaced by the player's local X axis, flattened and scaled to 60 units one way along it, then to 600 units the other way: the two steps that place the actor from the player's position. Last it is reversed, pointing from the actor back at the player; `pad` is never written
    byte    unknown_8[0x8]; // Reserved with the block and never accessed; role unproven
    s16     facingYaw;      // Bearing of the reversed `delta`: the yaw a catch rebuilds the actor's rotation around
    s16     turn;           // Wrapped turn from the actor's heading to the player; on a catch, the wrapped turn from the player's heading to that same bearing, whose sign picks which of the package's two player animation sets the held player plays and which side the actor is set down on
} _Actor04000LungeScratch;
STATIC_ASSERT_SIZEOF(_Actor04000LungeScratch, 0x14);

/// The actor's state handlers, indexed by `_Actor04000Work::state`.
///
/// The package defines one table. The per-frame tick copies it to the stack
/// before calling the entry of the current state with the enemy and its task.
/// The call is unconditional and every entry is a handler.
typedef struct {
    EnemyTaskFunc handlers[ACTOR_04000_STATE_COUNT]; // Handler of each `ACTOR_04000_STATE_*`
} _Actor04000StateTable;
STATIC_ASSERT_SIZEOF(_Actor04000StateTable, ACTOR_04000_STATE_COUNT * sizeof(EnemyTaskFunc));

/// Whole-unit part of the last step `_actorContactApplyGridPushback` applied.
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

extern Task* Actor04000_D0C710[2];

extern Task* Actor04000_D0C718[8];

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

static void _actor04000StateHidden(Enemy* enemy, Task* task);
static void _actor04000StateLatched(Enemy* enemy, Task* task);
static void _actor04000StateLungeRecover(Enemy* unusedEnemy, Task* task);
static void _actor04000StateAwaitBattle(Enemy* enemy, Task* task);
static void _actor04000StateSettle(Enemy* enemy, Task* task);
static void _actor04000StateRouse(Enemy* enemy, Task* task);
static void _actor04000StateHang(Enemy* enemy, Task* task);

static TmdSource _gActor04000BloodSucklerBody;
static void      _actor04000RefillDropSlots(Task* task);
static void      _actor04000Task(Task* task);
static void      _actor04000InitHangingPool(Task* task);
static void      _actor04000AwaitPoolBattle(Task* task);
static void      _actor04000HangingPoolTask(Task* task);

static s32 _actor04000ApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg);
static s32 _actor04000SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg);
static s32 _actor04000ReleaseHold(Task* task, s32 messageId, s32 unusedPayload, s32 unusedArg);
static s32 _actor04000RequestAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);

DamageAttack Actor04000_D07078[3] = {
    { 1, 7 },
    { 18, 7 },
    { 30, 0 },
};

EnemyParams Actor04000_D07084 = { Actor04000_D07078, 1, 8, 28, 4, 100, 100, 100, 0 };

PadScriptCmd Actor04000_D07094[3] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 2) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment Actor04000_D070A0[3] = {
    { 0, 0, 9, 0 },
    { 255, 255, 12, 1 },
    { 100, 50, 6, 1 },
};

static TmdBone _gActor04000BloodSucklerBodySkeleton[6] = {
#include "assets/blood_suckler_body_skeleton.inc"
};

static u32 _gActor04000BloodSucklerBodyPartVerts[6] = {
#include "assets/blood_suckler_body_partVerts.inc"
};

static SVECTOR _gActor04000BloodSucklerBodyVerts[94] = {
#include "assets/blood_suckler_body_verts.inc"
};

static SVECTOR _gActor04000BloodSucklerBodyNormals[94] = {
#include "assets/blood_suckler_body_normals.inc"
};

static u32 _gActor04000BloodSucklerBodyStream[999] = {
#include "assets/blood_suckler_body_stream.inc"
};

static TmdSource _gActor04000BloodSucklerBody = {
    0,
    5624,
    1228,
    6,
    _gActor04000BloodSucklerBodyPartVerts,
    _gActor04000BloodSucklerBodyVerts,
    _gActor04000BloodSucklerBodyNormals,
    _gActor04000BloodSucklerBodySkeleton,
    _gActor04000BloodSucklerBodyStream,
};

static AnimationPackedPose _gActor04000Actor104000Animation08878Bank1[4] = {
#include "assets/actor_104000_animation_08878_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation08878Bank4[21] = {
#include "assets/actor_104000_animation_08878_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation08878Records[43] = {
#include "assets/actor_104000_animation_08878_records.inc"
};

static u16 _gActor04000Actor104000Animation08878Indices[6] = {
#include "assets/actor_104000_animation_08878_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation08878 = {
    _gActor04000Actor104000Animation08878Records,
    _gActor04000Actor104000Animation08878Indices,
    { NULL, _gActor04000Actor104000Animation08878Bank1, NULL, NULL, _gActor04000Actor104000Animation08878Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation08B3CBank1[19] = {
#include "assets/actor_104000_animation_08B3C_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation08B3CBank4[36] = {
#include "assets/actor_104000_animation_08B3C_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation08B3CRecords[71] = {
#include "assets/actor_104000_animation_08B3C_records.inc"
};

static u16 _gActor04000Actor104000Animation08B3CIndices[6] = {
#include "assets/actor_104000_animation_08B3C_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation08B3C = {
    _gActor04000Actor104000Animation08B3CRecords,
    _gActor04000Actor104000Animation08B3CIndices,
    { NULL, _gActor04000Actor104000Animation08B3CBank1, NULL, NULL, _gActor04000Actor104000Animation08B3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation08DE0Bank1[20] = {
#include "assets/actor_104000_animation_08DE0_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation08DE0Bank4[31] = {
#include "assets/actor_104000_animation_08DE0_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation08DE0Records[65] = {
#include "assets/actor_104000_animation_08DE0_records.inc"
};

static u16 _gActor04000Actor104000Animation08DE0Indices[6] = {
#include "assets/actor_104000_animation_08DE0_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation08DE0 = {
    _gActor04000Actor104000Animation08DE0Records,
    _gActor04000Actor104000Animation08DE0Indices,
    { NULL, _gActor04000Actor104000Animation08DE0Bank1, NULL, NULL, _gActor04000Actor104000Animation08DE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation08FC0Bank1[6] = {
#include "assets/actor_104000_animation_08FC0_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation08FC0Bank4[38] = {
#include "assets/actor_104000_animation_08FC0_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation08FC0Records[51] = {
#include "assets/actor_104000_animation_08FC0_records.inc"
};

static u16 _gActor04000Actor104000Animation08FC0Indices[6] = {
#include "assets/actor_104000_animation_08FC0_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation08FC0 = {
    _gActor04000Actor104000Animation08FC0Records,
    _gActor04000Actor104000Animation08FC0Indices,
    { NULL, _gActor04000Actor104000Animation08FC0Bank1, NULL, NULL, _gActor04000Actor104000Animation08FC0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation090F8Bank1[4] = {
#include "assets/actor_104000_animation_090F8_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation090F8Bank4[13] = {
#include "assets/actor_104000_animation_090F8_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation090F8Records[40] = {
#include "assets/actor_104000_animation_090F8_records.inc"
};

static u16 _gActor04000Actor104000Animation090F8Indices[6] = {
#include "assets/actor_104000_animation_090F8_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation090F8 = {
    _gActor04000Actor104000Animation090F8Records,
    _gActor04000Actor104000Animation090F8Indices,
    { NULL, _gActor04000Actor104000Animation090F8Bank1, NULL, NULL, _gActor04000Actor104000Animation090F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation09298Bank1[7] = {
#include "assets/actor_104000_animation_09298_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation09298Bank4[28] = {
#include "assets/actor_104000_animation_09298_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation09298Records[42] = {
#include "assets/actor_104000_animation_09298_records.inc"
};

static u16 _gActor04000Actor104000Animation09298Indices[6] = {
#include "assets/actor_104000_animation_09298_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation09298 = {
    _gActor04000Actor104000Animation09298Records,
    _gActor04000Actor104000Animation09298Indices,
    { NULL, _gActor04000Actor104000Animation09298Bank1, NULL, NULL, _gActor04000Actor104000Animation09298Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation094ACBank1[6] = {
#include "assets/actor_104000_animation_094AC_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation094ACBank4[42] = {
#include "assets/actor_104000_animation_094AC_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation094ACRecords[60] = {
#include "assets/actor_104000_animation_094AC_records.inc"
};

static u16 _gActor04000Actor104000Animation094ACIndices[6] = {
#include "assets/actor_104000_animation_094AC_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation094AC = {
    _gActor04000Actor104000Animation094ACRecords,
    _gActor04000Actor104000Animation094ACIndices,
    { NULL, _gActor04000Actor104000Animation094ACBank1, NULL, NULL, _gActor04000Actor104000Animation094ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation098BCBank1[17] = {
#include "assets/actor_104000_animation_098BC_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation098BCBank4[85] = {
#include "assets/actor_104000_animation_098BC_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation098BCRecords[111] = {
#include "assets/actor_104000_animation_098BC_records.inc"
};

static u16 _gActor04000Actor104000Animation098BCIndices[6] = {
#include "assets/actor_104000_animation_098BC_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation098BC = {
    _gActor04000Actor104000Animation098BCRecords,
    _gActor04000Actor104000Animation098BCIndices,
    { NULL, _gActor04000Actor104000Animation098BCBank1, NULL, NULL, _gActor04000Actor104000Animation098BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation09A94Bank1[5] = {
#include "assets/actor_104000_animation_09A94_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation09A94Bank4[19] = {
#include "assets/actor_104000_animation_09A94_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation09A94Records[71] = {
#include "assets/actor_104000_animation_09A94_records.inc"
};

static u16 _gActor04000Actor104000Animation09A94Indices[6] = {
#include "assets/actor_104000_animation_09A94_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation09A94 = {
    _gActor04000Actor104000Animation09A94Records,
    _gActor04000Actor104000Animation09A94Indices,
    { NULL, _gActor04000Actor104000Animation09A94Bank1, NULL, NULL, _gActor04000Actor104000Animation09A94Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation0A260Bank1[14] = {
#include "assets/actor_104000_animation_0A260_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation0A260Bank4[158] = {
#include "assets/actor_104000_animation_0A260_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation0A260Records[279] = {
#include "assets/actor_104000_animation_0A260_records.inc"
};

static u16 _gActor04000Actor104000Animation0A260Indices[20] = {
#include "assets/actor_104000_animation_0A260_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation0A260 = {
    _gActor04000Actor104000Animation0A260Records,
    _gActor04000Actor104000Animation0A260Indices,
    { NULL, _gActor04000Actor104000Animation0A260Bank1, NULL, NULL, _gActor04000Actor104000Animation0A260Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation0A71CBank1[10] = {
#include "assets/actor_104000_animation_0A71C_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation0A71CBank4[105] = {
#include "assets/actor_104000_animation_0A71C_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation0A71CRecords[148] = {
#include "assets/actor_104000_animation_0A71C_records.inc"
};

static u16 _gActor04000Actor104000Animation0A71CIndices[20] = {
#include "assets/actor_104000_animation_0A71C_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation0A71C = {
    _gActor04000Actor104000Animation0A71CRecords,
    _gActor04000Actor104000Animation0A71CIndices,
    { NULL, _gActor04000Actor104000Animation0A71CBank1, NULL, NULL, _gActor04000Actor104000Animation0A71CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation0AECCBank1[14] = {
#include "assets/actor_104000_animation_0AECC_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation0AECCBank4[147] = {
#include "assets/actor_104000_animation_0AECC_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation0AECCRecords[283] = {
#include "assets/actor_104000_animation_0AECC_records.inc"
};

static u16 _gActor04000Actor104000Animation0AECCIndices[20] = {
#include "assets/actor_104000_animation_0AECC_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation0AECC = {
    _gActor04000Actor104000Animation0AECCRecords,
    _gActor04000Actor104000Animation0AECCIndices,
    { NULL, _gActor04000Actor104000Animation0AECCBank1, NULL, NULL, _gActor04000Actor104000Animation0AECCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation0B374Bank1[10] = {
#include "assets/actor_104000_animation_0B374_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation0B374Bank4[103] = {
#include "assets/actor_104000_animation_0B374_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation0B374Records[145] = {
#include "assets/actor_104000_animation_0B374_records.inc"
};

static u16 _gActor04000Actor104000Animation0B374Indices[20] = {
#include "assets/actor_104000_animation_0B374_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation0B374 = {
    _gActor04000Actor104000Animation0B374Records,
    _gActor04000Actor104000Animation0B374Indices,
    { NULL, _gActor04000Actor104000Animation0B374Bank1, NULL, NULL, _gActor04000Actor104000Animation0B374Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation0B6F0Bank1[15] = {
#include "assets/actor_104000_animation_0B6F0_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation0B6F0Bank4[70] = {
#include "assets/actor_104000_animation_0B6F0_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation0B6F0Records[95] = {
#include "assets/actor_104000_animation_0B6F0_records.inc"
};

static u16 _gActor04000Actor104000Animation0B6F0Indices[6] = {
#include "assets/actor_104000_animation_0B6F0_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation0B6F0 = {
    _gActor04000Actor104000Animation0B6F0Records,
    _gActor04000Actor104000Animation0B6F0Indices,
    { NULL, _gActor04000Actor104000Animation0B6F0Bank1, NULL, NULL, _gActor04000Actor104000Animation0B6F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation0BAA0Bank1[14] = {
#include "assets/actor_104000_animation_0BAA0_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation0BAA0Bank4[64] = {
#include "assets/actor_104000_animation_0BAA0_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation0BAA0Records[117] = {
#include "assets/actor_104000_animation_0BAA0_records.inc"
};

static u16 _gActor04000Actor104000Animation0BAA0Indices[6] = {
#include "assets/actor_104000_animation_0BAA0_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation0BAA0 = {
    _gActor04000Actor104000Animation0BAA0Records,
    _gActor04000Actor104000Animation0BAA0Indices,
    { NULL, _gActor04000Actor104000Animation0BAA0Bank1, NULL, NULL, _gActor04000Actor104000Animation0BAA0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation0BCDCBank1[14] = {
#include "assets/actor_104000_animation_0BCDC_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation0BCDCBank4[28] = {
#include "assets/actor_104000_animation_0BCDC_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation0BCDCRecords[60] = {
#include "assets/actor_104000_animation_0BCDC_records.inc"
};

static u16 _gActor04000Actor104000Animation0BCDCIndices[6] = {
#include "assets/actor_104000_animation_0BCDC_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation0BCDC = {
    _gActor04000Actor104000Animation0BCDCRecords,
    _gActor04000Actor104000Animation0BCDCIndices,
    { NULL, _gActor04000Actor104000Animation0BCDCBank1, NULL, NULL, _gActor04000Actor104000Animation0BCDCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation0BF20Bank1[10] = {
#include "assets/actor_104000_animation_0BF20_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation0BF20Bank4[40] = {
#include "assets/actor_104000_animation_0BF20_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation0BF20Records[62] = {
#include "assets/actor_104000_animation_0BF20_records.inc"
};

static u16 _gActor04000Actor104000Animation0BF20Indices[6] = {
#include "assets/actor_104000_animation_0BF20_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation0BF20 = {
    _gActor04000Actor104000Animation0BF20Records,
    _gActor04000Actor104000Animation0BF20Indices,
    { NULL, _gActor04000Actor104000Animation0BF20Bank1, NULL, NULL, _gActor04000Actor104000Animation0BF20Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation0C070Bank1[6] = {
#include "assets/actor_104000_animation_0C070_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation0C070Bank4[20] = {
#include "assets/actor_104000_animation_0C070_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation0C070Records[33] = {
#include "assets/actor_104000_animation_0C070_records.inc"
};

static u16 _gActor04000Actor104000Animation0C070Indices[6] = {
#include "assets/actor_104000_animation_0C070_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation0C070 = {
    _gActor04000Actor104000Animation0C070Records,
    _gActor04000Actor104000Animation0C070Indices,
    { NULL, _gActor04000Actor104000Animation0C070Bank1, NULL, NULL, _gActor04000Actor104000Animation0C070Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation0C1C0Bank1[6] = {
#include "assets/actor_104000_animation_0C1C0_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation0C1C0Bank4[20] = {
#include "assets/actor_104000_animation_0C1C0_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation0C1C0Records[33] = {
#include "assets/actor_104000_animation_0C1C0_records.inc"
};

static u16 _gActor04000Actor104000Animation0C1C0Indices[6] = {
#include "assets/actor_104000_animation_0C1C0_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation0C1C0 = {
    _gActor04000Actor104000Animation0C1C0Records,
    _gActor04000Actor104000Animation0C1C0Indices,
    { NULL, _gActor04000Actor104000Animation0C1C0Bank1, NULL, NULL, _gActor04000Actor104000Animation0C1C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation0C2ECBank1[5] = {
#include "assets/actor_104000_animation_0C2EC_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation0C2ECBank4[14] = {
#include "assets/actor_104000_animation_0C2EC_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation0C2ECRecords[33] = {
#include "assets/actor_104000_animation_0C2EC_records.inc"
};

static u16 _gActor04000Actor104000Animation0C2ECIndices[6] = {
#include "assets/actor_104000_animation_0C2EC_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation0C2EC = {
    _gActor04000Actor104000Animation0C2ECRecords,
    _gActor04000Actor104000Animation0C2ECIndices,
    { NULL, _gActor04000Actor104000Animation0C2ECBank1, NULL, NULL, _gActor04000Actor104000Animation0C2ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor04000Actor104000Animation0C49CBank1[8] = {
#include "assets/actor_104000_animation_0C49C_bank1.inc"
};

static AnimationPackedRotation _gActor04000Actor104000Animation0C49CBank4[28] = {
#include "assets/actor_104000_animation_0C49C_bank4.inc"
};

static AnimationRecord _gActor04000Actor104000Animation0C49CRecords[43] = {
#include "assets/actor_104000_animation_0C49C_records.inc"
};

static u16 _gActor04000Actor104000Animation0C49CIndices[6] = {
#include "assets/actor_104000_animation_0C49C_indices.inc"
};

static AnimationSet _gActor04000Actor104000Animation0C49C = {
    _gActor04000Actor104000Animation0C49CRecords,
    _gActor04000Actor104000Animation0C49CIndices,
    { NULL, _gActor04000Actor104000Animation0C49CBank1, NULL, NULL, _gActor04000Actor104000Animation0C49CBank4, NULL, NULL, NULL },
};

AnimationSet* Actor04000_D0C4C4[19] = {
    NULL,
    &_gActor04000Actor104000Animation08878,
    &_gActor04000Actor104000Animation08B3C,
    &_gActor04000Actor104000Animation08DE0,
    &_gActor04000Actor104000Animation08FC0,
    &_gActor04000Actor104000Animation090F8,
    &_gActor04000Actor104000Animation09298,
    &_gActor04000Actor104000Animation094AC,
    &_gActor04000Actor104000Animation098BC,
    &_gActor04000Actor104000Animation0B6F0,
    &_gActor04000Actor104000Animation09A94,
    &_gActor04000Actor104000Animation0BF20,
    &_gActor04000Actor104000Animation0C070,
    &_gActor04000Actor104000Animation0BAA0,
    &_gActor04000Actor104000Animation0BCDC,
    &_gActor04000Actor104000Animation0C1C0,
    &_gActor04000Actor104000Animation0C2EC,
    &_gActor04000Actor104000Animation0C49C,
    NULL,
};

AnimationSet* Actor04000_D0C510[4] = {
    NULL,
    &_gActor04000Actor104000Animation0A260,
    &_gActor04000Actor104000Animation0A71C,
    NULL,
};

AnimationSet* Actor04000_D0C520[4] = {
    NULL,
    &_gActor04000Actor104000Animation0AECC,
    &_gActor04000Actor104000Animation0B374,
    NULL,
};

AnimationPlayRequest Actor04000_D0C530 = { { .sets = Actor04000_D0C520 }, 1, ANIMATION_BLEND_RESET, 3, ANIMATION_WORLD_COLLISION_DISABLE };

u8 Actor04000_D0C544[364] = {
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
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    6,
    0,
    0,
    0,
    0,
    0,
    9,
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
    6,
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
    6,
    6,
    6,
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
    0,
};

TaskMessageEntry Actor04000_D0C6B0[6] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor04000SetModelDraw },
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor04000RequestAnimation },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor04000ApplyCommand },
    { ACTOR_MESSAGE_PLACE, actorMsgPlace },
    { ACTOR_MESSAGE_RELEASE_HOLD, _actor04000ReleaseHold },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc Actor04000_D0C6E0 = { { { TASK_BODY_TMD, 96 } }, _actor04000Task, { .model = &_gActor04000BloodSucklerBody } };

TaskFunc Actor04000_D0C6EC[4] = {
    _actor04000InitHangingPool,
    _actor04000AwaitPoolBattle,
    _actor04000RefillDropSlots,
    taskKill,
};

TaskDesc Actor04000_D0C6FC = { { { TASK_BODY_NONE, 96 } }, _actor04000HangingPoolTask, { .value = 0 } };

SVECTOR ActorContact_ScratchPosition;

Task* Actor04000_D0C710[2];

Task* Actor04000_D0C718[8];

extern EnemyParams Actor04000_D07084;

extern AnimationSet* Actor04000_D0C4C4[19];

extern TaskMessageEntry Actor04000_D0C6B0[6];

extern AnimationPlayRequest Actor04000_D0C530;

extern PadScriptCmd Actor04000_D07094[3];

extern PadScriptVibrationSegment Actor04000_D070A0[3];

extern AnimationSet* Actor04000_D0C510[4];

extern AnimationSet* Actor04000_D0C520[4];

/// The controller task's state handlers, indexed by its `state`.
extern TaskFunc Actor04000_D0C6EC[];

static s32  _actor04000PollAnimationSound(_Actor04000Work* work);
static void _actor04000SpawnEnemy(Enemy* enemy, Task* task);
static void _actor04000StateLunge(Enemy* enemy, Task* task);
static void _actor04000StateReleaseBurst(Enemy* enemy, Task* task);
static void _actor04000StateIdle(Enemy* enemy, Task* task);
static void _actor04000StateChase(Enemy* enemy, Task* task);
static void _actor04000StateSelfBurst(Enemy* enemy, Task* task);
static void _actor04000StateDeathBurst(Enemy* enemy, Task* task);
static void _actor04000SpawnHitEffect(Task* task, s16 hitYaw, u32 attackKey);
static void _actor04000ApplyAttackHit(Enemy* enemy, Task* task);
static void _actor04000StatePatrol(Enemy* enemy, Task* task);
static void _actor04000StateReturn(Enemy* enemy, Task* task);
static void _actor04000StateDrop(Enemy* enemy, Task* task);
static void _actor04000StateScriptedThrash(Enemy* enemy, Task* task);
static void _actor04000StateScriptedFall(Enemy* enemy, Task* task);
static void _actor04000StateScriptedLeap(Enemy* enemy, Task* task);
static void _actor04000Tick(Enemy* enemy, Task* task);

/// Clamps a wrapped turn and rebuilds the root yaw in 4096ths of a turn.
///
/// The task and scratch arguments must be stable, side-effect-free expressions;
/// each is evaluated more than once. The limit is a nonnegative signed constant within
/// the signed-halfword yaw range. Reads and replaces `scratchArg->angle`, uses
/// the live model root, and leaves dirty marking to its movement caller.
/// Expands to a compound statement; use only as a standalone statement.
#define ACTOR_04000_APPLY_LIMITED_YAW_TURN(taskArg, scratchArg, maxTurn)                                                          \
    {                                                                                                                             \
        if ((scratchArg)->angle > (maxTurn)) {                                                                                    \
            (scratchArg)->angle = (maxTurn);                                                                                      \
        }                                                                                                                         \
        if ((scratchArg)->angle < -(maxTurn)) {                                                                                   \
            (scratchArg)->angle = -(maxTurn);                                                                                     \
        }                                                                                                                         \
        (scratchArg)->angle += ratan2(-(taskArg)->extra.tmd->coords->coord.m[2][0], (taskArg)->extra.tmd->coords->coord.m[2][2]); \
        gfxRotMatrixY(&(taskArg)->extra.tmd->coords->coord, (scratchArg)->angle, GRAPHICS_ROTATION_REPLACE);                      \
    }

#include "../../shared/actor_contacts_steer.inc.c"

#include "../../shared/actor_contacts_push_contact.inc.c"

/// Applies room commands for hanging, saloon and general-store encounters.
///
/// Borrows the command through dispatch and returns 0, including for ignored
/// commands. Hanging-pool placements must be in 0..5; their index selects the
/// pool entry and ceiling position. The other contexts hide, arm or place the
/// actor for its scripted entry. The context is not saved in the work block.
static s32 _actor04000ApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg)
{
    _Actor04000Work* work;
    Enemy*           enemy;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (command->context.key == ACTOR_04000_COMMAND_CONTEXT_HANGING_POOL && command->command == ACTOR_04000_POOL_COMMAND_HANG) {
        work->state                                                   = ACTOR_04000_STATE_HANG;
        Actor04000_D0C718[enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT] = task;
        enemy->node.state.parts.flags                                 = WORLD_TARGET_NOT_LOCKABLE;
        switch (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) {
            case 0:
                task->extra.tmd->coords->coord.t[0]   = 0x116;
                task->extra.tmd->coords->coord.t[1]   = -0xBB8;
                task->extra.tmd->coords->coord.t[2]   = 0x6A4;
                task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                break;
            case 1:
                task->extra.tmd->coords->coord.t[0]   = 0x2BC;
                task->extra.tmd->coords->coord.t[1]   = -0xBB8;
                task->extra.tmd->coords->coord.t[2]   = 0x56A;
                task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                break;
            case 2:
                task->extra.tmd->coords->coord.t[0]   = -0x1E;
                task->extra.tmd->coords->coord.t[1]   = -0xBB8;
                task->extra.tmd->coords->coord.t[2]   = 0x500;
                task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                break;
            case 3:
                task->extra.tmd->coords->coord.t[0]   = 0xB2;
                task->extra.tmd->coords->coord.t[1]   = -0xBB8;
                task->extra.tmd->coords->coord.t[2]   = 0x22E;
                task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                break;
            case 4:
                task->extra.tmd->coords->coord.t[0]   = 0x21E;
                task->extra.tmd->coords->coord.t[1]   = -0xBB8;
                task->extra.tmd->coords->coord.t[2]   = -0xF2;
                task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                break;
            case 5:
                task->extra.tmd->coords->coord.t[0]   = -0x46;
                task->extra.tmd->coords->coord.t[1]   = -0xBB8;
                task->extra.tmd->coords->coord.t[2]   = -0x20B;
                task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                break;
        }
    }
    if (command->context.key == ACTOR_04000_COMMAND_CONTEXT_SALOON) {
        switch (command->command) {
            case ACTOR_04000_SALOON_COMMAND_HIDE:
                work->state = ACTOR_04000_STATE_HIDDEN;
                break;
            case ACTOR_04000_SALOON_COMMAND_START:
                switch (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) {
                    case 0:
                        work->state                         = ACTOR_04000_STATE_SCRIPTED_THRASH;
                        task->extra.tmd->coords->coord.t[0] = -0x3AC;
                        task->extra.tmd->coords->coord.t[1] = -0xF0;
                        task->extra.tmd->coords->coord.t[2] = 0x166C;
                        gfxRotMatrixY(&task->extra.tmd->coords->coord, 0x3E8, GRAPHICS_ROTATION_REPLACE);
                        sceneEngageBattle(1);
                        break;
                    case 1:
                        work->state                         = ACTOR_04000_STATE_SCRIPTED_FALL;
                        task->extra.tmd->coords->coord.t[0] = 0x2A8;
                        task->extra.tmd->coords->coord.t[1] = -0x7D0;
                        task->extra.tmd->coords->coord.t[2] = 0x189C;
                        gfxRotMatrixY(&task->extra.tmd->coords->coord, ACTOR_TRANSFORM_ANGLE_HALF_TURN, GRAPHICS_ROTATION_REPLACE);
                        break;
                }
                break;
        }
    }
    if (command->context.key == ACTOR_04000_COMMAND_CONTEXT_GENERAL_STORE) {
        switch (command->command) {
            case ACTOR_04000_STORE_COMMAND_AWAIT_BATTLE:
                work->state = ACTOR_04000_STATE_AWAIT_BATTLE;
                break;
            case ACTOR_04000_STORE_COMMAND_HIDE:
                work->state = ACTOR_04000_STATE_HIDDEN;
                break;
            case ACTOR_04000_STORE_COMMAND_ENTER:
                switch (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) {
                    case 0:
                        task->extra.tmd->coords->coord.t[0]   = 0xF1E;
                        task->extra.tmd->coords->coord.t[1]   = -0x384;
                        task->extra.tmd->coords->coord.t[2]   = 0xFE6;
                        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                        gfxRotMatrixY(&task->extra.tmd->coords->coord, -0x400, GRAPHICS_ROTATION_REPLACE);
                        work->state = ACTOR_04000_STATE_SCRIPTED_FALL;
                        break;
                    case 1:
                        task->extra.tmd->coords->coord.t[0] = 0xA1E;
                        task->extra.tmd->coords->coord.t[1] = -0x384;
                        task->extra.tmd->coords->coord.t[2] = 0x1590;
                        gfxRotMatrixY(&task->extra.tmd->coords->coord, 0x7D0, GRAPHICS_ROTATION_REPLACE);
                        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                        work->state                           = ACTOR_04000_STATE_SCRIPTED_LEAP;
                        break;
                    case 2:
                        task->extra.tmd->coords->coord.t[0] = 0x1A4;
                        task->extra.tmd->coords->coord.t[1] = -0x4C4;
                        task->extra.tmd->coords->coord.t[2] = 0x1194;
                        gfxRotMatrixY(&task->extra.tmd->coords->coord, 0x3E8, GRAPHICS_ROTATION_REPLACE);
                        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                        work->state                           = ACTOR_04000_STATE_SCRIPTED_LEAP;
                        break;
                    case 3:
                    case 4:
                    default:
                        work->state = ACTOR_04000_STATE_SCRIPTED_FALL;
                        break;
                }
                break;
        }
    }
    return 0;
}

#include "../../shared/anim_driver_tick.inc.c"

/// Polls the requested walk, run or idle animation for a sound script key.
///
/// Walk/run cue indices are latched so a held cue fires once; any other movement
/// cue clears the latch. Idle emits on each slot-1 control-jump tick. Returns 0
/// for no sound. The caller adds the placement index in bits 8..15 and spatial
/// audio parameters; this function only updates `lastSoundCueIndex`.
static s32 _actor04000PollAnimationSound(_Actor04000Work* work)
{
    enum {
        ACTOR_04000_WALK_CUE_A      = 0x15,
        ACTOR_04000_WALK_CUE_B      = 0x11,
        ACTOR_04000_RUN_CUE_A       = 0xD,
        ACTOR_04000_RUN_CUE_B       = 0x12,
        ACTOR_04000_SOUND_MOVEMENT  = 0x40280001,
        ACTOR_04000_SOUND_IDLE_LOOP = 0x400C0005,
    };
    u16 cueIndex;
    s32 cueValue;

    // Retain the halfword cue separately across the shared trigger and latch paths.
    switch (work->driver.requestedSet) {
        case ACTOR_04000_ANIMATION_WALK:
            cueIndex = work->rig.slots[ANIM_DRIVER_FIRST_SLOT].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            cueValue = cueIndex;
            if (cueValue != ACTOR_04000_WALK_CUE_A) {
                goto otherWalkCue;
            }
        emitMovementSound:
            if (work->lastSoundCueIndex == cueValue) {
                goto retainMovementCue;
            }
            work->lastSoundCueIndex = cueIndex;
            return ACTOR_04000_SOUND_MOVEMENT;
        otherWalkCue:
            if (cueValue == ACTOR_04000_WALK_CUE_B) {
                goto emitMovementSound;
            }
        clearMovementCue:
            work->lastSoundCueIndex = 0;
            break;
        case ACTOR_04000_ANIMATION_RUN:
            cueIndex = work->rig.slots[ANIM_DRIVER_FIRST_SLOT].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            cueValue = cueIndex;
            if (cueValue != ACTOR_04000_RUN_CUE_A && cueValue != ACTOR_04000_RUN_CUE_B) {
                goto clearMovementCue;
            }
            goto emitMovementSound;
        retainMovementCue:
            work->lastSoundCueIndex = cueIndex;
            break;
        case ACTOR_04000_ANIMATION_IDLE:
            if (work->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP) {
                return ACTOR_04000_SOUND_IDLE_LOOP;
            }
            break;
    }
    return 0;
}

// animation bank handed to `animationInitContext`

/// Initializes the hanging enemy's animation, collision bodies and patrol route.
///
/// Requires a live enemy, a model with six coordinates and the loaded clip bank.
/// Owns zeroed work until enemy teardown; allocation failure destroys the enemy
/// and task. Links two body probes and two inactive burst spheres, initializes
/// their complete contact arrays, and borrows the work's lighting matrices.
/// Patrol endpoints are 1000 parent-coordinate units ahead and behind spawn.
/// Spawn argument 1's high half selects IDLE for 1 and PATROL otherwise.
/// Playback and two unused tunings vary with placement index; HP uses the
/// fixed enemy parameter record. Acquires one scene battle reference.
static void _actor04000SpawnEnemy(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_04000_COLLISION_BODY_ID        = 12,
        ACTOR_04000_PATROL_ENDPOINT_DISTANCE = 1000,
        ACTOR_04000_SPAWN_IDLE_SELECTOR      = 1,
        ACTOR_04000_NO_PREVIOUS_STATE        = -1
    };
    TmdObject*             model;
    GfxCoord*              rootCoord;
    _Actor04000Work*       work;
    WorldCollisionContact* hitContacts;
    SVECTOR                offsetAndFacing;
    VECTOR                 lightingPosition;
    SVECTOR*               bodyOffset;
    SVECTOR*               patrolDirection;
    WorldCollisionBody*    gridBody;
    WorldCollisionBody*    hitBody;
    WorldCollisionBody*    burstAttackBody;
    WorldCollisionBody*    burstWaveBody;

    model      = task->extra.tmd;
    rootCoord  = model->coords;
    work       = memCalloc(sizeof(*work), false);
    task->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    rootCoord->parent = &gGfxViewCoord;
    task->msgTable    = Actor04000_D0C6B0;
    work->field_180   = 0;
    work->field_184   = 1;
    work->field_18C   = 3;
    work->field_188   = 0;
    work->field_190   = 1;
    model->flags      = 0;
    animationInitContext(&work->rig.anim, Actor04000_D0C4C4, model, work->rig.poses, work->rig.slots);

    // Link the grid probe, hit sphere and two initially inactive burst bodies.
    // Link two probes and two initially inactive burst spheres.
    gridBody                   = &work->gridBody;
    gridBody->coord            = task->extra.tmd->coords + 1;
    gridBody->context.contacts = work->gridContacts;
    gridBody->pos.vy           = -0x110;
    gridBody->pos.vx           = 0;
    gridBody->pos.vz           = 0;
    gridBody->key              = (WORLD_COLLISION_CONTACT_ENEMY_BODY | ACTOR_04000_COLLISION_BODY_ID);
    gridBody->radius           = 0x190;
    gridBody->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, gridBody);
    gridBody->flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    worldCollisionInitContacts(gridBody->context.contacts, ARRAY_SIZE(work->gridContacts), 0);

    hitBody                   = &work->hitBody;
    offsetAndFacing.vx        = 0;
    offsetAndFacing.vy        = -0x168;
    offsetAndFacing.vz        = 0;
    bodyOffset                = &offsetAndFacing;
    hitContacts               = work->hitContacts;
    hitBody->coord            = task->extra.tmd->coords + 2;
    hitBody->context.contacts = hitContacts;
    hitBody->pos.vx           = bodyOffset->vx;
    hitBody->pos.vy           = bodyOffset->vy;
    hitBody->pos.vz           = bodyOffset->vz;
    hitBody->key              = (WORLD_COLLISION_CONTACT_ENEMY_BODY | ACTOR_04000_COLLISION_BODY_ID);
    hitBody->radius           = 0x168;
    hitBody->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, hitBody);
    hitBody->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(hitBody->context.contacts, ARRAY_SIZE(work->hitContacts), 0);

    offsetAndFacing.vx                = 0;
    offsetAndFacing.vy                = 0;
    offsetAndFacing.vz                = 0;
    burstAttackBody                   = &work->burstAttackBody;
    burstAttackBody->coord            = &gGfxViewCoord;
    burstAttackBody->context.contacts = work->burstAttackContacts;
    burstAttackBody->pos.vx           = bodyOffset->vx;
    burstAttackBody->pos.vy           = bodyOffset->vy;
    burstAttackBody->pos.vz           = bodyOffset->vz;
    burstAttackBody->radius           = 0x500;
    burstAttackBody->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, burstAttackBody);
    worldCollisionInitContacts(burstAttackBody->context.contacts, ARRAY_SIZE(work->burstAttackContacts), 0);

    burstWaveBody                   = &work->burstWaveBody;
    burstWaveBody->coord            = &gGfxViewCoord;
    burstWaveBody->context.contacts = work->burstWaveContacts;
    burstWaveBody->pos.vx           = bodyOffset->vx;
    burstWaveBody->pos.vy           = bodyOffset->vy;
    burstWaveBody->pos.vz           = bodyOffset->vz;
    burstWaveBody->radius           = 0x80;
    burstWaveBody->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_BLASTS, burstWaveBody);
    worldCollisionInitContacts(burstWaveBody->context.contacts, ARRAY_SIZE(work->burstWaveContacts), 0);

    enemy->field_4    = &rootCoord->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = task->extra.tmd->coords + 2;
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->reactionFlags          = 0;
    enemy->hp = enemy->hpMax  = Actor04000_D07084.hpMax;
    enemy->param              = &Actor04000_D07084;
    enemy->recs               = hitContacts;
    work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
    work->driver.requestedSet = ACTOR_04000_ANIMATION_HOLD;
    work->driver.rate         = ANIMATION_RATE_ONE;
    work->driver.rateBias     = 0;
    _animDriverTick(task);
    work->field_17E         = 0;
    work->field_A           = 0;
    model->lightMtx         = &work->lightMtx;
    model->colorMtx         = &work->colorMtx;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    lightingPosition.vx = rootCoord->workm.t[0];
    lightingPosition.vy = rootCoord->workm.t[1];
    lightingPosition.vz = rootCoord->workm.t[2];
    worldCoordUpdateActorColor(enemy, &lightingPosition, 0, 0);
    work->field_1A0 = 5;
    work->field_1A2 = 0x14;
    if ((u16)(enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) % 2 == 1) {
        work->driver.rate += enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        work->field_1A2   += enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        work->field_1A0   += enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    } else {
        work->driver.rate -= (u16)(enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) / 2;
        work->field_1A2   -= (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) / 2;
        work->field_1A0   -= (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) / 2;
    }
    // Patrol endpoints straddle the spawn along its flattened facing direction.
    work->spawnPos.vx = task->extra.tmd->coords->coord.t[0];
    work->spawnPos.vy = task->extra.tmd->coords->coord.t[1];
    work->spawnPos.vz = task->extra.tmd->coords->coord.t[2];
    // The temporary vector now holds the flattened patrol-facing direction.
    gfxReadMatrixZAxis(&task->extra.tmd->coords->coord, &offsetAndFacing);
    offsetAndFacing.vy = 0;
    patrolDirection    = &offsetAndFacing;
    VectorNormalSS(patrolDirection, patrolDirection);
    gte_lddp(ACTOR_04000_PATROL_ENDPOINT_DISTANCE);
    gte_ldsv(patrolDirection);
    gte_gpf12();
    gte_stsv(patrolDirection);
    work->patrolPoints[0].vx = task->extra.tmd->coords->coord.t[0] + offsetAndFacing.vx;
    work->patrolPoints[0].vy = task->extra.tmd->coords->coord.t[1];
    work->patrolPoints[0].vz = task->extra.tmd->coords->coord.t[2] + offsetAndFacing.vz;
    work->patrolPoints[1].vx = task->extra.tmd->coords->coord.t[0] - offsetAndFacing.vx;
    work->patrolPoints[1].vy = task->extra.tmd->coords->coord.t[1];
    work->patrolPoints[1].vz = task->extra.tmd->coords->coord.t[2] - offsetAndFacing.vz;
    sceneAcquireBattleRef(0);
    if ((task->spawnArg1.value >> 16) == 0) {
        work->state = ACTOR_04000_STATE_PATROL;
    } else if ((task->spawnArg1.value >> 16) == ACTOR_04000_SPAWN_IDLE_SELECTOR) {
        work->state = ACTOR_04000_STATE_IDLE;
    } else {
        work->state = ACTOR_04000_STATE_PATROL;
    }
    work->prevState    = ACTOR_04000_NO_PREVIOUS_STATE;
    work->airborne     = 0;
    work->missedLunges = 0;
    task->state++;
}

/// Lunges at the player and starts a button-press hold when the catch succeeds.
///
/// Steps forward on ticks 8 and 9. From tick 9, a player within 600 X/Z units
/// and strictly less than an eighth turn can accept the twelve-press hold.
/// A catch selects a side-specific player clip and places the actor beside
/// the player, facing inward; a miss enters `LUNGE_RECOVER`.
static void _actor04000StateLunge(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_04000_LUNGE_MAX_YAW          = ACTOR_TRANSFORM_ANGLE_TURN / 8,
        ACTOR_04000_LUNGE_HOLD_PRESS_COUNT = 12,
        ACTOR_04000_PLAYER_LATCH_ANIMATION = 1,
    };

    _Actor04000Work*         work;
    Task*                    player;
    GameActor*               playerActor;
    TmdObject*               model;
    _Actor04000LungeScratch* scratch;
    GfxCoord*                facingCoord;
    GfxCoord*                rootCoord;
    s16                      yawDelta;
    s32                      absYawDelta;

    work        = task->work;
    player      = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    playerActor = player->work;
    if (work->stateEntered != 0) {
        model                                                     = task->extra.tmd;
        ((Enemy*)task->spawnArg2.pointer)->node.state.parts.flags = 0;
        sceneEngageBattle(1);
        model->flags              = 0;
        work->driver.state        = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rate         = ANIMATION_RATE_ONE;
        work->driver.rateBias     = 0;
        work->driver.requestedSet = ACTOR_04000_ANIMATION_LUNGE;
        work->gridBody.flags     |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _animDriverTick(task);
        work->stateFrame = 0;
        return;
    }
    _animDriverTick(task);
    work->stateFrame++;
    if (work->stateFrame < 8) {
        return;
    }
    if (work->stateFrame == 8) {
        _actorMovementTranslateForwardNonzero(task->extra.tmd->coords, 0x32);
        return;
    }
    if (work->stateFrame == 9) {
        _actorMovementTranslateForwardNonzero(task->extra.tmd->coords, 0x32);
    }
    // A missed catch recovers; only an accepted player hold overrides this.
    work->state       = ACTOR_04000_STATE_LUNGE_RECOVER;
    scratch           = SCRATCH_STACK_RESERVE_BLOCK(_Actor04000LungeScratch);
    rootCoord         = task->extra.tmd->coords;
    scratch->delta.vx = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
    scratch->delta.vy = gPlayerStatus.coordMtx->t[1] - rootCoord->coord.t[1];
    scratch->delta.vz = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
    facingCoord       = task->extra.tmd->coords;
    yawDelta          = ratan2(scratch->delta.vx, scratch->delta.vz) - ratan2(-facingCoord->coord.m[2][0], facingCoord->coord.m[2][2]);
    scratch->turn     = _actorAngleNormalizeYaw(yawDelta);
    if (!_actorRangeOutsideRadiusXZ(&scratch->delta, 600)) {
        absYawDelta = (scratch->turn >= 0) ? scratch->turn : -scratch->turn;
        if (absYawDelta < ACTOR_04000_LUNGE_MAX_YAW) {
            if (playerActor->mode != GAME_ACTOR_MODE_SCRIPTED) {
                work->playerButtonHold.pressCount = ACTOR_04000_LUNGE_HOLD_PRESS_COUNT;
                if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &work->playerButtonHold, 0) == 0) {
                    facingCoord   = player->extra.tmd->coords;
                    yawDelta      = ratan2(scratch->delta.vx, scratch->delta.vz) - ratan2(-facingCoord->coord.m[2][0], facingCoord->coord.m[2][2]);
                    scratch->turn = _actorAngleNormalizeYaw(yawDelta);
                    if (scratch->turn < 0) {
                        Actor04000_D0C530.source.sets = Actor04000_D0C510;
                    } else {
                        Actor04000_D0C530.source.sets = Actor04000_D0C520;
                    }
                    Actor04000_D0C530.animationId = ACTOR_04000_PLAYER_LATCH_ANIMATION;
                    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &Actor04000_D0C530, 0);
                    work->state         = ACTOR_04000_STATE_LATCHED;
                    work->holdingPlayer = 1;
                    // Place on the selected player side, then turn back toward them.
                    gfxReadMatrixXAxis(&player->extra.tmd->coords->coord, &scratch->delta);
                    scratch->delta.vy = 0;
                    VectorNormalSS(&scratch->delta, &scratch->delta);
                    if (scratch->turn < 0) {
                        gte_lddp(-0x3C);
                        gte_ldsv(&scratch->delta);
                        gte_gpf12();
                        gte_stsv(&scratch->delta);
                    } else {
                        gte_lddp(0x3C);
                        gte_ldsv(&scratch->delta);
                        gte_gpf12();
                        gte_stsv(&scratch->delta);
                    }
                    task->extra.tmd->coords->coord.t[0] = player->extra.tmd->coords->coord.t[0] + scratch->delta.vx;
                    task->extra.tmd->coords->coord.t[1] = player->extra.tmd->coords->coord.t[1];
                    task->extra.tmd->coords->coord.t[2] = player->extra.tmd->coords->coord.t[2] + scratch->delta.vz;
                    scratch->delta.vy                   = 0;
                    _actorMovementBuildDisplacement(&scratch->delta, -600);
                    task->extra.tmd->coords->coord.t[0] += scratch->delta.vx;
                    task->extra.tmd->coords->coord.t[2] += scratch->delta.vz;
                    scratch->delta.vx                    = -scratch->delta.vx;
                    scratch->delta.vy                    = -scratch->delta.vy;
                    scratch->delta.vz                    = -scratch->delta.vz;
                    scratch->facingYaw                   = ratan2(scratch->delta.vx, scratch->delta.vz);
                    gfxRotMatrixY(&task->extra.tmd->coords->coord, scratch->facingYaw, GRAPHICS_ROTATION_REPLACE);
                    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                }
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor04000LungeScratch);
}

/// Releases the latched player through a delayed burst and hides the actor.
///
/// Selects the held player's animation 2, drifts 21 units along the root's
/// flattened local X axis for 19 ticks, then releases the scripted hold on tick
/// 91. The attack and blast pulse begins on tick 92; the model reddens, fades
/// and swells before the state becomes `HIDDEN` on tick 119. Burst centres are
/// sampled on entry and remain fixed while the visible root drifts.
static void _actor04000StateReleaseBurst(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_04000_RELEASE_DRIFT_TICKS      = 0x13,
        ACTOR_04000_RELEASE_PLAYER_TICK      = 0x5B,
        ACTOR_04000_RELEASE_PULSE_TICK       = 0x5C,
        ACTOR_04000_RELEASE_WAVE_MIDDLE_TICK = 0x5D,
        ACTOR_04000_RELEASE_ATTACK_END_TICK  = 0x5E,
        ACTOR_04000_RELEASE_REWARDS_TICK     = 0x60,
        ACTOR_04000_RELEASE_HIDE_MODEL_TICK  = 0x62,
        ACTOR_04000_RELEASE_HIDDEN_TICK      = 0x77,
        ACTOR_04000_RELEASE_RED_START_TICK   = 0x17,
        ACTOR_04000_RELEASE_RED_BASE_TICK    = 0x16,
        ACTOR_04000_RELEASE_SCALE_END_TICK   = 0x65,
        ACTOR_04000_RELEASE_GROWTH_BASE_TICK = 0x5A,
        ACTOR_04000_RELEASE_DRIFT_STEP_UNITS = 21,
        ACTOR_04000_PLAYER_RELEASE_ANIMATION = 2,
    };
    SVECTOR          driftStep;
    SVECTOR*         driftStepPtr;
    VECTOR           scale;
    _Actor04000Work* work;
    TmdObject*       model;
    s16              scaleQ12;
    s32              audioPan;
    s32              soundId;

    work  = task->work;
    model = task->extra.tmd;
    if (work->stateEntered != 0) {
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                  = 0;
        work->hitBody.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstAttackBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstAttackBody.key     = damagePackEnemyAttackKey(enemy, ACTOR_04000_BURST_ATTACK_INDEX);
        work->burstWaveBody.key       = ACTOR_04000_BURST_WAVE_CONTACT_KEY;
        work->stateFrame              = 0;
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->savedColorMtx           = work->colorMtx;
        work->driver.requestedSet     = ACTOR_04000_ANIMATION_RELEASE_BURST;
        work->driver.state            = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias         = 0;
        _animDriverTick(task);
        work->burstWaveBody.pos.vx    = task->extra.tmd->coords->coord.t[0];
        work->burstWaveBody.pos.vy    = task->extra.tmd->coords->coord.t[1] - ACTOR_04000_BURST_WAVE_HEIGHT;
        work->burstWaveBody.pos.vz    = task->extra.tmd->coords->coord.t[2];
        work->burstAttackBody.pos.vx  = task->extra.tmd->coords->coord.t[0];
        work->burstAttackBody.pos.vy  = task->extra.tmd->coords->coord.t[1];
        work->burstAttackBody.pos.vz  = task->extra.tmd->coords->coord.t[2];
        Actor04000_D0C530.animationId = ACTOR_04000_PLAYER_RELEASE_ANIMATION;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &Actor04000_D0C530, 0);
        work->stateFrame = 0;
    }
    /// Steps the release drift along the root's flattened local X axis.
    ///
    /// Captures stable locals `task`, `driftStep` and `driftStepPtr`, borrowing
    /// the root and vector only for this tick. Normalizes in Q12 and narrows
    /// the scaled step to halfwords; applies X/Z and marks the root dirty.
    /// Expands to a compound statement with no walking freeze gate; use only
    /// as a standalone statement inside this handler.
#define ACTOR_04000_STEP_RELEASE_DRIFT()                                 \
    {                                                                    \
        gfxReadMatrixXAxis(&task->extra.tmd->coords->coord, &driftStep); \
        driftStepPtr = &driftStep;                                       \
        driftStep.vy = 0;                                                \
        VectorNormalSS(driftStepPtr, driftStepPtr);                      \
        gte_lddp(ACTOR_04000_RELEASE_DRIFT_STEP_UNITS);                  \
        gte_ldsv(driftStepPtr);                                          \
        gte_gpf12();                                                     \
        gte_stsv(driftStepPtr);                                          \
        task->extra.tmd->coords->coord.t[0]  += driftStep.vx;            \
        task->extra.tmd->coords->coord.t[2]  += driftStep.vz;            \
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;    \
    }
    if (work->stateFrame < ACTOR_04000_RELEASE_DRIFT_TICKS) {
        ACTOR_04000_STEP_RELEASE_DRIFT();
    }
#undef ACTOR_04000_STEP_RELEASE_DRIFT
    _animDriverTick(task);
    // Stage collision pulses, rewards and visual disappearance on fixed ticks.
    switch (work->stateFrame) {
        case ACTOR_04000_RELEASE_PLAYER_TICK:
            if (work->holdingPlayer == 1) {
                if (((GameActor*)(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->work)->mode == GAME_ACTOR_MODE_SCRIPTED) {
                    taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                }
                work->holdingPlayer = 0;
            }
            task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            break;
        case ACTOR_04000_RELEASE_PULSE_TICK:
            padScriptSpawnDepthScaled(Actor04000_D07094, Actor04000_D070A0,
                                      (s16)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
            work->burstAttackBody.radius = ACTOR_04000_BURST_ATTACK_RADIUS;
            work->burstWaveBody.radius   = ACTOR_04000_BURST_WAVE_START_RADIUS;
            work->burstAttackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->burstWaveBody.flags   |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[2], 1, NULL);
            break;
        case ACTOR_04000_RELEASE_WAVE_MIDDLE_TICK:
            work->burstWaveBody.radius = ACTOR_04000_BURST_WAVE_MIDDLE_RADIUS;
            break;
        case ACTOR_04000_RELEASE_ATTACK_END_TICK:
            work->burstWaveBody.radius   = ACTOR_04000_BURST_WAVE_END_RADIUS;
            work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case ACTOR_04000_RELEASE_REWARDS_TICK:
            sceneReleaseBattleRefWithRewards(task, 0xC);
            work->burstWaveBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case ACTOR_04000_RELEASE_HIDE_MODEL_TICK:
            if (work->airborne == 0) {
                effectSpawn(EFFECT_RED_GROUND_GLOW, task->extra.tmd->coords, 0, NULL);
            }
            model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            soundId      = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_04000_SOUND_BURST;
            audioPan     = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
            break;
        case ACTOR_04000_RELEASE_HIDDEN_TICK:
            work->state = ACTOR_04000_STATE_HIDDEN;
            break;
        default:
            work->colorMtx = work->savedColorMtx;
            break;
    }
    // Rebuild this tick's colour from the saved lighting so fading does not accumulate.
    work->colorMtx = work->savedColorMtx;
    if (work->stateFrame >= ACTOR_04000_RELEASE_RED_START_TICK && work->stateFrame < ACTOR_04000_RELEASE_PLAYER_TICK) {
        work->colorMtx.t[0] += (work->stateFrame - ACTOR_04000_RELEASE_RED_BASE_TICK) * 96;
    }
    if (work->stateFrame >= ACTOR_04000_RELEASE_PULSE_TICK && work->stateFrame < ACTOR_04000_RELEASE_SCALE_END_TICK) {
        scaleQ12 = ACTOR_04000_BURST_COLOR_START_Q12 - (work->stateFrame - ACTOR_04000_RELEASE_PULSE_TICK) * 600;
        if (scaleQ12 < ACTOR_04000_BURST_COLOR_ZERO_CUTOFF_Q12) {
            scale.vx = scale.vy = scale.vz = 0;
            _actorRenderRescaleYaw(task->extra.tmd->coords, ONE);
            ScaleMatrix(&work->colorMtx, &scale);
            work->colorMtx.t[0] = work->colorMtx.t[1] = work->colorMtx.t[2] = 0;
            _actorRenderRescaleYaw(task->extra.tmd->coords, ONE);
        } else {
            scale.vx = scale.vy = scale.vz = scaleQ12;
            work->colorMtx                 = work->savedColorMtx;
            ScaleMatrix(&work->colorMtx, &scale);
            gte_lddp(scaleQ12);
            gte_ldlvl(work->colorMtx.t);
            gte_gpf12();
            gte_stlvl(work->colorMtx.t);
            scaleQ12 = (work->stateFrame - ACTOR_04000_RELEASE_GROWTH_BASE_TICK) * (ONE / 4) + ONE;
            if (scaleQ12 > (2 * ONE)) {
                scaleQ12 = (2 * ONE);
            }
            _actorRenderRescaleYaw(task->extra.tmd->coords, scaleQ12);
        }
    }
    if (work->stateFrame < ACTOR_04000_BURST_TIMER_LIMIT) {
        work->stateFrame++;
    } else {
        work->state = ACTOR_04000_STATE_HIDDEN;
    }
}

/// Idles until a looping-animation roll or the nearby player rouses the actor.
///
/// After at least 25 slot-1 loop jumps, each jump has a one-in-eight wake
/// chance. A player within 2000 X/Z units engages battle and rouses it too.
static void _actor04000StateIdle(Enemy* enemy, Task* task)
{
    _Actor04000Work* work;
    GfxCoord*        rootCoord;
    SVECTOR          playerOffset;
    SVECTOR*         playerOffsetPtr;
    TmdObject*       model;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        work->driver.requestedSet     = ACTOR_04000_ANIMATION_IDLE;
        work->driver.state            = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias         = 0;
        work->hitBody.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->burstAttackBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _animDriverTick(task);
        return;
    }
    _animDriverTick(task);
    if ((work->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP) && work->driver.jumpCount >= 0x19) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 0x10) & 7)) {
            work->state = ACTOR_04000_STATE_ROUSE;
        }
    }
    rootCoord           = task->extra.tmd->coords;
    playerOffsetPtr     = &playerOffset;
    playerOffset.vx     = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
    playerOffsetPtr->vy = gPlayerStatus.coordMtx->t[1] - rootCoord->coord.t[1];
    playerOffsetPtr->vz = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
    if (!_actorRangeOutsideRadiusXZ(playerOffsetPtr, 2000)) {
        sceneEngageBattle(1);
        work->state = ACTOR_04000_STATE_ROUSE;
    }
}

/// Runs toward the player, lunging in reach or returning after a long separation.
///
/// Turns by at most 16 of 4096 yaw units per tick, then moves and resolves
/// grid and body contacts. Over 240 consecutive ticks beyond 1000 X/Z units
/// requests `RETURN`; a later reach test can override that with `LUNGE`.
static void _actor04000StateChase(Enemy* enemy, Task* task)
{
    enum { ACTOR_04000_CHASE_YAW_STEP = 16 };

    _Actor04000Work*  work;
    ActorTurnScratch* scratchHead;
    ActorTurnScratch* turnScratch;
    GfxCoord*         facingCoord;
    GfxCoord*         rangeCoord;
    GfxCoord*         rootCoord;
    TmdObject*        model;
    s16               yawDelta;
    s32               absYawDelta;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        work->driver.requestedSet     = ACTOR_04000_ANIMATION_RUN;
        work->driver.state            = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias         = ANIMATION_RATE_ONE;
        work->hitBody.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->burstAttackBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        sceneEngageBattle(1);
        _animDriverTick(task);
        work->chaseFarFrames = 0;
        return;
    }
    scratchHead = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    turnScratch = SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    _animDriverTick(task);
    rootCoord                = task->extra.tmd->coords;
    scratchHead[-1].delta.vx = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
    turnScratch->delta.vy    = gPlayerStatus.coordMtx->t[1] - rootCoord->coord.t[1];
    turnScratch->delta.vz    = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
    facingCoord              = task->extra.tmd->coords;
    yawDelta                 = ratan2(scratchHead[-1].delta.vx, turnScratch->delta.vz) - ratan2(-facingCoord->coord.m[2][0], facingCoord->coord.m[2][2]);
    turnScratch->angle       = _actorAngleNormalizeYaw(yawDelta);
    ACTOR_04000_APPLY_LIMITED_YAW_TURN(task, turnScratch, ACTOR_04000_CHASE_YAW_STEP);
    _actorMovementTranslateForwardNonzero(task->extra.tmd->coords, 0x14);
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    if (_actorRangeOutsideRadiusXZ(&turnScratch->delta, 1000)) {
        work->chaseFarFrames++;
    } else {
        work->chaseFarFrames = 0;
    }
    _actorContactApplyAvoidancePushback(task->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts), &turnScratch->delta);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    turnScratch->delta.vx                 = work->spawnPos.vx - task->extra.tmd->coords->coord.t[0];
    turnScratch->delta.vy                 = 0;
    turnScratch->delta.vz                 = work->spawnPos.vz - task->extra.tmd->coords->coord.t[2];
    _actorRangeOutsideRadiusXZ(&turnScratch->delta, 3000);
    if (work->chaseFarFrames > 0xF0) {
        work->state = ACTOR_04000_STATE_RETURN;
    }
    rangeCoord            = task->extra.tmd->coords;
    turnScratch->delta.vx = gPlayerStatus.coordMtx->t[0] - rangeCoord->coord.t[0];
    turnScratch->delta.vy = gPlayerStatus.coordMtx->t[1] - rangeCoord->coord.t[1];
    turnScratch->delta.vz = gPlayerStatus.coordMtx->t[2] - rangeCoord->coord.t[2];
    facingCoord           = task->extra.tmd->coords;
    yawDelta              = ratan2(turnScratch->delta.vx, turnScratch->delta.vz) - ratan2(-facingCoord->coord.m[2][0], facingCoord->coord.m[2][2]);
    turnScratch->angle    = _actorAngleNormalizeYaw(yawDelta);
    if (!_actorRangeOutsideRadiusXZ(&turnScratch->delta, 600)) {
        absYawDelta = (turnScratch->angle >= 0) ? turnScratch->angle : -turnScratch->angle;
        if (absYawDelta < ACTOR_TRANSFORM_ANGLE_TURN / 8) {
            work->state = ACTOR_04000_STATE_LUNGE;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

/// Runs the voluntary burst, releases a held player and finishes with zero HP.
///
/// Keeps taking hits until tick 40, ends its player hold on tick 41, and pulses
/// its entry-position attack and blast bodies from tick 42. It broadcasts a
/// hold-release request to all placed actors, credits rewards on tick 46, and
/// becomes `HIDDEN` with zero HP on tick 69. Airborne bursts omit the ground glow.
static void _actor04000StateSelfBurst(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_04000_SELF_HITS_END_TICK       = 0x28,
        ACTOR_04000_SELF_RELEASE_PLAYER_TICK = 0x29,
        ACTOR_04000_SELF_PULSE_TICK          = 0x2A,
        ACTOR_04000_SELF_WAVE_MIDDLE_TICK    = 0x2B,
        ACTOR_04000_SELF_WAVE_FULL_TICK      = 0x2C,
        ACTOR_04000_SELF_REWARDS_TICK        = 0x2E,
        ACTOR_04000_SELF_GLOW_TICK           = 0x30,
        ACTOR_04000_SELF_HIDE_MODEL_TICK     = 0x32,
        ACTOR_04000_SELF_SKIP_BUFFER_TICK    = 0x34,
        ACTOR_04000_SELF_HIDDEN_TICK         = 0x45,
        ACTOR_04000_SELF_RED_START_TICK      = 0x17,
        ACTOR_04000_SELF_RED_BASE_TICK       = 0x16,
        ACTOR_04000_SELF_SCALE_END_TICK      = 0x33,
        ACTOR_04000_SELF_GROWTH_BASE_TICK    = 0x28,
    };
    VECTOR           scale;
    _Actor04000Work* work;
    TmdObject*       model;
    s16              scaleQ12;
    s32              audioPan;
    s32              soundId;

    work  = task->work;
    model = task->extra.tmd;
    if (work->stateEntered != 0) {
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                  = 0;
        work->hitBody.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->burstAttackBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstAttackBody.key     = damagePackEnemyAttackKey(enemy, ACTOR_04000_BURST_ATTACK_INDEX);
        work->burstWaveBody.key       = ACTOR_04000_BURST_WAVE_CONTACT_KEY;
        work->stateFrame              = 0;
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->savedColorMtx           = work->colorMtx;
        work->driver.rateBias         = 0;
        _animDriverTick(task);
        work->burstWaveBody.pos.vx   = task->extra.tmd->coords->coord.t[0];
        work->burstWaveBody.pos.vy   = task->extra.tmd->coords->coord.t[1] - ACTOR_04000_BURST_WAVE_HEIGHT;
        work->burstWaveBody.pos.vz   = task->extra.tmd->coords->coord.t[2];
        work->burstAttackBody.pos.vx = task->extra.tmd->coords->coord.t[0];
        work->burstAttackBody.pos.vy = task->extra.tmd->coords->coord.t[1];
        work->burstAttackBody.pos.vz = task->extra.tmd->coords->coord.t[2];
        if (work->lastCommandContext == ACTOR_04000_COMMAND_CONTEXT_HANGING_POOL) {
            Actor04000_D0C718[enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT] = NULL;
        }
        return;
    }
    _animDriverTick(task);
    // Stage collision pulses, rewards and visual disappearance on fixed ticks.
    switch (work->stateFrame) {
        case ACTOR_04000_SELF_HITS_END_TICK:
            work->hitBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case ACTOR_04000_SELF_RELEASE_PLAYER_TICK:
            if (work->holdingPlayer == 1) {
                if (((GameActor*)(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->work)->mode == GAME_ACTOR_MODE_SCRIPTED) {
                    taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                }
                work->holdingPlayer = 0;
            }
            task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            break;
        case ACTOR_04000_SELF_PULSE_TICK:
            padScriptSpawnDepthScaled(Actor04000_D07094, Actor04000_D070A0,
                                      (s16)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
            work->burstAttackBody.radius = ACTOR_04000_BURST_ATTACK_RADIUS;
            work->burstWaveBody.radius   = ACTOR_04000_BURST_WAVE_START_RADIUS;
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, 0, ACTOR_MESSAGE_RELEASE_HOLD);
            work->burstAttackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->burstWaveBody.flags   |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[2], 1, NULL);
            break;
        case ACTOR_04000_SELF_WAVE_MIDDLE_TICK:
            work->burstWaveBody.radius = ACTOR_04000_BURST_WAVE_MIDDLE_RADIUS;
            break;
        case ACTOR_04000_SELF_WAVE_FULL_TICK:
            work->burstWaveBody.radius = ACTOR_04000_BURST_WAVE_END_RADIUS;
            break;
        case ACTOR_04000_SELF_REWARDS_TICK:
            sceneReleaseBattleRefWithRewards(task, 0xC);
            work->burstWaveBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case ACTOR_04000_SELF_GLOW_TICK:
            if (work->airborne == 0) {
                effectSpawn(EFFECT_RED_GROUND_GLOW, task->extra.tmd->coords, 0, NULL);
            }
            soundId  = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_04000_SOUND_BURST;
            audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
            break;
        case ACTOR_04000_SELF_HIDE_MODEL_TICK:
            work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            model->flags                 = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case ACTOR_04000_SELF_SKIP_BUFFER_TICK:
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_04000_SELF_HIDDEN_TICK:
            work->state = ACTOR_04000_STATE_HIDDEN;
            enemy->hp   = 0;
            break;
        default:
            work->colorMtx = work->savedColorMtx;
            break;
    }
    // Rebuild this tick's colour from the saved lighting so fading does not accumulate.
    work->colorMtx = work->savedColorMtx;
    if (work->stateFrame >= ACTOR_04000_SELF_RED_START_TICK && work->stateFrame < ACTOR_04000_SELF_RELEASE_PLAYER_TICK) {
        work->colorMtx.t[0] += (work->stateFrame - ACTOR_04000_SELF_RED_BASE_TICK) * 0x60;
    }
    if (work->stateFrame >= ACTOR_04000_SELF_PULSE_TICK && work->stateFrame < ACTOR_04000_SELF_SCALE_END_TICK) {
        scaleQ12 = ACTOR_04000_BURST_COLOR_START_Q12 - (work->stateFrame - ACTOR_04000_SELF_PULSE_TICK) * 600;
        if (scaleQ12 < ACTOR_04000_BURST_COLOR_ZERO_CUTOFF_Q12) {
            scale.vx = scale.vy = scale.vz = 0;
            _actorRenderRescaleYaw(task->extra.tmd->coords, ONE);
            ScaleMatrix(&work->colorMtx, &scale);
            work->colorMtx.t[0] = work->colorMtx.t[1] = work->colorMtx.t[2] = 0;
            _actorRenderRescaleYaw(task->extra.tmd->coords, ONE);
        } else {
            scale.vx = scale.vy = scale.vz = scaleQ12;
            work->colorMtx                 = work->savedColorMtx;
            ScaleMatrix(&work->colorMtx, &scale);
            gte_lddp(scaleQ12);
            gte_ldlvl(work->colorMtx.t);
            gte_gpf12();
            gte_stlvl(work->colorMtx.t);
            scaleQ12 = (work->stateFrame - ACTOR_04000_SELF_GROWTH_BASE_TICK) * (ONE / 4) + ONE;
            if (scaleQ12 > (2 * ONE)) {
                scaleQ12 = (2 * ONE);
            }
            _actorRenderRescaleYaw(task->extra.tmd->coords, scaleQ12);
        }
    }
    if (work->stateFrame < ACTOR_04000_BURST_TIMER_LIMIT) {
        work->stateFrame++;
    } else {
        work->state = ACTOR_04000_STATE_HIDDEN;
    }
}

/// Bursts after lethal damage, credits rewards and hides the actor.
///
/// Disables hit and grid collision, restarts animation 10 at 44 sixteenths of
/// a frame per tick, and samples the two burst centres on entry. The attack
/// pulses on tick 14; the blast grows from ticks 15 to 17 and ends on tick 19.
/// The colour fades and model swells before `HIDDEN` on tick 38. This handler
/// does not release a held player; the per-frame tick handles that separately.
static void _actor04000StateDeathBurst(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_04000_DEATH_FADE_TICK        = 0xD,
        ACTOR_04000_DEATH_ATTACK_TICK      = 0xE,
        ACTOR_04000_DEATH_WAVE_TICK        = 0xF,
        ACTOR_04000_DEATH_WAVE_MIDDLE_TICK = 0x10,
        ACTOR_04000_DEATH_WAVE_FULL_TICK   = 0x11,
        ACTOR_04000_DEATH_WAVE_END_TICK    = 0x13,
        ACTOR_04000_DEATH_HIDE_MODEL_TICK  = 0x15,
        ACTOR_04000_DEATH_SKIP_BUFFER_TICK = 0x17,
        ACTOR_04000_DEATH_HIDDEN_TICK      = 0x26,
        ACTOR_04000_DEATH_SCALE_END_TICK   = 0x16,
        ACTOR_04000_DEATH_FADE_BASE_TICK   = 0xB,
        ACTOR_04000_DEATH_ANIMATION_RATE   = 44,  // Sixteenths of a frame per driver tick
        ACTOR_04000_DEATH_MODEL_GROWTH_Q12 = 180, // Model scale increment per timer tick
    };
    VECTOR           scale;
    _Actor04000Work* work;
    TmdObject*       model;
    s16              scaleQ12;
    s32              audioPan;
    s32              soundId;

    work  = task->work;
    model = task->extra.tmd;
    if (work->stateEntered != 0) {
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                  = 0;
        work->hitBody.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstAttackBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstAttackBody.key     = damagePackEnemyAttackKey(enemy, ACTOR_04000_BURST_ATTACK_INDEX);
        work->burstWaveBody.key       = ACTOR_04000_BURST_WAVE_CONTACT_KEY;
        work->stateFrame              = 0;
        work->gridBody.flags         &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        work->savedColorMtx           = work->colorMtx;
        work->driver.requestedSet     = ACTOR_04000_ANIMATION_DEATH_BURST;
        work->driver.state            = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias         = 0;
        work->driver.rate             = ACTOR_04000_DEATH_ANIMATION_RATE;
        _animDriverTick(task);
        work->burstWaveBody.pos.vx   = task->extra.tmd->coords->coord.t[0];
        work->burstWaveBody.pos.vy   = task->extra.tmd->coords->coord.t[1] - ACTOR_04000_BURST_WAVE_HEIGHT;
        work->burstWaveBody.pos.vz   = task->extra.tmd->coords->coord.t[2];
        work->burstAttackBody.pos.vx = task->extra.tmd->coords->coord.t[0];
        work->burstAttackBody.pos.vy = task->extra.tmd->coords->coord.t[1];
        work->burstAttackBody.pos.vz = task->extra.tmd->coords->coord.t[2];
        sceneEngageBattle(1);
        if (work->lastCommandContext == ACTOR_04000_COMMAND_CONTEXT_HANGING_POOL) {
            Actor04000_D0C718[enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT] = NULL;
        }
    }
    _animDriverTick(task);
    // Stage collision pulses, rewards and visual disappearance on fixed ticks.
    switch (work->stateFrame) {
        case ACTOR_04000_DEATH_FADE_TICK:
            soundId  = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_04000_SOUND_BURST;
            audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
            task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            break;
        case ACTOR_04000_DEATH_ATTACK_TICK:
            work->burstAttackBody.radius = ACTOR_04000_BURST_ATTACK_RADIUS;
            work->burstAttackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[2], 1, NULL);
            padScriptSpawn(Actor04000_D07094, Actor04000_D070A0);
            break;
        case ACTOR_04000_DEATH_WAVE_TICK:
            sceneReleaseBattleRefWithRewards(task, 0xC);
            work->burstWaveBody.radius   = ACTOR_04000_BURST_WAVE_START_RADIUS;
            work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->burstWaveBody.flags   |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            break;
        case ACTOR_04000_DEATH_WAVE_MIDDLE_TICK:
            work->burstWaveBody.radius = ACTOR_04000_BURST_WAVE_MIDDLE_RADIUS;
            break;
        case ACTOR_04000_DEATH_WAVE_FULL_TICK:
            work->burstWaveBody.radius = ACTOR_04000_BURST_WAVE_END_RADIUS;
            break;
        case ACTOR_04000_DEATH_WAVE_END_TICK:
            work->burstWaveBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            if (work->airborne == 0) {
                effectSpawn(EFFECT_RED_GROUND_GLOW, task->extra.tmd->coords, 0, NULL);
            }
            break;
        case ACTOR_04000_DEATH_HIDE_MODEL_TICK:
            model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case ACTOR_04000_DEATH_SKIP_BUFFER_TICK:
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_04000_DEATH_HIDDEN_TICK:
            work->state = ACTOR_04000_STATE_HIDDEN;
            break;
    }
    if (work->stateFrame >= ACTOR_04000_DEATH_FADE_TICK && work->stateFrame < ACTOR_04000_DEATH_SCALE_END_TICK) {
        scaleQ12 = ACTOR_04000_BURST_COLOR_START_Q12 - (work->stateFrame - ACTOR_04000_DEATH_FADE_BASE_TICK) * 0x320;
        if (scaleQ12 < 0) {
            scaleQ12 = 0;
        }
        scale.vx = scale.vy = scale.vz = scaleQ12;
        work->colorMtx                 = work->savedColorMtx;
        ScaleMatrix(&work->colorMtx, &scale);
        gte_lddp(scaleQ12);
        gte_ldlvl(work->colorMtx.t);
        gte_gpf12();
        gte_stlvl(work->colorMtx.t);
        scaleQ12 = work->stateFrame * ACTOR_04000_DEATH_MODEL_GROWTH_Q12 + ONE;
        if (scaleQ12 > (2 * ONE)) {
            scaleQ12 = (2 * ONE);
        }
        _actorRenderRescaleYaw(task->extra.tmd->coords, scaleQ12);
    }
    if (work->stateFrame < ACTOR_04000_BURST_TIMER_LIMIT) {
        work->stateFrame++;
    } else {
        work->state = ACTOR_04000_STATE_HIDDEN;
    }
}

/// Spawns a player-attack hit effect on a randomly selected point of the struck side.
///
/// `hitYaw` is the signed relative yaw in 4096 units per turn; front is strictly
/// inside +/-512 and back strictly outside +/-1536. Remaining positive/negative
/// angles select right/left. Requires live work and model coordinates 0..5.
/// `attackKey` supplies the player attack's effect selector. Copies the chosen
/// local offset and part index into owned work, which must outlive the effect's
/// borrowed placement arguments. Releases the temporary scratch vector.
static void _actor04000SpawnHitEffect(Task* task, s16 hitYaw, u32 attackKey)
{
    enum {
        ACTOR_04000_HIT_FRONT_LIMIT  = 0x200,
        ACTOR_04000_HIT_BACK_LIMIT   = 0x600,
        ACTOR_04000_HIT_BACK_PART    = 1,
        ACTOR_04000_HIT_FRONT_PART   = 2,
        ACTOR_04000_HIT_LEFT_PART    = 4,
        ACTOR_04000_HIT_RIGHT_PART   = 5,
        ACTOR_04000_HIT_EFFECT_SIZE  = 0x100,
        ACTOR_04000_HIT_EFFECT_COUNT = 1 // Series count, also packed as the recipe high half
    };
    SVECTOR*         offsetAndPart;
    _Actor04000Work* work;
    s32              yawMagnitude;
    GfxCoord*        hitCoord;

    offsetAndPart = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    yawMagnitude  = (hitYaw >= 0) ? hitYaw : -hitYaw;
    work          = task->work;
    if (yawMagnitude < ACTOR_04000_HIT_FRONT_LIMIT) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 16) & 1)) {
            offsetAndPart->pad = ACTOR_04000_HIT_FRONT_PART;
            offsetAndPart->vx  = 80;
            offsetAndPart->vy  = -180;
            offsetAndPart->vz  = 330;
        } else {
            offsetAndPart->pad = ACTOR_04000_HIT_FRONT_PART;
            offsetAndPart->vx  = -60;
            offsetAndPart->vy  = -150;
            offsetAndPart->vz  = 300;
        }
    } else if (yawMagnitude > ACTOR_04000_HIT_BACK_LIMIT) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 16) & 1)) {
            offsetAndPart->pad = ACTOR_04000_HIT_BACK_PART;
            offsetAndPart->vx  = 0;
            offsetAndPart->vy  = 0;
            offsetAndPart->vz  = -180;
        } else {
            offsetAndPart->pad = ACTOR_04000_HIT_FRONT_PART;
            offsetAndPart->vx  = 2;
            offsetAndPart->vy  = -50;
            offsetAndPart->vz  = -50;
        }
    } else if (hitYaw > 0) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 16) & 1)) {
            offsetAndPart->pad = ACTOR_04000_HIT_RIGHT_PART;
            offsetAndPart->vx  = 100;
            offsetAndPart->vy  = 0;
            offsetAndPart->vz  = 0;
        } else {
            offsetAndPart->pad = ACTOR_04000_HIT_RIGHT_PART;
            offsetAndPart->vx  = 120;
            offsetAndPart->vy  = 0;
            offsetAndPart->vz  = 100;
        }
    } else {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 16) & 1)) {
            offsetAndPart->pad = ACTOR_04000_HIT_LEFT_PART;
            offsetAndPart->vx  = -100;
            offsetAndPart->vy  = 0;
            offsetAndPart->vz  = 0;
        } else {
            offsetAndPart->pad = ACTOR_04000_HIT_LEFT_PART;
            offsetAndPart->vx  = -120;
            offsetAndPart->vy  = 0;
            offsetAndPart->vz  = 100;
        }
    }
    // Copy the temporary placement into work retained by the spawned hit effect.
    work->hitEffectOffset         = *offsetAndPart;
    hitCoord                      = &task->extra.tmd->coords[offsetAndPart->pad];
    work->hitEffectArg.spawnArgLo = ACTOR_04000_HIT_EFFECT_SIZE;
    work->hitEffectArg.spawnArgHi = ACTOR_04000_HIT_EFFECT_COUNT;
    work->hitEffectArg.coord      = hitCoord;
    effectSpawnHit(damageGetPlayerAttackEffectId(attackKey), &task->extra.tmd->coords[offsetAndPart->pad], &work->hitEffectOffset, &work->hitEffectArg);
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

/// Returns the first attack contact key and copies its world position.
///
/// Borrows at most `contactCount` elements (a nonnegative signed-halfword count),
/// stopping at the first zero key. Returns 0 if no attack is found, leaving
/// `hitPos` untouched; a hit copies XYZ only, preserving its unused pad halfword.
static __inline__ s32 _actor04000FindAttackContactKey(SVECTOR* hitPos, const WorldCollisionContact* contacts, s16 contactCount)
{
    s16 contactIndex;

    for (contactIndex = 0; contactIndex < contactCount; contactIndex++) {
        if (!contacts[contactIndex].key.value)
            break;
        if ((contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_ATTACK) {
            hitPos->vx = contacts[contactIndex].point.vx;
            hitPos->vy = contacts[contactIndex].point.vy;
            hitPos->vz = contacts[contactIndex].point.vz;
            return contacts[contactIndex].key.value;
        }
    }
    return 0;
}

/// Composes the root and records the attack contact's relative hit-effect bearing.
///
/// Requires live model/scratch and a populated hitPos. Coordinate and offset
/// components narrow to signed halfwords; yaw uses 4096 units per turn and is
/// wrapped to [-2048, 2048], retaining both half-turn endpoints.
static inline void _actor04000ComputeHitBearing(Task* task, ActorHitTakenScratch* hitScratch)
{
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
    hitScratch->hitOffset.vx = task->extra.tmd->coords->workm.t[0];
    hitScratch->hitOffset.vy = task->extra.tmd->coords->workm.t[1];
    hitScratch->hitOffset.vz = task->extra.tmd->coords->workm.t[2];
    // Orient the hit effect from the contact bearing in the composed root frame.
    hitScratch->hitOffset.vx = hitScratch->hitPos.vx - task->extra.tmd->coords->workm.t[0];
    hitScratch->hitOffset.vy = hitScratch->hitPos.vy - task->extra.tmd->coords->workm.t[1];
    hitScratch->hitOffset.vz = hitScratch->hitPos.vz - task->extra.tmd->coords->workm.t[2];
    hitScratch->hitYaw       = ratan2(hitScratch->hitOffset.vx, hitScratch->hitOffset.vz) -
                         ratan2(-task->extra.tmd->coords->workm.m[2][0], task->extra.tmd->coords->workm.m[2][2]);
    hitScratch->hitYaw = _actorAngleNormalizeYaw(hitScratch->hitYaw);
}

/// Applies the first attack contact and releases any player held by the Blood Suckler.
///
/// Requires live enemy/model/work and available hit scratch storage. Processes
/// at most one attack in the eight-entry hit table, stopping at its first empty
/// key. Uses zero attack distance with reaction scaling disabled. Damage narrows
/// to an unsigned halfword before Life Drain/readout credit and HP subtraction;
/// nonpositive remaining HP selects death burst.
/// The effect uses a signed relative bearing in 4096-unit yaw; the model's
/// rotation is preserved. A held scripted player is released after any hit.
static void _actor04000ApplyAttackHit(Enemy* enemy, Task* task)
{
    enum { ACTOR_04000_HIT_SOUND             = SOUND_CHARACTER(0x28, 3),
           ACTOR_04000_UNUSED_REACTION_SCALE = 0x1000 };
    ActorHitTakenScratch* hitScratch;
    _Actor04000Work*      work;
    s32                   soundId;
    s32                   soundPan;

    work               = task->work;
    hitScratch         = SCRATCH_STACK_RESERVE_BLOCK(ActorHitTakenScratch);
    hitScratch->hitKey = _actor04000FindAttackContactKey(&hitScratch->hitPos, work->hitContacts, ARRAY_SIZE(work->hitContacts));

    if (hitScratch->hitKey != 0) {
        hitScratch->damage = damageComputePlayerAttack(hitScratch->hitKey, 0, 0, ACTOR_04000_UNUSED_REACTION_SCALE);
        _actor04000ComputeHitBearing(task, hitScratch);
        _actor04000SpawnHitEffect(task, hitScratch->hitYaw, hitScratch->hitKey);
        soundId  = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_04000_HIT_SOUND;
        soundPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        damageAccumulateLifeDrainHp(enemy, hitScratch->hitKey, hitScratch->damage, 0);
        worldTargetAddReadoutAmount(&enemy->node, hitScratch->damage, 0);
        enemy->hp -= hitScratch->damage;
        if (enemy->hp <= 0) {
            work->state = ACTOR_04000_STATE_DEATH_BURST;
        }
        if (work->holdingPlayer == 1) {
            if (((GameActor*)gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work)->mode == GAME_ACTOR_MODE_SCRIPTED) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            }
            work->holdingPlayer = 0;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorHitTakenScratch);
}

/// Walks between two patrol points and switches to chase when the player is noticed.
///
/// Turns by at most 32 of 4096 yaw units per tick. Arrival within 400 X/Z
/// units or 97 accumulated blocked ticks swaps endpoints. Loop jumps after
/// the twentieth also have a one-in-eight chance of returning to `SETTLE`.
static void _actor04000StatePatrol(Enemy* enemy, Task* task)
{
    enum { ACTOR_04000_PATROL_YAW_STEP = 32 };

    _Actor04000Work*  work;
    ActorTurnScratch* scratchHead;
    ActorTurnScratch* turnScratch;
    GfxCoord*         facingCoord;
    GfxCoord*         rangeCoord;
    TmdObject*        model;
    s16               yawDelta;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        work->driver.requestedSet     = ACTOR_04000_ANIMATION_WALK;
        work->driver.state            = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias         = 0;
        work->patrolIndex             = 0;
        work->hitBody.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->burstAttackBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _animDriverTick(task);
        work->stateFrame = 0;
        return;
    }
    scratchHead              = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    turnScratch              = SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    scratchHead[-1].delta.vx = work->patrolPoints[work->patrolIndex].vx - task->extra.tmd->coords->coord.t[0];
    turnScratch->delta.vy    = 0;
    turnScratch->delta.vz    = work->patrolPoints[work->patrolIndex].vz - task->extra.tmd->coords->coord.t[2];
    facingCoord              = task->extra.tmd->coords;
    yawDelta                 = ratan2(scratchHead[-1].delta.vx, turnScratch->delta.vz) - ratan2(-facingCoord->coord.m[2][0], facingCoord->coord.m[2][2]);
    turnScratch->angle       = _actorAngleNormalizeYaw(yawDelta);
    ACTOR_04000_APPLY_LIMITED_YAW_TURN(task, turnScratch, ACTOR_04000_PATROL_YAW_STEP);
    _actorMovementTranslateForwardNonzero(task->extra.tmd->coords, 5);
    if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts))) {
        work->stateFrame++;
    }
    if (!_actorRangeOutsideRadiusXZ(&turnScratch->delta, 400) || work->stateFrame > 0x60) {
        if (work->patrolIndex == 0) {
            work->patrolIndex = 1;
        } else {
            work->patrolIndex = 0;
        }
        work->stateFrame = 0;
    }
    _actorContactApplyAvoidancePushback(task->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts), &turnScratch->delta);
    rangeCoord            = task->extra.tmd->coords;
    turnScratch->delta.vx = gPlayerStatus.coordMtx->t[0] - rangeCoord->coord.t[0];
    turnScratch->delta.vy = gPlayerStatus.coordMtx->t[1] - rangeCoord->coord.t[1];
    turnScratch->delta.vz = gPlayerStatus.coordMtx->t[2] - rangeCoord->coord.t[2];
    if (!_actorRangeOutsideRadiusXZ(&turnScratch->delta, 2000)) {
        facingCoord = task->extra.tmd->coords;
        yawDelta    = ratan2(turnScratch->delta.vx, turnScratch->delta.vz) - ratan2(-facingCoord->coord.m[2][0], facingCoord->coord.m[2][2]);
        // Negative wrapped turns also meet this one-sided bearing test.
        if (_actorAngleNormalizeYaw(yawDelta) < ACTOR_TRANSFORM_ANGLE_TURN / 4 || !_actorRangeOutsideRadiusXZ(&turnScratch->delta, 1000)) {
            work->state = ACTOR_04000_STATE_CHASE;
        }
    }
    _animDriverTick(task);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if ((work->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP) && work->driver.jumpCount > 0x14) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 0x10) & 7)) {
            work->state = ACTOR_04000_STATE_SETTLE;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

/// Walks back to the spawn point, settling on arrival or resuming chase.
///
/// Turns by at most 16 of 4096 yaw units per tick. Arrival within 80 X/Z
/// units requests `SETTLE`; the later player test can override it with `CHASE`.
static void _actor04000StateReturn(Enemy* enemy, Task* task)
{
    enum { ACTOR_04000_RETURN_YAW_STEP = 16 };

    _Actor04000Work*  work;
    ActorTurnScratch* scratchHead;
    ActorTurnScratch* turnScratch;
    GfxCoord*         facingCoord;
    GfxCoord*         rangeCoord;
    TmdObject*        model;
    s16               yawDelta;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        work->driver.requestedSet     = ACTOR_04000_ANIMATION_WALK;
        work->driver.state            = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias         = 0;
        work->hitBody.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->burstAttackBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _animDriverTick(task);
        work->chaseFarFrames = 0;
        return;
    }
    scratchHead = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    turnScratch = SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    _animDriverTick(task);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    scratchHead[-1].delta.vx              = work->spawnPos.vx - task->extra.tmd->coords->coord.t[0];
    turnScratch->delta.vy                 = 0;
    turnScratch->delta.vz                 = work->spawnPos.vz - task->extra.tmd->coords->coord.t[2];
    facingCoord                           = task->extra.tmd->coords;
    yawDelta                              = ratan2(scratchHead[-1].delta.vx, turnScratch->delta.vz) - ratan2(-facingCoord->coord.m[2][0], facingCoord->coord.m[2][2]);
    turnScratch->angle                    = _actorAngleNormalizeYaw(yawDelta);
    ACTOR_04000_APPLY_LIMITED_YAW_TURN(task, turnScratch, ACTOR_04000_RETURN_YAW_STEP);
    _actorMovementTranslateForwardNonzero(task->extra.tmd->coords, 8);
    _actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    if (!_actorRangeOutsideRadiusXZ(&turnScratch->delta, 80)) {
        work->state = ACTOR_04000_STATE_SETTLE;
    }
    _actorContactApplyAvoidancePushback(task->extra.tmd->coords, work->hitContacts, ARRAY_SIZE(work->hitContacts), &turnScratch->delta);
    rangeCoord            = task->extra.tmd->coords;
    turnScratch->delta.vx = gPlayerStatus.coordMtx->t[0] - rangeCoord->coord.t[0];
    turnScratch->delta.vy = gPlayerStatus.coordMtx->t[1] - rangeCoord->coord.t[1];
    turnScratch->delta.vz = gPlayerStatus.coordMtx->t[2] - rangeCoord->coord.t[2];
    if (!_actorRangeOutsideRadiusXZ(&turnScratch->delta, 2000)) {
        facingCoord = task->extra.tmd->coords;
        yawDelta    = ratan2(turnScratch->delta.vx, turnScratch->delta.vz) - ratan2(-facingCoord->coord.m[2][0], facingCoord->coord.m[2][2]);
        // Negative wrapped turns also meet this one-sided bearing test.
        if (_actorAngleNormalizeYaw(yawDelta) < ACTOR_TRANSFORM_ANGLE_TURN / 4 || !_actorRangeOutsideRadiusXZ(&turnScratch->delta, 1000)) {
            work->state = ACTOR_04000_STATE_CHASE;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

#undef ACTOR_04000_APPLY_LIMITED_YAW_TURN

/// Drops a hanging actor to the floor while unwinding its upside-down roll.
///
/// Waits 50 ticks, then accelerates downward and removes at most 146 of
/// 4096 roll units per tick. Landing starts `ROUSE` and, in the nighttime
/// toilet, requests the placement-specific impact sound.
static void _actor04000StateDrop(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_04000_DROP_WAIT_TICKS  = 50,
        ACTOR_04000_DROP_ROLL_STEP   = 0x92,
        ACTOR_04000_DROP_SOUND_ENTRY = 6,
    };

    _Actor04000Work* work;
    s32              soundId;
    s32              panOffset;
    s32              remainingRoll;
    s16              rollStep;

    work = task->work;
    if (work->stateEntered != 0) {
        task->extra.tmd->flags       = 0;
        work->driver.requestedSet    = ACTOR_04000_ANIMATION_IDLE;
        work->driver.state           = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias        = 0;
        work->hitBody.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _animDriverTick(task);
        ratan2(-task->extra.tmd->coords->coord.m[2][0], task->extra.tmd->coords->coord.m[2][2]);
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->fallAccel                       = 10;
        work->fallSpeed                       = 0;
        work->stateFrame                      = 0;
        work->fallRoll                        = ACTOR_TRANSFORM_ANGLE_HALF_TURN;
        work->airborne                        = 1;
        return;
    }
    if (work->stateFrame < ACTOR_04000_DROP_WAIT_TICKS) {
        work->stateFrame++;
        return;
    }
    work->fallAccel                     += 4;
    work->fallSpeed                     += work->fallAccel;
    task->extra.tmd->coords->coord.t[1] += work->fallSpeed;
    if (task->extra.tmd->coords->coord.t[1] >= -0x12B) {
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_TOILET, 0, 0)) {
            soundId   = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_TOILET, ACTOR_04000_DROP_SOUND_ENTRY);
            panOffset = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, panOffset, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        }
        task->extra.tmd->coords->coord.t[1] = 0;
        work->state                         = ACTOR_04000_STATE_ROUSE;
        work->airborne                      = 0;
        gfxRotMatrixZ(&task->extra.tmd->coords->coord, -work->fallRoll, GRAPHICS_ROTATION_COMPOSE);
    } else {
        rollStep      = work->fallRoll;
        remainingRoll = rollStep;
        if (remainingRoll != 0) {
            rollStep = -rollStep;
            if (abs(remainingRoll) > ACTOR_04000_DROP_ROLL_STEP) {
                rollStep = (remainingRoll < 0) ? ACTOR_04000_DROP_ROLL_STEP : -ACTOR_04000_DROP_ROLL_STEP;
            }
            gfxRotMatrixZ(&task->extra.tmd->coords->coord, rollStep, GRAPHICS_ROTATION_COMPOSE);
            work->fallRoll += rollStep;
        }
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _animDriverTick(task);
}

/// Thrashes around the saloon entry point with fixed hit collision.
///
/// Repeats a 32-tick translation and playback-rate pattern while rebuilding yaw
/// and adding pitch. The hit sphere remains at (-940, -240, 5740) in the common
/// view frame. Mapped view 5 makes the enemy not lockable and clears its current
/// lock-on; other views restore its target flags. The actor remains airborne.
static void _actor04000StateScriptedThrash(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_04000_THRASH_PERIOD_TICKS      = 32,
        ACTOR_04000_THRASH_NOT_LOCKABLE_VIEW = 5,
    };
    _Actor04000Work* work;

    work = task->work;
    if (work->stateEntered != 0) {
        task->extra.tmd->flags        = 0;
        work->hitBody.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->burstAttackBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->driver.requestedSet     = ACTOR_04000_ANIMATION_RUN;
        work->driver.state            = ANIM_DRIVER_STATE_RESTART_2;
        work->driver.rateBias         = 0;
        _animDriverTick(task);
        gfxRotMatrixX(&task->extra.tmd->coords->coord, ACTOR_TRANSFORM_ANGLE_TURN / 4, GRAPHICS_ROTATION_COMPOSE);
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->airborne                        = 1;
        // Anchor hit collision at the scripted point while the root thrashes.
        work->hitBody.coord  = &gGfxViewCoord;
        work->hitBody.pos.vx = -0x3AC;
        work->hitBody.pos.vy = -0xF0;
        work->stateFrame     = 0;
        work->hitBody.pos.vz = 0x166C;
        return;
    }
    // Signed halfword wrap and signed remainder are part of the repeated motion.
    switch (++work->stateFrame % ACTOR_04000_THRASH_PERIOD_TICKS) {
        case 0:
            work->driver.rate                   = 4 * ANIMATION_RATE_ONE;
            task->extra.tmd->coords->coord.t[0] = -0x3AC;
            task->extra.tmd->coords->coord.t[1] = -0xF0;
            task->extra.tmd->coords->coord.t[2] = 0x166C;
            break;
        case 1:
        case 2:
        case 4:
        case 5:
        case 7:
        case 8:
            _actorMovementTranslateForwardNonzero(task->extra.tmd->coords, -0x78);
            break;
        case 11:
        case 12:
        case 14:
            _actorMovementTranslateForwardNonzero(task->extra.tmd->coords, 0xC8);
            break;
        case 17:
            work->driver.rate = ANIMATION_RATE_ONE;
            break;
        case 25:
            work->driver.rate = ANIMATION_RATE_ONE / 2;
            break;
    }
    gfxRotMatrixY(&task->extra.tmd->coords->coord, 0x44C, GRAPHICS_ROTATION_REPLACE);
    gfxRotMatrixX(&task->extra.tmd->coords->coord, 0x190, GRAPHICS_ROTATION_COMPOSE);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _animDriverTick(task);
    if ((u8)viewGetMappedIndex() == ACTOR_04000_THRASH_NOT_LOCKABLE_VIEW) {
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        worldTargetDisableNodeLockOn(&enemy->node);
        return;
    }
    enemy->node.state.parts.flags = 0;
}

/// Starts a scripted airborne entry with zero speed and timer.
///
/// Borrows the live enemy work, sets downward acceleration to ten coordinate
/// units per tick squared and marks it airborne. Also clears the entry's unread
/// work word, whose role is unproven. The caller chooses and primes animation
/// and controls later integration; this helper does not move the model.
static __inline__ void _actor04000InitScriptedAirborneMotion(_Actor04000Work* work)
{
    enum { ACTOR_04000_SCRIPTED_INITIAL_FALL_ACCEL = 10 };

    work->airborne   = 1;
    work->fallAccel  = ACTOR_04000_SCRIPTED_INITIAL_FALL_ACCEL;
    work->fallSpeed  = 0;
    work->stateFrame = 0;
    work->field_8    = 0;
}

/// Runs the scripted fall, pause and sideways tumble before starting chase.
///
/// Primes and holds animation 12 while falling to Y=0, then plays its hop.
/// Animation 16 waits 80 ticks for even placements; odd placements advance
/// immediately. Animation 17 slides along the saved X axis and ends in `CHASE`.
static void _actor04000StateScriptedFall(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_04000_SCRIPTED_FALL_PAUSE_CHECK_TICKS = 20,
        ACTOR_04000_SCRIPTED_FALL_PAUSE_TICKS       = 80,
    };
    _Actor04000Work* work;
    GfxCoord*        rootCoord;
    SVECTOR          slideStep;
    s32              height;
    s16              pauseTicks;

    work = task->work;
    if (work->stateEntered != 0) {
        task->extra.tmd->flags        = 0;
        work->hitBody.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->burstAttackBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = 0;
        work->driver.requestedSet     = ACTOR_04000_ANIMATION_SCRIPTED_FALL;
        work->driver.state            = ANIM_DRIVER_STATE_RESTART_2;
        work->driver.rateBias         = 0;
        work->driver.rate             = ACTOR_04000_ANIMATION_PRIME_RATE;
        _animDriverTick(task);
        _animDriverTick(task);
        work->driver.rate                     = 0;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->stateFrame                      = 0;
        _actor04000InitScriptedAirborneMotion(work);
        gfxReadMatrixXAxis(&task->extra.tmd->coords->coord, &work->slideDir);
        VectorNormalSS(&work->slideDir, &work->slideDir);
    }
    work->stateFrame++;
    // Animation selection also identifies the scripted movement phase.
    switch (work->driver.requestedSet) {
        case ACTOR_04000_ANIMATION_SCRIPTED_FALL:
            if (work->driver.rate == 0) {
                work->fallAccel += 2;
                work->fallSpeed += work->fallAccel;
                rootCoord        = task->extra.tmd->coords;
                height           = rootCoord->coord.t[1];
                if (height >= 0 || (height < 0 ? -height : height) < work->fallSpeed) {
                    rootCoord->coord.t[1] = 0;
                    work->driver.rate     = ANIMATION_RATE_ONE;
                    work->stateFrame      = 0;
                } else {
                    rootCoord->coord.t[1] = height + work->fallSpeed;
                }
                task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                return;
            }
            _animDriverTick(task);
            if (work->stateFrame < 0xA) {
                _actorMovementTranslateForwardNonzero(task->extra.tmd->coords, 0x23);
            }
            if (work->stateFrame < 4) {
                task->extra.tmd->coords->coord.t[1] -= 0x67;
            }
            if ((u32)((u16)work->stateFrame - 4) < 8) {
                task->extra.tmd->coords->coord.t[1]  -= 0xB;
                task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                gfxRotMatrixX(&task->extra.tmd->coords->coord, -0x100, GRAPHICS_ROTATION_COMPOSE);
            }
            if (work->stateFrame == 0xC) {
                work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
                work->driver.requestedSet = ACTOR_04000_ANIMATION_SCRIPTED_PAUSE;
                work->stateFrame          = 0;
                work->driver.rate         = ANIMATION_RATE_ONE;
            }
            break;
        case ACTOR_04000_ANIMATION_SCRIPTED_PAUSE:
            _animDriverTick(task);
            if (!((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) & 1)) {
                // Keep both checkpoints: the binary performs both comparisons.
                pauseTicks = work->stateFrame;
                if (pauseTicks < ACTOR_04000_SCRIPTED_FALL_PAUSE_CHECK_TICKS) {
                    break;
                }
                if (pauseTicks < ACTOR_04000_SCRIPTED_FALL_PAUSE_TICKS) {
                    break;
                }
            }
            work->driver.requestedSet = ACTOR_04000_ANIMATION_SCRIPTED_SLIDE;
            work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
            work->stateFrame          = 0;
            break;
        case ACTOR_04000_ANIMATION_SCRIPTED_SLIDE:
            _animDriverTick(task);
            if ((u32)((u16)work->stateFrame - 0xD) < 0x10) {
                task->extra.tmd->coords->coord.t[1] += 0xD;
                slideStep                            = work->slideDir;
                gte_lddp(0x14);
                gte_ldsv(&slideStep);
                gte_gpf12();
                gte_stsv(&slideStep);
                task->extra.tmd->coords->coord.t[0] += slideStep.vx;
                task->extra.tmd->coords->coord.t[2] += slideStep.vz;
                gfxRotMatrixZ(&task->extra.tmd->coords->coord, -0x88, GRAPHICS_ROTATION_COMPOSE);
                task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            }
            if (work->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
                work->state    = ACTOR_04000_STATE_CHASE;
                work->airborne = 0;
            }
            break;
    }
}

/// Plays the scripted entry leap before handing off to the scripted fall.
///
/// Advances animation 11 with forward steps in three time bands, adding
/// downward motion in the last band; tick 49 selects `SCRIPTED_FALL`.
static void _actor04000StateScriptedLeap(Enemy* enemy, Task* task)
{
    _Actor04000Work* work;

    work = task->work;
    if (work->stateEntered != 0) {
        task->extra.tmd->flags        = 0;
        work->hitBody.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->burstAttackBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = 0;
        work->driver.requestedSet     = ACTOR_04000_ANIMATION_SCRIPTED_LEAP;
        work->driver.state            = ANIM_DRIVER_STATE_RESTART_2;
        work->driver.rateBias         = 0;
        work->driver.rate             = ACTOR_04000_ANIMATION_PRIME_RATE;
        _animDriverTick(task);
        work->driver.rate                     = ANIMATION_RATE_ONE;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->stateFrame                      = 0;
        _actor04000InitScriptedAirborneMotion(work);
        return;
    }
    work->stateFrame++;
    _animDriverTick(task);
    if (work->stateFrame >= 0x13 && work->stateFrame < 0x23) {
        _actorMovementTranslateForwardNonzero(task->extra.tmd->coords, 4);
    }
    if (work->stateFrame >= 0x23 && work->stateFrame < 0x28) {
        _actorMovementTranslateForwardNonzero(task->extra.tmd->coords, 0xC);
    }
    if (work->stateFrame >= 0x28 && work->stateFrame < 0x31) {
        _actorMovementTranslateForwardNonzero(task->extra.tmd->coords, 0x18);
        task->extra.tmd->coords->coord.t[1] += 0x28;
    }
    if (work->stateFrame > 0x30) {
        work->state = ACTOR_04000_STATE_SCRIPTED_FALL;
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

static const _Actor04000StateTable Actor04000_D001F4 = {
    {
        _actor04000StateHidden,
        _actor04000StateSettle,
        _actor04000StateIdle,
        _actor04000StateRouse,
        _actor04000StateChase,
        _actor04000StateSelfBurst,
        _actor04000StateDeathBurst,
        _actor04000StatePatrol,
        _actor04000StateReturn,
        _actor04000StateAwaitBattle,
        _actor04000StateLunge,
        _actor04000StateLatched,
        _actor04000StateLungeRecover,
        _actor04000StateReleaseBurst,
        _actor04000StateHang,
        _actor04000StateDrop,
        _actor04000StateScriptedThrash,
        _actor04000StateScriptedFall,
        _actor04000StateScriptedLeap,
    }
};

/// Consumes this tick's grid, hit and burst-attack contacts without unlinking bodies.
static __inline__ void _actor04000ClearTickContacts(_Actor04000Work* work)
{
    worldCollisionClearContacts(work->gridContacts);
    worldCollisionClearContacts(work->hitContacts);
    worldCollisionClearContacts(work->burstAttackContacts);
}

/// Ends scripted control of the held player before clearing the actor's hold flag.
///
/// Requires live actor work and the current player task. Each synchronous
/// message uses a fresh slot lookup, so no player task/work pointer survives it.
static __inline__ void _actor04000ReleaseTickHold(_Actor04000Work* work)
{
    if (((GameActor*)gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work)->mode == GAME_ACTOR_MODE_SCRIPTED) {
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
    }
    work->holdingPlayer = 0;
}

/// Updates the hanging actor's behavior, player hold, combat and presentation for one frame.
///
/// Requires initialized work with a state in the nineteen-entry handler table.
/// Paused/hidden actors consume contacts and return without behavior or hold/hit
/// processing. Running actors draw state-dependent ground shadows, detect entry
/// and dispatch behavior. A held player is released when its animation stops or
/// the enemy's HP is negative; scripted control ends synchronously before the hold
/// flag clears. Hits may stage death. Audio samples pan before depth, and view
/// readiness marks the composed root dirty for its next use.
static void _actor04000Tick(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_04000_ACTIVE_SHADOW_HALF_SIZE = 256,
        ACTOR_04000_DROP_SHADOW_HALF_SIZE   = 96,
        ACTOR_04000_PAUSED_SHADOW_HALF_SIZE = 384,
        ACTOR_04000_SOUND_PLACEMENT_SHIFT   = 8
    };

    VECTOR                worldPosition;
    u8                    unusedFrameStorage[8]; // Unaccessed frame storage; its purpose is unproven.
    _Actor04000StateTable stateHandlers;
    GfxCoord              groundCoord;
    _Actor04000Work*      work;
    s32                   soundKey;
    s32                   soundPan;
    s32                   soundCue;

    work                                  = task->work;
    stateHandlers                         = Actor04000_D001F4;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
    worldPosition.vx = task->extra.tmd->coords->workm.t[0];
    worldPosition.vy = task->extra.tmd->coords->workm.t[1];
    worldPosition.vz = task->extra.tmd->coords->workm.t[2];
    worldCoordUpdateActorColor(enemy, &worldPosition, 0, 0);
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->state != ACTOR_04000_STATE_HIDDEN && work->state != ACTOR_04000_STATE_DEATH_BURST && work->state != ACTOR_04000_STATE_SELF_BURST && work->state != ACTOR_04000_STATE_RELEASE_BURST &&
                work->state != ACTOR_04000_STATE_DROP && work->state != ACTOR_04000_STATE_SCRIPTED_THRASH && work->state != ACTOR_04000_STATE_SCRIPTED_FALL) {
                task->extra.tmd->flags = 0;
                effectDrawGroundShadow(MATRIX_TRANS(&task->extra.tmd->coords->workm), ACTOR_04000_ACTIVE_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
            }
            if (work->state == ACTOR_04000_STATE_DROP) {
                gfxSetRotIdentity(&groundCoord.coord);
                groundCoord.coord.t[0]   = task->extra.tmd->coords->coord.t[0];
                groundCoord.coord.t[1]   = 0;
                groundCoord.coord.t[2]   = task->extra.tmd->coords->coord.t[2];
                groundCoord.parent       = &gGfxViewCoord;
                groundCoord.composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(&groundCoord);
                effectDrawGroundShadow(MATRIX_TRANS(&groundCoord.workm), ACTOR_04000_DROP_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            if (work->state != ACTOR_04000_STATE_HIDDEN && work->state != ACTOR_04000_STATE_DEATH_BURST && work->state != ACTOR_04000_STATE_RELEASE_BURST && work->state != ACTOR_04000_STATE_SELF_BURST &&
                work->state != ACTOR_04000_STATE_DROP && work->state != ACTOR_04000_STATE_SCRIPTED_THRASH && work->state != ACTOR_04000_STATE_SCRIPTED_FALL) {
                task->extra.tmd->flags = 0;
                effectDrawGroundShadow(MATRIX_TRANS(&task->extra.tmd->coords->workm), ACTOR_04000_PAUSED_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
            }
            _actor04000ClearTickContacts(work);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            _actor04000ClearTickContacts(work);
            return;
    }
    if (work->prevState != work->state) {
        work->stateEntered = 1;
    } else {
        work->stateEntered = 0;
    }
    work->prevState = work->state;
    stateHandlers.handlers[work->state](enemy, task);
    if (work->holdingPlayer == 1) {
        if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0 || enemy->hp < 0) {
            _actor04000ReleaseTickHold(work);
        }
    }
    if (enemy->hp > 0) {
        _actor04000ApplyAttackHit(enemy, task);
        if (enemy->hp <= 0) {
            work->state = ACTOR_04000_STATE_DEATH_BURST;
        }
    }
    _actor04000ClearTickContacts(work);
    soundCue = _actor04000PollAnimationSound(work);
    if (soundCue != 0) {
        soundKey = soundCue | ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_04000_SOUND_PLACEMENT_SHIFT);
        soundPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundKey, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    if (gGameSession->viewReady != 0) {
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

/// Keeps two encounter slots filled by dropping the farthest hanging candidates.
///
/// Releases a slot when its borrowed enemy has no HP. Scores the six live
/// hanging candidates in the roots' common frame after narrowing offsets to
/// signed halfwords; strictly greater X/Z squared distance wins, so ties keep
/// the first index. No candidate advances the controller toward teardown.
static void _actor04000RefillDropSlots(Task* task)
{
    enum {
        ACTOR_04000_POOL_NO_CANDIDATE_DISTANCE = -1,
    };

    s32           squaredDistances[8];
    SVECTOR       playerOffset;
    GfxCoord*     candidateCoord;
    MATRIX*       playerMatrix;
    PlayerStatus* playerStatus;
    s32           farthestSquaredDistance;
    s16           dropSlot;
    s16           poolIndex;
    s16           selectedPoolIndex;

    selectedPoolIndex = 0;
    for (dropSlot = 0; dropSlot < ARRAY_SIZE(Actor04000_D0C710); dropSlot++) {
        if (Actor04000_D0C710[dropSlot] != NULL) {
            if (((Enemy*)Actor04000_D0C710[dropSlot]->spawnArg2.pointer)->hp <= 0) {
                Actor04000_D0C710[dropSlot] = NULL;
            }
            if (Actor04000_D0C710[dropSlot] != NULL) {
                continue;
            }
        }
        // Score each remaining hanging actor; the first farthest index wins.
        poolIndex    = 0;
        playerStatus = &gPlayerStatus;
        for (; poolIndex < ACTOR_04000_HANGING_POOL_COUNT; poolIndex++) {
            if (Actor04000_D0C718[poolIndex] != NULL && ((_Actor04000Work*)Actor04000_D0C718[poolIndex]->work)->state == ACTOR_04000_STATE_HANG) {
                candidateCoord               = Actor04000_D0C718[poolIndex]->extra.tmd->coords;
                playerMatrix                 = playerStatus->coordMtx;
                playerOffset.vx              = playerMatrix->t[0] - candidateCoord->coord.t[0];
                playerOffset.vy              = playerMatrix->t[1] - candidateCoord->coord.t[1];
                playerOffset.vz              = playerMatrix->t[2] - candidateCoord->coord.t[2];
                squaredDistances[poolIndex]  = playerOffset.vx * playerOffset.vx;
                squaredDistances[poolIndex] += playerOffset.vz * playerOffset.vz;
            } else {
                squaredDistances[poolIndex] = ACTOR_04000_POOL_NO_CANDIDATE_DISTANCE;
            }
        }
        farthestSquaredDistance = ACTOR_04000_POOL_NO_CANDIDATE_DISTANCE;
        for (poolIndex = 0; poolIndex < ACTOR_04000_HANGING_POOL_COUNT; poolIndex++) {
            if (farthestSquaredDistance < squaredDistances[poolIndex]) {
                farthestSquaredDistance = squaredDistances[poolIndex];
                selectedPoolIndex       = poolIndex;
            }
        }
        if (farthestSquaredDistance == ACTOR_04000_POOL_NO_CANDIDATE_DISTANCE) {
            task->state++;
            return;
        }
        ((_Actor04000Work*)Actor04000_D0C718[selectedPoolIndex]->work)->state = ACTOR_04000_STATE_DROP;
        Actor04000_D0C710[dropSlot]                                           = Actor04000_D0C718[selectedPoolIndex];
        Actor04000_D0C718[selectedPoolIndex]                                  = NULL;
    }
}

/// Sets model draw policy and selects the corresponding patrol or hidden state.
///
/// Modes 0/1 hide/show and select `PATROL`; mode 2 keeps the existing draw
/// bits, disables automatic buffers and selects `HIDDEN`; mode 3 clears the
/// other draw bits first. Other values do nothing. Always returns 0.
static s32 _actor04000SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg)
{
    enum {
        ACTOR_04000_DRAW_DISABLE_AUTO_BUFFER           = 2,
        ACTOR_04000_DRAW_CLEAR_AND_DISABLE_AUTO_BUFFER = 3,
    };

    _Actor04000Work* work;
    TmdObject*       model;

    model = task->extra.tmd;
    work  = task->work;

    switch (drawMode) {
        case ACTOR_MESSAGE_DRAW_HIDE:
            model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->state  = ACTOR_04000_STATE_PATROL;
            break;
        case ACTOR_MESSAGE_DRAW_SHOW:
            model->flags = 0;
            work->state  = ACTOR_04000_STATE_PATROL;
            break;
        case ACTOR_04000_DRAW_DISABLE_AUTO_BUFFER:
            model->flags = (u16)(model->flags | TMD_OBJECT_SKIP_AUTO_BUFFER);
            work->state  = ACTOR_04000_STATE_HIDDEN;
            break;
        case ACTOR_04000_DRAW_CLEAR_AND_DISABLE_AUTO_BUFFER:
            model->flags = 0;
            work->state  = ACTOR_04000_STATE_HIDDEN;
            model->flags = (u16)(model->flags | TMD_OBJECT_SKIP_AUTO_BUFFER);
            break;
        default:
            return 0;
    }
    return 0;
}

#include "../../shared/actor_messages_place.inc.c"

/// Requests a release burst when the actor is holding the player.
///
/// `ACTOR_MESSAGE_RELEASE_HOLD` has no payload. Only `LATCHED` changes to
/// `RELEASE_BURST`; the burst state releases the player later. Returns 1.
static s32 _actor04000ReleaseHold(Task* task, s32 messageId, s32 unusedPayload, s32 unusedArg)
{
    _Actor04000Work* work;

    work = task->work;
    if (work->state == ACTOR_04000_STATE_LATCHED) {
        work->state = ACTOR_04000_STATE_RELEASE_BURST;
    }
    return 1;
}

/// Queues playback from this actor's fixed animation bank and returns 1.
///
/// Borrows the request only for dispatch; `animationId` must be a loaded
/// index in 1..17 and is stored as a signed halfword. A reset blend selects
/// restart 2, other blends restart 1; the driver currently treats both alike.
/// The source bank, blend duration and collision choice are ignored.
static s32 _actor04000RequestAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg)
{
    _Actor04000Work* work = task->work;

    work->driver.requestedSet = request->animationId;
    if (request->blend == ANIMATION_BLEND_RESET) {
        work->driver.state = ANIM_DRIVER_STATE_RESTART_2;
    } else {
        work->driver.state = ANIM_DRIVER_STATE_RESTART_1;
    }
    return 1;
}

#include "../../shared/coord_math_yaw_scale.inc.c"

/// Maintains the player latch, dealing attack-0 damage every eight ticks.
///
/// Entry resets the timer and advances animation once. Subsequent ticks damage
/// at timer multiples of eight, then increment it. After tick 40, a 9999-press
/// hold request selects `SELF_BURST` on acceptance (reply 0), otherwise
/// `RELEASE_BURST`. The player hold was established by the lunge state.
static void _actor04000StateLatched(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_04000_LATCH_DAMAGE_INTERVAL_TICKS = 8,
        ACTOR_04000_LATCH_HOLD_CHECK_TICK       = 40,
        ACTOR_04000_LATCH_CONTINUED_PRESS_COUNT = 9999,
    };
    _Actor04000Work* work;
    TmdObject*       model;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        work->driver.rate             = ANIMATION_RATE_ONE;
        work->driver.rateBias         = 0;
        work->gridBody.flags          = work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED;
        _animDriverTick(task);
        work->stateFrame = 0;
        return;
    }
    _animDriverTick(task);
    if (!(work->stateFrame & (ACTOR_04000_LATCH_DAMAGE_INTERVAL_TICKS - 1))) {
        padScriptSpawnVariableMotorRamp(3, 0xFF, 8);
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, 0), 0);
    }
    work->stateFrame++;
    if (work->stateFrame > ACTOR_04000_LATCH_HOLD_CHECK_TICK) {
        work->playerButtonHold.pressCount = ACTOR_04000_LATCH_CONTINUED_PRESS_COUNT;
        if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &work->playerButtonHold, 0) == 0) {
            work->state = ACTOR_04000_STATE_SELF_BURST;
        } else {
            work->state = ACTOR_04000_STATE_RELEASE_BURST;
        }
    }
}

/// Recovers from a missed lunge, chasing again or bursting after four misses.
///
/// Restarts animation 15 and increments the signed-byte miss count on entry.
/// The animation boundary selects `CHASE`; the fourth miss overrides that
/// selection with `SELF_BURST`. Target flags are restored through the task's
/// borrowed enemy pointer, which must remain live; the callback enemy is unused.
static void _actor04000StateLungeRecover(Enemy* unusedEnemy, Task* task)
{
    enum { ACTOR_04000_MISSED_LUNGE_LIMIT = 4 };
    _Actor04000Work* work;
    TmdObject*       model;
    Enemy*           enemy;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy                         = task->spawnArg2.pointer;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        work->driver.state            = ANIM_DRIVER_STATE_RESTART_2;
        work->driver.rate             = ANIMATION_RATE_ONE;
        work->driver.rateBias         = 0;
        work->driver.requestedSet     = ACTOR_04000_ANIMATION_LUNGE_RECOVER;
        work->gridBody.flags          = work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED;
        _animDriverTick(task);
        work->stateFrame = 0;
        work->missedLunges++;
    }
    _animDriverTick(task);
    if (work->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ACTOR_04000_STATE_CHASE;
    }
    if (work->missedLunges >= ACTOR_04000_MISSED_LUNGE_LIMIT) {
        work->state = ACTOR_04000_STATE_SELF_BURST;
    }
}

/// Hides the actor and disables targeting and all four collision bodies on entry.
static void _actor04000StateHidden(Enemy* enemy, Task* task)
{
    _Actor04000Work* work;
    TmdObject*       model;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                  = model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->hitBody.flags           = work->hitBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstAttackBody.flags   = work->burstAttackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags     = work->burstWaveBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags          = work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
    }
}

/// Waits for engaged battle with collision disabled, then selects patrol.
///
/// Entry hides the model and queues animation 1. Later ticks show and permit
/// lock-on only for placement indices 6 and 7; all other placements stay hidden.
/// The battle transition changes the state for the next per-frame dispatch.
static void _actor04000StateAwaitBattle(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_04000_AWAIT_VISIBLE_PLACEMENT_A = 6,
        ACTOR_04000_AWAIT_VISIBLE_PLACEMENT_B = 7,
    };
    _Actor04000Work* work;
    TmdObject*       model;

    work  = task->work;
    model = task->extra.tmd;
    if (work->stateEntered != 0) {
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->driver.requestedSet     = ACTOR_04000_ANIMATION_HOLD;
        work->driver.state            = ANIM_DRIVER_STATE_RESTART_2;
        work->hitBody.flags          &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstAttackBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        return;
    }
    _animDriverTick(task);
    switch (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) {
        case ACTOR_04000_AWAIT_VISIBLE_PLACEMENT_A:
        case ACTOR_04000_AWAIT_VISIBLE_PLACEMENT_B:
            enemy->node.state.parts.flags = 0;
            model->flags                  = 0;
            break;
        case 3:
        case 4:
        case 5:
        default:
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            model->flags                  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
    if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
        work->state = ACTOR_04000_STATE_PATROL;
    }
}

/// Plays the settling animation, then idles at its first boundary.
///
/// Entry restores visibility, targeting and hit/grid collision, disables burst
/// bodies and restarts animation 4. Later ticks advance it until the boundary.
static void _actor04000StateSettle(Enemy* enemy, Task* task)
{
    _Actor04000Work* work;
    TmdObject*       model;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        work->driver.requestedSet     = ACTOR_04000_ANIMATION_SETTLE;
        work->driver.state            = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias         = 0;
        work->hitBody.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->burstAttackBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _animDriverTick(task);
        return;
    }
    _animDriverTick(task);
    if (work->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ACTOR_04000_STATE_IDLE;
    }
}

/// Plays the rousing animation, then patrols at its first boundary.
///
/// Entry restores visibility, targeting and hit/grid collision, disables burst
/// bodies and restarts animation 6. Later ticks advance it until the boundary.
static void _actor04000StateRouse(Enemy* enemy, Task* task)
{
    _Actor04000Work* work;
    TmdObject*       model;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        model->flags                  = 0;
        work->driver.requestedSet     = ACTOR_04000_ANIMATION_ROUSE;
        work->driver.state            = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias         = 0;
        work->hitBody.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->burstAttackBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _animDriverTick(task);
        return;
    }
    _animDriverTick(task);
    if (work->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ACTOR_04000_STATE_PATROL;
    }
}

/// Holds the actor upside down until the pool controller selects it to drop.
///
/// Entry preserves its yaw while replacing roll with a half turn. Hit/grid
/// collision remain enabled, burst bodies are disabled and animation 1 plays.
/// The target stays scanned but not lockable; the controller enables targeting
/// when battle begins and later changes the state to `DROP`.
static void _actor04000StateHang(Enemy* enemy, Task* task)
{
    _Actor04000Work* work;
    s16              hangingYaw;

    work = task->work;
    if (work->stateEntered != 0) {
        task->extra.tmd->flags        = 0;
        enemy->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
        work->driver.requestedSet     = ACTOR_04000_ANIMATION_HOLD;
        work->driver.state            = ANIM_DRIVER_STATE_RESTART_2;
        work->driver.rateBias         = 0;
        work->hitBody.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->burstAttackBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _animDriverTick(task);
        hangingYaw = ratan2(-task->extra.tmd->coords->coord.m[2][0], task->extra.tmd->coords->coord.m[2][2]);
        gfxRotMatrixZ(&task->extra.tmd->coords->coord, ACTOR_TRANSFORM_ANGLE_HALF_TURN, GRAPHICS_ROTATION_REPLACE);
        gfxRotMatrixY(&task->extra.tmd->coords->coord, hangingYaw, GRAPHICS_ROTATION_COMPOSE);
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        return;
    }
    _animDriverTick(task);
}

/// The enemy's spawn, per-frame and teardown handlers, indexed by the task's
/// state.
static const EnemyTaskFuncTable3 Actor04000_D00240 = {
    _actor04000SpawnEnemy,
    _actor04000Tick,
    enemyDestroy,
};

/// Dispatches the enemy task's spawn, update or teardown phase.
///
/// `task->state` must be 0 (spawn), 1 (update) or 2 (destroy). The descriptor
/// provides a live TMD model and `spawnArg2.pointer` borrows the live `Enemy`.
/// Copies the three callbacks to the stack before dispatch; no index is checked.
static void _actor04000Task(Task* task)
{
    EnemyTaskFuncTable3 handlers;

    handlers = Actor04000_D00240;
    handlers.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Clears the encounter slots and broadcasts the nighttime toilet hanging command.
///
/// Only the six candidate entries are cleared; the other two placement entries
/// are untouched. The stack command is borrowed by synchronous dispatch, then
/// the controller advances to its battle gate.
static void _actor04000InitHangingPool(Task* task)
{
    ActorCommand command;
    s16          poolIndex;

    for (poolIndex = 0; poolIndex < ACTOR_04000_HANGING_POOL_COUNT; poolIndex++) {
        Actor04000_D0C718[poolIndex] = NULL;
    }
    Actor04000_D0C710[1]      = NULL;
    command.context.loc.stage = GAME_STAGE_DRYFIELD_NIGHT;
    command.context.loc.area  = GAME_AREA_DRYFIELD_NIGHT_TOILET;
    Actor04000_D0C710[0]      = NULL;
    command.command           = ACTOR_04000_POOL_COMMAND_HANG;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &command, ACTOR_COMMAND_MESSAGE_APPLY);
    task->state++;
}

/// Waits for battle before making the pooled actors lockable and enabling drops.
///
/// View 5 engages battle while any candidate remains. The engaged-battle
/// check precedes that trigger, so a newly triggered battle advances on the
/// next controller tick.
static void _actor04000AwaitPoolBattle(Task* task)
{
    enum { ACTOR_04000_POOL_BATTLE_VIEW = 5 };
    s16 poolIndex;

    if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
        for (poolIndex = 0; poolIndex < ACTOR_04000_HANGING_POOL_COUNT; poolIndex++) {
            if (Actor04000_D0C718[poolIndex] != NULL) {
                ((Enemy*)Actor04000_D0C718[poolIndex]->spawnArg2.pointer)->node.state.parts.flags = 0;
            }
        }
        task->state++;
    }
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view == ACTOR_04000_POOL_BATTLE_VIEW) {
        for (poolIndex = 0; poolIndex < ACTOR_04000_HANGING_POOL_COUNT; poolIndex++) {
            if (Actor04000_D0C718[poolIndex] != NULL) {
                sceneEngageBattle(1);
                return;
            }
        }
    }
}

/// Dispatches the hanging-pool controller's encounter phase.
///
/// `task->state` must be 0 (initialize), 1 (await battle), 2 (refill drop slots)
/// or 3 (kill). The descriptor supplies a bodyless task; the table and borrowed
/// actor handles belong to this overlay. Dispatch performs no index check.
static void _actor04000HangingPoolTask(Task* task)
{
    Actor04000_D0C6EC[task->state](task);
}
