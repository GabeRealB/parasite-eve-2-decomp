#include "actors/actor_403200.h"

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
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
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

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
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
#include "../../shared/actor_messages.h"
#include "../../shared/actor_contacts.h"

static void _gluttonSetShakeLevel(s8 level);

/// Selects dumping-hole behavior for this compiled Glutton instance.
///
/// Define before `glutton.h` and retain through every shared fragment. The
/// header defines `GLUTTON_DUMPING_HOLE` as the dimensionless integer 1;
/// the binding must remain a macro for the shared code's `#if` comparisons.
#define GLUTTON_ROOM GLUTTON_DUMPING_HOLE
#include "../../shared/glutton.h"

/// Package-specific state indices used by these handlers and command messages.
enum {
    ACTOR_403200_STATE_HIDDEN        = 0,
    ACTOR_403200_STATE_SHOWN         = 1,
    ACTOR_403200_STATE_RAIN          = 2,
    ACTOR_403200_STATE_SCRIPTED_POSE = 5,
    ACTOR_403200_STATE_GLOBS         = 6,
    ACTOR_403200_STATE_DEBRIS        = 7,
    ACTOR_403200_STATE_CHOOSE_ATTACK = 10,
    ACTOR_403200_STATE_COLLAPSE      = 12,
};

/// Reach in coordinate units at which the limb retracts, and units removed per tick.
enum {
    ACTOR_403200_LIMB_RETRACTION_MIN_REACH = 401,
    ACTOR_403200_LIMB_RETRACTION_STEP      = 200,
};

/// Clips and reach-dependent limb curves selected by these host handlers.
enum {
    ACTOR_403200_CLIP_IDLE               = 1,
    ACTOR_403200_CLIP_GLOB_LAUNCH        = 10,
    ACTOR_403200_CLIP_RETRACT_LIMB       = 13,
    ACTOR_403200_CLIP_COLLAPSE           = 18,
    ACTOR_403200_CLIP_DEBRIS_POSE        = 20,
    ACTOR_403200_LIMB_POSE_DOWNWARD      = 0,
    ACTOR_403200_LIMB_POSE_SHALLOW_CURVE = 3,
};

/// State-table tail indices and clips used by these dumping-hole handlers.
enum {
    ACTOR_403200_STATE_HOLD_PLAYER   = 13,
    ACTOR_403200_STATE_DEATH_HANDOFF = 18,
    ACTOR_403200_CLIP_RAIN_INHALE    = 3,
    ACTOR_403200_CLIP_SWIPE_RECOVERY = 5,
    ACTOR_403200_CLIP_GLOB_FIRST     = 6,
    ACTOR_403200_CLIP_DEBRIS         = 11,
    ACTOR_403200_CLIP_GLOB_REPEAT    = 12,
    ACTOR_403200_CLIP_HOLD_LOOP      = 14,
    ACTOR_403200_CLIP_HOLD_ENTRY     = 15,
    ACTOR_403200_CLIP_SUMMON         = 19,
};

/// Indices of this encounter's camera-selector table.
enum {
    ACTOR_403200_VIEW_SELECTOR_DEFAULT  = 0,
    ACTOR_403200_VIEW_SELECTOR_RAIN     = 1,
    ACTOR_403200_VIEW_SELECTOR_INHALE   = 2,
    ACTOR_403200_VIEW_SELECTOR_GLOBS    = 3,
    ACTOR_403200_VIEW_SELECTOR_SWIPE    = 4,
    ACTOR_403200_VIEW_SELECTOR_DEBRIS   = 5,
    ACTOR_403200_VIEW_SELECTOR_HOLD     = 6,
    ACTOR_403200_VIEW_SELECTOR_COLLAPSE = 7,
};

/// Summon commands broadcast synchronously in the synthetic 0x2C00 context.
enum {
    ACTOR_403200_BROADCAST_CONTEXT_STAGE = 0,
    ACTOR_403200_BROADCAST_CONTEXT_AREA  = 44,
    ACTOR_403200_SUMMON_COMMAND_NONE     = 0,
    ACTOR_403200_SUMMON_COMMAND_PULL     = 2,
    ACTOR_403200_SUMMON_COMMAND_VANISH   = 3,
};

/// Clips lent to the caught player and the state selected after fatal damage.
enum {
    ACTOR_403200_PLAYER_CLIP_INHALE_CAUGHT = 1,
    ACTOR_403200_PLAYER_CLIP_SWIPE_CAUGHT  = 2,
    ACTOR_403200_PLAYER_CLIP_SWIPE_RELEASE = 4,
    ACTOR_403200_PLAYER_FATAL_DAMAGE_STATE = 10,
};

/// Camera indices shared by the dumping-hole phase and position selectors.
enum {
    ACTOR_403200_VIEW_INDEX_MASK    = 0xFF,
    ACTOR_403200_VIEW_FALLBACK      = 1,
    ACTOR_403200_VIEW_PHASE0_MIDDLE = 2,
    ACTOR_403200_VIEW_PHASE0_NEAR   = 3,
    ACTOR_403200_VIEW_HOST_FAR      = 4,
    ACTOR_403200_VIEW_PHASE1_NEAR   = 34,
    ACTOR_403200_VIEW_LATE_NEAR     = 37,
    ACTOR_403200_VIEW_PHASE2_FAR    = 25,
    ACTOR_403200_VIEW_FAR_X         = 30,
};

/// Measures player-root range after signed-halfword XYZ narrowing.
///
/// Invoke as a standalone statement in a braced block, with stable lvalues:
/// a borrowed live Task* hostTask,
/// an SVECTOR offset and an s32 distanceOut. Offset and distanceOut are evaluated
/// repeatedly; the squared sum must fit s32. Reads both roots in their common
/// parent frame without composing them; leaves the vector's pad untouched.
#define ACTOR_403200_MEASURE_PLAYER_DISTANCE(hostTask, offset, distanceOut)                 \
    {                                                                                       \
        SVECTOR*  offsetPointer = &(offset);                                                \
        GfxCoord* hostRoot      = (hostTask)->extra.tmd->coords;                            \
        offsetPointer->vx       = gPlayerStatus.coordMtx->t[0] - hostRoot->coord.t[0];      \
        offsetPointer->vy       = gPlayerStatus.coordMtx->t[1] - hostRoot->coord.t[1];      \
        (distanceOut)           = (offset).vx * (offset).vx;                                \
        offsetPointer->vz       = gPlayerStatus.coordMtx->t[2] - hostRoot->coord.t[2];      \
        (distanceOut)          += (offset).vy * (offset).vy;                                \
        (distanceOut)           = SquareRoot0((distanceOut) + ((offset).vz * (offset).vz)); \
    }

extern s8 D_actor_403200_8015F8E0[8];

/// Points one of the host's camera view functions measures the player's
/// distance from, one for each phase of the fight.
///
/// As the host moves on through the arena, each phase has its own pair of
/// camera views to cut between. The function copies the whole set, takes the
/// point of the current phase and picks the view from how far the player
/// stands from it, with thresholds that depend on the view already showing.
/// `GluttonWork::phase` is the index, 0 to 3; nothing checks it.
typedef struct {
    SVECTOR points[4]; // World positions, indexed by phase; phases 2 and 3 share one point, and `pad` is zero
} _Actor403200ViewAnchors;
STATIC_ASSERT_SIZEOF(_Actor403200ViewAnchors, 0x20);

/// One spinner of the formation the host spawns: the model it is drawn with
/// and how long it waits once the set is released.
///
/// The host spawns its spinners together, one for each record, patching the
/// record's model into the spinner's task descriptor and passing
/// `chaseDelayClass` as the spawn argument.
typedef struct {
    TmdSource* model;           // Model installed in the spinner task's descriptor before the spawn
    s16        chaseDelayClass; // Spawn argument picking `GluttonSpinnerWork::chaseDelay` (0: 0x14 ticks, 1: 0x28, 2: 0x50)
} _Actor403200SpinnerSpawn;
STATIC_ASSERT_SIZEOF(_Actor403200SpinnerSpawn, 0x8);

/// Scratch-stack block the host's idle state works in for one tick.
///
/// The idle state is where every attack returns to. Each tick it turns the
/// neck toward the player, and once the delay between attacks has run out it
/// picks the next state from the fight's phase, the host's hit points and how
/// far the player stands from the host. It reserves one complete block and
/// releases it before returning; nothing in it outlasts the tick.
typedef struct {
    VECTOR  rangeOffset;    // Offset to the player's model root from the point (0, 0xFA, -0x25F) off the host's root, world units; `pad` is never written
    SVECTOR toPlayer;       // Offset from the host's root to the player's root coordinate; its yaw against the host's facing becomes the neck's yaw target. `pad` is never written
    s32     playerDistance; // Length of `rangeOffset`; the range phases 1 and 2 pick their attack by
    byte    field_1C[0x4];  // Reserved with the block and never accessed; role unproven
} _Actor403200IdleScratch;
STATIC_ASSERT_SIZEOF(_Actor403200IdleScratch, 0x20);

/// Scratch-stack block the host's rain-launch state works in for one tick.
///
/// The state launches the eight rain blobs one after another while the neck
/// keeps turning toward the player. It reserves one complete block and
/// releases it before returning.
typedef struct {
    SVECTOR toPlayer;     // Offset from the host's root to the player's root coordinate; its yaw against the host's facing becomes the neck's yaw target. `pad` is never written
    byte    field_8[0x4]; // Reserved with the block and never accessed; role unproven
} _Actor403200RainLaunchScratch;
STATIC_ASSERT_SIZEOF(_Actor403200RainLaunchScratch, 0xC);

/// Scratch-stack block the host's drag state works in for one tick.
///
/// The state draws the player in toward the host's part 4 while the spinners
/// fly: each tick the player is displaced along the line between the two, by
/// an amount the animation's cue and the fight's phase set, and the pad
/// rumbles at an interval the phase sets. A player drawn within reach while
/// the pull is strongest is caught: placed a fixed distance out from that part,
/// turned to face it or away from it, and handed the caught animation. The
/// state reserves one complete block and releases it before returning.
typedef struct {
    VECTOR3 displacement;   // This tick's pull on the player, world units: `offset` on X and Z, 0 on Y. Handed to the player as its pending displacement; left unwritten on a tick without pull
    byte    field_C[0x4];   // Reserved with the block and never accessed; role unproven
    SVECTOR offset;         // Working vector. First the offset from the host's root to the player's root coordinate, for the neck's yaw; then from part 4 to the player, normalised (4096 = 1.0) and scaled to the tick's pull toward that part. A catch reuses it for the placement's offset from `pullCentre`, then for the offset from the placement back to `pullCentre`
    SVECTOR pullCentre;     // Position of the host's part 4 in the view coordinate's space, taken when the player is caught
    byte    field_20[0x20]; // Reserved with the block and never accessed; role unproven
    s32     playerDistance; // Horizontal distance from part 4 to the player, world units; a catch needs it under 0x4B0
    byte    field_44[0x4];  // Reserved with the block and never accessed; role unproven
    s16     pullCentreYaw;  // Turn from a caught player's facing to the bearing of `pullCentre`, wrapped to +/-0x800; under 0x400 either way the player is placed facing the host, otherwise facing away. Stored but never read
    byte    field_4A[0x2];  // Reserved with the block and never accessed; role unproven
    s16     phasePull;      // Pull the phase adds to the base 0x19 units a tick (phase 0: 0, 1: 5, otherwise 0xA), before the cue's divisor
    s16     slot;           // Index into `GluttonWork::summons`, 0 or 1: counter of the loop that forgets the summons as the state ends
    s16     rumblePeriod;   // Ticks between pad rumble scripts during the pull (phase 0: 0x14, 1: 0x10, otherwise 0xC)
} _Actor403200DragScratch;
STATIC_ASSERT_SIZEOF(_Actor403200DragScratch, 0x54);

/// Gameplay's escort `TaskDesc` table; entry 3 is the pair this boss spawns.
extern TaskDesc D_actor_341700_80174D58;

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

/// Non-zero while the overlay is shutting down: the spawn states tear their
/// enemies down instead of standing them up, and the state-selecting tick
/// holds `stateTicks` at zero and re-rolls its sub-state.
extern s16 gGluttonEnded;

/// Set while the grab's animation is installed on the player; the player's
/// side clears it on release.
extern s32 gGluttonGrabActive;

/// Counter the launch state sets and the tick after it winds down; the escort
/// pose driver floors it, drops the escort's body by a fifth of it and picks
/// each phase's targets by its range.
extern s16 gGluttonLimbReach;

/// Cleared by both halves of the launch state and exposed through the setter /
/// getter pair `_gluttonSetSpinnersReleased` and `_gluttonGetSpinnersReleased`;
/// the spinner enemies wait for it to be 1 and die once it is 0.
extern s16 gGluttonSpinnersReleased;

/// Script pairs spawned on the per-frame body's and the death sequence's cues,
/// and on the frames the launch tick's phase selects.
extern PadScriptCmd              D_actor_403200_80141C5C[2];
extern PadScriptVibrationSegment D_actor_403200_80141C64[2];
extern PadScriptCmd              D_actor_403200_80141C6C[2];
extern PadScriptVibrationSegment D_actor_403200_80141C74[2];
extern PadScriptCmd              D_actor_403200_80141C7C[3];
extern PadScriptVibrationSegment D_actor_403200_80141C88[2];

/// Pair descriptors the host and its escorts publish as `Enemy::param`;
/// `hpMax` is the hit-point pool each one starts with.
extern EnemyParams D_actor_403200_80141C00;
extern EnemyParams D_actor_403200_80141C20;
extern EnemyParams D_actor_403200_80141C30;
extern EnemyParams D_actor_403200_80141C40;

/// Per-animation reset argument, a `[?][0x2D]` table indexed by the id that
/// was playing before the switch and the id being switched to.
extern s8 gGluttonAnimTransitions[][0x2D];

/// Animation-set tables: the host's two blocks, escort 0's two and escort 1's.
extern AnimationSet* D_actor_403200_8015E484[];
extern AnimationSet* D_actor_403200_8015E53C[];
extern AnimationSet* D_actor_403200_8015E5F4[];

/// Animation table the stand-up tick publishes to the player in its message
/// 0x3FF, and the one handed over when the placement yaw is outside +/-0x400.
extern AnimationSet* D_actor_403200_8015E6AC[];
extern AnimationSet* D_actor_403200_8015E6CC[];

/// Drop-point group the falling enemies use this round, rerolled whenever a
/// spawn arrives with `spawnArg1` 0.
extern u8 gGluttonRainGroup;

/// Animation-set table the grab states send the player as message 0x3FF;
/// entry 2 is refreshed from the player's own weapon block.
extern AnimationSet* gGluttonCaughtAnimSets[];

/// Spawn table of the seven escorts, indexed 0..6.
extern TaskDesc D_actor_403200_8015E72C[];

/// Per-`spawnArg1` offset from the host model to the point the falling enemy
/// is stood up at.
extern SVECTOR gGluttonRainLaunchOffsets[];
/// The drop points: `vz` is added to the ring x coordinate and `vx` (less
/// 0x189C) becomes the z coordinate.
extern SVECTOR gGluttonRainPoints[];
/// `[group][spawnArg1]` index into `gGluttonRainPoints`.
extern u8 gGluttonRainPointIndex[][8];

/// Enemy spawn table the three launch states of `_actor403200GlobLaunchState`
/// draw from.
extern TaskDesc gGluttonEscortTasks[];

/// The enemy task's message-handler table, parked in `Task::msgTable`.
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_403200_8015F770[8];

/// Three formations of nine positions, and each member's model/spawn argument.
extern SVECTOR                  D_actor_403200_8015F7B0[3][9];
extern _Actor403200SpinnerSpawn D_actor_403200_8015F888[9];

/// Shared 0x7DA payload buffer.
extern ActorCommand D_actor_403200_8015F8F4;

/// View-space point the launch tick clears and fills from the host's fourth
/// model part on a state change; the spinner enemies home on it.
extern SVECTOR gGluttonSpinnerTarget;

/// Button-press hold the glob's engulf state sends the player, asking for 40
/// presses.
extern GluttonButtonPressHoldStorage gGluttonGrabQuery;

/// The scratch coordinate the debris effect of `_actor403200DebrisState` is
/// built on: `F920` is the whole `GfxCoord` and `F924` its `coord` matrix,
/// which splat names separately because the code takes that address directly.
extern GfxCoord D_actor_403200_8015F920;

extern GfxCoord D_actor_403200_8015F970;

/// Allocation holding the placement the host sends the player, and the forty
/// bytes after it.
///
/// `placement` is the payload of `GAME_ACTOR_MESSAGE_PLACE`, kept in static
/// storage and lent to the player for the length of the dispatch. The drag
/// state fills it in when it catches the player. The state that follows
/// moves the player to the host's root: it rewrites the position alone and
/// sends the record again with the rotation the catch left.
///
/// Forty zero bytes separate the record from the object after it. No access
/// to them is recovered, so whether they are trailing fields of the record or
/// separate unreferenced variables is unproven; they stay in this allocation
/// only to keep the object after it at its address.
typedef struct {
    ActorTransform placement;    // Where the player is put and the Euler rotation it is given; only yaw is ever nonzero, and `pos.pad` is never written
    u8             field_18[40]; // Zero in the image; no access established and role unproven
} _Actor403200PlayerPlacementStorage;
STATIC_ASSERT_SIZEOF(_Actor403200PlayerPlacementStorage, 0x40);

extern _Actor403200PlayerPlacementStorage D_actor_403200_8015F9C0;

extern GameActorButtonPressHold D_actor_403200_8015FA00;

static void _actor403200BuildAngledWall(Task* host, s16 distance, s16 lowerEdgeDrop, s16 faceIndex);

static AnimationSet _gActor403200Animation1D9C0;
static AnimationSet _gActor403200Animation1DA6C;
static AnimationSet _gActor403200Animation1DB18;
static AnimationSet _gActor403200Animation1DE00;
static AnimationSet _gActor403200Animation1E0C4;
static AnimationSet _gActor403200Animation1E3A0;
static AnimationSet _gActor403200Animation1E6F0;
static AnimationSet _gActor403200Animation1E8D0;
static AnimationSet _gActor403200Animation1EAB0;
static AnimationSet _gActor403200Animation1ED88;
static AnimationSet _gActor403200Animation1EE60;
static AnimationSet _gActor403200Animation1EF3C;
static AnimationSet _gActor403200Animation1F324;
static AnimationSet _gActor403200Animation1F6E4;
static AnimationSet _gActor403200Animation1F9F8;
static AnimationSet _gActor403200Animation1FDDC;
static AnimationSet _gActor403200Animation20234;
static AnimationSet _gActor403200Animation2065C;
static AnimationSet _gActor403200Animation20A08;
static AnimationSet _gActor403200Animation20D10;
static AnimationSet _gActor403200Animation20FF8;
static AnimationSet _gActor403200Animation21260;
static AnimationSet _gActor403200Animation2132C;
static AnimationSet _gActor403200Animation213F8;
static AnimationSet _gActor403200Animation216D0;
static AnimationSet _gActor403200Animation21914;
static AnimationSet _gActor403200Animation21B54;
static AnimationSet _gActor403200Animation21D5C;
static AnimationSet _gActor403200Animation21EE8;
static AnimationSet _gActor403200Animation2207C;
static AnimationSet _gActor403200Animation224B4;
static AnimationSet _gActor403200Animation227F8;
static AnimationSet _gActor403200Animation22B10;
static AnimationSet _gActor403200Animation22E58;
static AnimationSet _gActor403200Animation2318C;
static AnimationSet _gActor403200Animation234B8;
static AnimationSet _gActor403200Animation23998;
static AnimationSet _gActor403200Animation23C2C;
static AnimationSet _gActor403200Animation23F38;
static AnimationSet _gActor403200Animation24FFC;
static AnimationSet _gActor403200Animation25B98;
static AnimationSet _gActor403200Animation25CA0;
static AnimationSet _gActor403200Animation25D80;
static AnimationSet _gActor403200Animation25E60;
static AnimationSet _gActor403200Animation25F68;
static AnimationSet _gActor403200Animation26030;
static AnimationSet _gActor403200Animation260F8;
static AnimationSet _gActor403200Animation261F0;
static AnimationSet _gActor403200Animation262B8;
static AnimationSet _gActor403200Animation26380;
static AnimationSet _gActor403200Animation26AB8;
static AnimationSet _gActor403200Animation27F04;
static AnimationSet _gActor403200Animation293D8;
static AnimationSet _gActor403200Animation29B70;
static AnimationSet _gActor403200Animation2A3F4;
static AnimationSet _gActor403200Animation2AC80;
static AnimationSet _gActor403200Animation2AD60;
static AnimationSet _gActor403200Animation2ADEC;
static AnimationSet _gActor403200Animation2AE78;
static AnimationSet _gActor403200Animation2BE50;

static AnimationSet _gActor403200Animation2B644;
static TmdSource    _gActor403200Model12884;
static TmdSource    _gActor403200Model13774;
static TmdSource    _gActor403200GluttonLegLeft;
static TmdSource    _gActor403200GluttonLegRight;
static TmdSource    _gActor403200Model1785C;
static TmdSource    _gActor403200Model186D8;
static TmdSource    _gActor403200Model18BE4;
static TmdSource    _gActor403200Model19284;
static TmdSource    _gActor403200Model199E4;
static TmdSource    _gActor403200Model1AC48;
static s32          _actor403200PickGlobView(Task* host, s16 phase);
static s32          _actor403200PickDebrisView(Task* host, s16 phase);
static s32          _actor403200PickCombatView(Task* host, s16 phase);
static s32          _actor403200PickRainView(Task* host, s16 phase);
static s32          _actor403200PickCloseRangeView(Task* host, s16 phase);
static s32          _actor403200PickAdvanceView(Task* unusedHost, s16 phase);
static s32          _actor403200PickSwipeView(Task* task, s16 phase);
static s32          _actor403200PickDefaultView(Task* host, s16 phase);
static s32          _actor403200PickPlayerXView(Task* unusedHost, s16 unusedPhase);
static void         _gluttonEscort6Task(Task* task);
static void         _gluttonPropTask(Task* task);

static s32  _actor403200SetModelDraw(Task* task, s32 unusedMessageId, s32 drawMode, s32 unusedSecondArg);
static s32  _actor403200ApplyCommand(Task* task, s32 unusedMessageId, const ActorCommand* command, s32 unusedSecondArg);
static s32  _actor403200HandleActorEvent(Task* task, s32 unusedMessageId, s32 event, s32 unusedSecondArg);
static s32  _actor403200ReleaseGlobGrab(Task* task, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg);
static void _actor403200HostTask(Task* host);
static void _actor403200IgnoreMessage2015(Task* task, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg);

extern DamageAttack D_actor_403200_80141BE8[6];

DamageAttack D_actor_403200_80141BE8[6] = {
    { 0, 0 },
    { 30, 0 },
    { 25, 3 },
    { 9999, 0 },
    { 50, 11 },
    { 35, 0 },
};

EnemyParams D_actor_403200_80141C00 = { D_actor_403200_80141BE8, 3000, 500, 200, 100, 100, 0, 0, 0 };

EnemyParams D_actor_403200_80141C10 = { D_actor_403200_80141BE8, 3000, 700, 200, 100, 100, 0, 0, 0 };

EnemyParams D_actor_403200_80141C20 = { D_actor_403200_80141BE8, 120, 0, 0, 0, 50, 0, 0, 0 };

EnemyParams D_actor_403200_80141C30 = { D_actor_403200_80141BE8, 120, 0, 0, 0, 50, 0, 0, 0 };

EnemyParams D_actor_403200_80141C40 = { D_actor_403200_80141BE8, 200, 0, 0, 0, 10, 0, 0, 0 };

s16 gGluttonEnded = 0;

s32 gGluttonGrabActive = 0;

s16 gGluttonLimbReach = 0;

s16 gGluttonSpinnersReleased = 0;

PadScriptCmd D_actor_403200_80141C5C[2] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_actor_403200_80141C64[2] = {
    { 0, 0, 7, 0 },
    { 255, 53, 27, 1 },
};

PadScriptCmd D_actor_403200_80141C6C[2] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_actor_403200_80141C74[2] = {
    { 186, 74, 32, 1 },
    { 0, 0, 7, 0 },
};

PadScriptCmd D_actor_403200_80141C7C[3] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_actor_403200_80141C88[2] = {
    { 22, 109, 20, 1 },
    { 80, 31, 34, 1 },
};

static TmdBone _gActor403200Model10824Skeleton[8] = {
#include "assets/actor_403200_model_10824_skeleton.inc"
};

static u32 _gActor403200Model10824PartVerts[8] = {
#include "assets/actor_403200_model_10824_partVerts.inc"
};

static SVECTOR _gActor403200Model10824Verts[135] = {
#include "assets/actor_403200_model_10824_verts.inc"
};

static SVECTOR _gActor403200Model10824Normals[135] = {
#include "assets/actor_403200_model_10824_normals.inc"
};

static u32 _gActor403200Model10824Stream[1732] = {
#include "assets/actor_403200_model_10824_stream.inc"
};

static TmdSource _gActor403200Model10824 = {
    0,
    8496,
    3588,
    8,
    _gActor403200Model10824PartVerts,
    _gActor403200Model10824Verts,
    _gActor403200Model10824Normals,
    _gActor403200Model10824Skeleton,
    _gActor403200Model10824Stream,
};

static TmdBone _gActor403200Model12884Skeleton[1] = {
#include "assets/actor_403200_model_12884_skeleton.inc"
};

static u32 _gActor403200Model12884PartVerts[1] = {
#include "assets/actor_403200_model_12884_partVerts.inc"
};

static SVECTOR _gActor403200Model12884Verts[82] = {
#include "assets/actor_403200_model_12884_verts.inc"
};

static SVECTOR _gActor403200Model12884Normals[79] = {
#include "assets/actor_403200_model_12884_normals.inc"
};

static u32 _gActor403200Model12884Stream[533] = {
#include "assets/actor_403200_model_12884_stream.inc"
};

static TmdSource _gActor403200Model12884 = {
    0,
    3696,
    0,
    1,
    _gActor403200Model12884PartVerts,
    _gActor403200Model12884Verts,
    _gActor403200Model12884Normals,
    _gActor403200Model12884Skeleton,
    _gActor403200Model12884Stream,
};

static TmdBone _gActor403200Model13774Skeleton[1] = {
#include "assets/actor_403200_model_13774_skeleton.inc"
};

static u32 _gActor403200Model13774PartVerts[1] = {
#include "assets/actor_403200_model_13774_partVerts.inc"
};

static SVECTOR _gActor403200Model13774Verts[90] = {
#include "assets/actor_403200_model_13774_verts.inc"
};

static SVECTOR _gActor403200Model13774Normals[112] = {
#include "assets/actor_403200_model_13774_normals.inc"
};

static u32 _gActor403200Model13774Stream[698] = {
#include "assets/actor_403200_model_13774_stream.inc"
};

static TmdSource _gActor403200Model13774 = {
    0,
    4808,
    0,
    1,
    _gActor403200Model13774PartVerts,
    _gActor403200Model13774Verts,
    _gActor403200Model13774Normals,
    _gActor403200Model13774Skeleton,
    _gActor403200Model13774Stream,
};

static TmdBone _gActor403200GluttonLegLeftSkeleton[4] = {
#include "assets/glutton_leg_left_skeleton.inc"
};

static u32 _gActor403200GluttonLegLeftPartVerts[4] = {
#include "assets/glutton_leg_left_partVerts.inc"
};

static SVECTOR _gActor403200GluttonLegLeftVerts[104] = {
#include "assets/glutton_leg_left_verts.inc"
};

static SVECTOR _gActor403200GluttonLegLeftNormals[115] = {
#include "assets/glutton_leg_left_normals.inc"
};

static u32 _gActor403200GluttonLegLeftStream[1032] = {
#include "assets/glutton_leg_left_stream.inc"
};

static TmdSource _gActor403200GluttonLegLeft = {
    0,
    6172,
    1048,
    4,
    _gActor403200GluttonLegLeftPartVerts,
    _gActor403200GluttonLegLeftVerts,
    _gActor403200GluttonLegLeftNormals,
    _gActor403200GluttonLegLeftSkeleton,
    _gActor403200GluttonLegLeftStream,
};

static TmdBone _gActor403200GluttonLegRightSkeleton[4] = {
#include "assets/glutton_leg_right_skeleton.inc"
};

static u32 _gActor403200GluttonLegRightPartVerts[4] = {
#include "assets/glutton_leg_right_partVerts.inc"
};

static SVECTOR _gActor403200GluttonLegRightVerts[104] = {
#include "assets/glutton_leg_right_verts.inc"
};

static SVECTOR _gActor403200GluttonLegRightNormals[115] = {
#include "assets/glutton_leg_right_normals.inc"
};

static u32 _gActor403200GluttonLegRightStream[1032] = {
#include "assets/glutton_leg_right_stream.inc"
};

static TmdSource _gActor403200GluttonLegRight = {
    0,
    6172,
    1048,
    4,
    _gActor403200GluttonLegRightPartVerts,
    _gActor403200GluttonLegRightVerts,
    _gActor403200GluttonLegRightNormals,
    _gActor403200GluttonLegRightSkeleton,
    _gActor403200GluttonLegRightStream,
};

static TmdBone _gActor403200Model1785CSkeleton[8] = {
#include "assets/actor_403200_model_1785C_skeleton.inc"
};

static u32 _gActor403200Model1785CPartVerts[8] = {
#include "assets/actor_403200_model_1785C_partVerts.inc"
};

static SVECTOR _gActor403200Model1785CVerts[82] = {
#include "assets/actor_403200_model_1785C_verts.inc"
};

static SVECTOR _gActor403200Model1785CNormals[82] = {
#include "assets/actor_403200_model_1785C_normals.inc"
};

static u32 _gActor403200Model1785CStream[835] = {
#include "assets/actor_403200_model_1785C_stream.inc"
};

static TmdSource _gActor403200Model1785C = {
    0,
    4140,
    1872,
    8,
    _gActor403200Model1785CPartVerts,
    _gActor403200Model1785CVerts,
    _gActor403200Model1785CNormals,
    _gActor403200Model1785CSkeleton,
    _gActor403200Model1785CStream,
};

static TmdBone _gActor403200Model186D8Skeleton[1] = {
#include "assets/actor_403200_model_186D8_skeleton.inc"
};

static u32 _gActor403200Model186D8PartVerts[1] = {
#include "assets/actor_403200_model_186D8_partVerts.inc"
};

static SVECTOR _gActor403200Model186D8Verts[36] = {
#include "assets/actor_403200_model_186D8_verts.inc"
};

static SVECTOR _gActor403200Model186D8Normals[1] = {
#include "assets/actor_403200_model_186D8_normals.inc"
};

static u32 _gActor403200Model186D8Stream[216] = {
#include "assets/actor_403200_model_186D8_stream.inc"
};

static TmdSource _gActor403200Model186D8 = {
    0,
    1560,
    0,
    1,
    _gActor403200Model186D8PartVerts,
    _gActor403200Model186D8Verts,
    _gActor403200Model186D8Normals,
    _gActor403200Model186D8Skeleton,
    _gActor403200Model186D8Stream,
};

static TmdBone _gActor403200Model18BE4Skeleton[1] = {
#include "assets/actor_403200_model_18BE4_skeleton.inc"
};

static u32 _gActor403200Model18BE4PartVerts[1] = {
#include "assets/actor_403200_model_18BE4_partVerts.inc"
};

static SVECTOR _gActor403200Model18BE4Verts[22] = {
#include "assets/actor_403200_model_18BE4_verts.inc"
};

static SVECTOR _gActor403200Model18BE4Normals[22] = {
#include "assets/actor_403200_model_18BE4_normals.inc"
};

static u32 _gActor403200Model18BE4Stream[173] = {
#include "assets/actor_403200_model_18BE4_stream.inc"
};

static TmdSource _gActor403200Model18BE4 = {
    0,
    1136,
    0,
    1,
    _gActor403200Model18BE4PartVerts,
    _gActor403200Model18BE4Verts,
    _gActor403200Model18BE4Normals,
    _gActor403200Model18BE4Skeleton,
    _gActor403200Model18BE4Stream,
};

static TmdBone _gActor403200Model19284Skeleton[1] = {
#include "assets/actor_403200_model_19284_skeleton.inc"
};

static u32 _gActor403200Model19284PartVerts[1] = {
#include "assets/actor_403200_model_19284_partVerts.inc"
};

static SVECTOR _gActor403200Model19284Verts[58] = {
#include "assets/actor_403200_model_19284_verts.inc"
};

static SVECTOR _gActor403200Model19284Normals[58] = {
#include "assets/actor_403200_model_19284_normals.inc"
};

static u32 _gActor403200Model19284Stream[313] = {
#include "assets/actor_403200_model_19284_stream.inc"
};

static TmdSource _gActor403200Model19284 = {
    0,
    2176,
    0,
    1,
    _gActor403200Model19284PartVerts,
    _gActor403200Model19284Verts,
    _gActor403200Model19284Normals,
    _gActor403200Model19284Skeleton,
    _gActor403200Model19284Stream,
};

static TmdBone _gActor403200Model199E4Skeleton[1] = {
#include "assets/actor_403200_model_199E4_skeleton.inc"
};

static u32 _gActor403200Model199E4PartVerts[1] = {
#include "assets/actor_403200_model_199E4_partVerts.inc"
};

static SVECTOR _gActor403200Model199E4Verts[70] = {
#include "assets/actor_403200_model_199E4_verts.inc"
};

static u32 _gActor403200Model199E4Stream[618] = {
#include "assets/actor_403200_model_199E4_stream.inc"
};

static TmdSource _gActor403200Model199E4 = {
    0,
    3536,
    0,
    1,
    _gActor403200Model199E4PartVerts,
    _gActor403200Model199E4Verts,
    &_gActor403200Model199E4Verts[70],
    _gActor403200Model199E4Skeleton,
    _gActor403200Model199E4Stream,
};

static TmdBone _gActor403200Model1AC48Skeleton[1] = {
#include "assets/actor_403200_model_1AC48_skeleton.inc"
};

static u32 _gActor403200Model1AC48PartVerts[1] = {
#include "assets/actor_403200_model_1AC48_partVerts.inc"
};

static SVECTOR _gActor403200Model1AC48Verts[135] = {
#include "assets/actor_403200_model_1AC48_verts.inc"
};

static SVECTOR _gActor403200Model1AC48Normals[135] = {
#include "assets/actor_403200_model_1AC48_normals.inc"
};

static u32 _gActor403200Model1AC48Stream[1400] = {
#include "assets/actor_403200_model_1AC48_stream.inc"
};

static TmdSource _gActor403200Model1AC48 = {
    0,
    9492,
    0,
    1,
    _gActor403200Model1AC48PartVerts,
    _gActor403200Model1AC48Verts,
    _gActor403200Model1AC48Normals,
    _gActor403200Model1AC48Skeleton,
    _gActor403200Model1AC48Stream,
};

static TmdBone _gActor403200Model1C474Skeleton[1] = {
#include "assets/actor_403200_model_1C474_skeleton.inc"
};

static u32 _gActor403200Model1C474PartVerts[1] = {
#include "assets/actor_403200_model_1C474_partVerts.inc"
};

static SVECTOR _gActor403200Model1C474Verts[32] = {
#include "assets/actor_403200_model_1C474_verts.inc"
};

static SVECTOR _gActor403200Model1C474Normals[32] = {
#include "assets/actor_403200_model_1C474_normals.inc"
};

static u32 _gActor403200Model1C474Stream[230] = {
#include "assets/actor_403200_model_1C474_stream.inc"
};

