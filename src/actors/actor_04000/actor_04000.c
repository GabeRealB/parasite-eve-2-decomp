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
#include "../../shared/coord_math.h"
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

static void Actor04000_Fn06A5C(Enemy* enemy, Task* task);
static void Actor04000_Fn06878(Enemy* arg0, Task* arg1);
static void Actor04000_Fn06994(Enemy* arg0, Task* arg1);
static void Actor04000_Fn06AC4(Enemy* arg0, Task* arg1);
static void Actor04000_Fn06BC8(Enemy* arg0, Task* arg1);
static void Actor04000_Fn06C80(Enemy* arg0, Task* arg1);
static void Actor04000_Fn06D38(Enemy* arg0, Task* arg1);

static TmdSource _gActor04000BloodSucklerBody;
void             Actor04000_Fn06380(Task*);
void             Actor04000_Fn06E4C(Task*);
void             Actor04000_Fn06EA8(Task*);
void             Actor04000_Fn06F54(Task*);
void             Actor04000_Fn0703C(Task*);

s32 Actor04000_Fn0093C(Task*, s32, ActorCommand*, s32);
s32 Actor04000_Fn06590(Task*, s32, s32, s32);
s32 Actor04000_Fn06704(Task*, s32, void*, s32);
s32 Actor04000_Fn06728(Task*, s32, AnimationPlayRequest*, s32);

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
    { ACTOR_MESSAGE_SET_MODEL_DRAW, Actor04000_Fn06590 },
    { ACTOR_MESSAGE_PLAY_ANIMATION, Actor04000_Fn06728 },
    { ACTOR_COMMAND_MESSAGE_APPLY, Actor04000_Fn0093C },
    { ACTOR_MESSAGE_PLACE, actorMsgPlace },
    { 2014, Actor04000_Fn06704 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc Actor04000_D0C6E0 = { { { TASK_BODY_TMD, 96 } }, Actor04000_Fn06E4C, { .model = &_gActor04000BloodSucklerBody } };

TaskFunc Actor04000_D0C6EC[4] = {
    Actor04000_Fn06EA8,
    Actor04000_Fn06F54,
    Actor04000_Fn06380,
    taskKill,
};

TaskDesc Actor04000_D0C6FC = { { { TASK_BODY_NONE, 96 } }, Actor04000_Fn0703C, { .value = 0 } };

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

static s32             Actor04000_Fn00FDC(_Actor04000Work* arg0);
static void            Actor04000_Fn010B8(Enemy* arg0, Task* arg1);
static void            Actor04000_Fn0168C(Enemy* arg0, Task* arg1);
static __inline__ void Actor204000_FaceScale(GfxCoord* coord, s16 s);
static void            Actor04000_Fn01E1C(Enemy* arg0, Task* arg1);
static void            Actor04000_Fn026FC(Enemy* arg0, Task* arg1);
static void            Actor04000_Fn028F0(Enemy* arg0, Task* arg1);
static void            Actor04000_Fn02F48(Enemy* arg0, Task* arg1);
static void            Actor04000_Fn03798(Enemy* arg0, Task* arg1);
static void            Actor04000_Fn03D30(Task* arg0, s16 arg1, u32 arg2);
static void            Actor04000_Fn03FB4(Enemy* arg0, Task* arg1);
static void            Actor04000_Fn0432C(Enemy* arg0, Task* arg1);
static void            Actor04000_Fn049C0(Enemy* arg0, Task* arg1);
static void            Actor04000_Fn04FA4(Enemy* arg0, Task* arg1);
static void            Actor04000_Fn0522C(Enemy* arg0, Task* arg1);
static void            Actor04000_Fn055C8(Enemy* arg0, Task* arg1);
static void            Actor04000_Fn05AE8(Enemy* arg0, Task* arg1);
static void            Actor04000_Fn05F0C(Enemy* arg0, Task* arg1);

#include "../../shared/actor_contacts_steer.inc.c"

#include "../../shared/actor_contacts_push_contact.inc.c"

/// Message handler: 0x1003/1 registers the actor in its lead slot and places
/// it at that slot's start point; 0x1203 and 0x302 move it to the scripted
/// positions for its slot and pick the next state.
s32 Actor04000_Fn0093C(Task* arg0, s32 arg1, ActorCommand* command, s32 arg3)
{
    _Actor04000Work* work;
    Enemy*           ctx;

    work = arg0->work;
    ctx  = arg0->spawnArg2.pointer;
    if (command->context.key == 0x1003 && command->command == 1) {
        work->state                                                 = ACTOR_04000_STATE_HANG;
        Actor04000_D0C718[ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT] = arg0;
        ctx->node.state.parts.flags                                 = WORLD_TARGET_NOT_LOCKABLE;
        switch (ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) {
            case 0:
                arg0->extra.tmd->coords->coord.t[0]   = 0x116;
                arg0->extra.tmd->coords->coord.t[1]   = -0xBB8;
                arg0->extra.tmd->coords->coord.t[2]   = 0x6A4;
                arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                break;
            case 1:
                arg0->extra.tmd->coords->coord.t[0]   = 0x2BC;
                arg0->extra.tmd->coords->coord.t[1]   = -0xBB8;
                arg0->extra.tmd->coords->coord.t[2]   = 0x56A;
                arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                break;
            case 2:
                arg0->extra.tmd->coords->coord.t[0]   = -0x1E;
                arg0->extra.tmd->coords->coord.t[1]   = -0xBB8;
                arg0->extra.tmd->coords->coord.t[2]   = 0x500;
                arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                break;
            case 3:
                arg0->extra.tmd->coords->coord.t[0]   = 0xB2;
                arg0->extra.tmd->coords->coord.t[1]   = -0xBB8;
                arg0->extra.tmd->coords->coord.t[2]   = 0x22E;
                arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                break;
            case 4:
                arg0->extra.tmd->coords->coord.t[0]   = 0x21E;
                arg0->extra.tmd->coords->coord.t[1]   = -0xBB8;
                arg0->extra.tmd->coords->coord.t[2]   = -0xF2;
                arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                break;
            case 5:
                arg0->extra.tmd->coords->coord.t[0]   = -0x46;
                arg0->extra.tmd->coords->coord.t[1]   = -0xBB8;
                arg0->extra.tmd->coords->coord.t[2]   = -0x20B;
                arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                break;
        }
    }
    if (command->context.key == 0x1203) {
        switch (command->command) {
            case 0:
                work->state = ACTOR_04000_STATE_HIDDEN;
                break;
            case 1:
                switch (ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) {
                    case 0:
                        work->state                         = ACTOR_04000_STATE_SCRIPTED_THRASH;
                        arg0->extra.tmd->coords->coord.t[0] = -0x3AC;
                        arg0->extra.tmd->coords->coord.t[1] = -0xF0;
                        arg0->extra.tmd->coords->coord.t[2] = 0x166C;
                        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0x3E8, 1);
                        sceneEngageBattle(1);
                        break;
                    case 1:
                        work->state                         = ACTOR_04000_STATE_SCRIPTED_FALL;
                        arg0->extra.tmd->coords->coord.t[0] = 0x2A8;
                        arg0->extra.tmd->coords->coord.t[1] = -0x7D0;
                        arg0->extra.tmd->coords->coord.t[2] = 0x189C;
                        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0x800, 1);
                        break;
                }
                break;
        }
    }
    if (command->context.key == 0x302) {
        switch (command->command) {
            case 0:
                work->state = ACTOR_04000_STATE_AWAIT_BATTLE;
                break;
            case 9:
                work->state = ACTOR_04000_STATE_HIDDEN;
                break;
            case 1:
                switch (ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) {
                    case 0:
                        arg0->extra.tmd->coords->coord.t[0]   = 0xF1E;
                        arg0->extra.tmd->coords->coord.t[1]   = -0x384;
                        arg0->extra.tmd->coords->coord.t[2]   = 0xFE6;
                        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, -0x400, 1);
                        work->state = ACTOR_04000_STATE_SCRIPTED_FALL;
                        break;
                    case 1:
                        arg0->extra.tmd->coords->coord.t[0] = 0xA1E;
                        arg0->extra.tmd->coords->coord.t[1] = -0x384;
                        arg0->extra.tmd->coords->coord.t[2] = 0x1590;
                        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0x7D0, 1);
                        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                        work->state                           = ACTOR_04000_STATE_SCRIPTED_LEAP;
                        break;
                    case 2:
                        arg0->extra.tmd->coords->coord.t[0] = 0x1A4;
                        arg0->extra.tmd->coords->coord.t[1] = -0x4C4;
                        arg0->extra.tmd->coords->coord.t[2] = 0x1194;
                        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0x3E8, 1);
                        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
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

/// Sound cue check keyed on the requested animation `driver.requestedSet`: for
/// 2 and 3, answers 0x40280001 the first time slot 1's cue index reaches one
/// of that animation's trigger indices (latched in `lastSoundCueIndex`, which
/// clears on any other index); for 5, answers 0x400C0005 on a tick slot 1
/// followed a control jump. Answers 0 otherwise.
static s32 Actor04000_Fn00FDC(_Actor04000Work* arg0)
{
    u16 id;
    s32 v;

    switch (arg0->driver.requestedSet) {
        case 2:
            id = arg0->rig.slots[ANIM_DRIVER_FIRST_SLOT].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            v  = id;
            if (v != 0x15) {
                goto not15;
            }
        check:
            if (arg0->lastSoundCueIndex == v) {
                goto same;
            }
            arg0->lastSoundCueIndex = id;
            return 0x40280001;
        not15:
            if (v == 0x11) {
                goto check;
            }
        clear:
            arg0->lastSoundCueIndex = 0;
            break;
        case 3:
            id = arg0->rig.slots[ANIM_DRIVER_FIRST_SLOT].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            v  = id;
            if (v != 0xD && v != 0x12) {
                goto clear;
            }
            goto check;
        same:
            arg0->lastSoundCueIndex = id;
            break;
        case 5:
            if (arg0->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP) {
                return 0x400C0005;
            }
            break;
    }
    return 0;
}

// animation bank handed to `animationInitContext`

