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
#include "gameplay/object_fields.h"
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
#include "../../shared/coord_math.h"
#include "../../shared/player_detection.h"
#include "../../shared/actor_contacts.h"
#include "../../shared/boss_stranger.h"

/// 0x2C-byte scratch frame `func_actor_110600_80133778` opens on
/// the scratch stack to lay one patrol node out: `m` receives a copy of the
/// walker coordinate's matrix, `v` the facing column `gfxReadMatrixZAxis` reads
/// out of it once it has been rotated and scaled by the GTE, and `i` the node
/// index the two loops below walk.
typedef struct Actor110600TsvScratch {
    /* 0x00 */ SVECTOR v;
    /* 0x08 */ MATRIX  m;
    /* 0x28 */ s16     i;
    /* 0x2A */ byte    pad_2A[0x2];
} Actor110600TsvScratch;
STATIC_ASSERT_SIZEOF(Actor110600TsvScratch, 0x2C);

/// Returns the patrol node nearest the walker: the squared XZ distance between
/// each node and the low halfwords of the walker coordinate's translation,
/// with the running best and the cursor staged in a `BossStrangerNodeNearestSelfScratch`.

/// Returns the node the walker's route cursor steps onto, reseeding the scan's
/// stored node byte for the `actor` variant of the walker. Same body as the
/// acropolis bridge room's `func_acropolis_bridge_801843A0`.

/// Re-resolves the walker's patrol node against `nav`'s `nodeOrder` at the
/// walker's `cursor` once the state or the node bytes have moved. Same body as
/// the acropolis bridge room's `func_acropolis_bridge_80184638`.

/// One per-frame behaviour step the walker runs while `skipGround` is clear:
/// steps it toward its current patrol node. `func_800E0C10` produces the
/// 16.16 delta; the high half of each component becomes the whole-unit step,
/// rounded away from zero whenever a fraction is left over. While `lockHeight`
/// is set the walker is pinned vertically, otherwise Y also carries a constant
/// 0x10 fall. Y is applied in three bands: a +8 hop above 0x20, a -0x20 drop
/// below -0x20, and the plain step in between. `offOrigin` is 1 when the local
/// X or Z translation is nonzero afterwards; nothing reads it. Same body as
/// the acropolis bridge room's `func_acropolis_bridge_80184908`.

/// The second per-frame behaviour step, skipped while `skipAvoid` is set. Same
/// body as the acropolis bridge room's `func_acropolis_bridge_80184B94`.

/// Turns the walker towards `pos` by at most `turnLimit` angle units a frame.
/// The wrapped relative bearing drives the consecutive-turn counter, then
/// becomes the absolute yaw the model's saved scale matrix is rebuilt around.

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
    ACTOR_110600_STATE_ENRAGE       = 0x18  // convulses while `enrageTint` builds, then chases at a faster `baseRate`; entered once, by the hit that takes it below half its hit points
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

/// Damage and hit-direction values in the 0x30-byte scratchpad frame used by
/// func_actor_110600_80136210.
typedef struct Actor110600HitScratch {
    /* 0x00 */ VECTOR  delta;
    /* 0x10 */ SVECTOR direction;
    /* 0x18 */ SVECTOR point;
    /* 0x20 */ s32     key;
    /* 0x24 */ u32     damage;
    /* 0x28 */ s32     distance;
    /* 0x2C */ s16     angle;
    /* 0x2E */ s16     pad;
} Actor110600HitScratch;

STATIC_ASSERT_SIZEOF(Actor110600HitScratch, 0x30);

/// The actor's state handlers, indexed by `_Actor110600Work::state`.
/// `func_actor_110600_80137F2C` copies the table to its frame before
/// dispatching, the same local jump table `Gp_EnemyDispatch` builds for the
/// shared `Gp_EnemyWaitFuncs`; four of the 25 slots are still unused.
typedef struct Actor110600StateTable {
    TaskFunc fn[0x19];
} Actor110600StateTable;
STATIC_ASSERT_SIZEOF(Actor110600StateTable, 0x64);

static const Actor110600StateTable D_actor_110600_80131F3C;

// Animation sets supplied by the paired actor overlay.

/// Twelve `SVECTOR` hit positions `func_actor_110600_80135E20` picks from by
/// damage magnitude. The fourth halfword (`pad`, unused by the effect) is the
/// model part index the spawned effect anchors to.
extern SVECTOR D_actor_110600_801485C4[12];

/// The actor's four display slots, which the 0x401 events repoint at one of the
/// objects above; `D_actor_110600_80148598` is the one events 2 and 6 swap.
extern AnimationSet* D_actor_110600_80148594;
extern AnimationSet* D_actor_110600_80148598;
extern AnimationSet* D_actor_110600_8014859C;
extern AnimationSet* D_actor_110600_801485A0;

/// Actor-command handler: records the command's stage, area and low command
/// byte in the work block's `lastCommand`, then dispatches on its context. The
/// patio's context (0x301) with command 1 enters state 0x14; the cafeteria's
/// (0x401) picks a display slot and a `animId` state per command — 1, 8 and
/// 9 only set the state, and 9 shares its tail with the five commands that
/// repoint a slot — parking the actor in state 0x11 with `prevState` cleared.
/// Returns 1 when it applied the command, 0 otherwise. `arg1` is unused; it
/// exists because the dispatch passes three arguments.
s32 func_actor_110600_80134040(Task* arg0, s32 arg1, ActorCommand* arg2, s32 arg3);

/// The `0x7D3` display handler: parks the actor in state 0x11 with
/// `animId` set from the requested state.
s32 func_actor_110600_8013839C(Task* arg0, s32 arg1, AnimationPlayRequest* arg2, s32 arg3);

/// Rebuilds `coord`'s Y rotation from its current yaw (`ratan2` of
/// `-m[2][0], m[2][2]`), scaled independently on each axis through a
/// `ActorScaleRotScratch` block borrowed from the scratchpad. Marks the coordinate dirty.
static void func_actor_110600_80138680(GfxCoord* coord, s16 sx, s16 sy, s16 sz);

s32         func_actor_110600_801387C0(Task* arg0, s32 msgId, s32 arg2, s32 arg3);
static void func_actor_110600_801388A4(Task* arg0);

/// The remaining entries of `D_actor_110600_80131F3C` that are still only
/// present as assembly, so that the table can name them. `func_actor_110600_80135A18`
/// (state 4) and the ones above carry their own documentation.
static void func_actor_110600_80135454(Task* arg0);
static void func_actor_110600_80136B20(Task* arg0);
static void func_actor_110600_801372CC(Task* arg0);
static void func_actor_110600_801377FC(Task* arg0);
static void func_actor_110600_80138980(Task* arg0);
static void func_actor_110600_80138A70(Task* arg0);
static void func_actor_110600_80138AFC(Task* arg0);
static void func_actor_110600_80138BD0(Task* arg0);

/// The actor's per-tick model update, driven from `Task::work` /
/// `Task::spawnArg2` off the pointer it is handed.
static void func_actor_110600_80134728(Task* arg0);

/// Reports the sound cue the model is currently owed — 0 while there is none —
/// which `func_actor_110600_80134728` queues as the id's low byte. Watches the
/// pose of the animation slot the `animId` state selects, reporting the cue
/// once per pose and remembering it in `lastCueIndex`.
static s32 func_actor_110600_80134564(_Actor110600Work* work);

/// Aiming stage: wraps the yaw from the model's root coordinate to the camera
/// target `gPlayerStatus.coordMtx` against the coordinate's own yaw into `lookYawTarget`, ticks
/// the model, and moves the actor to state 3 once slot 1 of `rig` reports
/// `ANIMATION_SLOT_REACHED_BOUNDARY`.
static void func_actor_110600_80135A18(Task* arg0);

/// Picks one of twelve hit positions out of `D_actor_110600_801485C4` by damage
/// magnitude `arg1`, then spawns effect `Gp_GetIdParam1(arg2)` on the model
/// part that entry names.
static void func_actor_110600_80135E20(Task* arg0, s16 arg1, s32 arg2);

/// Per-tick walker step. Opens a scratch frame and runs the chase, close-in
/// or patrol, then the speed ramp and the optional ground and avoidance steps.

/// Measures the walker's node against the coordinate it is moving towards,
/// leaving the three per-axis deltas in the scratchpad, and reports whether it
/// has arrived: 1 while the delta is inside either of two radii -- the walker's
/// `speedTarget` * 4, or a flat 300 -- and 0 once it is outside both.

/// Steers the walker along its patrol route: resolves the node the route
/// cursor names, and on the frame `bossStrangerArrived` reports arrival
/// it raises the route's `arrived` flag, clears the turn counters and steps
/// the cursor onto the next node — wrapping back to the first at
/// `OVERLAY_WALKER_ROUTE_END`. `pos` receives the position of the node it is
/// heading for, so on the arrival frame it already describes the new node.

/// Enters work state 2 (`animRequest`) on a live actor: clear the model object,
/// clear bit 0x8000 of `attackBody.flags` and set 0x4000 of `gridBody.flags`,
/// tag the enemy's link node, arm the `animId` / `animRate` timers, then run
/// 20 update ticks before parking `animRate` at -8 and ticking once more.
static void func_actor_110600_80138CA4(Task* arg0);

/// Re-enters work state 2 on a live actor: clear the model root coordinate,
/// re-allocate its TMD buffers, tag the enemy's link node, arm `animRequest` /
/// `animId` / `animRate` and the `gridBody.flags` 0x4000 / `attackBody.flags`
/// 0x8000 masks, then tick twice. On a dead one it is the model-shrink tail:
/// halves `animRate` each tick — parking at -0x10 when the halving lands on 1
/// and bouncing -1 back to 0x10 — and once `Gp_TickObjFlag2` reports 1, drops
/// bit 1 of the enemy node's flags and moves the actor to state 3.
static void func_actor_110600_80138D7C(Task* arg0);

/// Placement opcode: seeds the model's root coordinate from `placement`, then
/// rebuilds and rescales it from the actor's own heading.
s32 func_actor_110600_80133E48(Task* task, s32 arg1, ActorTransform* placement, s32 arg3);

/// Five-frame shake counter. Incremented each call, wraps at 5, and drives
/// `displaySetShakeY` with the low bit (0 or 1). Returns 1 on wrap.
extern s16 D_actor_110600_8014865C;
static s32 func_actor_110600_80138900(void);

/// Recoil push stage `func_actor_110600_80137AF4` indexes for the speed it
/// moves the actor by, and bumps once that push has landed. Reset to 0 first,
/// so the push only starts on the frame a live actor arrives.
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    s16 value;
    u8  retained[6];
} Actor110600Storage8688;
STATIC_ASSERT_SIZEOF(Actor110600Storage8688, 8);

extern Actor110600Storage8688 D_actor_110600_80148688;

/// Argument record `func_actor_110600_80135E20` fills for `func_800FDB18`:
/// model part 1's coordinate, scale 0x100 and count 3.
extern EffectSpawnArg D_actor_110600_80148698;

/// `Task::exitCallback` installed by the spawn handler: bump the two helper
/// tasks' `state` if present, unlink the three display nodes, drop the enemy's
/// `recs` slot, clear the screen shake, then `enemyDestroy`.
static void func_actor_110600_801387F4(Task* task);

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

/// Applies a rotation of `angle` about Y to `matrix` (main executable).

/// The complaint the route re-plan prints when the two node lists share no
/// slot at all. The string is spelled out rather than left a literal so the
/// re-plan reaches it by name, the way the original object does.
static const char _gPatrolNoPairMsg[] = "s->root_cnt == 0xff about \n";

// Message-table callbacks use the argument views required by this TU.

s32 func_actor_110600_80133E48(Task* task, s32 msgId, ActorTransform* placement, s32 arg3);
s32 func_actor_110600_80134040(Task*, s32, ActorCommand*, s32);
s32 func_actor_110600_8013839C(Task*, s32, AnimationPlayRequest*, s32);
s32 func_actor_110600_80138448(Task*, s32, s32, s32);
s32 func_actor_110600_80138538(Task*, s32, s32, s32);
s32 func_actor_110600_801387C0(Task*, s32, s32, s32);
s32 func_actor_110600_80138394(Task*, s32, s32, s32);

static TmdSource _gActor110600StrangerBody;
void             func_actor_110600_80138EA8(Task*);

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
    { 2015, func_actor_110600_80138394 },
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_110600_8013839C },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_110600_80138448 },
    { ACTOR_MESSAGE_IS_PRESENT, func_actor_110600_80138538 },
    { ACTOR_MESSAGE_PLACE, func_actor_110600_80133E48 },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_110600_80134040 },
    { 2007, func_actor_110600_801387C0 },
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

TaskDesc D_actor_110600_80148670 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, func_actor_110600_80138EA8, { .model = &_gActor110600StrangerBody } };

TaskDesc D_actor_110600_8014867C = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, func_actor_110600_80138EA8, { .model = &_gActor110600StrangerBody } };

Actor110600Storage8688 D_actor_110600_80148688 = { 0, { 0, 0, 0, 0, 0, 0 } };

SVECTOR ActorContact_ScratchPosition = { 0, 0, 0, 0 };

/// The contact routines' scratch position.
static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

EffectSpawnArg D_actor_110600_80148698 = { NULL, 0, 0 };

