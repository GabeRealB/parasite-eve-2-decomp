#include "gameplay/player_actor.h"

#include <psyq/sys/types.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

#include "gameplay/display.h"
#include "actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/geometry.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/gpu_image_upload.h"
#include "item_use.h"
#include "gameplay/items.h"
#include "items.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "player_actor.h"
#include "gameplay/player_state.h"
#include "player_state.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "actors/companion.h"

#include "main/task_types.h"

/// Number of `PlayerStatus::weapon` indices. 0 is no weapon; 1..32 are the weapons.
#define PLAYER_ACTOR_WEAPON_COUNT 33

/// Attack routine for every `PlayerStatus::weapon` index, stored so the whole table can be copied.
///
/// The attack dispatcher copies the table and calls the equipped weapon's slot
/// with the player task. Slot 0 is no weapon. Slots 1..32 are the weapon
/// packages, whose package id is the index plus 7. One weapon overlay is
/// loaded at a time, always at the same address, so a slot is that overlay's
/// attack routine and is callable only while its weapon is equipped. A package
/// with no code, and no weapon, uses the empty handler in this file. An
/// attack's first phase puts the actor in normal mode, state 4, and later
/// phases advance `GameActor::statePhase`; normal state 4 calls the same slot
/// each frame. Copying the table copies the routine pointers, not the overlay.
/// There is no terminator or bounds check, so the index has to stay in 0..32.
typedef struct {
    TaskFunc attacks[PLAYER_ACTOR_WEAPON_COUNT]; // Attack routine for that weapon index
} _PlayerActorWeaponAttacks;
STATIC_ASSERT_SIZEOF(_PlayerActorWeaponAttacks, sizeof(TaskFunc) * PLAYER_ACTOR_WEAPON_COUNT);

#include "main/display.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include <psyq/abs.h>
#include <psyq/rand.h>

/// Number of vertices in an `_EffectDeathFlameScratch`: six round the base
/// ring and six round the top ring, interleaved and a twelfth of a turn apart.
#define EFFECT_DEATH_FLAME_VERTEX_COUNT 12

/// Scratch-stack workspace for drawing one death flame, a hexagonal cone
/// with its tip cut off.
///
/// `vertices` goes once round the flame, a twelfth of a turn per entry. Even
/// entries are the base ring, in the flame coordinate's local XZ plane at the
/// flame's radius. Odd entries are the top ring, a fixed amount narrower and
/// displaced along local Y by the flame's height. Each vertex is staged in
/// local space, rotated by the coordinate's world matrix and moved by its
/// translation, then stored back as a world position narrowed to 16 bits.
///
/// Six side quads each join two neighbouring base vertices to the top
/// vertices that follow them, wrapping at the last one, and two more quads
/// close the top ring. Every quad is projected through the same four
/// `screenCorners`, one RTPS for corner 0 and one RTPT for corners 1..3, in
/// GPU quad strip order.
///
/// Reserve one complete block on the scratch stack and release it in reverse
/// order after drawing. The block is not cleared, and pointers into it must
/// not survive its release.
typedef struct {
    SVECTOR vertices[EFFECT_DEATH_FLAME_VERTEX_COUNT]; // World positions round the flame: even entries the base ring, odd the top ring
    DVECTOR screenCorners[4];                          // Current quad's signed screen X/Y pixels, written together as one GTE word per corner
    s32     depth;                                     // Last projected corner's SZ3 / 4, plus one: ordering-table and blend depth
    s32     projectionFlags;                           // GTE FLAG word after the quad's RTPT; bit 31 makes it negative and drops the quad
} _EffectDeathFlameScratch;
STATIC_ASSERT_SIZEOF(_EffectDeathFlameScratch, 0x78);

/// Scratch for walking a two-row, three-column grid of text cells.
///
/// The block holds the cell indices together with the colour and pen
/// position the text of the current cell would take. The routine that
/// reserves it only steps these values and draws nothing, so the colour and
/// the pen are written and never read: that they are text style is read from
/// the values stored, the two colours being the ones text requests use
/// elsewhere and the row tops and column pitch fitting screen-centred pixel
/// coordinates. The block is reserved on the scratch stack for the walk and
/// released before the routine returns. It is not cleared.
typedef struct {
    s32 row;      // Row being walked (0 first, 1 second)
    s32 column;   // Column being walked within the row, 0..2
    u32 colorRgb; // Text modulation RGB of the row in bits 0..23, red in the low byte, as in `TextDrawReq`; one colour for the first row, another after it
    s16 penX;     // Horizontal pen: restarts one column pitch before the first column on each row and advances a pitch per cell
    s16 penY;     // Vertical pen: set to the row's start before each row, then lowered by a fixed step per cell; the purpose of that step is unproven
} _PlayerActorTextGridScratch;
STATIC_ASSERT_SIZEOF(_PlayerActorTextGridScratch, 0x10);

/// Scratch for choosing the shortest signed turn between two angles.
///
/// The three words are the raw target-minus-current difference and that
/// difference shifted by one full turn in each direction. The caller keeps
/// the candidate with the smallest absolute value. The block is reserved on
/// the scratch stack for the comparison and released before the caller
/// returns. Angles use 4096 units per turn.
typedef struct {
    s32 direct;    // Target minus current
    s32 plusTurn;  // `direct` plus one full turn
    s32 minusTurn; // `direct` minus one full turn
} _PlayerActorShortestTurnScratch;
STATIC_ASSERT_SIZEOF(_PlayerActorShortestTurnScratch, 0xC);

/// Scratch-stack block for turning an actor's movement mode into its velocity for the frame.
///
/// A moving actor travels along its model's forward axis at one unit vector
/// (4096) divided by the mode's speed divisor. The circling mode also strafes
/// around the lock target: the model matrix is set aside, yawed to the strafe
/// heading to read that direction, and put back. One block is reserved per
/// call and released before returning. Angles use 4096 units per turn.
typedef struct {
    s32     speedDivisor;   // Movement mode's divisor: a unit direction divided by it is the distance covered per frame
    s32     strafeYaw;      // Circling only: the constant divided by the target distance, then the yaw from the forward axis to the strafe heading
    MATRIX  savedMatrix;    // Circling only: the model's local matrix, restored once the strafe heading has been read from it
    SVECTOR direction;      // Model's forward axis, then the strafe heading; while circling, `vx` first holds the |dx| + |dz| distance to the target
    VECTOR3 targetPosition; // Circling only: the lock target's position
    byte    field_3C[4];    // Never accessed; role unproven
} _PlayerActorMoveStepScratch;
STATIC_ASSERT_SIZEOF(_PlayerActorMoveStepScratch, 0x40);

/// Scratch-stack block for turning a player actor's body yaw toward its lock target.
///
/// Holds a temporary coordinate node standing at the equipped weapon's aiming
/// origin, the lock target's displacement from that point, whose X and Z give
/// the heading to turn toward, and the yaw worked out from it. One block is
/// reserved per call and released before returning. Angles use 4096 units per
/// turn.
typedef struct {
    GfxCoord originCoord;  // Aim origin: the weapon model's root transform moved to `originOffset`, re-expressed beneath the view node
    VECTOR3  targetDelta;  // Lock target's position beneath the view node, then that position minus the origin node's translation
    byte     field_5C[4];  // Never accessed; role unproven
    SVECTOR  originOffset; // Point in the weapon model's root space where the origin is placed
    s32      yaw;          // Heading of `targetDelta`, then the shortest signed turn toward it, then that turn clamped to the weapon's turn rate
} _PlayerActorAimYawScratch;
STATIC_ASSERT_SIZEOF(_PlayerActorAimYawScratch, 0x6C);

/// Scratch-stack block for working out a player actor's velocity on a flight of stairs.
///
/// A stair walk moves the actor along its model's forward axis, tilted up or
/// down by the flight's slope while it is on the steps; the stride that ends
/// the flight takes its heading from the untilted model instead. A velocity
/// is that unit direction (4096) divided by a speed divisor, so a larger
/// divisor is a slower stride. One block is reserved per call and released
/// before returning.
typedef struct {
    MATRIX  pitchedMatrix; // On-stairs stride only: the model's local matrix pitched about its own X axis by the flight's slope
    SVECTOR direction;     // Forward axis of `pitchedMatrix`, or of the model's own matrix for the closing stride, normalized to 4096
    s32     speedDivisor;  // On-stairs stride only: `direction` divided by it is the distance covered per frame (110 climbing, 100 descending)
} _PlayerActorStairClimbScratch;
STATIC_ASSERT_SIZEOF(_PlayerActorStairClimbScratch, 0x2C);

/// Scratch-stack block for pitching a player actor's aim toward its lock target.
///
/// Holds a temporary coordinate node standing at the point the aim is measured
/// from, the lock target's position and its displacement from that point, and
/// the elevation that displacement gives. A routine reserves one block, runs
/// one measurement per joint it drives, reusing every member, and releases the
/// block before returning. Angles use 4096 units per turn.
typedef struct {
    GfxCoord originCoord;    // Aim origin: the source node's transform moved to `originOffset`, re-expressed beneath the view node
    VECTOR3  targetDelta;    // `targetPosition` minus the origin node's translation
    byte     field_5C[4];    // Never accessed; role unproven
    VECTOR3  targetPosition; // Lock target's position beneath the view node
    byte     field_6C[4];    // Never accessed; role unproven
    SVECTOR  originOffset;   // Point in the source node's local space where the origin is placed
    s32      pitch;          // Target's elevation from the origin (positive toward -Y), then the clamped change applied to the driven joint angle
    s32      groundDistance; // Length of `targetDelta` in the XZ plane
    byte     field_80[4];    // Never accessed; role unproven
} _PlayerActorAimPitchScratch;
STATIC_ASSERT_SIZEOF(_PlayerActorAimPitchScratch, 0x84);

/// Immutable variable-motor pulse the player actor posts for an attack.
///
/// The poster indexes the one-entry table with the low halfword of its
/// selector. Normal-mode state 4 is the only caller and passes 0. While
/// `GameActor::rumblePosted` is clear, port 0's variable motor is posted at
/// `intensity` for `durationUnits` and the latch is set. `Pad_PostEvent`
/// doubles each unit into serviced controller polls. The stored pulse is full
/// intensity for two units. `field_1` is never read; its role is unproven.
typedef struct {
    u8  intensity;     // Variable-motor drive (0..255)
    u8  field_1;       // Never read; role unproven
    s16 durationUnits; // Duration before `Pad_PostEvent` doubles it into controller polls
} _PlayerActorVibrationPreset;
STATIC_ASSERT_SIZEOF(_PlayerActorVibrationPreset, 0x4);

extern EffectSpawnArg D_80112C74;

extern s32 D_80112C7C[];

/// CLUT X positions, in pixels, for effect sprites drawn from texture page
/// 0x29, in five rows of two. Each entry places a CLUT on VRAM row 0x10A; the
/// drawing function fixes the row and its caller picks the column.
extern u16 D_80112964[5][2];

/// Spawn-id words indexed by the 3-digit packing of `Gp_StateC08.attachId`
/// `(hundreds-1)*9 + (tens-1)*3 + ones - 1`. `Gp_EffTask07State1` uses this
/// when `field_3 == 1`, and `D_80112A50` when `field_3 == -1`.
extern s32 D_80112978[];

extern s32 D_80112A50[];

/// Spawn-id words for `Gp_EffCtlTaskAE`, indexed with the same 3-digit packing
/// of `Gp_StateC08.attachId` as `D_80112978`; the value becomes the task's
/// `Task::spawnArg1` sound id.
extern s32 D_80112B94[];

/// `GfxCoord` index parallel to `D_80112978`. `Gp_EffTask07State1` adds
/// it onto `TmdObject.coords` when `field_3 == 1`.
extern u16 D_80112B28[];

/// 4 packed RGB-nibble colors. `Gp_EffCtlTaskC1` indexes with
/// `TaskSpawnArg::halves.high & 3` and stores the halfword in `EffectWork.period`.
extern u16 D_80112C6C[];

/// u8 Task_Spawn type bases. `func_80104258` indexes
/// `D_80112DFC[arg2 + gPlayerStatus.resourceVariant - 2]`.
extern u8 D_80112DFC[];

/// The one variable-motor vibration preset. The poster indexes it with the
/// low halfword of its selector; the only call passes 0.
extern _PlayerActorVibrationPreset D_80112E28[];

/// s16 scale rows indexed by `GameActor.movementMode`. `Gp_StepPlayerMove` divides
/// the normalized matrix-column by `D_80112E10[movementMode]`.
extern s16 D_80112E10[];

/// u16 facing-step rows indexed by `GameActor.turnRateIndex`. `Gp_TurnPlayer`
/// adds `D_80112E20[turnRateIndex] * turnSign` onto `rotation.vy` (masked `0xFFF`).
extern u16 D_80112E20[];

/// 2-wide rows of `GfxCoord` indices. `func_8010403C` indexes
/// `D_80112E2C[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1][arg0]`.
extern u8 D_80112E2C[][2];

/// u16 turn-rate rows indexed by `gPlayerStatus.weapon`. `Gp_AimYawToLock`
/// clamps the wrapped yaw delta to this value (or 1.5x when
/// `func_800B9D80(0x2000)` is set).
extern u16 D_80112E30[];

/// NULL-terminated `GpuImageUpload*` lists for `func_801030CC`. Indexed as
/// `table[type * 4 + gPlayerStatus.resourceVariant - 5][frame]`. `D_80112E74` is
/// the `textureSequenceA` sequence; `D_80112EB4` is the `textureSequenceB` sequence.
extern GpuImageUpload** D_80112E74[];

extern GpuImageUpload** D_80112EB4[];

/// Per-item flag byte indexed by `gPlayerStatus.weapon`. Nonzero makes
/// `Gp_PlayerNormalState2` / `Gp_PlayerMode2StateA` pass `GameActor.attackButton` (the current
/// aim direction) to `func_80106264` instead of the default 1.
extern u8 D_80112EF8[];

/// 2-wide rows indexed by `gPlayerStatus.weapon`. Zero at `[i][0]`
/// makes `func_801088D4` abort the item-use path (`statePhase = 0x3E8`).
extern u8 D_80112F1C[][2];

/// 0x10-byte `VECTOR` rows indexed by `Gp_AttachActorObj` arg1: where the
/// weapon of that attach id sits on the actor. Copied through scratch; the low
/// 16 bits of `vx`/`vy`/`vz` seed the shape's `ends[1]`.
extern VECTOR D_80112FA4[];

/// Aiming origin of each weapon, indexed by weapon id: a point in the local
/// space of the root coordinate of the equipped weapon's model. The aim
/// routines stage a row in their scratch block and measure the lock target's
/// offset from the resulting world position.
extern SVECTOR D_801131B4[];

extern TaskMessageEntry Gp_PlayerMsgTable[28];

extern u16 D_80112DF4[4];

/// Unused, probably stale program data: likely per-weapon boolean flags.
/// No reader identified; the original flag meaning is unknown.
extern u8 D_80112ED4[33];

extern u16 D_801132BC[33][2];

static const TaskFuncTable3 Gp_EffTask07States;

/// Four-entry `Task::state` dispatcher: `Gp_InitPlayerWork`, `Gp_PlayerWorkState1`,
/// `Gp_PlayerWorkState2`, `Gp_TeardownSlot0`.
static const TaskFuncTable4 Gp_PlayerWorkStates;

/// Per-weapon handlers, indexed by `PlayerStatus::weapon` and copied by
/// `func_8010615C`. Most live in the weapon overlay loaded at the time;
/// `func_801065A0` serves the weapons with none.
static const _PlayerActorWeaponAttacks D_800978BC;

/// `mode` dispatcher: `Gp_TickPlayerNormal`, `Gp_TickPlayerMode1`, `Gp_TickPlayerMode2`.
static const TaskFuncTable3 Gp_PlayerModeFns;

/// `state` dispatcher copied by `Gp_TickPlayerNormal`.
static const TaskFuncTable8 D_8009794C;

/// `hitRegion` dispatcher: three slots of `Gp_PlayerMode1State0`, then `Gp_PlayerMode1State3`.
static const TaskFuncTable4 Gp_PlayerMode1States;

/// `state` dispatcher copied by `Gp_TickPlayerMode2`.
static const TaskFuncTable12 Gp_PlayerMode2States;

/// Enters scripted player playback using a receiver-valid animation-bank index.
///
/// The request also selects blending and world collision; returns zero.
/// `msgId` and `unusedArg3` are unused, including for direct initialization.
s32 func_80104508(Task* task, s32 msgId, AnimationPlayRequest* request, s32 unusedArg3);

s32 func_801055D4(Task* arg0, s32 arg1, s32 arg2, s32 arg3);

static void Gp_DrawEffSprite81(Task* arg0);

static void Gp_DrawEffSprite46(GfxCoord* arg0, s32 arg1, s16 arg2, u16 arg3);

static void Gp_DrawEffSpark(Task* arg0, s32 arg1, u8* arg2);

static void Gp_DrawEffQuadT29(GfxCoord* arg0, s32 arg1, u16 arg2, u16 arg3);

static void Gp_EffTask07State1(Task* arg0);

static void Gp_DrawEffTri(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);

static void Gp_EffTask07State0(Task* arg0);

static void func_800FCD00(Task* arg0);

/// Hand-written GTE routine. `arg2` is a full 32-bit word: the high half picks
/// the CLUT (palette column) and the low 12 bits are the billboard size, so it
/// must not be declared `s16` (that makes callers emit a spurious `sll`/`sra`
/// truncation). It is unsigned because the size is divided by `otz` with `divu`.
static void Gp_DrawEffSpriteE2(GfxCoord* arg0, u16 arg1, u32 arg2, s16 arg3);

/// Puts `obj`, one of the player's bodies, on the object list: a sphere of
/// `radius` at `(x, y, z)` under `coord`, using the actor's `i`th motion context
/// with contacts stored in `recs`, and keyed by the saved
/// game's character.
static inline void _gpLinkPlayerObj(GameActor* actor, s32 i, WorldCollisionBody* obj, GfxCoord* coord, WorldCollisionContact* recs, s16 x, s16 y,
                                    s16 z, u16 radius, u16 flags);

static void Gp_InitPlayerWork(Task* arg0);

static void Gp_PlayerWorkState1(Task* arg0);

static void func_8010133C(void);

static void Gp_PlayerWorkState2(Task* arg0);

static void Gp_TeardownSlot0(Task* arg0);

/// Latches this frame's pad state into the actor of `arg0`: keeps the previous
/// values of the per-frame bytes and of the held buttons, reads the session's
/// pad, and derives the newly pressed and released buttons from the two.
static inline void _gpCaptureActorPad(Task* arg0);

/// Clears the frame stamp of node `i` of the task's model, so it is composed
/// again, and returns that node's local matrix for the caller to rebuild.
static inline MATRIX* _gpRebuildCoordMatrix(Task* task, s32 i);

/// The signed turn from `from` to `to` (4096 units per revolution), taking
/// whichever of the direct difference and its one-revolution neighbours is
/// shortest.
static inline s16 _gpShortestTurn(s16 from, s16 to);

/// Turns `actor` toward its lock target once the target is farther than
/// `thresh` in the ground plane, by at most the equipped weapon's turn rate.
static inline void _gpAimYawAt(GameActor* actor, _PlayerActorAimYawScratch* block, s16 thresh);

/// Places `block->originCoord` at the point `offset` in the local space of
/// `src`, with the orientation of `src`.
static inline void _gpAimPitchPlace(_PlayerActorAimPitchScratch* block, GfxCoord* src, SVECTOR* offset);

/// Stores the lock target's position relative to `block->originCoord` in
/// `block->targetDelta` and returns the length of that offset in the ground
/// plane.
static inline s32 _gpAimPitchLockDelta(GameActor* actor, _PlayerActorAimPitchScratch* block);

static void Gp_AimPitchToLockAlt(Task* arg0);

static void Gp_AimPitchDirect(Task* arg0);

static void func_801030CC(Task* arg0);

inline static Task* spawn_tmd_attach(Task* arg0, s32 arg1, s32 arg2, s32 arg3);

static Task* func_80103294(Task* arg0, s32 arg1, s32 arg2);

inline static Task* spawn_attach(Task* parent, s32 row, s32 item);

static void Gp_CaptureActorPad(Task* arg0);

static void Gp_BindActorAnim(Task* arg0);

static s32 Gp_HpBand(void);

static s32 Gp_ApplyDirArg(Task* arg0, GameActorMoveBy* move);

static void func_80103CB4(GfxCoord* arg0, s32 arg1, VECTOR3* arg2, VECTOR3* arg3);

static GfxCoord* func_8010403C(s32 arg0);

static void func_801041FC(Task* arg0, s32 arg1);

static void func_80104A4C(Task* arg0);

static void func_80104AAC(Task* arg0);

/// Replaces player animation playback without clearing the current scripted state.
s32 func_80104CAC(Task* task, s32 msgId, AnimationPlayRequest* request, s32 unusedSecondArg);

/// Puts the player in `mode` mode 2 (`Gp_TickPlayerMode2`): clears the
/// movement state and the HUD flag, re-applies the equipped weapon, and during
/// an event clears flag 0x2000 on the actor's first object.
static inline void _gpSwitchToPlayerMode2(Task* arg0);

s32 func_80104F5C(Task* arg0, s32 arg1, GameActorStairClimb* climb, s32 unusedSecondArg);

s32 func_80105190(Task* arg0, s32 arg1, ActorTransform* transform, GameActorMoveAnim* moveAnim);

s32 func_801054D8(Task* arg0, s32 arg1, GameActorButtonPressHold* arg2, s32 unusedSecondArg);

s32 func_80105690(Task* arg0, s32 arg1, s32 arg2, s32 arg3);

s32 func_80105754(Task* arg0, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg);

s32 Gp_CopyPlayerAnim(Task* arg0, s32 arg1, const AnimationBankCopyRequest* request, s32 unusedSecondArg);

s32 Gp_ApplyPlayerDamage(Task* arg0, s32 arg1, s32 arg2, s32 unusedSecondArg);

s32 func_80105A8C(Task* arg0, s32 arg1, s32 arg2, s32 unusedSecondArg);

static void func_80105B0C(Task* arg0);

static void func_8010615C(Task* arg0);

static s32 func_801062DC(Task* arg0, s32 arg1);

static void func_801065A0(Task* task);

static void func_801065A8(Task* arg0);

static void Gp_TickPlayerNormal(Task* arg0);

static void Gp_PlayerNormalState2(Task* arg0);

static void Gp_PlayerNormalState5(Task* arg0);

/// Switches the player to `Gp_TickPlayerMode2` state 2 and starts the entry
/// animation `movementSign` selects: a `fade` of 0 resets the child slots to it,
/// anything else is passed to `playerActorPlayChildSlotsWithBlend`.
static inline void _gpEnterPlayerMode2(Task* task, s32 fade);

static void Gp_PlayerNormalState6(Task* arg0);

static void func_8010771C(Task* arg0);

static void Gp_PlayerMode2State3(Task* arg0);

static void Gp_PlayerMode2StateA(Task* arg0);

static void Gp_PlayerMode2StateB(Task* arg0);

static void Gp_TickPlayerActor(Task* arg0);

static void Gp_ArmLockOnState(Task* arg0);

static void func_80108568(Task* arg0);

static void func_801085D0(Task* arg0);

static void func_80108620(Task* arg0);

static void func_80108684(Task* arg0);

static void Gp_ResetActorAnimState(Task* arg0, s32 arg1);

static void func_80108A0C(Task* arg0);

static void func_80108AD4(Task* arg0);

static void Gp_PlayerMode2State8(Task* arg0);

/// Stores `arg1` as the actor's `targetNode` node, moving the `targeted` mark
/// from the node it replaces to `arg1`.
static inline void _gpSetLockNode(Task* arg0, WorldTargetNode* arg1);

static void Gp_TickPlayerMode1(Task* arg0);

static void Gp_TickPlayerMode2(Task* arg0);

static void func_80108FA0(Task* arg0);

static void Gp_PlayerNormalState1(Task* arg0);

static void func_801090E8(Task* arg0);

static void func_80109138(Task* arg0);

static void Gp_PlayerMode1State0(Task* arg0);

static void Gp_PlayerMode1State3(Task* task);

static void func_80109210(Task* arg0);

static void func_80109250(Task* arg0);

static s32 func_80109290(Task* arg0);

static void func_80109374(Task* arg0);

static void Gp_UpdateLockTarget(Task* arg0);

static void Gp_PlayerMode2State5(Task* arg0);

static void func_801095BC(s32* arg0);

static void Gp_PlayerMode2State7(Task* arg0);

static void Gp_PlayerMode2State9(Task* arg0);

static void func_80109720(Task* arg0);

static void func_80109818(Task* arg0);

/// Caps a level at 2.
static inline s32 _gpCapLevel(s32 level);

static void func_80109844(Task* arg0);

static void func_80109A1C(Task* arg0);

/// Ends the actor's current action: clears the fields `func_8010B210` resets,
/// sets `recoveryTicks` to 0x12 and restarts the state machine in the base state
/// for its `state` mode.
static inline void _gpResumeBaseState(Task* arg0);

extern AnimationBank D_p08_8012A85C;

extern AnimationBank D_m93r_8012ADF8;

extern AnimationBank D_m950_8012BA78;

extern AnimationBank D_p229_8012B51C;

extern AnimationBank D_unused_85_8012A474;

extern AnimationBank D_unused_86_8012A0D8;

extern AnimationBank D_mongoose_8012A9C0;

extern AnimationBank D_grenade_pistol_8012B2E4;

extern AnimationBank D_mm1_8012D184;

extern AnimationBank D_pa3_8012C18C;

extern AnimationBank D_sp12_8012BCC8;

extern AnimationBank D_as12_8012C300;

extern AnimationBank D_m4a1_8012B9E4;

extern AnimationBank D_m249_8012CF58;

extern AnimationBank D_tonfa_baton_8012BAB0;

extern AnimationBank D_8012EDD0;

extern AnimationBank D_gunblade_8012E108;

extern AnimationBank D_m4a1_hammer_8012D4F4;

extern AnimationBank D_m4a1_bayonet_8012D25C;

extern AnimationBank D_m4a1_grenade_8012DF50;

extern AnimationBank D_m4a1_pyke_8012D500;

extern AnimationBank D_m4a1_javelin_8012EA20;

extern AnimationBank D_mp5a5_8012B3CC;

extern GpuImageUpload* D_aya_10400_8011CC94[];

extern GpuImageUpload* D_aya_10300_8011CD1C[];

extern GpuImageUpload* D_aya_10200_8011D094[];

extern GpuImageUpload* D_8011D168[];

extern GpuImageUpload* D_aya_10400_8011CC9C[];

extern GpuImageUpload* D_aya_10300_8011CD24[];

extern GpuImageUpload* D_aya_10200_8011D09C[];

extern GpuImageUpload* D_8011D170[];

extern GpuImageUpload* D_aya_10400_8011CCAC[];

extern GpuImageUpload* D_aya_10300_8011CD34[];

extern GpuImageUpload* D_aya_10200_8011D0AC[];

extern GpuImageUpload* D_8011D180[];

extern GpuImageUpload* D_aya_10400_8011CCBC[];

extern GpuImageUpload* D_aya_10300_8011CD44[];

extern GpuImageUpload* D_aya_10200_8011D0BC[];

extern GpuImageUpload* D_8011D190[];

extern GpuImageUpload* D_aya_10400_8011CCDC[];

extern GpuImageUpload* D_aya_10300_8011CD64[];

extern GpuImageUpload* D_aya_10200_8011D0DC[];

extern GpuImageUpload* D_8011D1B0[];

extern GpuImageUpload* D_aya_10400_8011CCD4[];

extern GpuImageUpload* D_aya_10300_8011CD5C[];

extern GpuImageUpload* D_aya_10200_8011D0D4[];

extern GpuImageUpload* D_8011D1A8[];

extern AnimationBank D_kyle_800101_801756D0;

extern AnimationBank D_kyle_800102_801772E8;

extern AnimationBank D_kyle_800103_8017567C;

extern AnimationBank D_kyle_800104_80176C1C;

extern AnimationBank D_actor_800200_8016F208;

extern AnimationBank D_actor_800300_8016CB98;

/// Weapon overlay entry points, at fixed addresses.
void func_m93r_8011D1C4(Task* arg0);
void func_m4a1_8011D1C4(Task* arg0);
void func_m4a1_p1_8011D1C4(Task* arg0);
void func_m4a1_p2_8011D1C4(Task* arg0);

void func_grenade_pistol_8011D1D4(Task* arg0);
void func_mm1_8011D1D4(Task* arg0);

void func_p08_8011D1D8(Task* arg0);
void func_p08_snail_8011D1D8(Task* arg0);
void func_mongoose_8011D1D8(Task* arg0);

void func_m950_8011D1DC(Task* arg0);
void func_pa3_8011D1DC(Task* arg0);
void func_sp12_8011D1DC(Task* arg0);
void func_as12_8011D1DC(Task* arg0);
void func_m249_8011D1DC(Task* arg0);

void func_m4a1_grenade_8011D1EC(Task* arg0);

void func_m4a1_bayonet_8011DA34(Task* arg0);

void func_tonfa_baton_8011DBFC(Task* arg0);

void func_p229_8011DDA0(Task* arg0);

void func_mp5a5_8011DDA4(Task* arg0);
void func_mp5a5_p1_8011DDA4(Task* arg0);
void func_mp5a5_p2_8011DDA4(Task* arg0);

void func_gunblade_8011E040(Task* arg0);

void func_m4a1_pyke_8011E4F8(Task* arg0);

void func_m4a1_hammer_8011E710(Task* arg0);

void func_m4a1_javelin_8011F5D4(Task* arg0);

void func_hypervelocity_8011F724(Task* arg0);

u16 D_80112964[5][2] = {
    { 16, 240 },
    { 64, 256 },
    { 80, 272 },
    { 96, 288 },
    { 224, 304 },
};
s32 D_80112978[54] = {
    -0x7FF9FFF0,
    -0x7FF9FFF0,
    -0x7FF9FF04,
    -0x7FF9FFE5,
    -0x7FF9FE60,
    -0x7FF9FE60,
    -0x7FF9FF42,
    -0x7FF9FF42,
    -0x7FF9FF42,
    -0x7FF9FFE8,
    -0x7FF9FFE8,
    -0x7FF9FFE8,
    -0x7FF9FFEC,
    -0x7FF9FFEC,
    -0x7FF9FFEC,
    -0x7FF9FF40,
    -0x7FF9FF40,
    -0x7FF9FF40,
    -0x7FF9FFEE,
    -0x7FF9FFEE,
    -0x7FF9FFEE,
    -0x7FF9FFEB,
    -0x7FF9FFEB,
    -0x7FF9FFEB,
    -0x7FF9FF55,
    -0x7FF9FF55,
    -0x7FF9FF55,
    -0x7FF9FF35,
    -0x7FF9FF35,
    -0x7FF9FF35,
    -0x7FF9FF34,
    -0x7FF9FF34,
    -0x7FF9FF34,
    -0x7FF9FF31,
    -0x7FF9FF31,
    -0x7FF9FF31,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    -0x7FF9FE68,
    0,
    0,
    -0x7FF9FE63,
    0,
    0,
    -0x7FF9FE69,
    0,
    0,
};
s32 D_80112A50[54] = {
    -0x7FF9FF52,
    -0x7FF9FF52,
    -0x7FF9FF52,
    -0x7FF9FF52,
    -0x7FF9FF52,
    -0x7FF9FF52,
    -0x7FF9FF52,
    -0x7FF9FF52,
    -0x7FF9FF52,
    -0x7FF9FF58,
    -0x7FF9FF58,
    -0x7FF9FF58,
    -0x7FF9FF58,
    -0x7FF9FF58,
    -0x7FF9FF58,
    -0x7FF9FF58,
    -0x7FF9FF58,
    -0x7FF9FF58,
    -0x7FF9FF58,
    -0x7FF9FF58,
    -0x7FF9FF58,
    -0x7FF9FF58,
    -0x7FF9FF58,
    -0x7FF9FF58,
    -0x7FF9FF58,
    -0x7FF9FF58,
    -0x7FF9FF58,
    -0x7FF9FF58,
    -0x7FF9FF58,
    -0x7FF9FF58,
    -0x7FF9FF58,
    -0x7FF9FF58,
    -0x7FF9FF58,
    -0x7FF9FF58,
    -0x7FF9FF58,
    -0x7FF9FF58,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
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
u16 D_80112B28[54] = {
    12,
    12,
    12,
    0,
    0,
    0,
    0,
    0,
    0,
    12,
    12,
    12,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    12,
    0,
    0,
    12,
    0,
    0,
    12,
    0,
    0,
};
s32 D_80112B94[54] = {
    16,
    39,
    40,
    31,
    41,
    42,
    47,
    48,
    33,
    29,
    45,
    46,
    25,
    49,
    50,
    36,
    51,
    52,
    17,
    53,
    54,
    26,
    55,
    56,
    32,
    57,
    58,
    35,
    61,
    62,
    34,
    63,
    64,
    37,
    43,
    44,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
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
u16 D_80112C6C[4] = {
    2182,
    3268,
    3140,
    1100,
};
EffectSpawnArg D_80112C74    = { NULL, 512, 1 };
s32            D_80112C7C[3] = {
    -0x1FF4FFFF,
    -0x1FF1FFFF,
    -0x1FEEFFFF,
};

TaskMessageEntry Gp_PlayerMsgTable[28] = {
    { ANIMATION_MESSAGE_PLAY, func_80104508 },
    { GAME_ACTOR_MESSAGE_PLACE, func_80104D68 },
    { 1002, func_80104508 },
    { 1003, func_80104508 },
    { 1004, func_80104508 },
    { ANIMATION_MESSAGE_IS_PLAYING, func_8010583C },
    { GAME_ACTOR_MESSAGE_TURN_TO_YAW, func_80104E00 },
    { GAME_ACTOR_MESSAGE_CLIMB_STAIRS, func_80104F5C },
    { GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, func_80105828 },
    { GAME_ACTOR_MESSAGE_END_SCRIPTED, Gp_EnterActorMode2 },
    { GAME_ACTOR_MESSAGE_MOVE_TO, Gp_SetActorDest },
    { GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, func_80104684 },
    { ANIMATION_MESSAGE_INSTALL_AND_PLAY, func_80104B54 },
    { GAME_ACTOR_MESSAGE_ATTACH_TO_COORD, func_80105A60 },
    { GAME_ACTOR_MESSAGE_WALK_STEPS, func_801052B8 },
    { ANIMATION_MESSAGE_COPY_BANK_EXTENSION, Gp_CopyPlayerAnim },
    { GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, func_801054D8 },
    { GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_ApplyPlayerDamage },
    { 1018, func_80105690 },
    { 1019, func_80105190 },
    { 1020, func_80105A8C },
    { ANIMATION_MESSAGE_SET_RATE, func_801058BC },
    { GAME_ACTOR_MESSAGE_MOVE_BY, Gp_MoveActorBy },
    { ANIMATION_MESSAGE_REPLACE_AND_PLAY, func_80104CAC },
    { 1024, func_801055D4 },
    { GAME_ACTOR_MESSAGE_SET_TEXTURE_SEQUENCE, func_80105AB0 },
    { 1026, func_80105754 },
    { -1, NULL },
};
u16 Gp_WeaponIdBase[2] = {
    1,
    0,
};
AnimationBank* Gp_PlayerAnimBlkTbl[34] = { NULL, &D_p08_8012A85C, &D_p08_8012A85C, &D_m93r_8012ADF8, &D_m950_8012BA78, &D_p08_8012A85C, &D_p229_8012B51C, &D_unused_85_8012A474, &D_unused_86_8012A0D8, &D_unused_86_8012A0D8, &D_mongoose_8012A9C0, &D_unused_86_8012A0D8, &D_grenade_pistol_8012B2E4, &D_mm1_8012D184, &D_pa3_8012C18C, &D_sp12_8012BCC8, &D_as12_8012C300, &D_m4a1_8012B9E4, &D_m249_8012CF58, &D_unused_86_8012A0D8, &D_tonfa_baton_8012BAB0, &D_m4a1_8012B9E4, &D_m4a1_8012B9E4, &D_8012EDD0, &D_gunblade_8012E108, &D_unused_86_8012A0D8, &D_m4a1_hammer_8012D4F4, &D_m4a1_bayonet_8012D25C, &D_m4a1_grenade_8012DF50, &D_m4a1_pyke_8012D500, &D_m4a1_javelin_8012EA20, &D_mp5a5_8012B3CC, &D_mp5a5_8012B3CC, &D_mp5a5_8012B3CC };
u16            D_80112DF4[4]           = {
    0,
    64,
    155,
    0,
};
u8 D_80112DFC[8] = {
    9,
    13,
    17,
    21,
    135,
    139,
    143,
    147,
};
u8 D_80112E04[6][2] = {
    { 0, 0 },
    { 12, 8 },
    { 12, 8 },
    { 12, 8 },
    { 12, 8 },
    { 12, 8 },
};
s16 D_80112E10[8] = {
    0,
    112,
    170,
    50,
    128,
    248,
    48,
    170,
};
u16 D_80112E20[4] = {
    0,
    64,
    40,
    88,
};
_PlayerActorVibrationPreset D_80112E28[1] = {
    { 255, 0, 2 },
};
u8 D_80112E2C[2][2] = {
    { 18, 15 },
    { 0, 0 },
};
u16 D_80112E30[33] = {
    96,
    144,
    112,
    96,
    144,
    128,
    96,
    96,
    96,
    112,
    96,
    96,
    64,
    80,
    80,
    72,
    96,
    48,
    96,
    192,
    96,
    96,
    32,
    80,
    96,
    72,
    96,
    72,
    72,
    72,
    96,
    96,
    96
};
GpuImageUpload** D_80112E74[16] = { D_aya_10400_8011CC94, D_aya_10300_8011CD1C, D_aya_10200_8011D094, D_8011D168, D_aya_10400_8011CC9C, D_aya_10300_8011CD24, D_aya_10200_8011D09C, D_8011D170, D_aya_10400_8011CCAC, D_aya_10300_8011CD34, D_aya_10200_8011D0AC, D_8011D180, D_aya_10400_8011CCBC, D_aya_10300_8011CD44, D_aya_10200_8011D0BC, D_8011D190 };
GpuImageUpload** D_80112EB4[8]  = { D_aya_10400_8011CCDC, D_aya_10300_8011CD64, D_aya_10200_8011D0DC, D_8011D1B0, D_aya_10400_8011CCD4, D_aya_10300_8011CD5C, D_aya_10200_8011D0D4, D_8011D1A8 };
/// Unused, probably stale program data: likely per-weapon boolean flags.
/// No reader identified; the original flag meaning is unknown.
u8 D_80112ED4[33] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1 };
u8 D_80112EF8[33] = {
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    1,
    1,
    1,
    1,
    1,
    1
};
u8 D_80112F1C[33][2] = {
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 1, 1 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 1, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 1 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 }
};
u16 D_80112F60[33] = {
    256,
    8704,
    8704,
    8704,
    8704,
    8704,
    8704,
    8704,
    8704,
    8704,
    8704,
    7680,
    7680,
    8704,
    8704,
    8704,
    11776,
    8704,
    256,
    64800,
    11776,
    11776,
    8704,
    1024,
    256,
    11776,
    11776,
    11776,
    11776,
    11776,
    8704,
    8704,
    8704
};
VECTOR D_80112FA4[33] = {
    { 0, 0, 0, 0 },
    { 0, -96, 0, 0 },
    { 0, -96, 0, 0 },
    { 0, -96, 0, 0 },
    { 0, -96, 0, 0 },
    { 0, -96, 0, 0 },
    { 0, -96, 0, 0 },
    { 0, -96, 0, 0 },
    { 0, -96, 0, 0 },
    { 0, -96, 0, 0 },
    { 0, -96, 0, 0 },
    { 0, -96, 0, 0 },
    { 0, -96, 0, 0 },
    { 0, -96, 0, 0 },
    { 0, -96, 0, 0 },
    { 0, -96, 0, 0 },
    { 0, -96, 0, 0 },
    { 0, -96, 0, 0 },
    { 0, -96, 0, 0 },
    { 0, 104, 128, 0 },
    { 0, -96, 0, 0 },
    { 0, -96, 0, 0 },
    { 0, -96, 0, 0 },
    { 0, 32, 0, 0 },
    { 0, -96, 0, 0 },
    { 0, -96, 0, 0 },
    { 0, -96, 0, 0 },
    { 0, -96, 0, 0 },
    { 0, -96, 0, 0 },
    { 0, -96, 0, 0 },
    { 0, -96, 0, 0 },
    { 0, -96, 0, 0 },
    { 0, -96, 0, 0 },
};
SVECTOR D_801131B4[33] = {
    { 0, 0, 0, 0 },
    { 0, 320, 96, 0 },
    { 0, 320, -96, 0 },
    { -256, 448, -96, 0 },
    { 0, 320, 96, 0 },
    { 0, 448, 128, 0 },
    { 0, 288, 128, 0 },
    { 0, 288, 96, 0 },
    { 0, 0, 0, 0 },
    { 0, 256, -96, 0 },
    { 0, 0, 0, 0 },
    { 0, 448, -96, 0 },
    { 0, 640, -96, 0 },
    { 0, 560, 96, 0 },
    { 0, 672, -224, 0 },
    { 0, 752, -224, 0 },
    { -224, 672, -96, 0 },
    { -256, 864, -320, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 672, 96, 0 },
    { 0, 672, 96, 0 },
    { 0, 0, 0, 0 },
    { -288, 32, 768, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 560, 128, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { -256, 496, -96, 0 },
    { -256, 496, -96, 0 },
    { -256, 496, -96, 0 }
};
u16 D_801132BC[33][2] = {
    { 43, 6 },
    { 43, 6 },
    { 43, 6 },
    { 43, 6 },
    { 43, 6 },
    { 43, 6 },
    { 43, 6 },
    { 43, 6 },
    { 43, 6 },
    { 106, 6 },
    { 43, 6 },
    { 108, 6 },
    { 108, 6 },
    { 161, 6 },
    { 161, 6 },
    { 161, 6 },
    { 107, 6 },
    { 107, 6 },
    { 43, 6 },
    { 43, 6 },
    { 107, 6 },
    { 107, 6 },
    { 43, 6 },
    { 43, 6 },
    { 43, 6 },
    { 107, 6 },
    { 107, 6 },
    { 107, 6 },
    { 107, 6 },
    { 107, 6 },
    { 43, 6 },
    { 43, 6 },
    { 43, 6 }
};
TaskDesc D_80113340[2] = {
    { { { TASK_BODY_NONE, 192 } }, func_8010B3F8, { NULL } },
    { { { TASK_BODY_NONE, 192 } }, func_8010B520, { NULL } },
};
EffectSpawnArg D_80113358       = { NULL, 512, 3 };
u16            Gp_AllyIdBase[4] = {
    1,
    6,
    7,
    0,
};
AnimationBank* Gp_AnimBlkTbl[8] = { NULL, &D_kyle_800101_801756D0, &D_kyle_800101_801756D0, &D_kyle_800102_801772E8, &D_kyle_800103_8017567C, &D_kyle_800104_80176C1C, &D_actor_800200_8016F208, &D_actor_800300_8016CB98 };
/// Five observed flag entries; the final F0 EE EF bytes have no known reader.
/// Their meaning and relationship to the flag array are unconfirmed.
u8 D_80113388[8] = {
    0,
    0,
    1,
    1,
    1,
    240,
    238,
    239,
};

static const TaskFuncTable3 Gp_EffTask07States;

void Gp_EffSprTask46(Task* arg0)
{
    EffectWork*           mem;
    ModelObjectCoordBody* body;
    GfxCoord*             coord;
    s16                   flag;
    s32                   param;

    mem   = arg0->spawnArg2.pointer;
    body  = arg0->extra.coordBody;
    flag  = gRoomEffectState->effectControl;
    coord = body->coord;
    if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
        if (flag >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(mem, arg0);
        }
        Gp_DrawEffSprite46(coord, mem->scale, mem->period, mem->step);
        return;
    }

    actorRenderComposeCoord(coord);
    switch (arg0->state) {
        case 0:
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gfxRotMatrixY(&coord->coord, (gRandomLcgState >> 16) & 0xFFF, 1);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            mem->angle          = arg0->spawnArg1.halves.low & 0xFFF;
            param               = arg0->spawnArg1.halves.high;
            mem->step           = param & 0xF;
            if (arg0->spawnArg1.value & 0x20000000) {
                mem->period = 0x80;
                mem->scale  = mem->angle;
                arg0->state = 4;
            } else if (arg0->spawnArg1.value & 0x10000000) {
                mem->period = 0x40;
                mem->scale  = mem->angle;
                arg0->state = 2;
            } else {
                mem->period = 0x80;
                mem->scale  = 0;
                arg0->state = 1;
            }
            break;
        case 1:
            Gp_DrawEffSprite46(coord, mem->scale, mem->period, mem->step);
            if (mem->scale < mem->angle) {
                mem->scale += 6;
            } else {
                mem->scale  = mem->angle;
                arg0->state = 2;
            }
            break;
        case 2:
            Gp_DrawEffSprite46(coord, mem->scale, mem->period, mem->step);
            if (mem->period >= 0x41) {
                mem->period--;
            }
            break;
        case 3:
            Gp_DrawEffSprite46(coord, mem->scale, mem->period, mem->step);
            mem->period -= 4;
            if (mem->period < 4) {
                effectKillTask(mem, arg0);
            }
            break;
        case 4:
            Gp_DrawEffSprite46(coord, mem->scale, mem->period, mem->step);
            break;
    }
}

