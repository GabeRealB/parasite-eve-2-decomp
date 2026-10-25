#include "rooms/mine_cavern.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/stdio.h>

#include "common.h"
#include "gte.h"

#include "mine_cavern_private.h"

#include "actors/actor_00300.h"
#include "actors/actor_400600.h"
#include "actors/actor_402200.h"
#include "actors/actor_403000.h"
#include "actors/actor.h"

#include "gameplay/display.h"
#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/enemy_params.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/event_flags.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/random.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "../../shared/room/room_visual_effects.h"
#include "../../shared/effects/glow_draw.h"

/// Work block of the cavern's two target tasks, parked at `Task::work`.
///
/// The room spawns both tasks at each of its four target spots, and each
/// allocates one of these zeroed. The intact target is an enemy: attacks reach
/// it through `body`, and the hit that empties its hit points sets the spot's
/// bit in `GAME_FLAG_MINE_CAVERN_TARGETS_DESTROYED`, after which `body` is
/// retired and `frame` counts the 60-tick explosion sequence the task ends on.
/// The remains task keeps its model hidden until that bit is set; it links no
/// collision body and touches only the matrices, `centerCoord` and `frame`.
///
/// The model borrows `light` and `color` as its lighting matrices, so the
/// block has to outlive the model's last draw.
typedef struct {
    MATRIX                light;            // Light-direction matrix the model is drawn with; filled by the room-light query
    MATRIX                color;            // Light-colour matrix the model is drawn with; filled by the same query
    WorldCollisionBody    body;             // Target: 0x100-unit sphere on list 2 taking attacks; unlinked when the target is destroyed or its task exits
    WorldCollisionContact bodyContacts[4];  // Target: `body`'s contact table; a class-2 contact is a hit. Released every tick
    WorldCollisionBody    blast;            // Target: 3000-unit class-2 sphere on list 1, unlinked 9 ticks into the explosion. Its pair enable is only ever cleared
    WorldCollisionContact blastContacts[1]; // Target: `blast`'s contact table; released every tick, never read
    GfxCoord              centerCoord;      // Remains: child of the model's coordinate at (0, -0x320, 0), the target sphere's centre; recomposed while shown, reader unproven
    u16                   frame;            // Target: tick of the explosion sequence, 0..0x3B. Remains: ticks spent shown, never read
} _MineCavernTargetWork;
STATIC_ASSERT_SIZEOF(_MineCavernTargetWork, 0x14C);

// Wedge count and perspective numerator of the cavern's glow fans.
enum {
    MINE_CAVERN_GLOW_WEDGES           = 8,
    MINE_CAVERN_GLOW_PROJECTION_SCALE = 352
};

// CAP command selected by the first event scene's skip callback.
enum { MINE_CAVERN_EVENT_SKIP_CAP_SLOT = 2 };

static void _mineCavernDrawFixedGlows(void);
static void _mineCavernUpdateTargetEffects(void);
static void _mineCavernDrawDarknessAndEffects(void);
static void _mineCavernUpdateTargetSound(s16 targetIndex);
static void _mineCavernSpawnTargets(Task* task);
static void _mineCavernTickTargetEffects(Task* unusedTask);
static void _mineCavernTargetExplode(Enemy* enemy, Task* task);
static void _mineCavernTargetRemainsTick(Enemy* enemy, Task* task);

static void _mineCavernTargetExitCallback(Task* task);

/// Mode byte the cavern enemy's hit check switches on: 1 skips the check and 2
/// hides the model and skips it. Its wider role is unproven.

/// Scratch-stack block the intact target's per-frame hit check works in.
///
/// The check reserves one for the length of its body. Both vectors are reused
/// as it goes: each first serves the tests made every frame, then the hit taken
/// this frame, and the remaining fields exist only while a hit is processed.
typedef struct {
    VECTOR  vec;              // Model's world position, where its lighting is queried; then the offset from the target to the player's model
    SVECTOR offset;           // Offset from the target to the player, for the lock-on range test; then the hit's contact point, made relative to the model's world position
    s32     playerDistance;   // Length of the player offset in `vec`; the range the damage roll is made at
    u32     hitKey;           // Key of the class-2 contact taken as a hit; 0 when there is none, its 0x8000 bit is set, or the location variant is neither 1 nor 4
    s32     destroyedTargets; // `GAME_FLAG_MINE_CAVERN_TARGETS_DESTROYED` nibble, one bit per target spot, while this target's bit is added to it
    s16     hitBearing;       // Bearing of the contact from the model on the XZ plane, relative to the model's facing and wrapped to -0x800..0x800; never read
    s16     damage;           // Hit points the hit costs: the damage roll's result, then replaced by the table entry for the key's low seven bits
} _MineCavernTargetHitScratch;
STATIC_ASSERT_SIZEOF(_MineCavernTargetHitScratch, 0x28);

/// Composes a target root and samples its room lighting into task-owned matrices.
///
/// taskPtr must be a stable Task* and position a writable VECTOR lvalue;
/// both are evaluated repeatedly without other argument side effects. Writes
/// XYZ in whole composed-coordinate units, retaining the VECTOR's fourth word,
/// and borrows live model/work/room data. Populates the model's three light rows.
/// Expands to a braced block; invoke only within a braced statement block.
#define MINE_CAVERN_TARGET_UPDATE_LIGHTING(taskPtr, position)                                                                          \
    {                                                                                                                                  \
        enum { MINE_CAVERN_TARGET_LIGHT_FIRST = 0,                                                                                     \
               MINE_CAVERN_TARGET_LIGHT_COUNT = 3 };                                                                                   \
        actorRenderComposeCoord((taskPtr)->extra.tmd->coords);                                                                         \
        (position).vx = (taskPtr)->extra.tmd->coords->workm.t[0];                                                                      \
        (position).vy = (taskPtr)->extra.tmd->coords->workm.t[1];                                                                      \
        (position).vz = (taskPtr)->extra.tmd->coords->workm.t[2];                                                                      \
        worldCoordSetModelLighting((taskPtr)->extra.tmd, &(position), MINE_CAVERN_TARGET_LIGHT_FIRST, MINE_CAVERN_TARGET_LIGHT_COUNT); \
    }

static WorldCollisionGrid     _gMineCavernCollisionGrid;
static WorldCollisionOccluder _gMineCavernSightOccluders[2];
static WorldCollisionTrigger  _gMineCavernRoom1ViewBoundaryTriggers[20];
static WorldCollisionTrigger  _gMineCavernRooms2And3ViewBoundaryTriggers[18];
static WorldCollisionTrigger  _gMineCavernActionTriggers[13];
static WorldCoordRoomLights   _gMineCavernRoomLights;

static void _mineCavernActivateRoom2(void);
static void _mineCavernScriptNoop(void);
static void _mineCavernPrepareFinalEventBattle(void);