static TmdSource _gActor403200Model1C474 = {
    0,
    1664,
    0,
    1,
    _gActor403200Model1C474PartVerts,
    _gActor403200Model1C474Verts,
    _gActor403200Model1C474Normals,
    _gActor403200Model1C474Skeleton,
    _gActor403200Model1C474Stream,
};

static TmdBone _gActor403200GluttonPropSkeleton[1] = {
#include "assets/glutton_prop_skeleton.inc"
};

static u32 _gActor403200GluttonPropPartVerts[1] = {
#include "assets/glutton_prop_partVerts.inc"
};

static SVECTOR _gActor403200GluttonPropVerts[34] = {
#include "assets/glutton_prop_verts.inc"
};

static SVECTOR _gActor403200GluttonPropNormals[22] = {
#include "assets/glutton_prop_normals.inc"
};

static u32 _gActor403200GluttonPropStream[176] = {
#include "assets/glutton_prop_stream.inc"
};

static TmdSource _gActor403200GluttonProp = {
    0,
    1228,
    0,
    1,
    _gActor403200GluttonPropPartVerts,
    _gActor403200GluttonPropVerts,
    _gActor403200GluttonPropNormals,
    _gActor403200GluttonPropSkeleton,
    _gActor403200GluttonPropStream,
};

static TmdBone _gActor403200Model1CDA4Skeleton[1] = {
#include "assets/actor_403200_model_1CDA4_skeleton.inc"
};

static u32 _gActor403200Model1CDA4PartVerts[1] = {
#include "assets/actor_403200_model_1CDA4_partVerts.inc"
};

static SVECTOR _gActor403200Model1CDA4Verts[8] = {
#include "assets/actor_403200_model_1CDA4_verts.inc"
};

static SVECTOR _gActor403200Model1CDA4Normals[8] = {
#include "assets/actor_403200_model_1CDA4_normals.inc"
};

static u32 _gActor403200Model1CDA4Stream[48] = {
#include "assets/actor_403200_model_1CDA4_stream.inc"
};

static TmdSource _gActor403200Model1CDA4 = {
    0,
    312,
    0,
    1,
    _gActor403200Model1CDA4PartVerts,
    _gActor403200Model1CDA4Verts,
    _gActor403200Model1CDA4Normals,
    _gActor403200Model1CDA4Skeleton,
    _gActor403200Model1CDA4Stream,
};

static TmdBone _gActor403200Model1CFA0Skeleton[1] = {
#include "assets/actor_403200_model_1CFA0_skeleton.inc"
};

static u32 _gActor403200Model1CFA0PartVerts[1] = {
#include "assets/actor_403200_model_1CFA0_partVerts.inc"
};

static SVECTOR _gActor403200Model1CFA0Verts[16] = {
#include "assets/actor_403200_model_1CFA0_verts.inc"
};

static SVECTOR _gActor403200Model1CFA0Normals[14] = {
#include "assets/actor_403200_model_1CFA0_normals.inc"
};

static u32 _gActor403200Model1CFA0Stream[132] = {
#include "assets/actor_403200_model_1CFA0_stream.inc"
};

static TmdSource _gActor403200Model1CFA0 = {
    0,
    936,
    0,
    1,
    _gActor403200Model1CFA0PartVerts,
    _gActor403200Model1CFA0Verts,
    _gActor403200Model1CFA0Normals,
    _gActor403200Model1CFA0Skeleton,
    _gActor403200Model1CFA0Stream,
};

static TmdBone _gActor403200Model1D26CSkeleton[1] = {
#include "assets/actor_403200_model_1D26C_skeleton.inc"
};

static u32 _gActor403200Model1D26CPartVerts[1] = {
#include "assets/actor_403200_model_1D26C_partVerts.inc"
};

static SVECTOR _gActor403200Model1D26CVerts[8] = {
#include "assets/actor_403200_model_1D26C_verts.inc"
};

static SVECTOR _gActor403200Model1D26CNormals[6] = {
#include "assets/actor_403200_model_1D26C_normals.inc"
};

static u32 _gActor403200Model1D26CStream[48] = {
#include "assets/actor_403200_model_1D26C_stream.inc"
};

static TmdSource _gActor403200Model1D26C = {
    0,
    312,
    0,
    1,
    _gActor403200Model1D26CPartVerts,
    _gActor403200Model1D26CVerts,
    _gActor403200Model1D26CNormals,
    _gActor403200Model1D26CSkeleton,
    _gActor403200Model1D26CStream,
};

static TmdBone _gActor403200Model1D3E8Skeleton[1] = {
#include "assets/actor_403200_model_1D3E8_skeleton.inc"
};

static u32 _gActor403200Model1D3E8PartVerts[1] = {
#include "assets/actor_403200_model_1D3E8_partVerts.inc"
};

static SVECTOR _gActor403200Model1D3E8Verts[8] = {
#include "assets/actor_403200_model_1D3E8_verts.inc"
};

static SVECTOR _gActor403200Model1D3E8Normals[6] = {
#include "assets/actor_403200_model_1D3E8_normals.inc"
};

static u32 _gActor403200Model1D3E8Stream[48] = {
#include "assets/actor_403200_model_1D3E8_stream.inc"
};

static TmdSource _gActor403200Model1D3E8 = {
    0,
    312,
    0,
    1,
    _gActor403200Model1D3E8PartVerts,
    _gActor403200Model1D3E8Verts,
    _gActor403200Model1D3E8Normals,
    _gActor403200Model1D3E8Skeleton,
    _gActor403200Model1D3E8Stream,
};

static TmdBone _gActor403200Model1D524Skeleton[1] = {
#include "assets/actor_403200_model_1D524_skeleton.inc"
};

static u32 _gActor403200Model1D524PartVerts[1] = {
#include "assets/actor_403200_model_1D524_partVerts.inc"
};

static SVECTOR _gActor403200Model1D524Verts[4] = {
#include "assets/actor_403200_model_1D524_verts.inc"
};

static SVECTOR _gActor403200Model1D524Normals[2] = {
#include "assets/actor_403200_model_1D524_normals.inc"
};

static u32 _gActor403200Model1D524Stream[18] = {
#include "assets/actor_403200_model_1D524_stream.inc"
};

static TmdSource _gActor403200Model1D524 = {
    0,
    104,
    0,
    1,
    _gActor403200Model1D524PartVerts,
    _gActor403200Model1D524Verts,
    _gActor403200Model1D524Normals,
    _gActor403200Model1D524Skeleton,
    _gActor403200Model1D524Stream,
};

static TmdBone _gActor403200Model1D5241D630Skeleton[1] = {
#include "assets/actor_403200_model_1D524_1D630_skeleton.inc"
};

static u32 _gActor403200Model1D5241D630PartVerts[1] = {
#include "assets/actor_403200_model_1D524_1D630_partVerts.inc"
};

static SVECTOR _gActor403200Model1D5241D630Verts[4] = {
#include "assets/actor_403200_model_1D524_1D630_verts.inc"
};

static SVECTOR _gActor403200Model1D5241D630Normals[2] = {
#include "assets/actor_403200_model_1D524_1D630_normals.inc"
};

static u32 _gActor403200Model1D5241D630Stream[18] = {
#include "assets/actor_403200_model_1D524_1D630_stream.inc"
};

static TmdSource _gActor403200Model1D5241D630 = {
    0,
    104,
    0,
    1,
    _gActor403200Model1D5241D630PartVerts,
    _gActor403200Model1D5241D630Verts,
    _gActor403200Model1D5241D630Normals,
    _gActor403200Model1D5241D630Skeleton,
    _gActor403200Model1D5241D630Stream,
};

static TmdBone _gActor403200Model1D754Skeleton[1] = {
#include "assets/actor_403200_model_1D754_skeleton.inc"
};

static u32 _gActor403200Model1D754PartVerts[1] = {
#include "assets/actor_403200_model_1D754_partVerts.inc"
};

static SVECTOR _gActor403200Model1D754Verts[9] = {
#include "assets/actor_403200_model_1D754_verts.inc"
};

static SVECTOR _gActor403200Model1D754Normals[18] = {
#include "assets/actor_403200_model_1D754_normals.inc"
};

static u32 _gActor403200Model1D754Stream[85] = {
#include "assets/actor_403200_model_1D754_stream.inc"
};

static TmdSource _gActor403200Model1D754 = {
    0,
    528,
    0,
    1,
    _gActor403200Model1D754PartVerts,
    _gActor403200Model1D754Verts,
    _gActor403200Model1D754Normals,
    _gActor403200Model1D754Skeleton,
    _gActor403200Model1D754Stream,
};

static AnimationPackedPose _gActor403200Animation1D9C0Bank1[4] = {
#include "assets/actor_403200_animation_1D9C0_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation1D9C0Bank4[7] = {
#include "assets/actor_403200_animation_1D9C0_bank4.inc"
};

static AnimationRecord _gActor403200Animation1D9C0Records[38] = {
#include "assets/actor_403200_animation_1D9C0_records.inc"
};

static u16 _gActor403200Animation1D9C0Indices[8] = {
#include "assets/actor_403200_animation_1D9C0_indices.inc"
};