static void Gp_DrawEffSprite81(Task* arg0)
{
    EffectCentreScratch*  block;
    ModelObjectCoordBody* body;
    GfxCoord*             coord;
    EffectWork*           mem;
    POLY_FT4*             prim;

    body                 = arg0->extra.coordBody;
    coord                = body->coord;
    mem                  = arg0->spawnArg2.pointer;
    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
    block->worldPoint.vx = coord->workm.t[0];
    block->worldPoint.vy = coord->workm.t[1];
    block->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        block->depth++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyFT4(prim);
        setShadeTex(prim, 1);
        setSemiTrans(prim, mem->angle);
        prim->tpage         = 0x29;
        prim->clut          = ((D_80112964[1][mem->step] >> 4) & 0x3F) | 0x4280;
        prim->u0            = ((mem->age >> 1) & 7) * 16;
        prim->v0            = 0xB8;
        prim->u1            = (((mem->age >> 1) & 7) * 16) + 0xF;
        prim->v1            = 0xB8;
        prim->u2            = ((mem->age >> 1) & 7) * 16;
        prim->v2            = 0xC7;
        prim->u3            = (((mem->age >> 1) & 7) * 16) + 0xF;
        prim->v3            = 0xC7;
        block->screenExtent = ((mem->scale * 0xF) / block->depth) >> 1;
        prim->x0 = prim->x2 = block->screenX - (u16)block->screenExtent;
        prim->x1 = prim->x3 = block->screenX + (u16)block->screenExtent;
        prim->y0 = prim->y1 = block->screenY - (u16)block->screenExtent;
        prim->y2 = prim->y3 = block->screenY + (u16)block->screenExtent;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

static void Gp_DrawEffSprite46(GfxCoord* arg0, s32 arg1, s16 arg2, u16 arg3)
{
    EffectQuadScratch* quadScratch;
    s32                i;
    POLY_FT4*          prim;

    quadScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectQuadScratch);
    for (i = 0; i < ARRAY_SIZE(D_80111E38); i++) {
        quadScratch->vertices[i].vx = (u16)D_80111E38[i].axis0Sign * arg1;
        quadScratch->vertices[i].vy = 0;
        quadScratch->vertices[i].vz = (u16)D_80111E38[i].axis1Sign * arg1;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&quadScratch->vertices[i]);
        gte_rtv0();
        gte_stsv(&quadScratch->vertices[i]);
        quadScratch->vertices[i].vx += arg0->workm.t[0];
        quadScratch->vertices[i].vy += arg0->workm.t[1];
        quadScratch->vertices[i].vz += arg0->workm.t[2];
    }

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&quadScratch->vertices[0]);
    gte_rtps();
    gte_stsxy(&quadScratch->screenCorners[0]);
    gte_ldv3(&quadScratch->vertices[1], &quadScratch->vertices[2], &quadScratch->vertices[3]);
    gte_rtpt();
    gte_stsxy3(&quadScratch->screenCorners[1], &quadScratch->screenCorners[2], &quadScratch->screenCorners[3]);
    gte_stflg(&quadScratch->projectionFlags);
    if (quadScratch->projectionFlags >= 0) {
        gte_stszotz(&quadScratch->depth);
        quadScratch->depth += 0x20;
        prim                = gGpuPrimCursor;
        gGpuPrimCursor      = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->tpage = 0x29;
        prim->clut  = getClut(arg3 * 0x110 + 0x10, 0x10A);
        prim->v0    = 0xC8;
        prim->v1    = 0xC8;
        prim->r0    = arg2;
        prim->g0    = arg2;
        prim->b0    = arg2;
        prim->u0    = 0;
        prim->u1    = 0x37;
        prim->u2    = 0;
        prim->v2    = 0xFF;
        prim->u3    = 0x37;
        prim->v3    = 0xFF;
        prim->x0    = quadScratch->screenCorners[0].vx;
        prim->y0    = quadScratch->screenCorners[0].vy;
        prim->x1    = quadScratch->screenCorners[1].vx;
        prim->y1    = quadScratch->screenCorners[1].vy;
        prim->x2    = quadScratch->screenCorners[2].vx;
        prim->y2    = quadScratch->screenCorners[2].vy;
        prim->x3    = quadScratch->screenCorners[3].vx;
        prim->y3    = quadScratch->screenCorners[3].vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)quadScratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadScratch);
}

void Gp_EffSprTask81(Task* arg0)
{
    EffectWork*           mem;
    ModelObjectCoordBody* body;
    GfxCoord*             coord;
    GfxCoord*             parent;
    MATRIX*               m;
    MATRIX*               world;
    s16                   flag;
    s32                   one;

    body   = arg0->extra.coordBody;
    mem    = arg0->spawnArg2.pointer;
    flag   = gRoomEffectState->effectControl;
    coord  = body->coord;
    parent = mem->parent;
    if (flag >= ROOM_EFFECT_CONTROL_HIDDEN) {
        effectKillTask(mem, arg0);
        return;
    }

    actorRenderComposeCoord(parent);
    coord->workm = parent->workm;
    gte_SetRotMatrix(&parent->workm);
    gte_SetTransMatrix(&parent->workm);
    world = &gGfxViewCoord.workm;
    gfxMakeRelativeTransform(world, &coord->workm, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);

    switch (arg0->spawnArg1.value) {
        case 0:
            mem->scale            = 0x280;
            mem->step             = 1;
            mem->angle            = 0;
            arg0->spawnArg1.value = 1;
            if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                break;
            }
            Gp_SpawnEff(EFFECT_TRAIL_PUFF, coord, mem->scale + 0x22200 + mem->scale, 0);
            break;
        case 1:
            Gp_DrawEffSprite81(arg0);
            if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                break;
            }
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (((gRandomLcgState >> 16) & 3) == 0) {
                Gp_SpawnEff(EFFECT_TRAIL_PUFF, coord, mem->scale + 0x21000, 0);
            }
            mem->age++;
            break;
        case 2:
            if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                if (mem->index == 0) {
                    Gp_SpawnEff(EFFECT_TRAIL_PUFF, coord, mem->scale + 0x22200, 0);
                    mem->index           = 1;
                    mem->age             = 0;
                    mem->scale         >>= 2;
                    one                  = ONE;
                    *(s32*)&coord->coord = one;
                    m                    = &coord->coord;
                    MATRIX_PAIR(m, 0, 2) = 0;
                    MATRIX_PAIR(m, 1, 1) = one;
                    MATRIX_PAIR(m, 2, 0) = 0;
                    m->m[2][2]           = one;
                }
                mem->age += (u16)gDisplayState.animFrame & 1;
            }
            if (mem->age < 0x10) {
                Gp_DrawEffQuadT29(coord, mem->scale, mem->age >> 1, mem->step);
            } else {
                arg0->spawnArg1.value = 4;
                break;
            }
            goto lcg;
        case 3:
            if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING && mem->index == 0) {
                Gp_SpawnEff(EFFECT_TRAIL_PUFF, coord, mem->scale + 0x22200, 0);
                mem->index   = 1;
                mem->age     = 0;
                mem->scale >>= 2;
            }
            mem->age++;
            if (mem->age >= 0x10) {
                arg0->spawnArg1.value = 4;
                break;
            }
        lcg:
            if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                break;
            }
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((u16)((gRandomLcgState >> 16) % 3U) == 0) {
                Gp_SpawnEff(EFFECT_PROJECTILE_BURST_PARTICLE, coord, (s32)(mem->scale), 0);
            }
            break;
        case 4:
            effectKillTask(mem, arg0);
            break;
    }
}

void Gp_EffSprTask55(Task* arg0)
{
    EffectWork*         mem;
    GfxCoord*           coord;
    EffectShapeScratch* block;
    POLY_FT4*           prim;
    s16                 flag;
    s32                 t;
    s32                 amt;
    s32                 rng;
    s32                 temp;
    s32                 pal;
    SVECTOR*            vec;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        effectKillTask(mem, arg0);
    } else {
        actorRenderComposeCoord(coord);
        block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
        block->worldPoint.vx = coord->workm.t[0];
        block->worldPoint.vy = coord->workm.t[1];
        block->worldPoint.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->worldPoint);
        gte_rtps();
        gte_stsxy(&block->screenX);
        gte_stflg(&block->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&block->depth);
            prim           = gGpuPrimCursor;
            block->depth   = block->depth + 1;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2C);
            if (arg0->state == 0) {
                t   = (u16)arg0->spawnArg1.value & 0xFFF;
                amt = 0x200;
                if (t != 0) {
                    amt = t;
                }
                rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->scale      = amt;
                mem->angle      = ((u32)rng >> 16) & 0xFFF;
                temp            = ((u16)arg0->spawnArg1.value & 0xF000) << 16;
                gRandomLcgState = rng;
                if (temp != 0) {
                    temp = temp >> 28;
                } else {
                    temp = 1;
                }
                mem->period   = temp;
                pal           = arg0->spawnArg1.halves.high;
                mem->step     = pal & 3;
                mem->index    = (arg0->spawnArg1.value >> 28) & 1;
                mem->move.vx += ((mem->angle & 0xF) * rsin(mem->angle)) >> 12;
                mem->move.vy -= 0x18;
                mem->move.vz += ((mem->angle & 0xF) * rcos(mem->angle)) >> 12;
                if (arg0->spawnArg1.value & 0x100000) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vx    = ((gRandomLcgState >> 16) & 0x1F) - 0x10;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vy    = ((gRandomLcgState >> 16) & 0x1F) - 0x10;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vz    = ((gRandomLcgState >> 16) & 0x1F) - 0x10;
                }
                if (arg0->spawnArg1.value & 0x01000000) {
                    gte_lddp(mem->scale << 2);
                    vec = &mem->move;
                    gte_ldsv(vec);
                    gte_gpf12();
                    gte_stsv(vec);
                }
                arg0->state = 1;
            }
            prim->code            |= 3;
            prim->tpage            = ((mem->step & 3) << 5) | 9;
            prim->clut             = ((mem->index * 14) & 0x42BE) | 0x4281;
            prim->u0               = (mem->age / mem->period) << 5;
            prim->v0               = 0x78;
            prim->u1               = ((mem->age / mem->period) << 5) + 0x1F;
            prim->v1               = 0x78;
            prim->u2               = (mem->age / mem->period) << 5;
            prim->v2               = 0x97;
            prim->u3               = ((mem->age / mem->period) << 5) + 0x1F;
            prim->v3               = 0x97;
            block->extent.corner.x = (((mem->scale * 0x1F) / block->depth) * rsin(mem->angle)) >> 12;
            block->extent.corner.y = (((mem->scale * 0x1F) / block->depth) * rcos(mem->angle)) >> 12;
            prim->x0               = block->screenX + (u16)block->extent.corner.x;
            prim->x3               = block->screenX - (u16)block->extent.corner.x;
            prim->y0               = block->screenY - (u16)block->extent.corner.y;
            prim->y3               = block->screenY + (u16)block->extent.corner.y;
            block->extent.corner.x = (((mem->scale * 0x1F) / block->depth) * rsin(mem->angle + 0x400)) >> 12;
            block->extent.corner.y = (((mem->scale * 0x1F) / block->depth) * rcos(mem->angle + 0x400)) >> 12;
            prim->x1               = block->screenX + (u16)block->extent.corner.x;
            prim->x2               = block->screenX - (u16)block->extent.corner.x;
            prim->y1               = block->screenY - (u16)block->extent.corner.y;
            prim->y2               = block->screenY + (u16)block->extent.corner.y;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        coord->coord.t[0]  += mem->move.vx;
        coord->coord.t[1]  += mem->move.vy;
        coord->coord.t[2]  += mem->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        mem->move.vy       += 6;
        mem->age++;
        if (mem->age > mem->period * 8 - 1) {
            effectKillTask(mem, arg0);
        }
    }
}

void Gp_EffSprTask42(Task* arg0)
{
    EffectWork*         mem;
    GfxCoord*           coord;
    EffectShapeScratch* block;
    POLY_FT4*           prim;
    s16                 flag;
    s32                 t;
    s32                 amt;
    s32                 rng;
    s32                 temp;
    s32                 pal;
    SVECTOR*            vec;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        effectKillTask(mem, arg0);
    } else {
        actorRenderComposeCoord(coord);
        block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
        block->worldPoint.vx = coord->workm.t[0];
        block->worldPoint.vy = coord->workm.t[1];
        block->worldPoint.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->worldPoint);
        gte_rtps();
        gte_stsxy(&block->screenX);
        gte_stflg(&block->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&block->depth);
            prim           = gGpuPrimCursor;
            block->depth   = block->depth + 1;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2C);
            if (arg0->state == 0) {
                t   = (u16)arg0->spawnArg1.value & 0xFFF;
                amt = 0x200;
                if (t != 0) {
                    amt = t;
                }
                rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->scale      = amt;
                mem->angle      = ((u32)rng >> 16) & 0xFFF;
                temp            = ((u16)arg0->spawnArg1.value & 0xF000) << 16;
                gRandomLcgState = rng;
                if (temp != 0) {
                    temp = temp >> 28;
                } else {
                    temp = 1;
                }
                mem->period  = temp;
                pal          = arg0->spawnArg1.halves.high;
                mem->step    = pal & 3;
                mem->move.vy = mem->move.vy - 0x18;
                if (arg0->spawnArg1.value & 0x100000) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vx    = ((gRandomLcgState >> 16) & 0x1F) - 0x10;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vy    = ((gRandomLcgState >> 16) & 0x1F) - 0x10;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vz    = ((gRandomLcgState >> 16) & 0x1F) - 0x10;
                }
                if (arg0->spawnArg1.value & 0x01000000) {
                    gte_lddp(mem->scale << 2);
                    vec = &mem->move;
                    gte_ldsv(vec);
                    gte_gpf12();
                    gte_stsv(vec);
                }
                arg0->state = 1;
            }
            prim->code            |= 3;
            prim->tpage            = ((mem->step & 3) << 5) | 9;
            prim->clut             = 0x4285;
            prim->u0               = (mem->age / mem->period) * 0x10 - 0x80;
            prim->v0               = 0xB8;
            prim->u1               = (mem->age / mem->period) * 0x10 - 0x71;
            prim->v1               = 0xB8;
            prim->u2               = (mem->age / mem->period) * 0x10 - 0x80;
            prim->v2               = 0xC7;
            prim->u3               = (mem->age / mem->period) * 0x10 - 0x71;
            prim->v3               = 0xC7;
            block->extent.corner.x = (((mem->scale * 15) / block->depth) * rsin(mem->angle)) >> 12;
            block->extent.corner.y = (((mem->scale * 15) / block->depth) * rcos(mem->angle)) >> 12;
            prim->x0               = block->screenX + (u16)block->extent.corner.x;
            prim->x3               = block->screenX - (u16)block->extent.corner.x;
            prim->y0               = block->screenY - (u16)block->extent.corner.y;
            prim->y3               = block->screenY + (u16)block->extent.corner.y;
            block->extent.corner.x = (((mem->scale * 15) / block->depth) * rsin(mem->angle + 0x400)) >> 12;
            block->extent.corner.y = (((mem->scale * 15) / block->depth) * rcos(mem->angle + 0x400)) >> 12;
            prim->x1               = block->screenX + (u16)block->extent.corner.x;
            prim->x2               = block->screenX - (u16)block->extent.corner.x;
            prim->y1               = block->screenY - (u16)block->extent.corner.y;
            prim->y2               = block->screenY + (u16)block->extent.corner.y;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        coord->coord.t[0]  += mem->move.vx;
        coord->coord.t[1]  += mem->move.vy;
        coord->coord.t[2]  += mem->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        mem->move.vy       += 6;
        mem->age++;
        if (mem->age > mem->period * 8 - 1) {
            effectKillTask(mem, arg0);
        }
    }
}

void func_800F91AC(Task* arg0)
{
    EffectWork*       mem;
    GfxCoord*         coord;
    GfxRotationWords* rot;
    s16               flag;
    s16               width;
    s32               half;
    s32               i;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        if (arg0->state == 0) {
            coord->parent       = mem->parent;
            rot                 = (GfxRotationWords*)&coord->coord;
            rot->m00M01         = ONE;
            rot->m02M10         = 0;
            rot->m11M12         = ONE;
            rot->m20M21         = 0;
            rot->m22            = ONE;
            coord->coord.t[0]   = mem->pos.vx;
            coord->coord.t[1]   = mem->pos.vy;
            coord->coord.t[2]   = mem->pos.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            arg0->state         = 1;
            mem->scale          = arg0->spawnArg1.value;
            mem->angle          = arg0->spawnArg1.value >> 16;
            mem->period         = mem->angle * 3;
            mem->step           = mem->scale / 768 + 1;
        }
        actorRenderComposeCoord(coord);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        if (mem->age < mem->period) {
            goto spawn;
        }
    }
    effectKillTask(mem, arg0);
    return;
spawn:
    width = mem->scale;
    half  = width >> 1;
    for (i = 0; i < mem->step; i++) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->move.vx    = (s32)(gRandomLcgState >> 16) % width - half;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->move.vy    = (s32)(gRandomLcgState >> 16) % width - half;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->move.vz    = (s32)(gRandomLcgState >> 16) % width - half;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        Gp_SpawnEff(EFFECT_HIT_PUFF, coord, ((gRandomLcgState >> 16) & 0x1000) + 0x11200, &mem->move);
    }
    mem->age++;
}

void Gp_EffCtlTask9B(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s16         flag;
    EffectWork* spawned;
    s32         temp;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        if (arg0->state == 0) {
            coord->parent       = mem->parent;
            coord->coord.t[0]   = mem->pos.vx;
            coord->coord.t[1]   = mem->pos.vy;
            coord->coord.t[2]   = mem->pos.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            arg0->state         = 1;
            mem->scale          = ((u16)arg0->spawnArg1.value * 3u) >> 4;
            temp                = arg0->spawnArg1.halves.high;
            mem->angle          = temp;
            mem->period         = temp << 2;
            if ((mem->pos.vx | mem->pos.vy | mem->pos.vz) == 0) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->pos.vx     = ((gRandomLcgState >> 16) & 0xFFF) - 0x800;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->pos.vy     = ((gRandomLcgState >> 16) & 0xFFF) - 0x800;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->pos.vz     = ((gRandomLcgState >> 16) & 0xFFF) - 0x800;
            }
            VectorNormalSS(&mem->pos, &mem->move);
        }
        actorRenderComposeCoord(coord);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        if (mem->age < mem->period) {
            goto spawn;
        }
    }
    effectKillTask(mem, arg0);
    return;
spawn:
    spawned = Gp_SpawnEff(EFFECT_HIT_PUFF, coord, 0x12200, 0);
    if (spawned != NULL) {
        gte_lddp(mem->scale - mem->age * (mem->angle + 5));
        gte_ldsv(&mem->move);
        gte_gpf12();
        gte_stsv(&spawned->move);
    }
    mem->age++;
}

void Gp_EffSprTask30(Task* arg0)
{
    EffectWork*       mem;
    GfxCoord*         coord;
    GfxRotationWords* rot;
    s16               flag;
    s32               sub;
    s32               ret;
    s32               id;
    s32               base;
    SVECTOR           vec;
    SVECTOR           dir;
    SVECTOR           wpos;
    u8                color[3];

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
        if (flag >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(mem, arg0);
        }
        return;
    }
    actorRenderComposeCoord(coord);
    mem->age++;
    switch (arg0->state) {
        case 0:
            rot             = (GfxRotationWords*)&coord->coord;
            rot->m00M01     = ONE;
            rot->m11M12     = ONE;
            rot->m22        = ONE;
            rot->m02M10     = 0;
            rot->m20M21     = 0;
            mem->pos.vx     = arg0->spawnArg1.halves.low & 0xFFF;
            mem->scale      = 0x100;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->pos.vy     = (gRandomLcgState >> 16) & 7;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->index      = (gRandomLcgState >> 16) & 7;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->pos.vz     = (gRandomLcgState >> 16) & 0xFFF;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->period     = 0x200 - ((gRandomLcgState >> 16) & 0x3FF);
            if ((mem->move.vx | mem->move.vy | mem->move.vz) == 0) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vy    = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vz    = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&mem->move);
                gte_rtv0();
                gte_stsv(&mem->move);
            }
            VectorNormalSS(&mem->move, &mem->move);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            sub                   = arg0->spawnArg1.halves.high;
            arg0->state           = 1;
            arg0->spawnArg1.value = sub & 3;
            return;
        case 1:
            if (mem->age >= 0x51) {
                effectKillTask(mem, arg0);
                return;
            }
            mem->pos.vz += mem->period;
            if (mem->pos.vy != 0 && mem->age % mem->pos.vy == 0) {
                mem->index++;
            }
            gte_lddp(mem->scale);
            gte_ldsv(&mem->move);
            gte_gpf12();
            gte_stsv(&vec);
            coord->coord.t[0]  += vec.vx;
            coord->coord.t[1]  += vec.vy;
            coord->coord.t[2]  += vec.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            gte_SetRotMatrix(&gGfxViewCoord.workm);
            gte_ldv0(&vec);
            gte_rtv0();
            gte_stsv(&dir);
            wpos.vx = (u16)coord->workm.t[0];
            wpos.vy = (u16)coord->workm.t[1];
            wpos.vz = (u16)coord->workm.t[2];
            dir.vx += wpos.vx;
            dir.vy += wpos.vy;
            dir.vz += wpos.vz;
            ret     = func_800DE7CC(&dir, &wpos, &dir, &wpos);
            if (ret == 1) {
                coord->coord.t[0] -= vec.vx;
                coord->coord.t[1] -= vec.vy;
                coord->coord.t[2] -= vec.vz;
                mem->move.vx       = ((s16)(u16)wpos.vx >> 1) + (mem->move.vx >> 1);
                mem->move.vy       = wpos.vy + (mem->move.vy >> 1);
                mem->move.vz       = ((s16)(u16)wpos.vz >> 1) + (mem->move.vz >> 1);
                VectorNormalSS(&mem->move, &mem->move);
                mem->scale  = mem->scale >> 1;
                mem->period = mem->period >> 1;
                gte_lddp(mem->scale);
                gte_ldsv(&mem->move);
                gte_gpf12();
                gte_stsv(&vec);
                coord->coord.t[0]  += vec.vx;
                coord->coord.t[1]  += vec.vy;
                coord->coord.t[2]  += vec.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if (((gRandomLcgState >> 16) & 1) != 0) {
                    if (arg0->spawnArg1.value == 1) {
                        base = 0xC0001100;
                        id   = 0x60070;
                    } else {
                        base = 0x12100;
                        id   = 0x60055;
                    }
                    Gp_SpawnEff(id, coord, mem->pos.vx + base, NULL);
                }
                if (mem->age - mem->step < 8 && mem->scale < 0x20) {
                    arg0->state = arg0->spawnArg1.value + 2;
                    mem->scale  = 0;
                    mem->period = 0;
                    mem->angle  = 0x80;
                } else {
                    mem->step = mem->age;
                }
            } else if (mem->scale != 0) {
                actorRenderComposeCoord(coord);
                mem->move.vy += 0x10000 / mem->scale;
            }
            if (arg0->spawnArg1.value == 2) {
                color[0] = color[1] = color[2] = 0x80;
                Gp_DrawEffSpark(arg0, arg0->spawnArg1.value, color);
            } else {
                Gp_DrawEffSpark(arg0, arg0->spawnArg1.value, NULL);
            }
            return;
        case 2:
            actorRenderComposeCoord(coord);
            if (mem->age >= 0x33) {
                if (mem->angle < 0x10) {
                    mem->scale++;
                    if (mem->scale < 8) {
                        Gp_DrawEffQuadT29(coord, mem->period, mem->scale, 0);
                    } else {
                        effectKillTask(mem, arg0);
                    }
                } else {
                    u16 rnd;

                    color[0] = color[1] = color[2] = mem->angle;
                    Gp_DrawEffSpark(arg0, arg0->spawnArg1.value, color);
                    mem->period    += mem->pos.vx >> 4;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    rnd             = (gRandomLcgState >> 16) % 3;
                    if (rnd == 0) {
                        Gp_SpawnEff(EFFECT_RISING_WISP, coord, (s32)(mem->pos.vx), NULL);
                    }
                    Gp_DrawEffQuadT29(coord, mem->period, 0, 0);
                    mem->angle -= 0x10;
                }
            } else {
                Gp_DrawEffSpark(arg0, arg0->spawnArg1.value, NULL);
            }
            return;
        case 3:
            actorRenderComposeCoord(coord);
            if (mem->age >= 0x33) {
                if (mem->angle < 0x10) {
                    effectKillTask(mem, arg0);
                    return;
                }
                color[0] = color[1] = color[2] = mem->angle;
                Gp_DrawEffSpark(arg0, arg0->spawnArg1.value, color);
                mem->angle -= 0x10;
            } else {
                Gp_DrawEffSpark(arg0, arg0->spawnArg1.value, NULL);
            }
            return;
        case 4:
            actorRenderComposeCoord(coord);
            if (mem->age >= 0x33) {
                if (mem->angle < 0x10) {
                    mem->scale++;
                    if (mem->scale < 8) {
                        Gp_DrawEffQuadT29(coord, mem->period, mem->scale, 0);
                    } else {
                        effectKillTask(mem, arg0);
                    }
                } else {
                    u16 rnd;

                    color[0] = color[1] = color[2] = mem->angle;
                    Gp_DrawEffSpark(arg0, arg0->spawnArg1.value, color);
                    mem->period    += mem->pos.vx >> 4;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    rnd             = (gRandomLcgState >> 16) % 3;
                    if (rnd == 0) {
                        Gp_SpawnEff(EFFECT_RISING_WISP, coord, (s32)(mem->pos.vx), NULL);
                    }
                    Gp_DrawEffQuadT29(coord, mem->period, 0, 0);
                    mem->angle -= 0x10;
                }
            } else {
                color[0] = color[1] = color[2] = mem->angle;
                Gp_DrawEffSpark(arg0, arg0->spawnArg1.value, color);
            }
            return;
    }
}

static const TaskFuncTable3 Gp_EffTask07States = { {
    Gp_EffTask07State0,
    Gp_EffTask07State1,
    taskKill,
} };

static void Gp_DrawEffSpark(Task* arg0, s32 arg1, u8* arg2)
{
    EffectShapeScratch*   block;
    EffectWork*           mem;
    ModelObjectCoordBody* body;
    GfxCoord*             coord;
    POLY_FT4*             prim;
    u16                   abr;
    s32                   uv;
    s32                   uv2;
    s32                   ang;
    u16                   size;
    u16                   frame;
    u16                   angle;

    body                 = arg0->extra.coordBody;
    mem                  = arg0->spawnArg2.pointer;
    abr                  = 1;
    coord                = body->coord;
    size                 = mem->pos.vx;
    frame                = mem->index;
    angle                = mem->pos.vz;
    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    block->worldPoint.vx = coord->workm.t[0];
    block->worldPoint.vy = coord->workm.t[1];
    block->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        block->depth   = block->depth + 1;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        if (arg1 == 1) {
            if (arg2 != NULL) {
                abr = 2;
                setRGB0(prim, arg2[0], arg2[1], arg2[2]);
                setSemiTrans(prim, 1);
            } else {
                setRGB0(prim, 0x20, 0x20, 0x20);
            }
        } else if (arg2 != NULL) {
            setRGB0(prim, arg2[0], arg2[1], arg2[2]);
            setSemiTrans(prim, 1);
        } else {
            setcode(prim, 0x2D);
        }
        prim->tpage = (abr << 5) | 8;
        prim->clut  = 0x428E;
        uv          = (frame & 7) * 0x18;
        uv2         = uv + 0x17;
        setUV4(prim, uv, 0xB8, uv2, 0xB8, uv, 0xCF, uv2, 0xCF);
        block->extent.corner.x = ((((s16)size * 23) / block->depth) * rsin((s16)angle)) >> 12;
        block->extent.corner.y = ((((s16)size * 23) / block->depth) * rcos((s16)angle)) >> 12;
        prim->x0               = block->screenX + (u16)block->extent.corner.x;
        prim->x3               = block->screenX - (u16)block->extent.corner.x;
        prim->y0               = block->screenY - (u16)block->extent.corner.y;
        prim->y3               = block->screenY + (u16)block->extent.corner.y;
        ang                    = (s16)angle + 0x400;
        block->extent.corner.x = ((((s16)size * 23) / block->depth) * rsin(ang)) >> 12;
        block->extent.corner.y = ((((s16)size * 23) / block->depth) * rcos(ang)) >> 12;
        prim->x1               = block->screenX + (u16)block->extent.corner.x;
        prim->x2               = block->screenX - (u16)block->extent.corner.x;
        prim->y1               = block->screenY - (u16)block->extent.corner.y;
        prim->y2               = block->screenY + (u16)block->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

static void Gp_DrawEffQuadT29(GfxCoord* arg0, s32 arg1, u16 arg2, u16 arg3)
{
    EffectQuadScratch* quadScratch;
    s32                i;
    POLY_FT4*          prim;
    s32                u0;
    s32                u1;

    quadScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectQuadScratch);
    for (i = 0; i < ARRAY_SIZE(D_80111E38); i++) {
        quadScratch->vertices[i].vx = (u16)D_80111E38[i].axis0Sign * arg1;
        quadScratch->vertices[i].vy = 0;
        quadScratch->vertices[i].vz = (u16)D_80111E38[i].axis1Sign * arg1;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&quadScratch->vertices[i]);
        gte_rtv0();
        gte_stsv(&quadScratch->vertices[i]);
        quadScratch->vertices[i].vx += arg0->workm.t[0];
        quadScratch->vertices[i].vy += arg0->workm.t[1];
        quadScratch->vertices[i].vz += arg0->workm.t[2];
    }

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&quadScratch->vertices[0]);
    gte_rtps();
    gte_stsxy(&quadScratch->screenCorners[0]);
    gte_stflg(&quadScratch->projectionFlags);
    if (quadScratch->projectionFlags >= 0) {
        gte_ldv3(&quadScratch->vertices[1], &quadScratch->vertices[2], &quadScratch->vertices[3]);
        gte_rtpt();
        gte_stsxy3(&quadScratch->screenCorners[1], &quadScratch->screenCorners[2], &quadScratch->screenCorners[3]);
        gte_stflg(&quadScratch->projectionFlags);
        if (quadScratch->projectionFlags >= 0) {
            gte_stszotz(&quadScratch->depth);
            quadScratch->depth++;
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->tpage = 0x29;
            prim->clut  = ((D_80112964[2][arg3] >> 4) & 0x3F) | 0x4280;
            u0          = (arg2 & 7) * 0x10 - 0x80;
            u1          = (arg2 & 7) * 0x10 - 0x71;
            setUV4(prim, u0, 0xB8, u1, 0xB8, u0, 0xC7, u1, 0xC7);
            prim->x0 = quadScratch->screenCorners[0].vx;
            prim->y0 = quadScratch->screenCorners[0].vy;
            prim->x1 = quadScratch->screenCorners[1].vx;
            prim->y1 = quadScratch->screenCorners[1].vy;
            prim->x2 = quadScratch->screenCorners[2].vx;
            prim->y2 = quadScratch->screenCorners[2].vy;
            prim->x3 = quadScratch->screenCorners[3].vx;
            prim->y3 = quadScratch->screenCorners[3].vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)quadScratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadScratch);
}