TaskMessageEntry gMineCavernRoomMessages[7] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, mineCavernResolveTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, mineCavernRefuseKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, mineCavernHandleActionMessage },
    { ROOM_MESSAGE_COMMAND, mineCavernHandleCommandMessage },
    { ROOM_MESSAGE_ACTOR_EVENT, mineCavernAdvanceEvent },
    { ROOM_MESSAGE_SOUND, mineCavernHandleSoundMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gMineCavernCaptionProgressTasks[2] = {
    { .header = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, .callback = mineCavernCommitCaptionProgressTask, .data = { .value = 0 } },
    { .header = { .fields = { TASK_DESC_END, 0 } }, .callback = NULL, .data = { .value = 0 } },
};

static AnimationPackedPose _gMineCavernAnimation069D8Bank1[6] = {
#include "assets/mine_cavern_animation_069D8_bank1.inc"
};

static AnimationPackedRotation _gMineCavernAnimation069D8Bank4[46] = {
#include "assets/mine_cavern_animation_069D8_bank4.inc"
};

static AnimationRecord _gMineCavernAnimation069D8Records[109] = {
#include "assets/mine_cavern_animation_069D8_records.inc"
};

static u16 _gMineCavernAnimation069D8Indices[20] = {
#include "assets/mine_cavern_animation_069D8_indices.inc"
};

static AnimationSet _gMineCavernAnimation069D8 = {
    _gMineCavernAnimation069D8Records,
    _gMineCavernAnimation069D8Indices,
    { NULL, _gMineCavernAnimation069D8Bank1, NULL, NULL, _gMineCavernAnimation069D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMineCavernAnimation07178Bank1[13] = {
#include "assets/mine_cavern_animation_07178_bank1.inc"
};

static AnimationPackedRotation _gMineCavernAnimation07178Bank4[179] = {
#include "assets/mine_cavern_animation_07178_bank4.inc"
};

static AnimationRecord _gMineCavernAnimation07178Records[250] = {
#include "assets/mine_cavern_animation_07178_records.inc"
};

static u16 _gMineCavernAnimation07178Indices[20] = {
#include "assets/mine_cavern_animation_07178_indices.inc"
};

static AnimationSet _gMineCavernAnimation07178 = {
    _gMineCavernAnimation07178Records,
    _gMineCavernAnimation07178Indices,
    { NULL, _gMineCavernAnimation07178Bank1, NULL, NULL, _gMineCavernAnimation07178Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMineCavernAnimation07544Bank1[6] = {
#include "assets/mine_cavern_animation_07544_bank1.inc"
};

static AnimationPackedRotation _gMineCavernAnimation07544Bank4[64] = {
#include "assets/mine_cavern_animation_07544_bank4.inc"
};

static AnimationRecord _gMineCavernAnimation07544Records[141] = {
#include "assets/mine_cavern_animation_07544_records.inc"
};

static u16 _gMineCavernAnimation07544Indices[20] = {
#include "assets/mine_cavern_animation_07544_indices.inc"
};

static AnimationSet _gMineCavernAnimation07544 = {
    _gMineCavernAnimation07544Records,
    _gMineCavernAnimation07544Indices,
    { NULL, _gMineCavernAnimation07544Bank1, NULL, NULL, _gMineCavernAnimation07544Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMineCavernAnimation07784Bank1[4] = {
#include "assets/mine_cavern_animation_07784_bank1.inc"
};

static AnimationPackedRotation _gMineCavernAnimation07784Bank4[42] = {
#include "assets/mine_cavern_animation_07784_bank4.inc"
};

static AnimationRecord _gMineCavernAnimation07784Records[70] = {
#include "assets/mine_cavern_animation_07784_records.inc"
};

static u16 _gMineCavernAnimation07784Indices[20] = {
#include "assets/mine_cavern_animation_07784_indices.inc"
};

static AnimationSet _gMineCavernAnimation07784 = {
    _gMineCavernAnimation07784Records,
    _gMineCavernAnimation07784Indices,
    { NULL, _gMineCavernAnimation07784Bank1, NULL, NULL, _gMineCavernAnimation07784Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMineCavernAnimation07958Bank1[3] = {
#include "assets/mine_cavern_animation_07958_bank1.inc"
};

static AnimationPackedRotation _gMineCavernAnimation07958Bank4[31] = {
#include "assets/mine_cavern_animation_07958_bank4.inc"
};

static AnimationRecord _gMineCavernAnimation07958Records[57] = {
#include "assets/mine_cavern_animation_07958_records.inc"
};

static u16 _gMineCavernAnimation07958Indices[20] = {
#include "assets/mine_cavern_animation_07958_indices.inc"
};

static AnimationSet _gMineCavernAnimation07958 = {
    _gMineCavernAnimation07958Records,
    _gMineCavernAnimation07958Indices,
    { NULL, _gMineCavernAnimation07958Bank1, NULL, NULL, _gMineCavernAnimation07958Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMineCavernAnimation07BA4Bank1[3] = {
#include "assets/mine_cavern_animation_07BA4_bank1.inc"
};

static AnimationPackedRotation _gMineCavernAnimation07BA4Bank4[29] = {
#include "assets/mine_cavern_animation_07BA4_bank4.inc"
};

static AnimationRecord _gMineCavernAnimation07BA4Records[89] = {
#include "assets/mine_cavern_animation_07BA4_records.inc"
};

static u16 _gMineCavernAnimation07BA4Indices[20] = {
#include "assets/mine_cavern_animation_07BA4_indices.inc"
};

static AnimationSet _gMineCavernAnimation07BA4 = {
    _gMineCavernAnimation07BA4Records,
    _gMineCavernAnimation07BA4Indices,
    { NULL, _gMineCavernAnimation07BA4Bank1, NULL, NULL, _gMineCavernAnimation07BA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMineCavernAnimation08178Bank1[8] = {
#include "assets/mine_cavern_animation_08178_bank1.inc"
};

static AnimationPackedRotation _gMineCavernAnimation08178Bank4[117] = {
#include "assets/mine_cavern_animation_08178_bank4.inc"
};

static AnimationRecord _gMineCavernAnimation08178Records[212] = {
#include "assets/mine_cavern_animation_08178_records.inc"
};

static u16 _gMineCavernAnimation08178Indices[20] = {
#include "assets/mine_cavern_animation_08178_indices.inc"
};

static AnimationSet _gMineCavernAnimation08178 = {
    _gMineCavernAnimation08178Records,
    _gMineCavernAnimation08178Indices,
    { NULL, _gMineCavernAnimation08178Bank1, NULL, NULL, _gMineCavernAnimation08178Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMineCavernAnimation08420Bank1[4] = {
#include "assets/mine_cavern_animation_08420_bank1.inc"
};

static AnimationPackedRotation _gMineCavernAnimation08420Bank4[46] = {
#include "assets/mine_cavern_animation_08420_bank4.inc"
};

static AnimationRecord _gMineCavernAnimation08420Records[92] = {
#include "assets/mine_cavern_animation_08420_records.inc"
};

static u16 _gMineCavernAnimation08420Indices[20] = {
#include "assets/mine_cavern_animation_08420_indices.inc"
};

static AnimationSet _gMineCavernAnimation08420 = {
    _gMineCavernAnimation08420Records,
    _gMineCavernAnimation08420Indices,
    { NULL, _gMineCavernAnimation08420Bank1, NULL, NULL, _gMineCavernAnimation08420Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMineCavernAnimation08AC0Bank1[12] = {
#include "assets/mine_cavern_animation_08AC0_bank1.inc"
};

static AnimationPackedRotation _gMineCavernAnimation08AC0Bank4[164] = {
#include "assets/mine_cavern_animation_08AC0_bank4.inc"
};

static AnimationRecord _gMineCavernAnimation08AC0Records[204] = {
#include "assets/mine_cavern_animation_08AC0_records.inc"
};

static u16 _gMineCavernAnimation08AC0Indices[20] = {
#include "assets/mine_cavern_animation_08AC0_indices.inc"
};

static AnimationSet _gMineCavernAnimation08AC0 = {
    _gMineCavernAnimation08AC0Records,
    _gMineCavernAnimation08AC0Indices,
    { NULL, _gMineCavernAnimation08AC0Bank1, NULL, NULL, _gMineCavernAnimation08AC0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMineCavernAnimation08CB4Bank1[2] = {
#include "assets/mine_cavern_animation_08CB4_bank1.inc"
};

static AnimationPackedRotation _gMineCavernAnimation08CB4Bank4[20] = {
#include "assets/mine_cavern_animation_08CB4_bank4.inc"
};

static AnimationRecord _gMineCavernAnimation08CB4Records[79] = {
#include "assets/mine_cavern_animation_08CB4_records.inc"
};

static u16 _gMineCavernAnimation08CB4Indices[20] = {
#include "assets/mine_cavern_animation_08CB4_indices.inc"
};

static AnimationSet _gMineCavernAnimation08CB4 = {
    _gMineCavernAnimation08CB4Records,
    _gMineCavernAnimation08CB4Indices,
    { NULL, _gMineCavernAnimation08CB4Bank1, NULL, NULL, _gMineCavernAnimation08CB4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMineCavernAnimation092C4Bank1[11] = {
#include "assets/mine_cavern_animation_092C4_bank1.inc"
};

static AnimationPackedRotation _gMineCavernAnimation092C4Bank4[150] = {
#include "assets/mine_cavern_animation_092C4_bank4.inc"
};

static AnimationRecord _gMineCavernAnimation092C4Records[185] = {
#include "assets/mine_cavern_animation_092C4_records.inc"
};

static u16 _gMineCavernAnimation092C4Indices[20] = {
#include "assets/mine_cavern_animation_092C4_indices.inc"
};

static AnimationSet _gMineCavernAnimation092C4 = {
    _gMineCavernAnimation092C4Records,
    _gMineCavernAnimation092C4Indices,
    { NULL, _gMineCavernAnimation092C4Bank1, NULL, NULL, _gMineCavernAnimation092C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMineCavernAnimation096E4Bank1[8] = {
#include "assets/mine_cavern_animation_096E4_bank1.inc"
};

static AnimationPackedRotation _gMineCavernAnimation096E4Bank4[88] = {
#include "assets/mine_cavern_animation_096E4_bank4.inc"
};

static AnimationRecord _gMineCavernAnimation096E4Records[132] = {
#include "assets/mine_cavern_animation_096E4_records.inc"
};

static u16 _gMineCavernAnimation096E4Indices[20] = {
#include "assets/mine_cavern_animation_096E4_indices.inc"
};

static AnimationSet _gMineCavernAnimation096E4 = {
    _gMineCavernAnimation096E4Records,
    _gMineCavernAnimation096E4Indices,
    { NULL, _gMineCavernAnimation096E4Bank1, NULL, NULL, _gMineCavernAnimation096E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMineCavernAnimation099C8Bank1[5] = {
#include "assets/mine_cavern_animation_099C8_bank1.inc"
};

static AnimationPackedRotation _gMineCavernAnimation099C8Bank4[58] = {
#include "assets/mine_cavern_animation_099C8_bank4.inc"
};

static AnimationRecord _gMineCavernAnimation099C8Records[92] = {
#include "assets/mine_cavern_animation_099C8_records.inc"
};

static u16 _gMineCavernAnimation099C8Indices[20] = {
#include "assets/mine_cavern_animation_099C8_indices.inc"
};

static AnimationSet _gMineCavernAnimation099C8 = {
    _gMineCavernAnimation099C8Records,
    _gMineCavernAnimation099C8Indices,
    { NULL, _gMineCavernAnimation099C8Bank1, NULL, NULL, _gMineCavernAnimation099C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMineCavernAnimation09C2CBank1[2] = {
#include "assets/mine_cavern_animation_09C2C_bank1.inc"
};

static AnimationPackedRotation _gMineCavernAnimation09C2CBank4[22] = {
#include "assets/mine_cavern_animation_09C2C_bank4.inc"
};

static AnimationRecord _gMineCavernAnimation09C2CRecords[105] = {
#include "assets/mine_cavern_animation_09C2C_records.inc"
};

static u16 _gMineCavernAnimation09C2CIndices[20] = {
#include "assets/mine_cavern_animation_09C2C_indices.inc"
};

static AnimationSet _gMineCavernAnimation09C2C = {
    _gMineCavernAnimation09C2CRecords,
    _gMineCavernAnimation09C2CIndices,
    { NULL, _gMineCavernAnimation09C2CBank1, NULL, NULL, _gMineCavernAnimation09C2CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMineCavernAnimation09F1CBank1[4] = {
#include "assets/mine_cavern_animation_09F1C_bank1.inc"
};

static AnimationPackedRotation _gMineCavernAnimation09F1CBank4[40] = {
#include "assets/mine_cavern_animation_09F1C_bank4.inc"
};

static AnimationRecord _gMineCavernAnimation09F1CRecords[116] = {
#include "assets/mine_cavern_animation_09F1C_records.inc"
};

static u16 _gMineCavernAnimation09F1CIndices[20] = {
#include "assets/mine_cavern_animation_09F1C_indices.inc"
};

static AnimationSet _gMineCavernAnimation09F1C = {
    _gMineCavernAnimation09F1CRecords,
    _gMineCavernAnimation09F1CIndices,
    { NULL, _gMineCavernAnimation09F1CBank1, NULL, NULL, _gMineCavernAnimation09F1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMineCavernAnimation0A38CBank1[10] = {
#include "assets/mine_cavern_animation_0A38C_bank1.inc"
};

static AnimationPackedRotation _gMineCavernAnimation0A38CBank4[95] = {
#include "assets/mine_cavern_animation_0A38C_bank4.inc"
};

static AnimationRecord _gMineCavernAnimation0A38CRecords[139] = {
#include "assets/mine_cavern_animation_0A38C_records.inc"
};

static u16 _gMineCavernAnimation0A38CIndices[20] = {
#include "assets/mine_cavern_animation_0A38C_indices.inc"
};

static AnimationSet _gMineCavernAnimation0A38C = {
    _gMineCavernAnimation0A38CRecords,
    _gMineCavernAnimation0A38CIndices,
    { NULL, _gMineCavernAnimation0A38CBank1, NULL, NULL, _gMineCavernAnimation0A38CBank4, NULL, NULL, NULL },
};

/// Unused bodyless, priority-192 recipe for the cavern's timed sound task.
///
/// A single descriptor with no list terminator. The callback requires its live
/// task's elapsed-tick counter to start at zero and releases the task on skip
/// or expiry; this recipe carries no additional spawn data.
static TaskDesc _gMineCavernTimedSoundTaskDescriptor = {
    .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } },
    .callback = mineCavernTimedSoundTask,
    .data     = { .value = 0 }
};

/// Animation-set pointers installed in the player's equipped-weapon bank for cavern scenes.
///
/// The 17-word extension fills bank entries 47..63: its NULL first slot is
/// not playable, and the remaining entries supply clips 48..63. This counted
/// directory has no terminator. Copied pointers and their clip data remain
/// borrowed from the cavern, which must stay loaded through playback.
static AnimationSet* _gMineCavernSceneAnimationSets[17] = {
    NULL,
    &_gMineCavernAnimation069D8,
    &_gMineCavernAnimation07544,
    &_gMineCavernAnimation07784,
    &_gMineCavernAnimation07958,
    &_gMineCavernAnimation07BA4,
    &_gMineCavernAnimation08178,
    &_gMineCavernAnimation08420,
    &_gMineCavernAnimation07178,
    &_gMineCavernAnimation08AC0,
    &_gMineCavernAnimation08CB4,
    &_gMineCavernAnimation092C4,
    &_gMineCavernAnimation096E4,
    &_gMineCavernAnimation099C8,
    &_gMineCavernAnimation09C2C,
    &_gMineCavernAnimation09F1C,
    &_gMineCavernAnimation0A38C,
};

/// Installs the cavern's scene clips in the player's equipped-weapon bank extension.
///
/// Copies all 17 pointer words into slots 47..63 of the loaded, writable bank,
/// within its 32-word extension capacity. Slot 47 is NULL and is not playable;
/// slots 48..63 borrow room clips. Dispatch borrows this request and directory;
/// the cavern must remain loaded for subsequent playback of the copied pointers.
static AnimationBankCopyRequest _gMineCavernSceneAnimationBankCopyRequest = { _gMineCavernSceneAnimationSets, ARRAY_SIZE(_gMineCavernSceneAnimationSets) };

/// Unused bank-1, clip-1 request for immediate playback with grid collision disabled.
///
/// The reset transition ignores its zero blend duration. No current caller
/// establishes a receiver or resolves this stored bank selector.
static AnimationPlayRequest _gMineCavernUnusedBank1Clip1Request = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

enum {
    MINE_CAVERN_PLAYER_SCENE_CLIP         = ANIMATION_BANK_BASE_SET_COUNT + 1,
    MINE_CAVERN_PLAYER_SCENE_BLEND_FRAMES = 30
};

/// Blends the player into the cavern's baseline scene animation with grid collision disabled.
///
/// Uses extension slot 1 (clip 48), also the scripted move's arrival clip.
/// Scripts first install the room's animation extension in the equipped-weapon
/// bank; dispatch copies this request and replaces the retained bank selector.
/// The 30-frame blend and subsequent playback borrow the loaded room's clip.
static AnimationPlayRequest _gMineCavernPlayerSceneAnimationRequest = {
    { .index = 1 }, MINE_CAVERN_PLAYER_SCENE_CLIP, ANIMATION_BLEND_INTERPOLATE, MINE_CAVERN_PLAYER_SCENE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE
};

// Equipped-weapon extension slots and transition durations used by the intro.
enum {
    MINE_CAVERN_PLAYER_INTRO_CLIP49             = ANIMATION_BANK_BASE_SET_COUNT + 2,
    MINE_CAVERN_PLAYER_INTRO_CLIP50             = ANIMATION_BANK_BASE_SET_COUNT + 3,
    MINE_CAVERN_PLAYER_INTRO_CLIP51             = ANIMATION_BANK_BASE_SET_COUNT + 4,
    MINE_CAVERN_PLAYER_INTRO_CLIP52             = ANIMATION_BANK_BASE_SET_COUNT + 5,
    MINE_CAVERN_PLAYER_INTRO_CLIP53             = ANIMATION_BANK_BASE_SET_COUNT + 6,
    MINE_CAVERN_PLAYER_INTRO_CLIP54             = ANIMATION_BANK_BASE_SET_COUNT + 7,
    MINE_CAVERN_PLAYER_INTRO_QUICK_BLEND_FRAMES = 5,
    MINE_CAVERN_PLAYER_INTRO_LONG_BLEND_FRAMES  = 20,
    MINE_CAVERN_PLAYER_FINAL_BATTLE_AIM_CLIP    = 9
};

/// Player clip 49 at the intro's first animation cue, with a 30-frame blend.
///
/// Uses extension slot 2 after the scene bank copy, with grid collision disabled.
/// The interpreter copies the request and replaces its bank selector with the
/// equipped weapon's; the borrowed room clip must stay loaded through playback.
static AnimationPlayRequest _gMineCavernPlayerIntroClip49Request = { { .index = 1 }, MINE_CAVERN_PLAYER_INTRO_CLIP49, ANIMATION_BLEND_INTERPOLATE, MINE_CAVERN_PLAYER_SCENE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Player clip 50 when the intro starts area music, with a five-frame blend.
///
/// Uses extension slot 3 with grid collision disabled. Dispatch copies this
/// request and resolves the equipped-weapon bank; install the scene extension
/// first and keep its borrowed room clip loaded through playback.
static AnimationPlayRequest _gMineCavernPlayerIntroClip50Request = { { .index = 1 }, MINE_CAVERN_PLAYER_INTRO_CLIP50, ANIMATION_BLEND_INTERPOLATE, MINE_CAVERN_PLAYER_INTRO_QUICK_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Player clip 51 at the intro's timed pause, with a five-frame blend.
///
/// Uses extension slot 4 with grid collision disabled. Dispatch copies this
/// request and resolves the equipped-weapon bank; install the scene extension
/// first and keep its borrowed room clip loaded through playback.
static AnimationPlayRequest _gMineCavernPlayerIntroClip51Request = { { .index = 1 }, MINE_CAVERN_PLAYER_INTRO_CLIP51, ANIMATION_BLEND_INTERPOLATE, MINE_CAVERN_PLAYER_INTRO_QUICK_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Player clip 52 before the intro reveals the Blizzard Chaser, with a five-frame blend.
///
/// Uses extension slot 5 with grid collision disabled. Dispatch copies this
/// request and resolves the equipped-weapon bank; install the scene extension
/// first and keep its borrowed room clip loaded through playback.
static AnimationPlayRequest _gMineCavernPlayerIntroClip52Request = { { .index = 1 }, MINE_CAVERN_PLAYER_INTRO_CLIP52, ANIMATION_BLEND_INTERPOLATE, MINE_CAVERN_PLAYER_INTRO_QUICK_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Player clip 53 at the intro's battle placement, with a 20-frame blend.
///
/// Normal and skip paths use extension slot 6 with grid collision disabled.
/// Dispatch copies this request and resolves the equipped-weapon bank; install
/// the scene extension first and keep its borrowed room clip loaded through playback.
static AnimationPlayRequest _gMineCavernPlayerIntroClip53Request = { { .index = 1 }, MINE_CAVERN_PLAYER_INTRO_CLIP53, ANIMATION_BLEND_INTERPOLATE, MINE_CAVERN_PLAYER_INTRO_LONG_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Player clip 54 at the intro's second animation cue, with a 20-frame blend.
///
/// Uses extension slot 7 with grid collision disabled. Dispatch copies this
/// request and resolves the equipped-weapon bank; install the scene extension
/// first and keep its borrowed room clip loaded through playback.
static AnimationPlayRequest _gMineCavernPlayerIntroClip54Request = { { .index = 1 }, MINE_CAVERN_PLAYER_INTRO_CLIP54, ANIMATION_BLEND_INTERPOLATE, MINE_CAVERN_PLAYER_INTRO_LONG_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Unused bank-1, clip-55 request with a 30-frame blend and grid collision disabled.
///
/// Clip 55 is extension slot 8 if resolved against the player's installed
/// cavern scene bank. No current caller establishes that dispatch.
static AnimationPlayRequest _gMineCavernUnusedBank1Clip55Request = { { .index = 1 }, 55, ANIMATION_BLEND_INTERPOLATE, MINE_CAVERN_PLAYER_SCENE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

// Equipped-weapon extension indices used by the cavern's event playback.
enum {
    MINE_CAVERN_PLAYER_EVENT_BANK_PLACEHOLDER = 1,
    MINE_CAVERN_PLAYER_INTRO_APPROACH_CLIP    = ANIMATION_BANK_BASE_SET_COUNT + 8,
    MINE_CAVERN_PLAYER_FIRST_EVENT_CLIP56     = ANIMATION_BANK_BASE_SET_COUNT + 9,
    MINE_CAVERN_PLAYER_FIRST_EVENT_CLIP58     = ANIMATION_BANK_BASE_SET_COUNT + 11,
    MINE_CAVERN_PLAYER_SECOND_EVENT_CLIP62    = ANIMATION_BANK_BASE_SET_COUNT + 15,
    MINE_CAVERN_PLAYER_FIRST_EVENT_CLIP63     = ANIMATION_BANK_BASE_SET_COUNT + 16,
    MINE_CAVERN_PLAYER_EVENT_BLEND_FRAMES     = 5
};

/// Player clip 56 at the first event's CAP start, with a five-frame blend.
///
/// Uses extension slot 9 with grid collision disabled. Event dispatch copies
/// the request and substitutes the equipped-weapon bank. Install the scene
/// extension first and keep its borrowed room clip loaded through playback.
static AnimationPlayRequest _gMineCavernPlayerFirstEventClip56Request = {
    { .index = MINE_CAVERN_PLAYER_EVENT_BANK_PLACEHOLDER }, MINE_CAVERN_PLAYER_FIRST_EVENT_CLIP56, ANIMATION_BLEND_INTERPOLATE, MINE_CAVERN_PLAYER_EVENT_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE
};

/// Unused bank-1, clip-57 request with a five-frame blend and grid collision disabled.
///
/// Clip 57 is extension slot 10 if resolved against the player's installed
/// cavern scene bank. No current caller establishes that dispatch.
static AnimationPlayRequest _gMineCavernUnusedBank1Clip57Request = { { .index = 1 }, 57, ANIMATION_BLEND_INTERPOLATE, MINE_CAVERN_PLAYER_EVENT_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE };

/// Player clip 58 after the first event's alternate-health actor reset.
///
/// Blends for five frames into extension slot 11 with grid collision disabled.
/// Event dispatch copies the request and substitutes the equipped-weapon bank.
/// Install the scene extension first; its room clip stays borrowed through playback.
static AnimationPlayRequest _gMineCavernPlayerFirstEventClip58Request = {
    { .index = MINE_CAVERN_PLAYER_EVENT_BANK_PLACEHOLDER }, MINE_CAVERN_PLAYER_FIRST_EVENT_CLIP58, ANIMATION_BLEND_INTERPOLATE, MINE_CAVERN_PLAYER_EVENT_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE
};

/// Three unused bank-1 requests for clips 59..61 with grid collision disabled.
///
/// Clips 59 and 60 interpolate over five and two frames; clip 61 resets the
/// pose immediately. They correspond to scene-extension slots 12..14 if
/// resolved for the player, but no current caller establishes that dispatch.
static AnimationPlayRequest _gMineCavernUnusedBank1Clip59To61Requests[3] = {
    { { .index = 1 }, 59, ANIMATION_BLEND_INTERPOLATE, MINE_CAVERN_PLAYER_EVENT_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 60, ANIMATION_BLEND_INTERPOLATE, 2, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 61, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

/// Player clip 62 at view 19 of the second event, with an immediate pose reset.
///
/// Uses extension slot 15 with grid collision disabled; the zero blend duration
/// is ignored on reset. Dispatch copies the request and resolves the equipped
/// weapon's bank. The installed scene extension and room clip must remain loaded.
static AnimationPlayRequest _gMineCavernPlayerSecondEventClip62Request = {
    { .index = MINE_CAVERN_PLAYER_EVENT_BANK_PLACEHOLDER }, MINE_CAVERN_PLAYER_SECOND_EVENT_CLIP62, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE
};

/// Player clip 63 at view 16 of the first event, with an immediate pose reset.
///
/// Uses extension slot 16 with grid collision disabled; the zero blend duration
/// is ignored on reset. Dispatch copies the request and resolves the equipped
/// weapon's bank. The installed scene extension and room clip must remain loaded.
static AnimationPlayRequest _gMineCavernPlayerFirstEventClip63Request = {
    { .index = MINE_CAVERN_PLAYER_EVENT_BANK_PLACEHOLDER }, MINE_CAVERN_PLAYER_FIRST_EVENT_CLIP63, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE
};

/// Blends the player into the equipped weapon's aim clip before the final event battle.
///
/// Normal and skip paths use base clip 9 with a 30-frame blend and restore grid
/// collision. Dispatch copies the request and resolves its bank selector; the
/// equipped-weapon bank must remain loaded through playback.
static AnimationPlayRequest _gMineCavernPlayerFinalBattleAnimationRequest = { { .index = 1 }, MINE_CAVERN_PLAYER_FINAL_BATTLE_AIM_CLIP, ANIMATION_BLEND_INTERPOLATE, MINE_CAVERN_PLAYER_SCENE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE };

/// Player's initial placement for the cavern intro, facing along negative X.
///
/// XYZ is in whole room-coordinate units; yaw is -1/4 turn. Dispatch copies
/// position and Euler rotation to the player root and retains no record pointer.
static ActorTransform _gMineCavernPlayerIntroStartPlacement = { { 17410, 0, 1680, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Player's walk destination in the intro, also its placement when the intro is skipped.
///
/// XYZ is in whole room-coordinate units. The walk copies XYZ and ignores
/// rotation; the skip placement copies all Euler angles, with yaw -1/4 turn.
/// Both uses borrow this record only through dispatch.
static ActorTransform _gMineCavernPlayerIntroArrivalPlacement = { { 12710, 0, 1680, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Player placement at the first event's start, before its scene animation.
///
/// XYZ uses whole room-coordinate units; yaw is 569 in a 4096-unit turn.
/// Dispatch copies XYZ and all Euler angles, retaining no pointer to the record.
static ActorTransform _gMineCavernPlayerFirstEventStartPlacement = { { 6660, 0, 7200, 0 }, { 0, 569, 0, 0 } };

/// Player placement paired with clip 63 at view 16 of the first event.
///
/// XYZ uses whole room-coordinate units; yaw is 910 in a 4096-unit turn.
/// Dispatch copies XYZ and all Euler angles, retaining no pointer to the record.
static ActorTransform _gMineCavernPlayerFirstEventClip63Placement = { { 6550, 0, 7100, 0 }, { 0, 910, 0, 0 } };

/// Player placement shared by the final battle scene and its skip path.
///
/// XYZ is in whole room-coordinate units; yaw is 910 in a 4096-unit turn.
/// Dispatch copies position and Euler rotation to the player root, retaining no pointer.
static ActorTransform _gMineCavernPlayerFinalBattlePlacement = { { 3880, 0, 6678, 0 }, { 0, 910, 0, 0 } };

/// Player placement shared by the second event and its skip path.
///
/// XYZ uses whole room-coordinate units; yaw is -114 in a 4096-unit turn.
/// Dispatch copies XYZ and all Euler angles, retaining no pointer to the record.
static ActorTransform _gMineCavernPlayerSecondEventPlacement = { { 1900, 0, 160, 0 }, { 0, -114, 0, 0 } };

/// Player walk and arrival clips for the cavern intro's approach.
///
/// Clips 55 and 48 are equipped-weapon extension slots 8 and 1. Movement copies
/// their low halfwords and the destination XYZ, ignoring destination rotation.
/// Install the scene extension before walking; its clips remain borrowed during
/// playback, while this record is needed only through synchronous dispatch.
static GameActorMoveAnim _gMineCavernPlayerIntroMoveAnimations = {
    .approachAnimId = MINE_CAVERN_PLAYER_INTRO_APPROACH_CLIP,
    .arrivalAnimId  = MINE_CAVERN_PLAYER_SCENE_CLIP
};

/// Placement of the intro's darkened Blizzard Chaser before its normal-colour reveal.
///
/// Addresses placement slot 0. XYZ uses whole room units and yaw is +1/4 turn.
/// Dispatch copies the transform and records the heading, retaining no pointer.
static ActorTransform _gMineCavernBlizzardChaserIntroDarkenedPlacement = { { 7400, 0, 1750, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Blizzard Chaser placement when the intro restores its normal health and colour.
///
/// Addresses placement slot 0. XYZ uses whole room units and yaw is +1/4 turn.
/// Dispatch copies the transform and records the heading, retaining no pointer.
static ActorTransform _gMineCavernBlizzardChaserIntroRevealPlacement = { { 9250, 0, 1300, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Blizzard Chaser placement for the battle reached by the intro and its skip path.
///
/// Addresses cavern placement slot 0. XYZ is in whole room-coordinate units;
/// yaw is +1/4 turn. Dispatch copies the transform into the actor's root and
/// records its heading, retaining no pointer to this record.
static ActorTransform _gMineCavernBlizzardChaserIntroBattlePlacement = { { 9250, 0, 1800, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Blizzard Chaser placement shared by the first event and its skip path.
///
/// Addresses placement slot 0. XYZ uses whole room units and yaw is -455 in a
/// 4096-unit turn. Dispatch copies the transform and heading, retaining no pointer.
static ActorTransform _gMineCavernBlizzardChaserFirstEventPlacement = { { 7550, 0, 7360, 0 }, { 0, -455, 0, 0 } };

/// Blizzard Chaser placement shared by the second event and its skip path.
///
/// Addresses placement slot 0. XYZ uses whole room units and yaw is 910 in a
/// 4096-unit turn. Dispatch copies the transform and heading, retaining no pointer.
static ActorTransform _gMineCavernBlizzardChaserSecondEventPlacement = { { 1410, 0, 3000, 0 }, { 0, 910, 0, 0 } };

// Blizzard Chaser actions in the Mine/Shelter cavern command namespace.
enum {
    MINE_CAVERN_ACTOR_COMMAND_HIDE                   = 0,
    MINE_CAVERN_ACTOR_COMMAND_RESET_CLIP24           = 1,
    MINE_CAVERN_ACTOR_COMMAND_RESET_CLIP26_ALTERNATE = 3,
    MINE_CAVERN_ACTOR_COMMAND_PLAY_CLIP27            = 4,
    MINE_CAVERN_ACTOR_COMMAND_BURN                   = 5,
    MINE_CAVERN_ACTOR_COMMAND_RESET_CLIP25_ALTERNATE = 6,
    MINE_CAVERN_ACTOR_COMMAND_DISSOLVE               = 7,
    MINE_CAVERN_ACTOR_COMMAND_SCRIPTED_DISSOLVE      = 12,
    MINE_CAVERN_ACTOR_COMMAND_PLAY_CLIP24            = 10,
    MINE_CAVERN_ACTOR_COMMAND_SMOLDER                = 13
};

/// Zeroes the intro's Blizzard Chaser HP and selects its hidden state.
///
/// Addresses placement slot 0 in the Mine/Shelter cavern namespace.
/// The complete four-byte command is borrowed only through synchronous dispatch.
static ActorCommand _gMineCavernActorHideCommand = { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_CAVERN } }, MINE_CAVERN_ACTOR_COMMAND_HIDE };

/// Restores the Blizzard Chaser's normal health and restarts scripted clip 24.
///
/// Also restores normal colour, clears reaction flags and selects normal enemy
/// parameters. Addresses placement slot 0 in the Mine/Shelter cavern namespace;
/// the complete four-byte command is borrowed only through synchronous dispatch.
static ActorCommand _gMineCavernActorResetClip24Command = { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_CAVERN } }, MINE_CAVERN_ACTOR_COMMAND_RESET_CLIP24 };

enum { MINE_CAVERN_ACTOR_COMMAND_RESET_CLIP25 = 2 };

/// Restores the Blizzard Chaser's normal health and starts scripted clip 25.
///
/// Normal and skip intro paths address cavern placement slot 0 in the
/// Mine/Shelter cavern namespace. The receiver resets colour, reaction flags
/// and normal parameters, fills HP and restarts scripted state entry.
/// This record is borrowed only through synchronous dispatch.
static ActorCommand _gMineCavernActorResetClip25Command = { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_CAVERN } }, MINE_CAVERN_ACTOR_COMMAND_RESET_CLIP25 };

/// Restarts Blizzard Chaser clip 26 with alternate parameters and full alternate HP.
///
/// Restores normal colour and clears reaction flags without changing its burned
/// texture. Addresses placement slot 0 in the Mine/Shelter cavern namespace;
/// the complete four-byte command is borrowed only through synchronous dispatch.
static ActorCommand _gMineCavernActorResetClip26AlternateCommand = { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_CAVERN } }, MINE_CAVERN_ACTOR_COMMAND_RESET_CLIP26_ALTERNATE };

/// Restarts Blizzard Chaser clip 27, which advances to clip 29 when settled.
///
/// Restores normal colour and clears reaction flags, retaining HP and parameters.
/// Addresses placement slot 0 in the Mine/Shelter cavern namespace;
/// the complete four-byte command is borrowed only through synchronous dispatch.
static ActorCommand _gMineCavernActorPlayClip27Command = { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_CAVERN } }, MINE_CAVERN_ACTOR_COMMAND_PLAY_CLIP27 };

/// Starts the Blizzard Chaser's scripted burn and selects its burned texture/palette.
///
/// State entry plays clip 28 and timed burn bursts. Addresses placement slot 0
/// in the Mine/Shelter cavern namespace; the complete four-byte command is
/// borrowed only through synchronous dispatch.
static ActorCommand _gMineCavernActorBurnCommand = { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_CAVERN } }, MINE_CAVERN_ACTOR_COMMAND_BURN };

/// Unused cavern command restoring the Blizzard Chaser's alternate HP and clip 25.
///
/// Restores normal colour, clears reaction flags and selects alternate enemy
/// parameters before restarting scripted state entry. The four-byte record
/// belongs to the Mine/Shelter cavern command namespace; dispatch would borrow
/// it synchronously without retaining its address.
static ActorCommand _gMineCavernActorResetClip25AlternateCommand = { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_CAVERN } }, MINE_CAVERN_ACTOR_COMMAND_RESET_CLIP25_ALTERNATE };

/// Starts the Blizzard Chaser's death dissolve with its burned texture/palette.
///
/// State entry begins smoke, flattening and disappearance; a pending room event
/// can still be delivered. Addresses placement slot 0 in the Mine/Shelter cavern
/// namespace; the four-byte command is borrowed only through synchronous dispatch.
static ActorCommand _gMineCavernActorDissolveCommand = { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_CAVERN } }, MINE_CAVERN_ACTOR_COMMAND_DISSOLVE };

/// Restarts Blizzard Chaser clip 24 and darkens its scripted model colour.
///
/// Clears reaction flags but retains HP and parameters; darkening starts after
/// state entry. Addresses placement slot 0 in the Mine/Shelter cavern namespace;
/// the complete four-byte command is borrowed only through synchronous dispatch.
static ActorCommand _gMineCavernActorPlayClip24Command = { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_CAVERN } }, MINE_CAVERN_ACTOR_COMMAND_PLAY_CLIP24 };

enum { MINE_CAVERN_ACTOR_COMMAND_START_TURN = 11 };

/// Starts the placed Blizzard Chaser's turning behavior after a cavern scene.
///
/// The command belongs to the Mine/Shelter cavern namespace and addresses
/// placement slot 0. It starts turning with forward patrol/turn directions
/// and reverse watch direction; a burned actor also regains its alternate HP
/// and parameters. Normal and skip scripts borrow the record through dispatch.
static ActorCommand _gMineCavernActorStartTurnCommand = {
    { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_CAVERN } }, MINE_CAVERN_ACTOR_COMMAND_START_TURN
};

/// Unused cavern command starting the Blizzard Chaser's scripted dissolve.
///
/// Selects burned texture/palette and restarts the shorter dissolve sequence
/// without a room-event report. The four-byte record belongs to the Mine/Shelter
/// cavern command namespace; dispatch would borrow it synchronously.
static ActorCommand _gMineCavernActorScriptedDissolveCommand = { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_CAVERN } }, MINE_CAVERN_ACTOR_COMMAND_SCRIPTED_DISSOLVE };

/// Starts the Blizzard Chaser's scripted smolder with its burned texture/palette.
///
/// State entry plays clip 28 beneath a 300-tick smoke cycle. Addresses placement
/// slot 0 in the Mine/Shelter cavern namespace; the complete four-byte command
/// is borrowed only through synchronous dispatch.
static ActorCommand _gMineCavernActorSmolderCommand = { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_CAVERN } }, MINE_CAVERN_ACTOR_COMMAND_SMOLDER };

// Intro script selectors and waits, measured in event-task frames.
enum {
    MINE_CAVERN_EVENT_ACTOR_SLOT             = 0,
    MINE_CAVERN_INTRO_CAP_SLOT               = 1,
    MINE_CAVERN_INTRO_VIEW                   = 9,
    MINE_CAVERN_INTRO_BATTLE_VIEW            = 3,
    MINE_CAVERN_INTRO_COUNTDOWN_MUSIC_ENTRY  = 2,
    MINE_CAVERN_INTRO_ANIMATION_PAUSE_FRAMES = 20,
    MINE_CAVERN_INTRO_BATTLE_WAIT_FRAMES     = 3,
    MINE_CAVERN_INTRO_FADE_WAIT_FRAMES       = 8,
    MINE_CAVERN_INTRO_DEFAULT_FADE_DURATION  = 0 // Requests the interpreter's seven-frame ramp
};

EvsCommand gMineCavernIntroScript[41] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mineCavernSetAreaMusicEnabled }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMineCavernPlayerIntroStartPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gMineCavernSceneAnimationBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMineCavernPlayerSceneAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MINE_CAVERN_EVENT_ACTOR_SLOT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMineCavernActorHideCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = MINE_CAVERN_INTRO_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_MOVE_TO }, { .message = { .transform = &_gMineCavernPlayerIntroArrivalPlacement } }, { .message = { .moveAnimation = &_gMineCavernPlayerIntroMoveAnimations } } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = MINE_CAVERN_INTRO_CAP_SLOT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMineCavernPlayerIntroClip49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMineCavernPlayerIntroClip54Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMineCavernPlayerIntroClip50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = MINE_CAVERN_INTRO_ANIMATION_PAUSE_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMineCavernPlayerIntroClip51Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MINE_CAVERN_EVENT_ACTOR_SLOT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMineCavernActorPlayClip24Command } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MINE_CAVERN_EVENT_ACTOR_SLOT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMineCavernBlizzardChaserIntroDarkenedPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMineCavernPlayerIntroClip52Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MINE_CAVERN_EVENT_ACTOR_SLOT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMineCavernActorResetClip24Command } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MINE_CAVERN_EVENT_ACTOR_SLOT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMineCavernBlizzardChaserIntroRevealPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMineCavernPlayerIntroClip53Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MINE_CAVERN_EVENT_ACTOR_SLOT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMineCavernActorResetClip25Command } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MINE_CAVERN_EVENT_ACTOR_SLOT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMineCavernBlizzardChaserIntroBattlePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = mineCavernSelectCountdownMusicEntry }, { .value = MINE_CAVERN_INTRO_COUNTDOWN_MUSIC_ENTRY }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = mineCavernEngageScriptedBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = MINE_CAVERN_INTRO_BATTLE_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MINE_CAVERN_EVENT_ACTOR_SLOT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMineCavernActorStartTurnCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MINE_CAVERN_EVENT_ACTOR_SLOT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMineCavernBlizzardChaserIntroBattlePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = MINE_CAVERN_INTRO_BATTLE_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gMineCavernIntroSkipScript[19] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mineCavernSetAreaMusicEnabled }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = MINE_CAVERN_INTRO_DEFAULT_FADE_DURATION }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = MINE_CAVERN_INTRO_FADE_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMineCavernPlayerIntroArrivalPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gMineCavernSceneAnimationBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMineCavernPlayerIntroClip53Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MINE_CAVERN_EVENT_ACTOR_SLOT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMineCavernActorResetClip25Command } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MINE_CAVERN_EVENT_ACTOR_SLOT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMineCavernBlizzardChaserIntroBattlePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = mineCavernSelectCountdownMusicEntry }, { .value = MINE_CAVERN_INTRO_COUNTDOWN_MUSIC_ENTRY }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = MINE_CAVERN_INTRO_BATTLE_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = MINE_CAVERN_INTRO_FADE_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = mineCavernEngageScriptedBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = MINE_CAVERN_INTRO_BATTLE_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MINE_CAVERN_EVENT_ACTOR_SLOT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMineCavernActorStartTurnCommand } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

// Sound/CAP selectors shared by the normal and skipped event scenes.
enum {
    MINE_CAVERN_EVENT_ACTOR_SOUND_BANK   = 0x1E,
    MINE_CAVERN_EVENT_SCENE_MUSIC        = 11,
    MINE_CAVERN_FINAL_BATTLE_MUSIC_ENTRY = 4,
    MINE_CAVERN_SECOND_EVENT_CAP_SLOT    = 3
};

EvsCommand gMineCavernFirstEventScript[60] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mineCavernSetAreaMusicEnabled }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = mineCavernFadeOutMusic }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mineCavernReleaseEventBattleHold }, { .value = 31 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = mineCavernLockAttachmentsAndCancelEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMineCavernPlayerFirstEventStartPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gMineCavernSceneAnimationBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMineCavernPlayerSceneAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MINE_CAVERN_EVENT_ACTOR_SLOT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMineCavernBlizzardChaserFirstEventPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MINE_CAVERN_EVENT_ACTOR_SLOT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMineCavernActorBurnCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = MINE_CAVERN_EVENT_SKIP_CAP_SLOT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMineCavernPlayerFirstEventClip56Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MINE_CAVERN_EVENT_ACTOR_SLOT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMineCavernActorResetClip26AlternateCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_CHARACTER(MINE_CAVERN_EVENT_ACTOR_SOUND_BANK, 0x13) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMineCavernPlayerFirstEventClip58Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMineCavernPlayerFirstEventClip63Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMineCavernPlayerFirstEventClip63Placement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_CAVERN, 0x13) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMineCavernPlayerFinalBattlePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMineCavernPlayerFinalBattleAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MINE_CAVERN_EVENT_ACTOR_SLOT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMineCavernActorPlayClip27Command } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_CHARACTER(MINE_CAVERN_EVENT_ACTOR_SOUND_BANK, 0x14) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_CHARACTER(MINE_CAVERN_EVENT_ACTOR_SOUND_BANK, 0x15) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_CHARACTER(MINE_CAVERN_EVENT_ACTOR_SOUND_BANK, 0x15) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_CHARACTER(MINE_CAVERN_EVENT_ACTOR_SOUND_BANK, 0x14) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = SOUND_CHARACTER(MINE_CAVERN_EVENT_ACTOR_SOUND_BANK, 0x16) }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 100 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = mineCavernSetSceneMusicEvent }, { .value = MINE_CAVERN_EVENT_SCENE_MUSIC }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = mineCavernSelectCountdownMusicEntry }, { .value = MINE_CAVERN_FINAL_BATTLE_MUSIC_ENTRY }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineCavernPrepareFinalEventBattle }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = mineCavernEngageScriptedBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMineCavernActorStartTurnCommand } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gMineCavernFirstEventSkipScript[27] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mineCavernSetAreaMusicEnabled }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = mineCavernLockAttachmentsAndCancelEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMineCavernPlayerFinalBattlePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMineCavernPlayerFinalBattleAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MINE_CAVERN_EVENT_ACTOR_SLOT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMineCavernBlizzardChaserFirstEventPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MINE_CAVERN_EVENT_ACTOR_SLOT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMineCavernActorBurnCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MINE_CAVERN_EVENT_ACTOR_SLOT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMineCavernActorResetClip26AlternateCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MINE_CAVERN_EVENT_ACTOR_SLOT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMineCavernActorPlayClip27Command } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMineCavernActorStartTurnCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mineCavernReleaseEventBattleHold }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = mineCavernStartCaptionVariantOne }, { .value = MINE_CAVERN_EVENT_SKIP_CAP_SLOT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = mineCavernSetSceneMusicEvent }, { .value = MINE_CAVERN_EVENT_SCENE_MUSIC }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = mineCavernSelectCountdownMusicEntry }, { .value = MINE_CAVERN_FINAL_BATTLE_MUSIC_ENTRY }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineCavernPrepareFinalEventBattle }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = mineCavernEngageScriptedBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gMineCavernSecondEventScript[31] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mineCavernSetAreaMusicEnabled }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = mineCavernFadeOutMusic }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = mineCavernLockAttachmentsAndCancelEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMineCavernPlayerSecondEventPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gMineCavernSceneAnimationBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMineCavernPlayerSceneAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MINE_CAVERN_EVENT_ACTOR_SLOT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMineCavernBlizzardChaserSecondEventPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MINE_CAVERN_EVENT_ACTOR_SLOT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMineCavernActorSmolderCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 180 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMineCavernPlayerSecondEventClip62Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = MINE_CAVERN_SECOND_EVENT_CAP_SLOT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 90 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineCavernActivateRoom2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineCavernScriptNoop }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mineCavernReleaseEventBattleHold }, { .value = 95 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = mineCavernApplyPostEventAreaUpdates }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MINE_CAVERN_EVENT_ACTOR_SLOT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMineCavernActorDissolveCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gMineCavernSecondEventSkipScript[24] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mineCavernSetAreaMusicEnabled }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = mineCavernLockAttachmentsAndCancelEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMineCavernPlayerSecondEventPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gMineCavernSceneAnimationBankCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMineCavernPlayerSceneAnimationRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MINE_CAVERN_EVENT_ACTOR_SLOT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMineCavernBlizzardChaserSecondEventPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MINE_CAVERN_EVENT_ACTOR_SLOT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMineCavernActorSmolderCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MINE_CAVERN_EVENT_ACTOR_SLOT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMineCavernActorDissolveCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineCavernActivateRoom2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineCavernScriptNoop }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mineCavernReleaseEventBattleHold }, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = mineCavernApplyPostEventAreaUpdates }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// World-space points for the cavern's view-selected light glows and flash bursts.
///
/// Points 0..2 form a capsule polyline; points 3..10 and 12 are flare centres.
/// Point 11 is writable flash-spawn storage, randomized within a 64-unit cube
/// starting at (5990, -1460, -330). XYZ uses whole world-coordinate units.
/// Glow drawing borrows points synchronously; spawned effects retain the flash
/// address, so storage must remain loaded until those effects are released.
static SVECTOR _gMineCavernRoomLightPoints[13] = {
    { 970, -2060, -950, 0 },
    { 830, -1100, -1260, 0 },
    { 1080, -70, -710, 0 },
    { 4550, -2090, 280, 0 },
    { 0x36B0, -2060, 290, 0 },
    { 200, -2060, 5160, 0 },
    { 0x4560, -2060, 5000, 0 },
    { 9000, -2040, 8710, 0 },
    { 6910, -2780, 3700, 0 },
    { 6890, -1500, 4690, 0 },
    { 0x2E86, -1890, 5620, 0 },
    { 5990, -1460, -330, 0 },
    { 900, -1730, -0x36CE, 0 },
};

