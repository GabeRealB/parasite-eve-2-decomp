#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include <psyq/abs.h>
#include <psyq/memory.h>
#include <psyq/stdio.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"
#include "actors/hit_effect.h"
#include "stages/akropolis/actors/cafeteria_boss_stranger.h"

#include "actors/actor_210600.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/enemy_params.h"
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
#include "../../../../shared/actor/player_detection.h"
#include "../../../../shared/actor/actor_contacts.h"
#include "../../../../shared/creatures/boss_stranger/boss_stranger.h"

/// Scratch-stack block the patrol layout works in while it places the walker's nav nodes.
typedef struct {
    SVECTOR offset;    // Node's offset from the walker: the facing axis of `rotation`, then that axis scaled; `pad` is never written
    MATRIX  rotation;  // Copy of the walker coordinate's matrix, turned about Y once more for each node; only its rotation is read
    s16     nodeIndex; // Node being placed, from 1; then the route step being seeded, from 0, left on the end marker's slot
} _PatrolLayoutScratch;
STATIC_ASSERT_SIZEOF(_PatrolLayoutScratch, 0x2C);

/// Values of `_Work::state`: the index of the handler the per-frame tick runs. `RESTORED` is the only number without a
/// handler that is ever stored; what replaces it before the tick dispatches is not in this package. Nothing here selects
/// the states marked "never selected", which leaves `RISE_BACK`, `RISE` and `DOWN` unreachable as well.
enum {
    STATE_HIDDEN       = 0x00,          // not drawn, not lockable, `attackBody` and `gridBody` off
    STATE_RESTORED     = 0x01,          // set at spawn for an enemy with a saved spawn state; has no handler
    STATE_PATROL       = 0x02,          // walks the two `navNodes` until the player comes within a notice range
    STATE_CHASE        = 0x03,          // walks at the player, each footstep shaking the screen; attacks when close
    STATE_ALERT        = 0x04,          // plays `alert` with the look following the player, then chases
    STATE_ATTACK       = 0x05,          // swings at the player with `attackBody` enabled, then chases
    STATE_IDLE         = 0x06,          // stands on `idle`, now and then playing `idle_variation`; a hit alerts it
    STATE_FALL_BACK    = 0x07,          // plays `fall_back` and `fall_back_end`, then `DOWN` or, dead, `DEATH_BURN`; never selected
    STATE_FALL         = 0x08,          // plays `fall` edging forward, then `DOWN` or, dead, `DEATH_BURN`; never selected
    STATE_RISE_BACK    = 0x09,          // plays `rise_back` after a `DOWN` that followed `FALL_BACK`, then chases
    STATE_RISE         = 0x0A,          // plays `rise` after a `DOWN` that followed `FALL`, then chases
    STATE_DOWN         = 0x0B,          // holds its pose for `downFrames`, then rises
    STATE_DEATH_BURN   = 0x0C,          // burns away: the corpse-burn effect, then the model flattens, fades and stops being drawn
    STATE_DEATH_BURST  = 0x0D,          // stops drawing the model and throws five body parts from it; never selected
    STATE_STATUS_HOLD  = 0x0E,          // twitches in place until the status buildup runs out, then chases
    STATE_SCRIPTED     = 0x11,          // plays the animation a room command or a play request put in `animId`
    STATE_DEATH_THRASH = 0x13,          // turns in random jerks through `thrash`, plays `thrash_end`, then `DEATH_BURN`; never selected
    STATE_LURK         = 0x14,          // holds `scripted_0` until the player comes within 3000 or a hit alerts it; entered by a patio command
    STATE_LURK_ALERT   = 0x15,          // the alert that ends `LURK`: not lockable, then chases
    STATE_SHUDDER      = 0x16,          // jolts the root side to side for five ticks, then chases; never selected
    STATE_ALERT_REWIND = 0x17,          // runs `alert` forward 20 ticks on entry, then plays it backward until another state is set; entered by a cafeteria command
    STATE_ENRAGE       = 0x18,          // convulses on `fall` while `enrageTint` builds, then chases at a faster `baseRate`; entered once, by the hit that takes it below half its hit points
    CAFETERIA_BOSS_STRANGER_STATE_COUNT // number of states, and of the handlers in `_StateTable`
};

/// Parts of the Stranger rig the boss reaches; its aim point and attack sphere ride the lower neck.
enum {
    PELVIS_PART          = 1,
    CHEST_PART           = 2,
    LOWER_NECK_PART      = 3,
    HEAD_PART            = 5,
    JAW_PART             = 6,
    RIGHT_UPPER_ARM_PART = 7,
    RIGHT_FOREARM_PART   = 8,
    LEFT_UPPER_ARM_PART  = 9,
    LEFT_FOREARM_PART    = 10,
    RIGHT_THIGH_PART     = 11,
    RIGHT_FOOT_PART      = 14,
    LEFT_THIGH_PART      = 15,
    LEFT_FOOT_PART       = 18,
};

/// Cue indices on the pelvis track of `walk` where its two footsteps fall: each sounds its step and, in `CHASE`, shakes
/// the screen.
enum {
    WALK_STEP_A_CUE = 0x26,
    WALK_STEP_B_CUE = 0x33,
};

/// Values of `_Work::animRequest` and `_Work::blendRequest`.
enum {
    ANIM_REQUEST_BLEND   = 1, // seek the slots to the animation, blending over the frames its transition table gives
    ANIM_REQUEST_RESET   = 2, // restart the slots on the animation
    ANIM_REQUEST_PLAYING = 3, // the request has been applied
    ANIM_REQUEST_SETTLE  = 6  // restart the slots on the animation and run them 99 ticks before it shows; never requested
};

/// Actor-specific message slots; both payload words are ignored.
enum {
    MESSAGE_ALERT  = 2007,
    MESSAGE_IGNORE = 2015,
};

/// How far `gridBody` sits above the root.
enum { GRID_BODY_HEIGHT = 292 };

/// `baseRate` once `ENRAGE` has run, at which `CHASE` walks faster.
enum { ENRAGED_BASE_RATE = 56 };

/// Forces a message-selected state to run its entry setup on the next tick.
enum { PREV_STATE_NONE = -1 };

/// Work block of the Boss Stranger task.
typedef struct {
    s16                   state;                // `STATE_*`
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
    s16                   animRequest;          // `ANIM_REQUEST_*` for `rig`
    s16                   blendActive;          // 1 while `blendRig`'s animation is mixed in; cleared when its slot 1 reaches its boundary
    s16                   appliedAnim;          // animation `rig` was last started on; the row of the transition table a blend request reads
    s16                   animId;               // animation requested of `rig`
    s16                   animFrames;           // ticks since `animRequest` was last applied; `DEATH_THRASH` ends its first animation at 31
    s16                   animRate;             // playback rate of `rig`'s slots; negative plays backward
    s16                   baseRate;             // `animRate` most states start on: 20 or 16 by variant, `ENRAGED_BASE_RATE` once `ENRAGE` has run
    s16                   blendRequest;         // `ANIM_REQUEST_*` for `blendRig`
    s16                   blendAnimId;          // animation requested of `blendRig`; a hit asks for `hit_reaction`
    s16                   blendRate;            // playback rate of `blendRig`'s slots, 0x30 from each restart
    s16                   blendWeight;          // share of `blendRig`'s pose in the mix, of 0x1000; 0xB78 from each restart
    s16                   lookYawTarget;        // bearing to the player relative to the facing, or 0
    s16                   lookYaw;              // eased toward `lookYawTarget` by 0x100 a tick; within +-0x400, the head turns by it and the lower neck by a quarter
    s16                   burnHeightScale;      // height scale `DEATH_BURN` flattens the root from: the walker's scale on entry, 8 less a tick from tick 201 while above 0x800
    byte                  pad_8A8[0x2];         // never accessed
    s16                   hitCooldown;          // ticks before another hit is taken; set from the hit's id parameter 2
    s32                   lastCueIndex;         // slot 1's cue index (the feet's on `run`) as the sound cues last saw it, so a held cue fires once; 0 from each applied request
    s8                    field_8B0;            // cleared at spawn and never accessed again; role unproven
    s8                    rootDirty;            // 1 when the tick left the root coordinate marked dirty, which a ready view forces; never read
    byte                  pad_8B2[0x2];         // never accessed
    s16                   flinchSpeedScale;     // sets the `CHASE` step while `blendActive`: this times `animRate` over 1520, halved, so 0 at the starting rates
    s16                   walkSpeed;            // forward step per tick at rate 16 (10, 8 or 14 by variant): `PATROL` walks at it, `CHASE` at it times `animRate` over 16
    WorldCollisionBody    hitBody;              // sphere ahead of and above the root that takes the hits, off in `HIDDEN` and the death states; moved onto the chest by the killing hit
    WorldCollisionContact hitContacts[5];       // contacts of `hitBody`; also the enemy's hit records and what the walker avoids
    WorldCollisionBody    gridBody;             // sphere `GRID_BODY_HEIGHT` above the root, in world space, that the room grid pushes the walker with; also takes hits
    WorldCollisionContact gridContacts[12];     // contacts of `gridBody`, which the walker's ground step measures
    WorldCollisionBody    attackBody;           // sphere on the lower neck carrying the key of the attack in use; enabled only between two cue indices of `ATTACK`
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
    s16  enraged;                               // 1 once `ENRAGE` has been entered: hits do half damage, and `ATTACK` plays `enraged_attack` with attack 0 in place of `attack` with attack 1; cleared with the tint when a dead actor is asked whether it is present
    s16  footstepShake;                         // nonzero while a footstep's five-tick screen shake runs (1 or 2 by the cue index that started it)
    byte pad_BEA[0x2];                          // never accessed
} _Work;
STATIC_ASSERT_SIZEOF(_Work, 0xBEC);

/// Stops steering and starts an idle speed ramp from the current step.
#define ENTER_IDLE_WALKER(actorWork, walkerAlias, savedSpeed, targetValue, stepValue) \
    {                                                                                 \
        (savedSpeed)                  = (actorWork)->walker.speed;                    \
        (walkerAlias)                 = &(actorWork)->walker;                         \
        (actorWork)->walker.state     = BOSS_STRANGER_WALKER_IDLE;                    \
        (walkerAlias)->speedTarget    = (targetValue);                                \
        (walkerAlias)->speed          = (savedSpeed);                                 \
        (walkerAlias)->speedStep      = (stepValue);                                  \
        (actorWork)->walker.turnLimit = 0;                                            \
        (actorWork)->lookYaw          = 0;                                            \
        (actorWork)->lookYawTarget    = 0;                                            \
    }

/// Scratch-stack block of the damage step, which runs each tick the actor has health left and no hit cooldown running,
/// and reserves a block only while the player is alive.
typedef struct {
    VECTOR  toPlayer;       // Player's position minus the root's; never read back, and `pad` is never written
    SVECTOR hitOffset;      // `hitPos` minus the root's composed translation; `vx` and `vz` give the hit's bearing, and `pad` is never written
    SVECTOR hitPos;         // Point of the contact found; `pad` is never written
    s32     hitKey;         // Key of the contact found: the kind over the attack's packed id; 0 when neither body holds a damaging contact
    u32     damage;         // Damage of the hit: the roll for the range, times 5 on a critical roll, doubled when `hitYaw` reads beyond 0x500 either way and halved once enraged; then the damage of the over-time tick
    s32     playerDistance; // Length of `toPlayer`, the range the damage is rolled for; never read back
    s16     hitYaw;         // Bearing of `hitOffset` off the facing, wrapped to [-0x800, 0x800]. The doubling test reads it before this hit's bearing is stored, so it sees whatever the block's bytes held when it was reserved
} _HitScratch;
STATIC_ASSERT_SIZEOF(_HitScratch, 0x30);

/// The actor's state handlers, indexed by `_Work::state`.
typedef struct {
    TaskFunc handlers[CAFETERIA_BOSS_STRANGER_STATE_COUNT]; // Handler of each state, taking the actor's task
} _StateTable;
STATIC_ASSERT_SIZEOF(_StateTable, CAFETERIA_BOSS_STRANGER_STATE_COUNT * sizeof(TaskFunc));

static const _StateTable _gCafeteriaBossStrangerStateHandlers;

static s32 _cafeteriaBossStrangerApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 secondArg);

static s32 _cafeteriaBossStrangerPlayScriptedAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 secondArg);

static void _actorRenderSetYawAxisScales(GfxCoord* coord, s16 scaleX, s16 scaleY, s16 scaleZ);

static s32  _cafeteriaBossStrangerAlert(Task* task, s32 messageId, s32 firstArg, s32 secondArg);
static void _cafeteriaBossStrangerHiddenState(Task* task);

static void _cafeteriaBossStrangerChaseState(Task* task);
static void _cafeteriaBossStrangerDeathBurnState(Task* task);
static void _cafeteriaBossStrangerScriptedState(Task* task);
static void _cafeteriaBossStrangerLurkState(Task* task);
static void _cafeteriaBossStrangerFallState(Task* task);
static void _cafeteriaBossStrangerDownState(Task* task);
static void _cafeteriaBossStrangerRiseBackState(Task* task);
static void _cafeteriaBossStrangerRiseState(Task* task);

static void _cafeteriaBossStrangerTickAnimation(Task* task);

static s32 _cafeteriaBossStrangerPollAnimationSound(_Work* work);

static void _cafeteriaBossStrangerAlertState(Task* task);

static void _cafeteriaBossStrangerSpawnHitEffect(Task* task, s16 hitYaw, s32 attackKey);

static void _cafeteriaBossStrangerAlertRewindState(Task* task);

static void _cafeteriaBossStrangerStatusHoldState(Task* task);

static s32 _cafeteriaBossStrangerPlace(Task* task, s32 messageId, const ActorTransform* placement, s32 secondArg);

static s32 _cafeteriaBossStrangerStepFootstepShake(void);

static void _cafeteriaBossStrangerExit(Task* task);

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

/// Diagnostic when the Boss Stranger's node-order plan finds no candidate slot pair.
static const char _gBossStrangerNoPlanPairMessage[] = "s->root_cnt == 0xff about \n";

static s32  _cafeteriaBossStrangerSetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 secondArg);
static s32  _cafeteriaBossStrangerIsPresent(Task* task, s32 messageId, s32 firstArg, s32 secondArg);
static void _cafeteriaBossStrangerIgnoreMessage(Task* task, s32 messageId, s32 firstArg, s32 secondArg);

static TmdSource _gAkropolisCafeteriaBossStrangerBody;
static void      _cafeteriaBossStrangerTask(Task* task);

/// Swing attacks, indexed 0 for the enraged swing and 1 for the ordinary swing.
static DamageAttack _gCafeteriaBossStrangerAttacks[2] = {
    { 18, GAME_ACTOR_REACTION_SPARK_AND_PUFFS },
    { 15, GAME_ACTOR_REACTION_SPARK_AND_PUFFS },
};

/// Attacks of the unused 30-HP parameter preset; no live attack selects these rows.
static DamageAttack _gCafeteriaBossStrangerUnusedAttacks[2] = {
    { 18, GAME_ACTOR_REACTION_SPARK_AND_PUFFS },
    { 18, GAME_ACTOR_REACTION_ORDINARY },
};

/// Combat and reward parameters shared by the actor's live Enemy instances.
static EnemyParams _gCafeteriaBossStrangerEnemyParams = {
    .attacks              = _gCafeteriaBossStrangerAttacks,
    .hpMax                = 350,
    .exp                  = 300,
    .bp                   = 200,
    .mp                   = 30,
    .critChance           = 100,
    .buildupSteps         = 4,
    .damageOverTimeChance = 100,
    .damageOverTimeTicks  = 0,
};

/// Unused combat/reward preset with 30 HP; no recovered spawn installs it.
static EnemyParams _gCafeteriaBossStrangerUnusedEnemyParams = {
    .attacks              = _gCafeteriaBossStrangerUnusedAttacks,
    .hpMax                = 30,
    .exp                  = 42,
    .bp                   = 82,
    .mp                   = 4,
    .critChance           = 250,
    .buildupSteps         = 0,
    .damageOverTimeChance = 100,
    .damageOverTimeTicks  = 0,
};

/// The Odd Stranger's three combat tunings (down ticks, sidestep angle and delay, notice radius), as actor_401000
/// reads them; this package carries the table but nothing here reads it.
static ActorStrangerVariant _gCafeteriaBossStrangerUnusedStrangerVariants[3] = {
    { 20, 900, 12, 2000, { 0, 0, 0, 0 } },
    { 10, 800, 12, 2500, { 0, 0, 0, 0 } },
    { 0, 500, 12, 3000, { 0, 0, 0, 0 } },
};

#include "assets/rooms/akropolis/cafeteria/boss_stranger_schema.h"
#include "assets/enemies/stranger/sounds.h"

#include "assets/rooms/akropolis/cafeteria/boss_stranger_body.inc"

#include "assets/rooms/akropolis/cafeteria/boss_stranger_clips.inc"

/// Signed blend durations in normal-rate frames, indexed [applied clip][requested clip]. The triple-hit follow-up sets
/// its `scripted_2` to `fall_back_end` entry to six frames, which stays while the overlay is loaded.
static s8 _gCafeteriaBossStrangerAnimBlendFrames[45][45] = {
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

/// Animation-set bank borrowed by both nineteen-part rigs for the overlay's lifetime. Its `run`, `idle`,
/// `idle_variation` and `thrash_end` slots are empty: the run steps' cues, `IDLE` and `DEATH_THRASH` are code this
/// package carries without the clips.
#include "assets/rooms/akropolis/cafeteria/boss_stranger_bank.inc"

/// Hit-effect anchors grouped by yaw: front 0..4 on the chest, rear 5..7, sides 8..11 on an upper arm or the chest. The
/// random masks never pick the front's row 4 or the rear's row 6.
static ActorHitAnchor _gCafeteriaBossStrangerHitAnchors[12] = {
    { .anchor = { 60, -12, 30, CHEST_PART } },
    { .anchor = { -50, -130, 29, CHEST_PART } },
    { .anchor = { 20, -70, 25, CHEST_PART } },
    { .anchor = { -30, -65, 25, CHEST_PART } },
    { .anchor = { 60, -120, 30, CHEST_PART } },
    { .anchor = { 20, -20, -5, CHEST_PART } },
    { .anchor = { -15, -50, 0, CHEST_PART } },
    { .anchor = { 2, 10, -15, CHEST_PART } },
    { .anchor = { 14, 0, 0, RIGHT_UPPER_ARM_PART } },
    { .anchor = { 25, 0, 0, CHEST_PART } },
    { .anchor = { -14, 0, 0, LEFT_UPPER_ARM_PART } },
    { .anchor = { -25, 0, 0, CHEST_PART } },
};

/// Messages the live actor answers. The table has no end marker, so every sender must send one of these seven.
static TaskMessageEntry _gCafeteriaBossStrangerMessages[7] = {
    { MESSAGE_IGNORE, _cafeteriaBossStrangerIgnoreMessage },
    { ACTOR_MESSAGE_PLAY_ANIMATION, _cafeteriaBossStrangerPlayScriptedAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _cafeteriaBossStrangerSetModelDraw },
    { ACTOR_MESSAGE_IS_PRESENT, _cafeteriaBossStrangerIsPresent },
    { ACTOR_MESSAGE_PLACE, _cafeteriaBossStrangerPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _cafeteriaBossStrangerApplyCommand },
    { MESSAGE_ALERT, _cafeteriaBossStrangerAlert },
};

/// Overlay-wide footstep-shake phase, cycling 1..4 then 0 once per shake tick.
static s16 _gCafeteriaBossStrangerFootstepShakePhase = 0;

/// Two VRAM rectangles nothing here reads: the sources of the cafeteria's post-battle copy
/// (`_akropolisCafeteriaCopyPostBattleVram` moves the skin view 16 uploads at (704, 0) onto the boss's first texture page,
/// and palette row 271 onto its palette), which the room keeps its own copy of.
static RECT _gCafeteriaBossStrangerUnusedVramRects[2] = {
    { 704, 0, 64, 256 },
    { 0, 271, 256, 1 },
};

TaskDesc gCafeteriaBossStrangerTaskDesc = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_ACTOR } }, _cafeteriaBossStrangerTask, { .model = &_gAkropolisCafeteriaBossStrangerBody } };

/// Unreferenced copy of the actor's task recipe; no recovered resource selects it.
static TaskDesc _gCafeteriaBossStrangerUnusedTaskDesc = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_ACTOR } }, _cafeteriaBossStrangerTask, { .model = &_gAkropolisCafeteriaBossStrangerBody } };

/// Overlay-wide shudder step: reset on entry, 0..4 jolt the root, 5 resumes CHASE.
static s16 _gCafeteriaBossStrangerShudderStep;

SVECTOR ActorContact_ScratchPosition;

/// Returns this carrier's persistent last contact-push correction.
static inline SVECTOR* _actorContactGetLastPushStep(void)
{
    return &ActorContact_ScratchPosition;
}

/// Reusable hit-effect arguments; each spawn path overwrites all three members.
EffectSpawnArg _gCafeteriaBossStrangerHitEffectArgs;

/// Whole-unit step `_actorContactApplyGridPushback` last applied to its coordinate.
extern SVECTOR ActorContact_ScratchPosition;

static void _cafeteriaBossStrangerApplyDamage(Task* task);

static void        _cafeteriaBossStrangerLayPatrolNodes(BossStrangerWalker* walker, s16 radius, s16 nodeYawStep);
static inline void _cafeteriaBossStrangerRescaleRootYaw(Task* task, s16 uniformScale);
static void        _cafeteriaBossStrangerTickBlendedSlots(Task* task);
static inline void _cafeteriaBossStrangerLinkAttackBody(WorldCollisionBody* body, GfxCoord* partCoord, WorldCollisionContact* contacts, const SVECTOR* localPosition, u16 bodyFlags);
static inline void _cafeteriaBossStrangerInitWalkerScale(BossStrangerWalker* walker);
static void        _cafeteriaBossStrangerSpawn(Enemy* enemy, Task* task);
static void        _cafeteriaBossStrangerPatrolState(Task* task);
static inline s32  _cafeteriaBossStrangerTickFootstepShake(void);
static inline s32  _cafeteriaBossStrangerHasPlayerBodyContact(const WorldCollisionContact* contacts);
static void        _cafeteriaBossStrangerAttackState(Task* task);
static inline s32  _cafeteriaBossStrangerFindAttackContact(SVECTOR* hitPoint, const WorldCollisionContact* contacts, s16 contactCount);
static void        _cafeteriaBossStrangerIdleState(Task* task);
static void        _cafeteriaBossStrangerFallBackState(Task* task);
static inline void _cafeteriaBossStrangerShrinkBurnRootYaw(Task* task, const _Work* work, s16 heightScale);
static void        _cafeteriaBossStrangerDeathBurstState(Task* task);
static void        _cafeteriaBossStrangerDeathThrashState(Task* task);
static void        _cafeteriaBossStrangerLurkAlertState(Task* task);
static void        _cafeteriaBossStrangerShudderState(Task* task);
static void        _cafeteriaBossStrangerEnrageState(Task* task);
static void        _cafeteriaBossStrangerUpdate(Enemy* enemy, Task* task);

#include "../../../../shared/actor/actor_contacts_turn_joint.inc.c"

#include "../../../../shared/actor/actor_contacts_push_contact.inc.c"

#include "../../../../shared/creatures/boss_stranger/boss_stranger.inc.c"

/// Lays out the spawn-time patrol around the walker's current position.
static void _cafeteriaBossStrangerLayPatrolNodes(BossStrangerWalker* walker, s16 radius, s16 nodeYawStep)
{
    _PatrolLayoutScratch* scratch;

    if (walker->nav->nodeCount < 2)
        return;
    scratch                   = SCRATCH_STACK_RESERVE_BLOCK(_PatrolLayoutScratch);
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
    walker->route->nodeIndices[scratch->nodeIndex] = BOSS_STRANGER_ROUTE_END;
    SCRATCH_STACK_RELEASE_BLOCK(_PatrolLayoutScratch);
}

#include "../../../../shared/creatures/boss_stranger/boss_stranger_inlines.inc.c"

/// Rebuilds the actor root from its current yaw at a uniform signed Q12 scale.
static inline void _cafeteriaBossStrangerRescaleRootYaw(Task* task, s16 uniformScale)
{
    _actorRenderRescaleYaw(task->extra.tmd->coords, uniformScale);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Places the actor root and reapplies its walker's uniform scale.
static s32 _cafeteriaBossStrangerPlace(Task* task, s32 messageId, const ActorTransform* placement, s32 secondArg)
{
    _Work* work;

    work = task->work;

    task->extra.tmd->coords->coord.t[0] = placement->pos.vx;
    task->extra.tmd->coords->coord.t[1] = placement->pos.vy;
    task->extra.tmd->coords->coord.t[2] = placement->pos.vz;
    gfxRotMatrixX(&task->extra.tmd->coords->coord, placement->rot.vx, GRAPHICS_ROTATION_REPLACE);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, placement->rot.vy, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixZ(&task->extra.tmd->coords->coord, placement->rot.vz, GRAPHICS_ROTATION_COMPOSE);
    _cafeteriaBossStrangerRescaleRootYaw(task, work->walker.scale);
    work->placedYaw = ratan2(-task->extra.tmd->coords->coord.m[2][0],
                             task->extra.tmd->coords->coord.m[2][2]);
    return 1;
}

/// Actor commands the patio and the cafeteria send, in the cafeteria's own names (its `_gAkropolisCafeteriaStranger*Command`).
enum {
    PATIO_COMMAND_LURK                     = 1, // Holds `scripted_0` until the player comes near
    CAFETERIA_COMMAND_START                = 1, // Runs `alert` forward, then backward: `ALERT_REWIND`
    CAFETERIA_COMMAND_POST_BATTLE          = 2, // Plays the collapse in `scripted_1`
    CAFETERIA_COMMAND_FOLLOW_UP_TRIPLE_HIT = 3, // Plays the collapse in `scripted_2`, three hits on its cue, then `fall_back_end`
    CAFETERIA_COMMAND_RECOIL               = 4, // Plays the lesser Stranger's idle in `scripted_3`
    CAFETERIA_COMMAND_FOLLOW_UP            = 5, // Plays the third follow-up in `scripted_0`
    CAFETERIA_COMMAND_RANDOM_HIT           = 6, // Plays the room's seventh extra clip in `scripted_1`, one hit at random; never sent
    CAFETERIA_COMMAND_SETTLE               = 7, // As `RECOIL`
    CAFETERIA_COMMAND_FOLLOW_UP_BURN       = 8, // Burns away: `DEATH_BURN`
    CAFETERIA_COMMAND_FOLLOW_UP_CLIP       = 9, // Plays `command_9`
};

/// Applies the actor's Acropolis patio or cafeteria command.
static s32 _cafeteriaBossStrangerApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 secondArg)
{
    enum {
        CONTEXT_PATIO     = GAME_STAGE_AKROPOLIS | (GAME_AREA_AKROPOLIS_PATIO << 8),
        CONTEXT_CAFETERIA = GAME_STAGE_AKROPOLIS | (GAME_AREA_AKROPOLIS_CAFETERIA << 8),
    };

    _Work* work = task->work;

    // Cache every received command, including contexts or actions we reject.
    work->lastCommand.stage   = command->context.loc.stage;
    work->lastCommand.area    = command->context.loc.area;
    work->lastCommand.command = command->command;
    if (command->context.key == CONTEXT_PATIO) {
        if (command->command == PATIO_COMMAND_LURK) {
            work->state = STATE_LURK;
            return 1;
        }
        return 0;
    }
    if (command->context.key == CONTEXT_CAFETERIA) {
        switch (command->command) {
            default:
                return 0;
            case CAFETERIA_COMMAND_START:
                work->state     = STATE_ALERT_REWIND;
                work->prevState = PREV_STATE_NONE;
                return 1;
            case CAFETERIA_COMMAND_POST_BATTLE:
                work->animId                                                             = BOSS_STRANGER_ANIMATION_SCRIPTED_1;
                _gCafeteriaBossStrangerAnimationSets[BOSS_STRANGER_ANIMATION_SCRIPTED_1] = &gAkropolisCafeteriaBossStrangerCollapseClip;
                break;
            case CAFETERIA_COMMAND_FOLLOW_UP_TRIPLE_HIT:
                work->animId                                                             = BOSS_STRANGER_ANIMATION_SCRIPTED_2;
                _gCafeteriaBossStrangerAnimationSets[BOSS_STRANGER_ANIMATION_SCRIPTED_2] = &gAkropolisCafeteriaBossStrangerCollapseClip;
                break;
            case CAFETERIA_COMMAND_FOLLOW_UP:
                work->animId                                                             = BOSS_STRANGER_ANIMATION_SCRIPTED_0;
                _gCafeteriaBossStrangerAnimationSets[BOSS_STRANGER_ANIMATION_SCRIPTED_0] = &gAkropolisCafeteriaBossStrangerFollowUp3Clip;
                break;
            case CAFETERIA_COMMAND_RANDOM_HIT:
                work->animId                                                             = BOSS_STRANGER_ANIMATION_SCRIPTED_1;
                _gCafeteriaBossStrangerAnimationSets[BOSS_STRANGER_ANIMATION_SCRIPTED_1] = &gAkropolisCafeteriaExtraAnim7Clip;
                break;
            case CAFETERIA_COMMAND_RECOIL:
            case CAFETERIA_COMMAND_SETTLE:
                work->animId                                                             = BOSS_STRANGER_ANIMATION_SCRIPTED_3;
                _gCafeteriaBossStrangerAnimationSets[BOSS_STRANGER_ANIMATION_SCRIPTED_3] = &gStrangerLesserIdleClip;
                break;
            case CAFETERIA_COMMAND_FOLLOW_UP_BURN:
                work->state     = STATE_DEATH_BURN;
                work->prevState = PREV_STATE_NONE;
                return 1;
            case CAFETERIA_COMMAND_FOLLOW_UP_CLIP:
                work->animId = BOSS_STRANGER_ANIMATION_COMMAND_9;
                break;
        }
        work->state     = STATE_SCRIPTED;
        work->prevState = PREV_STATE_NONE;
        return 1;
    }
    return 0;
}