static void Gp_EffTask07State1(Task* arg0)
{
    Task* slot;
    s32   kind;
    s32   spawnId;
    s32   idx;

    slot = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (slot == NULL) {
        return;
    }
    if (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        return;
    }
    kind = Gp_StateC08.effectPhase;
    if (kind == 2) {
        return;
    }
    if ((gRoomEffectState->battleState != ROOM_EFFECT_BATTLE_ENGAGED) && ((Gp_StateC08.attachId / 10U) != ATTACHMENT_ID_HEALING_FAMILY)) {
        return;
    }
    if (kind == -1) {
        spawnId = D_80112A50[((u16)(Gp_StateC08.attachId / 100U) - 1) * 9 +
                             ((u16)((u16)(Gp_StateC08.attachId / 10U) % 10U) - 1) * 3 + kind +
                             (u16)(Gp_StateC08.attachId % 10U)];
        if (spawnId == 0) {
            return;
        }
        Gp_SpawnEff(spawnId, slot->extra.tmd->coords,
                    (s32)(Gp_StateC08.duration), 0);
    } else if (kind == 1) {
        idx = ((u16)(Gp_StateC08.attachId / 100U) - 1) * 9 +
              ((u16)((u16)(Gp_StateC08.attachId / 10U) % 10U) - 1) * 3 - 1;
        idx    += (u16)(Gp_StateC08.attachId % 10U);
        spawnId = D_80112978[idx];
        if (spawnId == 0) {
            return;
        }
        Gp_SpawnEff(spawnId,
                    &slot->extra.tmd->coords[D_80112B28[idx]], 0,
                    0);
    }
}

void func_800FAA14(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s32         pan;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (arg0->state == 0) {
        arg0->spawnArg1.value = D_80112B94[((u16)(Gp_StateC08.attachId / 100U) - 1) * 9 +
                                           ((u16)((u16)(Gp_StateC08.attachId / 10U) % 10U) - 1) * 3 +
                                           ((u16)(Gp_StateC08.attachId % 10U) - 1U)];
    }
    actorRenderComposeCoord(coord);
    if (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        goto kill;
    }
    if (Gp_StateC08.duration == 0) {
        goto kill;
    }
    if (Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_CANCELLED) {
        goto kill;
    }
    if (gRoomEffectState->battleState != ROOM_EFFECT_BATTLE_ENGAGED && (u16)(Gp_StateC08.attachId / 10U) != ATTACHMENT_ID_HEALING_FAMILY) {
    kill:
        if (arg0->spawnArg1.value != 0) {
            SndEvt_EnqueueType7(arg0->spawnArg1.value, 1);
        }
        effectKillTask(mem, arg0);
        return;
    }
    if (Gp_StateC08.duration >= 9) {
        if (mem->scale < 0x20) {
            if (mem->scale == 0) {
                pan = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(arg0->spawnArg1.value, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            Gp_SpawnEff(EFFECT_PE_CHARGE_PARTICLE, coord, 0, 0);
            mem->scale++;
        }
    }
}

void Gp_EffCtlTask32(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    GfxCoord*   parent;
    MATRIX*     m;
    s16         angle;
    s16         temp;
    s32         state;
    s32         lcg;
    u32         lcg2;
    u16         step;
    s32         one;
    s32         newState;

    mem      = arg0->spawnArg2.pointer;
    coord    = arg0->extra.coordBody->coord;
    step     = mem->age + 1;
    mem->age = step;
    state    = arg0->state;
    switch (state) {
        case 0:
            mem->angle      = 0x180;
            mem->period     = 0x80;
            mem->step       = 0x400;
            lcg             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            lcg2            = lcg * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            temp            = ((lcg2 >> 0x10) & 0x3FF) - 0x200;
            gRandomLcgState = lcg;
            mem->scale      = ((u32)lcg >> 0x10) & 0xFFF;
            gRandomLcgState = lcg2;
            mem->move.vz    = temp;
            mem->move.vx    = (rcos(temp) * mem->step) >> 0xC;
            mem->move.vy    = ((rsin(mem->move.vz) * mem->step) >> 0xC) - 0x400;
            parent =
                (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
            one                  = ONE;
            *(s32*)&coord->coord = one;
            coord->parent        = parent;
            m                    = &coord->coord;
            MATRIX_PAIR(m, 0, 2) = 0;
            MATRIX_PAIR(m, 1, 1) = one;
            MATRIX_PAIR(m, 2, 0) = 0;
            m->m[2][2]           = one;
            angle                = (mem->age + mem->scale) * 0x18;
            coord->coord.t[0]    = (rcos(angle) * mem->move.vx) >> 0xC;
            coord->coord.t[1] =
                mem->move.vy +
                ((rsin((gDisplayState.animFrame + mem->scale) << 6) * 0x60) >> 0xC);
            coord->coord.t[2]   = (rsin(angle) * mem->move.vx) >> 0xC;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            arg0->state = 1;
            if (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
                goto set_state_4;
            }
            if (Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_CANCELLED) {
                newState = 4;
                goto set_state;
            }
            break;
        case 1:
            angle             = ((s16)step + mem->scale) * 0x18;
            coord->coord.t[0] = (rcos(angle) * mem->move.vx) >> 0xC;
            coord->coord.t[1] =
                mem->move.vy +
                ((rsin((gDisplayState.animFrame + mem->scale) << 6) * 0x60) >> 0xC);
            coord->coord.t[2]   = (rsin(angle) * mem->move.vx) >> 0xC;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            if ((gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) || (Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_CANCELLED)) {
                newState = 3;
                goto set_state;
            }
            if (Gp_StateC08.duration < 8) {
                arg0->state = 2;
            }
            break;
        case 2:
            mem->step         = mem->step - 0x80;
            angle             = ((s16)step + mem->scale) * 0x18;
            mem->move.vx      = (rcos(mem->move.vz) * mem->step) >> 0xC;
            mem->move.vy      = ((rsin(mem->move.vz) * mem->step) >> 0xC) - 0x400;
            coord->coord.t[0] = (rcos(angle) * mem->move.vx) >> 0xC;
            coord->coord.t[1] =
                mem->move.vy +
                ((rsin((gDisplayState.animFrame + mem->scale) << 6) * 0x60) >> 0xC);
            coord->coord.t[2]   = (rsin(angle) * mem->move.vx) >> 0xC;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            if (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
                newState = 4;
                goto set_state;
            }
            if (mem->step < 0x80) {
                newState = 4;
                goto set_state;
            }
            if (Gp_StateC08.effectPhase == state) {
                newState = 4;
                goto set_state;
            }
            break;
        case 3:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[1]  += 0x40;
            actorRenderComposeCoord(coord);
            if (mem->period < 0xB) {
                newState = 4;
                goto set_state;
            }
            mem->period = mem->period - 0xA;
            break;
    }
    goto draw;
set_state_4:
    newState = 4;
set_state:
    arg0->state = newState;
draw:
    func_800EB6E8(coord, mem->age, mem->angle | 0x1000,
                  mem->period | 0x1000);
    if (arg0->state == 4) {
        effectKillTask(mem, arg0);
    }
}

void Gp_EffCtlTaskAE(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    GfxCoord*   parent;
    MATRIX*     m;
    s32         state;
    s32         one;
    s32         pan;
    s16         temp;
    u8          rgb[3];

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    state = arg0->state;
    switch (state) {
        case 0:
            parent =
                (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
            one                  = ONE;
            *(s32*)&coord->coord = one;
            coord->parent        = parent + 12;
            m                    = &coord->coord;
            MATRIX_PAIR(m, 0, 2) = 0;
            MATRIX_PAIR(m, 1, 1) = one;
            MATRIX_PAIR(m, 2, 0) = 0;
            m->m[2][2]           = one;
            coord->coord.t[0]    = 0;
            coord->coord.t[1]    = 0;
            coord->coord.t[2]    = 0;
            coord->composeStamp  = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            arg0->state = 1;
            mem->scale  = 0;
            mem->angle  = 0x40;
            mem->step   = 0x100 / arg0->spawnArg1.value;
            if (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
                goto kill;
            }
            if (Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_CANCELLED) {
                goto kill;
            }
            if (gRoomEffectState->battleState != ROOM_EFFECT_BATTLE_ENGAGED) {
                goto kill;
            }
            arg0->spawnArg1.value = D_80112B94[((u16)(Gp_StateC08.attachId / 100U) - 1) * 9 +
                                               ((u16)((u16)(Gp_StateC08.attachId / 10U) % 10U) - 1) * 3 +
                                               ((u16)(Gp_StateC08.attachId % 10U) - 1U)];
            pan                   = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(arg0->spawnArg1.value, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            return;
        case 1:
            actorRenderComposeCoord(coord);
            temp       = mem->scale + mem->step;
            mem->scale = temp;
            if (temp >= 0x100) {
                mem->scale = 0xFF;
            }
            temp       = mem->angle + 8;
            mem->angle = temp;
            if (temp >= 0x201) {
                mem->angle = 0x200;
            }
            rgb[0] = mem->scale;
            rgb[1] = mem->scale >> 1;
            rgb[2] = mem->scale >> 2;
            Gp_DrawRing(coord, mem->angle, rgb);
            Gp_DrawRing(coord, (s16)(mem->angle << 1), rgb);
            if (mem->scale >= 0x81) {
                temp        = (mem->step << 1) + mem->period;
                mem->period = temp;
                if (temp >= 0x100) {
                    mem->period = 0xFF;
                }
                rgb[0] = mem->period;
                rgb[1] = mem->period >> 1;
                rgb[2] = mem->period >> 2;
                Gp_DrawArc(coord, ((u8)Gp_StateC08.duration << 24) >> 17, 0x60, rgb);
            }
            if (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
                goto snd7;
            }
            if (Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_CANCELLED) {
                goto snd7;
            }
            if (gRoomEffectState->battleState != ROOM_EFFECT_BATTLE_ENGAGED) {
                goto snd7;
            }
            if (Gp_StateC08.duration != 0) {
                return;
            }
            mem->scale  = 0xFF;
            arg0->state = 2;
            return;
        case 2:
            actorRenderComposeCoord(coord);
            mem->age++;
            if (mem->angle <= 0) {
                goto kill;
            }
            rgb[0] = mem->scale;
            rgb[1] = mem->scale >> 1;
            rgb[2] = mem->scale >> 2;
            Gp_DrawRing(coord, mem->angle, rgb);
            Gp_DrawRing(coord, (s16)(mem->angle << 1), rgb);
            if (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
                goto snd7;
            }
            if (Gp_StateC08.effectPhase == state) {
                goto snd7;
            }
            if (gRoomEffectState->battleState == ROOM_EFFECT_BATTLE_ENGAGED) {
                goto decay;
            }
        snd7:
            SndEvt_EnqueueType7(arg0->spawnArg1.value, 1);
            arg0->state = 3;
            return;
        decay:
            mem->scale = mem->scale - 0x10;
            mem->angle = mem->angle - 0x30;
            return;
        case 3:
            actorRenderComposeCoord(coord);
            if (mem->scale < 0x11) {
                goto kill;
            }
            rgb[0] = mem->scale;
            rgb[1] = mem->scale >> 1;
            rgb[2] = mem->scale >> 2;
            Gp_DrawRing(coord, mem->angle, rgb);
            Gp_DrawRing(coord, (s16)(mem->angle << 1), rgb);
            mem->scale = mem->scale - 0x10;
            return;
    }
    return;
kill:
    effectKillTask(mem, arg0);
}

void Gp_EffCtlTaskC1(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s16         flag;
    s32         idx;
    u8          rgb[3];
    s32         scale;
    s32         angle;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->peEffectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
        if (flag >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(mem, arg0);
        }
        return;
    }

    if (arg0->state == 0) {
        gfxRotMatrixZ(&coord->coord, arg0->spawnArg1.value & 0xFFF, GRAPHICS_ROTATION_COMPOSE);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        mem->scale          = 0x80;
        mem->angle          = 0x100;
        idx                 = arg0->spawnArg1.halves.high;
        mem->period         = D_80112C6C[idx & 3];
        arg0->state         = 1;
    }

    actorRenderComposeCoord(coord);
    rgb[0] = (mem->scale * ((mem->period >> 8) & 0xF)) >> 3;
    rgb[1] = (mem->scale * ((u8)mem->period >> 4)) >> 3;
    rgb[2] = (mem->scale * (mem->period & 0xF)) >> 3;
    Gp_DrawBandEx(coord, mem->angle, 0x100, rgb);

    angle      = (u16)mem->angle;
    scale      = (u16)mem->scale;
    angle     += 0x80;
    scale     -= 8;
    mem->scale = scale;
    mem->angle = angle;
    if ((s16)scale < 9) {
        effectKillTask(mem, arg0);
    }
}

void Gp_EffCtlTaskF3(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    GfxCoord*   parent;
    Task*       slot;
    u8          rgb[3];

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (gRoomEffectState->peEffectControl != ROOM_EFFECT_CONTROL_RUNNING ||
        ((gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(mem, arg0);
        }
        return;
    }

    mem->age++;
    if (arg0->state == 0) {
        gRoomEffectState->peFxFlags |= ROOM_EFFECT_PE_ENERGY_SHOT_AURA;
        slot                         = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        parent                       = slot->extra.tmd->coords;
        coord->coord.t[0]            = 0;
        coord->coord.t[1]            = 0;
        coord->coord.t[2]            = 0;
        coord->composeStamp          = GRAPHICS_COORD_DIRTY;
        coord->parent                = parent + 8;
        arg0->state                  = 1;
        mem->index                   = (Gp_StateC08.attachId % 10U) - 1;
        mem->angle                   = 0x20;
        mem->period                  = mem->index * 128 + 0x180;
        mem->step                    = mem->index * 256 + 0x400;
    }

    actorRenderComposeCoord(coord);
    if (gRoomEffectState->burstRequest != 0) {
        rgb[2] = 0xC0;
        rgb[0] = 0xC0;
        rgb[1] = 0x60;
        Gp_DrawEffTri(coord, (s16)(mem->period + 0x80), (s16)(mem->index + 6), rgb);
        Gp_DrawRing(coord, mem->period, rgb);
        Gp_DrawRing(coord, (s16)(mem->period << 1), rgb);
        gRoomEffectState->burstRequest = false;
    }

    if (Gp_StateC08.energyShotTicks == 0 || !(gRoomEffectState->peFxFlags & ROOM_EFFECT_PE_ENERGY_SHOT_AURA) ||
        gRoomEffectState->battleState != ROOM_EFFECT_BATTLE_ENGAGED) {
        effectKillTask(mem, arg0);
        return;
    }

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    if ((gRandomLcgState >> 16) & 3) {
        return;
    }
    slot            = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    Gp_SpawnEff(EFFECT_RISING_ENERGY_SPARK,
                &slot->extra.tmd->coords[((gRandomLcgState >> 16) & 1) * 3 + 15],
                mem->step | 0x8000, 0);
}

static void Gp_DrawEffTri(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    EffectCentreScratch* block;
    POLY_G3*             prim;
    s16                  step;
    s32                  i;
    s32                  lcg;
    s32                  ang;
    s16                  scale;
    s16                  count;

    SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
    block                = SCRATCH_STACK_CURSOR(EffectCentreScratch);
    block->worldPoint.vx = arg0->workm.t[0];
    block->worldPoint.vy = arg0->workm.t[1];
    block->worldPoint.vz = arg0->workm.t[2];
    count                = arg2;
    step                 = 0x1000 / count;
    scale                = arg1;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        block->depth++;
        for (i = 0; i < step * count; i += step) {
            lcg             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = lcg;
            prim            = gGpuPrimCursor;
            gGpuPrimCursor  = prim + 1;
            setPolyG3(prim);
            setRGB0(prim, rgb[0], rgb[1], rgb[2]);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, 0, 0);
            block->screenExtent = (scale * 128) / block->depth;
            ang                 = (s16)(i + (s32)((u32)lcg >> 16) % step);
            prim->x0            = block->screenX;
            prim->y0            = block->screenY;
            prim->x1            = block->screenX + ((block->screenExtent * rsin(ang - 0x28)) >> 12);
            prim->y1            = block->screenY + ((block->screenExtent * rcos(ang - 0x28)) >> 12);
            prim->x2            = block->screenX + ((block->screenExtent * rsin(ang + 0x28)) >> 12);
            prim->y2            = block->screenY + ((block->screenExtent * rcos(ang + 0x28)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

void Gp_EffCtlTaskF4(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    Task*       slot;
    s16         flag;
    s32         y;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->peEffectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
        if (flag >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            goto kill;
        }
        slot = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        if (slot->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) {
            return;
        }
        actorRenderComposeCoord(coord);
        goto draw_lcg;
    }

    mem->age++;
    if (arg0->state == 0) {
        mem->move.vx    = 0;
        mem->move.vz    = 0;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->move.vy    = 0xFFF0 - ((gRandomLcgState >> 16) & 0x3F);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->scale      = (gRandomLcgState >> 16) & 0xFFF;
        mem->angle      = arg0->spawnArg1.halves.low & 0xFFF;
        arg0->state     = 1;
        mem->period     = arg0->spawnArg1.halves.low & 0xF000;
    }

    y                   = coord->coord.t[1] + mem->move.vy;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[1]   = y;
    actorRenderComposeCoord(coord);
    if ((mem->age & 3) == 0) {
        mem->index++;
    }
    if (mem->index >= 8) {
        goto kill;
    }
    if (mem->period & 0x8000) {
    draw_lcg:
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        Gp_DrawFxQuad(coord, mem->index, mem->angle,
                      mem->scale | ((gRandomLcgState >> 16) & 0x1000));
    } else {
        Gp_DrawFxQuad(coord, mem->index, mem->angle,
                      mem->scale | mem->period);
    }
    return;
kill:
    effectKillTask(mem, arg0);
}

void Gp_EffCtlTaskAC(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    GfxCoord*   parent;
    EffectWork* spawned;
    Task*       slot;
    u8          rgb[3];
    u8          col;
    s32         saved;
    s32         temp;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (gRoomEffectState->peEffectControl != ROOM_EFFECT_CONTROL_RUNNING ||
        ((gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (gRoomEffectState->peEffectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        goto kill;
    }

    mem->age++;
    if (arg0->state == 0) {
        gRoomEffectState->peFxFlags |= ROOM_EFFECT_PE_ANTIBODY_AURA;
        slot                         = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        parent                       = slot->extra.tmd->coords;
        coord->coord.t[0]            = 0;
        coord->coord.t[1]            = 0;
        coord->coord.t[2]            = 0;
        coord->composeStamp          = GRAPHICS_COORD_DIRTY;
        coord->parent                = parent + 1;
        arg0->state                  = 1;
        mem->index                   = (Gp_StateC08.attachId % 10U) - 1;
        mem->angle                   = 0x20;
        mem->period                  = ((mem->index + 1) * 3) << 7;
        mem->step                    = gPlayerStatus.hp;
    }

    actorRenderComposeCoord(coord);
    mem->scale = mem->angle + ((mem->age & 1) << 4);
    col        = mem->scale;
    rgb[1]     = col;
    rgb[0]     = col;
    rgb[2]     = mem->scale >> 1;
    Gp_DrawRing(coord, mem->period, rgb);
    Gp_DrawRing(coord, (s16)(mem->period << 1), rgb);

    if (Gp_StateC08.antibodyTicks == 0) {
        goto kill;
    }
    if (!(gRoomEffectState->peFxFlags & ROOM_EFFECT_PE_ANTIBODY_AURA)) {
        goto kill;
    }
    if (gRoomEffectState->battleState == ROOM_EFFECT_BATTLE_ENGAGED) {
        goto continue_fx;
    }
kill:
    SndEvt_EnqueueType7(SOUND_ANTIBODY_AURA_LOOP, 1);
    effectKillTask(mem, arg0);
    return;
continue_fx:
    saved = mem->step;
    if (gPlayerStatus.hp < saved) {
        if (!(gPlayerStatus.statusFlags & (PLAYER_STATUS_BERSERKER | PLAYER_STATUS_POISON)) && (mem->angle < 0xA0)) {
            s32 i;

            Gp_DrawEffTri(coord, 0x200, 6, rgb);
            mem->angle = 0xC0;
            for (i = 0; i < 0x555; i += 0x2AA) {
                spawned = Gp_SpawnEff(EFFECT_EXPANDING_COLOR_BAND, coord, i, 0);
                if (spawned != NULL) {
                    taskReparent(arg0, spawned->task);
                }
            }
            temp = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(SOUND_ANTIBODY_AURA_HIT, temp, (s8)worldCoordGetOriginAudioDepth(coord));
        } else if (mem->angle < 0x80) {
            mem->angle = 0x80;
        }
    } else {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (((gRandomLcgState >> 16) & 3) == 0) {
            slot            = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            Gp_SpawnEff(EFFECT_FLASH_BURST,
                        &slot->extra.tmd->coords[((gRandomLcgState >> 16) & 0xF) + 3],
                        0x10080, 0);
        }
    }

    mem->step = (u16)gPlayerStatus.hp;
    if (mem->angle < 0x21) {
        return;
    }
    mem->angle      = mem->angle - 8;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    if ((gRandomLcgState >> 16) & 1) {
        return;
    }
    slot            = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    Gp_SpawnEff(EFFECT_FLASH_BURST,
                &slot->extra.tmd->coords[((gRandomLcgState >> 16) & 0xF) + 3],
                0x10200, 0);
}

void Gp_EffCtlTask0E(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    GfxCoord*   parent;
    MATRIX*     m;
    Task*       slot;
    s16         flag;
    s32         one;
    u8          rgb[3];

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
        if (flag >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(mem, arg0);
        }
        return;
    }

    mem->age++;
    if (arg0->state == 0) {
        gRoomEffectState->peFxFlags |= ROOM_EFFECT_PE_STATUS_BURST;
        slot                         = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        parent                       = slot->extra.tmd->coords;
        one                          = ONE;
        *(s32*)&coord->coord         = one;
        coord->parent                = parent + 8;
        m                            = &coord->coord;
        MATRIX_PAIR(m, 0, 2)         = 0;
        MATRIX_PAIR(m, 1, 1)         = one;
        MATRIX_PAIR(m, 2, 0)         = 0;
        m->m[2][2]                   = one;
        coord->coord.t[0]            = 0;
        coord->coord.t[1]            = 0;
        coord->coord.t[2]            = 0;
        coord->composeStamp          = GRAPHICS_COORD_DIRTY;
        arg0->state                  = 1;
    }

    actorRenderComposeCoord(coord);
    if (gRoomEffectState->burstRequest != 0) {
        rgb[0] = 0xC0;
        rgb[1] = 0x30;
        rgb[2] = 0x60;
        Gp_DrawEffTri(coord, 0x200, 4, rgb);
        Gp_DrawRing(coord, 0x180, rgb);
        Gp_DrawRing(coord, 0x300, rgb);
        gRoomEffectState->burstRequest = false;
    }

    if ((gPlayerStatus.statusFlags & PLAYER_STATUS_BERSERKER) && (gRoomEffectState->peFxFlags & ROOM_EFFECT_PE_STATUS_BURST) &&
        (gRoomEffectState->battleState == ROOM_EFFECT_BATTLE_ENGAGED)) {
        return;
    }
    gRoomEffectState->screenFxFlags &= ~ROOM_EFFECT_SCREEN_BURST_GUARD;
    effectKillTask(mem, arg0);
}

void Gp_PulseState1C80(void)
{
    gRoomEffectState->pendingCancelFlags |= ROOM_EFFECT_CANCEL_PE;
}

static void Gp_EffTask07State0(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

void Gp_EffCtlTask07(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = Gp_EffTask07States;
    sp.funcs[arg0->state](arg0);
}

void Gp_EffCtlTaskA5(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s16         flag;
    s32         i;
    s32         temp;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
        if (flag >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            SndEvt_EnqueueType7(0xFF0D, 1);
            gRoomEffectState->rumbleCount = 0;
            effectKillTask(mem, arg0);
        }
        return;
    }

    actorRenderComposeCoord(coord);
    switch (arg0->state) {
        case 0:
            if (gRoomEffectState->rumbleCount == 0) {
                temp = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(SOUND_COMMON(0x0D), temp, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            gRoomEffectState->rumbleCount++;
            arg0->state = 1;
            /* fallthrough */
        case 1:
            if (arg0->spawnArg1.value == 0) {
                Gp_SpawnEff(EFFECT_DEATH_FLAME, coord, 1, 0);
                arg0->state = 2;
            } else if (mem->scale == 0) {
                for (i = 0; i < 3; i++) {
                    Gp_SpawnEff(EFFECT_DEATH_FLAME, coord, arg0->spawnArg1.value, 0);
                }
                mem->scale++;
            } else {
                mem->angle++;
                if (mem->angle >= 9) {
                    mem->scale = 0;
                    mem->angle = 0;
                    mem->index++;
                    if (mem->index >= arg0->spawnArg1.value) {
                        arg0->state = 2;
                    }
                }
            }
            break;
        case 2:
            mem->age++;
            if (mem->age >= 0x65) {
                gRoomEffectState->rumbleCount--;
                if (gRoomEffectState->rumbleCount <= 0) {
                    SndEvt_EnqueueType7(0xFF0D, 1);
                    gRoomEffectState->rumbleCount = 0;
                }
                effectKillTask(mem, arg0);
            }
            break;
    }
}

void Gp_EffCtlTaskA6(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    SVECTOR*    in;
    SVECTOR*    out;
    s16         flag;
    s32         temp;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        switch (arg0->state) {
            case 0:
                mem->age++;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->scale      = gRandomLcgState >> 16;
                mem->angle      = (mem->scale & 0xF) + 8;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                temp            = arg0->spawnArg1.value;
                mem->period     = -(temp << 4) - ((gRandomLcgState >> 16) & 0x7F);
                mem->step       = arg0->spawnArg1.value * 24 + 0xC0;
                gfxRotMatrixY(&coord->coord, mem->scale & 0xFF0, 1);
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                arg0->state = 1;
                mem->move.vz =
                    (mem->scale & 0x1F) % (arg0->spawnArg1.value * 3) + 7;
                func_800FCD00(arg0);
                Gp_SpawnEff(EFFECT_RISING_WISP, coord, mem->step * 3 + 0x3000, 0);
                return;
            case 1:
                if (gSceneCombatState.actorControl != SCENE_COMBAT_ACTORS_PAUSED) {
                    actorRenderComposeCoord(coord);
                    mem->age++;
                    if (mem->move.vz != 0) {
                        in            = &mem->move;
                        out           = &mem->pos;
                        mem->move.vz -= (mem->age & 3) / 3;
                        gte_SetRotMatrix(&coord->coord);
                        gte_ldv0(in);
                        gte_rtv0();
                        gte_stsv(out);
                        coord->coord.t[0]  += mem->pos.vx;
                        coord->coord.t[1]  += mem->pos.vy;
                        coord->coord.t[2]  += mem->pos.vz;
                        coord->composeStamp = GRAPHICS_COORD_DIRTY;
                    }
                    if (mem->age >= 0x81) {
                        arg0->state = 2;
                    }
                }
                goto do_fcd00;
            case 2:
                if (gSceneCombatState.actorControl != SCENE_COMBAT_ACTORS_PAUSED) {
                    actorRenderComposeCoord(coord);
                    mem->age++;
                    mem->angle  -= mem->age & 1;
                    mem->period += 2;
                    mem->step   += 2;
                    if (mem->angle <= 0) {
                        goto kill;
                    }
                    if (mem->period < 0) {
                        goto do_fcd00;
                    }
                    goto kill;
                }
                goto do_fcd00;
        }
        return;
    }
kill:
    effectKillTask(mem, arg0);
    return;
do_fcd00:
    func_800FCD00(arg0);
}

static void func_800FCD00(Task* arg0)
{
    EffectWork*               mem;
    GfxCoord*                 coord;
    _EffectDeathFlameScratch* block;
    POLY_F4*                  prim;
    u16                       y;
    u8                        r;
    u8                        g;
    u8                        b;
    u16                       rad;
    s16                       outer;
    s16                       inner;
    s16                       bright;
    s32                       heightSum;
    s32                       rawBright;
    s32                       sum;
    s32                       i;
    s32                       a;
    s32                       c;

    mem       = arg0->spawnArg2.pointer;
    coord     = arg0->extra.coordBody->coord;
    heightSum = (u16)mem->period + (rsin(mem->age << 6) >> 8);
    y         = heightSum;
    rad       = mem->step;
    sum       = (u8)gDisplayState.animFrame + (u8)mem->scale;
    rawBright = mem->angle;
    bright    = rawBright;
    inner     = rad - 0x40;
    if (sum & 0x10) {
        bright += 0xF;
        bright -= sum & 0xF;
    } else {
        bright += sum & 0xF;
    }
    r     = bright;
    g     = r >> 1;
    b     = r >> 2;
    outer = rad;
    block = SCRATCH_STACK_RESERVE_BLOCK(_EffectDeathFlameScratch);
    gte_SetTransMatrix(&GsWSMATRIX);

    i = 0;
    do {
        a                     = i * 0x155;
        c                     = rcos(a);
        block->vertices[i].vy = 0;
        block->vertices[i].vx = (c * outer) >> 12;
        block->vertices[i].vz = (rsin(a) * outer) >> 12;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->vertices[i]);
        gte_rtv0();
        gte_stsv(&block->vertices[i]);
        (u16) block->vertices[i].vx = (u16)block->vertices[i].vx + (u16)coord->workm.t[0];
        a                          += 0x155;
        (u16) block->vertices[i].vy = (u16)block->vertices[i].vy + (u16)coord->workm.t[1];
        (u16) block->vertices[i].vz = (u16)block->vertices[i].vz + (u16)coord->workm.t[2];
        c                           = rcos(a);
        i++;
        block->vertices[i].vy = y;
        block->vertices[i].vx = (c * inner) >> 12;
        block->vertices[i].vz = (rsin(a) * inner) >> 12;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->vertices[i]);
        gte_rtv0();
        gte_stsv(&block->vertices[i]);
        (u16) block->vertices[i].vx = (u16)block->vertices[i].vx + (u16)coord->workm.t[0];
        (u16) block->vertices[i].vy = (u16)block->vertices[i].vy + (u16)coord->workm.t[1];
        (u16) block->vertices[i].vz = (u16)block->vertices[i].vz + (u16)coord->workm.t[2];
        i++;
    } while (i < EFFECT_DEATH_FLAME_VERTEX_COUNT);

    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < EFFECT_DEATH_FLAME_VERTEX_COUNT; i += 2) {
        gte_ldv0(&block->vertices[i]);
        gte_rtps();
        gte_stsxy(&block->screenCorners[0]);
        gte_ldv3(&block->vertices[i + 1], &block->vertices[(i + 2) % EFFECT_DEATH_FLAME_VERTEX_COUNT], &block->vertices[(i + 3) % EFFECT_DEATH_FLAME_VERTEX_COUNT]);
        gte_rtpt();
        gte_stsxy3(&block->screenCorners[1], &block->screenCorners[2], &block->screenCorners[3]);
        gte_stflg(&block->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&block->depth);
            block->depth   = block->depth + 1;
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 5);
            setcode(prim, 0x28);
            prim->r0 = r;
            prim->g0 = g;
            prim->b0 = b;
            prim->x0 = (u16)block->screenCorners[0].vx;
            c        = block->screenCorners[0].vy;
            prim->y0 = c;
            prim->x1 = (u16)block->screenCorners[1].vx;
            c        = block->screenCorners[1].vy;
            prim->y1 = c;
            prim->x2 = (u16)block->screenCorners[2].vx;
            c        = block->screenCorners[2].vy;
            prim->y2 = c;
            prim->x3 = (u16)block->screenCorners[3].vx;
            c        = block->screenCorners[3].vy;
            prim->y3 = c;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
        }
    }

    for (i = 1; i < EFFECT_DEATH_FLAME_VERTEX_COUNT; i += 6) {
        gte_ldv0(&block->vertices[i]);
        gte_rtps();
        gte_stsxy(&block->screenCorners[0]);
        gte_ldv3(&block->vertices[i + 2], &block->vertices[(i + 6) % EFFECT_DEATH_FLAME_VERTEX_COUNT], &block->vertices[i + 4]);
        gte_rtpt();
        gte_stsxy3(&block->screenCorners[1], &block->screenCorners[2], &block->screenCorners[3]);
        gte_stflg(&block->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&block->depth);
            block->depth   = block->depth + 1;
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 5);
            setcode(prim, 0x28);
            prim->r0 = r;
            prim->g0 = g;
            prim->b0 = b;
            prim->x0 = (u16)block->screenCorners[0].vx;
            c        = block->screenCorners[0].vy;
            prim->y0 = c;
            prim->x1 = (u16)block->screenCorners[1].vx;
            c        = block->screenCorners[1].vy;
            prim->y1 = c;
            prim->x2 = (u16)block->screenCorners[2].vx;
            c        = block->screenCorners[2].vy;
            prim->y2 = c;
            prim->x3 = (u16)block->screenCorners[3].vx;
            c        = block->screenCorners[3].vy;
            prim->y3 = c;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
        }
    }

    SCRATCH_STACK_RELEASE_BLOCK(_EffectDeathFlameScratch);
}

void Gp_EffSprTaskA7(Task* arg0)
{
    EffectWork*             mem;
    GfxCoord*               coord;
    GfxCoord*               parent;
    MATRIX*                 m;
    EffectBillboardScratch* block;
    POLY_FT4*               prim;
    s16                     flag;
    s32                     rng;
    s32                     one;
    s16                     n;
    s16                     step;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        effectKillTask(mem, arg0);
        return;
    }
    if (arg0->state == 0) {
        rng                  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->scale           = ((u32)rng >> 16) & 0xFFF;
        mem->angle           = arg0->spawnArg1.halves.low & 0xFFF;
        parent               = mem->parent;
        mem->move.vy         = -(mem->scale & 7);
        one                  = ONE;
        *(s32*)&coord->coord = one;
        coord->parent        = parent;
        m                    = &coord->coord;
        MATRIX_PAIR(m, 0, 2) = 0;
        MATRIX_PAIR(m, 1, 1) = one;
        MATRIX_PAIR(m, 2, 0) = 0;
        m->m[2][2]           = one;
        coord->coord.t[2]    = 0;
        coord->coord.t[1]    = 0;
        coord->coord.t[0]    = 0;
        coord->composeStamp  = GRAPHICS_COORD_DIRTY;
        gRandomLcgState      = rng;
        arg0->state++;
    }
    actorRenderComposeCoord(coord);
    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectBillboardScratch);
    block->worldPoint.vx = coord->workm.t[0];
    block->worldPoint.vy = coord->workm.t[1];
    block->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    n = (((s32)arg0->spawnArg1.value >> 12) & 3) + 1;
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        prim = gGpuPrimCursor;
        block->depth++;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        setRGB0(prim, 0x60, 0x60, 0x60);
        prim->tpage          = 0x28;
        prim->clut           = 0x4253;
        prim->code          |= 2;
        prim->u0             = (mem->age / n) << 5;
        prim->v0             = 0x18;
        prim->u1             = ((mem->age / n) << 5) + 0x1F;
        prim->v1             = 0x18;
        prim->u2             = (mem->age / n) << 5;
        prim->v2             = 0x37;
        prim->u3             = ((mem->age / n) << 5) + 0x1F;
        prim->v3             = 0x37;
        block->cornerOffsetX = (((mem->angle * 31) / block->depth) * rsin(mem->scale)) >> 12;
        block->cornerOffsetY = (((mem->angle * 31) / block->depth) * rcos(mem->scale)) >> 12;
        prim->x0             = block->screenX + block->cornerOffsetX;
        prim->x3             = block->screenX - block->cornerOffsetX;
        prim->y0             = block->screenY - block->cornerOffsetY;
        prim->y3             = block->screenY + block->cornerOffsetY;
        block->cornerOffsetX = (((mem->angle * 31) / block->depth) * rsin(mem->scale + 0x400)) >> 12;
        block->cornerOffsetY = (((mem->angle * 31) / block->depth) * rcos(mem->scale + 0x400)) >> 12;
        prim->x1             = block->screenX + block->cornerOffsetX;
        prim->x2             = block->screenX - block->cornerOffsetX;
        prim->y1             = block->screenY - block->cornerOffsetY;
        prim->y2             = block->screenY + block->cornerOffsetY;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectBillboardScratch);
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        return;
    }
    step                = mem->move.vy - (mem->age & 1);
    mem->move.vy        = step;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[1]  += step;
    mem->age++;
    if (mem->age > n * 8 - 1) {
        effectKillTask(mem, arg0);
    }
}

void func_800FDB18(s32 arg0, GfxCoord* arg1, SVECTOR* arg2, EffectSpawnArg* arg3)
{
    GameActor* actor;
    s32        i;
    s32        pan;
    s16        id;

    id    = arg0;
    actor = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    if (arg3 == NULL) {
        arg3        = &D_80112C74;
        arg3->coord = arg1;
    } else {
        if (arg3->coord == NULL) {
            if (arg1 == NULL) {
                arg3->coord = &gGfxViewCoord;
                arg1        = arg3->coord;
            } else {
                arg3->coord = arg1;
            }
        } else if (arg1 == NULL) {
            arg1 = arg3->coord;
        }
    }
    switch ((u16)id) {
        case 1:
            if (actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key & 0x4000) {
                Gp_SpawnEff(EFFECT_HIT_PUFF, arg1, 0x12300, arg2);
                if (arg3->spawnArgHi >= 2) {
                    for (i = 0; i < arg3->spawnArgHi; i++) {
                        Gp_SpawnEff(EFFECT_HIT_PUFF, arg1, 0x111280, arg2);
                    }
                }
            } else {
                Gp_SpawnEff(EFFECT_HIT_PUFF, arg1, 0x12380, arg2);
                Gp_SpawnEff(EFFECT_HIT_PUFF, arg1, 0x111300, arg2);
                Gp_SpawnEff(EFFECT_HIT_PUFF, arg1, 0x111300, arg2);
                for (i = 0; i < arg3->spawnArgHi; i++) {
                    Gp_SpawnEff(EFFECT_HIT_PUFF, arg1, 0x111280, arg2);
                }
            }
            break;
        case 2:
            Gp_SpawnEff(EFFECT_HIT_PUFF, arg1, 0x10013380, arg2);
            Gp_SpawnEff(EFFECT_HIT_PUFF, arg1, 0x10111300, arg2);
            Gp_SpawnEff(EFFECT_HIT_PUFF, arg1, 0x10111300, arg2);
            for (i = 0; i < arg3->spawnArgHi; i++) {
                Gp_SpawnEff(EFFECT_HIT_PUFF, arg1, 0x10112280, arg2);
            }
            break;
        case 4:
            Gp_SpawnEff(EFFECT_HIT_PARTICLE_EMITTER, arg3->coord, arg3->spawnArgLo | (arg3->spawnArgHi << 16), arg2);
            break;
        case 5:
            Gp_SpawnEff(EFFECT_HIT_SPLATTER_SPRAY, arg1, arg3->spawnArgLo | (arg3->spawnArgHi << 16), arg2);
            break;
        case 6:
            Gp_SpawnEff(EFFECT_IMPACT_SPARK, arg1, 0x400, arg2);
            for (i = 0; i < arg3->spawnArgHi; i++) {
                Gp_SpawnEff(EFFECT_HIT_PUFF, arg1, 0x112300, arg2);
            }
            break;
        case 7:
            Gp_SpawnEff(EFFECT_HIT_SPARK_BURST, arg3->coord, arg3->spawnArgLo | (arg3->spawnArgHi << 16), arg2);
            break;
        case 8:
            for (i = 0; i < arg3->spawnArgHi * 3; i++) {
                Gp_SpawnEff(EFFECT_HIT_PUFF, arg3->coord, 0x1112300, arg2);
            }
            break;
        case 9:
            Gp_SpawnEff(EFFECT_HIT_SPLATTER_SPRAY, arg1, arg3->spawnArgLo | (arg3->spawnArgHi << 16), arg2);
            break;
        case 10:
            Gp_SpawnEff(EFFECT_0E3, arg1, arg3->spawnArgLo | 0x10000, arg2);
            break;
        case 11:
            Gp_SpawnEff(EFFECT_HIT_BLAST, arg3->coord, arg3->spawnArgLo | (arg3->spawnArgHi << 16), NULL);
            pan = (s8)worldCoordGetOriginAudioPan(arg1);
            sndEvtRequestScriptStart(D_80112C7C[(u16)(Gp_StateC08.attachId % 10U) - 1], pan,
                                     (s8)worldCoordGetOriginAudioDepth(arg1));
            break;
        case 12:
            Gp_SpawnEff(EFFECT_APOBIOSIS_SHARD, arg1, 1, NULL);
            break;
        case 13:
            for (i = 0; i < arg3->spawnArgHi; i++) {
                Gp_SpawnEff((EFFECT_LIFE_DRAIN_MOTE | EFFECT_SPAWN_UNLIMITED), arg1, 1, NULL);
            }
            break;
        case 15:
            Gp_SpawnEff(EFFECT_HIT_SPARK_BURST, arg3->coord, arg3->spawnArgLo | (arg3->spawnArgHi << 16), arg2);
            Gp_SpawnEff(EFFECT_M4A1_HAMMER_IMPACT_FLASH, arg1, 1, NULL);
            break;
        case 16:
            if (actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key & 0x4000) {
                Gp_SpawnEff(EFFECT_HIT_BLAST, arg3->coord, arg3->spawnArgLo | 0x10000, arg2);
            } else {
                case 3:
                    Gp_SpawnEff(EFFECT_HIT_BLAST, arg3->coord, arg3->spawnArgLo | (arg3->spawnArgHi << 16), arg2);
            }
            break;
    }
}

