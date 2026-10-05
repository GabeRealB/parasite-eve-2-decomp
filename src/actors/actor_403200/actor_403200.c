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
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
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
/// Selects dumping-hole behavior for this compiled Glutton instance.
///
/// Define before `glutton.h` and retain through every shared fragment. The
/// header defines `GLUTTON_DUMPING_HOLE` as the dimensionless integer 1;
/// the binding must remain a macro for the shared code's `#if` comparisons.
#define GLUTTON_ROOM GLUTTON_DUMPING_HOLE
#include "../../shared/glutton.h"

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

/// Exit callback of the boss task, installed by its spawn state.

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).

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
/// getter pair `gluttonSetSpinnersReleased` and `gluttonGetSpinnersReleased`;
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

/// Enemy spawn table the three launch states of `func_actor_403200_8013D9EC`
/// draw from.
extern TaskDesc gGluttonEscortTasks[];

/// The enemy task's message-handler table, parked in `Task::msgTable`.
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_403200_8015F770[8];

/// Three formations of nine positions, and each member's model/spawn argument.
extern SVECTOR                  D_actor_403200_8015F7B0[3][9];
extern _Actor403200SpinnerSpawn D_actor_403200_8015F888[9];

/// Non-zero once the launch state has published the enemy's position to the
/// player, and cleared again when it restarts.

/// Shared 0x7DA payload buffer.
extern ActorCommand D_actor_403200_8015F8F4;

/// View-space point the launch tick clears and fills from the host's fourth
/// model part on a state change; the spinner enemies home on it.
extern SVECTOR gGluttonSpinnerTarget;

/// Button-press hold the glob's engulf state sends the player, asking for 40
/// presses.
extern GluttonButtonPressHoldStorage gGluttonGrabQuery;

/// The scratch coordinate the debris effect of `func_actor_403200_8013DC3C` is
/// built on: `F920` is the whole `GfxCoord` and `F924` its `coord` matrix,
/// which splat names separately because the code takes that address directly.
extern GfxCoord D_actor_403200_8015F920;

extern GluttonCoord D_actor_403200_8015F970;

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

/// Handwritten overlay-local follow helper. `arg1`/`arg2` select the axis pair
/// and `arg3` the mode; takes the task, not the work block.
static void func_actor_403200_801408D8(Task* arg0, s16 arg1, s16 arg2, s16 arg3);

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
s32                 func_actor_403200_801341E8(Task*, s16);
s32                 func_actor_403200_80134374(Task*, s16);
s32                 func_actor_403200_801344C4(Task*, s16);
s32                 func_actor_403200_80134748(Task*, s16);
s32                 func_actor_403200_80134900(Task*, s16);
s32                 func_actor_403200_80134A14(Task*, s16);
s32                 func_actor_403200_80141124(Task*, s16);
s32                 func_actor_403200_80141180(Task*, s16);
s32                 func_actor_403200_801411A8(Task*, s16);
void                func_actor_403200_8014148C(Task*);

s32  func_actor_403200_80138468(Task*, s32, s32, s32);
s32  func_actor_403200_80138748(Task* task, s32 msgId, ActorCommand* msg, s32 arg3);
s32  func_actor_403200_80141A94(Task*, s32, s32, s32);
s32  func_actor_403200_80141B30(Task*, s32, s32, s32);
void func_actor_403200_80140E6C(Task*);
s32  func_actor_403200_8014196C(Task*, s32, s32, s32);

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
    func_actor_403200_80141180,
    func_actor_403200_80134748,
    func_actor_403200_801344C4,
    func_actor_403200_801341E8,
    func_actor_403200_80134900,
    func_actor_403200_80134374,
    func_actor_403200_80141124,
    func_actor_403200_801411A8,
    func_actor_403200_80134A14,
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
    { { { TASK_BODY_TMD, 96 } }, gluttonPropTask, { .model = &_gActor403200GluttonLegRight } },
    { { { TASK_BODY_TMD, 96 } }, gluttonPropTask, { .model = &_gActor403200GluttonLegLeft } },
    { { { TASK_BODY_TMD, 96 } }, gluttonPropTask, { .model = &_gActor403200Model12884 } },
    { { { TASK_BODY_TMD, 96 } }, gluttonPropTask, { .model = &_gActor403200Model13774 } },
    { { { TASK_BODY_TMD, 96 } }, gluttonPropTask, { .model = &_gActor403200Model1785C } },
    { { { TASK_BODY_TMD, 96 } }, gluttonPropTask, { .model = &_gActor403200Model18BE4 } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_403200_8014148C, { .model = &_gActor403200Model186D8 } },
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
    { { { TASK_BODY_TMD, 96 } }, gluttonGlobTask, { .model = &_gActor403200Model199E4 } },
    { { { TASK_BODY_COORD, 96 } }, gluttonRainTask, { .value = 0 } },
    { { { TASK_BODY_COORD, 96 } }, gluttonThrowTask, { .value = 0 } },
    { { { TASK_BODY_TMD, 96 } }, gluttonChunkTask, { .model = &_gActor403200Model1AC48 } },
    { { { TASK_BODY_TMD, 96 } }, gluttonSpinnerTask, { .model = &_gActor403200Model19284 } },
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
    { 2015, func_actor_403200_8014196C },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_403200_80138468 },
    { ACTOR_MESSAGE_IS_PRESENT, actorMsgIsPresent },
    { ACTOR_MESSAGE_PLACE, actorMsgPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_403200_80138748 },
    { ROOM_MESSAGE_ACTOR_EVENT, func_actor_403200_80141A94 },
    { 2014, func_actor_403200_80141B30 },
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
TaskDesc D_actor_403200_8015F8D0 = { { { TASK_BODY_TMD, 96 } }, func_actor_403200_80140E6C, { .model = &_gActor403200Model10824 } };

/// Four zero bytes between the descriptor and the next object. Nothing refers
/// to them; what they were is not established.
static u8 _actor403200Unreferenced8015F8DC[4] = { 0, 0, 0, 0 };

// Retain seven zero bytes after the accessed state byte.
// Their original role as spare storage or alignment remains unresolved.
s8 D_actor_403200_8015F8E0[8] = { 0 };

SVECTOR ActorContact_ScratchPosition = { 0, 0, 0, 0 };

/// The contact routines' scratch position.
static inline SVECTOR* ActorContact_GetScratchPosition(void)
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

GluttonCoord D_actor_403200_8015F970 = { .node = { 0, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } };

_Actor403200PlayerPlacementStorage D_actor_403200_8015F9C0;

GameActorButtonPressHold D_actor_403200_8015FA00;

/// Integer part of the last step `ActorContact_PushContact` applied.
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

static void func_actor_403200_8013FB54(Enemy* arg0, Task* arg1);

static void func_actor_403200_80141B40(Task* arg0);

static void func_actor_403200_8014122C(Task* arg0);

static void func_actor_403200_80141234(Task* arg0);

static void func_actor_403200_8014123C(Task* arg0);

static __inline__ void Actor403200_StepForward(GfxCoord* coord);
static __inline__ void Actor403200_SeedRootCoord(Task* task, GluttonWork* work);
static void            func_actor_403200_80134D40(Task* arg0);
static void            func_actor_403200_80138AFC(Enemy* enemy, Task* task);
static void            func_actor_403200_8013A4A0(Task* arg0);
static void            func_actor_403200_8013B23C(Task* arg0);
static void            func_actor_403200_8013B3C8(Task* arg0);
static void            func_actor_403200_8013B740(Task* arg0);
static void            func_actor_403200_8013B8C4(Task* arg0);
static void            func_actor_403200_8013C84C(Task* arg0);
static void            func_actor_403200_8013D028(Task* arg0);
static void            func_actor_403200_8013D78C(Task* arg0);
static void            func_actor_403200_8013D9EC(Task* arg0);
static void            func_actor_403200_8013DC3C(Task* arg0);
static void            func_actor_403200_8013E2FC(Task* arg0);
static void            func_actor_403200_8013E5A8(Task* arg0);
static void            func_actor_403200_8013E9C0(Task* arg0);
static void            func_actor_403200_8013EB64(Task* arg0);
static void            func_actor_403200_8013EF6C(Task* arg0);

/// Walk `coord` 0x19/0x1000 of the way along its own forward axis (column 2 of
/// its rotation, normalised and GPF-scaled) and flag it for rebuild. The
/// direction vector lives in an `SVECTOR` carved off the scratch head and
/// handed straight back.
static __inline__ void Actor403200_StepForward(GfxCoord* coord)
{
    u8*      head;
    SVECTOR* dir;

    head                       = SCRATCH_STACK_CURSOR(u8);
    dir                        = (SVECTOR*)(head - sizeof(SVECTOR));
    SCRATCH_STACK_CURSOR(void) = dir;

    gfxReadMatrixZAxis(&coord->coord, dir);
    VectorNormalSS(dir, dir);
    gte_lddp(0x19);
    gte_ldsv(dir);
    gte_gpf12();
    gte_stsv(dir);

    coord->coord.t[0]  += dir->vx;
    coord->coord.t[1]  += dir->vy;
    coord->coord.t[2]  += dir->vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;

    SCRATCH_STACK_RELEASE_BYTES(sizeof(SVECTOR));
}

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).

#include "../../shared/glutton_inlines.inc.c"

static __inline__ void Actor403200_SeedRootCoord(Task* task, GluttonWork* work)
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

    work->state            = 0;
    task->extra.tmd->flags = 0;
    SCRATCH_STACK_RELEASE_BYTES(sizeof(ActorScaleRotScratch));
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

s32 func_actor_403200_801341E8(Task* arg0, s16 arg1)
{
    SVECTOR   vec;
    SVECTOR*  vp;
    GfxCoord* coords;
    s32       dist;
    s32       value;
    s32       view;
    s32       flag;

    view   = Gp_GetViewIndex() & 0xFF;
    vp     = &vec;
    coords = arg0->extra.tmd->coords;
    vp->vx = gPlayerStatus.coordMtx->t[0] - coords->coord.t[0];
    vp->vy = gPlayerStatus.coordMtx->t[1] - coords->coord.t[1];
    dist   = vec.vx * vec.vx;
    vp->vz = gPlayerStatus.coordMtx->t[2] - coords->coord.t[2];
    dist  += vec.vy * vec.vy;
    dist   = SquareRoot0(dist + (vec.vz * vec.vz));
    switch (arg1) {
        case 0:
            if (view == 0x21) {
                value = 0x20;
                flag  = dist < 0x189D;
                if (flag) {
                    value = 0x21;
                }
                return value;
            }
            value = 0x21;
            flag  = dist < 0x1770;
            if (!flag) {
                value = 0x20;
            }
            return value;
        case 1:
            if ((view != 9) && (view != 10)) {
                value = 9;
                flag  = dist < 0x27D8;
            } else {
                flag = view;
                if (flag == 9) {
                    value = 0xA;
                    flag  = dist < 0x27D9;
                    if (flag) {
                        value = 9;
                    }
                    return value;
                }
                if (flag == 10) {
                    value = 9;
                    flag  = dist < 0x24EA;
                } else {
                    return 1;
                }
            }
            if (!flag) {
                value = 0xA;
            }
            return value;
        case 2:
            return 0x1B;
    }
    return 1;
}

s32 func_actor_403200_80134374(Task* arg0, s16 arg1)
{
    SVECTOR   vec;
    SVECTOR*  vp;
    GfxCoord* coords;
    s32       dist;
    s32       value;
    s32       view;
    s32       flag;

    view   = Gp_GetViewIndex() & 0xFF;
    vp     = &vec;
    coords = arg0->extra.tmd->coords;
    vp->vx = gPlayerStatus.coordMtx->t[0] - coords->coord.t[0];
    vp->vy = gPlayerStatus.coordMtx->t[1] - coords->coord.t[1];
    dist   = vec.vx * vec.vx;
    vp->vz = gPlayerStatus.coordMtx->t[2] - coords->coord.t[2];
    dist  += vec.vy * vec.vy;
    dist   = SquareRoot0(dist + (vec.vz * vec.vz));
    switch (arg1) {
        case 0:
        case 1:
            if ((view != 7) && (view != 8)) {
                value = 7;
                flag  = dist < 0x26AC;
            } else {
                flag = view;
                if (flag == 7) {
                    value = 8;
                    flag  = dist < 0x26AD;
                    if (flag) {
                        value = 7;
                    }
                    return value;
                }
                if (flag == 8) {
                    value = 7;
                    flag  = dist < 0x2328;
                } else {
                    return 1;
                }
            }
            if (!flag) {
                value = 8;
            }
            return value;
        case 2:
            return 0x1A;
    }
    return 1;
}

s32 func_actor_403200_801344C4(Task* arg0, s16 arg1)
{
    SVECTOR   vec;
    SVECTOR*  vp;
    GfxCoord* coords;
    Task*     task;
    s32       dist;
    s32       value;
    s32       view;
    s32       flag;

    view   = Gp_GetViewIndex() & 0xFF;
    task   = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    vp     = &vec;
    coords = arg0->extra.tmd->coords;
    vp->vx = gPlayerStatus.coordMtx->t[0] - coords->coord.t[0];
    vp->vy = gPlayerStatus.coordMtx->t[1] - coords->coord.t[1];
    dist   = vec.vx * vec.vx;
    vp->vz = gPlayerStatus.coordMtx->t[2] - coords->coord.t[2];
    dist  += vec.vy * vec.vy;
    dist   = SquareRoot0(dist + (vec.vz * vec.vz));
    switch (arg1) {
        case 0:
            if ((view != 2) && (view != 3) && (view != 4)) {
                if (dist < 0x2261) {
                    return 3;
                }
                value = 2;
                flag  = dist < 0x2FA8;
                if (!flag) {
                    value = 4;
                }
                goto done;
            } else {
                flag = view;
                if (flag == 3) {
                    value = 2;
                    flag  = dist < 0x2262;
                    if (flag) {
                        value = 3;
                    }
                    goto done;
                }
                if (flag == 2) {
                    if (dist < 0x1E14) {
                        return 3;
                    }
                    flag = dist < 0x2FA9;
                    if (flag) {
                        value = 2;
                    } else {
                        value = 4;
                    }
                    goto done;
                }
                if (flag == 4) {
                    value = 2;
                    flag  = dist < 0x2E18;
                    if (!flag) {
                        value = 4;
                    }
                    goto done;
                }
                value = 1;
                goto done;
            }
        case 1:
            flag = view;
            if ((flag != 0x22) && (flag != 4)) {
                value = 0x22;
                flag  = dist < 0x2455;
            } else {
                if (flag == 0x22) {
                    value = 4;
                    flag  = dist < 0x2456;
                    if (flag) {
                        value = 0x22;
                    }
                    goto done;
                }
                if (flag == 4) {
                    value = 0x22;
                    flag  = dist < 0x2260;
                } else {
                    value = 1;
                    goto done;
                }
            }
            if (!flag) {
                value = 4;
            }
            goto done;
        case 2:
            flag = view;
            if ((flag != 0x25) && (flag != 0x19)) {
                value = 0x25;
                flag  = dist < 0x1E5A;
            } else {
                if (flag == 0x25) {
                    value = 0x19;
                    flag  = dist < 0x1E5B;
                    if (flag) {
                        value = 0x25;
                    }
                    goto done;
                }
                if (flag == 0x19) {
                    value = 0x25;
                    flag  = dist < 0x1B58;
                } else {
                    value = 1;
                    goto done;
                }
            }
            if (!flag) {
                value = 0x19;
            }
            goto done;
        case 3:
            flag = view;
            if ((flag != 0x25) && (flag != 0x1E)) {
                flag  = task->extra.tmd->coords->coord.t[0];
                value = 0x25;
                flag  = flag < 0x4268;
            } else {
                if (flag == 0x25) {
                    flag  = task->extra.tmd->coords->coord.t[0];
                    value = 0x1E;
                    flag  = flag < 0x4651;
                    if (flag) {
                        value = 0x25;
                    }
                    goto done;
                }
                if (flag == 0x1E) {
                    flag  = task->extra.tmd->coords->coord.t[0];
                    value = 0x25;
                    flag  = flag < 0x4268;
                } else {
                    value = 1;
                    goto done;
                }
            }
            if (!flag) {
                value = 0x1E;
            }
            goto done;
    }
    value = 1;
done:
    return value;
}

s32 func_actor_403200_80134748(Task* arg0, s16 arg1)
{
    SVECTOR   vec;
    SVECTOR*  vp;
    GfxCoord* coords;
    s32       dist;
    s32       value;
    s32       view;
    s32       flag;

    view   = Gp_GetViewIndex() & 0xFF;
    vp     = &vec;
    coords = arg0->extra.tmd->coords;
    vp->vx = gPlayerStatus.coordMtx->t[0] - coords->coord.t[0];
    vp->vy = gPlayerStatus.coordMtx->t[1] - coords->coord.t[1];
    dist   = vec.vx * vec.vx;
    vp->vz = gPlayerStatus.coordMtx->t[2] - coords->coord.t[2];
    dist  += vec.vy * vec.vy;
    dist   = SquareRoot0(dist + (vec.vz * vec.vz));
    switch (arg1) {
        case 0:
            if ((view != 5) && (view != 6)) {
                value = 5;
                flag  = dist < 0x238C;
            } else {
                flag = view;
                if (flag == 5) {
                    value = 6;
                    flag  = dist < 0x238D;
                    if (flag) {
                        value = 5;
                    }
                    return value;
                }
                if (flag == 6) {
                    value = 5;
                    flag  = dist < 0x2198;
                } else {
                    return 1;
                }
            }
            if (!flag) {
                value = 6;
            }
            return value;
        case 1:
            if ((view != 0xB) && (view != 0xC)) {
                value = 0xB;
                flag  = dist < 0x238C;
            } else {
                flag = view;
                if (flag == 0xB) {
                    value = 0xC;
                    flag  = dist < 0x238D;
                    if (flag) {
                        value = 0xB;
                    }
                    return value;
                }
                if (flag == 0xC) {
                    value = 0xB;
                    flag  = dist < 0x1A90;
                } else {
                    return 1;
                }
            }
            if (!flag) {
                value = 0xC;
            }
            return value;
        case 2:
            return 0x1C;
    }
    return 1;
}

