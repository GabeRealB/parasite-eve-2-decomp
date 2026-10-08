#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>
#include <psyq/stdio.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "actors/actor_210600.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
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
#include "main/display.h"
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
#include "../../shared/player_detection.h"
#include "../../shared/actor_contacts.h"
#include "../../shared/boss_stranger.h"

/// Scratch-stack block the patrol layout works in while it places the walker's
/// nav nodes.
///
/// Node 0 is the walker's own position. Each node after it lies one further
/// turn about Y round the walker: `rotation` is turned by the layout's angle
/// once per node, and `offset` is its facing axis times the layout's scale, of
/// 4096. One block serves one layout and is released before it returns.
/// Angles are 4096ths of a turn.
typedef struct {
    SVECTOR offset;    // Node's offset from the walker: the facing axis of `rotation`, then that axis scaled; `pad` is never written
    MATRIX  rotation;  // Copy of the walker coordinate's matrix, turned about Y once more for each node; only its rotation is read
    s16     nodeIndex; // Node being placed, from 1; then the route step being seeded, from 0, left on the end marker's slot
} _Actor110600PatrolLayoutScratch;
STATIC_ASSERT_SIZEOF(_Actor110600PatrolLayoutScratch, 0x2C);

/// Values of `_Actor110600Work::state`: the index of the handler the per-frame
/// tick runs.
///
/// Five numbers have no handler. `RESTORED` is the only one of them that is
/// ever stored; what replaces it before the tick dispatches on it is not in
/// this package. Nothing here stores the states marked "never selected",
/// which leaves `RISE_BACK`, `RISE` and `DOWN` unreachable as well.
enum {
    ACTOR_110600_STATE_HIDDEN       = 0x00, // not drawn, not lockable, `attackBody` and `gridBody` off
    ACTOR_110600_STATE_RESTORED     = 0x01, // set at spawn for an enemy with a saved spawn state; has no handler
    ACTOR_110600_STATE_PATROL       = 0x02, // walks the two `navNodes` until the player comes within a notice range
    ACTOR_110600_STATE_CHASE        = 0x03, // walks at the player, each footstep shaking the screen; attacks when close
    ACTOR_110600_STATE_ALERT        = 0x04, // plays animation 0x15 with the look following the player, then chases
    ACTOR_110600_STATE_ATTACK       = 0x05, // swings at the player with `attackBody` enabled, then chases
    ACTOR_110600_STATE_IDLE         = 0x06, // stands on animation 0x18, now and then playing 0xE; a hit alerts it
    ACTOR_110600_STATE_FALL_BACK    = 0x07, // backs away through animations 0x1D and 0x1E, then `DOWN` or, dead, `DEATH_BURN`; never selected
    ACTOR_110600_STATE_FALL         = 0x08, // edges forward through animation 0xC, then `DOWN` or, dead, `DEATH_BURN`; never selected
    ACTOR_110600_STATE_RISE_BACK    = 0x09, // plays animation 0xF after a `DOWN` that followed `FALL_BACK`, then chases
    ACTOR_110600_STATE_RISE         = 0x0A, // plays animation 0x10 after a `DOWN` that followed `FALL`, then chases
    ACTOR_110600_STATE_DOWN         = 0x0B, // holds its pose for `downFrames`, then rises
    ACTOR_110600_STATE_DEATH_BURN   = 0x0C, // burns away: the corpse-burn effect, then the model flattens, fades and stops being drawn
    ACTOR_110600_STATE_DEATH_BURST  = 0x0D, // stops drawing the model and throws five body parts from it; never selected
    ACTOR_110600_STATE_STATUS_HOLD  = 0x0E, // twitches in place until the status buildup runs out, then chases
    ACTOR_110600_STATE_UNUSED_0F    = 0x0F, // has no handler and is never selected
    ACTOR_110600_STATE_UNUSED_10    = 0x10, // has no handler and is never selected
    ACTOR_110600_STATE_SCRIPTED     = 0x11, // plays the animation a room command or a play request put in `animId`
    ACTOR_110600_STATE_UNUSED_12    = 0x12, // has no handler and is never selected
    ACTOR_110600_STATE_DEATH_THRASH = 0x13, // turns in random jerks through animation 0x16, plays 0x21, then `DEATH_BURN`; never selected
    ACTOR_110600_STATE_LURK         = 0x14, // holds animation 0x22 until the player comes within 3000 or a hit alerts it; entered by a patio command
    ACTOR_110600_STATE_LURK_ALERT   = 0x15, // the alert that ends `LURK`: not lockable, then chases
    ACTOR_110600_STATE_SHUDDER      = 0x16, // jolts the root from side to side over five ticks, then chases; never selected
    ACTOR_110600_STATE_ALERT_REWIND = 0x17, // runs animation 0x15 forward 20 ticks on entry, then plays it backward until another state is set; entered by a cafeteria command
    ACTOR_110600_STATE_ENRAGE       = 0x18, // convulses while `enrageTint` builds, then chases at a faster `baseRate`; entered once, by the hit that takes it below half its hit points
    ACTOR_110600_STATE_COUNT                // number of states, and of the handlers in `_Actor110600StateTable`
};

/// Values of `_Actor110600Work::animRequest` and `_Actor110600Work::blendRequest`.
///
/// A zero-filled block holds 0, on which the driver only advances the slots.
/// The blend rig is only ever asked to reset.
enum {
    ACTOR_110600_ANIM_REQUEST_BLEND   = 1, // seek the slots to the animation, blending over the frames its transition table gives
    ACTOR_110600_ANIM_REQUEST_RESET   = 2, // restart the slots on the animation
    ACTOR_110600_ANIM_REQUEST_PLAYING = 3, // the request has been applied
    ACTOR_110600_ANIM_REQUEST_SETTLE  = 6  // restart the slots on the animation and run them 99 ticks before it shows; never requested
};

/// Scripted clip slots in this actor's animation-set table.
///
/// Slots 34..37 are the four adjacent writable animation-set pointers; the
/// commands below select their bindings. Slot 40 is play-request selector 4.
enum {
    ACTOR_110600_ANIM_SCRIPTED_SLOT0    = 0x22,
    ACTOR_110600_ANIM_SCRIPTED_SLOT1    = 0x23,
    ACTOR_110600_ANIM_SCRIPTED_SLOT2    = 0x24,
    ACTOR_110600_ANIM_SCRIPTED_SLOT3    = 0x25,
    ACTOR_110600_ANIM_SCRIPTED_REQUEST4 = 0x28,
};

/// Clip shared by the ordinary alert and the wake-up from lurk.
enum { ACTOR_110600_ANIM_ALERT = 21 };

/// Clips used by the patrol, knockdown and status-hold states.
enum {
    ACTOR_110600_ANIM_PATROL         = 2,
    ACTOR_110600_ANIM_ATTACK         = 5,
    ACTOR_110600_ANIM_FALL           = 12,
    ACTOR_110600_ANIM_IDLE_VARIATION = 14,
    ACTOR_110600_ANIM_RISE_BACK      = 15,
    ACTOR_110600_ANIM_RISE           = 16,
    ACTOR_110600_ANIM_IDLE           = 24,
    ACTOR_110600_ANIM_FALL_BACK      = 29,
    ACTOR_110600_ANIM_FALL_BACK_END  = 30,
};

/// Actor-specific message slots; both payload words are ignored.
enum {
    ACTOR_110600_MESSAGE_ALERT  = 2007,
    ACTOR_110600_MESSAGE_IGNORE = 2015,
};

/// Forces a message-selected state to run its entry setup on the next tick.
enum { ACTOR_110600_PREV_STATE_NONE = -1 };

/// Work block of the Boss Stranger task.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`; the
/// exit callback unlinks its three collision bodies. It holds the state
/// machine, both animation rigs and their driver's state, the bodies with
/// their contact records, storage for the model's matrices, the walker that
/// moves the root and the two-node patrol it walks.
///
/// Angles are 4096ths of a turn. Animation ids index the package's animation
/// bank; rates are sixteenths of a frame per tick. A cue index is the low ten
/// bits of the record a slot's current pose names
/// (`ANIMATION_POSE_CUE_INDEX_MASK`).
typedef struct {
    s16                   state;                // `ACTOR_110600_STATE_*`
    s16                   prevState;            // `state` the tick last ran; -1 makes the next tick enter `state` afresh
    s16                   stateEntered;         // 1 on the first tick of a state, when the handler sets itself up (0 otherwise)
    s16                   downFrames;           // ticks `DOWN` has left to wait; a random 0..31 on entry
    s16                   placedYaw;            // heading of the root after the last placement message; never read
    byte                  pad_A[0x2];           // never accessed
    u16                   noticeRangeAhead;     // distance at which `PATROL` notices a player within 1000 angle units of its facing
    u16                   noticeRangeAround;    // distance at which `PATROL` notices the player in any direction
    ActorAnimRig19        rig;                  // playback of the model's parts; slot 1's status and cue index time the states
    ActorAnimRig19        blendRig;             // second playback of the same model, mixed into slots 1 to 10 while `blendActive`
    s32                   lastFootstepCueIndex; // slot 1's cue index on the last `CHASE` tick, so each footstep starts its shake once
    s16                   animRequest;          // `ACTOR_110600_ANIM_REQUEST_*` for `rig`
    s16                   blendActive;          // 1 while `blendRig`'s animation is mixed in; cleared when its slot 1 reaches its boundary
    s16                   appliedAnim;          // animation `rig` was last started on; the row of the transition table a blend request reads
    s16                   animId;               // animation requested of `rig`
    s16                   animFrames;           // ticks since `animRequest` was last applied; `DEATH_THRASH` ends its first animation at 31
    s16                   animRate;             // playback rate of `rig`'s slots; negative plays backward
    s16                   baseRate;             // `animRate` most states start on: 20 or 16 by variant, 0x38 once `ENRAGE` has run
    s16                   blendRequest;         // `ACTOR_110600_ANIM_REQUEST_*` for `blendRig`
    s16                   blendAnimId;          // animation requested of `blendRig`; a hit asks for 0xB
    s16                   blendRate;            // playback rate of `blendRig`'s slots, 0x30 from each restart
    s16                   blendWeight;          // share of `blendRig`'s pose in the mix, of 0x1000; 0xB78 from each restart
    s16                   lookYawTarget;        // bearing to the player relative to the facing, or 0
    s16                   lookYaw;              // eased toward `lookYawTarget` by 0x100 a tick; within +-0x400, part 5 turns by it and part 3 by a quarter
    s16                   burnHeightScale;      // height scale `DEATH_BURN` flattens the root from: the walker's scale on entry, 8 less a tick from tick 201 while above 0x800
    byte                  pad_8A8[0x2];         // never accessed
    s16                   hitCooldown;          // ticks before another hit is taken; set from the hit's id parameter 2
    s32                   lastCueIndex;         // slot 1's cue index (slot 14's or 18's on animation 3) as the sound cues last saw it, so a held cue fires once; 0 from each applied request
    s8                    field_8B0;            // cleared at spawn and never accessed again; role unproven
    s8                    rootDirty;            // 1 when the tick left the root coordinate marked dirty, which a ready view forces; never read
    byte                  pad_8B2[0x2];         // never accessed
    s16                   flinchSpeedScale;     // sets the `CHASE` step while `blendActive`: this times `animRate` over 1520, halved, so 0 at the starting rates
    s16                   walkSpeed;            // forward step per tick at rate 16 (10, 8 or 14 by variant): `PATROL` walks at it, `CHASE` at it times `animRate` over 16
    WorldCollisionBody    hitBody;              // sphere ahead of and above the root that takes the hits, off in `HIDDEN` and the death states; moved onto part 2 by the killing hit
    WorldCollisionContact hitContacts[5];       // contacts of `hitBody`; also the enemy's hit records and what the walker avoids
    WorldCollisionBody    gridBody;             // sphere 0x124 above the root, in world space, that the room grid pushes the walker with; also takes hits
    WorldCollisionContact gridContacts[12];     // contacts of `gridBody`, which the walker's ground step measures
    WorldCollisionBody    attackBody;           // sphere on part 3 carrying the key of the attack in use; enabled only between two cue indices of `ATTACK`
    WorldCollisionContact attackContacts[1];    // contact of `attackBody`; a kind-0x10000 record is a landed swing, which disables the body
    MATRIX                lightMtx;             // storage for the model's `TmdObject::lightMtx`
    MATRIX                colorMtx;             // storage for the model's `TmdObject::colorMtx`
    MATRIX                burnColorMtx;         // `colorMtx` as tick 230 of `DEATH_BURN` found it; each later tick scales a fresh copy further down
    BossStrangerWalker    walker;               // moves and turns the root: patrols `navNodes`, chases the player or only ramps its speed
    BossStrangerNode      navNodes[2];          // storage for the walker's nav nodes: the root's position at spawn and a point 1500 to 5000 from it, by variant
    u8                    navNodeOrder[4];      // storage for the walker's node order; two entries are written
    u8                    routeNodeIndices[4];  // storage for the walker's patrol route; two indices and the end marker are written
    Task*                 childTask0;           // advanced one task state by the exit callback when set; nothing sets it
    Task*                 childTask1;           // advanced one task state by the exit callback when set; nothing sets it
    struct {
        u8 stage;                               // stage tag of the command's context
        u8 area;                                // area tag of the command's context
        u8 command;                             // low byte of the command word
    } lastCommand;                              // the last actor command received, recorded whether or not it was applied; `SCRIPTED` acts on the cafeteria's 3 and 6
    s16  stateFrame;                            // tick counter of `CHASE`, `ATTACK`, `DEATH_BURN` and the second stage of `ENRAGE`, cleared as each begins
    s16  enrageStage;                           // 0 while `ENRAGE` plays up to cue index 4, 1 while it convulses
    s16  enrageTint;                            // taken off the green and blue background colour of `colorMtx` each tick, and two thirds of it off the red; grows by 0x27 a tick of `ENRAGE`'s second stage
    s16  enraged;                               // 1 once `ENRAGE` has been entered: hits do half damage, and `ATTACK` plays animation 4 with attack 0 in place of animation 5 with attack 1; cleared with the tint when a dead actor is asked whether it is present
    s16  footstepShake;                         // nonzero while a footstep's five-tick screen shake runs (1 or 2 by the cue index that started it)
    byte pad_BEA[0x2];                          // never accessed
} _Actor110600Work;
STATIC_ASSERT_SIZEOF(_Actor110600Work, 0xBEC);

/// Stops steering and starts an idle speed ramp from the current step.
///
/// Standalone statement sequence requiring a braced context and arguments free
/// of side effects. `actorWork` is live work, `walkerAlias` is a writable walker
/// pointer and `savedSpeed` a writable u16. Target and step values narrow to
/// unsigned halfword game units per tick. Arguments are evaluated repeatedly;
/// current speed is retained and turn/look are cleared. The walker compares
/// speed gaps as signed halfwords and applies target 0xFFFE as a step of -2.
#define ACTOR_110600_ENTER_IDLE_WALKER(actorWork, walkerAlias, savedSpeed, targetValue, stepValue) \
    (savedSpeed)                  = (actorWork)->walker.speed;                                     \
    (walkerAlias)                 = &(actorWork)->walker;                                          \
    (actorWork)->walker.state     = BOSS_STRANGER_WALKER_IDLE;                                     \
    (walkerAlias)->speedTarget    = (targetValue);                                                 \
    (walkerAlias)->speed          = (savedSpeed);                                                  \
    (walkerAlias)->speedStep      = (stepValue);                                                   \
    (actorWork)->walker.turnLimit = 0;                                                             \
    (actorWork)->lookYaw          = 0;                                                             \
    (actorWork)->lookYawTarget    = 0

/// Scratch-stack block of the damage step, which runs each tick the actor has
/// health left and no hit cooldown running, and reserves a block only while
/// the player is alive.
///
/// The step looks for a damaging contact, kind 0x20000, on the hit body and
/// then on the grid body. When it finds one it rolls the damage for the
/// player's range, scales it, takes it off the enemy's health and works out
/// which way the hit lies from the facing. The damage-over-time tick that
/// follows reuses `damage` alone. The block is released before the step
/// returns. Angles are 4096ths of a turn.
typedef struct {
    VECTOR  toPlayer;       // Player's position minus the root's; never read back, and `pad` is never written
    SVECTOR hitOffset;      // `hitPos` minus the root's composed translation; `vx` and `vz` give the hit's bearing, and `pad` is never written
    SVECTOR hitPos;         // Point of the contact found; `pad` is never written
    s32     hitKey;         // Key of the contact found: the kind over the attack's packed id; 0 when neither body holds a damaging contact
    u32     damage;         // Damage of the hit: the roll for the range, times 5 on a critical roll, doubled when `hitYaw` reads beyond 0x500 either way and halved once enraged; then the damage of the over-time tick
    s32     playerDistance; // Length of `toPlayer`, the range the damage is rolled for; never read back
    s16     hitYaw;         // Bearing of `hitOffset` off the facing, wrapped to [-0x800, 0x800]. The doubling test reads it before this hit's bearing is stored, so it sees whatever the block's bytes held when it was reserved
} _Actor110600HitScratch;
STATIC_ASSERT_SIZEOF(_Actor110600HitScratch, 0x30);

/// The actor's state handlers, indexed by `_Actor110600Work::state`.
///
/// The package defines one table. The per-frame tick copies it to the stack
/// before calling the entry of the current state. The call is unconditional,
/// so the `NULL` entries of `ACTOR_110600_STATE_RESTORED` and the three
/// `ACTOR_110600_STATE_UNUSED_*` values mark states the actor must not be in
/// when the tick dispatches.
typedef struct {
    TaskFunc handlers[ACTOR_110600_STATE_COUNT]; // Handler of each state, taking the actor's task
} _Actor110600StateTable;
STATIC_ASSERT_SIZEOF(_Actor110600StateTable, ACTOR_110600_STATE_COUNT * sizeof(TaskFunc));

static const _Actor110600StateTable D_actor_110600_80131F3C;

// Animation sets supplied by the paired actor overlay.

/// Twelve `SVECTOR` hit positions `_actor110600SpawnHitEffect` picks from by
/// relative hit yaw. The fourth halfword (`pad`, unused by the effect) is the
/// model part index the spawned effect anchors to.
extern SVECTOR D_actor_110600_801485C4[12];

/// The actor's four display slots, which the 0x401 events repoint at one of the
/// objects above; `D_actor_110600_80148598` is the one events 2 and 6 swap.
extern AnimationSet* D_actor_110600_80148594;
extern AnimationSet* D_actor_110600_80148598;
extern AnimationSet* D_actor_110600_8014859C;
extern AnimationSet* D_actor_110600_801485A0;

static s32 _actor110600ApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 secondArg);

static s32 _actor110600PlayScriptedAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 secondArg);

static void _actorRenderSetYawAxisScales(GfxCoord* coord, s16 scaleX, s16 scaleY, s16 scaleZ);

static s32  _actor110600Alert(Task* task, s32 messageId, s32 firstArg, s32 secondArg);
static void _actor110600HiddenState(Task* task);

static void _actor110600ChaseState(Task* task);
static void _actor110600DeathBurnState(Task* task);
static void func_actor_110600_801372CC(Task* arg0);
static void _actor110600LurkState(Task* task);
static void _actor110600FallState(Task* task);
static void _actor110600DownState(Task* task);
static void _actor110600RiseBackState(Task* task);
static void _actor110600RiseState(Task* task);

static void _actor110600TickAnimation(Task* task);

static s32 _actor110600PollAnimationSound(_Actor110600Work* work);

static void _actor110600AlertState(Task* task);

static void _actor110600SpawnHitEffect(Task* task, s16 hitYaw, s32 attackKey);

static void _actor110600AlertRewindState(Task* task);

static void _actor110600StatusHoldState(Task* task);

static s32 _actor110600Place(Task* task, s32 messageId, const ActorTransform* placement, s32 secondArg);

/// Five-frame shake counter. Incremented each call, wraps at 5, and drives
/// `displaySetShakeY` with the low bit (0 or 1). Returns 1 on wrap.
extern s16 D_actor_110600_8014865C;
static s32 func_actor_110600_80138900(void);

/// Allocation holding the step counter of `ACTOR_110600_STATE_SHUDDER`.
///
/// Six zero bytes separate the counter from the contact scratch position that
/// follows it in the image. No access to them is recovered, so whether they
/// are alignment, trailing fields of this object or a separate unreferenced
/// variable is unproven; they stay in this allocation only to keep the data
/// after it at its address.
typedef struct {
    s16 step;         // Jolts made so far: cleared as the state is entered, 0 to 4 pick the sideways push of the tick, 5 hands over to `ACTOR_110600_STATE_CHASE`
    u8  unknown_2[6]; // Zero in the image; no access established and role unproven
} _Actor110600ShudderStepStorage;
STATIC_ASSERT_SIZEOF(_Actor110600ShudderStepStorage, 8);

/// Step counter of the `ACTOR_110600_STATE_SHUDDER` handler, in its static
/// allocation.
extern _Actor110600ShudderStepStorage D_actor_110600_80148688;

/// Argument record `_actor110600SpawnHitEffect` passes to `effectSpawnHit`:
/// model part 1's coordinate, low spawn argument 0x100 and high argument 3.
/// The selected effect kind determines whether the high argument is a count.
extern EffectSpawnArg D_actor_110600_80148698;

static void _actor110600Exit(Task* task);

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

/// Applies a rotation of `angle` about Y to `matrix` (main executable).

/// The complaint the route re-plan prints when the two node lists share no
/// slot at all. The string is spelled out rather than left a literal so the
/// re-plan reaches it by name, the way the original object does.
static const char _gPatrolNoPairMsg[] = "s->root_cnt == 0xff about \n";

static s32  _actor110600SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 secondArg);
static s32  _actor110600IsPresent(Task* task, s32 messageId, s32 firstArg, s32 secondArg);
static void _actor110600IgnoreMessage2015(Task* task, s32 messageId, s32 firstArg, s32 secondArg);

static TmdSource _gActor110600StrangerBody;
static void      _actor110600Task(Task* task);

DamageAttack D_actor_110600_80138F04[2] = {
    { 18, 7 },
    { 15, 7 },
};

DamageAttack D_actor_110600_80138F0C[2] = {
    { 18, 7 },
    { 18, 0 },
};

EnemyParams D_actor_110600_80138F14 = { D_actor_110600_80138F04, 350, 300, 200, 30, 100, 4, 100, 0 };

EnemyParams D_actor_110600_80138F24 = { D_actor_110600_80138F0C, 30, 42, 82, 4, 250, 0, 100, 0 };