/// Four-entry `Task::state` dispatcher: `Gp_InitPlayerWork`, `Gp_PlayerWorkState1`,
/// `Gp_PlayerWorkState2`, `Gp_TeardownSlot0`.
static const TaskFuncTable4 Gp_PlayerWorkStates = { {
    Gp_InitPlayerWork,
    Gp_PlayerWorkState1,
    Gp_PlayerWorkState2,
    Gp_TeardownSlot0,
} };

void Gp_EffCtlTask7F(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    GfxCoord*   parent;
    MATRIX*     m;
    s16         flag;
    s16         step;
    s32         temp;
    s32         one;
    s32         span;
    s16         divisor;
    s16         half;
    s32         count;
    s32         i;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        goto release;
    }
    if (arg0->state == 0) {
        parent               = mem->parent;
        one                  = ONE;
        *(s32*)&coord->coord = one;
        coord->parent        = parent;
        m                    = &coord->coord;
        MATRIX_PAIR(m, 0, 2) = 0;
        MATRIX_PAIR(m, 1, 1) = one;
        MATRIX_PAIR(m, 2, 0) = 0;
        m->m[2][2]           = one;
        coord->coord.t[0]    = mem->pos.vx;
        coord->coord.t[1]    = mem->pos.vy;
        coord->coord.t[2]    = mem->pos.vz;
        coord->composeStamp  = GRAPHICS_COORD_DIRTY;
        arg0->state          = 1;
        mem->scale           = arg0->spawnArg1.halves.low;
        temp                 = arg0->spawnArg1.halves.high;
        step                 = temp;
        mem->index           = temp;
        if (step != 1) {
            step = step * 3;
        } else {
            step = 1;
        }
        mem->angle  = step;
        mem->period = step * 2;
        mem->step   = mem->scale / 1280;
    }
    actorRenderComposeCoord(coord);
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        return;
    }
    if (mem->age >= mem->period) {
    release:
        effectKillTask(mem, arg0);
        return;
    }
    span            = mem->scale >> 1;
    half            = (u32)span >> 1;
    divisor         = span;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    if ((gRandomLcgState >> 16) & 1) {
        count = mem->step;
    } else {
        count = 1;
    }
    for (i = 0; i < count; i++) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->move.vx    = ((s32)(gRandomLcgState >> 16) % divisor) - half;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->move.vy    = (s32)(gRandomLcgState >> 16) % divisor;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->move.vz    = ((s32)(gRandomLcgState >> 16) % divisor) - half;
        if (mem->age < mem->angle) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (mem->age < (s32)(gRandomLcgState >> 16) % mem->period) {
                Gp_SpawnEff(EFFECT_ADDITIVE_PUFF, coord, (mem->move.vx & 0x10000) | 0x300,
                            &mem->move);
            } else {
                Gp_SpawnEff(EFFECT_FIRE_BURST, coord, 0x300, &mem->move);
            }
        } else {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, (mem->scale >> 2) + 0xC0013200,
                            &mem->move);
            }
        }
    }
    mem->age++;
}

void Gp_EffCtlTaskE3(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s16         flag;
    s32         temp;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        effectKillTask(mem, arg0);
    } else {
        if (arg0->state == 0) {
            coord->parent       = mem->parent;
            coord->coord.t[0]   = mem->pos.vx;
            coord->coord.t[1]   = mem->pos.vy;
            coord->coord.t[2]   = mem->pos.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            arg0->state         = 1;
            mem->scale          = arg0->spawnArg1.halves.low;
            temp                = arg0->spawnArg1.halves.high;
            mem->angle          = temp;
            mem->period         = temp << 2;
        }
        actorRenderComposeCoord(coord);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        mem->age++;
        if (mem->age >= mem->period) {
            effectKillTask(mem, arg0);
        } else {
            Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, (mem->scale >> 2) + 0x80021400, &mem->move);
        }
    }
}

void Gp_EffSprTask80(Task* arg0)
{
    EffectCentreScratch* block;
    GfxCoord*            coord;
    EffectWork*          mem;
    POLY_FT4*            prim;
    s16                  x;
    s16                  y;
    s32                  amt;
    s32                  t;
    u16                  uv;
    s32                  scale;
    s32                  c;
    u32                  rnd;
    s32                  flag2;
    u8*                  head;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        head                       = SCRATCH_STACK_CURSOR(u8) - sizeof(EffectCentreScratch);
        SCRATCH_STACK_CURSOR(void) = head;
        block                      = (EffectCentreScratch*)head;
        if (arg0->state == 0) {
            t   = (u16)arg0->spawnArg1.value & 0xFFF;
            amt = 0x200;
            if (t != 0) {
                amt = t;
            }
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->scale      = amt;
            mem->angle      = (gRandomLcgState >> 16) % 12 + 12;
            flag2           = arg0->spawnArg1.halves.high;
            if (flag2 & 1) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                rnd             = gRandomLcgState >> 16;
                rnd             = rnd % 40;
            } else {
                rnd = 0;
            }
            mem->step = rnd;
            arg0->state++;
            arg0->spawnArg1.value &= 0x80000000;
        }
        actorRenderComposeCoord(coord);
        block->worldPoint.vx = (u16)coord->workm.t[0];
        block->worldPoint.vy = (u16)coord->workm.t[1];
        block->worldPoint.vz = (u16)coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->worldPoint);
        gte_rtps();
        gte_stsxy(&block->screenX);
        gte_stflg(&block->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&block->depth);
            block->depth++;
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2C);
            if (mem->age < 0xC) {
                scale = mem->scale * mem->age / 12;
            } else {
                scale = (u16)mem->scale;
            }
            mem->period = scale;
            if (mem->angle - 8 < mem->age) {
                c = (mem->angle - mem->age + 1) * 0x10;
                setRGB0(prim, c, c, c);
            } else {
                prim->code |= 1;
            }
            prim->tpage         = 0x29;
            prim->clut          = 0x4282;
            prim->code         |= 2;
            uv                  = mem->age;
            prim->v0            = 0x98;
            prim->u0            = (s16)((s16)uv % 6) * 0x20;
            uv                  = mem->age;
            prim->v1            = 0x98;
            prim->u1            = ((s16)((s16)uv % 6) * 0x20) + 0x1F;
            uv                  = mem->age;
            prim->v2            = 0xB7;
            prim->u2            = (s16)((s16)uv % 6) * 0x20;
            uv                  = mem->age;
            prim->v3            = 0xB7;
            prim->u3            = ((s16)((s16)uv % 6) * 0x20) + 0x1F;
            block->screenExtent = (mem->period * 0x1F) / block->depth;
            x                   = block->screenX - (u16)block->screenExtent;
            prim->x2            = x;
            prim->x0            = x;
            x                   = block->screenX + (u16)block->screenExtent;
            prim->x3            = x;
            prim->x1            = x;
            y                   = block->screenY - (u16)block->screenExtent;
            prim->y1            = y;
            prim->y0            = y;
            y                   = block->screenY + (u16)block->screenExtent;
            prim->y3            = y;
            prim->y2            = y;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        if (mem->step != 0) {
            coord->coord.t[1]  -= mem->step;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        }
        mem->age++;
        if (mem->angle >= mem->age) {
            return;
        }
    }
    effectKillTask(mem, arg0);
}

void Gp_EffSprTask8D(Task* arg0)
{
    EffectCentreScratch* block;
    GfxCoord*            coord;
    EffectWork*          mem;
    POLY_FT4*            prim;
    s16                  x;
    s16                  y;
    s32                  amt;
    s32                  t;
    u16                  uv;
    s32                  scale;
    s32                  c;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        actorRenderComposeCoord(coord);
        block                = SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
        block->worldPoint.vx = coord->workm.t[0];
        block->worldPoint.vy = coord->workm.t[1];
        block->worldPoint.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->worldPoint);
        gte_rtps();
        gte_stsxy(&block->screenX);
        gte_stflg(&block->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&block->depth);
            block->depth++;
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2C);
            if (arg0->state == 0) {
                t   = (u16)arg0->spawnArg1.value & 0xFFF;
                amt = 0x200;
                if (t != 0) {
                    amt = t;
                }
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->scale      = amt;
                mem->angle      = ((gRandomLcgState >> 16) & 7) + 0x10;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->step       = (gRandomLcgState >> 16) % 0x30;
                arg0->state++;
                arg0->spawnArg1.value &= 0x80000000;
            }
            if (mem->age < 0xC) {
                scale = mem->scale * mem->age / 12;
            } else {
                scale = (u16)mem->scale;
            }
            mem->period = scale;
            if (mem->angle - 8 < mem->age) {
                c = (mem->angle - mem->age + 1) * 0x10;
                setRGB0(prim, c, c, c);
            } else {
                prim->code |= 1;
            }
            prim->tpage         = 0x28;
            prim->clut          = 0x430D;
            prim->code         |= 2;
            uv                  = mem->age;
            prim->v0            = 0xA0;
            prim->u0            = (uv & 7) * 0x18;
            uv                  = mem->age;
            prim->v1            = 0xA0;
            prim->u1            = ((uv & 7) * 0x18) + 0x17;
            uv                  = mem->age;
            prim->v2            = 0xB7;
            prim->u2            = (uv & 7) * 0x18;
            uv                  = mem->age;
            prim->v3            = 0xB7;
            prim->u3            = ((uv & 7) * 0x18) + 0x17;
            block->screenExtent = (mem->period * 0x17) / block->depth;
            x                   = block->screenX - (u16)block->screenExtent;
            prim->x2            = x;
            prim->x0            = x;
            x                   = block->screenX + (u16)block->screenExtent;
            prim->x3            = x;
            prim->x1            = x;
            y                   = block->screenY - (u16)block->screenExtent;
            prim->y1            = y;
            prim->y0            = y;
            y                   = block->screenY + (u16)block->screenExtent;
            prim->y3            = y;
            prim->y2            = y;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        coord->coord.t[1]  -= mem->step;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        mem->age++;
        if (mem->angle >= mem->age) {
            return;
        }
    }
    effectKillTask(mem, arg0);
}

void Gp_EffSprTask3F(Task* arg0)
{
    EffectWork*         mem;
    GfxCoord*           coord;
    GfxRotationWords*   rot;
    EffectShapeScratch* block;
    POLY_FT4*           prim;
    s32                 sub;
    s32                 temp;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        actorRenderComposeCoord(coord);
        block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
        block->worldPoint.vx = coord->workm.t[0];
        block->worldPoint.vy = coord->workm.t[1];
        block->worldPoint.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->worldPoint);
        gte_rtps();
        gte_stsxy(&block->screenX);
        gte_stflg(&block->projectionFlags);
        if (block->projectionFlags >= 0) {
            if (arg0->state == 0) {
                mem->period     = ((arg0->spawnArg1.value >> 12) & 3) + 2;
                temp            = (u16)arg0->spawnArg1.value & 0xFFF;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->angle      = (gRandomLcgState >> 16) & 0xFFF;
                mem->scale      = temp;
                sub             = arg0->spawnArg1.halves.high;
                mem->index      = sub & 1;
                if (mem->index != 0) {
                    rot                 = (GfxRotationWords*)&coord->coord;
                    coord->parent       = mem->parent;
                    rot->m00M01         = ONE;
                    rot->m02M10         = 0;
                    rot->m11M12         = ONE;
                    rot->m20M21         = 0;
                    rot->m22            = ONE;
                    coord->coord.t[2]   = 0;
                    coord->coord.t[1]   = 0;
                    coord->coord.t[0]   = 0;
                    coord->composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(coord);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vy    = -((gRandomLcgState >> 16) & 3);
                } else {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vx    = ((gRandomLcgState >> 16) & 0xF) - 8;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vy    = -((gRandomLcgState >> 16) & 0xF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vz    = ((gRandomLcgState >> 16) & 0xF) - 8;
                }
                arg0->state++;
            }
            gte_stszotz(&block->depth);
            block->depth   = block->depth + 1;
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->tpage = 0x28;
            prim->clut  = 0x4253;
            setUV4(prim, (mem->age / mem->period) << 5, 0x18,
                   ((mem->age / mem->period) << 5) + 0x1F, 0x18,
                   (mem->age / mem->period) << 5, 0x37,
                   ((mem->age / mem->period) << 5) + 0x1F, 0x37);
            block->extent.corner.x = (((mem->scale * 0x1F) / block->depth) * rsin(mem->angle)) >> 12;
            block->extent.corner.y = (((mem->scale * 0x1F) / block->depth) * rcos(mem->angle)) >> 12;
            prim->x0               = block->screenX + (u16)block->extent.corner.x;
            prim->x3               = block->screenX - (u16)block->extent.corner.x;
            prim->y0               = block->screenY - (u16)block->extent.corner.y;
            prim->y3               = block->screenY + (u16)block->extent.corner.y;
            block->extent.corner.x = (((mem->scale * 0x1F) / block->depth) * rsin(mem->angle + 0x400)) >> 12;
            block->extent.corner.y = (((mem->scale * 0x1F) / block->depth) * rcos(mem->angle + 0x400)) >> 12;
            prim->x1               = block->screenX + (u16)block->extent.corner.x;
            prim->x2               = block->screenX - (u16)block->extent.corner.x;
            prim->y1               = block->screenY - (u16)block->extent.corner.y;
            prim->y2               = block->screenY + (u16)block->extent.corner.y;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        if (mem->index != 0) {
            coord->coord.t[1]  += mem->move.vy;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        } else {
            coord->coord.t[0]  += mem->move.vx;
            coord->coord.t[1]  += mem->move.vy;
            coord->coord.t[2]  += mem->move.vz;
            coord->coord.t[1]  -= (s16)(mem->scale / 736);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        }
        mem->age++;
        if (mem->age <= (mem->period * 8) - 1) {
            return;
        }
    }
    effectKillTask(mem, arg0);
}

void func_800FF710(Task* arg0)
{
    EffectWork*  mem;
    GfxCoord*    coord;
    MATRIX*      m;
    s16          flag;
    register s32 old asm("v1");
    register s32 k asm("a2");
    s32          lcg;
    register s32 one asm("v1");
    s32          temp;
    s32          temp2;
    s32          i;
    s32          half;
    s32          r3;
    s32          id;
    u16          v;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        effectKillTask(mem, arg0);
    } else {
        if (arg0->state == 0) {
            one = ONE;
            k   = RANDOM_LCG_INCREMENT & ~0xFFFF;
            /* Keep the LCG upper half ahead of the coordinate stores. */
            TOUCH_REG(k);
            m                    = &coord->coord;
            coord->parent        = mem->parent;
            *(s32*)&coord->coord = one;
            MATRIX_PAIR(m, 1, 1) = one;
            m->m[2][2]           = one;
            old                  = gRandomLcgState;
            k                   |= RANDOM_LCG_INCREMENT & 0xFFFF;
            MATRIX_PAIR(m, 0, 2) = 0;
            MATRIX_PAIR(m, 2, 0) = 0;
            coord->coord.t[0]    = mem->pos.vx;
            coord->coord.t[1]    = mem->pos.vy;
            coord->coord.t[2]    = mem->pos.vz;
            coord->composeStamp  = GRAPHICS_COORD_DIRTY;
            arg0->state          = 1;
            lcg                  = old * RANDOM_LCG_MULTIPLIER + k;
            mem->index           = ((u32)lcg >> 16) & 0xFFF;
            temp                 = arg0->spawnArg1.halves.low;
            gRandomLcgState      = lcg;
            mem->scale           = temp;
            temp2                = arg0->spawnArg1.halves.high;
            mem->step            = ((s16)temp >> 10) + 1;
            mem->angle           = temp2;
            mem->period          = temp2 << 2;
        }
        actorRenderComposeCoord(coord);
        Gp_DrawEffSpriteE2(coord, (u16)(mem->age >> 1), mem->scale - 0x40, mem->index);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        if (mem->age >= mem->period) {
            effectKillTask(mem, arg0);
            return;
        }
        v    = mem->scale;
        half = (s16)v >> 1;
        for (i = 0; i < mem->step; i++) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (mem->angle >= (s32)((gRandomLcgState >> 16) & 3)) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = (s32)(gRandomLcgState >> 16) % (s16)v - half;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vy    = (s32)(gRandomLcgState >> 16) % (s16)v - half;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vz    = (s32)(gRandomLcgState >> 16) % (s16)v - half;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                r3              = (s32)(gRandomLcgState >> 16) % mem->period;

                id = 0x600E1;
                if (mem->age < r3) {
                    id = 0x600E0;
                }
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                Gp_SpawnEff(id, coord, ((gRandomLcgState >> 16) & 0x1FF) + 0x80,
                            &mem->move);
            }
        }
        mem->age++;
    }
}

void Gp_EffSprTaskE0(Task* arg0)
{
    EffectWork*         mem;
    GfxCoord*           coord;
    EffectShapeScratch* head;
    EffectShapeScratch* block;
    EffectShapeScratch* projectionScratch;
    POLY_FT4*           prim;
    s16                 flag;
    s32                 temp;
    s32                 pal;
    s32                 t;
    s32                 uv;
    u16                 vz;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag < ROOM_EFFECT_CONTROL_HIDDEN) {
        actorRenderComposeCoord(coord);
        head                                     = SCRATCH_STACK_CURSOR(EffectShapeScratch);
        (head - 1)->worldPoint.vx                = (u16)coord->workm.t[0];
        block                                    = head - 1;
        block->worldPoint.vy                     = (u16)coord->workm.t[1];
        vz                                       = (u16)coord->workm.t[2];
        SCRATCH_STACK_CURSOR(EffectShapeScratch) = block;
        block->worldPoint.vz                     = vz;
        projectionScratch                        = block;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&projectionScratch->worldPoint);
        gte_rtps();
        gte_stsxy(&(head - 1)->screenX);
        gte_stflg(&(head - 1)->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&(head - 1)->depth);
            block->depth -= 0x20;
            if (block->depth < 0x10) {
                block->depth = 0x10;
            }
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2C);
            if (arg0->state == 0) {
                temp            = (u16)arg0->spawnArg1.value & 0xFFF;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->scale      = temp + ((gRandomLcgState >> 16) & 0xFF);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->angle      = (gRandomLcgState >> 16) & 0xFFF;
                pal             = arg0->spawnArg1.halves.high;
                mem->step       = pal;
                arg0->state     = 1;
            }
            prim->tpage            = 0x29;
            prim->code            |= 3;
            t                      = (((u16)mem->step + 0x10A) << 6) | ((mem->step * 6) & 0x3F);
            prim->clut             = t;
            uv                     = mem->age;
            t                      = 0x50;
            prim->v0               = t;
            prim->u0               = uv * 0x28;
            uv                     = mem->age;
            prim->v1               = t;
            prim->u1               = uv * 0x28 + 0x27;
            uv                     = mem->age;
            t                      = 0x77;
            prim->v2               = t;
            prim->u2               = uv * 0x28;
            uv                     = mem->age;
            prim->v3               = t;
            prim->u3               = uv * 0x28 + 0x27;
            block->extent.corner.x = (((mem->scale * 0x27) / block->depth) * rsin(mem->angle)) >> 12;
            block->extent.corner.y = (((mem->scale * 0x27) / block->depth) * rcos(mem->angle)) >> 12;
            prim->x0               = block->screenX + (u16)block->extent.corner.x;
            prim->x3               = block->screenX - (u16)block->extent.corner.x;
            prim->y0               = block->screenY - (u16)block->extent.corner.y;
            prim->y3               = block->screenY + (u16)block->extent.corner.y;
            block->extent.corner.x = (((mem->scale * 0x27) / block->depth) * rsin(mem->angle + 0x400)) >> 12;
            block->extent.corner.y = (((mem->scale * 0x27) / block->depth) * rcos(mem->angle + 0x400)) >> 12;
            prim->x1               = block->screenX + (u16)block->extent.corner.x;
            prim->x2               = block->screenX - (u16)block->extent.corner.x;
            prim->y1               = block->screenY - (u16)block->extent.corner.y;
            prim->y2               = block->screenY + (u16)block->extent.corner.y;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        mem->age++;
        if (mem->age < 6) {
            return;
        }
    } else if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        return;
    }
    effectKillTask(mem, arg0);
}

void Gp_EffSprTaskE1(Task* arg0)
{
    EffectWork*         mem;
    GfxCoord*           coord;
    EffectShapeScratch* head;
    EffectShapeScratch* block;
    EffectShapeScratch* projectionScratch;
    POLY_FT4*           prim;
    s16                 flag;
    s32                 temp;
    s32                 pal;
    s32                 t;
    s32                 uv;
    u16                 vz;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
    } else {
        head                                     = SCRATCH_STACK_CURSOR(EffectShapeScratch);
        block                                    = head - 1;
        SCRATCH_STACK_CURSOR(EffectShapeScratch) = block;
        projectionScratch                        = block;
        if (arg0->state == 0) {
            temp            = (u16)arg0->spawnArg1.value & 0xFFF;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->scale      = temp + ((gRandomLcgState >> 16) & 0xFF);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->angle      = (gRandomLcgState >> 16) & 0xFFF;
            pal             = arg0->spawnArg1.halves.high;
            mem->step       = pal;
            arg0->state++;
        }
        actorRenderComposeCoord(coord);
        (head - 1)->worldPoint.vx = (u16)coord->workm.t[0];
        block->worldPoint.vy      = (u16)coord->workm.t[1];
        vz                        = (u16)coord->workm.t[2];
        block->worldPoint.vz      = vz;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&projectionScratch->worldPoint);
        gte_rtps();
        gte_stsxy(&(head - 1)->screenX);
        gte_stflg(&(head - 1)->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&(head - 1)->depth);
            block->depth -= 0x20;
            if (block->depth < 0x10) {
                block->depth = 0x10;
            }
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->tpage            = 0x28;
            prim->clut             = ((0x10C - mem->step) << 6) | (((0xC0 - mem->step * 80) >> 4) & 0x3F);
            uv                     = mem->age;
            t                      = 0x88;
            prim->v0               = t;
            prim->u0               = uv * 0x18;
            uv                     = mem->age;
            prim->v1               = t;
            prim->u1               = uv * 0x18 + 0x17;
            uv                     = mem->age;
            t                      = 0x9F;
            prim->v2               = t;
            prim->u2               = uv * 0x18;
            uv                     = mem->age;
            prim->v3               = t;
            prim->u3               = uv * 0x18 + 0x17;
            block->extent.corner.x = (((mem->scale * 23) / block->depth) * rsin(mem->angle)) >> 12;
            block->extent.corner.y = (((mem->scale * 23) / block->depth) * rcos(mem->angle)) >> 12;
            prim->x0               = block->screenX + (u16)block->extent.corner.x;
            prim->x3               = block->screenX - (u16)block->extent.corner.x;
            prim->y0               = block->screenY - (u16)block->extent.corner.y;
            prim->y3               = block->screenY + (u16)block->extent.corner.y;
            block->extent.corner.x = (((mem->scale * 23) / block->depth) * rsin(mem->angle + 0x400)) >> 12;
            block->extent.corner.y = (((mem->scale * 23) / block->depth) * rcos(mem->angle + 0x400)) >> 12;
            prim->x1               = block->screenX + (u16)block->extent.corner.x;
            prim->x2               = block->screenX - (u16)block->extent.corner.x;
            prim->y1               = block->screenY - (u16)block->extent.corner.y;
            prim->y2               = block->screenY + (u16)block->extent.corner.y;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        mem->age++;
        if (mem->age < 8) {
            return;
        }
    }
    effectKillTask(mem, arg0);
}

void Gp_EffSprTaskE2(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    GfxCoord*   parent;
    MATRIX*     m;
    s16         flag;
    s32         one;
    s32         temp;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag < ROOM_EFFECT_CONTROL_HIDDEN) {
        if (arg0->state == 0) {
            if (arg0->spawnArg1.value < 0) {
                parent               = mem->parent;
                one                  = ONE;
                *(s32*)&coord->coord = one;
                coord->parent        = parent;
                m                    = &coord->coord;
                MATRIX_PAIR(m, 0, 2) = 0;
                MATRIX_PAIR(m, 1, 1) = one;
                MATRIX_PAIR(m, 2, 0) = 0;
                m->m[2][2]           = one;
                coord->coord.t[0]    = mem->pos.vx;
                coord->coord.t[1]    = mem->pos.vy;
                coord->coord.t[2]    = mem->pos.vz;
                coord->composeStamp  = GRAPHICS_COORD_DIRTY;
            }
            temp            = (u16)arg0->spawnArg1.value & 0xFFF;
            mem->step       = 0;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->scale      = temp + ((gRandomLcgState >> 16) & 0xFF);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->angle      = (gRandomLcgState >> 16) & 0xFFF;
            arg0->state++;
        }
        actorRenderComposeCoord(coord);
        if (!(mem->age & 1)) {
            Gp_DrawEffSpriteE2(coord, (u16)(mem->age >> 1),
                               (s16)(mem->scale | mem->step), mem->angle);
        }
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        mem->age++;
        if (mem->age < 0xC) {
            return;
        }
    } else if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        return;
    }
    effectKillTask(mem, arg0);
}

/// Hand-written GTE routine. `arg2` is a full 32-bit word: the high half picks
/// the CLUT (palette column) and the low 12 bits are the billboard size, so it
/// must not be declared `s16` (that makes callers emit a spurious `sll`/`sra`
/// truncation). It is unsigned because the size is divided by `otz` with `divu`.
static void Gp_DrawEffSpriteE2(GfxCoord* arg0, u16 arg1, u32 arg2, s16 arg3)
{
    EffectShapeScratch* head;
    EffectShapeScratch* block;
    EffectShapeScratch* projectionScratch;
    POLY_FT4*           prim;
    u32                 pal;
    s32                 ang;
    u16                 vz;

    head                                     = SCRATCH_STACK_CURSOR(EffectShapeScratch);
    (head - 1)->worldPoint.vx                = (u16)arg0->workm.t[0];
    block                                    = head - 1;
    block->worldPoint.vy                     = (u16)arg0->workm.t[1];
    vz                                       = (u16)arg0->workm.t[2];
    SCRATCH_STACK_CURSOR(EffectShapeScratch) = block;
    block->worldPoint.vz                     = vz;
    projectionScratch                        = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projectionScratch->worldPoint);
    gte_rtps();
    pal   = arg2 >> 16;
    arg2 &= 0xFFF;
    gte_stsxy(&(head - 1)->screenX);
    gte_stflg(&(head - 1)->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&(head - 1)->depth);
        block->depth -= 0x20;
        if (block->depth < 0x10) {
            block->depth = 0x10;
        }
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2A;
        prim->clut  = getClut(0x130 - (s16)pal * 0xA0, pal + 0x10A);
        setUV4(prim, arg1 * 0x28, 0x38, arg1 * 0x28 + 0x27, 0x38, arg1 * 0x28, 0x5F,
               arg1 * 0x28 + 0x27, 0x5F);
        block->extent.corner.x = (((arg2 * 39) / block->depth) * rsin(arg3)) >> 12;
        block->extent.corner.y = (((arg2 * 39) / block->depth) * rcos(arg3)) >> 12;
        prim->x0               = block->screenX + (u16)block->extent.corner.x;
        prim->x3               = block->screenX - (u16)block->extent.corner.x;
        prim->y0               = block->screenY - (u16)block->extent.corner.y;
        prim->y3               = block->screenY + (u16)block->extent.corner.y;
        ang                    = arg3 + 0x400;
        block->extent.corner.x = (((arg2 * 39) / block->depth) * rsin(ang)) >> 12;
        block->extent.corner.y = (((arg2 * 39) / block->depth) * rcos(ang)) >> 12;
        prim->x1               = block->screenX + (u16)block->extent.corner.x;
        prim->x2               = block->screenX - (u16)block->extent.corner.x;
        prim->y1               = block->screenY - (u16)block->extent.corner.y;
        prim->y2               = block->screenY + (u16)block->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

/// Puts `obj`, one of the player's bodies, on the object list: a sphere of
/// `radius` at `(x, y, z)` under `coord`, taking its direction from the actor's
/// `i`th direction record, whose contacts go to `recs`, and keyed by the saved
/// game's character.
static inline void _gpLinkPlayerObj(GameActor* actor, s32 i, WorldCollisionBody* obj, GfxCoord* coord, WorldCollisionContact* recs, s16 x, s16 y,
                                    s16 z, u16 radius, u16 flags)
{
    obj->context.motion                        = &actor->collisionMotionContexts[i];
    obj->coord                                 = coord;
    actor->collisionMotionContexts[i].contacts = recs;
    obj->pos.vx                                = x;
    obj->pos.vy                                = y;
    obj->pos.vz                                = z;
    obj->key                                   = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId | 0x10000;
    obj->radius                                = radius;
    obj->flags                                 = flags;
    Gp_LinkObj(0, obj);
}