/// Whole-unit step `ActorContact_PushContact` last applied to its coordinate.
extern SVECTOR ActorContact_ScratchPosition;

/// Reset argument `animationSeekSlotWithBlend` is handed for the clip `animId` of the
/// `appliedAnim` stage: the `0x2D`-byte row of the animation table this overlay's
/// data carries at `D_actor_110600_80147D20`, indexed by the clip id. The row
/// stride is the row's own length, so the load is a signed byte.
extern s8 D_actor_110600_80147D20[][0x2D];

extern EnemyParams D_actor_110600_80138F14;

extern AnimationSet* D_actor_110600_8014850C[];

extern TaskMessageEntry D_actor_110600_80148624[7];

/// Per-frame step the tick hands off to once the `hitCooldown` countdown reaches
/// zero.
static void func_actor_110600_80136210(Task* arg0);

static void            func_actor_110600_80133778(BossStrangerWalker* work, s16 scale, s16 angle);
static __inline__ void Actor110600_ScaleRotation(Task* task, s16 scale);
static void            func_actor_110600_80134438(Task* arg0);
static __inline__ void Actor110600_InitBodyObj(WorldCollisionBody* obj, GfxCoord* coord, WorldCollisionContact* recs, SVECTOR* pos, s16 enabled);
static __inline__ void Actor110600_InitScale(BossStrangerWalker* walker);
static void            func_actor_110600_80134AB4(Enemy* enemy, Task* task);
static void            func_actor_110600_80135194(Task* arg0);
static __inline__ s16  Actor110600_WrapHitAngle(s16 angle);
static __inline__ s32  Actor110600_TickShake(void);
static __inline__ s32  Actor110600_HasRec10000(WorldCollisionContact* recs);
static void            func_actor_110600_80135B84(Task* arg0);
static __inline__ s32  Actor110600_FindHit(SVECTOR* point, WorldCollisionContact* recs, s16 count);
static void            func_actor_110600_80136888(Task* arg0);
static void            func_actor_110600_801369D8(Task* arg0);
static __inline__ void Actor110600_ApplyShrink(Task* arg0, _Actor110600Work* work, s16 y);
static void            func_actor_110600_80136ECC(Task* arg0);
static __inline__ void Actor110600_RescaleRoot(Task* arg0, s16 scale);
static void            func_actor_110600_80137684(Task* arg0);
static void            func_actor_110600_80137980(Task* arg0);
static void            func_actor_110600_80137AF4(Task* arg0);
static void            func_actor_110600_80137DB0(Task* arg0);
static void            func_actor_110600_80137F2C(Enemy* arg0, Task* arg1);

#include "../../shared/actor_contacts_turn_joint.inc.c"

#include "../../shared/actor_contacts_push_contact.inc.c"

#include "../../shared/boss_stranger_arrived.inc.c"

#include "../../shared/boss_stranger_follow_route.inc.c"

#include "../../shared/boss_stranger_nearest_actor.inc.c"

#include "../../shared/boss_stranger_nearest_self.inc.c"

#include "../../shared/boss_stranger_plan_toward.inc.c"

#include "../../shared/boss_stranger_ground_step.inc.c"

#include "../../shared/boss_stranger_avoid_contacts.inc.c"

#include "../../shared/boss_stranger_turn_toward.inc.c"

/// Debug rebuild of the walker's patrol table. Node 0 takes the walker's own
/// coordinate translation; every node above it takes that translation plus the
/// coordinate's facing column, rotated to `angle` and scaled by `scale` through
/// the GTE, and each node laid is logged as it is built. Each `nodeOrder`
/// entry is set to its own index. The route is then re-seeded from `nodeCount`
/// -- one node index per step with the `OVERLAY_WALKER_ROUTE_END` marker after
/// the last -- with the route's `field_4` and `cursor` cleared, and the scratch
/// frame released.
static void func_actor_110600_80133778(BossStrangerWalker* work, s16 scale, s16 angle)
{
    Actor110600TsvScratch* blk;
    u8*                    head;

    if (work->nav->nodeCount < 2)
        return;
    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - 0x2C;
    blk                      = (Actor110600TsvScratch*)(head - 0x2C);
    work->nav->nodes[0].x    = (u16)work->coord->coord.t[0];
    work->nav->nodes[0].y    = (u16)work->coord->coord.t[1];
    work->nav->nodes[0].z    = (u16)work->coord->coord.t[2];
    work->nav->nodeOrder[0]  = 0;
    blk->m                   = work->coord->coord;
    for (blk->i = 1; blk->i < work->nav->nodeCount; blk->i++) {
        gfxRotMatrixY(&blk->m, angle, 0);
        gfxReadMatrixZAxis(&blk->m, &blk->v);
        gte_lddp(scale);
        gte_ldsv(&blk->v);
        gte_gpf12();
        gte_stsv(&blk->v);
        work->nav->nodes[blk->i].x   = (u16)work->coord->coord.t[0] + (u16)blk->v.vx;
        work->nav->nodes[blk->i].y   = (u16)work->coord->coord.t[1] + (u16)blk->v.vy;
        work->nav->nodes[blk->i].z   = (u16)work->coord->coord.t[2] + (u16)blk->v.vz;
        work->nav->nodeOrder[blk->i] = blk->i;
        printf("emc_m->tsv[%d]( %d, %d, %d )\n", blk->i, work->nav->nodes[blk->i].x, work->nav->nodes[blk->i].y, work->nav->nodes[blk->i].z);
    }
    work->route->field_4 = 0;
    work->route->cursor  = 0;
    for (blk->i = 0; blk->i < work->nav->nodeCount; blk->i++) {
        work->route->nodeIndices[blk->i] = blk->i;
    }
    work->route->nodeIndices[blk->i] = OVERLAY_WALKER_ROUTE_END;
    SCRATCH_STACK_RELEASE_BYTES(0x2C);
}

#include "../../shared/boss_stranger_inlines.inc.c"

#include "../../shared/boss_stranger_tick.inc.c"

/// Rebuilds the model's root coordinate around the yaw it already faces and
/// rescales it uniformly: `ratan2` of the rotation's Z basis gives the yaw,
/// `gfxRotMatrixY` rebuilds the rotation from it and `ScaleMatrix` applies
/// `scale` on all three axes. The working matrix lives in a frame carved off
/// the scratch stack, which is handed back once the rotation has been copied
/// onto the coordinate. Written as an inline so the four scratch-head accesses
/// stay absolute; see `Actor444000_ShrinkRotation` in `actor_444000_5.c`.
static __inline__ void Actor110600_ScaleRotation(Task* task, s16 scale)
{
    ActorScaleRotScratch* blk;
    GfxCoord*             coord;
    u8*                   head;
    s16                   ang;
    u16                   m22;

    head                                       = SCRATCH_STACK_CURSOR(u8);
    coord                                      = task->extra.tmd->coords;
    blk                                        = (ActorScaleRotScratch*)(head - sizeof(ActorScaleRotScratch));
    SCRATCH_STACK_CURSOR(ActorScaleRotScratch) = blk;

    ang      = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->yaw = ang;
    gfxRotMatrixY(&blk->rotation, ang, 1);
    blk->scale.vz = scale;
    blk->scale.vy = scale;
    blk->scale.vx = scale;
    ScaleMatrix(&blk->rotation, &blk->scale);

    coord->coord.m[0][0]                  = (u16)((ActorScaleRotScratch*)(head - sizeof(ActorScaleRotScratch)))->rotation.m[0][0];
    coord->coord.m[0][1]                  = (u16)blk->rotation.m[0][1];
    coord->coord.m[0][2]                  = (u16)blk->rotation.m[0][2];
    coord->coord.m[1][0]                  = (u16)blk->rotation.m[1][0];
    coord->coord.m[1][1]                  = (u16)blk->rotation.m[1][1];
    coord->coord.m[1][2]                  = (u16)blk->rotation.m[1][2];
    coord->coord.m[2][0]                  = (u16)blk->rotation.m[2][0];
    coord->coord.m[2][1]                  = (u16)blk->rotation.m[2][1];
    m22                                   = (u16)blk->rotation.m[2][2];
    coord->composeStamp                   = GRAPHICS_COORD_DIRTY;
    coord->coord.m[2][2]                  = m22;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BYTES(sizeof(ActorScaleRotScratch));
}

/// Placement opcode: drops the model's root coordinate onto `placement` (the
/// three longs become its translation, the Euler angles go through
/// `gfxRotMatrixX`, `gfxRotMatrixY` and `gfxRotMatrixZ`), then rebuilds and rescales that coordinate
/// from the actor's own heading and caches the resulting yaw in the work
/// block's `placedYaw`. The rescale `coordSetYawScale` performs is
/// inlined behind the placement.
s32 func_actor_110600_80133E48(Task* task, s32 arg1, ActorTransform* placement, s32 arg3)
{
    _Actor110600Work* work;

    work = task->work;

    task->extra.tmd->coords->coord.t[0] = placement->pos.vx;
    task->extra.tmd->coords->coord.t[1] = placement->pos.vy;
    task->extra.tmd->coords->coord.t[2] = placement->pos.vz;
    gfxRotMatrixX(&task->extra.tmd->coords->coord, placement->rot.vx, GRAPHICS_ROTATION_REPLACE);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, placement->rot.vy, 0);
    gfxRotMatrixZ(&task->extra.tmd->coords->coord, placement->rot.vz, GRAPHICS_ROTATION_COMPOSE);
    Actor110600_ScaleRotation(task, work->walker.scale);
    work->placedYaw = ratan2(-task->extra.tmd->coords->coord.m[2][0],
                             task->extra.tmd->coords->coord.m[2][2]);
    return 1;
}

/// Actor-command handler: records the command's stage, area and low command
/// byte in the work block's `lastCommand`, then dispatches on its context. The
/// patio's context (0x301) with command 1 enters state 0x14; the cafeteria's
/// (0x401) picks a display slot and a `animId` state per command — 1, 8 and
/// 9 only set the state, and 9 shares its tail with the five commands that
/// repoint a slot — parking the actor in state 0x11 with `prevState` cleared.
/// Written with the share as a `goto` because the commands fall through into
/// it from case 9. `arg1` is unused; it exists because the dispatch passes
/// three arguments.
s32 func_actor_110600_80134040(Task* arg0, s32 arg1, ActorCommand* arg2, s32 arg3)
{
    _Actor110600Work* work = arg0->work;

    work->lastCommand.stage   = arg2->context.loc.stage;
    work->lastCommand.area    = arg2->context.loc.area;
    work->lastCommand.command = arg2->command;
    if (arg2->context.key == 0x301) {
        if (arg2->command == 1) {
            work->state = ACTOR_110600_STATE_LURK;
            return 1;
        }
        return 0;
    }
    if (arg2->context.key == 0x401) {
        switch (arg2->command) {
            default:
                return 0;
            case 1:
                work->state     = ACTOR_110600_STATE_ALERT_REWIND;
                work->prevState = -1;
                return 1;
            case 2:
                work->animId            = 0x23;
                D_actor_110600_80148598 = &gActor210600Animation11F5C;
                goto state_11;
            case 3:
                work->animId            = 0x24;
                D_actor_110600_8014859C = &gActor210600Animation11F5C;
                goto state_11;
            case 5:
                work->animId            = 0x22;
                D_actor_110600_80148594 = &gActor210600Animation12B30;
                goto state_11;
            case 6:
                work->animId            = 0x23;
                D_actor_110600_80148598 = &gActor210600Animation134C8;
                goto state_11;
            case 4:
            case 7:
                work->animId            = 0x25;
                D_actor_110600_801485A0 = &gActor210600Animation12244;
                goto state_11;
            case 8:
                work->state     = ACTOR_110600_STATE_DEATH_BURN;
                work->prevState = -1;
                return 1;
            case 9:
                work->animId = 0x11;
            state_11:
                work->state     = ACTOR_110600_STATE_SCRIPTED;
                work->prevState = -1;
                return 1;
        }
    }
    return 0;
}

#include "../../shared/player_detection_reach.inc.c"

/// Per-tick animation pass: for each clip id 1..0x12, the first ten (`i < 0xB`)
/// seed their slot's `rate` from the two work bytes and tick the primary and
/// blend contexts through `animationTickSlotPose`, then hand both poses to
/// `Gp_AnimWritePoseCopy` with `blendWeight` and its complement; the rest
/// only rewrite the primary slot and `animationTickSlot` it. Same body as
/// `func_actor_403000_801336B4`, which walks 24 slots instead of 19.
static void func_actor_110600_80134438(Task* arg0)
{
    AnimationPose     pose;
    AnimationPose     blendPose;
    AnimationContext* anim;
    s16               weight;
    s16               i;
    _Actor110600Work* work;

    work   = arg0->work;
    weight = work->blendWeight;
    anim   = &work->rig.anim;
    for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
        if (i < 0xB) {
            work->blendRig.slots[i].rate = work->blendRate;
            work->rig.slots[i].rate      = (work->animRate - 3);
            animationTickSlotPose(anim, i, &pose, 0);
            animationTickSlotPose(&work->blendRig.anim, i, &blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->rig.slots[i].rate = (work->animRate - 3);
            animationTickSlot(&work->rig.anim, i);
        }
    }
}