u16 D_actor_110600_80138F34[18] = {
    20,
    900,
    12,
    2000,
    0,
    0,
    10,
    800,
    12,
    2500,
    0,
    0,
    0,
    500,
    12,
    3000,
    0,
    0,
};

static TmdBone _gActor110600StrangerBodySkeleton[19] = {
#include "assets/stranger_body_skeleton.inc"
};

static u32 _gActor110600StrangerBodyPartVerts[19] = {
#include "assets/stranger_body_partVerts.inc"
};

static SVECTOR _gActor110600StrangerBodyVerts[311] = {
#include "assets/stranger_body_verts.inc"
};

static SVECTOR _gActor110600StrangerBodyNormals[309] = {
#include "assets/stranger_body_normals.inc"
};

static u32 _gActor110600StrangerBodyStream[4027] = {
#include "assets/stranger_body_stream.inc"
};

static TmdSource _gActor110600StrangerBody = {
    0,
    20180,
    7860,
    19,
    _gActor110600StrangerBodyPartVerts,
    _gActor110600StrangerBodyVerts,
    _gActor110600StrangerBodyNormals,
    _gActor110600StrangerBodySkeleton,
    _gActor110600StrangerBodyStream,
};

static AnimationPackedPose _gActor110600Animation0D63CBank1[29] = {
#include "assets/actor_110600_animation_0D63C_bank1.inc"
};

static AnimationPackedRotation _gActor110600Animation0D63CBank4[374] = {
#include "assets/actor_110600_animation_0D63C_bank4.inc"
};

static AnimationRecord _gActor110600Animation0D63CRecords[528] = {
#include "assets/actor_110600_animation_0D63C_records.inc"
};

static u16 _gActor110600Animation0D63CIndices[20] = {
#include "assets/actor_110600_animation_0D63C_indices.inc"
};