static void Gp_InitPlayerWork(Task* arg0)
{
    GameActor*             actor;
    TmdObject*             extra;
    GfxCoord*              coord;
    WorldCollisionBody*    obj;
    WorldCollisionContact* recs;
    s32                    kind;
    s32                    anim;
    AnimationPlayRequest   sp;
    Task*                  task;

    actor = arg0->work;
    extra = arg0->extra.tmd;
    coord = extra->coords;
    arg0->state++;
    arg0->msgTable                              = Gp_PlayerMsgTable;
    arg0->exitCallback                          = Gp_TeardownSlot0;
    actor->animationSlotCount                   = GAME_ACTOR_NORMAL_ANIMATION_SLOTS;
    gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER] = arg0;
    gPlayerStatus.coordMtx                      = &coord->coord;
    coord->parent                               = &gGfxViewCoord;
    coord->composeStamp                         = GRAPHICS_COORD_DIRTY;
    extra->flags                                = 0;
    RotMatrix(&actor->rotation, &coord->coord);
    Gp_BindActorAnim(arg0);

    actor->animationRate       = ANIMATION_RATE_ONE;
    actor->previousPosition.vx = coord->coord.t[0];
    actor->previousPosition.vy = coord->coord.t[1];
    actor->previousPosition.vz = coord->coord.t[2];

    recs = actor->collisionContacts;
    obj  = &actor->collisionBodies[GAME_ACTOR_BODY_ROOT];
    _gpLinkPlayerObj(actor, GAME_ACTOR_BODY_ROOT, obj, coord, recs, 0, -0x12C, 0, 0x12C, WORLD_COLLISION_BODY_MOTION_SPHERE);
    Gp_InitRec18Table(actor->collisionMotionContexts[0].contacts, ARRAY_SIZE(actor->collisionContacts), 0);
    obj->flags |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_ROOM_TRIGGER_ENABLED | WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);

    obj = &actor->collisionBodies[GAME_ACTOR_BODY_PART4];
    _gpLinkPlayerObj(actor, GAME_ACTOR_BODY_PART4, obj, arg0->extra.tmd->coords + 4, recs, 0, 0x64, 0x28, 0xDC, WORLD_COLLISION_BODY_MOTION_SPHERE | (GAME_ACTOR_BODY_PART4 << WORLD_COLLISION_CONTACT_BODY_INDEX_SHIFT));
    obj->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

    obj = &actor->collisionBodies[GAME_ACTOR_BODY_PART1];
    _gpLinkPlayerObj(actor, GAME_ACTOR_BODY_PART1, obj, arg0->extra.tmd->coords + 1, recs, 0, 0x52, 0, 0xDC, WORLD_COLLISION_BODY_MOTION_SPHERE | (GAME_ACTOR_BODY_PART1 << WORLD_COLLISION_CONTACT_BODY_INDEX_SHIFT));
    obj->flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);

    kind                      = actor->mode;
    anim                      = actor->actionArgument;
    actor->attachmentTasks[0] = func_80104258(arg0, 0, 1, 1);
    task                      = func_80104258(arg0, 1, 1, 1);
    actor->attachmentTasks[1] = task;
    if (task != NULL) {
        Gp_SyncHeldRelated();
        Gp_SpawnWeaponEff();
    }
    if (kind == 2) {
        sp.blendFrames          = 0;
        sp.source.index         = actor->animationBankIndex;
        sp.blend                = ANIMATION_BLEND_RESET;
        sp.animationId          = anim;
        sp.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        func_80104508(arg0, 0, &sp, 0);
        actor->collisionEnableMask = PLAYER_ACTOR_WORLD_COLLISION_DISABLE;
    }
    if ((GAME_LOCATION_WORD(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(1, 5, 0, 0)) {
        actor->restrictRunAndAim = 1;
    }
}

static void Gp_PlayerWorkState1(Task* arg0)
{
    GameActor*          actor;
    GfxCoord*           coord;
    WorldCollisionBody* objs[2];
    s32                 dy;
    s32                 i;
    s8                  bits;

    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    if (actor->mode != GAME_ACTOR_MODE_SCRIPTED &&
        (dy = coord->coord.t[1], dy = dy - actor->previousPosition.vy, dy = ABS(dy), dy >= 0x300)) {
        coord->coord.t[0] = actor->previousPosition.vx;
        coord->coord.t[1] = actor->previousPosition.vy;
        coord->coord.t[2] = actor->previousPosition.vz;
    } else {
        actor->previousPosition.vx = coord->coord.t[0];
        actor->previousPosition.vy = coord->coord.t[1];
        actor->previousPosition.vz = coord->coord.t[2];
        if (actor->collisionEnableMask & 1) {
            actor->gridResponse = func_801011D0(coord, actor->collisionMotionContexts[0].contacts, ARRAY_SIZE(actor->collisionContacts), &actor->surfaceClass);
        } else {
            actor->gridResponse = 0;
        }
    }

    objs[0] = &actor->collisionBodies[GAME_ACTOR_BODY_ROOT];
    objs[1] = &actor->collisionBodies[GAME_ACTOR_BODY_PART1];
    for (i = 0; i < 2; i++) {
        bits = actor->pendingCollisionUpdates;
        if ((bits >> i) & 1) {
            actor->collisionEnableMask |= 1 << i;
            objs[i]->flags             |= WORLD_COLLISION_BODY_GRID_ENABLED;
        } else if (bits & (8 << i)) {
            actor->collisionEnableMask &= ~(1 << i);
            objs[i]->flags             &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
        }
    }
    actor->pendingCollisionUpdates = 0;
    coord->composeStamp            = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
}

void Gp_AttachActorObj(Task* arg0, s32 id, s32 kind)
{
    GameActor*             actor;
    WorldCollisionBody*    obj;
    WorldCollisionCapsule* rec;
    VECTOR*                tmp;
    Task*                  task;
    s32                    scale;

    actor = arg0->work;
    obj   = &actor->collisionBodies[GAME_ACTOR_BODY_WEAPON];
    rec   = &actor->weaponShape;
    SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    tmp  = SCRATCH_STACK_CURSOR(VECTOR);
    task = actor->equipmentTasks[1];
    if (task != NULL) {
        actor->weaponCollisionCoord = *task->extra.tmd->coords;
        gfxRotMatrixX(&actor->weaponCollisionCoord.workm, 0x400, GRAPHICS_ROTATION_COMPOSE);
        obj->coord                                         = &actor->weaponCollisionCoord;
        actor->weaponCollisionCoord.param.rot.vx           = 0;
        actor->weaponCollisionCoord.param.rot.vy           = 0;
        actor->weaponCollisionCoord.param.rot.vz           = 0;
        obj->context.capsule                               = &actor->weaponShape;
        obj->flags                                         = WORLD_COLLISION_BODY_CAPSULE;
        obj->pos.vx                                        = 0;
        obj->pos.vy                                        = 0;
        obj->pos.vz                                        = 0;
        actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key = 0x20000 | (id << 8) | kind;
        *tmp                                               = D_80112FA4[id];
        rec->ends[1].vx                                    = tmp->vx;
        rec->ends[1].vy                                    = tmp->vy;
        rec->ends[1].vz                                    = tmp->vz;
        rec->ends[0].vx                                    = rec->ends[1].vx;
        rec->ends[0].vy                                    = rec->ends[1].vy;
        rec->ends[0].vz                                    = rec->ends[1].vz + D_80112F60[id];
        scale                                              = 0x100;
        if (gPlayerStatus.weapon == 0x13) {
            scale = 0x280;
        }
        rec->end1Radius = scale;
        if (kind != 0xD) {
            rec->end0Radius = scale;
        } else {
            rec->end0Radius = 0x900;
        }
        rec->contacts = actor->weaponContacts;
        Gp_LinkObj(1, obj);
        Gp_InitRec18Table(rec->contacts, ARRAY_SIZE(actor->weaponContacts), 0);
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Steps a 16.16 value that has a fractional part one whole unit away from
/// zero, so that its integer half rounds away from zero.
#define GP_ROUND_FIXED_AWAY(x)                      \
    do {                                            \
        if (((x) & 0xFFFF) != 0) {                  \
            (x) += ((x) >= 0) ? 0x10000 : -0x10000; \
        }                                           \
    } while (0)

s32 func_801011D0(GfxCoord* arg0, WorldCollisionContact* arg1, s32 arg2, s32* arg3)
{
    WorldCollisionDelta* delta;
    s32                  ret;

    delta = SCRATCH_STACK_RESERVE_BLOCK(WorldCollisionDelta);
    ret   = func_800E0FEC(arg1, delta, arg2, arg3);
    if (ret != 0) {
        GP_ROUND_FIXED_AWAY(delta->fixed.vx.word);
        GP_ROUND_FIXED_AWAY(delta->fixed.vy.word);
        GP_ROUND_FIXED_AWAY(delta->fixed.vz.word);
        arg0->coord.t[0] += delta->fixed.vx.halves.integer;
        arg0->coord.t[1] += delta->fixed.vy.halves.integer;
        arg0->coord.t[2] += delta->fixed.vz.halves.integer;
        if (arg3 != NULL) {
            *arg3 = func_800E1ACC((u8*)arg3);
        }
        if ((delta->fixed.vx.word | delta->fixed.vz.word) == 0) {
            ret = 0;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(WorldCollisionDelta);
    return ret;
}

static void func_8010133C(void)
{
    void**                       scratch;
    _PlayerActorTextGridScratch* head;
    _PlayerActorTextGridScratch* block;
    _PlayerActorTextGridScratch* grid;
    s32                          color;

    scratch                                               = SCRATCH_HEAD_ADDR;
    color                                                 = 0x808008;
    head                                                  = SCRATCH_HEAD_AT(scratch, _PlayerActorTextGridScratch);
    block                                                 = head - 1;
    SCRATCH_HEAD_AT(scratch, _PlayerActorTextGridScratch) = block;
    grid                                                  = block;
    grid->colorRgb                                        = color;
    grid->penY                                            = -0x58;
    for (grid->row = 0; grid->row < 2; grid->row++) {
        grid->column = 0;
        grid->penX   = -0x40;
        for (; grid->column < 3; grid->column++) {
            grid->penX += 0x40;
            grid->penY -= 0x50;
        }
        grid->colorRgb = 0x37A78;
        grid->penY     = 8;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_PlayerActorTextGridScratch);
}

static void Gp_PlayerWorkState2(Task* arg0)
{
    arg0->state = 3;
}

static void Gp_TeardownSlot0(Task* arg0)
{
    volatile GameActor* inner;
    Task*               task;

    inner                                       = arg0->work;
    arg0->exitCallback                          = NULL;
    gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER] = NULL;
    task                                        = inner->weaponEffectTask;
    if (task != NULL) {
        taskKill(task);
    }
    task = inner->equipmentTasks[0];
    if (task != NULL) {
        taskKill(task);
    }
    task = inner->equipmentTasks[1];
    if (task != NULL) {
        taskKill(task);
    }
    task = inner->attachmentTasks[0];
    if (task != NULL) {
        taskKill(task);
    }
    task = inner->attachmentTasks[1];
    if (task != NULL) {
        taskKill(task);
    }
    Gp_UnlinkObj((WorldCollisionBody*)&inner->collisionBodies[GAME_ACTOR_BODY_ROOT]);
    Gp_UnlinkObj((WorldCollisionBody*)&inner->collisionBodies[GAME_ACTOR_BODY_PART4]);
    Gp_UnlinkObj((WorldCollisionBody*)&inner->collisionBodies[GAME_ACTOR_BODY_PART1]);
    Gp_UnlinkObj((WorldCollisionBody*)&inner->collisionBodies[GAME_ACTOR_BODY_WEAPON]);
    Gp_UnlinkObj((WorldCollisionBody*)&inner->collisionBodies[GAME_ACTOR_BODY_AIM]);
    taskKill(arg0);
}

void Gp_PlayerWorkTask(Task* arg0)
{
    TaskFuncTable4 handlers;

    handlers = Gp_PlayerWorkStates;
    handlers.funcs[arg0->state](arg0);
}

/// Latches this frame's pad state into the actor of `arg0`: keeps the previous
/// values of the per-frame bytes and of the held buttons, reads the session's
/// pad, and derives the newly pressed and released buttons from the two.
static inline void _gpCaptureActorPad(Task* arg0)
{
    GameActor* actor;
    u16        buttons;

    actor                        = arg0->work;
    actor->previousMovementSign  = actor->movementSign;
    actor->previousTurnSign      = actor->turnSign;
    actor->previousPadHeld       = actor->padHeld;
    buttons                      = gGameSession->padHeld;
    actor->previousRunButtonHeld = actor->runButtonHeld;
    actor->padHeld               = buttons;
    actor->padPressed            = actor->padHeld & ~actor->previousPadHeld;
    actor->padReleased           = actor->previousPadHeld & ~actor->padHeld;
    actor->runButtonHeld         = (actor->padHeld >> 6) & 1;
}

void Gp_UpdatePlayerMove(void)
{
    Task*              work;
    GameActor*         actor;
    register GfxCoord* coord asm("s1");
    SVECTOR*           vec;
    Task*              task;
    MATRIX*            mat;

    work  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    actor = work->work;
    SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    vec   = SCRATCH_STACK_CURSOR(SVECTOR);
    coord = work->extra.tmd->coords;
    _gpCaptureActorPad(work);
    gSceneCombatState.signals.bytes.actionFlags = 0;
    if (D_80115768 == 0) {
        Gp_TickPlayerActor(work);
    }
    coord->coord.t[0]            += actor->pendingDisplacement.vx;
    coord->coord.t[1]            += actor->pendingDisplacement.vy;
    coord->coord.t[2]            += actor->pendingDisplacement.vz;
    actor->pendingDisplacement.vx = 0;
    actor->pendingDisplacement.vy = 0;
    actor->pendingDisplacement.vz = 0;
    Gp_ClearRec18Occupied(actor->collisionContacts);
    if (actor->equipmentTasks[1] != NULL) {
        Gp_ClearRec18Occupied(actor->weaponContacts);
    }
    if (actor->collisionEnableMask & 1) {
        coord->coord.t[1] += 0x80;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    if (actor->usesPushbackDirection != 0) {
        vec->vx = actor->pushbackDirection.vx;
        vec->vy = actor->pushbackDirection.vy;
        vec->vz = actor->pushbackDirection.vz;
    } else {
        vec->vx = coord->workm.m[0][2] * actor->movementSign;
        vec->vy = coord->workm.m[1][2] * actor->movementSign;
        vec->vz = coord->workm.m[2][2] * actor->movementSign;
    }
    task                                                 = actor->equipmentTasks[1];
    actor->collisionMotionContexts[0].motionDirection.vx = vec->vx;
    actor->collisionMotionContexts[0].motionDirection.vy = vec->vy;
    actor->collisionMotionContexts[0].motionDirection.vz = vec->vz;
    actor->collisionMotionContexts[1].motionDirection.vx = vec->vx;
    actor->collisionMotionContexts[1].motionDirection.vy = vec->vy;
    actor->collisionMotionContexts[1].motionDirection.vz = vec->vz;
    actor->collisionMotionContexts[2].motionDirection.vx = vec->vx;
    actor->collisionMotionContexts[2].motionDirection.vy = vec->vy;
    actor->collisionMotionContexts[2].motionDirection.vz = vec->vz;
    if (task != NULL) {
        actor->weaponCollisionCoord = *task->extra.tmd->coords;
        mat                         = &actor->weaponCollisionCoord.workm;
        if (gPlayerStatus.weapon != 0x17) {
            gfxRotMatrixX(mat, -0x400, GRAPHICS_ROTATION_COMPOSE);
            gfxRotMatrixY(mat, -0x20, 0);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

void Gp_TickActorAnimState(Task* arg0)
{
    GameActor*             actor;
    const AnimationRecord* rec;
    s32                    i;
    s32                    anim;
    s32                    extra;
    u16                    flags;

    actor = arg0->work;
    rec   = Gp_AnimGetRec(&actor->animationContext, actor->animationSlots + 1);
    switch (actor->animationState) {
        case 0:
        case 1:
            break;
        case 2:
            if (actor->statePhase != 0) {
                break;
            }
            if (func_8010583C(arg0, 0, 0, 0) != 0) {
                break;
            }
            anim               = 9;
            extra              = 5;
            i                  = 1;
            actor->statePhase += i;
            actor              = arg0->work;
            if (i < actor->animationSlotCount) {
                do {
                    animationPlaySlotWithBlend(&actor->animationContext, i, 0, anim, 0, 0, extra,
                                               actor->animationSets);
                    actor->animationSlots[i].rate = actor->animationRate;
                    i++;
                } while (i < actor->animationSlotCount);
            }
            break;
        case 3:
            break;
        case 5:
            if (rec != NULL) {
                if (func_80105894(arg0, 1, 0, 0) == 0) {
                    Gp_ResetActorAnimState(arg0, 3);
                }
            }
            break;
        case 4:
        case 6:
            if (rec != NULL) {
                if (func_80105894(arg0, 1, 0, 0) == 0) {
                    func_801066DC(arg0, 0);
                }
            }
            break;
        case 7:
        case 9:
            if (rec != NULL) {
                if (func_80105894(arg0, 1, 0, 0) == 0) {
                    actor->statePhase++;
                }
            }
            break;
        case 8:
            if (rec != NULL) {
                flags = actor->animationSlots[1].status.fields.flags;
                if ((flags & ANIMATION_SLOT_REACHED_BOUNDARY) || (flags & ANIMATION_SLOT_FOLLOWED_JUMP)) {
                    actor->statePhase++;
                    func_801066DC(arg0, 0);
                }
            }
            break;
        case 10:
            if (rec != NULL) {
                if (func_80105894(arg0, 1, 0, 0) == 0) {
                    actor->statePhase = 0x3E8;
                }
            }
            break;
    }
}

void Gp_StepPlayerMove(Task* arg0)
{
    GameActor*                   actor;
    GfxCoord*                    coord;
    _PlayerActorMoveStepScratch* block;

    block = SCRATCH_STACK_RESERVE_BLOCK(_PlayerActorMoveStepScratch);
    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch ((u16)actor->movementMode) {
        case 0:
            actor->velocity.vx = 0;
            actor->velocity.vy = 0;
            actor->velocity.vz = 0;
            break;
        case 1:
        case 2:
        case 3:
        case 5:
        case 6:
        case 7:
            if (Gp_AnimGetRec(&actor->animationContext,
                              actor->animationSlots + 1) == NULL) {
                actor->velocity.vx = 0;
                actor->velocity.vy = 0;
                actor->velocity.vz = 0;
            } else {
                block->speedDivisor = D_80112E10[(u16)actor->movementMode];
                gfxReadMatrixZAxis(&coord->coord, &block->direction);
                VectorNormalSS(&block->direction, &block->direction);
                actor->velocity.vx = block->direction.vx * actor->movementSign / block->speedDivisor;
                actor->velocity.vy = 0;
                actor->velocity.vz = block->direction.vz * actor->movementSign / block->speedDivisor;
            }
            break;
        case 4:
            block->speedDivisor = D_80112E10[(u16)actor->movementMode];
            gfxReadMatrixZAxis(&coord->coord, &block->direction);
            VectorNormalSS(&block->direction, &block->direction);
            actor->velocity.vx  = block->direction.vx * actor->movementSign / block->speedDivisor;
            actor->velocity.vy  = 0;
            actor->velocity.vz  = block->direction.vz * actor->movementSign / block->speedDivisor;
            coord->coord.t[0]  += actor->velocity.vx;
            coord->coord.t[1]  += actor->velocity.vy;
            coord->coord.t[2]  += actor->velocity.vz;
            block->savedMatrix  = coord->coord;
            block->speedDivisor = D_80112E10[(u16)actor->movementMode];
            Gp_GetLockPos(actor->targetNode, &block->targetPosition);
            block->direction.vx  = abs(coord->coord.t[0] - block->targetPosition.vx);
            block->direction.vx += abs(coord->coord.t[2] - block->targetPosition.vz);
            block->strafeYaw     = 0x640000;
            block->strafeYaw     = (0x800 - block->strafeYaw / (block->direction.vx * 0x274)) >> 1;
            gfxRotMatrixY(&coord->coord, block->strafeYaw, 0);
            gfxReadMatrixZAxis(&coord->coord, &block->direction);
            actor->velocity.vx = block->direction.vx * actor->turnSign / block->speedDivisor;
            actor->velocity.vy = 0;
            actor->velocity.vz = block->direction.vz * actor->turnSign / block->speedDivisor;
            coord->coord       = block->savedMatrix;
            break;
    }
    coord->coord.t[0] += actor->velocity.vx;
    coord->coord.t[1] += actor->velocity.vy;
    coord->coord.t[2] += actor->velocity.vz;
    SCRATCH_STACK_RELEASE_BLOCK(_PlayerActorMoveStepScratch);
}

/// Eases `angle` back toward zero by an eighth of itself, at least 0x20 per
/// call, and snaps it to zero once it is within 0x20; an angle that was not
/// yet zero sets `moving`. `step` receives the amount taken off.
#define GP_DECAY_ANGLE(angle, step, moving)           \
    do {                                              \
        if ((angle) != 0) {                           \
            (moving) = 1;                             \
            (step)   = (angle) >> 3;                  \
            if (ABS(step) < 0x20) {                   \
                (step) = ((step) < 0) ? -0x20 : 0x20; \
            }                                         \
            (angle) -= (step);                        \
            if (ABS(angle) < 0x21) {                  \
                (angle) = 0;                          \
            }                                         \
        }                                             \
    } while (0)

/// Clears the frame stamp of node `i` of the task's model, so it is composed
/// again, and returns that node's local matrix for the caller to rebuild.
static inline MATRIX* _gpRebuildCoordMatrix(Task* task, s32 i)
{
    GfxCoord* coord = &task->extra.tmd->coords[i];

    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return &coord->coord;
}

void Gp_TurnPlayer(Task* arg0)
{
    GameActor* actor;
    GfxCoord*  coord;
    MATRIX*    m;
    s32        moving;
    s16        step;

    actor  = arg0->work;
    coord  = arg0->extra.tmd->coords;
    moving = 0;
    if (actor->turnRateIndex != 0) {
        s32 dir = *(volatile u8*)&actor->turnSign;

        actor->rotation.vy = (actor->rotation.vy + D_80112E20[actor->turnRateIndex] * (s8)dir) & 0xFFF;
    }
    RotMatrix(&actor->rotation, &coord->coord);
    MatrixNormal(&coord->coord, &coord->coord);
    if (actor->aimTrackingState == GAME_ACTOR_AIM_TRACKING_DECAY) {
        GP_DECAY_ANGLE(actor->part2Pitch, step, moving);
        GP_DECAY_ANGLE(actor->part2Roll, step, moving);
        GP_DECAY_ANGLE(actor->part3Pitch, step, moving);
        GP_DECAY_ANGLE(actor->part3Roll, step, moving);
        GP_DECAY_ANGLE(actor->part6Pitch, step, moving);
        if (moving == 0) {
            actor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_OFF;
        }
    }
    m = _gpRebuildCoordMatrix(arg0, 2);
    RotMatrixX(actor->part2Pitch, m);
    RotMatrixZ(actor->part2Roll, m);
    MatrixNormal(m, m);
    m = _gpRebuildCoordMatrix(arg0, 3);
    RotMatrixX(actor->part3Pitch, m);
    RotMatrixZ(actor->part3Roll, m);
    MatrixNormal(m, m);
    m = _gpRebuildCoordMatrix(arg0, 4);
    gfxRotMatrixY(m, actor->aimYaw, 0);
    MatrixNormal(m, m);
    m = _gpRebuildCoordMatrix(arg0, 6);
    gfxRotMatrixX(m, actor->part6Pitch, GRAPHICS_ROTATION_COMPOSE);
    MatrixNormal(m, m);
}

/// The signed turn from `from` to `to` (4096 units per revolution), taking
/// whichever of the direct difference and its one-revolution neighbours is
/// shortest.
static inline s16 _gpShortestTurn(s16 from, s16 to)
{
    _PlayerActorShortestTurnScratch* candidates;

    SCRATCH_STACK_RESERVE_BLOCK(_PlayerActorShortestTurnScratch);
    candidates            = SCRATCH_STACK_CURSOR(_PlayerActorShortestTurnScratch);
    candidates->direct    = to - from;
    candidates->plusTurn  = candidates->direct + ACTOR_TRANSFORM_ANGLE_TURN;
    candidates->minusTurn = candidates->direct - ACTOR_TRANSFORM_ANGLE_TURN;
    if (ABS(candidates->direct) < ABS(candidates->plusTurn) && ABS(candidates->direct) < ABS(candidates->minusTurn)) {
        from = candidates->direct;
    } else if (ABS(candidates->plusTurn) < ABS(candidates->minusTurn)) {
        from = candidates->plusTurn;
    } else {
        from = candidates->minusTurn;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_PlayerActorShortestTurnScratch);
    return from;
}

/// Turns `actor` toward its lock target once the target is farther than
/// `thresh` in the ground plane, by at most the equipped weapon's turn rate.
static inline void _gpAimYawAt(GameActor* actor, _PlayerActorAimYawScratch* block, s16 thresh)
{
    SVECTOR*  offset;
    GfxCoord* src;
    VECTOR3*  lock;
    s32       dx;
    s32       dz;
    s32       limit;

    if (actor->targetNode != NULL) {
        offset                 = &D_801131B4[gPlayerStatus.weapon];
        src                    = actor->equipmentTasks[1]->extra.tmd->coords;
        block->originOffset.vx = offset->vx;
        block->originOffset.vy = offset->vy;
        block->originOffset.vz = offset->vz;
        Gp_PlaceCoordOffset(src, &block->originCoord, &block->originOffset);
        lock = &block->targetDelta;
        Gp_GetLockPos(actor->targetNode, lock);
        lock->vx -= block->originCoord.coord.t[0];
        lock->vy -= block->originCoord.coord.t[1];
        lock->vz -= block->originCoord.coord.t[2];
        dx        = block->targetDelta.vx;
        dx        = ABS(dx);
        dx        = dx * dx;
        dz        = block->targetDelta.vz;
        dz        = ABS(dz);
        dz        = dz * dz;
        if (SquareRoot0(dx + dz) > thresh) {
            block->yaw = ratan2(block->targetDelta.vx, block->targetDelta.vz);
            block->yaw = _gpShortestTurn(actor->rotation.vy, block->yaw);
            limit      = (s16)D_80112E30[gPlayerStatus.weapon];
            if (func_800B9D80(0x2000) != 0) {
                limit += limit >> 1;
            }
            if (block->yaw > limit) {
                block->yaw = limit;
            } else if (block->yaw < -limit) {
                block->yaw = -limit;
            }
            actor->rotation.vy = (actor->rotation.vy + block->yaw) & 0xFFF;
        }
    }
}

void Gp_AimYawToLock(Task* arg0, s32 arg1)
{
    GameActor*                 actor;
    _PlayerActorAimYawScratch* block;

    actor = arg0->work;
    block = SCRATCH_STACK_RESERVE_BLOCK(_PlayerActorAimYawScratch);
    _gpAimYawAt(actor, block, arg1);
    SCRATCH_STACK_RELEASE_BLOCK(_PlayerActorAimYawScratch);
}

/// Places `block->originCoord` at the point `offset` in the local space of
/// `src`, with the orientation of `src`.
static inline void _gpAimPitchPlace(_PlayerActorAimPitchScratch* block, GfxCoord* src, SVECTOR* offset)
{
    block->originOffset.vx = offset->vx;
    block->originOffset.vy = offset->vy;
    block->originOffset.vz = offset->vz;
    Gp_PlaceCoordOffset(src, &block->originCoord, &block->originOffset);
}

/// Stores the lock target's position relative to `block->originCoord` in
/// `block->targetDelta` and returns the length of that offset in the ground
/// plane.
static inline s32 _gpAimPitchLockDelta(GameActor* actor, _PlayerActorAimPitchScratch* block)
{
    VECTOR3* lock;
    VECTOR3* delta;
    s32      dx;
    s32      dz;

    lock = &block->targetPosition;
    Gp_GetLockPos(actor->targetNode, lock);
    delta     = &block->targetDelta;
    delta->vx = lock->vx - block->originCoord.coord.t[0];
    delta->vy = lock->vy - block->originCoord.coord.t[1];
    delta->vz = lock->vz - block->originCoord.coord.t[2];
    dx        = block->targetDelta.vx;
    dx        = ABS(dx);
    dx        = dx * dx;
    dz        = block->targetDelta.vz;
    dz        = ABS(dz);
    dz        = dz * dz;
    return SquareRoot0(dx + dz);
}

void Gp_AimPitchToLock(Task* arg0)
{
    GameActor*                   actor;
    _PlayerActorAimPitchScratch* block;
    GfxCoord*                    src;

    actor = arg0->work;
    block = SCRATCH_STACK_RESERVE_BLOCK(_PlayerActorAimPitchScratch);
    if (actor->targetNode != NULL) {
        src                    = arg0->extra.tmd->coords;
        block->originOffset.vx = 0;
        block->originOffset.vy = -0x400;
        block->originOffset.vz = 0;
        Gp_PlaceCoordOffset(&src[2], &block->originCoord, &block->originOffset);
        block->groundDistance   = _gpAimPitchLockDelta(actor, block);
        block->targetDelta.vy >>= 1;
        block->pitch            = ratan2(-block->targetDelta.vy, block->groundDistance) / 7 * 4;
        block->pitch           -= actor->part2Pitch;
        if (block->pitch > 0x30) {
            block->pitch = 0x30;
        } else if (block->pitch < -0x30) {
            block->pitch = -0x30;
        }
        if (ABS(actor->part2Pitch + block->pitch) <= 0x120) {
            actor->part2Pitch += block->pitch;
            actor->part2Roll   = (actor->part2Pitch / 5) * 3;
        }

        _gpAimPitchPlace(block, actor->equipmentTasks[1]->extra.tmd->coords, &D_801131B4[gPlayerStatus.weapon]);
        block->groundDistance = _gpAimPitchLockDelta(actor, block);
        block->pitch          = ratan2(-block->targetDelta.vy, block->groundDistance) / 7 * 4;
        block->pitch         -= actor->part3Pitch;
        if (block->pitch > 0x30) {
            block->pitch = 0x30;
        } else if (block->pitch < -0x30) {
            block->pitch = -0x30;
        }
        if (ABS(actor->part3Pitch + block->pitch) <= 0x100) {
            actor->part3Pitch += block->pitch;
            actor->part3Roll   = (actor->part3Pitch / 5) * 2;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_PlayerActorAimPitchScratch);
}

static void Gp_AimPitchToLockAlt(Task* arg0)
{
    GameActor*                   actor;
    _PlayerActorAimPitchScratch* block;
    GfxCoord*                    src;

    actor = arg0->work;
    block = SCRATCH_STACK_RESERVE_BLOCK(_PlayerActorAimPitchScratch);
    if (actor->targetNode != NULL) {
        src                    = arg0->extra.tmd->coords;
        block->originOffset.vx = 0;
        block->originOffset.vy = -0x400;
        block->originOffset.vz = 0;
        Gp_PlaceCoordOffset(&src[2], &block->originCoord, &block->originOffset);
        block->groundDistance   = _gpAimPitchLockDelta(actor, block);
        block->targetDelta.vy >>= 1;
        block->pitch            = ratan2(-block->targetDelta.vy, block->groundDistance) / 7 * 4;
        block->pitch           -= actor->part2Roll;
        if (block->pitch > 0x30) {
            block->pitch = 0x30;
        } else if (block->pitch < -0x30) {
            block->pitch = -0x30;
        }
        if (ABS(actor->part2Roll + block->pitch) <= 0x120) {
            actor->part2Roll += block->pitch;
        }

        _gpAimPitchPlace(block, actor->equipmentTasks[1]->extra.tmd->coords, &D_801131B4[gPlayerStatus.weapon]);
        block->groundDistance = _gpAimPitchLockDelta(actor, block);
        block->pitch          = ratan2(-block->targetDelta.vy, block->groundDistance) / 7 * 4;
        block->pitch         -= actor->part3Roll;
        if (block->pitch > 0x30) {
            block->pitch = 0x30;
        } else if (block->pitch < -0x30) {
            block->pitch = -0x30;
        }
        if (ABS(actor->part3Roll + block->pitch) <= 0x100) {
            actor->part3Roll += block->pitch;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_PlayerActorAimPitchScratch);
}

void Gp_AimPitchRec(Task* arg0, s32 arg1, s32 arg2)
{
    GameActor*                   actor;
    _PlayerActorAimPitchScratch* block;
    s32                          angle;

    actor = arg0->work;
    block = SCRATCH_STACK_RESERVE_BLOCK(_PlayerActorAimPitchScratch);
    if (actor->targetNode != NULL) {
        _gpAimPitchPlace(block, actor->equipmentTasks[1]->extra.tmd->coords, &D_801131B4[arg1]);
        block->groundDistance = _gpAimPitchLockDelta(actor, block);
        if (block->groundDistance > (s16)arg2) {
            block->pitch  = ratan2(-block->targetDelta.vy, block->groundDistance);
            block->pitch -= actor->part6Pitch;
            if (ABS(block->pitch) >= 0x20) {
                if (block->pitch > 0x30) {
                    block->pitch = 0x30;
                } else if (block->pitch < -0x30) {
                    block->pitch = -0x30;
                }
                if (ABS(actor->part6Pitch + block->pitch) <= 0x280) {
                    actor->part6Pitch += block->pitch;
                }
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_PlayerActorAimPitchScratch);
}

static void Gp_AimPitchDirect(Task* arg0)
{
    GameActor*                   actor;
    _PlayerActorAimPitchScratch* block;
    GfxCoord*                    src;

    actor = arg0->work;
    block = SCRATCH_STACK_RESERVE_BLOCK(_PlayerActorAimPitchScratch);
    if (actor->targetNode != NULL) {
        src                    = actor->equipmentTasks[1]->extra.tmd->coords;
        block->originOffset.vx = 0;
        block->originOffset.vy = 0;
        block->originOffset.vz = 0;
        Gp_PlaceCoordOffset(src, &block->originCoord, &block->originOffset);
        block->groundDistance = _gpAimPitchLockDelta(actor, block);
        block->pitch          = ratan2(-block->targetDelta.vy, block->groundDistance);
        block->pitch         -= actor->directAimPitch;
        if (ABS(block->pitch) >= 0x20) {
            if (block->pitch > 0x30) {
                block->pitch = 0x30;
            } else if (block->pitch < -0x30) {
                block->pitch = -0x30;
            }
            if (ABS(actor->directAimPitch + block->pitch) <= 0x280) {
                actor->directAimPitch += block->pitch;
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_PlayerActorAimPitchScratch);
}

static void func_801030CC(Task* arg0)
{
    RECT*           rect;
    GameActor*      actor;
    GpuImageUpload* uploadList;

    actor = arg0->work;
    rect  = SCRATCH_STACK_RESERVE_BLOCK(RECT);

    if ((s8)actor->textureSequenceA != 0) {
        actor->textureDelayA--;
        if ((s8)actor->textureDelayA <= 0) {
            uploadList = D_80112E74[(s8)actor->textureSequenceA * 4 + (gPlayerStatus.resourceVariant - 5)][(s8)actor->textureFrameA];
            if (uploadList != NULL) {
                rect->x = 0;
                rect->y = 0x4E;
                rect->w = 0x19;
                rect->h = 0x10;
                Gp_LoadActorImage(arg0, uploadList, rect);
                actor->textureDelayA = 4;
                actor->textureFrameA++;
            } else {
                actor->textureSequenceA = 0;
            }
        }
    }

    if ((s8)actor->textureSequenceB != 0) {
        actor->textureDelayB--;
        if ((s8)actor->textureDelayB <= 0) {
            uploadList = D_80112EB4[(s8)actor->textureSequenceB * 4 + (gPlayerStatus.resourceVariant - 5)][(s8)actor->textureFrameB];
            if (uploadList != NULL) {
                rect->x = 0xC;
                rect->y = 0x68;
                rect->w = 0xE;
                rect->h = 0x14;
                Gp_LoadActorImage(arg0, uploadList, rect);
                actor->textureDelayB = 8;
                actor->textureFrameB++;
            } else {
                actor->textureSequenceB = 0;
            }
        }
    }

    SCRATCH_STACK_RELEASE_BLOCK(RECT);
}

inline static Task* spawn_tmd_attach(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    Task*      task;
    GameActor* actor;
    TmdObject* extra;
    GfxCoord*  coord;
    TmdObject* obj;
    GfxCoord*  saved;
    u8*        table;
    s32        type;

    extra = arg0->extra.tmd;
    actor = arg0->work;
    saved = &extra->coords[D_80112E04[arg2][arg1]];
    table = D_80112DFC;
    type  = gPlayerStatus.resourceVariant - 2;
    task  = Task_Spawn(7, table[arg2 + type] + arg3 * 2 + arg1, 0, 0);
    if (task == NULL) {
        return NULL;
    }
    task->parent            = arg0;
    coord                   = task->extra.tmd->coords;
    coord->parent           = saved;
    coord->param.clearFlags = false;
    obj                     = task->extra.tmd;
    if (actor->companionWork != NULL) {
        obj->texturePageOffset = 4;
        obj->clutRowOffset     = 6;
    } else {
        obj->texturePageOffset = 6;
        obj->clutRowOffset     = 0;
    }
    tmdProcessStream(obj);
    tmdProcessStream(obj);
    return task;
}

static Task* func_80103294(Task* arg0, s32 arg1, s32 arg2)
{
    GameActor* actor;

    actor = arg0->work;
    if (actor->attachmentTasks[0] != NULL) {
        taskKill(actor->attachmentTasks[0]);
    }
    actor->attachmentTasks[0] = spawn_tmd_attach(arg0, 0, arg1, arg2);
    if (actor->attachmentTasks[1] != NULL) {
        taskKill(actor->attachmentTasks[1]);
    }
    actor->attachmentTasks[1] = spawn_tmd_attach(arg0, 1, arg1, arg2);
    return actor->attachmentTasks[1];
}

inline static Task* spawn_attach(Task* parent, s32 row, s32 item)
{
    GfxCoord*  saved;
    Task*      task;
    TmdObject* extra;
    GfxCoord*  coord;
    s32        type;

    saved = parent->extra.tmd->coords;
    if (item == 0) {
        return NULL;
    }
    type = D_80112DF4[row] - 1;
    task = Task_Spawn(7, type + item, 0, 0);
    if (task == NULL) {
        return NULL;
    }
    extra                   = task->extra.tmd;
    task->parent            = parent;
    coord                   = extra->coords;
    coord->parent           = saved;
    coord->param.clearFlags = true;
    return task;
}

Task* Gp_SpawnWeaponEff(void)
{
    Task*         work;
    GameActor*    actor;
    Task*         parent;
    Task*         task;
    PlayerStatus* cfg;
    s32           kind;
    s32           id;
    s32           arg2;
    TmdObject*    extra;
    GfxCoord*     coord;
    EffectWork*   eff;
    GameActor*    inner;
    TmdObject*    anim;

    work  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    actor = work->work;
    if (!work | !actor) {
        return 0;
    }

    parent = actor->attachmentTasks[1];
    if (parent == NULL) {
        goto join_4C;
    }

    task                     = spawn_attach(parent, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId, gPlayerStatus.weapon);
    actor->equipmentTasks[1] = task;
    if (task == NULL) {
        goto join_4C;
    }

    cfg = &gPlayerStatus;
    Gp_AttachActorObj(work, cfg->weapon, cfg->weaponSlotItem);
    if (actor->weaponEffectTask != NULL) {
        goto join_50;
    }

    kind  = 0x16;
    extra = actor->equipmentTasks[1]->extra.tmd;
    id    = cfg->weapon;
    coord = extra->coords;
    if (id != kind) {
        goto check_19;
    }
    id   = 0x80060024;
    arg2 = 0;
    goto do_call;

do_success:
    actor->weaponEffectTask = eff->task;
    taskReparent(work, eff->task);
    func_80106350(work, gPlayerStatus.weapon, 0);
    goto join_50;

check_19:
    if (id != 0x19) {
        goto check_1C;
    }
    id = 0x80060029;
    goto do_call_item;

check_1C:
    if (id != 0x1C) {
        goto join_50;
    }
    id = 0x8006002A;
do_call_item:
    arg2 = cfg->weapon;
do_call:
    eff = Gp_SpawnEff(id, coord, arg2, 0);
    if (eff != NULL) {
        goto do_success;
    }

join_4C:
join_50:
    actor->reloadEffectSuppressed = 0;
    inner                         = work->work;
    anim                          = work->extra.tmd;
    inner->animationBankIndex     = Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon;
    inner->animationSets          = Gp_PlayerAnimBlkTbl[inner->animationBankIndex]->table.sets;
    animationInitContext(&inner->animationContext, inner->animationSets, anim, inner->poseBuffer,
                         inner->animationSlots);
    func_801066DC(work, 1);
    actor->pendingCollisionUpdates                      = GAME_ACTOR_COLLISION_REQUEST_MASK;
    actor->collisionBodies[GAME_ACTOR_BODY_ROOT].flags |= WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED;
    return actor->equipmentTasks[1];
}

Task* Gp_SpawnPlayer(const ActorSpawnTransform* spawnTransform, u16 arg1, s32 arg2, ActorSpawnOptions* options)
{
    Task*      task;
    GameActor* actor;
    GfxCoord*  coord;

    task = Task_Spawn(7, gPlayerStatus.resourceVariant + 3, arg2, options);
    if (task != NULL) {
        goto have_task;
    }
    return NULL;

have_task:
    actor = memCalloc(sizeof(*actor), 0);
    if (actor != NULL) {
        goto have_actor;
    }
    taskKill(task);
    return NULL;

have_actor:
    gameSetTaskSlot(task, GAME_TASK_SLOT_PLAYER);
    task->work = actor;
    memFillBytes(actor, 0, sizeof(*actor));
    actor->actionArgument = options->initialAnimationId;
    actor->rotation.vy    = spawnTransform->yaw.angle;
    coord                 = task->extra.tmd->coords;
    coord->coord.t[0]     = spawnTransform->x;
    coord->coord.t[1]     = spawnTransform->y;
    coord->coord.t[2]     = spawnTransform->z;
    D_80115768            = 0;
    if (options->startScripted != 0) {
        actor->mode = GAME_ACTOR_MODE_SCRIPTED;
    }
    return task;
}

static void Gp_CaptureActorPad(Task* arg0)
{
    GameActor* actor;
    u16        buttons;
    s32        flag;

    actor                        = arg0->work;
    actor->previousMovementSign  = actor->movementSign;
    actor->previousTurnSign      = actor->turnSign;
    actor->previousPadHeld       = actor->padHeld;
    buttons                      = gGameSession->padHeld;
    actor->previousRunButtonHeld = actor->runButtonHeld;
    actor->padHeld               = buttons;
    actor->padPressed            = actor->padHeld & ~actor->previousPadHeld;
    actor->padReleased           = actor->previousPadHeld & ~actor->padHeld;
    flag                         = 1;
    actor->runButtonHeld         = (actor->padHeld >> 6) & flag;
}

static void Gp_BindActorAnim(Task* arg0)
{
    GameActor* actor;
    TmdObject* extra;

    actor                     = arg0->work;
    extra                     = arg0->extra.tmd;
    actor->animationBankIndex = Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon;
    actor->animationSets      = Gp_PlayerAnimBlkTbl[actor->animationBankIndex]->table.sets;
    animationInitContext(&actor->animationContext, actor->animationSets, extra, actor->poseBuffer,
                         actor->animationSlots);
}

void Gp_AnimResetChildSlots(Task* arg0, s32 arg1)
{
    GameActor* actor;
    s32        i;

    actor = arg0->work;
    i     = 1;
    if (i < actor->animationSlotCount) {
        do {
            animationResetSlot(&actor->animationContext, i, arg1);
            actor->animationSlots[i].rate = actor->animationRate;
            i++;
        } while (i < actor->animationSlotCount);
    }
}

void Gp_AnimPlayChildSlots(Task* arg0, s32 arg1, s32 arg2)
{
    GameActor* actor;
    s32        i;

    actor = arg0->work;
    i     = 1;
    if (i < actor->animationSlotCount) {
        do {
            animationPlaySlotWithBlend(&actor->animationContext, i, 0, arg1, 0, 0, 0, actor->animationSets);
            actor->animationSlots[i].rate = actor->animationRate;
            i++;
        } while (i < actor->animationSlotCount);
    }
}

void playerActorPlayChildSlotsWithBlend(Task* task, s32 setIndex, s32 unusedArgument, s32 blendFrames)
{
    enum { PLAYER_ACTOR_FIRST_CHILD_ANIMATION_SLOT = 1 };
    GameActor* actor;
    s32        slotIndex;

    actor = task->work;
    for (slotIndex = PLAYER_ACTOR_FIRST_CHILD_ANIMATION_SLOT; slotIndex < actor->animationSlotCount; slotIndex++) {
        // Capture at the slot's previous rate before applying the actor's rate to the new blend.
        animationPlaySlotWithBlend(&actor->animationContext, slotIndex, NULL, setIndex, 0, 0, blendFrames,
                                   actor->animationSets);
        actor->animationSlots[slotIndex].rate = actor->animationRate;
    }
}

void Gp_AnimTickChildSlots(Task* arg0)
{
    GameActor* inner;
    s32        i;

    inner = arg0->work;
    i     = 1;
    if (i < inner->animationSlotCount) {
        do {
            animationTickSlot(&inner->animationContext, i);
            i++;
        } while (i < inner->animationSlotCount);
    }
}

static s32 Gp_HpBand(void)
{
    PlayerStatus* p;
    s32           temp;
    s32           ret;

    p    = &gPlayerStatus;
    temp = (u16)p->hpMax << 16;
    if ((temp >> 17) < p->hp) {
        ret = 0;
    } else {
        ret = 1;
        if ((temp >> 18) >= p->hp) {
            ret = 2;
        }
    }
    return ret;
}

void Gp_DetachLinkNode(Task* arg0)
{
    GameActor*       inner;
    WorldTargetNode* node;

    inner = arg0->work;
    node  = inner->targetNode;
    if (node != NULL) {
        node->state.parts.targeted = 0;
        inner->targetNode          = NULL;
    }
    inner->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
}

static s32 Gp_ApplyDirArg(Task* arg0, GameActorMoveBy* move)
{
    GameActor* actor;
    GfxCoord*  coord;
    s16        delta;

    actor = arg0->work;
    if (move->collisionRequests == GAME_ACTOR_COLLISION_REQUEST_MASK) {
        if ((move->displacement.vx != 0) || (move->displacement.vz != 0)) {
            coord = arg0->extra.tmd->coords;
            delta = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]) - ratan2(move->displacement.vx, move->displacement.vz);
            if (delta > 0x801) {
                delta -= 0x1000;
            }
            if (delta < -0x800) {
                delta += 0x1000;
            }
            if (ABS(delta) < 0x400) {
                actor->movementSign = 1;
            } else {
                actor->movementSign = -1;
            }
        }
    }
    return actor->movementSign;
}

void func_80103C74(GfxCoord* arg0, VECTOR3* arg1, VECTOR3* arg2)
{
    arg2->vx = arg1->vx - arg0->coord.t[0];
    arg2->vy = arg1->vy - arg0->coord.t[1];
    arg2->vz = arg1->vz - arg0->coord.t[2];
}

static void func_80103CB4(GfxCoord* arg0, s32 arg1, VECTOR3* arg2, VECTOR3* arg3)
{
    u8*     head;
    VECTOR* vec;

    head                         = SCRATCH_STACK_CURSOR(u8);
    vec                          = (VECTOR*)(head - 0x10);
    SCRATCH_STACK_CURSOR(VECTOR) = vec;
    ((VECTOR*)(head - 0x10))->vx = 0;
    vec->vy                      = -0x600;
    vec->vz                      = 0;
    ApplyMatrixLV(&arg0->coord, vec, vec);
    arg3->vx = arg2->vx - (arg0->coord.t[0] + ((VECTOR*)(head - 0x10))->vx);
    SCRATCH_STACK_RELEASE_BYTES(0x10);
    arg3->vy = arg2->vy - (arg0->coord.t[1] + vec->vy);
    arg3->vz = arg2->vz - (arg0->coord.t[2] + vec->vz);
}

s32 func_80103D8C(s32 arg0, s32 arg1)
{
    arg0 = ABS(arg0);
    arg0 = arg0 * arg0;
    arg1 = ABS(arg1);
    arg1 = arg1 * arg1;
    return SquareRoot0(arg0 + arg1);
}

s32 func_80103DD4(VECTOR3* arg0, VECTOR3* arg1)
{
    u8*      head;
    VECTOR3* vec;
    s32      vz;
    s32      absz;
    s32      vx;

    head                          = SCRATCH_STACK_CURSOR(u8);
    ((VECTOR3*)(head - 0x10))->vx = arg0->vx - arg1->vx;
    vec                           = (VECTOR3*)(head - 0x10);
    vec->vy                       = arg0->vy - arg1->vy;
    vz                            = arg0->vz - arg1->vz;
    absz                          = ABS(vz);
    vec->vz                       = vz;
    absz                          = absz * absz;
    vx                            = ((VECTOR3*)(head - 0x10))->vx;
    vx                            = ABS(vx);
    vx                            = vx * vx;
    SCRATCH_STACK_CURSOR(VECTOR3) = vec;
    vx                            = SquareRoot0(vx + absz);
    SCRATCH_STACK_RELEASE_BYTES(0x10);
    return vx;
}

s16 func_80103E7C(s16 arg0, s16 arg1)
{
    void**                           head = SCRATCH_HEAD_ADDR;
    _PlayerActorShortestTurnScratch* candidates;

    SCRATCH_PUSH_AT(head, _PlayerActorShortestTurnScratch);
    candidates            = SCRATCH_HEAD_AT(head, _PlayerActorShortestTurnScratch);
    candidates->direct    = arg1 - arg0;
    candidates->plusTurn  = candidates->direct + ACTOR_TRANSFORM_ANGLE_TURN;
    candidates->minusTurn = candidates->direct - ACTOR_TRANSFORM_ANGLE_TURN;
    if (ABS(candidates->direct) < ABS(candidates->plusTurn) && ABS(candidates->direct) < ABS(candidates->minusTurn)) {
        arg0 = candidates->direct;
    } else if (ABS(candidates->plusTurn) < ABS(candidates->minusTurn)) {
        arg0 = candidates->plusTurn;
    } else {
        arg0 = candidates->minusTurn;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_PlayerActorShortestTurnScratch);
    return arg0;
}

void Gp_TrackLockTarget(Task* arg0)
{
    GameActor*       actor;
    WorldTargetNode* node;
    PlayerStatus*    p;
    s32              val;

    actor = arg0->work;
    node  = actor->targetNode;
    if (node == NULL) {
        actor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
        return;
    }
    if (node->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE) {
        node->state.parts.targeted = 0;
        actor->targetNode          = NULL;
        actor->aimTrackingState    = GAME_ACTOR_AIM_TRACKING_DECAY;
        return;
    }
    if ((s8)actor->aimTrackingState == GAME_ACTOR_AIM_TRACKING_TARGET) {
        p = &gPlayerStatus;
        if (p->weapon == 0x17) {
            val = 0x200;
        } else {
            val = 0x180;
        }
        Gp_AimYawToLock(arg0, val);
        if (p->weapon == 0x17) {
            Gp_AimPitchToLockAlt(arg0);
        } else {
            Gp_AimPitchToLock(arg0);
        }
    }
}

static GfxCoord* func_8010403C(s32 arg0)
{
    Task* slot;
    u8    idx;

    slot = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    idx  = D_80112E2C[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1][arg0];
    return &slot->extra.tmd->coords[idx];
}

void Gp_PlaceCoordOffset(GfxCoord* arg0, GfxCoord* arg1, SVECTOR* arg2)
{
    MATRIX* world;

    arg0->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(arg0);
    arg1->workm = arg0->workm;
    gte_SetRotMatrix(&arg0->workm);
    gte_SetTransMatrix(&arg0->workm);
    gte_ldv0(arg2);
    gte_rtv0tr();
    gte_stlvnl(arg1->workm.t);
    world = &gGfxViewCoord.workm;
    gfxMakeRelativeTransform(world, &arg1->workm, &arg1->coord);
    arg1->parent       = PARENT_OF(world, GfxCoord, workm);
    arg1->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(arg1);
}

s32 func_801041B4(Task* arg0)
{
    GameActor* actor;
    s32        i;

    actor = arg0->work;
    for (i = 0; i < 0x12; i++) {
        if ((actor->collisionContacts[i].key.value & 0x100100) == 0x100000) {
            return 1;
        }
    }
    return 0;
}

static void func_801041FC(Task* arg0, s32 arg1)
{
    GameActor*                   actor;
    _PlayerActorVibrationPreset* preset;
    s32                          idx;

    actor = arg0->work;
    idx   = arg1 & 0xFFFF;
    // Normal-mode state 4 calls this every tick; the latch allows one post.
    if (actor->rumblePosted == 0) {
        actor->rumblePosted++;
        preset = &D_80112E28[idx];
        Pad_PostEvent(0, PAD_VIBRATION_MOTOR_VARIABLE, preset->intensity, preset->durationUnits);
    }
}

Task* func_80104258(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    Task*      task;
    GameActor* actor;
    TmdObject* extra;
    GfxCoord*  coord;
    TmdObject* obj;
    GfxCoord*  saved;
    u8*        table;
    s32        type;

    extra = arg0->extra.tmd;
    actor = arg0->work;
    saved = &extra->coords[D_80112E04[arg2][arg1]];
    table = D_80112DFC;
    type  = gPlayerStatus.resourceVariant - 2;
    task  = Task_Spawn(7, table[arg2 + type] + arg3 * 2 + arg1, 0, 0);
    if (task == NULL) {
        return NULL;
    }
    task->parent            = arg0;
    coord                   = task->extra.tmd->coords;
    coord->parent           = saved;
    coord->param.clearFlags = false;
    obj                     = task->extra.tmd;
    if (actor->companionWork != NULL) {
        obj->texturePageOffset = 4;
        obj->clutRowOffset     = 6;
    } else {
        obj->texturePageOffset = 6;
        obj->clutRowOffset     = 0;
    }
    tmdProcessStream(obj);
    tmdProcessStream(obj);
    return task;
}

Task* func_80104364(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    Task*      task;
    GfxCoord*  saved;
    TmdObject* extra;
    GfxCoord*  coord;
    s32        type;

    saved = arg0->extra.tmd->coords;
    if (arg2 == 0) {
        return NULL;
    }
    type = D_80112DF4[arg1] - 1;
    task = Task_Spawn(7, type + arg2, arg3, 0);
    if (task == NULL) {
        return NULL;
    }
    extra                   = task->extra.tmd;
    task->parent            = arg0;
    coord                   = extra->coords;
    coord->parent           = saved;
    coord->param.clearFlags = true;
    return task;
}

s32 Gp_KillPlayerEffs(void)
{
    Task*      work;
    GameActor* actor;
    Task*      task;

    work  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    actor = work->work;
    if (!work | !actor) {
        return 0;
    }

    task = actor->equipmentTasks[0];
    if (task != NULL) {
        taskKill(task);
        actor->equipmentTasks[0] = NULL;
    }

    task = actor->equipmentTasks[1];
    if (task != NULL) {
        taskKill(task);
        actor->equipmentTasks[1] = NULL;
    }

    task = actor->weaponEffectTask;
    if (task != NULL) {
        taskKill(task);
        actor->weaponEffectTask = NULL;
    }

    Gp_UnlinkObj(&actor->collisionBodies[GAME_ACTOR_BODY_WEAPON]);
    return 1;
}

Task* func_80104490(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    Task*      task;
    GfxCoord*  saved;
    TmdObject* extra;

    saved  = ((GameActor*)arg0->work)->equipmentTasks[1]->extra.tmd->coords;
    arg2 <<= 2;
    arg1  += 0x60;
    task   = Task_Spawn(7, arg2 + arg1, arg3, 0);
    if (task == NULL) {
        return NULL;
    }
    extra                   = task->extra.tmd;
    task->parent            = arg0;
    (extra->coords)->parent = saved;
    return task;
}

s32 func_80104508(Task* task, s32 msgId, AnimationPlayRequest* request, s32 unusedArg3)
{
    GameActor*    actor;
    TmdObject*    extra;
    PlayerStatus* playerStatus;

    actor                                                 = task->work;
    extra                                                 = task->extra.tmd;
    playerStatus                                          = &gPlayerStatus;
    actor->mode                                           = GAME_ACTOR_MODE_SCRIPTED;
    actor->statePhase                                     = 0;
    actor->movementSign                                   = 0;
    actor->turnSign                                       = 0;
    playerStatus->interactionPressed                      = 0;
    actor->aimTrackingState                               = GAME_ACTOR_AIM_TRACKING_OFF;
    actor->part3Pitch                                     = 0;
    actor->part2Pitch                                     = 0;
    actor->part3Roll                                      = 0;
    actor->part2Roll                                      = 0;
    actor->aimYaw                                         = 0;
    actor->field_68                                       = 0;
    actor->part6Pitch                                     = 0;
    actor->hitRegion                                      = 0;
    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    func_80106350(task, playerStatus->weapon, 0);
    if (gGameSession->eventState != 0) {
        actor->collisionBodies[GAME_ACTOR_BODY_ROOT].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED);
    }
    // Select the animation table before resetting or blending its slots.
    actor->state = 1;
    if (actor->animationSets != Gp_PlayerAnimBlkTbl[request->source.index]->table.sets) {
        actor->animationSets = Gp_PlayerAnimBlkTbl[request->source.index]->table.sets;
        animationInitContext(&actor->animationContext, actor->animationSets, extra, actor->poseBuffer,
                             actor->animationSlots);
        actor->animationBankIndex = (u16)request->source.index;
    }
    actor->animationRate = ANIMATION_RATE_ONE;
    if (request->blend == ANIMATION_BLEND_RESET) {
        Gp_AnimResetChildSlots(task, request->animationId);
    } else {
        playerActorPlayChildSlotsWithBlend(task, request->animationId, 0, request->blendFrames);
    }
    if (request->enableWorldCollision == ANIMATION_WORLD_COLLISION_DISABLE) {
        actor->pendingCollisionUpdates = PLAYER_ACTOR_WORLD_COLLISION_DISABLE;
    } else {
        actor->pendingCollisionUpdates = PLAYER_ACTOR_WORLD_COLLISION_ENABLE;
    }
    return 0;
}

s32 func_80104684(Task* arg0, s32 arg1, s32 arg2, s32 unusedSecondArg)
{
    GameActor* actor;
    TmdObject* extra;
    void       (*func)(TmdObject*);
    Task*      node;
    Task*      child;
    Task*      cur;

    actor = arg0->work;
    extra = arg0->extra.tmd;
    func  = NULL;
    switch (arg2) {
        case 0:
            func         = Tmd_AllocBuffers;
            extra->flags = (extra->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW) & (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 1:
            extra->flags = extra->flags & (u16) ~(TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            break;
        case 2:
            func         = Tmd_FreeBuffers;
            extra->flags = extra->flags | (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            break;
        case 3:
            extra->flags = extra->flags | (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            break;
        case 4:
            func         = Tmd_AllocBuffers;
            extra->flags = extra->flags & (u16) ~(TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            break;
    }
    if (func != NULL) {
        func(extra);
    }
    if (actor->attachmentTasks[0] != NULL) {
        actor->attachmentTasks[0]->extra.tmd->flags = extra->flags;
        if (func != NULL) {
            func(extra);
        }
    }
    if (actor->attachmentTasks[1] != NULL) {
        actor->attachmentTasks[1]->extra.tmd->flags = extra->flags;
        if (func != NULL) {
            func(extra);
        }
    }
    if (actor->equipmentTasks[1] != NULL) {
        actor->equipmentTasks[1]->extra.tmd->flags = extra->flags;
        node                                       = actor->equipmentTasks[1];
        node                                       = node->firstChild;
        if (node != NULL) {
            child                   = node;
            child->extra.tmd->flags = extra->flags;
            cur                     = child;
            if (func != NULL) {
                func(extra);
            }
            while (cur->nextSibling != child) {
                cur                   = cur->nextSibling;
                cur->extra.tmd->flags = extra->flags;
                if (func != NULL) {
                    func(extra);
                }
            }
        }
    }
    return 0;
}

s32 Gp_EnterActorMode2(Task* arg0, s32 arg1, s32 arg2, s32 unusedArg3)
{
    TmdObject* extra;
    GameActor* actor;
    GfxCoord*  coord;
    GfxCoord*  next;
    u16        mode;
    VECTOR     vec;

    extra = arg0->extra.tmd;
    actor = arg0->work;
    coord = extra->coords;
    mode  = actor->mode;
    next  = coord + 1;
    if (mode != 2) {
        return 1;
    }
    if (arg2 != mode) {
        vec.vx = next->coord.t[0];
        vec.vy = next->coord.t[1];
        vec.vz = next->coord.t[2];
        ApplyMatrixLV(&coord->coord, &vec, &vec);
        coord->coord.t[0] += vec.vx;
        coord->coord.t[2] += vec.vz;
        next->coord.t[0]   = 0;
        next->coord.t[2]   = 0;
    }
    actor->previousPosition.vx                          = coord->coord.t[0];
    actor->previousPosition.vy                          = coord->coord.t[1];
    actor->previousPosition.vz                          = coord->coord.t[2];
    actor->animationBankIndex                           = Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon;
    actor->animationSets                                = Gp_PlayerAnimBlkTbl[actor->animationBankIndex]->table.sets;
    actor->animationRate                                = ANIMATION_RATE_ONE;
    actor->pendingCollisionUpdates                      = GAME_ACTOR_COLLISION_REQUEST_MASK;
    actor->collisionBodies[GAME_ACTOR_BODY_ROOT].flags |= WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED;
    if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
        animationInitContext(&actor->animationContext, actor->animationSets, extra, actor->poseBuffer,
                             actor->animationSlots);
        if (arg2 == mode) {
            Gp_ResetActorAnimState(arg0, 0);
        } else {
            func_8010870C(arg0, 0);
        }
        return 0;
    }
    if (arg2 == mode) {
        func_80108874(arg0);
        return 0;
    }
    if (arg2 == 1) {
        animationInitContext(&actor->animationContext, actor->animationSets, extra, actor->poseBuffer,
                             actor->animationSlots);
        func_801066DC(arg0, 1);
    } else {
        func_801066DC(arg0, 0);
    }
    return 0;
}

static void func_80104A4C(Task* arg0)
{
    GameActor*    actor;
    PlayerStatus* p;

    actor                 = arg0->work;
    p                     = &gPlayerStatus;
    p->interactionPressed = 0;
    if (actor->mode != GAME_ACTOR_MODE_SCRIPTED) {
        if (actor->mode == GAME_ACTOR_MODE_NORMAL) {
            if (actor->state == 0 || actor->state == 2) {
                if (actor->padPressed & 0x20) {
                    p->interactionPressed = 1;
                }
            }
        }
    }
}

static void func_80104AAC(Task* arg0)
{
    GameActor*    actor;
    PlayerStatus* p;

    actor                                                 = arg0->work;
    p                                                     = &gPlayerStatus;
    actor->mode                                           = GAME_ACTOR_MODE_SCRIPTED;
    actor->statePhase                                     = 0;
    actor->movementSign                                   = 0;
    actor->turnSign                                       = 0;
    p->interactionPressed                                 = 0;
    actor->aimTrackingState                               = GAME_ACTOR_AIM_TRACKING_OFF;
    actor->part3Pitch                                     = 0;
    actor->part2Pitch                                     = 0;
    actor->part3Roll                                      = 0;
    actor->part2Roll                                      = 0;
    actor->aimYaw                                         = 0;
    actor->field_68                                       = 0;
    actor->part6Pitch                                     = 0;
    actor->hitRegion                                      = 0;
    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    func_80106350(arg0, p->weapon, 0);
    if (gGameSession->eventState != 0) {
        actor->collisionBodies[GAME_ACTOR_BODY_ROOT].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED);
    }
}

s32 func_80104B54(Task* task, s32 msgId, AnimationPlayRequest* request, s32 unusedSecondArg)
{
    GameActor*    actor;
    TmdObject*    extra;
    PlayerStatus* playerStatus;

    actor                                                 = task->work;
    extra                                                 = task->extra.tmd;
    playerStatus                                          = &gPlayerStatus;
    actor->mode                                           = GAME_ACTOR_MODE_SCRIPTED;
    actor->statePhase                                     = 0;
    actor->movementSign                                   = 0;
    actor->turnSign                                       = 0;
    playerStatus->interactionPressed                      = 0;
    actor->aimTrackingState                               = GAME_ACTOR_AIM_TRACKING_OFF;
    actor->part3Pitch                                     = 0;
    actor->part2Pitch                                     = 0;
    actor->part3Roll                                      = 0;
    actor->part2Roll                                      = 0;
    actor->aimYaw                                         = 0;
    actor->field_68                                       = 0;
    actor->part6Pitch                                     = 0;
    actor->hitRegion                                      = 0;
    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    func_80106350(task, playerStatus->weapon, 0);
    if (gGameSession->eventState != 0) {
        actor->collisionBodies[GAME_ACTOR_BODY_ROOT].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED);
    }
    // Select the animation table before resetting or blending its slots.
    actor->state              = 1;
    actor->animationSets      = request->source.sets;
    actor->animationBankIndex = PLAYER_ACTOR_DIRECT_ANIMATION_BANK;
    actor->animationRate      = ANIMATION_RATE_ONE;
    if (request->blend == ANIMATION_BLEND_RESET) {
        animationInitContext(&actor->animationContext, actor->animationSets, extra, actor->poseBuffer,
                             actor->animationSlots);
        Gp_AnimResetChildSlots(task, request->animationId);
    } else {
        playerActorPlayChildSlotsWithBlend(task, request->animationId, 0, request->blendFrames);
    }
    if (request->enableWorldCollision == ANIMATION_WORLD_COLLISION_DISABLE) {
        actor->pendingCollisionUpdates = PLAYER_ACTOR_WORLD_COLLISION_DISABLE;
    } else {
        actor->pendingCollisionUpdates = PLAYER_ACTOR_WORLD_COLLISION_ENABLE;
    }
    return 0;
}

s32 func_80104CAC(Task* task, s32 msgId, AnimationPlayRequest* request, s32 unusedSecondArg)
{
    GameActor* actor;
    TmdObject* extra;
    s32        collisionUpdateMask;

    actor = task->work;
    extra = task->extra.tmd;
    // Replace playback without clearing the player's scripted state.
    actor->animationSets      = request->source.sets;
    actor->animationBankIndex = PLAYER_ACTOR_DIRECT_ANIMATION_BANK;
    actor->animationRate      = ANIMATION_RATE_ONE;
    if (request->blend == ANIMATION_BLEND_RESET) {
        animationInitContext(&actor->animationContext, actor->animationSets, extra, actor->poseBuffer,
                             actor->animationSlots);
        Gp_AnimResetChildSlots(task, request->animationId);
    } else {
        playerActorPlayChildSlotsWithBlend(task, request->animationId, 0, request->blendFrames);
    }
    collisionUpdateMask = request->enableWorldCollision;
    if (collisionUpdateMask == ANIMATION_WORLD_COLLISION_DISABLE) {
        collisionUpdateMask = PLAYER_ACTOR_WORLD_COLLISION_DISABLE;
    } else {
        collisionUpdateMask = PLAYER_ACTOR_WORLD_COLLISION_ENABLE;
    }
    actor->pendingCollisionUpdates = collisionUpdateMask;
    return 0;
}

s32 func_80104D68(Task* arg0, s32 arg1, ActorTransform* transform, s32 unusedSecondArg)
{
    TmdObject* extra;
    GameActor* actor;
    GfxCoord*  coord;
    MATRIX*    mtx;

    extra              = arg0->extra.tmd;
    actor              = (GameActor*)arg0->work;
    coord              = extra->coords;
    coord->coord.t[0]  = transform->pos.vx;
    coord->coord.t[1]  = transform->pos.vy;
    coord->coord.t[2]  = transform->pos.vz;
    actor->rotation.vx = transform->rot.vx;
    actor->rotation.vy = transform->rot.vy;
    actor->rotation.vz = transform->rot.vz;
    mtx                = &coord->coord;
    RotMatrix(&actor->rotation, mtx);
    MatrixNormal(mtx, mtx);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    return 0;
}

/// Puts the player in `mode` mode 2 (`Gp_TickPlayerMode2`): clears the
/// movement state and the HUD flag, re-applies the equipped weapon, and during
/// an event clears flag 0x2000 on the actor's first object.
static inline void _gpSwitchToPlayerMode2(Task* arg0)
{
    GameActor*    actor;
    PlayerStatus* p;

    actor                                                 = arg0->work;
    p                                                     = &gPlayerStatus;
    actor->mode                                           = GAME_ACTOR_MODE_SCRIPTED;
    actor->statePhase                                     = 0;
    actor->movementSign                                   = 0;
    actor->turnSign                                       = 0;
    p->interactionPressed                                 = 0;
    actor->aimTrackingState                               = GAME_ACTOR_AIM_TRACKING_OFF;
    actor->part3Pitch                                     = 0;
    actor->part2Pitch                                     = 0;
    actor->part3Roll                                      = 0;
    actor->part2Roll                                      = 0;
    actor->aimYaw                                         = 0;
    actor->field_68                                       = 0;
    actor->part6Pitch                                     = 0;
    actor->hitRegion                                      = 0;
    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    func_80106350(arg0, p->weapon, 0);
    if (gGameSession->eventState != 0) {
        actor->collisionBodies[GAME_ACTOR_BODY_ROOT].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED);
    }
}

s32 func_80104E00(Task* arg0, s32 arg1, ActorTransform* transform, s32 unusedArg3)
{
    GameActor* actor;
    s32*       head;
    s32        val;
    s32        mode;
    s16        angle;

    actor                      = arg0->work;
    head                       = SCRATCH_STACK_CURSOR(s32);
    SCRATCH_STACK_CURSOR(void) = head - 4;
    _gpSwitchToPlayerMode2(arg0);
    actor->scriptedMotionPending   = 1;
    actor->state                   = 2;
    actor->pendingCollisionUpdates = 0x38;
    angle                          = transform->rot.vy;
    actor->scriptMotion.targetYaw  = angle;
    val                            = func_80103E7C(actor->rotation.vy, angle);
    head[-4]                       = val;
    mode                           = 6;
    if (val < 0) {
        mode = 5;
    }
    Gp_AnimPlayChildSlots(arg0, mode, 0);
    SCRATCH_STACK_RELEASE_BYTES(0x10);
    return 0;
}

s32 func_80104F5C(Task* arg0, s32 arg1, GameActorStairClimb* climb, s32 unusedSecondArg)
{
    GameActor*    actor;
    PlayerStatus* p;
    s32           mode;

    actor                                                 = arg0->work;
    p                                                     = &gPlayerStatus;
    actor->mode                                           = GAME_ACTOR_MODE_SCRIPTED;
    actor->statePhase                                     = 0;
    actor->movementSign                                   = 0;
    actor->turnSign                                       = 0;
    p->interactionPressed                                 = 0;
    actor->aimTrackingState                               = GAME_ACTOR_AIM_TRACKING_OFF;
    actor->part3Pitch                                     = 0;
    actor->part2Pitch                                     = 0;
    actor->part3Roll                                      = 0;
    actor->part2Roll                                      = 0;
    actor->aimYaw                                         = 0;
    actor->field_68                                       = 0;
    actor->part6Pitch                                     = 0;
    actor->hitRegion                                      = 0;
    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    func_80106350(arg0, p->weapon, 0);
    if (gGameSession->eventState != 0) {
        actor->collisionBodies[GAME_ACTOR_BODY_ROOT].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED);
    }
    actor->state                   = 3;
    actor->scriptedMotionPending   = 1;
    actor->pendingCollisionUpdates = 0x38;
    actor->jumpVariant             = climb->descend;
    actor->scriptMotion.jumpSteps  = climb->stepCount;
    mode                           = 0x24;
    if (climb->descend != 0) {
        mode = 0x25;
    }
    Gp_AnimPlayChildSlots(arg0, mode, 0);
    return 0;
}

s32 Gp_SetActorDest(Task* arg0, s32 arg1, ActorTransform* transform, GameActorMoveAnim* moveAnim)
{
    GameActor*    actor;
    PlayerStatus* p;

    actor                                                 = arg0->work;
    p                                                     = &gPlayerStatus;
    actor->mode                                           = GAME_ACTOR_MODE_SCRIPTED;
    actor->statePhase                                     = 0;
    actor->movementSign                                   = 0;
    actor->turnSign                                       = 0;
    p->interactionPressed                                 = 0;
    actor->aimTrackingState                               = GAME_ACTOR_AIM_TRACKING_OFF;
    actor->part3Pitch                                     = 0;
    actor->part2Pitch                                     = 0;
    actor->part3Roll                                      = 0;
    actor->part2Roll                                      = 0;
    actor->aimYaw                                         = 0;
    actor->field_68                                       = 0;
    actor->part6Pitch                                     = 0;
    actor->hitRegion                                      = 0;
    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    func_80106350(arg0, p->weapon, 0);
    if (gGameSession->eventState != 0) {
        actor->collisionBodies[GAME_ACTOR_BODY_ROOT].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED);
    }
    actor->state                   = 4;
    actor->scriptedMotionPending   = 1;
    actor->pendingCollisionUpdates = 0x38;
    actor->destination.vx          = transform->pos.vx;
    actor->destination.vy          = transform->pos.vy;
    actor->destination.vz          = transform->pos.vz;
    // A null record, or a zero word, selects that clip's default. The low 16 bits are kept.
    if (moveAnim != NULL) {
        actor->actionArgument = moveAnim->approachAnimId;
        actor->actionValue    = moveAnim->arrivalAnimId;
    } else {
        actor->actionArgument = 0;
        actor->actionValue    = 0;
    }
    return 0;
}

s32 func_80105190(Task* arg0, s32 arg1, ActorTransform* transform, GameActorMoveAnim* moveAnim)
{
    GameActor*    actor;
    PlayerStatus* p;

    actor                                                 = arg0->work;
    p                                                     = &gPlayerStatus;
    actor->mode                                           = GAME_ACTOR_MODE_SCRIPTED;
    actor->statePhase                                     = 0;
    actor->movementSign                                   = 0;
    actor->turnSign                                       = 0;
    p->interactionPressed                                 = 0;
    actor->aimTrackingState                               = GAME_ACTOR_AIM_TRACKING_OFF;
    actor->part3Pitch                                     = 0;
    actor->part2Pitch                                     = 0;
    actor->part3Roll                                      = 0;
    actor->part2Roll                                      = 0;
    actor->aimYaw                                         = 0;
    actor->field_68                                       = 0;
    actor->part6Pitch                                     = 0;
    actor->hitRegion                                      = 0;
    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    func_80106350(arg0, p->weapon, 0);
    if (gGameSession->eventState != 0) {
        actor->collisionBodies[GAME_ACTOR_BODY_ROOT].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED);
    }
    actor->state                   = 4;
    actor->scriptedMotionPending   = 1;
    actor->pendingCollisionUpdates = 0x38;
    actor->destination.vx          = transform->pos.vx;
    actor->destination.vy          = transform->pos.vy;
    actor->destination.vz          = transform->pos.vz;
    // A null record, or a zero word, selects that clip's default. The low 16 bits are kept.
    if (moveAnim != NULL) {
        actor->actionArgument = moveAnim->approachAnimId;
        actor->actionValue    = moveAnim->arrivalAnimId;
    } else {
        actor->actionArgument = 0;
        actor->actionValue    = 0;
    }
    actor->state = 8;
    return 0;
}

s32 func_801052B8(Task* arg0, s32 arg1, GameActorWalkSteps* walkSteps, s32 unusedSecondArg)
{
    GameActor*    actor;
    PlayerStatus* p;

    actor                                                 = arg0->work;
    p                                                     = &gPlayerStatus;
    actor->mode                                           = GAME_ACTOR_MODE_SCRIPTED;
    actor->statePhase                                     = 0;
    actor->movementSign                                   = 0;
    actor->turnSign                                       = 0;
    p->interactionPressed                                 = 0;
    actor->aimTrackingState                               = GAME_ACTOR_AIM_TRACKING_OFF;
    actor->part3Pitch                                     = 0;
    actor->part2Pitch                                     = 0;
    actor->part3Roll                                      = 0;
    actor->part2Roll                                      = 0;
    actor->aimYaw                                         = 0;
    actor->field_68                                       = 0;
    actor->part6Pitch                                     = 0;
    actor->hitRegion                                      = 0;
    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    func_80106350(arg0, p->weapon, 0);
    if (gGameSession->eventState != 0) {
        actor->collisionBodies[GAME_ACTOR_BODY_ROOT].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED);
    }
    actor->state                   = 5;
    actor->scriptedMotionPending   = 1;
    actor->pendingCollisionUpdates = 0x38;
    actor->actionValue             = walkSteps->stepCount;
    actor->stateTimer              = walkSteps->field_4;
    return 0;
}

s32 Gp_MoveActorBy(Task* arg0, s32 arg1, GameActorMoveBy* move, s32 unusedSecondArg)
{
    GameActor*    actor;
    GfxCoord*     coord;
    PlayerStatus* p;

    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    if (move->keepControl == 0) {
        p                                                     = &gPlayerStatus;
        actor->mode                                           = GAME_ACTOR_MODE_SCRIPTED;
        actor->statePhase                                     = 0;
        actor->movementSign                                   = 0;
        actor->turnSign                                       = 0;
        p->interactionPressed                                 = 0;
        actor->aimTrackingState                               = GAME_ACTOR_AIM_TRACKING_OFF;
        actor->part3Pitch                                     = 0;
        actor->part2Pitch                                     = 0;
        actor->part3Roll                                      = 0;
        actor->part2Roll                                      = 0;
        actor->aimYaw                                         = 0;
        actor->field_68                                       = 0;
        actor->part6Pitch                                     = 0;
        actor->hitRegion                                      = 0;
        actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
        func_80106350(arg0, p->weapon, 0);
        if (gGameSession->eventState != 0) {
            actor->collisionBodies[GAME_ACTOR_BODY_ROOT].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED);
        }
        actor->state                 = 1;
        actor->scriptedMotionPending = 1;
    }
    actor->pendingCollisionUpdates = move->collisionRequests;
    coord->coord.t[0]             += move->displacement.vx;
    coord->coord.t[1]             += move->displacement.vy;
    coord->coord.t[2]             += move->displacement.vz;
    Gp_ApplyDirArg(arg0, move);
    return func_801041B4(arg0);
}

s32 func_801054D8(Task* arg0, s32 arg1, GameActorButtonPressHold* arg2, s32 unusedSecondArg)
{
    GameActor*    actor;
    PlayerStatus* p;

    actor = arg0->work;
    if ((s8)actor->recoveryTicks != 0) {
        return 1;
    }
    p                                                     = &gPlayerStatus;
    actor->mode                                           = GAME_ACTOR_MODE_SCRIPTED;
    actor->statePhase                                     = 0;
    actor->movementSign                                   = 0;
    actor->turnSign                                       = 0;
    p->interactionPressed                                 = 0;
    actor->aimTrackingState                               = GAME_ACTOR_AIM_TRACKING_OFF;
    actor->part3Pitch                                     = 0;
    actor->part2Pitch                                     = 0;
    actor->part3Roll                                      = 0;
    actor->part2Roll                                      = 0;
    actor->aimYaw                                         = 0;
    actor->field_68                                       = 0;
    actor->part6Pitch                                     = 0;
    actor->hitRegion                                      = 0;
    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    func_80106350(arg0, p->weapon, 0);
    if (gGameSession->eventState != 0) {
        actor->collisionBodies[GAME_ACTOR_BODY_ROOT].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED);
    }
    actor->state       = 6;
    Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
    actor->stateTimer  = arg2->pressCount;
    actor->actionValue = 0;
    return 0;
}

s32 func_801055D4(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    GameActor*    actor;
    PlayerStatus* p;

    actor                                                 = arg0->work;
    p                                                     = &gPlayerStatus;
    actor->mode                                           = GAME_ACTOR_MODE_SCRIPTED;
    actor->statePhase                                     = 0;
    actor->movementSign                                   = 0;
    actor->turnSign                                       = 0;
    p->interactionPressed                                 = 0;
    actor->aimTrackingState                               = GAME_ACTOR_AIM_TRACKING_OFF;
    actor->part3Pitch                                     = 0;
    actor->part2Pitch                                     = 0;
    actor->part3Roll                                      = 0;
    actor->part2Roll                                      = 0;
    actor->aimYaw                                         = 0;
    actor->field_68                                       = 0;
    actor->part6Pitch                                     = 0;
    actor->hitRegion                                      = 0;
    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    func_80106350(arg0, p->weapon, 0);
    if (gGameSession->eventState != 0) {
        actor->collisionBodies[GAME_ACTOR_BODY_ROOT].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED);
    }
    actor->state                   = 0xA;
    actor->pendingCollisionUpdates = 0x38;
    return 0;
}

s32 func_80105690(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    GameActor*    actor;
    PlayerStatus* p;

    actor                                                 = arg0->work;
    p                                                     = &gPlayerStatus;
    actor->mode                                           = GAME_ACTOR_MODE_SCRIPTED;
    actor->statePhase                                     = 0;
    actor->movementSign                                   = 0;
    actor->turnSign                                       = 0;
    p->interactionPressed                                 = 0;
    actor->aimTrackingState                               = GAME_ACTOR_AIM_TRACKING_OFF;
    actor->part3Pitch                                     = 0;
    actor->part2Pitch                                     = 0;
    actor->part3Roll                                      = 0;
    actor->part2Roll                                      = 0;
    actor->aimYaw                                         = 0;
    actor->field_68                                       = 0;
    actor->part6Pitch                                     = 0;
    actor->hitRegion                                      = 0;
    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    func_80106350(arg0, p->weapon, 0);
    if (gGameSession->eventState != 0) {
        actor->collisionBodies[GAME_ACTOR_BODY_ROOT].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED);
    }
    actor->state      = 7;
    actor->stateTimer = arg2;
    return 0;
}

s32 func_80105754(Task* arg0, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    GameActor*    actor;
    PlayerStatus* p;
    s32           ret;

    actor = arg0->work;
    ret   = 0;
    if (actor->mode != GAME_ACTOR_MODE_SCRIPTED) {
        p                                                     = &gPlayerStatus;
        actor->mode                                           = GAME_ACTOR_MODE_SCRIPTED;
        actor->statePhase                                     = 0;
        actor->movementSign                                   = 0;
        actor->turnSign                                       = 0;
        p->interactionPressed                                 = 0;
        actor->aimTrackingState                               = GAME_ACTOR_AIM_TRACKING_OFF;
        actor->part3Pitch                                     = 0;
        actor->part2Pitch                                     = 0;
        actor->part3Roll                                      = 0;
        actor->part2Roll                                      = 0;
        actor->aimYaw                                         = 0;
        actor->field_68                                       = 0;
        actor->part6Pitch                                     = 0;
        actor->hitRegion                                      = 0;
        actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
        func_80106350(arg0, p->weapon, ret);
        if (gGameSession->eventState != 0) {
            actor->collisionBodies[GAME_ACTOR_BODY_ROOT].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED);
        }
        actor->state = 0xB;
    } else {
        ret = 1;
    }
    return ret;
}

s32 func_80105828(Task* arg0, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    return ((GameActor*)arg0->work)->scriptedMotionPending;
}

s32 func_8010583C(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    GameActor* actor;
    s32        i;
    s32        ret;

    actor = arg0->work;
    ret   = 0;
    for (i = actor->animationSlotCount - 1; i > 0; i--) {
        // Any part not holding its boundary pose means the clip is still running.
        if ((actor->animationSlots[i].status.fields.flags & ANIMATION_SLOT_SETTLED) == 0) {
            ret = 1;
            break;
        }
    }
    return ret;
}

s32 func_80105894(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    AnimationSlot* slot;

    slot = &((GameActor*)arg0->work)->animationSlots[(u32)arg1];
    // Inside a segment: not holding the boundary, and this tick followed no jump.
    return (slot->status.fields.flags & (ANIMATION_SLOT_SETTLED | ANIMATION_SLOT_FOLLOWED_JUMP)) == 0;
}

s32 func_801058BC(Task* arg0, s32 arg1, s32 arg2, s32 unusedSecondArg)
{
    GameActor* actor;
    s32        i;

    actor = arg0->work;
    if (arg2 <= 0) {
        arg2 = 1;
    } else if (arg2 >= 0x80) {
        arg2 = 0x7F;
    }
    i = 1;
    if (i < actor->animationSlotCount) {
        do {
            actor->animationSlots[i].rate = arg2;
            i++;
        } while (i < actor->animationSlotCount);
    }
    actor->animationRate = arg2;
    return 0;
}

s32 Gp_CopyPlayerAnim(Task* arg0, s32 arg1, const AnimationBankCopyRequest* request, s32 unusedSecondArg)
{
    union {
        AnimationBank* block;
        s32*           words;
    } dest;
    const s32* src;
    s32        i;
    s32        count;

    dest.block = Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon];
    src        = request->source.words;
    count      = request->wordCount;
    if (count >= ANIMATION_BANK_EXTENSION_CAPACITY + 1) {
        return 1;
    }
    // Transfer raw words: the span can include records after the clip pointers.
    dest.words = &dest.block->table.words[ANIMATION_BANK_BASE_SET_COUNT];
    for (i = 0; i < request->wordCount; i++) {
        dest.words[i] = src[i];
    }
    return 0;
}

s32 Gp_ApplyPlayerDamage(Task* arg0, s32 arg1, s32 arg2, s32 unusedSecondArg)
{
    GameActor* actor;
    s32        ret;
    s32        out;

    actor = arg0->work;
    ret   = 0;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.cheatMode == 0) {
        ret = Gp_ApplyHpDamage(Gp_ScaleDamage(arg2, 0, &out, 0));
        if (ret != 0) {
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), 0x7DA, 0, 0x7DE);
        } else if (actor->companionWork == 0) {
            func_8010A42C(arg0, (u8)out);
        }
    }
    return ret;
}

