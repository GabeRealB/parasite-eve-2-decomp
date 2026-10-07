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
#include "../../shared/actor_contacts.h"
/// Binds the shared shake helper to this instance's borrowed host task pointer.
#define GLUTTON_HOST_TASK (_gGluttonHostTask.task)

/// Selects garbage-incinerator behavior for this compiled Glutton instance.
///
/// Define before `glutton.h` and retain through every shared fragment. The
/// header defines `GLUTTON_INCINERATOR` as the dimensionless integer 2;
/// the binding must remain a macro for the shared code's `#if` comparisons.
#define GLUTTON_ROOM GLUTTON_INCINERATOR
// Exported instance: another image refers to this package's copy by name.
#define gluttonSetShakeLevel actor444000GluttonSetShakeLevel
#include "../../shared/glutton.h"

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).

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
    SVECTOR   offset;        // Offset from the host's root to the player, whose yaw against the host's facing becomes the neck's yaw target; in the turn, the facing axis of `rootMatrix` scaled to the step forward, then that of the turned matrix scaled to the step back
    GfxMatrix rootMatrix;    // Working copy of the host's root matrix for one tick of the turn; a state change seeds its rotation with identity, which the copy replaces before anything reads it
    byte      unknown_28[2]; // Never accessed; role unproven
    s16       yaw;           // Host's heading advanced by this tick's 0xD, the yaw the rotation is rebuilt about; set to 0x800 on the tick the half turn completes
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

/// The package's event task, whose `Task::work` holds an
/// `_Actor444000EventWork`; `NULL` until that task's spawn state publishes it.
extern Task* D_actor_444000_80161860;
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

/// Gameplay-resident globals the state-3 hand-off touches: `D_shelter_b3_garbage_incinerator_80187150` is the
/// task table the successor is spawned from, `D_shelter_b3_garbage_incinerator_8018FBC8[0]` the view id copied
/// into `GameSession::sceneClock`, and `D_shelter_b3_garbage_incinerator_801855DE` a counter cleared with it.

/// Spawn tables `evsStartScriptWithSkip` forwards to `taskSpawn`, taken as raw
/// addresses: the first pair is used by the `spawnArg1` fast path in state 0
/// and the second by state 2.
extern EvsCommand D_actor_444000_80144634[];
extern EvsCommand D_actor_444000_8014488C[];
extern EvsCommand D_actor_444000_8014431C[];
extern EvsCommand D_actor_444000_801444E4[];

/// Animation-set table this overlay hands the player task as message 0x3F4's
/// `AnimationPlayRequest::source`, the counterpart of `_gActor403100PlayerAnimationSets`. The first
/// entry is a `AnimationSet` in the overlay's own data; the other three point at
/// its work areas.
extern AnimationSet* D_actor_444000_8014430C[4];

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

/// Pair descriptors the host and its escorts publish as `Enemy::param`;
/// `hpMax` is the hit-point pool each one starts with.
extern EnemyParams D_actor_444000_80144A28;
extern EnemyParams D_actor_444000_80144A38;
extern EnemyParams D_actor_444000_80144A48;
extern EnemyParams D_actor_444000_80144A58;

extern s16                       gGluttonEnded;
extern s32                       gGluttonGrabActive;
extern s16                       gGluttonLimbReach;
extern s16                       gGluttonSpinnersReleased;
extern PadScriptCmd              D_actor_444000_80144A74[2];
extern PadScriptVibrationSegment D_actor_444000_80144A7C[2];
extern PadScriptCmd              D_actor_444000_80144A84[2];
extern PadScriptVibrationSegment D_actor_444000_80144A8C[2];
/// Script pair the drag tick spawns every `padScriptPeriod` ticks.
extern PadScriptCmd              D_actor_444000_80144A94[3];
extern PadScriptVibrationSegment D_actor_444000_80144AA0[2];

/// Animation-set tables: the host's two blocks, escort 0's two and escort 1's.
extern AnimationSet* D_actor_444000_80161448[];
extern AnimationSet* D_actor_444000_80161500[];
extern AnimationSet* D_actor_444000_801615B8[];
/// The enemy task's message-handler table, parked in `Task::msgTable`.
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_444000_80161818[7];
/// Spawn table of the seven escorts, indexed 0..6.
extern TaskDesc D_actor_444000_801616B0[];
/// Effect argument block the spawn state points at the host's root coordinate.
extern EffectSpawnArg D_actor_444000_80161880;

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

/// Shared 0x7DA payload buffer, also used by `func_actor_444000_80141618`.
extern _Actor444000CommandStorage D_actor_444000_80161888;
/// Gameplay's escort `TaskDesc` table; entry 3 is the pair this boss spawns.
extern TaskDesc Actor04400_D107E4;

extern AnimationSet* gGluttonCaughtAnimSets[];

/// Which of the three drop-point groups the falling enemies use this round,
/// rerolled off `gRandomLcgState` whenever a spawn arrives with `spawnArg1` 0.
extern u8 gGluttonRainGroup;
/// Per-`spawnArg1` offset from the host model to the point the enemy is stood
/// up at when it is spawned.
extern SVECTOR gGluttonRainLaunchOffsets[];
/// The drop points themselves: `vz` is added to the ring x coordinate and `vx`
/// (less 0x189C) becomes the z coordinate.
extern SVECTOR gGluttonRainPoints[];
/// `[group][spawnArg1]` index into `gGluttonRainPoints`.
extern u8 gGluttonRainPointIndex[][8];
/// Button-press hold the glob's engulf state sends the player, asking for 40
/// presses.
extern GluttonButtonPressHoldStorage gGluttonGrabQuery;

/// World point the spinner chases: written by `func_actor_444000_8013E058`,
/// read by the spinner's tick as the target of its step.
extern SVECTOR gGluttonSpinnerTarget;

/// Shared coordinate `func_actor_444000_80140BBC` rebuilds when the fight
/// reaches sub-state 0x2D of state 9, parented to the host model's fifth part.
extern GluttonCoord D_actor_444000_801618B8;

/// Which of the three shared debris coordinates below the next launch uses,
/// cycled 0/1/2 by `func_actor_444000_801404C0`.
extern s16 D_actor_444000_80161850;

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

/// The three coordinates that debris effects are spawned on, each rebuilt in
/// view space from the first escort's second part.
extern _Actor444000DebrisCoordStorage D_actor_444000_80161948;
/// Spawn table of the enemy the arena fight drops in every tenth step.
extern TaskDesc gGluttonEscortTasks[];

/// The animation-set table the fight installs on the player through message
/// 0x3FF; entry 4 is rebuilt from the player's own weapon block before the
/// second (`field_4 == 4`) send.
/// The companion table used instead when the player is more than a quarter turn
/// off the host's facing, so the hold plays from the other side.

/// Static storage for the mark of how far a catch has taken the player.
///
/// The state the host runs once it has caught the player clears the flag as it
/// places the player ahead of the host, and sets it when the player's scripted
/// animation has stopped and it moves them onto the host's own position.
/// Nothing in the package, its room or the resident code reads the flag, so
/// what it was for is unproven.
///
/// Seven zero bytes separate the flag from the next object. No access to
/// them is recovered, so whether they are trailing fields of this object or a
/// separate unreferenced variable is unproven; they stay in this allocation
/// only to keep the data after it at its address.
typedef struct {
    s8 playerAtHost; // 0 from the start of a catch, 1 once the catch has put the player on the host's root position
    u8 unknown_1[7]; // Zero in the image; no access established and role unproven
} _Actor444000PlayerAtHostStorage;
STATIC_ASSERT_SIZEOF(_Actor444000PlayerAtHostStorage, 8);

/// Set while the escort-order tick holds the player at a placement of its own;
/// 1 marks the plain re-placement, 0 the full grab.
extern _Actor444000PlayerAtHostStorage D_actor_444000_80161868;

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

/// Shared message 0x3E9 placement payload the escort-order tick sends slot 3.
extern _Actor444000TransformStorage D_actor_444000_80161908;
/// Button-press hold the host sends the player before its grab and its swipe
/// take effect. Its press count is never written and stays zero.
extern GluttonButtonPressHoldStorage D_actor_444000_80161928;

/// Global game-mode byte; sits inside a small flag block, so it is declared as
/// an array -- the load has to keep aliasing the scratch stores beside it (see
/// DECOMPILATION_LEARNINGS.md, "Declare a fixed-address global as an array").

/// Per-animation reset argument, a `[?][0x2D]` table of `animId` indexed by
/// the id that was playing before the switch.
extern s8 gGluttonAnimTransitions[][0x2D];

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

static void func_actor_444000_8013482C(Task* task);
static void func_actor_444000_80135448(Task* arg0);
static void func_actor_444000_801371E8(Task* task, s32 scale, s16 face);
static void func_actor_444000_8013AFF8(Enemy* enemy, Task* task);
static void func_actor_444000_801423C4(Enemy* enemy, Task* task);
static void func_actor_444000_801434C4(Task* arg0);
static void func_actor_444000_801435CC(Task* arg0);
s32         func_actor_444000_80143D68(Task* arg0, s32 msgId, s32 arg2, s32 arg3);
s32         func_actor_444000_80143F38(Task* arg0, s32 msgId, s32 arg2, s32 arg3);
static void func_actor_444000_80143F4C(Task* arg0);

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
extern TmdSource    gActor444000GluttonLegRight;

static TmdSource _gActor444000Actor403200Model199E4;
static TmdSource _gActor444000Actor403200Model19284;
static TmdSource _gActor444000Actor403200Model1AC48;

extern TmdSource gActor444000Actor403200Model10824;
s32              func_actor_444000_8013A958(Task*, s32, s32, s32);
s32              func_actor_444000_8013ACD0(Task* task, s32 msgId, ActorCommand* msg, s32 arg3);
s32              func_actor_444000_80143D68(Task*, s32, s32, s32);
s32              func_actor_444000_80143D7C(Task* task, s32 msgId, ActorTransform* placement, s32 arg3);
s32              func_actor_444000_80143E68(Task*, s32, s32, s32);
s32              func_actor_444000_80143F38(Task*, s32, s32, s32);
void             func_actor_444000_80142F28(Task*);

void func_actor_444000_801321FC(s32);
void func_actor_444000_80132358(Task*);
void func_actor_444000_80132608(void);
void func_actor_444000_8013265C(s32);
void func_actor_444000_80132694(void);
void func_actor_444000_801326DC(void);
void func_actor_444000_80132724(s16);
void func_actor_444000_80132778(void);
void func_actor_444000_801327E8(s16);

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

AnimationSet* D_actor_444000_8014430C[4] = {
    &_gActor444000Animation124C4,
    &gActor444000Animation2E548,
    &gActor444000Animation2EA80,
    &gActor444000Animation2EE14,
};

EvsCommand D_actor_444000_8014431C[19] = {
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_444000_801327E8 }, { .value = ACTOR_444000_PLAYER_ACTION_WEAPON_CLIP_BLENDED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_444000_80132608 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_444000_801326DC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_444000_8013265C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_444000_8013265C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_444000_801321FC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_444000_801321FC }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_444000_80132694 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_444000_801444E4[14] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_444000_801326DC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_444000_801321FC }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_444000_80132694 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_444000_8013265C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_444000_80132608 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_444000_80144634[25] = {
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_444000_801327E8 }, { .value = ACTOR_444000_PLAYER_ACTION_EVENT_CLIP_0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_444000_801321FC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_444000_80132778 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_444000_801327E8 }, { .value = ACTOR_444000_PLAYER_ACTION_EVENT_CLIP_3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_444000_8013265C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_444000_801327E8 }, { .value = ACTOR_444000_PLAYER_ACTION_WEAPON_CLIP_BLENDED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_444000_8013265C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_444000_801321FC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_444000_801321FC }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_444000_80132694 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_444000_8014488C[15] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_444000_801321FC }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_444000_80132724 }, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_444000_80132694 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_444000_8013265C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_444000_80132778 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_444000_80132608 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_actor_444000_801449F4 = { { { TASK_BODY_NONE, 192 } }, func_actor_444000_80132358, { .value = 0 } };

DamageAttack D_actor_444000_80144A00[6] = {
    { 0, 0 },
    { 30, 0 },
    { 25, 3 },
    { 9999, 0 },
    { 50, 11 },
    { 35, 0 },
};

EnemyParams D_actor_444000_80144A18 = { D_actor_444000_80144A00, 3000, 500, 200, 100, 100, 0, 0, 0 };

EnemyParams D_actor_444000_80144A28 = { D_actor_444000_80144A00, 3000, 700, 200, 100, 100, 0, 0, 0 };

EnemyParams D_actor_444000_80144A38 = { D_actor_444000_80144A00, 120, 0, 0, 0, 50, 0, 0, 0 };

EnemyParams D_actor_444000_80144A48 = { D_actor_444000_80144A00, 120, 0, 0, 0, 50, 0, 0, 0 };

EnemyParams D_actor_444000_80144A58 = { D_actor_444000_80144A00, 200, 0, 0, 0, 10, 0, 0, 0 };

s16 gGluttonEnded = 0;

s32 gGluttonGrabActive = 0;

s16 gGluttonLimbReach = 0;

s16 gGluttonSpinnersReleased = 0;

PadScriptCmd D_actor_444000_80144A74[2] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_actor_444000_80144A7C[2] = {
    { 0, 0, 7, 0 },
    { 255, 53, 27, 1 },
};

PadScriptCmd D_actor_444000_80144A84[2] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_actor_444000_80144A8C[2] = {
    { 186, 74, 32, 1 },
    { 0, 0, 7, 0 },
};

PadScriptCmd D_actor_444000_80144A94[3] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_actor_444000_80144AA0[2] = {
    { 22, 109, 20, 1 },
    { 80, 31, 34, 1 },
};

static TmdBone _gActor444000Actor403200Model10824Skeleton[8] = {
#include "assets/actor_403200_model_10824_skeleton.inc"
};

static u32 _gActor444000Actor403200Model10824PartVerts[8] = {
#include "assets/actor_403200_model_10824_partVerts.inc"
};

static SVECTOR _gActor444000Actor403200Model10824Verts[135] = {
#include "assets/actor_403200_model_10824_verts.inc"
};

static SVECTOR _gActor444000Actor403200Model10824Normals[135] = {
#include "assets/actor_403200_model_10824_normals.inc"
};

static u32 _gActor444000Actor403200Model10824Stream[1732] = {
#include "assets/actor_403200_model_10824_stream.inc"
};

TmdSource gActor444000Actor403200Model10824 = {
    0,
    8496,
    3588,
    8,
    _gActor444000Actor403200Model10824PartVerts,
    _gActor444000Actor403200Model10824Verts,
    _gActor444000Actor403200Model10824Normals,
    _gActor444000Actor403200Model10824Skeleton,
    _gActor444000Actor403200Model10824Stream,
};

static TmdBone _gActor444000Actor403200Model12884Skeleton[1] = {
#include "assets/actor_403200_model_12884_skeleton.inc"
};

static u32 _gActor444000Actor403200Model12884PartVerts[1] = {
#include "assets/actor_403200_model_12884_partVerts.inc"
};

static SVECTOR _gActor444000Actor403200Model12884Verts[82] = {
#include "assets/actor_403200_model_12884_verts.inc"
};

static SVECTOR _gActor444000Actor403200Model12884Normals[79] = {
#include "assets/actor_403200_model_12884_normals.inc"
};

static u32 _gActor444000Actor403200Model12884Stream[533] = {
#include "assets/actor_403200_model_12884_stream.inc"
};

TmdSource gActor444000Actor403200Model12884 = {
    0,
    3696,
    0,
    1,
    _gActor444000Actor403200Model12884PartVerts,
    _gActor444000Actor403200Model12884Verts,
    _gActor444000Actor403200Model12884Normals,
    _gActor444000Actor403200Model12884Skeleton,
    _gActor444000Actor403200Model12884Stream,
};

static TmdBone _gActor444000Actor403200Model13774Skeleton[1] = {
#include "assets/actor_403200_model_13774_skeleton.inc"
};

static u32 _gActor444000Actor403200Model13774PartVerts[1] = {
#include "assets/actor_403200_model_13774_partVerts.inc"
};

static SVECTOR _gActor444000Actor403200Model13774Verts[90] = {
#include "assets/actor_403200_model_13774_verts.inc"
};

static SVECTOR _gActor444000Actor403200Model13774Normals[112] = {
#include "assets/actor_403200_model_13774_normals.inc"
};

static u32 _gActor444000Actor403200Model13774Stream[698] = {
#include "assets/actor_403200_model_13774_stream.inc"
};

TmdSource gActor444000Actor403200Model13774 = {
    0,
    4808,
    0,
    1,
    _gActor444000Actor403200Model13774PartVerts,
    _gActor444000Actor403200Model13774Verts,
    _gActor444000Actor403200Model13774Normals,
    _gActor444000Actor403200Model13774Skeleton,
    _gActor444000Actor403200Model13774Stream,
};

static TmdBone _gActor444000GluttonLegLeftSkeleton[4] = {
#include "assets/glutton_leg_left_skeleton.inc"
};

static u32 _gActor444000GluttonLegLeftPartVerts[4] = {
#include "assets/glutton_leg_left_partVerts.inc"
};

static SVECTOR _gActor444000GluttonLegLeftVerts[104] = {
#include "assets/glutton_leg_left_verts.inc"
};

static SVECTOR _gActor444000GluttonLegLeftNormals[115] = {
#include "assets/glutton_leg_left_normals.inc"
};

static u32 _gActor444000GluttonLegLeftStream[1032] = {
#include "assets/glutton_leg_left_stream.inc"
};

TmdSource gActor444000GluttonLegLeft = {
    0,
    6172,
    1048,
    4,
    _gActor444000GluttonLegLeftPartVerts,
    _gActor444000GluttonLegLeftVerts,
    _gActor444000GluttonLegLeftNormals,
    _gActor444000GluttonLegLeftSkeleton,
    _gActor444000GluttonLegLeftStream,
};

static TmdBone _gActor444000GluttonLegRightSkeleton[4] = {
#include "assets/glutton_leg_right_skeleton.inc"
};

static u32 _gActor444000GluttonLegRightPartVerts[4] = {
#include "assets/glutton_leg_right_partVerts.inc"
};

static SVECTOR _gActor444000GluttonLegRightVerts[104] = {
#include "assets/glutton_leg_right_verts.inc"
};

static SVECTOR _gActor444000GluttonLegRightNormals[115] = {
#include "assets/glutton_leg_right_normals.inc"
};

static u32 _gActor444000GluttonLegRightStream[1032] = {
#include "assets/glutton_leg_right_stream.inc"
};

TmdSource gActor444000GluttonLegRight = {
    0,
    6172,
    1048,
    4,
    _gActor444000GluttonLegRightPartVerts,
    _gActor444000GluttonLegRightVerts,
    _gActor444000GluttonLegRightNormals,
    _gActor444000GluttonLegRightSkeleton,
    _gActor444000GluttonLegRightStream,
};

static TmdBone _gActor444000Actor403200Model1785CSkeleton[8] = {
#include "assets/actor_403200_model_1785C_skeleton.inc"
};

static u32 _gActor444000Actor403200Model1785CPartVerts[8] = {
#include "assets/actor_403200_model_1785C_partVerts.inc"
};

static SVECTOR _gActor444000Actor403200Model1785CVerts[82] = {
#include "assets/actor_403200_model_1785C_verts.inc"
};

static SVECTOR _gActor444000Actor403200Model1785CNormals[82] = {
#include "assets/actor_403200_model_1785C_normals.inc"
};

static u32 _gActor444000Actor403200Model1785CStream[835] = {
#include "assets/actor_403200_model_1785C_stream.inc"
};

static TmdSource _gActor444000Actor403200Model1785C = {
    0,
    4140,
    1872,
    8,
    _gActor444000Actor403200Model1785CPartVerts,
    _gActor444000Actor403200Model1785CVerts,
    _gActor444000Actor403200Model1785CNormals,
    _gActor444000Actor403200Model1785CSkeleton,
    _gActor444000Actor403200Model1785CStream,
};

static TmdBone _gActor444000Actor403200Model18BE4Skeleton[1] = {
#include "assets/actor_403200_model_18BE4_skeleton.inc"
};

static u32 _gActor444000Actor403200Model18BE4PartVerts[1] = {
#include "assets/actor_403200_model_18BE4_partVerts.inc"
};

static SVECTOR _gActor444000Actor403200Model18BE4Verts[22] = {
#include "assets/actor_403200_model_18BE4_verts.inc"
};

static SVECTOR _gActor444000Actor403200Model18BE4Normals[22] = {
#include "assets/actor_403200_model_18BE4_normals.inc"
};

static u32 _gActor444000Actor403200Model18BE4Stream[173] = {
#include "assets/actor_403200_model_18BE4_stream.inc"
};

TmdSource gActor444000Actor403200Model18BE4 = {
    0,
    1136,
    0,
    1,
    _gActor444000Actor403200Model18BE4PartVerts,
    _gActor444000Actor403200Model18BE4Verts,
    _gActor444000Actor403200Model18BE4Normals,
    _gActor444000Actor403200Model18BE4Skeleton,
    _gActor444000Actor403200Model18BE4Stream,
};

static TmdBone _gActor444000Actor403200Model199E4Skeleton[1] = {
#include "assets/actor_403200_model_199E4_skeleton.inc"
};

static u32 _gActor444000Actor403200Model199E4PartVerts[1] = {
#include "assets/actor_403200_model_199E4_partVerts.inc"
};

static SVECTOR _gActor444000Actor403200Model199E4Verts[70] = {
#include "assets/actor_403200_model_199E4_verts.inc"
};

static u32 _gActor444000Actor403200Model199E4Stream[618] = {
#include "assets/actor_403200_model_199E4_stream.inc"
};

static TmdSource _gActor444000Actor403200Model199E4 = {
    0,
    3536,
    0,
    1,
    _gActor444000Actor403200Model199E4PartVerts,
    _gActor444000Actor403200Model199E4Verts,
    &_gActor444000Actor403200Model199E4Verts[70],
    _gActor444000Actor403200Model199E4Skeleton,
    _gActor444000Actor403200Model199E4Stream,
};

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

static TmdBone _gActor444000Actor403200Model19284Skeleton[1] = {
#include "assets/actor_403200_model_19284_skeleton.inc"
};

static u32 _gActor444000Actor403200Model19284PartVerts[1] = {
#include "assets/actor_403200_model_19284_partVerts.inc"
};

static SVECTOR _gActor444000Actor403200Model19284Verts[58] = {
#include "assets/actor_403200_model_19284_verts.inc"
};

static SVECTOR _gActor444000Actor403200Model19284Normals[58] = {
#include "assets/actor_403200_model_19284_normals.inc"
};

static u32 _gActor444000Actor403200Model19284Stream[313] = {
#include "assets/actor_403200_model_19284_stream.inc"
};

static TmdSource _gActor444000Actor403200Model19284 = {
    0,
    2176,
    0,
    1,
    _gActor444000Actor403200Model19284PartVerts,
    _gActor444000Actor403200Model19284Verts,
    _gActor444000Actor403200Model19284Normals,
    _gActor444000Actor403200Model19284Skeleton,
    _gActor444000Actor403200Model19284Stream,
};

static TmdBone _gActor444000Actor403200Model1AC48Skeleton[1] = {
#include "assets/actor_403200_model_1AC48_skeleton.inc"
};

static u32 _gActor444000Actor403200Model1AC48PartVerts[1] = {
#include "assets/actor_403200_model_1AC48_partVerts.inc"
};

static SVECTOR _gActor444000Actor403200Model1AC48Verts[135] = {
#include "assets/actor_403200_model_1AC48_verts.inc"
};

static SVECTOR _gActor444000Actor403200Model1AC48Normals[135] = {
#include "assets/actor_403200_model_1AC48_normals.inc"
};

static u32 _gActor444000Actor403200Model1AC48Stream[1400] = {
#include "assets/actor_403200_model_1AC48_stream.inc"
};

static TmdSource _gActor444000Actor403200Model1AC48 = {
    0,
    9492,
    0,
    1,
    _gActor444000Actor403200Model1AC48PartVerts,
    _gActor444000Actor403200Model1AC48Verts,
    _gActor444000Actor403200Model1AC48Normals,
    _gActor444000Actor403200Model1AC48Skeleton,
    _gActor444000Actor403200Model1AC48Stream,
};

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

