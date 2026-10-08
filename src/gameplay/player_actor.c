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
#include "effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/geometry.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/gpu_image_upload.h"
#include "item_use.h"
#include "gameplay/inventory.h"
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

#include "weapons/as12.h"
#include "weapons/m249.h"
#include "weapons/m4a1.h"
#include "weapons/gunblade.h"
#include "weapons/grenade_pistol.h"
#include "weapons/m4a1_bayonet.h"
#include "weapons/hypervelocity.h"
#include "weapons/m4a1_hammer.h"
#include "weapons/m4a1_grenade.h"
#include "weapons/m4a1_javelin.h"
#include "weapons/m4a1_pyke.h"
#include "weapons/m93r.h"
#include "weapons/m950.h"
#include "weapons/mp5a5.h"
#include "weapons/p08.h"
#include "weapons/p229.h"
#include "weapons/pa3.h"
#include "weapons/tonfa_baton.h"

#include "main/areas.h"
#include "main/task_types.h"

/// Equipped weapon indices used by player actor control.
enum {
    PLAYER_ACTOR_WEAPON_HYPERVELOCITY = 22,
    PLAYER_ACTOR_WEAPON_GUNBLADE      = 23,
    PLAYER_ACTOR_WEAPON_HAMMER        = 25,
    PLAYER_ACTOR_WEAPON_PYKE          = 28,
};

/// Scripted states entered by the movement, animation and attack messages.
enum {
    PLAYER_ACTOR_SCRIPTED_ANIMATION_STATE      = 1,
    PLAYER_ACTOR_SCRIPTED_TURN_STATE           = 2,
    PLAYER_ACTOR_SCRIPTED_STAIR_CLIMB_STATE    = 3,
    PLAYER_ACTOR_SCRIPTED_MOVE_TO_STATE        = 4,
    PLAYER_ACTOR_SCRIPTED_WALK_STEPS_STATE     = 5,
    PLAYER_ACTOR_SCRIPTED_PRESS_HOLD_STATE     = 6,
    PLAYER_ACTOR_SCRIPTED_RUN_TO_STATE         = 8,
    PLAYER_ACTOR_SCRIPTED_ATTACK_STATE         = 10,
    PLAYER_ACTOR_MESSAGE_ENTER_SCRIPTED_ATTACK = 1024,
};

/// Normal-mode state entered by the paralysis timer and its entry helper.
enum { PLAYER_ACTOR_NORMAL_PARALYSIS_STATE = 7 };

/// Maximum per-call lock-elevation change, in 4096 angle units per turn.
enum { PLAYER_ACTOR_LOCK_ELEVATION_STEP = 0x30 };

/// Phases and callback-tick spacing of the spark/puff and body-blast hit reactions.
enum {
    PLAYER_ACTOR_EFFECT_HIT_PHASE_START     = 0,
    PLAYER_ACTOR_EFFECT_HIT_PHASE_EMIT      = 1,
    PLAYER_ACTOR_EFFECT_HIT_PHASE_WAIT_CLIP = 2,
    PLAYER_ACTOR_EFFECT_HIT_PHASE_RESUME    = 3,
    PLAYER_ACTOR_EFFECT_HIT_DELAY_TICKS     = 6,
    PLAYER_ACTOR_EFFECT_HIT_DAMAGE_STEP     = 12,
};

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

/// Encodings used by the effect drawers in this translation unit.
enum {
    EFFECT_DRAW_TASK_NEW                   = 0,
    EFFECT_DRAW_TASK_ACTIVE                = 1,
    EFFECT_DRAW_SIZE_MASK                  = 0xFFF,
    EFFECT_DRAW_ANGLE_MASK                 = 0xFFF,
    EFFECT_DRAW_FULL_TURN                  = ONE,
    EFFECT_DRAW_QUARTER_TURN               = ONE / 4,
    EFFECT_DRAW_FRACTION_BITS              = 12,
    EFFECT_DRAW_TEXTURED_QUAD_PACKET_WORDS = 9,
    EFFECT_DRAW_TEXTURED_QUAD              = 0x2C,
    EFFECT_DRAW_ADDITIVE_TEXTURED_QUAD     = 0x2E,
    EFFECT_DRAW_RAW_ADDITIVE_TEXTURED_QUAD = 0x2F,
    EFFECT_DRAW_RAW_TEXTURE                = 1,
    EFFECT_DRAW_SEMITRANSPARENT            = 2,
};

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
/// `intensity` for `durationUnits` and the latch is set. `padPostVibrationRequest`
/// doubles each unit into serviced controller polls. The stored pulse is full
/// intensity for two units. `field_1` is never read; its role is unproven.
typedef struct {
    u8  intensity;     // Variable-motor drive (0..255)
    u8  field_1;       // Never read; role unproven
    s16 durationUnits; // Duration before `padPostVibrationRequest` doubles it into controller polls
} _PlayerActorVibrationPreset;
STATIC_ASSERT_SIZEOF(_PlayerActorVibrationPreset, 0x4);

extern EffectSpawnArg D_80112C74;

extern s32 D_80112C7C[];

/// CLUT X positions, in pixels, for effect sprites drawn from texture page
/// 0x29, in five rows of two. Each entry places a CLUT on VRAM row 0x10A; the
/// drawing function fixes the row and its caller picks the column.
extern u16 D_80112964[5][2];

/// Spawn-id words indexed by the 3-digit packing of `Gp_StateC08.attachId`
/// `(hundreds-1)*9 + (tens-1)*3 + ones - 1`. `_effectControlTask07State1` uses this
/// when `Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_RELEASED`, and
/// `D_80112A50` when `Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_CHARGE`.
extern s32 D_80112978[];

extern s32 D_80112A50[];

/// Spawn-id words for `effectControlTaskAE`, indexed with the same 3-digit packing
/// of `Gp_StateC08.attachId` as `D_80112978`; the value becomes the task's
/// `Task::spawnArg1` sound id.
extern s32 D_80112B94[];

/// `GfxCoord` index parallel to `D_80112978`. `_effectControlTask07State1` adds
/// it onto `TmdObject.coords` when `Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_RELEASED`.
extern u16 D_80112B28[];

/// 4 packed RGB-nibble colors. `effectPolyTaskC1` indexes with
/// `TaskSpawnArg::halves.high & 3` and stores the halfword in `EffectWork.period`.
extern u16 D_80112C6C[];

/// u8 taskSpawn type bases. `playerActorSpawnAttachment` indexes
/// `D_80112DFC[rigIndex + gPlayerStatus.resourceVariant - 2]`.
extern u8 D_80112DFC[];

/// The one variable-motor vibration preset. The poster indexes it with the
/// low halfword of its selector; the only call passes 0.
extern _PlayerActorVibrationPreset D_80112E28[];

/// s16 scale rows indexed by `GameActor.movementMode`. `playerActorStepMovement` divides
/// the normalized matrix-column by `D_80112E10[movementMode]`.
extern s16 D_80112E10[];

/// u16 facing-step rows indexed by `GameActor.turnRateIndex`. `playerActorUpdateFacing`
/// adds `D_80112E20[turnRateIndex] * turnSign` onto `rotation.vy` (masked `0xFFF`).
extern u16 D_80112E20[];

/// 2-wide rows of `GfxCoord` indices. `_playerActorGetCharacterPartCoord` indexes
/// `D_80112E2C[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1][partSelector]`.
extern u8 D_80112E2C[][2];

/// u16 turn-rate rows indexed by `gPlayerStatus.weapon`. `playerActorAimYawToLock`
/// clamps the wrapped yaw delta to this value (or 1.5x when
/// `equipmentHasEffect(EQUIPMENT_EFFECT_QUICK_FIRE)` is set).
extern u16 D_80112E30[];

/// NULL-terminated `GpuImageUpload*` lists for `_playerActorTickTextureSequences`. Indexed as
/// `table[type * 4 + gPlayerStatus.resourceVariant - 5][frame]`. `D_80112E74` is
/// the `textureSequenceA` sequence; `D_80112EB4` is the `textureSequenceB` sequence.
extern GpuImageUpload** D_80112E74[];

extern GpuImageUpload** D_80112EB4[];

/// Per-item flag byte indexed by `gPlayerStatus.weapon`. Nonzero makes
/// `_playerActorNormalState2` / `_playerActorTickScriptedAttack` pass `GameActor.attackButton` (the current
/// primary/secondary fire-input selector) to `playerActorQueryWeaponLoads` instead of the default 1.
extern u8 D_80112EF8[];

/// 2-wide rows indexed by `gPlayerStatus.weapon`. Zero at `[i][0]`
/// makes `playerActorEnterReload` abort the battle-end path (`statePhase = 0x3E8`).
extern u8 D_80112F1C[][2];

/// 0x10-byte `VECTOR` rows indexed by `playerActorInitWeaponCollision` weaponId: where the
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

/// Four-entry `Task::state` dispatcher: `_playerActorInitWork`, `_playerActorWorkState1`,
/// `_playerActorWorkState2`, `_playerActorTeardown`.
static const TaskFuncTable4 Gp_PlayerWorkStates;

/// Per-weapon handlers, indexed by `PlayerStatus::weapon` and copied by
/// `_playerActorDispatchWeaponAttack`. Most live in the weapon overlay loaded at the time;
/// `_playerActorNoWeaponAttack` serves the weapons with none.
static const _PlayerActorWeaponAttacks D_800978BC;

/// `mode` dispatcher: `_playerActorTickNormal`, `_playerActorTickDamage`, `_playerActorTickScripted`.
static const TaskFuncTable3 Gp_PlayerModeFns;

/// `state` dispatcher copied by `_playerActorTickNormal`.
static const TaskFuncTable8 D_8009794C;

/// `hitRegion` dispatcher: three slots of `_playerActorTickDamageReaction`, then `_playerActorDamageHitRegion3`.
static const TaskFuncTable4 Gp_PlayerMode1States;

/// `state` dispatcher copied by `_playerActorTickScripted`.
static const TaskFuncTable12 Gp_PlayerMode2States;

static s32 _playerActorPlayScriptedAnimation(Task* task, s32 unusedMessageId, const AnimationPlayRequest* request, s32 unusedSecondArg);

/// Additional player-table IDs sharing indexed animation playback and its request contract.
enum {
    PLAYER_ACTOR_MESSAGE_PLAY_ANIMATION_ALIAS_1002 = 1002,
    PLAYER_ACTOR_MESSAGE_PLAY_ANIMATION_ALIAS_1003 = 1003,
    PLAYER_ACTOR_MESSAGE_PLAY_ANIMATION_ALIAS_1004 = 1004,
};

static s32 _playerActorEnterScriptedAttack(Task* task, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg);

static void _effectDrawProjectileSprite(const Task* task);

static void _effectDrawGroundDecal(const GfxCoord* coord, s32 halfSize, s16 brightness, u16 palette);

static void _effectDrawGravityParticle(const Task* task, s32 particleVariant, const u8 tintRgb[3]);

static void _effectDrawAnimatedGroundQuad(const GfxCoord* coord, s32 halfSize, u16 frame, u16 palette);

static void _effectControlTask07State1(Task* unusedTask);

static void _effectDrawRadialTriangles(const GfxCoord* coord, s32 radius, s32 rayCount, const u8* rgb);

static void _effectControlTask07State0(Task* task);

static void _effectDrawDeathFlame(Task* task);

static void _effectDrawSparkBurstBillboard(const GfxCoord* coord, u16 frame, u32 packedSizePalette, s16 angle);

static inline void _playerActorLinkCollisionBody(GameActor* actor, s32 bodyIndex, WorldCollisionBody* body, GfxCoord* coord, WorldCollisionContact* contacts, s16 offsetX, s16 offsetY,
                                                 s16 offsetZ, u16 radius, u16 flags);

static void _playerActorInitWork(Task* task);

static void _playerActorWorkState1(Task* task);

static void _playerActorWorkState2(Task* task);

static void _playerActorTeardown(Task* task);

static inline void _playerActorCapturePad(Task* task);

static inline MATRIX* _playerActorInvalidatePartMatrix(Task* task, s32 partIndex);

static inline s16 _playerActorShortestTurn(s16 currentAngle, s16 targetAngle);

static inline void _playerActorAimYawAtLock(GameActor* actor, _PlayerActorAimYawScratch* scratch, s16 minGroundDistance);

static inline void _playerActorPlaceAimPitchOrigin(_PlayerActorAimPitchScratch* scratch, GfxCoord* source, const SVECTOR* localOffset);

static inline s32 _playerActorGetAimPitchTargetDelta(const GameActor* actor, _PlayerActorAimPitchScratch* scratch);

static void _playerActorAimRollToLock(Task* task);

static void _playerActorAimDirectPitchToLock(Task* task);

static void _playerActorTickTextureSequences(Task* task);

inline static Task* _playerActorSpawnEquippedWeapon(Task* parent, s32 characterId, s32 weaponId);

static void _playerActorCapturePadState(Task* task);

static void _animationBindPlayerWeaponBank(Task* task);

static s32 _playerActorGetIdleHealthBand(void);

static s32 _playerActorSetMovementSignFromDisplacement(Task* task, const GameActorMoveBy* move);

static void _playerActorPostVibrationPreset(Task* task, s32 presetIndex);

static void _playerActorCaptureInteractionPress(Task* task);

static void _playerActorBeginScriptedControl(Task* task);

static s32 _playerActorReplaceAnimation(Task* task, s32 unusedMessageId, const AnimationPlayRequest* request, s32 unusedSecondArg);

static inline void _playerActorEnterScriptedMode(Task* task);

static s32 _playerActorClimbStairs(Task* task, s32 unusedMessageId, const GameActorStairClimb* climb, s32 unusedSecondArg);

static s32 _playerActorRunTo(Task* task, s32 unusedMessageId, const VECTOR3* destination, const GameActorMoveAnim* moveAnim);

static s32 _playerActorAwaitButtonPresses(Task* task, s32 unusedMessageId, const GameActorButtonPressHold* request, s32 unusedSecondArg);

static s32 _playerActorEnterScriptedPresentation(Task* task, s32 unusedMessageId, s32 clipVariant, s32 unusedSecondArg);

static s32 _playerActorEnterItemUse(Task* task, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg);

static s32 _animationCopyPlayerBankExtension(Task* unusedTask, s32 unusedMessageId, const AnimationBankCopyRequest* request, s32 unusedSecondArg);

static s32 _playerActorApplyDamage(Task* task, s32 unusedMessageId, s32 attackKey, s32 unusedSecondArg);

static s32 _playerActorSetRunMovement(Task* task, s32 unusedMessageId, s32 runEnabled, s32 unusedSecondArg);

static void _playerActorTickDirectChildSlots(Task* task);

static void _playerActorDispatchWeaponAttack(Task* task);

static s32 _playerActorTryStartAutomaticReload(Task* task, s32 attackButton);

static void _playerActorNoWeaponAttack(Task* unusedTask);

static void _playerActorTickNormal(Task* task);

static void _playerActorNormalState2(Task* task);

static void _playerActorNormalState5(Task* task);

static inline void _playerActorResumeAimLocomotion(Task* task, s32 blendFrames);

static void _playerActorNormalState6(Task* task);

static void _playerActorUpdateParalysis(Task* task);

static void _playerActorTickScriptedStairClimb(Task* task);

static void _playerActorTickScriptedAttack(Task* task);

static void _playerActorTickScriptedItemUse(Task* task);

static void _playerActorTick(Task* task);

static void _playerActorUpdateAimEntry(Task* task);

static void _playerActorRefreshAimLocomotionAnimation(Task* task);

static void _playerActorTryEnterAim(Task* task);

static void _playerActorUpdateIdleTurnAnimation(Task* task);

static void _playerActorUpdateAimTurnAnimation(Task* task);

static void _playerActorEnterAimLocomotion(Task* task, s32 blendFrames);

static void _playerActorEnterPeAction(Task* task);

static void _playerActorStartParalysis(Task* task);

static void _playerActorTickScriptedRunTo(Task* task);

static inline void _playerActorSetTargetNode(Task* task, WorldTargetNode* target);

static void _playerActorTickDamage(Task* task);

static void _playerActorTickScripted(Task* task);

static void func_80108FA0(Task* arg0);

static void _playerActorNormalState1(Task* task);

static void _playerActorUpdateAimExit(Task* task);

static void func_80109138(Task* arg0);

static void _playerActorTickDamageReaction(Task* task);

static void _playerActorDamageHitRegion3(Task* unusedTask);

static void _playerActorUpdateTurnInput(Task* task);

static void _playerActorUpdateMovementInput(Task* task);

static s32 _playerActorTryEnterPeAction(Task* task);

static void _playerActorUpdateAimRequest(Task* task);

static void _playerActorUpdateLockTargetFromPad(Task* task);

static void _playerActorTickScriptedWalkSteps(Task* task);

static void _playerActorWriteWeaponSoundVariant(s32* variantBitsOut);

static void _playerActorTickScriptedPresentation(Task* task);

static void _playerActorScriptedState9(Task* task);

static void _playerActorUpdateTurnYawOffset(Task* task);

static void func_80109818(Task* arg0);

static inline s32 _playerActorClampHitEffectLevel(s32 hitEffectLevel);

static void _playerActorTickSparkPuffHit(Task* task);

static void _playerActorTickBodyBlastHit(Task* task);

/// Ends the actor's current action: clears the fields `playerActorClearPendingHit` resets,
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

void func_p08_snail_8011D1D8(Task* arg0);
void func_mongoose_8011D1D8(Task* arg0);

void func_sp12_8011D1DC(Task* arg0);

void func_mp5a5_p1_8011DDA4(Task* arg0);
void func_mp5a5_p2_8011DDA4(Task* arg0);

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
    { ANIMATION_MESSAGE_PLAY, _playerActorPlayScriptedAnimation },
    { GAME_ACTOR_MESSAGE_PLACE, playerActorPlace },
    { PLAYER_ACTOR_MESSAGE_PLAY_ANIMATION_ALIAS_1002, _playerActorPlayScriptedAnimation },
    { PLAYER_ACTOR_MESSAGE_PLAY_ANIMATION_ALIAS_1003, _playerActorPlayScriptedAnimation },
    { PLAYER_ACTOR_MESSAGE_PLAY_ANIMATION_ALIAS_1004, _playerActorPlayScriptedAnimation },
    { ANIMATION_MESSAGE_IS_PLAYING, playerActorIsAnimationPlaying },
    { GAME_ACTOR_MESSAGE_TURN_TO_YAW, playerActorTurnToYaw },
    { GAME_ACTOR_MESSAGE_CLIMB_STAIRS, _playerActorClimbStairs },
    { GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, playerActorIsScriptedMotionPending },
    { GAME_ACTOR_MESSAGE_END_SCRIPTED, playerActorEndScripted },
    { GAME_ACTOR_MESSAGE_MOVE_TO, playerActorMoveTo },
    { GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, playerActorSetModelDraw },
    { ANIMATION_MESSAGE_INSTALL_AND_PLAY, playerActorInstallScriptedAnimation },
    { GAME_ACTOR_MESSAGE_ATTACH_TO_COORD, playerActorAttachToCoord },
    { GAME_ACTOR_MESSAGE_WALK_STEPS, playerActorWalkSteps },
    { ANIMATION_MESSAGE_COPY_BANK_EXTENSION, _animationCopyPlayerBankExtension },
    { GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, _playerActorAwaitButtonPresses },
    { GAME_ACTOR_MESSAGE_APPLY_DAMAGE, _playerActorApplyDamage },
    { PLAYER_ACTOR_MESSAGE_ENTER_SCRIPTED_PRESENTATION, _playerActorEnterScriptedPresentation },
    { GAME_ACTOR_MESSAGE_RUN_TO, _playerActorRunTo },
    { GAME_ACTOR_MESSAGE_SET_RUN_MOVEMENT, _playerActorSetRunMovement },
    { ANIMATION_MESSAGE_SET_RATE, playerActorSetAnimationRate },
    { GAME_ACTOR_MESSAGE_MOVE_BY, playerActorMoveBy },
    { ANIMATION_MESSAGE_REPLACE_AND_PLAY, _playerActorReplaceAnimation },
    { PLAYER_ACTOR_MESSAGE_ENTER_SCRIPTED_ATTACK, _playerActorEnterScriptedAttack },
    { GAME_ACTOR_MESSAGE_SET_TEXTURE_SEQUENCE, playerActorSetTextureSequence },
    { PLAYER_ACTOR_MESSAGE_ENTER_ITEM_USE, _playerActorEnterItemUse },
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
    { { { TASK_BODY_NONE, 192 } }, effectPlayerBodyBlastTask, { NULL } },
    { { { TASK_BODY_NONE, 192 } }, effectPlayerBodyPuffTask, { NULL } },
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

void effectSpriteTask46(Task* task)
{
    enum {
        EFFECT_GROUND_DECAL_NEUTRAL_BRIGHTNESS = 128,
        EFFECT_GROUND_DECAL_DIM_BRIGHTNESS     = 64,
        EFFECT_GROUND_DECAL_GROW_STEP          = 6,
        EFFECT_GROUND_DECAL_FADE_STEP          = 4,
    };
    EffectWork*           work;
    ModelObjectCoordBody* body;
    GfxCoord*             coord;
    s16                   effectControl;
    s32                   paletteBits;

    work          = task->spawnArg2.pointer;
    body          = task->extra.coordBody;
    effectControl = gRoomEffectState->effectControl;
    coord         = body->coord;
    // Frozen effects still draw; cancellation retains the final draw after teardown.
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        _effectDrawGroundDecal(coord, work->scale, work->period, work->step);
        return;
    }

    // Rotate the decal in its local ground plane, then grow, dim or release it.
    actorRenderComposeCoord(coord);
    switch (task->state) {
        case EFFECT_GROUND_DECAL_STATE_NEW:
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gfxRotMatrixY(&coord->coord, (gRandomLcgState >> 16) & EFFECT_DRAW_ANGLE_MASK, GRAPHICS_ROTATION_REPLACE);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            work->angle         = task->spawnArg1.halves.low & EFFECT_DRAW_SIZE_MASK;
            paletteBits         = task->spawnArg1.halves.high;
            work->step          = paletteBits & 0xF;
            if (task->spawnArg1.value & EFFECT_GROUND_DECAL_START_FULL_BRIGHT) {
                work->period = EFFECT_GROUND_DECAL_NEUTRAL_BRIGHTNESS;
                work->scale  = work->angle;
                task->state  = EFFECT_GROUND_DECAL_STATE_HOLD;
            } else if (task->spawnArg1.value & EFFECT_GROUND_DECAL_START_DIM) {
                work->period = EFFECT_GROUND_DECAL_DIM_BRIGHTNESS;
                work->scale  = work->angle;
                task->state  = EFFECT_GROUND_DECAL_STATE_DIM;
            } else {
                work->period = EFFECT_GROUND_DECAL_NEUTRAL_BRIGHTNESS;
                work->scale  = 0;
                task->state  = EFFECT_GROUND_DECAL_STATE_GROW;
            }
            break;
        case EFFECT_GROUND_DECAL_STATE_GROW:
            _effectDrawGroundDecal(coord, work->scale, work->period, work->step);
            if (work->scale < work->angle) {
                work->scale += EFFECT_GROUND_DECAL_GROW_STEP;
            } else {
                work->scale = work->angle;
                task->state = EFFECT_GROUND_DECAL_STATE_DIM;
            }
            break;
        case EFFECT_GROUND_DECAL_STATE_DIM:
            _effectDrawGroundDecal(coord, work->scale, work->period, work->step);
            if (work->period >= EFFECT_GROUND_DECAL_DIM_BRIGHTNESS + 1) {
                work->period--;
            }
            break;
        case EFFECT_GROUND_DECAL_STATE_FADE:
            _effectDrawGroundDecal(coord, work->scale, work->period, work->step);
            work->period -= EFFECT_GROUND_DECAL_FADE_STEP;
            if (work->period < EFFECT_GROUND_DECAL_FADE_STEP) {
                effectKillTask(work, task);
            }
            break;
        case EFFECT_GROUND_DECAL_STATE_HOLD:
            _effectDrawGroundDecal(coord, work->scale, work->period, work->step);
            break;
    }
}

/// Projects the projectile sprite's cached world position into its screen centre.
///
/// Requires a live, word-aligned scratch block and a composed coordinate in
/// the current world-to-screen matrix's frame. XYZ narrow to signed halfwords
/// before RTPS; the paired screen halfwords and FLAG are written, while depth
/// stays in the GTE for the caller to read after rejecting negative FLAG.
/// Borrows both arguments and changes GTE matrix/projection state.
static inline void _effectProjectProjectileCentre(EffectCentreScratch* scratch, const GfxCoord* coord)
{
    scratch->worldPoint.vx = coord->workm.t[0];
    scratch->worldPoint.vy = coord->workm.t[1];
    scratch->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->worldPoint);
    gte_rtps();
    gte_stsxy(&scratch->screenX);
    gte_stflg(&scratch->projectionFlags);
}

/// Draws the projectile-following sprite of effect task 0x81 as an axis-aligned billboard.
///
/// Requires the task's live coordinate body and owned EffectWork, a composed
/// cached translation, scratch/GTE projection state and a writable GPU arena.
/// step indexes palette columns 0..1 (the task initializes it to 1); age selects
/// one of eight 16-texel frames, advancing every two running ticks. scale supplies the
/// perspective size; angle zero selects opaque raw texels, nonzero additive.
/// Sizes and arithmetic must fit s32. Depth is SZ3 / 4 plus one; a negative
/// projection FLAG skips drawing. Borrows all inputs and retains no pointers.
static void _effectDrawProjectileSprite(const Task* task)
{
    enum {
        EFFECT_PROJECTILE_SPRITE_DEPTH_BIAS   = 1,
        EFFECT_PROJECTILE_SPRITE_FRAME_COUNT  = 8,
        EFFECT_PROJECTILE_SPRITE_FRAME_SHIFT  = 1,
        EFFECT_PROJECTILE_SPRITE_CELL_SIZE    = 16,
        EFFECT_PROJECTILE_SPRITE_TEXTURE_V    = 184,
        EFFECT_PROJECTILE_SPRITE_TEXTURE_PAGE = getTPage(0, GPU_BLEND_ADD, 576, 0),
        EFFECT_PROJECTILE_SPRITE_CLUT_ROW     = 1,
        EFFECT_PROJECTILE_SPRITE_CLUT_Y       = 266,
    };
    EffectCentreScratch*        scratch;
    const ModelObjectCoordBody* body;
    const GfxCoord*             coord;
    const EffectWork*           work;
    POLY_FT4*                   quad;

    body    = task->extra.coordBody;
    coord   = body->coord;
    work    = task->spawnArg2.pointer;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
    _effectProjectProjectileCentre(scratch, coord);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&scratch->depth);
        scratch->depth += EFFECT_PROJECTILE_SPRITE_DEPTH_BIAS;
        quad            = gGpuPrimCursor;
        gGpuPrimCursor  = quad + 1;
        setPolyFT4(quad);
        setShadeTex(quad, 1);
        setSemiTrans(quad, work->angle);
        // Use raw texels; angle is the caller-controlled transparency toggle.
        quad->tpage = EFFECT_PROJECTILE_SPRITE_TEXTURE_PAGE;
        quad->clut  = getClut(D_80112964[EFFECT_PROJECTILE_SPRITE_CLUT_ROW][work->step], EFFECT_PROJECTILE_SPRITE_CLUT_Y);
        setUV4(quad, ((work->age >> EFFECT_PROJECTILE_SPRITE_FRAME_SHIFT) & (EFFECT_PROJECTILE_SPRITE_FRAME_COUNT - 1)) * EFFECT_PROJECTILE_SPRITE_CELL_SIZE, EFFECT_PROJECTILE_SPRITE_TEXTURE_V,
               (((work->age >> EFFECT_PROJECTILE_SPRITE_FRAME_SHIFT) & (EFFECT_PROJECTILE_SPRITE_FRAME_COUNT - 1)) * EFFECT_PROJECTILE_SPRITE_CELL_SIZE) + EFFECT_PROJECTILE_SPRITE_CELL_SIZE - 1, EFFECT_PROJECTILE_SPRITE_TEXTURE_V,
               ((work->age >> EFFECT_PROJECTILE_SPRITE_FRAME_SHIFT) & (EFFECT_PROJECTILE_SPRITE_FRAME_COUNT - 1)) * EFFECT_PROJECTILE_SPRITE_CELL_SIZE, EFFECT_PROJECTILE_SPRITE_TEXTURE_V + EFFECT_PROJECTILE_SPRITE_CELL_SIZE - 1,
               (((work->age >> EFFECT_PROJECTILE_SPRITE_FRAME_SHIFT) & (EFFECT_PROJECTILE_SPRITE_FRAME_COUNT - 1)) * EFFECT_PROJECTILE_SPRITE_CELL_SIZE) + EFFECT_PROJECTILE_SPRITE_CELL_SIZE - 1, EFFECT_PROJECTILE_SPRITE_TEXTURE_V + EFFECT_PROJECTILE_SPRITE_CELL_SIZE - 1);
        scratch->screenExtent = ((work->scale * (EFFECT_PROJECTILE_SPRITE_CELL_SIZE - 1)) / scratch->depth) >> 1;
        quad->x0 = quad->x2 = scratch->screenX - (u16)scratch->screenExtent;
        quad->x1 = quad->x3 = scratch->screenX + (u16)scratch->screenExtent;
        quad->y0 = quad->y1 = scratch->screenY - (u16)scratch->screenExtent;
        quad->y2 = quad->y3 = scratch->screenY + (u16)scratch->screenExtent;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

/// Projects four staged decal corners through `GsWSMATRIX` into screen pixels.
///
/// `quadScratch` must be writable and word-aligned, with four initialized
/// SVECTOR positions in GPU strip order and the input frame of `GsWSMATRIX`.
/// The caller loads that matrix's translation and the GTE projection settings.
/// Stores corner 0 before RTPT replaces the screen FIFO, then corners 1..3 and
/// the RTPT FLAG word. Corner 3's depth remains in SZ3; the scratch depth is
/// untouched. Borrows the block for this call and leaves GTE state changed.
static inline void _effectProjectGroundDecalCorners(EffectQuadScratch* quadScratch)
{
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&quadScratch->vertices[0]);
    gte_rtps();
    gte_stsxy(&quadScratch->screenCorners[0]);
    gte_ldv3(&quadScratch->vertices[1], &quadScratch->vertices[2], &quadScratch->vertices[3]);
    gte_rtpt();
    gte_stsxy3(&quadScratch->screenCorners[1], &quadScratch->screenCorners[2], &quadScratch->screenCorners[3]);
    gte_stflg(&quadScratch->projectionFlags);
}

/// Draws an additive 56-texel ground decal in a composed coordinate's local XZ plane.
///
/// halfSize is the half-side in coordinate units. Corners are narrowed to s16
/// after rotation and translation; sign products must fit s32. brightness supplies the low byte of each
/// RGB channel (128 is neutral); palette selects CLUT X = 16 + 272 * palette
/// on row 266. The coordinate is borrowed; no pointer survives this call.
/// Only the final RTPT FLAG rejects the quad; ordering uses corner 3's SZ3 / 4
/// plus 32. The caller supplies GTE projection settings and a live GPU arena.
static void _effectDrawGroundDecal(const GfxCoord* coord, s32 halfSize, s16 brightness, u16 palette)
{
    enum {
        EFFECT_GROUND_DECAL_DEPTH_BIAS    = 32,
        EFFECT_GROUND_DECAL_TEXTURE_PAGE  = getTPage(0, GPU_BLEND_ADD, 576, 0),
        EFFECT_GROUND_DECAL_TEXTURE_V     = 200,
        EFFECT_GROUND_DECAL_UV_SPAN       = 55,
        EFFECT_GROUND_DECAL_CLUT_X        = 16,
        EFFECT_GROUND_DECAL_CLUT_X_STRIDE = 272,
        EFFECT_GROUND_DECAL_CLUT_Y        = 266,
    };
    EffectQuadScratch* quadScratch;
    s32                cornerIndex;
    POLY_FT4*          quad;

    quadScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectQuadScratch);
    // Transform local XZ corners into narrowed world coordinates.
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(D_80111E38); cornerIndex++) {
        quadScratch->vertices[cornerIndex].vx = (u16)D_80111E38[cornerIndex].axis0Sign * halfSize;
        quadScratch->vertices[cornerIndex].vy = 0;
        quadScratch->vertices[cornerIndex].vz = (u16)D_80111E38[cornerIndex].axis1Sign * halfSize;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&quadScratch->vertices[cornerIndex]);
        gte_rtv0();
        gte_stsv(&quadScratch->vertices[cornerIndex]);
        quadScratch->vertices[cornerIndex].vx += coord->workm.t[0];
        quadScratch->vertices[cornerIndex].vy += coord->workm.t[1];
        quadScratch->vertices[cornerIndex].vz += coord->workm.t[2];
    }

    gte_SetTransMatrix(&GsWSMATRIX);
    _effectProjectGroundDecalCorners(quadScratch);
    if (quadScratch->projectionFlags >= 0) {
        gte_stszotz(&quadScratch->depth);
        quadScratch->depth += EFFECT_GROUND_DECAL_DEPTH_BIAS;
        quad                = gGpuPrimCursor;
        gGpuPrimCursor      = quad + 1;
        setlen(quad, EFFECT_DRAW_TEXTURED_QUAD_PACKET_WORDS);
        setcode(quad, EFFECT_DRAW_ADDITIVE_TEXTURED_QUAD);
        quad->tpage = EFFECT_GROUND_DECAL_TEXTURE_PAGE;
        quad->clut  = getClut(palette * EFFECT_GROUND_DECAL_CLUT_X_STRIDE + EFFECT_GROUND_DECAL_CLUT_X, EFFECT_GROUND_DECAL_CLUT_Y);
        quad->v0    = EFFECT_GROUND_DECAL_TEXTURE_V;
        quad->v1    = EFFECT_GROUND_DECAL_TEXTURE_V;
        quad->r0    = brightness;
        quad->g0    = brightness;
        quad->b0    = brightness;
        quad->u0    = 0;
        quad->u1    = EFFECT_GROUND_DECAL_UV_SPAN;
        quad->u2    = 0;
        quad->v2    = EFFECT_GROUND_DECAL_TEXTURE_V + EFFECT_GROUND_DECAL_UV_SPAN;
        quad->u3    = EFFECT_GROUND_DECAL_UV_SPAN;
        quad->v3    = EFFECT_GROUND_DECAL_TEXTURE_V + EFFECT_GROUND_DECAL_UV_SPAN;
        quad->x0    = quadScratch->screenCorners[0].vx;
        quad->y0    = quadScratch->screenCorners[0].vy;
        quad->x1    = quadScratch->screenCorners[1].vx;
        quad->y1    = quadScratch->screenCorners[1].vy;
        quad->x2    = quadScratch->screenCorners[2].vx;
        quad->y2    = quadScratch->screenCorners[2].vy;
        quad->x3    = quadScratch->screenCorners[3].vx;
        quad->y3    = quadScratch->screenCorners[3].vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)quadScratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadScratch);
}

/// Makes one random burst-particle trial while room effects are running.
///
/// Advances the shared LCG once and spawns when its upper halfword is divisible
/// by three. Requires an unflagged size in work->scale (0..4095); the burst
/// callers use the reduced projectile size. Borrows work and coord for this
/// call. The particle copies the coordinate pose and flies independently; its
/// retained parent pointer is not followed with these unflagged size arguments.
static inline void _effectSpawnProjectileBurstParticle(const EffectWork* work, GfxCoord* coord)
{
    enum { EFFECT_PROJECTILE_BURST_RANDOM_CHOICES = 3 };

    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        return;
    }
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    if ((u16)((gRandomLcgState >> 16) % (u32)EFFECT_PROJECTILE_BURST_RANDOM_CHOICES) == 0) {
        effectSpawn(EFFECT_PROJECTILE_BURST_PARTICLE, coord, (s32)work->scale, NULL);
    }
}

/// Copies a projectile's composed pose into the glow's view-parented coordinate.
///
/// glowCoord must be a distinct writable node parented to `gGfxViewCoord`;
/// projectileCoord and both parent chains must be live, writable and acyclic.
/// Requires a current view cache in the projectile's composition frame. Rebuilds
/// the projectile cache, converts that pose into the glow's parent frame, then
/// rebuilds the glow cache. Matrices use Q12 rotations and signed game-coordinate
/// translations. Borrows 48 scratch bytes and changes GTE state; retains no
/// pointer. The view rotation must be orthonormal for its transpose to invert it.
static inline void _effectSyncProjectileGlowCoord(GfxCoord* glowCoord, GfxCoord* projectileCoord)
{
    MATRIX* viewTransform;

    actorRenderComposeCoord(projectileCoord);
    glowCoord->workm = projectileCoord->workm;
    gte_SetRotMatrix(&projectileCoord->workm);
    gte_SetTransMatrix(&projectileCoord->workm);
    viewTransform = &gGfxViewCoord.workm;
    // Preserve the projectile's composed pose under the glow's view parent.
    gfxMakeRelativeTransform(viewTransform, &glowCoord->workm, &glowCoord->coord);
    glowCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(glowCoord);
}

void effectProjectileGlowTask(Task* task)
{
    enum {
        EFFECT_PROJECTILE_GLOW_INITIAL_SIZE      = 640,
        EFFECT_PROJECTILE_GLOW_PALETTE           = 1,
        EFFECT_PROJECTILE_GLOW_TRAIL_RANDOM_MASK = 3,
        EFFECT_PROJECTILE_GLOW_BURST_AGE_LIMIT   = 16,
        // Subtractive puff: size bias 512, two ticks per frame, blend mode 2.
        EFFECT_PROJECTILE_GLOW_BURST_PUFF_BASE = 0x22200,
        // Subtractive puff: one tick per frame, blend mode 2; size supplied separately.
        EFFECT_PROJECTILE_GLOW_TRAIL_PUFF_BITS = 0x21000,
    };
    EffectWork*           work;
    ModelObjectCoordBody* body;
    GfxCoord*             coord;
    GfxCoord*             parent;
    s16                   effectControl;

    body          = task->extra.coordBody;
    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = body->coord;
    parent        = work->parent;
    if (effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        effectKillTask(work, task);
        return;
    }

    // Follow the projectile even during a paused draw or either impact phase.
    _effectSyncProjectileGlowCoord(coord, parent);

    switch (task->spawnArg1.value) {
        case EFFECT_PROJECTILE_GLOW_NEW:
            work->scale           = EFFECT_PROJECTILE_GLOW_INITIAL_SIZE;
            work->step            = EFFECT_PROJECTILE_GLOW_PALETTE;
            work->angle           = 0; // Retained initialization; this controller never reads it.
            task->spawnArg1.value = EFFECT_PROJECTILE_GLOW_FLIGHT;
            if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                break;
            }
            effectSpawn(EFFECT_TRAIL_PUFF, coord, work->scale + EFFECT_PROJECTILE_GLOW_BURST_PUFF_BASE + work->scale, NULL);
            break;
        case EFFECT_PROJECTILE_GLOW_FLIGHT:
            _effectDrawProjectileSprite(task);
            if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                break;
            }
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (((gRandomLcgState >> 16) & EFFECT_PROJECTILE_GLOW_TRAIL_RANDOM_MASK) == 0) {
                effectSpawn(EFFECT_TRAIL_PUFF, coord, work->scale + EFFECT_PROJECTILE_GLOW_TRAIL_PUFF_BITS, NULL);
            }
            work->age++;
            break;
        case EFFECT_PROJECTILE_GLOW_QUAD_BURST:
            // Setup and quad age freeze on pause; the last accepted frame still draws.
            if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                if (work->index == 0) {
                    effectSpawn(EFFECT_TRAIL_PUFF, coord, work->scale + EFFECT_PROJECTILE_GLOW_BURST_PUFF_BASE, NULL);
                    work->index   = 1;
                    work->age     = 0;
                    work->scale >>= 2;
                    gfxSetRotIdentity(&coord->coord);
                }
                work->age += (u16)gDisplayState.animFrame & 1;
            }
            if (work->age < EFFECT_PROJECTILE_GLOW_BURST_AGE_LIMIT) {
                _effectDrawAnimatedGroundQuad(coord, work->scale, work->age >> 1, work->step);
            } else {
                task->spawnArg1.value = EFFECT_PROJECTILE_GLOW_KILL;
                break;
            }
            _effectSpawnProjectileBurstParticle(work, coord);
            break;
        case EFFECT_PROJECTILE_GLOW_PARTICLE_BURST:
            if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING && work->index == 0) {
                effectSpawn(EFFECT_TRAIL_PUFF, coord, work->scale + EFFECT_PROJECTILE_GLOW_BURST_PUFF_BASE, NULL);
                work->index   = 1;
                work->age     = 0;
                work->scale >>= 2;
            }
            // This burst expires during pause even though setup and emissions wait.
            work->age++;
            if (work->age >= EFFECT_PROJECTILE_GLOW_BURST_AGE_LIMIT) {
                task->spawnArg1.value = EFFECT_PROJECTILE_GLOW_KILL;
                break;
            }
            _effectSpawnProjectileBurstParticle(work, coord);
            break;
        case EFFECT_PROJECTILE_GLOW_KILL:
            effectKillTask(work, task);
            break;
    }
}

void effectSpriteTask55(Task* task)
{
    enum {
        EFFECT_PUFF_CLUT_BASE            = getClut(16, 266),
        EFFECT_PUFF_CLUT_PALETTE_STRIDE  = 14,
        EFFECT_PUFF_CLUT_OFFSET_MASK     = 0x42BE,
        EFFECT_PUFF_PALETTE_SHIFT        = 28,
        EFFECT_PUFF_TEXTURE_PAGE_BASE    = getTPage(0, GPU_BLEND_AVERAGE, 576, 0),
        EFFECT_PUFF_VELOCITY_SCALE_SHIFT = 2,
        EFFECT_PUFF_DEFAULT_SIZE         = 512,
        EFFECT_PUFF_CELL_SIZE            = 32,
        EFFECT_PUFF_TEXTURE_V            = 120,
        EFFECT_PUFF_FRAME_COUNT          = 8,
        EFFECT_PUFF_RANDOM_VELOCITY      = 0x100000,
        EFFECT_PUFF_SCALE_VELOCITY       = 0x01000000,
        EFFECT_PUFF_GRAVITY              = 6,
        EFFECT_PUFF_INITIAL_LIFT         = 24,
        EFFECT_PUFF_PERIOD_MASK          = 0xF000,
        EFFECT_PUFF_PALETTE_BIT          = 0x10000000,
    };
    EffectWork*         work;
    GfxCoord*           coord;
    EffectShapeScratch* scratch;
    POLY_FT4*           quad;
    s16                 effectControl;
    s32                 requestedSize;
    s32                 size;
    s32                 randomState;
    s32                 ticksPerFrame;
    s32                 blendBits;
    SVECTOR*            velocity;

/// Writes rotated screen corners using the live scratch, quad and work locals.
///
/// Use as a standalone statement in a braced block. Captures those three
/// locals and the enclosing task's cell-size constant, retains repeated field
/// reads and narrows each offset before adding it to the projected origin.
#define EFFECT_HIT_PUFF_SET_CORNERS()                                                                                                                                        \
    scratch->extent.corner.x = (((work->scale * (EFFECT_PUFF_CELL_SIZE - 1)) / scratch->depth) * rsin(work->angle)) >> EFFECT_DRAW_FRACTION_BITS;                            \
    scratch->extent.corner.y = (((work->scale * (EFFECT_PUFF_CELL_SIZE - 1)) / scratch->depth) * rcos(work->angle)) >> EFFECT_DRAW_FRACTION_BITS;                            \
    quad->x0                 = scratch->screenX + (u16)scratch->extent.corner.x;                                                                                             \
    quad->x3                 = scratch->screenX - (u16)scratch->extent.corner.x;                                                                                             \
    quad->y0                 = scratch->screenY - (u16)scratch->extent.corner.y;                                                                                             \
    quad->y3                 = scratch->screenY + (u16)scratch->extent.corner.y;                                                                                             \
    scratch->extent.corner.x = (((work->scale * (EFFECT_PUFF_CELL_SIZE - 1)) / scratch->depth) * rsin(work->angle + EFFECT_DRAW_QUARTER_TURN)) >> EFFECT_DRAW_FRACTION_BITS; \
    scratch->extent.corner.y = (((work->scale * (EFFECT_PUFF_CELL_SIZE - 1)) / scratch->depth) * rcos(work->angle + EFFECT_DRAW_QUARTER_TURN)) >> EFFECT_DRAW_FRACTION_BITS; \
    quad->x1                 = scratch->screenX + (u16)scratch->extent.corner.x;                                                                                             \
    quad->x2                 = scratch->screenX - (u16)scratch->extent.corner.x;                                                                                             \
    quad->y1                 = scratch->screenY - (u16)scratch->extent.corner.y;                                                                                             \
    quad->y2                 = scratch->screenY + (u16)scratch->extent.corner.y;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        effectKillTask(work, task);
    } else {
        actorRenderComposeCoord(coord);
        scratch                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
        scratch->worldPoint.vx = coord->workm.t[0];
        scratch->worldPoint.vy = coord->workm.t[1];
        scratch->worldPoint.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&scratch->worldPoint);
        gte_rtps();
        gte_stsxy(&scratch->screenX);
        gte_stflg(&scratch->projectionFlags);
        if (scratch->projectionFlags >= 0) {
            gte_stszotz(&scratch->depth);
            quad           = gGpuPrimCursor;
            scratch->depth = scratch->depth + 1;
            gGpuPrimCursor = quad + 1;
            setlen(quad, EFFECT_DRAW_TEXTURED_QUAD_PACKET_WORDS);
            setcode(quad, EFFECT_DRAW_TEXTURED_QUAD);
            // Initialize on the first accepted projection, before the first texture frame.
            if (task->state == EFFECT_DRAW_TASK_NEW) {
                requestedSize = (u16)task->spawnArg1.value & EFFECT_DRAW_SIZE_MASK;
                size          = EFFECT_PUFF_DEFAULT_SIZE;
                if (requestedSize != 0) {
                    size = requestedSize;
                }
                randomState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->scale     = size;
                work->angle     = ((u32)randomState >> 16) & EFFECT_DRAW_ANGLE_MASK;
                ticksPerFrame   = ((u16)task->spawnArg1.value & EFFECT_PUFF_PERIOD_MASK) << 16;
                gRandomLcgState = randomState;
                if (ticksPerFrame != 0) {
                    ticksPerFrame = ticksPerFrame >> 28;
                } else {
                    ticksPerFrame = 1;
                }
                work->period   = ticksPerFrame;
                blendBits      = task->spawnArg1.halves.high;
                work->step     = blendBits & 3;
                work->index    = (task->spawnArg1.value >> EFFECT_PUFF_PALETTE_SHIFT) & 1;
                work->move.vx += ((work->angle & 0xF) * rsin(work->angle)) >> EFFECT_DRAW_FRACTION_BITS;
                work->move.vy -= EFFECT_PUFF_INITIAL_LIFT;
                work->move.vz += ((work->angle & 0xF) * rcos(work->angle)) >> EFFECT_DRAW_FRACTION_BITS;
                if (task->spawnArg1.value & EFFECT_PUFF_RANDOM_VELOCITY) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->move.vx   = ((gRandomLcgState >> 16) & 0x1F) - 0x10;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->move.vy   = ((gRandomLcgState >> 16) & 0x1F) - 0x10;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->move.vz   = ((gRandomLcgState >> 16) & 0x1F) - 0x10;
                }
                if (task->spawnArg1.value & EFFECT_PUFF_SCALE_VELOCITY) {
                    gte_lddp(work->scale << EFFECT_PUFF_VELOCITY_SCALE_SHIFT);
                    velocity = &work->move;
                    gte_ldsv(velocity);
                    gte_gpf12();
                    gte_stsv(velocity);
                }
                task->state = EFFECT_DRAW_TASK_ACTIVE;
            }
            quad->code |= EFFECT_DRAW_RAW_TEXTURE | EFFECT_DRAW_SEMITRANSPARENT;
            quad->tpage = ((work->step & 3) << 5) | EFFECT_PUFF_TEXTURE_PAGE_BASE;
            quad->clut  = ((work->index * EFFECT_PUFF_CLUT_PALETTE_STRIDE) & EFFECT_PUFF_CLUT_OFFSET_MASK) | EFFECT_PUFF_CLUT_BASE;
            quad->u0    = (work->age / work->period) * EFFECT_PUFF_CELL_SIZE;
            quad->v0    = EFFECT_PUFF_TEXTURE_V;
            quad->u1    = ((work->age / work->period) * EFFECT_PUFF_CELL_SIZE) + EFFECT_PUFF_CELL_SIZE - 1;
            quad->v1    = EFFECT_PUFF_TEXTURE_V;
            quad->u2    = (work->age / work->period) * EFFECT_PUFF_CELL_SIZE;
            quad->v2    = EFFECT_PUFF_TEXTURE_V + EFFECT_PUFF_CELL_SIZE - 1;
            quad->u3    = ((work->age / work->period) * EFFECT_PUFF_CELL_SIZE) + EFFECT_PUFF_CELL_SIZE - 1;
            quad->v3    = EFFECT_PUFF_TEXTURE_V + EFFECT_PUFF_CELL_SIZE - 1;
            EFFECT_HIT_PUFF_SET_CORNERS();
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        // Motion and animation advance only while room effects are running.
        coord->coord.t[0]  += work->move.vx;
        coord->coord.t[1]  += work->move.vy;
        coord->coord.t[2]  += work->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        work->move.vy      += EFFECT_PUFF_GRAVITY;
        work->age++;
        if (work->age > work->period * EFFECT_PUFF_FRAME_COUNT - 1) {
            effectKillTask(work, task);
        }
    }
#undef EFFECT_HIT_PUFF_SET_CORNERS
}

void effectSpriteTask42(Task* task)
{
    enum {
        EFFECT_PUFF_TEXTURE_PAGE_BASE     = getTPage(0, GPU_BLEND_AVERAGE, 576, 0),
        EFFECT_PUFF_TEXTURE_U_WRAP_OFFSET = 128,
        EFFECT_PUFF_VELOCITY_SCALE_SHIFT  = 2,
        EFFECT_PUFF_DEFAULT_SIZE          = 512,
        EFFECT_PUFF_CELL_SIZE             = 16,
        EFFECT_PUFF_TEXTURE_V             = 184,
        EFFECT_PUFF_FRAME_COUNT           = 8,
        EFFECT_PUFF_RANDOM_VELOCITY       = 0x100000,
        EFFECT_PUFF_SCALE_VELOCITY        = 0x01000000,
        EFFECT_PUFF_GRAVITY               = 6,
        EFFECT_PUFF_INITIAL_LIFT          = 24,
        EFFECT_PUFF_PERIOD_MASK           = 0xF000,
        EFFECT_PUFF_CLUT                  = getClut(80, 266),
    };
    EffectWork*         work;
    GfxCoord*           coord;
    EffectShapeScratch* scratch;
    POLY_FT4*           quad;
    s16                 effectControl;
    s32                 requestedSize;
    s32                 size;
    s32                 randomState;
    s32                 ticksPerFrame;
    s32                 blendBits;
    SVECTOR*            velocity;

/// Writes rotated screen corners using the live scratch, quad and work locals.
///
/// Use as a standalone statement in a braced block. Captures those three
/// locals and the enclosing task's cell-size constant, retains repeated field
/// reads and narrows each offset before adding it to the projected origin.
#define EFFECT_TRAIL_PUFF_SET_CORNERS()                                                                                                                                      \
    scratch->extent.corner.x = (((work->scale * (EFFECT_PUFF_CELL_SIZE - 1)) / scratch->depth) * rsin(work->angle)) >> EFFECT_DRAW_FRACTION_BITS;                            \
    scratch->extent.corner.y = (((work->scale * (EFFECT_PUFF_CELL_SIZE - 1)) / scratch->depth) * rcos(work->angle)) >> EFFECT_DRAW_FRACTION_BITS;                            \
    quad->x0                 = scratch->screenX + (u16)scratch->extent.corner.x;                                                                                             \
    quad->x3                 = scratch->screenX - (u16)scratch->extent.corner.x;                                                                                             \
    quad->y0                 = scratch->screenY - (u16)scratch->extent.corner.y;                                                                                             \
    quad->y3                 = scratch->screenY + (u16)scratch->extent.corner.y;                                                                                             \
    scratch->extent.corner.x = (((work->scale * (EFFECT_PUFF_CELL_SIZE - 1)) / scratch->depth) * rsin(work->angle + EFFECT_DRAW_QUARTER_TURN)) >> EFFECT_DRAW_FRACTION_BITS; \
    scratch->extent.corner.y = (((work->scale * (EFFECT_PUFF_CELL_SIZE - 1)) / scratch->depth) * rcos(work->angle + EFFECT_DRAW_QUARTER_TURN)) >> EFFECT_DRAW_FRACTION_BITS; \
    quad->x1                 = scratch->screenX + (u16)scratch->extent.corner.x;                                                                                             \
    quad->x2                 = scratch->screenX - (u16)scratch->extent.corner.x;                                                                                             \
    quad->y1                 = scratch->screenY - (u16)scratch->extent.corner.y;                                                                                             \
    quad->y2                 = scratch->screenY + (u16)scratch->extent.corner.y;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        effectKillTask(work, task);
    } else {
        actorRenderComposeCoord(coord);
        scratch                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
        scratch->worldPoint.vx = coord->workm.t[0];
        scratch->worldPoint.vy = coord->workm.t[1];
        scratch->worldPoint.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&scratch->worldPoint);
        gte_rtps();
        gte_stsxy(&scratch->screenX);
        gte_stflg(&scratch->projectionFlags);
        if (scratch->projectionFlags >= 0) {
            gte_stszotz(&scratch->depth);
            quad           = gGpuPrimCursor;
            scratch->depth = scratch->depth + 1;
            gGpuPrimCursor = quad + 1;
            setlen(quad, EFFECT_DRAW_TEXTURED_QUAD_PACKET_WORDS);
            setcode(quad, EFFECT_DRAW_TEXTURED_QUAD);
            // Initialize on the first accepted projection, before the first texture frame.
            if (task->state == EFFECT_DRAW_TASK_NEW) {
                requestedSize = (u16)task->spawnArg1.value & EFFECT_DRAW_SIZE_MASK;
                size          = EFFECT_PUFF_DEFAULT_SIZE;
                if (requestedSize != 0) {
                    size = requestedSize;
                }
                randomState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->scale     = size;
                work->angle     = ((u32)randomState >> 16) & EFFECT_DRAW_ANGLE_MASK;
                ticksPerFrame   = ((u16)task->spawnArg1.value & EFFECT_PUFF_PERIOD_MASK) << 16;
                gRandomLcgState = randomState;
                if (ticksPerFrame != 0) {
                    ticksPerFrame = ticksPerFrame >> 28;
                } else {
                    ticksPerFrame = 1;
                }
                work->period  = ticksPerFrame;
                blendBits     = task->spawnArg1.halves.high;
                work->step    = blendBits & 3;
                work->move.vy = work->move.vy - EFFECT_PUFF_INITIAL_LIFT;
                if (task->spawnArg1.value & EFFECT_PUFF_RANDOM_VELOCITY) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->move.vx   = ((gRandomLcgState >> 16) & 0x1F) - 0x10;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->move.vy   = ((gRandomLcgState >> 16) & 0x1F) - 0x10;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->move.vz   = ((gRandomLcgState >> 16) & 0x1F) - 0x10;
                }
                if (task->spawnArg1.value & EFFECT_PUFF_SCALE_VELOCITY) {
                    gte_lddp(work->scale << EFFECT_PUFF_VELOCITY_SCALE_SHIFT);
                    velocity = &work->move;
                    gte_ldsv(velocity);
                    gte_gpf12();
                    gte_stsv(velocity);
                }
                task->state = EFFECT_DRAW_TASK_ACTIVE;
            }
            quad->code |= EFFECT_DRAW_RAW_TEXTURE | EFFECT_DRAW_SEMITRANSPARENT;
            quad->tpage = ((work->step & 3) << 5) | EFFECT_PUFF_TEXTURE_PAGE_BASE;
            quad->clut  = EFFECT_PUFF_CLUT;
            quad->u0    = (work->age / work->period) * EFFECT_PUFF_CELL_SIZE - EFFECT_PUFF_TEXTURE_U_WRAP_OFFSET;
            quad->v0    = EFFECT_PUFF_TEXTURE_V;
            quad->u1    = (work->age / work->period) * EFFECT_PUFF_CELL_SIZE - EFFECT_PUFF_TEXTURE_U_WRAP_OFFSET + EFFECT_PUFF_CELL_SIZE - 1;
            quad->v1    = EFFECT_PUFF_TEXTURE_V;
            quad->u2    = (work->age / work->period) * EFFECT_PUFF_CELL_SIZE - EFFECT_PUFF_TEXTURE_U_WRAP_OFFSET;
            quad->v2    = EFFECT_PUFF_TEXTURE_V + EFFECT_PUFF_CELL_SIZE - 1;
            quad->u3    = (work->age / work->period) * EFFECT_PUFF_CELL_SIZE - EFFECT_PUFF_TEXTURE_U_WRAP_OFFSET + EFFECT_PUFF_CELL_SIZE - 1;
            quad->v3    = EFFECT_PUFF_TEXTURE_V + EFFECT_PUFF_CELL_SIZE - 1;
            EFFECT_TRAIL_PUFF_SET_CORNERS();
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        // Motion and animation advance only while room effects are running.
        coord->coord.t[0]  += work->move.vx;
        coord->coord.t[1]  += work->move.vy;
        coord->coord.t[2]  += work->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        work->move.vy      += EFFECT_PUFF_GRAVITY;
        work->age++;
        if (work->age > work->period * EFFECT_PUFF_FRAME_COUNT - 1) {
            effectKillTask(work, task);
        }
    }
#undef EFFECT_TRAIL_PUFF_SET_CORNERS
}

/// Initializes a hit-puff emitter's parent-local placement and emission budget.
///
/// Requires the task's zeroed, counted work and writable coordinate, with a
/// parent that stays live until emitter teardown. The signed low spawn half is
/// cube width in game-coordinate units (1..32767 for emission); the signed high
/// half is duration in three-tick units (1..10922 without s16 overflow).
/// Stores lifetime in `period` and width / 768 + 1 attempts per tick in `step`.
static inline void _effectInitHitPuffEmitter(Task* task, EffectWork* work, GfxCoord* coord)
{
    enum {
        EFFECT_HIT_PUFF_TICKS_PER_DURATION_UNIT = 3,
        EFFECT_HIT_PUFF_WIDTH_PER_PARTICLE      = 768,
        EFFECT_HIT_PUFF_DURATION_SHIFT          = 16,
    };
    s32 durationUnits;

    coord->parent = work->parent;
    gfxSetRotIdentity(&coord->coord);
    coord->coord.t[0]   = work->pos.vx;
    coord->coord.t[1]   = work->pos.vy;
    coord->coord.t[2]   = work->pos.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    task->state         = EFFECT_DRAW_TASK_ACTIVE;
    work->scale         = task->spawnArg1.value;
    durationUnits       = task->spawnArg1.value >> EFFECT_HIT_PUFF_DURATION_SHIFT;
    work->angle         = durationUnits;
    work->period        = work->angle * EFFECT_HIT_PUFF_TICKS_PER_DURATION_UNIT;
    work->step          = work->scale / EFFECT_HIT_PUFF_WIDTH_PER_PARTICLE + 1;
}

void effectHitPuffEmitterTask(Task* task)
{
    enum {
        // Size 512, additive blending; choose one or two ticks per texture cell.
        EFFECT_HIT_PUFF_CHILD_ARGUMENT   = 0x11200,
        EFFECT_HIT_PUFF_CHILD_PERIOD_BIT = 0x1000,
    };
    EffectWork* work;
    GfxCoord*   coord;
    s16         effectControl;
    s16         emissionWidth;
    s32         halfWidth;
    s32         particleIndex;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        effectKillTask(work, task);
        return;
    }
    if (task->state == EFFECT_DRAW_TASK_NEW) {
        _effectInitHitPuffEmitter(task, work, coord);
    }
    actorRenderComposeCoord(coord);
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        return;
    }
    if (work->age >= work->period) {
        effectKillTask(work, task);
        return;
    }
    // Children snapshot independent placements within the emitter's local cube.
    emissionWidth = work->scale;
    halfWidth     = emissionWidth >> 1;
    for (particleIndex = 0; particleIndex < work->step; particleIndex++) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->move.vx   = (s32)(gRandomLcgState >> 16) % emissionWidth - halfWidth;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->move.vy   = (s32)(gRandomLcgState >> 16) % emissionWidth - halfWidth;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->move.vz   = (s32)(gRandomLcgState >> 16) % emissionWidth - halfWidth;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        effectSpawn(EFFECT_HIT_PUFF, coord, ((gRandomLcgState >> 16) & EFFECT_HIT_PUFF_CHILD_PERIOD_BIT) + EFFECT_HIT_PUFF_CHILD_ARGUMENT, &work->move);
    }
    work->age++;
}

void effectHitSplatterSprayTask(Task* task)
{
    enum {
        EFFECT_SPLATTER_SPEED_NUMERATOR = 3,
        EFFECT_SPLATTER_SPEED_SHIFT     = 4,
        EFFECT_SPLATTER_DURATION_SHIFT  = 2,
        EFFECT_SPLATTER_DECAY_BIAS      = 5,
        // Additive puff: size 512, two ticks per animation cell.
        EFFECT_SPLATTER_PUFF_ARGUMENT = 0x12200,
    };
    EffectWork* work;
    GfxCoord*   coord;
    s16         effectControl;
    EffectWork* puffWork;
    s32         durationUnits;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        effectKillTask(work, task);
        return;
    }
    // The copied placement offset also supplies the emission direction.
    if (task->state == EFFECT_DRAW_TASK_NEW) {
        coord->parent       = work->parent;
        coord->coord.t[0]   = work->pos.vx;
        coord->coord.t[1]   = work->pos.vy;
        coord->coord.t[2]   = work->pos.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        task->state         = EFFECT_DRAW_TASK_ACTIVE;
        work->scale         = ((u16)task->spawnArg1.value * (u32)EFFECT_SPLATTER_SPEED_NUMERATOR) >> EFFECT_SPLATTER_SPEED_SHIFT;
        durationUnits       = task->spawnArg1.halves.high;
        work->angle         = durationUnits;
        work->period        = durationUnits << EFFECT_SPLATTER_DURATION_SHIFT;
        if ((work->pos.vx | work->pos.vy | work->pos.vz) == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->pos.vx    = ((gRandomLcgState >> 16) & EFFECT_DRAW_ANGLE_MASK) - EFFECT_DRAW_FULL_TURN / 2;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->pos.vy    = ((gRandomLcgState >> 16) & EFFECT_DRAW_ANGLE_MASK) - EFFECT_DRAW_FULL_TURN / 2;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->pos.vz    = ((gRandomLcgState >> 16) & EFFECT_DRAW_ANGLE_MASK) - EFFECT_DRAW_FULL_TURN / 2;
        }
        VectorNormalSS(&work->pos, &work->move);
    }
    actorRenderComposeCoord(coord);
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        return;
    }
    if (work->age >= work->period) {
        effectKillTask(work, task);
        return;
    }
    // Seed each independent puff with a Q12 direction scaled by the declining speed.
    puffWork = effectSpawn(EFFECT_HIT_PUFF, coord, EFFECT_SPLATTER_PUFF_ARGUMENT, NULL);
    if (puffWork != NULL) {
        gte_lddp(work->scale - work->age * (work->angle + EFFECT_SPLATTER_DECAY_BIAS));
        gte_ldsv(&work->move);
        gte_gpf12();
        gte_stsv(&puffWork->move);
    }
    work->age++;
}

void effectGravityParticleTask(Task* task)
{
    enum {
        EFFECT_GRAVITY_PARTICLE_SETTLED_RAW           = 2,
        EFFECT_GRAVITY_PARTICLE_SETTLED_SUBTRACT      = 3,
        EFFECT_GRAVITY_PARTICLE_SETTLED_TINTED        = 4,
        EFFECT_GRAVITY_PARTICLE_SUBTRACT_VARIANT      = 1,
        EFFECT_GRAVITY_PARTICLE_TINTED_VARIANT        = 2,
        EFFECT_GRAVITY_PARTICLE_VARIANT_MASK          = 3,
        EFFECT_GRAVITY_PARTICLE_FRAME_COUNT           = 8,
        EFFECT_GRAVITY_PARTICLE_INITIAL_SPEED         = 256,
        EFFECT_GRAVITY_PARTICLE_SPIN_BIAS             = 512,
        EFFECT_GRAVITY_PARTICLE_SPIN_MASK             = 1023,
        EFFECT_GRAVITY_PARTICLE_RANDOM_COMPONENT_MASK = 255,
        EFFECT_GRAVITY_PARTICLE_RANDOM_COMPONENT_BIAS = 128,
        EFFECT_GRAVITY_PARTICLE_FLIGHT_TICKS          = 81,
        EFFECT_GRAVITY_PARTICLE_FADE_START_TICK       = 51,
        EFFECT_GRAVITY_PARTICLE_SETTLE_INTERVAL       = 8,
        EFFECT_GRAVITY_PARTICLE_SETTLE_SPEED          = 32,
        EFFECT_GRAVITY_PARTICLE_BRIGHTNESS            = 128,
        EFFECT_GRAVITY_PARTICLE_FADE_STEP             = 16,
        EFFECT_GRAVITY_PARTICLE_GRAVITY_NUMERATOR     = 0x10000,
        EFFECT_GRAVITY_PARTICLE_GRID_HIT              = 1,
        // Size is added to these packed period/blend encodings.
        EFFECT_GRAVITY_PARTICLE_SMOKE_ARGUMENT = 0xC0001100,
        EFFECT_GRAVITY_PARTICLE_PUFF_ARGUMENT  = 0x12100,
    };
    EffectWork* work;
    GfxCoord*   coord;
    s16         effectControl;
    s32         variantBits;
    s32         hit;
    s32         childEffectId;
    s32         childArgumentBase;
    SVECTOR     displacement;
    SVECTOR     probeEndOrHit;
    SVECTOR     probeStartOrNormal;
    u8          tintRgb[3];

/// Advances the particle once by its speed-scaled Q12 heading.
///
/// Captures the live work/coord pointers and writable displacement vector.
/// GTE saturation and signed-halfword stores are retained; translation advances
/// in the coordinate's parent frame and is marked dirty. Use as a standalone
/// statement in a braced block; the caller composes or rolls back the step.
#define EFFECT_GRAVITY_PARTICLE_ADVANCE()  \
    gte_lddp(work->scale);                 \
    gte_ldsv(&work->move);                 \
    gte_gpf12();                           \
    gte_stsv(&displacement);               \
    coord->coord.t[0]  += displacement.vx; \
    coord->coord.t[1]  += displacement.vy; \
    coord->coord.t[2]  += displacement.vz; \
    coord->composeStamp = GRAPHICS_COORD_DIRTY;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }
    actorRenderComposeCoord(coord);
    work->age++;
    switch (task->state) {
        case EFFECT_DRAW_TASK_NEW:
            // Reuse pos as size/frame-period/spin; keep the supplied or random Q12 heading in move.
            gfxSetRotIdentity(&coord->coord);
            work->pos.vx    = task->spawnArg1.halves.low & EFFECT_DRAW_SIZE_MASK;
            work->scale     = EFFECT_GRAVITY_PARTICLE_INITIAL_SPEED;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->pos.vy    = (gRandomLcgState >> 16) & (EFFECT_GRAVITY_PARTICLE_FRAME_COUNT - 1);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->index     = (gRandomLcgState >> 16) & (EFFECT_GRAVITY_PARTICLE_FRAME_COUNT - 1);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->pos.vz    = (gRandomLcgState >> 16) & EFFECT_DRAW_ANGLE_MASK;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->period    = EFFECT_GRAVITY_PARTICLE_SPIN_BIAS - ((gRandomLcgState >> 16) & EFFECT_GRAVITY_PARTICLE_SPIN_MASK);
            if ((work->move.vx | work->move.vy | work->move.vz) == 0) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vx   = EFFECT_GRAVITY_PARTICLE_RANDOM_COMPONENT_BIAS - ((gRandomLcgState >> 16) & EFFECT_GRAVITY_PARTICLE_RANDOM_COMPONENT_MASK);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vy   = EFFECT_GRAVITY_PARTICLE_RANDOM_COMPONENT_BIAS - ((gRandomLcgState >> 16) & EFFECT_GRAVITY_PARTICLE_RANDOM_COMPONENT_MASK);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vz   = EFFECT_GRAVITY_PARTICLE_RANDOM_COMPONENT_BIAS - ((gRandomLcgState >> 16) & EFFECT_GRAVITY_PARTICLE_RANDOM_COMPONENT_MASK);
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&work->move);
                gte_rtv0();
                gte_stsv(&work->move);
            }
            VectorNormalSS(&work->move, &work->move);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            variantBits           = task->spawnArg1.halves.high;
            task->state           = EFFECT_DRAW_TASK_ACTIVE;
            task->spawnArg1.value = variantBits & EFFECT_GRAVITY_PARTICLE_VARIANT_MASK;
            return;
        case EFFECT_DRAW_TASK_ACTIVE:
            if (work->age >= EFFECT_GRAVITY_PARTICLE_FLIGHT_TICKS) {
                effectKillTask(work, task);
                return;
            }
            work->pos.vz += work->period;
            if (work->pos.vy != 0 && work->age % work->pos.vy == 0) {
                work->index++;
            }
            EFFECT_GRAVITY_PARTICLE_ADVANCE();
            gte_SetRotMatrix(&gGfxViewCoord.workm);
            gte_ldv0(&displacement);
            gte_rtv0();
            gte_stsv(&probeEndOrHit);
            probeStartOrNormal.vx = coord->workm.t[0];
            probeStartOrNormal.vy = coord->workm.t[1];
            probeStartOrNormal.vz = coord->workm.t[2];
            probeEndOrHit.vx     += probeStartOrNormal.vx;
            probeEndOrHit.vy     += probeStartOrNormal.vy;
            probeEndOrHit.vz     += probeStartOrNormal.vz;
            // The aliased outputs replace the view-space endpoints with a hit point and room-space normal.
            hit = worldCollisionProbeGridSegment(&probeEndOrHit, &probeStartOrNormal, &probeEndOrHit, &probeStartOrNormal);
            if (hit == EFFECT_GRAVITY_PARTICLE_GRID_HIT) {
                coord->coord.t[0] -= displacement.vx;
                coord->coord.t[1] -= displacement.vy;
                coord->coord.t[2] -= displacement.vz;
                work->move.vx      = (probeStartOrNormal.vx >> 1) + (work->move.vx >> 1);
                work->move.vy      = probeStartOrNormal.vy + (work->move.vy >> 1);
                work->move.vz      = (probeStartOrNormal.vz >> 1) + (work->move.vz >> 1);
                VectorNormalSS(&work->move, &work->move);
                work->scale  = work->scale >> 1;
                work->period = work->period >> 1;
                EFFECT_GRAVITY_PARTICLE_ADVANCE();
                actorRenderComposeCoord(coord);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if (((gRandomLcgState >> 16) & 1) != 0) {
                    if (task->spawnArg1.value == EFFECT_GRAVITY_PARTICLE_SUBTRACT_VARIANT) {
                        childArgumentBase = EFFECT_GRAVITY_PARTICLE_SMOKE_ARGUMENT;
                        childEffectId     = EFFECT_SMOKE_PUFF;
                    } else {
                        childArgumentBase = EFFECT_GRAVITY_PARTICLE_PUFF_ARGUMENT;
                        childEffectId     = EFFECT_HIT_PUFF;
                    }
                    effectSpawn(childEffectId, coord, work->pos.vx + childArgumentBase, NULL);
                }
                // Two close, slow impacts settle the particle into its variant-specific fade.
                if (work->age - work->step < EFFECT_GRAVITY_PARTICLE_SETTLE_INTERVAL && work->scale < EFFECT_GRAVITY_PARTICLE_SETTLE_SPEED) {
                    task->state  = task->spawnArg1.value + EFFECT_GRAVITY_PARTICLE_SETTLED_RAW;
                    work->scale  = 0;
                    work->period = 0;
                    work->angle  = EFFECT_GRAVITY_PARTICLE_BRIGHTNESS;
                } else {
                    work->step = work->age;
                }
            } else if (work->scale != 0) {
                actorRenderComposeCoord(coord);
                work->move.vy += EFFECT_GRAVITY_PARTICLE_GRAVITY_NUMERATOR / work->scale;
            }
            if (task->spawnArg1.value == EFFECT_GRAVITY_PARTICLE_TINTED_VARIANT) {
                tintRgb[0] = tintRgb[1] = tintRgb[2] = EFFECT_GRAVITY_PARTICLE_BRIGHTNESS;
                _effectDrawGravityParticle(task, task->spawnArg1.value, tintRgb);
            } else {
                _effectDrawGravityParticle(task, task->spawnArg1.value, NULL);
            }
            return;
        // Raw and tinted variants spread a ground quad and occasionally shed a wisp.
        case EFFECT_GRAVITY_PARTICLE_SETTLED_RAW:
            actorRenderComposeCoord(coord);
            if (work->age >= EFFECT_GRAVITY_PARTICLE_FADE_START_TICK) {
                if (work->angle < EFFECT_GRAVITY_PARTICLE_FADE_STEP) {
                    work->scale++;
                    if (work->scale < EFFECT_GRAVITY_PARTICLE_FRAME_COUNT) {
                        _effectDrawAnimatedGroundQuad(coord, work->period, work->scale, 0);
                    } else {
                        effectKillTask(work, task);
                    }
                } else {
                    u16 wispRoll;

                    tintRgb[0] = tintRgb[1] = tintRgb[2] = work->angle;
                    _effectDrawGravityParticle(task, task->spawnArg1.value, tintRgb);
                    work->period   += work->pos.vx >> 4;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    wispRoll        = (gRandomLcgState >> 16) % 3;
                    if (wispRoll == 0) {
                        effectSpawn(EFFECT_RISING_WISP, coord, (s32)work->pos.vx, NULL);
                    }
                    _effectDrawAnimatedGroundQuad(coord, work->period, 0, 0);
                    work->angle -= EFFECT_GRAVITY_PARTICLE_FADE_STEP;
                }
            } else {
                _effectDrawGravityParticle(task, task->spawnArg1.value, NULL);
            }
            return;
        case EFFECT_GRAVITY_PARTICLE_SETTLED_SUBTRACT:
            actorRenderComposeCoord(coord);
            if (work->age >= EFFECT_GRAVITY_PARTICLE_FADE_START_TICK) {
                if (work->angle < EFFECT_GRAVITY_PARTICLE_FADE_STEP) {
                    effectKillTask(work, task);
                    return;
                }
                tintRgb[0] = tintRgb[1] = tintRgb[2] = work->angle;
                _effectDrawGravityParticle(task, task->spawnArg1.value, tintRgb);
                work->angle -= EFFECT_GRAVITY_PARTICLE_FADE_STEP;
            } else {
                _effectDrawGravityParticle(task, task->spawnArg1.value, NULL);
            }
            return;
        case EFFECT_GRAVITY_PARTICLE_SETTLED_TINTED:
            actorRenderComposeCoord(coord);
            if (work->age >= EFFECT_GRAVITY_PARTICLE_FADE_START_TICK) {
                if (work->angle < EFFECT_GRAVITY_PARTICLE_FADE_STEP) {
                    work->scale++;
                    if (work->scale < EFFECT_GRAVITY_PARTICLE_FRAME_COUNT) {
                        _effectDrawAnimatedGroundQuad(coord, work->period, work->scale, 0);
                    } else {
                        effectKillTask(work, task);
                    }
                } else {
                    u16 wispRoll;

                    tintRgb[0] = tintRgb[1] = tintRgb[2] = work->angle;
                    _effectDrawGravityParticle(task, task->spawnArg1.value, tintRgb);
                    work->period   += work->pos.vx >> 4;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    wispRoll        = (gRandomLcgState >> 16) % 3;
                    if (wispRoll == 0) {
                        effectSpawn(EFFECT_RISING_WISP, coord, (s32)work->pos.vx, NULL);
                    }
                    _effectDrawAnimatedGroundQuad(coord, work->period, 0, 0);
                    work->angle -= EFFECT_GRAVITY_PARTICLE_FADE_STEP;
                }
            } else {
                tintRgb[0] = tintRgb[1] = tintRgb[2] = work->angle;
                _effectDrawGravityParticle(task, task->spawnArg1.value, tintRgb);
            }
            return;
    }
#undef EFFECT_GRAVITY_PARTICLE_ADVANCE
}

static const TaskFuncTable3 Gp_EffTask07States = { {
    _effectControlTask07State0,
    _effectControlTask07State1,
    taskKill,
} };

/// Places the gravity particle's two opposite-corner pairs around its projected centre.
///
/// Requires a live writable quad and aligned scratch block with projected
/// centre and positive depth initialized. Signed halfword size scales the
/// 23-texel centre-to-corner span before depth division; spin angle uses 4096
/// units per turn. Products must fit s32. Reuses the two corner offsets in
/// scratch and writes all four pixel pairs modulo 65536. Retains no pointers.
static inline void _effectPlaceGravityParticleCorners(EffectShapeScratch* scratch, POLY_FT4* quad, s16 size, s16 spinAngle)
{
    enum { EFFECT_GRAVITY_PARTICLE_CORNER_CELL_SIZE = 24 };
    s32 perpendicularAngle;

    // Rotate two opposite-corner pairs in screen space.
    scratch->extent.corner.x = (((size * (EFFECT_GRAVITY_PARTICLE_CORNER_CELL_SIZE - 1)) / scratch->depth) * rsin(spinAngle)) >> EFFECT_DRAW_FRACTION_BITS;
    scratch->extent.corner.y = (((size * (EFFECT_GRAVITY_PARTICLE_CORNER_CELL_SIZE - 1)) / scratch->depth) * rcos(spinAngle)) >> EFFECT_DRAW_FRACTION_BITS;
    quad->x0                 = scratch->screenX + (u16)scratch->extent.corner.x;
    quad->x3                 = scratch->screenX - (u16)scratch->extent.corner.x;
    quad->y0                 = scratch->screenY - (u16)scratch->extent.corner.y;
    quad->y3                 = scratch->screenY + (u16)scratch->extent.corner.y;
    perpendicularAngle       = spinAngle + EFFECT_DRAW_QUARTER_TURN;
    scratch->extent.corner.x = (((size * (EFFECT_GRAVITY_PARTICLE_CORNER_CELL_SIZE - 1)) / scratch->depth) * rsin(perpendicularAngle)) >> EFFECT_DRAW_FRACTION_BITS;
    scratch->extent.corner.y = (((size * (EFFECT_GRAVITY_PARTICLE_CORNER_CELL_SIZE - 1)) / scratch->depth) * rcos(perpendicularAngle)) >> EFFECT_DRAW_FRACTION_BITS;
    quad->x1                 = scratch->screenX + (u16)scratch->extent.corner.x;
    quad->x2                 = scratch->screenX - (u16)scratch->extent.corner.x;
    quad->y1                 = scratch->screenY - (u16)scratch->extent.corner.y;
    quad->y2                 = scratch->screenY + (u16)scratch->extent.corner.y;
}

/// Draws the gravity particle's animated texture as a rotated screen-space quad.
///
/// Requires a live coordinate body, owned EffectWork, composed cached translation,
/// scratch/GTE state and a writable GPU arena. pos.vx supplies signed size,
/// pos.vz signed spin angle (4096 units per turn), and index's low three bits the
/// 24-texel frame. The scaled size is a centre-to-corner distance, not a half-side.
/// tintRgb is NULL or three borrowed readable RGB bytes (128 neutral): variant 1
/// subtracts a tint or draws opaque dark modulation without one; other variants
/// add a tint or draw opaque raw texels. Only variant 1 is distinguished.
/// Projection narrows XYZ to s16, uses SZ3 / 4 plus one for size and ordering,
/// and skips a negative FLAG. Products must fit s32. Retains no pointers.
static void _effectDrawGravityParticle(const Task* task, s32 particleVariant, const u8 tintRgb[3])
{
    enum {
        EFFECT_GRAVITY_PARTICLE_DEPTH_BIAS       = 1,
        EFFECT_GRAVITY_PARTICLE_SUBTRACT_VARIANT = 1,
        EFFECT_GRAVITY_PARTICLE_DARK_BRIGHTNESS  = 32,
        EFFECT_GRAVITY_PARTICLE_FRAME_COUNT      = 8,
        EFFECT_GRAVITY_PARTICLE_CELL_SIZE        = 24,
        EFFECT_GRAVITY_PARTICLE_TEXTURE_V        = 184,
        EFFECT_GRAVITY_PARTICLE_TEXTURE_PAGE     = getTPage(0, GPU_BLEND_AVERAGE, 512, 0),
        EFFECT_GRAVITY_PARTICLE_CLUT             = getClut(224, 266),
        EFFECT_GRAVITY_PARTICLE_BLEND_SHIFT      = 5,
    };
    EffectShapeScratch*         scratch;
    const EffectWork*           work;
    const ModelObjectCoordBody* body;
    const GfxCoord*             coord;
    POLY_FT4*                   quad;
    u16                         blendMode;
    s32                         textureU;
    s32                         textureUEnd;
    s16                         size;
    u16                         frameIndex;
    s16                         spinAngle;

    body                   = task->extra.coordBody;
    work                   = task->spawnArg2.pointer;
    blendMode              = GPU_BLEND_ADD;
    coord                  = body->coord;
    size                   = work->pos.vx;
    frameIndex             = work->index;
    spinAngle              = work->pos.vz;
    scratch                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    scratch->worldPoint.vx = coord->workm.t[0];
    scratch->worldPoint.vy = coord->workm.t[1];
    scratch->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->worldPoint);
    gte_rtps();
    gte_stsxy(&scratch->screenX);
    gte_stflg(&scratch->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&scratch->depth);
        scratch->depth = scratch->depth + EFFECT_GRAVITY_PARTICLE_DEPTH_BIAS;
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setlen(quad, EFFECT_DRAW_TEXTURED_QUAD_PACKET_WORDS);
        setcode(quad, EFFECT_DRAW_TEXTURED_QUAD);
        // Tinted particles blend; an untinted subtract variant remains dark and opaque.
        if (particleVariant == EFFECT_GRAVITY_PARTICLE_SUBTRACT_VARIANT) {
            if (tintRgb != NULL) {
                blendMode = GPU_BLEND_SUBTRACT;
                setRGB0(quad, tintRgb[0], tintRgb[1], tintRgb[2]);
                setSemiTrans(quad, 1);
            } else {
                setRGB0(quad, EFFECT_GRAVITY_PARTICLE_DARK_BRIGHTNESS, EFFECT_GRAVITY_PARTICLE_DARK_BRIGHTNESS, EFFECT_GRAVITY_PARTICLE_DARK_BRIGHTNESS);
            }
        } else if (tintRgb != NULL) {
            setRGB0(quad, tintRgb[0], tintRgb[1], tintRgb[2]);
            setSemiTrans(quad, 1);
        } else {
            setcode(quad, EFFECT_DRAW_TEXTURED_QUAD | EFFECT_DRAW_RAW_TEXTURE);
        }
        quad->tpage = (blendMode << EFFECT_GRAVITY_PARTICLE_BLEND_SHIFT) | EFFECT_GRAVITY_PARTICLE_TEXTURE_PAGE;
        quad->clut  = EFFECT_GRAVITY_PARTICLE_CLUT;
        textureU    = (frameIndex & (EFFECT_GRAVITY_PARTICLE_FRAME_COUNT - 1)) * EFFECT_GRAVITY_PARTICLE_CELL_SIZE;
        textureUEnd = textureU + EFFECT_GRAVITY_PARTICLE_CELL_SIZE - 1;
        setUV4(quad, textureU, EFFECT_GRAVITY_PARTICLE_TEXTURE_V, textureUEnd, EFFECT_GRAVITY_PARTICLE_TEXTURE_V, textureU, EFFECT_GRAVITY_PARTICLE_TEXTURE_V + EFFECT_GRAVITY_PARTICLE_CELL_SIZE - 1, textureUEnd, EFFECT_GRAVITY_PARTICLE_TEXTURE_V + EFFECT_GRAVITY_PARTICLE_CELL_SIZE - 1);
        _effectPlaceGravityParticleCorners(scratch, quad, size, spinAngle);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

/// Projects the animated ground quad's first world-space corner for early rejection.
///
/// Requires a live, word-aligned scratch block with vertex 0 initialized and
/// current world-to-screen matrices and GTE projection settings. Writes screen
/// corner 0 and FLAG; a negative FLAG rejects the quad before its other corners.
/// Changes the GTE matrices and projection registers; does not write depth.
static inline void _effectProjectAnimatedGroundQuadFirstCorner(EffectQuadScratch* quadScratch)
{
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&quadScratch->vertices[0]);
    gte_rtps();
    gte_stsxy(&quadScratch->screenCorners[0]);
    gte_stflg(&quadScratch->projectionFlags);
}

/// Projects the animated ground quad's remaining three world-space corners together.
///
/// Requires initialized vertices 1..3 in the same live scratch block after a
/// successful first-corner projection, with its GTE settings still installed.
/// Writes screen corners 1..3 and replaces FLAG; a negative FLAG rejects this
/// stage. Leaves corner 3's SZ3 in the GTE for depth, without storing it here.
static inline void _effectProjectAnimatedGroundQuadRemainingCorners(EffectQuadScratch* quadScratch)
{
    gte_ldv3(&quadScratch->vertices[1], &quadScratch->vertices[2], &quadScratch->vertices[3]);
    gte_rtpt();
    gte_stsxy3(&quadScratch->screenCorners[1], &quadScratch->screenCorners[2], &quadScratch->screenCorners[3]);
    gte_stflg(&quadScratch->projectionFlags);
}

/// Draws a raw additive eight-frame textured square in a composed coordinate's local XZ plane.
///
/// halfSize is a signed half-side in game-coordinate units; the sign products
/// must fit s32. Rotation and translation narrow each corner to s16. frame's
/// low three bits select a 16-by-16-texel cell; palette must be 0 or 1, selecting
/// row 2 of the CLUT-X table at VRAM Y=266. Borrows the coordinate for this call.
/// Requires initialized scratch/GTE state and a writable GPU primitive arena.
/// Both projection stages reject negative FLAG; depth is corner 3's SZ3 / 4 + 1.
static void _effectDrawAnimatedGroundQuad(const GfxCoord* coord, s32 halfSize, u16 frame, u16 palette)
{
    enum {
        EFFECT_ANIMATED_GROUND_FRAME_MASK = 7,
        EFFECT_ANIMATED_GROUND_CELL_SIZE  = 16,
        EFFECT_ANIMATED_GROUND_U_ORIGIN   = -128,
        EFFECT_ANIMATED_GROUND_V_ORIGIN   = 184,
        EFFECT_ANIMATED_GROUND_UV_SPAN    = EFFECT_ANIMATED_GROUND_CELL_SIZE - 1,
        EFFECT_ANIMATED_GROUND_CLUT_ROW   = 2,
        EFFECT_ANIMATED_GROUND_CLUT_Y     = 266,
        EFFECT_ANIMATED_GROUND_DEPTH_BIAS = 1,
    };
    EffectQuadScratch* quadScratch;
    s32                cornerIndex;
    POLY_FT4*          quad;
    s32                textureU0;
    s32                textureU1;

    quadScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectQuadScratch);
    // Transform the local square into narrowed world coordinates.
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(D_80111E38); cornerIndex++) {
        quadScratch->vertices[cornerIndex].vx = (u16)D_80111E38[cornerIndex].axis0Sign * halfSize;
        quadScratch->vertices[cornerIndex].vy = 0;
        quadScratch->vertices[cornerIndex].vz = (u16)D_80111E38[cornerIndex].axis1Sign * halfSize;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&quadScratch->vertices[cornerIndex]);
        gte_rtv0();
        gte_stsv(&quadScratch->vertices[cornerIndex]);
        quadScratch->vertices[cornerIndex].vx += coord->workm.t[0];
        quadScratch->vertices[cornerIndex].vy += coord->workm.t[1];
        quadScratch->vertices[cornerIndex].vz += coord->workm.t[2];
    }

    // Preserve the early RTPS rejection before projecting the remaining corners.
    _effectProjectAnimatedGroundQuadFirstCorner(quadScratch);
    if (quadScratch->projectionFlags >= 0) {
        _effectProjectAnimatedGroundQuadRemainingCorners(quadScratch);
        if (quadScratch->projectionFlags >= 0) {
            gte_stszotz(&quadScratch->depth);
            quadScratch->depth += EFFECT_ANIMATED_GROUND_DEPTH_BIAS;
            quad                = gGpuPrimCursor;
            gGpuPrimCursor      = quad + 1;
            setlen(quad, EFFECT_DRAW_TEXTURED_QUAD_PACKET_WORDS);
            setcode(quad, EFFECT_DRAW_RAW_ADDITIVE_TEXTURED_QUAD);
            quad->tpage = EFFECT_SPRITE_ATLAS_TEXTURE_PAGE;
            quad->clut  = getClut(D_80112964[EFFECT_ANIMATED_GROUND_CLUT_ROW][palette], EFFECT_ANIMATED_GROUND_CLUT_Y);
            textureU0   = (frame & EFFECT_ANIMATED_GROUND_FRAME_MASK) * EFFECT_ANIMATED_GROUND_CELL_SIZE + EFFECT_ANIMATED_GROUND_U_ORIGIN;
            textureU1   = (frame & EFFECT_ANIMATED_GROUND_FRAME_MASK) * EFFECT_ANIMATED_GROUND_CELL_SIZE + (EFFECT_ANIMATED_GROUND_U_ORIGIN + EFFECT_ANIMATED_GROUND_UV_SPAN);
            setUV4(quad, textureU0, EFFECT_ANIMATED_GROUND_V_ORIGIN, textureU1, EFFECT_ANIMATED_GROUND_V_ORIGIN,
                   textureU0, EFFECT_ANIMATED_GROUND_V_ORIGIN + EFFECT_ANIMATED_GROUND_UV_SPAN,
                   textureU1, EFFECT_ANIMATED_GROUND_V_ORIGIN + EFFECT_ANIMATED_GROUND_UV_SPAN);
            quad->x0 = quadScratch->screenCorners[0].vx;
            quad->y0 = quadScratch->screenCorners[0].vy;
            quad->x1 = quadScratch->screenCorners[1].vx;
            quad->y1 = quadScratch->screenCorners[1].vy;
            quad->x2 = quadScratch->screenCorners[2].vx;
            quad->y2 = quadScratch->screenCorners[2].vy;
            quad->x3 = quadScratch->screenCorners[3].vx;
            quad->y3 = quadScratch->screenCorners[3].vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)quadScratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadScratch);
}

/// Dispatches the current Parasite Energy charge or release effect, control-07 state 1.
///
/// The callback task is unused. A missing player, PE cancellation or cancelled
/// attachment emits nothing. Outside engaged combat only Healing is accepted.
/// Charge passes signed cast-duration frames at the player root; release uses
/// the selected model part and argument zero. Zero table entries emit nothing;
/// held and idle phases also do nothing. Each call may spawn again; no phase is
/// consumed. Armed abilities 0..17 with effective levels 1..3 map to indices
/// 0..53; no range check runs. Nonzero release entries select model parts 0,
/// 1 or 12, which must exist. Requires initialized attachment/effect state.
static void _effectControlTask07State1(Task* unusedTask)
{
    Task* playerTask;
    s32   effectPhase;
    s32   effectId;
    s32   effectIndex;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (playerTask == NULL) {
        return;
    }
    if (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        return;
    }
    effectPhase = Gp_StateC08.effectPhase;
    if (effectPhase == ATTACHMENT_EFFECT_CANCELLED) {
        return;
    }
    if ((gRoomEffectState->battleState != ROOM_EFFECT_BATTLE_ENGAGED) && ((Gp_StateC08.attachId / 10U) != ATTACHMENT_ID_HEALING_FAMILY)) {
        return;
    }
    // Charge and release share the decimal family/level index, with different placement.
    if (effectPhase == ATTACHMENT_EFFECT_CHARGE) {
        effectId = D_80112A50[((u16)(Gp_StateC08.attachId / 100U) - 1) * 9 +
                              ((u16)((u16)(Gp_StateC08.attachId / 10U) % 10U) - 1) * 3 + effectPhase +
                              (u16)(Gp_StateC08.attachId % 10U)];
        if (effectId == 0) {
            return;
        }
        effectSpawn(effectId, playerTask->extra.tmd->coords,
                    (s32)(Gp_StateC08.duration), NULL);
    } else if (effectPhase == ATTACHMENT_EFFECT_RELEASED) {
        effectIndex = ((u16)(Gp_StateC08.attachId / 100U) - 1) * 9 +
                      ((u16)((u16)(Gp_StateC08.attachId / 10U) % 10U) - 1) * 3 - 1;
        effectIndex += (u16)(Gp_StateC08.attachId % 10U);
        effectId     = D_80112978[effectIndex];
        if (effectId == 0) {
            return;
        }
        effectSpawn(effectId,
                    &playerTask->extra.tmd->coords[D_80112B28[effectIndex]], 0,
                    NULL);
    }
}

void effectPeChargeTask(Task* task)
{
    enum {
        EFFECT_PE_CHARGE_PARTICLE_LIMIT     = 32,
        EFFECT_PE_CHARGE_MIN_EMISSION_TICKS = 9,
        EFFECT_PE_CHARGE_GROUP_STRIDE       = 9,
    };
    EffectWork* work;
    GfxCoord*   coord;
    s32         audioPan;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    // This callback leaves state unchanged, so new tasks refresh the sound each tick.
    if (task->state == EFFECT_DRAW_TASK_NEW) {
        task->spawnArg1.value = D_80112B94[((u16)(Gp_StateC08.attachId / 100U) - 1) * EFFECT_PE_CHARGE_GROUP_STRIDE +
                                           ((u16)((u16)(Gp_StateC08.attachId / 10U) % 10U) - 1) * ATTACHMENT_AREA_LEVEL_COUNT +
                                           ((u16)(Gp_StateC08.attachId % 10U) - 1U)];
    }
    actorRenderComposeCoord(coord);
    if (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN || Gp_StateC08.duration == 0 ||
        Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_CANCELLED ||
        (gRoomEffectState->battleState != ROOM_EFFECT_BATTLE_ENGAGED && (u16)(Gp_StateC08.attachId / 10U) != ATTACHMENT_ID_HEALING_FAMILY)) {
        if (task->spawnArg1.value != 0) {
            sndEvtRequestScriptStop(task->spawnArg1.value, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        }
        effectKillTask(work, task);
        return;
    }
    if (Gp_StateC08.duration >= EFFECT_PE_CHARGE_MIN_EMISSION_TICKS) {
        if (work->scale < EFFECT_PE_CHARGE_PARTICLE_LIMIT) {
            if (work->scale == 0) {
                audioPan = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(task->spawnArg1.value, audioPan, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            effectSpawn(EFFECT_PE_CHARGE_PARTICLE, coord, 0, NULL);
            work->scale++;
        }
    }
}

/// Places and composes the charge particle on its player-relative orbit.
///
/// `coord` and its acyclic parent chain must be writable and live. `work->move.vx`
/// is the horizontal radius and `work->move.vy` the base height, in parent-space
/// coordinate units. `orbitAngle` uses 4096 units per turn. The display frame
/// plus `work->scale` drives a 96-unit vertical bob at 64 angle units per frame.
/// Replaces the local translation and refreshes the full-chain cache, preserving
/// local rotation and parent. Borrows both inputs for this call; changes GTE state.
static inline void _effectPositionChargeParticle(GfxCoord* coord, const EffectWork* work, s16 orbitAngle)
{
    enum { EFFECT_CHARGE_PARTICLE_BOB_AMPLITUDE = 96 };
    coord->coord.t[0] = (rcos(orbitAngle) * work->move.vx) >> EFFECT_DRAW_FRACTION_BITS;
    coord->coord.t[1] =
        work->move.vy +
        ((rsin((gDisplayState.animFrame + work->scale) << 6) * EFFECT_CHARGE_PARTICLE_BOB_AMPLITUDE) >> EFFECT_DRAW_FRACTION_BITS);
    coord->coord.t[2]   = (rsin(orbitAngle) * work->move.vx) >> EFFECT_DRAW_FRACTION_BITS;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
}

void effectSpriteTask32(Task* task)
{
    enum {
        EFFECT_CHARGE_PARTICLE_SPRITE_SIZE        = 384,
        EFFECT_CHARGE_PARTICLE_INITIAL_BRIGHTNESS = 128,
        EFFECT_CHARGE_PARTICLE_INITIAL_RADIUS     = 1024,
        EFFECT_CHARGE_PARTICLE_ELEVATION_MASK     = 1023,
        EFFECT_CHARGE_PARTICLE_ELEVATION_BIAS     = 512,
        EFFECT_CHARGE_PARTICLE_HEIGHT_OFFSET      = 1024,
        EFFECT_CHARGE_PARTICLE_ANGLE_STEP         = 24,
        EFFECT_CHARGE_PARTICLE_CONTRACT_STEP      = 128,
        EFFECT_CHARGE_PARTICLE_CONTRACT_DURATION  = 8,
        EFFECT_CHARGE_PARTICLE_FALL_STEP          = 64,
        EFFECT_CHARGE_PARTICLE_FADE_STEP          = 10,
        EFFECT_CHARGE_PARTICLE_SPRITE_BANK        = 0x1000,
        EFFECT_CHARGE_PARTICLE_PALETTE_BITS       = 0x1000,
        EFFECT_CHARGE_PARTICLE_STATE_NEW          = 0,
        EFFECT_CHARGE_PARTICLE_STATE_ORBIT        = 1,
        EFFECT_CHARGE_PARTICLE_STATE_CONTRACT     = 2,
        EFFECT_CHARGE_PARTICLE_STATE_FALL         = 3,
        EFFECT_CHARGE_PARTICLE_STATE_RELEASE      = 4,
    };
    EffectWork* work;
    GfxCoord*   coord;
    GfxCoord*   playerCoord;
    s16         orbitAngle;
    s16         elevationAngle;
    s32         entryState;
    s32         randomState;
    u32         nextRandomState;
    u16         nextAge;

    work       = task->spawnArg2.pointer;
    coord      = task->extra.coordBody->coord;
    nextAge    = work->age + 1;
    work->age  = nextAge;
    entryState = task->state;
    // Age and orbit advance independently of the ordinary room-effect pause.
    switch (entryState) {
        case EFFECT_CHARGE_PARTICLE_STATE_NEW:
            work->angle     = EFFECT_CHARGE_PARTICLE_SPRITE_SIZE;
            work->period    = EFFECT_CHARGE_PARTICLE_INITIAL_BRIGHTNESS;
            work->step      = EFFECT_CHARGE_PARTICLE_INITIAL_RADIUS;
            randomState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            nextRandomState = randomState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            elevationAngle  = ((nextRandomState >> 0x10) & EFFECT_CHARGE_PARTICLE_ELEVATION_MASK) - EFFECT_CHARGE_PARTICLE_ELEVATION_BIAS;
            gRandomLcgState = randomState;
            work->scale     = ((u32)randomState >> 0x10) & EFFECT_DRAW_ANGLE_MASK;
            gRandomLcgState = nextRandomState;
            work->move.vz   = elevationAngle;
            work->move.vx   = (rcos(elevationAngle) * work->step) >> EFFECT_DRAW_FRACTION_BITS;
            work->move.vy   = ((rsin(work->move.vz) * work->step) >> EFFECT_DRAW_FRACTION_BITS) - EFFECT_CHARGE_PARTICLE_HEIGHT_OFFSET;
            playerCoord =
                (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
            coord->parent = playerCoord;
            gfxSetRotIdentity(&coord->coord);
            orbitAngle = (work->age + work->scale) * EFFECT_CHARGE_PARTICLE_ANGLE_STEP;
            _effectPositionChargeParticle(coord, work, orbitAngle);
            task->state = EFFECT_CHARGE_PARTICLE_STATE_ORBIT;
            if (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
                task->state = EFFECT_CHARGE_PARTICLE_STATE_RELEASE;
                break;
            }
            if (Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_CANCELLED) {
                task->state = EFFECT_CHARGE_PARTICLE_STATE_RELEASE;
                break;
            }
            break;
        case EFFECT_CHARGE_PARTICLE_STATE_ORBIT:
            orbitAngle = ((s16)nextAge + work->scale) * EFFECT_CHARGE_PARTICLE_ANGLE_STEP;
            _effectPositionChargeParticle(coord, work, orbitAngle);
            if ((gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) || (Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_CANCELLED)) {
                task->state = EFFECT_CHARGE_PARTICLE_STATE_FALL;
                break;
            }
            if (Gp_StateC08.duration < EFFECT_CHARGE_PARTICLE_CONTRACT_DURATION) {
                task->state = EFFECT_CHARGE_PARTICLE_STATE_CONTRACT;
            }
            break;
        case EFFECT_CHARGE_PARTICLE_STATE_CONTRACT:
            work->step    = work->step - EFFECT_CHARGE_PARTICLE_CONTRACT_STEP;
            orbitAngle    = ((s16)nextAge + work->scale) * EFFECT_CHARGE_PARTICLE_ANGLE_STEP;
            work->move.vx = (rcos(work->move.vz) * work->step) >> EFFECT_DRAW_FRACTION_BITS;
            work->move.vy = ((rsin(work->move.vz) * work->step) >> EFFECT_DRAW_FRACTION_BITS) - EFFECT_CHARGE_PARTICLE_HEIGHT_OFFSET;
            _effectPositionChargeParticle(coord, work, orbitAngle);
            if (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
                task->state = EFFECT_CHARGE_PARTICLE_STATE_RELEASE;
                break;
            }
            if (work->step < EFFECT_CHARGE_PARTICLE_CONTRACT_STEP) {
                task->state = EFFECT_CHARGE_PARTICLE_STATE_RELEASE;
                break;
            }
            if (Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_CANCELLED) {
                task->state = EFFECT_CHARGE_PARTICLE_STATE_RELEASE;
                break;
            }
            break;
        case EFFECT_CHARGE_PARTICLE_STATE_FALL:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[1]  += EFFECT_CHARGE_PARTICLE_FALL_STEP;
            actorRenderComposeCoord(coord);
            if (work->period >= EFFECT_CHARGE_PARTICLE_FADE_STEP + 1) {
                work->period = work->period - EFFECT_CHARGE_PARTICLE_FADE_STEP;
                break;
            }
            task->state = EFFECT_CHARGE_PARTICLE_STATE_RELEASE;
            break;
    }
    effectDrawModulatedBillboard(coord, work->age, work->angle | EFFECT_CHARGE_PARTICLE_SPRITE_BANK,
                                 work->period | EFFECT_CHARGE_PARTICLE_PALETTE_BITS);
    if (task->state == EFFECT_CHARGE_PARTICLE_STATE_RELEASE) {
        effectKillTask(work, task);
    }
}

void effectControlTaskAE(Task* task)
{
    enum {
        EFFECT_CHARGE_GLOW_PLAYER_JOINT     = 12,
        EFFECT_CHARGE_GLOW_INITIAL_RADIUS   = 64,
        EFFECT_CHARGE_GLOW_BRIGHTNESS_SCALE = 256,
        EFFECT_CHARGE_GLOW_MAX_BRIGHTNESS   = 255,
        EFFECT_CHARGE_GLOW_RADIUS_STEP      = 8,
        EFFECT_CHARGE_GLOW_MAX_RADIUS       = 512,
        EFFECT_CHARGE_GLOW_BAND_THRESHOLD   = 129,
        EFFECT_CHARGE_GLOW_BAND_WIDTH       = 96,
        EFFECT_CHARGE_GLOW_FADE_STEP        = 16,
        EFFECT_CHARGE_GLOW_SHRINK_STEP      = 48,
        EFFECT_CHARGE_GLOW_STATE_NEW        = 0,
        EFFECT_CHARGE_GLOW_STATE_CHARGE     = 1,
        EFFECT_CHARGE_GLOW_STATE_SHRINK     = 2,
        EFFECT_CHARGE_GLOW_STATE_FADE       = 3,
    };
    EffectWork* work;
    GfxCoord*   coord;
    GfxCoord*   playerCoords;
    s32         entryState;
    s32         audioPan;
    s16         nextBrightness;
    s16         nextRadius;
    s16         nextBandBrightness;
    u8          rgb[3];

// Packs RGB bytes from work->scale; captures rgb and work and reads brightness three times.
#define EFFECT_CHARGE_GLOW_SET_COLOR() \
    rgb[0] = work->scale;              \
    rgb[1] = work->scale >> 1;         \
    rgb[2] = work->scale >> 2;

    work       = task->spawnArg2.pointer;
    coord      = task->extra.coordBody->coord;
    entryState = task->state;
    // The charge sound and concentric glow follow the live attachment cast phase.
    switch (entryState) {
        case EFFECT_CHARGE_GLOW_STATE_NEW:
            playerCoords =
                (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
            coord->parent = playerCoords + EFFECT_CHARGE_GLOW_PLAYER_JOINT;
            gfxSetRotIdentity(&coord->coord);
            coord->coord.t[0]   = 0;
            coord->coord.t[1]   = 0;
            coord->coord.t[2]   = 0;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            task->state = EFFECT_CHARGE_GLOW_STATE_CHARGE;
            work->scale = 0;
            work->angle = EFFECT_CHARGE_GLOW_INITIAL_RADIUS;
            work->step  = EFFECT_CHARGE_GLOW_BRIGHTNESS_SCALE / task->spawnArg1.value;
            if (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN || Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_CANCELLED ||
                gRoomEffectState->battleState != ROOM_EFFECT_BATTLE_ENGAGED) {
                break;
            }
            task->spawnArg1.value = D_80112B94[((u16)(Gp_StateC08.attachId / 100U) - 1) * 9 +
                                               ((u16)((u16)(Gp_StateC08.attachId / 10U) % 10U) - 1) * 3 +
                                               ((u16)(Gp_StateC08.attachId % 10U) - 1U)];
            audioPan              = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(task->spawnArg1.value, audioPan, (s8)worldCoordGetOriginAudioDepth(coord));
            return;
        case EFFECT_CHARGE_GLOW_STATE_CHARGE:
            actorRenderComposeCoord(coord);
            nextBrightness = work->scale + work->step;
            work->scale    = nextBrightness;
            if (nextBrightness >= EFFECT_CHARGE_GLOW_BRIGHTNESS_SCALE) {
                work->scale = EFFECT_CHARGE_GLOW_MAX_BRIGHTNESS;
            }
            nextRadius  = work->angle + EFFECT_CHARGE_GLOW_RADIUS_STEP;
            work->angle = nextRadius;
            if (nextRadius >= EFFECT_CHARGE_GLOW_MAX_RADIUS + 1) {
                work->angle = EFFECT_CHARGE_GLOW_MAX_RADIUS;
            }
            EFFECT_CHARGE_GLOW_SET_COLOR();
            effectDrawGouraudDisc(coord, work->angle, rgb);
            effectDrawGouraudDisc(coord, (s16)(work->angle << 1), rgb);
            if (work->scale >= EFFECT_CHARGE_GLOW_BAND_THRESHOLD) {
                nextBandBrightness = (work->step << 1) + work->period;
                work->period       = nextBandBrightness;
                if (nextBandBrightness >= EFFECT_CHARGE_GLOW_BRIGHTNESS_SCALE) {
                    work->period = EFFECT_CHARGE_GLOW_MAX_BRIGHTNESS;
                }
                rgb[0] = work->period;
                rgb[1] = work->period >> 1;
                rgb[2] = work->period >> 2;
                effectDrawOuterGlowBand(coord, ((u8)Gp_StateC08.duration << 24) >> 17, EFFECT_CHARGE_GLOW_BAND_WIDTH, rgb);
            }
            if (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN || Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_CANCELLED ||
                gRoomEffectState->battleState != ROOM_EFFECT_BATTLE_ENGAGED) {
                sndEvtRequestScriptStop(task->spawnArg1.value, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                task->state = EFFECT_CHARGE_GLOW_STATE_FADE;
                return;
            }
            if (Gp_StateC08.duration != 0) {
                return;
            }
            work->scale = EFFECT_CHARGE_GLOW_MAX_BRIGHTNESS;
            task->state = EFFECT_CHARGE_GLOW_STATE_SHRINK;
            return;
        case EFFECT_CHARGE_GLOW_STATE_SHRINK:
            actorRenderComposeCoord(coord);
            work->age++;
            if (work->angle <= 0) {
                break;
            }
            EFFECT_CHARGE_GLOW_SET_COLOR();
            effectDrawGouraudDisc(coord, work->angle, rgb);
            effectDrawGouraudDisc(coord, (s16)(work->angle << 1), rgb);
            if (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN || Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_CANCELLED ||
                gRoomEffectState->battleState != ROOM_EFFECT_BATTLE_ENGAGED) {
                sndEvtRequestScriptStop(task->spawnArg1.value, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                task->state = EFFECT_CHARGE_GLOW_STATE_FADE;
                return;
            }
            work->scale = work->scale - EFFECT_CHARGE_GLOW_FADE_STEP;
            work->angle = work->angle - EFFECT_CHARGE_GLOW_SHRINK_STEP;
            return;
        case EFFECT_CHARGE_GLOW_STATE_FADE:
            actorRenderComposeCoord(coord);
            if (work->scale < EFFECT_CHARGE_GLOW_FADE_STEP + 1) {
                break;
            }
            EFFECT_CHARGE_GLOW_SET_COLOR();
            effectDrawGouraudDisc(coord, work->angle, rgb);
            effectDrawGouraudDisc(coord, (s16)(work->angle << 1), rgb);
            work->scale = work->scale - EFFECT_CHARGE_GLOW_FADE_STEP;
            return;
        default:
            return;
    }
    effectKillTask(work, task);
#undef EFFECT_CHARGE_GLOW_SET_COLOR
}

void effectPolyTaskC1(Task* task)
{
    enum {
        EFFECT_COLOR_BAND_INITIAL_BRIGHTNESS = 128,
        EFFECT_COLOR_BAND_INITIAL_RADIUS     = 256,
        EFFECT_COLOR_BAND_WIDTH              = 256,
        EFFECT_COLOR_BAND_RADIUS_STEP        = 128,
        EFFECT_COLOR_BAND_FADE_STEP          = 8,
    };
    EffectWork* work;
    GfxCoord*   coord;
    s16         effectControl;
    s32         colorBits;
    u8          rgb[3];
    s32         nextBrightness;
    s32         nextRadius;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->peEffectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }

    if (task->state == EFFECT_DRAW_TASK_NEW) {
        gfxRotMatrixZ(&coord->coord, task->spawnArg1.value & EFFECT_DRAW_ANGLE_MASK, GRAPHICS_ROTATION_COMPOSE);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        work->scale         = EFFECT_COLOR_BAND_INITIAL_BRIGHTNESS;
        work->angle         = EFFECT_COLOR_BAND_INITIAL_RADIUS;
        colorBits           = task->spawnArg1.halves.high;
        work->period        = D_80112C6C[colorBits & 3];
        task->state         = EFFECT_DRAW_TASK_ACTIVE;
    }

    actorRenderComposeCoord(coord);
    // Expand the colored edge while fading its packed RGB-nibble modulation.
    rgb[0] = (work->scale * ((work->period >> 8) & 0xF)) >> 3;
    rgb[1] = (work->scale * ((u8)work->period >> 4)) >> 3;
    rgb[2] = (work->scale * (work->period & 0xF)) >> 3;
    effectDrawInnerGlowBand(coord, work->angle, EFFECT_COLOR_BAND_WIDTH, rgb);

    nextRadius      = (u16)work->angle;
    nextBrightness  = (u16)work->scale;
    nextRadius     += EFFECT_COLOR_BAND_RADIUS_STEP;
    nextBrightness -= EFFECT_COLOR_BAND_FADE_STEP;
    work->scale     = nextBrightness;
    work->angle     = nextRadius;
    if ((s16)nextBrightness < EFFECT_COLOR_BAND_FADE_STEP + 1) {
        effectKillTask(work, task);
    }
}

/// Attaches an Energy Shot aura's local origin to player model part 8.
///
/// coord must be writable and distinct from the player coordinates. playerCoords
/// must provide at least nine live elements; part 8 stays borrowed until the
/// aura is released. Clears translation in that part's frame and marks the aura
/// dirty without composing it. Preserves local rotation and the cached matrix.
static inline void _effectInitEnergyShotAuraCoord(GfxCoord* coord, GfxCoord* playerCoords)
{
    enum { EFFECT_ENERGY_SHOT_PARENT_PART = 8 };
    coord->coord.t[0]   = 0;
    coord->coord.t[1]   = 0;
    coord->coord.t[2]   = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->parent       = playerCoords + EFFECT_ENERGY_SHOT_PARENT_PART;
}

void effectEnergyShotAuraTask(Task* task)
{
    enum {
        EFFECT_ENERGY_SHOT_LEVEL_RADIX          = 10,
        EFFECT_ENERGY_SHOT_BASE_RADIUS          = 384,
        EFFECT_ENERGY_SHOT_RADIUS_PER_LEVEL     = 128,
        EFFECT_ENERGY_SHOT_BASE_SPARK_SIZE      = 1024,
        EFFECT_ENERGY_SHOT_SPARK_SIZE_PER_LEVEL = 256,
        EFFECT_ENERGY_SHOT_BASE_RAY_COUNT       = 6,
        EFFECT_ENERGY_SHOT_BURST_RED_BLUE       = 192,
        EFFECT_ENERGY_SHOT_BURST_GREEN          = 96,
        EFFECT_ENERGY_SHOT_FIRST_SPARK_PART     = 15,
        EFFECT_ENERGY_SHOT_SPARK_PART_STRIDE    = 3,
        EFFECT_ENERGY_SHOT_SPARK_RANDOM_PALETTE = 0x8000,
    };
    EffectWork* work;
    GfxCoord*   coord;
    GfxCoord*   playerCoords;
    Task*       playerTask;
    u8          rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->peEffectControl != ROOM_EFFECT_CONTROL_RUNNING ||
        ((gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }

    work->age++;
    if (task->state == EFFECT_DRAW_TASK_NEW) {
        gRoomEffectState->peFxFlags |= ROOM_EFFECT_PE_ENERGY_SHOT_AURA;
        playerTask                   = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        playerCoords                 = playerTask->extra.tmd->coords;
        _effectInitEnergyShotAuraCoord(coord, playerCoords);
        task->state  = EFFECT_DRAW_TASK_ACTIVE;
        work->index  = (Gp_StateC08.attachId % (u32)EFFECT_ENERGY_SHOT_LEVEL_RADIX) - 1;
        work->angle  = 0x20; // Retained initialization; this controller never reads it.
        work->period = work->index * EFFECT_ENERGY_SHOT_RADIUS_PER_LEVEL + EFFECT_ENERGY_SHOT_BASE_RADIUS;
        work->step   = work->index * EFFECT_ENERGY_SHOT_SPARK_SIZE_PER_LEVEL + EFFECT_ENERGY_SHOT_BASE_SPARK_SIZE;
    }

    actorRenderComposeCoord(coord);
    // A weapon shot consumes the shared burst request; ambient hand sparks continue independently.
    if (gRoomEffectState->burstRequest != 0) {
        rgb[2] = EFFECT_ENERGY_SHOT_BURST_RED_BLUE;
        rgb[0] = EFFECT_ENERGY_SHOT_BURST_RED_BLUE;
        rgb[1] = EFFECT_ENERGY_SHOT_BURST_GREEN;
        _effectDrawRadialTriangles(coord, (s16)(work->period + EFFECT_ENERGY_SHOT_RADIUS_PER_LEVEL), (s16)(work->index + EFFECT_ENERGY_SHOT_BASE_RAY_COUNT), rgb);
        effectDrawGouraudDisc(coord, work->period, rgb);
        effectDrawGouraudDisc(coord, (s16)(work->period << 1), rgb);
        gRoomEffectState->burstRequest = false;
    }

    if (Gp_StateC08.energyShotTicks == 0 || !(gRoomEffectState->peFxFlags & ROOM_EFFECT_PE_ENERGY_SHOT_AURA) ||
        gRoomEffectState->battleState != ROOM_EFFECT_BATTLE_ENGAGED) {
        effectKillTask(work, task);
        return;
    }

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    if ((gRandomLcgState >> 16) & 3) {
        return;
    }
    playerTask      = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    effectSpawn(EFFECT_RISING_ENERGY_SPARK,
                &playerTask->extra.tmd->coords[((gRandomLcgState >> 16) & 1) * EFFECT_ENERGY_SHOT_SPARK_PART_STRIDE + EFFECT_ENERGY_SHOT_FIRST_SPARK_PART],
                work->step | EFFECT_ENERGY_SHOT_SPARK_RANDOM_PALETTE, NULL);
}

/// Fills one Gouraud ray triangle within an angular sector.
///
/// Borrows writable `scratch` with a projected centre and positive depth,
/// a word-aligned packet and three readable RGB bytes. `narrowRadius` is in
/// coordinate units; screen radius is radius * 128 / depth pixels. `sectorAngle`
/// and positive `angleStep` use 4096 units per turn; the state's upper half
/// chooses a ray within that sector, narrowed to s16. Its black outer vertices
/// lie 40 angle units either side. Products must fit s32; screen stores narrow
/// to s16. Sets packet kind, length, colours and corners and updates scratch
/// extent. The caller links the packet and selects blending; no RNG is advanced.
static inline void _effectFillRadialTriangle(EffectCentreScratch* scratch, POLY_G3* triangle, const u8* rgb, s16 narrowRadius, s32 sectorAngle, u32 randomState, s16 angleStep)
{
    enum { EFFECT_RADIAL_TRIANGLE_RADIUS_NUMERATOR = 128,
           EFFECT_RADIAL_TRIANGLE_HALF_ANGLE       = 40 };
    s32 rayAngle;
    setPolyG3(triangle);
    setRGB0(triangle, rgb[0], rgb[1], rgb[2]);
    setRGB1(triangle, 0, 0, 0);
    setRGB2(triangle, 0, 0, 0);
    scratch->screenExtent = (narrowRadius * EFFECT_RADIAL_TRIANGLE_RADIUS_NUMERATOR) / scratch->depth;
    rayAngle              = (s16)(sectorAngle + (s32)((u32)randomState >> 16) % angleStep);
    triangle->x0          = scratch->screenX;
    triangle->y0          = scratch->screenY;
    triangle->x1          = scratch->screenX + ((scratch->screenExtent * rsin(rayAngle - EFFECT_RADIAL_TRIANGLE_HALF_ANGLE)) >> EFFECT_DRAW_FRACTION_BITS);
    triangle->y1          = scratch->screenY + ((scratch->screenExtent * rcos(rayAngle - EFFECT_RADIAL_TRIANGLE_HALF_ANGLE)) >> EFFECT_DRAW_FRACTION_BITS);
    triangle->x2          = scratch->screenX + ((scratch->screenExtent * rsin(rayAngle + EFFECT_RADIAL_TRIANGLE_HALF_ANGLE)) >> EFFECT_DRAW_FRACTION_BITS);
    triangle->y2          = scratch->screenY + ((scratch->screenExtent * rcos(rayAngle + EFFECT_RADIAL_TRIANGLE_HALF_ANGLE)) >> EFFECT_DRAW_FRACTION_BITS);
}

/// Draws randomized additive Gouraud rays around a composed coordinate's origin.
///
/// radius and rayCount narrow to signed 16 bits. Current callers pass positive
/// radii and 4 or 6..8 rays; rayCount's signed low half must be in 1..4096.
/// Each sector chooses a random angle, spanning 40 angle units either side,
/// and joins two black outer vertices to the origin colored by three rgb bytes.
/// The screen radius is radius * 128 / (SZ3 / 4 + 1) pixels. A negative FLAG
/// drops the burst; otherwise it advances the global RNG once per ray.
/// Inputs are borrowed for the call. Queued packets borrow the frame arena.
static void _effectDrawRadialTriangles(const GfxCoord* coord, s32 radius, s32 rayCount, const u8* rgb)
{
    EffectCentreScratch* scratch;
    POLY_G3*             triangle;
    s16                  angleStep;
    s32                  sectorAngle;
    s32                  randomState;
    s16                  narrowRadius;
    s16                  narrowRayCount;

    SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
    scratch                = SCRATCH_STACK_CURSOR(EffectCentreScratch);
    scratch->worldPoint.vx = coord->workm.t[0];
    scratch->worldPoint.vy = coord->workm.t[1];
    scratch->worldPoint.vz = coord->workm.t[2];
    narrowRayCount         = rayCount;
    angleStep              = EFFECT_DRAW_FULL_TURN / narrowRayCount;
    narrowRadius           = radius;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->worldPoint);
    gte_rtps();
    gte_stsxy(&scratch->screenX);
    gte_stflg(&scratch->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&scratch->depth);
        scratch->depth++;
        // Randomize one narrow ray within each equal angular sector.
        for (sectorAngle = 0; sectorAngle < angleStep * narrowRayCount; sectorAngle += angleStep) {
            randomState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = randomState;
            triangle        = gGpuPrimCursor;
            gGpuPrimCursor  = triangle + 1;
            _effectFillRadialTriangle(scratch, triangle, rgb, narrowRadius, sectorAngle, randomState, angleStep);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    triangle);
            gpuSetPrimitiveBlendMode(triangle, GPU_BLEND_ADD, scratch->depth);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

void effectSpriteTaskF4(Task* task)
{
    enum {
        EFFECT_ENERGY_SPARK_BASE_Y_BITS      = 0xFFF0,
        EFFECT_ENERGY_SPARK_Y_SPREAD_MASK    = 0x3F,
        EFFECT_ENERGY_SPARK_PALETTE_ONE_BITS = 0x1000,
        EFFECT_ENERGY_SPARK_FRAME_COUNT      = 8,
        EFFECT_ENERGY_SPARK_FRAME_TICKS      = 4,
        EFFECT_ENERGY_SPARK_RANDOM_PALETTE   = 0x8000,
        EFFECT_ENERGY_SPARK_PALETTE_MASK     = 0xF000,
    };
    EffectWork* work;
    GfxCoord*   coord;
    Task*       playerTask;
    s16         effectControl;
    s32         nextY;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->peEffectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
            return;
        }
        playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        if (playerTask->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) {
            return;
        }
        actorRenderComposeCoord(coord);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        effectDrawSpinningBillboard(coord, work->index, work->angle,
                                    work->scale | ((gRandomLcgState >> 16) & EFFECT_ENERGY_SPARK_PALETTE_ONE_BITS));
        return;
    }

    // Four update ticks advance one animation cell; negative local Y rises.
    work->age++;
    if (task->state == EFFECT_DRAW_TASK_NEW) {
        work->move.vx   = 0;
        work->move.vz   = 0;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->move.vy   = EFFECT_ENERGY_SPARK_BASE_Y_BITS - ((gRandomLcgState >> 16) & EFFECT_ENERGY_SPARK_Y_SPREAD_MASK);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->scale     = (gRandomLcgState >> 16) & EFFECT_DRAW_ANGLE_MASK;
        work->angle     = task->spawnArg1.halves.low & EFFECT_DRAW_SIZE_MASK;
        task->state     = EFFECT_DRAW_TASK_ACTIVE;
        work->period    = task->spawnArg1.halves.low & EFFECT_ENERGY_SPARK_PALETTE_MASK;
    }

    nextY               = coord->coord.t[1] + work->move.vy;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[1]   = nextY;
    actorRenderComposeCoord(coord);
    if ((work->age & (EFFECT_ENERGY_SPARK_FRAME_TICKS - 1)) == 0) {
        work->index++;
    }
    if (work->index < EFFECT_ENERGY_SPARK_FRAME_COUNT) {
        if (work->period & EFFECT_ENERGY_SPARK_RANDOM_PALETTE) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            effectDrawSpinningBillboard(coord, work->index, work->angle,
                                        work->scale | ((gRandomLcgState >> 16) & EFFECT_ENERGY_SPARK_PALETTE_ONE_BITS));
        } else {
            effectDrawSpinningBillboard(coord, work->index, work->angle,
                                        work->scale | work->period);
        }
        return;
    }
    effectKillTask(work, task);
}

/// Attaches an Antibody aura's local origin to player model part 1.
///
/// coord must be writable and distinct from the player coordinates. playerCoords
/// must provide at least two live elements; part 1 stays borrowed until the
/// aura is released. Clears translation in that part's frame and marks the aura
/// dirty without composing it. Preserves local rotation and the cached matrix.
static inline void _effectInitAntibodyAuraCoord(GfxCoord* coord, GfxCoord* playerCoords)
{
    enum { EFFECT_ANTIBODY_PARENT_PART = 1 };
    coord->coord.t[0]   = 0;
    coord->coord.t[1]   = 0;
    coord->coord.t[2]   = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->parent       = playerCoords + EFFECT_ANTIBODY_PARENT_PART;
}

void effectAntibodyAuraTask(Task* task)
{
    enum {
        EFFECT_ANTIBODY_LEVEL_RADIX               = 10,
        EFFECT_ANTIBODY_BASE_BRIGHTNESS           = 32,
        EFFECT_ANTIBODY_FLICKER_SHIFT             = 4,
        EFFECT_ANTIBODY_RADIUS_MULTIPLIER         = 3,
        EFFECT_ANTIBODY_RADIUS_SHIFT              = 7,
        EFFECT_ANTIBODY_HIT_PULSE_LIMIT           = 160,
        EFFECT_ANTIBODY_HIT_BRIGHTNESS            = 192,
        EFFECT_ANTIBODY_SUPPRESSED_HIT_BRIGHTNESS = 128,
        EFFECT_ANTIBODY_HIT_RAY_RADIUS            = 512,
        EFFECT_ANTIBODY_HIT_RAY_COUNT             = 6,
        EFFECT_ANTIBODY_HIT_BAND_ANGLE_LIMIT      = EFFECT_DRAW_FULL_TURN / 3,
        EFFECT_ANTIBODY_HIT_BAND_ANGLE_STEP       = EFFECT_DRAW_FULL_TURN / 6,
        EFFECT_ANTIBODY_FIRST_FLASH_PART          = 3,
        EFFECT_ANTIBODY_FLASH_PART_MASK           = 15,
        EFFECT_ANTIBODY_FADE_STEP                 = 8,
        // Flash size 128 or 512, with the same high-half variant bit.
        EFFECT_ANTIBODY_AMBIENT_FLASH_ARGUMENT = 0x10080,
        EFFECT_ANTIBODY_HIT_FLASH_ARGUMENT     = 0x10200,
    };
    EffectWork* work;
    GfxCoord*   coord;
    GfxCoord*   playerCoords;
    EffectWork* bandWork;
    Task*       playerTask;
    u8          rgb[3];
    u8          brightness;
    s32         previousHp;
    s32         audioPan;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->peEffectControl != ROOM_EFFECT_CONTROL_RUNNING ||
        ((gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (gRoomEffectState->peEffectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        sndEvtRequestScriptStop(SOUND_ANTIBODY_AURA_LOOP, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        effectKillTask(work, task);
        return;
    }

    work->age++;
    if (task->state == EFFECT_DRAW_TASK_NEW) {
        gRoomEffectState->peFxFlags |= ROOM_EFFECT_PE_ANTIBODY_AURA;
        playerTask                   = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        playerCoords                 = playerTask->extra.tmd->coords;
        _effectInitAntibodyAuraCoord(coord, playerCoords);
        task->state  = EFFECT_DRAW_TASK_ACTIVE;
        work->index  = (Gp_StateC08.attachId % (u32)EFFECT_ANTIBODY_LEVEL_RADIX) - 1;
        work->angle  = EFFECT_ANTIBODY_BASE_BRIGHTNESS;
        work->period = ((work->index + 1) * EFFECT_ANTIBODY_RADIUS_MULTIPLIER) << EFFECT_ANTIBODY_RADIUS_SHIFT;
        work->step   = gPlayerStatus.hp;
    }

    actorRenderComposeCoord(coord);
    work->scale = work->angle + ((work->age & 1) << EFFECT_ANTIBODY_FLICKER_SHIFT);
    brightness  = work->scale;
    rgb[1]      = brightness;
    rgb[0]      = brightness;
    rgb[2]      = work->scale >> 1;
    effectDrawGouraudDisc(coord, work->period, rgb);
    effectDrawGouraudDisc(coord, (s16)(work->period << 1), rgb);

    if (Gp_StateC08.antibodyTicks == 0 || !(gRoomEffectState->peFxFlags & ROOM_EFFECT_PE_ANTIBODY_AURA) ||
        gRoomEffectState->battleState != ROOM_EFFECT_BATTLE_ENGAGED) {
        sndEvtRequestScriptStop(SOUND_ANTIBODY_AURA_LOOP, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        effectKillTask(work, task);
        return;
    }
    // Damage raises brightness and parents three response bands to this aura.
    previousHp = work->step;
    if (gPlayerStatus.hp < previousHp) {
        if (!(gPlayerStatus.statusFlags & (PLAYER_STATUS_BERSERKER | PLAYER_STATUS_POISON)) && (work->angle < EFFECT_ANTIBODY_HIT_PULSE_LIMIT)) {
            s32 bandAngle;

            _effectDrawRadialTriangles(coord, EFFECT_ANTIBODY_HIT_RAY_RADIUS, EFFECT_ANTIBODY_HIT_RAY_COUNT, rgb);
            work->angle = EFFECT_ANTIBODY_HIT_BRIGHTNESS;
            for (bandAngle = 0; bandAngle < EFFECT_ANTIBODY_HIT_BAND_ANGLE_LIMIT; bandAngle += EFFECT_ANTIBODY_HIT_BAND_ANGLE_STEP) {
                bandWork = effectSpawn(EFFECT_EXPANDING_COLOR_BAND, coord, bandAngle, NULL);
                if (bandWork != NULL) {
                    taskReparent(task, bandWork->task);
                }
            }
            audioPan = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(SOUND_ANTIBODY_AURA_HIT, audioPan, (s8)worldCoordGetOriginAudioDepth(coord));
        } else if (work->angle < EFFECT_ANTIBODY_SUPPRESSED_HIT_BRIGHTNESS) {
            work->angle = EFFECT_ANTIBODY_SUPPRESSED_HIT_BRIGHTNESS;
        }
    } else {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (((gRandomLcgState >> 16) & 3) == 0) {
            playerTask      = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            effectSpawn(EFFECT_FLASH_BURST,
                        &playerTask->extra.tmd->coords[((gRandomLcgState >> 16) & EFFECT_ANTIBODY_FLASH_PART_MASK) + EFFECT_ANTIBODY_FIRST_FLASH_PART],
                        EFFECT_ANTIBODY_AMBIENT_FLASH_ARGUMENT, NULL);
        }
    }

    // Retain the signed halfword HP snapshot, then relax the pulse toward its base brightness.
    work->step = (u16)gPlayerStatus.hp;
    if (work->angle < EFFECT_ANTIBODY_BASE_BRIGHTNESS + 1) {
        return;
    }
    work->angle     = work->angle - EFFECT_ANTIBODY_FADE_STEP;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    if ((gRandomLcgState >> 16) & 1) {
        return;
    }
    playerTask      = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    effectSpawn(EFFECT_FLASH_BURST,
                &playerTask->extra.tmd->coords[((gRandomLcgState >> 16) & EFFECT_ANTIBODY_FLASH_PART_MASK) + EFFECT_ANTIBODY_FIRST_FLASH_PART],
                EFFECT_ANTIBODY_HIT_FLASH_ARGUMENT, NULL);
}

/// Parents the Berserker shot burst at player part 8 with an identity local transform.
///
/// `coord` must be writable. `playerCoords` must contain at least nine live
/// coordinates and remain live while the burst retains its parent. Zero local
/// translation places the burst at the part's origin; the dirty stamp requests
/// recomposition before its world transform is used.
static inline void _effectInitStatusBurstCoord(GfxCoord* coord, GfxCoord* playerCoords)
{
    enum { EFFECT_STATUS_BURST_PARENT_PART = 8 };

    coord->parent = playerCoords + EFFECT_STATUS_BURST_PARENT_PART;
    gfxSetRotIdentity(&coord->coord);
    coord->coord.t[0]   = 0;
    coord->coord.t[1]   = 0;
    coord->coord.t[2]   = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

void effectControlTask0E(Task* task)
{
    EffectWork* work;
    GfxCoord*   coord;
    GfxCoord*   playerCoords;
    Task*       playerTask;
    s16         effectControl;
    u8          rgb[3];

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }

    work->age++;
    if (task->state == EFFECT_DRAW_TASK_NEW) {
        gRoomEffectState->peFxFlags |= ROOM_EFFECT_PE_STATUS_BURST;
        playerTask                   = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        playerCoords                 = playerTask->extra.tmd->coords;
        _effectInitStatusBurstCoord(coord, playerCoords);
        task->state = EFFECT_DRAW_TASK_ACTIVE;
    }

    actorRenderComposeCoord(coord);
    // Consume one shot's burst request while the status effect is running.
    if (gRoomEffectState->burstRequest != 0) {
        rgb[0] = 0xC0;
        rgb[1] = 0x30;
        rgb[2] = 0x60;
        _effectDrawRadialTriangles(coord, 0x200, 4, rgb);
        effectDrawGouraudDisc(coord, 0x180, rgb);
        effectDrawGouraudDisc(coord, 0x300, rgb);
        gRoomEffectState->burstRequest = false;
    }

    if ((gPlayerStatus.statusFlags & PLAYER_STATUS_BERSERKER) && (gRoomEffectState->peFxFlags & ROOM_EFFECT_PE_STATUS_BURST) &&
        (gRoomEffectState->battleState == ROOM_EFFECT_BATTLE_ENGAGED)) {
        return;
    }
    gRoomEffectState->screenFxFlags &= ~ROOM_EFFECT_SCREEN_BURST_GUARD;
    effectKillTask(work, task);
}

void roomEffectRequestCancelPe(void)
{
    gRoomEffectState->pendingCancelFlags |= ROOM_EFFECT_CANCEL_PE;
}

/// Advances effect controller 07 from its initial state to its active dispatcher.
static void _effectControlTask07State0(Task* task)
{
    task->state = task->state + 1;
}

void effectControlTask07(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = Gp_EffTask07States;
    stateHandlers.funcs[task->state](task);
}

void effectCorpseBurnTask(Task* task)
{
    enum {
        EFFECT_CORPSE_BURN_SINGLE_FLAME_ARGUMENT = 0,
        EFFECT_CORPSE_BURN_NEW                   = 0,
        EFFECT_CORPSE_BURN_EMITTING              = 1,
        EFFECT_CORPSE_BURN_LINGERING             = 2,
        EFFECT_CORPSE_BURN_SOUND                 = SOUND_COMMON(0x0D),
        EFFECT_CORPSE_BURN_FLAMES_PER_BURST      = 3,
        EFFECT_CORPSE_BURN_BURST_WAIT_TICKS      = 9,
        EFFECT_CORPSE_BURN_LINGER_TICKS          = 101,
        EFFECT_CORPSE_BURN_SINGLE_FLAME_TUNING   = 1,
    };
    EffectWork* work;
    GfxCoord*   coord;
    s16         effectControl;
    s32         flameIndex;
    s32         audioPan;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            sndEvtRequestScriptStop(EFFECT_CORPSE_BURN_SOUND | SOUND_SCRIPT_STOP_ALL_INSTANCES, SOUND_SCRIPT_STOP_KEEP_RELEASE);
            gRoomEffectState->rumbleCount = 0;
            effectKillTask(work, task);
        }
        return;
    }

    actorRenderComposeCoord(coord);
    switch (task->state) {
        // Share one loop across burn controllers; cancellation stops every instance.
        case EFFECT_CORPSE_BURN_NEW:
            if (gRoomEffectState->rumbleCount == 0) {
                audioPan = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(EFFECT_CORPSE_BURN_SOUND, audioPan, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            gRoomEffectState->rumbleCount++;
            task->state = EFFECT_CORPSE_BURN_EMITTING;
            /* fallthrough */
        case EFFECT_CORPSE_BURN_EMITTING:
            // scale marks a pending burst wait, angle counts that wait, index counts completed bursts.
            if (task->spawnArg1.value == EFFECT_CORPSE_BURN_SINGLE_FLAME_ARGUMENT) {
                effectSpawn(EFFECT_DEATH_FLAME, coord, EFFECT_CORPSE_BURN_SINGLE_FLAME_TUNING, NULL);
                task->state = EFFECT_CORPSE_BURN_LINGERING;
            } else if (work->scale == 0) {
                for (flameIndex = 0; flameIndex < EFFECT_CORPSE_BURN_FLAMES_PER_BURST; flameIndex++) {
                    effectSpawn(EFFECT_DEATH_FLAME, coord, task->spawnArg1.value, NULL);
                }
                work->scale++;
            } else {
                work->angle++;
                if (work->angle >= EFFECT_CORPSE_BURN_BURST_WAIT_TICKS) {
                    work->scale = 0;
                    work->angle = 0;
                    work->index++;
                    if (work->index >= task->spawnArg1.value) {
                        task->state = EFFECT_CORPSE_BURN_LINGERING;
                    }
                }
            }
            break;
        case EFFECT_CORPSE_BURN_LINGERING:
            work->age++;
            if (work->age >= EFFECT_CORPSE_BURN_LINGER_TICKS) {
                gRoomEffectState->rumbleCount--;
                if (gRoomEffectState->rumbleCount <= 0) {
                    sndEvtRequestScriptStop(EFFECT_CORPSE_BURN_SOUND | SOUND_SCRIPT_STOP_ALL_INSTANCES, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    gRoomEffectState->rumbleCount = 0;
                }
                effectKillTask(work, task);
            }
            break;
    }
}

/// Advances a death flame by its decelerating local velocity.
///
/// Requires distinct live writable coord/work and initialized GTE state.
/// work->move is velocity in local coordinate units per controller tick. Ages
/// congruent to 3 modulo 4 subtract one from its signed-halfword Z component;
/// the caller increments age first and skips this helper once Z reaches zero.
/// Applies the local Q12 matrix to velocity, overwrites work->pos XYZ with
/// signed-halfword displacement in the parent frame, adds it to translation
/// and marks the coordinate dirty. Translation sums must fit s32. Changes GTE
/// rotation state; neither composes the coordinate nor retains a pointer.
static inline void _effectStepDeathFlameCreep(GfxCoord* coord, EffectWork* work)
{
    enum { EFFECT_DEATH_FLAME_CREEP_INTERVAL_MASK = 3 };
    SVECTOR* localVelocity;
    SVECTOR* parentDisplacement;

    localVelocity      = &work->move;
    parentDisplacement = &work->pos;
    work->move.vz     -= (work->age & EFFECT_DEATH_FLAME_CREEP_INTERVAL_MASK) / EFFECT_DEATH_FLAME_CREEP_INTERVAL_MASK;
    gte_SetRotMatrix(&coord->coord);
    gte_ldv0(localVelocity);
    gte_rtv0();
    gte_stsv(parentDisplacement);
    coord->coord.t[0]  += work->pos.vx;
    coord->coord.t[1]  += work->pos.vy;
    coord->coord.t[2]  += work->pos.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

void effectDeathFlameTask(Task* task)
{
    enum {
        EFFECT_DEATH_FLAME_CREEPING               = 1,
        EFFECT_DEATH_FLAME_DYING                  = 2,
        EFFECT_DEATH_FLAME_BRIGHTNESS_MASK        = 15,
        EFFECT_DEATH_FLAME_BRIGHTNESS_BASE        = 8,
        EFFECT_DEATH_FLAME_HEIGHT_TUNING_SHIFT    = 4,
        EFFECT_DEATH_FLAME_HEIGHT_JITTER_MASK     = 127,
        EFFECT_DEATH_FLAME_RADIUS_BASE            = 192,
        EFFECT_DEATH_FLAME_RADIUS_PER_TUNING      = 24,
        EFFECT_DEATH_FLAME_HEADING_MASK           = 0xFF0,
        EFFECT_DEATH_FLAME_CREEP_SEED_MASK        = 31,
        EFFECT_DEATH_FLAME_CREEP_RANGE_PER_TUNING = 3,
        EFFECT_DEATH_FLAME_CREEP_BASE             = 7,
        EFFECT_DEATH_FLAME_DIE_START_TICK         = 129,
        EFFECT_DEATH_FLAME_HEIGHT_STEP            = 2,
        EFFECT_DEATH_FLAME_RADIUS_STEP            = 2,
        EFFECT_DEATH_FLAME_WISP_PERIOD_BITS       = 0x3000,
    };
    EffectWork* work;
    GfxCoord*   coord;
    s16         effectControl;
    s32         flameTuning;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        effectKillTask(work, task);
        return;
    }
    switch (task->state) {
        case EFFECT_DRAW_TASK_NEW:
            // scale seeds heading/flicker; angle is brightness, period top Y, step base radius.
            work->age++;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->scale     = gRandomLcgState >> 16;
            work->angle     = (work->scale & EFFECT_DEATH_FLAME_BRIGHTNESS_MASK) + EFFECT_DEATH_FLAME_BRIGHTNESS_BASE;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            flameTuning     = task->spawnArg1.value;
            work->period    = -(flameTuning << EFFECT_DEATH_FLAME_HEIGHT_TUNING_SHIFT) - ((gRandomLcgState >> 16) & EFFECT_DEATH_FLAME_HEIGHT_JITTER_MASK);
            work->step      = task->spawnArg1.value * EFFECT_DEATH_FLAME_RADIUS_PER_TUNING + EFFECT_DEATH_FLAME_RADIUS_BASE;
            gfxRotMatrixY(&coord->coord, work->scale & EFFECT_DEATH_FLAME_HEADING_MASK, GRAPHICS_ROTATION_REPLACE);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            task->state = EFFECT_DEATH_FLAME_CREEPING;
            work->move.vz =
                (work->scale & EFFECT_DEATH_FLAME_CREEP_SEED_MASK) % (task->spawnArg1.value * EFFECT_DEATH_FLAME_CREEP_RANGE_PER_TUNING) + EFFECT_DEATH_FLAME_CREEP_BASE;
            _effectDrawDeathFlame(task);
            effectSpawn(EFFECT_RISING_WISP, coord, work->step * 3 + EFFECT_DEATH_FLAME_WISP_PERIOD_BITS, NULL);
            return;
        case EFFECT_DEATH_FLAME_CREEPING:
            // Actor pause freezes movement and age while preserving the visible flame.
            if (gSceneCombatState.actorControl != SCENE_COMBAT_ACTORS_PAUSED) {
                actorRenderComposeCoord(coord);
                work->age++;
                if (work->move.vz != 0) {
                    _effectStepDeathFlameCreep(coord, work);
                }
                if (work->age >= EFFECT_DEATH_FLAME_DIE_START_TICK) {
                    task->state = EFFECT_DEATH_FLAME_DYING;
                }
            }
            break;
        case EFFECT_DEATH_FLAME_DYING:
            if (gSceneCombatState.actorControl != SCENE_COMBAT_ACTORS_PAUSED) {
                actorRenderComposeCoord(coord);
                work->age++;
                work->angle  -= work->age & 1;
                work->period += EFFECT_DEATH_FLAME_HEIGHT_STEP;
                work->step   += EFFECT_DEATH_FLAME_RADIUS_STEP;
                if (work->angle <= 0 || work->period >= 0) {
                    effectKillTask(work, task);
                    return;
                }
            }
            break;
        default:
            return;
    }
    _effectDrawDeathFlame(task);
}

/// Draws the death flame as six additive side quads and two top quads.
///
/// Requires the task's live EffectWork and coordinate body with a composed
/// world matrix, current world-to-screen/GTE state and a writable GPU arena.
/// step gives the base radius; period plus a sine of age gives local top Y,
/// with the top ring 64 game units narrower. angle and a 32-frame triangle wave
/// of display frame plus scale give red brightness, green/blue half/quarter red.
/// Local/world vertex stores retain low signed halfwords; colour narrows to u8.
/// Negative projection FLAG skips a quad; accepted depth is SZ3 / 4 plus one.
/// Reserves and releases one scratch block and retains no task/work pointers.
static void _effectDrawDeathFlame(Task* task)
{
    enum {
        EFFECT_DEATH_FLAME_ANGLE_STEP          = EFFECT_DRAW_FULL_TURN / EFFECT_DEATH_FLAME_VERTEX_COUNT,
        EFFECT_DEATH_FLAME_TOP_INSET           = 64,
        EFFECT_DEATH_FLAME_HEIGHT_ANGLE_SHIFT  = 6,
        EFFECT_DEATH_FLAME_HEIGHT_WAVE_SHIFT   = 8,
        EFFECT_DEATH_FLAME_FLICKER_HALF_PERIOD = 16,
        EFFECT_DEATH_FLAME_FLICKER_MASK        = EFFECT_DEATH_FLAME_FLICKER_HALF_PERIOD - 1,
        EFFECT_DEATH_FLAME_DEPTH_BIAS          = 1,
    };
    const EffectWork*         work;
    const GfxCoord*           coord;
    _EffectDeathFlameScratch* scratch;
    POLY_F4*                  quad;
    u16                       heightBits;
    u8                        red;
    u8                        green;
    u8                        blue;
    u16                       radiusBits;
    s16                       baseRadius;
    s16                       topRadius;
    s16                       brightness;
    s32                       animatedHeight;
    s32                       baseBrightness;
    s32                       flickerPhase;
    s32                       vertexIndex;
    s32                       angle;
    s32                       cosine;
    s32                       screenY;

/// Queues the current death-flame quad only when its projection FLAG is accepted.
///
/// Use as a standalone statement inside a braced block. Captures initialized
/// scratch, RGB bytes, writable quad/screenY locals and the depth-bias constant;
/// reads current GTE depth and consumes the GPU cursor, retaining load/store order.
#define EFFECT_DEATH_FLAME_QUEUE_QUAD()                                                                                                                     \
    if (scratch->projectionFlags >= 0) {                                                                                                                    \
        gte_stszotz(&scratch->depth);                                                                                                                       \
        scratch->depth = scratch->depth + EFFECT_DEATH_FLAME_DEPTH_BIAS;                                                                                    \
        quad           = gGpuPrimCursor;                                                                                                                    \
        gGpuPrimCursor = quad + 1;                                                                                                                          \
        setPolyF4(quad);                                                                                                                                    \
        quad->r0 = red;                                                                                                                                     \
        quad->g0 = green;                                                                                                                                   \
        quad->b0 = blue;                                                                                                                                    \
        quad->x0 = (u16)scratch->screenCorners[0].vx;                                                                                                       \
        screenY  = scratch->screenCorners[0].vy;                                                                                                            \
        quad->y0 = screenY;                                                                                                                                 \
        quad->x1 = (u16)scratch->screenCorners[1].vx;                                                                                                       \
        screenY  = scratch->screenCorners[1].vy;                                                                                                            \
        quad->y1 = screenY;                                                                                                                                 \
        quad->x2 = (u16)scratch->screenCorners[2].vx;                                                                                                       \
        screenY  = scratch->screenCorners[2].vy;                                                                                                            \
        quad->y2 = screenY;                                                                                                                                 \
        quad->x3 = (u16)scratch->screenCorners[3].vx;                                                                                                       \
        screenY  = scratch->screenCorners[3].vy;                                                                                                            \
        quad->y3 = screenY;                                                                                                                                 \
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), \
                quad);                                                                                                                                      \
        gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->depth);                                                                                      \
    }

    // Pulse the height and red-dominant brightness from independent phases.
    work           = task->spawnArg2.pointer;
    coord          = task->extra.coordBody->coord;
    animatedHeight = (u16)work->period + (rsin(work->age << EFFECT_DEATH_FLAME_HEIGHT_ANGLE_SHIFT) >> EFFECT_DEATH_FLAME_HEIGHT_WAVE_SHIFT);
    heightBits     = animatedHeight;
    radiusBits     = work->step;
    flickerPhase   = (u8)gDisplayState.animFrame + (u8)work->scale;
    baseBrightness = work->angle;
    brightness     = baseBrightness;
    topRadius      = radiusBits - EFFECT_DEATH_FLAME_TOP_INSET;
    if (flickerPhase & EFFECT_DEATH_FLAME_FLICKER_HALF_PERIOD) {
        brightness += EFFECT_DEATH_FLAME_FLICKER_MASK;
        brightness -= flickerPhase & EFFECT_DEATH_FLAME_FLICKER_MASK;
    } else {
        brightness += flickerPhase & EFFECT_DEATH_FLAME_FLICKER_MASK;
    }
    red        = brightness;
    green      = red >> 1;
    blue       = red >> 2;
    baseRadius = radiusBits;
    scratch    = SCRATCH_STACK_RESERVE_BLOCK(_EffectDeathFlameScratch);
    gte_SetTransMatrix(&GsWSMATRIX);

    // Build alternating base/top vertices, retaining low halfwords after translation.
    vertexIndex = 0;
    do {
        angle                             = vertexIndex * EFFECT_DEATH_FLAME_ANGLE_STEP;
        cosine                            = rcos(angle);
        scratch->vertices[vertexIndex].vy = 0;
        scratch->vertices[vertexIndex].vx = (cosine * baseRadius) >> EFFECT_DRAW_FRACTION_BITS;
        scratch->vertices[vertexIndex].vz = (rsin(angle) * baseRadius) >> EFFECT_DRAW_FRACTION_BITS;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&scratch->vertices[vertexIndex]);
        gte_rtv0();
        gte_stsv(&scratch->vertices[vertexIndex]);
        scratch->vertices[vertexIndex].vx = (u16)scratch->vertices[vertexIndex].vx + (u16)coord->workm.t[0];
        angle                            += EFFECT_DEATH_FLAME_ANGLE_STEP;
        scratch->vertices[vertexIndex].vy = (u16)scratch->vertices[vertexIndex].vy + (u16)coord->workm.t[1];
        scratch->vertices[vertexIndex].vz = (u16)scratch->vertices[vertexIndex].vz + (u16)coord->workm.t[2];
        cosine                            = rcos(angle);
        vertexIndex++;
        scratch->vertices[vertexIndex].vy = heightBits;
        scratch->vertices[vertexIndex].vx = (cosine * topRadius) >> EFFECT_DRAW_FRACTION_BITS;
        scratch->vertices[vertexIndex].vz = (rsin(angle) * topRadius) >> EFFECT_DRAW_FRACTION_BITS;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&scratch->vertices[vertexIndex]);
        gte_rtv0();
        gte_stsv(&scratch->vertices[vertexIndex]);
        scratch->vertices[vertexIndex].vx = (u16)scratch->vertices[vertexIndex].vx + (u16)coord->workm.t[0];
        scratch->vertices[vertexIndex].vy = (u16)scratch->vertices[vertexIndex].vy + (u16)coord->workm.t[1];
        scratch->vertices[vertexIndex].vz = (u16)scratch->vertices[vertexIndex].vz + (u16)coord->workm.t[2];
        vertexIndex++;
    } while (vertexIndex < EFFECT_DEATH_FLAME_VERTEX_COUNT);

    // Project the six side quads, then the two quads that close the top.
    gte_SetRotMatrix(&GsWSMATRIX);
    for (vertexIndex = 0; vertexIndex < EFFECT_DEATH_FLAME_VERTEX_COUNT; vertexIndex += 2) {
        gte_ldv0(&scratch->vertices[vertexIndex]);
        gte_rtps();
        gte_stsxy(&scratch->screenCorners[0]);
        gte_ldv3(&scratch->vertices[vertexIndex + 1], &scratch->vertices[(vertexIndex + 2) % EFFECT_DEATH_FLAME_VERTEX_COUNT], &scratch->vertices[(vertexIndex + 3) % EFFECT_DEATH_FLAME_VERTEX_COUNT]);
        gte_rtpt();
        gte_stsxy3(&scratch->screenCorners[1], &scratch->screenCorners[2], &scratch->screenCorners[3]);
        gte_stflg(&scratch->projectionFlags);
        EFFECT_DEATH_FLAME_QUEUE_QUAD();
    }

    for (vertexIndex = 1; vertexIndex < EFFECT_DEATH_FLAME_VERTEX_COUNT; vertexIndex += 6) {
        gte_ldv0(&scratch->vertices[vertexIndex]);
        gte_rtps();
        gte_stsxy(&scratch->screenCorners[0]);
        gte_ldv3(&scratch->vertices[vertexIndex + 2], &scratch->vertices[(vertexIndex + 6) % EFFECT_DEATH_FLAME_VERTEX_COUNT], &scratch->vertices[vertexIndex + 4]);
        gte_rtpt();
        gte_stsxy3(&scratch->screenCorners[1], &scratch->screenCorners[2], &scratch->screenCorners[3]);
        gte_stflg(&scratch->projectionFlags);
        EFFECT_DEATH_FLAME_QUEUE_QUAD();
    }

    SCRATCH_STACK_RELEASE_BLOCK(_EffectDeathFlameScratch);
#undef EFFECT_DEATH_FLAME_QUEUE_QUAD
}

void effectSpriteTaskA7(Task* task)
{
    enum {
        EFFECT_RISING_WISP_CELL_SIZE    = 32,
        EFFECT_RISING_WISP_FRAME_COUNT  = 8,
        EFFECT_RISING_WISP_BRIGHTNESS   = 96,
        EFFECT_RISING_WISP_CLUT         = getClut(304, 265),
        EFFECT_RISING_WISP_TEXTURE_PAGE = getTPage(0, GPU_BLEND_ADD, 512, 0),
        EFFECT_RISING_WISP_TEXTURE_V    = 24,
    };
    EffectWork*             work;
    GfxCoord*               coord;
    GfxCoord*               parentCoord;
    EffectBillboardScratch* scratch;
    POLY_FT4*               quad;
    s16                     effectControl;
    s32                     randomState;
    s16                     ticksPerFrame;
    s16                     nextVelocityY;

/// Writes rotated screen corners using the live scratch, quad and work locals.
///
/// Use as a standalone statement in a braced block. Captures those three
/// locals and the enclosing task's cell-size constant, retains repeated field
/// reads and narrows each offset before adding it to the projected origin.
#define EFFECT_RISING_WISP_SET_CORNERS()                                                                                                                                          \
    scratch->cornerOffsetX = (((work->angle * (EFFECT_RISING_WISP_CELL_SIZE - 1)) / scratch->depth) * rsin(work->scale)) >> EFFECT_DRAW_FRACTION_BITS;                            \
    scratch->cornerOffsetY = (((work->angle * (EFFECT_RISING_WISP_CELL_SIZE - 1)) / scratch->depth) * rcos(work->scale)) >> EFFECT_DRAW_FRACTION_BITS;                            \
    quad->x0               = scratch->screenX + scratch->cornerOffsetX;                                                                                                           \
    quad->x3               = scratch->screenX - scratch->cornerOffsetX;                                                                                                           \
    quad->y0               = scratch->screenY - scratch->cornerOffsetY;                                                                                                           \
    quad->y3               = scratch->screenY + scratch->cornerOffsetY;                                                                                                           \
    scratch->cornerOffsetX = (((work->angle * (EFFECT_RISING_WISP_CELL_SIZE - 1)) / scratch->depth) * rsin(work->scale + EFFECT_DRAW_QUARTER_TURN)) >> EFFECT_DRAW_FRACTION_BITS; \
    scratch->cornerOffsetY = (((work->angle * (EFFECT_RISING_WISP_CELL_SIZE - 1)) / scratch->depth) * rcos(work->scale + EFFECT_DRAW_QUARTER_TURN)) >> EFFECT_DRAW_FRACTION_BITS; \
    quad->x1               = scratch->screenX + scratch->cornerOffsetX;                                                                                                           \
    quad->x2               = scratch->screenX - scratch->cornerOffsetX;                                                                                                           \
    quad->y1               = scratch->screenY - scratch->cornerOffsetY;                                                                                                           \
    quad->y2               = scratch->screenY + scratch->cornerOffsetY;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        effectKillTask(work, task);
        return;
    }
    // Reparent at the origin; upward acceleration is expressed in that frame.
    if (task->state == EFFECT_DRAW_TASK_NEW) {
        randomState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->scale   = ((u32)randomState >> 16) & EFFECT_DRAW_ANGLE_MASK;
        work->angle   = task->spawnArg1.halves.low & EFFECT_DRAW_SIZE_MASK;
        parentCoord   = work->parent;
        work->move.vy = -(work->scale & 7);
        coord->parent = parentCoord;
        gfxSetRotIdentity(&coord->coord);
        coord->coord.t[2]   = 0;
        coord->coord.t[1]   = 0;
        coord->coord.t[0]   = 0;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        gRandomLcgState     = randomState;
        task->state++;
    }
    actorRenderComposeCoord(coord);
    scratch                = SCRATCH_STACK_RESERVE_BLOCK(EffectBillboardScratch);
    scratch->worldPoint.vx = coord->workm.t[0];
    scratch->worldPoint.vy = coord->workm.t[1];
    scratch->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->worldPoint);
    gte_rtps();
    ticksPerFrame = (((s32)task->spawnArg1.value >> 12) & 3) + 1;
    gte_stsxy(&scratch->screenX);
    gte_stflg(&scratch->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&scratch->depth);
        quad = gGpuPrimCursor;
        scratch->depth++;
        gGpuPrimCursor = quad + 1;
        setlen(quad, EFFECT_DRAW_TEXTURED_QUAD_PACKET_WORDS);
        setcode(quad, EFFECT_DRAW_TEXTURED_QUAD);
        setRGB0(quad, EFFECT_RISING_WISP_BRIGHTNESS, EFFECT_RISING_WISP_BRIGHTNESS, EFFECT_RISING_WISP_BRIGHTNESS);
        quad->tpage = EFFECT_RISING_WISP_TEXTURE_PAGE;
        quad->clut  = EFFECT_RISING_WISP_CLUT;
        quad->code |= EFFECT_DRAW_SEMITRANSPARENT;
        quad->u0    = (work->age / ticksPerFrame) * EFFECT_RISING_WISP_CELL_SIZE;
        quad->v0    = EFFECT_RISING_WISP_TEXTURE_V;
        quad->u1    = ((work->age / ticksPerFrame) * EFFECT_RISING_WISP_CELL_SIZE) + EFFECT_RISING_WISP_CELL_SIZE - 1;
        quad->v1    = EFFECT_RISING_WISP_TEXTURE_V;
        quad->u2    = (work->age / ticksPerFrame) * EFFECT_RISING_WISP_CELL_SIZE;
        quad->v2    = EFFECT_RISING_WISP_TEXTURE_V + EFFECT_RISING_WISP_CELL_SIZE - 1;
        quad->u3    = ((work->age / ticksPerFrame) * EFFECT_RISING_WISP_CELL_SIZE) + EFFECT_RISING_WISP_CELL_SIZE - 1;
        quad->v3    = EFFECT_RISING_WISP_TEXTURE_V + EFFECT_RISING_WISP_CELL_SIZE - 1;
        EFFECT_RISING_WISP_SET_CORNERS();
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectBillboardScratch);
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        return;
    }
    nextVelocityY       = work->move.vy - (work->age & 1);
    work->move.vy       = nextVelocityY;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[1]  += nextVelocityY;
    work->age++;
    if (work->age > ticksPerFrame * EFFECT_RISING_WISP_FRAME_COUNT - 1) {
        effectKillTask(work, task);
    }
#undef EFFECT_RISING_WISP_SET_CORNERS
}

void effectSpawnHit(s32 effectKind, GfxCoord* coord, SVECTOR* localOffset, EffectSpawnArg* spawnRecord)
{
    enum {
        EFFECT_HIT_PUFF_PRIMARY_ARGUMENT        = 0x12380,
        EFFECT_HIT_PUFF_ALTERNATE_ARGUMENT      = 0x12300,
        EFFECT_HIT_PUFF_RANDOM_ARGUMENT         = 0x111300,
        EFFECT_HIT_PUFF_REPEAT_ARGUMENT         = 0x111280,
        EFFECT_HIT_PUFF_TINTED_PRIMARY_ARGUMENT = 0x10013380,
        EFFECT_HIT_PUFF_TINTED_RANDOM_ARGUMENT  = 0x10111300,
        EFFECT_HIT_PUFF_TINTED_REPEAT_ARGUMENT  = 0x10112280,
        EFFECT_HIT_PUFF_SPARK_ARGUMENT          = 0x112300,
        EFFECT_HIT_PUFF_DENSE_ARGUMENT          = 0x1112300,
        EFFECT_HIT_SPARK_SIZE                   = 0x400,
        EFFECT_HIT_ARGUMENT_HIGH_SHIFT          = 16,
        EFFECT_HIT_ARGUMENT_SINGLE              = 0x10000,
    };
    GameActor* actor;
    s32        spawnIndex;
    s32        pan;
    u16        kind;

    kind  = effectKind;
    actor = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    // Resolve call placement and the record's independently retained placement.
    if (spawnRecord == NULL) {
        spawnRecord        = &D_80112C74;
        spawnRecord->coord = coord;
    } else {
        if (spawnRecord->coord == NULL) {
            if (coord == NULL) {
                spawnRecord->coord = &gGfxViewCoord;
                coord              = spawnRecord->coord;
            } else {
                spawnRecord->coord = coord;
            }
        } else if (coord == NULL) {
            coord = spawnRecord->coord;
        }
    }
    switch (kind) {
        case EFFECT_HIT_KIND_WEAPON_PUFF:
            if (actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key & PLAYER_ACTOR_WEAPON_ATTACK_ALTERNATE) {
                effectSpawn(EFFECT_HIT_PUFF, coord, EFFECT_HIT_PUFF_ALTERNATE_ARGUMENT, localOffset);
                if (spawnRecord->spawnArgHi >= 2) {
                    for (spawnIndex = 0; spawnIndex < spawnRecord->spawnArgHi; spawnIndex++) {
                        effectSpawn(EFFECT_HIT_PUFF, coord, EFFECT_HIT_PUFF_REPEAT_ARGUMENT, localOffset);
                    }
                }
            } else {
                effectSpawn(EFFECT_HIT_PUFF, coord, EFFECT_HIT_PUFF_PRIMARY_ARGUMENT, localOffset);
                effectSpawn(EFFECT_HIT_PUFF, coord, EFFECT_HIT_PUFF_RANDOM_ARGUMENT, localOffset);
                effectSpawn(EFFECT_HIT_PUFF, coord, EFFECT_HIT_PUFF_RANDOM_ARGUMENT, localOffset);
                for (spawnIndex = 0; spawnIndex < spawnRecord->spawnArgHi; spawnIndex++) {
                    effectSpawn(EFFECT_HIT_PUFF, coord, EFFECT_HIT_PUFF_REPEAT_ARGUMENT, localOffset);
                }
            }
            break;
        case EFFECT_HIT_KIND_TINTED_PUFF:
            effectSpawn(EFFECT_HIT_PUFF, coord, EFFECT_HIT_PUFF_TINTED_PRIMARY_ARGUMENT, localOffset);
            effectSpawn(EFFECT_HIT_PUFF, coord, EFFECT_HIT_PUFF_TINTED_RANDOM_ARGUMENT, localOffset);
            effectSpawn(EFFECT_HIT_PUFF, coord, EFFECT_HIT_PUFF_TINTED_RANDOM_ARGUMENT, localOffset);
            for (spawnIndex = 0; spawnIndex < spawnRecord->spawnArgHi; spawnIndex++) {
                effectSpawn(EFFECT_HIT_PUFF, coord, EFFECT_HIT_PUFF_TINTED_REPEAT_ARGUMENT, localOffset);
            }
            break;
        case EFFECT_HIT_KIND_PARTICLE_EMITTER:
            effectSpawn(EFFECT_HIT_PARTICLE_EMITTER, spawnRecord->coord, spawnRecord->spawnArgLo | (spawnRecord->spawnArgHi << EFFECT_HIT_ARGUMENT_HIGH_SHIFT), localOffset);
            break;
        case EFFECT_HIT_KIND_SPLATTER:
            effectSpawn(EFFECT_HIT_SPLATTER_SPRAY, coord, spawnRecord->spawnArgLo | (spawnRecord->spawnArgHi << EFFECT_HIT_ARGUMENT_HIGH_SHIFT), localOffset);
            break;
        case EFFECT_HIT_KIND_SPARK_AND_PUFFS:
            effectSpawn(EFFECT_IMPACT_SPARK, coord, EFFECT_HIT_SPARK_SIZE, localOffset);
            for (spawnIndex = 0; spawnIndex < spawnRecord->spawnArgHi; spawnIndex++) {
                effectSpawn(EFFECT_HIT_PUFF, coord, EFFECT_HIT_PUFF_SPARK_ARGUMENT, localOffset);
            }
            break;
        case EFFECT_HIT_KIND_SPARK_BURST:
            effectSpawn(EFFECT_HIT_SPARK_BURST, spawnRecord->coord, spawnRecord->spawnArgLo | (spawnRecord->spawnArgHi << EFFECT_HIT_ARGUMENT_HIGH_SHIFT), localOffset);
            break;
        case EFFECT_HIT_KIND_DENSE_PUFFS:
            for (spawnIndex = 0; spawnIndex < spawnRecord->spawnArgHi * 3; spawnIndex++) {
                effectSpawn(EFFECT_HIT_PUFF, spawnRecord->coord, EFFECT_HIT_PUFF_DENSE_ARGUMENT, localOffset);
            }
            break;
        case EFFECT_HIT_KIND_SPLATTER_ALT:
            effectSpawn(EFFECT_HIT_SPLATTER_SPRAY, coord, spawnRecord->spawnArgLo | (spawnRecord->spawnArgHi << EFFECT_HIT_ARGUMENT_HIGH_SHIFT), localOffset);
            break;
        case EFFECT_HIT_KIND_CONTROL_E3:
            effectSpawn(EFFECT_0E3, coord, spawnRecord->spawnArgLo | EFFECT_HIT_ARGUMENT_SINGLE, localOffset);
            break;
        case EFFECT_HIT_KIND_BLAST_WITH_SOUND:
            effectSpawn(EFFECT_HIT_BLAST, spawnRecord->coord, spawnRecord->spawnArgLo | (spawnRecord->spawnArgHi << EFFECT_HIT_ARGUMENT_HIGH_SHIFT), NULL);
            pan = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(D_80112C7C[(u16)(Gp_StateC08.attachId % 10U) - 1], pan,
                                     (s8)worldCoordGetOriginAudioDepth(coord));
            break;
        case EFFECT_HIT_KIND_APOBIOSIS_SHARD:
            effectSpawn(EFFECT_APOBIOSIS_SHARD, coord, 1, NULL);
            break;
        case EFFECT_HIT_KIND_LIFE_DRAIN_MOTES:
            for (spawnIndex = 0; spawnIndex < spawnRecord->spawnArgHi; spawnIndex++) {
                effectSpawn((EFFECT_LIFE_DRAIN_MOTE | EFFECT_SPAWN_UNLIMITED), coord, 1, NULL);
            }
            break;
        case EFFECT_HIT_KIND_HAMMER_FLASH:
            effectSpawn(EFFECT_HIT_SPARK_BURST, spawnRecord->coord, spawnRecord->spawnArgLo | (spawnRecord->spawnArgHi << EFFECT_HIT_ARGUMENT_HIGH_SHIFT), localOffset);
            effectSpawn(EFFECT_M4A1_HAMMER_IMPACT_FLASH, coord, 1, NULL);
            break;
        case EFFECT_HIT_KIND_WEAPON_BLAST:
            if (actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key & PLAYER_ACTOR_WEAPON_ATTACK_ALTERNATE) {
                effectSpawn(EFFECT_HIT_BLAST, spawnRecord->coord, spawnRecord->spawnArgLo | EFFECT_HIT_ARGUMENT_SINGLE, localOffset);
            } else {
                case EFFECT_HIT_KIND_BLAST:
                    effectSpawn(EFFECT_HIT_BLAST, spawnRecord->coord, spawnRecord->spawnArgLo | (spawnRecord->spawnArgHi << EFFECT_HIT_ARGUMENT_HIGH_SHIFT), localOffset);
            }
            break;
    }
}

/// Four-entry `Task::state` dispatcher: `_playerActorInitWork`, `_playerActorWorkState1`,
/// `_playerActorWorkState2`, `_playerActorTeardown`.
static const TaskFuncTable4 Gp_PlayerWorkStates = { {
    _playerActorInitWork,
    _playerActorWorkState1,
    _playerActorWorkState2,
    _playerActorTeardown,
} };

/// Chooses one hit-blast offset, consuming three successive shared random draws.
///
/// Requires writable work and offsetRange in 1..32767. Before s16 narrowing,
/// X/Z span -offsetHalfRange..offsetRange-1-offsetHalfRange; Y is 0..offsetRange-1,
/// in parent-coordinate units. Advances the shared LCG once per component;
/// each draw uses its unsigned upper halfword before signed remainder.
/// Writes only work->move XYZ, each narrowed to s16; vector metadata stays intact.
static inline void _effectChooseHitBlastOffset(EffectWork* work, s16 offsetRange, s16 offsetHalfRange)
{
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->move.vx   = ((s32)(gRandomLcgState >> 16) % offsetRange) - offsetHalfRange;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->move.vy   = (s32)(gRandomLcgState >> 16) % offsetRange;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->move.vz   = ((s32)(gRandomLcgState >> 16) % offsetRange) - offsetHalfRange;
}

void effectControlTask7F(Task* task)
{
    enum {
        EFFECT_HIT_BLAST_SINGLE_DURATION    = 1,
        EFFECT_HIT_BLAST_UNITS_PER_EMISSION = 1280,
        EFFECT_HIT_BLAST_FLAME_SIZE         = 0x300,
        EFFECT_HIT_BLAST_PUFF_RISE_FLAG     = 0x10000,
        // Quarter-additive smoke: size 512, period 3, view-rise motion 1.
        EFFECT_HIT_BLAST_SMOKE_ARGUMENT_BASE = 0xC0013200,
    };
    EffectWork* work;
    GfxCoord*   coord;
    s16         effectControl;
    s16         flameTicks;
    s32         durationUnits;
    s32         offsetSpan;
    s16         offsetRange;
    s16         offsetHalfRange;
    s32         burstCount;
    s32         burstIndex;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        effectKillTask(work, task);
        return;
    }
    if (task->state == EFFECT_DRAW_TASK_NEW) {
        // Retain the size/duration halves; angle and period count phase ticks here.
        coord->parent = work->parent;
        gfxSetRotIdentity(&coord->coord);
        coord->coord.t[0]   = work->pos.vx;
        coord->coord.t[1]   = work->pos.vy;
        coord->coord.t[2]   = work->pos.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        task->state         = EFFECT_DRAW_TASK_ACTIVE;
        work->scale         = task->spawnArg1.halves.low;
        durationUnits       = task->spawnArg1.halves.high;
        flameTicks          = durationUnits;
        work->index         = durationUnits;
        if (flameTicks != EFFECT_HIT_BLAST_SINGLE_DURATION) {
            flameTicks = flameTicks * 3;
        } else {
            flameTicks = EFFECT_HIT_BLAST_SINGLE_DURATION;
        }
        work->angle  = flameTicks;
        work->period = flameTicks * 2;
        work->step   = work->scale / EFFECT_HIT_BLAST_UNITS_PER_EMISSION;
    }
    actorRenderComposeCoord(coord);
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        return;
    }
    if (work->age >= work->period) {
        effectKillTask(work, task);
        return;
    }
    // Keep both signed-halfword range stores and the logical half-range shift.
    offsetSpan      = work->scale >> 1;
    offsetHalfRange = (u32)offsetSpan >> 1;
    offsetRange     = offsetSpan;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    if ((gRandomLcgState >> 16) & 1) {
        burstCount = work->step;
    } else {
        burstCount = 1;
    }
    for (burstIndex = 0; burstIndex < burstCount; burstIndex++) {
        _effectChooseHitBlastOffset(work, offsetRange, offsetHalfRange);
        if (work->age < work->angle) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (work->age < (s32)(gRandomLcgState >> 16) % work->period) {
                // The signed X promotion sets the rise flag only on the negative side.
                effectSpawn(EFFECT_ADDITIVE_PUFF, coord, (work->move.vx & EFFECT_HIT_BLAST_PUFF_RISE_FLAG) | EFFECT_HIT_BLAST_FLAME_SIZE,
                            &work->move);
            } else {
                effectSpawn(EFFECT_FIRE_BURST, coord, EFFECT_HIT_BLAST_FLAME_SIZE, &work->move);
            }
        } else {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                effectSpawn(EFFECT_SMOKE_PUFF, coord, (work->scale >> 2) + EFFECT_HIT_BLAST_SMOKE_ARGUMENT_BASE,
                            &work->move);
            }
        }
    }
    work->age++;
}

void effectHitSmokeEmitterTask(Task* task)
{
    enum {
        EFFECT_HIT_SMOKE_DURATION_SHIFT = 2,
        // Smoke: size 1024 plus a quarter of the source size, one tick per cell,
        // view-space drift/rise mode 2 and additive blending.
        EFFECT_HIT_SMOKE_ARGUMENT_BASE = 0x80021400,
    };
    EffectWork* work;
    GfxCoord*   coord;
    s16         effectControl;
    s32         durationUnits;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        effectKillTask(work, task);
    } else {
        if (task->state == EFFECT_DRAW_TASK_NEW) {
            coord->parent       = work->parent;
            coord->coord.t[0]   = work->pos.vx;
            coord->coord.t[1]   = work->pos.vy;
            coord->coord.t[2]   = work->pos.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->state         = EFFECT_DRAW_TASK_ACTIVE;
            work->scale         = task->spawnArg1.halves.low;
            durationUnits       = task->spawnArg1.halves.high;
            work->angle         = durationUnits;
            work->period        = durationUnits << EFFECT_HIT_SMOKE_DURATION_SHIFT;
        }
        actorRenderComposeCoord(coord);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        // Age advances before emission, so a positive duration emits one fewer puff than its tick count.
        work->age++;
        if (work->age >= work->period) {
            effectKillTask(work, task);
        } else {
            effectSpawn(EFFECT_SMOKE_PUFF, coord, (work->scale >> 2) + EFFECT_HIT_SMOKE_ARGUMENT_BASE, &work->move);
        }
    }
}

/// Writes an axis-aligned, perspective-sized billboard's screen corners.
///
/// Borrows writable projected scratch and a quad for this call. sizeNumerator
/// is coordinate-space size times the texture cell's side minus one (31 puff,
/// 23 fire burst); that product must fit s32. scratch->depth must be positive.
/// Stores numerator / depth in screenExtent, narrows it to u16 for centre
/// arithmetic, then screen coordinates narrow to s16. Changes only scratch
/// extent and quad X/Y, retaining all packet setup and linking fields.
static inline void _effectSetPerspectiveBillboardCorners(EffectCentreScratch* scratch, POLY_FT4* quad, s32 sizeNumerator)
{
    s16 screenX;
    s16 screenY;
    scratch->screenExtent = sizeNumerator / scratch->depth;
    screenX               = scratch->screenX - (u16)scratch->screenExtent;
    quad->x2              = screenX;
    quad->x0              = screenX;
    screenX               = scratch->screenX + (u16)scratch->screenExtent;
    quad->x3              = screenX;
    quad->x1              = screenX;
    screenY               = scratch->screenY - (u16)scratch->screenExtent;
    quad->y1              = screenY;
    quad->y0              = screenY;
    screenY               = scratch->screenY + (u16)scratch->screenExtent;
    quad->y3              = screenY;
    quad->y2              = screenY;
}

void effectSpriteTask80(Task* task)
{
    enum {
        EFFECT_PUFF_MIN_LAST_AGE        = 12,
        EFFECT_PUFF_LIFETIME_SPAN       = 12,
        EFFECT_PUFF_RISE_FLAG           = 1,
        EFFECT_PUFF_RISE_SPEED_RANGE    = 40,
        EFFECT_PUFF_GROW_TICKS          = 12,
        EFFECT_PUFF_FADE_TICKS          = 8,
        EFFECT_PUFF_CELL_SIZE           = 32,
        EFFECT_PUFF_FRAME_COUNT         = 6,
        EFFECT_PUFF_TEXTURE_V           = 152,
        EFFECT_PUFF_TEXTURE_PAGE        = getTPage(0, GPU_BLEND_ADD, 576, 0),
        EFFECT_PUFF_CLUT                = getClut(32, 266),
        EFFECT_PUFF_DEFAULT_SIZE        = 512,
        EFFECT_PUFF_FADE_STEP           = 16,
        EFFECT_PUFF_RETAINED_SPAWN_FLAG = 0x80000000,
    };
    EffectCentreScratch* scratch;
    GfxCoord*            coord;
    EffectWork*          work;
    POLY_FT4*            quad;
    s32                  size;
    s32                  requestedSize;
    u16                  frameAge;
    s32                  currentSize;
    s32                  brightness;
    u32                  riseSpeed;
    s32                  riseFlags;
    EffectCentreScratch* scratchStart;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        scratchStart                              = SCRATCH_STACK_CURSOR(EffectCentreScratch) - 1;
        SCRATCH_STACK_CURSOR(EffectCentreScratch) = scratchStart;
        scratch                                   = scratchStart;
        // Choose lifetime and optional rise speed before any projection can reject it.
        if (task->state == EFFECT_DRAW_TASK_NEW) {
            requestedSize = (u16)task->spawnArg1.value & EFFECT_DRAW_SIZE_MASK;
            size          = EFFECT_PUFF_DEFAULT_SIZE;
            if (requestedSize != 0) {
                size = requestedSize;
            }
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->scale     = size;
            work->angle     = (gRandomLcgState >> 16) % EFFECT_PUFF_LIFETIME_SPAN + EFFECT_PUFF_MIN_LAST_AGE;
            riseFlags       = task->spawnArg1.halves.high;
            if (riseFlags & EFFECT_PUFF_RISE_FLAG) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                riseSpeed       = gRandomLcgState >> 16;
                riseSpeed       = riseSpeed % EFFECT_PUFF_RISE_SPEED_RANGE;
            } else {
                riseSpeed = 0;
            }
            work->step = riseSpeed;
            task->state++;
            task->spawnArg1.value &= EFFECT_PUFF_RETAINED_SPAWN_FLAG;
        }
        actorRenderComposeCoord(coord);
        scratch->worldPoint.vx = (u16)coord->workm.t[0];
        scratch->worldPoint.vy = (u16)coord->workm.t[1];
        scratch->worldPoint.vz = (u16)coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&scratch->worldPoint);
        gte_rtps();
        gte_stsxy(&scratch->screenX);
        gte_stflg(&scratch->projectionFlags);
        if (scratch->projectionFlags >= 0) {
            gte_stszotz(&scratch->depth);
            scratch->depth++;
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setlen(quad, EFFECT_DRAW_TEXTURED_QUAD_PACKET_WORDS);
            setcode(quad, EFFECT_DRAW_TEXTURED_QUAD);
            if (work->age < EFFECT_PUFF_GROW_TICKS) {
                currentSize = work->scale * work->age / EFFECT_PUFF_GROW_TICKS;
            } else {
                currentSize = (u16)work->scale;
            }
            work->period = currentSize;
            if (work->angle - EFFECT_PUFF_FADE_TICKS < work->age) {
                brightness = (work->angle - work->age + 1) * EFFECT_PUFF_FADE_STEP;
                setRGB0(quad, brightness, brightness, brightness);
            } else {
                quad->code |= EFFECT_DRAW_RAW_TEXTURE;
            }
            quad->tpage = EFFECT_PUFF_TEXTURE_PAGE;
            quad->clut  = EFFECT_PUFF_CLUT;
            quad->code |= EFFECT_DRAW_SEMITRANSPARENT;
            frameAge    = work->age;
            quad->v0    = EFFECT_PUFF_TEXTURE_V;
            quad->u0    = (s16)((s16)frameAge % EFFECT_PUFF_FRAME_COUNT) * EFFECT_PUFF_CELL_SIZE;
            frameAge    = work->age;
            quad->v1    = EFFECT_PUFF_TEXTURE_V;
            quad->u1    = ((s16)((s16)frameAge % EFFECT_PUFF_FRAME_COUNT) * EFFECT_PUFF_CELL_SIZE) + EFFECT_PUFF_CELL_SIZE - 1;
            frameAge    = work->age;
            quad->v2    = EFFECT_PUFF_TEXTURE_V + EFFECT_PUFF_CELL_SIZE - 1;
            quad->u2    = (s16)((s16)frameAge % EFFECT_PUFF_FRAME_COUNT) * EFFECT_PUFF_CELL_SIZE;
            frameAge    = work->age;
            quad->v3    = EFFECT_PUFF_TEXTURE_V + EFFECT_PUFF_CELL_SIZE - 1;
            quad->u3    = ((s16)((s16)frameAge % EFFECT_PUFF_FRAME_COUNT) * EFFECT_PUFF_CELL_SIZE) + EFFECT_PUFF_CELL_SIZE - 1;
            _effectSetPerspectiveBillboardCorners(scratch, quad, work->period * (EFFECT_PUFF_CELL_SIZE - 1));
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        if (work->step != 0) {
            coord->coord.t[1]  -= work->step;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        }
        work->age++;
        if (work->angle >= work->age) {
            return;
        }
    }
    effectKillTask(work, task);
}

void effectSpriteTask8D(Task* task)
{
    enum {
        EFFECT_PUFF_MIN_LAST_AGE        = 16,
        EFFECT_PUFF_LIFETIME_MASK       = 7,
        EFFECT_PUFF_RISE_SPEED_RANGE    = 48,
        EFFECT_PUFF_GROW_TICKS          = 12,
        EFFECT_PUFF_FADE_TICKS          = 8,
        EFFECT_PUFF_CELL_SIZE           = 24,
        EFFECT_PUFF_FRAME_COUNT         = 8,
        EFFECT_PUFF_TEXTURE_V           = 160,
        EFFECT_PUFF_TEXTURE_PAGE        = getTPage(0, GPU_BLEND_ADD, 512, 0),
        EFFECT_PUFF_CLUT                = getClut(208, 268),
        EFFECT_PUFF_DEFAULT_SIZE        = 512,
        EFFECT_PUFF_FADE_STEP           = 16,
        EFFECT_PUFF_RETAINED_SPAWN_FLAG = 0x80000000,
    };
    EffectCentreScratch* scratch;
    GfxCoord*            coord;
    EffectWork*          work;
    POLY_FT4*            quad;
    s32                  size;
    s32                  requestedSize;
    u16                  frameAge;
    s32                  currentSize;
    s32                  brightness;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        actorRenderComposeCoord(coord);
        scratch                = SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
        scratch->worldPoint.vx = coord->workm.t[0];
        scratch->worldPoint.vy = coord->workm.t[1];
        scratch->worldPoint.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&scratch->worldPoint);
        gte_rtps();
        gte_stsxy(&scratch->screenX);
        gte_stflg(&scratch->projectionFlags);
        if (scratch->projectionFlags >= 0) {
            gte_stszotz(&scratch->depth);
            scratch->depth++;
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setlen(quad, EFFECT_DRAW_TEXTURED_QUAD_PACKET_WORDS);
            setcode(quad, EFFECT_DRAW_TEXTURED_QUAD);
            // Initialization waits for an accepted projection; age still advances offscreen.
            if (task->state == EFFECT_DRAW_TASK_NEW) {
                requestedSize = (u16)task->spawnArg1.value & EFFECT_DRAW_SIZE_MASK;
                size          = EFFECT_PUFF_DEFAULT_SIZE;
                if (requestedSize != 0) {
                    size = requestedSize;
                }
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->scale     = size;
                work->angle     = ((gRandomLcgState >> 16) & EFFECT_PUFF_LIFETIME_MASK) + EFFECT_PUFF_MIN_LAST_AGE;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->step      = (gRandomLcgState >> 16) % EFFECT_PUFF_RISE_SPEED_RANGE;
                task->state++;
                task->spawnArg1.value &= EFFECT_PUFF_RETAINED_SPAWN_FLAG;
            }
            if (work->age < EFFECT_PUFF_GROW_TICKS) {
                currentSize = work->scale * work->age / EFFECT_PUFF_GROW_TICKS;
            } else {
                currentSize = (u16)work->scale;
            }
            work->period = currentSize;
            if (work->angle - EFFECT_PUFF_FADE_TICKS < work->age) {
                brightness = (work->angle - work->age + 1) * EFFECT_PUFF_FADE_STEP;
                setRGB0(quad, brightness, brightness, brightness);
            } else {
                quad->code |= EFFECT_DRAW_RAW_TEXTURE;
            }
            quad->tpage = EFFECT_PUFF_TEXTURE_PAGE;
            quad->clut  = EFFECT_PUFF_CLUT;
            quad->code |= EFFECT_DRAW_SEMITRANSPARENT;
            frameAge    = work->age;
            quad->v0    = EFFECT_PUFF_TEXTURE_V;
            quad->u0    = (frameAge & (EFFECT_PUFF_FRAME_COUNT - 1)) * EFFECT_PUFF_CELL_SIZE;
            frameAge    = work->age;
            quad->v1    = EFFECT_PUFF_TEXTURE_V;
            quad->u1    = ((frameAge & (EFFECT_PUFF_FRAME_COUNT - 1)) * EFFECT_PUFF_CELL_SIZE) + EFFECT_PUFF_CELL_SIZE - 1;
            frameAge    = work->age;
            quad->v2    = EFFECT_PUFF_TEXTURE_V + EFFECT_PUFF_CELL_SIZE - 1;
            quad->u2    = (frameAge & (EFFECT_PUFF_FRAME_COUNT - 1)) * EFFECT_PUFF_CELL_SIZE;
            frameAge    = work->age;
            quad->v3    = EFFECT_PUFF_TEXTURE_V + EFFECT_PUFF_CELL_SIZE - 1;
            quad->u3    = ((frameAge & (EFFECT_PUFF_FRAME_COUNT - 1)) * EFFECT_PUFF_CELL_SIZE) + EFFECT_PUFF_CELL_SIZE - 1;
            _effectSetPerspectiveBillboardCorners(scratch, quad, work->period * (EFFECT_PUFF_CELL_SIZE - 1));
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        coord->coord.t[1]  -= work->step;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        work->age++;
        if (work->angle >= work->age) {
            return;
        }
    }
    effectKillTask(work, task);
}

void effectSpriteTask3F(Task* task)
{
    enum {
        EFFECT_BURST_PARTICLE_FRAME_COUNT       = 8,
        EFFECT_BURST_PARTICLE_CELL_SIZE         = 32,
        EFFECT_BURST_PARTICLE_TEXTURE_V         = 24,
        EFFECT_BURST_PARTICLE_TEXTURE_PAGE      = getTPage(0, GPU_BLEND_ADD, 512, 0),
        EFFECT_BURST_PARTICLE_CLUT              = getClut(304, 265),
        EFFECT_BURST_PARTICLE_SIZE_LIFT_DIVISOR = 736,
    };
    EffectWork*         work;
    GfxCoord*           coord;
    EffectShapeScratch* scratch;
    POLY_FT4*           quad;
    s32                 attachmentBits;
    s32                 size;

/// Writes rotated screen corners using the live scratch, quad and work locals.
///
/// Use as a standalone statement in a braced block. Captures those three
/// locals and the enclosing task's cell-size constant, retains repeated field
/// reads and narrows each offset before adding it to the projected origin.
#define EFFECT_PROJECTILE_BURST_PARTICLE_SET_CORNERS()                                                                                                                                 \
    scratch->extent.corner.x = (((work->scale * (EFFECT_BURST_PARTICLE_CELL_SIZE - 1)) / scratch->depth) * rsin(work->angle)) >> EFFECT_DRAW_FRACTION_BITS;                            \
    scratch->extent.corner.y = (((work->scale * (EFFECT_BURST_PARTICLE_CELL_SIZE - 1)) / scratch->depth) * rcos(work->angle)) >> EFFECT_DRAW_FRACTION_BITS;                            \
    quad->x0                 = scratch->screenX + (u16)scratch->extent.corner.x;                                                                                                       \
    quad->x3                 = scratch->screenX - (u16)scratch->extent.corner.x;                                                                                                       \
    quad->y0                 = scratch->screenY - (u16)scratch->extent.corner.y;                                                                                                       \
    quad->y3                 = scratch->screenY + (u16)scratch->extent.corner.y;                                                                                                       \
    scratch->extent.corner.x = (((work->scale * (EFFECT_BURST_PARTICLE_CELL_SIZE - 1)) / scratch->depth) * rsin(work->angle + EFFECT_DRAW_QUARTER_TURN)) >> EFFECT_DRAW_FRACTION_BITS; \
    scratch->extent.corner.y = (((work->scale * (EFFECT_BURST_PARTICLE_CELL_SIZE - 1)) / scratch->depth) * rcos(work->angle + EFFECT_DRAW_QUARTER_TURN)) >> EFFECT_DRAW_FRACTION_BITS; \
    quad->x1                 = scratch->screenX + (u16)scratch->extent.corner.x;                                                                                                       \
    quad->x2                 = scratch->screenX - (u16)scratch->extent.corner.x;                                                                                                       \
    quad->y1                 = scratch->screenY - (u16)scratch->extent.corner.y;                                                                                                       \
    quad->y2                 = scratch->screenY + (u16)scratch->extent.corner.y;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        actorRenderComposeCoord(coord);
        scratch                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
        scratch->worldPoint.vx = coord->workm.t[0];
        scratch->worldPoint.vy = coord->workm.t[1];
        scratch->worldPoint.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&scratch->worldPoint);
        gte_rtps();
        gte_stsxy(&scratch->screenX);
        gte_stflg(&scratch->projectionFlags);
        if (scratch->projectionFlags >= 0) {
            // Initialize after projection, retaining its screen point when reparenting.
            if (task->state == EFFECT_DRAW_TASK_NEW) {
                work->period    = ((task->spawnArg1.value >> 12) & 3) + 2;
                size            = (u16)task->spawnArg1.value & EFFECT_DRAW_SIZE_MASK;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->angle     = (gRandomLcgState >> 16) & EFFECT_DRAW_ANGLE_MASK;
                work->scale     = size;
                attachmentBits  = task->spawnArg1.halves.high;
                work->index     = attachmentBits & 1;
                if (work->index != 0) {
                    coord->parent = work->parent;
                    gfxSetRotIdentity(&coord->coord);
                    coord->coord.t[2]   = 0;
                    coord->coord.t[1]   = 0;
                    coord->coord.t[0]   = 0;
                    coord->composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(coord);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->move.vy   = -((gRandomLcgState >> 16) & 3);
                } else {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->move.vx   = ((gRandomLcgState >> 16) & 0xF) - 8;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->move.vy   = -((gRandomLcgState >> 16) & 0xF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->move.vz   = ((gRandomLcgState >> 16) & 0xF) - 8;
                }
                task->state++;
            }
            gte_stszotz(&scratch->depth);
            scratch->depth = scratch->depth + 1;
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setlen(quad, EFFECT_DRAW_TEXTURED_QUAD_PACKET_WORDS);
            setcode(quad, EFFECT_DRAW_RAW_ADDITIVE_TEXTURED_QUAD);
            quad->tpage = EFFECT_BURST_PARTICLE_TEXTURE_PAGE;
            quad->clut  = EFFECT_BURST_PARTICLE_CLUT;
            setUV4(quad, (work->age / work->period) * EFFECT_BURST_PARTICLE_CELL_SIZE, EFFECT_BURST_PARTICLE_TEXTURE_V,
                   ((work->age / work->period) * EFFECT_BURST_PARTICLE_CELL_SIZE) + EFFECT_BURST_PARTICLE_CELL_SIZE - 1, EFFECT_BURST_PARTICLE_TEXTURE_V,
                   (work->age / work->period) * EFFECT_BURST_PARTICLE_CELL_SIZE, EFFECT_BURST_PARTICLE_TEXTURE_V + EFFECT_BURST_PARTICLE_CELL_SIZE - 1,
                   ((work->age / work->period) * EFFECT_BURST_PARTICLE_CELL_SIZE) + EFFECT_BURST_PARTICLE_CELL_SIZE - 1, EFFECT_BURST_PARTICLE_TEXTURE_V + EFFECT_BURST_PARTICLE_CELL_SIZE - 1);
            EFFECT_PROJECTILE_BURST_PARTICLE_SET_CORNERS();
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        if (work->index != 0) {
            coord->coord.t[1]  += work->move.vy;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        } else {
            coord->coord.t[0]  += work->move.vx;
            coord->coord.t[1]  += work->move.vy;
            coord->coord.t[2]  += work->move.vz;
            coord->coord.t[1]  -= (s16)(work->scale / EFFECT_BURST_PARTICLE_SIZE_LIFT_DIVISOR);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        }
        work->age++;
        if (work->age <= (work->period * EFFECT_BURST_PARTICLE_FRAME_COUNT) - 1) {
            return;
        }
    }
    effectKillTask(work, task);
#undef EFFECT_PROJECTILE_BURST_PARTICLE_SET_CORNERS
}

/// Attempts one independently placed flash or fading spark within an emitter cube.
///
/// Requires live emitter work and a composed coordinate, emissionWidth 1..32767
/// game-coordinate units, halfWidth = emissionWidth / 2 and positive period.
/// Consumes five random samples, even when spawning fails. Writes the offset
/// into `move`; the child snapshots it and runs independently of the emitter.
/// Age biases the choice toward fading sparks; the size argument is 128..639.
static inline void _effectEmitHitSparkParticle(EffectWork* work, GfxCoord* coord, s16 emissionWidth, s32 halfWidth)
{
    enum {
        EFFECT_HIT_SPARK_PARTICLE_SIZE_MASK = 511,
        EFFECT_HIT_SPARK_PARTICLE_MIN_SIZE  = 128,
    };
    s32 transitionAge;
    s32 particleEffectId;

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->move.vx   = (s32)(gRandomLcgState >> 16) % emissionWidth - halfWidth;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->move.vy   = (s32)(gRandomLcgState >> 16) % emissionWidth - halfWidth;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->move.vz   = (s32)(gRandomLcgState >> 16) % emissionWidth - halfWidth;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    transitionAge   = (s32)(gRandomLcgState >> 16) % work->period;

    particleEffectId = EFFECT_SPARK_FADE;
    if (work->age < transitionAge) {
        particleEffectId = EFFECT_FLASH_BURST;
    }
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    effectSpawn(particleEffectId, coord, ((gRandomLcgState >> 16) & EFFECT_HIT_SPARK_PARTICLE_SIZE_MASK) + EFFECT_HIT_SPARK_PARTICLE_MIN_SIZE, &work->move);
}

void effectHitSparkBurstTask(Task* task)
{
    enum {
        EFFECT_HIT_SPARK_DENSITY_SHIFT   = 10,
        EFFECT_HIT_SPARK_DURATION_SHIFT  = 2,
        EFFECT_HIT_SPARK_FRAME_AGE_SHIFT = 1,
        EFFECT_HIT_SPARK_BILLBOARD_INSET = 64,
        EFFECT_HIT_SPARK_EMISSION_MASK   = 3,
    };
    EffectWork*  work;
    GfxCoord*    coord;
    s16          effectControl;
    register s32 previousRandomState asm("v1");
    s32          requestedWidth;
    s32          durationUnits;
    s32          particleIndex;
    s32          halfWidth;
    s16          emissionWidth;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        effectKillTask(work, task);
    } else {
        // Attach at the copied offset; the high half tunes chance and lifetime.
        if (task->state == EFFECT_DRAW_TASK_NEW) {
            coord->parent = work->parent;
            gfxSetRotIdentity(&coord->coord);
            coord->coord.t[0]   = work->pos.vx;
            coord->coord.t[1]   = work->pos.vy;
            coord->coord.t[2]   = work->pos.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->state         = EFFECT_DRAW_TASK_ACTIVE;
            // The previous random state shares $v1 with the identity's ONE.
            previousRandomState = gRandomLcgState;
            gRandomLcgState     = previousRandomState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->index         = (gRandomLcgState >> 16) & EFFECT_DRAW_ANGLE_MASK;
            requestedWidth      = task->spawnArg1.halves.low;
            work->scale         = requestedWidth;
            durationUnits       = task->spawnArg1.halves.high;
            work->step          = ((s16)requestedWidth >> EFFECT_HIT_SPARK_DENSITY_SHIFT) + 1;
            work->angle         = durationUnits;
            work->period        = durationUnits << EFFECT_HIT_SPARK_DURATION_SHIFT;
        }
        actorRenderComposeCoord(coord);
        // Draw before the running-only expiry test, including the final frame.
        _effectDrawSparkBurstBillboard(coord, (u16)(work->age >> EFFECT_HIT_SPARK_FRAME_AGE_SHIFT), work->scale - EFFECT_HIT_SPARK_BILLBOARD_INSET, work->index);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        if (work->age >= work->period) {
            effectKillTask(work, task);
            return;
        }
        emissionWidth = work->scale;
        halfWidth     = emissionWidth >> 1;
        for (particleIndex = 0; particleIndex < work->step; particleIndex++) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (work->angle >= (s32)((gRandomLcgState >> 16) & EFFECT_HIT_SPARK_EMISSION_MASK)) {
                _effectEmitHitSparkParticle(work, coord, emissionWidth, halfWidth);
            }
        }
        work->age++;
    }
}

void effectSpriteTaskE0(Task* task)
{
    enum {
        EFFECT_BURST_CLUT_BASE_Y        = 266,
        EFFECT_BURST_CLUT_ROW_SHIFT     = 6,
        EFFECT_BURST_CLUT_COLUMN_STRIDE = 6,
        EFFECT_BURST_CLUT_COLUMN_MASK   = 63,
        EFFECT_BURST_CELL_SIZE          = 40,
        EFFECT_BURST_TEXTURE_V          = 80,
        EFFECT_BURST_FRAME_COUNT        = 6,
        EFFECT_BURST_DEPTH_BIAS         = 32,
        EFFECT_BURST_MIN_DEPTH          = 16,
        EFFECT_BURST_TEXTURE_PAGE       = getTPage(0, GPU_BLEND_ADD, 576, 0),
    };
    EffectWork*         work;
    GfxCoord*           coord;
    EffectShapeScratch* scratchEnd;
    EffectShapeScratch* scratch;
    EffectShapeScratch* projectionScratch;
    POLY_FT4*           quad;
    s16                 effectControl;
    s32                 baseSize;
    s32                 paletteBits;
    s32                 clut;
    s32                 textureV;
    s32                 frameAge;
    u16                 worldZ;

/// Writes rotated screen corners using the live scratch, quad and work locals.
///
/// Use as a standalone statement in a braced block. Captures those three
/// locals and the enclosing task's cell-size constant, retains repeated field
/// reads and narrows each offset before adding it to the projected origin.
#define EFFECT_FLASH_BURST_SET_CORNERS()                                                                                                                                      \
    scratch->extent.corner.x = (((work->scale * (EFFECT_BURST_CELL_SIZE - 1)) / scratch->depth) * rsin(work->angle)) >> EFFECT_DRAW_FRACTION_BITS;                            \
    scratch->extent.corner.y = (((work->scale * (EFFECT_BURST_CELL_SIZE - 1)) / scratch->depth) * rcos(work->angle)) >> EFFECT_DRAW_FRACTION_BITS;                            \
    quad->x0                 = scratch->screenX + (u16)scratch->extent.corner.x;                                                                                              \
    quad->x3                 = scratch->screenX - (u16)scratch->extent.corner.x;                                                                                              \
    quad->y0                 = scratch->screenY - (u16)scratch->extent.corner.y;                                                                                              \
    quad->y3                 = scratch->screenY + (u16)scratch->extent.corner.y;                                                                                              \
    scratch->extent.corner.x = (((work->scale * (EFFECT_BURST_CELL_SIZE - 1)) / scratch->depth) * rsin(work->angle + EFFECT_DRAW_QUARTER_TURN)) >> EFFECT_DRAW_FRACTION_BITS; \
    scratch->extent.corner.y = (((work->scale * (EFFECT_BURST_CELL_SIZE - 1)) / scratch->depth) * rcos(work->angle + EFFECT_DRAW_QUARTER_TURN)) >> EFFECT_DRAW_FRACTION_BITS; \
    quad->x1                 = scratch->screenX + (u16)scratch->extent.corner.x;                                                                                              \
    quad->x2                 = scratch->screenX - (u16)scratch->extent.corner.x;                                                                                              \
    quad->y1                 = scratch->screenY - (u16)scratch->extent.corner.y;                                                                                              \
    quad->y2                 = scratch->screenY + (u16)scratch->extent.corner.y;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        actorRenderComposeCoord(coord);
        scratchEnd                               = SCRATCH_STACK_CURSOR(EffectShapeScratch);
        (scratchEnd - 1)->worldPoint.vx          = (u16)coord->workm.t[0];
        scratch                                  = scratchEnd - 1;
        scratch->worldPoint.vy                   = (u16)coord->workm.t[1];
        worldZ                                   = (u16)coord->workm.t[2];
        SCRATCH_STACK_CURSOR(EffectShapeScratch) = scratch;
        scratch->worldPoint.vz                   = worldZ;
        projectionScratch                        = scratch;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&projectionScratch->worldPoint);
        gte_rtps();
        gte_stsxy(&(scratchEnd - 1)->screenX);
        gte_stflg(&(scratchEnd - 1)->projectionFlags);
        if (scratch->projectionFlags >= 0) {
            gte_stszotz(&(scratchEnd - 1)->depth);
            scratch->depth -= EFFECT_BURST_DEPTH_BIAS;
            if (scratch->depth < EFFECT_BURST_MIN_DEPTH) {
                scratch->depth = EFFECT_BURST_MIN_DEPTH;
            }
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setlen(quad, EFFECT_DRAW_TEXTURED_QUAD_PACKET_WORDS);
            setcode(quad, EFFECT_DRAW_TEXTURED_QUAD);
            // The first visible cell chooses size and rotation; offscreen ticks still age.
            if (task->state == EFFECT_DRAW_TASK_NEW) {
                baseSize        = (u16)task->spawnArg1.value & EFFECT_DRAW_SIZE_MASK;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->scale     = baseSize + ((gRandomLcgState >> 16) & 0xFF);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->angle     = (gRandomLcgState >> 16) & EFFECT_DRAW_ANGLE_MASK;
                paletteBits     = task->spawnArg1.halves.high;
                work->step      = paletteBits;
                task->state     = EFFECT_DRAW_TASK_ACTIVE;
            }
            quad->tpage = EFFECT_BURST_TEXTURE_PAGE;
            quad->code |= EFFECT_DRAW_RAW_TEXTURE | EFFECT_DRAW_SEMITRANSPARENT;
            clut        = (((u16)work->step + EFFECT_BURST_CLUT_BASE_Y) << EFFECT_BURST_CLUT_ROW_SHIFT) | ((work->step * EFFECT_BURST_CLUT_COLUMN_STRIDE) & EFFECT_BURST_CLUT_COLUMN_MASK);
            quad->clut  = clut;
            frameAge    = work->age;
            textureV    = EFFECT_BURST_TEXTURE_V;
            quad->v0    = textureV;
            quad->u0    = frameAge * EFFECT_BURST_CELL_SIZE;
            frameAge    = work->age;
            quad->v1    = textureV;
            quad->u1    = frameAge * EFFECT_BURST_CELL_SIZE + EFFECT_BURST_CELL_SIZE - 1;
            frameAge    = work->age;
            textureV    = EFFECT_BURST_TEXTURE_V + EFFECT_BURST_CELL_SIZE - 1;
            quad->v2    = textureV;
            quad->u2    = frameAge * EFFECT_BURST_CELL_SIZE;
            frameAge    = work->age;
            quad->v3    = textureV;
            quad->u3    = frameAge * EFFECT_BURST_CELL_SIZE + EFFECT_BURST_CELL_SIZE - 1;
            EFFECT_FLASH_BURST_SET_CORNERS();
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        work->age++;
        if (work->age < EFFECT_BURST_FRAME_COUNT) {
            return;
        }
    } else if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        return;
    }
    effectKillTask(work, task);
#undef EFFECT_FLASH_BURST_SET_CORNERS
}

void effectSpriteTaskE1(Task* task)
{
    enum {
        EFFECT_BURST_CLUT_BASE_X       = 192,
        EFFECT_BURST_CLUT_BASE_Y       = 268,
        EFFECT_BURST_CLUT_X_STRIDE     = 80,
        EFFECT_BURST_CLUT_ROW_SHIFT    = 6,
        EFFECT_BURST_CLUT_COLUMN_SHIFT = 4,
        EFFECT_BURST_CLUT_COLUMN_MASK  = 63,
        EFFECT_BURST_CELL_SIZE         = 24,
        EFFECT_BURST_TEXTURE_V         = 136,
        EFFECT_BURST_FRAME_COUNT       = 8,
        EFFECT_BURST_DEPTH_BIAS        = 32,
        EFFECT_BURST_MIN_DEPTH         = 16,
        EFFECT_BURST_TEXTURE_PAGE      = getTPage(0, GPU_BLEND_ADD, 512, 0),
    };
    EffectWork*         work;
    GfxCoord*           coord;
    EffectShapeScratch* scratchEnd;
    EffectShapeScratch* scratch;
    EffectShapeScratch* projectionScratch;
    POLY_FT4*           quad;
    s16                 effectControl;
    s32                 baseSize;
    s32                 paletteBits;
    s32                 textureValue;
    s32                 frameAge;
    u16                 worldZ;

/// Writes rotated screen corners using the live scratch, quad and work locals.
///
/// Use as a standalone statement in a braced block. Captures those three
/// locals and the enclosing task's cell-size constant, retains repeated field
/// reads and narrows each offset before adding it to the projected origin.
#define EFFECT_SPARK_FADE_SET_CORNERS()                                                                                                                                       \
    scratch->extent.corner.x = (((work->scale * (EFFECT_BURST_CELL_SIZE - 1)) / scratch->depth) * rsin(work->angle)) >> EFFECT_DRAW_FRACTION_BITS;                            \
    scratch->extent.corner.y = (((work->scale * (EFFECT_BURST_CELL_SIZE - 1)) / scratch->depth) * rcos(work->angle)) >> EFFECT_DRAW_FRACTION_BITS;                            \
    quad->x0                 = scratch->screenX + (u16)scratch->extent.corner.x;                                                                                              \
    quad->x3                 = scratch->screenX - (u16)scratch->extent.corner.x;                                                                                              \
    quad->y0                 = scratch->screenY - (u16)scratch->extent.corner.y;                                                                                              \
    quad->y3                 = scratch->screenY + (u16)scratch->extent.corner.y;                                                                                              \
    scratch->extent.corner.x = (((work->scale * (EFFECT_BURST_CELL_SIZE - 1)) / scratch->depth) * rsin(work->angle + EFFECT_DRAW_QUARTER_TURN)) >> EFFECT_DRAW_FRACTION_BITS; \
    scratch->extent.corner.y = (((work->scale * (EFFECT_BURST_CELL_SIZE - 1)) / scratch->depth) * rcos(work->angle + EFFECT_DRAW_QUARTER_TURN)) >> EFFECT_DRAW_FRACTION_BITS; \
    quad->x1                 = scratch->screenX + (u16)scratch->extent.corner.x;                                                                                              \
    quad->x2                 = scratch->screenX - (u16)scratch->extent.corner.x;                                                                                              \
    quad->y1                 = scratch->screenY - (u16)scratch->extent.corner.y;                                                                                              \
    quad->y2                 = scratch->screenY + (u16)scratch->extent.corner.y;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
    } else {
        scratchEnd                               = SCRATCH_STACK_CURSOR(EffectShapeScratch);
        scratch                                  = scratchEnd - 1;
        SCRATCH_STACK_CURSOR(EffectShapeScratch) = scratch;
        projectionScratch                        = scratch;
        // Initialize before projection so offscreen ticks use the same lifetime.
        if (task->state == EFFECT_DRAW_TASK_NEW) {
            baseSize        = (u16)task->spawnArg1.value & EFFECT_DRAW_SIZE_MASK;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->scale     = baseSize + ((gRandomLcgState >> 16) & 0xFF);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->angle     = (gRandomLcgState >> 16) & EFFECT_DRAW_ANGLE_MASK;
            paletteBits     = task->spawnArg1.halves.high;
            work->step      = paletteBits;
            task->state++;
        }
        actorRenderComposeCoord(coord);
        (scratchEnd - 1)->worldPoint.vx = (u16)coord->workm.t[0];
        scratch->worldPoint.vy          = (u16)coord->workm.t[1];
        worldZ                          = (u16)coord->workm.t[2];
        scratch->worldPoint.vz          = worldZ;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&projectionScratch->worldPoint);
        gte_rtps();
        gte_stsxy(&(scratchEnd - 1)->screenX);
        gte_stflg(&(scratchEnd - 1)->projectionFlags);
        if (scratch->projectionFlags >= 0) {
            gte_stszotz(&(scratchEnd - 1)->depth);
            scratch->depth -= EFFECT_BURST_DEPTH_BIAS;
            if (scratch->depth < EFFECT_BURST_MIN_DEPTH) {
                scratch->depth = EFFECT_BURST_MIN_DEPTH;
            }
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setlen(quad, EFFECT_DRAW_TEXTURED_QUAD_PACKET_WORDS);
            setcode(quad, EFFECT_DRAW_RAW_ADDITIVE_TEXTURED_QUAD);
            quad->tpage  = EFFECT_BURST_TEXTURE_PAGE;
            quad->clut   = ((EFFECT_BURST_CLUT_BASE_Y - work->step) << EFFECT_BURST_CLUT_ROW_SHIFT) | (((EFFECT_BURST_CLUT_BASE_X - work->step * EFFECT_BURST_CLUT_X_STRIDE) >> EFFECT_BURST_CLUT_COLUMN_SHIFT) & EFFECT_BURST_CLUT_COLUMN_MASK);
            frameAge     = work->age;
            textureValue = EFFECT_BURST_TEXTURE_V;
            quad->v0     = textureValue;
            quad->u0     = frameAge * EFFECT_BURST_CELL_SIZE;
            frameAge     = work->age;
            quad->v1     = textureValue;
            quad->u1     = frameAge * EFFECT_BURST_CELL_SIZE + EFFECT_BURST_CELL_SIZE - 1;
            frameAge     = work->age;
            textureValue = EFFECT_BURST_TEXTURE_V + EFFECT_BURST_CELL_SIZE - 1;
            quad->v2     = textureValue;
            quad->u2     = frameAge * EFFECT_BURST_CELL_SIZE;
            frameAge     = work->age;
            quad->v3     = textureValue;
            quad->u3     = frameAge * EFFECT_BURST_CELL_SIZE + EFFECT_BURST_CELL_SIZE - 1;
            EFFECT_SPARK_FADE_SET_CORNERS();
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        work->age++;
        if (work->age < EFFECT_BURST_FRAME_COUNT) {
            return;
        }
    }
    effectKillTask(work, task);
#undef EFFECT_SPARK_FADE_SET_CORNERS
}

/// Installs the spark burst's identity-oriented, parent-relative spawn placement.
///
/// Reads XYZ of the work's signed local offset and stores its borrowed parent,
/// which must remain live while the coordinate refers to it. Writes all nine
/// Q12 identity coefficients and XYZ translation without touching stored Euler
/// state or the cached matrix; refresh is deferred to the task.
static inline void _effectInitSparkBurstCoord(GfxCoord* coord, const EffectWork* work)
{
    coord->parent = work->parent;
    gfxSetRotIdentity(&coord->coord);
    coord->coord.t[0]   = work->pos.vx;
    coord->coord.t[1]   = work->pos.vy;
    coord->coord.t[2]   = work->pos.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

void effectSpriteTaskE2(Task* task)
{
    enum {
        EFFECT_SPARK_BURST_SIZE_JITTER_MASK = 0xFF,
        EFFECT_SPARK_BURST_DRAW_AGE_MASK    = 1,
        EFFECT_SPARK_BURST_FRAME_AGE_SHIFT  = 1,
        EFFECT_SPARK_BURST_LIFETIME_TICKS   = 12,
        EFFECT_SPARK_BURST_PALETTE_ZERO     = 0,
    };
    EffectWork* work;
    GfxCoord*   coord;
    s16         effectControl;
    s32         baseSize;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        if (task->state == EFFECT_DRAW_TASK_NEW) {
            if (task->spawnArg1.value < 0) {
                _effectInitSparkBurstCoord(coord, work);
            }
            baseSize        = (u16)task->spawnArg1.value & EFFECT_DRAW_SIZE_MASK;
            work->step      = EFFECT_SPARK_BURST_PALETTE_ZERO;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->scale     = baseSize + ((gRandomLcgState >> 16) & EFFECT_SPARK_BURST_SIZE_JITTER_MASK);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->angle     = (gRandomLcgState >> 16) & EFFECT_DRAW_ANGLE_MASK;
            task->state++;
        }
        actorRenderComposeCoord(coord);
        // Blink on alternate running ages.
        if (!(work->age & EFFECT_SPARK_BURST_DRAW_AGE_MASK)) {
            _effectDrawSparkBurstBillboard(coord, (u16)(work->age >> EFFECT_SPARK_BURST_FRAME_AGE_SHIFT),
                                           (s16)(work->scale | work->step), work->angle);
        }
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        work->age++;
        if (work->age < EFFECT_SPARK_BURST_LIFETIME_TICKS) {
            return;
        }
    } else if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        return;
    }
    effectKillTask(work, task);
}

/// Draws one raw additive, randomly oriented spark-burst animation cell.
///
/// coord is borrowed with its world matrix composed. frame selects a 40-texel
/// cell at V=56 on texture page 0x2A; U narrows to the GPU byte. packedSizePalette
/// has the unsigned size numerator in bits 0..11 and the palette selector in
/// bits 16..31. The screen half-diagonal is size * 39 / max(SZ3 / 4 - 32, 16)
/// pixels, rotated by angle (4096 units per turn). The palette chooses CLUT
/// X = 304 - 160 * (s16)palette, Y = 266 + palette, with GPU field truncation.
/// A negative projection FLAG suppresses drawing. Scratch is released before
/// returning; the queued packet borrows the frame arena until GPU completion.
static void _effectDrawSparkBurstBillboard(const GfxCoord* coord, u16 frame, u32 packedSizePalette, s16 angle)
{
    enum {
        EFFECT_BURST_CLUT_X        = 304,
        EFFECT_BURST_CLUT_X_STRIDE = 160,
        EFFECT_BURST_CLUT_Y        = 266,
        EFFECT_BURST_CELL_SIZE     = 40,
        EFFECT_BURST_TEXTURE_V     = 56,
        EFFECT_BURST_FRAME_COUNT   = 6,
        EFFECT_BURST_DEPTH_BIAS    = 32,
        EFFECT_BURST_MIN_DEPTH     = 16,
        EFFECT_BURST_TEXTURE_PAGE  = getTPage(0, GPU_BLEND_ADD, 640, 0),
    };
    EffectShapeScratch* scratchEnd;
    EffectShapeScratch* scratch;
    EffectShapeScratch* projectionScratch;
    POLY_FT4*           quad;
    u32                 palette;
    s32                 perpendicularAngle;
    u16                 worldZ;

/// Writes the spark-burst billboard's rotated screen corners.
///
/// Captures scratch, quad, packedSizePalette, angle and perpendicularAngle,
/// plus the enclosing cell-size constant. Retains unsigned perspective division
/// and offset narrowing to u16; use as a standalone statement in a braced block.
#define EFFECT_SPARK_BURST_BILLBOARD_SET_CORNERS()                                                                                                              \
    scratch->extent.corner.x = (((packedSizePalette * (EFFECT_BURST_CELL_SIZE - 1)) / scratch->depth) * rsin(angle)) >> EFFECT_DRAW_FRACTION_BITS;              \
    scratch->extent.corner.y = (((packedSizePalette * (EFFECT_BURST_CELL_SIZE - 1)) / scratch->depth) * rcos(angle)) >> EFFECT_DRAW_FRACTION_BITS;              \
    quad->x0                 = scratch->screenX + (u16)scratch->extent.corner.x;                                                                                \
    quad->x3                 = scratch->screenX - (u16)scratch->extent.corner.x;                                                                                \
    quad->y0                 = scratch->screenY - (u16)scratch->extent.corner.y;                                                                                \
    quad->y3                 = scratch->screenY + (u16)scratch->extent.corner.y;                                                                                \
    perpendicularAngle       = angle + EFFECT_DRAW_QUARTER_TURN;                                                                                                \
    scratch->extent.corner.x = (((packedSizePalette * (EFFECT_BURST_CELL_SIZE - 1)) / scratch->depth) * rsin(perpendicularAngle)) >> EFFECT_DRAW_FRACTION_BITS; \
    scratch->extent.corner.y = (((packedSizePalette * (EFFECT_BURST_CELL_SIZE - 1)) / scratch->depth) * rcos(perpendicularAngle)) >> EFFECT_DRAW_FRACTION_BITS; \
    quad->x1                 = scratch->screenX + (u16)scratch->extent.corner.x;                                                                                \
    quad->x2                 = scratch->screenX - (u16)scratch->extent.corner.x;                                                                                \
    quad->y1                 = scratch->screenY - (u16)scratch->extent.corner.y;                                                                                \
    quad->y2                 = scratch->screenY + (u16)scratch->extent.corner.y;

    scratchEnd                               = SCRATCH_STACK_CURSOR(EffectShapeScratch);
    (scratchEnd - 1)->worldPoint.vx          = (u16)coord->workm.t[0];
    scratch                                  = scratchEnd - 1;
    scratch->worldPoint.vy                   = (u16)coord->workm.t[1];
    worldZ                                   = (u16)coord->workm.t[2];
    SCRATCH_STACK_CURSOR(EffectShapeScratch) = scratch;
    scratch->worldPoint.vz                   = worldZ;
    projectionScratch                        = scratch;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projectionScratch->worldPoint);
    gte_rtps();
    palette            = packedSizePalette >> 16;
    packedSizePalette &= EFFECT_DRAW_SIZE_MASK;
    gte_stsxy(&(scratchEnd - 1)->screenX);
    gte_stflg(&(scratchEnd - 1)->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&(scratchEnd - 1)->depth);
        // Pull the billboard forward for both perspective sizing and sorting.
        scratch->depth -= EFFECT_BURST_DEPTH_BIAS;
        if (scratch->depth < EFFECT_BURST_MIN_DEPTH) {
            scratch->depth = EFFECT_BURST_MIN_DEPTH;
        }
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setlen(quad, EFFECT_DRAW_TEXTURED_QUAD_PACKET_WORDS);
        setcode(quad, EFFECT_DRAW_RAW_ADDITIVE_TEXTURED_QUAD);
        quad->tpage = EFFECT_BURST_TEXTURE_PAGE;
        quad->clut  = getClut(EFFECT_BURST_CLUT_X - (s16)palette * EFFECT_BURST_CLUT_X_STRIDE, palette + EFFECT_BURST_CLUT_Y);
        setUV4(quad, frame * EFFECT_BURST_CELL_SIZE, EFFECT_BURST_TEXTURE_V, frame * EFFECT_BURST_CELL_SIZE + EFFECT_BURST_CELL_SIZE - 1, EFFECT_BURST_TEXTURE_V, frame * EFFECT_BURST_CELL_SIZE, EFFECT_BURST_TEXTURE_V + EFFECT_BURST_CELL_SIZE - 1,
               frame * EFFECT_BURST_CELL_SIZE + EFFECT_BURST_CELL_SIZE - 1, EFFECT_BURST_TEXTURE_V + EFFECT_BURST_CELL_SIZE - 1);
        EFFECT_SPARK_BURST_BILLBOARD_SET_CORNERS();
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
#undef EFFECT_SPARK_BURST_BILLBOARD_SET_CORNERS
}

/// Initializes and links one of the player's three motion-sphere collision bodies.
///
/// `bodyIndex` is ROOT, PART4 or PART1 and selects the matching motion context.
/// The signed local XYZ offset and unsigned radius use game-coordinate units;
/// `flags` must select MOTION_SPHERE and may encode the receiving body index.
/// Publishes the live save's character identity as a player-body contact key.
/// The body must be unlinked; actor, coordinate and contact storage are borrowed
/// until unlinking. Initialize contacts before collision passes; all three
/// callers share collisionContacts. Does not initialize the motion direction.
static inline void _playerActorLinkCollisionBody(GameActor* actor, s32 bodyIndex, WorldCollisionBody* body, GfxCoord* coord, WorldCollisionContact* contacts, s16 offsetX, s16 offsetY,
                                                 s16 offsetZ, u16 radius, u16 flags)
{
    body->context.motion                               = &actor->collisionMotionContexts[bodyIndex];
    body->coord                                        = coord;
    actor->collisionMotionContexts[bodyIndex].contacts = contacts;
    body->pos.vx                                       = offsetX;
    body->pos.vy                                       = offsetY;
    body->pos.vz                                       = offsetZ;
    body->key                                          = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId | WORLD_COLLISION_CONTACT_PLAYER_BODY;
    body->radius                                       = radius;
    body->flags                                        = flags;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_BODIES, body);
}

/// Initializes the live player's model, collision bodies and attachment tasks.
///
/// Entry is player-work Task state 0 with allocated GameActor/model playback
/// resources and root/parts 1 and 4 present. Advances to state 1 and installs
/// message/teardown callbacks. Registers the borrowed root matrix in player
/// status, links three motion spheres sharing the actor's contact array and
/// creates two owned attachment tasks. A successful second attachment restores
/// equipment. Preserves the initial mode/clip across those spawns; scripted
/// mode resets that clip with collision disabled. Acropolis Plaza restricts
/// running and aiming. The published root matrix is borrowed until model release;
/// teardown unlinks the bodies and clears the player task registry slot.
static void _playerActorInitWork(Task* task)
{
    enum {
        PLAYER_ACTOR_ROOT_COLLISION_RADIUS = 300,
        PLAYER_ACTOR_PART_COLLISION_RADIUS = 220,
        PLAYER_ACTOR_COLLISION_PART4       = 4,
        PLAYER_ACTOR_COLLISION_PART1       = 1,
    };
    GameActor*             actor;
    TmdObject*             model;
    GfxCoord*              rootCoord;
    WorldCollisionBody*    collisionBody;
    WorldCollisionContact* contacts;
    s32                    initialMode;
    s32                    initialAnimationId;
    AnimationPlayRequest   animationRequest;
    Task*                  attachmentTask;

    actor     = task->work;
    model     = task->extra.tmd;
    rootCoord = model->coords;
    task->state++;
    task->msgTable                              = Gp_PlayerMsgTable;
    task->exitCallback                          = _playerActorTeardown;
    actor->animationSlotCount                   = GAME_ACTOR_NORMAL_ANIMATION_SLOTS;
    gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER] = task;
    gPlayerStatus.coordMtx                      = &rootCoord->coord;
    rootCoord->parent                           = &gGfxViewCoord;
    rootCoord->composeStamp                     = GRAPHICS_COORD_DIRTY;
    model->flags                                = 0;
    RotMatrix(&actor->rotation, &rootCoord->coord);
    _animationBindPlayerWeaponBank(task);

    actor->animationRate       = ANIMATION_RATE_ONE;
    actor->previousPosition.vx = rootCoord->coord.t[0];
    actor->previousPosition.vy = rootCoord->coord.t[1];
    actor->previousPosition.vz = rootCoord->coord.t[2];

    // All motion spheres share the actor-owned contact table; only the root queries triggers.
    contacts      = actor->collisionContacts;
    collisionBody = &actor->collisionBodies[GAME_ACTOR_BODY_ROOT];
    _playerActorLinkCollisionBody(actor, GAME_ACTOR_BODY_ROOT, collisionBody, rootCoord, contacts, 0, -PLAYER_ACTOR_ROOT_COLLISION_RADIUS, 0, PLAYER_ACTOR_ROOT_COLLISION_RADIUS, WORLD_COLLISION_BODY_MOTION_SPHERE);
    worldCollisionInitContacts(actor->collisionMotionContexts[GAME_ACTOR_BODY_ROOT].contacts, ARRAY_SIZE(actor->collisionContacts), 0);
    collisionBody->flags |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_ROOM_TRIGGER_ENABLED | WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);

    collisionBody = &actor->collisionBodies[GAME_ACTOR_BODY_PART4];
    _playerActorLinkCollisionBody(actor, GAME_ACTOR_BODY_PART4, collisionBody, task->extra.tmd->coords + PLAYER_ACTOR_COLLISION_PART4, contacts, 0, 100, 40, PLAYER_ACTOR_PART_COLLISION_RADIUS, WORLD_COLLISION_BODY_MOTION_SPHERE | (GAME_ACTOR_BODY_PART4 << WORLD_COLLISION_CONTACT_BODY_INDEX_SHIFT));
    collisionBody->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

    collisionBody = &actor->collisionBodies[GAME_ACTOR_BODY_PART1];
    _playerActorLinkCollisionBody(actor, GAME_ACTOR_BODY_PART1, collisionBody, task->extra.tmd->coords + PLAYER_ACTOR_COLLISION_PART1, contacts, 0, 82, 0, PLAYER_ACTOR_PART_COLLISION_RADIUS, WORLD_COLLISION_BODY_MOTION_SPHERE | (GAME_ACTOR_BODY_PART1 << WORLD_COLLISION_CONTACT_BODY_INDEX_SHIFT));
    collisionBody->flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);

    // Attachment creation may update actor state, so retain the spawn's scripted request first.
    initialMode               = actor->mode;
    initialAnimationId        = actor->actionArgument;
    actor->attachmentTasks[0] = playerActorSpawnAttachment(task, 0, PLAYER_ACTOR_ATTACHMENT_PLAYER_RIG, PLAYER_ACTOR_ATTACHMENT_SECOND_PAIR);
    attachmentTask            = playerActorSpawnAttachment(task, 1, PLAYER_ACTOR_ATTACHMENT_PLAYER_RIG, PLAYER_ACTOR_ATTACHMENT_SECOND_PAIR);
    actor->attachmentTasks[1] = attachmentTask;
    if (attachmentTask != NULL) {
        equipmentSyncPrimaryAttackSelector();
        playerActorRestoreEquipment();
    }
    if (initialMode == GAME_ACTOR_MODE_SCRIPTED) {
        animationRequest.blendFrames          = 0;
        animationRequest.source.index         = actor->animationBankIndex;
        animationRequest.blend                = ANIMATION_BLEND_RESET;
        animationRequest.animationId          = initialAnimationId;
        animationRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        _playerActorPlayScriptedAnimation(task, 0, &animationRequest, 0);
        actor->collisionEnableMask = PLAYER_ACTOR_WORLD_COLLISION_DISABLE;
    }
    if ((GAME_LOCATION_WORD(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PLAZA, 0, 0)) {
        actor->restrictRunAndAim = 1;
    }
}

/// Tests whether the root's height differs from the accepted position by less than 768 game units.
///
/// Borrows positions in the same parent frame and ignores X/Z. Equality rejects
/// the step. The signed subtraction and its absolute value must fit s32.
static inline s32 _playerActorIsHeightStepWithinLimit(const GfxCoord* rootCoord, const VECTOR3* previousPosition)
{
    enum { PLAYER_ACTOR_VERTICAL_STEP_LIMIT = 0x300 };
    s32 verticalChange;

    verticalChange = rootCoord->coord.t[1];
    verticalChange = verticalChange - previousPosition->vy;
    verticalChange = ABS(verticalChange);
    return verticalChange < PLAYER_ACTOR_VERTICAL_STEP_LIMIT;
}

/// Updates collision response and deferred body enables, then composes the player root.
///
/// Task work and model root must be live. Outside scripted mode, a vertical
/// change of at least 768 game units restores all three accepted coordinates.
/// Otherwise records the position before applying grid contact pushback.
/// Applies enable/disable requests to the root and part-1 bodies, with enable
/// taking precedence, then consumes the entire request byte. Only bits 0/1 and
/// their disable counterparts 3/4 are applied; all other request bits are discarded.
static void _playerActorWorkState1(Task* task)
{
    enum { PLAYER_ACTOR_ROOT_GRID_RESPONSE = 1 };
    GameActor*          actor;
    GfxCoord*           rootCoord;
    WorldCollisionBody* collisionBodies[2];
    s32                 bodyIndex;
    s8                  collisionRequests;

    actor     = task->work;
    rootCoord = task->extra.tmd->coords;
    // Reject a discontinuous height without applying the old contact response.
    if (actor->mode != GAME_ACTOR_MODE_SCRIPTED &&
        !_playerActorIsHeightStepWithinLimit(rootCoord, &actor->previousPosition)) {
        rootCoord->coord.t[0] = actor->previousPosition.vx;
        rootCoord->coord.t[1] = actor->previousPosition.vy;
        rootCoord->coord.t[2] = actor->previousPosition.vz;
    } else {
        actor->previousPosition.vx = rootCoord->coord.t[0];
        actor->previousPosition.vy = rootCoord->coord.t[1];
        actor->previousPosition.vz = rootCoord->coord.t[2];
        if (actor->collisionEnableMask & PLAYER_ACTOR_ROOT_GRID_RESPONSE) {
            actor->gridResponse = worldCollisionApplyResponsePushback(rootCoord, actor->collisionMotionContexts[0].contacts, ARRAY_SIZE(actor->collisionContacts), &actor->surfaceClass);
        } else {
            actor->gridResponse = 0;
        }
    }

    // The request's bit order follows this body pair, rather than body-array indices.
    collisionBodies[0] = &actor->collisionBodies[GAME_ACTOR_BODY_ROOT];
    collisionBodies[1] = &actor->collisionBodies[GAME_ACTOR_BODY_PART1];
    for (bodyIndex = 0; bodyIndex < ARRAY_SIZE(collisionBodies); bodyIndex++) {
        collisionRequests = actor->pendingCollisionUpdates;
        if ((collisionRequests >> bodyIndex) & 1) {
            actor->collisionEnableMask        |= 1 << bodyIndex;
            collisionBodies[bodyIndex]->flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        } else if (collisionRequests & ((1 << GAME_ACTOR_COLLISION_DISABLE_REQUEST_SHIFT) << bodyIndex)) {
            actor->collisionEnableMask        &= ~(1 << bodyIndex);
            collisionBodies[bodyIndex]->flags &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
        }
    }
    actor->pendingCollisionUpdates = 0;
    rootCoord->composeStamp        = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
}

void playerActorInitWeaponCollision(Task* actorTask, s32 weaponId, s32 attackRow)
{
    enum {
        PLAYER_ACTOR_WEAPON_QUARTER_TURN       = ACTOR_TRANSFORM_ANGLE_TURN / 4,
        PLAYER_ACTOR_WEAPON_RADIUS             = 0x100,
        PLAYER_ACTOR_TONFA_RADIUS              = 0x280,
        PLAYER_ACTOR_WEAPON_SPREAD_RADIUS      = 0x900,
        PLAYER_ACTOR_TONFA_WEAPON_ID           = 0x13,
        PLAYER_ACTOR_SPREAD_ATTACK_ROW         = 0xD,
        PLAYER_ACTOR_WEAPON_DISTANCE_ROW_SHIFT = 8,
    };
    GameActor*             actor;
    WorldCollisionBody*    weaponBody;
    WorldCollisionCapsule* capsule;
    VECTOR*                endpoint;
    Task*                  weaponTask;
    s32                    radius;

    actor      = actorTask->work;
    weaponBody = &actor->collisionBodies[GAME_ACTOR_BODY_WEAPON];
    capsule    = &actor->weaponShape;
    SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    endpoint   = SCRATCH_STACK_CURSOR(VECTOR);
    weaponTask = actor->equipmentTasks[1];
    if (weaponTask != NULL) {
        // Keep a collision coordinate independent of the rendered weapon node.
        actor->weaponCollisionCoord = *weaponTask->extra.tmd->coords;
        gfxRotMatrixX(&actor->weaponCollisionCoord.workm, PLAYER_ACTOR_WEAPON_QUARTER_TURN, GRAPHICS_ROTATION_COMPOSE);
        weaponBody->coord                                  = &actor->weaponCollisionCoord;
        actor->weaponCollisionCoord.param.rot.vx           = 0;
        actor->weaponCollisionCoord.param.rot.vy           = 0;
        actor->weaponCollisionCoord.param.rot.vz           = 0;
        weaponBody->context.capsule                        = &actor->weaponShape;
        weaponBody->flags                                  = WORLD_COLLISION_BODY_CAPSULE;
        weaponBody->pos.vx                                 = 0;
        weaponBody->pos.vy                                 = 0;
        weaponBody->pos.vz                                 = 0;
        actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key = WORLD_COLLISION_CONTACT_ATTACK | (weaponId << PLAYER_ACTOR_WEAPON_DISTANCE_ROW_SHIFT) | attackRow;
        *endpoint                                          = D_80112FA4[weaponId];
        capsule->ends[1].vx                                = endpoint->vx;
        capsule->ends[1].vy                                = endpoint->vy;
        capsule->ends[1].vz                                = endpoint->vz;
        capsule->ends[0].vx                                = capsule->ends[1].vx;
        capsule->ends[0].vy                                = capsule->ends[1].vy;
        capsule->ends[0].vz                                = capsule->ends[1].vz + D_80112F60[weaponId];
        radius                                             = PLAYER_ACTOR_WEAPON_RADIUS;
        if (gPlayerStatus.weapon == PLAYER_ACTOR_TONFA_WEAPON_ID) {
            radius = PLAYER_ACTOR_TONFA_RADIUS;
        }
        capsule->end1Radius = radius;
        if (attackRow != PLAYER_ACTOR_SPREAD_ATTACK_ROW) {
            capsule->end0Radius = radius;
        } else {
            capsule->end0Radius = PLAYER_ACTOR_WEAPON_SPREAD_RADIUS;
        }
        capsule->contacts = actor->weaponContacts;
        worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, weaponBody);
        worldCollisionInitContacts(capsule->contacts, ARRAY_SIZE(actor->weaponContacts), 0);
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

s32 worldCollisionApplyResponsePushback(GfxCoord* coord, const WorldCollisionContact* contacts, s32 contactCount, s32* surfaceClassOut)
{
    WorldCollisionDelta* delta;
    s32                  response;

    delta    = SCRATCH_STACK_RESERVE_BLOCK(WorldCollisionDelta);
    response = worldCollisionResolveResponsePushback(contacts, delta, contactCount, surfaceClassOut);
    // Apply whole-unit correction, then turn the returned surface mask into a class.
    if (response != WORLD_COLLISION_PUSHBACK_NO_GRID_HIT) {
        GP_ROUND_FIXED_AWAY(delta->fixed.vx.word);
        GP_ROUND_FIXED_AWAY(delta->fixed.vy.word);
        GP_ROUND_FIXED_AWAY(delta->fixed.vz.word);
        coord->coord.t[0] += delta->fixed.vx.halves.integer;
        coord->coord.t[1] += delta->fixed.vy.halves.integer;
        coord->coord.t[2] += delta->fixed.vz.halves.integer;
        if (surfaceClassOut != NULL) {
            *surfaceClassOut = worldCollisionSurfaceClassFromMask((const u8*)surfaceClassOut);
        }
        if ((delta->fixed.vx.word | delta->fixed.vz.word) == 0) {
            response = WORLD_COLLISION_PUSHBACK_NO_GRID_HIT;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(WorldCollisionDelta);
    return response;
}

/// Walks the retained text-grid pen and colour state without drawing any text.
///
/// Has no callers in this image. Borrows one complete scratch-stack block for
/// two rows of three cells and releases it before returning; no output survives.
static void _playerActorWalkTextGrid(void)
{
    enum {
        PLAYER_ACTOR_TEXT_GRID_ROW_COUNT      = 2,
        PLAYER_ACTOR_TEXT_GRID_COLUMN_COUNT   = 3,
        PLAYER_ACTOR_TEXT_GRID_COLUMN_PITCH   = 64,
        PLAYER_ACTOR_TEXT_GRID_CELL_Y_STEP    = 80,
        PLAYER_ACTOR_TEXT_GRID_FIRST_ROW_Y    = -88,
        PLAYER_ACTOR_TEXT_GRID_SECOND_ROW_Y   = 8,
        PLAYER_ACTOR_TEXT_GRID_FIRST_ROW_RGB  = 0x808008,
        PLAYER_ACTOR_TEXT_GRID_SECOND_ROW_RGB = 0x37A78,
    };
    void**                       cursorSlot;
    _PlayerActorTextGridScratch* previousCursor;
    _PlayerActorTextGridScratch* block;
    _PlayerActorTextGridScratch* grid;
    u32                          firstRowColorRgb;

    cursorSlot                                               = SCRATCH_HEAD_ADDR;
    firstRowColorRgb                                         = PLAYER_ACTOR_TEXT_GRID_FIRST_ROW_RGB;
    previousCursor                                           = SCRATCH_HEAD_AT(cursorSlot, _PlayerActorTextGridScratch);
    block                                                    = previousCursor - 1;
    SCRATCH_HEAD_AT(cursorSlot, _PlayerActorTextGridScratch) = block;
    grid                                                     = block;
    grid->colorRgb                                           = firstRowColorRgb;
    grid->penY                                               = PLAYER_ACTOR_TEXT_GRID_FIRST_ROW_Y;
    for (grid->row = 0; grid->row < PLAYER_ACTOR_TEXT_GRID_ROW_COUNT; grid->row++) {
        grid->column = 0;
        grid->penX   = -PLAYER_ACTOR_TEXT_GRID_COLUMN_PITCH;
        for (; grid->column < PLAYER_ACTOR_TEXT_GRID_COLUMN_COUNT; grid->column++) {
            grid->penX += PLAYER_ACTOR_TEXT_GRID_COLUMN_PITCH;
            grid->penY -= PLAYER_ACTOR_TEXT_GRID_CELL_Y_STEP;
        }
        grid->colorRgb = PLAYER_ACTOR_TEXT_GRID_SECOND_ROW_RGB;
        grid->penY     = PLAYER_ACTOR_TEXT_GRID_SECOND_ROW_Y;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_PlayerActorTextGridScratch);
}

/// Schedules player-task teardown for the next work-dispatch tick.
static void _playerActorWorkState2(Task* task)
{
    enum { PLAYER_ACTOR_WORK_TEARDOWN_STATE = 3 };
    task->state = PLAYER_ACTOR_WORK_TEARDOWN_STATE;
}

/// Releases the player task's attached tasks and collision bodies before killing it.
///
/// Clears its exit callback and the player registry slot first. Each present
/// child must be live; task teardown owns the work/model release after all five
/// embedded collision bodies have been unlinked. Callers must not reuse the work.
static void _playerActorTeardown(Task* task)
{
    GameActor* actor;
    Task*      childTask;

    actor                                       = task->work;
    task->exitCallback                          = NULL;
    gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER] = NULL;
    childTask                                   = actor->weaponEffectTask;
    if (childTask != NULL) {
        taskKill(childTask);
    }
    childTask = actor->equipmentTasks[0];
    if (childTask != NULL) {
        taskKill(childTask);
    }
    childTask = actor->equipmentTasks[1];
    if (childTask != NULL) {
        taskKill(childTask);
    }
    childTask = actor->attachmentTasks[0];
    if (childTask != NULL) {
        taskKill(childTask);
    }
    childTask = actor->attachmentTasks[1];
    if (childTask != NULL) {
        taskKill(childTask);
    }
    worldCollisionUnlinkBody(&actor->collisionBodies[GAME_ACTOR_BODY_ROOT]);
    worldCollisionUnlinkBody(&actor->collisionBodies[GAME_ACTOR_BODY_PART4]);
    worldCollisionUnlinkBody(&actor->collisionBodies[GAME_ACTOR_BODY_PART1]);
    worldCollisionUnlinkBody(&actor->collisionBodies[GAME_ACTOR_BODY_WEAPON]);
    worldCollisionUnlinkBody(&actor->collisionBodies[GAME_ACTOR_BODY_AIM]);
    taskKill(task);
}

void playerActorWorkTask(Task* task)
{
    TaskFuncTable4 handlers;

    handlers = Gp_PlayerWorkStates;
    handlers.funcs[task->state](task);
}

/// Captures the session's logical pad input and preserves the actor's previous input.
///
/// `task->work` must be a live player `GameActor`. Captures movement/turn signs,
/// held buttons and the run button before deriving press and release edges.
/// The logical Cross bit is the run input, independent of physical pad mapping.
static inline void _playerActorCapturePad(Task* task)
{
    enum { PLAYER_ACTOR_RUN_BUTTON_SHIFT = 6 };
    GameActor* actor;
    u16        heldButtons;

    actor                        = task->work;
    actor->previousMovementSign  = actor->movementSign;
    actor->previousTurnSign      = actor->turnSign;
    actor->previousPadHeld       = actor->padHeld;
    heldButtons                  = gGameSession->padHeld;
    actor->previousRunButtonHeld = actor->runButtonHeld;
    actor->padHeld               = heldButtons;
    actor->padPressed            = actor->padHeld & ~actor->previousPadHeld;
    actor->padReleased           = actor->previousPadHeld & ~actor->padHeld;
    actor->runButtonHeld         = (actor->padHeld >> PLAYER_ACTOR_RUN_BUTTON_SHIFT) & 1;
}

/// Selects the player's collision heading in composed view space.
///
/// A nonzero override selects the normalized push-back vector; otherwise uses
/// the root's Q12 Z column times movementSign (-1 backward, 0 stopped, 1 forward).
/// Does not normalize the matrix column. Borrows live actor/root state and
/// writable scratch; writes XYZ only, narrowing each result to its signed low
/// halfword. The root cache and push-back vector must use the same view frame.
static inline void _playerActorStageCollisionHeading(SVECTOR* motionDirection, const GameActor* actor, const GfxCoord* coord)
{
    if (actor->usesPushbackDirection != 0) {
        motionDirection->vx = actor->pushbackDirection.vx;
        motionDirection->vy = actor->pushbackDirection.vy;
        motionDirection->vz = actor->pushbackDirection.vz;
    } else {
        motionDirection->vx = coord->workm.m[0][2] * actor->movementSign;
        motionDirection->vy = coord->workm.m[1][2] * actor->movementSign;
        motionDirection->vz = coord->workm.m[2][2] * actor->movementSign;
    }
}

/// Copies one view-space Q12 heading to the player's three motion-sphere contexts.
///
/// Requires live actor work and readable XYZ. Copies the root, part-4 and part-1
/// headings without normalization; each SVECTOR pad and contact pointer is intact.
static inline void _playerActorPublishCollisionHeading(GameActor* actor, const SVECTOR* motionDirection)
{
    actor->collisionMotionContexts[GAME_ACTOR_BODY_ROOT].motionDirection.vx  = motionDirection->vx;
    actor->collisionMotionContexts[GAME_ACTOR_BODY_ROOT].motionDirection.vy  = motionDirection->vy;
    actor->collisionMotionContexts[GAME_ACTOR_BODY_ROOT].motionDirection.vz  = motionDirection->vz;
    actor->collisionMotionContexts[GAME_ACTOR_BODY_PART4].motionDirection.vx = motionDirection->vx;
    actor->collisionMotionContexts[GAME_ACTOR_BODY_PART4].motionDirection.vy = motionDirection->vy;
    actor->collisionMotionContexts[GAME_ACTOR_BODY_PART4].motionDirection.vz = motionDirection->vz;
    actor->collisionMotionContexts[GAME_ACTOR_BODY_PART1].motionDirection.vx = motionDirection->vx;
    actor->collisionMotionContexts[GAME_ACTOR_BODY_PART1].motionDirection.vy = motionDirection->vy;
    actor->collisionMotionContexts[GAME_ACTOR_BODY_PART1].motionDirection.vz = motionDirection->vz;
}

void playerActorUpdateMove(void)
{
    enum {
        PLAYER_ACTOR_MOVE_ROOT_GRID_ENABLED = 1 << GAME_ACTOR_BODY_ROOT,
        PLAYER_ACTOR_MOVE_COLLISION_Y_BIAS  = 128,
        PLAYER_ACTOR_WEAPON_COLLISION_PITCH = -ACTOR_TRANSFORM_ANGLE_TURN / 4,
        PLAYER_ACTOR_WEAPON_COLLISION_YAW   = -32,
    };
    Task*      playerTask;
    GameActor* actor;
    GfxCoord*  coord;
    SVECTOR*   motionDirection;
    Task*      weaponTask;
    MATRIX*    weaponMatrix;

    playerTask      = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    actor           = playerTask->work;
    motionDirection = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    coord           = playerTask->extra.tmd->coords;
    _playerActorCapturePad(playerTask);
    gSceneCombatState.signals.bytes.actionFlags = 0;
    // Holding the state tick leaves input capture and collision preparation active.
    if (D_80115768 == 0) {
        _playerActorTick(playerTask);
    }
    coord->coord.t[0]            += actor->pendingDisplacement.vx;
    coord->coord.t[1]            += actor->pendingDisplacement.vy;
    coord->coord.t[2]            += actor->pendingDisplacement.vz;
    actor->pendingDisplacement.vx = 0;
    actor->pendingDisplacement.vy = 0;
    actor->pendingDisplacement.vz = 0;
    worldCollisionClearContacts(actor->collisionContacts);
    if (actor->equipmentTasks[1] != NULL) {
        worldCollisionClearContacts(actor->weaponContacts);
    }
    if (actor->collisionEnableMask & PLAYER_ACTOR_MOVE_ROOT_GRID_ENABLED) {
        coord->coord.t[1] += PLAYER_ACTOR_MOVE_COLLISION_Y_BIAS;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    // All three motion spheres use one Q12 heading in the composition frame.
    _playerActorStageCollisionHeading(motionDirection, actor, coord);
    weaponTask = actor->equipmentTasks[1];
    _playerActorPublishCollisionHeading(actor, motionDirection);
    if (weaponTask != NULL) {
        coord                       = weaponTask->extra.tmd->coords;
        actor->weaponCollisionCoord = *coord;
        weaponMatrix                = &actor->weaponCollisionCoord.workm;
        if (gPlayerStatus.weapon != PLAYER_ACTOR_WEAPON_GUNBLADE) {
            gfxRotMatrixX(weaponMatrix, PLAYER_ACTOR_WEAPON_COLLISION_PITCH, GRAPHICS_ROTATION_COMPOSE);
            gfxRotMatrixY(weaponMatrix, PLAYER_ACTOR_WEAPON_COLLISION_YAW, GRAPHICS_ROTATION_COMPOSE);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

/// Blends the active child slots into an aim-holding clip at the actor's playback rate.
///
/// `slotIndex` starts a suffix of the live child slots (at least 1); the active
/// prefix must fit initialized slot and pose storage. The set must provide each
/// slot's track; `setIndex` narrows to u16 for the lookup. `blendFrames` counts
/// whole normal-rate frames, 0..2047. Capture
/// uses each slot's previous rate before replacing it with the actor's rate.
/// Model, pose and borrowed clip lifetimes follow `animationPlaySlotWithBlend`.
static inline void _playerActorBlendAimHoldSlots(Task* task, s32 slotIndex, s32 setIndex, s32 blendFrames)
{
    GameActor* actor;

    actor = task->work;
    if (slotIndex < actor->animationSlotCount) {
        do {
            animationPlaySlotWithBlend(&actor->animationContext, slotIndex, NULL, setIndex, 0, 0, blendFrames, actor->animationSets);
            actor->animationSlots[slotIndex].rate = actor->animationRate;
            slotIndex++;
        } while (slotIndex < actor->animationSlotCount);
    }
}

void playerActorTickAnimationState(Task* task)
{
    enum {
        PLAYER_ACTOR_ANIMATION_AIM_ENTRY                      = 2,
        PLAYER_ACTOR_ANIMATION_RETURN_TO_LOCOMOTION           = 4,
        PLAYER_ACTOR_ANIMATION_RETURN_TO_AIM                  = 5,
        PLAYER_ACTOR_ANIMATION_RETURN_TO_LOCOMOTION_ALTERNATE = 6,
        PLAYER_ACTOR_ANIMATION_ADVANCE_ACTION_PHASE           = 7,
        PLAYER_ACTOR_ANIMATION_BOUNDARY_RETURN_TO_LOCOMOTION  = 8,
        PLAYER_ACTOR_ANIMATION_PARASITE_ENERGY_PHASE          = 9,
        PLAYER_ACTOR_ANIMATION_MARK_ACTION_COMPLETE           = 10,
        PLAYER_ACTOR_ANIMATION_AIM_HOLD_SET                   = 9,
        PLAYER_ACTOR_ANIMATION_AIM_HOLD_BLEND_FRAMES          = 5,
        PLAYER_ACTOR_ANIMATION_RETURN_TO_AIM_BLEND_FRAMES     = 3,
        PLAYER_ACTOR_ANIMATION_PHASE_COMPLETE                 = 1000,
    };
    GameActor*             actor;
    const AnimationRecord* record;
    s32                    slotIndex;
    s32                    setIndex;
    s32                    blendFrames;
    u16                    flags;

    actor  = task->work;
    record = animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1);
    switch (actor->animationState) {
        case 0:
        case 1:
            break;
        case PLAYER_ACTOR_ANIMATION_AIM_ENTRY:
            if (actor->statePhase != 0) {
                break;
            }
            if (playerActorIsAnimationPlaying(task, 0, 0, 0) != 0) {
                break;
            }
            // Keep clip, blend and slot locals live across the phase update.
            setIndex           = PLAYER_ACTOR_ANIMATION_AIM_HOLD_SET;
            blendFrames        = PLAYER_ACTOR_ANIMATION_AIM_HOLD_BLEND_FRAMES;
            slotIndex          = 1;
            actor->statePhase += slotIndex;
            _playerActorBlendAimHoldSlots(task, slotIndex, setIndex, blendFrames);
            break;
        case 3:
            break;
        case PLAYER_ACTOR_ANIMATION_RETURN_TO_AIM:
            if (record != NULL) {
                if (playerActorIsSlotAdvancingLinearly(task, 1, 0, 0) == 0) {
                    _playerActorEnterAimLocomotion(task, PLAYER_ACTOR_ANIMATION_RETURN_TO_AIM_BLEND_FRAMES);
                }
            }
            break;
        case PLAYER_ACTOR_ANIMATION_RETURN_TO_LOCOMOTION:
        case PLAYER_ACTOR_ANIMATION_RETURN_TO_LOCOMOTION_ALTERNATE:
            if (record != NULL) {
                if (playerActorIsSlotAdvancingLinearly(task, 1, 0, 0) == 0) {
                    playerActorEnterLocomotion(task, 0);
                }
            }
            break;
        case PLAYER_ACTOR_ANIMATION_ADVANCE_ACTION_PHASE:
        case PLAYER_ACTOR_ANIMATION_PARASITE_ENERGY_PHASE:
            if (record != NULL) {
                if (playerActorIsSlotAdvancingLinearly(task, 1, 0, 0) == 0) {
                    actor->statePhase++;
                }
            }
            break;
        case PLAYER_ACTOR_ANIMATION_BOUNDARY_RETURN_TO_LOCOMOTION:
            if (record != NULL) {
                flags = actor->animationSlots[1].status.fields.flags;
                if ((flags & ANIMATION_SLOT_REACHED_BOUNDARY) || (flags & ANIMATION_SLOT_FOLLOWED_JUMP)) {
                    actor->statePhase++;
                    playerActorEnterLocomotion(task, 0);
                }
            }
            break;
        case PLAYER_ACTOR_ANIMATION_MARK_ACTION_COMPLETE:
            if (record != NULL) {
                if (playerActorIsSlotAdvancingLinearly(task, 1, 0, 0) == 0) {
                    actor->statePhase = PLAYER_ACTOR_ANIMATION_PHASE_COMPLETE;
                }
            }
            break;
    }
}

/// Sets horizontal root displacement from its normalized local forward axis.
///
/// Movement modes 1..7 select a nonzero speed-row divisor; `movementSign`
/// selects forward/backward/stopped direction. The SDK axis is Q12, while X/Z
/// velocity is game-coordinate units per tick and Y is cleared. The live root
/// must meet SDK normalization requirements. Borrows the caller's movement
/// scratch without retaining it or changing the root translation.
static inline void _playerActorSetForwardVelocity(GameActor* actor, GfxCoord* rootCoord, _PlayerActorMoveStepScratch* scratch)
{
    scratch->speedDivisor = D_80112E10[(u16)actor->movementMode];
    gfxReadMatrixZAxis(&rootCoord->coord, &scratch->direction);
    VectorNormalSS(&scratch->direction, &scratch->direction);
    actor->velocity.vx = scratch->direction.vx * actor->movementSign / scratch->speedDivisor;
    actor->velocity.vy = 0;
    actor->velocity.vz = scratch->direction.vz * actor->movementSign / scratch->speedDivisor;
}

void playerActorStepMovement(Task* task)
{
    enum {
        PLAYER_ACTOR_MOVEMENT_STOPPED       = 0,
        PLAYER_ACTOR_MOVEMENT_FORWARD_WALK  = 1,
        PLAYER_ACTOR_MOVEMENT_BACKWARD_WALK = 2,
        PLAYER_ACTOR_MOVEMENT_RUN           = 3,
        PLAYER_ACTOR_MOVEMENT_CIRCLING      = 4,
        PLAYER_ACTOR_CIRCLE_HALF_TURN       = ACTOR_TRANSFORM_ANGLE_HALF_TURN,
        // 4096 * 1600 / (628 * radius) approximates a 16-unit arc in angle units.
        PLAYER_ACTOR_CIRCLE_ARC_NUMERATOR       = 0x640000,
        PLAYER_ACTOR_CIRCLE_CIRCUMFERENCE_SCALE = 0x274,
    };
    GameActor*                   actor;
    GfxCoord*                    coord;
    _PlayerActorMoveStepScratch* block;

    block = SCRATCH_STACK_RESERVE_BLOCK(_PlayerActorMoveStepScratch);
    actor = task->work;
    coord = task->extra.tmd->coords;
    switch ((u16)actor->movementMode) {
        case PLAYER_ACTOR_MOVEMENT_STOPPED:
            actor->velocity.vx = 0;
            actor->velocity.vy = 0;
            actor->velocity.vz = 0;
            break;
        case PLAYER_ACTOR_MOVEMENT_FORWARD_WALK:
        case PLAYER_ACTOR_MOVEMENT_BACKWARD_WALK:
        case PLAYER_ACTOR_MOVEMENT_RUN:
        case 5:
        case 6:
        case 7:
            if (animationGetCurrentRecord(&actor->animationContext,
                                          actor->animationSlots + 1) == NULL) {
                actor->velocity.vx = 0;
                actor->velocity.vy = 0;
                actor->velocity.vz = 0;
            } else {
                _playerActorSetForwardVelocity(actor, coord, block);
            }
            break;
        case PLAYER_ACTOR_MOVEMENT_CIRCLING:
            _playerActorSetForwardVelocity(actor, coord, block);
            coord->coord.t[0] += actor->velocity.vx;
            coord->coord.t[1] += actor->velocity.vy;
            coord->coord.t[2] += actor->velocity.vz;
            // Circle after applying forward motion, then restore the root orientation.
            block->savedMatrix  = coord->coord;
            block->speedDivisor = D_80112E10[(u16)actor->movementMode];
            worldTargetGetBodyPosition(actor->targetNode, &block->targetPosition);
            block->direction.vx  = abs(coord->coord.t[0] - block->targetPosition.vx);
            block->direction.vx += abs(coord->coord.t[2] - block->targetPosition.vz);
            block->strafeYaw     = PLAYER_ACTOR_CIRCLE_ARC_NUMERATOR;
            block->strafeYaw     = (PLAYER_ACTOR_CIRCLE_HALF_TURN - block->strafeYaw / (block->direction.vx * PLAYER_ACTOR_CIRCLE_CIRCUMFERENCE_SCALE)) >> 1;
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

/// Invalidates a model part's cached composition and returns its local matrix.
///
/// `partIndex` must address a live coordinate of the task's model. The returned
/// matrix is borrowed for immediate rotation updates; this helper does not
/// rebuild it or compose the node.
static inline MATRIX* _playerActorInvalidatePartMatrix(Task* task, s32 partIndex)
{
    GfxCoord* coord = &task->extra.tmd->coords[partIndex];

    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return &coord->coord;
}

void playerActorUpdateFacing(Task* task)
{
    GameActor* actor;
    GfxCoord*  coord;
    MATRIX*    partMatrix;
    s32        aimAnglesMoving;
    s16        decayStep;

    actor           = task->work;
    coord           = task->extra.tmd->coords;
    aimAnglesMoving = 0;
    if (actor->turnRateIndex != 0) {
        s32 turnSign = (u8) * (volatile s8*)&actor->turnSign;

        actor->rotation.vy = (actor->rotation.vy + D_80112E20[actor->turnRateIndex] * (s8)turnSign) & ACTOR_TRANSFORM_ANGLE_MASK;
    }
    RotMatrix(&actor->rotation, &coord->coord);
    MatrixNormal(&coord->coord, &coord->coord);
    // Ease residual aim offsets back to the ordinary pose before composing the parts.
    if (actor->aimTrackingState == GAME_ACTOR_AIM_TRACKING_DECAY) {
        GP_DECAY_ANGLE(actor->part2Pitch, decayStep, aimAnglesMoving);
        GP_DECAY_ANGLE(actor->part2Roll, decayStep, aimAnglesMoving);
        GP_DECAY_ANGLE(actor->part3Pitch, decayStep, aimAnglesMoving);
        GP_DECAY_ANGLE(actor->part3Roll, decayStep, aimAnglesMoving);
        GP_DECAY_ANGLE(actor->part6Pitch, decayStep, aimAnglesMoving);
        if (aimAnglesMoving == 0) {
            actor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_OFF;
        }
    }
    partMatrix = _playerActorInvalidatePartMatrix(task, 2);
    RotMatrixX(actor->part2Pitch, partMatrix);
    RotMatrixZ(actor->part2Roll, partMatrix);
    MatrixNormal(partMatrix, partMatrix);
    partMatrix = _playerActorInvalidatePartMatrix(task, 3);
    RotMatrixX(actor->part3Pitch, partMatrix);
    RotMatrixZ(actor->part3Roll, partMatrix);
    MatrixNormal(partMatrix, partMatrix);
    partMatrix = _playerActorInvalidatePartMatrix(task, 4);
    gfxRotMatrixY(partMatrix, actor->aimYaw, 0);
    MatrixNormal(partMatrix, partMatrix);
    partMatrix = _playerActorInvalidatePartMatrix(task, 6);
    gfxRotMatrixX(partMatrix, actor->part6Pitch, GRAPHICS_ROTATION_COMPOSE);
    MatrixNormal(partMatrix, partMatrix);
}

/// Returns the shortest signed turn between two angles, in 4096 units per turn.
///
/// Current yaw is wrapped to 0..4095; the target may use that range or the
/// signed heading range -2048..2047. Compares target-minus-current and its
/// one-turn neighbours, returning -2048..2048; half-turn ties take the wrapped
/// candidate. Reserves and releases 12 scratch-stack bytes. Candidate arithmetic
/// is s32 and the selected result narrows to s16.
static inline s16 _playerActorShortestTurn(s16 currentAngle, s16 targetAngle)
{
    _PlayerActorShortestTurnScratch* candidates;

    SCRATCH_STACK_RESERVE_BLOCK(_PlayerActorShortestTurnScratch);
    candidates            = SCRATCH_STACK_CURSOR(_PlayerActorShortestTurnScratch);
    candidates->direct    = targetAngle - currentAngle;
    candidates->plusTurn  = candidates->direct + ACTOR_TRANSFORM_ANGLE_TURN;
    candidates->minusTurn = candidates->direct - ACTOR_TRANSFORM_ANGLE_TURN;
    if (ABS(candidates->direct) < ABS(candidates->plusTurn) && ABS(candidates->direct) < ABS(candidates->minusTurn)) {
        currentAngle = candidates->direct;
    } else if (ABS(candidates->plusTurn) < ABS(candidates->minusTurn)) {
        currentAngle = candidates->plusTurn;
    } else {
        currentAngle = candidates->minusTurn;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_PlayerActorShortestTurnScratch);
    return currentAngle;
}

/// Returns the planar length of signed XZ components in game-coordinate units.
///
/// Absolute values, squares and their sum must fit s32. Uses `SquareRoot0`
/// without scale conversion or overflow checks; retains no pointers.
static inline s32 _playerActorAimPlanarLength(s32 x, s32 z)
{
    x = ABS(x);
    x = x * x;
    z = ABS(z);
    z = z * z;
    return SquareRoot0(x + z);
}

/// Places the yaw aim origin from a borrowed local weapon offset.
///
/// Copies XYZ to the caller's yaw scratch before placing its coordinate node.
/// XYZ are signed local game-coordinate units; the offset's fourth halfword is
/// neither read nor copied. The caller owns a reserved scratch block and keeps
/// both source pointers readable for this call. The source coordinate's cache
/// is updated. Coordinate hierarchy and GTE requirements follow
/// `actorRenderPlaceCoordOffset`; no source pointer is retained.
static inline void _playerActorPlaceAimYawOrigin(_PlayerActorAimYawScratch* scratch, GfxCoord* weaponCoord, const SVECTOR* localOffset)
{
    scratch->originOffset.vx = localOffset->vx;
    scratch->originOffset.vy = localOffset->vy;
    scratch->originOffset.vz = localOffset->vz;
    actorRenderPlaceCoordOffset(weaponCoord, &scratch->originCoord, &scratch->originOffset);
}

/// Turns body yaw toward a live lock beyond a signed ground-distance threshold.
///
/// Borrows the caller's yaw scratch and the actor/weapon coordinates; no pointer
/// is retained. Distances use game units and angles use 4096 units per turn.
/// Requires a valid weapon row and initialized GTE/scratch state. The planar
/// absolute values, squares and sum must fit s32. A missing target is a no-op.
static inline void _playerActorAimYawAtLock(GameActor* actor, _PlayerActorAimYawScratch* scratch, s16 minGroundDistance)
{
    SVECTOR*  localOffset;
    GfxCoord* weaponCoord;
    VECTOR3*  targetDelta;
    s32       turnLimit;

    if (actor->targetNode != NULL) {
        localOffset = &D_801131B4[gPlayerStatus.weapon];
        weaponCoord = actor->equipmentTasks[1]->extra.tmd->coords;
        _playerActorPlaceAimYawOrigin(scratch, weaponCoord, localOffset);
        targetDelta = &scratch->targetDelta;
        worldTargetGetBodyPosition(actor->targetNode, targetDelta);
        targetDelta->vx -= scratch->originCoord.coord.t[0];
        targetDelta->vy -= scratch->originCoord.coord.t[1];
        targetDelta->vz -= scratch->originCoord.coord.t[2];
        if (_playerActorAimPlanarLength(scratch->targetDelta.vx, scratch->targetDelta.vz) > minGroundDistance) {
            scratch->yaw = ratan2(scratch->targetDelta.vx, scratch->targetDelta.vz);
            scratch->yaw = _playerActorShortestTurn(actor->rotation.vy, scratch->yaw);
            turnLimit    = (s16)D_80112E30[gPlayerStatus.weapon];
            if (equipmentHasEffect(EQUIPMENT_EFFECT_QUICK_FIRE) != 0) {
                turnLimit += turnLimit >> 1;
            }
            if (scratch->yaw > turnLimit) {
                scratch->yaw = turnLimit;
            } else if (scratch->yaw < -turnLimit) {
                scratch->yaw = -turnLimit;
            }
            actor->rotation.vy = (actor->rotation.vy + scratch->yaw) & ACTOR_TRANSFORM_ANGLE_MASK;
        }
    }
}

void playerActorAimYawToLock(Task* task, s32 minGroundDistance)
{
    GameActor*                 actor;
    _PlayerActorAimYawScratch* scratch;

    actor   = task->work;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_PlayerActorAimYawScratch);
    _playerActorAimYawAtLock(actor, scratch, minGroundDistance);
    SCRATCH_STACK_RELEASE_BLOCK(_PlayerActorAimYawScratch);
}

/// Stages a local weapon offset and places the pitch-aim origin at that point.
///
/// Copies only XYZ, in signed game-coordinate units, into the caller's live
/// scratch block. Source, view and coordinate lifetime/cache requirements are
/// those of `actorRenderPlaceCoordOffset`; the offset is read only for this call.
static inline void _playerActorPlaceAimPitchOrigin(_PlayerActorAimPitchScratch* scratch, GfxCoord* source, const SVECTOR* localOffset)
{
    scratch->originOffset.vx = localOffset->vx;
    scratch->originOffset.vy = localOffset->vy;
    scratch->originOffset.vz = localOffset->vz;
    actorRenderPlaceCoordOffset(source, &scratch->originCoord, &scratch->originOffset);
}

/// Stages the lock target's position and aim-origin displacement, returning its planar length.
///
/// Requires a live borrowed target and a placed origin in the caller's scratch
/// block, in the target position's coordinate frame. Writes XYZ position and
/// delta; ignores Y for the returned game-unit length. Absolute values, squares
/// and their sum must fit s32. Retains no pointers; the caller owns the scratch.
static inline s32 _playerActorGetAimPitchTargetDelta(const GameActor* actor, _PlayerActorAimPitchScratch* scratch)
{
    VECTOR3* targetPosition;
    VECTOR3* targetDelta;

    targetPosition = &scratch->targetPosition;
    worldTargetGetBodyPosition(actor->targetNode, targetPosition);
    targetDelta     = &scratch->targetDelta;
    targetDelta->vx = targetPosition->vx - scratch->originCoord.coord.t[0];
    targetDelta->vy = targetPosition->vy - scratch->originCoord.coord.t[1];
    targetDelta->vz = targetPosition->vz - scratch->originCoord.coord.t[2];
    return _playerActorAimPlanarLength(scratch->targetDelta.vx, scratch->targetDelta.vz);
}

void playerActorAimPitchToLock(Task* task)
{
    enum {
        PLAYER_ACTOR_LOCK_PITCH_STEP        = 0x30,
        PLAYER_ACTOR_LOCK_PART2_PITCH_LIMIT = 0x120,
        PLAYER_ACTOR_LOCK_PART3_PITCH_LIMIT = 0x100,
        PLAYER_ACTOR_LOCK_PART2_ORIGIN_Y    = -0x400,
    };
    GameActor*                   actor;
    _PlayerActorAimPitchScratch* scratch;
    GfxCoord*                    modelCoords;

/// Clamps and applies a lock-pitch step, deriving roll for an accepted angle.
///
/// Arguments must be stable, side-effect-free lvalues/values: scratch and partPitch
/// are evaluated repeatedly. Uses this function's PLAYER_ACTOR_LOCK_PITCH_STEP.
#define PLAYER_ACTOR_APPLY_LOCK_PITCH(scratch, partPitch, partRoll, pitchLimit, rollWeight) \
    do {                                                                                    \
        if ((scratch)->pitch > PLAYER_ACTOR_LOCK_PITCH_STEP) {                              \
            (scratch)->pitch = PLAYER_ACTOR_LOCK_PITCH_STEP;                                \
        } else if ((scratch)->pitch < -PLAYER_ACTOR_LOCK_PITCH_STEP) {                      \
            (scratch)->pitch = -PLAYER_ACTOR_LOCK_PITCH_STEP;                               \
        }                                                                                   \
        if (ABS((partPitch) + (scratch)->pitch) <= (pitchLimit)) {                          \
            (partPitch) += (scratch)->pitch;                                                \
            (partRoll)   = ((partPitch) / 5) * (rollWeight);                                \
        }                                                                                   \
    } while (0)

    actor   = task->work;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_PlayerActorAimPitchScratch);
    if (actor->targetNode != NULL) {
        // Measure torso elevation above part 2, then from the weapon aim origin.
        modelCoords              = task->extra.tmd->coords;
        scratch->originOffset.vx = 0;
        scratch->originOffset.vy = PLAYER_ACTOR_LOCK_PART2_ORIGIN_Y;
        scratch->originOffset.vz = 0;
        actorRenderPlaceCoordOffset(&modelCoords[2], &scratch->originCoord, &scratch->originOffset);
        scratch->groundDistance   = _playerActorGetAimPitchTargetDelta(actor, scratch);
        scratch->targetDelta.vy >>= 1;
        scratch->pitch            = ratan2(-scratch->targetDelta.vy, scratch->groundDistance) / 7 * 4;
        scratch->pitch           -= actor->part2Pitch;
        PLAYER_ACTOR_APPLY_LOCK_PITCH(scratch, actor->part2Pitch, actor->part2Roll, PLAYER_ACTOR_LOCK_PART2_PITCH_LIMIT, 3);

        _playerActorPlaceAimPitchOrigin(scratch, actor->equipmentTasks[1]->extra.tmd->coords, &D_801131B4[gPlayerStatus.weapon]);
        scratch->groundDistance = _playerActorGetAimPitchTargetDelta(actor, scratch);
        scratch->pitch          = ratan2(-scratch->targetDelta.vy, scratch->groundDistance) / 7 * 4;
        scratch->pitch         -= actor->part3Pitch;
        PLAYER_ACTOR_APPLY_LOCK_PITCH(scratch, actor->part3Pitch, actor->part3Roll, PLAYER_ACTOR_LOCK_PART3_PITCH_LIMIT, 2);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_PlayerActorAimPitchScratch);
}
#undef PLAYER_ACTOR_APPLY_LOCK_PITCH

/// Eases Gunblade lock elevation into the model's part-2 and part-3 roll angles.
///
/// Angles use 4096 units per turn, stepping by at most 48 and bounded to
/// 288/256 units. Part 2 measures above its origin with vertical delta halved;
/// part 3 measures from the equipped weapon offset. Both elevations use the
/// truncated 4/7 response. A missing lock is a no-op.
/// Requires live model part 2, equipped model 1 and target, a valid weapon row,
/// scratch/GTE state and arithmetic meeting the pitch-target helper contract.
static void _playerActorAimRollToLock(Task* task)
{
    enum {
        PLAYER_ACTOR_LOCK_PART2_ROLL_LIMIT = 0x120,
        PLAYER_ACTOR_LOCK_PART3_ROLL_LIMIT = 0x100,
        PLAYER_ACTOR_LOCK_ROLL_ORIGIN_Y    = -0x400,
    };
    GameActor*                   actor;
    _PlayerActorAimPitchScratch* scratch;
    GfxCoord*                    modelCoords;

/// Clamps a lock-elevation delta and applies it within a joint's absolute limit.
///
/// Arguments must be stable, side-effect-free values/lvalues; scratch and
/// jointAngle are evaluated repeatedly. The joint is a signed halfword,
/// the delta is s32 and their sum must fit s32. Angles use 4096 units per turn;
/// uses the source-local PLAYER_ACTOR_LOCK_ELEVATION_STEP constant.
#define PLAYER_ACTOR_APPLY_LOCK_ELEVATION_STEP(scratch, jointAngle, angleLimit) \
    do {                                                                        \
        if ((scratch)->pitch > PLAYER_ACTOR_LOCK_ELEVATION_STEP) {              \
            (scratch)->pitch = PLAYER_ACTOR_LOCK_ELEVATION_STEP;                \
        } else if ((scratch)->pitch < -PLAYER_ACTOR_LOCK_ELEVATION_STEP) {      \
            (scratch)->pitch = -PLAYER_ACTOR_LOCK_ELEVATION_STEP;               \
        }                                                                       \
        if (ABS((jointAngle) + (scratch)->pitch) <= (angleLimit)) {             \
            (jointAngle) += (scratch)->pitch;                                   \
        }                                                                       \
    } while (0)

    actor   = task->work;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_PlayerActorAimPitchScratch);
    if (actor->targetNode != NULL) {
        modelCoords              = task->extra.tmd->coords;
        scratch->originOffset.vx = 0;
        scratch->originOffset.vy = PLAYER_ACTOR_LOCK_ROLL_ORIGIN_Y;
        scratch->originOffset.vz = 0;
        actorRenderPlaceCoordOffset(&modelCoords[2], &scratch->originCoord, &scratch->originOffset);
        scratch->groundDistance   = _playerActorGetAimPitchTargetDelta(actor, scratch);
        scratch->targetDelta.vy >>= 1;
        scratch->pitch            = ratan2(-scratch->targetDelta.vy, scratch->groundDistance) / 7 * 4;
        scratch->pitch           -= actor->part2Roll;
        PLAYER_ACTOR_APPLY_LOCK_ELEVATION_STEP(scratch, actor->part2Roll, PLAYER_ACTOR_LOCK_PART2_ROLL_LIMIT);

        _playerActorPlaceAimPitchOrigin(scratch, actor->equipmentTasks[1]->extra.tmd->coords, &D_801131B4[gPlayerStatus.weapon]);
        scratch->groundDistance = _playerActorGetAimPitchTargetDelta(actor, scratch);
        scratch->pitch          = ratan2(-scratch->targetDelta.vy, scratch->groundDistance) / 7 * 4;
        scratch->pitch         -= actor->part3Roll;
        PLAYER_ACTOR_APPLY_LOCK_ELEVATION_STEP(scratch, actor->part3Roll, PLAYER_ACTOR_LOCK_PART3_ROLL_LIMIT);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_PlayerActorAimPitchScratch);
}

void playerActorAimPart6PitchToLock(Task* task, s32 weaponId, s32 minGroundDistance)
{
    enum {
        PLAYER_ACTOR_LOCK_PART6_PITCH_LIMIT = 0x280,
        PLAYER_ACTOR_LOCK_PITCH_DEAD_ZONE   = 0x20,
    };
    GameActor*                   actor;
    _PlayerActorAimPitchScratch* scratch;

    actor   = task->work;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_PlayerActorAimPitchScratch);
    if (actor->targetNode != NULL) {
        _playerActorPlaceAimPitchOrigin(scratch, actor->equipmentTasks[1]->extra.tmd->coords, &D_801131B4[weaponId]);
        scratch->groundDistance = _playerActorGetAimPitchTargetDelta(actor, scratch);
        if (scratch->groundDistance > (s16)minGroundDistance) {
            scratch->pitch  = ratan2(-scratch->targetDelta.vy, scratch->groundDistance);
            scratch->pitch -= actor->part6Pitch;
            if (ABS(scratch->pitch) >= PLAYER_ACTOR_LOCK_PITCH_DEAD_ZONE) {
                PLAYER_ACTOR_APPLY_LOCK_ELEVATION_STEP(scratch, actor->part6Pitch, PLAYER_ACTOR_LOCK_PART6_PITCH_LIMIT);
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_PlayerActorAimPitchScratch);
}

/// Eases independently tracked pitch toward the lock from the weapon's root origin.
///
/// Uses a zero local offset. Deltas below 32 angle units are ignored; changes
/// are limited to 48 units per call and absolute pitch to 640, in 4096 units
/// per turn. A missing target changes nothing. Requires live actor work,
/// equipped model 1 and a borrowed target in the world frame beneath the view.
/// Scratch/GTE state and planar arithmetic must meet the pitch helpers' contracts.
static void _playerActorAimDirectPitchToLock(Task* task)
{
    enum {
        PLAYER_ACTOR_DIRECT_PITCH_DEAD_ZONE = 0x20,
        PLAYER_ACTOR_DIRECT_PITCH_LIMIT     = 0x280,
    };
    GameActor*                   actor;
    _PlayerActorAimPitchScratch* scratch;
    GfxCoord*                    weaponCoords;

    actor   = task->work;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_PlayerActorAimPitchScratch);
    if (actor->targetNode != NULL) {
        weaponCoords             = actor->equipmentTasks[1]->extra.tmd->coords;
        scratch->originOffset.vx = 0;
        scratch->originOffset.vy = 0;
        scratch->originOffset.vz = 0;
        actorRenderPlaceCoordOffset(weaponCoords, &scratch->originCoord, &scratch->originOffset);
        scratch->groundDistance = _playerActorGetAimPitchTargetDelta(actor, scratch);
        scratch->pitch          = ratan2(-scratch->targetDelta.vy, scratch->groundDistance);
        scratch->pitch         -= actor->directAimPitch;
        if (ABS(scratch->pitch) >= PLAYER_ACTOR_DIRECT_PITCH_DEAD_ZONE) {
            PLAYER_ACTOR_APPLY_LOCK_ELEVATION_STEP(scratch, actor->directAimPitch, PLAYER_ACTOR_DIRECT_PITCH_LIMIT);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_PlayerActorAimPitchScratch);
}
#undef PLAYER_ACTOR_APPLY_LOCK_ELEVATION_STEP

/// Queues a texture frame in a rectangle relative to the player model's texture page.
///
/// Requires a live task and TMD; borrows writable scratch rectangle storage.
/// xHalfWords counts half VRAM words, widthWords counts 16-bit words, and
/// yRows/heightRows count
/// rows. The translated destination must fit VRAM and the first frame upload's
/// pixels must cover that full rectangle. A present upload list is writable,
/// terminated and remains live through this call; its word-aligned pixels stay
/// live until GPU transfer completes. NULL fills the rectangle without uploading.
static inline void _playerActorUploadTextureFrame(Task* task, GpuImageUpload* frameUploads, RECT* textureRect,
                                                  s16 xHalfWords, s16 yRows, s16 widthWords, s16 heightRows)
{
    textureRect->x = xHalfWords;
    textureRect->y = yRows;
    textureRect->w = widthWords;
    textureRect->h = heightRows;
    actorRenderUploadTexture(task, frameUploads, textureRect);
}

/// Advances the player's two independent, null-terminated texture sequences.
///
/// Nonzero selectors choose A rows 1..4 or B rows 1..2 for resourceVariant
/// 1..4; frame indices must reach a NULL entry before signed-byte wrap.
/// Delays count actor ticks and reload to four/eight after each queued frame.
/// Reads selectors, delays and frame indices as s8 despite their byte storage.
/// Requires live actor/model, writable loaded upload lists and scratch space
/// for one RECT. Pixels remain live until their queued GPU transfers finish.
static void _playerActorTickTextureSequences(Task* task)
{
    enum {
        PLAYER_ACTOR_TEXTURE_VARIANT_COUNT = 4,
        PLAYER_ACTOR_TEXTURE_INDEX_BIAS    = PLAYER_ACTOR_TEXTURE_VARIANT_COUNT + 1,
        PLAYER_ACTOR_TEXTURE_SEQUENCE_OFF  = 0,
        PLAYER_ACTOR_TEXTURE_A_DELAY_TICKS = 4,
        PLAYER_ACTOR_TEXTURE_B_DELAY_TICKS = 8,
        PLAYER_ACTOR_TEXTURE_A_X           = 0,
        PLAYER_ACTOR_TEXTURE_A_Y           = 78,
        PLAYER_ACTOR_TEXTURE_A_WIDTH_WORDS = 25,
        PLAYER_ACTOR_TEXTURE_A_HEIGHT_ROWS = 16,
        PLAYER_ACTOR_TEXTURE_B_X           = 12,
        PLAYER_ACTOR_TEXTURE_B_Y           = 104,
        PLAYER_ACTOR_TEXTURE_B_WIDTH_WORDS = 14,
        PLAYER_ACTOR_TEXTURE_B_HEIGHT_ROWS = 20,
    };
    RECT*           textureRect;
    GameActor*      actor;
    GpuImageUpload* frameUploads;

    actor       = task->work;
    textureRect = SCRATCH_STACK_RESERVE_BLOCK(RECT);

    // Each channel stops at its own terminator without changing the other.
    if ((s8)actor->textureSequenceA != PLAYER_ACTOR_TEXTURE_SEQUENCE_OFF) {
        actor->textureDelayA--;
        if ((s8)actor->textureDelayA <= 0) {
            frameUploads = D_80112E74[(s8)actor->textureSequenceA * PLAYER_ACTOR_TEXTURE_VARIANT_COUNT + (gPlayerStatus.resourceVariant - PLAYER_ACTOR_TEXTURE_INDEX_BIAS)][(s8)actor->textureFrameA];
            if (frameUploads != NULL) {
                _playerActorUploadTextureFrame(task, frameUploads, textureRect, PLAYER_ACTOR_TEXTURE_A_X, PLAYER_ACTOR_TEXTURE_A_Y,
                                               PLAYER_ACTOR_TEXTURE_A_WIDTH_WORDS, PLAYER_ACTOR_TEXTURE_A_HEIGHT_ROWS);
                actor->textureDelayA = PLAYER_ACTOR_TEXTURE_A_DELAY_TICKS;
                actor->textureFrameA++;
            } else {
                actor->textureSequenceA = PLAYER_ACTOR_TEXTURE_SEQUENCE_OFF;
            }
        }
    }

    if ((s8)actor->textureSequenceB != PLAYER_ACTOR_TEXTURE_SEQUENCE_OFF) {
        actor->textureDelayB--;
        if ((s8)actor->textureDelayB <= 0) {
            frameUploads = D_80112EB4[(s8)actor->textureSequenceB * PLAYER_ACTOR_TEXTURE_VARIANT_COUNT + (gPlayerStatus.resourceVariant - PLAYER_ACTOR_TEXTURE_INDEX_BIAS)][(s8)actor->textureFrameB];
            if (frameUploads != NULL) {
                _playerActorUploadTextureFrame(task, frameUploads, textureRect, PLAYER_ACTOR_TEXTURE_B_X, PLAYER_ACTOR_TEXTURE_B_Y,
                                               PLAYER_ACTOR_TEXTURE_B_WIDTH_WORDS, PLAYER_ACTOR_TEXTURE_B_HEIGHT_ROWS);
                actor->textureDelayB = PLAYER_ACTOR_TEXTURE_B_DELAY_TICKS;
                actor->textureFrameB++;
            } else {
                actor->textureSequenceB = PLAYER_ACTOR_TEXTURE_SEQUENCE_OFF;
            }
        }
    }

    SCRATCH_STACK_RELEASE_BLOCK(RECT);
}

/// Selects player or companion attachment textures and rebuilds both primitive halves.
///
/// Player models use a six-page offset with no CLUT-row displacement; companion
/// models use four pages and six rows. Requires a live model source, coordinates
/// and allocated two-half primitive buffer, plus the builder's graphics/scratch
/// resources. Borrows actor/model storage and restores the next-half selector.
static inline void _playerActorBindAttachmentTextures(TmdObject* attachmentModel, const GameActor* actor)
{
    enum {
        PLAYER_ACTOR_ATTACHMENT_TEXTURE_PAGE_OFFSET           = 6,
        PLAYER_ACTOR_ATTACHMENT_CLUT_ROW_OFFSET               = 0,
        PLAYER_ACTOR_COMPANION_ATTACHMENT_TEXTURE_PAGE_OFFSET = 4,
        PLAYER_ACTOR_COMPANION_ATTACHMENT_CLUT_ROW_OFFSET     = 6,
    };

    if (actor->companionWork != NULL) {
        attachmentModel->texturePageOffset = PLAYER_ACTOR_COMPANION_ATTACHMENT_TEXTURE_PAGE_OFFSET;
        attachmentModel->clutRowOffset     = PLAYER_ACTOR_COMPANION_ATTACHMENT_CLUT_ROW_OFFSET;
    } else {
        attachmentModel->texturePageOffset = PLAYER_ACTOR_ATTACHMENT_TEXTURE_PAGE_OFFSET;
        attachmentModel->clutRowOffset     = PLAYER_ACTOR_ATTACHMENT_CLUT_ROW_OFFSET;
    }
    tmdBuildBufferHalf(attachmentModel);
    tmdBuildBufferHalf(attachmentModel);
}

/// Creates one attachment model anchor for the private pair-replacement path.
///
/// attachmentIndex and pairVariant are 0..1; rigIndex is 0..5 and its sum with
/// resourceVariant - 2 must index descriptor bases 0..7. The selected rig joint
/// and bank-7 descriptor/model must exist. Returns NULL on spawn failure;
/// otherwise the child borrows the actor's joint until teardown and uses its
/// texture bank. Caller owns the child and installs it in the attachment pair.
static inline Task* _playerActorSpawnAttachment(Task* actorTask, s32 attachmentIndex, s32 rigIndex, s32 pairVariant)
{
    enum {
        PLAYER_ACTOR_ATTACHMENT_BANK          = 7,
        PLAYER_ACTOR_ATTACHMENT_RESOURCE_BIAS = 2,
        PLAYER_ACTOR_ATTACHMENT_PAIR_SIZE     = 2,
    };
    Task*            task;
    const GameActor* actor;
    TmdObject*       actorModel;
    GfxCoord*        attachmentCoord;
    TmdObject*       attachmentModel;
    GfxCoord*        parentCoord;
    const u8*        descriptorBases;
    s32              resourceOffset;

    actorModel      = actorTask->extra.tmd;
    actor           = actorTask->work;
    parentCoord     = &actorModel->coords[D_80112E04[rigIndex][attachmentIndex]];
    descriptorBases = D_80112DFC;
    resourceOffset  = gPlayerStatus.resourceVariant - PLAYER_ACTOR_ATTACHMENT_RESOURCE_BIAS;
    task            = taskSpawn(PLAYER_ACTOR_ATTACHMENT_BANK, descriptorBases[rigIndex + resourceOffset] + pairVariant * PLAYER_ACTOR_ATTACHMENT_PAIR_SIZE + attachmentIndex, 0, 0);
    if (task == NULL) {
        return NULL;
    }
    // Keep the first update's draw flags and parent the anchor beneath its rig joint.
    task->parent                      = actorTask;
    attachmentCoord                   = task->extra.tmd->coords;
    attachmentCoord->parent           = parentCoord;
    attachmentCoord->param.clearFlags = false;
    attachmentModel                   = task->extra.tmd;
    _playerActorBindAttachmentTextures(attachmentModel, actor);
    return task;
}

/// Tears down one owned attachment anchor and installs its replacement, or NULL.
///
/// actor must be actorTask's live work; attachmentIndex and pairVariant are
/// 0..1, rigIndex is 0..5 and rigIndex + resourceVariant - 2 must be 0..7.
/// Requires loaded bank-7 descriptors and the selected rig coordinates.
/// Teardown precedes allocation, so failure leaves the slot empty. Successful
/// children borrow the rig joint until teardown; no old child is restored.
static inline void _playerActorReplaceAttachmentSlot(Task* actorTask, GameActor* actor, s32 attachmentIndex, s32 rigIndex, s32 pairVariant)
{
    Task* oldAttachment;

    oldAttachment = actor->attachmentTasks[attachmentIndex];
    if (oldAttachment != NULL) {
        taskKill(oldAttachment);
    }
    actor->attachmentTasks[attachmentIndex] = _playerActorSpawnAttachment(actorTask, attachmentIndex, rigIndex, pairVariant);
}

/// Replaces both actor attachment slots in order and returns the second new task.
///
/// Has no callers in this image. Requires live actor/model resources, rigIndex
/// 0..5, pairVariant 0..1, and rigIndex + resourceVariant - 2 in 0..7 with the
/// selected bank-7 descriptors loaded. Each old child is killed before its
/// replacement is attempted. A failed spawn leaves that slot NULL, without
/// rolling back the other slot; the result reports only the second spawn.
/// Successful children borrow rig coordinates until their parent is torn down.
static Task* _playerActorReplaceAttachments(Task* actorTask, s32 rigIndex, s32 pairVariant)
{
    GameActor* actor;

    actor = actorTask->work;
    _playerActorReplaceAttachmentSlot(actorTask, actor, 0, rigIndex, pairVariant);
    _playerActorReplaceAttachmentSlot(actorTask, actor, 1, rigIndex, pairVariant);
    return actor->attachmentTasks[1];
}

/// Spawns the selected character's weapon model as a child of an attachment task.
///
/// Character id 0..3 selects a bank-7 descriptor base; weapon ids 1..32
/// select its unchecked entry, which must be loaded. Zero weapon or allocation failure
/// returns NULL. Borrows the parent's model root for coordinate parenting,
/// sets the task's parent and requests local coordinate initialization.
/// Parent, descriptor code and model resources must stay live with the child.
inline static Task* _playerActorSpawnEquippedWeapon(Task* parent, s32 characterId, s32 weaponId)
{
    enum {
        PLAYER_ACTOR_WEAPON_MODEL_BANK = 7,
    };
    GfxCoord*  parentCoord;
    Task*      weaponTask;
    TmdObject* weaponModel;
    GfxCoord*  weaponCoord;
    s32        descriptorBase;

    parentCoord = parent->extra.tmd->coords;
    if (weaponId == PLAYER_STATUS_EQUIPMENT_NONE) {
        return NULL;
    }
    descriptorBase = D_80112DF4[characterId] - 1;
    weaponTask     = taskSpawn(PLAYER_ACTOR_WEAPON_MODEL_BANK, descriptorBase + weaponId, 0, 0);
    if (weaponTask == NULL) {
        return NULL;
    }
    weaponModel                   = weaponTask->extra.tmd;
    weaponTask->parent            = parent;
    weaponCoord                   = weaponModel->coords;
    weaponCoord->parent           = parentCoord;
    weaponCoord->param.clearFlags = true;
    return weaponTask;
}

Task* playerActorRestoreEquipment(void)
{
    enum {
        PLAYER_ACTOR_HYPERVELOCITY_PERSISTENT_EFFECT = EFFECT_SPAWN_UNLIMITED | EFFECT_ID(EFFECT_TASK_BANK, 0x24),
        PLAYER_ACTOR_HAMMER_PERSISTENT_EFFECT        = EFFECT_SPAWN_UNLIMITED | EFFECT_ID(EFFECT_TASK_BANK, 0x29),
        PLAYER_ACTOR_PYKE_PERSISTENT_EFFECT          = EFFECT_SPAWN_UNLIMITED | EFFECT_ID(EFFECT_TASK_BANK, 0x2A),
    };
    Task*         playerTask;
    GameActor*    actor;
    Task*         attachmentTask;
    Task*         weaponTask;
    PlayerStatus* playerStatus;
    s32           weaponId;
    s32           effectId;
    s32           effectArgument;
    TmdObject*    weaponModel;
    GfxCoord*     weaponCoord;
    EffectWork*   weaponEffect;
    GameActor*    playbackActor;
    TmdObject*    playerModel;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    actor      = playerTask->work;
    if (!playerTask | !actor) {
        return NULL;
    }

    attachmentTask = actor->attachmentTasks[1];
    if (attachmentTask != NULL) {
        weaponTask               = _playerActorSpawnEquippedWeapon(attachmentTask, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId, gPlayerStatus.weapon);
        actor->equipmentTasks[1] = weaponTask;
        if (weaponTask != NULL) {
            playerStatus = &gPlayerStatus;
            playerActorInitWeaponCollision(playerTask, playerStatus->weapon, playerStatus->weaponSlotItem);
            if (actor->weaponEffectTask == NULL) {
                weaponModel = actor->equipmentTasks[1]->extra.tmd;
                weaponId    = playerStatus->weapon;
                weaponCoord = weaponModel->coords;
                if (weaponId != PLAYER_ACTOR_WEAPON_HYPERVELOCITY) {
                    goto selectAttachmentEffect;
                }
                effectId       = PLAYER_ACTOR_HYPERVELOCITY_PERSISTENT_EFFECT;
                effectArgument = 0;
                goto spawnPersistentEffect;

            attachPersistentEffect:
                actor->weaponEffectTask = weaponEffect->task;
                taskReparent(playerTask, weaponEffect->task);
                playerActorResetWeaponAttack(playerTask, gPlayerStatus.weapon, 0);
                goto restorePlayback;

            selectAttachmentEffect:
                if (weaponId == PLAYER_ACTOR_WEAPON_HAMMER) {
                    effectId = PLAYER_ACTOR_HAMMER_PERSISTENT_EFFECT;
                } else {
                    if (weaponId != PLAYER_ACTOR_WEAPON_PYKE) {
                        goto restorePlayback;
                    }
                    effectId = PLAYER_ACTOR_PYKE_PERSISTENT_EFFECT;
                }
                effectArgument = playerStatus->weapon;
            spawnPersistentEffect:
                weaponEffect = effectSpawn(effectId, weaponCoord, effectArgument, 0);
                if (weaponEffect != NULL) {
                    goto attachPersistentEffect;
                }
            }
        }
    }

restorePlayback:
    // Restore the native animation bank even if model or effect allocation failed.
    actor->reloadEffectSuppressed     = 0;
    playbackActor                     = playerTask->work;
    playerModel                       = playerTask->extra.tmd;
    playbackActor->animationBankIndex = Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon;
    playbackActor->animationSets      = Gp_PlayerAnimBlkTbl[playbackActor->animationBankIndex]->table.sets;
    animationInitContext(&playbackActor->animationContext, playbackActor->animationSets, playerModel, playbackActor->poseBuffer,
                         playbackActor->animationSlots);
    playerActorEnterLocomotion(playerTask, 1);
    actor->pendingCollisionUpdates                      = GAME_ACTOR_COLLISION_REQUEST_MASK;
    actor->collisionBodies[GAME_ACTOR_BODY_ROOT].flags |= WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED;
    return actor->equipmentTasks[1];
}

Task* playerActorSpawn(const ActorSpawnTransform* spawnTransform, u16 unusedCharacterId, s32 spawnArg, ActorSpawnOptions* options)
{
    enum {
        PLAYER_ACTOR_SPAWN_BANK            = 7,
        PLAYER_ACTOR_SPAWN_DESCRIPTOR_BIAS = 3,
    };
    Task*      task;
    GameActor* actor;
    GfxCoord*  coord;

    task = taskSpawn(PLAYER_ACTOR_SPAWN_BANK, gPlayerStatus.resourceVariant + PLAYER_ACTOR_SPAWN_DESCRIPTOR_BIAS, spawnArg, options);
    if (task == NULL) {
        return NULL;
    }

    actor = memCalloc(sizeof(*actor), 0);
    if (actor == NULL) {
        taskKill(task);
        return NULL;
    }

    // Publish the allocated actor before initializing its deferred first task state.
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

/// Captures session buttons and preserves the actor's preceding input and motion signs.
///
/// Requires live GameActor work and session input already remapped/suppressed
/// for this tick. Press/release edges compare the full 16-bit masks; held
/// Cross supplies the logical run bit. Snapshots signs before movement derives
/// their new values. Borrows task and session storage only for this call.
static void _playerActorCapturePadState(Task* task)
{
    GameActor* actor;
    u16        heldButtons;

    actor                        = task->work;
    actor->previousMovementSign  = actor->movementSign;
    actor->previousTurnSign      = actor->turnSign;
    actor->previousPadHeld       = actor->padHeld;
    heldButtons                  = gGameSession->padHeld;
    actor->previousRunButtonHeld = actor->runButtonHeld;
    actor->padHeld               = heldButtons;
    actor->padPressed            = actor->padHeld & ~actor->previousPadHeld;
    actor->padReleased           = actor->previousPadHeld & ~actor->padHeld;
    actor->runButtonHeld         = (actor->padHeld & PAD_BUTTON_CROSS) != 0;
}

/// Binds the character's equipped-weapon bank to the player's animation context.
///
/// characterId must be 1 or 2 and its base plus weapon must select a non-NULL
/// loaded bank in entries 0..33. Borrows that bank and the live model, slots
/// and word-aligned pose buffer for subsequent playback. Does not start a clip
/// or reset slots; their active extent must meet `animationInitContext`'s contract.
static void _animationBindPlayerWeaponBank(Task* task)
{
    GameActor* actor;
    TmdObject* model;

    actor                     = task->work;
    model                     = task->extra.tmd;
    actor->animationBankIndex = Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon;
    actor->animationSets      = Gp_PlayerAnimBlkTbl[actor->animationBankIndex]->table.sets;
    animationInitContext(&actor->animationContext, actor->animationSets, model, actor->poseBuffer,
                         actor->animationSlots);
}

void playerActorResetChildSlots(Task* task, s32 setIndex)
{
    enum { PLAYER_ACTOR_FIRST_CHILD_ANIMATION_SLOT = 1 };
    GameActor* actor;
    s32        slotIndex;

    actor = task->work;
    for (slotIndex = PLAYER_ACTOR_FIRST_CHILD_ANIMATION_SLOT; slotIndex < actor->animationSlotCount; slotIndex++) {
        animationResetSlot(&actor->animationContext, slotIndex, setIndex);
        actor->animationSlots[slotIndex].rate = actor->animationRate;
    }
}

void playerActorPlayChildSlots(Task* task, s32 setIndex, s32 unusedArgument)
{
    enum { PLAYER_ACTOR_FIRST_CHILD_ANIMATION_SLOT = 1 };
    GameActor* actor;
    s32        slotIndex;

    actor = task->work;
    for (slotIndex = PLAYER_ACTOR_FIRST_CHILD_ANIMATION_SLOT; slotIndex < actor->animationSlotCount; slotIndex++) {
        // Capture the prior pose at its previous rate, then apply the actor's rate.
        animationPlaySlotWithBlend(&actor->animationContext, slotIndex, NULL, setIndex, 0, 0, 0, actor->animationSets);
        actor->animationSlots[slotIndex].rate = actor->animationRate;
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

void playerActorTickChildSlots(Task* task)
{
    enum { PLAYER_ACTOR_FIRST_CHILD_ANIMATION_SLOT = 1 };
    GameActor* actor;
    s32        slotIndex;

    actor = task->work;
    for (slotIndex = PLAYER_ACTOR_FIRST_CHILD_ANIMATION_SLOT; slotIndex < actor->animationSlotCount; slotIndex++) {
        animationTickSlot(&actor->animationContext, slotIndex);
    }
}

/// Returns the player's idle-clip band: 0 above half HP, 1 above a quarter, 2 otherwise.
///
/// Equality belongs to the lower band. Thresholds truncate by signed shifts of
/// the maximum HP's low 16 bits; the normal maximum is nonnegative.
static s32 _playerActorGetIdleHealthBand(void)
{
    enum {
        PLAYER_ACTOR_IDLE_HEALTH_ABOVE_HALF      = 0,
        PLAYER_ACTOR_IDLE_HEALTH_ABOVE_QUARTER   = 1,
        PLAYER_ACTOR_IDLE_HEALTH_AT_MOST_QUARTER = 2,
    };
    const PlayerStatus* playerStatus;
    s32                 shiftedMaxHp;
    s32                 healthBand;

    playerStatus = &gPlayerStatus;
    shiftedMaxHp = (u16)playerStatus->hpMax << 16;
    if ((shiftedMaxHp >> 17) < playerStatus->hp) {
        healthBand = PLAYER_ACTOR_IDLE_HEALTH_ABOVE_HALF;
    } else {
        healthBand = PLAYER_ACTOR_IDLE_HEALTH_ABOVE_QUARTER;
        if ((shiftedMaxHp >> 18) >= playerStatus->hp) {
            healthBand = PLAYER_ACTOR_IDLE_HEALTH_AT_MOST_QUARTER;
        }
    }
    return healthBand;
}

void playerActorClearLockTarget(Task* task)
{
    GameActor*       actor;
    WorldTargetNode* target;

    actor  = task->work;
    target = actor->targetNode;
    if (target != NULL) {
        target->state.parts.targeted = 0;
        actor->targetNode            = NULL;
    }
    actor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
}

/// Selects forward/backward motion from an all-enable collision displacement.
///
/// Borrows a displacement in the model root's parent frame. Only request mask
/// 7 with nonzero X/Z changes the sign: within a quarter-turn of facing is 1,
/// otherwise -1. All other requests retain it. Returns the resulting sign.
/// Requires live actor/model work; yaw differences narrow to signed halfwords.
static s32 _playerActorSetMovementSignFromDisplacement(Task* task, const GameActorMoveBy* move)
{
    GameActor* actor;
    GfxCoord*  rootCoord;
    s16        headingDelta;

    actor = task->work;
    if (move->collisionRequests == GAME_ACTOR_COLLISION_REQUEST_MASK) {
        if ((move->displacement.vx != 0) || (move->displacement.vz != 0)) {
            rootCoord    = task->extra.tmd->coords;
            headingDelta = ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]) - ratan2(move->displacement.vx, move->displacement.vz);
            // Keep the asymmetric positive half-turn boundary used by this path.
            if (headingDelta > ACTOR_TRANSFORM_ANGLE_HALF_TURN + 1) {
                headingDelta -= ACTOR_TRANSFORM_ANGLE_TURN;
            }
            if (headingDelta < -ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
                headingDelta += ACTOR_TRANSFORM_ANGLE_TURN;
            }
            if (ABS(headingDelta) < ACTOR_TRANSFORM_ANGLE_TURN / 4) {
                actor->movementSign = 1;
            } else {
                actor->movementSign = -1;
            }
        }
    }
    return actor->movementSign;
}

void playerActorGetPointDelta(const GfxCoord* coord, const VECTOR3* point, VECTOR3* delta)
{
    delta->vx = point->vx - coord->coord.t[0];
    delta->vy = point->vy - coord->coord.t[1];
    delta->vz = point->vz - coord->coord.t[2];
}

/// Subtracts a coordinate's transformed local point (0, -1536, 0) from a point.
///
/// Has no callers in this image; unusedArgument has no effect. Inputs share
/// the coordinate's parent frame and use game-coordinate units. Reads only
/// the local transform, without composing it; writes XYZ with retained GTE
/// fixed-point behavior. Inputs are borrowed for the call and point may alias
/// delta. Uses one VECTOR scratch block and changes GTE state.
static void _playerActorGetElevatedPointDelta(GfxCoord* coord, s32 unusedArgument, const VECTOR3* point, VECTOR3* delta)
{
    enum { PLAYER_ACTOR_ELEVATED_POINT_HEIGHT = 1536 };
    VECTOR* scratchEnd;
    VECTOR* localOffset;

    scratchEnd                   = SCRATCH_STACK_CURSOR(VECTOR);
    localOffset                  = scratchEnd - 1;
    SCRATCH_STACK_CURSOR(VECTOR) = localOffset;
    localOffset->vx              = 0;
    localOffset->vy              = -PLAYER_ACTOR_ELEVATED_POINT_HEIGHT;
    localOffset->vz              = 0;
    ApplyMatrixLV(&coord->coord, localOffset, localOffset);
    delta->vx = point->vx - (coord->coord.t[0] + localOffset->vx);
    delta->vy = point->vy - (coord->coord.t[1] + localOffset->vy);
    delta->vz = point->vz - (coord->coord.t[2] + localOffset->vz);
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

s32 playerActorPlanarLength(s32 x, s32 z)
{
    x = ABS(x);
    x = x * x;
    z = ABS(z);
    z = z * z;
    return SquareRoot0(x + z);
}

s32 playerActorPlanarDistance(const VECTOR3* firstPoint, const VECTOR3* secondPoint)
{
    PlayerActorPlanarDistanceScratch* head;
    PlayerActorPlanarDistanceScratch* block;
    s32                               zDifference;
    s32                               zSquared;
    s32                               xSquared;
    s32                               distance;

    head = SCRATCH_STACK_CURSOR(PlayerActorPlanarDistanceScratch);
    // Stage XYZ before publishing the reservation to the nested square-root call.
    head[-1].delta.vx                                      = firstPoint->vx - secondPoint->vx;
    block                                                  = head - 1;
    block->delta.vy                                        = firstPoint->vy - secondPoint->vy;
    zDifference                                            = firstPoint->vz - secondPoint->vz;
    zSquared                                               = ABS(zDifference);
    block->delta.vz                                        = zDifference;
    zSquared                                               = zSquared * zSquared;
    xSquared                                               = head[-1].delta.vx;
    xSquared                                               = ABS(xSquared);
    xSquared                                               = xSquared * xSquared;
    SCRATCH_STACK_CURSOR(PlayerActorPlanarDistanceScratch) = block;
    distance                                               = SquareRoot0(xSquared + zSquared);
    SCRATCH_STACK_RELEASE_BLOCK(PlayerActorPlanarDistanceScratch);
    return distance;
}

s16 playerActorShortestTurn(s16 currentAngle, s16 targetAngle)
{
    void**                           scratchHeadAddress = SCRATCH_HEAD_ADDR;
    _PlayerActorShortestTurnScratch* candidates;

    SCRATCH_PUSH_AT(scratchHeadAddress, _PlayerActorShortestTurnScratch);
    candidates            = SCRATCH_HEAD_AT(scratchHeadAddress, _PlayerActorShortestTurnScratch);
    candidates->direct    = targetAngle - currentAngle;
    candidates->plusTurn  = candidates->direct + ACTOR_TRANSFORM_ANGLE_TURN;
    candidates->minusTurn = candidates->direct - ACTOR_TRANSFORM_ANGLE_TURN;
    if (ABS(candidates->direct) < ABS(candidates->plusTurn) && ABS(candidates->direct) < ABS(candidates->minusTurn)) {
        currentAngle = candidates->direct;
    } else if (ABS(candidates->plusTurn) < ABS(candidates->minusTurn)) {
        currentAngle = candidates->plusTurn;
    } else {
        currentAngle = candidates->minusTurn;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_PlayerActorShortestTurnScratch);
    return currentAngle;
}

void playerActorTrackLockTarget(Task* task)
{
    enum {
        PLAYER_ACTOR_LOCK_MIN_GROUND_DISTANCE          = 0x180,
        PLAYER_ACTOR_GUNBLADE_LOCK_MIN_GROUND_DISTANCE = 0x200,
    };
    GameActor*       actor;
    WorldTargetNode* target;
    PlayerStatus*    playerStatus;
    s32              minGroundDistance;

    actor  = task->work;
    target = actor->targetNode;
    if (target == NULL) {
        actor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
        return;
    }
    if (target->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE) {
        target->state.parts.targeted = 0;
        actor->targetNode            = NULL;
        actor->aimTrackingState      = GAME_ACTOR_AIM_TRACKING_DECAY;
        return;
    }
    if (actor->aimTrackingState == GAME_ACTOR_AIM_TRACKING_TARGET) {
        playerStatus = &gPlayerStatus;
        if (playerStatus->weapon == PLAYER_ACTOR_WEAPON_GUNBLADE) {
            minGroundDistance = PLAYER_ACTOR_GUNBLADE_LOCK_MIN_GROUND_DISTANCE;
        } else {
            minGroundDistance = PLAYER_ACTOR_LOCK_MIN_GROUND_DISTANCE;
        }
        playerActorAimYawToLock(task, minGroundDistance);
        if (playerStatus->weapon == PLAYER_ACTOR_WEAPON_GUNBLADE) {
            _playerActorAimRollToLock(task);
        } else {
            playerActorAimPitchToLock(task);
        }
    }
}

/// Returns the live player's character-specific coordinate for selector 0 or 1.
///
/// Has no callers in this image. Requires an occupied player slot, a loaded
/// model and saved characterId 1..2. Character 1 selects parts 18/15;
/// character 2 selects root 0 for either selector. Selector roles are unproven.
/// Returns a borrowed node valid only while that player's model remains live.
static GfxCoord* _playerActorGetCharacterPartCoord(s32 partSelector)
{
    Task* playerTask;
    u8    partIndex;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    partIndex  = D_80112E2C[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1][partSelector];
    return &playerTask->extra.tmd->coords[partIndex];
}

void actorRenderPlaceCoordOffset(GfxCoord* source, GfxCoord* placed, const SVECTOR* localOffset)
{
    MATRIX* viewMatrix;

    // Apply the source's complete transform to the local-space point.
    source->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(source);
    placed->workm = source->workm;
    gte_SetRotMatrix(&source->workm);
    gte_SetTransMatrix(&source->workm);
    gte_ldv0(localOffset);
    gte_rtv0tr();
    gte_stlvnl(placed->workm.t);
    // Express the composed placement beneath the view node.
    viewMatrix = &gGfxViewCoord.workm;
    gfxMakeRelativeTransform(viewMatrix, &placed->workm, &placed->coord);
    placed->parent       = PARENT_OF(viewMatrix, GfxCoord, workm);
    placed->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(placed);
}

s32 playerActorHasWallContact(Task* task)
{
    const GameActor* actor;
    s32              contactIndex;

    actor = task->work;
    for (contactIndex = 0; contactIndex < ARRAY_SIZE(actor->collisionContacts); contactIndex++) {
        if ((actor->collisionContacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_GRID_FLOOR) == WORLD_COLLISION_CONTACT_GRID) {
            return 1;
        }
    }
    return 0;
}

/// Posts the selected attack vibration once while the actor's rumble latch is clear.
///
/// Requires live GameActor work. The selector narrows to its low 16 bits;
/// only index 0 is supported by the one-entry preset table and current caller.
/// Posts port 0's variable motor using duration units converted by the pad API
/// to twice as many serviced polls. The latch is incremented only from zero.
static void _playerActorPostVibrationPreset(Task* task, s32 presetIndex)
{
    GameActor*                         actor;
    const _PlayerActorVibrationPreset* preset;
    s32                                selectedPresetIndex;

    actor               = task->work;
    selectedPresetIndex = (u16)presetIndex;
    // Normal-mode state 4 calls this every tick; the latch allows one post.
    if (actor->rumblePosted == 0) {
        actor->rumblePosted++;
        preset = &D_80112E28[selectedPresetIndex];
        padPostVibrationRequest(0, PAD_VIBRATION_MOTOR_VARIABLE, preset->intensity, preset->durationUnits);
    }
}

Task* playerActorSpawnAttachment(Task* actorTask, s32 attachmentIndex, s32 rigIndex, s32 pairVariant)
{
    enum {
        PLAYER_ACTOR_ATTACHMENT_BANK          = 7,
        PLAYER_ACTOR_ATTACHMENT_RESOURCE_BIAS = 2,
        PLAYER_ACTOR_ATTACHMENT_PAIR_SIZE     = 2,
    };
    Task*            task;
    const GameActor* actor;
    TmdObject*       actorModel;
    GfxCoord*        attachmentCoord;
    TmdObject*       attachmentModel;
    GfxCoord*        parentCoord;
    const u8*        descriptorBases;
    s32              resourceOffset;

    actorModel      = actorTask->extra.tmd;
    actor           = actorTask->work;
    parentCoord     = &actorModel->coords[D_80112E04[rigIndex][attachmentIndex]];
    descriptorBases = D_80112DFC;
    resourceOffset  = gPlayerStatus.resourceVariant - PLAYER_ACTOR_ATTACHMENT_RESOURCE_BIAS;
    task            = taskSpawn(PLAYER_ACTOR_ATTACHMENT_BANK, descriptorBases[rigIndex + resourceOffset] + pairVariant * PLAYER_ACTOR_ATTACHMENT_PAIR_SIZE + attachmentIndex, 0, 0);
    if (task == NULL) {
        return NULL;
    }
    // Attach beneath the rig joint; the first update keeps spawn-time draw flags.
    task->parent                      = actorTask;
    attachmentCoord                   = task->extra.tmd->coords;
    attachmentCoord->parent           = parentCoord;
    attachmentCoord->param.clearFlags = false;
    attachmentModel                   = task->extra.tmd;
    _playerActorBindAttachmentTextures(attachmentModel, actor);
    return task;
}

Task* playerActorSpawnWeaponModel(Task* parentTask, s32 characterId, s32 weaponId, s32 spawnArg)
{
    enum { PLAYER_ACTOR_WEAPON_MODEL_BANK = 7 };
    Task*      task;
    GfxCoord*  parentCoord;
    TmdObject* weaponModel;
    GfxCoord*  weaponCoord;
    s32        descriptorBase;

    parentCoord = parentTask->extra.tmd->coords;
    if (weaponId == PLAYER_STATUS_EQUIPMENT_NONE) {
        return NULL;
    }
    descriptorBase = D_80112DF4[characterId] - 1;
    task           = taskSpawn(PLAYER_ACTOR_WEAPON_MODEL_BANK, descriptorBase + weaponId, spawnArg, 0);
    if (task == NULL) {
        return NULL;
    }
    weaponModel                   = task->extra.tmd;
    task->parent                  = parentTask;
    weaponCoord                   = weaponModel->coords;
    weaponCoord->parent           = parentCoord;
    weaponCoord->param.clearFlags = true;
    return task;
}

s32 playerActorRemoveEquipment(void)
{
    Task*      playerTask;
    GameActor* actor;
    Task*      equipmentTask;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    actor      = playerTask->work;
    if (!playerTask | !actor) {
        return 0;
    }

    equipmentTask = actor->equipmentTasks[0];
    if (equipmentTask != NULL) {
        taskKill(equipmentTask);
        actor->equipmentTasks[0] = NULL;
    }

    equipmentTask = actor->equipmentTasks[1];
    if (equipmentTask != NULL) {
        taskKill(equipmentTask);
        actor->equipmentTasks[1] = NULL;
    }

    equipmentTask = actor->weaponEffectTask;
    if (equipmentTask != NULL) {
        taskKill(equipmentTask);
        actor->weaponEffectTask = NULL;
    }

    worldCollisionUnlinkBody(&actor->collisionBodies[GAME_ACTOR_BODY_WEAPON]);
    return 1;
}

Task* playerActorSpawnGrenadeProjectile(Task* actorTask, s32 actorVariant, s32 launcherIndex, s32 spawnArg)
{
    enum {
        PLAYER_ACTOR_GRENADE_PROJECTILE_BANK = 7,
        PLAYER_ACTOR_GRENADE_PROJECTILE_BASE = 0x60,
        PLAYER_ACTOR_GRENADE_LAUNCHER_SHIFT  = 2,
    };
    Task*            task;
    const GameActor* actor;
    GfxCoord*        muzzleCoord;
    TmdObject*       projectileModel;
    s32              launcherOffset;

    actor          = actorTask->work;
    muzzleCoord    = actor->equipmentTasks[1]->extra.tmd->coords;
    launcherOffset = launcherIndex << PLAYER_ACTOR_GRENADE_LAUNCHER_SHIFT;
    actorVariant  += PLAYER_ACTOR_GRENADE_PROJECTILE_BASE;
    task           = taskSpawn(PLAYER_ACTOR_GRENADE_PROJECTILE_BANK, launcherOffset + actorVariant, spawnArg, 0);
    if (task == NULL) {
        return NULL;
    }
    projectileModel                 = task->extra.tmd;
    task->parent                    = actorTask;
    projectileModel->coords->parent = muzzleCoord;
    return task;
}

/// Clears movement and aim state and enters scripted player control.
///
/// Resets phase, signs, aim angles, pending hit and the interaction press latch,
/// then resets weapon attack effects. During an event it also disables the
/// root body's view triggers. Does not select a scripted state or clip and does
/// not clear turn-rate or movement-mode indices. Requires live actor, session,
/// equipped-effect and saved-supply state; retains no new pointers.
static inline void _playerActorEnterScriptedMode(Task* task)
{
    GameActor*    actor;
    PlayerStatus* playerStatus;

    actor                                                 = task->work;
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
    playerActorResetWeaponAttack(task, playerStatus->weapon, 0);
    if (gGameSession->eventState != 0) {
        actor->collisionBodies[GAME_ACTOR_BODY_ROOT].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED);
    }
}

/// Takes scripted control and plays a clip from a loaded player animation bank.
///
/// Borrows request only for this call; source.index must select a non-NULL bank
/// in `Gp_PlayerAnimBlkTbl` (currently 1..33). Its sets and records stay live during
/// playback. Requires live actor/model, initialized pose and child-slot storage,
/// session and weapon effects. animationId narrows to u16 and must supply every
/// active child track; when blending, blendFrames counts 0..2047 whole normal-rate
/// frames and is ignored on reset.
/// Every nonzero blend chooses blending. Rebinds the context and
/// records the bank index only when the set-table pointer changes.
///
/// Requests deferred grid participation from enableWorldCollision, retaining
/// only the request's meaning (zero disable, nonzero enable). Returns 0;
/// message ID and second payload word are unused, including direct initialization.
static s32 _playerActorPlayScriptedAnimation(Task* task, s32 unusedMessageId, const AnimationPlayRequest* request, s32 unusedSecondArg)
{
    GameActor* actor;
    TmdObject* model;

    actor = task->work;
    model = task->extra.tmd;
    _playerActorEnterScriptedMode(task);
    // Select the animation table before resetting or blending its slots.
    actor->state = PLAYER_ACTOR_SCRIPTED_ANIMATION_STATE;
    if (actor->animationSets != Gp_PlayerAnimBlkTbl[request->source.index]->table.sets) {
        actor->animationSets = Gp_PlayerAnimBlkTbl[request->source.index]->table.sets;
        animationInitContext(&actor->animationContext, actor->animationSets, model, actor->poseBuffer,
                             actor->animationSlots);
        actor->animationBankIndex = (u16)request->source.index;
    }
    actor->animationRate = ANIMATION_RATE_ONE;
    if (request->blend == ANIMATION_BLEND_RESET) {
        playerActorResetChildSlots(task, request->animationId);
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

/// Copies parent draw flags to an attachment, then repeats the operation on the parent.
///
/// Both models must be live. A non-NULL `bufferOperation` receives `parentModel`
/// after the flag copy, including when the operation was already applied to it.
/// NULL only copies flags; the attachment's buffer is never the operation's target.
static inline void _playerActorApplyDrawToAttachment(Task* attachment, TmdObject* parentModel, void (*bufferOperation)(TmdObject*))
{
    attachment->extra.tmd->flags = parentModel->flags;
    if (bufferOperation != NULL) {
        bufferOperation(parentModel);
    }
}

s32 playerActorSetModelDraw(Task* task, s32 unusedMessageId, s32 drawMode, s32 unusedSecondArg)
{
    GameActor* actor;
    TmdObject* model;
    void       (*bufferOperation)(TmdObject*);
    Task*      childHead;
    Task*      firstChild;
    Task*      child;

    actor           = task->work;
    model           = task->extra.tmd;
    bufferOperation = NULL;
    switch (drawMode) {
        case PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE:
            bufferOperation = tmdAllocPrimitiveBuffer;
            model->flags    = (model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW) & (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO:
            model->flags = model->flags & (u16) ~(TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            break;
        case PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE:
            bufferOperation = tmdFreePrimitiveBuffer;
            model->flags    = model->flags | (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            break;
        case PLAYER_ACTOR_MODEL_DRAW_HIDE_KEEP:
            model->flags = model->flags | (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            break;
        case PLAYER_ACTOR_MODEL_DRAW_SHOW_ALLOCATE:
            bufferOperation = tmdAllocPrimitiveBuffer;
            model->flags    = model->flags & (u16) ~(TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            break;
    }
    if (bufferOperation != NULL) {
        bufferOperation(model);
    }
    // Children inherit all flags; each selected operation still targets the parent.
    if (actor->attachmentTasks[0] != NULL) {
        _playerActorApplyDrawToAttachment(actor->attachmentTasks[0], model, bufferOperation);
    }
    if (actor->attachmentTasks[1] != NULL) {
        _playerActorApplyDrawToAttachment(actor->attachmentTasks[1], model, bufferOperation);
    }
    if (actor->equipmentTasks[1] != NULL) {
        actor->equipmentTasks[1]->extra.tmd->flags = model->flags;
        childHead                                  = actor->equipmentTasks[1];
        childHead                                  = childHead->firstChild;
        if (childHead != NULL) {
            firstChild                   = childHead;
            firstChild->extra.tmd->flags = model->flags;
            child                        = firstChild;
            if (bufferOperation != NULL) {
                bufferOperation(model);
            }
            while (child->nextSibling != firstChild) {
                child                   = child->nextSibling;
                child->extra.tmd->flags = model->flags;
                if (bufferOperation != NULL) {
                    bufferOperation(model);
                }
            }
        }
    }
    return 0;
}

s32 playerActorEndScripted(Task* task, s32 unusedMessageId, s32 resumeMode, s32 unusedSecondArg)
{
    TmdObject* model;
    GameActor* actor;
    GfxCoord*  rootCoord;
    GfxCoord*  part1Coord;
    u16        actorMode;
    VECTOR     part1Offset;

/// Transfers part 1's animated offset into root X/Z, preserving both Y translations.
///
/// Borrows live nodes and a writable VECTOR lvalue for staging. Arguments must
/// be stable and side-effect-free; each is evaluated repeatedly. All XYZ are
/// transformed by the root's local basis, then only X/Z are transferred and
/// cleared. Does not compose or invalidate coordinate caches. No configuration
/// bindings or captured locals are required. Expand only as a standalone
/// statement sequence inside a compound block.
#define PLAYER_ACTOR_BAKE_PART1_OFFSET(rootCoord, part1Coord, part1Offset) \
    (part1Offset).vx = (part1Coord)->coord.t[0];                           \
    (part1Offset).vy = (part1Coord)->coord.t[1];                           \
    (part1Offset).vz = (part1Coord)->coord.t[2];                           \
    ApplyMatrixLV(&(rootCoord)->coord, &(part1Offset), &(part1Offset));    \
    (rootCoord)->coord.t[0] += (part1Offset).vx;                           \
    (rootCoord)->coord.t[2] += (part1Offset).vz;                           \
    (part1Coord)->coord.t[0] = 0;                                          \
    (part1Coord)->coord.t[2] = 0

    model      = task->extra.tmd;
    actor      = task->work;
    rootCoord  = model->coords;
    actorMode  = actor->mode;
    part1Coord = rootCoord + 1;
    if (actorMode != GAME_ACTOR_MODE_SCRIPTED) {
        return 1;
    }
    if (resumeMode != actorMode) {
        // Bake part 1's animated horizontal offset into the root before changing banks.
        PLAYER_ACTOR_BAKE_PART1_OFFSET(rootCoord, part1Coord, part1Offset);
    }
    actor->previousPosition.vx                          = rootCoord->coord.t[0];
    actor->previousPosition.vy                          = rootCoord->coord.t[1];
    actor->previousPosition.vz                          = rootCoord->coord.t[2];
    actor->animationBankIndex                           = Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon;
    actor->animationSets                                = Gp_PlayerAnimBlkTbl[actor->animationBankIndex]->table.sets;
    actor->animationRate                                = ANIMATION_RATE_ONE;
    actor->pendingCollisionUpdates                      = GAME_ACTOR_COLLISION_REQUEST_MASK;
    actor->collisionBodies[GAME_ACTOR_BODY_ROOT].flags |= WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED;
    if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
        animationInitContext(&actor->animationContext, actor->animationSets, model, actor->poseBuffer,
                             actor->animationSlots);
        if (resumeMode == actorMode) {
            _playerActorEnterAimLocomotion(task, 0);
        } else {
            playerActorEnterAim(task, 0);
        }
        return 0;
    }
    if (resumeMode == actorMode) {
        playerActorExitAim(task);
        return 0;
    }
    if (resumeMode == PLAYER_ACTOR_END_SCRIPTED_RESET_ANIMATION) {
        animationInitContext(&actor->animationContext, actor->animationSets, model, actor->poseBuffer,
                             actor->animationSlots);
        playerActorEnterLocomotion(task, 1);
    } else {
        playerActorEnterLocomotion(task, 0);
    }
    return 0;
}
#undef PLAYER_ACTOR_BAKE_PART1_OFFSET

/// Captures a new logical interaction press while the player is in locomotion or aim.
///
/// Clears the live status latch on every tick, including other modes/states.
/// Requires live GameActor work with layout-remapped `padPressed` for this tick.
static void _playerActorCaptureInteractionPress(Task* task)
{
    enum {
        PLAYER_ACTOR_INTERACTION_LOCOMOTION_STATE = 0,
        PLAYER_ACTOR_INTERACTION_AIM_STATE        = 2,
    };
    GameActor*    actor;
    PlayerStatus* playerStatus;

    actor                            = task->work;
    playerStatus                     = &gPlayerStatus;
    playerStatus->interactionPressed = 0;
    if (actor->mode != GAME_ACTOR_MODE_SCRIPTED) {
        if (actor->mode == GAME_ACTOR_MODE_NORMAL) {
            if (actor->state == PLAYER_ACTOR_INTERACTION_LOCOMOTION_STATE || actor->state == PLAYER_ACTOR_INTERACTION_AIM_STATE) {
                if (actor->padPressed & PAD_BUTTON_CIRCLE) {
                    playerStatus->interactionPressed = 1;
                }
            }
        }
    }
}

/// Takes scripted control after clearing movement, aim offsets and weapon contact.
///
/// Requires live player work, session and equipped weapon-effect resources.
/// Clears the interaction latch and pending hit, then resets weapon effects;
/// an active event also suppresses root-body view triggers. The scripted state,
/// animation bank and clip remain for the caller to select. No callers in this image.
static void _playerActorBeginScriptedControl(Task* task)
{
    _playerActorEnterScriptedMode(task);
}

s32 playerActorInstallScriptedAnimation(Task* task, s32 unusedMessageId, const AnimationPlayRequest* request, s32 unusedSecondArg)
{
    GameActor* actor;
    TmdObject* model;

    actor = task->work;
    model = task->extra.tmd;
    _playerActorEnterScriptedMode(task);
    // Select the animation table before resetting or blending its slots.
    actor->state              = PLAYER_ACTOR_SCRIPTED_ANIMATION_STATE;
    actor->animationSets      = request->source.sets;
    actor->animationBankIndex = PLAYER_ACTOR_DIRECT_ANIMATION_BANK;
    actor->animationRate      = ANIMATION_RATE_ONE;
    if (request->blend == ANIMATION_BLEND_RESET) {
        animationInitContext(&actor->animationContext, actor->animationSets, model, actor->poseBuffer,
                             actor->animationSlots);
        playerActorResetChildSlots(task, request->animationId);
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

/// Replaces child-slot playback while retaining the player's current control state.
///
/// Borrows the request through dispatch and its set table and clip data for
/// playback. Sets the direct-bank sentinel and normal rate. Reset initializes
/// the animation context and slots; every nonzero blend captures the current
/// pose and uses blendFrames (0..2047 whole normal-rate frames). animationId
/// must select a loaded set with every active child track: reset indexes the
/// full word, while blending keeps its low 16 bits. Live actor/model, pose and
/// slot storage must meet the child-slot playback contracts.
/// Queues grid disablement for zero enableWorldCollision, enablement otherwise.
/// Returns 0; message ID and second payload word are unused.
static s32 _playerActorReplaceAnimation(Task* task, s32 unusedMessageId, const AnimationPlayRequest* request, s32 unusedSecondArg)
{
    GameActor* actor;
    TmdObject* model;
    s32        collisionUpdateMask;

    actor = task->work;
    model = task->extra.tmd;
    // Blending retains the existing context and captures its current pose.
    actor->animationSets      = request->source.sets;
    actor->animationBankIndex = PLAYER_ACTOR_DIRECT_ANIMATION_BANK;
    actor->animationRate      = ANIMATION_RATE_ONE;
    if (request->blend == ANIMATION_BLEND_RESET) {
        animationInitContext(&actor->animationContext, actor->animationSets, model, actor->poseBuffer,
                             actor->animationSlots);
        playerActorResetChildSlots(task, request->animationId);
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

s32 playerActorPlace(Task* task, s32 unusedMessageId, const ActorTransform* transform, s32 unusedSecondArg)
{
    TmdObject* model;
    GameActor* actor;
    GfxCoord*  rootCoord;
    MATRIX*    localMatrix;

    model                 = task->extra.tmd;
    actor                 = task->work;
    rootCoord             = model->coords;
    rootCoord->coord.t[0] = transform->pos.vx;
    rootCoord->coord.t[1] = transform->pos.vy;
    rootCoord->coord.t[2] = transform->pos.vz;
    actor->rotation.vx    = transform->rot.vx;
    actor->rotation.vy    = transform->rot.vy;
    actor->rotation.vz    = transform->rot.vz;
    localMatrix           = &rootCoord->coord;
    RotMatrix(&actor->rotation, localMatrix);
    MatrixNormal(localMatrix, localMatrix);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    return 0;
}

s32 playerActorTurnToYaw(Task* task, s32 unusedMessageId, const ActorTransform* transform, s32 unusedSecondArg)
{
    enum {
        PLAYER_ACTOR_TURN_SCRATCH_BYTES   = 16,
        PLAYER_ACTOR_TURN_NEGATIVE_SET    = 5,
        PLAYER_ACTOR_TURN_NONNEGATIVE_SET = 6,
    };
    GameActor* actor;
    s32*       turnScratch;
    s32        turnDelta;
    s32        setIndex;
    s16        targetYaw;

    actor       = task->work;
    turnScratch = SCRATCH_STACK_RESERVE_BYTES(PLAYER_ACTOR_TURN_SCRATCH_BYTES);
    _playerActorEnterScriptedMode(task);
    actor->scriptedMotionPending   = 1;
    actor->state                   = PLAYER_ACTOR_SCRIPTED_TURN_STATE;
    actor->pendingCollisionUpdates = PLAYER_ACTOR_WORLD_COLLISION_DISABLE;
    targetYaw                      = transform->rot.vy;
    actor->scriptMotion.targetYaw  = targetYaw;
    turnDelta                      = playerActorShortestTurn(actor->rotation.vy, targetYaw);
    // The binary retains the turn word in this 16-byte reservation.
    turnScratch[0] = turnDelta;
    setIndex       = PLAYER_ACTOR_TURN_NONNEGATIVE_SET;
    if (turnDelta < 0) {
        setIndex = PLAYER_ACTOR_TURN_NEGATIVE_SET;
    }
    playerActorPlayChildSlots(task, setIndex, 0);
    SCRATCH_STACK_RELEASE_BYTES(PLAYER_ACTOR_TURN_SCRATCH_BYTES);
    return 0;
}

/// Takes scripted control and starts a stair flight along the player's facing.
///
/// Borrows the request through dispatch and copies its direction and step count
/// into signed halfwords. The retained direction selects ascent for zero,
/// descent otherwise; the initial clip tests the full request word. The retained
/// step count must be positive. Requires live actor, session, weapon effects
/// and native stair clips; no request pointer survives. Queues grid disablement
/// and marks motion pending until the stair state finishes. Returns 0; message
/// ID and second payload word are unused.
static s32 _playerActorClimbStairs(Task* task, s32 unusedMessageId, const GameActorStairClimb* climb, s32 unusedSecondArg)
{
    enum {
        PLAYER_ACTOR_STAIR_ASCENT_SET  = 0x24,
        PLAYER_ACTOR_STAIR_DESCENT_SET = 0x25,
    };
    GameActor* actor;
    s32        setIndex;

    actor = task->work;
    _playerActorEnterScriptedMode(task);
    // The stair state derives its pitched stride and footstep timing next tick.
    actor->state                   = PLAYER_ACTOR_SCRIPTED_STAIR_CLIMB_STATE;
    actor->scriptedMotionPending   = 1;
    actor->pendingCollisionUpdates = PLAYER_ACTOR_WORLD_COLLISION_DISABLE;
    actor->jumpVariant             = climb->descend;
    actor->scriptMotion.jumpSteps  = climb->stepCount;
    setIndex                       = PLAYER_ACTOR_STAIR_ASCENT_SET;
    if (climb->descend != 0) {
        setIndex = PLAYER_ACTOR_STAIR_DESCENT_SET;
    }
    playerActorPlayChildSlots(task, setIndex, 0);
    return 0;
}

s32 playerActorMoveTo(Task* task, s32 unusedMessageId, const ActorTransform* transform, const GameActorMoveAnim* moveAnim)
{
    GameActor* actor;

    actor = task->work;
    _playerActorEnterScriptedMode(task);
    actor->state                   = PLAYER_ACTOR_SCRIPTED_MOVE_TO_STATE;
    actor->scriptedMotionPending   = 1;
    actor->pendingCollisionUpdates = PLAYER_ACTOR_WORLD_COLLISION_DISABLE;
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

/// Takes scripted control and runs the player to a borrowed XYZ destination.
///
/// Copies game-coordinate XYZ in the model root's parent frame. The first
/// payload may be a VECTOR3 or the leading position of an ActorTransform;
/// rotation and the vector's fourth word are unread. Optional moveAnim IDs
/// narrow to halfwords, with zero selecting the running and standing defaults.
/// Payloads need readable storage only through dispatch. Requires live actor,
/// session, weapon effects and native animation resources. Queues grid
/// disablement and marks motion pending; state 8 turns first, then runs using
/// the walk-to state's horizontal arrival test. Returns 0; message ID is unused.
static s32 _playerActorRunTo(Task* task, s32 unusedMessageId, const VECTOR3* destination, const GameActorMoveAnim* moveAnim)
{
    GameActor* actor;

    actor = task->work;
    _playerActorEnterScriptedMode(task);
    actor->state                   = PLAYER_ACTOR_SCRIPTED_MOVE_TO_STATE;
    actor->scriptedMotionPending   = 1;
    actor->pendingCollisionUpdates = PLAYER_ACTOR_WORLD_COLLISION_DISABLE;
    actor->destination.vx          = destination->vx;
    actor->destination.vy          = destination->vy;
    actor->destination.vz          = destination->vz;
    // A null record, or a zero word, selects that clip's default. The low 16 bits are kept.
    if (moveAnim != NULL) {
        actor->actionArgument = moveAnim->approachAnimId;
        actor->actionValue    = moveAnim->arrivalAnimId;
    } else {
        actor->actionArgument = 0;
        actor->actionValue    = 0;
    }
    actor->state = PLAYER_ACTOR_SCRIPTED_RUN_TO_STATE;
    return 0;
}

s32 playerActorWalkSteps(Task* task, s32 unusedMessageId, const GameActorWalkSteps* walkSteps, s32 unusedSecondArg)
{
    GameActor* actor;

    actor = task->work;
    _playerActorEnterScriptedMode(task);
    actor->state                   = PLAYER_ACTOR_SCRIPTED_WALK_STEPS_STATE;
    actor->scriptedMotionPending   = 1;
    actor->pendingCollisionUpdates = PLAYER_ACTOR_WORLD_COLLISION_DISABLE;
    actor->actionValue             = walkSteps->stepCount;
    actor->stateTimer              = walkSteps->field_4;
    return 0;
}

s32 playerActorMoveBy(Task* task, s32 unusedMessageId, const GameActorMoveBy* move, s32 unusedSecondArg)
{
    GameActor* actor;
    GfxCoord*  rootCoord;

    actor     = task->work;
    rootCoord = task->extra.tmd->coords;
    if (move->keepControl == 0) {
        _playerActorEnterScriptedMode(task);
        actor->state                 = PLAYER_ACTOR_SCRIPTED_ANIMATION_STATE;
        actor->scriptedMotionPending = 1;
    }
    actor->pendingCollisionUpdates = move->collisionRequests;
    rootCoord->coord.t[0]         += move->displacement.vx;
    rootCoord->coord.t[1]         += move->displacement.vy;
    rootCoord->coord.t[2]         += move->displacement.vz;
    _playerActorSetMovementSignFromDisplacement(task, move);
    return playerActorHasWallContact(task);
}

/// Takes scripted control for a hold released by direction-pad or face-button presses.
///
/// Returns 1 without changes while the recovery byte is nonzero; otherwise
/// clears movement, interaction and aim state, resets weapon effects and
/// acquires the attachment event lock. Copies pressCount and clears the
/// signed-halfword progress counter; animation is unread. A positive count
/// must fit s16 to be reachable; nonpositive counts finish on the first tick. Does
/// not replace the current clip or queue new collision updates. Requires live
/// actor/session/equipment and a request readable through dispatch only.
/// Returns 0 after acceptance; message ID and second payload word are unused.
static s32 _playerActorAwaitButtonPresses(Task* task, s32 unusedMessageId, const GameActorButtonPressHold* request, s32 unusedSecondArg)
{
    GameActor* actor;

    actor = task->work;
    // Recovery uses the state machine's signed-byte interpretation.
    if ((s8)actor->recoveryTicks != 0) {
        return 1;
    }
    _playerActorEnterScriptedMode(task);
    // Count input edges in state 6 while the attachment event lock holds.
    actor->state       = PLAYER_ACTOR_SCRIPTED_PRESS_HOLD_STATE;
    Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
    actor->stateTimer  = request->pressCount;
    actor->actionValue = 0;
    return 0;
}

/// Enters scripted state 10, waiting for fire input with world collision disabled.
///
/// Clears movement/aim/attack state through the scripted-mode entry reset and
/// queues collision disablement. Leaves the current animation and lock target
/// intact. Requires live actor, session and equipment state. The message ID
/// and both payload words are unused; returns 0.
static s32 _playerActorEnterScriptedAttack(Task* task, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    GameActor* actor;

    actor = task->work;
    _playerActorEnterScriptedMode(task);
    actor->state                   = PLAYER_ACTOR_SCRIPTED_ATTACK_STATE;
    actor->pendingCollisionUpdates = PLAYER_ACTOR_WORLD_COLLISION_DISABLE;
    return 0;
}

/// Takes scripted control and selects the player's paired presentation clips (state 7).
///
/// Zero clipVariant selects the active bank's clip 32; any nonzero s32 selects clip 33.
/// Playback starts on the next scripted tick. Clears movement signs, aim
/// offsets and weapon attack effects; event entry disables root view triggers.
/// Requires live actor, session and equipment resources and an active animation
/// bank containing both clips. The message ID and second payload word are unused. Returns 0.
static s32 _playerActorEnterScriptedPresentation(Task* task, s32 unusedMessageId, s32 clipVariant, s32 unusedSecondArg)
{
    enum { PLAYER_ACTOR_SCRIPTED_PRESENTATION_STATE = 7 };
    GameActor* actor;

    actor = task->work;
    _playerActorEnterScriptedMode(task);
    actor->state      = PLAYER_ACTOR_SCRIPTED_PRESENTATION_STATE;
    actor->stateTimer = clipVariant;
    return 0;
}

/// Starts the deferred item-use presentation unless the player already has scripted control.
///
/// Accepting returns 0, clears movement/aim/weapon-attack state and enters
/// scripted state 11, which plays active-bank clip 40. Event entry also disables
/// root view triggers. Already-scripted control returns 1 without changing it.
/// Requires live actor, session and equipment resources; later playback needs
/// an active bank containing clip 40. The message ID and both payload words are unused.
static s32 _playerActorEnterItemUse(Task* task, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    enum {
        PLAYER_ACTOR_ITEM_USE_STATE    = 11,
        PLAYER_ACTOR_ITEM_USE_ACCEPTED = 0,
        PLAYER_ACTOR_ITEM_USE_BUSY     = 1,
    };
    GameActor* actor;
    s32        result;

    actor  = task->work;
    result = PLAYER_ACTOR_ITEM_USE_ACCEPTED;
    if (actor->mode != GAME_ACTOR_MODE_SCRIPTED) {
        _playerActorEnterScriptedMode(task);
        actor->state = PLAYER_ACTOR_ITEM_USE_STATE;
    } else {
        result = PLAYER_ACTOR_ITEM_USE_BUSY;
    }
    return result;
}

s32 playerActorIsScriptedMotionPending(Task* task, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    const GameActor* actor = task->work;

    return actor->scriptedMotionPending;
}

s32 playerActorIsAnimationPlaying(Task* task, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    const GameActor* actor;
    s32              slotIndex;
    s32              playing;

    actor   = task->work;
    playing = 0;
    for (slotIndex = actor->animationSlotCount - 1; slotIndex > 0; slotIndex--) {
        // Any part not holding its boundary pose means the clip is still running.
        if ((actor->animationSlots[slotIndex].status.fields.flags & ANIMATION_SLOT_SETTLED) == 0) {
            playing = 1;
            break;
        }
    }
    return playing;
}

s32 playerActorIsSlotAdvancingLinearly(Task* task, s32 slotIndex, s32 unusedFirstArg, s32 unusedSecondArg)
{
    const GameActor*     actor;
    const AnimationSlot* slot;

    actor = task->work;
    slot  = &actor->animationSlots[(u32)slotIndex];
    return (slot->status.fields.flags & (ANIMATION_SLOT_SETTLED | ANIMATION_SLOT_FOLLOWED_JUMP)) == 0;
}

s32 playerActorSetAnimationRate(Task* task, s32 unusedMessageId, s32 rate, s32 unusedSecondArg)
{
    enum {
        PLAYER_ACTOR_ANIMATION_RATE_MIN = 1,
        PLAYER_ACTOR_ANIMATION_RATE_MAX = 0x7F,
    };
    GameActor* actor;
    s32        slotIndex;

    actor = task->work;
    if (rate <= 0) {
        rate = PLAYER_ACTOR_ANIMATION_RATE_MIN;
    } else if (rate >= PLAYER_ACTOR_ANIMATION_RATE_MAX + 1) {
        rate = PLAYER_ACTOR_ANIMATION_RATE_MAX;
    }
    slotIndex = 1;
    if (slotIndex < actor->animationSlotCount) {
        do {
            actor->animationSlots[slotIndex].rate = rate;
            slotIndex++;
        } while (slotIndex < actor->animationSlotCount);
    }
    actor->animationRate = rate;
    return 0;
}

/// Copies raw words into the equipped player bank's extension without starting playback.
///
/// characterId must be 1 or 2; its base plus weapon must select a non-NULL
/// loaded, writable bank in entries 0..33. Counts above 32 return 1 without
/// copying; nonpositive counts copy nothing and return 0. Accepted positive
/// counts overwrite words 47 onward and return 0. The borrowed request and
/// readable word-aligned source must remain valid through this call; the count
/// is reread each iteration and must remain within the checked capacity.
/// Copied clip pointers remain borrowed for playback; raw extension words may
/// include other payloads. Receiver, message ID and second payload are unused.
static s32 _animationCopyPlayerBankExtension(Task* unusedTask, s32 unusedMessageId, const AnimationBankCopyRequest* request, s32 unusedSecondArg)
{
    s32*       destinationWords;
    const s32* sourceWords;
    s32        wordIndex;
    s32        wordCount;

    destinationWords = Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon]->table.words;
    sourceWords      = request->source.words;
    wordCount        = request->wordCount;
    if (wordCount >= ANIMATION_BANK_EXTENSION_CAPACITY + 1) {
        return 1;
    }
    // Transfer raw words: the span can include records after the clip pointers.
    destinationWords = &destinationWords[ANIMATION_BANK_BASE_SET_COUNT];
    for (wordIndex = 0; wordIndex < request->wordCount; wordIndex++) {
        destinationWords[wordIndex] = sourceWords[wordIndex];
    }
    return 0;
}

/// Applies a packed enemy attack to HP and reports whether the damage was fatal.
///
/// `GAME_ACTOR_MESSAGE_APPLY_DAMAGE` supplies a category-4 attack key: 12-bit
/// power and a four-bit reaction. Requires live actor/player/session state and
/// the HP and difficulty domains of `damageComputeReceived`. Cheat mode
/// bypasses damage.
/// Fatal damage broadcasts release-hold; a surviving player receives the low
/// reaction byte, while companion work suppresses that effect. Returns the
/// fatal reply unchanged. The message id and second payload are unused.
/// Other key categories leave reaction unwritten; callers must supply category 4.
static s32 _playerActorApplyDamage(Task* task, s32 unusedMessageId, s32 attackKey, s32 unusedSecondArg)
{
    GameActor* actor;
    s32        fatalReply;
    s32        reaction;

    actor      = task->work;
    fatalReply = 0;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.cheatMode == 0) {
        fatalReply = playerStateApplyHpDamage(damageComputeReceived(attackKey, 0, &reaction, 0));
        if (fatalReply != 0) {
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, 0, ACTOR_MESSAGE_RELEASE_HOLD);
        } else if (actor->companionWork == NULL) {
            playerStateApplyReactionEffect(task, (u8)reaction);
        }
    }
    return fatalReply;
}

s32 playerActorAttachToCoord(Task* task, s32 unusedMessageId, GfxCoord* parent, s32 unusedSecondArg)
{
    gfxReparentCoord(parent, task->extra.tmd->coords);
    return 0;
}

/// Selects the player's walking or running movement speed for message 1020.
///
/// Zero `runEnabled` selects walk; every nonzero value selects run. Changes
/// only the movement selector on the live actor and returns 0.
static s32 _playerActorSetRunMovement(Task* task, s32 unusedMessageId, s32 runEnabled, s32 unusedSecondArg)
{
    enum {
        PLAYER_ACTOR_RUN_MOVEMENT_WALK = 1,
        PLAYER_ACTOR_RUN_MOVEMENT_RUN  = 3,
    };
    GameActor* actor;

    actor = task->work;
    if (runEnabled == 0) {
        actor->movementMode = PLAYER_ACTOR_RUN_MOVEMENT_WALK;
    } else {
        actor->movementMode = PLAYER_ACTOR_RUN_MOVEMENT_RUN;
    }
    return 0;
}

s32 playerActorSetTextureSequence(Task* task, s32 unusedMessageId, s32 sequence, s32 unusedSecondArg)
{
    enum {
        PLAYER_ACTOR_TEXTURE_SEQUENCE_RESET       = 0,
        PLAYER_ACTOR_TEXTURE_SEQUENCE_DEFAULT_A   = 1,
        PLAYER_ACTOR_TEXTURE_SEQUENCE_DEFAULT_B   = 2,
        PLAYER_ACTOR_TEXTURE_SEQUENCE_SECOND_BASE = 4,
    };
    GameActor* actor;

    actor = task->work;
    if (sequence == PLAYER_ACTOR_TEXTURE_SEQUENCE_RESET) {
        actor->textureSequenceA = PLAYER_ACTOR_TEXTURE_SEQUENCE_DEFAULT_A;
        actor->textureSequenceB = PLAYER_ACTOR_TEXTURE_SEQUENCE_DEFAULT_B;
        actor->textureDelayA    = 0;
        actor->textureDelayB    = 0;
        actor->textureFrameA    = 0;
        actor->textureFrameB    = 0;
    } else if (sequence < PLAYER_ACTOR_TEXTURE_SEQUENCE_SECOND_BASE) {
        actor->textureSequenceA = sequence + 1;
        actor->textureDelayA    = 0;
        actor->textureFrameA    = 0;
    } else {
        actor->textureSequenceB = sequence - (PLAYER_ACTOR_TEXTURE_SEQUENCE_SECOND_BASE - 1);
        actor->textureDelayB    = 0;
        actor->textureFrameB    = 0;
    }
    return 0;
}

/// Advances each child animation slot directly into its model coordinate.
///
/// Visits slots 1 through animationSlotCount - 1, leaving slot 0 intact.
/// Requires live GameActor animation resources and an initialized active prefix
/// within animationSlots. Each slot's trackIndex must equal its array index:
/// the direct tick reconstructs and retains that array base in the context.
/// Playback, scratch and GTE requirements follow `animationTickPlayerSlot`.
static void _playerActorTickDirectChildSlots(Task* task)
{
    enum { PLAYER_ACTOR_DIRECT_FIRST_CHILD_SLOT = 1 };
    GameActor* actor;
    s32        slotIndex;

    actor     = task->work;
    slotIndex = PLAYER_ACTOR_DIRECT_FIRST_CHILD_SLOT;
    if (slotIndex < actor->animationSlotCount) {
        do {
            animationTickPlayerSlot(&actor->animationContext, actor->animationSlots + slotIndex);
            slotIndex++;
        } while (slotIndex < actor->animationSlotCount);
    }
}

void playerActorSetPendingDisplacement(const VECTOR3* displacement)
{
    GameActor* actor;

    actor                         = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    actor->pendingDisplacement.vx = displacement->vx;
    actor->pendingDisplacement.vy = displacement->vy;
    actor->pendingDisplacement.vz = displacement->vz;
}

s32 playerActorSpawnWeaponImpact(const WorldCollisionContact* contacts, const GfxCoord* weaponCoord, GfxCoord* impactCoordOut)
{
    enum {
        PLAYER_ACTOR_IMPACT_NO_CANDIDATE        = 0x7FFFFFFF,
        PLAYER_ACTOR_IMPACT_JITTER_MASK         = 7,
        PLAYER_ACTOR_IMPACT_JAVELIN_WEAPON_ID   = 0x1D,
        PLAYER_ACTOR_IMPACT_FIRE_AMMUNITION_ROW = 0xE,
        PLAYER_ACTOR_IMPACT_FIRE_SIZE           = 0x300,
        PLAYER_ACTOR_IMPACT_SMOKE_ARGUMENT      = 0xC0013300,
    };
    s32                             nearestDistance;
    s32                             surfaceClass;
    PlayerActorWeaponImpactScratch* scratch;
    const WorldCollisionContact*    contact;
    s32                             contactIndex;
    s32                             impactMarked;
    s32                             nearestContactIndex;
    s32                             distance;

    nearestDistance = PLAYER_ACTOR_IMPACT_NO_CANDIDATE;
    if (worldCollisionCountContactsByKind(contacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
        return 0;
    }
    scratch = SCRATCH_STACK_RESERVE_BLOCK(PlayerActorWeaponImpactScratch);
    for (contactIndex = 0, nearestContactIndex = 0; contactIndex < ARRAY_SIZE(((GameActor*)0)->weaponContacts); contactIndex++) {
        contact = &contacts[contactIndex];
        if (contact->key.value & WORLD_COLLISION_CONTACT_GRID) {
            distance  = abs(weaponCoord->workm.t[0] - contact->point.vx);
            distance += abs(weaponCoord->workm.t[1] - contact->point.vy);
            distance += abs(weaponCoord->workm.t[2] - contact->point.vz);
            if (distance < nearestDistance) {
                worldCollisionResolveResponsePushback(contact, &scratch->pushback, 1, &surfaceClass);
                surfaceClass = worldCollisionSurfaceClassFromMask((const u8*)&surfaceClass);
                if (Gp_RoomParamTables[gGameSession->location.loc.stage - 1][gGameSession->location.loc.area - 1][surfaceClass]->weaponImpactEnabled != WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS) {
                    nearestDistance     = distance;
                    nearestContactIndex = contactIndex;
                }
            }
        }
    }
    // Report the jittered contact point; the retained scratch rotation also affects effect placement.
    if (nearestDistance != PLAYER_ACTOR_IMPACT_NO_CANDIDATE) {
        impactMarked                      = 1;
        scratch->impactCoord.parent       = NULL;
        scratch->impactCoord.composeStamp = GRAPHICS_COORD_SUPPLIED_CACHE;
        scratch->impactCoord.workm.t[0]   = contacts[nearestContactIndex].point.vx;
        scratch->impactCoord.workm.t[1]   = contacts[nearestContactIndex].point.vy;
        scratch->impactCoord.workm.t[2]   = contacts[nearestContactIndex].point.vz;
        scratch->jitter.vx                = rand() & PLAYER_ACTOR_IMPACT_JITTER_MASK;
        scratch->jitter.vy                = rand() & PLAYER_ACTOR_IMPACT_JITTER_MASK;
        scratch->jitter.vz                = rand() & PLAYER_ACTOR_IMPACT_JITTER_MASK;
        if (impactCoordOut != NULL) {
            impactCoordOut->workm.t[0] = scratch->impactCoord.workm.t[0] + scratch->jitter.vx;
            impactCoordOut->workm.t[1] = scratch->impactCoord.workm.t[1] + scratch->jitter.vy;
            impactCoordOut->workm.t[2] = scratch->impactCoord.workm.t[2] + scratch->jitter.vz;
        }
        if (gPlayerStatus.weapon != PLAYER_ACTOR_IMPACT_JAVELIN_WEAPON_ID) {
            if (gPlayerStatus.weaponSlotItem == PLAYER_ACTOR_IMPACT_FIRE_AMMUNITION_ROW) {
                effectSpawn(EFFECT_FIRE_BURST, &scratch->impactCoord, PLAYER_ACTOR_IMPACT_FIRE_SIZE, &scratch->jitter);
                effectSpawn(EFFECT_ADDITIVE_PUFF, &scratch->impactCoord, PLAYER_ACTOR_IMPACT_FIRE_SIZE, &scratch->jitter);
                effectSpawn(EFFECT_SMOKE_PUFF, &scratch->impactCoord, PLAYER_ACTOR_IMPACT_SMOKE_ARGUMENT, &scratch->jitter);
            } else {
                effectSpawn(EFFECT_IMPACT_SPARK, &scratch->impactCoord, 0, &scratch->jitter);
            }
        }
    } else {
        impactMarked = 0;
    }
    SCRATCH_STACK_RELEASE_BLOCK(PlayerActorWeaponImpactScratch);
    return impactMarked;
}

s32 playerActorPlayFootstepCue(Task* task)
{
    // Actor mode is the low halfword and state the high halfword of this selector.
    enum {
        PLAYER_ACTOR_FOOTSTEP_STAIR_CLIMB_MODE_STATE = (3 << 16) | GAME_ACTOR_MODE_SCRIPTED
    };
    enum { PLAYER_ACTOR_FOOTSTEP_RUNNING_MOVEMENT_MODE = 3 };
    // The loaded surface-soundEvent bank keeps companion entries 100 slots after the player's.
    enum { PLAYER_ACTOR_FOOTSTEP_COMPANION_ENTRY_OFFSET = 100 };

    enum {
        PLAYER_ACTOR_FOOTSTEP_PLAYER_CUE2_PART    = 18,
        PLAYER_ACTOR_FOOTSTEP_COMPANION_CUE2_PART = 19,
        PLAYER_ACTOR_FOOTSTEP_CUE1_PART_OFFSET    = 3,
        // Size 0x300, two ticks per sprite frame, with optional child puffs.
        PLAYER_ACTOR_FOOTSTEP_DUST_ARGUMENT = (s32)0x80000000 | (2 << 12) | 0x300,
    };

    GameActor*                          actor;
    const AnimationRecord*              record;
    GfxCoord*                           audioCoord;
    s32                                 soundEvent;
    s8                                  cueBits;
    s32                                 audioPan;
    s32                                 footPartIndex;
    const WorldCollisionFootstepSounds* footstepSounds;

    soundEvent = WORLD_COLLISION_FOOTSTEP_SILENT;
    actor      = task->work;
    audioCoord = task->extra.tmd->coords + 1;
    record     = animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1);
    // A cue is consumed once per record identity, including silent surfaces.
    if (record != NULL && record != actor->lastCueRecord) {
        actor->lastCueRecord = record;
        switch (cueBits = record->flags & ANIMATION_RECORD_CUE_MASK) {
            case ANIMATION_RECORD_CUE_1:
            case ANIMATION_RECORD_CUE_2:
                footstepSounds = Gp_RoomParamTables[gGameSession->location.loc.stage - 1][gGameSession->location.loc.area - 1][actor->surfaceClass]->footstepSounds;
                if (footstepSounds != NULL) {
                    if (*(s32*)&actor->mode == PLAYER_ACTOR_FOOTSTEP_STAIR_CLIMB_MODE_STATE) {
                        soundEvent = footstepSounds->scriptedJump;
                    } else if ((u16)actor->movementMode == PLAYER_ACTOR_FOOTSTEP_RUNNING_MOVEMENT_MODE) {
                        soundEvent = footstepSounds->run;
                        sceneLatchActionSignal(SCENE_COMBAT_ACTION_SIGNAL_FOOTSTEP);
                    } else {
                        soundEvent = footstepSounds->walk;
                    }
                    // Select the paired cue and companion entry only for a non-silent base.
                    if (soundEvent != WORLD_COLLISION_FOOTSTEP_SILENT) {
                        if (cueBits == ANIMATION_RECORD_CUE_1) {
                            soundEvent++;
                        }
                        if (actor->companionWork != NULL) {
                            soundEvent += PLAYER_ACTOR_FOOTSTEP_COMPANION_ENTRY_OFFSET;
                        }
                        audioPan = (s8)worldCoordGetOriginAudioPan(audioCoord);
                        sndEvtRequestScriptStart(soundEvent, audioPan, (s8)worldCoordGetOriginAudioDepth(audioCoord));
                    }
                    if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
                        footPartIndex = PLAYER_ACTOR_FOOTSTEP_PLAYER_CUE2_PART;
                        if (actor->companionWork != NULL) {
                            footPartIndex = PLAYER_ACTOR_FOOTSTEP_COMPANION_CUE2_PART;
                        }
                        if (cueBits == ANIMATION_RECORD_CUE_1) {
                            footPartIndex -= PLAYER_ACTOR_FOOTSTEP_CUE1_PART_OFFSET;
                        }
                        effectSpawn(EFFECT_DUST_PUFF, &task->extra.tmd->coords[footPartIndex], PLAYER_ACTOR_FOOTSTEP_DUST_ARGUMENT, NULL);
                    }
                }
                break;
        }
    }
    return soundEvent;
}

s8 playerActorReadAttackButton(Task* task)
{
    GameActor* actor;
    u16        mode;
    s32        heldButtons;
    s32        primaryMask;
    s32        secondaryMask;

    actor = task->work;
    mode  = actor->mode;
    if (mode != GAME_ACTOR_MODE_SCRIPTED) {
        heldButtons   = actor->padHeld;
        primaryMask   = PAD_BUTTON_R1;
        secondaryMask = PAD_BUTTON_R2;
    } else {
        heldButtons = gPadStates[0].buttons;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout == mode) {
            primaryMask   = PAD_BUTTON_SQUARE;
            secondaryMask = PAD_BUTTON_TRIANGLE;
        } else {
            primaryMask   = PAD_BUTTON_R1;
            secondaryMask = PAD_BUTTON_R2;
        }
    }
    actor->attackButton = PLAYER_ACTOR_ATTACK_BUTTON_NONE;
    if (heldButtons & primaryMask) {
        actor->attackButton = PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY;
    } else if (heldButtons & secondaryMask) {
        actor->attackButton = PLAYER_ACTOR_ATTACK_BUTTON_SECONDARY;
    }
    return actor->attackButton;
}

/// Per-weapon handlers, indexed by `PlayerStatus::weapon` and copied by
/// `_playerActorDispatchWeaponAttack`. Most live in the weapon overlay loaded at the time;
/// `_playerActorNoWeaponAttack` serves the weapons with none.
static const _PlayerActorWeaponAttacks D_800978BC = { {
    _playerActorNoWeaponAttack,
    func_p08_snail_8011D1D8,
    m93rAttackState,
    m950AttackState,
    p08AttackState,
    p229AttackState,
    _playerActorNoWeaponAttack,
    _playerActorNoWeaponAttack,
    _playerActorNoWeaponAttack,
    func_mongoose_8011D1D8,
    _playerActorNoWeaponAttack,
    grenadePistolAttackState,
    mm1AttackState,
    pa3AttackState,
    func_sp12_8011D1DC,
    as12AttackState,
    m4a1AttackState,
    m249AttackState,
    _playerActorNoWeaponAttack,
    tonfaBatonAttackState,
    func_m4a1_p1_8011D1C4,
    func_m4a1_p2_8011D1C4,
    hypervelocityAttackState,
    gunbladeAttackState,
    _playerActorNoWeaponAttack,
    m4a1HammerAttackState,
    m4a1BayonetAttackState,
    m4a1GrenadeAttackState,
    m4a1PykeAttackState,
    m4a1JavelinAttackState,
    mp5a5AttackState,
    func_mp5a5_p1_8011DDA4,
    func_mp5a5_p2_8011DDA4,
} };

/// Runs the equipped weapon's attack handler, both on entry and on subsequent attack ticks.
///
/// Requires live GameActor work, a weapon index in 0..32 and that weapon's
/// overlay loaded. Stops forward/backward movement and installs the logical
/// buttons allowed to interrupt a completed attack after its cancel delay.
static void _playerActorDispatchWeaponAttack(Task* task)
{
    enum {
        PLAYER_ACTOR_ATTACK_CANCEL_BUTTONS = PAD_BUTTON_UP | PAD_BUTTON_RIGHT | PAD_BUTTON_DOWN | PAD_BUTTON_LEFT |
                                             PAD_BUTTON_START | PAD_BUTTON_SQUARE | PAD_BUTTON_TRIANGLE | PAD_BUTTON_R1 | PAD_BUTTON_R2,
    };
    GameActor*                actor;
    _PlayerActorWeaponAttacks weaponAttacks;

    weaponAttacks        = D_800978BC;
    actor                = task->work;
    actor->actionPadMask = PLAYER_ACTOR_ATTACK_CANCEL_BUTTONS;
    actor->movementSign  = 0;
    weaponAttacks.attacks[gPlayerStatus.weapon](task);
}

void playerActorUpdateWeaponCollisionKey(void)
{
    enum { PLAYER_ACTOR_COLLISION_WEAPON_SHIFT = 8 };
    GameActor* actor = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;

    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key = WORLD_COLLISION_CONTACT_ATTACK | (gPlayerStatus.weapon << PLAYER_ACTOR_COLLISION_WEAPON_SHIFT) | gPlayerStatus.weaponSlotItem;
}

void playerActorSetWeaponAttackFlags(Task* task, s32 attachmentAttack, s32 alternateAttack)
{
    enum {
        PLAYER_ACTOR_WEAPON_ATTACK_SELECTOR_SHIFT = 14,
        PLAYER_ACTOR_WEAPON_ATTACK_KEEP_MASK      = 0xFFFF3FFF,
    };
    GameActor* actor;

    actor                                              = task->work;
    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key = (actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key & PLAYER_ACTOR_WEAPON_ATTACK_KEEP_MASK) | (((attachmentAttack << 1) | alternateAttack) << PLAYER_ACTOR_WEAPON_ATTACK_SELECTOR_SHIFT);
}

s32 playerActorQueryWeaponLoads(s32 loadMask)
{
    s32 weaponItemId;
    s32 loads;

    weaponItemId = gPlayerStatus.weapon + EQUIPMENT_WEAPON_ITEM_FIRST - 1;
    loads        = 0;
    if (loadMask & PLAYER_ACTOR_WEAPON_LOAD_PRIMARY) {
        loads = equipmentConsumeWeaponLoad(weaponItemId, EQUIPMENT_WEAPON_LOAD_QUERY_PRIMARY);
    }
    if (loadMask & PLAYER_ACTOR_WEAPON_LOAD_SECONDARY) {
        loads |= equipmentConsumeWeaponLoad(weaponItemId, EQUIPMENT_WEAPON_LOAD_QUERY_SECONDARY) << PLAYER_ACTOR_WEAPON_LOAD_SECONDARY_SHIFT;
    }
    return loads;
}

/// Starts an automatic reload when the selected weapon consumable can be loaded.
///
/// Fire selector 1 chooses primary, all other values secondary; the caller uses
/// 1 or 2 after an empty-load query. Returns 1 when it enters reload, otherwise 0.
/// Requires a live player task and weapon index 1..32 for the unchecked saved
/// load lookup, plus the animation/equipment resources of `playerActorEnterReload`.
/// Eligibility tests compatible positive stock; it does not require an empty load.
static s32 _playerActorTryStartAutomaticReload(Task* task, s32 attackButton)
{
    s32 reloadStarted;
    s32 loadSelection;
    s32 weaponId;

    reloadStarted = 0;
    weaponId      = gPlayerStatus.weapon;
    loadSelection = attackButton != PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY;
    if (equipmentCanReloadSelectedWeaponConsumable(weaponId + EQUIPMENT_WEAPON_ITEM_FIRST - 1, loadSelection) == 1) {
        playerActorEnterReload(task, loadSelection, PLAYER_ACTOR_RELOAD_AUTOMATIC);
        reloadStarted = 1;
    }
    return reloadStarted;
}

void playerActorResetWeaponAttack(Task* task, s32 weaponId, s32 unusedArgument)
{
    enum {
        PLAYER_ACTOR_HYPERVELOCITY_CANCEL_CHARGE = -1,
        PLAYER_ACTOR_PYKE_GLOW_IDLE              = 1,
        PLAYER_ACTOR_PYKE_GLOW_FIRING            = 2,
        PLAYER_ACTOR_PYKE_GLOW_RESET_IDLE        = 3,
        PLAYER_ACTOR_PYKE_GLOW_RESET_OFF_SHIFT   = 2,
    };
    GameActor* actor;
    s32        pykeRequest;

    actor = task->work;
    if (weaponId == PLAYER_ACTOR_WEAPON_HYPERVELOCITY) {
        if (actor->weaponEffectTask != NULL) {
            actor->weaponEffectTask->spawnArg1.value = PLAYER_ACTOR_HYPERVELOCITY_CANCEL_CHARGE;
        }
        sndEvtRequestScriptStop(SOUND_HYPERVELOCITY_CHARGE_START, SOUND_SCRIPT_STOP_NO_FADE);
        sndEvtRequestScriptStop(SOUND_HYPERVELOCITY_CHARGE_CANCEL, SOUND_SCRIPT_STOP_NO_FADE);
        sndEvtRequestScriptStop(SOUND_HYPERVELOCITY_CHARGE_LOOP, SOUND_SCRIPT_STOP_NO_FADE);
    } else if (weaponId == PLAYER_ACTOR_WEAPON_HAMMER) {
        if (actor->weaponEffectTask != NULL) {
            if (equipmentConsumeWeaponLoad(EQUIPMENT_WEAPON_ITEM_FIRST - 1 + PLAYER_ACTOR_WEAPON_HAMMER, EQUIPMENT_WEAPON_LOAD_QUERY_SECONDARY) != 0) {
                actor->weaponEffectTask->spawnArg1.value = M4A1_HAMMER_GLOW_IDLE;
            } else {
                actor->weaponEffectTask->spawnArg1.value = M4A1_HAMMER_GLOW_OFF;
            }
        }
    } else if (weaponId == PLAYER_ACTOR_WEAPON_PYKE) {
        if (actor->weaponEffectTask != NULL) {
            if (equipmentConsumeWeaponLoad(EQUIPMENT_WEAPON_ITEM_FIRST - 1 + PLAYER_ACTOR_WEAPON_PYKE, EQUIPMENT_WEAPON_LOAD_QUERY_SECONDARY) != 0) {
                pykeRequest = PLAYER_ACTOR_PYKE_GLOW_IDLE;
                if (actor->weaponEffectTask->spawnArg1.value == PLAYER_ACTOR_PYKE_GLOW_FIRING) {
                    pykeRequest = PLAYER_ACTOR_PYKE_GLOW_RESET_IDLE;
                }
                actor->weaponEffectTask->spawnArg1.value = pykeRequest;
            } else {
                actor->weaponEffectTask->spawnArg1.value = (actor->weaponEffectTask->spawnArg1.value == PLAYER_ACTOR_PYKE_GLOW_FIRING) << PLAYER_ACTOR_PYKE_GLOW_RESET_OFF_SHIFT;
            }
            if (actor->companionWork == NULL) {
                sndEvtRequestScriptStop(SOUND_PYKE_FIRE_TAIL, SOUND_SCRIPT_STOP_NO_FADE);
            } else {
                sndEvtRequestScriptStop(SOUND_COMPANION_PYKE_FIRE_TAIL, SOUND_SCRIPT_STOP_NO_FADE);
            }
        }
    }
    // Ease the pose back and disable the weapon body after resetting its effect.
    actor->aimTrackingState                               = GAME_ACTOR_AIM_TRACKING_DECAY;
    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
}

void worldCoordPlaySound(const GfxCoord* coord, s32 soundEvent, s32 signalNoise)
{
    s32 pan;

    pan = (s8)worldCoordGetOriginAudioPan(coord);
    sndEvtRequestScriptStart(soundEvent, pan, (s8)worldCoordGetOriginAudioDepth(coord));
    if (signalNoise == 1) {
        sceneLatchActionSignal(SCENE_COMBAT_ACTION_SIGNAL_NOISE);
    }
}

void weaponRecordUse(s32 weaponId)
{
    enum { WEAPON_USE_COUNT_MAX = 99999 };

    // Weapon ids start at 1; the counts are indexed from 0.
    weaponId--;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponUseCounts[weaponId] < WEAPON_USE_COUNT_MAX) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.weaponUseCounts[weaponId]++;
    }
}

void playerActorFinishWeaponAttack(Task* task)
{
    enum { PLAYER_ACTOR_ATTACK_FINISH_BLEND_FRAMES = 3 };
    GameActor* actor = task->work;

    if (actor->aimControl & GAME_ACTOR_AIM_REQUEST_SCRIPTED) {
        _playerActorEnterScriptedAttack(task, 0, 0, 0);
    } else {
        _playerActorEnterAimLocomotion(task, PLAYER_ACTOR_ATTACK_FINISH_BLEND_FRAMES);
    }
}

/// Empty attack slot for no weapon and weapon packages without executable attack code.
static void _playerActorNoWeaponAttack(Task* unusedTask)
{
}

/// Updates normal state 0's aim, locomotion and idle-animation requests.
///
/// Requires live actor work and native child playback. Aim entry takes priority;
/// otherwise movement/run changes select locomotion, stationary turn changes
/// select a turn clip, and 300 direction-free ticks select a health-band idle
/// clip. The signed-halfword idle counter saturates at 32767 and is reset by
/// locomotion entry, not by this function when a direction is held.
static void _playerActorUpdateNormalLocomotion(Task* task)
{
    enum {
        PLAYER_ACTOR_HEALTH_IDLE_SET_FIRST    = 23,
        PLAYER_ACTOR_HEALTH_IDLE_BLEND_FRAMES = 5,
        PLAYER_ACTOR_IDLE_ANIMATION_TICKS     = 300,
        PLAYER_ACTOR_IDLE_TICKS_MAX           = 32767,
        PLAYER_ACTOR_NORMAL_AIM_BLEND_FRAMES  = 5,
    };
    GameActor* actor;

    actor = task->work;
    _playerActorUpdateAimRequest(task);
    if (actor->aimControl & GAME_ACTOR_AIM_REQUEST_ENTER) {
        actor->aimTransitionPending = true;
        playerActorEnterAim(task, PLAYER_ACTOR_NORMAL_AIM_BLEND_FRAMES);
    } else if (actor->movementSign != actor->previousMovementSign ||
               (actor->runButtonHeld != actor->previousRunButtonHeld && actor->movementSign == 1)) {
        playerActorEnterLocomotion(task, 0);
    } else if (actor->movementSign == 0 && actor->turnSign != actor->previousTurnSign) {
        _playerActorUpdateIdleTurnAnimation(task);
    } else if ((actor->padHeld & (PAD_BUTTON_UP | PAD_BUTTON_RIGHT | PAD_BUTTON_DOWN | PAD_BUTTON_LEFT)) == 0) {
        if (actor->idleTicks < PLAYER_ACTOR_IDLE_TICKS_MAX) {
            actor->idleTicks++;
            if (actor->idleTicks == PLAYER_ACTOR_IDLE_ANIMATION_TICKS) {
                playerActorPlayChildSlotsWithBlend(task, _playerActorGetIdleHealthBand() + PLAYER_ACTOR_HEALTH_IDLE_SET_FIRST,
                                                   0, PLAYER_ACTOR_HEALTH_IDLE_BLEND_FRAMES);
            }
        }
    }
}

/// Selects the ordinary forward-walk clip while installing its movement index.
///
/// Requires live GameActor work. The supplied movement index is stored with
/// signed-halfword narrowing. Equipment slot 1 selects native-bank clip 2
/// when present or clip 19 when absent. Returns the set index without examining
/// the clip table, starting playback or retaining a pointer.
static inline s32 _playerActorSelectLocomotionWalkSet(GameActor* actor, s32 movementMode)
{
    enum {
        PLAYER_ACTOR_LOCOMOTION_SET_WALK         = 2,
        PLAYER_ACTOR_LOCOMOTION_SET_UNARMED_WALK = 19,
    };
    s32 setIndex;

    setIndex            = PLAYER_ACTOR_LOCOMOTION_SET_WALK;
    actor->movementMode = movementMode;
    if (actor->equipmentTasks[1] == NULL) {
        setIndex = PLAYER_ACTOR_LOCOMOTION_SET_UNARMED_WALK;
    }
    return setIndex;
}

void playerActorEnterLocomotion(Task* task, s16 resetAnimation)
{
    enum {
        PLAYER_ACTOR_LOCOMOTION_STATE             = 0,
        PLAYER_ACTOR_LOCOMOTION_CONTROLLER        = 0,
        PLAYER_ACTOR_LOCOMOTION_STOPPED           = 0,
        PLAYER_ACTOR_LOCOMOTION_WALK              = 1,
        PLAYER_ACTOR_LOCOMOTION_BACKWARD          = 2,
        PLAYER_ACTOR_LOCOMOTION_RUN               = 3,
        PLAYER_ACTOR_LOCOMOTION_TURN_MOVING       = 1,
        PLAYER_ACTOR_LOCOMOTION_TURN_IDLE         = 3,
        PLAYER_ACTOR_LOCOMOTION_SET_IDLE          = 1,
        PLAYER_ACTOR_LOCOMOTION_SET_BACKWARD      = 3,
        PLAYER_ACTOR_LOCOMOTION_SET_RUN           = 4,
        PLAYER_ACTOR_LOCOMOTION_SET_NEGATIVE_TURN = 5,
        PLAYER_ACTOR_LOCOMOTION_SET_POSITIVE_TURN = 6,
        PLAYER_ACTOR_LOCOMOTION_BLEND_FRAMES      = 4,
    };
    GameActor* actor;
    s32        setIndex;
    s32        movement;

    actor                 = task->work;
    movement              = actor->movementSign;
    actor->state          = PLAYER_ACTOR_LOCOMOTION_STATE;
    actor->animationState = PLAYER_ACTOR_LOCOMOTION_CONTROLLER;
    // Select a native-bank clip and the matching movement/turn-rate indices.
    if (movement == 0) {
        if (actor->turnSign != 0) {
            if (actor->turnSign == 1) {
                setIndex = PLAYER_ACTOR_LOCOMOTION_SET_POSITIVE_TURN;
            } else {
                setIndex = PLAYER_ACTOR_LOCOMOTION_SET_NEGATIVE_TURN;
            }
        } else {
            setIndex = PLAYER_ACTOR_LOCOMOTION_SET_IDLE;
        }
        actor->movementMode  = PLAYER_ACTOR_LOCOMOTION_STOPPED;
        actor->turnRateIndex = PLAYER_ACTOR_LOCOMOTION_TURN_IDLE;
    } else if ((actor->padHeld & PAD_BUTTON_CROSS) && (movement != -1)) {
        movement             = PLAYER_ACTOR_LOCOMOTION_WALK;
        actor->turnRateIndex = movement;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.moveMode == 0 && actor->restrictRunAndAim == 0) {
            actor->movementMode = PLAYER_ACTOR_LOCOMOTION_RUN;
            setIndex            = PLAYER_ACTOR_LOCOMOTION_SET_RUN;
        } else {
            setIndex = _playerActorSelectLocomotionWalkSet(actor, movement);
        }
    } else {
        actor->turnRateIndex = PLAYER_ACTOR_LOCOMOTION_TURN_MOVING;
        if (actor->movementSign == 1) {
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.moveMode != 0 && actor->restrictRunAndAim == 0) {
                actor->movementMode = PLAYER_ACTOR_LOCOMOTION_RUN;
                setIndex            = PLAYER_ACTOR_LOCOMOTION_SET_RUN;
            } else {
                setIndex = _playerActorSelectLocomotionWalkSet(actor, PLAYER_ACTOR_LOCOMOTION_WALK);
            }
        } else {
            actor->movementMode = PLAYER_ACTOR_LOCOMOTION_BACKWARD;
            setIndex            = PLAYER_ACTOR_LOCOMOTION_SET_BACKWARD;
        }
    }
    actor->mode       = GAME_ACTOR_MODE_NORMAL;
    actor->statePhase = 0;
    actor->idleTicks  = 0;
    if (resetAnimation != 0) {
        playerActorResetChildSlots(task, setIndex);
    } else {
        playerActorPlayChildSlotsWithBlend(task, setIndex, 0, PLAYER_ACTOR_LOCOMOTION_BLEND_FRAMES);
    }
}

/// `mode` dispatcher: `_playerActorTickNormal`, `_playerActorTickDamage`, `_playerActorTickScripted`.
static const TaskFuncTable3 Gp_PlayerModeFns = { {
    _playerActorTickNormal,
    _playerActorTickDamage,
    _playerActorTickScripted,
} };

/// `state` dispatcher copied by `_playerActorTickNormal`.
static const TaskFuncTable8 D_8009794C = { {
    func_80108FA0,
    _playerActorNormalState1,
    _playerActorNormalState2,
    _playerActorUpdateAimExit,
    func_80109138,
    _playerActorNormalState5,
    _playerActorNormalState6,
    _playerActorUpdateParalysis,
} };

/// Saves normal control and starts a paralysis episode with weapon contacts disabled.
///
/// Borrows live player/native playback and status resources. Clears movement,
/// turn, phase and rumble state, releases lock-on and locks attachment events
/// before resetting weapon effects and blending child slots 3..5 to set 25
/// over six normal-rate frames. Requires normal state 0..6 and an equipped
/// weapon index 0..32; stateAux retains the interrupted state for recovery.
static inline void _playerActorEnterParalysis(Task* task, const PlayerStatus* status)
{
    enum {
        PLAYER_ACTOR_PARALYSIS_FIRST_SLOT   = 3,
        PLAYER_ACTOR_PARALYSIS_SET          = 25,
        PLAYER_ACTOR_PARALYSIS_BLEND_FRAMES = 6,
    };
    GameActor* actor;
    u16        previousState;

    actor                 = task->work;
    previousState         = actor->state;
    actor->mode           = GAME_ACTOR_MODE_NORMAL;
    actor->state          = PLAYER_ACTOR_NORMAL_PARALYSIS_STATE;
    actor->movementMode   = 0;
    actor->turnRateIndex  = 0;
    actor->animationState = 0;
    actor->statePhase     = 0;
    actor->rumblePosted   = 0;
    actor->movementSign   = 0;
    actor->turnSign       = 0;
    actor->stateAux       = previousState;
    playerActorClearLockTarget(task);
    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    Gp_StateC08.flags                                    |= ATTACHMENT_FLAG_EVENT_LOCK;
    playerActorResetWeaponAttack(task, status->weapon, 0);
    playerActorPlayChildSlotsWithBlend(task, PLAYER_ACTOR_PARALYSIS_SET, PLAYER_ACTOR_PARALYSIS_FIRST_SLOT, PLAYER_ACTOR_PARALYSIS_BLEND_FRAMES);
}

/// Advances normal player control, status effects, animation and movement in that order.
///
/// Requires live player GameActor/model, native playback and equipment resources;
/// state must index the eight normal handlers (0..7). Applies confusion before
/// deriving movement input. After 90 eligible paralysis ticks, saves the old
/// state and enters paralysis before dispatch; the byte counter is tested as s8.
/// Contact damage and movement precede the final zero-HP pose request.
static void _playerActorTickNormal(Task* task)
{
    enum {
        PLAYER_ACTOR_PARALYSIS_ENTRY_TICKS   = 90,
        PLAYER_ACTOR_DEATH_POSE_BLEND_FRAMES = 4,
    };
    GameActor*     actor;
    PlayerStatus*  status;
    TaskFuncTable8 states;

    states = D_8009794C;
    actor  = task->work;
    if (gPlayerStatus.statusFlags & PLAYER_STATUS_CONFUSION) {
        playerActorApplyConfusionInput(task);
    }
    if (actor->movementInputDisabled == 0) {
        _playerActorUpdateMovementInput(task);
        _playerActorUpdateTurnInput(task);
    } else {
        actor->movementSign = 0;
        actor->turnSign     = 0;
    }
    // Paralysis saves the interrupted state before disabling weapon contact.
    status = &gPlayerStatus;
    if (status->statusFlags & PLAYER_STATUS_PARALYSIS) {
        if (actor->state != PLAYER_ACTOR_NORMAL_PARALYSIS_STATE) {
            actor->paralysisProgress++;
            if ((s8)actor->paralysisProgress >= PLAYER_ACTOR_PARALYSIS_ENTRY_TICKS) {
                _playerActorEnterParalysis(task, status);
            }
        }
    }
    states.funcs[actor->state](task);
    // Status and contact damage can change control before animation and movement.
    playerStateTickStatusEffects(task);
    playerActorCheckContactDamage(task);
    playerActorTickAnimationState(task);
    playerActorTickChildSlots(task);
    playerActorUpdateFacing(task);
    playerActorStepMovement(task);
    if (gPlayerStatus.hp <= 0) {
        playerActorEnterStoppedPose(task, PLAYER_ACTOR_DEATH_POSE_BLEND_FRAMES);
    }
}

/// Tracks aim locomotion and starts a weapon attack or automatic reload (normal state 2).
///
/// Requires live native playback, equipped weapon index 1..32 and its loaded
/// attack code. Fire input selects primary/secondary loads unless the weapon
/// queries primary loads for both inputs; a table flag permits load-free attacks.
/// A ready slot-1 record and zero cooldown gate attacks. Berserker costs 2 HP
/// before the live-HP test. Empty-fire handling requires a new fire-button edge
/// and imposes ten ticks of cooldown; footsteps are processed on every call.
static void _playerActorNormalState2(Task* task)
{
    enum {
        PLAYER_ACTOR_AIM_MOVEMENT_BLEND_FRAMES = 4,
        PLAYER_ACTOR_BERSERKER_SHOT_HP_COST    = 2,
        PLAYER_ACTOR_EMPTY_FIRE_COOLDOWN_TICKS = 10,
        PLAYER_ACTOR_EMPTY_SOUND_WEAPON_SHIFT  = 16,
        PLAYER_ACTOR_EMPTY_FIRE_SOUND          = SOUND_WEAPON(0, 1),
        PLAYER_ACTOR_AIM_DIRECTION_BUTTONS     = PAD_BUTTON_UP | PAD_BUTTON_RIGHT | PAD_BUTTON_DOWN | PAD_BUTTON_LEFT,
    };
    GameActor* actor;
    s32        loadMask;
    s32        remainingLoads;
    u8         attackButton;
    u16        pressedButtons;
    s32        soundVariantBits;
    s32        emptySoundBits;
    s32        weaponSoundBits;

    actor = task->work;
    if (actor->movementSign != actor->previousMovementSign) {
        _playerActorEnterAimLocomotion(task, PLAYER_ACTOR_AIM_MOVEMENT_BLEND_FRAMES);
    } else if (actor->movementSign == 0 && actor->turnSign != actor->previousTurnSign) {
        _playerActorUpdateAimTurnAnimation(task);
    }
    // A pending PE action takes priority over target tracking and fire input.
    if (_playerActorTryEnterPeAction(task) == 0) {
        if ((actor->padHeld & PLAYER_ACTOR_AIM_DIRECTION_BUTTONS) == 0) {
            playerActorTrackLockTarget(task);
        }
        _playerActorUpdateLockTargetFromPad(task);
        if (playerActorReadAttackButton(task) != 0 &&
            animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) != NULL &&
            actor->attackControl.cooldownTicks == 0) {
            loadMask       = D_80112EF8[gPlayerStatus.weapon] != 0 ? actor->attackButton : PLAYER_ACTOR_WEAPON_LOAD_PRIMARY;
            remainingLoads = playerActorQueryWeaponLoads(loadMask);
            if (remainingLoads > 0 ||
                (attackButton = actor->attackButton,
                 D_80112F1C[gPlayerStatus.weapon][(u8)(attackButton - PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY)] != 0)) {
                if (gPlayerStatus.statusFlags & PLAYER_STATUS_BERSERKER) {
                    playerStateApplyHpDamage(PLAYER_ACTOR_BERSERKER_SHOT_HP_COST);
                }
                if (gPlayerStatus.hp > 0) {
                    actor->aimControl = GAME_ACTOR_AIM_REQUEST_ENTER;
                    actor->statePhase = 0;
                    _playerActorDispatchWeaponAttack(task);
                }
            } else if (remainingLoads == 0) {
                pressedButtons = actor->padPressed;
                if ((s8)attackButton == PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY ? (pressedButtons & PAD_BUTTON_R1) : (pressedButtons & PAD_BUTTON_R2)) {
                    actor->attackControl.cooldownTicks = PLAYER_ACTOR_EMPTY_FIRE_COOLDOWN_TICKS;
                    if (_playerActorTryStartAutomaticReload(task, loadMask) == 0) {
                        _playerActorWriteWeaponSoundVariant(&soundVariantBits);
                        weaponSoundBits = gPlayerStatus.weapon << PLAYER_ACTOR_EMPTY_SOUND_WEAPON_SHIFT;
                        emptySoundBits  = soundVariantBits | PLAYER_ACTOR_EMPTY_FIRE_SOUND;
                        worldCoordPlaySound(task->extra.tmd->coords, weaponSoundBits | emptySoundBits, 0);
                    }
                }
            }
        }
    }
    playerActorPlayFootstepCue(task);
}

/// Consumes a changed slot-1 record identity before interpreting reload cue flags.
///
/// Both arguments repeat and must be stable pointers/lvalues without side
/// effects. Requires live actor playback; record receives the borrowed result.
/// Updates lastCueRecord even for records without cue flags. Defined only for
/// this reload driver and undefined afterward.
#define PLAYER_ACTOR_CONSUME_RELOAD_RECORD(actorWork, cueRecord)                                                           \
    (((cueRecord) = animationGetCurrentRecord(&(actorWork)->animationContext, (actorWork)->animationSlots + 1)) != NULL && \
     (cueRecord) != (actorWork)->lastCueRecord &&                                                                          \
     ((actorWork)->lastCueRecord = (cueRecord), 1))

/// Drives weapon-specific reload sounds, effects and ammunition application (normal state 5).
///
/// Requires live actor/native playback and equipment slot 1 with a model root;
/// weapon index 1..32 selects its cue pattern. actionValue retains reload source
/// (0 automatic, 1 menu, 2 battle end); stateAux is load selection (0 primary,
/// 1 secondary). Each new slot-1 record with both cue bits set is consumed once.
/// The loading cue enters phase 100 and applies the selected or pending consumable;
/// later cues can still play sounds. Animation control handles the eventual exit.
/// Before loading, Cross cancels non-battle-end reloads into aim locomotion and
/// suppresses a later reload effect. Ordinary ticks also update target selection.
static void _playerActorNormalState5(Task* task)
{
    enum {
        PLAYER_ACTOR_RELOAD_PHASE_START              = 0,
        PLAYER_ACTOR_RELOAD_PHASE_WAIT_CUE           = 1,
        PLAYER_ACTOR_MONGOOSE_RELOAD_PHASE_WAIT_LOAD = 2,
        PLAYER_ACTOR_GRENADE_RELOAD_CUE_COUNT        = 2,
        PLAYER_ACTOR_RELOAD_CANCEL_MOVEMENT_STOPPED  = 0,
        PLAYER_ACTOR_RELOAD_CANCEL_MOVEMENT_BACKWARD = 2,
        PLAYER_ACTOR_RELOAD_CANCEL_MOVEMENT_FORWARD  = 3,
        PLAYER_ACTOR_RELOAD_CANCEL_TURN_MOVING       = 1,
        PLAYER_ACTOR_RELOAD_CANCEL_TURN_IDLE         = 3,
        PLAYER_ACTOR_RELOAD_PHASE_LOADED             = 100,
        PLAYER_ACTOR_RELOAD_TIMER_EXPIRED            = -1,
        PLAYER_ACTOR_MONGOOSE_RELOAD_DELAY_TICKS     = 10,
        PLAYER_ACTOR_RELOAD_CANCEL_AIM_STATE         = 2,
        PLAYER_ACTOR_RELOAD_CANCEL_IDLE_SET          = 9,
        PLAYER_ACTOR_RELOAD_CANCEL_FORWARD_SET       = 12,
        PLAYER_ACTOR_RELOAD_CANCEL_TURN_SET          = 13,
        PLAYER_ACTOR_RELOAD_CANCEL_BLEND_FRAMES      = 6,
        PLAYER_ACTOR_RELOAD_SOUND_WEAPON_SHIFT       = 16,
        PLAYER_ACTOR_RELOAD_SOUND_VARIANT_SHIFT      = 24,
        PLAYER_ACTOR_RELOAD_SOUND_START              = SOUND_WEAPON(0, 2),
        PLAYER_ACTOR_RELOAD_SOUND_LOAD               = SOUND_WEAPON(0, 3),
        PLAYER_ACTOR_GRENADE_RELOAD_SOUND_FIRST_CUE  = SOUND_WEAPON(0, 8),
        PLAYER_ACTOR_GRENADE_RELOAD_SOUND_SECOND_CUE = SOUND_WEAPON(0, 5),
        PLAYER_ACTOR_GRENADE_RELOAD_SOUND_LOADED_CUE = SOUND_WEAPON(0, 9),
        PLAYER_ACTOR_GRENADE_RELOAD_SOUND_VARIANTS   = 3,
        PLAYER_ACTOR_RELOAD_WEAPON_M950              = 3,
        PLAYER_ACTOR_RELOAD_WEAPON_MONGOOSE          = 9,
        PLAYER_ACTOR_RELOAD_WEAPON_GRENADE_PISTOL    = 11,
        PLAYER_ACTOR_RELOAD_WEAPON_MM1               = 12,
        PLAYER_ACTOR_RELOAD_WEAPON_PA3               = 13,
        PLAYER_ACTOR_RELOAD_WEAPON_SP12              = 14,
        PLAYER_ACTOR_RELOAD_WEAPON_AS12              = 15,
        PLAYER_ACTOR_RELOAD_WEAPON_M4A1              = 16,
        PLAYER_ACTOR_RELOAD_WEAPON_M249              = 17,
        PLAYER_ACTOR_RELOAD_WEAPON_TONFA_BATON       = 19,
        PLAYER_ACTOR_RELOAD_WEAPON_M4A1_P1           = 20,
        PLAYER_ACTOR_RELOAD_WEAPON_M4A1_P2           = 21,
        PLAYER_ACTOR_RELOAD_WEAPON_M4A1_BAYONET      = 26,
        PLAYER_ACTOR_RELOAD_WEAPON_M4A1_GRENADE      = 27,
        PLAYER_ACTOR_RELOAD_WEAPON_M4A1_JAVELIN      = 29,
    };
    GameActor*             actor;
    GameActor*             aimActor;
    const AnimationRecord* record;
    GfxCoord*              weaponCoord;
    s32                    weaponSoundBits;
    s32                    loadCompleted;
    s32                    setIndex;
    s32                    turnRateIndex;
    s32                    movementSign;
    s32                    suppressEffect;
    s32                    cueSoundBits;
    s32                    ticksRemaining;
    s32                    phase;
    u8                     consumableItemId;
    u16                    nextPhase;
    s32                    soundBits;

    actor               = task->work;
    loadCompleted       = 0;
    weaponCoord         = actor->equipmentTasks[1]->extra.tmd->coords;
    weaponSoundBits     = gPlayerStatus.weapon << PLAYER_ACTOR_RELOAD_SOUND_WEAPON_SHIFT;
    actor->movementSign = 0;
    _playerActorWriteWeaponSoundVariant(&soundBits);
    if (actor->actionValue != PLAYER_ACTOR_RELOAD_BATTLE_END && (actor->padPressed & PAD_BUTTON_CROSS) && actor->statePhase != PLAYER_ACTOR_RELOAD_PHASE_LOADED) {
        actor->reloadEffectSuppressed = 1;
        aimActor                      = task->work;
        aimActor->mode                = GAME_ACTOR_MODE_NORMAL;
        aimActor->state               = PLAYER_ACTOR_RELOAD_CANCEL_AIM_STATE;
        aimActor->movementMode        = PLAYER_ACTOR_RELOAD_CANCEL_MOVEMENT_STOPPED;
        if (aimActor->movementSign != 0) {
            turnRateIndex = PLAYER_ACTOR_RELOAD_CANCEL_TURN_MOVING;
        } else {
            turnRateIndex = PLAYER_ACTOR_RELOAD_CANCEL_TURN_IDLE;
        }
        aimActor->turnRateIndex  = turnRateIndex;
        aimActor->animationState = 0;
        aimActor->statePhase     = PLAYER_ACTOR_RELOAD_PHASE_START;
        if (gPlayerStatus.statusFlags & PLAYER_STATUS_DARKNESS) {
            playerActorClearLockTarget(task);
            aimActor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
        } else {
            aimActor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_TARGET;
        }
        movementSign = aimActor->movementSign;
        if (movementSign == 0) {
            if (aimActor->turnSign != 0) {
                setIndex = PLAYER_ACTOR_RELOAD_CANCEL_TURN_SET;
            } else {
                setIndex = PLAYER_ACTOR_RELOAD_CANCEL_IDLE_SET;
            }
        } else if (movementSign == 1) {
            setIndex                   = PLAYER_ACTOR_RELOAD_CANCEL_FORWARD_SET;
            aimActor->movementMode     = PLAYER_ACTOR_RELOAD_CANCEL_MOVEMENT_FORWARD;
            aimActor->aimTrackingState = movementSign;
        } else {
            aimActor->movementMode = PLAYER_ACTOR_RELOAD_CANCEL_MOVEMENT_BACKWARD;
            setIndex               = PLAYER_ACTOR_RELOAD_CANCEL_TURN_SET;
        }
        playerActorPlayChildSlotsWithBlend(task, setIndex, 0, PLAYER_ACTOR_RELOAD_CANCEL_BLEND_FRAMES);
        return;
    }

    // Weapons use distinct cue sequences; only their loading cues apply ammunition.
    switch (gPlayerStatus.weapon) {
        case PLAYER_ACTOR_RELOAD_WEAPON_M950:
        case PLAYER_ACTOR_RELOAD_WEAPON_M249:
            if (PLAYER_ACTOR_CONSUME_RELOAD_RECORD(actor, record)) {
                if ((record->flags & ANIMATION_RECORD_CUE_MASK) == ANIMATION_RECORD_CUE_MASK) {
                    if (actor->statePhase == PLAYER_ACTOR_RELOAD_PHASE_START) {
                        actor->statePhase = PLAYER_ACTOR_RELOAD_PHASE_WAIT_CUE;
                        worldCoordPlaySound(task->extra.tmd->coords, weaponSoundBits | PLAYER_ACTOR_RELOAD_SOUND_START, 0);
                    } else {
                        worldCoordPlaySound(task->extra.tmd->coords, weaponSoundBits | PLAYER_ACTOR_RELOAD_SOUND_LOAD, 0);
                        loadCompleted     = 1;
                        actor->statePhase = PLAYER_ACTOR_RELOAD_PHASE_LOADED;
                    }
                }
            }
            break;
        case PLAYER_ACTOR_RELOAD_WEAPON_MONGOOSE:
            switch (actor->statePhase) {
                case PLAYER_ACTOR_RELOAD_PHASE_START:
                    actor->statePhase = PLAYER_ACTOR_RELOAD_PHASE_WAIT_CUE;
                    actor->stateTimer = PLAYER_ACTOR_MONGOOSE_RELOAD_DELAY_TICKS;
                    /* fallthrough */
                case PLAYER_ACTOR_RELOAD_PHASE_WAIT_CUE:
                    ticksRemaining    = actor->stateTimer - 1;
                    actor->stateTimer = ticksRemaining;
                    if (ticksRemaining == PLAYER_ACTOR_RELOAD_TIMER_EXPIRED) {
                        actor->statePhase += 1;
                        worldCoordPlaySound(task->extra.tmd->coords, weaponSoundBits | PLAYER_ACTOR_RELOAD_SOUND_START, 0);
                        if (actor->reloadEffectSuppressed == 0) {
                            effectSpawn(EFFECT_RELOAD_CASINGS_DROP, weaponCoord, 0, NULL);
                        }
                    }
                    break;
                case PLAYER_ACTOR_MONGOOSE_RELOAD_PHASE_WAIT_LOAD:
                case PLAYER_ACTOR_RELOAD_PHASE_LOADED:
                    if (PLAYER_ACTOR_CONSUME_RELOAD_RECORD(actor, record)) {
                        if ((record->flags & ANIMATION_RECORD_CUE_MASK) == ANIMATION_RECORD_CUE_MASK) {
                            worldCoordPlaySound(task->extra.tmd->coords, weaponSoundBits | PLAYER_ACTOR_RELOAD_SOUND_LOAD, 0);
                            loadCompleted                 = 1;
                            actor->reloadEffectSuppressed = 0;
                            actor->statePhase             = PLAYER_ACTOR_RELOAD_PHASE_LOADED;
                        }
                    }
                    break;
            }
            break;
        case PLAYER_ACTOR_RELOAD_WEAPON_GRENADE_PISTOL:
            if (actor->statePhase == PLAYER_ACTOR_RELOAD_PHASE_START) {
                actor->statePhase = PLAYER_ACTOR_RELOAD_PHASE_WAIT_CUE;
                cueSoundBits      = PLAYER_ACTOR_RELOAD_SOUND_START;
                worldCoordPlaySound(task->extra.tmd->coords, weaponSoundBits | (soundBits | cueSoundBits), 0);
            } else {
                if (PLAYER_ACTOR_CONSUME_RELOAD_RECORD(actor, record)) {
                    if ((record->flags & ANIMATION_RECORD_CUE_MASK) == ANIMATION_RECORD_CUE_MASK) {
                        cueSoundBits = PLAYER_ACTOR_RELOAD_SOUND_LOAD;
                        worldCoordPlaySound(task->extra.tmd->coords, weaponSoundBits | (soundBits | cueSoundBits), 0);
                        loadCompleted     = 1;
                        actor->statePhase = PLAYER_ACTOR_RELOAD_PHASE_LOADED;
                    }
                }
            }
            break;
        case PLAYER_ACTOR_RELOAD_WEAPON_MM1:
            if (actor->statePhase == PLAYER_ACTOR_RELOAD_PHASE_START) {
                suppressEffect    = actor->reloadEffectSuppressed;
                actor->statePhase = PLAYER_ACTOR_RELOAD_PHASE_WAIT_CUE;
                if (suppressEffect == 0) {
                    effectSpawn(EFFECT_RELOAD_EMITTER, weaponCoord, (s32)gPlayerStatus.weapon, NULL);
                }
            }
            if (PLAYER_ACTOR_CONSUME_RELOAD_RECORD(actor, record)) {
                if ((record->flags & ANIMATION_RECORD_CUE_MASK) == ANIMATION_RECORD_CUE_MASK) {
                    cueSoundBits = PLAYER_ACTOR_RELOAD_SOUND_LOAD;
                    worldCoordPlaySound(task->extra.tmd->coords, weaponSoundBits | (soundBits | cueSoundBits), 0);
                    loadCompleted                 = 1;
                    actor->reloadEffectSuppressed = 0;
                    actor->statePhase             = PLAYER_ACTOR_RELOAD_PHASE_LOADED;
                }
            }
            break;
        case PLAYER_ACTOR_RELOAD_WEAPON_PA3:
        case PLAYER_ACTOR_RELOAD_WEAPON_SP12:
        case PLAYER_ACTOR_WEAPON_GUNBLADE:
            if (PLAYER_ACTOR_CONSUME_RELOAD_RECORD(actor, record)) {
                if ((record->flags & ANIMATION_RECORD_CUE_MASK) == ANIMATION_RECORD_CUE_MASK) {
                    if (actor->statePhase == PLAYER_ACTOR_RELOAD_PHASE_START) {
                        cueSoundBits = PLAYER_ACTOR_RELOAD_SOUND_LOAD;
                        worldCoordPlaySound(task->extra.tmd->coords, weaponSoundBits | (soundBits | cueSoundBits), 0);
                        loadCompleted     = 1;
                        actor->statePhase = PLAYER_ACTOR_RELOAD_PHASE_LOADED;
                    } else {
                        cueSoundBits = PLAYER_ACTOR_RELOAD_SOUND_START;
                        worldCoordPlaySound(task->extra.tmd->coords, weaponSoundBits | (soundBits | cueSoundBits), 0);
                    }
                }
            }
            break;
        case PLAYER_ACTOR_RELOAD_WEAPON_AS12:
            if (PLAYER_ACTOR_CONSUME_RELOAD_RECORD(actor, record)) {
                if ((record->flags & ANIMATION_RECORD_CUE_MASK) == ANIMATION_RECORD_CUE_MASK) {
                    switch (actor->statePhase) {
                        case PLAYER_ACTOR_RELOAD_PHASE_START:
                            actor->statePhase = PLAYER_ACTOR_RELOAD_PHASE_WAIT_CUE;
                            cueSoundBits      = PLAYER_ACTOR_RELOAD_SOUND_LOAD;
                            worldCoordPlaySound(task->extra.tmd->coords, weaponSoundBits | (soundBits | cueSoundBits), 0);
                            break;
                        case PLAYER_ACTOR_RELOAD_PHASE_WAIT_CUE:
                            cueSoundBits = PLAYER_ACTOR_RELOAD_SOUND_LOAD;
                            worldCoordPlaySound(task->extra.tmd->coords, weaponSoundBits | (soundBits | cueSoundBits), 0);
                            loadCompleted     = 1;
                            actor->statePhase = PLAYER_ACTOR_RELOAD_PHASE_LOADED;
                            break;
                        case PLAYER_ACTOR_RELOAD_PHASE_LOADED:
                            cueSoundBits = PLAYER_ACTOR_RELOAD_SOUND_START;
                            worldCoordPlaySound(task->extra.tmd->coords, weaponSoundBits | (soundBits | cueSoundBits), 0);
                            break;
                    }
                }
            }
            break;
        case PLAYER_ACTOR_RELOAD_WEAPON_M4A1:
        case PLAYER_ACTOR_RELOAD_WEAPON_M4A1_P1:
        case PLAYER_ACTOR_RELOAD_WEAPON_M4A1_P2:
        case PLAYER_ACTOR_WEAPON_HAMMER:
        case PLAYER_ACTOR_RELOAD_WEAPON_M4A1_BAYONET:
        case PLAYER_ACTOR_WEAPON_PYKE:
        case PLAYER_ACTOR_RELOAD_WEAPON_M4A1_JAVELIN:
            if (PLAYER_ACTOR_CONSUME_RELOAD_RECORD(actor, record)) {
                if ((record->flags & ANIMATION_RECORD_CUE_MASK) == ANIMATION_RECORD_CUE_MASK) {
                    if (actor->statePhase == PLAYER_ACTOR_RELOAD_PHASE_START) {
                        actor->statePhase = PLAYER_ACTOR_RELOAD_PHASE_WAIT_CUE;
                        worldCoordPlaySound(task->extra.tmd->coords, weaponSoundBits | PLAYER_ACTOR_RELOAD_SOUND_LOAD, 0);
                    } else {
                        worldCoordPlaySound(task->extra.tmd->coords, weaponSoundBits | PLAYER_ACTOR_RELOAD_SOUND_LOAD, 0);
                        loadCompleted     = 1;
                        actor->statePhase = PLAYER_ACTOR_RELOAD_PHASE_LOADED;
                    }
                }
            }
            break;
        case PLAYER_ACTOR_RELOAD_WEAPON_TONFA_BATON:
            break;
        case PLAYER_ACTOR_RELOAD_WEAPON_M4A1_GRENADE:
            if (PLAYER_ACTOR_CONSUME_RELOAD_RECORD(actor, record)) {
                if ((record->flags & ANIMATION_RECORD_CUE_MASK) == ANIMATION_RECORD_CUE_MASK) {
                    consumableItemId = equipmentGetWeaponLoad(gPlayerStatus.weapon + (EQUIPMENT_WEAPON_ITEM_FIRST - 1))->secondaryItemId;
                    if (consumableItemId - (INVENTORY_CONSUMABLE_ITEM_FIRST - 1) > 0) {
                        soundBits = ((consumableItemId - INVENTORY_CONSUMABLE_ITEM_FIRST) % PLAYER_ACTOR_GRENADE_RELOAD_SOUND_VARIANTS) << PLAYER_ACTOR_RELOAD_SOUND_VARIANT_SHIFT;
                    }
                    // Retain one accumulator for the variant and the full sound request.
                    soundBits = weaponSoundBits | soundBits;
                    if (actor->stateAux != 0) {
                        phase = actor->statePhase;
                        if (phase == PLAYER_ACTOR_RELOAD_PHASE_START) {
                            soundBits |= PLAYER_ACTOR_GRENADE_RELOAD_SOUND_FIRST_CUE;
                        } else if (phase == PLAYER_ACTOR_RELOAD_PHASE_WAIT_CUE) {
                            soundBits |= PLAYER_ACTOR_GRENADE_RELOAD_SOUND_SECOND_CUE;
                        } else if (phase == PLAYER_ACTOR_RELOAD_PHASE_LOADED) {
                            soundBits |= PLAYER_ACTOR_GRENADE_RELOAD_SOUND_LOADED_CUE;
                        }
                        worldCoordPlaySound(task->extra.tmd->coords, soundBits, 0);
                    } else {
                        worldCoordPlaySound(task->extra.tmd->coords, soundBits | PLAYER_ACTOR_RELOAD_SOUND_LOAD, 0);
                    }
                    nextPhase         = actor->statePhase + 1;
                    actor->statePhase = nextPhase;
                    if (nextPhase == PLAYER_ACTOR_GRENADE_RELOAD_CUE_COUNT) {
                        loadCompleted     = 1;
                        actor->statePhase = PLAYER_ACTOR_RELOAD_PHASE_LOADED;
                    }
                }
            }
            break;
        default:
            if (actor->statePhase == PLAYER_ACTOR_RELOAD_PHASE_START) {
                actor->statePhase = PLAYER_ACTOR_RELOAD_PHASE_WAIT_CUE;
                worldCoordPlaySound(task->extra.tmd->coords, weaponSoundBits | PLAYER_ACTOR_RELOAD_SOUND_START, 0);
            } else {
                if (PLAYER_ACTOR_CONSUME_RELOAD_RECORD(actor, record)) {
                    if ((record->flags & ANIMATION_RECORD_CUE_MASK) == ANIMATION_RECORD_CUE_MASK) {
                        worldCoordPlaySound(task->extra.tmd->coords, weaponSoundBits | PLAYER_ACTOR_RELOAD_SOUND_LOAD, 0);
                        loadCompleted     = 1;
                        actor->statePhase = PLAYER_ACTOR_RELOAD_PHASE_LOADED;
                    }
                }
            }
            break;
    }
    if (loadCompleted != 0) {
        if (actor->actionValue != PLAYER_ACTOR_RELOAD_AUTOMATIC) {
            equipmentLoadPendingConsumable(gPlayerStatus.weapon + (EQUIPMENT_WEAPON_ITEM_FIRST - 1), actor->stateAux);
        } else {
            equipmentReloadSelectedWeaponConsumable(gPlayerStatus.weapon + (EQUIPMENT_WEAPON_ITEM_FIRST - 1), actor->stateAux);
        }
    }
    _playerActorUpdateLockTargetFromPad(task);
}
#undef PLAYER_ACTOR_CONSUME_RELOAD_RECORD

/// Resumes normal-mode aim locomotion after a Parasite Energy action.
///
/// Stops displacement initially, resets phase/controller and chooses native
/// clip 9 (idle), 12 (forward) or 13 (backward/turn). Forward movement uses
/// speed mode 3 with aim decay, backward mode 2 with tracking. Darkness drops
/// the lock and requests decay. Zero blendFrames resets child slots; 1..2047
/// blends for whole normal-rate frames. Requires live native-bank playback.
static inline void _playerActorResumeAimLocomotion(Task* task, s32 blendFrames)
{
    enum {
        PLAYER_ACTOR_AIM_LOCOMOTION_STATE      = 2,
        PLAYER_ACTOR_AIM_LOCOMOTION_CONTROLLER = 0,
        PLAYER_ACTOR_AIM_LOCOMOTION_STOPPED    = 0,
        PLAYER_ACTOR_AIM_LOCOMOTION_BACKWARD   = 2,
        PLAYER_ACTOR_AIM_LOCOMOTION_FORWARD    = 3,
        PLAYER_ACTOR_AIM_TURN_MOVING           = 1,
        PLAYER_ACTOR_AIM_TURN_IDLE             = 3,
        PLAYER_ACTOR_AIM_SET_IDLE              = 9,
        PLAYER_ACTOR_AIM_SET_FORWARD           = 12,
        PLAYER_ACTOR_AIM_SET_BACKWARD_OR_TURN  = 13,
    };
    GameActor* actor;
    s32        setIndex;
    s32        movementSign;
    s32        turnRateIndex;

    actor               = task->work;
    actor->mode         = GAME_ACTOR_MODE_NORMAL;
    actor->state        = PLAYER_ACTOR_AIM_LOCOMOTION_STATE;
    actor->movementMode = PLAYER_ACTOR_AIM_LOCOMOTION_STOPPED;
    if (actor->movementSign != 0) {
        turnRateIndex = PLAYER_ACTOR_AIM_TURN_MOVING;
    } else {
        turnRateIndex = PLAYER_ACTOR_AIM_TURN_IDLE;
    }
    actor->turnRateIndex  = turnRateIndex;
    actor->animationState = PLAYER_ACTOR_AIM_LOCOMOTION_CONTROLLER;
    actor->statePhase     = 0;
    if (gPlayerStatus.statusFlags & PLAYER_STATUS_DARKNESS) {
        playerActorClearLockTarget(task);
        actor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
    } else {
        actor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_TARGET;
    }
    movementSign = actor->movementSign;
    if (movementSign == 0) {
        if (actor->turnSign != 0) {
            setIndex = PLAYER_ACTOR_AIM_SET_BACKWARD_OR_TURN;
        } else {
            setIndex = PLAYER_ACTOR_AIM_SET_IDLE;
        }
    } else if (movementSign == 1) {
        setIndex                = PLAYER_ACTOR_AIM_SET_FORWARD;
        actor->movementMode     = PLAYER_ACTOR_AIM_LOCOMOTION_FORWARD;
        actor->aimTrackingState = movementSign;
    } else {
        actor->movementMode = PLAYER_ACTOR_AIM_LOCOMOTION_BACKWARD;
        setIndex            = PLAYER_ACTOR_AIM_SET_BACKWARD_OR_TURN;
    }
    if (blendFrames == 0) {
        playerActorResetChildSlots(task, setIndex);
    } else {
        playerActorPlayChildSlotsWithBlend(task, setIndex, 0, blendFrames);
    }
}

/// Selects the native animation-set index for one Parasite Energy phase.
///
/// Borrows live actor work. actionArgument is the family selected from the
/// attachment ID (0, 1 or other); the returned set index must exist in the
/// active native bank for every child track. Does not start playback.
static inline s32 _playerActorSelectPeSet(const GameActor* actor, s32 variant0Set, s32 variant1Set, s32 otherSet)
{
    s32 setIndex;

    if (actor->actionArgument == 0) {
        setIndex = variant0Set;
    } else if (actor->actionArgument == 1) {
        setIndex = variant1Set;
    } else {
        setIndex = otherSet;
    }
    return setIndex;
}

/// Runs the normal-mode Parasite Energy release, charge and cast sequence (state 6).
///
/// Requires live actor/native animation resources and the active attachment.
/// actionArgument selects clip family 0, 1 or other; stateAux holds the prior
/// normal state. Phase 0 requests release, phases 1/4 await clip completion,
/// phase 2 starts the charge clip and phase 3 waits for duration to reach zero.
/// Cancellation selects phase 5 immediately; that phase restores locomotion
/// or aiming. The animation controller advances phases 1 and 4 after playback.
static void _playerActorNormalState6(Task* task)
{
    enum {
        PLAYER_ACTOR_PE_PHASE_RELEASE           = 0,
        PLAYER_ACTOR_PE_PHASE_WAIT_RELEASE      = 1,
        PLAYER_ACTOR_PE_PHASE_CHARGE            = 2,
        PLAYER_ACTOR_PE_PHASE_WAIT_CHARGE       = 3,
        PLAYER_ACTOR_PE_PHASE_WAIT_CAST         = 4,
        PLAYER_ACTOR_PE_PHASE_RESUME            = 5,
        PLAYER_ACTOR_PE_ANIMATION_IDLE          = 0,
        PLAYER_ACTOR_PE_ANIMATION_ADVANCE_PHASE = 9,
        PLAYER_ACTOR_PE_RELEASE_SET_VARIANT0    = 26,
        PLAYER_ACTOR_PE_RELEASE_SET_VARIANT1    = 29,
        PLAYER_ACTOR_PE_RELEASE_SET_OTHER       = 42,
        PLAYER_ACTOR_PE_CHARGE_SET_VARIANT0     = 27,
        PLAYER_ACTOR_PE_CHARGE_SET_VARIANT1     = 30,
        PLAYER_ACTOR_PE_CHARGE_SET_OTHER        = 43,
        PLAYER_ACTOR_PE_CAST_SET_VARIANT0       = 28,
        PLAYER_ACTOR_PE_CAST_SET_VARIANT1       = 31,
        PLAYER_ACTOR_PE_CAST_SET_OTHER          = 44,
        PLAYER_ACTOR_PE_RELEASE_BLEND_FRAMES    = 6,
        PLAYER_ACTOR_PE_PREVIOUS_LOCOMOTION     = 0,
        PLAYER_ACTOR_PE_PREVIOUS_AIM_ENTRY      = 1,
        PLAYER_ACTOR_PE_AIM_ENTRY_BLEND_FRAMES  = 6,
        PLAYER_ACTOR_PE_AIM_BLEND_FRAMES        = 8,
    };
    GameActor* actor;
    s32        setIndex;
    s32        actionSignal;

    actor               = task->work;
    actor->movementSign = 0;
    if (Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_CANCELLED) {
        actor->statePhase = PLAYER_ACTOR_PE_PHASE_RESUME;
    }
    switch (actor->statePhase) {
        case PLAYER_ACTOR_PE_PHASE_RELEASE:
            // Start the attachment countdown while the release clip plays.
            actor->animationState = PLAYER_ACTOR_PE_ANIMATION_ADVANCE_PHASE;
            actor->statePhase    += 1;
            Gp_StateC08.flags    |= ATTACHMENT_FLAG_RELEASE;
            setIndex              = _playerActorSelectPeSet(actor, PLAYER_ACTOR_PE_RELEASE_SET_VARIANT0, PLAYER_ACTOR_PE_RELEASE_SET_VARIANT1, PLAYER_ACTOR_PE_RELEASE_SET_OTHER);
            playerActorPlayChildSlotsWithBlend(task, setIndex, 0, PLAYER_ACTOR_PE_RELEASE_BLEND_FRAMES);
            sceneLatchActionSignal(SCENE_COMBAT_ACTION_SIGNAL_PE_ACTIVE);
            break;
        case PLAYER_ACTOR_PE_PHASE_CHARGE:
            actor->animationState = PLAYER_ACTOR_PE_ANIMATION_IDLE;
            actor->statePhase    += 1;
            setIndex              = _playerActorSelectPeSet(actor, PLAYER_ACTOR_PE_CHARGE_SET_VARIANT0, PLAYER_ACTOR_PE_CHARGE_SET_VARIANT1, PLAYER_ACTOR_PE_CHARGE_SET_OTHER);
            playerActorResetChildSlots(task, setIndex);
            // Check the countdown on the same tick that starts the charge clip.
        case PLAYER_ACTOR_PE_PHASE_WAIT_CHARGE:
            if (Gp_StateC08.duration == 0) {
                actor->animationState = PLAYER_ACTOR_PE_ANIMATION_ADVANCE_PHASE;
                actor->statePhase    += 1;
                actionSignal          = SCENE_COMBAT_ACTION_SIGNAL_PE_CAST_300_TO_600;
                if (Gp_StateC08.attachId < ATTACHMENT_ID_EARLY_SPELL_LIMIT || Gp_StateC08.attachId > ATTACHMENT_ID_LAST_SPELL) {
                    actionSignal = SCENE_COMBAT_ACTION_SIGNAL_PE_CAST_OTHER;
                }
                sceneLatchActionSignal(actionSignal);
                setIndex = _playerActorSelectPeSet(actor, PLAYER_ACTOR_PE_CAST_SET_VARIANT0, PLAYER_ACTOR_PE_CAST_SET_VARIANT1, PLAYER_ACTOR_PE_CAST_SET_OTHER);
                playerActorResetChildSlots(task, setIndex);
                break;
            }
            // An unfinished charge keeps publishing the active PE stimulus.
        case PLAYER_ACTOR_PE_PHASE_WAIT_RELEASE:
            sceneLatchActionSignal(SCENE_COMBAT_ACTION_SIGNAL_PE_ACTIVE);
            break;
        case PLAYER_ACTOR_PE_PHASE_WAIT_CAST:
            break;
        case PLAYER_ACTOR_PE_PHASE_RESUME:
            if (actor->stateAux == PLAYER_ACTOR_PE_PREVIOUS_LOCOMOTION) {
                playerActorEnterLocomotion(task, 0);
                break;
            }
            _playerActorResumeAimLocomotion(task, actor->stateAux == PLAYER_ACTOR_PE_PREVIOUS_AIM_ENTRY ? PLAYER_ACTOR_PE_AIM_ENTRY_BLEND_FRAMES : PLAYER_ACTOR_PE_AIM_BLEND_FRAMES);
            break;
    }
}

/// Runs a paralysis episode and resumes the player's previous normal control (state 7).
///
/// Stops forward movement. Escape requires fifteen ticks with any newly pressed
/// direction or face button; simultaneous buttons count once. Clearing Paralysis
/// ends the episode immediately, including an unsigned counter wrap on that tick.
/// A saved state 0 resumes locomotion; every other state resumes aim locomotion
/// with a six-frame blend, retaining Darkness handling and the current turn input.
/// Requires live actor, session/equipment and native animation resources.
static void _playerActorUpdateParalysis(Task* task)
{
    enum {
        PLAYER_ACTOR_PARALYSIS_PHASE_START         = 0,
        PLAYER_ACTOR_PARALYSIS_PHASE_WAIT          = 1,
        PLAYER_ACTOR_PARALYSIS_ESCAPE_PRESSES      = 15,
        PLAYER_ACTOR_PARALYSIS_PREVIOUS_LOCOMOTION = 0,
        PLAYER_ACTOR_PARALYSIS_RESUME_BLEND_FRAMES = 6,
        PLAYER_ACTOR_PARALYSIS_ESCAPE_BUTTONS      = PAD_BUTTON_UP | PAD_BUTTON_RIGHT | PAD_BUTTON_DOWN | PAD_BUTTON_LEFT |
                                                PAD_BUTTON_TRIANGLE | PAD_BUTTON_CIRCLE | PAD_BUTTON_CROSS | PAD_BUTTON_SQUARE,
    };
    GameActor* actor;
    s32        waitingPhase;

    actor               = task->work;
    actor->movementSign = 0;
    if (!(gPlayerStatus.statusFlags & PLAYER_STATUS_PARALYSIS)) {
        actor->statePhase        = PLAYER_ACTOR_PARALYSIS_PHASE_WAIT;
        actor->paralysisProgress = 0;
    }
    switch (actor->statePhase) {
        case PLAYER_ACTOR_PARALYSIS_PHASE_START:
            waitingPhase             = PLAYER_ACTOR_PARALYSIS_PHASE_WAIT;
            actor->statePhase        = waitingPhase;
            actor->paralysisProgress = PLAYER_ACTOR_PARALYSIS_ESCAPE_PRESSES;
            // Count at most one press per tick, including the entry tick.
        case PLAYER_ACTOR_PARALYSIS_PHASE_WAIT:
            if (actor->padPressed & PLAYER_ACTOR_PARALYSIS_ESCAPE_BUTTONS) {
                actor->paralysisProgress--;
            }
            if ((s8)actor->paralysisProgress > 0) {
                break;
            }
            if (actor->stateAux == PLAYER_ACTOR_PARALYSIS_PREVIOUS_LOCOMOTION) {
                playerActorEnterLocomotion(task, 0);
                break;
            }
            // Restore aim locomotion using the saved pre-paralysis state.
            _playerActorResumeAimLocomotion(task, PLAYER_ACTOR_PARALYSIS_RESUME_BLEND_FRAMES);
            break;
    }
}

/// Adds one active tick's stair velocity to the model root in its parent frame.
///
/// Borrows a live root and three readable s32 displacement components, in
/// whole game-coordinate units per active tick in the root's parent frame.
/// Each signed translation sum must fit s32. Retains neither pointer and leaves
/// the composition stamp intact; the actor frame must invalidate it before
/// using the root's cached matrix after this update.
static inline void _playerActorApplyStairVelocity(GfxCoord* rootCoord, const VECTOR3* velocity)
{
    rootCoord->coord.t[0] += velocity->vx;
    rootCoord->coord.t[1] += velocity->vy;
    rootCoord->coord.t[2] += velocity->vz;
}

/// Advances a scripted stair flight using animation footstep cues to gate displacement.
///
/// Requires live native-bank playback/root and a positive retained stair count.
/// `jumpVariant` selects ascent for zero, descent otherwise; `scriptMotion.jumpSteps`
/// is the remaining stair count. Pitch is 384 of 4096 angle units per turn.
/// The normalized forward axis is Q12 and velocity
/// is game-coordinate displacement per active tick in the root's parent frame.
/// Ascent finishes with a level stride; descent accelerates X/Z for its last step
/// while retaining pitched Y. Completion clears motion pending and blends to
/// scripted animation state 1. Reserves/releases one stair scratch block per call.
static void _playerActorTickScriptedStairClimb(Task* task)
{
    enum {
        PLAYER_ACTOR_STAIR_START_PHASE              = 0,
        PLAYER_ACTOR_STAIR_ASCEND_PHASE             = 1,
        PLAYER_ACTOR_STAIR_ASCENT_FINISH_PHASE      = 2,
        PLAYER_ACTOR_STAIR_DESCEND_PHASE            = 3,
        PLAYER_ACTOR_STAIR_PITCH                    = 0x180,
        PLAYER_ACTOR_STAIR_ASCENT_DIVISOR           = 110,
        PLAYER_ACTOR_STAIR_DESCENT_DIVISOR          = 100,
        PLAYER_ACTOR_STAIR_ASCENT_FINISH_DIVISOR    = 180,
        PLAYER_ACTOR_STAIR_DESCENT_FINISH_DIVISOR   = 58,
        PLAYER_ACTOR_STAIR_ASCENT_STRIDE_TICKS      = 10,
        PLAYER_ACTOR_STAIR_ASCENT_LAST_STRIDE_TICKS = 11,
        PLAYER_ACTOR_STAIR_ASCENT_FINISH_TICKS      = 8,
        PLAYER_ACTOR_STAIR_DESCENT_STRIDE_TICKS     = 9,
        PLAYER_ACTOR_STAIR_DESCENT_EDGE_TICKS       = 5,
        PLAYER_ACTOR_STAIR_ASCENT_FINISH_EVEN_SET   = 38,
        PLAYER_ACTOR_STAIR_ASCENT_FINISH_ODD_SET    = 39,
        PLAYER_ACTOR_STAIR_FINISH_BLEND_FRAMES      = 3,
        PLAYER_ACTOR_STAIR_IDLE_SET                 = 1,
        PLAYER_ACTOR_STAIR_IDLE_BLEND_FRAMES        = 5,
    };
    _PlayerActorStairClimbScratch* scratch;
    GameActor*                     actor;
    GfxCoord*                      rootCoord;
    s32                            stairPitch;
    s32                            strideTicks;
    s32                            setIndex;

    scratch   = SCRATCH_STACK_RESERVE_BLOCK(_PlayerActorStairClimbScratch);
    actor     = task->work;
    rootCoord = task->extra.tmd->coords;
    switch (actor->statePhase) {
        case PLAYER_ACTOR_STAIR_START_PHASE:
            // Head along the model's forward axis tilted by the flight's slope.
            scratch->pitchedMatrix = rootCoord->coord;
            stairPitch             = -PLAYER_ACTOR_STAIR_PITCH;
            if (actor->jumpVariant == 0) {
                stairPitch = PLAYER_ACTOR_STAIR_PITCH;
            }
            gfxRotMatrixX(&scratch->pitchedMatrix, stairPitch, GRAPHICS_ROTATION_COMPOSE);
            gfxReadMatrixZAxis(&scratch->pitchedMatrix, &scratch->direction);
            VectorNormalSS(&scratch->direction, &scratch->direction);
            if (actor->jumpVariant == 0) {
                actor->statePhase     = PLAYER_ACTOR_STAIR_ASCEND_PHASE;
                actor->stateTimer     = 0;
                actor->actionValue    = actor->scriptMotion.jumpSteps & 1;
                scratch->speedDivisor = PLAYER_ACTOR_STAIR_ASCENT_DIVISOR;
            } else {
                actor->statePhase     = PLAYER_ACTOR_STAIR_DESCEND_PHASE;
                actor->stateTimer     = PLAYER_ACTOR_STAIR_DESCENT_EDGE_TICKS;
                scratch->speedDivisor = PLAYER_ACTOR_STAIR_DESCENT_DIVISOR;
            }
            actor->velocity.vx = scratch->direction.vx / scratch->speedDivisor;
            actor->velocity.vy = scratch->direction.vy / scratch->speedDivisor;
            actor->velocity.vz = scratch->direction.vz / scratch->speedDivisor;
            break;
        case PLAYER_ACTOR_STAIR_ASCEND_PHASE:
            // Each non-silent cue starts a stride; cue ticks themselves do not move.
            if (playerActorPlayFootstepCue(task) != 0) {
                strideTicks = PLAYER_ACTOR_STAIR_ASCENT_STRIDE_TICKS;
                if (actor->scriptMotion.jumpSteps == 1) {
                    strideTicks = PLAYER_ACTOR_STAIR_ASCENT_LAST_STRIDE_TICKS;
                }
                actor->stateTimer = strideTicks;
            } else if (actor->stateTimer > 0) {
                actor->stateTimer--;
                if (actor->stateTimer == 0) {
                    actor->scriptMotion.jumpSteps--;
                    if (actor->scriptMotion.jumpSteps <= 0) {
                        actor->stateTimer = PLAYER_ACTOR_STAIR_ASCENT_FINISH_TICKS;
                        actor->statePhase++;
                        gfxReadMatrixZAxis(&rootCoord->coord, &scratch->direction);
                        VectorNormalSS(&scratch->direction, &scratch->direction);
                        actor->velocity.vx = (s16)(scratch->direction.vx / PLAYER_ACTOR_STAIR_ASCENT_FINISH_DIVISOR);
                        actor->velocity.vy = (s16)(scratch->direction.vy / PLAYER_ACTOR_STAIR_ASCENT_FINISH_DIVISOR);
                        setIndex           = PLAYER_ACTOR_STAIR_ASCENT_FINISH_EVEN_SET;
                        actor->velocity.vz = (s16)(scratch->direction.vz / PLAYER_ACTOR_STAIR_ASCENT_FINISH_DIVISOR);
                        if (actor->actionValue != 0) {
                            setIndex = PLAYER_ACTOR_STAIR_ASCENT_FINISH_ODD_SET;
                        }
                        playerActorPlayChildSlotsWithBlend(task, setIndex, 0, PLAYER_ACTOR_STAIR_FINISH_BLEND_FRAMES);
                    }
                }
                _playerActorApplyStairVelocity(rootCoord, &actor->velocity);
            }
            break;
        case PLAYER_ACTOR_STAIR_ASCENT_FINISH_PHASE:
            playerActorPlayFootstepCue(task);
            if (actor->stateTimer > 0) {
                actor->stateTimer--;
                _playerActorApplyStairVelocity(rootCoord, &actor->velocity);
            }
            if (playerActorIsAnimationPlaying(task, 0, 0, 0) == 0) {
            finishFlight:
                actor->scriptedMotionPending = 0;
                actor->state                 = PLAYER_ACTOR_SCRIPTED_ANIMATION_STATE;
                playerActorPlayChildSlotsWithBlend(task, PLAYER_ACTOR_STAIR_IDLE_SET, 0, PLAYER_ACTOR_STAIR_IDLE_BLEND_FRAMES);
            }
            break;
        case PLAYER_ACTOR_STAIR_DESCEND_PHASE:
            // The last descent stride levels only X/Z; pitched Y is retained.
            if (playerActorPlayFootstepCue(task) != 0) {
                if (actor->scriptMotion.jumpSteps == 1) {
                    gfxReadMatrixZAxis(&rootCoord->coord, &scratch->direction);
                    VectorNormalSS(&scratch->direction, &scratch->direction);
                    actor->velocity.vx = (s16)(scratch->direction.vx / PLAYER_ACTOR_STAIR_DESCENT_FINISH_DIVISOR);
                    actor->velocity.vz = (s16)(scratch->direction.vz / PLAYER_ACTOR_STAIR_DESCENT_FINISH_DIVISOR);
                }
                strideTicks = PLAYER_ACTOR_STAIR_DESCENT_STRIDE_TICKS;
                if (actor->scriptMotion.jumpSteps == 1) {
                    strideTicks = PLAYER_ACTOR_STAIR_DESCENT_EDGE_TICKS;
                }
                actor->stateTimer = strideTicks;
                actor->scriptMotion.jumpSteps--;
            } else if (actor->stateTimer != 0) {
                actor->stateTimer--;
                _playerActorApplyStairVelocity(rootCoord, &actor->velocity);
            } else if (actor->scriptMotion.jumpSteps == 0) {
                goto finishFlight;
            }
            break;
    }
    playerActorTickChildSlots(task);
    SCRATCH_STACK_RELEASE_BLOCK(_PlayerActorStairClimbScratch);
}

/// Steps scripted move-to yaw toward the destination and records its clamped turn.
///
/// Requires the root translation and destination in the same parent frame,
/// with XYZ in game-coordinate units. Writes all three displacement components;
/// X/Z set the heading. Angles use 4096 units per turn and the signed step is
/// clamped to +/-64 before wrapping yaw to 0..4095. Phase 0 becomes phase 1
/// within that interval, including exactly +/-64. Caller owns the writable
/// approach scratch; borrows the root and changes actor yaw/target yaw/phase.
static inline void _playerActorStepMoveToYaw(GameActor* actor, const GfxCoord* rootCoord, PlayerActorApproachScratch* scratch)
{
    enum {
        PLAYER_ACTOR_MOVE_TO_TURN_STEP   = 64,
        PLAYER_ACTOR_MOVE_TO_START_PHASE = 0,
        PLAYER_ACTOR_MOVE_TO_TURN_PHASE  = 1,
    };
    s32 turnDelta;

    scratch->targetDelta.vx       = actor->destination.vx - rootCoord->coord.t[0];
    scratch->targetDelta.vy       = actor->destination.vy - rootCoord->coord.t[1];
    scratch->targetDelta.vz       = actor->destination.vz - rootCoord->coord.t[2];
    actor->scriptMotion.targetYaw = ratan2(scratch->targetDelta.vx, scratch->targetDelta.vz);
    turnDelta                     = playerActorShortestTurn(actor->rotation.vy, actor->scriptMotion.targetYaw);
    scratch->turnStep             = turnDelta;
    if (turnDelta > PLAYER_ACTOR_MOVE_TO_TURN_STEP) {
        scratch->turnStep = PLAYER_ACTOR_MOVE_TO_TURN_STEP;
    } else if (turnDelta < -PLAYER_ACTOR_MOVE_TO_TURN_STEP) {
        scratch->turnStep = -PLAYER_ACTOR_MOVE_TO_TURN_STEP;
    } else if (actor->statePhase == PLAYER_ACTOR_MOVE_TO_START_PHASE) {
        actor->statePhase = PLAYER_ACTOR_MOVE_TO_TURN_PHASE;
    }
    actor->rotation.vy = (actor->rotation.vy + scratch->turnStep) & ACTOR_TRANSFORM_ANGLE_MASK;
}

void playerActorTickScriptedMoveTo(Task* task)
{
    enum {
        PLAYER_ACTOR_MOVE_TO_START_PHASE      = 0,
        PLAYER_ACTOR_MOVE_TO_TURN_PHASE       = 1,
        PLAYER_ACTOR_MOVE_TO_TRAVEL_PHASE     = 2,
        PLAYER_ACTOR_MOVE_TO_WALK_MODE        = 1,
        PLAYER_ACTOR_MOVE_TO_FORWARD          = 1,
        PLAYER_ACTOR_MOVE_TO_IDLE_SET         = 1,
        PLAYER_ACTOR_MOVE_TO_WALK_SET         = 2,
        PLAYER_ACTOR_MOVE_TO_UNARMED_WALK_SET = 19,
        PLAYER_ACTOR_MOVE_TO_TURN_LEFT_SET    = 5,
        PLAYER_ACTOR_MOVE_TO_TURN_RIGHT_SET   = 6,
        PLAYER_ACTOR_MOVE_TO_BLEND_FRAMES     = 5,
        PLAYER_ACTOR_MOVE_TO_ARRIVAL_LIMIT    = 105,
    };
    PlayerActorApproachScratch* scratch;
    GfxCoord*                   rootCoord;
    GameActor*                  actor;
    s32                         animationId;

    actor     = task->work;
    rootCoord = task->extra.tmd->coords;
    scratch   = SCRATCH_STACK_RESERVE_BLOCK(PlayerActorApproachScratch);
    _playerActorStepMoveToYaw(actor, rootCoord, scratch);

    // Finish the initial turn before selecting the approach clip and speed.
    switch (actor->statePhase) {
        case PLAYER_ACTOR_MOVE_TO_START_PHASE:
            actor->statePhase = PLAYER_ACTOR_MOVE_TO_TURN_PHASE;
            animationId       = PLAYER_ACTOR_MOVE_TO_TURN_RIGHT_SET;
            if (scratch->turnStep < 0) {
                animationId = PLAYER_ACTOR_MOVE_TO_TURN_LEFT_SET;
            }
            playerActorPlayChildSlots(task, animationId, 1);
            // Continue turning on the same tick that starts the turn clip.
        case PLAYER_ACTOR_MOVE_TO_TURN_PHASE:
            if (scratch->turnStep == 0) {
                actor->movementMode = PLAYER_ACTOR_MOVE_TO_WALK_MODE;
                actor->statePhase++;
                if (actor->actionArgument == 0) {
                    animationId = PLAYER_ACTOR_MOVE_TO_WALK_SET;
                    if (actor->equipmentTasks[1] == NULL) {
                        animationId = PLAYER_ACTOR_MOVE_TO_UNARMED_WALK_SET;
                    }
                } else {
                    animationId = actor->actionArgument;
                }
                playerActorPlayChildSlotsWithBlend(task, animationId, 0, PLAYER_ACTOR_MOVE_TO_BLEND_FRAMES);
            }
            break;
        case PLAYER_ACTOR_MOVE_TO_TRAVEL_PHASE:
            // Arrival is an open XZ square; destination Y does not gate it.
            if (abs(rootCoord->coord.t[0] - actor->destination.vx) < PLAYER_ACTOR_MOVE_TO_ARRIVAL_LIMIT) {
                if (abs(rootCoord->coord.t[2] - actor->destination.vz) < PLAYER_ACTOR_MOVE_TO_ARRIVAL_LIMIT) {
                    actor->scriptedMotionPending = 0;
                    actor->state                 = PLAYER_ACTOR_SCRIPTED_ANIMATION_STATE;
                    animationId                  = PLAYER_ACTOR_MOVE_TO_IDLE_SET;
                    if (actor->actionValue != 0) {
                        animationId = actor->actionValue;
                    }
                    playerActorPlayChildSlotsWithBlend(task, animationId, 0, PLAYER_ACTOR_MOVE_TO_BLEND_FRAMES);
                    break;
                }
            }
            actor->movementSign = PLAYER_ACTOR_MOVE_TO_FORWARD;
            playerActorStepMovement(task);
            playerActorPlayFootstepCue(task);
            break;
    }
    playerActorTickChildSlots(task);
    SCRATCH_STACK_RELEASE_BLOCK(PlayerActorApproachScratch);
}

/// Starts normal-mode aim exit after scripted firing loses its engaged battle.
///
/// actor must be task->work with live native playback resources. Stops movement,
/// selects the return-to-locomotion controller and blends aim-exit set 8 for six
/// whole normal-rate frames. Retains neither pointer.
/// Clears the target after starting the clip, preserving transition ordering.
static inline void _playerActorBeginScriptedAttackAimExit(Task* task, GameActor* actor)
{
    enum {
        PLAYER_ACTOR_SCRIPTED_ATTACK_AIM_EXIT_STATE     = 3,
        PLAYER_ACTOR_SCRIPTED_ATTACK_AIM_EXIT_TURN_RATE = 2,
        PLAYER_ACTOR_SCRIPTED_ATTACK_RETURN_CONTROLLER  = 4,
        PLAYER_ACTOR_SCRIPTED_ATTACK_STOPPED            = 0,
        PLAYER_ACTOR_SCRIPTED_ATTACK_AIM_EXIT_SET       = 8,
        PLAYER_ACTOR_SCRIPTED_ATTACK_AIM_EXIT_BLEND     = 6,
    };

    actor->state          = PLAYER_ACTOR_SCRIPTED_ATTACK_AIM_EXIT_STATE;
    actor->turnRateIndex  = PLAYER_ACTOR_SCRIPTED_ATTACK_AIM_EXIT_TURN_RATE;
    actor->mode           = GAME_ACTOR_MODE_NORMAL;
    actor->movementMode   = PLAYER_ACTOR_SCRIPTED_ATTACK_STOPPED;
    actor->animationState = PLAYER_ACTOR_SCRIPTED_ATTACK_RETURN_CONTROLLER;
    actor->statePhase     = 0;
    playerActorPlayChildSlotsWithBlend(task, PLAYER_ACTOR_SCRIPTED_ATTACK_AIM_EXIT_SET, 0, PLAYER_ACTOR_SCRIPTED_ATTACK_AIM_EXIT_BLEND);
    playerActorClearLockTarget(task);
}

/// Waits for fire input under scripted control, then dispatches the equipped weapon attack.
///
/// Requires live actor/playback and weapon tables/resources for index 1..32.
/// Fire selects primary or secondary loads; a load-free attack is also allowed
/// by its table flag. Empty selected loads play the weapon's empty-fire sound
/// with a 20-tick cooldown. Battle disengagement enters normal aim exit; zero
/// HP suppresses both paths. Child slots tick even when neither path runs.
static void _playerActorTickScriptedAttack(Task* task)
{
    enum {
        PLAYER_ACTOR_SCRIPTED_ATTACK_EMPTY_COOLDOWN_TICKS = 20,
        PLAYER_ACTOR_SCRIPTED_ATTACK_WEAPON_SOUND_SHIFT   = 16,
        PLAYER_ACTOR_SCRIPTED_ATTACK_EMPTY_SOUND          = 0x20000001,
    };
    s32        soundVariantBits;
    GameActor* actor;
    s32        remainingLoads;
    s32        loadMask;
    u8         attackButton;
    s32        weaponSoundBits;
    s32        emptySoundBits;

    actor = task->work;
    if (gPlayerStatus.hp > 0) {
        if (gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_ENGAGED) {
            _playerActorBeginScriptedAttackAimExit(task, actor);
        } else if (playerActorReadAttackButton(task) != 0 &&
                   animationGetCurrentRecord(&actor->animationContext,
                                             actor->animationSlots + 1) != NULL &&
                   actor->attackControl.cooldownTicks == 0) {
            loadMask       = D_80112EF8[gPlayerStatus.weapon] != 0 ? (s8)actor->attackButton : PLAYER_ACTOR_WEAPON_LOAD_PRIMARY;
            remainingLoads = playerActorQueryWeaponLoads(loadMask);
            if (remainingLoads > 0 ||
                (attackButton = actor->attackButton,
                 D_80112F1C[gPlayerStatus.weapon][(u8)(attackButton - PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY)] != 0)) {
                actor->aimControl = GAME_ACTOR_AIM_REQUEST_SCRIPTED;
                actor->statePhase = 0;
                _playerActorDispatchWeaponAttack(task);
            } else if (remainingLoads == 0) {
                _playerActorWriteWeaponSoundVariant(&soundVariantBits);
                actor->attackControl.cooldownTicks = PLAYER_ACTOR_SCRIPTED_ATTACK_EMPTY_COOLDOWN_TICKS;
                weaponSoundBits                    = gPlayerStatus.weapon << PLAYER_ACTOR_SCRIPTED_ATTACK_WEAPON_SOUND_SHIFT;
                emptySoundBits                     = soundVariantBits | PLAYER_ACTOR_SCRIPTED_ATTACK_EMPTY_SOUND;
                worldCoordPlaySound(task->extra.tmd->coords, weaponSoundBits | emptySoundBits, 0);
            }
        }
    }
    playerActorTickChildSlots(task);
}

/// Plays the deferred item-use clip, then restores normal aim locomotion.
///
/// Requires live actor/native playback and characterId 1..5. Clip 40 blends in
/// over six normal-rate frames. Once slot 1 has a record and the character rig's
/// attachment-slot playback settles or follows a control jump, aim locomotion
/// resumes over four frames. Child playback runs before contact damage on every
/// tick, including the transition tick; this state does not apply the item.
static void _playerActorTickScriptedItemUse(Task* task)
{
    enum {
        PLAYER_ACTOR_ITEM_USE_START_PHASE  = 0,
        PLAYER_ACTOR_ITEM_USE_PLAY_PHASE   = 1,
        PLAYER_ACTOR_ITEM_USE_SET          = 40,
        PLAYER_ACTOR_ITEM_USE_ENTRY_BLEND  = 6,
        PLAYER_ACTOR_ITEM_USE_RESUME_BLEND = 4,
        PLAYER_ACTOR_ITEM_USE_RIG_SLOT     = 1,
    };
    GameActor* actor;

    actor = task->work;
    switch (actor->statePhase) {
        case PLAYER_ACTOR_ITEM_USE_START_PHASE: {
            s32 playbackPhase;

            playbackPhase     = PLAYER_ACTOR_ITEM_USE_PLAY_PHASE;
            actor->statePhase = playbackPhase;
            playerActorPlayChildSlotsWithBlend(task, PLAYER_ACTOR_ITEM_USE_SET, 0, PLAYER_ACTOR_ITEM_USE_ENTRY_BLEND);
            break;
        }
        case PLAYER_ACTOR_ITEM_USE_PLAY_PHASE:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) != NULL) {
                if (playerActorIsSlotAdvancingLinearly(task, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][PLAYER_ACTOR_ITEM_USE_RIG_SLOT], 0, 0) == 0) {
                    _playerActorResumeAimLocomotion(task, PLAYER_ACTOR_ITEM_USE_RESUME_BLEND);
                }
            }
            break;
    }
    playerActorTickChildSlots(task);
    playerActorCheckContactDamage(task);
}

/// Advances the player's current control mode and its actor-tick timers.
///
/// Requires live GameActor work/model and mode 0..2. Captures the interaction
/// press before mode dispatch, decrements positive attack/recovery timers,
/// clears the pushback-heading override, then updates turn yaw and textures.
/// Recovery retains its signed-byte interpretation. The caller's hold gate
/// skips this whole tick while its outer movement/collision pass still runs.
static void _playerActorTick(Task* task)
{
    GameActor*     actor;
    TaskFuncTable3 modeHandlers;

    modeHandlers = Gp_PlayerModeFns;
    actor        = task->work;
    _playerActorCaptureInteractionPress(task);
    if (actor->attackControl.cooldownTicks > 0) {
        actor->attackControl.cooldownTicks--;
    }
    if ((s8)actor->recoveryTicks > 0) {
        actor->recoveryTicks--;
    }
    actor->usesPushbackDirection = 0;
    modeHandlers.funcs[actor->mode](task);
    _playerActorUpdateTurnYawOffset(task);
    _playerActorTickTextureSequences(task);
}

/// Completes aim entry or requests aim exit without advancing target tracking.
///
/// Requires live actor/native animation resources and a live borrowed target
/// if selected. Stops movement and samples a target candidate. An unfinished
/// battle with a candidate, engaged battle, or nonzero saved gate retains aim;
/// completed entry consumes its transition and blends into aim locomotion.
/// Without those gates, an exit request releases the lock and starts decay.
/// The saved gate's broader purpose is unproven.
static void _playerActorUpdateAimEntry(Task* task)
{
    enum { PLAYER_ACTOR_AIM_ENTRY_BLEND_FRAMES = 3 };
    GameActor*       actor;
    WorldTargetNode* targetCandidate;

/// Completes a nonzero aim-entry phase, consuming its pending target transition.
///
/// Arguments must be stable, side-effect-free pointers; all repeat. Requires
/// live actor/native playback and a live candidate when non-NULL. Captures
/// this function's three-frame blend constant; undefined after this function.
#define PLAYER_ACTOR_COMPLETE_AIM_ENTRY(playerTask, playerWork, lockCandidate)                 \
    do {                                                                                       \
        if ((playerWork)->statePhase != 0) {                                                   \
            sceneEngageBattle(1);                                                              \
            if ((playerWork)->aimTransitionPending != 0) {                                     \
                (playerWork)->aimTransitionPending = 0;                                        \
                if ((lockCandidate) != NULL) {                                                 \
                    playerActorSetLockTarget((playerTask), (lockCandidate));                   \
                }                                                                              \
            }                                                                                  \
            _playerActorEnterAimLocomotion((playerTask), PLAYER_ACTOR_AIM_ENTRY_BLEND_FRAMES); \
        }                                                                                      \
    } while (0)

    actor               = task->work;
    targetCandidate     = worldTargetFindLockNode(task);
    actor->movementSign = 0;
    if ((targetCandidate != NULL && gSceneCombatState.signals.bytes.battlePhase < SCENE_COMBAT_BATTLE_FINISHED) || gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED ||
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.field_929 != 0) {
        PLAYER_ACTOR_COMPLETE_AIM_ENTRY(task, actor, targetCandidate);
    } else {
        _playerActorUpdateAimRequest(task);
        if (actor->aimControl & GAME_ACTOR_AIM_REQUEST_EXIT) {
            actor->aimTransitionPending = 0;
            actor->aimTrackingState     = GAME_ACTOR_AIM_TRACKING_DECAY;
            playerActorClearLockTarget(task);
            playerActorExitAim(task);
        }
    }
}
#undef PLAYER_ACTOR_COMPLETE_AIM_ENTRY

/// Refreshes aim locomotion's clip when movement changes, or turning changes at rest.
///
/// Requires live player work and native-bank playback. A movement-sign change
/// reenters normal state 2 with a four-frame blend and refreshes aim tracking.
/// Otherwise only a stationary turn-sign change blends the turn clip (five
/// frames), preserving the state and tracking. Unchanged input does nothing.
/// Has no callers in this image; captured previous signs must describe the prior tick.
static void _playerActorRefreshAimLocomotionAnimation(Task* task)
{
    enum { PLAYER_ACTOR_AIM_MOVEMENT_BLEND_FRAMES = 4 };
    GameActor* actor;

    actor = task->work;
    if (actor->movementSign != actor->previousMovementSign) {
        _playerActorEnterAimLocomotion(task, PLAYER_ACTOR_AIM_MOVEMENT_BLEND_FRAMES);
    } else if (actor->movementSign == 0 && actor->turnSign != actor->previousTurnSign) {
        _playerActorUpdateAimTurnAnimation(task);
    }
}

/// Stops movement input and enters aim when the refreshed held-button request permits it.
///
/// Requires live player work, equipment/attachment state and native playback.
/// Held Square, idle attachment effects, an equipped weapon and unrestricted
/// aim request normal state 1 with a four-frame blend. Other combinations leave
/// the current state intact with an exit request. Has no callers in this image.
static void _playerActorTryEnterAim(Task* task)
{
    enum { PLAYER_ACTOR_AIM_REQUEST_BLEND_FRAMES = 4 };
    GameActor* actor;

    actor               = task->work;
    actor->movementSign = 0;
    _playerActorUpdateAimRequest(task);
    if (actor->aimControl & GAME_ACTOR_AIM_REQUEST_ENTER) {
        playerActorEnterAim(task, PLAYER_ACTOR_AIM_REQUEST_BLEND_FRAMES);
    }
}

/// Blends the stationary player's child slots into idle or the selected turn clip.
///
/// Requires the native animation bank's clips 1, 5 and 6. Leaves the actor's
/// current state index intact, stops movement, selects the idle turn rate and
/// resets the animation controller, phase and idle timer. Blends for five frames.
static void _playerActorUpdateIdleTurnAnimation(Task* task)
{
    enum {
        PLAYER_ACTOR_IDLE_TURN_STOPPED      = 0,
        PLAYER_ACTOR_IDLE_TURN_CONTROLLER   = 0,
        PLAYER_ACTOR_IDLE_TURN_RATE_INDEX   = 3,
        PLAYER_ACTOR_IDLE_TURN_SET_IDLE     = 1,
        PLAYER_ACTOR_IDLE_TURN_SET_NEGATIVE = 5,
        PLAYER_ACTOR_IDLE_TURN_SET_POSITIVE = 6,
        PLAYER_ACTOR_IDLE_TURN_BLEND_FRAMES = 5,
    };
    GameActor* actor;
    s32        setIndex;

    actor                 = task->work;
    actor->mode           = GAME_ACTOR_MODE_NORMAL;
    actor->movementMode   = PLAYER_ACTOR_IDLE_TURN_STOPPED;
    actor->turnRateIndex  = PLAYER_ACTOR_IDLE_TURN_RATE_INDEX;
    actor->animationState = PLAYER_ACTOR_IDLE_TURN_CONTROLLER;
    actor->statePhase     = 0;
    actor->idleTicks      = 0;
    if (actor->turnSign == 0) {
        setIndex = PLAYER_ACTOR_IDLE_TURN_SET_IDLE;
    } else if (actor->turnSign == 1) {
        setIndex = PLAYER_ACTOR_IDLE_TURN_SET_POSITIVE;
    } else {
        setIndex = PLAYER_ACTOR_IDLE_TURN_SET_NEGATIVE;
    }
    playerActorPlayChildSlotsWithBlend(task, setIndex, 0, PLAYER_ACTOR_IDLE_TURN_BLEND_FRAMES);
}

/// Blends aim locomotion's child tracks after a change in turn input.
///
/// Keeps the current state and aim tracking, enters normal mode and resets phase
/// and controller. Movement input selects mode 3/clip 12 forward or mode 2/clip 13
/// backward; without movement, turning uses clip 13 and rest uses clip 9.
/// Requires live actor/native animation resources. Blends for five normal-rate
/// frames and selects moving or stationary turn-rate row 1 or 3 respectively.
static void _playerActorUpdateAimTurnAnimation(Task* task)
{
    enum {
        PLAYER_ACTOR_AIM_TURN_STOPPED      = 0,
        PLAYER_ACTOR_AIM_TURN_BACKWARD     = 2,
        PLAYER_ACTOR_AIM_TURN_FORWARD      = 3,
        PLAYER_ACTOR_AIM_TURN_RATE_MOVING  = 1,
        PLAYER_ACTOR_AIM_TURN_RATE_IDLE    = 3,
        PLAYER_ACTOR_AIM_TURN_CONTROLLER   = 0,
        PLAYER_ACTOR_AIM_TURN_SET_IDLE     = 9,
        PLAYER_ACTOR_AIM_TURN_SET_FORWARD  = 12,
        PLAYER_ACTOR_AIM_TURN_SET_OTHER    = 13,
        PLAYER_ACTOR_AIM_TURN_BLEND_FRAMES = 5,
    };
    GameActor* actor;
    s32        setIndex;
    s32        movementMode;

    actor                 = task->work;
    actor->mode           = GAME_ACTOR_MODE_NORMAL;
    actor->movementMode   = PLAYER_ACTOR_AIM_TURN_STOPPED;
    actor->animationState = PLAYER_ACTOR_AIM_TURN_CONTROLLER;
    actor->statePhase     = 0;
    if (actor->movementSign != 0) {
        if (actor->movementSign == 1) {
            movementMode = PLAYER_ACTOR_AIM_TURN_FORWARD;
        } else {
            movementMode = PLAYER_ACTOR_AIM_TURN_BACKWARD;
        }
        setIndex             = PLAYER_ACTOR_AIM_TURN_SET_OTHER;
        actor->movementMode  = movementMode;
        actor->turnRateIndex = PLAYER_ACTOR_AIM_TURN_RATE_MOVING;
        if (actor->movementSign == 1) {
            setIndex = PLAYER_ACTOR_AIM_TURN_SET_FORWARD;
        }
    } else {
        setIndex             = PLAYER_ACTOR_AIM_TURN_SET_OTHER;
        actor->turnRateIndex = PLAYER_ACTOR_AIM_TURN_RATE_IDLE;
        if (actor->turnSign == 0) {
            setIndex = PLAYER_ACTOR_AIM_TURN_SET_IDLE;
        }
    }
    playerActorPlayChildSlotsWithBlend(task, setIndex, 0, PLAYER_ACTOR_AIM_TURN_BLEND_FRAMES);
}

void playerActorEnterAim(Task* task, s32 blendFrames)
{
    enum {
        PLAYER_ACTOR_AIM_ENTRY_STATE         = 1,
        PLAYER_ACTOR_AIM_ENTRY_CONTROLLER    = 2,
        PLAYER_ACTOR_AIM_ENTRY_SET           = 7,
        PLAYER_ACTOR_AIM_ENTRY_STOPPED       = 0,
        PLAYER_ACTOR_AIM_ENTRY_TURN_DISABLED = 0,
    };
    GameActor* actor;

    actor                              = task->work;
    actor->state                       = PLAYER_ACTOR_AIM_ENTRY_STATE;
    actor->mode                        = GAME_ACTOR_MODE_NORMAL;
    actor->movementMode                = PLAYER_ACTOR_AIM_ENTRY_STOPPED;
    actor->turnRateIndex               = PLAYER_ACTOR_AIM_ENTRY_TURN_DISABLED;
    actor->animationState              = PLAYER_ACTOR_AIM_ENTRY_CONTROLLER;
    actor->statePhase                  = 0;
    actor->attackControl.cooldownTicks = 0;
    if (blendFrames == 0) {
        playerActorResetChildSlots(task, PLAYER_ACTOR_AIM_ENTRY_SET);
    } else {
        playerActorPlayChildSlotsWithBlend(task, PLAYER_ACTOR_AIM_ENTRY_SET, 0, blendFrames);
    }
}

/// Enters normal-mode aim locomotion with a native-bank clip chosen from movement input.
///
/// Stops displacement initially, selects turn rate, resets controller/phase,
/// and uses idle clip 9, forward clip 12 or backward/turn clip 13. Forward
/// movement resumes in mode 3 with aim decay, backward in mode 2 with tracking.
/// Darkness releases the target and requests decay. Zero blendFrames restarts
/// the child slots directly; 1..2047 blends captured poses for whole normal-rate
/// frames. Actor, model and animation lifetimes follow child-slot playback.
static void _playerActorEnterAimLocomotion(Task* task, s32 blendFrames)
{
    enum {
        PLAYER_ACTOR_AIM_LOCOMOTION_STATE      = 2,
        PLAYER_ACTOR_AIM_LOCOMOTION_CONTROLLER = 0,
        PLAYER_ACTOR_AIM_LOCOMOTION_STOPPED    = 0,
        PLAYER_ACTOR_AIM_LOCOMOTION_BACKWARD   = 2,
        PLAYER_ACTOR_AIM_LOCOMOTION_FORWARD    = 3,
        PLAYER_ACTOR_AIM_TURN_MOVING           = 1,
        PLAYER_ACTOR_AIM_TURN_IDLE             = 3,
        PLAYER_ACTOR_AIM_SET_IDLE              = 9,
        PLAYER_ACTOR_AIM_SET_FORWARD           = 12,
        PLAYER_ACTOR_AIM_SET_BACKWARD_OR_TURN  = 13,
    };
    GameActor* actor;
    s32        setIndex;
    s32        movementSign;
    s32        turnRateIndex;

    actor               = task->work;
    actor->mode         = GAME_ACTOR_MODE_NORMAL;
    actor->state        = PLAYER_ACTOR_AIM_LOCOMOTION_STATE;
    actor->movementMode = PLAYER_ACTOR_AIM_LOCOMOTION_STOPPED;
    if (actor->movementSign != 0) {
        turnRateIndex = PLAYER_ACTOR_AIM_TURN_MOVING;
    } else {
        turnRateIndex = PLAYER_ACTOR_AIM_TURN_IDLE;
    }
    actor->turnRateIndex  = turnRateIndex;
    actor->animationState = PLAYER_ACTOR_AIM_LOCOMOTION_CONTROLLER;
    actor->statePhase     = 0;
    // Darkness releases the lock; otherwise locomotion keeps tracking the selected target.
    if (gPlayerStatus.statusFlags & PLAYER_STATUS_DARKNESS) {
        playerActorClearLockTarget(task);
        actor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
    } else {
        actor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_TARGET;
    }
    movementSign = actor->movementSign;
    if (movementSign == 0) {
        if (actor->turnSign != 0) {
            setIndex = PLAYER_ACTOR_AIM_SET_BACKWARD_OR_TURN;
        } else {
            setIndex = PLAYER_ACTOR_AIM_SET_IDLE;
        }
    } else if (movementSign == 1) {
        setIndex                = PLAYER_ACTOR_AIM_SET_FORWARD;
        actor->movementMode     = PLAYER_ACTOR_AIM_LOCOMOTION_FORWARD;
        actor->aimTrackingState = movementSign;
    } else {
        actor->movementMode = PLAYER_ACTOR_AIM_LOCOMOTION_BACKWARD;
        setIndex            = PLAYER_ACTOR_AIM_SET_BACKWARD_OR_TURN;
    }
    if (blendFrames == 0) {
        playerActorResetChildSlots(task, setIndex);
    } else {
        playerActorPlayChildSlotsWithBlend(task, setIndex, 0, blendFrames);
    }
}

void playerActorExitAim(Task* task)
{
    enum {
        PLAYER_ACTOR_AIM_EXIT_STATE        = 3,
        PLAYER_ACTOR_AIM_EXIT_STOPPED      = 0,
        PLAYER_ACTOR_AIM_EXIT_TURN_RATE    = 2,
        PLAYER_ACTOR_AIM_EXIT_CONTROLLER   = 4,
        PLAYER_ACTOR_AIM_EXIT_SET          = 8,
        PLAYER_ACTOR_AIM_EXIT_BLEND_FRAMES = 6,
    };
    GameActor* actor;

    actor                 = task->work;
    actor->state          = PLAYER_ACTOR_AIM_EXIT_STATE;
    actor->mode           = GAME_ACTOR_MODE_NORMAL;
    actor->movementMode   = PLAYER_ACTOR_AIM_EXIT_STOPPED;
    actor->turnRateIndex  = PLAYER_ACTOR_AIM_EXIT_TURN_RATE;
    actor->animationState = PLAYER_ACTOR_AIM_EXIT_CONTROLLER;
    actor->statePhase     = 0;
    playerActorPlayChildSlotsWithBlend(task, PLAYER_ACTOR_AIM_EXIT_SET, 0, PLAYER_ACTOR_AIM_EXIT_BLEND_FRAMES);
    playerActorClearLockTarget(task);
}

void playerActorEnterReload(Task* task, s32 loadSelection, s32 reloadSource)
{
    enum {
        PLAYER_ACTOR_RELOAD_STATE                    = 5,
        PLAYER_ACTOR_RELOAD_RETURN_TO_AIM_CONTROLLER = 5,
        PLAYER_ACTOR_RELOAD_FINISH_CONTROLLER        = 10,
        PLAYER_ACTOR_RELOAD_SET_PRIMARY              = 14,
        PLAYER_ACTOR_RELOAD_SET_BATTLE_END           = 20,
        PLAYER_ACTOR_RELOAD_BLEND_FRAMES             = 3,
        PLAYER_ACTOR_RELOAD_PHASE_COMPLETE           = 1000,
    };
    GameActor* actor;
    s32        setIndex;

    actor = task->work;
    if (reloadSource == PLAYER_ACTOR_RELOAD_BATTLE_END) {
        if (playerActorQueryWeaponLoads(loadSelection) != 0) {
            if (D_80112F1C[gPlayerStatus.weapon][0] == 0) {
                actor->statePhase = PLAYER_ACTOR_RELOAD_PHASE_COMPLETE;
                return;
            }
        }
        actor->animationState = PLAYER_ACTOR_RELOAD_FINISH_CONTROLLER;
        setIndex              = PLAYER_ACTOR_RELOAD_SET_BATTLE_END;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 1) {
            actor800100EnterReload(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), 0);
        }
    } else {
        if (reloadSource == PLAYER_ACTOR_RELOAD_MENU) {
            if (actor->mode == GAME_ACTOR_MODE_SCRIPTED) {
                return;
            }
        }
        actor->animationState = PLAYER_ACTOR_RELOAD_RETURN_TO_AIM_CONTROLLER;
        setIndex              = loadSelection + PLAYER_ACTOR_RELOAD_SET_PRIMARY;
    }

    // State 5 applies the selected load when its weapon-specific cue completes.
    actor->state         = PLAYER_ACTOR_RELOAD_STATE;
    actor->mode          = GAME_ACTOR_MODE_NORMAL;
    actor->movementMode  = 0;
    actor->turnRateIndex = 0;
    actor->statePhase    = 0;
    actor->stateAux      = loadSelection;
    actor->actionValue   = reloadSource;
    playerActorResetWeaponAttack(task, gPlayerStatus.weapon, 0);
    playerActorPlayChildSlotsWithBlend(task, setIndex, 0, PLAYER_ACTOR_RELOAD_BLEND_FRAMES);
}

/// Selects the player's release/charge/cast clip family from the current attachment id.
///
/// Writes `actionArgument` (0 sets 26..28, 1 sets 29..31, 2 sets 42..44).
/// Each family supplies release, charge and cast clips, in that order.
/// Items select 0 for tens digit 1 and 1 otherwise; spells select 2 for digit 3,
/// otherwise 1 below 300 and 0 at or above it.
/// Borrows live player work and attachment state without starting playback.
static inline void _playerActorChoosePeClipFamily(GameActor* actor)
{
    enum {
        PLAYER_ACTOR_PE_CLIP_FAMILY0             = 0,
        PLAYER_ACTOR_PE_CLIP_FAMILY1             = 1,
        PLAYER_ACTOR_PE_CLIP_FAMILY2             = 2,
        PLAYER_ACTOR_PE_ITEM_FAMILY0_TENS_DIGIT  = 1,
        PLAYER_ACTOR_PE_SPELL_FAMILY2_TENS_DIGIT = 3,
    };
    s32 attachmentTensDigit;

    attachmentTensDigit = Gp_StateC08.attachId % 100 / 10;
    if (Gp_StateC08.attachId >= ATTACHMENT_ID_ITEM) {
        if (attachmentTensDigit == PLAYER_ACTOR_PE_ITEM_FAMILY0_TENS_DIGIT) {
            actor->actionArgument = PLAYER_ACTOR_PE_CLIP_FAMILY0;
        } else {
            actor->actionArgument = PLAYER_ACTOR_PE_CLIP_FAMILY1;
        }
    } else if (attachmentTensDigit == PLAYER_ACTOR_PE_SPELL_FAMILY2_TENS_DIGIT) {
        actor->actionArgument = PLAYER_ACTOR_PE_CLIP_FAMILY2;
    } else if (Gp_StateC08.attachId < ATTACHMENT_ID_EARLY_SPELL_LIMIT_U) {
        actor->actionArgument = PLAYER_ACTOR_PE_CLIP_FAMILY1;
    } else {
        actor->actionArgument = PLAYER_ACTOR_PE_CLIP_FAMILY0;
    }
}

/// Enters the normal-mode Parasite Energy action and saves the interrupted state.
///
/// Requires live player/attachment state and a normal state to resume;
/// subsequent release/charge/cast playback requires the native animation bank.
/// Entry is unconditional: the caller supplies an attachment ready for release.
/// Stops displacement, disables turning, resets controller/phase and requests
/// aim decay. Selects clip family 0..2 from the attachment id without releasing
/// the lock target or changing attachment ownership. Has no callers in this image.
static void _playerActorEnterPeAction(Task* task)
{
    enum { PLAYER_ACTOR_PE_ENTRY_STATE = 6 };
    GameActor* actor;
    u16        previousState;

    actor                   = task->work;
    previousState           = actor->state;
    actor->state            = PLAYER_ACTOR_PE_ENTRY_STATE;
    actor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
    actor->mode             = GAME_ACTOR_MODE_NORMAL;
    actor->movementMode     = 0;
    actor->turnRateIndex    = 0;
    actor->animationState   = 0;
    actor->statePhase       = 0;
    actor->movementSign     = 0;
    actor->stateAux         = previousState;
    _playerActorChoosePeClipFamily(actor);
}

/// Starts a paralysis episode with stopped control and disabled weapon contacts.
///
/// Requires live player/native playback, session and weapon-effect resources,
/// a normal state 0..6 and equipped weapon index 0..32. Saves the old state for
/// recovery, clears rumble, releases lock-on and locks attachment events before
/// resetting weapon effects. Blends child slots 3..5 into native set 25 over
/// six normal-rate frames. Has no callers in this image.
static void _playerActorStartParalysis(Task* task)
{
    enum {
        PLAYER_ACTOR_PARALYSIS_FIRST_SLOT   = 3,
        PLAYER_ACTOR_PARALYSIS_SET          = 25,
        PLAYER_ACTOR_PARALYSIS_BLEND_FRAMES = 6,
    };
    GameActor* actor;
    u16        previousState;

    actor                 = task->work;
    previousState         = actor->state;
    actor->mode           = GAME_ACTOR_MODE_NORMAL;
    actor->state          = PLAYER_ACTOR_NORMAL_PARALYSIS_STATE;
    actor->movementMode   = 0;
    actor->turnRateIndex  = 0;
    actor->animationState = 0;
    actor->statePhase     = 0;
    actor->rumblePosted   = 0;
    actor->movementSign   = 0;
    actor->turnSign       = 0;
    actor->stateAux       = previousState;
    // Release targeting and lock attachment events before restarting weapon effects.
    playerActorClearLockTarget(task);
    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    Gp_StateC08.flags                                    |= ATTACHMENT_FLAG_EVENT_LOCK;
    playerActorResetWeaponAttack(task, gPlayerStatus.weapon, 0);
    playerActorPlayChildSlotsWithBlend(task, PLAYER_ACTOR_PARALYSIS_SET, PLAYER_ACTOR_PARALYSIS_FIRST_SLOT, PLAYER_ACTOR_PARALYSIS_BLEND_FRAMES);
}

void playerActorScriptedState0(Task* task)
{
    _playerActorTickDirectChildSlots(task);
    playerActorPlayFootstepCue(task);
}

void playerActorTickScriptedAnimation(Task* task)
{
    playerActorTickChildSlots(task);
    playerActorPlayFootstepCue(task);
}

void playerActorMode2State2(Task* task)
{
    enum {
        PLAYER_ACTOR_SCRIPTED_TURN_STEP       = 0x40,
        PLAYER_ACTOR_SCRIPTED_TURN_IDLE_SET   = 1,
        PLAYER_ACTOR_SCRIPTED_TURN_IDLE_BLEND = 5,
    };
    GameActor* actor;
    s16        currentYaw;
    s16        targetYaw;
    u16        storedTargetYaw;
    s32        yawDistance;
    s32        wrappedTargetYaw;
    s32        turnDelta;
    s32        idleStateAndSet;

    actor           = task->work;
    currentYaw      = actor->rotation.vy;
    targetYaw       = actor->scriptMotion.targetYaw;
    storedTargetYaw = actor->scriptMotion.targetYaw;
    yawDistance     = currentYaw - targetYaw;
    if (yawDistance < 0) {
        yawDistance = -yawDistance;
    }
    // Check the stored target and its lower one-turn image before stepping.
    if (yawDistance < PLAYER_ACTOR_SCRIPTED_TURN_STEP + 1 || (wrappedTargetYaw = targetYaw - ACTOR_TRANSFORM_ANGLE_TURN, yawDistance = currentYaw - wrappedTargetYaw, yawDistance = ABS(yawDistance), yawDistance < PLAYER_ACTOR_SCRIPTED_TURN_STEP + 1)) {
        idleStateAndSet              = PLAYER_ACTOR_SCRIPTED_TURN_IDLE_SET;
        actor->rotation.vy           = storedTargetYaw;
        actor->scriptedMotionPending = 0;
        actor->state                 = idleStateAndSet;
        playerActorPlayChildSlotsWithBlend(task, idleStateAndSet, 0, PLAYER_ACTOR_SCRIPTED_TURN_IDLE_BLEND);
    } else {
        turnDelta = playerActorShortestTurn(currentYaw, targetYaw);
        if (turnDelta > PLAYER_ACTOR_SCRIPTED_TURN_STEP) {
            turnDelta = PLAYER_ACTOR_SCRIPTED_TURN_STEP;
        } else if (turnDelta < -PLAYER_ACTOR_SCRIPTED_TURN_STEP) {
            turnDelta = -PLAYER_ACTOR_SCRIPTED_TURN_STEP;
        }
        actor->rotation.vy = ((u16)actor->rotation.vy + turnDelta) & ACTOR_TRANSFORM_ANGLE_MASK;
    }
    playerActorTickChildSlots(task);
}

/// Turns toward a scripted destination, then runs using the move-to arrival test.
///
/// Requires the move-to state's live actor/native playback/root resources and
/// destination in the root's parent frame. Only the tick completing the turn
/// overrides walking with movement mode 3 and run set 4 (or actionArgument).
/// The delegated arrival test is |dx| < 105 and |dz| < 105 game units, ignoring Y;
/// it clears motion pending, enters state 1 and selects the arrival clip.
static void _playerActorTickScriptedRunTo(Task* task)
{
    enum {
        PLAYER_ACTOR_RUN_TO_START_PHASE   = 0,
        PLAYER_ACTOR_RUN_TO_TURN_PHASE    = 1,
        PLAYER_ACTOR_RUN_TO_TRAVEL_PHASE  = 2,
        PLAYER_ACTOR_RUN_TO_MOVEMENT_MODE = 3,
        PLAYER_ACTOR_RUN_TO_SET           = 4,
        PLAYER_ACTOR_RUN_TO_BLEND_FRAMES  = 5,
    };
    GameActor* actor;
    s32        setIndex;

    actor = task->work;
    switch (actor->statePhase) {
        case PLAYER_ACTOR_RUN_TO_START_PHASE:
        case PLAYER_ACTOR_RUN_TO_TURN_PHASE:
            playerActorTickScriptedMoveTo(task);
            if (actor->statePhase == PLAYER_ACTOR_RUN_TO_TRAVEL_PHASE) {
                actor->movementMode = PLAYER_ACTOR_RUN_TO_MOVEMENT_MODE;
                setIndex            = PLAYER_ACTOR_RUN_TO_SET;
                if (actor->actionArgument != 0) {
                    setIndex = actor->actionArgument;
                }
                playerActorPlayChildSlotsWithBlend(task, setIndex, 0, PLAYER_ACTOR_RUN_TO_BLEND_FRAMES);
            }
            break;
        case PLAYER_ACTOR_RUN_TO_TRAVEL_PHASE:
            playerActorTickScriptedMoveTo(task);
            break;
    }
}

void playerActorMode2State6(Task* task)
{
    enum {
        PLAYER_ACTOR_PRESS_HOLD_WAITING        = 0,
        PLAYER_ACTOR_PRESS_HOLD_RELEASED       = 1,
        PLAYER_ACTOR_PRESS_HOLD_RECOVERY_TICKS = 18,
        PLAYER_ACTOR_PRESS_HOLD_BUTTON_MASK    = PAD_BUTTON_UP | PAD_BUTTON_RIGHT | PAD_BUTTON_DOWN | PAD_BUTTON_LEFT |
                                              PAD_BUTTON_TRIANGLE | PAD_BUTTON_CIRCLE | PAD_BUTTON_CROSS | PAD_BUTTON_SQUARE,
    };
    GameActor* actor;

    actor = task->work;
    if (actor->actionValue >= actor->stateTimer) {
        actor->recoveryTicks = PLAYER_ACTOR_PRESS_HOLD_RECOVERY_TICKS;
        if (actor->statePhase == PLAYER_ACTOR_PRESS_HOLD_WAITING) {
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, 0, ACTOR_MESSAGE_RELEASE_HOLD);
            actor->statePhase = PLAYER_ACTOR_PRESS_HOLD_RELEASED;
        }
    } else if (actor->padPressed & PLAYER_ACTOR_PRESS_HOLD_BUTTON_MASK) {
        actor->actionValue++;
    }
    playerActorTickChildSlots(task);
}

/// Selects the actor's target and transfers the targeted mark from its old node.
///
/// `target` must be non-NULL and live while retained by the actor. Re-selecting
/// the same node still marks it targeted. A distinct old node must remain live
/// for this call so its mark can be cleared; no ownership is transferred.
static inline void _playerActorSetTargetNode(Task* task, WorldTargetNode* target)
{
    GameActor*       actor;
    WorldTargetNode* previousTarget;

    actor          = task->work;
    previousTarget = actor->targetNode;
    if (previousTarget != target) {
        if (previousTarget != NULL) {
            previousTarget->state.parts.targeted = 0;
        }
        actor->targetNode = target;
    }
    target->state.parts.targeted = 1;
}

void playerActorSetLockTarget(Task* task, WorldTargetNode* target)
{
    _playerActorSetTargetNode(task, target);
}

/// `hitRegion` dispatcher: three slots of `_playerActorTickDamageReaction`, then `_playerActorDamageHitRegion3`.
static const TaskFuncTable4 Gp_PlayerMode1States = { {
    _playerActorTickDamageReaction,
    _playerActorTickDamageReaction,
    _playerActorTickDamageReaction,
    _playerActorDamageHitRegion3,
} };

/// Dispatches a damage reaction, then advances animation, facing and movement.
///
/// Requires live GameActor/model and the selected reaction's resources.
/// hitRegion must be 0..3: 0..2 run damage presentation, 3 is an empty handler.
/// The selector is interpreted as u16; GameActor.state is not used here.
static void _playerActorTickDamage(Task* task)
{
    TaskFuncTable4 handlers;
    GameActor*     actor;

    handlers = Gp_PlayerMode1States;
    actor    = task->work;
    handlers.funcs[(u16)actor->hitRegion](task);
    playerActorTickAnimationState(task);
    playerActorTickChildSlots(task);
    playerActorUpdateFacing(task);
    playerActorStepMovement(task);
}

/// `state` dispatcher copied by `_playerActorTickScripted`.
static const TaskFuncTable12 Gp_PlayerMode2States = { {
    playerActorScriptedState0,
    playerActorTickScriptedAnimation,
    playerActorMode2State2,
    _playerActorTickScriptedStairClimb,
    playerActorTickScriptedMoveTo,
    _playerActorTickScriptedWalkSteps,
    playerActorMode2State6,
    _playerActorTickScriptedPresentation,
    _playerActorTickScriptedRunTo,
    _playerActorScriptedState9,
    _playerActorTickScriptedAttack,
    _playerActorTickScriptedItemUse,
} };

/// Dispatches the player's scripted action and updates facing.
///
/// Requires live actor/model and playback/resources for the selected state
/// (0..11). Each handler owns its animation and movement stepping. At zero HP,
/// restores the native weapon bank and requests the death pose unless the
/// post-handler state is scripted attack (10); that handler retains control.
static void _playerActorTickScripted(Task* task)
{
    enum { PLAYER_ACTOR_SCRIPTED_DEATH_POSE_BLEND_FRAMES = 4 };
    GameActor*      actor;
    TaskFuncTable12 states;

    states = Gp_PlayerMode2States;
    actor  = task->work;
    states.funcs[actor->state](task);
    playerActorUpdateFacing(task);
    if (gPlayerStatus.hp <= 0 && actor->state != PLAYER_ACTOR_SCRIPTED_ATTACK_STATE) {
        _animationBindPlayerWeaponBank(task);
        playerActorEnterStoppedPose(task, PLAYER_ACTOR_SCRIPTED_DEATH_POSE_BLEND_FRAMES);
    }
}

static void func_80108FA0(Task* arg0)
{
    _playerActorUpdateNormalLocomotion(arg0);
    _playerActorTryEnterPeAction(arg0);
    playerActorPlayFootstepCue(arg0);
}

/// Completes a finished normal aim-entry phase and blends into aim locomotion.
///
/// Zero phase leaves everything intact; a nonzero phase engages an idle battle
/// and resumes aiming with a three-frame blend. actor must be task's live work
/// with native playback resources. A pending transition is consumed even when
/// targetCandidate is NULL. A selected candidate is borrowed until the actor
/// releases or replaces it; without a pending transition it is ignored.
static inline void _playerActorFinishNormalAimEntry(Task* task, GameActor* actor, WorldTargetNode* targetCandidate)
{
    enum { PLAYER_ACTOR_AIM_ENTRY_BLEND_FRAMES = 3 };

    if (actor->statePhase != 0) {
        sceneEngageBattle(1);
        if (actor->aimTransitionPending != 0) {
            actor->aimTransitionPending = 0;
            if (targetCandidate != NULL) {
                playerActorSetLockTarget(task, targetCandidate);
            }
        }
        _playerActorEnterAimLocomotion(task, PLAYER_ACTOR_AIM_ENTRY_BLEND_FRAMES);
    }
}

/// Completes aim entry or begins aim exit in normal-mode state 1.
///
/// Requires live actor work, native child-slot resources and a live borrowed
/// target if selected. Movement stops while aim tracking continues. A target
/// in an unfinished battle, engaged battle phase or nonzero saved gate
/// retains aim entry; a completed phase then enters aim locomotion with a
/// three-frame blend. Without those gates, an exit request clears the target
/// and starts angle decay. The saved gate's wider purpose is unproven.
static void _playerActorNormalState1(Task* task)
{
    GameActor*       actor;
    WorldTargetNode* node;

    playerActorTrackLockTarget(task);
    actor               = task->work;
    node                = worldTargetFindLockNode(task);
    actor->movementSign = 0;
    if ((node != NULL && gSceneCombatState.signals.bytes.battlePhase < SCENE_COMBAT_BATTLE_FINISHED) || gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED ||
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.field_929 != 0) {
        _playerActorFinishNormalAimEntry(task, actor, node);
    } else {
        _playerActorUpdateAimRequest(task);
        if (actor->aimControl & GAME_ACTOR_AIM_REQUEST_EXIT) {
            actor->aimTransitionPending = 0;
            actor->aimTrackingState     = GAME_ACTOR_AIM_TRACKING_DECAY;
            playerActorClearLockTarget(task);
            playerActorExitAim(task);
        }
    }
}

/// Keeps aim exit stationary and permits a new aim request (normal state 3).
///
/// Replaces the aim request from held Square and current attachment/equipment
/// restrictions. An entry request starts aim entry with a four-frame blend;
/// otherwise the dispatcher continues the existing exit animation. Requires
/// live actor, equipment/attachment state and native animation resources.
static void _playerActorUpdateAimExit(Task* task)
{
    enum { PLAYER_ACTOR_AIM_EXIT_REENTRY_BLEND_FRAMES = 4 };
    GameActor* actor;

    actor               = task->work;
    actor->movementSign = 0;
    _playerActorUpdateAimRequest(task);
    if (actor->aimControl & GAME_ACTOR_AIM_REQUEST_ENTER) {
        playerActorEnterAim(task, PLAYER_ACTOR_AIM_EXIT_REENTRY_BLEND_FRAMES);
    }
}

static void func_80109138(Task* arg0)
{
    _playerActorDispatchWeaponAttack(arg0);
    _playerActorPostVibrationPreset(arg0, 0);
    _playerActorUpdateLockTargetFromPad(arg0);
}

/// Dispatches damage presentation for player hit-region selectors 0, 1 and 2.
///
/// Requires live GameActor work and the selected reaction's playback/effect
/// resources. Ordinary/status reactions await clip completion; poison, three
/// descending-part flashes, body blasts and spark/puffs use their own handlers.
/// Reaction 4 and other unhandled byte values do nothing here. Does not consume
/// the reaction byte; animation, child-slot playback and movement tick afterward
/// in the enclosing damage-mode dispatcher.
static void _playerActorTickDamageReaction(Task* task)
{
    enum {
        GAME_ACTOR_REACTION_HIT_FLASHES     = 5,
        GAME_ACTOR_REACTION_BODY_BLAST      = 6,
        GAME_ACTOR_REACTION_SPARK_AND_PUFFS = 7,
    };
    GameActor* actor;
    u8         reaction;

    actor    = task->work;
    reaction = actor->damageReaction;
    switch (reaction) {
        case GAME_ACTOR_REACTION_ORDINARY:
        case GAME_ACTOR_REACTION_DARKNESS:
        case GAME_ACTOR_REACTION_PARALYSIS:
        case GAME_ACTOR_REACTION_SILENCE:
        case GAME_ACTOR_REACTION_STATUS20:
        case GAME_ACTOR_REACTION_CONFUSION:
        case GAME_ACTOR_REACTION_BERSERKER:
            playerActorFinishDamageReaction(task);
            break;
        case GAME_ACTOR_REACTION_HIT_FLASHES:
            playerActorTickHitFlashes(task);
            break;
        case GAME_ACTOR_REACTION_BODY_BLAST:
            _playerActorTickBodyBlastHit(task);
            break;
        case GAME_ACTOR_REACTION_POISON:
            playerActorTickPoisonHit(task);
            break;
        case GAME_ACTOR_REACTION_SPARK_AND_PUFFS:
            _playerActorTickSparkPuffHit(task);
            break;
    }
}

/// Empty damage-mode callback for hit-region selector 3.
///
/// The mode dispatcher still advances animation, turning and movement afterward.
static void _playerActorDamageHitRegion3(Task* unusedTask)
{
}

/// Sets the normal-mode yaw/strafe sign from logical held horizontal directions.
///
/// Requires live GameActor work. Left wins simultaneous left/right input;
/// the signed-byte result is -1 left, +1 right, or 0 neither. Reads the actor's
/// remapped/confusion-adjusted buttons without consuming input.
static void _playerActorUpdateTurnInput(Task* task)
{
    GameActor* actor;
    u16        heldButtons;

    actor       = task->work;
    heldButtons = actor->padHeld;
    if (heldButtons & (PAD_BUTTON_LEFT | PAD_BUTTON_RIGHT)) {
        if (heldButtons & PAD_BUTTON_LEFT) {
            actor->turnSign = -1;
        } else {
            actor->turnSign = 1;
        }
    } else {
        actor->turnSign = 0;
    }
}

/// Sets the normal-mode forward/backward sign from logical held vertical directions.
///
/// Requires live GameActor work. Down wins simultaneous up/down input;
/// the signed-byte result is -1 backward, +1 forward, or 0 neither. Reads the
/// actor's remapped/confusion-adjusted buttons without consuming input.
static void _playerActorUpdateMovementInput(Task* task)
{
    GameActor* actor;
    u16        heldButtons;

    actor       = task->work;
    heldButtons = actor->padHeld;
    if (heldButtons & (PAD_BUTTON_UP | PAD_BUTTON_DOWN)) {
        if (heldButtons & PAD_BUTTON_DOWN) {
            actor->movementSign = -1;
        } else {
            actor->movementSign = 1;
        }
    } else {
        actor->movementSign = 0;
    }
}

/// Starts the normal-mode Parasite Energy action when an attachment is held.
///
/// Returns 0 and changes nothing unless effectPhase is HELD; accepting returns 1,
/// saves the previous normal state and enters state 6 with displacement stopped,
/// turn/controller/phase reset and aim decay requested. actionArgument selects
/// native release/charge/cast clip family 0, 1 or 2: item IDs use 0 for tens digit
/// 1 and 1 otherwise; spells use 2 for digit 3, otherwise 1 below 300 and 0 at or above it.
/// Requires live GameActor work and attachment state; later playback requires
/// the native bank. Does not release the target or change attachment ownership.
static s32 _playerActorTryEnterPeAction(Task* task)
{
    enum {
        PLAYER_ACTOR_PE_ENTRY_STATE              = 6,
        PLAYER_ACTOR_PE_ENTRY_STOPPED            = 0,
        PLAYER_ACTOR_PE_ENTRY_TURN_DISABLED      = 0,
        PLAYER_ACTOR_PE_ENTRY_CONTROLLER         = 0,
        PLAYER_ACTOR_PE_CLIP_FAMILY0             = 0,
        PLAYER_ACTOR_PE_CLIP_FAMILY1             = 1,
        PLAYER_ACTOR_PE_CLIP_FAMILY2             = 2,
        PLAYER_ACTOR_PE_ITEM_FAMILY0_TENS_DIGIT  = 1,
        PLAYER_ACTOR_PE_SPELL_FAMILY2_TENS_DIGIT = 3,
    };
    GameActor* actor;
    u16        previousState;
    s32        attachmentTensDigit;
    s32        entered;

    entered = 0;
    if (Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) {
        actor = task->work;
        // Save the interrupted normal state for the release/charge/cast return.
        previousState           = actor->state;
        actor->state            = PLAYER_ACTOR_PE_ENTRY_STATE;
        actor->mode             = GAME_ACTOR_MODE_NORMAL;
        actor->movementMode     = PLAYER_ACTOR_PE_ENTRY_STOPPED;
        actor->turnRateIndex    = PLAYER_ACTOR_PE_ENTRY_TURN_DISABLED;
        actor->animationState   = PLAYER_ACTOR_PE_ENTRY_CONTROLLER;
        actor->statePhase       = 0;
        actor->movementSign     = 0;
        actor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
        actor->stateAux         = previousState;
        attachmentTensDigit     = (u16)(Gp_StateC08.attachId % 100 / 10);
        if (Gp_StateC08.attachId >= ATTACHMENT_ID_ITEM) {
            if (attachmentTensDigit == PLAYER_ACTOR_PE_ITEM_FAMILY0_TENS_DIGIT) {
                actor->actionArgument = PLAYER_ACTOR_PE_CLIP_FAMILY0;
            } else {
                actor->actionArgument = PLAYER_ACTOR_PE_CLIP_FAMILY1;
            }
        } else if (attachmentTensDigit == PLAYER_ACTOR_PE_SPELL_FAMILY2_TENS_DIGIT) {
            actor->actionArgument = PLAYER_ACTOR_PE_CLIP_FAMILY2;
        } else if (Gp_StateC08.attachId < ATTACHMENT_ID_EARLY_SPELL_LIMIT_U) {
            actor->actionArgument = PLAYER_ACTOR_PE_CLIP_FAMILY1;
        } else {
            actor->actionArgument = PLAYER_ACTOR_PE_CLIP_FAMILY0;
        }
        entered = 1;
    }
    return entered;
}

/// Replaces the aim request from held Square and the actor's action restrictions.
///
/// Requests entry only with idle attachment effects, an equipped weapon and
/// unrestricted aim; every other combination requests exit. Reads logical
/// actor input without consuming an edge or changing the selected target.
static void _playerActorUpdateAimRequest(Task* task)
{
    GameActor* actor;

    actor = task->work;
    if ((actor->padHeld & PAD_BUTTON_SQUARE) && (Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_IDLE) && (gPlayerStatus.weapon != PLAYER_STATUS_EQUIPMENT_NONE) &&
        (actor->restrictRunAndAim == 0)) {
        actor->aimControl = GAME_ACTOR_AIM_REQUEST_ENTER;
    } else {
        actor->aimControl = GAME_ACTOR_AIM_REQUEST_EXIT;
    }
}

/// Applies Cross release, Square acquisition and left/right target cycling.
///
/// Cross press has priority over selection. Existing locks cycle on Square
/// press or held Square plus a direction edge; new locks require Square press
/// without Darkness. Targets are borrowed, and old/new nodes must remain live.
/// Selection retains the binary's unguarded target-mark store when a scan
/// returns NULL; the runtime invariant preventing that case is unproven.
static void _playerActorUpdateLockTargetFromPad(Task* task)
{
    GameActor* actor;
    u16        pressedButtons;

    actor = task->work;
    if (actor->targetNode != NULL) {
        pressedButtons = actor->padPressed;
        if (pressedButtons & PAD_BUTTON_CROSS) {
            playerActorClearLockTarget(task);
        } else if (((actor->padHeld & PAD_BUTTON_SQUARE) && (pressedButtons & (PAD_BUTTON_LEFT | PAD_BUTTON_RIGHT))) || (pressedButtons & PAD_BUTTON_SQUARE)) {
            _playerActorSetTargetNode(task, worldTargetFindLockNodeFromPad(task));
        }
    } else if ((actor->padPressed & PAD_BUTTON_SQUARE) && !(gPlayerStatus.statusFlags & PLAYER_STATUS_DARKNESS)) {
        actor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_TARGET;
        _playerActorSetTargetNode(task, worldTargetFindLockNode(task));
    }
}

/// Walks forward until the requested number of non-silent footstep cues has occurred.
///
/// actionValue is a positive signed-halfword count set by the walk-steps message.
/// Requires live actor/root/playback and footstep surface/sound/effect resources.
/// Selects walk set 2, or unarmed set 19 when equipment slot 1 is empty; movement
/// mode 1 supplies game-coordinate displacement per tick. Entry falls through
/// to movement/cue handling. A nonzero cue decrements the count without moving
/// that tick; completion clears motion pending and blends to state 1/idle set 1.
/// stateTimer only chooses between legacy values of an ignored playback argument.
static void _playerActorTickScriptedWalkSteps(Task* task)
{
    enum {
        PLAYER_ACTOR_WALK_STEPS_START_PHASE = 0,
        PLAYER_ACTOR_WALK_STEPS_WALK_PHASE  = 1,
        PLAYER_ACTOR_WALK_STEPS_FORWARD     = 1,
        PLAYER_ACTOR_WALK_STEPS_WALK_SET    = 2,
        PLAYER_ACTOR_WALK_STEPS_UNARMED_SET = 19,
        PLAYER_ACTOR_WALK_STEPS_IDLE_SET    = 1,
        PLAYER_ACTOR_WALK_STEPS_IDLE_BLEND  = 5,
    };
    GameActor* actor;
    s32        setIndex;
    s32        walkingPhaseAndMode;
    s32        unusedPlaybackArgument;

    actor = task->work;
    switch (actor->statePhase) {
        case PLAYER_ACTOR_WALK_STEPS_START_PHASE:
            setIndex            = PLAYER_ACTOR_WALK_STEPS_WALK_SET;
            walkingPhaseAndMode = PLAYER_ACTOR_WALK_STEPS_WALK_PHASE;
            actor->statePhase   = walkingPhaseAndMode;
            actor->movementMode = walkingPhaseAndMode;
            if (actor->equipmentTasks[1] == NULL) {
                setIndex = PLAYER_ACTOR_WALK_STEPS_UNARMED_SET;
            }
            unusedPlaybackArgument = 1;
            if (actor->stateTimer == 0) {
                unusedPlaybackArgument = 6;
            }
            playerActorPlayChildSlots(task, setIndex, unusedPlaybackArgument);
            // Count or move on the entry tick as well as later walking ticks.
        case PLAYER_ACTOR_WALK_STEPS_WALK_PHASE:
            if (playerActorPlayFootstepCue(task) != 0) {
                actor->actionValue--;
                if (actor->actionValue <= 0) {
                    actor->scriptedMotionPending = 0;
                    actor->state                 = PLAYER_ACTOR_SCRIPTED_ANIMATION_STATE;
                    playerActorPlayChildSlotsWithBlend(task, PLAYER_ACTOR_WALK_STEPS_IDLE_SET, 0, PLAYER_ACTOR_WALK_STEPS_IDLE_BLEND);
                }
            } else {
                actor->movementSign = PLAYER_ACTOR_WALK_STEPS_FORWARD;
                playerActorStepMovement(task);
            }
            break;
    }
    playerActorTickChildSlots(task);
}

/// Writes the equipped ammunition's sound-script variant in bits 24..31.
///
/// Overwrites one writable s32; the caller adds the weapon and event bits.
/// Grenade/shotgun rows 10..15 cycle through variants 0..2; other rows yield
/// zero. M4A1 Grenade instead uses its secondary item relative to row 10,
/// clamping an absent item to zero. Requires live equipment/save state; no
/// pointer is retained. The high-byte shift preserves the signed s32 result.
static void _playerActorWriteWeaponSoundVariant(s32* variantBitsOut)
{
    enum {
        PLAYER_ACTOR_SOUND_M4A1_GRENADE_WEAPON = 27,
        PLAYER_ACTOR_SOUND_SPECIAL_AMMO_FIRST  = 10,
        PLAYER_ACTOR_SOUND_SPECIAL_AMMO_COUNT  = 6,
        PLAYER_ACTOR_SOUND_VARIANTS_PER_GROUP  = 3,
        PLAYER_ACTOR_SOUND_VARIANT_SHIFT       = 24,
    };
    PlayerStatus*          playerStatus;
    volatile PlayerStatus* livePlayerStatus;

    playerStatus = &gPlayerStatus;
    if (playerStatus->weapon == PLAYER_ACTOR_SOUND_M4A1_GRENADE_WEAPON) {
        *variantBitsOut = equipmentGetWeaponLoad(playerStatus->weapon + (EQUIPMENT_WEAPON_ITEM_FIRST - 1))->secondaryItemId - (INVENTORY_CONSUMABLE_ITEM_FIRST - 1);
        if (*variantBitsOut < 0) {
            *variantBitsOut = PLAYER_ACTOR_SOUND_SPECIAL_AMMO_FIRST;
        }
        *variantBitsOut = (*variantBitsOut - PLAYER_ACTOR_SOUND_SPECIAL_AMMO_FIRST) << PLAYER_ACTOR_SOUND_VARIANT_SHIFT;
    } else {
        // Keep the binary's separate byte reads for the range test and variant.
        livePlayerStatus = playerStatus;
        if ((u32)(livePlayerStatus->weaponSlotItem - PLAYER_ACTOR_SOUND_SPECIAL_AMMO_FIRST) < PLAYER_ACTOR_SOUND_SPECIAL_AMMO_COUNT) {
            *variantBitsOut = ((livePlayerStatus->weaponSlotItem - 1) % PLAYER_ACTOR_SOUND_VARIANTS_PER_GROUP) << PLAYER_ACTOR_SOUND_VARIANT_SHIFT;
            if (*variantBitsOut < 0) {
                *variantBitsOut = 0;
            }
        } else {
            *variantBitsOut = 0;
        }
    }
}

/// Starts one of the paired scripted presentation clips and keeps checking contact damage.
///
/// The retained stateTimer halfword selects active-bank set 32 for zero or 33
/// otherwise; the clips' visual roles are unproven. Requires live actor/model,
/// both sets in the active bank and contact-damage resources. Entry resets
/// child playback, then falls through to advance it on the same tick. Phase 1
/// continues playback; this state performs no completion or motion-pending update.
static void _playerActorTickScriptedPresentation(Task* task)
{
    enum {
        PLAYER_ACTOR_PRESENTATION_START_PHASE = 0,
        PLAYER_ACTOR_PRESENTATION_PLAY_PHASE  = 1,
        PLAYER_ACTOR_PRESENTATION_ZERO_SET    = 32,
        PLAYER_ACTOR_PRESENTATION_NONZERO_SET = 33,
    };
    GameActor* actor;
    s32        setIndex;
    s32        playbackPhase;

    actor = task->work;
    switch (actor->statePhase) {
        case PLAYER_ACTOR_PRESENTATION_START_PHASE:
            setIndex          = PLAYER_ACTOR_PRESENTATION_ZERO_SET;
            playbackPhase     = PLAYER_ACTOR_PRESENTATION_PLAY_PHASE;
            actor->statePhase = playbackPhase;
            if (actor->stateTimer != 0) {
                setIndex = PLAYER_ACTOR_PRESENTATION_NONZERO_SET;
            }
            playerActorPlayChildSlots(task, setIndex, 1);
            // Advance the new clip on the entry tick as well as later ticks.
        case PLAYER_ACTOR_PRESENTATION_PLAY_PHASE:
            playerActorTickChildSlots(task);
            break;
    }
    playerActorCheckContactDamage(task);
}

/// Advances only child animation slots in scripted player state 9.
static void _playerActorScriptedState9(Task* task)
{
    playerActorTickChildSlots(task);
}

/// Updates model part 4's relative yaw from horizontal input and recenters it afterward.
///
/// Angles use 4096 units per turn. Normal-mode input steps by 32 within +/-416;
/// left wins when both directions are held. Otherwise the offset decays by an
/// eighth of itself, at least 64, and snaps to zero within 64. Requires live
/// actor work and at least five model coordinates; dirties part 4 every call.
static void _playerActorUpdateTurnYawOffset(Task* task)
{
    enum {
        PLAYER_ACTOR_TURN_YAW_PART         = 4,
        PLAYER_ACTOR_TURN_YAW_INPUT_STEP   = 32,
        PLAYER_ACTOR_TURN_YAW_LIMIT        = 416,
        PLAYER_ACTOR_TURN_YAW_RECENTER_MIN = 64,
    };
    GameActor* actor;
    GfxCoord*  coords;
    u16        heldButtons;
    s16        yawStep;

    /// Recenters a nonzero relative turn yaw by an eighth, with a 64-unit minimum step.
    ///
    /// `actor` is a stable pointer to writable GameActor work; `yawStep` is a
    /// writable s16 lvalue. Both arguments are evaluated repeatedly. Uses the
    /// enclosing function's recenter constant; removed with #undef after this use.
#define PLAYER_ACTOR_RECENTER_TURN_YAW(actor, yawStep)                       \
    do {                                                                     \
        s32 decayStep;                                                       \
        s32 originalDecayStep;                                               \
        decayStep         = (actor)->aimYaw >> 3;                            \
        (yawStep)         = decayStep;                                       \
        originalDecayStep = decayStep;                                       \
        if (ABS(originalDecayStep) < PLAYER_ACTOR_TURN_YAW_RECENTER_MIN) {   \
            decayStep = PLAYER_ACTOR_TURN_YAW_RECENTER_MIN;                  \
            if (originalDecayStep < 0) {                                     \
                decayStep = -PLAYER_ACTOR_TURN_YAW_RECENTER_MIN;             \
            }                                                                \
            (yawStep) = decayStep;                                           \
        }                                                                    \
        (actor)->aimYaw -= (yawStep);                                        \
        if (ABS((actor)->aimYaw) < PLAYER_ACTOR_TURN_YAW_RECENTER_MIN + 1) { \
            (actor)->aimYaw = 0;                                             \
        }                                                                    \
    } while (0)

    coords                                          = task->extra.tmd->coords;
    actor                                           = task->work;
    coords[PLAYER_ACTOR_TURN_YAW_PART].composeStamp = GRAPHICS_COORD_DIRTY;
    heldButtons                                     = actor->padHeld;
    if ((heldButtons & (PAD_BUTTON_LEFT | PAD_BUTTON_RIGHT)) && (actor->mode == GAME_ACTOR_MODE_NORMAL)) {
        if (heldButtons & PAD_BUTTON_LEFT) {
            yawStep = -PLAYER_ACTOR_TURN_YAW_INPUT_STEP;
        } else {
            yawStep = PLAYER_ACTOR_TURN_YAW_INPUT_STEP;
        }
        if (ABS(actor->aimYaw + yawStep) < PLAYER_ACTOR_TURN_YAW_LIMIT + 1) {
            actor->aimYaw += yawStep;
        }
    } else if (actor->aimYaw != 0) {
        PLAYER_ACTOR_RECENTER_TURN_YAW(actor, yawStep);
    }
#undef PLAYER_ACTOR_RECENTER_TURN_YAW
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

/// Caps the player's damage-derived hit-effect level at 2.
///
/// The reaction derives its nonnegative level as unsigned pending damage / 12;
/// the capped value selects burst size and repetition count. Negative inputs
/// are preserved by this signed helper.
static inline s32 _playerActorClampHitEffectLevel(s32 hitEffectLevel)
{
    enum { PLAYER_ACTOR_HIT_EFFECT_MAX_LEVEL = 2 };
    s32 clampedLevel = PLAYER_ACTOR_HIT_EFFECT_MAX_LEVEL;
    if (hitEffectLevel < PLAYER_ACTOR_HIT_EFFECT_MAX_LEVEL + 1) {
        clampedLevel = hitEffectLevel;
    }
    return clampedLevel;
}

/// Sets an XYZ offset at a motion body's origin, shifted upward for the root body.
///
/// Borrows writable localOffset and live actor work for this call. The recorded
/// hit-body byte is interpreted as signed; valid motion-body indices are 0..2.
/// Only root index 0 selects Y = -400; every other byte selects the zero vector.
/// Components use game-coordinate units in the chosen body's local frame.
/// Vector metadata is untouched, and no pointer is retained.
static inline void _playerActorInitHitOffset(SVECTOR* localOffset, const GameActor* actor)
{
    enum { PLAYER_ACTOR_HIT_ROOT_OFFSET_Y = -400 };
    s32 offsetY;

    localOffset->vx = 0;
    offsetY         = 0;
    if ((s8)actor->hitBodyIndex == GAME_ACTOR_BODY_ROOT) {
        offsetY = PLAYER_ACTOR_HIT_ROOT_OFFSET_Y;
    }
    localOffset->vy = offsetY;
    localOffset->vz = 0;
}

/// Ends the actor's current action: clears the fields `playerActorClearPendingHit` resets,
/// sets `recoveryTicks` to 0x12 and restarts the state machine in the base state
/// for its `state` mode.
static inline void _gpResumeBaseState(Task* arg0)
{
    GameActor* inner;

    inner = arg0->work;
    playerActorClearPendingHit(arg0);
    inner->recoveryTicks = 0x12;
    if (inner->state != 0) {
        playerActorEnterAim(arg0, 0xC);
    } else {
        playerActorEnterLocomotion(arg0, 0);
    }
}

/// Advances reaction 7's spark-and-puff bursts at the recorded hit body.
///
/// Requires live player actor/model, motion body index 0..2, native hit clip
/// and effect/scratch resources. Interprets pending damage as its unsigned low
/// halfword, divides by 12 and caps at 2. Phase 0 records the hit coordinate
/// and puff count; phase 1 decrements the signed-halfword burst counter before
/// emitting, with six countdown ticks between bursts. A zero level therefore
/// starts at -1 after decrement. Clip-end controller 7 also advances phases;
/// phase 2 waits and phase 3 resumes aim or locomotion with recovery.
/// The shared spawn record must keep this hit's coordinate/count until the
/// emission phase ends; the coordinate must outlive every spawned effect.
static void _playerActorTickSparkPuffHit(Task* task)
{
    enum {
        PLAYER_ACTOR_SPARK_PUFF_RECORD_SIZE_BASE = 288,
        PLAYER_ACTOR_SPARK_PUFF_RECORD_SIZE_STEP = 32,
        PLAYER_ACTOR_SPARK_PUFF_SPLATTER_LEVEL   = 3,
    };
    SVECTOR*        localOffset;
    GameActor*      actor;
    EffectSpawnArg* spawnRecord;
    GfxCoord*       hitCoord;
    s32             effectKind;
    s32             hitEffectLevel;

    actor          = task->work;
    hitEffectLevel = (u16)actor->pendingDamage / PLAYER_ACTOR_EFFECT_HIT_DAMAGE_STEP;
    localOffset    = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    spawnRecord    = &D_80113358;
    hitEffectLevel = _playerActorClampHitEffectLevel(hitEffectLevel);
    switch (actor->statePhase) {
        case PLAYER_ACTOR_EFFECT_HIT_PHASE_START:
            // Retain hit placement and count across waits; this recipe ignores the size half.
            actor->statePhase       = PLAYER_ACTOR_EFFECT_HIT_PHASE_EMIT;
            hitCoord                = ((s8)actor->hitBodyIndex + actor->collisionBodies)->coord;
            spawnRecord->spawnArgLo = (hitEffectLevel * PLAYER_ACTOR_SPARK_PUFF_RECORD_SIZE_STEP) + PLAYER_ACTOR_SPARK_PUFF_RECORD_SIZE_BASE;
            spawnRecord->spawnArgHi = hitEffectLevel + 1;
            D_80113358.coord        = hitCoord;
            actor->stateTimer       = 0;
            actor->actionValue      = hitEffectLevel;
            /* fallthrough */
        case PLAYER_ACTOR_EFFECT_HIT_PHASE_EMIT:
            if (actor->stateTimer == 0) {
                // Retain the splatter threshold even though the capped level selects sparks/puffs.
                effectKind = EFFECT_HIT_KIND_SPLATTER;
                if (hitEffectLevel < PLAYER_ACTOR_SPARK_PUFF_SPLATTER_LEVEL) {
                    effectKind = EFFECT_HIT_KIND_SPARK_AND_PUFFS;
                }
                actor->actionValue--;
                if (actor->actionValue == 0) {
                    actor->statePhase++;
                } else {
                    actor->stateTimer = PLAYER_ACTOR_EFFECT_HIT_DELAY_TICKS;
                }
                _playerActorInitHitOffset(localOffset, actor);
                effectSpawnHit(effectKind, spawnRecord->coord, localOffset, spawnRecord);
            } else {
                actor->stateTimer--;
            }
            break;
        case PLAYER_ACTOR_EFFECT_HIT_PHASE_WAIT_CLIP:
            break;
        case PLAYER_ACTOR_EFFECT_HIT_PHASE_RESUME:
            _gpResumeBaseState(task);
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

/// Emits a damage-scaled blast at an already selected player-model coordinate.
///
/// hitEffectLevel is 0..2; size is 192 + 96*level parent-coordinate units.
/// The high half is level+1: the flame phase lasts 1/6/9 running ticks and the
/// total lifetime 2/12/18 ticks, followed by release on the next running tick.
/// Borrows a writable spawnRecord already holding the selected coordinate.
/// The record may be reused after return; the spawned blast borrows that
/// coordinate through its lifetime. Requires live player and effect/graphics/
/// scratch resources.
static inline void _effectSpawnPlayerHitBlast(EffectSpawnArg* spawnRecord, s32 hitEffectLevel)
{
    enum {
        EFFECT_PLAYER_HIT_BLAST_SIZE_BASE = 192,
        EFFECT_PLAYER_HIT_BLAST_SIZE_STEP = 96,
    };
    GfxCoord* blastCoord;

    blastCoord              = spawnRecord->coord;
    spawnRecord->spawnArgLo = (hitEffectLevel * EFFECT_PLAYER_HIT_BLAST_SIZE_STEP) + EFFECT_PLAYER_HIT_BLAST_SIZE_BASE;
    spawnRecord->spawnArgHi = hitEffectLevel + 1;
    effectSpawnHit(EFFECT_HIT_KIND_BLAST, blastCoord, NULL, spawnRecord);
}

/// Advances reaction 6's three-part body-blast sequence and clip-driven recovery.
///
/// Requires live player work, model parts 2..4, native hit clip and effect
/// resources. Phase 0 resets the part counter and falls into emission; phase 1
/// increments it before selecting parts 2, 3, 4, with six countdown ticks
/// between blasts. Each emission reinterprets pending damage as u16, divides
/// by 12 and caps at 2. Clip-end controller 7 also advances the phase; phase 2
/// waits and phase 3 resumes aim or locomotion with 18 recovery ticks.
/// Borrows the shared spawn record; coordinates must outlive spawned blasts.
static void _playerActorTickBodyBlastHit(Task* task)
{
    enum {
        PLAYER_ACTOR_BODY_BLAST_COUNT     = 3,
        PLAYER_ACTOR_BODY_BLAST_PART_BASE = 1,
    };
    GameActor*      actor;
    EffectSpawnArg* spawnRecord;
    GfxCoord*       blastCoord;
    s32             hitEffectLevel;

    actor = task->work;
    switch (actor->statePhase) {
        case PLAYER_ACTOR_EFFECT_HIT_PHASE_START:
            actor->statePhase  = PLAYER_ACTOR_EFFECT_HIT_PHASE_EMIT;
            actor->stateTimer  = 0;
            actor->actionValue = 0;
            /* fallthrough */
        case PLAYER_ACTOR_EFFECT_HIT_PHASE_EMIT:
            if (actor->stateTimer == 0) {
                spawnRecord = &D_80113358;
                actor->actionValue++;
                if (actor->actionValue == PLAYER_ACTOR_BODY_BLAST_COUNT) {
                    actor->statePhase++;
                } else {
                    actor->stateTimer = PLAYER_ACTOR_EFFECT_HIT_DELAY_TICKS;
                }
                // Select the next model part before filling the shared blast recipe.
                blastCoord         = &task->extra.tmd->coords[actor->actionValue + PLAYER_ACTOR_BODY_BLAST_PART_BASE];
                spawnRecord->coord = blastCoord;
                hitEffectLevel     = (u16)((u16)actor->pendingDamage / PLAYER_ACTOR_EFFECT_HIT_DAMAGE_STEP);
                hitEffectLevel     = _playerActorClampHitEffectLevel(hitEffectLevel);
                _effectSpawnPlayerHitBlast(spawnRecord, hitEffectLevel);
            } else {
                actor->stateTimer--;
            }
            break;
        case PLAYER_ACTOR_EFFECT_HIT_PHASE_WAIT_CLIP:
            break;
        case PLAYER_ACTOR_EFFECT_HIT_PHASE_RESUME:
            _gpResumeBaseState(task);
            break;
    }
}