s32 func_80105A60(Task* arg0, s32 arg1, GfxCoord* arg2, s32 unusedSecondArg)
{
    Gp_ReparentCoord(arg2, arg0->extra.tmd->coords);
    return 0;
}

s32 func_80105A8C(Task* arg0, s32 arg1, s32 arg2, s32 unusedSecondArg)
{
    GameActor* inner;

    inner = arg0->work;
    if (arg2 == 0) {
        inner->movementMode = 1;
    } else {
        inner->movementMode = 3;
    }
    return 0;
}

s32 func_80105AB0(Task* arg0, s32 arg1, s32 arg2, s32 unusedSecondArg)
{
    GameActor* inner;

    inner = arg0->work;
    if (arg2 == 0) {
        inner->textureSequenceA = 1;
        inner->textureSequenceB = 2;
        inner->textureDelayA    = 0;
        inner->textureDelayB    = 0;
        inner->textureFrameA    = 0;
        inner->textureFrameB    = 0;
    } else if (arg2 < 4) {
        inner->textureSequenceA = arg2 + 1;
        inner->textureDelayA    = 0;
        inner->textureFrameA    = 0;
    } else {
        inner->textureSequenceB = arg2 - 3;
        inner->textureDelayB    = 0;
        inner->textureFrameB    = 0;
    }
    return 0;
}