s32 func_actor_403200_80134900(Task* arg0, s16 arg1)
{
    SVECTOR   pos;
    SVECTOR*  p;
    GfxCoord* coords;
    s32       dist;
    s32       flag;
    s32       value;
    s32       view;

    view   = Gp_GetViewIndex() & 0xFF;
    p      = &pos;
    coords = arg0->extra.tmd->coords;
    p->vx  = gPlayerStatus.coordMtx->t[0] - coords->coord.t[0];
    p->vy  = gPlayerStatus.coordMtx->t[1] - coords->coord.t[1];
    dist   = pos.vx * pos.vx;
    p->vz  = gPlayerStatus.coordMtx->t[2] - coords->coord.t[2];
    dist  += pos.vy * pos.vy;
    dist   = SquareRoot0(dist + (pos.vz * pos.vz));
    switch (arg1) {
        default:
            return 1;
        case 0:
        case 1:
        case 2:
            flag = view;
            if ((flag != 0x25) && (flag != 0x19)) {
                value = 0x25;
                flag  = dist < 0x1E5A;
                if (!flag) {
                    value = 0x19;
                }
                return value;
            } else if (flag == 0x25) {
                value = 0x25;
                flag  = dist < 0x1E5A;
                if (!flag) {
                    value = 0x19;
                }
                return value;
            } else {
                value = 0x25;
                flag  = dist < 0x1B58;
                if (!flag) {
                    value = 0x19;
                }
                return value;
            }
    }
}

/// Reference positions the view selector below measures the player against.
static const _Actor403200ViewAnchors D_actor_403200_80131E64 = {
    {
        { 0x10B4, 1, -0x17DD, 0 },
        { 0x1CD0, 1, -0x17DD, 0 },
        { 0x2888, 1, -0x17DD, 0 },
        { 0x2888, 1, -0x17DD, 0 },
    },
};

/// State handlers of the escort model task `gluttonPropTask` and
/// `func_actor_403200_8014148C` dispatch: texture setup, coordinate refresh,
/// teardown.
static const EnemyTaskFuncTable3 gGluttonPropStates = {
    {
        gluttonPropSetup,
        gluttonPropTick,
        enemyDestroy,
    },
};

s32 func_actor_403200_80134A14(Task* arg0, s16 arg1)
{
    SVECTOR                 vec;
    _Actor403200ViewAnchors anchors;
    Task*                   obj;
    s32                     dist;
    s32                     value;
    s32                     view;
    s32                     flag;

    view    = Gp_GetViewIndex() & 0xFF;
    obj     = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    anchors = D_actor_403200_80131E64;
    vec.vx  = obj->extra.tmd->coords->coord.t[0] - anchors.points[arg1].vx;
    dist    = vec.vx * vec.vx;
    vec.vy  = obj->extra.tmd->coords->coord.t[1] - anchors.points[arg1].vy;
    dist   += vec.vy * vec.vy;
    vec.vz  = obj->extra.tmd->coords->coord.t[2] - anchors.points[arg1].vz;
    dist    = SquareRoot0(dist + (vec.vz * vec.vz));
    switch (arg1) {
        case 0:
            if ((view != 2) && (view != 3) && (view != 4)) {
                if (dist < 0x2261) {
                    return 3;
                }
                value = 2;
                flag  = dist < 0x2FA8;
                if (!flag) {
                    value = 4;
                }
                goto done;
            } else {
                flag = view;
                if (flag == 3) {
                    value = 2;
                    flag  = dist < 0x2262;
                    if (flag) {
                        value = 3;
                    }
                    goto done;
                }
                if (flag == 2) {
                    if (dist < 0x1E14) {
                        return 3;
                    }
                    flag = dist < 0x2FA9;
                    if (flag) {
                        value = 2;
                    } else {
                        value = 4;
                    }
                    goto done;
                }
                if (flag == 4) {
                    value = 2;
                    flag  = dist < 0x2E18;
                    if (!flag) {
                        value = 4;
                    }
                    goto done;
                }
                value = 1;
                goto done;
            }
        case 1:
            flag = view;
            if ((flag != 0x22) && (flag != 4)) {
                value = 0x22;
                flag  = dist < 0x2455;
            } else {
                if (flag == 0x22) {
                    value = 4;
                    flag  = dist < 0x2456;
                    if (flag) {
                        value = 0x22;
                    }
                    goto done;
                }
                if (flag == 4) {
                    value = 0x22;
                    flag  = dist < 0x2260;
                } else {
                    value = 1;
                    goto done;
                }
            }
            if (!flag) {
                value = 4;
            }
            goto done;
        case 2:
            flag = view;
            if ((flag != 0x25) && (flag != 0x19)) {
                value = 0x25;
                flag  = dist < 0x1E5A;
            } else {
                if (flag == 0x25) {
                    value = 0x19;
                    flag  = dist < 0x1E5B;
                    if (flag) {
                        value = 0x25;
                    }
                    goto done;
                }
                if (flag == 0x19) {
                    value = 0x25;
                    flag  = dist < 0x1B58;
                } else {
                    value = 1;
                    goto done;
                }
            }
            if (!flag) {
                value = 0x19;
            }
            goto done;
        case 3:
            flag = view;
            if ((flag != 0x25) && (flag != 0x1E)) {
                flag  = obj->extra.tmd->coords->coord.t[0];
                value = 0x25;
                flag  = flag < 0x4268;
            } else {
                if (flag == 0x25) {
                    flag  = obj->extra.tmd->coords->coord.t[0];
                    value = 0x1E;
                    flag  = flag < 0x4651;
                    if (flag) {
                        value = 0x25;
                    }
                    goto done;
                }
                if (flag == 0x1E) {
                    flag  = obj->extra.tmd->coords->coord.t[0];
                    value = 0x25;
                    flag  = flag < 0x4268;
                } else {
                    value = 1;
                    goto done;
                }
            }
            if (!flag) {
                value = 0x1E;
            }
            goto done;
    }
    value = 1;
done:
    return value;
}

/// The enemy's walk-out state: a reset request re-arms the block (the two
/// neck flags, the animation step, `hostExposed`, clip 2 and the
/// `wallDistanceTarget`), then the per-frame body runs and the animation frame the
/// mask leaves is tested against 0x12 and 0x18 -- each one-shot cue spawning a
/// script and a type-6 sound with the enemy's pan and half its depth, once per
/// arrival -- before being latched into `clip.prevSlot2Cue`. Unless the game is frozen
/// the model is then stepped 0x19/0x1000 forward along its own facing, its
/// `composeStamp` cleared, and once it has run out to x 0x1CCA in state 0 or 0x2882 in
/// state 1 the step advances and re-arms `state`.
static void func_actor_403200_80134D40(Task* arg0)
{
    GluttonWork* work;
    Enemy*       enemy;
    GfxCoord*    model;
    s16          frame;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;

    if (work->stateChanged != 0) {
        work->neckPitchEnabled   = 1;
        work->neckYawEnabled     = 1;
        work->hostExposed        = 0;
        work->animId             = 2;
        work->animStep           = GLUTTON_ANIM_STEP_BLEND;
        work->neckPitchTarget    = 0;
        work->wallDistanceTarget = 0xE74;
    }

    SCRATCH_STACK_RESERVE_BYTES(0xC);
    gluttonTickAnim(arg0);

    frame = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    if (frame == 0x12 && work->clip.prevSlot2Cue != frame) {
        s32 id;
        s32 pan;

        work->shakeLevel = GLUTTON_SHAKE_LONG;
        Gp_SpawnScript18(D_actor_403200_80141C5C, D_actor_403200_80141C64);
        id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200001;
        pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(id, pan,
                                 (s8)(worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords) / 2));
    }

    frame = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    if (frame == 0x18 && work->clip.prevSlot2Cue != frame) {
        s32 id;
        s32 pan;

        work->shakeLevel = GLUTTON_SHAKE_LONG;
        Gp_SpawnScript18(D_actor_403200_80141C5C, D_actor_403200_80141C64);
        id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200001;
        pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(id, pan,
                                 (s8)(worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords) / 2));
    }

    work->clip.prevSlot2Cue = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;

    model = arg0->extra.tmd->coords;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
        Actor403200_StepForward(model);
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;

    switch (work->phase) {
        case 0:
            if (arg0->extra.tmd->coords->coord.t[0] >= 0x1CCA) {
                work->phase++;
                work->state = 3;
            }
            break;
        case 1:
            if (arg0->extra.tmd->coords->coord.t[0] >= 0x2882) {
                work->phase++;
                work->state = 3;
            }
            break;
    }

    if (work->stateTicks > 0) {
        work->viewSelector = 8;
    }

    SCRATCH_STACK_RELEASE_BYTES(0xC);
}

#include "../../shared/glutton_throw_spawn.inc.c"

#include "../../shared/glutton_throw_fly.inc.c"

/// State handlers of the enemy stood up on the host's first escort: spawn,
/// flight, teardown.
static const EnemyTaskFuncTable3 gGluttonThrowStates = {
    {
        gluttonThrowSpawn,
        gluttonThrowFly,
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
        gluttonGlobSpawn,
        gluttonGlobFall,
        gluttonGlobEngulf,
        gluttonGlobHold,
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
        gluttonChunkSpawn,
        gluttonChunkFall,
        gluttonChunkSettle,
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
        gluttonRainSpawn,
        gluttonRainRise,
        gluttonRainFall,
        gluttonRainSplat,
        enemyDestroy,
    },
};

#include "../../shared/glutton_spinner_spawn.inc.c"

#include "../../shared/glutton_spinner_chase.inc.c"

/// State handlers of the spinner enemy: spawn, wait, home, teardown.
static const EnemyTaskFuncTable4 gGluttonSpinnerStates = {
    {
        gluttonSpinnerSpawn,
        gluttonSpinnerWait,
        gluttonSpinnerChase,
        enemyDestroy,
    },
};

#include "../../shared/glutton_shake_tick.inc.c"