#include "../../../../shared/actor/player_detection_reach.inc.c"

/// Advances both rigs and blends their rotations over model parts 1..10.
static void _cafeteriaBossStrangerTickBlendedSlots(Task* task)
{
    enum { PART_END       = 11,
           MAIN_RATE_BIAS = 3 };

    AnimationPose     mainPose;
    AnimationPose     overlayPose;
    AnimationContext* mainAnim;
    s16               mainWeight;
    s16               slotIndex;
    _Work*            work;

    work       = task->work;
    mainWeight = work->blendWeight;
    mainAnim   = &work->rig.anim;
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        if (slotIndex < PART_END) {
            work->blendRig.slots[slotIndex].rate = work->blendRate;
            work->rig.slots[slotIndex].rate      = (work->animRate - MAIN_RATE_BIAS);
            animationTickSlotPose(mainAnim, slotIndex, &mainPose, 0);
            animationTickSlotPose(&work->blendRig.anim, slotIndex, &overlayPose, 0);
            animationApplyPoseWithBlendedRotation(mainAnim, slotIndex, &mainPose, &overlayPose, mainWeight, ONE - mainWeight);
        } else {
            work->rig.slots[slotIndex].rate = (work->animRate - MAIN_RATE_BIAS);
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
}

/// Returns the sound script due at the active animation's pose cue, or zero; the sounds are the Stranger bank's
/// (`enemies/stranger/sounds`), named as its samples are.
static s32 _cafeteriaBossStrangerPollAnimationSound(_Work* work)
{
    enum {
        CHASE_RIGHT_FOOT_CUE         = 0xC5,
        CHASE_LEFT_FOOT_CUE          = 0xFD,
        CHASE_LEFT_FOOT_SUPPRESS_CUE = 0xFC,
        ALERT_CUE                    = 4,
        ENRAGED_ATTACK_CUE           = 9,
        ATTACK_CUE                   = 11,
        SOUND_RUN_STEP_B             = SOUND_CHARACTER(SOUND_BANK_STRANGER, STRANGER_SOUND_RUN_STEP_B),
        SOUND_RUN_STEP_A             = SOUND_CHARACTER(SOUND_BANK_STRANGER, STRANGER_SOUND_RUN_STEP_A),
        SOUND_WALK_STEP_B            = SOUND_CHARACTER(SOUND_BANK_STRANGER, STRANGER_SOUND_WALK_STEP_B),
        SOUND_WALK_STEP_A            = SOUND_CHARACTER(SOUND_BANK_STRANGER, STRANGER_SOUND_WALK_STEP_A),
        SOUND_ALERT                  = SOUND_CHARACTER(SOUND_BANK_STRANGER, STRANGER_SOUND_ALERT),
        SOUND_STRIKE                 = SOUND_CHARACTER(SOUND_BANK_STRANGER, STRANGER_SOUND_STRIKE),
    };

    s32 rightFootCueIndex;
    s32 leftFootCueIndex;
    s32 patrolCueIndex;
    s32 alertCueIndex;
    s32 enragedAttackCueIndex;
    s32 attackCueIndex;
    s32 previousCueIndex;
    s16 clipSelector;

    clipSelector = (u16)work->animId - BOSS_STRANGER_ANIMATION_WALK;
    switch (clipSelector) {
        case BOSS_STRANGER_ANIMATION_RUN - BOSS_STRANGER_ANIMATION_WALK:
            rightFootCueIndex = work->rig.slots[RIGHT_FOOT_PART].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (rightFootCueIndex == CHASE_RIGHT_FOOT_CUE) {
                previousCueIndex = work->lastCueIndex;
                if (previousCueIndex != rightFootCueIndex) {
                    work->lastCueIndex = rightFootCueIndex;
                    return SOUND_RUN_STEP_B;
                }
                work->lastCueIndex = previousCueIndex;
                return SOUND_SCRIPT_REQUEST_NO_OP;
            }
            leftFootCueIndex = work->rig.slots[LEFT_FOOT_PART].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            // This cue suppresses against 0xFC, while the emitted cue stores 0xFD.
            if (leftFootCueIndex == CHASE_LEFT_FOOT_CUE) {
                if (work->lastCueIndex != CHASE_LEFT_FOOT_SUPPRESS_CUE) {
                    work->lastCueIndex = leftFootCueIndex;
                    return SOUND_RUN_STEP_A;
                }
                work->lastCueIndex = leftFootCueIndex;
                break;
            }
            work->lastCueIndex = 0;
            break;
        case BOSS_STRANGER_ANIMATION_WALK - BOSS_STRANGER_ANIMATION_WALK:
            patrolCueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (patrolCueIndex == WALK_STEP_B_CUE) {
                if (work->lastCueIndex != patrolCueIndex) {
                    work->lastCueIndex = patrolCueIndex;
                    return SOUND_WALK_STEP_B;
                }
                work->lastCueIndex = patrolCueIndex;
            } else if (patrolCueIndex == WALK_STEP_A_CUE) {
                previousCueIndex = work->lastCueIndex;
                if (previousCueIndex != patrolCueIndex) {
                    work->lastCueIndex = patrolCueIndex;
                    return SOUND_WALK_STEP_A;
                }
                work->lastCueIndex = previousCueIndex;
            } else {
                work->lastCueIndex = 0;
            }
            break;
        case BOSS_STRANGER_ANIMATION_ALERT - BOSS_STRANGER_ANIMATION_WALK:
            alertCueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (alertCueIndex == ALERT_CUE && work->lastCueIndex != alertCueIndex) {
                work->lastCueIndex = alertCueIndex;
                return SOUND_ALERT;
            }
            work->lastCueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            break;
        case BOSS_STRANGER_ANIMATION_ENRAGED_ATTACK - BOSS_STRANGER_ANIMATION_WALK:
            enragedAttackCueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (enragedAttackCueIndex == ENRAGED_ATTACK_CUE && work->lastCueIndex != enragedAttackCueIndex) {
                work->lastCueIndex = enragedAttackCueIndex;
                return SOUND_STRIKE;
            }
            work->lastCueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            break;
        case BOSS_STRANGER_ANIMATION_ATTACK - BOSS_STRANGER_ANIMATION_WALK:
            attackCueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (attackCueIndex == ATTACK_CUE && work->lastCueIndex != attackCueIndex) {
                work->lastCueIndex = attackCueIndex;
                return SOUND_STRIKE;
            }
            work->lastCueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            break;
    }
    return SOUND_SCRIPT_REQUEST_NO_OP;
}

/// Advances main slots 1..18 at the actor's current sixteenth-frame rate.
static inline void _cafeteriaBossStrangerTickMainSlots(Task* task)
{
    _Work* work;
    s32    slotIndex;

    work      = task->work;
    slotIndex = 1;
    do {
        work->rig.slots[slotIndex].rate = work->animRate;
        animationTickSlot(&work->rig.anim, slotIndex);
        slotIndex += 1;
    } while (slotIndex < ARRAY_SIZE(work->rig.slots));
}

/// Seeks main slots 1..18 to the requested clip using signed frame durations.
static inline void _cafeteriaBossStrangerSeekRequestedAnimation(_Work* work)
{
    _Work* seekWork;
    s32    requestedAnimId;
    s32    seekSlotIndex;

    seekWork      = work;
    seekSlotIndex = 1;
    do {
        work->rig.slots[seekSlotIndex].rate = seekWork->animRate;
        requestedAnimId                     = seekWork->animId;
        animationSeekSlotWithBlend(&seekWork->rig.anim, seekSlotIndex, requestedAnimId, 0, _gCafeteriaBossStrangerAnimBlendFrames[seekWork->appliedAnim][requestedAnimId]);
        seekSlotIndex += 1;
    } while (seekSlotIndex < ARRAY_SIZE(work->rig.slots));
    seekWork->appliedAnim = seekWork->animId;
}

/// Restarts main slots 1..18 on the requested loaded clip and records its ID.
static inline void _cafeteriaBossStrangerResetRequestedAnimation(_Work* work)
{
    _Work* resetWork;
    s32    resetSlotIndex;

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
static inline void _cafeteriaBossStrangerResetBlendAnimation(Task* task)
{
    enum {
        RATE        = 3 * ANIMATION_RATE_ONE,
        MAIN_WEIGHT = 0xB78,
    };
    _Work* blendWork;
    s32    blendSlotIndex;

    blendWork              = task->work;
    blendSlotIndex         = 1;
    blendWork->blendRate   = RATE;
    blendWork->blendWeight = MAIN_WEIGHT;
    do {
        // The rate goes to the main rig's slot, which the tick below
        // overwrites; the blend slots take theirs in the blend pass.
        blendWork->rig.slots[blendSlotIndex].rate = blendWork->blendRate;
        animationResetSlot(&blendWork->blendRig.anim, blendSlotIndex, blendWork->blendAnimId);
        blendSlotIndex += 1;
    } while (blendSlotIndex < ARRAY_SIZE(blendWork->blendRig.slots));
}

/// Restarts a requested pose and runs it for 99 hidden ticks.
static inline void _cafeteriaBossStrangerSettleRequestedAnimation(Task* task, _Work* work)
{
    enum { SETTLE_TICK_END = 100 };
    _Work* settleWork;
    s32    slotOrTickIndex;

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
        _cafeteriaBossStrangerTickMainSlots(task);
        slotOrTickIndex += 1;
    } while (slotOrTickIndex < SETTLE_TICK_END);
}

/// Applies animation requests, advances the actor's poses and sounds their cues.
static void _cafeteriaBossStrangerTickAnimation(Task* task)
{
    enum {
        STEP  = 0x100,
        LIMIT = 0x400,
    };

    _Work* work;
    Enemy* enemy;
    s32    targetYaw;
    s32    currentYaw;
    s32    targetYawBits;
    s32    currentYawBits;
    s32    jointYawBits;
    s16    jointYaw;
    s32    cueSoundId;
    s32    instanceSoundId;
    s32    audioPan;
    s16    request;

    work    = task->work;
    request = work->animRequest;
    enemy   = task->spawnArg2.pointer;
    if (request == ANIM_REQUEST_BLEND) {
        _cafeteriaBossStrangerSeekRequestedAnimation(work);
        work->animRequest  = ANIM_REQUEST_PLAYING;
        work->animFrames   = 0;
        work->lastCueIndex = 0;
    } else if (request == ANIM_REQUEST_RESET) {
        _cafeteriaBossStrangerResetRequestedAnimation(work);
        work->animRequest  = ANIM_REQUEST_PLAYING;
        work->animFrames   = 0;
        work->lastCueIndex = 0;
    } else if (request == ANIM_REQUEST_SETTLE) {
        _cafeteriaBossStrangerSettleRequestedAnimation(task, work);
        work->animRequest  = ANIM_REQUEST_PLAYING;
        work->animFrames   = 0;
        work->lastCueIndex = 0;
    }
    if (work->blendRequest == ANIM_REQUEST_RESET) {
        _cafeteriaBossStrangerResetBlendAnimation(task);
        work->blendRequest = ANIM_REQUEST_PLAYING;
    }
    work->animFrames = (u16)(work->animFrames + 1);
    if (work->blendActive == 0) {
        _cafeteriaBossStrangerTickMainSlots(task);
    } else {
        _cafeteriaBossStrangerTickBlendedSlots(task);
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
        if ((targetYaw - currentYaw) >= STEP + 1) {
            work->lookYaw = (s16)(currentYawBits + STEP);
        } else {
            work->lookYaw = (s16)targetYawBits;
        }
    } else if ((currentYaw - targetYaw) >= STEP + 1) {
        work->lookYaw = (s16)(currentYawBits - STEP);
    } else {
        work->lookYaw = (s16)targetYawBits;
    }
    jointYaw     = work->lookYaw;
    jointYawBits = (u16)work->lookYaw;
    if (jointYaw != 0) {
        if (jointYaw >= LIMIT + 1) {
            jointYawBits = LIMIT;
        }
        if (jointYaw < -LIMIT) {
            jointYawBits = -LIMIT;
        }
        _actorRenderYawJointInWorld(&task->extra.tmd->coords[HEAD_PART], (s16)jointYawBits);
        _actorRenderYawJointInWorld(&task->extra.tmd->coords[LOWER_NECK_PART], (s16)jointYawBits >> 2);
    }
    // Attribute the cue to this placed enemy before applying positional audio.
    cueSoundId = _cafeteriaBossStrangerPollAnimationSound(work);
    if (cueSoundId != SOUND_SCRIPT_REQUEST_NO_OP) {
        instanceSoundId = cueSoundId | ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        audioPan        = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(instanceSoundId, audioPan, (s32)(s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
}

/// Initializes and links the actor's attack sphere on a model part.
static inline void _cafeteriaBossStrangerLinkAttackBody(WorldCollisionBody* body, GfxCoord* partCoord, WorldCollisionContact* contacts, const SVECTOR* localPosition, u16 bodyFlags)
{
    enum { ATTACK_RADIUS = 512 };

    body->context.contacts = contacts;
    body->coord            = partCoord;
    body->pos.vx           = localPosition->vx;
    body->pos.vy           = localPosition->vy;
    body->pos.vz           = localPosition->vz;
    body->radius           = ATTACK_RADIUS;
    body->flags            = bodyFlags;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, body);
}

/// Initializes the walker's saved rotation to its uniform Q12 scale.
static inline void _cafeteriaBossStrangerInitWalkerScale(BossStrangerWalker* walker)
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
static inline void _cafeteriaBossStrangerBindPatrolStorage(_Work* work)
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
static void _cafeteriaBossStrangerSpawn(Enemy* enemy, Task* task)
{
    enum {
        SPAWN_BEHAVIOR_SHIFT     = 16,
        SPAWN_BEHAVIOR_MASK      = 0xF,
        SPAWN_PATROL             = 0,
        SPAWN_HIDDEN_OR_RESTORED = 1,
        SPAWN_HIDDEN_NO_BUFFER   = 2,
        SPAWN_KEEP_HIDDEN        = 3,
        SPAWN_IDLE               = 4,
        MOVEMENT_TUNING_MASK     = 0xF0,
        MOVEMENT_TUNING_STANDARD = 0,
        MOVEMENT_TUNING_LARGE    = 0x10,
        MOVEMENT_TUNING_LARGEST  = 0x20,
        STANDARD_ANIMATION_RATE  = 20,
        MODEL_SCALE_LARGE        = 4500, // Q12 uniform scale
        MODEL_SCALE_LARGEST      = 6500, // Q12 uniform scale
        NOTICE_TUNING_MASK       = 0xF00,
        NOTICE_TUNING_SHORT      = 0,
        NOTICE_TUNING_MEDIUM     = 0x100,
        NOTICE_TUNING_LONG       = 0x200,
        PATROL_TUNING_MASK       = 0xF000,
        PATROL_TUNING_SHORT      = 0,
        PATROL_TUNING_MEDIUM     = 0x1000,
        PATROL_TUNING_LONG       = 0x2000,
        PATROL_NODE_YAW_STEP     = 0x764,
        GRID_BODY_ID             = 13,
        GRID_BODY_RADIUS         = 420,
        HIT_BODY_RADIUS          = 330,
        HIT_BODY_HEIGHT          = 700,
        HIT_BODY_FORWARD         = 400,
        INITIAL_TURN_LIMIT       = 0x20,
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
    _Work*                 work;
    _Work*                 lightingWork;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    if (((task->spawnArg1.value >> SPAWN_BEHAVIOR_SHIFT) & SPAWN_BEHAVIOR_MASK) != SPAWN_HIDDEN_NO_BUFFER) {
        model->flags = 0;
        tmdAllocPrimitiveBuffer(model);
    }
    work       = memCalloc(sizeof(_Work), false);
    task->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    // Publish the work-backed model and enemy bindings before starting playback.
    task->exitCallback      = _cafeteriaBossStrangerExit;
    lightingWork            = task->work;
    lightingModel           = task->extra.tmd;
    lightingModel->lightMtx = &lightingWork->lightMtx;
    lightingModel->colorMtx = &lightingWork->colorMtx;
    enemy->field_4          = &rootCoord->coord;
    enemy->field_48         = 0;
    enemy->bodyPos.vx       = 0;
    enemy->bodyPos.vy       = 0;
    enemy->bodyPos.vz       = 0;
    enemy->coord            = task->extra.tmd->coords + LOWER_NECK_PART;
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->reactionFlags          = 0;
    enemy->param                  = &_gCafeteriaBossStrangerEnemyParams;
    enemy->hpMax = enemy->hp = _gCafeteriaBossStrangerEnemyParams.hpMax;
    hitContacts              = work->hitContacts;
    enemy->recs              = hitContacts;
    animationInitContext(&work->rig.anim, _gCafeteriaBossStrangerAnimationSets, model, work->rig.poses, work->rig.slots);
    animationInitContext(&work->blendRig.anim, _gCafeteriaBossStrangerAnimationSets, model, work->blendRig.poses, work->blendRig.slots);
    work->animRequest   = ANIM_REQUEST_RESET;
    work->blendActive   = 0;
    work->animId        = BOSS_STRANGER_ANIMATION_WALK;
    work->lookYaw       = 0;
    work->lookYawTarget = 0;
    _cafeteriaBossStrangerTickAnimation(task);
    // Link the world-space grid sphere and the model-relative hit/attack spheres.
    gridContacts                    = work->gridContacts;
    work->enrageTint                = 0;
    work->enraged                   = 0;
    work->gridBody.coord            = &gGfxViewCoord;
    walkerContacts                  = gridContacts;
    work->gridBody.context.contacts = gridContacts;
    work->gridBody.pos.vx           = (u16)task->extra.tmd->coords->coord.t[0];
    work->gridBody.pos.vy           = (s16)((u16)task->extra.tmd->coords->coord.t[1] - GRID_BODY_HEIGHT);
    work->gridBody.pos.vz           = (u16)task->extra.tmd->coords->coord.t[2];
    work->gridBody.key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | GRID_BODY_ID;
    work->gridBody.radius           = GRID_BODY_RADIUS;
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
    hitBody->radius           = HIT_BODY_RADIUS;
    hitBody->flags            = sphereKind;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, hitBody);
    hitBody->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(hitBody->context.contacts, ARRAY_SIZE(work->hitContacts), 0);
    work->hitBody.pos.vy   = -HIT_BODY_HEIGHT;
    attackBody             = &work->attackBody;
    work->hitBody.key      = WORLD_COLLISION_CONTACT_ENEMY_BODY;
    work->hitBody.pos.vx   = 0;
    work->hitBody.pos.vz   = HIT_BODY_FORWARD;
    attackLocalPosition.vx = 0;
    attackLocalPosition.vy = 0;
    attackLocalPosition.vz = 0;
    _cafeteriaBossStrangerLinkAttackBody(attackBody, task->extra.tmd->coords + LOWER_NECK_PART, work->attackContacts, &attackLocalPosition, sphereKind);
    worldCollisionInitContacts(attackBody->context.contacts, ARRAY_SIZE(work->attackContacts), 0);
    task->msgTable          = _gCafeteriaBossStrangerMessages;
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
    work->walker.turnLimit  = INITIAL_TURN_LIMIT;
    if ((GAME_LOCATION_WORD(gGameSession->location) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_PATIO, 0, 0)) {
        work->walker.lockHeight = 0;
    } else {
        work->walker.lockHeight = true;
    }
    work->walker.skipGround = 0;
    work->walker.skipAvoid  = 1;
    work->walker.playerId   = (u8)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId;
    // Bind the two-node patrol to storage owned by this work block.
    _cafeteriaBossStrangerBindPatrolStorage(work);
    work->walker.playerId = (u8)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId;
    switch (task->spawnArg1.value & MOVEMENT_TUNING_MASK) {
        case MOVEMENT_TUNING_STANDARD:
            work->animRate         = STANDARD_ANIMATION_RATE;
            work->baseRate         = STANDARD_ANIMATION_RATE;
            work->walker.scale     = ONE;
            work->walkSpeed        = 10;
            work->flinchSpeedScale = 75;
            break;
        case MOVEMENT_TUNING_LARGE:
            work->animRate         = ANIMATION_RATE_ONE;
            work->baseRate         = ANIMATION_RATE_ONE;
            work->walker.scale     = MODEL_SCALE_LARGE;
            work->walkSpeed        = 8;
            work->flinchSpeedScale = 66;
            break;
        case MOVEMENT_TUNING_LARGEST:
            work->animRate         = ANIMATION_RATE_ONE;
            work->baseRate         = ANIMATION_RATE_ONE;
            work->walker.scale     = MODEL_SCALE_LARGEST;
            work->walkSpeed        = 14;
            work->flinchSpeedScale = 63;
            break;
        default:
            work->animRate     = STANDARD_ANIMATION_RATE;
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
    switch (task->spawnArg1.value & NOTICE_TUNING_MASK) {
        case NOTICE_TUNING_SHORT:
            work->noticeRangeAhead  = 3000;
            work->noticeRangeAround = 500;
            break;
        case NOTICE_TUNING_MEDIUM:
            work->noticeRangeAhead  = 4000;
            work->noticeRangeAround = 1000;
            break;
        case NOTICE_TUNING_LONG:
            work->noticeRangeAhead  = 6000;
            work->noticeRangeAround = 2000;
            break;
        default:
            work->noticeRangeAhead  = 8000;
            work->noticeRangeAround = 2000;
            break;
    }
    switch (task->spawnArg1.value & PATROL_TUNING_MASK) {
        case PATROL_TUNING_SHORT:
            _cafeteriaBossStrangerLayPatrolNodes(&work->walker, 1500, PATROL_NODE_YAW_STEP);
            break;
        case PATROL_TUNING_MEDIUM:
            _cafeteriaBossStrangerLayPatrolNodes(&work->walker, 3000, PATROL_NODE_YAW_STEP);
            break;
        case PATROL_TUNING_LONG:
            _cafeteriaBossStrangerLayPatrolNodes(&work->walker, 4000, PATROL_NODE_YAW_STEP);
            break;
        default:
            _cafeteriaBossStrangerLayPatrolNodes(&work->walker, 5000, PATROL_NODE_YAW_STEP);
            break;
    }
    switch ((task->spawnArg1.value >> SPAWN_BEHAVIOR_SHIFT) & SPAWN_BEHAVIOR_MASK) {
        case SPAWN_HIDDEN_OR_RESTORED:
            if (enemy->spawnState == 0) {
                work->state = STATE_HIDDEN;
            } else {
                work->state = STATE_RESTORED;
            }
            break;
        case SPAWN_HIDDEN_NO_BUFFER:
            work->state = STATE_HIDDEN;
            break;
        case SPAWN_KEEP_HIDDEN:
            break;
        case SPAWN_IDLE:
            work->state = STATE_IDLE;
            break;
        case SPAWN_PATROL:
        default:
            if (enemy->spawnState == 0) {
                work->state = STATE_PATROL;
            } else {
                work->state = STATE_RESTORED;
            }
            break;
    }
    _cafeteriaBossStrangerInitWalkerScale(&work->walker);
    work->prevState = PREV_STATE_NONE;
    task->state    += 1;
}

/// Walks the patrol route until the player enters a notice range.
static void _cafeteriaBossStrangerPatrolState(Task* task)
{
    enum { PATROL_TURN_LIMIT = 16,
           NOTICE_ANGLE      = 1000 };

    _Work*     work;
    TmdObject* model;
    Enemy*     enemy;
    GfxCoord*  rootCoord;
    SVECTOR*   offset;
    SVECTOR    playerOffset;
    s16        playerTurn;

    work  = task->work;
    model = task->extra.tmd;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model->flags                  = 0;
        work->animRate                = work->baseRate;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->animRequest             = ANIM_REQUEST_RESET;
        work->animId                  = BOSS_STRANGER_ANIMATION_WALK;
        work->walker.state            = BOSS_STRANGER_WALKER_PATROL;
        work->walker.turnLimit        = PATROL_TURN_LIMIT;
    }
    work->walker.speed = work->walkSpeed;
    _bossStrangerTick(&work->walker);
    // Test both the forward notice cone and the shorter all-around range.
    rootCoord = task->extra.tmd->coords;
    offset    = &playerOffset;
    _actorPositionDeltaToPlayer(&gPlayerStatus, rootCoord, offset);
    playerTurn = _actorAngleTurnToOffset(task->extra.tmd->coords, playerOffset.vx, offset->vz);
    if (abs(playerTurn) < NOTICE_ANGLE) {
        if (actorOutsideRadius(&playerOffset, work->noticeRangeAhead) == 0)
            work->state = STATE_ALERT;
    }
    if (actorOutsideRadius(&playerOffset, work->noticeRangeAround) == 0)
        work->state = STATE_ALERT;
    _cafeteriaBossStrangerTickAnimation(task);
}

/// Advances the shared five-tick footstep shake and reports its end.
static inline s32 _cafeteriaBossStrangerTickFootstepShake(void)
{
    enum { FOOTSTEP_SHAKE_TICKS = 5 };

    _gCafeteriaBossStrangerFootstepShakePhase++;
    if (_gCafeteriaBossStrangerFootstepShakePhase == FOOTSTEP_SHAKE_TICKS)
        _gCafeteriaBossStrangerFootstepShakePhase = 0;
    if (!(_gCafeteriaBossStrangerFootstepShakePhase & 1))
        displaySetShakeY(0);
    else
        displaySetShakeY(1);
    if (_gCafeteriaBossStrangerFootstepShakePhase == 0) {
        displaySetShakeY(0);
        return 1;
    }
    return 0;
}

/// Chases the player and selects ATTACK when close enough.
static void _cafeteriaBossStrangerChaseState(Task* task)
{
    enum {
        ENRAGED_TURN_LIMIT   = 48,
        CHASE_TURN_LIMIT     = 28,
        STOP_DISTANCE        = 420,
        BLEND_SPEED_DIVISOR  = 1520,
        CLOSE_STOP_RADIUS    = 900,
        ATTACK_INNER_RADIUS  = 500,
        ATTACK_OUTER_RADIUS  = 1000,
        ATTACK_FACING_LIMIT  = 128,
        ALIGNED_ATTACK_TICKS = 25,
        ATTACK_TIMEOUT_TICKS = 91,
        STEP_B_SHAKE         = 1,
        STEP_A_SHAKE         = 2,
    };

    _Work*              work;
    TmdObject*          model;
    Enemy*              enemy;
    BossStrangerWalker* walker;
    GfxCoord*           rootCoord;
    SVECTOR*            offset;
    SVECTOR             playerOffset;
    s16                 playerTurn;
    u16                 forwardSpeed;
    s32                 stepBCueIndex;
    s32                 stepACueIndex;

    work  = task->work;
    model = task->extra.tmd;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model->flags                  = 0;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->animRequest             = ANIM_REQUEST_BLEND;
        work->animId                  = BOSS_STRANGER_ANIMATION_WALK;
        work->walker.state            = BOSS_STRANGER_WALKER_CHASE;
        work->animRate                = work->baseRate;
        if (work->baseRate == ENRAGED_BASE_RATE)
            work->walker.turnLimit = ENRAGED_TURN_LIMIT;
        // The ordinary limit also overwrites the enraged entry limit.
        work->walker.turnLimit = CHASE_TURN_LIMIT;
        work->stateFrame       = 0;
    }
    // Scale movement by playback rate, then stop inside the player clearance.
    if (work->blendActive == 0) {
        work->walker.speed = work->walkSpeed * work->animRate / ANIMATION_RATE_ONE;
        if ((s16)_playerDetectionOutOfReach(task->extra.tmd->coords, STOP_DISTANCE, (s16)work->walker.speed) == 0)
            work->walker.speed = 0;
    } else {
        work->walker.speed = (u16)(work->flinchSpeedScale * work->animRate / BLEND_SPEED_DIVISOR) / 2;
        if ((s16)_playerDetectionOutOfReach(task->extra.tmd->coords, STOP_DISTANCE, (s16)work->walker.speed) == 0)
            work->walker.speed = 0;
    }
    rootCoord = task->extra.tmd->coords;
    offset    = &playerOffset;
    _actorPositionDeltaToPlayer(&gPlayerStatus, rootCoord, offset);
    if (actorOutsideRadius(&playerOffset, CLOSE_STOP_RADIUS) == 0)
        work->walker.speed = 0;
    walker              = &work->walker;
    forwardSpeed        = work->walker.speed;
    walker->speedStep   = 0;
    walker->speedTarget = forwardSpeed;
    walker->speed       = forwardSpeed;
    _bossStrangerTick(walker);
    work->stateFrame++;
    playerTurn = _actorAngleTurnToOffset(task->extra.tmd->coords, playerOffset.vx, offset->vz);
    if (abs(playerTurn) < ATTACK_FACING_LIMIT) {
        if (actorOutsideRadius(&playerOffset, ATTACK_INNER_RADIUS) != 0) {
            if (actorOutsideRadius(&playerOffset, ATTACK_OUTER_RADIUS) == 0 && work->stateFrame >= ALIGNED_ATTACK_TICKS)
                work->state = STATE_ATTACK;
        }
    }
    if (actorOutsideRadius(&playerOffset, ATTACK_OUTER_RADIUS) == 0 && work->stateFrame >= ATTACK_TIMEOUT_TICKS)
        work->state = STATE_ATTACK;
    work->lookYawTarget = playerTurn;
    _cafeteriaBossStrangerTickAnimation(task);
    // Start each footstep shake once when its slot-1 cue changes.
    stepBCueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    if ((stepBCueIndex == WALK_STEP_B_CUE) && (work->lastFootstepCueIndex != stepBCueIndex)) {
        work->footstepShake = STEP_B_SHAKE;
    }
    stepACueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    if ((stepACueIndex == WALK_STEP_A_CUE) && (work->lastFootstepCueIndex != stepACueIndex)) {
        work->footstepShake = STEP_A_SHAKE;
    }
    work->lastFootstepCueIndex = (s32)(work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK);
    if (work->footstepShake != 0) {
        if (_cafeteriaBossStrangerTickFootstepShake() != 0)
            work->footstepShake = 0;
    }
}

/// Plays the alert clip while looking toward the player, then starts chasing.
static void _cafeteriaBossStrangerAlertState(Task* task)
{
    _Work*     work;
    TmdObject* model;
    Enemy*     enemy;
    GfxCoord*  rootCoord;
    SVECTOR    playerOffset;
    SVECTOR*   offset;
    s16        playerTurn;

    work  = task->work;
    model = task->extra.tmd;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model->flags                  = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->animRequest             = ANIM_REQUEST_BLEND;
        work->animId                  = BOSS_STRANGER_ANIMATION_ALERT;
        work->animRate                = work->baseRate;
    }
    rootCoord           = task->extra.tmd->coords;
    offset              = &playerOffset;
    playerOffset.vx     = (u16)gPlayerStatus.coordMtx->t[0] - (u16)rootCoord->coord.t[0];
    offset->vy          = (u16)gPlayerStatus.coordMtx->t[1] - (u16)rootCoord->coord.t[1];
    offset->vz          = (u16)gPlayerStatus.coordMtx->t[2] - (u16)rootCoord->coord.t[2];
    playerTurn          = _actorAngleTurnToOffset(task->extra.tmd->coords, playerOffset.vx, offset->vz);
    work->lookYawTarget = playerTurn;
    _cafeteriaBossStrangerTickAnimation(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = STATE_CHASE;
    }
}

/// Reports whether the attack sphere's one contact identifies a player body.
static inline s32 _cafeteriaBossStrangerHasPlayerBodyContact(const WorldCollisionContact* contacts)
{
    enum { ATTACK_CONTACT_COUNT = 1 };
    s16 contactIndex;

    for (contactIndex = 0; contactIndex < ATTACK_CONTACT_COUNT; contactIndex++) {
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
static void _cafeteriaBossStrangerAttackState(Task* task)
{
    enum {
        NORMAL              = 1,
        ENRAGED             = 0,
        ENRAGED_ATTACK_RATE = 26,
        BRAKE_STEP          = 8,
        TURN_LIMIT          = 0x10,
        TURN_STOP_TICK      = 11,
        ENRAGED_START_CUE   = 15,
        ENRAGED_END_CUE     = 21,
        START_CUE           = 16,
        END_CUE             = 19,
    };

    _Work*              work;
    _Work*              attackWork;
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
        work->animRequest             = ANIM_REQUEST_BLEND;
        attackWork                    = task->work;
        if (attackWork->enraged != 0) {
            work->attackBody.key = damagePackEnemyAttackKey(enemy, ENRAGED);
            work->animId         = BOSS_STRANGER_ANIMATION_ENRAGED_ATTACK;
            work->animRate       = ENRAGED_ATTACK_RATE;
        } else {
            work->attackBody.key = damagePackEnemyAttackKey(enemy, NORMAL);
            work->animId         = BOSS_STRANGER_ANIMATION_ATTACK;
            work->animRate       = ANIMATION_RATE_ONE;
        }
        work->walker.state     = BOSS_STRANGER_WALKER_CHASE;
        walker                 = &work->walker;
        entrySpeed             = work->walker.speed;
        walker->speedTarget    = 0;
        walker->speedStep      = BRAKE_STEP;
        walker->speed          = entrySpeed;
        work->walker.turnLimit = TURN_LIMIT;
        work->lookYaw          = 0;
        work->lookYawTarget    = 0;
        work->stateFrame       = 0;
    }
    work->stateFrame++;
    if (work->stateFrame >= TURN_STOP_TICK) {
        work->walker.turnLimit = 0;
    }
    _cafeteriaBossStrangerTickAnimation(task);
    // Arm pair testing only inside the selected swing's cue window.
    if (work->animId == BOSS_STRANGER_ANIMATION_ENRAGED_ATTACK) {
        switch (work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) {
            case ENRAGED_START_CUE:
                work->attackBody.flags = (u16)(work->attackBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
                break;
            case ENRAGED_END_CUE:
                work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
                break;
        }
    }
    if (work->animId == BOSS_STRANGER_ANIMATION_ATTACK) {
        switch (work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) {
            case START_CUE:
                work->attackBody.flags = (u16)(work->attackBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
                break;
            case END_CUE:
                work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
                break;
        }
    }
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = STATE_CHASE;
    }
    if (_cafeteriaBossStrangerHasPlayerBodyContact(work->attackContacts)) {
        sndEvtRequestScriptStart(SOUND_STRANGER_ATTACK_HIT, (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords),
                                 (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
    }
}

/// Spawns the attack's hit effect at a model position selected by hit yaw.
static void _cafeteriaBossStrangerSpawnHitEffect(Task* task, s16 hitYaw, s32 attackKey)
{
    enum {
        FRONT_YAW_LIMIT      = 0x200,
        REAR_YAW_LIMIT       = 0x600,
        EFFECT_ARGUMENT_LOW  = 0x100,
        EFFECT_ARGUMENT_HIGH = 3,
    };

    ActorHitAnchor* hitAnchor;
    s32             absoluteHitYaw;

    hitAnchor = SCRATCH_STACK_RESERVE_BLOCK(ActorHitAnchor);
    // Use yaw sectors to choose the effect position; the rear mask keeps only bit 1.
    absoluteHitYaw = (hitYaw >= 0) ? hitYaw : -hitYaw;
    if (absoluteHitYaw < FRONT_YAW_LIMIT) {
        RANDOM_LCG_ADVANCE();
        switch ((s32)RANDOM_LCG_DRAW() & 3) {
            case 0:
                *hitAnchor = _gCafeteriaBossStrangerHitAnchors[0];
                break;
            case 1:
                *hitAnchor = _gCafeteriaBossStrangerHitAnchors[1];
                break;
            case 2:
                *hitAnchor = _gCafeteriaBossStrangerHitAnchors[2];
                break;
            case 3:
                *hitAnchor = _gCafeteriaBossStrangerHitAnchors[3];
                break;
            default:
                *hitAnchor = _gCafeteriaBossStrangerHitAnchors[4];
                break;
        }
    } else if (absoluteHitYaw > REAR_YAW_LIMIT) {
        RANDOM_LCG_ADVANCE();
        switch ((s32)RANDOM_LCG_DRAW() & 2) {
            case 0:
                *hitAnchor = _gCafeteriaBossStrangerHitAnchors[5];
                break;
            case 1:
                *hitAnchor = _gCafeteriaBossStrangerHitAnchors[6];
                break;
            default:
                *hitAnchor = _gCafeteriaBossStrangerHitAnchors[7];
                break;
        }
    } else if (hitYaw > 0) {
        RANDOM_LCG_ADVANCE();
        if (RANDOM_LCG_DRAW() & 1) {
            *hitAnchor = _gCafeteriaBossStrangerHitAnchors[8];
        } else {
            *hitAnchor = _gCafeteriaBossStrangerHitAnchors[9];
        }
    } else {
        RANDOM_LCG_ADVANCE();
        if (RANDOM_LCG_DRAW() & 1) {
            *hitAnchor = _gCafeteriaBossStrangerHitAnchors[10];
        } else {
            *hitAnchor = _gCafeteriaBossStrangerHitAnchors[11];
        }
    }
    _gCafeteriaBossStrangerHitEffectArgs.coord      = &task->extra.tmd->coords[PELVIS_PART];
    _gCafeteriaBossStrangerHitEffectArgs.spawnArgLo = EFFECT_ARGUMENT_LOW;
    _gCafeteriaBossStrangerHitEffectArgs.spawnArgHi = EFFECT_ARGUMENT_HIGH;
    effectSpawnHit(damageGetPlayerAttackEffectId(attackKey), &task->extra.tmd->coords[hitAnchor->anchor.partIndex], &hitAnchor->position, &_gCafeteriaBossStrangerHitEffectArgs);
    SCRATCH_STACK_RELEASE_BLOCK(ActorHitAnchor);
}

/// Returns the first attack contact key and copies its world-space XYZ.
static inline s32 _cafeteriaBossStrangerFindAttackContact(SVECTOR* hitPoint, const WorldCollisionContact* contacts, s16 contactCount)
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
static inline void _cafeteriaBossStrangerAccountAttackDamage(Enemy* enemy, const _HitScratch* hit)
{
    damageAccumulateLifeDrainHp(enemy, hit->hitKey, hit->damage, 0);
    enemy->hp = (u16)enemy->hp - (u16)hit->damage;
    worldTargetAddReadoutAmount(&enemy->node, hit->damage, 0);
}

/// Applies player attack contacts and ongoing damage to the Stranger enemy.
static void _cafeteriaBossStrangerApplyDamage(Task* task)
{
    // These attribute codes select local behavior; their shared meanings are unproven.
    enum {
        REACTION_RESET_BLEND_4         = 4,
        REACTION_RESET_BLEND_5         = 5,
        REACTION_IGNORE_8              = 8,
        REACTION_IGNORE_9              = 9,
        REACTION_ATTRIBUTE_MASK        = 0xFFFF,
        RETAINED_YAW_DAMAGE_MULTIPLIER = 2,
        CRITICAL_DAMAGE_MULTIPLIER     = 5,
        RETAINED_YAW_BONUS_THRESHOLD   = 1281,
    };

    _Work*        work;
    Enemy*        enemy;
    GfxCoord*     facing;
    s16           relativeHitYaw;
    s16           hitOffsetZ;
    s16           priorState;
    s32           retainedYawMagnitude;
    s32           hitBearing;
    s32           playerOffsetX;
    s32           playerOffsetY;
    s32           playerOffsetZ;
    s32           playerDistance;
    s32           audioPan;
    u32           reaction;
    _HitScratch*  scratch;
    PlayerStatus* player;

    enemy  = task->spawnArg2.pointer;
    work   = task->work;
    player = &gPlayerStatus;
    if (player->hp <= 0) {
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        return;
    }
    scratch         = SCRATCH_STACK_RESERVE_BLOCK(_HitScratch);
    scratch->hitKey = _cafeteriaBossStrangerFindAttackContact(&scratch->hitPos, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    if (!scratch->hitKey) {
        scratch->hitKey = _cafeteriaBossStrangerFindAttackContact(&scratch->hitPos, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    }
    if (scratch->hitKey) {
        priorState = work->state;
        if ((priorState == STATE_PATROL) || (priorState == STATE_IDLE) || (priorState == STATE_LURK)) {
            work->state = STATE_ALERT;
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
            scratch->damage *= CRITICAL_DAMAGE_MULTIPLIER;
            effectSpawn(EFFECT_CRITICAL_HIT, task->extra.tmd->coords + CHEST_PART, 0, NULL);
        }
        // Retained behavior: the bonus reads old scratch bytes before this hit
        // writes hitYaw below. Their source is unproven, not necessarily a prior hit.
        retainedYawMagnitude = scratch->hitYaw;
        if (retainedYawMagnitude < 0) {
            retainedYawMagnitude = -retainedYawMagnitude;
        }
        if (retainedYawMagnitude >= RETAINED_YAW_BONUS_THRESHOLD) {
            scratch->damage *= RETAINED_YAW_DAMAGE_MULTIPLIER;
        }
        if (work->enraged == 1) {
            scratch->damage >>= 1;
        }
        // Credit drain and record damage before composing the current hit bearing.
        _cafeteriaBossStrangerAccountAttackDamage(enemy, scratch);
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
        _cafeteriaBossStrangerSpawnHitEffect(task, scratch->hitYaw, scratch->hitKey);
        work->lookYaw       = 0;
        work->lookYawTarget = 0;
        if ((work->enraged == 0) && (enemy->hp < (_gCafeteriaBossStrangerEnemyParams.hpMax >> 1))) {
            work->state   = STATE_ENRAGE;
            work->enraged = 1;
        }
        if (enemy->hp <= 0) {
            work->hitBody.pos.vx    = 0;
            work->hitBody.pos.vy    = 0;
            work->hitBody.pos.vz    = 0;
            work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->hitBody.coord     = task->extra.tmd->coords + CHEST_PART;
            work->lookYaw           = 0;
            work->lookYawTarget     = 0;
            displaySetShakeY(0);
        } else {
            audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(SOUND_STRANGER_HURT, (s32)audioPan, (s32)(s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        }
        work->hitCooldown = damageGetPlayerAttackHitCooldown(scratch->hitKey);
        reaction          = damageGetPlayerAttackReaction(scratch->hitKey) & REACTION_ATTRIBUTE_MASK;
        switch (reaction) {
            case DAMAGE_PLAYER_REACTION_NONE:
            case REACTION_RESET_BLEND_4:
            case REACTION_RESET_BLEND_5:
            case DAMAGE_PLAYER_REACTION_EXPLOSION:
            case DAMAGE_PLAYER_REACTION_INCENDIARY:
                work->blendActive  = 1;
                work->blendAnimId  = BOSS_STRANGER_ANIMATION_HIT_REACTION;
                work->blendRequest = ANIM_REQUEST_RESET;
                break;
            case DAMAGE_PLAYER_REACTION_STAGGER:
            case REACTION_IGNORE_8:
            case REACTION_IGNORE_9:
                break;
            case DAMAGE_PLAYER_REACTION_BUILDUP:
                damageStartEnemyBuildup(enemy, scratch->hitKey, 0);
                work->state = STATE_STATUS_HOLD;
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
            if (work->state != STATE_STATUS_HOLD) {
                work->blendActive  = 1;
                work->blendAnimId  = BOSS_STRANGER_ANIMATION_HIT_REACTION;
                work->blendRequest = ANIM_REQUEST_RESET;
            } else {
                work->prevState = PREV_STATE_NONE;
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_HitScratch);
}

/// Stops the walker and alternates the idle clip with an occasional variation.
static void _cafeteriaBossStrangerIdleState(Task* task)
{
    enum { IDLE_DECELERATION = 8 };

    _Work*              work;
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
        work->animRequest             = ANIM_REQUEST_BLEND;
        work->animId                  = BOSS_STRANGER_ANIMATION_IDLE;
        work->animRate                = work->baseRate;
        ENTER_IDLE_WALKER(work, walker, entrySpeed, 0, IDLE_DECELERATION);
    }
    _bossStrangerTick(&work->walker);
    _cafeteriaBossStrangerTickAnimation(task);
    if (work->animId == BOSS_STRANGER_ANIMATION_IDLE) {
        if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP) {
            randomValue     = RANDOM_LCG_NEXT(gRandomLcgState);
            gRandomLcgState = randomValue;
            if (!((randomValue >> 16) & 7)) {
                work->animId      = BOSS_STRANGER_ANIMATION_IDLE_VARIATION;
                work->animRequest = ANIM_REQUEST_BLEND;
                _cafeteriaBossStrangerTickAnimation(task);
            }
        }
    }
    if ((work->animId == BOSS_STRANGER_ANIMATION_IDLE_VARIATION) && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        work->animId      = BOSS_STRANGER_ANIMATION_IDLE;
        work->animRequest = ANIM_REQUEST_BLEND;
        _cafeteriaBossStrangerTickAnimation(task);
    }
}

/// Plays the backward fall and landing clips, then selects DOWN or DEATH_BURN.
static void _cafeteriaBossStrangerFallBackState(Task* task)
{
    enum { SPEED      = 0xFFFE,
           SPEED_STEP = 2 };

    _Work*              work;
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
        work->animRequest             = ANIM_REQUEST_RESET;
        work->animId                  = BOSS_STRANGER_ANIMATION_FALL_BACK;
        work->animRate                = ANIMATION_RATE_ONE;
        ENTER_IDLE_WALKER(work, walker, entrySpeed, SPEED, SPEED_STEP);
    }
    landingWalker = &work->walker;
    _bossStrangerTick(landingWalker);
    _cafeteriaBossStrangerTickAnimation(task);
    if (work->animId == BOSS_STRANGER_ANIMATION_FALL_BACK) {
        if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
            landingSpeed               = work->walker.speed;
            work->animId               = BOSS_STRANGER_ANIMATION_FALL_BACK_END;
            work->animRequest          = ANIM_REQUEST_RESET;
            landingWalker->speedTarget = 0;
            landingWalker->speedStep   = SPEED_STEP;
            landingWalker->speed       = landingSpeed;
            return;
        }
    }
    if ((work->animId == BOSS_STRANGER_ANIMATION_FALL_BACK_END) && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        if (enemy->hp > 0) {
            work->state = STATE_DOWN;
        } else {
            work->state = STATE_DEATH_BURN;
        }
    }
}

/// Flattens the burning corpse around its current yaw at the current burn tick.
static inline void _cafeteriaBossStrangerShrinkBurnRootYaw(Task* task, const _Work* work, s16 heightScale)
{
    enum { BURN_SHRINK_ORIGIN_TICK = 300 };

    TmdObject*            model;
    GfxCoord*             rootCoord;
    s16                   horizontalScale;
    ActorScaleRotScratch* yawScratch;
    s16                   yaw;

    yawScratch      = SCRATCH_STACK_RESERVE_BLOCK(ActorScaleRotScratch);
    model           = task->extra.tmd;
    rootCoord       = model->coords;
    horizontalScale = work->walker.scale;
    heightScale    -= (work->stateFrame - BURN_SHRINK_ORIGIN_TICK) * 2;
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
static inline void _cafeteriaBossStrangerFadeBurnColor(_Work* work, VECTOR* colorScale)
{
    work->colorMtx = work->burnColorMtx;
    ScaleMatrix(&work->colorMtx, colorScale);
    gte_lddp(colorScale->vz);
    gte_ldlvl(&work->colorMtx.t[0]);
    gte_gpf12();
    gte_stlvl(&work->colorMtx.t[0]);
}

/// Burns, flattens and fades the dead Stranger while its task remains alive.
static void _cafeteriaBossStrangerDeathBurnState(Task* task)
{
    enum {
        FRAME_LIMIT                = 1000,
        EFFECT_FRAME_1             = 200,
        EFFECT_FRAME_2             = 400,
        FADE_START_FRAME           = 230,
        UNUSED_OFFSET_FRAME_1      = 250,
        UNUSED_OFFSET_FRAME_2      = 420,
        REASSERT_TRANSLUCENT_FRAME = 253,
        HIDE_FRAME                 = 600,
        FLATTEN_START_FRAME        = 201,
        EFFECT_ARGUMENT            = 3,
        EFFECT_OFFSET              = 300,
        UNUSED_OFFSET              = 400,
        FADE_ORIGIN_Q12            = 3000,
        FADE_STEP_Q12              = 4,
        HEIGHT_FLOOR_Q12           = 2048,
        HEIGHT_STEP_Q12            = 8
    };

    _Work*  work;
    Enemy*  enemy;
    SVECTOR effectOffset;
    VECTOR  colorScale;
    s16     heightScale;

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
    if (work->stateFrame < FRAME_LIMIT) {
        work->stateFrame = (u16)work->stateFrame + 1;
    }
    switch (work->stateFrame) {
        case FADE_START_FRAME:
            task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            work->burnColorMtx     = work->colorMtx;
            break;
        case EFFECT_FRAME_1:
        case EFFECT_FRAME_2:
            effectOffset.vx = EFFECT_OFFSET;
            effectOffset.vy = 0;
            effectOffset.vz = 0;
            effectSpawn(EFFECT_CORPSE_BURN, &task->extra.tmd->coords[LOWER_NECK_PART], EFFECT_ARGUMENT, &effectOffset);
            break;
        // These retail cue frames prepare an offset without spawning an effect.
        case UNUSED_OFFSET_FRAME_1:
        case UNUSED_OFFSET_FRAME_2:
            effectOffset.vx = UNUSED_OFFSET;
            effectOffset.vy = 0;
            effectOffset.vz = 0;
            break;
        case HIDE_FRAME:
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
    // Reapply the fade to the saved matrix, rather than compounding it each tick.
    if (work->stateFrame >= FADE_START_FRAME) {
        colorScale.vx = colorScale.vy = colorScale.vz = FADE_ORIGIN_Q12 + work->stateFrame * -FADE_STEP_Q12;
        _cafeteriaBossStrangerFadeBurnColor(work, &colorScale);
    }
    if (work->stateFrame == REASSERT_TRANSLUCENT_FRAME) {
        task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
    }
    if (work->stateFrame >= FLATTEN_START_FRAME) {
        heightScale = work->burnHeightScale;
        if (work->burnHeightScale >= HEIGHT_FLOOR_Q12 + 1) {
            heightScale          -= HEIGHT_STEP_Q12;
            work->burnHeightScale = heightScale;
            _cafeteriaBossStrangerShrinkBurnRootYaw(task, work, heightScale);
        }
    }
}

/// Hides the dying body and spawns five textured body-part effects on state entry.
static void _cafeteriaBossStrangerDeathBurstState(Task* task)
{
    enum {
        SMALL_PUFF_SIZE = 0x200,
        LARGE_PUFF_SIZE = 0x300,
    };

    _Work*     work;
    Enemy*     enemy;
    TmdObject* bodyModel;

    work = task->work;
    if (work->stateEntered != 0) {
        enemy                         = task->spawnArg2.pointer;
        bodyModel                     = task->extra.tmd;
        bodyModel->flags              = 0;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->walker.turnLimit        = 0;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        bodyModel->flags              = TMD_OBJECT_SKIP_ACTIVE_DRAW;

        // Detach five parts; each successful spawn resolves the current placement anew.
        _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, &task->extra.tmd->coords[JAW_PART], SMALL_PUFF_SIZE, NULL), enemy);

        _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, &task->extra.tmd->coords[RIGHT_FOREARM_PART], SMALL_PUFF_SIZE, NULL), enemy);

        _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, &task->extra.tmd->coords[LEFT_FOREARM_PART], SMALL_PUFF_SIZE, NULL), enemy);

        _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, &task->extra.tmd->coords[RIGHT_THIGH_PART], LARGE_PUFF_SIZE, NULL), enemy);

        _actorRenderApplyEffectPlacementTextureOffsets(effectSpawn(EFFECT_BURST_BODY_PART_BANK10, &task->extra.tmd->coords[LEFT_THIGH_PART], LARGE_PUFF_SIZE, NULL), enemy);
    }
}

/// Part-local offset for the scripted triple hit: 100 model units along the jaw's Z axis.
static const SVECTOR _gCafeteriaBossStrangerScriptedHitOffset = { 0, 0, 100, 0 };

/// Plays the script-selected clip and its cafeteria-command hit effects.
static void _cafeteriaBossStrangerScriptedState(Task* task)
{
    enum {
        HIT_ATTACK_KEY          = 0x1001,
        EFFECT_ARGUMENT         = 0x100,
        EFFECT_REPEATS          = 3,
        TRANSITION_CUE          = 11,
        TRIPLE_HIT_CUE          = 4,
        TRANSITION_BLEND_FRAMES = 6,
    };

    _Work*          work;
    Enemy*          enemy;
    SVECTOR         localHitOffset;
    EffectSpawnArg* alternateEffect;
    EffectSpawnArg* tailEffect;
    GfxCoord*       firstRootCoord;
    GfxCoord*       alternateRootCoord;
    GfxCoord*       tripleRootCoord;
    u32             randomValue;

    work = task->work;
    if (work->stateEntered != 0) {
        enemy                         = task->spawnArg2.pointer;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->animRequest             = ANIM_REQUEST_RESET;
        work->animRate                = ANIMATION_RATE_ONE;
        work->lookYawTarget           = 0;
        work->lookYaw                 = 0;
        if ((work->lastCommand.stage == GAME_STAGE_AKROPOLIS) && (work->lastCommand.area == GAME_AREA_AKROPOLIS_CAFETERIA) && (work->lastCommand.command == CAFETERIA_COMMAND_RANDOM_HIT)) {
            randomValue     = RANDOM_LCG_NEXT(gRandomLcgState);
            gRandomLcgState = randomValue;
            if ((randomValue >> 16) & 1) {
                localHitOffset.vx                               = -0xE;
                localHitOffset.vy                               = 0;
                localHitOffset.vz                               = 0;
                firstRootCoord                                  = task->extra.tmd->coords;
                _gCafeteriaBossStrangerHitEffectArgs.spawnArgLo = EFFECT_ARGUMENT;
                _gCafeteriaBossStrangerHitEffectArgs.spawnArgHi = EFFECT_REPEATS;
                _gCafeteriaBossStrangerHitEffectArgs.coord      = firstRootCoord;
                effectSpawnHit(damageGetPlayerAttackEffectId(HIT_ATTACK_KEY), &task->extra.tmd->coords[LEFT_UPPER_ARM_PART], &localHitOffset, &_gCafeteriaBossStrangerHitEffectArgs);
            } else {
                alternateEffect             = &_gCafeteriaBossStrangerHitEffectArgs;
                localHitOffset.vx           = -0x19;
                localHitOffset.vy           = 0;
                localHitOffset.vz           = 0;
                alternateRootCoord          = task->extra.tmd->coords;
                alternateEffect->spawnArgLo = EFFECT_ARGUMENT;
                alternateEffect->spawnArgHi = EFFECT_REPEATS;
                alternateEffect->coord      = alternateRootCoord;
                effectSpawnHit(damageGetPlayerAttackEffectId(HIT_ATTACK_KEY), &task->extra.tmd->coords[CHEST_PART], &localHitOffset, &_gCafeteriaBossStrangerHitEffectArgs);
            }
        }
    }
    // Play the selected room clip without the hit-reaction overlay.
    work->blendActive = 0;
    _cafeteriaBossStrangerTickAnimation(task);

    _actorRenderRescaleYaw(task->extra.tmd->coords, work->walker.scale);

    if ((work->lastCommand.stage == GAME_STAGE_AKROPOLIS) && (work->lastCommand.area == GAME_AREA_AKROPOLIS_CAFETERIA) && (work->lastCommand.command == CAFETERIA_COMMAND_FOLLOW_UP_TRIPLE_HIT) && (work->animId != BOSS_STRANGER_ANIMATION_FALL_BACK_END)) {
        if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == TRANSITION_CUE) {
            _gCafeteriaBossStrangerAnimBlendFrames[BOSS_STRANGER_ANIMATION_SCRIPTED_2][BOSS_STRANGER_ANIMATION_FALL_BACK_END] = TRANSITION_BLEND_FRAMES;
            work->animId                                                                                                      = BOSS_STRANGER_ANIMATION_FALL_BACK_END;
            work->animRequest                                                                                                 = ANIM_REQUEST_BLEND;
        }
        if (work->animId != BOSS_STRANGER_ANIMATION_FALL_BACK_END) {
            if (((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == TRIPLE_HIT_CUE) && (work->lastCueIndex != (work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK))) {
                localHitOffset         = _gCafeteriaBossStrangerScriptedHitOffset;
                tailEffect             = &_gCafeteriaBossStrangerHitEffectArgs;
                tripleRootCoord        = task->extra.tmd->coords;
                tailEffect->spawnArgLo = EFFECT_ARGUMENT;
                tailEffect->spawnArgHi = EFFECT_REPEATS;
                tailEffect->coord      = tripleRootCoord;
                effectSpawnHit(damageGetPlayerAttackEffectId(HIT_ATTACK_KEY), &task->extra.tmd->coords[JAW_PART], &localHitOffset, &_gCafeteriaBossStrangerHitEffectArgs);
                effectSpawnHit(damageGetPlayerAttackEffectId(HIT_ATTACK_KEY), &task->extra.tmd->coords[JAW_PART], &localHitOffset, &_gCafeteriaBossStrangerHitEffectArgs);
                effectSpawnHit(damageGetPlayerAttackEffectId(HIT_ATTACK_KEY), &task->extra.tmd->coords[JAW_PART], &localHitOffset, &_gCafeteriaBossStrangerHitEffectArgs);
            }
        }
    }
}

/// Thrashes through two clips before handing the corpse to DEATH_BURN.
static void _cafeteriaBossStrangerDeathThrashState(Task* task)
{
    enum {
        THRASH_TICKS           = 31,
        THRASH_RATE            = 2 * ANIMATION_RATE_ONE,
        THRASH_FORWARD_YAW     = 50,
        THRASH_REVERSE_YAW     = -120,
        THRASH_RANDOM_MASK     = 15,
        THRASH_FORWARD_CHOICES = 10,
    };

    _Work* work;
    Enemy* enemy;
    u32    randomValue;

    work = task->work;
    if (work->stateEntered != 0) {
        enemy                         = task->spawnArg2.pointer;
        task->extra.tmd->flags        = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->animRate                = THRASH_RATE;
        work->animRequest             = ANIM_REQUEST_BLEND;
        work->walker.turnLimit        = 0;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        work->animId                  = BOSS_STRANGER_ANIMATION_THRASH;
    }
    _cafeteriaBossStrangerTickAnimation(task);
    // Bias the root jerk forward, then finish on the transition clip.
    if (work->animId == BOSS_STRANGER_ANIMATION_THRASH) {
        randomValue     = RANDOM_LCG_NEXT(gRandomLcgState);
        gRandomLcgState = randomValue;
        if (((randomValue >> 16) & THRASH_RANDOM_MASK) < THRASH_FORWARD_CHOICES) {
            gfxRotMatrixY(&task->extra.tmd->coords->coord, THRASH_FORWARD_YAW, 0);
        } else {
            gfxRotMatrixY(&task->extra.tmd->coords->coord, THRASH_REVERSE_YAW, 0);
        }
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        if ((work->animId == BOSS_STRANGER_ANIMATION_THRASH) && (work->animFrames >= THRASH_TICKS)) {
            work->animRate    = ANIMATION_RATE_ONE;
            work->animRequest = ANIM_REQUEST_BLEND;
            work->animId      = BOSS_STRANGER_ANIMATION_THRASH_END;
        }
    }
    if ((work->animId == BOSS_STRANGER_ANIMATION_THRASH_END) && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        work->state = STATE_DEATH_BURN;
    }
}

/// Holds the lurk pose until the player is within 3000 horizontal units.
static void _cafeteriaBossStrangerLurkState(Task* task)
{
    enum {
        LURK_NOTICE_RADIUS = 3000,
    };

    _Work*     work;
    TmdObject* model;
    Enemy*     enemy;
    GfxCoord*  rootCoord;
    SVECTOR    playerOffset;
    SVECTOR*   offset;

    work = task->work;
    if (work->stateEntered != 0) {
        enemy                         = task->spawnArg2.pointer;
        model                         = task->extra.tmd;
        work->animId                  = BOSS_STRANGER_ANIMATION_SCRIPTED_0;
        work->animRequest             = ANIM_REQUEST_RESET;
        model->flags                  = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->walker.turnLimit        = 0;
        work->lookYaw                 = 0;
        work->animRate                = ANIMATION_RATE_ONE;
        work->lookYawTarget           = 0;
    }
    _cafeteriaBossStrangerTickAnimation(task);
    work->lastCueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    rootCoord          = task->extra.tmd->coords;
    offset             = &playerOffset;
    playerOffset.vx    = (u16)gPlayerStatus.coordMtx->t[0] - (u16)rootCoord->coord.t[0];
    offset->vy         = (u16)gPlayerStatus.coordMtx->t[1] - (u16)rootCoord->coord.t[1];
    offset->vz         = (u16)gPlayerStatus.coordMtx->t[2] - (u16)rootCoord->coord.t[2];
    if (!actorOutsideRadius(offset, LURK_NOTICE_RADIUS)) {
        work->state = STATE_LURK_ALERT;
    }
}

/// Plays the lurk wake-up alert while un-lockable, then starts chasing.
static void _cafeteriaBossStrangerLurkAlertState(Task* task)
{
    _Work*     work;
    TmdObject* model;
    Enemy*     enemy;
    GfxCoord*  rootCoord;
    SVECTOR    playerOffset;
    SVECTOR*   offset;
    s16        playerTurn;

    work = task->work;
    if (work->stateEntered != 0) {
        enemy                         = task->spawnArg2.pointer;
        model                         = task->extra.tmd;
        work->animId                  = BOSS_STRANGER_ANIMATION_ALERT;
        work->animRequest             = ANIM_REQUEST_BLEND;
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
    _cafeteriaBossStrangerTickAnimation(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = STATE_CHASE;
    }
}

/// Jolts the root along its local X axis for five ticks, then starts chasing.
static void _cafeteriaBossStrangerShudderState(Task* task)
{
    enum {
        PUSH0      = 800,
        PUSH1      = -1000,
        PUSH2      = 400,
        PUSH3      = -400,
        PUSH4      = 200,
        PUSH_COUNT = 5,
    };

    _Work*     work;
    TmdObject* model;
    Enemy*     enemy;
    SVECTOR    sideStep;
    GfxCoord*  rootCoord;

    work  = task->work;
    model = task->extra.tmd;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model->flags                       = 0;
        work->attackBody.flags             = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags               = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags      = WORLD_TARGET_HIDE_HP;
        _gCafeteriaBossStrangerShudderStep = 0;
    }
    // Normalize the root's side axis so displacement is in parent-coordinate units.
    gfxReadMatrixXAxis(&task->extra.tmd->coords->coord, &sideStep);
    VectorNormalSS(&sideStep, &sideStep);
/// Scales a Q12 shudder direction by a signed parent-coordinate distance.
#define SCALE_SHUDDER_STEP(direction, distance) \
    {                                           \
        gte_lddp(distance);                     \
        gte_ldsv(direction);                    \
        gte_gpf12();                            \
        gte_stsv(direction);                    \
    }
    switch (_gCafeteriaBossStrangerShudderStep) {
        case 0:
            SCALE_SHUDDER_STEP(&sideStep, PUSH0);
            break;
        case 1:
            SCALE_SHUDDER_STEP(&sideStep, PUSH1);
            break;
        case 2:
            SCALE_SHUDDER_STEP(&sideStep, PUSH2);
            break;
        case 3:
            SCALE_SHUDDER_STEP(&sideStep, PUSH3);
            break;
        case 4:
            SCALE_SHUDDER_STEP(&sideStep, PUSH4);
            break;
        case PUSH_COUNT:
            work->state = STATE_CHASE;
            return;
    }
#undef SCALE_SHUDDER_STEP
    task->extra.tmd->coords->coord.t[0] += sideStep.vx;
    task->extra.tmd->coords->coord.t[2] += sideStep.vz;
    rootCoord                            = task->extra.tmd->coords;
    _gCafeteriaBossStrangerShudderStep++;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Builds the enrage tint while oscillating the pose, then accelerates chase.
static void _cafeteriaBossStrangerEnrageState(Task* task)
{
    enum {
        ENRAGE_STAGE_ENTER       = 0,
        ENRAGE_STAGE_OSCILLATE   = 1,
        ENRAGE_ENTRY_RATE        = 6,
        ENRAGE_REVERSE_RATE      = -12,
        ENRAGE_FORWARD_RATE      = 8,
        ENRAGE_CUE               = 4,
        ENRAGE_OSCILLATION_TICKS = 53,
        ENRAGE_TINT_STEP         = 39,
    };

    _Work*     work;
    Enemy*     enemy;
    TmdObject* model;
    s16        halvedRate;
    s32        enrageStage;

    work = task->work;
    if (work->stateEntered != 0) {
        enemy                         = task->spawnArg2.pointer;
        model                         = task->extra.tmd;
        model->flags                  = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->animId                  = BOSS_STRANGER_ANIMATION_FALL;
        work->animRequest             = ANIM_REQUEST_RESET;
        work->walker.turnLimit        = 0;
        work->lookYaw                 = 0;
        work->animRate                = ENRAGE_ENTRY_RATE;
        _cafeteriaBossStrangerTickAnimation(task);
        work->stateFrame  = 0;
        work->enrageStage = ENRAGE_STAGE_ENTER;
    }
    enrageStage       = work->enrageStage;
    work->blendActive = 0;
    switch (enrageStage) {
        case ENRAGE_STAGE_ENTER:
            _cafeteriaBossStrangerTickAnimation(task);
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == ENRAGE_CUE) {
                work->animRate    = -ANIMATION_RATE_ONE;
                work->enrageStage = (s16)((u16)work->enrageStage + 1);
                return;
            }
            return;
        case ENRAGE_STAGE_OSCILLATE:
            // Alternate decaying forward/reverse rates while accumulating the tint.
            halvedRate     = work->animRate / 2;
            work->animRate = halvedRate;
            work->stateFrame++;
            if (work->animRate == enrageStage) {
                work->animRate = ENRAGE_REVERSE_RATE;
            }
            if (work->animRate == -1) {
                work->animRate = ENRAGE_FORWARD_RATE;
            }
            _cafeteriaBossStrangerTickAnimation(task);
            if (work->stateFrame >= ENRAGE_OSCILLATION_TICKS) {
                work->animRate = ENRAGED_BASE_RATE;
                work->baseRate = ENRAGED_BASE_RATE;
                work->state    = STATE_CHASE;
            }
            work->enrageTint += ENRAGE_TINT_STEP;
            break;
    }
}

/// State handlers indexed by `_Work::state`, copied before each dispatch.
static const _StateTable _gCafeteriaBossStrangerStateHandlers = { {
    _cafeteriaBossStrangerHiddenState,
    NULL,
    _cafeteriaBossStrangerPatrolState,
    _cafeteriaBossStrangerChaseState,
    _cafeteriaBossStrangerAlertState,
    _cafeteriaBossStrangerAttackState,
    _cafeteriaBossStrangerIdleState,
    _cafeteriaBossStrangerFallBackState,
    _cafeteriaBossStrangerFallState,
    _cafeteriaBossStrangerRiseBackState,
    _cafeteriaBossStrangerRiseState,
    _cafeteriaBossStrangerDownState,
    _cafeteriaBossStrangerDeathBurnState,
    _cafeteriaBossStrangerDeathBurstState,
    _cafeteriaBossStrangerStatusHoldState,
    NULL,
    NULL,
    _cafeteriaBossStrangerScriptedState,
    NULL,
    _cafeteriaBossStrangerDeathThrashState,
    _cafeteriaBossStrangerLurkState,
    _cafeteriaBossStrangerLurkAlertState,
    _cafeteriaBossStrangerShudderState,
    _cafeteriaBossStrangerAlertRewindState,
    _cafeteriaBossStrangerEnrageState,
} };

/// Discards the Boss Stranger's grid, received-hit and outgoing-attack contacts.
static inline void _cafeteriaBossStrangerClearTickContacts(_Work* work)
{
    worldCollisionClearContacts(work->gridContacts);
    worldCollisionClearContacts(work->hitContacts);
    worldCollisionClearContacts(work->attackContacts);
}

/// Updates the Boss Stranger state, collision pose, damage and lighting each tick.
static void _cafeteriaBossStrangerUpdate(Enemy* enemy, Task* task)
{
    enum {
        SHADOW_HALF_SIZE = 640,
    };

    VECTOR      lightingPosition;
    _StateTable states;
    _Work*      work;

    work   = task->work;
    states = _gCafeteriaBossStrangerStateHandlers;

    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
    lightingPosition.vx = task->extra.tmd->coords->workm.t[0];
    lightingPosition.vy = task->extra.tmd->coords->workm.t[1];
    lightingPosition.vz = task->extra.tmd->coords->workm.t[2];
    worldCoordUpdateActorColor(enemy, &lightingPosition, 0, 0);

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if ((work->state != STATE_HIDDEN) && (work->state != STATE_DEATH_BURN)) {
                task->extra.tmd->flags = 0;
                effectDrawGroundShadow(GRAPHICS_MATRIX_TRANSLATION(&task->extra.tmd->coords->workm), SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            if ((work->state != STATE_DEATH_BURN) && (work->state != STATE_HIDDEN)) {
                effectDrawGroundShadow(GRAPHICS_MATRIX_TRANSLATION(&task->extra.tmd->coords->workm), SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
            }
            _cafeteriaBossStrangerClearTickContacts(work);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            _cafeteriaBossStrangerClearTickContacts(work);
            return;
    }

    // Publish state entry and the pre-step grid position before dispatch.
    if (work->prevState != work->state) {
        work->stateEntered = 1;
    } else {
        work->stateEntered = 0;
    }
    work->prevState = work->state;

    work->gridBody.pos.vx = (u16)task->extra.tmd->coords->coord.t[0];
    work->gridBody.pos.vy = (u16)task->extra.tmd->coords->coord.t[1];
    work->gridBody.pos.vz = (u16)task->extra.tmd->coords->coord.t[2];
    states.handlers[work->state](task);
    work->gridBody.pos.vx = (u16)task->extra.tmd->coords->coord.t[0];
    work->gridBody.pos.vy = (u16)((u16)task->extra.tmd->coords->coord.t[1] - GRID_BODY_HEIGHT);
    work->gridBody.pos.vz = (u16)task->extra.tmd->coords->coord.t[2];

    // Apply damage after the pose update, then discard this tick's contacts.
    if (enemy->hp > 0) {
        if (work->hitCooldown > 0) {
            work->hitCooldown--;
        } else {
            _cafeteriaBossStrangerApplyDamage(task);
        }
    }
    if ((enemy->hp <= 0) && (gPlayerStatus.hp <= 0)) {
        enemy->hp = 1;
    }
    _cafeteriaBossStrangerClearTickContacts(work);
    if (gGameSession->viewReady != 0) {
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (task->extra.tmd->coords->composeStamp == GRAPHICS_COORD_DIRTY) {
        work->rootDirty = 1;
    } else {
        work->rootDirty = 0;
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
    work->colorMtx.t[1] -= work->enrageTint;
    work->colorMtx.t[2] -= work->enrageTint;
    work->colorMtx.t[0] -= (work->enrageTint * 2) / 3;
    if ((work->state == STATE_DEATH_BURN) || (work->state == STATE_HIDDEN) || (work->state == STATE_DEATH_BURST) || (work->state == STATE_DEATH_THRASH)) {
        work->hitBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    } else {
        work->hitBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
}

/// Receives `MESSAGE_IGNORE` without changing the actor.
static void _cafeteriaBossStrangerIgnoreMessage(Task* task, s32 messageId, s32 firstArg, s32 secondArg)
{
}

/// Enemy-task callbacks for states 0 spawn, 1 update and 2 destroy.
static const EnemyTaskFuncTable3 _gCafeteriaBossStrangerTaskHandlers = {
    _cafeteriaBossStrangerSpawn,
    _cafeteriaBossStrangerUpdate,
    enemyDestroy,
};

/// Selects a scripted clip and restarts entry into the SCRIPTED state.
static s32 _cafeteriaBossStrangerPlayScriptedAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 secondArg)
{
    enum { RESUME_SCRIPTED = 1 };

    _Work* work;
    Enemy* enemy;
    s32    animationSelector;

    animationSelector = request->animationId;
    work              = task->work;
    enemy             = task->spawnArg2.pointer;
    switch (animationSelector) {
        case 0:
            work->animId      = BOSS_STRANGER_ANIMATION_SCRIPTED_0;
            enemy->spawnState = RESUME_SCRIPTED;
            areaSaveEnemyPose(enemy);
            break;
        case 1:
            work->animId = BOSS_STRANGER_ANIMATION_SCRIPTED_1;
            break;
        case 2:
            work->animId = BOSS_STRANGER_ANIMATION_SCRIPTED_2;
            break;
        case 3:
            work->animId = BOSS_STRANGER_ANIMATION_SCRIPTED_3;
            break;
        case 4:
            enemy->spawnState = RESUME_SCRIPTED;
            areaSaveEnemyPose(enemy);
            work->animId = BOSS_STRANGER_ANIMATION_SCRIPTED_4;
            break;
    }
    work->state     = STATE_SCRIPTED;
    work->prevState = PREV_STATE_NONE;
    return 0;
}

/// Sets draw/buffer handling while respecting the enemy's saved resume state.
static s32 _cafeteriaBossStrangerSetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 secondArg)
{
    enum {
        DRAW_HIDE_ALLOCATE           = 0,
        DRAW_SHOW_ALLOCATE           = 1,
        DRAW_SKIP_AUTO_BUFFER        = 2,
        DRAW_RESUME_SKIP_AUTO_BUFFER = 3,
        RESUME_HIDDEN                = 4,
    };

    TmdObject* model;
    _Work*     work;
    Enemy*     enemy;

    model = task->extra.tmd;
    enemy = task->spawnArg2.pointer;
    work  = task->work;
    switch (drawMode) {
        case DRAW_HIDE_ALLOCATE:
            model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(model);
            work->state = STATE_HIDDEN;
            break;
        case DRAW_SHOW_ALLOCATE:
            if (enemy->spawnState == 0) {
                model->flags = 0;
                tmdAllocPrimitiveBuffer(model);
            } else if (enemy->spawnState == RESUME_HIDDEN) {
                model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                work->state  = STATE_HIDDEN;
            } else {
                model->flags = 0;
                tmdAllocPrimitiveBuffer(model);
            }
            break;
        case DRAW_SKIP_AUTO_BUFFER:
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->state   = STATE_HIDDEN;
            break;
        case DRAW_RESUME_SKIP_AUTO_BUFFER:
            if (enemy->spawnState == RESUME_HIDDEN) {
                model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            } else {
                model->flags = 0;
            }
            work->state   = STATE_HIDDEN;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
    }
    return 0;
}

/// Returns 1 while the enemy is alive, otherwise clears its enrage effects.
static s32 _cafeteriaBossStrangerIsPresent(Task* task, s32 messageId, s32 firstArg, s32 secondArg)
{
    _Work* work;
    Enemy* enemy;

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

#include "../../../../shared/actor/coord_math_yaw_scale.inc.c"

/// Replaces a coordinate's rotation with its current yaw and independent axis scales.
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
static s32 _cafeteriaBossStrangerAlert(Task* task, s32 messageId, s32 firstArg, s32 secondArg)
{
    _Work* work;

    work = task->work;
    sceneAcquireBattleRef(0);
    work->state = STATE_ALERT;
    return 1;
}

/// Releases the actor's collision and child-task bindings before destroying it.
static void _cafeteriaBossStrangerExit(Task* task)
{
    _Work* work;
    Enemy* enemy;
    Task*  firstChildTask;
    Task*  secondChildTask;

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
static void _cafeteriaBossStrangerHiddenState(Task* task)
{
    TmdObject* model;
    _Work*     work;
    Enemy*     enemy;

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

/// Advances the overlay-wide five-tick vertical footstep shake and reports completion.
static s32 _cafeteriaBossStrangerStepFootstepShake(void)
{
    return _cafeteriaBossStrangerTickFootstepShake();
}

/// Plays the forward fall while edging ahead, then selects DOWN or DEATH_BURN.
static void _cafeteriaBossStrangerFallState(Task* task)
{
    enum { FORWARD_SPEED = 2,
           SPEED_STEP    = 8 };

    _Work*              work;
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
        work->animRequest             = ANIM_REQUEST_RESET;
        work->animId                  = BOSS_STRANGER_ANIMATION_FALL;
        work->animRate                = ANIMATION_RATE_ONE;
        ENTER_IDLE_WALKER(work, walker, entrySpeed, FORWARD_SPEED, SPEED_STEP);
    }
    _bossStrangerTick(&work->walker);
    _cafeteriaBossStrangerTickAnimation(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        if (enemy->hp > 0) {
            work->state = STATE_DOWN;
        } else {
            work->state = STATE_DEATH_BURN;
        }
    }
}

/// Holds the fallen pose for a random delay before choosing its recovery clip.
static void _cafeteriaBossStrangerDownState(Task* task)
{
    enum { DOWN_DELAY_MASK = 31 };

    _Work* work;
    u32    randomValue;
    s16    remainingTicks;

    work = task->work;
    if (work->stateEntered != 0) {
        randomValue      = RANDOM_LCG_NEXT(gRandomLcgState);
        gRandomLcgState  = randomValue;
        work->downFrames = (randomValue >> 16) & DOWN_DELAY_MASK;
    }
    remainingTicks   = work->downFrames - 1;
    work->downFrames = remainingTicks;
    if (remainingTicks < 0) {
        switch (work->animId) {
            case BOSS_STRANGER_ANIMATION_FALL_BACK_END:
                work->state = STATE_RISE_BACK;
                return;
            case BOSS_STRANGER_ANIMATION_FALL:
                work->state = STATE_RISE;
                break;
        }
    }
}

/// Plays the recovery from a backward fall, then resumes CHASE.
static void _cafeteriaBossStrangerRiseBackState(Task* task)
{
    enum { RISE_DECELERATION = 8 };

    _Work*              work;
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
        work->animRequest             = ANIM_REQUEST_RESET;
        work->animId                  = BOSS_STRANGER_ANIMATION_RISE_BACK;
        work->animRate                = work->baseRate;
        ENTER_IDLE_WALKER(work, walker, entrySpeed, 0, RISE_DECELERATION);
    }
    _bossStrangerTick(&work->walker);
    _cafeteriaBossStrangerTickAnimation(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = STATE_CHASE;
    }
}

/// Plays the recovery from a forward fall, then resumes CHASE.
static void _cafeteriaBossStrangerRiseState(Task* task)
{
    enum { RISE_DECELERATION = 8 };

    _Work*              work;
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
        work->animRequest             = ANIM_REQUEST_RESET;
        work->animId                  = BOSS_STRANGER_ANIMATION_RISE;
        work->animRate                = work->baseRate;
        ENTER_IDLE_WALKER(work, walker, entrySpeed, 0, RISE_DECELERATION);
    }
    _bossStrangerTick(&work->walker);
    _cafeteriaBossStrangerTickAnimation(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = STATE_CHASE;
    }
}

#undef ENTER_IDLE_WALKER

/// Seeds the alert pose ahead, then plays it backward at half rate.
static void _cafeteriaBossStrangerAlertRewindState(Task* task)
{
    enum { WARMUP_TICKS = 20,
           REVERSE_RATE = -8 };

    _Work*     work;
    Enemy*     enemy;
    TmdObject* model;
    s16        warmupTick;

    work       = task->work;
    warmupTick = 0;
    if (work->stateEntered != 0) {
        enemy                         = task->spawnArg2.pointer;
        model                         = task->extra.tmd;
        model->flags                  = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->animId                  = BOSS_STRANGER_ANIMATION_ALERT;
        work->animRequest             = ANIM_REQUEST_RESET;
        work->walker.turnLimit        = 0;
        work->lookYaw                 = 0;
        work->animRate                = ANIMATION_RATE_ONE;
        work->blendActive             = 0;
        // Apply the reset, then advance twenty normal-rate ticks before rewinding.
        _cafeteriaBossStrangerTickAnimation(task);
        work->stateFrame = 0;
        for (; warmupTick < WARMUP_TICKS; warmupTick++) {
            _cafeteriaBossStrangerTickAnimation(task);
        }
    }
    work->animRate = REVERSE_RATE;
    _cafeteriaBossStrangerTickAnimation(task);
}

/// Halves status-hold playback, reversing at +1 and restarting forward at -1.
static inline void _cafeteriaBossStrangerOscillateStatusRate(_Work* work)
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
static void _cafeteriaBossStrangerStatusHoldState(Task* task)
{
    enum { STATUS_INITIAL_RATE = 3 * ANIMATION_RATE_ONE };

    _Work*     work;
    Enemy*     enemy;
    TmdObject* model;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->animRequest       = ANIM_REQUEST_RESET;
        work->animId            = BOSS_STRANGER_ANIMATION_ATTACK;
        work->animRate          = STATUS_INITIAL_RATE;
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        _cafeteriaBossStrangerTickAnimation(task);
        _cafeteriaBossStrangerTickAnimation(task);
        return;
    }
    // Alternate decaying forward and reverse playback while buildup remains.
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _cafeteriaBossStrangerOscillateStatusRate(work);
    _cafeteriaBossStrangerTickAnimation(task);
    if (damageTickEnemyBuildup(enemy) == 1) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->state           = STATE_CHASE;
    }
}

/// Dispatches the Boss Stranger task through spawn, update and teardown.
static void _cafeteriaBossStrangerTask(Task* task)
{
    EnemyTaskFuncTable3 handlers;

    handlers = _gCafeteriaBossStrangerTaskHandlers;
    handlers.funcs[task->state](task->spawnArg2.pointer, task);
}