/// Spawn state: allocates the work block, links the four collision objects and
/// the enemy node, seeds the size and HP from the enemy's level nibble, records
/// the spawn position and the points 1000 units ahead and behind it, then
/// starts in `IDLE` when the high half of `Task::spawnArg1` is 1 and `PATROL` otherwise.
static void Actor04000_Fn010B8(Enemy* arg0, Task* arg1)
{
    TmdObject*             obj;
    GfxCoord*              coord;
    _Actor04000Work*       work;
    WorldCollisionContact* hits;
    SVECTOR                sv;
    VECTOR                 pos;
    SVECTOR*               p;
    SVECTOR*               q;
    WorldCollisionBody*    o1;
    WorldCollisionBody*    o2;
    WorldCollisionBody*    o3;
    WorldCollisionBody*    o4;

    obj        = arg1->extra.tmd;
    coord      = obj->coords;
    work       = memCalloc(sizeof(_Actor04000Work), 0);
    arg1->work = work;
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    coord->parent   = &gGfxViewCoord;
    arg1->msgTable  = Actor04000_D0C6B0;
    work->field_180 = 0;
    work->field_184 = 1;
    work->field_18C = 3;
    work->field_188 = 0;
    work->field_190 = 1;
    obj->flags      = 0;
    animationInitContext(&work->rig.anim, Actor04000_D0C4C4, obj, work->rig.poses, work->rig.slots);

    o1                   = &work->gridBody;
    o1->coord            = arg1->extra.tmd->coords + 1;
    o1->context.contacts = work->gridContacts;
    o1->pos.vy           = -0x110;
    o1->pos.vx           = 0;
    o1->pos.vz           = 0;
    o1->key              = 0x3000C;
    o1->radius           = 0x190;
    o1->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, o1);
    o1->flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    worldCollisionInitContacts(o1->context.contacts, 8, 0);

    o2                   = &work->hitBody;
    sv.vx                = 0;
    sv.vy                = -0x168;
    sv.vz                = 0;
    p                    = &sv;
    hits                 = work->hitContacts;
    o2->coord            = arg1->extra.tmd->coords + 2;
    o2->context.contacts = hits;
    o2->pos.vx           = p->vx;
    o2->pos.vy           = p->vy;
    o2->pos.vz           = p->vz;
    o2->key              = 0x3000C;
    o2->radius           = 0x168;
    o2->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, o2);
    o2->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(o2->context.contacts, 8, 0);

    sv.vx                = 0;
    sv.vy                = 0;
    sv.vz                = 0;
    o3                   = &work->burstAttackBody;
    o3->coord            = &gGfxViewCoord;
    o3->context.contacts = work->burstAttackContacts;
    o3->pos.vx           = p->vx;
    o3->pos.vy           = p->vy;
    o3->pos.vz           = p->vz;
    o3->radius           = 0x500;
    o3->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, o3);
    worldCollisionInitContacts(o3->context.contacts, 1, 0);

    o4                   = &work->burstWaveBody;
    o4->coord            = &gGfxViewCoord;
    o4->context.contacts = work->burstWaveContacts;
    o4->pos.vx           = p->vx;
    o4->pos.vy           = p->vy;
    o4->pos.vz           = p->vz;
    o4->radius           = 0x80;
    o4->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_BLASTS, o4);
    worldCollisionInitContacts(o4->context.contacts, 1, 0);

    arg0->field_4    = &coord->coord;
    arg0->field_48   = 0;
    arg0->bodyPos.vx = 0;
    arg0->bodyPos.vy = 0;
    arg0->bodyPos.vz = 0;
    arg0->coord      = arg1->extra.tmd->coords + 2;
    worldTargetLinkNode(&arg0->node);
    arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    arg0->reactionFlags          = 0;
    arg0->hp = arg0->hpMax    = Actor04000_D07084.hpMax;
    arg0->param               = &Actor04000_D07084;
    arg0->recs                = hits;
    work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
    work->driver.requestedSet = 1;
    work->driver.rate         = ANIMATION_RATE_ONE;
    work->driver.rateBias     = 0;
    _animDriverTick(arg1);
    work->field_17E     = 0;
    work->field_A       = 0;
    obj->lightMtx       = &work->lightMtx;
    obj->colorMtx       = &work->colorMtx;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    worldCoordUpdateActorColor(arg0, &pos, 0, 0);
    work->field_1A0 = 5;
    work->field_1A2 = 0x14;
    if ((u16)(arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) % 2 == 1) {
        work->driver.rate += arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        work->field_1A2   += arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        work->field_1A0   += arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    } else {
        work->driver.rate -= (u16)(arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) / 2;
        work->field_1A2   -= (arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) / 2;
        work->field_1A0   -= (arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) / 2;
    }
    work->spawnPos.vx = arg1->extra.tmd->coords->coord.t[0];
    work->spawnPos.vy = arg1->extra.tmd->coords->coord.t[1];
    work->spawnPos.vz = arg1->extra.tmd->coords->coord.t[2];
    gfxReadMatrixZAxis(&arg1->extra.tmd->coords->coord, &sv);
    sv.vy = 0;
    q     = &sv;
    VectorNormalSS(q, q);
    gte_lddp(1000);
    gte_ldsv(q);
    gte_gpf12();
    gte_stsv(q);
    work->patrolPoints[0].vx = arg1->extra.tmd->coords->coord.t[0] + sv.vx;
    work->patrolPoints[0].vy = arg1->extra.tmd->coords->coord.t[1];
    work->patrolPoints[0].vz = arg1->extra.tmd->coords->coord.t[2] + sv.vz;
    work->patrolPoints[1].vx = arg1->extra.tmd->coords->coord.t[0] - sv.vx;
    work->patrolPoints[1].vy = arg1->extra.tmd->coords->coord.t[1];
    work->patrolPoints[1].vz = arg1->extra.tmd->coords->coord.t[2] - sv.vz;
    /* the gameplay prototype takes no argument, but this call site passes 0 */
    (sceneAcquireBattleRef)(0);
    if ((arg1->spawnArg1.value >> 16) == 0) {
        work->state = ACTOR_04000_STATE_PATROL;
    } else if ((arg1->spawnArg1.value >> 16) == 1) {
        work->state = ACTOR_04000_STATE_IDLE;
    } else {
        work->state = ACTOR_04000_STATE_PATROL;
    }
    work->prevState    = -1;
    work->airborne     = 0;
    work->missedLunges = 0;
    arg1->state++;
}

/// Lunge state: steps forward on frames 8 and 9, then from frame 9 on grabs
/// the player when within 600 units and a quarter turn of the facing, dispatches
/// the side-dependent grab message and snaps the model beside and facing them.
static void Actor04000_Fn0168C(Enemy* arg0, Task* arg1)
{
    _Actor04000Work*         work;
    Task*                    player;
    GameActor*               actor;
    TmdObject*               obj;
    _Actor04000LungeScratch* scratch;
    GfxCoord*                coord;
    GfxCoord*                pos;
    s16                      angle;
    s32                      mag;

    work   = arg1->work;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    actor  = player->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg1->extra.tmd;
        ((Enemy*)arg1->spawnArg2.pointer)->node.state.parts.flags = 0;
        sceneEngageBattle(1);
        obj->flags                = 0;
        work->driver.state        = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rate         = ANIMATION_RATE_ONE;
        work->driver.rateBias     = 0;
        work->driver.requestedSet = 0xD;
        work->gridBody.flags     |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _animDriverTick(arg1);
        work->stateFrame = 0;
        return;
    }
    _animDriverTick(arg1);
    work->stateFrame++;
    if (work->stateFrame < 8) {
        return;
    }
    if (work->stateFrame == 8) {
        actorStepForward(arg1->extra.tmd->coords, 0x32);
        return;
    }
    if (work->stateFrame == 9) {
        actorStepForward(arg1->extra.tmd->coords, 0x32);
    }
    work->state       = ACTOR_04000_STATE_LUNGE_RECOVER;
    scratch           = SCRATCH_STACK_RESERVE_BLOCK(_Actor04000LungeScratch);
    pos               = arg1->extra.tmd->coords;
    scratch->delta.vx = gPlayerStatus.coordMtx->t[0] - pos->coord.t[0];
    scratch->delta.vy = gPlayerStatus.coordMtx->t[1] - pos->coord.t[1];
    scratch->delta.vz = gPlayerStatus.coordMtx->t[2] - pos->coord.t[2];
    coord             = arg1->extra.tmd->coords;
    angle             = ratan2(scratch->delta.vx, scratch->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    scratch->turn     = _actorAngleNormalizeYaw(angle);
    if (!overlayOutOfRange(&scratch->delta, 600)) {
        mag = (scratch->turn >= 0) ? scratch->turn : -scratch->turn;
        if (mag < 0x200) {
            if (actor->mode != GAME_ACTOR_MODE_SCRIPTED) {
                work->playerButtonHold.pressCount = 12;
                if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &work->playerButtonHold, 0) == 0) {
                    coord         = player->extra.tmd->coords;
                    angle         = ratan2(scratch->delta.vx, scratch->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
                    scratch->turn = _actorAngleNormalizeYaw(angle);
                    if (scratch->turn < 0) {
                        Actor04000_D0C530.source.sets = Actor04000_D0C510;
                    } else {
                        Actor04000_D0C530.source.sets = Actor04000_D0C520;
                    }
                    Actor04000_D0C530.animationId = 1;
                    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &Actor04000_D0C530, 0);
                    work->state         = ACTOR_04000_STATE_LATCHED;
                    work->holdingPlayer = 1;
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
                    arg1->extra.tmd->coords->coord.t[0] = player->extra.tmd->coords->coord.t[0] + scratch->delta.vx;
                    arg1->extra.tmd->coords->coord.t[1] = player->extra.tmd->coords->coord.t[1];
                    arg1->extra.tmd->coords->coord.t[2] = player->extra.tmd->coords->coord.t[2] + scratch->delta.vz;
                    scratch->delta.vy                   = 0;
                    VectorNormalSS(&scratch->delta, &scratch->delta);
                    gte_lddp(-0x258);
                    gte_ldsv(&scratch->delta);
                    gte_gpf12();
                    gte_stsv(&scratch->delta);
                    arg1->extra.tmd->coords->coord.t[0] += scratch->delta.vx;
                    arg1->extra.tmd->coords->coord.t[2] += scratch->delta.vz;
                    scratch->delta.vx                    = -scratch->delta.vx;
                    scratch->delta.vy                    = -scratch->delta.vy;
                    scratch->delta.vz                    = -scratch->delta.vz;
                    scratch->facingYaw                   = ratan2(scratch->delta.vx, scratch->delta.vz);
                    gfxRotMatrixY(&arg1->extra.tmd->coords->coord, scratch->facingYaw, 1);
                    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                }
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor04000LungeScratch);
}