static AnimationSet _gActor110600Animation0D63C = {
    _gActor110600Animation0D63CRecords,
    _gActor110600Animation0D63CIndices,
    { NULL, _gActor110600Animation0D63CBank1, NULL, NULL, _gActor110600Animation0D63CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor110600Animation0E664Bank1[27] = {
#include "assets/actor_110600_animation_0E664_bank1.inc"
};

static AnimationPackedRotation _gActor110600Animation0E664Bank4[406] = {
#include "assets/actor_110600_animation_0E664_bank4.inc"
};

static AnimationRecord _gActor110600Animation0E664Records[527] = {
#include "assets/actor_110600_animation_0E664_records.inc"
};

static u16 _gActor110600Animation0E664Indices[20] = {
#include "assets/actor_110600_animation_0E664_indices.inc"
};

static AnimationSet _gActor110600Animation0E664 = {
    _gActor110600Animation0E664Records,
    _gActor110600Animation0E664Indices,
    { NULL, _gActor110600Animation0E664Bank1, NULL, NULL, _gActor110600Animation0E664Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor110600Animation0F0D4Bank1[24] = {
#include "assets/actor_110600_animation_0F0D4_bank1.inc"
};

static AnimationPackedRotation _gActor110600Animation0F0D4Bank4[248] = {
#include "assets/actor_110600_animation_0F0D4_bank4.inc"
};

static AnimationRecord _gActor110600Animation0F0D4Records[328] = {
#include "assets/actor_110600_animation_0F0D4_records.inc"
};

static u16 _gActor110600Animation0F0D4Indices[20] = {
#include "assets/actor_110600_animation_0F0D4_indices.inc"
};

static AnimationSet _gActor110600Animation0F0D4 = {
    _gActor110600Animation0F0D4Records,
    _gActor110600Animation0F0D4Indices,
    { NULL, _gActor110600Animation0F0D4Bank1, NULL, NULL, _gActor110600Animation0F0D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor110600Animation0F6B4Bank1[10] = {
#include "assets/actor_110600_animation_0F6B4_bank1.inc"
};

static AnimationPackedRotation _gActor110600Animation0F6B4Bank4[138] = {
#include "assets/actor_110600_animation_0F6B4_bank4.inc"
};

static AnimationRecord _gActor110600Animation0F6B4Records[188] = {
#include "assets/actor_110600_animation_0F6B4_records.inc"
};

static u16 _gActor110600Animation0F6B4Indices[20] = {
#include "assets/actor_110600_animation_0F6B4_indices.inc"
};

static AnimationSet _gActor110600Animation0F6B4 = {
    _gActor110600Animation0F6B4Records,
    _gActor110600Animation0F6B4Indices,
    { NULL, _gActor110600Animation0F6B4Bank1, NULL, NULL, _gActor110600Animation0F6B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor110600Animation0FC98Bank1[8] = {
#include "assets/actor_110600_animation_0FC98_bank1.inc"
};

static AnimationPackedRotation _gActor110600Animation0FC98Bank4[151] = {
#include "assets/actor_110600_animation_0FC98_bank4.inc"
};

static AnimationRecord _gActor110600Animation0FC98Records[182] = {
#include "assets/actor_110600_animation_0FC98_records.inc"
};

static u16 _gActor110600Animation0FC98Indices[20] = {
#include "assets/actor_110600_animation_0FC98_indices.inc"
};

static AnimationSet _gActor110600Animation0FC98 = {
    _gActor110600Animation0FC98Records,
    _gActor110600Animation0FC98Indices,
    { NULL, _gActor110600Animation0FC98Bank1, NULL, NULL, _gActor110600Animation0FC98Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor110600Animation10290Bank1[14] = {
#include "assets/actor_110600_animation_10290_bank1.inc"
};

static AnimationPackedRotation _gActor110600Animation10290Bank4[134] = {
#include "assets/actor_110600_animation_10290_bank4.inc"
};

static AnimationRecord _gActor110600Animation10290Records[186] = {
#include "assets/actor_110600_animation_10290_records.inc"
};

static u16 _gActor110600Animation10290Indices[20] = {
#include "assets/actor_110600_animation_10290_indices.inc"
};

static AnimationSet _gActor110600Animation10290 = {
    _gActor110600Animation10290Records,
    _gActor110600Animation10290Indices,
    { NULL, _gActor110600Animation10290Bank1, NULL, NULL, _gActor110600Animation10290Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor110600Animation10CE4Bank1[20] = {
#include "assets/actor_110600_animation_10CE4_bank1.inc"
};

static AnimationPackedRotation _gActor110600Animation10CE4Bank4[228] = {
#include "assets/actor_110600_animation_10CE4_bank4.inc"
};

static AnimationRecord _gActor110600Animation10CE4Records[353] = {
#include "assets/actor_110600_animation_10CE4_records.inc"
};

static u16 _gActor110600Animation10CE4Indices[20] = {
#include "assets/actor_110600_animation_10CE4_indices.inc"
};

static AnimationSet _gActor110600Animation10CE4 = {
    _gActor110600Animation10CE4Records,
    _gActor110600Animation10CE4Indices,
    { NULL, _gActor110600Animation10CE4Bank1, NULL, NULL, _gActor110600Animation10CE4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor110600Animation11A28Bank1[23] = {
#include "assets/actor_110600_animation_11A28_bank1.inc"
};

static AnimationPackedRotation _gActor110600Animation11A28Bank4[334] = {
#include "assets/actor_110600_animation_11A28_bank4.inc"
};

static AnimationRecord _gActor110600Animation11A28Records[426] = {
#include "assets/actor_110600_animation_11A28_records.inc"
};

static u16 _gActor110600Animation11A28Indices[20] = {
#include "assets/actor_110600_animation_11A28_indices.inc"
};

static AnimationSet _gActor110600Animation11A28 = {
    _gActor110600Animation11A28Records,
    _gActor110600Animation11A28Indices,
    { NULL, _gActor110600Animation11A28Bank1, NULL, NULL, _gActor110600Animation11A28Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor110600Animation12298Bank1[14] = {
#include "assets/actor_110600_animation_12298_bank1.inc"
};

static AnimationPackedRotation _gActor110600Animation12298Bank4[216] = {
#include "assets/actor_110600_animation_12298_bank4.inc"
};

static AnimationRecord _gActor110600Animation12298Records[262] = {
#include "assets/actor_110600_animation_12298_records.inc"
};

static u16 _gActor110600Animation12298Indices[20] = {
#include "assets/actor_110600_animation_12298_indices.inc"
};

static AnimationSet _gActor110600Animation12298 = {
    _gActor110600Animation12298Records,
    _gActor110600Animation12298Indices,
    { NULL, _gActor110600Animation12298Bank1, NULL, NULL, _gActor110600Animation12298Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor110600Animation12474Bank1[2] = {
#include "assets/actor_110600_animation_12474_bank1.inc"
};

static AnimationPackedRotation _gActor110600Animation12474Bank4[17] = {
#include "assets/actor_110600_animation_12474_bank4.inc"
};

static AnimationRecord _gActor110600Animation12474Records[76] = {
#include "assets/actor_110600_animation_12474_records.inc"
};

static u16 _gActor110600Animation12474Indices[20] = {
#include "assets/actor_110600_animation_12474_indices.inc"
};

static AnimationSet _gActor110600Animation12474 = {
    _gActor110600Animation12474Records,
    _gActor110600Animation12474Indices,
    { NULL, _gActor110600Animation12474Bank1, NULL, NULL, _gActor110600Animation12474Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor110600Animation12604Bank1[2] = {
#include "assets/actor_110600_animation_12604_bank1.inc"
};

static AnimationPackedRotation _gActor110600Animation12604Bank4[17] = {
#include "assets/actor_110600_animation_12604_bank4.inc"
};

static AnimationRecord _gActor110600Animation12604Records[57] = {
#include "assets/actor_110600_animation_12604_records.inc"
};

static u16 _gActor110600Animation12604Indices[20] = {
#include "assets/actor_110600_animation_12604_indices.inc"
};

static AnimationSet _gActor110600Animation12604 = {
    _gActor110600Animation12604Records,
    _gActor110600Animation12604Indices,
    { NULL, _gActor110600Animation12604Bank1, NULL, NULL, _gActor110600Animation12604Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor110600Animation12994Bank1[5] = {
#include "assets/actor_110600_animation_12994_bank1.inc"
};

static AnimationPackedRotation _gActor110600Animation12994Bank4[82] = {
#include "assets/actor_110600_animation_12994_bank4.inc"
};

static AnimationRecord _gActor110600Animation12994Records[111] = {
#include "assets/actor_110600_animation_12994_records.inc"
};

static u16 _gActor110600Animation12994Indices[20] = {
#include "assets/actor_110600_animation_12994_indices.inc"
};

static AnimationSet _gActor110600Animation12994 = {
    _gActor110600Animation12994Records,
    _gActor110600Animation12994Indices,
    { NULL, _gActor110600Animation12994Bank1, NULL, NULL, _gActor110600Animation12994Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor110600Animation131CCBank1[16] = {
#include "assets/actor_110600_animation_131CC_bank1.inc"
};

static AnimationPackedRotation _gActor110600Animation131CCBank4[199] = {
#include "assets/actor_110600_animation_131CC_bank4.inc"
};

static AnimationRecord _gActor110600Animation131CCRecords[259] = {
#include "assets/actor_110600_animation_131CC_records.inc"
};

static u16 _gActor110600Animation131CCIndices[20] = {
#include "assets/actor_110600_animation_131CC_indices.inc"
};

static AnimationSet _gActor110600Animation131CC = {
    _gActor110600Animation131CCRecords,
    _gActor110600Animation131CCIndices,
    { NULL, _gActor110600Animation131CCBank1, NULL, NULL, _gActor110600Animation131CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor110600Animation1371CBank1[10] = {
#include "assets/actor_110600_animation_1371C_bank1.inc"
};

static AnimationPackedRotation _gActor110600Animation1371CBank4[113] = {
#include "assets/actor_110600_animation_1371C_bank4.inc"
};

static AnimationRecord _gActor110600Animation1371CRecords[177] = {
#include "assets/actor_110600_animation_1371C_records.inc"
};

static u16 _gActor110600Animation1371CIndices[20] = {
#include "assets/actor_110600_animation_1371C_indices.inc"
};

static AnimationSet _gActor110600Animation1371C = {
    _gActor110600Animation1371CRecords,
    _gActor110600Animation1371CIndices,
    { NULL, _gActor110600Animation1371CBank1, NULL, NULL, _gActor110600Animation1371CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor110600Animation138FCBank1[2] = {
#include "assets/actor_110600_animation_138FC_bank1.inc"
};

static AnimationPackedRotation _gActor110600Animation138FCBank4[18] = {
#include "assets/actor_110600_animation_138FC_bank4.inc"
};

static AnimationRecord _gActor110600Animation138FCRecords[76] = {
#include "assets/actor_110600_animation_138FC_records.inc"
};

static u16 _gActor110600Animation138FCIndices[20] = {
#include "assets/actor_110600_animation_138FC_indices.inc"
};

static AnimationSet _gActor110600Animation138FC = {
    _gActor110600Animation138FCRecords,
    _gActor110600Animation138FCIndices,
    { NULL, _gActor110600Animation138FCBank1, NULL, NULL, _gActor110600Animation138FCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor110600Animation13CA0Bank1[10] = {
#include "assets/actor_110600_animation_13CA0_bank1.inc"
};

static AnimationPackedRotation _gActor110600Animation13CA0Bank4[73] = {
#include "assets/actor_110600_animation_13CA0_bank4.inc"
};

static AnimationRecord _gActor110600Animation13CA0Records[110] = {
#include "assets/actor_110600_animation_13CA0_records.inc"
};

static u16 _gActor110600Animation13CA0Indices[20] = {
#include "assets/actor_110600_animation_13CA0_indices.inc"
};

static AnimationSet _gActor110600Animation13CA0 = {
    _gActor110600Animation13CA0Records,
    _gActor110600Animation13CA0Indices,
    { NULL, _gActor110600Animation13CA0Bank1, NULL, NULL, _gActor110600Animation13CA0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor110600Animation14054Bank1[6] = {
#include "assets/actor_110600_animation_14054_bank1.inc"
};

static AnimationPackedRotation _gActor110600Animation14054Bank4[81] = {
#include "assets/actor_110600_animation_14054_bank4.inc"
};

static AnimationRecord _gActor110600Animation14054Records[118] = {
#include "assets/actor_110600_animation_14054_records.inc"
};

static u16 _gActor110600Animation14054Indices[20] = {
#include "assets/actor_110600_animation_14054_indices.inc"
};

static AnimationSet _gActor110600Animation14054 = {
    _gActor110600Animation14054Records,
    _gActor110600Animation14054Indices,
    { NULL, _gActor110600Animation14054Bank1, NULL, NULL, _gActor110600Animation14054Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor110600Animation142A4Bank1[4] = {
#include "assets/actor_110600_animation_142A4_bank1.inc"
};

static AnimationPackedRotation _gActor110600Animation142A4Bank4[45] = {
#include "assets/actor_110600_animation_142A4_bank4.inc"
};

static AnimationRecord _gActor110600Animation142A4Records[71] = {
#include "assets/actor_110600_animation_142A4_records.inc"
};

static u16 _gActor110600Animation142A4Indices[20] = {
#include "assets/actor_110600_animation_142A4_indices.inc"
};

static AnimationSet _gActor110600Animation142A4 = {
    _gActor110600Animation142A4Records,
    _gActor110600Animation142A4Indices,
    { NULL, _gActor110600Animation142A4Bank1, NULL, NULL, _gActor110600Animation142A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor110600Animation14500Bank1[4] = {
#include "assets/actor_110600_animation_14500_bank1.inc"
};

static AnimationPackedRotation _gActor110600Animation14500Bank4[47] = {
#include "assets/actor_110600_animation_14500_bank4.inc"
};

static AnimationRecord _gActor110600Animation14500Records[72] = {
#include "assets/actor_110600_animation_14500_records.inc"
};

static u16 _gActor110600Animation14500Indices[20] = {
#include "assets/actor_110600_animation_14500_indices.inc"
};

static AnimationSet _gActor110600Animation14500 = {
    _gActor110600Animation14500Records,
    _gActor110600Animation14500Indices,
    { NULL, _gActor110600Animation14500Bank1, NULL, NULL, _gActor110600Animation14500Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor110600Animation15ED8Bank1[55] = {
#include "assets/actor_110600_animation_15ED8_bank1.inc"
};

static AnimationPackedRotation _gActor110600Animation15ED8Bank4[612] = {
#include "assets/actor_110600_animation_15ED8_bank4.inc"
};

static AnimationRecord _gActor110600Animation15ED8Records[857] = {
#include "assets/actor_110600_animation_15ED8_records.inc"
};

static u16 _gActor110600Animation15ED8Indices[20] = {
#include "assets/actor_110600_animation_15ED8_indices.inc"
};

static AnimationSet _gActor110600Animation15ED8 = {
    _gActor110600Animation15ED8Records,
    _gActor110600Animation15ED8Indices,
    { NULL, _gActor110600Animation15ED8Bank1, NULL, NULL, _gActor110600Animation15ED8Bank4, NULL, NULL, NULL },
};

s8 D_actor_110600_80147D20[45][45] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 4, 4, 4, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 3, 8, 0, 0, 0, 0, 15, 5, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 4, 0, 0, 4, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 8, 5, 0, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 12, 12, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 8, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AnimationSet* D_actor_110600_8014850C[34] = {
    NULL,
    NULL,
    &_gActor110600Animation15ED8,
    NULL,
    &_gActor110600Animation0D63C,
    &_gActor110600Animation0E664,
    NULL,
    NULL,
    NULL,
    NULL,
    &_gActor110600Animation0F0D4,
    &_gActor110600Animation0F6B4,
    &_gActor110600Animation0FC98,
    &_gActor110600Animation10290,
    NULL,
    &_gActor110600Animation11A28,
    &_gActor110600Animation12298,
    &_gActor110600Animation12474,
    &_gActor110600Animation12604,
    NULL,
    NULL,
    &_gActor110600Animation131CC,
    &_gActor110600Animation1371C,
    NULL,
    NULL,
    NULL,
    NULL,
    &_gActor110600Animation138FC,
    NULL,
    &_gActor110600Animation13CA0,
    &_gActor110600Animation14054,
    &_gActor110600Animation142A4,
    &_gActor110600Animation14500,
    NULL,
};

AnimationSet* D_actor_110600_80148594 = &gActor210600Animation11F5C;

AnimationSet* D_actor_110600_80148598 = &gActor210600Animation12244;

AnimationSet* D_actor_110600_8014859C = &gActor210600Animation12B30;

AnimationSet* D_actor_110600_801485A0 = &gActor210600Animation1310C;

AnimationSet* D_actor_110600_801485A4[8] = {
    &_gActor110600Animation14054,
    &_gActor110600Animation12994,
    &gActor210600Animation134C8,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

SVECTOR D_actor_110600_801485C4[12] = {
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

TaskMessageEntry D_actor_110600_80148624[7] = {
    { ACTOR_110600_MESSAGE_IGNORE, _actor110600IgnoreMessage2015 },
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor110600PlayScriptedAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor110600SetModelDraw },
    { ACTOR_MESSAGE_IS_PRESENT, _actor110600IsPresent },
    { ACTOR_MESSAGE_PLACE, _actor110600Place },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor110600ApplyCommand },
    { ACTOR_110600_MESSAGE_ALERT, _actor110600Alert },
};

s16 D_actor_110600_8014865C = 0;

u16 D_actor_110600_80148660[8] = {
    704,
    0,
    64,
    256,
    0,
    271,
    256,
    1,
};

TaskDesc D_actor_110600_80148670 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, _actor110600Task, { .model = &_gActor110600StrangerBody } };

TaskDesc D_actor_110600_8014867C = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, _actor110600Task, { .model = &_gActor110600StrangerBody } };

_Actor110600ShudderStepStorage D_actor_110600_80148688 = { 0, { 0 } };

SVECTOR ActorContact_ScratchPosition = { 0, 0, 0, 0 };

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

EffectSpawnArg D_actor_110600_80148698 = { NULL, 0, 0 };

/// Whole-unit step `_actorContactApplyGridPushback` last applied to its coordinate.
extern SVECTOR ActorContact_ScratchPosition;

/// Reset argument `animationSeekSlotWithBlend` is handed for the clip `animId` of the
/// `appliedAnim` stage: the `0x2D`-byte row of the animation table this overlay's
/// data carries at `D_actor_110600_80147D20`, indexed by the clip id. The row
/// stride is the row's own length, so the load is a signed byte.
extern s8 D_actor_110600_80147D20[][0x2D];

extern EnemyParams D_actor_110600_80138F14;

extern AnimationSet* D_actor_110600_8014850C[];

extern TaskMessageEntry D_actor_110600_80148624[7];

static void _actor110600ApplyDamage(Task* task);

static void            _actor110600LayPatrolNodes(BossStrangerWalker* walker, s16 radius, s16 nodeYawStep);
static __inline__ void _actor110600RescaleRootYaw(Task* task, s16 uniformScale);
static void            _actor110600TickBlendedSlots(Task* task);
static __inline__ void _actor110600LinkAttackBody(WorldCollisionBody* body, GfxCoord* partCoord, WorldCollisionContact* contacts, const SVECTOR* localPosition, u16 bodyFlags);
static __inline__ void _actor110600InitWalkerScale(BossStrangerWalker* walker);
static void            _actor110600Spawn(Enemy* enemy, Task* task);
static void            _actor110600PatrolState(Task* task);
static __inline__ s32  _actor110600TickFootstepShake(void);
static __inline__ s32  _actor110600HasPlayerBodyContact(const WorldCollisionContact* contacts);
static void            _actor110600AttackState(Task* task);
static __inline__ s32  _actor110600FindAttackContact(SVECTOR* hitPoint, const WorldCollisionContact* contacts, s16 contactCount);
static void            _actor110600IdleState(Task* task);
static void            _actor110600FallBackState(Task* task);
static __inline__ void _actor110600ShrinkBurnRootYaw(Task* task, const _Actor110600Work* work, s16 heightScale);
static void            func_actor_110600_80136ECC(Task* arg0);
static void            _actor110600DeathThrashState(Task* task);
static void            _actor110600LurkAlertState(Task* task);
static void            _actor110600ShudderState(Task* task);
static void            _actor110600EnrageState(Task* task);
static void            func_actor_110600_80137F2C(Enemy* arg0, Task* arg1);

#include "../../shared/actor_contacts_turn_joint.inc.c"

#include "../../shared/actor_contacts_push_contact.inc.c"

#include "../../shared/boss_stranger_arrived.inc.c"

#include "../../shared/boss_stranger_follow_route.inc.c"

#include "../../shared/boss_stranger_nearest_player.inc.c"

#include "../../shared/boss_stranger_nearest_self.inc.c"

#include "../../shared/boss_stranger_plan_toward.inc.c"

#include "../../shared/boss_stranger_ground_step.inc.c"

#include "../../shared/boss_stranger_avoid_contacts.inc.c"

#include "../../shared/boss_stranger_turn_toward.inc.c"

/// Lays out the spawn-time patrol around the walker's current position.
///
/// Node 0 is the root's translation. Each later node adds the matrix's Q12
/// facing axis, turned by another `nodeYawStep`, times `radius` in coordinate
/// units. Angles use 4096 units per turn. Navigation and route storage must
/// provide `nodeCount` nodes/order entries and `nodeCount + 1` route bytes;
/// this actor supplies two nodes and a terminated route. Counts below two leave
/// all storage unchanged. Resets the route cursor, logs each added node and
/// releases its scratch block before returning.
static void _actor110600LayPatrolNodes(BossStrangerWalker* walker, s16 radius, s16 nodeYawStep)
{
    _Actor110600PatrolLayoutScratch* scratch;

    if (walker->nav->nodeCount < 2)
        return;
    scratch                   = SCRATCH_STACK_RESERVE_BLOCK(_Actor110600PatrolLayoutScratch);
    walker->nav->nodes[0].x   = (u16)walker->coord->coord.t[0];
    walker->nav->nodes[0].y   = (u16)walker->coord->coord.t[1];
    walker->nav->nodes[0].z   = (u16)walker->coord->coord.t[2];
    walker->nav->nodeOrder[0] = 0;
    // Place the remaining nodes by accumulating the turn about Y.
    scratch->rotation = walker->coord->coord;
    for (scratch->nodeIndex = 1; scratch->nodeIndex < walker->nav->nodeCount; scratch->nodeIndex++) {
        gfxRotMatrixY(&scratch->rotation, nodeYawStep, GRAPHICS_ROTATION_COMPOSE);
        gfxReadMatrixZAxis(&scratch->rotation, &scratch->offset);
        gte_lddp(radius);
        gte_ldsv(&scratch->offset);
        gte_gpf12();
        gte_stsv(&scratch->offset);
        walker->nav->nodes[scratch->nodeIndex].x   = (u16)walker->coord->coord.t[0] + scratch->offset.vx;
        walker->nav->nodes[scratch->nodeIndex].y   = (u16)walker->coord->coord.t[1] + scratch->offset.vy;
        walker->nav->nodes[scratch->nodeIndex].z   = (u16)walker->coord->coord.t[2] + scratch->offset.vz;
        walker->nav->nodeOrder[scratch->nodeIndex] = scratch->nodeIndex;
        printf("emc_m->tsv[%d]( %d, %d, %d )\n", scratch->nodeIndex, walker->nav->nodes[scratch->nodeIndex].x, walker->nav->nodes[scratch->nodeIndex].y, walker->nav->nodes[scratch->nodeIndex].z);
    }
    // Rebuild the route over all live nodes, followed by its end marker.
    walker->route->field_4 = 0;
    walker->route->cursor  = 0;
    for (scratch->nodeIndex = 0; scratch->nodeIndex < walker->nav->nodeCount; scratch->nodeIndex++) {
        walker->route->nodeIndices[scratch->nodeIndex] = scratch->nodeIndex;
    }
    walker->route->nodeIndices[scratch->nodeIndex] = OVERLAY_WALKER_ROUTE_END;
    SCRATCH_STACK_RELEASE_BLOCK(_Actor110600PatrolLayoutScratch);
}

#include "../../shared/boss_stranger_inlines.inc.c"

#include "../../shared/boss_stranger_tick.inc.c"

/// Rebuilds the actor root from its current yaw at a uniform signed Q12 scale.
///
/// Requires a live model root and initialized scratch storage. Discards pitch
/// and roll, preserves translation and marks composition dirty. `ONE` is full
/// scale; zero collapses the axes. The task-level stamp refreshes the root after
/// the rotation copy.
static __inline__ void _actor110600RescaleRootYaw(Task* task, s16 uniformScale)
{
    _actorRenderRescaleYaw(task->extra.tmd->coords, uniformScale);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Places the actor root and reapplies its walker's uniform scale.
///
/// Borrows a readable transform through synchronous dispatch: XYZ are in the
/// root parent's coordinate frame and rotations use 4096 units per turn.
/// Applies Rx * Ry * Rz, then keeps only the resulting yaw and records it in
/// `placedYaw`. Requires live actor work/model and initialized scratch storage.
/// Ignores the message ID and second payload; returns 1.
static s32 _actor110600Place(Task* task, s32 messageId, const ActorTransform* placement, s32 secondArg)
{
    _Actor110600Work* work;

    work = task->work;

    task->extra.tmd->coords->coord.t[0] = placement->pos.vx;
    task->extra.tmd->coords->coord.t[1] = placement->pos.vy;
    task->extra.tmd->coords->coord.t[2] = placement->pos.vz;
    gfxRotMatrixX(&task->extra.tmd->coords->coord, placement->rot.vx, GRAPHICS_ROTATION_REPLACE);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, placement->rot.vy, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixZ(&task->extra.tmd->coords->coord, placement->rot.vz, GRAPHICS_ROTATION_COMPOSE);
    _actor110600RescaleRootYaw(task, work->walker.scale);
    work->placedYaw = ratan2(-task->extra.tmd->coords->coord.m[2][0],
                             task->extra.tmd->coords->coord.m[2][2]);
    return 1;
}

/// Applies the actor's Acropolis patio or cafeteria command.
///
/// Borrows a readable four-byte command through dispatch and records its stage,
/// area and low command byte even when rejected. Patio command 1 enters LURK.
/// Cafeteria commands select a scripted clip/binding, ALERT_REWIND or DEATH_BURN;
/// accepted cafeteria commands reset state entry. The writable animation slots
/// and referenced clip data must stay loaded through playback. Ignores the ID
/// and second payload; returns 1 when accepted, 0 otherwise.
static s32 _actor110600ApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 secondArg)
{
    enum {
        ACTOR_110600_CONTEXT_PATIO                      = GAME_STAGE_ACROPOLIS | (GAME_AREA_ACROPOLIS_PATIO << 8),
        ACTOR_110600_CONTEXT_CAFETERIA                  = GAME_STAGE_ACROPOLIS | (GAME_AREA_ACROPOLIS_CAFETERIA << 8),
        ACTOR_110600_PATIO_COMMAND_LURK                 = 1,
        ACTOR_110600_CAFETERIA_COMMAND_ALERT_REWIND     = 1,
        ACTOR_110600_CAFETERIA_COMMAND_PLAY_SLOT1       = 2,
        ACTOR_110600_CAFETERIA_COMMAND_PLAY_SLOT2       = 3,
        ACTOR_110600_CAFETERIA_COMMAND_PLAY_SLOT3       = 4,
        ACTOR_110600_CAFETERIA_COMMAND_PLAY_SLOT0       = 5,
        ACTOR_110600_CAFETERIA_COMMAND_PLAY_SLOT1_ALT   = 6,
        ACTOR_110600_CAFETERIA_COMMAND_PLAY_SLOT3_ALIAS = 7,
        ACTOR_110600_CAFETERIA_COMMAND_DEATH_BURN       = 8,
        ACTOR_110600_CAFETERIA_COMMAND_PLAY_CLIP17      = 9,
        ACTOR_110600_ANIM_COMMAND9                      = 0x11,
    };

    _Actor110600Work* work = task->work;

    // Cache every received command, including contexts or actions we reject.
    work->lastCommand.stage   = command->context.loc.stage;
    work->lastCommand.area    = command->context.loc.area;
    work->lastCommand.command = command->command;
    if (command->context.key == ACTOR_110600_CONTEXT_PATIO) {
        if (command->command == ACTOR_110600_PATIO_COMMAND_LURK) {
            work->state = ACTOR_110600_STATE_LURK;
            return 1;
        }
        return 0;
    }
    if (command->context.key == ACTOR_110600_CONTEXT_CAFETERIA) {
        switch (command->command) {
            default:
                return 0;
            case ACTOR_110600_CAFETERIA_COMMAND_ALERT_REWIND:
                work->state     = ACTOR_110600_STATE_ALERT_REWIND;
                work->prevState = ACTOR_110600_PREV_STATE_NONE;
                return 1;
            case ACTOR_110600_CAFETERIA_COMMAND_PLAY_SLOT1:
                work->animId            = ACTOR_110600_ANIM_SCRIPTED_SLOT1;
                D_actor_110600_80148598 = &gActor210600Animation11F5C;
                break;
            case ACTOR_110600_CAFETERIA_COMMAND_PLAY_SLOT2:
                work->animId            = ACTOR_110600_ANIM_SCRIPTED_SLOT2;
                D_actor_110600_8014859C = &gActor210600Animation11F5C;
                break;
            case ACTOR_110600_CAFETERIA_COMMAND_PLAY_SLOT0:
                work->animId            = ACTOR_110600_ANIM_SCRIPTED_SLOT0;
                D_actor_110600_80148594 = &gActor210600Animation12B30;
                break;
            case ACTOR_110600_CAFETERIA_COMMAND_PLAY_SLOT1_ALT:
                work->animId            = ACTOR_110600_ANIM_SCRIPTED_SLOT1;
                D_actor_110600_80148598 = &gActor210600Animation134C8;
                break;
            case ACTOR_110600_CAFETERIA_COMMAND_PLAY_SLOT3:
            case ACTOR_110600_CAFETERIA_COMMAND_PLAY_SLOT3_ALIAS:
                work->animId            = ACTOR_110600_ANIM_SCRIPTED_SLOT3;
                D_actor_110600_801485A0 = &gActor210600Animation12244;
                break;
            case ACTOR_110600_CAFETERIA_COMMAND_DEATH_BURN:
                work->state     = ACTOR_110600_STATE_DEATH_BURN;
                work->prevState = ACTOR_110600_PREV_STATE_NONE;
                return 1;
            case ACTOR_110600_CAFETERIA_COMMAND_PLAY_CLIP17:
                work->animId = ACTOR_110600_ANIM_COMMAND9;
                break;
        }
        work->state     = ACTOR_110600_STATE_SCRIPTED;
        work->prevState = ACTOR_110600_PREV_STATE_NONE;
        return 1;
    }
    return 0;
}

#include "../../shared/player_detection_reach.inc.c"

/// Advances both rigs and blends their rotations over model parts 1..10.
///
/// Main slots 1..18 run at `animRate - 3` sixteenths of a frame per tick;
/// secondary slots 1..10 run at `blendRate`. Translation comes from the main
/// pose. `blendWeight` weights the main rotation, with `ONE - blendWeight`
/// weighting the secondary rotation, in Q12 units. Requires initialized rigs,
/// loaded clips with those tracks, a live nineteen-part model and scratch
/// storage for pose application. Slot 0 is untouched.
static void _actor110600TickBlendedSlots(Task* task)
{
    enum { ACTOR_110600_BLEND_PART_END       = 11,
           ACTOR_110600_BLEND_MAIN_RATE_BIAS = 3 };

    AnimationPose     mainPose;
    AnimationPose     overlayPose;
    AnimationContext* mainAnim;
    s16               mainWeight;
    s16               slotIndex;
    _Actor110600Work* work;

    work       = task->work;
    mainWeight = work->blendWeight;
    mainAnim   = &work->rig.anim;
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        if (slotIndex < ACTOR_110600_BLEND_PART_END) {
            work->blendRig.slots[slotIndex].rate = work->blendRate;
            work->rig.slots[slotIndex].rate      = (work->animRate - ACTOR_110600_BLEND_MAIN_RATE_BIAS);
            animationTickSlotPose(mainAnim, slotIndex, &mainPose, 0);
            animationTickSlotPose(&work->blendRig.anim, slotIndex, &overlayPose, 0);
            animationApplyPoseWithBlendedRotation(mainAnim, slotIndex, &mainPose, &overlayPose, mainWeight, ONE - mainWeight);
        } else {
            work->rig.slots[slotIndex].rate = (work->animRate - ACTOR_110600_BLEND_MAIN_RATE_BIAS);
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
}

/// Returns the sound script due at the active animation's pose cue, or zero.
///
/// Requires live work and initialized slots. Selects cues by `animId`, then
/// reads low-ten-bit pose record indices, not clip IDs. Patrol and most action
/// cues suppress a held record using `lastCueIndex`; chase observes parts 14
/// and 18, with part 18's separate suppression record retained. Returned IDs
/// name the Stranger bank with a zero instance byte; the driver adds the
/// enemy's placement index before requesting the sound.
static s32 _actor110600PollAnimationSound(_Actor110600Work* work)
{
    enum {
        ACTOR_110600_CUE_CLIP_PATROL           = 2,
        ACTOR_110600_CUE_CLIP_CHASE            = 3,
        ACTOR_110600_CUE_CLIP_ENRAGED_ATTACK   = 4,
        ACTOR_110600_CUE_CLIP_ATTACK           = 5,
        ACTOR_110600_CUE_CLIP_ALERT            = 21,
        ACTOR_110600_CHASE_PART14_CUE          = 0xC5,
        ACTOR_110600_CHASE_PART18_CUE          = 0xFD,
        ACTOR_110600_CHASE_PART18_SUPPRESS_CUE = 0xFC,
        ACTOR_110600_PATROL_CUE_LATE           = 0x33,
        ACTOR_110600_PATROL_CUE_EARLY          = 0x26,
        ACTOR_110600_ALERT_CUE                 = 4,
        ACTOR_110600_ENRAGED_ATTACK_CUE        = 9,
        ACTOR_110600_ATTACK_CUE                = 11,
        ACTOR_110600_SOUND_CHASE_PART14        = SOUND_CHARACTER(SOUND_BANK_STRANGER, 4),
        ACTOR_110600_SOUND_CHASE_PART18        = SOUND_CHARACTER(SOUND_BANK_STRANGER, 3),
        ACTOR_110600_SOUND_PATROL_LATE         = SOUND_CHARACTER(SOUND_BANK_STRANGER, 2),
        ACTOR_110600_SOUND_PATROL_EARLY        = SOUND_CHARACTER(SOUND_BANK_STRANGER, 1),
        ACTOR_110600_SOUND_ALERT               = SOUND_CHARACTER(SOUND_BANK_STRANGER, 6),
        ACTOR_110600_SOUND_ATTACK              = SOUND_CHARACTER(SOUND_BANK_STRANGER, 12),
    };

    s32 part14CueIndex;
    s32 part18CueIndex;
    s32 patrolCueIndex;
    s32 alertCueIndex;
    s32 enragedAttackCueIndex;
    s32 attackCueIndex;
    s32 previousCueIndex;
    s16 clipSelector;

    clipSelector = (u16)work->animId - ACTOR_110600_CUE_CLIP_PATROL;
    switch (clipSelector) {
        case ACTOR_110600_CUE_CLIP_CHASE - ACTOR_110600_CUE_CLIP_PATROL:
            part14CueIndex = work->rig.slots[14].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (part14CueIndex == ACTOR_110600_CHASE_PART14_CUE) {
                previousCueIndex = work->lastCueIndex;
                if (previousCueIndex != part14CueIndex) {
                    work->lastCueIndex = part14CueIndex;
                    return ACTOR_110600_SOUND_CHASE_PART14;
                }
                work->lastCueIndex = previousCueIndex;
                return SOUND_SCRIPT_REQUEST_NO_OP;
            }
            part18CueIndex = work->rig.slots[18].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            // This cue suppresses against 0xFC, while the emitted cue stores 0xFD.
            if (part18CueIndex == ACTOR_110600_CHASE_PART18_CUE) {
                if (work->lastCueIndex != ACTOR_110600_CHASE_PART18_SUPPRESS_CUE) {
                    work->lastCueIndex = part18CueIndex;
                    return ACTOR_110600_SOUND_CHASE_PART18;
                }
                work->lastCueIndex = part18CueIndex;
                break;
            }
            work->lastCueIndex = 0;
            break;
        case ACTOR_110600_CUE_CLIP_PATROL - ACTOR_110600_CUE_CLIP_PATROL:
            patrolCueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (patrolCueIndex == ACTOR_110600_PATROL_CUE_LATE) {
                if (work->lastCueIndex != patrolCueIndex) {
                    work->lastCueIndex = patrolCueIndex;
                    return ACTOR_110600_SOUND_PATROL_LATE;
                }
                work->lastCueIndex = patrolCueIndex;
            } else if (patrolCueIndex == ACTOR_110600_PATROL_CUE_EARLY) {
                previousCueIndex = work->lastCueIndex;
                if (previousCueIndex != patrolCueIndex) {
                    work->lastCueIndex = patrolCueIndex;
                    return ACTOR_110600_SOUND_PATROL_EARLY;
                }
                work->lastCueIndex = previousCueIndex;
            } else {
                work->lastCueIndex = 0;
            }
            break;
        case ACTOR_110600_CUE_CLIP_ALERT - ACTOR_110600_CUE_CLIP_PATROL:
            alertCueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (alertCueIndex == ACTOR_110600_ALERT_CUE && work->lastCueIndex != alertCueIndex) {
                work->lastCueIndex = alertCueIndex;
                return ACTOR_110600_SOUND_ALERT;
            }
            work->lastCueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            break;
        case ACTOR_110600_CUE_CLIP_ENRAGED_ATTACK - ACTOR_110600_CUE_CLIP_PATROL:
            enragedAttackCueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (enragedAttackCueIndex == ACTOR_110600_ENRAGED_ATTACK_CUE && work->lastCueIndex != enragedAttackCueIndex) {
                work->lastCueIndex = enragedAttackCueIndex;
                return ACTOR_110600_SOUND_ATTACK;
            }
            work->lastCueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            break;
        case ACTOR_110600_CUE_CLIP_ATTACK - ACTOR_110600_CUE_CLIP_PATROL:
            attackCueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (attackCueIndex == ACTOR_110600_ATTACK_CUE && work->lastCueIndex != attackCueIndex) {
                work->lastCueIndex = attackCueIndex;
                return ACTOR_110600_SOUND_ATTACK;
            }
            work->lastCueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            break;
    }
    return SOUND_SCRIPT_REQUEST_NO_OP;
}

/// Advances main slots 1..18 at the actor's current sixteenth-frame rate.
///
/// Requires live actor work, an initialized rig and loaded referenced tracks.
/// Rates use sixteenths of a frame per tick and narrow to each slot's signed
/// byte. Borrows the task's work for this call and leaves slot 0 untouched.
static __inline__ void _actor110600TickMainSlots(Task* task)
{
    _Actor110600Work* work;
    s32               slotIndex;

    work      = task->work;
    slotIndex = 1;
    do {
        work->rig.slots[slotIndex].rate = work->animRate;
        animationTickSlot(&work->rig.anim, slotIndex);
        slotIndex += 1;
    } while (slotIndex < ARRAY_SIZE(work->rig.slots));
}

/// Seeks main slots 1..18 to the requested clip using signed frame durations.
///
/// Both clip IDs must be in 0..44 and select loaded tracks and pose storage.
/// Rates narrow to signed bytes in sixteenths of a frame per tick; table
/// entries are signed whole normal-rate frames. Captures a ticked pose, seeks
/// each existing track to its start with that duration, and records the applied
/// clip. Slot 0 is untouched.
static __inline__ void _actor110600SeekRequestedAnimation(_Actor110600Work* work)
{
    _Actor110600Work* seekWork;
    s32               requestedAnimId;
    s32               seekSlotIndex;

    seekWork      = work;
    seekSlotIndex = 1;
    do {
        work->rig.slots[seekSlotIndex].rate = seekWork->animRate;
        requestedAnimId                     = seekWork->animId;
        animationSeekSlotWithBlend(&seekWork->rig.anim, seekSlotIndex, requestedAnimId, 0, D_actor_110600_80147D20[seekWork->appliedAnim][requestedAnimId]);
        seekSlotIndex += 1;
    } while (seekSlotIndex < ARRAY_SIZE(work->rig.slots));
    seekWork->appliedAnim = seekWork->animId;
}

/// Restarts main slots 1..18 on the requested loaded clip and records its ID.
///
/// Requires live work and loaded tracks for every driven slot. Reset replaces
/// each preceding rate write with normal rate; the ordinary
/// tick following this request reapplies `animRate`. Slot 0 is untouched.
static __inline__ void _actor110600ResetRequestedAnimation(_Actor110600Work* work)
{
    _Actor110600Work* resetWork;
    s32               resetSlotIndex;

    resetWork      = work;
    resetSlotIndex = 1;
    do {
        work->rig.slots[resetSlotIndex].rate = resetWork->animRate;
        animationResetSlot(&resetWork->rig.anim, resetSlotIndex, resetWork->animId);
        resetSlotIndex += 1;
    } while (resetSlotIndex < ARRAY_SIZE(work->rig.slots));
    resetWork->appliedAnim = resetWork->animId;
}

/// Restarts secondary slots 1..18 with the actor's default rotation mix.
///
/// Requires loaded `blendAnimId` tracks. The retained rate writes target the
/// primary rig, while reset gives the secondary rig normal playback rate.
/// Its next blended tick supplies three frames per tick; weight is Q12 and
/// belongs to the primary rotation. Does not enable blending by itself.
static __inline__ void _actor110600ResetBlendAnimation(Task* task)
{
    enum {
        ACTOR_110600_BLEND_RATE        = 3 * ANIMATION_RATE_ONE,
        ACTOR_110600_BLEND_MAIN_WEIGHT = 0xB78,
    };
    _Actor110600Work* blendWork;
    s32               blendSlotIndex;

    blendWork              = task->work;
    blendSlotIndex         = 1;
    blendWork->blendRate   = ACTOR_110600_BLEND_RATE;
    blendWork->blendWeight = ACTOR_110600_BLEND_MAIN_WEIGHT;
    do {
        // The rate goes to the main rig's slot, which the tick below
        // overwrites; the blend slots take theirs in the blend pass.
        blendWork->rig.slots[blendSlotIndex].rate = blendWork->blendRate;
        animationResetSlot(&blendWork->blendRig.anim, blendSlotIndex, blendWork->blendAnimId);
        blendSlotIndex += 1;
    } while (blendSlotIndex < ARRAY_SIZE(blendWork->blendRig.slots));
}

/// Restarts a requested pose and runs it for 99 hidden ticks.
///
/// `work` must be the task's live work, with initialized rigs and a loaded
/// requested clip. Resetting slots 1..18 uses normal rate; each hidden tick
/// reapplies the current rate in sixteenths of a frame per tick.
/// Slot 0 is untouched. The driver performs one ordinary tick afterwards.
static __inline__ void _actor110600SettleRequestedAnimation(Task* task, _Actor110600Work* work)
{
    enum { ACTOR_110600_SETTLE_TICK_END = 100 };
    _Actor110600Work* settleWork;
    s32               slotOrTickIndex;

    settleWork      = work;
    slotOrTickIndex = 1;
    do {
        work->rig.slots[slotOrTickIndex].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&settleWork->rig.anim, slotOrTickIndex, settleWork->animId);
        slotOrTickIndex += 1;
    } while (slotOrTickIndex < ARRAY_SIZE(work->rig.slots));
    settleWork->appliedAnim = settleWork->animId;

    // Reuse the reset cursor as the hidden-tick counter.
    slotOrTickIndex = 1;
    do {
        _actor110600TickMainSlots(task);
        slotOrTickIndex += 1;
    } while (slotOrTickIndex < ACTOR_110600_SETTLE_TICK_END);
}

/// Applies animation requests, advances the actor's poses and sounds their cues.
///
/// Requires live actor work, enemy and a nineteen-part model with both rigs
/// bound to loaded clips. Slots 1..18 are driven; slot 0 is untouched. Blend
/// requests index the signed-byte 45-by-45 duration table by previous/requested
/// clip, both in 0..44, in normal-rate frames. Reset replaces slot rates before
/// the tick reapplies `animRate`, in sixteenths of a frame. SETTLE resets the
/// requested clip and advances it 99 ticks, then this call's ordinary tick.
///
/// A secondary RESET supplies three-frame playback and a Q12 main-pose weight
/// of 0xB78. Its retained rate writes target the primary rig before resetting
/// the secondary slots, as in actor_01900 and actor_403000. Active blending ends
/// when secondary slot 1 reaches a boundary. Finally eases `lookYaw` by 0x100,
/// clamps it to +/-0x400 and applies it to part 5 and one quarter to part 3;
/// the cue sound receives this enemy's placement instance, pan and depth.
static void _actor110600TickAnimation(Task* task)
{
    enum {
        ACTOR_110600_LOOK_YAW_STEP  = 0x100,
        ACTOR_110600_LOOK_YAW_LIMIT = 0x400,
    };

    _Actor110600Work* work;
    Enemy*            enemy;
    s32               targetYaw;
    s32               currentYaw;
    s32               targetYawBits;
    s32               currentYawBits;
    s32               jointYawBits;
    s16               jointYaw;
    s32               cueSoundId;
    s32               instanceSoundId;
    s32               audioPan;
    s16               request;

    work    = task->work;
    request = work->animRequest;
    enemy   = task->spawnArg2.pointer;
    if (request == ACTOR_110600_ANIM_REQUEST_BLEND) {
        _actor110600SeekRequestedAnimation(work);
        work->animRequest  = ACTOR_110600_ANIM_REQUEST_PLAYING;
        work->animFrames   = 0;
        work->lastCueIndex = 0;
    } else if (request == ACTOR_110600_ANIM_REQUEST_RESET) {
        _actor110600ResetRequestedAnimation(work);
        work->animRequest  = ACTOR_110600_ANIM_REQUEST_PLAYING;
        work->animFrames   = 0;
        work->lastCueIndex = 0;
    } else if (request == ACTOR_110600_ANIM_REQUEST_SETTLE) {
        _actor110600SettleRequestedAnimation(task, work);
        work->animRequest  = ACTOR_110600_ANIM_REQUEST_PLAYING;
        work->animFrames   = 0;
        work->lastCueIndex = 0;
    }
    if (work->blendRequest == ACTOR_110600_ANIM_REQUEST_RESET) {
        _actor110600ResetBlendAnimation(task);
        work->blendRequest = ACTOR_110600_ANIM_REQUEST_PLAYING;
    }
    work->animFrames = (u16)(work->animFrames + 1);
    if (work->blendActive == 0) {
        _actor110600TickMainSlots(task);
    } else {
        _actor110600TickBlendedSlots(task);
        if (work->blendRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
            work->blendActive = 0;
        }
    }
    // Ease the signed yaw in 4096ths of a turn, then limit the joint turn.
    targetYaw      = work->lookYawTarget;
    currentYaw     = work->lookYaw;
    targetYawBits  = (u16)work->lookYawTarget;
    currentYawBits = (u16)work->lookYaw;
    if (currentYaw < targetYaw) {
        if ((targetYaw - currentYaw) >= ACTOR_110600_LOOK_YAW_STEP + 1) {
            work->lookYaw = (s16)(currentYawBits + ACTOR_110600_LOOK_YAW_STEP);
        } else {
            work->lookYaw = (s16)targetYawBits;
        }
    } else if ((currentYaw - targetYaw) >= ACTOR_110600_LOOK_YAW_STEP + 1) {
        work->lookYaw = (s16)(currentYawBits - ACTOR_110600_LOOK_YAW_STEP);
    } else {
        work->lookYaw = (s16)targetYawBits;
    }
    jointYaw     = work->lookYaw;
    jointYawBits = (u16)work->lookYaw;
    if (jointYaw != 0) {
        if (jointYaw >= ACTOR_110600_LOOK_YAW_LIMIT + 1) {
            jointYawBits = ACTOR_110600_LOOK_YAW_LIMIT;
        }
        if (jointYaw < -ACTOR_110600_LOOK_YAW_LIMIT) {
            jointYawBits = -ACTOR_110600_LOOK_YAW_LIMIT;
        }
        _actorRenderYawJointInWorld(&task->extra.tmd->coords[5], (s16)jointYawBits);
        _actorRenderYawJointInWorld(&task->extra.tmd->coords[3], (s16)jointYawBits >> 2);
    }
    // Attribute the cue to this placed enemy before applying positional audio.
    cueSoundId = _actor110600PollAnimationSound(work);
    if (cueSoundId != SOUND_SCRIPT_REQUEST_NO_OP) {
        instanceSoundId = cueSoundId | ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        audioPan        = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(instanceSoundId, audioPan, (s32)(s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
}

/// Initializes and links the actor's attack sphere on a model part.
///
/// The live body borrows `partCoord` and writable contacts until unlinking;
/// `bodyFlags` must select the direct-contact sphere kind. XYZ of the borrowed
/// position are copied in the part's local coordinate units, with radius 512.
/// The caller owns contact initialization and the packed attack key. This
/// actor supplies one contact and initially leaves pair testing disabled.
static __inline__ void _actor110600LinkAttackBody(WorldCollisionBody* body, GfxCoord* partCoord, WorldCollisionContact* contacts, const SVECTOR* localPosition, u16 bodyFlags)
{
    enum { ACTOR_110600_ATTACK_RADIUS = 512 };

    body->context.contacts = contacts;
    body->coord            = partCoord;
    body->pos.vx           = localPosition->vx;
    body->pos.vy           = localPosition->vy;
    body->pos.vz           = localPosition->vz;
    body->radius           = ACTOR_110600_ATTACK_RADIUS;
    body->flags            = bodyFlags;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, body);
}

/// Initializes the walker's saved rotation to its uniform Q12 scale.
///
/// Requires live walker storage and initialized scratch space for one VECTOR.
/// Clears translation and off-diagonal coefficients. Zero and `ONE` both
/// leave the identity matrix; other signed scales replace its diagonal. The
/// turning step later uses this matrix as the actor's scale baseline.
static __inline__ void _actor110600InitWalkerScale(BossStrangerWalker* walker)
{
    VECTOR* scaleVector;
    s32     uniformScale;

    scaleVector              = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    walker->scaleMtx.m[0][0] = walker->scaleMtx.m[1][1] = walker->scaleMtx.m[2][2] = ONE;
    walker->scaleMtx.m[0][1] = walker->scaleMtx.m[0][2] = walker->scaleMtx.m[1][0] = walker->scaleMtx.m[1][2] = walker->scaleMtx.m[2][0] = walker->scaleMtx.m[2][1] = 0;
    walker->scaleMtx.t[0] = walker->scaleMtx.t[1] = walker->scaleMtx.t[2] = 0;
    uniformScale                                                          = walker->scale;
    if (uniformScale != 0 && uniformScale != ONE) {
        scaleVector->vx = scaleVector->vy = scaleVector->vz = uniformScale;
        ScaleMatrix(&walker->scaleMtx, scaleVector);
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Binds the walker's two-node patrol to its actor-owned navigation storage.
///
/// Requires live work at a stable address for the walker's lifetime. Navigation
/// and route metadata point into the work block, as do their borrowed arrays.
/// Both live navigation counts are two; the layout supplies two route indices
/// and an end marker in the four-byte route buffer. Does not clear contents or
/// reset the route cursor, so spawn supplies zeroed work before layout.
/// The route's otherwise unread byte retains its spawn value of 2; its role is
/// unproven.
static __inline__ void _actor110600BindPatrolStorage(_Actor110600Work* work)
{
    work->walker.nav                   = &work->walker.navData;
    work->walker.route                 = &work->walker.routeData;
    work->walker.navData.nodeCount     = ARRAY_SIZE(work->navNodes);
    work->walker.navData.orderCount    = ARRAY_SIZE(work->navNodes);
    work->walker.routeData.field_4     = 2;
    work->walker.navData.nodes         = work->navNodes;
    work->walker.navData.nodeOrder     = work->navNodeOrder;
    work->walker.routeData.nodeIndices = work->routeNodeIndices;
}

/// Allocates the Boss Stranger work and binds its model, bodies and patrol.
///
/// Requires a live enemy/task pair, a nineteen-part model, the paired actor
/// animation overlay and initialized collision, target and scratch services.
/// Spawn-argument nibbles select movement tuning, notice ranges, patrol radius
/// and initial behavior. Mode 2 skips primitive-buffer allocation. A saved
/// spawn state selects RESTORED in modes 0/1; mode 3 retains zero-filled HIDDEN.
/// Allocation failure destroys the enemy. Success installs the exit callback,
/// links three work-backed collision bodies and advances the task state; work
/// and model storage must remain live until teardown.
static void _actor110600Spawn(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_110600_SPAWN_BEHAVIOR_SHIFT     = 16,
        ACTOR_110600_SPAWN_BEHAVIOR_MASK      = 0xF,
        ACTOR_110600_SPAWN_PATROL             = 0,
        ACTOR_110600_SPAWN_HIDDEN_OR_RESTORED = 1,
        ACTOR_110600_SPAWN_HIDDEN_NO_BUFFER   = 2,
        ACTOR_110600_SPAWN_KEEP_HIDDEN        = 3,
        ACTOR_110600_SPAWN_IDLE               = 4,
        ACTOR_110600_MOVEMENT_TUNING_MASK     = 0xF0,
        ACTOR_110600_MOVEMENT_TUNING_STANDARD = 0,
        ACTOR_110600_MOVEMENT_TUNING_LARGE    = 0x10,
        ACTOR_110600_MOVEMENT_TUNING_LARGEST  = 0x20,
        ACTOR_110600_STANDARD_ANIMATION_RATE  = 20,
        ACTOR_110600_MODEL_SCALE_LARGE        = 4500, // Q12 uniform scale
        ACTOR_110600_MODEL_SCALE_LARGEST      = 6500, // Q12 uniform scale
        ACTOR_110600_NOTICE_TUNING_MASK       = 0xF00,
        ACTOR_110600_NOTICE_TUNING_SHORT      = 0,
        ACTOR_110600_NOTICE_TUNING_MEDIUM     = 0x100,
        ACTOR_110600_NOTICE_TUNING_LONG       = 0x200,
        ACTOR_110600_PATROL_TUNING_MASK       = 0xF000,
        ACTOR_110600_PATROL_TUNING_SHORT      = 0,
        ACTOR_110600_PATROL_TUNING_MEDIUM     = 0x1000,
        ACTOR_110600_PATROL_TUNING_LONG       = 0x2000,
        ACTOR_110600_PATROL_NODE_YAW_STEP     = 0x764,
        ACTOR_110600_INITIAL_WALK_CLIP        = 2,
        ACTOR_110600_GRID_BODY_ID             = 13,
        ACTOR_110600_GRID_BODY_RADIUS         = 420,
        ACTOR_110600_GRID_BODY_HEIGHT         = 292,
        ACTOR_110600_HIT_BODY_RADIUS          = 330,
        ACTOR_110600_HIT_BODY_HEIGHT          = 700,
        ACTOR_110600_HIT_BODY_FORWARD         = 400,
        ACTOR_110600_ATTACK_PART              = 3,
        ACTOR_110600_INITIAL_TURN_LIMIT       = 0x20,
    };

    SVECTOR                attackLocalPosition;
    VECTOR                 worldPosition;
    WorldCollisionContact* walkerContacts;
    WorldCollisionBody*    hitBody;
    WorldCollisionBody*    attackBody;
    WorldCollisionContact* hitContacts;
    WorldCollisionContact* gridContacts;
    GfxCoord*              rootCoord;
    TmdObject*             model;
    s16                    sphereKind;
    u32                    placementIndex;
    TmdObject*             lightingModel;
    _Actor110600Work*      work;
    _Actor110600Work*      lightingWork;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    if (((task->spawnArg1.value >> ACTOR_110600_SPAWN_BEHAVIOR_SHIFT) & ACTOR_110600_SPAWN_BEHAVIOR_MASK) != ACTOR_110600_SPAWN_HIDDEN_NO_BUFFER) {
        model->flags = 0;
        tmdAllocPrimitiveBuffer(model);
    }
    work       = memCalloc(sizeof(_Actor110600Work), false);
    task->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    // Publish the work-backed model and enemy bindings before starting playback.
    task->exitCallback      = _actor110600Exit;
    lightingWork            = task->work;
    lightingModel           = task->extra.tmd;
    lightingModel->lightMtx = &lightingWork->lightMtx;
    lightingModel->colorMtx = &lightingWork->colorMtx;
    enemy->field_4          = &rootCoord->coord;
    enemy->field_48         = 0;
    enemy->bodyPos.vx       = 0;
    enemy->bodyPos.vy       = 0;
    enemy->bodyPos.vz       = 0;
    enemy->coord            = task->extra.tmd->coords + ACTOR_110600_ATTACK_PART;
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->reactionFlags          = 0;
    enemy->param                  = &D_actor_110600_80138F14;
    enemy->hpMax = enemy->hp = D_actor_110600_80138F14.hpMax;
    hitContacts              = work->hitContacts;
    enemy->recs              = hitContacts;
    animationInitContext(&work->rig.anim, D_actor_110600_8014850C, model, work->rig.poses, work->rig.slots);
    animationInitContext(&work->blendRig.anim, D_actor_110600_8014850C, model, work->blendRig.poses, work->blendRig.slots);
    work->animRequest   = ACTOR_110600_ANIM_REQUEST_RESET;
    work->blendActive   = 0;
    work->animId        = ACTOR_110600_INITIAL_WALK_CLIP;
    work->lookYaw       = 0;
    work->lookYawTarget = 0;
    _actor110600TickAnimation(task);
    // Link the world-space grid sphere and the model-relative hit/attack spheres.
    gridContacts                    = work->gridContacts;
    work->enrageTint                = 0;
    work->enraged                   = 0;
    work->gridBody.coord            = &gGfxViewCoord;
    walkerContacts                  = gridContacts;
    work->gridBody.context.contacts = gridContacts;
    work->gridBody.pos.vx           = (u16)task->extra.tmd->coords->coord.t[0];
    work->gridBody.pos.vy           = (s16)((u16)task->extra.tmd->coords->coord.t[1] - ACTOR_110600_GRID_BODY_HEIGHT);
    work->gridBody.pos.vz           = (u16)task->extra.tmd->coords->coord.t[2];
    work->gridBody.key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | ACTOR_110600_GRID_BODY_ID;
    work->gridBody.radius           = ACTOR_110600_GRID_BODY_RADIUS;
    work->gridBody.flags = sphereKind = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->gridBody);
    work->gridBody.flags = (u16)(work->gridBody.flags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    worldCollisionInitContacts(work->gridBody.context.contacts, ARRAY_SIZE(work->gridContacts), 0);
    hitBody                   = &work->hitBody;
    hitBody->coord            = task->extra.tmd->coords;
    hitBody->context.contacts = hitContacts;
    hitBody->pos.vx           = 0;
    hitBody->pos.vy           = 0;
    hitBody->pos.vz           = 0;
    hitBody->key              = WORLD_COLLISION_CONTACT_ENEMY_BODY;
    hitBody->radius           = ACTOR_110600_HIT_BODY_RADIUS;
    hitBody->flags            = sphereKind;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, hitBody);
    hitBody->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(hitBody->context.contacts, ARRAY_SIZE(work->hitContacts), 0);
    work->hitBody.pos.vy   = -ACTOR_110600_HIT_BODY_HEIGHT;
    attackBody             = &work->attackBody;
    work->hitBody.key      = WORLD_COLLISION_CONTACT_ENEMY_BODY;
    work->hitBody.pos.vx   = 0;
    work->hitBody.pos.vz   = ACTOR_110600_HIT_BODY_FORWARD;
    attackLocalPosition.vx = 0;
    attackLocalPosition.vy = 0;
    attackLocalPosition.vz = 0;
    _actor110600LinkAttackBody(attackBody, task->extra.tmd->coords + ACTOR_110600_ATTACK_PART, work->attackContacts, &attackLocalPosition, sphereKind);
    worldCollisionInitContacts(attackBody->context.contacts, ARRAY_SIZE(work->attackContacts), 0);
    task->msgTable          = D_actor_110600_80148624;
    work->childTask0        = 0;
    work->childTask1        = 0;
    work->field_8B0         = 0;
    work->rootDirty         = true;
    rootCoord->parent       = &gGfxViewCoord;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    worldPosition.vx = rootCoord->workm.t[0];
    worldPosition.vy = rootCoord->workm.t[1];
    worldPosition.vz = rootCoord->workm.t[2];
    worldCoordSetModelLighting(task->extra.tmd, &worldPosition, 0, 3);
    work->walker.coord      = rootCoord;
    work->walker.recs       = walkerContacts;
    work->walker.recCount   = ARRAY_SIZE(work->gridContacts);
    work->walker.avoidCount = ARRAY_SIZE(work->hitContacts);
    work->walker.state      = BOSS_STRANGER_WALKER_IDLE;
    work->walker.avoidRecs  = hitContacts;
    work->walker.scale      = 0;
    work->walker.turnLimit  = ACTOR_110600_INITIAL_TURN_LIMIT;
    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PATIO, 0, 0)) {
        work->walker.lockHeight = 0;
    } else {
        work->walker.lockHeight = true;
    }
    work->walker.skipGround = 0;
    work->walker.skipAvoid  = 1;
    work->walker.playerId   = (u8)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId;
    // Bind the two-node patrol to storage owned by this work block.
    _actor110600BindPatrolStorage(work);
    work->walker.playerId = (u8)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId;
    switch (task->spawnArg1.value & ACTOR_110600_MOVEMENT_TUNING_MASK) {
        case ACTOR_110600_MOVEMENT_TUNING_STANDARD:
            work->animRate         = ACTOR_110600_STANDARD_ANIMATION_RATE;
            work->baseRate         = ACTOR_110600_STANDARD_ANIMATION_RATE;
            work->walker.scale     = ONE;
            work->walkSpeed        = 10;
            work->flinchSpeedScale = 75;
            break;
        case ACTOR_110600_MOVEMENT_TUNING_LARGE:
            work->animRate         = ANIMATION_RATE_ONE;
            work->baseRate         = ANIMATION_RATE_ONE;
            work->walker.scale     = ACTOR_110600_MODEL_SCALE_LARGE;
            work->walkSpeed        = 8;
            work->flinchSpeedScale = 66;
            break;
        case ACTOR_110600_MOVEMENT_TUNING_LARGEST:
            work->animRate         = ANIMATION_RATE_ONE;
            work->baseRate         = ANIMATION_RATE_ONE;
            work->walker.scale     = ACTOR_110600_MODEL_SCALE_LARGEST;
            work->walkSpeed        = 14;
            work->flinchSpeedScale = 63;
            break;
        default:
            work->animRate     = ACTOR_110600_STANDARD_ANIMATION_RATE;
            work->walker.scale = ONE;
            break;
    }
    // Alternate small rate offsets between placed instances.
    placementIndex = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    if ((s16)(placementIndex & 1) == 1) {
        work->animRate += placementIndex >> 1;
    } else {
        work->animRate -= placementIndex >> 1;
    }
    switch (task->spawnArg1.value & ACTOR_110600_NOTICE_TUNING_MASK) {
        case ACTOR_110600_NOTICE_TUNING_SHORT:
            work->noticeRangeAhead  = 3000;
            work->noticeRangeAround = 500;
            break;
        case ACTOR_110600_NOTICE_TUNING_MEDIUM:
            work->noticeRangeAhead  = 4000;
            work->noticeRangeAround = 1000;
            break;
        case ACTOR_110600_NOTICE_TUNING_LONG:
            work->noticeRangeAhead  = 6000;
            work->noticeRangeAround = 2000;
            break;
        default:
            work->noticeRangeAhead  = 8000;
            work->noticeRangeAround = 2000;
            break;
    }
    switch (task->spawnArg1.value & ACTOR_110600_PATROL_TUNING_MASK) {
        case ACTOR_110600_PATROL_TUNING_SHORT:
            _actor110600LayPatrolNodes(&work->walker, 1500, ACTOR_110600_PATROL_NODE_YAW_STEP);
            break;
        case ACTOR_110600_PATROL_TUNING_MEDIUM:
            _actor110600LayPatrolNodes(&work->walker, 3000, ACTOR_110600_PATROL_NODE_YAW_STEP);
            break;
        case ACTOR_110600_PATROL_TUNING_LONG:
            _actor110600LayPatrolNodes(&work->walker, 4000, ACTOR_110600_PATROL_NODE_YAW_STEP);
            break;
        default:
            _actor110600LayPatrolNodes(&work->walker, 5000, ACTOR_110600_PATROL_NODE_YAW_STEP);
            break;
    }
    switch ((task->spawnArg1.value >> ACTOR_110600_SPAWN_BEHAVIOR_SHIFT) & ACTOR_110600_SPAWN_BEHAVIOR_MASK) {
        case ACTOR_110600_SPAWN_HIDDEN_OR_RESTORED:
            if (enemy->spawnState == 0) {
                work->state = ACTOR_110600_STATE_HIDDEN;
            } else {
                work->state = ACTOR_110600_STATE_RESTORED;
            }
            break;
        case ACTOR_110600_SPAWN_HIDDEN_NO_BUFFER:
            work->state = ACTOR_110600_STATE_HIDDEN;
            break;
        case ACTOR_110600_SPAWN_KEEP_HIDDEN:
            break;
        case ACTOR_110600_SPAWN_IDLE:
            work->state = ACTOR_110600_STATE_IDLE;
            break;
        case ACTOR_110600_SPAWN_PATROL:
        default:
            if (enemy->spawnState == 0) {
                work->state = ACTOR_110600_STATE_PATROL;
            } else {
                work->state = ACTOR_110600_STATE_RESTORED;
            }
            break;
    }
    _actor110600InitWalkerScale(&work->walker);
    work->prevState = ACTOR_110600_PREV_STATE_NONE;
    task->state    += 1;
}

/// Walks the patrol route until the player enters a notice range.
///
/// Requires live actor work/model/enemy and an initialized two-node patrol.
/// Entry resets the patrol clip and disables attacks. Player offsets narrow to
/// signed halfwords in the roots' common parent frame; yaw uses 4096 units per
/// turn. The forward cone or the all-around range selects ALERT. Movement runs
/// before the range tests, and animation advances last.
static void _actor110600PatrolState(Task* task)
{
    enum { ACTOR_110600_PATROL_TURN_LIMIT = 16,
           ACTOR_110600_NOTICE_ANGLE      = 1000 };

    _Actor110600Work* work;
    TmdObject*        model;
    Enemy*            enemy;
    GfxCoord*         rootCoord;
    SVECTOR*          offset;
    SVECTOR           playerOffset;
    s16               playerTurn;

    work  = task->work;
    model = task->extra.tmd;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model->flags                  = 0;
        work->animRate                = work->baseRate;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->animRequest             = ACTOR_110600_ANIM_REQUEST_RESET;
        work->animId                  = ACTOR_110600_ANIM_PATROL;
        work->walker.state            = BOSS_STRANGER_WALKER_PATROL;
        work->walker.turnLimit        = ACTOR_110600_PATROL_TURN_LIMIT;
    }
    work->walker.speed = work->walkSpeed;
    _bossStrangerTick(&work->walker);
    // Test both the forward notice cone and the shorter all-around range.
    rootCoord = task->extra.tmd->coords;
    offset    = &playerOffset;
    _actorPositionDeltaToPlayer(&gPlayerStatus, rootCoord, offset);
    playerTurn = _actorAngleTurnToOffset(task->extra.tmd->coords, playerOffset.vx, offset->vz);
    if (abs(playerTurn) < ACTOR_110600_NOTICE_ANGLE) {
        if (actorOutsideRadius(&playerOffset, work->noticeRangeAhead) == 0)
            work->state = ACTOR_110600_STATE_ALERT;
    }
    if (actorOutsideRadius(&playerOffset, work->noticeRangeAround) == 0)
        work->state = ACTOR_110600_STATE_ALERT;
    _actor110600TickAnimation(task);
}

/// Advances the shared five-tick footstep shake and reports its end.
///
/// The overlay-wide counter starts at zero and cycles through 1..4 then zero.
/// Alternates vertical display shake between 1 and 0, leaving it at zero on
/// completion. Returns 1 on that fifth tick, otherwise 0. All actors in this
/// overlay share the counter.
static __inline__ s32 _actor110600TickFootstepShake(void)
{
    enum { ACTOR_110600_FOOTSTEP_SHAKE_TICKS = 5 };

    D_actor_110600_8014865C++;
    if (D_actor_110600_8014865C == ACTOR_110600_FOOTSTEP_SHAKE_TICKS)
        D_actor_110600_8014865C = 0;
    if (!(D_actor_110600_8014865C & 1))
        displaySetShakeY(0);
    else
        displaySetShakeY(1);
    if (D_actor_110600_8014865C == 0) {
        displaySetShakeY(0);
        return 1;
    }
    return 0;
}

/// Chases the player and selects ATTACK when close enough.
///
/// Requires live actor work/model/enemy, player roots in a common parent frame
/// and initialized walker/animation services. Entry blends to the patrol clip;
/// movement uses the signed sixteenth-frame rate and a slower blended-hit step.
/// An aligned player at XZ distance 500 <= distance < 1000 permits attack from
/// tick 25; distance < 1000 permits it from tick 91. Look yaw follows the player,
/// and slot-1 footstep cues drive the shared five-tick screen shake.
static void _actor110600ChaseState(Task* task)
{
    enum {
        ACTOR_110600_ENRAGED_BASE_RATE    = 56,
        ACTOR_110600_ENRAGED_TURN_LIMIT   = 48,
        ACTOR_110600_CHASE_TURN_LIMIT     = 28,
        ACTOR_110600_STOP_DISTANCE        = 420,
        ACTOR_110600_BLEND_SPEED_DIVISOR  = 1520,
        ACTOR_110600_CLOSE_STOP_RADIUS    = 900,
        ACTOR_110600_ATTACK_INNER_RADIUS  = 500,
        ACTOR_110600_ATTACK_OUTER_RADIUS  = 1000,
        ACTOR_110600_ATTACK_FACING_LIMIT  = 128,
        ACTOR_110600_ALIGNED_ATTACK_TICKS = 25,
        ACTOR_110600_ATTACK_TIMEOUT_TICKS = 91,
        ACTOR_110600_FOOTSTEP_CUE_LATE    = 0x33,
        ACTOR_110600_FOOTSTEP_CUE_EARLY   = 0x26,
        ACTOR_110600_FOOTSTEP_SHAKE_LATE  = 1,
        ACTOR_110600_FOOTSTEP_SHAKE_EARLY = 2,
    };

    _Actor110600Work*   work;
    TmdObject*          model;
    Enemy*              enemy;
    BossStrangerWalker* walker;
    GfxCoord*           rootCoord;
    SVECTOR*            offset;
    SVECTOR             playerOffset;
    s16                 playerTurn;
    u16                 forwardSpeed;
    s32                 lateCueIndex;
    s32                 earlyCueIndex;

    work  = task->work;
    model = task->extra.tmd;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model->flags                  = 0;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->animRequest             = ACTOR_110600_ANIM_REQUEST_BLEND;
        work->animId                  = ACTOR_110600_ANIM_PATROL;
        work->walker.state            = BOSS_STRANGER_WALKER_CHASE;
        work->animRate                = work->baseRate;
        if (work->baseRate == ACTOR_110600_ENRAGED_BASE_RATE)
            work->walker.turnLimit = ACTOR_110600_ENRAGED_TURN_LIMIT;
        // The ordinary limit also overwrites the enraged entry limit.
        work->walker.turnLimit = ACTOR_110600_CHASE_TURN_LIMIT;
        work->stateFrame       = 0;
    }
    // Scale movement by playback rate, then stop inside the player clearance.
    if (work->blendActive == 0) {
        work->walker.speed = work->walkSpeed * work->animRate / ANIMATION_RATE_ONE;
        if ((s16)_playerDetectionOutOfReach(task->extra.tmd->coords, ACTOR_110600_STOP_DISTANCE, (s16)work->walker.speed) == 0)
            work->walker.speed = 0;
    } else {
        work->walker.speed = (u16)(work->flinchSpeedScale * work->animRate / ACTOR_110600_BLEND_SPEED_DIVISOR) / 2;
        if ((s16)_playerDetectionOutOfReach(task->extra.tmd->coords, ACTOR_110600_STOP_DISTANCE, (s16)work->walker.speed) == 0)
            work->walker.speed = 0;
    }
    rootCoord = task->extra.tmd->coords;
    offset    = &playerOffset;
    _actorPositionDeltaToPlayer(&gPlayerStatus, rootCoord, offset);
    if (actorOutsideRadius(&playerOffset, ACTOR_110600_CLOSE_STOP_RADIUS) == 0)
        work->walker.speed = 0;
    walker              = &work->walker;
    forwardSpeed        = work->walker.speed;
    walker->speedStep   = 0;
    walker->speedTarget = forwardSpeed;
    walker->speed       = forwardSpeed;
    _bossStrangerTick(walker);
    work->stateFrame++;
    playerTurn = _actorAngleTurnToOffset(task->extra.tmd->coords, playerOffset.vx, offset->vz);
    if (abs(playerTurn) < ACTOR_110600_ATTACK_FACING_LIMIT) {
        if (actorOutsideRadius(&playerOffset, ACTOR_110600_ATTACK_INNER_RADIUS) != 0) {
            if (actorOutsideRadius(&playerOffset, ACTOR_110600_ATTACK_OUTER_RADIUS) == 0 && work->stateFrame >= ACTOR_110600_ALIGNED_ATTACK_TICKS)
                work->state = ACTOR_110600_STATE_ATTACK;
        }
    }
    if (actorOutsideRadius(&playerOffset, ACTOR_110600_ATTACK_OUTER_RADIUS) == 0 && work->stateFrame >= ACTOR_110600_ATTACK_TIMEOUT_TICKS)
        work->state = ACTOR_110600_STATE_ATTACK;
    work->lookYawTarget = playerTurn;
    _actor110600TickAnimation(task);
    // Start each footstep shake once when its slot-1 cue changes.
    lateCueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    if ((lateCueIndex == ACTOR_110600_FOOTSTEP_CUE_LATE) && (work->lastFootstepCueIndex != lateCueIndex)) {
        work->footstepShake = ACTOR_110600_FOOTSTEP_SHAKE_LATE;
    }
    earlyCueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    if ((earlyCueIndex == ACTOR_110600_FOOTSTEP_CUE_EARLY) && (work->lastFootstepCueIndex != earlyCueIndex)) {
        work->footstepShake = ACTOR_110600_FOOTSTEP_SHAKE_EARLY;
    }
    work->lastFootstepCueIndex = (s32)(work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK);
    if (work->footstepShake != 0) {
        if (_actor110600TickFootstepShake() != 0)
            work->footstepShake = 0;
    }
}

/// Plays the alert clip while looking toward the player, then starts chasing.
///
/// Requires live actor work, enemy and loaded alert tracks. Entry restores the
/// normal model/grid behavior and the variant playback rate. The look target
/// is a signed yaw in 4096 units per turn, measured from halfword-truncated
/// player/root offsets in their common parent frame. Main slot 1 reaching its
/// boundary selects CHASE; this handler does not step the walker.
static void _actor110600AlertState(Task* task)
{
    _Actor110600Work* work;
    TmdObject*        model;
    Enemy*            enemy;
    GfxCoord*         rootCoord;
    SVECTOR           playerOffset;
    SVECTOR*          offset;
    s16               playerTurn;

    work  = task->work;
    model = task->extra.tmd;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model->flags                  = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->animRequest             = ACTOR_110600_ANIM_REQUEST_BLEND;
        work->animId                  = ACTOR_110600_ANIM_ALERT;
        work->animRate                = work->baseRate;
    }
    rootCoord           = task->extra.tmd->coords;
    offset              = &playerOffset;
    playerOffset.vx     = (u16)gPlayerStatus.coordMtx->t[0] - (u16)rootCoord->coord.t[0];
    offset->vy          = (u16)gPlayerStatus.coordMtx->t[1] - (u16)rootCoord->coord.t[1];
    offset->vz          = (u16)gPlayerStatus.coordMtx->t[2] - (u16)rootCoord->coord.t[2];
    playerTurn          = _actorAngleTurnToOffset(task->extra.tmd->coords, playerOffset.vx, offset->vz);
    work->lookYawTarget = playerTurn;
    _actor110600TickAnimation(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ACTOR_110600_STATE_CHASE;
    }
}

/// Reports whether the attack sphere's one contact identifies a player body.
///
/// Requires one readable contact; a zero key reports false. The category also
/// covers companion bodies. Reads no later entry and does not clear the contact.
static __inline__ s32 _actor110600HasPlayerBodyContact(const WorldCollisionContact* contacts)
{
    enum { ACTOR_110600_ATTACK_CONTACT_COUNT = 1 };
    s16 contactIndex;

    for (contactIndex = 0; contactIndex < ACTOR_110600_ATTACK_CONTACT_COUNT; contactIndex++) {
        if (contacts[contactIndex].key.value == 0) {
            break;
        }
        if ((contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
            return true;
        }
    }
    return false;
}

/// Runs an animation-timed swing and disables it after a player-body contact.
///
/// Requires live actor work, enemy, initialized attack contacts and the selected
/// attack tracks. Enraged actors use attack 0/clip 4 at 26 sixteenths per tick;
/// others use attack 1/clip 5 at normal rate. Entry ramps walker speed toward
/// zero and clears look yaw; this handler configures movement without ticking
/// the walker. Turn control stops at tick 11. Slot-1 cues delimit pair testing;
/// its boundary returns to CHASE. A retained player/companion contact sounds the
/// hit and disables pair testing without clearing that contact.
static void _actor110600AttackState(Task* task)
{
    enum {
        ACTOR_110600_ATTACK_NORMAL            = 1,
        ACTOR_110600_ATTACK_ENRAGED           = 0,
        ACTOR_110600_ATTACK_CLIP              = 5,
        ACTOR_110600_ATTACK_ENRAGED_CLIP      = 4,
        ACTOR_110600_ATTACK_ENRAGED_RATE      = 26,
        ACTOR_110600_ATTACK_BRAKE_STEP        = 8,
        ACTOR_110600_ATTACK_TURN_LIMIT        = 0x10,
        ACTOR_110600_ATTACK_TURN_STOP_TICK    = 11,
        ACTOR_110600_ATTACK_ENRAGED_START_CUE = 15,
        ACTOR_110600_ATTACK_ENRAGED_END_CUE   = 21,
        ACTOR_110600_ATTACK_START_CUE         = 16,
        ACTOR_110600_ATTACK_END_CUE           = 19,
    };

    _Actor110600Work*   work;
    _Actor110600Work*   attackWork;
    BossStrangerWalker* walker;
    Enemy*              enemy;
    TmdObject*          model;
    u16                 entrySpeed;

    work  = task->work;
    model = task->extra.tmd;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model->flags                  = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->animRequest             = ACTOR_110600_ANIM_REQUEST_BLEND;
        attackWork                    = task->work;
        if (attackWork->enraged != 0) {
            work->attackBody.key = damagePackEnemyAttackKey(enemy, ACTOR_110600_ATTACK_ENRAGED);
            work->animId         = ACTOR_110600_ATTACK_ENRAGED_CLIP;
            work->animRate       = ACTOR_110600_ATTACK_ENRAGED_RATE;
        } else {
            work->attackBody.key = damagePackEnemyAttackKey(enemy, ACTOR_110600_ATTACK_NORMAL);
            work->animId         = ACTOR_110600_ATTACK_CLIP;
            work->animRate       = ANIMATION_RATE_ONE;
        }
        work->walker.state     = BOSS_STRANGER_WALKER_CHASE;
        walker                 = &work->walker;
        entrySpeed             = work->walker.speed;
        walker->speedTarget    = 0;
        walker->speedStep      = ACTOR_110600_ATTACK_BRAKE_STEP;
        walker->speed          = entrySpeed;
        work->walker.turnLimit = ACTOR_110600_ATTACK_TURN_LIMIT;
        work->lookYaw          = 0;
        work->lookYawTarget    = 0;
        work->stateFrame       = 0;
    }
    work->stateFrame++;
    if (work->stateFrame >= ACTOR_110600_ATTACK_TURN_STOP_TICK) {
        work->walker.turnLimit = 0;
    }
    _actor110600TickAnimation(task);
    // Arm pair testing only inside the selected swing's cue window.
    if (work->animId == ACTOR_110600_ATTACK_ENRAGED_CLIP) {
        switch (work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) {
            case ACTOR_110600_ATTACK_ENRAGED_START_CUE:
                work->attackBody.flags = (u16)(work->attackBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
                break;
            case ACTOR_110600_ATTACK_ENRAGED_END_CUE:
                work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
                break;
        }
    }
    if (work->animId == ACTOR_110600_ATTACK_CLIP) {
        switch (work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) {
            case ACTOR_110600_ATTACK_START_CUE:
                work->attackBody.flags = (u16)(work->attackBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
                break;
            case ACTOR_110600_ATTACK_END_CUE:
                work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
                break;
        }
    }
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ACTOR_110600_STATE_CHASE;
    }
    if (_actor110600HasPlayerBodyContact(work->attackContacts)) {
        sndEvtRequestScriptStart(SOUND_STRANGER_ATTACK_HIT, (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords),
                                 (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
    }
}

/// Spawns the attack's hit effect at a model position selected by hit yaw.
///
/// `hitYaw` is the signed relative bearing in 4096 units per turn, normally
/// -2048..2048; `attackKey` is the packed damaging-contact key. Front/rear hits
/// use different random position groups; side hits use the yaw's sign. The
/// position record's fourth halfword selects model part 2, 7 or 9. Requires a
/// live model and loaded effect resources. Reuses the overlay's effect argument
/// record and releases its scratch SVECTOR after spawning. Placement copies
/// XYZ during the call; the effect's retained offset pointer has no later C reader.
static void _actor110600SpawnHitEffect(Task* task, s16 hitYaw, s32 attackKey)
{
    enum {
        ACTOR_110600_HIT_FRONT_YAW_LIMIT      = 0x200,
        ACTOR_110600_HIT_REAR_YAW_LIMIT       = 0x600,
        ACTOR_110600_HIT_EFFECT_ARGUMENT_LOW  = 0x100,
        ACTOR_110600_HIT_EFFECT_ARGUMENT_HIGH = 3,
    };

    SVECTOR* hitPosition;
    s32      absoluteHitYaw;

    hitPosition = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    // Use yaw sectors to choose the effect position; the rear mask keeps only bit 1.
    absoluteHitYaw = (hitYaw >= 0) ? hitYaw : -hitYaw;
    if (absoluteHitYaw < ACTOR_110600_HIT_FRONT_YAW_LIMIT) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        switch ((s32)(gRandomLcgState >> 16) & 3) {
            case 0:
                *hitPosition = D_actor_110600_801485C4[0];
                break;
            case 1:
                *hitPosition = D_actor_110600_801485C4[1];
                break;
            case 2:
                *hitPosition = D_actor_110600_801485C4[2];
                break;
            case 3:
                *hitPosition = D_actor_110600_801485C4[3];
                break;
            default:
                *hitPosition = D_actor_110600_801485C4[4];
                break;
        }
    } else if (absoluteHitYaw > ACTOR_110600_HIT_REAR_YAW_LIMIT) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        switch ((s32)(gRandomLcgState >> 16) & 2) {
            case 0:
                *hitPosition = D_actor_110600_801485C4[5];
                break;
            case 1:
                *hitPosition = D_actor_110600_801485C4[6];
                break;
            default:
                *hitPosition = D_actor_110600_801485C4[7];
                break;
        }
    } else if (hitYaw > 0) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            *hitPosition = D_actor_110600_801485C4[8];
        } else {
            *hitPosition = D_actor_110600_801485C4[9];
        }
    } else {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            *hitPosition = D_actor_110600_801485C4[10];
        } else {
            *hitPosition = D_actor_110600_801485C4[11];
        }
    }
    D_actor_110600_80148698.coord      = &task->extra.tmd->coords[1];
    D_actor_110600_80148698.spawnArgLo = ACTOR_110600_HIT_EFFECT_ARGUMENT_LOW;
    D_actor_110600_80148698.spawnArgHi = ACTOR_110600_HIT_EFFECT_ARGUMENT_HIGH;
    effectSpawnHit(damageGetPlayerAttackEffectId(attackKey), &task->extra.tmd->coords[hitPosition->pad], hitPosition, &D_actor_110600_80148698);
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

/// Returns the first attack contact key and copies its world-space XYZ.
///
/// Scans at most `contactCount` readable elements, stopping at the first zero
/// key. The nonnegative count is in contacts, not bytes; callers supply five
/// hit-body or twelve grid-body contacts. A zero result leaves `hitPoint`
/// unchanged; a hit preserves its unused fourth halfword. Contacts are borrowed
/// and never changed.
static __inline__ s32 _actor110600FindAttackContact(SVECTOR* hitPoint, const WorldCollisionContact* contacts, s16 contactCount)
{
    s16 contactIndex;
    for (contactIndex = 0; contactIndex < contactCount; contactIndex++) {
        if (!contacts[contactIndex].key.value)
            break;
        if ((contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_ATTACK) {
            hitPoint->vx = contacts[contactIndex].point.vx;
            hitPoint->vy = contacts[contactIndex].point.vy;
            hitPoint->vz = contacts[contactIndex].point.vz;
            return contacts[contactIndex].key.value;
        }
    }
    return 0;
}

/// Credits Life Drain, subtracts narrowed HP and posts the attack's damage readout.
///
/// Requires a live enemy and populated, caller-owned scratch hit key/damage.
/// HP arithmetic deliberately narrows both operands to unsigned halfwords.
static inline void _actor110600AccountAttackDamage(Enemy* enemy, const _Actor110600HitScratch* hit)
{
    damageAccumulateLifeDrainHp(enemy, hit->hitKey, hit->damage, 0);
    enemy->hp = (u16)enemy->hp - (u16)hit->damage;
    worldTargetAddReadoutAmount(&enemy->node, hit->damage, 0);
}

/// Applies player attack contacts and ongoing damage to the Stranger enemy.
///
/// Requires live actor work/model/enemy, current contact arrays, player state and
/// scratch/GTE state. Takes the first attack among five hit contacts, falling back
/// to twelve grid contacts. Updates HP, drain/readouts, reactions and hit cooldown;
/// with a dead player it only disables the attack body. HP subtraction retains
/// its unsigned-halfword narrowing. Scratch storage is released before returning.
static void _actor110600ApplyDamage(Task* task)
{
    // These attribute codes select local behavior; their shared meanings are unproven.
    enum {
        ACTOR_110600_REACTION_RESET_BLEND_4         = 4,
        ACTOR_110600_REACTION_RESET_BLEND_5         = 5,
        ACTOR_110600_REACTION_IGNORE_8              = 8,
        ACTOR_110600_REACTION_IGNORE_9              = 9,
        ACTOR_110600_REACTION_ATTRIBUTE_MASK        = 0xFFFF,
        ACTOR_110600_RETAINED_YAW_DAMAGE_MULTIPLIER = 2,
        ACTOR_110600_HIT_BODY_PART                  = 2,
        ACTOR_110600_CRITICAL_DAMAGE_MULTIPLIER     = 5,
        ACTOR_110600_RETAINED_YAW_BONUS_THRESHOLD   = 1281,
        ACTOR_110600_ANIM_HIT_REACTION              = 11,
    };

    _Actor110600Work*       work;
    Enemy*                  enemy;
    GfxCoord*               facing;
    s16                     relativeHitYaw;
    s16                     hitOffsetZ;
    s16                     priorState;
    s32                     retainedYawMagnitude;
    s32                     hitBearing;
    s32                     playerOffsetX;
    s32                     playerOffsetY;
    s32                     playerOffsetZ;
    s32                     playerDistance;
    s32                     audioPan;
    u32                     reaction;
    _Actor110600HitScratch* scratch;
    PlayerStatus*           player;

    enemy  = task->spawnArg2.pointer;
    work   = task->work;
    player = &gPlayerStatus;
    if (player->hp <= 0) {
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        return;
    }
    scratch         = SCRATCH_STACK_RESERVE_BLOCK(_Actor110600HitScratch);
    scratch->hitKey = _actor110600FindAttackContact(&scratch->hitPos, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    if (!scratch->hitKey) {
        scratch->hitKey = _actor110600FindAttackContact(&scratch->hitPos, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    }
    if (scratch->hitKey) {
        priorState = work->state;
        if ((priorState == ACTOR_110600_STATE_PATROL) || (priorState == ACTOR_110600_STATE_IDLE) || (priorState == ACTOR_110600_STATE_LURK)) {
            work->state = ACTOR_110600_STATE_ALERT;
        }
        playerOffsetX           = player->coordMtx->t[0] - task->extra.tmd->coords->coord.t[0];
        scratch->toPlayer.vx    = playerOffsetX;
        playerOffsetY           = player->coordMtx->t[1] - task->extra.tmd->coords->coord.t[1];
        scratch->toPlayer.vy    = playerOffsetY;
        playerOffsetZ           = player->coordMtx->t[2] - task->extra.tmd->coords->coord.t[2];
        scratch->toPlayer.vz    = playerOffsetZ;
        playerDistance          = SquareRoot0((playerOffsetX * playerOffsetX) + (playerOffsetY * playerOffsetY) + (playerOffsetZ * playerOffsetZ));
        scratch->playerDistance = playerDistance;
        scratch->damage         = damageComputePlayerAttack(scratch->hitKey, playerDistance, 0, 0);
        if (damageRollCriticalHit(enemy, scratch->hitKey, 0) != 0) {
            scratch->damage *= ACTOR_110600_CRITICAL_DAMAGE_MULTIPLIER;
            effectSpawn(EFFECT_CRITICAL_HIT, task->extra.tmd->coords + ACTOR_110600_HIT_BODY_PART, 0, NULL);
        }
        // Retained behavior: the bonus reads old scratch bytes before this hit
        // writes hitYaw below. Their source is unproven, not necessarily a prior hit.
        retainedYawMagnitude = scratch->hitYaw;
        if (retainedYawMagnitude < 0) {
            retainedYawMagnitude = -retainedYawMagnitude;
        }
        if (retainedYawMagnitude >= ACTOR_110600_RETAINED_YAW_BONUS_THRESHOLD) {
            scratch->damage *= ACTOR_110600_RETAINED_YAW_DAMAGE_MULTIPLIER;
        }
        if (work->enraged == 1) {
            scratch->damage >>= 1;
        }
        // Credit drain and record damage before composing the current hit bearing.
        _actor110600AccountAttackDamage(enemy, scratch);
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(task->extra.tmd->coords);
        scratch->hitOffset.vx = (s16)(scratch->hitPos.vx - (u16)task->extra.tmd->coords->workm.t[0]);
        scratch->hitOffset.vy = (s16)(scratch->hitPos.vy - (u16)task->extra.tmd->coords->workm.t[1]);
        hitOffsetZ            = scratch->hitPos.vz - (u16)task->extra.tmd->coords->workm.t[2];
        scratch->hitOffset.vz = hitOffsetZ;
        hitBearing            = ratan2(scratch->hitOffset.vx, hitOffsetZ);
        facing                = task->extra.tmd->coords;
        relativeHitYaw        = hitBearing - ratan2((s32)-facing->workm.m[2][0], (s32)facing->workm.m[2][2]);
        scratch->hitYaw       = relativeHitYaw;
        scratch->hitYaw       = _actorAngleNormalizeYaw(scratch->hitYaw);
        _actor110600SpawnHitEffect(task, scratch->hitYaw, scratch->hitKey);
        work->lookYaw       = 0;
        work->lookYawTarget = 0;
        if ((work->enraged == 0) && (enemy->hp < (s32)((u16)D_actor_110600_80138F14.hpMax >> 1))) {
            work->state   = ACTOR_110600_STATE_ENRAGE;
            work->enraged = 1;
        }
        if (enemy->hp <= 0) {
            work->hitBody.pos.vx    = 0;
            work->hitBody.pos.vy    = 0;
            work->hitBody.pos.vz    = 0;
            work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->hitBody.coord     = task->extra.tmd->coords + ACTOR_110600_HIT_BODY_PART;
            work->lookYaw           = 0;
            work->lookYawTarget     = 0;
            displaySetShakeY(0);
        } else {
            audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(SOUND_STRANGER_HURT, (s32)audioPan, (s32)(s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        }
        work->hitCooldown = damageGetPlayerAttackHitCooldown(scratch->hitKey);
        reaction          = damageGetPlayerAttackReaction(scratch->hitKey) & ACTOR_110600_REACTION_ATTRIBUTE_MASK;
        switch (reaction) {
            case DAMAGE_PLAYER_REACTION_NONE:
            case ACTOR_110600_REACTION_RESET_BLEND_4:
            case ACTOR_110600_REACTION_RESET_BLEND_5:
            case DAMAGE_PLAYER_REACTION_EXPLOSION:
            case DAMAGE_PLAYER_REACTION_INCENDIARY:
                work->blendActive  = 1;
                work->blendAnimId  = ACTOR_110600_ANIM_HIT_REACTION;
                work->blendRequest = ACTOR_110600_ANIM_REQUEST_RESET;
                break;
            case DAMAGE_PLAYER_REACTION_STAGGER:
            case ACTOR_110600_REACTION_IGNORE_8:
            case ACTOR_110600_REACTION_IGNORE_9:
                break;
            case DAMAGE_PLAYER_REACTION_BUILDUP:
                damageStartEnemyBuildup(enemy, scratch->hitKey, 0);
                work->state = ACTOR_110600_STATE_STATUS_HOLD;
                break;
            case DAMAGE_PLAYER_REACTION_POISON:
                damageTryStartEnemyDamageOverTime(enemy, scratch->hitKey, 0);
                break;
        }
    }
    // Ongoing damage is accounted separately and may request the same hit clip.
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        scratch->damage = damageTickEnemyDamageOverTime(enemy);
        if (damageIsEnemyDamageOverTimeExpired(enemy) != 0) {
            enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
        if (scratch->damage != 0) {
            enemy->hp = (u16)enemy->hp - (u16)scratch->damage;
            worldTargetAddReadoutAmount(&enemy->node, scratch->damage, 0);
            if (work->state != ACTOR_110600_STATE_STATUS_HOLD) {
                work->blendActive  = 1;
                work->blendAnimId  = ACTOR_110600_ANIM_HIT_REACTION;
                work->blendRequest = ACTOR_110600_ANIM_REQUEST_RESET;
            } else {
                work->prevState = -1;
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor110600HitScratch);
}

/// Stops the walker and alternates the idle clip with an occasional variation.
///
/// Requires live actor work/model/enemy and loaded clips 24 and 14. Entry
/// disables attacks, clears look/turning and ramps speed toward zero by eight
/// units per tick. At an idle control jump, one in eight random draws requests
/// the variation; its slot-1 boundary blends back to idle. Both the walker and
/// animation advance each tick.
static void _actor110600IdleState(Task* task)
{
    enum { ACTOR_110600_IDLE_DECELERATION = 8 };

    _Actor110600Work*   work;
    BossStrangerWalker* walker;
    Enemy*              enemy;
    TmdObject*          model;
    u32                 randomValue;
    u16                 entrySpeed;

    work  = task->work;
    model = task->extra.tmd;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model->flags                  = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->animRequest             = ACTOR_110600_ANIM_REQUEST_BLEND;
        work->animId                  = ACTOR_110600_ANIM_IDLE;
        work->animRate                = work->baseRate;
        ACTOR_110600_ENTER_IDLE_WALKER(work, walker, entrySpeed, 0, ACTOR_110600_IDLE_DECELERATION);
    }
    _bossStrangerTick(&work->walker);
    _actor110600TickAnimation(task);
    if (work->animId == ACTOR_110600_ANIM_IDLE) {
        if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP) {
            randomValue     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = randomValue;
            if (!((randomValue >> 16) & 7)) {
                work->animId      = ACTOR_110600_ANIM_IDLE_VARIATION;
                work->animRequest = ACTOR_110600_ANIM_REQUEST_BLEND;
                _actor110600TickAnimation(task);
            }
        }
    }
    if ((work->animId == ACTOR_110600_ANIM_IDLE_VARIATION) && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        work->animId      = ACTOR_110600_ANIM_IDLE;
        work->animRequest = ACTOR_110600_ANIM_REQUEST_BLEND;
        _actor110600TickAnimation(task);
    }
}

/// Plays the backward fall and landing clips, then selects DOWN or DEATH_BURN.
///
/// Requires live actor work/model/enemy and loaded clips 29 and 30. Entry
/// disables attacks and turning, and ramps the unsigned walker speed toward
/// 0xFFFE, applied as a signed step of -2. The first slot-1 boundary restarts
/// clip 30 and ramps toward zero; its boundary chooses DOWN while health is
/// positive, otherwise DEATH_BURN. No recovered state writer selects FALL_BACK.
static void _actor110600FallBackState(Task* task)
{
    enum { ACTOR_110600_FALL_BACK_SPEED      = 0xFFFE,
           ACTOR_110600_FALL_BACK_SPEED_STEP = 2 };

    _Actor110600Work*   work;
    BossStrangerWalker* walker;
    BossStrangerWalker* landingWalker;
    Enemy*              enemy;
    u16                 entrySpeed;
    u16                 landingSpeed;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        task->extra.tmd->flags        = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->animRequest             = ACTOR_110600_ANIM_REQUEST_RESET;
        work->animId                  = ACTOR_110600_ANIM_FALL_BACK;
        work->animRate                = ANIMATION_RATE_ONE;
        ACTOR_110600_ENTER_IDLE_WALKER(work, walker, entrySpeed, ACTOR_110600_FALL_BACK_SPEED, ACTOR_110600_FALL_BACK_SPEED_STEP);
    }
    landingWalker = &work->walker;
    _bossStrangerTick(landingWalker);
    _actor110600TickAnimation(task);
    if (work->animId == ACTOR_110600_ANIM_FALL_BACK) {
        if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
            landingSpeed               = work->walker.speed;
            work->animId               = ACTOR_110600_ANIM_FALL_BACK_END;
            work->animRequest          = ACTOR_110600_ANIM_REQUEST_RESET;
            landingWalker->speedTarget = 0;
            landingWalker->speedStep   = ACTOR_110600_FALL_BACK_SPEED_STEP;
            landingWalker->speed       = landingSpeed;
            return;
        }
    }
    if ((work->animId == ACTOR_110600_ANIM_FALL_BACK_END) && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        if (enemy->hp > 0) {
            work->state = ACTOR_110600_STATE_DOWN;
        } else {
            work->state = ACTOR_110600_STATE_DEATH_BURN;
        }
    }
}

/// Flattens the burning corpse around its current yaw at the current burn tick.
///
/// Requires live model/work and 0x58 free aligned scratch bytes for the yaw
/// rebuild. Work is borrowed read-only; scratch is released before return.
/// X/Z use the walker's signed Q12 scale; Y uses the supplied signed Q12 height
/// minus two units per tick from tick 300, narrowed to a halfword. Earlier ticks
/// therefore increase Y relative to the supplied height. Pitch/roll and prior
/// scale are discarded, translation stays intact and composition becomes dirty.
static __inline__ void _actor110600ShrinkBurnRootYaw(Task* task, const _Actor110600Work* work, s16 heightScale)
{
    enum { ACTOR_110600_BURN_SHRINK_ORIGIN_TICK = 300 };

    TmdObject*            model;
    GfxCoord*             rootCoord;
    s16                   horizontalScale;
    ActorScaleRotScratch* yawScratch;
    s16                   yaw;

    yawScratch      = SCRATCH_STACK_RESERVE_BLOCK(ActorScaleRotScratch);
    model           = task->extra.tmd;
    rootCoord       = model->coords;
    horizontalScale = work->walker.scale;
    heightScale    -= (work->stateFrame - ACTOR_110600_BURN_SHRINK_ORIGIN_TICK) * 2;
    yaw             = ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]);
    yawScratch->yaw = yaw;
    gfxRotMatrixY(&yawScratch->rotation, yaw, GRAPHICS_ROTATION_REPLACE);
    yawScratch->scale.vx = horizontalScale;
    yawScratch->scale.vy = heightScale;
    yawScratch->scale.vz = horizontalScale;
    ScaleMatrix(&yawScratch->rotation, &yawScratch->scale);
    _actorRenderCopyRotation(rootCoord, yawScratch->rotation.m);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleRotScratch);
}

/// Scales the saved corpse color matrix and its RGB translation for the burn fade.
///
/// colorScale supplies signed Q12 XYZ axis factors; Z also scales all three
/// translation components through the GTE. The caller supplies a uniform scale.
/// Starts from burnColorMtx on every call, so the fade does not compound. Work
/// and the full VECTOR stay live through the transfer; negative factors are kept.
static __inline__ void _actor110600FadeBurnColor(_Actor110600Work* work, VECTOR* colorScale)
{
    work->colorMtx = work->burnColorMtx;
    ScaleMatrix(&work->colorMtx, colorScale);
    gte_lddp(colorScale->vz);
    gte_ldlvl(&work->colorMtx.t[0]);
    gte_gpf12();
    gte_stlvl(&work->colorMtx.t[0]);
}

/// Burns, flattens and fades the dead Stranger while its task remains alive.
///
/// Entry disables attack pairs, grid tests and lock-on and saves the walker Q12
/// height. Burn effects use chest-local offsets at frames 200 and 400; frame 230
/// captures color and starts the signed Q12 fade 3000-4*frame. The root flattens
/// from frame 201 while saved height exceeds 2048; the yaw helper also applies its
/// tick-dependent Y correction. Drawing stops at frame 600, while the counter
/// continues to its cap of 1000. Requires live model/work and 0x58 free aligned
/// scratch bytes for the root rebuild. Work is retained for the task's teardown.
static void _actor110600DeathBurnState(Task* task)
{
    enum {
        ACTOR_110600_BURN_FRAME_LIMIT                = 1000,
        ACTOR_110600_BURN_EFFECT_FRAME_1             = 200,
        ACTOR_110600_BURN_EFFECT_FRAME_2             = 400,
        ACTOR_110600_BURN_FADE_START_FRAME           = 230,
        ACTOR_110600_BURN_UNUSED_OFFSET_FRAME_1      = 250,
        ACTOR_110600_BURN_UNUSED_OFFSET_FRAME_2      = 420,
        ACTOR_110600_BURN_REASSERT_TRANSLUCENT_FRAME = 253,
        ACTOR_110600_BURN_HIDE_FRAME                 = 600,
        ACTOR_110600_BURN_FLATTEN_START_FRAME        = 201,
        ACTOR_110600_BURN_CHEST_PART                 = 3,
        ACTOR_110600_BURN_EFFECT_ARGUMENT            = 3,
        ACTOR_110600_BURN_EFFECT_OFFSET              = 300,
        ACTOR_110600_BURN_UNUSED_OFFSET              = 400,
        ACTOR_110600_BURN_FADE_ORIGIN_Q12            = 3000,
        ACTOR_110600_BURN_FADE_STEP_Q12              = 4,
        ACTOR_110600_BURN_HEIGHT_FLOOR_Q12           = 2048,
        ACTOR_110600_BURN_HEIGHT_STEP_Q12            = 8
    };

    _Actor110600Work* work;
    Enemy*            enemy;
    SVECTOR           effectOffset;
    VECTOR            colorScale;
    s16               heightScale;

    work = task->work;
    if (work->stateEntered != 0) {
        enemy                         = task->spawnArg2.pointer;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        work->stateFrame              = 0;
        work->burnHeightScale         = work->walker.scale;
    }
    if (work->stateFrame < ACTOR_110600_BURN_FRAME_LIMIT) {
        work->stateFrame = (u16)work->stateFrame + 1;
    }
    switch (work->stateFrame) {
        case ACTOR_110600_BURN_FADE_START_FRAME:
            task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            work->burnColorMtx     = work->colorMtx;
            break;
        case ACTOR_110600_BURN_EFFECT_FRAME_1:
        case ACTOR_110600_BURN_EFFECT_FRAME_2:
            effectOffset.vx = ACTOR_110600_BURN_EFFECT_OFFSET;
            effectOffset.vy = 0;
            effectOffset.vz = 0;
            effectSpawn(EFFECT_CORPSE_BURN, &task->extra.tmd->coords[ACTOR_110600_BURN_CHEST_PART], ACTOR_110600_BURN_EFFECT_ARGUMENT, &effectOffset);
            break;
        // These retail cue frames prepare an offset without spawning an effect.
        case ACTOR_110600_BURN_UNUSED_OFFSET_FRAME_1:
        case ACTOR_110600_BURN_UNUSED_OFFSET_FRAME_2:
            effectOffset.vx = ACTOR_110600_BURN_UNUSED_OFFSET;
            effectOffset.vy = 0;
            effectOffset.vz = 0;
            break;
        case ACTOR_110600_BURN_HIDE_FRAME:
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
    // Reapply the fade to the saved matrix, rather than compounding it each tick.
    if (work->stateFrame >= ACTOR_110600_BURN_FADE_START_FRAME) {
        colorScale.vx = colorScale.vy = colorScale.vz = ACTOR_110600_BURN_FADE_ORIGIN_Q12 + work->stateFrame * -ACTOR_110600_BURN_FADE_STEP_Q12;
        _actor110600FadeBurnColor(work, &colorScale);
    }
    if (work->stateFrame == ACTOR_110600_BURN_REASSERT_TRANSLUCENT_FRAME) {
        task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
    }
    if (work->stateFrame >= ACTOR_110600_BURN_FLATTEN_START_FRAME) {
        heightScale = work->burnHeightScale;
        if (work->burnHeightScale >= ACTOR_110600_BURN_HEIGHT_FLOOR_Q12 + 1) {
            heightScale          -= ACTOR_110600_BURN_HEIGHT_STEP_Q12;
            work->burnHeightScale = heightScale;
            _actor110600ShrinkBurnRootYaw(task, work, heightScale);
        }
    }
}

/// Re-dresses a live actor: take the model object out of draw, drop bit 0x8000
/// of `attackBody.flags` and bit 0x4000 of `gridBody.flags`, tag the enemy's
/// link node, clear the `walker.turnLimit` / `lookYaw` / `lookYawTarget` timers and hand
/// the model the 0x80 texture page, then spawn five effects off its part
/// coordinates 6, 8, 10, 11 and 15 (`effectSpawn` bank 0xA0005, buffer sizes
/// 0x200 / 0x200 / 0x200 / 0x300 / 0x300). Each spawned model object takes its
/// texture page and CLUT from the nested area record the actor's own area key
/// resolves to, and is streamed twice once its aux buffer exists.
static void func_actor_110600_80136ECC(Task* arg0)
{
    _Actor110600Work* work;
    Enemy*            enemy;
    TmdObject*        obj;
    GameLocationKey   key;
    u8                areaByte0;
    u32               raw1, index1;
    EffectWork*       effect1;
    TmdObject*        model1;
    AreaPlacement*    entry1;
    GameLocationKey*  sessionKey1;
    u32               raw2, index2;
    EffectWork*       effect2;
    TmdObject*        model2;
    AreaPlacement*    entry2;
    GameLocationKey*  sessionKey2;
    u32               raw3, index3;
    EffectWork*       effect3;
    TmdObject*        model3;
    AreaPlacement*    entry3;
    GameLocationKey*  sessionKey3;
    u32               raw4, index4;
    EffectWork*       effect4;
    TmdObject*        model4;
    AreaPlacement*    entry4;
    GameLocationKey*  sessionKey4;
    u32               raw5, index5;
    EffectWork*       effect5;
    TmdObject*        model5;
    AreaPlacement*    entry5;
    GameLocationKey*  sessionKey5;

    work = arg0->work;
    if (work->stateEntered != 0) {
        enemy                         = arg0->spawnArg2.pointer;
        obj                           = arg0->extra.tmd;
        obj->flags                    = 0;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->walker.turnLimit        = 0;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        obj->flags                    = TMD_OBJECT_SKIP_ACTIVE_DRAW;

        effect1 = effectSpawn(EFFECT_BURST_BODY_PART_BANK10, &arg0->extra.tmd->coords[6], 0x200, NULL);
        if (effect1 != NULL) {
            sessionKey1 = &gGameSession->location.loc;
            raw1        = enemy->placeKey;
            model1      = effect1->task->extra.tmd;
            key.stage   = sessionKey1->stage;
            key.area    = sessionKey1->area;
            key.room    = sessionKey1->room;
            areaByte0   = gGameSession->location.loc.view;
            index1      = raw1 >> 12;
            key.view    = areaByte0;
            areaSyncLocationVariant(&key);
            entry1                    = gpAreaPlaceAt(areaGetVariant(&key)->placements, index1);
            model1->texturePageOffset = entry1->texturePageOffset;
            model1->clutRowOffset     = entry1->clutRowOffset;
            if (model1->buffer != NULL) {
                tmdBuildBufferHalf(model1);
                tmdBuildBufferHalf(model1);
            }
        }

        effect2 = effectSpawn(EFFECT_BURST_BODY_PART_BANK10, &arg0->extra.tmd->coords[8], 0x200, NULL);
        if (effect2 != NULL) {
            sessionKey2 = &gGameSession->location.loc;
            raw2        = enemy->placeKey;
            model2      = effect2->task->extra.tmd;
            key.stage   = sessionKey2->stage;
            key.area    = sessionKey2->area;
            key.room    = sessionKey2->room;
            areaByte0   = gGameSession->location.loc.view;
            index2      = raw2 >> 12;
            key.view    = areaByte0;
            areaSyncLocationVariant(&key);
            entry2                    = gpAreaPlaceAt(areaGetVariant(&key)->placements, index2);
            model2->texturePageOffset = entry2->texturePageOffset;
            model2->clutRowOffset     = entry2->clutRowOffset;
            if (model2->buffer != NULL) {
                tmdBuildBufferHalf(model2);
                tmdBuildBufferHalf(model2);
            }
        }

        effect3 = effectSpawn(EFFECT_BURST_BODY_PART_BANK10, &arg0->extra.tmd->coords[10], 0x200, NULL);
        if (effect3 != NULL) {
            sessionKey3 = &gGameSession->location.loc;
            raw3        = enemy->placeKey;
            model3      = effect3->task->extra.tmd;
            key.stage   = sessionKey3->stage;
            key.area    = sessionKey3->area;
            key.room    = sessionKey3->room;
            areaByte0   = gGameSession->location.loc.view;
            index3      = raw3 >> 12;
            key.view    = areaByte0;
            areaSyncLocationVariant(&key);
            entry3                    = gpAreaPlaceAt(areaGetVariant(&key)->placements, index3);
            model3->texturePageOffset = entry3->texturePageOffset;
            model3->clutRowOffset     = entry3->clutRowOffset;
            if (model3->buffer != NULL) {
                tmdBuildBufferHalf(model3);
                tmdBuildBufferHalf(model3);
            }
        }

        effect4 = effectSpawn(EFFECT_BURST_BODY_PART_BANK10, &arg0->extra.tmd->coords[11], 0x300, NULL);
        if (effect4 != NULL) {
            sessionKey4 = &gGameSession->location.loc;
            raw4        = enemy->placeKey;
            model4      = effect4->task->extra.tmd;
            key.stage   = sessionKey4->stage;
            key.area    = sessionKey4->area;
            key.room    = sessionKey4->room;
            areaByte0   = gGameSession->location.loc.view;
            index4      = raw4 >> 12;
            key.view    = areaByte0;
            areaSyncLocationVariant(&key);
            entry4                    = gpAreaPlaceAt(areaGetVariant(&key)->placements, index4);
            model4->texturePageOffset = entry4->texturePageOffset;
            model4->clutRowOffset     = entry4->clutRowOffset;
            if (model4->buffer != NULL) {
                tmdBuildBufferHalf(model4);
                tmdBuildBufferHalf(model4);
            }
        }

        effect5 = effectSpawn(EFFECT_BURST_BODY_PART_BANK10, &arg0->extra.tmd->coords[15], 0x300, NULL);
        if (effect5 != NULL) {
            sessionKey5 = &gGameSession->location.loc;
            raw5        = enemy->placeKey;
            model5      = effect5->task->extra.tmd;
            key.stage   = sessionKey5->stage;
            key.area    = sessionKey5->area;
            key.room    = sessionKey5->room;
            areaByte0   = gGameSession->location.loc.view;
            index5      = raw5 >> 12;
            key.view    = areaByte0;
            areaSyncLocationVariant(&key);
            entry5                    = gpAreaPlaceAt(areaGetVariant(&key)->placements, index5);
            model5->texturePageOffset = entry5->texturePageOffset;
            model5->clutRowOffset     = entry5->clutRowOffset;
            if (model5->buffer != NULL) {
                tmdBuildBufferHalf(model5);
                tmdBuildBufferHalf(model5);
            }
        }
    }
}

/// Offset, 100 units along Z, that `func_actor_110600_801372CC` hands
/// `effectSpawnHit` with the model's seventh coordinate when it spawns its
/// three effects.
static const SVECTOR D_actor_110600_80131F1C = { 0, 0, 100, 0 };

static void func_actor_110600_801372CC(Task* arg0)
{
    _Actor110600Work* work;
    Enemy*            enemy;
    SVECTOR           vec;
    EffectSpawnArg*   d;
    EffectSpawnArg*   tailEffect;
    GfxCoord*         effectCoord;
    GfxCoord*         effectCoord2;
    GfxCoord*         effectCoord3;
    u32               rng;

    work = arg0->work;
    if (work->stateEntered != 0) {
        enemy                         = arg0->spawnArg2.pointer;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->animRequest             = ACTOR_110600_ANIM_REQUEST_RESET;
        work->animRate                = 0x10;
        work->lookYawTarget           = 0;
        work->lookYaw                 = 0;
        if ((work->lastCommand.stage == GAME_STAGE_ACROPOLIS) && (work->lastCommand.area == GAME_AREA_ACROPOLIS_CAFETERIA) && (work->lastCommand.command == 6)) {
            rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = rng;
            if ((rng >> 16) & 1) {
                vec.vx                             = -0xE;
                vec.vy                             = 0;
                vec.vz                             = 0;
                effectCoord                        = arg0->extra.tmd->coords;
                D_actor_110600_80148698.spawnArgLo = 0x100;
                D_actor_110600_80148698.spawnArgHi = 3;
                D_actor_110600_80148698.coord      = effectCoord;
                effectSpawnHit(damageGetPlayerAttackEffectId(0x1001), &arg0->extra.tmd->coords[9], &vec, &D_actor_110600_80148698);
            } else {
                d             = &D_actor_110600_80148698;
                vec.vx        = -0x19;
                vec.vy        = 0;
                vec.vz        = 0;
                effectCoord2  = arg0->extra.tmd->coords;
                d->spawnArgLo = 0x100;
                d->spawnArgHi = 3;
                d->coord      = effectCoord2;
                effectSpawnHit(damageGetPlayerAttackEffectId(0x1001), &arg0->extra.tmd->coords[2], &vec, &D_actor_110600_80148698);
            }
        }
    }
    work->blendActive = 0;
    _actor110600TickAnimation(arg0);

    _actorRenderRescaleYaw(arg0->extra.tmd->coords, work->walker.scale);

    if ((work->lastCommand.stage == GAME_STAGE_ACROPOLIS) && (work->lastCommand.area == GAME_AREA_ACROPOLIS_CAFETERIA) && (work->lastCommand.command == 3) && (work->animId != 0x1E)) {
        if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0xB) {
            D_actor_110600_80147D20[0x24][0x1E] = 6;
            work->animId                        = 0x1E;
            work->animRequest                   = ACTOR_110600_ANIM_REQUEST_BLEND;
        }
        if (work->animId != 0x1E) {
            if (((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 4) && (work->lastCueIndex != (work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK))) {
                vec                    = D_actor_110600_80131F1C;
                tailEffect             = &D_actor_110600_80148698;
                effectCoord3           = arg0->extra.tmd->coords;
                tailEffect->spawnArgLo = 0x100;
                tailEffect->spawnArgHi = 3;
                tailEffect->coord      = effectCoord3;
                effectSpawnHit(damageGetPlayerAttackEffectId(0x1001), &arg0->extra.tmd->coords[6], &vec, &D_actor_110600_80148698);
                effectSpawnHit(damageGetPlayerAttackEffectId(0x1001), &arg0->extra.tmd->coords[6], &vec, &D_actor_110600_80148698);
                effectSpawnHit(damageGetPlayerAttackEffectId(0x1001), &arg0->extra.tmd->coords[6], &vec, &D_actor_110600_80148698);
            }
        }
    }
}

/// Thrashes through two clips before handing the corpse to DEATH_BURN.
///
/// Entry disables attacks and grid correction and makes the enemy un-lockable.
/// Requires live actor work/model/enemy and clips 22 and 33. Clip 22 plays at
/// two frames per tick while random signed yaw jerks mark the root dirty. At
/// tick 31 it requests clip 33 at normal rate; that clip's slot-1 boundary
/// selects DEATH_BURN. No local code selects this state.
static void _actor110600DeathThrashState(Task* task)
{
    enum {
        ACTOR_110600_DEATH_THRASH_CLIP            = 22,
        ACTOR_110600_DEATH_TRANSITION_CLIP        = 33,
        ACTOR_110600_DEATH_THRASH_TICKS           = 31,
        ACTOR_110600_DEATH_THRASH_RATE            = 2 * ANIMATION_RATE_ONE,
        ACTOR_110600_DEATH_THRASH_FORWARD_YAW     = 50,
        ACTOR_110600_DEATH_THRASH_REVERSE_YAW     = -120,
        ACTOR_110600_DEATH_THRASH_RANDOM_MASK     = 15,
        ACTOR_110600_DEATH_THRASH_FORWARD_CHOICES = 10,
    };

    _Actor110600Work* work;
    Enemy*            enemy;
    u32               randomValue;

    work = task->work;
    if (work->stateEntered != 0) {
        enemy                         = task->spawnArg2.pointer;
        task->extra.tmd->flags        = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->animRate                = ACTOR_110600_DEATH_THRASH_RATE;
        work->animRequest             = ACTOR_110600_ANIM_REQUEST_BLEND;
        work->walker.turnLimit        = 0;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        work->animId                  = ACTOR_110600_DEATH_THRASH_CLIP;
    }
    _actor110600TickAnimation(task);
    // Bias the root jerk forward, then finish on the transition clip.
    if (work->animId == ACTOR_110600_DEATH_THRASH_CLIP) {
        randomValue     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = randomValue;
        if (((randomValue >> 16) & ACTOR_110600_DEATH_THRASH_RANDOM_MASK) < ACTOR_110600_DEATH_THRASH_FORWARD_CHOICES) {
            gfxRotMatrixY(&task->extra.tmd->coords->coord, ACTOR_110600_DEATH_THRASH_FORWARD_YAW, 0);
        } else {
            gfxRotMatrixY(&task->extra.tmd->coords->coord, ACTOR_110600_DEATH_THRASH_REVERSE_YAW, 0);
        }
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        if ((work->animId == ACTOR_110600_DEATH_THRASH_CLIP) && (work->animFrames >= ACTOR_110600_DEATH_THRASH_TICKS)) {
            work->animRate    = ANIMATION_RATE_ONE;
            work->animRequest = ACTOR_110600_ANIM_REQUEST_BLEND;
            work->animId      = ACTOR_110600_DEATH_TRANSITION_CLIP;
        }
    }
    if ((work->animId == ACTOR_110600_DEATH_TRANSITION_CLIP) && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        work->state = ACTOR_110600_STATE_DEATH_BURN;
    }
}

/// Holds the lurk pose until the player is within 3000 horizontal units.
///
/// Requires live actor work/model/enemy and the command-bound clip 34. Entry
/// restores model/grid behavior, clears turning and starts normal playback.
/// The range uses signed-halfword offsets in the roots' common parent frame;
/// equality remains in LURK. Animation is ticked before range selection.
/// Nearer players select LURK_ALERT; the damage handler can instead select ALERT.
static void _actor110600LurkState(Task* task)
{
    enum {
        ACTOR_110600_LURK_NOTICE_RADIUS = 3000,
    };

    _Actor110600Work* work;
    TmdObject*        model;
    Enemy*            enemy;
    GfxCoord*         rootCoord;
    SVECTOR           playerOffset;
    SVECTOR*          offset;

    work = task->work;
    if (work->stateEntered != 0) {
        enemy                         = task->spawnArg2.pointer;
        model                         = task->extra.tmd;
        work->animId                  = ACTOR_110600_ANIM_SCRIPTED_SLOT0;
        work->animRequest             = ACTOR_110600_ANIM_REQUEST_RESET;
        model->flags                  = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->walker.turnLimit        = 0;
        work->lookYaw                 = 0;
        work->animRate                = ANIMATION_RATE_ONE;
        work->lookYawTarget           = 0;
    }
    _actor110600TickAnimation(task);
    work->lastCueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    rootCoord          = task->extra.tmd->coords;
    offset             = &playerOffset;
    playerOffset.vx    = (u16)gPlayerStatus.coordMtx->t[0] - (u16)rootCoord->coord.t[0];
    offset->vy         = (u16)gPlayerStatus.coordMtx->t[1] - (u16)rootCoord->coord.t[1];
    offset->vz         = (u16)gPlayerStatus.coordMtx->t[2] - (u16)rootCoord->coord.t[2];
    if (!actorOutsideRadius(offset, ACTOR_110600_LURK_NOTICE_RADIUS)) {
        work->state = ACTOR_110600_STATE_LURK_ALERT;
    }
}

/// Plays the lurk wake-up alert while un-lockable, then starts chasing.
///
/// Requires live actor work/model/enemy and loaded alert tracks. Entry clears
/// walker turning and look yaw and plays at normal rate. The player look target
/// uses signed yaw in 4096 units per turn from halfword-truncated offsets in
/// the common parent frame. Main slot 1 reaching its boundary selects CHASE;
/// this handler does not step the walker.
static void _actor110600LurkAlertState(Task* task)
{
    _Actor110600Work* work;
    TmdObject*        model;
    Enemy*            enemy;
    GfxCoord*         rootCoord;
    SVECTOR           playerOffset;
    SVECTOR*          offset;
    s16               playerTurn;

    work = task->work;
    if (work->stateEntered != 0) {
        enemy                         = task->spawnArg2.pointer;
        model                         = task->extra.tmd;
        work->animId                  = ACTOR_110600_ANIM_ALERT;
        work->animRequest             = ACTOR_110600_ANIM_REQUEST_BLEND;
        model->flags                  = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->walker.turnLimit        = 0;
        work->lookYaw                 = 0;
        work->animRate                = ANIMATION_RATE_ONE;
    }
    rootCoord           = task->extra.tmd->coords;
    offset              = &playerOffset;
    playerOffset.vx     = (u16)gPlayerStatus.coordMtx->t[0] - (u16)rootCoord->coord.t[0];
    offset->vy          = (u16)gPlayerStatus.coordMtx->t[1] - (u16)rootCoord->coord.t[1];
    offset->vz          = (u16)gPlayerStatus.coordMtx->t[2] - (u16)rootCoord->coord.t[2];
    playerTurn          = _actorAngleTurnToOffset(task->extra.tmd->coords, playerOffset.vx, offset->vz);
    work->lookYawTarget = playerTurn;
    _actor110600TickAnimation(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ACTOR_110600_STATE_CHASE;
    }
}

/// Jolts the root along its local X axis for five ticks, then starts chasing.
///
/// Requires live actor work/model/enemy and a nonzero root X axis suitable for
/// normalization. Whole-unit displacements are 800, -1000, 400, -400 and 200;
/// only parent-frame X/Z translation is applied. Clobbers GTE state and marks
/// the root dirty after each push. The sixth call selects CHASE without a push.
/// The step counter is shared by the overlay's actors; no local code selects
/// this state.
static void _actor110600ShudderState(Task* task)
{
    enum {
        ACTOR_110600_SHUDDER_PUSH0      = 800,
        ACTOR_110600_SHUDDER_PUSH1      = -1000,
        ACTOR_110600_SHUDDER_PUSH2      = 400,
        ACTOR_110600_SHUDDER_PUSH3      = -400,
        ACTOR_110600_SHUDDER_PUSH4      = 200,
        ACTOR_110600_SHUDDER_PUSH_COUNT = 5,
    };

    _Actor110600Work* work;
    TmdObject*        model;
    Enemy*            enemy;
    SVECTOR           sideStep;
    GfxCoord*         rootCoord;

    work  = task->work;
    model = task->extra.tmd;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model->flags                  = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        D_actor_110600_80148688.step  = 0;
    }
    // Normalize the root's side axis so displacement is in parent-coordinate units.
    gfxReadMatrixXAxis(&task->extra.tmd->coords->coord, &sideStep);
    VectorNormalSS(&sideStep, &sideStep);
/// Scales a writable Q12 direction by signed parent-coordinate distance.
///
/// Standalone statement sequence for this handler; arguments must have no side
/// effects. `direction` is evaluated twice and `distance` once. Narrows XYZ to
/// signed halfwords, clobbers the GTE and changes no coordinate by itself.
#define ACTOR_110600_SCALE_SHUDDER_STEP(direction, distance) \
    gte_lddp(distance);                                      \
    gte_ldsv(direction);                                     \
    gte_gpf12();                                             \
    gte_stsv(direction)
    switch (D_actor_110600_80148688.step) {
        case 0:
            ACTOR_110600_SCALE_SHUDDER_STEP(&sideStep, ACTOR_110600_SHUDDER_PUSH0);
            break;
        case 1:
            ACTOR_110600_SCALE_SHUDDER_STEP(&sideStep, ACTOR_110600_SHUDDER_PUSH1);
            break;
        case 2:
            ACTOR_110600_SCALE_SHUDDER_STEP(&sideStep, ACTOR_110600_SHUDDER_PUSH2);
            break;
        case 3:
            ACTOR_110600_SCALE_SHUDDER_STEP(&sideStep, ACTOR_110600_SHUDDER_PUSH3);
            break;
        case 4:
            ACTOR_110600_SCALE_SHUDDER_STEP(&sideStep, ACTOR_110600_SHUDDER_PUSH4);
            break;
        case ACTOR_110600_SHUDDER_PUSH_COUNT:
            work->state = ACTOR_110600_STATE_CHASE;
            return;
    }
#undef ACTOR_110600_SCALE_SHUDDER_STEP
    task->extra.tmd->coords->coord.t[0] += sideStep.vx;
    task->extra.tmd->coords->coord.t[2] += sideStep.vz;
    rootCoord                            = task->extra.tmd->coords;
    D_actor_110600_80148688.step++;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Builds the enrage tint while oscillating the pose, then accelerates chase.
///
/// Requires live actor work/model/enemy and loaded clip 12. Entry starts slow
/// playback and clears the two-stage sequence. Cue 4 reverses at normal rate;
/// successive ticks halve the signed rate toward zero, replacing 1 with -12
/// and -1 with 8. After 53 oscillation ticks both base and current rates become
/// 56 sixteenths per tick and CHASE is selected. Each oscillation tick adds 39
/// to the tint accumulator; the outer enemy tick applies that tint to lighting.
static void _actor110600EnrageState(Task* task)
{
    enum {
        ACTOR_110600_ENRAGE_STAGE_ENTER       = 0,
        ACTOR_110600_ENRAGE_STAGE_OSCILLATE   = 1,
        ACTOR_110600_ENRAGE_CLIP              = 12,
        ACTOR_110600_ENRAGE_ENTRY_RATE        = 6,
        ACTOR_110600_ENRAGE_REVERSE_RATE      = -12,
        ACTOR_110600_ENRAGE_FORWARD_RATE      = 8,
        ACTOR_110600_ENRAGE_CUE               = 4,
        ACTOR_110600_ENRAGE_OSCILLATION_TICKS = 53,
        ACTOR_110600_ENRAGED_RATE             = 56,
        ACTOR_110600_ENRAGE_TINT_STEP         = 39,
    };

    _Actor110600Work* work;
    Enemy*            enemy;
    TmdObject*        model;
    s16               halvedRate;
    s32               enrageStage;

    work = task->work;
    if (work->stateEntered != 0) {
        enemy                         = task->spawnArg2.pointer;
        model                         = task->extra.tmd;
        model->flags                  = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->animId                  = ACTOR_110600_ENRAGE_CLIP;
        work->animRequest             = ACTOR_110600_ANIM_REQUEST_RESET;
        work->walker.turnLimit        = 0;
        work->lookYaw                 = 0;
        work->animRate                = ACTOR_110600_ENRAGE_ENTRY_RATE;
        _actor110600TickAnimation(task);
        work->stateFrame  = 0;
        work->enrageStage = ACTOR_110600_ENRAGE_STAGE_ENTER;
    }
    enrageStage       = work->enrageStage;
    work->blendActive = 0;
    switch (enrageStage) {
        case ACTOR_110600_ENRAGE_STAGE_ENTER:
            _actor110600TickAnimation(task);
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == ACTOR_110600_ENRAGE_CUE) {
                work->animRate    = -ANIMATION_RATE_ONE;
                work->enrageStage = (s16)((u16)work->enrageStage + 1);
                return;
            }
            return;
        case ACTOR_110600_ENRAGE_STAGE_OSCILLATE:
            // Alternate decaying forward/reverse rates while accumulating the tint.
            halvedRate     = work->animRate / 2;
            work->animRate = halvedRate;
            work->stateFrame++;
            if (work->animRate == enrageStage) {
                work->animRate = ACTOR_110600_ENRAGE_REVERSE_RATE;
            }
            if (work->animRate == -1) {
                work->animRate = ACTOR_110600_ENRAGE_FORWARD_RATE;
            }
            _actor110600TickAnimation(task);
            if (work->stateFrame >= ACTOR_110600_ENRAGE_OSCILLATION_TICKS) {
                work->animRate = ACTOR_110600_ENRAGED_RATE;
                work->baseRate = ACTOR_110600_ENRAGED_RATE;
                work->state    = ACTOR_110600_STATE_CHASE;
            }
            work->enrageTint += ACTOR_110600_ENRAGE_TINT_STEP;
            break;
    }
}

/// The actor's state handlers, indexed by `_Actor110600Work::state`. splat
/// migrates the table into the `.s` of the function that reads it, so it is
/// written out here to keep the block in the unit's `.rodata` now that
/// `func_actor_110600_80137F2C` is decompiled.
static const _Actor110600StateTable D_actor_110600_80131F3C = { {
    _actor110600HiddenState,
    NULL,
    _actor110600PatrolState,
    _actor110600ChaseState,
    _actor110600AlertState,
    _actor110600AttackState,
    _actor110600IdleState,
    _actor110600FallBackState,
    _actor110600FallState,
    _actor110600RiseBackState,
    _actor110600RiseState,
    _actor110600DownState,
    _actor110600DeathBurnState,
    func_actor_110600_80136ECC,
    _actor110600StatusHoldState,
    NULL,
    NULL,
    func_actor_110600_801372CC,
    NULL,
    _actor110600DeathThrashState,
    _actor110600LurkState,
    _actor110600LurkAlertState,
    _actor110600ShudderState,
    _actor110600AlertRewindState,
    _actor110600EnrageState,
} };

/// The actor's enemy tick, the middle entry of the `D_actor_110600_80131FA0`
/// triple `_actor110600Spawn` / this / `enemyDestroy`: copies
/// `D_actor_110600_80131F3C` onto its frame, rebuilds the model root's
/// coordinate and hands its translation to `worldCoordUpdateActorColor`, then switches
/// on `gSceneCombatState.actorControl`.
///
/// Modes 1 and 2 skip the state handler entirely — each clears the three
/// `WorldCollisionContact` tables and returns, mode 2 stamping `field_C` to 0x80 for the
/// hidden pose first, and mode 1 drawing the ground quad on the way unless the
/// model sits in the death or hit pose. Mode 0 draws the quad the same way with
/// `field_C` zeroed and then falls through.
///
/// The fall-through stages the model root's translation into the `gridBody`
/// display node, runs the handler `state` selects out of the stack copy,
/// restages the same three halfwords with Y dropped by 0x124 for the pose it
/// just advanced into, and gives the enemy 1 HP back once the remaining-enemy
/// count has run out. The tail clears the three tables again, marks the root
/// clean, shifts the colour matrix's translation down by the shrink `enrageTint`
/// — the matrix state 12 scales — and keeps `hitBody` out of the ground
/// effect's way by clearing bit 0x8000 while the actor is in a death or hit
/// pose.
static void func_actor_110600_80137F2C(Enemy* arg0, Task* arg1)
{
    VECTOR                 pos;
    _Actor110600StateTable states;
    _Actor110600Work*      work;

    work   = arg1->work;
    states = D_actor_110600_80131F3C;

    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(arg1->extra.tmd->coords);
    pos.vx = arg1->extra.tmd->coords->workm.t[0];
    pos.vy = arg1->extra.tmd->coords->workm.t[1];
    pos.vz = arg1->extra.tmd->coords->workm.t[2];
    worldCoordUpdateActorColor(arg0, &pos, 0, 0);

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if ((work->state != ACTOR_110600_STATE_HIDDEN) && (work->state != ACTOR_110600_STATE_DEATH_BURN)) {
                arg1->extra.tmd->flags = 0;
                effectDrawGroundShadow(MATRIX_TRANS(&arg1->extra.tmd->coords->workm), 0x280, gRoomEffectState->groundShadowShade);
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            if ((work->state != ACTOR_110600_STATE_DEATH_BURN) && (work->state != ACTOR_110600_STATE_HIDDEN)) {
                effectDrawGroundShadow(MATRIX_TRANS(&arg1->extra.tmd->coords->workm), 0x280, gRoomEffectState->groundShadowShade);
            }
            worldCollisionClearContacts(work->gridContacts);
            worldCollisionClearContacts(work->hitContacts);
            worldCollisionClearContacts(work->attackContacts);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            arg1->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            worldCollisionClearContacts(work->gridContacts);
            worldCollisionClearContacts(work->hitContacts);
            worldCollisionClearContacts(work->attackContacts);
            return;
    }

    if (work->prevState != work->state) {
        work->stateEntered = 1;
    } else {
        work->stateEntered = 0;
    }
    work->prevState = (u16)work->state;

    work->gridBody.pos.vx = (u16)arg1->extra.tmd->coords->coord.t[0];
    work->gridBody.pos.vy = (u16)arg1->extra.tmd->coords->coord.t[1];
    work->gridBody.pos.vz = (u16)arg1->extra.tmd->coords->coord.t[2];
    states.handlers[work->state](arg1);
    work->gridBody.pos.vx = (u16)arg1->extra.tmd->coords->coord.t[0];
    work->gridBody.pos.vy = (u16)((u16)arg1->extra.tmd->coords->coord.t[1] - 0x124);
    work->gridBody.pos.vz = (u16)arg1->extra.tmd->coords->coord.t[2];

    if (arg0->hp > 0) {
        if (work->hitCooldown > 0) {
            work->hitCooldown = (s16)((u16)work->hitCooldown - 1);
        } else {
            _actor110600ApplyDamage(arg1);
        }
    }
    if ((arg0->hp <= 0) && (gPlayerStatus.hp <= 0)) {
        arg0->hp = 1;
    }
    worldCollisionClearContacts(work->gridContacts);
    worldCollisionClearContacts(work->hitContacts);
    worldCollisionClearContacts(work->attackContacts);
    if (gGameSession->viewReady != 0) {
        arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (arg1->extra.tmd->coords->composeStamp == GRAPHICS_COORD_DIRTY) {
        work->rootDirty = 1;
    } else {
        work->rootDirty = 0;
    }
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(arg1->extra.tmd->coords);
    work->colorMtx.t[1] -= work->enrageTint;
    work->colorMtx.t[2] -= work->enrageTint;
    work->colorMtx.t[0] -= (work->enrageTint * 2) / 3;
    if ((work->state == ACTOR_110600_STATE_DEATH_BURN) || (work->state == ACTOR_110600_STATE_HIDDEN) || (work->state == ACTOR_110600_STATE_DEATH_BURST) || (work->state == ACTOR_110600_STATE_DEATH_THRASH)) {
        work->hitBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    } else {
        work->hitBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
}

/// Receives message 2015 without changing the actor.
///
/// Both payload words and the ID are ignored. This handler has no result;
/// senders must not consume the dispatch result register.
static void _actor110600IgnoreMessage2015(Task* task, s32 messageId, s32 firstArg, s32 secondArg)
{
}

/// The enemy task's three state handlers - spawn, per-frame tick and teardown -
/// that `_actor110600Task` dispatches through by `Task::state`.
static const EnemyTaskFuncTable3 D_actor_110600_80131FA0 = {
    _actor110600Spawn,
    func_actor_110600_80137F2C,
    enemyDestroy,
};

/// Selects a scripted clip and restarts entry into the SCRIPTED state.
///
/// Borrows a readable request through dispatch. Selectors 0..4 map to clips
/// 34, 35, 36, 37 and 40; 0 and 4 also save the current enemy pose with resume
/// state 1. Other selectors keep the clip but still restart SCRIPTED. Only
/// `animationId` is read; bank/blend fields, ID and second payload are ignored.
/// Requires live actor work/model, enemy and loaded selected clip; returns 0.
static s32 _actor110600PlayScriptedAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 secondArg)
{
    enum { ACTOR_110600_RESUME_SCRIPTED = 1 };

    _Actor110600Work* work;
    Enemy*            enemy;
    s32               animationSelector;

    animationSelector = request->animationId;
    work              = task->work;
    enemy             = task->spawnArg2.pointer;
    switch (animationSelector) {
        case 0:
            work->animId      = ACTOR_110600_ANIM_SCRIPTED_SLOT0;
            enemy->spawnState = ACTOR_110600_RESUME_SCRIPTED;
            areaSaveEnemyPose(enemy);
            break;
        case 1:
            work->animId = ACTOR_110600_ANIM_SCRIPTED_SLOT1;
            break;
        case 2:
            work->animId = ACTOR_110600_ANIM_SCRIPTED_SLOT2;
            break;
        case 3:
            work->animId = ACTOR_110600_ANIM_SCRIPTED_SLOT3;
            break;
        case 4:
            enemy->spawnState = ACTOR_110600_RESUME_SCRIPTED;
            areaSaveEnemyPose(enemy);
            work->animId = ACTOR_110600_ANIM_SCRIPTED_REQUEST4;
            break;
    }
    work->state     = ACTOR_110600_STATE_SCRIPTED;
    work->prevState = ACTOR_110600_PREV_STATE_NONE;
    return 0;
}

/// Sets draw/buffer handling while respecting the enemy's saved resume state.
///
/// Requires live actor work/model and enemy. Mode 0 hides, allocates buffers
/// and enters HIDDEN. Mode 1 shows and allocates unless resume state 4 keeps it
/// hidden. Mode 2 disables automatic buffering and enters HIDDEN. Mode 3 sets
/// visibility from resume state 4, enters HIDDEN and disables automatic
/// buffering. Other modes do nothing. Ignores ID/second payload; returns 0.
static s32 _actor110600SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 secondArg)
{
    enum {
        ACTOR_110600_DRAW_HIDE_ALLOCATE           = 0,
        ACTOR_110600_DRAW_SHOW_ALLOCATE           = 1,
        ACTOR_110600_DRAW_SKIP_AUTO_BUFFER        = 2,
        ACTOR_110600_DRAW_RESUME_SKIP_AUTO_BUFFER = 3,
        ACTOR_110600_RESUME_HIDDEN                = 4,
    };

    TmdObject*        model;
    _Actor110600Work* work;
    Enemy*            enemy;

    model = task->extra.tmd;
    enemy = task->spawnArg2.pointer;
    work  = task->work;
    switch (drawMode) {
        case ACTOR_110600_DRAW_HIDE_ALLOCATE:
            model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(model);
            work->state = ACTOR_110600_STATE_HIDDEN;
            break;
        case ACTOR_110600_DRAW_SHOW_ALLOCATE:
            if (enemy->spawnState == 0) {
                model->flags = 0;
                tmdAllocPrimitiveBuffer(model);
            } else if (enemy->spawnState == ACTOR_110600_RESUME_HIDDEN) {
                model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                work->state  = ACTOR_110600_STATE_HIDDEN;
            } else {
                model->flags = 0;
                tmdAllocPrimitiveBuffer(model);
            }
            break;
        case ACTOR_110600_DRAW_SKIP_AUTO_BUFFER:
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->state   = ACTOR_110600_STATE_HIDDEN;
            break;
        case ACTOR_110600_DRAW_RESUME_SKIP_AUTO_BUFFER:
            if (enemy->spawnState == ACTOR_110600_RESUME_HIDDEN) {
                model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            } else {
                model->flags = 0;
            }
            work->state   = ACTOR_110600_STATE_HIDDEN;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
    }
    return 0;
}

/// Returns 1 while the enemy is alive, otherwise clears its enrage effects.
///
/// Requires live actor work and enemy. A nonpositive health value clears the
/// work's enrage tint/latch and the enemy's reaction flags before returning 0.
/// The ID and both payloads are ignored.
static s32 _actor110600IsPresent(Task* task, s32 messageId, s32 firstArg, s32 secondArg)
{
    _Actor110600Work* work;
    Enemy*            enemy;

    enemy = task->spawnArg2.pointer;
    work  = task->work;
    if (enemy->hp > 0) {
        return 1;
    }
    work->enrageTint     = 0;
    enemy->reactionFlags = 0;
    work->enraged        = 0;
    return 0;
}

#include "../../shared/coord_math_yaw_scale.inc.c"

/// Replaces a coordinate's rotation with its current yaw and independent axis scales.
///
/// Requires a live writable word-aligned coordinate outside the scratch stack.
/// Heading uses 4096 units per turn. The three factors are signed Q12 (`ONE`
/// is unity); zero collapses an axis and a negative factor reverses it. Scaling
/// narrows coefficients to halfwords without saturation. Discards pitch/roll
/// and prior scale, preserves translation/parent/Euler angles and marks the
/// composition cache dirty. No recovered caller uses this standalone helper.
///
/// The initialized scratch stack needs 0x58 free aligned bytes for the scale
/// block and nested yaw workspace. Both are released before return.
static void _actorRenderSetYawAxisScales(GfxCoord* coord, s16 scaleX, s16 scaleY, s16 scaleZ)
{
    ActorScaleRotScratch* yawScratch;
    s16                   yaw;

    yawScratch      = SCRATCH_STACK_RESERVE_BLOCK(ActorScaleRotScratch);
    yaw             = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    yawScratch->yaw = yaw;
    gfxRotMatrixY(&yawScratch->rotation, yaw, GRAPHICS_ROTATION_REPLACE);
    yawScratch->scale.vx = scaleX;
    yawScratch->scale.vy = scaleY;
    yawScratch->scale.vz = scaleZ;
    ScaleMatrix(&yawScratch->rotation, &yawScratch->scale);

    _actorRenderCopyRotation(coord, yawScratch->rotation.m);
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleRotScratch);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Acquires a battle reference and switches the actor to ALERT.
///
/// Requires live actor work and scene combat state. Does not reset `prevState`;
/// the enemy tick detects the state change. Repeated messages acquire another
/// reference. Ignores the ID and both payloads; returns 1.
static s32 _actor110600Alert(Task* task, s32 messageId, s32 firstArg, s32 secondArg)
{
    _Actor110600Work* work;

    work = task->work;
    sceneAcquireBattleRef(0);
    work->state = ACTOR_110600_STATE_ALERT;
    return 1;
}

/// Releases the actor's collision and child-task bindings before destroying it.
///
/// Installed as the task exit callback. Requires a live enemy and model task;
/// work may be NULL after an allocation failure. When work exists, advances
/// each present child task one state, unlinks the attack/hit/grid bodies and
/// clears the enemy's borrowed contact table. Then stops screen shake and
/// lets `enemyDestroy` release the enemy and task resources.
static void _actor110600Exit(Task* task)
{
    _Actor110600Work* work;
    Enemy*            enemy;
    Task*             firstChildTask;
    Task*             secondChildTask;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work != NULL) {
        firstChildTask = work->childTask0;
        if (firstChildTask != NULL) {
            firstChildTask->state++;
        }
        secondChildTask = work->childTask1;
        if (secondChildTask != NULL) {
            secondChildTask->state++;
        }
        // End all borrowed collision bindings before releasing the work block.
        worldCollisionUnlinkBody(&work->attackBody);
        worldCollisionUnlinkBody(&work->hitBody);
        worldCollisionUnlinkBody(&work->gridBody);
        enemy->recs = NULL;
    }
    displaySetShakeY(0);
    enemyDestroy(enemy, task);
}

/// Hides the actor and disables targeting, attacks and grid correction on entry.
///
/// Requires live actor work/model/enemy. Retains the existing model flags while
/// adding the skip-draw bit. Does not advance animation or the walker; later
/// messages or spawn behavior must select another state.
static void _actor110600HiddenState(Task* task)
{
    TmdObject*        model;
    _Actor110600Work* work;
    Enemy*            enemy;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy                         = task->spawnArg2.pointer;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                  = (u16)(model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW);
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
    }
}

static s32 func_actor_110600_80138900(void)
{
    s16* p;
    s16  next;
    s32  cur;

    p    = &D_actor_110600_8014865C;
    next = (u16)*p + 1;
    *p   = next;
    if (next == 5) {
        *p = 0;
    }
    cur = (u16)*p;
    if ((cur & 1) == 0) {
        displaySetShakeY(0);
    } else {
        displaySetShakeY(1);
    }
    if (D_actor_110600_8014865C != 0) {
        return 0;
    }
    displaySetShakeY(0);
    return 1;
}

/// Plays the forward fall while edging ahead, then selects DOWN or DEATH_BURN.
///
/// Requires live actor work/model/enemy and loaded clip 12. Entry disables
/// attacks and turning and ramps toward a two-unit forward step by eight units
/// per tick. The slot-1 boundary chooses DOWN while health is positive, otherwise
/// DEATH_BURN. No recovered state writer selects FALL.
static void _actor110600FallState(Task* task)
{
    enum { ACTOR_110600_FALL_FORWARD_SPEED = 2,
           ACTOR_110600_FALL_SPEED_STEP    = 8 };

    _Actor110600Work*   work;
    BossStrangerWalker* walker;
    Enemy*              enemy;
    u16                 entrySpeed;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        task->extra.tmd->flags        = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->animRequest             = ACTOR_110600_ANIM_REQUEST_RESET;
        work->animId                  = ACTOR_110600_ANIM_FALL;
        work->animRate                = ANIMATION_RATE_ONE;
        ACTOR_110600_ENTER_IDLE_WALKER(work, walker, entrySpeed, ACTOR_110600_FALL_FORWARD_SPEED, ACTOR_110600_FALL_SPEED_STEP);
    }
    _bossStrangerTick(&work->walker);
    _actor110600TickAnimation(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        if (enemy->hp > 0) {
            work->state = ACTOR_110600_STATE_DOWN;
        } else {
            work->state = ACTOR_110600_STATE_DEATH_BURN;
        }
    }
}

/// Holds the fallen pose for a random delay before choosing its recovery clip.
///
/// Requires live actor work. Entry draws a delay of 0..31 ticks; decrementing
/// below zero selects RISE_BACK after clip 30 or RISE after clip 12. Other clip
/// IDs leave the state unchanged. Neither animation nor movement advances here.
static void _actor110600DownState(Task* task)
{
    enum { ACTOR_110600_DOWN_DELAY_MASK = 31 };

    _Actor110600Work* work;
    u32               randomValue;
    s16               remainingTicks;

    work = task->work;
    if (work->stateEntered != 0) {
        randomValue      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState  = randomValue;
        work->downFrames = (randomValue >> 16) & ACTOR_110600_DOWN_DELAY_MASK;
    }
    remainingTicks   = work->downFrames - 1;
    work->downFrames = remainingTicks;
    if (remainingTicks < 0) {
        switch (work->animId) {
            case ACTOR_110600_ANIM_FALL_BACK_END:
                work->state = ACTOR_110600_STATE_RISE_BACK;
                return;
            case ACTOR_110600_ANIM_FALL:
                work->state = ACTOR_110600_STATE_RISE;
                break;
        }
    }
}

/// Plays the recovery from a backward fall, then resumes CHASE.
///
/// Requires live actor work/model/enemy and loaded clip 15. Entry resets
/// the clip at the variant base rate, disables attacks/turning and ramps
/// movement toward zero by eight units per tick. Walker and animation both
/// advance; the slot-1 boundary selects CHASE.
static void _actor110600RiseBackState(Task* task)
{
    enum { ACTOR_110600_RISE_DECELERATION = 8 };

    _Actor110600Work*   work;
    BossStrangerWalker* walker;
    Enemy*              enemy;
    u16                 entrySpeed;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        task->extra.tmd->flags        = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->animRequest             = ACTOR_110600_ANIM_REQUEST_RESET;
        work->animId                  = ACTOR_110600_ANIM_RISE_BACK;
        work->animRate                = work->baseRate;
        ACTOR_110600_ENTER_IDLE_WALKER(work, walker, entrySpeed, 0, ACTOR_110600_RISE_DECELERATION);
    }
    _bossStrangerTick(&work->walker);
    _actor110600TickAnimation(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ACTOR_110600_STATE_CHASE;
    }
}

/// Plays the recovery from a forward fall, then resumes CHASE.
///
/// Requires live actor work/model/enemy and loaded clip 16. Entry resets
/// the clip at the variant base rate, disables attacks/turning and ramps
/// movement toward zero by eight units per tick. Walker and animation both
/// advance; the slot-1 boundary selects CHASE.
static void _actor110600RiseState(Task* task)
{
    enum { ACTOR_110600_RISE_DECELERATION = 8 };

    _Actor110600Work*   work;
    BossStrangerWalker* walker;
    Enemy*              enemy;
    u16                 entrySpeed;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        task->extra.tmd->flags        = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->animRequest             = ACTOR_110600_ANIM_REQUEST_RESET;
        work->animId                  = ACTOR_110600_ANIM_RISE;
        work->animRate                = work->baseRate;
        ACTOR_110600_ENTER_IDLE_WALKER(work, walker, entrySpeed, 0, ACTOR_110600_RISE_DECELERATION);
    }
    _bossStrangerTick(&work->walker);
    _actor110600TickAnimation(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ACTOR_110600_STATE_CHASE;
    }
}

#undef ACTOR_110600_ENTER_IDLE_WALKER

/// Seeds the alert pose ahead, then plays it backward at half rate.
///
/// Requires live actor work/model/enemy and loaded alert tracks. Entry makes
/// the actor un-lockable, disables attacks and blending, applies the alert reset
/// and advances twenty more normal-rate ticks. Every callback then advances at
/// -8 sixteenths of a frame per tick. No automatic state transition ends rewind.
static void _actor110600AlertRewindState(Task* task)
{
    enum { ACTOR_110600_ALERT_WARMUP_TICKS = 20,
           ACTOR_110600_ALERT_REVERSE_RATE = -8 };

    _Actor110600Work* work;
    Enemy*            enemy;
    TmdObject*        model;
    s16               warmupTick;

    work       = task->work;
    warmupTick = 0;
    if (work->stateEntered != 0) {
        enemy                         = task->spawnArg2.pointer;
        model                         = task->extra.tmd;
        model->flags                  = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->animId                  = ACTOR_110600_ANIM_ALERT;
        work->animRequest             = ACTOR_110600_ANIM_REQUEST_RESET;
        work->walker.turnLimit        = 0;
        work->lookYaw                 = 0;
        work->animRate                = ANIMATION_RATE_ONE;
        work->blendActive             = 0;
        // Apply the reset, then advance twenty normal-rate ticks before rewinding.
        _actor110600TickAnimation(task);
        work->stateFrame = 0;
        for (; warmupTick < ACTOR_110600_ALERT_WARMUP_TICKS; warmupTick++) {
            _actor110600TickAnimation(task);
        }
    }
    work->animRate = ACTOR_110600_ALERT_REVERSE_RATE;
    _actor110600TickAnimation(task);
}

/// Halves status-hold playback, reversing at +1 and restarting forward at -1.
///
/// The live work rate uses signed sixteenth-frame units. Division truncates
/// toward zero; zero stays zero, and reaching either unit restarts the opposite
/// direction at one full frame per tick. Does not advance animation.
static __inline__ void _actor110600OscillateStatusRate(_Actor110600Work* work)
{
    s16 halvedRate;

    halvedRate     = work->animRate / 2;
    work->animRate = halvedRate;
    if (halvedRate == 1) {
        work->animRate = -ANIMATION_RATE_ONE;
    }
    if (work->animRate == -1) {
        work->animRate = ANIMATION_RATE_ONE;
    }
}

/// Oscillates the attack pose until the enemy status buildup expires.
///
/// Requires live actor work/model/enemy and loaded attack tracks. Entry shows
/// and buffers the model, disables attacks and starts clip 5 at three frames
/// per tick, advancing twice. Later ticks halve the signed rate toward zero;
/// +1 restarts at -16 and -1 at +16, in sixteenth-frame units. Buildup completion
/// clears its reaction bits and selects CHASE. Entry does not consume buildup.
static void _actor110600StatusHoldState(Task* task)
{
    enum { ACTOR_110600_STATUS_INITIAL_RATE = 3 * ANIMATION_RATE_ONE };

    _Actor110600Work* work;
    Enemy*            enemy;
    TmdObject*        model;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->animRequest       = ACTOR_110600_ANIM_REQUEST_RESET;
        work->animId            = ACTOR_110600_ANIM_ATTACK;
        work->animRate          = ACTOR_110600_STATUS_INITIAL_RATE;
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        _actor110600TickAnimation(task);
        _actor110600TickAnimation(task);
        return;
    }
    // Alternate decaying forward and reverse playback while buildup remains.
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _actor110600OscillateStatusRate(work);
    _actor110600TickAnimation(task);
    if (damageTickEnemyBuildup(enemy) == 1) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->state           = ACTOR_110600_STATE_CHASE;
    }
}

/// Dispatches the Boss Stranger task through spawn, update and teardown.
///
/// The task must be live with state 0..2 and its Enemy in spawnArg2. Copies the
/// three callback pointers to a local table before dispatch; no bounds check is
/// performed. The actor overlay must stay loaded, and teardown may free the task.
static void _actor110600Task(Task* task)
{
    EnemyTaskFuncTable3 handlers;

    handlers = D_actor_110600_80131FA0;
    handlers.funcs[task->state](task->spawnArg2.pointer, task);
}