/// Initializes this overlay's three halo tint rows and retained trailing halfword.
///
/// Rows 0..2 give RGB right shifts of brightness (0 full, 1 half, 2 quarter).
/// Requires `ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE` = `RoomFxHaloStorage` and an empty
/// `ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND`. The trailing u16 is preserved
/// verbatim; its role is unproven and it is not a fourth tint row.
/// Bind around `room_visual_effects_halo_data.inc.c`, then undefine.
/// Expands to a constant braced initializer, with no arguments or side effects.
#define ROOM_VISUAL_EFFECTS_HALO_STORAGE_INITIALIZER { \
    .entries = {                                       \
        { .rShift = 0, .gShift = 1, .bShift = 2 },     \
        { .rShift = 2, .gShift = 1, .bShift = 0 },     \
        { .rShift = 0, .gShift = 2, .bShift = 1 },     \
    },                                                 \
    .alignmentFill = 0x3C95,                           \
}
/// Selects the halo tint storage record, including its retained trailing halfword.
///
/// Expands to the `RoomFxHaloStorage` type token; requires an empty
/// `ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND` and a
/// `ROOM_VISUAL_EFFECTS_HALO_STORAGE_INITIALIZER` for `entries` and `alignmentFill`.
/// Only the three RGB-shift rows are read, at indices 0..2; the halfword's
/// role is unproven. Include `room_visual_effects.h` first, bind around
/// `room_visual_effects_halo_data.inc.c`, then undefine.
#define ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE RoomFxHaloStorage
/// Declares one halo storage record, retaining the halfword after its tint rows.
///
/// This empty suffix requires `ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE` to be
/// `RoomFxHaloStorage`. Bind around `room_visual_effects_halo_data.inc.c`.
#define ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND
#include "../../shared/room/room_visual_effects_halo_data.inc.c"

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);
static void _glowDrawFlare(const SVECTOR* worldPoint, s32 textureIndex, s32 radiusScale);

/// Returns this overlay's three read-only halo tint rows for spawn indices 0..2.
static inline const RoomFxShade* _roomVisualEffectsGetHaloShades(void)
{
    return _gRoomVisualEffectsHaloShades.entries;
}
#undef ROOM_VISUAL_EFFECTS_HALO_STORAGE_INITIALIZER
#undef ROOM_VISUAL_EFFECTS_HALO_STORAGE_TYPE
#undef ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND

WorldCollisionRoomResources gMineCavernRoomCollisionResources[3] = {
    { &_gMineCavernCollisionGrid, _gMineCavernRoom1ViewBoundaryTriggers, _gMineCavernActionTriggers, _gMineCavernSightOccluders },
    { &_gMineCavernCollisionGrid, _gMineCavernRooms2And3ViewBoundaryTriggers, _gMineCavernActionTriggers, _gMineCavernSightOccluders },
    { &_gMineCavernCollisionGrid, _gMineCavernRooms2And3ViewBoundaryTriggers, _gMineCavernActionTriggers, _gMineCavernSightOccluders },
};

WorldCoordRoomLighting gMineCavernRoomLighting[3] = {
    { &_gMineCavernRoomLights, NULL },
    { &_gMineCavernRoomLights, NULL },
    { &_gMineCavernRoomLights, NULL },
};

/// Room 2's logical-view map to 1-based camera, image and sprite indices.
///
/// Logical views 2 and 3 are exchanged; the other views 1..25 map identically.
/// The final three stored zeros are outside the room's 25-view search extent,
/// not terminators. The room directory borrows this storage while loaded.
static u8 _gMineCavernRoom2ViewMap[28] = {
    1,
    3,
    2,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
    25,
    0,
    0,
    0,
};

/// Room 3's logical-view map to 1-based camera, image and sprite indices.
///
/// Exchanges logical views 2/3, 5/25 and 23/24; the other views 1..25 map
/// identically. The final three stored zeros lie outside the 25-view search
/// extent, not at a terminator. The room directory borrows this storage while loaded.
static u8 _gMineCavernRoom3ViewMap[28] = {
    1,
    3,
    2,
    4,
    25,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    24,
    23,
    5,
    0,
    0,
    0,
};

u8* gMineCavernRoomViewMaps[3] = {
    gViewIdentityMap,
    _gMineCavernRoom2ViewMap,
    _gMineCavernRoom3ViewMap,
};

enum { MINE_CAVERN_LOGICAL_VIEW_COUNT = 25 };

ViewCount gMineCavernRoomViewCounts[3] = {
    MINE_CAVERN_LOGICAL_VIEW_COUNT, MINE_CAVERN_LOGICAL_VIEW_COUNT, MINE_CAVERN_LOGICAL_VIEW_COUNT
};

enum { MINE_CAVERN_GORGE_WARP_MAP_FLAG = 0x1C0 };

DirectionWarpEntry gMineCavernWarpEntries[2] = {
    { { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 17700, 0, 2500 }, { 0, 0, 0, 0 }, { { .word = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4 }, 17700, 0, 2500 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_CAVERN, 2), SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_CAVERN, 1), DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, MINE_CAVERN_GORGE_WARP_MAP_FLAG },
    { { { .word = 0 }, 2840, 0, 0 }, { 0, 0, 0, 0 }, { { .word = 0 }, 2840, 0, 0 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_CAVERN, 4), SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_CAVERN, 3), SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_CAVERN, 0x12), 23, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_MINE_CAVERN },
};

enum {
    MINE_CAVERN_COLLISION_GRID_CELL_COUNT_X = 6,
    MINE_CAVERN_COLLISION_GRID_CELL_COUNT_Z = 3
};

/// Room-space plane normals for the cavern collision grid, with 4096 per unit.
static SVECTOR _gMineCavernCollisionGridNormals[30] = {
#include "assets/mine_cavern_collision_0C25C_normals.inc"
};

/// Signed room-space vertex pool for the cavern collision grid, in whole game units.
static SVECTOR _gMineCavernCollisionGridVerts[88] = {
#include "assets/mine_cavern_collision_0C25C_verts.inc"
};

/// Cavern collision quads indexing the grid's vertex and normal pools.
static WorldCollisionGridFace _gMineCavernCollisionGridFaces[38] = {
#include "assets/mine_cavern_collision_0C25C_faces.inc"
};

/// Packed face-index lists for the cavern grid's cells, each ending at -1.
static s16 _gMineCavernCollisionGridCells[188] = {
#include "assets/mine_cavern_collision_0C25C_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// `halfwordOffset` is an integer constant measured in `s16` elements of
/// `_gMineCavernCollisionGridCells` and occurs once in the expansion.
/// It must start a complete `WORLD_COLLISION_GRID_CELL_END`-terminated list
/// within the pool. Returns a borrowed `s16*` valid while this overlay is loaded.
/// Set `collisionSource.cell_face_list_macro` to this identifier; keep the
/// binding through the table include, then undefine it.
#define MINE_CAVERN_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gMineCavernCollisionGridCells[(halfwordOffset)])
/// X-major pointers to the cavern grid's 6 by 3 terminated face-index lists.
static s16* _gMineCavernCollisionGridTable[MINE_CAVERN_COLLISION_GRID_CELL_COUNT_X * MINE_CAVERN_COLLISION_GRID_CELL_COUNT_Z] = {
#include "assets/mine_cavern_collision_0C25C_table.inc"
};
#undef MINE_CAVERN_CELL_FACE_LIST_AT_OFFSET

/// Collision mesh and cell index shared by cavern rooms 1..3.
///
/// Covers 6 by 3 cells of 4000 whole game units, with origin (-1000, -1200)
/// on X/Z. All 38 faces are quads, referencing 88 vertices and 30 Q12 normals.
/// Activation binds the descriptor to the current view. The descriptor and
/// its private pools remain borrowed while the cavern overlay is loaded.
static WorldCollisionGrid _gMineCavernCollisionGrid = {
    NULL, _gMineCavernCollisionGridNormals, _gMineCavernCollisionGridVerts, _gMineCavernCollisionGridFaces, _gMineCavernCollisionGridTable, 1000, 1200, MINE_CAVERN_COLLISION_GRID_CELL_COUNT_X, MINE_CAVERN_COLLISION_GRID_CELL_COUNT_Z, 4000, ARRAY_SIZE(_gMineCavernCollisionGridFaces)
};

ViewCamera gMineCavernViewCameras[25] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -9000, 0x541A, -4500 } }, 269 },
    { { { { -4078, 0, -376 }, { -147, 3767, 1601 }, { 346, 1607, -3751 } }, { -0x3C2B, 3769, -9847 } }, 282 },
    { { { { 760, 0, -4024 }, { -1190, 3912, -224 }, { 3844, 1211, 726 } }, { -7741, 2840, -961 } }, 257 },
    { { { { 1072, 0, 3953 }, { 1176, 3910, -319 }, { -3774, 1219, 1023 } }, { -0x3909, 2740, -561 } }, 257 },
    { { { { -4077, 0, 384 }, { 137, 3822, 1464 }, { -358, 1470, -3806 } }, { -2391, 3550, -9951 } }, 257 },
    { { { { -688, 0, 4037 }, { 1013, 3964, 172 }, { -3908, 1028, -666 } }, { -0x3002, 3145, -8160 } }, 289 },
    { { { { -1126, 0, -3938 }, { -1263, 3879, 361 }, { 3729, 1314, -1066 } }, { -3683, 3259, -8447 } }, 282 },
    { { { { -945, 0, -3985 }, { -1292, 3874, 306 }, { 3770, 1328, -894 } }, { -8013, 3259, -8447 } }, 282 },
    { { { { 3340, 0, -2370 }, { -15, 4095, -21 }, { 2370, 26, 3339 } }, { -0x30EC, 1602, 1567 } }, 257 },
    { { { { 3539, 0, 2061 }, { 130, 4087, -223 }, { -2057, 258, 3532 } }, { -0x3825, 1489, 1491 } }, 551 },
    { { { { -6, 0, 4095 }, { 313, 4083, 0 }, { -4083, 313, -6 } }, { -0x30DE, 1598, -1721 } }, 257 },
    { { { { 2476, 0, -3262 }, { -2253, 2962, -1710 }, { 2359, 2828, 1791 } }, { -0x28A8, 3082, -638 } }, 197 },
    { { { { 1289, 0, 3887 }, { -518, 4059, 171 }, { -3853, -545, 1278 } }, { -0x3974, 602, -808 } }, 246 },
    { { { { -1648, 0, 3749 }, { 215, 4089, 94 }, { -3743, 235, -1645 } }, { -9183, 1059, -8459 } }, 269 },
    { { { { 3593, 0, -1965 }, { -1404, 2863, -2569 }, { 1373, 2928, 2512 } }, { -6222, 3044, -5668 } }, 254 },
    { { { { 841, 0, 4008 }, { -456, 4069, 95 }, { -3982, -466, 835 } }, { -7616, 1152, -6716 } }, 282 },
    { { { { 1753, 0, -3701 }, { 533, 4053, 252 }, { 3663, -590, 1735 } }, { -2707, 799, -5551 } }, 278 },
    { { { { 2645, 0, -3126 }, { -2616, 2242, -2214 }, { 1711, 3427, 1448 } }, { -12, 4448, 151 } }, 282 },
    { { { { -2728, 0, -3055 }, { -283, 4078, 253 }, { 3042, 380, -2716 } }, { -1023, 1439, -1057 } }, 285 },
    { { { { 4090, 0, -212 }, { -77, 3810, -1499 }, { 197, 1501, 3805 } }, { -0x3F71, 3033, 838 } }, 257 },
    { { { { 4068, 0, 476 }, { 189, 3756, -1621 }, { -436, 1632, 3731 } }, { -2741, 3275, 638 } }, 257 },
    { { { { 1072, 0, 3953 }, { 1176, 3910, -319 }, { -3774, 1219, 1023 } }, { -9591, 2740, -561 } }, 257 },
    { { { { -4084, 0, -300 }, { -69, 3985, 942 }, { 292, 944, -3974 } }, { -1641, 2934, -7401 } }, 257 },
    { { { { -4084, 0, -300 }, { -69, 3985, 942 }, { 292, 944, -3974 } }, { -1641, 2934, -7401 } }, 257 },
    { { { { -4077, 0, 384 }, { 137, 3822, 1464 }, { -358, 1470, -3806 } }, { -2391, 3550, -9951 } }, 257 },
};

// Eight-bit texture pages, shared palette and neutral sprite modulation.
enum {
    MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896 = getTPage(1, 0, 896, 0),
    MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960 = getTPage(1, 0, 960, 0),
    MINE_CAVERN_SPRITE_CLUT              = getClut(0, 255),
    MINE_CAVERN_PROGRESS_SPRITE_CLUT     = getClut(0, 256),
    MINE_CAVERN_SPRITE_NEUTRAL_COLOR     = 128
};