static AnimationSet _gActor403200Animation1D9C0 = {
    _gActor403200Animation1D9C0Records,
    _gActor403200Animation1D9C0Indices,
    { NULL, _gActor403200Animation1D9C0Bank1, NULL, NULL, _gActor403200Animation1D9C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation1DA6CBank1[4] = {
#include "assets/actor_403200_animation_1DA6C_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation1DA6CBank4[1] = {
#include "assets/actor_403200_animation_1DA6C_bank4.inc"
};

static AnimationRecord _gActor403200Animation1DA6CRecords[18] = {
#include "assets/actor_403200_animation_1DA6C_records.inc"
};

static u16 _gActor403200Animation1DA6CIndices[4] = {
#include "assets/actor_403200_animation_1DA6C_indices.inc"
};

static AnimationSet _gActor403200Animation1DA6C = {
    _gActor403200Animation1DA6CRecords,
    _gActor403200Animation1DA6CIndices,
    { NULL, _gActor403200Animation1DA6CBank1, NULL, NULL, _gActor403200Animation1DA6CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation1DB18Bank1[4] = {
#include "assets/actor_403200_animation_1DB18_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation1DB18Bank4[1] = {
#include "assets/actor_403200_animation_1DB18_bank4.inc"
};

static AnimationRecord _gActor403200Animation1DB18Records[18] = {
#include "assets/actor_403200_animation_1DB18_records.inc"
};

static u16 _gActor403200Animation1DB18Indices[4] = {
#include "assets/actor_403200_animation_1DB18_indices.inc"
};

static AnimationSet _gActor403200Animation1DB18 = {
    _gActor403200Animation1DB18Records,
    _gActor403200Animation1DB18Indices,
    { NULL, _gActor403200Animation1DB18Bank1, NULL, NULL, _gActor403200Animation1DB18Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation1DE00Bank1[7] = {
#include "assets/actor_403200_animation_1DE00_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation1DE00Bank4[49] = {
#include "assets/actor_403200_animation_1DE00_bank4.inc"
};

static AnimationRecord _gActor403200Animation1DE00Records[102] = {
#include "assets/actor_403200_animation_1DE00_records.inc"
};

static u16 _gActor403200Animation1DE00Indices[8] = {
#include "assets/actor_403200_animation_1DE00_indices.inc"
};

static AnimationSet _gActor403200Animation1DE00 = {
    _gActor403200Animation1DE00Records,
    _gActor403200Animation1DE00Indices,
    { NULL, _gActor403200Animation1DE00Bank1, NULL, NULL, _gActor403200Animation1DE00Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation1E0C4Bank1[29] = {
#include "assets/actor_403200_animation_1E0C4_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation1E0C4Bank4[17] = {
#include "assets/actor_403200_animation_1E0C4_bank4.inc"
};

static AnimationRecord _gActor403200Animation1E0C4Records[61] = {
#include "assets/actor_403200_animation_1E0C4_records.inc"
};

static u16 _gActor403200Animation1E0C4Indices[4] = {
#include "assets/actor_403200_animation_1E0C4_indices.inc"
};

static AnimationSet _gActor403200Animation1E0C4 = {
    _gActor403200Animation1E0C4Records,
    _gActor403200Animation1E0C4Indices,
    { NULL, _gActor403200Animation1E0C4Bank1, NULL, NULL, _gActor403200Animation1E0C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation1E3A0Bank1[30] = {
#include "assets/actor_403200_animation_1E3A0_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation1E3A0Bank4[18] = {
#include "assets/actor_403200_animation_1E3A0_bank4.inc"
};

static AnimationRecord _gActor403200Animation1E3A0Records[63] = {
#include "assets/actor_403200_animation_1E3A0_records.inc"
};

static u16 _gActor403200Animation1E3A0Indices[4] = {
#include "assets/actor_403200_animation_1E3A0_indices.inc"
};

static AnimationSet _gActor403200Animation1E3A0 = {
    _gActor403200Animation1E3A0Records,
    _gActor403200Animation1E3A0Indices,
    { NULL, _gActor403200Animation1E3A0Bank1, NULL, NULL, _gActor403200Animation1E3A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation1E6F0Bank1[20] = {
#include "assets/actor_403200_animation_1E6F0_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation1E6F0Bank4[48] = {
#include "assets/actor_403200_animation_1E6F0_bank4.inc"
};

static AnimationRecord _gActor403200Animation1E6F0Records[90] = {
#include "assets/actor_403200_animation_1E6F0_records.inc"
};

static u16 _gActor403200Animation1E6F0Indices[8] = {
#include "assets/actor_403200_animation_1E6F0_indices.inc"
};

static AnimationSet _gActor403200Animation1E6F0 = {
    _gActor403200Animation1E6F0Records,
    _gActor403200Animation1E6F0Indices,
    { NULL, _gActor403200Animation1E6F0Bank1, NULL, NULL, _gActor403200Animation1E6F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation1E8D0Bank1[18] = {
#include "assets/actor_403200_animation_1E8D0_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation1E8D0Bank4[13] = {
#include "assets/actor_403200_animation_1E8D0_bank4.inc"
};

static AnimationRecord _gActor403200Animation1E8D0Records[41] = {
#include "assets/actor_403200_animation_1E8D0_records.inc"
};

static u16 _gActor403200Animation1E8D0Indices[4] = {
#include "assets/actor_403200_animation_1E8D0_indices.inc"
};

static AnimationSet _gActor403200Animation1E8D0 = {
    _gActor403200Animation1E8D0Records,
    _gActor403200Animation1E8D0Indices,
    { NULL, _gActor403200Animation1E8D0Bank1, NULL, NULL, _gActor403200Animation1E8D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation1EAB0Bank1[18] = {
#include "assets/actor_403200_animation_1EAB0_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation1EAB0Bank4[13] = {
#include "assets/actor_403200_animation_1EAB0_bank4.inc"
};

static AnimationRecord _gActor403200Animation1EAB0Records[41] = {
#include "assets/actor_403200_animation_1EAB0_records.inc"
};

static u16 _gActor403200Animation1EAB0Indices[4] = {
#include "assets/actor_403200_animation_1EAB0_indices.inc"
};

static AnimationSet _gActor403200Animation1EAB0 = {
    _gActor403200Animation1EAB0Records,
    _gActor403200Animation1EAB0Indices,
    { NULL, _gActor403200Animation1EAB0Bank1, NULL, NULL, _gActor403200Animation1EAB0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation1ED88Bank1[12] = {
#include "assets/actor_403200_animation_1ED88_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation1ED88Bank4[51] = {
#include "assets/actor_403200_animation_1ED88_bank4.inc"
};

static AnimationRecord _gActor403200Animation1ED88Records[81] = {
#include "assets/actor_403200_animation_1ED88_records.inc"
};

static u16 _gActor403200Animation1ED88Indices[8] = {
#include "assets/actor_403200_animation_1ED88_indices.inc"
};

static AnimationSet _gActor403200Animation1ED88 = {
    _gActor403200Animation1ED88Records,
    _gActor403200Animation1ED88Indices,
    { NULL, _gActor403200Animation1ED88Bank1, NULL, NULL, _gActor403200Animation1ED88Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation1EE60Bank1[6] = {
#include "assets/actor_403200_animation_1EE60_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation1EE60Bank4[4] = {
#include "assets/actor_403200_animation_1EE60_bank4.inc"
};

static AnimationRecord _gActor403200Animation1EE60Records[20] = {
#include "assets/actor_403200_animation_1EE60_records.inc"
};

static u16 _gActor403200Animation1EE60Indices[4] = {
#include "assets/actor_403200_animation_1EE60_indices.inc"
};

static AnimationSet _gActor403200Animation1EE60 = {
    _gActor403200Animation1EE60Records,
    _gActor403200Animation1EE60Indices,
    { NULL, _gActor403200Animation1EE60Bank1, NULL, NULL, _gActor403200Animation1EE60Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation1EF3CBank1[6] = {
#include "assets/actor_403200_animation_1EF3C_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation1EF3CBank4[5] = {
#include "assets/actor_403200_animation_1EF3C_bank4.inc"
};

static AnimationRecord _gActor403200Animation1EF3CRecords[20] = {
#include "assets/actor_403200_animation_1EF3C_records.inc"
};

static u16 _gActor403200Animation1EF3CIndices[4] = {
#include "assets/actor_403200_animation_1EF3C_indices.inc"
};

static AnimationSet _gActor403200Animation1EF3C = {
    _gActor403200Animation1EF3CRecords,
    _gActor403200Animation1EF3CIndices,
    { NULL, _gActor403200Animation1EF3CBank1, NULL, NULL, _gActor403200Animation1EF3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation1F324Bank1[18] = {
#include "assets/actor_403200_animation_1F324_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation1F324Bank4[72] = {
#include "assets/actor_403200_animation_1F324_bank4.inc"
};

static AnimationRecord _gActor403200Animation1F324Records[110] = {
#include "assets/actor_403200_animation_1F324_records.inc"
};

static u16 _gActor403200Animation1F324Indices[8] = {
#include "assets/actor_403200_animation_1F324_indices.inc"
};

static AnimationSet _gActor403200Animation1F324 = {
    _gActor403200Animation1F324Records,
    _gActor403200Animation1F324Indices,
    { NULL, _gActor403200Animation1F324Bank1, NULL, NULL, _gActor403200Animation1F324Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation1F6E4Bank1[39] = {
#include "assets/actor_403200_animation_1F6E4_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation1F6E4Bank4[32] = {
#include "assets/actor_403200_animation_1F6E4_bank4.inc"
};

static AnimationRecord _gActor403200Animation1F6E4Records[79] = {
#include "assets/actor_403200_animation_1F6E4_records.inc"
};

static u16 _gActor403200Animation1F6E4Indices[4] = {
#include "assets/actor_403200_animation_1F6E4_indices.inc"
};

static AnimationSet _gActor403200Animation1F6E4 = {
    _gActor403200Animation1F6E4Records,
    _gActor403200Animation1F6E4Indices,
    { NULL, _gActor403200Animation1F6E4Bank1, NULL, NULL, _gActor403200Animation1F6E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation1F9F8Bank1[32] = {
#include "assets/actor_403200_animation_1F9F8_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation1F9F8Bank4[23] = {
#include "assets/actor_403200_animation_1F9F8_bank4.inc"
};

static AnimationRecord _gActor403200Animation1F9F8Records[66] = {
#include "assets/actor_403200_animation_1F9F8_records.inc"
};

static u16 _gActor403200Animation1F9F8Indices[4] = {
#include "assets/actor_403200_animation_1F9F8_indices.inc"
};

static AnimationSet _gActor403200Animation1F9F8 = {
    _gActor403200Animation1F9F8Records,
    _gActor403200Animation1F9F8Indices,
    { NULL, _gActor403200Animation1F9F8Bank1, NULL, NULL, _gActor403200Animation1F9F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation1FDDCBank1[11] = {
#include "assets/actor_403200_animation_1FDDC_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation1FDDCBank4[78] = {
#include "assets/actor_403200_animation_1FDDC_bank4.inc"
};

static AnimationRecord _gActor403200Animation1FDDCRecords[124] = {
#include "assets/actor_403200_animation_1FDDC_records.inc"
};

static u16 _gActor403200Animation1FDDCIndices[8] = {
#include "assets/actor_403200_animation_1FDDC_indices.inc"
};

static AnimationSet _gActor403200Animation1FDDC = {
    _gActor403200Animation1FDDCRecords,
    _gActor403200Animation1FDDCIndices,
    { NULL, _gActor403200Animation1FDDCBank1, NULL, NULL, _gActor403200Animation1FDDCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation20234Bank1[47] = {
#include "assets/actor_403200_animation_20234_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation20234Bank4[30] = {
#include "assets/actor_403200_animation_20234_bank4.inc"
};

static AnimationRecord _gActor403200Animation20234Records[95] = {
#include "assets/actor_403200_animation_20234_records.inc"
};

static u16 _gActor403200Animation20234Indices[4] = {
#include "assets/actor_403200_animation_20234_indices.inc"
};

static AnimationSet _gActor403200Animation20234 = {
    _gActor403200Animation20234Records,
    _gActor403200Animation20234Indices,
    { NULL, _gActor403200Animation20234Bank1, NULL, NULL, _gActor403200Animation20234Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation2065CBank1[46] = {
#include "assets/actor_403200_animation_2065C_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation2065CBank4[27] = {
#include "assets/actor_403200_animation_2065C_bank4.inc"
};

static AnimationRecord _gActor403200Animation2065CRecords[89] = {
#include "assets/actor_403200_animation_2065C_records.inc"
};

static u16 _gActor403200Animation2065CIndices[4] = {
#include "assets/actor_403200_animation_2065C_indices.inc"
};

static AnimationSet _gActor403200Animation2065C = {
    _gActor403200Animation2065CRecords,
    _gActor403200Animation2065CIndices,
    { NULL, _gActor403200Animation2065CBank1, NULL, NULL, _gActor403200Animation2065CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation20A08Bank1[13] = {
#include "assets/actor_403200_animation_20A08_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation20A08Bank4[77] = {
#include "assets/actor_403200_animation_20A08_bank4.inc"
};

static AnimationRecord _gActor403200Animation20A08Records[105] = {
#include "assets/actor_403200_animation_20A08_records.inc"
};

static u16 _gActor403200Animation20A08Indices[8] = {
#include "assets/actor_403200_animation_20A08_indices.inc"
};

static AnimationSet _gActor403200Animation20A08 = {
    _gActor403200Animation20A08Records,
    _gActor403200Animation20A08Indices,
    { NULL, _gActor403200Animation20A08Bank1, NULL, NULL, _gActor403200Animation20A08Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation20D10Bank1[33] = {
#include "assets/actor_403200_animation_20D10_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation20D10Bank4[21] = {
#include "assets/actor_403200_animation_20D10_bank4.inc"
};

static AnimationRecord _gActor403200Animation20D10Records[62] = {
#include "assets/actor_403200_animation_20D10_records.inc"
};

static u16 _gActor403200Animation20D10Indices[4] = {
#include "assets/actor_403200_animation_20D10_indices.inc"
};

static AnimationSet _gActor403200Animation20D10 = {
    _gActor403200Animation20D10Records,
    _gActor403200Animation20D10Indices,
    { NULL, _gActor403200Animation20D10Bank1, NULL, NULL, _gActor403200Animation20D10Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation20FF8Bank1[31] = {
#include "assets/actor_403200_animation_20FF8_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation20FF8Bank4[21] = {
#include "assets/actor_403200_animation_20FF8_bank4.inc"
};

static AnimationRecord _gActor403200Animation20FF8Records[60] = {
#include "assets/actor_403200_animation_20FF8_records.inc"
};

static u16 _gActor403200Animation20FF8Indices[4] = {
#include "assets/actor_403200_animation_20FF8_indices.inc"
};

static AnimationSet _gActor403200Animation20FF8 = {
    _gActor403200Animation20FF8Records,
    _gActor403200Animation20FF8Indices,
    { NULL, _gActor403200Animation20FF8Bank1, NULL, NULL, _gActor403200Animation20FF8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation21260Bank1[13] = {
#include "assets/actor_403200_animation_21260_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation21260Bank4[38] = {
#include "assets/actor_403200_animation_21260_bank4.inc"
};

static AnimationRecord _gActor403200Animation21260Records[63] = {
#include "assets/actor_403200_animation_21260_records.inc"
};

static u16 _gActor403200Animation21260Indices[8] = {
#include "assets/actor_403200_animation_21260_indices.inc"
};

static AnimationSet _gActor403200Animation21260 = {
    _gActor403200Animation21260Records,
    _gActor403200Animation21260Indices,
    { NULL, _gActor403200Animation21260Bank1, NULL, NULL, _gActor403200Animation21260Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation2132CBank1[6] = {
#include "assets/actor_403200_animation_2132C_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation2132CBank4[5] = {
#include "assets/actor_403200_animation_2132C_bank4.inc"
};

static AnimationRecord _gActor403200Animation2132CRecords[16] = {
#include "assets/actor_403200_animation_2132C_records.inc"
};

static u16 _gActor403200Animation2132CIndices[4] = {
#include "assets/actor_403200_animation_2132C_indices.inc"
};

static AnimationSet _gActor403200Animation2132C = {
    _gActor403200Animation2132CRecords,
    _gActor403200Animation2132CIndices,
    { NULL, _gActor403200Animation2132CBank1, NULL, NULL, _gActor403200Animation2132CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation213F8Bank1[6] = {
#include "assets/actor_403200_animation_213F8_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation213F8Bank4[5] = {
#include "assets/actor_403200_animation_213F8_bank4.inc"
};

static AnimationRecord _gActor403200Animation213F8Records[16] = {
#include "assets/actor_403200_animation_213F8_records.inc"
};

static u16 _gActor403200Animation213F8Indices[4] = {
#include "assets/actor_403200_animation_213F8_indices.inc"
};

static AnimationSet _gActor403200Animation213F8 = {
    _gActor403200Animation213F8Records,
    _gActor403200Animation213F8Indices,
    { NULL, _gActor403200Animation213F8Bank1, NULL, NULL, _gActor403200Animation213F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation216D0Bank1[8] = {
#include "assets/actor_403200_animation_216D0_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation216D0Bank4[49] = {
#include "assets/actor_403200_animation_216D0_bank4.inc"
};

static AnimationRecord _gActor403200Animation216D0Records[95] = {
#include "assets/actor_403200_animation_216D0_records.inc"
};

static u16 _gActor403200Animation216D0Indices[8] = {
#include "assets/actor_403200_animation_216D0_indices.inc"
};

static AnimationSet _gActor403200Animation216D0 = {
    _gActor403200Animation216D0Records,
    _gActor403200Animation216D0Indices,
    { NULL, _gActor403200Animation216D0Bank1, NULL, NULL, _gActor403200Animation216D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation21914Bank1[22] = {
#include "assets/actor_403200_animation_21914_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation21914Bank4[12] = {
#include "assets/actor_403200_animation_21914_bank4.inc"
};

static AnimationRecord _gActor403200Animation21914Records[55] = {
#include "assets/actor_403200_animation_21914_records.inc"
};

static u16 _gActor403200Animation21914Indices[4] = {
#include "assets/actor_403200_animation_21914_indices.inc"
};

static AnimationSet _gActor403200Animation21914 = {
    _gActor403200Animation21914Records,
    _gActor403200Animation21914Indices,
    { NULL, _gActor403200Animation21914Bank1, NULL, NULL, _gActor403200Animation21914Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation21B54Bank1[23] = {
#include "assets/actor_403200_animation_21B54_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation21B54Bank4[10] = {
#include "assets/actor_403200_animation_21B54_bank4.inc"
};

static AnimationRecord _gActor403200Animation21B54Records[53] = {
#include "assets/actor_403200_animation_21B54_records.inc"
};

static u16 _gActor403200Animation21B54Indices[4] = {
#include "assets/actor_403200_animation_21B54_indices.inc"
};

static AnimationSet _gActor403200Animation21B54 = {
    _gActor403200Animation21B54Records,
    _gActor403200Animation21B54Indices,
    { NULL, _gActor403200Animation21B54Bank1, NULL, NULL, _gActor403200Animation21B54Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation21D5CBank1[9] = {
#include "assets/actor_403200_animation_21D5C_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation21D5CBank4[33] = {
#include "assets/actor_403200_animation_21D5C_bank4.inc"
};

static AnimationRecord _gActor403200Animation21D5CRecords[56] = {
#include "assets/actor_403200_animation_21D5C_records.inc"
};

static u16 _gActor403200Animation21D5CIndices[8] = {
#include "assets/actor_403200_animation_21D5C_indices.inc"
};

static AnimationSet _gActor403200Animation21D5C = {
    _gActor403200Animation21D5CRecords,
    _gActor403200Animation21D5CIndices,
    { NULL, _gActor403200Animation21D5CBank1, NULL, NULL, _gActor403200Animation21D5CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation21EE8Bank1[15] = {
#include "assets/actor_403200_animation_21EE8_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation21EE8Bank4[11] = {
#include "assets/actor_403200_animation_21EE8_bank4.inc"
};

static AnimationRecord _gActor403200Animation21EE8Records[31] = {
#include "assets/actor_403200_animation_21EE8_records.inc"
};

static u16 _gActor403200Animation21EE8Indices[4] = {
#include "assets/actor_403200_animation_21EE8_indices.inc"
};

static AnimationSet _gActor403200Animation21EE8 = {
    _gActor403200Animation21EE8Records,
    _gActor403200Animation21EE8Indices,
    { NULL, _gActor403200Animation21EE8Bank1, NULL, NULL, _gActor403200Animation21EE8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation2207CBank1[15] = {
#include "assets/actor_403200_animation_2207C_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation2207CBank4[12] = {
#include "assets/actor_403200_animation_2207C_bank4.inc"
};

static AnimationRecord _gActor403200Animation2207CRecords[32] = {
#include "assets/actor_403200_animation_2207C_records.inc"
};

static u16 _gActor403200Animation2207CIndices[4] = {
#include "assets/actor_403200_animation_2207C_indices.inc"
};

static AnimationSet _gActor403200Animation2207C = {
    _gActor403200Animation2207CRecords,
    _gActor403200Animation2207CIndices,
    { NULL, _gActor403200Animation2207CBank1, NULL, NULL, _gActor403200Animation2207CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation224B4Bank1[22] = {
#include "assets/actor_403200_animation_224B4_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation224B4Bank4[73] = {
#include "assets/actor_403200_animation_224B4_bank4.inc"
};

static AnimationRecord _gActor403200Animation224B4Records[117] = {
#include "assets/actor_403200_animation_224B4_records.inc"
};

static u16 _gActor403200Animation224B4Indices[8] = {
#include "assets/actor_403200_animation_224B4_indices.inc"
};

static AnimationSet _gActor403200Animation224B4 = {
    _gActor403200Animation224B4Records,
    _gActor403200Animation224B4Indices,
    { NULL, _gActor403200Animation224B4Bank1, NULL, NULL, _gActor403200Animation224B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation227F8Bank1[33] = {
#include "assets/actor_403200_animation_227F8_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation227F8Bank4[29] = {
#include "assets/actor_403200_animation_227F8_bank4.inc"
};

static AnimationRecord _gActor403200Animation227F8Records[69] = {
#include "assets/actor_403200_animation_227F8_records.inc"
};

static u16 _gActor403200Animation227F8Indices[4] = {
#include "assets/actor_403200_animation_227F8_indices.inc"
};

static AnimationSet _gActor403200Animation227F8 = {
    _gActor403200Animation227F8Records,
    _gActor403200Animation227F8Indices,
    { NULL, _gActor403200Animation227F8Bank1, NULL, NULL, _gActor403200Animation227F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation22B10Bank1[31] = {
#include "assets/actor_403200_animation_22B10_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation22B10Bank4[26] = {
#include "assets/actor_403200_animation_22B10_bank4.inc"
};

static AnimationRecord _gActor403200Animation22B10Records[67] = {
#include "assets/actor_403200_animation_22B10_records.inc"
};

static u16 _gActor403200Animation22B10Indices[4] = {
#include "assets/actor_403200_animation_22B10_indices.inc"
};

static AnimationSet _gActor403200Animation22B10 = {
    _gActor403200Animation22B10Records,
    _gActor403200Animation22B10Indices,
    { NULL, _gActor403200Animation22B10Bank1, NULL, NULL, _gActor403200Animation22B10Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation22E58Bank1[5] = {
#include "assets/actor_403200_animation_22E58_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation22E58Bank4[76] = {
#include "assets/actor_403200_animation_22E58_bank4.inc"
};

static AnimationRecord _gActor403200Animation22E58Records[105] = {
#include "assets/actor_403200_animation_22E58_records.inc"
};

static u16 _gActor403200Animation22E58Indices[8] = {
#include "assets/actor_403200_animation_22E58_indices.inc"
};

static AnimationSet _gActor403200Animation22E58 = {
    _gActor403200Animation22E58Records,
    _gActor403200Animation22E58Indices,
    { NULL, _gActor403200Animation22E58Bank1, NULL, NULL, _gActor403200Animation22E58Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation2318CBank1[39] = {
#include "assets/actor_403200_animation_2318C_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation2318CBank4[14] = {
#include "assets/actor_403200_animation_2318C_bank4.inc"
};

static AnimationRecord _gActor403200Animation2318CRecords[62] = {
#include "assets/actor_403200_animation_2318C_records.inc"
};

static u16 _gActor403200Animation2318CIndices[4] = {
#include "assets/actor_403200_animation_2318C_indices.inc"
};

static AnimationSet _gActor403200Animation2318C = {
    _gActor403200Animation2318CRecords,
    _gActor403200Animation2318CIndices,
    { NULL, _gActor403200Animation2318CBank1, NULL, NULL, _gActor403200Animation2318CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation234B8Bank1[38] = {
#include "assets/actor_403200_animation_234B8_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation234B8Bank4[15] = {
#include "assets/actor_403200_animation_234B8_bank4.inc"
};

static AnimationRecord _gActor403200Animation234B8Records[62] = {
#include "assets/actor_403200_animation_234B8_records.inc"
};

static u16 _gActor403200Animation234B8Indices[4] = {
#include "assets/actor_403200_animation_234B8_indices.inc"
};

static AnimationSet _gActor403200Animation234B8 = {
    _gActor403200Animation234B8Records,
    _gActor403200Animation234B8Indices,
    { NULL, _gActor403200Animation234B8Bank1, NULL, NULL, _gActor403200Animation234B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation23998Bank1[24] = {
#include "assets/actor_403200_animation_23998_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation23998Bank4[88] = {
#include "assets/actor_403200_animation_23998_bank4.inc"
};

static AnimationRecord _gActor403200Animation23998Records[138] = {
#include "assets/actor_403200_animation_23998_records.inc"
};

static u16 _gActor403200Animation23998Indices[8] = {
#include "assets/actor_403200_animation_23998_indices.inc"
};

static AnimationSet _gActor403200Animation23998 = {
    _gActor403200Animation23998Records,
    _gActor403200Animation23998Indices,
    { NULL, _gActor403200Animation23998Bank1, NULL, NULL, _gActor403200Animation23998Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation23C2CBank1[26] = {
#include "assets/actor_403200_animation_23C2C_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation23C2CBank4[19] = {
#include "assets/actor_403200_animation_23C2C_bank4.inc"
};

static AnimationRecord _gActor403200Animation23C2CRecords[56] = {
#include "assets/actor_403200_animation_23C2C_records.inc"
};

static u16 _gActor403200Animation23C2CIndices[4] = {
#include "assets/actor_403200_animation_23C2C_indices.inc"
};

static AnimationSet _gActor403200Animation23C2C = {
    _gActor403200Animation23C2CRecords,
    _gActor403200Animation23C2CIndices,
    { NULL, _gActor403200Animation23C2CBank1, NULL, NULL, _gActor403200Animation23C2CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation23F38Bank1[30] = {
#include "assets/actor_403200_animation_23F38_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation23F38Bank4[26] = {
#include "assets/actor_403200_animation_23F38_bank4.inc"
};

static AnimationRecord _gActor403200Animation23F38Records[67] = {
#include "assets/actor_403200_animation_23F38_records.inc"
};

static u16 _gActor403200Animation23F38Indices[4] = {
#include "assets/actor_403200_animation_23F38_indices.inc"
};

static AnimationSet _gActor403200Animation23F38 = {
    _gActor403200Animation23F38Records,
    _gActor403200Animation23F38Indices,
    { NULL, _gActor403200Animation23F38Bank1, NULL, NULL, _gActor403200Animation23F38Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation24FFCBank1[53] = {
#include "assets/actor_403200_animation_24FFC_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation24FFCBank4[373] = {
#include "assets/actor_403200_animation_24FFC_bank4.inc"
};

static AnimationRecord _gActor403200Animation24FFCRecords[521] = {
#include "assets/actor_403200_animation_24FFC_records.inc"
};

static u16 _gActor403200Animation24FFCIndices[20] = {
#include "assets/actor_403200_animation_24FFC_indices.inc"
};

static AnimationSet _gActor403200Animation24FFC = {
    _gActor403200Animation24FFCRecords,
    _gActor403200Animation24FFCIndices,
    { NULL, _gActor403200Animation24FFCBank1, NULL, NULL, _gActor403200Animation24FFCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation25B98Bank1[36] = {
#include "assets/actor_403200_animation_25B98_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation25B98Bank4[255] = {
#include "assets/actor_403200_animation_25B98_bank4.inc"
};

static AnimationRecord _gActor403200Animation25B98Records[360] = {
#include "assets/actor_403200_animation_25B98_records.inc"
};

static u16 _gActor403200Animation25B98Indices[20] = {
#include "assets/actor_403200_animation_25B98_indices.inc"
};

static AnimationSet _gActor403200Animation25B98 = {
    _gActor403200Animation25B98Records,
    _gActor403200Animation25B98Indices,
    { NULL, _gActor403200Animation25B98Bank1, NULL, NULL, _gActor403200Animation25B98Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation25CA0Bank1[4] = {
#include "assets/actor_403200_animation_25CA0_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation25CA0Bank4[10] = {
#include "assets/actor_403200_animation_25CA0_bank4.inc"
};

static AnimationRecord _gActor403200Animation25CA0Records[30] = {
#include "assets/actor_403200_animation_25CA0_records.inc"
};

static u16 _gActor403200Animation25CA0Indices[8] = {
#include "assets/actor_403200_animation_25CA0_indices.inc"
};

static AnimationSet _gActor403200Animation25CA0 = {
    _gActor403200Animation25CA0Records,
    _gActor403200Animation25CA0Indices,
    { NULL, _gActor403200Animation25CA0Bank1, NULL, NULL, _gActor403200Animation25CA0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation25D80Bank1[5] = {
#include "assets/actor_403200_animation_25D80_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation25D80Bank4[8] = {
#include "assets/actor_403200_animation_25D80_bank4.inc"
};

static AnimationRecord _gActor403200Animation25D80Records[21] = {
#include "assets/actor_403200_animation_25D80_records.inc"
};

static u16 _gActor403200Animation25D80Indices[4] = {
#include "assets/actor_403200_animation_25D80_indices.inc"
};

static AnimationSet _gActor403200Animation25D80 = {
    _gActor403200Animation25D80Records,
    _gActor403200Animation25D80Indices,
    { NULL, _gActor403200Animation25D80Bank1, NULL, NULL, _gActor403200Animation25D80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation25E60Bank1[5] = {
#include "assets/actor_403200_animation_25E60_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation25E60Bank4[8] = {
#include "assets/actor_403200_animation_25E60_bank4.inc"
};

static AnimationRecord _gActor403200Animation25E60Records[21] = {
#include "assets/actor_403200_animation_25E60_records.inc"
};

static u16 _gActor403200Animation25E60Indices[4] = {
#include "assets/actor_403200_animation_25E60_indices.inc"
};

static AnimationSet _gActor403200Animation25E60 = {
    _gActor403200Animation25E60Records,
    _gActor403200Animation25E60Indices,
    { NULL, _gActor403200Animation25E60Bank1, NULL, NULL, _gActor403200Animation25E60Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation25F68Bank1[6] = {
#include "assets/actor_403200_animation_25F68_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation25F68Bank4[3] = {
#include "assets/actor_403200_animation_25F68_bank4.inc"
};

static AnimationRecord _gActor403200Animation25F68Records[31] = {
#include "assets/actor_403200_animation_25F68_records.inc"
};

static u16 _gActor403200Animation25F68Indices[8] = {
#include "assets/actor_403200_animation_25F68_indices.inc"
};

static AnimationSet _gActor403200Animation25F68 = {
    _gActor403200Animation25F68Records,
    _gActor403200Animation25F68Indices,
    { NULL, _gActor403200Animation25F68Bank1, NULL, NULL, _gActor403200Animation25F68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation26030Bank1[6] = {
#include "assets/actor_403200_animation_26030_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation26030Bank4[1] = {
#include "assets/actor_403200_animation_26030_bank4.inc"
};

static AnimationRecord _gActor403200Animation26030Records[19] = {
#include "assets/actor_403200_animation_26030_records.inc"
};

static u16 _gActor403200Animation26030Indices[4] = {
#include "assets/actor_403200_animation_26030_indices.inc"
};

static AnimationSet _gActor403200Animation26030 = {
    _gActor403200Animation26030Records,
    _gActor403200Animation26030Indices,
    { NULL, _gActor403200Animation26030Bank1, NULL, NULL, _gActor403200Animation26030Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation260F8Bank1[6] = {
#include "assets/actor_403200_animation_260F8_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation260F8Bank4[1] = {
#include "assets/actor_403200_animation_260F8_bank4.inc"
};

static AnimationRecord _gActor403200Animation260F8Records[19] = {
#include "assets/actor_403200_animation_260F8_records.inc"
};

static u16 _gActor403200Animation260F8Indices[4] = {
#include "assets/actor_403200_animation_260F8_indices.inc"
};

static AnimationSet _gActor403200Animation260F8 = {
    _gActor403200Animation260F8Records,
    _gActor403200Animation260F8Indices,
    { NULL, _gActor403200Animation260F8Bank1, NULL, NULL, _gActor403200Animation260F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation261F0Bank1[2] = {
#include "assets/actor_403200_animation_261F0_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation261F0Bank4[9] = {
#include "assets/actor_403200_animation_261F0_bank4.inc"
};

static AnimationRecord _gActor403200Animation261F0Records[33] = {
#include "assets/actor_403200_animation_261F0_records.inc"
};

static u16 _gActor403200Animation261F0Indices[8] = {
#include "assets/actor_403200_animation_261F0_indices.inc"
};

static AnimationSet _gActor403200Animation261F0 = {
    _gActor403200Animation261F0Records,
    _gActor403200Animation261F0Indices,
    { NULL, _gActor403200Animation261F0Bank1, NULL, NULL, _gActor403200Animation261F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation262B8Bank1[4] = {
#include "assets/actor_403200_animation_262B8_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation262B8Bank4[5] = {
#include "assets/actor_403200_animation_262B8_bank4.inc"
};

static AnimationRecord _gActor403200Animation262B8Records[21] = {
#include "assets/actor_403200_animation_262B8_records.inc"
};

static u16 _gActor403200Animation262B8Indices[4] = {
#include "assets/actor_403200_animation_262B8_indices.inc"
};

static AnimationSet _gActor403200Animation262B8 = {
    _gActor403200Animation262B8Records,
    _gActor403200Animation262B8Indices,
    { NULL, _gActor403200Animation262B8Bank1, NULL, NULL, _gActor403200Animation262B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation26380Bank1[4] = {
#include "assets/actor_403200_animation_26380_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation26380Bank4[5] = {
#include "assets/actor_403200_animation_26380_bank4.inc"
};

static AnimationRecord _gActor403200Animation26380Records[21] = {
#include "assets/actor_403200_animation_26380_records.inc"
};

static u16 _gActor403200Animation26380Indices[4] = {
#include "assets/actor_403200_animation_26380_indices.inc"
};

static AnimationSet _gActor403200Animation26380 = {
    _gActor403200Animation26380Records,
    _gActor403200Animation26380Indices,
    { NULL, _gActor403200Animation26380Bank1, NULL, NULL, _gActor403200Animation26380Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation26AB8Bank1[3] = {
#include "assets/actor_403200_animation_26AB8_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation26AB8Bank4[203] = {
#include "assets/actor_403200_animation_26AB8_bank4.inc"
};

static AnimationRecord _gActor403200Animation26AB8Records[236] = {
#include "assets/actor_403200_animation_26AB8_records.inc"
};

static u16 _gActor403200Animation26AB8Indices[8] = {
#include "assets/actor_403200_animation_26AB8_indices.inc"
};

static AnimationSet _gActor403200Animation26AB8 = {
    _gActor403200Animation26AB8Records,
    _gActor403200Animation26AB8Indices,
    { NULL, _gActor403200Animation26AB8Bank1, NULL, NULL, _gActor403200Animation26AB8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation27F04Bank1[279] = {
#include "assets/actor_403200_animation_27F04_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation27F04Bank4[73] = {
#include "assets/actor_403200_animation_27F04_bank4.inc"
};

static AnimationRecord _gActor403200Animation27F04Records[377] = {
#include "assets/actor_403200_animation_27F04_records.inc"
};

static u16 _gActor403200Animation27F04Indices[4] = {
#include "assets/actor_403200_animation_27F04_indices.inc"
};

static AnimationSet _gActor403200Animation27F04 = {
    _gActor403200Animation27F04Records,
    _gActor403200Animation27F04Indices,
    { NULL, _gActor403200Animation27F04Bank1, NULL, NULL, _gActor403200Animation27F04Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation293D8Bank1[287] = {
#include "assets/actor_403200_animation_293D8_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation293D8Bank4[72] = {
#include "assets/actor_403200_animation_293D8_bank4.inc"
};

static AnimationRecord _gActor403200Animation293D8Records[388] = {
#include "assets/actor_403200_animation_293D8_records.inc"
};

static u16 _gActor403200Animation293D8Indices[4] = {
#include "assets/actor_403200_animation_293D8_indices.inc"
};

static AnimationSet _gActor403200Animation293D8 = {
    _gActor403200Animation293D8Records,
    _gActor403200Animation293D8Indices,
    { NULL, _gActor403200Animation293D8Bank1, NULL, NULL, _gActor403200Animation293D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation29B70Bank1[50] = {
#include "assets/actor_403200_animation_29B70_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation29B70Bank4[120] = {
#include "assets/actor_403200_animation_29B70_bank4.inc"
};

static AnimationRecord _gActor403200Animation29B70Records[202] = {
#include "assets/actor_403200_animation_29B70_records.inc"
};

static u16 _gActor403200Animation29B70Indices[8] = {
#include "assets/actor_403200_animation_29B70_indices.inc"
};

static AnimationSet _gActor403200Animation29B70 = {
    _gActor403200Animation29B70Records,
    _gActor403200Animation29B70Indices,
    { NULL, _gActor403200Animation29B70Bank1, NULL, NULL, _gActor403200Animation29B70Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation2A3F4Bank1[104] = {
#include "assets/actor_403200_animation_2A3F4_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation2A3F4Bank4[55] = {
#include "assets/actor_403200_animation_2A3F4_bank4.inc"
};

static AnimationRecord _gActor403200Animation2A3F4Records[166] = {
#include "assets/actor_403200_animation_2A3F4_records.inc"
};

static u16 _gActor403200Animation2A3F4Indices[4] = {
#include "assets/actor_403200_animation_2A3F4_indices.inc"
};

static AnimationSet _gActor403200Animation2A3F4 = {
    _gActor403200Animation2A3F4Records,
    _gActor403200Animation2A3F4Indices,
    { NULL, _gActor403200Animation2A3F4Bank1, NULL, NULL, _gActor403200Animation2A3F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation2AC80Bank1[104] = {
#include "assets/actor_403200_animation_2AC80_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation2AC80Bank4[54] = {
#include "assets/actor_403200_animation_2AC80_bank4.inc"
};

static AnimationRecord _gActor403200Animation2AC80Records[169] = {
#include "assets/actor_403200_animation_2AC80_records.inc"
};

static u16 _gActor403200Animation2AC80Indices[4] = {
#include "assets/actor_403200_animation_2AC80_indices.inc"
};

static AnimationSet _gActor403200Animation2AC80 = {
    _gActor403200Animation2AC80Records,
    _gActor403200Animation2AC80Indices,
    { NULL, _gActor403200Animation2AC80Bank1, NULL, NULL, _gActor403200Animation2AC80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation2AD60Bank1[3] = {
#include "assets/actor_403200_animation_2AD60_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation2AD60Bank4[7] = {
#include "assets/actor_403200_animation_2AD60_bank4.inc"
};

static AnimationRecord _gActor403200Animation2AD60Records[26] = {
#include "assets/actor_403200_animation_2AD60_records.inc"
};

static u16 _gActor403200Animation2AD60Indices[8] = {
#include "assets/actor_403200_animation_2AD60_indices.inc"
};

static AnimationSet _gActor403200Animation2AD60 = {
    _gActor403200Animation2AD60Records,
    _gActor403200Animation2AD60Indices,
    { NULL, _gActor403200Animation2AD60Bank1, NULL, NULL, _gActor403200Animation2AD60Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation2ADECBank1[3] = {
#include "assets/actor_403200_animation_2ADEC_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation2ADECBank4[2] = {
#include "assets/actor_403200_animation_2ADEC_bank4.inc"
};

static AnimationRecord _gActor403200Animation2ADECRecords[12] = {
#include "assets/actor_403200_animation_2ADEC_records.inc"
};

static u16 _gActor403200Animation2ADECIndices[4] = {
#include "assets/actor_403200_animation_2ADEC_indices.inc"
};

static AnimationSet _gActor403200Animation2ADEC = {
    _gActor403200Animation2ADECRecords,
    _gActor403200Animation2ADECIndices,
    { NULL, _gActor403200Animation2ADECBank1, NULL, NULL, _gActor403200Animation2ADECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation2AE78Bank1[3] = {
#include "assets/actor_403200_animation_2AE78_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation2AE78Bank4[2] = {
#include "assets/actor_403200_animation_2AE78_bank4.inc"
};

static AnimationRecord _gActor403200Animation2AE78Records[12] = {
#include "assets/actor_403200_animation_2AE78_records.inc"
};

static u16 _gActor403200Animation2AE78Indices[4] = {
#include "assets/actor_403200_animation_2AE78_indices.inc"
};

static AnimationSet _gActor403200Animation2AE78 = {
    _gActor403200Animation2AE78Records,
    _gActor403200Animation2AE78Indices,
    { NULL, _gActor403200Animation2AE78Bank1, NULL, NULL, _gActor403200Animation2AE78Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation2B644Bank1[14] = {
#include "assets/actor_403200_animation_2B644_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation2B644Bank4[158] = {
#include "assets/actor_403200_animation_2B644_bank4.inc"
};

static AnimationRecord _gActor403200Animation2B644Records[279] = {
#include "assets/actor_403200_animation_2B644_records.inc"
};

static u16 _gActor403200Animation2B644Indices[20] = {
#include "assets/actor_403200_animation_2B644_indices.inc"
};

static AnimationSet _gActor403200Animation2B644 = {
    _gActor403200Animation2B644Records,
    _gActor403200Animation2B644Indices,
    { NULL, _gActor403200Animation2B644Bank1, NULL, NULL, _gActor403200Animation2B644Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation2BE50Bank1[15] = {
#include "assets/actor_403200_animation_2BE50_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation2BE50Bank4[206] = {
#include "assets/actor_403200_animation_2BE50_bank4.inc"
};

static AnimationRecord _gActor403200Animation2BE50Records[244] = {
#include "assets/actor_403200_animation_2BE50_records.inc"
};

static u16 _gActor403200Animation2BE50Indices[20] = {
#include "assets/actor_403200_animation_2BE50_indices.inc"
};

static AnimationSet _gActor403200Animation2BE50 = {
    _gActor403200Animation2BE50Records,
    _gActor403200Animation2BE50Indices,
    { NULL, _gActor403200Animation2BE50Bank1, NULL, NULL, _gActor403200Animation2BE50Bank4, NULL, NULL, NULL },
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
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 20, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
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

AnimationSet* D_actor_403200_8015E484[46] = {
    NULL,
    &_gActor403200Animation1D9C0,
    &_gActor403200Animation1DE00,
    &_gActor403200Animation1E6F0,
    &_gActor403200Animation1FDDC,
    &_gActor403200Animation20A08,
    &_gActor403200Animation1ED88,
    &_gActor403200Animation25CA0,
    &_gActor403200Animation25F68,
    &_gActor403200Animation261F0,
    &_gActor403200Animation2AD60,
    &_gActor403200Animation1F324,
    &_gActor403200Animation21260,
    &_gActor403200Animation22E58,
    &_gActor403200Animation216D0,
    &_gActor403200Animation21D5C,
    &_gActor403200Animation224B4,
    &_gActor403200Animation23998,
    &_gActor403200Animation26AB8,
    &_gActor403200Animation1F324,
    &_gActor403200Animation29B70,
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

AnimationSet* D_actor_403200_8015E53C[46] = {
    NULL,
    &_gActor403200Animation1DA6C,
    &_gActor403200Animation1E0C4,
    &_gActor403200Animation1E8D0,
    &_gActor403200Animation20234,
    &_gActor403200Animation20D10,
    &_gActor403200Animation1EE60,
    &_gActor403200Animation25D80,
    &_gActor403200Animation26030,
    &_gActor403200Animation262B8,
    &_gActor403200Animation2ADEC,
    &_gActor403200Animation1F6E4,
    &_gActor403200Animation2132C,
    &_gActor403200Animation2318C,
    &_gActor403200Animation21914,
    &_gActor403200Animation21EE8,
    &_gActor403200Animation227F8,
    &_gActor403200Animation23C2C,
    &_gActor403200Animation27F04,
    &_gActor403200Animation1F6E4,
    &_gActor403200Animation2A3F4,
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

AnimationSet* D_actor_403200_8015E5F4[46] = {
    NULL,
    &_gActor403200Animation1DB18,
    &_gActor403200Animation1E3A0,
    &_gActor403200Animation1EAB0,
    &_gActor403200Animation2065C,
    &_gActor403200Animation20FF8,
    &_gActor403200Animation1EF3C,
    &_gActor403200Animation25E60,
    &_gActor403200Animation260F8,
    &_gActor403200Animation26380,
    &_gActor403200Animation2AE78,
    &_gActor403200Animation1F9F8,
    &_gActor403200Animation213F8,
    &_gActor403200Animation234B8,
    &_gActor403200Animation21B54,
    &_gActor403200Animation2207C,
    &_gActor403200Animation22B10,
    &_gActor403200Animation23F38,
    &_gActor403200Animation293D8,
    &_gActor403200Animation1F9F8,
    &_gActor403200Animation2AC80,
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

AnimationSet* D_actor_403200_8015E6AC[8] = {
    NULL,
    &_gActor403200Animation25B98,
    &_gActor403200Animation2BE50,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

AnimationSet* D_actor_403200_8015E6CC[7] = {
    NULL,
    &_gActor403200Animation24FFC,
    &_gActor403200Animation2BE50,
    NULL,
    NULL,
    NULL,
    NULL,
};

/// Picks the room's camera view for the host's current situation.
///
/// The host's tick calls one of these each frame, chosen by
/// `GluttonWork::viewSelector`, unless the view is locked. `host` is the
/// boss's task and `phase` is `GluttonWork::phase`, the step of the fight.
/// Returns the index of the view the room should show; the tick stores it as
/// the live view when it differs from the current one.
typedef s32 (*_Actor403200ViewFunc)(Task* host, s16 phase);

_Actor403200ViewFunc D_actor_403200_8015E6E8[9] = {
    _actor403200PickDefaultView,
    _actor403200PickRainView,
    _actor403200PickCombatView,
    _actor403200PickGlobView,
    _actor403200PickCloseRangeView,
    _actor403200PickDebrisView,
    _actor403200PickSwipeView,
    _actor403200PickPlayerXView,
    _actor403200PickAdvanceView,
};

u8 gGluttonRainGroup = 0;

AnimationSet* gGluttonCaughtAnimSets[7] = {
    NULL,
    &_gActor403200Animation2B644,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

TaskDesc D_actor_403200_8015E72C[7] = {
    { { { TASK_BODY_TMD, 96 } }, _gluttonPropTask, { .model = &_gActor403200GluttonLegRight } },
    { { { TASK_BODY_TMD, 96 } }, _gluttonPropTask, { .model = &_gActor403200GluttonLegLeft } },
    { { { TASK_BODY_TMD, 96 } }, _gluttonPropTask, { .model = &_gActor403200Model12884 } },
    { { { TASK_BODY_TMD, 96 } }, _gluttonPropTask, { .model = &_gActor403200Model13774 } },
    { { { TASK_BODY_TMD, 96 } }, _gluttonPropTask, { .model = &_gActor403200Model1785C } },
    { { { TASK_BODY_TMD, 96 } }, _gluttonPropTask, { .model = &_gActor403200Model18BE4 } },
    { { { TASK_BODY_TMD, 96 } }, _gluttonEscort6Task, { .model = &_gActor403200Model186D8 } },
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

TaskDesc gGluttonEscortTasks[5] = {
    { { { TASK_BODY_TMD, 96 } }, _gluttonGlobTask, { .model = &_gActor403200Model199E4 } },
    { { { TASK_BODY_COORD, 96 } }, _gluttonRainTask, { .value = 0 } },
    { { { TASK_BODY_COORD, 96 } }, _gluttonThrowTask, { .value = 0 } },
    { { { TASK_BODY_TMD, 96 } }, _gluttonChunkTask, { .model = &_gActor403200Model1AC48 } },
    { { { TASK_BODY_TMD, 96 } }, _gluttonSpinnerTask, { .model = &_gActor403200Model19284 } },
};

static AnimationPackedPose _gActor403200Animation2CDE0Bank1[7] = {
#include "assets/actor_403200_animation_2CDE0_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation2CDE0Bank4[65] = {
#include "assets/actor_403200_animation_2CDE0_bank4.inc"
};

static AnimationRecord _gActor403200Animation2CDE0Records[123] = {
#include "assets/actor_403200_animation_2CDE0_records.inc"
};

static u16 _gActor403200Animation2CDE0Indices[20] = {
#include "assets/actor_403200_animation_2CDE0_indices.inc"
};

AnimationSet gActor403200Animation2CDE0 = {
    _gActor403200Animation2CDE0Records,
    _gActor403200Animation2CDE0Indices,
    { NULL, _gActor403200Animation2CDE0Bank1, NULL, NULL, _gActor403200Animation2CDE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation2CF64Bank1[2] = {
#include "assets/actor_403200_animation_2CF64_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation2CF64Bank4[13] = {
#include "assets/actor_403200_animation_2CF64_bank4.inc"
};

static AnimationRecord _gActor403200Animation2CF64Records[58] = {
#include "assets/actor_403200_animation_2CF64_records.inc"
};

static u16 _gActor403200Animation2CF64Indices[20] = {
#include "assets/actor_403200_animation_2CF64_indices.inc"
};

AnimationSet gActor403200Animation2CF64 = {
    _gActor403200Animation2CF64Records,
    _gActor403200Animation2CF64Indices,
    { NULL, _gActor403200Animation2CF64Bank1, NULL, NULL, _gActor403200Animation2CF64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation2D1D0Bank1[4] = {
#include "assets/actor_403200_animation_2D1D0_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation2D1D0Bank4[35] = {
#include "assets/actor_403200_animation_2D1D0_bank4.inc"
};

static AnimationRecord _gActor403200Animation2D1D0Records[88] = {
#include "assets/actor_403200_animation_2D1D0_records.inc"
};

static u16 _gActor403200Animation2D1D0Indices[20] = {
#include "assets/actor_403200_animation_2D1D0_indices.inc"
};

AnimationSet gActor403200Animation2D1D0 = {
    _gActor403200Animation2D1D0Records,
    _gActor403200Animation2D1D0Indices,
    { NULL, _gActor403200Animation2D1D0Bank1, NULL, NULL, _gActor403200Animation2D1D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403200Animation2D928Bank1[16] = {
#include "assets/actor_403200_animation_2D928_bank1.inc"
};

static AnimationPackedRotation _gActor403200Animation2D928Bank4[153] = {
#include "assets/actor_403200_animation_2D928_bank4.inc"
};

static AnimationRecord _gActor403200Animation2D928Records[249] = {
#include "assets/actor_403200_animation_2D928_records.inc"
};

static u16 _gActor403200Animation2D928Indices[20] = {
#include "assets/actor_403200_animation_2D928_indices.inc"
};

AnimationSet gActor403200Animation2D928 = {
    _gActor403200Animation2D928Records,
    _gActor403200Animation2D928Indices,
    { NULL, _gActor403200Animation2D928Bank1, NULL, NULL, _gActor403200Animation2D928Bank4, NULL, NULL, NULL },
};

TaskMessageEntry D_actor_403200_8015F770[8] = {
    { 2015, _actor403200IgnoreMessage2015 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor403200SetModelDraw },
    { ACTOR_MESSAGE_IS_PRESENT, actorMsgIsPresent },
    { ACTOR_MESSAGE_PLACE, actorMsgPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor403200ApplyCommand },
    { ROOM_MESSAGE_ACTOR_EVENT, _actor403200HandleActorEvent },
    { 2014, _actor403200ReleaseGlobGrab },
    { TASK_MESSAGE_TABLE_END, NULL },
};

SVECTOR D_actor_403200_8015F7B0[3][9] = {
    { { 0x2EE0, 0, -1500, 0 }, { 0x32C8, 0, -1600, 0 }, { 0x3E80, 0, -1300, 0 }, { 0x2EE0, 0, -0x2904, 0 }, { 0x32C8, 0, -0x2968, 0 }, { 0x3E80, 0, -0x283C, 0 }, { 0x36B0, 0, -0x2904, 0 }, { 0x3A98, 0, -600, 0 }, { 0x4650, 0, -0x283C, 0 } },
    { { 0x2EE0, 0, -1500, 0 }, { 0x32C8, 0, -1600, 0 }, { 0x3E80, 0, -1300, 0 }, { 0x2EE0, 0, -0x2904, 0 }, { 0x32C8, 0, -0x2968, 0 }, { 0x3E80, 0, -0x283C, 0 }, { 0x36B0, 0, -0x2904, 0 }, { 0x3A98, 0, -600, 0 }, { 0x4650, 0, -0x283C, 0 } },
    { { 0x2EE0, 0, -1500, 0 }, { 0x32C8, 0, -1600, 0 }, { 0x3E80, 0, -1300, 0 }, { 0x2EE0, 0, -0x2904, 0 }, { 0x32C8, 0, -0x2968, 0 }, { 0x3E80, 0, -0x283C, 0 }, { 0x36B0, 0, -0x2904, 0 }, { 0x3A98, 0, -600, 0 }, { 0x4650, 0, -0x283C, 0 } },
};

_Actor403200SpinnerSpawn D_actor_403200_8015F888[9] = {
    { &_gActor403200Model1C474, 0 },
    { &_gActor403200GluttonProp, 1 },
    { &_gActor403200Model1CDA4, 2 },
    { &_gActor403200Model1CFA0, 2 },
    { &_gActor403200Model1D26C, 1 },
    { &_gActor403200Model1D3E8, 0 },
    { &_gActor403200Model1D524, 0 },
    { &_gActor403200Model1D5241D630, 1 },
    { &_gActor403200Model1D754, 2 },
};

/// The package's spawn entry: the descriptor the boss's task is created from.
///
/// The room's resource tables name the descriptor as a task table of one
/// entry, which the boss's placement spawns; nothing in the package reads it.
/// The task it describes carries the host's model and starts in the enemy
/// task's dispatcher.
///
/// The room refers to it as a task descriptor, so that is what the object is.
TaskDesc D_actor_403200_8015F8D0 = { { { TASK_BODY_TMD, 96 } }, _actor403200HostTask, { .model = &_gActor403200Model10824 } };

/// Four zero bytes between the descriptor and the next object. Nothing refers
/// to them; what they were is not established.
static u8 _actor403200Unreferenced8015F8DC[4] = { 0, 0, 0, 0 };

// Retain seven zero bytes after the accessed state byte.
// Their original role as spare storage or alignment remains unresolved.
s8 D_actor_403200_8015F8E0[8] = { 0 };

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

/// Borrowed host task reference for screen-shake requests while the boss lives.
///
/// NULL until successful spawn; teardown does not clear this reference.
static Task* _gGluttonHostTask = NULL;

ActorCommand D_actor_403200_8015F8F4 = { { .loc = { 0, 0 } }, 0 };

SVECTOR gGluttonSpinnerTarget = { 0, 0, 0, 0 };

GluttonButtonPressHoldStorage gGluttonGrabQuery = { { { 0 }, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0 } };

GfxCoord D_actor_403200_8015F920 = { 0, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL };

GfxCoord D_actor_403200_8015F970 = { 0, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL };

_Actor403200PlayerPlacementStorage D_actor_403200_8015F9C0;

GameActorButtonPressHold D_actor_403200_8015FA00;

/// Integer part of the last step `_actorContactApplyGridPushback` applied.
extern SVECTOR ActorContact_ScratchPosition;

extern _Actor403200ViewFunc D_actor_403200_8015E6E8[];

/// Scratch-stack block the host's tick works in for one frame.
///
/// The tick reserves one complete block once the scene lets the actors act and
/// releases it before returning; nothing in it outlasts the tick. Only `view`
/// is ever accessed.
typedef struct {
    byte unknown_0[0x18]; // Reserved with the block and never accessed; role unproven
    s16  view;            // Camera view the host's current view function picks this frame; it becomes the room's live view when it differs from the one showing
    byte unknown_1A[0x2]; // Reserved with the block and never accessed; role unproven
} _Actor403200TickScratch;
STATIC_ASSERT_SIZEOF(_Actor403200TickScratch, 0x1C);

/// The host's state handlers stored as a value for whole-table copies.
///
/// `GluttonWork::state` is the index. The host's tick copies the table and
/// calls the current state's handler with the host's task once a frame; there
/// is no terminator or bounds check. The table has 25 slots, of which the
/// states the host enters, 0 to 0x12, are filled; the rest are NULL and must
/// not be selected.
typedef struct {
    TaskFunc funcs[25]; // Handlers in state order
} _Actor403200StateTable;
STATIC_ASSERT_SIZEOF(_Actor403200StateTable, 0x64);

static void _actor403200Tick(Enemy* enemy, Task* task);

static void _actor403200ShownState(Task* task);

static void _actor403200NoopState16(Task* task);

static void _actor403200NoopState17(Task* task);

static void _actor403200DeathHandoffState(Task* task);

static void _actor403200AdvanceState(Task* host);
static void _actor403200Spawn(Enemy* enemy, Task* task);
static void _actor403200HitGroups3To5(Task* hostTask);
static void _actor403200RainLaunchState(Task* hostTask);
static void _actor403200SpawnSpinnerFormation(Task* hostTask);
static void _actor403200InhaleState(Task* hostTask);
static void _actor403200HoldPlayerState(Task* hostTask);
static void _actor403200SwipeState(Task* hostTask);
static void _actor403200GlobLaunchState(Task* hostTask);
static void _actor403200DebrisState(Task* hostTask);
static void _actor403200ScriptedPoseState(Task* hostTask);
static void _actor403200CollapseState(Task* hostTask);
static void _actor403200SummonState(Task* hostTask);

/// Spawns escort 0's critical-hit flash at an 800-unit local forward offset.
///
/// Borrows live work, escort 0's part 1 and writable hit scratch through the
/// synchronous effect spawn. Requires initialized scratch for the nested call.
/// The burst snapshots placement and does not follow the retained source pointers.
static __inline__ void _actor403200SpawnCriticalFlash(GluttonWork* work, GluttonHitScratch* scratch)
{
    scratch->offset.vy = 0;
    scratch->offset.vx = 0;
    scratch->offset.vz = 0x320;
    effectSpawn(EFFECT_CRITICAL_HIT, &work->escorts[0]->task->extra.tmd->coords[1], 0, &scratch->offset);
}

/// Allocates missing primitive-buffer halves for the host and its live escorts.
///
/// Requires live host work and TMD models on every non-NULL escort. All seven
/// escort slots are visited. Each model owns its auxiliary-heap buffer.
/// Allocation failures are ignored; existing buffers are retained.
static __inline__ void _actor403200AllocateModelBuffers(Task* host)
{
    GluttonWork* work;
    s16          escortIndex;

    work = host->work;
    tmdAllocPrimitiveBuffer(host->extra.tmd);
    for (escortIndex = 0; escortIndex < ARRAY_SIZE(work->escorts); escortIndex++) {
        if (work->escorts[escortIndex] != NULL) {
            tmdAllocPrimitiveBuffer(work->escorts[escortIndex]->task->extra.tmd);
        }
    }
}

/// Releases primitive-buffer halves for the host and its live escorts.
///
/// Requires live host work and TMD models on every non-NULL escort. All seven
/// escort slots are visited. Each model owns its auxiliary-heap buffer.
/// GPU consumption of these buffers must have finished before release.
static __inline__ void _actor403200FreeModelBuffers(Task* host)
{
    GluttonWork* work;
    s16          escortIndex;

    work = host->work;
    tmdFreePrimitiveBuffer(host->extra.tmd);
    for (escortIndex = 0; escortIndex < ARRAY_SIZE(work->escorts); escortIndex++) {
        if (work->escorts[escortIndex] != NULL) {
            tmdFreePrimitiveBuffer(work->escorts[escortIndex]->task->extra.tmd);
        }
    }
}

/// Copies the host model's flags to live escorts from escortIndex onward.
///
/// Invoke as a standalone statement in a braced block or switch case.
/// hostTask and hostWork must be stable, borrowed Task*
/// and GluttonWork* pointers with live models on every non-NULL escort. Both
/// pointers are evaluated repeatedly. escortIndex must be a signed-halfword
/// local initialized in 0..7; it is advanced to seven. No pointer is retained.
#define ACTOR_403200_COPY_ESCORT_MODEL_FLAGS(hostTask, hostWork, escortIndex)                              \
    {                                                                                                      \
        for (; (escortIndex) < ARRAY_SIZE((hostWork)->escorts); (escortIndex)++) {                         \
            if ((hostWork)->escorts[(escortIndex)] != NULL) {                                              \
                (hostWork)->escorts[(escortIndex)]->task->extra.tmd->flags = (hostTask)->extra.tmd->flags; \
            }                                                                                              \
        }                                                                                                  \
    }

/// Advances the boss root 25 coordinate units along its normalized local Z axis.
///
/// Requires a live coordinate and initialized scratch with one SVECTOR (8 bytes).
/// The displacement narrows to signed halfwords and
/// includes Y; GTE quantization may change its length. Marks composition dirty
/// and releases scratch. The caller decides whether actors are frozen.
static __inline__ void _actor403200AdvanceRoot(GfxCoord* coord)
{
    SVECTOR* displacement;

    displacement = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);

    gfxReadMatrixZAxis(&coord->coord, displacement);
    _actorMovementBuildDisplacement(displacement, 25);

    coord->coord.t[0]  += displacement->vx;
    coord->coord.t[1]  += displacement->vy;
    coord->coord.t[2]  += displacement->vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;

    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

#include "../../shared/glutton_inlines.inc.c"

/// Restores the boss root to its current yaw at unit scale and selects hidden state.
///
/// Discards pitch, roll and previous scale, preserving translation and parent.
/// Clears model flags and dirties composition. Requires live host work/model
/// and initialized scratch with ActorScaleRotScratch plus axis-rotation space.
static __inline__ void _actor403200ResetHostPose(Task* task, GluttonWork* work)
{
    _actorRenderRescaleYaw(task->extra.tmd->coords, ONE);
    work->state            = ACTOR_403200_STATE_HIDDEN;
    task->extra.tmd->flags = 0;
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

/// Selects the glob-launch camera by phase and player distance from the host.
///
/// Selector 3 uses views 33/32 in phase 0, 9/10 in phase 1, and 27 in phase 2.
/// The current mapped view provides hysteresis at the cut thresholds.
/// Requires live host/player roots in their common parent frame. XYZ offsets
/// narrow to signed halfwords before their squared length is evaluated in s32;
/// the squared sum must be representable. The result is a room view index. Other
/// phases return view 1.
static s32 _actor403200PickGlobView(Task* host, s16 phase)
{
    enum {
        ACTOR_403200_GLOB_PHASE0_NEAR_VIEW = 33,
        ACTOR_403200_GLOB_PHASE0_FAR_VIEW  = 32,
        ACTOR_403200_GLOB_PHASE1_NEAR_VIEW = 9,
        ACTOR_403200_GLOB_PHASE1_FAR_VIEW  = 10,
        ACTOR_403200_GLOB_PHASE2_VIEW      = 27,
    };
    SVECTOR toPlayer;
    s32     playerDistance;
    s32     nextView;
    s32     currentView;
    s32     selection;

    currentView = viewGetMappedIndex() & ACTOR_403200_VIEW_INDEX_MASK;
    ACTOR_403200_MEASURE_PLAYER_DISTANCE(host, toPlayer, playerDistance);
    // The selection temporary carries a view test, then a range predicate.
    switch (phase) {
        case 0:
            if (currentView == ACTOR_403200_GLOB_PHASE0_NEAR_VIEW) {
                nextView  = ACTOR_403200_GLOB_PHASE0_FAR_VIEW;
                selection = playerDistance < 0x189D;
                if (selection) {
                    nextView = ACTOR_403200_GLOB_PHASE0_NEAR_VIEW;
                }
                return nextView;
            }
            nextView  = ACTOR_403200_GLOB_PHASE0_NEAR_VIEW;
            selection = playerDistance < 0x1770;
            if (!selection) {
                nextView = ACTOR_403200_GLOB_PHASE0_FAR_VIEW;
            }
            return nextView;
        case 1:
            if ((currentView != ACTOR_403200_GLOB_PHASE1_NEAR_VIEW) && (currentView != ACTOR_403200_GLOB_PHASE1_FAR_VIEW)) {
                nextView  = ACTOR_403200_GLOB_PHASE1_NEAR_VIEW;
                selection = playerDistance < 0x27D8;
            } else {
                selection = currentView;
                if (selection == ACTOR_403200_GLOB_PHASE1_NEAR_VIEW) {
                    nextView  = ACTOR_403200_GLOB_PHASE1_FAR_VIEW;
                    selection = playerDistance < 0x27D9;
                    if (selection) {
                        nextView = ACTOR_403200_GLOB_PHASE1_NEAR_VIEW;
                    }
                    return nextView;
                }
                if (selection == ACTOR_403200_GLOB_PHASE1_FAR_VIEW) {
                    nextView  = ACTOR_403200_GLOB_PHASE1_NEAR_VIEW;
                    selection = playerDistance < 0x24EA;
                } else {
                    return ACTOR_403200_VIEW_FALLBACK;
                }
            }
            if (!selection) {
                nextView = ACTOR_403200_GLOB_PHASE1_FAR_VIEW;
            }
            return nextView;
        case 2:
            return ACTOR_403200_GLOB_PHASE2_VIEW;
    }
    return ACTOR_403200_VIEW_FALLBACK;
}

/// Selects the debris camera by phase and player distance from the host.
///
/// Selector 5 uses views 7/8 in phases 0 and 1, and 26 in phase 2.
/// The current mapped view provides hysteresis at the cut thresholds.
/// Requires live host/player roots in their common parent frame. XYZ offsets
/// narrow to signed halfwords before their squared length is evaluated in s32;
/// the squared sum must be representable. The result is a room view index. Other
/// phases return view 1.
static s32 _actor403200PickDebrisView(Task* host, s16 phase)
{
    enum {
        ACTOR_403200_DEBRIS_NEAR_VIEW   = 7,
        ACTOR_403200_DEBRIS_FAR_VIEW    = 8,
        ACTOR_403200_DEBRIS_PHASE2_VIEW = 26,
    };
    SVECTOR toPlayer;
    s32     playerDistance;
    s32     nextView;
    s32     currentView;
    s32     selection;

    currentView = viewGetMappedIndex() & ACTOR_403200_VIEW_INDEX_MASK;
    ACTOR_403200_MEASURE_PLAYER_DISTANCE(host, toPlayer, playerDistance);
    // The selection temporary carries a view test, then a range predicate.
    switch (phase) {
        case 0:
        case 1:
            if ((currentView != ACTOR_403200_DEBRIS_NEAR_VIEW) && (currentView != ACTOR_403200_DEBRIS_FAR_VIEW)) {
                nextView  = ACTOR_403200_DEBRIS_NEAR_VIEW;
                selection = playerDistance < 0x26AC;
            } else {
                selection = currentView;
                if (selection == ACTOR_403200_DEBRIS_NEAR_VIEW) {
                    nextView  = ACTOR_403200_DEBRIS_FAR_VIEW;
                    selection = playerDistance < 0x26AD;
                    if (selection) {
                        nextView = ACTOR_403200_DEBRIS_NEAR_VIEW;
                    }
                    return nextView;
                }
                if (selection == ACTOR_403200_DEBRIS_FAR_VIEW) {
                    nextView  = ACTOR_403200_DEBRIS_NEAR_VIEW;
                    selection = playerDistance < 0x2328;
                } else {
                    return ACTOR_403200_VIEW_FALLBACK;
                }
            }
            if (!selection) {
                nextView = ACTOR_403200_DEBRIS_FAR_VIEW;
            }
            return nextView;
        case 2:
            return ACTOR_403200_DEBRIS_PHASE2_VIEW;
    }
    return ACTOR_403200_VIEW_FALLBACK;
}

/// Selects the combat camera by phase, range and the current view.
///
/// Selector 2 uses views 3/2/4 in phase 0, 34/4 in phase 1 and 37/25 in
/// phase 2. Phase 3 uses player X to choose 37/30; other phases return view 1.
/// The current mapped view provides hysteresis at the cut thresholds.
/// Requires live host/player roots in their common parent frame. XYZ offsets
/// narrow to signed halfwords before their squared length is evaluated in s32;
/// the squared sum must be representable. The result is a room view index.
static s32 _actor403200PickCombatView(Task* host, s16 phase)
{
    SVECTOR toPlayer;
    Task*   player;
    s32     playerDistance;
    s32     currentView;
    s32     viewChoice;

    currentView = viewGetMappedIndex() & ACTOR_403200_VIEW_INDEX_MASK;
    player      = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    ACTOR_403200_MEASURE_PLAYER_DISTANCE(host, toPlayer, playerDistance);
    switch (phase) {
        case 0:
            if ((currentView != ACTOR_403200_VIEW_PHASE0_MIDDLE) && (currentView != ACTOR_403200_VIEW_PHASE0_NEAR) && (currentView != ACTOR_403200_VIEW_HOST_FAR)) {
                if (playerDistance < 0x2261) {
                    return ACTOR_403200_VIEW_PHASE0_NEAR;
                }
                if (playerDistance < 0x2FA8) {
                    return ACTOR_403200_VIEW_PHASE0_MIDDLE;
                }
                return ACTOR_403200_VIEW_HOST_FAR;
            }
            viewChoice = currentView;
            if (viewChoice == ACTOR_403200_VIEW_PHASE0_NEAR) {
                if (playerDistance > 0x2261) {
                    return ACTOR_403200_VIEW_PHASE0_MIDDLE;
                }
                return ACTOR_403200_VIEW_PHASE0_NEAR;
            }
            if (viewChoice == ACTOR_403200_VIEW_PHASE0_MIDDLE) {
                if (playerDistance < 0x1E14) {
                    return ACTOR_403200_VIEW_PHASE0_NEAR;
                }
                if (playerDistance > 0x2FA8) {
                    return ACTOR_403200_VIEW_HOST_FAR;
                }
                return ACTOR_403200_VIEW_PHASE0_MIDDLE;
            }
            if (viewChoice == ACTOR_403200_VIEW_HOST_FAR) {
                if (playerDistance < 0x2E18) {
                    return ACTOR_403200_VIEW_PHASE0_MIDDLE;
                }
                return ACTOR_403200_VIEW_HOST_FAR;
            }
            return ACTOR_403200_VIEW_FALLBACK;
        case 1:
            viewChoice = currentView;
            if ((viewChoice != ACTOR_403200_VIEW_PHASE1_NEAR) && (viewChoice != ACTOR_403200_VIEW_HOST_FAR)) {
                if (playerDistance < 0x2455) {
                    return ACTOR_403200_VIEW_PHASE1_NEAR;
                }
                return ACTOR_403200_VIEW_HOST_FAR;
            }
            if (viewChoice == ACTOR_403200_VIEW_PHASE1_NEAR) {
                if (playerDistance > 0x2455) {
                    return ACTOR_403200_VIEW_HOST_FAR;
                }
                return ACTOR_403200_VIEW_PHASE1_NEAR;
            }
            if (viewChoice == ACTOR_403200_VIEW_HOST_FAR) {
                if (playerDistance < 0x2260) {
                    return ACTOR_403200_VIEW_PHASE1_NEAR;
                }
                return ACTOR_403200_VIEW_HOST_FAR;
            }
            return ACTOR_403200_VIEW_FALLBACK;
        case 2:
            viewChoice = currentView;
            if ((viewChoice != ACTOR_403200_VIEW_LATE_NEAR) && (viewChoice != ACTOR_403200_VIEW_PHASE2_FAR)) {
                if (playerDistance < 0x1E5A) {
                    return ACTOR_403200_VIEW_LATE_NEAR;
                }
                return ACTOR_403200_VIEW_PHASE2_FAR;
            }
            if (viewChoice == ACTOR_403200_VIEW_LATE_NEAR) {
                if (playerDistance > 0x1E5A) {
                    return ACTOR_403200_VIEW_PHASE2_FAR;
                }
                return ACTOR_403200_VIEW_LATE_NEAR;
            }
            if (viewChoice == ACTOR_403200_VIEW_PHASE2_FAR) {
                if (playerDistance < 0x1B58) {
                    return ACTOR_403200_VIEW_LATE_NEAR;
                }
                return ACTOR_403200_VIEW_PHASE2_FAR;
            }
            return ACTOR_403200_VIEW_FALLBACK;
        case 3:
            viewChoice = currentView;
            if ((viewChoice != ACTOR_403200_VIEW_LATE_NEAR) && (viewChoice != ACTOR_403200_VIEW_FAR_X)) {
                if (player->extra.tmd->coords->coord.t[0] < 0x4268) {
                    return ACTOR_403200_VIEW_LATE_NEAR;
                }
                return ACTOR_403200_VIEW_FAR_X;
            }
            if (viewChoice == ACTOR_403200_VIEW_LATE_NEAR) {
                if (player->extra.tmd->coords->coord.t[0] > 0x4650) {
                    return ACTOR_403200_VIEW_FAR_X;
                }
                return ACTOR_403200_VIEW_LATE_NEAR;
            }
            if (viewChoice == ACTOR_403200_VIEW_FAR_X) {
                if (player->extra.tmd->coords->coord.t[0] < 0x4268) {
                    return ACTOR_403200_VIEW_LATE_NEAR;
                }
                return ACTOR_403200_VIEW_FAR_X;
            }
            return ACTOR_403200_VIEW_FALLBACK;
    }
    return ACTOR_403200_VIEW_FALLBACK;
}

/// Selects the rain-launch camera by phase and player distance from the host.
///
/// Selector 1 uses views 5/6 in phase 0, 11/12 in phase 1, and 28 in phase 2.
/// The current mapped view provides hysteresis at the cut thresholds.
/// Requires live host/player roots in their common parent frame. XYZ offsets
/// narrow to signed halfwords before their squared length is evaluated in s32;
/// the squared sum must be representable. The result is a room view index. Other
/// phases return view 1.
static s32 _actor403200PickRainView(Task* host, s16 phase)
{
    enum {
        ACTOR_403200_RAIN_PHASE0_NEAR_VIEW = 5,
        ACTOR_403200_RAIN_PHASE0_FAR_VIEW  = 6,
        ACTOR_403200_RAIN_PHASE1_NEAR_VIEW = 11,
        ACTOR_403200_RAIN_PHASE1_FAR_VIEW  = 12,
        ACTOR_403200_RAIN_PHASE2_VIEW      = 28,
    };
    SVECTOR toPlayer;
    s32     playerDistance;
    s32     nextView;
    s32     currentView;
    s32     selection;

    currentView = viewGetMappedIndex() & ACTOR_403200_VIEW_INDEX_MASK;
    ACTOR_403200_MEASURE_PLAYER_DISTANCE(host, toPlayer, playerDistance);
    // The selection temporary carries a view test, then a range predicate.
    switch (phase) {
        case 0:
            if ((currentView != ACTOR_403200_RAIN_PHASE0_NEAR_VIEW) && (currentView != ACTOR_403200_RAIN_PHASE0_FAR_VIEW)) {
                nextView  = ACTOR_403200_RAIN_PHASE0_NEAR_VIEW;
                selection = playerDistance < 0x238C;
            } else {
                selection = currentView;
                if (selection == ACTOR_403200_RAIN_PHASE0_NEAR_VIEW) {
                    nextView  = ACTOR_403200_RAIN_PHASE0_FAR_VIEW;
                    selection = playerDistance < 0x238D;
                    if (selection) {
                        nextView = ACTOR_403200_RAIN_PHASE0_NEAR_VIEW;
                    }
                    return nextView;
                }
                if (selection == ACTOR_403200_RAIN_PHASE0_FAR_VIEW) {
                    nextView  = ACTOR_403200_RAIN_PHASE0_NEAR_VIEW;
                    selection = playerDistance < 0x2198;
                } else {
                    return ACTOR_403200_VIEW_FALLBACK;
                }
            }
            if (!selection) {
                nextView = ACTOR_403200_RAIN_PHASE0_FAR_VIEW;
            }
            return nextView;
        case 1:
            if ((currentView != ACTOR_403200_RAIN_PHASE1_NEAR_VIEW) && (currentView != ACTOR_403200_RAIN_PHASE1_FAR_VIEW)) {
                nextView  = ACTOR_403200_RAIN_PHASE1_NEAR_VIEW;
                selection = playerDistance < 0x238C;
            } else {
                selection = currentView;
                if (selection == ACTOR_403200_RAIN_PHASE1_NEAR_VIEW) {
                    nextView  = ACTOR_403200_RAIN_PHASE1_FAR_VIEW;
                    selection = playerDistance < 0x238D;
                    if (selection) {
                        nextView = ACTOR_403200_RAIN_PHASE1_NEAR_VIEW;
                    }
                    return nextView;
                }
                if (selection == ACTOR_403200_RAIN_PHASE1_FAR_VIEW) {
                    nextView  = ACTOR_403200_RAIN_PHASE1_NEAR_VIEW;
                    selection = playerDistance < 0x1A90;
                } else {
                    return ACTOR_403200_VIEW_FALLBACK;
                }
            }
            if (!selection) {
                nextView = ACTOR_403200_RAIN_PHASE1_FAR_VIEW;
            }
            return nextView;
        case 2:
            return ACTOR_403200_RAIN_PHASE2_VIEW;
    }
    return ACTOR_403200_VIEW_FALLBACK;
}

/// Selects views 37/25 by player distance from the host.
///
/// Selector 4 uses the same pair in phases 0..2; other phases return view 1.
/// The current mapped view provides hysteresis at the cut thresholds.
/// Requires live host/player roots in their common parent frame. XYZ offsets
/// narrow to signed halfwords before their squared length is evaluated in s32;
/// the squared sum must be representable. The result is a room view index.
static s32 _actor403200PickCloseRangeView(Task* host, s16 phase)
{
    SVECTOR toPlayer;
    s32     playerDistance;
    s32     selection;
    s32     nextView;
    s32     currentView;

    currentView = viewGetMappedIndex() & ACTOR_403200_VIEW_INDEX_MASK;
    ACTOR_403200_MEASURE_PLAYER_DISTANCE(host, toPlayer, playerDistance);
    // The selection temporary carries a view test, then a range predicate.
    switch (phase) {
        default:
            return ACTOR_403200_VIEW_FALLBACK;
        case 0:
        case 1:
        case 2:
            selection = currentView;
            if ((selection != ACTOR_403200_VIEW_LATE_NEAR) && (selection != ACTOR_403200_VIEW_PHASE2_FAR)) {
                nextView  = ACTOR_403200_VIEW_LATE_NEAR;
                selection = playerDistance < 0x1E5A;
                if (!selection) {
                    nextView = ACTOR_403200_VIEW_PHASE2_FAR;
                }
                return nextView;
            } else if (selection == ACTOR_403200_VIEW_LATE_NEAR) {
                nextView  = ACTOR_403200_VIEW_LATE_NEAR;
                selection = playerDistance < 0x1E5A;
                if (!selection) {
                    nextView = ACTOR_403200_VIEW_PHASE2_FAR;
                }
                return nextView;
            } else {
                nextView  = ACTOR_403200_VIEW_LATE_NEAR;
                selection = playerDistance < 0x1B58;
                if (!selection) {
                    nextView = ACTOR_403200_VIEW_PHASE2_FAR;
                }
                return nextView;
            }
    }
}

#undef ACTOR_403200_MEASURE_PLAYER_DISTANCE

/// Reference positions the view selector below measures the player against.
static const _Actor403200ViewAnchors D_actor_403200_80131E64 = {
    {
        { 0x10B4, 1, -0x17DD, 0 },
        { 0x1CD0, 1, -0x17DD, 0 },
        { 0x2888, 1, -0x17DD, 0 },
        { 0x2888, 1, -0x17DD, 0 },
    },
};

/// State handlers of the escort model task `_gluttonPropTask` and
/// `_gluttonEscort6Task` dispatch: texture setup, coordinate refresh,
/// teardown.
static const EnemyTaskFuncTable3 gGluttonPropStates = {
    {
        _gluttonPropSetup,
        _gluttonPropTick,
        enemyDestroy,
    },
};

/// Selects the advancing host's camera from a fixed anchor for each phase.
///
/// Selector 8 uses the combat selector's view pairs and hysteresis. Requires
/// phase 0..3: the four-entry anchor array is indexed before the switch and has
/// no bounds check. The host argument is unused. Player and anchors share room
/// coordinates; XYZ offsets narrow to signed halfwords, then their squared
/// sum must fit s32. Phase 3 selects by player X. Returns a room view index.
static s32 _actor403200PickAdvanceView(Task* unusedHost, s16 phase)
{
    SVECTOR                 toAnchor;
    _Actor403200ViewAnchors anchors;
    Task*                   player;
    s32                     playerDistance;
    s32                     currentView;
    s32                     viewChoice;

    currentView     = viewGetMappedIndex() & ACTOR_403200_VIEW_INDEX_MASK;
    player          = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    anchors         = D_actor_403200_80131E64;
    toAnchor.vx     = player->extra.tmd->coords->coord.t[0] - anchors.points[phase].vx;
    playerDistance  = toAnchor.vx * toAnchor.vx;
    toAnchor.vy     = player->extra.tmd->coords->coord.t[1] - anchors.points[phase].vy;
    playerDistance += toAnchor.vy * toAnchor.vy;
    toAnchor.vz     = player->extra.tmd->coords->coord.t[2] - anchors.points[phase].vz;
    playerDistance  = SquareRoot0(playerDistance + (toAnchor.vz * toAnchor.vz));
    switch (phase) {
        case 0:
            if ((currentView != ACTOR_403200_VIEW_PHASE0_MIDDLE) && (currentView != ACTOR_403200_VIEW_PHASE0_NEAR) && (currentView != ACTOR_403200_VIEW_HOST_FAR)) {
                if (playerDistance < 0x2261) {
                    return ACTOR_403200_VIEW_PHASE0_NEAR;
                }
                if (playerDistance < 0x2FA8) {
                    return ACTOR_403200_VIEW_PHASE0_MIDDLE;
                }
                return ACTOR_403200_VIEW_HOST_FAR;
            }
            viewChoice = currentView;
            if (viewChoice == ACTOR_403200_VIEW_PHASE0_NEAR) {
                if (playerDistance > 0x2261) {
                    return ACTOR_403200_VIEW_PHASE0_MIDDLE;
                }
                return ACTOR_403200_VIEW_PHASE0_NEAR;
            }
            if (viewChoice == ACTOR_403200_VIEW_PHASE0_MIDDLE) {
                if (playerDistance < 0x1E14) {
                    return ACTOR_403200_VIEW_PHASE0_NEAR;
                }
                if (playerDistance > 0x2FA8) {
                    return ACTOR_403200_VIEW_HOST_FAR;
                }
                return ACTOR_403200_VIEW_PHASE0_MIDDLE;
            }
            if (viewChoice == ACTOR_403200_VIEW_HOST_FAR) {
                if (playerDistance < 0x2E18) {
                    return ACTOR_403200_VIEW_PHASE0_MIDDLE;
                }
                return ACTOR_403200_VIEW_HOST_FAR;
            }
            return ACTOR_403200_VIEW_FALLBACK;
        case 1:
            viewChoice = currentView;
            if ((viewChoice != ACTOR_403200_VIEW_PHASE1_NEAR) && (viewChoice != ACTOR_403200_VIEW_HOST_FAR)) {
                if (playerDistance < 0x2455) {
                    return ACTOR_403200_VIEW_PHASE1_NEAR;
                }
                return ACTOR_403200_VIEW_HOST_FAR;
            }
            if (viewChoice == ACTOR_403200_VIEW_PHASE1_NEAR) {
                if (playerDistance > 0x2455) {
                    return ACTOR_403200_VIEW_HOST_FAR;
                }
                return ACTOR_403200_VIEW_PHASE1_NEAR;
            }
            if (viewChoice == ACTOR_403200_VIEW_HOST_FAR) {
                if (playerDistance < 0x2260) {
                    return ACTOR_403200_VIEW_PHASE1_NEAR;
                }
                return ACTOR_403200_VIEW_HOST_FAR;
            }
            return ACTOR_403200_VIEW_FALLBACK;
        case 2:
            viewChoice = currentView;
            if ((viewChoice != ACTOR_403200_VIEW_LATE_NEAR) && (viewChoice != ACTOR_403200_VIEW_PHASE2_FAR)) {
                if (playerDistance < 0x1E5A) {
                    return ACTOR_403200_VIEW_LATE_NEAR;
                }
                return ACTOR_403200_VIEW_PHASE2_FAR;
            }
            if (viewChoice == ACTOR_403200_VIEW_LATE_NEAR) {
                if (playerDistance > 0x1E5A) {
                    return ACTOR_403200_VIEW_PHASE2_FAR;
                }
                return ACTOR_403200_VIEW_LATE_NEAR;
            }
            if (viewChoice == ACTOR_403200_VIEW_PHASE2_FAR) {
                if (playerDistance < 0x1B58) {
                    return ACTOR_403200_VIEW_LATE_NEAR;
                }
                return ACTOR_403200_VIEW_PHASE2_FAR;
            }
            return ACTOR_403200_VIEW_FALLBACK;
        case 3:
            viewChoice = currentView;
            if ((viewChoice != ACTOR_403200_VIEW_LATE_NEAR) && (viewChoice != ACTOR_403200_VIEW_FAR_X)) {
                if (player->extra.tmd->coords->coord.t[0] < 0x4268) {
                    return ACTOR_403200_VIEW_LATE_NEAR;
                }
                return ACTOR_403200_VIEW_FAR_X;
            }
            if (viewChoice == ACTOR_403200_VIEW_LATE_NEAR) {
                if (player->extra.tmd->coords->coord.t[0] > 0x4650) {
                    return ACTOR_403200_VIEW_FAR_X;
                }
                return ACTOR_403200_VIEW_LATE_NEAR;
            }
            if (viewChoice == ACTOR_403200_VIEW_FAR_X) {
                if (player->extra.tmd->coords->coord.t[0] < 0x4268) {
                    return ACTOR_403200_VIEW_LATE_NEAR;
                }
                return ACTOR_403200_VIEW_FAR_X;
            }
            return ACTOR_403200_VIEW_FALLBACK;
    }
    return ACTOR_403200_VIEW_FALLBACK;
}

/// Plays one advancing step's rumble, camera shake and positioned sound.
///
/// Borrows initialized host work, live coordinates and the enemy's sound-instance
/// key. The sound uses half the host's audio depth; no pointer is retained.
static __inline__ void _actor403200PlayAdvanceFootstep(Task* host, GluttonWork* work, Enemy* enemy)
{
    s32 soundId;
    s32 soundPan;

    work->shakeLevel = GLUTTON_SHAKE_LONG;
    padScriptSpawn(D_actor_403200_80141C5C, D_actor_403200_80141C64);
    soundId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 1);
    soundPan = (s8)worldCoordGetOriginAudioPan(host->extra.tmd->coords);
    sndEvtRequestScriptStart(soundId, soundPan,
                             (s8)(worldCoordGetOriginAudioDepth(host->extra.tmd->coords) / 2));
}

/// Advances the host into the next fight phase while playing its walking clip.
///
/// State 9 starts clip 2 and moves 25 coordinate units per unfrozen tick along
/// the normalized local Z axis. Phases 0 and 1 end at root X >= 7370 and 10370,
/// advance the phase and select inhale state. After the first tick, selector 8
/// uses fixed phase anchors while the host moves. Requires initialized work,
/// models, animation rigs and scratch; its 12 reserved bytes are untouched.
static void _actor403200AdvanceState(Task* host)
{
    enum {
        ACTOR_403200_ADVANCE_CLIP                = 2,
        ACTOR_403200_ADVANCE_FIRST_FOOTSTEP_CUE  = 18,
        ACTOR_403200_ADVANCE_SECOND_FOOTSTEP_CUE = 24,
        ACTOR_403200_ADVANCE_VIEW_SELECTOR       = 8,
        ACTOR_403200_ADVANCE_SCRATCH_BYTES       = 12,
    };
    GluttonWork* work;
    Enemy*       enemy;
    GfxCoord*    hostRoot;
    s16          cueIndex;

    work  = host->work;
    enemy = host->spawnArg2.pointer;

    if (work->stateChanged != 0) {
        work->neckPitchEnabled   = 1;
        work->neckYawEnabled     = 1;
        work->hostExposed        = 0;
        work->animId             = ACTOR_403200_ADVANCE_CLIP;
        work->animStep           = GLUTTON_ANIM_STEP_BLEND;
        work->neckPitchTarget    = 0;
        work->wallDistanceTarget = 0xE74;
    }

    SCRATCH_STACK_RESERVE_BYTES(ACTOR_403200_ADVANCE_SCRATCH_BYTES);
    _gluttonTickAnim(host);

    // Fire each walking cue once on arrival, then retain the current cue.
    cueIndex = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    if (cueIndex == ACTOR_403200_ADVANCE_FIRST_FOOTSTEP_CUE && work->clip.prevSlot2Cue != cueIndex) {
        _actor403200PlayAdvanceFootstep(host, work, enemy);
    }

    cueIndex = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    if (cueIndex == ACTOR_403200_ADVANCE_SECOND_FOOTSTEP_CUE && work->clip.prevSlot2Cue != cueIndex) {
        _actor403200PlayAdvanceFootstep(host, work, enemy);
    }

    work->clip.prevSlot2Cue = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;

    hostRoot = host->extra.tmd->coords;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
        _actor403200AdvanceRoot(hostRoot);
    }
    host->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;

    switch (work->phase) {
        case 0:
            if (host->extra.tmd->coords->coord.t[0] >= 0x1CCA) {
                work->phase++;
                work->state = GLUTTON_STATE_INHALE;
            }
            break;
        case 1:
            if (host->extra.tmd->coords->coord.t[0] >= 0x2882) {
                work->phase++;
                work->state = GLUTTON_STATE_INHALE;
            }
            break;
    }

    if (work->stateTicks > 0) {
        work->viewSelector = ACTOR_403200_ADVANCE_VIEW_SELECTOR;
    }

    SCRATCH_STACK_RELEASE_BYTES(ACTOR_403200_ADVANCE_SCRATCH_BYTES);
}

#include "../../shared/glutton_throw_spawn.inc.c"

#include "../../shared/glutton_throw_fly.inc.c"

/// State handlers of the enemy stood up on the host's first escort: spawn,
/// flight, teardown.
static const EnemyTaskFuncTable3 gGluttonThrowStates = {
    {
        _gluttonThrowSpawn,
        _gluttonThrowFly,
        enemyDestroy,
    },
};

#include "../../shared/glutton_glob_spawn.inc.c"

#include "../../shared/glutton_glob_fall.inc.c"

#include "../../shared/glutton_glob_engulf.inc.c"

#include "../../shared/glutton_glob_hold.inc.c"

/// State handlers of the grab enemy, by state: entry, bounce, rise, hold and
/// teardown.
static const EnemyTaskFuncTable5 gGluttonGlobStates = {
    {
        _gluttonGlobSpawn,
        _gluttonGlobFall,
        _gluttonGlobEngulf,
        _gluttonGlobHold,
        enemyDestroy,
    },
};

#include "../../shared/glutton_chunk_spawn.inc.c"

#include "../../shared/glutton_chunk_fall.inc.c"

#include "../../shared/glutton_chunk_settle.inc.c"

/// State handlers of the enemy dropped from the host's part 3: spawn, fall,
/// settle, teardown.
static const EnemyTaskFuncTable4 gGluttonChunkStates = {
    {
        _gluttonChunkSpawn,
        _gluttonChunkFall,
        _gluttonChunkSettle,
        enemyDestroy,
    },
};

#include "../../shared/glutton_rain_spawn.inc.c"

#include "../../shared/glutton_rain_rise.inc.c"

#include "../../shared/glutton_rain_fall.inc.c"

#include "../../shared/glutton_rain_splat.inc.c"

/// State handlers of the enemy that rises out of view and slams back down:
/// spawn, rise, descent, landing, teardown.
static const EnemyTaskFuncTable5 gGluttonRainStates = {
    {
        _gluttonRainSpawn,
        _gluttonRainRise,
        _gluttonRainFall,
        _gluttonRainSplat,
        enemyDestroy,
    },
};

#include "../../shared/glutton_spinner_spawn.inc.c"

#include "../../shared/glutton_spinner_chase.inc.c"

/// State handlers of the spinner enemy: spawn, wait, home, teardown.
static const EnemyTaskFuncTable4 gGluttonSpinnerStates = {
    {
        _gluttonSpinnerSpawn,
        _gluttonSpinnerWait,
        _gluttonSpinnerChase,
        enemyDestroy,
    },
};

#include "../../shared/glutton_shake_tick.inc.c"

/// Applies the boss's model visibility, state-reset and buffer-allocation mode.
///
/// Requires live host work/model and models on every non-NULL escort. Mode 0
/// allocates then hides and selects hidden state; 1 shows then allocates; 2 hides
/// and selects hidden state; 3 hides while retaining state. All handled modes
/// cancel the buffer-free countdown and copy host flags to live escorts.
/// Unknown modes do nothing. Returns 0; the other message arguments are ignored.
static s32 _actor403200SetModelDraw(Task* task, s32 unusedMessageId, s32 drawMode, s32 unusedSecondArg)
{
    enum {
        ACTOR_403200_MODEL_DRAW_HIDE_RESET_ALLOCATE = 0,
        ACTOR_403200_MODEL_DRAW_SHOW_ALLOCATE       = 1,
        ACTOR_403200_MODEL_DRAW_HIDE_RESET          = 2,
        ACTOR_403200_MODEL_DRAW_HIDE_KEEP_STATE     = 3,
    };
    GluttonWork* work;
    GluttonWork* flagWork;
    s16          flagIndex;

    work = task->work;
    switch (drawMode) {
        case ACTOR_403200_MODEL_DRAW_HIDE_RESET_ALLOCATE:
            _actor403200AllocateModelBuffers(task);
            flagWork                = task->work;
            flagWork->freeCountdown = 0;
            task->extra.tmd->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            flagIndex               = 0;
            ACTOR_403200_COPY_ESCORT_MODEL_FLAGS(task, flagWork, flagIndex);
            work->state = ACTOR_403200_STATE_HIDDEN;
            break;
        case ACTOR_403200_MODEL_DRAW_SHOW_ALLOCATE:
            flagWork                = task->work;
            flagWork->freeCountdown = 0;
            task->extra.tmd->flags  = 0;
            flagIndex               = 0;
            ACTOR_403200_COPY_ESCORT_MODEL_FLAGS(task, flagWork, flagIndex);
            _actor403200AllocateModelBuffers(task);
            break;
        case ACTOR_403200_MODEL_DRAW_HIDE_RESET:
            work->freeCountdown    = 0;
            flagWork               = work;
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            flagIndex              = 0;
            ACTOR_403200_COPY_ESCORT_MODEL_FLAGS(task, flagWork, flagIndex);
            work->state = ACTOR_403200_STATE_HIDDEN;
            break;
        case ACTOR_403200_MODEL_DRAW_HIDE_KEEP_STATE:
            flagIndex               = 0;
            flagWork                = task->work;
            flagWork->freeCountdown = 0;
            task->extra.tmd->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            ACTOR_403200_COPY_ESCORT_MODEL_FLAGS(task, flagWork, flagIndex);
            break;
    }
    return 0;
}

/// Applies a borrowed stage/area command to the boss and records its low command byte.
///
/// Requires initialized host work, enemy, rigs, models and scratch. Commands in
/// the dumping-hole namespace hide, select scripted poses, resume combat or
/// collapse; the incinerator namespace hides or prepares a glob attack.
/// The complete signed command is dispatched; only lastCommand narrows to u8.
/// Context bytes are copied even for an unsupported command. Returns 1 and
/// retains no payload pointer. The ID and second message argument are ignored.
static s32 _actor403200ApplyCommand(Task* task, s32 unusedMessageId, const ActorCommand* command, s32 unusedSecondArg)
{
    enum {
        ACTOR_403200_DUMPING_HOLE_CONTEXT              = GAME_STAGE_MINE_SHELTER | (GAME_AREA_SHELTER_B3_DUMPING_HOLE << 8),
        ACTOR_403200_INCINERATOR_CONTEXT               = GAME_STAGE_MINE_SHELTER | (GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR << 8),
        ACTOR_403200_COMMAND_HIDE                      = 0,
        ACTOR_403200_COMMAND_PLAY_DEBRIS_POSE          = 2,
        ACTOR_403200_COMMAND_RESTORE_POSE              = 3,
        ACTOR_403200_COMMAND_RESUME_BATTLE             = 5,
        ACTOR_403200_COMMAND_COLLAPSE                  = 10,
        ACTOR_403200_COMMAND_COLLAPSE_SKIP_SHORT       = 11,
        ACTOR_403200_COMMAND_COLLAPSE_RESET_VIEW       = 12,
        ACTOR_403200_COMMAND_COLLAPSE_SKIP_LONG        = 19,
        ACTOR_403200_INCINERATOR_COMMAND_HIDE          = 0,
        ACTOR_403200_INCINERATOR_COMMAND_PREPARE_GLOBS = 1,
        ACTOR_403200_COLLAPSE_SKIP_SHORT_TICKS         = 150,
        ACTOR_403200_COLLAPSE_SKIP_LONG_TICKS          = 600,
        ACTOR_403200_ANIM_RATE_FAST_FORWARD            = 127,
    };
    GluttonWork* work;
    GluttonWork* flagWork;
    Enemy*       enemy;
    s16          flagIndex;
    s32          soundId;
    s32          soundPan;
    s32          commandId;

    work  = task->work;
    enemy = task->spawnArg2.pointer;

    work->lastCommandStage = command->context.loc.stage;
    work->lastCommandArea  = command->context.loc.area;
    work->lastCommand      = (u8)command->command;

    if (command->context.key == ACTOR_403200_DUMPING_HOLE_CONTEXT) {
        commandId = command->command;
        switch (commandId) {
            case ACTOR_403200_COMMAND_HIDE:
                work->state              = ACTOR_403200_STATE_HIDDEN;
                work->wallDistanceTarget = 0xFA0;
                break;

            case ACTOR_403200_COMMAND_PLAY_DEBRIS_POSE:
                work->state    = ACTOR_403200_STATE_SCRIPTED_POSE;
                work->animId   = ACTOR_403200_CLIP_DEBRIS_POSE;
                work->animStep = GLUTTON_ANIM_STEP_RESTART;
                soundId        = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 2);
                soundPan       = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
                sndEvtRequestScriptStart(soundId, soundPan,
                                         (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
                break;

            case ACTOR_403200_COMMAND_RESTORE_POSE:
                work->state     = ACTOR_403200_STATE_SCRIPTED_POSE;
                work->prevState = GLUTTON_STATE_REENTER;
                work->animId    = ACTOR_403200_CLIP_RETRACT_LIMB;
                work->animStep  = GLUTTON_ANIM_STEP_BLEND;
                break;

            case ACTOR_403200_COMMAND_RESUME_BATTLE:
                work->state             = ACTOR_403200_STATE_CHOOSE_ATTACK;
                flagWork                = task->work;
                flagWork->freeCountdown = 0;
                task->extra.tmd->flags  = 0;
                flagIndex               = 0;
                ACTOR_403200_COPY_ESCORT_MODEL_FLAGS(task, flagWork, flagIndex);
                _actor403200AllocateModelBuffers(task);
                work->viewSelector = 0;
                work->viewLocked   = 0;
                break;

            case ACTOR_403200_COMMAND_COLLAPSE:
                work->state        = ACTOR_403200_STATE_COLLAPSE;
                work->prevState    = GLUTTON_STATE_REENTER;
                work->animId       = ACTOR_403200_CLIP_COLLAPSE;
                work->animStep     = GLUTTON_ANIM_STEP_RESTART;
                work->viewSelector = 7;
                work->collapseSkip = 0;
                work->viewLocked   = 1;
                break;

            case ACTOR_403200_COMMAND_COLLAPSE_SKIP_SHORT:
                work->state        = ACTOR_403200_STATE_COLLAPSE;
                work->prevState    = GLUTTON_STATE_REENTER;
                work->animId       = ACTOR_403200_CLIP_COLLAPSE;
                work->collapseSkip = ACTOR_403200_COLLAPSE_SKIP_SHORT_TICKS;
                work->animStep     = GLUTTON_ANIM_STEP_RESTART;
                work->viewLocked   = 1;
                break;

            case ACTOR_403200_COMMAND_COLLAPSE_RESET_VIEW:
                work->viewSelector = 7;
                work->viewLocked   = 0;
                work->state        = ACTOR_403200_STATE_SHOWN;
                // The binary falls through into the long collapse skip.

            case ACTOR_403200_COMMAND_COLLAPSE_SKIP_LONG:
                work->state        = ACTOR_403200_STATE_COLLAPSE;
                work->viewSelector = 4;
                work->prevState    = GLUTTON_STATE_REENTER;
                work->animId       = ACTOR_403200_CLIP_COLLAPSE;
                work->collapseSkip = ACTOR_403200_COLLAPSE_SKIP_LONG_TICKS;
                work->animStep     = GLUTTON_ANIM_STEP_RESTART;
                work->viewLocked   = 0;
                gGluttonLimbReach  = 0x640;
                break;
        }
    }

    if (command->context.key == ACTOR_403200_INCINERATOR_CONTEXT) {
        switch (command->command) {
            case ACTOR_403200_INCINERATOR_COMMAND_HIDE:
                work->state = ACTOR_403200_STATE_HIDDEN;
                break;

            case ACTOR_403200_INCINERATOR_COMMAND_PREPARE_GLOBS:
                work->animId   = ACTOR_403200_CLIP_GLOB_LAUNCH;
                work->animStep = GLUTTON_ANIM_STEP_RESTART;
                work->animRate = ACTOR_403200_ANIM_RATE_FAST_FORWARD;
                _gluttonTickAnim(task);
                while (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
                    _gluttonTickAnim(task);
                }
                work->animRate                        = ANIMATION_RATE_ONE;
                task->extra.tmd->coords->coord.t[0]   = -0xBB8;
                task->extra.tmd->coords->coord.t[1]   = 0;
                task->extra.tmd->coords->coord.t[2]   = -0x992;
                task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                work->state                           = ACTOR_403200_STATE_GLOBS;
                break;
        }
    }
    return 1;
}

/// Allocates and initializes the dumping-hole Glutton and its seven escort tasks.
///
/// The enemy and task must be the live host pair with a model covering parts 0..4.
/// Owns zeroed `GluttonWork`, animation rigs and collision bodies until task exit;
/// escort models borrow its lighting matrices. Work allocation failure destroys
/// the host. Escorts 0 and 1 must spawn successfully; later combat also requires
/// escort 3. Other optional spawn failures retain NULL slots. Scratch must support
/// the nested coordinate, collision and animation calls.
static void _actor403200Spawn(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_403200_SPAWN_WALK_CLIP = 2,
    };
    GluttonWork*           work;
    GluttonWork*           lightingWork;
    TmdObject*             hostModel;
    GfxCoord*              rootCoord;
    GfxCoord*              swipeCoord;
    Enemy*                 escort;
    Task*                  escortTask;
    WorldCollisionContact* swipeContacts;
    SVECTOR                forwardOffset;
    SVECTOR*               forwardPointer;
    VECTOR                 lightingPosition;

    s16 escortIndex;

    hostModel = task->extra.tmd;
    rootCoord = hostModel->coords;

    // Install ownership before linking collision bodies or spawning child enemies.
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
    enemy->hp            = D_actor_403200_80141C00.hpMax;
    enemy->param         = &D_actor_403200_80141C00;
    enemy->recs          = work->hits[0].contacts;

    animationInitContext(&work->hostRig.anim, D_actor_403200_8015E484, hostModel, work->hostRig.poses, work->hostRig.slots);
    animationInitContext(&work->hostBlendRig.anim, D_actor_403200_8015E484, hostModel, work->hostBlendRig.poses, work->hostBlendRig.slots);

    work->animStep        = GLUTTON_ANIM_STEP_RESTART;
    work->animId          = ACTOR_403200_SPAWN_WALK_CLIP;
    work->limbPoseEnabled = 1;
    work->blending        = 0;
    work->neckYawTarget = work->neckYaw = 0;
    work->animRate = work->field_7B8 = ANIMATION_RATE_ONE;

    worldCollisionBindEnemySphere(&task->extra.tmd->coords[4], &work->hits[1].body, work->hits[1].contacts, ARRAY_SIZE(work->hits[1].contacts), 0x20, 0x300);
    worldCollisionBindEnemySphere(&task->extra.tmd->coords[4], &work->hits[0].body, work->hits[0].contacts, ARRAY_SIZE(work->hits[0].contacts), 0x20, 0x300);
    worldCollisionBindEnemySphere(&task->extra.tmd->coords[1], &work->hits[2].body, work->hits[2].contacts, ARRAY_SIZE(work->hits[2].contacts), 0x20, 0xBB8);

    work->hits[1].body.pos.vz = -0x100;
    work->hits[2].body.pos.vy = 0x400;
    work->hits[1].body.pos.vx = 0;
    work->hits[1].body.pos.vy = 0;
    work->hits[2].body.pos.vx = 0;
    work->hits[2].body.pos.vz = -0x400;

    gfxReadMatrixZAxis(&task->extra.tmd->coords->coord, &forwardOffset);
    forwardOffset.vy = 0;
    forwardPointer   = &forwardOffset;
    VectorNormalSS(forwardPointer, forwardPointer);
    gte_lddp(0x1388);
    gte_ldsv(forwardPointer);
    gte_gpf12();
    gte_stsv(forwardPointer);

    work->playerAnim.source.sets          = NULL;
    work->playerAnim.animationId          = ACTOR_403200_PLAYER_CLIP_INHALE_CAUGHT;
    work->playerAnim.blend                = ANIMATION_BLEND_RESET;
    work->playerAnim.blendFrames          = 3;
    work->playerAnim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
    work->deathTicks                      = 0;
    task->msgTable                        = D_actor_403200_8015F770;
    rootCoord->parent                     = &gGfxViewCoord;
    rootCoord->composeStamp               = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);

    work->prevState = GLUTTON_STATE_REENTER;
    _actor403200AllocateModelBuffers(task);

    _actor403200ResetHostPose(task, work);

    // The first two escorts supply the paired animation rigs and hit groups.
    escort                                                = enemySpawnFromTable(D_actor_403200_8015E72C, 0, 0, task->spawnArg2.pointer);
    work->escorts[0]                                      = escort;
    escort->task->extra.tmd->coords->parent               = task->extra.tmd->coords;
    work->escorts[0]->task->extra.tmd->coords->coord.t[0] = 0;
    work->escorts[0]->task->extra.tmd->coords->coord.t[1] = 0;
    work->escorts[0]->task->extra.tmd->coords->coord.t[2] = 0;
    work->escorts[0]->task->extra.tmd->flags              = 0;
    animationInitContext(&work->escort0Rig.anim, D_actor_403200_8015E53C, work->escorts[0]->task->extra.tmd, work->escort0Rig.poses,
                         work->escort0Rig.slots);
    animationInitContext(&work->escort0BlendRig.anim, D_actor_403200_8015E53C, work->escorts[0]->task->extra.tmd, work->escort0BlendRig.poses,
                         work->escort0BlendRig.slots);
    work->escorts[0]->field_4    = &task->extra.tmd->coords->coord;
    work->escorts[0]->field_48   = 0;
    work->escorts[0]->bodyPos.vx = 0xC8;
    work->escorts[0]->bodyPos.vy = 0;
    work->escorts[0]->bodyPos.vz = 0x3E8;
    work->escorts[0]->coord      = &work->escorts[0]->task->extra.tmd->coords[1];
    worldTargetLinkNode(&work->escorts[0]->node);
    work->escorts[0]->reactionFlags = 0;
    work->escorts[0]->hp            = D_actor_403200_80141C00.hpMax;
    work->groups3To5Pool            = D_actor_403200_80141C20.hpMax;
    work->escorts[0]->param         = &D_actor_403200_80141C20;
    work->escorts[0]->recs          = work->hits[3].contacts;
    worldCollisionBindEnemySphere(&work->escorts[0]->task->extra.tmd->coords[1], &work->hits[3].body, work->hits[3].contacts, ARRAY_SIZE(work->hits[3].contacts),
                                  0x20, 0x300);
    worldCollisionBindEnemySphere(&work->escorts[0]->task->extra.tmd->coords[2], &work->hits[4].body, work->hits[4].contacts, ARRAY_SIZE(work->hits[4].contacts),
                                  0x20, 0x300);
    worldCollisionBindEnemySphere(&work->escorts[0]->task->extra.tmd->coords[3], &work->hits[5].body, work->hits[5].contacts, ARRAY_SIZE(work->hits[5].contacts),
                                  0x20, 0x300);

    escort                                                = enemySpawnFromTable(D_actor_403200_8015E72C, 1, 0, task->spawnArg2.pointer);
    work->escorts[1]                                      = escort;
    escort->task->extra.tmd->coords->parent               = task->extra.tmd->coords;
    work->escorts[1]->task->extra.tmd->coords->coord.t[0] = 0;
    work->escorts[1]->task->extra.tmd->coords->coord.t[1] = 0;
    work->escorts[1]->task->extra.tmd->coords->coord.t[2] = 0;
    work->escorts[1]->task->extra.tmd->flags              = 0;
    animationInitContext(&work->escort1Rig.anim, D_actor_403200_8015E5F4, work->escorts[1]->task->extra.tmd, work->escort1Rig.poses,
                         work->escort1Rig.slots);
    animationInitContext(&work->escort1BlendRig.anim, D_actor_403200_8015E5F4, work->escorts[1]->task->extra.tmd, work->escort1BlendRig.poses,
                         work->escort1BlendRig.slots);
    work->escorts[1]->field_4    = &task->extra.tmd->coords->coord;
    work->escorts[1]->field_48   = 0;
    work->escorts[1]->bodyPos.vx = -0xC8;
    work->escorts[1]->bodyPos.vy = 0;
    work->escorts[1]->bodyPos.vz = 0x3E8;
    work->escorts[1]->coord      = &work->escorts[1]->task->extra.tmd->coords[1];
    worldTargetLinkNode(&work->escorts[1]->node);
    work->escorts[1]->reactionFlags = 0;
    work->escorts[1]->hp            = D_actor_403200_80141C00.hpMax;
    work->groups6To8Pool            = D_actor_403200_80141C30.hpMax;
    work->escorts[1]->param         = &D_actor_403200_80141C30;
    work->escorts[1]->recs          = work->hits[6].contacts;
    worldCollisionBindEnemySphere(&work->escorts[1]->task->extra.tmd->coords[1], &work->hits[6].body, work->hits[6].contacts, ARRAY_SIZE(work->hits[6].contacts),
                                  0x20, 0x300);
    worldCollisionBindEnemySphere(&work->escorts[1]->task->extra.tmd->coords[2], &work->hits[7].body, work->hits[7].contacts, ARRAY_SIZE(work->hits[7].contacts),
                                  0x20, 0x300);
    worldCollisionBindEnemySphere(&work->escorts[1]->task->extra.tmd->coords[3], &work->hits[8].body, work->hits[8].contacts, ARRAY_SIZE(work->hits[8].contacts),
                                  0x20, 0x300);

    escort           = enemySpawnFromTable(D_actor_403200_8015E72C, 2, 0, task->spawnArg2.pointer);
    work->escorts[2] = escort;
    if (escort != NULL) {
        escort->task->extra.tmd->coords->parent               = &task->extra.tmd->coords[4];
        work->escorts[2]->task->extra.tmd->coords->coord.t[0] = 0;
        work->escorts[2]->task->extra.tmd->coords->coord.t[1] = 0x59;
        work->escorts[2]->task->extra.tmd->coords->coord.t[2] = -0x64;
        work->escorts[2]->task->extra.tmd->flags              = 0;
    }

    escort           = enemySpawnFromTable(D_actor_403200_8015E72C, 3, 0, task->spawnArg2.pointer);
    work->escorts[3] = escort;
    if (escort != NULL) {
        escort->task->extra.tmd->coords->parent               = &task->extra.tmd->coords[3];
        work->escorts[3]->task->extra.tmd->coords->coord.t[0] = 0;
        work->escorts[3]->task->extra.tmd->coords->coord.t[1] = 0;
        work->escorts[3]->task->extra.tmd->coords->coord.t[2] = 0;
        work->escorts[3]->task->extra.tmd->flags              = 0;
        work->escorts[3]->field_4                             = &task->extra.tmd->coords->coord;
        work->escorts[3]->field_48                            = 0;
        work->escorts[3]->bodyPos.vx                          = 0;
        work->escorts[3]->bodyPos.vy                          = 0x1F4;
        work->escorts[3]->bodyPos.vz                          = 0x384;
        work->escorts[3]->coord                               = work->escorts[3]->task->extra.tmd->coords;
        worldTargetLinkNode(&work->escorts[3]->node);
        work->escorts[3]->reactionFlags = 0;
        work->escorts[3]->hp            = D_actor_403200_80141C00.hpMax;
        work->groups1To2Pool            = D_actor_403200_80141C40.hpMax;
        work->escorts[3]->param         = &D_actor_403200_80141C40;
        work->escorts[3]->recs          = work->hits[1].contacts;
    }

    escort           = enemySpawnFromTable(D_actor_403200_8015E72C, 4, 0, task->spawnArg2.pointer);
    work->escorts[4] = escort;
    if (escort != NULL) {
        escort->task->extra.tmd->coords->parent               = &task->extra.tmd->coords[4];
        work->escorts[4]->task->extra.tmd->coords->coord.t[0] = 0;
        work->escorts[4]->task->extra.tmd->coords->coord.t[1] = 0;
        work->escorts[4]->task->extra.tmd->coords->coord.t[2] = 0x14;
        work->escorts[4]->task->extra.tmd->flags              = 0;
    }

    escort           = enemySpawnFromTable(D_actor_403200_8015E72C, 5, 0, task->spawnArg2.pointer);
    work->escorts[5] = escort;
    if (escort != NULL) {
        escort->task->extra.tmd->coords->parent               = &task->extra.tmd->coords[2];
        work->escorts[5]->task->extra.tmd->coords->coord.t[0] = 0;
        work->escorts[5]->task->extra.tmd->coords->coord.t[1] = 0x67C;
        work->escorts[5]->task->extra.tmd->coords->coord.t[2] = 0xC8;
        work->escorts[5]->task->extra.tmd->flags              = 0;
    }

    escort           = enemySpawnFromTable(D_actor_403200_8015E72C, 6, 0, task->spawnArg2.pointer);
    work->escorts[6] = escort;
    if (escort != NULL) {
        escort->task->extra.tmd->coords->parent               = &task->extra.tmd->coords[1];
        work->escorts[6]->task->extra.tmd->coords->coord.t[0] = 0;
        work->escorts[6]->task->extra.tmd->coords->coord.t[1] = 0x62C;
        work->escorts[6]->task->extra.tmd->coords->coord.t[2] = 0x5DC;
        work->escorts[6]->task->extra.tmd->flags              = 0;
    }

    work->groups6To8Pool  = 0x3C;
    swipeCoord            = &work->swipeCoord;
    work->escorts[6]      = NULL;
    work->viewLocked      = 0;
    work->viewSelector    = ACTOR_403200_VIEW_SELECTOR_DEFAULT;
    work->phase           = 0;
    work->groups3To5Pool  = 0x32;
    work->spinnersSpawned = 0;

    work->swipeCoord.parent = task->extra.tmd->coords;
    gfxSetRotIdentity(&work->swipeCoord.coord);
    work->swipeCoord.coord.t[0] = work->swipeCoord.coord.t[1] = work->swipeCoord.coord.t[2] = 0;
    work->swipeCoord.composeStamp                                                           = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(swipeCoord);

    // The limb strike uses a separate capsule under the host root.
    work->swipeCapsule.ends[1].vz   = 0x1B58;
    swipeContacts                   = work->swipeContacts;
    work->swipeCapsule.end0Radius   = 0x258;
    work->swipeCapsule.end1Radius   = 0x258;
    work->swipeCapsule.ends[0].vx   = 0;
    work->swipeCapsule.ends[0].vy   = 0;
    work->swipeCapsule.ends[0].vz   = 0;
    work->swipeCapsule.ends[1].vx   = 0;
    work->swipeCapsule.ends[1].vy   = 0;
    work->swipeCapsule.contacts     = swipeContacts;
    work->swipeBody.coord           = swipeCoord;
    work->swipeBody.context.capsule = &work->swipeCapsule;
    work->swipeBody.pos.vx          = 0;
    work->swipeBody.pos.vy          = -0xFA;
    work->swipeBody.pos.vz          = 0x25F;
    work->swipeBody.key             = WORLD_COLLISION_CONTACT_ENEMY_BODY | 0x20;
    work->swipeBody.radius          = 0;
    work->swipeBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->swipeBody);
    worldCollisionInitContacts(swipeContacts, ARRAY_SIZE(work->swipeContacts), 0);
    work->swipeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    lightingWork              = task->work;
    task->extra.tmd->lightMtx = &lightingWork->lightMtx;
    task->extra.tmd->colorMtx = &lightingWork->colorMtx;
    for (escortIndex = 0; escortIndex < ARRAY_SIZE(lightingWork->escorts); escortIndex++) {
        escort = lightingWork->escorts[escortIndex];
        if (escort != NULL) {
            escortTask                      = escort->task;
            escortTask->extra.tmd->lightMtx = &lightingWork->lightMtx;
            escortTask->extra.tmd->colorMtx = &lightingWork->colorMtx;
        }
    }

    actorRenderComposeCoord(rootCoord);
    lightingPosition.vx = rootCoord->workm.t[0];
    lightingPosition.vy = rootCoord->workm.t[1];
    lightingPosition.vz = rootCoord->workm.t[2];
    worldCoordUpdateActorColor(enemy, &lightingPosition, 0, 0);
    _gluttonTickAnim(task);

    D_actor_403200_8015F8F4.context.loc.stage = ACTOR_403200_BROADCAST_CONTEXT_STAGE;
    D_actor_403200_8015F8F4.context.loc.area  = ACTOR_403200_BROADCAST_CONTEXT_AREA;
    D_actor_403200_8015F8F4.command           = ACTOR_403200_SUMMON_COMMAND_NONE;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_actor_403200_8015F8F4, ACTOR_COMMAND_MESSAGE_APPLY);

    work->wallDistance = work->wallDistanceTarget = 0x9C4;
    work->wallDrop                                = 0x190;
    _gGluttonHostTask                             = task;
    work->summonsSpawned = work->summonsAlive = 0;
    task->state                              += 1;
}

#include "../../shared/glutton_hit_group0.inc.c"

#include "../../shared/glutton_hit_groups1to2.inc.c"

/// Applies the first attack contacting escort 0's three hit spheres.
///
/// Requires live host work/enemy, escorts 0, 1 and 3, initialized contacts and
/// scratch space for `GluttonHitScratch` and nested effect calls. Groups are tried
/// in order 3, 4, 5. Damage uses player range in coordinate units, is divided by
/// six with a minimum of one for a nonzero hit, and is mirrored to the three
/// target escorts. Eligible critical hits quadruple damage and request summoning;
/// exhausting the group pool also requests summoning and refills it to 50 HP.
static void _actor403200HitGroups3To5(Task* hostTask)
{
    enum {
        ACTOR_403200_GROUPS3_TO5_POOL_HP = 50,
    };
    GluttonHitScratch* scratch;
    GluttonWork*       work;
    Enemy*             enemy;
    PlayerStatus*      playerStatus;

    s32    dxSquared;
    s32    dySquared;
    s32    dzSquared;
    u32    scaledDamage;
    s16    contactYaw;
    s16    bossState;
    s16    hitCooldown;
    u16    sharedHp;
    Enemy* escort3;
    Enemy* escort0;
    Enemy* escort1;

    playerStatus = &gPlayerStatus;
    enemy        = hostTask->spawnArg2.pointer;
    work         = hostTask->work;
    scratch      = SCRATCH_STACK_RESERVE_BLOCK(GluttonHitScratch);
    // Only the first group with an attack and surviving effect key consumes this tick.
    if ((_gluttonScanGroup(scratch, &work->hits[3]) != 0 && _gluttonSpawnGroupHitEffect(scratch, &work->hits[3])) ||
        (_gluttonScanGroup(scratch, &work->hits[4]) != 0 && _gluttonSpawnGroupHitEffect(scratch, &work->hits[4])) ||
        (_gluttonScanGroup(scratch, &work->hits[5]) != 0 && _gluttonSpawnGroupHitEffect(scratch, &work->hits[5]))) {
        hitCooldown              = damageGetPlayerAttackHitCooldown(scratch->attackKey);
        work->groups6To8Cooldown = hitCooldown;
        work->groups3To5Cooldown = hitCooldown;
        work->group0Cooldown     = hitCooldown;
        work->groups1To2Cooldown = hitCooldown;
        damageGetPlayerAttackReaction(scratch->attackKey);

        scratch->toPlayer.vx    = (playerStatus->coordMtx->t[0] - hostTask->extra.tmd->coords->coord.t[0]) + 0x51F;
        dxSquared               = scratch->toPlayer.vx * scratch->toPlayer.vx;
        scratch->toPlayer.vy    = (playerStatus->coordMtx->t[1] - hostTask->extra.tmd->coords->coord.t[1]) - 0xFA;
        dySquared               = scratch->toPlayer.vy * scratch->toPlayer.vy;
        scratch->toPlayer.vz    = (playerStatus->coordMtx->t[2] - hostTask->extra.tmd->coords->coord.t[2]) + 0x25F;
        dzSquared               = scratch->toPlayer.vz * scratch->toPlayer.vz;
        scratch->playerDistance = SquareRoot0(dxSquared + dySquared + dzSquared);
        scratch->damage         = damageComputePlayerAttack(scratch->attackKey, scratch->playerDistance, 0, 0);

        if (damageRollCriticalHit(work->escorts[0], scratch->attackKey, 0) != 0 && (bossState = work->state, bossState != ACTOR_403200_STATE_HOLD_PLAYER) && bossState != GLUTTON_STATE_INHALE &&
            bossState != GLUTTON_STATE_ADVANCE && bossState != GLUTTON_STATE_SUMMON && bossState != GLUTTON_STATE_HEAL && bossState != GLUTTON_STATE_RETRACT_LIMB && bossState != GLUTTON_STATE_SWIPE && work->playerCaught != 1 &&
            gSceneCombatState.battleRefs == 1) {
            _actor403200SpawnCriticalFlash(work, scratch);
            scratch->damage *= 4;
            work->state      = GLUTTON_STATE_SUMMON;
        }

        // Share one truncated HP result across the host and target escorts.
        scaledDamage = scratch->damage / 6;
        if (scaledDamage == 0) {
            if (scratch->damage == 0) {
                scratch->damage = 0;
            } else {
                scratch->damage = 1;
            }
        } else {
            scratch->damage = scaledDamage;
        }
        damageAccumulateLifeDrainHp(enemy, scratch->attackKey, scratch->damage, 0);
        enemy->hp            -= scratch->damage;
        escort3               = work->escorts[3];
        sharedHp              = enemy->hp;
        escort0               = work->escorts[0];
        escort1               = work->escorts[1];
        escort3->hp           = sharedHp;
        escort1->hp           = sharedHp;
        escort0->hp           = sharedHp;
        work->groups3To5Pool -= scratch->damage;
        if (work->groups3To5Pool <= 0 && (bossState = work->state, bossState != ACTOR_403200_STATE_HOLD_PLAYER) && bossState != GLUTTON_STATE_INHALE && bossState != GLUTTON_STATE_ADVANCE && bossState != GLUTTON_STATE_SUMMON &&
            bossState != GLUTTON_STATE_HEAL && bossState != GLUTTON_STATE_RETRACT_LIMB && bossState != GLUTTON_STATE_SWIPE && work->playerCaught != 1 && gSceneCombatState.battleRefs == 1) {
            _actor403200SpawnCriticalFlash(work, scratch);
            work->state          = GLUTTON_STATE_SUMMON;
            work->groups3To5Pool = ACTOR_403200_GROUPS3_TO5_POOL_HP;
        }

        worldTargetAddReadoutAmount(&work->escorts[0]->node, scratch->damage, 0);
        work->escorts[0]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(work->escorts[0]->task->extra.tmd->coords);
        scratch->offset.vx = scratch->contactPoint.vx - work->escorts[0]->task->extra.tmd->coords->workm.t[0];
        scratch->offset.vy = scratch->contactPoint.vy - work->escorts[0]->task->extra.tmd->coords->workm.t[1];
        scratch->offset.vz = scratch->contactPoint.vz - work->escorts[0]->task->extra.tmd->coords->workm.t[2];
        contactYaw         = ratan2(scratch->offset.vx, scratch->offset.vz) -
                     ratan2(-hostTask->extra.tmd->coords->workm.m[2][0],
                            hostTask->extra.tmd->coords->workm.m[2][2]);
        scratch->contactYaw = contactYaw;
        scratch->contactYaw = _actorAngleNormalizeYaw(contactYaw);

        work->neckYaw       = 0;
        work->neckYawTarget = 0;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GluttonHitScratch);
}

#include "../../shared/glutton_hit_groups6to8.inc.c"

/// Hides the host and live escorts, releasing their model buffers two ticks after entry.
///
/// Requires live host work and models. Entry resets the state timer; this state
/// does not advance animation. The dispatcher supplies stateChanged/stateTicks.
static void _actor403200HiddenState(Task* task)
{
    enum {
        ACTOR_403200_HIDE_FREE_DELAY_TICKS = 2,
    };
    GluttonWork* work;
    GluttonWork* flagWork;
    TmdObject*   hostModel;
    s32          drawFlags;
    s32          delayedDrawFlags;
    s16          flagIndex;

    work      = task->work;
    hostModel = task->extra.tmd;
    if (work->stateChanged != 0) {
        hostModel->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        flagWork                = task->work;
        flagIndex               = 0;
        flagWork->freeCountdown = 0;
        task->extra.tmd->flags  = (drawFlags = TMD_OBJECT_SKIP_ACTIVE_DRAW);
        ACTOR_403200_COPY_ESCORT_MODEL_FLAGS(task, flagWork, flagIndex);
        work->stateTicks = 0;
        return;
    }
    if (work->stateTicks == ACTOR_403200_HIDE_FREE_DELAY_TICKS) {
        hostModel->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        flagWork                = task->work;
        delayedDrawFlags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        flagIndex               = 0;
        flagWork->freeCountdown = 0;
        task->extra.tmd->flags  = (drawFlags = delayedDrawFlags);
        ACTOR_403200_COPY_ESCORT_MODEL_FLAGS(task, flagWork, flagIndex);
        _actor403200FreeModelBuffers(task);
    }
}

/// Launches eight rain blobs on fixed ticks while tracking the player's bearing.
///
/// State 2 restarts clip 3, shows the host and live escorts, and launches blob
/// indices 0..7 at ticks 19, 26, 28, 35, 50, 74, 78 and 82. Animation completion
/// returns to attack selection. Requires live work, enemy, rigs and models,
/// successful projectile spawns and initialized scratch. Neck yaw uses 4096 units
/// per turn; offsets narrow to signed halfwords in the roots' common parent frame.
static void _actor403200RainLaunchState(Task* hostTask)
{
    enum {
        ACTOR_403200_RAIN_FIRST_LAUNCH_TICK = 19,
        ACTOR_403200_RAIN_VIEW_START_TICK   = 21,
    };
    GluttonWork*                   work;
    GluttonWork*                   flagWork;
    _Actor403200RainLaunchScratch* scratch;
    s16                            flagIndex;

    s16 launchTick;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor403200RainLaunchScratch);
    work    = hostTask->work;
    if (work->stateChanged != 0) {
        work->lastAttack           = ACTOR_403200_STATE_RAIN;
        work->animId               = ACTOR_403200_CLIP_RAIN_INHALE;
        work->animStep             = GLUTTON_ANIM_STEP_RESTART;
        flagWork                   = hostTask->work;
        flagWork->freeCountdown    = 0;
        hostTask->extra.tmd->flags = 0;
        flagIndex                  = 0;
        ACTOR_403200_COPY_ESCORT_MODEL_FLAGS(hostTask, flagWork, flagIndex);
        _actor403200AllocateModelBuffers(hostTask);
        work->neckYawEnabled   = 1;
        work->neckPitchEnabled = 1;
        work->hostExposed      = 0;
    }
    launchTick = work->stateTicks - ACTOR_403200_RAIN_FIRST_LAUNCH_TICK;
    switch (launchTick) {
        case 0:
            enemySpawnFromTable(gGluttonEscortTasks, 1, 0, hostTask->spawnArg2.pointer)->workType = ENEMY_WORK_PLAIN;
            break;
        case 7:
            enemySpawnFromTable(gGluttonEscortTasks, 1, 1, hostTask->spawnArg2.pointer)->workType = ENEMY_WORK_PLAIN;
            break;
        case 9:
            enemySpawnFromTable(gGluttonEscortTasks, 1, 2, hostTask->spawnArg2.pointer)->workType = ENEMY_WORK_PLAIN;
            break;
        case 0x10:
            enemySpawnFromTable(gGluttonEscortTasks, 1, 3, hostTask->spawnArg2.pointer)->workType = ENEMY_WORK_PLAIN;
            break;
        case 0x1F:
            enemySpawnFromTable(gGluttonEscortTasks, 1, 4, hostTask->spawnArg2.pointer)->workType = ENEMY_WORK_PLAIN;
            break;
        case 0x37:
            enemySpawnFromTable(gGluttonEscortTasks, 1, 5, hostTask->spawnArg2.pointer)->workType = ENEMY_WORK_PLAIN;
            break;
        case 0x3B:
            enemySpawnFromTable(gGluttonEscortTasks, 1, 6, hostTask->spawnArg2.pointer)->workType = ENEMY_WORK_PLAIN;
            break;
        case 0x3F:
            enemySpawnFromTable(gGluttonEscortTasks, 1, 7, hostTask->spawnArg2.pointer)->workType = ENEMY_WORK_PLAIN;
            break;
    }
    _gluttonTickAnim(hostTask);
    if (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ACTOR_403200_STATE_CHOOSE_ATTACK;
    }
    if (work->stateTicks >= ACTOR_403200_RAIN_VIEW_START_TICK) {
        work->viewSelector = ACTOR_403200_VIEW_SELECTOR_RAIN;
    }
    work->neckYawTarget = _actorAngleTurnToPlayer(hostTask, &scratch->toPlayer, &gPlayerStatus);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403200RainLaunchScratch);
}

/// Places up to nine spinners with the formation's models and chase delays.
///
/// Requires live host work and writable projectile descriptors. An LCG draw picks
/// position row 0..2, folding draw 3 onto row 0; all current rows are identical.
/// Stops at the first failed spawn. The spinner index 0..8 is ORed into the place
/// key's index nibble. Spawned enemy tasks own their models; the host retains only
/// the most recent spawn while positioning it.
static void _actor403200SpawnSpinnerFormation(Task* hostTask)
{
    GluttonWork* work;
    Enemy*       spinner;
    s16          spinnerIndex;
    s16          formationIndex;

    work = hostTask->work;

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    formationIndex  = (gRandomLcgState >> 16) & 3;
    if (formationIndex == 3) {
        formationIndex = 0;
    }

    for (spinnerIndex = 0; spinnerIndex < ARRAY_SIZE(D_actor_403200_8015F888); spinnerIndex++) {
        gGluttonEscortTasks[4].data.model = D_actor_403200_8015F888[spinnerIndex].model;
        spinner                           = enemySpawnFromTable(gGluttonEscortTasks, 4, D_actor_403200_8015F888[spinnerIndex].chaseDelayClass, NULL);
        work->lastSpawned                 = spinner;
        if (spinner == NULL) {
            break;
        }
        spinner->task->extra.tmd->coords->coord.t[0]           = D_actor_403200_8015F7B0[formationIndex][spinnerIndex].vx;
        work->lastSpawned->task->extra.tmd->coords->coord.t[1] = D_actor_403200_8015F7B0[formationIndex][spinnerIndex].vy;
        work->lastSpawned->task->extra.tmd->coords->coord.t[2] = D_actor_403200_8015F7B0[formationIndex][spinnerIndex].vz;
        work->lastSpawned->workType                            = ENEMY_WORK_PLAIN;
        work->lastSpawned->placeKey                           |= spinnerIndex << ENEMY_PLACE_INDEX_SHIFT;
    }
}

/// Pulls the player toward the mouth and releases the waiting spinner formation.
///
/// State 3 uses phase-dependent coordinate-unit pulls and animation cue windows.
/// A living player in catch range who accepts the hold is placed 900 units from
/// host part 4, oriented toward or away from it, and given the corresponding
/// caught clip before selecting player-hold state. Completion withdraws the
/// spinner release, forgets summons and returns to attack selection. Requires
/// live work, host/player models and rigs, synchronous message payload borrowing,
/// and scratch capacity for the drag block and nested calls.
static void _actor403200InhaleState(Task* hostTask)
{
    enum {
        ACTOR_403200_INHALE_BASE_PULL        = 25,
        ACTOR_403200_INHALE_CATCH_RADIUS     = 1200,
        ACTOR_403200_INHALE_PLACEMENT_RADIUS = 900,
        ACTOR_403200_INHALE_VIEW_TICK        = 20,
        ACTOR_403200_INHALE_FIRST_RUMBLE_CUE = 10,
        ACTOR_403200_INHALE_RUMBLE_CUE_COUNT = 9,
        ACTOR_403200_INHALE_FIRST_CATCH_CUE  = 11,
        ACTOR_403200_INHALE_CATCH_CUE_COUNT  = 5,
    };
    GluttonWork*             work;
    GluttonWork*             flagWork;
    Enemy*                   enemy;
    Task*                    playerTask;
    PlayerStatus*            playerStatus;
    _Actor403200DragScratch* scratch;
    s16                      flagIndex;

    work         = hostTask->work;
    enemy        = hostTask->spawnArg2.pointer;
    playerTask   = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    playerStatus = &gPlayerStatus;
    scratch      = SCRATCH_STACK_RESERVE_BLOCK(_Actor403200DragScratch);

    if (work->stateChanged != 0) {
        work->lastAttack           = GLUTTON_STATE_INHALE;
        work->animId               = ACTOR_403200_CLIP_RAIN_INHALE;
        work->animStep             = GLUTTON_ANIM_STEP_RESTART;
        flagWork                   = hostTask->work;
        flagWork->freeCountdown    = 0;
        hostTask->extra.tmd->flags = 0;
        flagIndex                  = 0;
        ACTOR_403200_COPY_ESCORT_MODEL_FLAGS(hostTask, flagWork, flagIndex);
        _actor403200AllocateModelBuffers(hostTask);
        work->neckPitchTarget    = 0;
        work->neckPitchEnabled   = 1;
        work->neckYawEnabled     = 1;
        work->viewLocked         = 0;
        work->hostExposed        = 0;
        gGluttonSpinnerTarget.vz = 0;
        gGluttonSpinnerTarget.vy = 0;
        gGluttonSpinnerTarget.vx = 0;
        _actorRenderTransformLocalPointToWorld(&hostTask->extra.tmd->coords[3], &gGluttonSpinnerTarget);
        D_actor_403200_8015F8F4.context.loc.stage = ACTOR_403200_BROADCAST_CONTEXT_STAGE;
        D_actor_403200_8015F8F4.context.loc.area  = ACTOR_403200_BROADCAST_CONTEXT_AREA;
        D_actor_403200_8015F8F4.command           = ACTOR_403200_SUMMON_COMMAND_PULL;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_actor_403200_8015F8F4, ACTOR_COMMAND_MESSAGE_APPLY);
        {
            s16 releasedFlag         = 1;
            work->wallDistanceTarget = 0xC80;
            gGluttonSpinnersReleased = releasedFlag;
        }
    }

    _gluttonTickAnim(hostTask);

    work->neckYawTarget = _actorAngleTurnToPlayer(hostTask, &scratch->offset, &gPlayerStatus);

    scratch->offset.vz = 0;
    scratch->offset.vy = 0;
    scratch->offset.vx = 0;
    _actorRenderTransformLocalPointToWorld(&hostTask->extra.tmd->coords[4], &scratch->offset);

    // Subtraction deliberately wraps through 16 bits before normalization.
    scratch->offset.vx       = (u16)playerTask->extra.tmd->coords->coord.t[0] - (u16)scratch->offset.vx;
    scratch->offset.vy       = (u16)playerTask->extra.tmd->coords->coord.t[1] - (u16)scratch->offset.vy;
    scratch->offset.vz       = (u16)playerTask->extra.tmd->coords->coord.t[2] - (u16)scratch->offset.vz;
    scratch->playerDistance  = scratch->offset.vx * scratch->offset.vx;
    scratch->playerDistance += scratch->offset.vz * scratch->offset.vz;
    scratch->playerDistance  = SquareRoot0(scratch->playerDistance);
    VectorNormalSS(&scratch->offset, &scratch->offset);

    switch (work->phase) {
        case 0:
            scratch->rumblePeriod = 0x14;
            break;
        case 1:
            scratch->rumblePeriod = 0x10;
            break;
        case 2:
        default:
            scratch->rumblePeriod = 0xC;
            break;
    }
    if (((u32)((work->hostRig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) - ACTOR_403200_INHALE_FIRST_RUMBLE_CUE) < (u32)ACTOR_403200_INHALE_RUMBLE_CUE_COUNT) && ((work->stateTicks % scratch->rumblePeriod) == 0)) {
        padScriptSpawn(D_actor_403200_80141C7C, D_actor_403200_80141C88);
    }

    switch (work->phase) {
        case 0:
            scratch->phasePull = 0;
            break;
        case 1:
            scratch->phasePull = 5;
            break;
        case 2:
        default:
            scratch->phasePull = 0xA;
            break;
    }

    if (work->stateTicks == 0xA) {
        s32 soundId;
        s32 soundPan;

        soundId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x17);
        soundPan = (s8)worldCoordGetOriginAudioPan(hostTask->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(hostTask->extra.tmd->coords));
    }
    if (work->stateTicks == 0x3C) {
        s32 soundId;
        s32 soundPan;

        soundId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0A);
        soundPan = (s8)worldCoordGetOriginAudioPan(hostTask->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(hostTask->extra.tmd->coords));
    }
    if (work->stateTicks == 0xE8) {
        sndEvtRequestScriptStop((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0A), SOUND_SCRIPT_STOP_KEEP_RELEASE);
    }

    /// Scales a borrowed, word-aligned SVECTOR with GTE Q12 multiplication.
    ///
    /// Invoke as a statement with a stable pointer and a signed-halfword-range
    /// factor. The pointer is evaluated by both load and store; the factor once.
    /// A normalized Q12 vector becomes a coordinate-unit displacement.
#define ACTOR_403200_SCALE_PULL_VECTOR(direction, factor) \
    {                                                     \
        gte_lddp(factor);                                 \
        gte_ldsv(direction);                              \
        gte_gpf12();                                      \
        gte_stsv(direction);                              \
    }

    // Scale the normalized player direction to this cue's pull in coordinate units.
    work->hostExposed = 1;
    switch (work->hostRig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) {
        case 9:
            ACTOR_403200_SCALE_PULL_VECTOR(&scratch->offset, -(scratch->phasePull + ACTOR_403200_INHALE_BASE_PULL) / 4);
            work->neckPitchTarget = 0x180;
            break;
        case 10:
            ACTOR_403200_SCALE_PULL_VECTOR(&scratch->offset, -(scratch->phasePull + ACTOR_403200_INHALE_BASE_PULL) / 2);
            break;
        case 11:
        case 13:
        case 15:
            ACTOR_403200_SCALE_PULL_VECTOR(&scratch->offset, -(scratch->phasePull + ACTOR_403200_INHALE_BASE_PULL));
            work->neckPitchTarget = 0x2B2;
            break;
        case 12:
        case 14:
            ACTOR_403200_SCALE_PULL_VECTOR(&scratch->offset, -((scratch->phasePull + ACTOR_403200_INHALE_BASE_PULL) * 3) / 2);
            work->neckPitchTarget = 0x500;
            break;
        case 16:
            ACTOR_403200_SCALE_PULL_VECTOR(&scratch->offset, -(scratch->phasePull + ACTOR_403200_INHALE_BASE_PULL) / 3);
            work->neckPitchTarget = 0x100;
            break;
        case 17:
        case 18:
            ACTOR_403200_SCALE_PULL_VECTOR(&scratch->offset, -(scratch->phasePull + ACTOR_403200_INHALE_BASE_PULL) / 3);
            work->neckPitchTarget = 0x400;
            break;
        case 19:
        case 20:
            scratch->offset.vz = 0;
            scratch->offset.vx = 0;
            ACTOR_403200_SCALE_PULL_VECTOR(&scratch->offset, -(scratch->phasePull + ACTOR_403200_INHALE_BASE_PULL) / 6);
            work->neckPitchTarget = 0;
            break;
        default:
            work->hostExposed  = 0;
            scratch->offset.vz = 0;
            scratch->offset.vx = 0;
            break;
    }

    // The catch borrows static placement and animation payloads synchronously.
    if (((u32)((work->hostRig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) - ACTOR_403200_INHALE_FIRST_CATCH_CUE) < (u32)ACTOR_403200_INHALE_CATCH_CUE_COUNT) && (scratch->playerDistance < ACTOR_403200_INHALE_CATCH_RADIUS) && (enemy->hp > 0) &&
        (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &D_actor_403200_8015FA00, 0) == 0)) {
        SVECTOR* directionPointer;
        s16      playerYaw;

        work->state            = ACTOR_403200_STATE_HOLD_PLAYER;
        work->playerCaught     = 1;
        scratch->pullCentre.vz = 0;
        scratch->pullCentre.vy = 0;
        scratch->pullCentre.vx = 0;
        _actorRenderTransformLocalPointToWorld(&hostTask->extra.tmd->coords[4], &scratch->pullCentre);

        scratch->offset.vx  = (u16)playerTask->extra.tmd->coords->coord.t[0] - (u16)scratch->pullCentre.vx;
        scratch->offset.vy  = 0;
        scratch->offset.vz  = (u16)playerTask->extra.tmd->coords->coord.t[2] - (u16)scratch->pullCentre.vz;
        playerYaw           = _actorAngleTurnToDirection(hostTask->extra.tmd->coords, &scratch->offset);
        directionPointer    = &scratch->offset;
        work->neckYawTarget = playerYaw;
        VectorNormalSS(directionPointer, directionPointer);
        ACTOR_403200_SCALE_PULL_VECTOR(directionPointer, ACTOR_403200_INHALE_PLACEMENT_RADIUS);

        D_actor_403200_8015F9C0.placement.pos.vx = scratch->pullCentre.vx + scratch->offset.vx;
        D_actor_403200_8015F9C0.placement.pos.vy = playerTask->extra.tmd->coords->coord.t[1];
        {
            s32 centreZ = scratch->pullCentre.vz;
            s32 offsetZ = scratch->offset.vz;

            D_actor_403200_8015F9C0.placement.rot.vx = 0;
            D_actor_403200_8015F9C0.placement.rot.vz = 0;
            D_actor_403200_8015F9C0.placement.pos.vz = centreZ + offsetZ;
        }
        {
            u16 centreX = (u16)scratch->pullCentre.vx;
            u16 placedX = (u16)D_actor_403200_8015F9C0.placement.pos.vx;

            scratch->offset.vy = 0;
            scratch->offset.vx = centreX - placedX;
        }
        scratch->offset.vz = (u16)scratch->pullCentre.vz - (u16)D_actor_403200_8015F9C0.placement.pos.vz;
        playerYaw          = _actorAngleTurnToDirection(playerTask->extra.tmd->coords, directionPointer);
        {
            s32 playerTurn = playerYaw;

            scratch->pullCentreYaw = playerTurn;
            if (abs(playerTurn) < ACTOR_TRANSFORM_ANGLE_HALF_TURN / 2) {
                D_actor_403200_8015F9C0.placement.rot.vy = ratan2((s32)scratch->offset.vx, (s32)scratch->offset.vz);
                work->playerAnim.source.sets             = D_actor_403200_8015E6AC;
            } else {
                D_actor_403200_8015F9C0.placement.rot.vy = ratan2((s32)scratch->offset.vx, (s32)scratch->offset.vz) + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
                work->playerAnim.source.sets             = D_actor_403200_8015E6CC;
            }
        }
        if (playerStatus->hp > 0) {
            TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_PLACE, &D_actor_403200_8015F9C0.placement, 0);
        }
        work->playerAnim.animationId = ACTOR_403200_PLAYER_CLIP_INHALE_CAUGHT;
        work->playerAnim.blend       = ANIMATION_BLEND_RESET;
        work->playerAnim.blendFrames = 0;
        work->field_F02              = 1;
        TASK_MESSAGE_DISPATCH_POINTER(playerTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &work->playerAnim, 0);
        work->caughtTicks = 0;
    }

    if (scratch->offset.vx != 0 || scratch->offset.vz != 0) {
        scratch->displacement.vx = scratch->offset.vx;
        scratch->displacement.vy = 0;
        scratch->displacement.vz = scratch->offset.vz;
        playerActorSetPendingDisplacement(&scratch->displacement);
    }

    if (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        D_actor_403200_8015F8F4.context.loc.stage = ACTOR_403200_BROADCAST_CONTEXT_STAGE;
        D_actor_403200_8015F8F4.context.loc.area  = ACTOR_403200_BROADCAST_CONTEXT_AREA;
        D_actor_403200_8015F8F4.command           = ACTOR_403200_SUMMON_COMMAND_VANISH;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_actor_403200_8015F8F4, ACTOR_COMMAND_MESSAGE_APPLY);
        gGluttonSpinnersReleased = 0;
        work->spinnersSpawned    = 0;
        work->state              = ACTOR_403200_STATE_CHOOSE_ATTACK;
        for (scratch->slot = 0; scratch->slot < ARRAY_SIZE(work->summons); scratch->slot++) {
            work->summons[scratch->slot] = NULL;
        }
        work->summonsAlive = 0;
    }
    if (work->stateTicks == ACTOR_403200_INHALE_VIEW_TICK) {
        work->viewSelector = ACTOR_403200_VIEW_SELECTOR_INHALE;
    }

#undef ACTOR_403200_SCALE_PULL_VECTOR

    SCRATCH_STACK_RELEASE_BLOCK(_Actor403200DragScratch);
}

/// Keeps the caught player under the Glutton's bite and holding animations.
///
/// State 13 withdraws the inhale, disables lock-on and enters clip 15, followed
/// by clip 14. It applies attack 3 during the entry clip and relays the player's
/// animation request during the first 24 state ticks. A finished player clip
/// places a surviving player at the host root, preserving the catch's rotation.
/// Requires live host/player work and models, escorts 0, 1 and 3, initialized rigs
/// and scratch; all message payloads are borrowed synchronously.
static void _actor403200HoldPlayerState(Task* hostTask)
{
    enum {
        ACTOR_403200_HOLD_SCRATCH_BYTES         = 60,
        ACTOR_403200_HOLD_PLAYER_REPLAY_TICKS   = 24,
        ACTOR_403200_HOLD_ATTACK_ID             = 3,
        ACTOR_403200_PLAYER_DEATH_SOUND_TICKS   = 30,
        ACTOR_403200_PLAYER_DEATH_FADE_FRAMES   = 54,
        ACTOR_403200_PLAYER_DEATH_RESTART_TICKS = 90,
    };
    GluttonWork*  work;
    GluttonWork*  flagWork;
    Enemy*        enemy;
    Task*         playerTask;
    PlayerStatus* playerStatus;
    SVECTOR       playerOffset;
    SVECTOR*      offsetPointer;
    GfxCoord*     hostRoot;
    s16           flagIndex;

    s16 playerYaw;

    work         = hostTask->work;
    enemy        = hostTask->spawnArg2.pointer;
    playerTask   = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    playerStatus = &gPlayerStatus;
    if (work->stateChanged != 0) {
        D_actor_403200_8015F8F4.context.loc.stage = ACTOR_403200_BROADCAST_CONTEXT_STAGE;
        D_actor_403200_8015F8F4.context.loc.area  = ACTOR_403200_BROADCAST_CONTEXT_AREA;
        D_actor_403200_8015F8F4.command           = ACTOR_403200_SUMMON_COMMAND_VANISH;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_actor_403200_8015F8F4, ACTOR_COMMAND_MESSAGE_APPLY);
        gGluttonSpinnersReleased = 0;
        sndEvtRequestScriptStop((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0A), SOUND_SCRIPT_STOP_KEEP_RELEASE);
        work->animId               = ACTOR_403200_CLIP_HOLD_ENTRY;
        work->animStep             = GLUTTON_ANIM_STEP_RESTART;
        flagWork                   = hostTask->work;
        flagWork->freeCountdown    = 0;
        hostTask->extra.tmd->flags = 0;
        flagIndex                  = 0;
        ACTOR_403200_COPY_ESCORT_MODEL_FLAGS(hostTask, flagWork, flagIndex);
        _actor403200AllocateModelBuffers(hostTask);
        work->neckYawEnabled   = 1;
        work->viewSelector     = ACTOR_403200_VIEW_SELECTOR_HOLD;
        work->neckPitchTarget  = 0;
        work->neckPitchEnabled = 0;
        work->hostExposed      = 0;
        playerOffset.vz        = 0;
        playerOffset.vy        = 0;
        playerOffset.vx        = 0;
        _actorRenderTransformLocalPointToWorld(&hostTask->extra.tmd->coords[4], &playerOffset);
        playerOffset.vx                           = playerTask->extra.tmd->coords[0].coord.t[0] - playerOffset.vx;
        playerOffset.vy                           = 0;
        playerOffset.vz                           = playerTask->extra.tmd->coords[0].coord.t[2] - playerOffset.vz;
        offsetPointer                             = &playerOffset;
        hostRoot                                  = hostTask->extra.tmd->coords;
        playerYaw                                 = _actorAngleTurnToDirection(hostRoot, offsetPointer);
        work->neckYawTarget                       = playerYaw;
        D_actor_403200_8015F8E0[0]                = 0;
        D_actor_403200_8015F8F4.context.loc.stage = ACTOR_403200_BROADCAST_CONTEXT_STAGE;
        D_actor_403200_8015F8F4.context.loc.area  = ACTOR_403200_BROADCAST_CONTEXT_AREA;
        D_actor_403200_8015F8F4.command           = ACTOR_403200_SUMMON_COMMAND_VANISH;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_actor_403200_8015F8F4, ACTOR_COMMAND_MESSAGE_APPLY);
        work->wallDistanceTarget = 0x9C4;
        gGluttonSpinnersReleased = 0;
        Gp_StateC08.flags       |= ATTACHMENT_FLAG_EVENT_LOCK;
        roomEffectRequestCancelAll();
        worldTargetDisableNodeLockOn(&enemy->node);
        worldTargetDisableNodeLockOn(&work->escorts[3]->node);
        worldTargetDisableNodeLockOn(&work->escorts[0]->node);
        worldTargetDisableNodeLockOn(&work->escorts[1]->node);
        return;
    }

    // The recovered hold tick reserves this block without accessing it.
    SCRATCH_STACK_RESERVE_BYTES(ACTOR_403200_HOLD_SCRATCH_BYTES);
    _gluttonTickAnim(hostTask);
    if ((work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) && (work->animId == ACTOR_403200_CLIP_HOLD_ENTRY)) {
        work->animStep = GLUTTON_ANIM_STEP_RESTART;
        work->animId   = ACTOR_403200_CLIP_HOLD_LOOP;
    }
    if (work->animId == ACTOR_403200_CLIP_HOLD_ENTRY) {
        if (playerStatus->hp > 0) {
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, ACTOR_403200_HOLD_ATTACK_ID), 0);
            if (playerStatus->hp <= 0) {
                ((GameActor*)playerTask->work)->state = ACTOR_403200_PLAYER_FATAL_DAMAGE_STATE;
                gGameSession->deathSoundCountdown     = ACTOR_403200_PLAYER_DEATH_SOUND_TICKS;
                gGameSession->deathFadeFrames         = ACTOR_403200_PLAYER_DEATH_FADE_FRAMES;
                gGameSession->deathRestartDelay       = ACTOR_403200_PLAYER_DEATH_RESTART_TICKS;
            }
        }
        if (((work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x19) && (work->prevSlot3Cue != (work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK))) {
            s32 soundId;
            s32 soundPan;
            s32 soundDepth;

            soundId    = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x11);
            soundPan   = (s8)worldCoordGetOriginAudioPan(hostTask->extra.tmd->coords);
            soundDepth = (s8)worldCoordGetOriginAudioDepth(hostTask->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, soundPan, soundDepth);
        }
        work->prevSlot3Cue = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    }
    if (work->animId == ACTOR_403200_CLIP_HOLD_LOOP) {
        if (((work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x1E) && (work->prevSlot3Cue != (work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK))) {
            padScriptSpawnVariableMotorRamp(4, 0xFF, 8);
        }
        if (((work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x23) && (work->prevSlot3Cue != (work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK))) {
            s32 soundId;
            s32 soundPan;

            soundId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x12);
            soundPan = (s8)worldCoordGetOriginAudioPan(hostTask->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, soundPan,
                                     (s8)worldCoordGetOriginAudioDepth(hostTask->extra.tmd->coords));
            padScriptSpawnVariableMotorRamp(4, 0xFF, 8);
        }
        if (((work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x27) && (work->prevSlot3Cue != (work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK))) {
            s32 soundId;
            s32 soundPan;

            soundId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x12);
            soundPan = (s8)worldCoordGetOriginAudioPan(hostTask->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, soundPan,
                                     (s8)worldCoordGetOriginAudioDepth(hostTask->extra.tmd->coords));
            padScriptSpawnVariableMotorRamp(4, 0xFF, 8);
        }
        work->prevSlot3Cue = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    }
    if ((taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) && (playerStatus->hp > 0)) {
        D_actor_403200_8015F9C0.placement.pos.vx = hostTask->extra.tmd->coords[0].coord.t[0];
        D_actor_403200_8015F9C0.placement.pos.vy = hostTask->extra.tmd->coords[0].coord.t[1];
        D_actor_403200_8015F9C0.placement.pos.vz = hostTask->extra.tmd->coords[0].coord.t[2];
        TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_PLACE, &D_actor_403200_8015F9C0.placement, 0);
        D_actor_403200_8015F8E0[0] = 1;
    }
    if (work->stateTicks < ACTOR_403200_HOLD_PLAYER_REPLAY_TICKS) {
        sndEvtRequestScriptStop((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0A), SOUND_SCRIPT_STOP_KEEP_RELEASE);
        TASK_MESSAGE_DISPATCH_POINTER(playerTask, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
        work->caughtTicks = 0;
    }
    SCRATCH_STACK_RELEASE_BYTES(ACTOR_403200_HOLD_SCRATCH_BYTES);
}

/// Tests a bounded contact prefix for a player-body collision key.
///
/// Returns 1 for the player/companion body kind, otherwise 0. Stops at the
/// first zero key or contactCount entries; nonpositive counts return 0.
/// The count measures elements and must not exceed the borrowed readable
/// table's capacity. No contact is consumed or changed.
static inline s32 _actor403200HasPlayerContact(const WorldCollisionContact* contacts, s16 contactCount)
{
    s16 contactIndex;

    for (contactIndex = 0; contactIndex < contactCount; contactIndex++) {
        if (contacts[contactIndex].key.value == 0) {
            break;
        }
        if ((contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
            return 1;
        }
    }
    return 0;
}

/// Extends and retracts the limb swipe, catching a player contact on its strike.
///
/// State 11 starts clip 4, enables the capsule for the new slot-1 cue 12, and
/// blends to recovery clip 5 at tick 60. Reach uses coordinate units and retained
/// 16-bit arithmetic. A player contact applies attack 4 and installs caught clip
/// 2; the host tick controls release. Tick 220 returns to attack selection.
/// Requires initialized host/player work, escort 0's model, collision contacts,
/// animation rigs and scratch; its own 48-byte reservation is untouched.
static void _actor403200SwipeState(Task* hostTask)
{
    enum {
        ACTOR_403200_SWIPE_SCRATCH_BYTES      = 48,
        ACTOR_403200_SWIPE_STRIKE_CUE         = 12,
        ACTOR_403200_SWIPE_RECOVERY_CUE       = 28,
        ACTOR_403200_SWIPE_RECOVERY_TICK      = 60,
        ACTOR_403200_SWIPE_RETURN_TICK        = 220,
        ACTOR_403200_SWIPE_ATTACK_ID          = 4,
        ACTOR_403200_SWIPE_VIEW_START_TICK    = 21,
        ACTOR_403200_SWIPE_LIMB_UPWARD        = 1,
        ACTOR_403200_SWIPE_LIMB_HOLD_TIP      = 2,
        ACTOR_403200_SWIPE_LIMB_FOLD_BASE     = 4,
        ACTOR_403200_SWIPE_LIMB_FAST_DOWNWARD = 5,
    };
    GluttonWork* work;
    GluttonWork* flagWork;
    Enemy*       enemy;
    Task*        playerTask;
    Task*        damageTarget;
    s16          flagIndex;

    s16 strikeCue;
    s16 recoveryCue;
    s16 damageReply;
    s32 resetId;
    s32 resetPan;
    s32 swipeId;
    s32 swipePan;
    s32 swipe2Id;
    s32 swipe2Pan;
    s32 hitId;
    s32 hitPan;
    s32 cueId;
    s32 cuePan;

    work       = hostTask->work;
    enemy      = hostTask->spawnArg2.pointer;
    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    SCRATCH_STACK_RESERVE_BYTES(ACTOR_403200_SWIPE_SCRATCH_BYTES);

    if (work->stateChanged != 0) {
        work->lastAttack           = GLUTTON_STATE_SWIPE;
        work->animId               = GLUTTON_ANIM_SWIPE;
        work->animStep             = GLUTTON_ANIM_STEP_RESTART;
        flagWork                   = hostTask->work;
        flagWork->freeCountdown    = 0;
        hostTask->extra.tmd->flags = 0;
        flagIndex                  = 0;
        ACTOR_403200_COPY_ESCORT_MODEL_FLAGS(hostTask, flagWork, flagIndex);
        _actor403200AllocateModelBuffers(hostTask);
        work->neckYawEnabled   = 1;
        work->neckPitchEnabled = 0;
        work->hostExposed      = 1;
        work->limbPoseEnabled  = 1;
        gfxRotMatrixY(&work->swipeCoord.coord, work->neckYaw, 1);
        work->swipeCoord.composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&work->swipeCoord);
        work->wallDistanceTarget = 0xC80;
        resetId                  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x17);
        resetPan                 = (s8)worldCoordGetOriginAudioPan(hostTask->extra.tmd->coords);
        sndEvtRequestScriptStart(resetId, resetPan,
                                 (s8)worldCoordGetOriginAudioDepth(hostTask->extra.tmd->coords));
    }

    work->swipeCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&work->swipeCoord);

    // Enable the strike only for the newly reached cue, before animation advances.
    if (work->animId == GLUTTON_ANIM_SWIPE && (strikeCue = work->hostRig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == ACTOR_403200_SWIPE_STRIKE_CUE &&
        work->prevSwipeCue != strikeCue) {
        work->shakeLevel       = GLUTTON_SHAKE_LONG;
        work->swipeBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        padScriptSpawnVariableMotorRamp(0x30, 0xFF, 8);
        swipeId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x19);
        swipePan = (s8)worldCoordGetOriginAudioPan(
            &work->escorts[0]->task->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(
            swipeId, swipePan,
            (s8)(worldCoordGetOriginAudioDepth(
                     &work->escorts[0]->task->extra.tmd->coords[1]) /
                 2));
        swipe2Id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x1A);
        swipe2Pan = (s8)worldCoordGetOriginAudioPan(
            &work->escorts[0]->task->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(
            swipe2Id, swipe2Pan,
            (s8)(worldCoordGetOriginAudioDepth(
                     &work->escorts[0]->task->extra.tmd->coords[1]) /
                 2));
    } else {
        work->swipeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }

    if (work->animId == ACTOR_403200_CLIP_SWIPE_RECOVERY && (recoveryCue = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == ACTOR_403200_SWIPE_RECOVERY_CUE &&
        work->prevSwipeCue != recoveryCue) {
        work->hostExposed = 0;
        work->shakeLevel  = GLUTTON_SHAKE_LONG;
        padScriptSpawnVariableMotorRamp(0x20, 0x7F, 8);
        hitId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x1B);
        hitPan = (s8)worldCoordGetOriginAudioPan(
            &work->escorts[0]->task->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(
            hitId, hitPan,
            (s8)(worldCoordGetOriginAudioDepth(
                     &work->escorts[0]->task->extra.tmd->coords[1]) /
                 2));
    }

    if (work->animId == GLUTTON_ANIM_SWIPE) {
        work->prevSwipeCue = work->hostRig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    } else {
        work->prevSwipeCue = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    }

    switch (work->stateTicks) {
        case 0x14:
            gGluttonLimbReach = 0x640;
            work->limbPose    = ACTOR_403200_LIMB_POSE_DOWNWARD;
            break;
        case 0x22:
            work->limbPose = ACTOR_403200_SWIPE_LIMB_UPWARD;
            break;
        case 0x2B:
            work->limbPose = ACTOR_403200_SWIPE_LIMB_FAST_DOWNWARD;
            cueId          = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x18);
            cuePan         = (s8)worldCoordGetOriginAudioPan(
                &work->escorts[0]->task->extra.tmd->coords[1]);
            sndEvtRequestScriptStart(
                cueId, cuePan,
                (s8)(worldCoordGetOriginAudioDepth(
                         &work->escorts[0]->task->extra.tmd->coords[1]) /
                     2));
            break;
        case 0x2D:
            work->limbPose = ACTOR_403200_SWIPE_LIMB_HOLD_TIP;
            break;
        case 0x38:
            work->limbPose = ACTOR_403200_SWIPE_LIMB_FOLD_BASE;
            break;
        case 0x44:
            work->limbPose = ACTOR_403200_LIMB_POSE_SHALLOW_CURVE;
            break;
        case ACTOR_403200_SWIPE_RETURN_TICK:
            work->state = ACTOR_403200_STATE_CHOOSE_ATTACK;
            break;
    }

    if ((u32)((u16)work->stateTicks - 0x29) < 6 && gGluttonLimbReach < 0x1770) {
        gGluttonLimbReach = (u16)gGluttonLimbReach + 0x258;
    }

    _gluttonTickAnim(hostTask);

    // Collision from the previous pass installs the player's caught animation.
    if (_actor403200HasPlayerContact(work->swipeContacts, ARRAY_SIZE(work->swipeContacts)) != 0 && enemy->hp > 0 &&
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &D_actor_403200_8015FA00, 0) == 0) {
        damageTarget           = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        damageReply            = taskMessageDispatch(damageTarget, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, ACTOR_403200_SWIPE_ATTACK_ID), 0);
        work->swipeDamageReply = damageReply;
        if (damageReply == 1) {
            ((GameActor*)playerTask->work)->state = ACTOR_403200_PLAYER_FATAL_DAMAGE_STATE;
        }
        work->playerAnim.source.sets = D_actor_403200_8015E6AC;
        work->playerCaught           = 1;
        work->playerAnim.animationId = ACTOR_403200_PLAYER_CLIP_SWIPE_CAUGHT;
        work->playerAnim.blend       = ANIMATION_BLEND_RESET;
        work->playerAnim.blendFrames = 0;
        TASK_MESSAGE_DISPATCH_POINTER(playerTask, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
        work->caughtTicks = 0;
    }

    if (work->stateTicks == ACTOR_403200_SWIPE_RECOVERY_TICK && work->animId == GLUTTON_ANIM_SWIPE) {
        work->animId   = ACTOR_403200_CLIP_SWIPE_RECOVERY;
        work->animStep = GLUTTON_ANIM_STEP_BLEND;
    }

    // Keep the signed reach test separate from each unsigned-halfword subtraction.
    if (work->stateTicks >= 0x39) {
        if (gGluttonLimbReach >= 0xBB9) {
            gGluttonLimbReach = (u16)gGluttonLimbReach - 0xC8;
        } else {
            gGluttonLimbReach = (u16)gGluttonLimbReach - 0x1E;
        }
    }

    if (work->stateTicks >= ACTOR_403200_SWIPE_VIEW_START_TICK) {
        work->viewSelector = ACTOR_403200_VIEW_SELECTOR_SWIPE;
    }

    SCRATCH_STACK_RELEASE_BYTES(ACTOR_403200_SWIPE_SCRATCH_BYTES);
}

/// Shows the boss at its drop-in position and settles its root onto the floor.
///
/// Entry restarts idle animation and allocates host/live-escort buffers. Height
/// steps alternate within four-tick groups, starting gently at tick 21 and
/// faster at tick 61; negative height clamps to zero. Animation completion
/// selects shown state. Requires live models, rigs and initialized scratch.
static void _actor403200DropInState(Task* task)
{
    enum {
        ACTOR_403200_DROP_SLOW_START_TICK = 21,
        ACTOR_403200_DROP_FAST_START_TICK = 61,
        ACTOR_403200_DROP_ROOT_X          = 4200,
        ACTOR_403200_DROP_ROOT_Y          = 2000,
        ACTOR_403200_DROP_ROOT_Z          = -6000,
    };
    GluttonWork* work;
    GluttonWork* flagWork;
    s16          flagIndex;
    s16          stateFrame;

    work = task->work;
    if (work->stateChanged != 0) {
        work->animId            = ACTOR_403200_CLIP_IDLE;
        work->animStep          = GLUTTON_ANIM_STEP_RESTART;
        flagWork                = task->work;
        flagWork->freeCountdown = 0;
        task->extra.tmd->flags  = 0;
        flagIndex               = 0;
        ACTOR_403200_COPY_ESCORT_MODEL_FLAGS(task, flagWork, flagIndex);
        _actor403200AllocateModelBuffers(task);
        task->extra.tmd->coords->coord.t[1] = ACTOR_403200_DROP_ROOT_Y;
        task->extra.tmd->coords->coord.t[0] = ACTOR_403200_DROP_ROOT_X;
        task->extra.tmd->coords->coord.t[2] = ACTOR_403200_DROP_ROOT_Z;
        work->wallDistanceTarget            = 0xFA0;
    }
    _gluttonTickAnim(task);
    if (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ACTOR_403200_STATE_SHOWN;
    }
    if (task->extra.tmd->coords->coord.t[1] > 0) {
        stateFrame = work->stateTicks;
        if (stateFrame >= ACTOR_403200_DROP_FAST_START_TICK) {
            task->extra.tmd->coords->coord.t[1] +=
                ((stateFrame % 4) < 2) ? 0x50 : -0x64;
        } else if (stateFrame >= ACTOR_403200_DROP_SLOW_START_TICK) {
            task->extra.tmd->coords->coord.t[1] +=
                ((stateFrame % 4) < 2) ? 0x14 : -0x1E;
        }
    }
    if (task->extra.tmd->coords->coord.t[1] < 0) {
        task->extra.tmd->coords->coord.t[1] = 0;
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Spits three globs and then enters the debris attack.
///
/// State 6 launches at ticks 57, 69 and 76, restarting the repeated launch clip
/// between spits. Animation completion selects state 7. Requires live host work,
/// enemy, escort models and initialized animation rigs, plus successful glob
/// spawns. Positions and projectile lifetimes are owned by the spawn handlers.
static void _actor403200GlobLaunchState(Task* hostTask)
{
    enum {
        ACTOR_403200_GLOB_FIRST_LAUNCH_TICK = 57,
        ACTOR_403200_GLOB_VIEW_START_TICK   = 21,
    };
    GluttonWork* work;
    GluttonWork* flagWork;
    Enemy*       enemy;
    Enemy*       glob;
    s16          flagIndex;

    s16 launchTick;
    s32 soundId;
    s32 soundPan;

    work  = hostTask->work;
    enemy = hostTask->spawnArg2.pointer;
    if (work->stateChanged != 0) {
        work->lastAttack           = ACTOR_403200_STATE_GLOBS;
        work->animId               = ACTOR_403200_CLIP_GLOB_FIRST;
        work->animStep             = GLUTTON_ANIM_STEP_RESTART;
        flagWork                   = hostTask->work;
        flagWork->freeCountdown    = 0;
        hostTask->extra.tmd->flags = 0;
        flagIndex                  = 0;
        ACTOR_403200_COPY_ESCORT_MODEL_FLAGS(hostTask, flagWork, flagIndex);
        _actor403200AllocateModelBuffers(hostTask);
        work->neckYawEnabled     = 1;
        work->neckPitchEnabled   = 1;
        work->hostExposed        = 0;
        work->wallDistanceTarget = 0xC80;
        soundId                  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x17);
        soundPan                 = (s8)worldCoordGetOriginAudioPan(hostTask->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, soundPan,
                                 (s8)worldCoordGetOriginAudioDepth(hostTask->extra.tmd->coords));
    }
    launchTick = work->stateTicks - ACTOR_403200_GLOB_FIRST_LAUNCH_TICK;
    switch (launchTick) {
        case 6:
            work->animId   = ACTOR_403200_CLIP_GLOB_REPEAT;
            work->animStep = GLUTTON_ANIM_STEP_BLEND;
            break;
        case 13:
            work->animId   = ACTOR_403200_CLIP_GLOB_REPEAT;
            work->animStep = GLUTTON_ANIM_STEP_RESTART;
            break;
        case 0:
        case 12:
        case 19:
            glob              = enemySpawnFromTable(gGluttonEscortTasks, 0, 0, hostTask->spawnArg2.pointer);
            glob->workType    = ENEMY_WORK_PLAIN;
            work->lastSpawned = glob;
            break;
    }
    _gluttonTickAnim(hostTask);
    if (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ACTOR_403200_STATE_DEBRIS;
    }
    if (work->stateTicks >= ACTOR_403200_GLOB_VIEW_START_TICK) {
        work->viewSelector = ACTOR_403200_VIEW_SELECTOR_GLOBS;
    }
}

/// Emits drifting debris and falling chunks until the debris clip completes.
///
/// State 7 starts clip 11. From tick 61 it emits a sprite every third tick from
/// escort 0's part 1, offset 800 coordinate units along a flattened direction;
/// chunks spawn on ticks congruent to 4 modulo 10. Animation completion returns
/// to attack selection and stops the loop sound. Requires live models/rigs,
/// successful chunk spawns and initialized scratch for nested coordinate calls.
static void _actor403200DebrisState(Task* hostTask)
{
    enum {
        ACTOR_403200_DEBRIS_EMIT_START_TICK      = 61,
        ACTOR_403200_DEBRIS_VIEW_START_TICK      = 21,
        ACTOR_403200_DEBRIS_EMIT_OFFSET          = 800,
        ACTOR_403200_DEBRIS_EMIT_PERIOD          = 3,
        ACTOR_403200_DEBRIS_CHUNK_PERIOD         = 10,
        ACTOR_403200_DEBRIS_CHUNK_TICK_REMAINDER = 4,
        // Size 1664, period 5, speed 160, parent-forward, palette 1, alternate sheet.
        // Preserve bit 15, which is omitted from the extracted frame period.
        ACTOR_403200_DEBRIS_SPRITE_OPTIONS = 0x97A0D680,
    };
    GluttonWork* work;
    GluttonWork* flagWork;
    Enemy*       enemy;
    Enemy*       chunk;
    SVECTOR      emissionVector;
    SVECTOR*     emissionPointer;
    s16          flagIndex;

    s32 resetId;
    s32 resetPan;
    s32 cueId;
    s32 cuePan;
    s32 hitId;
    s32 hitPan;

    work  = hostTask->work;
    enemy = hostTask->spawnArg2.pointer;
    if (work->stateChanged != 0) {
        work->lastAttack           = ACTOR_403200_STATE_DEBRIS;
        work->animId               = ACTOR_403200_CLIP_DEBRIS;
        work->animStep             = GLUTTON_ANIM_STEP_RESTART;
        flagWork                   = hostTask->work;
        flagWork->freeCountdown    = 0;
        hostTask->extra.tmd->flags = 0;
        flagIndex                  = 0;
        ACTOR_403200_COPY_ESCORT_MODEL_FLAGS(hostTask, flagWork, flagIndex);
        _actor403200AllocateModelBuffers(hostTask);
        work->neckPitchEnabled   = 1;
        work->neckYawEnabled     = 1;
        work->hostExposed        = 0;
        work->wallDistanceTarget = 0xC80;
        resetId                  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x17);
        resetPan                 = (s8)worldCoordGetOriginAudioPan(hostTask->extra.tmd->coords);
        sndEvtRequestScriptStart(resetId, resetPan,
                                 (s8)worldCoordGetOriginAudioDepth(hostTask->extra.tmd->coords));
    }

    if (work->stateTicks == 0x3B) {
        cueId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x16);
        cuePan = (s8)worldCoordGetOriginAudioPan(
            &work->escorts[0]->task->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(
            cueId, cuePan,
            (s8)worldCoordGetOriginAudioDepth(
                &work->escorts[0]->task->extra.tmd->coords[1]));
    }

    if (work->stateTicks == 0x3C) {
        hitId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0D);
        hitPan = (s8)worldCoordGetOriginAudioPan(
            &work->escorts[0]->task->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(
            hitId, hitPan,
            (s8)worldCoordGetOriginAudioDepth(
                &work->escorts[0]->task->extra.tmd->coords[1]));
    }

    if ((s16)(u16)work->stateTicks >= ACTOR_403200_DEBRIS_EMIT_START_TICK) {
        if ((s16)((s16)(u16)work->stateTicks % ACTOR_403200_DEBRIS_EMIT_PERIOD) == 0) {
            // Build an emitter from the limb part, excluding the view transform.
            _actorRenderAccumulateWorldRotation(
                &work->escorts[0]->task->extra.tmd->coords[1],
                &D_actor_403200_8015F920.coord);

            emissionVector.vz = 0;
            emissionVector.vy = 0;
            emissionVector.vx = 0;
            _actorRenderTransformLocalPointToWorld(&work->escorts[0]->task->extra.tmd->coords[1],
                                                   &emissionVector);

            D_actor_403200_8015F920.parent     = &gGfxViewCoord;
            D_actor_403200_8015F920.coord.t[0] = emissionVector.vx;
            D_actor_403200_8015F920.coord.t[1] = emissionVector.vy;
            D_actor_403200_8015F920.coord.t[2] = emissionVector.vz;
            gfxRotMatrixY(&D_actor_403200_8015F920.coord, 0x80, 0);
            gfxRotMatrixX(&D_actor_403200_8015F920.coord, -0x80, GRAPHICS_ROTATION_COMPOSE);
            gfxReadMatrixZAxis(&D_actor_403200_8015F920.coord, &emissionVector);

            emissionPointer   = &emissionVector;
            emissionVector.vy = 0;
            VectorNormalSS(emissionPointer, emissionPointer);

            gte_lddp(ACTOR_403200_DEBRIS_EMIT_OFFSET);
            gte_ldsv(emissionPointer);
            gte_gpf12();
            gte_stsv(emissionPointer);

            D_actor_403200_8015F920.coord.t[0]  += emissionVector.vx;
            D_actor_403200_8015F920.coord.t[1]  += emissionVector.vy;
            D_actor_403200_8015F920.coord.t[2]  += emissionVector.vz;
            D_actor_403200_8015F920.composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(&D_actor_403200_8015F920);
            effectSpawn(EFFECT_SHELTER_B3_DUMPING_HOLE_DRIFT_SPRITE, &D_actor_403200_8015F920, ACTOR_403200_DEBRIS_SPRITE_OPTIONS, NULL);
        }
        if ((s16)((s16)(u16)work->stateTicks % ACTOR_403200_DEBRIS_CHUNK_PERIOD) == ACTOR_403200_DEBRIS_CHUNK_TICK_REMAINDER) {
            chunk             = enemySpawnFromTable(gGluttonEscortTasks, 2, 0, hostTask->spawnArg2.pointer);
            chunk->workType   = ENEMY_WORK_PLAIN;
            work->lastSpawned = chunk;
        }
    }

    _gluttonTickAnim(hostTask);

    if (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ACTOR_403200_STATE_CHOOSE_ATTACK;
        sndEvtRequestScriptStop((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0D), SOUND_SCRIPT_STOP_KEEP_RELEASE);
    }

    if (work->stateTicks >= ACTOR_403200_DEBRIS_VIEW_START_TICK) {
        work->viewSelector = ACTOR_403200_VIEW_SELECTOR_DEBRIS;
    }
}

/// Plays the room-selected pose while keeping the camera locked and limb retracted.
///
/// State 5 retains its requested clip. Clip 20 fast-forwards to slot-1 cue 52
/// at six times normal rate, then blends to clip 13 on completion. Clip 13's
/// new slot-2 cue 21 requests rumble and shake. Clip 9 prepares a coordinate
/// under host part 4 at tick 45; its further role is unproven. Requires live work,
/// models, rigs and scratch; clip 20 must be able to reach the fast-forward cue.
static void _actor403200ScriptedPoseState(Task* hostTask)
{
    enum {
        ACTOR_403200_POSE_RETRACT_CUE      = 21,
        ACTOR_403200_POSE_FAST_FORWARD_CUE = 52,
    };
    GluttonWork* work;
    GluttonWork* flagWork;
    GfxCoord*    attachmentCoord;
    GfxCoord*    hostRoot;
    s32          previousAnimId;
    s32          poseCue;
    s16          flagIndex;

    work = hostTask->work;
    if (work->stateChanged != 0) {
        flagWork                   = hostTask->work;
        work->freeCountdown        = 0;
        hostTask->extra.tmd->flags = 0;
        flagIndex                  = 0;
        ACTOR_403200_COPY_ESCORT_MODEL_FLAGS(hostTask, flagWork, flagIndex);
        _actor403200AllocateModelBuffers(hostTask);
        work->animRate           = ANIMATION_RATE_ONE;
        work->viewLocked         = 1;
        work->limbPose           = ACTOR_403200_LIMB_POSE_DOWNWARD;
        work->wallDistanceTarget = 0xFA0;
    }
    if (work->animId == ACTOR_403200_CLIP_RETRACT_LIMB) {
        poseCue = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (poseCue == ACTOR_403200_POSE_RETRACT_CUE && work->clip.prevSlot2Cue != poseCue) {
            work->shakeLevel = GLUTTON_SHAKE_LONG;
            padScriptSpawn(D_actor_403200_80141C6C, D_actor_403200_80141C74);
        }
        work->clip.prevSlot2Cue = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    }
    if (work->animId == 9 && work->stateTicks == 0x2D) {
        hostRoot        = hostTask->extra.tmd->coords;
        attachmentCoord = &D_actor_403200_8015F970;
        gfxSetRotIdentity(&attachmentCoord->coord);
        D_actor_403200_8015F970.coord.t[1]   = -0x64;
        D_actor_403200_8015F970.coord.t[0]   = 0;
        D_actor_403200_8015F970.coord.t[2]   = 0x64;
        D_actor_403200_8015F970.composeStamp = GRAPHICS_COORD_DIRTY;
        D_actor_403200_8015F970.parent       = &hostRoot[4];
        actorRenderComposeCoord(&D_actor_403200_8015F970);
    }
    previousAnimId = work->animId;
    if (previousAnimId == ACTOR_403200_CLIP_DEBRIS_POSE) {
        if (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
            work->animId   = ACTOR_403200_CLIP_RETRACT_LIMB;
            work->animStep = GLUTTON_ANIM_STEP_BLEND;
        }
        if (work->animId == previousAnimId && work->animStep == GLUTTON_ANIM_STEP_RESTART) {
            work->animRate = (6 * ANIMATION_RATE_ONE);
            _gluttonTickAnim(hostTask);
            while ((u32)(work->hostRig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) < ACTOR_403200_POSE_FAST_FORWARD_CUE) {
                _gluttonTickAnim(hostTask);
            }
            work->animRate = ANIMATION_RATE_ONE;
        }
    }
    if (gGluttonLimbReach >= ACTOR_403200_LIMB_RETRACTION_MIN_REACH) {
        gGluttonLimbReach = (u16)gGluttonLimbReach - ACTOR_403200_LIMB_RETRACTION_STEP;
    }
    _gluttonTickAnim(hostTask);
    hostTask->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Plays the room-commanded collapse with its skip, rumble and sound cues.
///
/// State 12 preserves the clip selected by the room command, normally clip 18.
/// Entry advances up to `collapseSkip / 4` animation ticks at four times normal
/// rate, stopping at the slot boundary, then restores normal rate. Limb reach
/// retracts 200 coordinate units above 500; new slot-2 cue 28 shakes the view.
/// Requires live work/enemy, models and rigs, initialized scratch, and an active
/// room director to select subsequent states.
static void _actor403200CollapseState(Task* hostTask)
{
    enum {
        ACTOR_403200_COLLAPSE_RETRACT_MIN_REACH = 501,
        ACTOR_403200_COLLAPSE_IMPACT_CUE        = 28,
    };
    GluttonWork* work;
    GluttonWork* flagWork;
    Enemy*       enemy;
    Enemy*       soundEnemy;
    s16          flagIndex;
    s16          progressIndex;
    s32          impactCue;
    s32          soundCue;

    work  = hostTask->work;
    enemy = hostTask->spawnArg2.pointer;
    if (work->stateChanged != 0) {
        soundEnemy                 = hostTask->spawnArg2.pointer;
        flagWork                   = hostTask->work;
        work->freeCountdown        = 0;
        hostTask->extra.tmd->flags = 0;
        flagIndex                  = 0;
        ACTOR_403200_COPY_ESCORT_MODEL_FLAGS(hostTask, flagWork, flagIndex);
        _actor403200AllocateModelBuffers(hostTask);
        // Honor the room command's skip without advancing past the clip boundary.
        work->animRate         = (4 * ANIMATION_RATE_ONE);
        work->neckPitchEnabled = 0;
        work->neckYawEnabled   = 0;
        progressIndex          = 0;
        while (progressIndex < work->collapseSkip / 4) {
            _gluttonTickAnim(hostTask);
            progressIndex++;
            if (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
                break;
            }
        }
        work->viewSelector = ACTOR_403200_VIEW_SELECTOR_COLLAPSE;
        work->animRate     = ANIMATION_RATE_ONE;
        sndEvtRequestScriptStop((((u16)soundEnemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0A), SOUND_SCRIPT_STOP_KEEP_RELEASE);
        work->limbPose           = ACTOR_403200_LIMB_POSE_DOWNWARD;
        work->wallDistanceTarget = 0xFA0;
    }
    if (gGluttonLimbReach >= ACTOR_403200_COLLAPSE_RETRACT_MIN_REACH) {
        gGluttonLimbReach = (u16)gGluttonLimbReach - ACTOR_403200_LIMB_RETRACTION_STEP;
    }
    _gluttonTickAnim(hostTask);
    hostTask->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    impactCue                                 = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    if (impactCue == ACTOR_403200_COLLAPSE_IMPACT_CUE && work->clip.prevSlot2Cue != impactCue) {
        work->shakeLevel = GLUTTON_SHAKE_LONG;
        padScriptSpawn(D_actor_403200_80141C5C, D_actor_403200_80141C64);
    }
    work->clip.prevSlot2Cue = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    if (work->animId == ACTOR_403200_CLIP_COLLAPSE) {
        soundCue = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (soundCue == 0x33 && work->prevSlot3Cue != soundCue) {
            s32 soundId;
            s32 soundPan;

            soundId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x13);
            soundPan = (s8)worldCoordGetOriginAudioPan(hostTask->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, soundPan,
                                     (s8)worldCoordGetOriginAudioDepth(hostTask->extra.tmd->coords));
        }
        soundCue = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (soundCue == 0x3D && work->prevSlot3Cue != soundCue) {
            s32 soundId;
            s32 soundPan;

            soundId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x03);
            soundPan = (s8)worldCoordGetOriginAudioPan(hostTask->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, soundPan,
                                     (s8)worldCoordGetOriginAudioDepth(hostTask->extra.tmd->coords));
        }
        soundCue = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (soundCue == 0x4E && work->prevSlot3Cue != soundCue) {
            s32 soundId;
            s32 soundPan;

            soundId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x14);
            soundPan = (s8)worldCoordGetOriginAudioPan(hostTask->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, soundPan,
                                     (s8)worldCoordGetOriginAudioDepth(hostTask->extra.tmd->coords));
        }
        soundCue = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (soundCue == 0x71 && work->prevSlot3Cue != soundCue) {
            s32 soundId;
            s32 soundPan;

            soundId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x15);
            soundPan = (s8)worldCoordGetOriginAudioPan(hostTask->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, soundPan,
                                     (s8)worldCoordGetOriginAudioDepth(hostTask->extra.tmd->coords));
        }
        work->prevSlot3Cue = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    }
}

/// Plays the limb-retraction reaction and returns to attack selection when it ends.
///
/// Entry exposes the host, disables neck tracking and selects clip 13. Later
/// ticks retract 200 coordinate units while reach is at least 401, and restore
/// the default camera selector from tick 21. Requires live host work/enemy,
/// models, rigs and initialized scratch, including the unused 12-byte block.
static void _actor403200RetractLimbState(Task* task)
{
    enum {
        ACTOR_403200_RETRACT_VIEW_RESET_TICK = 21,
        ACTOR_403200_RETRACT_SCRATCH_BYTES   = 12,
    };
    GluttonWork* work;
    Enemy*       enemy;
    s32          previousAnimId;
    s32          soundId;
    s32          soundPan;

    work = task->work;
    if (work->stateChanged != 0) {
        enemy                  = task->spawnArg2.pointer;
        previousAnimId         = work->animId;
        work->neckPitchEnabled = 0;
        work->neckYawEnabled   = 0;
        work->hostExposed      = 1;
        if (previousAnimId != ACTOR_403200_CLIP_RETRACT_LIMB) {
            work->animStep = GLUTTON_ANIM_STEP_BLEND;
            work->animId   = ACTOR_403200_CLIP_RETRACT_LIMB;
        } else {
            work->animStep = GLUTTON_ANIM_STEP_RESTART;
            work->animId   = previousAnimId;
        }
        soundId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 4);
        soundPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, soundPan,
                                 (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        work->limbPose           = ACTOR_403200_LIMB_POSE_SHALLOW_CURVE;
        work->wallDistanceTarget = 0xC80;
        sndEvtRequestScriptStop((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0D), SOUND_SCRIPT_STOP_KEEP_RELEASE);
        sndEvtRequestScriptStop((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 9), SOUND_SCRIPT_STOP_KEEP_RELEASE);
        return;
    }
    // This twelve-byte reservation is unused by the recovered body.
    SCRATCH_STACK_RESERVE_BYTES(ACTOR_403200_RETRACT_SCRATCH_BYTES);
    _gluttonTickAnim(task);
    if (gGluttonLimbReach >= ACTOR_403200_LIMB_RETRACTION_MIN_REACH) {
        gGluttonLimbReach = (u16)gGluttonLimbReach - ACTOR_403200_LIMB_RETRACTION_STEP;
        work->limbPose    = ACTOR_403200_LIMB_POSE_DOWNWARD;
    }
    if (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = ACTOR_403200_STATE_CHOOSE_ATTACK;
    }
    if (work->stateTicks >= ACTOR_403200_RETRACT_VIEW_RESET_TICK) {
        work->viewSelector = 0;
    }
    SCRATCH_STACK_RELEASE_BYTES(ACTOR_403200_RETRACT_SCRATCH_BYTES);
}

/// Aims the neck at the player and selects the next attack, arena advance or heal.
///
/// Entry blends to idle and arms a 40-tick attack delay only if none is pending.
/// Phase 0/1 HP thresholds trigger advancement; otherwise phase, range and the
/// previous attack choose the next attack. Positive signed pendingHeals wins.
/// Range is measured in coordinate units from root + (0, 250, -607). Neck yaw
/// uses 4096 units per turn and signed-halfword offsets in the roots' common
/// parent frame. Requires live host/player work, models, rigs and scratch.
static void _actor403200ChooseAttackState(Task* task)
{
    enum {
        ACTOR_403200_ATTACK_DELAY_TICKS             = 40,
        ACTOR_403200_ATTACK_RANGE_ORIGIN_Y          = 250,
        ACTOR_403200_ATTACK_RANGE_ORIGIN_NEGATIVE_Z = 607,
        ACTOR_403200_PHASE0_ADVANCE_HP              = 1500,
        ACTOR_403200_PHASE1_ADVANCE_HP              = 800,
        ACTOR_403200_PHASE0_RAIN_MIN_X              = 10001,
        ACTOR_403200_PHASE1_GLOB_MIN_RANGE          = 8401,
        ACTOR_403200_PHASE1_DEBRIS_MIN_RANGE        = 6301,
        ACTOR_403200_PHASE2_FAR_MIN_RANGE           = 9001,
    };
    _Actor403200IdleScratch* scratch;
    GluttonWork*             work;
    Enemy*                   enemy;
    Task*                    player;

    work   = task->work;
    enemy  = task->spawnArg2.pointer;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);

    if (work->stateChanged != 0) {
        work->neckYawEnabled   = 1;
        work->neckPitchEnabled = 1;
        work->neckPitchTarget  = 0;
        if (work->attackDelay == 0) {
            work->attackDelay = ACTOR_403200_ATTACK_DELAY_TICKS;
        }
        work->wallDistanceTarget = 0xE10;
        work->animId             = ACTOR_403200_CLIP_IDLE;
        work->animStep           = GLUTTON_ANIM_STEP_BLEND;
        work->hostExposed        = 0;
        work->animRate           = ANIMATION_RATE_ONE;
    }
    if (gGluttonLimbReach >= ACTOR_403200_LIMB_RETRACTION_MIN_REACH) {
        gGluttonLimbReach = (u16)gGluttonLimbReach - ACTOR_403200_LIMB_RETRACTION_STEP;
        work->limbPose    = ACTOR_403200_LIMB_POSE_DOWNWARD;
    }
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor403200IdleScratch);
    _gluttonTickAnim(task);

    work->neckYawTarget = _actorAngleTurnToPlayer(task, &scratch->toPlayer, &gPlayerStatus);
    if (gGluttonEnded == 1) {
        work->stateTicks = 0;
    }
    // Phase advancement precedes range-based attacks; healing overrides the choice.
    if (work->attackDelay <= work->stateTicks) {
        scratch->rangeOffset.vx = player->extra.tmd->coords->coord.t[0] -
                                  task->extra.tmd->coords->coord.t[0];
        scratch->rangeOffset.vy = (player->extra.tmd->coords->coord.t[1] -
                                   task->extra.tmd->coords->coord.t[1]) -
                                  ACTOR_403200_ATTACK_RANGE_ORIGIN_Y;
        scratch->rangeOffset.vz = (player->extra.tmd->coords->coord.t[2] -
                                   task->extra.tmd->coords->coord.t[2]) +
                                  ACTOR_403200_ATTACK_RANGE_ORIGIN_NEGATIVE_Z;
        scratch->playerDistance = SquareRoot0(scratch->rangeOffset.vx * scratch->rangeOffset.vx + scratch->rangeOffset.vy * scratch->rangeOffset.vy +
                                              scratch->rangeOffset.vz * scratch->rangeOffset.vz);
        switch (work->phase) {
            case 0:
                if (enemy->hp < ACTOR_403200_PHASE0_ADVANCE_HP) {
                    work->state = GLUTTON_STATE_ADVANCE;
                } else if (player->extra.tmd->coords->coord.t[0] -
                               task->extra.tmd->coords->coord.t[0] >=
                           ACTOR_403200_PHASE0_RAIN_MIN_X) {
                    work->state = ACTOR_403200_STATE_RAIN;
                } else if (work->lastAttack != GLUTTON_STATE_INHALE) {
                    work->state = GLUTTON_STATE_INHALE;
                } else {
                    work->state = ACTOR_403200_STATE_RAIN;
                }
                break;
            case 1:
                if (enemy->hp < ACTOR_403200_PHASE1_ADVANCE_HP) {
                    work->state = GLUTTON_STATE_ADVANCE;
                } else {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    if (((gRandomLcgState >> 16) & 0xF) == 0) {
                        work->state = GLUTTON_STATE_INHALE;
                    } else if (scratch->playerDistance >= ACTOR_403200_PHASE1_GLOB_MIN_RANGE) {
                        if (work->lastAttack == ACTOR_403200_STATE_GLOBS) {
                            work->state = ACTOR_403200_STATE_RAIN;
                        } else {
                            work->state = ACTOR_403200_STATE_GLOBS;
                        }
                    } else if (scratch->playerDistance >= ACTOR_403200_PHASE1_DEBRIS_MIN_RANGE) {
                        if (work->lastAttack == ACTOR_403200_STATE_DEBRIS) {
                            work->state = ACTOR_403200_STATE_RAIN;
                        } else {
                            work->state = ACTOR_403200_STATE_DEBRIS;
                        }
                    } else {
                        work->state = ACTOR_403200_STATE_RAIN;
                    }
                }
                break;
            case 2:
                if (scratch->playerDistance >= ACTOR_403200_PHASE2_FAR_MIN_RANGE) {
                    if (work->lastAttack == ACTOR_403200_STATE_RAIN) {
                        work->state = ACTOR_403200_STATE_GLOBS;
                    } else {
                        work->state = ACTOR_403200_STATE_RAIN;
                    }
                } else if (work->lastAttack == ACTOR_403200_STATE_RAIN) {
                    work->state = GLUTTON_STATE_SWIPE;
                } else {
                    work->state = ACTOR_403200_STATE_RAIN;
                }
                break;
        }
        if ((s8)work->pendingHeals > 0) {
            work->state = GLUTTON_STATE_HEAL;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403200IdleScratch);
}

/// Orders a live summon to emerge at a phase-selected room spot.
///
/// At state tick 70, selects summon 0; at tick 120, summon 1. Other ticks do
/// nothing. The phase and slot choose the spot in command bits 8..11, and an
/// independent random draw chooses the emergence motion in bits 4..7. Low
/// nibble 1 requests emergence in synthetic command context 0x2C00.
/// Borrows work and the caller's live scratch block, overwriting its slot;
/// the command record is consumed synchronously by the summon task.
static inline void _actor403200CommandSummonOnCue(GluttonWork* work, GluttonSummonScratch* scratch)
{
    enum {
        ACTOR_403200_FIRST_SUMMON_ORDER_TICK  = 70,
        ACTOR_403200_SECOND_SUMMON_ORDER_TICK = 120,
        ACTOR_403200_SUMMON_COMMAND_STAGE     = 0,
        ACTOR_403200_SUMMON_COMMAND_AREA      = 44,
        ACTOR_403200_SUMMON_SPOT_SHIFT        = 8,
        // Spot-table rows at Z -550 (HIGH) and -12450 (LOW), with X in room units.
        ACTOR_403200_SUMMON_SPOT_HIGH_Z_X10500 = 3,
        ACTOR_403200_SUMMON_SPOT_HIGH_Z_X13500 = 4,
        ACTOR_403200_SUMMON_SPOT_HIGH_Z_X16500 = 5,
        ACTOR_403200_SUMMON_SPOT_LOW_Z_X10500  = 9,
        ACTOR_403200_SUMMON_SPOT_LOW_Z_X13500  = 10,
        ACTOR_403200_SUMMON_SPOT_LOW_Z_X16500  = 11,
        ACTOR_403200_SUMMON_MOTION_COUNT       = 3,
    };
    s32 nextRandomState;

    if (work->stateTicks == ACTOR_403200_FIRST_SUMMON_ORDER_TICK) {
        scratch->slot = 0;
    } else if (work->stateTicks == ACTOR_403200_SECOND_SUMMON_ORDER_TICK) {
        scratch->slot = 1;
    } else {
        return;
    }
    if (work->summons[scratch->slot] != NULL) {
        D_actor_403200_8015F8F4.context.loc.stage = ACTOR_403200_SUMMON_COMMAND_STAGE;
        D_actor_403200_8015F8F4.context.loc.area  = ACTOR_403200_SUMMON_COMMAND_AREA;
        switch (work->phase) {
            case 0:
                if (scratch->slot == 0) {
                    gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    if (!((gRandomLcgState >> 16) & 1)) {
                        D_actor_403200_8015F8F4.command = ACTOR_403200_SUMMON_SPOT_HIGH_Z_X10500;
                    } else {
                        D_actor_403200_8015F8F4.command = ACTOR_403200_SUMMON_SPOT_HIGH_Z_X13500;
                    }
                } else {
                    gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    if (!((gRandomLcgState >> 16) & 1)) {
                        D_actor_403200_8015F8F4.command = ACTOR_403200_SUMMON_SPOT_LOW_Z_X10500;
                    } else {
                        D_actor_403200_8015F8F4.command = ACTOR_403200_SUMMON_SPOT_LOW_Z_X13500;
                    }
                }
                break;
            case 1:
                if (scratch->slot == 0) {
                    gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    if (!((gRandomLcgState >> 16) & 1)) {
                        D_actor_403200_8015F8F4.command = ACTOR_403200_SUMMON_SPOT_LOW_Z_X13500;
                    } else {
                        D_actor_403200_8015F8F4.command = ACTOR_403200_SUMMON_SPOT_LOW_Z_X16500;
                    }
                } else {
                    gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    if (!((gRandomLcgState >> 16) & 1)) {
                        D_actor_403200_8015F8F4.command = ACTOR_403200_SUMMON_SPOT_HIGH_Z_X13500;
                    } else {
                        D_actor_403200_8015F8F4.command = ACTOR_403200_SUMMON_SPOT_HIGH_Z_X16500;
                    }
                }
                break;
            case 2:
            default:
                if (scratch->slot == 0) {
                    D_actor_403200_8015F8F4.command = ACTOR_403200_SUMMON_SPOT_HIGH_Z_X16500;
                } else {
                    D_actor_403200_8015F8F4.command = ACTOR_403200_SUMMON_SPOT_LOW_Z_X16500;
                }
                break;
        }
        // Pack the selected spot and one of three emergence motions.
        D_actor_403200_8015F8F4.command <<= ACTOR_403200_SUMMON_SPOT_SHIFT;
        nextRandomState                   = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        D_actor_403200_8015F8F4.command  |= (s16)OVERLAY_ENCOUNTER_APPEAR_COMMAND(0, ((u32)nextRandomState >> 16) % ACTOR_403200_SUMMON_MOTION_COUNT);
        gRandomLcgState                   = nextRandomState;
        TASK_MESSAGE_DISPATCH_POINTER(work->summons[scratch->slot]->task, ACTOR_COMMAND_MESSAGE_APPLY, &D_actor_403200_8015F8F4, 0);
    }
}

/// Refills two summon slots, orders their emergence and then resumes inhaling.
///
/// State 14 runs only before encounter shutdown. It caps live summons at two
/// and successful spawns at eight, borrowing current area placement 3's texture
/// offsets for new models. Clip 19 exposes the host on cues 4..12, then blends
/// to idle. Tick 331 or idle with only the host's battle reference selects inhale.
/// Requires live work/enemy/player roots, loaded summon resources and a current
/// area variant with placement 3, plus scratch for the complete summon block.
static void _actor403200SummonState(Task* hostTask)
{
    enum {
        ACTOR_403200_SUMMON_SPAWN_LIMIT     = 8,
        ACTOR_403200_SUMMON_PLACEMENT_INDEX = 3,
        ACTOR_403200_SUMMON_RETURN_TICK     = 331,
    };
    GluttonSummonScratch* scratch;
    GluttonWork*          work;
    Enemy*                enemy;
    Enemy*                summon;
    PlayerStatus*         playerStatus;

    TmdObject*     summonModel;
    AreaPlacement* placement;

    s32 cueId;
    s32 cuePan;
    s32 blastId;
    s32 blastPan;
    s32 previousAnimId;

    u32 exposureCue;

    work  = hostTask->work;
    enemy = hostTask->spawnArg2.pointer;
    if (gGluttonEnded != 1) {
        scratch = SCRATCH_STACK_RESERVE_BLOCK(GluttonSummonScratch);
        if (work->stateChanged != 0) {
            previousAnimId         = work->animId;
            work->neckPitchEnabled = 0;
            work->neckYawEnabled   = 1;
            work->hostExposed      = 0;
            if (previousAnimId != ACTOR_403200_CLIP_SUMMON) {
                work->animId   = ACTOR_403200_CLIP_SUMMON;
                work->animStep = GLUTTON_ANIM_STEP_BLEND;
            } else {
                work->animStep = GLUTTON_ANIM_STEP_RESTART;
                work->animId   = previousAnimId;
            }
            work->animRate = ANIMATION_RATE_ONE;
            // Each successful spawn owns its task; the host keeps two borrowed handles.
            for (scratch->slot = 0; scratch->slot < ARRAY_SIZE(work->summons); scratch->slot++) {
                if (work->summons[scratch->slot] == NULL && work->summonsAlive < ARRAY_SIZE(work->summons) && (u8)work->summonsSpawned < ACTOR_403200_SUMMON_SPAWN_LIMIT) {
                    work->summons[scratch->slot] = enemySpawnFromTable(&D_actor_341700_80174D58, 3, 2, NULL);
                    if (work->summons[scratch->slot] != NULL) {
                        work->summonsSpawned++;
                        summonModel                    = work->summons[scratch->slot]->task->extra.tmd;
                        placement                      = &_areaGetCurrentVariant()->placements[ACTOR_403200_SUMMON_PLACEMENT_INDEX];
                        summonModel->texturePageOffset = placement->texturePageOffset;
                        summonModel->clutRowOffset     = placement->clutRowOffset;
                        if (summonModel->buffer != NULL) {
                            tmdBuildBufferHalf(summonModel);
                            tmdBuildBufferHalf(summonModel);
                        }
                        work->summons[scratch->slot]->workType = ENEMY_WORK_PLAIN;
                        summon                                 = work->summons[scratch->slot];
                        summon->placeKey                      |= scratch->slot << ENEMY_PLACE_INDEX_SHIFT;
                        work->summonsAlive++;
                    }
                }
            }
            work->wallDistanceTarget = 0xC80;
            sndEvtRequestScriptStop((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0D), SOUND_SCRIPT_STOP_KEEP_RELEASE);
            sndEvtRequestScriptStop((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 9), SOUND_SCRIPT_STOP_KEEP_RELEASE);
        }
        if (gGluttonLimbReach >= ACTOR_403200_LIMB_RETRACTION_MIN_REACH) {
            gGluttonLimbReach = (u16)gGluttonLimbReach - ACTOR_403200_LIMB_RETRACTION_STEP;
        }
        _gluttonTickAnim(hostTask);
        if (work->animId == ACTOR_403200_CLIP_SUMMON && (exposureCue = work->hostRig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 4 && exposureCue < 0xD) {
            work->hostExposed = 1;
        } else {
            work->hostExposed = 0;
        }
        playerStatus        = &gPlayerStatus;
        work->neckYawTarget = _actorAngleTurnToPlayer(hostTask, &scratch->toPlayer, playerStatus);
        if ((work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) && work->animId == ACTOR_403200_CLIP_SUMMON) {
            work->animId   = ACTOR_403200_CLIP_IDLE;
            work->animStep = GLUTTON_ANIM_STEP_BLEND;
        }
        if (work->stateTicks >= ACTOR_403200_SUMMON_RETURN_TICK || (work->animId == ACTOR_403200_CLIP_IDLE && gSceneCombatState.battleRefs == 1)) {
            work->state = GLUTTON_STATE_INHALE;
        }
        if (work->stateTicks == 6) {
            cueId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x04);
            cuePan = (s8)worldCoordGetOriginAudioPan(hostTask->extra.tmd->coords);
            sndEvtRequestScriptStart(cueId, cuePan, (s8)worldCoordGetOriginAudioDepth(hostTask->extra.tmd->coords));
        }
        if (work->stateTicks == 0x3B) {
            blastId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x10);
            blastPan = (s8)worldCoordGetOriginAudioPan(hostTask->extra.tmd->coords);
            sndEvtRequestScriptStart(blastId, blastPan, (s8)(worldCoordGetOriginAudioDepth(hostTask->extra.tmd->coords) / 2));
            work->shakeLevel = GLUTTON_SHAKE_LONG;
            padScriptSpawn(D_actor_403200_80141C5C, D_actor_403200_80141C64);
        }
        if (work->stateTicks == 0x23) {
            work->viewSelector = ACTOR_403200_VIEW_SELECTOR_DEFAULT;
        }
        _actor403200CommandSummonOnCue(work, scratch);
        SCRATCH_STACK_RELEASE_BLOCK(GluttonSummonScratch);
    }
}

#include "../../shared/glutton_escort_state.inc.c"

/// The host's state handlers, indexed by `state`; the last six slots are
/// empty. Slots 16 and 17 have empty handlers; slot 18 hands death off to the room.
static const _Actor403200StateTable D_actor_403200_80132154 = {
    {
        _actor403200HiddenState,
        _actor403200ShownState,
        _actor403200RainLaunchState,
        _actor403200InhaleState,
        _actor403200DropInState,
        _actor403200ScriptedPoseState,
        _actor403200GlobLaunchState,
        _actor403200DebrisState,
        _actor403200RetractLimbState,
        _actor403200AdvanceState,
        _actor403200ChooseAttackState,
        _actor403200SwipeState,
        _actor403200CollapseState,
        _actor403200HoldPlayerState,
        _actor403200SummonState,
        gluttonEscortState,
        _actor403200NoopState16,
        _actor403200NoopState17,
        _actor403200DeathHandoffState,
    },
};

/// The host task's three states -- spawn/setup, per-frame tick and teardown --
/// dispatched through by state.
static const EnemyTaskFuncTable3 D_actor_403200_801321B8 = {
    {
        _actor403200Spawn,
        _actor403200Tick,
        enemyDestroy,
    },
};

/// Clears occupied contacts of all nine hit spheres and the limb swipe.
///
/// Each five-entry table must be initialized with its final-entry flag intact.
/// Keeps empty entries and the terminators for the next collision pass; neither
/// bodies nor their list membership or enabled flags are changed.
static inline void _actor403200ClearContacts(GluttonWork* work)
{
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
}

/// Updates the dumping-hole host's combat state, targets, camera and player hold.
///
/// The enemy/task pair must own initialized `GluttonWork`, rigs and models;
/// escorts 0, 1, 3 and 4 must be live. Runs the handler selected by state 0..18
/// once per active tick, with an entry flag and a saturating signed-halfword
/// timer. Paused or hidden scene control clears contacts and returns after the
/// presentation phase. Camera selection is suppressed for script-spawned hosts.
/// Death waits for summons and player release before handing control to the room.
/// Requires initialized scratch, valid view/phase indices and, at death handoff,
/// the room's ten-trigger array. Scratch reservations are released before return;
/// synchronous message payloads are borrowed only while dispatch runs.
static void _actor403200Tick(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_403200_DEATH_WAIT_TICKS            = 38,
        ACTOR_403200_DEATH_TICK_LIMIT            = 256,
        ACTOR_403200_DEATH_HANDOFF_TICK          = 8,
        ACTOR_403200_STATE_TICK_LIMIT            = 32767,
        ACTOR_403200_SWIPE_RELEASE_REPLAY_TICKS  = 40,
        ACTOR_403200_SWIPE_RELEASE_MIN_TICKS     = 23,
        ACTOR_403200_PLAYER_WEAPON_RELEASE_CLIP  = 7,
        ACTOR_403200_PLAYER_SCRIPTED_EXIT_REASON = 2,
    };
    VECTOR                   lightingPosition;
    _Actor403200StateTable   stateHandlers;
    GluttonWork*             work;
    GluttonWork*             releaseWork;
    GluttonWork*             flagWork;
    _Actor403200TickScratch* scratch;
    WorldCollisionTrigger*   triggers;
    TmdObject*               hostModel;
    TmdObject*               escortModel;
    Task*                    playerTask;
    Task*                    clampedPlayer;
    Enemy*                   litEnemy;
    s16                      releaseIndex;
    s16                      hideIndex;
    s16                      showIndex;
    s16                      caughtMode;
    u16                      caughtTicks;
    u8                       viewReady;
    u8                       battlePhase;
    SVECTOR*                 exitOrigin;
    s8                       targetFlags;
    s32                      playerZ;

    work          = task->work;
    playerTask    = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    stateHandlers = D_actor_403200_80132154;

    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);

    releaseWork = task->work;
    if (releaseWork->freeCountdown != 0) {
        task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        releaseWork->freeCountdown--;
        if (releaseWork->freeCountdown == 0) {
            hostModel         = task->extra.tmd;
            hostModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            tmdFreePrimitiveBuffer(task->extra.tmd);
            for (releaseIndex = 0; releaseIndex < ARRAY_SIZE(releaseWork->escorts); releaseIndex++) {
                if (releaseWork->escorts[releaseIndex] != NULL) {
                    escortModel         = releaseWork->escorts[releaseIndex]->task->extra.tmd;
                    escortModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
                    tmdFreePrimitiveBuffer(releaseWork->escorts[releaseIndex]->task->extra.tmd);
                }
            }
        }
    }

    lightingPosition.vx = task->extra.tmd->coords[3].workm.t[0];
    lightingPosition.vy = task->extra.tmd->coords[3].workm.t[1];
    lightingPosition.vz = task->extra.tmd->coords[3].workm.t[2];

    clampedPlayer = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (clampedPlayer->extra.tmd->coords->coord.t[1] > 0) {
        clampedPlayer->extra.tmd->coords->coord.t[1] = 0;
    }
    playerZ = clampedPlayer->extra.tmd->coords->coord.t[2];
    if (playerZ >= -0xED7) {
        clampedPlayer->extra.tmd->coords->coord.t[2] = -0xED8;
    } else if (playerZ < -0x206C) {
        clampedPlayer->extra.tmd->coords->coord.t[2] = -0x206C;
    }

    if (work->hostExposed != work->prevHostExposed) {
        worldCoordUpdateActorColor(enemy, &lightingPosition, 0, 0);
        worldCoordUpdateActorColor(work->escorts[3], &lightingPosition, 0, 0);
        work->prevHostExposed = work->hostExposed;
    }
    litEnemy = enemy;
    if (work->hostExposed == 0) {
        litEnemy = work->escorts[3];
    }
    worldCoordUpdateActorColor(litEnemy, &lightingPosition, 0, 0);

    if (work->state == GLUTTON_STATE_SWIPE && gGluttonLimbReach >= 0x7D1) {
        work->escorts[4]->task->extra.tmd->otOffset = -8;
    } else {
        work->escorts[4]->task->extra.tmd->otOffset = 0;
    }

    if (((viewGetMappedIndex() & ACTOR_403200_VIEW_INDEX_MASK) == 0x1E) || ((viewGetMappedIndex() & ACTOR_403200_VIEW_INDEX_MASK) == 0x1D)) {
        flagWork                = task->work;
        flagWork->freeCountdown = 0;
        task->extra.tmd->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        hideIndex               = 0;
        ACTOR_403200_COPY_ESCORT_MODEL_FLAGS(task, flagWork, hideIndex);
    } else if (work->state != ACTOR_403200_STATE_HIDDEN) {
        flagWork                = task->work;
        flagWork->freeCountdown = 0;
        task->extra.tmd->flags  = 0;
        showIndex               = 0;
        ACTOR_403200_COPY_ESCORT_MODEL_FLAGS(task, flagWork, showIndex);
    }

    // Presentation still updates while actor logic is suspended.
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            _actor403200ClearContacts(work);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            _actor403200ClearContacts(work);
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            break;
    }

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor403200TickScratch);

    // Consume the previous collision pass before the state changes enabled bodies.
    if (enemy->hp > 0) {
        if (work->playerCaught != 1 && work->state != ACTOR_403200_STATE_HOLD_PLAYER) {
            if (work->groups1To2Cooldown > 0) {
                work->groups1To2Cooldown = (s16)((u16)work->groups1To2Cooldown - 1);
            } else {
                _gluttonHitGroups1To2(task);
            }
            if (work->group0Cooldown > 0) {
                work->group0Cooldown = (s16)((u16)work->group0Cooldown - 1);
            } else {
                _gluttonHitGroup0(task);
            }
            if (work->groups3To5Cooldown > 0) {
                work->groups3To5Cooldown = (s16)((u16)work->groups3To5Cooldown - 1);
            } else {
                _actor403200HitGroups3To5(task);
            }
            if (work->groups6To8Cooldown > 0) {
                work->groups6To8Cooldown = (s16)((u16)work->groups6To8Cooldown - 1);
            } else {
                _gluttonHitGroups6To8(task);
            }
        }
    }
    if (enemy->hp <= 0) {
        if (gPlayerStatus.hp <= 0) {
            enemy->hp     = 1;
            gGluttonEnded = 0;
        }
    }

    if (work->summonsAlive > 0) {
        work->deathDelay = ACTOR_403200_DEATH_WAIT_TICKS;
    }
    if (work->deathDelay > 0 && work->summonsAlive == 0) {
        work->deathDelay = (s16)((u16)work->deathDelay - 1);
    }

    if (enemy->hp <= 0) {
        if (work->deathDelay > 0) {
            enemy->hp = 1;
        }
        if (enemy->hp <= 0 && gGluttonEnded == 0) {
            gGluttonEnded                             = 1;
            D_actor_403200_8015F8F4.context.loc.stage = ACTOR_403200_BROADCAST_CONTEXT_STAGE;
            D_actor_403200_8015F8F4.context.loc.area  = ACTOR_403200_BROADCAST_CONTEXT_AREA;
            D_actor_403200_8015F8F4.command           = ACTOR_403200_SUMMON_COMMAND_VANISH;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_actor_403200_8015F8F4, ACTOR_COMMAND_MESSAGE_APPLY);
            gGluttonSpinnersReleased = 0;
        }
    }

    if (gGluttonEnded == 1 && work->deathDelay <= 0 && work->playerCaught == 0 &&
        work->deathTicks < ACTOR_403200_DEATH_TICK_LIMIT) {
        work->deathTicks = (s16)((u16)work->deathTicks + 1);
    }

    if (work->deathTicks == ACTOR_403200_DEATH_HANDOFF_TICK) {
        work->state                  = ACTOR_403200_STATE_DEATH_HANDOFF;
        gSceneCombatState.battleRefs = 1;
        // Dumping Hole room variant 1 installs ten contiguous triggers quads.
        triggers              = Gp_PendingObj4C;
        triggers[9].origin.vx = task->extra.tmd->coords->coord.t[0] + 0xFA0;
        exitOrigin            = &triggers[9].origin;
        exitOrigin->vy        = task->extra.tmd->coords->coord.t[1] - 0x64;
        exitOrigin->vz        = task->extra.tmd->coords->coord.t[2];
        sndEvtRequestScriptStop((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0A), SOUND_SCRIPT_STOP_KEEP_RELEASE);
        sndEvtRequestScriptStop((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0D), SOUND_SCRIPT_STOP_KEEP_RELEASE);
    }

    if (enemy->hp <= 0) {
        battlePhase = gSceneCombatState.signals.bytes.battlePhase;
        if (battlePhase == SCENE_COMBAT_BATTLE_FINISHED) {
            work->viewSelector = battlePhase;
            work->phase        = 3;
            work->viewLocked   = 0;
        }
    }

    // Snapshot entry state before the handler may request its next transition.
    if (work->prevState != work->state) {
        work->stateChanged = 1;
        work->stateTicks   = 0;
    } else {
        if (work->stateTicks < ACTOR_403200_STATE_TICK_LIMIT) {
            work->stateTicks = (s16)((u16)work->stateTicks + 1);
        }
        work->stateChanged = 0;
    }

    work->prevState = (u16)work->state;
    if (work->state != ACTOR_403200_STATE_HIDDEN && work->state != ACTOR_403200_STATE_SCRIPTED_POSE && work->state != ACTOR_403200_STATE_COLLAPSE && work->state != GLUTTON_STATE_INHALE &&
        work->spinnersSpawned == 0) {
        viewReady = gGameSession->viewReady;
        if (viewReady == 1 && work->state != ACTOR_403200_STATE_HIDDEN && work->state != ACTOR_403200_STATE_SCRIPTED_POSE) {
            work->spinnersSpawned = viewReady;
            _actor403200SpawnSpinnerFormation(task);
        }
    }

    stateHandlers.funcs[work->state](task);

    if ((u16)work->state < 2 || work->state == ACTOR_403200_STATE_SCRIPTED_POSE || work->state == ACTOR_403200_STATE_COLLAPSE) {
        targetFlags                              = WORLD_TARGET_NOT_LOCKABLE;
        enemy->node.state.parts.flags            = WORLD_TARGET_NOT_LOCKABLE;
        work->escorts[3]->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->escorts[0]->node.state.parts.flags = targetFlags;
        work->escorts[1]->node.state.parts.flags = targetFlags;
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
        targetFlags                              = WORLD_TARGET_HIDE_HP;
        work->escorts[3]->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->escorts[0]->node.state.parts.flags = targetFlags;
        work->escorts[1]->node.state.parts.flags = targetFlags;
    }

    // Retain group 2's enabled flag even in the alternate branch.
    if (work->state != ACTOR_403200_STATE_HIDDEN && work->hostExposed == 1) {
        work->hits[0].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->hits[0].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    if (work->state != ACTOR_403200_STATE_HIDDEN && work->hostExposed != 1) {
        work->hits[1].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->hits[2].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->hits[1].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->hits[2].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
    if (work->state != ACTOR_403200_STATE_HIDDEN) {
        work->hits[3].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->hits[4].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->hits[5].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->hits[3].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->hits[4].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->hits[5].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    if (work->state != ACTOR_403200_STATE_HIDDEN) {
        work->hits[6].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->hits[7].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->hits[8].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->hits[6].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->hits[7].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->hits[8].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }

    _actor403200ClearContacts(work);
    _gluttonShakeTick(task);

    if (work->viewLocked == 0 && work->state != ACTOR_403200_STATE_HIDDEN) {
        scratch->view = D_actor_403200_8015E6E8[work->viewSelector](task, work->phase);
        if (((viewGetMappedIndex() & ACTOR_403200_VIEW_INDEX_MASK) != scratch->view) &&
            (task->spawnArg1.value >> 16) == 0) {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = scratch->view;
        }
    }

    // The swipe hold releases through its player clip; inhale holding has its own state.
    caughtMode = work->playerCaught;
    if (caughtMode == 1 && work->state != ACTOR_403200_STATE_HOLD_PLAYER) {
        caughtTicks       = work->caughtTicks + 1;
        work->caughtTicks = caughtTicks;
        if (work->swipeDamageReply == caughtMode) {
            if (work->playerAnim.animationId == ACTOR_403200_PLAYER_CLIP_SWIPE_CAUGHT) {
                work->playerAnim.source.sets = D_actor_403200_8015E6AC;
                work->playerAnim.blend       = ANIMATION_BLEND_RESET;
                work->playerAnim.blendFrames = 0;
                TASK_MESSAGE_DISPATCH_POINTER(playerTask, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
                work->caughtTicks = 0;
            }
        } else if (work->playerAnim.animationId == ACTOR_403200_PLAYER_CLIP_SWIPE_CAUGHT && (s16)caughtTicks < ACTOR_403200_SWIPE_RELEASE_REPLAY_TICKS) {
            work->playerAnim.source.sets = D_actor_403200_8015E6AC;
            work->playerAnim.blend       = ANIMATION_BLEND_RESET;
            work->playerAnim.blendFrames = 0;
            TASK_MESSAGE_DISPATCH_POINTER(playerTask, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
        }

        if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
            switch (work->playerAnim.animationId) {
                case ACTOR_403200_PLAYER_CLIP_SWIPE_CAUGHT:
                    if (work->swipeDamageReply != 1 && (s16)work->caughtTicks >= ACTOR_403200_SWIPE_RELEASE_MIN_TICKS) {
                        work->playerAnim.source.sets = D_actor_403200_8015E6AC;
                        D_actor_403200_8015E6AC[4] =
                            (Gp_PlayerAnimBlkTbl
                                 [Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon])
                                ->table.sets[ACTOR_403200_PLAYER_WEAPON_RELEASE_CLIP];
                        work->playerAnim.animationId = ACTOR_403200_PLAYER_CLIP_SWIPE_RELEASE;
                        work->playerAnim.blend       = ANIMATION_BLEND_INTERPOLATE;
                        work->playerAnim.blendFrames = 3;
                        TASK_MESSAGE_DISPATCH_POINTER(playerTask, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
                        work->caughtTicks = 0;
                    }
                    break;
                case ACTOR_403200_PLAYER_CLIP_SWIPE_RELEASE:
                    if (work->swipeDamageReply != 1) {
                        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, ACTOR_403200_PLAYER_SCRIPTED_EXIT_REASON, 0);
                        work->playerCaught = 0;
                    }
                    break;
            }
        }
    }

    if (gGluttonEnded == 1) {
        if (work->playerCaught == gGluttonEnded) {
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, ACTOR_403200_PLAYER_SCRIPTED_EXIT_REASON, 0);
            work->playerCaught = 0;
        }
    }

    SCRATCH_STACK_RELEASE_BLOCK(_Actor403200TickScratch);
}

/// Rebuilds the two angled collision-grid quads ahead of the host.
///
/// `distance` and `lowerEdgeDrop` use room-coordinate units; the drop subtracts Y at each
/// quad's lower edge. Each half extends 3000 units along a normalized local X
/// axis. Its outer edge adds 1000/4096 of the already narrowed forward offset.
/// faceIndex selects consecutive faces/normals and eight vertices starting at
/// 4 * faceIndex; the caller uses 0. Requires live host coordinates and writable
/// active-grid pools covering those indices. Normals have unit Q12 length;
/// surface class 3 belongs to the dumping-hole grid. Cell lists are retained.
static void _actor403200BuildAngledWall(Task* host, s16 distance, s16 lowerEdgeDrop, s16 faceIndex)
{
    enum {
        ACTOR_403200_ANGLED_WALL_HALF_WIDTH    = 3000,
        ACTOR_403200_ANGLED_WALL_BEND_SCALE    = 1000,
        ACTOR_403200_ANGLED_WALL_SURFACE_CLASS = 3,
    };
    SVECTOR                 halfWidthOffset;
    SVECTOR                 forwardOffset;
    SVECTOR*                normals   = Gp_GridParams->normals;
    SVECTOR*                vertices  = Gp_GridParams->vertices;
    WorldCollisionGridFace* faces     = Gp_GridParams->faces;
    WorldCollisionGridFace  rightFace = {
        { faceIndex * 4, faceIndex * 4 + 1, faceIndex * 4 + 2, faceIndex * 4 + 3 }, faceIndex, ACTOR_403200_ANGLED_WALL_SURFACE_CLASS
    };
    WorldCollisionGridFace leftFace = {
        { (faceIndex + 1) * 4, (faceIndex + 1) * 4 + 1, (faceIndex + 1) * 4 + 2, (faceIndex + 1) * 4 + 3 }, faceIndex + 1, ACTOR_403200_ANGLED_WALL_SURFACE_CLASS
    };
    SVECTOR* halfWidthPointer;
    SVECTOR* secondNormal;

    // Scale the local axes into the forward and lateral wall offsets.
    gfxReadMatrixZAxis(&host->extra.tmd->coords->coord, &forwardOffset);
    gfxReadMatrixXAxis(&host->extra.tmd->coords->coord, &halfWidthOffset);
    halfWidthPointer = &halfWidthOffset;
    VectorNormalSS(halfWidthPointer, halfWidthPointer);
    VectorNormalSS(&forwardOffset, &forwardOffset);
    gte_lddp(distance);
    gte_ldsv(&forwardOffset);
    gte_gpf12();
    gte_stsv(&forwardOffset);
    gte_lddp(ACTOR_403200_ANGLED_WALL_HALF_WIDTH);
    gte_ldsv(halfWidthPointer);
    gte_gpf12();
    gte_stsv(halfWidthPointer);

    vertices[faceIndex * 4].vx = vertices[faceIndex * 4 + 2].vx =
        host->extra.tmd->coords->coord.t[0] + halfWidthOffset.vx + forwardOffset.vx;
    vertices[faceIndex * 4].vy = vertices[faceIndex * 4 + 2].vy =
        host->extra.tmd->coords->coord.t[1] + halfWidthOffset.vy + forwardOffset.vy;
    vertices[faceIndex * 4].vz = vertices[faceIndex * 4 + 2].vz =
        host->extra.tmd->coords->coord.t[2] + halfWidthOffset.vz + forwardOffset.vz;

    vertices[faceIndex * 4 + 1].vx = vertices[faceIndex * 4 + 3].vx =
        host->extra.tmd->coords->coord.t[0] + forwardOffset.vx;
    vertices[faceIndex * 4 + 1].vy = vertices[faceIndex * 4 + 3].vy =
        host->extra.tmd->coords->coord.t[1] + forwardOffset.vy;
    vertices[faceIndex * 4 + 1].vz = vertices[faceIndex * 4 + 3].vz =
        host->extra.tmd->coords->coord.t[2] + forwardOffset.vz;

    vertices[(faceIndex + 1) * 4].vx = vertices[(faceIndex + 1) * 4 + 2].vx =
        host->extra.tmd->coords->coord.t[0] + forwardOffset.vx;
    vertices[(faceIndex + 1) * 4].vy = vertices[(faceIndex + 1) * 4 + 2].vy =
        host->extra.tmd->coords->coord.t[1] + forwardOffset.vy;
    vertices[(faceIndex + 1) * 4].vz = vertices[(faceIndex + 1) * 4 + 2].vz =
        host->extra.tmd->coords->coord.t[2] + forwardOffset.vz;

    vertices[(faceIndex + 1) * 4 + 1].vx = vertices[(faceIndex + 1) * 4 + 3].vx =
        host->extra.tmd->coords->coord.t[0] + forwardOffset.vx - halfWidthOffset.vx;
    vertices[(faceIndex + 1) * 4 + 1].vy = vertices[(faceIndex + 1) * 4 + 3].vy =
        host->extra.tmd->coords->coord.t[1] + forwardOffset.vy - halfWidthOffset.vy;
    vertices[(faceIndex + 1) * 4 + 1].vz = vertices[(faceIndex + 1) * 4 + 3].vz =
        host->extra.tmd->coords->coord.t[2] + forwardOffset.vz - halfWidthOffset.vz;

    // Push the outside edges forward and derive a normal for each half.
    gte_lddp(ACTOR_403200_ANGLED_WALL_BEND_SCALE);
    gte_ldsv(&forwardOffset);
    gte_gpf12();
    gte_stsv(&forwardOffset);

    vertices[faceIndex * 4].vx = vertices[faceIndex * 4 + 2].vx += forwardOffset.vx;
    vertices[faceIndex * 4].vy = vertices[faceIndex * 4 + 2].vy += forwardOffset.vy;
    vertices[faceIndex * 4].vz = vertices[faceIndex * 4 + 2].vz += forwardOffset.vz;
    vertices[faceIndex * 4 + 5].vx = vertices[faceIndex * 4 + 7].vx += forwardOffset.vx;
    vertices[faceIndex * 4 + 5].vy = vertices[faceIndex * 4 + 7].vy += forwardOffset.vy;
    vertices[faceIndex * 4 + 5].vz = vertices[faceIndex * 4 + 7].vz += forwardOffset.vz;

    normals[faceIndex].vz = vertices[faceIndex * 4].vx - vertices[faceIndex * 4 + 1].vx;
    normals[faceIndex].vy = vertices[faceIndex * 4 + 1].vy - vertices[faceIndex * 4].vy;
    normals[faceIndex].vx = vertices[faceIndex * 4 + 1].vz - vertices[faceIndex * 4].vz;
    VectorNormalSS(&normals[faceIndex], &normals[faceIndex]);

    secondNormal     = &normals[faceIndex] + 1;
    secondNormal->vz = vertices[(faceIndex + 1) * 4].vx - vertices[(faceIndex + 1) * 4 + 1].vx;
    secondNormal->vy = vertices[(faceIndex + 1) * 4 + 1].vy - vertices[(faceIndex + 1) * 4].vy;
    secondNormal->vx = vertices[(faceIndex + 1) * 4 + 1].vz - vertices[(faceIndex + 1) * 4].vz;
    VectorNormalSS(secondNormal, secondNormal);

    // Extend the lower edge without changing the cell lists.
    vertices[faceIndex * 4].vy     -= lowerEdgeDrop;
    vertices[faceIndex * 4 + 1].vy -= lowerEdgeDrop;
    vertices[faceIndex * 4 + 4].vy -= lowerEdgeDrop;
    vertices[faceIndex * 4 + 5].vy -= lowerEdgeDrop;

    rightFace.surfaceClass = ACTOR_403200_ANGLED_WALL_SURFACE_CLASS;
    leftFace.surfaceClass  = ACTOR_403200_ANGLED_WALL_SURFACE_CLASS;
    faces[faceIndex]       = rightFace;
    faces[faceIndex + 1]   = leftFace;
}

/// Dispatches the dumping-hole host task after maintaining its moving barrier.
///
/// With initialized work, forgets dead summons, moves wallDistance toward its
/// target by 50 room units per tick and rebuilds the two angled wall faces.
/// Clamps player X to at least host X + wallDistance before dispatching the
/// spawn, frame or teardown handler selected by Task::state (0..2). The first
/// spawn tick has no work and skips this upkeep. Requires a live player model
/// and the dumping-hole grid while work exists; its enemy handle is borrowed.
static void _actor403200HostTask(Task* host)
{
    enum { ACTOR_403200_WALL_DISTANCE_STEP = 50 };
    EnemyTaskFuncTable3 taskStates;
    GluttonWork*        work;
    Task*               player;
    Enemy*              enemy;
    s32                 distanceDelta;
    s32                 minimumPlayerX;
    GfxCoord*           playerCoord;
    GfxCoord*           hostRoot;

    taskStates = D_actor_403200_801321B8;
    player     = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    work       = host->work;
    enemy      = host->spawnArg2.pointer;
    if (work != NULL) {
        if (work->summons[0] != NULL && work->summons[0]->hp <= 0) {
            work->summons[0] = NULL;
        }
        if (work->summons[1] != NULL && work->summons[1]->hp <= 0) {
            work->summons[1] = NULL;
        }
        distanceDelta = work->wallDistanceTarget - work->wallDistance;
        if (distanceDelta < 0) {
            distanceDelta = -distanceDelta;
        }
        if (distanceDelta >= ACTOR_403200_WALL_DISTANCE_STEP + 1) {
            if (work->wallDistance < work->wallDistanceTarget) {
                work->wallDistance = (u16)work->wallDistance + ACTOR_403200_WALL_DISTANCE_STEP;
            } else {
                work->wallDistance = (u16)work->wallDistance - ACTOR_403200_WALL_DISTANCE_STEP;
            }
        } else {
            work->wallDistance = (u16)work->wallDistanceTarget;
        }
        // Keep the collision wall and the player's minimum lead together.
        _actor403200BuildAngledWall(host, work->wallDistance, work->wallDrop, 0);
        playerCoord    = player->extra.tmd->coords;
        hostRoot       = host->extra.tmd->coords;
        minimumPlayerX = hostRoot->coord.t[0] + work->wallDistance;
        if (playerCoord->coord.t[0] < minimumPlayerX) {
            playerCoord->coord.t[0] = minimumPlayerX;
        }
    }
    taskStates.funcs[host->state](enemy, host);
}

#include "../../shared/glutton_quad_heights.inc.c"

#include "../../shared/glutton_exit.inc.c"

#include "../../shared/glutton_shake_level.inc.c"

#include "../../shared/glutton_set_spinners_released.inc.c"

#include "../../shared/glutton_get_spinners_released.inc.c"

/// Returns the limb-swipe camera view for a fight phase.
///
/// Phases 0, 1 and 2 select views 19, 7 and 37; other values return view 1.
/// The host task is unused. This is selector 6 in the host's view table.
static s32 _actor403200PickSwipeView(Task* task, s16 phase)
{
    enum {
        ACTOR_403200_SWIPE_PHASE0_VIEW   = 19,
        ACTOR_403200_SWIPE_PHASE1_VIEW   = 7,
        ACTOR_403200_SWIPE_PHASE2_VIEW   = 37,
        ACTOR_403200_SWIPE_FALLBACK_VIEW = 1,
    };
    switch (phase) {
        case 0:
            return ACTOR_403200_SWIPE_PHASE0_VIEW;
        case 1:
            return ACTOR_403200_SWIPE_PHASE1_VIEW;
        case 2:
            return ACTOR_403200_SWIPE_PHASE2_VIEW;
    }
    return ACTOR_403200_SWIPE_FALLBACK_VIEW;
}

/// Selects the default combat camera for the host's current fight phase.
///
/// Selector 0 forwards the combat selector through signed-halfword narrowing,
/// then widens the view index to the callback's s32 result. It shares that
/// selector's live-model and coordinate requirements.
static s32 _actor403200PickDefaultView(Task* host, s16 phase)
{
    return (s16)_actor403200PickCombatView(host, phase);
}

/// Selects views 37/30 from the player's X position, with hysteresis.
///
/// Selector 7 ignores host and phase. It switches to view 30 at X >= 16000,
/// and back to view 37 below X 15000 while view 30 is current. Requires the
/// live player's model root; X uses its parent frame's coordinate units.
static s32 _actor403200PickPlayerXView(Task* unusedHost, s16 unusedPhase)
{
    Task* player;
    s32   xTest;
    s32   nextView;
    s32   currentView;

    currentView = viewGetMappedIndex() & ACTOR_403200_VIEW_INDEX_MASK;
    player      = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (currentView == ACTOR_403200_VIEW_FAR_X) {
        xTest    = player->extra.tmd->coords->coord.t[0];
        xTest    = xTest < 0x3A98;
        nextView = ACTOR_403200_VIEW_LATE_NEAR;
    } else {
        xTest    = player->extra.tmd->coords->coord.t[0];
        xTest    = xTest < 0x3E80;
        nextView = ACTOR_403200_VIEW_LATE_NEAR;
    }
    if (xTest == 0) {
        nextView = ACTOR_403200_VIEW_FAR_X;
    }
    return nextView;
}

/// Empty handler for the host's state-table slot 16.
///
/// No transition into this slot is established; the task argument is unused.
static void _actor403200NoopState16(Task* task)
{
}

/// Empty handler for the host's state-table slot 17.
///
/// No transition into this slot is established; the task argument is unused.
static void _actor403200NoopState17(Task* task)
{
}

/// Advances the death pose and notifies the room eight ticks after state entry.
///
/// State-table slot 18 resets its timer on entry, then ticks animation. At tick
/// 8 it sends ROOM_MESSAGE_ACTOR_EVENT with zero payloads to the room task and
/// stops the host's cue 10. Requires live work, enemy, rigs and room task.
static void _actor403200DeathHandoffState(Task* task)
{
    enum {
        ACTOR_403200_DEATH_HANDOFF_TICK = 8,
    };
    GluttonWork* work;
    Enemy*       enemy;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateChanged != 0) {
        work->stateTicks = 0;
    }
    _gluttonTickAnim(task);
    if (work->stateTicks == ACTOR_403200_DEATH_HANDOFF_TICK) {
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, 0, 0);
        sndEvtRequestScriptStop(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0A), SOUND_SCRIPT_STOP_KEEP_RELEASE);
    }
}

#include "../../shared/glutton_prop_setup.inc.c"

#include "../../shared/glutton_prop_tick.inc.c"

/// Selects the escort-model task callback instantiated by the shared fragment.
///
/// Must name a previously declared `void (Task*)` callback; its declaration
/// supplies the linkage. Bind around each inclusion, then undefine it. The
/// first private instance serves descriptor slots 0..5; the second serves slot 6.
#define GLUTTON_PROP_TASK _gluttonPropTask
#include "../../shared/glutton_prop_task.inc.c"
#undef GLUTTON_PROP_TASK

#define GLUTTON_PROP_TASK _gluttonEscort6Task
#include "../../shared/glutton_prop_task.inc.c"
#undef GLUTTON_PROP_TASK

#include "../../shared/glutton_throw_task.inc.c"

#include "../../shared/glutton_glob_task.inc.c"

#include "../../shared/glutton_chunk_task.inc.c"

#include "../../shared/glutton_rain_task.inc.c"

#include "../../shared/glutton_spinner_wait.inc.c"

#include "../../shared/glutton_spinner_task.inc.c"

/// Empty callback installed for the boss's message 2015.
///
/// The message's wider purpose is unproven. All arguments are ignored, and the
/// binary supplies no defined return value; senders must discard the result.
static void _actor403200IgnoreMessage2015(Task* task, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
}

#include "../../shared/actor_messages_is_present.inc.c"

#include "../../shared/actor_messages_place.inc.c"

/// Credits healing or records the end of a summoned enemy.
///
/// Event 0 increments pendingHeals, posts a -100 HP readout and adds 100 HP only
/// while the host is alive, without clamping. Event 1 decrements a positive
/// summonsAlive count and arms a two-tick death delay once none remain (also
/// when already zero). Requires live host work/enemy. Other events do nothing.
/// Returns 1; the message ID and second argument are ignored.
static s32 _actor403200HandleActorEvent(Task* task, s32 unusedMessageId, s32 event, s32 unusedSecondArg)
{
    enum {
        ACTOR_403200_EVENT_HEAL                    = 0,
        ACTOR_403200_EVENT_SUMMON_ENDED            = 1,
        ACTOR_403200_HEAL_HP                       = 100,
        ACTOR_403200_LAST_SUMMON_DEATH_DELAY_TICKS = 2,
    };
    GluttonWork* work  = task->work;
    Enemy*       enemy = task->spawnArg2.pointer;

    switch (event) {
        case ACTOR_403200_EVENT_HEAL:
            work->pendingHeals++;
            worldTargetAddReadoutAmount(&enemy->node, -ACTOR_403200_HEAL_HP, 0);
            if (enemy->hp > 0) {
                enemy->hp += ACTOR_403200_HEAL_HP;
            }
            break;
        case ACTOR_403200_EVENT_SUMMON_ENDED:
            if (work->summonsAlive > 0) {
                work->summonsAlive--;
                if (work->summonsAlive > 0) {
                    break;
                }
            }
            work->deathDelay = ACTOR_403200_LAST_SUMMON_DEATH_DELAY_TICKS;
            break;
    }
    return 1;
}

/// Clears the shared glob-grab latch so an engulfing glob can release the player.
///
/// Installed for message 2014. Returns 1 and ignores every message argument;
/// the engulfing projectile performs its own subsequent release/teardown.
static s32 _actor403200ReleaseGlobGrab(Task* task, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    gGluttonGrabActive = 0;
    return 1;
}

/// Shows the host and live escorts with neck tracking disabled, then ticks animation.
///
/// Entry clears model flags and the buffer-free countdown without allocating
/// buffers. Requires live host work/models and initialized animation rigs.
static void _actor403200ShownState(Task* task)
{
    GluttonWork* work;
    GluttonWork* flagWork;
    s16          flagIndex;

    work = task->work;
    if (work->stateChanged != 0) {
        flagWork               = task->work;
        work->freeCountdown    = 0;
        task->extra.tmd->flags = 0;
        flagIndex              = 0;
        ACTOR_403200_COPY_ESCORT_MODEL_FLAGS(task, flagWork, flagIndex);
        work->neckPitchEnabled = 0;
        work->neckYawEnabled   = 0;
    } else {
        _gluttonTickAnim(task);
    }
}