/// Sound cue the pose the model has reached has earned this tick, 0 for none:
/// each state watches one clip id of the animation slot `animId` selects and
/// reports its `0x401D00NN` cue the first time that id is held, remembering it
/// in `lastCueIndex` so the cue is not repeated. State 3 reads slots 14 and 18
/// (cues 4 and 3, the second only while `lastCueIndex` is not already 0xFC);
/// states 2, 21, 4 and 5 read slot 1, with state 2 the only one watching two
/// ids (cues 2 and 1) and clearing the memory when neither is held. Every other
/// way out re-reads the slot-1 pose into `lastCueIndex`.
static s32 func_actor_110600_80134564(_Actor110600Work* work)
{
    s32 id14;
    s32 id18;
    s32 id2;
    s32 id21;
    s32 id4;
    s32 id5;
    s32 prev;
    s16 state;

    state = (u16)work->animId - 2;
    switch (state) {
        case 1:
            id14 = work->rig.slots[14].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (id14 == 0xC5) {
                prev = work->lastCueIndex;
                if (prev != id14) {
                    work->lastCueIndex = id14;
                    return 0x401D0004;
                }
                work->lastCueIndex = prev;
                return 0;
            }
            id18 = work->rig.slots[18].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (id18 == 0xFD) {
                if (work->lastCueIndex != 0xFC) {
                    work->lastCueIndex = id18;
                    return 0x401D0003;
                }
                work->lastCueIndex = id18;
                break;
            }
            work->lastCueIndex = 0;
            break;
        case 0:
            id2 = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (id2 == 0x33) {
                if (work->lastCueIndex != id2) {
                    work->lastCueIndex = id2;
                    return 0x401D0002;
                }
                work->lastCueIndex = id2;
            } else if (id2 == 0x26) {
                prev = work->lastCueIndex;
                if (prev != id2) {
                    work->lastCueIndex = id2;
                    return 0x401D0001;
                }
                work->lastCueIndex = prev;
            } else {
                work->lastCueIndex = 0;
            }
            break;
        case 19:
            id21 = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (id21 == 4 && work->lastCueIndex != id21) {
                work->lastCueIndex = id21;
                return 0x401D0006;
            }
            work->lastCueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            break;
        case 2:
            id4 = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (id4 == 9 && work->lastCueIndex != id4) {
                work->lastCueIndex = id4;
                return 0x401D000C;
            }
            work->lastCueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            break;
        case 3:
            id5 = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (id5 == 0xB && work->lastCueIndex != id5) {
                work->lastCueIndex = id5;
                return 0x401D000C;
            }
            work->lastCueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            break;
    }
    return 0;
}