/// Sprite drawing batches for cavern mapped view 1.
///
/// The zero-count first batch selects decoded background strips; the next
/// record ends the list. The source slot retains this list's address without
/// supplying any source elements. Storage belongs to the loaded cavern.
static SpriteBatch _gMineCavernView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for cavern mapped view 2.
///
/// The batches cover all 49 elements. Positions and dimensions are pixels
/// relative to the view centre; depths are unscaled OT sorting values.
/// The loaded cavern owns these writable source records.
static SpriteSource _gMineCavernView2SpriteSources[49] = {
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 8 } }, -104, -16, 1376, { .fields = { 96, 104 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 40 } }, -152, -104, 1264, { .fields = { 104, 48 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, -128, -72, 1343, { .fields = { 88, 32 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 24 } }, -152, -32, 1240, { .fields = { 96, 80 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 24 } }, -160, -32, 1207, { .fields = { 88, 80 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, -120, 8, 1422, { .fields = { 88, 56 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, -120, -16, 1421, { .fields = { 64, 96 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, -112, 32, 1485, { .fields = { 72, 224 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 32 } }, -104, 56, 1483, { .fields = { 120, 184 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 24 } }, -96, 56, 1516, { .fields = { 96, 216 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 32 } }, -128, 56, 1486, { .fields = { 96, 120 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, -128, 32, 1483, { .fields = { 88, 152 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 48 } }, -136, -16, 1404, { .fields = { 112, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 32 } }, -120, -48, 1344, { .fields = { 96, 184 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 32 } }, -128, -48, 1307, { .fields = { 120, 120 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 32 } }, -128, -104, 1221, { .fields = { 104, 88 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 16 } }, -160, -120, 1171, { .fields = { 32, 216 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 16 } }, -120, -120, 1187, { .fields = { 32, 184 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 24 } }, -136, -40, 1278, { .fields = { 88, 200 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 24 } }, -144, -40, 1258, { .fields = { 88, 224 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, -144, -64, 1271, { .fields = { 80, 176 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 80, 16 } }, 80, -48, 1317, { .fields = { 120, 200 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 32 } }, 8, 8, 1516, { .fields = { 104, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 8 } }, 24, 0, 1533, { .fields = { 56, 168 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 24 } }, 16, 8, 1375, { .fields = { 64, 72 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, 72, -24, 1220, { .fields = { 56, 232 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, 72, -8, 1166, { .fields = { 72, 152 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, 72, 16, 1149, { .fields = { 88, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, 72, 48, 1190, { .fields = { 104, 216 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, 72, 80, 1247, { .fields = { 72, 200 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, 72, 104, 1223, { .fields = { 48, 72 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 32 } }, 40, -24, 1660, { .fields = { 104, 152 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 32 } }, 64, -24, 1355, { .fields = { 120, 216 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 24 } }, 40, 8, 1357, { .fields = { 40, 144 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 24 } }, 48, 96, 1241, { .fields = { 56, 176 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, 152, 72, 1399, { .fields = { 104, 32 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 8 } }, 56, -32, 1317, { .fields = { 72, 248 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 24 } }, 16, 64, 1372, { .fields = { 48, 24 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 8 } }, 32, 88, 1361, { .fields = { 104, 248 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 16 } }, 16, 48, 1350, { .fields = { 24, 88 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 16 } }, 16, 32, 1366, { .fields = { 24, 104 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, 56, 32, 1407, { .fields = { 80, 128 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, 56, 56, 1393, { .fields = { 80, 104 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, 56, 80, 1319, { .fields = { 40, 232 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 72, 16 } }, 88, -24, 1317, { .fields = { 112, 168 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 72, 24 } }, 88, -8, 1224, { .fields = { 16, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 72, 24 } }, 88, 16, 1197, { .fields = { 16, 48 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 64, 8 } }, 96, 64, 1316, { .fields = { 8, 248 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 72, 24 } }, 88, 40, 1223, { .fields = { 8, 120 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite drawing batches for cavern mapped view 2.
///
/// The zero-count first batch selects decoded background strips. Subsequent
/// ranges cover all 49 source sprites and end with `SPRITE_BATCH_END`.
/// The loaded cavern owns this writable list.
static SpriteBatch _gMineCavernView2SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 1, 0 } },
    { 21, ARRAY_SIZE(_gMineCavernView2SpriteSources) - 21, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for cavern mapped view 3.
///
/// The batches cover all 64 elements. Positions and dimensions are pixels
/// relative to the view centre; depths are unscaled OT sorting values.
/// The loaded cavern owns these writable source records.
static SpriteSource _gMineCavernView3SpriteSources[64] = {
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 56 } }, 144, -24, 519, { .fields = { 80, 128 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 40 } }, 136, 32, 591, { .fields = { 0, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 48 } }, 128, 72, 599, { .fields = { 24, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 16 } }, 80, -120, 519, { .fields = { 64, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 16 } }, 120, -120, 506, { .fields = { 72, 120 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 16 } }, 48, -120, 531, { .fields = { 72, 16 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 16 } }, 48, -104, 564, { .fields = { 80, 48 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, 72, -104, 584, { .fields = { 104, 48 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, 88, -88, 569, { .fields = { 104, 24 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, 104, -80, 554, { .fields = { 96, 96 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, 120, -72, 553, { .fields = { 96, 72 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 16 } }, 136, -64, 549, { .fields = { 8, 240 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, 152, -80, 519, { .fields = { 120, 24 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, 144, -48, 533, { .fields = { 48, 232 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 24 } }, 48, -96, 1121, { .fields = { 104, 224 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 32 } }, 80, -96, 1116, { .fields = { 112, 104 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 56 } }, 104, -120, 1058, { .fields = { 48, 128 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 48 } }, 96, -64, 1123, { .fields = { 40, 56 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 56 } }, 88, -16, 1195, { .fields = { 64, 184 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 48 } }, 80, 40, 1294, { .fields = { 32, 200 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 48 } }, 96, 40, 1176, { .fields = { 24, 104 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 24 } }, 88, -120, 1611, { .fields = { 104, 136 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 32 } }, 48, -96, 1705, { .fields = { 104, 168 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 64 } }, 80, -96, 1688, { .fields = { 72, 64 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 72 } }, 72, -32, 1767, { .fields = { 96, 144 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 56 } }, 72, -88, 1462, { .fields = { 56, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 48 } }, -160, -88, 791, { .fields = { 24, 152 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 24 } }, -160, -40, 803, { .fields = { 104, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 32 } }, -24, 0, 1917, { .fields = { 16, 136 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, -40, -40, 1950, { .fields = { 96, 200 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, -32, -16, 1924, { .fields = { 88, 32 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 40 } }, -40, 0, 1693, { .fields = { 96, 216 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -40, -16, 1706, { .fields = { 64, 240 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 8 } }, -80, -64, 1699, { .fields = { 40, 48 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 8 } }, -136, -40, 1465, { .fields = { 96, 136 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 16 } }, -120, -48, 1504, { .fields = { 80, 240 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 16 } }, -96, -48, 1654, { .fields = { 80, 168 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 16 } }, -72, -48, 1673, { .fields = { 80, 152 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, -56, -32, 1791, { .fields = { 8, 40 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, -56, 0, 1697, { .fields = { 8, 104 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, -56, 32, 1467, { .fields = { 8, 72 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, -136, -32, 799, { .fields = { 88, 136 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 64 } }, -160, -16, 785, { .fields = { 72, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 72 } }, -160, 48, 866, { .fields = { 104, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 64 } }, -136, -16, 810, { .fields = { 96, 72 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 72 } }, -136, 48, 864, { .fields = { 112, 72 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 64 } }, -120, -16, 845, { .fields = { 96, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 72 } }, -120, 48, 861, { .fields = { 120, 144 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 16 } }, -120, -32, 1436, { .fields = { 80, 184 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 16 } }, -96, -32, 1514, { .fields = { 80, 224 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, -72, -32, 1722, { .fields = { 24, 88 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 40 } }, -112, 40, 969, { .fields = { 8, 200 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 24 } }, -112, 80, 1084, { .fields = { 112, 200 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 16 } }, -112, 104, 956, { .fields = { 72, 240 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 32 } }, -112, -16, 1446, { .fields = { 112, 72 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 24 } }, -112, 16, 1197, { .fields = { 48, 104 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 40 } }, -88, -16, 1515, { .fields = { 112, 216 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, -88, 24, 1379, { .fields = { 8, 168 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 48 } }, -88, 56, 1095, { .fields = { 48, 184 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, -72, -16, 1689, { .fields = { 0, 136 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, -72, 16, 1409, { .fields = { 120, 40 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 40 } }, -72, 48, 1245, { .fields = { 24, 48 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 8 } }, -96, -56, 1691, { .fields = { 104, 248 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 8 } }, -120, -56, 1631, { .fields = { 104, 160 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite drawing batches for cavern mapped view 3.
///
/// The zero-count first batch selects decoded background strips. Subsequent
/// ranges cover all 64 source sprites and end with `SPRITE_BATCH_END`.
/// The loaded cavern owns this writable list.
static SpriteBatch _gMineCavernView3SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 0, 0 } },
    { 14, 7, 0, 0, { 3, 0 } },
    { 21, 4, 0, 0, { 2, 0 } },
    { 25, 1, 0, 0, { 4, 0 } },
    { 26, ARRAY_SIZE(_gMineCavernView3SpriteSources) - 26, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for cavern mapped view 4.
///
/// The batches cover all 49 elements. Positions and dimensions are pixels
/// relative to the view centre; depths are unscaled OT sorting values.
/// The loaded cavern owns these writable source records.
/// The last element uses the separate nursery-progress palette row.
static SpriteSource _gMineCavernView4SpriteSources[49] = {
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 8 } }, -24, -24, 2965, { .fields = { 88, 248 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, 144, -40, 1045, { .fields = { 72, 16 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, 96, -88, 1093, { .fields = { 96, 80 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, 88, -72, 1107, { .fields = { 64, 144 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, 80, -88, 2170, { .fields = { 80, 72 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, 64, -96, 2174, { .fields = { 96, 112 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, 48, -96, 2137, { .fields = { 88, 144 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 32 } }, 24, -96, 2093, { .fields = { 24, 224 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 32 } }, 16, -96, 2239, { .fields = { 72, 144 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 88 } }, 16, -64, 2297, { .fields = { 120, 88 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 56 } }, 0, -96, 2037, { .fields = { 16, 112 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 56 } }, 0, -40, 2003, { .fields = { 32, 112 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 48 } }, -8, -48, 2384, { .fields = { 56, 208 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 40 } }, -16, -48, 2936, { .fields = { 48, 208 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 40 } }, -8, -88, 2379, { .fields = { 112, 48 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 72 } }, 128, 48, 913, { .fields = { 64, 176 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 72 } }, 128, -24, 982, { .fields = { 48, 72 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 88 } }, 104, -56, 1274, { .fields = { 104, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 88 } }, 104, 32, 997, { .fields = { 96, 88 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 88 } }, 88, 24, 1051, { .fields = { 80, 88 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 80 } }, 88, -56, 1089, { .fields = { 96, 176 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 72 } }, 72, -64, 1116, { .fields = { 72, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 88 } }, 72, 8, 1193, { .fields = { 88, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 48 } }, 24, -64, 2661, { .fields = { 0, 208 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 48 } }, 24, -16, 1737, { .fields = { 120, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 64 } }, 40, -16, 1554, { .fields = { 48, 144 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 48 } }, 40, -64, 2681, { .fields = { 104, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 64 } }, 56, -64, 2178, { .fields = { 56, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 80 } }, 56, 0, 1301, { .fields = { 112, 176 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, 112, -88, 1250, { .fields = { 96, 48 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 24 } }, -128, -120, 2187, { .fields = { 56, 56 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, -72, -96, 2175, { .fields = { 72, 144 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 48 } }, -104, -96, 2175, { .fields = { 96, 208 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 56 } }, -104, -48, 1919, { .fields = { 16, 56 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 48, 16 } }, -64, -120, 453, { .fields = { 48, 232 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 48, 16 } }, -96, -104, 447, { .fields = { 48, 128 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 48, 16 } }, -120, -88, 445, { .fields = { 40, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 24 } }, -136, -72, 445, { .fields = { 56, 104 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 56 } }, -160, -88, 444, { .fields = { 8, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, -160, -120, 424, { .fields = { 88, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 56 } }, -160, -32, 454, { .fields = { 16, 168 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 48 } }, -160, 24, 460, { .fields = { 112, 112 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 48 } }, -160, 72, 488, { .fields = { 104, 160 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 24 } }, -120, -120, 1546, { .fields = { 72, 32 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 24 } }, -120, -96, 1575, { .fields = { 64, 80 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 24 } }, -88, -96, 1598, { .fields = { 64, 208 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 56 } }, -104, -16, 1713, { .fields = { 32, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 56 } }, -112, -72, 1600, { .fields = { 120, 56 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_PROGRESS_SPRITE_CLUT, { .fields = { 48, 32 } }, -88, -56, 4500, { .fields = { 56, 176 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite drawing batches for cavern mapped view 4.
///
/// The zero-count first batch selects decoded background strips. Subsequent
/// ranges cover all 49 source sprites and end with `SPRITE_BATCH_END`.
/// The loaded cavern owns this writable list.
/// Nursery progress controls the last range (source element 48).
static SpriteBatch _gMineCavernView4SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 30, 0, 0, { 0, 0 } },
    { 30, 4, 0, 0, { 3, 0 } },
    { 34, 9, 0, 0, { 2, 0 } },
    { 43, 5, 0, 0, { 4, 0 } },
    { 48, ARRAY_SIZE(_gMineCavernView4SpriteSources) - 48, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for cavern mapped views 5 and 25.
///
/// The batches cover all 58 elements. Positions and dimensions are pixels
/// relative to the view centre; depths are unscaled OT sorting values.
/// The loaded cavern owns these writable source records.
/// The last two elements use the separate nursery-progress palette row.
static SpriteSource _gMineCavernView5SpriteSources[58] = {
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 8 } }, 32, -72, 2490, { .fields = { 16, 184 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 40 } }, 104, -120, 1149, { .fields = { 56, 216 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, 112, -80, 1208, { .fields = { 40, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, 112, -48, 1213, { .fields = { 40, 160 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, 128, -40, 963, { .fields = { 40, 128 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, 144, -32, 877, { .fields = { 24, 160 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, 96, -80, 1231, { .fields = { 0, 184 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, 96, -56, 1200, { .fields = { 32, 64 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, 80, -56, 1370, { .fields = { 112, 184 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 48 } }, 56, -120, 1764, { .fields = { 56, 96 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 32 } }, 72, -72, 1567, { .fields = { 48, 64 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, 56, -72, 1726, { .fields = { 40, 96 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, 40, -72, 2111, { .fields = { 88, 96 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 24 } }, 112, -16, 1314, { .fields = { 0, 32 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 32 } }, 88, -24, 1302, { .fields = { 8, 64 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 32 } }, 80, 8, 1368, { .fields = { 24, 224 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 24 } }, 80, 40, 1427, { .fields = { 120, 224 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 8 } }, 88, 64, 1427, { .fields = { 0, 248 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 64 } }, 48, -40, 2005, { .fields = { 104, 96 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 16 } }, 136, 0, 650, { .fields = { 120, 208 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 40 } }, 96, 0, 686, { .fields = { 56, 40 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 16 } }, 112, 0, 625, { .fields = { 80, 144 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 48 } }, 136, 16, 552, { .fields = { 56, 144 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 32 } }, 112, 16, 621, { .fields = { 0, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, 96, 40, 750, { .fields = { 24, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 48 } }, 88, 72, 803, { .fields = { 72, 48 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 40 } }, 144, 80, 1070, { .fields = { 88, 216 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 64, 16 } }, 96, 104, 836, { .fields = { 56, 208 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 24 } }, 72, -120, 1134, { .fields = { 120, 160 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 32 } }, 88, -96, 1185, { .fields = { 16, 192 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 24 } }, 32, -120, 1736, { .fields = { 112, 128 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 24 } }, 48, -96, 1800, { .fields = { 56, 192 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 32 } }, 80, -40, 1300, { .fields = { 16, 128 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 48 } }, -160, -80, 1200, { .fields = { 80, 96 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 48 } }, -136, -80, 1432, { .fields = { 96, 48 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 40 } }, -136, -32, 1464, { .fields = { 56, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 40 } }, -160, -32, 1325, { .fields = { 104, 216 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 48 } }, -136, 8, 1243, { .fields = { 72, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 48 } }, -160, 8, 1213, { .fields = { 88, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 96 } }, -120, -72, 1677, { .fields = { 112, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, -120, 24, 1337, { .fields = { 24, 96 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 16 } }, -104, -24, 1737, { .fields = { 112, 80 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 16 } }, -88, -8, 1613, { .fields = { 80, 152 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 16 } }, -96, 8, 1387, { .fields = { 80, 168 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 32 } }, -104, -8, 1804, { .fields = { 48, 192 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, -56, 8, 1904, { .fields = { 56, 80 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, -56, 24, 1717, { .fields = { 104, 224 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, -56, 40, 1563, { .fields = { 104, 240 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, -56, 56, 1450, { .fields = { 96, 80 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -56, 72, 1293, { .fields = { 104, 128 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -56, 88, 1235, { .fields = { 104, 184 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 48, 32 } }, -104, 24, 1292, { .fields = { 8, 32 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 56, 24 } }, -112, 56, 1255, { .fields = { 80, 56 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 56, 24 } }, -112, 80, 1209, { .fields = { 72, 24 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 48, 24 } }, -160, 56, 1237, { .fields = { 72, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 48, 32 } }, -160, 80, 1188, { .fields = { 104, 96 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_PROGRESS_SPRITE_CLUT, { .fields = { 48, 56 } }, 8, -48, 3000, { .fields = { 80, 160 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_PROGRESS_SPRITE_CLUT, { .fields = { 8, 24 } }, 64, -16, 2500, { .fields = { 120, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite drawing batches for cavern mapped views 5 and 25.
///
/// The zero-count first batch selects decoded background strips. Subsequent
/// ranges cover all 58 source sprites and end with `SPRITE_BATCH_END`.
/// The loaded cavern owns this writable list.
/// Nursery progress controls the last range (source elements 56 and 57).
static SpriteBatch _gMineCavernView5SpriteBatches[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 3, 0 } },
    { 19, 8, 0, 0, { 0, 0 } },
    { 27, 1, 0, 0, { 5, 0 } },
    { 28, 5, 0, 0, { 1, 0 } },
    { 33, 23, 0, 0, { 4, 0 } },
    { 56, ARRAY_SIZE(_gMineCavernView5SpriteSources) - 56, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for cavern mapped view 6.
///
/// The batches cover all 41 elements. Positions and dimensions are pixels
/// relative to the view centre; depths are unscaled OT sorting values.
/// The loaded cavern owns these writable source records.
static SpriteSource _gMineCavernView6SpriteSources[41] = {
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 8 } }, -32, 0, 2417, { .fields = { 72, 200 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 8 } }, -160, 88, 1195, { .fields = { 72, 48 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 40 } }, -160, -72, 1599, { .fields = { 64, 216 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 48 } }, -136, -80, 1569, { .fields = { 72, 56 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 48 } }, -112, -80, 1466, { .fields = { 96, 56 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 24 } }, -112, -32, 1502, { .fields = { 48, 184 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 64 } }, -120, -8, 1601, { .fields = { 112, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 24 } }, -160, -8, 1299, { .fields = { 32, 136 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, -136, -8, 1469, { .fields = { 40, 112 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 24 } }, -160, -32, 1641, { .fields = { 32, 24 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 24 } }, -136, -32, 1698, { .fields = { 40, 64 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 24 } }, -160, 96, 1187, { .fields = { 48, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 32 } }, -152, 88, 1235, { .fields = { 56, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 32 } }, -144, 80, 1287, { .fields = { 64, 168 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 32 } }, -136, 72, 1343, { .fields = { 56, 32 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 40 } }, -128, 64, 1406, { .fields = { 88, 216 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 32 } }, -120, 56, 1471, { .fields = { 56, 200 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 8 } }, -160, 80, 1255, { .fields = { 112, 240 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 8 } }, -160, 72, 1320, { .fields = { 32, 48 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 8 } }, -152, 64, 1394, { .fields = { 80, 104 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 8 } }, -136, 56, 1458, { .fields = { 112, 248 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 56 } }, -160, 16, 1445, { .fields = { 104, 56 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 48 } }, -152, 16, 1453, { .fields = { 64, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 40 } }, -136, 16, 1639, { .fields = { 56, 128 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 24 } }, -112, 56, 1546, { .fields = { 48, 88 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 48 } }, -104, -80, 1535, { .fields = { 80, 168 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 56 } }, -104, -32, 1620, { .fields = { 80, 112 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 56 } }, -104, 24, 1731, { .fields = { 80, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 40 } }, -72, 40, 1654, { .fields = { 56, 88 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 40 } }, -56, 32, 1696, { .fields = { 64, 48 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 40 } }, -48, 24, 1829, { .fields = { 96, 216 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 40 } }, -40, 16, 1985, { .fields = { 104, 216 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 32 } }, -32, 8, 2198, { .fields = { 56, 168 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 24 } }, -24, 8, 2416, { .fields = { 56, 232 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 64 } }, -72, -80, 1694, { .fields = { 112, 64 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 56 } }, -72, -16, 1712, { .fields = { 112, 128 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 56 } }, -56, -80, 1878, { .fields = { 112, 184 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 48 } }, -48, -72, 1977, { .fields = { 72, 152 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 56 } }, -56, -24, 2052, { .fields = { 120, 184 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 48 } }, -48, -24, 2065, { .fields = { 72, 104 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 24 } }, -40, -8, 2300, { .fields = { 48, 160 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite drawing batches for cavern mapped view 6.
///
/// The zero-count first batch selects decoded background strips. Subsequent
/// ranges cover all 41 source sprites and end with `SPRITE_BATCH_END`.
/// The loaded cavern owns this writable list.
static SpriteBatch _gMineCavernView6SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gMineCavernView6SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for cavern mapped view 7.
///
/// The batches cover all 39 elements. Positions and dimensions are pixels
/// relative to the view centre; depths are unscaled OT sorting values.
/// The loaded cavern owns these writable source records.
static SpriteSource _gMineCavernView7SpriteSources[39] = {
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 48 } }, -16, -56, 2855, { .fields = { 104, 192 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 72 } }, -8, -64, 2755, { .fields = { 120, 72 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 16 } }, 0, -72, 2761, { .fields = { 24, 152 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 16 } }, 32, -72, 2137, { .fields = { 24, 168 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 56, 24 } }, 104, 96, 993, { .fields = { 24, 104 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 48 } }, 136, 48, 995, { .fields = { 80, 96 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 40 } }, 136, 8, 949, { .fields = { 72, 144 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 72 } }, 152, -64, 896, { .fields = { 120, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, 120, 64, 1307, { .fields = { 72, 184 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, 104, 56, 1408, { .fields = { 72, 40 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, 88, 48, 1520, { .fields = { 48, 56 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, 72, 32, 1690, { .fields = { 48, 80 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 48 } }, 16, -24, 2109, { .fields = { 96, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 48 } }, 0, -24, 2185, { .fields = { 104, 48 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, 0, -56, 2470, { .fields = { 64, 72 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 32 } }, 16, -56, 2041, { .fields = { 56, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, 56, 16, 1921, { .fields = { 40, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 48 } }, 40, -8, 2127, { .fields = { 104, 96 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 48 } }, 40, -56, 2105, { .fields = { 112, 208 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 40 } }, 56, -24, 1939, { .fields = { 80, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, 88, 24, 1573, { .fields = { 56, 200 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, 104, 32, 1484, { .fields = { 56, 224 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, 120, 40, 1404, { .fields = { 40, 128 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 24 } }, 144, -16, 1225, { .fields = { 80, 72 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, 144, -32, 1495, { .fields = { 104, 240 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 56, 8 } }, 88, -32, 1835, { .fields = { 0, 24 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 8 } }, 104, -24, 1396, { .fields = { 48, 248 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 8 } }, 120, -16, 1314, { .fields = { 32, 192 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 8 } }, 128, -8, 1266, { .fields = { 40, 184 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, 56, -56, 2121, { .fields = { 72, 216 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 64 } }, 72, -32, 1853, { .fields = { 112, 144 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, 72, -56, 2146, { .fields = { 56, 176 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, 80, -80, 1792, { .fields = { 56, 128 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, 96, -80, 1969, { .fields = { 56, 152 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, 88, -56, 1972, { .fields = { 88, 232 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 24 } }, 104, -56, 1943, { .fields = { 48, 32 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 48 } }, 88, -24, 2046, { .fields = { 88, 48 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 48 } }, 104, -16, 1734, { .fields = { 96, 144 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 40 } }, 120, 0, 1555, { .fields = { 88, 192 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite drawing batches for cavern mapped view 7.
///
/// The zero-count first batch selects decoded background strips. Subsequent
/// ranges cover all 39 source sprites and end with `SPRITE_BATCH_END`.
/// The loaded cavern owns this writable list.
static SpriteBatch _gMineCavernView7SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gMineCavernView7SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for cavern mapped view 8.
///
/// The batches cover all 27 elements. Positions and dimensions are pixels
/// relative to the view centre; depths are unscaled OT sorting values.
/// The loaded cavern owns these writable source records.
static SpriteSource _gMineCavernView8SpriteSources[27] = {
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 24 } }, 32, 16, 1874, { .fields = { 96, 56 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 72 } }, 40, -32, 1803, { .fields = { 112, 80 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 16 } }, 48, -48, 1732, { .fields = { 56, 240 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 8 } }, 64, -56, 1739, { .fields = { 32, 104 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 8 } }, 72, -48, 1761, { .fields = { 40, 120 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, 104, -56, 1760, { .fields = { 64, 224 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 8 } }, 120, -56, 1645, { .fields = { 56, 112 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 16 } }, 136, -56, 1495, { .fields = { 48, 72 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, 56, -32, 1809, { .fields = { 64, 144 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 40 } }, 56, 0, 1896, { .fields = { 112, 216 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 48 } }, 64, 40, 1407, { .fields = { 96, 208 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 56 } }, 120, 32, 1084, { .fields = { 80, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 32 } }, 120, 88, 1119, { .fields = { 56, 176 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, 144, 88, 1146, { .fields = { 72, 112 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 64 } }, 120, -32, 1091, { .fields = { 88, 80 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 64 } }, 144, -32, 1018, { .fields = { 96, 144 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 56 } }, 144, 32, 1109, { .fields = { 80, 144 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 8 } }, 72, -40, 1645, { .fields = { 40, 136 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 8 } }, 104, -40, 1535, { .fields = { 48, 128 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, 72, -32, 1579, { .fields = { 56, 88 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 56 } }, 72, -16, 1478, { .fields = { 64, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 64 } }, 80, 40, 1259, { .fields = { 112, 152 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, 88, -32, 1480, { .fields = { 64, 208 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, 104, -32, 1436, { .fields = { 56, 56 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 56 } }, 88, -16, 1445, { .fields = { 72, 56 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 56 } }, 104, -16, 1148, { .fields = { 80, 200 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 80 } }, 96, 40, 1116, { .fields = { 104, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite drawing batches for cavern mapped view 8.
///
/// The zero-count first batch selects decoded background strips. Subsequent
/// ranges cover all 27 source sprites and end with `SPRITE_BATCH_END`.
/// The loaded cavern owns this writable list.
static SpriteBatch _gMineCavernView8SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gMineCavernView8SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for cavern mapped view 9.
///
/// The batches cover all 11 elements. Positions and dimensions are pixels
/// relative to the view centre; depths are unscaled OT sorting values.
/// The loaded cavern owns these writable source records.
static SpriteSource _gMineCavernView9SpriteSources[11] = {
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 120 } }, -160, -120, 276, { .fields = { 88, 120 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 120 } }, -160, 0, 275, { .fields = { 88, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 56, 48 } }, -120, 72, 296, { .fields = { 104, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 64, 32 } }, -64, 80, 340, { .fields = { 80, 144 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 48 } }, 0, 72, 375, { .fields = { 120, 48 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 48 } }, 40, 64, 434, { .fields = { 112, 96 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 64 } }, 72, 56, 487, { .fields = { 56, 192 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 72, 48 } }, 88, -120, 638, { .fields = { 112, 192 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 48, 96 } }, 104, -72, 630, { .fields = { 40, 96 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 56, 96 } }, 104, 24, 623, { .fields = { 32, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 88 } }, 0, -120, 558, { .fields = { 16, 96 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite drawing batches for cavern mapped view 9.
///
/// The zero-count first batch selects decoded background strips. Subsequent
/// ranges cover all 11 source sprites and end with `SPRITE_BATCH_END`.
/// The loaded cavern owns this writable list.
static SpriteBatch _gMineCavernView9SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gMineCavernView9SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Texture rectangles and sorting depths for cavern mapped view 10.
///
/// The batches cover all 3 elements. Positions and dimensions are pixels
/// relative to the view centre; depths are unscaled OT sorting values.
/// The loaded cavern owns these writable source records.
static SpriteSource _gMineCavernView10SpriteSources[3] = {
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 48, 40 } }, -160, 80, 375, { .fields = { 80, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 16 } }, -112, 104, 375, { .fields = { 88, 40 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 48, 8 } }, -72, 112, 375, { .fields = { 80, 56 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite drawing batches for cavern mapped view 10.
///
/// The zero-count first batch selects decoded background strips. Subsequent
/// ranges cover all 3 source sprites and end with `SPRITE_BATCH_END`.
/// The loaded cavern owns this writable list.
static SpriteBatch _gMineCavernView10SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gMineCavernView10SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Sprite drawing batches for cavern mapped view 11.
///
/// The zero-count first batch selects decoded background strips; the next
/// record ends the list. The source slot retains this list's address without
/// supplying any source elements. Storage belongs to the loaded cavern.
static SpriteBatch _gMineCavernView11SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite-batch list for cavern mapped view 12.
///
/// The zero-count first record selects decoded background strips; the next
/// record ends the list. The view's source slot retains this address without
/// supplying source elements. Storage belongs to the loaded cavern.
static SpriteBatch _gMineCavernView12SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite-batch list for cavern mapped view 13.
///
/// The zero-count first record selects decoded background strips; the next
/// record ends the list. The view's source slot retains this address without
/// supplying source elements. Storage belongs to the loaded cavern.
static SpriteBatch _gMineCavernView13SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite-batch list for cavern mapped view 14.
///
/// The zero-count first record selects decoded background strips; the next
/// record ends the list. The view's source slot retains this address without
/// supplying source elements. Storage belongs to the loaded cavern.
static SpriteBatch _gMineCavernView14SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite-batch list for cavern mapped view 15.
///
/// The zero-count first record selects decoded background strips; the next
/// record ends the list. The view's source slot retains this address without
/// supplying source elements. Storage belongs to the loaded cavern.
static SpriteBatch _gMineCavernView15SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite-batch list for cavern mapped view 16.
///
/// The zero-count first record selects decoded background strips; the next
/// record ends the list. The view's source slot retains this address without
/// supplying source elements. Storage belongs to the loaded cavern.
static SpriteBatch _gMineCavernView16SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite-batch list for cavern mapped view 17.
///
/// The zero-count first record selects decoded background strips; the next
/// record ends the list. The view's source slot retains this address without
/// supplying source elements. Storage belongs to the loaded cavern.
static SpriteBatch _gMineCavernView17SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite-batch list for cavern mapped view 18.
///
/// The zero-count first record selects decoded background strips; the next
/// record ends the list. The view's source slot retains this address without
/// supplying source elements. Storage belongs to the loaded cavern.
static SpriteBatch _gMineCavernView18SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite-batch list for cavern mapped view 19.
///
/// The zero-count first record selects decoded background strips; the next
/// record ends the list. The view's source slot retains this address without
/// supplying source elements. Storage belongs to the loaded cavern.
static SpriteBatch _gMineCavernView19SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Source rectangles for cavern mapped view 20.
///
/// The 48 elements have no terminator; `_gMineCavernView20SpriteBatches`
/// selects ranges within them. Positions and sizes use drawing-origin pixels;
/// texture origins use texels and depth is an unscaled sorting value. Pages
/// use eight-bit textures and neutral RGB. Storage requires the cavern loaded.
static SpriteSource _gMineCavernView20SpriteSources[48] = {
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 64, 8 } }, -160, -120, 250, { .fields = { 120, 184 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 64, 8 } }, -160, -104, 250, { .fields = { 24, 248 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 56, 16 } }, -72, -120, 250, { .fields = { 8, 208 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 24 } }, -96, -120, 250, { .fields = { 64, 200 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 88, 8 } }, -16, -120, 250, { .fields = { 96, 192 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 88, 8 } }, 72, -120, 250, { .fields = { 96, 200 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 8 } }, 48, 8, 1549, { .fields = { 80, 24 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, 48, -72, 1458, { .fields = { 88, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 32 } }, 64, -72, 1446, { .fields = { 72, 32 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 56 } }, 56, -40, 1541, { .fields = { 104, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, 48, 16, 1549, { .fields = { 56, 112 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, 64, 16, 1532, { .fields = { 40, 120 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 16 } }, 64, 32, 1460, { .fields = { 32, 88 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 16 } }, 56, -120, 1339, { .fields = { 16, 104 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 32 } }, 64, -104, 1383, { .fields = { 96, 56 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 48, 16 } }, -160, -64, 1157, { .fields = { 8, 152 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 24 } }, -48, -24, 1399, { .fields = { 48, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 24 } }, -48, 0, 1529, { .fields = { 56, 160 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 32 } }, -48, 24, 1458, { .fields = { 112, 144 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 24 } }, -56, -24, 1402, { .fields = { 56, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 24 } }, -64, -24, 1375, { .fields = { 56, 136 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 24 } }, -56, 0, 1470, { .fields = { 64, 176 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 48 } }, -56, 24, 1296, { .fields = { 120, 56 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 24 } }, -64, 0, 1379, { .fields = { 56, 184 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 32 } }, -64, 24, 1280, { .fields = { 120, 224 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 32 } }, -120, 56, 1105, { .fields = { 88, 176 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 32 } }, -160, 56, 1096, { .fields = { 72, 120 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 40 } }, -88, 48, 1131, { .fields = { 112, 104 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, -80, 32, 1361, { .fields = { 40, 168 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 24 } }, -72, 48, 1275, { .fields = { 96, 32 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -88, 32, 1113, { .fields = { 112, 88 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 24 } }, -112, 32, 1127, { .fields = { 64, 152 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 24 } }, -136, 32, 1105, { .fields = { 64, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 24 } }, -160, 32, 1101, { .fields = { 88, 152 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 24 } }, -120, -48, 1189, { .fields = { 72, 64 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 24 } }, -160, -48, 1266, { .fields = { 40, 224 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 24 } }, -128, 8, 1076, { .fields = { 48, 24 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 32 } }, -128, -24, 1170, { .fields = { 80, 88 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 24 } }, -160, 8, 1077, { .fields = { 40, 48 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 24 } }, -104, 8, 1089, { .fields = { 56, 88 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 16 } }, -96, -24, 1380, { .fields = { 88, 240 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 16 } }, -96, -8, 1396, { .fields = { 40, 72 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, -80, 8, 1374, { .fields = { 72, 176 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 32 } }, -160, -24, 1225, { .fields = { 88, 208 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, -112, -64, 1289, { .fields = { 40, 136 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 40 } }, -96, -64, 1369, { .fields = { 120, 144 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 40 } }, -88, -64, 1470, { .fields = { 120, 184 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 24 } }, -80, -48, 1579, { .fields = { 80, 224 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite drawing batches for cavern mapped view 20.
///
/// The zero-count first record selects decoded background strips. The ranges
/// 0..5, 6..14 and 15..47 cover `_gMineCavernView20SpriteSources` exactly;
/// the final record ends the list. Hidden and packet-skip flags start clear;
/// the serialized trailer bytes have unproven roles. Storage requires the
/// cavern loaded.
static SpriteBatch _gMineCavernView20SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 1, 0 } },
    { 6, 9, 0, 0, { 2, 0 } },
    { 15, 33, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Source rectangles for cavern mapped view 21.
///
/// The 52 elements have no terminator; `_gMineCavernView21SpriteBatches`
/// selects ranges within them. Positions and sizes use drawing-origin pixels;
/// texture origins use texels and depth is an unscaled sorting value. Pages
/// use eight-bit textures and neutral RGB. Storage requires the cavern loaded.
static SpriteSource _gMineCavernView21SpriteSources[52] = {
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 56 } }, -160, -8, 1006, { .fields = { 112, 56 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 40 } }, -144, 80, 1067, { .fields = { 72, 88 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 40 } }, -160, 80, 1062, { .fields = { 96, 168 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, -160, 48, 1030, { .fields = { 80, 160 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, -144, 56, 1099, { .fields = { 56, 136 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 24 } }, -120, -120, 1412, { .fields = { 48, 112 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, -96, -120, 1401, { .fields = { 56, 88 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 8 } }, -104, 16, 1621, { .fields = { 0, 72 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 8 } }, -104, 24, 1582, { .fields = { 0, 80 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 8 } }, -104, 32, 1525, { .fields = { 8, 144 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 24 } }, -112, -96, 1459, { .fields = { 56, 160 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 16 } }, -112, -56, 1575, { .fields = { 32, 168 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, -104, -40, 1559, { .fields = { 64, 64 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, -88, -16, 1674, { .fields = { 72, 128 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, -104, -16, 1669, { .fields = { 64, 32 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, -88, -64, 1505, { .fields = { 48, 240 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 32 } }, -88, -48, 1574, { .fields = { 88, 128 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 16 } }, -112, -72, 1472, { .fields = { 32, 80 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 8 } }, -88, -72, 1472, { .fields = { 16, 64 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 72, 8 } }, 56, 96, 1071, { .fields = { 88, 120 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 16 } }, 128, 96, 1052, { .fields = { 0, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 56, 24 } }, 48, 56, 1115, { .fields = { 24, 184 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 56, 48 } }, 48, 8, 1089, { .fields = { 64, 208 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 16 } }, 56, -8, 1282, { .fields = { 8, 32 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 8 } }, 48, -16, 1435, { .fields = { 0, 88 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 8 } }, 48, -24, 1480, { .fields = { 0, 112 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, 48, -8, 1537, { .fields = { 80, 32 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 32 } }, 80, -24, 1250, { .fields = { 64, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 40 } }, 112, -104, 1394, { .fields = { 96, 88 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 40 } }, 128, -104, 1180, { .fields = { 96, 48 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 40 } }, 144, -104, 989, { .fields = { 96, 128 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 48 } }, 104, -64, 1492, { .fields = { 112, 160 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 48 } }, 120, -64, 1353, { .fields = { 112, 112 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 48 } }, 136, -64, 1031, { .fields = { 88, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 16 } }, 104, 48, 1278, { .fields = { 32, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 16 } }, 104, 64, 1179, { .fields = { 32, 64 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 56 } }, 144, 40, 1206, { .fields = { 112, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 48 } }, 136, 48, 1049, { .fields = { 120, 208 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 16 } }, 104, 32, 1217, { .fields = { 24, 208 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 16 } }, 104, 16, 1299, { .fields = { 24, 224 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 16 } }, 104, 0, 1341, { .fields = { 24, 48 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 16 } }, 104, -16, 1443, { .fields = { 24, 16 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, 32, 0, 1749, { .fields = { 80, 192 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, 32, 16, 1557, { .fields = { 48, 32 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, 32, 32, 1408, { .fields = { 32, 240 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, 32, 48, 1285, { .fields = { 32, 112 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, 32, 64, 1181, { .fields = { 24, 128 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, 32, 80, 1114, { .fields = { 32, 32 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 48, 16 } }, 48, 80, 1089, { .fields = { 8, 96 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 16 } }, 96, 80, 1085, { .fields = { 16, 152 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 40 } }, 144, 0, 954, { .fields = { 80, 48 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, 144, -16, 1074, { .fields = { 40, 136 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite drawing batches for cavern mapped view 21.
///
/// The zero-count first record selects decoded background strips. The ranges
/// 0..18 and 19..51 cover `_gMineCavernView21SpriteSources` exactly;
/// the final record ends the list. Hidden and packet-skip flags start clear;
/// the serialized trailer bytes have unproven roles. Storage requires the
/// cavern loaded.
static SpriteBatch _gMineCavernView21SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 1, 0 } },
    { 19, 33, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Source rectangles for cavern mapped view 22.
///
/// The 59 elements have no terminator; `_gMineCavernView22SpriteBatches`
/// selects ranges within them. Positions and sizes use drawing-origin pixels;
/// texture origins use texels and depth is an unscaled sorting value. Pages
/// use eight-bit textures and neutral RGB. Storage requires the cavern loaded.
/// Elements 58 use the progress palette and nursery-controlled batch.
static SpriteSource _gMineCavernView22SpriteSources[59] = {
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 40 } }, -160, -120, 421, { .fields = { 96, 216 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 48 } }, -160, -80, 430, { .fields = { 8, 144 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 24 } }, -136, -72, 437, { .fields = { 80, 208 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 48, 16 } }, -120, -88, 440, { .fields = { 56, 80 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 48, 16 } }, -96, -104, 441, { .fields = { 56, 64 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 48, 16 } }, -64, -120, 449, { .fields = { 56, 112 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 64 } }, -160, -32, 468, { .fields = { 64, 192 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 48 } }, -160, 32, 459, { .fields = { 0, 48 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 40 } }, -160, 80, 485, { .fields = { 120, 192 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 8 } }, -144, -120, 937, { .fields = { 72, 248 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 48, 16 } }, -144, -112, 966, { .fields = { 56, 96 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 56, 16 } }, -96, -112, 982, { .fields = { 48, 48 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 16 } }, -88, -96, 998, { .fields = { 64, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 16 } }, -144, -96, 966, { .fields = { 64, 16 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 56, 16 } }, -136, -80, 982, { .fields = { 48, 32 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 24 } }, -136, -64, 986, { .fields = { 120, 232 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 16 } }, -136, -40, 1039, { .fields = { 64, 128 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 16 } }, -128, -24, 1028, { .fields = { 72, 232 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 16 } }, -128, -8, 1045, { .fields = { 48, 160 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 16 } }, -128, 8, 1080, { .fields = { 48, 216 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 16 } }, -120, 24, 1108, { .fields = { 48, 200 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 16 } }, -120, 40, 1116, { .fields = { 72, 144 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 16 } }, -120, 72, 1150, { .fields = { 32, 240 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 16 } }, -120, 56, 1149, { .fields = { 64, 184 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 40, 24 } }, -112, -112, 1187, { .fields = { 80, 160 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 56 } }, -96, -88, 1187, { .fields = { 64, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 8 } }, 80, -64, 1336, { .fields = { 8, 136 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 48 } }, 144, -112, 910, { .fields = { 48, 96 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 24 } }, 16, 8, 1795, { .fields = { 64, 168 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, 24, -16, 1748, { .fields = { 104, 232 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, 24, 8, 1699, { .fields = { 120, 176 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, 24, 24, 1601, { .fields = { 104, 184 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 24 } }, 40, -32, 1486, { .fields = { 72, 168 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 80 } }, 48, -88, 1330, { .fields = { 96, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 72 } }, 40, -8, 1451, { .fields = { 96, 80 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 88 } }, 56, -8, 1217, { .fields = { 112, 112 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 112 } }, 72, -8, 1097, { .fields = { 112, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 40 } }, 88, -8, 950, { .fields = { 104, 88 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 40 } }, 112, -8, 1194, { .fields = { 104, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 48 } }, 88, 32, 1173, { .fields = { 40, 192 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 48 } }, 112, 32, 872, { .fields = { 32, 48 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 40 } }, 88, 80, 1048, { .fields = { 104, 48 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 40 } }, 112, 80, 957, { .fields = { 0, 96 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 56 } }, 128, 64, 825, { .fields = { 64, 56 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 56 } }, 144, 64, 933, { .fields = { 112, 200 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 64 } }, 128, 0, 1027, { .fields = { 80, 64 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 64 } }, 144, 0, 1140, { .fields = { 80, 128 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 64 } }, 144, -64, 951, { .fields = { 96, 152 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 64 } }, 128, -64, 1098, { .fields = { 80, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 40 } }, 128, -104, 995, { .fields = { 120, 136 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 48 } }, 120, -56, 1183, { .fields = { 32, 144 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 48 } }, 112, -56, 888, { .fields = { 32, 192 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 56 } }, 64, -64, 1187, { .fields = { 64, 112 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 48 } }, 80, -56, 1032, { .fields = { 32, 96 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 48 } }, 96, -56, 930, { .fields = { 48, 48 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, 72, -96, 1342, { .fields = { 104, 128 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 48 } }, 88, -104, 1241, { .fields = { 16, 96 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 48 } }, 104, -104, 853, { .fields = { 40, 144 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_PROGRESS_SPRITE_CLUT, { .fields = { 72, 48 } }, -96, -40, 2750, { .fields = { 120, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite drawing batches for cavern mapped view 22.
///
/// The zero-count first record selects decoded background strips. The ranges
/// 0..8, 9..23, 24..25, 26..57 and 58 cover `_gMineCavernView22SpriteSources` exactly;
/// the final record ends the list. Hidden and packet-skip flags start clear;
/// the serialized trailer bytes have unproven roles. Storage requires the
/// cavern loaded.
/// Nursery progress controls visibility of the range 58.
static SpriteBatch _gMineCavernView22SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 0, 0 } },
    { 9, 15, 0, 0, { 3, 0 } },
    { 24, 2, 0, 0, { 2, 0 } },
    { 26, 32, 0, 0, { 4, 0 } },
    { 58, 1, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Source rectangles for cavern mapped view 23.
///
/// The 47 elements have no terminator; `_gMineCavernView23SpriteBatches`
/// selects ranges within them. Positions and sizes use drawing-origin pixels;
/// texture origins use texels and depth is an unscaled sorting value. Pages
/// use eight-bit textures and neutral RGB. Storage requires the cavern loaded.
/// Elements 45..46 use the progress palette and nursery-controlled batch.
static SpriteSource _gMineCavernView23SpriteSources[47] = {
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 16 } }, 72, -120, 1099, { .fields = { 24, 168 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 40 } }, 88, -104, 1100, { .fields = { 48, 48 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 72 } }, 112, -120, 1125, { .fields = { 72, 72 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 72 } }, 104, -48, 1173, { .fields = { 72, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 48 } }, 96, 24, 1242, { .fields = { 40, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, 88, 72, 1271, { .fields = { 80, 224 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 8 } }, 104, 72, 1274, { .fields = { 64, 248 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 16 } }, 104, 80, 1250, { .fields = { 16, 32 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 8 } }, -72, 64, 1195, { .fields = { 56, 248 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 8 } }, -56, 80, 1266, { .fields = { 88, 248 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 24 } }, -160, 96, 669, { .fields = { 56, 224 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 16 } }, -160, 80, 755, { .fields = { 24, 184 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 144 } }, -152, -64, 1125, { .fields = { 104, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 144 } }, -160, -64, 1145, { .fields = { 120, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, -136, 16, 1125, { .fields = { 32, 144 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 16 } }, -120, 24, 1125, { .fields = { 16, 216 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 48, 16 } }, -120, 40, 1049, { .fields = { 0, 200 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, -136, 40, 1060, { .fields = { 32, 96 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, -136, 56, 902, { .fields = { 32, 80 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 16 } }, -120, 56, 943, { .fields = { 24, 128 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -96, 56, 1004, { .fields = { 40, 112 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -88, 56, 1062, { .fields = { 32, 112 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -80, 56, 1173, { .fields = { 32, 48 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 16 } }, -136, 72, 855, { .fields = { 16, 64 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, -136, 88, 759, { .fields = { 112, 224 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 32 } }, -120, 88, 829, { .fields = { 96, 224 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, -112, 104, 1075, { .fields = { 32, 232 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, -96, 104, 1075, { .fields = { 24, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -80, 104, 1015, { .fields = { 32, 16 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -72, 104, 1056, { .fields = { 40, 64 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -64, 104, 1106, { .fields = { 48, 88 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 32 } }, -112, 72, 872, { .fields = { 104, 224 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -104, 72, 930, { .fields = { 48, 104 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -104, 88, 991, { .fields = { 48, 120 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -96, 88, 991, { .fields = { 56, 128 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -88, 88, 1012, { .fields = { 64, 112 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -80, 88, 1089, { .fields = { 64, 128 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -72, 88, 1118, { .fields = { 56, 112 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -64, 88, 1162, { .fields = { 48, 216 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -56, 88, 1225, { .fields = { 48, 200 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -96, 72, 1083, { .fields = { 40, 48 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -88, 72, 1080, { .fields = { 48, 232 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -80, 72, 1105, { .fields = { 48, 152 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -72, 72, 1191, { .fields = { 48, 136 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -64, 72, 1244, { .fields = { 48, 184 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_PROGRESS_SPRITE_CLUT, { .fields = { 72, 80 } }, 32, -8, 2250, { .fields = { 56, 144 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_PROGRESS_SPRITE_CLUT, { .fields = { 16, 24 } }, 120, 40, 1750, { .fields = { 56, 88 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite drawing batches for cavern mapped view 23.
///
/// The zero-count first record selects decoded background strips. The ranges
/// 0..7, 8..44 and 45..46 cover `_gMineCavernView23SpriteSources` exactly;
/// the final record ends the list. Hidden and packet-skip flags start clear;
/// the serialized trailer bytes have unproven roles. Storage requires the
/// cavern loaded.
/// Nursery progress controls visibility of the range 45..46.
static SpriteBatch _gMineCavernView23SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 1, 0 } },
    { 8, 37, 0, 0, { 2, 0 } },
    { 45, 2, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Source rectangles for cavern mapped view 24.
///
/// The 48 elements have no terminator; `_gMineCavernView24SpriteBatches`
/// selects ranges within them. Positions and sizes use drawing-origin pixels;
/// texture origins use texels and depth is an unscaled sorting value. Pages
/// use eight-bit textures and neutral RGB. Storage requires the cavern loaded.
/// Elements 46..47 use the progress palette and nursery-controlled batch.
static SpriteSource _gMineCavernView24SpriteSources[48] = {
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 72, 80 } }, -8, -96, 2250, { .fields = { 32, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 8 } }, -72, 64, 1195, { .fields = { 80, 248 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 8 } }, -56, 80, 1266, { .fields = { 88, 248 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 24 } }, -160, 96, 669, { .fields = { 40, 224 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 16 } }, -160, 80, 755, { .fields = { 16, 240 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 144 } }, -160, -64, 1145, { .fields = { 104, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 144 } }, -152, -64, 1125, { .fields = { 112, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, -136, 16, 1125, { .fields = { 0, 136 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 16 } }, -120, 24, 1125, { .fields = { 8, 224 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, -136, 40, 1060, { .fields = { 56, 96 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, -136, 56, 902, { .fields = { 64, 128 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 16 } }, -136, 72, 855, { .fields = { 80, 128 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 32 } }, -136, 88, 759, { .fields = { 112, 224 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 32 } }, -120, 88, 829, { .fields = { 104, 224 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 48, 16 } }, -120, 40, 1049, { .fields = { 112, 40 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 16 } }, -120, 56, 943, { .fields = { 120, 152 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -96, 56, 1004, { .fields = { 16, 120 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -88, 56, 1062, { .fields = { 56, 80 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -80, 56, 1173, { .fields = { 56, 112 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 32 } }, -112, 72, 872, { .fields = { 96, 224 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, -112, 104, 1075, { .fields = { 16, 56 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 16 } }, -96, 104, 1075, { .fields = { 8, 72 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -80, 104, 1015, { .fields = { 64, 112 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -72, 104, 1056, { .fields = { 64, 80 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -64, 104, 1106, { .fields = { 56, 128 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -104, 72, 930, { .fields = { 16, 104 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -104, 88, 991, { .fields = { 16, 88 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -96, 72, 1083, { .fields = { 8, 120 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -88, 72, 1080, { .fields = { 16, 184 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -80, 72, 1105, { .fields = { 16, 136 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -72, 72, 1191, { .fields = { 16, 152 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -64, 72, 1244, { .fields = { 8, 88 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -96, 88, 991, { .fields = { 16, 200 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -88, 88, 1012, { .fields = { 8, 56 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -80, 88, 1089, { .fields = { 8, 200 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -72, 88, 1118, { .fields = { 8, 240 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -64, 88, 1162, { .fields = { 8, 168 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 8, 16 } }, -56, 88, 1225, { .fields = { 16, 168 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 16 } }, 72, -120, 1099, { .fields = { 112, 104 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 40 } }, 88, -104, 1100, { .fields = { 8, 0 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 72 } }, 112, -120, 1125, { .fields = { 24, 80 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 72 } }, 104, -48, 1173, { .fields = { 24, 152 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 32, 48 } }, 96, 24, 1242, { .fields = { 72, 80 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 16, 24 } }, 88, 72, 1271, { .fields = { 80, 224 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 8 } }, 104, 72, 1274, { .fields = { 56, 248 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X896, MINE_CAVERN_SPRITE_CLUT, { .fields = { 24, 16 } }, 104, 80, 1250, { .fields = { 120, 184 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_PROGRESS_SPRITE_CLUT, { .fields = { 72, 80 } }, 32, -8, 2250, { .fields = { 56, 144 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
    { MINE_CAVERN_SPRITE_TEXTURE_PAGE_X960, MINE_CAVERN_PROGRESS_SPRITE_CLUT, { .fields = { 16, 24 } }, 120, 40, 1750, { .fields = { 64, 224 } }, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, MINE_CAVERN_SPRITE_NEUTRAL_COLOR, 0 },
};

/// Sprite drawing batches for cavern mapped view 24.
///
/// The zero-count first record selects decoded background strips. The ranges
/// 0, 1..37, 38..45 and 46..47 cover `_gMineCavernView24SpriteSources` exactly;
/// the final record ends the list. Hidden and packet-skip flags start clear;
/// the serialized trailer bytes have unproven roles. Storage requires the
/// cavern loaded.
/// Nursery progress controls visibility of the range 46..47.
static SpriteBatch _gMineCavernView24SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 3, 0 } },
    { 1, 37, 0, 0, { 0, 0 } },
    { 38, 8, 0, 0, { 2, 0 } },
    { 46, 2, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Unused empty sprite-batch list retained in the cavern's room data.
///
/// A zero-count background-strip batch followed by `SPRITE_BATCH_END`.
/// No mapped view selects this list; visibility and packet-skip flags are clear.
static SpriteBatch _gMineCavernUnusedEmptySpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gMineCavernSpriteViews[25] = {
    { { .empty = _gMineCavernView1SpriteBatches }, _gMineCavernView1SpriteBatches, NULL },
    { { .elements = _gMineCavernView2SpriteSources }, _gMineCavernView2SpriteBatches, NULL },
    { { .elements = _gMineCavernView3SpriteSources }, _gMineCavernView3SpriteBatches, NULL },
    { { .elements = _gMineCavernView4SpriteSources }, _gMineCavernView4SpriteBatches, NULL },
    { { .elements = _gMineCavernView5SpriteSources }, _gMineCavernView5SpriteBatches, NULL },
    { { .elements = _gMineCavernView6SpriteSources }, _gMineCavernView6SpriteBatches, NULL },
    { { .elements = _gMineCavernView7SpriteSources }, _gMineCavernView7SpriteBatches, NULL },
    { { .elements = _gMineCavernView8SpriteSources }, _gMineCavernView8SpriteBatches, NULL },
    { { .elements = _gMineCavernView9SpriteSources }, _gMineCavernView9SpriteBatches, NULL },
    { { .elements = _gMineCavernView10SpriteSources }, _gMineCavernView10SpriteBatches, NULL },
    { { .empty = _gMineCavernView11SpriteBatches }, _gMineCavernView11SpriteBatches, NULL },
    { { .empty = _gMineCavernView12SpriteBatches }, _gMineCavernView12SpriteBatches, NULL },
    { { .empty = _gMineCavernView13SpriteBatches }, _gMineCavernView13SpriteBatches, NULL },
    { { .empty = _gMineCavernView14SpriteBatches }, _gMineCavernView14SpriteBatches, NULL },
    { { .empty = _gMineCavernView15SpriteBatches }, _gMineCavernView15SpriteBatches, NULL },
    { { .empty = _gMineCavernView16SpriteBatches }, _gMineCavernView16SpriteBatches, NULL },
    { { .empty = _gMineCavernView17SpriteBatches }, _gMineCavernView17SpriteBatches, NULL },
    { { .empty = _gMineCavernView18SpriteBatches }, _gMineCavernView18SpriteBatches, NULL },
    { { .empty = _gMineCavernView19SpriteBatches }, _gMineCavernView19SpriteBatches, NULL },
    { { .elements = _gMineCavernView20SpriteSources }, _gMineCavernView20SpriteBatches, NULL },
    { { .elements = _gMineCavernView21SpriteSources }, _gMineCavernView21SpriteBatches, NULL },
    { { .elements = _gMineCavernView22SpriteSources }, _gMineCavernView22SpriteBatches, NULL },
    { { .elements = _gMineCavernView23SpriteSources }, _gMineCavernView23SpriteBatches, NULL },
    { { .elements = _gMineCavernView24SpriteSources }, _gMineCavernView24SpriteBatches, NULL },
    { { .elements = _gMineCavernView5SpriteSources }, _gMineCavernView5SpriteBatches, NULL },
};

/// The eight point lights shared by all three cavern rooms and every view.
///
/// Positions and falloff radii use whole world-coordinate units; RGB uses
/// 12 fractional bits. Full-strength radii are zero and outer radii vary per
/// light. Room lighting reparents and composes their coordinates and uses
/// attenuation as query scratch, so this overlay-owned array stays writable.
static WorldCoordPointLight _gMineCavernPointLights[8] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8770, -2001, 8760 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3799, 3112 }, { 0, 0 } }, 0, 7000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 240, -2001, 4380 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3799, 3112 }, { 0, 0 } }, 0, 5000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x4560, -2001, 5000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3799, 3112 }, { 0, 0 } }, 0, 5000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x36E2, -2001, 270 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3799, 3112 }, { 0, 0 } }, 0, 5000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4530, -2001, 270 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3799, 3112 }, { 0, 0 } }, 0, 5000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9080, -2001, 270 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1324, 1594, 1648 }, { 0, 0 } }, 0, 5000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3581, -2001, 8980 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1324, 1495, 1416 }, { 0, 0 } }, 0, 5261 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x3C5B, -2001, 8800 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1324, 1676, 1723 }, { 0, 0 } }, 0, 4299 },
};

/// The cavern's shared collection of eight point lights.
///
/// All three room-lighting entries borrow this one record. No directional or
/// cone lights are present; the collection and its mutable point array stay
/// valid for the loaded cavern's lifetime.
static WorldCoordRoomLights _gMineCavernRoomLights = {
    0, NULL, ARRAY_SIZE(_gMineCavernPointLights), _gMineCavernPointLights, 0, NULL
};

/// Directed view boundaries for cavern room 1, in whole room-coordinate units.
///
/// Source/destination bytes are logical views 2..8 and 20..22. Setup mutates
/// coordinates, links and enable flags; the twentieth record is included and
/// carries LAST. Storage must stay loaded until the runtime list is unlinked.
static WorldCollisionTrigger _gMineCavernRoom1ViewBoundaryTriggers[20] = {
    { NULL, NULL, NULL, { 1520, -2688, 6880, 0 }, { { -2544, -3456, 1376, 0 }, { 2544, -3456, -1376, 0 }, { -2544, 3456, 1376, 0 }, { 2544, 3456, -1376, 0 } }, { -1959, 0, -3621, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1377, -2496, 6754, 0 }, { { 2656, -3456, -1408, 0 }, { -2656, -3456, 1408, 0 }, { 2656, 3456, -1408, 0 }, { -2656, 3456, 1408, 0 } }, { 1920, 0, 3621, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7890, -2528, 7376, 0 }, { { 144, -3456, 2947, 0 }, { -147, -3456, -2950, 0 }, { 144, 3456, 2947, 0 }, { -147, 3456, -2950, 0 } }, { -4105, 0, 202, 0 }, { 0, 0, 4096, 0 }, 4521, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7376, -2528, 7407, 0 }, { { -560, -3456, -2880, 0 }, { 560, -3456, 2880, 0 }, { -560, 3456, -2880, 0 }, { 560, 3456, 2880, 0 } }, { 4030, 0, -784, 0 }, { 0, 0, 4096, 0 }, 4521, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x41B0, -2464, 6368, 0 }, { { -2640, -3456, -393, 0 }, { 2605, -3456, 379, 0 }, { -2640, 3456, -393, 0 }, { 2605, 3456, 379, 0 } }, { 597, 0, -4061, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 2, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x4210, -2529, 6176, 0 }, { { 3083, -3456, 279, 0 }, { -3125, -3456, -286, 0 }, { 3083, 3456, 279, 0 }, { -3125, 3456, -286, 0 } }, { -373, 0, 4088, 0 }, { 0, 0, 4096, 0 }, 4636, 0, 8, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x41D1, -2528, 2944, 0 }, { { -2555, -3456, 655, 0 }, { 2554, -3456, -720, 0 }, { -2555, 3456, 655, 0 }, { 2554, 3456, -720, 0 } }, { -1065, 0, -3956, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 3, 20, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x40A1, -2817, 2720, 0 }, { { 2333, -3456, -816, 0 }, { -2346, -3456, 754, 0 }, { 2333, 3456, -816, 0 }, { -2346, 3456, 754, 0 } }, { 1304, 0, 3886, 0 }, { 0, 0, 4096, 0 }, 4222, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1568, -2656, 3184, 0 }, { { -2723, -3456, -593, 0 }, { 2711, -3456, 583, 0 }, { -2723, 3456, -593, 0 }, { 2711, 3456, 583, 0 } }, { 870, 0, -4026, 0 }, { 0, 0, 4096, 0 }, 4434, 0, 22, 21, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1520, -2656, 2896, 0 }, { { 2712, -3456, 458, 0 }, { -2732, -3456, -469, 0 }, { 2712, 3456, 458, 0 }, { -2732, 3456, -469, 0 } }, { -692, 0, 4057, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 5, 22, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2CC0, -2561, 1184, 0 }, { { -16, -3456, 2640, 0 }, { 16, -3456, -2640, 0 }, { -16, 3456, 2640, 0 }, { 16, 3456, -2640, 0 } }, { -4115, 0, -25, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2A81, -2497, 1072, 0 }, { { 0, -3456, -2672, 0 }, { 1, -3456, 2672, 0 }, { 0, 3456, -2672, 0 }, { 1, 3456, 2672, 0 } }, { 4110, 0, -1, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x40BE, -2656, 2653, 0 }, { { 2335, -3456, -804, 0 }, { -2352, -3456, 764, 0 }, { 2335, 3456, -804, 0 }, { -2352, 3456, 764, 0 } }, { 1302, 0, 3893, 0 }, { 0, 0, 4096, 0 }, 4222, 0, 20, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x4240, -2464, 6463, 0 }, { { -2470, -3456, -299, 0 }, { 2437, -3456, 288, 0 }, { -2470, 3456, -299, 0 }, { 2437, 3456, 288, 0 } }, { 487, 0, -4077, 0 }, { 0, 0, 4096, 0 }, 4222, 0, 20, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2176, -2432, 6880, 0 }, { { -2544, -3456, 1376, 0 }, { 2544, -3456, -1376, 0 }, { -2544, 3456, 1376, 0 }, { 2544, 3456, -1376, 0 } }, { -1959, 0, -3621, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 21, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1535, -2400, 2847, 0 }, { { 2824, -3456, 537, 0 }, { -2853, -3456, -561, 0 }, { 2824, 3456, 537, 0 }, { -2853, 3456, -561, 0 } }, { -782, 0, 4038, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 21, 22, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2FDF, -2496, 7423, 0 }, { { 438, -3456, 2858, 0 }, { -439, -3456, -2859, 0 }, { 438, 3456, 2858, 0 }, { -439, 3456, -2859, 0 } }, { -4068, 0, 623, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2DFE, -2560, 7486, 0 }, { { -441, -3456, -2860, 0 }, { 436, -3456, 2855, 0 }, { -441, 3456, -2860, 0 }, { 436, 3456, 2855, 0 } }, { 4066, 0, -625, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6079, -2464, 1343, 0 }, { { -439, -3456, -2859, 0 }, { 438, -3456, 2858, 0 }, { -439, 3456, -2859, 0 }, { 438, 3456, 2858, 0 } }, { 4067, 0, -625, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 4, 22, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6559, -2560, 1343, 0 }, { { 438, -3456, 2858, 0 }, { -439, -3456, -2859, 0 }, { 438, 3456, 2858, 0 }, { -439, 3456, -2859, 0 } }, { -4068, 0, 623, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 22, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Directed view boundaries shared by cavern rooms 2 and 3.
///
/// Source/destination bytes are logical views 2..8 and 22..23; geometry uses
/// whole room-coordinate units. Setup mutates coordinates, links and enable
/// flags. The eighteenth record carries LAST and is included before stopping;
/// keep storage loaded until the runtime list is unlinked.
static WorldCollisionTrigger _gMineCavernRooms2And3ViewBoundaryTriggers[18] = {
    { NULL, NULL, NULL, { 1520, -2688, 6880, 0 }, { { -2544, -3456, 1376, 0 }, { 2544, -3456, -1376, 0 }, { -2544, 3456, 1376, 0 }, { 2544, 3456, -1376, 0 } }, { -1959, 0, -3621, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1377, -2496, 6754, 0 }, { { 2656, -3456, -1408, 0 }, { -2656, -3456, 1408, 0 }, { 2656, 3456, -1408, 0 }, { -2656, 3456, 1408, 0 } }, { 1920, 0, 3621, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7890, -2528, 7376, 0 }, { { 144, -3456, 2947, 0 }, { -147, -3456, -2950, 0 }, { 144, 3456, 2947, 0 }, { -147, 3456, -2950, 0 } }, { -4105, 0, 202, 0 }, { 0, 0, 4096, 0 }, 4521, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7376, -2528, 7407, 0 }, { { -560, -3456, -2880, 0 }, { 560, -3456, 2880, 0 }, { -560, 3456, -2880, 0 }, { 560, 3456, 2880, 0 } }, { 4030, 0, -784, 0 }, { 0, 0, 4096, 0 }, 4521, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x4210, -2529, 6176, 0 }, { { 3083, -3456, 279, 0 }, { -3125, -3456, -286, 0 }, { 3083, 3456, 279, 0 }, { -3125, 3456, -286, 0 } }, { -373, 0, 4088, 0 }, { 0, 0, 4096, 0 }, 4636, 0, 8, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x41D1, -2528, 2944, 0 }, { { -2555, -3456, 655, 0 }, { 2554, -3456, -720, 0 }, { -2555, 3456, 655, 0 }, { 2554, 3456, -720, 0 } }, { -1065, 0, -3956, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x40A1, -2817, 2720, 0 }, { { 2333, -3456, -816, 0 }, { -2346, -3456, 754, 0 }, { 2333, 3456, -816, 0 }, { -2346, 3456, 754, 0 } }, { 1304, 0, 3886, 0 }, { 0, 0, 4096, 0 }, 4222, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1568, -2656, 3184, 0 }, { { -2723, -3456, -593, 0 }, { 2711, -3456, 583, 0 }, { -2723, 3456, -593, 0 }, { 2711, 3456, 583, 0 } }, { 870, 0, -4026, 0 }, { 0, 0, 4096, 0 }, 4434, 0, 23, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1520, -2656, 2896, 0 }, { { 2712, -3456, 458, 0 }, { -2732, -3456, -469, 0 }, { 2712, 3456, 458, 0 }, { -2732, 3456, -469, 0 } }, { -692, 0, 4057, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 5, 23, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2CC0, -2561, 1184, 0 }, { { -16, -3456, 2640, 0 }, { 16, -3456, -2640, 0 }, { -16, 3456, 2640, 0 }, { 16, 3456, -2640, 0 } }, { -4115, 0, -25, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 4, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2A81, -2497, 1072, 0 }, { { 0, -3456, -2672, 0 }, { 1, -3456, 2672, 0 }, { 0, 3456, -2672, 0 }, { 1, 3456, 2672, 0 } }, { 4110, 0, -1, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 2, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x4240, -2464, 6463, 0 }, { { -2470, -3456, -299, 0 }, { 2437, -3456, 288, 0 }, { -2470, 3456, -299, 0 }, { 2437, 3456, 288, 0 } }, { 487, 0, -4077, 0 }, { 0, 0, 4096, 0 }, 4222, 0, 3, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2FDF, -2496, 7423, 0 }, { { 438, -3456, 2858, 0 }, { -439, -3456, -2859, 0 }, { 438, 3456, 2858, 0 }, { -439, 3456, -2859, 0 } }, { -4068, 0, 623, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2DFE, -2560, 7486, 0 }, { { -441, -3456, -2860, 0 }, { 436, -3456, 2855, 0 }, { -441, 3456, -2860, 0 }, { 436, 3456, 2855, 0 } }, { 4066, 0, -625, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6079, -2464, 1343, 0 }, { { -439, -3456, -2859, 0 }, { 438, -3456, 2858, 0 }, { -439, 3456, -2859, 0 }, { 438, 3456, 2858, 0 } }, { 4067, 0, -625, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 4, 22, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6559, -2560, 1343, 0 }, { { 438, -3456, 2858, 0 }, { -439, -3456, -2859, 0 }, { 438, 3456, 2858, 0 }, { -439, 3456, -2859, 0 } }, { -4068, 0, 623, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 22, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2815, -2368, 1055, 0 }, { { -438, -3456, -2858, 0 }, { 439, -3456, 2859, 0 }, { -438, 3456, -2858, 0 }, { 439, 3456, 2859, 0 } }, { 4067, 0, -625, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 22, 23, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3263, -2464, 1151, 0 }, { { 298, -3456, 2877, 0 }, { -298, -3456, -2877, 0 }, { 298, 3456, 2877, 0 }, { -298, 3456, -2877, 0 } }, { -4095, 0, 423, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 23, 22, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

// Warp requests pack this area's departure endpoint above the destination slot.
enum {
    MINE_CAVERN_TRIGGER_GORGE_WARP_REQUEST      = (1 << 4) | 3,
    MINE_CAVERN_TRIGGER_PASSAGE_WARP_REQUEST    = (2 << 4) | 1,
    MINE_CAVERN_TRIGGER_PASSAGE_STATUS_COMMAND  = 1,
    MINE_CAVERN_TRIGGER_PROGRESS_CAPTION_ACTION = 6,
    MINE_CAVERN_TRIGGER_TARGET0_CAP_COMMAND     = 8,
    MINE_CAVERN_TRIGGER_TARGET1_CAP_COMMAND     = 14,
    MINE_CAVERN_TRIGGER_TARGET2_CAP_COMMAND     = 15,
    MINE_CAVERN_TRIGGER_TARGET3_CAP_COMMAND     = 16,
    MINE_CAVERN_TRIGGER_TARGET_CAP_COMMAND      = 12
};

/// Interaction regions shared by all three cavern rooms.
///
/// Includes warps to the gorge and secret passage, room CAP interactions,
/// a progress caption outside battle and CAP 12 around the four targets.
/// Geometry uses whole room-coordinate units and Q12 normals. Setup mutates
/// coordinates, links and enable flags, including the thirteenth LAST record;
/// keep storage loaded until the runtime list is unlinked.
static WorldCollisionTrigger _gMineCavernActionTriggers[13] = {
    { NULL, NULL, NULL, { 0x4740, -48, 1888, 0 }, { { -544, 0, -1824, 0 }, { 544, 0, -1824, 0 }, { -544, 0, 1824, 0 }, { 544, 0, 1824, 0 } }, { 0, 4097, 0, 0 }, { -4096, 0, 0, 0 }, 1902, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_MINE_GORGE, MINE_CAVERN_TRIGGER_GORGE_WARP_REQUEST, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1792, -48, -352, 0 }, { { 1024, 0, -544, 0 }, { 1024, 0, 544, 0 }, { -1024, 0, -544, 0 }, { -1024, 0, 544, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4096, 0 }, 1159, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_MINE_SECRET_PASSAGE, MINE_CAVERN_TRIGGER_PASSAGE_WARP_REQUEST, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 896, -64, 1440, 0 }, { { -1056, 0, -864, 0 }, { 1088, 0, -864, 0 }, { -1056, 0, 1920, 0 }, { 1024, 0, -192, 0 } }, { 0, 4104, 0, 0 }, { 2275, 0, 3405, 0 }, 2187, WORLD_COLLISION_TRIGGER_ACTION_CAP, MINE_CAVERN_TRIGGER_PASSAGE_STATUS_COMMAND, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6880, -64, 832, 0 }, { { -1344, 0, -864, 0 }, { 1344, 0, -864, 0 }, { -1344, 0, 864, 0 }, { 1344, 0, 864, 0 } }, { 0, 4108, 0, 0 }, { 0, 0, 4096, 0 }, 1593, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, MINE_CAVERN_TRIGGER_PROGRESS_CAPTION_ACTION, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1360, -64, 32, 0 }, { { -560, 0, -1216, 0 }, { 592, 0, -1216, 0 }, { -560, 0, 1216, 0 }, { 528, 0, 1216, 0 } }, { 0, 4099, 0, 0 }, { 4076, 0, 401, 0 }, 1348, WORLD_COLLISION_TRIGGER_ACTION_CAP, MINE_CAVERN_TRIGGER_PASSAGE_STATUS_COMMAND, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4512, -64, 224, 0 }, { { -1514, 0, -997, 0 }, { 166, 0, -99, 0 }, { -455, 0, 1954, 0 }, { 2505, 0, -316, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, 4096, 0 }, 2521, WORLD_COLLISION_TRIGGER_ACTION_CAP, MINE_CAVERN_TRIGGER_TARGET0_CAP_COMMAND, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4320, -64, 8864, 0 }, { { 1906, 0, 290, 0 }, { -66, 0, 293, 0 }, { -19, 0, -1958, 0 }, { -1910, 0, 269, 0 } }, { 0, 4112, 0, 0 }, { 0, 0, 4096, 0 }, 1958, WORLD_COLLISION_TRIGGER_ACTION_CAP, MINE_CAVERN_TRIGGER_TARGET2_CAP_COMMAND, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3580, -64, 8832, 0 }, { { 1906, 0, 226, 0 }, { -66, 0, 229, 0 }, { -19, 0, -1670, 0 }, { -1846, 0, 173, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, 4096, 0 }, 1915, WORLD_COLLISION_TRIGGER_ACTION_CAP, MINE_CAVERN_TRIGGER_TARGET3_CAP_COMMAND, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3546, -64, 356, 0 }, { { -1912, 0, -262, 0 }, { 60, 0, -265, 0 }, { 13, 0, 1570, 0 }, { 1840, 0, -305, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, 4096, 0 }, 1928, WORLD_COLLISION_TRIGGER_ACTION_CAP, MINE_CAVERN_TRIGGER_TARGET1_CAP_COMMAND, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8848, -64, 5312, 0 }, { { -2576, 0, -448, 0 }, { 2576, 0, -448, 0 }, { -2576, 0, 448, 0 }, { 2576, 0, 448, 0 } }, { 0, 4109, 0, 0 }, { 0, 0, 4096, 0 }, 2610, WORLD_COLLISION_TRIGGER_ACTION_CAP, MINE_CAVERN_TRIGGER_TARGET_CAP_COMMAND, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9200, -64, 2432, 0 }, { { -5504, 0, -448, 0 }, { 5504, 0, -448, 0 }, { -5504, 0, 448, 0 }, { 5504, 0, 448, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, -4096, 0 }, 5514, WORLD_COLLISION_TRIGGER_ACTION_CAP, MINE_CAVERN_TRIGGER_TARGET_CAP_COMMAND, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4784, -64, 6048, 0 }, { { -1472, 0, -448, 0 }, { 1472, 0, -448, 0 }, { -1472, 0, 448, 0 }, { 1472, 0, 448, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, 4096, 0 }, 1536, WORLD_COLLISION_TRIGGER_ACTION_CAP, MINE_CAVERN_TRIGGER_TARGET_CAP_COMMAND, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3300, -64, 6112, 0 }, { { -1472, 0, -448, 0 }, { 1472, 0, -448, 0 }, { -1472, 0, 448, 0 }, { 1472, 0, 448, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, 4096, 0 }, 1536, WORLD_COLLISION_TRIGGER_ACTION_CAP, MINE_CAVERN_TRIGGER_TARGET_CAP_COMMAND, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Two room-space sight-blocking quads shared by cavern rooms 1..3.
///
/// Geometry uses whole game units and Q12 normals. The low flag bit is retained
/// with no proven interpretation. Setup links and enables both records; the
/// second carries LAST. Keep mutable storage loaded until the list is unlinked.
static WorldCollisionOccluder _gMineCavernSightOccluders[2] = {
    { NULL, NULL, { 9216, -2384, 4448, 0 }, { { -5856, -3408, -1088, 0 }, { 5856, -3408, 1088, 0 }, { -5856, 3408, -1088, 0 }, { 5856, 3408, 1088, 0 } }, { 750, 0, -4041, 0 }, 6850, 1, 0 },
    { NULL, NULL, { 9183, -2320, 4399, 0 }, { { -5890, -3344, 1060, 0 }, { 5891, -3344, -1059, 0 }, { -5890, 3344, 1060, 0 }, { 5891, 3344, -1059, 0 } }, { -726, 0, -4034, 0 }, 6850, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

// Placement/resource keys and the shared texture adjustment of cavern actors.
enum {
    MINE_CAVERN_RESOURCE_BRAIN_STINGER   = 3,
    MINE_CAVERN_RESOURCE_ZEBRA_STALKER   = 6,
    MINE_CAVERN_RESOURCE_KNIGHT_GOLEM    = 22,
    MINE_CAVERN_RESOURCE_BLIZZARD_CHASER = 30,
    MINE_CAVERN_ZEBRA_STALKER_NO_CEILING = 1,
    MINE_CAVERN_ACTOR_CLUT_ROW_OFFSET    = 2
};

/// Blizzard Chaser loading and spawn recipe for cavern placement variant 1.
///
/// Resource key 30 resolves placement slot 0 to file 403000 and descriptor 0.
/// The second record ends the list. Storage and its task descriptor remain
/// borrowed during loading, spawning and model-flag updates.
static AreaResource _gMineCavernVariant1Resources[2] = {
    { .entryId = MINE_CAVERN_RESOURCE_BLIZZARD_CHASER, .fileNumber = 30, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor403000TaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Zebra Stalker loading and spawn recipe for cavern placement variant 2.
///
/// Resource key 6 resolves the two placements to file 400600 and descriptor 0.
/// The second record ends the table. Storage and its task descriptor must stay
/// loaded through loading, spawning and model-flag updates.
static AreaResource _gMineCavernVariant2Resources[2] = {
    { .entryId = MINE_CAVERN_RESOURCE_ZEBRA_STALKER, .fileNumber = 6, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor400600ZebraStalkerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Zebra Stalker loading and spawn recipe for cavern placement variant 3.
///
/// Resource key 6 resolves the two placements to file 400600 and descriptor 0.
/// The second record ends the table. Storage and its task descriptor must stay
/// loaded through loading, spawning and model-flag updates.
static AreaResource _gMineCavernVariant3Resources[2] = {
    { .entryId = MINE_CAVERN_RESOURCE_ZEBRA_STALKER, .fileNumber = 6, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor400600ZebraStalkerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Brain Stinger loading and spawn recipe for cavern placement variant 11.
///
/// Resource key 3 resolves both placements to file 100300 and descriptor 0.
/// The second record ends the table. Storage and its task table must stay
/// loaded through loading, spawning and model-flag updates.
static AreaResource _gMineCavernVariant11Resources[2] = {
    { .entryId = MINE_CAVERN_RESOURCE_BRAIN_STINGER, .fileNumber = 3, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = gActor100300Tasks },
    { .entryId = AREA_PLACEMENT_END },
};

/// Knight GOLEM loading and spawn recipe for cavern placement variant 17.
///
/// Resource key 22 resolves the placement to file 402200 and descriptor 0.
/// The second record ends the table. Storage and its task descriptor must stay
/// loaded through loading, spawning and model-flag updates.
static AreaResource _gMineCavernVariant17Resources[2] = {
    { .entryId = MINE_CAVERN_RESOURCE_KNIGHT_GOLEM, .fileNumber = 22, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .unknown_6 = { 0, 0 }, .taskTable = &gActor402200GolemTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Blizzard Chaser placement for the cavern's initial layout, variant 1.
///
/// Resource key 30 supplies the actor in placement slot 0; the next record
/// terminates the list. XYZ uses world units and yaw uses 4096 units per turn.
/// The base-file placement applies a two-row CLUT offset. Spawned actors borrow
/// the placement, so the cavern must remain loaded until they are released.
static AreaPlacement _gMineCavernVariant1Placements[2] = {
    { .entryId = MINE_CAVERN_RESOURCE_BLIZZARD_CHASER, .variant = 0, .mode = 0, .x = 2000, .y = 0, .z = 3960, .yaw = 3072, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = MINE_CAVERN_ACTOR_CLUT_ROW_OFFSET, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

/// Two Zebra Stalker placements for cavern layout variant 2.
///
/// Both use resource key 6 and mode 1, disabling ceiling jumps. XYZ uses world
/// units and yaw uses 4096 units per turn. Base-file loading applies a two-row
/// CLUT offset; the third record ends the list. Spawned actors borrow these
/// records, so the cavern must remain loaded until they are released.
static AreaPlacement _gMineCavernVariant2Placements[3] = {
    { .entryId = MINE_CAVERN_RESOURCE_ZEBRA_STALKER, .variant = 0, .mode = MINE_CAVERN_ZEBRA_STALKER_NO_CEILING, .x = 0x2C24, .y = 0, .z = 1750, .yaw = 900, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = MINE_CAVERN_ACTOR_CLUT_ROW_OFFSET, .rowIndex = 0 },
    { .entryId = MINE_CAVERN_RESOURCE_ZEBRA_STALKER, .variant = 0, .mode = MINE_CAVERN_ZEBRA_STALKER_NO_CEILING, .x = 6700, .y = 0, .z = 1500, .yaw = 3000, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = MINE_CAVERN_ACTOR_CLUT_ROW_OFFSET, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

/// Two Zebra Stalker placements for cavern layout variant 3.
///
/// Both use resource key 6 and mode 1, disabling ceiling jumps. XYZ uses world
/// units and yaw uses 4096 units per turn. Base-file loading applies a two-row
/// CLUT offset; the third record ends the list. Spawned actors borrow these
/// records, so the cavern must remain loaded until they are released.
static AreaPlacement _gMineCavernVariant3Placements[3] = {
    { .entryId = MINE_CAVERN_RESOURCE_ZEBRA_STALKER, .variant = 0, .mode = MINE_CAVERN_ZEBRA_STALKER_NO_CEILING, .x = 2272, .y = 0, .z = 1792, .yaw = 1536, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = MINE_CAVERN_ACTOR_CLUT_ROW_OFFSET, .rowIndex = 0 },
    { .entryId = MINE_CAVERN_RESOURCE_ZEBRA_STALKER, .variant = 0, .mode = MINE_CAVERN_ZEBRA_STALKER_NO_CEILING, .x = 0x2920, .y = 0, .z = 7296, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = MINE_CAVERN_ACTOR_CLUT_ROW_OFFSET, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

/// Two Brain Stinger placements for cavern layout variant 11.
///
/// Resource key 3 supplies both actors; modes 0 and 1 select their patrol routes.
/// XYZ uses world units and yaw uses 4096 units per turn. Base-file loading
/// applies a two-row CLUT offset; the third record ends the list. Spawned actors
/// borrow these records, so the cavern must stay loaded until they are released.
static AreaPlacement _gMineCavernVariant11Placements[3] = {
    { .entryId = MINE_CAVERN_RESOURCE_BRAIN_STINGER, .variant = 0, .mode = 0, .x = 5500, .y = 0, .z = 1500, .yaw = 1024, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = MINE_CAVERN_ACTOR_CLUT_ROW_OFFSET, .rowIndex = 0 },
    { .entryId = MINE_CAVERN_RESOURCE_BRAIN_STINGER, .variant = 0, .mode = 1, .x = 8500, .y = 0, .z = 7500, .yaw = 2048, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = MINE_CAVERN_ACTOR_CLUT_ROW_OFFSET, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

/// Stationary Knight GOLEM placement for cavern layout variant 17.
///
/// Resource key 22 supplies placement slot 0 at the world origin with zero yaw;
/// mode 0 disables patrol. Base-file loading applies a two-row CLUT offset.
/// The second record ends the list. The actor borrows its placement, so the
/// cavern must remain loaded until it is released.
static AreaPlacement _gMineCavernVariant17Placements[2] = {
    { .entryId = MINE_CAVERN_RESOURCE_KNIGHT_GOLEM, .variant = 0, .mode = 0, .x = 0, .y = 0, .z = 0, .yaw = 0, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = MINE_CAVERN_ACTOR_CLUT_ROW_OFFSET, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gMineCavernAreaVariants[22] = {
    { NULL, NULL },
    { _gMineCavernVariant1Placements, _gMineCavernVariant1Resources },
    { _gMineCavernVariant2Placements, _gMineCavernVariant2Resources },
    { _gMineCavernVariant3Placements, _gMineCavernVariant3Resources },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { _gMineCavernVariant11Placements, _gMineCavernVariant11Resources },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { _gMineCavernVariant17Placements, _gMineCavernVariant17Resources },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

enum {
    MINE_CAVERN_FOOTSTEP_WALK_SCRIPT = 73,
    MINE_CAVERN_FOOTSTEP_RUN_SCRIPT  = 75
};

/// Walk, run and scripted-jump footstep requests for cavern surface classes 1 and 2.
///
/// Type-1 requests select the loaded bank: walk/jump use entry 73, running 75.
/// The paired animation cue adds one and companion playback adds 100.
/// Surface records borrow this room-owned record while the cavern is loaded.
static WorldCollisionFootstepSounds _gMineCavernFootstepSounds = {
    SOUND_SCRIPT_REQUEST_TYPE_1 | MINE_CAVERN_FOOTSTEP_WALK_SCRIPT,
    SOUND_SCRIPT_REQUEST_TYPE_1 | MINE_CAVERN_FOOTSTEP_RUN_SCRIPT,
    SOUND_SCRIPT_REQUEST_TYPE_1 | MINE_CAVERN_FOOTSTEP_WALK_SCRIPT,
};

/// Shared collision policy for cavern surface classes 0 and 3..7.
///
/// Blocks probes, permits weapon impacts and applies pushback without footstep
/// cues. The class directory borrows this individual record while loaded.
static WorldCollisionSurfaceProperties _gMineCavernDefaultSurface = {
    0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL
};

/// Collision policy for cavern surface class 1.
///
/// Blocks probes, permits weapon impacts and applies pushback, with the room's
/// footstep cues. The class directory borrows this individual record while loaded.
static WorldCollisionSurfaceProperties _gMineCavernSurfaceClass1Properties = {
    0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &_gMineCavernFootstepSounds
};

/// Collision policy for cavern surface class 2.
///
/// Passes probes, permits weapon impacts and applies pushback, with the room's
/// footstep cues. The class directory borrows this individual record while loaded.
static WorldCollisionSurfaceProperties _gMineCavernSurfaceClass2Properties = {
    0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &_gMineCavernFootstepSounds
};

WorldCollisionSurfaceProperties* gMineCavernSurfacePropertiesByClass[8] = {
    &_gMineCavernDefaultSurface,
    &_gMineCavernSurfaceClass1Properties,
    &_gMineCavernSurfaceClass2Properties,
    &_gMineCavernDefaultSurface,
    &_gMineCavernDefaultSurface,
    &_gMineCavernDefaultSurface,
    &_gMineCavernDefaultSurface,
    &_gMineCavernDefaultSurface,
};

enum { MINE_CAVERN_POST_EVENT_MAP_MARK = 1 };

AreaApplyRec gMineCavernPostEventAreaUpdates[9] = {
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_MESA, 2, AREA_APPLY_MODE_ALWAYS | MINE_CAVERN_POST_EVENT_MAP_MARK },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_CAVERN, 4, AREA_APPLY_MODE_ALWAYS },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_TUNNEL_ENTRANCE, 4, AREA_APPLY_MODE_ALWAYS | MINE_CAVERN_POST_EVENT_MAP_MARK },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_TUNNEL, 4, AREA_APPLY_MODE_ALWAYS | MINE_CAVERN_POST_EVENT_MAP_MARK },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_GORGE, 2, AREA_APPLY_MODE_ALWAYS | MINE_CAVERN_POST_EVENT_MAP_MARK },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_FORKED_TUNNEL, 2, AREA_APPLY_MODE_ALWAYS | MINE_CAVERN_POST_EVENT_MAP_MARK },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STOREROOM, 11, AREA_APPLY_MODE_BOUNTY_NIGHTMARE | MINE_CAVERN_POST_EVENT_MAP_MARK },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, 11, AREA_APPLY_MODE_BOUNTY_NIGHTMARE | MINE_CAVERN_POST_EVENT_MAP_MARK },
    { AREA_APPLY_END, 0, 0, 0 },
};

/// GPU red intensity at the centre of each fixed cavern glow fan.
///
/// Unsigned byte copied to the centre vertex of every additive wedge.
static u8 _gMineCavernFixedGlowCenterRed = 48;

/// GPU green intensity at the centre of each fixed cavern glow fan.
///
/// Unsigned byte copied to the centre vertex of every additive wedge.
static u8 _gMineCavernFixedGlowCenterGreen = 32;

/// GPU blue intensity at the centre of each fixed cavern glow fan.
///
/// Unsigned byte copied to the centre vertex of every additive wedge.
static u8 _gMineCavernFixedGlowCenterBlue = 0;

/// GPU red intensity at both rim vertices of each fixed cavern glow wedge.
///
/// The zero rim fades the additive centre colour to black at the perimeter.
static u8 _gMineCavernFixedGlowRimRed = 0;

/// GPU green intensity at both rim vertices of each fixed cavern glow wedge.
///
/// The zero rim fades the additive centre colour to black at the perimeter.
static u8 _gMineCavernFixedGlowRimGreen = 0;

/// GPU blue intensity at both rim vertices of each fixed cavern glow wedge.
///
/// The zero rim fades the additive centre colour to black at the perimeter.
static u8 _gMineCavernFixedGlowRimBlue = 0;

/// Halfword stored after the fixed glow's colours. Nothing in the package reads
/// it, so its role is unproven.
static u16 D_mine_cavern_8018E356 = 1128;

/// Red intensity (0..255) at the centre of each destroyed target's additive glow.
static u8 _gMineCavernTargetGlowCenterRed = 42;
/// Green intensity (0..255) at the centre of each destroyed target's additive glow.
static u8 _gMineCavernTargetGlowCenterGreen = 25;
/// Blue intensity (0..255) at the centre of each destroyed target's additive glow.
static u8 _gMineCavernTargetGlowCenterBlue = 0;
/// Red intensity at both rim vertices of each target glow; zero fades red to black.
static u8 _gMineCavernTargetGlowRimRed = 0;
/// Green intensity at both rim vertices of each target glow; zero fades green to black.
static u8 _gMineCavernTargetGlowRimGreen = 0;
/// Blue intensity at both rim vertices of each target glow; zero adds no blue at the rim.
static u8 _gMineCavernTargetGlowRimBlue = 0;

/// Halfword stored after the target glow's colours. Nothing in the package
/// reads it, so its role is unproven.
static u16 D_mine_cavern_8018E35E = 1960;

static void _mineCavernRefreshTargetLight(s16 targetIndex);
static void _mineCavernDrawTargetGlow(s16 targetIndex);
static void _mineCavernTargetSpawn(Enemy* enemy, Task* task);
static void _mineCavernTargetCheckHit(Enemy* enemy, Task* task);
static void _mineCavernTargetRemainsSpawn(Enemy* enemy, Task* task);
static void _mineCavernTargetRetireBody(Enemy* enemy, Task* task);

/// Selects the cavern's mote, halo, orange-burst and spark-emitter effect tasks.
///
/// Installs complete bank-6 effect IDs for actor and effect spawners, without
/// spawning any tasks. Called once when the cavern glow task initializes;
/// the cavern overlay must stay loaded while these bindings are used.
static inline void _mineCavernRegisterRoomEffects(void)
{
    gRoomEffectMoteId         = EFFECT_MINE_CAVERN_MOTE;
    gRoomEffectHaloId         = EFFECT_MINE_CAVERN_HALO;
    gRoomEffectOrangeBurstId  = EFFECT_MINE_CAVERN_ORANGE_BURST;
    gRoomEffectSparkEmitterId = EFFECT_MINE_CAVERN_SPARK_EMITTER;
}

/// Starts and mixes a target sound with separately sampled signed-byte pans.
///
/// Arguments are stable expressions: each is evaluated once per sound request.
/// `soundCoord` borrows a composed coordinate; `attenuation` is 0..127 in script attenuation units.
/// Start precedes mix; each samples pan separately even when queue admission
/// fails. The casts intentionally narrow the pan to its low signed byte.
#define MINE_CAVERN_START_AND_MIX_TARGET_SOUND(scriptId, soundCoord, attenuation)                           \
    do {                                                                                                    \
        sndEvtRequestScriptStart((scriptId), (s8)worldCoordGetOriginAudioPan((soundCoord)), (attenuation)); \
        sndEvtRequestScriptMix((scriptId), (s8)worldCoordGetOriginAudioPan((soundCoord)), (attenuation));   \
    } while (0)

/// Adds the four saved destroyed-target bits to the caller's count.
///
/// `destroyedMask` is a stable s32 nibble; only its low four bits are tested.
/// `destroyedCount` is a writable u8 or s16, incremented once per set bit;
/// callers initialize it to zero to obtain a count in 0..4. `bitIndex` is a
/// distinct writable s32 or s16, overwritten by the loop and left at four.
/// Captures the four-point table for its bound. All arguments are evaluated
/// repeatedly and must have no access side effects; use as a standalone loop.
#define MINE_CAVERN_COUNT_DESTROYED_TARGETS(destroyedMask, destroyedCount, bitIndex)             \
    for ((bitIndex) = 0; (bitIndex) < ARRAY_SIZE(gMineCavernTargetEffectPoints); (bitIndex)++) { \
        if (((destroyedMask) >> (bitIndex)) & 1) {                                               \
            (destroyedCount)++;                                                                  \
        }                                                                                        \
    }

/// Selects room 2 in the live save and session, requesting room-object relinking.
static void _mineCavernActivateRoom2(void)
{
    enum { MINE_CAVERN_POST_SCENE_ROOM = 2 };

    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = MINE_CAVERN_POST_SCENE_ROOM;
    gGameSession->location.roomState                            = MINE_CAVERN_POST_SCENE_ROOM;
    gGameSession->roomObjsDirty                                 = true;
}

/// No-op callback retained after the room switch in both cavern event scripts.
static void _mineCavernScriptNoop(void)
{
}

/// Prepares the final scripted battle in cavern variant 4.
///
/// Clears battle phase, holds, PE target count and pending rewards before the
/// script engages combat. Requires the live cavern session and combat state.
static void _mineCavernPrepareFinalEventBattle(void)
{
    enum { MINE_CAVERN_FINAL_EVENT_VARIANT = 4 };

    gGameSession->location.variant              = MINE_CAVERN_FINAL_EVENT_VARIANT;
    gSceneCombatState.signals.bytes.battlePhase = SCENE_COMBAT_BATTLE_IDLE;
    gSceneCombatState.peTargetCount             = 0;
    gSceneCombatState.battleRefs                = 0;
    gSceneCombatState.expReward                 = 0;
    gSceneCombatState.bpReward                  = 0;
    gSceneCombatState.mpReward                  = 0;
}

void mineCavernResetEventBattleReleaseCount(void)
{
    gMineCavernEventBattleReleaseCount = 0;
}

/// Sets visibility of the five cavern sprite batches tied to nursery progress.
///
/// `views` borrows the loaded 25-view cavern directory with mutable batch arrays.
/// `hidden` is 0 to show or 1 to hide; the selected batches must remain live.
static inline void _mineCavernSetProgressSpriteBatchesHidden(const SpriteView* views, s32 hidden)
{
    views[3].batches[5].hidden  = hidden;
    views[4].batches[6].hidden  = hidden;
    views[21].batches[5].hidden = hidden;
    views[22].batches[3].hidden = hidden;
    views[23].batches[4].hidden = hidden;
}

void mineCavernSetProgressSpritesHidden(s32 hiddenValue)
{
    const GameLocationKey* location;
    const SpriteView*      views;
    s32                    hidden;

    location = &gGameSession->location;
    views    = gSpriteAreaTables[location->stage - 1]->areaViews[location->area - 1];
    hidden   = hiddenValue & 0xFF;

    if (hidden == true) {
        _mineCavernSetProgressSpriteBatchesHidden(views, hidden);
        return;
    }
    if (hidden == false) {
        _mineCavernSetProgressSpriteBatchesHidden(views, false);
    }
}

void mineCavernDrawRoomLightGlowsTask(Task* task)
{
    enum { MINE_CAVERN_GLOWS_INIT        = 0,
           MINE_CAVERN_GLOWS_ACTIVE      = 1,
           MINE_CAVERN_FLASH_ENABLED     = 1,
           MINE_CAVERN_FLASH_POINT       = 11,
           MINE_CAVERN_FLASH_JITTER_MASK = 63,
           MINE_CAVERN_FLASH_MIN_X       = 5990,
           MINE_CAVERN_FLASH_MIN_Y       = -1460,
           MINE_CAVERN_FLASH_MIN_Z       = -330 };

    if (task->state == MINE_CAVERN_GLOWS_INIT) {
        _mineCavernRegisterRoomEffects();
        gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
        task->state                      = MINE_CAVERN_GLOWS_ACTIVE;
    }

    if (gameFlagGetNibble(GAME_FLAG_0C4) == MINE_CAVERN_FLASH_ENABLED) {
        RANDOM_LCG_ADVANCE();
        if ((RANDOM_LCG_DRAW() & 7) == 0) {
            SVECTOR* flashPoint = &_gMineCavernRoomLightPoints[MINE_CAVERN_FLASH_POINT];

            RANDOM_LCG_ADVANCE();
            flashPoint->vx = (RANDOM_LCG_DRAW() & MINE_CAVERN_FLASH_JITTER_MASK) + MINE_CAVERN_FLASH_MIN_X;
            RANDOM_LCG_ADVANCE();
            flashPoint->vy = (RANDOM_LCG_DRAW() & MINE_CAVERN_FLASH_JITTER_MASK) + MINE_CAVERN_FLASH_MIN_Y;
            RANDOM_LCG_ADVANCE();
            flashPoint->vz = (RANDOM_LCG_DRAW() & MINE_CAVERN_FLASH_JITTER_MASK) + MINE_CAVERN_FLASH_MIN_Z;
            effectSpawn(EFFECT_FLASH_BURST, NULL, 0x300, flashPoint);
        }
    }

    // Mapped views select light points; three cases deliberately fall through.
    switch (viewGetMappedIndex() & 0xFF) {
        case 2:
        case 3:
        case 9: {
            const SVECTOR* lightPoints = &_gMineCavernRoomLightPoints[4];
            _glowDrawFlare(&lightPoints[0], 1, 0x300);
            _glowDrawFlare(&lightPoints[2], 1, 0x300);
            break;
        }
        case 4: {
            const SVECTOR* lightPoints = &_gMineCavernRoomLightPoints[3];
            _glowDrawFlare(&lightPoints[0], 1, 0x300);
            _glowDrawFlare(&lightPoints[5], 1, 0x300);
            break;
        }
        case 5:
            _glowDrawFlare(&_gMineCavernRoomLightPoints[5], 1, 0x300);
        case 23: {
            const SVECTOR* lightPoints = _gMineCavernRoomLightPoints;
            _glowDrawCapsule(&lightPoints[0], 0x180, 0x222);
            _glowDrawCapsule(&lightPoints[1], 0x180, 0x222);
            _glowDrawFlare(&lightPoints[3], 1, 0x300);
            break;
        }
        case 6: {
            const SVECTOR* lightPoints = &_gMineCavernRoomLightPoints[5];
            _glowDrawFlare(&lightPoints[0], 1, 0x300);
            _glowDrawFlare(&lightPoints[2], 1, 0x300);
            _glowDrawFlare(&lightPoints[4], 1, 0x300);
            break;
        }
        case 7: {
            const SVECTOR* lightPoints = &_gMineCavernRoomLightPoints[4];
            _glowDrawFlare(&lightPoints[0], 1, 0x300);
            _glowDrawFlare(&lightPoints[2], 1, 0x300);
            _glowDrawFlare(&lightPoints[3], 1, 0x300);
            break;
        }
        case 8:
        case 20:
            _glowDrawFlare(&_gMineCavernRoomLightPoints[6], 1, 0x300);
            break;
        case 10:
            _glowDrawFlare(&_gMineCavernRoomLightPoints[10], 1, 0x300);
            break;
        case 11:
            _glowDrawFlare(&_gMineCavernRoomLightPoints[5], 1, 0x300);
        case 13: {
            const SVECTOR* lightPoints = &_gMineCavernRoomLightPoints[3];
            _glowDrawFlare(&lightPoints[0], 1, 0x300);
            _glowDrawFlare(&lightPoints[5], 1, 0x300);
            _glowDrawFlare(&lightPoints[6], 1, 0x300);
            break;
        }
        case 14:
        case 16:
        case 21:
            _glowDrawFlare(&_gMineCavernRoomLightPoints[5], 1, 0x300);
            break;
        case 17:
            _glowDrawFlare(&_gMineCavernRoomLightPoints[7], 1, 0x300);
            break;
        case 24:
            _glowDrawFlare(&_gMineCavernRoomLightPoints[12], 1, 0x300);
        case 22:
            _glowDrawFlare(&_gMineCavernRoomLightPoints[3], 1, 0x300);
            break;
        case 25: {
            const SVECTOR* lightPoints = &_gMineCavernRoomLightPoints[3];
            _glowDrawFlare(&lightPoints[0], 1, 0x300);
            _glowDrawFlare(&lightPoints[2], 1, 0x300);
            break;
        }
    }
}

#include "../../shared/effects/glow_draw_capsule.inc.c"

#include "../../shared/effects/glow_draw_flare.inc.c"

#include "../../shared/room/room_visual_effects.inc.c"

void mineCavernRoomVisualEffectsMoteTask(Task* task)
{
    _roomVisualEffectsMoteTask(task);
}

#include "../../shared/room/room_visual_effects_halo.inc.c"

void mineCavernRoomVisualEffectsHaloTask(Task* task)
{
    _roomVisualEffectsHaloTask(task);
}

void mineCavernRoomVisualEffectsHaloOrangeBurstTask(Task* task)
{
    _roomVisualEffectsHaloOrangeBurstTask(task);
}

#include "../../shared/room/room_visual_effects_glow_quad.inc.c"
#include "../../shared/room/room_visual_effects_flash.inc.c"

void mineCavernRoomVisualEffectsSparkEmitterTask(Task* task)
{
    _roomVisualEffectsSparkEmitterTask(task);
}

/// Queues eight additive wedges at a pixel centre and camera Z / 4 depth.
///
/// Requires caller locals `triangle` (POLY_G3*), `blendMode` (DR_TPAGE*) and
/// `wedgeIndex` (s32), and live frame packet/OT storage. Positions preserve
/// unsigned halfword wrapping, radius is pixels, RGB expressions are bytes,
/// and `trigShift` is the fractional-bit count (12) for Q12 sine/cosine. Arguments are evaluated
/// repeatedly, including rim colours twice per wedge, and must be stable.
/// Reservations retain POLY_GT3/DR_MODE extents. Each blend command is linked
/// after its triangle and therefore executes first in the GPU's linked list.
/// The byte reservations are 40 + 12 bytes, larger than the submitted G3 and
/// tpage packets; retain those extents and require 416 free bytes per full fan.
#define MINE_CAVERN_DRAW_GLOW_FAN(screenX, screenY, pixelRadius, depthQuarterZ, trigShift, centerRed, centerGreen, centerBlue, rimRed, rimGreen, rimBlue) \
    for (wedgeIndex = 0; wedgeIndex < MINE_CAVERN_GLOW_WEDGES; wedgeIndex++) {                                                                            \
        triangle       = gGpuPrimCursor;                                                                                                                  \
        gGpuPrimCursor = (u8*)triangle + sizeof(POLY_GT3);                                                                                                \
        setPolyG3(triangle);                                                                                                                              \
        triangle->r0 = (centerRed);                                                                                                                       \
        triangle->g0 = (centerGreen);                                                                                                                     \
        triangle->b0 = (centerBlue);                                                                                                                      \
        triangle->x0 = (screenX);                                                                                                                         \
        triangle->y0 = (screenY);                                                                                                                         \
        triangle->r1 = (rimRed);                                                                                                                          \
        triangle->g1 = (rimGreen);                                                                                                                        \
        triangle->b1 = (rimBlue);                                                                                                                         \
        triangle->r2 = (rimRed);                                                                                                                          \
        triangle->g2 = (rimGreen);                                                                                                                        \
        triangle->b2 = (rimBlue);                                                                                                                         \
        setSemiTrans(triangle, 1);                                                                                                                        \
        triangle->x1 = (screenX) + ((rsin(wedgeIndex * GLOW_EIGHTH_TURN) * (pixelRadius)) >> (trigShift));                                                \
        triangle->y1 = (screenY) + ((rcos(wedgeIndex * GLOW_EIGHTH_TURN) * (pixelRadius)) >> (trigShift));                                                \
        triangle->x2 = (screenX) + ((rsin(wedgeIndex * GLOW_EIGHTH_TURN + GLOW_EIGHTH_TURN) * (pixelRadius)) >> (trigShift));                             \
        triangle->y2 = (screenY) + ((rcos(wedgeIndex * GLOW_EIGHTH_TURN + GLOW_EIGHTH_TURN) * (pixelRadius)) >> (trigShift));                             \
        addPrim(&gGpuCurrentOt[(depthQuarterZ) >> 4], triangle);                                                                                          \
        blendMode      = gGpuPrimCursor;                                                                                                                  \
        gGpuPrimCursor = (u8*)blendMode + sizeof(DR_MODE);                                                                                                \
        setDrawTPage(blendMode, 0, 0, getTPage(0, GPU_BLEND_ADD, 640, 0));                                                                                \
        addPrim(&gGpuCurrentOt[(depthQuarterZ) >> 4], blendMode);                                                                                         \
    }

/// Queues additive eight-wedge glows at the cavern's six fixed light points.
///
/// Skips point 3 in mapped view 4 and rejects negative projection flags. The
/// radius shrinks with the destroyed-target count and flickers with the LCG.
/// Depth is camera Z / 4 and must be positive and in the current OT's range;
/// pixel radius is the jittered scale times 352 divided by camera Z.
/// Requires a live view transform and frame packet arena; packets live until
/// GPU completion. Each wedge reserves 52 bytes, including its blend command.
static void _mineCavernDrawFixedGlows(void)
{
    s32       packedScreenPosition;
    s32       projectionFlags;
    s32       depthQuarterZ;
    POLY_G3*  triangle;
    DR_TPAGE* blendMode;
    s32       pixelRadius;
    s32       wedgeIndex;
    s32       destroyedMask;
    u8        destroyedCount;
    s32       pointIndex;
    s32       baseRadiusScale;
    u16       viewId;
    s32       radiusScale;
    s32       trigShift;
    u16       screenX;
    u16       screenY;

    destroyedMask  = gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_TARGETS_DESTROYED);
    viewId         = viewGetMappedIndex() & 0xFF;
    destroyedCount = 0;
    MINE_CAVERN_COUNT_DESTROYED_TARGETS(destroyedMask, destroyedCount, pointIndex);
    switch (destroyedCount) {
        case 0:
        case 1:
            baseRadiusScale = 0x428;
            break;
        case 2:
            baseRadiusScale = 0x3C0;
            break;
        case 3:
            baseRadiusScale = 0xC8;
            break;
        case 4:
        default:
            baseRadiusScale = 0x80;
            break;
    }
    // Project world points through the view; reject negative GTE flags.
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    radiusScale = baseRadiusScale;
    for (pointIndex = 0; pointIndex < ARRAY_SIZE(gMineCavernFixedGlowPoints); pointIndex++) {
        trigShift = GLOW_TRIG_SHIFT;
        if (pointIndex == 3 && viewId == 4) {
            continue;
        }
        gte_ldv0(&gMineCavernFixedGlowPoints[pointIndex]);
        gte_rtps();
        gte_stsxy(&packedScreenPosition);
        gte_stflg(&projectionFlags);
        gte_stszotz(&depthQuarterZ);
        if (projectionFlags < 0) {
            continue;
        }
        screenX = packedScreenPosition;
        screenY = packedScreenPosition >> 16;
        RANDOM_LCG_ADVANCE();
        pixelRadius = (s32)(radiusScale + (RANDOM_LCG_DRAW() & 0xF)) * MINE_CAVERN_GLOW_PROJECTION_SCALE / (depthQuarterZ * 4);
        MINE_CAVERN_DRAW_GLOW_FAN(screenX, screenY, pixelRadius, depthQuarterZ, trigShift,
                                  _gMineCavernFixedGlowCenterRed, _gMineCavernFixedGlowCenterGreen, _gMineCavernFixedGlowCenterBlue,
                                  _gMineCavernFixedGlowRimRed, _gMineCavernFixedGlowRimGreen, _gMineCavernFixedGlowRimBlue);
    }
}

/// Refreshes the transient light at one destroyed target's position.
///
/// `targetIndex` is 0..3 and selects transient slot 4..7 for two light updates.
/// The loaded room supplies Q12 RGB and world-unit radii; the outer radius
/// receives 0..2047 units of jitter. The slot retains its view-coordinate parent.
static void _mineCavernRefreshTargetLight(s16 targetIndex)
{
    enum { MINE_CAVERN_TARGET_LIGHT_FIRST_SLOT = 4,
           MINE_CAVERN_TARGET_LIGHT_TICKS      = 2 };

    WorldCoordTransientPointLight* lightSlot  = &gWorldCoordTransientPointLights[MINE_CAVERN_TARGET_LIGHT_FIRST_SLOT + targetIndex];
    WorldCoordPointLight*          pointLight = &lightSlot->light;

    lightSlot->framesLeft                              = MINE_CAVERN_TARGET_LIGHT_TICKS;
    pointLight->inner                                  = gMineCavernTargetLightInnerRadius;
    pointLight->outer                                  = gMineCavernTargetLightBaseOuterRadius + (((RANDOM_LCG_ADVANCE()) >> 16) & 0x7FF);
    pointLight->head.color.r                           = gMineCavernTargetLightRed;
    pointLight->head.color.g                           = gMineCavernTargetLightGreen;
    pointLight->head.color.b                           = gMineCavernTargetLightBlue;
    pointLight->head.transform.lighting.local.t[0]     = gMineCavernTargetEffectPoints[targetIndex].vx;
    pointLight->head.transform.lighting.local.t[1]     = gMineCavernTargetEffectPoints[targetIndex].vy;
    pointLight->head.transform.lighting.local.t[2]     = gMineCavernTargetEffectPoints[targetIndex].vz;
    lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Queues an additive eight-wedge glow at one destroyed target's position.
///
/// `targetIndex` is 0..3. Radius shrinks with the destroyed-target count; its
/// low seven bits flicker with the LCG. Negative projection flags emit nothing.
/// Depth is camera Z / 4 and must be positive and in the current OT's range;
/// pixel radius is the jittered scale times 352 divided by camera Z.
/// Requires live room/view data and frame packet storage through GPU completion.
/// Each wedge reserves 52 bytes, including its blend command.
static void _mineCavernDrawTargetGlow(s16 targetIndex)
{
    s32       packedScreenPosition;
    s32       projectionFlags;
    s32       depthQuarterZ;
    POLY_G3*  triangle;
    DR_TPAGE* blendMode;
    s32       pixelRadius;
    s32       wedgeIndex;
    s32       destroyedMask;
    u8        destroyedCount;
    s32       bitIndex;
    s32       baseRadiusScale;
    u16       screenX;
    u16       screenY;

    destroyedMask  = gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_TARGETS_DESTROYED);
    destroyedCount = 0;
    MINE_CAVERN_COUNT_DESTROYED_TARGETS(destroyedMask, destroyedCount, bitIndex);
    switch (destroyedCount) {
        case 0:
        case 1:
            baseRadiusScale = 0x780;
            break;
        case 2:
            baseRadiusScale = 0x500;
            break;
        case 3:
            baseRadiusScale = 0x280;
            break;
        case 4:
        default:
            baseRadiusScale = 0x200;
            break;
    }
    // Project world points through the view; reject negative GTE flags.
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_ldv0(&gMineCavernTargetEffectPoints[targetIndex]);
    gte_rtps();
    gte_stsxy(&packedScreenPosition);
    gte_stflg(&projectionFlags);
    gte_stszotz(&depthQuarterZ);
    if (projectionFlags >= 0) {
        screenX = packedScreenPosition;
        screenY = packedScreenPosition >> 16;
        RANDOM_LCG_ADVANCE();
        pixelRadius = (s32)(baseRadiusScale | (RANDOM_LCG_DRAW() & 0x7F)) * MINE_CAVERN_GLOW_PROJECTION_SCALE / (depthQuarterZ * 4);
        MINE_CAVERN_DRAW_GLOW_FAN(screenX, screenY, pixelRadius, depthQuarterZ, GLOW_TRIG_SHIFT,
                                  _gMineCavernTargetGlowCenterRed, _gMineCavernTargetGlowCenterGreen, _gMineCavernTargetGlowCenterBlue,
                                  _gMineCavernTargetGlowRimRed, _gMineCavernTargetGlowRimGreen, _gMineCavernTargetGlowRimBlue);
    }
}

#undef MINE_CAVERN_DRAW_GLOW_FAN

/// Updates destroyed targets' lights, view-dependent sounds, glows and puffs.
///
/// Targets 0..3 run before story chapter 5. Light and sound updates are independent
/// of the visibility lists; sounds refresh when a view becomes ready or the mask
/// changes. Visible targets emit puffs every ninth running tick or on a view
/// change, with world offsets -64..63 on each axis. Actor pause/hide stops puff
/// emission and cadence advancement, while glow and light updates continue.
/// Cadence uses the shared tick counter's signed low halfword.
/// Requires the loaded cavern's target tables, view, sound bank and frame arena.
static void _mineCavernUpdateTargetEffects(void)
{
    enum { MINE_CAVERN_TARGET_PUFF_INTERVAL        = 9,
           MINE_CAVERN_TARGET_EFFECTS_STOP_CHAPTER = 5 };

    // The binary retains this unused vector in the stack frame.
    VECTOR         unused;
    GfxCoord       puffCoord;
    const SVECTOR* targetPoint;
    s32            viewId;
    s32            destroyedMask;
    s16            targetIndex;
    s16            viewIndex;
    s16            visibleView;

    viewId        = viewGetMappedIndex() & 0xFF;
    destroyedMask = gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_TARGETS_DESTROYED);
    for (targetIndex = 0; targetIndex < ARRAY_SIZE(gMineCavernTargetEffectPoints) && gameFlagGetNibble(EVENT_FLAG_STORY_CHAPTER) < MINE_CAVERN_TARGET_EFFECTS_STOP_CHAPTER; targetIndex++) {
        if (!((destroyedMask >> targetIndex) & 1)) {
            continue;
        }
        _mineCavernRefreshTargetLight(targetIndex);
        if (gGameSession->viewReady == true || gMineCavernTargetEffectsDestroyedMask != destroyedMask) {
            _mineCavernUpdateTargetSound(targetIndex);
        }
        for (viewIndex = 0; viewIndex < ARRAY_SIZE(gMineCavernTargetEffectViews[targetIndex]) && gMineCavernTargetEffectViews[targetIndex][viewIndex] != MINE_CAVERN_TARGET_EFFECT_VIEW_NONE; viewIndex++) {
            visibleView = gMineCavernTargetEffectViews[targetIndex][viewIndex];
            if (visibleView != (u8)viewId) {
                continue;
            }
            _mineCavernDrawTargetGlow(targetIndex);
            if ((s16)((s16)gMineCavernTargetEffectsTicks % MINE_CAVERN_TARGET_PUFF_INTERVAL) != 0 && gMineCavernTargetEffectsPreviousView == visibleView) {
                continue;
            }
            if (gSceneCombatState.actorControl != SCENE_COMBAT_ACTORS_RUNNING) {
                continue;
            }
            // This puff uses the placement snapshot and never follows its retained parent pointer.
            gfxSetRotIdentity(&puffCoord.coord);
            puffCoord.parent       = &gGfxViewCoord;
            targetPoint            = &gMineCavernTargetEffectPoints[targetIndex];
            puffCoord.coord.t[0]   = targetPoint->vx + ((RANDOM_LCG_ADVANCE()) >> 16 & 0x7F) - 0x40;
            puffCoord.coord.t[1]   = targetPoint->vy + ((RANDOM_LCG_ADVANCE()) >> 16 & 0x7F) - 0x40;
            puffCoord.coord.t[2]   = targetPoint->vz + ((RANDOM_LCG_ADVANCE()) >> 16 & 0x7F) - 0x40;
            puffCoord.composeStamp = GRAPHICS_COORD_DIRTY;
            effectSpawn(EFFECT_ADDITIVE_PUFF, &puffCoord, 0x800004FF, NULL);
        }
    }
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        gMineCavernTargetEffectsTicks++;
    }
    gMineCavernTargetEffectsPreviousView  = viewId;
    gMineCavernTargetEffectsDestroyedMask = destroyedMask;
}

/// Queues the cavern's subtractive darkness, fixed glows and target glow/smoke passes.
///
/// The four destroyed-target bits select tint row 0..4. A centred 320x240 quad
/// and its subtractive drawing-mode command are pushed onto OT bucket zero;
/// the command executes before the quad. Reserves one `POLY_F4` and a full
/// `DR_MODE` although only its first command word is submitted. Requires live
/// cavern point/tint tables, session, scratch stack and enough frame packet/OT
/// storage for this pair and the two subsequent passes. Retains no packets
/// beyond the frame and does not restore the previous drawing mode.
static void _mineCavernDrawDarknessAndEffects(void)
{
    enum { MINE_CAVERN_DARKNESS_HALF_WIDTH  = 160,
           MINE_CAVERN_DARKNESS_HALF_HEIGHT = 120 };
    POLY_F4* darknessQuad;
    DR_MODE* subtractMode;
    s32      destroyedMask;
    s16      targetIndex;
    s16      destroyedCount;

    destroyedMask  = gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_TARGETS_DESTROYED);
    destroyedCount = 0;

    darknessQuad   = gGpuPrimCursor;
    gGpuPrimCursor = darknessQuad + 1;
    setPolyF4(darknessQuad);
    setSemiTrans(darknessQuad, 1);

    MINE_CAVERN_COUNT_DESTROYED_TARGETS(destroyedMask, destroyedCount, targetIndex);
    darknessQuad->r0 = gMineCavernDarknessColors[destroyedCount].r;
    darknessQuad->g0 = gMineCavernDarknessColors[destroyedCount].g;
    darknessQuad->b0 = gMineCavernDarknessColors[destroyedCount].b;

    darknessQuad->x0 = -MINE_CAVERN_DARKNESS_HALF_WIDTH;
    darknessQuad->y0 = -MINE_CAVERN_DARKNESS_HALF_HEIGHT;
    darknessQuad->x1 = MINE_CAVERN_DARKNESS_HALF_WIDTH;
    darknessQuad->y1 = -MINE_CAVERN_DARKNESS_HALF_HEIGHT;
    darknessQuad->x2 = -MINE_CAVERN_DARKNESS_HALF_WIDTH;
    darknessQuad->y2 = MINE_CAVERN_DARKNESS_HALF_HEIGHT;
    darknessQuad->x3 = MINE_CAVERN_DARKNESS_HALF_WIDTH;
    darknessQuad->y3 = MINE_CAVERN_DARKNESS_HALF_HEIGHT;
    addPrim(gGpuCurrentOt, darknessQuad);

    // Head insertion puts the blend selection before the quad in GPU execution order.
    subtractMode   = gGpuPrimCursor;
    gGpuPrimCursor = subtractMode + 1;
    setDrawTPage(subtractMode, 0, 0, getTPage(0, GPU_BLEND_SUBTRACT, 640, 0));
    addPrim(gGpuCurrentOt, subtractMode);

    _mineCavernDrawFixedGlows();
    _mineCavernUpdateTargetEffects();
}

#undef MINE_CAVERN_COUNT_DESTROYED_TARGETS

enum {
    MINE_CAVERN_TARGET_EFFECTS_STATE_SPAWN   = 0,
    MINE_CAVERN_TARGET_EFFECTS_STATE_DRAW    = 1,
    MINE_CAVERN_TARGET_EFFECTS_STATE_RELEASE = 2
};

/// Cavern target-effects controller states: spawn, draw/update and release.
///
/// All three Task* callbacks are copied by value, then indexed by Task::state
/// without a bounds check or terminator. Requires state 0..2 and loaded cavern
/// code; attract-demo scene 3 suppresses dispatch. The release handler kills
/// the controller task.
static const TaskFuncTable3 _gMineCavernTargetEffectsStates = {
    .funcs = {
        [MINE_CAVERN_TARGET_EFFECTS_STATE_SPAWN]   = _mineCavernSpawnTargets,
        [MINE_CAVERN_TARGET_EFFECTS_STATE_DRAW]    = _mineCavernTickTargetEffects,
        [MINE_CAVERN_TARGET_EFFECTS_STATE_RELEASE] = taskKill },
};

/// Starts and mixes or stops one destroyed target's sound for the mapped view.
///
/// `targetIndex` is 0..3. Each sound uses the target's composed world position for
/// signed-byte pan and a view-selected attenuation. Start and mix each query pan;
/// unlisted views leave targets 0 and 1 unchanged and stop targets 2 and 3.
/// Only translation is initialized in the temporary coordinate, and only its
/// composed origin is consumed. Requires the cavern's live view and sound bank.
static void _mineCavernUpdateTargetSound(s16 targetIndex)
{
    GfxCoord soundCoord;
    s32      viewId;

    viewId                  = viewGetMappedIndex() & 0xFF;
    soundCoord.parent       = &gGfxViewCoord;
    soundCoord.coord.t[0]   = gMineCavernTargetEffectPoints[targetIndex].vx;
    soundCoord.coord.t[1]   = gMineCavernTargetEffectPoints[targetIndex].vy;
    soundCoord.coord.t[2]   = gMineCavernTargetEffectPoints[targetIndex].vz;
    soundCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&soundCoord);

    switch (targetIndex) {
        case 0:
            switch (viewId) {
                case 2:
                case 6:
                case 7:
                case 8:
                case 9:
                case 10:
                case 11:
                case 12:
                case 13:
                case 14:
                case 15:
                case 16:
                case 17:
                case 20:
                    sndEvtRequestScriptStop(SOUND_MINE_CAVERN_GLOW_POINT_0, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    break;
                case 3:
                    MINE_CAVERN_START_AND_MIX_TARGET_SOUND(SOUND_MINE_CAVERN_GLOW_POINT_0, &soundCoord, 0x59);
                    break;
                case 4:
                    MINE_CAVERN_START_AND_MIX_TARGET_SOUND(SOUND_MINE_CAVERN_GLOW_POINT_0, &soundCoord, 0x59);
                    break;
                case 5:
                case 25:
                    MINE_CAVERN_START_AND_MIX_TARGET_SOUND(SOUND_MINE_CAVERN_GLOW_POINT_0, &soundCoord, 0x59);
                    break;
                case 18:
                    MINE_CAVERN_START_AND_MIX_TARGET_SOUND(SOUND_MINE_CAVERN_GLOW_POINT_0, &soundCoord, 0x20);
                    break;
                case 19:
                    MINE_CAVERN_START_AND_MIX_TARGET_SOUND(SOUND_MINE_CAVERN_GLOW_POINT_0, &soundCoord, 0xD);
                    break;
                case 21:
                    MINE_CAVERN_START_AND_MIX_TARGET_SOUND(SOUND_MINE_CAVERN_GLOW_POINT_0, &soundCoord, 0x33);
                    break;
                case 22:
                    MINE_CAVERN_START_AND_MIX_TARGET_SOUND(SOUND_MINE_CAVERN_GLOW_POINT_0, &soundCoord, 0x33);
                    break;
                case 23:
                case 24:
                    MINE_CAVERN_START_AND_MIX_TARGET_SOUND(SOUND_MINE_CAVERN_GLOW_POINT_0, &soundCoord, 0x40);
                    break;
            }
            break;
        case 1:
            switch (viewId) {
                case 2:
                    MINE_CAVERN_START_AND_MIX_TARGET_SOUND(SOUND_MINE_CAVERN_GLOW_POINT_1, &soundCoord, 0x40);
                    break;
                case 3:
                    MINE_CAVERN_START_AND_MIX_TARGET_SOUND(SOUND_MINE_CAVERN_GLOW_POINT_1, &soundCoord, 0x33);
                    break;
                case 4:
                    MINE_CAVERN_START_AND_MIX_TARGET_SOUND(SOUND_MINE_CAVERN_GLOW_POINT_1, &soundCoord, 0x20);
                    break;
                case 20:
                    MINE_CAVERN_START_AND_MIX_TARGET_SOUND(SOUND_MINE_CAVERN_GLOW_POINT_1, &soundCoord, 0x33);
                    break;
                case 22:
                    MINE_CAVERN_START_AND_MIX_TARGET_SOUND(SOUND_MINE_CAVERN_GLOW_POINT_1, &soundCoord, 0x59);
                    break;
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                case 10:
                case 11:
                case 12:
                case 13:
                case 14:
                case 15:
                case 16:
                case 17:
                case 18:
                case 19:
                case 21:
                case 23:
                case 24:
                case 25:
                    sndEvtRequestScriptStop(SOUND_MINE_CAVERN_GLOW_POINT_1, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    break;
            }
            break;
        case 2:
            switch (viewId) {
                case 5:
                case 25:
                    MINE_CAVERN_START_AND_MIX_TARGET_SOUND(SOUND_MINE_CAVERN_GLOW_POINT_2, &soundCoord, 0x33);
                    break;
                case 6:
                    MINE_CAVERN_START_AND_MIX_TARGET_SOUND(SOUND_MINE_CAVERN_GLOW_POINT_2, &soundCoord, 0x33);
                    break;
                case 7:
                    MINE_CAVERN_START_AND_MIX_TARGET_SOUND(SOUND_MINE_CAVERN_GLOW_POINT_2, &soundCoord, 0x46);
                    break;
                case 14:
                case 15:
                    MINE_CAVERN_START_AND_MIX_TARGET_SOUND(SOUND_MINE_CAVERN_GLOW_POINT_2, &soundCoord, 0x20);
                    break;
                case 16:
                    MINE_CAVERN_START_AND_MIX_TARGET_SOUND(SOUND_MINE_CAVERN_GLOW_POINT_2, &soundCoord, 0x46);
                    break;
                case 17:
                    MINE_CAVERN_START_AND_MIX_TARGET_SOUND(SOUND_MINE_CAVERN_GLOW_POINT_2, &soundCoord, 0x20);
                    break;
                case 8:
                case 21:
                    MINE_CAVERN_START_AND_MIX_TARGET_SOUND(SOUND_MINE_CAVERN_GLOW_POINT_2, &soundCoord, 0x59);
                    break;
                case 23:
                case 24:
                    MINE_CAVERN_START_AND_MIX_TARGET_SOUND(SOUND_MINE_CAVERN_GLOW_POINT_2, &soundCoord, 0x40);
                    break;
                case 2:
                case 3:
                case 4:
                case 9:
                case 10:
                case 11:
                case 12:
                case 13:
                case 18:
                case 19:
                case 20:
                case 22:
                default:
                    sndEvtRequestScriptStop(SOUND_MINE_CAVERN_GLOW_POINT_2, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    break;
            }
            break;
        case 3:
            switch (viewId) {
                case 2:
                    MINE_CAVERN_START_AND_MIX_TARGET_SOUND(SOUND_MINE_CAVERN_GLOW_POINT_3, &soundCoord, 0x40);
                    break;
                case 7:
                    MINE_CAVERN_START_AND_MIX_TARGET_SOUND(SOUND_MINE_CAVERN_GLOW_POINT_3, &soundCoord, 0x33);
                    break;
                case 8:
                    MINE_CAVERN_START_AND_MIX_TARGET_SOUND(SOUND_MINE_CAVERN_GLOW_POINT_3, &soundCoord, 0x20);
                    break;
                case 6:
                case 20:
                    MINE_CAVERN_START_AND_MIX_TARGET_SOUND(SOUND_MINE_CAVERN_GLOW_POINT_3, &soundCoord, 0x46);
                    break;
                case 3:
                case 4:
                case 5:
                case 9:
                case 10:
                case 11:
                case 12:
                case 13:
                case 14:
                case 15:
                case 16:
                case 17:
                case 18:
                case 19:
                case 21:
                case 22:
                case 23:
                case 24:
                case 25:
                default:
                    sndEvtRequestScriptStop(SOUND_MINE_CAVERN_GLOW_POINT_3, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    break;
            }
            break;
    }
}

/// Spawns the cavern's four target spots and enters the effects state.
///
/// Creates an intact enemy only for an unset destroyed bit, and a remains task
/// for every spot. Descriptor indices 0 and 1 select those two models; the spot
/// index 0..3 is their spawn argument. Spawn failure is not retried here.
/// Requires a live state-0 controller and loaded cavern target resources.
static void _mineCavernSpawnTargets(Task* task)
{
    enum { MINE_CAVERN_TARGET_DESC_INTACT  = 0,
           MINE_CAVERN_TARGET_DESC_REMAINS = 1 };

    s16 targetIndex;
    s32 destroyedMask;

    destroyedMask = gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_TARGETS_DESTROYED);
    for (targetIndex = 0; targetIndex < ARRAY_SIZE(gMineCavernTargetModelPositions); targetIndex++) {
        if (!((destroyedMask >> targetIndex) & 1)) {
            enemySpawnFromTable(gMineCavernTargetTaskDescs, MINE_CAVERN_TARGET_DESC_INTACT, targetIndex, NULL);
        }
        enemySpawnFromTable(gMineCavernTargetTaskDescs, MINE_CAVERN_TARGET_DESC_REMAINS, targetIndex, NULL);
    }
    task->state++;
}

/// Draws one frame of cavern darkness and target effects for controller state 1.
///
/// Borrows the live frame arena and cavern resources; the task argument is
/// unused. Does not advance the state or release the controller.
static void _mineCavernTickTargetEffects(Task* unusedTask)
{
    _mineCavernDrawDarknessAndEffects();
}

void mineCavernTargetEffectsTask(Task* task)
{
    enum { MINE_CAVERN_TARGETS_DISABLED_DEMO = 3 };

    TaskFuncTable3 handlers;

    handlers = _gMineCavernTargetEffectsStates;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != MINE_CAVERN_TARGETS_DISABLED_DEMO) {
        handlers.funcs[task->state](task);
    }
}

/// Places an intact cavern target and publishes its collision and lock-on bodies.
///
/// State 0 requires a live enemy/model and a low unsigned spawn-selector half
/// in 0..3. Owns zeroed work whose matrices outlast model drawing; allocation
/// failure destroys the enemy. Initializes a 256-unit attack-taking sphere
/// centred at local Y = -800 and a 3000-unit blast sphere with pairing disabled,
/// then enters the hit-check state. Normal teardown must unlink both bodies
/// before freeing work; the exit callback alone unlinks only the target body.
/// The original blast X/Y offset load reads an unsigned halfword at address 2;
/// its runtime value and purpose remain unproven and that access is retained.
static void _mineCavernTargetSpawn(Enemy* enemy, Task* task)
{
    enum { MINE_CAVERN_TARGET_CENTER_Y     = -800,
           MINE_CAVERN_TARGET_BODY_RADIUS  = 256,
           MINE_CAVERN_TARGET_BLAST_RADIUS = 3000,
           MINE_CAVERN_TARGET_BODY_KEY     = DAMAGE_HAZARD_CATEGORY,
           MINE_CAVERN_TARGET_BLAST_KEY    = 0x22121 };

    _MineCavernTargetWork* allocation;
    _MineCavernTargetWork* work;
    WorldCollisionBody*    body;
    WorldCollisionBody*    blast;
    u16                    blastOffsetBits;
    VECTOR                 worldPosition;

    allocation = memCalloc(sizeof(*allocation), false);
    work       = allocation;
    task->work = allocation;
    if (allocation == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback                    = _mineCavernTargetExitCallback;
    task->extra.tmd->coords->parent       = &gGfxViewCoord;
    task->extra.tmd->flags                = 0;
    task->extra.tmd->lightMtx             = &work->light;
    task->extra.tmd->colorMtx             = &work->color;
    task->extra.tmd->coords->coord.t[0]   = gMineCavernTargetModelPositions[(u16)task->spawnArg1.value].vx;
    task->extra.tmd->coords->coord.t[1]   = gMineCavernTargetModelPositions[(u16)task->spawnArg1.value].vy;
    task->extra.tmd->coords->coord.t[2]   = gMineCavernTargetModelPositions[(u16)task->spawnArg1.value].vz;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    body                                  = &work->body;
    body->coord                           = task->extra.tmd->coords;
    body->context.contacts                = work->bodyContacts;
    body->pos.vx                          = 0;
    body->pos.vy                          = MINE_CAVERN_TARGET_CENTER_Y;
    body->pos.vz                          = 0;
    body->key                             = MINE_CAVERN_TARGET_BODY_KEY;
    body->radius                          = MINE_CAVERN_TARGET_BODY_RADIUS;
    body->flags                           = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, body);
    body->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(body->context.contacts, ARRAY_SIZE(work->bodyContacts), 0);
    work->body.flags       |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    blast                   = &work->blast;
    blast->coord            = task->extra.tmd->coords;
    blast->context.contacts = work->blastContacts;
    // Retain the original address-2 load; no source object is established.
    blastOffsetBits = ((SVECTOR*)NULL)->vy;
    blast->pos.vz   = 0;
    blast->radius   = MINE_CAVERN_TARGET_BLAST_RADIUS;
    blast->flags    = WORLD_COLLISION_BODY_SPHERE;
    blast->pos.vy   = blastOffsetBits;
    blast->pos.vx   = blastOffsetBits;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, blast);
    worldCollisionInitContacts(blast->context.contacts, ARRAY_SIZE(work->blastContacts), 0);
    work->blast.key    = MINE_CAVERN_TARGET_BLAST_KEY;
    work->blast.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    enemy->hp          = gMineCavernTargetParams.hpMax;
    enemy->param       = &gMineCavernTargetParams;
    MINE_CAVERN_TARGET_UPDATE_LIGHTING(task, worldPosition);
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = MINE_CAVERN_TARGET_CENTER_Y;
    enemy->bodyPos.vz = 0;
    enemy->coord      = task->extra.tmd->coords;
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    task->state++;
}

#include "../../shared/room/room_find_attack_contact.inc.c"

/// Updates the intact target's lock-on eligibility, lighting and first weapon hit.
///
/// State 1 requires initialized target work, a live player/model and spawn spot
/// 0..3. Hidden actor control hides the target; paused control leaves it intact.
/// Lock-on also requires engaged battle, an XZ distance at most 6000 coordinate
/// units and location variant 1 or 4. Hits require those variants and a class-2
/// contact without the attachment-table bit, independently of lock-on eligibility.
/// The weapon row must index the room's 36-entry damage table. Death records the
/// spot's destruction bit, queues its positional sound and enters body retirement.
/// Both contact tables clear after each active check; scratch storage is released.
static void _mineCavernTargetCheckHit(Enemy* enemy, Task* task)
{
    enum { MINE_CAVERN_TARGET_LOCK_ON_RADIUS       = 6000,
           MINE_CAVERN_TARGET_HIT_ATTACHMENT_BIT   = 0x8000,
           MINE_CAVERN_TARGET_WEAPON_ROW_MASK      = 0x7F,
           MINE_CAVERN_TARGET_DESTROY_SOUND        = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_CAVERN, 0x14),
           MINE_CAVERN_TARGET_SOUND_INSTANCE_SHIFT = 8 };

    _MineCavernTargetWork*       work;
    Task*                        playerTask;
    _MineCavernTargetHitScratch* scratchTop;
    _MineCavernTargetHitScratch* hitScratch;
    GfxCoord*                    rootCoord;
    SVECTOR*                     playerOffset;
    s16                          relativeHitBearing;
    u32                          hitKey;
    s32                          soundId;
    s32                          panOffset;

    work       = task->work;
    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            return;
    }

    // Stage the player offset before publishing the reserved scratch block.
    rootCoord                                         = task->extra.tmd->coords;
    scratchTop                                        = SCRATCH_STACK_CURSOR(_MineCavernTargetHitScratch);
    scratchTop[-1].offset.vx                          = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
    playerOffset                                      = &scratchTop[-1].offset;
    playerOffset->vy                                  = gPlayerStatus.coordMtx->t[1] - rootCoord->coord.t[1];
    SCRATCH_STACK_CURSOR(_MineCavernTargetHitScratch) = scratchTop - 1;
    playerOffset->vz                                  = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
    hitScratch                                        = scratchTop - 1;

    if (_actorRangeOutsideRadiusXZ(playerOffset, MINE_CAVERN_TARGET_LOCK_ON_RADIUS) || gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_ENGAGED ||
        (gGameSession->location.variant != gSceneCombatState.signals.bytes.battlePhase && gGameSession->location.variant != 4)) {
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    } else {
        enemy->node.state.parts.flags = 0;
    }

    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    MINE_CAVERN_TARGET_UPDATE_LIGHTING(task, hitScratch->vec);
    task->extra.tmd->flags = 0;

    hitKey             = _roomFindAttackContact(&hitScratch->offset, work->bodyContacts, ARRAY_SIZE(work->bodyContacts));
    hitScratch->hitKey = hitKey;
    if (hitKey & MINE_CAVERN_TARGET_HIT_ATTACHMENT_BIT) {
        hitScratch->hitKey = 0;
    }
    if (gGameSession->location.variant != 1 && gGameSession->location.variant != 4) {
        hitScratch->hitKey = 0;
    }

    if (hitScratch->hitKey != 0) {
        hitScratch->offset.vx -= task->extra.tmd->coords->workm.t[0];
        hitScratch->offset.vy -= task->extra.tmd->coords->workm.t[1];
        hitScratch->offset.vz -= task->extra.tmd->coords->workm.t[2];
        relativeHitBearing = hitScratch->hitBearing = ratan2(hitScratch->offset.vx, hitScratch->offset.vz) - ratan2(-task->extra.tmd->coords->workm.m[2][0],
                                                                                                                    task->extra.tmd->coords->workm.m[2][2]);
        hitScratch->hitBearing                      = _actorAngleNormalizeYaw(relativeHitBearing);
        hitScratch->vec.vx                          = playerTask->extra.tmd->coords->coord.t[0] - task->extra.tmd->coords->coord.t[0];
        hitScratch->vec.vy                          = playerTask->extra.tmd->coords->coord.t[1] - task->extra.tmd->coords->coord.t[1];
        hitScratch->vec.vz                          = playerTask->extra.tmd->coords->coord.t[2] - task->extra.tmd->coords->coord.t[2];
        hitScratch->playerDistance                  = SquareRoot0(hitScratch->vec.vx * hitScratch->vec.vx + hitScratch->vec.vy * hitScratch->vec.vy + hitScratch->vec.vz * hitScratch->vec.vz);
        // Keep the damage roll's side effects before replacing its result with target damage.
        hitScratch->damage = damageComputePlayerAttack(hitScratch->hitKey, hitScratch->playerDistance, 0, 0);
        hitScratch->damage = gMineCavernTargetDamageByAttackRow[hitScratch->hitKey & MINE_CAVERN_TARGET_WEAPON_ROW_MASK];
        enemy->hp         -= hitScratch->damage;
        worldTargetAddReadoutAmount(&enemy->node, hitScratch->damage, 0);
        if (enemy->hp <= 0) {
            hitScratch->destroyedTargets = gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_TARGETS_DESTROYED);
            if (!((hitScratch->destroyedTargets >> (u16)task->spawnArg1.value) & 1)) {
                hitScratch->destroyedTargets |= 1 << (u16)task->spawnArg1.value;
                gameFlagSetNibble(GAME_FLAG_MINE_CAVERN_TARGETS_DESTROYED, hitScratch->destroyedTargets);
                task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            soundId   = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << MINE_CAVERN_TARGET_SOUND_INSTANCE_SHIFT) | MINE_CAVERN_TARGET_DESTROY_SOUND;
            panOffset = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, panOffset, (s8)(worldCoordGetOriginAudioDepth(task->extra.tmd->coords) / 2));
            task->state++;
        }
    }
    worldCollisionClearContacts(work->bodyContacts);
    worldCollisionClearContacts(work->blastContacts);
    SCRATCH_STACK_RELEASE_BLOCK(_MineCavernTargetHitScratch);
}

#undef MINE_CAVERN_TARGET_UPDATE_LIGHTING

/// Allocates and places the target's remains model before its visibility tick.
///
/// State 0 of `mineCavernTargetRemainsTask`. The low unsigned half of
/// `spawnArg1.value` selects one of the four target spots (0..3). Requires a
/// live enemy and model, room lighting and view state. The task owns the zeroed
/// work block; its matrices supply the model's lighting until teardown.
/// Allocation failure destroys the enemy; success advances to state 1.
static void _mineCavernTargetRemainsSpawn(Enemy* enemy, Task* task)
{
    _MineCavernTargetWork* allocation;
    _MineCavernTargetWork* work;
    VECTOR                 worldPosition;

    allocation = memCalloc(sizeof(*allocation), false);
    work       = allocation;
    task->work = allocation;
    if (allocation == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->extra.tmd->coords->parent       = &gGfxViewCoord;
    task->extra.tmd->flags                = 0;
    task->extra.tmd->lightMtx             = &work->light;
    task->extra.tmd->colorMtx             = &work->color;
    task->extra.tmd->coords->coord.t[0]   = gMineCavernTargetModelPositions[(u16)task->spawnArg1.value].vx;
    task->extra.tmd->coords->coord.t[1]   = gMineCavernTargetModelPositions[(u16)task->spawnArg1.value].vy;
    task->extra.tmd->coords->coord.t[2]   = gMineCavernTargetModelPositions[(u16)task->spawnArg1.value].vz;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
    worldPosition.vx = task->extra.tmd->coords->workm.t[0];
    worldPosition.vy = task->extra.tmd->coords->workm.t[1];
    worldPosition.vz = task->extra.tmd->coords->workm.t[2];
    worldCoordSetModelLighting(task->extra.tmd, &worldPosition, 0, 3);
    task->state++;
}

/// Unlinks the intact target's attack-taking body when its exit callback is dispatched.
///
/// `task` must be live; its work may be NULL, otherwise it is a live target
/// work block. This callback releases no storage. Before that block is freed,
/// its blast body must be unlinked or the collision lists must be discarded.
static void _mineCavernTargetExitCallback(Task* task)
{
    _MineCavernTargetWork* work;

    work = task->work;
    if (work != NULL) {
        worldCollisionUnlinkBody(&work->body);
    }
}

/// Stops target hits and lock-on, then starts the explosion sequence at tick zero.
///
/// State 2 of the intact target task. `enemy` and the task's target work must
/// remain live. The blast body stays linked for the explosion to retire.
static void _mineCavernTargetRetireBody(Enemy* enemy, Task* task)
{
    _MineCavernTargetWork* work;

    work                          = task->work;
    work->body.flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    worldCollisionUnlinkBody(&work->body);
    work->frame = 0;
    task->state++;
}

/// Diagnostic line printed at tick 0 before the target's initial explosion.
///
/// A NUL-terminated format with no conversion fields, resident for the print.
static const char _gMineCavernTargetInitialBlastLog[] = "BOMB1\n";

/// Diagnostic line printed at tick 1 before the target's second explosion.
///
/// A NUL-terminated format with no conversion fields, resident for the print.
static const char _gMineCavernTargetSecondBlastLog[] = "BOMB2\n";

/// Intact target states: 0 spawn, 1 hit check, 2 retire body, 3 explode, 4 destroy.
///
/// The dispatcher copies all five two-argument callbacks and indexes directly
/// by `Task::state`, without bounds checking or a terminator. Keep the cavern
/// loaded while an intact target can dispatch them.
static const EnemyTaskFuncTable5 _gMineCavernTargetStates = {
    {
        _mineCavernTargetSpawn,
        _mineCavernTargetCheckHit,
        _mineCavernTargetRetireBody,
        _mineCavernTargetExplode,
        enemyDestroy,
    },
};

/// State handlers of the cavern target remains: 0 spawn, 1 reveal/update, 2 destroy.
///
/// Indexed directly by Task::state with no bounds check or terminator. The
/// dispatcher copies all three two-argument enemy callbacks by value; the
/// overlay must stay loaded while a remains task can dispatch them.
static const EnemyTaskFuncTable3 _gMineCavernTargetRemainsStates = {
    { _mineCavernTargetRemainsSpawn, _mineCavernTargetRemainsTick, enemyDestroy },
};

/// Runs the destroyed target's 60-tick explosion, then advances to teardown.
///
/// State 3 of the intact target. Hides the model, emits blasts on ticks 0, 1,
/// 2 and 4, clears blast pairing on 3 and 5, and unlinks the blast body on 9.
/// The second blast decreases local Y by ten units per tick. Target work and
/// model coordinates must remain live; `enemy` is an unused dispatch argument.
/// The blast is already unlinked when state 4 releases the enemy.
static void _mineCavernTargetExplode(Enemy* enemy, Task* task)
{
    // Low 12 bits set size; bits 12..15 set period; the high byte selects random drift.
    enum { MINE_CAVERN_TARGET_INITIAL_BLAST       = 0x01001200,
           MINE_CAVERN_TARGET_SECOND_BLAST        = 0x01000580,
           MINE_CAVERN_TARGET_REPEAT_BLAST        = 0x01002500,
           MINE_CAVERN_TARGET_BLAST_UNLINK_TICK   = 9,
           MINE_CAVERN_TARGET_EXPLOSION_LAST_TICK = 59 };

    _MineCavernTargetWork* work;
    EffectWork*            explosion;
    u16                    tick;

    work = task->work;

    task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;

    tick        = work->frame;
    work->frame = tick + 1;

    switch ((s16)tick) {
        case 0:
            printf(_gMineCavernTargetInitialBlastLog);
            task->extra.tmd->coords->coord.t[1]   = -0x258;
            task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(task->extra.tmd->coords);
            effectSpawn(EFFECT_EXPLOSION, task->extra.tmd->coords, MINE_CAVERN_TARGET_INITIAL_BLAST, NULL);
            return;

        case 1:
            printf(_gMineCavernTargetSecondBlastLog);
            explosion = effectSpawn(EFFECT_EXPLOSION, task->extra.tmd->coords, MINE_CAVERN_TARGET_SECOND_BLAST, NULL);
            if (explosion != NULL) {
                explosion->move.vx = 0;
                explosion->move.vy = -0xA;
                explosion->move.vz = 0;
            }
            return;

        case 2:
        case 4:
            effectSpawn(EFFECT_EXPLOSION, task->extra.tmd->coords, MINE_CAVERN_TARGET_REPEAT_BLAST, NULL);
            return;

        case 3:
        case 5:
            work->blast.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            return;

        case MINE_CAVERN_TARGET_BLAST_UNLINK_TICK:
            worldCollisionUnlinkBody(&work->blast);
            return;

        case MINE_CAVERN_TARGET_EXPLOSION_LAST_TICK:
            task->state++;
            break;

        default:
            return;
    }
}

void mineCavernTargetTask(Task* task)
{
    EnemyTaskFuncTable5 handlers;

    handlers = _gMineCavernTargetStates;
    handlers.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Lights the target remains and reveals them once that target is destroyed.
///
/// State 1 of `mineCavernTargetRemainsTask`; enemy is an unused dispatch argument.
/// Requires initialized target work, model and view/lighting state, with the low
/// unsigned spawn-selector half in 0..3. A clear destruction bit suppresses
/// active drawing. A set bit rebuilds the model-local centre at Y = -800 whole
/// coordinate units, increments the unsigned-halfword frame counter and enables
/// drawing. The task keeps this state until external teardown.
static void _mineCavernTargetRemainsTick(Enemy* enemy, Task* task)
{
    enum { MINE_CAVERN_TARGET_REMAINS_CENTER_Y = -800 };
    _MineCavernTargetWork* work;
    VECTOR                 worldPosition;
    // Retain the original frame's unused eight-byte vector slot.
    SVECTOR unusedStackVector;

    work = task->work;

    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
    worldPosition.vx = task->extra.tmd->coords->workm.t[0];
    worldPosition.vy = task->extra.tmd->coords->workm.t[1];
    worldPosition.vz = task->extra.tmd->coords->workm.t[2];
    worldCoordSetModelLighting(task->extra.tmd, &worldPosition, 0, 3);

    if (!((gameFlagGetNibble(GAME_FLAG_MINE_CAVERN_TARGETS_DESTROYED) >> (u16)task->spawnArg1.value) & 1)) {
        task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        gfxSetRotIdentity(&work->centerCoord.coord);
        work->centerCoord.parent       = task->extra.tmd->coords;
        work->centerCoord.coord.t[2]   = 0;
        work->centerCoord.coord.t[0]   = 0;
        work->centerCoord.coord.t[1]   = MINE_CAVERN_TARGET_REMAINS_CENTER_Y;
        work->centerCoord.composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&work->centerCoord);
        work->frame++;
        task->extra.tmd->flags = 0;
    }
}

void mineCavernTargetRemainsTask(Task* task)
{
    EnemyTaskFuncTable3 handlers;

    handlers = _gMineCavernTargetRemainsStates;
    handlers.funcs[task->state](task->spawnArg2.pointer, task);
}
