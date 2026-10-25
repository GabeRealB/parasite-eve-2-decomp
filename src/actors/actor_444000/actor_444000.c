#include "actors/actor_04400.h"
#include "actors/actor_444000.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_transitions.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/player_state.h"
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
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/shelter_b3_garbage_incinerator.h"
#include "../../shared/actor/actor_contacts.h"
#include "../../shared/creatures/mad_chaser/mad_chaser.h"
/// Binds the shared shake helper to this instance's borrowed host task pointer.
#define GLUTTON_HOST_TASK (_gGluttonHostTask.task)

/// Selects garbage-incinerator behavior for this compiled Glutton instance.
///
/// Define before `glutton.h` and retain through every shared fragment. The
/// header defines `GLUTTON_INCINERATOR` as the dimensionless integer 2;
/// the binding must remain a macro for the shared code's `#if` comparisons.
#define GLUTTON_ROOM GLUTTON_INCINERATOR
/// Selects this package's public `void(s8 level)` screen-shake request setter.
///
/// Its interface is declared in `actors/actor_444000.h` for actor_341900 and
/// actor_342000. Bind before `glutton.h` and retain through the setter fragment;
/// `GLUTTON_HOST_TASK` selects the live host whose request byte it writes.
#define GLUTTON_SET_SHAKE_LEVEL actor444000GluttonSetShakeLevel
#include "../../shared/creatures/glutton/glutton.h"

/// `_Actor444000EventWork::playerAction`: the one-shot request the event script
/// hands the player.
///
/// The "event" clips are those of the package's own animation sets; the
/// "weapon" clip is clip 1 of the bank the equipped weapon selects.
enum {
    ACTOR_444000_PLAYER_ACTION_NONE                = 0, // Nothing pending
    ACTOR_444000_PLAYER_ACTION_WEAPON_CLIP_BLENDED = 1, // Blends into the weapon clip over ten frames
    ACTOR_444000_PLAYER_ACTION_EVENT_CLIP_3        = 2, // Cuts to event clip 3 and sounds the alert if it has not sounded yet
    ACTOR_444000_PLAYER_ACTION_EVENT_CLIP_0        = 3, // Locks the attachments for the event and blends into event clip 0 over ten frames
};

/// Incinerator-only states used by placement and command messages.
enum {
    ACTOR_444000_STATE_DORMANT           = 0,
    ACTOR_444000_STATE_RETURN_TO_ADVANCE = 17,
    ACTOR_444000_STATE_COLLAPSE          = 18,
    ACTOR_444000_STATE_COLLAPSED         = 19,
};

/// Incinerator handler slots used by root setup and attack selection.
enum {
    ACTOR_444000_STATE_IDLE          = 1,
    ACTOR_444000_STATE_THROW_DEBRIS  = 7,
    ACTOR_444000_STATE_CHOOSE_ATTACK = 10,
};

/// Commands in the Mine/Shelter garbage-incinerator namespace.
enum {
    ACTOR_444000_COMMAND_STOP          = 0,
    ACTOR_444000_COMMAND_REPOSITION    = 1,
    ACTOR_444000_COMMAND_SKIP_COLLAPSE = 19,
};

/// Presentation requests shared by the post-boss script and its skip path.
enum {
    ACTOR_444000_DESCENT_RESELECT_VIEW     = 0,
    ACTOR_444000_DESCENT_UPDATE_WITH_BLEND = 1,
    ACTOR_444000_DESCENT_UPDATE_SKIPPED    = 2,
};

/// Final arena phase and spatial actor-event values sent to the room task.
enum {
    ACTOR_444000_PHASE_LIFT                       = 6,
    ACTOR_444000_ROOM_EVENT_START_DESCENT         = 0,
    ACTOR_444000_ROOM_EVENT_BOSS_DIED_ON_APPROACH = 1,
    ACTOR_444000_ROOM_EVENT_BOSS_DIED_ON_LIFT     = 2,
};

/// Incinerator behavior slots used by this package's frame dispatcher.
enum {
    ACTOR_444000_STATE_LIMB_ANIMATION        = 5,
    ACTOR_444000_STATE_RESUME_LIMB_ANIMATION = 12,
    ACTOR_444000_STATE_CATCH_PLAYER          = 13,
    ACTOR_444000_STATE_LIFT_IDLE             = 16,
};

/// Synthetic Mad Chaser namespace, reset wall heights and caught-player control.
enum {
    ACTOR_444000_MAD_CHASER_CONTEXT_STAGE = 0,
    ACTOR_444000_MAD_CHASER_CONTEXT_AREA  = 44,
    ACTOR_444000_COMBAT_WALL_TOP_Y        = 500,
    ACTOR_444000_COMBAT_WALL_BOTTOM_Y     = 800,
    ACTOR_444000_PLAYER_SCRIPTED_IDLE     = 10,
};

/// Work block of the package's event task: the scene that follows the Glutton's
/// death in the garbage incinerator.
///
/// It is a separate, much smaller block than the enemy's `GluttonWork`. The
/// task's spawn state allocates it zeroed and keeps it at `Task::work` for the
/// task's life. The event script cannot be handed the task, so its callbacks
/// reach the block through the task pointer that state publishes.
///
/// The script drives the player by leaving a request in `playerAction`, which
/// the task performs on its next tick. The script and the script that replaces
/// it when the scene is skipped ask for the same one-time effects, which
/// `alertPlayed` and `combatReset` keep from happening twice.
typedef struct {
    byte  unknown_0[0x20];  // Zeroed allocation bytes; no access established and role unproven
    Task* player;           // The player task, the receiver of the event's model-draw and animation messages
    Task* framebufferBlend; // Framebuffer-blend effect task while one runs, else `NULL`
    s16   savedView;        // Session view slot captured as the script starts, which the script's callbacks select again
    u16   alertPlayed;      // Set once the alert has sounded, so the script and its skip path sound it once between them (0/1)
    u16   playerAction;     // Pending player request, cleared once performed (`ACTOR_444000_PLAYER_ACTION_NONE`, else one of `ACTOR_444000_PLAYER_ACTION_*`)
    u16   playerActionStep; // Step within the pending request, zeroed with every new one; no request of this package has steps, so nothing reads it
    u16   combatReset;      // Set once the scene's battle state has been wound down, which the script and its skip path each ask for (0/1)
    byte  unknown_32[2];    // Zeroed allocation bytes; no access established and role unproven
} _Actor444000EventWork;
STATIC_ASSERT_SIZEOF(_Actor444000EventWork, 0x34);

/// Scratch-stack block of the state that walks the host out along its facing,
/// swings it through a half turn and walks it back.
///
/// One block serves one tick of that state. Every tick uses `offset` for the
/// neck's yaw target; only the ticks of the half turn use the rest. The turn
/// rebuilds the root rotation about `yaw` each tick rather than spinning the
/// host in place: the working matrix is moved forward along its facing,
/// rotated, written back, and the host is then moved back along the new facing
/// by a slightly shorter step, so it swings round a point ahead of it. Yaws are
/// 4096 units per turn.
typedef struct {
    SVECTOR offset;        // Offset from the host's root to the player, whose yaw against the host's facing becomes the neck's yaw target; in the turn, the facing axis of `rootMatrix` scaled to the step forward, then that of the turned matrix scaled to the step back
    MATRIX  rootMatrix;    // Working copy of the host's root matrix for one tick of the turn; a state change seeds its rotation with identity, which the copy replaces before anything reads it
    byte    unknown_28[2]; // Never accessed; role unproven
    s16     yaw;           // Host's heading advanced by this tick's 0xD, the yaw the rotation is rebuilt about; set to 0x800 on the tick the half turn completes
} _Actor444000RunScratch;
STATIC_ASSERT_SIZEOF(_Actor444000RunScratch, 0x2C);

/// Scratch-stack block of the state the host runs once it has caught the
/// player: the range it holds the player at and the placement it gives them.
///
/// One block serves one tick of that state. The tick that enters the state
/// measures the player's range and, when they stand too close, moves them back
/// out along the line from the host; that tick and one later tick then place
/// the player ahead of the host's fifth part, turned to face it or to face
/// directly away, whichever is nearer the heading they already have. Yaws are
/// 4096 units per turn.
typedef struct {
    SVECTOR offset;           // Working vector: `playerDelta` scaled to length 0xCE4, the offset from the host's root the player is moved back out to; `anchor` minus the player's position and then the reverse, for the two bearings; the reverse scaled to length 0x384, the offset from `anchor` the player is placed at
    SVECTOR anchor;           // Origin of the host's fifth part, in the space the root coordinates share
    VECTOR  playerDelta;      // Player's position minus the host's on the ground plane, world units; only `vx` and `vz` are written or read
    byte    unknown_20[0x20]; // Never accessed; role unproven
    s32     playerDistance;   // Length of `playerDelta`; under 0xB54 the player is moved back out
    byte    unknown_44[0x6];  // Never accessed; role unproven
    s16     playerYaw;        // Yaw the player is placed at: the bearing from the player to `anchor`, or its reverse when the player faces more than a quarter turn off that bearing
} _Actor444000CatchScratch;
STATIC_ASSERT_SIZEOF(_Actor444000CatchScratch, 0x4C);

/// Scratch-stack block of the state in which the host drags the player toward
/// it.
///
/// One block serves one tick of that state. The tick measures the player from
/// the host's fifth part, turns that offset into a displacement toward the
/// part whose length the fight's phase and the animation's cue decide, and
/// leaves it with the player actor as its pending displacement. Lengths are
/// world units.
typedef struct {
    VECTOR3 displacement;     // Displacement handed to the player actor for this tick: `offset` on the ground plane, with `vy` zero
    byte    unknown_C[0x4];   // Never accessed; role unproven
    SVECTOR offset;           // Working vector: the host's root to the player, for the neck's yaw target; the origin of the host's fifth part; the player's position minus that origin, normalised and then scaled to the tick's pull, which is negative so that it points at the part
    byte    unknown_18[0x20]; // Never accessed; role unproven
    s32     playerDistance;   // Ground-plane length of `offset` from the fifth part to the player, before it is normalised; under 0x4B0 the host may catch the player
    byte    unknown_3C[0x8];  // Never accessed; role unproven
    s16     phasePull;        // Pull the fight's phase adds to the base 0x19 (0, 5, 10 or 15); the animation's cue takes a fraction or multiple of the sum
    s16     slot;             // Index into `GluttonWork::summons`, 0 or 1: counter of the loop that forgets both summons as the state ends
    s16     padScriptPeriod;  // Ticks between the pad scripts started over the pulling stretch of the animation (0x19, 0x11 or 0xE by phase)
    byte    unknown_4A[0x2];  // Never accessed; role unproven
} _Actor444000DragScratch;
STATIC_ASSERT_SIZEOF(_Actor444000DragScratch, 0x4C);

/// Storage for this Glutton instance's borrowed host task pointer.
typedef struct {
    Task* task;         // Main boss task; NULL until successful spawn, not cleared at teardown
    u8    unknown_4[4]; // Zero-initialized bytes with no established accesses; role unproven
} _GluttonHostTaskStorage;
STATIC_ASSERT_SIZEOF(_GluttonHostTaskStorage, 8);

/// Weapon class (1 is the class whose animations sit at the low base) and the
/// equipped-weapon index within it; together they pick the player animation the
/// action-1 cue installs.

/// 0xFF-terminated area-record list this overlay applies on entry.

/// Main-executable globals with no module header yet: `gDisplayState.pendingMode` gates the
/// event on the "everything is dead" state, and `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent` is the ending
/// selector the death sequence latches.

/// The room-owned table `gShelterB3GarbageIncineratorBlazeTaskDescs` starts the
/// countdown controller after the boss scene. The hand-off also copies
/// `gShelterB3GarbageIncineratorSuccessorSceneTiming.initialSceneClock` into
/// `GameSession::sceneClock` and clears `gShelterB3GarbageIncineratorDoorEventSuspended`.

/// Spawn tables `evsStartScriptWithSkip` forwards to `taskSpawn`, taken as raw
/// addresses: the first pair is used by the `spawnArg1` fast path in state 0
/// and the second by state 2.

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

/// Static storage for the command record the boss sends other actors.
///
/// `command` is the payload of `ACTOR_COMMAND_MESSAGE_APPLY`, lent to the
/// receiver for the length of each dispatch. One record serves every sender in
/// the package, each filling it in just before it sends: the announcements of
/// the fight's progress, which the scene manager forwards to its actors, and
/// the orders the boss gives each of its two summons directly. All of them use
/// the synthetic context of stage 0, area 44.
///
/// Four zero bytes separate the record from the next object. No access to
/// them is recovered, so whether they are trailing fields of this object or a
/// separate unreferenced variable is unproven; they stay in this allocation
/// only to keep the data after it at its address.
typedef struct {
    ActorCommand command;      // Record the receiver borrows; an announcement's command word is 0, 2 or 3, a summon's order a number in the high byte over a random one of 0x01, 0x11 and 0x21 in the low
    u8           unknown_4[4]; // Zero in the image; no access established and role unproven
} _Actor444000CommandStorage;
STATIC_ASSERT_SIZEOF(_Actor444000CommandStorage, 8);

/// Number of coordinates the debris launches rotate through.
enum { ACTOR_444000_DEBRIS_COORD_COUNT = 3 };

/// Static storage for the coordinates the boss's debris effects ride.
///
/// A debris launch takes the next coordinate in rotation, rebuilds it under
/// the view coordinate at the launch point and spawns its effect on it. The
/// effect is handed the coordinate itself rather than a copy.
///
/// 0x118 zero bytes separate the coordinates from the next object, which is
/// not a whole number of further coordinates. No access to them is recovered,
/// so whether they belong to this object or are separate unreferenced
/// variables is unproven; they stay in this allocation only to keep the data
/// after it at its address.
typedef struct {
    GfxCoord coords[ACTOR_444000_DEBRIS_COORD_COUNT]; // Parented to the view coordinate; each is rewritten whole by the launch that takes it
    u8       unknown_F0[0x118];                       // Zero in the image; no access established and role unproven
} _Actor444000DebrisCoordStorage;
STATIC_ASSERT_SIZEOF(_Actor444000DebrisCoordStorage, 0x208);

/// Static storage for the placement the host gives the player once it has
/// caught them.
///
/// `placement` is the payload of `GAME_ACTOR_MESSAGE_PLACE`, lent to the player
/// for the length of the dispatch, which consumes it. The catch fills it in
/// twice over: ahead of the host's fifth part with the yaw that faces the
/// player at the part or directly away from it, and, once the player's
/// scripted animation has stopped, at the host's own position with every angle
/// zero.
///
/// Eight zero bytes separate the record from the next object. No access to
/// them is recovered, so whether they are trailing fields of this object or a
/// separate unreferenced variable is unproven; they stay in this allocation
/// only to keep the data after it at its address.
typedef struct {
    ActorTransform placement;     // Record the player borrows; pitch and roll are always zero
    u8             unknown_18[8]; // Zero in the image; no access established and role unproven
} _Actor444000TransformStorage;
STATIC_ASSERT_SIZEOF(_Actor444000TransformStorage, 32);

/// Global game-mode byte; sits inside a small flag block, so it is declared as
/// an array -- the load has to keep aliasing the scratch stores beside it (see
/// DECOMPILATION_LEARNINGS.md, "Declare a fixed-address global as an array").

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

static void _actor444000RunArenaRouteState(Task* task);
static void _actor444000CollapseState(Task* task);
static void _actor444000BuildCornerWalls(Task* task, s32 distance, s16 faceIndex);
static void _actor444000Spawn(Enemy* enemy, Task* task);
static void _actor444000Tick(Enemy* enemy, Task* task);
static void _actor444000LiftIdleState(Task* task);
static void _actor444000ReturnToAdvanceState(Task* task);
static void _actor444000IdleState(Task* task);

static AnimationSet _gActor444000Animation21904;
static AnimationSet _gActor444000Animation21AE4;
static AnimationSet _gActor444000Animation21CC4;
static AnimationSet _gActor444000Animation220AC;
static AnimationSet _gActor444000Animation2246C;
static AnimationSet _gActor444000Animation22780;
static AnimationSet _gActor444000Animation22B64;
static AnimationSet _gActor444000Animation22FBC;
static AnimationSet _gActor444000Animation233E4;
static AnimationSet _gActor444000Animation23790;
static AnimationSet _gActor444000Animation23A98;
static AnimationSet _gActor444000Animation23D80;
static AnimationSet _gActor444000Animation23FE8;
static AnimationSet _gActor444000Animation240B4;
static AnimationSet _gActor444000Animation24180;
static AnimationSet _gActor444000Animation24458;
static AnimationSet _gActor444000Animation2469C;
static AnimationSet _gActor444000Animation248DC;
static AnimationSet _gActor444000Animation24AE4;
static AnimationSet _gActor444000Animation24C70;
static AnimationSet _gActor444000Animation24E04;
static AnimationSet _gActor444000Animation2523C;
static AnimationSet _gActor444000Animation25580;
static AnimationSet _gActor444000Animation25898;

static AnimationSet _gActor444000Animation26720;
static AnimationSet _gActor444000Animation269B4;
static AnimationSet _gActor444000Animation26CC0;
static AnimationSet _gActor444000Animation27D84;
static AnimationSet _gActor444000Animation28920;
static AnimationSet _gActor444000Animation28A28;
static AnimationSet _gActor444000Animation28B08;
static AnimationSet _gActor444000Animation28BE8;
static AnimationSet _gActor444000Animation28CF0;
static AnimationSet _gActor444000Animation28DB8;
static AnimationSet _gActor444000Animation28E80;
static AnimationSet _gActor444000Animation28F78;
static AnimationSet _gActor444000Animation29040;
static AnimationSet _gActor444000Animation29108;
static AnimationSet _gActor444000Animation29840;
static AnimationSet _gActor444000Animation2AC8C;
static AnimationSet _gActor444000Animation2C160;

static AnimationSet _gActor444000Animation2CB24;
static AnimationSet _gActor444000Animation2D330;
extern TmdSource    gGluttonLegRight;

static TmdSource _gGluttonGlob;
static TmdSource _gGluttonSpinner;
static TmdSource _gGluttonChunk;

extern TmdSource gGluttonBody;

static s32 _actor444000SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedSecondArg);
static s32 _actor444000ApplyCommand(Task* task, s32 messageId, const ActorCommand* request, s32 unusedSecondArg);
static s32 _actor444000IsPresent(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg);
static s32 _actor444000Place(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedSecondArg);
static s32 _actor444000HandleActorEvent(Task* task, s32 messageId, s32 event, s32 unusedSecondArg);
static s32 _actor444000SetDormantState(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg);

static void _actor444000EventPlayAlertOnce(void);
static void _actor444000EventSetPlayerModelDraw(s32 drawMode);
static void _actor444000EventStopFramebufferBlend(void);
static void _actor444000EventDismissMadChasers(void);
static void _actor444000EventBroadcastCommand(s16 command);
static void _actor444000EventEndBattleOnce(void);
static void _actor444000EventRequestPlayerAction(s16 action);

static void _actor444000GluttonTask(Task* task);

static void _actor444000EventUpdateDescentRoom(s32 updateMode);
static void _actor444000IncineratorEventTask(Task* task);

static AnimationPackedPose _gActor444000Animation124C4Bank1[6] = {
#include "assets/actor_444000_animation_124C4_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation124C4Bank4[46] = {
#include "assets/actor_444000_animation_124C4_bank4.inc"
};

static AnimationRecord _gActor444000Animation124C4Records[109] = {
#include "assets/actor_444000_animation_124C4_records.inc"
};

static u16 _gActor444000Animation124C4Indices[20] = {
#include "assets/actor_444000_animation_124C4_indices.inc"
};

static AnimationSet _gActor444000Animation124C4 = {
    _gActor444000Animation124C4Records,
    _gActor444000Animation124C4Indices,
    { NULL, _gActor444000Animation124C4Bank1, NULL, NULL, _gActor444000Animation124C4Bank4, NULL, NULL, NULL },
};

/// Player clips for the post-boss alert and lift-descent scene.
///
/// Clip 0 starts locked event playback; clip 3 accompanies the alert. The
/// player borrows these native sets through playback while the overlay lives.
static AnimationSet* _gActor444000EventPlayerAnimationSets[4] = {
    &_gActor444000Animation124C4,
    &gActor444000Animation2E548,
    &gActor444000Animation2EA80,
    &gActor444000Animation2EE14,
};

enum {
    ACTOR_444000_CAP_POST_BOSS_ALERT = 4,
    ACTOR_444000_CAP_DESCENT         = 8,
};

/// Post-boss incinerator alert and room-descent event, synchronized by CAP command 4.
///
/// Nineteen commands including END. Requires the published event task/work, live
/// player, incinerator room resources and CAP loaded. Dismisses the Mad Chasers,
/// restores player drawing, applies the descent room and ends player scripting.
/// The event task consumes the queued player actions while this script runs.
static EvsCommand _gActor444000PostBossAlertScript[19] = {
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_444000_CAP_POST_BOSS_ALERT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor444000EventRequestPlayerAction }, { .value = ACTOR_444000_PLAYER_ACTION_WEAPON_CLIP_BLENDED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor444000EventPlayAlertOnce }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor444000EventDismissMadChasers }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor444000EventSetPlayerModelDraw }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor444000EventSetPlayerModelDraw }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor444000EventUpdateDescentRoom }, { .value = ACTOR_444000_DESCENT_RESELECT_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor444000EventUpdateDescentRoom }, { .value = ACTOR_444000_DESCENT_UPDATE_WITH_BLEND }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor444000EventStopFramebufferBlend }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip path establishing the post-boss alert event's final room and player state.
///
/// Fourteen commands including END; borrows the same live event task and room
/// resources as the normal script. Fades across descent updates, dismisses the
/// Mad Chasers, clears blending, plays the alert once and ends player scripting.
static EvsCommand _gActor444000PostBossAlertSkipScript[14] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor444000EventDismissMadChasers }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor444000EventUpdateDescentRoom }, { .value = ACTOR_444000_DESCENT_UPDATE_SKIPPED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor444000EventStopFramebufferBlend }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor444000EventSetPlayerModelDraw }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor444000EventPlayAlertOnce }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Later-phase incinerator descent event, synchronized by CAP command 8.
///
/// Twenty-five commands including END. A nonzero event-task spawn argument selects
/// this path. Borrows its published event work, player, event clips and room/CAP
/// resources; ends combat once, plays the alert clip, updates the descent room
/// and releases player scripting. The event task advances queued player actions.
static EvsCommand _gActor444000DescentScript[25] = {
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_444000_CAP_DESCENT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor444000EventRequestPlayerAction }, { .value = ACTOR_444000_PLAYER_ACTION_EVENT_CLIP_0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor444000EventUpdateDescentRoom }, { .value = ACTOR_444000_DESCENT_RESELECT_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor444000EventEndBattleOnce }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor444000EventRequestPlayerAction }, { .value = ACTOR_444000_PLAYER_ACTION_EVENT_CLIP_3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor444000EventSetPlayerModelDraw }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor444000EventRequestPlayerAction }, { .value = ACTOR_444000_PLAYER_ACTION_WEAPON_CLIP_BLENDED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor444000EventSetPlayerModelDraw }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor444000EventUpdateDescentRoom }, { .value = ACTOR_444000_DESCENT_RESELECT_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor444000EventUpdateDescentRoom }, { .value = ACTOR_444000_DESCENT_UPDATE_WITH_BLEND }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor444000EventStopFramebufferBlend }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip path completing the later-phase collapse and incinerator descent.
///
/// Fifteen commands including END. Borrows the live event task and room resources.
/// Updates the room, broadcasts skip-collapse, clears blending, restores player
/// drawing, ends combat and plays the alert once before ending player scripting.
static EvsCommand _gActor444000DescentSkipScript[15] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor444000EventUpdateDescentRoom }, { .value = ACTOR_444000_DESCENT_UPDATE_SKIPPED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor444000EventBroadcastCommand }, { .value = ACTOR_444000_COMMAND_SKIP_COLLAPSE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor444000EventStopFramebufferBlend }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor444000EventSetPlayerModelDraw }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor444000EventEndBattleOnce }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor444000EventPlayAlertOnce }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc gActor444000IncineratorEventTaskDesc = {
    .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } },
    .callback = _actor444000IncineratorEventTask,
    .data     = { .value = 0 },
};

/// Contact attacks shared by the host and attached-part parameter records.
///
/// Power is packed through its low twelve bits before player HP scaling.
/// The thrown sphere poisons; the swipe applies berserker. Entry 0 has no
/// recovered selector and remains a zero-power category-4 attack if packed.
static DamageAttack _gActor444000GluttonAttacks[6] = {
    { 0, GAME_ACTOR_REACTION_ORDINARY },    // Unselected zero-power row
    { 30, GAME_ACTOR_REACTION_ORDINARY },   // Falling rain blob
    { 25, GAME_ACTOR_REACTION_POISON },     // Thrown sphere
    { 9999, GAME_ACTOR_REACTION_ORDINARY }, // Caught-player damage; packed power is 9999 & 0xFFF
    { 50, GAME_ACTOR_REACTION_BERSERKER },  // Limb swipe
    { 35, GAME_ACTOR_REACTION_ORDINARY },   // Healing-sequence debris chunk
};

/// Retained Glutton tuning with 3000 HP and rewards of 500 experience, 200 BP and 100 MP.
///
/// Borrows the host's contact-attack rows. No enemy installs this record;
/// the live host uses the separate 700-experience tuning.
static EnemyParams _gActor444000UnusedGluttonParams = {
    .attacks              = _gActor444000GluttonAttacks,
    .hpMax                = 3000,
    .exp                  = 500,
    .bp                   = 200,
    .mp                   = 100,
    .critChance           = 100,
    .buildupSteps         = 0,
    .damageOverTimeChance = 0,
    .damageOverTimeTicks  = 0,
};

/// Host combat parameters and release rewards for the incinerator encounter.
///
/// The host and targetable escorts start at 3000 HP; only the host borrows
/// this record. Release credits 700 experience, 200 BP and 100 MP.
static EnemyParams _gActor444000GluttonHostParams = {
    .attacks              = _gActor444000GluttonAttacks,
    .hpMax                = 3000,
    .exp                  = 700,
    .bp                   = 200,
    .mp                   = 100,
    .critChance           = 100,
    .buildupSteps         = 0,
    .damageOverTimeChance = 0,
    .damageOverTimeTicks  = 0,
};

/// Right-leg combat parameters and the 120-HP reaction pool for hit groups 3..5.
///
/// Escort 0 borrows this record but starts with the host's HP, rather than
/// hpMax. Emptying the separate pool triggers a reaction and refills it.
static EnemyParams _gActor444000GluttonRightLegParams = {
    .attacks              = _gActor444000GluttonAttacks,
    .hpMax                = 120,
    .exp                  = 0,
    .bp                   = 0,
    .mp                   = 0,
    .critChance           = 50,
    .buildupSteps         = 0,
    .damageOverTimeChance = 0,
    .damageOverTimeTicks  = 0,
};

/// Left-leg combat parameters and the 120-HP reaction pool for hit groups 6..8.
///
/// Escort 1 borrows this record but starts with the host's HP. The shared
/// incinerator hit handler refills its emptied reaction pool from hpMax.
static EnemyParams _gActor444000GluttonLeftLegParams = {
    .attacks              = _gActor444000GluttonAttacks,
    .hpMax                = 120,
    .exp                  = 0,
    .bp                   = 0,
    .mp                   = 0,
    .critChance           = 50,
    .buildupSteps         = 0,
    .damageOverTimeChance = 0,
    .damageOverTimeTicks  = 0,
};

/// Escort-3 combat parameters and the 200-HP pool for host hit groups 1..2.
///
/// The targetable attached part starts at the host's HP and borrows this
/// zero-reward record; hpMax seeds the separate host damage pool.
static EnemyParams _gActor444000GluttonGroups1To2Params = {
    .attacks              = _gActor444000GluttonAttacks,
    .hpMax                = 200,
    .exp                  = 0,
    .bp                   = 0,
    .mp                   = 0,
    .critChance           = 10,
    .buildupSteps         = 0,
    .damageOverTimeChance = 0,
    .damageOverTimeTicks  = 0,
};

/// Encounter-end latch shared by the host and its projectiles (0 running, 1 ended).
///
/// Boss death sets it; simultaneous player death keeps the boss alive and clears
/// it. Value 1 makes projectile states release caught players and tear down.
/// This signed-halfword latch is separate from the spinner-release word.
static s16 _gGluttonEncounterEnded = GLUTTON_ENCOUNTER_RUNNING;

/// Glob-hold latch (0 released, 1 following an accepted player hold).
///
/// Acceptance of the forty-press request sets it. A caught glob advances when
/// it is cleared; the engulf timeout also clears it. Dumping-hole player-release
/// broadcasts clear the same latch. The host's own catches use separate work flags.
static s32 _gGluttonGlobHoldActive = GLUTTON_GLOB_HOLD_RELEASED;

/// Total local-Z extension of the limb's five extending segments, in game units.
///
/// The pose driver floors this signed-halfword reach at 460 and assigns one
/// fifth to each of parts 2..6. Swipe states extend and retract it and select
/// curvature from its range. Unsigned-halfword arithmetic casts preserve wrapping
/// before the signed-halfword store.
static s16 _gGluttonLimbReach = 0;

/// Raw spinner-release word (1 starts waiting spinners, 0 cancels chasing ones).
///
/// Zero leaves a spinner still waiting visible. The setter preserves any s16
/// value; values other than 0 and 1 trigger neither specific test. Encounter end
/// tears down spinners independently of this word.
static s16 _gGluttonSpinnersReleased = GLUTTON_SPINNER_RELEASE_OFF;

PadScriptCmd gActor444000GluttonImpactPadCommands[2] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment gActor444000GluttonImpactPadSegments[2] = {
    { 0, 0, 7, 0 },
    { 255, 53, 27, 1 },
};

/// Simultaneous binary pulse and variable ramp at the limb-retraction cue.
///
/// Both lanes PLAY once and STOP at command 1; segment indices are 1 for
/// the binary lane and 0 for the variable lane. The script borrows both arrays.
static PadScriptCmd _gActor444000GluttonLimbRetractPadCommands[2] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

/// Limb-retraction rumble: variable intensity 186 toward 74 over 32 frames,
/// and a seven-frame binary pulse. The fourth byte's role is unproven.
static PadScriptVibrationSegment _gActor444000GluttonLimbRetractPadSegments[2] = {
    { 186, 74, 32, 1 },
    { 0, 0, 7, 0 },
};

/// Variable-motor-only inhale rumble, restarted at the phase's cue interval.
///
/// The binary lane immediately STOPs; the variable lane plays segments 0
/// and 1 in sequence, then STOPs at command 2. Tasks borrow both arrays.
static PadScriptCmd _gActor444000GluttonInhalePadCommands[3] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

/// Inhale rumble ramps: 22 toward 109 over 20 frames, then 80 toward 31
/// over 34 frames. Intensities are bytes; the fourth byte's role is unproven.
static PadScriptVibrationSegment _gActor444000GluttonInhalePadSegments[2] = {
    { 22, 109, 20, 1 },
    { 80, 31, 34, 1 },
};

#include "assets/enemies/glutton/body.inc"

#include "assets/enemies/glutton/actor_403200_model_12884.inc"

#include "assets/enemies/glutton/actor_403200_model_13774.inc"

#include "assets/enemies/glutton/leg_left.inc"

#include "assets/enemies/glutton/leg_right.inc"

#include "assets/enemies/glutton/limb.inc"

#include "assets/enemies/glutton/actor_403200_model_18BE4.inc"

#include "assets/enemies/glutton/glob.inc"

static TmdBone _gActor444000Model1C814Skeleton[1] = {
#include "assets/actor_444000_model_1C814_skeleton.inc"
};

static u32 _gActor444000Model1C814PartVerts[1] = {
#include "assets/actor_444000_model_1C814_partVerts.inc"
};

static SVECTOR _gActor444000Model1C814Verts[101] = {
#include "assets/actor_444000_model_1C814_verts.inc"
};

static SVECTOR _gActor444000Model1C814Normals[20] = {
#include "assets/actor_444000_model_1C814_normals.inc"
};

static u32 _gActor444000Model1C814Stream[465] = {
#include "assets/actor_444000_model_1C814_stream.inc"
};

TmdSource gActor444000Model1C814 = {
    0,
    3904,
    0,
    1,
    _gActor444000Model1C814PartVerts,
    _gActor444000Model1C814Verts,
    _gActor444000Model1C814Normals,
    _gActor444000Model1C814Skeleton,
    _gActor444000Model1C814Stream,
};

static TmdBone _gActor444000Model1D36CSkeleton[1] = {
#include "assets/actor_444000_model_1D36C_skeleton.inc"
};

static u32 _gActor444000Model1D36CPartVerts[1] = {
#include "assets/actor_444000_model_1D36C_partVerts.inc"
};

static SVECTOR _gActor444000Model1D36CVerts[101] = {
#include "assets/actor_444000_model_1D36C_verts.inc"
};

static SVECTOR _gActor444000Model1D36CNormals[20] = {
#include "assets/actor_444000_model_1D36C_normals.inc"
};

static u32 _gActor444000Model1D36CStream[461] = {
#include "assets/actor_444000_model_1D36C_stream.inc"
};

TmdSource gActor444000Model1D36C = {
    0,
    3904,
    0,
    1,
    _gActor444000Model1D36CPartVerts,
    _gActor444000Model1D36CVerts,
    _gActor444000Model1D36CNormals,
    _gActor444000Model1D36CSkeleton,
    _gActor444000Model1D36CStream,
};

static TmdBone _gActor444000Model1DC9CSkeleton[1] = {
#include "assets/actor_444000_model_1DC9C_skeleton.inc"
};

static u32 _gActor444000Model1DC9CPartVerts[1] = {
#include "assets/actor_444000_model_1DC9C_partVerts.inc"
};

static SVECTOR _gActor444000Model1DC9CVerts[54] = {
#include "assets/actor_444000_model_1DC9C_verts.inc"
};

static u32 _gActor444000Model1DC9CStream[209] = {
#include "assets/actor_444000_model_1DC9C_stream.inc"
};

TmdSource gActor444000Model1DC9C = {
    0,
    1576,
    0,
    1,
    _gActor444000Model1DC9CPartVerts,
    _gActor444000Model1DC9CVerts,
    &_gActor444000Model1DC9CVerts[54],
    _gActor444000Model1DC9CSkeleton,
    _gActor444000Model1DC9CStream,
};

static TmdBone _gActor444000Model1E14CSkeleton[1] = {
#include "assets/actor_444000_model_1E14C_skeleton.inc"
};

static u32 _gActor444000Model1E14CPartVerts[1] = {
#include "assets/actor_444000_model_1E14C_partVerts.inc"
};

static SVECTOR _gActor444000Model1E14CVerts[36] = {
#include "assets/actor_444000_model_1E14C_verts.inc"
};

static u32 _gActor444000Model1E14CStream[129] = {
#include "assets/actor_444000_model_1E14C_stream.inc"
};

TmdSource gActor444000Model1E14C = {
    0,
    944,
    0,
    1,
    _gActor444000Model1E14CPartVerts,
    _gActor444000Model1E14CVerts,
    &_gActor444000Model1E14CVerts[36],
    _gActor444000Model1E14CSkeleton,
    _gActor444000Model1E14CStream,
};

#include "assets/enemies/glutton/spinner.inc"

#include "assets/enemies/glutton/chunk.inc"

static AnimationPackedPose _gActor444000Animation20BD4Bank1[4] = {
#include "assets/actor_444000_animation_20BD4_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation20BD4Bank4[7] = {
#include "assets/actor_444000_animation_20BD4_bank4.inc"
};

static AnimationRecord _gActor444000Animation20BD4Records[38] = {
#include "assets/actor_444000_animation_20BD4_records.inc"
};

static u16 _gActor444000Animation20BD4Indices[8] = {
#include "assets/actor_444000_animation_20BD4_indices.inc"
};

