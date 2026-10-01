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
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
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
#include "../../shared/glutton.h"

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).

/// Work block of the overlay's event/controller task -- the one
/// `D_actor_444000_80161860` points at, which is a different and much smaller
/// block than the enemy's `GluttonWork` above.
///
/// `func_actor_444000_80132358` allocates it with `memCalloc(0x34, 0)`,
/// `Mem_Set`s 0x34 bytes and parks it in that task's `Task::work` slot, so
/// the size is anchored; the same function stores the `gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)`
/// task in `field_20` and publishes its owning task in
/// `D_actor_444000_80161860`. `field_20` is the target of every
/// `Gp_DispatchMsg` the leaf helpers send, and they null-check it first
/// (`func_actor_444000_801321FC`). `field_2C` is the action index
/// `func_actor_444000_80132054` switches on, with `field_2E` the sub-state
/// counter reset alongside it. `field_2A` is a one-shot flag guarding the sound
/// cue `func_actor_444000_80132608` enqueues.
typedef struct Actor444000EventWork {
    /* 0x00 */ byte  pad_0[0x20];
    /* 0x20 */ Task* field_20; // gameGetTaskSlot(GAME_TASK_SLOT_PLAYER) task, the Gp_DispatchMsg target
    /* 0x24 */ Task* field_24; // subordinate task, killed and cleared by func_actor_444000_80132694
                               /// Area-record id published to `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view` on every enter/re-enter. The
                               /// spawn state writes it as a halfword, clearing the byte at 0x29 with it,
                               /// while every reader takes the low byte, so both views are named.
    /* 0x28 */ union {
        u8  b;
        s16 h;
    } field_28;
    /* 0x2A */ u16  field_2A; // one-shot flag: set once func_actor_444000_80132608 has played its cue
    /* 0x2C */ u16  field_2C; // action index, switched on by func_actor_444000_80132054
    /* 0x2E */ s16  field_2E; // cleared whenever field_2C is set
    /* 0x30 */ u16  field_30; // one-shot flag: set once func_actor_444000_80132778 has armed the death sequence
    /* 0x32 */ byte pad_32[0x2];
} Actor444000EventWork;
STATIC_ASSERT_SIZEOF(Actor444000EventWork, 0x34);

/// Scratchpad frame `func_actor_444000_8013482C` carves off the scratch stack
/// for the run-out / turn / run-back pass. `dir` is first the offset from the
/// model to the player, whose yaw against the model's own facing becomes
/// `GluttonWork::field_7C4`, and later the normalised, GPF-scaled step the
/// turn adds to the coordinate; `m` is the working copy of the model's root
/// coordinate and `angle` the yaw `gfxRotMatrixY` rebuilds it from.
typedef struct Actor444000RunScratch {
    /* 0x00 */ SVECTOR    dir;
    /* 0x08 */ OverlayMat m;
    /* 0x28 */ s16        pad_28;
    /* 0x2A */ s16        angle;
} Actor444000RunScratch;
STATIC_ASSERT_SIZEOF(Actor444000RunScratch, 0x2C);

/// Work block of the enemy dispatched through `gGluttonChunkStates` --
/// named for that table because the creature itself is not identified yet.
/// `gluttonChunkSpawn` allocates it with `memCalloc(0x1C0, 0)` and
/// parks it in that task's `Task::work` slot, so the size is anchored rather
/// than guessed.
///
/// The two named fields are the pair the dispatcher
/// `gluttonChunkTask` keeps: `field_1B4` is the state it last ran and
/// `field_1A8` the flag it sets when that state has changed since.
typedef struct Actor444000F0CWork {
    /* 0x000 */ byte pad_0[0x1A8];
    /* 0x1A8 */ s16  field_1A8; // set when the dispatcher sees the state change, cleared when it has not
    /* 0x1AA */ byte pad_1AA[0xA];
    /* 0x1B4 */ s16  field_1B4; // the state the dispatcher last ran, so it can spot the change
    /* 0x1B6 */ byte pad_1B6[0xA];
} Actor444000F0CWork;
STATIC_ASSERT_SIZEOF(Actor444000F0CWork, 0x1C0);

/// 0x4C-byte scratchpad frame `func_actor_444000_8013EC84` carves off
/// the scratch stack for the escort-order tick. `delta` is the player-relative
/// offset in the arena plane whose length is `dist` -- under 0xB54 the player is
/// dragged back along `dir` to a fixed range -- and `pos` is the host's fifth
/// part carried into view space, which the yaw `angle` and the final message
/// 0x3E9 placement are both built from. `dir` doubles as `VectorNormalSS`'s
/// workspace throughout.
typedef struct Actor444000WarpScratch {
    /* 0x00 */ SVECTOR dir;
    /* 0x08 */ SVECTOR pos;
    /* 0x10 */ VECTOR  delta;
    /* 0x20 */ MATRIX  m;
    /* 0x40 */ s32     dist;  // length of `delta`, in world units
    /* 0x44 */ byte    pad_44[0x6];
    /* 0x4A */ s16     angle; // yaw handed to the placement, wrapped to +/-0x800
} Actor444000WarpScratch;
STATIC_ASSERT_SIZEOF(Actor444000WarpScratch, 0x4C);

/// 0x4C-byte scratchpad frame `func_actor_444000_8013E058` takes off the
/// scratch-pad stack for the drag tick. `dir` starts as the player-relative offset in
/// the arena plane, is carried into view space, renormalised and then scaled by
/// the per-frame pull the animation frame selects; `push` is the same vector as
/// the 32-bit triple `func_80105B74` copies onto the player actor, `dist` is the
/// offset's length and `pull` / `period` are the frame-derived strength and the
/// script-spawn interval the current phase uses.
typedef struct Actor444000DragScratch {
    /* 0x00 */ VECTOR3 push;
    /* 0x0C */ byte    pad_C[0x4];
    /* 0x10 */ SVECTOR dir;
    /* 0x18 */ byte    pad_18[0x20];
    /* 0x38 */ s32     dist;   // length of `dir` before it is normalised
    /* 0x3C */ byte    pad_3C[0x8];
    /* 0x44 */ s16     pull;   // phase offset folded into the gpf scale
    /* 0x46 */ s16     i;      // escort slot being cleared, 0 or 1
    /* 0x48 */ s16     period; // frames between script spawns
    /* 0x4A */ byte    pad_4A[0x2];
} Actor444000DragScratch;
STATIC_ASSERT_SIZEOF(Actor444000DragScratch, 0x4C);

/// The overlay's event/controller task, whose `work` holds an
/// `Actor444000EventWork`.
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

/// Spawn tables `func_800E8634` forwards to `Task_Spawn`, taken as raw
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

/// The contact routines' scratch position.
static inline SVECTOR* ActorContact_GetScratchPosition(void)
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
/// Script pair the drag tick spawns every `period` frames.
extern PadScriptCmd              D_actor_444000_80144A94[3];
extern PadScriptVibrationSegment D_actor_444000_80144AA0[2];

/// Animation-set tables: the host's two blocks, escort 0's two and escort 1's.
extern AnimationSet* D_actor_444000_80161448[];
extern AnimationSet* D_actor_444000_80161500[];
extern AnimationSet* D_actor_444000_801615B8[];
/// The enemy task's message-handler table, parked in `Task::msgTable`.
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*);
        s32 (*call1)(Task*, s32, ActorCommand* request);
        s32 (*call2)(Task*, s32, ActorTransform*);
        s32 (*call3)(Task*, s32, s32);
    } handler;
} Actor444000MessageEntry;
STATIC_ASSERT_SIZEOF(Actor444000MessageEntry, 8);

extern Actor444000MessageEntry D_actor_444000_80161818[7];
/// Spawn table of the seven escorts, indexed 0..6.
extern TaskDesc D_actor_444000_801616B0[];
/// Effect argument block the spawn state points at the host's root coordinate.
extern EffectSpawnArg D_actor_444000_80161880;
/// Shared 0x7DA payload buffer, also used by `func_actor_444000_80141618`.
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    ActorCommand value;
    u8           retained[4];
} Actor444000Storage1888;
STATIC_ASSERT_SIZEOF(Actor444000Storage1888, 8);

extern Actor444000Storage1888 D_actor_444000_80161888;
/// Gameplay's escort `TaskDesc` table; entry 3 is the pair this boss spawns.
extern TaskDesc D_80172604;

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
/// Reply buffer the hold state hands message 0x3F8.
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    GpDelayArg value;
    u8         retained[8];
} Actor444000Storage1898;
STATIC_ASSERT_SIZEOF(Actor444000Storage1898, 32);

extern Actor444000Storage1898 gGluttonGrabQuery;

/// World point the spinner chases: written by `func_actor_444000_8013E058`,
/// read by the spinner's tick as the target of its step.
extern SVECTOR gGluttonSpinnerTarget;

/// Shared coordinate `func_actor_444000_80140BBC` rebuilds when the fight
/// reaches sub-state 0x2D of state 9, parented to the host model's fifth part.
extern GluttonDropCoord D_actor_444000_801618B8;

/// Which of the three shared debris coordinates below the next launch uses,
/// cycled 0/1/2 by `func_actor_444000_801404C0`.
extern s16 D_actor_444000_80161850;
/// The three coordinates that debris effects are spawned on, each rebuilt in
/// view space from the first escort's second part.
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    GfxCoord value[6];
    u8       retained[40];
} Actor444000Storage1948;
STATIC_ASSERT_SIZEOF(Actor444000Storage1948, 520);

extern Actor444000Storage1948 D_actor_444000_80161948;
/// Spawn table of the enemy the arena fight drops in every tenth step.
extern TaskDesc gGluttonEscortTasks[];

/// The animation-set table the fight installs on the player through message
/// 0x3FF; entry 4 is rebuilt from the player's own weapon block before the
/// second (`field_4 == 4`) send.
/// The companion table used instead when the player is more than a quarter turn
/// off the host's facing, so the hold plays from the other side.
/// Set while the escort-order tick holds the player at a placement of its own;
/// 1 marks the plain re-placement, 0 the full grab.
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    s8 value;
    u8 retained[7];
} Actor444000Storage1868;
STATIC_ASSERT_SIZEOF(Actor444000Storage1868, 8);

extern Actor444000Storage1868 D_actor_444000_80161868;
/// Shared message 0x3E9 placement payload the escort-order tick sends slot 3.
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    ActorTransform value;
    u8             retained[8];
} Actor444000Storage1908;
STATIC_ASSERT_SIZEOF(Actor444000Storage1908, 32);

extern Actor444000Storage1908 D_actor_444000_80161908;
/// Reply buffer the fight hands message 0x3F8 before asking for the hold.
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    GpDelayArg value;
    u8         retained[8];
} Actor444000Storage1928;
STATIC_ASSERT_SIZEOF(Actor444000Storage1928, 32);

extern Actor444000Storage1928 D_actor_444000_80161928;

/// Global game-mode byte; sits inside a small flag block, so it is declared as
/// an array -- the load has to keep aliasing the scratch stores beside it (see
/// DECOMPILATION_LEARNINGS.md, "Declare a fixed-address global as an array").

/// Per-animation reset argument, a `[?][0x2D]` table of `field_7B3` indexed by
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
s32         func_actor_444000_80143D68(Task* arg0);
s32         func_actor_444000_80143F38(Task* arg0);
static void func_actor_444000_80143F4C(Task* arg0);

extern AnimationSet D_actor_444000_80153724;
extern AnimationSet D_actor_444000_80153904;
extern AnimationSet D_actor_444000_80153AE4;
extern AnimationSet D_actor_444000_80153ECC;
extern AnimationSet D_actor_444000_8015428C;
extern AnimationSet D_actor_444000_801545A0;
extern AnimationSet D_actor_444000_80154984;
extern AnimationSet D_actor_444000_80154DDC;
extern AnimationSet D_actor_444000_80155204;
extern AnimationSet D_actor_444000_801555B0;
extern AnimationSet D_actor_444000_801558B8;
extern AnimationSet D_actor_444000_80155BA0;
extern AnimationSet D_actor_444000_80155E08;
extern AnimationSet D_actor_444000_80155ED4;
extern AnimationSet D_actor_444000_80155FA0;
extern AnimationSet D_actor_444000_80156278;
extern AnimationSet D_actor_444000_801564BC;
extern AnimationSet D_actor_444000_801566FC;
extern AnimationSet D_actor_444000_80156904;
extern AnimationSet D_actor_444000_80156A90;
extern AnimationSet D_actor_444000_80156C24;
extern AnimationSet D_actor_444000_8015705C;
extern AnimationSet D_actor_444000_801573A0;
extern AnimationSet D_actor_444000_801576B8;

extern AnimationSet D_actor_444000_80158540;
extern AnimationSet D_actor_444000_801587D4;
extern AnimationSet D_actor_444000_80158AE0;
extern AnimationSet D_actor_444000_80159BA4;
extern AnimationSet D_actor_444000_8015A740;
extern AnimationSet D_actor_444000_8015A848;
extern AnimationSet D_actor_444000_8015A928;
extern AnimationSet D_actor_444000_8015AA08;
extern AnimationSet D_actor_444000_8015AB10;
extern AnimationSet D_actor_444000_8015ABD8;
extern AnimationSet D_actor_444000_8015ACA0;
extern AnimationSet D_actor_444000_8015AD98;
extern AnimationSet D_actor_444000_8015AE60;
extern AnimationSet D_actor_444000_8015AF28;
extern AnimationSet D_actor_444000_8015B660;
extern AnimationSet D_actor_444000_8015CAAC;
extern AnimationSet D_actor_444000_8015DF80;

extern AnimationSet D_actor_444000_8015E944;
extern AnimationSet D_actor_444000_8015F150;
extern TmdSource    D_actor_444000_8014BE0C;

extern TmdSource D_actor_444000_8014E220;
extern TmdSource D_actor_444000_80150A40;
extern TmdSource D_actor_444000_801528DC;

extern TmdSource D_actor_444000_80146F68;
s32              func_actor_444000_8013A958(Task*, s32, s32);
s32              func_actor_444000_8013ACD0(Task*, s32, ActorCommand* msg);
s32              func_actor_444000_80143D68(Task*);
s32              func_actor_444000_80143D7C(Task*, s32, ActorTransform* placement);
s32              func_actor_444000_80143E68(Task*, s32, s32);
s32              func_actor_444000_80143F38(Task*);
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

AnimationPackedPose D_actor_444000_80144008[6] = {
#include "assets/actor_444000_animation_124C4_bank1.inc"
};

AnimationPackedRotation D_actor_444000_80144050[46] = {
#include "assets/actor_444000_animation_124C4_bank4.inc"
};

AnimationRecord D_actor_444000_80144108[109] = {
#include "assets/actor_444000_animation_124C4_records.inc"
};

u16 D_actor_444000_801442BC[20] = {
#include "assets/actor_444000_animation_124C4_indices.inc"
};

AnimationSet D_actor_444000_801442E4 = {
    D_actor_444000_80144108,
    D_actor_444000_801442BC,
    { NULL, D_actor_444000_80144008, NULL, NULL, D_actor_444000_80144050, NULL, NULL, NULL },
};

AnimationSet* D_actor_444000_8014430C[4] = {
    &D_actor_444000_801442E4,
    &D_actor_444000_80160368,
    &D_actor_444000_801608A0,
    &D_actor_444000_80160C34,
};