/// Turns `coord` to face along its own Z axis in the XZ plane and scales the
/// rotation uniformly by `s`, working on a scratch block.
static __inline__ void Actor204000_FaceScale(GfxCoord* coord, s16 s)
{
    ActorScaleRotScratch* head;
    ActorScaleRotScratch* sc;

    head                                       = SCRATCH_STACK_CURSOR(ActorScaleRotScratch);
    sc                                         = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleRotScratch) = sc;
    sc->yaw                                    = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    gfxRotMatrixY(&sc->rotation, sc->yaw, 1);
    sc->scale.vx = sc->scale.vy = sc->scale.vz = s;
    ScaleMatrix(&sc->rotation, &head[-1].scale);
    coord->coord.m[0][0] = head[-1].rotation.m[0][0];
    coord->coord.m[0][1] = sc->rotation.m[0][1];
    coord->coord.m[0][2] = sc->rotation.m[0][2];
    coord->coord.m[1][0] = sc->rotation.m[1][0];
    coord->coord.m[1][1] = sc->rotation.m[1][1];
    coord->coord.m[1][2] = sc->rotation.m[1][2];
    coord->coord.m[2][0] = sc->rotation.m[2][0];
    coord->coord.m[2][1] = sc->rotation.m[2][1];
    coord->coord.m[2][2] = sc->rotation.m[2][2];
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleRotScratch);
}