static void func_80105B0C(Task* arg0)
{
    GameActor* inner;
    s32        i;

    inner = arg0->work;
    i     = 1;
    if (i < inner->animationSlotCount) {
        do {
            Gp_AnimTickSlot2(&inner->animationContext, inner->animationSlots + i);
            i++;
        } while (i < inner->animationSlotCount);
    }
}

void func_80105B74(VECTOR3* arg0)
{
    GameActor* actor;

    actor                         = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    actor->pendingDisplacement.vx = arg0->vx;
    actor->pendingDisplacement.vy = arg0->vy;
    actor->pendingDisplacement.vz = arg0->vz;
}

s32 Gp_PickNearestRec18(WorldCollisionContact* arg0, GfxCoord* arg1, GfxCoord* arg2)
{
    s32                             minDist;
    s32                             idx;
    PlayerActorWeaponImpactScratch* block;
    WorldCollisionContact*          rec;
    s32                             i;
    s32                             bestIdx;
    s32                             dist;

    minDist = 0x7FFFFFFF;
    if (Gp_CountRec18Hi(arg0, 0x30000) != 0) {
        return 0;
    }
    block = SCRATCH_STACK_RESERVE_BLOCK(PlayerActorWeaponImpactScratch);
    for (i = 0, bestIdx = 0; i < 6; i++) {
        rec = &arg0[i];
        if (rec->key.value & 0x100000) {
            dist  = abs(arg1->workm.t[0] - rec->point.vx);
            dist += abs(arg1->workm.t[1] - rec->point.vy);
            dist += abs(arg1->workm.t[2] - rec->point.vz);
            if (dist < minDist) {
                func_800E0FEC(rec, &block->pushback, 1, &idx);
                idx = func_800E1ACC((u8*)&idx);
                if (Gp_RoomParamTables[gGameSession->location.loc.stage - 1][gGameSession->location.loc.area - 1][idx]->weaponImpactEnabled != WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS) {
                    minDist = dist;
                    bestIdx = i;
                }
            }
        }
    }
    if (minDist != 0x7FFFFFFF) {
        i                               = 1;
        block->impactCoord.parent       = NULL;
        block->impactCoord.composeStamp = GRAPHICS_COORD_SUPPLIED_CACHE;
        block->impactCoord.workm.t[0]   = arg0[bestIdx].point.vx;
        block->impactCoord.workm.t[1]   = arg0[bestIdx].point.vy;
        block->impactCoord.workm.t[2]   = arg0[bestIdx].point.vz;
        block->jitter.vx                = rand() & 7;
        block->jitter.vy                = rand() & 7;
        block->jitter.vz                = rand() & 7;
        if (arg2 != NULL) {
            arg2->workm.t[0] = block->impactCoord.workm.t[0] + block->jitter.vx;
            arg2->workm.t[1] = block->impactCoord.workm.t[1] + block->jitter.vy;
            arg2->workm.t[2] = block->impactCoord.workm.t[2] + block->jitter.vz;
        }
        if (gPlayerStatus.weapon != 0x1D) {
            if (gPlayerStatus.weaponSlotItem == 0xE) {
                Gp_SpawnEff(EFFECT_FIRE_BURST, &block->impactCoord, 0x300, &block->jitter);
                Gp_SpawnEff(EFFECT_ADDITIVE_PUFF, &block->impactCoord, 0x300, &block->jitter);
                Gp_SpawnEff(EFFECT_SMOKE_PUFF, &block->impactCoord, 0xC0013300, &block->jitter);
            } else {
                Gp_SpawnEff(EFFECT_IMPACT_SPARK, &block->impactCoord, 0, &block->jitter);
            }
        }
    } else {
        i = 0;
    }
    SCRATCH_STACK_RELEASE_BLOCK(PlayerActorWeaponImpactScratch);
    return i;
}

s32 func_80105ED4(Task* arg0)
{
    // Actor mode is the low halfword and state the high halfword of this selector.
    enum {
        PLAYER_ACTOR_FOOTSTEP_SCRIPTED_JUMP_MODE_STATE = (3 << 16) | GAME_ACTOR_MODE_SCRIPTED
    };
    enum { PLAYER_ACTOR_FOOTSTEP_RUNNING_MOVEMENT_MODE = 3 };
    // The loaded surface-sound bank keeps companion entries 100 slots after the player's.
    enum { PLAYER_ACTOR_FOOTSTEP_COMPANION_ENTRY_OFFSET = 100 };

    GameActor*                          actor;
    const AnimationRecord*              rec;
    GfxCoord*                           obj;
    s32                                 sound;
    s8                                  cueBits;
    s32                                 pan;
    s32                                 index;
    const WorldCollisionFootstepSounds* footstepSounds;

    sound = WORLD_COLLISION_FOOTSTEP_SILENT;
    actor = arg0->work;
    obj   = arg0->extra.tmd->coords + 1;
    rec   = Gp_AnimGetRec(&actor->animationContext, actor->animationSlots + 1);
    if (rec != NULL && rec != actor->lastCueRecord) {
        actor->lastCueRecord = rec;
        switch (cueBits = rec->flags & ANIMATION_RECORD_CUE_MASK) {
            case ANIMATION_RECORD_CUE_1:
            case ANIMATION_RECORD_CUE_2:
                footstepSounds = Gp_RoomParamTables[gGameSession->location.loc.stage - 1][gGameSession->location.loc.area - 1][actor->surfaceClass]->footstepSounds;
                if (footstepSounds != NULL) {
                    if (*(s32*)&actor->mode == PLAYER_ACTOR_FOOTSTEP_SCRIPTED_JUMP_MODE_STATE) {
                        sound = footstepSounds->scriptedJump;
                    } else if ((u16)actor->movementMode == PLAYER_ACTOR_FOOTSTEP_RUNNING_MOVEMENT_MODE) {
                        sound = footstepSounds->run;
                        Gp_SetStateF0Bit(5);
                    } else {
                        sound = footstepSounds->walk;
                    }
                    // Select the paired cue and companion entry only for a non-silent base.
                    if (sound != WORLD_COLLISION_FOOTSTEP_SILENT) {
                        if (cueBits == ANIMATION_RECORD_CUE_1) {
                            sound++;
                        }
                        if (actor->companionWork != NULL) {
                            sound += PLAYER_ACTOR_FOOTSTEP_COMPANION_ENTRY_OFFSET;
                        }
                        pan = (s8)worldCoordGetOriginAudioPan(obj);
                        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(obj));
                    }
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        index = 0x12;
                        if (actor->companionWork != NULL) {
                            index = 0x13;
                        }
                        if (cueBits == ANIMATION_RECORD_CUE_1) {
                            index -= 3;
                        }
                        Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[index], 0x80002300, NULL);
                    }
                }
                break;
        }
    }
    return sound;
}

s32 func_801060E0(Task* arg0)
{
    GameActor* actor;
    u16        mode;
    s32        flags;
    s32        mask1;
    s32        mask2;

    actor = arg0->work;
    mode  = actor->mode;
    if (mode != 2) {
        flags = actor->padHeld;
        mask1 = 8;
        mask2 = 2;
    } else {
        flags = gPadStates[0].buttons;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout == mode) {
            mask1 = 0x80;
            mask2 = 0x10;
        } else {
            mask1 = 8;
            mask2 = 2;
        }
    }
    actor->attackButton = 0;
    if (flags & mask1) {
        actor->attackButton = 1;
    } else if (flags & mask2) {
        actor->attackButton = 2;
    }
    return actor->attackButton;
}

/// Per-weapon handlers, indexed by `PlayerStatus::weapon` and copied by
/// `func_8010615C`. Most live in the weapon overlay loaded at the time;
/// `func_801065A0` serves the weapons with none.
static const _PlayerActorWeaponAttacks D_800978BC = { {
    func_801065A0,
    func_p08_snail_8011D1D8,
    func_m93r_8011D1C4,
    func_m950_8011D1DC,
    func_p08_8011D1D8,
    func_p229_8011DDA0,
    func_801065A0,
    func_801065A0,
    func_801065A0,
    func_mongoose_8011D1D8,
    func_801065A0,
    func_grenade_pistol_8011D1D4,
    func_mm1_8011D1D4,
    func_pa3_8011D1DC,
    func_sp12_8011D1DC,
    func_as12_8011D1DC,
    func_m4a1_8011D1C4,
    func_m249_8011D1DC,
    func_801065A0,
    func_tonfa_baton_8011DBFC,
    func_m4a1_p1_8011D1C4,
    func_m4a1_p2_8011D1C4,
    func_hypervelocity_8011F724,
    func_gunblade_8011E040,
    func_801065A0,
    func_m4a1_hammer_8011E710,
    func_m4a1_bayonet_8011DA34,
    func_m4a1_grenade_8011D1EC,
    func_m4a1_pyke_8011E4F8,
    func_m4a1_javelin_8011F5D4,
    func_mp5a5_8011DDA4,
    func_mp5a5_p1_8011DDA4,
    func_mp5a5_p2_8011DDA4,
} };

static void func_8010615C(Task* arg0)
{
    GameActor*                actor;
    _PlayerActorWeaponAttacks weaponAttacks;

    weaponAttacks        = D_800978BC;
    actor                = arg0->work;
    actor->actionPadMask = 0xF89A;
    actor->movementSign  = 0;
    weaponAttacks.attacks[gPlayerStatus.weapon](arg0);
}

void func_801061F0(void)
{
    GameActor* actor = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;

    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key = 0x20000 | (gPlayerStatus.weapon << 8) | gPlayerStatus.weaponSlotItem;
}

void func_80106238(Task* arg0, s32 arg1, s32 arg2)
{
    GameActor* actor;

    actor                                              = arg0->work;
    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key = (actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key & 0xFFFF3FFF) | (((arg1 << 1) | arg2) << 14);
}

s32 func_80106264(s32 arg0)
{
    s32 item;
    s32 ret;

    item = gPlayerStatus.weapon + 0x7F;
    ret  = 0;
    if (arg0 & 1) {
        ret = Gp_ConsumeSlotQty(item, 0);
    }
    if (arg0 & 2) {
        ret |= Gp_ConsumeSlotQty(item, 0x100) << 16;
    }
    return ret;
}

static s32 func_801062DC(Task* arg0, s32 arg1)
{
    s32 ret;
    s32 flag;
    s32 item;

    ret  = 0;
    item = gPlayerStatus.weapon;
    flag = arg1 != 1;
    if (Gp_UnequipRelated(item + 0x7F, flag) == 1) {
        func_801088D4(arg0, flag, ret);
        ret = 1;
    }
    return ret;
}

void func_80106350(Task* arg0, s32 arg1, s32 arg2)
{
    GameActor* actor;
    s32        value;

    actor = arg0->work;
    if (arg1 == 0x16) {
        if (actor->weaponEffectTask != NULL) {
            actor->weaponEffectTask->spawnArg1.value = -1;
        }
        SndEvt_EnqueueType7(SOUND_HYPERVELOCITY_CHARGE_START, 0);
        SndEvt_EnqueueType7(SOUND_HYPERVELOCITY_CHARGE_CANCEL, 0);
        SndEvt_EnqueueType7(SOUND_HYPERVELOCITY_CHARGE_LOOP, 0);
    } else if (arg1 == 0x19) {
        if (actor->weaponEffectTask != NULL) {
            if (Gp_ConsumeSlotQty(0x98, 0x100) != 0) {
                actor->weaponEffectTask->spawnArg1.value = 1;
            } else {
                actor->weaponEffectTask->spawnArg1.value = 0;
            }
        }
    } else if (arg1 == 0x1C) {
        if (actor->weaponEffectTask != NULL) {
            if (Gp_ConsumeSlotQty(0x9B, 0x100) != 0) {
                value = 1;
                if (actor->weaponEffectTask->spawnArg1.value == 2) {
                    value = 3;
                }
                actor->weaponEffectTask->spawnArg1.value = value;
            } else {
                actor->weaponEffectTask->spawnArg1.value = (actor->weaponEffectTask->spawnArg1.value == 2) << 2;
            }
            if (actor->companionWork == NULL) {
                SndEvt_EnqueueType7(SOUND_PYKE_FIRE_TAIL, 0);
            } else {
                SndEvt_EnqueueType7(SOUND_COMPANION_PYKE_FIRE_TAIL, 0);
            }
        }
    }
    actor->aimTrackingState                               = GAME_ACTOR_AIM_TRACKING_DECAY;
    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
}

void Gp_PlayObjSfx(GfxCoord* coord, s32 sfx, s32 arg2)
{
    s32 temp;

    temp = (s8)worldCoordGetOriginAudioPan(coord);
    sndEvtRequestScriptStart(sfx, temp, (s8)worldCoordGetOriginAudioDepth(coord));
    if (arg2 == 1) {
        Gp_SetStateF0Bit(1);
    }
}

void func_80106518(s32 arg0)
{
    arg0--;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponUseCounts[arg0] < 99999) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponUseCounts[arg0]++;
    }
}

void func_80106550(Task* arg0)
{
    if (((GameActor*)arg0->work)->aimControl & GAME_ACTOR_AIM_REQUEST_SCRIPTED) {
        func_801055D4(arg0, 0, 0, 0);
    } else {
        Gp_ResetActorAnimState(arg0, 3);
    }
}

static void func_801065A0(Task* task)
{
}

static void func_801065A8(Task* arg0)
{
    GameActor* inner;

    inner = arg0->work;
    func_80109374(arg0);
    if (inner->aimControl & GAME_ACTOR_AIM_REQUEST_ENTER) {
        inner->aimTransitionPending = 1;
        func_8010870C(arg0, 5);
    } else if (inner->movementSign != inner->previousMovementSign ||
               (inner->runButtonHeld != inner->previousRunButtonHeld && inner->movementSign == 1)) {
        func_801066DC(arg0, 0);
    } else if (inner->movementSign == 0 && inner->turnSign != inner->previousTurnSign) {
        func_80108620(arg0);
    } else if ((inner->padHeld & 0xF000) == 0) {
        if (inner->idleTicks < 0x7FFF) {
            inner->idleTicks++;
            if (inner->idleTicks == 0x12C) {
                playerActorPlayChildSlotsWithBlend(arg0, Gp_HpBand() + 0x17, 0, 5);
            }
        }
    }
}

void func_801066DC(Task* arg0, s16 arg1)
{
    GameActor* inner;
    s32        mode;
    s32        temp;

    inner                 = arg0->work;
    temp                  = inner->movementSign;
    inner->state          = 0;
    inner->animationState = 0;
    if (temp == 0) {
        if (inner->turnSign != 0) {
            if (inner->turnSign == 1) {
                mode = 6;
            } else {
                mode = 5;
            }
        } else {
            mode = 1;
        }
        inner->movementMode  = 0;
        inner->turnRateIndex = 3;
    } else if ((inner->padHeld & 0x40) && (temp != -1)) {
        temp                 = 1;
        inner->turnRateIndex = temp;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.moveMode == 0 && inner->restrictRunAndAim == 0) {
            inner->movementMode = 3;
            mode                = 4;
        } else {
            mode                = 2;
            inner->movementMode = temp;
            if (inner->equipmentTasks[1] == NULL) {
                mode = 0x13;
            }
        }
    } else {
        inner->turnRateIndex = 1;
        if (inner->movementSign == 1) {
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.moveMode != 0 && inner->restrictRunAndAim == 0) {
                inner->movementMode = 3;
                mode                = 4;
            } else {
                inner->movementMode = 1;
                mode                = 2;
                if (inner->equipmentTasks[1] == NULL) {
                    mode = 0x13;
                }
            }
        } else {
            inner->movementMode = 2;
            mode                = 3;
        }
    }
    inner->mode       = GAME_ACTOR_MODE_NORMAL;
    inner->statePhase = 0;
    inner->idleTicks  = 0;
    if (arg1 != 0) {
        Gp_AnimResetChildSlots(arg0, mode);
    } else {
        playerActorPlayChildSlotsWithBlend(arg0, mode, 0, 4);
    }
}

/// `mode` dispatcher: `Gp_TickPlayerNormal`, `Gp_TickPlayerMode1`, `Gp_TickPlayerMode2`.
static const TaskFuncTable3 Gp_PlayerModeFns = { {
    Gp_TickPlayerNormal,
    Gp_TickPlayerMode1,
    Gp_TickPlayerMode2,
} };

/// `state` dispatcher copied by `Gp_TickPlayerNormal`.
static const TaskFuncTable8 D_8009794C = { {
    func_80108FA0,
    Gp_PlayerNormalState1,
    Gp_PlayerNormalState2,
    func_801090E8,
    func_80109138,
    Gp_PlayerNormalState5,
    Gp_PlayerNormalState6,
    func_8010771C,
} };

static void Gp_TickPlayerNormal(Task* arg0)
{
    GameActor*     actor;
    GameActor*     inner;
    PlayerStatus*  p;
    u16            prev;
    TaskFuncTable8 sp;

    sp    = D_8009794C;
    actor = arg0->work;
    if (gPlayerStatus.statusFlags & PLAYER_STATUS_CONFUSION) {
        func_8010A670(arg0);
    }
    if (actor->movementInputDisabled == 0) {
        func_80109250(arg0);
        func_80109210(arg0);
    } else {
        actor->movementSign = 0;
        actor->turnSign     = 0;
    }
    p = &gPlayerStatus;
    if (p->statusFlags & PLAYER_STATUS_PARALYSIS) {
        if (actor->state != 7) {
            actor->paralysisProgress++;
            if ((s8)actor->paralysisProgress >= 0x5A) {
                inner                 = arg0->work;
                prev                  = inner->state;
                inner->mode           = GAME_ACTOR_MODE_NORMAL;
                inner->state          = 7;
                inner->movementMode   = 0;
                inner->turnRateIndex  = 0;
                inner->animationState = 0;
                inner->statePhase     = 0;
                inner->rumblePosted   = 0;
                inner->movementSign   = 0;
                inner->turnSign       = 0;
                inner->stateAux       = prev;
                Gp_DetachLinkNode(arg0);
                inner->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                Gp_StateC08.flags                                    |= ATTACHMENT_FLAG_EVENT_LOCK;
                func_80106350(arg0, p->weapon, 0);
                playerActorPlayChildSlotsWithBlend(arg0, 0x19, 3, 6);
            }
        }
    }
    sp.funcs[actor->state](arg0);
    func_80109FC4(arg0);
    Gp_PlayerStepSfx(arg0);
    Gp_TickActorAnimState(arg0);
    Gp_AnimTickChildSlots(arg0);
    Gp_TurnPlayer(arg0);
    Gp_StepPlayerMove(arg0);
    if (gPlayerStatus.hp <= 0) {
        Gp_StopPlayerAnim(arg0, 4);
    }
}

static void Gp_PlayerNormalState2(Task* arg0)
{
    GameActor* actor;
    s32        dir;
    s32        res;
    u8         item;
    u16        pad;
    s32        variant;
    s32        val;
    s32        base;

    actor = arg0->work;
    if (actor->movementSign != actor->previousMovementSign) {
        Gp_ResetActorAnimState(arg0, 4);
    } else if (actor->movementSign == 0 && actor->turnSign != actor->previousTurnSign) {
        func_80108684(arg0);
    }
    if (func_80109290(arg0) == 0) {
        if ((actor->padHeld & 0xF000) == 0) {
            Gp_TrackLockTarget(arg0);
        }
        Gp_UpdateLockTarget(arg0);
        if ((s8)func_801060E0(arg0) != 0 &&
            Gp_AnimGetRec(&actor->animationContext, actor->animationSlots + 1) != NULL &&
            actor->attackControl.cooldownTicks == 0) {
            dir = D_80112EF8[gPlayerStatus.weapon] != 0 ? actor->attackButton : 1;
            res = func_80106264(dir);
            if (res > 0 ||
                (item = actor->attackButton,
                 D_80112F1C[gPlayerStatus.weapon][(u8)(item - 1)] != 0)) {
                if (gPlayerStatus.statusFlags & PLAYER_STATUS_BERSERKER) {
                    Gp_ApplyHpDamage(2);
                }
                if (gPlayerStatus.hp > 0) {
                    actor->aimControl = GAME_ACTOR_AIM_REQUEST_ENTER;
                    actor->statePhase = 0;
                    func_8010615C(arg0);
                }
            } else if (res == 0) {
                pad = actor->padPressed;
                if ((s8)item == 1 ? (pad & 8) : (pad & 2)) {
                    actor->attackControl.cooldownTicks = 0xA;
                    if (func_801062DC(arg0, dir) == 0) {
                        func_801095BC(&variant);
                        base = gPlayerStatus.weapon << 16;
                        val  = variant | 0x20000001;
                        Gp_PlayObjSfx(arg0->extra.tmd->coords, base | val, 0);
                    }
                }
            }
        }
    }
    func_80105ED4(arg0);
}

static void Gp_PlayerNormalState5(Task* arg0)
{
    GameActor*             actor;
    GameActor*             inner;
    const AnimationRecord* rec;
    GfxCoord*              coord;
    s32                    base;
    s32                    done;
    s32                    mode;
    s32                    temp;
    s32                    flags;
    s32                    tick;
    s32                    step;
    u8                     item;
    u16                    next;
    s32                    variant;

    actor               = arg0->work;
    done                = 0;
    coord               = actor->equipmentTasks[1]->extra.tmd->coords;
    base                = gPlayerStatus.weapon << 16;
    actor->movementSign = 0;
    func_801095BC(&variant);
    if (actor->actionValue != 2 && (actor->padPressed & 0x40) && actor->statePhase != 0x64) {
        actor->reloadEffectSuppressed = 1;
        inner                         = arg0->work;
        inner->mode                   = GAME_ACTOR_MODE_NORMAL;
        inner->state                  = 2;
        inner->movementMode           = 0;
        if (inner->movementSign != 0) {
            temp = 1;
        } else {
            temp = 3;
        }
        inner->turnRateIndex  = temp;
        inner->animationState = 0;
        inner->statePhase     = 0;
        if (gPlayerStatus.statusFlags & PLAYER_STATUS_DARKNESS) {
            Gp_DetachLinkNode(arg0);
            inner->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
        } else {
            inner->aimTrackingState = GAME_ACTOR_AIM_TRACKING_TARGET;
        }
        temp = inner->movementSign;
        if (temp == 0) {
            if (inner->turnSign != 0) {
                mode = 0xD;
            } else {
                mode = 9;
            }
        } else if (temp == 1) {
            mode                    = 0xC;
            inner->movementMode     = 3;
            inner->aimTrackingState = temp;
        } else {
            inner->movementMode = 2;
            mode                = 0xD;
        }
        playerActorPlayChildSlotsWithBlend(arg0, mode, 0, 6);
        return;
    }

    switch (gPlayerStatus.weapon) {
        case 3:
        case 17:
            rec = Gp_AnimGetRec(&actor->animationContext, actor->animationSlots + 1);
            if (rec != NULL && rec != actor->lastCueRecord) {
                actor->lastCueRecord = rec;
                if ((rec->flags & ANIMATION_RECORD_CUE_MASK) == ANIMATION_RECORD_CUE_MASK) {
                    if (actor->statePhase == 0) {
                        actor->statePhase = 1;
                        Gp_PlayObjSfx(arg0->extra.tmd->coords, base | 0x20000002, 0);
                    } else {
                        Gp_PlayObjSfx(arg0->extra.tmd->coords, base | 0x20000003, 0);
                        done              = 1;
                        actor->statePhase = 0x64;
                    }
                }
            }
            break;
        case 9:
            switch (actor->statePhase) {
                case 0:
                    actor->statePhase = 1;
                    actor->stateTimer = 0xA;
                    /* fallthrough */
                case 1:
                    tick              = actor->stateTimer - 1;
                    actor->stateTimer = tick;
                    if (tick == -1) {
                        actor->statePhase += 1;
                        Gp_PlayObjSfx(arg0->extra.tmd->coords, base | 0x20000002, 0);
                        if (actor->reloadEffectSuppressed == 0) {
                            Gp_SpawnEff(EFFECT_RELOAD_CASINGS_DROP, coord, 0, NULL);
                        }
                    }
                    break;
                case 2:
                case 0x64:
                    rec = Gp_AnimGetRec(&actor->animationContext,
                                        actor->animationSlots + 1);
                    if (rec != NULL && rec != actor->lastCueRecord) {
                        actor->lastCueRecord = rec;
                        if ((rec->flags & ANIMATION_RECORD_CUE_MASK) == ANIMATION_RECORD_CUE_MASK) {
                            Gp_PlayObjSfx(arg0->extra.tmd->coords, base | 0x20000003, 0);
                            done                          = 1;
                            actor->reloadEffectSuppressed = 0;
                            actor->statePhase             = 0x64;
                        }
                    }
                    break;
            }
            break;
        case 11:
            if (actor->statePhase == 0) {
                actor->statePhase = 1;
                flags             = 0x20000002;
                Gp_PlayObjSfx(arg0->extra.tmd->coords, base | (variant | flags), 0);
            } else {
                rec = Gp_AnimGetRec(&actor->animationContext,
                                    actor->animationSlots + 1);
                if (rec != NULL && rec != actor->lastCueRecord) {
                    actor->lastCueRecord = rec;
                    if ((rec->flags & ANIMATION_RECORD_CUE_MASK) == ANIMATION_RECORD_CUE_MASK) {
                        flags = 0x20000003;
                        Gp_PlayObjSfx(arg0->extra.tmd->coords, base | (variant | flags), 0);
                        done              = 1;
                        actor->statePhase = 0x64;
                    }
                }
            }
            break;
        case 12:
            if (actor->statePhase == 0) {
                temp              = actor->reloadEffectSuppressed;
                actor->statePhase = 1;
                if (temp == 0) {
                    Gp_SpawnEff(EFFECT_RELOAD_EMITTER, coord, (s32)gPlayerStatus.weapon, NULL);
                }
            }
            rec = Gp_AnimGetRec(&actor->animationContext, actor->animationSlots + 1);
            if (rec != NULL && rec != actor->lastCueRecord) {
                actor->lastCueRecord = rec;
                if ((rec->flags & ANIMATION_RECORD_CUE_MASK) == ANIMATION_RECORD_CUE_MASK) {
                    flags = 0x20000003;
                    Gp_PlayObjSfx(arg0->extra.tmd->coords, base | (variant | flags), 0);
                    done                          = 1;
                    actor->reloadEffectSuppressed = 0;
                    actor->statePhase             = 0x64;
                }
            }
            break;
        case 13:
        case 14:
        case 23:
            rec = Gp_AnimGetRec(&actor->animationContext, actor->animationSlots + 1);
            if (rec != NULL && rec != actor->lastCueRecord) {
                actor->lastCueRecord = rec;
                if ((rec->flags & ANIMATION_RECORD_CUE_MASK) == ANIMATION_RECORD_CUE_MASK) {
                    if (actor->statePhase == 0) {
                        flags = 0x20000003;
                        Gp_PlayObjSfx(arg0->extra.tmd->coords, base | (variant | flags), 0);
                        done              = 1;
                        actor->statePhase = 0x64;
                    } else {
                        flags = 0x20000002;
                        Gp_PlayObjSfx(arg0->extra.tmd->coords, base | (variant | flags), 0);
                    }
                }
            }
            break;
        case 15:
            rec = Gp_AnimGetRec(&actor->animationContext, actor->animationSlots + 1);
            if (rec != NULL && rec != actor->lastCueRecord) {
                actor->lastCueRecord = rec;
                if ((rec->flags & ANIMATION_RECORD_CUE_MASK) == ANIMATION_RECORD_CUE_MASK) {
                    switch (actor->statePhase) {
                        case 0:
                            actor->statePhase = 1;
                            flags             = 0x20000003;
                            Gp_PlayObjSfx(arg0->extra.tmd->coords, base | (variant | flags), 0);
                            break;
                        case 1:
                            flags = 0x20000003;
                            Gp_PlayObjSfx(arg0->extra.tmd->coords, base | (variant | flags), 0);
                            done              = 1;
                            actor->statePhase = 0x64;
                            break;
                        case 0x64:
                            flags = 0x20000002;
                            Gp_PlayObjSfx(arg0->extra.tmd->coords, base | (variant | flags), 0);
                            break;
                    }
                }
            }
            break;
        case 16:
        case 20:
        case 21:
        case 25:
        case 26:
        case 28:
        case 29:
            rec = Gp_AnimGetRec(&actor->animationContext, actor->animationSlots + 1);
            if (rec != NULL && rec != actor->lastCueRecord) {
                actor->lastCueRecord = rec;
                if ((rec->flags & ANIMATION_RECORD_CUE_MASK) == ANIMATION_RECORD_CUE_MASK) {
                    if (actor->statePhase == 0) {
                        actor->statePhase = 1;
                        Gp_PlayObjSfx(arg0->extra.tmd->coords, base | 0x20000003, 0);
                    } else {
                        Gp_PlayObjSfx(arg0->extra.tmd->coords, base | 0x20000003, 0);
                        done              = 1;
                        actor->statePhase = 0x64;
                    }
                }
            }
            break;
        case 19:
            break;
        case 27:
            rec = Gp_AnimGetRec(&actor->animationContext, actor->animationSlots + 1);
            if (rec != NULL && rec != actor->lastCueRecord) {
                actor->lastCueRecord = rec;
                if ((rec->flags & ANIMATION_RECORD_CUE_MASK) == ANIMATION_RECORD_CUE_MASK) {
                    item = Gp_GetItemSlot(gPlayerStatus.weapon + 0x7F)->secondaryItemId;
                    if (item - 0x9F > 0) {
                        variant = ((item - 0xA0) % 3) << 24;
                    }
                    variant = base | variant;
                    if (actor->stateAux != 0) {
                        step = actor->statePhase;
                        if (step == 0) {
                            variant |= 0x20000008;
                        } else if (step == 1) {
                            variant |= 0x20000005;
                        } else if (step == 0x64) {
                            variant |= 0x20000009;
                        }
                        Gp_PlayObjSfx(arg0->extra.tmd->coords, variant, 0);
                    } else {
                        Gp_PlayObjSfx(arg0->extra.tmd->coords, variant | 0x20000003, 0);
                    }
                    next              = actor->statePhase + 1;
                    actor->statePhase = next;
                    if (next == 2) {
                        done              = 1;
                        actor->statePhase = 0x64;
                    }
                }
            }
            break;
        default:
            if (actor->statePhase == 0) {
                actor->statePhase = 1;
                Gp_PlayObjSfx(arg0->extra.tmd->coords, base | 0x20000002, 0);
            } else {
                rec = Gp_AnimGetRec(&actor->animationContext,
                                    actor->animationSlots + 1);
                if (rec != NULL && rec != actor->lastCueRecord) {
                    actor->lastCueRecord = rec;
                    if ((rec->flags & ANIMATION_RECORD_CUE_MASK) == ANIMATION_RECORD_CUE_MASK) {
                        Gp_PlayObjSfx(arg0->extra.tmd->coords, base | 0x20000003, 0);
                        done              = 1;
                        actor->statePhase = 0x64;
                    }
                }
            }
            break;
    }
    if (done != 0) {
        if (actor->actionValue != 0) {
            Gp_FlushPendingRelated(gPlayerStatus.weapon + 0x7F, actor->stateAux);
        } else {
            Gp_FillRelated(gPlayerStatus.weapon + 0x7F, actor->stateAux);
        }
    }
    Gp_UpdateLockTarget(arg0);
}

/// Switches the player to `Gp_TickPlayerMode2` state 2 and starts the entry
/// animation `movementSign` selects: a `fade` of 0 resets the child slots to it,
/// anything else is passed to `playerActorPlayChildSlotsWithBlend`.
static inline void _gpEnterPlayerMode2(Task* task, s32 fade)
{
    GameActor* inner;
    s32        mode;
    s32        temp;

    inner               = task->work;
    inner->mode         = GAME_ACTOR_MODE_NORMAL;
    inner->state        = 2;
    inner->movementMode = 0;
    if (inner->movementSign != 0) {
        temp = 1;
    } else {
        temp = 3;
    }
    inner->turnRateIndex  = temp;
    inner->animationState = 0;
    inner->statePhase     = 0;
    if (gPlayerStatus.statusFlags & PLAYER_STATUS_DARKNESS) {
        Gp_DetachLinkNode(task);
        inner->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
    } else {
        inner->aimTrackingState = GAME_ACTOR_AIM_TRACKING_TARGET;
    }
    temp = inner->movementSign;
    if (temp == 0) {
        if (inner->turnSign != 0) {
            mode = 0xD;
        } else {
            mode = 9;
        }
    } else if (temp == 1) {
        mode                    = 0xC;
        inner->movementMode     = 3;
        inner->aimTrackingState = temp;
    } else {
        inner->movementMode = 2;
        mode                = 0xD;
    }
    if (fade == 0) {
        Gp_AnimResetChildSlots(task, mode);
    } else {
        playerActorPlayChildSlotsWithBlend(task, mode, 0, fade);
    }
}

static void Gp_PlayerNormalState6(Task* arg0)
{
    GameActor* actor;
    s32        mode;
    s32        snd;

    actor               = arg0->work;
    actor->movementSign = 0;
    if (Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_CANCELLED) {
        actor->statePhase = 5;
    }
    switch (actor->statePhase) {
        case 0:
            actor->animationState = 9;
            actor->statePhase    += 1;
            Gp_StateC08.flags    |= ATTACHMENT_FLAG_RELEASE;
            if (actor->actionArgument == 0) {
                mode = 0x1A;
            } else if (actor->actionArgument == 1) {
                mode = 0x1D;
            } else {
                mode = 0x2A;
            }
            playerActorPlayChildSlotsWithBlend(arg0, mode, 0, 6);
            Gp_SetStateF0Bit(2);
            break;
        case 2:
            actor->animationState = 0;
            actor->statePhase    += 1;
            if (actor->actionArgument == 0) {
                mode = 0x1B;
            } else if (actor->actionArgument == 1) {
                mode = 0x1E;
            } else {
                mode = 0x2B;
            }
            Gp_AnimResetChildSlots(arg0, mode);
        case 3:
            if (Gp_StateC08.duration == 0) {
                actor->animationState = 9;
                actor->statePhase    += 1;
                snd                   = 4;
                if (Gp_StateC08.attachId < ATTACHMENT_ID_EARLY_SPELL_LIMIT || Gp_StateC08.attachId > ATTACHMENT_ID_LAST_SPELL) {
                    snd = 3;
                }
                Gp_SetStateF0Bit(snd);
                if (actor->actionArgument == 0) {
                    mode = 0x1C;
                } else if (actor->actionArgument == 1) {
                    mode = 0x1F;
                } else {
                    mode = 0x2C;
                }
                Gp_AnimResetChildSlots(arg0, mode);
                break;
            }
        case 1:
            Gp_SetStateF0Bit(2);
            break;
        case 4:
            break;
        case 5:
            if (actor->stateAux == 0) {
                func_801066DC(arg0, 0);
                break;
            }
            _gpEnterPlayerMode2(arg0, actor->stateAux == 1 ? 6 : 8);
            break;
    }
}

static void func_8010771C(Task* arg0)
{
    GameActor* actor;
    GameActor* inner;
    s32        mode;
    s32        temp;
    s32        flag;

    actor               = arg0->work;
    actor->movementSign = 0;
    if (!(gPlayerStatus.statusFlags & PLAYER_STATUS_PARALYSIS)) {
        actor->statePhase        = 1;
        actor->paralysisProgress = 0;
    }
    switch (actor->statePhase) {
        case 0:
            flag                     = 1;
            actor->statePhase        = flag;
            actor->paralysisProgress = 0xF;
        case 1:
            if (actor->padPressed & 0xF0F0) {
                actor->paralysisProgress--;
            }
            if ((s8)actor->paralysisProgress > 0) {
                break;
            }
            if (actor->stateAux == 0) {
                func_801066DC(arg0, 0);
                break;
            }
            inner               = arg0->work;
            inner->mode         = GAME_ACTOR_MODE_NORMAL;
            inner->state        = 2;
            inner->movementMode = 0;
            if (inner->movementSign != 0) {
                temp = 1;
            } else {
                temp = 3;
            }
            inner->turnRateIndex  = temp;
            inner->animationState = 0;
            inner->statePhase     = 0;
            if (gPlayerStatus.statusFlags & PLAYER_STATUS_DARKNESS) {
                Gp_DetachLinkNode(arg0);
                inner->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
            } else {
                inner->aimTrackingState = GAME_ACTOR_AIM_TRACKING_TARGET;
            }
            temp = inner->movementSign;
            if (temp == 0) {
                if (inner->turnSign != 0) {
                    mode = 0xD;
                } else {
                    mode = 9;
                }
            } else if (temp == 1) {
                mode                    = 0xC;
                inner->movementMode     = 3;
                inner->aimTrackingState = temp;
            } else {
                inner->movementMode = 2;
                mode                = 0xD;
            }
            playerActorPlayChildSlotsWithBlend(arg0, mode, 0, 6);
            break;
    }
}