/// Per-tick animation stage machine driven off the task's work block.
///
/// Stages 1, 2 and 6 arm every slot 1..0x12 and then park the stage at 3 with
/// the frame counter `animFrames` and `lastCueIndex` cleared: stage 1 plays the
/// clip at `animId` through `animationSeekSlotWithBlend`, taking each slot's reset
/// argument out of the `appliedAnim` row of `D_actor_110600_80147D20`; stage 2
/// arms the same clip with `animationResetSlot`; stage 6 arms clip 0x10 and then
/// ticks the context 99 times so the pose settles before it is shown. A
/// `blendRequest` of 2 is the blend stage instead — it arms the blend context with
/// clip `blendAnimId` at weight `blendWeight` and parks `blendRequest` at 3.
///
/// Every tick then steps all slots, either directly or — while `blendActive` is
/// set — through the blend pass `func_actor_110600_80134438`, which ends the
/// wait once blend slot 1 reports its clamp. `lookYaw` walks towards
/// `lookYawTarget` in 0x100 steps, and the turn that leaves, clamped to ±0x400, is
/// handed to joints 5 and 3 of the model root (the second a quarter of it).
/// Finally the id `func_actor_110600_80134564` reports is queued through
/// `SndEvt_EnqueueType6` with the model root's pan and depth; the bits 12..15
/// of the enemy's `field_8` are appended to it.
static void func_actor_110600_80134728(Task* arg0)
{
    _Actor110600Work* work;
    _Actor110600Work* seekWork;
    _Actor110600Work* resetWork;
    _Actor110600Work* warmWork;
    _Actor110600Work* blendWork;
    _Actor110600Work* tickWork;
    Enemy*            enemy;
    s32               animation;
    s32               seekIndex;
    s32               resetIndex;
    s32               warmIndex;
    s32               blendIndex;
    s32               tickIndex;
    s32               targetAngle;
    s32               currentAngle;
    s32               targetAngleBits;
    s32               currentAngleBits;
    s32               turn;
    s16               turnNow;
    s32               sound;
    s32               soundId;
    s32               pan;
    s16               state;

    work  = arg0->work;
    state = work->animRequest;
    enemy = arg0->spawnArg2.pointer;
    if (state == ACTOR_110600_ANIM_REQUEST_BLEND) {
        seekWork  = work;
        seekIndex = 1;
        do {
            work->rig.slots[seekIndex].rate = seekWork->animRate;
            animation                       = seekWork->animId;
            animationSeekSlotWithBlend(&seekWork->rig.anim, seekIndex, animation, 0, D_actor_110600_80147D20[seekWork->appliedAnim][animation]);
            seekIndex += 1;
        } while (seekIndex < ARRAY_SIZE(work->rig.slots));
        seekWork->appliedAnim = (u16)seekWork->animId;
        work->animRequest     = ACTOR_110600_ANIM_REQUEST_PLAYING;
        work->animFrames      = 0;
        work->lastCueIndex    = 0;
    } else if (state == ACTOR_110600_ANIM_REQUEST_RESET) {
        resetWork  = work;
        resetIndex = 1;
        do {
            work->rig.slots[resetIndex].rate = resetWork->animRate;
            animationResetSlot(&resetWork->rig.anim, resetIndex, resetWork->animId);
            resetIndex += 1;
        } while (resetIndex < ARRAY_SIZE(work->rig.slots));
        resetWork->appliedAnim = (u16)resetWork->animId;
        work->animRequest      = ACTOR_110600_ANIM_REQUEST_PLAYING;
        work->animFrames       = 0;
        work->lastCueIndex     = 0;
    } else if (state == ACTOR_110600_ANIM_REQUEST_SETTLE) {
        warmWork  = work;
        warmIndex = 1;
        do {
            work->rig.slots[warmIndex].rate = ANIMATION_RATE_ONE;
            animationResetSlot(&warmWork->rig.anim, warmIndex, warmWork->animId);
            warmIndex += 1;
        } while (warmIndex < ARRAY_SIZE(work->rig.slots));
        warmWork->appliedAnim = (u16)warmWork->animId;
        warmIndex             = 1;
        do {
            tickWork  = arg0->work;
            tickIndex = 1;
            do {
                tickWork->rig.slots[tickIndex].rate = tickWork->animRate;
                animationTickSlot(&tickWork->rig.anim, tickIndex);
                tickIndex += 1;
            } while (tickIndex < ARRAY_SIZE(work->rig.slots));
            warmIndex += 1;
        } while (warmIndex < 0x64);
        work->animRequest  = ACTOR_110600_ANIM_REQUEST_PLAYING;
        work->animFrames   = 0;
        work->lastCueIndex = 0;
    }
    if (work->blendRequest == ACTOR_110600_ANIM_REQUEST_RESET) {
        blendWork              = arg0->work;
        blendIndex             = 1;
        blendWork->blendRate   = 0x30;
        blendWork->blendWeight = 0xB78;
        do {
            // The rate goes to the main rig's slot, which the tick below
            // overwrites; the blend slots take theirs in the blend pass.
            blendWork->rig.slots[blendIndex].rate = blendWork->blendRate;
            animationResetSlot(&blendWork->blendRig.anim, blendIndex, blendWork->blendAnimId);
            blendIndex += 1;
        } while (blendIndex < ARRAY_SIZE(work->rig.slots));
        work->blendRequest = ACTOR_110600_ANIM_REQUEST_PLAYING;
    }
    work->animFrames = (u16)(work->animFrames + 1);
    if (work->blendActive == 0) {
        tickWork  = arg0->work;
        tickIndex = 1;
        do {
            tickWork->rig.slots[tickIndex].rate = tickWork->animRate;
            animationTickSlot(&tickWork->rig.anim, tickIndex);
            tickIndex += 1;
        } while (tickIndex < ARRAY_SIZE(work->rig.slots));
    } else {
        func_actor_110600_80134438(arg0);
        if (work->blendRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
            work->blendActive = 0;
        }
    }
    targetAngle      = work->lookYawTarget;
    currentAngle     = work->lookYaw;
    targetAngleBits  = (u16)work->lookYawTarget;
    currentAngleBits = (u16)work->lookYaw;
    if (currentAngle < targetAngle) {
        if ((targetAngle - currentAngle) >= 0x101) {
            work->lookYaw = (s16)(currentAngleBits + 0x100);
        } else {
            goto block_31;
        }
    } else if ((currentAngle - targetAngle) >= 0x101) {
        work->lookYaw = (s16)(currentAngleBits - 0x100);
    } else {
    block_31:
        work->lookYaw = (s16)targetAngleBits;
    }
    turnNow = work->lookYaw;
    turn    = (u16)work->lookYaw;
    if (turnNow != 0) {
        if (turnNow >= 0x401) {
            turn = 0x400;
        }
        if (turnNow < -0x400) {
            turn = -0x400;
        }
        ActorContact_TurnJoint(&arg0->extra.tmd->coords[5], (s16)turn);
        ActorContact_TurnJoint(&arg0->extra.tmd->coords[3], (s16)((s32)(turn << 0x10) >> 0x12));
    }
    sound = func_actor_110600_80134564(work);
    if (sound != 0) {
        soundId = sound | ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s32)(s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
}

static __inline__ void Actor110600_InitBodyObj(WorldCollisionBody* obj, GfxCoord* coord, WorldCollisionContact* recs, SVECTOR* pos, s16 enabled)
{
    obj->context.contacts = recs;
    obj->coord            = coord;
    obj->pos.vx           = (u16)pos->vx;
    obj->pos.vy           = (u16)pos->vy;
    obj->pos.vz           = (u16)pos->vz;
    obj->radius           = 0x200;
    obj->flags            = enabled;
    Gp_LinkObj(3, obj);
}

static __inline__ void Actor110600_InitScale(BossStrangerWalker* walker)
{
    VECTOR *head, *scale;
    s32     amount;
    head                         = SCRATCH_STACK_CURSOR(VECTOR);
    scale                        = head - 1;
    SCRATCH_STACK_CURSOR(VECTOR) = scale;
    walker->scaleMtx.m[0][0] = walker->scaleMtx.m[1][1] = walker->scaleMtx.m[2][2] = 0x1000;
    walker->scaleMtx.m[0][1] = walker->scaleMtx.m[0][2] = walker->scaleMtx.m[1][0] = walker->scaleMtx.m[1][2] = walker->scaleMtx.m[2][0] = walker->scaleMtx.m[2][1] = 0;
    walker->scaleMtx.t[0] = walker->scaleMtx.t[1] = walker->scaleMtx.t[2] = 0;
    amount                                                                = walker->scale;
    if (amount != 0 && amount != 0x1000) {
        scale->vx = scale->vy = scale->vz = amount;
        ScaleMatrix(&walker->scaleMtx, scale);
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

static void func_actor_110600_80134AB4(Enemy* enemy, Task* task)
{
    SVECTOR                pos;
    VECTOR                 world;
    WorldCollisionContact* savedRecs;
    WorldCollisionBody*    obj;
    WorldCollisionBody*    bodyObj;
    WorldCollisionContact* contactRecs;
    WorldCollisionContact* walkRecs;
    GfxCoord*              coord;
    TmdObject*             model;
    s16                    enabled;
    u32                    placement;
    TmdObject*             boundModel;
    _Actor110600Work*      work;
    _Actor110600Work*      boundWork;

    model = task->extra.tmd;
    coord = model->coords;
    if (((task->spawnArg1.value >> 16) & 0xF) != 2) {
        model->flags = 0;
        Tmd_AllocBuffers(model);
    }
    work       = memCalloc(sizeof(_Actor110600Work), false);
    task->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback   = func_actor_110600_801387F4;
    boundWork            = task->work;
    boundModel           = task->extra.tmd;
    boundModel->lightMtx = &boundWork->lightMtx;
    boundModel->colorMtx = &boundWork->colorMtx;
    enemy->field_4       = &coord->coord;
    enemy->field_48      = 0;
    enemy->bodyPos.vx    = 0;
    enemy->bodyPos.vy    = 0;
    enemy->bodyPos.vz    = 0;
    enemy->coord         = task->extra.tmd->coords + 3;
    Gp_LinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->reactionFlags          = 0;
    enemy->param                  = &D_actor_110600_80138F14;
    enemy->hpMax = enemy->hp = D_actor_110600_80138F14.hpMax;
    contactRecs              = work->hitContacts;
    enemy->recs              = contactRecs;
    animationInitContext(&work->rig.anim, D_actor_110600_8014850C, model, work->rig.poses, work->rig.slots);
    animationInitContext(&work->blendRig.anim, D_actor_110600_8014850C, model, work->blendRig.poses, work->blendRig.slots);
    work->animRequest   = ACTOR_110600_ANIM_REQUEST_RESET;
    work->blendActive   = 0;
    work->animId        = 2;
    work->lookYaw       = 0;
    work->lookYawTarget = 0;
    func_actor_110600_80134728(task);
    walkRecs                        = work->gridContacts;
    work->enrageTint                = 0;
    work->enraged                   = 0;
    work->gridBody.coord            = &gGfxViewCoord;
    savedRecs                       = walkRecs;
    work->gridBody.context.contacts = walkRecs;
    work->gridBody.pos.vx           = (u16)task->extra.tmd->coords->coord.t[0];
    work->gridBody.pos.vy           = (s16)((u16)task->extra.tmd->coords->coord.t[1] - 0x124);
    work->gridBody.pos.vz           = (u16)task->extra.tmd->coords->coord.t[2];
    work->gridBody.key              = 0x3000D;
    work->gridBody.radius           = 0x1A4;
    work->gridBody.flags = enabled = 1;
    Gp_LinkObj(2, &work->gridBody);
    work->gridBody.flags = (u16)(work->gridBody.flags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    Gp_InitRec18Table(work->gridBody.context.contacts, ARRAY_SIZE(work->gridContacts), 0);
    obj                   = &work->hitBody;
    obj->coord            = task->extra.tmd->coords;
    obj->context.contacts = contactRecs;
    obj->pos.vx           = 0;
    obj->pos.vy           = 0;
    obj->pos.vz           = 0;
    obj->key              = 0x30000;
    obj->radius           = 0x14A;
    obj->flags            = enabled;
    Gp_LinkObj(2, obj);
    obj->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_InitRec18Table(obj->context.contacts, ARRAY_SIZE(work->hitContacts), 0);
    work->hitBody.pos.vy = -0x2BC;
    bodyObj              = &work->attackBody;
    work->hitBody.key    = 0x30000;
    work->hitBody.pos.vx = 0;
    work->hitBody.pos.vz = 0x190;
    pos.vx               = 0;
    pos.vy               = 0;
    pos.vz               = 0;
    Actor110600_InitBodyObj(bodyObj, task->extra.tmd->coords + 3, work->attackContacts, &pos, enabled);
    Gp_InitRec18Table(bodyObj->context.contacts, ARRAY_SIZE(work->attackContacts), 0);
    task->msgTable      = D_actor_110600_80148624;
    work->childTask0    = 0;
    work->childTask1    = 0;
    work->field_8B0     = 0;
    work->rootDirty     = enabled;
    coord->parent       = &gGfxViewCoord;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    world.vx = coord->workm.t[0];
    world.vy = coord->workm.t[1];
    world.vz = coord->workm.t[2];
    func_800D7A9C(task->extra.tmd, &world, 0, 3);
    work->walker.coord      = coord;
    work->walker.recs       = savedRecs;
    work->walker.recCount   = ARRAY_SIZE(work->gridContacts);
    work->walker.avoidCount = ARRAY_SIZE(work->hitContacts);
    work->walker.state      = BOSS_STRANGER_WALKER_IDLE;
    work->walker.avoidRecs  = contactRecs;
    work->walker.scale      = 0;
    work->walker.turnLimit  = 0x20;
    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(1, 3, 0, 0)) {
        work->walker.lockHeight = 0;
    } else {
        work->walker.lockHeight = enabled;
    }
    work->walker.skipGround            = 0;
    work->walker.skipAvoid             = 1;
    work->walker.playerId              = (u8)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId;
    work->walker.nav                   = &work->walker.navData;
    work->walker.route                 = &work->walker.routeData;
    work->walker.navData.nodeCount     = ARRAY_SIZE(work->navNodes);
    work->walker.navData.orderCount    = 2;
    work->walker.routeData.field_4     = 2;
    work->walker.navData.nodes         = work->navNodes;
    work->walker.navData.nodeOrder     = work->navNodeOrder;
    work->walker.routeData.nodeIndices = work->routeNodeIndices;
    work->walker.playerId              = (u8)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId;
    switch (task->spawnArg1.value & 0xF0) {
        case 0:
            work->animRate         = 20;
            work->baseRate         = 20;
            work->walker.scale     = 0x1000;
            work->walkSpeed        = 10;
            work->flinchSpeedScale = 75;
            break;
        case 0x10:
            work->animRate         = 16;
            work->baseRate         = 16;
            work->walker.scale     = 4500;
            work->walkSpeed        = 8;
            work->flinchSpeedScale = 66;
            break;
        case 0x20:
            work->animRate         = 16;
            work->baseRate         = 16;
            work->walker.scale     = 6500;
            work->walkSpeed        = 14;
            work->flinchSpeedScale = 63;
            break;
        default:
            work->animRate     = 20;
            work->walker.scale = 0x1000;
            break;
    }
    placement = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    if ((s16)(placement & 1) == 1) {
        work->animRate += placement >> 1;
    } else {
        work->animRate -= placement >> 1;
    }
    switch (task->spawnArg1.value & 0xF00) {
        case 0:
            work->noticeRangeAhead  = 3000;
            work->noticeRangeAround = 500;
            break;
        case 0x100:
            work->noticeRangeAhead  = 4000;
            work->noticeRangeAround = 1000;
            break;
        case 0x200:
            work->noticeRangeAhead  = 6000;
            work->noticeRangeAround = 2000;
            break;
        default:
            work->noticeRangeAhead  = 8000;
            work->noticeRangeAround = 2000;
            break;
    }
    switch (task->spawnArg1.value & 0xF000) {
        case 0:
            func_actor_110600_80133778(&work->walker, 1500, 0x764);
            break;
        case 0x1000:
            func_actor_110600_80133778(&work->walker, 3000, 0x764);
            break;
        case 0x2000:
            func_actor_110600_80133778(&work->walker, 4000, 0x764);
            break;
        default:
            func_actor_110600_80133778(&work->walker, 5000, 0x764);
            break;
    }
    switch ((task->spawnArg1.value >> 16) & 0xF) {
        case 1:
            if (enemy->spawnState == 0) {
                work->state = ACTOR_110600_STATE_HIDDEN;
            } else {
                work->state = ACTOR_110600_STATE_RESTORED;
            }
            break;
        case 2:
            work->state = ACTOR_110600_STATE_HIDDEN;
            break;
        case 3:
            break;
        case 4:
            work->state = ACTOR_110600_STATE_IDLE;
            break;
        case 0:
        default:
            if (enemy->spawnState == 0) {
                work->state = ACTOR_110600_STATE_PATROL;
            } else {
                work->state = ACTOR_110600_STATE_RESTORED;
            }
            break;
    }
    Actor110600_InitScale(&work->walker);
    work->prevState = -1;
    task->state    += 1;
}

/// Aiming stage: re-arms the aim on a live actor — clear the model object, drop
/// bit 0x8000 of `attackBody.flags` and set 0x4000 of `gridBody.flags`, tag the
/// enemy's link node, reload `animRate` from `baseRate`, park the stage at 2
/// (`animRequest` / `animId`) and the walker at state 3 with its turn limit at
/// 0x10. The aim itself is one bearing: the yaw of the player delta from
/// the model's root coordinate, minus that coordinate's own yaw, wrapped into
/// [-0x800, 0x800]. While it is under 0x3E8 and again unconditionally, the XZ
/// delta is measured against the `noticeRangeAhead` / `noticeRangeAround` ranges, and falling
/// inside either moves the actor to state 4. Every tick the walker is stepped
/// first and the model ticked last.
static void func_actor_110600_80135194(Task* arg0)
{
    _Actor110600Work* work;
    TmdObject*        obj;
    Enemy*            enemy;
    GfxCoord*         coord;
    GfxCoord*         facing;
    SVECTOR           delta;
    SVECTOR*          d;
    s16               angle;

    work  = arg0->work;
    obj   = arg0->extra.tmd;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj->flags                    = 0;
        work->animRate                = work->baseRate;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->animRequest             = ACTOR_110600_ANIM_REQUEST_RESET;
        work->animId                  = 2;
        work->walker.state            = BOSS_STRANGER_WALKER_PATROL;
        work->walker.turnLimit        = 0x10;
    }
    work->walker.speed = work->walkSpeed;
    bossStrangerTick(&work->walker);
    coord    = arg0->extra.tmd->coords;
    d        = &delta;
    delta.vx = (u16)gPlayerStatus.coordMtx->t[0] - (u16)coord->coord.t[0];
    d->vy    = (u16)gPlayerStatus.coordMtx->t[1] - (u16)coord->coord.t[1];
    d->vz    = (u16)gPlayerStatus.coordMtx->t[2] - (u16)coord->coord.t[2];
    facing   = arg0->extra.tmd->coords;
    angle    = ratan2(delta.vx, d->vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
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
    if (abs(angle) < 0x3E8) {
        if (actorOutsideRadius(&delta, work->noticeRangeAhead) == 0)
            work->state = ACTOR_110600_STATE_ALERT;
    }
    if (actorOutsideRadius(&delta, work->noticeRangeAround) == 0)
        work->state = ACTOR_110600_STATE_ALERT;
    func_actor_110600_80134728(arg0);
}

static __inline__ s16 Actor110600_WrapHitAngle(s16 angle)
{
    if (angle < 0) {
    neg:
        if (angle < -0x800) {
            angle += 0x1000;
            goto neg;
        }
    } else {
    pos:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto pos;
        }
    }
    return angle;
}

static __inline__ s32 Actor110600_TickShake(void)
{
    D_actor_110600_8014865C++;
    if (D_actor_110600_8014865C == 5)
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

static void func_actor_110600_80135454(Task* arg0)
{
    _Actor110600Work*   work;
    TmdObject*          obj;
    Enemy*              enemy;
    GfxCoord*           coord;
    GfxCoord*           facing;
    BossStrangerWalker* walker;
    SVECTOR             delta;
    SVECTOR*            d;
    s16                 angle;
    u16                 ramp;
    s32                 pose;
    s32                 nextPose;

    work  = arg0->work;
    obj   = arg0->extra.tmd;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj->flags                    = 0;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->animRequest             = ACTOR_110600_ANIM_REQUEST_BLEND;
        work->animId                  = 2;
        work->walker.state            = BOSS_STRANGER_WALKER_CHASE;
        work->animRate                = work->baseRate;
        if (work->baseRate == 0x38)
            work->walker.turnLimit = 0x30;
        work->walker.turnLimit = 0x1C;
        work->stateFrame       = 0;
    }
    if (work->blendActive == 0) {
        work->walker.speed = work->walkSpeed * work->animRate / 16;
        if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x1A4, (s16)work->walker.speed) == 0)
            work->walker.speed = 0;
    } else {
        work->walker.speed = (u16)(work->flinchSpeedScale * work->animRate / 1520) / 2;
        if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x1A4, (s16)work->walker.speed) == 0)
            work->walker.speed = 0;
    }
    coord    = arg0->extra.tmd->coords;
    d        = &delta;
    delta.vx = (u16)gPlayerStatus.coordMtx->t[0] - (u16)coord->coord.t[0];
    d->vy    = (u16)gPlayerStatus.coordMtx->t[1] - (u16)coord->coord.t[1];
    d->vz    = (u16)gPlayerStatus.coordMtx->t[2] - (u16)coord->coord.t[2];
    if (actorOutsideRadius(&delta, 900) == 0)
        work->walker.speed = 0;
    walker              = &work->walker;
    ramp                = work->walker.speed;
    walker->speedStep   = 0;
    walker->speedTarget = ramp;
    walker->speed       = ramp;
    bossStrangerTick(walker);
    work->stateFrame++;
    facing = arg0->extra.tmd->coords;
    angle  = ratan2(delta.vx, d->vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    angle  = Actor110600_WrapHitAngle(angle);
    if (abs(angle) < 0x80) {
        if (actorOutsideRadius(&delta, 500) != 0) {
            if (actorOutsideRadius(&delta, 1000) == 0 && work->stateFrame >= 25)
                work->state = ACTOR_110600_STATE_ATTACK;
        }
    }
    if (actorOutsideRadius(&delta, 1000) == 0 && work->stateFrame >= 91)
        work->state = ACTOR_110600_STATE_ATTACK;
    work->lookYawTarget = angle;
    func_actor_110600_80134728(arg0);
    pose = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    if ((pose == 0x33) && (work->lastFootstepCueIndex != pose)) {
        work->footstepShake = 1;
    }
    nextPose = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    if ((nextPose == 0x26) && (work->lastFootstepCueIndex != nextPose)) {
        work->footstepShake = 2;
    }
    work->lastFootstepCueIndex = (s32)(work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK);
    if (work->footstepShake != 0) {
        if (Actor110600_TickShake() != 0)
            work->footstepShake = 0;
    }
}