/// The escort-group reset the enemy runs whenever its state changes: it turns
/// the host model's flag word around and pushes it onto all seven escorts'
/// models, differing in what the word becomes and whether the model buffers are
/// (re)allocated first. `work->freeCountdown` is cleared on every path, and the two
/// that end with the work block's state index reset are the ones that set the
/// word to 0x80.
///
/// Same body as `func_actor_444000_8013A958` without that sibling's
/// `TmdObject::buffer` buffer tests, so every escort is re-allocated
/// unconditionally.
s32 func_actor_403200_80138468(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    GluttonWork* work;
    GluttonWork* buffers;
    GluttonWork* escorts;
    GluttonWork* rebuilt;
    s16          i;
    s16          j;

    work = task->work;
    switch (arg2) {
        case 0:
            buffers = task->work;
            tmdAllocPrimitiveBuffer(task->extra.tmd);
            for (j = 0; j < ARRAY_SIZE(buffers->escorts); j++) {
                if (buffers->escorts[j] != NULL) {
                    tmdAllocPrimitiveBuffer(buffers->escorts[j]->task->extra.tmd);
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
            rebuilt = task->work;
            tmdAllocPrimitiveBuffer(task->extra.tmd);
            for (j = 0; j < ARRAY_SIZE(rebuilt->escorts); j++) {
                if (rebuilt->escorts[j] != NULL) {
                    tmdAllocPrimitiveBuffer(rebuilt->escorts[j]->task->extra.tmd);
                }
            }
            break;
        case 2:
            work->freeCountdown    = 0;
            escorts                = work;
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            for (i = 0; i < ARRAY_SIZE(escorts->escorts); i++) {
                if (escorts->escorts[i] != NULL) {
                    escorts->escorts[i]->task->extra.tmd->flags = task->extra.tmd->flags;
                }
            }
            work->state = 0;
            break;
        case 3:
            i                      = 0;
            escorts                = task->work;
            escorts->freeCountdown = 0;
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            for (; i < ARRAY_SIZE(escorts->escorts); i++) {
                if (escorts->escorts[i] != NULL) {
                    escorts->escorts[i]->task->extra.tmd->flags = task->extra.tmd->flags;
                }
            }
            break;
    }
    return 0;
}

/// Handles message 0x7DB: records the payload and dispatches the sender's
/// action to reset the escorts, select an attack, or finish the return pose.
s32 func_actor_403200_80138748(Task* task, s32 msgId, ActorCommand* msg, s32 arg3)
{
    GluttonWork* work;
    GluttonWork* escorts;
    GluttonWork* rebuilt;
    Enemy*       temp_enemy;
    s16          i;
    s16          j;
    s32          sound;
    s32          pan;
    s32          action;

    work       = task->work;
    temp_enemy = (Enemy*)task->spawnArg2.pointer;

    work->lastCommandStage = msg->context.loc.stage;
    work->lastCommandArea  = msg->context.loc.area;
    work->lastCommand      = (u8)msg->command;

    if (msg->context.key == 0x2704) {
        action = msg->command;
        switch (action) {
            case 0:
                work->state              = 0;
                work->wallDistanceTarget = 0xFA0;
                break;

            case 2:
                work->state    = 5;
                work->animId   = 0x14;
                work->animStep = GLUTTON_ANIM_STEP_RESTART;
                sound          = ((temp_enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200002;
                pan            = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
                sndEvtRequestScriptStart(sound, pan,
                                         (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
                break;

            case 3:
                work->state     = 5;
                work->prevState = -1;
                work->animId    = 0xD;
                work->animStep  = GLUTTON_ANIM_STEP_BLEND;
                break;

            case 5:
                work->state            = 0xA;
                escorts                = task->work;
                escorts->freeCountdown = 0;
                task->extra.tmd->flags = 0;
                for (i = 0; i < ARRAY_SIZE(escorts->escorts); i++) {
                    if (escorts->escorts[i] != NULL) {
                        escorts->escorts[i]->task->extra.tmd->flags =
                            task->extra.tmd->flags;
                    }
                }
                rebuilt = task->work;
                tmdAllocPrimitiveBuffer(task->extra.tmd);
                for (j = 0; j < ARRAY_SIZE(rebuilt->escorts); j++) {
                    if (rebuilt->escorts[j] != NULL) {
                        tmdAllocPrimitiveBuffer(rebuilt->escorts[j]->task->extra.tmd);
                    }
                }
                work->viewSelector = 0;
                work->viewLocked   = 0;
                break;

            case 10:
                work->state        = 0xC;
                work->prevState    = -1;
                work->animId       = 0x12;
                work->animStep     = GLUTTON_ANIM_STEP_RESTART;
                work->viewSelector = 7;
                work->collapseSkip = 0;
                work->viewLocked   = 1;
                break;

            case 11:
                work->state        = 0xC;
                work->prevState    = -1;
                work->animId       = 0x12;
                work->collapseSkip = 0x96;
                work->animStep     = GLUTTON_ANIM_STEP_RESTART;
                work->viewLocked   = 1;
                break;

            case 12:
                work->viewSelector = 7;
                work->viewLocked   = 0;
                work->state        = 1;

            case 19:
                work->state        = 0xC;
                work->viewSelector = 4;
                work->prevState    = -1;
                work->animId       = 0x12;
                work->collapseSkip = 0x258;
                work->animStep     = GLUTTON_ANIM_STEP_RESTART;
                work->viewLocked   = 0;
                gGluttonLimbReach  = 0x640;
                break;
        }
    }

    if (msg->context.key == 0x2804) {
        switch (msg->command) {
            case 0:
                work->state = 0;
                break;

            case 1:
                work->animId   = 0xA;
                work->animStep = GLUTTON_ANIM_STEP_RESTART;
                work->animRate = 0x7F;
                gluttonTickAnim(task);
                while (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
                    gluttonTickAnim(task);
                }
                work->animRate                        = 0x10;
                task->extra.tmd->coords->coord.t[0]   = -0xBB8;
                task->extra.tmd->coords->coord.t[1]   = 0;
                task->extra.tmd->coords->coord.t[2]   = -0x992;
                task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                work->state                           = 6;
                break;
        }
    }
    return 1;
}

/// Spawn state of the arena boss: allocate its work block, wire the host enemy
/// up to the model's root coordinate and its nine collision objects, then spawn
/// the seven escorts that make up the rest of the creature.
static void func_actor_403200_80138AFC(Enemy* enemy, Task* task)
{
    GluttonWork*           work;
    GluttonWork*           buffers;
    GluttonWork*           escorts;
    GfxMatrix*             mtx;
    TmdObject*             tmd;
    GfxCoord*              coord;
    GfxCoord*              freeCoord;
    Enemy*                 esc;
    Task*                  escTask;
    WorldCollisionContact* recs2;
    SVECTOR                dir;
    SVECTOR*               gteDir;
    VECTOR                 pos;
    s16                    i;
    s16                    j;

    tmd   = task->extra.tmd;
    coord = tmd->coords;

    work       = memCalloc(sizeof(GluttonWork), 0);
    task->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }

    (Gp_IncStateF0Ref)(0);
    task->exitCallback = gluttonExit;

    enemy->field_4    = &task->extra.tmd->coords->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = -0xC8;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &task->extra.tmd->coords[4];
    Gp_LinkNode(&enemy->node);
    enemy->reactionFlags = 0;
    enemy->hp            = D_actor_403200_80141C00.hpMax;
    enemy->param         = &D_actor_403200_80141C00;
    enemy->recs          = work->hits[0].contacts;

    animationInitContext(&work->hostRig.anim, D_actor_403200_8015E484, tmd, work->hostRig.poses, work->hostRig.slots);
    animationInitContext(&work->hostBlendRig.anim, D_actor_403200_8015E484, tmd, work->hostBlendRig.poses, work->hostBlendRig.slots);

    work->animStep        = GLUTTON_ANIM_STEP_RESTART;
    work->animId          = 2;
    work->limbPoseEnabled = 1;
    work->blending        = 0;
    work->neckYawTarget = work->neckYaw = 0;
    work->animRate = work->field_7B8 = 0x10;

    func_8010C980(&task->extra.tmd->coords[4], &work->hits[1].body, work->hits[1].contacts, ARRAY_SIZE(work->hits[1].contacts), 0x20, 0x300);
    func_8010C980(&task->extra.tmd->coords[4], &work->hits[0].body, work->hits[0].contacts, ARRAY_SIZE(work->hits[0].contacts), 0x20, 0x300);
    func_8010C980(&task->extra.tmd->coords[1], &work->hits[2].body, work->hits[2].contacts, ARRAY_SIZE(work->hits[2].contacts), 0x20, 0xBB8);

    work->hits[1].body.pos.vz = -0x100;
    work->hits[2].body.pos.vy = 0x400;
    work->hits[1].body.pos.vx = 0;
    work->hits[1].body.pos.vy = 0;
    work->hits[2].body.pos.vx = 0;
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
    task->msgTable                        = D_actor_403200_8015F770;
    coord->parent                         = &gGfxViewCoord;
    coord->composeStamp                   = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);

    work->prevState = -1;
    buffers         = task->work;
    tmdAllocPrimitiveBuffer(task->extra.tmd);
    for (i = 0; i < ARRAY_SIZE(buffers->escorts); i++) {
        if (buffers->escorts[i] != NULL) {
            tmdAllocPrimitiveBuffer(buffers->escorts[i]->task->extra.tmd);
        }
    }

    Actor403200_SeedRootCoord(task, work);

    esc                                                   = Gp_SpawnEnemyFromTable(D_actor_403200_8015E72C, 0, 0, task->spawnArg2.pointer);
    work->escorts[0]                                      = esc;
    esc->task->extra.tmd->coords->parent                  = task->extra.tmd->coords;
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
    Gp_LinkNode(&work->escorts[0]->node);
    work->escorts[0]->reactionFlags = 0;
    work->escorts[0]->hp            = D_actor_403200_80141C00.hpMax;
    work->groups3To5Pool            = D_actor_403200_80141C20.hpMax;
    work->escorts[0]->param         = &D_actor_403200_80141C20;
    work->escorts[0]->recs          = work->hits[3].contacts;
    func_8010C980(&work->escorts[0]->task->extra.tmd->coords[1], &work->hits[3].body, work->hits[3].contacts, ARRAY_SIZE(work->hits[3].contacts),
                  0x20, 0x300);
    func_8010C980(&work->escorts[0]->task->extra.tmd->coords[2], &work->hits[4].body, work->hits[4].contacts, ARRAY_SIZE(work->hits[4].contacts),
                  0x20, 0x300);
    func_8010C980(&work->escorts[0]->task->extra.tmd->coords[3], &work->hits[5].body, work->hits[5].contacts, ARRAY_SIZE(work->hits[5].contacts),
                  0x20, 0x300);

    esc                                                   = Gp_SpawnEnemyFromTable(D_actor_403200_8015E72C, 1, 0, task->spawnArg2.pointer);
    work->escorts[1]                                      = esc;
    esc->task->extra.tmd->coords->parent                  = task->extra.tmd->coords;
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
    Gp_LinkNode(&work->escorts[1]->node);
    work->escorts[1]->reactionFlags = 0;
    work->escorts[1]->hp            = D_actor_403200_80141C00.hpMax;
    work->groups6To8Pool            = D_actor_403200_80141C30.hpMax;
    work->escorts[1]->param         = &D_actor_403200_80141C30;
    work->escorts[1]->recs          = work->hits[6].contacts;
    func_8010C980(&work->escorts[1]->task->extra.tmd->coords[1], &work->hits[6].body, work->hits[6].contacts, ARRAY_SIZE(work->hits[6].contacts),
                  0x20, 0x300);
    func_8010C980(&work->escorts[1]->task->extra.tmd->coords[2], &work->hits[7].body, work->hits[7].contacts, ARRAY_SIZE(work->hits[7].contacts),
                  0x20, 0x300);
    func_8010C980(&work->escorts[1]->task->extra.tmd->coords[3], &work->hits[8].body, work->hits[8].contacts, ARRAY_SIZE(work->hits[8].contacts),
                  0x20, 0x300);

    esc              = Gp_SpawnEnemyFromTable(D_actor_403200_8015E72C, 2, 0, task->spawnArg2.pointer);
    work->escorts[2] = esc;
    if (esc != NULL) {
        esc->task->extra.tmd->coords->parent                  = &task->extra.tmd->coords[4];
        work->escorts[2]->task->extra.tmd->coords->coord.t[0] = 0;
        work->escorts[2]->task->extra.tmd->coords->coord.t[1] = 0x59;
        work->escorts[2]->task->extra.tmd->coords->coord.t[2] = -0x64;
        work->escorts[2]->task->extra.tmd->flags              = 0;
    }

    esc              = Gp_SpawnEnemyFromTable(D_actor_403200_8015E72C, 3, 0, task->spawnArg2.pointer);
    work->escorts[3] = esc;
    if (esc != NULL) {
        esc->task->extra.tmd->coords->parent                  = &task->extra.tmd->coords[3];
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
        Gp_LinkNode(&work->escorts[3]->node);
        work->escorts[3]->reactionFlags = 0;
        work->escorts[3]->hp            = D_actor_403200_80141C00.hpMax;
        work->groups1To2Pool            = D_actor_403200_80141C40.hpMax;
        work->escorts[3]->param         = &D_actor_403200_80141C40;
        work->escorts[3]->recs          = work->hits[1].contacts;
    }

    esc              = Gp_SpawnEnemyFromTable(D_actor_403200_8015E72C, 4, 0, task->spawnArg2.pointer);
    work->escorts[4] = esc;
    if (esc != NULL) {
        esc->task->extra.tmd->coords->parent                  = &task->extra.tmd->coords[4];
        work->escorts[4]->task->extra.tmd->coords->coord.t[0] = 0;
        work->escorts[4]->task->extra.tmd->coords->coord.t[1] = 0;
        work->escorts[4]->task->extra.tmd->coords->coord.t[2] = 0x14;
        work->escorts[4]->task->extra.tmd->flags              = 0;
    }

    esc              = Gp_SpawnEnemyFromTable(D_actor_403200_8015E72C, 5, 0, task->spawnArg2.pointer);
    work->escorts[5] = esc;
    if (esc != NULL) {
        esc->task->extra.tmd->coords->parent                  = &task->extra.tmd->coords[2];
        work->escorts[5]->task->extra.tmd->coords->coord.t[0] = 0;
        work->escorts[5]->task->extra.tmd->coords->coord.t[1] = 0x67C;
        work->escorts[5]->task->extra.tmd->coords->coord.t[2] = 0xC8;
        work->escorts[5]->task->extra.tmd->flags              = 0;
    }

    esc              = Gp_SpawnEnemyFromTable(D_actor_403200_8015E72C, 6, 0, task->spawnArg2.pointer);
    work->escorts[6] = esc;
    if (esc != NULL) {
        esc->task->extra.tmd->coords->parent                  = &task->extra.tmd->coords[1];
        work->escorts[6]->task->extra.tmd->coords->coord.t[0] = 0;
        work->escorts[6]->task->extra.tmd->coords->coord.t[1] = 0x62C;
        work->escorts[6]->task->extra.tmd->coords->coord.t[2] = 0x5DC;
        work->escorts[6]->task->extra.tmd->flags              = 0;
    }

    work->groups6To8Pool  = 0x3C;
    freeCoord             = &work->swipeCoord.node;
    work->escorts[6]      = NULL;
    work->viewLocked      = 0;
    work->viewSelector    = 0;
    work->phase           = 0;
    work->groups3To5Pool  = 0x32;
    work->spinnersSpawned = 0;

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
    recs2                           = work->swipeContacts;
    work->swipeCapsule.end0Radius   = 0x258;
    work->swipeCapsule.end1Radius   = 0x258;
    work->swipeCapsule.ends[0].vx   = 0;
    work->swipeCapsule.ends[0].vy   = 0;
    work->swipeCapsule.ends[0].vz   = 0;
    work->swipeCapsule.ends[1].vx   = 0;
    work->swipeCapsule.ends[1].vy   = 0;
    work->swipeCapsule.contacts     = recs2;
    work->swipeBody.coord           = freeCoord;
    work->swipeBody.context.capsule = &work->swipeCapsule;
    work->swipeBody.pos.vx          = 0;
    work->swipeBody.pos.vy          = -0xFA;
    work->swipeBody.pos.vz          = 0x25F;
    work->swipeBody.key             = 0x30000 | 0x20;
    work->swipeBody.radius          = 0;
    work->swipeBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
    Gp_LinkObj(2, &work->swipeBody);
    Gp_InitRec18Table(recs2, ARRAY_SIZE(work->swipeContacts), 0);
    work->swipeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    escorts                   = task->work;
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
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    gluttonTickAnim(task);

    D_actor_403200_8015F8F4.context.loc.stage = 0;
    D_actor_403200_8015F8F4.context.loc.area  = 0x2C;
    D_actor_403200_8015F8F4.command           = 0;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_actor_403200_8015F8F4, ACTOR_COMMAND_MESSAGE_APPLY);

    work->wallDistance = work->wallDistanceTarget = 0x9C4;
    work->wallDrop                                = 0x190;
    _gGluttonHostTask                             = task;
    work->summonsSpawned = work->summonsAlive = 0;
    task->state                              += 1;
}

#include "../../shared/glutton_hit_group0.inc.c"

#include "../../shared/glutton_hit_groups1to2.inc.c"

/// The hit handler for collision groups 3, 4 and 5 -- `gluttonHitGroups1To2`
/// done three times over the parts it does not cover, each group only scanned
/// when the previous one landed nothing and the part it hit reported no attack
/// id back. Like the sibling actor's `func_actor_444000_8013CA60`, this one runs
/// no `Gp_GetIdParam0` switch: the call is made and its kind thrown away, so
/// every hit is treated alike, and the group 3 arm is the one that gives up and
/// leaves the frame once it comes back empty.
///
/// The damage is the distance-scaled hit quadrupled when `Gp_RollEnemyChance`
/// fires, then divided by six (never down to zero unless it already was), and
/// comes off the host, the two escorts sharing its pool and `groups3To5Pool`.
/// Emptying that pool spawns the same effect again and refills it to 0x32. Both
/// effect spawns and the state change to 0xE are skipped while the boss is in
/// one of the seven states that ignore hits, while the player hold is armed, or
/// unless `gSceneCombatState.battleRefs` is 1.
///
/// `pos` / `pos2` / `pos3` are all `&sc->contactPoint`, and are not spare: each group's
/// scan writes the contact point through its own pointer, which is what keeps
/// the three `sh` pairs in `a3` then `a2` twice. `esc3` / `esc0` / `esc1` and
/// the `hp` load are the sibling's arrangement, but evaluated before
/// `func_800DA6E8` so `host->field_40` is still in a register and the three
/// stores reuse it; the pool subtraction after them carries the same `field_40`
/// value for the same reason.
static void func_actor_403200_8013A4A0(Task* arg0)
{
    GluttonHitScratch*     sc;
    GluttonWork*           work;
    Enemy*                 host;
    PlayerStatus*          cfg;
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
    s16                    param;
    u16                    hp;
    Enemy*                 esc3;
    Enemy*                 esc0;
    Enemy*                 esc1;

    cfg  = &gPlayerStatus;
    host = (Enemy*)arg0->spawnArg2.pointer;
    work = arg0->work;
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
        gluttonHitEffect(work->hits[3].body.coord, id);
        if (sc->attackKey != 0) {
            goto body;
        }
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
        gluttonHitEffect(work->hits[4].body.coord, id);
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
    gluttonHitEffect(work->hits[5].body.coord, id);
    if (sc->attackKey == 0) {
        goto out;
    }
body:
    param                    = Gp_GetIdParam2(sc->attackKey);
    work->groups6To8Cooldown = param;
    work->groups3To5Cooldown = param;
    work->group0Cooldown     = param;
    work->groups1To2Cooldown = param;
    Gp_GetIdParam0(sc->attackKey);

    sc->toPlayer.vx    = (cfg->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0]) + 0x51F;
    dx2                = sc->toPlayer.vx * sc->toPlayer.vx;
    sc->toPlayer.vy    = (cfg->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1]) - 0xFA;
    dy2                = sc->toPlayer.vy * sc->toPlayer.vy;
    sc->toPlayer.vz    = (cfg->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2]) + 0x25F;
    dz2                = sc->toPlayer.vz * sc->toPlayer.vz;
    sc->playerDistance = SquareRoot0(dx2 + dy2 + dz2);
    sc->damage         = Gp_ComputeDamage(sc->attackKey, sc->playerDistance, 0, 0);

    if (Gp_RollEnemyChance(work->escorts[0], sc->attackKey, 0) != 0 && (state = work->state, state != 0xD) && state != 3 &&
        state != 9 && state != 0xE && state != 0xF && state != 8 && state != 0xB && work->playerCaught != 1 &&
        gSceneCombatState.battleRefs == 1) {
        sc->offset.vy = 0;
        sc->offset.vx = 0;
        sc->offset.vz = 0x320;
        Gp_SpawnEff(EFFECT_CRITICAL_HIT, &work->escorts[0]->task->extra.tmd->coords[1], 0, &sc->offset);
        sc->damage *= 4;
        work->state = 0xE;
    }

    dmg = sc->damage / 6;
    if (dmg == 0) {
        dmg = 1;
        if (sc->damage == 0) {
            sc->damage = 0;
            goto stored;
        }
    }
    sc->damage = dmg;
stored:
    func_800E2C78(host, sc->attackKey, sc->damage, 0);
    host->hp             -= sc->damage;
    esc3                  = work->escorts[3];
    hp                    = host->hp;
    esc0                  = work->escorts[0];
    esc1                  = work->escorts[1];
    esc3->hp              = hp;
    esc1->hp              = hp;
    esc0->hp              = hp;
    work->groups3To5Pool -= sc->damage;
    if (work->groups3To5Pool <= 0 && (state = work->state, state != 0xD) && state != 3 && state != 9 && state != 0xE &&
        state != 0xF && state != 8 && state != 0xB && work->playerCaught != 1 && gSceneCombatState.battleRefs == 1) {
        sc->offset.vy = 0;
        sc->offset.vx = 0;
        sc->offset.vz = 0x320;
        Gp_SpawnEff(EFFECT_CRITICAL_HIT, &work->escorts[0]->task->extra.tmd->coords[1], 0, &sc->offset);
        work->state          = 0xE;
        work->groups3To5Pool = 0x32;
    }

    func_800DA6E8(&work->escorts[0]->node, sc->damage, 0);
    work->escorts[0]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(work->escorts[0]->task->extra.tmd->coords);
    sc->offset.vx = sc->contactPoint.vx - work->escorts[0]->task->extra.tmd->coords->workm.t[0];
    sc->offset.vy = sc->contactPoint.vy - work->escorts[0]->task->extra.tmd->coords->workm.t[1];
    sc->offset.vz = sc->contactPoint.vz - work->escorts[0]->task->extra.tmd->coords->workm.t[2];
    angle         = ratan2(sc->offset.vx, sc->offset.vz) -
            ratan2(-arg0->extra.tmd->coords->workm.m[2][0],
                   arg0->extra.tmd->coords->workm.m[2][2]);
    do {
        sc->contactYaw = angle;
        if (angle < 0) {
        wrapUp:
            if (angle < -0x800) {
                angle += 0x1000;
                goto wrapUp;
            }
        } else {
        wrapDown:
            if (angle > 0x800) {
                angle -= 0x1000;
                goto wrapDown;
            }
        }
    } while (0);
    sc->contactYaw = angle;

    work->neckYaw       = 0;
    work->neckYawTarget = 0;
out:
    SCRATCH_STACK_RELEASE_BLOCK(GluttonHitScratch);
}

#include "../../shared/glutton_hit_groups6to8.inc.c"

/// Reset handler: pushes the host model's `field_C` onto each of the seven
/// escorts, and once the sub-state counter has reached 2 releases the host's and
/// every escort's model buffers. Same shape as
/// `func_actor_403200_80141B40` with a second arm keyed on `stateTicks`.
///
/// The `modelFlag` copy is not redundant: the second arm's `0x80` has to reach
/// the store as a 32-bit value of its own, or the two arms merge it into the
/// first arm's constant and the second `li $v0, 0x80` disappears.
static void func_actor_403200_8013B23C(Task* arg0)
{
    GluttonWork* work;
    GluttonWork* escorts;
    GluttonWork* dying;
    TmdObject*   tmd;
    s32          flag;
    s32          modelFlag;
    s16          i;
    s16          j;

    work = arg0->work;
    tmd  = arg0->extra.tmd;
    if (work->stateChanged != 0) {
        tmd->flags             = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        escorts                = arg0->work;
        i                      = 0;
        escorts->freeCountdown = 0;
        arg0->extra.tmd->flags = (flag = TMD_OBJECT_SKIP_ACTIVE_DRAW);
        for (; i < ARRAY_SIZE(escorts->escorts); i++) {
            if (escorts->escorts[i] != NULL) {
                escorts->escorts[i]->task->extra.tmd->flags =
                    arg0->extra.tmd->flags;
            }
        }
        work->stateTicks = 0;
        return;
    }
    if (work->stateTicks == 2) {
        tmd->flags             = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        escorts                = arg0->work;
        modelFlag              = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        i                      = 0;
        escorts->freeCountdown = 0;
        arg0->extra.tmd->flags = (flag = modelFlag);
        for (; i < ARRAY_SIZE(escorts->escorts); i++) {
            if (escorts->escorts[i] != NULL) {
                escorts->escorts[i]->task->extra.tmd->flags =
                    arg0->extra.tmd->flags;
            }
        }
        dying = arg0->work;
        tmdFreePrimitiveBuffer(arg0->extra.tmd);
        for (j = 0; j < ARRAY_SIZE(dying->escorts); j++) {
            if (dying->escorts[j] != NULL) {
                tmdFreePrimitiveBuffer(dying->escorts[j]->task->extra.tmd);
            }
        }
    }
}