static void Gp_PlayerMode2State3(Task* arg0)
{
    _PlayerActorStairClimbScratch* block;
    _PlayerActorStairClimbScratch* blockAlias; // Second pointer to `block`, kept so each division reloads the divisor
    GameActor*                     actor;
    GfxCoord*                      coord;
    s32                            angle;
    s32                            delay;
    s32                            mode;

    block      = SCRATCH_STACK_RESERVE_BLOCK(_PlayerActorStairClimbScratch);
    blockAlias = block;
    actor      = arg0->work;
    coord      = arg0->extra.tmd->coords;
    switch (actor->statePhase) {
        case 0:
            // Head along the model's forward axis tilted by the flight's slope.
            block->pitchedMatrix = coord->coord;
            angle                = -0x180;
            if (actor->jumpVariant == 0) {
                angle = 0x180;
            }
            gfxRotMatrixX(&block->pitchedMatrix, angle, GRAPHICS_ROTATION_COMPOSE);
            gfxReadMatrixZAxis(&block->pitchedMatrix, &block->direction);
            VectorNormalSS(&block->direction, &block->direction);
            if (actor->jumpVariant == 0) {
                actor->statePhase   = 1;
                actor->stateTimer   = 0;
                actor->actionValue  = actor->scriptMotion.jumpSteps & 1;
                block->speedDivisor = 110;
            } else {
                actor->statePhase   = 3;
                actor->stateTimer   = 5;
                block->speedDivisor = 100;
            }
            actor->velocity.vx = blockAlias->direction.vx / blockAlias->speedDivisor;
            actor->velocity.vy = blockAlias->direction.vy / blockAlias->speedDivisor;
            actor->velocity.vz = blockAlias->direction.vz / blockAlias->speedDivisor;
            break;
        case 1:
            if (func_80105ED4(arg0) != 0) {
                delay = 0xA;
                if (actor->scriptMotion.jumpSteps == 1) {
                    delay = 0xB;
                }
                actor->stateTimer = delay;
            } else if (actor->stateTimer > 0) {
                actor->stateTimer--;
                if (actor->stateTimer == 0) {
                    actor->scriptMotion.jumpSteps--;
                    if (actor->scriptMotion.jumpSteps <= 0) {
                        actor->stateTimer = 8;
                        actor->statePhase++;
                        gfxReadMatrixZAxis(&coord->coord, &block->direction);
                        VectorNormalSS(&block->direction, &block->direction);
                        actor->velocity.vx = (s16)(block->direction.vx / 180);
                        actor->velocity.vy = (s16)(block->direction.vy / 180);
                        mode               = 0x26;
                        actor->velocity.vz = (s16)(block->direction.vz / 180);
                        if (actor->actionValue != 0) {
                            mode = 0x27;
                        }
                        playerActorPlayChildSlotsWithBlend(arg0, mode, 0, 3);
                    }
                }
                coord->coord.t[0] += actor->velocity.vx;
                coord->coord.t[1] += actor->velocity.vy;
                coord->coord.t[2] += actor->velocity.vz;
            }
            break;
        case 2:
            func_80105ED4(arg0);
            if (actor->stateTimer > 0) {
                actor->stateTimer--;
                coord->coord.t[0] += actor->velocity.vx;
                coord->coord.t[1] += actor->velocity.vy;
                coord->coord.t[2] += actor->velocity.vz;
            }
            if (func_8010583C(arg0, 0, 0, 0) == 0) {
            block_land:
                actor->scriptedMotionPending = 0;
                actor->state                 = 1;
                playerActorPlayChildSlotsWithBlend(arg0, 1, 0, 5);
            }
            break;
        case 3:
            if (func_80105ED4(arg0) != 0) {
                if (actor->scriptMotion.jumpSteps == 1) {
                    gfxReadMatrixZAxis(&coord->coord, &block->direction);
                    VectorNormalSS(&block->direction, &block->direction);
                    actor->velocity.vx = (s16)(block->direction.vx / 58);
                    actor->velocity.vz = (s16)(block->direction.vz / 58);
                }
                delay = 9;
                if (actor->scriptMotion.jumpSteps == 1) {
                    delay = 5;
                }
                actor->stateTimer = delay;
                actor->scriptMotion.jumpSteps--;
            } else if (actor->stateTimer != 0) {
                actor->stateTimer--;
                coord->coord.t[0] += actor->velocity.vx;
                coord->coord.t[1] += actor->velocity.vy;
                coord->coord.t[2] += actor->velocity.vz;
            } else if (actor->scriptMotion.jumpSteps == 0) {
                goto block_land;
            }
            break;
    }
    Gp_AnimTickChildSlots(arg0);
    SCRATCH_STACK_RELEASE_BLOCK(_PlayerActorStairClimbScratch);
}

void Gp_PlayerMode2State4(Task* arg0)
{
    PlayerActorApproachScratch* block;
    GfxCoord*                   coord;
    GameActor*                  actor;
    s32                         val;
    s32                         mode;

    actor                         = arg0->work;
    coord                         = arg0->extra.tmd->coords;
    block                         = SCRATCH_STACK_RESERVE_BLOCK(PlayerActorApproachScratch);
    block->targetDelta.vx         = actor->destination.vx - coord->coord.t[0];
    block->targetDelta.vy         = actor->destination.vy - coord->coord.t[1];
    block->targetDelta.vz         = actor->destination.vz - coord->coord.t[2];
    actor->scriptMotion.targetYaw = ratan2(block->targetDelta.vx, block->targetDelta.vz);
    val                           = func_80103E7C(actor->rotation.vy, actor->scriptMotion.targetYaw);
    block->turnStep               = val;
    if (val > 0x40) {
        block->turnStep = 0x40;
    } else if (val < -0x40) {
        block->turnStep = -0x40;
    } else if (actor->statePhase == 0) {
        actor->statePhase = 1;
    }
    actor->rotation.vy = (actor->rotation.vy + block->turnStep) & 0xFFF;
    switch (actor->statePhase) {
        case 0:
            actor->statePhase = 1;
            mode              = 6;
            if (block->turnStep < 0) {
                mode = 5;
            }
            Gp_AnimPlayChildSlots(arg0, mode, 1);
        case 1:
            if (block->turnStep == 0) {
                actor->movementMode = 1;
                actor->statePhase++;
                if (actor->actionArgument == 0) {
                    mode = 2;
                    if (actor->equipmentTasks[1] == NULL) {
                        mode = 0x13;
                    }
                } else {
                    mode = actor->actionArgument;
                }
                playerActorPlayChildSlotsWithBlend(arg0, mode, 0, 5);
            }
            break;
        case 2:
            if (abs(coord->coord.t[0] - actor->destination.vx) < 0x69) {
                if (abs(coord->coord.t[2] - actor->destination.vz) < 0x69) {
                    actor->scriptedMotionPending = 0;
                    actor->state                 = 1;
                    mode                         = 1;
                    if (actor->actionValue != 0) {
                        mode = actor->actionValue;
                    }
                    playerActorPlayChildSlotsWithBlend(arg0, mode, 0, 5);
                    break;
                }
            }
            actor->movementSign = 1;
            Gp_StepPlayerMove(arg0);
            func_80105ED4(arg0);
            break;
    }
    Gp_AnimTickChildSlots(arg0);
    SCRATCH_STACK_RELEASE_BLOCK(PlayerActorApproachScratch);
}

static void Gp_PlayerMode2StateA(Task* arg0)
{
    s32        variant;
    GameActor* actor;
    s32        res;
    s32        dir;
    u8         item;
    s32        base;
    s32        val;

    actor = arg0->work;
    if (gPlayerStatus.hp > 0) {
        if (gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_ENGAGED) {
            actor->state          = 3;
            actor->turnRateIndex  = 2;
            actor->mode           = GAME_ACTOR_MODE_NORMAL;
            actor->movementMode   = 0;
            actor->animationState = 4;
            actor->statePhase     = 0;
            playerActorPlayChildSlotsWithBlend(arg0, 8, 0, 6);
            Gp_DetachLinkNode(arg0);
        } else if ((s8)func_801060E0(arg0) != 0 &&
                   Gp_AnimGetRec(&actor->animationContext,
                                 actor->animationSlots + 1) != NULL &&
                   actor->attackControl.cooldownTicks == 0) {
            dir = D_80112EF8[gPlayerStatus.weapon] != 0 ? (s8)actor->attackButton : 1;
            res = func_80106264(dir);
            if (res > 0 ||
                (item = actor->attackButton,
                 D_80112F1C[gPlayerStatus.weapon][(u8)(item - 1)] != 0)) {
                actor->aimControl = GAME_ACTOR_AIM_REQUEST_SCRIPTED;
                actor->statePhase = 0;
                func_8010615C(arg0);
            } else if (res == 0) {
                func_801095BC(&variant);
                actor->attackControl.cooldownTicks = 0x14;
                base                               = gPlayerStatus.weapon << 16;
                val                                = variant | 0x20000001;
                Gp_PlayObjSfx(arg0->extra.tmd->coords, base | val, 0);
            }
        }
    }
    Gp_AnimTickChildSlots(arg0);
}

static void Gp_PlayerMode2StateB(Task* arg0)
{
    GameActor* actor;
    GameActor* inner;
    s32        mode;
    s32        temp;
    s32        flag;

    actor = arg0->work;
    switch (actor->statePhase) {
        case 0:
            flag              = 1;
            actor->statePhase = flag;
            playerActorPlayChildSlotsWithBlend(arg0, 0x28, 0, 6);
            break;
        case 1:
            if (Gp_AnimGetRec(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                if (func_80105894(arg0, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0) {
                    inner               = arg0->work;
                    inner->mode         = GAME_ACTOR_MODE_NORMAL;
                    inner->state        = 2;
                    inner->movementMode = 0;
                    if (inner->movementSign != 0) {
                        temp = 1;
                    } else {
                        temp = 3;
                    }
                    inner->turnRateIndex  = temp;
                    inner->animationState = 0;
                    inner->statePhase     = 0;
                    if (gPlayerStatus.statusFlags & PLAYER_STATUS_DARKNESS) {
                        Gp_DetachLinkNode(arg0);
                        inner->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
                    } else {
                        inner->aimTrackingState = GAME_ACTOR_AIM_TRACKING_TARGET;
                    }
                    temp = inner->movementSign;
                    if (temp == 0) {
                        if (inner->turnSign != 0) {
                            mode = 0xD;
                        } else {
                            mode = 9;
                        }
                    } else if (temp == 1) {
                        mode                    = 0xC;
                        inner->movementMode     = 3;
                        inner->aimTrackingState = temp;
                    } else {
                        inner->movementMode = 2;
                        mode                = 0xD;
                    }
                    playerActorPlayChildSlotsWithBlend(arg0, mode, 0, 4);
                }
            }
            break;
    }
    Gp_AnimTickChildSlots(arg0);
    Gp_PlayerStepSfx(arg0);
}

static void Gp_TickPlayerActor(Task* arg0)
{
    GameActor*     inner;
    TaskFuncTable3 sp;

    sp    = Gp_PlayerModeFns;
    inner = arg0->work;
    func_80104A4C(arg0);
    if (inner->attackControl.cooldownTicks > 0) {
        inner->attackControl.cooldownTicks--;
    }
    if ((s8)inner->recoveryTicks > 0) {
        inner->recoveryTicks--;
    }
    inner->usesPushbackDirection = 0;
    sp.funcs[inner->mode](arg0);
    func_80109720(arg0);
    func_801030CC(arg0);
}

static void Gp_ArmLockOnState(Task* arg0)
{
    GameActor*       inner;
    WorldTargetNode* node;
    s32              flag;

    inner               = arg0->work;
    node                = Gp_FindLockNode(arg0);
    inner->movementSign = 0;
    if ((node != NULL && gSceneCombatState.signals.bytes.battlePhase < SCENE_COMBAT_BATTLE_FINISHED) || (flag = 1, gSceneCombatState.signals.bytes.battlePhase == flag) ||
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.field_929 != 0) {
        if (inner->statePhase != 0) {
            Gp_ArmStateF0(1);
            if (inner->aimTransitionPending != 0) {
                inner->aimTransitionPending = 0;
                if (node != NULL) {
                    func_80108E0C(arg0, node);
                }
            }
            Gp_ResetActorAnimState(arg0, 3);
        }
    } else {
        func_80109374(arg0);
        if (inner->aimControl & GAME_ACTOR_AIM_REQUEST_EXIT) {
            inner->aimTransitionPending = 0;
            inner->aimTrackingState     = flag;
            Gp_DetachLinkNode(arg0);
            func_80108874(arg0);
        }
    }
}

static void func_80108568(Task* arg0)
{
    GameActor* actor;

    actor = arg0->work;
    if (actor->movementSign != actor->previousMovementSign) {
        Gp_ResetActorAnimState(arg0, 4);
    } else if (actor->movementSign == 0) {
        if (actor->turnSign != actor->previousTurnSign) {
            func_80108684(arg0);
        }
    }
}

static void func_801085D0(Task* arg0)
{
    GameActor* inner;

    inner               = arg0->work;
    inner->movementSign = 0;
    func_80109374(arg0);
    if (inner->aimControl & GAME_ACTOR_AIM_REQUEST_ENTER) {
        func_8010870C(arg0, 4);
    }
}

static void func_80108620(Task* arg0)
{
    GameActor* inner;
    s32        mode;

    inner                 = arg0->work;
    inner->mode           = GAME_ACTOR_MODE_NORMAL;
    inner->movementMode   = 0;
    inner->turnRateIndex  = 3;
    inner->animationState = 0;
    inner->statePhase     = 0;
    inner->idleTicks      = 0;
    if (inner->turnSign == 0) {
        mode = 1;
    } else if (inner->turnSign == 1) {
        mode = 6;
    } else {
        mode = 5;
    }
    playerActorPlayChildSlotsWithBlend(arg0, mode, 0, 5);
}

static void func_80108684(Task* arg0)
{
    GameActor* inner;
    s32        mode;
    s32        temp;

    inner                 = arg0->work;
    inner->mode           = GAME_ACTOR_MODE_NORMAL;
    inner->movementMode   = 0;
    inner->animationState = 0;
    inner->statePhase     = 0;
    if (inner->movementSign != 0) {
        if (inner->movementSign == 1) {
            temp = 3;
        } else {
            temp = 2;
        }
        mode                 = 0xD;
        inner->movementMode  = temp;
        inner->turnRateIndex = 1;
        if (inner->movementSign == 1) {
            mode = 0xC;
        }
    } else {
        mode                 = 0xD;
        inner->turnRateIndex = 3;
        if (inner->turnSign == 0) {
            mode = 9;
        }
    }
    playerActorPlayChildSlotsWithBlend(arg0, mode, 0, 5);
}

void func_8010870C(Task* arg0, s32 arg1)
{
    GameActor* inner;

    inner                              = arg0->work;
    inner->state                       = 1;
    inner->mode                        = GAME_ACTOR_MODE_NORMAL;
    inner->movementMode                = 0;
    inner->turnRateIndex               = 0;
    inner->animationState              = 2;
    inner->statePhase                  = 0;
    inner->attackControl.cooldownTicks = 0;
    if (arg1 == 0) {
        Gp_AnimResetChildSlots(arg0, 7);
    } else {
        playerActorPlayChildSlotsWithBlend(arg0, 7, 0, arg1);
    }
}

static void Gp_ResetActorAnimState(Task* arg0, s32 arg1)
{
    GameActor* inner;
    s32        mode;
    s32        temp;

    inner               = arg0->work;
    inner->mode         = GAME_ACTOR_MODE_NORMAL;
    inner->state        = 2;
    inner->movementMode = 0;
    if (inner->movementSign != 0) {
        temp = 1;
    } else {
        temp = 3;
    }
    inner->turnRateIndex  = temp;
    inner->animationState = 0;
    inner->statePhase     = 0;
    if (gPlayerStatus.statusFlags & PLAYER_STATUS_DARKNESS) {
        Gp_DetachLinkNode(arg0);
        inner->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
    } else {
        inner->aimTrackingState = GAME_ACTOR_AIM_TRACKING_TARGET;
    }
    temp = inner->movementSign;
    if (temp == 0) {
        if (inner->turnSign != 0) {
            mode = 0xD;
        } else {
            mode = 9;
        }
    } else if (temp == 1) {
        mode                    = 0xC;
        inner->movementMode     = 3;
        inner->aimTrackingState = temp;
    } else {
        inner->movementMode = 2;
        mode                = 0xD;
    }
    if (arg1 == 0) {
        Gp_AnimResetChildSlots(arg0, mode);
    } else {
        playerActorPlayChildSlotsWithBlend(arg0, mode, 0, arg1);
    }
}

void func_80108874(Task* arg0)
{
    GameActor* inner;

    inner                 = arg0->work;
    inner->state          = 3;
    inner->mode           = GAME_ACTOR_MODE_NORMAL;
    inner->movementMode   = 0;
    inner->turnRateIndex  = 2;
    inner->animationState = 4;
    inner->statePhase     = 0;
    playerActorPlayChildSlotsWithBlend(arg0, 8, 0, 6);
    Gp_DetachLinkNode(arg0);
}

void func_801088D4(Task* arg0, s32 arg1, s32 arg2)
{
    GameActor* inner;
    s32        mode;

    inner = arg0->work;
    if (arg2 == 2) {
        if (func_80106264(arg1) != 0) {
            if (D_80112F1C[gPlayerStatus.weapon][0] == 0) {
                inner->statePhase = 0x3E8;
                return;
            }
        }
        inner->animationState = 0xA;
        mode                  = 0x14;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 1) {
            func_actor_800100_80166E94(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), 0);
        }
    } else {
        if (arg2 == 1) {
            if (inner->mode == GAME_ACTOR_MODE_SCRIPTED) {
                return;
            }
        }
        inner->animationState = 5;
        mode                  = arg1 + 0xE;
    }
    inner->state         = 5;
    inner->mode          = GAME_ACTOR_MODE_NORMAL;
    inner->movementMode  = 0;
    inner->turnRateIndex = 0;
    inner->statePhase    = 0;
    inner->stateAux      = arg1;
    inner->actionValue   = arg2;
    func_80106350(arg0, gPlayerStatus.weapon, 0);
    playerActorPlayChildSlotsWithBlend(arg0, mode, 0, 3);
}

static void func_80108A0C(Task* arg0)
{
    GameActor* inner;
    u16        prev;
    s32        tens;

    inner                   = arg0->work;
    prev                    = inner->state;
    inner->state            = 6;
    inner->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
    inner->mode             = GAME_ACTOR_MODE_NORMAL;
    inner->movementMode     = 0;
    inner->turnRateIndex    = 0;
    inner->animationState   = 0;
    inner->statePhase       = 0;
    inner->movementSign     = 0;
    inner->stateAux         = prev;
    tens                    = Gp_StateC08.attachId % 100 / 10;
    if (Gp_StateC08.attachId >= ATTACHMENT_ID_ITEM) {
        if (tens == 1) {
            inner->actionArgument = 0;
        } else {
            inner->actionArgument = 1;
        }
    } else if (tens == 3) {
        inner->actionArgument = 2;
    } else if (Gp_StateC08.attachId < ATTACHMENT_ID_EARLY_SPELL_LIMIT_U) {
        inner->actionArgument = 1;
    } else {
        inner->actionArgument = 0;
    }
}

static void func_80108AD4(Task* arg0)
{
    GameActor* inner;
    u16        prev;

    inner                 = arg0->work;
    prev                  = inner->state;
    inner->mode           = GAME_ACTOR_MODE_NORMAL;
    inner->state          = 7;
    inner->movementMode   = 0;
    inner->turnRateIndex  = 0;
    inner->animationState = 0;
    inner->statePhase     = 0;
    inner->rumblePosted   = 0;
    inner->movementSign   = 0;
    inner->turnSign       = 0;
    inner->stateAux       = prev;
    Gp_DetachLinkNode(arg0);
    inner->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    Gp_StateC08.flags                                    |= ATTACHMENT_FLAG_EVENT_LOCK;
    func_80106350(arg0, gPlayerStatus.weapon, 0);
    playerActorPlayChildSlotsWithBlend(arg0, 0x19, 3, 6);
}

void Gp_PlayerMode2State0(Task* arg0)
{
    func_80105B0C(arg0);
    func_80105ED4(arg0);
}

void Gp_PlayerMode2State1(Task* arg0)
{
    Gp_AnimTickChildSlots(arg0);
    func_80105ED4(arg0);
}

void Gp_PlayerMode2State2(Task* arg0)
{
    GameActor* inner;
    s16        cur;
    s16        tgt;
    u16        raw;
    s32        temp;
    s32        wrap;
    s32        delta;
    s32        flag;

    inner = arg0->work;
    cur   = inner->rotation.vy;
    tgt   = inner->scriptMotion.targetYaw;
    raw   = inner->scriptMotion.targetYaw;
    temp  = cur - tgt;
    if (temp < 0) {
        temp = -temp;
    }
    if (temp < 0x41 || (wrap = tgt - 0x1000, temp = cur - wrap, temp = ABS(temp), temp < 0x41)) {
        flag                         = 1;
        inner->rotation.vy           = raw;
        inner->scriptedMotionPending = 0;
        inner->state                 = flag;
        playerActorPlayChildSlotsWithBlend(arg0, flag, 0, 5);
    } else {
        delta = func_80103E7C(cur, tgt);
        if (delta > 0x40) {
            delta = 0x40;
        } else if (delta < -0x40) {
            delta = -0x40;
        }
        inner->rotation.vy = ((u16)inner->rotation.vy + delta) & 0xFFF;
    }
    Gp_AnimTickChildSlots(arg0);
}

static void Gp_PlayerMode2State8(Task* arg0)
{
    GameActor* inner;
    s32        mode;

    inner = arg0->work;
    switch (inner->statePhase) {
        case 0:
        case 1:
            Gp_PlayerMode2State4(arg0);
            if (inner->statePhase == 2) {
                inner->movementMode = 3;
                mode                = 4;
                if (inner->actionArgument != 0) {
                    mode = inner->actionArgument;
                }
                playerActorPlayChildSlotsWithBlend(arg0, mode, 0, 5);
            }
            break;
        case 2:
            Gp_PlayerMode2State4(arg0);
            break;
    }
}

void Gp_PlayerMode2State6(Task* arg0)
{
    GameActor* inner;

    inner = arg0->work;
    if (inner->actionValue >= inner->stateTimer) {
        inner->recoveryTicks = 0x12;
        if (inner->statePhase == 0) {
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), 0x7DA, 0, 0x7DE);
            inner->statePhase = 1;
        }
    } else if (inner->padPressed & 0xF0F0) {
        inner->actionValue++;
    }
    Gp_AnimTickChildSlots(arg0);
}

/// Stores `arg1` as the actor's `targetNode` node, moving the `targeted` mark
/// from the node it replaces to `arg1`.
static inline void _gpSetLockNode(Task* arg0, WorldTargetNode* arg1)
{
    GameActor*       inner;
    WorldTargetNode* node;

    inner = arg0->work;
    node  = inner->targetNode;
    if (node != arg1) {
        if (node != NULL) {
            node->state.parts.targeted = 0;
        }
        inner->targetNode = arg1;
    }
    arg1->state.parts.targeted = 1;
}

void func_80108E0C(Task* arg0, WorldTargetNode* arg1)
{
    _gpSetLockNode(arg0, arg1);
}

/// `hitRegion` dispatcher: three slots of `Gp_PlayerMode1State0`, then `Gp_PlayerMode1State3`.
static const TaskFuncTable4 Gp_PlayerMode1States = { {
    Gp_PlayerMode1State0,
    Gp_PlayerMode1State0,
    Gp_PlayerMode1State0,
    Gp_PlayerMode1State3,
} };

static void Gp_TickPlayerMode1(Task* arg0)
{
    TaskFuncTable4 handlers;

    handlers = Gp_PlayerMode1States;
    handlers.funcs[(u16)((GameActor*)arg0->work)->hitRegion](arg0);
    Gp_TickActorAnimState(arg0);
    Gp_AnimTickChildSlots(arg0);
    Gp_TurnPlayer(arg0);
    Gp_StepPlayerMove(arg0);
}

/// `state` dispatcher copied by `Gp_TickPlayerMode2`.
static const TaskFuncTable12 Gp_PlayerMode2States = { {
    Gp_PlayerMode2State0,
    Gp_PlayerMode2State1,
    Gp_PlayerMode2State2,
    Gp_PlayerMode2State3,
    Gp_PlayerMode2State4,
    Gp_PlayerMode2State5,
    Gp_PlayerMode2State6,
    Gp_PlayerMode2State7,
    Gp_PlayerMode2State8,
    Gp_PlayerMode2State9,
    Gp_PlayerMode2StateA,
    Gp_PlayerMode2StateB,
} };

static void Gp_TickPlayerMode2(Task* arg0)
{
    GameActor*      inner;
    TaskFuncTable12 sp;

    sp    = Gp_PlayerMode2States;
    inner = arg0->work;
    sp.funcs[inner->state](arg0);
    Gp_TurnPlayer(arg0);
    if (gPlayerStatus.hp <= 0 && inner->state != 0xA) {
        Gp_BindActorAnim(arg0);
        Gp_StopPlayerAnim(arg0, 4);
    }
}

static void func_80108FA0(Task* arg0)
{
    func_801065A8(arg0);
    func_80109290(arg0);
    func_80105ED4(arg0);
}

static void Gp_PlayerNormalState1(Task* arg0)
{
    GameActor*       inner;
    WorldTargetNode* node;
    s32              flag;

    Gp_TrackLockTarget(arg0);
    inner               = arg0->work;
    node                = Gp_FindLockNode(arg0);
    inner->movementSign = 0;
    if ((node != NULL && gSceneCombatState.signals.bytes.battlePhase < SCENE_COMBAT_BATTLE_FINISHED) || (flag = 1, gSceneCombatState.signals.bytes.battlePhase == flag) ||
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.field_929 != 0) {
        if (inner->statePhase != 0) {
            Gp_ArmStateF0(1);
            if (inner->aimTransitionPending != 0) {
                inner->aimTransitionPending = 0;
                if (node != NULL) {
                    func_80108E0C(arg0, node);
                }
            }
            Gp_ResetActorAnimState(arg0, 3);
        }
    } else {
        func_80109374(arg0);
        if (inner->aimControl & GAME_ACTOR_AIM_REQUEST_EXIT) {
            inner->aimTransitionPending = 0;
            inner->aimTrackingState     = flag;
            Gp_DetachLinkNode(arg0);
            func_80108874(arg0);
        }
    }
}

static void func_801090E8(Task* arg0)
{
    GameActor* inner;

    inner               = arg0->work;
    inner->movementSign = 0;
    func_80109374(arg0);
    if (inner->aimControl & GAME_ACTOR_AIM_REQUEST_ENTER) {
        func_8010870C(arg0, 4);
    }
}

static void func_80109138(Task* arg0)
{
    func_8010615C(arg0);
    func_801041FC(arg0, 0);
    Gp_UpdateLockTarget(arg0);
}

static void Gp_PlayerMode1State0(Task* arg0)
{
    GameActor* inner;
    u8         kind;

    inner = arg0->work;
    kind  = inner->damageReaction;
    switch (kind) {
        case 0:
        case 1:
        case 2:
        case 8:
        case 9:
        case 10:
        case 11:
            func_8010ABD4(arg0);
            break;
        case 5:
            func_8010AC54(arg0);
            break;
        case 6:
            func_80109A1C(arg0);
            break;
        case 3:
            func_8010AD64(arg0);
            break;
        case 7:
            func_80109844(arg0);
            break;
    }
}

static void Gp_PlayerMode1State3(Task* task)
{
}

static void func_80109210(Task* arg0)
{
    GameActor* inner;
    u16        flags;

    inner = arg0->work;
    flags = inner->padHeld;
    if (flags & 0xA000) {
        if (flags & 0x8000) {
            inner->turnSign = -1;
        } else {
            inner->turnSign = 1;
        }
    } else {
        inner->turnSign = 0;
    }
}

static void func_80109250(Task* arg0)
{
    GameActor* inner;
    u16        flags;

    inner = arg0->work;
    flags = inner->padHeld;
    if (flags & 0x5000) {
        if (flags & 0x4000) {
            inner->movementSign = -1;
        } else {
            inner->movementSign = 1;
        }
    } else {
        inner->movementSign = 0;
    }
}

static s32 func_80109290(Task* arg0)
{
    GameActor* inner;
    u16        prev;
    s32        tens;
    s32        ret;

    ret = 0;
    if (Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) {
        inner                   = arg0->work;
        prev                    = inner->state;
        inner->state            = 6;
        inner->mode             = GAME_ACTOR_MODE_NORMAL;
        inner->movementMode     = 0;
        inner->turnRateIndex    = 0;
        inner->animationState   = 0;
        inner->statePhase       = 0;
        inner->movementSign     = 0;
        inner->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
        inner->stateAux         = prev;
        tens                    = (u16)(Gp_StateC08.attachId % 100 / 10);
        if (Gp_StateC08.attachId >= ATTACHMENT_ID_ITEM) {
            if (tens == 1) {
                inner->actionArgument = 0;
            } else {
                inner->actionArgument = 1;
            }
        } else if (tens == 3) {
            inner->actionArgument = 2;
        } else if (Gp_StateC08.attachId < ATTACHMENT_ID_EARLY_SPELL_LIMIT_U) {
            inner->actionArgument = 1;
        } else {
            inner->actionArgument = 0;
        }
        ret = 1;
    }
    return ret;
}

static void func_80109374(Task* arg0)
{
    GameActor* inner;

    inner = arg0->work;
    if ((inner->padHeld & 0x80) && (Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_IDLE) && (gPlayerStatus.weapon != PLAYER_STATUS_EQUIPMENT_NONE) &&
        (inner->restrictRunAndAim == 0)) {
        inner->aimControl = GAME_ACTOR_AIM_REQUEST_ENTER;
    } else {
        inner->aimControl = GAME_ACTOR_AIM_REQUEST_EXIT;
    }
}

static void Gp_UpdateLockTarget(Task* arg0)
{
    GameActor* inner;
    u16        flags;

    inner = arg0->work;
    if (inner->targetNode != NULL) {
        flags = inner->padPressed;
        if (flags & 0x40) {
            Gp_DetachLinkNode(arg0);
        } else if (((inner->padHeld & 0x80) && (flags & 0xA000)) || (flags & 0x80)) {
            _gpSetLockNode(arg0, Gp_FindLockNodePad(arg0));
        }
    } else if ((inner->padPressed & 0x80) && !(gPlayerStatus.statusFlags & PLAYER_STATUS_DARKNESS)) {
        inner->aimTrackingState = GAME_ACTOR_AIM_TRACKING_TARGET;
        _gpSetLockNode(arg0, Gp_FindLockNode(arg0));
    }
}

static void Gp_PlayerMode2State5(Task* arg0)
{
    GameActor* inner;
    s32        mode;
    s32        flag;
    s32        arg2;

    inner = arg0->work;
    switch (inner->statePhase) {
        case 0:
            mode                = 2;
            flag                = 1;
            inner->statePhase   = flag;
            inner->movementMode = flag;
            if (inner->equipmentTasks[1] == NULL) {
                mode = 0x13;
            }
            arg2 = 1;
            if (inner->stateTimer == 0) {
                arg2 = 6;
            }
            Gp_AnimPlayChildSlots(arg0, mode, arg2);
        case 1:
            if (func_80105ED4(arg0) != 0) {
                inner->actionValue--;
                if (inner->actionValue <= 0) {
                    inner->scriptedMotionPending = 0;
                    inner->state                 = 1;
                    playerActorPlayChildSlotsWithBlend(arg0, 1, 0, 5);
                }
            } else {
                inner->movementSign = 1;
                Gp_StepPlayerMove(arg0);
            }
            break;
    }
    Gp_AnimTickChildSlots(arg0);
}

static void func_801095BC(s32* arg0)
{
    PlayerStatus*          p;
    volatile PlayerStatus* vp;

    p = &gPlayerStatus;
    if (p->weapon == 0x1B) {
        *arg0 = Gp_GetItemSlot(p->weapon + 0x7F)->secondaryItemId - 0x9F;
        if (*arg0 < 0) {
            *arg0 = 0xA;
        }
        *arg0 = (*arg0 - 0xA) << 24;
    } else {
        vp = p;
        if ((u32)(vp->weaponSlotItem - 0xA) < 6U) {
            *arg0 = ((vp->weaponSlotItem - 1) % 3) << 24;
            if (*arg0 < 0) {
                *arg0 = 0;
            }
        } else {
            *arg0 = 0;
        }
    }
}

static void Gp_PlayerMode2State7(Task* arg0)
{
    GameActor* inner;
    s32        mode;
    s32        flag;

    inner = arg0->work;
    switch (inner->statePhase) {
        case 0:
            mode              = 0x20;
            flag              = 1;
            inner->statePhase = flag;
            if (inner->stateTimer != 0) {
                mode = 0x21;
            }
            Gp_AnimPlayChildSlots(arg0, mode, 1);
        case 1:
            Gp_AnimTickChildSlots(arg0);
            break;
    }
    Gp_PlayerStepSfx(arg0);
}

static void Gp_PlayerMode2State9(Task* arg0)
{
    Gp_AnimTickChildSlots(arg0);
}

static void func_80109720(Task* arg0)
{
    GameActor* actor;
    GfxCoord*  coord;
    u16        flags;
    s16        delta;
    s32        val;
    s32        temp;

    coord                 = arg0->extra.tmd->coords;
    actor                 = arg0->work;
    coord[4].composeStamp = GRAPHICS_COORD_DIRTY;
    flags                 = actor->padHeld;
    if ((flags & 0xA000) && (actor->mode == GAME_ACTOR_MODE_NORMAL)) {
        if (flags & 0x8000) {
            delta = -0x20;
        } else {
            delta = 0x20;
        }
        if (ABS(actor->aimYaw + delta) < 0x1A1) {
            actor->aimYaw += delta;
        }
    } else if (actor->aimYaw != 0) {
        val   = actor->aimYaw >> 3;
        delta = val;
        temp  = val;
        if (ABS(temp) < 0x40) {
            val = 0x40;
            if (temp < 0) {
                val = -0x40;
            }
            delta = val;
        }
        actor->aimYaw -= delta;
        if (ABS(actor->aimYaw) < 0x41) {
            actor->aimYaw = 0;
        }
    }
}

static void func_80109818(Task* arg0)
{
    GameActor* inner;

    inner                 = arg0->work;
    inner->mode           = GAME_ACTOR_MODE_NORMAL;
    inner->state          = 4;
    inner->movementMode   = 0;
    inner->turnRateIndex  = 0;
    inner->animationState = 5;
    inner->statePhase     = 0;
    inner->rumblePosted   = 0;
}

/// Caps a level at 2.
static inline s32 _gpCapLevel(s32 level)
{
    s32 capped = 2;
    if (level < 3) {
        capped = level;
    }
    return capped;
}

static void func_80109844(Task* arg0)
{
    u8*             head;
    SVECTOR*        vec;
    GameActor*      inner;
    GameActor*      inner2;
    EffectSpawnArg* params;
    GfxCoord*       coord;
    s32             idx;
    s32             temp;
    s32             val;

    inner                    = arg0->work;
    temp                     = (u16)inner->pendingDamage / 12;
    head                     = SCRATCH_STACK_CURSOR(u8);
    params                   = &D_80113358;
    head                    -= 8;
    SCRATCH_STACK_CURSOR(u8) = head;
    vec                      = (SVECTOR*)head;
    temp                     = _gpCapLevel(temp);
    switch (inner->statePhase) {
        case 0:
            inner->statePhase  = 1;
            coord              = ((s8)inner->hitBodyIndex + inner->collisionBodies)->coord;
            params->spawnArgLo = (temp * 0x20) + 0x120;
            params->spawnArgHi = temp + 1;
            D_80113358.coord   = coord;
            inner->stateTimer  = 0;
            inner->actionValue = temp;
            /* fallthrough */
        case 1:
            if (inner->stateTimer == 0) {
                idx = 5;
                if (temp < 3) {
                    idx = 6;
                }
                inner->actionValue--;
                if (inner->actionValue == 0) {
                    inner->statePhase++;
                } else {
                    inner->stateTimer = 6;
                }
                vec->vx = 0;
                val     = 0;
                if ((s8)inner->hitBodyIndex == 0) {
                    val = -0x190;
                }
                vec->vy = val;
                vec->vz = 0;
                func_800FDB18(idx, params->coord, vec, params);
            } else {
                inner->stateTimer--;
            }
            break;
        case 2:
            break;
        case 3:
            inner2 = arg0->work;
            func_8010B210(arg0);
            inner2->recoveryTicks = 0x12;
            if (inner2->state != 0) {
                func_8010870C(arg0, 0xC);
            } else {
                func_801066DC(arg0, 0);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
}

static void func_80109A1C(Task* arg0)
{
    GameActor*      inner;
    EffectSpawnArg* params;
    GfxCoord*       coords;
    s32             idx;
    s32             temp;

    inner = arg0->work;
    switch (inner->statePhase) {
        case 0:
            inner->statePhase  = 1;
            inner->stateTimer  = 0;
            inner->actionValue = 0;
            /* fallthrough */
        case 1:
            if (inner->stateTimer == 0) {
                params = &D_80113358;
                inner->actionValue++;
                if (inner->actionValue == 3) {
                    inner->statePhase++;
                } else {
                    inner->stateTimer = 6;
                }
                coords        = &arg0->extra.tmd->coords[inner->actionValue + 1];
                params->coord = coords;
                temp          = (u16)((u16)inner->pendingDamage / 12);
                idx           = 2;
                if (temp < 3) {
                    idx = temp;
                }
                temp               = idx;
                params->spawnArgLo = (temp * 0x60) + 0xC0;
                params->spawnArgHi = temp + 1;
                func_800FDB18(3, coords, 0, params);
            } else {
                inner->stateTimer--;
            }
            break;
        case 2:
            break;
        case 3:
            func_8010B210(arg0);
            inner->recoveryTicks = 0x12;
            if (inner->state != 0) {
                func_8010870C(arg0, 0xC);
            } else {
                func_801066DC(arg0, 0);
            }
            break;
    }
}

/// Ends the actor's current action: clears the fields `func_8010B210` resets,
/// sets `recoveryTicks` to 0x12 and restarts the state machine in the base state
/// for its `state` mode.
static inline void _gpResumeBaseState(Task* arg0)
{
    GameActor* inner;

    inner = arg0->work;
    func_8010B210(arg0);
    inner->recoveryTicks = 0x12;
    if (inner->state != 0) {
        func_8010870C(arg0, 0xC);
    } else {
        func_801066DC(arg0, 0);
    }
}