/// Aiming stage: points the model at the player. Entering on a live
/// actor re-arms it — clear the model object, drop bit 0x8000 of
/// `attackBody.flags` and set 0x4000 of `gridBody.flags`, tag the enemy's link
/// node, then park the stage timer at 0x15 with `animRate` reloaded from
/// `baseRate`. The yaw of the delta from the model's root coordinate to
/// `gPlayerStatus.coordMtx`'s translation goes through `ratan2`, has the coordinate's own
/// yaw (`ratan2` of `-m[2][0]`, `m[2][2]`) subtracted, and is wrapped into
/// [-0x800, 0x800] before it lands in `lookYawTarget`; the model is ticked and the
/// actor moves on (state 3) once slot 1 of `rig` reports
/// `ANIMATION_SLOT_REACHED_BOUNDARY`.
/// Both translations are measured in their low 16 bits, so all three delta
/// reads are `u16`.
static void func_actor_110600_80135A18(Task* arg0)
{
    _Actor110600Work* work;
    TmdObject*        obj;
    Enemy*            enemy;
    GfxCoord*         coord;
    GfxCoord*         facing;
    SVECTOR           delta;
    SVECTOR*          d;
    s16               angle;

    work  = arg0->work;
    obj   = arg0->extra.tmd;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj->flags                    = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->animRequest             = ACTOR_110600_ANIM_REQUEST_BLEND;
        work->animId                  = 0x15;
        work->animRate                = work->baseRate;
    }
    coord    = arg0->extra.tmd->coords;
    d        = &delta;
    delta.vx = (u16)gPlayerStatus.coordMtx->t[0] - (u16)coord->coord.t[0];
    d->vy    = (u16)gPlayerStatus.coordMtx->t[1] - (u16)coord->coord.t[1];
    d->vz    = (u16)gPlayerStatus.coordMtx->t[2] - (u16)coord->coord.t[2];
    facing   = arg0->extra.tmd->coords;
    angle    = ratan2(delta.vx, d->vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
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
    work->lookYawTarget = angle;
    func_actor_110600_80134728(arg0);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ACTOR_110600_STATE_CHASE;
    }
}

/// 1 when the first of `recs` carries the kind 0x10000 tag: the walk breaks on
/// an empty slot and reports 0.
static __inline__ s32 Actor110600_HasRec10000(WorldCollisionContact* recs)
{
    s16 i;

    for (i = 0; i < 1; i++) {
        if (!recs[i].key.value) {
            break;
        }
        if ((recs[i].key.value & 0xFFFF0000) == 0x10000) {
            return 1;
        }
    }
    return 0;
}

/// Firing stage. Entering on a live actor re-arms it: clear the model object,
/// take 0x8000 off `attackBody.flags` and put 0x4000 on `gridBody.flags`, tag
/// the enemy's link node, and pick one of the two patrol modes off
/// `enraged` — a zeroed one packs the model pair with 1 and holds the stage
/// at `animId` 5 for 0x10 ticks, a set one packs it with 0 and holds mode 4
/// for 0x1A. `walker.state` is raised and `walker` is re-armed
/// for a fresh patrol (`speedTarget` cleared, `speed` reloaded from its own
/// value, `speedStep` = 8) with `lookYawTarget` / `lookYaw` cleared and
/// `walker.turnLimit` parked at 0x10 to cover the first ten ticks. Every tick after
/// that raises `stateFrame`, which retires `walker.turnLimit` once it passes 0xB, and
/// ticks the model; slot 1's cue index then drives the pair of flag edges the
/// mode owns — 0xF raises and 0x15 drops 0x8000 in mode 4, 0x10 / 0x13 the
/// same in mode 5. Slot 1's `ANIMATION_SLOT_REACHED_BOUNDARY` moves the actor on
/// (state 3). Finally, while the first `WorldCollisionContact` record still carries the
/// 0x10000 kind tag, the model root's pan and depth are played as sound
/// 0x401D000D and 0x8000 comes off `attackBody.flags`.
static void func_actor_110600_80135B84(Task* arg0)
{
    _Actor110600Work*   work;
    BossStrangerWalker* walker;
    Enemy*              enemy;
    TmdObject*          obj;
    u16                 ramp;

    work  = arg0->work;
    obj   = arg0->extra.tmd;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj->flags                    = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->animRequest             = ACTOR_110600_ANIM_REQUEST_BLEND;
        if (((_Actor110600Work*)arg0->work)->enraged != 0) {
            work->attackBody.key = Gp_PackObjPair(enemy, 0);
            work->animId         = 4;
            work->animRate       = 0x1A;
        } else {
            work->attackBody.key = Gp_PackObjPair(enemy, 1);
            work->animId         = 5;
            work->animRate       = 0x10;
        }
        work->walker.state     = BOSS_STRANGER_WALKER_CHASE;
        walker                 = &work->walker;
        ramp                   = work->walker.speed;
        walker->speedTarget    = 0;
        walker->speedStep      = 8;
        walker->speed          = ramp;
        work->walker.turnLimit = 0x10;
        work->lookYaw          = 0;
        work->lookYawTarget    = 0;
        work->stateFrame       = 0;
    }
    work->stateFrame++;
    if (work->stateFrame >= 0xB) {
        work->walker.turnLimit = 0;
    }
    func_actor_110600_80134728(arg0);
    if (work->animId == 4) {
        switch (work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) {
            case 0xF:
                work->attackBody.flags = (u16)(work->attackBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
                break;
            case 0x15:
                work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
                break;
        }
    }
    if (work->animId == 5) {
        switch (work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) {
            case 0x10:
                work->attackBody.flags = (u16)(work->attackBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
                break;
            case 0x13:
                work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
                break;
        }
    }
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ACTOR_110600_STATE_CHASE;
    }
    if (Actor110600_HasRec10000(work->attackContacts)) {
        SndEvt_EnqueueType6(SOUND_STRANGER_ATTACK_HIT, (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords),
                            (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
    }
}

static void func_actor_110600_80135E20(Task* arg0, s16 arg1, s32 arg2)
{
    SVECTOR* sc;
    s32      mag;

    sc  = (SVECTOR*)SCRATCH_STACK_RESERVE_BYTES(8);
    mag = (arg1 >= 0) ? arg1 : -arg1;
    if (mag < 0x200) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        switch ((s32)(gRandomLcgState >> 16) & 3) {
            case 0:
                *sc = D_actor_110600_801485C4[0];
                break;
            case 1:
                *sc = D_actor_110600_801485C4[1];
                break;
            case 2:
                *sc = D_actor_110600_801485C4[2];
                break;
            case 3:
                *sc = D_actor_110600_801485C4[3];
                break;
            default:
                *sc = D_actor_110600_801485C4[4];
                break;
        }
    } else if (mag > 0x600) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        switch ((s32)(gRandomLcgState >> 16) & 2) {
            case 0:
                *sc = D_actor_110600_801485C4[5];
                break;
            case 1:
                *sc = D_actor_110600_801485C4[6];
                break;
            default:
                *sc = D_actor_110600_801485C4[7];
                break;
        }
    } else if (arg1 > 0) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            *sc = D_actor_110600_801485C4[8];
        } else {
            *sc = D_actor_110600_801485C4[9];
        }
    } else {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            *sc = D_actor_110600_801485C4[10];
        } else {
            *sc = D_actor_110600_801485C4[11];
        }
    }
    D_actor_110600_80148698.coord      = &arg0->extra.tmd->coords[1];
    D_actor_110600_80148698.spawnArgLo = 0x100;
    D_actor_110600_80148698.spawnArgHi = 3;
    func_800FDB18(Gp_GetIdParam1(arg2) & 0xFFFF, &arg0->extra.tmd->coords[sc->pad], sc, &D_actor_110600_80148698);
    SCRATCH_STACK_RELEASE_BYTES(8);
}

static __inline__ s32 Actor110600_FindHit(SVECTOR* point, WorldCollisionContact* recs, s16 count)
{
    s16 i;
    for (i = 0; i < count; i++) {
        if (!recs[i].key.value)
            break;
        if ((recs[i].key.value & 0xFFFF0000) == 0x20000) {
            point->vx = recs[i].point.vx;
            point->vy = recs[i].point.vy;
            point->vz = recs[i].point.vz;
            return recs[i].key.value;
        }
    }
    return 0;
}

static void func_actor_110600_80136210(Task* arg0)
{
    _Actor110600Work*      work;
    Enemy*                 enemy;
    GfxCoord*              facing;
    s16                    angle;
    s16                    dz;
    s16                    state;
    s32                    magnitude;
    s32                    yaw;
    s32                    x;
    s32                    y;
    s32                    z;
    s32                    distance;
    s32                    pan;
    u32                    kind;
    Actor110600HitScratch* sc;
    PlayerStatus*          player;

    enemy  = arg0->spawnArg2.pointer;
    work   = arg0->work;
    player = &gPlayerStatus;
    if (player->hp <= 0) {
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        return;
    }
    sc      = (Actor110600HitScratch*)SCRATCH_STACK_RESERVE_BYTES(0x30);
    sc->key = Actor110600_FindHit(&sc->point, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    if (!sc->key) {
        sc->key = Actor110600_FindHit(&sc->point, work->gridContacts, ARRAY_SIZE(work->gridContacts));
    }
    if (sc->key) {
        state = work->state;
        if ((state == ACTOR_110600_STATE_PATROL) || (state == ACTOR_110600_STATE_IDLE) || (state == ACTOR_110600_STATE_LURK)) {
            work->state = ACTOR_110600_STATE_ALERT;
        }
        x            = player->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0];
        sc->delta.vx = x;
        y            = player->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1];
        sc->delta.vy = y;
        z            = player->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2];
        sc->delta.vz = z;
        distance     = SquareRoot0((x * x) + (y * y) + (z * z));
        sc->distance = distance;
        sc->damage   = Gp_ComputeDamage((u32)sc->key, (u32)distance, 0, 0);
        if (Gp_RollEnemyChance(enemy, (u32)sc->key, 0) != 0) {
            sc->damage = (u32)(sc->damage * 5);
            Gp_SpawnEff(EFFECT_CRITICAL_HIT, arg0->extra.tmd->coords + 2, 0, NULL);
        }
        magnitude = sc->angle;
        if (magnitude < 0) {
            magnitude = -magnitude;
        }
        if (magnitude >= 0x501) {
            sc->damage = (u32)(sc->damage * 2);
        }
        if (work->enraged == 1) {
            sc->damage = (u32)((u32)sc->damage >> 1);
        }
        func_800E2C78(enemy, sc->key, (s32)sc->damage, 0);
        enemy->hp = (u16)enemy->hp - (u16)sc->damage;
        func_800DA6E8(&enemy->node, (s32)sc->damage, 0);
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(arg0->extra.tmd->coords);
        sc->direction.vx = (s16)(sc->point.vx - (u16)arg0->extra.tmd->coords->workm.t[0]);
        sc->direction.vy = (s16)(sc->point.vy - (u16)arg0->extra.tmd->coords->workm.t[1]);
        dz               = sc->point.vz - (u16)arg0->extra.tmd->coords->workm.t[2];
        sc->direction.vz = dz;
        yaw              = ratan2((s32)sc->direction.vx, (s32)dz);
        facing           = arg0->extra.tmd->coords;
        angle            = yaw - ratan2((s32)-facing->workm.m[2][0], (s32)facing->workm.m[2][2]);
        sc->angle        = angle;
        sc->angle        = Actor110600_WrapHitAngle(sc->angle);
        func_actor_110600_80135E20(arg0, sc->angle, sc->key);
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
            work->hitBody.coord     = arg0->extra.tmd->coords + 2;
            work->lookYaw           = 0;
            work->lookYawTarget     = 0;
            displaySetShakeY(0);
        } else {
            pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(SOUND_STRANGER_HURT, (s32)pan, (s32)(s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        }
        work->hitCooldown = Gp_GetIdParam2(sc->key);
        kind              = Gp_GetIdParam0(sc->key) & 0xFFFF;
        switch (kind) {
            case 0:
            case 4:
            case 5:
            case 6:
            case 7:
                work->blendActive  = 1;
                work->blendAnimId  = 0xB;
                work->blendRequest = ACTOR_110600_ANIM_REQUEST_RESET;
                break;
            case 1:
            case 8:
            case 9:
                break;
            case 2:
                Gp_SetObjFlag2(enemy, sc->key, 0);
                work->state = ACTOR_110600_STATE_STATUS_HOLD;
                break;
            case 3:
                Gp_SetObjFlag4(enemy, sc->key, 0);
                break;
        }
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        sc->damage = Gp_TickObjFlag4(enemy);
        if (Gp_ObjFlag4Expired(enemy) != 0) {
            enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
        if (sc->damage != 0) {
            enemy->hp = (u16)enemy->hp - (u16)sc->damage;
            func_800DA6E8(&enemy->node, (s32)sc->damage, 0);
            if (work->state != ACTOR_110600_STATE_STATUS_HOLD) {
                work->blendActive  = 1;
                work->blendAnimId  = 0xB;
                work->blendRequest = ACTOR_110600_ANIM_REQUEST_RESET;
            } else {
                work->prevState = -1;
            }
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(0x30);
}

/// Timer stage that walks between the two long `animId` values. Entering on
/// a live actor re-arms it: clear the model object, take 0x8000 off
/// `attackBody.flags` and put 0x4000 on `gridBody.flags`, tag the enemy's link
/// node, set the stage timer to 0x18 and `animRate` from `baseRate`, then
/// re-arm `walker` for a fresh patrol (`speedTarget` cleared,
/// `speed` reloaded from its own value, `speedStep` = 8) with `walker.state` /
/// `walker.turnLimit` / `lookYaw` / `lookYawTarget` cleared. Every tick after that steps
/// the walker and the model, then retimes: at 0x18 a draw of `gRandomLcgState`
/// whose seventh bit is clear drops it to 0xE, and at 0xE slot 1's
/// `ANIMATION_SLOT_REACHED_BOUNDARY` puts it back to 0x18.
/// Both retimes re-enter state 1 (`animRequest`) and tick once more.
static void func_actor_110600_80136888(Task* arg0)
{
    _Actor110600Work*   work;
    BossStrangerWalker* walker;
    Enemy*              enemy;
    TmdObject*          obj;
    u32                 rng;
    u16                 ramp;

    work  = arg0->work;
    obj   = arg0->extra.tmd;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj->flags                    = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->animRequest             = ACTOR_110600_ANIM_REQUEST_BLEND;
        work->animId                  = 0x18;
        work->animRate                = work->baseRate;
        ramp                          = work->walker.speed;
        walker                        = &work->walker;
        work->walker.state            = BOSS_STRANGER_WALKER_IDLE;
        walker->speedTarget           = 0;
        walker->speed                 = ramp;
        walker->speedStep             = 8;
        work->walker.turnLimit        = 0;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
    }
    bossStrangerTick(&work->walker);
    func_actor_110600_80134728(arg0);
    if (work->animId == 0x18) {
        if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP) {
            rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = rng;
            if (!((rng >> 16) & 7)) {
                work->animId      = 0xE;
                work->animRequest = ACTOR_110600_ANIM_REQUEST_BLEND;
                func_actor_110600_80134728(arg0);
            }
        }
    }
    if ((work->animId == 0xE) && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        work->animId      = 0x18;
        work->animRequest = ACTOR_110600_ANIM_REQUEST_BLEND;
        func_actor_110600_80134728(arg0);
    }
}

/// The walker's handoff stage. Entering on a live actor re-arms it: clear the
/// model object, take 0x8000 off `attackBody.flags` and put 0x4000 on
/// `gridBody.flags`, tag the enemy's link node, park the stage timer at 0x1D
/// with `animRate` at 0x10, then re-arm `walker` to run its
/// patrol out (`speedTarget` = 0xFFFE, `speed` reloaded from its own value,
/// `speedStep` = 2) with `walker.state` / `walker.turnLimit` / `lookYaw` /
/// `lookYawTarget` cleared. Every tick after that steps the walker and the model; at
/// 0x1D slot 1's `ANIMATION_SLOT_REACHED_BOUNDARY` moves the stage to 0x1E and
/// re-seeds the walker block, and at 0x1E that same bit picks what the actor
/// does next: 0xB while the enemy's `hp` is still positive, 0xC once it
/// has run out.
static void func_actor_110600_801369D8(Task* arg0)
{
    _Actor110600Work*   work;
    BossStrangerWalker* walker;
    BossStrangerWalker* walker2;
    Enemy*              enemy;
    u16                 ramp;
    u16                 ramp2;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        arg0->extra.tmd->flags        = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->animRequest             = ACTOR_110600_ANIM_REQUEST_RESET;
        work->animId                  = 0x1D;
        work->animRate                = 0x10;
        ramp                          = work->walker.speed;
        walker                        = &work->walker;
        work->walker.state            = BOSS_STRANGER_WALKER_IDLE;
        walker->speedTarget           = 0xFFFE;
        walker->speed                 = ramp;
        walker->speedStep             = 2;
        work->walker.turnLimit        = 0;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
    }
    walker2 = &work->walker;
    bossStrangerTick(walker2);
    func_actor_110600_80134728(arg0);
    if (work->animId == 0x1D) {
        if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
            ramp2                = work->walker.speed;
            work->animId         = 0x1E;
            work->animRequest    = ACTOR_110600_ANIM_REQUEST_RESET;
            walker2->speedTarget = 0;
            walker2->speedStep   = 2;
            walker2->speed       = ramp2;
            return;
        }
    }
    if ((work->animId == 0x1E) && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        if (enemy->hp > 0) {
            work->state = ACTOR_110600_STATE_DOWN;
        } else {
            work->state = ACTOR_110600_STATE_DEATH_BURN;
        }
    }
}