/// State-change reset for the enemy's launch state: `func_actor_403200_8013B23C`'s
/// reset half with a yaw servo in the middle. It arms the stand-up pair
/// (`lastAttack` 2, `animId` 3), turns animation slot 2 on, clears the host
/// model's flag word and walks the seven escorts pushing that word onto each of
/// their models, allocates the host's and every escort's buffers, and only then
/// turns the enemy to face the player -- the host root part's position made
/// relative to the player's root coordinate, `ratan2` of that pair less the
/// enemy's own facing, wrapped to +/-0x800 into `neckYawTarget`. On the way out it
/// runs the per-frame body, re-arms the state to 0xA on the animation slot's
/// flag, and latches `viewSelector` once the state counter is past 0x14.
///
/// The switch is on the state counter and spawns from
/// `gGluttonEscortTasks`, each of the eight counter values picking its own
/// table index; the spawned enemy is dropped, unlike the arena reset's. The
/// `state` copy is what keeps the switch index 16-bit, as in
/// `func_actor_403200_8013D9EC`.
static void func_actor_403200_8013B3C8(Task* arg0)
{
    GluttonWork*                   work;
    GluttonWork*                   escorts;
    GluttonWork*                   dying;
    GfxCoord*                      model;
    GfxCoord*                      facing;
    _Actor403200RainLaunchScratch* sc;
    s16                            i;
    s16                            j;
    s16                            state;
    s16                            ang;

    sc   = SCRATCH_STACK_RESERVE_BLOCK(_Actor403200RainLaunchScratch);
    work = arg0->work;
    if (work->stateChanged != 0) {
        work->lastAttack       = 2;
        work->animId           = 3;
        work->animStep         = GLUTTON_ANIM_STEP_RESTART;
        escorts                = arg0->work;
        escorts->freeCountdown = 0;
        arg0->extra.tmd->flags = 0;
        for (i = 0; i < ARRAY_SIZE(escorts->escorts); i++) {
            if (escorts->escorts[i] != NULL) {
                escorts->escorts[i]->task->extra.tmd->flags =
                    arg0->extra.tmd->flags;
            }
        }
        dying = arg0->work;
        tmdAllocPrimitiveBuffer(arg0->extra.tmd);
        for (j = 0; j < ARRAY_SIZE(dying->escorts); j++) {
            if (dying->escorts[j] != NULL) {
                tmdAllocPrimitiveBuffer(dying->escorts[j]->task->extra.tmd);
            }
        }
        work->neckYawEnabled   = 1;
        work->neckPitchEnabled = 1;
        work->hostExposed      = 0;
    }
    state = work->stateTicks - 0x13;
    switch (state) {
        case 0:
            Gp_SpawnEnemyFromTable(gGluttonEscortTasks, 1, 0, arg0->spawnArg2.pointer)->workType = ENEMY_WORK_PLAIN;
            break;
        case 7:
            Gp_SpawnEnemyFromTable(gGluttonEscortTasks, 1, 1, arg0->spawnArg2.pointer)->workType = ENEMY_WORK_PLAIN;
            break;
        case 9:
            Gp_SpawnEnemyFromTable(gGluttonEscortTasks, 1, 2, arg0->spawnArg2.pointer)->workType = ENEMY_WORK_PLAIN;
            break;
        case 0x10:
            Gp_SpawnEnemyFromTable(gGluttonEscortTasks, 1, 3, arg0->spawnArg2.pointer)->workType = ENEMY_WORK_PLAIN;
            break;
        case 0x1F:
            Gp_SpawnEnemyFromTable(gGluttonEscortTasks, 1, 4, arg0->spawnArg2.pointer)->workType = ENEMY_WORK_PLAIN;
            break;
        case 0x37:
            Gp_SpawnEnemyFromTable(gGluttonEscortTasks, 1, 5, arg0->spawnArg2.pointer)->workType = ENEMY_WORK_PLAIN;
            break;
        case 0x3B:
            Gp_SpawnEnemyFromTable(gGluttonEscortTasks, 1, 6, arg0->spawnArg2.pointer)->workType = ENEMY_WORK_PLAIN;
            break;
        case 0x3F:
            Gp_SpawnEnemyFromTable(gGluttonEscortTasks, 1, 7, arg0->spawnArg2.pointer)->workType = ENEMY_WORK_PLAIN;
            break;
    }
    gluttonTickAnim(arg0);
    if (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = 0xA;
    }
    if (work->stateTicks >= 0x15) {
        work->viewSelector = 1;
    }
    model           = arg0->extra.tmd->coords;
    sc->toPlayer.vx = gPlayerStatus.coordMtx->t[0] - model->coord.t[0];
    sc->toPlayer.vy = gPlayerStatus.coordMtx->t[1] - model->coord.t[1];
    sc->toPlayer.vz = gPlayerStatus.coordMtx->t[2] - model->coord.t[2];
    facing          = arg0->extra.tmd->coords;
    ang             = ratan2(sc->toPlayer.vx, sc->toPlayer.vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    if (ang < 0) {
    wrapUp:
        if (ang < -0x800) {
            ang += 0x1000;
            goto wrapUp;
        }
    } else {
    wrapDown:
        if (ang > 0x800) {
            ang -= 0x1000;
            goto wrapDown;
        }
    }
    work->neckYawTarget = ang;
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403200RainLaunchScratch);
}

/// Spawns up to nine enemies in a randomly selected formation, stopping when
/// a spawn fails. Each member's index becomes the high nibble of its place key.
static void func_actor_403200_8013B740(Task* arg0)
{
    GluttonWork* work;
    Enemy*       enemy;
    s16          i;
    s16          formation;

    work = arg0->work;

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    formation       = (gRandomLcgState >> 16) & 3;
    if (formation == 3) {
        formation = 0;
    }

    for (i = 0; i < 9; i++) {
        gGluttonEscortTasks[4].data.model = D_actor_403200_8015F888[i].model;
        enemy                             = Gp_SpawnEnemyFromTable(gGluttonEscortTasks, 4, D_actor_403200_8015F888[i].chaseDelayClass, NULL);
        work->lastSpawned                 = enemy;
        if (enemy == NULL) {
            break;
        }
        enemy->task->extra.tmd->coords->coord.t[0]             = D_actor_403200_8015F7B0[formation][i].vx;
        work->lastSpawned->task->extra.tmd->coords->coord.t[1] = D_actor_403200_8015F7B0[formation][i].vy;
        work->lastSpawned->task->extra.tmd->coords->coord.t[2] = D_actor_403200_8015F7B0[formation][i].vz;
        work->lastSpawned->workType                            = ENEMY_WORK_PLAIN;
        work->lastSpawned->placeKey                           |= i << ENEMY_PLACE_INDEX_SHIFT;
    }
}