AnimationSet gActor444000Animation20BD4 = {
    _gActor444000Animation20BD4Records,
    _gActor444000Animation20BD4Indices,
    { NULL, _gActor444000Animation20BD4Bank1, NULL, NULL, _gActor444000Animation20BD4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation20C80Bank1[4] = {
#include "assets/actor_444000_animation_20C80_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation20C80Bank4[1] = {
#include "assets/actor_444000_animation_20C80_bank4.inc"
};

static AnimationRecord _gActor444000Animation20C80Records[18] = {
#include "assets/actor_444000_animation_20C80_records.inc"
};

static u16 _gActor444000Animation20C80Indices[4] = {
#include "assets/actor_444000_animation_20C80_indices.inc"
};

AnimationSet gActor444000Animation20C80 = {
    _gActor444000Animation20C80Records,
    _gActor444000Animation20C80Indices,
    { NULL, _gActor444000Animation20C80Bank1, NULL, NULL, _gActor444000Animation20C80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation20D2CBank1[4] = {
#include "assets/actor_444000_animation_20D2C_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation20D2CBank4[1] = {
#include "assets/actor_444000_animation_20D2C_bank4.inc"
};

static AnimationRecord _gActor444000Animation20D2CRecords[18] = {
#include "assets/actor_444000_animation_20D2C_records.inc"
};

static u16 _gActor444000Animation20D2CIndices[4] = {
#include "assets/actor_444000_animation_20D2C_indices.inc"
};

AnimationSet gActor444000Animation20D2C = {
    _gActor444000Animation20D2CRecords,
    _gActor444000Animation20D2CIndices,
    { NULL, _gActor444000Animation20D2CBank1, NULL, NULL, _gActor444000Animation20D2CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation21014Bank1[7] = {
#include "assets/actor_444000_animation_21014_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation21014Bank4[49] = {
#include "assets/actor_444000_animation_21014_bank4.inc"
};

static AnimationRecord _gActor444000Animation21014Records[102] = {
#include "assets/actor_444000_animation_21014_records.inc"
};

static u16 _gActor444000Animation21014Indices[8] = {
#include "assets/actor_444000_animation_21014_indices.inc"
};

AnimationSet gActor444000Animation21014 = {
    _gActor444000Animation21014Records,
    _gActor444000Animation21014Indices,
    { NULL, _gActor444000Animation21014Bank1, NULL, NULL, _gActor444000Animation21014Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation212D8Bank1[29] = {
#include "assets/actor_444000_animation_212D8_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation212D8Bank4[17] = {
#include "assets/actor_444000_animation_212D8_bank4.inc"
};

static AnimationRecord _gActor444000Animation212D8Records[61] = {
#include "assets/actor_444000_animation_212D8_records.inc"
};

static u16 _gActor444000Animation212D8Indices[4] = {
#include "assets/actor_444000_animation_212D8_indices.inc"
};

AnimationSet gActor444000Animation212D8 = {
    _gActor444000Animation212D8Records,
    _gActor444000Animation212D8Indices,
    { NULL, _gActor444000Animation212D8Bank1, NULL, NULL, _gActor444000Animation212D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation215B4Bank1[30] = {
#include "assets/actor_444000_animation_215B4_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation215B4Bank4[18] = {
#include "assets/actor_444000_animation_215B4_bank4.inc"
};

static AnimationRecord _gActor444000Animation215B4Records[63] = {
#include "assets/actor_444000_animation_215B4_records.inc"
};

static u16 _gActor444000Animation215B4Indices[4] = {
#include "assets/actor_444000_animation_215B4_indices.inc"
};

AnimationSet gActor444000Animation215B4 = {
    _gActor444000Animation215B4Records,
    _gActor444000Animation215B4Indices,
    { NULL, _gActor444000Animation215B4Bank1, NULL, NULL, _gActor444000Animation215B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation21904Bank1[20] = {
#include "assets/actor_444000_animation_21904_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation21904Bank4[48] = {
#include "assets/actor_444000_animation_21904_bank4.inc"
};

static AnimationRecord _gActor444000Animation21904Records[90] = {
#include "assets/actor_444000_animation_21904_records.inc"
};

static u16 _gActor444000Animation21904Indices[8] = {
#include "assets/actor_444000_animation_21904_indices.inc"
};

static AnimationSet _gActor444000Animation21904 = {
    _gActor444000Animation21904Records,
    _gActor444000Animation21904Indices,
    { NULL, _gActor444000Animation21904Bank1, NULL, NULL, _gActor444000Animation21904Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation21AE4Bank1[18] = {
#include "assets/actor_444000_animation_21AE4_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation21AE4Bank4[13] = {
#include "assets/actor_444000_animation_21AE4_bank4.inc"
};

static AnimationRecord _gActor444000Animation21AE4Records[41] = {
#include "assets/actor_444000_animation_21AE4_records.inc"
};

static u16 _gActor444000Animation21AE4Indices[4] = {
#include "assets/actor_444000_animation_21AE4_indices.inc"
};

static AnimationSet _gActor444000Animation21AE4 = {
    _gActor444000Animation21AE4Records,
    _gActor444000Animation21AE4Indices,
    { NULL, _gActor444000Animation21AE4Bank1, NULL, NULL, _gActor444000Animation21AE4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation21CC4Bank1[18] = {
#include "assets/actor_444000_animation_21CC4_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation21CC4Bank4[13] = {
#include "assets/actor_444000_animation_21CC4_bank4.inc"
};

static AnimationRecord _gActor444000Animation21CC4Records[41] = {
#include "assets/actor_444000_animation_21CC4_records.inc"
};

static u16 _gActor444000Animation21CC4Indices[4] = {
#include "assets/actor_444000_animation_21CC4_indices.inc"
};

static AnimationSet _gActor444000Animation21CC4 = {
    _gActor444000Animation21CC4Records,
    _gActor444000Animation21CC4Indices,
    { NULL, _gActor444000Animation21CC4Bank1, NULL, NULL, _gActor444000Animation21CC4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation220ACBank1[18] = {
#include "assets/actor_444000_animation_220AC_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation220ACBank4[72] = {
#include "assets/actor_444000_animation_220AC_bank4.inc"
};

static AnimationRecord _gActor444000Animation220ACRecords[110] = {
#include "assets/actor_444000_animation_220AC_records.inc"
};

static u16 _gActor444000Animation220ACIndices[8] = {
#include "assets/actor_444000_animation_220AC_indices.inc"
};

static AnimationSet _gActor444000Animation220AC = {
    _gActor444000Animation220ACRecords,
    _gActor444000Animation220ACIndices,
    { NULL, _gActor444000Animation220ACBank1, NULL, NULL, _gActor444000Animation220ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation2246CBank1[39] = {
#include "assets/actor_444000_animation_2246C_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation2246CBank4[32] = {
#include "assets/actor_444000_animation_2246C_bank4.inc"
};

static AnimationRecord _gActor444000Animation2246CRecords[79] = {
#include "assets/actor_444000_animation_2246C_records.inc"
};

static u16 _gActor444000Animation2246CIndices[4] = {
#include "assets/actor_444000_animation_2246C_indices.inc"
};

static AnimationSet _gActor444000Animation2246C = {
    _gActor444000Animation2246CRecords,
    _gActor444000Animation2246CIndices,
    { NULL, _gActor444000Animation2246CBank1, NULL, NULL, _gActor444000Animation2246CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation22780Bank1[32] = {
#include "assets/actor_444000_animation_22780_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation22780Bank4[23] = {
#include "assets/actor_444000_animation_22780_bank4.inc"
};

static AnimationRecord _gActor444000Animation22780Records[66] = {
#include "assets/actor_444000_animation_22780_records.inc"
};

static u16 _gActor444000Animation22780Indices[4] = {
#include "assets/actor_444000_animation_22780_indices.inc"
};

static AnimationSet _gActor444000Animation22780 = {
    _gActor444000Animation22780Records,
    _gActor444000Animation22780Indices,
    { NULL, _gActor444000Animation22780Bank1, NULL, NULL, _gActor444000Animation22780Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation22B64Bank1[11] = {
#include "assets/actor_444000_animation_22B64_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation22B64Bank4[78] = {
#include "assets/actor_444000_animation_22B64_bank4.inc"
};

static AnimationRecord _gActor444000Animation22B64Records[124] = {
#include "assets/actor_444000_animation_22B64_records.inc"
};

static u16 _gActor444000Animation22B64Indices[8] = {
#include "assets/actor_444000_animation_22B64_indices.inc"
};

static AnimationSet _gActor444000Animation22B64 = {
    _gActor444000Animation22B64Records,
    _gActor444000Animation22B64Indices,
    { NULL, _gActor444000Animation22B64Bank1, NULL, NULL, _gActor444000Animation22B64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation22FBCBank1[47] = {
#include "assets/actor_444000_animation_22FBC_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation22FBCBank4[30] = {
#include "assets/actor_444000_animation_22FBC_bank4.inc"
};

static AnimationRecord _gActor444000Animation22FBCRecords[95] = {
#include "assets/actor_444000_animation_22FBC_records.inc"
};

static u16 _gActor444000Animation22FBCIndices[4] = {
#include "assets/actor_444000_animation_22FBC_indices.inc"
};

static AnimationSet _gActor444000Animation22FBC = {
    _gActor444000Animation22FBCRecords,
    _gActor444000Animation22FBCIndices,
    { NULL, _gActor444000Animation22FBCBank1, NULL, NULL, _gActor444000Animation22FBCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation233E4Bank1[46] = {
#include "assets/actor_444000_animation_233E4_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation233E4Bank4[27] = {
#include "assets/actor_444000_animation_233E4_bank4.inc"
};

static AnimationRecord _gActor444000Animation233E4Records[89] = {
#include "assets/actor_444000_animation_233E4_records.inc"
};

static u16 _gActor444000Animation233E4Indices[4] = {
#include "assets/actor_444000_animation_233E4_indices.inc"
};

static AnimationSet _gActor444000Animation233E4 = {
    _gActor444000Animation233E4Records,
    _gActor444000Animation233E4Indices,
    { NULL, _gActor444000Animation233E4Bank1, NULL, NULL, _gActor444000Animation233E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation23790Bank1[13] = {
#include "assets/actor_444000_animation_23790_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation23790Bank4[77] = {
#include "assets/actor_444000_animation_23790_bank4.inc"
};

static AnimationRecord _gActor444000Animation23790Records[105] = {
#include "assets/actor_444000_animation_23790_records.inc"
};

static u16 _gActor444000Animation23790Indices[8] = {
#include "assets/actor_444000_animation_23790_indices.inc"
};

static AnimationSet _gActor444000Animation23790 = {
    _gActor444000Animation23790Records,
    _gActor444000Animation23790Indices,
    { NULL, _gActor444000Animation23790Bank1, NULL, NULL, _gActor444000Animation23790Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation23A98Bank1[33] = {
#include "assets/actor_444000_animation_23A98_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation23A98Bank4[21] = {
#include "assets/actor_444000_animation_23A98_bank4.inc"
};

static AnimationRecord _gActor444000Animation23A98Records[62] = {
#include "assets/actor_444000_animation_23A98_records.inc"
};

static u16 _gActor444000Animation23A98Indices[4] = {
#include "assets/actor_444000_animation_23A98_indices.inc"
};

static AnimationSet _gActor444000Animation23A98 = {
    _gActor444000Animation23A98Records,
    _gActor444000Animation23A98Indices,
    { NULL, _gActor444000Animation23A98Bank1, NULL, NULL, _gActor444000Animation23A98Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation23D80Bank1[31] = {
#include "assets/actor_444000_animation_23D80_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation23D80Bank4[21] = {
#include "assets/actor_444000_animation_23D80_bank4.inc"
};

static AnimationRecord _gActor444000Animation23D80Records[60] = {
#include "assets/actor_444000_animation_23D80_records.inc"
};

static u16 _gActor444000Animation23D80Indices[4] = {
#include "assets/actor_444000_animation_23D80_indices.inc"
};

static AnimationSet _gActor444000Animation23D80 = {
    _gActor444000Animation23D80Records,
    _gActor444000Animation23D80Indices,
    { NULL, _gActor444000Animation23D80Bank1, NULL, NULL, _gActor444000Animation23D80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation23FE8Bank1[13] = {
#include "assets/actor_444000_animation_23FE8_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation23FE8Bank4[38] = {
#include "assets/actor_444000_animation_23FE8_bank4.inc"
};

static AnimationRecord _gActor444000Animation23FE8Records[63] = {
#include "assets/actor_444000_animation_23FE8_records.inc"
};

static u16 _gActor444000Animation23FE8Indices[8] = {
#include "assets/actor_444000_animation_23FE8_indices.inc"
};

static AnimationSet _gActor444000Animation23FE8 = {
    _gActor444000Animation23FE8Records,
    _gActor444000Animation23FE8Indices,
    { NULL, _gActor444000Animation23FE8Bank1, NULL, NULL, _gActor444000Animation23FE8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation240B4Bank1[6] = {
#include "assets/actor_444000_animation_240B4_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation240B4Bank4[5] = {
#include "assets/actor_444000_animation_240B4_bank4.inc"
};

static AnimationRecord _gActor444000Animation240B4Records[16] = {
#include "assets/actor_444000_animation_240B4_records.inc"
};

static u16 _gActor444000Animation240B4Indices[4] = {
#include "assets/actor_444000_animation_240B4_indices.inc"
};

static AnimationSet _gActor444000Animation240B4 = {
    _gActor444000Animation240B4Records,
    _gActor444000Animation240B4Indices,
    { NULL, _gActor444000Animation240B4Bank1, NULL, NULL, _gActor444000Animation240B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation24180Bank1[6] = {
#include "assets/actor_444000_animation_24180_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation24180Bank4[5] = {
#include "assets/actor_444000_animation_24180_bank4.inc"
};

static AnimationRecord _gActor444000Animation24180Records[16] = {
#include "assets/actor_444000_animation_24180_records.inc"
};

static u16 _gActor444000Animation24180Indices[4] = {
#include "assets/actor_444000_animation_24180_indices.inc"
};

static AnimationSet _gActor444000Animation24180 = {
    _gActor444000Animation24180Records,
    _gActor444000Animation24180Indices,
    { NULL, _gActor444000Animation24180Bank1, NULL, NULL, _gActor444000Animation24180Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation24458Bank1[8] = {
#include "assets/actor_444000_animation_24458_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation24458Bank4[49] = {
#include "assets/actor_444000_animation_24458_bank4.inc"
};

static AnimationRecord _gActor444000Animation24458Records[95] = {
#include "assets/actor_444000_animation_24458_records.inc"
};

static u16 _gActor444000Animation24458Indices[8] = {
#include "assets/actor_444000_animation_24458_indices.inc"
};

static AnimationSet _gActor444000Animation24458 = {
    _gActor444000Animation24458Records,
    _gActor444000Animation24458Indices,
    { NULL, _gActor444000Animation24458Bank1, NULL, NULL, _gActor444000Animation24458Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation2469CBank1[22] = {
#include "assets/actor_444000_animation_2469C_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation2469CBank4[12] = {
#include "assets/actor_444000_animation_2469C_bank4.inc"
};

static AnimationRecord _gActor444000Animation2469CRecords[55] = {
#include "assets/actor_444000_animation_2469C_records.inc"
};

static u16 _gActor444000Animation2469CIndices[4] = {
#include "assets/actor_444000_animation_2469C_indices.inc"
};

static AnimationSet _gActor444000Animation2469C = {
    _gActor444000Animation2469CRecords,
    _gActor444000Animation2469CIndices,
    { NULL, _gActor444000Animation2469CBank1, NULL, NULL, _gActor444000Animation2469CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation248DCBank1[23] = {
#include "assets/actor_444000_animation_248DC_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation248DCBank4[10] = {
#include "assets/actor_444000_animation_248DC_bank4.inc"
};

static AnimationRecord _gActor444000Animation248DCRecords[53] = {
#include "assets/actor_444000_animation_248DC_records.inc"
};

static u16 _gActor444000Animation248DCIndices[4] = {
#include "assets/actor_444000_animation_248DC_indices.inc"
};

static AnimationSet _gActor444000Animation248DC = {
    _gActor444000Animation248DCRecords,
    _gActor444000Animation248DCIndices,
    { NULL, _gActor444000Animation248DCBank1, NULL, NULL, _gActor444000Animation248DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation24AE4Bank1[9] = {
#include "assets/actor_444000_animation_24AE4_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation24AE4Bank4[33] = {
#include "assets/actor_444000_animation_24AE4_bank4.inc"
};

static AnimationRecord _gActor444000Animation24AE4Records[56] = {
#include "assets/actor_444000_animation_24AE4_records.inc"
};

static u16 _gActor444000Animation24AE4Indices[8] = {
#include "assets/actor_444000_animation_24AE4_indices.inc"
};

static AnimationSet _gActor444000Animation24AE4 = {
    _gActor444000Animation24AE4Records,
    _gActor444000Animation24AE4Indices,
    { NULL, _gActor444000Animation24AE4Bank1, NULL, NULL, _gActor444000Animation24AE4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation24C70Bank1[15] = {
#include "assets/actor_444000_animation_24C70_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation24C70Bank4[11] = {
#include "assets/actor_444000_animation_24C70_bank4.inc"
};

static AnimationRecord _gActor444000Animation24C70Records[31] = {
#include "assets/actor_444000_animation_24C70_records.inc"
};

static u16 _gActor444000Animation24C70Indices[4] = {
#include "assets/actor_444000_animation_24C70_indices.inc"
};

static AnimationSet _gActor444000Animation24C70 = {
    _gActor444000Animation24C70Records,
    _gActor444000Animation24C70Indices,
    { NULL, _gActor444000Animation24C70Bank1, NULL, NULL, _gActor444000Animation24C70Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation24E04Bank1[15] = {
#include "assets/actor_444000_animation_24E04_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation24E04Bank4[12] = {
#include "assets/actor_444000_animation_24E04_bank4.inc"
};

static AnimationRecord _gActor444000Animation24E04Records[32] = {
#include "assets/actor_444000_animation_24E04_records.inc"
};

static u16 _gActor444000Animation24E04Indices[4] = {
#include "assets/actor_444000_animation_24E04_indices.inc"
};

static AnimationSet _gActor444000Animation24E04 = {
    _gActor444000Animation24E04Records,
    _gActor444000Animation24E04Indices,
    { NULL, _gActor444000Animation24E04Bank1, NULL, NULL, _gActor444000Animation24E04Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation2523CBank1[22] = {
#include "assets/actor_444000_animation_2523C_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation2523CBank4[73] = {
#include "assets/actor_444000_animation_2523C_bank4.inc"
};

static AnimationRecord _gActor444000Animation2523CRecords[117] = {
#include "assets/actor_444000_animation_2523C_records.inc"
};

static u16 _gActor444000Animation2523CIndices[8] = {
#include "assets/actor_444000_animation_2523C_indices.inc"
};

static AnimationSet _gActor444000Animation2523C = {
    _gActor444000Animation2523CRecords,
    _gActor444000Animation2523CIndices,
    { NULL, _gActor444000Animation2523CBank1, NULL, NULL, _gActor444000Animation2523CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation25580Bank1[33] = {
#include "assets/actor_444000_animation_25580_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation25580Bank4[29] = {
#include "assets/actor_444000_animation_25580_bank4.inc"
};

static AnimationRecord _gActor444000Animation25580Records[69] = {
#include "assets/actor_444000_animation_25580_records.inc"
};

static u16 _gActor444000Animation25580Indices[4] = {
#include "assets/actor_444000_animation_25580_indices.inc"
};

static AnimationSet _gActor444000Animation25580 = {
    _gActor444000Animation25580Records,
    _gActor444000Animation25580Indices,
    { NULL, _gActor444000Animation25580Bank1, NULL, NULL, _gActor444000Animation25580Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation25898Bank1[31] = {
#include "assets/actor_444000_animation_25898_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation25898Bank4[26] = {
#include "assets/actor_444000_animation_25898_bank4.inc"
};

static AnimationRecord _gActor444000Animation25898Records[67] = {
#include "assets/actor_444000_animation_25898_records.inc"
};

static u16 _gActor444000Animation25898Indices[4] = {
#include "assets/actor_444000_animation_25898_indices.inc"
};

static AnimationSet _gActor444000Animation25898 = {
    _gActor444000Animation25898Records,
    _gActor444000Animation25898Indices,
    { NULL, _gActor444000Animation25898Bank1, NULL, NULL, _gActor444000Animation25898Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation25BE0Bank1[5] = {
#include "assets/actor_444000_animation_25BE0_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation25BE0Bank4[76] = {
#include "assets/actor_444000_animation_25BE0_bank4.inc"
};

static AnimationRecord _gActor444000Animation25BE0Records[105] = {
#include "assets/actor_444000_animation_25BE0_records.inc"
};

static u16 _gActor444000Animation25BE0Indices[8] = {
#include "assets/actor_444000_animation_25BE0_indices.inc"
};

AnimationSet gActor444000Animation25BE0 = {
    _gActor444000Animation25BE0Records,
    _gActor444000Animation25BE0Indices,
    { NULL, _gActor444000Animation25BE0Bank1, NULL, NULL, _gActor444000Animation25BE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation25F14Bank1[39] = {
#include "assets/actor_444000_animation_25F14_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation25F14Bank4[14] = {
#include "assets/actor_444000_animation_25F14_bank4.inc"
};

static AnimationRecord _gActor444000Animation25F14Records[62] = {
#include "assets/actor_444000_animation_25F14_records.inc"
};

static u16 _gActor444000Animation25F14Indices[4] = {
#include "assets/actor_444000_animation_25F14_indices.inc"
};

AnimationSet gActor444000Animation25F14 = {
    _gActor444000Animation25F14Records,
    _gActor444000Animation25F14Indices,
    { NULL, _gActor444000Animation25F14Bank1, NULL, NULL, _gActor444000Animation25F14Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation26240Bank1[38] = {
#include "assets/actor_444000_animation_26240_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation26240Bank4[15] = {
#include "assets/actor_444000_animation_26240_bank4.inc"
};

static AnimationRecord _gActor444000Animation26240Records[62] = {
#include "assets/actor_444000_animation_26240_records.inc"
};

static u16 _gActor444000Animation26240Indices[4] = {
#include "assets/actor_444000_animation_26240_indices.inc"
};

AnimationSet gActor444000Animation26240 = {
    _gActor444000Animation26240Records,
    _gActor444000Animation26240Indices,
    { NULL, _gActor444000Animation26240Bank1, NULL, NULL, _gActor444000Animation26240Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation26720Bank1[24] = {
#include "assets/actor_444000_animation_26720_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation26720Bank4[88] = {
#include "assets/actor_444000_animation_26720_bank4.inc"
};

static AnimationRecord _gActor444000Animation26720Records[138] = {
#include "assets/actor_444000_animation_26720_records.inc"
};

static u16 _gActor444000Animation26720Indices[8] = {
#include "assets/actor_444000_animation_26720_indices.inc"
};

static AnimationSet _gActor444000Animation26720 = {
    _gActor444000Animation26720Records,
    _gActor444000Animation26720Indices,
    { NULL, _gActor444000Animation26720Bank1, NULL, NULL, _gActor444000Animation26720Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation269B4Bank1[26] = {
#include "assets/actor_444000_animation_269B4_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation269B4Bank4[19] = {
#include "assets/actor_444000_animation_269B4_bank4.inc"
};

static AnimationRecord _gActor444000Animation269B4Records[56] = {
#include "assets/actor_444000_animation_269B4_records.inc"
};

static u16 _gActor444000Animation269B4Indices[4] = {
#include "assets/actor_444000_animation_269B4_indices.inc"
};

static AnimationSet _gActor444000Animation269B4 = {
    _gActor444000Animation269B4Records,
    _gActor444000Animation269B4Indices,
    { NULL, _gActor444000Animation269B4Bank1, NULL, NULL, _gActor444000Animation269B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation26CC0Bank1[30] = {
#include "assets/actor_444000_animation_26CC0_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation26CC0Bank4[26] = {
#include "assets/actor_444000_animation_26CC0_bank4.inc"
};

static AnimationRecord _gActor444000Animation26CC0Records[67] = {
#include "assets/actor_444000_animation_26CC0_records.inc"
};

static u16 _gActor444000Animation26CC0Indices[4] = {
#include "assets/actor_444000_animation_26CC0_indices.inc"
};

static AnimationSet _gActor444000Animation26CC0 = {
    _gActor444000Animation26CC0Records,
    _gActor444000Animation26CC0Indices,
    { NULL, _gActor444000Animation26CC0Bank1, NULL, NULL, _gActor444000Animation26CC0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation27D84Bank1[53] = {
#include "assets/actor_444000_animation_27D84_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation27D84Bank4[373] = {
#include "assets/actor_444000_animation_27D84_bank4.inc"
};

static AnimationRecord _gActor444000Animation27D84Records[521] = {
#include "assets/actor_444000_animation_27D84_records.inc"
};

static u16 _gActor444000Animation27D84Indices[20] = {
#include "assets/actor_444000_animation_27D84_indices.inc"
};

static AnimationSet _gActor444000Animation27D84 = {
    _gActor444000Animation27D84Records,
    _gActor444000Animation27D84Indices,
    { NULL, _gActor444000Animation27D84Bank1, NULL, NULL, _gActor444000Animation27D84Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation28920Bank1[36] = {
#include "assets/actor_444000_animation_28920_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation28920Bank4[255] = {
#include "assets/actor_444000_animation_28920_bank4.inc"
};

static AnimationRecord _gActor444000Animation28920Records[360] = {
#include "assets/actor_444000_animation_28920_records.inc"
};

static u16 _gActor444000Animation28920Indices[20] = {
#include "assets/actor_444000_animation_28920_indices.inc"
};

static AnimationSet _gActor444000Animation28920 = {
    _gActor444000Animation28920Records,
    _gActor444000Animation28920Indices,
    { NULL, _gActor444000Animation28920Bank1, NULL, NULL, _gActor444000Animation28920Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation28A28Bank1[4] = {
#include "assets/actor_444000_animation_28A28_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation28A28Bank4[10] = {
#include "assets/actor_444000_animation_28A28_bank4.inc"
};

static AnimationRecord _gActor444000Animation28A28Records[30] = {
#include "assets/actor_444000_animation_28A28_records.inc"
};

static u16 _gActor444000Animation28A28Indices[8] = {
#include "assets/actor_444000_animation_28A28_indices.inc"
};

static AnimationSet _gActor444000Animation28A28 = {
    _gActor444000Animation28A28Records,
    _gActor444000Animation28A28Indices,
    { NULL, _gActor444000Animation28A28Bank1, NULL, NULL, _gActor444000Animation28A28Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation28B08Bank1[5] = {
#include "assets/actor_444000_animation_28B08_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation28B08Bank4[8] = {
#include "assets/actor_444000_animation_28B08_bank4.inc"
};

static AnimationRecord _gActor444000Animation28B08Records[21] = {
#include "assets/actor_444000_animation_28B08_records.inc"
};

static u16 _gActor444000Animation28B08Indices[4] = {
#include "assets/actor_444000_animation_28B08_indices.inc"
};

static AnimationSet _gActor444000Animation28B08 = {
    _gActor444000Animation28B08Records,
    _gActor444000Animation28B08Indices,
    { NULL, _gActor444000Animation28B08Bank1, NULL, NULL, _gActor444000Animation28B08Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation28BE8Bank1[5] = {
#include "assets/actor_444000_animation_28BE8_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation28BE8Bank4[8] = {
#include "assets/actor_444000_animation_28BE8_bank4.inc"
};

static AnimationRecord _gActor444000Animation28BE8Records[21] = {
#include "assets/actor_444000_animation_28BE8_records.inc"
};

static u16 _gActor444000Animation28BE8Indices[4] = {
#include "assets/actor_444000_animation_28BE8_indices.inc"
};

static AnimationSet _gActor444000Animation28BE8 = {
    _gActor444000Animation28BE8Records,
    _gActor444000Animation28BE8Indices,
    { NULL, _gActor444000Animation28BE8Bank1, NULL, NULL, _gActor444000Animation28BE8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation28CF0Bank1[6] = {
#include "assets/actor_444000_animation_28CF0_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation28CF0Bank4[3] = {
#include "assets/actor_444000_animation_28CF0_bank4.inc"
};

static AnimationRecord _gActor444000Animation28CF0Records[31] = {
#include "assets/actor_444000_animation_28CF0_records.inc"
};

static u16 _gActor444000Animation28CF0Indices[8] = {
#include "assets/actor_444000_animation_28CF0_indices.inc"
};

static AnimationSet _gActor444000Animation28CF0 = {
    _gActor444000Animation28CF0Records,
    _gActor444000Animation28CF0Indices,
    { NULL, _gActor444000Animation28CF0Bank1, NULL, NULL, _gActor444000Animation28CF0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation28DB8Bank1[6] = {
#include "assets/actor_444000_animation_28DB8_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation28DB8Bank4[1] = {
#include "assets/actor_444000_animation_28DB8_bank4.inc"
};

static AnimationRecord _gActor444000Animation28DB8Records[19] = {
#include "assets/actor_444000_animation_28DB8_records.inc"
};

static u16 _gActor444000Animation28DB8Indices[4] = {
#include "assets/actor_444000_animation_28DB8_indices.inc"
};

static AnimationSet _gActor444000Animation28DB8 = {
    _gActor444000Animation28DB8Records,
    _gActor444000Animation28DB8Indices,
    { NULL, _gActor444000Animation28DB8Bank1, NULL, NULL, _gActor444000Animation28DB8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation28E80Bank1[6] = {
#include "assets/actor_444000_animation_28E80_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation28E80Bank4[1] = {
#include "assets/actor_444000_animation_28E80_bank4.inc"
};

static AnimationRecord _gActor444000Animation28E80Records[19] = {
#include "assets/actor_444000_animation_28E80_records.inc"
};

static u16 _gActor444000Animation28E80Indices[4] = {
#include "assets/actor_444000_animation_28E80_indices.inc"
};

static AnimationSet _gActor444000Animation28E80 = {
    _gActor444000Animation28E80Records,
    _gActor444000Animation28E80Indices,
    { NULL, _gActor444000Animation28E80Bank1, NULL, NULL, _gActor444000Animation28E80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation28F78Bank1[2] = {
#include "assets/actor_444000_animation_28F78_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation28F78Bank4[9] = {
#include "assets/actor_444000_animation_28F78_bank4.inc"
};

static AnimationRecord _gActor444000Animation28F78Records[33] = {
#include "assets/actor_444000_animation_28F78_records.inc"
};

static u16 _gActor444000Animation28F78Indices[8] = {
#include "assets/actor_444000_animation_28F78_indices.inc"
};

static AnimationSet _gActor444000Animation28F78 = {
    _gActor444000Animation28F78Records,
    _gActor444000Animation28F78Indices,
    { NULL, _gActor444000Animation28F78Bank1, NULL, NULL, _gActor444000Animation28F78Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation29040Bank1[4] = {
#include "assets/actor_444000_animation_29040_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation29040Bank4[5] = {
#include "assets/actor_444000_animation_29040_bank4.inc"
};

static AnimationRecord _gActor444000Animation29040Records[21] = {
#include "assets/actor_444000_animation_29040_records.inc"
};

static u16 _gActor444000Animation29040Indices[4] = {
#include "assets/actor_444000_animation_29040_indices.inc"
};

static AnimationSet _gActor444000Animation29040 = {
    _gActor444000Animation29040Records,
    _gActor444000Animation29040Indices,
    { NULL, _gActor444000Animation29040Bank1, NULL, NULL, _gActor444000Animation29040Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation29108Bank1[4] = {
#include "assets/actor_444000_animation_29108_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation29108Bank4[5] = {
#include "assets/actor_444000_animation_29108_bank4.inc"
};

static AnimationRecord _gActor444000Animation29108Records[21] = {
#include "assets/actor_444000_animation_29108_records.inc"
};

static u16 _gActor444000Animation29108Indices[4] = {
#include "assets/actor_444000_animation_29108_indices.inc"
};

static AnimationSet _gActor444000Animation29108 = {
    _gActor444000Animation29108Records,
    _gActor444000Animation29108Indices,
    { NULL, _gActor444000Animation29108Bank1, NULL, NULL, _gActor444000Animation29108Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation29840Bank1[3] = {
#include "assets/actor_444000_animation_29840_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation29840Bank4[203] = {
#include "assets/actor_444000_animation_29840_bank4.inc"
};

static AnimationRecord _gActor444000Animation29840Records[236] = {
#include "assets/actor_444000_animation_29840_records.inc"
};

static u16 _gActor444000Animation29840Indices[8] = {
#include "assets/actor_444000_animation_29840_indices.inc"
};

static AnimationSet _gActor444000Animation29840 = {
    _gActor444000Animation29840Records,
    _gActor444000Animation29840Indices,
    { NULL, _gActor444000Animation29840Bank1, NULL, NULL, _gActor444000Animation29840Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation2AC8CBank1[279] = {
#include "assets/actor_444000_animation_2AC8C_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation2AC8CBank4[73] = {
#include "assets/actor_444000_animation_2AC8C_bank4.inc"
};

static AnimationRecord _gActor444000Animation2AC8CRecords[377] = {
#include "assets/actor_444000_animation_2AC8C_records.inc"
};

static u16 _gActor444000Animation2AC8CIndices[4] = {
#include "assets/actor_444000_animation_2AC8C_indices.inc"
};

static AnimationSet _gActor444000Animation2AC8C = {
    _gActor444000Animation2AC8CRecords,
    _gActor444000Animation2AC8CIndices,
    { NULL, _gActor444000Animation2AC8CBank1, NULL, NULL, _gActor444000Animation2AC8CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation2C160Bank1[287] = {
#include "assets/actor_444000_animation_2C160_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation2C160Bank4[72] = {
#include "assets/actor_444000_animation_2C160_bank4.inc"
};

static AnimationRecord _gActor444000Animation2C160Records[388] = {
#include "assets/actor_444000_animation_2C160_records.inc"
};

static u16 _gActor444000Animation2C160Indices[4] = {
#include "assets/actor_444000_animation_2C160_indices.inc"
};

static AnimationSet _gActor444000Animation2C160 = {
    _gActor444000Animation2C160Records,
    _gActor444000Animation2C160Indices,
    { NULL, _gActor444000Animation2C160Bank1, NULL, NULL, _gActor444000Animation2C160Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation2C240Bank1[3] = {
#include "assets/actor_444000_animation_2C240_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation2C240Bank4[7] = {
#include "assets/actor_444000_animation_2C240_bank4.inc"
};

static AnimationRecord _gActor444000Animation2C240Records[26] = {
#include "assets/actor_444000_animation_2C240_records.inc"
};

static u16 _gActor444000Animation2C240Indices[8] = {
#include "assets/actor_444000_animation_2C240_indices.inc"
};

AnimationSet gActor444000Animation2C240 = {
    _gActor444000Animation2C240Records,
    _gActor444000Animation2C240Indices,
    { NULL, _gActor444000Animation2C240Bank1, NULL, NULL, _gActor444000Animation2C240Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation2C2CCBank1[3] = {
#include "assets/actor_444000_animation_2C2CC_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation2C2CCBank4[2] = {
#include "assets/actor_444000_animation_2C2CC_bank4.inc"
};

static AnimationRecord _gActor444000Animation2C2CCRecords[12] = {
#include "assets/actor_444000_animation_2C2CC_records.inc"
};

static u16 _gActor444000Animation2C2CCIndices[4] = {
#include "assets/actor_444000_animation_2C2CC_indices.inc"
};

AnimationSet gActor444000Animation2C2CC = {
    _gActor444000Animation2C2CCRecords,
    _gActor444000Animation2C2CCIndices,
    { NULL, _gActor444000Animation2C2CCBank1, NULL, NULL, _gActor444000Animation2C2CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation2C358Bank1[3] = {
#include "assets/actor_444000_animation_2C358_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation2C358Bank4[2] = {
#include "assets/actor_444000_animation_2C358_bank4.inc"
};

static AnimationRecord _gActor444000Animation2C358Records[12] = {
#include "assets/actor_444000_animation_2C358_records.inc"
};

static u16 _gActor444000Animation2C358Indices[4] = {
#include "assets/actor_444000_animation_2C358_indices.inc"
};

AnimationSet gActor444000Animation2C358 = {
    _gActor444000Animation2C358Records,
    _gActor444000Animation2C358Indices,
    { NULL, _gActor444000Animation2C358Bank1, NULL, NULL, _gActor444000Animation2C358Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation2CB24Bank1[14] = {
#include "assets/actor_444000_animation_2CB24_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation2CB24Bank4[158] = {
#include "assets/actor_444000_animation_2CB24_bank4.inc"
};

static AnimationRecord _gActor444000Animation2CB24Records[279] = {
#include "assets/actor_444000_animation_2CB24_records.inc"
};

static u16 _gActor444000Animation2CB24Indices[20] = {
#include "assets/actor_444000_animation_2CB24_indices.inc"
};

static AnimationSet _gActor444000Animation2CB24 = {
    _gActor444000Animation2CB24Records,
    _gActor444000Animation2CB24Indices,
    { NULL, _gActor444000Animation2CB24Bank1, NULL, NULL, _gActor444000Animation2CB24Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation2D330Bank1[15] = {
#include "assets/actor_444000_animation_2D330_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation2D330Bank4[206] = {
#include "assets/actor_444000_animation_2D330_bank4.inc"
};

static AnimationRecord _gActor444000Animation2D330Records[244] = {
#include "assets/actor_444000_animation_2D330_records.inc"
};

static u16 _gActor444000Animation2D330Indices[20] = {
#include "assets/actor_444000_animation_2D330_indices.inc"
};

static AnimationSet _gActor444000Animation2D330 = {
    _gActor444000Animation2D330Records,
    _gActor444000Animation2D330Indices,
    { NULL, _gActor444000Animation2D330Bank1, NULL, NULL, _gActor444000Animation2D330Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation2E198Bank1[39] = {
#include "assets/actor_444000_animation_2E198_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation2E198Bank4[353] = {
#include "assets/actor_444000_animation_2E198_bank4.inc"
};

static AnimationRecord _gActor444000Animation2E198Records[432] = {
#include "assets/actor_444000_animation_2E198_records.inc"
};

static u16 _gActor444000Animation2E198Indices[20] = {
#include "assets/actor_444000_animation_2E198_indices.inc"
};

AnimationSet gActor444000Animation2E198 = {
    _gActor444000Animation2E198Records,
    _gActor444000Animation2E198Indices,
    { NULL, _gActor444000Animation2E198Bank1, NULL, NULL, _gActor444000Animation2E198Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation2E548Bank1[6] = {
#include "assets/actor_444000_animation_2E548_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation2E548Bank4[73] = {
#include "assets/actor_444000_animation_2E548_bank4.inc"
};

static AnimationRecord _gActor444000Animation2E548Records[125] = {
#include "assets/actor_444000_animation_2E548_records.inc"
};

static u16 _gActor444000Animation2E548Indices[20] = {
#include "assets/actor_444000_animation_2E548_indices.inc"
};

AnimationSet gActor444000Animation2E548 = {
    _gActor444000Animation2E548Records,
    _gActor444000Animation2E548Indices,
    { NULL, _gActor444000Animation2E548Bank1, NULL, NULL, _gActor444000Animation2E548Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation2EA80Bank1[10] = {
#include "assets/actor_444000_animation_2EA80_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation2EA80Bank4[117] = {
#include "assets/actor_444000_animation_2EA80_bank4.inc"
};

static AnimationRecord _gActor444000Animation2EA80Records[167] = {
#include "assets/actor_444000_animation_2EA80_records.inc"
};

static u16 _gActor444000Animation2EA80Indices[20] = {
#include "assets/actor_444000_animation_2EA80_indices.inc"
};

AnimationSet gActor444000Animation2EA80 = {
    _gActor444000Animation2EA80Records,
    _gActor444000Animation2EA80Indices,
    { NULL, _gActor444000Animation2EA80Bank1, NULL, NULL, _gActor444000Animation2EA80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor444000Animation2EE14Bank1[7] = {
#include "assets/actor_444000_animation_2EE14_bank1.inc"
};

static AnimationPackedRotation _gActor444000Animation2EE14Bank4[65] = {
#include "assets/actor_444000_animation_2EE14_bank4.inc"
};

static AnimationRecord _gActor444000Animation2EE14Records[123] = {
#include "assets/actor_444000_animation_2EE14_records.inc"
};

static u16 _gActor444000Animation2EE14Indices[20] = {
#include "assets/actor_444000_animation_2EE14_indices.inc"
};

AnimationSet gActor444000Animation2EE14 = {
    _gActor444000Animation2EE14Records,
    _gActor444000Animation2EE14Indices,
    { NULL, _gActor444000Animation2EE14Bank1, NULL, NULL, _gActor444000Animation2EE14Bank4, NULL, NULL, NULL },
};

/// Blend duration in animation frames, indexed by previous and requested clip.
///
/// Both indices must be in 0..44; the requested clip must exist in all three
/// driving rigs. Zero seeks immediately; positive values blend for that duration.
/// Each compiled encounter owns its matrix and borrows it for its lifetime.
static s8 _gGluttonAnimBlendFrames[GLUTTON_ANIM_BLEND_CLIP_COUNT][GLUTTON_ANIM_BLEND_CLIP_COUNT] = {
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
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
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Incinerator host clip bank shared by its driving and blending rigs.
///
/// Live clips occupy 1..5 and 7..19; clip 19 repeats clip 11.
/// The remaining stored words are zero. No read beyond clip 19 establishes
/// the original table's end; the existing allocation is retained.
/// Native sets live with the overlay.
static AnimationSet* _gActor444000GluttonHostAnimationSets[46] = {
    NULL,
    &gActor444000Animation20BD4,
    &gActor444000Animation21014,
    &_gActor444000Animation21904,
    &_gActor444000Animation22B64,
    &_gActor444000Animation23790,
    NULL,
    &_gActor444000Animation28A28,
    &_gActor444000Animation28CF0,
    &_gActor444000Animation28F78,
    &gActor444000Animation2C240,
    &_gActor444000Animation220AC,
    &_gActor444000Animation23FE8,
    &gActor444000Animation25BE0,
    &_gActor444000Animation24458,
    &_gActor444000Animation24AE4,
    &_gActor444000Animation2523C,
    &_gActor444000Animation26720,
    &_gActor444000Animation29840,
    &_gActor444000Animation220AC,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

/// Right-leg clip bank for escort 0's driving and blending rigs.
///
/// Live clip indices follow the host bank, with clip 19 repeating clip 11.
/// Other stored words are zero; no read establishes the original table's end
/// beyond clip 19. Both rigs borrow the table and native sets.
static AnimationSet* _gActor444000GluttonRightLegAnimationSets[46] = {
    NULL,
    &gActor444000Animation20C80,
    &gActor444000Animation212D8,
    &_gActor444000Animation21AE4,
    &_gActor444000Animation22FBC,
    &_gActor444000Animation23A98,
    NULL,
    &_gActor444000Animation28B08,
    &_gActor444000Animation28DB8,
    &_gActor444000Animation29040,
    &gActor444000Animation2C2CC,
    &_gActor444000Animation2246C,
    &_gActor444000Animation240B4,
    &gActor444000Animation25F14,
    &_gActor444000Animation2469C,
    &_gActor444000Animation24C70,
    &_gActor444000Animation25580,
    &_gActor444000Animation269B4,
    &_gActor444000Animation2AC8C,
    &_gActor444000Animation2246C,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

/// Left-leg clip bank for escort 1's driving and blending rigs.
///
/// Live clip indices follow the host bank, with clip 19 repeating clip 11.
/// Other stored words are zero; no read establishes the original table's end
/// beyond clip 19. Both rigs borrow the table and native sets.
static AnimationSet* _gActor444000GluttonLeftLegAnimationSets[46] = {
    NULL,
    &gActor444000Animation20D2C,
    &gActor444000Animation215B4,
    &_gActor444000Animation21CC4,
    &_gActor444000Animation233E4,
    &_gActor444000Animation23D80,
    NULL,
    &_gActor444000Animation28BE8,
    &_gActor444000Animation28E80,
    &_gActor444000Animation29108,
    &gActor444000Animation2C358,
    &_gActor444000Animation22780,
    &_gActor444000Animation24180,
    &gActor444000Animation26240,
    &_gActor444000Animation248DC,
    &_gActor444000Animation24E04,
    &_gActor444000Animation25898,
    &_gActor444000Animation26CC0,
    &_gActor444000Animation2C160,
    &_gActor444000Animation22780,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

enum { ACTOR_444000_PLAYER_CAPTURE_FACING_AWAY_BASE = 4 };

/// Player capture clips for the host's inhale catch and limb swipe.
///
/// The forward view starts at 0, the facing-away view at 4. Each catch view
/// supplies clip 1; clip 2 is the swipe hold. Swipe recovery overwrites entry
/// 4 with equipped-weapon set 7 and plays it as forward-view clip 4, so the
/// views share mutable storage. Playback borrows the table and native sets.
static AnimationSet* _gActor444000GluttonPlayerCaptureAnimationSets[8] = {
    NULL,
    &_gActor444000Animation28920,
    &_gActor444000Animation2D330,
    NULL,
    NULL,
    &_gActor444000Animation27D84,
    &_gActor444000Animation2D330,
    NULL,
};

/// Landing-pattern group shared by one sequence of eight rain blobs.
///
/// Launch index 0 rerolls it: groups 0, 1 and 2 have probabilities 1/4, 1/2
/// and 1/4. The remaining launches retain it until the next sequence begins.
static u8 _gGluttonRainGroup = 0;

/// Player animation bank for a glob engulf and its release.
///
/// Slot 1 holds the native caught clip; slot 2 is refreshed from equipped-weapon
/// bank clip 9 before release. Other slots are NULL. The mutable seven-slot bank
/// remains live while the player borrows it for playback.
static AnimationSet* _gGluttonGlobPlayerAnimSets[GLUTTON_GLOB_PLAYER_ANIM_SET_COUNT] = {
    NULL,
    &_gActor444000Animation2CB24,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

static void      _gluttonEscort6Task(Task* task);
static void      _gluttonPropTask(Task* task);
extern TmdSource gGluttonLegLeft;
extern TmdSource gGluttonActor403200Model12884;
extern TmdSource gGluttonActor403200Model13774;
static TmdSource _gGluttonLimb;
extern TmdSource gGluttonActor403200Model18BE4;
static TmdSource _gActor444000GluttonEscort6ModelSource;

/// Recipes indexed by the host's seven attached-part slots.
///
/// Slots 0/1 are the animated right/left legs; 2..5 are additional attached
/// models. Slot 6 is spawned only for a zero high spawn-argument halfword;
/// both room placements suppress it. Its zero-filled source has no recovered
/// initialization. All seven callbacks have the same prop-task lifecycle.
static TaskDesc _gActor444000GluttonAttachedTaskDescs[7] = {
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _gluttonPropTask, { .model = &gGluttonLegRight } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _gluttonPropTask, { .model = &gGluttonLegLeft } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _gluttonPropTask, { .model = &gGluttonActor403200Model12884 } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _gluttonPropTask, { .model = &gGluttonActor403200Model13774 } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _gluttonPropTask, { .model = &_gGluttonLimb } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _gluttonPropTask, { .model = &gGluttonActor403200Model18BE4 } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _gluttonEscort6Task, { .model = &_gActor444000GluttonEscort6ModelSource } },
};

/// Launch translations for the eight rain blobs, indexed by spawnArg1's low half.
///
/// XYZ use whole game-coordinate units in the view parent's frame. X and Z
/// are added to the host root; Y is an absolute height. No host rotation is
/// applied. All Y and pad components are zero.
static SVECTOR _gGluttonRainLaunchOffsets[GLUTTON_RAIN_BLOB_COUNT] = {
    { -1000, 0, -1800, 0 },
    { 800, 0, -800, 0 },
    { -1300, 0, 200, 0 },
    { 1200, 0, 1000, 0 },
    { 900, 0, -1700, 0 },
    { -800, 0, -880, 0 },
    { 1280, 0, 0, 0 },
    { -1100, 0, 900, 0 },
};

/// Pattern-space landing points selected by the rain group's index row.
///
/// Components use whole game-coordinate units: vz offsets the distance-led
/// arena X, while vx minus 6300 plus jitter supplies arena Z. Y and pad are
/// unused zeros. Index 15 is an unselected zero entry; its role and membership
/// in the original table are unproven, so the stored extent is retained.
static SVECTOR _gGluttonRainPoints[GLUTTON_RAIN_POINT_COUNT] = {
    { -2000, 0, -1800, 0 },
    { -1200, 0, -1900, 0 },
    { -80, 0, -1880, 0 },
    { 990, 0, -1790, 0 },
    { 1900, 0, -1650, 0 },
    { -1880, 0, -100, 0 },
    { -1000, 0, 150, 0 },
    { 80, 0, 80, 0 },
    { 1090, 0, -90, 0 },
    { 2100, 0, 50, 0 },
    { -1900, 0, 1100, 0 },
    { -900, 0, -1150, 0 },
    { 0, 0, -1800, 0 },
    { 1290, 0, 1900, 0 },
    { 1700, 0, 1500, 0 },
    { 0, 0, 0, 0 },
};

/// Landing-point indices for each group and rain-blob launch index.
///
/// Rows 0..2 each supply eight indices into `_gGluttonRainPoints`, all 0..14.
/// Launch 4 still samples its entry before replacing the landing X/Z with the
/// player's current position. Both indices are unchecked.
static u8 _gGluttonRainPointIndex[GLUTTON_RAIN_GROUP_COUNT][GLUTTON_RAIN_BLOB_COUNT] = {
    { 7, 10, 8, 0, 4, 3, 12, 9 },
    { 2, 5, 12, 0, 6, 14, 13, 10 },
    { 14, 13, 9, 0, 12, 7, 10, 11 },
};

/// Spawn recipes for the encounter's attack projectiles and healing chunks.
///
/// Indices 0..3 select glob, rain blob, thrown sphere and healing chunk. Rain
/// and thrown-sphere tasks have coordinate bodies; the other tasks have TMD
/// bodies. Dumping-hole slot 4 is a spinner whose model is patched per spawn;
/// the incinerator owns its spinner descriptor separately. Callbacks are private
/// instances and each spawned task owns its body and work until teardown.
static TaskDesc _gGluttonProjectileTaskDescs[GLUTTON_PROJECTILE_TASK_COUNT] = {
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _gluttonGlobTask, { .model = &_gGluttonGlob } },
    { { { TASK_BODY_COORD, TASK_PRIORITY_ACTOR } }, _gluttonRainTask, { .value = 0 } },
    { { { TASK_BODY_COORD, TASK_PRIORITY_ACTOR } }, _gluttonThrowTask, { .value = 0 } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _gluttonChunkTask, { .model = &_gGluttonChunk } },
};

/// Unspawned recipe for a Glutton floor spinner with its own TMD body.
///
/// Entry 0 requires a live Enemy in spawnArg2 and a delay class in the low
/// half of spawnArg1 (0 short, 1 medium, otherwise long). Model geometry and
/// callbacks belong to this overlay and must outlive the task.
static TaskDesc _gActor444000UnusedGluttonSpinnerTaskDesc = {
    .header   = { .fields = { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } },
    .callback = _gluttonSpinnerTask,
    .data     = { .model = &_gGluttonSpinner },
};

/// Host message dispatch for drawing, presence, placement, commands and room events.
///
/// Six live entries end with `TASK_MESSAGE_TABLE_END`. The host borrows the
/// table until teardown; pointer payloads are consumed during dispatch.
static TaskMessageEntry _gActor444000GluttonMessages[7] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor444000SetModelDraw },
    { ACTOR_MESSAGE_IS_PRESENT, _actor444000IsPresent },
    { ACTOR_MESSAGE_PLACE, _actor444000Place },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor444000ApplyCommand },
    { ROOM_MESSAGE_ACTOR_EVENT, _actor444000HandleActorEvent },
    { SCENE_MESSAGE_EXIT_PLACED_ACTORS, _actor444000SetDormantState },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Last debris-effect coordinate selected, advanced modulo the three live nodes.
///
/// Starts at 0, so the first launch takes node 1. The signed-halfword index
/// remains in 0..2 and persists between host states and spawns in this overlay.
static s16 _gActor444000GluttonDebrisCoordIndex = 0;

TaskDesc gActor444000GluttonHostTaskDesc = { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _actor444000GluttonTask, { .model = &gGluttonBody } };

/// Borrowed live post-boss event task used by this package's script callbacks.
///
/// `NULL` until event spawn publishes it; its work is `_Actor444000EventWork`.
/// Teardown does not clear it, so callbacks require the live event.
static Task* _gActor444000IncineratorEventTask = NULL;

/// Unreferenced initialized word; its role and encoding are unproven.
static u32 D_actor_444000_80161864 = 0x1A90C60D;

/// Catch progress latch: zero for part-relative placement, one after root placement.
///
/// Only writes are recovered; the purpose of retaining this condition is
/// unproven. The seven trailing zero bytes remain unexplained.
static GluttonPlayerAtHostStorage _gActor444000GluttonPlayerAtHost = { 0, { 0, 0, 0, 0, 0, 0, 0 } };

SVECTOR ActorContact_ScratchPosition = { 0, 0, 0, 0 };

/// Borrowed host task reference for screen-shake requests while the boss lives.
static _GluttonHostTaskStorage _gGluttonHostTask = { NULL, { 0 } };

/// Effect spawn record initialized to the live host's root coordinate.
///
/// Host spawn stores argument halves 0x100 and 2. No reader or effect using
/// this record is recovered; its intended effect and retention are unproven.
static EffectSpawnArg _gActor444000GluttonRootEffectSpawnArg = { NULL, 0, 0 };

/// Reusable Mad Chaser command payload for scene broadcasts and individual summons.
///
/// Uses synthetic stage 0, area 44: command kinds 0/2/3 announce no command,
/// pull and vanish; emergence packs the room spot in bits 8..11 and move
/// 0..2 in bits 4..7. Receivers consume it synchronously. The four trailing
/// zero bytes have no established access or object boundary.
static _Actor444000CommandStorage _gActor444000GluttonMadChaserCommand = { { { .loc = { 0, 0 } }, 0 }, { 0, 0, 0, 0 } };

/// Spinner chase point in the spinner roots' parent-coordinate frame.
///
/// Components narrow to signed-halfword game units; pad is unused. Inhale
/// refreshes it from part 3's origin, on the host in the dumping hole or placed
/// actor 0 in the incinerator. The shared chase subtracts each root translation.
static SVECTOR _gGluttonSpinnerTarget = { 0, 0, 0, 0 };

/// Forty-press player-hold request used by the glob engulf.
///
/// Only hold.pressCount is filled; the player copies it during synchronous
/// dispatch. The eight trailing zero bytes have no established access or
/// original object boundary and remain in the allocation.
static GluttonButtonPressHoldStorage _gGluttonGlobEngulfHold = { { { 0 }, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0 } };

/// Mouth-relative node rebuilt at clip 9 tick 45 with local offset (0,-100,100).
///
/// Its parent is host part 4 and its rotation is identity. No recovered
/// consumer uses the composed node afterwards, so its purpose is unproven.
static GfxCoord _gActor444000GluttonMouthCoord = { 0, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL };

/// Placement payload lent to the caught player through synchronous dispatch.
///
/// Holds either the mouth-relative world placement and facing yaw, or the
/// host root position with zero angles. Positions use game units, yaws 4096
/// per turn. The eight trailing zero bytes remain unexplained.
static _Actor444000TransformStorage _gActor444000GluttonCaughtPlayerPlacement = { { { 0, 0, 0, 0 }, { 0, 0, 0, 0 } }, { 0, 0, 0, 0, 0, 0, 0, 0 } };

/// Zero-press player hold requested before the host's inhale catch or swipe.
///
/// Dispatch tests recovery and, on reply zero, takes scripted control; the
/// hold ends on its first tick because `pressCount` remains zero. The player
/// copies the request during dispatch. Eight trailing zero bytes are unproven.
static GluttonButtonPressHoldStorage _gActor444000GluttonGrabHold = { { { 0 }, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0 } };

/// Three persistent coordinates borrowed by debris effect tasks.
///
/// Each launch rewrites the next node from right-leg part 1's world pose,
/// parents it to the view and offsets it by 800 game units. Spawn snapshots
/// placement; the effect's later initializer borrows the node's rotation for
/// its velocity. Nodes are reused every third launch. The 0x118 trailing
/// zero bytes remain unexplained.
static _Actor444000DebrisCoordStorage _gActor444000GluttonDebrisCoords = { { { 0, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL }, { 0, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL }, { 0, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, { 0 } };

/// Zero-filled model source selected by attached-part descriptor 6.
///
/// Both room placements suppress this optional escort. No initialization is
/// recovered; the zero part count and NULL stream cannot supply a live model
/// for the retained spawn branch.
static TmdSource _gActor444000GluttonEscort6ModelSource = { 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL };

static void        _actor444000EventPerformPlayerAction(Task* task);
static inline void _actor444000StepForward(GfxCoord* coord);
static inline void _actor444000FlattenRoot(Task* task);
static inline void _actor444000InitializeRootPose(Task* task, GluttonWork* work);
static void        _actor444000HitGroups3To5(Task* task);
static void        _actor444000DormantState(Task* task);
static void        _actor444000CollapsedState(Task* task);
static void        _actor444000InhaleState(Task* task);
static inline void _actor444000PlaceCaughtPlayer(Task* task, GluttonWork* work,
                                                 Task* player, _Actor444000CatchScratch* catchScratch,
                                                 const PlayerStatus* playerStatus);
static void        _actor444000CatchPlayerState(Task* task);
static void        _actor444000SwipeState(Task* task);
static void        _actor444000ThrowDebrisState(Task* task);
static void        _actor444000LimbAnimationState(Task* task);
static void        _actor444000ResumeLimbAnimationState(Task* task);
static void        _actor444000RetractLimbState(Task* task);
static void        _actor444000ChooseAttackState(Task* task);
static void        _actor444000SummonState(Task* task);
static void        _actor444000ClampPlayerToArena(void);

/// Starts the post-boss incinerator alert once across normal and skipped playback.
///
/// Requires the published event task and its live work. The work latch is shared
/// with the script callback and survives until that event task is released.
static inline void _actor444000EventEnsureAlert(void)
{
    _Actor444000EventWork* published = _gActor444000IncineratorEventTask->work;

    if (published->alertPlayed == 0) {
        sndEvtRequestScriptStart(SOUND_SHELTER_B3_INCINERATOR_ALERT, 0, 0);
        published->alertPlayed = 1;
    }
}

/// Performs and consumes the event task's pending player-animation action.
///
/// Requires live event work and loaded character/weapon or event animation
/// resources. Weapon clip 1 blends for ten frames; event clip 3 resets and
/// sounds the shared alert once; event clip 0 cancels room effects, locks
/// attachments and blends for ten frames. All requests disable world collision.
/// Requests are consumed synchronously; their resources remain borrowed by
/// playback. Unknown actions are also cleared.
static void _actor444000EventPerformPlayerAction(Task* task)
{
    enum {
        ACTOR_444000_EVENT_PRIMARY_CHARACTER   = 1,
        ACTOR_444000_EVENT_PRIMARY_BANK_BASE   = 1,
        ACTOR_444000_EVENT_ALTERNATE_BANK_BASE = 34,
        ACTOR_444000_EVENT_WEAPON_CLIP         = 1,
        ACTOR_444000_EVENT_ALERT_CLIP          = 3,
        ACTOR_444000_EVENT_LOCK_CLIP           = 0,
        ACTOR_444000_EVENT_BLEND_FRAMES        = 10,
    };
    _Actor444000EventWork* work = task->work;
    _Actor444000EventWork* reloaded;
    AnimationPlayRequest   animationRequest;
    s32                    bankIndex;

    /// Cancels room effects, locks attachments and starts the event player's clip zero.
    ///
    /// Requires Task* `task`, writable AnimationPlayRequest `animationRequest`,
    /// and _Actor444000EventWork* `reloaded` locals, plus this function's clip
    /// and blend constants. Re-reads work after cancellation; a NULL player
    /// sends nothing. Expands to one block and retains no request pointer.
#define ACTOR_444000_EVENT_START_LOCKED_PLAYER_CLIP()                                                                  \
    {                                                                                                                  \
        roomEffectRequestCancelAll();                                                                                  \
        gAttachmentState.flags |= ATTACHMENT_FLAG_EVENT_LOCK;                                                          \
        reloaded                = task->work;                                                                          \
        if (reloaded->player != NULL) {                                                                                \
            animationRequest.source.sets          = _gActor444000EventPlayerAnimationSets;                             \
            animationRequest.animationId          = ACTOR_444000_EVENT_LOCK_CLIP;                                      \
            animationRequest.blend                = ANIMATION_BLEND_INTERPOLATE;                                       \
            animationRequest.blendFrames          = ACTOR_444000_EVENT_BLEND_FRAMES;                                   \
            animationRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;                                 \
            TASK_MESSAGE_DISPATCH_POINTER(reloaded->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &animationRequest, 0); \
        }                                                                                                              \
    }

    switch (work->playerAction) {
        case ACTOR_444000_PLAYER_ACTION_NONE:
            break;
        case ACTOR_444000_PLAYER_ACTION_WEAPON_CLIP_BLENDED:
            // The equipped weapon selects the bank; each character has its own run of banks.
            bankIndex = gPlayerStatus.weapon;
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == ACTOR_444000_EVENT_PRIMARY_CHARACTER) {
                bankIndex += ACTOR_444000_EVENT_PRIMARY_BANK_BASE;
            } else {
                bankIndex += ACTOR_444000_EVENT_ALTERNATE_BANK_BASE;
            }
            animationRequest.source.index         = bankIndex;
            animationRequest.animationId          = ACTOR_444000_EVENT_WEAPON_CLIP;
            animationRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
            animationRequest.blendFrames          = ACTOR_444000_EVENT_BLEND_FRAMES;
            animationRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &animationRequest, 0);
            break;
        case ACTOR_444000_PLAYER_ACTION_EVENT_CLIP_3:
            if (work->player != NULL) {
                animationRequest.source.sets          = _gActor444000EventPlayerAnimationSets;
                animationRequest.animationId          = ACTOR_444000_EVENT_ALERT_CLIP;
                animationRequest.blend                = ANIMATION_BLEND_RESET;
                animationRequest.blendFrames          = 0;
                animationRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(work->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &animationRequest, 0);
            }
            // The alert sounds once, whether this request or the script's own callback reaches it first.
            _actor444000EventEnsureAlert();
            break;
        case ACTOR_444000_PLAYER_ACTION_EVENT_CLIP_0:
            ACTOR_444000_EVENT_START_LOCKED_PLAYER_CLIP();
            break;
    }
    work->playerAction = ACTOR_444000_PLAYER_ACTION_NONE;
}
#undef ACTOR_444000_EVENT_START_LOCKED_PLAYER_CLIP

/// Restores the saved event view and updates the room selected by the lift descent.
///
/// updateMode is ACTOR_444000_DESCENT_RESELECT_VIEW (0), UPDATE_WITH_BLEND (1),
/// or UPDATE_SKIPPED (2); other values do nothing. Mode 0 restores the view.
/// Modes 1/2 synchronize saved/live rooms and reload area updates; mode 1 also
/// starts the double-pass previous-frame blend. Requires published event work,
/// live session/save state and loaded incinerator area records.
static void _actor444000EventUpdateDescentRoom(s32 updateMode)
{
    enum {
        ACTOR_444000_DESCENT_WAITING_ROOM    = 4,
        ACTOR_444000_DESCENT_MOVING_ROOM     = 5,
        ACTOR_444000_DESCENT_LANDED_ROOM     = 6,
        ACTOR_444000_EVENT_BLEND_BANK        = 1,
        ACTOR_444000_EVENT_BLEND_DESCRIPTOR  = 0x2D,
        ACTOR_444000_EVENT_BLEND_DOUBLE_PASS = 16,
    };
    _Actor444000EventWork* work;

    work = _gActor444000IncineratorEventTask->work;
    switch (updateMode) {
        case ACTOR_444000_DESCENT_RESELECT_VIEW:
            gGameSession->viewDirty                                = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = work->savedView;
            break;
        case ACTOR_444000_DESCENT_UPDATE_WITH_BLEND:
        case ACTOR_444000_DESCENT_UPDATE_SKIPPED:
            // Both saved and live locations follow the lift phase before room data reloads.
            switch (gGameSession->incineratorDescentPhase) {
                case GAME_SESSION_INCINERATOR_DESCENT_WAITING:
                    gGameSession->location.roomState                            = ACTOR_444000_DESCENT_WAITING_ROOM;
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = ACTOR_444000_DESCENT_WAITING_ROOM;
                    break;
                case GAME_SESSION_INCINERATOR_DESCENT_MOVING:
                    gGameSession->location.roomState                            = ACTOR_444000_DESCENT_MOVING_ROOM;
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = ACTOR_444000_DESCENT_MOVING_ROOM;
                    break;
                case GAME_SESSION_INCINERATOR_DESCENT_LANDED:
                case GAME_SESSION_INCINERATOR_DESCENT_COMPLETE:
                    gGameSession->location.roomState                            = ACTOR_444000_DESCENT_LANDED_ROOM;
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = ACTOR_444000_DESCENT_LANDED_ROOM;
                    break;
            }
            gGameSession->eventRoomIndex                           = gGameSession->location.roomState - 1;
            gGameSession->incineratorRoomGroup                     = 1;
            gGameSession->roomObjsDirty                            = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = work->savedView;
            areaApplySavedUpdates(gShelterB3GarbageIncineratorDescentAreaUpdates);
            if (updateMode == ACTOR_444000_DESCENT_UPDATE_WITH_BLEND) {
                work->framebufferBlend = taskSpawn(ACTOR_444000_EVENT_BLEND_BANK, ACTOR_444000_EVENT_BLEND_DESCRIPTOR, ACTOR_444000_EVENT_BLEND_DOUBLE_PASS, 0);
            }
            gGameSession->viewDirty = 1;
            break;
    }
}

/// Clears the incinerator event's combat state and selects its post-boss music once.
///
/// Requires live event work, session and save. If the work is not yet latched,
/// clears all battle references, action signals and enemy alerts, installs a
/// fifteen-frame end delay in the idle phase, requests weapon re-equipping and
/// selects scene event 13. Stores the low 16 bits of `completionLatch`; they must
/// be nonzero to prevent repetition. The event task supplies its current state, 1.
static inline void _actor444000EndEventCombat(_Actor444000EventWork* work, s32 completionLatch)
{
    enum {
        ACTOR_444000_EVENT_BATTLE_END_DELAY_FRAMES = 15,
        ACTOR_444000_EVENT_SCENE_MUSIC             = 13,
    };
    if (work->combatReset == 0) {
        gSceneCombatState.battleRefs                        = 0;
        gSceneCombatState.signals.bytes.endDelayFrames      = ACTOR_444000_EVENT_BATTLE_END_DELAY_FRAMES;
        gSceneCombatState.signals.bytes.battlePhase         = SCENE_COMBAT_BATTLE_IDLE;
        gSceneCombatState.signals.bytes.actionFlags         = 0;
        gSceneCombatState.signals.bytes.enemyAlert          = 0;
        gGameSession->flowFlags                            |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = ACTOR_444000_EVENT_SCENE_MUSIC;
        work->combatReset                                   = completionLatch;
    }
}

/// Runs the incinerator post-boss event and hands control back to the room.
///
/// Updates stop while scene, menu or actor control is paused. Task states 0..3
/// allocate/publish work, wait 701 active ticks to end combat, wait another
/// 21 ticks to start the ending script, then await script completion. A nonzero
/// first spawn argument instead starts the descent script directly. Requires
/// loaded incinerator resources and successful event-work allocation; the
/// retained failure path kills the task but still reaches the callback tail.
static void _actor444000IncineratorEventTask(Task* task)
{
    enum {
        ACTOR_444000_EVENT_INITIALIZE         = 0,
        ACTOR_444000_EVENT_WAIT_BATTLE_END    = 1,
        ACTOR_444000_EVENT_WAIT_ENDING        = 2,
        ACTOR_444000_EVENT_WAIT_SCRIPT        = 3,
        ACTOR_444000_EVENT_BATTLE_WAIT_FRAMES = 0x2BD,
        ACTOR_444000_EVENT_ENDING_WAIT_FRAMES = 21,
    };

    _Actor444000EventWork* work = task->work;
    _Actor444000EventWork* allocatedWork;
    _Actor444000EventWork* eventWork;
    s32                    eventState;
    s16                    nextTick;

    if (gGameSession->sceneUpdatesPaused != 0) {
        return;
    }
    if (gAttachmentState.menuOpen != ATTACHMENT_MENU_CLOSED) {
        return;
    }
    if (gSceneCombatState.actorControl != SCENE_COMBAT_ACTORS_RUNNING) {
        return;
    }

    eventState = task->state;
    switch (eventState) {
        case ACTOR_444000_EVENT_INITIALIZE:
            if (gAttachmentState.mode == ATTACHMENT_MODE_WHEEL) {
                return;
            }
            if (gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                return;
            }
            allocatedWork = memCalloc(sizeof(*allocatedWork), false);
            task->work    = allocatedWork;
            if (allocatedWork == NULL) {
                taskKill(task);
            } else {
                memFillBytes(allocatedWork, 0, sizeof(*allocatedWork));
                allocatedWork->player             = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                _gActor444000IncineratorEventTask = task;
            }
            if (task->spawnArg1.value != 0) {
                work            = task->work;
                work->savedView = gGameSession->location.view;
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                evsStartScriptWithSkip(_gActor444000DescentScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gActor444000DescentSkipScript);
                task->state = ACTOR_444000_EVENT_WAIT_SCRIPT;
            } else {
                task->state += 1;
            }
            break;
        case ACTOR_444000_EVENT_WAIT_BATTLE_END:
            // End combat once before beginning the post-boss script.
            nextTick            = (u16)task->killCountdown + 1;
            task->killCountdown = nextTick;
            if (nextTick >= ACTOR_444000_EVENT_BATTLE_WAIT_FRAMES) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                eventWork = _gActor444000IncineratorEventTask->work;
                _actor444000EndEventCombat(eventWork, eventState);
                task->killCountdown = 0;
                task->state        += 1;
            }
            break;
        case ACTOR_444000_EVENT_WAIT_ENDING:
            nextTick            = (u16)task->killCountdown + 1;
            task->killCountdown = nextTick;
            if (nextTick >= ACTOR_444000_EVENT_ENDING_WAIT_FRAMES) {
                work->savedView = gGameSession->location.view;
                evsStartScriptWithSkip(_gActor444000PostBossAlertScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gActor444000PostBossAlertSkipScript);
                task->state += 1;
            }
            break;
        case ACTOR_444000_EVENT_WAIT_SCRIPT:
            // Hand the settled room back to its incinerator controller.
            if (gGameSession->eventState == 0) {
                gShelterB3GarbageIncineratorDoorEventSuspended = SHELTER_B3_GARBAGE_INCINERATOR_DOOR_EVENT_RUNNING;
                gGameSession->sceneClock                       = gShelterB3GarbageIncineratorSuccessorSceneTiming.initialSceneClock;
                taskSpawnFromTable(gShelterB3GarbageIncineratorBlazeTaskDescs, SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_CONTROLLER_TASK, SHELTER_B3_GARBAGE_INCINERATOR_BLAZE_SKIP_ALERT, 0);
                taskKill(task);
                return;
            }
            break;
    }
    _actor444000EventPerformPlayerAction(task);
}

/// Plays the incinerator alert once during the post-boss event.
///
/// Requires the live published event task and its initialized work. The latch
/// is shared with the player-action tick so normal and skip scripts agree.
static void _actor444000EventPlayAlertOnce(void)
{
    _Actor444000EventWork* work = _gActor444000IncineratorEventTask->work;

    if (work->alertPlayed == 0) {
        sndEvtRequestScriptStart(SOUND_SHELTER_B3_INCINERATOR_ALERT, 0, 0);
        work->alertPlayed = 1;
    }
}

/// Sends the event player's model-draw mode through synchronous dispatch.
///
/// Requires the live published event task and player. `drawMode` uses
/// `PLAYER_ACTOR_MODEL_DRAW_*`; the scripts hide with allocation or show with
/// automatic buffers. The player's reply is discarded.
static void _actor444000EventSetPlayerModelDraw(s32 drawMode)
{
    _Actor444000EventWork* work = _gActor444000IncineratorEventTask->work;

    taskMessageDispatch(work->player, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, drawMode, 0);
}

/// Stops the event's framebuffer-blend task and clears its borrowed handle.
///
/// Requires the live published event task and initialized work. A NULL handle
/// makes repeated calls harmless; a non-NULL handle must still be live.
static void _actor444000EventStopFramebufferBlend(void)
{
    _Actor444000EventWork* work = _gActor444000IncineratorEventTask->work;

    if (work->framebufferBlend != NULL) {
        taskKill(work->framebufferBlend);
        work->framebufferBlend = NULL;
    }
}

/// Broadcasts the synthetic Mad Chaser vanish command to placed actors.
///
/// Requires a live scene manager. Stage 0/area 44 identifies the Mad Chaser
/// command namespace. The stack record is borrowed only through synchronous
/// dispatch; receivers that do not accept that namespace ignore it.
static void _actor444000EventDismissMadChasers(void)
{
    enum {
        ACTOR_444000_MAD_CHASER_COMMAND_STAGE = 0,
        ACTOR_444000_MAD_CHASER_COMMAND_AREA  = 44,
    };
    ActorCommand msg;

    msg.context.loc.stage = ACTOR_444000_MAD_CHASER_COMMAND_STAGE;
    msg.context.loc.area  = ACTOR_444000_MAD_CHASER_COMMAND_AREA;
    msg.command           = MAD_CHASER_COMMAND_VANISH;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
}

/// Broadcasts a command in the current session's stage/area namespace.
///
/// Requires a live session and scene manager. The script uses
/// `ACTOR_444000_COMMAND_SKIP_COLLAPSE` to complete the boss collapse when
/// skipped. The signed input's bits are stored in the unsigned command halfword;
/// the stack record remains live through synchronous dispatch.
static void _actor444000EventBroadcastCommand(s16 command)
{
    ActorCommand msg;

    msg.context.loc.stage = gGameSession->location.stage;
    msg.context.loc.area  = gGameSession->location.area;
    msg.command           = command;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
}

/// Ends the event's battle once and selects its post-boss scene music.
///
/// Requires the live published event task, session and live save. Clears battle
/// holds and stimuli, leaves a 15-frame end delay in the idle phase, requests
/// weapon re-equipping, and latches completion for normal and skip scripts.
static void _actor444000EventEndBattleOnce(void)
{
    enum {
        ACTOR_444000_EVENT_BATTLE_END_DELAY_FRAMES = 15,
        ACTOR_444000_EVENT_SCENE_MUSIC             = 13,
    };
    _Actor444000EventWork* work = _gActor444000IncineratorEventTask->work;

    if (work->combatReset == 0) {
        gSceneCombatState.battleRefs                        = 0;
        gSceneCombatState.signals.bytes.endDelayFrames      = ACTOR_444000_EVENT_BATTLE_END_DELAY_FRAMES;
        gSceneCombatState.signals.bytes.battlePhase         = SCENE_COMBAT_BATTLE_IDLE;
        gSceneCombatState.signals.bytes.actionFlags         = 0;
        gSceneCombatState.signals.bytes.enemyAlert          = 0;
        gGameSession->flowFlags                            |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = ACTOR_444000_EVENT_SCENE_MUSIC;
        work->combatReset                                   = 1;
    }
}

/// Queues one player action for the event task's next update.
///
/// Requires the live published event task and initialized work. `action` uses
/// `ACTOR_444000_PLAYER_ACTION_*`; a new request replaces any pending one and
/// resets its step. The request halfword retains the signed input's bits.
static void _actor444000EventRequestPlayerAction(s16 action)
{
    _Actor444000EventWork* work = _gActor444000IncineratorEventTask->work;

    work->playerAction     = action;
    work->playerActionStep = 0;
}

#include "../../shared/actor/actor_contacts_turn_joint.inc.c"

#include "../../shared/actor/actor_contacts_push_contact.inc.c"

#include "../../shared/creatures/glutton/glutton_wall.inc.c"

/// Moves the boss 50 parent-coordinate units along its normalized local Z axis.
///
/// Requires a live writable coordinate and an initialized scratch stack with
/// eight aligned bytes free. Includes vertical movement; normalization removes
/// the old scale. SDK/GTE quantization narrows displacement to signed halfwords.
/// Marks composition dirty and releases scratch before returning. The caller
/// handles freezing and collision correction.
static inline void _actor444000StepForward(GfxCoord* coord)
{
    enum {
        ACTOR_444000_ADVANCE_STEP_DISTANCE = 50,
    };
    SVECTOR* displacement;

    displacement = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);

    gfxReadMatrixZAxis(&coord->coord, displacement);
    _actorMovementBuildDisplacement(displacement, ACTOR_444000_ADVANCE_STEP_DISTANCE);

    coord->coord.t[0]  += displacement->vx;
    coord->coord.t[1]  += displacement->vy;
    coord->coord.t[2]  += displacement->vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;

    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

/// Presents one run-clip footstep with host-relative audio and controller vibration.
///
/// Requires the live host task/enemy/work and loaded rumble records. Audio pan
/// and half depth narrow to signed bytes; the placement index supplies instance
/// bits in the Glutton script key. Pointers are borrowed only for this call.
static inline void _actor444000RunFootstep(Task* task, Enemy* enemy, GluttonWork* work)
{
    enum { ACTOR_444000_RUN_FOOTSTEP_SOUND = SOUND_CHARACTER(SOUND_BANK_GLUTTON, 1) };
    s32 soundId;
    s32 soundPan;

    work->shakeLevel = GLUTTON_SHAKE_LONG;
    padScriptSpawn(gActor444000GluttonImpactPadCommands, gActor444000GluttonImpactPadSegments);
    soundId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_444000_RUN_FOOTSTEP_SOUND;
    soundPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    sndEvtRequestScriptStart(soundId, soundPan, (s8)(worldCoordGetOriginAudioDepth(task->extra.tmd->coords) / 2));
}

/// Runs the boss out, swings it through a half turn and advances back toward the lift.
///
/// Requires live host/player models, enemy work, driving rigs and scratch space.
/// Phase 0 advances along X; phase 1 turns about a point ahead of the root;
/// phases 2..5 advance along Z, pausing at attack checkpoints. The turn uses
/// 4096-unit yaw; translations use root-parent world units. Straight steps obey
/// actorsFrozen, while the turn retains its original independent timing.
/// The final Z threshold enters lift idle and sends room actor event 0.
static void _actor444000RunArenaRouteState(Task* task)
{
    enum {
        ACTOR_444000_RUN_CLIP                  = 2,
        ACTOR_444000_RUN_FIRST_STEP_CUE        = 18,
        ACTOR_444000_RUN_SECOND_STEP_CUE       = 24,
        ACTOR_444000_RUN_FIRST_ATTACK_X        = 6000,
        ACTOR_444000_RUN_TURN_X                = 8500,
        ACTOR_444000_RUN_TURN_STEP             = 13,
        ACTOR_444000_RUN_TURN_FORWARD_DISTANCE = 3050,
        ACTOR_444000_RUN_TURN_BACK_DISTANCE    = -3000,
        ACTOR_444000_RUN_SECOND_ATTACK_Z       = -4999,
        ACTOR_444000_RUN_THIRD_ATTACK_Z        = -9499,
        ACTOR_444000_RUN_FOURTH_ATTACK_Z       = -12999,
        ACTOR_444000_RUN_LIFT_Z                = -16899,
    };
    _Actor444000RunScratch* runScratch;
    TmdObject*              hostModel;
    GluttonWork*            work;
    Enemy*                  enemy;
    GfxCoord*               turnCoord;
    GfxCoord*               rootCoord;
    s32                     poseCue;

    SCRATCH_STACK_RESERVE_BYTES(sizeof(_Actor444000RunScratch));
    runScratch = SCRATCH_STACK_CURSOR(_Actor444000RunScratch);

    work  = task->work;
    enemy = task->spawnArg2.pointer;

    if (work->stateChanged != 0) {
        hostModel                     = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        hostModel->flags              = 0;
        work->neckPitchEnabled        = 1;
        work->animId                  = ACTOR_444000_RUN_CLIP;
        work->neckYawEnabled          = 1;
        work->animStep                = GLUTTON_ANIM_STEP_BLEND;
        work->hostExposed             = 0;
        work->neckPitchTarget         = 0;
        gfxSetRotIdentity(&runScratch->rootMatrix);
    }

    _gluttonTickAnim(task);

    poseCue = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    if (poseCue == ACTOR_444000_RUN_FIRST_STEP_CUE && work->clip.prevSlot2Cue != poseCue) {
        _actor444000RunFootstep(task, enemy, work);
    }

    poseCue = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    if (poseCue == ACTOR_444000_RUN_SECOND_STEP_CUE && work->clip.prevSlot2Cue != poseCue) {
        _actor444000RunFootstep(task, enemy, work);
    }

    work->clip.prevSlot2Cue = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;

    rootCoord             = task->extra.tmd->coords;
    runScratch->offset.vx = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
    runScratch->offset.vy = gPlayerStatus.coordMtx->t[1] - rootCoord->coord.t[1];
    runScratch->offset.vz = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];

    work->neckYawTarget = _actorAngleTurnToOffset(task->extra.tmd->coords, runScratch->offset.vx, runScratch->offset.vz);

    // Phase one swings about a point ahead of the root; later phases return along Z.
    switch (work->phase) {
        case 0: {
            s32       actorsFrozen = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen;
            GfxCoord* stepCoord    = task->extra.tmd->coords;

            if (actorsFrozen != 1) {
                _actor444000StepForward(stepCoord);
            }
        }
            task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            if (task->extra.tmd->coords->coord.t[0] >= ACTOR_444000_RUN_FIRST_ATTACK_X) {
                work->state = ACTOR_444000_STATE_CHOOSE_ATTACK;
                work->phase++;
            }
            break;

        case 1:
            turnCoord = task->extra.tmd->coords;
            if (turnCoord->coord.t[0] < ACTOR_444000_RUN_TURN_X) {
                if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
                    _actor444000StepForward(turnCoord);
                }
            } else {
                runScratch->yaw        = ratan2(-turnCoord->coord.m[2][0], turnCoord->coord.m[2][2]) + ACTOR_444000_RUN_TURN_STEP;
                runScratch->rootMatrix = task->extra.tmd->coords->coord;

                gfxReadMatrixZAxis(&runScratch->rootMatrix, &runScratch->offset);
                VectorNormalSS(&runScratch->offset, &runScratch->offset);
                gte_lddp(ACTOR_444000_RUN_TURN_FORWARD_DISTANCE);
                gte_ldsv(&runScratch->offset);
                gte_gpf12();
                gte_stsv(&runScratch->offset);

                runScratch->rootMatrix.t[0] += runScratch->offset.vx;
                runScratch->rootMatrix.t[1] += runScratch->offset.vy;
                runScratch->rootMatrix.t[2] += runScratch->offset.vz;

                gfxRotMatrixY(&runScratch->rootMatrix, runScratch->yaw, GRAPHICS_ROTATION_REPLACE);
                task->extra.tmd->coords->coord = runScratch->rootMatrix;

                gfxReadMatrixZAxis(&runScratch->rootMatrix, &runScratch->offset);
                VectorNormalSS(&runScratch->offset, &runScratch->offset);
                gte_lddp(ACTOR_444000_RUN_TURN_BACK_DISTANCE);
                gte_ldsv(&runScratch->offset);
                gte_gpf12();
                gte_stsv(&runScratch->offset);

                task->extra.tmd->coords->coord.t[0]  += runScratch->offset.vx;
                task->extra.tmd->coords->coord.t[1]  += runScratch->offset.vy;
                task->extra.tmd->coords->coord.t[2]  += runScratch->offset.vz;
                task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;

                if (ACTOR_TRANSFORM_ANGLE_TURN / 2 - ABS(runScratch->yaw) < ACTOR_444000_RUN_TURN_STEP) {
                    runScratch->yaw = ACTOR_TRANSFORM_ANGLE_TURN / 2;
                    gfxRotMatrixY(&task->extra.tmd->coords->coord, ACTOR_TRANSFORM_ANGLE_TURN / 2, GRAPHICS_ROTATION_REPLACE);
                    work->phase++;
                }
            }
            break;

        case 2: {
            s32       actorsFrozen = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen;
            GfxCoord* stepCoord    = task->extra.tmd->coords;

            if (actorsFrozen != 1) {
                _actor444000StepForward(stepCoord);
            }
        }
            task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            if (task->extra.tmd->coords->coord.t[2] < ACTOR_444000_RUN_SECOND_ATTACK_Z) {
                work->state = ACTOR_444000_STATE_CHOOSE_ATTACK;
                work->phase++;
            }
            break;

        case 3: {
            s32       actorsFrozen = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen;
            GfxCoord* stepCoord    = task->extra.tmd->coords;

            if (actorsFrozen != 1) {
                _actor444000StepForward(stepCoord);
            }
        }
            task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            if (task->extra.tmd->coords->coord.t[2] < ACTOR_444000_RUN_THIRD_ATTACK_Z) {
                work->state = ACTOR_444000_STATE_CHOOSE_ATTACK;
                work->phase++;
            }
            break;

        case 4: {
            s32       actorsFrozen = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen;
            GfxCoord* stepCoord    = task->extra.tmd->coords;

            if (actorsFrozen != 1) {
                _actor444000StepForward(stepCoord);
            }
        }
            task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            if (task->extra.tmd->coords->coord.t[2] < ACTOR_444000_RUN_FOURTH_ATTACK_Z) {
                work->state = ACTOR_444000_STATE_CHOOSE_ATTACK;
                work->phase++;
            }
            break;

        case 5: {
            s32       actorsFrozen = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen;
            GfxCoord* stepCoord    = task->extra.tmd->coords;

            if (actorsFrozen != 1) {
                _actor444000StepForward(stepCoord);
            }
        }
            task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            if (task->extra.tmd->coords->coord.t[2] < ACTOR_444000_RUN_LIFT_Z) {
                work->state = ACTOR_444000_STATE_LIFT_IDLE;
                work->phase++;
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, ACTOR_444000_ROOM_EVENT_START_DESCENT, 0);
            }
            break;
    }

    SCRATCH_STACK_RELEASE_BYTES(sizeof(_Actor444000RunScratch));
}

/// Collapses the dying boss, detaches its parts and shrinks their visible models.
///
/// Requires initialized host/escort rigs, live escort slots 0..4 and the writable
/// incinerator grid with vertices 24..31. Part detachment preserves world pose;
/// shrink scales are Q12 and durations count state ticks. The running clip and
/// its settled boundary use separate timelines. Completion restores the two
/// collision-wall heights; enemy/task ownership remains with normal teardown.
static void _actor444000CollapseState(Task* task)
{
    enum {
        ACTOR_444000_COLLAPSE_COLOR_TICK           = 20,
        ACTOR_444000_COLLAPSE_PART_2_TICK          = 130,
        ACTOR_444000_COLLAPSE_PART_4_TICK          = 330,
        ACTOR_444000_COLLAPSE_PART_3_TICK          = 476,
        ACTOR_444000_SETTLED_PART_1_START_TICK     = 20,
        ACTOR_444000_SETTLED_HOST_START_TICK       = 13,
        ACTOR_444000_SETTLED_HOST_BLACK_TICK       = 120,
        ACTOR_444000_SETTLED_FINAL_PART_TICK       = 160,
        ACTOR_444000_COLLAPSE_SINK_STEP            = 30,
        ACTOR_444000_LIMB_RETRACT_MIN_REACH        = 401,
        ACTOR_444000_LIMB_RETRACT_STEP             = 200,
        ACTOR_444000_COLLAPSE_CLIP                 = 18,
        ACTOR_444000_COLLAPSED_VARIANT             = 3,
        ACTOR_444000_PART_SHRINK_TICKS             = 40,
        ACTOR_444000_HOST_SHRINK_TICKS             = 120,
        ACTOR_444000_FINAL_PART_SHRINK_TICKS       = 90,
        ACTOR_444000_COLLAPSE_PART_EFFECT_ARGUMENT = 0x13401800,
    };
    GluttonWork* work;
    Enemy*       enemy;
    TmdObject*   hostModel;
    MATRIX       worldRotation;
    SVECTOR*     wallVertices;
    s32          poseCue;
    s32          shrinkTick;

    work  = task->work;
    enemy = task->spawnArg2.pointer;

    if (work->stateChanged != 0) {
        s32 soundId;
        s32 soundPan;

        hostModel                     = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        hostModel->flags              = 0;
        work->animId                  = ACTOR_444000_COLLAPSE_CLIP;
        work->neckPitchEnabled        = 0;
        work->neckYawEnabled          = 0;
        work->hostExposed             = 0;
        work->animStep                = GLUTTON_ANIM_STEP_BLEND;
        work->neckPitchTarget         = 0;

        _gluttonTickAnim(task);

        gGameSession->location.variant = ACTOR_444000_COLLAPSED_VARIANT;
        soundId                        = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR, 7);
        soundPan                       = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, soundPan, (s8)(worldCoordGetOriginAudioDepth(task->extra.tmd->coords) / 2));
        return;
    }

    if (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->stateTicks = 0;
        _gluttonTickAnim(task);
    }

    if (!(work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED)) {
        if (_gGluttonLimbReach >= ACTOR_444000_LIMB_RETRACT_MIN_REACH) {
            _gGluttonLimbReach = (u16)_gGluttonLimbReach - ACTOR_444000_LIMB_RETRACT_STEP;
        }

        _gluttonTickAnim(task);

        poseCue = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (poseCue == 0x33 && work->prevSlot3Cue != poseCue) {
            s32 soundId;
            s32 soundPan;

            soundId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x13);
            soundPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        }

        poseCue = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (poseCue == 0x3D && work->prevSlot3Cue != poseCue) {
            s32 soundId;
            s32 soundPan;

            soundId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x03);
            soundPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        }

        poseCue = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (poseCue == 0x4E && work->prevSlot3Cue != poseCue) {
            s32 soundId;
            s32 soundPan;

            soundId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x14);
            soundPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        }

        poseCue = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (poseCue == 0x71 && work->prevSlot3Cue != poseCue) {
            s32 soundId;
            s32 soundPan;

            soundId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x15);
            soundPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        }

        work->prevSlot3Cue = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;

        // Detach each part with its world pose before independently sinking and flattening it.
        {
            SVECTOR partOrigin;
            /// Reparents one live escort to the view at a host part's world pose.
            ///
            /// Captures task, work and writable worldRotation/partOrigin scratch.
            /// Both indices must be side-effect-free constants for existing parts;
            /// they are evaluated repeatedly. World translation narrows to signed
            /// halfwords before installation. No scratch pointer is retained.
#define ACTOR_444000_DETACH_COLLAPSED_PART(escortIndex, partIndex)                                  \
    do {                                                                                            \
        _actorRenderAccumulateWorldRotation(&task->extra.tmd->coords[(partIndex)], &worldRotation); \
        partOrigin.vz = 0;                                                                          \
        partOrigin.vy = 0;                                                                          \
        partOrigin.vx = 0;                                                                          \
        _actorRenderTransformLocalPointToWorld(&task->extra.tmd->coords[(partIndex)], &partOrigin); \
        work->escorts[(escortIndex)]->task->extra.tmd->coords->parent       = &gGfxViewCoord;       \
        work->escorts[(escortIndex)]->task->extra.tmd->coords->coord        = worldRotation;        \
        work->escorts[(escortIndex)]->task->extra.tmd->coords->coord.t[0]   = partOrigin.vx;        \
        work->escorts[(escortIndex)]->task->extra.tmd->coords->coord.t[1]   = partOrigin.vy;        \
        work->escorts[(escortIndex)]->task->extra.tmd->coords->coord.t[2]   = partOrigin.vz;        \
        work->escorts[(escortIndex)]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY; \
    } while (0)
            switch (work->stateTicks) {
                case ACTOR_444000_COLLAPSE_COLOR_TICK:
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                    worldCoordSetActorColorMode(work->escorts[3], ENEMY_COLOR_WEIGHTED);
                    break;

                case ACTOR_444000_COLLAPSE_PART_2_TICK:
                    ACTOR_444000_DETACH_COLLAPSED_PART(2, 4);
                    break;

                case ACTOR_444000_COLLAPSE_PART_3_TICK:
                    ACTOR_444000_DETACH_COLLAPSED_PART(3, 3);
                    break;

                case ACTOR_444000_COLLAPSE_PART_4_TICK:
                    ACTOR_444000_DETACH_COLLAPSED_PART(4, 4);
                    work->limbPoseEnabled = 0;
                    break;
            }
#undef ACTOR_444000_DETACH_COLLAPSED_PART
        }

        if (work->stateTicks >= ACTOR_444000_COLLAPSE_PART_2_TICK + 1) {
            SVECTOR effectOffset;
            if (work->escorts[2]->task->extra.tmd->coords->coord.t[1] <
                task->extra.tmd->coords->coord.t[1]) {
                work->escorts[2]->task->extra.tmd->coords->coord.t[1] +=
                    (work->stateTicks - ACTOR_444000_COLLAPSE_PART_2_TICK) * ACTOR_444000_COLLAPSE_SINK_STEP;
            } else if (task->extra.tmd->coords->coord.t[1] <
                       work->escorts[2]->task->extra.tmd->coords->coord.t[1]) {
                work->escorts[2]->task->extra.tmd->coords->coord.t[1] =
                    task->extra.tmd->coords->coord.t[1];
                effectSpawn(EFFECT_196, work->escorts[2]->task->extra.tmd->coords, ACTOR_444000_COLLAPSE_PART_EFFECT_ARGUMENT,
                            NULL);
            }

            shrinkTick = work->stateTicks - ACTOR_444000_COLLAPSE_PART_2_TICK;
            if (shrinkTick < ACTOR_444000_PART_SHRINK_TICKS) {
                _actorRenderRescaleYawY(work->escorts[2]->task->extra.tmd->coords, ONE,
                                        (s16)(ONE - ((shrinkTick * ONE) / ACTOR_444000_PART_SHRINK_TICKS)));
                work->escorts[2]->task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;

                if ((s16)((s16)(u16)work->stateTicks % 5) == 0) {
                    switch ((s16)((s16)((s16)(u16)work->stateTicks / 5) % 3)) {
                        case 0:
                            effectOffset.vz = 0;
                            effectOffset.vy = 0;
                            effectOffset.vx = 0;
                            effectSpawn(EFFECT_196, work->escorts[2]->task->extra.tmd->coords,
                                        ACTOR_444000_COLLAPSE_PART_EFFECT_ARGUMENT, &effectOffset);
                            break;
                        case 1:
                            effectOffset.vx = 0x320;
                            effectOffset.vy = 0;
                            effectOffset.vz = -0x320;
                            effectSpawn(EFFECT_196, work->escorts[2]->task->extra.tmd->coords,
                                        ACTOR_444000_COLLAPSE_PART_EFFECT_ARGUMENT, &effectOffset);
                            break;
                        case 2:
                            effectOffset.vx = -0x320;
                            effectOffset.vy = 0;
                            effectOffset.vz = 0x320;
                            effectSpawn(EFFECT_196, work->escorts[2]->task->extra.tmd->coords,
                                        ACTOR_444000_COLLAPSE_PART_EFFECT_ARGUMENT, &effectOffset);
                            break;
                    }
                }
            } else if (shrinkTick == ACTOR_444000_PART_SHRINK_TICKS) {
                work->escorts[2]->task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                effectSpawn(EFFECT_196, work->escorts[2]->task->extra.tmd->coords, ACTOR_444000_COLLAPSE_PART_EFFECT_ARGUMENT,
                            NULL);
            }
        }

        if (work->stateTicks >= ACTOR_444000_COLLAPSE_PART_4_TICK + 1) {
            SVECTOR effectOffset;
            if (work->escorts[4]->task->extra.tmd->coords->coord.t[1] <
                task->extra.tmd->coords->coord.t[1]) {
                work->escorts[4]->task->extra.tmd->coords->coord.t[1] +=
                    (work->stateTicks - ACTOR_444000_COLLAPSE_PART_4_TICK) * ACTOR_444000_COLLAPSE_SINK_STEP;
            } else if (task->extra.tmd->coords->coord.t[1] <
                       work->escorts[4]->task->extra.tmd->coords->coord.t[1]) {
                work->escorts[4]->task->extra.tmd->coords->coord.t[1] =
                    task->extra.tmd->coords->coord.t[1];
                effectSpawn(EFFECT_196, work->escorts[4]->task->extra.tmd->coords, ACTOR_444000_COLLAPSE_PART_EFFECT_ARGUMENT,
                            NULL);
            }

            shrinkTick = work->stateTicks - ACTOR_444000_COLLAPSE_PART_4_TICK;
            if (shrinkTick < ACTOR_444000_PART_SHRINK_TICKS) {
                _actorRenderRescaleYawY(work->escorts[4]->task->extra.tmd->coords, ONE,
                                        (s16)(ONE - ((shrinkTick * ONE) / ACTOR_444000_PART_SHRINK_TICKS)));
                work->escorts[4]->task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;

                if ((s16)((s16)(u16)work->stateTicks % 5) == 0) {
                    switch ((s16)((s16)((s16)(u16)work->stateTicks / 5) % 3)) {
                        case 0:
                            effectOffset.vz = 0;
                            effectOffset.vy = 0;
                            effectOffset.vx = 0;
                            effectSpawn(EFFECT_196, work->escorts[4]->task->extra.tmd->coords,
                                        ACTOR_444000_COLLAPSE_PART_EFFECT_ARGUMENT, &effectOffset);
                            break;
                        case 1:
                            effectOffset.vx = 0x320;
                            effectOffset.vy = 0;
                            effectOffset.vz = -0x320;
                            effectSpawn(EFFECT_196, work->escorts[4]->task->extra.tmd->coords,
                                        ACTOR_444000_COLLAPSE_PART_EFFECT_ARGUMENT, &effectOffset);
                            break;
                        case 2:
                            effectOffset.vx = -0x320;
                            effectOffset.vy = 0;
                            effectOffset.vz = 0x320;
                            effectSpawn(EFFECT_196, work->escorts[4]->task->extra.tmd->coords,
                                        ACTOR_444000_COLLAPSE_PART_EFFECT_ARGUMENT, &effectOffset);
                            break;
                    }
                }
            } else if (shrinkTick == ACTOR_444000_PART_SHRINK_TICKS) {
                work->escorts[4]->task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                effectSpawn(EFFECT_196, work->escorts[4]->task->extra.tmd->coords, ACTOR_444000_COLLAPSE_PART_EFFECT_ARGUMENT,
                            NULL);
            }
        }

        if (work->stateTicks >= ACTOR_444000_COLLAPSE_PART_3_TICK + 1) {
            SVECTOR effectOffset;
            if (work->escorts[3]->task->extra.tmd->coords->coord.t[1] <
                task->extra.tmd->coords->coord.t[1]) {
                work->escorts[3]->task->extra.tmd->coords->coord.t[1] +=
                    (work->stateTicks - ACTOR_444000_COLLAPSE_PART_3_TICK) * ACTOR_444000_COLLAPSE_SINK_STEP;
            } else if (task->extra.tmd->coords->coord.t[1] <
                       work->escorts[3]->task->extra.tmd->coords->coord.t[1]) {
                work->escorts[3]->task->extra.tmd->coords->coord.t[1] =
                    task->extra.tmd->coords->coord.t[1];
            }

            shrinkTick = work->stateTicks - ACTOR_444000_COLLAPSE_PART_3_TICK;
            if (shrinkTick < ACTOR_444000_PART_SHRINK_TICKS) {
                _actorRenderRescaleYawY(work->escorts[3]->task->extra.tmd->coords, ONE,
                                        (s16)(ONE - ((shrinkTick * ONE) / ACTOR_444000_PART_SHRINK_TICKS)));
                work->escorts[3]->task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;

                if ((s16)((s16)(u16)work->stateTicks % 5) == 0) {
                    switch ((s16)((s16)((s16)(u16)work->stateTicks / 5) % 3)) {
                        case 0:
                            effectOffset.vz = 0;
                            effectOffset.vy = 0;
                            effectOffset.vx = 0;
                            effectSpawn(EFFECT_196, work->escorts[3]->task->extra.tmd->coords,
                                        ACTOR_444000_COLLAPSE_PART_EFFECT_ARGUMENT, &effectOffset);
                            break;
                        case 1:
                            effectOffset.vx = 0x320;
                            effectOffset.vy = 0;
                            effectOffset.vz = -0x320;
                            effectSpawn(EFFECT_196, work->escorts[3]->task->extra.tmd->coords,
                                        ACTOR_444000_COLLAPSE_PART_EFFECT_ARGUMENT, &effectOffset);
                            break;
                        case 2:
                            effectOffset.vx = -0x320;
                            effectOffset.vy = 0;
                            effectOffset.vz = 0x320;
                            effectSpawn(EFFECT_196, work->escorts[3]->task->extra.tmd->coords,
                                        ACTOR_444000_COLLAPSE_PART_EFFECT_ARGUMENT, &effectOffset);
                            break;
                    }
                }
            } else if (shrinkTick == ACTOR_444000_PART_SHRINK_TICKS) {
                work->escorts[3]->task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
        }
    } else {
        SVECTOR effectOffset;
        // The settled clip starts a second timeline for the host and remaining parts.
        switch (work->stateTicks) {
            case ACTOR_444000_PART_SHRINK_TICKS:
                break;
            case ACTOR_444000_SETTLED_HOST_BLACK_TICK:
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                break;
        }

        if (work->stateTicks > 0) {
            if (work->stateTicks < ACTOR_444000_PART_SHRINK_TICKS) {
                _actorRenderRescaleYawY(work->escorts[0]->task->extra.tmd->coords, ONE,
                                        (s16)(ONE - ((work->stateTicks * ONE) / ACTOR_444000_PART_SHRINK_TICKS)));
                work->escorts[0]->task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            } else if (work->stateTicks == ACTOR_444000_PART_SHRINK_TICKS) {
                work->escorts[0]->task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
        }

        if (work->stateTicks >= ACTOR_444000_SETTLED_PART_1_START_TICK + 1) {
            shrinkTick = work->stateTicks - ACTOR_444000_SETTLED_PART_1_START_TICK;
            if (shrinkTick < ACTOR_444000_PART_SHRINK_TICKS) {
                _actorRenderRescaleYawY(work->escorts[1]->task->extra.tmd->coords, ONE,
                                        (s16)(ONE - ((shrinkTick * ONE) / ACTOR_444000_PART_SHRINK_TICKS)));
                work->escorts[1]->task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            } else if (shrinkTick == ACTOR_444000_PART_SHRINK_TICKS) {
                work->escorts[1]->task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
        }

        if (work->stateTicks >= ACTOR_444000_SETTLED_HOST_START_TICK + 1) {
            shrinkTick = work->stateTicks - ACTOR_444000_SETTLED_HOST_START_TICK;
            if (shrinkTick < ACTOR_444000_HOST_SHRINK_TICKS) {
                _actorRenderRescaleYawY(task->extra.tmd->coords, ONE,
                                        (s16)(ONE - ((shrinkTick * ONE) / ACTOR_444000_HOST_SHRINK_TICKS)));
                task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            } else if (shrinkTick == ACTOR_444000_HOST_SHRINK_TICKS) {
                task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
        }

        if ((s16)((s16)(u16)work->stateTicks % 3) == 0 && (s16)(u16)work->stateTicks - ACTOR_444000_SETTLED_HOST_START_TICK < ACTOR_444000_HOST_SHRINK_TICKS) {
            switch ((s16)((s16)((s16)(u16)work->stateTicks / 3) % 5)) {
                case 0:
                    effectOffset.vz = 0;
                    effectOffset.vy = 0;
                    effectOffset.vx = 0;
                    effectSpawn(EFFECT_196, task->extra.tmd->coords, 0x14101900, &effectOffset);
                    break;
                case 1:
                    effectOffset.vx = 0x960;
                    effectOffset.vy = 0;
                    effectOffset.vz = -0x960;
                    effectSpawn(EFFECT_196, task->extra.tmd->coords, 0x13201800, &effectOffset);
                    break;
                case 2:
                    effectOffset.vx = -0x9C4;
                    effectOffset.vy = 0;
                    effectOffset.vz = 0x9C4;
                    effectSpawn(EFFECT_196, task->extra.tmd->coords, 0x131C1800, &effectOffset);
                    break;
                case 3:
                    effectOffset.vx = -0x6A4;
                    effectOffset.vy = 0;
                    effectOffset.vz = 0x640;
                    effectSpawn(EFFECT_196, task->extra.tmd->coords, 0x14101800, &effectOffset);
                    break;
                case 4:
                    effectOffset.vx = 0x6A4;
                    effectOffset.vy = 0;
                    effectOffset.vz = -0x640;
                    effectSpawn(EFFECT_196, task->extra.tmd->coords, 0x14301800, &effectOffset);
                    break;
            }
        }

        if (work->stateTicks >= ACTOR_444000_SETTLED_FINAL_PART_TICK + 1) {
            shrinkTick = work->stateTicks - ACTOR_444000_SETTLED_FINAL_PART_TICK;
            if (shrinkTick < ACTOR_444000_FINAL_PART_SHRINK_TICKS) {
                _actorRenderRescaleYawY(work->escorts[3]->task->extra.tmd->coords, ONE,
                                        (s16)(ONE / 2 - ((shrinkTick * (ONE / 2)) / ACTOR_444000_FINAL_PART_SHRINK_TICKS)));
            } else if (shrinkTick == ACTOR_444000_FINAL_PART_SHRINK_TICKS) {
                task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;

                wallVertices        = gWorldCollisionActiveGrid->vertices;
                wallVertices[24].vy = ACTOR_444000_COMBAT_WALL_TOP_Y;
                wallVertices[25].vy = ACTOR_444000_COMBAT_WALL_TOP_Y;
                wallVertices[26].vy = ACTOR_444000_COMBAT_WALL_BOTTOM_Y;
                wallVertices[27].vy = ACTOR_444000_COMBAT_WALL_BOTTOM_Y;
                wallVertices[28].vy = ACTOR_444000_COMBAT_WALL_TOP_Y;
                wallVertices[29].vy = ACTOR_444000_COMBAT_WALL_TOP_Y;
                wallVertices[30].vy = ACTOR_444000_COMBAT_WALL_BOTTOM_Y;
                wallVertices[31].vy = ACTOR_444000_COMBAT_WALL_BOTTOM_Y;
            }
        }

        if (work->stateTicks == ACTOR_444000_SETTLED_FINAL_PART_TICK) {
            sndEvtRequestScriptStop((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR, 7), SOUND_SCRIPT_STOP_KEEP_RELEASE);
        }
    }
}

/// Rebuilds the two connected collision walls used while the boss turns.
///
/// `distance` uses parent-coordinate units along the normalized host Z axis.
/// Requires a live model and the writable incinerator grid: two faces/normals
/// starting at `faceIndex`, and eight vertices starting at `4 * faceIndex`.
/// The caller uses face 6. The first wall joins (11500, -7000) in X/Z to
/// the projected host point; the second extends 7000 units along +X.
/// Both span 400 units in Y and use room surface class 2. Coordinates retain
/// low-halfword wrapping; normals are Q12. Cell lists are left intact.
static void _actor444000BuildCornerWalls(Task* task, s32 distance, s16 faceIndex)
{
    enum {
        ACTOR_444000_CORNER_WALL_ANCHOR_X      = 11500,
        ACTOR_444000_CORNER_WALL_ANCHOR_Z      = -7000,
        ACTOR_444000_CORNER_WALL_RETURN_LENGTH = 7000,
        ACTOR_444000_CORNER_WALL_HEIGHT        = 400,
        ACTOR_444000_CORNER_WALL_SURFACE_CLASS = 2,
    };
    SVECTOR                 forwardOffset;
    SVECTOR*                normals    = gWorldCollisionActiveGrid->normals;
    SVECTOR*                vertices   = gWorldCollisionActiveGrid->vertices;
    WorldCollisionGridFace* faces      = gWorldCollisionActiveGrid->faces;
    WorldCollisionGridFace  anchorWall = { { faceIndex * 4, faceIndex * 4 + 1, faceIndex * 4 + 2, faceIndex * 4 + 3 }, faceIndex, ACTOR_444000_CORNER_WALL_SURFACE_CLASS };
    WorldCollisionGridFace  returnWall = {
        { (faceIndex + 1) * 4, (faceIndex + 1) * 4 + 1, (faceIndex + 1) * 4 + 2, (faceIndex + 1) * 4 + 3 }, faceIndex + 1, ACTOR_444000_CORNER_WALL_SURFACE_CLASS
    };
    SVECTOR* forwardOffsetPointer;

    // Project the host's forward point, then join the fixed corner to it.
    gfxReadMatrixZAxis(&task->extra.tmd->coords->coord, &forwardOffset);
    forwardOffsetPointer = &forwardOffset;
    VectorNormalSS(forwardOffsetPointer, forwardOffsetPointer);
    gte_lddp(distance);
    gte_ldsv(forwardOffsetPointer);
    gte_gpf12();
    gte_stsv(forwardOffsetPointer);

    vertices[faceIndex * 4].vx = vertices[faceIndex * 4 + 2].vx = ACTOR_444000_CORNER_WALL_ANCHOR_X;
    vertices[faceIndex * 4].vy = vertices[faceIndex * 4 + 2].vy = (u16)task->extra.tmd->coords->coord.t[1];
    vertices[faceIndex * 4].vz = vertices[faceIndex * 4 + 2].vz = ACTOR_444000_CORNER_WALL_ANCHOR_Z;
    vertices[faceIndex * 4 + 1].vx                              = vertices[faceIndex * 4 + 3].vx =
        (u16)task->extra.tmd->coords->coord.t[0] + (u16)forwardOffset.vx;
    vertices[faceIndex * 4 + 1].vy = vertices[faceIndex * 4 + 3].vy =
        (u16)task->extra.tmd->coords->coord.t[1] + (u16)forwardOffset.vy;
    vertices[faceIndex * 4 + 1].vz = vertices[faceIndex * 4 + 3].vz =
        (u16)task->extra.tmd->coords->coord.t[2] + (u16)forwardOffset.vz;
    vertices[faceIndex * 4].vy     = (u16)vertices[faceIndex * 4].vy - ACTOR_444000_CORNER_WALL_HEIGHT;
    vertices[faceIndex * 4 + 1].vy = (u16)vertices[faceIndex * 4 + 1].vy - ACTOR_444000_CORNER_WALL_HEIGHT;
    faces[faceIndex]               = anchorWall;

    normals[faceIndex].vz = (u16)vertices[faceIndex * 4].vx - (u16)vertices[faceIndex * 4 + 1].vx;
    normals[faceIndex].vy = (u16)vertices[faceIndex * 4 + 1].vy - (u16)vertices[faceIndex * 4].vy;
    normals[faceIndex].vx = (u16)vertices[faceIndex * 4 + 1].vz - (u16)vertices[faceIndex * 4].vz;
    VectorNormalSS(&normals[faceIndex], &normals[faceIndex]);

    // Return along the room X axis from the shared endpoint.
    vertices[faceIndex * 4 + 4].vx = vertices[faceIndex * 4 + 6].vx =
        (u16)task->extra.tmd->coords->coord.t[0] + (u16)forwardOffset.vx;
    vertices[faceIndex * 4 + 4].vy = vertices[faceIndex * 4 + 6].vy =
        (u16)task->extra.tmd->coords->coord.t[1] + (u16)forwardOffset.vy;
    vertices[faceIndex * 4 + 4].vz = vertices[faceIndex * 4 + 6].vz =
        (u16)task->extra.tmd->coords->coord.t[2] + (u16)forwardOffset.vz;
    vertices[faceIndex * 4 + 5].vx = vertices[faceIndex * 4 + 7].vx =
        (u16)task->extra.tmd->coords->coord.t[0] + (u16)forwardOffset.vx + ACTOR_444000_CORNER_WALL_RETURN_LENGTH;
    vertices[faceIndex * 4 + 5].vy = vertices[faceIndex * 4 + 7].vy =
        (u16)task->extra.tmd->coords->coord.t[1] + (u16)forwardOffset.vy;
    vertices[faceIndex * 4 + 5].vz = vertices[faceIndex * 4 + 7].vz =
        (u16)task->extra.tmd->coords->coord.t[2] + (u16)forwardOffset.vz;
    vertices[faceIndex * 4 + 4].vy = (u16)vertices[faceIndex * 4 + 4].vy - ACTOR_444000_CORNER_WALL_HEIGHT;
    vertices[faceIndex * 4 + 5].vy = (u16)vertices[faceIndex * 4 + 5].vy - ACTOR_444000_CORNER_WALL_HEIGHT;
    faces[faceIndex + 1]           = returnWall;

    normals[faceIndex + 1].vx = 0;
    normals[faceIndex + 1].vy = 0;
    normals[faceIndex + 1].vz = -ONE;
}

#include "../../shared/creatures/glutton/glutton_throw.inc.c"

#include "../../shared/creatures/glutton/glutton_inlines.inc.c"

#include "../../shared/creatures/glutton/glutton_glob.inc.c"

#include "../../shared/creatures/glutton/glutton_chunk.inc.c"

/// Attached-model lifecycle indexed by Task::state: texture setup, refresh, teardown.
///
/// Both escort dispatchers copy all three EnemyTaskFunc callbacks before
/// indexing state 0..2. Each receives the live enemy and task and may release
/// them during teardown; there is no terminator or bounds check.
static const EnemyTaskFuncTable3 _gGluttonPropStates = {
    {
        _gluttonPropSetup,
        _gluttonPropTick,
        enemyDestroy,
    },
};

/// Thrown-sphere lifecycle indexed by Task::state: spawn, flight and teardown.
static const EnemyTaskFuncTable3 _gGluttonThrowStates = {
    {
        _gluttonThrowSpawn,
        _gluttonThrowFly,
        enemyDestroy,
    },
};

/// Catching-glob lifecycle: spawn, descent, engulf, player release and teardown.
///
/// Task::state is in 0..4. The dispatcher copies all five typed callbacks
/// before calling one; the selected handler may destroy the enemy and task.
static const EnemyTaskFuncTable5 _gGluttonGlobStates = {
    {
        _gluttonGlobSpawn,
        _gluttonGlobFall,
        _gluttonGlobEngulf,
        _gluttonGlobHold,
        enemyDestroy,
    },
};

#include "../../shared/creatures/glutton/glutton_chunk_settle.inc.c"

#include "../../shared/creatures/glutton/glutton_rain.inc.c"

/// Debris-chunk lifecycle indexed by Task::state: spawn, fall, settle, teardown.
///
/// The dispatcher copies all four EnemyTaskFunc callbacks before indexing
/// state 0..3. Each receives the live enemy and its TMD-body task. Falling
/// and settling own independent attack/grid spheres until unlink and teardown.
static const EnemyTaskFuncTable4 _gGluttonChunkStates = {
    {
        _gluttonChunkSpawn,
        _gluttonChunkFall,
        _gluttonChunkSettle,
        enemyDestroy,
    },
};

/// Rain-blob lifecycle indexed by Task::state: spawn, rise, fall, splat, teardown.
///
/// The dispatcher copies all five EnemyTaskFunc callbacks before indexing
/// state 0..4. Each receives the live enemy and coordinate-body task. Spawn
/// owns projectile work and a child visual effect; later states require it.
static const EnemyTaskFuncTable5 _gGluttonRainStates = {
    {
        _gluttonRainSpawn,
        _gluttonRainRise,
        _gluttonRainFall,
        _gluttonRainSplat,
        enemyDestroy,
    },
};

/// Spinner lifecycle: spawn, visible floor wait, chase and teardown.
///
/// Task::state is in 0..3. Waiting permits normal drawing until release;
/// the dispatcher copies all four typed callbacks before calling one.
static const EnemyTaskFuncTable4 _gGluttonSpinnerStates = {
    {
        _gluttonSpinnerSpawn,
        _gluttonSpinnerWait,
        _gluttonSpinnerChase,
        enemyDestroy,
    },
};

#include "../../shared/creatures/glutton/glutton_rain_splat.inc.c"

#include "../../shared/creatures/glutton/glutton_spinner.inc.c"

#include "../../shared/creatures/glutton/glutton_shake_tick.inc.c"

/// Copies the host's complete draw flags to every live escort model.
///
/// Requires live host work/model and a live model for each non-NULL escort.
/// Traverses all seven slots, borrowing every pointer only for the call. Each
/// write reads the current host flags; no flags are merged or cleared.
static inline void _actor444000CopyEscortModelFlags(Task* task, GluttonWork* work)
{
    s16 escortIndex;

    for (escortIndex = 0; escortIndex < ARRAY_SIZE(work->escorts); escortIndex++) {
        if (work->escorts[escortIndex] != NULL) {
            work->escorts[escortIndex]->task->extra.tmd->flags = task->extra.tmd->flags;
        }
    }
}

/// Sets drawing and buffer policy for the Glutton host and its seven escorts.
///
/// Requires live host work and model; each non-NULL escort must have a live
/// model. Modes: 0 allocate missing buffers and hide, 1 show then allocate
/// missing buffers, 2 hide and defer buffer release by three updates, 3 show
/// all models and disable automatic allocation on the host alone. Modes 0/2
/// return the boss to its dormant state. Unknown modes do nothing. Returns 0;
/// `messageId` and `unusedSecondArg` are ignored.
static s32 _actor444000SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedSecondArg)
{
    enum {
        ACTOR_444000_MODEL_DRAW_HIDE_ALLOCATE   = 0,
        ACTOR_444000_MODEL_DRAW_SHOW_ALLOCATE   = 1,
        ACTOR_444000_MODEL_DRAW_HIDE_RELEASE    = 2,
        ACTOR_444000_MODEL_DRAW_SHOW_NO_AUTO    = 3,
        ACTOR_444000_MODEL_RELEASE_DELAY_FRAMES = 3,
    };
    TmdObject*   hostModel;
    GluttonWork* work;
    GluttonWork* flagWork;
    GluttonWork* hiddenBufferWork;
    GluttonWork* visibleBufferWork;
    TmdObject*   visibleHostModel;
    TmdObject*   escortModel;
    s32          hostFlags;
    s16          bufferIndex;

    hostModel = task->extra.tmd;
    work      = task->work;
    switch (drawMode) {
        // Ensure buffers before hiding and propagating the host flags.
        case ACTOR_444000_MODEL_DRAW_HIDE_ALLOCATE:
            hiddenBufferWork = task->work;
            if (hostModel->buffer == NULL) {
                tmdAllocPrimitiveBuffer(hostModel);
            }
            for (bufferIndex = 0; bufferIndex < ARRAY_SIZE(hiddenBufferWork->escorts); bufferIndex++) {
                if (hiddenBufferWork->escorts[bufferIndex] != NULL) {
                    escortModel = hiddenBufferWork->escorts[bufferIndex]->task->extra.tmd;
                    if (escortModel->buffer == NULL) {
                        tmdAllocPrimitiveBuffer(escortModel);
                    }
                }
            }
            flagWork                = task->work;
            flagWork->freeCountdown = 0;
            task->extra.tmd->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            _actor444000CopyEscortModelFlags(task, flagWork);
            work->state = ACTOR_444000_STATE_DORMANT;
            break;
        // Clear drawing flags before buffer allocation on the show path.
        case ACTOR_444000_MODEL_DRAW_SHOW_ALLOCATE:
            flagWork                = task->work;
            flagWork->freeCountdown = 0;
            task->extra.tmd->flags  = 0;
            _actor444000CopyEscortModelFlags(task, flagWork);
            visibleHostModel  = task->extra.tmd;
            visibleBufferWork = task->work;
            if (visibleHostModel->buffer == NULL) {
                tmdAllocPrimitiveBuffer(visibleHostModel);
            }
            for (bufferIndex = 0; bufferIndex < ARRAY_SIZE(visibleBufferWork->escorts); bufferIndex++) {
                if (visibleBufferWork->escorts[bufferIndex] != NULL) {
                    escortModel = visibleBufferWork->escorts[bufferIndex]->task->extra.tmd;
                    if (escortModel->buffer == NULL) {
                        tmdAllocPrimitiveBuffer(escortModel);
                    }
                }
            }
            break;
        // The frame update releases host and escort buffers after this delay.
        case ACTOR_444000_MODEL_DRAW_HIDE_RELEASE:
            hostModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            hostFlags         = hostModel->flags;
            flagWork          = task->work;
            if (hostFlags & TMD_OBJECT_SKIP_AUTO_BUFFER) {
                flagWork->freeCountdown = ACTOR_444000_MODEL_RELEASE_DELAY_FRAMES;
                task->extra.tmd->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            } else {
                flagWork->freeCountdown = 0;
                task->extra.tmd->flags  = hostFlags;
            }
            _actor444000CopyEscortModelFlags(task, flagWork);
            work->state = ACTOR_444000_STATE_DORMANT;
            break;
        case ACTOR_444000_MODEL_DRAW_SHOW_NO_AUTO:
            hostModel->flags        = 0;
            flagWork                = task->work;
            flagWork->freeCountdown = 0;
            task->extra.tmd->flags  = 0;
            _actor444000CopyEscortModelFlags(task, flagWork);
            hostModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
    }
    return 0;
}

/// Flattens the boss root at its existing yaw and refreshes its composition.
///
/// Requires a live TMD task with root coordinate and initialized scratch stack
/// with 0x58 free aligned bytes. Replaces pitch, roll and scale with X/Z unity
/// and Y zero, preserving translation and hierarchy. Releases its temporary
/// frame before composing; composition also requires its normal scratch space.
static inline void _actor444000FlattenRoot(Task* task)
{
    GfxCoord* coord = task->extra.tmd->coords;

    // The common rebuild releases scratch before the composition refresh.
    _actorRenderRescaleYawY(coord, ONE, 0);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
}
/// Applies an incinerator command and records its context in the boss work.
///
/// Requires live host/escorts, initialized animation rigs and a complete borrowed
/// request through dispatch. Only Mine/Shelter garbage-incinerator context acts:
/// 0 stops the boss, 1 advances clip 10 then repositions it to resume advancing,
/// 19 selects the collapsed state, blackens host/escort 3, locks the view, raises
/// floor vertices 24..31 and flattens the root. Command 19 requires that writable
/// room grid. The cached command retains only the low byte; dispatch uses the
/// full halfword. Returns 1 even for ignored contexts or commands; other args
/// are ignored.
static s32 _actor444000ApplyCommand(Task* task, s32 messageId, const ActorCommand* request, s32 unusedSecondArg)
{
    enum {
        ACTOR_444000_COMMAND_CONTEXT         = (GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR << 8) | GAME_STAGE_MINE_SHELTER,
        ACTOR_444000_REPOSITION_CLIP         = 10,
        ACTOR_444000_REPOSITION_FINAL_RATE   = ANIMATION_RATE_ONE / 16,
        ACTOR_444000_COLLAPSED_FLOOR_LOWER_Y = 500,
        ACTOR_444000_COLLAPSED_FLOOR_UPPER_Y = 800,
    };
    GluttonWork* work  = task->work;
    Enemy*       enemy = task->spawnArg2.pointer;
    SVECTOR*     floorVertices;
    s32          command;

    // Retain every context, including ignored commands; the cached selector is a byte.
    work->lastCommandStage = request->context.loc.stage;
    work->lastCommandArea  = request->context.loc.area;
    work->lastCommand      = (u8)request->command;

    if (request->context.key == ACTOR_444000_COMMAND_CONTEXT) {
        command = request->command;
        switch (command) {
            case ACTOR_444000_COMMAND_STOP:
                work->state = ACTOR_444000_STATE_DORMANT;
                sndEvtRequestScriptStop(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0A), SOUND_SCRIPT_STOP_KEEP_RELEASE);
                sndEvtRequestScriptStop(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0D), SOUND_SCRIPT_STOP_KEEP_RELEASE);
                break;

            case ACTOR_444000_COMMAND_REPOSITION:
                work->animId   = ACTOR_444000_REPOSITION_CLIP;
                work->animStep = GLUTTON_ANIM_STEP_RESTART;
                _gluttonTickAnim(task);
                _gluttonTickAnim(task);
                _gluttonTickAnim(task);
                work->animRate = ACTOR_444000_REPOSITION_FINAL_RATE;
                _gluttonTickAnim(task);
                work->animRate                        = ANIMATION_RATE_ONE;
                task->extra.tmd->coords->coord.t[0]   = -0xBB8;
                task->extra.tmd->coords->coord.t[1]   = 0;
                task->extra.tmd->coords->coord.t[2]   = -0x992;
                task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                work->state                           = ACTOR_444000_STATE_RETURN_TO_ADVANCE;
                sndEvtRequestScriptStop(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0A), SOUND_SCRIPT_STOP_KEEP_RELEASE);
                sndEvtRequestScriptStop(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0D), SOUND_SCRIPT_STOP_KEEP_RELEASE);
                break;

            case ACTOR_444000_COMMAND_SKIP_COLLAPSE:
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                worldCoordSetActorColorMode(work->escorts[3], ENEMY_COLOR_BLACK);
                work->state      = ACTOR_444000_STATE_COLLAPSED;
                work->prevState  = GLUTTON_STATE_REENTER;
                work->viewLocked = 1;
                sndEvtRequestScriptStop(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR, 7), SOUND_SCRIPT_STOP_KEEP_RELEASE);

                // Complete the collapse floor pose when the presentation is skipped.
                floorVertices        = gWorldCollisionActiveGrid->vertices;
                floorVertices[24].vy = ACTOR_444000_COLLAPSED_FLOOR_LOWER_Y;
                floorVertices[25].vy = ACTOR_444000_COLLAPSED_FLOOR_LOWER_Y;
                floorVertices[26].vy = ACTOR_444000_COLLAPSED_FLOOR_UPPER_Y;
                floorVertices[27].vy = ACTOR_444000_COLLAPSED_FLOOR_UPPER_Y;
                floorVertices[28].vy = ACTOR_444000_COLLAPSED_FLOOR_LOWER_Y;
                floorVertices[29].vy = ACTOR_444000_COLLAPSED_FLOOR_LOWER_Y;
                floorVertices[30].vy = ACTOR_444000_COLLAPSED_FLOOR_UPPER_Y;
                floorVertices[31].vy = ACTOR_444000_COLLAPSED_FLOOR_UPPER_Y;

                _actor444000FlattenRoot(task);
                break;
        }
    }
    return 1;
}

/// Resets the host root to its current yaw at unity scale and enters idle.
///
/// Requires the live host work/model and 0x58 aligned scratch bytes including
/// nested rotation work. Discards pitch, roll and previous scale, preserves
/// translation/hierarchy, dirties composition and clears model draw flags.
/// All reservations are released before return.
static inline void _actor444000InitializeRootPose(Task* task, GluttonWork* work)
{
    GfxCoord* rootCoord = task->extra.tmd->coords;

    _actorRenderRescaleYaw(rootCoord, ONE);
    work->state            = ACTOR_444000_STATE_IDLE;
    task->extra.tmd->flags = 0;
}

/// Restores missing primitive buffers on the host and its occupied escort slots.
///
/// Requires live models and work. Existing buffers and draw flags are preserved;
/// each successful auxiliary-heap allocation remains owned by its model. Failure
/// leaves the buffer NULL. Pointers are borrowed only for this call.
static inline void _actor444000EnsureModelBuffers(TmdObject* hostModel, const GluttonWork* hostWork)
{
    TmdObject* escortModel;
    s16        escortIndex;

    if (hostModel->buffer == NULL) {
        tmdAllocPrimitiveBuffer(hostModel);
    }
    for (escortIndex = 0; escortIndex < ARRAY_SIZE(hostWork->escorts); escortIndex++) {
        if (hostWork->escorts[escortIndex] != NULL) {
            escortModel = hostWork->escorts[escortIndex]->task->extra.tmd;
            if (escortModel->buffer == NULL) {
                tmdAllocPrimitiveBuffer(escortModel);
            }
        }
    }
}

/// Initializes the incinerator Glutton host, its attached enemies and collision bodies.
///
/// Allocates task-owned primary-heap GluttonWork and installs the host exit
/// callback. Requires a live host Enemy/model, loaded animation/descriptor data
/// and successful required escort spawns. Slots 0..5 are always spawned; slot 6
/// is omitted when spawnArg1's high halfword is nonzero. Host and escorts borrow
/// rig, contact and lighting storage from the host work until teardown.
/// Allocation failure destroys the host; subsequent child-spawn failures retain
/// the original unchecked behavior. Successful setup publishes the host task.
static void _actor444000Spawn(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_444000_HOST_INITIAL_CLIP       = 2,
        ACTOR_444000_HEAD_SPHERE_RADIUS      = 768,
        ACTOR_444000_ROOT_SPHERE_RADIUS      = 3000,
        ACTOR_444000_INITIAL_WALL_DISTANCE   = 4000,
        ACTOR_444000_INITIAL_BATTLE_REFS     = 10,
        ACTOR_444000_CAUGHT_PLAYER_CLIP      = 1,
        ACTOR_444000_PLAYER_BLEND_FRAMES     = 3,
        ACTOR_444000_MAD_CHASER_COMMAND_NONE = 0,
        ACTOR_444000_COLLISION_BODY_KEY      = 0x20,
        ACTOR_444000_SWIPE_LENGTH            = 7000,
        ACTOR_444000_SWIPE_RADIUS            = 600,
    };
    GluttonWork* work;
    GluttonWork* bufferWork;
    GluttonWork* lightingWork;
    TmdObject*   hostModel;
    TmdObject*   bufferModel;
    GfxCoord*    rootCoord;
    GfxCoord*    swipeCoord;
    Enemy*       escort;
    Task*        escortTask;
    SVECTOR      forwardOffset;
    SVECTOR*     forwardOffsetPointer;
    VECTOR       worldPosition;
    s16          lightingIndex;
    s16          summonIndex;

    hostModel = task->extra.tmd;
    rootCoord = hostModel->coords;

    work       = memCalloc(sizeof(*work), false);
    task->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }

    (sceneAcquireBattleRef)(0);
    task->exitCallback = _gluttonExit;

    enemy->field_4    = &task->extra.tmd->coords->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = -0xC8;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &task->extra.tmd->coords[4];
    worldTargetLinkNode(&enemy->node);
    enemy->reactionFlags = 0;
    enemy->hp            = _gActor444000GluttonHostParams.hpMax;
    enemy->param         = &_gActor444000GluttonHostParams;
    enemy->recs          = work->hits[0].contacts;

    // The work owns both driving rigs and the collision records borrowed by enemy/model APIs.
    animationInitContext(&work->hostRig.anim, _gActor444000GluttonHostAnimationSets, hostModel, work->hostRig.poses, work->hostRig.slots);
    animationInitContext(&work->hostBlendRig.anim, _gActor444000GluttonHostAnimationSets, hostModel, work->hostBlendRig.poses, work->hostBlendRig.slots);

    work->animStep        = GLUTTON_ANIM_STEP_RESTART;
    work->animId          = ACTOR_444000_HOST_INITIAL_CLIP;
    work->limbPoseEnabled = 1;
    work->blending        = 0;
    work->neckYawTarget = work->neckYaw = 0;
    work->animRate = work->field_7B8 = ANIMATION_RATE_ONE;

    worldCollisionBindEnemySphere(&task->extra.tmd->coords[4], &work->hits[0].body, work->hits[0].contacts, ARRAY_SIZE(work->hits[0].contacts), ACTOR_444000_COLLISION_BODY_KEY, ACTOR_444000_HEAD_SPHERE_RADIUS);
    worldCollisionBindEnemySphere(&task->extra.tmd->coords[4], &work->hits[1].body, work->hits[1].contacts, ARRAY_SIZE(work->hits[1].contacts), ACTOR_444000_COLLISION_BODY_KEY, ACTOR_444000_HEAD_SPHERE_RADIUS);
    worldCollisionBindEnemySphere(&task->extra.tmd->coords[1], &work->hits[2].body, work->hits[2].contacts, ARRAY_SIZE(work->hits[2].contacts), ACTOR_444000_COLLISION_BODY_KEY, ACTOR_444000_ROOT_SPHERE_RADIUS);

    work->hits[1].body.pos.vx = 0;
    work->hits[1].body.pos.vy = 0;
    work->hits[1].body.pos.vz = -0x100;
    work->hits[2].body.pos.vx = 0;
    work->hits[2].body.pos.vy = 0x400;
    work->hits[2].body.pos.vz = -0x400;

    gfxReadMatrixZAxis(&task->extra.tmd->coords->coord, &forwardOffset);
    forwardOffset.vy     = 0;
    forwardOffsetPointer = &forwardOffset;
    VectorNormalSS(forwardOffsetPointer, forwardOffsetPointer);
    gte_lddp(0x1388);
    gte_ldsv(forwardOffsetPointer);
    gte_gpf12();
    gte_stsv(forwardOffsetPointer);

    work->playerAnim.source.sets          = NULL;
    work->playerAnim.animationId          = ACTOR_444000_CAUGHT_PLAYER_CLIP;
    work->playerAnim.blend                = ANIMATION_BLEND_RESET;
    work->playerAnim.blendFrames          = ACTOR_444000_PLAYER_BLEND_FRAMES;
    work->playerAnim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
    work->deathTicks                      = 0;
    task->msgTable                        = _gActor444000GluttonMessages;
    rootCoord->parent                     = &gGfxViewCoord;
    rootCoord->composeStamp               = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);

    _gActor444000GluttonRootEffectSpawnArg.coord      = task->extra.tmd->coords;
    _gActor444000GluttonRootEffectSpawnArg.spawnArgLo = 0x100;
    _gActor444000GluttonRootEffectSpawnArg.spawnArgHi = 2;
    work->prevState                                   = GLUTTON_STATE_REENTER;

    bufferModel = task->extra.tmd;
    bufferWork  = task->work;
    _actor444000EnsureModelBuffers(bufferModel, bufferWork);

    _actor444000InitializeRootPose(task, work);

    // Escort tasks are independent enemies, with roots attached to the host hierarchy.
    escort                                                = enemySpawnFromTable(_gActor444000GluttonAttachedTaskDescs, 0, 0, task->spawnArg2.pointer);
    work->escorts[0]                                      = escort;
    escort->task->extra.tmd->coords->parent               = task->extra.tmd->coords;
    work->escorts[0]->task->extra.tmd->coords->coord.t[0] = 0;
    work->escorts[0]->task->extra.tmd->coords->coord.t[1] = 0;
    work->escorts[0]->task->extra.tmd->coords->coord.t[2] = 0;
    work->escorts[0]->task->extra.tmd->flags              = 0;
    animationInitContext(&work->escort0Rig.anim, _gActor444000GluttonRightLegAnimationSets, work->escorts[0]->task->extra.tmd, work->escort0Rig.poses,
                         work->escort0Rig.slots);
    animationInitContext(&work->escort0BlendRig.anim, _gActor444000GluttonRightLegAnimationSets, work->escorts[0]->task->extra.tmd, work->escort0BlendRig.poses,
                         work->escort0BlendRig.slots);
    work->escorts[0]->field_4    = &task->extra.tmd->coords->coord;
    work->escorts[0]->field_48   = 0;
    work->escorts[0]->bodyPos.vx = 0xC8;
    work->escorts[0]->bodyPos.vy = 0;
    work->escorts[0]->bodyPos.vz = 0x3E8;
    work->escorts[0]->coord      = &work->escorts[0]->task->extra.tmd->coords[1];
    worldTargetLinkNode(&work->escorts[0]->node);
    work->escorts[0]->reactionFlags = 0;
    work->escorts[0]->hp            = _gActor444000GluttonHostParams.hpMax;
    work->groups3To5Pool            = _gActor444000GluttonRightLegParams.hpMax;
    work->escorts[0]->param         = &_gActor444000GluttonRightLegParams;
    work->escorts[0]->recs          = work->hits[3].contacts;
    worldCollisionBindEnemySphere(&work->escorts[0]->task->extra.tmd->coords[1], &work->hits[3].body, work->hits[3].contacts, ARRAY_SIZE(work->hits[3].contacts),
                                  ACTOR_444000_COLLISION_BODY_KEY, ACTOR_444000_HEAD_SPHERE_RADIUS);
    worldCollisionBindEnemySphere(&work->escorts[0]->task->extra.tmd->coords[2], &work->hits[4].body, work->hits[4].contacts, ARRAY_SIZE(work->hits[4].contacts),
                                  ACTOR_444000_COLLISION_BODY_KEY, ACTOR_444000_HEAD_SPHERE_RADIUS);
    worldCollisionBindEnemySphere(&work->escorts[0]->task->extra.tmd->coords[3], &work->hits[5].body, work->hits[5].contacts, ARRAY_SIZE(work->hits[5].contacts),
                                  ACTOR_444000_COLLISION_BODY_KEY, ACTOR_444000_HEAD_SPHERE_RADIUS);

    escort                                                = enemySpawnFromTable(_gActor444000GluttonAttachedTaskDescs, 1, 0, task->spawnArg2.pointer);
    work->escorts[1]                                      = escort;
    escort->task->extra.tmd->coords->parent               = task->extra.tmd->coords;
    work->escorts[1]->task->extra.tmd->coords->coord.t[0] = 0;
    work->escorts[1]->task->extra.tmd->coords->coord.t[1] = 0;
    work->escorts[1]->task->extra.tmd->coords->coord.t[2] = 0;
    work->escorts[1]->task->extra.tmd->flags              = 0;
    animationInitContext(&work->escort1Rig.anim, _gActor444000GluttonLeftLegAnimationSets, work->escorts[1]->task->extra.tmd, work->escort1Rig.poses,
                         work->escort1Rig.slots);
    animationInitContext(&work->escort1BlendRig.anim, _gActor444000GluttonLeftLegAnimationSets, work->escorts[1]->task->extra.tmd, work->escort1BlendRig.poses,
                         work->escort1BlendRig.slots);
    work->escorts[1]->field_4    = &task->extra.tmd->coords->coord;
    work->escorts[1]->field_48   = 0;
    work->escorts[1]->bodyPos.vx = -0xC8;
    work->escorts[1]->bodyPos.vy = 0;
    work->escorts[1]->bodyPos.vz = 0x3E8;
    work->escorts[1]->coord      = &work->escorts[1]->task->extra.tmd->coords[1];
    worldTargetLinkNode(&work->escorts[1]->node);
    work->escorts[1]->reactionFlags = 0;
    work->escorts[1]->hp            = _gActor444000GluttonHostParams.hpMax;
    work->groups6To8Pool            = _gActor444000GluttonLeftLegParams.hpMax;
    work->escorts[1]->param         = &_gActor444000GluttonLeftLegParams;
    work->escorts[1]->recs          = work->hits[6].contacts;
    worldCollisionBindEnemySphere(&work->escorts[1]->task->extra.tmd->coords[1], &work->hits[6].body, work->hits[6].contacts, ARRAY_SIZE(work->hits[6].contacts),
                                  ACTOR_444000_COLLISION_BODY_KEY, ACTOR_444000_HEAD_SPHERE_RADIUS);
    worldCollisionBindEnemySphere(&work->escorts[1]->task->extra.tmd->coords[2], &work->hits[7].body, work->hits[7].contacts, ARRAY_SIZE(work->hits[7].contacts),
                                  ACTOR_444000_COLLISION_BODY_KEY, ACTOR_444000_HEAD_SPHERE_RADIUS);
    worldCollisionBindEnemySphere(&work->escorts[1]->task->extra.tmd->coords[3], &work->hits[8].body, work->hits[8].contacts, ARRAY_SIZE(work->hits[8].contacts),
                                  ACTOR_444000_COLLISION_BODY_KEY, ACTOR_444000_HEAD_SPHERE_RADIUS);

    escort                                                = enemySpawnFromTable(_gActor444000GluttonAttachedTaskDescs, 2, 0, task->spawnArg2.pointer);
    work->escorts[2]                                      = escort;
    escort->task->extra.tmd->coords->parent               = &task->extra.tmd->coords[4];
    work->escorts[2]->task->extra.tmd->coords->coord.t[0] = 0;
    work->escorts[2]->task->extra.tmd->coords->coord.t[1] = 0x59;
    work->escorts[2]->task->extra.tmd->coords->coord.t[2] = -0x64;
    work->escorts[2]->task->extra.tmd->flags              = 0;

    escort                                                = enemySpawnFromTable(_gActor444000GluttonAttachedTaskDescs, 3, 0, task->spawnArg2.pointer);
    work->escorts[3]                                      = escort;
    escort->task->extra.tmd->coords->parent               = &task->extra.tmd->coords[3];
    work->escorts[3]->task->extra.tmd->coords->coord.t[0] = 0;
    work->escorts[3]->task->extra.tmd->coords->coord.t[1] = 0;
    work->escorts[3]->task->extra.tmd->coords->coord.t[2] = 0;
    work->escorts[3]->task->extra.tmd->flags              = 0;
    work->escorts[3]->field_4                             = &task->extra.tmd->coords->coord;
    work->escorts[3]->field_48                            = 0;
    work->escorts[3]->bodyPos.vx                          = 0;
    work->escorts[3]->bodyPos.vy                          = 0;
    work->escorts[3]->bodyPos.vz                          = 0x514;
    work->escorts[3]->coord                               = work->escorts[3]->task->extra.tmd->coords;
    worldTargetLinkNode(&work->escorts[3]->node);
    work->escorts[3]->reactionFlags = 0;
    work->escorts[3]->hp            = _gActor444000GluttonHostParams.hpMax;
    work->groups1To2Pool            = _gActor444000GluttonGroups1To2Params.hpMax;
    work->escorts[3]->param         = &_gActor444000GluttonGroups1To2Params;
    work->escorts[3]->recs          = work->hits[1].contacts;

    // The swipe has its own root-relative capsule and five-contact table.
    swipeCoord              = &work->swipeCoord;
    work->swipeCoord.parent = task->extra.tmd->coords;
    gfxSetRotIdentity(&work->swipeCoord.coord);
    work->swipeCoord.coord.t[0] = work->swipeCoord.coord.t[1] = work->swipeCoord.coord.t[2] = 0;
    work->swipeCoord.composeStamp                                                           = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(swipeCoord);

    work->swipeCapsule.ends[1].vz   = ACTOR_444000_SWIPE_LENGTH;
    work->swipeCapsule.end0Radius   = ACTOR_444000_SWIPE_RADIUS;
    work->swipeCapsule.end1Radius   = ACTOR_444000_SWIPE_RADIUS;
    work->swipeCapsule.ends[0].vx   = 0;
    work->swipeCapsule.ends[0].vy   = 0;
    work->swipeCapsule.ends[0].vz   = 0;
    work->swipeCapsule.ends[1].vx   = 0;
    work->swipeCapsule.ends[1].vy   = 0;
    work->swipeCapsule.contacts     = work->swipeContacts;
    work->swipeBody.coord           = swipeCoord;
    work->swipeBody.context.capsule = &work->swipeCapsule;
    work->swipeBody.pos.vx          = 0;
    work->swipeBody.pos.vy          = -0xFA;
    work->swipeBody.pos.vz          = 0x25F;
    work->swipeBody.key             = WORLD_COLLISION_CONTACT_ENEMY_BODY | ACTOR_444000_COLLISION_BODY_KEY;
    work->swipeBody.radius          = 0;
    work->swipeBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->swipeBody);
    worldCollisionInitContacts(work->swipeContacts, ARRAY_SIZE(work->swipeContacts), 0);
    work->swipeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    escort                                                = enemySpawnFromTable(_gActor444000GluttonAttachedTaskDescs, 4, 0, task->spawnArg2.pointer);
    work->escorts[4]                                      = escort;
    escort->task->extra.tmd->coords->parent               = &task->extra.tmd->coords[4];
    work->escorts[4]->task->extra.tmd->coords->coord.t[0] = 0;
    work->escorts[4]->task->extra.tmd->coords->coord.t[1] = 0;
    work->escorts[4]->task->extra.tmd->coords->coord.t[2] = 0x14;
    work->escorts[4]->task->extra.tmd->flags              = 0;

    escort                                                = enemySpawnFromTable(_gActor444000GluttonAttachedTaskDescs, 5, 0, task->spawnArg2.pointer);
    work->escorts[5]                                      = escort;
    escort->task->extra.tmd->coords->parent               = &task->extra.tmd->coords[2];
    work->escorts[5]->task->extra.tmd->coords->coord.t[0] = 0;
    work->escorts[5]->task->extra.tmd->coords->coord.t[1] = 0x67C;
    work->escorts[5]->task->extra.tmd->coords->coord.t[2] = 0xC8;
    work->escorts[5]->task->extra.tmd->flags              = 0;

    if ((task->spawnArg1.value >> 16) == 0) {
        escort                                                = enemySpawnFromTable(_gActor444000GluttonAttachedTaskDescs, 6, 0, task->spawnArg2.pointer);
        work->escorts[6]                                      = escort;
        escort->task->extra.tmd->coords->parent               = &task->extra.tmd->coords[1];
        work->escorts[6]->task->extra.tmd->coords->coord.t[0] = 0;
        work->escorts[6]->task->extra.tmd->coords->coord.t[1] = 0x62C;
        work->escorts[6]->task->extra.tmd->coords->coord.t[2] = 0x5DC;
        work->escorts[6]->task->extra.tmd->flags              = 0;
    } else {
        work->escorts[6] = NULL;
    }

    work->viewLocked     = 0;
    work->viewSelector   = 0;
    work->phase          = 0;
    work->groups6To8Pool = (s16)_gActor444000GluttonLeftLegParams.hpMax;
    work->groups3To5Pool = (s16)_gActor444000GluttonRightLegParams.hpMax;

    lightingWork = task->work;

    _gGluttonEncounterEnded = GLUTTON_ENCOUNTER_RUNNING;

    task->extra.tmd->lightMtx = &lightingWork->lightMtx;
    task->extra.tmd->colorMtx = &lightingWork->colorMtx;
    for (lightingIndex = 0; lightingIndex < ARRAY_SIZE(lightingWork->escorts); lightingIndex++) {
        escort = lightingWork->escorts[lightingIndex];
        if (escort != NULL) {
            escortTask                      = escort->task;
            escortTask->extra.tmd->lightMtx = &lightingWork->lightMtx;
            escortTask->extra.tmd->colorMtx = &lightingWork->colorMtx;
        }
    }

    actorRenderComposeCoord(rootCoord);
    worldPosition.vx = rootCoord->workm.t[0];
    worldPosition.vy = rootCoord->workm.t[1];
    worldPosition.vz = rootCoord->workm.t[2];
    if (work->hostExposed != work->prevHostExposed) {
        worldCoordUpdateActorColor(enemy, &worldPosition, 0, 0);
        worldCoordUpdateActorColor(work->escorts[3], &worldPosition, 0, 0);
        work->prevHostExposed = work->hostExposed;
    }
    if (work->hostExposed != 0) {
        worldCoordUpdateActorColor(enemy, &worldPosition, 0, 0);
    } else {
        worldCoordUpdateActorColor(work->escorts[3], &worldPosition, 0, 0);
    }
    _gluttonTickAnim(task);

    _gActor444000GluttonMadChaserCommand.command.context.loc.stage = ACTOR_444000_MAD_CHASER_CONTEXT_STAGE;
    _gActor444000GluttonMadChaserCommand.command.context.loc.area  = ACTOR_444000_MAD_CHASER_CONTEXT_AREA;
    _gActor444000GluttonMadChaserCommand.command.command           = ACTOR_444000_MAD_CHASER_COMMAND_NONE;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &_gActor444000GluttonMadChaserCommand.command, ACTOR_COMMAND_MESSAGE_APPLY);

    work->wallDistance = work->wallDistanceTarget = ACTOR_444000_INITIAL_WALL_DISTANCE;
    for (summonIndex = 0; summonIndex < ARRAY_SIZE(work->summons); summonIndex++) {
        work->summons[summonIndex] = NULL;
    }

    gSceneCombatState.battleRefs = ACTOR_444000_INITIAL_BATTLE_REFS;
    sceneReleaseBattleRefWithRewards(task, 0x20);
    _gGluttonHostTask.task = task;
    work->summonsSpawned = work->summonsAlive = 0;
    task->state                              += 1;
}

#include "../../shared/creatures/glutton/glutton_hit.inc.c"

/// Processes the first attack on the boss's first three-part escort.
///
/// Requires live host, escort 0, contact tables and scratch space. Scans groups
/// 3..5 in order, stopping each prefix at a zero key. Damage uses a signed-halfword
/// 3D player offset, critical multiplication and division by six with a nonzero
/// floor. It subtracts from host HP and the part pool, queues the escort readout
/// and stores a wrapped 4096-unit contact yaw. Pool depletion and critical
/// reactions select summon only when the current state permits reactions.
static void _actor444000HitGroups3To5(Task* task)
{
    enum {
        ACTOR_444000_PART_DAMAGE_DIVISOR        = 6,
        ACTOR_444000_CRITICAL_DAMAGE_MULTIPLIER = 4,
    };
    GluttonHitScratch*     hitScratch;
    GluttonWork*           work;
    Enemy*                 host;
    PlayerStatus*          playerStatus;
    GfxCoord*              hitCoord;
    WorldCollisionContact* group3Contacts;
    WorldCollisionContact* group4Contacts;
    WorldCollisionContact* group5Contacts;
    SVECTOR*               group3ContactPoint;
    SVECTOR*               group4ContactPoint;
    SVECTOR*               group5ContactPoint;
    s32                    attackKey;
    s32                    distanceXSquared;
    s32                    distanceYSquared;
    s32                    distanceZSquared;
    u32                    reducedDamage;
    s16                    contactYaw;
    s16                    currentState;

    playerStatus = &gPlayerStatus;
    host         = task->spawnArg2.pointer;
    work         = task->work;
    hitScratch   = SCRATCH_STACK_RESERVE_BLOCK(GluttonHitScratch);
    // Search in part order; a zero key terminates each contact prefix.
    group3ContactPoint    = &hitScratch->contactPoint;
    group3Contacts        = work->hits[3].contacts;
    attackKey             = _gluttonFindHit(group3ContactPoint, group3Contacts, ARRAY_SIZE(work->hits[3].contacts));
    hitScratch->attackKey = attackKey;
    if (attackKey != 0) {
        hitCoord = work->hits[3].body.coord;
        goto hit;
    }

    group4ContactPoint    = &hitScratch->contactPoint;
    group4Contacts        = work->hits[4].contacts;
    attackKey             = _gluttonFindHit(group4ContactPoint, group4Contacts, ARRAY_SIZE(work->hits[4].contacts));
    hitScratch->attackKey = attackKey;
    if (attackKey != 0) {
        hitCoord = work->hits[4].body.coord;
    hit:
        _gluttonHitEffect(hitCoord, attackKey);
        if (hitScratch->attackKey != 0) {
            goto body;
        }
    }

    group5ContactPoint    = &hitScratch->contactPoint;
    group5Contacts        = work->hits[5].contacts;
    attackKey             = _gluttonFindHit(group5ContactPoint, group5Contacts, ARRAY_SIZE(work->hits[5].contacts));
    hitScratch->attackKey = attackKey;
    if (attackKey == 0) {
        goto out;
    }
    _gluttonHitEffect(work->hits[5].body.coord, attackKey);
    if (hitScratch->attackKey == 0) {
        goto out;
    }
body:
    // Share damage with the host and refill the part pool only when reactions are allowed.
    work->groups3To5Cooldown = damageGetPlayerAttackHitCooldown(hitScratch->attackKey);
    damageGetPlayerAttackReaction(hitScratch->attackKey);

    hitScratch->toPlayer.vx    = (playerStatus->coordMtx->t[0] - task->extra.tmd->coords->coord.t[0]) + 0x51F;
    distanceXSquared           = hitScratch->toPlayer.vx * hitScratch->toPlayer.vx;
    hitScratch->toPlayer.vy    = (playerStatus->coordMtx->t[1] - task->extra.tmd->coords->coord.t[1]) - 0xFA;
    distanceYSquared           = hitScratch->toPlayer.vy * hitScratch->toPlayer.vy;
    hitScratch->toPlayer.vz    = (playerStatus->coordMtx->t[2] - task->extra.tmd->coords->coord.t[2]) + 0x25F;
    distanceZSquared           = hitScratch->toPlayer.vz * hitScratch->toPlayer.vz;
    hitScratch->playerDistance = SquareRoot0(distanceXSquared + distanceYSquared + distanceZSquared);
    hitScratch->damage         = damageComputePlayerAttack(hitScratch->attackKey, hitScratch->playerDistance, 0, 0);

    if (damageRollCriticalHit(work->escorts[0], hitScratch->attackKey, 0) != 0 && (currentState = work->state, currentState != ACTOR_444000_STATE_CATCH_PLAYER) && currentState != GLUTTON_STATE_INHALE &&
        currentState != GLUTTON_STATE_ADVANCE && currentState != GLUTTON_STATE_SUMMON && currentState != GLUTTON_STATE_HEAL && currentState != GLUTTON_STATE_RETRACT_LIMB && currentState != GLUTTON_STATE_SWIPE && work->phase != 0 &&
        work->playerCaught != 1) {
        hitScratch->offset.vz = 0x1F4;
        hitScratch->offset.vy = 0;
        hitScratch->offset.vx = 0;
        hitScratch->offset.vy = 0;
        hitScratch->offset.vx = 0;
        hitScratch->offset.vz = 0x258;
        effectSpawn(EFFECT_CRITICAL_HIT, &work->escorts[0]->task->extra.tmd->coords[1], 0, &hitScratch->offset);
        hitScratch->damage *= ACTOR_444000_CRITICAL_DAMAGE_MULTIPLIER;
        work->state         = GLUTTON_STATE_SUMMON;
    }

    reducedDamage = hitScratch->damage / ACTOR_444000_PART_DAMAGE_DIVISOR;
    if (reducedDamage == 0) {
        if (hitScratch->damage == 0) {
            hitScratch->damage = 0;
        } else {
            hitScratch->damage = 1;
        }
    } else {
        hitScratch->damage = reducedDamage;
    }
    damageAccumulateLifeDrainHp(host, hitScratch->attackKey, hitScratch->damage, 0);
    host->hp             -= hitScratch->damage;
    work->groups3To5Pool -= hitScratch->damage;
    if (work->groups3To5Pool <= 0 && (currentState = work->state, currentState != ACTOR_444000_STATE_CATCH_PLAYER) && currentState != GLUTTON_STATE_INHALE && currentState != GLUTTON_STATE_ADVANCE && currentState != GLUTTON_STATE_SUMMON &&
        currentState != GLUTTON_STATE_HEAL && currentState != GLUTTON_STATE_RETRACT_LIMB && currentState != GLUTTON_STATE_SWIPE && work->phase != 0 && work->playerCaught != 1) {
        hitScratch->offset.vy = 0;
        hitScratch->offset.vx = 0;
        hitScratch->offset.vz = 0x258;
        effectSpawn(EFFECT_CRITICAL_HIT, &work->escorts[0]->task->extra.tmd->coords[1], 0, &hitScratch->offset);
        work->state          = GLUTTON_STATE_SUMMON;
        work->groups3To5Pool = (s16)_gActor444000GluttonRightLegParams.hpMax;
    }

    worldTargetAddReadoutAmount(&work->escorts[0]->node, hitScratch->damage, 0);
    work->escorts[0]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(work->escorts[0]->task->extra.tmd->coords);
    hitScratch->offset.vx = hitScratch->contactPoint.vx - work->escorts[0]->task->extra.tmd->coords->workm.t[0];
    hitScratch->offset.vy = hitScratch->contactPoint.vy - work->escorts[0]->task->extra.tmd->coords->workm.t[1];
    hitScratch->offset.vz = hitScratch->contactPoint.vz - work->escorts[0]->task->extra.tmd->coords->workm.t[2];
    contactYaw            = ratan2(hitScratch->offset.vx, hitScratch->offset.vz) -
                 ratan2(-task->extra.tmd->coords->workm.m[2][0],
                        task->extra.tmd->coords->workm.m[2][2]);
    hitScratch->contactYaw = contactYaw;
    hitScratch->contactYaw = _actorAngleNormalizeYaw(contactYaw);

    if (work->animId != GLUTTON_ANIM_SWIPE) {
        work->neckYaw       = 0;
        work->neckYawTarget = 0;
    }
out:
    SCRATCH_STACK_RELEASE_BLOCK(GluttonHitScratch);
}

#include "../../shared/creatures/glutton/glutton_hit_groups6to8.inc.c"

/// Releases the host and all occupied escort slots' primitive buffers.
///
/// Requires live host work and models, and completed GPU use of their buffers.
/// Each model owns its auxiliary-heap buffer; release clears that pointer while
/// leaving the model alive for later reallocation.
static inline void _actor444000ReleaseModelBuffers(Task* task)
{
    GluttonWork* bufferWork = task->work;
    s16          bufferIndex;

    tmdFreePrimitiveBuffer(task->extra.tmd);
    for (bufferIndex = 0; bufferIndex < ARRAY_SIZE(bufferWork->escorts); bufferIndex++) {
        if (bufferWork->escorts[bufferIndex] != NULL) {
            tmdFreePrimitiveBuffer(bufferWork->escorts[bufferIndex]->task->extra.tmd);
        }
    }
}

/// Hides the host and escorts and releases their primitive buffers.
///
/// Requires live host work/model and a live model for each non-NULL escort.
/// Entry requests clip 10 at normal rate and a three-update release countdown.
/// Later ticks advance animation while the state counter is below ten and
/// release buffers at tick two. The outer boss tick also services the countdown.
static void _actor444000DormantState(Task* task)
{
    enum {
        ACTOR_444000_DORMANT_CLIP                = 10,
        ACTOR_444000_DORMANT_RELEASE_DELAY_TICKS = 3,
        ACTOR_444000_DORMANT_RELEASE_TICK        = 2,
        ACTOR_444000_DORMANT_ANIMATION_TICKS     = 10,
    };
    GluttonWork* work;
    GluttonWork* flagWork;

    work = task->work;
    if (work->stateChanged != 0) {
        flagWork               = task->work;
        work->freeCountdown    = ACTOR_444000_DORMANT_RELEASE_DELAY_TICKS;
        task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        _actor444000CopyEscortModelFlags(task, flagWork);
        work->animId     = ACTOR_444000_DORMANT_CLIP;
        work->animStep   = GLUTTON_ANIM_STEP_RESTART;
        work->stateTicks = 0;
        work->animRate   = ANIMATION_RATE_ONE;
        _gluttonTickAnim(task);
    } else {
        if (work->stateTicks < ACTOR_444000_DORMANT_ANIMATION_TICKS) {
            _gluttonTickAnim(task);
        }
        if (work->stateTicks == ACTOR_444000_DORMANT_RELEASE_TICK) {
            _actor444000ReleaseModelBuffers(task);
        }
    }
}

/// Releases the frame retained by `_actorRenderRescaleYawYReserve`.
///
/// Requires its matching reservation at the top of the initialized scratch
/// stack, with any nested reservations already released. Releases one complete
/// `ActorScaleRotScratch`; no scratch pointer may be used afterward.
static inline void _actorRenderReleaseYawScratch(void)
{
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleRotScratch);
}

/// Rebuilds a coordinate at its current yaw with unity X/Z and a requested Y scale.
///
/// Requires a live writable coordinate outside scratch and an initialized stack
/// with 0x58 free aligned bytes. `verticalScale` is a signed 32-bit Q12 factor
/// (`ONE` is unity); the callers use 0 or ONE/4. Discards pitch, roll and old
/// scale, preserves translation/hierarchy and marks composition dirty. Products
/// retain the SDK's arithmetic and halfword narrowing without saturation.
/// Retains one `ActorScaleRotScratch` frame: the caller must release it with
/// `_actorRenderReleaseYawScratch` before its storage is reused.
static inline void _actorRenderRescaleYawYReserve(GfxCoord* coord, s32 verticalScale)
{
    ActorScaleRotScratch* yawScratch;
    s16                   yaw;

    yawScratch = SCRATCH_STACK_RESERVE_BLOCK(ActorScaleRotScratch);

    // Replace the old pitch, roll and scale while preserving the heading.
    yaw             = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    yawScratch->yaw = yaw;
    gfxRotMatrixY(&yawScratch->rotation, yaw, GRAPHICS_ROTATION_REPLACE);
    yawScratch->scale.vx = ONE;
    yawScratch->scale.vy = verticalScale;
    yawScratch->scale.vz = ONE;
    ScaleMatrix(&yawScratch->rotation, &yawScratch->scale);

    // Retain the frame until the caller finishes its coordinate writes.
    _actorRenderCopyRotation(coord, yawScratch->rotation.m);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Keeps the defeated host and its attached body models in their collapsed pose.
///
/// Requires live host work and escorts 0..4 with models, initialized animation
/// rigs and scratch capacity for a yaw rebuild. Entry enables drawing and
/// disables neck overrides. Each tick advances animation and rebuilds six
/// model roots with zero Y scale except escort 3, which retains quarter height.
/// The final dirty write targets escort 2 again, as the original does.
static void _actor444000CollapsedState(Task* task)
{
    GluttonWork* work;
    GluttonWork* flagWork;

    work = task->work;
    if (work->stateChanged != 0) {
        task->extra.tmd->flags  = 0;
        flagWork                = task->work;
        flagWork->freeCountdown = 0;
        task->extra.tmd->flags  = 0;
        _actor444000CopyEscortModelFlags(task, flagWork);
        work->neckPitchEnabled = 0;
        work->neckYawEnabled   = 0;
    }

    _gluttonTickAnim(task);

    // Rebuild each yaw at its collapse scale, preserving the final dirty target.
    _actorRenderRescaleYawY(work->escorts[2]->task->extra.tmd->coords, ONE, 0);
    work->escorts[2]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;

    _actorRenderRescaleYawY(task->extra.tmd->coords, ONE, 0);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;

    _actorRenderRescaleYawY(work->escorts[4]->task->extra.tmd->coords, ONE, 0);
    work->escorts[4]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;

    _actorRenderRescaleYawY(work->escorts[3]->task->extra.tmd->coords, ONE, ONE / 4);
    work->escorts[3]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;

    _actorRenderRescaleYawY(work->escorts[0]->task->extra.tmd->coords, ONE, 0);
    work->escorts[0]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;

    _actorRenderRescaleYawY(work->escorts[1]->task->extra.tmd->coords, ONE, 0);
    work->escorts[2]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Pulls the player toward the mouth and enters the caught-player state on a successful hold.
///
/// Requires live host/player models, placed actor 0, initialized rigs and scratch.
/// Root translations share a parent frame; the mouth origin is transformed to
/// world space. Signed-halfword offsets and clip cues determine each ground-plane
/// pull in world units. Entry restores model buffers and announces room command 2;
/// animation completion announces command 3 and returns to attack selection.
/// The final phase clamps player Z instead of catching; scripted modes suppress
/// pending displacement. Summon pointers are forgotten without destroying tasks.
static void _actor444000InhaleState(Task* task)
{
    enum {
        ACTOR_444000_INHALE_CLIP           = 3,
        ACTOR_444000_INHALE_CATCH_DISTANCE = 1200,
        ACTOR_444000_INHALE_BASE_PULL      = 25,
        ACTOR_444000_INHALE_FINAL_PLAYER_Z = -21200,
    };
    GluttonWork*             work  = task->work;
    Enemy*                   enemy = task->spawnArg2.pointer;
    Task*                    player;
    GameActor*               playerActor;
    _Actor444000DragScratch* dragScratch;
    GluttonWork*             flagWork;
    GluttonWork*             bufferWork;
    PlayerStatus*            playerStatus;
    GfxCoord*                playerCoord;
    GfxCoord*                hostCoord;
    GfxCoord*                hostYawCoord;
    GfxCoord*                playerClampCoord;
    SVECTOR*                 spinnerTarget;
    SVECTOR*                 playerOffset;
    TmdObject*               hostModel;
    s16                      neckYaw;
    s16                      playerOffsetZ;

    player      = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    dragScratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor444000DragScratch);
    playerActor = player->work;

    if (work->stateChanged != 0) {
        work->animId            = ACTOR_444000_INHALE_CLIP;
        work->animStep          = GLUTTON_ANIM_STEP_RESTART;
        flagWork                = task->work;
        flagWork->freeCountdown = 0;
        task->extra.tmd->flags  = 0;
        _actor444000CopyEscortModelFlags(task, flagWork);
        hostModel  = task->extra.tmd;
        bufferWork = task->work;
        _actor444000EnsureModelBuffers(hostModel, bufferWork);
        work->neckPitchTarget  = 0;
        work->neckPitchEnabled = 1;
        work->neckYawEnabled   = 1;
        work->hostExposed      = 0;
        spinnerTarget          = &_gGluttonSpinnerTarget;
        spinnerTarget->vz      = 0;
        spinnerTarget->vy      = 0;
        spinnerTarget->vx      = 0;
        _actorRenderTransformLocalPointToWorld(&sceneFindPlacedActor(0)->extra.tmd->coords[3], spinnerTarget);
        _gActor444000GluttonMadChaserCommand.command.context.loc.stage = ACTOR_444000_MAD_CHASER_CONTEXT_STAGE;
        _gActor444000GluttonMadChaserCommand.command.context.loc.area  = ACTOR_444000_MAD_CHASER_CONTEXT_AREA;
        _gActor444000GluttonMadChaserCommand.command.command           = MAD_CHASER_COMMAND_PULL;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &_gActor444000GluttonMadChaserCommand.command, ACTOR_COMMAND_MESSAGE_APPLY);
    }

    playerCoord = player->extra.tmd->coords;
    if (playerCoord->coord.t[1] > 0) {
        playerCoord->coord.t[1]                 = 0;
        player->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    _gluttonTickAnim(task);

    playerStatus           = &gPlayerStatus;
    hostCoord              = task->extra.tmd->coords;
    playerOffset           = &dragScratch->offset;
    dragScratch->offset.vx = (u16)playerStatus->coordMtx->t[0] - (u16)hostCoord->coord.t[0];
    playerOffset->vy       = (u16)playerStatus->coordMtx->t[1] - (u16)hostCoord->coord.t[1];
    playerOffsetZ          = (u16)playerStatus->coordMtx->t[2] - (u16)hostCoord->coord.t[2];
    playerOffset->vz       = playerOffsetZ;
    hostYawCoord           = task->extra.tmd->coords;
    neckYaw                = ratan2(dragScratch->offset.vx, playerOffsetZ) - ratan2(-hostYawCoord->coord.m[2][0], hostYawCoord->coord.m[2][2]);
    work->neckYawTarget    = _actorAngleNormalizeYaw(neckYaw);

    // Measure from the mouth part in world space before normalizing the pull direction.
    dragScratch->offset.vz = 0;
    dragScratch->offset.vy = 0;
    dragScratch->offset.vx = 0;
    _actorRenderTransformLocalPointToWorld(&task->extra.tmd->coords[4], &dragScratch->offset);

    dragScratch->offset.vx       = (u16)player->extra.tmd->coords->coord.t[0] - (u16)dragScratch->offset.vx;
    dragScratch->offset.vy       = (u16)player->extra.tmd->coords->coord.t[1] - (u16)dragScratch->offset.vy;
    dragScratch->offset.vz       = (u16)player->extra.tmd->coords->coord.t[2] - (u16)dragScratch->offset.vz;
    dragScratch->playerDistance  = dragScratch->offset.vx * dragScratch->offset.vx;
    dragScratch->playerDistance += dragScratch->offset.vz * dragScratch->offset.vz;
    dragScratch->playerDistance  = SquareRoot0(dragScratch->playerDistance);
    VectorNormalSS(&dragScratch->offset, &dragScratch->offset);

    switch (work->phase) {
        case 0:
            dragScratch->padScriptPeriod = 0x19;
            break;
        case 1:
            dragScratch->padScriptPeriod = 0x11;
            break;
        case 2:
        default:
            dragScratch->padScriptPeriod = 0xE;
            break;
    }
    if (((u32)((work->hostRig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) - 0xA) < 9U) && ((work->stateTicks % dragScratch->padScriptPeriod) == 0)) {
        padScriptSpawn(_gActor444000GluttonInhalePadCommands, _gActor444000GluttonInhalePadSegments);
    }

    switch (work->phase) {
        case 0:
        case 6:
            dragScratch->phasePull = 0;
            break;
        case 1:
            dragScratch->phasePull = 5;
            break;
        case 2:
            dragScratch->phasePull = 0xA;
            break;
        case 3:
        case 4:
        case 5:
        default:
            dragScratch->phasePull = 0xF;
            break;
    }

    if (work->stateTicks == 0x3C) {
        s32 soundId;
        s32 soundPan;

        soundId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0A);
        soundPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    if (work->stateTicks == 0xE8) {
        sndEvtRequestScriptStop((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0A), SOUND_SCRIPT_STOP_KEEP_RELEASE);
    }

    // Clip cues select the signed displacement for this tick, including an exposed-body window.
    work->hostExposed = 1;
    switch (work->hostRig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) {
        case 9:
            gte_lddp(-(dragScratch->phasePull + ACTOR_444000_INHALE_BASE_PULL) / 4);
            gte_ldsv(&dragScratch->offset);
            gte_gpf12();
            gte_stsv(&dragScratch->offset);
            work->neckPitchTarget = 0x180;
            break;
        case 10:
            gte_lddp(-(dragScratch->phasePull + ACTOR_444000_INHALE_BASE_PULL) / 2);
            gte_ldsv(&dragScratch->offset);
            gte_gpf12();
            gte_stsv(&dragScratch->offset);
            break;
        case 11:
        case 13:
        case 15:
            gte_lddp(-(dragScratch->phasePull + ACTOR_444000_INHALE_BASE_PULL));
            gte_ldsv(&dragScratch->offset);
            gte_gpf12();
            gte_stsv(&dragScratch->offset);
            work->neckPitchTarget = 0x2B2;
            break;
        case 12:
        case 14:
            gte_lddp(-((dragScratch->phasePull + ACTOR_444000_INHALE_BASE_PULL) * 3) / 2);
            gte_ldsv(&dragScratch->offset);
            gte_gpf12();
            gte_stsv(&dragScratch->offset);
            work->neckPitchTarget = 0x500;
            break;
        case 16:
            gte_lddp(-(dragScratch->phasePull + ACTOR_444000_INHALE_BASE_PULL) / 3);
            gte_ldsv(&dragScratch->offset);
            gte_gpf12();
            gte_stsv(&dragScratch->offset);
            work->neckPitchTarget = 0x100;
            break;
        case 17:
        case 18:
            gte_lddp(-(dragScratch->phasePull + ACTOR_444000_INHALE_BASE_PULL) / 3);
            gte_ldsv(&dragScratch->offset);
            gte_gpf12();
            gte_stsv(&dragScratch->offset);
            work->neckPitchTarget = 0x400;
            break;
        case 19:
        case 20:
            dragScratch->offset.vz = 0;
            dragScratch->offset.vx = 0;
            gte_lddp(-(dragScratch->phasePull + ACTOR_444000_INHALE_BASE_PULL) / 6);
            gte_ldsv(&dragScratch->offset);
            gte_gpf12();
            gte_stsv(&dragScratch->offset);
            work->neckPitchTarget = 0;
            break;
        default:
            work->hostExposed      = 0;
            dragScratch->offset.vz = 0;
            dragScratch->offset.vx = 0;
            break;
    }

    if (((u32)((work->hostRig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) - 0xB) < 5U) && (dragScratch->playerDistance < ACTOR_444000_INHALE_CATCH_DISTANCE) && (work->phase < ACTOR_444000_PHASE_LIFT)) {
        if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &_gActor444000GluttonGrabHold.hold, 0) == 0) {
            work->state        = ACTOR_444000_STATE_CATCH_PLAYER;
            work->playerCaught = 1;
        }
    }
    if (work->phase >= ACTOR_444000_PHASE_LIFT) {
        playerClampCoord = player->extra.tmd->coords;
        if (playerClampCoord->coord.t[2] > ACTOR_444000_INHALE_FINAL_PLAYER_Z) {
            playerClampCoord->coord.t[2] = ACTOR_444000_INHALE_FINAL_PLAYER_Z;
        }
    }

    if (dragScratch->offset.vx != 0 || dragScratch->offset.vz != 0) {
        dragScratch->displacement.vx = dragScratch->offset.vx;
        dragScratch->displacement.vy = 0;
        dragScratch->displacement.vz = dragScratch->offset.vz;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != 2 && gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != 0xA && playerActor->mode != GAME_ACTOR_MODE_SCRIPTED) {
            playerActorSetPendingDisplacement(&dragScratch->displacement);
        }
    }

    if (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        _gActor444000GluttonMadChaserCommand.command.context.loc.stage = ACTOR_444000_MAD_CHASER_CONTEXT_STAGE;
        _gActor444000GluttonMadChaserCommand.command.context.loc.area  = ACTOR_444000_MAD_CHASER_CONTEXT_AREA;
        _gActor444000GluttonMadChaserCommand.command.command           = MAD_CHASER_COMMAND_VANISH;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &_gActor444000GluttonMadChaserCommand.command, ACTOR_COMMAND_MESSAGE_APPLY);
        work->state = ACTOR_444000_STATE_CHOOSE_ATTACK;
        for (dragScratch->slot = 0; dragScratch->slot < ARRAY_SIZE(work->summons); dragScratch->slot++) {
            work->summons[dragScratch->slot] = NULL;
        }
    }
    work->summonsAlive = 0;
    SCRATCH_STACK_RELEASE_BLOCK(_Actor444000DragScratch);
}

/// Places a living caught player 900 units from the host's fifth-part origin.
///
/// Requires live host/player models, a writable catch scratch block, a complete
/// coordinate chain to world space and initialized scratch for nested calls.
/// The player is placed in the common root frame at their existing height.
/// The nearer facing toward or away from the host selects the animation-set
/// half and placement yaw (4096 units per turn); the host neck aims back at
/// the player. Scratch and player status are borrowed, and the shared placement
/// payload is consumed synchronously. A dead player still updates scratch and
/// animation selection but receives no placement message.
static inline void _actor444000PlaceCaughtPlayer(Task* task, GluttonWork* work,
                                                 Task* player, _Actor444000CatchScratch* catchScratch,
                                                 const PlayerStatus* playerStatus)
{
    enum {
        ACTOR_444000_CAUGHT_PLAYER_DISTANCE = 900,
    };
    GfxCoord* playerCoord;
    s32       playerTurn;

    // Choose the nearer facing while retaining the catch's animation side.
    catchScratch->anchor.vz = 0;
    catchScratch->anchor.vy = 0;
    catchScratch->anchor.vx = 0;
    _actorRenderTransformLocalPointToWorld(&task->extra.tmd->coords[4], &catchScratch->anchor);

    catchScratch->offset.vx = catchScratch->anchor.vx - player->extra.tmd->coords->coord.t[0];
    catchScratch->offset.vy = 0;
    catchScratch->offset.vz = catchScratch->anchor.vz - player->extra.tmd->coords->coord.t[2];
    playerTurn              = _actorAngleTurnToOffset(player->extra.tmd->coords, catchScratch->offset.vx, catchScratch->offset.vz);
    catchScratch->playerYaw = playerTurn;
    if (abs(catchScratch->playerYaw) > ACTOR_TRANSFORM_ANGLE_HALF_TURN / 2) {
        if (catchScratch->playerYaw > 0) {
            catchScratch->playerYaw = playerTurn - ACTOR_TRANSFORM_ANGLE_HALF_TURN;
        } else {
            catchScratch->playerYaw = playerTurn + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
        }
        work->playerAnim.source.sets = (_gActor444000GluttonPlayerCaptureAnimationSets + ACTOR_444000_PLAYER_CAPTURE_FACING_AWAY_BASE);
    } else {
        work->playerAnim.source.sets = _gActor444000GluttonPlayerCaptureAnimationSets;
    }
    playerCoord              = player->extra.tmd->coords;
    catchScratch->playerYaw += ratan2(-playerCoord->coord.m[2][0], playerCoord->coord.m[2][2]);

    catchScratch->offset.vx = player->extra.tmd->coords->coord.t[0] - catchScratch->anchor.vx;
    catchScratch->offset.vy = 0;
    catchScratch->offset.vz = player->extra.tmd->coords->coord.t[2] - catchScratch->anchor.vz;
    work->neckYawTarget     = _actorAngleTurnToOffset(task->extra.tmd->coords, catchScratch->offset.vx, catchScratch->offset.vz);

    // Hold the player at a fixed ground-plane radius from the host part.
    VectorNormalSS(&catchScratch->offset, &catchScratch->offset);
    gte_lddp(ACTOR_444000_CAUGHT_PLAYER_DISTANCE);
    gte_ldsv(&catchScratch->offset);
    gte_gpf12();
    gte_stsv(&catchScratch->offset);

    _gActor444000GluttonCaughtPlayerPlacement.placement.pos.vx = catchScratch->anchor.vx + catchScratch->offset.vx;
    _gActor444000GluttonCaughtPlayerPlacement.placement.pos.vy = player->extra.tmd->coords->coord.t[1];
    _gActor444000GluttonCaughtPlayerPlacement.placement.pos.vz = catchScratch->anchor.vz + catchScratch->offset.vz;
    _gActor444000GluttonCaughtPlayerPlacement.placement.rot.vx = 0;
    _gActor444000GluttonCaughtPlayerPlacement.placement.rot.vy = catchScratch->playerYaw;
    _gActor444000GluttonCaughtPlayerPlacement.placement.rot.vz = 0;
    if (playerStatus->hp > 0) {
        TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_PLACE, &_gActor444000GluttonCaughtPlayerPlacement.placement, 0);
    }
}

/// Holds and damages the player caught by the boss's inhale.
///
/// Requires live host/player models, initialized rigs, writable wall vertices
/// 24..31 and scratch space. Entry corrects root distance below 2900 units to
/// 3300, then applies the part-relative caught placement and disables lock-on.
/// Clip 15 damages a living player every tick; its boundary starts clip 14.
/// Fatal damage selects scripted idle state 10 to retain the caught animation.
/// At tick 23 the caught-player animation is reinstalled. Stopped player playback
/// places them at the host root. Placement payloads are consumed synchronously.
static void _actor444000CatchPlayerState(Task* task)
{
    enum {
        ACTOR_444000_CATCH_DEATH_SOUND_TICKS      = 30,
        ACTOR_444000_CATCH_DEATH_FADE_FRAMES      = 54,
        ACTOR_444000_CATCH_DEATH_RESTART_TICKS    = 90,
        ACTOR_444000_CAUGHT_PLAYER_CLIP           = 1,
        ACTOR_444000_CATCH_CLIP                   = 15,
        ACTOR_444000_CAUGHT_PLAYER_RELEASE_CLIP   = 14,
        ACTOR_444000_CATCH_ENTRY_MIN_DISTANCE     = 2900,
        ACTOR_444000_CATCH_ENTRY_OUTWARD_DISTANCE = 3300,
        ACTOR_444000_CAUGHT_PLAYER_ATTACK         = 3,
        ACTOR_444000_CAUGHT_PLAYER_REPLACE_TICK   = 23,
    };
    _Actor444000CatchScratch* catchScratch;
    GluttonWork*              work;
    GluttonWork*              flagWork;
    GluttonWork*              bufferWork;
    Enemy*                    enemy;
    Task*                     player;
    PlayerStatus*             playerStatus;
    Task*                     damageTarget;
    TmdObject*                hostModel;
    SVECTOR*                  wallVertices;
    s32                       poseCue;
    s32                       cueId;
    s32                       cuePan;
    s32                       hitId;
    s32                       hitPan;
    s32                       endId;
    s32                       endPan;

    work         = task->work;
    enemy        = task->spawnArg2.pointer;
    player       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    playerStatus = &gPlayerStatus;

    if (work->stateChanged != 0) {
        catchScratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor444000CatchScratch);

        wallVertices        = gWorldCollisionActiveGrid->vertices;
        wallVertices[24].vy = ACTOR_444000_COMBAT_WALL_TOP_Y;
        wallVertices[25].vy = ACTOR_444000_COMBAT_WALL_TOP_Y;
        wallVertices[26].vy = ACTOR_444000_COMBAT_WALL_BOTTOM_Y;
        wallVertices[27].vy = ACTOR_444000_COMBAT_WALL_BOTTOM_Y;
        wallVertices[28].vy = ACTOR_444000_COMBAT_WALL_TOP_Y;
        wallVertices[29].vy = ACTOR_444000_COMBAT_WALL_TOP_Y;
        wallVertices[30].vy = ACTOR_444000_COMBAT_WALL_BOTTOM_Y;
        wallVertices[31].vy = ACTOR_444000_COMBAT_WALL_BOTTOM_Y;

        work->animId            = ACTOR_444000_CATCH_CLIP;
        work->animStep          = GLUTTON_ANIM_STEP_RESTART;
        flagWork                = task->work;
        flagWork->freeCountdown = 0;

        task->extra.tmd->flags = 0;
        _actor444000CopyEscortModelFlags(task, flagWork);

        hostModel  = task->extra.tmd;
        bufferWork = task->work;
        _actor444000EnsureModelBuffers(hostModel, bufferWork);

        work->neckPitchTarget  = 0;
        work->neckPitchEnabled = 0;
        work->neckYawEnabled   = 1;
        work->hostExposed      = 0;

        catchScratch->playerDelta.vx = player->extra.tmd->coords->coord.t[0] -
                                       task->extra.tmd->coords->coord.t[0];
        catchScratch->playerDelta.vz = player->extra.tmd->coords->coord.t[2] -
                                       task->extra.tmd->coords->coord.t[2];
        catchScratch->playerDistance = SquareRoot0(catchScratch->playerDelta.vx * catchScratch->playerDelta.vx + catchScratch->playerDelta.vz * catchScratch->playerDelta.vz);
        // Preserve the root-relative outward correction before the part-relative placement.
        if (catchScratch->playerDistance < ACTOR_444000_CATCH_ENTRY_MIN_DISTANCE) {
            catchScratch->offset.vy = 0;
            catchScratch->offset.vx = catchScratch->playerDelta.vx;
            catchScratch->offset.vz = catchScratch->playerDelta.vz;
            VectorNormalSS(&catchScratch->offset, &catchScratch->offset);
            gte_lddp(ACTOR_444000_CATCH_ENTRY_OUTWARD_DISTANCE);
            gte_ldsv(&catchScratch->offset);
            gte_gpf12();
            gte_stsv(&catchScratch->offset);
            player->extra.tmd->coords->coord.t[0] =
                task->extra.tmd->coords->coord.t[0] + catchScratch->offset.vx;
            player->extra.tmd->coords->coord.t[2] =
                task->extra.tmd->coords->coord.t[2] + catchScratch->offset.vz;
            player->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(player->extra.tmd->coords);
        }

        _actor444000PlaceCaughtPlayer(task, work, player, catchScratch, playerStatus);

        _gActor444000GluttonPlayerAtHost.playerAtHost = GLUTTON_PLAYER_NOT_AT_HOST_ROOT;
        gAttachmentState.flags                       |= ATTACHMENT_FLAG_EVENT_LOCK;
        roomEffectRequestCancelAll();
        worldTargetDisableNodeLockOn(&enemy->node);
        worldTargetDisableNodeLockOn(&work->escorts[3]->node);
        worldTargetDisableNodeLockOn(&work->escorts[0]->node);
        worldTargetDisableNodeLockOn(&work->escorts[1]->node);
        sndEvtRequestScriptStop((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0A), SOUND_SCRIPT_STOP_KEEP_RELEASE);

        _gActor444000GluttonMadChaserCommand.command.context.loc.stage = ACTOR_444000_MAD_CHASER_CONTEXT_STAGE;
        _gActor444000GluttonMadChaserCommand.command.context.loc.area  = ACTOR_444000_MAD_CHASER_CONTEXT_AREA;
        _gActor444000GluttonMadChaserCommand.command.command           = MAD_CHASER_COMMAND_VANISH;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &_gActor444000GluttonMadChaserCommand.command, ACTOR_COMMAND_MESSAGE_APPLY);
    } else {
        catchScratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor444000CatchScratch);
        _gluttonTickAnim(task);

        if ((work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) && work->animId == ACTOR_444000_CATCH_CLIP) {
            work->animStep = GLUTTON_ANIM_STEP_RESTART;
            work->animId   = ACTOR_444000_CAUGHT_PLAYER_RELEASE_CLIP;
        }

        // The catch clip applies attack entry 3 every tick while the player remains alive.
        if (work->animId == ACTOR_444000_CATCH_CLIP) {
            if (playerStatus->hp > 0) {
                damageTarget = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                taskMessageDispatch(damageTarget, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, ACTOR_444000_CAUGHT_PLAYER_ATTACK), 0);
                if (playerStatus->hp <= 0) {
                    ((GameActor*)player->work)->state = ACTOR_444000_PLAYER_SCRIPTED_IDLE;
                    gGameSession->deathSoundCountdown = ACTOR_444000_CATCH_DEATH_SOUND_TICKS;
                    gGameSession->deathFadeFrames     = ACTOR_444000_CATCH_DEATH_FADE_FRAMES;
                    gGameSession->deathRestartDelay   = ACTOR_444000_CATCH_DEATH_RESTART_TICKS;
                }
            }
            poseCue = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (poseCue == 0x19 && work->prevSlot3Cue != poseCue) {
                cueId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x11);
                cuePan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
                sndEvtRequestScriptStart(cueId, cuePan,
                                         (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
            }
            work->prevSlot3Cue = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        }

        if (work->animId == ACTOR_444000_CAUGHT_PLAYER_RELEASE_CLIP) {
            poseCue = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (poseCue == 0x1D && work->prevSlot3Cue != poseCue) {
                padScriptSpawnVariableMotorRamp(4, 0xFF, 8);
            }
            poseCue = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (poseCue == 0x23 && work->prevSlot3Cue != poseCue) {
                hitId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x12);
                hitPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
                sndEvtRequestScriptStart(hitId, hitPan,
                                         (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
                padScriptSpawnVariableMotorRamp(4, 0xFF, 8);
            }
            poseCue = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (poseCue == 0x27 && work->prevSlot3Cue != poseCue) {
                endId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x12);
                endPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
                sndEvtRequestScriptStart(endId, endPan,
                                         (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
                padScriptSpawnVariableMotorRamp(4, 0xFF, 8);
            }
            work->prevSlot3Cue = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        }

        if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
            _gActor444000GluttonCaughtPlayerPlacement.placement.pos.vx = task->extra.tmd->coords->coord.t[0];
            _gActor444000GluttonCaughtPlayerPlacement.placement.pos.vy = task->extra.tmd->coords->coord.t[1];
            _gActor444000GluttonCaughtPlayerPlacement.placement.pos.vz = task->extra.tmd->coords->coord.t[2];
            _gActor444000GluttonCaughtPlayerPlacement.placement.rot.vx = 0;
            _gActor444000GluttonCaughtPlayerPlacement.placement.rot.vy = 0;
            _gActor444000GluttonCaughtPlayerPlacement.placement.rot.vz = 0;
            TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_PLACE, &_gActor444000GluttonCaughtPlayerPlacement.placement, 0);
            _gActor444000GluttonPlayerAtHost.playerAtHost = GLUTTON_PLAYER_AT_HOST_ROOT;
        }

        if (work->stateTicks == ACTOR_444000_CAUGHT_PLAYER_REPLACE_TICK) {
            sndEvtRequestScriptStop((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0A), SOUND_SCRIPT_STOP_KEEP_RELEASE);
            _actor444000PlaceCaughtPlayer(task, work, player, catchScratch, playerStatus);
            work->playerAnim.animationId = ACTOR_444000_CAUGHT_PLAYER_CLIP;
            work->playerAnim.blend       = ANIMATION_BLEND_RESET;
            work->playerAnim.blendFrames = 0;
            work->field_F02              = 1;
            TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
        }
    }

    SCRATCH_STACK_RELEASE_BLOCK(_Actor444000CatchScratch);
}

/// Reports whether the contact prefix contains a player or companion body.
///
/// `contactCount` counts readable elements, from 0 to 32767. A zero key stops
/// the scan even if later records are occupied; other keys are classified by
/// their high halfword. Returns 0 or 1 and retains no pointer.
static inline s32 _actor444000HasPlayerBodyContact(const WorldCollisionContact* contacts, s16 contactCount)
{
    enum { ACTOR_444000_EMPTY_CONTACT_KEY = 0 };
    s16 contactIndex;

    for (contactIndex = 0; contactIndex < contactCount; contactIndex++) {
        if (contacts[contactIndex].key.value == ACTOR_444000_EMPTY_CONTACT_KEY) {
            break;
        }
        if ((contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
            return 1;
        }
    }
    return 0;
}

/// Animates the two limb swipes and manages a player caught by the swipe capsule.
///
/// Requires live host/player/escort models, initialized rigs, five swipe contacts
/// and scratch space. Entry restores buffers and aims the capsule at neck yaw.
/// The first strike cue enables pair testing for one update; player-body contact
/// requests a hold and attack entry 4. The caught animation is resent until it
/// can transition to the equipped weapon's recovery clip; fatal replies retain
/// scripted idle state 10 so playback survives fatal damage. Limb reach uses
/// world units and explicit halfword
/// wrapping; state timing returns to attack selection at tick 220.
static void _actor444000SwipeState(Task* task)
{
    enum {
        ACTOR_444000_SWIPE_CONTACT_CUE         = 12,
        ACTOR_444000_SECOND_SWIPE_IMPACT_CUE   = 28,
        ACTOR_444000_SWIPE_END_TICK            = 220,
        ACTOR_444000_SECOND_SWIPE_START_TICK   = 60,
        ACTOR_444000_SWIPE_EXTENSION_TICK      = 41,
        ACTOR_444000_SWIPE_EXTENSION_TICKS     = 6,
        ACTOR_444000_SWIPE_RETRACT_TICK        = 57,
        ACTOR_444000_SWIPE_INITIAL_REACH       = 1600,
        ACTOR_444000_SWIPE_MAX_REACH           = 6000,
        ACTOR_444000_SWIPE_EXTENSION_STEP      = 600,
        ACTOR_444000_SWIPE_RETRACT_FAST_LIMIT  = 3001,
        ACTOR_444000_SWIPE_RETRACT_FAST_STEP   = 200,
        ACTOR_444000_SWIPE_RETRACT_SLOW_STEP   = 30,
        ACTOR_444000_SWIPE_CAUGHT_PLAYER_CLIP  = 2,
        ACTOR_444000_SWIPE_PLAYER_RECOVER_CLIP = 4,
        ACTOR_444000_SWIPE_WEAPON_RECOVER_SET  = 7,
        ACTOR_444000_SWIPE_PLAYER_BLEND_FRAMES = 3,
        ACTOR_444000_SWIPE_PLAYER_HOLD_TICKS   = 40,
        ACTOR_444000_SWIPE_PLAYER_RECOVER_TICK = 23,
        ACTOR_444000_SWIPE_FATAL_REPLY         = 1,
        ACTOR_444000_LIMB_POSE_DOWNWARD        = 0,
        ACTOR_444000_LIMB_POSE_UPWARD          = 1,
        ACTOR_444000_LIMB_POSE_HOLD_TIP        = 2,
        ACTOR_444000_LIMB_POSE_SHALLOW_CURVE   = 3,
        ACTOR_444000_LIMB_POSE_FOLD_BASE       = 4,
        ACTOR_444000_LIMB_POSE_FAST_DOWNWARD   = 5,
        ACTOR_444000_SECOND_SWIPE_CLIP         = 5,
        ACTOR_444000_SWIPE_SCRATCH_BYTES       = 48,
        ACTOR_444000_SWIPE_ATTACK              = 4,
    };
    GluttonWork* work;
    GluttonWork* flagWork;
    GluttonWork* bufferWork;
    Enemy*       enemy;
    Task*        player;
    Task*        damageTarget;
    TmdObject*   hostModel;
    GfxCoord*    swipeCoord;
    s16          playerCaught;
    s32          firstSwipeCue;
    s32          secondSwipeCue;
    s32          resetId;
    s32          resetPan;
    s32          swipeId;
    s32          swipePan;
    s32          swipe2Id;
    s32          swipe2Pan;
    s32          hitId;
    s32          hitPan;
    s32          cueId;
    s32          cuePan;
    u16          caughtTicks;

    work   = task->work;
    enemy  = task->spawnArg2.pointer;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    // This reservation is unused by the recovered body and remains around nested operations.
    SCRATCH_STACK_RESERVE_BYTES(ACTOR_444000_SWIPE_SCRATCH_BYTES);

    if (work->stateChanged != 0) {
        work->lastAttack        = GLUTTON_STATE_SWIPE;
        work->animId            = GLUTTON_ANIM_SWIPE;
        work->animStep          = GLUTTON_ANIM_STEP_RESTART;
        flagWork                = task->work;
        flagWork->freeCountdown = 0;
        task->extra.tmd->flags  = 0;
        _actor444000CopyEscortModelFlags(task, flagWork);
        hostModel  = task->extra.tmd;
        bufferWork = task->work;
        _actor444000EnsureModelBuffers(hostModel, bufferWork);
        work->neckYawEnabled   = 1;
        work->neckPitchEnabled = 0;
        work->hostExposed      = 1;
        work->limbPoseEnabled  = 1;
        gfxRotMatrixY(&work->swipeCoord.coord, work->neckYaw, GRAPHICS_ROTATION_REPLACE);
        work->swipeCoord.composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&work->swipeCoord);
        work->wallDistanceTarget = 0xC80;

        resetId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x17);
        resetPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(resetId, resetPan,
                                 (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }

    swipeCoord                    = &work->swipeCoord;
    work->swipeCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(swipeCoord);

    // Enable the swipe capsule only on the first strike cue; held cues do not retrigger.
    if (work->animId == GLUTTON_ANIM_SWIPE && (firstSwipeCue = work->hostRig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == ACTOR_444000_SWIPE_CONTACT_CUE &&
        work->prevSwipeCue != firstSwipeCue) {
        gfxRotMatrixY(&work->swipeCoord.coord, work->neckYaw, GRAPHICS_ROTATION_REPLACE);
        work->swipeCoord.composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(swipeCoord);
        work->shakeLevel       = GLUTTON_SHAKE_LONG;
        work->swipeBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        padScriptSpawnVariableMotorRamp(0x30, 0xFF, 8);

        swipeId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x19);
        swipePan = (s8)worldCoordGetOriginAudioPan(&work->escorts[0]->task->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(
            swipeId, swipePan,
            (s8)(worldCoordGetOriginAudioDepth(&work->escorts[0]->task->extra.tmd->coords[1]) / 2));

        swipe2Id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x1A);
        swipe2Pan = (s8)worldCoordGetOriginAudioPan(&work->escorts[0]->task->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(
            swipe2Id, swipe2Pan,
            (s8)(worldCoordGetOriginAudioDepth(&work->escorts[0]->task->extra.tmd->coords[1]) / 2));
    } else {
        work->swipeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }

    if (work->animId == ACTOR_444000_SECOND_SWIPE_CLIP && (secondSwipeCue = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == ACTOR_444000_SECOND_SWIPE_IMPACT_CUE &&
        work->prevSwipeCue != secondSwipeCue) {
        work->shakeLevel = GLUTTON_SHAKE_LONG;
        padScriptSpawnVariableMotorRamp(0x20, 0x8F, 8);

        hitId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x1B);
        hitPan = (s8)worldCoordGetOriginAudioPan(&work->escorts[0]->task->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(
            hitId, hitPan,
            (s8)(worldCoordGetOriginAudioDepth(&work->escorts[0]->task->extra.tmd->coords[1]) / 2));
    }

    if (work->animId == GLUTTON_ANIM_SWIPE) {
        work->prevSwipeCue = work->hostRig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    } else {
        work->prevSwipeCue = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    }

    switch (work->stateTicks) {
        case 0x14:
            _gGluttonLimbReach = ACTOR_444000_SWIPE_INITIAL_REACH;
            work->limbPose     = ACTOR_444000_LIMB_POSE_DOWNWARD;
            break;
        case 0x22:
            work->limbPose = ACTOR_444000_LIMB_POSE_UPWARD;
            break;
        case 0x2B:
            work->limbPose = ACTOR_444000_LIMB_POSE_FAST_DOWNWARD;
            cueId          = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x18);
            cuePan         = (s8)worldCoordGetOriginAudioPan(&work->escorts[0]->task->extra.tmd->coords[1]);
            sndEvtRequestScriptStart(
                cueId, cuePan,
                (s8)(worldCoordGetOriginAudioDepth(&work->escorts[0]->task->extra.tmd->coords[1]) /
                     2));
            break;
        case 0x2D:
            work->limbPose = ACTOR_444000_LIMB_POSE_HOLD_TIP;
            break;
        case 0x38:
            work->limbPose = ACTOR_444000_LIMB_POSE_FOLD_BASE;
            break;
        case 0x44:
            work->limbPose = ACTOR_444000_LIMB_POSE_SHALLOW_CURVE;
            break;
        case ACTOR_444000_SWIPE_END_TICK:
            work->state = ACTOR_444000_STATE_CHOOSE_ATTACK;
            break;
    }

    if ((u32)((u16)work->stateTicks - ACTOR_444000_SWIPE_EXTENSION_TICK) < ACTOR_444000_SWIPE_EXTENSION_TICKS && _gGluttonLimbReach < ACTOR_444000_SWIPE_MAX_REACH) {
        _gGluttonLimbReach = (u16)_gGluttonLimbReach + ACTOR_444000_SWIPE_EXTENSION_STEP;
    }

    _gluttonTickAnim(task);

    if (_actor444000HasPlayerBodyContact(work->swipeContacts, ARRAY_SIZE(work->swipeContacts)) && TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &_gActor444000GluttonGrabHold.hold, 0) == 0) {
        damageTarget           = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        work->swipeDamageReply = taskMessageDispatch(damageTarget, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, ACTOR_444000_SWIPE_ATTACK), 0);
        if (work->swipeDamageReply == ACTOR_444000_SWIPE_FATAL_REPLY) {
            ((GameActor*)player->work)->state = ACTOR_444000_PLAYER_SCRIPTED_IDLE;
        }
        work->playerAnim.source.sets = _gActor444000GluttonPlayerCaptureAnimationSets;
        work->playerCaught           = 1;
        work->playerAnim.animationId = ACTOR_444000_SWIPE_CAUGHT_PLAYER_CLIP;
        work->playerAnim.blend       = ANIMATION_BLEND_RESET;
        work->playerAnim.blendFrames = 0;
        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
        work->caughtTicks = 0;
    }

    // Re-send the caught-player clip until the hold can transition to weapon recovery.
    playerCaught = work->playerCaught;
    if (playerCaught == 1 && work->state != ACTOR_444000_STATE_CATCH_PLAYER) {
        caughtTicks       = work->caughtTicks + 1;
        work->caughtTicks = caughtTicks;
        if (work->swipeDamageReply == playerCaught) {
            if (work->playerAnim.animationId == ACTOR_444000_SWIPE_CAUGHT_PLAYER_CLIP) {
                work->playerAnim.source.sets = _gActor444000GluttonPlayerCaptureAnimationSets;
                work->playerAnim.blend       = ANIMATION_BLEND_RESET;
                work->playerAnim.blendFrames = 0;
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
                work->caughtTicks = 0;
            }
        } else if (work->playerAnim.animationId == ACTOR_444000_SWIPE_CAUGHT_PLAYER_CLIP && (s16)caughtTicks < ACTOR_444000_SWIPE_PLAYER_HOLD_TICKS) {
            work->playerAnim.source.sets = _gActor444000GluttonPlayerCaptureAnimationSets;
            work->playerAnim.blend       = ANIMATION_BLEND_RESET;
            work->playerAnim.blendFrames = 0;
            TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
        }

        if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
            switch (work->playerAnim.animationId) {
                case ACTOR_444000_SWIPE_CAUGHT_PLAYER_CLIP:
                    if (work->swipeDamageReply != ACTOR_444000_SWIPE_FATAL_REPLY && (s16)work->caughtTicks >= ACTOR_444000_SWIPE_PLAYER_RECOVER_TICK) {
                        work->playerAnim.source.sets                                                           = _gActor444000GluttonPlayerCaptureAnimationSets;
                        _gActor444000GluttonPlayerCaptureAnimationSets[ACTOR_444000_SWIPE_PLAYER_RECOVER_CLIP] = (gPlayerActorWeaponAnimationBanks
                                                                                                                      [gPlayerActorWeaponAnimationBankBases[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon])[ACTOR_444000_SWIPE_WEAPON_RECOVER_SET];
                        work->playerAnim.animationId                                                           = ACTOR_444000_SWIPE_PLAYER_RECOVER_CLIP;
                        work->playerAnim.blend                                                                 = ANIMATION_BLEND_INTERPOLATE;
                        work->playerAnim.blendFrames                                                           = ACTOR_444000_SWIPE_PLAYER_BLEND_FRAMES;
                        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
                        work->caughtTicks = 0;
                    }
                    break;
                case ACTOR_444000_SWIPE_PLAYER_RECOVER_CLIP:
                    if (work->swipeDamageReply != ACTOR_444000_SWIPE_FATAL_REPLY) {
                        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET, 0);
                        work->playerCaught = 0;
                    }
                    break;
            }
        }
    }

    if (work->stateTicks == ACTOR_444000_SECOND_SWIPE_START_TICK && work->animId == GLUTTON_ANIM_SWIPE) {
        work->animId   = ACTOR_444000_SECOND_SWIPE_CLIP;
        work->animStep = GLUTTON_ANIM_STEP_BLEND;
    }

    if (work->stateTicks >= ACTOR_444000_SWIPE_RETRACT_TICK) {
        if (_gGluttonLimbReach >= ACTOR_444000_SWIPE_RETRACT_FAST_LIMIT) {
            _gGluttonLimbReach = (u16)_gGluttonLimbReach - ACTOR_444000_SWIPE_RETRACT_FAST_STEP;
        } else {
            _gGluttonLimbReach = (u16)_gGluttonLimbReach - ACTOR_444000_SWIPE_RETRACT_SLOW_STEP;
        }
    }

    SCRATCH_STACK_RELEASE_BYTES(ACTOR_444000_SWIPE_SCRATCH_BYTES);
}

/// Throws debris from the first escort's part and spawns the corresponding falling enemies.
///
/// Requires live host/escort 0 models, initialized rigs and persistent debris
/// coordinate storage. From tick 61, every fifth tick refreshes one of three
/// effect coordinates from the part's world pose and offsets it 800 ground-plane
/// units; every tenth tick with remainder 4 spawns descriptor 2's enemy.
/// That spawn must succeed. Slot 1's boundary returns to attack selection.
/// Effect coordinates remain borrowed by their spawned tasks.
static void _actor444000ThrowDebrisState(Task* task)
{
    enum {
        ACTOR_444000_THROW_DEBRIS_CLIP      = 11,
        ACTOR_444000_DEBRIS_LAUNCH_YAW      = 128,
        ACTOR_444000_DEBRIS_LAUNCH_DISTANCE = 800,
        ACTOR_444000_DEBRIS_EFFECT_ARGUMENT = 0x27A0D600,
    };
    GluttonWork* work;
    GluttonWork* flagWork;
    Enemy*       enemy;
    Enemy*       spawned;
    GfxCoord*    launchCoord;
    SVECTOR      launchVector;
    SVECTOR*     launchVectorPointer;
    s32          resetId;
    s32          resetPan;
    s32          cueId;
    s32          cuePan;
    s32          hitId;
    s32          hitPan;

    work  = task->work;
    enemy = task->spawnArg2.pointer;

    if (work->stateChanged != 0) {
        work->lastAttack        = ACTOR_444000_STATE_THROW_DEBRIS;
        work->animId            = ACTOR_444000_THROW_DEBRIS_CLIP;
        work->animStep          = GLUTTON_ANIM_STEP_RESTART;
        flagWork                = task->work;
        flagWork->freeCountdown = 0;

        task->extra.tmd->flags = 0;
        _actor444000CopyEscortModelFlags(task, flagWork);
        work->neckPitchEnabled = 1;
        work->neckYawEnabled   = 1;
        work->hostExposed      = 0;

        resetId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x17);
        resetPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(resetId, resetPan,
                                 (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }

    if (work->stateTicks == 0x3B) {
        cueId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x16);
        cuePan = (s8)worldCoordGetOriginAudioPan(&work->escorts[0]->task->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(cueId, cuePan,
                                 (s8)worldCoordGetOriginAudioDepth(&work->escorts[0]->task->extra.tmd->coords[1]));
    }

    if (work->stateTicks == 0x3C) {
        hitId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0D);
        hitPan = (s8)worldCoordGetOriginAudioPan(&work->escorts[0]->task->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(hitId, hitPan,
                                 (s8)worldCoordGetOriginAudioDepth(&work->escorts[0]->task->extra.tmd->coords[1]));
    }

    if ((s16)(u16)work->stateTicks >= 0x3D) {
        if ((s16)((s16)(u16)work->stateTicks % 5) == 0) {
            if (_gActor444000GluttonDebrisCoordIndex >= (s32)ARRAY_SIZE(_gActor444000GluttonDebrisCoords.coords) - 1) {
                _gActor444000GluttonDebrisCoordIndex = 0;
            } else {
                _gActor444000GluttonDebrisCoordIndex = (u16)_gActor444000GluttonDebrisCoordIndex + 1;
            }

            // Preserve the part's world pose in a persistent effect coordinate, then offset its origin.
            _actorRenderAccumulateWorldRotation(&work->escorts[0]->task->extra.tmd->coords[1],
                                                &_gActor444000GluttonDebrisCoords.coords[_gActor444000GluttonDebrisCoordIndex].coord);
            _gActor444000GluttonDebrisCoords.coords[_gActor444000GluttonDebrisCoordIndex].parent = &gGfxViewCoord;

            launchVector.vz = 0;
            launchVector.vy = 0;
            launchVector.vx = 0;
            _actorRenderTransformLocalPointToWorld(&work->escorts[0]->task->extra.tmd->coords[1], &launchVector);

            _gActor444000GluttonDebrisCoords.coords[_gActor444000GluttonDebrisCoordIndex].coord.t[0] = launchVector.vx;
            _gActor444000GluttonDebrisCoords.coords[_gActor444000GluttonDebrisCoordIndex].coord.t[1] = launchVector.vy;
            _gActor444000GluttonDebrisCoords.coords[_gActor444000GluttonDebrisCoordIndex].coord.t[2] = launchVector.vz;
            gfxRotMatrixY(&_gActor444000GluttonDebrisCoords.coords[_gActor444000GluttonDebrisCoordIndex].coord, ACTOR_444000_DEBRIS_LAUNCH_YAW, GRAPHICS_ROTATION_COMPOSE);
            gfxRotMatrixX(&_gActor444000GluttonDebrisCoords.coords[_gActor444000GluttonDebrisCoordIndex].coord, -ACTOR_444000_DEBRIS_LAUNCH_YAW, GRAPHICS_ROTATION_COMPOSE);
            gfxReadMatrixZAxis(&_gActor444000GluttonDebrisCoords.coords[_gActor444000GluttonDebrisCoordIndex].coord, &launchVector);

            launchVectorPointer = &launchVector;
            launchVector.vy     = 0;
            VectorNormalSS(launchVectorPointer, launchVectorPointer);

            gte_lddp(ACTOR_444000_DEBRIS_LAUNCH_DISTANCE);
            gte_ldsv(launchVectorPointer);
            gte_gpf12();
            gte_stsv(launchVectorPointer);

            launchCoord               = &_gActor444000GluttonDebrisCoords.coords[_gActor444000GluttonDebrisCoordIndex];
            launchCoord->coord.t[0]  += launchVector.vx;
            launchCoord->coord.t[1]  += launchVector.vy;
            launchCoord->coord.t[2]  += launchVector.vz;
            launchCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(launchCoord);
            effectSpawn(EFFECT_196, &_gActor444000GluttonDebrisCoords.coords[_gActor444000GluttonDebrisCoordIndex], ACTOR_444000_DEBRIS_EFFECT_ARGUMENT, NULL);
        }
        if ((s16)((s16)(u16)work->stateTicks % 10) == 4) {
            spawned           = enemySpawnFromTable(_gGluttonProjectileTaskDescs, GLUTTON_PROJECTILE_THROW, 0, task->spawnArg2.pointer);
            spawned->workType = ENEMY_WORK_ACTOR;
            work->lastSpawned = spawned;
        }
    }

    _gluttonTickAnim(task);

    if (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ACTOR_444000_STATE_CHOOSE_ATTACK;
        sndEvtRequestScriptStop((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0D), SOUND_SCRIPT_STOP_KEEP_RELEASE);
    }
}

/// Presents the current scripted limb clip and composes its mouth-relative node.
///
/// Requires live host/escort models, initialized rigs and mouth-coordinate
/// storage. Entry restores model flags/buffers and normal animation rate.
/// Clip 13 cues vibration, clip 9 tick 45 positions the mouth-relative node on
/// part 4, and clip 20 blends into clip 13 at its boundary. Retracts limb reach
/// and advances animation without selecting a new behavior state.
static void _actor444000LimbAnimationState(Task* task)
{
    enum {
        ACTOR_444000_LIMB_RETRACT_MIN_REACH = 401,
        ACTOR_444000_LIMB_RETRACT_STEP      = 200,
        ACTOR_444000_LIMB_RETRACT_CLIP      = 13,
        ACTOR_444000_LIMB_EFFECT_CLIP       = 9,
        ACTOR_444000_LIMB_TRANSITION_CLIP   = 20,
    };
    GluttonWork* work;
    GluttonWork* flagWork;
    GluttonWork* bufferWork;
    TmdObject*   hostModel;
    GfxCoord*    mouthCoord;
    GfxCoord*    hostCoords;
    s32          poseCue;

    work = task->work;
    if (work->stateChanged != 0) {
        flagWork                = task->work;
        flagWork->freeCountdown = 0;
        task->extra.tmd->flags  = 0;
        _actor444000CopyEscortModelFlags(task, flagWork);
        hostModel  = task->extra.tmd;
        bufferWork = task->work;
        _actor444000EnsureModelBuffers(hostModel, bufferWork);
        work->animRate = ANIMATION_RATE_ONE;
    }
    if (work->animId == ACTOR_444000_LIMB_RETRACT_CLIP) {
        poseCue = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (poseCue == 0x15 && work->clip.prevSlot2Cue != poseCue) {
            work->shakeLevel = GLUTTON_SHAKE_LONG;
            padScriptSpawn(_gActor444000GluttonLimbRetractPadCommands, _gActor444000GluttonLimbRetractPadSegments);
        }
        work->clip.prevSlot2Cue = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    }
    if (work->animId == ACTOR_444000_LIMB_EFFECT_CLIP && work->stateTicks == 0x2D) {
        hostCoords = task->extra.tmd->coords;
        mouthCoord = &_gActor444000GluttonMouthCoord;
        gfxSetRotIdentity(&mouthCoord->coord);
        _gActor444000GluttonMouthCoord.coord.t[1]   = -0x64;
        _gActor444000GluttonMouthCoord.coord.t[0]   = 0;
        _gActor444000GluttonMouthCoord.coord.t[2]   = 0x64;
        _gActor444000GluttonMouthCoord.composeStamp = GRAPHICS_COORD_DIRTY;
        _gActor444000GluttonMouthCoord.parent       = &hostCoords[4];
        actorRenderComposeCoord(&_gActor444000GluttonMouthCoord);
    }
    if (work->animId == ACTOR_444000_LIMB_TRANSITION_CLIP && (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        work->animId   = ACTOR_444000_LIMB_RETRACT_CLIP;
        work->animStep = GLUTTON_ANIM_STEP_BLEND;
    }
    if (_gGluttonLimbReach >= ACTOR_444000_LIMB_RETRACT_MIN_REACH) {
        _gGluttonLimbReach = (u16)_gGluttonLimbReach - ACTOR_444000_LIMB_RETRACT_STEP;
    }
    _gluttonTickAnim(task);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Restores model buffers and advances the current limb clip through saved skip progress.
///
/// Requires live host/escort models and initialized rigs. Entry disables neck
/// overrides, ticks collapseSkip/8 times at rate 127 (normal rate is 16), then
/// restores normal rate and stops the inhale sound. Negative skip progress runs
/// no extra ticks. Subsequent updates retract limb reach and cue vibration once
/// at slot 2 cue 28. This handler leaves the selected clip/state intact.
static void _actor444000ResumeLimbAnimationState(Task* task)
{
    enum {
        ACTOR_444000_LIMB_RETRACT_MIN_REACH = 401,
        ACTOR_444000_LIMB_RETRACT_STEP      = 200,
        ACTOR_444000_LIMB_FAST_FORWARD_RATE = 127,
        ACTOR_444000_LIMB_SKIP_DIVISOR      = 8,
    };
    GluttonWork* work;
    GluttonWork* flagWork;
    GluttonWork* bufferWork;
    Enemy*       enemy;
    TmdObject*   hostModel;
    s16          fastForwardTick;
    s32          poseCue;

    work = task->work;
    if (work->stateChanged != 0) {
        enemy                   = task->spawnArg2.pointer;
        flagWork                = task->work;
        flagWork->freeCountdown = 0;
        task->extra.tmd->flags  = 0;
        _actor444000CopyEscortModelFlags(task, flagWork);
        hostModel  = task->extra.tmd;
        bufferWork = task->work;
        _actor444000EnsureModelBuffers(hostModel, bufferWork);
        work->animRate         = ACTOR_444000_LIMB_FAST_FORWARD_RATE;
        work->neckPitchEnabled = 0;
        work->neckYawEnabled   = 0;
        // Skip progress is in normal ticks; keep the original division and 127/16 playback rate.
        for (fastForwardTick = 0; fastForwardTick < work->collapseSkip / ACTOR_444000_LIMB_SKIP_DIVISOR; fastForwardTick++) {
            _gluttonTickAnim(task);
        }
        work->animRate = ANIMATION_RATE_ONE;
        sndEvtRequestScriptStop((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0A), SOUND_SCRIPT_STOP_KEEP_RELEASE);
    }
    if (_gGluttonLimbReach >= ACTOR_444000_LIMB_RETRACT_MIN_REACH) {
        _gGluttonLimbReach = (u16)_gGluttonLimbReach - ACTOR_444000_LIMB_RETRACT_STEP;
    }
    _gluttonTickAnim(task);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    poseCue                               = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    if (poseCue == 0x1C && work->clip.prevSlot2Cue != poseCue) {
        work->shakeLevel = GLUTTON_SHAKE_LONG;
        padScriptSpawn(gActor444000GluttonImpactPadCommands, gActor444000GluttonImpactPadSegments);
    }
    work->clip.prevSlot2Cue = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
}

/// Plays the hit reaction that retracts the limb before another attack choice.
///
/// Requires live enemy/model/work, initialized animation rigs and scratch.
/// Entry exposes the host, hides its HP display, selects clip 13, starts the
/// reaction sound and stops the limb sound. Subsequent ticks retract reach by
/// 200 while it is at least 401, clear the limb blend weight in that case and
/// return to attack selection when host animation slot 1 reaches a boundary.
static void _actor444000RetractLimbState(Task* task)
{
    enum {
        ACTOR_444000_RETRACT_LIMB_CLIP         = 13,
        ACTOR_444000_LIMB_RETRACTION_MIN_REACH = 401,
        ACTOR_444000_LIMB_RETRACTION_STEP      = 200,
        ACTOR_444000_RETRACT_SCRATCH_BYTES     = 12,
    };
    GluttonWork* work;
    Enemy*       enemy;
    TmdObject*   hostModel;
    s32          previousClip;
    s32          soundId;
    s32          soundPan;

    work = task->work;
    if (work->stateChanged != 0) {
        hostModel                     = task->extra.tmd;
        enemy                         = task->spawnArg2.pointer;
        enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        hostModel->flags              = 0;
        previousClip                  = work->animId;
        work->neckPitchEnabled        = 0;
        work->neckYawEnabled          = 0;
        work->hostExposed             = 1;
        if (previousClip != ACTOR_444000_RETRACT_LIMB_CLIP) {
            work->animStep = GLUTTON_ANIM_STEP_BLEND;
            work->animId   = ACTOR_444000_RETRACT_LIMB_CLIP;
        } else {
            work->animStep = GLUTTON_ANIM_STEP_RESTART;
            work->animId   = previousClip;
        }
        soundId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 4);
        soundPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        sndEvtRequestScriptStop((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0D), SOUND_SCRIPT_STOP_KEEP_RELEASE);
        return;
    }
    // This twelve-byte reservation is unused by the recovered body.
    SCRATCH_STACK_RESERVE_BYTES(ACTOR_444000_RETRACT_SCRATCH_BYTES);
    if (_gGluttonLimbReach >= ACTOR_444000_LIMB_RETRACTION_MIN_REACH) {
        work->limbPose     = 0;
        _gGluttonLimbReach = (u16)_gGluttonLimbReach - ACTOR_444000_LIMB_RETRACTION_STEP;
    }
    _gluttonTickAnim(task);
    if (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ACTOR_444000_STATE_CHOOSE_ATTACK;
    }
    SCRATCH_STACK_RELEASE_BYTES(ACTOR_444000_RETRACT_SCRATCH_BYTES);
}

/// Aims the neck at the player and chooses the next attack or arena advance.
///
/// Requires live host/player models and enemy work, initialized rigs and scratch.
/// Entry blends to idle and arms a 40-tick delay only when none is pending.
/// Summons take priority over phase/depth/HP advances, then healing and final
/// phase debris throws take priority over range-based random attack choices.
/// Range is a 3D distance from root + (1311, 250, -607), with signed-halfword
/// components; 3801 and 9001 divide its attack bands. Neck yaw uses 4096 units
/// per turn. The player and host translations must share a parent frame.
static void _actor444000ChooseAttackState(Task* task)
{
    enum {
        ACTOR_444000_ATTACK_DELAY_TICKS             = 40,
        ACTOR_444000_IDLE_CLIP                      = 1,
        ACTOR_444000_LIMB_RETRACTION_MIN_REACH      = 401,
        ACTOR_444000_LIMB_RETRACTION_STEP           = 200,
        ACTOR_444000_PHASE1_ADVANCE_Z               = -7500,
        ACTOR_444000_PHASE3_ADVANCE_Z               = -12500,
        ACTOR_444000_PHASE4_ADVANCE_Z               = -15800,
        ACTOR_444000_PHASE5_ADVANCE_Z               = -17000,
        ACTOR_444000_PHASE4_ADVANCE_HP              = 2500,
        ACTOR_444000_PHASE5_ADVANCE_HP              = 2000,
        ACTOR_444000_ATTACK_RANGE_ORIGIN_X          = 1311,
        ACTOR_444000_ATTACK_RANGE_ORIGIN_Y          = 250,
        ACTOR_444000_ATTACK_RANGE_ORIGIN_NEGATIVE_Z = 607,
        ACTOR_444000_NEAR_ATTACK_DISTANCE           = 3801,
        ACTOR_444000_FAR_ATTACK_DISTANCE            = 9001,
    };
    GluttonHitScratch* rangeScratch;
    GluttonWork*       work;
    Enemy*             enemy;
    Task*              player;
    GfxCoord*          hostCoord;
    SVECTOR            playerOffset;
    SVECTOR*           playerOffsetPointer;

    work   = task->work;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    enemy  = task->spawnArg2.pointer;

    if (work->stateChanged != 0) {
        work->neckYawEnabled   = 1;
        work->neckPitchEnabled = 1;
        work->neckPitchTarget  = 0;
        if (work->attackDelay == 0) {
            work->attackDelay = ACTOR_444000_ATTACK_DELAY_TICKS;
        }
        work->animId      = ACTOR_444000_IDLE_CLIP;
        work->animStep    = GLUTTON_ANIM_STEP_BLEND;
        work->hostExposed = 0;
    }

    playerOffsetPointer     = &playerOffset;
    hostCoord               = task->extra.tmd->coords;
    playerOffsetPointer->vx = gPlayerStatus.coordMtx->t[0] - hostCoord->coord.t[0];
    playerOffsetPointer->vy = gPlayerStatus.coordMtx->t[1] - hostCoord->coord.t[1];
    playerOffsetPointer->vz = gPlayerStatus.coordMtx->t[2] - hostCoord->coord.t[2];
    work->neckYawTarget     = _actorAngleTurnToOffset(task->extra.tmd->coords, playerOffsetPointer->vx, playerOffsetPointer->vz);

    if (_gGluttonLimbReach >= ACTOR_444000_LIMB_RETRACTION_MIN_REACH) {
        _gGluttonLimbReach = (u16)_gGluttonLimbReach - ACTOR_444000_LIMB_RETRACTION_STEP;
        work->limbPose     = 0;
    }
    _gluttonTickAnim(task);

    // Advance and healing triggers precede range-based attack selection.
    if (work->attackDelay > 0) {
        work->attackDelay = work->attackDelay - 1;
        return;
    }
    if (work->summonsAlive > 0) {
        work->state = GLUTTON_STATE_INHALE;
        return;
    }

    if (work->phase == 0) {
        work->state = GLUTTON_STATE_ADVANCE;
        return;
    }
    if (work->phase == 1 && player->extra.tmd->coords->coord.t[2] < ACTOR_444000_PHASE1_ADVANCE_Z) {
        work->state = GLUTTON_STATE_ADVANCE;
        return;
    }
    if (work->phase == 2) {
        work->state = GLUTTON_STATE_ADVANCE;
        return;
    }
    if (work->phase == 3 && player->extra.tmd->coords->coord.t[2] < ACTOR_444000_PHASE3_ADVANCE_Z) {
        work->state = GLUTTON_STATE_ADVANCE;
        return;
    }
    if (work->phase == 4 && player->extra.tmd->coords->coord.t[2] < ACTOR_444000_PHASE4_ADVANCE_Z &&
        enemy->hp < ACTOR_444000_PHASE4_ADVANCE_HP) {
        work->state = GLUTTON_STATE_ADVANCE;
        return;
    }
    if (work->phase == 5 && player->extra.tmd->coords->coord.t[2] < ACTOR_444000_PHASE5_ADVANCE_Z &&
        enemy->hp < ACTOR_444000_PHASE5_ADVANCE_HP) {
        work->state = GLUTTON_STATE_ADVANCE;
        return;
    }

    if ((s8)work->pendingHeals > 0) {
        work->state = GLUTTON_STATE_HEAL;
        return;
    }
    if (work->phase == 6) {
        work->state = ACTOR_444000_STATE_THROW_DEBRIS;
        return;
    }

    // Quantize the offset before measuring the three-dimensional attack range.
    rangeScratch              = SCRATCH_STACK_RESERVE_BLOCK(GluttonHitScratch);
    rangeScratch->toPlayer.vx = player->extra.tmd->coords->coord.t[0] -
                                task->extra.tmd->coords->coord.t[0] - ACTOR_444000_ATTACK_RANGE_ORIGIN_X;
    rangeScratch->toPlayer.vy = player->extra.tmd->coords->coord.t[1] -
                                task->extra.tmd->coords->coord.t[1] - ACTOR_444000_ATTACK_RANGE_ORIGIN_Y;
    rangeScratch->toPlayer.vz = player->extra.tmd->coords->coord.t[2] -
                                task->extra.tmd->coords->coord.t[2] + ACTOR_444000_ATTACK_RANGE_ORIGIN_NEGATIVE_Z;
    rangeScratch->playerDistance = SquareRoot0(rangeScratch->toPlayer.vx * rangeScratch->toPlayer.vx + rangeScratch->toPlayer.vy * rangeScratch->toPlayer.vy +
                                               rangeScratch->toPlayer.vz * rangeScratch->toPlayer.vz);
    if (rangeScratch->playerDistance < ACTOR_444000_FAR_ATTACK_DISTANCE) {
        if (rangeScratch->playerDistance >= ACTOR_444000_NEAR_ATTACK_DISTANCE) {
            RANDOM_LCG_ADVANCE();
            if (RANDOM_LCG_DRAW() & 1) {
                work->state = ACTOR_444000_STATE_THROW_DEBRIS;
            } else {
                work->state = GLUTTON_STATE_INHALE;
            }
        } else {
            RANDOM_LCG_ADVANCE();
            if (RANDOM_LCG_DRAW() & 1) {
                work->state = GLUTTON_STATE_INHALE;
            } else {
                work->state = GLUTTON_STATE_SWIPE;
            }
        }
    } else {
        work->state = GLUTTON_STATE_INHALE;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GluttonHitScratch);
}

/// Spawns and commands up to two Mad Chasers during the boss's summon sequence.
///
/// Requires live host/player models, initialized rigs, scratch, current-area
/// placement 2 and four readable Mad Chaser descriptors starting at
/// `gActor04400MadChaserTaskDescs`. Index 3 selects the hidden-model callback
/// with an automatic primitive buffer. Successful spawns borrow placement 2's texture offsets;
/// the encounter limits total spawns to eight and prohibits them at phase 6.
/// Ticks 70/120 command slot 0/1 to emerge from phase-selected room spots with
/// one of three random emergence moves. Player yaw uses 4096-unit angles.
/// Tick 331, or idle playback with no live summons, returns to inhale.
static void _actor444000SummonState(Task* task)
{
    enum {
        ACTOR_444000_LIMB_RETRACT_MIN_REACH   = 401,
        ACTOR_444000_LIMB_RETRACT_STEP        = 200,
        ACTOR_444000_FIRST_SUMMON_ORDER_TICK  = 70,
        ACTOR_444000_SECOND_SUMMON_ORDER_TICK = 120,
        ACTOR_444000_SUMMON_TEXTURE_PLACEMENT = 2,
        ACTOR_444000_SUMMON_TASK_INDEX        = ACTOR_04400_MAD_CHASER_TASK_SCRIPTED,
        ACTOR_444000_SUMMON_ARGUMENT          = 2,
        ACTOR_444000_SUMMON_CLIP              = 19,
        ACTOR_444000_SUMMON_IDLE_CLIP         = 1,
        ACTOR_444000_SUMMON_TOTAL_LIMIT       = 8,
        ACTOR_444000_SUMMON_END_TICK          = 331,
        ACTOR_444000_SUMMON_SPOT_SHIFT        = 8,
        ACTOR_444000_SUMMON_MOVE_STRIDE       = 16,
    };
    GluttonSummonScratch* summonScratch;
    GluttonWork*          work;
    Enemy*                enemy;
    Enemy*                summon;
    PlayerStatus*         playerStatus;
    GfxCoord*             rootCoord;
    TmdObject*            summonModel;
    AreaPlacement*        texturePlacement;
    GameLocationKey       location;
    GameLocationKey*      sessionLocation;
    s32                   cueId;
    s32                   cuePan;
    s32                   blastId;
    s32                   blastPan;
    s32                   nextRandomState;
    s32                   previousClip;
    u32                   poseCue;

    summonScratch = SCRATCH_STACK_RESERVE_BLOCK(GluttonSummonScratch);
    work          = task->work;
    enemy         = task->spawnArg2.pointer;
    if (work->stateChanged != 0) {
        work->neckPitchEnabled = 0;
        work->neckYawEnabled   = 1;
        previousClip           = work->animId;
        work->animRate         = ANIMATION_RATE_ONE;
        work->hostExposed      = 0;
        if (previousClip != ACTOR_444000_SUMMON_CLIP) {
            work->animId   = ACTOR_444000_SUMMON_CLIP;
            work->animStep = GLUTTON_ANIM_STEP_BLEND;
        } else {
            work->animStep = GLUTTON_ANIM_STEP_RESTART;
            work->animId   = previousClip;
        }
        for (summonScratch->slot = 0; summonScratch->slot < ARRAY_SIZE(work->summons); summonScratch->slot++) {
            if (work->summons[summonScratch->slot] == NULL && (u8)work->summonsSpawned < ACTOR_444000_SUMMON_TOTAL_LIMIT && work->phase < ACTOR_444000_PHASE_LIFT) {
                work->summons[summonScratch->slot] = enemySpawnFromTable(gActor04400MadChaserTaskDescs, ACTOR_444000_SUMMON_TASK_INDEX, ACTOR_444000_SUMMON_ARGUMENT, NULL);
                if (work->summons[summonScratch->slot] != NULL) {
                    work->summonsSpawned++;
                    summonModel        = work->summons[summonScratch->slot]->task->extra.tmd;
                    sessionLocation    = &gGameSession->location;
                    location.stage     = sessionLocation->stage;
                    location.area      = sessionLocation->area;
                    location.roomState = sessionLocation->roomState;
                    location.view      = sessionLocation->view;
                    areaSyncLocationVariant(&location);
                    texturePlacement               = &areaGetVariant(&location)->placements[ACTOR_444000_SUMMON_TEXTURE_PLACEMENT];
                    summonModel->texturePageOffset = texturePlacement->texturePageOffset;
                    summonModel->clutRowOffset     = texturePlacement->clutRowOffset;
                    if (summonModel->buffer != NULL) {
                        tmdBuildBufferHalf(summonModel);
                        tmdBuildBufferHalf(summonModel);
                    }
                    work->summons[summonScratch->slot]->workType = ENEMY_WORK_ACTOR;
                    summon                                       = work->summons[summonScratch->slot];
                    summon->placeKey                            |= summonScratch->slot << ENEMY_PLACE_INDEX_SHIFT;
                    work->summonsAlive++;
                }
            }
        }
        work->wallDistanceTarget = 0xC80;
        sndEvtRequestScriptStop((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0D), SOUND_SCRIPT_STOP_KEEP_RELEASE);
    }
    if (_gGluttonLimbReach >= ACTOR_444000_LIMB_RETRACT_MIN_REACH) {
        _gGluttonLimbReach = (u16)_gGluttonLimbReach - ACTOR_444000_LIMB_RETRACT_STEP;
    }
    _gluttonTickAnim(task);
    if (work->animId == ACTOR_444000_SUMMON_CLIP && (poseCue = work->hostRig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 4 && poseCue < 0xD) {
        work->hostExposed = 1;
    } else {
        work->hostExposed = 0;
    }
    playerStatus               = &gPlayerStatus;
    rootCoord                  = task->extra.tmd->coords;
    summonScratch->toPlayer.vx = playerStatus->coordMtx->t[0] - rootCoord->coord.t[0];
    summonScratch->toPlayer.vy = playerStatus->coordMtx->t[1] - rootCoord->coord.t[1];
    summonScratch->toPlayer.vz = playerStatus->coordMtx->t[2] - rootCoord->coord.t[2];
    work->neckYawTarget        = _actorAngleTurnToOffset(task->extra.tmd->coords, summonScratch->toPlayer.vx, summonScratch->toPlayer.vz);
    if ((work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) && work->animId == ACTOR_444000_SUMMON_CLIP) {
        work->animId   = ACTOR_444000_SUMMON_IDLE_CLIP;
        work->animStep = GLUTTON_ANIM_STEP_BLEND;
        work->animRate = ANIMATION_RATE_ONE;
        _gluttonTickAnim(task);
    }
    if (work->stateTicks >= ACTOR_444000_SUMMON_END_TICK || (work->animId == ACTOR_444000_SUMMON_IDLE_CLIP && work->summonsAlive == 0)) {
        work->state = GLUTTON_STATE_INHALE;
    }
    if (work->stateTicks == 6) {
        cueId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x04);
        cuePan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(cueId, cuePan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    if (work->stateTicks == 0x3B) {
        blastId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x10);
        blastPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(blastId, blastPan, (s8)(worldCoordGetOriginAudioDepth(task->extra.tmd->coords) / 2));
        work->shakeLevel = GLUTTON_SHAKE_LONG;
        padScriptSpawn(gActor444000GluttonImpactPadCommands, gActor444000GluttonImpactPadSegments);
    }
    if (work->stateTicks == ACTOR_444000_FIRST_SUMMON_ORDER_TICK || work->stateTicks == ACTOR_444000_SECOND_SUMMON_ORDER_TICK) {
        if (work->stateTicks == ACTOR_444000_FIRST_SUMMON_ORDER_TICK) {
            summonScratch->slot = 0;
        } else {
            summonScratch->slot = 1;
        }
        if (work->summons[summonScratch->slot] != NULL && work->phase < ACTOR_444000_PHASE_LIFT) {
            _gActor444000GluttonMadChaserCommand.command.context.loc.stage = ACTOR_444000_MAD_CHASER_CONTEXT_STAGE;
            _gActor444000GluttonMadChaserCommand.command.context.loc.area  = ACTOR_444000_MAD_CHASER_CONTEXT_AREA;
            switch (work->phase) {
                case 0:
                case 1:
                    if (summonScratch->slot == 0) {
                        RANDOM_LCG_ADVANCE();
                        if (!(RANDOM_LCG_DRAW() & 1)) {
                            _gActor444000GluttonMadChaserCommand.command.command = 3;
                        } else {
                            _gActor444000GluttonMadChaserCommand.command.command = 4;
                        }
                    } else {
                        RANDOM_LCG_ADVANCE();
                        if (!(RANDOM_LCG_DRAW() & 1)) {
                            _gActor444000GluttonMadChaserCommand.command.command = 5;
                        } else {
                            _gActor444000GluttonMadChaserCommand.command.command = 6;
                        }
                    }
                    break;
                case 2:
                case 3:
                    if (summonScratch->slot == 0) {
                        RANDOM_LCG_ADVANCE();
                        if (!(RANDOM_LCG_DRAW() & 1)) {
                            _gActor444000GluttonMadChaserCommand.command.command = 0xd;
                        } else {
                            _gActor444000GluttonMadChaserCommand.command.command = 8;
                        }
                    } else {
                        RANDOM_LCG_ADVANCE();
                        if (!(RANDOM_LCG_DRAW() & 1)) {
                            _gActor444000GluttonMadChaserCommand.command.command = 7;
                        } else {
                            _gActor444000GluttonMadChaserCommand.command.command = 0xe;
                        }
                    }
                    break;
                case 4:
                case 5:
                    if (summonScratch->slot == 0) {
                        RANDOM_LCG_ADVANCE();
                        if ((RANDOM_LCG_DRAW() & 1)) {
                            _gActor444000GluttonMadChaserCommand.command.command = 9;
                        } else {
                            _gActor444000GluttonMadChaserCommand.command.command = 0xf;
                        }
                    } else {
                        RANDOM_LCG_ADVANCE();
                        if (!(RANDOM_LCG_DRAW() & 1)) {
                            _gActor444000GluttonMadChaserCommand.command.command = 9;
                        } else {
                            _gActor444000GluttonMadChaserCommand.command.command = 0xf;
                        }
                    }
                    break;
            }
            // Bits 8..11 choose the room spot; bits 4..7 choose one of three emergence moves.
            _gActor444000GluttonMadChaserCommand.command.command <<= ACTOR_444000_SUMMON_SPOT_SHIFT;
            nextRandomState                                        = RANDOM_LCG_NEXT(gRandomLcgState);
            _gActor444000GluttonMadChaserCommand.command.command  |= (s16)(((((u32)nextRandomState >> 16) % 3) * ACTOR_444000_SUMMON_MOVE_STRIDE) | MAD_CHASER_COMMAND_EMERGE);
            gRandomLcgState                                        = nextRandomState;
            TASK_MESSAGE_DISPATCH_POINTER(work->summons[summonScratch->slot]->task, ACTOR_COMMAND_MESSAGE_APPLY, &_gActor444000GluttonMadChaserCommand.command, 0);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(GluttonSummonScratch);
}

#include "../../shared/creatures/glutton/glutton_escort_state.inc.c"

/// Clamps the live player root to the incinerator arena's depth-banded corridor.
///
/// Coordinates use root-parent world units. Positive Y is clamped to zero;
/// Z is bounded by 0 and -26000, and interior depth bands select X limits.
/// The end-cap branches only clamp Z, so X is not clamped on those calls.
/// Requires a live player model. Writes translation without dirtying composition.
static void _actor444000ClampPlayerToArena(void)
{
    enum {
        ACTOR_444000_ARENA_FRONT_BAND_BACK_Z       = -1000,
        ACTOR_444000_ARENA_WIDE_APPROACH_BACK_Z    = -5000,
        ACTOR_444000_ARENA_TURN_BACK_Z             = -7000,
        ACTOR_444000_ARENA_FIRST_WIDENING_FRONT_Z  = -13200,
        ACTOR_444000_ARENA_FIRST_WIDENING_BACK_Z   = -14750,
        ACTOR_444000_ARENA_SECOND_WIDENING_FRONT_Z = -21250,
        ACTOR_444000_ARENA_SECOND_WIDENING_BACK_Z  = -22800,
        ACTOR_444000_ARENA_BACK_Z                  = -26000,
        ACTOR_444000_ARENA_CORRIDOR_RIGHT_X        = 16500,
        ACTOR_444000_ARENA_WIDENING_RIGHT_X        = 17500,
        ACTOR_444000_ARENA_CORRIDOR_LEFT_X         = 11500,
        ACTOR_444000_ARENA_TURN_LEFT_X             = 9500,
    };
    Task* player;
    s32   playerZ;

/// Clamps the player's root X translation to an inclusive arena interval.
///
/// Requires a live player model and ordered signed bounds in root-parent units.
/// Arguments must have no side effects: playerTask occurs up to four times and
/// each bound once, or twice when its clamp fires. Retains Y/Z and composition
/// stamps. Captures no identifiers; forms one compound statement.
#define ACTOR_444000_CLAMP_PLAYER_X(playerTask, minimumX, maximumX)     \
    {                                                                   \
        if ((playerTask)->extra.tmd->coords->coord.t[0] < (minimumX)) { \
            (playerTask)->extra.tmd->coords->coord.t[0] = (minimumX);   \
        }                                                               \
        if ((playerTask)->extra.tmd->coords->coord.t[0] > (maximumX)) { \
            (playerTask)->extra.tmd->coords->coord.t[0] = (maximumX);   \
        }                                                               \
    }

    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);

    if (player->extra.tmd->coords->coord.t[1] > 0) {
        player->extra.tmd->coords->coord.t[1] = 0;
    }

    playerZ = player->extra.tmd->coords->coord.t[2];
    if (playerZ > 0) {
        player->extra.tmd->coords->coord.t[2] = 0;
    } else if (playerZ > ACTOR_444000_ARENA_FRONT_BAND_BACK_Z) {
        ACTOR_444000_CLAMP_PLAYER_X(player, 0, ACTOR_444000_ARENA_CORRIDOR_RIGHT_X);
    } else if (playerZ > ACTOR_444000_ARENA_WIDE_APPROACH_BACK_Z) {
        ACTOR_444000_CLAMP_PLAYER_X(player, 0, ACTOR_444000_ARENA_CORRIDOR_RIGHT_X);
    } else if (playerZ > ACTOR_444000_ARENA_TURN_BACK_Z) {
        ACTOR_444000_CLAMP_PLAYER_X(player, ACTOR_444000_ARENA_TURN_LEFT_X, ACTOR_444000_ARENA_CORRIDOR_RIGHT_X);
    } else if (playerZ > ACTOR_444000_ARENA_FIRST_WIDENING_FRONT_Z) {
        ACTOR_444000_CLAMP_PLAYER_X(player, ACTOR_444000_ARENA_CORRIDOR_LEFT_X, ACTOR_444000_ARENA_CORRIDOR_RIGHT_X);
    } else if (playerZ > ACTOR_444000_ARENA_FIRST_WIDENING_BACK_Z) {
        ACTOR_444000_CLAMP_PLAYER_X(player, ACTOR_444000_ARENA_CORRIDOR_LEFT_X, ACTOR_444000_ARENA_WIDENING_RIGHT_X);
    } else if (playerZ > ACTOR_444000_ARENA_SECOND_WIDENING_FRONT_Z) {
        ACTOR_444000_CLAMP_PLAYER_X(player, ACTOR_444000_ARENA_CORRIDOR_LEFT_X, ACTOR_444000_ARENA_CORRIDOR_RIGHT_X);
    } else if (playerZ > ACTOR_444000_ARENA_SECOND_WIDENING_BACK_Z) {
        ACTOR_444000_CLAMP_PLAYER_X(player, ACTOR_444000_ARENA_CORRIDOR_LEFT_X, ACTOR_444000_ARENA_WIDENING_RIGHT_X);
    } else if (playerZ > ACTOR_444000_ARENA_BACK_Z) {
        ACTOR_444000_CLAMP_PLAYER_X(player, ACTOR_444000_ARENA_CORRIDOR_LEFT_X, ACTOR_444000_ARENA_CORRIDOR_RIGHT_X);
    } else {
        player->extra.tmd->coords->coord.t[2] = ACTOR_444000_ARENA_BACK_Z;
    }
}
#undef ACTOR_444000_CLAMP_PLAYER_X

/// Updates the incinerator boss's combat state, model visibility and collision publication.
///
/// Requires task-owned GluttonWork, live host/player/escort models and initialized
/// rigs. state must select a non-NULL entry in the 21-slot local table; holes
/// 2, 4, 6 and 20 are not dispatched. Paused/hidden actor control clears contacts
/// and returns after visibility and lighting updates. Running updates process
/// hit cooldowns, death notifications, state entry/timing and the selected handler.
/// Lock-on and collision flags follow the resulting state and exposed body;
/// contacts are consumed each update. GPU use must finish before buffer release.
static void _actor444000Tick(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_444000_HIDDEN_VIEW_INDEX  = 9,
        ACTOR_444000_DEATH_TICK_LIMIT   = 256,
        ACTOR_444000_TICK_SCRATCH_BYTES = 28,
        ACTOR_444000_STATE_TICK_LIMIT   = 0x7FFF,
    };
    PlayerStatus* playerStatus = &gPlayerStatus;
    GluttonWork*  work         = task->work;
    VECTOR        lightingPosition;
    TaskFunc      stateHandlers[] = {
        _actor444000DormantState,
        _actor444000IdleState,
        NULL,
        _actor444000InhaleState,
        NULL,
        _actor444000LimbAnimationState,
        NULL,
        _actor444000ThrowDebrisState,
        _actor444000RetractLimbState,
        _actor444000RunArenaRouteState,
        _actor444000ChooseAttackState,
        _actor444000SwipeState,
        _actor444000ResumeLimbAnimationState,
        _actor444000CatchPlayerState,
        _actor444000SummonState,
        _gluttonHealState,
        _actor444000LiftIdleState,
        _actor444000ReturnToAdvanceState,
        _actor444000CollapseState,
        _actor444000CollapsedState,
        NULL,
    };
    GluttonWork* bufferWork;
    GluttonWork* flagWork;
    s32          viewIndex;
    s16          bufferIndex;

    viewIndex                               = viewGetMappedIndex() & 0xFF;
    task->extra.tmd->coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&task->extra.tmd->coords[0]);

    bufferWork = task->work;
    if (bufferWork->freeCountdown != 0) {
        task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        if (--bufferWork->freeCountdown == 0) {
            task->extra.tmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            tmdFreePrimitiveBuffer(task->extra.tmd);
            for (bufferIndex = 0; bufferIndex < ARRAY_SIZE(bufferWork->escorts); bufferIndex++) {
                if (bufferWork->escorts[bufferIndex] != NULL) {
                    bufferWork->escorts[bufferIndex]->task->extra.tmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
                    tmdFreePrimitiveBuffer(bufferWork->escorts[bufferIndex]->task->extra.tmd);
                }
            }
        }
    }

    _actor444000ClampPlayerToArena();

    lightingPosition.vx = task->extra.tmd->coords[3].workm.t[0];
    lightingPosition.vy = task->extra.tmd->coords[3].workm.t[1];
    lightingPosition.vz = task->extra.tmd->coords[3].workm.t[2];

    if (work->hostExposed != work->prevHostExposed) {
        worldCoordUpdateActorColor(enemy, &lightingPosition, 0, 0);
        worldCoordUpdateActorColor(work->escorts[3], &lightingPosition, 0, 0);
        work->prevHostExposed = work->hostExposed;
    }
    worldCoordUpdateActorColor(work->hostExposed != 0 ? enemy : work->escorts[3], &lightingPosition, 0, 0);

    if (work->state == GLUTTON_STATE_SWIPE) {
        work->escorts[4]->task->extra.tmd->otOffset = -1;
    } else {
        work->escorts[4]->task->extra.tmd->otOffset = 0;
    }

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->state != ACTOR_444000_STATE_DORMANT) {
                if (viewIndex == ACTOR_444000_HIDDEN_VIEW_INDEX) {
                    flagWork                = task->work;
                    flagWork->freeCountdown = 0;
                    task->extra.tmd->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    _actor444000CopyEscortModelFlags(task, flagWork);
                } else {
                    flagWork                = task->work;
                    flagWork->freeCountdown = 0;
                    task->extra.tmd->flags  = 0;
                    _actor444000CopyEscortModelFlags(task, flagWork);
                }
            }
            break;

        case SCENE_COMBAT_ACTORS_PAUSED:
            if (work->state != ACTOR_444000_STATE_DORMANT) {
                if (viewIndex == ACTOR_444000_HIDDEN_VIEW_INDEX) {
                    flagWork                = task->work;
                    flagWork->freeCountdown = 0;
                    task->extra.tmd->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    _actor444000CopyEscortModelFlags(task, flagWork);
                } else {
                    flagWork                = task->work;
                    flagWork->freeCountdown = 0;
                    task->extra.tmd->flags  = 0;
                    _actor444000CopyEscortModelFlags(task, flagWork);
                }
            }
            worldCollisionClearContacts(work->hits[0].contacts);
            worldCollisionClearContacts(work->hits[1].contacts);
            worldCollisionClearContacts(work->hits[2].contacts);
            worldCollisionClearContacts(work->hits[3].contacts);
            worldCollisionClearContacts(work->hits[4].contacts);
            worldCollisionClearContacts(work->hits[5].contacts);
            worldCollisionClearContacts(work->hits[6].contacts);
            worldCollisionClearContacts(work->hits[7].contacts);
            worldCollisionClearContacts(work->hits[8].contacts);
            worldCollisionClearContacts(work->swipeContacts);
            return;

        case SCENE_COMBAT_ACTORS_HIDDEN:
            flagWork                = task->work;
            flagWork->freeCountdown = 0;
            task->extra.tmd->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            _actor444000CopyEscortModelFlags(task, flagWork);
            worldCollisionClearContacts(work->hits[0].contacts);
            worldCollisionClearContacts(work->hits[1].contacts);
            worldCollisionClearContacts(work->hits[2].contacts);
            worldCollisionClearContacts(work->hits[3].contacts);
            worldCollisionClearContacts(work->hits[4].contacts);
            worldCollisionClearContacts(work->hits[5].contacts);
            worldCollisionClearContacts(work->hits[6].contacts);
            worldCollisionClearContacts(work->hits[7].contacts);
            worldCollisionClearContacts(work->hits[8].contacts);
            worldCollisionClearContacts(work->swipeContacts);
            return;
    }

    // This outer reservation is unused by the recovered body but encloses the handler calls.
    SCRATCH_STACK_RESERVE_BYTES(ACTOR_444000_TICK_SCRATCH_BYTES);

    // Contact groups have independent cooldowns; player holds suppress all incoming attacks.
    if (enemy->hp > 0) {
        if (work->playerCaught != 1 && playerStatus->hp > 0 && work->state != ACTOR_444000_STATE_CATCH_PLAYER) {
            if (work->groups1To2Cooldown > 0) {
                work->groups1To2Cooldown--;
            } else {
                _gluttonHitGroups1To2(task);
            }
            if (work->group0Cooldown > 0) {
                work->group0Cooldown--;
            } else {
                _gluttonHitGroup0(task);
            }
            if (work->groups3To5Cooldown > 0) {
                work->groups3To5Cooldown--;
            } else {
                _actor444000HitGroups3To5(task);
            }
            if (work->groups6To8Cooldown > 0) {
                work->groups6To8Cooldown--;
            } else {
                _gluttonHitGroups6To8(task);
            }
        }
    }
    // Keep the boss alive on simultaneous player death; otherwise notify the room before collapse.
    if (enemy->hp <= 0) {
        if (playerStatus->hp <= 0) {
            enemy->hp               = 1;
            _gGluttonEncounterEnded = GLUTTON_ENCOUNTER_RUNNING;
        }
        if (enemy->hp <= 0 && work->state != ACTOR_444000_STATE_DORMANT) {
            switch (work->deathTicks) {
                case 0:
                    _gGluttonEncounterEnded                                        = GLUTTON_ENCOUNTER_ENDED;
                    _gActor444000GluttonMadChaserCommand.command.context.loc.stage = ACTOR_444000_MAD_CHASER_CONTEXT_STAGE;
                    _gActor444000GluttonMadChaserCommand.command.context.loc.area  = ACTOR_444000_MAD_CHASER_CONTEXT_AREA;
                    _gActor444000GluttonMadChaserCommand.command.command           = MAD_CHASER_COMMAND_VANISH;
                    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &_gActor444000GluttonMadChaserCommand.command, ACTOR_COMMAND_MESSAGE_APPLY);
                    break;

                case 3:
                    if (playerStatus->hp > 0) {
                        if (work->phase == ACTOR_444000_PHASE_LIFT) {
                            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, ACTOR_444000_ROOM_EVENT_BOSS_DIED_ON_LIFT, 0);
                        } else {
                            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, ACTOR_444000_ROOM_EVENT_BOSS_DIED_ON_APPROACH, 0);
                        }
                        work->state = ACTOR_444000_STATE_COLLAPSE;
                        sndEvtRequestScriptStop(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0A), SOUND_SCRIPT_STOP_KEEP_RELEASE);
                        sndEvtRequestScriptStop(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0D), SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    }
                    break;
            }
            if (work->deathTicks < ACTOR_444000_DEATH_TICK_LIMIT) {
                work->deathTicks++;
            }
        }
    }

    // Publish entry once, then cap the signed per-state tick counter before dispatch.
    if (work->prevState != work->state) {
        work->stateChanged = 1;
        work->stateTicks   = 0;
    } else {
        if (work->stateTicks < ACTOR_444000_STATE_TICK_LIMIT) {
            work->stateTicks++;
        }
        work->stateChanged = 0;
    }
    work->prevState = work->state;
    stateHandlers[work->state](task);

    // Lock-on and pair collision follow the body exposed by the state just dispatched.
    if ((u16)work->state < 2 || work->state == ACTOR_444000_STATE_LIMB_ANIMATION || work->state == ACTOR_444000_STATE_COLLAPSE ||
        work->state == ACTOR_444000_STATE_COLLAPSED || work->state == ACTOR_444000_STATE_RESUME_LIMB_ANIMATION) {
        enemy->node.state.parts.flags            = WORLD_TARGET_NOT_LOCKABLE;
        work->escorts[3]->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->escorts[0]->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->escorts[1]->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    } else if (work->hostExposed != 0) {
        if (worldTargetGetActorLockMask(&work->escorts[3]->node) != 0) {
            worldTargetSetPlayerLock(&enemy->node);
        }
        enemy->node.state.parts.flags            = WORLD_TARGET_HIDE_HP;
        work->escorts[3]->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
        work->escorts[0]->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
        work->escorts[1]->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
    } else {
        if (worldTargetGetActorLockMask(&enemy->node) != 0) {
            worldTargetSetPlayerLock(&work->escorts[3]->node);
        }
        enemy->node.state.parts.flags            = WORLD_TARGET_NOT_LOCKABLE;
        work->escorts[3]->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->escorts[0]->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->escorts[1]->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
    }

    if (work->state != ACTOR_444000_STATE_DORMANT && work->state != ACTOR_444000_STATE_COLLAPSE && work->state != ACTOR_444000_STATE_COLLAPSED &&
        work->state != ACTOR_444000_STATE_LIMB_ANIMATION && work->state != ACTOR_444000_STATE_RESUME_LIMB_ANIMATION && work->hostExposed == 1) {
        work->hits[0].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->hits[0].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }

    if (work->state != ACTOR_444000_STATE_DORMANT && work->state != ACTOR_444000_STATE_COLLAPSE && work->state != ACTOR_444000_STATE_COLLAPSED &&
        work->state != ACTOR_444000_STATE_LIMB_ANIMATION && work->state != ACTOR_444000_STATE_RESUME_LIMB_ANIMATION && work->hostExposed != 1) {
        work->hits[1].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->hits[2].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->hits[1].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->hits[2].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }

    if (work->state != ACTOR_444000_STATE_DORMANT && work->state != ACTOR_444000_STATE_LIMB_ANIMATION && work->state != ACTOR_444000_STATE_RESUME_LIMB_ANIMATION &&
        work->state != ACTOR_444000_STATE_COLLAPSED && work->state != ACTOR_444000_STATE_COLLAPSE) {
        work->hits[3].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->hits[4].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->hits[5].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->hits[3].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->hits[4].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->hits[5].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }

    if (work->state != ACTOR_444000_STATE_DORMANT && work->state != ACTOR_444000_STATE_LIMB_ANIMATION && work->state != ACTOR_444000_STATE_RESUME_LIMB_ANIMATION &&
        work->state != ACTOR_444000_STATE_COLLAPSED && work->state != ACTOR_444000_STATE_COLLAPSE) {
        work->hits[6].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->hits[7].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->hits[8].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->hits[6].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->hits[7].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->hits[8].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }

    worldCollisionClearContacts(work->hits[0].contacts);
    worldCollisionClearContacts(work->hits[1].contacts);
    worldCollisionClearContacts(work->hits[2].contacts);
    worldCollisionClearContacts(work->hits[3].contacts);
    worldCollisionClearContacts(work->hits[4].contacts);
    worldCollisionClearContacts(work->hits[5].contacts);
    worldCollisionClearContacts(work->hits[6].contacts);
    worldCollisionClearContacts(work->hits[7].contacts);
    worldCollisionClearContacts(work->hits[8].contacts);
    worldCollisionClearContacts(work->swipeContacts);

    if (work->state != ACTOR_444000_STATE_LIMB_ANIMATION) {
        _gluttonShakeTick(task);
    }

    SCRATCH_STACK_RELEASE_BYTES(ACTOR_444000_TICK_SCRATCH_BYTES);
}

/// Restores the elevated Y endpoints of the boss's two collision-wall quads.
///
/// Requires the writable incinerator grid with vertices 24..31. For each quad,
/// the first two endpoints become Y=500 and the last two Y=800, in grid units.
/// X/Z, normals, face descriptors and cell lists are left intact.
static inline void _actor444000ResetWallHeights(void)
{
    enum {
        ACTOR_444000_RESET_WALL_TOP_Y    = 500,
        ACTOR_444000_RESET_WALL_BOTTOM_Y = 800,
    };
    SVECTOR* vertices;

    vertices        = gWorldCollisionActiveGrid->vertices;
    vertices[24].vy = ACTOR_444000_RESET_WALL_TOP_Y;
    vertices[25].vy = ACTOR_444000_RESET_WALL_TOP_Y;
    vertices[26].vy = ACTOR_444000_RESET_WALL_BOTTOM_Y;
    vertices[27].vy = ACTOR_444000_RESET_WALL_BOTTOM_Y;
    vertices[28].vy = ACTOR_444000_RESET_WALL_TOP_Y;
    vertices[29].vy = ACTOR_444000_RESET_WALL_TOP_Y;
    vertices[30].vy = ACTOR_444000_RESET_WALL_BOTTOM_Y;
    vertices[31].vy = ACTOR_444000_RESET_WALL_BOTTOM_Y;
}

/// Maintains Glutton's moving collision walls and player bounds before task dispatch.
///
/// Requires the live enemy, player and writable incinerator collision grid;
/// task states 0..2 select initialization, frame update or destruction.
/// Existing work chooses a wall lead in game-coordinate units and approaches
/// it by 50 per call. Active fight phases rebuild wall X/Z and constrain the
/// player; dormant state restores only vertices 24..31's wall endpoint heights.
/// Phase 2 retains the original X-tested assignment to the player's Z.
static void _actor444000GluttonTask(Task* task)
{
    enum {
        ACTOR_444000_WALL_INHALE_DISTANCE  = 3000,
        ACTOR_444000_WALL_CLOSE_DISTANCE   = 3400,
        ACTOR_444000_WALL_FAR_DISTANCE     = 5000,
        ACTOR_444000_WALL_DROP             = 400,
        ACTOR_444000_WALL_DISTANCE_STEP    = 50,
        ACTOR_444000_WALL_SNAP_DISTANCE    = 51,
        ACTOR_444000_CORNER_MIN_PLAYER_X   = 11500,
        ACTOR_444000_CORNER_PLAYER_Z_LEAD  = 2000,
        ACTOR_444000_PHASE2_PLAYER_Z_LIMIT = 0x251C,
        ACTOR_444000_DYNAMIC_WALL_FACE     = 6,
        ACTOR_444000_WALL_REFERENCE_PART   = 4,
    };

    EnemyTaskFunc handlers[3] = {
        _actor444000Spawn,
        _actor444000Tick,
        enemyDestroy,
    };
    SVECTOR      bossPartPosition;
    GluttonWork* work;
    Enemy*       enemy;
    Task*        player;
    s32          distanceError;
    s16          fightState;

    enemy  = task->spawnArg2.pointer;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    work   = task->work;
    if (work != NULL) {
        if (work->summons[0] != NULL && work->summons[0]->hp <= 0) {
            work->summons[0] = NULL;
        }
        if (work->summons[1] != NULL && work->summons[1]->hp <= 0) {
            work->summons[1] = NULL;
        }

        fightState = work->state;
        if (fightState == GLUTTON_STATE_INHALE) {
            work->wallDistanceTarget = ACTOR_444000_WALL_INHALE_DISTANCE;
            work->wallDrop           = ACTOR_444000_WALL_DROP;
        } else if (fightState == GLUTTON_STATE_ADVANCE) {
            work->wallDistanceTarget = ACTOR_444000_WALL_FAR_DISTANCE;
            work->wallDrop           = ACTOR_444000_WALL_DROP;
        } else if (fightState == ACTOR_444000_STATE_RETURN_TO_ADVANCE) {
            work->wallDistanceTarget = ACTOR_444000_WALL_FAR_DISTANCE;
            work->wallDrop           = ACTOR_444000_WALL_DROP;
        } else if (work->phase != 0) {
            work->wallDistanceTarget = ACTOR_444000_WALL_CLOSE_DISTANCE;
            work->wallDrop           = ACTOR_444000_WALL_DROP;
        } else {
            work->wallDistanceTarget = ACTOR_444000_WALL_FAR_DISTANCE;
            work->wallDrop           = ACTOR_444000_WALL_DROP;
        }

        distanceError = work->wallDistanceTarget - work->wallDistance;
        if (distanceError < 0) {
            distanceError = -distanceError;
        }
        if (distanceError >= ACTOR_444000_WALL_SNAP_DISTANCE) {
            if (work->wallDistance < work->wallDistanceTarget) {
                work->wallDistance = (u16)work->wallDistance + ACTOR_444000_WALL_DISTANCE_STEP;
            } else {
                work->wallDistance = (u16)work->wallDistance - ACTOR_444000_WALL_DISTANCE_STEP;
            }
        } else {
            work->wallDistance = (u16)work->wallDistanceTarget;
        }

        fightState = work->state;
        if (fightState != ACTOR_444000_STATE_DORMANT) {
            // Keep collapse and inactive phases outside the wall rebuild.
            if (fightState != ACTOR_444000_STATE_COLLAPSE) {
                if (fightState != ACTOR_444000_STATE_COLLAPSED && fightState != ACTOR_444000_STATE_LIMB_ANIMATION && fightState != ACTOR_444000_STATE_RESUME_LIMB_ANIMATION) {
                    if (work->phase == 1 && fightState == GLUTTON_STATE_ADVANCE) {
                        _actor444000BuildCornerWalls(task, work->wallDistance, ACTOR_444000_DYNAMIC_WALL_FACE);
                        {
                            GfxCoord* playerCoord = player->extra.tmd->coords;

                            if (playerCoord->coord.t[0] < ACTOR_444000_CORNER_MIN_PLAYER_X) {
                                playerCoord->coord.t[0] = ACTOR_444000_CORNER_MIN_PLAYER_X;
                            }
                        }
                        bossPartPosition.vx = bossPartPosition.vy = bossPartPosition.vz = 0;
                        _actorRenderTransformLocalPointToWorld(task->extra.tmd->coords + ACTOR_444000_WALL_REFERENCE_PART, &bossPartPosition);
                        {
                            GfxCoord* playerCoord    = player->extra.tmd->coords;
                            s32       maximumPlayerZ = bossPartPosition.vz - ACTOR_444000_CORNER_PLAYER_Z_LEAD;

                            if (maximumPlayerZ < playerCoord->coord.t[2]) {
                                playerCoord->coord.t[2] = maximumPlayerZ;
                            }
                        }
                    } else {
                        _gluttonBuildWall(task, work->wallDistance, work->wallDrop, ACTOR_444000_DYNAMIC_WALL_FACE);
                    }

                    if (work->phase == 0 || (work->phase == 1 && work->state != GLUTTON_STATE_ADVANCE)) {
                        GfxCoord* playerCoord    = player->extra.tmd->coords;
                        GfxCoord* selfCoord      = task->extra.tmd->coords;
                        s32       minimumPlayerX = work->wallDistance + selfCoord->coord.t[0];

                        if (playerCoord->coord.t[0] < minimumPlayerX) {
                            playerCoord->coord.t[0] = minimumPlayerX;
                        }
                    } else {
                        GfxCoord* playerCoord    = player->extra.tmd->coords;
                        GfxCoord* selfCoord      = task->extra.tmd->coords;
                        s32       maximumPlayerZ = selfCoord->coord.t[2] - work->wallDistance;

                        if (maximumPlayerZ < playerCoord->coord.t[2]) {
                            playerCoord->coord.t[2] = maximumPlayerZ;
                        }
                    }

                    if (work->phase == 2) {
                        GfxCoord* playerCoord = player->extra.tmd->coords;

                        if (playerCoord->coord.t[0] < ACTOR_444000_PHASE2_PLAYER_Z_LIMIT) {
                            playerCoord->coord.t[2] = ACTOR_444000_PHASE2_PLAYER_Z_LIMIT;
                        }
                    }
                }
            }
            // Wall rebuilding can return the fight to its dormant state.
            if (work->state == ACTOR_444000_STATE_DORMANT) {
                _actor444000ResetWallHeights();
            }
        } else {
            _actor444000ResetWallHeights();
        }

        fightState = work->state;
        if (fightState == ACTOR_444000_STATE_LIMB_ANIMATION) {
            _gluttonBuildWall(task, work->wallDistance, work->wallDrop, ACTOR_444000_DYNAMIC_WALL_FACE);
        }
    }

    handlers[task->state](enemy, task);
}

#include "../../shared/creatures/glutton/glutton_quad_heights.inc.c"

/// Idles on the moving lift while alternating the neck between two yaw targets.
///
/// Requires live host work, escorts 0/1, initialized rigs and the writable room
/// grid. Entry blends to clip 1, enables neck overrides, clears pending heals,
/// biases those escorts two OT buckets and installs the low lift walls.
/// After animation updates, every 60 state ticks selects +690 or -418 yaw
/// units on a 120-tick cycle; angles use 4096 units per turn.
static void _actor444000LiftIdleState(Task* task)
{
    enum {
        ACTOR_444000_LIFT_IDLE_CLIP           = 1,
        ACTOR_444000_LIFT_ESCORT_OT_OFFSET    = 2,
        ACTOR_444000_LIFT_LOOK_INTERVAL_TICKS = 60,
        ACTOR_444000_LIFT_LOOK_PERIOD_TICKS   = 120,
        ACTOR_444000_LIFT_LOOK_YAW_FIRST      = 690,
        ACTOR_444000_LIFT_LOOK_YAW_SECOND     = -418,
    };
    GluttonWork* work;
    s16          stateTicks;

    work = task->work;
    if (work->stateChanged != 0) {
        work->neckPitchEnabled                      = 1;
        work->neckYawEnabled                        = 1;
        work->hostExposed                           = 0;
        work->animId                                = ACTOR_444000_LIFT_IDLE_CLIP;
        work->animStep                              = GLUTTON_ANIM_STEP_BLEND;
        work->neckPitchTarget                       = 0;
        work->pendingHeals                          = 0;
        work->escorts[0]->task->extra.tmd->otOffset = ACTOR_444000_LIFT_ESCORT_OT_OFFSET;
        work->escorts[1]->task->extra.tmd->otOffset = ACTOR_444000_LIFT_ESCORT_OT_OFFSET;
        shelterB3GarbageIncineratorSetLiftCollisionWalls();
    }
    _gluttonTickAnim(task);
    stateTicks = work->stateTicks;
    if (stateTicks % ACTOR_444000_LIFT_LOOK_INTERVAL_TICKS == 0) {
        if (stateTicks % ACTOR_444000_LIFT_LOOK_PERIOD_TICKS == 0) {
            work->neckYawTarget = ACTOR_444000_LIFT_LOOK_YAW_FIRST;
        } else {
            work->neckYawTarget = ACTOR_444000_LIFT_LOOK_YAW_SECOND;
        }
    }
}

/// Plays the repositioning clip before returning the boss to its advance state.
///
/// Requires live host enemy/model/work and initialized animation rigs. Entry
/// restarts clip 12 at normal rate, disables neck overrides and exposes the
/// escort body. Tick ten starts a spatial Glutton cue. Host slot 1 reaching
/// an animation boundary selects advance; sound pan/depth narrow to s8.
static void _actor444000ReturnToAdvanceState(Task* task)
{
    enum {
        ACTOR_444000_RETURN_TO_ADVANCE_CLIP = 12,
        ACTOR_444000_RETURN_SOUND_TICK      = 10,
    };
    GluttonWork* work;
    Enemy*       enemy;
    TmdObject*   hostModel;
    s32          soundId;
    s32          soundPan;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateChanged != 0) {
        hostModel                     = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        hostModel->flags              = 0;
        work->animId                  = ACTOR_444000_RETURN_TO_ADVANCE_CLIP;
        work->animStep                = GLUTTON_ANIM_STEP_RESTART;
        work->neckPitchEnabled        = 0;
        work->neckYawEnabled          = 0;
        work->hostExposed             = 0;
        work->animRate                = ANIMATION_RATE_ONE;
        work->neckPitchTarget         = 0;
        work->stateTicks              = 0;
    }
    if (work->stateTicks == ACTOR_444000_RETURN_SOUND_TICK) {
        soundId  = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x17);
        soundPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    _gluttonTickAnim(task);
    if (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = GLUTTON_STATE_ADVANCE;
    }
}

#include "../../shared/creatures/glutton/glutton_prop.inc.c"

/// Selects the escort-model task callback instantiated by the shared fragment.
///
/// Must name a previously declared `void (Task*)` callback; its declaration
/// supplies the linkage. Bind around each inclusion, then undefine it. The
/// first private instance serves descriptor slots 0..5; the second serves slot 6.
#define GLUTTON_PROP_TASK _gluttonPropTask
#include "../../shared/creatures/glutton/glutton_prop_task.inc.c"
#undef GLUTTON_PROP_TASK

#define GLUTTON_PROP_TASK _gluttonEscort6Task
#include "../../shared/creatures/glutton/glutton_prop_task.inc.c"
#undef GLUTTON_PROP_TASK

/// Binds the incinerator thrown-sphere dispatcher to its lifecycle callback table.
///
/// Must expand to the identifier of an overlay-local const `EnemyTaskFuncTable3`
/// object, indexed by `Task::state`: 0 spawn, 1 flight, 2 destroy. All three
/// callbacks are copied once before checking combat control; there is no
/// terminator or index check. Each callback has signature `void(Enemy*, Task*)`
/// and may destroy the live enemy and its coordinate-body task. The table and
/// callback code must remain loaded for dispatch. Bind only around
/// `glutton_throw_task.inc.c`, then undefine it. This identifier alias takes no
/// arguments, captures no locals and adds no runtime side effects.
#define GLUTTON_THROW_STATES _gGluttonThrowStates
#include "../../shared/creatures/glutton/glutton_throw_task.inc.c"
#undef GLUTTON_THROW_STATES

/// Binds the incinerator glob dispatcher to its lifecycle callback table.
///
/// Must expand to the identifier of an overlay-local const `EnemyTaskFuncTable5`
/// object, indexed by `Task::state`: 0 spawn, 1 descend, 2 engulf, 3 release the
/// player, 4 destroy. All five callbacks are copied once before checking combat
/// control; there is no terminator or index check. Each callback has signature
/// `void(Enemy*, Task*)` and may destroy the live enemy and task. The table and
/// callback code must remain loaded for dispatch. Bind only around
/// `glutton.inc.c`, then undefine it. This identifier alias takes no
/// arguments, captures no locals and adds no runtime side effects.
#define GLUTTON_GLOB_STATES _gGluttonGlobStates
#include "../../shared/creatures/glutton/glutton.inc.c"

/// Binds the incinerator spinner dispatcher to its task-state callback table.
///
/// Must expand to the identifier of an overlay-local const `EnemyTaskFuncTable4`
/// object, ordered by `Task::state`: 0 spawn, 1 wait for release, 2 chase,
/// 3 destroy. The dispatcher reads it once and copies all four callbacks before
/// checking combat control. Each callback has signature `void(Enemy*, Task*)`
/// and may destroy the live enemy and task. Keep the table and callback code
/// loaded for dispatch. Bind only around `glutton_spinner_task.inc.c`, then
/// undefine it; the alias has no arguments or runtime side effects.
#define GLUTTON_SPINNER_STATES _gGluttonSpinnerStates
#include "../../shared/creatures/glutton/glutton_spinner_task.inc.c"
#undef GLUTTON_SPINNER_STATES

/// Reports whether the boss enemy has positive HP.
///
/// Requires a live Enemy in `task->spawnArg2.pointer`. Returns 0 or 1; the
/// message ID and both payloads are ignored.
static s32 _actor444000IsPresent(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    Enemy* enemy = task->spawnArg2.pointer;

    return enemy->hp > 0;
}

/// Places the boss root while preserving its collapse orientation.
///
/// Requires live work/model and a complete borrowed placement through dispatch.
/// Translation uses parent-frame world units; Euler angles use 4096 per turn.
/// Rebuilds X/Y/Z rotation except in collapse states 18/19, which retain their
/// current basis. Marks composition dirty without refreshing it. Returns 1;
/// the message ID and second payload are ignored.
static s32 _actor444000Place(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedSecondArg)
{
    GluttonWork* work = task->work;

    task->extra.tmd->coords->coord.t[0] = placement->pos.vx;
    task->extra.tmd->coords->coord.t[1] = placement->pos.vy;
    task->extra.tmd->coords->coord.t[2] = placement->pos.vz;
    if ((u32)((u16)work->state - ACTOR_444000_STATE_COLLAPSE) >= (u32)(ACTOR_444000_STATE_COLLAPSED - ACTOR_444000_STATE_COLLAPSE + 1)) {
        gfxRotMatrixX(&task->extra.tmd->coords->coord, placement->rot.vx, GRAPHICS_ROTATION_REPLACE);
        gfxRotMatrixY(&task->extra.tmd->coords->coord, placement->rot.vy, GRAPHICS_ROTATION_COMPOSE);
        gfxRotMatrixZ(&task->extra.tmd->coords->coord, placement->rot.vz, GRAPHICS_ROTATION_COMPOSE);
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    return 1;
}

/// Handles summon capture and despawn reports sent to the placed boss.
///
/// Requires live boss work/Enemy and live non-NULL tracked summons through
/// dispatch. Event 0 increments the byte heal counter, queues a 100-HP healing
/// readout and grants HP only to a living boss, without clamping. Event 1
/// decrements a positive summon count, sets a two-update death delay and clears
/// either dead entry in the two-slot summon table. Unknown events do nothing.
/// Returns 1; the message ID and second payload are ignored.
static s32 _actor444000HandleActorEvent(Task* task, s32 messageId, s32 event, s32 unusedSecondArg)
{
    enum {
        ACTOR_444000_EVENT_SUMMON_CAPTURED     = 0,
        ACTOR_444000_EVENT_SUMMON_DESPAWNED    = 1,
        ACTOR_444000_SUMMON_HEAL_HP            = 100,
        ACTOR_444000_SUMMON_DEATH_DELAY_FRAMES = 2,
    };
    GluttonWork* work  = task->work;
    Enemy*       enemy = task->spawnArg2.pointer;

    switch (event) {
        // Credit the capture even after boss death; HP is raised only while alive.
        case ACTOR_444000_EVENT_SUMMON_CAPTURED:
            work->pendingHeals++;
            worldTargetAddReadoutAmount(&enemy->node, -ACTOR_444000_SUMMON_HEAL_HP, 0);
            if (enemy->hp > 0) {
                enemy->hp += ACTOR_444000_SUMMON_HEAL_HP;
            }
            break;
        // Despawn reports arrive while tracked enemy storage is still live.
        case ACTOR_444000_EVENT_SUMMON_DESPAWNED:
            if (work->summonsAlive > 0) {
                work->summonsAlive--;
            }
            work->deathDelay = ACTOR_444000_SUMMON_DEATH_DELAY_FRAMES;
            if (work->summons[0] != NULL && work->summons[0]->hp <= 0) {
                work->summons[0] = NULL;
            }
            if (work->summons[1] != NULL && work->summons[1]->hp <= 0) {
                work->summons[1] = NULL;
            }
            break;
    }
    return 1;
}

/// Selects the boss's dormant behavior for actor message 0x7D9.
///
/// Requires live boss work. Its frame update handles hiding and buffer cleanup
/// after this state change. Returns 1; the message ID and both payloads are
/// ignored.
static s32 _actor444000SetDormantState(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    GluttonWork* work = task->work;

    work->state = ACTOR_444000_STATE_DORMANT;
    return 1;
}

/// Keeps the boss's initialized idle pose with neck overrides disabled.
///
/// Requires live host work/model, all occupied escort models and initialized rigs.
/// Entry clears draw flags and the buffer-release countdown across the host and
/// escorts, then disables neck pitch/yaw. Later ticks advance animation.
static void _actor444000IdleState(Task* task)
{
    GluttonWork* work;
    GluttonWork* flagWork;

    work = task->work;
    if (work->stateChanged != 0) {
        task->extra.tmd->flags  = 0;
        flagWork                = task->work;
        flagWork->freeCountdown = 0;
        task->extra.tmd->flags  = 0;
        _actor444000CopyEscortModelFlags(task, flagWork);
        work->neckPitchEnabled = 0;
        work->neckYawEnabled   = 0;
    } else {
        _gluttonTickAnim(task);
    }
}