static __inline__ void Actor110600_ApplyShrink(Task* arg0, _Actor110600Work* work, s16 y)
{
    TmdObject*            obj;
    GfxCoord*             coord;
    ActorScaleRotScratch* blk;
    ActorScaleRotScratch* head;
    s16                   page;
    s16                   ang;
    u16                   m22;
    ActorScaleRotScratch* restoredHead;

    head                                       = SCRATCH_STACK_CURSOR(ActorScaleRotScratch);
    obj                                        = arg0->extra.tmd;
    coord                                      = obj->coords;
    page                                       = work->walker.scale;
    blk                                        = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleRotScratch) = blk;
    y                                         -= (work->stateFrame - 0x12C) * 2;
    ang                                        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->yaw                                   = ang;
    gfxRotMatrixY(&blk->rotation, ang, 1);
    blk->scale.vx = page;
    blk->scale.vy = y;
    blk->scale.vz = page;
    ScaleMatrix(&blk->rotation, &blk->scale);
    coord->coord.m[0][0]                       = (u16)(head - 1)->rotation.m[0][0];
    coord->coord.m[0][1]                       = (u16)blk->rotation.m[0][1];
    coord->coord.m[0][2]                       = (u16)blk->rotation.m[0][2];
    coord->coord.m[1][0]                       = (u16)blk->rotation.m[1][0];
    coord->coord.m[1][1]                       = (u16)blk->rotation.m[1][1];
    coord->coord.m[1][2]                       = (u16)blk->rotation.m[1][2];
    coord->coord.m[2][0]                       = (u16)blk->rotation.m[2][0];
    coord->coord.m[2][1]                       = (u16)blk->rotation.m[2][1];
    restoredHead                               = SCRATCH_STACK_CURSOR(ActorScaleRotScratch);
    m22                                        = (u16)blk->rotation.m[2][2];
    coord->composeStamp                        = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_CURSOR(ActorScaleRotScratch) = restoredHead + 1;
    coord->coord.m[2][2]                       = m22;
}

static void func_actor_110600_80136B20(Task* arg0)
{
    _Actor110600Work* work;
    Enemy*            enemy;
    SVECTOR           pos;
    VECTOR            scale;
    s16               y;

    work = arg0->work;
    if (work->stateEntered != 0) {
        enemy                         = arg0->spawnArg2.pointer;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        work->stateFrame              = 0;
        work->burnHeightScale         = work->walker.scale;
    }
    if (work->stateFrame < 0x3E8) {
        work->stateFrame = (u16)work->stateFrame + 1;
    }
    switch (work->stateFrame) {
        case 0xE6:
            arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            work->burnColorMtx     = work->colorMtx;
            break;
        case 0xC8:
        case 0x190:
            pos.vx = 0x12C;
            pos.vy = 0;
            pos.vz = 0;
            Gp_SpawnEff(EFFECT_CORPSE_BURN, &arg0->extra.tmd->coords[3], 3, &pos);
            break;
        case 0xFA:
        case 0x1A4:
            pos.vx = 0x190;
            pos.vy = 0;
            pos.vz = 0;
            break;
        case 0x258:
            arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
    if (work->stateFrame >= 0xE6) {
        scale.vx = scale.vy = scale.vz = 0xBB8 + work->stateFrame * -4;
        work->colorMtx                 = work->burnColorMtx;
        ScaleMatrix(&work->colorMtx, &scale);
        gte_lddp(scale.vz);
        gte_ldlvl(&work->colorMtx.t[0]);
        gte_gpf12();
        gte_stlvl(&work->colorMtx.t[0]);
    }
    if (work->stateFrame == 0xFD) {
        arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
    }
    if (work->stateFrame >= 0xC9) {
        y = work->burnHeightScale;
        if (work->burnHeightScale >= 0x801) {
            y                    -= 8;
            work->burnHeightScale = y;
            Actor110600_ApplyShrink(arg0, work, y);
        }
    }
}

/// Re-dresses a live actor: take the model object out of draw, drop bit 0x8000
/// of `attackBody.flags` and bit 0x4000 of `gridBody.flags`, tag the enemy's
/// link node, clear the `walker.turnLimit` / `lookYaw` / `lookYawTarget` timers and hand
/// the model the 0x80 texture page, then spawn five effects off its part
/// coordinates 6, 8, 10, 11 and 15 (`Gp_SpawnEff` bank 0xA0005, buffer sizes
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

        effect1 = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, &arg0->extra.tmd->coords[6], 0x200, NULL);
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
            entry1                    = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->placements, index1);
            model1->texturePageOffset = entry1->texturePageOffset;
            model1->clutRowOffset     = entry1->clutRowOffset;
            if (model1->buffer != NULL) {
                tmdProcessStream(model1);
                tmdProcessStream(model1);
            }
        }

        effect2 = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, &arg0->extra.tmd->coords[8], 0x200, NULL);
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
            entry2                    = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->placements, index2);
            model2->texturePageOffset = entry2->texturePageOffset;
            model2->clutRowOffset     = entry2->clutRowOffset;
            if (model2->buffer != NULL) {
                tmdProcessStream(model2);
                tmdProcessStream(model2);
            }
        }

        effect3 = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, &arg0->extra.tmd->coords[10], 0x200, NULL);
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
            entry3                    = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->placements, index3);
            model3->texturePageOffset = entry3->texturePageOffset;
            model3->clutRowOffset     = entry3->clutRowOffset;
            if (model3->buffer != NULL) {
                tmdProcessStream(model3);
                tmdProcessStream(model3);
            }
        }

        effect4 = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, &arg0->extra.tmd->coords[11], 0x300, NULL);
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
            entry4                    = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->placements, index4);
            model4->texturePageOffset = entry4->texturePageOffset;
            model4->clutRowOffset     = entry4->clutRowOffset;
            if (model4->buffer != NULL) {
                tmdProcessStream(model4);
                tmdProcessStream(model4);
            }
        }

        effect5 = Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, &arg0->extra.tmd->coords[15], 0x300, NULL);
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
            entry5                    = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->placements, index5);
            model5->texturePageOffset = entry5->texturePageOffset;
            model5->clutRowOffset     = entry5->clutRowOffset;
            if (model5->buffer != NULL) {
                tmdProcessStream(model5);
                tmdProcessStream(model5);
            }
        }
    }
}

/// Offset, 100 units along Z, that `func_actor_110600_801372CC` hands
/// `func_800FDB18` with the model's seventh coordinate when it spawns its
/// three effects.
static const SVECTOR D_actor_110600_80131F1C = { 0, 0, 100, 0 };

static __inline__ void Actor110600_RescaleRoot(Task* arg0, s16 scale)
{
    ActorScaleRotScratch* blk;
    GfxCoord*             coord;
    u8*                   head;
    s16                   ang;
    u16                   m22;

    head                                       = SCRATCH_STACK_CURSOR(u8);
    coord                                      = arg0->extra.tmd->coords;
    blk                                        = (ActorScaleRotScratch*)(head - sizeof(ActorScaleRotScratch));
    SCRATCH_STACK_CURSOR(ActorScaleRotScratch) = blk;
    ang                                        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->yaw                                   = ang;
    gfxRotMatrixY(&blk->rotation, ang, 1);
    blk->scale.vz = (s16)scale;
    blk->scale.vy = (s16)scale;
    blk->scale.vx = (s16)scale;
    ScaleMatrix(&blk->rotation, &blk->scale);
    coord->coord.m[0][0] = (u16)((ActorScaleRotScratch*)(head - sizeof(ActorScaleRotScratch)))->rotation.m[0][0];
    coord->coord.m[0][1] = (u16)blk->rotation.m[0][1];
    coord->coord.m[0][2] = (u16)blk->rotation.m[0][2];
    coord->coord.m[1][0] = (u16)blk->rotation.m[1][0];
    coord->coord.m[1][1] = (u16)blk->rotation.m[1][1];
    coord->coord.m[1][2] = (u16)blk->rotation.m[1][2];
    coord->coord.m[2][0] = (u16)blk->rotation.m[2][0];
    coord->coord.m[2][1] = (u16)blk->rotation.m[2][1];
    m22                  = (u16)blk->rotation.m[2][2];
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    coord->coord.m[2][2] = m22;
    SCRATCH_STACK_RELEASE_BYTES(sizeof(ActorScaleRotScratch));
}

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
                func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg0->extra.tmd->coords[9], &vec, &D_actor_110600_80148698);
            } else {
                d             = &D_actor_110600_80148698;
                vec.vx        = -0x19;
                vec.vy        = 0;
                vec.vz        = 0;
                effectCoord2  = arg0->extra.tmd->coords;
                d->spawnArgLo = 0x100;
                d->spawnArgHi = 3;
                d->coord      = effectCoord2;
                func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg0->extra.tmd->coords[2], &vec, &D_actor_110600_80148698);
            }
        }
    }
    work->blendActive = 0;
    func_actor_110600_80134728(arg0);

    Actor110600_RescaleRoot(arg0, work->walker.scale);

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
                func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg0->extra.tmd->coords[6], &vec, &D_actor_110600_80148698);
                func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg0->extra.tmd->coords[6], &vec, &D_actor_110600_80148698);
                func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg0->extra.tmd->coords[6], &vec, &D_actor_110600_80148698);
            }
        }
    }
}