/// Per-frame body of the launch state's pull. A state change re-arms the
/// escorts and tells the scene (message 0x7DA, action 0x2C). Each tick yaws
/// the enemy toward the player and scales a pull from the animation frame;
/// inside the swipe window, once the player accepts message 0x3F8, it places
/// them (`GAME_ACTOR_MESSAGE_PLACE`) and hands over an animation (0x3F4).
static void func_actor_403200_8013B8C4(Task* arg0)
{
    GluttonWork*             work;
    GluttonWork*             escorts;
    GluttonWork*             dying;
    Enemy*                   enemy;
    Task*                    task;
    PlayerStatus*            cfg;
    _Actor403200DragScratch* sc;
    s16                      i;
    s16                      j;

    work  = arg0->work;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    task  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    cfg   = &gPlayerStatus;
    sc    = SCRATCH_STACK_RESERVE_BLOCK(_Actor403200DragScratch);

    if (work->stateChanged != 0) {
        work->lastAttack       = 3;
        work->animId           = 3;
        work->animStep         = GLUTTON_ANIM_STEP_RESTART;
        escorts                = arg0->work;
        escorts->freeCountdown = 0;
        arg0->extra.tmd->flags = 0;
        for (i = 0; i < ARRAY_SIZE(escorts->escorts); i++) {
            if (escorts->escorts[i] != NULL) {
                escorts->escorts[i]->task->extra.tmd->flags =
                    arg0->extra.tmd->flags;
            }
        }
        dying = arg0->work;
        tmdAllocPrimitiveBuffer(arg0->extra.tmd);
        for (j = 0; j < ARRAY_SIZE(dying->escorts); j++) {
            if (dying->escorts[j] != NULL) {
                tmdAllocPrimitiveBuffer(dying->escorts[j]->task->extra.tmd);
            }
        }
        work->neckPitchTarget    = 0;
        work->neckPitchEnabled   = 1;
        work->neckYawEnabled     = 1;
        work->viewLocked         = 0;
        work->hostExposed        = 0;
        gGluttonSpinnerTarget.vz = 0;
        gGluttonSpinnerTarget.vy = 0;
        gGluttonSpinnerTarget.vx = 0;
        actorLocalToView(&arg0->extra.tmd->coords[3], &gGluttonSpinnerTarget);
        D_actor_403200_8015F8F4.context.loc.stage = 0;
        D_actor_403200_8015F8F4.context.loc.area  = 0x2C;
        D_actor_403200_8015F8F4.command           = 2;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_actor_403200_8015F8F4, ACTOR_COMMAND_MESSAGE_APPLY);
        {
            s16 armed                = 1;
            work->wallDistanceTarget = 0xC80;
            gGluttonSpinnersReleased = armed;
        }
    }

    gluttonTickAnim(arg0);

    work->neckYawTarget = actorPositionYaw(arg0, &sc->offset, &gPlayerStatus);

    sc->offset.vz = 0;
    sc->offset.vy = 0;
    sc->offset.vx = 0;
    actorLocalToView(&arg0->extra.tmd->coords[4], &sc->offset);

    sc->offset.vx       = (u16)task->extra.tmd->coords->coord.t[0] - (u16)sc->offset.vx;
    sc->offset.vy       = (u16)task->extra.tmd->coords->coord.t[1] - (u16)sc->offset.vy;
    sc->offset.vz       = (u16)task->extra.tmd->coords->coord.t[2] - (u16)sc->offset.vz;
    sc->playerDistance  = sc->offset.vx * sc->offset.vx;
    sc->playerDistance += sc->offset.vz * sc->offset.vz;
    sc->playerDistance  = SquareRoot0(sc->playerDistance);
    VectorNormalSS(&sc->offset, &sc->offset);

    switch (work->phase) {
        case 0:
            sc->rumblePeriod = 0x14;
            break;
        case 1:
            sc->rumblePeriod = 0x10;
            break;
        case 2:
        default:
            sc->rumblePeriod = 0xC;
            break;
    }
    if (((u32)((work->hostRig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) - 0xA) < 9U) && ((work->stateTicks % sc->rumblePeriod) == 0)) {
        Gp_SpawnScript18(D_actor_403200_80141C7C, D_actor_403200_80141C88);
    }

    switch (work->phase) {
        case 0:
            sc->phasePull = 0;
            break;
        case 1:
            sc->phasePull = 5;
            break;
        case 2:
        default:
            sc->phasePull = 0xA;
            break;
    }

    if (work->stateTicks == 0xA) {
        s32 sfx;
        s32 pan;

        sfx = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200017;
        pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(sfx, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (work->stateTicks == 0x3C) {
        s32 sfx;
        s32 pan;

        sfx = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000A;
        pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(sfx, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (work->stateTicks == 0xE8) {
        SndEvt_EnqueueType7((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000A, 1);
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

    if (((u32)((work->hostRig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) - 0xB) < 5U) && (sc->playerDistance < 0x4B0) && (enemy->hp > 0) &&
        (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &D_actor_403200_8015FA00, 0) == 0)) {
        SVECTOR* dirp;
        s16      ang;

        work->state        = 0xD;
        work->playerCaught = 1;
        sc->pullCentre.vz  = 0;
        sc->pullCentre.vy  = 0;
        sc->pullCentre.vx  = 0;
        actorLocalToView(&arg0->extra.tmd->coords[4], &sc->pullCentre);

        sc->offset.vx       = (u16)task->extra.tmd->coords->coord.t[0] - (u16)sc->pullCentre.vx;
        sc->offset.vy       = 0;
        sc->offset.vz       = (u16)task->extra.tmd->coords->coord.t[2] - (u16)sc->pullCentre.vz;
        ang                 = actorViewYaw(arg0->extra.tmd->coords, &sc->offset);
        dirp                = &sc->offset;
        work->neckYawTarget = ang;
        VectorNormalSS(dirp, dirp);
        gte_lddp(0x384);
        gte_ldsv(dirp);
        gte_gpf12();
        gte_stsv(dirp);

        D_actor_403200_8015F9C0.placement.pos.vx = sc->pullCentre.vx + sc->offset.vx;
        D_actor_403200_8015F9C0.placement.pos.vy = task->extra.tmd->coords->coord.t[1];
        {
            s32 pz = sc->pullCentre.vz;
            s32 dz = sc->offset.vz;

            D_actor_403200_8015F9C0.placement.rot.vx = 0;
            D_actor_403200_8015F9C0.placement.rot.vz = 0;
            D_actor_403200_8015F9C0.placement.pos.vz = pz + dz;
        }
        {
            u16 px = (u16)sc->pullCentre.vx;
            u16 mx = (u16)D_actor_403200_8015F9C0.placement.pos.vx;

            sc->offset.vy = 0;
            sc->offset.vx = px - mx;
        }
        sc->offset.vz = (u16)sc->pullCentre.vz - (u16)D_actor_403200_8015F9C0.placement.pos.vz;
        ang           = actorViewYaw(task->extra.tmd->coords, dirp);
        {
            s32 ext = ang;

            sc->pullCentreYaw = ext;
            if (abs(ext) < 0x400) {
                D_actor_403200_8015F9C0.placement.rot.vy = ratan2((s32)sc->offset.vx, (s32)sc->offset.vz);
                work->playerAnim.source.sets             = D_actor_403200_8015E6AC;
            } else {
                D_actor_403200_8015F9C0.placement.rot.vy = ratan2((s32)sc->offset.vx, (s32)sc->offset.vz) + 0x800;
                work->playerAnim.source.sets             = D_actor_403200_8015E6CC;
            }
        }
        if (cfg->hp > 0) {
            TASK_MESSAGE_DISPATCH_POINTER(task, GAME_ACTOR_MESSAGE_PLACE, &D_actor_403200_8015F9C0.placement, 0);
        }
        work->playerAnim.animationId = 1;
        work->playerAnim.blend       = ANIMATION_BLEND_RESET;
        work->playerAnim.blendFrames = 0;
        work->field_F02              = 1;
        TASK_MESSAGE_DISPATCH_POINTER(task, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &work->playerAnim, 0);
        work->caughtTicks = 0;
    }

    if (sc->offset.vx != 0 || sc->offset.vz != 0) {
        sc->displacement.vx = sc->offset.vx;
        sc->displacement.vy = 0;
        sc->displacement.vz = sc->offset.vz;
        func_80105B74(&sc->displacement);
    }

    if (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        D_actor_403200_8015F8F4.context.loc.stage = 0;
        D_actor_403200_8015F8F4.context.loc.area  = 0x2C;
        D_actor_403200_8015F8F4.command           = 3;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_actor_403200_8015F8F4, ACTOR_COMMAND_MESSAGE_APPLY);
        gGluttonSpinnersReleased = 0;
        work->spinnersSpawned    = 0;
        work->state              = 0xA;
        for (sc->slot = 0; sc->slot < 2; sc->slot++) {
            work->summons[sc->slot] = NULL;
        }
        work->summonsAlive = 0;
    }
    if (work->stateTicks == 0x14) {
        work->viewSelector = 2;
    }

    SCRATCH_STACK_RELEASE_BLOCK(_Actor403200DragScratch);
}

/// State-change reset for the enemy's launch state, and the tick that walks it
/// out of sub-state 0xF into 0xE.
///
/// The reset half is `func_actor_403200_8013B23C`'s with a yaw servo in the
/// middle: it tells the scene (message 0x7DA, action 0x2C), arms sub-state 0xF
/// with animation 2, clears the host model's flag word and walks the seven
/// escorts pushing that word onto each of their models, allocates the host's and
/// every escort's buffers, and only then turns the enemy to face the player --
/// the fourth model part's position carried into view space, made relative to
/// the player's root coordinate with y zeroed, `ratan2` of that pair less the
/// enemy's own facing, wrapped to +/-0x800 into `neckYawTarget`. It tells the scene
/// a second time, arms `wallDistanceTarget`, raises bit 0 of `Gp_StateC08.flags`,
/// pulses the state and clears the node slot of the host and of escorts 3, 0
/// and 1.
///
/// The tick runs the per-frame body, steps 0xF to 0xE on the second animation
/// slot's flag, and while still in 0xF hands the player the launch message
/// (0x3F9) with `gPlayerStatus.hp` as its gate: the two arms either side
/// of that dispatch write the ramp timings into `gGameSession` and stamp escort
/// 3. The four one-shot cues all latch on the third animation slot's frame,
/// masked to ten bits, against the frame `prevSlot3Cue` saw last, and once the
/// state counter is past 0x18 the type-7 cue and the 0x3FF animation message go
/// out together.
///
/// Three things here are load-bearing. The yaw's arguments are read through
/// `posp` and the matrix half through `coord`: read straight off `view` the
/// stores would be forwarded into both arguments (two `sll`/`sra` pairs),
/// while through the pointer each stays a load out of the struct, which is what
/// the target does -- the second is reloaded from its slot, the first is folded
/// back onto `a0`, and `coord` is what keeps `field_8` in `s0` across the call.
/// The cue locals are declared inside each arm so local-alloc colours them per
/// block; hoisted to the top of the function they become one global pseudo and
/// the id and pan come out in each other's registers. And in the second 0x7DA
/// block `D_actor_403200_8015F8E0[0]` is cleared before the `neckYawTarget` store, so
/// its address is the one computed first.
static void func_actor_403200_8013C84C(Task* arg0)
{
    GluttonWork*  work;
    GluttonWork*  escorts;
    GluttonWork*  dying;
    Enemy*        enemy;
    Task*         task;
    PlayerStatus* cfg;
    SVECTOR       view;
    SVECTOR*      posp;
    GfxCoord*     coord;
    s16           i;
    s16           j;
    s16           yaw;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    task  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    cfg   = &gPlayerStatus;
    if (work->stateChanged != 0) {
        D_actor_403200_8015F8F4.context.loc.stage = 0;
        D_actor_403200_8015F8F4.context.loc.area  = 0x2C;
        D_actor_403200_8015F8F4.command           = 3;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_actor_403200_8015F8F4, ACTOR_COMMAND_MESSAGE_APPLY);
        gGluttonSpinnersReleased = 0;
        SndEvt_EnqueueType7((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000A, 1);
        work->animId           = 0xF;
        work->animStep         = GLUTTON_ANIM_STEP_RESTART;
        escorts                = arg0->work;
        escorts->freeCountdown = 0;
        arg0->extra.tmd->flags = 0;
        for (i = 0; i < ARRAY_SIZE(escorts->escorts); i++) {
            if (escorts->escorts[i] != NULL) {
                escorts->escorts[i]->task->extra.tmd->flags =
                    arg0->extra.tmd->flags;
            }
        }
        dying = arg0->work;
        tmdAllocPrimitiveBuffer(arg0->extra.tmd);
        for (j = 0; j < ARRAY_SIZE(dying->escorts); j++) {
            if (dying->escorts[j] != NULL) {
                tmdAllocPrimitiveBuffer(dying->escorts[j]->task->extra.tmd);
            }
        }
        work->neckYawEnabled   = 1;
        work->viewSelector     = 6;
        work->neckPitchTarget  = 0;
        work->neckPitchEnabled = 0;
        work->hostExposed      = 0;
        view.vz                = 0;
        view.vy                = 0;
        view.vx                = 0;
        actorLocalToView(&arg0->extra.tmd->coords[4], &view);
        view.vx = task->extra.tmd->coords[0].coord.t[0] - view.vx;
        view.vy = 0;
        view.vz = task->extra.tmd->coords[0].coord.t[2] - view.vz;
        posp    = &view;
        coord   = arg0->extra.tmd->coords;
        yaw     = ratan2(posp->vx, posp->vz) -
              ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        if (yaw < 0) {
        wrapUp:
            if (yaw < -0x800) {
                yaw += 0x1000;
                goto wrapUp;
            }
        } else {
        wrapDown:
            if (yaw > 0x800) {
                yaw -= 0x1000;
                goto wrapDown;
            }
        }
        work->neckYawTarget                       = yaw;
        D_actor_403200_8015F8E0[0]                = 0;
        D_actor_403200_8015F8F4.context.loc.stage = 0;
        D_actor_403200_8015F8F4.context.loc.area  = 0x2C;
        D_actor_403200_8015F8F4.command           = 3;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_actor_403200_8015F8F4, ACTOR_COMMAND_MESSAGE_APPLY);
        work->wallDistanceTarget = 0x9C4;
        gGluttonSpinnersReleased = 0;
        Gp_StateC08.flags       |= ATTACHMENT_FLAG_EVENT_LOCK;
        Gp_PulseState1C();
        Gp_ClearNodeSlots(&enemy->node);
        Gp_ClearNodeSlots(&work->escorts[3]->node);
        Gp_ClearNodeSlots(&work->escorts[0]->node);
        Gp_ClearNodeSlots(&work->escorts[1]->node);
        return;
    }

    SCRATCH_STACK_RESERVE_BYTES(0x3C);
    gluttonTickAnim(arg0);
    if ((work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) && (work->animId == 0xF)) {
        work->animStep = GLUTTON_ANIM_STEP_RESTART;
        work->animId   = 0xE;
    }
    if (work->animId == 0xF) {
        if (cfg->hp > 0) {
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackObjPair(enemy, 3), 0);
            if (cfg->hp <= 0) {
                ((GameActor*)task->work)->state   = 0xA;
                gGameSession->deathSoundCountdown = 0x1E;
                gGameSession->deathFadeFrames     = 0x36;
                gGameSession->deathRestartDelay   = 0x5A;
            }
        }
        if (((work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x19) && (work->prevSlot3Cue != (work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK))) {
            s32 sfx;
            s32 pan;
            s32 depth;

            sfx   = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200011;
            pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            depth = (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords);
            sndEvtRequestScriptStart(sfx, pan, depth);
        }
        work->prevSlot3Cue = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    }
    if (work->animId == 0xE) {
        if (((work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x1E) && (work->prevSlot3Cue != (work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK))) {
            Gp_SpawnPadLerp(4, 0xFF, 8);
        }
        if (((work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x23) && (work->prevSlot3Cue != (work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK))) {
            s32 sfx;
            s32 pan;

            sfx = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200012;
            pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            sndEvtRequestScriptStart(sfx, pan,
                                     (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            Gp_SpawnPadLerp(4, 0xFF, 8);
        }
        if (((work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x27) && (work->prevSlot3Cue != (work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK))) {
            s32 sfx;
            s32 pan;

            sfx = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200012;
            pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            sndEvtRequestScriptStart(sfx, pan,
                                     (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            Gp_SpawnPadLerp(4, 0xFF, 8);
        }
        work->prevSlot3Cue = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    }
    if ((taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) && (cfg->hp > 0)) {
        D_actor_403200_8015F9C0.placement.pos.vx = arg0->extra.tmd->coords[0].coord.t[0];
        D_actor_403200_8015F9C0.placement.pos.vy = arg0->extra.tmd->coords[0].coord.t[1];
        D_actor_403200_8015F9C0.placement.pos.vz = arg0->extra.tmd->coords[0].coord.t[2];
        TASK_MESSAGE_DISPATCH_POINTER(task, GAME_ACTOR_MESSAGE_PLACE, &D_actor_403200_8015F9C0.placement, 0);
        D_actor_403200_8015F8E0[0] = 1;
    }
    if (work->stateTicks < 0x18) {
        SndEvt_EnqueueType7((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000A, 1);
        TASK_MESSAGE_DISPATCH_POINTER(task, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
        work->caughtTicks = 0;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x3C);
}

/// State-change reset for the enemy's stand-up, plus the swipe tick that runs
/// on every step afterwards. The reset half is `func_actor_403200_8013B23C`'s
/// with the buffer allocator on the second walk in place of the release: it
/// clears the host model's flag word, walks the seven escorts pushing that word
/// onto each of their models, allocates the host's and every escort's buffers,
/// rebuilds the free coordinate `swipeCoord` from `neckYaw` and arms `wallDistanceTarget`
/// at 0xC80 before playing the entry cue.
///
/// Every step then clears that coordinate's flag and updates it, and each of the
/// two swipe sub-states watches one animation slot's frame: sub-state 4 raises
/// bit 0x8000 of `swipeBody.flags` and fires its two cues once
/// `hostRig.slots[1]` reaches frame 0xC, sub-state 5 clears `hostExposed` and fires its
/// single cue on `hostRig.slots[2]` frame 0x1C. Both cues are positioned on the first
/// escort's second coordinate at half depth, and whichever sub-state is live is
/// the one whose frame `prevSwipeCue` is refreshed from -- the shared mask is what
/// makes the pair one-shot. The switch on `stateTicks` arms the escort pose index
/// `limbPose` for seven states, 0x14 and 0xDC also seeding the shared countdown
/// `gGluttonLimbReach` and re-arming `state`, and the 0x29..0x2E window
/// raises that countdown by 0x258 while it is still under 0x1770.
///
/// The tail runs the per-frame body, scans the five
/// `swipeContacts` records for one whose high half is 0x10000, and -- when it finds one,
/// the enemy's HP is positive and the player's 0x3F8 query comes back zero --
/// asks the player for the hold (0x3F9) and re-sends it the animation, stamping
/// the player's `field_956` when the hold was taken. Past frame 0x39 the shared
/// countdown is walked down 0x1E, or 0xC8 once it is past 0xBB9, and past 0x15
/// the state arms `viewSelector`.
///
/// The countdown's two arms are load-bearing: the `>= 0xBB9` test reads the
/// halfword signed (`lh`) while each arm subtracts from it zero-extended
/// (`lhu`), and writing the pair as one assignment off a shared temp lets CSE
/// fold the compare onto the earlier zero-extended load, which costs an
/// `sll`/`sra` re-extension pair the original does not have.
static void func_actor_403200_8013D028(Task* arg0)
{
    GluttonWork*           work;
    GluttonWork*           escorts;
    GluttonWork*           dying;
    WorldCollisionContact* recs;
    Enemy*                 enemy;
    Task*                  task;
    Task*                  target;
    s16                    i;
    s16                    j;
    s16                    k;
    s16                    frame;
    s16                    frame2;
    s16                    reply;
    s32                    found;
    s32                    resetId;
    s32                    resetPan;
    s32                    swipeId;
    s32                    swipePan;
    s32                    swipe2Id;
    s32                    swipe2Pan;
    s32                    hitId;
    s32                    hitPan;
    s32                    cueId;
    s32                    cuePan;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    task  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
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
                escorts->escorts[i]->task->extra.tmd->flags =
                    arg0->extra.tmd->flags;
            }
        }
        dying = arg0->work;
        tmdAllocPrimitiveBuffer(arg0->extra.tmd);
        for (j = 0; j < ARRAY_SIZE(dying->escorts); j++) {
            if (dying->escorts[j] != NULL) {
                tmdAllocPrimitiveBuffer(dying->escorts[j]->task->extra.tmd);
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
        resetId                  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200017;
        resetPan                 = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(resetId, resetPan,
                                 (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }

    work->swipeCoord.node.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&work->swipeCoord.node);

    if (work->animId == 4 && (frame = work->hostRig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0xC &&
        work->prevSwipeCue != frame) {
        work->shakeLevel       = GLUTTON_SHAKE_LONG;
        work->swipeBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        Gp_SpawnPadLerp(0x30, 0xFF, 8);
        swipeId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200019;
        swipePan = (s8)worldCoordGetOriginAudioPan(
            &work->escorts[0]->task->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(
            swipeId, swipePan,
            (s8)(worldCoordGetOriginAudioDepth(
                     &work->escorts[0]->task->extra.tmd->coords[1]) /
                 2));
        swipe2Id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020001A;
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

    if (work->animId == 5 && (frame2 = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x1C &&
        work->prevSwipeCue != frame2) {
        work->hostExposed = 0;
        work->shakeLevel  = GLUTTON_SHAKE_LONG;
        Gp_SpawnPadLerp(0x20, 0x7F, 8);
        hitId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020001B;
        hitPan = (s8)worldCoordGetOriginAudioPan(
            &work->escorts[0]->task->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(
            hitId, hitPan,
            (s8)(worldCoordGetOriginAudioDepth(
                     &work->escorts[0]->task->extra.tmd->coords[1]) /
                 2));
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
            cuePan         = (s8)worldCoordGetOriginAudioPan(
                &work->escorts[0]->task->extra.tmd->coords[1]);
            sndEvtRequestScriptStart(
                cueId, cuePan,
                (s8)(worldCoordGetOriginAudioDepth(
                         &work->escorts[0]->task->extra.tmd->coords[1]) /
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

    gluttonTickAnim(arg0);

    recs = work->swipeContacts;
    for (k = 0; k < ARRAY_SIZE(work->swipeContacts); k++) {
        if (recs[k].key.value == 0) {
            goto missed;
        }
        if ((recs[k].key.value & 0xFFFF0000) == 0x10000) {
            found = 1;
            goto scanned;
        }
    }
missed:
    found = 0;
scanned:
    if (found != 0 && enemy->hp > 0 &&
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &D_actor_403200_8015FA00, 0) == 0) {
        target                 = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        reply                  = taskMessageDispatch(target, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackObjPair(enemy, 4), 0);
        work->swipeDamageReply = reply;
        if (reply == 1) {
            ((GameActor*)task->work)->state = 0xA;
        }
        work->playerAnim.source.sets = D_actor_403200_8015E6AC;
        work->playerCaught           = 1;
        work->playerAnim.animationId = 2;
        work->playerAnim.blend       = ANIMATION_BLEND_RESET;
        work->playerAnim.blendFrames = 0;
        TASK_MESSAGE_DISPATCH_POINTER(task, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
        work->caughtTicks = 0;
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

    if (work->stateTicks >= 0x15) {
        work->viewSelector = 4;
    }

    SCRATCH_STACK_RELEASE_BYTES(0x30);
}

/// State-change reset for the enemy's stand-up, and the height servo that runs
/// on every tick afterwards. The reset half is `func_actor_403200_8013B23C`'s
/// with the buffer allocator on the second walk in place of the release: it
/// clears the host model's flag word, walks the seven escorts pushing that word
/// onto each of their models, allocates the host's and every escort's buffers
/// and then parks the root coordinate at x 0x1068, y 0x7D0, z -0x1770, arming
/// `wallDistanceTarget` at 0xFA0.
///
/// The servo steps that root y by +0x50 / -0x64 while `stateTicks` is at or past
/// 0x3D, and by the gentler +0x14 / -0x1E while it is between 0x15 and 0x3D, so
/// the enemy eases back to the ground as it finishes standing up; below 0x15 it
/// stops moving. Which way each step goes is the frame's position inside its
/// group of four -- `frame % 4 < 2` on the `s16` local, whose 16-bit
/// truncation is what puts the `sll 16` / `sra 16` pair in front of the `slti`.
static void func_actor_403200_8013D78C(Task* arg0)
{
    GluttonWork* work;
    GluttonWork* escorts;
    GluttonWork* dying;
    s16          i;
    s16          j;
    s16          frame;

    work = arg0->work;
    if (work->stateChanged != 0) {
        work->animId           = 1;
        work->animStep         = GLUTTON_ANIM_STEP_RESTART;
        escorts                = arg0->work;
        escorts->freeCountdown = 0;
        arg0->extra.tmd->flags = 0;
        for (i = 0; i < ARRAY_SIZE(escorts->escorts); i++) {
            if (escorts->escorts[i] != NULL) {
                escorts->escorts[i]->task->extra.tmd->flags =
                    arg0->extra.tmd->flags;
            }
        }
        dying = arg0->work;
        tmdAllocPrimitiveBuffer(arg0->extra.tmd);
        for (j = 0; j < ARRAY_SIZE(dying->escorts); j++) {
            if (dying->escorts[j] != NULL) {
                tmdAllocPrimitiveBuffer(dying->escorts[j]->task->extra.tmd);
            }
        }
        arg0->extra.tmd->coords->coord.t[1] = 0x7D0;
        arg0->extra.tmd->coords->coord.t[0] = 0x1068;
        arg0->extra.tmd->coords->coord.t[2] = -0x1770;
        work->wallDistanceTarget            = 0xFA0;
    }
    gluttonTickAnim(arg0);
    if (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = 1;
    }
    if (arg0->extra.tmd->coords->coord.t[1] > 0) {
        frame = work->stateTicks;
        if (frame >= 0x3D) {
            arg0->extra.tmd->coords->coord.t[1] +=
                ((frame % 4) < 2) ? 0x50 : -0x64;
        } else if (frame >= 0x15) {
            arg0->extra.tmd->coords->coord.t[1] +=
                ((frame % 4) < 2) ? 0x14 : -0x1E;
        }
    }
    if (arg0->extra.tmd->coords->coord.t[1] < 0) {
        arg0->extra.tmd->coords->coord.t[1] = 0;
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// State-change reset for the enemy's stand-up. It clears the host model's flag
/// word, walks the seven escorts pushing that word onto each of their models,
/// allocates every escort's model buffers and then arms the block -- `neckYawEnabled`
/// and `neckPitchEnabled` at 1, `hostExposed` at 0, `wallDistanceTarget` at 0xC80 -- before
/// playing the type-6 cue built from the spawn record's `field_8`. Same shape as
/// `func_actor_403200_8013B23C`'s reset half, with the buffer allocator on the
/// second walk in place of the release.
///
/// The state then writes its two cue frames, and the three states at 0x39, 0x45
/// and 0x4C spawn `lastSpawned` from `gGluttonEscortTasks`; every other state
/// in the 0x39..0x4C window falls through to the dispatcher.
///
/// The `state` copy is what keeps the switch index 16-bit: switched on
/// `stateTicks - 0x39` directly the index is an `int`, and the `lh` the load
/// becomes carries the sign extension the original does with a separate
/// `sll`/`sra` pair (dropping 2 instructions and 2.8% of the match).
static void func_actor_403200_8013D9EC(Task* arg0)
{
    GluttonWork* work;
    GluttonWork* escorts;
    GluttonWork* dying;
    Enemy*       enemy;
    Enemy*       spawned;
    s16          i;
    s16          j;
    s16          state;
    s32          sfx;
    s32          pan;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateChanged != 0) {
        work->lastAttack       = 6;
        work->animId           = 6;
        work->animStep         = GLUTTON_ANIM_STEP_RESTART;
        escorts                = arg0->work;
        escorts->freeCountdown = 0;
        arg0->extra.tmd->flags = 0;
        for (i = 0; i < ARRAY_SIZE(escorts->escorts); i++) {
            if (escorts->escorts[i] != NULL) {
                escorts->escorts[i]->task->extra.tmd->flags =
                    arg0->extra.tmd->flags;
            }
        }
        dying = arg0->work;
        tmdAllocPrimitiveBuffer(arg0->extra.tmd);
        for (j = 0; j < ARRAY_SIZE(dying->escorts); j++) {
            if (dying->escorts[j] != NULL) {
                tmdAllocPrimitiveBuffer(dying->escorts[j]->task->extra.tmd);
            }
        }
        work->neckYawEnabled     = 1;
        work->neckPitchEnabled   = 1;
        work->hostExposed        = 0;
        work->wallDistanceTarget = 0xC80;
        sfx                      = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200017;
        pan                      = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(sfx, pan,
                                 (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    state = work->stateTicks - 0x39;
    switch (state) {
        case 6:
            work->animId   = 0xC;
            work->animStep = GLUTTON_ANIM_STEP_BLEND;
            break;
        case 13:
            work->animId   = 0xC;
            work->animStep = GLUTTON_ANIM_STEP_RESTART;
            break;
        case 0:
        case 12:
        case 19:
            spawned           = Gp_SpawnEnemyFromTable(gGluttonEscortTasks, 0, 0, arg0->spawnArg2.pointer);
            spawned->workType = ENEMY_WORK_PLAIN;
            work->lastSpawned = spawned;
            break;
    }
    gluttonTickAnim(arg0);
    if (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = 7;
    }
    if (work->stateTicks >= 0x15) {
        work->viewSelector = 3;
    }
}

/// The state that rains debris on the arena `func_actor_403200_8013D9EC` opens.
///
/// A reset request re-arms the block on animation 0xB, clears the host model's
/// flag word and pushes it onto each of the seven escorts' models, makes sure
/// the host and every escort has its model buffers allocated, and plays the
/// entry cue.
///
/// Sub-states 0x3B and 0x3C each fire a one-shot cue positioned on part 1 of
/// the first escort. From 0x3D on the state also drops debris: every third step
/// the shared scratch coordinate is rebuilt on that same part -- its rotation
/// accumulated up the parent chain, its origin carried into view space, then
/// turned a quarter turn each way so `gfxReadMatrixZAxis` yields the launch
/// direction, which is normalised and scaled to 0x320 before being added to the
/// origin -- and an effect is spawned on it. Every tenth step a fresh enemy is
/// spawned from `gGluttonEscortTasks` and remembered in `lastSpawned`.
///
/// The tick then runs the per-frame body and hands over to state 0xA once the
/// second animation slot raises its flag.
static void func_actor_403200_8013DC3C(Task* arg0)
{
    GluttonWork* work;
    GluttonWork* escorts;
    GluttonWork* dying;
    Enemy*       enemy;
    Enemy*       spawned;
    SVECTOR      pos;
    SVECTOR*     posp;
    s16          i;
    s16          j;
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
                escorts->escorts[i]->task->extra.tmd->flags =
                    arg0->extra.tmd->flags;
            }
        }
        dying = arg0->work;
        tmdAllocPrimitiveBuffer(arg0->extra.tmd);
        for (j = 0; j < ARRAY_SIZE(dying->escorts); j++) {
            if (dying->escorts[j] != NULL) {
                tmdAllocPrimitiveBuffer(dying->escorts[j]->task->extra.tmd);
            }
        }
        work->neckPitchEnabled   = 1;
        work->neckYawEnabled     = 1;
        work->hostExposed        = 0;
        work->wallDistanceTarget = 0xC80;
        resetId                  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200017;
        resetPan                 = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(resetId, resetPan,
                                 (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }

    if (work->stateTicks == 0x3B) {
        cueId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200016;
        cuePan = (s8)worldCoordGetOriginAudioPan(
            &work->escorts[0]->task->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(
            cueId, cuePan,
            (s8)worldCoordGetOriginAudioDepth(
                &work->escorts[0]->task->extra.tmd->coords[1]));
    }

    if (work->stateTicks == 0x3C) {
        hitId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000D;
        hitPan = (s8)worldCoordGetOriginAudioPan(
            &work->escorts[0]->task->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(
            hitId, hitPan,
            (s8)worldCoordGetOriginAudioDepth(
                &work->escorts[0]->task->extra.tmd->coords[1]));
    }

    if ((s16)(u16)work->stateTicks >= 0x3D) {
        if ((s16)((s16)(u16)work->stateTicks % 3) == 0) {
            actorAccumulateToView(
                &work->escorts[0]->task->extra.tmd->coords[1],
                &D_actor_403200_8015F920.coord);

            pos.vz = 0;
            pos.vy = 0;
            pos.vx = 0;
            actorLocalToView(&work->escorts[0]->task->extra.tmd->coords[1],
                             &pos);

            D_actor_403200_8015F920.parent     = &gGfxViewCoord;
            D_actor_403200_8015F920.coord.t[0] = pos.vx;
            D_actor_403200_8015F920.coord.t[1] = pos.vy;
            D_actor_403200_8015F920.coord.t[2] = pos.vz;
            gfxRotMatrixY(&D_actor_403200_8015F920.coord, 0x80, 0);
            gfxRotMatrixX(&D_actor_403200_8015F920.coord, -0x80, GRAPHICS_ROTATION_COMPOSE);
            gfxReadMatrixZAxis(&D_actor_403200_8015F920.coord, &pos);

            posp   = &pos;
            pos.vy = 0;
            VectorNormalSS(posp, posp);

            gte_lddp(0x320);
            gte_ldsv(posp);
            gte_gpf12();
            gte_stsv(posp);

            D_actor_403200_8015F920.coord.t[0]  += pos.vx;
            D_actor_403200_8015F920.coord.t[1]  += pos.vy;
            D_actor_403200_8015F920.coord.t[2]  += pos.vz;
            D_actor_403200_8015F920.composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(&D_actor_403200_8015F920);
            Gp_SpawnEff(EFFECT_SHELTER_B3_DUMPING_HOLE_DRIFT_SPRITE, &D_actor_403200_8015F920, 0x97A0D680, NULL);
        }
        if ((s16)((s16)(u16)work->stateTicks % 10) == 4) {
            spawned           = Gp_SpawnEnemyFromTable(gGluttonEscortTasks, 2, 0, arg0->spawnArg2.pointer);
            spawned->workType = ENEMY_WORK_PLAIN;
            work->lastSpawned = spawned;
        }
    }

    gluttonTickAnim(arg0);

    if (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = 0xA;
        SndEvt_EnqueueType7((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000D, 1);
    }

    if (work->stateTicks >= 0x15) {
        work->viewSelector = 5;
    }
}

static void func_actor_403200_8013E2FC(Task* arg0)
{
    GluttonWork* work;
    GluttonWork* escorts;
    GluttonWork* dying;
    GfxMatrix*   mtx;
    GfxCoord*    coords;
    s32          state;
    s32          frame;
    s16          i;
    s16          j;

    work = arg0->work;
    if (work->stateChanged != 0) {
        escorts                = arg0->work;
        work->freeCountdown    = 0;
        arg0->extra.tmd->flags = 0;
        for (i = 0; i < ARRAY_SIZE(escorts->escorts); i++) {
            if (escorts->escorts[i] != NULL) {
                escorts->escorts[i]->task->extra.tmd->flags =
                    arg0->extra.tmd->flags;
            }
        }
        dying = arg0->work;
        tmdAllocPrimitiveBuffer(arg0->extra.tmd);
        for (j = 0; j < ARRAY_SIZE(dying->escorts); j++) {
            if (dying->escorts[j] != NULL) {
                tmdAllocPrimitiveBuffer(dying->escorts[j]->task->extra.tmd);
            }
        }
        work->animRate           = 0x10;
        work->viewLocked         = 1;
        work->limbPose           = 0;
        work->wallDistanceTarget = 0xFA0;
    }
    if (work->animId == 0xD) {
        frame = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (frame == 0x15 && work->clip.prevSlot2Cue != frame) {
            work->shakeLevel = GLUTTON_SHAKE_LONG;
            Gp_SpawnScript18(D_actor_403200_80141C6C, D_actor_403200_80141C74);
        }
        work->clip.prevSlot2Cue = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    }
    if (work->animId == 9 && work->stateTicks == 0x2D) {
        coords                                                    = arg0->extra.tmd->coords;
        D_actor_403200_8015F970.packed.coord.rotationWords.m00M01 = ONE;
        mtx                                                       = &D_actor_403200_8015F970.packed.coord;
        mtx->rotationWords.m02M10                                 = 0;
        mtx->rotationWords.m11M12                                 = ONE;
        mtx->rotationWords.m20M21                                 = 0;
        mtx->rotationWords.m22                                    = ONE;
        D_actor_403200_8015F970.node.coord.t[1]                   = -0x64;
        D_actor_403200_8015F970.node.coord.t[0]                   = 0;
        D_actor_403200_8015F970.node.coord.t[2]                   = 0x64;
        D_actor_403200_8015F970.node.composeStamp                 = GRAPHICS_COORD_DIRTY;
        D_actor_403200_8015F970.node.parent                       = &coords[4];
        actorRenderComposeCoord(&D_actor_403200_8015F970.node);
    }
    state = work->animId;
    if (state == 0x14) {
        if (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
            work->animId   = 0xD;
            work->animStep = GLUTTON_ANIM_STEP_BLEND;
        }
        if (work->animId == state && work->animStep == GLUTTON_ANIM_STEP_RESTART) {
            work->animRate = 0x60;
            gluttonTickAnim(arg0);
            while ((u32)(work->hostRig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) < 0x34) {
                gluttonTickAnim(arg0);
            }
            work->animRate = 0x10;
        }
    }
    if (gGluttonLimbReach >= 0x191) {
        gGluttonLimbReach = (u16)gGluttonLimbReach - 0xC8;
    }
    gluttonTickAnim(arg0);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// State 0x12, the enemy's death sequence: the model is torn down and rebuilt
/// so the collapse animation can run on it.
///
/// A reset request clears the host model's `field_C` and pushes the cleared
/// word onto each of the seven escorts' own model objects, allocates the host's
/// and every escort's model buffers, forces `collapseSkip / 4` extra per-frame
/// steps -- stopping early once `hostRig.slots[1].flags` bit 0 is set -- and then re-arms the
/// animation slot at 0x10, plays the type-7 death cue and leaves the yaw target
/// at 0xFA0 and the escort pose cleared.
///
/// The rest of the tick winds the shared `gGluttonLimbReach` counter down
/// by 0xC8 once it has passed 0x1F4, runs the per-frame body, clears the host
/// coordinate's rebuild flag, and on frame 0x1C of `hostRig.slots[2]` arms the screen
/// shake at level 3 and spawns the `D_actor_403200_80141C5C` script pair. While
/// `animId` is still 0x12 four one-shot cues fire on frames 0x33, 0x3D, 0x4E
/// and 0x71 of `hostRig.slots[3]`, each latching the frame it saw in `prevSlot3Cue`.
static void func_actor_403200_8013E5A8(Task* arg0)
{
    GluttonWork* work;
    GluttonWork* escorts;
    GluttonWork* dying;
    Enemy*       enemy;
    Enemy*       obj;
    s16          i;
    s16          j;
    s32          state;
    s32          frame;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateChanged != 0) {
        obj                    = arg0->spawnArg2.pointer;
        escorts                = arg0->work;
        work->freeCountdown    = 0;
        arg0->extra.tmd->flags = 0;
        for (i = 0; i < ARRAY_SIZE(escorts->escorts); i++) {
            if (escorts->escorts[i] != NULL) {
                escorts->escorts[i]->task->extra.tmd->flags =
                    arg0->extra.tmd->flags;
            }
        }
        dying = arg0->work;
        tmdAllocPrimitiveBuffer(arg0->extra.tmd);
        for (j = 0; j < ARRAY_SIZE(dying->escorts); j++) {
            if (dying->escorts[j] != NULL) {
                tmdAllocPrimitiveBuffer(dying->escorts[j]->task->extra.tmd);
            }
        }
        work->animRate         = 0x40;
        work->neckPitchEnabled = 0;
        work->neckYawEnabled   = 0;
        j                      = 0;
        while (j < work->collapseSkip / 4) {
            gluttonTickAnim(arg0);
            j++;
            if (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
                break;
            }
        }
        work->viewSelector = 7;
        work->animRate     = 0x10;
        SndEvt_EnqueueType7((((u16)obj->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000A, 1);
        work->limbPose           = 0;
        work->wallDistanceTarget = 0xFA0;
    }
    if (gGluttonLimbReach >= 0x1F5) {
        gGluttonLimbReach = (u16)gGluttonLimbReach - 0xC8;
    }
    gluttonTickAnim(arg0);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    state                                 = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    if (state == 0x1C && work->clip.prevSlot2Cue != state) {
        work->shakeLevel = GLUTTON_SHAKE_LONG;
        Gp_SpawnScript18(D_actor_403200_80141C5C, D_actor_403200_80141C64);
    }
    work->clip.prevSlot2Cue = work->hostRig.slots[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    if (work->animId == 0x12) {
        frame = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (frame == 0x33 && work->prevSlot3Cue != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200013;
            pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            sndEvtRequestScriptStart(id, pan,
                                     (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        }
        frame = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (frame == 0x3D && work->prevSlot3Cue != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200003;
            pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            sndEvtRequestScriptStart(id, pan,
                                     (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        }
        frame = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (frame == 0x4E && work->prevSlot3Cue != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200014;
            pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            sndEvtRequestScriptStart(id, pan,
                                     (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        }
        frame = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (frame == 0x71 && work->prevSlot3Cue != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200015;
            pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            sndEvtRequestScriptStart(id, pan,
                                     (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        }
        work->prevSlot3Cue = work->hostRig.slots[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    }
}

/// The enemy's attack-launch body: when the dispatcher has flagged the state
/// change it re-arms the work block (`limbPose` at 3, `wallDistanceTarget` at 0xC80) and
/// plays the two launch cues -- a type-6 with the object's pan and depth, then
/// type-7s for ids 0x0D and 0x09 -- and otherwise runs the per-frame body,
/// winding the shared `gGluttonLimbReach` counter down by 0xC8 once it has
/// passed 0x190 and clearing `viewSelector` once `stateTicks` has passed 0x14.
static void func_actor_403200_8013E9C0(Task* arg0)
{
    GluttonWork* work;
    Enemy*       obj;
    s32          state;
    s32          id;
    s32          pan;

    work = arg0->work;
    if (work->stateChanged != 0) {
        obj                    = arg0->spawnArg2.pointer;
        state                  = work->animId;
        work->neckPitchEnabled = 0;
        work->neckYawEnabled   = 0;
        work->hostExposed      = 1;
        if (state != 0xD) {
            work->animStep = GLUTTON_ANIM_STEP_BLEND;
            work->animId   = 0xD;
        } else {
            work->animStep = GLUTTON_ANIM_STEP_RESTART;
            work->animId   = state;
        }
        id  = (((u16)obj->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200004;
        pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(id, pan,
                                 (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->limbPose           = 3;
        work->wallDistanceTarget = 0xC80;
        SndEvt_EnqueueType7((((u16)obj->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000D, 1);
        SndEvt_EnqueueType7((((u16)obj->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200009, 1);
        return;
    }
    SCRATCH_STACK_RESERVE_BYTES(0xC);
    gluttonTickAnim(arg0);
    if (gGluttonLimbReach >= 0x191) {
        gGluttonLimbReach = (u16)gGluttonLimbReach - 0xC8;
        work->limbPose    = 0;
    }
    if (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->state = 0xA;
    }
    if (work->stateTicks >= 0x15) {
        work->viewSelector = 0;
    }
    SCRATCH_STACK_RELEASE_BYTES(0xC);
}

/// State-selecting tick of the enemy's approach: on the tick the dispatcher has
/// flagged a state change it re-arms the work block -- the two flags, the
/// stagger countdown at 0x28, the yaw target at 0xE10 and the animation slot at
/// 0x10 -- and winds the shared `gGluttonLimbReach` counter down by 0xC8
/// once it has passed 0x190.
///
/// It then runs the per-frame body and aims the enemy at the player: the
/// player's root coordinate minus the part's own translation gives the pair
/// `ratan2` turns into a yaw, taken relative to the part's facing the same way
/// the group-0 hit handler does it, and the result is wrapped to +/-0x800 into
/// `neckYawTarget`. `gGluttonEnded` holding `stateTicks` at zero makes the
/// per-frame body's animation re-arm win the next tick.
///
/// Once the `attackDelay` stagger countdown has run out it walks the three
/// `phase` sub-states, in which the player-relative range and the enemy's
/// remaining HP pick the next state, and a roll of `gRandomLcgState` breaks the tie
/// between the two strafing states; the state already in `lastAttack` is never
/// re-selected twice in a row. A positive heal counter in `pendingHeals` overrides
/// all of it with the heal state 0xF.
///
/// The x range that sub-state 0 tests is the player-relative offset taken
/// again from the two models, not `playerDistance`: the two share only the frame, and the y test
/// carries the -0xFA the z one carries +0x25F, the offsets the hit handler puts
/// on the same pair.
static void func_actor_403200_8013EB64(Task* arg0)
{
    _Actor403200IdleScratch* sc;
    GluttonWork*             work;
    Enemy*                   enemy;
    Task*                    player;
    GfxCoord*                coord;
    GfxCoord*                facing;
    SVECTOR*                 toPlayer;
    s16                      angle;

    work   = arg0->work;
    enemy  = arg0->spawnArg2.pointer;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);

    if (work->stateChanged != 0) {
        work->neckYawEnabled   = 1;
        work->neckPitchEnabled = 1;
        work->neckPitchTarget  = 0;
        if (work->attackDelay == 0) {
            work->attackDelay = 0x28;
        }
        work->wallDistanceTarget = 0xE10;
        work->animId             = 1;
        work->animStep           = GLUTTON_ANIM_STEP_BLEND;
        work->hostExposed        = 0;
        work->animRate           = 0x10;
    }
    if (gGluttonLimbReach >= 0x191) {
        gGluttonLimbReach = (u16)gGluttonLimbReach - 0xC8;
        work->limbPose    = 0;
    }
    sc = SCRATCH_STACK_RESERVE_BLOCK(_Actor403200IdleScratch);
    gluttonTickAnim(arg0);

    coord        = arg0->extra.tmd->coords;
    toPlayer     = &sc->toPlayer;
    toPlayer->vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    toPlayer->vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    toPlayer->vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    facing       = arg0->extra.tmd->coords;
    angle        = ratan2(toPlayer->vx, toPlayer->vz) -
            ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    if (angle < 0) {
    wrapUp:
        if (angle < -0x800) {
            angle += 0x1000;
            goto wrapUp;
        }
    } else {
    wrapDown:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto wrapDown;
        }
    }
    work->neckYawTarget = angle;
    if (gGluttonEnded == 1) {
        work->stateTicks = 0;
    }
    if (work->attackDelay <= work->stateTicks) {
        sc->rangeOffset.vx = player->extra.tmd->coords->coord.t[0] -
                             arg0->extra.tmd->coords->coord.t[0];
        sc->rangeOffset.vy = (player->extra.tmd->coords->coord.t[1] -
                              arg0->extra.tmd->coords->coord.t[1]) -
                             0xFA;
        sc->rangeOffset.vz = (player->extra.tmd->coords->coord.t[2] -
                              arg0->extra.tmd->coords->coord.t[2]) +
                             0x25F;
        sc->playerDistance = SquareRoot0(sc->rangeOffset.vx * sc->rangeOffset.vx + sc->rangeOffset.vy * sc->rangeOffset.vy +
                                         sc->rangeOffset.vz * sc->rangeOffset.vz);
        switch (work->phase) {
            case 0:
                if (enemy->hp < 0x5DC) {
                    work->state = 9;
                } else if (player->extra.tmd->coords->coord.t[0] -
                               arg0->extra.tmd->coords->coord.t[0] >=
                           0x2711) {
                    work->state = 2;
                } else if (work->lastAttack != 3) {
                    work->state = 3;
                } else {
                    work->state = 2;
                }
                break;
            case 1:
                if (enemy->hp < 0x320) {
                    work->state = 9;
                } else {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    if (((gRandomLcgState >> 16) & 0xF) == 0) {
                        work->state = 3;
                    } else if (sc->playerDistance >= 0x20D1) {
                        if (work->lastAttack == 6) {
                            work->state = 2;
                        } else {
                            work->state = 6;
                        }
                    } else if (sc->playerDistance >= 0x189D) {
                        if (work->lastAttack == 7) {
                            work->state = 2;
                        } else {
                            work->state = 7;
                        }
                    } else {
                        work->state = 2;
                    }
                }
                break;
            case 2:
                if (sc->playerDistance >= 0x2329) {
                    if (work->lastAttack == 2) {
                        work->state = 6;
                    } else {
                        work->state = 2;
                    }
                } else if (work->lastAttack == 2) {
                    work->state = 0xB;
                } else {
                    work->state = 2;
                }
                break;
        }
        if ((s8)work->pendingHeals > 0) {
            work->state = 0xF;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403200IdleScratch);
}

/// Summon tick of the arena fight. While `gGluttonEnded` is 1
/// the whole body is skipped; otherwise it reserves a
/// `GluttonSummonScratch` on the scratch stack.
///
/// On the dispatcher's re-arm tick it tops the two `summons` slots back up
/// to two live summons (`summonsAlive` < 2 and `summonsSpawned` < 8), dresses each
/// model from the current area record's fourth placement, stamps the slot
/// index into `Enemy::placeKey`, and plays the two type-7 launch cues.
/// Every later tick yaws the host at the player, and at `stateTicks` 0x46 / 0x78
/// it sends summon 0 or 1 a 0x7DB order whose action is picked from
/// `phase` and a coin flip of `gRandomLcgState`.
static void func_actor_403200_8013EF6C(Task* arg0)
{
    GluttonSummonScratch* sc;
    GluttonWork*          work;
    Enemy*                host;
    Enemy*                escort;
    PlayerStatus*         cfg;
    GfxCoord*             coord;
    GfxCoord*             facing;
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
    s16                   angle;
    s16                   sel;
    u32                   frame;

    work = arg0->work;
    host = arg0->spawnArg2.pointer;
    if (gGluttonEnded != 1) {
        sc = SCRATCH_STACK_RESERVE_BLOCK(GluttonSummonScratch);
        if (work->stateChanged != 0) {
            state                  = work->animId;
            work->neckPitchEnabled = 0;
            work->neckYawEnabled   = 1;
            work->hostExposed      = 0;
            if (state != 0x13) {
                work->animId   = 0x13;
                work->animStep = GLUTTON_ANIM_STEP_BLEND;
            } else {
                work->animStep = GLUTTON_ANIM_STEP_RESTART;
                work->animId   = state;
            }
            work->animRate = 0x10;
            for (sc->slot = 0; sc->slot < 2; sc->slot++) {
                if (work->summons[sc->slot] == NULL && work->summonsAlive < 2 && (u8)work->summonsSpawned < 8) {
                    work->summons[sc->slot] = Gp_SpawnEnemyFromTable(&D_actor_341700_80174D58, 3, 2, NULL);
                    if (work->summons[sc->slot] != NULL) {
                        work->summonsSpawned++;
                        model      = work->summons[sc->slot]->task->extra.tmd;
                        sessionKey = &gGameSession->location.loc;
                        key.stage  = sessionKey->stage;
                        key.area   = sessionKey->area;
                        key.room   = sessionKey->room;
                        key.view   = sessionKey->view;
                        areaSyncLocationVariant(&key);
                        entry                    = &Gp_GetNestedAreaRec(&key)->placements[3];
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
            SndEvt_EnqueueType7((((u16)host->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000D, 1);
            SndEvt_EnqueueType7((((u16)host->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200009, 1);
        }
        if (gGluttonLimbReach >= 0x191) {
            gGluttonLimbReach = (u16)gGluttonLimbReach - 0xC8;
        }
        gluttonTickAnim(arg0);
        if (work->animId == 0x13 && (frame = work->hostRig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 4 && frame < 0xD) {
            work->hostExposed = 1;
        } else {
            work->hostExposed = 0;
        }
        cfg             = &gPlayerStatus;
        coord           = arg0->extra.tmd->coords;
        sc->toPlayer.vx = cfg->coordMtx->t[0] - coord->coord.t[0];
        sc->toPlayer.vy = cfg->coordMtx->t[1] - coord->coord.t[1];
        sc->toPlayer.vz = cfg->coordMtx->t[2] - coord->coord.t[2];
        facing          = arg0->extra.tmd->coords;
        angle           = ratan2(sc->toPlayer.vx, sc->toPlayer.vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
        if (angle < 0) {
        wrapUp:
            if (angle < -0x800) {
                angle += 0x1000;
                goto wrapUp;
            }
        } else {
        wrapDown:
            if (angle > 0x800) {
                angle -= 0x1000;
                goto wrapDown;
            }
        }
        work->neckYawTarget = angle;
        if ((work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) && work->animId == 0x13) {
            work->animId   = 1;
            work->animStep = GLUTTON_ANIM_STEP_BLEND;
        }
        if (work->stateTicks >= 0x14B || (work->animId == 1 && gSceneCombatState.battleRefs == 1)) {
            work->state = 3;
        }
        if (work->stateTicks == 6) {
            cueId  = (((u16)host->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200004;
            cuePan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            sndEvtRequestScriptStart(cueId, cuePan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        }
        if (work->stateTicks == 0x3B) {
            blastId  = (((u16)host->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200010;
            blastPan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            sndEvtRequestScriptStart(blastId, blastPan, (s8)(worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords) / 2));
            work->shakeLevel = GLUTTON_SHAKE_LONG;
            Gp_SpawnScript18(D_actor_403200_80141C5C, D_actor_403200_80141C64);
        }
        if (work->stateTicks == 0x23) {
            work->viewSelector = 0;
        }
        if (work->stateTicks == 0x46) {
            sc->slot = 0;
            goto dispatch;
        }
        if (work->stateTicks == 0x78) {
            sc->slot = 1;
        dispatch:
            if (work->summons[sc->slot] != NULL) {
                D_actor_403200_8015F8F4.context.loc.stage = 0;
                D_actor_403200_8015F8F4.context.loc.area  = 0x2C;
                sel                                       = work->phase;
                if (sel == 1) {
                    goto L_case1;
                }
                if (sel >= 2) {
                    goto L_default;
                }
                if (sel != 0) {
                    goto L_default;
                }
                if (sc->slot == 0) {
                    gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    if (!((gRandomLcgState >> 16) & 1)) {
                        D_actor_403200_8015F8F4.command = 3;
                    } else {
                        D_actor_403200_8015F8F4.command = 4;
                    }
                } else {
                    gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    if (!((gRandomLcgState >> 16) & 1)) {
                        D_actor_403200_8015F8F4.command = 9;
                    } else {
                        D_actor_403200_8015F8F4.command = 0xA;
                    }
                }
                goto L_join;
            L_case1:
                if (sc->slot == 0) {
                    gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    if (!((gRandomLcgState >> 16) & 1)) {
                        D_actor_403200_8015F8F4.command = 0xA;
                    } else {
                        D_actor_403200_8015F8F4.command = 0xB;
                    }
                } else {
                    gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    if (!((gRandomLcgState >> 16) & 1)) {
                        D_actor_403200_8015F8F4.command = 4;
                    } else {
                        D_actor_403200_8015F8F4.command = 5;
                    }
                }
                goto L_join;
            L_default:
                if (sc->slot == 0) {
                    D_actor_403200_8015F8F4.command = 5;
                } else {
                    D_actor_403200_8015F8F4.command = 0xB;
                }
            L_join:
                D_actor_403200_8015F8F4.command <<= 8;
                rnd                               = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                D_actor_403200_8015F8F4.command  |= (s16)(((((u32)rnd >> 16) % 3) * 0x10) | 1);
                gRandomLcgState                   = rnd;
                TASK_MESSAGE_DISPATCH_POINTER(work->summons[sc->slot]->task, ACTOR_COMMAND_MESSAGE_APPLY, &D_actor_403200_8015F8F4, 0);
            }
        }
    out:
        SCRATCH_STACK_RELEASE_BLOCK(GluttonSummonScratch);
    }
}

#include "../../shared/glutton_escort_state.inc.c"

/// The host's state handlers, indexed by `state`; the last six slots are
/// empty. Two of the handlers take no argument and are called through the
/// table's type anyway.
static const _Actor403200StateTable D_actor_403200_80132154 = {
    {
        func_actor_403200_8013B23C,
        func_actor_403200_80141B40,
        func_actor_403200_8013B3C8,
        func_actor_403200_8013B8C4,
        func_actor_403200_8013D78C,
        func_actor_403200_8013E2FC,
        func_actor_403200_8013D9EC,
        func_actor_403200_8013DC3C,
        func_actor_403200_8013E9C0,
        func_actor_403200_80134D40,
        func_actor_403200_8013EB64,
        func_actor_403200_8013D028,
        func_actor_403200_8013E5A8,
        func_actor_403200_8013C84C,
        func_actor_403200_8013EF6C,
        gluttonEscortState,
        func_actor_403200_8014122C,
        func_actor_403200_80141234,
        func_actor_403200_8014123C,
    },
};

/// The host task's three states -- spawn/setup, per-frame tick and teardown --
/// dispatched through by state.
static const EnemyTaskFuncTable3 D_actor_403200_801321B8 = {
    {
        func_actor_403200_80138AFC,
        func_actor_403200_8013FB54,
        enemyDestroy,
    },
};

/// Per-frame tick for the enemy task. Updates the host coordinate, hides or
/// shows the escorts, and either returns on the cinematic mode byte or runs
/// the hit handlers, the death handoff and the state in `state`.
static void func_actor_403200_8013FB54(Enemy* arg0, Task* arg1)
{
    VECTOR                   pos;
    _Actor403200StateTable   states;
    GluttonWork*             work;
    GluttonWork*             dying;
    GluttonWork*             vis;
    _Actor403200TickScratch* scratch;
    WorldCollisionTrigger*   pending;
    TmdObject*               tmd;
    TmdObject*               escortTmd;
    Task*                    player;
    Task*                    slot3;
    Enemy*                   colorEnemy;
    s16                      i;
    s16                      j;
    s16                      k;
    s16                      mode;
    u16                      count;
    u8                       viewReady;
    s32                      d801153f4;
    u8                       stateF0;
    SVECTOR*                 pendingPos;
    s8                       nodeFlags;
    s32                      t2;

    work   = arg1->work;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    states = D_actor_403200_80132154;

    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(arg1->extra.tmd->coords);

    dying = arg1->work;
    if (dying->freeCountdown != 0) {
        arg1->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        dying->freeCountdown--;
        if (dying->freeCountdown == 0) {
            tmd         = arg1->extra.tmd;
            tmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            tmdFreePrimitiveBuffer(arg1->extra.tmd);
            for (i = 0; i < ARRAY_SIZE(dying->escorts); i++) {
                if (dying->escorts[i] != NULL) {
                    escortTmd         = dying->escorts[i]->task->extra.tmd;
                    escortTmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
                    tmdFreePrimitiveBuffer(dying->escorts[i]->task->extra.tmd);
                }
            }
        }
    }

    pos.vx = arg1->extra.tmd->coords[3].workm.t[0];
    pos.vy = arg1->extra.tmd->coords[3].workm.t[1];
    pos.vz = arg1->extra.tmd->coords[3].workm.t[2];

    slot3 = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (slot3->extra.tmd->coords->coord.t[1] > 0) {
        slot3->extra.tmd->coords->coord.t[1] = 0;
    }
    t2 = slot3->extra.tmd->coords->coord.t[2];
    if (t2 >= -0xED7) {
        slot3->extra.tmd->coords->coord.t[2] = -0xED8;
    } else if (t2 < -0x206C) {
        slot3->extra.tmd->coords->coord.t[2] = -0x206C;
    }

    if (work->hostExposed != work->prevHostExposed) {
        Gp_UpdateActorColor(arg0, &pos, 0, 0);
        Gp_UpdateActorColor(work->escorts[3], &pos, 0, 0);
        work->prevHostExposed = work->hostExposed;
    }
    colorEnemy = arg0;
    if (work->hostExposed == 0) {
        colorEnemy = work->escorts[3];
    }
    Gp_UpdateActorColor(colorEnemy, &pos, 0, 0);

    if (work->state == 0xB && gGluttonLimbReach >= 0x7D1) {
        work->escorts[4]->task->extra.tmd->otOffset = -8;
    } else {
        work->escorts[4]->task->extra.tmd->otOffset = 0;
    }

    if (((Gp_GetViewIndex() & 0xFF) == 0x1E) || ((Gp_GetViewIndex() & 0xFF) == 0x1D)) {
        vis                    = arg1->work;
        vis->freeCountdown     = 0;
        arg1->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        for (j = 0; j < ARRAY_SIZE(vis->escorts); j++) {
            if (vis->escorts[j] != NULL) {
                vis->escorts[j]->task->extra.tmd->flags =
                    arg1->extra.tmd->flags;
            }
        }
    } else if (work->state != 0) {
        vis                    = arg1->work;
        vis->freeCountdown     = 0;
        arg1->extra.tmd->flags = 0;
        for (k = 0; k < ARRAY_SIZE(vis->escorts); k++) {
            if (vis->escorts[k] != NULL) {
                vis->escorts[k]->task->extra.tmd->flags =
                    arg1->extra.tmd->flags;
            }
        }
    }

    d801153f4 = gSceneCombatState.actorControl;
    if (d801153f4 == 1) {
        goto clear_and_return;
    }
    if (d801153f4 < 2) {
        goto after_mode;
    }
    if (d801153f4 != 2) {
        goto after_mode;
    }
clear_and_return:
    Gp_ClearRec18Occupied(work->hits[0].contacts);
    Gp_ClearRec18Occupied(work->hits[1].contacts);
    Gp_ClearRec18Occupied(work->hits[2].contacts);
    Gp_ClearRec18Occupied(work->hits[3].contacts);
    Gp_ClearRec18Occupied(work->hits[4].contacts);
    Gp_ClearRec18Occupied(work->hits[5].contacts);
    Gp_ClearRec18Occupied(work->hits[6].contacts);
    Gp_ClearRec18Occupied(work->hits[7].contacts);
    Gp_ClearRec18Occupied(work->hits[8].contacts);
    Gp_ClearRec18Occupied(work->swipeContacts);
    return;
after_mode:

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor403200TickScratch);

    if (arg0->hp > 0) {
        if (work->playerCaught != 1 && work->state != 0xD) {
            if (work->groups1To2Cooldown > 0) {
                work->groups1To2Cooldown = (s16)((u16)work->groups1To2Cooldown - 1);
            } else {
                gluttonHitGroups1To2(arg1);
            }
            if (work->group0Cooldown > 0) {
                work->group0Cooldown = (s16)((u16)work->group0Cooldown - 1);
            } else {
                gluttonHitGroup0(arg1);
            }
            if (work->groups3To5Cooldown > 0) {
                work->groups3To5Cooldown = (s16)((u16)work->groups3To5Cooldown - 1);
            } else {
                func_actor_403200_8013A4A0(arg1);
            }
            if (work->groups6To8Cooldown > 0) {
                work->groups6To8Cooldown = (s16)((u16)work->groups6To8Cooldown - 1);
            } else {
                gluttonHitGroups6To8(arg1);
            }
        }
    }
    if (arg0->hp <= 0) {
        if (gPlayerStatus.hp <= 0) {
            arg0->hp      = 1;
            gGluttonEnded = 0;
        }
    }

    if (work->summonsAlive > 0) {
        work->deathDelay = 0x26;
    }
    if (work->deathDelay > 0 && work->summonsAlive == 0) {
        work->deathDelay = (s16)((u16)work->deathDelay - 1);
    }

    if (arg0->hp <= 0) {
        if (work->deathDelay > 0) {
            arg0->hp = 1;
        }
        if (arg0->hp <= 0 && gGluttonEnded == 0) {
            gGluttonEnded                             = 1;
            D_actor_403200_8015F8F4.context.loc.stage = 0;
            D_actor_403200_8015F8F4.context.loc.area  = 0x2C;
            D_actor_403200_8015F8F4.command           = 3;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_actor_403200_8015F8F4, ACTOR_COMMAND_MESSAGE_APPLY);
            gGluttonSpinnersReleased = 0;
        }
    }

    if (gGluttonEnded == 1 && work->deathDelay <= 0 && work->playerCaught == 0 &&
        work->deathTicks < 0x100) {
        work->deathTicks = (s16)((u16)work->deathTicks + 1);
    }

    if (work->deathTicks == 8) {
        work->state                  = 0x12;
        gSceneCombatState.battleRefs = 1;
        // Dumping Hole room variant 1 installs ten contiguous pending quads.
        pending              = Gp_PendingObj4C;
        pending[9].origin.vx = arg1->extra.tmd->coords->coord.t[0] + 0xFA0;
        pendingPos           = &pending[9].origin;
        pendingPos->vy       = arg1->extra.tmd->coords->coord.t[1] - 0x64;
        pendingPos->vz       = arg1->extra.tmd->coords->coord.t[2];
        SndEvt_EnqueueType7((((u16)arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000A, 1);
        SndEvt_EnqueueType7((((u16)arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000D, 1);
    }

    if (arg0->hp <= 0) {
        stateF0 = gSceneCombatState.signals.bytes.battlePhase;
        if (stateF0 == 2) {
            work->viewSelector = stateF0;
            work->phase        = 3;
            work->viewLocked   = 0;
        }
    }

    if (work->prevState != work->state) {
        work->stateChanged = 1;
        work->stateTicks   = 0;
    } else {
        if (work->stateTicks < 0x7FFF) {
            work->stateTicks = (s16)((u16)work->stateTicks + 1);
        }
        work->stateChanged = 0;
    }

    work->prevState = (u16)work->state;
    if (work->state != 0 && work->state != 5 && work->state != 0xC && work->state != 3 &&
        work->spinnersSpawned == 0) {
        viewReady = gGameSession->viewReady;
        if (viewReady == 1 && work->state != 0 && work->state != 5) {
            work->spinnersSpawned = viewReady;
            func_actor_403200_8013B740(arg1);
        }
    }

    states.funcs[work->state](arg1);

    if ((u16)work->state < 2 || work->state == 5 || work->state == 0xC) {
        nodeFlags                                = 1;
        arg0->node.state.parts.flags             = WORLD_TARGET_NOT_LOCKABLE;
        work->escorts[3]->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->escorts[0]->node.state.parts.flags = nodeFlags;
        work->escorts[1]->node.state.parts.flags = nodeFlags;
    } else if (work->hostExposed != 0) {
        if (Gp_NodeSlotMask(&work->escorts[3]->node) != 0) {
            Gp_AssignNodeSlot0(&arg0->node);
        }
        arg0->node.state.parts.flags             = WORLD_TARGET_HIDE_HP;
        work->escorts[3]->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
        work->escorts[0]->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
        work->escorts[1]->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
    } else {
        if (Gp_NodeSlotMask(&arg0->node) != 0) {
            Gp_AssignNodeSlot0(&work->escorts[3]->node);
        }
        arg0->node.state.parts.flags             = WORLD_TARGET_NOT_LOCKABLE;
        nodeFlags                                = 8;
        work->escorts[3]->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->escorts[0]->node.state.parts.flags = nodeFlags;
        work->escorts[1]->node.state.parts.flags = nodeFlags;
    }

    if (work->state != 0 && work->hostExposed == 1) {
        work->hits[0].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->hits[0].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    if (work->state != 0 && work->hostExposed != 1) {
        work->hits[1].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->hits[2].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->hits[1].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->hits[2].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
    if (work->state != 0) {
        work->hits[3].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->hits[4].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->hits[5].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->hits[3].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->hits[4].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->hits[5].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    if (work->state != 0) {
        work->hits[6].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->hits[7].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->hits[8].body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->hits[6].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->hits[7].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->hits[8].body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }

    Gp_ClearRec18Occupied(work->hits[0].contacts);
    Gp_ClearRec18Occupied(work->hits[1].contacts);
    Gp_ClearRec18Occupied(work->hits[2].contacts);
    Gp_ClearRec18Occupied(work->hits[3].contacts);
    Gp_ClearRec18Occupied(work->hits[4].contacts);
    Gp_ClearRec18Occupied(work->hits[5].contacts);
    Gp_ClearRec18Occupied(work->hits[6].contacts);
    Gp_ClearRec18Occupied(work->hits[7].contacts);
    Gp_ClearRec18Occupied(work->hits[8].contacts);
    Gp_ClearRec18Occupied(work->swipeContacts);
    gluttonShakeTick(arg1);

    if (work->viewLocked == 0 && work->state != 0) {
        scratch->view = D_actor_403200_8015E6E8[work->viewSelector](arg1, work->phase);
        if (((Gp_GetViewIndex() & 0xFF) != scratch->view) &&
            (arg1->spawnArg1.value >> 16) == 0) {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = scratch->view;
        }
    }

    mode = work->playerCaught;
    if (mode == 1 && work->state != 0xD) {
        count             = work->caughtTicks + 1;
        work->caughtTicks = count;
        if (work->swipeDamageReply == mode) {
            if (work->playerAnim.animationId == 2) {
                work->playerAnim.source.sets = D_actor_403200_8015E6AC;
                work->playerAnim.blend       = ANIMATION_BLEND_RESET;
                work->playerAnim.blendFrames = 0;
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
                work->caughtTicks = 0;
            }
        } else if (work->playerAnim.animationId == 2 && (s16)count < 0x28) {
            work->playerAnim.source.sets = D_actor_403200_8015E6AC;
            work->playerAnim.blend       = ANIMATION_BLEND_RESET;
            work->playerAnim.blendFrames = 0;
            TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
        }

        if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
            switch (work->playerAnim.animationId) {
                case 2:
                    if (work->swipeDamageReply != 1 && (s16)work->caughtTicks >= 0x17) {
                        work->playerAnim.source.sets = D_actor_403200_8015E6AC;
                        D_actor_403200_8015E6AC[4] =
                            (Gp_PlayerAnimBlkTbl
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

    if (gGluttonEnded == 1) {
        if (work->playerCaught == gGluttonEnded) {
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 2, 0);
            work->playerCaught = 0;
        }
    }

    SCRATCH_STACK_RELEASE_BLOCK(_Actor403200TickScratch);
}

static void func_actor_403200_801408D8(Task* task, s16 scale, s16 drop, s16 index)
{
    SVECTOR                 dir;
    SVECTOR                 normal;
    SVECTOR*                pool  = Gp_GridParams->normals;
    SVECTOR*                verts = Gp_GridParams->vertices;
    WorldCollisionGridFace* faces = Gp_GridParams->faces;
    WorldCollisionGridFace  face  = {
        { index * 4, index * 4 + 1, index * 4 + 2, index * 4 + 3 }, index, 3
    };
    WorldCollisionGridFace face2 = {
        { (index + 1) * 4, (index + 1) * 4 + 1, (index + 1) * 4 + 2, (index + 1) * 4 + 3 }, index + 1, 3
    };
    SVECTOR* d;

    gfxReadMatrixZAxis(&task->extra.tmd->coords->coord, &normal);
    Gfx_MatrixCol0(&task->extra.tmd->coords->coord, &dir);
    d = &dir;
    VectorNormalSS(d, d);
    VectorNormalSS(&normal, &normal);
    gte_lddp(scale);
    gte_ldsv(&normal);
    gte_gpf12();
    gte_stsv(&normal);
    gte_lddp(0xBB8);
    gte_ldsv(d);
    gte_gpf12();
    gte_stsv(d);

    verts[index * 4].vx = verts[index * 4 + 2].vx =
        task->extra.tmd->coords->coord.t[0] + dir.vx + normal.vx;
    verts[index * 4].vy = verts[index * 4 + 2].vy =
        task->extra.tmd->coords->coord.t[1] + dir.vy + normal.vy;
    verts[index * 4].vz = verts[index * 4 + 2].vz =
        task->extra.tmd->coords->coord.t[2] + dir.vz + normal.vz;

    verts[index * 4 + 1].vx = verts[index * 4 + 3].vx =
        task->extra.tmd->coords->coord.t[0] + normal.vx;
    verts[index * 4 + 1].vy = verts[index * 4 + 3].vy =
        task->extra.tmd->coords->coord.t[1] + normal.vy;
    verts[index * 4 + 1].vz = verts[index * 4 + 3].vz =
        task->extra.tmd->coords->coord.t[2] + normal.vz;

    verts[(index + 1) * 4].vx = verts[(index + 1) * 4 + 2].vx =
        task->extra.tmd->coords->coord.t[0] + normal.vx;
    verts[(index + 1) * 4].vy = verts[(index + 1) * 4 + 2].vy =
        task->extra.tmd->coords->coord.t[1] + normal.vy;
    verts[(index + 1) * 4].vz = verts[(index + 1) * 4 + 2].vz =
        task->extra.tmd->coords->coord.t[2] + normal.vz;

    verts[(index + 1) * 4 + 1].vx = verts[(index + 1) * 4 + 3].vx =
        task->extra.tmd->coords->coord.t[0] + normal.vx - dir.vx;
    verts[(index + 1) * 4 + 1].vy = verts[(index + 1) * 4 + 3].vy =
        task->extra.tmd->coords->coord.t[1] + normal.vy - dir.vy;
    verts[(index + 1) * 4 + 1].vz = verts[(index + 1) * 4 + 3].vz =
        task->extra.tmd->coords->coord.t[2] + normal.vz - dir.vz;

    gte_lddp(0x3E8);
    gte_ldsv(&normal);
    gte_gpf12();
    gte_stsv(&normal);

    verts[index * 4].vx = verts[index * 4 + 2].vx += normal.vx;
    verts[index * 4].vy = verts[index * 4 + 2].vy += normal.vy;
    verts[index * 4].vz = verts[index * 4 + 2].vz += normal.vz;
    verts[index * 4 + 5].vx = verts[index * 4 + 7].vx += normal.vx;
    verts[index * 4 + 5].vy = verts[index * 4 + 7].vy += normal.vy;
    verts[index * 4 + 5].vz = verts[index * 4 + 7].vz += normal.vz;

    pool[index].vz = verts[index * 4].vx - verts[index * 4 + 1].vx;
    pool[index].vy = verts[index * 4 + 1].vy - verts[index * 4].vy;
    pool[index].vx = verts[index * 4 + 1].vz - verts[index * 4].vz;
    VectorNormalSS(&pool[index], &pool[index]);

    (&pool[index])[1].vz = verts[(index + 1) * 4].vx - verts[(index + 1) * 4 + 1].vx;
    (&pool[index])[1].vy = verts[(index + 1) * 4 + 1].vy - verts[(index + 1) * 4].vy;
    (&pool[index])[1].vx = verts[(index + 1) * 4 + 1].vz - verts[(index + 1) * 4].vz;
    VectorNormalSS(&(&pool[index])[1], &(&pool[index])[1]);

    verts[index * 4].vy     -= drop;
    verts[index * 4 + 1].vy -= drop;
    verts[index * 4 + 4].vy -= drop;
    verts[index * 4 + 5].vy -= drop;

    face.surfaceClass  = 3;
    face2.surfaceClass = 3;
    faces[index]       = face;
    faces[index + 1]   = face2;
}

/// The enemy's upkeep tick, run by the dispatcher through the same
/// `D_actor_403200_801321B8` table the other tasks in this overlay use. It drops
/// each of the two `summons` whose HP has run out, then walks the work
/// block's `wallDistance` toward `wallDistanceTarget` by 0x32 a tick -- snapping once the
/// two are within 0x33 -- calls the follow helper with the new value, and
/// finally lifts the host's own X up to the escort's so the party never sinks
/// below the enemy. The tick ends by dispatching on `state` through the local
/// copy of the handler table.
void func_actor_403200_80140E6C(Task* arg0)
{
    EnemyTaskFuncTable3 sp;
    GluttonWork*        work;
    Task*               player;
    Enemy*              enemy;
    s32                 diff;
    s32                 y;
    GfxCoord*           playerCoord;
    GfxCoord*           selfCoord;

    sp     = D_actor_403200_801321B8;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    work   = arg0->work;
    enemy  = arg0->spawnArg2.pointer;
    if (work != NULL) {
        if (work->summons[0] != NULL && work->summons[0]->hp <= 0) {
            work->summons[0] = NULL;
        }
        if (work->summons[1] != NULL && work->summons[1]->hp <= 0) {
            work->summons[1] = NULL;
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
        func_actor_403200_801408D8(arg0, work->wallDistance, work->wallDrop, 0);
        playerCoord = player->extra.tmd->coords;
        selfCoord   = arg0->extra.tmd->coords;
        y           = selfCoord->coord.t[0] + work->wallDistance;
        if (playerCoord->coord.t[0] < y) {
            playerCoord->coord.t[0] = y;
        }
    }
    sp.funcs[arg0->state](enemy, arg0);
}

#include "../../shared/glutton_quad_heights.inc.c"

#include "../../shared/glutton_exit.inc.c"

#include "../../shared/glutton_shake_level.inc.c"

#include "../../shared/glutton_set_spinners_released.inc.c"

#include "../../shared/glutton_get_spinners_released.inc.c"

s32 func_actor_403200_80141124(Task* arg0, s16 arg1)
{
    switch (arg1) {
        case 0:
            return 0x13;
        case 1:
            return 7;
        case 2:
            return 0x25;
    }
    return 1;
}

s32 func_actor_403200_80141180(Task* arg0, s16 arg1)
{
    return (s16)func_actor_403200_801344C4(arg0, arg1);
}

/// Returns 0x25 for the current view, or 0x1E when the slot-3 model's X
/// translation is at or above a threshold that depends on the view index:
/// 0x3A98 for view 0x1E, 0x3E80 otherwise.
s32 func_actor_403200_801411A8(Task* arg0, s16 arg1)
{
    Task* task;
    s32   flag;
    s32   value;
    s32   view;

    view = Gp_GetViewIndex() & 0xFF;
    task = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (view == 0x1E) {
        flag  = task->extra.tmd->coords->coord.t[0];
        flag  = flag < 0x3A98;
        value = 0x25;
    } else {
        flag  = task->extra.tmd->coords->coord.t[0];
        flag  = flag < 0x3E80;
        value = 0x25;
    }
    if (flag == 0) {
        value = 0x1E;
    }
    return value;
}

static void func_actor_403200_8014122C(Task* arg0)
{
}

static void func_actor_403200_80141234(Task* arg0)
{
}

/// The state handler `D_actor_403200_80132154` lists for state 8. Re-arms the
/// sub-state counter if the dispatcher saw a state change this tick, runs the
/// per-frame body, and on the tick the counter reaches 8 tells the player's
/// task (message 0x13F4) and plays the actor's cue.
static void func_actor_403200_8014123C(Task* arg0)
{
    GluttonWork* work;
    Enemy*       enemy;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateChanged != 0) {
        work->stateTicks = 0;
    }
    gluttonTickAnim(arg0);
    if (work->stateTicks == 8) {
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), ROOM_MESSAGE_ACTOR_EVENT, 0, 0);
        SndEvt_EnqueueType7(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000A, 1);
    }
}

#include "../../shared/glutton_prop_setup.inc.c"

#include "../../shared/glutton_prop_tick.inc.c"

#include "../../shared/glutton_prop_task.inc.c"

/// A further copy, under this file's own name.
#define gluttonPropTask func_actor_403200_8014148C
#include "../../shared/glutton_prop_task.inc.c"
#undef gluttonPropTask

#include "../../shared/glutton_throw_task.inc.c"

#include "../../shared/glutton_glob_task.inc.c"

#include "../../shared/glutton_chunk_task.inc.c"

#include "../../shared/glutton_rain_task.inc.c"

#include "../../shared/glutton_spinner_wait.inc.c"

#include "../../shared/glutton_spinner_task.inc.c"

s32 func_actor_403200_8014196C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
}

#include "../../shared/actor_messages_is_present.inc.c"

#include "../../shared/actor_messages_place.inc.c"

/// Per-frame upkeep for the enemy, dispatched by `arg2`: state 0 bumps the
/// heal counter, files a negative "damage" with `func_800DA6E8` so the HUD
/// shows it as a heal, and tops the enemy's HP back up by 0x64; state 1 ticks
/// `summonsAlive` down and, once it has run out, re-arms the enemy's
/// `deathDelay`. Same body as `func_actor_444000_80143E68` without its tracked
/// `summons` slots.
s32 func_actor_403200_80141A94(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    GluttonWork* work  = arg0->work;
    Enemy*       enemy = arg0->spawnArg2.pointer;

    switch (arg2) {
        case 0:
            work->pendingHeals++;
            func_800DA6E8(&enemy->node, -0x64, 0);
            if (enemy->hp > 0) {
                enemy->hp += 0x64;
            }
            break;
        case 1:
            if (work->summonsAlive > 0) {
                work->summonsAlive--;
                if (work->summonsAlive > 0) {
                    break;
                }
            }
            work->deathDelay = 2;
            break;
    }
    return 1;
}

s32 func_actor_403200_80141B30(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    gGluttonGrabActive = 0;
    return 1;
}

/// State-change reset: once the dispatcher has flagged the change in
/// `stateChanged`, drop the re-arm marker and push the host model's `field_C` onto
/// every live escort's own model object. Same body as
/// `func_actor_444000_80143F4C`.
static void func_actor_403200_80141B40(Task* arg0)
{
    GluttonWork* work;
    GluttonWork* escorts;
    s16          i;

    work = arg0->work;
    if (work->stateChanged != 0) {
        escorts                = arg0->work;
        work->freeCountdown    = 0;
        arg0->extra.tmd->flags = 0;
        for (i = 0; i < ARRAY_SIZE(escorts->escorts); i++) {
            if (escorts->escorts[i] != NULL) {
                escorts->escorts[i]->task->extra.tmd->flags =
                    arg0->extra.tmd->flags;
            }
        }
        work->neckPitchEnabled = 0;
        work->neckYawEnabled   = 0;
    } else {
        gluttonTickAnim(arg0);
    }
}