s8 gGluttonAnimTransitions[45][45] = {
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

AnimationSet* D_actor_444000_80161448[46] = {
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

AnimationSet* D_actor_444000_80161500[46] = {
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

AnimationSet* D_actor_444000_801615B8[46] = {
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

AnimationSet* D_actor_444000_80161670[8] = {
    NULL,
    &_gActor444000Animation28920,
    &_gActor444000Animation2D330,
    NULL,
    NULL,
    &_gActor444000Animation27D84,
    &_gActor444000Animation2D330,
    NULL,
};

u8 gGluttonRainGroup = 0;

AnimationSet* gGluttonCaughtAnimSets[7] = {
    NULL,
    &_gActor444000Animation2CB24,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

void             func_actor_444000_80143888(Task*);
extern TmdSource gActor444000GluttonLegLeft;
extern TmdSource gActor444000Actor403200Model12884;
extern TmdSource gActor444000Actor403200Model13774;
static TmdSource _gActor444000Actor403200Model1785C;
extern TmdSource gActor444000Actor403200Model18BE4;
extern TmdSource D_actor_444000_80161B50;

TaskDesc D_actor_444000_801616B0[7] = {
    { { { TASK_BODY_TMD, 96 } }, gluttonPropTask, { .model = &gActor444000GluttonLegRight } },
    { { { TASK_BODY_TMD, 96 } }, gluttonPropTask, { .model = &gActor444000GluttonLegLeft } },
    { { { TASK_BODY_TMD, 96 } }, gluttonPropTask, { .model = &gActor444000Actor403200Model12884 } },
    { { { TASK_BODY_TMD, 96 } }, gluttonPropTask, { .model = &gActor444000Actor403200Model13774 } },
    { { { TASK_BODY_TMD, 96 } }, gluttonPropTask, { .model = &_gActor444000Actor403200Model1785C } },
    { { { TASK_BODY_TMD, 96 } }, gluttonPropTask, { .model = &gActor444000Actor403200Model18BE4 } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_444000_80143888, { .model = &D_actor_444000_80161B50 } },
};

SVECTOR gGluttonRainLaunchOffsets[8] = {
    { -1000, 0, -1800, 0 },
    { 800, 0, -800, 0 },
    { -1300, 0, 200, 0 },
    { 1200, 0, 1000, 0 },
    { 900, 0, -1700, 0 },
    { -800, 0, -880, 0 },
    { 1280, 0, 0, 0 },
    { -1100, 0, 900, 0 },
};

SVECTOR gGluttonRainPoints[16] = {
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

u8 gGluttonRainPointIndex[3][8] = {
    { 7, 10, 8, 0, 4, 3, 12, 9 },
    { 2, 5, 12, 0, 6, 14, 13, 10 },
    { 14, 13, 9, 0, 12, 7, 10, 11 },
};

TaskDesc gGluttonEscortTasks[4] = {
    { { { TASK_BODY_TMD, 96 } }, gluttonGlobTask, { .model = &_gActor444000Actor403200Model199E4 } },
    { { { TASK_BODY_COORD, 96 } }, gluttonRainTask, { .value = 0 } },
    { { { TASK_BODY_COORD, 96 } }, gluttonThrowTask, { .value = 0 } },
    { { { TASK_BODY_TMD, 96 } }, gluttonChunkTask, { .model = &_gActor444000Actor403200Model1AC48 } },
};

TaskDesc D_actor_444000_8016180C = { { { TASK_BODY_TMD, 96 } }, gluttonSpinnerTask, { .model = &_gActor444000Actor403200Model19284 } };

TaskMessageEntry D_actor_444000_80161818[7] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_444000_8013A958 },
    { ACTOR_MESSAGE_IS_PRESENT, func_actor_444000_80143D68 },
    { ACTOR_MESSAGE_PLACE, func_actor_444000_80143D7C },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_444000_8013ACD0 },
    { ROOM_MESSAGE_ACTOR_EVENT, func_actor_444000_80143E68 },
    { SCENE_MESSAGE_EXIT_PLACED_ACTORS, func_actor_444000_80143F38 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

s16 D_actor_444000_80161850 = 0;

TaskDesc D_actor_444000_80161854 = { { { TASK_BODY_TMD, 96 } }, func_actor_444000_80142F28, { .model = &gActor444000Actor403200Model10824 } };

Task* D_actor_444000_80161860 = NULL;

u32 D_actor_444000_80161864 = 0x1A90C60D;

_Actor444000PlayerAtHostStorage D_actor_444000_80161868 = { 0, { 0, 0, 0, 0, 0, 0, 0 } };

SVECTOR ActorContact_ScratchPosition = { 0, 0, 0, 0 };

/// Borrowed host task reference for screen-shake requests while the boss lives.
static _GluttonHostTaskStorage _gGluttonHostTask = { NULL, { 0 } };

EffectSpawnArg D_actor_444000_80161880 = { NULL, 0, 0 };

_Actor444000CommandStorage D_actor_444000_80161888 = { { { .loc = { 0, 0 } }, 0 }, { 0, 0, 0, 0 } };

SVECTOR gGluttonSpinnerTarget = { 0, 0, 0, 0 };

GluttonButtonPressHoldStorage gGluttonGrabQuery = { { { 0 }, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0 } };

GluttonCoord D_actor_444000_801618B8 = { .node = { 0, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } };

_Actor444000TransformStorage D_actor_444000_80161908 = { { { 0, 0, 0, 0 }, { 0, 0, 0, 0 } }, { 0, 0, 0, 0, 0, 0, 0, 0 } };

GluttonButtonPressHoldStorage D_actor_444000_80161928 = { { { 0 }, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0 } };

_Actor444000DebrisCoordStorage D_actor_444000_80161948 = { { { 0, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL }, { 0, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL }, { 0, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, { 0 } };

TmdSource D_actor_444000_80161B50 = { 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL };

static void            func_actor_444000_80132054(Task* task);
static __inline__ void Actor444000_StepForward(GfxCoord* coord);
static __inline__ void Actor444000_SquashRotation(GfxCoord* coord, s16 y);
static __inline__ void Actor444000_RebuildRotation(Task* task);
static __inline__ void Actor444000_SeedRootCoord(Task* task, GluttonWork* work);
static void            func_actor_444000_8013CA60(Task* task);
static void            func_actor_444000_8013D810(Task* arg0);
static __inline__ void Actor444000_ReleaseRotScratch(void);
static __inline__ void Actor444000_FlattenRotation(GfxCoord* coord, s32 vy);
static void            func_actor_444000_8013D96C(Task* arg0);
static void            func_actor_444000_8013E058(Task* task);
static __inline__ void Actor444000_PlacePlayerAhead(Task* task, GluttonWork* work,
                                                    Task* player, _Actor444000CatchScratch* sc,
                                                    PlayerStatus* cfg);
static void            func_actor_444000_8013EC84(Task* arg0);
static void            func_actor_444000_8013FB74(Task* arg0);
static void            func_actor_444000_801404C0(Task* arg0);
static void            func_actor_444000_80140BBC(Task* arg0);
static void            func_actor_444000_80140E28(Task* arg0);
static void            func_actor_444000_8014105C(Task* arg0);
static void            func_actor_444000_801411C8(Task* arg0);
static void            func_actor_444000_80141618(Task* task);
static void            func_actor_444000_80142254(void);

/// Run one step of the event task: perform the request pending in
/// `_Actor444000EventWork::playerAction`, then clear it so it fires once.
static void func_actor_444000_80132054(Task* task)
{
    _Actor444000EventWork* work = task->work;
    _Actor444000EventWork* published;
    _Actor444000EventWork* reloaded;
    AnimationPlayRequest   msg;
    s32                    anim;

    switch (work->playerAction) {
        case ACTOR_444000_PLAYER_ACTION_NONE:
            break;
        case ACTOR_444000_PLAYER_ACTION_WEAPON_CLIP_BLENDED:
            // The equipped weapon selects the bank; each character has its own run of banks.
            anim = gPlayerStatus.weapon;
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) {
                anim += 1;
            } else {
                anim += 0x22;
            }
            msg.source.index         = anim;
            msg.animationId          = 1;
            msg.blend                = ANIMATION_BLEND_INTERPOLATE;
            msg.blendFrames          = 0xA;
            msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &msg, 0);
            break;
        case ACTOR_444000_PLAYER_ACTION_EVENT_CLIP_3:
            if (work->player != NULL) {
                msg.source.sets          = D_actor_444000_8014430C;
                msg.animationId          = 3;
                msg.blend                = ANIMATION_BLEND_RESET;
                msg.blendFrames          = 0;
                msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(work->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
            }
            // The alert sounds once, whether this request or the script's own callback reaches it first.
            published = D_actor_444000_80161860->work;
            if (published->alertPlayed == 0) {
                sndEvtRequestScriptStart(SOUND_SHELTER_B3_INCINERATOR_ALERT, 0, 0);
                published->alertPlayed = 1;
            }
            break;
        case ACTOR_444000_PLAYER_ACTION_EVENT_CLIP_0:
            roomEffectRequestCancelAll();
            Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
            reloaded           = task->work;
            if (reloaded->player != NULL) {
                msg.source.sets          = D_actor_444000_8014430C;
                msg.animationId          = 0;
                msg.blend                = ANIMATION_BLEND_INTERPOLATE;
                msg.blendFrames          = 0xA;
                msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(reloaded->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
            }
            break;
    }
    work->playerAction = ACTOR_444000_PLAYER_ACTION_NONE;
}

/// Bring the room's presentation up to date for an enter (0), a first entry
/// (1) or a re-entry (2): pick the view set from the current disc/scenario
/// stage in `GameSession::incineratorDescentPhase`, reselect the view saved in
/// `_Actor444000EventWork::savedView`, and on a first entry start the
/// framebuffer-blend effect. Any other `arg0` does nothing.
void func_actor_444000_801321FC(s32 arg0)
{
    _Actor444000EventWork* work;

    work = D_actor_444000_80161860->work;
    switch (arg0) {
        case 0:
            gGameSession->viewDirty                                    = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = work->savedView;
            break;
        case 1:
        case 2:
            switch (gGameSession->incineratorDescentPhase) {
                case GAME_SESSION_INCINERATOR_DESCENT_WAITING:
                    gGameSession->location.loc.room                            = 4;
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 4;
                    break;
                case GAME_SESSION_INCINERATOR_DESCENT_MOVING:
                    gGameSession->location.loc.room                            = 5;
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 5;
                    break;
                case GAME_SESSION_INCINERATOR_DESCENT_LANDED:
                case GAME_SESSION_INCINERATOR_DESCENT_COMPLETE:
                    gGameSession->location.loc.room                            = 6;
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 6;
                    break;
            }
            gGameSession->eventRoomIndex                               = gGameSession->location.loc.room - 1;
            gGameSession->incineratorRoomGroup                         = 1;
            gGameSession->roomObjsDirty                                = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = work->savedView;
            Gp_ApplyAreaRecs(D_shelter_b3_garbage_incinerator_8018FB6C);
            if (arg0 == 1) {
                work->framebufferBlend = taskSpawn(1, 0x2D, 0x10, 0);
            }
            gGameSession->viewDirty = 1;
            break;
    }
}

/// Task body of the overlay's event/controller task, run once per frame while
/// the session is not paused (`GameSession::sceneUpdatesPaused`), the attachment wheel is closed
/// (`Gp_StateC08.menuOpen`) and the battle state is not frozen
/// (`gSceneCombatState.actorControl`).
///
/// State 0 allocates the `_Actor444000EventWork` block and publishes the task in
/// `D_actor_444000_80161860`; a task spawned with `spawnArg1` set jumps
/// straight to state 3, otherwise it advances one state at a time. State 1
/// counts 0x2BD frames and then arms the death/ending sequence once. State 2
/// counts 0x15 frames and hands off to the follow-up task table. State 3 waits
/// for the room to settle, spawns the successor from `D_shelter_b3_garbage_incinerator_80187150` and kills
/// this task.
void func_actor_444000_80132358(Task* task)
{
    _Actor444000EventWork* work = task->work;
    _Actor444000EventWork* alloc;
    _Actor444000EventWork* published;
    s32                    state;
    s16                    timer;

    if (gGameSession->sceneUpdatesPaused != 0) {
        return;
    }
    if (Gp_StateC08.menuOpen != ATTACHMENT_MENU_CLOSED) {
        return;
    }
    if (gSceneCombatState.actorControl != SCENE_COMBAT_ACTORS_RUNNING) {
        return;
    }

    state = task->state;
    switch (state) {
        case 0:
            if (Gp_StateC08.mode == ATTACHMENT_MODE_WHEEL) {
                return;
            }
            if (gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                return;
            }
            alloc      = memCalloc(sizeof(*alloc), false);
            task->work = alloc;
            if (alloc == NULL) {
                taskKill(task);
            } else {
                memFillBytes(alloc, 0, sizeof(*alloc));
                alloc->player           = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                D_actor_444000_80161860 = task;
            }
            if (task->spawnArg1.value != 0) {
                work            = task->work;
                work->savedView = gGameSession->location.loc.view;
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                evsStartScriptWithSkip(D_actor_444000_80144634, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_444000_8014488C);
                task->state = 3;
            } else {
                task->state += 1;
            }
            break;
        case 1:
            timer               = (u16)task->killCountdown + 1;
            task->killCountdown = timer;
            if (timer >= 0x2BD) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                published = D_actor_444000_80161860->work;
                if (published->combatReset == 0) {
                    gSceneCombatState.battleRefs                        = 0;
                    gSceneCombatState.signals.bytes.endDelayFrames      = 0xF;
                    gSceneCombatState.signals.bytes.battlePhase         = SCENE_COMBAT_BATTLE_IDLE;
                    gSceneCombatState.signals.bytes.actionFlags         = 0;
                    gSceneCombatState.signals.bytes.enemyAlert          = 0;
                    gGameSession->flowFlags                            |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 0xD;
                    published->combatReset                              = state;
                }
                task->killCountdown = 0;
                task->state        += 1;
            }
            break;
        case 2:
            timer               = (u16)task->killCountdown + 1;
            task->killCountdown = timer;
            if (timer >= 0x15) {
                work->savedView = gGameSession->location.loc.view;
                evsStartScriptWithSkip(D_actor_444000_8014431C, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_444000_801444E4);
                task->state += 1;
            }
            break;
        case 3:
            if (gGameSession->eventState == 0) {
                D_shelter_b3_garbage_incinerator_801855DE = 0;
                gGameSession->sceneClock                  = D_shelter_b3_garbage_incinerator_8018FBC8[0];
                taskSpawnFromTable(D_shelter_b3_garbage_incinerator_80187150, 0, 1, 0);
                taskKill(task);
                return;
            }
            break;
    }
    func_actor_444000_80132054(task);
}

/// Play the event's alert once, latching `_Actor444000EventWork::alertPlayed`
/// so a repeat call is a no-op.
void func_actor_444000_80132608(void)
{
    _Actor444000EventWork* work = D_actor_444000_80161860->work;

    if (work->alertPlayed == 0) {
        sndEvtRequestScriptStart(SOUND_SHELTER_B3_INCINERATOR_ALERT, 0, 0);
        work->alertPlayed = 1;
    }
}

/// Send the model-draw switch `arg0` to the player task the event work block
/// carries.
void func_actor_444000_8013265C(s32 arg0)
{
    _Actor444000EventWork* work = D_actor_444000_80161860->work;

    taskMessageDispatch(work->player, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, arg0, 0);
}

/// Kill the framebuffer-blend effect task the event work block carries, if one
/// is running.
void func_actor_444000_80132694(void)
{
    _Actor444000EventWork* work = D_actor_444000_80161860->work;

    if (work->framebufferBlend != NULL) {
        taskKill(work->framebufferBlend);
        work->framebufferBlend = NULL;
    }
}

void func_actor_444000_801326DC(void)
{
    ActorCommand msg;

    msg.context.loc.stage = 0;
    msg.context.loc.area  = 0x2C;
    msg.command           = 3;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
}

/// Send message 0x7DA to the slot-4 task, tagged with the current session's
/// stage and area and the caller's selector. Nothing in the actor calls it.
void func_actor_444000_80132724(s16 arg0)
{
    ActorCommand msg;

    msg.context.loc.stage = gGameSession->location.loc.stage;
    msg.context.loc.area  = gGameSession->location.loc.area;
    msg.command           = arg0;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
}

/// Arm the actor's death sequence once: reset the `gSceneCombatState` claim block,
/// flag the session and pick area script 0xD, then latch
/// `_Actor444000EventWork::combatReset` so a later call does nothing.
void func_actor_444000_80132778(void)
{
    _Actor444000EventWork* work = D_actor_444000_80161860->work;

    if (work->combatReset == 0) {
        gSceneCombatState.battleRefs                        = 0;
        gSceneCombatState.signals.bytes.endDelayFrames      = 0xF;
        gSceneCombatState.signals.bytes.battlePhase         = SCENE_COMBAT_BATTLE_IDLE;
        gSceneCombatState.signals.bytes.actionFlags         = 0;
        gSceneCombatState.signals.bytes.enemyAlert          = 0;
        gGameSession->flowFlags                            |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 0xD;
        work->combatReset                                   = 1;
    }
}

/// Leave a player request (`ACTOR_444000_PLAYER_ACTION_*`) for the event task's
/// next tick, zeroing the step that goes with it.
void func_actor_444000_801327E8(s16 action)
{
    _Actor444000EventWork* work = D_actor_444000_80161860->work;

    work->playerAction     = action;
    work->playerActionStep = 0;
}

#include "../../shared/actor_contacts_turn_joint.inc.c"

#include "../../shared/actor_contacts_push_contact.inc.c"

#include "../../shared/glutton_wall.inc.c"

#include "../../shared/glutton_pose_limb.inc.c"

#include "../../shared/glutton_turn_neck.inc.c"

#include "../../shared/glutton_pitch_neck.inc.c"

#include "../../shared/glutton_seed_blend.inc.c"

#include "../../shared/glutton_switch_anim.inc.c"

#include "../../shared/glutton_tick_blended.inc.c"

#include "../../shared/glutton_tick_anim.inc.c"

#include "../../shared/glutton_hit_effect.inc.c"

/// Walk `coord` a fixed 0x32/0x1000 of its own forward axis (column 2 of its
/// rotation, normalised and GPF-scaled) and flag it for rebuild. The direction
/// vector lives in an `SVECTOR` carved off the scratch stack and handed straight
/// back; written as an inline so those scratch-head accesses stay absolute, the
/// same reason as `_actorRenderRescaleYawHalf` above.
static __inline__ void Actor444000_StepForward(GfxCoord* coord)
{
    u8*      head;
    SVECTOR* dir;

    head                          = SCRATCH_STACK_CURSOR(u8);
    dir                           = (SVECTOR*)(head - sizeof(SVECTOR));
    SCRATCH_STACK_CURSOR(SVECTOR) = dir;

    gfxReadMatrixZAxis(&coord->coord, dir);
    VectorNormalSS(dir, dir);
    gte_lddp(0x32);
    gte_ldsv(dir);
    gte_gpf12();
    gte_stsv(dir);

    coord->coord.t[0]  += dir->vx;
    coord->coord.t[1]  += dir->vy;
    coord->coord.t[2]  += dir->vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;

    SCRATCH_STACK_RELEASE_BYTES(sizeof(SVECTOR));
}

/// The run-out / turn / run-back pass, stepped by `GluttonWork::phase`.
///
/// A reset request re-arms the block: the model's flag word and the enemy's
/// link state are cleared, both colour steps are switched on, animation 2 is
/// requested and the scratch matrix is seeded with an identity rotation.
///
/// Every step publishes the yaw from the model's own facing to the player in
/// `neckYawTarget`, wrapped into +/-0x800, and -- unless the game is frozen --
/// walks the model forward along that facing. State 0 runs out to x 0x1770,
/// state 1 turns the model 0xD a step until it has swung the full half turn
/// (its rotation is rebuilt from the running `yaw` rather than spun in
/// place), and states 2 to 5 run it back through -0x1387, -0x251B and -0x32C7.
/// Past -0x4203 the task hands over to state 0x10 and tells the player task
/// (slot 7) message 0x13F4.
static void func_actor_444000_8013482C(Task* task)
{
    _Actor444000RunScratch* sc;
    GfxMatrix*              mat;
    TmdObject*              tmd;
    GluttonWork*            work;
    Enemy*                  enemy;
    GfxCoord*               coord;
    GfxCoord*               model;
    u8*                     head;
    s32                     frame;

    head = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_RESERVE_BYTES(sizeof(_Actor444000RunScratch));
    sc = SCRATCH_STACK_CURSOR(_Actor444000RunScratch);

    work  = task->work;
    enemy = task->spawnArg2.pointer;

    if (work->stateChanged != 0) {
        tmd                           = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        tmd->flags                    = 0;
        work->neckPitchEnabled        = 1;
        work->animId                  = 2;
        work->neckYawEnabled          = 1;
        work->animStep                = GLUTTON_ANIM_STEP_BLEND;
        work->hostExposed             = 0;
        work->neckPitchTarget         = 0;
        mat                           = &((_Actor444000RunScratch*)(head - sizeof(_Actor444000RunScratch)))->rootMatrix;
        mat->rotationWords.m00M01     = ONE;
        mat->rotationWords.m02M10     = 0;
        mat->rotationWords.m11M12     = ONE;
        mat->rotationWords.m20M21     = 0;
        mat->rotationWords.m22        = ONE;
    }

    _gluttonTickAnim(task);

    frame = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    if (frame == 0x12 && work->clip.prevSlot2Cue != frame) {
        s32 id;
        s32 pan;

        work->shakeLevel = GLUTTON_SHAKE_LONG;
        padScriptSpawn(D_actor_444000_80144A74, D_actor_444000_80144A7C);
        id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200001;
        pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(id, pan, (s8)(worldCoordGetOriginAudioDepth(task->extra.tmd->coords) / 2));
    }

    frame = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    if (frame == 0x18 && work->clip.prevSlot2Cue != frame) {
        s32 id;
        s32 pan;

        work->shakeLevel = GLUTTON_SHAKE_LONG;
        padScriptSpawn(D_actor_444000_80144A74, D_actor_444000_80144A7C);
        id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200001;
        pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(id, pan, (s8)(worldCoordGetOriginAudioDepth(task->extra.tmd->coords) / 2));
    }

    work->clip.prevSlot2Cue = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;

    model         = task->extra.tmd->coords;
    sc->offset.vx = gPlayerStatus.coordMtx->t[0] - model->coord.t[0];
    sc->offset.vy = gPlayerStatus.coordMtx->t[1] - model->coord.t[1];
    sc->offset.vz = gPlayerStatus.coordMtx->t[2] - model->coord.t[2];

    work->neckYawTarget = actorYawTo(task->extra.tmd->coords, sc->offset.vx, sc->offset.vz);

    switch (work->phase) {
        case 0: {
            s32       paused = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen;
            GfxCoord* c      = task->extra.tmd->coords;

            if (paused != 1) {
                Actor444000_StepForward(c);
            }
        }
            task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            if (task->extra.tmd->coords->coord.t[0] >= 0x1770) {
                work->state = 0xA;
                work->phase++;
            }
            break;

        case 1:
            coord = task->extra.tmd->coords;
            if (coord->coord.t[0] < 0x2134) {
                if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
                    Actor444000_StepForward(coord);
                }
            } else {
                sc->yaw            = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]) + 0xD;
                sc->rootMatrix.mat = task->extra.tmd->coords->coord;

                gfxReadMatrixZAxis(&sc->rootMatrix.mat, &sc->offset);
                VectorNormalSS(&sc->offset, &sc->offset);
                gte_lddp(0xBEA);
                gte_ldsv(&sc->offset);
                gte_gpf12();
                gte_stsv(&sc->offset);

                sc->rootMatrix.mat.t[0] += sc->offset.vx;
                sc->rootMatrix.mat.t[1] += sc->offset.vy;
                sc->rootMatrix.mat.t[2] += sc->offset.vz;

                gfxRotMatrixY(&sc->rootMatrix.mat, sc->yaw, 1);
                task->extra.tmd->coords->coord = sc->rootMatrix.mat;

                gfxReadMatrixZAxis(&sc->rootMatrix.mat, &sc->offset);
                VectorNormalSS(&sc->offset, &sc->offset);
                gte_lddp(-0xBB8);
                gte_ldsv(&sc->offset);
                gte_gpf12();
                gte_stsv(&sc->offset);

                task->extra.tmd->coords->coord.t[0]  += sc->offset.vx;
                task->extra.tmd->coords->coord.t[1]  += sc->offset.vy;
                task->extra.tmd->coords->coord.t[2]  += sc->offset.vz;
                task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;

                if (0x800 - ABS(sc->yaw) < 0xD) {
                    sc->yaw = 0x800;
                    gfxRotMatrixY(&task->extra.tmd->coords->coord, 0x800, 1);
                    work->phase++;
                }
            }
            break;

        case 2: {
            s32       paused = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen;
            GfxCoord* c      = task->extra.tmd->coords;

            if (paused != 1) {
                Actor444000_StepForward(c);
            }
        }
            task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            if (task->extra.tmd->coords->coord.t[2] < -0x1387) {
                work->state = 0xA;
                work->phase++;
            }
            break;

        case 3: {
            s32       paused = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen;
            GfxCoord* c      = task->extra.tmd->coords;

            if (paused != 1) {
                Actor444000_StepForward(c);
            }
        }
            task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            if (task->extra.tmd->coords->coord.t[2] < -0x251B) {
                work->state = 0xA;
                work->phase++;
            }
            break;

        case 4: {
            s32       paused = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen;
            GfxCoord* c      = task->extra.tmd->coords;

            if (paused != 1) {
                Actor444000_StepForward(c);
            }
        }
            task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            if (task->extra.tmd->coords->coord.t[2] < -0x32C7) {
                work->state = 0xA;
                work->phase++;
            }
            break;

        case 5: {
            s32       paused = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen;
            GfxCoord* c      = task->extra.tmd->coords;

            if (paused != 1) {
                Actor444000_StepForward(c);
            }
        }
            task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            if (task->extra.tmd->coords->coord.t[2] < -0x4203) {
                work->state = 0x10;
                work->phase++;
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, 0, 0);
            }
            break;
    }

    SCRATCH_STACK_RELEASE_BYTES(sizeof(_Actor444000RunScratch));
}

/// Rebuild `coord`'s rotation around the yaw it already faces, left at full
/// width but scaled by `y` vertically -- the squash the death sequence retracts
/// each body with. The same shape as `_actorRenderRescaleYawXZ` below, except
/// the vertical scale arrives as an `s16`, which is what puts its sign
/// extension at the `scale.vy` store rather than at the call site. The working
/// matrix lives in a frame carved off the scratch stack, handed back once the
/// rotation has been copied onto the coordinate.
static __inline__ void Actor444000_SquashRotation(GfxCoord* coord, s16 y)
{
    ActorScaleRotScratch* sc;
    s16                   ang;

    sc                                         = (ActorScaleRotScratch*)(SCRATCH_STACK_CURSOR(u8) - sizeof(ActorScaleRotScratch));
    SCRATCH_STACK_CURSOR(ActorScaleRotScratch) = sc;

    ang     = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->yaw = ang;
    gfxRotMatrixY(&sc->rotation, ang, 1);
    sc->scale.vx = 0x1000;
    sc->scale.vy = y;
    sc->scale.vz = 0x1000;
    ScaleMatrix(&sc->rotation, &sc->scale);

    coord->coord.m[0][0] = sc->rotation.m[0][0];
    coord->coord.m[0][1] = sc->rotation.m[0][1];
    coord->coord.m[0][2] = sc->rotation.m[0][2];
    coord->coord.m[1][0] = sc->rotation.m[1][0];
    coord->coord.m[1][1] = sc->rotation.m[1][1];
    coord->coord.m[1][2] = sc->rotation.m[1][2];
    coord->coord.m[2][0] = sc->rotation.m[2][0];
    coord->coord.m[2][1] = sc->rotation.m[2][1];
    coord->coord.m[2][2] = sc->rotation.m[2][2];
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;

    SCRATCH_STACK_RELEASE_BYTES(sizeof(ActorScaleRotScratch));
}

/// State 0x12, the death sequence: the boss collapses, each of its escort
/// bodies is cut loose from the model hierarchy and then squashed flat in its
/// own window of the sub-state counter.
///
/// A reset request re-arms the block on animation 0x12, clears the enemy's link
/// state, the model's flag word and the four counters, marks the session
/// (`gGameSession::location.loc.variant` 3) and plays the death cue at half depth.
///
/// The rest of the tick splits on `ANIMATION_SLOT_SETTLED` in the second
/// animation slot: whether the collapse animation is still running or is
/// holding its boundary pose.
///
/// While it runs, `gGluttonLimbReach` is walked down 0xC8 a step until it
/// is under 0x191, four one-shot cues fire on frames 0x33, 0x3D, 0x4E and 0x71
/// of the fourth slot, and sub-states 0x14, 0x82, 0x14A and 0x1DC each hand one
/// body over: 0x14 switches the host and escort 3 to light mode 1, while the
/// other three reparent escort 2, 4 and 3's model to `gGfxViewCoord`. That
/// reparenting is why both halves of the part's placement have to be resolved
/// by hand -- `actorAccumulateToView` for the rotation it had up the
/// chain and `actorLocalToView` for its origin -- the same pair
/// `gluttonThrowSpawn` uses. Past each of those sub-states the body
/// sinks toward the host's own height 0x1E a step, clamped there, and squashes
/// from 0x1000 to nothing over 0x28 steps, throwing effect 0x60196 at one of
/// three offsets every fifth step and raising flag 0x80 on the last one.
///
/// Once the animation has finished, escort 0, escort 1 and the host model are
/// squashed over their own windows (0..0x28, 0x14..0x3C and 0xD..0x85), the
/// host throws one of five effects around itself every third step of its
/// window, escort 3 halves its height over the 0x5A steps from 0xA1 -- the step
/// that ends it also raises the arena floor's last eight grid corners -- and
/// step 0xA0 enqueues the collapse cue.
static void func_actor_444000_80135448(Task* task)
{
    GluttonWork* work;
    Enemy*       enemy;
    TmdObject*   tmd;
    MATRIX       mat;
    SVECTOR      pos;
    SVECTOR*     verts;
    s32          frame;
    s32          step;

    work  = task->work;
    enemy = task->spawnArg2.pointer;

    if (work->stateChanged != 0) {
        s32 id;
        s32 pan;

        tmd                           = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        tmd->flags                    = 0;
        work->animId                  = 0x12;
        work->neckPitchEnabled        = 0;
        work->neckYawEnabled          = 0;
        work->hostExposed             = 0;
        work->animStep                = GLUTTON_ANIM_STEP_BLEND;
        work->neckPitchTarget         = 0;

        _gluttonTickAnim(task);

        gGameSession->location.loc.variant = 3;
        id                                 = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54280007;
        pan                                = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(id, pan, (s8)(worldCoordGetOriginAudioDepth(task->extra.tmd->coords) / 2));
        return;
    }

    if (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->stateTicks = 0;
        _gluttonTickAnim(task);
    }

    if (!(work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED)) {
        if (gGluttonLimbReach >= 0x191) {
            gGluttonLimbReach = (u16)gGluttonLimbReach - 0xC8;
        }

        _gluttonTickAnim(task);

        frame = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (frame == 0x33 && work->prevSlot3Cue != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200013;
            pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(id, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        }

        frame = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (frame == 0x3D && work->prevSlot3Cue != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200003;
            pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(id, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        }

        frame = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (frame == 0x4E && work->prevSlot3Cue != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200014;
            pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(id, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        }

        frame = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (frame == 0x71 && work->prevSlot3Cue != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200015;
            pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(id, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        }

        work->prevSlot3Cue = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;

        switch (work->stateTicks) {
            case 0x14:
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                worldCoordSetActorColorMode(work->escorts[3], ENEMY_COLOR_WEIGHTED);
                break;

            case 0x82:
                actorAccumulateToView(&task->extra.tmd->coords[4], &mat);

                pos.vz = 0;
                pos.vy = 0;
                pos.vx = 0;
                actorLocalToView(&task->extra.tmd->coords[4], &pos);

                work->escorts[2]->task->extra.tmd->coords->parent       = &gGfxViewCoord;
                work->escorts[2]->task->extra.tmd->coords->coord        = mat;
                work->escorts[2]->task->extra.tmd->coords->coord.t[0]   = pos.vx;
                work->escorts[2]->task->extra.tmd->coords->coord.t[1]   = pos.vy;
                work->escorts[2]->task->extra.tmd->coords->coord.t[2]   = pos.vz;
                work->escorts[2]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                break;

            case 0x1DC:
                actorAccumulateToView(&task->extra.tmd->coords[3], &mat);

                pos.vz = 0;
                pos.vy = 0;
                pos.vx = 0;
                actorLocalToView(&task->extra.tmd->coords[3], &pos);

                work->escorts[3]->task->extra.tmd->coords->parent       = &gGfxViewCoord;
                work->escorts[3]->task->extra.tmd->coords->coord        = mat;
                work->escorts[3]->task->extra.tmd->coords->coord.t[0]   = pos.vx;
                work->escorts[3]->task->extra.tmd->coords->coord.t[1]   = pos.vy;
                work->escorts[3]->task->extra.tmd->coords->coord.t[2]   = pos.vz;
                work->escorts[3]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                break;

            case 0x14A:
                actorAccumulateToView(&task->extra.tmd->coords[4], &mat);

                pos.vz = 0;
                pos.vy = 0;
                pos.vx = 0;
                actorLocalToView(&task->extra.tmd->coords[4], &pos);

                work->escorts[4]->task->extra.tmd->coords->parent       = &gGfxViewCoord;
                work->escorts[4]->task->extra.tmd->coords->coord        = mat;
                work->escorts[4]->task->extra.tmd->coords->coord.t[0]   = pos.vx;
                work->escorts[4]->task->extra.tmd->coords->coord.t[1]   = pos.vy;
                work->escorts[4]->task->extra.tmd->coords->coord.t[2]   = pos.vz;
                work->escorts[4]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                work->limbPoseEnabled                                   = 0;
                break;
        }

        if (work->stateTicks >= 0x83) {
            if (work->escorts[2]->task->extra.tmd->coords->coord.t[1] <
                task->extra.tmd->coords->coord.t[1]) {
                work->escorts[2]->task->extra.tmd->coords->coord.t[1] +=
                    (work->stateTicks - 0x82) * 0x1E;
            } else if (task->extra.tmd->coords->coord.t[1] <
                       work->escorts[2]->task->extra.tmd->coords->coord.t[1]) {
                work->escorts[2]->task->extra.tmd->coords->coord.t[1] =
                    task->extra.tmd->coords->coord.t[1];
                effectSpawn(EFFECT_196, work->escorts[2]->task->extra.tmd->coords, 0x13401800,
                            NULL);
            }

            step = work->stateTicks - 0x82;
            if (step < 0x28) {
                Actor444000_SquashRotation(work->escorts[2]->task->extra.tmd->coords,
                                           (s16)(0x1000 - ((step * 0x1000) / 40)));
                work->escorts[2]->task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;

                if ((s16)((s16)(u16)work->stateTicks % 5) == 0) {
                    switch ((s16)((s16)((s16)(u16)work->stateTicks / 5) % 3)) {
                        case 0:
                            pos.vz = 0;
                            pos.vy = 0;
                            pos.vx = 0;
                            effectSpawn(EFFECT_196, work->escorts[2]->task->extra.tmd->coords,
                                        0x13401800, &pos);
                            break;
                        case 1:
                            pos.vx = 0x320;
                            pos.vy = 0;
                            pos.vz = -0x320;
                            effectSpawn(EFFECT_196, work->escorts[2]->task->extra.tmd->coords,
                                        0x13401800, &pos);
                            break;
                        case 2:
                            pos.vx = -0x320;
                            pos.vy = 0;
                            pos.vz = 0x320;
                            effectSpawn(EFFECT_196, work->escorts[2]->task->extra.tmd->coords,
                                        0x13401800, &pos);
                            break;
                    }
                }
            } else if (step == 0x28) {
                work->escorts[2]->task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                effectSpawn(EFFECT_196, work->escorts[2]->task->extra.tmd->coords, 0x13401800,
                            NULL);
            }
        }

        if (work->stateTicks >= 0x14B) {
            if (work->escorts[4]->task->extra.tmd->coords->coord.t[1] <
                task->extra.tmd->coords->coord.t[1]) {
                work->escorts[4]->task->extra.tmd->coords->coord.t[1] +=
                    (work->stateTicks - 0x14A) * 0x1E;
            } else if (task->extra.tmd->coords->coord.t[1] <
                       work->escorts[4]->task->extra.tmd->coords->coord.t[1]) {
                work->escorts[4]->task->extra.tmd->coords->coord.t[1] =
                    task->extra.tmd->coords->coord.t[1];
                effectSpawn(EFFECT_196, work->escorts[4]->task->extra.tmd->coords, 0x13401800,
                            NULL);
            }

            step = work->stateTicks - 0x14A;
            if (step < 0x28) {
                Actor444000_SquashRotation(work->escorts[4]->task->extra.tmd->coords,
                                           (s16)(0x1000 - ((step * 0x1000) / 40)));
                work->escorts[4]->task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;

                if ((s16)((s16)(u16)work->stateTicks % 5) == 0) {
                    switch ((s16)((s16)((s16)(u16)work->stateTicks / 5) % 3)) {
                        case 0:
                            pos.vz = 0;
                            pos.vy = 0;
                            pos.vx = 0;
                            effectSpawn(EFFECT_196, work->escorts[4]->task->extra.tmd->coords,
                                        0x13401800, &pos);
                            break;
                        case 1:
                            pos.vx = 0x320;
                            pos.vy = 0;
                            pos.vz = -0x320;
                            effectSpawn(EFFECT_196, work->escorts[4]->task->extra.tmd->coords,
                                        0x13401800, &pos);
                            break;
                        case 2:
                            pos.vx = -0x320;
                            pos.vy = 0;
                            pos.vz = 0x320;
                            effectSpawn(EFFECT_196, work->escorts[4]->task->extra.tmd->coords,
                                        0x13401800, &pos);
                            break;
                    }
                }
            } else if (step == 0x28) {
                work->escorts[4]->task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                effectSpawn(EFFECT_196, work->escorts[4]->task->extra.tmd->coords, 0x13401800,
                            NULL);
            }
        }

        if (work->stateTicks >= 0x1DD) {
            if (work->escorts[3]->task->extra.tmd->coords->coord.t[1] <
                task->extra.tmd->coords->coord.t[1]) {
                work->escorts[3]->task->extra.tmd->coords->coord.t[1] +=
                    (work->stateTicks - 0x1DC) * 0x1E;
            } else if (task->extra.tmd->coords->coord.t[1] <
                       work->escorts[3]->task->extra.tmd->coords->coord.t[1]) {
                work->escorts[3]->task->extra.tmd->coords->coord.t[1] =
                    task->extra.tmd->coords->coord.t[1];
            }

            step = work->stateTicks - 0x1DC;
            if (step < 0x28) {
                Actor444000_SquashRotation(work->escorts[3]->task->extra.tmd->coords,
                                           (s16)(0x1000 - ((step * 0x1000) / 40)));
                work->escorts[3]->task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;

                if ((s16)((s16)(u16)work->stateTicks % 5) == 0) {
                    switch ((s16)((s16)((s16)(u16)work->stateTicks / 5) % 3)) {
                        case 0:
                            pos.vz = 0;
                            pos.vy = 0;
                            pos.vx = 0;
                            effectSpawn(EFFECT_196, work->escorts[3]->task->extra.tmd->coords,
                                        0x13401800, &pos);
                            break;
                        case 1:
                            pos.vx = 0x320;
                            pos.vy = 0;
                            pos.vz = -0x320;
                            effectSpawn(EFFECT_196, work->escorts[3]->task->extra.tmd->coords,
                                        0x13401800, &pos);
                            break;
                        case 2:
                            pos.vx = -0x320;
                            pos.vy = 0;
                            pos.vz = 0x320;
                            effectSpawn(EFFECT_196, work->escorts[3]->task->extra.tmd->coords,
                                        0x13401800, &pos);
                            break;
                    }
                }
            } else if (step == 0x28) {
                work->escorts[3]->task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
        }
    } else {
        switch (work->stateTicks) {
            case 0x28:
                break;
            case 0x78:
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                break;
        }

        if (work->stateTicks > 0) {
            if (work->stateTicks < 0x28) {
                Actor444000_SquashRotation(work->escorts[0]->task->extra.tmd->coords,
                                           (s16)(0x1000 - ((work->stateTicks * 0x1000) / 40)));
                work->escorts[0]->task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            } else if (work->stateTicks == 0x28) {
                work->escorts[0]->task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
        }

        if (work->stateTicks >= 0x15) {
            step = work->stateTicks - 0x14;
            if (step < 0x28) {
                Actor444000_SquashRotation(work->escorts[1]->task->extra.tmd->coords,
                                           (s16)(0x1000 - ((step * 0x1000) / 40)));
                work->escorts[1]->task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            } else if (step == 0x28) {
                work->escorts[1]->task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
        }

        if (work->stateTicks >= 0xE) {
            step = work->stateTicks - 0xD;
            if (step < 0x78) {
                Actor444000_SquashRotation(task->extra.tmd->coords,
                                           (s16)(0x1000 - ((step * 0x1000) / 120)));
                task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            } else if (step == 0x78) {
                task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
        }

        if ((s16)((s16)(u16)work->stateTicks % 3) == 0 && (s16)(u16)work->stateTicks - 0xD < 0x78) {
            switch ((s16)((s16)((s16)(u16)work->stateTicks / 3) % 5)) {
                case 0:
                    pos.vz = 0;
                    pos.vy = 0;
                    pos.vx = 0;
                    effectSpawn(EFFECT_196, task->extra.tmd->coords, 0x14101900, &pos);
                    break;
                case 1:
                    pos.vx = 0x960;
                    pos.vy = 0;
                    pos.vz = -0x960;
                    effectSpawn(EFFECT_196, task->extra.tmd->coords, 0x13201800, &pos);
                    break;
                case 2:
                    pos.vx = -0x9C4;
                    pos.vy = 0;
                    pos.vz = 0x9C4;
                    effectSpawn(EFFECT_196, task->extra.tmd->coords, 0x131C1800, &pos);
                    break;
                case 3:
                    pos.vx = -0x6A4;
                    pos.vy = 0;
                    pos.vz = 0x640;
                    effectSpawn(EFFECT_196, task->extra.tmd->coords, 0x14101800, &pos);
                    break;
                case 4:
                    pos.vx = 0x6A4;
                    pos.vy = 0;
                    pos.vz = -0x640;
                    effectSpawn(EFFECT_196, task->extra.tmd->coords, 0x14301800, &pos);
                    break;
            }
        }

        if (work->stateTicks >= 0xA1) {
            step = work->stateTicks - 0xA0;
            if (step < 0x5A) {
                Actor444000_SquashRotation(work->escorts[3]->task->extra.tmd->coords,
                                           (s16)(0x800 - ((step * 0x800) / 90)));
            } else if (step == 0x5A) {
                task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;

                verts        = Gp_GridParams->vertices;
                verts[24].vy = 0x1F4;
                verts[25].vy = 0x1F4;
                verts[26].vy = 0x320;
                verts[27].vy = 0x320;
                verts[28].vy = 0x1F4;
                verts[29].vy = 0x1F4;
                verts[30].vy = 0x320;
                verts[31].vy = 0x320;
            }
        }

        if (work->stateTicks == 0xA0) {
            sndEvtRequestScriptStop((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR, 7), SOUND_SCRIPT_STOP_KEEP_RELEASE);
        }
    }
}

static void func_actor_444000_801371E8(Task* task, s32 scale, s16 face)
{
    SVECTOR                 dir;
    SVECTOR*                norms   = Gp_GridParams->normals;
    SVECTOR*                corners = Gp_GridParams->vertices;
    WorldCollisionGridFace* faces   = Gp_GridParams->faces;
    WorldCollisionGridFace  quad0   = { { face * 4, face * 4 + 1, face * 4 + 2, face * 4 + 3 }, face, 2 };
    WorldCollisionGridFace  quad1   = {
        { (face + 1) * 4, (face + 1) * 4 + 1, (face + 1) * 4 + 2, (face + 1) * 4 + 3 }, face + 1, 2
    };
    SVECTOR* d;

    gfxReadMatrixZAxis(&task->extra.tmd->coords->coord, &dir);
    d = &dir;
    VectorNormalSS(d, d);
    gte_lddp(scale);
    gte_ldsv(d);
    gte_gpf12();
    gte_stsv(d);

    corners[face * 4].vx = corners[face * 4 + 2].vx = 0x2CEC;
    corners[face * 4].vy = corners[face * 4 + 2].vy = (u16)task->extra.tmd->coords->coord.t[1];
    corners[face * 4].vz = corners[face * 4 + 2].vz = -0x1B58;
    corners[face * 4 + 1].vx                        = corners[face * 4 + 3].vx =
        (u16)task->extra.tmd->coords->coord.t[0] + (u16)dir.vx;
    corners[face * 4 + 1].vy = corners[face * 4 + 3].vy =
        (u16)task->extra.tmd->coords->coord.t[1] + (u16)dir.vy;
    corners[face * 4 + 1].vz = corners[face * 4 + 3].vz =
        (u16)task->extra.tmd->coords->coord.t[2] + (u16)dir.vz;
    corners[face * 4].vy     = (u16)corners[face * 4].vy - 0x190;
    corners[face * 4 + 1].vy = (u16)corners[face * 4 + 1].vy - 0x190;
    faces[face]              = quad0;

    norms[face].vz = (u16)corners[face * 4].vx - (u16)corners[face * 4 + 1].vx;
    norms[face].vy = (u16)corners[face * 4 + 1].vy - (u16)corners[face * 4].vy;
    norms[face].vx = (u16)corners[face * 4 + 1].vz - (u16)corners[face * 4].vz;
    VectorNormalSS(&norms[face], &norms[face]);

    corners[face * 4 + 4].vx = corners[face * 4 + 6].vx =
        (u16)task->extra.tmd->coords->coord.t[0] + (u16)dir.vx;
    corners[face * 4 + 4].vy = corners[face * 4 + 6].vy =
        (u16)task->extra.tmd->coords->coord.t[1] + (u16)dir.vy;
    corners[face * 4 + 4].vz = corners[face * 4 + 6].vz =
        (u16)task->extra.tmd->coords->coord.t[2] + (u16)dir.vz;
    corners[face * 4 + 5].vx = corners[face * 4 + 7].vx =
        (u16)task->extra.tmd->coords->coord.t[0] + (u16)dir.vx + 0x1B58;
    corners[face * 4 + 5].vy = corners[face * 4 + 7].vy =
        (u16)task->extra.tmd->coords->coord.t[1] + (u16)dir.vy;
    corners[face * 4 + 5].vz = corners[face * 4 + 7].vz =
        (u16)task->extra.tmd->coords->coord.t[2] + (u16)dir.vz;
    corners[face * 4 + 4].vy = (u16)corners[face * 4 + 4].vy - 0x190;
    corners[face * 4 + 5].vy = (u16)corners[face * 4 + 5].vy - 0x190;
    faces[face + 1]          = quad1;

    norms[face + 1].vx = 0;
    norms[face + 1].vy = 0;
    norms[face + 1].vz = -0x1000;
}

#include "../../shared/glutton_throw_spawn.inc.c"

#include "../../shared/glutton_throw_fly.inc.c"

#include "../../shared/glutton_inlines.inc.c"

#include "../../shared/glutton_glob_spawn.inc.c"

#include "../../shared/glutton_glob_fall.inc.c"

#include "../../shared/glutton_glob_engulf.inc.c"

#include "../../shared/glutton_glob_hold.inc.c"

#include "../../shared/glutton_chunk_spawn.inc.c"

#include "../../shared/glutton_chunk_fall.inc.c"

/// State handlers of the textured prop sub-task: setup, per-frame refresh and
/// teardown.
static const EnemyTaskFuncTable3 gGluttonPropStates = {
    {
        gluttonPropSetup,
        _gluttonPropTick,
        enemyDestroy,
    },
};

/// State handlers of the enemy `gluttonThrowTask` dispatches.
static const EnemyTaskFuncTable3 gGluttonThrowStates = {
    {
        gluttonThrowSpawn,
        _gluttonThrowFly,
        enemyDestroy,
    },
};

/// State handlers of the grab enemy, by state: setup, bounce, rise, hold and
/// teardown.
static const EnemyTaskFuncTable5 gGluttonGlobStates = {
    {
        gluttonGlobSpawn,
        _gluttonGlobFall,
        gluttonGlobEngulf,
        _gluttonGlobHold,
        enemyDestroy,
    },
};

#include "../../shared/glutton_chunk_settle.inc.c"

#include "../../shared/glutton_rain_spawn.inc.c"

#include "../../shared/glutton_rain_rise.inc.c"

#include "../../shared/glutton_rain_fall.inc.c"

/// State handlers of the enemy `gluttonChunkTask` dispatches: spawn,
/// descent, settle and teardown.
static const EnemyTaskFuncTable4 gGluttonChunkStates = {
    {
        gluttonChunkSpawn,
        _gluttonChunkFall,
        _gluttonChunkSettle,
        enemyDestroy,
    },
};

/// State handlers of the dropped enemy: spawn, ascent, descent, landing and
/// teardown.
static const EnemyTaskFuncTable5 gGluttonRainStates = {
    {
        gluttonRainSpawn,
        _gluttonRainRise,
        _gluttonRainFall,
        gluttonRainSplat,
        enemyDestroy,
    },
};

/// State handlers of the spinner enemy: spawn, hidden wait, chase and teardown.
static const EnemyTaskFuncTable4 gGluttonSpinnerStates = {
    {
        _gluttonSpinnerSpawn,
        _gluttonSpinnerWait,
        gluttonSpinnerChase,
        enemyDestroy,
    },
};

#include "../../shared/glutton_rain_splat.inc.c"

#include "../../shared/glutton_spinner_spawn.inc.c"

#include "../../shared/glutton_spinner_chase.inc.c"

#include "../../shared/glutton_shake_tick.inc.c"

/// Message 0x7D5 handler, the visibility control the event task drives the
/// boss with: each sub-command sets the host model's flag word and pushes it
/// onto all seven escorts' models, differing in what the flag word becomes and
/// whether the model buffers are (re)allocated first.
///
/// 0 brings the group back with buffers and the 0x80 flag, 1 clears the flag
/// before making sure the buffers exist, 2 sets `TMD_OBJECT_SKIP_AUTO_BUFFER`
/// and, finding that bit set, stores 3 in `freeCountdown` and replaces the flag
/// word with `TMD_OBJECT_SKIP_ACTIVE_DRAW`. 3 clears the word, pushes the
/// clear, then sets `TMD_OBJECT_SKIP_AUTO_BUFFER` on the host alone. Cases 0
/// and 2 also reset `state`. Case 2 tests the bit it just set, so that test
/// is always true.
s32 func_actor_444000_8013A958(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    TmdObject*   tmd;
    GluttonWork* work;
    GluttonWork* escorts;
    GluttonWork* buffers;
    GluttonWork* rebuilt;
    TmdObject*   hostTmd;
    TmdObject*   escortTmd;
    s32          flags;
    s16          i;
    s16          j;

    tmd  = task->extra.tmd;
    work = task->work;
    switch (arg2) {
        case 0:
            buffers = task->work;
            if (tmd->buffer == NULL) {
                tmdAllocPrimitiveBuffer(tmd);
            }
            for (j = 0; j < ARRAY_SIZE(buffers->escorts); j++) {
                if (buffers->escorts[j] != NULL) {
                    escortTmd = buffers->escorts[j]->task->extra.tmd;
                    if (escortTmd->buffer == NULL) {
                        tmdAllocPrimitiveBuffer(escortTmd);
                    }
                }
            }
            escorts                = task->work;
            escorts->freeCountdown = 0;
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            for (i = 0; i < ARRAY_SIZE(escorts->escorts); i++) {
                if (escorts->escorts[i] != NULL) {
                    escorts->escorts[i]->task->extra.tmd->flags = task->extra.tmd->flags;
                }
            }
            work->state = 0;
            break;
        case 1:
            escorts                = task->work;
            escorts->freeCountdown = 0;
            task->extra.tmd->flags = 0;
            for (i = 0; i < ARRAY_SIZE(escorts->escorts); i++) {
                if (escorts->escorts[i] != NULL) {
                    escorts->escorts[i]->task->extra.tmd->flags = task->extra.tmd->flags;
                }
            }
            hostTmd = task->extra.tmd;
            rebuilt = task->work;
            if (hostTmd->buffer == NULL) {
                tmdAllocPrimitiveBuffer(hostTmd);
            }
            for (j = 0; j < ARRAY_SIZE(rebuilt->escorts); j++) {
                if (rebuilt->escorts[j] != NULL) {
                    escortTmd = rebuilt->escorts[j]->task->extra.tmd;
                    if (escortTmd->buffer == NULL) {
                        tmdAllocPrimitiveBuffer(escortTmd);
                    }
                }
            }
            break;
        case 2:
            tmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            flags       = tmd->flags;
            escorts     = task->work;
            if (flags & TMD_OBJECT_SKIP_AUTO_BUFFER) {
                escorts->freeCountdown = 3;
                task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            } else {
                escorts->freeCountdown = 0;
                task->extra.tmd->flags = flags;
            }
            for (i = 0; i < ARRAY_SIZE(escorts->escorts); i++) {
                if (escorts->escorts[i] != NULL) {
                    escorts->escorts[i]->task->extra.tmd->flags = task->extra.tmd->flags;
                }
            }
            work->state = 0;
            break;
        case 3:
            tmd->flags             = 0;
            escorts                = task->work;
            escorts->freeCountdown = 0;
            task->extra.tmd->flags = 0;
            for (i = 0; i < ARRAY_SIZE(escorts->escorts); i++) {
                if (escorts->escorts[i] != NULL) {
                    escorts->escorts[i]->task->extra.tmd->flags = task->extra.tmd->flags;
                }
            }
            tmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
    }
    return 0;
}

/// Rebuilds the host's root coordinate around the yaw it is already facing:
/// `ratan2` of the rotation's Z basis gives the yaw, `gfxRotMatrixY` rebuilds
/// the rotation from it, and `ScaleMatrix` widens it to 1.0 / 0.0 / 1.0 so the
/// model flattens vertically. The working matrix lives in a frame carved off
/// the scratch stack, which is handed back before the coordinate is refreshed.
static __inline__ void Actor444000_RebuildRotation(Task* task)
{
    GfxCoord*             coord = task->extra.tmd->coords;
    ActorScaleRotScratch* sc;
    s16                   ang;

    sc                                         = (ActorScaleRotScratch*)(SCRATCH_STACK_CURSOR(u8) - sizeof(ActorScaleRotScratch));
    SCRATCH_STACK_CURSOR(ActorScaleRotScratch) = sc;

    ang     = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->yaw = ang;
    gfxRotMatrixY(&sc->rotation, ang, 1);
    sc->scale.vx = 0x1000;
    sc->scale.vy = 0;
    sc->scale.vz = 0x1000;
    ScaleMatrix(&sc->rotation, &sc->scale);

    coord->coord.m[0][0] = sc->rotation.m[0][0];
    coord->coord.m[0][1] = sc->rotation.m[0][1];
    coord->coord.m[0][2] = sc->rotation.m[0][2];
    coord->coord.m[1][0] = sc->rotation.m[1][0];
    coord->coord.m[1][1] = sc->rotation.m[1][1];
    coord->coord.m[1][2] = sc->rotation.m[1][2];
    coord->coord.m[2][0] = sc->rotation.m[2][0];
    coord->coord.m[2][1] = sc->rotation.m[2][1];
    coord->coord.m[2][2] = sc->rotation.m[2][2];
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;

    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BYTES(sizeof(ActorScaleRotScratch));
    actorRenderComposeCoord(task->extra.tmd->coords);
}

/// The enemy task's 0x7DB message handler, listed in `D_actor_444000_80161818`.
/// The three payload bytes are always recorded in the work block; only messages
/// from sender 0x2804 act, and then on three of the selector's values. 0 and 1
/// both announce the state change with the same pair of cues, 1 additionally
/// re-arms the animation blocks and drops the model onto its start position,
/// and 19 switches the host and its fourth escort to light mode 2 before
/// raising eight floor vertices and flattening the model's rotation.
s32 func_actor_444000_8013ACD0(Task* task, s32 msgId, ActorCommand* msg, s32 arg3)
{
    GluttonWork* work  = task->work;
    Enemy*       enemy = task->spawnArg2.pointer;
    SVECTOR*     verts;
    s32          action;

    work->lastCommandStage = msg->context.loc.stage;
    work->lastCommandArea  = msg->context.loc.area;
    work->lastCommand      = (u8)msg->command;

    if (msg->context.key == 0x2804) {
        action = msg->command;
        switch (action) {
            case 0:
                work->state = 0;
                sndEvtRequestScriptStop(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0A), SOUND_SCRIPT_STOP_KEEP_RELEASE);
                sndEvtRequestScriptStop(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0D), SOUND_SCRIPT_STOP_KEEP_RELEASE);
                break;

            case 1:
                work->animId   = 0xA;
                work->animStep = GLUTTON_ANIM_STEP_RESTART;
                _gluttonTickAnim(task);
                _gluttonTickAnim(task);
                _gluttonTickAnim(task);
                work->animRate = 1;
                _gluttonTickAnim(task);
                work->animRate                        = 0x10;
                task->extra.tmd->coords->coord.t[0]   = -0xBB8;
                task->extra.tmd->coords->coord.t[1]   = 0;
                task->extra.tmd->coords->coord.t[2]   = -0x992;
                task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                work->state                           = 0x11;
                sndEvtRequestScriptStop(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0A), SOUND_SCRIPT_STOP_KEEP_RELEASE);
                sndEvtRequestScriptStop(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0D), SOUND_SCRIPT_STOP_KEEP_RELEASE);
                break;

            case 19:
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                worldCoordSetActorColorMode(work->escorts[3], ENEMY_COLOR_BLACK);
                work->state      = action;
                work->prevState  = -1;
                work->viewLocked = 1;
                sndEvtRequestScriptStop(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR, 7), SOUND_SCRIPT_STOP_KEEP_RELEASE);

                verts        = Gp_GridParams->vertices;
                verts[24].vy = 0x1F4;
                verts[25].vy = 0x1F4;
                verts[26].vy = 0x320;
                verts[27].vy = 0x320;
                verts[28].vy = 0x1F4;
                verts[29].vy = 0x1F4;
                verts[30].vy = 0x320;
                verts[31].vy = 0x320;

                Actor444000_RebuildRotation(task);
                break;
        }
    }
    return 1;
}

/// Rebuilds the host's root coordinate from its own facing yaw with a uniform
/// 1.0 scale, marks the model for a rebuild and arms the first state. The
/// matrix lives in a frame taken off the scratch stack, which is handed back
/// once the rotation has been copied out; inlined so each scratch-head access
/// keeps its own `lui` instead of sharing a CSE'd register.
static __inline__ void Actor444000_SeedRootCoord(Task* task, GluttonWork* work)
{
    GfxCoord*             coord = task->extra.tmd->coords;
    ActorScaleRotScratch* sc;
    s16                   ang;

    sc                                         = (ActorScaleRotScratch*)(SCRATCH_STACK_CURSOR(u8) - sizeof(ActorScaleRotScratch));
    SCRATCH_STACK_CURSOR(ActorScaleRotScratch) = sc;

    ang     = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->yaw = ang;
    gfxRotMatrixY(&sc->rotation, ang, 1);
    sc->scale.vx = sc->scale.vy = sc->scale.vz = 0x1000;
    ScaleMatrix(&sc->rotation, &sc->scale);

    coord->coord.m[0][0] = sc->rotation.m[0][0];
    coord->coord.m[0][1] = sc->rotation.m[0][1];
    coord->coord.m[0][2] = sc->rotation.m[0][2];
    coord->coord.m[1][0] = sc->rotation.m[1][0];
    coord->coord.m[1][1] = sc->rotation.m[1][1];
    coord->coord.m[1][2] = sc->rotation.m[1][2];
    coord->coord.m[2][0] = sc->rotation.m[2][0];
    coord->coord.m[2][1] = sc->rotation.m[2][1];
    coord->coord.m[2][2] = sc->rotation.m[2][2];
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;

    work->state            = 1;
    task->extra.tmd->flags = 0;
    SCRATCH_STACK_RELEASE_BYTES(sizeof(ActorScaleRotScratch));
}

/// Spawn state of the arena boss: allocate its `GluttonWork`, wire the host
/// enemy up to the model's root coordinate and its nine collision objects, then
/// spawn the seven escorts that make up the rest of the creature.
///
/// The host takes collision groups 0..2 (group 0's record table doubles as its
/// own `Enemy::recs`), escort 0 takes 3..5 and escort 1 takes 6..8; each
/// group is armed through `func_8010C980` on one of the model's part
/// coordinates. Escorts 2..5 are parented to a part coordinate at a fixed
/// offset and nothing else, and escort 6 is only spawned when the high half of
/// `Task::spawnArg1` is clear. Escort 3 is the one the colour updates treat as
/// the host's twin, so it shares the host's group-1 record table.
///
/// The root coordinate is flattened to its own yaw with a uniform 1.0 scale
/// (`Actor444000_SeedRootCoord`), the swipe capsule `swipeBody` is linked
/// by hand around the free coordinate `swipeCoord`, and both the host and every
/// escort model are pointed at the work block's light and colour matrices
/// before the fight announces itself with message 0x7DA.
static void func_actor_444000_8013AFF8(Enemy* enemy, Task* task)
{
    GluttonWork* work;
    GluttonWork* buffers;
    GluttonWork* escorts;
    GfxMatrix*   mtx;
    TmdObject*   tmd;
    TmdObject*   model;
    TmdObject*   escortTmd;
    GfxCoord*    coord;
    GfxCoord*    freeCoord;
    Enemy*       esc;
    Task*        escTask;
    SVECTOR      dir;
    SVECTOR*     gteDir;
    VECTOR       pos;
    s16          i;
    s16          j;
    s16          k;

    tmd   = task->extra.tmd;
    coord = tmd->coords;

    work       = memCalloc(sizeof(GluttonWork), 0);
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
    enemy->hp            = D_actor_444000_80144A28.hpMax;
    enemy->param         = &D_actor_444000_80144A28;
    enemy->recs          = work->hits[0].contacts;

    animationInitContext(&work->hostRig.anim, D_actor_444000_80161448, tmd, work->hostRig.poses, work->hostRig.slots);
    animationInitContext(&work->hostBlendRig.anim, D_actor_444000_80161448, tmd, work->hostBlendRig.poses, work->hostBlendRig.slots);

    work->animStep        = GLUTTON_ANIM_STEP_RESTART;
    work->animId          = 2;
    work->limbPoseEnabled = 1;
    work->blending        = 0;
    work->neckYawTarget = work->neckYaw = 0;
    work->animRate = work->field_7B8 = 0x10;

    func_8010C980(&task->extra.tmd->coords[4], &work->hits[0].body, work->hits[0].contacts, ARRAY_SIZE(work->hits[0].contacts), 0x20, 0x300);
    func_8010C980(&task->extra.tmd->coords[4], &work->hits[1].body, work->hits[1].contacts, ARRAY_SIZE(work->hits[1].contacts), 0x20, 0x300);
    func_8010C980(&task->extra.tmd->coords[1], &work->hits[2].body, work->hits[2].contacts, ARRAY_SIZE(work->hits[2].contacts), 0x20, 0xBB8);

    work->hits[1].body.pos.vx = 0;
    work->hits[1].body.pos.vy = 0;
    work->hits[1].body.pos.vz = -0x100;
    work->hits[2].body.pos.vx = 0;
    work->hits[2].body.pos.vy = 0x400;
    work->hits[2].body.pos.vz = -0x400;

    gfxReadMatrixZAxis(&task->extra.tmd->coords->coord, &dir);
    dir.vy = 0;
    gteDir = &dir;
    VectorNormalSS(gteDir, gteDir);
    gte_lddp(0x1388);
    gte_ldsv(gteDir);
    gte_gpf12();
    gte_stsv(gteDir);

    work->playerAnim.source.sets          = NULL;
    work->playerAnim.animationId          = 1;
    work->playerAnim.blend                = ANIMATION_BLEND_RESET;
    work->playerAnim.blendFrames          = 3;
    work->playerAnim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
    work->deathTicks                      = 0;
    task->msgTable                        = D_actor_444000_80161818;
    coord->parent                         = &gGfxViewCoord;
    coord->composeStamp                   = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);

    D_actor_444000_80161880.coord      = task->extra.tmd->coords;
    D_actor_444000_80161880.spawnArgLo = 0x100;
    D_actor_444000_80161880.spawnArgHi = 2;
    work->prevState                    = -1;

    model   = task->extra.tmd;
    buffers = task->work;
    if (model->buffer == NULL) {
        tmdAllocPrimitiveBuffer(model);
    }
    for (i = 0; i < ARRAY_SIZE(buffers->escorts); i++) {
        if (buffers->escorts[i] != NULL) {
            escortTmd = buffers->escorts[i]->task->extra.tmd;
            if (escortTmd->buffer == NULL) {
                tmdAllocPrimitiveBuffer(escortTmd);
            }
        }
    }

    Actor444000_SeedRootCoord(task, work);

    esc                                                   = enemySpawnFromTable(D_actor_444000_801616B0, 0, 0, task->spawnArg2.pointer);
    work->escorts[0]                                      = esc;
    esc->task->extra.tmd->coords->parent                  = task->extra.tmd->coords;
    work->escorts[0]->task->extra.tmd->coords->coord.t[0] = 0;
    work->escorts[0]->task->extra.tmd->coords->coord.t[1] = 0;
    work->escorts[0]->task->extra.tmd->coords->coord.t[2] = 0;
    work->escorts[0]->task->extra.tmd->flags              = 0;
    animationInitContext(&work->escort0Rig.anim, D_actor_444000_80161500, work->escorts[0]->task->extra.tmd, work->escort0Rig.poses,
                         work->escort0Rig.slots);
    animationInitContext(&work->escort0BlendRig.anim, D_actor_444000_80161500, work->escorts[0]->task->extra.tmd, work->escort0BlendRig.poses,
                         work->escort0BlendRig.slots);
    work->escorts[0]->field_4    = &task->extra.tmd->coords->coord;
    work->escorts[0]->field_48   = 0;
    work->escorts[0]->bodyPos.vx = 0xC8;
    work->escorts[0]->bodyPos.vy = 0;
    work->escorts[0]->bodyPos.vz = 0x3E8;
    work->escorts[0]->coord      = &work->escorts[0]->task->extra.tmd->coords[1];
    worldTargetLinkNode(&work->escorts[0]->node);
    work->escorts[0]->reactionFlags = 0;
    work->escorts[0]->hp            = D_actor_444000_80144A28.hpMax;
    work->groups3To5Pool            = D_actor_444000_80144A38.hpMax;
    work->escorts[0]->param         = &D_actor_444000_80144A38;
    work->escorts[0]->recs          = work->hits[3].contacts;
    func_8010C980(&work->escorts[0]->task->extra.tmd->coords[1], &work->hits[3].body, work->hits[3].contacts, ARRAY_SIZE(work->hits[3].contacts),
                  0x20, 0x300);
    func_8010C980(&work->escorts[0]->task->extra.tmd->coords[2], &work->hits[4].body, work->hits[4].contacts, ARRAY_SIZE(work->hits[4].contacts),
                  0x20, 0x300);
    func_8010C980(&work->escorts[0]->task->extra.tmd->coords[3], &work->hits[5].body, work->hits[5].contacts, ARRAY_SIZE(work->hits[5].contacts),
                  0x20, 0x300);

    esc                                                   = enemySpawnFromTable(D_actor_444000_801616B0, 1, 0, task->spawnArg2.pointer);
    work->escorts[1]                                      = esc;
    esc->task->extra.tmd->coords->parent                  = task->extra.tmd->coords;
    work->escorts[1]->task->extra.tmd->coords->coord.t[0] = 0;
    work->escorts[1]->task->extra.tmd->coords->coord.t[1] = 0;
    work->escorts[1]->task->extra.tmd->coords->coord.t[2] = 0;
    work->escorts[1]->task->extra.tmd->flags              = 0;
    animationInitContext(&work->escort1Rig.anim, D_actor_444000_801615B8, work->escorts[1]->task->extra.tmd, work->escort1Rig.poses,
                         work->escort1Rig.slots);
    animationInitContext(&work->escort1BlendRig.anim, D_actor_444000_801615B8, work->escorts[1]->task->extra.tmd, work->escort1BlendRig.poses,
                         work->escort1BlendRig.slots);
    work->escorts[1]->field_4    = &task->extra.tmd->coords->coord;
    work->escorts[1]->field_48   = 0;
    work->escorts[1]->bodyPos.vx = -0xC8;
    work->escorts[1]->bodyPos.vy = 0;
    work->escorts[1]->bodyPos.vz = 0x3E8;
    work->escorts[1]->coord      = &work->escorts[1]->task->extra.tmd->coords[1];
    worldTargetLinkNode(&work->escorts[1]->node);
    work->escorts[1]->reactionFlags = 0;
    work->escorts[1]->hp            = D_actor_444000_80144A28.hpMax;
    work->groups6To8Pool            = D_actor_444000_80144A48.hpMax;
    work->escorts[1]->param         = &D_actor_444000_80144A48;
    work->escorts[1]->recs          = work->hits[6].contacts;
    func_8010C980(&work->escorts[1]->task->extra.tmd->coords[1], &work->hits[6].body, work->hits[6].contacts, ARRAY_SIZE(work->hits[6].contacts),
                  0x20, 0x300);
    func_8010C980(&work->escorts[1]->task->extra.tmd->coords[2], &work->hits[7].body, work->hits[7].contacts, ARRAY_SIZE(work->hits[7].contacts),
                  0x20, 0x300);
    func_8010C980(&work->escorts[1]->task->extra.tmd->coords[3], &work->hits[8].body, work->hits[8].contacts, ARRAY_SIZE(work->hits[8].contacts),
                  0x20, 0x300);

    esc                                                   = enemySpawnFromTable(D_actor_444000_801616B0, 2, 0, task->spawnArg2.pointer);
    work->escorts[2]                                      = esc;
    esc->task->extra.tmd->coords->parent                  = &task->extra.tmd->coords[4];
    work->escorts[2]->task->extra.tmd->coords->coord.t[0] = 0;
    work->escorts[2]->task->extra.tmd->coords->coord.t[1] = 0x59;
    work->escorts[2]->task->extra.tmd->coords->coord.t[2] = -0x64;
    work->escorts[2]->task->extra.tmd->flags              = 0;

    esc                                                   = enemySpawnFromTable(D_actor_444000_801616B0, 3, 0, task->spawnArg2.pointer);
    work->escorts[3]                                      = esc;
    esc->task->extra.tmd->coords->parent                  = &task->extra.tmd->coords[3];
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
    work->escorts[3]->hp            = D_actor_444000_80144A28.hpMax;
    work->groups1To2Pool            = D_actor_444000_80144A58.hpMax;
    work->escorts[3]->param         = &D_actor_444000_80144A58;
    work->escorts[3]->recs          = work->hits[1].contacts;

    freeCoord                                          = &work->swipeCoord.node;
    work->swipeCoord.node.parent                       = task->extra.tmd->coords;
    work->swipeCoord.packed.coord.rotationWords.m00M01 = ONE;
    mtx                                                = &work->swipeCoord.packed.coord;
    mtx->rotationWords.m02M10                          = 0;
    mtx->rotationWords.m11M12                          = ONE;
    mtx->rotationWords.m20M21                          = 0;
    mtx->rotationWords.m22                             = ONE;
    work->swipeCoord.node.coord.t[0] = work->swipeCoord.node.coord.t[1] = work->swipeCoord.node.coord.t[2] = 0;
    work->swipeCoord.node.composeStamp                                                                     = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(freeCoord);

    work->swipeCapsule.ends[1].vz   = 0x1B58;
    work->swipeCapsule.end0Radius   = 0x258;
    work->swipeCapsule.end1Radius   = 0x258;
    work->swipeCapsule.ends[0].vx   = 0;
    work->swipeCapsule.ends[0].vy   = 0;
    work->swipeCapsule.ends[0].vz   = 0;
    work->swipeCapsule.ends[1].vx   = 0;
    work->swipeCapsule.ends[1].vy   = 0;
    work->swipeCapsule.contacts     = work->swipeContacts;
    work->swipeBody.coord           = freeCoord;
    work->swipeBody.context.capsule = &work->swipeCapsule;
    work->swipeBody.pos.vx          = 0;
    work->swipeBody.pos.vy          = -0xFA;
    work->swipeBody.pos.vz          = 0x25F;
    work->swipeBody.key             = 0x30000 | 0x20;
    work->swipeBody.radius          = 0;
    work->swipeBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->swipeBody);
    worldCollisionInitContacts(work->swipeContacts, ARRAY_SIZE(work->swipeContacts), 0);
    work->swipeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    esc                                                   = enemySpawnFromTable(D_actor_444000_801616B0, 4, 0, task->spawnArg2.pointer);
    work->escorts[4]                                      = esc;
    esc->task->extra.tmd->coords->parent                  = &task->extra.tmd->coords[4];
    work->escorts[4]->task->extra.tmd->coords->coord.t[0] = 0;
    work->escorts[4]->task->extra.tmd->coords->coord.t[1] = 0;
    work->escorts[4]->task->extra.tmd->coords->coord.t[2] = 0x14;
    work->escorts[4]->task->extra.tmd->flags              = 0;

    esc                                                   = enemySpawnFromTable(D_actor_444000_801616B0, 5, 0, task->spawnArg2.pointer);
    work->escorts[5]                                      = esc;
    esc->task->extra.tmd->coords->parent                  = &task->extra.tmd->coords[2];
    work->escorts[5]->task->extra.tmd->coords->coord.t[0] = 0;
    work->escorts[5]->task->extra.tmd->coords->coord.t[1] = 0x67C;
    work->escorts[5]->task->extra.tmd->coords->coord.t[2] = 0xC8;
    work->escorts[5]->task->extra.tmd->flags              = 0;

    if ((task->spawnArg1.value >> 16) == 0) {
        esc                                                   = enemySpawnFromTable(D_actor_444000_801616B0, 6, 0, task->spawnArg2.pointer);
        work->escorts[6]                                      = esc;
        esc->task->extra.tmd->coords->parent                  = &task->extra.tmd->coords[1];
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
    work->groups6To8Pool = (s16)D_actor_444000_80144A48.hpMax;
    work->groups3To5Pool = (s16)D_actor_444000_80144A38.hpMax;

    escorts = task->work;

    gGluttonEnded = 0;

    task->extra.tmd->lightMtx = &escorts->lightMtx;
    task->extra.tmd->colorMtx = &escorts->colorMtx;
    for (j = 0; j < ARRAY_SIZE(escorts->escorts); j++) {
        esc = escorts->escorts[j];
        if (esc != NULL) {
            escTask                      = esc->task;
            escTask->extra.tmd->lightMtx = &escorts->lightMtx;
            escTask->extra.tmd->colorMtx = &escorts->colorMtx;
        }
    }

    actorRenderComposeCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    if (work->hostExposed != work->prevHostExposed) {
        worldCoordUpdateActorColor(enemy, &pos, 0, 0);
        worldCoordUpdateActorColor(work->escorts[3], &pos, 0, 0);
        work->prevHostExposed = work->hostExposed;
    }
    if (work->hostExposed != 0) {
        worldCoordUpdateActorColor(enemy, &pos, 0, 0);
    } else {
        worldCoordUpdateActorColor(work->escorts[3], &pos, 0, 0);
    }
    _gluttonTickAnim(task);

    D_actor_444000_80161888.command.context.loc.stage = 0;
    D_actor_444000_80161888.command.context.loc.area  = 0x2C;
    D_actor_444000_80161888.command.command           = 0;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_actor_444000_80161888.command, ACTOR_COMMAND_MESSAGE_APPLY);

    work->wallDistance = work->wallDistanceTarget = 0xFA0;
    for (k = 0; k < 2; k++) {
        work->summons[k] = NULL;
    }

    gSceneCombatState.battleRefs = 0xA;
    sceneReleaseBattleRefWithRewards(task, 0x20);
    _gGluttonHostTask.task = task;
    work->summonsSpawned = work->summonsAlive = 0;
    task->state                              += 1;
}

#include "../../shared/glutton_hit_group0.inc.c"

#include "../../shared/glutton_hit_groups1to2.inc.c"

/// The hit handler for collision groups 3, 4 and 5 -- `_gluttonHitGroups1To2`
/// done three times, the next group only scanned when the previous one landed
/// nothing and the part it hit reported no attack id back. Unlike groups 1 and 2
/// this one runs no `damageGetPlayerAttackReaction` switch: the call is made and its kind
/// thrown away, so every hit is treated alike.
///
/// The damage is the distance-scaled hit quadrupled when `damageRollCriticalHit`
/// fires, then divided by six (never down to zero unless it already was), and
/// comes off the host, the first escort and `groups3To5Pool`. Emptying that pool
/// spawns the same effect again and refills it from
/// `D_actor_444000_80144A38.hpMax`. Both effect spawns and the state change to 0xE
/// are skipped while the boss is in one of the seven states that ignore hits,
/// while `phase` is clear, or while the player hold is armed.
static void func_actor_444000_8013CA60(Task* task)
{
    GluttonHitScratch*     sc;
    GluttonWork*           work;
    Enemy*                 host;
    PlayerStatus*          cfg;
    GfxCoord*              coord;
    WorldCollisionContact* recs;
    WorldCollisionContact* recs2;
    WorldCollisionContact* recs3;
    SVECTOR*               pos;
    SVECTOR*               pos2;
    SVECTOR*               pos3;
    s32                    id;
    s32                    dx2;
    s32                    dy2;
    s32                    dz2;
    u32                    dmg;
    s16                    angle;
    s16                    state;
    s16                    i;
    s16                    i2;
    s16                    i3;

    cfg  = &gPlayerStatus;
    host = task->spawnArg2.pointer;
    work = task->work;
    sc   = SCRATCH_STACK_RESERVE_BLOCK(GluttonHitScratch);
    pos  = &sc->contactPoint;
    recs = work->hits[3].contacts;
    for (i = 0; i < ARRAY_SIZE(work->hits[3].contacts); i++) {
        if (recs[i].key.value == 0) {
            goto missed1;
        }
        if ((recs[i].key.value & 0xFFFF0000) == 0x20000) {
            pos->vx = recs[i].point.vx;
            pos->vy = recs[i].point.vy;
            pos->vz = recs[i].point.vz;
            id      = recs[i].key.value;
            goto found1;
        }
    }
missed1:
    id = 0;
found1:
    sc->attackKey = id;
    if (id != 0) {
        coord = work->hits[3].body.coord;
        goto hit;
    }

    pos2  = &sc->contactPoint;
    recs2 = work->hits[4].contacts;
    for (i2 = 0; i2 < ARRAY_SIZE(work->hits[4].contacts); i2++) {
        if (recs2[i2].key.value == 0) {
            goto missed2;
        }
        if ((recs2[i2].key.value & 0xFFFF0000) == 0x20000) {
            pos2->vx = recs2[i2].point.vx;
            pos2->vy = recs2[i2].point.vy;
            pos2->vz = recs2[i2].point.vz;
            id       = recs2[i2].key.value;
            goto found2;
        }
    }
missed2:
    id = 0;
found2:
    sc->attackKey = id;
    if (id != 0) {
        coord = work->hits[4].body.coord;
    hit:
        _gluttonHitEffect(coord, id);
        if (sc->attackKey != 0) {
            goto body;
        }
    }

    pos3  = &sc->contactPoint;
    recs3 = work->hits[5].contacts;
    for (i3 = 0; i3 < ARRAY_SIZE(work->hits[5].contacts); i3++) {
        if (recs3[i3].key.value == 0) {
            goto missed3;
        }
        if ((recs3[i3].key.value & 0xFFFF0000) == 0x20000) {
            pos3->vx = recs3[i3].point.vx;
            pos3->vy = recs3[i3].point.vy;
            pos3->vz = recs3[i3].point.vz;
            id       = recs3[i3].key.value;
            goto found3;
        }
    }
missed3:
    id = 0;
found3:
    sc->attackKey = id;
    if (id == 0) {
        goto out;
    }
    _gluttonHitEffect(work->hits[5].body.coord, id);
    if (sc->attackKey == 0) {
        goto out;
    }
body:
    work->groups3To5Cooldown = damageGetPlayerAttackHitCooldown(sc->attackKey);
    damageGetPlayerAttackReaction(sc->attackKey);

    sc->toPlayer.vx    = (cfg->coordMtx->t[0] - task->extra.tmd->coords->coord.t[0]) + 0x51F;
    dx2                = sc->toPlayer.vx * sc->toPlayer.vx;
    sc->toPlayer.vy    = (cfg->coordMtx->t[1] - task->extra.tmd->coords->coord.t[1]) - 0xFA;
    dy2                = sc->toPlayer.vy * sc->toPlayer.vy;
    sc->toPlayer.vz    = (cfg->coordMtx->t[2] - task->extra.tmd->coords->coord.t[2]) + 0x25F;
    dz2                = sc->toPlayer.vz * sc->toPlayer.vz;
    sc->playerDistance = SquareRoot0(dx2 + dy2 + dz2);
    sc->damage         = damageComputePlayerAttack(sc->attackKey, sc->playerDistance, 0, 0);

    if (damageRollCriticalHit(work->escorts[0], sc->attackKey, 0) != 0 && (state = work->state, state != 0xD) && state != 3 &&
        state != 9 && state != 0xE && state != 0xF && state != 8 && state != 0xB && work->phase != 0 &&
        work->playerCaught != 1) {
        sc->offset.vz = 0x1F4;
        sc->offset.vy = 0;
        sc->offset.vx = 0;
        sc->offset.vy = 0;
        sc->offset.vx = 0;
        sc->offset.vz = 0x258;
        effectSpawn(EFFECT_CRITICAL_HIT, &work->escorts[0]->task->extra.tmd->coords[1], 0, &sc->offset);
        sc->damage *= 4;
        work->state = 0xE;
    }

    dmg = sc->damage / 6;
    if (dmg == 0) {
        if (sc->damage == 0) {
            sc->damage = 0;
        } else {
            sc->damage = 1;
        }
    } else {
        sc->damage = dmg;
    }
    damageAccumulateLifeDrainHp(host, sc->attackKey, sc->damage, 0);
    host->hp             -= sc->damage;
    work->groups3To5Pool -= sc->damage;
    if (work->groups3To5Pool <= 0 && (state = work->state, state != 0xD) && state != 3 && state != 9 && state != 0xE &&
        state != 0xF && state != 8 && state != 0xB && work->phase != 0 && work->playerCaught != 1) {
        sc->offset.vy = 0;
        sc->offset.vx = 0;
        sc->offset.vz = 0x258;
        effectSpawn(EFFECT_CRITICAL_HIT, &work->escorts[0]->task->extra.tmd->coords[1], 0, &sc->offset);
        work->state          = 0xE;
        work->groups3To5Pool = (s16)D_actor_444000_80144A38.hpMax;
    }

    worldTargetAddReadoutAmount(&work->escorts[0]->node, sc->damage, 0);
    work->escorts[0]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(work->escorts[0]->task->extra.tmd->coords);
    sc->offset.vx = sc->contactPoint.vx - work->escorts[0]->task->extra.tmd->coords->workm.t[0];
    sc->offset.vy = sc->contactPoint.vy - work->escorts[0]->task->extra.tmd->coords->workm.t[1];
    sc->offset.vz = sc->contactPoint.vz - work->escorts[0]->task->extra.tmd->coords->workm.t[2];
    angle         = ratan2(sc->offset.vx, sc->offset.vz) -
            ratan2(-task->extra.tmd->coords->workm.m[2][0],
                   task->extra.tmd->coords->workm.m[2][2]);
    sc->contactYaw = angle;
    sc->contactYaw = _actorAngleNormalizeYaw(angle);

    if (work->animId != 4) {
        work->neckYaw       = 0;
        work->neckYawTarget = 0;
    }
out:
    SCRATCH_STACK_RELEASE_BLOCK(GluttonHitScratch);
}

#include "../../shared/glutton_hit_groups6to8.inc.c"

/// Reset/teardown handler: when the work block is asking for a reset, arm the
/// re-spawn sequence and push the host's model flag word onto each of the seven
/// escorts' models. Otherwise run the ordinary re-arm while the sub-state
/// counter is still below 0xA, and once it reaches 2 release the host's and
/// every escort's model buffers.
static void func_actor_444000_8013D810(Task* arg0)
{
    GluttonWork* work;
    GluttonWork* escorts;
    GluttonWork* dying;
    s16          i;
    s16          j;

    work = arg0->work;
    if (work->stateChanged != 0) {
        escorts                = arg0->work;
        work->freeCountdown    = 3;
        arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        for (i = 0; i < ARRAY_SIZE(escorts->escorts); i++) {
            if (escorts->escorts[i] != NULL) {
                escorts->escorts[i]->task->extra.tmd->flags = arg0->extra.tmd->flags;
            }
        }
        work->animId     = 0xA;
        work->animStep   = GLUTTON_ANIM_STEP_RESTART;
        work->stateTicks = 0;
        work->animRate   = 0x10;
        _gluttonTickAnim(arg0);
    } else {
        if (work->stateTicks < 0xA) {
            _gluttonTickAnim(arg0);
        }
        if (work->stateTicks == 2) {
            dying = arg0->work;
            tmdFreePrimitiveBuffer(arg0->extra.tmd);
            for (j = 0; j < ARRAY_SIZE(dying->escorts); j++) {
                if (dying->escorts[j] != NULL) {
                    tmdFreePrimitiveBuffer(dying->escorts[j]->task->extra.tmd);
                }
            }
        }
    }
}

/// Hands the scratchpad frame `Actor444000_FlattenRotation` borrowed back to
/// the scratch stack. Written as an inline like the rotation itself: only
/// inline-expanded code keeps the absolute `lui $at` form of the scratch-head
/// accesses, so a release written straight into the caller does not match.
static __inline__ void Actor444000_ReleaseRotScratch(void)
{
    SCRATCH_STACK_RELEASE_BYTES(sizeof(ActorScaleRotScratch));
}

/// Rebuilds one model's root coordinate around the yaw it already faces and
/// flattens it vertically: `ratan2` of the rotation's Z basis gives the yaw,
/// `gfxRotMatrixY` rebuilds the rotation from it and `ScaleMatrix` applies
/// 1.0 / `vy` / 1.0. The working matrix lives in a frame carved off
/// the scratch stack; the caller releases it with
/// `Actor444000_ReleaseRotScratch` once it has cleared the coordinate again.
static __inline__ void Actor444000_FlattenRotation(GfxCoord* coord, s32 vy)
{
    ActorScaleRotScratch* sc;
    s16                   ang;

    sc                                         = (ActorScaleRotScratch*)(SCRATCH_STACK_CURSOR(u8) - sizeof(ActorScaleRotScratch));
    SCRATCH_STACK_CURSOR(ActorScaleRotScratch) = sc;

    ang     = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->yaw = ang;
    gfxRotMatrixY(&sc->rotation, ang, 1);
    sc->scale.vx = 0x1000;
    sc->scale.vy = vy;
    sc->scale.vz = 0x1000;
    ScaleMatrix(&sc->rotation, &sc->scale);

    coord->coord.m[0][0] = sc->rotation.m[0][0];
    coord->coord.m[0][1] = sc->rotation.m[0][1];
    coord->coord.m[0][2] = sc->rotation.m[0][2];
    coord->coord.m[1][0] = sc->rotation.m[1][0];
    coord->coord.m[1][1] = sc->rotation.m[1][1];
    coord->coord.m[1][2] = sc->rotation.m[1][2];
    coord->coord.m[2][0] = sc->rotation.m[2][0];
    coord->coord.m[2][1] = sc->rotation.m[2][1];
    coord->coord.m[2][2] = sc->rotation.m[2][2];
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
}

/// Re-arm handler run once the block asks for a reset: clear the host's model
/// flag word onto itself and every escort, clear `neckPitchEnabled` and `neckYawEnabled`, then
/// step the animation on and flatten six of the models -- escorts 2, 4, 3, 0 and
/// 1 plus the host itself -- onto the ground plane. Escort 3 keeps a little
/// height (`vy` 0x400) where the rest are flattened outright. The last release
/// clears escort 2's coordinate flag again rather than escort 1's, which looks
/// like a copy-paste slip in the original but is what the ROM does.
static void func_actor_444000_8013D96C(Task* arg0)
{
    GluttonWork* work;
    GluttonWork* escorts;
    s16          i;

    work = arg0->work;
    if (work->stateChanged != 0) {
        arg0->extra.tmd->flags = 0;
        escorts                = arg0->work;
        escorts->freeCountdown = 0;
        arg0->extra.tmd->flags = 0;
        for (i = 0; i < ARRAY_SIZE(escorts->escorts); i++) {
            if (escorts->escorts[i] != NULL) {
                escorts->escorts[i]->task->extra.tmd->flags = arg0->extra.tmd->flags;
            }
        }
        work->neckPitchEnabled = 0;
        work->neckYawEnabled   = 0;
    }

    _gluttonTickAnim(arg0);

    Actor444000_FlattenRotation(work->escorts[2]->task->extra.tmd->coords, 0);
    work->escorts[2]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Actor444000_ReleaseRotScratch();

    Actor444000_FlattenRotation(arg0->extra.tmd->coords, 0);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Actor444000_ReleaseRotScratch();

    Actor444000_FlattenRotation(work->escorts[4]->task->extra.tmd->coords, 0);
    work->escorts[4]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Actor444000_ReleaseRotScratch();

    Actor444000_FlattenRotation(work->escorts[3]->task->extra.tmd->coords, 0x400);
    work->escorts[3]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Actor444000_ReleaseRotScratch();

    Actor444000_FlattenRotation(work->escorts[0]->task->extra.tmd->coords, 0);
    work->escorts[0]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Actor444000_ReleaseRotScratch();

    Actor444000_FlattenRotation(work->escorts[1]->task->extra.tmd->coords, 0);
    work->escorts[2]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Actor444000_ReleaseRotScratch();
}

/// Drag tick of the arena fight: the state the boss runs while it is hauling the
/// player in along the line between them.
///
/// A reset request (`stateChanged`) re-arms the block on animation 3, clears the host
/// model's flag word and pushes it onto each of the seven escorts' models, makes
/// sure the host and every escort has its model buffers allocated, re-seeds the
/// spinner target `gGluttonSpinnerTarget` from the fourth part of placed actor 0's
/// model and announces sub-state 2 through message 0x7DA.
///
/// Every tick then pins the player down to the arena floor, runs the ordinary
/// re-arm and stows the yaw from the host to the player -- relative to the
/// host's own facing, wrapped to +/-0x800 -- in `neckYawTarget`. The host's fifth
/// part is carried into view space, the player-relative offset from there gives
/// the direction and distance the pull works along, and the animation frame
/// picks how hard: `phasePull` is the phase's base strength and the frame divides
/// `-(phasePull + 0x19)` by 1, 2, 3, 4, 6 or 2/3 before `gte_gpf12` scales the
/// normalised direction by it. Frames outside 9..20 drop the pull and clear
/// `hostExposed`. `playerActorSetPendingDisplacement` hands the result to the player actor unless the
/// game is in mode 2 or 0xA or the player is already in mode 2.
///
/// Alongside that: a script fires every `padScriptPeriod` ticks while the frame sits in
/// 0xA..0x12, two cues play on frames 0x3C and 0xE8, the fight asks slot 3 for
/// the hold (message 0x3F8) once the player is inside 0x4B0 on frames 0xB..0xF
/// and phase 6 onward clamps the player back behind -0x52D0. Once `hostRig.slots[1]`
/// raises its flag the fight announces sub-state 3, moves to state 0xA and drops
/// its two spawned escorts.
static void func_actor_444000_8013E058(Task* task)
{
    GluttonWork*             work  = task->work;
    Enemy*                   enemy = task->spawnArg2.pointer;
    Task*                    slot3;
    GameActor*               actor;
    _Actor444000DragScratch* sc;
    GluttonWork*             escorts;
    GluttonWork*             buffers;
    PlayerStatus*            cfg;
    GfxCoord*                coord;
    GfxCoord*                facing;
    GfxCoord*                yawCoord;
    GfxCoord*                clamp;
    SVECTOR*                 posp;
    SVECTOR*                 dirp;
    TmdObject*               tmd;
    TmdObject*               escortTmd;
    s16                      angle;
    s16                      i;
    s16                      j;
    s16                      dz;

    slot3 = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    sc    = SCRATCH_STACK_RESERVE_BLOCK(_Actor444000DragScratch);
    actor = slot3->work;

    if (work->stateChanged != 0) {
        work->animId           = 3;
        work->animStep         = GLUTTON_ANIM_STEP_RESTART;
        escorts                = task->work;
        escorts->freeCountdown = 0;
        task->extra.tmd->flags = 0;
        for (i = 0; i < ARRAY_SIZE(escorts->escorts); i++) {
            if (escorts->escorts[i] != NULL) {
                escorts->escorts[i]->task->extra.tmd->flags = task->extra.tmd->flags;
            }
        }
        tmd     = task->extra.tmd;
        buffers = task->work;
        if (tmd->buffer == NULL) {
            tmdAllocPrimitiveBuffer(tmd);
        }
        for (j = 0; j < ARRAY_SIZE(buffers->escorts); j++) {
            if (buffers->escorts[j] != NULL) {
                escortTmd = buffers->escorts[j]->task->extra.tmd;
                if (escortTmd->buffer == NULL) {
                    tmdAllocPrimitiveBuffer(escortTmd);
                }
            }
        }
        work->neckPitchTarget  = 0;
        work->neckPitchEnabled = 1;
        work->neckYawEnabled   = 1;
        work->hostExposed      = 0;
        posp                   = &gGluttonSpinnerTarget;
        posp->vz               = 0;
        posp->vy               = 0;
        posp->vx               = 0;
        actorLocalToView(&sceneFindPlacedActor(0)->extra.tmd->coords[3], posp);
        D_actor_444000_80161888.command.context.loc.stage = 0;
        D_actor_444000_80161888.command.context.loc.area  = 0x2C;
        D_actor_444000_80161888.command.command           = 2;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_actor_444000_80161888.command, ACTOR_COMMAND_MESSAGE_APPLY);
    }

    coord = slot3->extra.tmd->coords;
    if (coord->coord.t[1] > 0) {
        coord->coord.t[1]                      = 0;
        slot3->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    _gluttonTickAnim(task);

    cfg           = &gPlayerStatus;
    facing        = task->extra.tmd->coords;
    dirp          = &sc->offset;
    sc->offset.vx = (u16)cfg->coordMtx->t[0] - (u16)facing->coord.t[0];
    dirp->vy      = (u16)cfg->coordMtx->t[1] - (u16)facing->coord.t[1];
    dz            = (u16)cfg->coordMtx->t[2] - (u16)facing->coord.t[2];
    dirp->vz      = dz;
    yawCoord      = task->extra.tmd->coords;
    angle         = ratan2(sc->offset.vx, dz) - ratan2(-yawCoord->coord.m[2][0], yawCoord->coord.m[2][2]);
    if (angle < 0) {
        while (1) {
            if (angle < -0x800) {
                angle += 0x1000;
                continue;
            }
            break;
        }
    } else {
        while (1) {
            if (angle > 0x800) {
                angle -= 0x1000;
                continue;
            }
            break;
        }
    }
    work->neckYawTarget = angle;

    sc->offset.vz = 0;
    sc->offset.vy = 0;
    sc->offset.vx = 0;
    actorLocalToView(&task->extra.tmd->coords[4], &sc->offset);

    sc->offset.vx       = (u16)slot3->extra.tmd->coords->coord.t[0] - (u16)sc->offset.vx;
    sc->offset.vy       = (u16)slot3->extra.tmd->coords->coord.t[1] - (u16)sc->offset.vy;
    sc->offset.vz       = (u16)slot3->extra.tmd->coords->coord.t[2] - (u16)sc->offset.vz;
    sc->playerDistance  = sc->offset.vx * sc->offset.vx;
    sc->playerDistance += sc->offset.vz * sc->offset.vz;
    sc->playerDistance  = SquareRoot0(sc->playerDistance);
    VectorNormalSS(&sc->offset, &sc->offset);

    switch (work->phase) {
        case 0:
            sc->padScriptPeriod = 0x19;
            break;
        case 1:
            sc->padScriptPeriod = 0x11;
            break;
        case 2:
        default:
            sc->padScriptPeriod = 0xE;
            break;
    }
    if (((u32)((work->hostRig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) - 0xA) < 9U) && ((work->stateTicks % sc->padScriptPeriod) == 0)) {
        padScriptSpawn(D_actor_444000_80144A94, D_actor_444000_80144AA0);
    }

    switch (work->phase) {
        case 0:
        case 6:
            sc->phasePull = 0;
            break;
        case 1:
            sc->phasePull = 5;
            break;
        case 2:
            sc->phasePull = 0xA;
            break;
        case 3:
        case 4:
        case 5:
        default:
            sc->phasePull = 0xF;
            break;
    }

    if (work->stateTicks == 0x3C) {
        s32 id;
        s32 pan;

        id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000A;
        pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(id, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    if (work->stateTicks == 0xE8) {
        sndEvtRequestScriptStop((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0A), SOUND_SCRIPT_STOP_KEEP_RELEASE);
    }

    work->hostExposed = 1;
    switch (work->hostRig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) {
        case 9:
            gte_lddp(-(sc->phasePull + 0x19) / 4);
            gte_ldsv(&sc->offset);
            gte_gpf12();
            gte_stsv(&sc->offset);
            work->neckPitchTarget = 0x180;
            break;
        case 10:
            gte_lddp(-(sc->phasePull + 0x19) / 2);
            gte_ldsv(&sc->offset);
            gte_gpf12();
            gte_stsv(&sc->offset);
            break;
        case 11:
        case 13:
        case 15:
            gte_lddp(-(sc->phasePull + 0x19));
            gte_ldsv(&sc->offset);
            gte_gpf12();
            gte_stsv(&sc->offset);
            work->neckPitchTarget = 0x2B2;
            break;
        case 12:
        case 14:
            gte_lddp(-((sc->phasePull + 0x19) * 3) / 2);
            gte_ldsv(&sc->offset);
            gte_gpf12();
            gte_stsv(&sc->offset);
            work->neckPitchTarget = 0x500;
            break;
        case 16:
            gte_lddp(-(sc->phasePull + 0x19) / 3);
            gte_ldsv(&sc->offset);
            gte_gpf12();
            gte_stsv(&sc->offset);
            work->neckPitchTarget = 0x100;
            break;
        case 17:
        case 18:
            gte_lddp(-(sc->phasePull + 0x19) / 3);
            gte_ldsv(&sc->offset);
            gte_gpf12();
            gte_stsv(&sc->offset);
            work->neckPitchTarget = 0x400;
            break;
        case 19:
        case 20:
            sc->offset.vz = 0;
            sc->offset.vx = 0;
            gte_lddp(-(sc->phasePull + 0x19) / 6);
            gte_ldsv(&sc->offset);
            gte_gpf12();
            gte_stsv(&sc->offset);
            work->neckPitchTarget = 0;
            break;
        default:
            work->hostExposed = 0;
            sc->offset.vz     = 0;
            sc->offset.vx     = 0;
            break;
    }

    if (((u32)((work->hostRig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) - 0xB) < 5U) && (sc->playerDistance < 0x4B0) && (work->phase < 6)) {
        if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &D_actor_444000_80161928.hold, 0) == 0) {
            work->state        = 0xD;
            work->playerCaught = 1;
        }
    }
    if (work->phase >= 6) {
        clamp = slot3->extra.tmd->coords;
        if (clamp->coord.t[2] > -0x52D0) {
            clamp->coord.t[2] = -0x52D0;
        }
    }

    if (sc->offset.vx != 0 || sc->offset.vz != 0) {
        sc->displacement.vx = sc->offset.vx;
        sc->displacement.vy = 0;
        sc->displacement.vz = sc->offset.vz;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != 2 && gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != 0xA && actor->mode != GAME_ACTOR_MODE_SCRIPTED) {
            playerActorSetPendingDisplacement(&sc->displacement);
        }
    }

    if (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        D_actor_444000_80161888.command.context.loc.stage = 0;
        D_actor_444000_80161888.command.context.loc.area  = 0x2C;
        D_actor_444000_80161888.command.command           = 3;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_actor_444000_80161888.command, ACTOR_COMMAND_MESSAGE_APPLY);
        work->state = 0xA;
        for (sc->slot = 0; sc->slot < 2; sc->slot++) {
            work->summons[sc->slot] = NULL;
        }
    }
    work->summonsAlive = 0;
    SCRATCH_STACK_RELEASE_BLOCK(_Actor444000DragScratch);
}

/// Escort-order tick of the arena fight: the state the boss runs while it has
/// the player pinned in front of it.
///
/// A reset request raises the eight arena floor vertices `Gp_GridParams` keeps
/// at 24..31, re-arms the block on animation 0xF, clears the host model's flag
/// word and pushes it onto each of the seven escorts' models, makes sure the
/// host and every escort has its model buffers allocated, and -- if the player
/// has drifted inside 0xB54 -- drags them back out to that range along the
/// line between the two. It then places the player with
/// `Actor444000_PlacePlayerAhead`, hands the fight the battle flag, drops the
/// host and three of its escorts out of the actor slots and orders escort 3
/// through a 0x7DA message.
///
/// Every other tick runs the ordinary re-arm first. Animation 0xF hands over to
/// 0xE once the second slot raises its flag, and each animation fires one-shot
/// cues on the frames it reaches -- 0x19 under 0xF, 0x1D / 0x23 / 0x27 under
/// 0xE, the last two also kicking the pad -- with `prevSlot3Cue` remembering the
/// frame so none repeats while it is held. While message 0x3ED reports the
/// player free they are put back on the host's own position, and sub-state 0x17
/// re-places them and re-sends the 0x3FF animation.
/// Places the player in front of the host and points the pair at each other:
/// the host's fifth part is carried into view space, the yaw from there to the
/// player picks which of the two message-0x3FF animation tables the tick will
/// send (`..._80161680` past a quarter turn, `..._80161670` within it), and the
/// opposite yaw is stowed in `neckYawTarget` for the drive step. The normalised
/// direction scaled to 0x384 is where the player is asked to stand.
static __inline__ void Actor444000_PlacePlayerAhead(Task* task, GluttonWork* work,
                                                    Task* player, _Actor444000CatchScratch* sc,
                                                    PlayerStatus* cfg)
{
    GfxCoord* coord;
    s32       yaw;

    sc->anchor.vz = 0;
    sc->anchor.vy = 0;
    sc->anchor.vx = 0;
    actorLocalToView(&task->extra.tmd->coords[4], &sc->anchor);

    sc->offset.vx = sc->anchor.vx - player->extra.tmd->coords->coord.t[0];
    sc->offset.vy = 0;
    sc->offset.vz = sc->anchor.vz - player->extra.tmd->coords->coord.t[2];
    yaw           = actorYawTo(player->extra.tmd->coords, sc->offset.vx, sc->offset.vz);
    sc->playerYaw = yaw;
    if (abs(sc->playerYaw) > 0x400) {
        if (sc->playerYaw > 0) {
            sc->playerYaw = yaw - 0x800;
        } else {
            sc->playerYaw = yaw + 0x800;
        }
        work->playerAnim.source.sets = (D_actor_444000_80161670 + 4);
    } else {
        work->playerAnim.source.sets = D_actor_444000_80161670;
    }
    coord          = player->extra.tmd->coords;
    sc->playerYaw += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);

    sc->offset.vx       = player->extra.tmd->coords->coord.t[0] - sc->anchor.vx;
    sc->offset.vy       = 0;
    sc->offset.vz       = player->extra.tmd->coords->coord.t[2] - sc->anchor.vz;
    work->neckYawTarget = actorYawTo(task->extra.tmd->coords, sc->offset.vx, sc->offset.vz);

    VectorNormalSS(&sc->offset, &sc->offset);
    gte_lddp(0x384);
    gte_ldsv(&sc->offset);
    gte_gpf12();
    gte_stsv(&sc->offset);

    D_actor_444000_80161908.placement.pos.vx = sc->anchor.vx + sc->offset.vx;
    D_actor_444000_80161908.placement.pos.vy = player->extra.tmd->coords->coord.t[1];
    D_actor_444000_80161908.placement.pos.vz = sc->anchor.vz + sc->offset.vz;
    D_actor_444000_80161908.placement.rot.vx = 0;
    D_actor_444000_80161908.placement.rot.vy = sc->playerYaw;
    D_actor_444000_80161908.placement.rot.vz = 0;
    if (cfg->hp > 0) {
        TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_PLACE, &D_actor_444000_80161908.placement, 0);
    }
}