/// Frames 0x5B onward of the collapse: drifts the model along its facing for the
/// first 0x13 frames, steps the effects keyed on `stateFrame`, then fades the colour
/// matrix out and grows the model over frames 0x5C-0x64.
static void Actor04000_Fn01E1C(Enemy* arg0, Task* arg1)
{
    SVECTOR          dir;
    SVECTOR*         d;
    VECTOR           scale;
    _Actor04000Work* work;
    TmdObject*       obj;
    s16              s;
    s32              pan;
    s32              id;

    work = arg1->work;
    obj  = arg1->extra.tmd;
    if (work->stateEntered != 0) {
        arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                   = 0;
        work->hitBody.flags         &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstAttackBody.key    = damagePackEnemyAttackKey(arg0, 1);
        work->burstWaveBody.key      = 0x22222;
        work->stateFrame             = 0;
        work->gridBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->savedColorMtx          = work->colorMtx;
        work->driver.requestedSet    = 0xE;
        work->driver.state           = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias        = 0;
        _animDriverTick(arg1);
        work->burstWaveBody.pos.vx    = arg1->extra.tmd->coords->coord.t[0];
        work->burstWaveBody.pos.vy    = arg1->extra.tmd->coords->coord.t[1] - 0x1F4;
        work->burstWaveBody.pos.vz    = arg1->extra.tmd->coords->coord.t[2];
        work->burstAttackBody.pos.vx  = arg1->extra.tmd->coords->coord.t[0];
        work->burstAttackBody.pos.vy  = arg1->extra.tmd->coords->coord.t[1];
        work->burstAttackBody.pos.vz  = arg1->extra.tmd->coords->coord.t[2];
        Actor04000_D0C530.animationId = 2;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &Actor04000_D0C530, 0);
        work->stateFrame = 0;
    }
    if (work->stateFrame < 0x13) {
        gfxReadMatrixXAxis(&arg1->extra.tmd->coords->coord, &dir);
        d      = &dir;
        dir.vy = 0;
        VectorNormalSS(d, d);
        gte_lddp(0x15);
        gte_ldsv(d);
        gte_gpf12();
        gte_stsv(d);
        arg1->extra.tmd->coords->coord.t[0]  += dir.vx;
        arg1->extra.tmd->coords->coord.t[2]  += dir.vz;
        arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    _animDriverTick(arg1);
    switch (work->stateFrame) {
        case 0x5B:
            if (work->holdingPlayer == 1) {
                if (((GameActor*)(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->work)->mode == GAME_ACTOR_MODE_SCRIPTED) {
                    taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                }
                work->holdingPlayer = 0;
            }
            arg1->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            break;
        case 0x5C:
            padScriptSpawnDepthScaled(Actor04000_D07094, Actor04000_D070A0,
                                      (s16)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
            work->burstAttackBody.radius = 0x3E8;
            work->burstWaveBody.radius   = 0xFA;
            work->burstAttackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->burstWaveBody.flags   |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            effectSpawn(EFFECT_CRITICAL_HIT, &arg1->extra.tmd->coords[2], 1, NULL);
            break;
        case 0x5D:
            work->burstWaveBody.radius = 0x1F4;
            break;
        case 0x5E:
            work->burstWaveBody.radius   = 0x3E8;
            work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case 0x60:
            sceneReleaseBattleRefWithRewards(arg1, 0xC);
            work->burstWaveBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case 0x62:
            if (work->airborne == 0) {
                effectSpawn(EFFECT_RED_GROUND_GLOW, arg1->extra.tmd->coords, 0, NULL);
            }
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            id         = ((arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40280004;
            pan        = (s8)worldCoordGetOriginAudioPan(arg1->extra.tmd->coords);
            sndEvtRequestScriptStart(id, pan, (s8)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
            break;
        case 0x77:
            work->state = ACTOR_04000_STATE_HIDDEN;
            break;
        default:
            work->colorMtx = work->savedColorMtx;
            break;
    }
    work->colorMtx = work->savedColorMtx;
    if (work->stateFrame >= 0x17 && work->stateFrame < 0x5B) {
        work->colorMtx.t[0] += (work->stateFrame - 0x16) * 0x60;
    }
    if (work->stateFrame >= 0x5C && work->stateFrame < 0x65) {
        s = 0xBB8 - (work->stateFrame - 0x5C) * 600;
        if (s < 0x4B0) {
            scale.vx = scale.vy = scale.vz = 0;
            Actor204000_FaceScale(arg1->extra.tmd->coords, 0x1000);
            ScaleMatrix(&work->colorMtx, &scale);
            work->colorMtx.t[0] = work->colorMtx.t[1] = work->colorMtx.t[2] = 0;
            Actor204000_FaceScale(arg1->extra.tmd->coords, 0x1000);
        } else {
            scale.vx = scale.vy = scale.vz = s;
            work->colorMtx                 = work->savedColorMtx;
            ScaleMatrix(&work->colorMtx, &scale);
            gte_lddp(s);
            gte_ldlvl(work->colorMtx.t);
            gte_gpf12();
            gte_stlvl(work->colorMtx.t);
            s = (work->stateFrame - 0x5A) * 0x400 + 0x1000;
            if (s > 0x2000) {
                s = 0x2000;
            }
            Actor204000_FaceScale(arg1->extra.tmd->coords, s);
        }
    }
    if (work->stateFrame < 0x400) {
        work->stateFrame++;
    } else {
        work->state = ACTOR_04000_STATE_HIDDEN;
    }
}

/// Restarts the actor when `stateEntered` is set; otherwise steps it, occasionally
/// switches to `ROUSE` on a random roll, and arms the player state when the
/// player comes within 2000 units.
static void Actor04000_Fn026FC(Enemy* arg0, Task* arg1)
{
    _Actor04000Work* work;
    GfxCoord*        coord;
    SVECTOR          delta;
    SVECTOR*         d;
    TmdObject*       obj;

    work = arg1->work;
    if (work->stateEntered != 0) {
        obj                          = arg1->extra.tmd;
        arg0->node.state.parts.flags = 0;
        obj->flags                   = 0;
        work->driver.requestedSet    = 5;
        work->driver.state           = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias        = 0;
        work->hitBody.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _animDriverTick(arg1);
        return;
    }
    _animDriverTick(arg1);
    if ((work->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP) && work->driver.jumpCount >= 0x19) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 0x10) & 7)) {
            work->state = ACTOR_04000_STATE_ROUSE;
        }
    }
    coord    = arg1->extra.tmd->coords;
    d        = &delta;
    delta.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    d->vy    = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    d->vz    = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    if (!overlayOutOfRange(d, 2000)) {
        sceneEngageBattle(1);
        work->state = ACTOR_04000_STATE_ROUSE;
    }
}

/// Chasing state: restarts the actor when `stateEntered` is set; otherwise turns
/// toward the player by at most 0x10 a frame and steps forward, counting
/// frames spent more than 1000 units away (`RETURN` after 240), and switches to
/// `LUNGE` within 600 units and an eighth turn of the facing.
static void Actor04000_Fn028F0(Enemy* arg0, Task* arg1)
{
    _Actor04000Work*  work;
    ActorTurnScratch* head;
    ActorTurnScratch* turn;
    GfxCoord*         coord;
    GfxCoord*         target;
    GfxCoord*         pos;
    TmdObject*        obj;
    s16               angle;
    s32               mag;

    work = arg1->work;
    if (work->stateEntered != 0) {
        obj                          = arg1->extra.tmd;
        arg0->node.state.parts.flags = 0;
        obj->flags                   = 0;
        work->driver.requestedSet    = 3;
        work->driver.state           = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias        = ANIMATION_RATE_ONE;
        work->hitBody.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        sceneEngageBattle(1);
        _animDriverTick(arg1);
        work->chaseFarFrames = 0;
        return;
    }
    head = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    turn = SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    _animDriverTick(arg1);
    pos               = arg1->extra.tmd->coords;
    head[-1].delta.vx = gPlayerStatus.coordMtx->t[0] - pos->coord.t[0];
    turn->delta.vy    = gPlayerStatus.coordMtx->t[1] - pos->coord.t[1];
    turn->delta.vz    = gPlayerStatus.coordMtx->t[2] - pos->coord.t[2];
    coord             = arg1->extra.tmd->coords;
    angle             = ratan2(head[-1].delta.vx, turn->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    turn->angle       = _actorAngleNormalizeYaw(angle);
    if (turn->angle > 0x10) {
        turn->angle = 0x10;
    }
    if (turn->angle < -0x10) {
        turn->angle = -0x10;
    }
    turn->angle += ratan2(-arg1->extra.tmd->coords->coord.m[2][0], arg1->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&arg1->extra.tmd->coords->coord, turn->angle, 1);
    actorStepForward(arg1->extra.tmd->coords, 0x14);
    _actorContactApplyGridPushback(arg1->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    if (overlayOutOfRange(&turn->delta, 1000)) {
        work->chaseFarFrames++;
    } else {
        work->chaseFarFrames = 0;
    }
    ActorContact_Steer(arg1->extra.tmd->coords, work->hitContacts, 8, &turn->delta);
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    turn->delta.vx                        = work->spawnPos.vx - arg1->extra.tmd->coords->coord.t[0];
    turn->delta.vy                        = 0;
    turn->delta.vz                        = work->spawnPos.vz - arg1->extra.tmd->coords->coord.t[2];
    overlayOutOfRange(&turn->delta, 3000);
    if (work->chaseFarFrames > 0xF0) {
        work->state = ACTOR_04000_STATE_RETURN;
    }
    target         = arg1->extra.tmd->coords;
    turn->delta.vx = gPlayerStatus.coordMtx->t[0] - target->coord.t[0];
    turn->delta.vy = gPlayerStatus.coordMtx->t[1] - target->coord.t[1];
    turn->delta.vz = gPlayerStatus.coordMtx->t[2] - target->coord.t[2];
    coord          = arg1->extra.tmd->coords;
    angle          = ratan2(turn->delta.vx, turn->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    turn->angle    = _actorAngleNormalizeYaw(angle);
    if (!overlayOutOfRange(&turn->delta, 600)) {
        mag = (turn->angle >= 0) ? turn->angle : -turn->angle;
        if (mag < 0x200) {
            work->state = ACTOR_04000_STATE_LUNGE;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

/// Frames 0x28 onward of the collapse: steps the effects keyed on `stateFrame`,
/// then fades the colour matrix out and grows the model over frames 0x2A-0x32.
static void Actor04000_Fn02F48(Enemy* arg0, Task* arg1)
{
    VECTOR           scale;
    _Actor04000Work* work;
    TmdObject*       obj;
    s16              s;
    s32              pan;
    s32              id;

    work = arg1->work;
    obj  = arg1->extra.tmd;
    if (work->stateEntered != 0) {
        arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                   = 0;
        work->hitBody.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstAttackBody.key    = damagePackEnemyAttackKey(arg0, 1);
        work->burstWaveBody.key      = 0x22222;
        work->stateFrame             = 0;
        work->gridBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->savedColorMtx          = work->colorMtx;
        work->driver.rateBias        = 0;
        _animDriverTick(arg1);
        work->burstWaveBody.pos.vx   = arg1->extra.tmd->coords->coord.t[0];
        work->burstWaveBody.pos.vy   = arg1->extra.tmd->coords->coord.t[1] - 0x1F4;
        work->burstWaveBody.pos.vz   = arg1->extra.tmd->coords->coord.t[2];
        work->burstAttackBody.pos.vx = arg1->extra.tmd->coords->coord.t[0];
        work->burstAttackBody.pos.vy = arg1->extra.tmd->coords->coord.t[1];
        work->burstAttackBody.pos.vz = arg1->extra.tmd->coords->coord.t[2];
        if (work->lastCommandContext == 0x1003) {
            Actor04000_D0C718[arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT] = NULL;
        }
        return;
    }
    _animDriverTick(arg1);
    switch (work->stateFrame) {
        case 0x28:
            work->hitBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case 0x29:
            if (work->holdingPlayer == 1) {
                if (((GameActor*)(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->work)->mode == GAME_ACTOR_MODE_SCRIPTED) {
                    taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                }
                work->holdingPlayer = 0;
            }
            arg1->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            break;
        case 0x2A:
            padScriptSpawnDepthScaled(Actor04000_D07094, Actor04000_D070A0,
                                      (s16)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
            work->burstAttackBody.radius = 0x3E8;
            work->burstWaveBody.radius   = 0xFA;
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), 0x7DA, 0, 0x7DE);
            work->burstAttackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->burstWaveBody.flags   |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            effectSpawn(EFFECT_CRITICAL_HIT, &arg1->extra.tmd->coords[2], 1, NULL);
            break;
        case 0x2B:
            work->burstWaveBody.radius = 0x1F4;
            break;
        case 0x2C:
            work->burstWaveBody.radius = 0x3E8;
            break;
        case 0x2E:
            sceneReleaseBattleRefWithRewards(arg1, 0xC);
            work->burstWaveBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case 0x30:
            if (work->airborne == 0) {
                effectSpawn(EFFECT_RED_GROUND_GLOW, arg1->extra.tmd->coords, 0, NULL);
            }
            id  = ((arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40280004;
            pan = (s8)worldCoordGetOriginAudioPan(arg1->extra.tmd->coords);
            sndEvtRequestScriptStart(id, pan, (s8)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
            break;
        case 0x32:
            work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            obj->flags                   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case 0x34:
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 0x45:
            work->state = ACTOR_04000_STATE_HIDDEN;
            arg0->hp    = 0;
            break;
        default:
            work->colorMtx = work->savedColorMtx;
            break;
    }
    work->colorMtx = work->savedColorMtx;
    if (work->stateFrame >= 0x17 && work->stateFrame < 0x29) {
        work->colorMtx.t[0] += (work->stateFrame - 0x16) * 0x60;
    }
    if (work->stateFrame >= 0x2A && work->stateFrame < 0x33) {
        s = 0xBB8 - (work->stateFrame - 0x2A) * 600;
        if (s < 0x4B0) {
            scale.vx = scale.vy = scale.vz = 0;
            Actor204000_FaceScale(arg1->extra.tmd->coords, 0x1000);
            ScaleMatrix(&work->colorMtx, &scale);
            work->colorMtx.t[0] = work->colorMtx.t[1] = work->colorMtx.t[2] = 0;
            Actor204000_FaceScale(arg1->extra.tmd->coords, 0x1000);
        } else {
            scale.vx = scale.vy = scale.vz = s;
            work->colorMtx                 = work->savedColorMtx;
            ScaleMatrix(&work->colorMtx, &scale);
            gte_lddp(s);
            gte_ldlvl(work->colorMtx.t);
            gte_gpf12();
            gte_stlvl(work->colorMtx.t);
            s = (work->stateFrame - 0x28) * 0x400 + 0x1000;
            if (s > 0x2000) {
                s = 0x2000;
            }
            Actor204000_FaceScale(arg1->extra.tmd->coords, s);
        }
    }
    if (work->stateFrame < 0x400) {
        work->stateFrame++;
    } else {
        work->state = ACTOR_04000_STATE_HIDDEN;
    }
}

/// Death state: saves the colour matrix, then fades it and grows the model
/// over frames 13-21 while stepping through the collapse effects.
static void Actor04000_Fn03798(Enemy* arg0, Task* arg1)
{
    VECTOR           scale;
    _Actor04000Work* work;
    TmdObject*       obj;
    s16              s;
    s32              pan;
    s32              id;

    work = arg1->work;
    obj  = arg1->extra.tmd;
    if (work->stateEntered != 0) {
        arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                   = 0;
        work->hitBody.flags         &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstAttackBody.key    = damagePackEnemyAttackKey(arg0, 1);
        work->burstWaveBody.key      = 0x22222;
        work->stateFrame             = 0;
        work->gridBody.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        work->savedColorMtx          = work->colorMtx;
        work->driver.requestedSet    = 0xA;
        work->driver.state           = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias        = 0;
        work->driver.rate            = 0x2C;
        _animDriverTick(arg1);
        work->burstWaveBody.pos.vx   = arg1->extra.tmd->coords->coord.t[0];
        work->burstWaveBody.pos.vy   = arg1->extra.tmd->coords->coord.t[1] - 0x1F4;
        work->burstWaveBody.pos.vz   = arg1->extra.tmd->coords->coord.t[2];
        work->burstAttackBody.pos.vx = arg1->extra.tmd->coords->coord.t[0];
        work->burstAttackBody.pos.vy = arg1->extra.tmd->coords->coord.t[1];
        work->burstAttackBody.pos.vz = arg1->extra.tmd->coords->coord.t[2];
        sceneEngageBattle(1);
        if (work->lastCommandContext == 0x1003) {
            Actor04000_D0C718[arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT] = NULL;
        }
    }
    _animDriverTick(arg1);
    switch (work->stateFrame) {
        case 0xD:
            id  = ((arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40280004;
            pan = (s8)worldCoordGetOriginAudioPan(arg1->extra.tmd->coords);
            sndEvtRequestScriptStart(id, pan, (s8)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
            arg1->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            break;
        case 0xE:
            work->burstAttackBody.radius = 0x3E8;
            work->burstAttackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            effectSpawn(EFFECT_CRITICAL_HIT, &arg1->extra.tmd->coords[2], 1, NULL);
            padScriptSpawn(Actor04000_D07094, Actor04000_D070A0);
            break;
        case 0xF:
            sceneReleaseBattleRefWithRewards(arg1, 0xC);
            work->burstWaveBody.radius   = 0xFA;
            work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->burstWaveBody.flags   |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            break;
        case 0x10:
            work->burstWaveBody.radius = 0x1F4;
            break;
        case 0x11:
            work->burstWaveBody.radius = 0x3E8;
            break;
        case 0x13:
            work->burstWaveBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            if (work->airborne == 0) {
                effectSpawn(EFFECT_RED_GROUND_GLOW, arg1->extra.tmd->coords, 0, NULL);
            }
            break;
        case 0x15:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case 0x17:
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 0x26:
            work->state = ACTOR_04000_STATE_HIDDEN;
            break;
    }
    if (work->stateFrame >= 0xD && work->stateFrame < 0x16) {
        s = 0xBB8 - (work->stateFrame - 0xB) * 0x320;
        if (s < 0) {
            s = 0;
        }
        scale.vx = scale.vy = scale.vz = s;
        work->colorMtx                 = work->savedColorMtx;
        ScaleMatrix(&work->colorMtx, &scale);
        gte_lddp(s);
        gte_ldlvl(work->colorMtx.t);
        gte_gpf12();
        gte_stlvl(work->colorMtx.t);
        s = work->stateFrame * 0xB4 + 0x1000;
        if (s > 0x2000) {
            s = 0x2000;
        }
        Actor204000_FaceScale(arg1->extra.tmd->coords, s);
    }
    if (work->stateFrame < 0x400) {
        work->stateFrame++;
    } else {
        work->state = ACTOR_04000_STATE_HIDDEN;
    }
}

/// Picks a random offset and coordinate index for an effect from the hit
/// angle `arg1` (front, back, right or left), copies it into `work->hitEffectOffset` and
/// spawns the effect for hit id `arg2`.
static void Actor04000_Fn03D30(Task* arg0, s16 arg1, u32 arg2)
{
    SVECTOR*         sc;
    _Actor04000Work* work;
    s32              mag;
    GfxCoord*        coord;

    sc   = (SVECTOR*)SCRATCH_STACK_RESERVE_BYTES(sizeof(SVECTOR));
    mag  = (arg1 >= 0) ? arg1 : -arg1;
    work = arg0->work;
    if (mag < 0x200) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 16) & 1)) {
            sc->pad = 2;
            sc->vx  = 80;
            sc->vy  = -180;
            sc->vz  = 330;
        } else {
            sc->pad = 2;
            sc->vx  = -60;
            sc->vy  = -150;
            sc->vz  = 300;
        }
    } else if (mag > 0x600) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 16) & 1)) {
            sc->pad = 1;
            sc->vx  = 0;
            sc->vy  = 0;
            sc->vz  = -180;
        } else {
            sc->pad = 2;
            sc->vx  = 2;
            sc->vy  = -50;
            sc->vz  = -50;
        }
    } else if (arg1 > 0) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 16) & 1)) {
            sc->pad = 5;
            sc->vx  = 100;
            sc->vy  = 0;
            sc->vz  = 0;
        } else {
            sc->pad = 5;
            sc->vx  = 120;
            sc->vy  = 0;
            sc->vz  = 100;
        }
    } else {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 16) & 1)) {
            sc->pad = 4;
            sc->vx  = -100;
            sc->vy  = 0;
            sc->vz  = 0;
        } else {
            sc->pad = 4;
            sc->vx  = -120;
            sc->vy  = 0;
            sc->vz  = 100;
        }
    }
    work->hitEffectOffset         = *sc;
    coord                         = &arg0->extra.tmd->coords[sc->pad];
    work->hitEffectArg.spawnArgLo = 0x100;
    work->hitEffectArg.spawnArgHi = 1;
    work->hitEffectArg.coord      = coord;
    effectSpawnHit(damageGetPlayerAttackEffectId(arg2), &arg0->extra.tmd->coords[sc->pad], &work->hitEffectOffset, &work->hitEffectArg);
    SCRATCH_STACK_RELEASE_BYTES(sizeof(SVECTOR));
}