/// The turn-away stage, the pick-up twin of the `animId` == 0x16 leg of
/// `func_actor_110600_80136888`: entering on a live actor clears the model
/// object, takes 0x8000 off `attackBody.flags` and 0x4000 off
/// `gridBody.flags`, tags the enemy's link node with 1 and parks the timer at
/// `animRate` = 0x20 with `animRequest` re-armed, `walker.turnLimit` / `lookYaw` /
/// `lookYawTarget` cleared. Every tick after that steps the shared handler and, at
/// 0x16, rolls `gRandomLcgState` and turns the model's root coordinate by the yaw
/// the roll's low nibble picks — 0x32 while it is under 0xA, -0x78 past it —
/// clearing the coordinate's `composeStamp`. Once `animFrames` has run up to 0x1F the
/// stage drops the timer to 0x10, re-arms `animRequest` and steps to 0x21, where
/// slot 1's `ANIMATION_SLOT_REACHED_BOUNDARY` walks it on to state 0xC.
static void func_actor_110600_80137684(Task* arg0)
{
    _Actor110600Work* work;
    Enemy*            enemy;
    u32               rng;

    work = arg0->work;
    if (work->stateEntered != 0) {
        enemy                         = arg0->spawnArg2.pointer;
        arg0->extra.tmd->flags        = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->animRate                = 0x20;
        work->animRequest             = ACTOR_110600_ANIM_REQUEST_BLEND;
        work->walker.turnLimit        = 0;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
        work->animId                  = 0x16;
    }
    func_actor_110600_80134728(arg0);
    if (work->animId == 0x16) {
        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rng;
        if (((rng >> 16) & 0xF) < 0xA) {
            gfxRotMatrixY(&arg0->extra.tmd->coords->coord, 0x32, 0);
        } else {
            gfxRotMatrixY(&arg0->extra.tmd->coords->coord, -0x78, 0);
        }
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        if ((work->animId == 0x16) && (work->animFrames >= 0x1F)) {
            work->animRate    = 0x10;
            work->animRequest = ACTOR_110600_ANIM_REQUEST_BLEND;
            work->animId      = 0x21;
        }
    }
    if ((work->animId == 0x21) && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        work->state = ACTOR_110600_STATE_DEATH_BURN;
    }
}

static void func_actor_110600_801377FC(Task* arg0)
{
    _Actor110600Work* work;
    TmdObject*        obj;
    Enemy*            enemy;
    GfxCoord*         coord;
    SVECTOR           delta;
    SVECTOR*          d;

    work = arg0->work;
    if (work->stateEntered != 0) {
        enemy                         = arg0->spawnArg2.pointer;
        obj                           = arg0->extra.tmd;
        work->animId                  = 0x22;
        work->animRequest             = ACTOR_110600_ANIM_REQUEST_RESET;
        obj->flags                    = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->walker.turnLimit        = 0;
        work->lookYaw                 = 0;
        work->animRate                = 0x10;
        work->lookYawTarget           = 0;
    }
    func_actor_110600_80134728(arg0);
    work->lastCueIndex = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    coord              = arg0->extra.tmd->coords;
    d                  = &delta;
    delta.vx           = (u16)gPlayerStatus.coordMtx->t[0] - (u16)coord->coord.t[0];
    d->vy              = (u16)gPlayerStatus.coordMtx->t[1] - (u16)coord->coord.t[1];
    d->vz              = (u16)gPlayerStatus.coordMtx->t[2] - (u16)coord->coord.t[2];
    if (!actorOutsideRadius(d, 0xBB8)) {
        work->state = ACTOR_110600_STATE_LURK_ALERT;
    }
}

/// Aiming stage that re-arms the model behaviour on a live actor — `animId`
/// = 0x15 with `animRequest` = 1, the model object's `field_C` cleared, bit 0x8000
/// off `attackBody.flags` and 0x4000 on `gridBody.flags`, the enemy's link node
/// tagged 1 with `walker.turnLimit` / `lookYaw` cleared and `animRate` = 0x10 — then
/// wraps the yaw from the model's root coordinate to the player
/// `gPlayerStatus.coordMtx` against the coordinate's own yaw (`ratan2` of `-m[2][0]`,
/// `m[2][2]`) into `lookYawTarget`. Ticks the model and moves the actor to state 3
/// once slot 1 of `rig` reports `ANIMATION_SLOT_REACHED_BOUNDARY`. Same wrap as
/// `func_actor_110600_80135A18`.
static void func_actor_110600_80137980(Task* arg0)
{
    _Actor110600Work* work;
    TmdObject*        obj;
    Enemy*            enemy;
    GfxCoord*         coord;
    GfxCoord*         facing;
    SVECTOR           delta;
    SVECTOR*          d;
    s16               angle;

    work = arg0->work;
    if (work->stateEntered != 0) {
        enemy                         = arg0->spawnArg2.pointer;
        obj                           = arg0->extra.tmd;
        work->animId                  = 0x15;
        work->animRequest             = ACTOR_110600_ANIM_REQUEST_BLEND;
        obj->flags                    = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->walker.turnLimit        = 0;
        work->lookYaw                 = 0;
        work->animRate                = 0x10;
    }
    coord    = arg0->extra.tmd->coords;
    d        = &delta;
    delta.vx = (u16)gPlayerStatus.coordMtx->t[0] - (u16)coord->coord.t[0];
    d->vy    = (u16)gPlayerStatus.coordMtx->t[1] - (u16)coord->coord.t[1];
    d->vz    = (u16)gPlayerStatus.coordMtx->t[2] - (u16)coord->coord.t[2];
    facing   = arg0->extra.tmd->coords;
    angle    = ratan2(delta.vx, d->vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
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
    work->lookYawTarget = angle;
    func_actor_110600_80134728(arg0);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ACTOR_110600_STATE_CHASE;
    }
}

/// Stage-driven recoil push, one stage per tick. On a live actor it clears the
/// model's flags, drops bit 0x8000 of `attackBody.flags` and sets 0x4000 of
/// `gridBody.flags`, tags the enemy's link node and restarts the stage at 0.
/// The push takes column 0 of the model root coordinate, normalises it out of
/// place and scales it by the stage — 0x320, -0x3E8, 0x190, -0x190, 0xC8,
/// through the GTE's interpolation register. Only the X and Z components are
/// added to the coordinate's translation, and the coordinate is marked dirty so
/// the tree is recomputed. Stage 5 pushes nothing: it moves the actor to state
/// 3 and leaves the counter parked.
static void func_actor_110600_80137AF4(Task* arg0)
{
    _Actor110600Work* work;
    TmdObject*        obj;
    Enemy*            enemy;
    SVECTOR           vec;
    GfxCoord*         coord;

    work  = arg0->work;
    obj   = arg0->extra.tmd;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj->flags                    = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        D_actor_110600_80148688.value = 0;
    }
    Gfx_MatrixCol0(&arg0->extra.tmd->coords->coord, &vec);
    VectorNormalSS(&vec, &vec);
    switch (D_actor_110600_80148688.value) {
        case 0:
            gte_lddp(0x320);
            gte_ldsv(&vec);
            gte_gpf12();
            gte_stsv(&vec);
            break;
        case 1:
            gte_lddp(-0x3E8);
            gte_ldsv(&vec);
            gte_gpf12();
            gte_stsv(&vec);
            break;
        case 2:
            gte_lddp(0x190);
            gte_ldsv(&vec);
            gte_gpf12();
            gte_stsv(&vec);
            break;
        case 3:
            gte_lddp(-0x190);
            gte_ldsv(&vec);
            gte_gpf12();
            gte_stsv(&vec);
            break;
        case 4:
            gte_lddp(0xC8);
            gte_ldsv(&vec);
            gte_gpf12();
            gte_stsv(&vec);
            break;
        case 5:
            work->state = ACTOR_110600_STATE_CHASE;
            return;
    }
    arg0->extra.tmd->coords->coord.t[0] += vec.vx;
    arg0->extra.tmd->coords->coord.t[2] += vec.vz;
    coord                                = arg0->extra.tmd->coords;
    D_actor_110600_80148688.value++;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Death stage machine, entering on a live actor: take the model out of draw,
/// drop bit 0x8000 of `attackBody.flags` and set 0x4000 of `gridBody.flags`,
/// tag the enemy's link node, arm `animId` / `animRequest` and the `animRate`
/// timer, tick once and clear both `stateFrame` and the `enrageStage` stage. Stage
/// 0 idles on that timer — once slot 1's cue index reaches 4 it parks
/// `animRate` at -0x10 and steps to stage 1. Stage 1 is the shrink tail:
/// halves `animRate` each tick, parking at -0xC when the halving lands on the
/// stage value and bouncing -1 back to 8, and after 0x35 ticks parks
/// `animRate` / `baseRate` at 0x38 and moves the actor to state 3. Every
/// stage-1 tick also adds 0x27 to `enrageTint`.
static void func_actor_110600_80137DB0(Task* arg0)
{
    _Actor110600Work* work;
    Enemy*            enemy;
    TmdObject*        obj;
    s16               step;
    s32               state;

    work = arg0->work;
    if (work->stateEntered != 0) {
        enemy                         = arg0->spawnArg2.pointer;
        obj                           = arg0->extra.tmd;
        obj->flags                    = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->animId                  = 0xC;
        work->animRequest             = ACTOR_110600_ANIM_REQUEST_RESET;
        work->walker.turnLimit        = 0;
        work->lookYaw                 = 0;
        work->animRate                = 6;
        func_actor_110600_80134728(arg0);
        work->stateFrame  = 0;
        work->enrageStage = 0;
    }
    state             = work->enrageStage;
    work->blendActive = 0;
    switch (state) {
        case 0:
            func_actor_110600_80134728(arg0);
            if ((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 4) {
                work->animRate    = -0x10;
                work->enrageStage = (s16)((u16)work->enrageStage + 1);
                return;
            }
            return;
        case 1:
            step           = (s16)work->animRate / 2;
            work->animRate = step;
            work->stateFrame++;
            if (work->animRate == state) {
                work->animRate = -0xC;
            }
            if (work->animRate == -1) {
                work->animRate = 8;
            }
            func_actor_110600_80134728(arg0);
            if (work->stateFrame >= 0x35) {
                work->animRate = 0x38;
                work->baseRate = 0x38;
                work->state    = ACTOR_110600_STATE_CHASE;
            }
            work->enrageTint += 0x27;
            break;
    }
}

/// The actor's state handlers, indexed by `_Actor110600Work::state`. splat
/// migrates the table into the `.s` of the function that reads it, so it is
/// written out here to keep the block in the unit's `.rodata` now that
/// `func_actor_110600_80137F2C` is decompiled.
static const Actor110600StateTable D_actor_110600_80131F3C = {
    func_actor_110600_801388A4,
    NULL,
    func_actor_110600_80135194,
    func_actor_110600_80135454,
    func_actor_110600_80135A18,
    func_actor_110600_80135B84,
    func_actor_110600_80136888,
    func_actor_110600_801369D8,
    func_actor_110600_80138980,
    func_actor_110600_80138AFC,
    func_actor_110600_80138BD0,
    func_actor_110600_80138A70,
    func_actor_110600_80136B20,
    func_actor_110600_80136ECC,
    func_actor_110600_80138D7C,
    NULL,
    NULL,
    func_actor_110600_801372CC,
    NULL,
    func_actor_110600_80137684,
    func_actor_110600_801377FC,
    func_actor_110600_80137980,
    func_actor_110600_80137AF4,
    func_actor_110600_80138CA4,
    func_actor_110600_80137DB0,
};

/// The actor's enemy tick, the middle entry of the `D_actor_110600_80131FA0`
/// triple `func_actor_110600_80134AB4` / this / `enemyDestroy`: copies
/// `D_actor_110600_80131F3C` onto its frame, rebuilds the model root's
/// coordinate and hands its translation to `Gp_UpdateActorColor`, then switches
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
    VECTOR                pos;
    Actor110600StateTable states;
    _Actor110600Work*     work;

    work   = arg1->work;
    states = D_actor_110600_80131F3C;

    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(arg1->extra.tmd->coords);
    pos.vx = arg1->extra.tmd->coords->workm.t[0];
    pos.vy = arg1->extra.tmd->coords->workm.t[1];
    pos.vz = arg1->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(arg0, &pos, 0, 0);

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if ((work->state != ACTOR_110600_STATE_HIDDEN) && (work->state != ACTOR_110600_STATE_DEATH_BURN)) {
                arg1->extra.tmd->flags = 0;
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&arg1->extra.tmd->coords->workm), 0x280, gRoomEffectState->groundShadowShade);
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            if ((work->state != ACTOR_110600_STATE_DEATH_BURN) && (work->state != ACTOR_110600_STATE_HIDDEN)) {
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&arg1->extra.tmd->coords->workm), 0x280, gRoomEffectState->groundShadowShade);
            }
            Gp_ClearRec18Occupied(work->gridContacts);
            Gp_ClearRec18Occupied(work->hitContacts);
            Gp_ClearRec18Occupied(work->attackContacts);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            arg1->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Gp_ClearRec18Occupied(work->gridContacts);
            Gp_ClearRec18Occupied(work->hitContacts);
            Gp_ClearRec18Occupied(work->attackContacts);
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
    states.fn[work->state](arg1);
    work->gridBody.pos.vx = (u16)arg1->extra.tmd->coords->coord.t[0];
    work->gridBody.pos.vy = (u16)((u16)arg1->extra.tmd->coords->coord.t[1] - 0x124);
    work->gridBody.pos.vz = (u16)arg1->extra.tmd->coords->coord.t[2];

    if (arg0->hp > 0) {
        if (work->hitCooldown > 0) {
            work->hitCooldown = (s16)((u16)work->hitCooldown - 1);
        } else {
            func_actor_110600_80136210(arg1);
        }
        if (arg0->hp > 0) {
            goto block_24;
        }
    }
    if (gPlayerStatus.hp <= 0) {
        arg0->hp = 1;
    }