static void func_actor_444000_8013EC84(Task* arg0)
{
    _Actor444000CatchScratch* sc;
    GluttonWork*              work;
    GluttonWork*              escorts;
    GluttonWork*              buffers;
    Enemy*                    enemy;
    Task*                     player;
    PlayerStatus*             cfg;
    Task*                     target;
    TmdObject*                tmd;
    TmdObject*                escortTmd;
    SVECTOR*                  verts;
    s16                       i;
    s16                       j;
    s32                       frame;
    s32                       cueId;
    s32                       cuePan;
    s32                       hitId;
    s32                       hitPan;
    s32                       endId;
    s32                       endPan;

    work   = arg0->work;
    enemy  = arg0->spawnArg2.pointer;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    cfg    = &gPlayerStatus;

    if (work->stateChanged != 0) {
        sc = SCRATCH_STACK_RESERVE_BLOCK(_Actor444000CatchScratch);

        verts        = Gp_GridParams->vertices;
        verts[24].vy = 0x1F4;
        verts[25].vy = 0x1F4;
        verts[26].vy = 0x320;
        verts[27].vy = 0x320;
        verts[28].vy = 0x1F4;
        verts[29].vy = 0x1F4;
        verts[30].vy = 0x320;
        verts[31].vy = 0x320;

        work->animId           = 0xF;
        work->animStep         = GLUTTON_ANIM_STEP_RESTART;
        escorts                = arg0->work;
        escorts->freeCountdown = 0;

        arg0->extra.tmd->flags = 0;
        for (i = 0; i < ARRAY_SIZE(escorts->escorts); i++) {
            if (escorts->escorts[i] != NULL) {
                escorts->escorts[i]->task->extra.tmd->flags = arg0->extra.tmd->flags;
            }
        }

        tmd     = arg0->extra.tmd;
        buffers = arg0->work;
        if (tmd->buffer == NULL) {
            tmdAllocPrimitiveBuffer(tmd);
        }
        for (j = 0; j < ARRAY_SIZE(buffers->escorts); j++) {
            if (buffers->escorts[j] != NULL) {
                escortTmd = buffers->escorts[j]->task->extra.tmd;
                if (escortTmd->buffer == NULL) {
                    tmdAllocPrimitiveBuffer(escortTmd);
                }
            }
        }

        work->neckPitchTarget  = 0;
        work->neckPitchEnabled = 0;
        work->neckYawEnabled   = 1;
        work->hostExposed      = 0;

        sc->playerDelta.vx = player->extra.tmd->coords->coord.t[0] -
                             arg0->extra.tmd->coords->coord.t[0];
        sc->playerDelta.vz = player->extra.tmd->coords->coord.t[2] -
                             arg0->extra.tmd->coords->coord.t[2];
        sc->playerDistance = SquareRoot0(sc->playerDelta.vx * sc->playerDelta.vx + sc->playerDelta.vz * sc->playerDelta.vz);
        if (sc->playerDistance < 0xB54) {
            sc->offset.vy = 0;
            sc->offset.vx = sc->playerDelta.vx;
            sc->offset.vz = sc->playerDelta.vz;
            VectorNormalSS(&sc->offset, &sc->offset);
            gte_lddp(0xCE4);
            gte_ldsv(&sc->offset);
            gte_gpf12();
            gte_stsv(&sc->offset);
            player->extra.tmd->coords->coord.t[0] =
                arg0->extra.tmd->coords->coord.t[0] + sc->offset.vx;
            player->extra.tmd->coords->coord.t[2] =
                arg0->extra.tmd->coords->coord.t[2] + sc->offset.vz;
            player->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(player->extra.tmd->coords);
        }

        Actor444000_PlacePlayerAhead(arg0, work, player, sc, cfg);

        D_actor_444000_80161868.playerAtHost = 0;
        Gp_StateC08.flags                   |= ATTACHMENT_FLAG_EVENT_LOCK;
        roomEffectRequestCancelAll();
        worldTargetDisableNodeLockOn(&enemy->node);
        worldTargetDisableNodeLockOn(&work->escorts[3]->node);
        worldTargetDisableNodeLockOn(&work->escorts[0]->node);
        worldTargetDisableNodeLockOn(&work->escorts[1]->node);
        sndEvtRequestScriptStop((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0A), SOUND_SCRIPT_STOP_KEEP_RELEASE);

        D_actor_444000_80161888.command.context.loc.stage = 0;
        D_actor_444000_80161888.command.context.loc.area  = 0x2C;
        D_actor_444000_80161888.command.command           = 3;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_actor_444000_80161888.command, ACTOR_COMMAND_MESSAGE_APPLY);
    } else {
        sc = SCRATCH_STACK_RESERVE_BLOCK(_Actor444000CatchScratch);
        _gluttonTickAnim(arg0);

        if ((work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) && work->animId == 0xF) {
            work->animStep = GLUTTON_ANIM_STEP_RESTART;
            work->animId   = 0xE;
        }

        if (work->animId == 0xF) {
            if (cfg->hp > 0) {
                target = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                taskMessageDispatch(target, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, 3), 0);
                if (cfg->hp <= 0) {
                    ((GameActor*)player->work)->state = 0xA;
                    gGameSession->deathSoundCountdown = 0x1E;
                    gGameSession->deathFadeFrames     = 0x36;
                    gGameSession->deathRestartDelay   = 0x5A;
                }
            }
            frame = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (frame == 0x19 && work->prevSlot3Cue != frame) {
                cueId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200011;
                cuePan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                sndEvtRequestScriptStart(cueId, cuePan,
                                         (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            }
            work->prevSlot3Cue = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        }

        if (work->animId == 0xE) {
            frame = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (frame == 0x1D && work->prevSlot3Cue != frame) {
                padScriptSpawnVariableMotorRamp(4, 0xFF, 8);
            }
            frame = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (frame == 0x23 && work->prevSlot3Cue != frame) {
                hitId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200012;
                hitPan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                sndEvtRequestScriptStart(hitId, hitPan,
                                         (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                padScriptSpawnVariableMotorRamp(4, 0xFF, 8);
            }
            frame = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (frame == 0x27 && work->prevSlot3Cue != frame) {
                endId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200012;
                endPan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                sndEvtRequestScriptStart(endId, endPan,
                                         (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                padScriptSpawnVariableMotorRamp(4, 0xFF, 8);
            }
            work->prevSlot3Cue = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        }

        if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
            D_actor_444000_80161908.placement.pos.vx = arg0->extra.tmd->coords->coord.t[0];
            D_actor_444000_80161908.placement.pos.vy = arg0->extra.tmd->coords->coord.t[1];
            D_actor_444000_80161908.placement.pos.vz = arg0->extra.tmd->coords->coord.t[2];
            D_actor_444000_80161908.placement.rot.vx = 0;
            D_actor_444000_80161908.placement.rot.vy = 0;
            D_actor_444000_80161908.placement.rot.vz = 0;
            TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_PLACE, &D_actor_444000_80161908.placement, 0);
            D_actor_444000_80161868.playerAtHost = 1;
        }

        if (work->stateTicks == 0x17) {
            sndEvtRequestScriptStop((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0A), SOUND_SCRIPT_STOP_KEEP_RELEASE);
            Actor444000_PlacePlayerAhead(arg0, work, player, sc, cfg);
            work->playerAnim.animationId = 1;
            work->playerAnim.blend       = ANIMATION_BLEND_RESET;
            work->playerAnim.blendFrames = 0;
            work->field_F02              = 1;
            TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
        }
    }

    SCRATCH_STACK_RELEASE_BLOCK(_Actor444000CatchScratch);
}

/// Whether any of the leading `count` contact records is a kind 0x10000
/// (player) contact, stopping at the first empty record.
static inline s32 _actor444000HasPlayerContact(WorldCollisionContact* records, s16 count)
{
    s16 i;

    for (i = 0; i < count; i++) {
        if (records[i].key.value == 0) {
            break;
        }
        if ((records[i].key.value & 0xFFFF0000) == 0x10000) {
            return 1;
        }
    }
    return 0;
}

/// Tick of the arena fight that runs the boss' two swipes and keeps the player
/// pinned in the scripted animation.
///
/// A reset request re-arms the block on animation 4, clears the host model's
/// flag word and pushes it onto each of the seven escorts' models, makes sure
/// the host and every escort has its model buffers allocated, then rebuilds the
/// free coordinate at `swipeCoord` from `neckYaw` and plays the entry cue.
/// That coordinate is pushed through `actorRenderComposeCoord` again on every step.
///
/// The two swipes are one-shot: animation 4 reaching frame 0xC raises bit
/// 0x8000 of the collision object's flags, kicks the pad and fires two cues,
/// and animation 5 reaching frame 0x1C fires a third. `prevSwipeCue` remembers the
/// frame each step so neither repeats while the frame is held, and the bit is
/// cleared on every step the first swipe is not live.
///
/// `stateTicks` then picks the blend weight in `limbPose` (and hands over to
/// state 0xA at 0xDC), and while it sits in 0x29..0x2E the shared timer
/// `gGluttonLimbReach` climbs by 0x258 a step up to 0x1770 -- past 0x39 it
/// is wound back down again instead.
///
/// The rest is the player hold: once one of the collision object's five records
/// reports a hit of class 1, message 0x3F8 is asked whether the player can be
/// taken over and message 0x3F9 asks for the hold itself, with `swipeDamageReply`
/// keeping that reply and `playerCaught` marking the hold as ours. While it is,
/// the 0x3FF animation is re-sent every step the reply and `playerCaught` agree
/// (or, if they do not, for the first 0x28 steps), and after 0x17 steps without
/// the hold the payload is swapped for the player's own weapon animation
/// (`field_4` 4). A reply of something other than 1 on that second stage
/// cancels the animation with message 0x3F1 and drops the hold.
static void func_actor_444000_8013FB74(Task* arg0)
{
    GluttonWork* work;
    GluttonWork* escorts;
    GluttonWork* buffers;
    Enemy*       enemy;
    Task*        player;
    Task*        target;
    TmdObject*   tmd;
    TmdObject*   escortTmd;
    GfxCoord*    coord;
    s16          i;
    s16          j;
    s16          mode;
    s32          frame;
    s32          frame2;
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
    u16          count;

    work   = arg0->work;
    enemy  = arg0->spawnArg2.pointer;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    SCRATCH_STACK_RESERVE_BYTES(0x30);

    if (work->stateChanged != 0) {
        work->lastAttack       = 0xB;
        work->animId           = 4;
        work->animStep         = GLUTTON_ANIM_STEP_RESTART;
        escorts                = arg0->work;
        escorts->freeCountdown = 0;
        arg0->extra.tmd->flags = 0;
        for (i = 0; i < ARRAY_SIZE(escorts->escorts); i++) {
            if (escorts->escorts[i] != NULL) {
                escorts->escorts[i]->task->extra.tmd->flags = arg0->extra.tmd->flags;
            }
        }
        tmd     = arg0->extra.tmd;
        buffers = arg0->work;
        if (tmd->buffer == NULL) {
            tmdAllocPrimitiveBuffer(tmd);
        }
        for (j = 0; j < ARRAY_SIZE(buffers->escorts); j++) {
            if (buffers->escorts[j] != NULL) {
                escortTmd = buffers->escorts[j]->task->extra.tmd;
                if (escortTmd->buffer == NULL) {
                    tmdAllocPrimitiveBuffer(escortTmd);
                }
            }
        }
        work->neckYawEnabled   = 1;
        work->neckPitchEnabled = 0;
        work->hostExposed      = 1;
        work->limbPoseEnabled  = 1;
        gfxRotMatrixY(&work->swipeCoord.node.coord, work->neckYaw, 1);
        work->swipeCoord.node.composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&work->swipeCoord.node);
        work->wallDistanceTarget = 0xC80;

        resetId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200017;
        resetPan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(resetId, resetPan,
                                 (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }

    coord                              = &work->swipeCoord.node;
    work->swipeCoord.node.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);

    if (work->animId == 4 && (frame = work->hostRig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0xC &&
        work->prevSwipeCue != frame) {
        gfxRotMatrixY(&work->swipeCoord.node.coord, work->neckYaw, 1);
        work->swipeCoord.node.composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        work->shakeLevel       = GLUTTON_SHAKE_LONG;
        work->swipeBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        padScriptSpawnVariableMotorRamp(0x30, 0xFF, 8);

        swipeId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200019;
        swipePan = (s8)worldCoordGetOriginAudioPan(&work->escorts[0]->task->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(
            swipeId, swipePan,
            (s8)(worldCoordGetOriginAudioDepth(&work->escorts[0]->task->extra.tmd->coords[1]) / 2));

        swipe2Id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020001A;
        swipe2Pan = (s8)worldCoordGetOriginAudioPan(&work->escorts[0]->task->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(
            swipe2Id, swipe2Pan,
            (s8)(worldCoordGetOriginAudioDepth(&work->escorts[0]->task->extra.tmd->coords[1]) / 2));
    } else {
        work->swipeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }

    if (work->animId == 5 && (frame2 = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x1C &&
        work->prevSwipeCue != frame2) {
        work->shakeLevel = GLUTTON_SHAKE_LONG;
        padScriptSpawnVariableMotorRamp(0x20, 0x8F, 8);

        hitId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020001B;
        hitPan = (s8)worldCoordGetOriginAudioPan(&work->escorts[0]->task->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(
            hitId, hitPan,
            (s8)(worldCoordGetOriginAudioDepth(&work->escorts[0]->task->extra.tmd->coords[1]) / 2));
    }

    if (work->animId == 4) {
        work->prevSwipeCue = work->hostRig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    } else {
        work->prevSwipeCue = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    }

    switch (work->stateTicks) {
        case 0x14:
            gGluttonLimbReach = 0x640;
            work->limbPose    = 0;
            break;
        case 0x22:
            work->limbPose = 1;
            break;
        case 0x2B:
            work->limbPose = 5;
            cueId          = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200018;
            cuePan         = (s8)worldCoordGetOriginAudioPan(&work->escorts[0]->task->extra.tmd->coords[1]);
            sndEvtRequestScriptStart(
                cueId, cuePan,
                (s8)(worldCoordGetOriginAudioDepth(&work->escorts[0]->task->extra.tmd->coords[1]) /
                     2));
            break;
        case 0x2D:
            work->limbPose = 2;
            break;
        case 0x38:
            work->limbPose = 4;
            break;
        case 0x44:
            work->limbPose = 3;
            break;
        case 0xDC:
            work->state = 0xA;
            break;
    }

    if ((u32)((u16)work->stateTicks - 0x29) < 6 && gGluttonLimbReach < 0x1770) {
        gGluttonLimbReach = (u16)gGluttonLimbReach + 0x258;
    }

    _gluttonTickAnim(arg0);

    if (_actor444000HasPlayerContact(work->swipeContacts, ARRAY_SIZE(work->swipeContacts)) && TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &D_actor_444000_80161928.hold, 0) == 0) {
        target                 = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        work->swipeDamageReply = taskMessageDispatch(target, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, 4), 0);
        if (work->swipeDamageReply == 1) {
            ((GameActor*)player->work)->state = 0xA;
        }
        work->playerAnim.source.sets = D_actor_444000_80161670;
        work->playerCaught           = 1;
        work->playerAnim.animationId = 2;
        work->playerAnim.blend       = ANIMATION_BLEND_RESET;
        work->playerAnim.blendFrames = 0;
        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
        work->caughtTicks = 0;
    }

    mode = work->playerCaught;
    if (mode == 1 && work->state != 0xD) {
        count             = work->caughtTicks + 1;
        work->caughtTicks = count;
        if (work->swipeDamageReply == mode) {
            if (work->playerAnim.animationId == 2) {
                work->playerAnim.source.sets = D_actor_444000_80161670;
                work->playerAnim.blend       = ANIMATION_BLEND_RESET;
                work->playerAnim.blendFrames = 0;
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
                work->caughtTicks = 0;
            }
        } else if (work->playerAnim.animationId == 2 && (s16)count < 0x28) {
            work->playerAnim.source.sets = D_actor_444000_80161670;
            work->playerAnim.blend       = ANIMATION_BLEND_RESET;
            work->playerAnim.blendFrames = 0;
            TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
        }

        if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
            switch (work->playerAnim.animationId) {
                case 2:
                    if (work->swipeDamageReply != 1 && (s16)work->caughtTicks >= 0x17) {
                        work->playerAnim.source.sets = D_actor_444000_80161670;
                        D_actor_444000_80161670[4]   = (Gp_PlayerAnimBlkTbl
                                                          [Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon])
                                                         ->table.sets[7];
                        work->playerAnim.animationId = 4;
                        work->playerAnim.blend       = ANIMATION_BLEND_INTERPOLATE;
                        work->playerAnim.blendFrames = 3;
                        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
                        work->caughtTicks = 0;
                    }
                    break;
                case 4:
                    if (work->swipeDamageReply != 1) {
                        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 2, 0);
                        work->playerCaught = 0;
                    }
                    break;
            }
        }
    }

    if (work->stateTicks == 0x3C && work->animId == 4) {
        work->animId   = 5;
        work->animStep = GLUTTON_ANIM_STEP_BLEND;
    }

    if (work->stateTicks >= 0x39) {
        if (gGluttonLimbReach >= 0xBB9) {
            gGluttonLimbReach = (u16)gGluttonLimbReach - 0xC8;
        } else {
            gGluttonLimbReach = (u16)gGluttonLimbReach - 0x1E;
        }
    }

    SCRATCH_STACK_RELEASE_BYTES(0x30);
}

/// Per-tick state of the arena fight once it is under way. A reset request
/// re-arms the block on animation 0xB, clears the host model's flag word and
/// pushes it onto each of the seven escorts' models, then plays the entry cue.
///
/// Sub-states 0x3B and 0x3C each fire a one-shot cue positioned at the first
/// escort's second coordinate. From 0x3D on the fight also drops debris: every
/// fifth step one of the three shared coordinates in
/// `D_actor_444000_80161948.coords` is rebuilt at that escort's second part -- its
/// rotation accumulated up the parent chain, its origin carried into view
/// space, then turned a quarter turn each way so `gfxReadMatrixZAxis` yields the
/// launch direction, which is normalised and scaled to 0x320 before being
/// added to the origin -- and an effect is spawned on it. Every tenth step a
/// fresh enemy is spawned from `gGluttonEscortTasks` and remembered in
/// `lastSpawned`.
///
/// The tick then runs the ordinary re-arm and hands over to state 0xA once the
/// second animation slot raises its flag.
static void func_actor_444000_801404C0(Task* arg0)
{
    GluttonWork* work;
    GluttonWork* escorts;
    Enemy*       enemy;
    Enemy*       spawned;
    GfxCoord*    coord;
    SVECTOR      pos;
    SVECTOR*     posp;
    s16          i;
    s32          resetId;
    s32          resetPan;
    s32          cueId;
    s32          cuePan;
    s32          hitId;
    s32          hitPan;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;

    if (work->stateChanged != 0) {
        work->lastAttack       = 7;
        work->animId           = 0xB;
        work->animStep         = GLUTTON_ANIM_STEP_RESTART;
        escorts                = arg0->work;
        escorts->freeCountdown = 0;

        arg0->extra.tmd->flags = 0;
        for (i = 0; i < ARRAY_SIZE(escorts->escorts); i++) {
            if (escorts->escorts[i] != NULL) {
                escorts->escorts[i]->task->extra.tmd->flags = arg0->extra.tmd->flags;
            }
        }
        work->neckPitchEnabled = 1;
        work->neckYawEnabled   = 1;
        work->hostExposed      = 0;

        resetId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200017;
        resetPan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(resetId, resetPan,
                                 (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }

    if (work->stateTicks == 0x3B) {
        cueId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200016;
        cuePan = (s8)worldCoordGetOriginAudioPan(&work->escorts[0]->task->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(cueId, cuePan,
                                 (s8)worldCoordGetOriginAudioDepth(&work->escorts[0]->task->extra.tmd->coords[1]));
    }

    if (work->stateTicks == 0x3C) {
        hitId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000D;
        hitPan = (s8)worldCoordGetOriginAudioPan(&work->escorts[0]->task->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(hitId, hitPan,
                                 (s8)worldCoordGetOriginAudioDepth(&work->escorts[0]->task->extra.tmd->coords[1]));
    }

    if ((s16)(u16)work->stateTicks >= 0x3D) {
        if ((s16)((s16)(u16)work->stateTicks % 5) == 0) {
            if (D_actor_444000_80161850 >= ACTOR_444000_DEBRIS_COORD_COUNT - 1) {
                D_actor_444000_80161850 = 0;
            } else {
                D_actor_444000_80161850 = (u16)D_actor_444000_80161850 + 1;
            }

            actorAccumulateToView(&work->escorts[0]->task->extra.tmd->coords[1],
                                  &D_actor_444000_80161948.coords[D_actor_444000_80161850].coord);
            D_actor_444000_80161948.coords[D_actor_444000_80161850].parent = &gGfxViewCoord;

            pos.vz = 0;
            pos.vy = 0;
            pos.vx = 0;
            actorLocalToView(&work->escorts[0]->task->extra.tmd->coords[1], &pos);

            D_actor_444000_80161948.coords[D_actor_444000_80161850].coord.t[0] = pos.vx;
            D_actor_444000_80161948.coords[D_actor_444000_80161850].coord.t[1] = pos.vy;
            D_actor_444000_80161948.coords[D_actor_444000_80161850].coord.t[2] = pos.vz;
            gfxRotMatrixY(&D_actor_444000_80161948.coords[D_actor_444000_80161850].coord, 0x80, 0);
            gfxRotMatrixX(&D_actor_444000_80161948.coords[D_actor_444000_80161850].coord, -0x80, GRAPHICS_ROTATION_COMPOSE);
            gfxReadMatrixZAxis(&D_actor_444000_80161948.coords[D_actor_444000_80161850].coord, &pos);

            posp   = &pos;
            pos.vy = 0;
            VectorNormalSS(posp, posp);

            gte_lddp(0x320);
            gte_ldsv(posp);
            gte_gpf12();
            gte_stsv(posp);

            coord               = &D_actor_444000_80161948.coords[D_actor_444000_80161850];
            coord->coord.t[0]  += pos.vx;
            coord->coord.t[1]  += pos.vy;
            coord->coord.t[2]  += pos.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            effectSpawn(EFFECT_196, &D_actor_444000_80161948.coords[D_actor_444000_80161850], 0x27A0D600, NULL);
        }
        if ((s16)((s16)(u16)work->stateTicks % 10) == 4) {
            spawned           = enemySpawnFromTable(gGluttonEscortTasks, 2, 0, arg0->spawnArg2.pointer);
            spawned->workType = ENEMY_WORK_PLAIN;
            work->lastSpawned = spawned;
        }
    }

    _gluttonTickAnim(arg0);

    if (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = 0xA;
        sndEvtRequestScriptStop((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0D), SOUND_SCRIPT_STOP_KEEP_RELEASE);
    }
}

/// Tick of the arena fight state that runs alongside `func_actor_444000_80140E28`:
/// on a reset request it clears the host model's flag word, pushes it onto each
/// of the seven escorts' models, makes sure the host and every escort has its
/// model buffers allocated and restores the normal blend weight.
///
/// The three state checks that follow are independent. In state 0xD the 0x18
/// script is spawned once, on the step the third animation slot first reaches
/// frame 0x15, which `clip.prevSlot2Cue` remembers so the spawn does not repeat while
/// the frame is held. In state 9 at sub-state 0x2D the shared coordinate
/// `D_actor_444000_801618B8` is rebuilt as an identity sitting 100 units below
/// and 100 in front of the host model's fifth part, which it is parented to.
/// State 0x14 hands over to state 0xD once the second slot raises its flag.
/// The tick then runs the ordinary re-arm and re-flags the root coordinate for
/// rebuild.
static void func_actor_444000_80140BBC(Task* arg0)
{
    GluttonWork* work;
    GluttonWork* escorts;
    GluttonWork* buffers;
    TmdObject*   tmd;
    TmdObject*   escortTmd;
    GfxMatrix*   mtx;
    GfxCoord*    coords;
    s16          i;
    s16          j;
    s32          frame;

    work = arg0->work;
    if (work->stateChanged != 0) {
        escorts                = arg0->work;
        escorts->freeCountdown = 0;
        arg0->extra.tmd->flags = 0;
        for (i = 0; i < ARRAY_SIZE(escorts->escorts); i++) {
            if (escorts->escorts[i] != NULL) {
                escorts->escorts[i]->task->extra.tmd->flags = arg0->extra.tmd->flags;
            }
        }
        tmd     = arg0->extra.tmd;
        buffers = arg0->work;
        if (tmd->buffer == NULL) {
            tmdAllocPrimitiveBuffer(tmd);
        }
        for (j = 0; j < ARRAY_SIZE(buffers->escorts); j++) {
            if (buffers->escorts[j] != NULL) {
                escortTmd = buffers->escorts[j]->task->extra.tmd;
                if (escortTmd->buffer == NULL) {
                    tmdAllocPrimitiveBuffer(escortTmd);
                }
            }
        }
        work->animRate = 0x10;
    }
    if (work->animId == 0xD) {
        frame = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (frame == 0x15 && work->clip.prevSlot2Cue != frame) {
            work->shakeLevel = GLUTTON_SHAKE_LONG;
            padScriptSpawn(D_actor_444000_80144A84, D_actor_444000_80144A8C);
        }
        work->clip.prevSlot2Cue = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    }
    if (work->animId == 9 && work->stateTicks == 0x2D) {
        coords                                                    = arg0->extra.tmd->coords;
        D_actor_444000_801618B8.packed.coord.rotationWords.m00M01 = ONE;
        mtx                                                       = &D_actor_444000_801618B8.packed.coord;
        mtx->rotationWords.m02M10                                 = 0;
        mtx->rotationWords.m11M12                                 = ONE;
        mtx->rotationWords.m20M21                                 = 0;
        mtx->rotationWords.m22                                    = ONE;
        D_actor_444000_801618B8.node.coord.t[1]                   = -0x64;
        D_actor_444000_801618B8.node.coord.t[0]                   = 0;
        D_actor_444000_801618B8.node.coord.t[2]                   = 0x64;
        D_actor_444000_801618B8.node.composeStamp                 = GRAPHICS_COORD_DIRTY;
        D_actor_444000_801618B8.node.parent                       = &coords[4];
        actorRenderComposeCoord(&D_actor_444000_801618B8.node);
    }
    if (work->animId == 0x14 && (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        work->animId   = 0xD;
        work->animStep = GLUTTON_ANIM_STEP_BLEND;
    }
    if (gGluttonLimbReach >= 0x191) {
        gGluttonLimbReach = (u16)gGluttonLimbReach - 0xC8;
    }
    _gluttonTickAnim(arg0);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Reset handler for the arena fight: on a reset request, clear the host
/// model's flag word, push it onto each of the seven escorts' models, make sure
/// the host and every escort has its model buffers allocated, then fast-forward
/// the animation by running the re-arm step an eighth of `collapseSkip` times
/// before restoring the normal blend weight and playing the entry cue.
///
/// Either way the tick then runs the ordinary re-arm, re-flags the root
/// coordinate for rebuild, and spawns the 0x18 script once -- on the step the
/// second animation slot first reaches frame 0x1C, which `clip.prevSlot2Cue` remembers
/// so the spawn does not repeat while the frame is held.
static void func_actor_444000_80140E28(Task* arg0)
{
    GluttonWork* work;
    GluttonWork* escorts;
    GluttonWork* buffers;
    Enemy*       obj;
    TmdObject*   tmd;
    TmdObject*   escortTmd;
    s16          i;
    s16          j;
    s16          k;
    s32          frame;

    work = arg0->work;
    if (work->stateChanged != 0) {
        obj                    = arg0->spawnArg2.pointer;
        escorts                = arg0->work;
        escorts->freeCountdown = 0;
        arg0->extra.tmd->flags = 0;
        for (i = 0; i < ARRAY_SIZE(escorts->escorts); i++) {
            if (escorts->escorts[i] != NULL) {
                escorts->escorts[i]->task->extra.tmd->flags = arg0->extra.tmd->flags;
            }
        }
        tmd     = arg0->extra.tmd;
        buffers = arg0->work;
        if (tmd->buffer == NULL) {
            tmdAllocPrimitiveBuffer(tmd);
        }
        for (j = 0; j < ARRAY_SIZE(buffers->escorts); j++) {
            if (buffers->escorts[j] != NULL) {
                escortTmd = buffers->escorts[j]->task->extra.tmd;
                if (escortTmd->buffer == NULL) {
                    tmdAllocPrimitiveBuffer(escortTmd);
                }
            }
        }
        work->animRate         = 0x7F;
        work->neckPitchEnabled = 0;
        work->neckYawEnabled   = 0;
        for (k = 0; k < work->collapseSkip / 8; k++) {
            _gluttonTickAnim(arg0);
        }
        work->animRate = 0x10;
        sndEvtRequestScriptStop((((u16)obj->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0A), SOUND_SCRIPT_STOP_KEEP_RELEASE);
    }
    if (gGluttonLimbReach >= 0x191) {
        gGluttonLimbReach = (u16)gGluttonLimbReach - 0xC8;
    }
    _gluttonTickAnim(arg0);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    frame                                 = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    if (frame == 0x1C && work->clip.prevSlot2Cue != frame) {
        work->shakeLevel = GLUTTON_SHAKE_LONG;
        padScriptSpawn(D_actor_444000_80144A74, D_actor_444000_80144A7C);
    }
    work->clip.prevSlot2Cue = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
}

static void func_actor_444000_8014105C(Task* arg0)
{
    GluttonWork* work;
    Enemy*       obj;
    TmdObject*   tmd;
    s32          state;
    s32          id;
    s32          pan;

    work = arg0->work;
    if (work->stateChanged != 0) {
        tmd                         = arg0->extra.tmd;
        obj                         = arg0->spawnArg2.pointer;
        obj->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        tmd->flags                  = 0;
        state                       = work->animId;
        work->neckPitchEnabled      = 0;
        work->neckYawEnabled        = 0;
        work->hostExposed           = 1;
        if (state != 0xD) {
            work->animStep = GLUTTON_ANIM_STEP_BLEND;
            work->animId   = 0xD;
        } else {
            work->animStep = GLUTTON_ANIM_STEP_RESTART;
            work->animId   = state;
        }
        id  = (((u16)obj->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200004;
        pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(id, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        sndEvtRequestScriptStop((((u16)obj->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0D), SOUND_SCRIPT_STOP_KEEP_RELEASE);
        return;
    }
    SCRATCH_STACK_RESERVE_BYTES(0xC);
    if (gGluttonLimbReach >= 0x191) {
        work->limbPose    = 0;
        gGluttonLimbReach = (u16)gGluttonLimbReach - 0xC8;
    }
    _gluttonTickAnim(arg0);
    if (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = 0xA;
    }
    SCRATCH_STACK_RELEASE_BYTES(0xC);
}

/// Idle/approach tick of the arena fight: re-arms the block on request, keeps
/// the boss yawed at `gPlayerStatus.coordMtx` (the player's coordinate matrix)
/// and then picks the state to run next.
///
/// `neckYawTarget` is that yaw, relative to the host part's own facing and wrapped
/// into +/-0x800. `attackDelay` is a stagger countdown -- while it is positive the
/// tick only spins it down, and the reset arms it to 0x28 if it is not already
/// running.
///
/// The choice is a ladder: `summonsAlive` picks state 3 outright, then each attack
/// pattern in `phase` has a depth the player has to be past before the
/// fight advances to state 9, the last two only while the boss still has HP in
/// hand. Failing all of those, `pendingHeals` picks 0xF and pattern 6 picks 7, and
/// otherwise the distance from the player to a point just in front of the host
/// picks between 3, 7 and 0xB on a coin flip off `gRandomLcgState`.
///
/// `coord` and `facing` are the same coordinate read twice on purpose: the
/// stores into `vec` cut the first read's value, and the second read has to
/// outlive the first `ratan2` call.
static void func_actor_444000_801411C8(Task* arg0)
{
    GluttonHitScratch* sc;
    GluttonWork*       work;
    Enemy*             enemy;
    Task*              player;
    GfxCoord*          coord;
    SVECTOR            vec;
    SVECTOR*           d;

    work   = arg0->work;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    enemy  = arg0->spawnArg2.pointer;

    if (work->stateChanged != 0) {
        work->neckYawEnabled   = 1;
        work->neckPitchEnabled = 1;
        work->neckPitchTarget  = 0;
        if (work->attackDelay == 0) {
            work->attackDelay = 0x28;
        }
        work->animId      = 1;
        work->animStep    = GLUTTON_ANIM_STEP_BLEND;
        work->hostExposed = 0;
    }

    d                   = &vec;
    coord               = arg0->extra.tmd->coords;
    d->vx               = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    d->vy               = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    d->vz               = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    work->neckYawTarget = actorYawTo(arg0->extra.tmd->coords, d->vx, d->vz);

    if (gGluttonLimbReach >= 0x191) {
        gGluttonLimbReach = (u16)gGluttonLimbReach - 0xC8;
        work->limbPose    = 0;
    }
    _gluttonTickAnim(arg0);

    if (work->attackDelay > 0) {
        work->attackDelay = work->attackDelay - 1;
        return;
    }
    if (work->summonsAlive > 0) {
        work->state = 3;
        return;
    }

    if (work->phase == 0) {
        work->state = 9;
        return;
    }
    if (work->phase == 1 && player->extra.tmd->coords->coord.t[2] < -0x1D4C) {
        work->state = 9;
        return;
    }
    if (work->phase == 2) {
        work->state = 9;
        return;
    }
    if (work->phase == 3 && player->extra.tmd->coords->coord.t[2] < -0x30D4) {
        work->state = 9;
        return;
    }
    if (work->phase == 4 && player->extra.tmd->coords->coord.t[2] < -0x3DB8 &&
        enemy->hp < 0x9C4) {
        work->state = 9;
        return;
    }
    if (work->phase == 5 && player->extra.tmd->coords->coord.t[2] < -0x4268 &&
        enemy->hp < 0x7D0) {
        work->state = 9;
        return;
    }

    if ((s8)work->pendingHeals > 0) {
        work->state = 0xF;
        return;
    }
    if (work->phase == 6) {
        work->state = 7;
        return;
    }

    sc              = SCRATCH_STACK_RESERVE_BLOCK(GluttonHitScratch);
    sc->toPlayer.vx = player->extra.tmd->coords->coord.t[0] -
                      arg0->extra.tmd->coords->coord.t[0] - 0x51F;
    sc->toPlayer.vy = player->extra.tmd->coords->coord.t[1] -
                      arg0->extra.tmd->coords->coord.t[1] - 0xFA;
    sc->toPlayer.vz = player->extra.tmd->coords->coord.t[2] -
                      arg0->extra.tmd->coords->coord.t[2] + 0x25F;
    sc->playerDistance = SquareRoot0(sc->toPlayer.vx * sc->toPlayer.vx + sc->toPlayer.vy * sc->toPlayer.vy +
                                     sc->toPlayer.vz * sc->toPlayer.vz);
    if (sc->playerDistance < 0x2329) {
        if (sc->playerDistance >= 0xED9) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                work->state = 7;
            } else {
                work->state = 3;
            }
        } else {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                work->state = 3;
            } else {
                work->state = 0xB;
            }
        }
    } else {
        work->state = 3;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GluttonHitScratch);
}

/// Summon tick of the arena fight: re-arms the block on request and, on
/// that first pass, tops the two `summons` slots back up to two live
/// enemies, seeding each one's model texture page from the current area record
/// and stamping its slot index into `Enemy::placeKey`. Every tick it then
/// yaws the host at the player, and at sub-state 0x46 / 0x78 it sends summon 0
/// or 1 a 0x7DB order whose action is picked from `phase` and a coin flip.
static void func_actor_444000_80141618(Task* task)
{
    GluttonSummonScratch* sc;
    GluttonWork*          work;
    Enemy*                host;
    Enemy*                escort;
    PlayerStatus*         cfg;
    GfxCoord*             coord;
    TmdObject*            model;
    AreaPlacement*        entry;
    GameLocationKey       key;
    GameLocationKey*      sessionKey;
    s32                   cueId;
    s32                   cuePan;
    s32                   blastId;
    s32                   blastPan;
    s32                   rnd;
    s32                   state;
    u32                   frame;

    sc   = SCRATCH_STACK_RESERVE_BLOCK(GluttonSummonScratch);
    work = task->work;
    host = task->spawnArg2.pointer;
    if (work->stateChanged != 0) {
        work->neckPitchEnabled = 0;
        work->neckYawEnabled   = 1;
        state                  = work->animId;
        work->animRate         = 0x10;
        work->hostExposed      = 0;
        if (state != 0x13) {
            work->animId   = 0x13;
            work->animStep = GLUTTON_ANIM_STEP_BLEND;
        } else {
            work->animStep = GLUTTON_ANIM_STEP_RESTART;
            work->animId   = state;
        }
        for (sc->slot = 0; sc->slot < 2; sc->slot++) {
            if (work->summons[sc->slot] == NULL && (u8)work->summonsSpawned < 8 && work->phase < 6) {
                work->summons[sc->slot] = enemySpawnFromTable(&Actor04400_D107E4, 3, 2, NULL);
                if (work->summons[sc->slot] != NULL) {
                    work->summonsSpawned++;
                    model      = work->summons[sc->slot]->task->extra.tmd;
                    sessionKey = &gGameSession->location.loc;
                    key.stage  = sessionKey->stage;
                    key.area   = sessionKey->area;
                    key.room   = sessionKey->room;
                    key.view   = sessionKey->view;
                    areaSyncLocationVariant(&key);
                    entry                    = &Gp_GetNestedAreaRec(&key)->placements[2];
                    model->texturePageOffset = entry->texturePageOffset;
                    model->clutRowOffset     = entry->clutRowOffset;
                    if (model->buffer != NULL) {
                        tmdBuildBufferHalf(model);
                        tmdBuildBufferHalf(model);
                    }
                    work->summons[sc->slot]->workType = ENEMY_WORK_PLAIN;
                    escort                            = work->summons[sc->slot];
                    escort->placeKey                 |= sc->slot << ENEMY_PLACE_INDEX_SHIFT;
                    work->summonsAlive++;
                }
            }
        }
        work->wallDistanceTarget = 0xC80;
        sndEvtRequestScriptStop((((u16)host->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0D), SOUND_SCRIPT_STOP_KEEP_RELEASE);
    }
    if (gGluttonLimbReach >= 0x191) {
        gGluttonLimbReach = (u16)gGluttonLimbReach - 0xC8;
    }
    _gluttonTickAnim(task);
    if (work->animId == 0x13 && (frame = work->hostRig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 4 && frame < 0xD) {
        work->hostExposed = 1;
    } else {
        work->hostExposed = 0;
    }
    cfg                 = &gPlayerStatus;
    coord               = task->extra.tmd->coords;
    sc->toPlayer.vx     = cfg->coordMtx->t[0] - coord->coord.t[0];
    sc->toPlayer.vy     = cfg->coordMtx->t[1] - coord->coord.t[1];
    sc->toPlayer.vz     = cfg->coordMtx->t[2] - coord->coord.t[2];
    work->neckYawTarget = actorYawTo(task->extra.tmd->coords, sc->toPlayer.vx, sc->toPlayer.vz);
    if ((work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) && work->animId == 0x13) {
        work->animId   = 1;
        work->animStep = GLUTTON_ANIM_STEP_BLEND;
        work->animRate = 0x10;
        _gluttonTickAnim(task);
    }
    if (work->stateTicks >= 0x14B || (work->animId == 1 && work->summonsAlive == 0)) {
        work->state = 3;
    }
    if (work->stateTicks == 6) {
        cueId  = (((u16)host->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200004;
        cuePan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(cueId, cuePan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    if (work->stateTicks == 0x3B) {
        blastId  = (((u16)host->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200010;
        blastPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(blastId, blastPan, (s8)(worldCoordGetOriginAudioDepth(task->extra.tmd->coords) / 2));
        work->shakeLevel = GLUTTON_SHAKE_LONG;
        padScriptSpawn(D_actor_444000_80144A74, D_actor_444000_80144A7C);
    }
    if (work->stateTicks == 0x46 || work->stateTicks == 0x78) {
        if (work->stateTicks == 0x46) {
            sc->slot = 0;
        } else {
            sc->slot = 1;
        }
        if (work->summons[sc->slot] != NULL && work->phase < 6) {
            D_actor_444000_80161888.command.context.loc.stage = 0;
            D_actor_444000_80161888.command.context.loc.area  = 0x2C;
            switch (work->phase) {
                case 0:
                case 1:
                    if (sc->slot == 0) {
                        gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                        if (!((gRandomLcgState >> 16) & 1)) {
                            D_actor_444000_80161888.command.command = 3;
                        } else {
                            D_actor_444000_80161888.command.command = 4;
                        }
                    } else {
                        gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                        if (!((gRandomLcgState >> 16) & 1)) {
                            D_actor_444000_80161888.command.command = 5;
                        } else {
                            D_actor_444000_80161888.command.command = 6;
                        }
                    }
                    break;
                case 2:
                case 3:
                    if (sc->slot == 0) {
                        gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                        if (!((gRandomLcgState >> 16) & 1)) {
                            D_actor_444000_80161888.command.command = 0xd;
                        } else {
                            D_actor_444000_80161888.command.command = 8;
                        }
                    } else {
                        gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                        if (!((gRandomLcgState >> 16) & 1)) {
                            D_actor_444000_80161888.command.command = 7;
                        } else {
                            D_actor_444000_80161888.command.command = 0xe;
                        }
                    }
                    break;
                case 4:
                case 5:
                    if (sc->slot == 0) {
                        gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                        if (((gRandomLcgState >> 16) & 1)) {
                            D_actor_444000_80161888.command.command = 9;
                        } else {
                            D_actor_444000_80161888.command.command = 0xf;
                        }
                    } else {
                        gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                        if (!((gRandomLcgState >> 16) & 1)) {
                            D_actor_444000_80161888.command.command = 9;
                        } else {
                            D_actor_444000_80161888.command.command = 0xf;
                        }
                    }
                    break;
            }
            D_actor_444000_80161888.command.command <<= 8;
            rnd                                       = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            D_actor_444000_80161888.command.command  |= (s16)(((((u32)rnd >> 16) % 3) * 0x10) | 1);
            gRandomLcgState                           = rnd;
            TASK_MESSAGE_DISPATCH_POINTER(work->summons[sc->slot]->task, ACTOR_COMMAND_MESSAGE_APPLY, &D_actor_444000_80161888.command, 0);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(GluttonSummonScratch);
}

#include "../../shared/glutton_escort_state.inc.c"

/// Keeps the player inside the arena: clamps the player model's root
/// translation every tick. `t[1]` (height) is never allowed above 0, and `t[2]`
/// (depth) is capped at 0 in front and -26000 at the back. The `t[2]` ladder
/// then picks the `t[0]` (lateral) corridor for that depth band, so the walls
/// narrow and widen as the player moves through the room.
static void func_actor_444000_80142254(void)
{
    Task* player;
    s32   z;

    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);

    if (player->extra.tmd->coords->coord.t[1] > 0) {
        player->extra.tmd->coords->coord.t[1] = 0;
    }

    z = player->extra.tmd->coords->coord.t[2];
    if (z > 0) {
        player->extra.tmd->coords->coord.t[2] = 0;
    } else if (z > -1000) {
        if (player->extra.tmd->coords->coord.t[0] < 0) {
            player->extra.tmd->coords->coord.t[0] = 0;
        }
        if (player->extra.tmd->coords->coord.t[0] > 16500) {
            player->extra.tmd->coords->coord.t[0] = 16500;
        }
    } else if (z > -5000) {
        if (player->extra.tmd->coords->coord.t[0] < 0) {
            player->extra.tmd->coords->coord.t[0] = 0;
        }
        if (player->extra.tmd->coords->coord.t[0] > 16500) {
            player->extra.tmd->coords->coord.t[0] = 16500;
        }
    } else if (z > -7000) {
        if (player->extra.tmd->coords->coord.t[0] < 9500) {
            player->extra.tmd->coords->coord.t[0] = 9500;
        }
        if (player->extra.tmd->coords->coord.t[0] > 16500) {
            player->extra.tmd->coords->coord.t[0] = 16500;
        }
    } else if (z > -13200) {
        if (player->extra.tmd->coords->coord.t[0] < 11500) {
            player->extra.tmd->coords->coord.t[0] = 11500;
        }
        if (player->extra.tmd->coords->coord.t[0] > 16500) {
            player->extra.tmd->coords->coord.t[0] = 16500;
        }
    } else if (z > -14750) {
        if (player->extra.tmd->coords->coord.t[0] < 11500) {
            player->extra.tmd->coords->coord.t[0] = 11500;
        }
        if (player->extra.tmd->coords->coord.t[0] > 17500) {
            player->extra.tmd->coords->coord.t[0] = 17500;
        }
    } else if (z > -21250) {
        if (player->extra.tmd->coords->coord.t[0] < 11500) {
            player->extra.tmd->coords->coord.t[0] = 11500;
        }
        if (player->extra.tmd->coords->coord.t[0] > 16500) {
            player->extra.tmd->coords->coord.t[0] = 16500;
        }
    } else if (z > -22800) {
        if (player->extra.tmd->coords->coord.t[0] < 11500) {
            player->extra.tmd->coords->coord.t[0] = 11500;
        }
        if (player->extra.tmd->coords->coord.t[0] > 17500) {
            player->extra.tmd->coords->coord.t[0] = 17500;
        }
    } else if (z > -26000) {
        if (player->extra.tmd->coords->coord.t[0] < 11500) {
            player->extra.tmd->coords->coord.t[0] = 11500;
        }
        if (player->extra.tmd->coords->coord.t[0] > 16500) {
            player->extra.tmd->coords->coord.t[0] = 16500;
        }
    } else {
        player->extra.tmd->coords->coord.t[2] = -26000;
    }
}

/// Per-frame body of the boss task: refreshes the host model's coordinate,
/// times out the "model buffers freed" countdown, re-lights the host and its
/// escorts, then runs the state handler `GluttonWork::state` selects and
/// republishes every collision group.
///
/// `gSceneCombatState.actorControl` gates how much of that runs. While the controller task is
/// suspended (1 or 2) the tick only pushes the host's `TmdObject::flags`
/// onto the escorts and clears the collision tables, and returns; only the
/// running case (0) and anything else falls through to the state machine.
/// Within the suspended cases the view index decides whether that flag word is
/// 0x80 (hidden) or 0.
///
/// `freeCountdown` is a countdown armed when the fight hides the models: while it
/// runs the host is flagged hidden, and the step that takes it to zero also
/// raises bit 2 and hands every model's buffers back with `tmdFreePrimitiveBuffer`.
///
/// `hostExposed` selects which of the two bodies is the "live" one -- the host
/// (`enemy`) or escort 3 (`escorts[3]`) -- and that choice drives the colour
/// update, the link-node slots and which collision groups publish their
/// `0x8000` bit this frame. States 0, 1, 5, 0xC, 0x12 and 0x13 are the inert
/// ones: they park both bodies on slot 1 and clear every group.
///
/// `deathTicks` is the death timer, only started once the host's HP is gone:
/// step 0 tells the scene (message 0x7DA, action 0x2C) and latches
/// `gGluttonEnded`, step 3 tells the player's task (0x13F4) and moves
/// the fight to state 0x12 with the two death cues.
///
/// The dispatch table is a local, as in `func_actor_444000_80142F28`.
static void func_actor_444000_801423C4(Enemy* enemy, Task* task)
{
    PlayerStatus* cfg  = &gPlayerStatus;
    GluttonWork*  work = task->work;
    VECTOR        pos;
    TaskFunc      handlers[0x15] = {
        func_actor_444000_8013D810,
        func_actor_444000_80143F4C,
        NULL,
        func_actor_444000_8013E058,
        NULL,
        func_actor_444000_80140BBC,
        NULL,
        func_actor_444000_801404C0,
        func_actor_444000_8014105C,
        func_actor_444000_8013482C,
        func_actor_444000_801411C8,
        func_actor_444000_8013FB74,
        func_actor_444000_80140E28,
        func_actor_444000_8013EC84,
        func_actor_444000_80141618,
        gluttonEscortState,
        func_actor_444000_801434C4,
        func_actor_444000_801435CC,
        func_actor_444000_80135448,
        func_actor_444000_8013D96C,
        NULL,
    };
    GluttonWork* escorts;
    GluttonWork* flagged;
    s32          view;
    s16          i;
    s16          j;

    view                                    = viewGetMappedIndex() & 0xFF;
    task->extra.tmd->coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&task->extra.tmd->coords[0]);

    escorts = task->work;
    if (escorts->freeCountdown != 0) {
        task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        if (--escorts->freeCountdown == 0) {
            task->extra.tmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            tmdFreePrimitiveBuffer(task->extra.tmd);
            for (j = 0; j < ARRAY_SIZE(escorts->escorts); j++) {
                if (escorts->escorts[j] != NULL) {
                    escorts->escorts[j]->task->extra.tmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
                    tmdFreePrimitiveBuffer(escorts->escorts[j]->task->extra.tmd);
                }
            }
        }
    }

    func_actor_444000_80142254();

    pos.vx = task->extra.tmd->coords[3].workm.t[0];
    pos.vy = task->extra.tmd->coords[3].workm.t[1];
    pos.vz = task->extra.tmd->coords[3].workm.t[2];

    if (work->hostExposed != work->prevHostExposed) {
        worldCoordUpdateActorColor(enemy, &pos, 0, 0);
        worldCoordUpdateActorColor(work->escorts[3], &pos, 0, 0);
        work->prevHostExposed = work->hostExposed;
    }
    worldCoordUpdateActorColor(work->hostExposed != 0 ? enemy : work->escorts[3], &pos, 0, 0);

    if (work->state == 0xB) {
        work->escorts[4]->task->extra.tmd->otOffset = -1;
    } else {
        work->escorts[4]->task->extra.tmd->otOffset = 0;
    }

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->state != 0) {
                if (view == 9) {
                    flagged                = task->work;
                    flagged->freeCountdown = 0;
                    task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    for (i = 0; i < ARRAY_SIZE(flagged->escorts); i++) {
                        if (flagged->escorts[i] != NULL) {
                            flagged->escorts[i]->task->extra.tmd->flags =
                                task->extra.tmd->flags;
                        }
                    }
                } else {
                    flagged                = task->work;
                    flagged->freeCountdown = 0;
                    task->extra.tmd->flags = 0;
                    for (i = 0; i < ARRAY_SIZE(flagged->escorts); i++) {
                        if (flagged->escorts[i] != NULL) {
                            flagged->escorts[i]->task->extra.tmd->flags =
                                task->extra.tmd->flags;
                        }
                    }
                }
            }
            break;

        case SCENE_COMBAT_ACTORS_PAUSED:
            if (work->state != 0) {
                if (view == 9) {
                    flagged                = task->work;
                    flagged->freeCountdown = 0;
                    task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    for (i = 0; i < ARRAY_SIZE(flagged->escorts); i++) {
                        if (flagged->escorts[i] != NULL) {
                            flagged->escorts[i]->task->extra.tmd->flags =
                                task->extra.tmd->flags;
                        }
                    }
                } else {
                    flagged                = task->work;
                    flagged->freeCountdown = 0;
                    task->extra.tmd->flags = 0;
                    for (i = 0; i < ARRAY_SIZE(flagged->escorts); i++) {
                        if (flagged->escorts[i] != NULL) {
                            flagged->escorts[i]->task->extra.tmd->flags =
                                task->extra.tmd->flags;
                        }
                    }
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
            flagged                = task->work;
            flagged->freeCountdown = 0;
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            for (i = 0; i < ARRAY_SIZE(flagged->escorts); i++) {
                if (flagged->escorts[i] != NULL) {
                    flagged->escorts[i]->task->extra.tmd->flags =
                        task->extra.tmd->flags;
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
    }

    SCRATCH_STACK_RESERVE_BYTES(0x1C);

    if (enemy->hp > 0) {
        if (work->playerCaught != 1 && cfg->hp > 0 && work->state != 0xD) {
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
                func_actor_444000_8013CA60(task);
            }
            if (work->groups6To8Cooldown > 0) {
                work->groups6To8Cooldown--;
            } else {
                _gluttonHitGroups6To8(task);
            }
        }
    }
    if (enemy->hp <= 0) {
        if (cfg->hp <= 0) {
            enemy->hp     = 1;
            gGluttonEnded = 0;
        }
        if (enemy->hp <= 0 && work->state != 0) {
            switch (work->deathTicks) {
                case 0:
                    gGluttonEnded                                     = 1;
                    D_actor_444000_80161888.command.context.loc.stage = 0;
                    D_actor_444000_80161888.command.context.loc.area  = 0x2C;
                    D_actor_444000_80161888.command.command           = 3;
                    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_actor_444000_80161888.command, ACTOR_COMMAND_MESSAGE_APPLY);
                    break;

                case 3:
                    if (cfg->hp > 0) {
                        if (work->phase == 6) {
                            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, 2, 0);
                        } else {
                            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, 1, 0);
                        }
                        work->state = 0x12;
                        sndEvtRequestScriptStop(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0A), SOUND_SCRIPT_STOP_KEEP_RELEASE);
                        sndEvtRequestScriptStop(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0D), SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    }
                    break;
            }
            if (work->deathTicks < 0x100) {
                work->deathTicks++;
            }
        }
    }

    if (work->prevState != work->state) {
        work->stateChanged = 1;
        work->stateTicks   = 0;
    } else {
        if (work->stateTicks < 0x7FFF) {
            work->stateTicks++;
        }
        work->stateChanged = 0;
    }
    work->prevState = work->state;
    handlers[work->state](task);

    if ((u16)work->state < 2 || work->state == 5 || work->state == 0x12 ||
        work->state == 0x13 || work->state == 0xC) {
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

    if (work->state != 0 && work->state != 0x12 && work->state != 0x13 &&
        work->state != 5 && work->state != 0xC && work->hostExposed == 1) {
        work->hits[0].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->hits[0].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }

    if (work->state != 0 && work->state != 0x12 && work->state != 0x13 &&
        work->state != 5 && work->state != 0xC && work->hostExposed != 1) {
        work->hits[1].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->hits[2].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->hits[1].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->hits[2].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }

    if (work->state != 0 && work->state != 5 && work->state != 0xC &&
        work->state != 0x13 && work->state != 0x12) {
        work->hits[3].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->hits[4].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->hits[5].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->hits[3].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->hits[4].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->hits[5].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }

    if (work->state != 0 && work->state != 5 && work->state != 0xC &&
        work->state != 0x13 && work->state != 0x12) {
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

    if (work->state != 5) {
        _gluttonShakeTick(task);
    }

    SCRATCH_STACK_RELEASE_BYTES(0x1C);
}

/// Puts the two floor quads `Gp_GridParams` keeps at vertices 24..31 back at
/// their default heights: 500 for each quad's first two vertices, 800 for the
/// other two.
static inline void _actor444000ResetFloorQuads(void)
{
    SVECTOR* verts;

    verts        = Gp_GridParams->vertices;
    verts[24].vy = 0x1F4;
    verts[25].vy = 0x1F4;
    verts[26].vy = 0x320;
    verts[27].vy = 0x320;
    verts[28].vy = 0x1F4;
    verts[29].vy = 0x1F4;
    verts[30].vy = 0x320;
    verts[31].vy = 0x320;
}

/// Per-frame tail of the arena fight: keeps the camera pulled back far enough
/// to hold both the boss and the player, then runs the state the task is in.
///
/// `wallDistance` is the camera distance actually in use and `wallDistanceTarget` the one
/// the current state asks for -- 0xBB8 while the boss is grappling (state 3),
/// 0xD48 for the close patterns and 0x1388 otherwise -- walked 0x32 per frame
/// until the two are within 0x33 of each other. `wallDrop` is the companion
/// height the floor-marker helpers take.
///
/// Most states hand that pair to `_gluttonBuildWall`, which rebuilds
/// grid quad 6 as a wall in front of the boss. The exception is pattern 1 in state 9: it uses
/// `func_actor_444000_801371E8` instead, floors the player's own x at 0x2CEC,
/// and pushes the player back by the boss part's view-space depth less 0x7D0 --
/// part 4 of the boss model carried up the coordinate chain by
/// `actorLocalToView`. Whether the camera distance is then added to x or
/// subtracted from z is the same split: patterns other than 1-in-state-9 widen
/// x, the rest pull z in, and pattern 2 additionally floors z at 0x251C.
///
/// States 0, 5, 0xC, 0x12 and 0x13 skip all of that. State 0 -- and any state
/// the calls above dropped back to 0 -- also resets the two floor quads
/// `Gp_GridParams` keeps at vertices 24..31 to their default heights, and
/// state 5 still wants the marker.
///
/// The dispatch table is a local: `Task::state` picks the spawn state, this
/// tick, or `enemyDestroy`.
void func_actor_444000_80142F28(Task* arg0)
{
    void (*handlers[3])(Enemy*, Task*) = {
        func_actor_444000_8013AFF8,
        func_actor_444000_801423C4,
        enemyDestroy,
    };
    SVECTOR      result;
    GluttonWork* work;
    Enemy*       enemy;
    Task*        player;
    s32          diff;
    s16          state;

    enemy  = arg0->spawnArg2.pointer;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    work   = arg0->work;
    if (work != NULL) {
        if (work->summons[0] != NULL && work->summons[0]->hp <= 0) {
            work->summons[0] = NULL;
        }
        if (work->summons[1] != NULL && work->summons[1]->hp <= 0) {
            work->summons[1] = NULL;
        }

        state = work->state;
        if (state == 3) {
            work->wallDistanceTarget = 0xBB8;
            work->wallDrop           = 0x190;
        } else if (state == 9) {
            work->wallDistanceTarget = 0x1388;
            work->wallDrop           = 0x190;
        } else if (state == 0x11) {
            work->wallDistanceTarget = 0x1388;
            work->wallDrop           = 0x190;
        } else if (work->phase != 0) {
            work->wallDistanceTarget = 0xD48;
            work->wallDrop           = 0x190;
        } else {
            work->wallDistanceTarget = 0x1388;
            work->wallDrop           = 0x190;
        }

        diff = work->wallDistanceTarget - work->wallDistance;
        if (diff < 0) {
            diff = -diff;
        }
        if (diff >= 0x33) {
            if (work->wallDistance < work->wallDistanceTarget) {
                work->wallDistance = (u16)work->wallDistance + 0x32;
            } else {
                work->wallDistance = (u16)work->wallDistance - 0x32;
            }
        } else {
            work->wallDistance = (u16)work->wallDistanceTarget;
        }

        state = work->state;
        if (state != 0) {
            /* Split so that `0x12` and `0x13` are not the innermost `&&` pair:
               `fold_range_test` would turn two adjacent constants into one
               `sltiu` range check. */
            if (state != 0x12) {
                if (state != 0x13 && state != 5 && state != 0xC) {
                    if (work->phase == 1 && state == 9) {
                        func_actor_444000_801371E8(arg0, work->wallDistance, 6);
                        {
                            GfxCoord* playerCoord = player->extra.tmd->coords;

                            if (playerCoord->coord.t[0] < 0x2CEC) {
                                playerCoord->coord.t[0] = 0x2CEC;
                            }
                        }
                        result.vx = result.vy = result.vz = 0;
                        actorLocalToView(arg0->extra.tmd->coords + 4, &result);
                        {
                            GfxCoord* playerCoord = player->extra.tmd->coords;
                            s32       z           = result.vz - 0x7D0;

                            if (z < playerCoord->coord.t[2]) {
                                playerCoord->coord.t[2] = z;
                            }
                        }
                    } else {
                        _gluttonBuildWall(arg0, work->wallDistance, work->wallDrop, 6);
                    }

                    if (work->phase == 0 || (work->phase == 1 && work->state != 9)) {
                        GfxCoord* playerCoord = player->extra.tmd->coords;
                        GfxCoord* selfCoord   = arg0->extra.tmd->coords;
                        s32       x           = work->wallDistance + selfCoord->coord.t[0];

                        if (playerCoord->coord.t[0] < x) {
                            playerCoord->coord.t[0] = x;
                        }
                    } else {
                        GfxCoord* playerCoord = player->extra.tmd->coords;
                        GfxCoord* selfCoord   = arg0->extra.tmd->coords;
                        s32       z           = selfCoord->coord.t[2] - work->wallDistance;

                        if (z < playerCoord->coord.t[2]) {
                            playerCoord->coord.t[2] = z;
                        }
                    }

                    if (work->phase == 2) {
                        GfxCoord* playerCoord = player->extra.tmd->coords;

                        if (playerCoord->coord.t[0] < 0x251C) {
                            playerCoord->coord.t[2] = 0x251C;
                        }
                    }
                }
            }
            /* Re-read: the calls above can drop the fight back to state 0. */
            if (work->state == 0) {
                _actor444000ResetFloorQuads();
            }
        } else {
            _actor444000ResetFloorQuads();
        }

        state = work->state;
        if (state == 5) {
            _gluttonBuildWall(arg0, work->wallDistance, work->wallDrop, 6);
        }
    }

    handlers[arg0->state](enemy, arg0);
}

#include "../../shared/glutton_quad_heights.inc.c"

#include "../../shared/glutton_exit.inc.c"

#include "../../shared/glutton_shake_level.inc.c"

#include "../../shared/glutton_set_spinners_released.inc.c"

#include "../../shared/glutton_get_spinners_released.inc.c"

/// The re-arm's counterpart: on a reset request it sets the two neck flags
/// and requests clip 1 rather than clearing them, and pushes `field_E = 2` onto
/// the first two escorts' model objects. Every tick it also parks one of two
/// yaw presets in `neckYawTarget`, alternating every 60 counts.
static void func_actor_444000_801434C4(Task* arg0)
{
    GluttonWork* work;
    s16          tick;

    work = arg0->work;
    if (work->stateChanged != 0) {
        work->neckPitchEnabled                      = 1;
        work->neckYawEnabled                        = 1;
        work->hostExposed                           = 0;
        work->animId                                = 1;
        work->animStep                              = GLUTTON_ANIM_STEP_BLEND;
        work->neckPitchTarget                       = 0;
        work->pendingHeals                          = 0;
        work->escorts[0]->task->extra.tmd->otOffset = 2;
        work->escorts[1]->task->extra.tmd->otOffset = 2;
        shelterB3GarbageIncineratorSetLiftCollisionWalls();
    }
    _gluttonTickAnim(arg0);
    tick = work->stateTicks;
    if (tick % 60 == 0) {
        if (tick % 120 == 0) {
            work->neckYawTarget = 0x2B2;
        } else {
            work->neckYawTarget = -0x1A2;
        }
    }
}

static void func_actor_444000_801435CC(Task* arg0)
{
    GluttonWork* work;
    Enemy*       obj;
    TmdObject*   tmd;
    s32          id;
    s32          pan;

    work = arg0->work;
    obj  = arg0->spawnArg2.pointer;
    if (work->stateChanged != 0) {
        tmd                         = arg0->extra.tmd;
        obj->node.state.parts.flags = 0;
        tmd->flags                  = 0;
        work->animId                = 0xC;
        work->animStep              = GLUTTON_ANIM_STEP_RESTART;
        work->neckPitchEnabled      = 0;
        work->neckYawEnabled        = 0;
        work->hostExposed           = 0;
        work->animRate              = 0x10;
        work->neckPitchTarget       = 0;
        work->stateTicks            = 0;
    }
    if (work->stateTicks == 0xA) {
        id  = ((obj->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200017;
        pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(id, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    _gluttonTickAnim(arg0);
    if (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = 9;
    }
}

#include "../../shared/glutton_prop_setup.inc.c"

#include "../../shared/glutton_prop_tick.inc.c"

#include "../../shared/glutton_prop_task.inc.c"

/// A further copy, under this file's own name.
#define gluttonPropTask func_actor_444000_80143888
#include "../../shared/glutton_prop_task.inc.c"
#undef gluttonPropTask

#include "../../shared/glutton_throw_task.inc.c"

#include "../../shared/glutton_glob_task.inc.c"

#include "../../shared/glutton_chunk_task.inc.c"

#include "../../shared/glutton_rain_task.inc.c"

#include "../../shared/glutton_spinner_wait.inc.c"

#include "../../shared/glutton_spinner_task.inc.c"

s32 func_actor_444000_80143D68(Task* arg0, s32 msgId, s32 arg2, s32 arg3)
{
    return ((Enemy*)arg0->spawnArg2.pointer)->hp > 0;
}

/// Seeds the enemy's `TmdObject` coordinate frame from `placement`: the three
/// longs become the translation, the Euler angles are applied X/Y/Z unless the
/// work block's state index is 0x12 or 0x13, and the coordinate is marked
/// dirty. Same body as `ActorsShared80135990` with that state gate added.
s32 func_actor_444000_80143D7C(Task* arg0, s32 arg1, ActorTransform* placement, s32 arg3)
{
    GluttonWork* work = arg0->work;

    arg0->extra.tmd->coords->coord.t[0] = placement->pos.vx;
    arg0->extra.tmd->coords->coord.t[1] = placement->pos.vy;
    arg0->extra.tmd->coords->coord.t[2] = placement->pos.vz;
    if ((u32)((u16)work->state - 0x12) >= 2U) {
        gfxRotMatrixX(&arg0->extra.tmd->coords->coord, placement->rot.vx, GRAPHICS_ROTATION_REPLACE);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, placement->rot.vy, 0);
        gfxRotMatrixZ(&arg0->extra.tmd->coords->coord, placement->rot.vz, GRAPHICS_ROTATION_COMPOSE);
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    return 1;
}

/// Per-frame upkeep for the enemy, dispatched by `arg2`: state 0 bumps the
/// heal counter, files a negative "damage" with `worldTargetAddReadoutAmount` so the HUD
/// shows it as a heal, and tops the enemy's HP back up by 0x64; state 1 ticks
/// `summonsAlive` down, re-arms `deathDelay` and drops either tracked
/// enemy whose HP has run out.
s32 func_actor_444000_80143E68(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    GluttonWork* work = arg0->work;
    Enemy*       obj  = arg0->spawnArg2.pointer;

    switch (arg2) {
        case 0:
            work->pendingHeals++;
            worldTargetAddReadoutAmount(&obj->node, -0x64, 0);
            if (obj->hp > 0) {
                obj->hp += 0x64;
            }
            break;
        case 1:
            if (work->summonsAlive > 0) {
                work->summonsAlive--;
            }
            work->deathDelay = 2;
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

s32 func_actor_444000_80143F38(Task* arg0, s32 msgId, s32 arg2, s32 arg3)
{
    ((GluttonWork*)arg0->work)->state = 0;
    return 1;
}

/// Reset handler: when the work block is asking for a reset, stop the enemy's
/// own model drawing and push that same flag word onto each of the seven
/// escorts' models, then clear `neckPitchEnabled` and `neckYawEnabled`. Otherwise just run
/// the ordinary re-arm in `_gluttonTickAnim`.
static void func_actor_444000_80143F4C(Task* arg0)
{
    GluttonWork* work;
    GluttonWork* escorts;
    s16          i;

    work = arg0->work;
    if (work->stateChanged != 0) {
        arg0->extra.tmd->flags = 0;
        escorts                = arg0->work;
        escorts->freeCountdown = 0;
        arg0->extra.tmd->flags = 0;
        for (i = 0; i < ARRAY_SIZE(escorts->escorts); i++) {
            if (escorts->escorts[i] != NULL) {
                escorts->escorts[i]->task->extra.tmd->flags = arg0->extra.tmd->flags;
            }
        }
        work->neckPitchEnabled = 0;
        work->neckYawEnabled   = 0;
    } else {
        _gluttonTickAnim(arg0);
    }
}