/// First `WorldCollisionContact` among the `count` at `records` whose id has high word 2,
/// copying its position to `pos`; 0 at the first empty record.
static __inline__ s32 Actor04000_FindHit(SVECTOR* pos, WorldCollisionContact* records, s16 count)
{
    s16 i;

    for (i = 0; i < count; i++) {
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

/// Applies the first type-2 hit in `work->hitContacts`: computes its damage, turns the
/// model toward the hit, plays the impact sound and subtracts the damage from
/// `arg0->field_40`, switching to `DEATH_BURST` once it runs out.
static void Actor04000_Fn03FB4(Enemy* arg0, Task* arg1)
{
    ActorHitTakenScratch* sc;
    _Actor04000Work*      work;
    s16                   angle;
    s32                   snd;
    s32                   pan;

    work       = arg1->work;
    sc         = SCRATCH_STACK_RESERVE_BLOCK(ActorHitTakenScratch);
    sc->hitKey = Actor04000_FindHit(&sc->hitPos, work->hitContacts, ARRAY_SIZE(work->hitContacts));

    if (sc->hitKey != 0) {
        sc->damage                            = damageComputePlayerAttack(sc->hitKey, 0, 0, 0x1000);
        arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(arg1->extra.tmd->coords);
        sc->hitOffset.vx = arg1->extra.tmd->coords->workm.t[0];
        sc->hitOffset.vy = arg1->extra.tmd->coords->workm.t[1];
        sc->hitOffset.vz = arg1->extra.tmd->coords->workm.t[2];
        sc->hitOffset.vx = sc->hitPos.vx - arg1->extra.tmd->coords->workm.t[0];
        sc->hitOffset.vy = sc->hitPos.vy - arg1->extra.tmd->coords->workm.t[1];
        sc->hitOffset.vz = sc->hitPos.vz - arg1->extra.tmd->coords->workm.t[2];
        angle            = ratan2(sc->hitOffset.vx, sc->hitOffset.vz) -
                ratan2(-arg1->extra.tmd->coords->workm.m[2][0], arg1->extra.tmd->coords->workm.m[2][2]);
        sc->hitYaw = angle;
        sc->hitYaw = _actorAngleNormalizeYaw(angle);
        Actor04000_Fn03D30(arg1, sc->hitYaw, sc->hitKey);
        snd = ((arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40280003;
        pan = (s8)worldCoordGetOriginAudioPan(arg1->extra.tmd->coords);
        sndEvtRequestScriptStart(snd, pan, (s8)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
        damageAccumulateLifeDrainHp(arg0, sc->hitKey, sc->damage, 0);
        worldTargetAddReadoutAmount(&arg0->node, sc->damage, 0);
        arg0->hp -= sc->damage;
        if (arg0->hp <= 0) {
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

/// Patrol state: restarts the actor when `stateEntered` is set; otherwise turns the
/// model toward the current patrol point by at most 0x20 a frame and steps it
/// forward, swapping patrol points within 400 units or after 97 blocked frames,
/// switching to `CHASE` when the player is within 2000 units and either
/// inside a quarter turn of the facing or within 1000 units, and occasionally to
/// `SETTLE` once `driver.jumpCount` passes 20.
static void Actor04000_Fn0432C(Enemy* arg0, Task* arg1)
{
    _Actor04000Work*  work;
    ActorTurnScratch* head;
    ActorTurnScratch* turn;
    GfxCoord*         coord;
    GfxCoord*         target;
    TmdObject*        obj;
    s16               angle;

    work = arg1->work;
    if (work->stateEntered != 0) {
        obj                          = arg1->extra.tmd;
        arg0->node.state.parts.flags = 0;
        obj->flags                   = 0;
        work->driver.requestedSet    = 2;
        work->driver.state           = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias        = 0;
        work->patrolIndex            = 0;
        work->hitBody.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _animDriverTick(arg1);
        work->stateFrame = 0;
        return;
    }
    head              = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    turn              = SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    head[-1].delta.vx = work->patrolPoints[work->patrolIndex].vx - arg1->extra.tmd->coords->coord.t[0];
    turn->delta.vy    = 0;
    turn->delta.vz    = work->patrolPoints[work->patrolIndex].vz - arg1->extra.tmd->coords->coord.t[2];
    coord             = arg1->extra.tmd->coords;
    angle             = ratan2(head[-1].delta.vx, turn->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    turn->angle       = _actorAngleNormalizeYaw(angle);
    if (turn->angle > 0x20) {
        turn->angle = 0x20;
    }
    if (turn->angle < -0x20) {
        turn->angle = -0x20;
    }
    turn->angle += ratan2(-arg1->extra.tmd->coords->coord.m[2][0], arg1->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&arg1->extra.tmd->coords->coord, turn->angle, 1);
    actorStepForward(arg1->extra.tmd->coords, 5);
    if (_actorContactApplyGridPushback(arg1->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts))) {
        work->stateFrame++;
    }
    if (!overlayOutOfRange(&turn->delta, 400) || work->stateFrame > 0x60) {
        if (work->patrolIndex == 0) {
            work->patrolIndex = 1;
        } else {
            work->patrolIndex = 0;
        }
        work->stateFrame = 0;
    }
    ActorContact_Steer(arg1->extra.tmd->coords, work->hitContacts, 8, &turn->delta);
    target         = arg1->extra.tmd->coords;
    turn->delta.vx = gPlayerStatus.coordMtx->t[0] - target->coord.t[0];
    turn->delta.vy = gPlayerStatus.coordMtx->t[1] - target->coord.t[1];
    turn->delta.vz = gPlayerStatus.coordMtx->t[2] - target->coord.t[2];
    if (!overlayOutOfRange(&turn->delta, 2000)) {
        coord = arg1->extra.tmd->coords;
        angle = ratan2(turn->delta.vx, turn->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        if (_actorAngleNormalizeYaw(angle) < 0x400 || !overlayOutOfRange(&turn->delta, 1000)) {
            work->state = ACTOR_04000_STATE_CHASE;
        }
    }
    _animDriverTick(arg1);
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if ((work->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP) && work->driver.jumpCount > 0x14) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (!((gRandomLcgState >> 0x10) & 7)) {
            work->state = ACTOR_04000_STATE_SETTLE;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

/// Walking state: restarts the actor when `stateEntered` is set; otherwise turns the
/// model toward its spawn point by at most 0x10 a frame and steps it forward,
/// switching to `SETTLE` within 80 units of the spawn point and to `CHASE` when
/// the player is within 2000 units and either inside a quarter turn of
/// the facing or within 1000 units.
static void Actor04000_Fn049C0(Enemy* arg0, Task* arg1)
{
    _Actor04000Work*  work;
    ActorTurnScratch* head;
    ActorTurnScratch* turn;
    GfxCoord*         coord;
    GfxCoord*         target;
    TmdObject*        obj;
    s16               angle;

    work = arg1->work;
    if (work->stateEntered != 0) {
        obj                          = arg1->extra.tmd;
        arg0->node.state.parts.flags = 0;
        obj->flags                   = 0;
        work->driver.requestedSet    = 2;
        work->driver.state           = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias        = 0;
        work->hitBody.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _animDriverTick(arg1);
        work->chaseFarFrames = 0;
        return;
    }
    head = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    turn = SCRATCH_STACK_RESERVE_BLOCK(ActorTurnScratch);
    _animDriverTick(arg1);
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    head[-1].delta.vx                     = work->spawnPos.vx - arg1->extra.tmd->coords->coord.t[0];
    turn->delta.vy                        = 0;
    turn->delta.vz                        = work->spawnPos.vz - arg1->extra.tmd->coords->coord.t[2];
    coord                                 = arg1->extra.tmd->coords;
    angle                                 = ratan2(head[-1].delta.vx, turn->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    turn->angle                           = _actorAngleNormalizeYaw(angle);
    if (turn->angle > 0x10) {
        turn->angle = 0x10;
    }
    if (turn->angle < -0x10) {
        turn->angle = -0x10;
    }
    turn->angle += ratan2(-arg1->extra.tmd->coords->coord.m[2][0], arg1->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&arg1->extra.tmd->coords->coord, turn->angle, 1);
    actorStepForward(arg1->extra.tmd->coords, 8);
    _actorContactApplyGridPushback(arg1->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    if (!overlayOutOfRange(&turn->delta, 80)) {
        work->state = ACTOR_04000_STATE_SETTLE;
    }
    ActorContact_Steer(arg1->extra.tmd->coords, work->hitContacts, 8, &turn->delta);
    target         = arg1->extra.tmd->coords;
    turn->delta.vx = gPlayerStatus.coordMtx->t[0] - target->coord.t[0];
    turn->delta.vy = gPlayerStatus.coordMtx->t[1] - target->coord.t[1];
    turn->delta.vz = gPlayerStatus.coordMtx->t[2] - target->coord.t[2];
    if (!overlayOutOfRange(&turn->delta, 2000)) {
        coord = arg1->extra.tmd->coords;
        angle = ratan2(turn->delta.vx, turn->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        if (_actorAngleNormalizeYaw(angle) < 0x400 || !overlayOutOfRange(&turn->delta, 1000)) {
            work->state = ACTOR_04000_STATE_CHASE;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}

/// Restarts the actor when `stateEntered` is set; otherwise waits 50 frames, then
/// drops the model with growing speed, unwinding its Z roll by at most 0x92 a
/// frame, and on landing plays the impact sound and switches to `ROUSE`.
static void Actor04000_Fn04FA4(Enemy* arg0, Task* arg1)
{
    _Actor04000Work* work;
    s32              id;
    s32              pan;
    s32              rot;
    s16              step;

    work = arg1->work;
    if (work->stateEntered != 0) {
        arg1->extra.tmd->flags       = 0;
        work->driver.requestedSet    = 5;
        work->driver.state           = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias        = 0;
        work->hitBody.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _animDriverTick(arg1);
        ratan2(-arg1->extra.tmd->coords->coord.m[2][0], arg1->extra.tmd->coords->coord.m[2][2]);
        arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->fallAccel                       = 10;
        work->fallSpeed                       = 0;
        work->stateFrame                      = 0;
        work->fallRoll                        = 0x800;
        work->airborne                        = 1;
        return;
    }
    if (work->stateFrame < 0x32) {
        work->stateFrame++;
        return;
    }
    work->fallAccel                     += 4;
    work->fallSpeed                     += work->fallAccel;
    arg1->extra.tmd->coords->coord.t[1] += work->fallSpeed;
    if (arg1->extra.tmd->coords->coord.t[1] >= -0x12B) {
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(3, 16, 0, 0)) {
            id  = ((arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x53100006;
            pan = (s8)worldCoordGetOriginAudioPan(arg1->extra.tmd->coords);
            sndEvtRequestScriptStart(id, pan, (s8)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
        }
        arg1->extra.tmd->coords->coord.t[1] = 0;
        work->state                         = ACTOR_04000_STATE_ROUSE;
        work->airborne                      = 0;
        gfxRotMatrixZ(&arg1->extra.tmd->coords->coord, -work->fallRoll, GRAPHICS_ROTATION_COMPOSE);
    } else {
        step = work->fallRoll;
        rot  = step;
        if (rot != 0) {
            step = -step;
            if (abs(rot) > 0x92) {
                step = (rot < 0) ? 0x92 : -0x92;
            }
            gfxRotMatrixZ(&arg1->extra.tmd->coords->coord, step, GRAPHICS_ROTATION_COMPOSE);
            work->fallRoll += step;
        }
    }
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _animDriverTick(arg1);
}

/// Restarts the actor when `stateEntered` is set; otherwise cycles `stateFrame` through
/// a 32-frame loop that resets the model position, steps it back and forth
/// and changes `driver.rate`, then spins it and raises `field_14` in view 5.
static void Actor04000_Fn0522C(Enemy* arg0, Task* arg1)
{
    _Actor04000Work* work;

    work = arg1->work;
    if (work->stateEntered != 0) {
        arg1->extra.tmd->flags       = 0;
        work->hitBody.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        arg0->node.state.parts.flags = 0;
        work->driver.requestedSet    = 3;
        work->driver.state           = ANIM_DRIVER_STATE_RESTART_2;
        work->driver.rateBias        = 0;
        _animDriverTick(arg1);
        gfxRotMatrixX(&arg1->extra.tmd->coords->coord, 0x400, GRAPHICS_ROTATION_COMPOSE);
        arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->airborne                        = 1;
        work->hitBody.coord                   = &gGfxViewCoord;
        work->hitBody.pos.vx                  = -0x3AC;
        work->hitBody.pos.vy                  = -0xF0;
        work->stateFrame                      = 0;
        work->hitBody.pos.vz                  = 0x166C;
        return;
    }
    switch (++work->stateFrame % 32) {
        case 0:
            work->driver.rate                   = 0x40;
            arg1->extra.tmd->coords->coord.t[0] = -0x3AC;
            arg1->extra.tmd->coords->coord.t[1] = -0xF0;
            arg1->extra.tmd->coords->coord.t[2] = 0x166C;
            break;
        case 1:
        case 2:
        case 4:
        case 5:
        case 7:
        case 8:
            actorStepForward(arg1->extra.tmd->coords, -0x78);
            break;
        case 11:
        case 12:
        case 14:
            actorStepForward(arg1->extra.tmd->coords, 0xC8);
            break;
        case 17:
            work->driver.rate = ANIMATION_RATE_ONE;
            break;
        case 25:
            work->driver.rate = 8;
            break;
    }
    gfxRotMatrixY(&arg1->extra.tmd->coords->coord, 0x44C, 1);
    gfxRotMatrixX(&arg1->extra.tmd->coords->coord, 0x190, GRAPHICS_ROTATION_COMPOSE);
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _animDriverTick(arg1);
    if ((u8)viewGetMappedIndex() == 5) {
        arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        worldTargetDisableNodeLockOn(&(arg0)->node);
        return;
    }
    arg0->node.state.parts.flags = 0;
}

/// Restarts the actor when `stateEntered` is set; otherwise runs animation 12 (drop the
/// model to the ground, then hop forward and tip it over), animation 16 (wait for
/// the flag or 80 frames) and animation 17 (slide along `slideDir` while rolling).
static void Actor04000_Fn055C8(Enemy* arg0, Task* arg1)
{
    _Actor04000Work* work;
    GfxCoord*        coord;
    SVECTOR          sv;
    s32              y;
    s16              t;

    work = arg1->work;
    if (work->stateEntered != 0) {
        arg1->extra.tmd->flags       = 0;
        work->hitBody.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        arg0->node.state.parts.flags = 0;
        work->driver.requestedSet    = 0xC;
        work->driver.state           = ANIM_DRIVER_STATE_RESTART_2;
        work->driver.rateBias        = 0;
        work->driver.rate            = 1;
        _animDriverTick(arg1);
        _animDriverTick(arg1);
        work->driver.rate                     = 0;
        arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->stateFrame                      = 0;
        work->airborne                        = 1;
        work->fallAccel                       = 0xA;
        work->fallSpeed                       = 0;
        work->stateFrame                      = 0;
        work->field_8                         = 0;
        gfxReadMatrixXAxis(&arg1->extra.tmd->coords->coord, &work->slideDir);
        VectorNormalSS(&work->slideDir, &work->slideDir);
    }
    work->stateFrame++;
    switch (work->driver.requestedSet) {
        case 12:
            if (work->driver.rate == 0) {
                work->fallAccel += 2;
                work->fallSpeed += work->fallAccel;
                coord            = arg1->extra.tmd->coords;
                y                = coord->coord.t[1];
                if (y >= 0 || (y < 0 ? -y : y) < work->fallSpeed) {
                    coord->coord.t[1] = 0;
                    work->driver.rate = ANIMATION_RATE_ONE;
                    work->stateFrame  = 0;
                } else {
                    coord->coord.t[1] = y + work->fallSpeed;
                }
                arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                return;
            }
            _animDriverTick(arg1);
            if (work->stateFrame < 0xA) {
                actorStepForward(arg1->extra.tmd->coords, 0x23);
            }
            if (work->stateFrame < 4) {
                arg1->extra.tmd->coords->coord.t[1] -= 0x67;
            }
            if ((u32)((u16)work->stateFrame - 4) < 8) {
                arg1->extra.tmd->coords->coord.t[1]  -= 0xB;
                arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                gfxRotMatrixX(&arg1->extra.tmd->coords->coord, -0x100, GRAPHICS_ROTATION_COMPOSE);
            }
            if (work->stateFrame == 0xC) {
                work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
                work->driver.requestedSet = 0x10;
                work->stateFrame          = 0;
                work->driver.rate         = ANIMATION_RATE_ONE;
            }
            break;
        case 16:
            _animDriverTick(arg1);
            if (!((arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) & 1)) {
                t = work->stateFrame;
                if (t < 0x14) {
                    break;
                }
                if (t < 0x50) {
                    break;
                }
            }
            work->driver.requestedSet = 0x11;
            work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
            work->stateFrame          = 0;
            break;
        case 17:
            _animDriverTick(arg1);
            if ((u32)((u16)work->stateFrame - 0xD) < 0x10) {
                arg1->extra.tmd->coords->coord.t[1] += 0xD;
                sv                                   = work->slideDir;
                gte_lddp(0x14);
                gte_ldsv(&sv);
                gte_gpf12();
                gte_stsv(&sv);
                arg1->extra.tmd->coords->coord.t[0] += sv.vx;
                arg1->extra.tmd->coords->coord.t[2] += sv.vz;
                gfxRotMatrixZ(&arg1->extra.tmd->coords->coord, -0x88, GRAPHICS_ROTATION_COMPOSE);
                arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            }
            if (work->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
                work->state    = ACTOR_04000_STATE_CHASE;
                work->airborne = 0;
            }
            break;
    }
}

/// Resets the actor when `stateEntered` is set; otherwise advances the `stateFrame`
/// timer, stepping the model forward in three speed bands and switching to
/// `SCRIPTED_FALL` once it passes 48.
static void Actor04000_Fn05AE8(Enemy* arg0, Task* arg1)
{
    _Actor04000Work* work;

    work = arg1->work;
    if (work->stateEntered != 0) {
        arg1->extra.tmd->flags       = 0;
        work->hitBody.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        arg0->node.state.parts.flags = 0;
        work->driver.requestedSet    = 0xB;
        work->driver.state           = ANIM_DRIVER_STATE_RESTART_2;
        work->driver.rateBias        = 0;
        work->driver.rate            = 1;
        _animDriverTick(arg1);
        work->driver.rate                     = ANIMATION_RATE_ONE;
        arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->stateFrame                      = 0;
        work->airborne                        = 1;
        work->fallAccel                       = 0xA;
        work->fallSpeed                       = 0;
        work->stateFrame                      = 0;
        work->field_8                         = 0;
        return;
    }
    work->stateFrame++;
    _animDriverTick(arg1);
    if (work->stateFrame >= 0x13 && work->stateFrame < 0x23) {
        actorStepForward(arg1->extra.tmd->coords, 4);
    }
    if (work->stateFrame >= 0x23 && work->stateFrame < 0x28) {
        actorStepForward(arg1->extra.tmd->coords, 0xC);
    }
    if (work->stateFrame >= 0x28 && work->stateFrame < 0x31) {
        actorStepForward(arg1->extra.tmd->coords, 0x18);
        arg1->extra.tmd->coords->coord.t[1] += 0x28;
    }
    if (work->stateFrame > 0x30) {
        work->state = ACTOR_04000_STATE_SCRIPTED_FALL;
    }
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

static const _Actor04000StateTable Actor04000_D001F4 = {
    {
        Actor04000_Fn06A5C,
        Actor04000_Fn06BC8,
        Actor04000_Fn026FC,
        Actor04000_Fn06C80,
        Actor04000_Fn028F0,
        Actor04000_Fn02F48,
        Actor04000_Fn03798,
        Actor04000_Fn0432C,
        Actor04000_Fn049C0,
        Actor04000_Fn06AC4,
        Actor04000_Fn0168C,
        Actor04000_Fn06878,
        Actor04000_Fn06994,
        Actor04000_Fn01E1C,
        Actor04000_Fn06D38,
        Actor04000_Fn04FA4,
        Actor04000_Fn0522C,
        Actor04000_Fn055C8,
        Actor04000_Fn05AE8,
    }
};

/// Per-frame tick: tints the model from its position, draws the ground shadow
/// for the current light mode, runs the state handler (flagging a state change
/// in `stateEntered`), applies pending hits and plays the queued sound.
static void Actor04000_Fn05F0C(Enemy* arg0, Task* arg1)
{
    VECTOR                pos;
    SVECTOR               unused; // never written; retail's frame keeps 8 bytes here
    _Actor04000StateTable table;
    GfxCoord              coord;
    _Actor04000Work*      work;
    s32                   snd;
    s32                   pan;
    s32                   id;

    work                                  = arg1->work;
    table                                 = Actor04000_D001F4;
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(arg1->extra.tmd->coords);
    pos.vx = arg1->extra.tmd->coords->workm.t[0];
    pos.vy = arg1->extra.tmd->coords->workm.t[1];
    pos.vz = arg1->extra.tmd->coords->workm.t[2];
    worldCoordUpdateActorColor(arg0, &pos, 0, 0);
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->state != ACTOR_04000_STATE_HIDDEN && work->state != ACTOR_04000_STATE_DEATH_BURST && work->state != ACTOR_04000_STATE_SELF_BURST && work->state != ACTOR_04000_STATE_RELEASE_BURST &&
                work->state != ACTOR_04000_STATE_DROP && work->state != ACTOR_04000_STATE_SCRIPTED_THRASH && work->state != ACTOR_04000_STATE_SCRIPTED_FALL) {
                arg1->extra.tmd->flags = 0;
                effectDrawGroundShadow(MATRIX_TRANS(&arg1->extra.tmd->coords->workm), 0x100, gRoomEffectState->groundShadowShade);
            }
            if (work->state == ACTOR_04000_STATE_DROP) {
                gfxSetRotIdentity(&coord.coord);
                coord.coord.t[0]                          = arg1->extra.tmd->coords->coord.t[0];
                coord.coord.t[1]                          = 0;
                coord.coord.t[2]                          = arg1->extra.tmd->coords->coord.t[2];
                coord.parent                              = &gGfxViewCoord;
                coord.composeStamp                        = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(&coord);
                effectDrawGroundShadow(MATRIX_TRANS(&coord.workm), 0x60, gRoomEffectState->groundShadowShade);
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            if (work->state != ACTOR_04000_STATE_HIDDEN && work->state != ACTOR_04000_STATE_DEATH_BURST && work->state != ACTOR_04000_STATE_RELEASE_BURST && work->state != ACTOR_04000_STATE_SELF_BURST &&
                work->state != ACTOR_04000_STATE_DROP && work->state != ACTOR_04000_STATE_SCRIPTED_THRASH && work->state != ACTOR_04000_STATE_SCRIPTED_FALL) {
                arg1->extra.tmd->flags = 0;
                effectDrawGroundShadow(MATRIX_TRANS(&arg1->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
            }
            worldCollisionClearContacts(work->gridContacts);
            worldCollisionClearContacts(work->hitContacts);
            worldCollisionClearContacts(work->burstAttackContacts);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            arg1->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            worldCollisionClearContacts(work->gridContacts);
            worldCollisionClearContacts(work->hitContacts);
            worldCollisionClearContacts(work->burstAttackContacts);
            return;
    }
    if (work->prevState != work->state) {
        work->stateEntered = 1;
    } else {
        work->stateEntered = 0;
    }
    work->prevState = work->state;
    table.handlers[work->state](arg0, arg1);
    if (work->holdingPlayer == 1) {
        if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0 || arg0->hp < 0) {
            if (((GameActor*)gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work)->mode == GAME_ACTOR_MODE_SCRIPTED) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            }
            work->holdingPlayer = 0;
        }
    }
    if (arg0->hp > 0) {
        Actor04000_Fn03FB4(arg0, arg1);
        if (arg0->hp <= 0) {
            work->state = ACTOR_04000_STATE_DEATH_BURST;
        }
    }
    worldCollisionClearContacts(work->gridContacts);
    worldCollisionClearContacts(work->hitContacts);
    worldCollisionClearContacts(work->burstAttackContacts);
    id = Actor04000_Fn00FDC(work);
    if (id != 0) {
        snd = id | ((arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan = (s8)worldCoordGetOriginAudioPan(arg1->extra.tmd->coords);
        sndEvtRequestScriptStart(snd, pan, (s8)worldCoordGetOriginAudioDepth(arg1->extra.tmd->coords));
    }
    if (gGameSession->viewReady != 0) {
        arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

/// Refills the two lead slots: a slot whose actor's `field_40` has run out is
/// cleared, and an empty one takes the pooled actor in `HANG` farthest from
/// the player, moving it to `DROP`. With no candidate left the controller
/// task moves on to its next state.
void Actor04000_Fn06380(Task* arg0)
{
    s32           dist[8];
    SVECTOR       d;
    GfxCoord*     coord;
    MATRIX*       m;
    PlayerStatus* cfg;
    s32           best;
    s16           i;
    s16           j;
    s16           bi;

    bi = 0;
    for (i = 0; i < 2; i++) {
        if (Actor04000_D0C710[i] != NULL) {
            if (((Enemy*)Actor04000_D0C710[i]->spawnArg2.pointer)->hp <= 0) {
                Actor04000_D0C710[i] = NULL;
            }
            if (Actor04000_D0C710[i] != NULL) {
                continue;
            }
        }
        j   = 0;
        cfg = &gPlayerStatus;
        for (; j < 6; j++) {
            if (Actor04000_D0C718[j] != NULL && ((_Actor04000Work*)Actor04000_D0C718[j]->work)->state == ACTOR_04000_STATE_HANG) {
                coord    = Actor04000_D0C718[j]->extra.tmd->coords;
                m        = cfg->coordMtx;
                d.vx     = m->t[0] - coord->coord.t[0];
                d.vy     = m->t[1] - coord->coord.t[1];
                d.vz     = m->t[2] - coord->coord.t[2];
                dist[j]  = d.vx * d.vx;
                dist[j] += d.vz * d.vz;
            } else {
                dist[j] = -1;
            }
        }
        best = -1;
        for (j = 0; j < 6; j++) {
            if (best < dist[j]) {
                best = dist[j];
                bi   = j;
            }
        }
        if (best == -1) {
            arg0->state++;
            return;
        }
        ((_Actor04000Work*)Actor04000_D0C718[bi]->work)->state = ACTOR_04000_STATE_DROP;
        Actor04000_D0C710[i]                                   = Actor04000_D0C718[bi];
        Actor04000_D0C718[bi]                                  = NULL;
    }
}

/// Handler for message 0x7D5: sets the display object's visibility bits and
/// the work block's state from a mode. 0 shows the object (0x80) and sets
/// `PATROL`, 1 hides it and sets `PATROL`, 2 raises the lost-model flag (4) and clears
/// the state, 3 hides it, clears the state and then raises the flag, and any
/// other mode leaves both alone. Always answers 0.
s32 Actor04000_Fn06590(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    _Actor04000Work* work;
    TmdObject*       obj;

    obj  = task->extra.tmd;
    work = task->work;

    switch (arg2) {
        case 0:
            obj->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->state = ACTOR_04000_STATE_PATROL;
            break;
        case 1:
            obj->flags  = 0;
            work->state = ACTOR_04000_STATE_PATROL;
            break;
        case 2:
            obj->flags  = (u16)(obj->flags | TMD_OBJECT_SKIP_AUTO_BUFFER);
            work->state = ACTOR_04000_STATE_HIDDEN;
            break;
        case 3:
            obj->flags  = 0;
            work->state = ACTOR_04000_STATE_HIDDEN;
            obj->flags  = (u16)(obj->flags | TMD_OBJECT_SKIP_AUTO_BUFFER);
            break;
        default:
            return 0;
    }
    return 0;
}

#include "../../shared/actor_messages_place.inc.c"

/// Message handler for 0x7DE: advances the work block's state from `LATCHED` to
/// `RELEASE_BURST` and leaves any other state alone.
s32 Actor04000_Fn06704(Task* arg0, s32 arg1, void* arg2, s32 arg3)
{
    _Actor04000Work* work;

    work = arg0->work;
    if (work->state == ACTOR_04000_STATE_LATCHED) {
        work->state = ACTOR_04000_STATE_RELEASE_BURST;
    }
    return 1;
}

/// Handler for message 0x7D3: latches the animation id the sender asks for and
/// requests driver restart 2 when its blend is a reset, 1 otherwise. Always answers 1.
s32 Actor04000_Fn06728(Task* task, s32 arg1, AnimationPlayRequest* msg, s32 arg3)
{
    _Actor04000Work* work = task->work;

    work->driver.requestedSet = msg->animationId;
    if (msg->blend == ANIMATION_BLEND_RESET) {
        work->driver.state = ANIM_DRIVER_STATE_RESTART_2;
    } else {
        work->driver.state = ANIM_DRIVER_STATE_RESTART_1;
    }
    return 1;
}

#include "../../shared/coord_math_yaw_scale.inc.c"

static void Actor04000_Fn06878(Enemy* arg0, Task* arg1)
{
    _Actor04000Work* work;
    TmdObject*       obj;

    work = arg1->work;
    if (work->stateEntered != 0) {
        obj                          = arg1->extra.tmd;
        arg0->node.state.parts.flags = 0;
        obj->flags                   = 0;
        work->driver.rate            = ANIMATION_RATE_ONE;
        work->driver.rateBias        = 0;
        work->gridBody.flags         = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        _animDriverTick(arg1);
        work->stateFrame = 0;
        return;
    }
    _animDriverTick(arg1);
    if (!(work->stateFrame & 7)) {
        padScriptSpawnVariableMotorRamp(3, 0xFF, 8);
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(arg0, 0), 0);
    }
    work->stateFrame++;
    if (work->stateFrame > 0x28) {
        work->playerButtonHold.pressCount = 9999;
        if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &work->playerButtonHold, 0) == 0) {
            work->state = ACTOR_04000_STATE_SELF_BURST;
        } else {
            work->state = ACTOR_04000_STATE_RELEASE_BURST;
        }
    }
}

static void Actor04000_Fn06994(Enemy* arg0, Task* arg1)
{
    _Actor04000Work* work;
    TmdObject*       obj;

    work = arg1->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg1->extra.tmd;
        ((Enemy*)arg1->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        work->driver.state                                        = ANIM_DRIVER_STATE_RESTART_2;
        work->driver.rate                                         = ANIMATION_RATE_ONE;
        work->driver.rateBias                                     = 0;
        work->driver.requestedSet                                 = 0xF;
        work->gridBody.flags                                      = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        _animDriverTick(arg1);
        work->stateFrame = 0;
        work->missedLunges++;
    }
    _animDriverTick(arg1);
    if (work->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ACTOR_04000_STATE_CHASE;
    }
    if (work->missedLunges >= 4) {
        work->state = ACTOR_04000_STATE_SELF_BURST;
    }
}

/// `HIDDEN` of the actor's per-frame dispatch: on the frame the state is
/// entered (`stateEntered` latch) it marks the enemy not lockable, raises the
/// display object's 0x80 bit, and clears the gate bits of the work block's
/// four collision objects (the high bit on three, 0x4000 on `gridBody`).
static void Actor04000_Fn06A5C(Enemy* enemy, Task* task)
{
    _Actor04000Work* work;
    TmdObject*       obj;

    work = task->work;
    if (work->stateEntered != 0) {
        obj                           = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                    = (u16)(obj->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW);
        work->hitBody.flags           = (u16)(work->hitBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->burstAttackBody.flags   = (u16)(work->burstAttackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->burstWaveBody.flags     = (u16)(work->burstWaveBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
    }
}

static void Actor04000_Fn06AC4(Enemy* arg0, Task* arg1)
{
    _Actor04000Work* work;
    TmdObject*       obj;

    work = arg1->work;
    obj  = arg1->extra.tmd;
    if (work->stateEntered != 0) {
        arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->driver.requestedSet    = 1;
        work->driver.state           = ANIM_DRIVER_STATE_RESTART_2;
        work->hitBody.flags         &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        return;
    }
    _animDriverTick(arg1);
    switch (arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) {
        case 6:
        case 7:
            arg0->node.state.parts.flags = 0;
            obj->flags                   = 0;
            break;
        case 3:
        case 4:
        case 5:
        default:
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            obj->flags                   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
    if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
        work->state = ACTOR_04000_STATE_PATROL;
    }
}

static void Actor04000_Fn06BC8(Enemy* arg0, Task* arg1)
{
    _Actor04000Work* work;
    TmdObject*       obj;

    work = arg1->work;
    if (work->stateEntered != 0) {
        obj                          = arg1->extra.tmd;
        arg0->node.state.parts.flags = 0;
        obj->flags                   = 0;
        work->driver.requestedSet    = 4;
        work->driver.state           = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias        = 0;
        work->hitBody.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _animDriverTick(arg1);
        return;
    }
    _animDriverTick(arg1);
    if (work->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ACTOR_04000_STATE_IDLE;
    }
}

static void Actor04000_Fn06C80(Enemy* arg0, Task* arg1)
{
    _Actor04000Work* work;
    TmdObject*       obj;

    work = arg1->work;
    if (work->stateEntered != 0) {
        obj                          = arg1->extra.tmd;
        arg0->node.state.parts.flags = 0;
        obj->flags                   = 0;
        work->driver.requestedSet    = 6;
        work->driver.state           = ANIM_DRIVER_STATE_RESTART_1;
        work->driver.rateBias        = 0;
        work->hitBody.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _animDriverTick(arg1);
        return;
    }
    _animDriverTick(arg1);
    if (work->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ACTOR_04000_STATE_PATROL;
    }
}

static void Actor04000_Fn06D38(Enemy* arg0, Task* arg1)
{
    _Actor04000Work* work;
    s16              angle;

    work = arg1->work;
    if (work->stateEntered != 0) {
        arg1->extra.tmd->flags       = 0;
        arg0->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
        work->driver.requestedSet    = 1;
        work->driver.state           = ANIM_DRIVER_STATE_RESTART_2;
        work->driver.rateBias        = 0;
        work->hitBody.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->burstAttackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->burstWaveBody.flags   &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _animDriverTick(arg1);
        angle = ratan2(-arg1->extra.tmd->coords->coord.m[2][0], arg1->extra.tmd->coords->coord.m[2][2]);
        gfxRotMatrixZ(&arg1->extra.tmd->coords->coord, 0x800, GRAPHICS_ROTATION_REPLACE);
        gfxRotMatrixY(&arg1->extra.tmd->coords->coord, angle, 0);
        arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        return;
    }
    _animDriverTick(arg1);
}

/// The enemy's spawn, per-frame and teardown handlers, indexed by the task's
/// state.
static const EnemyTaskFuncTable3 Actor04000_D00240 = {
    Actor04000_Fn010B8,
    Actor04000_Fn05F0C,
    enemyDestroy,
};

/// The enemy task's callback: runs the handler for the task's current state,
/// copying the table onto the stack before the call.
void Actor04000_Fn06E4C(Task* task)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor04000_D00240;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}

void Actor04000_Fn06EA8(Task* arg0)
{
    ActorCommand msg;
    s16          i;

    for (i = 0; i < 6; i++) {
        Actor04000_D0C718[i] = NULL;
    }
    Actor04000_D0C710[1]  = NULL;
    msg.context.loc.stage = 3;
    msg.context.loc.area  = 0x10;
    Actor04000_D0C710[0]  = NULL;
    msg.command           = 1;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
    arg0->state++;
}

void Actor04000_Fn06F54(Task* arg0)
{
    s16 i;

    if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
        for (i = 0; i < 6; i++) {
            if (Actor04000_D0C718[i] != NULL) {
                ((Enemy*)Actor04000_D0C718[i]->spawnArg2.pointer)->node.state.parts.flags = 0;
            }
        }
        arg0->state++;
    }
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view == 5) {
        for (i = 0; i < 6; i++) {
            if (Actor04000_D0C718[i] != NULL) {
                sceneEngageBattle(1);
                return;
            }
        }
    }
}

void Actor04000_Fn0703C(Task* arg0)
{
    Actor04000_D0C6EC[arg0->state](arg0);
}