block_24:
    Gp_ClearRec18Occupied(work->gridContacts);
    Gp_ClearRec18Occupied(work->hitContacts);
    Gp_ClearRec18Occupied(work->attackContacts);
    if (gGameSession->viewReady != 0) {
        arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (arg1->extra.tmd->coords->composeStamp == GRAPHICS_COORD_DIRTY) {
        work->rootDirty = 1;
    } else {
        work->rootDirty = 0;
    }
    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(arg1->extra.tmd->coords);
    work->colorMtx.t[1] -= work->enrageTint;
    work->colorMtx.t[2] -= work->enrageTint;
    work->colorMtx.t[0] -= (work->enrageTint * 2) / 3;
    if ((work->state == ACTOR_110600_STATE_DEATH_BURN) || (work->state == ACTOR_110600_STATE_HIDDEN) || (work->state == ACTOR_110600_STATE_DEATH_BURST) || (work->state == ACTOR_110600_STATE_DEATH_THRASH)) {
        work->hitBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    } else {
        work->hitBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
}

s32 func_actor_110600_80138394(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
}

/// The enemy task's three state handlers - spawn, per-frame tick and teardown -
/// that `func_actor_110600_80138EA8` dispatches through by `Task::state`.
static const EnemyTaskFuncTable3 D_actor_110600_80131FA0 = {
    func_actor_110600_80134AB4,
    func_actor_110600_80137F2C,
    enemyDestroy,
};

/// The `0x7D3` handler of the display-opcode table `D_actor_110600_80148624`:
/// maps the requested state onto the work block's `animId` (0x22..0x28) and
/// parks the actor in state 0x11 with `prevState` cleared. States 0 and 4 also
/// stamp the enemy's occupancy tag and re-save its pose; state 0 writes its own
/// `animId` ahead of those calls, so it skips the store the other four share,
/// which is the tail the compiler merged out of the four `break`s.
///
/// The table GCC emits for this switch is what pins the package's
/// `rodata_head`: it lands at 0x18C, 8-aligned only if this unit's `.rodata`
/// starts at 0x4 rather than 0x0 — the package id ahead of it is prepended, not
/// compiled — and behind the id it picks up `.align 3`'s 4-byte pad instead.
s32 func_actor_110600_8013839C(Task* arg0, s32 arg1, AnimationPlayRequest* arg2, s32 arg3)
{
    _Actor110600Work* work;
    Enemy*            enemy;
    s32               state;

    state = arg2->animationId;
    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    switch (state) {
        case 0:
            work->animId      = 0x22;
            enemy->spawnState = 1;
            Gp_SaveEnemyPose(enemy);
            break;
        case 1:
            work->animId = 0x23;
            break;
        case 2:
            work->animId = 0x24;
            break;
        case 3:
            work->animId = 0x25;
            break;
        case 4:
            enemy->spawnState = 1;
            Gp_SaveEnemyPose(enemy);
            work->animId = 0x28;
            break;
    }
    work->state     = ACTOR_110600_STATE_SCRIPTED;
    work->prevState = -1;
    return 0;
}

/// Display-object handler: `arg2` selects the mode. `Enemy.spawnState`, the
/// occupancy tag `Gp_SaveEnemyPose` writes, chooses the flag word in modes 1
/// and 3.
///
/// Mode 0 hides the model with `TMD_OBJECT_SKIP_ACTIVE_DRAW`, allocates its
/// buffers and restarts `state`. Mode 1 shows it and allocates the buffers
/// unless the tag is 4, in which case it hides the model and restarts
/// `state`. Mode 2 sets `TMD_OBJECT_SKIP_AUTO_BUFFER` and restarts `state`.
/// Mode 3 hides the model when the tag is 4 and otherwise clears the flag word,
/// then restarts `state` and sets `TMD_OBJECT_SKIP_AUTO_BUFFER`. `arg1` is
/// unused; the dispatch passes three arguments.
s32 func_actor_110600_80138448(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    TmdObject*        obj;
    _Actor110600Work* work;
    Enemy*            enemy;

    obj   = arg0->extra.tmd;
    enemy = arg0->spawnArg2.pointer;
    work  = arg0->work;
    switch (arg2) {
        case 0:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Tmd_AllocBuffers(obj);
            work->state = ACTOR_110600_STATE_HIDDEN;
            break;
        case 1:
            if (enemy->spawnState == 0) {
                obj->flags = 0;
                Tmd_AllocBuffers(obj);
            } else if (enemy->spawnState == 4) {
                obj->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                work->state = ACTOR_110600_STATE_HIDDEN;
            } else {
                obj->flags = 0;
                Tmd_AllocBuffers(obj);
            }
            break;
        case 2:
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->state = ACTOR_110600_STATE_HIDDEN;
            break;
        case 3:
            if (enemy->spawnState == 4) {
                obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            } else {
                obj->flags = 0;
            }
            work->state = ACTOR_110600_STATE_HIDDEN;
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
    }
    return 0;
}

s32 func_actor_110600_80138538(Task* arg0, s32 msgId, s32 arg2, s32 arg3)
{
    _Actor110600Work* work;
    Enemy*            enemy;

    enemy = (Enemy*)arg0->spawnArg2.pointer;
    work  = arg0->work;
    if (enemy->hp > 0) {
        return 1;
    }
    work->enrageTint     = 0;
    enemy->reactionFlags = 0;
    work->enraged        = 0;
    return 0;
}

#include "../../shared/coord_math_yaw_scale.inc.c"

static void func_actor_110600_80138680(GfxCoord* coord, s16 sx, s16 sy, s16 sz)
{
    void**                scratch;
    ActorScaleRotScratch* head;
    ActorScaleRotScratch* blk;
    s16                   ang;
    u16                   m22;

    scratch                                        = SCRATCH_HEAD_ADDR;
    head                                           = SCRATCH_HEAD_AT(scratch, ActorScaleRotScratch);
    blk                                            = head - 1;
    SCRATCH_HEAD_AT(scratch, ActorScaleRotScratch) = blk;

    ang      = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->yaw = ang;
    gfxRotMatrixY(&blk->rotation, ang, 1);
    blk->scale.vx = sx;
    blk->scale.vy = sy;
    blk->scale.vz = sz;
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
    SCRATCH_POP_AT(scratch, ActorScaleRotScratch);
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    coord->coord.m[2][2] = m22;
}

s32 func_actor_110600_801387C0(Task* arg0, s32 msgId, s32 arg2, s32 arg3)
{
    _Actor110600Work* work;

    work = arg0->work;
    (Gp_IncStateF0Ref)(0);
    work->state = ACTOR_110600_STATE_ALERT;
    return 1;
}

static void func_actor_110600_801387F4(Task* task)
{
    _Actor110600Work* work;
    Enemy*            enemy;
    Task*             helper;
    Task*             helper2;

    work  = task->work;
    enemy = (Enemy*)task->spawnArg2.pointer;
    if (work != NULL) {
        helper = work->childTask0;
        if (helper != NULL) {
            helper->state++;
        }
        helper2 = work->childTask1;
        if (helper2 != NULL) {
            helper2->state++;
        }
        Gp_UnlinkObj(&work->attackBody);
        Gp_UnlinkObj(&work->hitBody);
        Gp_UnlinkObj(&work->gridBody);
        enemy->recs = 0;
    }
    displaySetShakeY(0);
    enemyDestroy(enemy, task);
}

static void func_actor_110600_801388A4(Task* arg0)
{
    TmdObject*        obj;
    _Actor110600Work* work;

    work = arg0->work;
    if (work->stateEntered != 0) {
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        obj->flags                                                = (u16)(obj->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW);
        work->attackBody.flags                                    = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags                                      = (u16)(work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
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

static void func_actor_110600_80138980(Task* arg0)
{
    _Actor110600Work*   work;
    BossStrangerWalker* walker;
    Enemy*              enemy;
    u16                 ramp;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        arg0->extra.tmd->flags        = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->animRequest             = ACTOR_110600_ANIM_REQUEST_RESET;
        work->animId                  = 0xC;
        work->animRate                = 0x10;
        ramp                          = work->walker.speed;
        walker                        = &work->walker;
        work->walker.state            = BOSS_STRANGER_WALKER_IDLE;
        walker->speedTarget           = 2;
        walker->speed                 = ramp;
        walker->speedStep             = 8;
        work->walker.turnLimit        = 0;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
    }
    bossStrangerTick(&work->walker);
    func_actor_110600_80134728(arg0);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        if (enemy->hp > 0) {
            work->state = ACTOR_110600_STATE_DOWN;
        } else {
            work->state = ACTOR_110600_STATE_DEATH_BURN;
        }
    }
}

static void func_actor_110600_80138A70(Task* arg0)
{
    _Actor110600Work* work;
    u32               rng;
    s16               timer;

    work = arg0->work;
    if (work->stateEntered != 0) {
        rng              = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState  = rng;
        work->downFrames = (rng >> 16) & 0x1F;
    }
    timer            = work->downFrames - 1;
    work->downFrames = timer;
    if (timer < 0) {
        switch (work->animId) {
            case 30:
                work->state = ACTOR_110600_STATE_RISE_BACK;
                return;
            case 12:
                work->state = ACTOR_110600_STATE_RISE;
                break;
        }
    }
}

static void func_actor_110600_80138AFC(Task* arg0)
{
    _Actor110600Work*   work;
    BossStrangerWalker* walker;
    Enemy*              enemy;
    u16                 ramp;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        arg0->extra.tmd->flags        = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->animRequest             = ACTOR_110600_ANIM_REQUEST_RESET;
        work->animId                  = 0xF;
        work->animRate                = work->baseRate;
        ramp                          = work->walker.speed;
        walker                        = &work->walker;
        work->walker.state            = BOSS_STRANGER_WALKER_IDLE;
        walker->speedTarget           = 0;
        walker->speed                 = ramp;
        walker->speedStep             = 8;
        work->walker.turnLimit        = 0;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
    }
    bossStrangerTick(&work->walker);
    func_actor_110600_80134728(arg0);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ACTOR_110600_STATE_CHASE;
    }
}

static void func_actor_110600_80138BD0(Task* arg0)
{
    _Actor110600Work*   work;
    BossStrangerWalker* walker;
    Enemy*              enemy;
    u16                 ramp;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        arg0->extra.tmd->flags        = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->animRequest             = ACTOR_110600_ANIM_REQUEST_RESET;
        work->animId                  = 0x10;
        work->animRate                = work->baseRate;
        ramp                          = work->walker.speed;
        walker                        = &work->walker;
        work->walker.state            = BOSS_STRANGER_WALKER_IDLE;
        walker->speedTarget           = 0;
        walker->speed                 = ramp;
        walker->speedStep             = 8;
        work->walker.turnLimit        = 0;
        work->lookYaw                 = 0;
        work->lookYawTarget           = 0;
    }
    bossStrangerTick(&work->walker);
    func_actor_110600_80134728(arg0);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ACTOR_110600_STATE_CHASE;
    }
}

static void func_actor_110600_80138CA4(Task* arg0)
{
    _Actor110600Work* work;
    Enemy*            enemy;
    TmdObject*        obj;
    s16               i;

    work = arg0->work;
    i    = 0;
    if (work->stateEntered != 0) {
        enemy                         = arg0->spawnArg2.pointer;
        obj                           = arg0->extra.tmd;
        obj->flags                    = 0;
        work->attackBody.flags        = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        work->gridBody.flags          = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->animId                  = 0x15;
        work->animRequest             = ACTOR_110600_ANIM_REQUEST_RESET;
        work->walker.turnLimit        = 0;
        work->lookYaw                 = 0;
        work->animRate                = 0x10;
        work->blendActive             = 0;
        func_actor_110600_80134728(arg0);
        work->stateFrame = 0;
        for (i = 0; i < 0x14; i++) {
            func_actor_110600_80134728(arg0);
        }
    }
    work->animRate = -8;
    func_actor_110600_80134728(arg0);
}

static void func_actor_110600_80138D7C(Task* arg0)
{
    _Actor110600Work* work;
    Enemy*            enemy;
    TmdObject*        obj;
    s16               step;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj                           = arg0->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        obj->flags                    = 0;
        Tmd_AllocBuffers(obj);
        work->animRequest       = ACTOR_110600_ANIM_REQUEST_RESET;
        work->animId            = 5;
        work->animRate          = 0x30;
        work->gridBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        func_actor_110600_80134728(arg0);
        func_actor_110600_80134728(arg0);
        return;
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    step                                  = (s16)work->animRate / 2;
    work->animRate                        = step;
    if (step == 1) {
        work->animRate = -0x10;
    }
    if (work->animRate == -1) {
        work->animRate = 0x10;
    }
    func_actor_110600_80134728(arg0);
    if (Gp_TickObjFlag2(enemy) == 1) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->state           = ACTOR_110600_STATE_CHASE;
    }
}

/// Runs the enemy task's current state handler from the actor's three-entry
/// table (spawn, per-frame tick, teardown), copying the table onto the stack
/// before the call.
void func_actor_110600_80138EA8(Task* task)
{
    EnemyTaskFuncTable3 sp;

    sp = D_actor_110600_80131FA0;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}