EvsCommand D_actor_444000_8014431C[19] = {
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_444000_801327E8 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_444000_801327E8 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_444000_801321FC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_444000_80132778 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_444000_801327E8 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_444000_8013265C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_444000_801327E8 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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

TmdBone D_actor_444000_80144AA8[8] = {
#include "assets/actor_444000_model_15148_skeleton.inc"
};

u32 D_actor_444000_80144BC8[8] = {
#include "assets/actor_444000_model_15148_partVerts.inc"
};

SVECTOR D_actor_444000_80144BE8[135] = {
#include "assets/actor_444000_model_15148_verts.inc"
};

SVECTOR D_actor_444000_80145020[135] = {
#include "assets/actor_444000_model_15148_normals.inc"
};

u32 D_actor_444000_80145458[1732] = {
#include "assets/actor_444000_model_15148_stream.inc"
};

TmdSource D_actor_444000_80146F68 = {
    0,
    8496,
    3588,
    8,
    D_actor_444000_80144BC8,
    D_actor_444000_80144BE8,
    D_actor_444000_80145020,
    D_actor_444000_80144AA8,
    D_actor_444000_80145458,
};

TmdBone D_actor_444000_80146F8C[1] = {
#include "assets/actor_444000_model_15EF0_skeleton.inc"
};

u32 D_actor_444000_80146FB0[1] = {
#include "assets/actor_444000_model_15EF0_partVerts.inc"
};

SVECTOR D_actor_444000_80146FB4[82] = {
#include "assets/actor_444000_model_15EF0_verts.inc"
};

SVECTOR D_actor_444000_80147244[79] = {
#include "assets/actor_444000_model_15EF0_normals.inc"
};

u32 D_actor_444000_801474BC[533] = {
#include "assets/actor_444000_model_15EF0_stream.inc"
};

TmdSource D_actor_444000_80147D10 = {
    0,
    3696,
    0,
    1,
    D_actor_444000_80146FB0,
    D_actor_444000_80146FB4,
    D_actor_444000_80147244,
    D_actor_444000_80146F8C,
    D_actor_444000_801474BC,
};

TmdBone D_actor_444000_80147D34[1] = {
#include "assets/actor_444000_model_17074_skeleton.inc"
};

u32 D_actor_444000_80147D58[1] = {
#include "assets/actor_444000_model_17074_partVerts.inc"
};

SVECTOR D_actor_444000_80147D5C[90] = {
#include "assets/actor_444000_model_17074_verts.inc"
};

SVECTOR D_actor_444000_8014802C[112] = {
#include "assets/actor_444000_model_17074_normals.inc"
};

u32 D_actor_444000_801483AC[698] = {
#include "assets/actor_444000_model_17074_stream.inc"
};

TmdSource D_actor_444000_80148E94 = {
    0,
    4808,
    0,
    1,
    D_actor_444000_80147D58,
    D_actor_444000_80147D5C,
    D_actor_444000_8014802C,
    D_actor_444000_80147D34,
    D_actor_444000_801483AC,
};

TmdBone D_actor_444000_80148EB8[4] = {
#include "assets/actor_444000_model_18830_skeleton.inc"
};

u32 D_actor_444000_80148F48[4] = {
#include "assets/actor_444000_model_18830_partVerts.inc"
};

SVECTOR D_actor_444000_80148F58[104] = {
#include "assets/actor_444000_model_18830_verts.inc"
};

SVECTOR D_actor_444000_80149298[115] = {
#include "assets/actor_444000_model_18830_normals.inc"
};

u32 D_actor_444000_80149630[1032] = {
#include "assets/actor_444000_model_18830_stream.inc"
};

TmdSource D_actor_444000_8014A650 = {
    0,
    6172,
    1048,
    4,
    D_actor_444000_80148F48,
    D_actor_444000_80148F58,
    D_actor_444000_80149298,
    D_actor_444000_80148EB8,
    D_actor_444000_80149630,
};

TmdBone D_actor_444000_8014A674[4] = {
#include "assets/actor_444000_model_19FEC_skeleton.inc"
};

u32 D_actor_444000_8014A704[4] = {
#include "assets/actor_444000_model_19FEC_partVerts.inc"
};

SVECTOR D_actor_444000_8014A714[104] = {
#include "assets/actor_444000_model_19FEC_verts.inc"
};

SVECTOR D_actor_444000_8014AA54[115] = {
#include "assets/actor_444000_model_19FEC_normals.inc"
};

u32 D_actor_444000_8014ADEC[1032] = {
#include "assets/actor_444000_model_19FEC_stream.inc"
};

TmdSource D_actor_444000_8014BE0C = {
    0,
    6172,
    1048,
    4,
    D_actor_444000_8014A704,
    D_actor_444000_8014A714,
    D_actor_444000_8014AA54,
    D_actor_444000_8014A674,
    D_actor_444000_8014ADEC,
};

TmdBone D_actor_444000_8014BE30[8] = {
#include "assets/actor_444000_model_1B37C_skeleton.inc"
};

u32 D_actor_444000_8014BF50[8] = {
#include "assets/actor_444000_model_1B37C_partVerts.inc"
};

SVECTOR D_actor_444000_8014BF70[82] = {
#include "assets/actor_444000_model_1B37C_verts.inc"
};

SVECTOR D_actor_444000_8014C200[82] = {
#include "assets/actor_444000_model_1B37C_normals.inc"
};

u32 D_actor_444000_8014C490[835] = {
#include "assets/actor_444000_model_1B37C_stream.inc"
};

TmdSource D_actor_444000_8014D19C = {
    0,
    4140,
    1872,
    8,
    D_actor_444000_8014BF50,
    D_actor_444000_8014BF70,
    D_actor_444000_8014C200,
    D_actor_444000_8014BE30,
    D_actor_444000_8014C490,
};

TmdBone D_actor_444000_8014D1C0[1] = {
#include "assets/actor_444000_model_1B7DC_skeleton.inc"
};

u32 D_actor_444000_8014D1E4[1] = {
#include "assets/actor_444000_model_1B7DC_partVerts.inc"
};

SVECTOR D_actor_444000_8014D1E8[22] = {
#include "assets/actor_444000_model_1B7DC_verts.inc"
};

SVECTOR D_actor_444000_8014D298[22] = {
#include "assets/actor_444000_model_1B7DC_normals.inc"
};

u32 D_actor_444000_8014D348[173] = {
#include "assets/actor_444000_model_1B7DC_stream.inc"
};

TmdSource D_actor_444000_8014D5FC = {
    0,
    1136,
    0,
    1,
    D_actor_444000_8014D1E4,
    D_actor_444000_8014D1E8,
    D_actor_444000_8014D298,
    D_actor_444000_8014D1C0,
    D_actor_444000_8014D348,
};

TmdBone D_actor_444000_8014D620[1] = {
#include "assets/actor_444000_model_1C400_skeleton.inc"
};

u32 D_actor_444000_8014D644[1] = {
#include "assets/actor_444000_model_1C400_partVerts.inc"
};

SVECTOR D_actor_444000_8014D648[70] = {
#include "assets/actor_444000_model_1C400_verts.inc"
};

u32 D_actor_444000_8014D878[618] = {
#include "assets/actor_444000_model_1C400_stream.inc"
};

TmdSource D_actor_444000_8014E220 = {
    0,
    3536,
    0,
    1,
    D_actor_444000_8014D644,
    D_actor_444000_8014D648,
    &D_actor_444000_8014D648[70],
    D_actor_444000_8014D620,
    D_actor_444000_8014D878,
};

TmdBone D_actor_444000_8014E244[1] = {
#include "assets/actor_444000_model_1CF58_skeleton.inc"
};

u32 D_actor_444000_8014E268[1] = {
#include "assets/actor_444000_model_1CF58_partVerts.inc"
};

SVECTOR D_actor_444000_8014E26C[101] = {
#include "assets/actor_444000_model_1CF58_verts.inc"
};

SVECTOR D_actor_444000_8014E594[20] = {
#include "assets/actor_444000_model_1CF58_normals.inc"
};

u32 D_actor_444000_8014E634[465] = {
#include "assets/actor_444000_model_1CF58_stream.inc"
};

TmdSource D_actor_444000_8014ED78 = {
    0,
    3904,
    0,
    1,
    D_actor_444000_8014E268,
    D_actor_444000_8014E26C,
    D_actor_444000_8014E594,
    D_actor_444000_8014E244,
    D_actor_444000_8014E634,
};

TmdBone D_actor_444000_8014ED9C[1] = {
#include "assets/actor_444000_model_1DAA0_skeleton.inc"
};

u32 D_actor_444000_8014EDC0[1] = {
#include "assets/actor_444000_model_1DAA0_partVerts.inc"
};

SVECTOR D_actor_444000_8014EDC4[101] = {
#include "assets/actor_444000_model_1DAA0_verts.inc"
};

SVECTOR D_actor_444000_8014F0EC[20] = {
#include "assets/actor_444000_model_1DAA0_normals.inc"
};

u32 D_actor_444000_8014F18C[461] = {
#include "assets/actor_444000_model_1DAA0_stream.inc"
};

TmdSource D_actor_444000_8014F8C0 = {
    0,
    3904,
    0,
    1,
    D_actor_444000_8014EDC0,
    D_actor_444000_8014EDC4,
    D_actor_444000_8014F0EC,
    D_actor_444000_8014ED9C,
    D_actor_444000_8014F18C,
};

TmdBone D_actor_444000_8014F8E4[1] = {
#include "assets/actor_444000_model_1DFE0_skeleton.inc"
};

u32 D_actor_444000_8014F908[1] = {
#include "assets/actor_444000_model_1DFE0_partVerts.inc"
};

SVECTOR D_actor_444000_8014F90C[54] = {
#include "assets/actor_444000_model_1DFE0_verts.inc"
};

u32 D_actor_444000_8014FABC[209] = {
#include "assets/actor_444000_model_1DFE0_stream.inc"
};

TmdSource D_actor_444000_8014FE00 = {
    0,
    1576,
    0,
    1,
    D_actor_444000_8014F908,
    D_actor_444000_8014F90C,
    &D_actor_444000_8014F90C[54],
    D_actor_444000_8014F8E4,
    D_actor_444000_8014FABC,
};

TmdBone D_actor_444000_8014FE24[1] = {
#include "assets/actor_444000_model_1E350_skeleton.inc"
};

u32 D_actor_444000_8014FE48[1] = {
#include "assets/actor_444000_model_1E350_partVerts.inc"
};

SVECTOR D_actor_444000_8014FE4C[36] = {
#include "assets/actor_444000_model_1E350_verts.inc"
};

u32 D_actor_444000_8014FF6C[129] = {
#include "assets/actor_444000_model_1E350_stream.inc"
};

TmdSource D_actor_444000_80150170 = {
    0,
    944,
    0,
    1,
    D_actor_444000_8014FE48,
    D_actor_444000_8014FE4C,
    &D_actor_444000_8014FE4C[36],
    D_actor_444000_8014FE24,
    D_actor_444000_8014FF6C,
};

TmdBone D_actor_444000_80150194[1] = {
#include "assets/actor_444000_model_1EC20_skeleton.inc"
};

u32 D_actor_444000_801501B8[1] = {
#include "assets/actor_444000_model_1EC20_partVerts.inc"
};

SVECTOR D_actor_444000_801501BC[58] = {
#include "assets/actor_444000_model_1EC20_verts.inc"
};

SVECTOR D_actor_444000_8015038C[58] = {
#include "assets/actor_444000_model_1EC20_normals.inc"
};

u32 D_actor_444000_8015055C[313] = {
#include "assets/actor_444000_model_1EC20_stream.inc"
};

TmdSource D_actor_444000_80150A40 = {
    0,
    2176,
    0,
    1,
    D_actor_444000_801501B8,
    D_actor_444000_801501BC,
    D_actor_444000_8015038C,
    D_actor_444000_80150194,
    D_actor_444000_8015055C,
};

TmdBone D_actor_444000_80150A64[1] = {
#include "assets/actor_444000_model_20ABC_skeleton.inc"
};

u32 D_actor_444000_80150A88[1] = {
#include "assets/actor_444000_model_20ABC_partVerts.inc"
};

SVECTOR D_actor_444000_80150A8C[135] = {
#include "assets/actor_444000_model_20ABC_verts.inc"
};

SVECTOR D_actor_444000_80150EC4[135] = {
#include "assets/actor_444000_model_20ABC_normals.inc"
};

u32 D_actor_444000_801512FC[1400] = {
#include "assets/actor_444000_model_20ABC_stream.inc"
};

TmdSource D_actor_444000_801528DC = {
    0,
    9492,
    0,
    1,
    D_actor_444000_80150A88,
    D_actor_444000_80150A8C,
    D_actor_444000_80150EC4,
    D_actor_444000_80150A64,
    D_actor_444000_801512FC,
};

AnimationPackedPose D_actor_444000_80152900[4] = {
#include "assets/actor_444000_animation_20BD4_bank1.inc"
};

AnimationPackedRotation D_actor_444000_80152930[7] = {
#include "assets/actor_444000_animation_20BD4_bank4.inc"
};

AnimationRecord D_actor_444000_8015294C[38] = {
#include "assets/actor_444000_animation_20BD4_records.inc"
};

u16 D_actor_444000_801529E4[8] = {
#include "assets/actor_444000_animation_20BD4_indices.inc"
};

AnimationSet D_actor_444000_801529F4 = {
    D_actor_444000_8015294C,
    D_actor_444000_801529E4,
    { NULL, D_actor_444000_80152900, NULL, NULL, D_actor_444000_80152930, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_80152A1C[4] = {
#include "assets/actor_444000_animation_20C80_bank1.inc"
};

AnimationPackedRotation D_actor_444000_80152A4C[1] = {
#include "assets/actor_444000_animation_20C80_bank4.inc"
};

AnimationRecord D_actor_444000_80152A50[18] = {
#include "assets/actor_444000_animation_20C80_records.inc"
};

u16 D_actor_444000_80152A98[4] = {
#include "assets/actor_444000_animation_20C80_indices.inc"
};

AnimationSet D_actor_444000_80152AA0 = {
    D_actor_444000_80152A50,
    D_actor_444000_80152A98,
    { NULL, D_actor_444000_80152A1C, NULL, NULL, D_actor_444000_80152A4C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_80152AC8[4] = {
#include "assets/actor_444000_animation_20D2C_bank1.inc"
};

AnimationPackedRotation D_actor_444000_80152AF8[1] = {
#include "assets/actor_444000_animation_20D2C_bank4.inc"
};

AnimationRecord D_actor_444000_80152AFC[18] = {
#include "assets/actor_444000_animation_20D2C_records.inc"
};

u16 D_actor_444000_80152B44[4] = {
#include "assets/actor_444000_animation_20D2C_indices.inc"
};

AnimationSet D_actor_444000_80152B4C = {
    D_actor_444000_80152AFC,
    D_actor_444000_80152B44,
    { NULL, D_actor_444000_80152AC8, NULL, NULL, D_actor_444000_80152AF8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_80152B74[7] = {
#include "assets/actor_444000_animation_21014_bank1.inc"
};

AnimationPackedRotation D_actor_444000_80152BC8[49] = {
#include "assets/actor_444000_animation_21014_bank4.inc"
};

AnimationRecord D_actor_444000_80152C8C[102] = {
#include "assets/actor_444000_animation_21014_records.inc"
};

u16 D_actor_444000_80152E24[8] = {
#include "assets/actor_444000_animation_21014_indices.inc"
};

AnimationSet D_actor_444000_80152E34 = {
    D_actor_444000_80152C8C,
    D_actor_444000_80152E24,
    { NULL, D_actor_444000_80152B74, NULL, NULL, D_actor_444000_80152BC8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_80152E5C[29] = {
#include "assets/actor_444000_animation_212D8_bank1.inc"
};

AnimationPackedRotation D_actor_444000_80152FB8[17] = {
#include "assets/actor_444000_animation_212D8_bank4.inc"
};

AnimationRecord D_actor_444000_80152FFC[61] = {
#include "assets/actor_444000_animation_212D8_records.inc"
};

u16 D_actor_444000_801530F0[4] = {
#include "assets/actor_444000_animation_212D8_indices.inc"
};

AnimationSet D_actor_444000_801530F8 = {
    D_actor_444000_80152FFC,
    D_actor_444000_801530F0,
    { NULL, D_actor_444000_80152E5C, NULL, NULL, D_actor_444000_80152FB8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_80153120[30] = {
#include "assets/actor_444000_animation_215B4_bank1.inc"
};

AnimationPackedRotation D_actor_444000_80153288[18] = {
#include "assets/actor_444000_animation_215B4_bank4.inc"
};

AnimationRecord D_actor_444000_801532D0[63] = {
#include "assets/actor_444000_animation_215B4_records.inc"
};

u16 D_actor_444000_801533CC[4] = {
#include "assets/actor_444000_animation_215B4_indices.inc"
};

AnimationSet D_actor_444000_801533D4 = {
    D_actor_444000_801532D0,
    D_actor_444000_801533CC,
    { NULL, D_actor_444000_80153120, NULL, NULL, D_actor_444000_80153288, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_801533FC[20] = {
#include "assets/actor_444000_animation_21904_bank1.inc"
};

AnimationPackedRotation D_actor_444000_801534EC[48] = {
#include "assets/actor_444000_animation_21904_bank4.inc"
};

AnimationRecord D_actor_444000_801535AC[90] = {
#include "assets/actor_444000_animation_21904_records.inc"
};

u16 D_actor_444000_80153714[8] = {
#include "assets/actor_444000_animation_21904_indices.inc"
};

AnimationSet D_actor_444000_80153724 = {
    D_actor_444000_801535AC,
    D_actor_444000_80153714,
    { NULL, D_actor_444000_801533FC, NULL, NULL, D_actor_444000_801534EC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_8015374C[18] = {
#include "assets/actor_444000_animation_21AE4_bank1.inc"
};

AnimationPackedRotation D_actor_444000_80153824[13] = {
#include "assets/actor_444000_animation_21AE4_bank4.inc"
};

AnimationRecord D_actor_444000_80153858[41] = {
#include "assets/actor_444000_animation_21AE4_records.inc"
};

u16 D_actor_444000_801538FC[4] = {
#include "assets/actor_444000_animation_21AE4_indices.inc"
};

AnimationSet D_actor_444000_80153904 = {
    D_actor_444000_80153858,
    D_actor_444000_801538FC,
    { NULL, D_actor_444000_8015374C, NULL, NULL, D_actor_444000_80153824, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_8015392C[18] = {
#include "assets/actor_444000_animation_21CC4_bank1.inc"
};

AnimationPackedRotation D_actor_444000_80153A04[13] = {
#include "assets/actor_444000_animation_21CC4_bank4.inc"
};

AnimationRecord D_actor_444000_80153A38[41] = {
#include "assets/actor_444000_animation_21CC4_records.inc"
};

u16 D_actor_444000_80153ADC[4] = {
#include "assets/actor_444000_animation_21CC4_indices.inc"
};

AnimationSet D_actor_444000_80153AE4 = {
    D_actor_444000_80153A38,
    D_actor_444000_80153ADC,
    { NULL, D_actor_444000_8015392C, NULL, NULL, D_actor_444000_80153A04, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_80153B0C[18] = {
#include "assets/actor_444000_animation_220AC_bank1.inc"
};

AnimationPackedRotation D_actor_444000_80153BE4[72] = {
#include "assets/actor_444000_animation_220AC_bank4.inc"
};

AnimationRecord D_actor_444000_80153D04[110] = {
#include "assets/actor_444000_animation_220AC_records.inc"
};

u16 D_actor_444000_80153EBC[8] = {
#include "assets/actor_444000_animation_220AC_indices.inc"
};

AnimationSet D_actor_444000_80153ECC = {
    D_actor_444000_80153D04,
    D_actor_444000_80153EBC,
    { NULL, D_actor_444000_80153B0C, NULL, NULL, D_actor_444000_80153BE4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_80153EF4[39] = {
#include "assets/actor_444000_animation_2246C_bank1.inc"
};

AnimationPackedRotation D_actor_444000_801540C8[32] = {
#include "assets/actor_444000_animation_2246C_bank4.inc"
};

AnimationRecord D_actor_444000_80154148[79] = {
#include "assets/actor_444000_animation_2246C_records.inc"
};

u16 D_actor_444000_80154284[4] = {
#include "assets/actor_444000_animation_2246C_indices.inc"
};

AnimationSet D_actor_444000_8015428C = {
    D_actor_444000_80154148,
    D_actor_444000_80154284,
    { NULL, D_actor_444000_80153EF4, NULL, NULL, D_actor_444000_801540C8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_801542B4[32] = {
#include "assets/actor_444000_animation_22780_bank1.inc"
};

AnimationPackedRotation D_actor_444000_80154434[23] = {
#include "assets/actor_444000_animation_22780_bank4.inc"
};

AnimationRecord D_actor_444000_80154490[66] = {
#include "assets/actor_444000_animation_22780_records.inc"
};

u16 D_actor_444000_80154598[4] = {
#include "assets/actor_444000_animation_22780_indices.inc"
};

AnimationSet D_actor_444000_801545A0 = {
    D_actor_444000_80154490,
    D_actor_444000_80154598,
    { NULL, D_actor_444000_801542B4, NULL, NULL, D_actor_444000_80154434, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_801545C8[11] = {
#include "assets/actor_444000_animation_22B64_bank1.inc"
};

AnimationPackedRotation D_actor_444000_8015464C[78] = {
#include "assets/actor_444000_animation_22B64_bank4.inc"
};

AnimationRecord D_actor_444000_80154784[124] = {
#include "assets/actor_444000_animation_22B64_records.inc"
};

u16 D_actor_444000_80154974[8] = {
#include "assets/actor_444000_animation_22B64_indices.inc"
};

AnimationSet D_actor_444000_80154984 = {
    D_actor_444000_80154784,
    D_actor_444000_80154974,
    { NULL, D_actor_444000_801545C8, NULL, NULL, D_actor_444000_8015464C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_801549AC[47] = {
#include "assets/actor_444000_animation_22FBC_bank1.inc"
};

AnimationPackedRotation D_actor_444000_80154BE0[30] = {
#include "assets/actor_444000_animation_22FBC_bank4.inc"
};

AnimationRecord D_actor_444000_80154C58[95] = {
#include "assets/actor_444000_animation_22FBC_records.inc"
};

u16 D_actor_444000_80154DD4[4] = {
#include "assets/actor_444000_animation_22FBC_indices.inc"
};

AnimationSet D_actor_444000_80154DDC = {
    D_actor_444000_80154C58,
    D_actor_444000_80154DD4,
    { NULL, D_actor_444000_801549AC, NULL, NULL, D_actor_444000_80154BE0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_80154E04[46] = {
#include "assets/actor_444000_animation_233E4_bank1.inc"
};

AnimationPackedRotation D_actor_444000_8015502C[27] = {
#include "assets/actor_444000_animation_233E4_bank4.inc"
};

AnimationRecord D_actor_444000_80155098[89] = {
#include "assets/actor_444000_animation_233E4_records.inc"
};

u16 D_actor_444000_801551FC[4] = {
#include "assets/actor_444000_animation_233E4_indices.inc"
};

AnimationSet D_actor_444000_80155204 = {
    D_actor_444000_80155098,
    D_actor_444000_801551FC,
    { NULL, D_actor_444000_80154E04, NULL, NULL, D_actor_444000_8015502C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_8015522C[13] = {
#include "assets/actor_444000_animation_23790_bank1.inc"
};

AnimationPackedRotation D_actor_444000_801552C8[77] = {
#include "assets/actor_444000_animation_23790_bank4.inc"
};

AnimationRecord D_actor_444000_801553FC[105] = {
#include "assets/actor_444000_animation_23790_records.inc"
};

u16 D_actor_444000_801555A0[8] = {
#include "assets/actor_444000_animation_23790_indices.inc"
};

AnimationSet D_actor_444000_801555B0 = {
    D_actor_444000_801553FC,
    D_actor_444000_801555A0,
    { NULL, D_actor_444000_8015522C, NULL, NULL, D_actor_444000_801552C8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_801555D8[33] = {
#include "assets/actor_444000_animation_23A98_bank1.inc"
};

AnimationPackedRotation D_actor_444000_80155764[21] = {
#include "assets/actor_444000_animation_23A98_bank4.inc"
};

AnimationRecord D_actor_444000_801557B8[62] = {
#include "assets/actor_444000_animation_23A98_records.inc"
};

u16 D_actor_444000_801558B0[4] = {
#include "assets/actor_444000_animation_23A98_indices.inc"
};

AnimationSet D_actor_444000_801558B8 = {
    D_actor_444000_801557B8,
    D_actor_444000_801558B0,
    { NULL, D_actor_444000_801555D8, NULL, NULL, D_actor_444000_80155764, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_801558E0[31] = {
#include "assets/actor_444000_animation_23D80_bank1.inc"
};

AnimationPackedRotation D_actor_444000_80155A54[21] = {
#include "assets/actor_444000_animation_23D80_bank4.inc"
};

AnimationRecord D_actor_444000_80155AA8[60] = {
#include "assets/actor_444000_animation_23D80_records.inc"
};

u16 D_actor_444000_80155B98[4] = {
#include "assets/actor_444000_animation_23D80_indices.inc"
};

AnimationSet D_actor_444000_80155BA0 = {
    D_actor_444000_80155AA8,
    D_actor_444000_80155B98,
    { NULL, D_actor_444000_801558E0, NULL, NULL, D_actor_444000_80155A54, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_80155BC8[13] = {
#include "assets/actor_444000_animation_23FE8_bank1.inc"
};

AnimationPackedRotation D_actor_444000_80155C64[38] = {
#include "assets/actor_444000_animation_23FE8_bank4.inc"
};

AnimationRecord D_actor_444000_80155CFC[63] = {
#include "assets/actor_444000_animation_23FE8_records.inc"
};

u16 D_actor_444000_80155DF8[8] = {
#include "assets/actor_444000_animation_23FE8_indices.inc"
};

AnimationSet D_actor_444000_80155E08 = {
    D_actor_444000_80155CFC,
    D_actor_444000_80155DF8,
    { NULL, D_actor_444000_80155BC8, NULL, NULL, D_actor_444000_80155C64, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_80155E30[6] = {
#include "assets/actor_444000_animation_240B4_bank1.inc"
};

AnimationPackedRotation D_actor_444000_80155E78[5] = {
#include "assets/actor_444000_animation_240B4_bank4.inc"
};

AnimationRecord D_actor_444000_80155E8C[16] = {
#include "assets/actor_444000_animation_240B4_records.inc"
};

u16 D_actor_444000_80155ECC[4] = {
#include "assets/actor_444000_animation_240B4_indices.inc"
};

AnimationSet D_actor_444000_80155ED4 = {
    D_actor_444000_80155E8C,
    D_actor_444000_80155ECC,
    { NULL, D_actor_444000_80155E30, NULL, NULL, D_actor_444000_80155E78, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_80155EFC[6] = {
#include "assets/actor_444000_animation_24180_bank1.inc"
};

AnimationPackedRotation D_actor_444000_80155F44[5] = {
#include "assets/actor_444000_animation_24180_bank4.inc"
};

AnimationRecord D_actor_444000_80155F58[16] = {
#include "assets/actor_444000_animation_24180_records.inc"
};

u16 D_actor_444000_80155F98[4] = {
#include "assets/actor_444000_animation_24180_indices.inc"
};

AnimationSet D_actor_444000_80155FA0 = {
    D_actor_444000_80155F58,
    D_actor_444000_80155F98,
    { NULL, D_actor_444000_80155EFC, NULL, NULL, D_actor_444000_80155F44, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_80155FC8[8] = {
#include "assets/actor_444000_animation_24458_bank1.inc"
};

AnimationPackedRotation D_actor_444000_80156028[49] = {
#include "assets/actor_444000_animation_24458_bank4.inc"
};

AnimationRecord D_actor_444000_801560EC[95] = {
#include "assets/actor_444000_animation_24458_records.inc"
};

u16 D_actor_444000_80156268[8] = {
#include "assets/actor_444000_animation_24458_indices.inc"
};

AnimationSet D_actor_444000_80156278 = {
    D_actor_444000_801560EC,
    D_actor_444000_80156268,
    { NULL, D_actor_444000_80155FC8, NULL, NULL, D_actor_444000_80156028, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_801562A0[22] = {
#include "assets/actor_444000_animation_2469C_bank1.inc"
};

AnimationPackedRotation D_actor_444000_801563A8[12] = {
#include "assets/actor_444000_animation_2469C_bank4.inc"
};

AnimationRecord D_actor_444000_801563D8[55] = {
#include "assets/actor_444000_animation_2469C_records.inc"
};

u16 D_actor_444000_801564B4[4] = {
#include "assets/actor_444000_animation_2469C_indices.inc"
};

AnimationSet D_actor_444000_801564BC = {
    D_actor_444000_801563D8,
    D_actor_444000_801564B4,
    { NULL, D_actor_444000_801562A0, NULL, NULL, D_actor_444000_801563A8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_801564E4[23] = {
#include "assets/actor_444000_animation_248DC_bank1.inc"
};

AnimationPackedRotation D_actor_444000_801565F8[10] = {
#include "assets/actor_444000_animation_248DC_bank4.inc"
};

AnimationRecord D_actor_444000_80156620[53] = {
#include "assets/actor_444000_animation_248DC_records.inc"
};

u16 D_actor_444000_801566F4[4] = {
#include "assets/actor_444000_animation_248DC_indices.inc"
};

AnimationSet D_actor_444000_801566FC = {
    D_actor_444000_80156620,
    D_actor_444000_801566F4,
    { NULL, D_actor_444000_801564E4, NULL, NULL, D_actor_444000_801565F8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_80156724[9] = {
#include "assets/actor_444000_animation_24AE4_bank1.inc"
};

AnimationPackedRotation D_actor_444000_80156790[33] = {
#include "assets/actor_444000_animation_24AE4_bank4.inc"
};

AnimationRecord D_actor_444000_80156814[56] = {
#include "assets/actor_444000_animation_24AE4_records.inc"
};

u16 D_actor_444000_801568F4[8] = {
#include "assets/actor_444000_animation_24AE4_indices.inc"
};

AnimationSet D_actor_444000_80156904 = {
    D_actor_444000_80156814,
    D_actor_444000_801568F4,
    { NULL, D_actor_444000_80156724, NULL, NULL, D_actor_444000_80156790, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_8015692C[15] = {
#include "assets/actor_444000_animation_24C70_bank1.inc"
};

AnimationPackedRotation D_actor_444000_801569E0[11] = {
#include "assets/actor_444000_animation_24C70_bank4.inc"
};

AnimationRecord D_actor_444000_80156A0C[31] = {
#include "assets/actor_444000_animation_24C70_records.inc"
};

u16 D_actor_444000_80156A88[4] = {
#include "assets/actor_444000_animation_24C70_indices.inc"
};

AnimationSet D_actor_444000_80156A90 = {
    D_actor_444000_80156A0C,
    D_actor_444000_80156A88,
    { NULL, D_actor_444000_8015692C, NULL, NULL, D_actor_444000_801569E0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_80156AB8[15] = {
#include "assets/actor_444000_animation_24E04_bank1.inc"
};

AnimationPackedRotation D_actor_444000_80156B6C[12] = {
#include "assets/actor_444000_animation_24E04_bank4.inc"
};

AnimationRecord D_actor_444000_80156B9C[32] = {
#include "assets/actor_444000_animation_24E04_records.inc"
};

u16 D_actor_444000_80156C1C[4] = {
#include "assets/actor_444000_animation_24E04_indices.inc"
};

AnimationSet D_actor_444000_80156C24 = {
    D_actor_444000_80156B9C,
    D_actor_444000_80156C1C,
    { NULL, D_actor_444000_80156AB8, NULL, NULL, D_actor_444000_80156B6C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_80156C4C[22] = {
#include "assets/actor_444000_animation_2523C_bank1.inc"
};

AnimationPackedRotation D_actor_444000_80156D54[73] = {
#include "assets/actor_444000_animation_2523C_bank4.inc"
};

AnimationRecord D_actor_444000_80156E78[117] = {
#include "assets/actor_444000_animation_2523C_records.inc"
};

u16 D_actor_444000_8015704C[8] = {
#include "assets/actor_444000_animation_2523C_indices.inc"
};

AnimationSet D_actor_444000_8015705C = {
    D_actor_444000_80156E78,
    D_actor_444000_8015704C,
    { NULL, D_actor_444000_80156C4C, NULL, NULL, D_actor_444000_80156D54, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_80157084[33] = {
#include "assets/actor_444000_animation_25580_bank1.inc"
};

AnimationPackedRotation D_actor_444000_80157210[29] = {
#include "assets/actor_444000_animation_25580_bank4.inc"
};

AnimationRecord D_actor_444000_80157284[69] = {
#include "assets/actor_444000_animation_25580_records.inc"
};

u16 D_actor_444000_80157398[4] = {
#include "assets/actor_444000_animation_25580_indices.inc"
};

AnimationSet D_actor_444000_801573A0 = {
    D_actor_444000_80157284,
    D_actor_444000_80157398,
    { NULL, D_actor_444000_80157084, NULL, NULL, D_actor_444000_80157210, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_801573C8[31] = {
#include "assets/actor_444000_animation_25898_bank1.inc"
};

AnimationPackedRotation D_actor_444000_8015753C[26] = {
#include "assets/actor_444000_animation_25898_bank4.inc"
};

AnimationRecord D_actor_444000_801575A4[67] = {
#include "assets/actor_444000_animation_25898_records.inc"
};

u16 D_actor_444000_801576B0[4] = {
#include "assets/actor_444000_animation_25898_indices.inc"
};

AnimationSet D_actor_444000_801576B8 = {
    D_actor_444000_801575A4,
    D_actor_444000_801576B0,
    { NULL, D_actor_444000_801573C8, NULL, NULL, D_actor_444000_8015753C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_801576E0[5] = {
#include "assets/actor_444000_animation_25BE0_bank1.inc"
};

AnimationPackedRotation D_actor_444000_8015771C[76] = {
#include "assets/actor_444000_animation_25BE0_bank4.inc"
};

AnimationRecord D_actor_444000_8015784C[105] = {
#include "assets/actor_444000_animation_25BE0_records.inc"
};

u16 D_actor_444000_801579F0[8] = {
#include "assets/actor_444000_animation_25BE0_indices.inc"
};

AnimationSet D_actor_444000_80157A00 = {
    D_actor_444000_8015784C,
    D_actor_444000_801579F0,
    { NULL, D_actor_444000_801576E0, NULL, NULL, D_actor_444000_8015771C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_80157A28[39] = {
#include "assets/actor_444000_animation_25F14_bank1.inc"
};

AnimationPackedRotation D_actor_444000_80157BFC[14] = {
#include "assets/actor_444000_animation_25F14_bank4.inc"
};

AnimationRecord D_actor_444000_80157C34[62] = {
#include "assets/actor_444000_animation_25F14_records.inc"
};

u16 D_actor_444000_80157D2C[4] = {
#include "assets/actor_444000_animation_25F14_indices.inc"
};

AnimationSet D_actor_444000_80157D34 = {
    D_actor_444000_80157C34,
    D_actor_444000_80157D2C,
    { NULL, D_actor_444000_80157A28, NULL, NULL, D_actor_444000_80157BFC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_80157D5C[38] = {
#include "assets/actor_444000_animation_26240_bank1.inc"
};

AnimationPackedRotation D_actor_444000_80157F24[15] = {
#include "assets/actor_444000_animation_26240_bank4.inc"
};

AnimationRecord D_actor_444000_80157F60[62] = {
#include "assets/actor_444000_animation_26240_records.inc"
};

u16 D_actor_444000_80158058[4] = {
#include "assets/actor_444000_animation_26240_indices.inc"
};

AnimationSet D_actor_444000_80158060 = {
    D_actor_444000_80157F60,
    D_actor_444000_80158058,
    { NULL, D_actor_444000_80157D5C, NULL, NULL, D_actor_444000_80157F24, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_80158088[24] = {
#include "assets/actor_444000_animation_26720_bank1.inc"
};

AnimationPackedRotation D_actor_444000_801581A8[88] = {
#include "assets/actor_444000_animation_26720_bank4.inc"
};

AnimationRecord D_actor_444000_80158308[138] = {
#include "assets/actor_444000_animation_26720_records.inc"
};

u16 D_actor_444000_80158530[8] = {
#include "assets/actor_444000_animation_26720_indices.inc"
};

AnimationSet D_actor_444000_80158540 = {
    D_actor_444000_80158308,
    D_actor_444000_80158530,
    { NULL, D_actor_444000_80158088, NULL, NULL, D_actor_444000_801581A8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_80158568[26] = {
#include "assets/actor_444000_animation_269B4_bank1.inc"
};

AnimationPackedRotation D_actor_444000_801586A0[19] = {
#include "assets/actor_444000_animation_269B4_bank4.inc"
};

AnimationRecord D_actor_444000_801586EC[56] = {
#include "assets/actor_444000_animation_269B4_records.inc"
};

u16 D_actor_444000_801587CC[4] = {
#include "assets/actor_444000_animation_269B4_indices.inc"
};

AnimationSet D_actor_444000_801587D4 = {
    D_actor_444000_801586EC,
    D_actor_444000_801587CC,
    { NULL, D_actor_444000_80158568, NULL, NULL, D_actor_444000_801586A0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_801587FC[30] = {
#include "assets/actor_444000_animation_26CC0_bank1.inc"
};

AnimationPackedRotation D_actor_444000_80158964[26] = {
#include "assets/actor_444000_animation_26CC0_bank4.inc"
};

AnimationRecord D_actor_444000_801589CC[67] = {
#include "assets/actor_444000_animation_26CC0_records.inc"
};

u16 D_actor_444000_80158AD8[4] = {
#include "assets/actor_444000_animation_26CC0_indices.inc"
};

AnimationSet D_actor_444000_80158AE0 = {
    D_actor_444000_801589CC,
    D_actor_444000_80158AD8,
    { NULL, D_actor_444000_801587FC, NULL, NULL, D_actor_444000_80158964, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_80158B08[53] = {
#include "assets/actor_444000_animation_27D84_bank1.inc"
};

AnimationPackedRotation D_actor_444000_80158D84[373] = {
#include "assets/actor_444000_animation_27D84_bank4.inc"
};

AnimationRecord D_actor_444000_80159358[521] = {
#include "assets/actor_444000_animation_27D84_records.inc"
};

u16 D_actor_444000_80159B7C[20] = {
#include "assets/actor_444000_animation_27D84_indices.inc"
};

AnimationSet D_actor_444000_80159BA4 = {
    D_actor_444000_80159358,
    D_actor_444000_80159B7C,
    { NULL, D_actor_444000_80158B08, NULL, NULL, D_actor_444000_80158D84, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_80159BCC[36] = {
#include "assets/actor_444000_animation_28920_bank1.inc"
};

AnimationPackedRotation D_actor_444000_80159D7C[255] = {
#include "assets/actor_444000_animation_28920_bank4.inc"
};

AnimationRecord D_actor_444000_8015A178[360] = {
#include "assets/actor_444000_animation_28920_records.inc"
};

u16 D_actor_444000_8015A718[20] = {
#include "assets/actor_444000_animation_28920_indices.inc"
};

AnimationSet D_actor_444000_8015A740 = {
    D_actor_444000_8015A178,
    D_actor_444000_8015A718,
    { NULL, D_actor_444000_80159BCC, NULL, NULL, D_actor_444000_80159D7C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_8015A768[4] = {
#include "assets/actor_444000_animation_28A28_bank1.inc"
};

AnimationPackedRotation D_actor_444000_8015A798[10] = {
#include "assets/actor_444000_animation_28A28_bank4.inc"
};

AnimationRecord D_actor_444000_8015A7C0[30] = {
#include "assets/actor_444000_animation_28A28_records.inc"
};

u16 D_actor_444000_8015A838[8] = {
#include "assets/actor_444000_animation_28A28_indices.inc"
};

AnimationSet D_actor_444000_8015A848 = {
    D_actor_444000_8015A7C0,
    D_actor_444000_8015A838,
    { NULL, D_actor_444000_8015A768, NULL, NULL, D_actor_444000_8015A798, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_8015A870[5] = {
#include "assets/actor_444000_animation_28B08_bank1.inc"
};

AnimationPackedRotation D_actor_444000_8015A8AC[8] = {
#include "assets/actor_444000_animation_28B08_bank4.inc"
};

AnimationRecord D_actor_444000_8015A8CC[21] = {
#include "assets/actor_444000_animation_28B08_records.inc"
};

u16 D_actor_444000_8015A920[4] = {
#include "assets/actor_444000_animation_28B08_indices.inc"
};

AnimationSet D_actor_444000_8015A928 = {
    D_actor_444000_8015A8CC,
    D_actor_444000_8015A920,
    { NULL, D_actor_444000_8015A870, NULL, NULL, D_actor_444000_8015A8AC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_8015A950[5] = {
#include "assets/actor_444000_animation_28BE8_bank1.inc"
};

AnimationPackedRotation D_actor_444000_8015A98C[8] = {
#include "assets/actor_444000_animation_28BE8_bank4.inc"
};

AnimationRecord D_actor_444000_8015A9AC[21] = {
#include "assets/actor_444000_animation_28BE8_records.inc"
};

u16 D_actor_444000_8015AA00[4] = {
#include "assets/actor_444000_animation_28BE8_indices.inc"
};

AnimationSet D_actor_444000_8015AA08 = {
    D_actor_444000_8015A9AC,
    D_actor_444000_8015AA00,
    { NULL, D_actor_444000_8015A950, NULL, NULL, D_actor_444000_8015A98C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_8015AA30[6] = {
#include "assets/actor_444000_animation_28CF0_bank1.inc"
};

AnimationPackedRotation D_actor_444000_8015AA78[3] = {
#include "assets/actor_444000_animation_28CF0_bank4.inc"
};

AnimationRecord D_actor_444000_8015AA84[31] = {
#include "assets/actor_444000_animation_28CF0_records.inc"
};

u16 D_actor_444000_8015AB00[8] = {
#include "assets/actor_444000_animation_28CF0_indices.inc"
};

AnimationSet D_actor_444000_8015AB10 = {
    D_actor_444000_8015AA84,
    D_actor_444000_8015AB00,
    { NULL, D_actor_444000_8015AA30, NULL, NULL, D_actor_444000_8015AA78, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_8015AB38[6] = {
#include "assets/actor_444000_animation_28DB8_bank1.inc"
};

AnimationPackedRotation D_actor_444000_8015AB80[1] = {
#include "assets/actor_444000_animation_28DB8_bank4.inc"
};

AnimationRecord D_actor_444000_8015AB84[19] = {
#include "assets/actor_444000_animation_28DB8_records.inc"
};

u16 D_actor_444000_8015ABD0[4] = {
#include "assets/actor_444000_animation_28DB8_indices.inc"
};

AnimationSet D_actor_444000_8015ABD8 = {
    D_actor_444000_8015AB84,
    D_actor_444000_8015ABD0,
    { NULL, D_actor_444000_8015AB38, NULL, NULL, D_actor_444000_8015AB80, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_8015AC00[6] = {
#include "assets/actor_444000_animation_28E80_bank1.inc"
};

AnimationPackedRotation D_actor_444000_8015AC48[1] = {
#include "assets/actor_444000_animation_28E80_bank4.inc"
};

AnimationRecord D_actor_444000_8015AC4C[19] = {
#include "assets/actor_444000_animation_28E80_records.inc"
};

u16 D_actor_444000_8015AC98[4] = {
#include "assets/actor_444000_animation_28E80_indices.inc"
};

AnimationSet D_actor_444000_8015ACA0 = {
    D_actor_444000_8015AC4C,
    D_actor_444000_8015AC98,
    { NULL, D_actor_444000_8015AC00, NULL, NULL, D_actor_444000_8015AC48, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_8015ACC8[2] = {
#include "assets/actor_444000_animation_28F78_bank1.inc"
};

AnimationPackedRotation D_actor_444000_8015ACE0[9] = {
#include "assets/actor_444000_animation_28F78_bank4.inc"
};

AnimationRecord D_actor_444000_8015AD04[33] = {
#include "assets/actor_444000_animation_28F78_records.inc"
};

u16 D_actor_444000_8015AD88[8] = {
#include "assets/actor_444000_animation_28F78_indices.inc"
};

AnimationSet D_actor_444000_8015AD98 = {
    D_actor_444000_8015AD04,
    D_actor_444000_8015AD88,
    { NULL, D_actor_444000_8015ACC8, NULL, NULL, D_actor_444000_8015ACE0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_8015ADC0[4] = {
#include "assets/actor_444000_animation_29040_bank1.inc"
};

AnimationPackedRotation D_actor_444000_8015ADF0[5] = {
#include "assets/actor_444000_animation_29040_bank4.inc"
};

AnimationRecord D_actor_444000_8015AE04[21] = {
#include "assets/actor_444000_animation_29040_records.inc"
};

u16 D_actor_444000_8015AE58[4] = {
#include "assets/actor_444000_animation_29040_indices.inc"
};

AnimationSet D_actor_444000_8015AE60 = {
    D_actor_444000_8015AE04,
    D_actor_444000_8015AE58,
    { NULL, D_actor_444000_8015ADC0, NULL, NULL, D_actor_444000_8015ADF0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_8015AE88[4] = {
#include "assets/actor_444000_animation_29108_bank1.inc"
};

AnimationPackedRotation D_actor_444000_8015AEB8[5] = {
#include "assets/actor_444000_animation_29108_bank4.inc"
};

AnimationRecord D_actor_444000_8015AECC[21] = {
#include "assets/actor_444000_animation_29108_records.inc"
};

u16 D_actor_444000_8015AF20[4] = {
#include "assets/actor_444000_animation_29108_indices.inc"
};

AnimationSet D_actor_444000_8015AF28 = {
    D_actor_444000_8015AECC,
    D_actor_444000_8015AF20,
    { NULL, D_actor_444000_8015AE88, NULL, NULL, D_actor_444000_8015AEB8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_8015AF50[3] = {
#include "assets/actor_444000_animation_29840_bank1.inc"
};

AnimationPackedRotation D_actor_444000_8015AF74[203] = {
#include "assets/actor_444000_animation_29840_bank4.inc"
};

AnimationRecord D_actor_444000_8015B2A0[236] = {
#include "assets/actor_444000_animation_29840_records.inc"
};

u16 D_actor_444000_8015B650[8] = {
#include "assets/actor_444000_animation_29840_indices.inc"
};

AnimationSet D_actor_444000_8015B660 = {
    D_actor_444000_8015B2A0,
    D_actor_444000_8015B650,
    { NULL, D_actor_444000_8015AF50, NULL, NULL, D_actor_444000_8015AF74, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_8015B688[279] = {
#include "assets/actor_444000_animation_2AC8C_bank1.inc"
};

AnimationPackedRotation D_actor_444000_8015C39C[73] = {
#include "assets/actor_444000_animation_2AC8C_bank4.inc"
};

AnimationRecord D_actor_444000_8015C4C0[377] = {
#include "assets/actor_444000_animation_2AC8C_records.inc"
};

u16 D_actor_444000_8015CAA4[4] = {
#include "assets/actor_444000_animation_2AC8C_indices.inc"
};

AnimationSet D_actor_444000_8015CAAC = {
    D_actor_444000_8015C4C0,
    D_actor_444000_8015CAA4,
    { NULL, D_actor_444000_8015B688, NULL, NULL, D_actor_444000_8015C39C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_8015CAD4[287] = {
#include "assets/actor_444000_animation_2C160_bank1.inc"
};

AnimationPackedRotation D_actor_444000_8015D848[72] = {
#include "assets/actor_444000_animation_2C160_bank4.inc"
};

AnimationRecord D_actor_444000_8015D968[388] = {
#include "assets/actor_444000_animation_2C160_records.inc"
};

u16 D_actor_444000_8015DF78[4] = {
#include "assets/actor_444000_animation_2C160_indices.inc"
};

AnimationSet D_actor_444000_8015DF80 = {
    D_actor_444000_8015D968,
    D_actor_444000_8015DF78,
    { NULL, D_actor_444000_8015CAD4, NULL, NULL, D_actor_444000_8015D848, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_8015DFA8[3] = {
#include "assets/actor_444000_animation_2C240_bank1.inc"
};

AnimationPackedRotation D_actor_444000_8015DFCC[7] = {
#include "assets/actor_444000_animation_2C240_bank4.inc"
};

AnimationRecord D_actor_444000_8015DFE8[26] = {
#include "assets/actor_444000_animation_2C240_records.inc"
};

u16 D_actor_444000_8015E050[8] = {
#include "assets/actor_444000_animation_2C240_indices.inc"
};

AnimationSet D_actor_444000_8015E060 = {
    D_actor_444000_8015DFE8,
    D_actor_444000_8015E050,
    { NULL, D_actor_444000_8015DFA8, NULL, NULL, D_actor_444000_8015DFCC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_8015E088[3] = {
#include "assets/actor_444000_animation_2C2CC_bank1.inc"
};

AnimationPackedRotation D_actor_444000_8015E0AC[2] = {
#include "assets/actor_444000_animation_2C2CC_bank4.inc"
};

AnimationRecord D_actor_444000_8015E0B4[12] = {
#include "assets/actor_444000_animation_2C2CC_records.inc"
};

u16 D_actor_444000_8015E0E4[4] = {
#include "assets/actor_444000_animation_2C2CC_indices.inc"
};

AnimationSet D_actor_444000_8015E0EC = {
    D_actor_444000_8015E0B4,
    D_actor_444000_8015E0E4,
    { NULL, D_actor_444000_8015E088, NULL, NULL, D_actor_444000_8015E0AC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_8015E114[3] = {
#include "assets/actor_444000_animation_2C358_bank1.inc"
};

AnimationPackedRotation D_actor_444000_8015E138[2] = {
#include "assets/actor_444000_animation_2C358_bank4.inc"
};

AnimationRecord D_actor_444000_8015E140[12] = {
#include "assets/actor_444000_animation_2C358_records.inc"
};

u16 D_actor_444000_8015E170[4] = {
#include "assets/actor_444000_animation_2C358_indices.inc"
};

AnimationSet D_actor_444000_8015E178 = {
    D_actor_444000_8015E140,
    D_actor_444000_8015E170,
    { NULL, D_actor_444000_8015E114, NULL, NULL, D_actor_444000_8015E138, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_8015E1A0[14] = {
#include "assets/actor_444000_animation_2CB24_bank1.inc"
};

AnimationPackedRotation D_actor_444000_8015E248[158] = {
#include "assets/actor_444000_animation_2CB24_bank4.inc"
};

AnimationRecord D_actor_444000_8015E4C0[279] = {
#include "assets/actor_444000_animation_2CB24_records.inc"
};

u16 D_actor_444000_8015E91C[20] = {
#include "assets/actor_444000_animation_2CB24_indices.inc"
};

AnimationSet D_actor_444000_8015E944 = {
    D_actor_444000_8015E4C0,
    D_actor_444000_8015E91C,
    { NULL, D_actor_444000_8015E1A0, NULL, NULL, D_actor_444000_8015E248, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_8015E96C[15] = {
#include "assets/actor_444000_animation_2D330_bank1.inc"
};

AnimationPackedRotation D_actor_444000_8015EA20[206] = {
#include "assets/actor_444000_animation_2D330_bank4.inc"
};

AnimationRecord D_actor_444000_8015ED58[244] = {
#include "assets/actor_444000_animation_2D330_records.inc"
};

u16 D_actor_444000_8015F128[20] = {
#include "assets/actor_444000_animation_2D330_indices.inc"
};

AnimationSet D_actor_444000_8015F150 = {
    D_actor_444000_8015ED58,
    D_actor_444000_8015F128,
    { NULL, D_actor_444000_8015E96C, NULL, NULL, D_actor_444000_8015EA20, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_8015F178[39] = {
#include "assets/actor_444000_animation_2E198_bank1.inc"
};

AnimationPackedRotation D_actor_444000_8015F34C[353] = {
#include "assets/actor_444000_animation_2E198_bank4.inc"
};

AnimationRecord D_actor_444000_8015F8D0[432] = {
#include "assets/actor_444000_animation_2E198_records.inc"
};

u16 D_actor_444000_8015FF90[20] = {
#include "assets/actor_444000_animation_2E198_indices.inc"
};

AnimationSet D_actor_444000_8015FFB8 = {
    D_actor_444000_8015F8D0,
    D_actor_444000_8015FF90,
    { NULL, D_actor_444000_8015F178, NULL, NULL, D_actor_444000_8015F34C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_8015FFE0[6] = {
#include "assets/actor_444000_animation_2E548_bank1.inc"
};

AnimationPackedRotation D_actor_444000_80160028[73] = {
#include "assets/actor_444000_animation_2E548_bank4.inc"
};

AnimationRecord D_actor_444000_8016014C[125] = {
#include "assets/actor_444000_animation_2E548_records.inc"
};

u16 D_actor_444000_80160340[20] = {
#include "assets/actor_444000_animation_2E548_indices.inc"
};

AnimationSet D_actor_444000_80160368 = {
    D_actor_444000_8016014C,
    D_actor_444000_80160340,
    { NULL, D_actor_444000_8015FFE0, NULL, NULL, D_actor_444000_80160028, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_80160390[10] = {
#include "assets/actor_444000_animation_2EA80_bank1.inc"
};

AnimationPackedRotation D_actor_444000_80160408[117] = {
#include "assets/actor_444000_animation_2EA80_bank4.inc"
};

AnimationRecord D_actor_444000_801605DC[167] = {
#include "assets/actor_444000_animation_2EA80_records.inc"
};

u16 D_actor_444000_80160878[20] = {
#include "assets/actor_444000_animation_2EA80_indices.inc"
};

AnimationSet D_actor_444000_801608A0 = {
    D_actor_444000_801605DC,
    D_actor_444000_80160878,
    { NULL, D_actor_444000_80160390, NULL, NULL, D_actor_444000_80160408, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_444000_801608C8[7] = {
#include "assets/actor_444000_animation_2EE14_bank1.inc"
};

AnimationPackedRotation D_actor_444000_8016091C[65] = {
#include "assets/actor_444000_animation_2EE14_bank4.inc"
};

AnimationRecord D_actor_444000_80160A20[123] = {
#include "assets/actor_444000_animation_2EE14_records.inc"
};

u16 D_actor_444000_80160C0C[20] = {
#include "assets/actor_444000_animation_2EE14_indices.inc"
};

AnimationSet D_actor_444000_80160C34 = {
    D_actor_444000_80160A20,
    D_actor_444000_80160C0C,
    { NULL, D_actor_444000_801608C8, NULL, NULL, D_actor_444000_8016091C, NULL, NULL, NULL },
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
    &D_actor_444000_801529F4,
    &D_actor_444000_80152E34,
    &D_actor_444000_80153724,
    &D_actor_444000_80154984,
    &D_actor_444000_801555B0,
    NULL,
    &D_actor_444000_8015A848,
    &D_actor_444000_8015AB10,
    &D_actor_444000_8015AD98,
    &D_actor_444000_8015E060,
    &D_actor_444000_80153ECC,
    &D_actor_444000_80155E08,
    &D_actor_444000_80157A00,
    &D_actor_444000_80156278,
    &D_actor_444000_80156904,
    &D_actor_444000_8015705C,
    &D_actor_444000_80158540,
    &D_actor_444000_8015B660,
    &D_actor_444000_80153ECC,
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
    &D_actor_444000_80152AA0,
    &D_actor_444000_801530F8,
    &D_actor_444000_80153904,
    &D_actor_444000_80154DDC,
    &D_actor_444000_801558B8,
    NULL,
    &D_actor_444000_8015A928,
    &D_actor_444000_8015ABD8,
    &D_actor_444000_8015AE60,
    &D_actor_444000_8015E0EC,
    &D_actor_444000_8015428C,
    &D_actor_444000_80155ED4,
    &D_actor_444000_80157D34,
    &D_actor_444000_801564BC,
    &D_actor_444000_80156A90,
    &D_actor_444000_801573A0,
    &D_actor_444000_801587D4,
    &D_actor_444000_8015CAAC,
    &D_actor_444000_8015428C,
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
    &D_actor_444000_80152B4C,
    &D_actor_444000_801533D4,
    &D_actor_444000_80153AE4,
    &D_actor_444000_80155204,
    &D_actor_444000_80155BA0,
    NULL,
    &D_actor_444000_8015AA08,
    &D_actor_444000_8015ACA0,
    &D_actor_444000_8015AF28,
    &D_actor_444000_8015E178,
    &D_actor_444000_801545A0,
    &D_actor_444000_80155FA0,
    &D_actor_444000_80158060,
    &D_actor_444000_801566FC,
    &D_actor_444000_80156C24,
    &D_actor_444000_801576B8,
    &D_actor_444000_80158AE0,
    &D_actor_444000_8015DF80,
    &D_actor_444000_801545A0,
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
    &D_actor_444000_8015A740,
    &D_actor_444000_8015F150,
    NULL,
    NULL,
    &D_actor_444000_80159BA4,
    &D_actor_444000_8015F150,
    NULL,
};

u8 gGluttonRainGroup = 0;

AnimationSet* gGluttonCaughtAnimSets[7] = {
    NULL,
    &D_actor_444000_8015E944,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

void             func_actor_444000_80143888(Task*);
extern TmdSource D_actor_444000_8014A650;
extern TmdSource D_actor_444000_80147D10;
extern TmdSource D_actor_444000_80148E94;
extern TmdSource D_actor_444000_8014D19C;
extern TmdSource D_actor_444000_8014D5FC;
extern TmdSource D_actor_444000_80161B50;

TaskDesc D_actor_444000_801616B0[7] = {
    { { { TASK_BODY_TMD, 96 } }, gluttonPropTask, { .model = &D_actor_444000_8014BE0C } },
    { { { TASK_BODY_TMD, 96 } }, gluttonPropTask, { .model = &D_actor_444000_8014A650 } },
    { { { TASK_BODY_TMD, 96 } }, gluttonPropTask, { .model = &D_actor_444000_80147D10 } },
    { { { TASK_BODY_TMD, 96 } }, gluttonPropTask, { .model = &D_actor_444000_80148E94 } },
    { { { TASK_BODY_TMD, 96 } }, gluttonPropTask, { .model = &D_actor_444000_8014D19C } },
    { { { TASK_BODY_TMD, 96 } }, gluttonPropTask, { .model = &D_actor_444000_8014D5FC } },
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
    { { { TASK_BODY_TMD, 96 } }, gluttonGlobTask, { .model = &D_actor_444000_8014E220 } },
    { { { TASK_BODY_COORD, 96 } }, gluttonRainTask, { .value = 0 } },
    { { { TASK_BODY_COORD, 96 } }, gluttonThrowTask, { .value = 0 } },
    { { { TASK_BODY_TMD, 96 } }, gluttonChunkTask, { .model = &D_actor_444000_801528DC } },
};

TaskDesc D_actor_444000_8016180C = { { { TASK_BODY_TMD, 96 } }, gluttonSpinnerTask, { .model = &D_actor_444000_80150A40 } };

Actor444000MessageEntry D_actor_444000_80161818[7] = {
    { 2005, { .call3 = func_actor_444000_8013A958 } },
    { 2006, { .call0 = func_actor_444000_80143D68 } },
    { 2004, { .call2 = func_actor_444000_80143D7C } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call1 = func_actor_444000_8013ACD0 } },
    { 5108, { .call3 = func_actor_444000_80143E68 } },
    { 2009, { .call0 = func_actor_444000_80143F38 } },
    { TASK_MESSAGE_TABLE_END, { .call0 = NULL } },
};

s16 D_actor_444000_80161850 = 0;

TaskDesc D_actor_444000_80161854 = { { { TASK_BODY_TMD, 96 } }, func_actor_444000_80142F28, { .model = &D_actor_444000_80146F68 } };

Task* D_actor_444000_80161860 = NULL;

u32 D_actor_444000_80161864 = 0x1A90C60D;

Actor444000Storage1868 D_actor_444000_80161868 = { 0, { 0, 0, 0, 0, 0, 0, 0 } };

SVECTOR ActorContact_ScratchPosition = { 0, 0, 0, 0 };

/// Borrowed host task reference for screen-shake requests while the boss lives.
static _GluttonHostTaskStorage _gGluttonHostTask = { NULL, { 0 } };

EffectSpawnArg D_actor_444000_80161880 = { NULL, 0, 0 };

Actor444000Storage1888 D_actor_444000_80161888 = { { { .loc = { 0, 0 } }, 0 }, { 0, 0, 0, 0 } };

SVECTOR gGluttonSpinnerTarget = { 0, 0, 0, 0 };

Actor444000Storage1898 gGluttonGrabQuery = { { { 0, 0, 0, 0 }, 0, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0 } };

GluttonDropCoord D_actor_444000_801618B8 = { .c = { 0, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } };

Actor444000Storage1908 D_actor_444000_80161908 = { { { 0, 0, 0, 0 }, { 0, 0, 0, 0 } }, { 0, 0, 0, 0, 0, 0, 0, 0 } };

Actor444000Storage1928 D_actor_444000_80161928 = { { { 0, 0, 0, 0 }, 0, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0 } };

Actor444000Storage1948 D_actor_444000_80161948 = { { { 0, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL }, { 0, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL }, { 0, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL }, { 0, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL }, { 0, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL }, { 0, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } };

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
                                                    Task* player, Actor444000WarpScratch* sc,
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

/// Run one step of the event task: act on the pending action index in
/// `field_2C`, then clear it so the action fires once.
static void func_actor_444000_80132054(Task* task)
{
    Actor444000EventWork* work = (Actor444000EventWork*)task->work;
    Actor444000EventWork* other;
    Actor444000EventWork* target;
    AnimationPlayRequest  msg;
    s32                   anim;

    switch (work->field_2C) {
        case 0:
            break;
        case 1:
            /* Install the weapon-specific player animation on the slot-3 task. */
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
        case 2:
            if (work->field_20 != NULL) {
                msg.source.sets          = D_actor_444000_8014430C;
                msg.animationId          = 3;
                msg.blend                = ANIMATION_BLEND_RESET;
                msg.blendFrames          = 0;
                msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(work->field_20, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
            }
            /* Same one-shot cue as func_actor_444000_80132608. */
            other = (Actor444000EventWork*)D_actor_444000_80161860->work;
            if (other->field_2A == 0) {
                SndEvt_EnqueueType6(0x54280005, 0, 0);
                other->field_2A = 1;
            }
            break;
        case 3:
            Gp_PulseState1C();
            Gp_StateC08.field_6 |= 1;
            target               = (Actor444000EventWork*)task->work;
            if (target->field_20 != NULL) {
                msg.source.sets          = D_actor_444000_8014430C;
                msg.animationId          = 0;
                msg.blend                = ANIMATION_BLEND_INTERPOLATE;
                msg.blendFrames          = 0xA;
                msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(target->field_20, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
            }
            break;
    }
    work->field_2C = 0;
}

/// Bring the room's presentation up to date for an enter (0), a first entry
/// (1) or a re-entry (2): pick the view set from the current disc/scenario
/// stage in `GameSession::incineratorDescentPhase`, republish the area-record id, and on a
/// first entry spawn the accompanying task. Any other `arg0` does nothing.
void func_actor_444000_801321FC(s32 arg0)
{
    Actor444000EventWork* work;

    work = (Actor444000EventWork*)D_actor_444000_80161860->work;
    switch (arg0) {
        case 0:
            gGameSession->viewDirty                                    = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = work->field_28.b;
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
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = work->field_28.b;
            Gp_ApplyAreaRecs(D_shelter_b3_garbage_incinerator_8018FB6C);
            if (arg0 == 1) {
                work->field_24 = Task_Spawn(1, 0x2D, 0x10, 0);
            }
            gGameSession->viewDirty = 1;
            break;
    }
}

/// Task body of the overlay's event/controller task, run once per frame while
/// the session is not paused (`GameSession::sceneUpdatesPaused`), no cutscene is active
/// (`Gp_StateC08.field_9`) and the battle state is not frozen
/// (`gSceneCombatState.actorControl`).
///
/// State 0 allocates the `Actor444000EventWork` block and publishes the task in
/// `D_actor_444000_80161860`; a task spawned with `spawnArg1` set jumps
/// straight to state 3, otherwise it advances one state at a time. State 1
/// counts 0x2BD frames and then arms the death/ending sequence once. State 2
/// counts 0x15 frames and hands off to the follow-up task table. State 3 waits
/// for the room to settle, spawns the successor from `D_shelter_b3_garbage_incinerator_80187150` and kills
/// this task.
void func_actor_444000_80132358(Task* task)
{
    Actor444000EventWork* work = (Actor444000EventWork*)task->work;
    Actor444000EventWork* alloc;
    Actor444000EventWork* other;
    s32                   state;
    s16                   timer;

    if (gGameSession->sceneUpdatesPaused != 0) {
        return;
    }
    if (Gp_StateC08.field_9 != 0) {
        return;
    }
    if (gSceneCombatState.actorControl != SCENE_COMBAT_ACTORS_RUNNING) {
        return;
    }

    state = task->state;
    switch (state) {
        case 0:
            if (Gp_StateC08.field_A == 1) {
                return;
            }
            if (gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                return;
            }
            alloc      = memCalloc(sizeof(Actor444000EventWork), false);
            task->work = alloc;
            if (alloc == NULL) {
                taskKill(task);
            } else {
                Mem_Set(alloc, 0, sizeof(Actor444000EventWork));
                alloc->field_20         = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                D_actor_444000_80161860 = task;
            }
            if (task->spawnArg1.value != 0) {
                work             = (Actor444000EventWork*)task->work;
                work->field_28.h = gGameSession->location.loc.view;
                Gp_MsgPlayerWeapon(0);
                func_800E8634(D_actor_444000_80144634, 0, D_actor_444000_8014488C);
                task->state = 3;
            } else {
                task->state += 1;
            }
            break;
        case 1:
            timer               = (u16)task->killCountdown + 1;
            task->killCountdown = timer;
            if (timer >= 0x2BD) {
                Gp_MsgPlayerWeapon(0);
                other = (Actor444000EventWork*)D_actor_444000_80161860->work;
                if (other->field_30 == 0) {
                    gSceneCombatState.battleRefs                        = 0;
                    gSceneCombatState.signals.bytes.endDelayFrames      = 0xF;
                    gSceneCombatState.signals.bytes.battlePhase         = SCENE_COMBAT_BATTLE_IDLE;
                    gSceneCombatState.signals.bytes.actionFlags         = 0;
                    gSceneCombatState.signals.bytes.enemyAlert          = 0;
                    gGameSession->flowFlags                            |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 0xD;
                    other->field_30                                     = state;
                }
                task->killCountdown = 0;
                task->state        += 1;
            }
            break;
        case 2:
            timer               = (u16)task->killCountdown + 1;
            task->killCountdown = timer;
            if (timer >= 0x15) {
                work->field_28.h = gGameSession->location.loc.view;
                func_800E8634(D_actor_444000_8014431C, 0, D_actor_444000_801444E4);
                task->state += 1;
            }
            break;
        case 3:
            if (gGameSession->eventState == 0) {
                D_shelter_b3_garbage_incinerator_801855DE = 0;
                gGameSession->sceneClock                  = D_shelter_b3_garbage_incinerator_8018FBC8[0];
                Task_SpawnFromTable(D_shelter_b3_garbage_incinerator_80187150, 0, 1, 0);
                taskKill(task);
                return;
            }
            break;
    }
    func_actor_444000_80132054(task);
}

/// Play the event's sound cue once, latching a flag so a repeat call is a no-op.
void func_actor_444000_80132608(void)
{
    Actor444000EventWork* work = (Actor444000EventWork*)D_actor_444000_80161860->work;

    if (work->field_2A == 0) {
        SndEvt_EnqueueType6(0x54280005, 0, 0);
        work->field_2A = 1;
    }
}

/// Forward a message to the slot-3 task the event work block carries.
void func_actor_444000_8013265C(s32 arg0)
{
    Actor444000EventWork* work = (Actor444000EventWork*)D_actor_444000_80161860->work;

    Gp_DispatchMsg(work->field_20, 0x3F3, arg0, 0);
}

/// Kill the subordinate task the event work block carries, if it is still alive.
void func_actor_444000_80132694(void)
{
    Actor444000EventWork* work = (Actor444000EventWork*)D_actor_444000_80161860->work;

    if (work->field_24 != NULL) {
        taskKill(work->field_24);
        work->field_24 = NULL;
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
/// flag the session and pick area script 0xD, then latch `field_30` so a later
/// call does nothing.
void func_actor_444000_80132778(void)
{
    Actor444000EventWork* work = (Actor444000EventWork*)D_actor_444000_80161860->work;

    if (work->field_30 == 0) {
        gSceneCombatState.battleRefs                        = 0;
        gSceneCombatState.signals.bytes.endDelayFrames      = 0xF;
        gSceneCombatState.signals.bytes.battlePhase         = SCENE_COMBAT_BATTLE_IDLE;
        gSceneCombatState.signals.bytes.actionFlags         = 0;
        gSceneCombatState.signals.bytes.enemyAlert          = 0;
        gGameSession->flowFlags                            |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 0xD;
        work->field_30                                      = 1;
    }
}

/// Set the actor's action index, resetting the sub-state counter that goes
/// with it.
void func_actor_444000_801327E8(s16 action)
{
    Actor444000EventWork* work = (Actor444000EventWork*)D_actor_444000_80161860->work;

    work->field_2C = action;
    work->field_2E = 0;
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
/// same reason as `gluttonShrinkRotation` above.
static __inline__ void Actor444000_StepForward(GfxCoord* coord)
{
    u8*      head;
    SVECTOR* dir;

    head                          = SCRATCH_STACK_CURSOR(u8);
    dir                           = (SVECTOR*)(head - sizeof(SVECTOR));
    SCRATCH_STACK_CURSOR(SVECTOR) = dir;

    Gfx_MatrixCol2(&coord->coord, dir);
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

/// The run-out / turn / run-back pass, stepped by `GluttonWork::field_F08`.
///
/// A reset request re-arms the block: the model's flag word and the enemy's
/// link state are cleared, both colour steps are switched on, animation 2 is
/// requested and the scratch matrix is seeded with an identity rotation.
///
/// Every step publishes the yaw from the model's own facing to the player in
/// `field_7C4`, wrapped into +/-0x800, and -- unless the game is frozen --
/// walks the model forward along that facing. State 0 runs out to x 0x1770,
/// state 1 turns the model 0xD a step until it has swung the full half turn
/// (its rotation is rebuilt from the running `angle` rather than spun in
/// place), and states 2 to 5 run it back through -0x1387, -0x251B and -0x32C7.
/// Past -0x4203 the task hands over to state 0x10 and tells the player task
/// (slot 7) message 0x13F4.
static void func_actor_444000_8013482C(Task* task)
{
    Actor444000RunScratch* sc;
    OverlayMat*            mat;
    TmdObject*             tmd;
    GluttonWork*           work;
    Enemy*                 enemy;
    GfxCoord*              coord;
    GfxCoord*              model;
    GfxCoord*              facing;
    u8*                    head;
    s16                    ang;
    s32                    frame;

    head = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_RESERVE_BYTES(sizeof(Actor444000RunScratch));
    sc = SCRATCH_STACK_CURSOR(Actor444000RunScratch);

    work  = task->work;
    enemy = task->spawnArg2.pointer;

    if (work->field_4 != 0) {
        tmd                           = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        tmd->flags                    = 0;
        work->field_EF4               = 1;
        work->field_7B3               = 2;
        work->field_EF6               = 1;
        work->field_7B0               = 1;
        work->field_EFA               = 0;
        work->field_EFE               = 0;
        mat                           = &((Actor444000RunScratch*)(head - sizeof(Actor444000RunScratch)))->m;
        mat->ident.m00M01             = ONE;
        mat->ident.m02M10             = 0;
        mat->ident.m11M12             = ONE;
        mat->ident.m20M21             = 0;
        mat->ident.m22                = ONE;
    }

    gluttonTickAnim(task);

    frame = work->slots0[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    if (frame == 0x12 && work->field_7D8 != frame) {
        s32 id;
        s32 pan;

        work->field_EAC = 3;
        Gp_SpawnScript18(D_actor_444000_80144A74, D_actor_444000_80144A7C);
        id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200001;
        pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        SndEvt_EnqueueType6(id, pan, (s8)(worldCoordGetOriginAudioDepth(task->extra.tmd->coords) / 2));
    }

    frame = work->slots0[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    if (frame == 0x18 && work->field_7D8 != frame) {
        s32 id;
        s32 pan;

        work->field_EAC = 3;
        Gp_SpawnScript18(D_actor_444000_80144A74, D_actor_444000_80144A7C);
        id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200001;
        pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        SndEvt_EnqueueType6(id, pan, (s8)(worldCoordGetOriginAudioDepth(task->extra.tmd->coords) / 2));
    }

    work->field_7D8 = work->slots0[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;

    model      = task->extra.tmd->coords;
    sc->dir.vx = gPlayerStatus.coordMtx->t[0] - model->coord.t[0];
    sc->dir.vy = gPlayerStatus.coordMtx->t[1] - model->coord.t[1];
    sc->dir.vz = gPlayerStatus.coordMtx->t[2] - model->coord.t[2];

    facing = task->extra.tmd->coords;
    ang    = ratan2(sc->dir.vx, sc->dir.vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);

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

    work->field_7C4 = ang;

    switch (work->field_F08) {
        case 0: {
            s32       paused = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen;
            GfxCoord* c      = task->extra.tmd->coords;

            if (paused != 1) {
                Actor444000_StepForward(c);
            }
        }
            task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            if (task->extra.tmd->coords->coord.t[0] >= 0x1770) {
                work->field_0 = 0xA;
                work->field_F08++;
            }
            break;

        case 1:
            coord = task->extra.tmd->coords;
            if (coord->coord.t[0] < 0x2134) {
                if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
                    Actor444000_StepForward(coord);
                }
            } else {
                sc->angle = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]) + 0xD;
                sc->m.mat = task->extra.tmd->coords->coord;

                Gfx_MatrixCol2(&sc->m.mat, &sc->dir);
                VectorNormalSS(&sc->dir, &sc->dir);
                gte_lddp(0xBEA);
                gte_ldsv(&sc->dir);
                gte_gpf12();
                gte_stsv(&sc->dir);

                sc->m.mat.t[0] += sc->dir.vx;
                sc->m.mat.t[1] += sc->dir.vy;
                sc->m.mat.t[2] += sc->dir.vz;

                gfxRotMatrixY(&sc->m.mat, sc->angle, 1);
                task->extra.tmd->coords->coord = sc->m.mat;

                Gfx_MatrixCol2(&sc->m.mat, &sc->dir);
                VectorNormalSS(&sc->dir, &sc->dir);
                gte_lddp(-0xBB8);
                gte_ldsv(&sc->dir);
                gte_gpf12();
                gte_stsv(&sc->dir);

                task->extra.tmd->coords->coord.t[0]  += sc->dir.vx;
                task->extra.tmd->coords->coord.t[1]  += sc->dir.vy;
                task->extra.tmd->coords->coord.t[2]  += sc->dir.vz;
                task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;

                if (0x800 - ABS(sc->angle) < 0xD) {
                    sc->angle = 0x800;
                    gfxRotMatrixY(&task->extra.tmd->coords->coord, 0x800, 1);
                    work->field_F08++;
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
                work->field_0 = 0xA;
                work->field_F08++;
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
                work->field_0 = 0xA;
                work->field_F08++;
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
                work->field_0 = 0xA;
                work->field_F08++;
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
                work->field_0 = 0x10;
                work->field_F08++;
                Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), 0x13F4, 0, 0);
            }
            break;
    }

    SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor444000RunScratch));
}

/// Rebuild `coord`'s rotation around the yaw it already faces, left at full
/// width but scaled by `y` vertically -- the squash the death sequence retracts
/// each body with. The same shape as `gluttonScaleRotation` below, except
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

    ang       = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle = ang;
    gfxRotMatrixY(&sc->m, ang, 1);
    sc->scale.vx = 0x1000;
    sc->scale.vy = y;
    sc->scale.vz = 0x1000;
    ScaleMatrix(&sc->m, &sc->scale);

    coord->coord.m[0][0] = sc->m.m[0][0];
    coord->coord.m[0][1] = sc->m.m[0][1];
    coord->coord.m[0][2] = sc->m.m[0][2];
    coord->coord.m[1][0] = sc->m.m[1][0];
    coord->coord.m[1][1] = sc->m.m[1][1];
    coord->coord.m[1][2] = sc->m.m[1][2];
    coord->coord.m[2][0] = sc->m.m[2][0];
    coord->coord.m[2][1] = sc->m.m[2][1];
    coord->coord.m[2][2] = sc->m.m[2][2];
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

    if (work->field_4 != 0) {
        s32 id;
        s32 pan;

        tmd                           = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
        tmd->flags                    = 0;
        work->field_7B3               = 0x12;
        work->field_EF4               = 0;
        work->field_EF6               = 0;
        work->field_EFA               = 0;
        work->field_7B0               = 1;
        work->field_EFE               = 0;

        gluttonTickAnim(task);

        gGameSession->location.loc.variant = 3;
        id                                 = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54280007;
        pan                                = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        SndEvt_EnqueueType6(id, pan, (s8)(worldCoordGetOriginAudioDepth(task->extra.tmd->coords) / 2));
        return;
    }

    if (work->slots0[1].flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->field_6 = 0;
        gluttonTickAnim(task);
    }

    if (!(work->slots0[1].flags & ANIMATION_SLOT_SETTLED)) {
        if (gGluttonLimbReach >= 0x191) {
            gGluttonLimbReach = (u16)gGluttonLimbReach - 0xC8;
        }

        gluttonTickAnim(task);

        frame = work->slots0[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (frame == 0x33 && work->field_7A8 != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200013;
            pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        }

        frame = work->slots0[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (frame == 0x3D && work->field_7A8 != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200003;
            pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        }

        frame = work->slots0[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (frame == 0x4E && work->field_7A8 != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200014;
            pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        }

        frame = work->slots0[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (frame == 0x71 && work->field_7A8 != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200015;
            pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        }

        work->field_7A8 = work->slots0[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;

        switch (work->field_6) {
            case 0x14:
                Gp_SetLightMode(enemy, ENEMY_COLOR_WEIGHTED);
                Gp_SetLightMode(work->field_ECC[3], ENEMY_COLOR_WEIGHTED);
                break;

            case 0x82:
                actorAccumulateToView(&task->extra.tmd->coords[4], &mat);

                pos.vz = 0;
                pos.vy = 0;
                pos.vx = 0;
                actorLocalToView(&task->extra.tmd->coords[4], &pos);

                work->field_ECC[2]->task->extra.tmd->coords->parent       = &gGfxViewCoord;
                work->field_ECC[2]->task->extra.tmd->coords->coord        = mat;
                work->field_ECC[2]->task->extra.tmd->coords->coord.t[0]   = pos.vx;
                work->field_ECC[2]->task->extra.tmd->coords->coord.t[1]   = pos.vy;
                work->field_ECC[2]->task->extra.tmd->coords->coord.t[2]   = pos.vz;
                work->field_ECC[2]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                break;

            case 0x1DC:
                actorAccumulateToView(&task->extra.tmd->coords[3], &mat);

                pos.vz = 0;
                pos.vy = 0;
                pos.vx = 0;
                actorLocalToView(&task->extra.tmd->coords[3], &pos);

                work->field_ECC[3]->task->extra.tmd->coords->parent       = &gGfxViewCoord;
                work->field_ECC[3]->task->extra.tmd->coords->coord        = mat;
                work->field_ECC[3]->task->extra.tmd->coords->coord.t[0]   = pos.vx;
                work->field_ECC[3]->task->extra.tmd->coords->coord.t[1]   = pos.vy;
                work->field_ECC[3]->task->extra.tmd->coords->coord.t[2]   = pos.vz;
                work->field_ECC[3]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                break;

            case 0x14A:
                actorAccumulateToView(&task->extra.tmd->coords[4], &mat);

                pos.vz = 0;
                pos.vy = 0;
                pos.vx = 0;
                actorLocalToView(&task->extra.tmd->coords[4], &pos);

                work->field_ECC[4]->task->extra.tmd->coords->parent       = &gGfxViewCoord;
                work->field_ECC[4]->task->extra.tmd->coords->coord        = mat;
                work->field_ECC[4]->task->extra.tmd->coords->coord.t[0]   = pos.vx;
                work->field_ECC[4]->task->extra.tmd->coords->coord.t[1]   = pos.vy;
                work->field_ECC[4]->task->extra.tmd->coords->coord.t[2]   = pos.vz;
                work->field_ECC[4]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                work->field_EF8                                           = 0;
                break;
        }

        if (work->field_6 >= 0x83) {
            if (work->field_ECC[2]->task->extra.tmd->coords->coord.t[1] <
                task->extra.tmd->coords->coord.t[1]) {
                work->field_ECC[2]->task->extra.tmd->coords->coord.t[1] +=
                    (work->field_6 - 0x82) * 0x1E;
            } else if (task->extra.tmd->coords->coord.t[1] <
                       work->field_ECC[2]->task->extra.tmd->coords->coord.t[1]) {
                work->field_ECC[2]->task->extra.tmd->coords->coord.t[1] =
                    task->extra.tmd->coords->coord.t[1];
                Gp_SpawnEff(0x60196, work->field_ECC[2]->task->extra.tmd->coords, 0x13401800,
                            NULL);
            }

            step = work->field_6 - 0x82;
            if (step < 0x28) {
                Actor444000_SquashRotation(work->field_ECC[2]->task->extra.tmd->coords,
                                           (s16)(0x1000 - ((step * 0x1000) / 40)));
                work->field_ECC[2]->task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;

                if ((s16)((s16)(u16)work->field_6 % 5) == 0) {
                    switch ((s16)((s16)((s16)(u16)work->field_6 / 5) % 3)) {
                        case 0:
                            pos.vz = 0;
                            pos.vy = 0;
                            pos.vx = 0;
                            Gp_SpawnEff(0x60196, work->field_ECC[2]->task->extra.tmd->coords,
                                        0x13401800, &pos);
                            break;
                        case 1:
                            pos.vx = 0x320;
                            pos.vy = 0;
                            pos.vz = -0x320;
                            Gp_SpawnEff(0x60196, work->field_ECC[2]->task->extra.tmd->coords,
                                        0x13401800, &pos);
                            break;
                        case 2:
                            pos.vx = -0x320;
                            pos.vy = 0;
                            pos.vz = 0x320;
                            Gp_SpawnEff(0x60196, work->field_ECC[2]->task->extra.tmd->coords,
                                        0x13401800, &pos);
                            break;
                    }
                }
            } else if (step == 0x28) {
                work->field_ECC[2]->task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                Gp_SpawnEff(0x60196, work->field_ECC[2]->task->extra.tmd->coords, 0x13401800,
                            NULL);
            }
        }

        if (work->field_6 >= 0x14B) {
            if (work->field_ECC[4]->task->extra.tmd->coords->coord.t[1] <
                task->extra.tmd->coords->coord.t[1]) {
                work->field_ECC[4]->task->extra.tmd->coords->coord.t[1] +=
                    (work->field_6 - 0x14A) * 0x1E;
            } else if (task->extra.tmd->coords->coord.t[1] <
                       work->field_ECC[4]->task->extra.tmd->coords->coord.t[1]) {
                work->field_ECC[4]->task->extra.tmd->coords->coord.t[1] =
                    task->extra.tmd->coords->coord.t[1];
                Gp_SpawnEff(0x60196, work->field_ECC[4]->task->extra.tmd->coords, 0x13401800,
                            NULL);
            }

            step = work->field_6 - 0x14A;
            if (step < 0x28) {
                Actor444000_SquashRotation(work->field_ECC[4]->task->extra.tmd->coords,
                                           (s16)(0x1000 - ((step * 0x1000) / 40)));
                work->field_ECC[4]->task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;

                if ((s16)((s16)(u16)work->field_6 % 5) == 0) {
                    switch ((s16)((s16)((s16)(u16)work->field_6 / 5) % 3)) {
                        case 0:
                            pos.vz = 0;
                            pos.vy = 0;
                            pos.vx = 0;
                            Gp_SpawnEff(0x60196, work->field_ECC[4]->task->extra.tmd->coords,
                                        0x13401800, &pos);
                            break;
                        case 1:
                            pos.vx = 0x320;
                            pos.vy = 0;
                            pos.vz = -0x320;
                            Gp_SpawnEff(0x60196, work->field_ECC[4]->task->extra.tmd->coords,
                                        0x13401800, &pos);
                            break;
                        case 2:
                            pos.vx = -0x320;
                            pos.vy = 0;
                            pos.vz = 0x320;
                            Gp_SpawnEff(0x60196, work->field_ECC[4]->task->extra.tmd->coords,
                                        0x13401800, &pos);
                            break;
                    }
                }
            } else if (step == 0x28) {
                work->field_ECC[4]->task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                Gp_SpawnEff(0x60196, work->field_ECC[4]->task->extra.tmd->coords, 0x13401800,
                            NULL);
            }
        }

        if (work->field_6 >= 0x1DD) {
            if (work->field_ECC[3]->task->extra.tmd->coords->coord.t[1] <
                task->extra.tmd->coords->coord.t[1]) {
                work->field_ECC[3]->task->extra.tmd->coords->coord.t[1] +=
                    (work->field_6 - 0x1DC) * 0x1E;
            } else if (task->extra.tmd->coords->coord.t[1] <
                       work->field_ECC[3]->task->extra.tmd->coords->coord.t[1]) {
                work->field_ECC[3]->task->extra.tmd->coords->coord.t[1] =
                    task->extra.tmd->coords->coord.t[1];
            }

            step = work->field_6 - 0x1DC;
            if (step < 0x28) {
                Actor444000_SquashRotation(work->field_ECC[3]->task->extra.tmd->coords,
                                           (s16)(0x1000 - ((step * 0x1000) / 40)));
                work->field_ECC[3]->task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;

                if ((s16)((s16)(u16)work->field_6 % 5) == 0) {
                    switch ((s16)((s16)((s16)(u16)work->field_6 / 5) % 3)) {
                        case 0:
                            pos.vz = 0;
                            pos.vy = 0;
                            pos.vx = 0;
                            Gp_SpawnEff(0x60196, work->field_ECC[3]->task->extra.tmd->coords,
                                        0x13401800, &pos);
                            break;
                        case 1:
                            pos.vx = 0x320;
                            pos.vy = 0;
                            pos.vz = -0x320;
                            Gp_SpawnEff(0x60196, work->field_ECC[3]->task->extra.tmd->coords,
                                        0x13401800, &pos);
                            break;
                        case 2:
                            pos.vx = -0x320;
                            pos.vy = 0;
                            pos.vz = 0x320;
                            Gp_SpawnEff(0x60196, work->field_ECC[3]->task->extra.tmd->coords,
                                        0x13401800, &pos);
                            break;
                    }
                }
            } else if (step == 0x28) {
                work->field_ECC[3]->task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
        }
    } else {
        switch (work->field_6) {
            case 0x28:
                break;
            case 0x78:
                Gp_SetLightMode(enemy, ENEMY_COLOR_BLACK);
                break;
        }

        if (work->field_6 > 0) {
            if (work->field_6 < 0x28) {
                Actor444000_SquashRotation(work->field_ECC[0]->task->extra.tmd->coords,
                                           (s16)(0x1000 - ((work->field_6 * 0x1000) / 40)));
                work->field_ECC[0]->task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            } else if (work->field_6 == 0x28) {
                work->field_ECC[0]->task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
        }

        if (work->field_6 >= 0x15) {
            step = work->field_6 - 0x14;
            if (step < 0x28) {
                Actor444000_SquashRotation(work->field_ECC[1]->task->extra.tmd->coords,
                                           (s16)(0x1000 - ((step * 0x1000) / 40)));
                work->field_ECC[1]->task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            } else if (step == 0x28) {
                work->field_ECC[1]->task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
        }

        if (work->field_6 >= 0xE) {
            step = work->field_6 - 0xD;
            if (step < 0x78) {
                Actor444000_SquashRotation(task->extra.tmd->coords,
                                           (s16)(0x1000 - ((step * 0x1000) / 120)));
                task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            } else if (step == 0x78) {
                task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
        }

        if ((s16)((s16)(u16)work->field_6 % 3) == 0 && (s16)(u16)work->field_6 - 0xD < 0x78) {
            switch ((s16)((s16)((s16)(u16)work->field_6 / 3) % 5)) {
                case 0:
                    pos.vz = 0;
                    pos.vy = 0;
                    pos.vx = 0;
                    Gp_SpawnEff(0x60196, task->extra.tmd->coords, 0x14101900, &pos);
                    break;
                case 1:
                    pos.vx = 0x960;
                    pos.vy = 0;
                    pos.vz = -0x960;
                    Gp_SpawnEff(0x60196, task->extra.tmd->coords, 0x13201800, &pos);
                    break;
                case 2:
                    pos.vx = -0x9C4;
                    pos.vy = 0;
                    pos.vz = 0x9C4;
                    Gp_SpawnEff(0x60196, task->extra.tmd->coords, 0x131C1800, &pos);
                    break;
                case 3:
                    pos.vx = -0x6A4;
                    pos.vy = 0;
                    pos.vz = 0x640;
                    Gp_SpawnEff(0x60196, task->extra.tmd->coords, 0x14101800, &pos);
                    break;
                case 4:
                    pos.vx = 0x6A4;
                    pos.vy = 0;
                    pos.vz = -0x640;
                    Gp_SpawnEff(0x60196, task->extra.tmd->coords, 0x14301800, &pos);
                    break;
            }
        }

        if (work->field_6 >= 0xA1) {
            step = work->field_6 - 0xA0;
            if (step < 0x5A) {
                Actor444000_SquashRotation(work->field_ECC[3]->task->extra.tmd->coords,
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

        if (work->field_6 == 0xA0) {
            SndEvt_EnqueueType7((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54280007, 1);
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

    Gfx_MatrixCol2(&task->extra.tmd->coords->coord, &dir);
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
static const GpEnemyTaskFuncTable3 gGluttonPropStates = {
    {
        gluttonPropSetup,
        gluttonPropTick,
        Gp_DestroyEnemy,
    },
};

/// State handlers of the enemy `gluttonThrowTask` dispatches.
static const GpEnemyTaskFuncTable3 gGluttonThrowStates = {
    {
        gluttonThrowSpawn,
        gluttonThrowFly,
        Gp_DestroyEnemy,
    },
};

/// State handlers of the grab enemy, by state: setup, bounce, rise, hold and
/// teardown.
static const GpEnemyTaskFuncTable5 gGluttonGlobStates = {
    {
        gluttonGlobSpawn,
        gluttonGlobFall,
        gluttonGlobEngulf,
        gluttonGlobHold,
        Gp_DestroyEnemy,
    },
};

#include "../../shared/glutton_chunk_settle.inc.c"

#include "../../shared/glutton_rain_spawn.inc.c"

#include "../../shared/glutton_rain_rise.inc.c"

#include "../../shared/glutton_rain_fall.inc.c"

/// State handlers of the enemy `gluttonChunkTask` dispatches: spawn,
/// descent, settle and teardown.
static const GpEnemyTaskFuncTable4 gGluttonChunkStates = {
    {
        gluttonChunkSpawn,
        gluttonChunkFall,
        gluttonChunkSettle,
        Gp_DestroyEnemy,
    },
};

/// State handlers of the dropped enemy: spawn, ascent, descent, landing and
/// teardown.
static const GpEnemyTaskFuncTable5 gGluttonRainStates = {
    {
        gluttonRainSpawn,
        gluttonRainRise,
        gluttonRainFall,
        gluttonRainSplat,
        Gp_DestroyEnemy,
    },
};

/// State handlers of the spinner enemy: spawn, hidden wait, chase and teardown.
static const GpEnemyTaskFuncTable4 gGluttonSpinnerStates = {
    {
        gluttonSpinnerSpawn,
        gluttonSpinnerWait,
        gluttonSpinnerChase,
        Gp_DestroyEnemy,
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
/// and, finding that bit set, stores 3 in `field_7F3` and replaces the flag
/// word with `TMD_OBJECT_SKIP_ACTIVE_DRAW`. 3 clears the word, pushes the
/// clear, then sets `TMD_OBJECT_SKIP_AUTO_BUFFER` on the host alone. Cases 0
/// and 2 also reset `field_0`. Case 2 tests the bit it just set, so that test
/// is always true.
s32 func_actor_444000_8013A958(Task* task, s32 msgId, s32 arg2)
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
                Tmd_AllocBuffers(tmd);
            }
            for (j = 0; j < 7; j++) {
                if (buffers->field_ECC[j] != NULL) {
                    escortTmd = buffers->field_ECC[j]->task->extra.tmd;
                    if (escortTmd->buffer == NULL) {
                        Tmd_AllocBuffers(escortTmd);
                    }
                }
            }
            escorts                = task->work;
            escorts->field_7F3     = 0;
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            for (i = 0; i < 7; i++) {
                if (escorts->field_ECC[i] != NULL) {
                    escorts->field_ECC[i]->task->extra.tmd->flags = task->extra.tmd->flags;
                }
            }
            work->field_0 = 0;
            break;
        case 1:
            escorts                = task->work;
            escorts->field_7F3     = 0;
            task->extra.tmd->flags = 0;
            for (i = 0; i < 7; i++) {
                if (escorts->field_ECC[i] != NULL) {
                    escorts->field_ECC[i]->task->extra.tmd->flags = task->extra.tmd->flags;
                }
            }
            hostTmd = task->extra.tmd;
            rebuilt = task->work;
            if (hostTmd->buffer == NULL) {
                Tmd_AllocBuffers(hostTmd);
            }
            for (j = 0; j < 7; j++) {
                if (rebuilt->field_ECC[j] != NULL) {
                    escortTmd = rebuilt->field_ECC[j]->task->extra.tmd;
                    if (escortTmd->buffer == NULL) {
                        Tmd_AllocBuffers(escortTmd);
                    }
                }
            }
            break;
        case 2:
            tmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            flags       = tmd->flags;
            escorts     = task->work;
            if (flags & TMD_OBJECT_SKIP_AUTO_BUFFER) {
                escorts->field_7F3     = 3;
                task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            } else {
                escorts->field_7F3     = 0;
                task->extra.tmd->flags = flags;
            }
            for (i = 0; i < 7; i++) {
                if (escorts->field_ECC[i] != NULL) {
                    escorts->field_ECC[i]->task->extra.tmd->flags = task->extra.tmd->flags;
                }
            }
            work->field_0 = 0;
            break;
        case 3:
            tmd->flags             = 0;
            escorts                = task->work;
            escorts->field_7F3     = 0;
            task->extra.tmd->flags = 0;
            for (i = 0; i < 7; i++) {
                if (escorts->field_ECC[i] != NULL) {
                    escorts->field_ECC[i]->task->extra.tmd->flags = task->extra.tmd->flags;
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

    ang       = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle = ang;
    gfxRotMatrixY(&sc->m, ang, 1);
    sc->scale.vx = 0x1000;
    sc->scale.vy = 0;
    sc->scale.vz = 0x1000;
    ScaleMatrix(&sc->m, &sc->scale);

    coord->coord.m[0][0] = sc->m.m[0][0];
    coord->coord.m[0][1] = sc->m.m[0][1];
    coord->coord.m[0][2] = sc->m.m[0][2];
    coord->coord.m[1][0] = sc->m.m[1][0];
    coord->coord.m[1][1] = sc->m.m[1][1];
    coord->coord.m[1][2] = sc->m.m[1][2];
    coord->coord.m[2][0] = sc->m.m[2][0];
    coord->coord.m[2][1] = sc->m.m[2][1];
    coord->coord.m[2][2] = sc->m.m[2][2];
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;

    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BYTES(sizeof(ActorScaleRotScratch));
    Gp_UpdateCoord(task->extra.tmd->coords);
}

/// The enemy task's 0x7DB message handler, listed in `D_actor_444000_80161818`.
/// The three payload bytes are always recorded in the work block; only messages
/// from sender 0x2804 act, and then on three of the selector's values. 0 and 1
/// both announce the state change with the same pair of cues, 1 additionally
/// re-arms the animation blocks and drops the model onto its start position,
/// and 19 switches the host and its fourth escort to light mode 2 before
/// raising eight floor vertices and flattening the model's rotation.
s32 func_actor_444000_8013ACD0(Task* task, s32 msgId, ActorCommand* msg)
{
    GluttonWork* work  = task->work;
    Enemy*       enemy = task->spawnArg2.pointer;
    SVECTOR*     verts;
    s32          action;

    work->field_EC4 = msg->context.loc.stage;
    work->field_EC5 = msg->context.loc.area;
    work->field_EC6 = (u8)msg->command;

    if (msg->context.key == 0x2804) {
        action = msg->command;
        switch (action) {
            case 0:
                work->field_0 = 0;
                SndEvt_EnqueueType7(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000A, 1);
                SndEvt_EnqueueType7(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000D, 1);
                break;

            case 1:
                work->field_7B3 = 0xA;
                work->field_7B0 = 2;
                gluttonTickAnim(task);
                gluttonTickAnim(task);
                gluttonTickAnim(task);
                work->field_7B6 = 1;
                gluttonTickAnim(task);
                work->field_7B6                       = 0x10;
                task->extra.tmd->coords->coord.t[0]   = -0xBB8;
                task->extra.tmd->coords->coord.t[1]   = 0;
                task->extra.tmd->coords->coord.t[2]   = -0x992;
                task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                work->field_0                         = 0x11;
                SndEvt_EnqueueType7(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000A, 1);
                SndEvt_EnqueueType7(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000D, 1);
                break;

            case 19:
                Gp_SetLightMode(enemy, ENEMY_COLOR_BLACK);
                Gp_SetLightMode(work->field_ECC[3], ENEMY_COLOR_BLACK);
                work->field_0   = action;
                work->field_2   = -1;
                work->field_F04 = 1;
                SndEvt_EnqueueType7(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x54280007, 1);

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

    ang       = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle = ang;
    gfxRotMatrixY(&sc->m, ang, 1);
    sc->scale.vx = sc->scale.vy = sc->scale.vz = 0x1000;
    ScaleMatrix(&sc->m, &sc->scale);

    coord->coord.m[0][0] = sc->m.m[0][0];
    coord->coord.m[0][1] = sc->m.m[0][1];
    coord->coord.m[0][2] = sc->m.m[0][2];
    coord->coord.m[1][0] = sc->m.m[1][0];
    coord->coord.m[1][1] = sc->m.m[1][1];
    coord->coord.m[1][2] = sc->m.m[1][2];
    coord->coord.m[2][0] = sc->m.m[2][0];
    coord->coord.m[2][1] = sc->m.m[2][1];
    coord->coord.m[2][2] = sc->m.m[2][2];
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;

    work->field_0          = 1;
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
/// (`Actor444000_SeedRootCoord`), the tenth collision object at `obj` is linked
/// by hand around the free coordinate `field_E3C`, and both the host and every
/// escort model are pointed at the work block's light and colour matrices
/// before the fight announces itself with message 0x7DA.
static void func_actor_444000_8013AFF8(Enemy* enemy, Task* task)
{
    GluttonWork* work;
    GluttonWork* buffers;
    GluttonWork* escorts;
    OverlayMat*  mtx;
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

    work       = memCalloc(0xF24, 0);
    task->work = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
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
    enemy->hp            = D_actor_444000_80144A28.hpMax;
    enemy->param         = &D_actor_444000_80144A28;
    enemy->recs          = work->hits[0].recs;

    func_800B3F84(&work->anim0, D_actor_444000_80161448, tmd, work->aux0, work->slots0);
    func_800B3F84(&work->anim1, D_actor_444000_80161448, tmd, work->aux1, work->slots1);

    work->field_7B0 = 2;
    work->field_7B3 = 2;
    work->field_EF8 = 1;
    work->field_7B1 = 0;
    work->field_7C4 = work->field_7C8 = 0;
    work->field_7B6 = work->field_7B8 = 0x10;

    func_8010C980(&task->extra.tmd->coords[4], &work->hits[0].obj, work->hits[0].recs, 5, 0x20, 0x300);
    func_8010C980(&task->extra.tmd->coords[4], &work->hits[1].obj, work->hits[1].recs, 5, 0x20, 0x300);
    func_8010C980(&task->extra.tmd->coords[1], &work->hits[2].obj, work->hits[2].recs, 5, 0x20, 0xBB8);

    work->hits[1].obj.pos.vx = 0;
    work->hits[1].obj.pos.vy = 0;
    work->hits[1].obj.pos.vz = -0x100;
    work->hits[2].obj.pos.vx = 0;
    work->hits[2].obj.pos.vy = 0x400;
    work->hits[2].obj.pos.vz = -0x400;

    Gfx_MatrixCol2(&task->extra.tmd->coords->coord, &dir);
    dir.vy = 0;
    gteDir = &dir;
    VectorNormalSS(gteDir, gteDir);
    gte_lddp(0x1388);
    gte_ldsv(gteDir);
    gte_gpf12();
    gte_stsv(gteDir);

    work->anim.source.sets          = NULL;
    work->anim.animationId          = 1;
    work->anim.blend                = ANIMATION_BLEND_RESET;
    work->anim.blendFrames          = 3;
    work->anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
    work->field_F12                 = 0;
    task->msgTable                  = D_actor_444000_80161818;
    coord->parent                   = &gGfxViewCoord;
    coord->composeStamp             = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);

    D_actor_444000_80161880.coord      = task->extra.tmd->coords;
    D_actor_444000_80161880.spawnArgLo = 0x100;
    D_actor_444000_80161880.spawnArgHi = 2;
    work->field_2                      = -1;

    model   = task->extra.tmd;
    buffers = task->work;
    if (model->buffer == NULL) {
        Tmd_AllocBuffers(model);
    }
    for (i = 0; i < 7; i++) {
        if (buffers->field_ECC[i] != NULL) {
            escortTmd = buffers->field_ECC[i]->task->extra.tmd;
            if (escortTmd->buffer == NULL) {
                Tmd_AllocBuffers(escortTmd);
            }
        }
    }

    Actor444000_SeedRootCoord(task, work);

    esc                                                     = Gp_SpawnEnemyFromTable(D_actor_444000_801616B0, 0, 0, task->spawnArg2.pointer);
    work->field_ECC[0]                                      = esc;
    esc->task->extra.tmd->coords->parent                    = task->extra.tmd->coords;
    work->field_ECC[0]->task->extra.tmd->coords->coord.t[0] = 0;
    work->field_ECC[0]->task->extra.tmd->coords->coord.t[1] = 0;
    work->field_ECC[0]->task->extra.tmd->coords->coord.t[2] = 0;
    work->field_ECC[0]->task->extra.tmd->flags              = 0;
    func_800B3F84(&work->anim2, D_actor_444000_80161500, work->field_ECC[0]->task->extra.tmd, work->aux2,
                  work->slots2);
    func_800B3F84(&work->anim3, D_actor_444000_80161500, work->field_ECC[0]->task->extra.tmd, work->aux3,
                  work->slots3);
    work->field_ECC[0]->field_4    = &task->extra.tmd->coords->coord;
    work->field_ECC[0]->field_48   = 0;
    work->field_ECC[0]->bodyPos.vx = 0xC8;
    work->field_ECC[0]->bodyPos.vy = 0;
    work->field_ECC[0]->bodyPos.vz = 0x3E8;
    work->field_ECC[0]->coord      = &work->field_ECC[0]->task->extra.tmd->coords[1];
    Gp_LinkNode(&work->field_ECC[0]->node);
    work->field_ECC[0]->reactionFlags = 0;
    work->field_ECC[0]->hp            = D_actor_444000_80144A28.hpMax;
    work->field_F0A                   = D_actor_444000_80144A38.hpMax;
    work->field_ECC[0]->param         = &D_actor_444000_80144A38;
    work->field_ECC[0]->recs          = work->hits[3].recs;
    func_8010C980(&work->field_ECC[0]->task->extra.tmd->coords[1], &work->hits[3].obj, work->hits[3].recs, 5,
                  0x20, 0x300);
    func_8010C980(&work->field_ECC[0]->task->extra.tmd->coords[2], &work->hits[4].obj, work->hits[4].recs, 5,
                  0x20, 0x300);
    func_8010C980(&work->field_ECC[0]->task->extra.tmd->coords[3], &work->hits[5].obj, work->hits[5].recs, 5,
                  0x20, 0x300);

    esc                                                     = Gp_SpawnEnemyFromTable(D_actor_444000_801616B0, 1, 0, task->spawnArg2.pointer);
    work->field_ECC[1]                                      = esc;
    esc->task->extra.tmd->coords->parent                    = task->extra.tmd->coords;
    work->field_ECC[1]->task->extra.tmd->coords->coord.t[0] = 0;
    work->field_ECC[1]->task->extra.tmd->coords->coord.t[1] = 0;
    work->field_ECC[1]->task->extra.tmd->coords->coord.t[2] = 0;
    work->field_ECC[1]->task->extra.tmd->flags              = 0;
    func_800B3F84(&work->anim4, D_actor_444000_801615B8, work->field_ECC[1]->task->extra.tmd, work->aux4,
                  work->slots4);
    func_800B3F84(&work->anim5, D_actor_444000_801615B8, work->field_ECC[1]->task->extra.tmd, work->aux5,
                  work->slots5);
    work->field_ECC[1]->field_4    = &task->extra.tmd->coords->coord;
    work->field_ECC[1]->field_48   = 0;
    work->field_ECC[1]->bodyPos.vx = -0xC8;
    work->field_ECC[1]->bodyPos.vy = 0;
    work->field_ECC[1]->bodyPos.vz = 0x3E8;
    work->field_ECC[1]->coord      = &work->field_ECC[1]->task->extra.tmd->coords[1];
    Gp_LinkNode(&work->field_ECC[1]->node);
    work->field_ECC[1]->reactionFlags = 0;
    work->field_ECC[1]->hp            = D_actor_444000_80144A28.hpMax;
    work->field_F0C                   = D_actor_444000_80144A48.hpMax;
    work->field_ECC[1]->param         = &D_actor_444000_80144A48;
    work->field_ECC[1]->recs          = work->hits[6].recs;
    func_8010C980(&work->field_ECC[1]->task->extra.tmd->coords[1], &work->hits[6].obj, work->hits[6].recs, 5,
                  0x20, 0x300);
    func_8010C980(&work->field_ECC[1]->task->extra.tmd->coords[2], &work->hits[7].obj, work->hits[7].recs, 5,
                  0x20, 0x300);
    func_8010C980(&work->field_ECC[1]->task->extra.tmd->coords[3], &work->hits[8].obj, work->hits[8].recs, 5,
                  0x20, 0x300);

    esc                                                     = Gp_SpawnEnemyFromTable(D_actor_444000_801616B0, 2, 0, task->spawnArg2.pointer);
    work->field_ECC[2]                                      = esc;
    esc->task->extra.tmd->coords->parent                    = &task->extra.tmd->coords[4];
    work->field_ECC[2]->task->extra.tmd->coords->coord.t[0] = 0;
    work->field_ECC[2]->task->extra.tmd->coords->coord.t[1] = 0x59;
    work->field_ECC[2]->task->extra.tmd->coords->coord.t[2] = -0x64;
    work->field_ECC[2]->task->extra.tmd->flags              = 0;

    esc                                                     = Gp_SpawnEnemyFromTable(D_actor_444000_801616B0, 3, 0, task->spawnArg2.pointer);
    work->field_ECC[3]                                      = esc;
    esc->task->extra.tmd->coords->parent                    = &task->extra.tmd->coords[3];
    work->field_ECC[3]->task->extra.tmd->coords->coord.t[0] = 0;
    work->field_ECC[3]->task->extra.tmd->coords->coord.t[1] = 0;
    work->field_ECC[3]->task->extra.tmd->coords->coord.t[2] = 0;
    work->field_ECC[3]->task->extra.tmd->flags              = 0;
    work->field_ECC[3]->field_4                             = &task->extra.tmd->coords->coord;
    work->field_ECC[3]->field_48                            = 0;
    work->field_ECC[3]->bodyPos.vx                          = 0;
    work->field_ECC[3]->bodyPos.vy                          = 0;
    work->field_ECC[3]->bodyPos.vz                          = 0x514;
    work->field_ECC[3]->coord                               = work->field_ECC[3]->task->extra.tmd->coords;
    Gp_LinkNode(&work->field_ECC[3]->node);
    work->field_ECC[3]->reactionFlags = 0;
    work->field_ECC[3]->hp            = D_actor_444000_80144A28.hpMax;
    work->field_F0E                   = D_actor_444000_80144A58.hpMax;
    work->field_ECC[3]->param         = &D_actor_444000_80144A58;
    work->field_ECC[3]->recs          = work->hits[1].recs;

    freeCoord                             = &work->field_E3C.c;
    work->field_E3C.c.parent              = task->extra.tmd->coords;
    work->field_E3C.ident.rotation.m00M01 = ONE;
    mtx                                   = (OverlayMat*)&work->field_E3C.c.coord;
    mtx->ident.m02M10                     = 0;
    mtx->ident.m11M12                     = ONE;
    mtx->ident.m20M21                     = 0;
    mtx->ident.m22                        = ONE;
    work->field_E3C.c.coord.t[0] = work->field_E3C.c.coord.t[1] = work->field_E3C.c.coord.t[2] = 0;
    work->field_E3C.c.composeStamp                                                             = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(freeCoord);

    work->d4rec.ends[1].vz    = 0x1B58;
    work->d4rec.end0Radius    = 0x258;
    work->d4rec.end1Radius    = 0x258;
    work->d4rec.ends[0].vx    = 0;
    work->d4rec.ends[0].vy    = 0;
    work->d4rec.ends[0].vz    = 0;
    work->d4rec.ends[1].vx    = 0;
    work->d4rec.ends[1].vy    = 0;
    work->d4rec.contacts      = work->recs2;
    work->obj.coord           = freeCoord;
    work->obj.context.capsule = &work->d4rec;
    work->obj.pos.vx          = 0;
    work->obj.pos.vy          = -0xFA;
    work->obj.pos.vz          = 0x25F;
    work->obj.key             = 0x30000 | 0x20;
    work->obj.radius          = 0;
    work->obj.flags           = WORLD_COLLISION_BODY_CAPSULE;
    Gp_LinkObj(2, &work->obj);
    Gp_InitRec18Table(work->recs2, 5, 0);
    work->obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    esc                                                     = Gp_SpawnEnemyFromTable(D_actor_444000_801616B0, 4, 0, task->spawnArg2.pointer);
    work->field_ECC[4]                                      = esc;
    esc->task->extra.tmd->coords->parent                    = &task->extra.tmd->coords[4];
    work->field_ECC[4]->task->extra.tmd->coords->coord.t[0] = 0;
    work->field_ECC[4]->task->extra.tmd->coords->coord.t[1] = 0;
    work->field_ECC[4]->task->extra.tmd->coords->coord.t[2] = 0x14;
    work->field_ECC[4]->task->extra.tmd->flags              = 0;

    esc                                                     = Gp_SpawnEnemyFromTable(D_actor_444000_801616B0, 5, 0, task->spawnArg2.pointer);
    work->field_ECC[5]                                      = esc;
    esc->task->extra.tmd->coords->parent                    = &task->extra.tmd->coords[2];
    work->field_ECC[5]->task->extra.tmd->coords->coord.t[0] = 0;
    work->field_ECC[5]->task->extra.tmd->coords->coord.t[1] = 0x67C;
    work->field_ECC[5]->task->extra.tmd->coords->coord.t[2] = 0xC8;
    work->field_ECC[5]->task->extra.tmd->flags              = 0;

    if ((task->spawnArg1.value >> 16) == 0) {
        esc                                                     = Gp_SpawnEnemyFromTable(D_actor_444000_801616B0, 6, 0, task->spawnArg2.pointer);
        work->field_ECC[6]                                      = esc;
        esc->task->extra.tmd->coords->parent                    = &task->extra.tmd->coords[1];
        work->field_ECC[6]->task->extra.tmd->coords->coord.t[0] = 0;
        work->field_ECC[6]->task->extra.tmd->coords->coord.t[1] = 0x62C;
        work->field_ECC[6]->task->extra.tmd->coords->coord.t[2] = 0x5DC;
        work->field_ECC[6]->task->extra.tmd->flags              = 0;
    } else {
        work->field_ECC[6] = NULL;
    }

    work->field_F04 = 0;
    work->field_F06 = 0;
    work->field_F08 = 0;
    work->field_F0C = (s16)D_actor_444000_80144A48.hpMax;
    work->field_F0A = (s16)D_actor_444000_80144A38.hpMax;

    escorts = task->work;

    gGluttonEnded = 0;

    task->extra.tmd->lightMtx = &escorts->lightMtx;
    task->extra.tmd->colorMtx = &escorts->colorMtx;
    for (j = 0; j < 7; j++) {
        esc = escorts->field_ECC[j];
        if (esc != NULL) {
            escTask                      = esc->task;
            escTask->extra.tmd->lightMtx = &escorts->lightMtx;
            escTask->extra.tmd->colorMtx = &escorts->colorMtx;
        }
    }

    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    if (work->field_EFA != work->field_EFC) {
        Gp_UpdateActorColor(enemy, &pos, 0, 0);
        Gp_UpdateActorColor(work->field_ECC[3], &pos, 0, 0);
        work->field_EFC = work->field_EFA;
    }
    if (work->field_EFA != 0) {
        Gp_UpdateActorColor(enemy, &pos, 0, 0);
    } else {
        Gp_UpdateActorColor(work->field_ECC[3], &pos, 0, 0);
    }
    gluttonTickAnim(task);

    D_actor_444000_80161888.value.context.loc.stage = 0;
    D_actor_444000_80161888.value.context.loc.area  = 0x2C;
    D_actor_444000_80161888.value.command           = 0;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_actor_444000_80161888.value, ACTOR_COMMAND_MESSAGE_APPLY);

    work->field_E94 = work->field_E96 = 0xFA0;
    for (k = 0; k < 2; k++) {
        work->field_EE8[k] = NULL;
    }

    gSceneCombatState.battleRefs = 0xA;
    Gp_ReleaseStateF0Add(task, 0x20);
    _gGluttonHostTask.task = task;
    work->field_F1B = work->field_F1C = 0;
    task->state                      += 1;
}

#include "../../shared/glutton_hit_group0.inc.c"

#include "../../shared/glutton_hit_groups1to2.inc.c"

/// The hit handler for collision groups 3, 4 and 5 -- `gluttonHitGroups1To2`
/// done three times, the next group only scanned when the previous one landed
/// nothing and the part it hit reported no attack id back. Unlike groups 1 and 2
/// this one runs no `Gp_GetIdParam0` switch: the call is made and its kind
/// thrown away, so every hit is treated alike.
///
/// The damage is the distance-scaled hit quadrupled when `Gp_RollEnemyChance`
/// fires, then divided by six (never down to zero unless it already was), and
/// comes off the host, the first escort and `field_F0A`. Emptying that pool
/// spawns the same effect again and refills it from
/// `D_actor_444000_80144A38.hpMax`. Both effect spawns and the state change to 0xE
/// are skipped while the boss is in one of the seven states that ignore hits,
/// while `field_F08` is clear, or while the player hold is armed.
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
    sc   = (GluttonHitScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(GluttonHitScratch));
    pos  = &sc->pos;
    recs = work->hits[3].recs;
    for (i = 0; i < 5; i++) {
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
    sc->id = id;
    if (id != 0) {
        coord = work->hits[3].obj.coord;
        goto hit;
    }

    pos2  = &sc->pos;
    recs2 = work->hits[4].recs;
    for (i2 = 0; i2 < 5; i2++) {
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
    sc->id = id;
    if (id != 0) {
        coord = work->hits[4].obj.coord;
    hit:
        gluttonHitEffect(coord, id);
        if (sc->id != 0) {
            goto body;
        }
    }

    pos3  = &sc->pos;
    recs3 = work->hits[5].recs;
    for (i3 = 0; i3 < 5; i3++) {
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
    sc->id = id;
    if (id == 0) {
        goto out;
    }
    gluttonHitEffect(work->hits[5].obj.coord, id);
    if (sc->id == 0) {
        goto out;
    }
body:
    work->field_E8E = Gp_GetIdParam2(sc->id);
    Gp_GetIdParam0(sc->id);

    sc->delta.vx = (cfg->coordMtx->t[0] - task->extra.tmd->coords->coord.t[0]) + 0x51F;
    dx2          = sc->delta.vx * sc->delta.vx;
    sc->delta.vy = (cfg->coordMtx->t[1] - task->extra.tmd->coords->coord.t[1]) - 0xFA;
    dy2          = sc->delta.vy * sc->delta.vy;
    sc->delta.vz = (cfg->coordMtx->t[2] - task->extra.tmd->coords->coord.t[2]) + 0x25F;
    dz2          = sc->delta.vz * sc->delta.vz;
    sc->dist     = SquareRoot0(dx2 + dy2 + dz2);
    sc->damage   = Gp_ComputeDamage(sc->id, sc->dist, 0, 0);

    if (Gp_RollEnemyChance(work->field_ECC[0], sc->id, 0) != 0 && (state = work->field_0, state != 0xD) && state != 3 &&
        state != 9 && state != 0xE && state != 0xF && state != 8 && state != 0xB && work->field_F08 != 0 &&
        work->field_EC8 != 1) {
        sc->rot.vz = 0x1F4;
        sc->rot.vy = 0;
        sc->rot.vx = 0;
        sc->rot.vy = 0;
        sc->rot.vx = 0;
        sc->rot.vz = 0x258;
        Gp_SpawnEff(0x6009C, &work->field_ECC[0]->task->extra.tmd->coords[1], 0, &sc->rot);
        sc->damage   *= 4;
        work->field_0 = 0xE;
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
    func_800E2C78(host, sc->id, sc->damage, 0);
    host->hp        -= sc->damage;
    work->field_F0A -= sc->damage;
    if (work->field_F0A <= 0 && (state = work->field_0, state != 0xD) && state != 3 && state != 9 && state != 0xE &&
        state != 0xF && state != 8 && state != 0xB && work->field_F08 != 0 && work->field_EC8 != 1) {
        sc->rot.vy = 0;
        sc->rot.vx = 0;
        sc->rot.vz = 0x258;
        Gp_SpawnEff(0x6009C, &work->field_ECC[0]->task->extra.tmd->coords[1], 0, &sc->rot);
        work->field_0   = 0xE;
        work->field_F0A = (s16)D_actor_444000_80144A38.hpMax;
    }

    func_800DA6E8(&work->field_ECC[0]->node, sc->damage, 0);
    work->field_ECC[0]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(work->field_ECC[0]->task->extra.tmd->coords);
    sc->rot.vx = sc->pos.vx - work->field_ECC[0]->task->extra.tmd->coords->workm.t[0];
    sc->rot.vy = sc->pos.vy - work->field_ECC[0]->task->extra.tmd->coords->workm.t[1];
    sc->rot.vz = sc->pos.vz - work->field_ECC[0]->task->extra.tmd->coords->workm.t[2];
    angle      = ratan2(sc->rot.vx, sc->rot.vz) -
            ratan2(-task->extra.tmd->coords->workm.m[2][0],
                   task->extra.tmd->coords->workm.m[2][2]);
    sc->angle = angle;
    if (angle < 0) {
    wrapUp3:
        if (angle < -0x800) {
            angle += 0x1000;
            goto wrapUp3;
        }
    } else {
    wrapDown3:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto wrapDown3;
        }
    }
    sc->angle = angle;

    if (work->field_7B3 != 4) {
        work->field_7C8 = 0;
        work->field_7C4 = 0;
    }
out:
    SCRATCH_STACK_RELEASE_BYTES(sizeof(GluttonHitScratch));
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
    if (work->field_4 != 0) {
        escorts                = arg0->work;
        work->field_7F3        = 3;
        arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                escorts->field_ECC[i]->task->extra.tmd->flags = arg0->extra.tmd->flags;
            }
        }
        work->field_7B3 = 0xA;
        work->field_7B0 = 2;
        work->field_6   = 0;
        work->field_7B6 = 0x10;
        gluttonTickAnim(arg0);
    } else {
        if (work->field_6 < 0xA) {
            gluttonTickAnim(arg0);
        }
        if (work->field_6 == 2) {
            dying = arg0->work;
            Tmd_FreeBuffers(arg0->extra.tmd);
            for (j = 0; j < 7; j++) {
                if (dying->field_ECC[j] != NULL) {
                    Tmd_FreeBuffers(dying->field_ECC[j]->task->extra.tmd);
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

    ang       = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle = ang;
    gfxRotMatrixY(&sc->m, ang, 1);
    sc->scale.vx = 0x1000;
    sc->scale.vy = vy;
    sc->scale.vz = 0x1000;
    ScaleMatrix(&sc->m, &sc->scale);

    coord->coord.m[0][0] = sc->m.m[0][0];
    coord->coord.m[0][1] = sc->m.m[0][1];
    coord->coord.m[0][2] = sc->m.m[0][2];
    coord->coord.m[1][0] = sc->m.m[1][0];
    coord->coord.m[1][1] = sc->m.m[1][1];
    coord->coord.m[1][2] = sc->m.m[1][2];
    coord->coord.m[2][0] = sc->m.m[2][0];
    coord->coord.m[2][1] = sc->m.m[2][1];
    coord->coord.m[2][2] = sc->m.m[2][2];
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
}

/// Re-arm handler run once the block asks for a reset: clear the host's model
/// flag word onto itself and every escort, drop the two counters at 0xEF4, then
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
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags = 0;
        escorts                = arg0->work;
        escorts->field_7F3     = 0;
        arg0->extra.tmd->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                escorts->field_ECC[i]->task->extra.tmd->flags = arg0->extra.tmd->flags;
            }
        }
        work->field_EF4 = 0;
        work->field_EF6 = 0;
    }

    gluttonTickAnim(arg0);

    Actor444000_FlattenRotation(work->field_ECC[2]->task->extra.tmd->coords, 0);
    work->field_ECC[2]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Actor444000_ReleaseRotScratch();

    Actor444000_FlattenRotation(arg0->extra.tmd->coords, 0);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Actor444000_ReleaseRotScratch();

    Actor444000_FlattenRotation(work->field_ECC[4]->task->extra.tmd->coords, 0);
    work->field_ECC[4]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Actor444000_ReleaseRotScratch();

    Actor444000_FlattenRotation(work->field_ECC[3]->task->extra.tmd->coords, 0x400);
    work->field_ECC[3]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Actor444000_ReleaseRotScratch();

    Actor444000_FlattenRotation(work->field_ECC[0]->task->extra.tmd->coords, 0);
    work->field_ECC[0]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Actor444000_ReleaseRotScratch();

    Actor444000_FlattenRotation(work->field_ECC[1]->task->extra.tmd->coords, 0);
    work->field_ECC[2]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Actor444000_ReleaseRotScratch();
}

/// Drag tick of the arena fight: the state the boss runs while it is hauling the
/// player in along the line between them.
///
/// A reset request (`field_4`) re-arms the block on animation 3, clears the host
/// model's flag word and pushes it onto each of the seven escorts' models, makes
/// sure the host and every escort has its model buffers allocated, re-seeds the
/// spinner target `gGluttonSpinnerTarget` from the fourth part of slot 4's
/// model and announces sub-state 2 through message 0x7DA.
///
/// Every tick then pins the player down to the arena floor, runs the ordinary
/// re-arm and stows the yaw from the host to the player -- relative to the
/// host's own facing, wrapped to +/-0x800 -- in `field_7C4`. The host's fifth
/// part is carried into view space, the player-relative offset from there gives
/// the direction and distance the pull works along, and the animation frame
/// picks how hard: `pull` is the phase's base strength and the frame divides
/// `-(pull + 0x19)` by 1, 2, 3, 4, 6 or 2/3 before `gte_gpf12` scales the
/// normalised direction by it. Frames outside 9..20 drop the pull and clear
/// `field_EFA`. `func_80105B74` hands the result to the player actor unless the
/// game is in mode 2 or 0xA or the player is already in mode 2.
///
/// Alongside that: a script fires every `period` frames while the frame sits in
/// 0xA..0x12, two cues play on frames 0x3C and 0xE8, the fight asks slot 3 for
/// the hold (message 0x3F8) once the player is inside 0x4B0 on frames 0xB..0xF
/// and phase 6 onward clamps the player back behind -0x52D0. Once `slots0[1]`
/// raises its flag the fight announces sub-state 3, moves to state 0xA and drops
/// its two spawned escorts.
static void func_actor_444000_8013E058(Task* task)
{
    GluttonWork*            work  = task->work;
    Enemy*                  enemy = task->spawnArg2.pointer;
    Task*                   slot3;
    GameActor*              actor;
    Actor444000DragScratch* sc;
    GluttonWork*            escorts;
    GluttonWork*            buffers;
    PlayerStatus*           cfg;
    GfxCoord*               coord;
    GfxCoord*               facing;
    GfxCoord*               yawCoord;
    GfxCoord*               clamp;
    SVECTOR*                posp;
    SVECTOR*                dirp;
    TmdObject*              tmd;
    TmdObject*              escortTmd;
    s16                     angle;
    s16                     i;
    s16                     j;
    s16                     dz;

    slot3 = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    sc    = (Actor444000DragScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(Actor444000DragScratch));
    actor = slot3->work;

    if (work->field_4 != 0) {
        work->field_7B3        = 3;
        work->field_7B0        = 2;
        escorts                = task->work;
        escorts->field_7F3     = 0;
        task->extra.tmd->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                escorts->field_ECC[i]->task->extra.tmd->flags = task->extra.tmd->flags;
            }
        }
        tmd     = task->extra.tmd;
        buffers = task->work;
        if (tmd->buffer == NULL) {
            Tmd_AllocBuffers(tmd);
        }
        for (j = 0; j < 7; j++) {
            if (buffers->field_ECC[j] != NULL) {
                escortTmd = buffers->field_ECC[j]->task->extra.tmd;
                if (escortTmd->buffer == NULL) {
                    Tmd_AllocBuffers(escortTmd);
                }
            }
        }
        work->field_EFE = 0;
        work->field_EF4 = 1;
        work->field_EF6 = 1;
        work->field_EFA = 0;
        posp            = &gGluttonSpinnerTarget;
        posp->vz        = 0;
        posp->vy        = 0;
        posp->vx        = 0;
        actorLocalToView(&Gp_LookupSlot4(0)->extra.tmd->coords[3], posp);
        D_actor_444000_80161888.value.context.loc.stage = 0;
        D_actor_444000_80161888.value.context.loc.area  = 0x2C;
        D_actor_444000_80161888.value.command           = 2;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_actor_444000_80161888.value, ACTOR_COMMAND_MESSAGE_APPLY);
    }

    coord = slot3->extra.tmd->coords;
    if (coord->coord.t[1] > 0) {
        coord->coord.t[1]                      = 0;
        slot3->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    gluttonTickAnim(task);

    cfg        = &gPlayerStatus;
    facing     = task->extra.tmd->coords;
    dirp       = &sc->dir;
    sc->dir.vx = (u16)cfg->coordMtx->t[0] - (u16)facing->coord.t[0];
    dirp->vy   = (u16)cfg->coordMtx->t[1] - (u16)facing->coord.t[1];
    dz         = (u16)cfg->coordMtx->t[2] - (u16)facing->coord.t[2];
    dirp->vz   = dz;
    yawCoord   = task->extra.tmd->coords;
    angle      = ratan2(sc->dir.vx, dz) - ratan2(-yawCoord->coord.m[2][0], yawCoord->coord.m[2][2]);
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
    work->field_7C4 = angle;

    sc->dir.vz = 0;
    sc->dir.vy = 0;
    sc->dir.vx = 0;
    actorLocalToView(&task->extra.tmd->coords[4], &sc->dir);

    sc->dir.vx = (u16)slot3->extra.tmd->coords->coord.t[0] - (u16)sc->dir.vx;
    sc->dir.vy = (u16)slot3->extra.tmd->coords->coord.t[1] - (u16)sc->dir.vy;
    sc->dir.vz = (u16)slot3->extra.tmd->coords->coord.t[2] - (u16)sc->dir.vz;
    sc->dist   = sc->dir.vx * sc->dir.vx;
    sc->dist  += sc->dir.vz * sc->dir.vz;
    sc->dist   = SquareRoot0(sc->dist);
    VectorNormalSS(&sc->dir, &sc->dir);

    switch (work->field_F08) {
        case 0:
            sc->period = 0x19;
            break;
        case 1:
            sc->period = 0x11;
            break;
        case 2:
        default:
            sc->period = 0xE;
            break;
    }
    if (((u32)((work->slots0[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) - 0xA) < 9U) && ((work->field_6 % sc->period) == 0)) {
        Gp_SpawnScript18(D_actor_444000_80144A94, D_actor_444000_80144AA0);
    }

    switch (work->field_F08) {
        case 0:
        case 6:
            sc->pull = 0;
            break;
        case 1:
            sc->pull = 5;
            break;
        case 2:
            sc->pull = 0xA;
            break;
        case 3:
        case 4:
        case 5:
        default:
            sc->pull = 0xF;
            break;
    }

    if (work->field_6 == 0x3C) {
        s32 id;
        s32 pan;

        id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000A;
        pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    if (work->field_6 == 0xE8) {
        SndEvt_EnqueueType7((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000A, 1);
    }

    work->field_EFA = 1;
    switch (work->slots0[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) {
        case 9:
            gte_lddp(-(sc->pull + 0x19) / 4);
            gte_ldsv(&sc->dir);
            gte_gpf12();
            gte_stsv(&sc->dir);
            work->field_EFE = 0x180;
            break;
        case 10:
            gte_lddp(-(sc->pull + 0x19) / 2);
            gte_ldsv(&sc->dir);
            gte_gpf12();
            gte_stsv(&sc->dir);
            break;
        case 11:
        case 13:
        case 15:
            gte_lddp(-(sc->pull + 0x19));
            gte_ldsv(&sc->dir);
            gte_gpf12();
            gte_stsv(&sc->dir);
            work->field_EFE = 0x2B2;
            break;
        case 12:
        case 14:
            gte_lddp(-((sc->pull + 0x19) * 3) / 2);
            gte_ldsv(&sc->dir);
            gte_gpf12();
            gte_stsv(&sc->dir);
            work->field_EFE = 0x500;
            break;
        case 16:
            gte_lddp(-(sc->pull + 0x19) / 3);
            gte_ldsv(&sc->dir);
            gte_gpf12();
            gte_stsv(&sc->dir);
            work->field_EFE = 0x100;
            break;
        case 17:
        case 18:
            gte_lddp(-(sc->pull + 0x19) / 3);
            gte_ldsv(&sc->dir);
            gte_gpf12();
            gte_stsv(&sc->dir);
            work->field_EFE = 0x400;
            break;
        case 19:
        case 20:
            sc->dir.vz = 0;
            sc->dir.vx = 0;
            gte_lddp(-(sc->pull + 0x19) / 6);
            gte_ldsv(&sc->dir);
            gte_gpf12();
            gte_stsv(&sc->dir);
            work->field_EFE = 0;
            break;
        default:
            work->field_EFA = 0;
            sc->dir.vz      = 0;
            sc->dir.vx      = 0;
            break;
    }

    if (((u32)((work->slots0[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) - 0xB) < 5U) && (sc->dist < 0x4B0) && (work->field_F08 < 6)) {
        if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3F8, &D_actor_444000_80161928.value, 0) == 0) {
            work->field_0   = 0xD;
            work->field_EC8 = 1;
        }
    }
    if (work->field_F08 >= 6) {
        clamp = slot3->extra.tmd->coords;
        if (clamp->coord.t[2] > -0x52D0) {
            clamp->coord.t[2] = -0x52D0;
        }
    }

    if (sc->dir.vx != 0 || sc->dir.vz != 0) {
        sc->push.vx = sc->dir.vx;
        sc->push.vy = 0;
        sc->push.vz = sc->dir.vz;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != 2 && gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != 0xA && actor->mode != GAME_ACTOR_MODE_SCRIPTED) {
            func_80105B74(&sc->push);
        }
    }

    if (work->slots0[1].flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        D_actor_444000_80161888.value.context.loc.stage = 0;
        D_actor_444000_80161888.value.context.loc.area  = 0x2C;
        D_actor_444000_80161888.value.command           = 3;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_actor_444000_80161888.value, ACTOR_COMMAND_MESSAGE_APPLY);
        work->field_0 = 0xA;
        for (sc->i = 0; sc->i < 2; sc->i++) {
            work->field_EE8[sc->i] = NULL;
        }
    }
    work->field_F1C = 0;
    SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor444000DragScratch));
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
/// 0xE, the last two also kicking the pad -- with `field_7A8` remembering the
/// frame so none repeats while it is held. While message 0x3ED reports the
/// player free they are put back on the host's own position, and sub-state 0x17
/// re-places them and re-sends the 0x3FF animation.
/// Places the player in front of the host and points the pair at each other:
/// the host's fifth part is carried into view space, the yaw from there to the
/// player picks which of the two message-0x3FF animation tables the tick will
/// send (`..._80161680` past a quarter turn, `..._80161670` within it), and the
/// opposite yaw is stowed in `field_7C4` for the drive step. The normalised
/// direction scaled to 0x384 is where the player is asked to stand.
static __inline__ void Actor444000_PlacePlayerAhead(Task* task, GluttonWork* work,
                                                    Task* player, Actor444000WarpScratch* sc,
                                                    PlayerStatus* cfg)
{
    GfxCoord* coord;
    GfxCoord* facing;
    s16       angle;
    s32       yaw;

    sc->pos.vz = 0;
    sc->pos.vy = 0;
    sc->pos.vx = 0;
    actorLocalToView(&task->extra.tmd->coords[4], &sc->pos);

    sc->dir.vx = sc->pos.vx - player->extra.tmd->coords->coord.t[0];
    sc->dir.vy = 0;
    sc->dir.vz = sc->pos.vz - player->extra.tmd->coords->coord.t[2];
    facing     = player->extra.tmd->coords;
    angle      = ratan2(sc->dir.vx, sc->dir.vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
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
    yaw       = angle;
    sc->angle = yaw;
    if (abs(sc->angle) > 0x400) {
        if (sc->angle > 0) {
            sc->angle = yaw - 0x800;
        } else {
            sc->angle = yaw + 0x800;
        }
        work->anim.source.sets = (D_actor_444000_80161670 + 4);
    } else {
        work->anim.source.sets = D_actor_444000_80161670;
    }
    coord      = player->extra.tmd->coords;
    sc->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);

    sc->dir.vx = player->extra.tmd->coords->coord.t[0] - sc->pos.vx;
    sc->dir.vy = 0;
    sc->dir.vz = player->extra.tmd->coords->coord.t[2] - sc->pos.vz;
    facing     = task->extra.tmd->coords;
    angle      = ratan2(sc->dir.vx, sc->dir.vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    if (angle < 0) {
    wrapUp2:
        if (angle < -0x800) {
            angle += 0x1000;
            goto wrapUp2;
        }
    } else {
    wrapDown2:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto wrapDown2;
        }
    }
    work->field_7C4 = angle;

    VectorNormalSS(&sc->dir, &sc->dir);
    gte_lddp(0x384);
    gte_ldsv(&sc->dir);
    gte_gpf12();
    gte_stsv(&sc->dir);

    D_actor_444000_80161908.value.pos.vx = sc->pos.vx + sc->dir.vx;
    D_actor_444000_80161908.value.pos.vy = player->extra.tmd->coords->coord.t[1];
    D_actor_444000_80161908.value.pos.vz = sc->pos.vz + sc->dir.vz;
    D_actor_444000_80161908.value.rot.vx = 0;
    D_actor_444000_80161908.value.rot.vy = sc->angle;
    D_actor_444000_80161908.value.rot.vz = 0;
    if (cfg->hp > 0) {
        TASK_MESSAGE_DISPATCH_POINTER(player, 0x3E9, &D_actor_444000_80161908.value, 0);
    }
}

static void func_actor_444000_8013EC84(Task* arg0)
{
    Actor444000WarpScratch* sc;
    GluttonWork*            work;
    GluttonWork*            escorts;
    GluttonWork*            buffers;
    Enemy*                  enemy;
    Task*                   player;
    PlayerStatus*           cfg;
    Task*                   target;
    TmdObject*              tmd;
    TmdObject*              escortTmd;
    SVECTOR*                verts;
    s16                     i;
    s16                     j;
    s32                     frame;
    s32                     cueId;
    s32                     cuePan;
    s32                     hitId;
    s32                     hitPan;
    s32                     endId;
    s32                     endPan;

    work   = arg0->work;
    enemy  = arg0->spawnArg2.pointer;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    cfg    = &gPlayerStatus;

    if (work->field_4 != 0) {
        sc = (Actor444000WarpScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(Actor444000WarpScratch));

        verts        = Gp_GridParams->vertices;
        verts[24].vy = 0x1F4;
        verts[25].vy = 0x1F4;
        verts[26].vy = 0x320;
        verts[27].vy = 0x320;
        verts[28].vy = 0x1F4;
        verts[29].vy = 0x1F4;
        verts[30].vy = 0x320;
        verts[31].vy = 0x320;

        work->field_7B3    = 0xF;
        work->field_7B0    = 2;
        escorts            = arg0->work;
        escorts->field_7F3 = 0;

        arg0->extra.tmd->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                escorts->field_ECC[i]->task->extra.tmd->flags = arg0->extra.tmd->flags;
            }
        }

        tmd     = arg0->extra.tmd;
        buffers = arg0->work;
        if (tmd->buffer == NULL) {
            Tmd_AllocBuffers(tmd);
        }
        for (j = 0; j < 7; j++) {
            if (buffers->field_ECC[j] != NULL) {
                escortTmd = buffers->field_ECC[j]->task->extra.tmd;
                if (escortTmd->buffer == NULL) {
                    Tmd_AllocBuffers(escortTmd);
                }
            }
        }

        work->field_EFE = 0;
        work->field_EF4 = 0;
        work->field_EF6 = 1;
        work->field_EFA = 0;

        sc->delta.vx = player->extra.tmd->coords->coord.t[0] -
                       arg0->extra.tmd->coords->coord.t[0];
        sc->delta.vz = player->extra.tmd->coords->coord.t[2] -
                       arg0->extra.tmd->coords->coord.t[2];
        sc->dist = SquareRoot0(sc->delta.vx * sc->delta.vx + sc->delta.vz * sc->delta.vz);
        if (sc->dist < 0xB54) {
            sc->dir.vy = 0;
            sc->dir.vx = sc->delta.vx;
            sc->dir.vz = sc->delta.vz;
            VectorNormalSS(&sc->dir, &sc->dir);
            gte_lddp(0xCE4);
            gte_ldsv(&sc->dir);
            gte_gpf12();
            gte_stsv(&sc->dir);
            player->extra.tmd->coords->coord.t[0] =
                arg0->extra.tmd->coords->coord.t[0] + sc->dir.vx;
            player->extra.tmd->coords->coord.t[2] =
                arg0->extra.tmd->coords->coord.t[2] + sc->dir.vz;
            player->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(player->extra.tmd->coords);
        }

        Actor444000_PlacePlayerAhead(arg0, work, player, sc, cfg);

        D_actor_444000_80161868.value = 0;
        Gp_StateC08.field_6          |= 1;
        Gp_PulseState1C();
        Gp_ClearNodeSlots(&enemy->node);
        Gp_ClearNodeSlots(&work->field_ECC[3]->node);
        Gp_ClearNodeSlots(&work->field_ECC[0]->node);
        Gp_ClearNodeSlots(&work->field_ECC[1]->node);
        SndEvt_EnqueueType7((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000A, 1);

        D_actor_444000_80161888.value.context.loc.stage = 0;
        D_actor_444000_80161888.value.context.loc.area  = 0x2C;
        D_actor_444000_80161888.value.command           = 3;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_actor_444000_80161888.value, ACTOR_COMMAND_MESSAGE_APPLY);
    } else {
        sc = (Actor444000WarpScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(Actor444000WarpScratch));
        gluttonTickAnim(arg0);

        if ((work->slots0[1].flags & ANIMATION_SLOT_REACHED_BOUNDARY) && work->field_7B3 == 0xF) {
            work->field_7B0 = 2;
            work->field_7B3 = 0xE;
        }

        if (work->field_7B3 == 0xF) {
            if (cfg->hp > 0) {
                target = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                Gp_DispatchMsg(target, 0x3F9, Gp_PackObjPair(enemy, 3), 0);
                if (cfg->hp <= 0) {
                    ((GameActor*)player->work)->state = 0xA;
                    gGameSession->deathSoundCountdown = 0x1E;
                    gGameSession->deathFadeFrames     = 0x36;
                    gGameSession->deathRestartDelay   = 0x5A;
                }
            }
            frame = work->slots0[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (frame == 0x19 && work->field_7A8 != frame) {
                cueId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200011;
                cuePan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                SndEvt_EnqueueType6(cueId, cuePan,
                                    (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            }
            work->field_7A8 = work->slots0[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        }

        if (work->field_7B3 == 0xE) {
            frame = work->slots0[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (frame == 0x1D && work->field_7A8 != frame) {
                Gp_SpawnPadLerp(4, 0xFF, 8);
            }
            frame = work->slots0[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (frame == 0x23 && work->field_7A8 != frame) {
                hitId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200012;
                hitPan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                SndEvt_EnqueueType6(hitId, hitPan,
                                    (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                Gp_SpawnPadLerp(4, 0xFF, 8);
            }
            frame = work->slots0[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (frame == 0x27 && work->field_7A8 != frame) {
                endId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200012;
                endPan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                SndEvt_EnqueueType6(endId, endPan,
                                    (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                Gp_SpawnPadLerp(4, 0xFF, 8);
            }
            work->field_7A8 = work->slots0[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        }

        if (Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3ED, 0, 0) == 0) {
            D_actor_444000_80161908.value.pos.vx = arg0->extra.tmd->coords->coord.t[0];
            D_actor_444000_80161908.value.pos.vy = arg0->extra.tmd->coords->coord.t[1];
            D_actor_444000_80161908.value.pos.vz = arg0->extra.tmd->coords->coord.t[2];
            D_actor_444000_80161908.value.rot.vx = 0;
            D_actor_444000_80161908.value.rot.vy = 0;
            D_actor_444000_80161908.value.rot.vz = 0;
            TASK_MESSAGE_DISPATCH_POINTER(player, 0x3E9, &D_actor_444000_80161908.value, 0);
            D_actor_444000_80161868.value = 1;
        }

        if (work->field_6 == 0x17) {
            SndEvt_EnqueueType7((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000A, 1);
            Actor444000_PlacePlayerAhead(arg0, work, player, sc, cfg);
            work->anim.animationId = 1;
            work->anim.blend       = ANIMATION_BLEND_RESET;
            work->anim.blendFrames = 0;
            work->field_F02        = 1;
            TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->anim, 0);
        }
    }

    SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor444000WarpScratch));
}

/// Tick of the arena fight that runs the boss' two swipes and keeps the player
/// pinned in the scripted animation.
///
/// A reset request re-arms the block on animation 4, clears the host model's
/// flag word and pushes it onto each of the seven escorts' models, makes sure
/// the host and every escort has its model buffers allocated, then rebuilds the
/// free coordinate at `field_E3C` from `field_7C8` and plays the entry cue.
/// That coordinate is pushed through `Gp_UpdateCoord` again on every step.
///
/// The two swipes are one-shot: animation 4 reaching frame 0xC raises bit
/// 0x8000 of the collision object's flags, kicks the pad and fires two cues,
/// and animation 5 reaching frame 0x1C fires a third. `field_7AC` remembers the
/// frame each step so neither repeats while the frame is held, and the bit is
/// cleared on every step the first swipe is not live.
///
/// `field_6` then picks the blend weight in `field_7A4` (and hands over to
/// state 0xA at 0xDC), and while it sits in 0x29..0x2E the shared timer
/// `gGluttonLimbReach` climbs by 0x258 a step up to 0x1770 -- past 0x39 it
/// is wound back down again instead.
///
/// The rest is the player hold: once one of the collision object's five records
/// reports a hit of class 1, message 0x3F8 is asked whether the player can be
/// taken over and message 0x3F9 asks for the hold itself, with `field_ECA`
/// keeping that reply and `field_EC8` marking the hold as ours. While it is,
/// the 0x3FF animation is re-sent every step the reply and `field_EC8` agree
/// (or, if they do not, for the first 0x28 steps), and after 0x17 steps without
/// the hold the payload is swapped for the player's own weapon animation
/// (`field_4` 4). A reply of something other than 1 on that second stage
/// cancels the animation with message 0x3F1 and drops the hold.
static void func_actor_444000_8013FB74(Task* arg0)
{
    GluttonWork*           work;
    GluttonWork*           escorts;
    GluttonWork*           buffers;
    Enemy*                 enemy;
    Task*                  player;
    Task*                  target;
    TmdObject*             tmd;
    TmdObject*             escortTmd;
    GfxCoord*              coord;
    WorldCollisionContact* recs;
    s16                    i;
    s16                    j;
    s16                    k;
    s16                    mode;
    s32                    found;
    s32                    frame;
    s32                    frame2;
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
    u16                    count;

    work   = arg0->work;
    enemy  = arg0->spawnArg2.pointer;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    SCRATCH_STACK_RESERVE_BYTES(0x30);

    if (work->field_4 != 0) {
        work->field_F1D        = 0xB;
        work->field_7B3        = 4;
        work->field_7B0        = 2;
        escorts                = arg0->work;
        escorts->field_7F3     = 0;
        arg0->extra.tmd->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                escorts->field_ECC[i]->task->extra.tmd->flags = arg0->extra.tmd->flags;
            }
        }
        tmd     = arg0->extra.tmd;
        buffers = arg0->work;
        if (tmd->buffer == NULL) {
            Tmd_AllocBuffers(tmd);
        }
        for (j = 0; j < 7; j++) {
            if (buffers->field_ECC[j] != NULL) {
                escortTmd = buffers->field_ECC[j]->task->extra.tmd;
                if (escortTmd->buffer == NULL) {
                    Tmd_AllocBuffers(escortTmd);
                }
            }
        }
        work->field_EF6 = 1;
        work->field_EF4 = 0;
        work->field_EFA = 1;
        work->field_EF8 = 1;
        gfxRotMatrixY(&work->field_E3C.c.coord, work->field_7C8, 1);
        work->field_E3C.c.composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&work->field_E3C.c);
        work->field_E96 = 0xC80;

        resetId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200017;
        resetPan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(resetId, resetPan,
                            (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }

    coord                          = &work->field_E3C.c;
    work->field_E3C.c.composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);

    if (work->field_7B3 == 4 && (frame = work->slots0[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0xC &&
        work->field_7AC != frame) {
        gfxRotMatrixY(&work->field_E3C.c.coord, work->field_7C8, 1);
        work->field_E3C.c.composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(coord);
        work->field_EAC  = 3;
        work->obj.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        Gp_SpawnPadLerp(0x30, 0xFF, 8);

        swipeId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200019;
        swipePan = (s8)worldCoordGetOriginAudioPan(&work->field_ECC[0]->task->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(
            swipeId, swipePan,
            (s8)(worldCoordGetOriginAudioDepth(&work->field_ECC[0]->task->extra.tmd->coords[1]) / 2));

        swipe2Id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020001A;
        swipe2Pan = (s8)worldCoordGetOriginAudioPan(&work->field_ECC[0]->task->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(
            swipe2Id, swipe2Pan,
            (s8)(worldCoordGetOriginAudioDepth(&work->field_ECC[0]->task->extra.tmd->coords[1]) / 2));
    } else {
        work->obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }

    if (work->field_7B3 == 5 && (frame2 = work->slots0[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x1C &&
        work->field_7AC != frame2) {
        work->field_EAC = 3;
        Gp_SpawnPadLerp(0x20, 0x8F, 8);

        hitId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020001B;
        hitPan = (s8)worldCoordGetOriginAudioPan(&work->field_ECC[0]->task->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(
            hitId, hitPan,
            (s8)(worldCoordGetOriginAudioDepth(&work->field_ECC[0]->task->extra.tmd->coords[1]) / 2));
    }

    if (work->field_7B3 == 4) {
        work->field_7AC = work->slots0[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    } else {
        work->field_7AC = work->slots0[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    }

    switch (work->field_6) {
        case 0x14:
            gGluttonLimbReach = 0x640;
            work->field_7A4   = 0;
            break;
        case 0x22:
            work->field_7A4 = 1;
            break;
        case 0x2B:
            work->field_7A4 = 5;
            cueId           = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200018;
            cuePan          = (s8)worldCoordGetOriginAudioPan(&work->field_ECC[0]->task->extra.tmd->coords[1]);
            SndEvt_EnqueueType6(
                cueId, cuePan,
                (s8)(worldCoordGetOriginAudioDepth(&work->field_ECC[0]->task->extra.tmd->coords[1]) /
                     2));
            break;
        case 0x2D:
            work->field_7A4 = 2;
            break;
        case 0x38:
            work->field_7A4 = 4;
            break;
        case 0x44:
            work->field_7A4 = 3;
            break;
        case 0xDC:
            work->field_0 = 0xA;
            break;
    }

    if ((u32)((u16)work->field_6 - 0x29) < 6 && gGluttonLimbReach < 0x1770) {
        gGluttonLimbReach = (u16)gGluttonLimbReach + 0x258;
    }

    gluttonTickAnim(arg0);

    recs = work->recs2;
    for (k = 0; k < 5; k++) {
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
    if (found != 0 && TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3F8, &D_actor_444000_80161928.value, 0) == 0) {
        target          = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        work->field_ECA = Gp_DispatchMsg(target, 0x3F9, Gp_PackObjPair(enemy, 4), 0);
        if (work->field_ECA == 1) {
            ((GameActor*)player->work)->state = 0xA;
        }
        work->anim.source.sets = D_actor_444000_80161670;
        work->field_EC8        = 1;
        work->anim.animationId = 2;
        work->anim.blend       = ANIMATION_BLEND_RESET;
        work->anim.blendFrames = 0;
        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->anim, 0);
        work->field_7CA = 0;
    }

    mode = work->field_EC8;
    if (mode == 1 && work->field_0 != 0xD) {
        count           = work->field_7CA + 1;
        work->field_7CA = count;
        if (work->field_ECA == mode) {
            if (work->anim.animationId == 2) {
                work->anim.source.sets = D_actor_444000_80161670;
                work->anim.blend       = ANIMATION_BLEND_RESET;
                work->anim.blendFrames = 0;
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->anim, 0);
                work->field_7CA = 0;
            }
        } else if (work->anim.animationId == 2 && (s16)count < 0x28) {
            work->anim.source.sets = D_actor_444000_80161670;
            work->anim.blend       = ANIMATION_BLEND_RESET;
            work->anim.blendFrames = 0;
            TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->anim, 0);
        }

        if (Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3ED, 0, 0) == 0) {
            switch (work->anim.animationId) {
                case 2:
                    if (work->field_ECA != 1 && (s16)work->field_7CA >= 0x17) {
                        work->anim.source.sets     = D_actor_444000_80161670;
                        D_actor_444000_80161670[4] = (Gp_PlayerAnimBlkTbl
                                                          [Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon])
                                                         ->table.sets[7];
                        work->anim.animationId = 4;
                        work->anim.blend       = ANIMATION_BLEND_INTERPOLATE;
                        work->anim.blendFrames = 3;
                        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->anim, 0);
                        work->field_7CA = 0;
                    }
                    break;
                case 4:
                    if (work->field_ECA != 1) {
                        Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3F1, 2, 0);
                        work->field_EC8 = 0;
                    }
                    break;
            }
        }
    }

    if (work->field_6 == 0x3C && work->field_7B3 == 4) {
        work->field_7B3 = 5;
        work->field_7B0 = 1;
    }

    if (work->field_6 >= 0x39) {
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
/// `D_actor_444000_80161948.value` is rebuilt at that escort's second part -- its
/// rotation accumulated up the parent chain, its origin carried into view
/// space, then turned a quarter turn each way so `Gfx_MatrixCol2` yields the
/// launch direction, which is normalised and scaled to 0x320 before being
/// added to the origin -- and an effect is spawned on it. Every tenth step a
/// fresh enemy is spawned from `gGluttonEscortTasks` and remembered in
/// `field_EF0`.
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

    if (work->field_4 != 0) {
        work->field_F1D    = 7;
        work->field_7B3    = 0xB;
        work->field_7B0    = 2;
        escorts            = arg0->work;
        escorts->field_7F3 = 0;

        arg0->extra.tmd->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                escorts->field_ECC[i]->task->extra.tmd->flags = arg0->extra.tmd->flags;
            }
        }
        work->field_EF4 = 1;
        work->field_EF6 = 1;
        work->field_EFA = 0;

        resetId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200017;
        resetPan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(resetId, resetPan,
                            (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }

    if (work->field_6 == 0x3B) {
        cueId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200016;
        cuePan = (s8)worldCoordGetOriginAudioPan(&work->field_ECC[0]->task->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(cueId, cuePan,
                            (s8)worldCoordGetOriginAudioDepth(&work->field_ECC[0]->task->extra.tmd->coords[1]));
    }

    if (work->field_6 == 0x3C) {
        hitId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000D;
        hitPan = (s8)worldCoordGetOriginAudioPan(&work->field_ECC[0]->task->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(hitId, hitPan,
                            (s8)worldCoordGetOriginAudioDepth(&work->field_ECC[0]->task->extra.tmd->coords[1]));
    }

    if ((s16)(u16)work->field_6 >= 0x3D) {
        if ((s16)((s16)(u16)work->field_6 % 5) == 0) {
            if (D_actor_444000_80161850 >= 2) {
                D_actor_444000_80161850 = 0;
            } else {
                D_actor_444000_80161850 = (u16)D_actor_444000_80161850 + 1;
            }

            actorAccumulateToView(&work->field_ECC[0]->task->extra.tmd->coords[1],
                                  &D_actor_444000_80161948.value[D_actor_444000_80161850].coord);
            D_actor_444000_80161948.value[D_actor_444000_80161850].parent = &gGfxViewCoord;

            pos.vz = 0;
            pos.vy = 0;
            pos.vx = 0;
            actorLocalToView(&work->field_ECC[0]->task->extra.tmd->coords[1], &pos);

            D_actor_444000_80161948.value[D_actor_444000_80161850].coord.t[0] = pos.vx;
            D_actor_444000_80161948.value[D_actor_444000_80161850].coord.t[1] = pos.vy;
            D_actor_444000_80161948.value[D_actor_444000_80161850].coord.t[2] = pos.vz;
            gfxRotMatrixY(&D_actor_444000_80161948.value[D_actor_444000_80161850].coord, 0x80, 0);
            Gfx_RotMatrixX(&D_actor_444000_80161948.value[D_actor_444000_80161850].coord, -0x80, 0);
            Gfx_MatrixCol2(&D_actor_444000_80161948.value[D_actor_444000_80161850].coord, &pos);

            posp   = &pos;
            pos.vy = 0;
            VectorNormalSS(posp, posp);

            gte_lddp(0x320);
            gte_ldsv(posp);
            gte_gpf12();
            gte_stsv(posp);

            coord               = &D_actor_444000_80161948.value[D_actor_444000_80161850];
            coord->coord.t[0]  += pos.vx;
            coord->coord.t[1]  += pos.vy;
            coord->coord.t[2]  += pos.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            Gp_SpawnEff(0x60196, &D_actor_444000_80161948.value[D_actor_444000_80161850], 0x27A0D600, NULL);
        }
        if ((s16)((s16)(u16)work->field_6 % 10) == 4) {
            spawned           = Gp_SpawnEnemyFromTable(gGluttonEscortTasks, 2, 0, arg0->spawnArg2.pointer);
            spawned->workType = ENEMY_WORK_PLAIN;
            work->field_EF0   = spawned;
        }
    }

    gluttonTickAnim(arg0);

    if (work->slots0[1].flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->field_0 = 0xA;
        SndEvt_EnqueueType7((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000D, 1);
    }
}

/// Tick of the arena fight state that runs alongside `func_actor_444000_80140E28`:
/// on a reset request it clears the host model's flag word, pushes it onto each
/// of the seven escorts' models, makes sure the host and every escort has its
/// model buffers allocated and restores the normal blend weight.
///
/// The three state checks that follow are independent. In state 0xD the 0x18
/// script is spawned once, on the step the third animation slot first reaches
/// frame 0x15, which `field_7D8` remembers so the spawn does not repeat while
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
    OverlayMat*  mtx;
    GfxCoord*    coords;
    s16          i;
    s16          j;
    s32          frame;

    work = arg0->work;
    if (work->field_4 != 0) {
        escorts                = arg0->work;
        escorts->field_7F3     = 0;
        arg0->extra.tmd->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                escorts->field_ECC[i]->task->extra.tmd->flags = arg0->extra.tmd->flags;
            }
        }
        tmd     = arg0->extra.tmd;
        buffers = arg0->work;
        if (tmd->buffer == NULL) {
            Tmd_AllocBuffers(tmd);
        }
        for (j = 0; j < 7; j++) {
            if (buffers->field_ECC[j] != NULL) {
                escortTmd = buffers->field_ECC[j]->task->extra.tmd;
                if (escortTmd->buffer == NULL) {
                    Tmd_AllocBuffers(escortTmd);
                }
            }
        }
        work->field_7B6 = 0x10;
    }
    if (work->field_7B3 == 0xD) {
        frame = work->slots0[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (frame == 0x15 && work->field_7D8 != frame) {
            work->field_EAC = 3;
            Gp_SpawnScript18(D_actor_444000_80144A84, D_actor_444000_80144A8C);
        }
        work->field_7D8 = work->slots0[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    }
    if (work->field_7B3 == 9 && work->field_6 == 0x2D) {
        coords                                        = arg0->extra.tmd->coords;
        D_actor_444000_801618B8.ident.rotation.m00M01 = ONE;
        mtx                                           = (OverlayMat*)&D_actor_444000_801618B8.c.coord;
        mtx->ident.m02M10                             = 0;
        mtx->ident.m11M12                             = ONE;
        mtx->ident.m20M21                             = 0;
        mtx->ident.m22                                = ONE;
        D_actor_444000_801618B8.c.coord.t[1]          = -0x64;
        D_actor_444000_801618B8.c.coord.t[0]          = 0;
        D_actor_444000_801618B8.c.coord.t[2]          = 0x64;
        D_actor_444000_801618B8.c.composeStamp        = GRAPHICS_COORD_DIRTY;
        D_actor_444000_801618B8.c.parent              = &coords[4];
        Gp_UpdateCoord(&D_actor_444000_801618B8.c);
    }
    if (work->field_7B3 == 0x14 && (work->slots0[1].flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        work->field_7B3 = 0xD;
        work->field_7B0 = 1;
    }
    if (gGluttonLimbReach >= 0x191) {
        gGluttonLimbReach = (u16)gGluttonLimbReach - 0xC8;
    }
    gluttonTickAnim(arg0);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Reset handler for the arena fight: on a reset request, clear the host
/// model's flag word, push it onto each of the seven escorts' models, make sure
/// the host and every escort has its model buffers allocated, then fast-forward
/// the animation by running the re-arm step an eighth of `field_F14` times
/// before restoring the normal blend weight and playing the entry cue.
///
/// Either way the tick then runs the ordinary re-arm, re-flags the root
/// coordinate for rebuild, and spawns the 0x18 script once -- on the step the
/// second animation slot first reaches frame 0x1C, which `field_7D8` remembers
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
    if (work->field_4 != 0) {
        obj                    = arg0->spawnArg2.pointer;
        escorts                = arg0->work;
        escorts->field_7F3     = 0;
        arg0->extra.tmd->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                escorts->field_ECC[i]->task->extra.tmd->flags = arg0->extra.tmd->flags;
            }
        }
        tmd     = arg0->extra.tmd;
        buffers = arg0->work;
        if (tmd->buffer == NULL) {
            Tmd_AllocBuffers(tmd);
        }
        for (j = 0; j < 7; j++) {
            if (buffers->field_ECC[j] != NULL) {
                escortTmd = buffers->field_ECC[j]->task->extra.tmd;
                if (escortTmd->buffer == NULL) {
                    Tmd_AllocBuffers(escortTmd);
                }
            }
        }
        work->field_7B6 = 0x7F;
        work->field_EF4 = 0;
        work->field_EF6 = 0;
        for (k = 0; k < work->field_F14 / 8; k++) {
            gluttonTickAnim(arg0);
        }
        work->field_7B6 = 0x10;
        SndEvt_EnqueueType7((((u16)obj->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000A, 1);
    }
    if (gGluttonLimbReach >= 0x191) {
        gGluttonLimbReach = (u16)gGluttonLimbReach - 0xC8;
    }
    gluttonTickAnim(arg0);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    frame                                 = work->slots0[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    if (frame == 0x1C && work->field_7D8 != frame) {
        work->field_EAC = 3;
        Gp_SpawnScript18(D_actor_444000_80144A74, D_actor_444000_80144A7C);
    }
    work->field_7D8 = work->slots0[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
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
    if (work->field_4 != 0) {
        tmd                         = arg0->extra.tmd;
        obj                         = arg0->spawnArg2.pointer;
        obj->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        tmd->flags                  = 0;
        state                       = work->field_7B3;
        work->field_EF4             = 0;
        work->field_EF6             = 0;
        work->field_EFA             = 1;
        if (state != 0xD) {
            work->field_7B0 = 1;
            work->field_7B3 = 0xD;
        } else {
            work->field_7B0 = 2;
            work->field_7B3 = state;
        }
        id  = (((u16)obj->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200004;
        pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        SndEvt_EnqueueType7((((u16)obj->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000D, 1);
        return;
    }
    SCRATCH_STACK_RESERVE_BYTES(0xC);
    if (gGluttonLimbReach >= 0x191) {
        work->field_7A4   = 0;
        gGluttonLimbReach = (u16)gGluttonLimbReach - 0xC8;
    }
    gluttonTickAnim(arg0);
    if (work->slots0[1].flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->field_0 = 0xA;
    }
    SCRATCH_STACK_RELEASE_BYTES(0xC);
}

/// Idle/approach tick of the arena fight: re-arms the block on request, keeps
/// the boss yawed at `gPlayerStatus.coordMtx` (the player's coordinate matrix)
/// and then picks the state to run next.
///
/// `field_7C4` is that yaw, relative to the host part's own facing and wrapped
/// into +/-0x800. `field_F10` is a stagger countdown -- while it is positive the
/// tick only spins it down, and the reset arms it to 0x28 if it is not already
/// running.
///
/// The choice is a ladder: `field_F1C` picks state 3 outright, then each attack
/// pattern in `field_F08` has a depth the player has to be past before the
/// fight advances to state 9, the last two only while the boss still has HP in
/// hand. Failing all of those, `field_F1A` picks 0xF and pattern 6 picks 7, and
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
    GfxCoord*          facing;
    SVECTOR            vec;
    SVECTOR*           d;
    s16                angle;

    work   = arg0->work;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    enemy  = arg0->spawnArg2.pointer;

    if (work->field_4 != 0) {
        work->field_EF6 = 1;
        work->field_EF4 = 1;
        work->field_EFE = 0;
        if (work->field_F10 == 0) {
            work->field_F10 = 0x28;
        }
        work->field_7B3 = 1;
        work->field_7B0 = 1;
        work->field_EFA = 0;
    }

    d      = &vec;
    coord  = arg0->extra.tmd->coords;
    d->vx  = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    d->vy  = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    d->vz  = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    facing = arg0->extra.tmd->coords;
    angle  = ratan2(d->vx, d->vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
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
    work->field_7C4 = angle;

    if (gGluttonLimbReach >= 0x191) {
        gGluttonLimbReach = (u16)gGluttonLimbReach - 0xC8;
        work->field_7A4   = 0;
    }
    gluttonTickAnim(arg0);

    if (work->field_F10 > 0) {
        work->field_F10 = work->field_F10 - 1;
        return;
    }
    if (work->field_F1C > 0) {
        work->field_0 = 3;
        return;
    }

    if (work->field_F08 == 0) {
        work->field_0 = 9;
        return;
    }
    if (work->field_F08 == 1 && player->extra.tmd->coords->coord.t[2] < -0x1D4C) {
        work->field_0 = 9;
        return;
    }
    if (work->field_F08 == 2) {
        work->field_0 = 9;
        return;
    }
    if (work->field_F08 == 3 && player->extra.tmd->coords->coord.t[2] < -0x30D4) {
        work->field_0 = 9;
        return;
    }
    if (work->field_F08 == 4 && player->extra.tmd->coords->coord.t[2] < -0x3DB8 &&
        enemy->hp < 0x9C4) {
        work->field_0 = 9;
        return;
    }
    if (work->field_F08 == 5 && player->extra.tmd->coords->coord.t[2] < -0x4268 &&
        enemy->hp < 0x7D0) {
        work->field_0 = 9;
        return;
    }

    if ((s8)work->field_F1A > 0) {
        work->field_0 = 0xF;
        return;
    }
    if (work->field_F08 == 6) {
        work->field_0 = 7;
        return;
    }

    sc           = (GluttonHitScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(GluttonHitScratch));
    sc->delta.vx = player->extra.tmd->coords->coord.t[0] -
                   arg0->extra.tmd->coords->coord.t[0] - 0x51F;
    sc->delta.vy = player->extra.tmd->coords->coord.t[1] -
                   arg0->extra.tmd->coords->coord.t[1] - 0xFA;
    sc->delta.vz = player->extra.tmd->coords->coord.t[2] -
                   arg0->extra.tmd->coords->coord.t[2] + 0x25F;
    sc->dist = SquareRoot0(sc->delta.vx * sc->delta.vx + sc->delta.vy * sc->delta.vy +
                           sc->delta.vz * sc->delta.vz);
    if (sc->dist < 0x2329) {
        if (sc->dist >= 0xED9) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                work->field_0 = 7;
            } else {
                work->field_0 = 3;
            }
        } else {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                work->field_0 = 3;
            } else {
                work->field_0 = 0xB;
            }
        }
    } else {
        work->field_0 = 3;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(GluttonHitScratch));
}

/// Escort-spawn tick of the arena fight: re-arms the block on request and, on
/// that first pass, tops the two escort slots (`field_EE8`) back up to two live
/// enemies, seeding each one's model texture page from the current area record
/// and stamping its slot index into `Enemy::placeKey`. Every tick it then
/// yaws the host at the player, and at sub-state 0x46 / 0x78 it sends escort 0
/// or 1 a 0x7DB order whose action is picked from `field_F08` and a coin flip.
static void func_actor_444000_80141618(Task* task)
{
    GluttonSpawnScratch* sc;
    GluttonWork*         work;
    Enemy*               host;
    Enemy*               escort;
    PlayerStatus*        cfg;
    GfxCoord*            coord;
    GfxCoord*            facing;
    TmdObject*           model;
    AreaPlacement*       entry;
    GameLocationKey      key;
    GameLocationKey*     sessionKey;
    s32                  cueId;
    s32                  cuePan;
    s32                  blastId;
    s32                  blastPan;
    s32                  rnd;
    s32                  state;
    s16                  angle;
    u32                  frame;

    sc   = (GluttonSpawnScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(GluttonSpawnScratch));
    work = task->work;
    host = task->spawnArg2.pointer;
    if (work->field_4 != 0) {
        work->field_EF4 = 0;
        work->field_EF6 = 1;
        state           = work->field_7B3;
        work->field_7B6 = 0x10;
        work->field_EFA = 0;
        if (state != 0x13) {
            work->field_7B3 = 0x13;
            work->field_7B0 = 1;
        } else {
            work->field_7B0 = 2;
            work->field_7B3 = state;
        }
        for (sc->i = 0; sc->i < 2; sc->i++) {
            if (work->field_EE8[sc->i] == NULL && (u8)work->field_F1B < 8 && work->field_F08 < 6) {
                work->field_EE8[sc->i] = Gp_SpawnEnemyFromTable(&D_80172604, 3, 2, NULL);
                if (work->field_EE8[sc->i] != NULL) {
                    work->field_F1B++;
                    model      = work->field_EE8[sc->i]->task->extra.tmd;
                    sessionKey = &gGameSession->location.loc;
                    key.stage  = sessionKey->stage;
                    key.area   = sessionKey->area;
                    key.room   = sessionKey->room;
                    key.view   = sessionKey->view;
                    areaSyncLocationVariant(&key);
                    entry                    = &Gp_GetNestedAreaRec(&key)->field_0[2];
                    model->texturePageOffset = entry->texturePageOffset;
                    model->clutRowOffset     = entry->clutRowOffset;
                    if (model->buffer != NULL) {
                        tmdProcessStream(model);
                        tmdProcessStream(model);
                    }
                    work->field_EE8[sc->i]->workType = ENEMY_WORK_PLAIN;
                    escort                           = work->field_EE8[sc->i];
                    escort->placeKey                |= sc->i << ENEMY_PLACE_INDEX_SHIFT;
                    work->field_F1C++;
                }
            }
        }
        work->field_E96 = 0xC80;
        SndEvt_EnqueueType7((((u16)host->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000D, 1);
    }
    if (gGluttonLimbReach >= 0x191) {
        gGluttonLimbReach = (u16)gGluttonLimbReach - 0xC8;
    }
    gluttonTickAnim(task);
    if (work->field_7B3 == 0x13 && (frame = work->slots0[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 4 && frame < 0xD) {
        work->field_EFA = 1;
    } else {
        work->field_EFA = 0;
    }
    cfg          = &gPlayerStatus;
    coord        = task->extra.tmd->coords;
    sc->delta.vx = cfg->coordMtx->t[0] - coord->coord.t[0];
    sc->delta.vy = cfg->coordMtx->t[1] - coord->coord.t[1];
    sc->delta.vz = cfg->coordMtx->t[2] - coord->coord.t[2];
    facing       = task->extra.tmd->coords;
    angle        = ratan2(sc->delta.vx, sc->delta.vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
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
    work->field_7C4 = angle;
    if ((work->slots0[1].flags & ANIMATION_SLOT_REACHED_BOUNDARY) && work->field_7B3 == 0x13) {
        work->field_7B3 = 1;
        work->field_7B0 = 1;
        work->field_7B6 = 0x10;
        gluttonTickAnim(task);
    }
    if (work->field_6 >= 0x14B || (work->field_7B3 == 1 && work->field_F1C == 0)) {
        work->field_0 = 3;
    }
    if (work->field_6 == 6) {
        cueId  = (((u16)host->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200004;
        cuePan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        SndEvt_EnqueueType6(cueId, cuePan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    if (work->field_6 == 0x3B) {
        blastId  = (((u16)host->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200010;
        blastPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        SndEvt_EnqueueType6(blastId, blastPan, (s8)(worldCoordGetOriginAudioDepth(task->extra.tmd->coords) / 2));
        work->field_EAC = 3;
        Gp_SpawnScript18(D_actor_444000_80144A74, D_actor_444000_80144A7C);
    }
    if (work->field_6 != 0x46 && work->field_6 != 0x78) {
        goto out;
    }
    if (work->field_6 == 0x46) {
        sc->i = 0;
    } else {
        sc->i = 1;
    }
    if (work->field_EE8[sc->i] != NULL && work->field_F08 < 6) {
        D_actor_444000_80161888.value.context.loc.stage = 0;
        D_actor_444000_80161888.value.context.loc.area  = 0x2C;
        switch (work->field_F08) {
            case 0:
            case 1:
                if (sc->i == 0) {
                    gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    if (!((gRandomLcgState >> 16) & 1)) {
                        D_actor_444000_80161888.value.command = 3;
                    } else {
                        D_actor_444000_80161888.value.command = 4;
                    }
                } else {
                    gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    if (!((gRandomLcgState >> 16) & 1)) {
                        D_actor_444000_80161888.value.command = 5;
                    } else {
                        D_actor_444000_80161888.value.command = 6;
                    }
                }
                break;
            case 2:
            case 3:
                if (sc->i == 0) {
                    gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    if (!((gRandomLcgState >> 16) & 1)) {
                        D_actor_444000_80161888.value.command = 0xd;
                    } else {
                        D_actor_444000_80161888.value.command = 8;
                    }
                } else {
                    gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    if (!((gRandomLcgState >> 16) & 1)) {
                        D_actor_444000_80161888.value.command = 7;
                    } else {
                        D_actor_444000_80161888.value.command = 0xe;
                    }
                }
                break;
            case 4:
            case 5:
                if (sc->i == 0) {
                    gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    if (((gRandomLcgState >> 16) & 1)) {
                        D_actor_444000_80161888.value.command = 9;
                    } else {
                        D_actor_444000_80161888.value.command = 0xf;
                    }
                } else {
                    gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    if (!((gRandomLcgState >> 16) & 1)) {
                        D_actor_444000_80161888.value.command = 9;
                    } else {
                        D_actor_444000_80161888.value.command = 0xf;
                    }
                }
                break;
        }
        D_actor_444000_80161888.value.command <<= 8;
        rnd                                     = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        D_actor_444000_80161888.value.command  |= (s16)(((((u32)rnd >> 16) % 3) * 0x10) | 1);
        gRandomLcgState                         = rnd;
        TASK_MESSAGE_DISPATCH_POINTER(work->field_EE8[sc->i]->task, ACTOR_COMMAND_MESSAGE_APPLY, &D_actor_444000_80161888.value, 0);
    }
out:
    SCRATCH_STACK_RELEASE_BYTES(sizeof(GluttonSpawnScratch));
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
/// escorts, then runs the state handler `GluttonWork::field_0` selects and
/// republishes every collision group.
///
/// `gSceneCombatState.actorControl` gates how much of that runs. While the controller task is
/// suspended (1 or 2) the tick only pushes the host's `TmdObject::flags`
/// onto the escorts and clears the collision tables, and returns; only the
/// running case (0) and anything else falls through to the state machine.
/// Within the suspended cases the view index decides whether that flag word is
/// 0x80 (hidden) or 0.
///
/// `field_7F3` is a countdown armed when the fight hides the models: while it
/// runs the host is flagged hidden, and the step that takes it to zero also
/// raises bit 2 and hands every model's buffers back with `Tmd_FreeBuffers`.
///
/// `field_EFA` selects which of the two bodies is the "live" one -- the host
/// (`enemy`) or escort 3 (`field_ECC[3]`) -- and that choice drives the colour
/// update, the link-node slots and which collision groups publish their
/// `0x8000` bit this frame. States 0, 1, 5, 0xC, 0x12 and 0x13 are the inert
/// ones: they park both bodies on slot 1 and clear every group.
///
/// `field_F12` is the death timer, only started once the host's HP is gone:
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

    view                                    = Gp_GetViewIndex() & 0xFF;
    task->extra.tmd->coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&task->extra.tmd->coords[0]);

    escorts = task->work;
    if (escorts->field_7F3 != 0) {
        task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        if (--escorts->field_7F3 == 0) {
            task->extra.tmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            Tmd_FreeBuffers(task->extra.tmd);
            for (j = 0; j < 7; j++) {
                if (escorts->field_ECC[j] != NULL) {
                    escorts->field_ECC[j]->task->extra.tmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
                    Tmd_FreeBuffers(escorts->field_ECC[j]->task->extra.tmd);
                }
            }
        }
    }

    func_actor_444000_80142254();

    pos.vx = task->extra.tmd->coords[3].workm.t[0];
    pos.vy = task->extra.tmd->coords[3].workm.t[1];
    pos.vz = task->extra.tmd->coords[3].workm.t[2];

    if (work->field_EFA != work->field_EFC) {
        Gp_UpdateActorColor(enemy, &pos, 0, 0);
        Gp_UpdateActorColor(work->field_ECC[3], &pos, 0, 0);
        work->field_EFC = work->field_EFA;
    }
    Gp_UpdateActorColor(work->field_EFA != 0 ? enemy : work->field_ECC[3], &pos, 0, 0);

    if (work->field_0 == 0xB) {
        work->field_ECC[4]->task->extra.tmd->otOffset = -1;
    } else {
        work->field_ECC[4]->task->extra.tmd->otOffset = 0;
    }

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->field_0 != 0) {
                if (view == 9) {
                    flagged                = task->work;
                    flagged->field_7F3     = 0;
                    task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    for (i = 0; i < 7; i++) {
                        if (flagged->field_ECC[i] != NULL) {
                            flagged->field_ECC[i]->task->extra.tmd->flags =
                                task->extra.tmd->flags;
                        }
                    }
                } else {
                    flagged                = task->work;
                    flagged->field_7F3     = 0;
                    task->extra.tmd->flags = 0;
                    for (i = 0; i < 7; i++) {
                        if (flagged->field_ECC[i] != NULL) {
                            flagged->field_ECC[i]->task->extra.tmd->flags =
                                task->extra.tmd->flags;
                        }
                    }
                }
            }
            break;

        case SCENE_COMBAT_ACTORS_PAUSED:
            if (work->field_0 != 0) {
                if (view == 9) {
                    flagged                = task->work;
                    flagged->field_7F3     = 0;
                    task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    for (i = 0; i < 7; i++) {
                        if (flagged->field_ECC[i] != NULL) {
                            flagged->field_ECC[i]->task->extra.tmd->flags =
                                task->extra.tmd->flags;
                        }
                    }
                } else {
                    flagged                = task->work;
                    flagged->field_7F3     = 0;
                    task->extra.tmd->flags = 0;
                    for (i = 0; i < 7; i++) {
                        if (flagged->field_ECC[i] != NULL) {
                            flagged->field_ECC[i]->task->extra.tmd->flags =
                                task->extra.tmd->flags;
                        }
                    }
                }
            }
            Gp_ClearRec18Occupied(work->hits[0].recs);
            Gp_ClearRec18Occupied(work->hits[1].recs);
            Gp_ClearRec18Occupied(work->hits[2].recs);
            Gp_ClearRec18Occupied(work->hits[3].recs);
            Gp_ClearRec18Occupied(work->hits[4].recs);
            Gp_ClearRec18Occupied(work->hits[5].recs);
            Gp_ClearRec18Occupied(work->hits[6].recs);
            Gp_ClearRec18Occupied(work->hits[7].recs);
            Gp_ClearRec18Occupied(work->hits[8].recs);
            Gp_ClearRec18Occupied(work->recs2);
            return;

        case SCENE_COMBAT_ACTORS_HIDDEN:
            flagged                = task->work;
            flagged->field_7F3     = 0;
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            for (i = 0; i < 7; i++) {
                if (flagged->field_ECC[i] != NULL) {
                    flagged->field_ECC[i]->task->extra.tmd->flags =
                        task->extra.tmd->flags;
                }
            }
            Gp_ClearRec18Occupied(work->hits[0].recs);
            Gp_ClearRec18Occupied(work->hits[1].recs);
            Gp_ClearRec18Occupied(work->hits[2].recs);
            Gp_ClearRec18Occupied(work->hits[3].recs);
            Gp_ClearRec18Occupied(work->hits[4].recs);
            Gp_ClearRec18Occupied(work->hits[5].recs);
            Gp_ClearRec18Occupied(work->hits[6].recs);
            Gp_ClearRec18Occupied(work->hits[7].recs);
            Gp_ClearRec18Occupied(work->hits[8].recs);
            Gp_ClearRec18Occupied(work->recs2);
            return;
    }

    SCRATCH_STACK_RESERVE_BYTES(0x1C);

    if (enemy->hp > 0) {
        if (work->field_EC8 != 1 && cfg->hp > 0 && work->field_0 != 0xD) {
            if (work->field_E92 > 0) {
                work->field_E92--;
            } else {
                gluttonHitGroups1To2(task);
            }
            if (work->field_E8C > 0) {
                work->field_E8C--;
            } else {
                gluttonHitGroup0(task);
            }
            if (work->field_E8E > 0) {
                work->field_E8E--;
            } else {
                func_actor_444000_8013CA60(task);
            }
            if (work->field_E90 > 0) {
                work->field_E90--;
            } else {
                gluttonHitGroups6To8(task);
            }
        }
    }
    if (enemy->hp <= 0) {
        if (cfg->hp <= 0) {
            enemy->hp     = 1;
            gGluttonEnded = 0;
        }
        if (enemy->hp <= 0 && work->field_0 != 0) {
            switch (work->field_F12) {
                case 0:
                    gGluttonEnded                                   = 1;
                    D_actor_444000_80161888.value.context.loc.stage = 0;
                    D_actor_444000_80161888.value.context.loc.area  = 0x2C;
                    D_actor_444000_80161888.value.command           = 3;
                    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_actor_444000_80161888.value, ACTOR_COMMAND_MESSAGE_APPLY);
                    break;

                case 3:
                    if (cfg->hp > 0) {
                        if (work->field_F08 == 6) {
                            Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), 0x13F4, 2, 0);
                        } else {
                            Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), 0x13F4, 1, 0);
                        }
                        work->field_0 = 0x12;
                        SndEvt_EnqueueType7(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000A, 1);
                        SndEvt_EnqueueType7(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000D, 1);
                    }
                    break;
            }
            if (work->field_F12 < 0x100) {
                work->field_F12++;
            }
        }
    }

    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
        work->field_6 = 0;
    } else {
        if (work->field_6 < 0x7FFF) {
            work->field_6++;
        }
        work->field_4 = 0;
    }
    work->field_2 = work->field_0;
    handlers[work->field_0](task);

    if ((u16)work->field_0 < 2 || work->field_0 == 5 || work->field_0 == 0x12 ||
        work->field_0 == 0x13 || work->field_0 == 0xC) {
        enemy->node.state.parts.flags              = WORLD_TARGET_NOT_LOCKABLE;
        work->field_ECC[3]->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->field_ECC[0]->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->field_ECC[1]->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    } else if (work->field_EFA != 0) {
        if (Gp_NodeSlotMask(&work->field_ECC[3]->node) != 0) {
            Gp_AssignNodeSlot0(&enemy->node);
        }
        enemy->node.state.parts.flags              = WORLD_TARGET_HIDE_HP;
        work->field_ECC[3]->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
        work->field_ECC[0]->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
        work->field_ECC[1]->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
    } else {
        if (Gp_NodeSlotMask(&enemy->node) != 0) {
            Gp_AssignNodeSlot0(&work->field_ECC[3]->node);
        }
        enemy->node.state.parts.flags              = WORLD_TARGET_NOT_LOCKABLE;
        work->field_ECC[3]->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->field_ECC[0]->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->field_ECC[1]->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
    }

    if (work->field_0 != 0 && work->field_0 != 0x12 && work->field_0 != 0x13 &&
        work->field_0 != 5 && work->field_0 != 0xC && work->field_EFA == 1) {
        work->hits[0].obj.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->hits[0].obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }

    if (work->field_0 != 0 && work->field_0 != 0x12 && work->field_0 != 0x13 &&
        work->field_0 != 5 && work->field_0 != 0xC && work->field_EFA != 1) {
        work->hits[1].obj.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->hits[2].obj.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->hits[1].obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->hits[2].obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }

    if (work->field_0 != 0 && work->field_0 != 5 && work->field_0 != 0xC &&
        work->field_0 != 0x13 && work->field_0 != 0x12) {
        work->hits[3].obj.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->hits[4].obj.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->hits[5].obj.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->hits[3].obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->hits[4].obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->hits[5].obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }

    if (work->field_0 != 0 && work->field_0 != 5 && work->field_0 != 0xC &&
        work->field_0 != 0x13 && work->field_0 != 0x12) {
        work->hits[6].obj.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->hits[7].obj.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->hits[8].obj.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->hits[6].obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->hits[7].obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->hits[8].obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }

    Gp_ClearRec18Occupied(work->hits[0].recs);
    Gp_ClearRec18Occupied(work->hits[1].recs);
    Gp_ClearRec18Occupied(work->hits[2].recs);
    Gp_ClearRec18Occupied(work->hits[3].recs);
    Gp_ClearRec18Occupied(work->hits[4].recs);
    Gp_ClearRec18Occupied(work->hits[5].recs);
    Gp_ClearRec18Occupied(work->hits[6].recs);
    Gp_ClearRec18Occupied(work->hits[7].recs);
    Gp_ClearRec18Occupied(work->hits[8].recs);
    Gp_ClearRec18Occupied(work->recs2);

    if (work->field_0 != 5) {
        gluttonShakeTick(task);
    }

    SCRATCH_STACK_RELEASE_BYTES(0x1C);
}

/// Per-frame tail of the arena fight: keeps the camera pulled back far enough
/// to hold both the boss and the player, then runs the state the task is in.
///
/// `field_E94` is the camera distance actually in use and `field_E96` the one
/// the current state asks for -- 0xBB8 while the boss is grappling (state 3),
/// 0xD48 for the close patterns and 0x1388 otherwise -- walked 0x32 per frame
/// until the two are within 0x33 of each other. `field_E98` is the companion
/// height the floor-marker helpers take.
///
/// Most states hand that pair to `gluttonBuildWall`, which rebuilds
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
/// tick, or `Gp_DestroyEnemy`.
void func_actor_444000_80142F28(Task* arg0)
{
    void (*handlers[3])(Enemy*, Task*) = {
        func_actor_444000_8013AFF8,
        func_actor_444000_801423C4,
        Gp_DestroyEnemy,
    };
    SVECTOR      result;
    GluttonWork* work;
    Enemy*       enemy;
    Task*        player;
    SVECTOR*     verts;
    s32          diff;
    s16          state;

    enemy  = arg0->spawnArg2.pointer;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    work   = arg0->work;
    if (work != NULL) {
        if (work->field_EE8[0] != NULL && work->field_EE8[0]->hp <= 0) {
            work->field_EE8[0] = NULL;
        }
        if (work->field_EE8[1] != NULL && work->field_EE8[1]->hp <= 0) {
            work->field_EE8[1] = NULL;
        }

        state = work->field_0;
        if (state == 3) {
            work->field_E96 = 0xBB8;
            work->field_E98 = 0x190;
        } else if (state == 9) {
            work->field_E96 = 0x1388;
            work->field_E98 = 0x190;
        } else if (state == 0x11) {
            work->field_E96 = 0x1388;
            work->field_E98 = 0x190;
        } else if (work->field_F08 != 0) {
            work->field_E96 = 0xD48;
            work->field_E98 = 0x190;
        } else {
            work->field_E96 = 0x1388;
            work->field_E98 = 0x190;
        }

        diff = work->field_E96 - work->field_E94;
        if (diff < 0) {
            diff = -diff;
        }
        if (diff >= 0x33) {
            if (work->field_E94 < work->field_E96) {
                work->field_E94 = (u16)work->field_E94 + 0x32;
            } else {
                work->field_E94 = (u16)work->field_E94 - 0x32;
            }
        } else {
            work->field_E94 = (u16)work->field_E96;
        }

        state = work->field_0;
        if (state != 0) {
            /* Split so that `0x12` and `0x13` are not the innermost `&&` pair:
               `fold_range_test` would turn two adjacent constants into one
               `sltiu` range check. */
            if (state != 0x12) {
                if (state != 0x13 && state != 5 && state != 0xC) {
                    if (work->field_F08 == 1 && state == 9) {
                        func_actor_444000_801371E8(arg0, work->field_E94, 6);
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
                        gluttonBuildWall(arg0, work->field_E94, work->field_E98, 6);
                    }

                    if (work->field_F08 == 0 || (work->field_F08 == 1 && work->field_0 != 9)) {
                        GfxCoord* playerCoord = player->extra.tmd->coords;
                        GfxCoord* selfCoord   = arg0->extra.tmd->coords;
                        s32       x           = work->field_E94 + selfCoord->coord.t[0];

                        if (playerCoord->coord.t[0] < x) {
                            playerCoord->coord.t[0] = x;
                        }
                    } else {
                        GfxCoord* playerCoord = player->extra.tmd->coords;
                        GfxCoord* selfCoord   = arg0->extra.tmd->coords;
                        s32       z           = selfCoord->coord.t[2] - work->field_E94;

                        if (z < playerCoord->coord.t[2]) {
                            playerCoord->coord.t[2] = z;
                        }
                    }

                    if (work->field_F08 == 2) {
                        GfxCoord* playerCoord = player->extra.tmd->coords;

                        if (playerCoord->coord.t[0] < 0x251C) {
                            playerCoord->coord.t[2] = 0x251C;
                        }
                    }
                }
            }
            /* Re-read: the calls above can drop the fight back to state 0. The
               `goto` is what lets the `state == 0` edge reach the reset
               directly, as the ROM does. */
            if (work->field_0 != 0) {
                goto skipGrid;
            }
        }

        verts        = Gp_GridParams->vertices;
        verts[24].vy = 0x1F4;
        verts[25].vy = 0x1F4;
        verts[26].vy = 0x320;
        verts[27].vy = 0x320;
        verts[28].vy = 0x1F4;
        verts[29].vy = 0x1F4;
        verts[30].vy = 0x320;
        verts[31].vy = 0x320;

    skipGrid:
        state = work->field_0;
        if (state == 5) {
            gluttonBuildWall(arg0, work->field_E94, work->field_E98, 6);
        }
    }

    handlers[arg0->state](enemy, arg0);
}

#include "../../shared/glutton_quad_heights.inc.c"

#include "../../shared/glutton_exit.inc.c"

#include "../../shared/glutton_shake_level.inc.c"

#include "../../shared/glutton_set_spinners_released.inc.c"

#include "../../shared/glutton_get_spinners_released.inc.c"

/// The re-arm's counterpart: on a reset request it sets the two 0xEF4 counters
/// and the 0x7B0 pair rather than clearing them, and pushes `field_E = 2` onto
/// the first two escorts' model objects. Every tick it also parks one of two
/// yaw presets in `field_7C4`, alternating every 60 counts.
static void func_actor_444000_801434C4(Task* arg0)
{
    GluttonWork* work;
    s16          tick;

    work = arg0->work;
    if (work->field_4 != 0) {
        work->field_EF4                               = 1;
        work->field_EF6                               = 1;
        work->field_EFA                               = 0;
        work->field_7B3                               = 1;
        work->field_7B0                               = 1;
        work->field_EFE                               = 0;
        work->field_F1A                               = 0;
        work->field_ECC[0]->task->extra.tmd->otOffset = 2;
        work->field_ECC[1]->task->extra.tmd->otOffset = 2;
        func_shelter_b3_garbage_incinerator_80185220();
    }
    gluttonTickAnim(arg0);
    tick = work->field_6;
    if (tick % 60 == 0) {
        if (tick % 120 == 0) {
            work->field_7C4 = 0x2B2;
        } else {
            work->field_7C4 = -0x1A2;
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
    if (work->field_4 != 0) {
        tmd                         = arg0->extra.tmd;
        obj->node.state.parts.flags = 0;
        tmd->flags                  = 0;
        work->field_7B3             = 0xC;
        work->field_7B0             = 2;
        work->field_EF4             = 0;
        work->field_EF6             = 0;
        work->field_EFA             = 0;
        work->field_7B6             = 0x10;
        work->field_EFE             = 0;
        work->field_6               = 0;
    }
    if (work->field_6 == 0xA) {
        id  = ((obj->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200017;
        pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    gluttonTickAnim(arg0);
    if (work->slots0[1].flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->field_0 = 9;
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

s32 func_actor_444000_80143D68(Task* arg0)
{
    return ((Enemy*)arg0->spawnArg2.pointer)->hp > 0;
}

/// Seeds the enemy's `TmdObject` coordinate frame from `placement`: the three
/// longs become the translation, the Euler angles are applied X/Y/Z unless the
/// work block's state index is 0x12 or 0x13, and the coordinate is marked
/// dirty. Same body as `ActorsShared80135990` with that state gate added.
s32 func_actor_444000_80143D7C(Task* arg0, s32 arg1, ActorTransform* placement)
{
    GluttonWork* work = arg0->work;

    arg0->extra.tmd->coords->coord.t[0] = placement->pos.vx;
    arg0->extra.tmd->coords->coord.t[1] = placement->pos.vy;
    arg0->extra.tmd->coords->coord.t[2] = placement->pos.vz;
    if ((u32)((u16)work->field_0 - 0x12) >= 2U) {
        Gfx_RotMatrixX(&arg0->extra.tmd->coords->coord, placement->rot.vx, 1);
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, placement->rot.vy, 0);
        Gfx_RotMatrixZ(&arg0->extra.tmd->coords->coord, placement->rot.vz, 0);
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    return 1;
}

/// Per-frame upkeep for the enemy, dispatched by `arg2`: state 0 bumps the
/// heal counter, files a negative "damage" with `func_800DA6E8` so the HUD
/// shows it as a heal, and tops the enemy's HP back up by 0x64; state 1 ticks
/// the countdown at 0xF1C down, re-arms `field_F16` and drops either tracked
/// enemy whose HP has run out.
s32 func_actor_444000_80143E68(Task* arg0, s32 arg1, s32 arg2)
{
    GluttonWork* work = arg0->work;
    Enemy*       obj  = arg0->spawnArg2.pointer;

    switch (arg2) {
        case 0:
            work->field_F1A++;
            func_800DA6E8(&obj->node, -0x64, 0);
            if (obj->hp > 0) {
                obj->hp += 0x64;
            }
            break;
        case 1:
            if (work->field_F1C > 0) {
                work->field_F1C--;
            }
            work->field_F16 = 2;
            if (work->field_EE8[0] != NULL && work->field_EE8[0]->hp <= 0) {
                work->field_EE8[0] = NULL;
            }
            if (work->field_EE8[1] != NULL && work->field_EE8[1]->hp <= 0) {
                work->field_EE8[1] = NULL;
            }
            break;
    }
    return 1;
}

s32 func_actor_444000_80143F38(Task* arg0)
{
    ((GluttonWork*)arg0->work)->field_0 = 0;
    return 1;
}

/// Reset handler: when the work block is asking for a reset, stop the enemy's
/// own model drawing and push that same flag word onto each of the seven
/// escorts' models, then clear the two counters at 0xEF4. Otherwise just run
/// the ordinary re-arm in `gluttonTickAnim`.
static void func_actor_444000_80143F4C(Task* arg0)
{
    GluttonWork* work;
    GluttonWork* escorts;
    s16          i;

    work = arg0->work;
    if (work->field_4 != 0) {
        arg0->extra.tmd->flags = 0;
        escorts                = arg0->work;
        escorts->field_7F3     = 0;
        arg0->extra.tmd->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                escorts->field_ECC[i]->task->extra.tmd->flags = arg0->extra.tmd->flags;
            }
        }
        work->field_EF4 = 0;
        work->field_EF6 = 0;
    } else {
        gluttonTickAnim(arg0);
    }
}
