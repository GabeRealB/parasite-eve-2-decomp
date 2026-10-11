#include "rooms/acropolis_sanctuary.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_210700.h"

#include "actors/task_tables.h"

#include "gameplay/companion_load.h"
#include "gameplay/captions.h"
#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_akropolis.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/actor_contacts.h"

static WorldCollisionTrigger _gAcropolisSanctuaryActionTriggers[17];

/// Views selected by player placement and the mosaic's retained cleanup latch.
enum {
    ACROPOLIS_SANCTUARY_VIEW_MOSAIC_LATCH_SET = 16,
};

/// Packed shard arguments, projection resources and mosaic motion limits.
enum {
    ACROPOLIS_SANCTUARY_MOSAIC_MESSAGE_START     = 3101,
    ACROPOLIS_SANCTUARY_MOSAIC_MIN_DEPTH         = 17,
    ACROPOLIS_SANCTUARY_MOSAIC_TEXTURE_PAGE      = getTPage(1, GPU_BLEND_AVERAGE, 768, 0),
    ACROPOLIS_SANCTUARY_MOSAIC_PALETTE           = getClut(0, 264),
    ACROPOLIS_SANCTUARY_MOSAIC_GRAVITY           = 3,
    ACROPOLIS_SANCTUARY_MOSAIC_DRIFT_FRAMES      = 60,
    ACROPOLIS_SANCTUARY_MOSAIC_SPLIT_PERIOD      = 60U,
    ACROPOLIS_SANCTUARY_MOSAIC_SPLIT_HEIGHT      = -3071,
    ACROPOLIS_SANCTUARY_MOSAIC_BOUNCE_X          = -10048,
    ACROPOLIS_SANCTUARY_MOSAIC_BOUNCE_Y          = -3799,
    ACROPOLIS_SANCTUARY_MOSAIC_CLEAR_X           = -10432,
    ACROPOLIS_SANCTUARY_MOSAIC_SHARD_DRIFT_SHIFT = 12,
    ACROPOLIS_SANCTUARY_MOSAIC_SHARD_AIRBORNE    = 1 << ACROPOLIS_SANCTUARY_MOSAIC_SHARD_DRIFT_SHIFT,
};

/// Script requests are distinct from the cutscene work's stored phases.
enum {
    ACROPOLIS_SANCTUARY_CUTSCENE_CUE_HOLD         = 0,
    ACROPOLIS_SANCTUARY_CUTSCENE_CUE_PLACE_PLAYER = 1,
    ACROPOLIS_SANCTUARY_CUTSCENE_CUE_CHANGE_SOUND = 2,
};

/// Script-selected phases and placement status of the sanctuary cutscene.
enum {
    ACROPOLIS_SANCTUARY_CUTSCENE_PHASE_INITIAL      = 0,
    ACROPOLIS_SANCTUARY_CUTSCENE_PHASE_HOLD         = 1,
    ACROPOLIS_SANCTUARY_CUTSCENE_PHASE_PLACE_PLAYER = 2,
    ACROPOLIS_SANCTUARY_CUTSCENE_PLACEMENT_PENDING  = 0,
    ACROPOLIS_SANCTUARY_CUTSCENE_PLACEMENT_APPLIED  = 1,
};

/// Task-owned state for the sanctuary cutscene's script-cued player placement.
///
/// Stored in `Task::work` on the primary heap and freed by default task teardown.
/// Script requests require the initialized cutscene task to remain live; the
/// borrowed player task must remain live through its animation and placement
/// messages. Each placement request resets the one-shot status.
typedef struct {
    Task* playerTask;       // Borrowed player task captured at creation
    u16   phase;            // Script phase (0 initial, 1 hold, 2 place player)
    u16   placementApplied; // Player setup status (0 pending, 1 applied)
    u8    field_8[4];       // Cleared at creation; role and field boundaries unproven
} _AcropolisSanctuaryCutsceneWork;
STATIC_ASSERT_SIZEOF(_AcropolisSanctuaryCutsceneWork, 0xC);

/// One tile of the sanctuary's mosaic: the piece of the mosaic sheet it shows
/// and how it comes away from the wall.
///
/// The sheet is 180 x 256 texels, covered without gaps by 15 x 16 texel cells
/// and 30 x 32 blocks of four. A tile's texel origin is also its place on the
/// wall, a texel being 2147/256 world units wide and 1145/128 tall, so the
/// mosaic is laid out exactly as it is drawn on the sheet. The middle of the
/// mosaic is thrown clear at once; the tiles around it stay in place longer the
/// further out they sit, then work loose and crumble.
typedef struct {
    s16 extentU;    // Texels from `originU` to the tile's last column (14 or 29)
    s16 extentV;    // Texels from `originV` to the tile's last row (15 or 31)
    s16 originU;    // Left texel column on the mosaic sheet, 0..165
    s16 originV;    // Top texel row on the mosaic sheet, 0..240
    s16 thrown;     // How the tile leaves (0 works loose slowly and sheds shards, 1 thrown clear)
    s16 holdFrames; // Frames the tile stays in place before it moves; 0 when thrown
    s16 sizeClass;  // Tile size (0 one 15 x 16 texel cell, 1 a 30 x 32 block)
} _AcropolisSanctuaryMosaicTile;
STATIC_ASSERT_SIZEOF(_AcropolisSanctuaryMosaicTile, 0xE);

/// Scratch-stack block one mosaic shard's triangle is drawn from.
///
/// Each corner is staged in `corners` as one of a mosaic tile's corner offsets,
/// in the shard's own frame, and replaced in place twice: first by that offset
/// scaled to the shard's size, then by the corner's world position, narrowed to
/// signed 16-bit coordinate units. One RTPT then projects the three together;
/// the screen positions go straight into the packet, so the block keeps none of
/// them.
///
/// The block is one word longer than the words the drawer uses, and the room's
/// whole-tile block carries the same spare word after its four corners.
///
/// Reserve the whole block and release it before the drawer returns; no pointer
/// into it survives release.
typedef struct {
    s32     otz;         // SZ3 / 4 of the projection, a quarter of the last corner's depth; draw threshold and ordering-table depth
    SVECTOR corners[3];  // Local corner workspace, then the world positions supplied to the projection
    u8      field_1C[4]; // Reserved with the block but never read or written; role and field boundaries unproven
} _AcropolisSanctuaryMosaicShardScratch;
STATIC_ASSERT_SIZEOF(_AcropolisSanctuaryMosaicShardScratch, 0x20);

/// Scratch-stack block one whole mosaic tile's quad is drawn from.
///
/// Each corner is staged in `corners` as one of the tile's size-class corner
/// offsets, in the tile's own frame, and replaced in place by the corner's
/// world position, narrowed to signed 16-bit coordinate units. An RTPS for the
/// first corner and one RTPT for the other three then project them; the screen
/// positions go straight into the packet, so the block keeps none of them.
/// Unlike `_AcropolisSanctuaryMosaicShardScratch` the corners are never scaled:
/// an intact tile is always drawn at its size class's own dimensions.
///
/// The block is one word longer than the words the drawer uses, as the shard
/// block is after its three corners.
///
/// Reserve the whole block and release it before the drawer returns; no pointer
/// into it survives release.
typedef struct {
    s32     otz;         // SZ3 / 4 of the projection, a quarter of the last corner's depth; draw threshold and ordering-table depth
    SVECTOR corners[4];  // Local corner workspace, then the world positions supplied to the projection
    u8      field_24[4]; // Reserved with the block but never read or written; role and field boundaries unproven
} _AcropolisSanctuaryMosaicTileScratch;
STATIC_ASSERT_SIZEOF(_AcropolisSanctuaryMosaicTileScratch, 0x28);

/// Grey levels of the sanctuary flame sprite, one byte per variant.
///
/// Bits 8..9 of the flame's spawn argument select the variant. That bitfield
/// is two bits wide, but only variants 0, 1 and 2 have a byte here. Variants
/// 0 and 1 store the same level and variant 2 is dimmer; the twelve flames
/// this room places select 0 and 2. The overlay keeps two tables, the resting
/// grey and the amount added on odd frames. The drawer copies a table and
/// then reads one byte, so the padding after the table is not a fourth level.
typedef struct {
    u8 grey[3]; // 0..255, written to R, G and B; variants 0 and 1 bright, 2 dim
} _AcropolisSanctuaryFlameGrey;
STATIC_ASSERT_SIZEOF(_AcropolisSanctuaryFlameGrey, 3);

/// `gPlayerStatus.weapon` is the
/// equipped-weapon index the slot-3 msg 0x3E8 record is keyed on,
/// `gDisplayState.pendingMode` and `gAttachmentState.mode` gate the cutscene task's setup (the latter is 1 while the attachment wheel is open) and `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId` picks which of the two
/// weapon-id bases that record uses. `gDisplayState.spriteVariant` is set to 1 alongside the
/// save writes when the task hands off to task 0x11, the same way the fountain
/// and helicopter-pad rooms set it.

static WorldCollisionGrid _gAcropolisSanctuaryCollisionGrid;

/// Whole-unit X/Y/Z displacement left by the last call of
/// `_actorContactApplyGridPushback`.
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

static void _acropolisSanctuaryUpdateRoomProgress(Task* unusedTask);
static void _acropolisSanctuaryInitRoomTask(Task* task);
static void _acropolisSanctuaryPlaceBlockerCollision(void);
static void _acropolisSanctuarySelectViewSpriteBatch(s32 useSecondBatch, s32 viewId);

/// Key-item use message handled by this room's task.
enum { ACROPOLIS_SANCTUARY_MESSAGE_USE_KEY_ITEM = 0x13F1 };

/// Unsigned byte fields in a script's packed view-sprite selection.
enum {
    ACROPOLIS_SANCTUARY_SCRIPT_SPRITE_FIELD_MASK  = 0xFF,
    ACROPOLIS_SANCTUARY_SCRIPT_SPRITE_BATCH_SHIFT = 8,
};

/// Sets a flame billboard's square screen bounds around its projected centre.
///
/// Borrows a writable `flameQuad` and read-only `flameScratch` for this call.
/// Initialize `screenPos` and a nonnegative `halfExtent` in pixels; their sums
/// and differences must fit signed 32-bit arithmetic. Each edge narrows to a
/// signed 16-bit coordinate before its paired stores. Vertex indices 0..3 are
/// top-left, top-right, bottom-left and bottom-right. Writes only X/Y coordinates.
static inline void _acropolisSanctuarySetFlameQuadBounds(POLY_FT4* flameQuad, const RoomGlowSpriteScratch* flameScratch)
{
    s16 left;
    s16 right;
    s16 top;
    s16 bottom;

    left          = flameScratch->screenPos.vx - flameScratch->halfExtent;
    flameQuad->x0 = flameQuad->x2 = left;

    right         = flameScratch->screenPos.vx + flameScratch->halfExtent;
    flameQuad->x1 = flameQuad->x3 = right;

    top           = flameScratch->screenPos.vy - flameScratch->halfExtent;
    flameQuad->y0 = flameQuad->y1 = top;

    bottom        = flameScratch->screenPos.vy + flameScratch->halfExtent;
    flameQuad->y2 = flameQuad->y3 = bottom;
}

/// Sanctuary room-task states, copied as a complete table before dispatch.
///
/// Slots 0..2 are initialization, active progress updates and teardown.
/// There is no terminator or bounds check; the overlay must stay loaded.
static const TaskFuncTable3 _gAcropolisSanctuaryRoomTaskStates = {
    .funcs = { _acropolisSanctuaryInitRoomTask, _acropolisSanctuaryUpdateRoomProgress, taskKill },
};

/// Mosaic breakup origin relative to the room-effect task's coordinate.
///
/// XYZ use whole coordinate units. Message dispatch copies all eight bytes
/// locally, and spawning snapshots XYZ; no pointer to this record is retained.
static const SVECTOR _gAcropolisSanctuaryMosaicOffset = { -10230, -6090, -7230, 0 };

/// Resting flame grey for texture/palette variants 0..2.
///
/// The drawer copies exactly three bytes and applies the selected level to RGB.
static const _AcropolisSanctuaryFlameGrey _gAcropolisSanctuaryFlameBaseGrey = { { 0x60, 0x60, 0x10 } };
/// Flame grey added on odd display frames for texture/palette variants 0..2.
///
/// The drawer copies exactly three bytes; variant 3 has no table entry.
static const _AcropolisSanctuaryFlameGrey _gAcropolisSanctuaryFlameFlickerGrey = { { 0x10, 0x10, 0x08 } };
/// Unread byte after the two three-byte flame tables; its role is unproven.
static const u8 D_acropolis_sanctuary_8017D5DF = 0xF1;

static s32 _acropolisSanctuaryStartMosaic(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg);

static void _acropolisSanctuaryCutsceneTask(Task* task);

static void _acropolisSanctuaryRequestCutsceneCue(s32 cue);

static WorldCollisionTrigger _gAcropolisSanctuaryViewBoundaryTriggers[18];
static WorldCoordRoomLights  _gAcropolisSanctuaryRoomLights;

static void _acropolisSanctuaryApplyScriptSpriteSelection(u32 packedSelection);
static void _acropolisSanctuaryRestorePlayerWeaponAnimation(void);

static s32 _acropolisSanctuaryResolvePromenadeExit(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _acropolisSanctuaryRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unused);
static s32 _acropolisSanctuaryHandleRoomCommand(Task* task, s32 messageId, s32 command, s32 unused);
static s32 _acropolisSanctuaryHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

static AnimationPackedPose _gAcropolisSanctuaryAnimation03234Bank1[8] = {
#include "assets/acropolis_sanctuary_animation_03234_bank1.inc"
};

static AnimationPackedRotation _gAcropolisSanctuaryAnimation03234Bank4[115] = {
#include "assets/acropolis_sanctuary_animation_03234_bank4.inc"
};

static AnimationRecord _gAcropolisSanctuaryAnimation03234Records[150] = {
#include "assets/acropolis_sanctuary_animation_03234_records.inc"
};

static u16 _gAcropolisSanctuaryAnimation03234Indices[20] = {
#include "assets/acropolis_sanctuary_animation_03234_indices.inc"
};

static AnimationSet _gAcropolisSanctuaryAnimation03234 = {
    _gAcropolisSanctuaryAnimation03234Records,
    _gAcropolisSanctuaryAnimation03234Indices,
    { NULL, _gAcropolisSanctuaryAnimation03234Bank1, NULL, NULL, _gAcropolisSanctuaryAnimation03234Bank4, NULL, NULL, NULL },
};

/// Message routes borrowed by the sanctuary room task while it is live.
///
/// Resolve takes request/reply room events; command and key-item use take
/// integer selectors; room action borrows a direction-action request. The
/// terminal record makes unsupported IDs return zero.
static const TaskMessageEntry _gAcropolisSanctuaryRoomMessages[] __attribute__((section(".data"))) = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _acropolisSanctuaryResolvePromenadeExit },
    { ROOM_MESSAGE_COMMAND, _acropolisSanctuaryHandleRoomCommand },
    { ACROPOLIS_SANCTUARY_MESSAGE_USE_KEY_ITEM, _acropolisSanctuaryRejectKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _acropolisSanctuaryHandleRoomAction },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Player placement at the start of the sanctuary scene.
///
/// Position (-9700, 0, -7910) uses the model root parent's coordinate frame;
/// yaw is a quarter turn (4096 units/turn). The script borrows this record
/// through synchronous placement dispatch.
static ActorTransform _gAcropolisSanctuaryPlayerSceneStartPlacement = {
    .pos = { -9700, 0, -7910, 0 },
    .rot = { 0, ONE / 4, 0, 0 },
};

/// Shared zero placement for Rupert and the player during the sanctuary scene.
///
/// Each receiver places its model root at its own parent's origin with zero
/// Euler angles. Both scripts borrow the complete record synchronously.
static ActorTransform _gAcropolisSanctuarySceneOriginPlacement = {
    .pos = { 0, 0, 0, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Unread placement-shaped record; its intended receiver and frame are unproven.
static ActorTransform D_acropolis_sanctuary_80180874 = { { 0, 0, -900, 0 }, { 0, -1024, 0, 0 } };

/// Player root placement before sanctuary scene clip 48.
///
/// Position (-100, 0, -400) uses the root parent's coordinate frame; all
/// Euler angles are zero. Placement dispatch borrows this record synchronously.
static ActorTransform _gAcropolisSanctuaryPlayerSceneClip48Placement = {
    .pos = { -100, 0, -400, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Player root placement before sanctuary scene clip 49.
///
/// Position (-100, 0, -400) uses the root parent's coordinate frame; all
/// Euler angles are zero. This separately authored cue repeats clip 48's
/// placement and is borrowed synchronously.
static ActorTransform _gAcropolisSanctuaryPlayerSceneClip49Placement = {
    .pos = { -100, 0, -400, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Rupert's placement restored after the sanctuary scene or on a later room entry.
///
/// Position (0, 0, 480) is in the model root parent's coordinate frame; all
/// Euler angles are zero. Placement messages borrow this record synchronously.
static ActorTransform _gAcropolisSanctuaryRupertRestoredPlacement = {
    .pos = { 0, 0, 480, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Player root placement for the sanctuary scene's closing poses.
///
/// Position (0, 0, -900) uses the root parent's coordinate frame; all
/// Euler angles are zero. Placement dispatch borrows this record synchronously.
static ActorTransform _gAcropolisSanctuaryPlayerSceneClosingPlacement = {
    .pos = { 0, 0, -900, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Player placement after the sanctuary scene, including its skip path.
///
/// Position (-5724, 0, -8276) uses room units and yaw -1024 is a quarter turn.
/// The player placement message borrows this record synchronously.
static ActorTransform _gAcropolisSanctuaryPlayerSceneEndPlacement = {
    .pos = { -5724, 0, -8276, 0 },
    .rot = { 0, -ONE / 4, 0, 0 },
};

/// Resets the player's equipped-weapon clip 1 and disables world collision.
///
/// Event scripts borrow this request synchronously. The interpreter copies it
/// and replaces `source.index` with the equipped weapon's bank before dispatch;
/// the stored selector is retained data, not the bank used for playback.
static AnimationPlayRequest _gAcropolisSanctuaryPlayerWeaponClip1Request = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Nine animation sets copied into the player's sanctuary scene bank extension.
///
/// Entries replace bank slots 47..55: eight borrowed Rupert animation sets,
/// followed by the room's weapon-restoration set. This table supplies nine
/// pointer words; the room and Rupert resources must outlive playback.
static AnimationSet* _gAcropolisSanctuaryPlayerSceneAnimationSets[9] = {
    &gActor210700Animation01E2C,
    &gActor210700Animation027F8,
    &gActor210700Animation02B54,
    &gActor210700Animation02E0C,
    &gActor210700Animation0318C,
    &gActor210700Animation03424,
    &gActor210700Animation03BD0,
    &gActor210700Animation067E8,
    &_gAcropolisSanctuaryAnimation03234,
};

/// Installs the nine sanctuary scene sets in the player's equipped-weapon bank.
///
/// Copies nine pointer words to slots 47..55 without starting playback. The
/// request is borrowed synchronously; the room and Rupert animation resources
/// must remain loaded while those copied pointers are used.
static AnimationBankCopyRequest _gAcropolisSanctuaryPlayerSceneBankExtensionRequest = {
    .source    = { .sets = _gAcropolisSanctuaryPlayerSceneAnimationSets },
    .wordCount = ARRAY_SIZE(_gAcropolisSanctuaryPlayerSceneAnimationSets),
};

/// Resets player sanctuary scene clip 47.
///
/// The script copies this request and resolves the equipped-weapon bank;
/// the stored source selector is retained data. Clip 47 uses scene-extension
/// entry 0. Playback disables world collision; the room and Rupert
/// resources must stay loaded while the borrowed animation runs.
/// The stored blend duration is ignored on reset.
static AnimationPlayRequest _gAcropolisSanctuaryPlayerSceneClip47Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 1,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets player sanctuary scene clip 48.
///
/// The script copies this request and resolves the equipped-weapon bank;
/// the stored source selector is retained data. Clip 48 uses scene-extension
/// entry 1. Playback disables world collision; the room and Rupert
/// resources must stay loaded while the borrowed animation runs.
/// The stored blend duration is ignored on reset.
static AnimationPlayRequest _gAcropolisSanctuaryPlayerSceneClip48Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 1,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets player sanctuary scene clip 49.
///
/// The script copies this request and resolves the equipped-weapon bank;
/// the stored source selector is retained data. Clip 49 uses scene-extension
/// entry 2. Playback disables world collision; the room and Rupert
/// resources must stay loaded while the borrowed animation runs.
/// The stored blend duration is ignored on reset.
static AnimationPlayRequest _gAcropolisSanctuaryPlayerSceneClip49Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 2,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 1,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends over 5 frames into player sanctuary scene clip 50.
///
/// The script copies this request and resolves the equipped-weapon bank;
/// the stored source selector is retained data. Clip 50 uses scene-extension
/// entry 3. Playback disables world collision; the room and Rupert
/// resources must stay loaded while the borrowed animation runs.
static AnimationPlayRequest _gAcropolisSanctuaryPlayerSceneClip50Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 3,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 5,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends over 5 frames into player sanctuary scene clip 51.
///
/// The script copies this request and resolves the equipped-weapon bank;
/// the stored source selector is retained data. Clip 51 uses scene-extension
/// entry 4. Playback disables world collision; the room and Rupert
/// resources must stay loaded while the borrowed animation runs.
static AnimationPlayRequest _gAcropolisSanctuaryPlayerSceneClip51Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 4,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 5,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends over 5 frames into player sanctuary scene clip 52.
///
/// The script copies this request and resolves the equipped-weapon bank;
/// the stored source selector is retained data. Clip 52 uses scene-extension
/// entry 5. Playback disables world collision; the room and Rupert
/// resources must stay loaded while the borrowed animation runs.
static AnimationPlayRequest _gAcropolisSanctuaryPlayerSceneClip52Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 5,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 5,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends over 15 frames into player sanctuary scene clip 52.
///
/// The script copies this request and resolves the equipped-weapon bank;
/// the stored source selector is retained data. Clip 52 uses scene-extension
/// entry 5. Playback disables world collision; the room and Rupert
/// resources must stay loaded while the borrowed animation runs.
static AnimationPlayRequest _gAcropolisSanctuaryPlayerSceneClip52LongBlendRequest = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 5,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 15,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends over 5 frames into player sanctuary scene clip 53.
///
/// The script copies this request and resolves the equipped-weapon bank;
/// the stored source selector is retained data. Clip 53 uses scene-extension
/// entry 6. Playback disables world collision; the room and Rupert
/// resources must stay loaded while the borrowed animation runs.
static AnimationPlayRequest _gAcropolisSanctuaryPlayerSceneClip53Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 6,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 5,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets player sanctuary scene clip 54.
///
/// The script copies this request and resolves the equipped-weapon bank;
/// the stored source selector is retained data. Clip 54 uses scene-extension
/// entry 7. Playback disables world collision; the room and Rupert
/// resources must stay loaded while the borrowed animation runs.
/// The stored blend duration is ignored on reset.
static AnimationPlayRequest _gAcropolisSanctuaryPlayerSceneClip54Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 7,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets the player's weapon-restoration clip 55 after the sanctuary scene poses.
///
/// Equipped weapon index 2 uses scene-extension entry 8. Its room animation
/// must remain loaded throughout playback.
/// The callback writes the current weapon bank into `source.index` before
/// synchronous dispatch. Playback disables world collision.
static AnimationPlayRequest _gAcropolisSanctuaryPlayerSpecialWeaponRestoreRequest = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 8,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets the player's weapon-restoration clip 7 after the sanctuary scene poses.
///
/// All equipped weapon indices except 2 use the bank's existing clip 7.
/// The selected weapon animation resources must remain loaded during playback.
/// The callback writes the current weapon bank into `source.index` before
/// synchronous dispatch. Playback disables world collision.
static AnimationPlayRequest _gAcropolisSanctuaryPlayerWeaponRestoreRequest = {
    .source               = { .index = 1 },
    .animationId          = 7,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts No. 9 GOLEM's body tracks on sanctuary scene clip 1.
///
/// The actor consumes only `animationId` during synchronous dispatch and
/// retains no request pointer. Bank, blend, duration and collision words are
/// retained payload data; its loaded animation resources outlive playback.
static AnimationPlayRequest _gAcropolisSanctuaryNo9Clip1Request = {
    .source               = { .index = 0 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 1,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts No. 9 GOLEM's body tracks on sanctuary scene clip 2.
///
/// The actor consumes only `animationId` during synchronous dispatch and
/// retains no request pointer. Bank, blend, duration and collision words are
/// retained payload data; its loaded animation resources outlive playback.
static AnimationPlayRequest _gAcropolisSanctuaryNo9Clip2Request = {
    .source               = { .index = 0 },
    .animationId          = 2,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 1,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts No. 9 GOLEM's body tracks on sanctuary scene clip 3.
///
/// The actor consumes only `animationId` during synchronous dispatch and
/// retains no request pointer. Bank, blend, duration and collision words are
/// retained payload data; its loaded animation resources outlive playback.
static AnimationPlayRequest _gAcropolisSanctuaryNo9Clip3Request = {
    .source               = { .index = 0 },
    .animationId          = 3,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 1,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts No. 9 GOLEM's body tracks on arrival-scene clip 4.
///
/// Dispatch borrows this record synchronously and consumes only animationId.
/// The other words are retained payload data; loaded clips outlive playback.
static const AnimationPlayRequest _gAcropolisSanctuaryNo9Clip4Request __attribute__((section(".data"))) = {
    .source               = { .index = 0 },
    .animationId          = 4,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 1,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Selects Rupert's bank-0 clip 1 without blending during the arrival scene.
///
/// Dispatch borrows the record synchronously. Changed clips reset slots 1..19;
/// an already selected clip keeps playing. Duration and collision are ignored.
static const AnimationPlayRequest _gAcropolisSanctuaryRupertClip1Request __attribute__((section(".data"))) = {
    .source               = { .index = 0 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 1,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Selects Rupert's bank-0 clip 2 without blending during the arrival scene.
///
/// Dispatch borrows the record synchronously. Changed clips reset slots 1..19;
/// an already selected clip keeps playing. Duration and collision are ignored.
static const AnimationPlayRequest _gAcropolisSanctuaryRupertClip2Request __attribute__((section(".data"))) = {
    .source               = { .index = 0 },
    .animationId          = 2,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 1,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Requests Rupert's bank-0 clip 3 with interpolation.
///
/// Scene scripts borrow this record synchronously. Rupert blends changed clips
/// over six frames and does not restart the same clip. The stored one-frame
/// duration and collision-disable option are retained but ignored by him.
static AnimationPlayRequest _gAcropolisSanctuaryRupertClip3Request = {
    .source               = { .index = 0 },
    .animationId          = 3,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 1,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Selects Rupert's bank-0 clip 4 after restoring his arrival-scene placement.
///
/// Dispatch borrows the record synchronously. Changed clips reset slots 1..19;
/// an already selected clip keeps playing. Duration and collision are ignored.
static const AnimationPlayRequest _gAcropolisSanctuaryRupertClip4Request __attribute__((section(".data"))) = {
    .source               = { .index = 0 },
    .animationId          = 4,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 1,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Requests Rupert's bank-0 clip 5 with interpolation.
///
/// Scene scripts borrow this record synchronously. Rupert blends changed clips
/// over six frames and does not restart the same clip. The stored one-frame
/// duration and collision-disable option are retained but ignored by him.
static AnimationPlayRequest _gAcropolisSanctuaryRupertClip5Request = {
    .source               = { .index = 0 },
    .animationId          = 5,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 1,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Requests Rupert's bank-0 clip 6 with interpolation.
///
/// Scene scripts borrow this record synchronously. Rupert blends changed clips
/// over six frames and does not restart the same clip. The stored one-frame
/// duration and collision-disable option are retained but ignored by him.
static AnimationPlayRequest _gAcropolisSanctuaryRupertClip6Request = {
    .source               = { .index = 0 },
    .animationId          = 6,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 1,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Selects Rupert's bank-0 clip 3 for scene skip and later room entry.
///
/// Resets tracks when the clip changes; selecting the same bank and clip leaves
/// playback running. Dispatch borrows the request synchronously. Rupert ignores
/// the duration and collision words, and his animation resources remain loaded.
static AnimationPlayRequest _gAcropolisSanctuaryRupertRestoredClip3Request = {
    .source               = { .index = 0 },
    .animationId          = 3,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Scene/audio stream selected for the sanctuary's first warp-3 arrival.
///
/// Group 1, stream 7 and qualifier 11 select the sanctuary folder's audio.
/// The event interpreter borrows this key until scene cleanup or script end.
static const EvsSceneKey _gAcropolisSanctuaryArrivalSceneKey __attribute__((section(".data"))) = {
    .group    = 1,
    .streamId = 7,
    .subId    = 11,
};

/// Unread scene-key-shaped record; no consumer establishes its event role.
static EvsSceneKey D_acropolis_sanctuary_80180B04 = { .group = 1, .streamId = 7, .subId = 21 };

/// Placed actors selected by the scene-message operand's placement index.
enum {
    ACROPOLIS_SANCTUARY_SCENE_ACTOR_NO9    = 0,
    ACROPOLIS_SANCTUARY_SCENE_ACTOR_RUPERT = 1,
};

/// CAP command slots started by the room's three event sequences.
enum {
    ACROPOLIS_SANCTUARY_CAP_ARRIVAL   = 1,
    ACROPOLIS_SANCTUARY_CAP_BLOCKER   = 2,
    ACROPOLIS_SANCTUARY_CAP_DEPARTURE = 3,
};

/// Receiver-specific draw payloads; No. 9 uses flags and Rupert uses modes.
enum {
    ACROPOLIS_SANCTUARY_NO9_DRAW_SHOW             = 1 << 0,
    ACROPOLIS_SANCTUARY_NO9_DRAW_HIDE_SKIP_AUTO   = 1 << 1,
    ACROPOLIS_SANCTUARY_RUPERT_DRAW_SHOW_ALLOCATE = 1,
    ACROPOLIS_SANCTUARY_RUPERT_DRAW_HIDE_RELEASE  = 2,
};

/// CAP-cued first-arrival scene with No. 9 GOLEM, Rupert and the mosaic breakup.
///
/// Starts once on warp 3 before bridge progress, installs the player's scene
/// clips and restores the player's weapon pose and placement at the end.
/// The interpreter borrows this END-terminated stream and all referenced room
/// and actor resources through completion or the arrival skip stream.
static EvsCommand _gAcropolisSanctuaryArrivalScript[] = {
    // Prepare actor visibility and the player's temporary scene animation bank.
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 61 }, { .value = 64 }, { .value = 80 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_RUPERT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisSanctuarySceneOriginPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_NO9 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACROPOLIS_SANCTUARY_NO9_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisSanctuaryPlayerWeaponClip1Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gAcropolisSanctuaryPlayerSceneBankExtensionRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gAcropolisSanctuaryArrivalSceneKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACROPOLIS_SANCTUARY_CAP_ARRIVAL }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU32 = _acropolisSanctuaryApplyScriptSpriteSelection }, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU32 = _acropolisSanctuaryApplyScriptSpriteSelection }, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_RUPERT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .animation = &_gAcropolisSanctuaryRupertClip1Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_NO9 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gAcropolisSanctuaryNo9Clip1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisSanctuaryPlayerSceneStartPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_RUPERT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACROPOLIS_SANCTUARY_RUPERT_DRAW_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_NO9 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACROPOLIS_SANCTUARY_NO9_DRAW_HIDE_SKIP_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _acropolisSanctuaryRestorePlayerWeaponAnimation }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_RUPERT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACROPOLIS_SANCTUARY_RUPERT_DRAW_SHOW_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_NO9 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACROPOLIS_SANCTUARY_NO9_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_RUPERT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .animation = &_gAcropolisSanctuaryRupertClip2Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_NO9 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gAcropolisSanctuaryNo9Clip2Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_NO9 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gAcropolisSanctuaryNo9Clip3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_NO9 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .animation = &_gAcropolisSanctuaryNo9Clip4Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisSanctuarySceneOriginPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisSanctuaryPlayerSceneClip47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU32 = _acropolisSanctuaryApplyScriptSpriteSelection }, { .value = (1 << ACROPOLIS_SANCTUARY_SCRIPT_SPRITE_BATCH_SHIFT) | 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_RUPERT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACROPOLIS_SANCTUARY_RUPERT_DRAW_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_NO9 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACROPOLIS_SANCTUARY_NO9_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Reveal the alternate wall sprites before spawning the mosaic debris.
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU32 = _acropolisSanctuaryApplyScriptSpriteSelection }, { .value = (1 << ACROPOLIS_SANCTUARY_SCRIPT_SPRITE_BATCH_SHIFT) | 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_ROOM_EFFECT }, { .value = 0 }, { .value = ACROPOLIS_SANCTUARY_MOSAIC_MESSAGE_START }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_RUPERT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACROPOLIS_SANCTUARY_RUPERT_DRAW_SHOW_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_NO9 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACROPOLIS_SANCTUARY_NO9_DRAW_HIDE_SKIP_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisSanctuaryPlayerSceneClip48Placement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisSanctuaryPlayerSceneClip48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_RUPERT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisSanctuaryRupertRestoredPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_RUPERT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .animation = &_gAcropolisSanctuaryRupertClip4Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 73 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_RUPERT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .animation = &_gAcropolisSanctuaryRupertClip3Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisSanctuaryPlayerSceneClip52Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisSanctuaryPlayerSceneClip49Placement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisSanctuaryPlayerSceneClip49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisSanctuaryPlayerSceneClip50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 27 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisSanctuaryPlayerSceneClip52Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_RUPERT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .animation = &_gAcropolisSanctuaryRupertClip5Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_RUPERT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .animation = &_gAcropolisSanctuaryRupertClip6Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_RUPERT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .animation = &_gAcropolisSanctuaryRupertClip3Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisSanctuaryPlayerSceneClip49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisSanctuaryPlayerSceneClip50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 27 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisSanctuaryPlayerSceneClip52Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_RUPERT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .animation = &_gAcropolisSanctuaryRupertClip5Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_RUPERT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .animation = &_gAcropolisSanctuaryRupertClip6Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_RUPERT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .animation = &_gAcropolisSanctuaryRupertClip3Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisSanctuaryPlayerSceneClip49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisSanctuaryPlayerSceneClip50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 27 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisSanctuaryPlayerSceneClip52Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_RUPERT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .animation = &_gAcropolisSanctuaryRupertClip5Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_RUPERT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .animation = &_gAcropolisSanctuaryRupertClip6Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_RUPERT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .animation = &_gAcropolisSanctuaryRupertClip3Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisSanctuaryPlayerSceneClip51Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 71 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisSanctuaryPlayerSceneClip52LongBlendRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisSanctuaryPlayerSceneClip53Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisSanctuaryPlayerSceneClosingPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 56 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisSanctuaryPlayerSceneClip54Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_RUPERT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .animation = &_gAcropolisSanctuaryRupertClip6Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_RUPERT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .animation = &_gAcropolisSanctuaryRupertClip3Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Restore weapon playback and release scripted control before the final placement.
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisSanctuaryPlayerWeaponClip1Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_RESET_ANIMATION }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisSanctuaryPlayerSceneEndPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip completion for the sanctuary arrival scene.
///
/// Fades across player/Rupert restoration, hides No. 9, releases scene control
/// and restarts area music. The interpreter borrows this END-terminated stream
/// and its placement/animation payloads until cleanup completes.
static EvsCommand _gAcropolisSanctuaryArrivalSkipScript[] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisSanctuaryPlayerWeaponClip1Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisSanctuaryPlayerSceneEndPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_RUPERT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gAcropolisSanctuaryRupertRestoredPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_RUPERT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .animation = &_gAcropolisSanctuaryRupertRestoredClip3Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_NO9 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACROPOLIS_SANCTUARY_NO9_DRAW_HIDE_SKIP_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_RUPERT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACROPOLIS_SANCTUARY_RUPERT_DRAW_SHOW_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// CAP-2 interaction with Rupert while the sanctuary blocker is uncleared.
///
/// Room command 0 starts this END-terminated stream. It selects no scene audio,
/// cues Rupert's clips 5, 6 and 3, then releases player scripted control.
/// Keep this stream and the borrowed actor requests loaded through completion.
static EvsCommand _gAcropolisSanctuaryBlockerScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACROPOLIS_SANCTUARY_CAP_BLOCKER }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gAcropolisSanctuaryPlayerWeaponClip1Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_RUPERT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .animation = &_gAcropolisSanctuaryRupertClip5Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_RUPERT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .animation = &_gAcropolisSanctuaryRupertClip6Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACROPOLIS_SANCTUARY_SCENE_ACTOR_RUPERT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .animation = &_gAcropolisSanctuaryRupertClip3Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static AnimationPackedPose _gAcropolisSanctuaryAnimation04708Bank1[7] = {
#include "assets/acropolis_sanctuary_animation_04708_bank1.inc"
};

static AnimationPackedRotation _gAcropolisSanctuaryAnimation04708Bank4[84] = {
#include "assets/acropolis_sanctuary_animation_04708_bank4.inc"
};

static AnimationRecord _gAcropolisSanctuaryAnimation04708Records[120] = {
#include "assets/acropolis_sanctuary_animation_04708_records.inc"
};

static u16 _gAcropolisSanctuaryAnimation04708Indices[20] = {
#include "assets/acropolis_sanctuary_animation_04708_indices.inc"
};

static AnimationSet _gAcropolisSanctuaryAnimation04708 = {
    _gAcropolisSanctuaryAnimation04708Records,
    _gAcropolisSanctuaryAnimation04708Indices,
    { NULL, _gAcropolisSanctuaryAnimation04708Bank1, NULL, NULL, _gAcropolisSanctuaryAnimation04708Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisSanctuaryAnimation04AFCBank1[8] = {
#include "assets/acropolis_sanctuary_animation_04AFC_bank1.inc"
};

static AnimationPackedRotation _gAcropolisSanctuaryAnimation04AFCBank4[88] = {
#include "assets/acropolis_sanctuary_animation_04AFC_bank4.inc"
};

static AnimationRecord _gAcropolisSanctuaryAnimation04AFCRecords[121] = {
#include "assets/acropolis_sanctuary_animation_04AFC_records.inc"
};

static u16 _gAcropolisSanctuaryAnimation04AFCIndices[20] = {
#include "assets/acropolis_sanctuary_animation_04AFC_indices.inc"
};

static AnimationSet _gAcropolisSanctuaryAnimation04AFC = {
    _gAcropolisSanctuaryAnimation04AFCRecords,
    _gAcropolisSanctuaryAnimation04AFCIndices,
    { NULL, _gAcropolisSanctuaryAnimation04AFCBank1, NULL, NULL, _gAcropolisSanctuaryAnimation04AFCBank4, NULL, NULL, NULL },
};

/// Single-set table installed for the player's sanctuary departure cutscene.
///
/// Install-and-play borrows this table and starts clip 0. Keep the table and
/// its room animation set loaded through playback and the roof-garden handoff.
static AnimationSet* _gAcropolisSanctuaryCutscenePlayerAnimationSets[] = {
    &_gAcropolisSanctuaryAnimation04708,
};

/// Unread scene-key-shaped record; the departure script selects a NULL key.
static EvsSceneKey D_acropolis_sanctuary_801820E8 = { .group = 1, .streamId = 14, .subId = 11 };

/// CAP-3 departure sequence arming player placement and the sound transition.
///
/// The bodyless cutscene task starts this END-terminated stream and handles the
/// roof-garden transfer after event completion. CAP cues invoke its singleton
/// work, which must remain live; the stream selects no scene/audio key.
static EvsCommand _gAcropolisSanctuaryDepartureScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACROPOLIS_SANCTUARY_CAP_DEPARTURE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _acropolisSanctuaryRequestCutsceneCue }, { .value = ACROPOLIS_SANCTUARY_CUTSCENE_CUE_PLACE_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _acropolisSanctuaryRequestCutsceneCue }, { .value = ACROPOLIS_SANCTUARY_CUTSCENE_CUE_CHANGE_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip completion releasing the sanctuary departure scene under a primary fade.
///
/// This END-terminated stream leaves the roof-garden handoff to the cutscene
/// task. The interpreter borrows it through cleanup; it does not replay cues.
static EvsCommand _gAcropolisSanctuaryDepartureSkipScript[] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

enum { ACROPOLIS_SANCTUARY_CUTSCENE_TASK_PRIORITY = 192 };

/// Bodyless task recipe for the first sanctuary room action 1.
///
/// Direct spawning reads this single entry synchronously with zero payloads.
/// The task owns its separately allocated work. Keep the room loaded through
/// the departure scene and roof-garden handoff; data is unused.
static TaskDesc _gAcropolisSanctuaryCutsceneTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = ACROPOLIS_SANCTUARY_CUTSCENE_TASK_PRIORITY } },
    .callback = _acropolisSanctuaryCutsceneTask,
    .data     = { .value = 0 },
};

/// Four blocker face normals, using 4096 for unit length.
static SVECTOR _gAcropolisSanctuaryBlockerCollisionGridNormals[4] = {
#include "assets/acropolis_sanctuary_collision_04D2C_normals.inc"
};

/// Eight authored blocker vertices in whole room-coordinate units.
static SVECTOR _gAcropolisSanctuaryBlockerCollisionGridVerts[8] = {
#include "assets/acropolis_sanctuary_collision_04D2C_verts.inc"
};

/// Four blocker quads indexing the eight vertices and four normals.
static WorldCollisionGridFace _gAcropolisSanctuaryBlockerCollisionGridFaces[4] = {
#include "assets/acropolis_sanctuary_collision_04D2C_faces.inc"
};

/// Template cell face IDs 0..3, then -1; the sixth authored halfword is unused.
static s16 _gAcropolisSanctuaryBlockerCollisionGridCells[6] = {
#include "assets/acropolis_sanctuary_collision_04D2C_cells.inc"
};

#define GRID_CELL(i) (&_gAcropolisSanctuaryBlockerCollisionGridCells[i])
/// Single template cell pointing at the start of its terminated face-ID list.
static s16* _gAcropolisSanctuaryBlockerCollisionGridTable[1] = {
#include "assets/acropolis_sanctuary_collision_04D2C_table.inc"
};
#undef GRID_CELL

/// Authored collision template for the sanctuary's movable blocker.
///
/// Four normals (4096/unit), eight room-unit vertices and four quad faces are
/// restored into the live room grid. Restoration copies vector XYZ only, sets
/// surface class 1 and applies the saved-state offset. The one-cell index and
/// grid biases belong to this template and are not installed in the live grid.
/// Keep these borrowed pools loaded through each restoration.
static WorldCollisionGrid _gAcropolisSanctuaryBlockerCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gAcropolisSanctuaryBlockerCollisionGridNormals,
    .vertices    = _gAcropolisSanctuaryBlockerCollisionGridVerts,
    .faces       = _gAcropolisSanctuaryBlockerCollisionGridFaces,
    .cellFaceIds = _gAcropolisSanctuaryBlockerCollisionGridTable,
    .xBias       = 4941,
    .zBias       = 8914,
    .cellCountX  = 1,
    .cellCountZ  = 1,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gAcropolisSanctuaryBlockerCollisionGridFaces),
};

/// Mosaic-start route borrowed by the sanctuary room-effect task.
///
/// The start message ignores both payload words. The terminal record makes
/// unsupported IDs return zero; this table and callback outlive the receiver.
static const TaskMessageEntry _gAcropolisSanctuaryRoomEffectMessages[] __attribute__((section(".data"))) = {
    { ACROPOLIS_SANCTUARY_MOSAIC_MESSAGE_START, _acropolisSanctuaryStartMosaic },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Row selectors shared with each mosaic tile's sizeClass.
enum {
    ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL = 0,
    ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE = 1,
};

/// Authored tile release modes; crumbling tiles add 0..7 random hold frames.
enum {
    ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE = 0,
    ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN  = 1,
};

/// Seventy-two mosaic fragments, indexed by tile and shard spawn IDs 0..71.
///
/// Origins and extents select sheet texels and placement; hold times use frames.
/// The spawner also repeats sixteen selected IDs. Both tile sizes and release
/// modes are retained here; the two V=178 origins are authored data. Keep the
/// table loaded until every tile and shard task has ended.
static _AcropolisSanctuaryMosaicTile _gAcropolisSanctuaryMosaicTiles[] = {
    { 29, 31, 0, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 24, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 29, 31, 30, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 16, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 14, 15, 60, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 16, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
    { 14, 15, 75, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 16, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
    { 14, 15, 90, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 16, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
    { 14, 15, 105, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 16, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
    { 29, 31, 120, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 16, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 29, 31, 150, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 24, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 29, 31, 60, 16, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 8, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 29, 31, 90, 16, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 8, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 29, 31, 0, 32, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 16, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 29, 31, 30, 32, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 8, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 29, 31, 120, 32, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 8, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 29, 31, 150, 32, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 16, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 29, 31, 60, 48, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 29, 31, 90, 48, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 29, 31, 0, 64, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 8, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 29, 31, 30, 64, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 29, 31, 120, 64, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 29, 31, 150, 64, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 8, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 14, 15, 60, 80, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
    { 29, 31, 75, 80, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 14, 15, 105, 80, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
    { 14, 15, 0, 96, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 8, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
    { 29, 31, 15, 96, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 14, 15, 45, 96, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
    { 14, 15, 60, 96, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
    { 14, 15, 105, 96, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
    { 14, 15, 120, 96, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
    { 29, 31, 135, 96, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 14, 15, 165, 96, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 8, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
    { 14, 15, 0, 112, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 16, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
    { 29, 31, 45, 112, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 14, 15, 75, 112, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
    { 14, 15, 90, 112, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
    { 29, 31, 105, 112, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 14, 15, 165, 112, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 16, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
    { 14, 15, 0, 128, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 24, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
    { 29, 31, 15, 128, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 14, 15, 75, 128, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
    { 14, 15, 90, 128, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
    { 29, 31, 135, 128, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 14, 15, 165, 128, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 24, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
    { 14, 15, 0, 144, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 16, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
    { 14, 15, 45, 144, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
    { 14, 15, 60, 144, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
    { 29, 31, 75, 144, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 14, 15, 105, 144, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
    { 14, 15, 120, 144, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
    { 14, 15, 165, 144, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 16, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
    { 29, 31, 0, 160, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 8, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 29, 31, 30, 160, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 14, 15, 60, 160, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
    { 14, 15, 105, 160, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
    { 29, 31, 120, 160, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 29, 31, 150, 160, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 8, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 29, 31, 60, 178, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 29, 31, 90, 178, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 29, 31, 0, 192, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 16, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 29, 31, 30, 192, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 29, 31, 120, 192, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 29, 31, 150, 192, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 16, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 29, 31, 60, 208, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 29, 31, 90, 208, ACROPOLIS_SANCTUARY_MOSAIC_TILE_THROWN, 0, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 29, 31, 0, 224, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 24, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 29, 31, 30, 224, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 8, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 29, 31, 120, 224, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 8, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 29, 31, 150, 224, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 24, ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE },
    { 14, 15, 60, 240, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 8, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
    { 14, 15, 75, 240, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 16, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
    { 14, 15, 90, 240, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 16, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
    { 14, 15, 105, 240, ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE, 8, ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL },
};

/// Centre-relative corners of the mosaic's two tile sizes, in local room units.
///
/// Indexed by size class (0 a 15 x 16 texel cell, 1 a 30 x 32 block), then
/// POLY_FT4 corner 0..3. The local YZ plane maps increasing texture U towards
/// -Z and V towards +Y. Subtracting corner 0 places that texture origin at the
/// wall position while spawning the tile at its centre. Shards use the first
/// three small-tile corners and scale them by their 4.12 size factor.
static SVECTOR _gAcropolisSanctuaryMosaicTileCorners[2][4] = {
    [ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL] = { { 0, -72, 63, 0 }, { 0, -72, -62, 0 }, { 0, 71, 63, 0 }, { 0, 71, -62, 0 } },
    [ACROPOLIS_SANCTUARY_MOSAIC_TILE_LARGE] = { { 0, -143, 126, 0 }, { 0, -143, -125, 0 }, { 0, 143, 126, 0 }, { 0, 143, -125, 0 } },
};

/// Sixteen mosaic tile IDs receiving a second whole-tile effect.
///
/// Every ID is in 0..71 and indexes the authored mosaic tile table. The spawner
/// visits the entire list after spawning all 72 tiles, retaining no list pointer.
static const s16 _gAcropolisSanctuaryMosaicRepeatedTileIndices[] __attribute__((section(".data"))) = {
    20,
    22,
    25,
    26,
    27,
    28,
    33,
    34,
    39,
    40,
    44,
    45,
    47,
    48,
    52,
    53,
};

/// States stored in the mosaic debris cleanup latch.
enum {
    ACROPOLIS_SANCTUARY_MOSAIC_CLEANUP_CLEAR   = 0,
    ACROPOLIS_SANCTUARY_MOSAIC_CLEANUP_LATCHED = 1,
};

/// View-driven latch accelerating cleanup of mosaic debris outside view 16.
///
/// Room-effect initialization clears it; view 16 sets it, view 12 retains it,
/// and other views clear it. Tile and shard tasks use it to expire debris past
/// the room's cleanup boundaries. Stored as a word (0 clear, 1 latched).
static s32 _gAcropolisSanctuaryMosaicCleanupLatched = ACROPOLIS_SANCTUARY_MOSAIC_CLEANUP_CLEAR;

/// Twelve flame origins in whole units of the room-effect coordinate frame.
///
/// IDs 0..5 spawn dim variant-2 flames at size 640; IDs 6..11 spawn variant-0
/// flames at size 160. IDs also index the flame view masks. Spawning snapshots
/// XYZ, so the child does not follow the borrowed placement pointer.
static SVECTOR _gAcropolisSanctuaryFlamePositions[] = {
    { -0x2738, -2088, -6680, 0 },
    { -0x2738, -2088, -9280, 0 },
    { -2720, -2088, -6460, 0 },
    { -2720, -2088, -9570, 0 },
    { -6690, -2088, -4220, 0 },
    { -8310, -2028, -4220, 0 },
    { -2420, -1630, -7210, 0 },
    { -2420, -1680, -7330, 0 },
    { -2420, -1630, -7450, 0 },
    { -2420, -1630, -8490, 0 },
    { -2420, -1680, -8600, 0 },
    { -2420, -1630, -8730, 0 },
};

/// Visible logical views for each of the twelve sanctuary flame placements.
///
/// Placement indices 0..11 match the room's flame-position table. Bit 0 selects
/// view 1 and bit 15 view 16; the drawer requires a current view in 1..16.
static u16 _gAcropolisSanctuaryFlameViewMasks[12] = {
    0x0A42, // Views 2, 7, 10, 12
    0x0852, // Views 2, 5, 7, 12
    0x152C, // Views 3, 4, 6, 9, 11, 13
    0x012C, // Views 3, 4, 6, 9
    0x3460, // Views 6, 7, 11, 13, 14
    0x3440, // Views 7, 11, 13, 14
    0x11AC, // Views 3, 4, 6, 8, 9, 13
    0x11AC, // Views 3, 4, 6, 8, 9, 13
    0x11AC, // Views 3, 4, 6, 8, 9, 13
    0x01AC, // Views 3, 4, 6, 8, 9
    0x01AC, // Views 3, 4, 6, 8, 9
    0x01AC, // Views 3, 4, 6, 8, 9
};

WorldCollisionRoomResources gAcropolisSanctuaryRoomCollisionResources[1] = {
    { .grid = &_gAcropolisSanctuaryCollisionGrid, .viewBoundaryTriggers = _gAcropolisSanctuaryViewBoundaryTriggers, .actionTriggers = _gAcropolisSanctuaryActionTriggers, .occluders = NULL },
};

u8* gAcropolisSanctuaryViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gAcropolisSanctuaryViewCounts[1] = { ARRAY_SIZE(gAcropolisSanctuarySpriteViews) };

WorldCoordRoomLighting gAcropolisSanctuaryRoomLightingTable[1] = {
    { &_gAcropolisSanctuaryRoomLights, NULL },
};

/// Map-marker nibbles updated when arriving through the sanctuary's doorways.
enum {
    ACROPOLIS_SANCTUARY_ROOF_GARDEN_MAP_FLAG = 492,
    ACROPOLIS_SANCTUARY_PROMENADE_MAP_FLAG   = 493,
};

DirectionWarpEntry gAcropolisSanctuaryWarpEntries[3] = {
    { { { .word = ONE / 4 }, -9757, 2, -8070 }, { 0, 0, 0, 0 }, { { .word = ONE / 4 }, -9757, 2, -8070 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SANCTUARY, 2), SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SANCTUARY, 1), DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, ACROPOLIS_SANCTUARY_PROMENADE_MAP_FLAG },
    { { { .word = ONE / 2 }, -7465, 2, -4239 }, { 0, 0, 0, 0 }, { { .word = ONE / 2 }, -7465, 2, -4239 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SANCTUARY, 4), SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SANCTUARY, 3), DIRECTION_WARP_SOUND_NONE, 7, DIRECTION_WARP_FLAG_NONE, ACROPOLIS_SANCTUARY_ROOF_GARDEN_MAP_FLAG },
    { { { .word = ONE / 4 }, -9757, 2, -8070 }, { 0, 0, 0, 0 }, { { .word = ONE / 4 }, -9757, 2, -8070 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SANCTUARY, 2), SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SANCTUARY, 1), DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, ACROPOLIS_SANCTUARY_PROMENADE_MAP_FLAG },
};

/// Sanctuary collision normals, including four rebuilt blocker normals.
static SVECTOR _gAcropolisSanctuaryCollisionGridNormals[42] = {
#include "assets/acropolis_sanctuary_collision_05FA8_normals.inc"
};

/// Sanctuary collision vertices, including eight rebuilt blocker vertices.
static SVECTOR _gAcropolisSanctuaryCollisionGridVerts[136] = {
#include "assets/acropolis_sanctuary_collision_05FA8_verts.inc"
};

/// Sanctuary collision faces, including four rebuilt blocker faces.
static WorldCollisionGridFace _gAcropolisSanctuaryCollisionGridFaces[85] = {
#include "assets/acropolis_sanctuary_collision_05FA8_faces.inc"
};

/// Signed face-index pool for the nine cells, with -1 ending each live list.
static s16 _gAcropolisSanctuaryCollisionGridCells[386] = {
#include "assets/acropolis_sanctuary_collision_05FA8_cells.inc"
};

#define GRID_CELL(i) (&_gAcropolisSanctuaryCollisionGridCells[i])
/// Cell-list starts indexed by x * 3 + z, for cell coordinates 0..2.
static s16* _gAcropolisSanctuaryCollisionGridTable[9] = {
#include "assets/acropolis_sanctuary_collision_05FA8_table.inc"
};
#undef GRID_CELL

/// Live sanctuary collision mesh and its nine X-major cell lists.
///
/// Coordinates use room units and normals use 4096 for unit length. The grid
/// spans three 4000-unit cells on each axis from origin (-10820, -13101).
/// Room setup binds its view coordinate. The first four normals and faces and
/// first eight vertices are rebuilt from the movable-blocker template.
static WorldCollisionGrid _gAcropolisSanctuaryCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gAcropolisSanctuaryCollisionGridNormals,
    .vertices    = _gAcropolisSanctuaryCollisionGridVerts,
    .faces       = _gAcropolisSanctuaryCollisionGridFaces,
    .cellFaceIds = _gAcropolisSanctuaryCollisionGridTable,
    .xBias       = 10820,
    .zBias       = 13101,
    .cellCountX  = 3,
    .cellCountZ  = 3,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gAcropolisSanctuaryCollisionGridFaces),
};

/// Eighteen directed view boundaries for sanctuary room 1.
///
/// Source/destination bytes select logical views 2..8. Room setup binds, links
/// and enables every record through the last; the live links and hit latches
/// require writable overlay storage until teardown unlinks them.
static WorldCollisionTrigger _gAcropolisSanctuaryViewBoundaryTriggers[18] = {
    { NULL, NULL, NULL, { -7168, -1376, -8000, 0 }, { { 0, -2048, -1504, 0 }, { 0, 2048, -1504, 0 }, { 0, -2048, 1504, 0 }, { 0, 2048, 1504, 0 } }, { -4097, 0, 0, 0 }, { 0, 0, 0, 0 }, 2534, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -9729, -1344, -5953, 0 }, { { 1242, -2048, -514, 0 }, { 1242, 2048, -514, 0 }, { -1241, -2048, 515, 0 }, { -1241, 2048, 515, 0 } }, { -1570, 0, -3788, 0 }, { 0, 0, 0, 0 }, 2442, 0, 2, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -9665, -1376, -0x2721, 0 }, { { -1241, -2048, -514, 0 }, { -1241, 2048, -514, 0 }, { 1242, -2048, 514, 0 }, { 1242, 2048, 514, 0 } }, { -1569, 0, 3787, 0 }, { 0, 0, 0, 0 }, 2442, 0, 2, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7456, -1088, -7981, 0 }, { { 0, -2048, 1376, 0 }, { 0, 2048, 1376, 0 }, { 0, -2048, -1376, 0 }, { 0, 2048, -1376, 0 } }, { 4105, 0, 0, 0 }, { 0, 0, 0, 0 }, 2455, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -9953, -1344, -9504, 0 }, { { 1242, -2048, 514, 0 }, { 1242, 2048, 514, 0 }, { -1241, -2048, -514, 0 }, { -1241, 2048, -514, 0 } }, { 1568, 0, -3788, 0 }, { 0, 0, 0, 0 }, 2442, 0, 5, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -9985, -1216, -6529, 0 }, { { -1265, -2048, 453, 0 }, { -1265, 2048, 453, 0 }, { 1265, -2048, -453, 0 }, { 1265, 2048, -453, 0 } }, { 1382, 0, 3859, 0 }, { 0, 0, 0, 0 }, 2442, 0, 7, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7203, -1248, -0x2D63, 0 }, { { -5, -2048, 1057, 0 }, { -5, 2048, 1057, 0 }, { 5, -2048, -1056, 0 }, { 5, 2048, -1056, 0 } }, { 4099, 0, 19, 0 }, { 0, 0, 0, 0 }, 2304, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6850, -1312, -0x2D81, 0 }, { { 5, -2048, -1184, 0 }, { 5, 2048, -1184, 0 }, { -5, -2048, 1185, 0 }, { -5, 2048, 1185, 0 } }, { -4112, 0, -18, 0 }, { 0, 0, 0, 0 }, 2360, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6704, -1280, -4259, 0 }, { { -5, -2048, 1217, 0 }, { -5, 2048, 1217, 0 }, { 5, -2048, -1216, 0 }, { 5, 2048, -1216, 0 } }, { 4103, 0, 16, 0 }, { 0, 0, 0, 0 }, 2374, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6399, -1344, -4258, 0 }, { { 5, -2048, -1312, 0 }, { 5, 2048, -1312, 0 }, { -5, -2048, 1313, 0 }, { -5, 2048, 1313, 0 } }, { -4098, 0, -16, 0 }, { 0, 0, 0, 0 }, 2428, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4337, -1344, -5456, 0 }, { { -1472, -2048, -864, 0 }, { -1472, 2048, -864, 0 }, { 1473, -2048, 864, 0 }, { 1473, 2048, 864, 0 } }, { -2078, 0, 3540, 0 }, { 0, 0, 0, 0 }, 2660, 0, 6, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4401, -1312, -4928, 0 }, { { 1457, -2048, 896, 0 }, { 1457, 2048, 896, 0 }, { -1456, -2048, -896, 0 }, { -1456, 2048, -896, 0 } }, { 2149, 0, -3494, 0 }, { 0, 0, 0, 0 }, 2660, 0, 3, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3907, -1312, -0x2763, 0 }, { { 1810, -2048, -622, 0 }, { 1810, 2048, -622, 0 }, { -1809, -2048, 623, 0 }, { -1809, 2048, 623, 0 } }, { -1334, 0, -3877, 0 }, { 0, 0, 0, 0 }, 2792, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3970, -1312, -0x2884, 0 }, { { -2065, -2048, 751, 0 }, { -2065, 2048, 751, 0 }, { 2066, -2048, -750, 0 }, { 2066, 2048, -750, 0 } }, { 1405, 0, 3867, 0 }, { 0, 0, 0, 0 }, 2996, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2955, -1312, -8921, 0 }, { { 1025, -2048, -671, 0 }, { 1025, 2048, -671, 0 }, { -1026, -2048, 670, 0 }, { -1026, 2048, 670, 0 } }, { -2250, 0, -3441, 0 }, { 0, 0, 0, 0 }, 2374, 0, 3, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3298, -1344, -6786, 0 }, { { 958, -2048, 764, 0 }, { 958, 2048, 764, 0 }, { -958, -2048, -764, 0 }, { -958, 2048, -764, 0 } }, { 2562, 0, -3214, 0 }, { 0, 0, 0, 0 }, 2374, 0, 8, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2979, -1280, -6915, 0 }, { { -953, -2048, -769, 0 }, { -953, 2048, -769, 0 }, { 954, -2048, 770, 0 }, { 954, 2048, 770, 0 } }, { -2582, 0, 3198, 0 }, { 0, 0, 0, 0 }, 2374, 0, 3, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3201, -1344, -9089, 0 }, { { -1013, -2048, 690, 0 }, { -1013, 2048, 690, 0 }, { 1013, -2048, -690, 0 }, { 1013, 2048, -690, 0 } }, { 2314, 0, 3398, 0 }, { 0, 0, 0, 0 }, 2374, 0, 8, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Seventeen sanctuary interaction triggers, including doors and weapon CAPs.
///
/// Rows 0/1 request promenade/roof-garden warps; rows 2..4 and 7/8 request
/// CAPs, and row 5 automatically sends room action 1. Rows 6 and 9..16 request
/// weapon CAP 7 and lose only their enabled bit when object 28 reaches state 2.
/// Room setup binds and links all rows through the final LAST marker.
static WorldCollisionTrigger _gAcropolisSanctuaryActionTriggers[17] = {
    { NULL, NULL, NULL, { -9920, -42, -7968, 0 }, { { -287, 0, -1024, 0 }, { 288, 0, -1024, 0 }, { -287, 0, 1024, 0 }, { 288, 0, 1024, 0 } }, { 0, 4106, 0, 0 }, { 4096, 0, 0, 0 }, 1063, WORLD_COLLISION_TRIGGER_ACTION_WARP, 11, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7728, -24, -4448, 0 }, { { -735, 0, -256, 0 }, { 736, 0, -256, 0 }, { -735, 0, 256, 0 }, { 736, 0, 256, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, -4096, 0 }, 778, WORLD_COLLISION_TRIGGER_ACTION_WARP, 13, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3648, -64, -8016, 0 }, { { -319, 0, -432, 0 }, { 320, 0, -432, 0 }, { -319, 0, 432, 0 }, { 320, 0, 432, 0 } }, { 0, 4099, 0, 0 }, { 4096, 0, 0, 0 }, 535, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5312, -64, -8288, 0 }, { { -543, 0, -896, 0 }, { 544, 0, -896, 0 }, { -543, 0, 896, 0 }, { 544, 0, 896, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 1047, WORLD_COLLISION_TRIGGER_ACTION_CAP, 0, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2736, -61, -7896, 0 }, { { -655, 0, -680, 0 }, { 656, 0, -1448, 0 }, { -655, 0, 856, 0 }, { 656, 0, 1272, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 1588, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7712, -32, -4416, 0 }, { { -895, 0, -672, 0 }, { 896, 0, -672, 0 }, { -895, 0, 672, 0 }, { 896, 0, 672, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 1115, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -6608, -64, -8086, 0 }, { { -3, 0, 125, 0 }, { -105, 0, -3, 0 }, { 113, 0, -14, 0 }, { -2, 0, -106, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4095, 0 }, 124, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 7, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -4768, -64, -6992, 0 }, { { -751, 0, -720, 0 }, { 752, 0, -720, 0 }, { -751, 0, 720, 0 }, { 752, 0, 720, 0 } }, { 0, 4097, 0, 0 }, { -995, 0, 3973, 0 }, 1039, WORLD_COLLISION_TRIGGER_ACTION_CAP, 0, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4768, -64, -8864, 0 }, { { -751, 0, -624, 0 }, { 752, 0, -624, 0 }, { -751, 0, 624, 0 }, { 752, 0, 624, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, -4096, 0 }, 976, WORLD_COLLISION_TRIGGER_ACTION_CAP, 0, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6446, -64, -7903, 0 }, { { -115, 0, 362, 0 }, { -377, 0, 99, 0 }, { 385, 0, -104, 0 }, { 110, 0, -356, 0 } }, { 0, 4100, 0, 0 }, { 2895, 0, 2895, 0 }, 398, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6800, -64, -8240, 0 }, { { -99, 0, 378, 0 }, { -393, 0, 83, 0 }, { 401, 0, -88, 0 }, { 94, 0, -372, 0 } }, { 0, 4106, 0, 0 }, { -2896, 0, -2896, 0 }, 409, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6802, -64, -7904, 0 }, { { 58, 0, 362, 0 }, { -364, 0, -61, 0 }, { 371, 0, 56, 0 }, { -64, 0, -356, 0 } }, { 0, 4095, 0, 0 }, { -2897, 0, 2895, 0 }, 374, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6448, -64, -8256, 0 }, { { 74, 0, 346, 0 }, { -348, 0, -77, 0 }, { 355, 0, 72, 0 }, { -80, 0, -340, 0 } }, { 0, 4095, 0, 0 }, { 2896, 0, -2896, 0 }, 362, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6633, -64, -7809, 0 }, { { 1, 0, 277, 0 }, { -186, 0, 5, 0 }, { 178, 0, -9, 0 }, { 10, 0, -270, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4093, 0 }, 275, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6617, -64, -8385, 0 }, { { -15, 0, 277, 0 }, { -170, 0, 5, 0 }, { 194, 0, -9, 0 }, { -6, 0, -270, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4093, 0 }, 277, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6305, -64, -8097, 0 }, { { -276, 0, -8, 0 }, { -4, 0, -163, 0 }, { 10, 0, 170, 0 }, { 270, 0, 1, 0 } }, { 0, 4094, 0, 0 }, { 4093, 0, 0, 0 }, 275, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6912, -64, -8073, 0 }, { { -276, 0, 0, 0 }, { -4, 0, -155, 0 }, { 10, 0, 146, 0 }, { 270, 0, 9, 0 } }, { 0, 4095, 0, 0 }, { -4093, 0, 0, 0 }, 275, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Resource keys shared with the map overlay's sanctuary placement records.
enum {
    ACROPOLIS_SANCTUARY_RESOURCE_NO9    = 27,
    ACROPOLIS_SANCTUARY_RESOURCE_RUPERT = 102,
};

/// Actor resources for sanctuary layout variant 1: No. 9 GOLEM and Rupert.
///
/// Placement IDs 27 and 102 select global-library files 110700 and 210700
/// and their actor task descriptors. The final record ends the resource walk.
static AreaResource _gAcropolisSanctuaryNo9AndRupertResources[3] = {
    { ACROPOLIS_SANCTUARY_RESOURCE_NO9, 107, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor110700No9GolemTaskDesc },
    { ACROPOLIS_SANCTUARY_RESOURCE_RUPERT, 107, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &gActor210700RupertBroderickTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Actor resources for sanctuary layout variant 2: Rupert alone.
///
/// Placement ID 102 selects global-library file 210700 and Rupert's task
/// descriptor. The final record ends the resource walk.
static AreaResource _gAcropolisSanctuaryRupertResources[2] = {
    { ACROPOLIS_SANCTUARY_RESOURCE_RUPERT, 107, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &gActor210700RupertBroderickTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Saved layout selectors; all other slots in the published table are absent.
enum {
    ACROPOLIS_SANCTUARY_AREA_VARIANT_NO9_AND_RUPERT = 1,
    ACROPOLIS_SANCTUARY_AREA_VARIANT_RUPERT         = 2,
};

AreaVariant gAcropolisSanctuaryAreaVariants[12] = {
    [ACROPOLIS_SANCTUARY_AREA_VARIANT_NO9_AND_RUPERT] = {
        .placements = gMapAkropolisSanctuaryNo9AndRupertPlacements,
        .resources  = _gAcropolisSanctuaryNo9AndRupertResources,
    },
    [ACROPOLIS_SANCTUARY_AREA_VARIANT_RUPERT] = {
        .placements = gMapAkropolisSanctuaryRupertPlacements,
        .resources  = _gAcropolisSanctuaryRupertResources,
    },
};

/// Authored point lights contributing in every view of the Acropolis sanctuary.
///
/// Positions and radii use integer world units; RGB intensities have 12
/// fractional bits (`ONE` is 1.0). The room-light collection borrows this
/// writable table while the overlay is loaded: coordinate updates set its
/// parents and composed matrices, and lighting queries overwrite attenuation.
static WorldCoordPointLight _gAcropolisSanctuaryPointLights[] = {
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -7720, -3500, -5973 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2867, 2867, 2867 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 4575,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4989, -3500, -6313 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2867, 2867, 2867 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 4488,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5050, -3500, -10034 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2867, 2867, 2867 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 4671,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -7720, -3500, -10034 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2867, 2867, 2867 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 4284,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -10120, -2000, -9282 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, 3276 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 4500,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -10110, -2000, -6674 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, 3276 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 4499,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2870, -2000, -6481 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, 3276 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 4500,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2870, -2000, -9568 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, 3276 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 4500,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6180, -3500, -8019 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE / 2, ONE / 2, ONE / 2 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 10000,
        .outer = 10000,
    },
};

/// The sanctuary's nine writable point lights, shared by all room views.
///
/// Light transforms and attenuation are updated during composition and shading;
/// this collection borrows their storage for the loaded overlay's lifetime.
static WorldCoordRoomLights _gAcropolisSanctuaryRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gAcropolisSanctuaryPointLights),
    .pointLights           = _gAcropolisSanctuaryPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// GPU encodings and neutral RGB modulation for the room's authored view sprites.
enum {
    ACROPOLIS_SANCTUARY_SPRITE_PAGE_896      = getTPage(1, GPU_BLEND_AVERAGE, 896, 0),
    ACROPOLIS_SANCTUARY_SPRITE_PAGE_960      = getTPage(1, GPU_BLEND_AVERAGE, 960, 0),
    ACROPOLIS_SANCTUARY_SPRITE_CLUT_255      = getClut(0, 255),
    ACROPOLIS_SANCTUARY_SPRITE_CLUT_256      = getClut(0, 256),
    ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL = 128,
};

/// View 1's image-strip selection and terminal batch, with no source sprites.
static SpriteBatch _gAcropolisSanctuaryView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 2's 40 texture rectangles and sorting depths for cached room sprites.
///
/// Positions and dimensions are pixels; depths are unscaled ordering values.
/// The view's batches partition this array, which stays live with the room overlay.
static SpriteSource _gAcropolisSanctuaryView2SpriteSources[40] = {
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 56 } }, -48, 16, 883, { .fields = { 120, 200 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 88 } }, -56, 16, 701, { .fields = { 64, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 88 } }, -64, 32, 503, { .fields = { 72, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 72 } }, -72, 48, 480, { .fields = { 56, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 56 } }, -80, 64, 411, { .fields = { 96, 200 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 48 } }, -88, 72, 343, { .fields = { 24, 144 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 32 } }, -96, 88, 328, { .fields = { 56, 72 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 16 } }, -104, 104, 312, { .fields = { 0, 176 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 48 } }, 56, 16, 853, { .fields = { 24, 48 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 48 } }, 64, 16, 863, { .fields = { 24, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 56 } }, 72, 16, 843, { .fields = { 104, 200 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 64 } }, 80, 24, 736, { .fields = { 32, 136 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 64 } }, 88, 32, 691, { .fields = { 32, 72 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 64 } }, 96, 32, 638, { .fields = { 40, 136 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 16 } }, 104, 32, 626, { .fields = { 0, 72 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 32 } }, 104, 88, 516, { .fields = { 16, 48 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 72 } }, 112, 48, 504, { .fields = { 48, 72 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 72 } }, 120, 48, 522, { .fields = { 48, 144 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 64 } }, 128, 56, 512, { .fields = { 40, 72 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 56 } }, 136, 64, 480, { .fields = { 64, 200 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 56 } }, 144, 64, 450, { .fields = { 112, 200 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 56 } }, 152, 64, 117, { .fields = { 56, 200 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 112, 56 } }, -160, 0, 857, { .fields = { 64, 200 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 24, 16 } }, -64, -16, 852, { .fields = { 112, 160 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 16 } }, -48, 8, 853, { .fields = { 72, 88 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 16 } }, -64, 56, 874, { .fields = { 8, 80 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 80, 48 } }, 80, 0, 862, { .fields = { 80, 96 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 24, 56 } }, 56, -16, 859, { .fields = { 72, 200 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 24 } }, 64, 40, 852, { .fields = { 8, 168 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 16 } }, 80, 48, 867, { .fields = { 64, 88 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -64, 48, 510, { .fields = { 24, 192 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 8 } }, 104, 72, 512, { .fields = { 16, 192 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 72, 96 } }, -160, 24, 523, { .fields = { 56, 104 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 32, 24 } }, -88, 0, 510, { .fields = { 112, 48 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 24, 72 } }, -88, 24, 523, { .fields = { 32, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 24 } }, -88, 96, 383, { .fields = { 8, 144 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 48, 104 } }, 112, 16, 504, { .fields = { 80, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 40, 16 } }, 104, 0, 504, { .fields = { 96, 144 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 40 } }, 104, 16, 513, { .fields = { 48, 216 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 24, 48 } }, -160, 72, 192, { .fields = { 0, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
};

/// View 2's sprite ranges after the image-strip selector, ending at `SPRITE_BATCH_END`.
///
/// The leading zero-count record keeps the decoded background image.
static SpriteBatch _gAcropolisSanctuaryView2SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 22, 0, 0, { 3, 0 } },
    { 22, 8, 0, 0, { 0, 0 } },
    { 30, 9, 0, 0, { 2, 0 } },
    { 39, ARRAY_SIZE(_gAcropolisSanctuaryView2SpriteSources) - 39, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's 50 texture rectangles and sorting depths for cached room sprites.
///
/// Positions and dimensions are pixels; depths are unscaled ordering values.
/// The view's batches partition this array, which stays live with the room overlay.
static SpriteSource _gAcropolisSanctuaryView3SpriteSources[50] = {
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 48, 24 } }, -8, -24, 1316, { .fields = { 88, 24 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 32, 32 } }, 0, 0, 1351, { .fields = { 16, 56 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -40, 48, 974, { .fields = { 96, 128 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 8 } }, 120, 112, 317, { .fields = { 104, 248 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 48 } }, -48, 8, 970, { .fields = { 48, 56 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 48 } }, -56, 16, 877, { .fields = { 56, 208 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 56 } }, -64, 16, 789, { .fields = { 80, 168 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 64 } }, -72, 24, 726, { .fields = { 88, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 56 } }, -80, 32, 693, { .fields = { 80, 112 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 64 } }, -88, 40, 610, { .fields = { 88, 64 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 72 } }, -96, 40, 602, { .fields = { 112, 72 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 72 } }, -104, 48, 450, { .fields = { 112, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 72 } }, -112, 48, 510, { .fields = { 120, 176 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 64 } }, -120, 56, 427, { .fields = { 96, 64 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 64 } }, -128, 56, 427, { .fields = { 96, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 64 } }, -136, 56, 445, { .fields = { 104, 72 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 56 } }, -144, 64, 418, { .fields = { 72, 112 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 48 } }, -152, 72, 385, { .fields = { 56, 56 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 40 } }, -160, 80, 387, { .fields = { 112, 216 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 56 } }, 56, 16, 915, { .fields = { 88, 184 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 88 } }, 64, 16, 734, { .fields = { 120, 88 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 88 } }, 72, 32, 636, { .fields = { 120, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 72 } }, 80, 48, 512, { .fields = { 104, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 56 } }, 88, 64, 447, { .fields = { 80, 56 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 56 } }, 96, 64, 388, { .fields = { 88, 128 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 48 } }, 104, 72, 352, { .fields = { 72, 168 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 32 } }, 112, 88, 336, { .fields = { 72, 216 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 72, 40 } }, -152, -8, 1073, { .fields = { 112, 104 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 32, 16 } }, -152, 32, 681, { .fields = { 8, 144 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 32, 32 } }, -80, 0, 1043, { .fields = { 112, 56 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 16 } }, -56, -16, 1000, { .fields = { 40, 144 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 48 } }, -48, 0, 1100, { .fields = { 56, 112 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 24, 16 } }, 56, -16, 931, { .fields = { 112, 192 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 24 } }, 56, 0, 924, { .fields = { 120, 168 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 32 } }, 64, 24, 729, { .fields = { 104, 216 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 88, 40 } }, 72, 0, 937, { .fields = { 96, 208 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -104, -8, 626, { .fields = { 120, 248 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 64, 48 } }, -160, 16, 628, { .fields = { 8, 160 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 32, 16 } }, -112, 0, 631, { .fields = { 0, 88 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 24, 72 } }, -96, 16, 632, { .fields = { 96, 144 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 48 } }, -72, 32, 790, { .fields = { 64, 208 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 32, 24 } }, 72, 0, 562, { .fields = { 104, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 24 } }, 80, 80, 478, { .fields = { 120, 144 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 72, 48, 548, { .fields = { 112, 144 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 80, 56 } }, 80, 24, 585, { .fields = { 8, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 32 } }, -160, 88, 387, { .fields = { 96, 216 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 16 } }, 144, 64, 207, { .fields = { 32, 88 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 32 } }, 152, 88, 210, { .fields = { 80, 224 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 8 } }, 128, 112, 337, { .fields = { 96, 136 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 56 } }, -160, -8, 668, { .fields = { 64, 56 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
};

/// View 3's sprite ranges after the image-strip selector, ending at `SPRITE_BATCH_END`.
///
/// The leading zero-count record keeps the decoded background image.
static SpriteBatch _gAcropolisSanctuaryView3SpriteBatches[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 3, 0 } },
    { 2, 25, 0, 0, { 0, 0 } },
    { 27, 9, 0, 0, { 5, 0 } },
    { 36, 9, 0, 0, { 1, 0 } },
    { 45, 4, 0, 0, { 4, 0 } },
    { 49, ARRAY_SIZE(_gAcropolisSanctuaryView3SpriteSources) - 49, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's 46 texture rectangles and sorting depths for cached room sprites.
///
/// Positions and dimensions are pixels; depths are unscaled ordering values.
/// The view's batches partition this array, which stays live with the room overlay.
static SpriteSource _gAcropolisSanctuaryView4SpriteSources[46] = {
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 48, 56 } }, -128, -32, 1356, { .fields = { 64, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 24, 24 } }, -128, -24, 979, { .fields = { 0, 112 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 40 } }, -120, 0, 571, { .fields = { 48, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 72, 40 } }, -104, -16, 960, { .fields = { 104, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 16 } }, -104, 24, 575, { .fields = { 32, 40 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 32, 48 } }, -32, -16, 921, { .fields = { 40, 112 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 56 } }, 0, -8, 918, { .fields = { 80, 168 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 56, 64 } }, -160, 0, 571, { .fields = { 72, 104 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 64, 56 } }, -104, 8, 565, { .fields = { 8, 56 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 32, 64 } }, -40, 8, 601, { .fields = { 96, 168 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 24, 24 } }, -32, 72, 537, { .fields = { 8, 136 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 32, 24 } }, -160, 96, 187, { .fields = { 0, 224 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 32, 8 } }, -104, 112, 278, { .fields = { 40, 160 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -160, 112, 208, { .fields = { 72, 96 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 16 } }, 8, -16, 880, { .fields = { 0, 136 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 16 } }, 8, 32, 906, { .fields = { 8, 40 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 64 } }, 0, -16, 901, { .fields = { 112, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, -8, 0, 818, { .fields = { 24, 112 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 16 } }, -16, 8, 697, { .fields = { 0, 152 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 104 } }, -24, 0, 527, { .fields = { 120, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, -16, 80, 545, { .fields = { 0, 64 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 16 } }, -32, 88, 554, { .fields = { 0, 88 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 40 } }, -32, 8, 510, { .fields = { 112, 64 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, -40, 32, 460, { .fields = { 0, 40 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 32 } }, -48, 40, 420, { .fields = { 48, 224 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 32 } }, -56, 48, 391, { .fields = { 40, 224 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, -64, 64, 361, { .fields = { 120, 232 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 32 } }, -72, 64, 341, { .fields = { 56, 224 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, -80, 72, 325, { .fields = { 32, 112 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, -88, 80, 305, { .fields = { 32, 136 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, -96, 88, 288, { .fields = { 32, 224 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, -104, 88, 277, { .fields = { 112, 232 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, -112, 96, 263, { .fields = { 104, 232 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, -120, 96, 253, { .fields = { 96, 232 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 16 } }, -128, 104, 243, { .fields = { 0, 168 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 16 } }, -136, 104, 235, { .fields = { 16, 40 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 16 } }, -144, 104, 228, { .fields = { 48, 40 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 16 } }, -152, 104, 228, { .fields = { 56, 40 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 48 } }, -160, 8, 612, { .fields = { 104, 56 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 48 } }, -152, 8, 606, { .fields = { 96, 56 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 40 } }, -144, 8, 598, { .fields = { 72, 56 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 48 } }, -136, 0, 588, { .fields = { 80, 56 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 48 } }, -128, 0, 583, { .fields = { 88, 56 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 16 } }, -120, 32, 670, { .fields = { 24, 40 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 72, 56 } }, 56, -120, 354, { .fields = { 8, 168 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 32, 32 } }, 56, -80, 600, { .fields = { 64, 224 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
};

/// View 4's sprite ranges after the image-strip selector, ending at `SPRITE_BATCH_END`.
///
/// The leading zero-count record keeps the decoded background image.
static SpriteBatch _gAcropolisSanctuaryView4SpriteBatches[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 6, 0 } },
    { 1, 6, 0, 0, { 1, 0 } },
    { 7, 4, 0, 0, { 4, 0 } },
    { 11, 2, 0, 0, { 0, 0 } },
    { 13, 25, 0, 0, { 5, 0 } },
    { 38, 6, 0, 0, { 3, 0 } },
    { 44, 1, 0, 0, { 7, 0 } },
    { 45, ARRAY_SIZE(_gAcropolisSanctuaryView4SpriteSources) - 45, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5's 41 texture rectangles and sorting depths for cached room sprites.
///
/// Positions and dimensions are pixels; depths are unscaled ordering values.
/// The view's batches partition this array, which stays live with the room overlay.
static SpriteSource _gAcropolisSanctuaryView5SpriteSources[41] = {
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 16 } }, 112, 24, 600, { .fields = { 56, 240 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 40 } }, 120, 0, 754, { .fields = { 80, 56 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 48 } }, 128, 0, 607, { .fields = { 48, 48 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 48 } }, 136, 0, 611, { .fields = { 40, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 56 } }, 144, 0, 614, { .fields = { 72, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 56 } }, 152, 0, 619, { .fields = { 64, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -16, 40, 961, { .fields = { 112, 88 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 64 } }, -8, -16, 945, { .fields = { 104, 184 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 16 } }, 0, 32, 916, { .fields = { 48, 240 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 16 } }, 0, 0, 879, { .fields = { 40, 240 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 8, 0, 711, { .fields = { 0, 40 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 96 } }, 16, 0, 579, { .fields = { 120, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 40 } }, 8, 56, 651, { .fields = { 72, 56 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 16 } }, 24, 80, 563, { .fields = { 72, 240 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 32 } }, 24, 16, 572, { .fields = { 64, 56 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 32, 32, 545, { .fields = { 56, 56 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 40, 40, 534, { .fields = { 112, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 48, 48, 473, { .fields = { 120, 40 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 56, 56, 442, { .fields = { 0, 152 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 64, 64, 413, { .fields = { 0, 64 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 16 } }, 72, 72, 393, { .fields = { 64, 240 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 80, 72, 374, { .fields = { 120, 152 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 88, 72, 360, { .fields = { 120, 64 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 16 } }, 96, 80, 347, { .fields = { 56, 80 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 16 } }, 104, 80, 341, { .fields = { 24, 240 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 56 } }, 112, 64, 322, { .fields = { 80, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 16 } }, 104, 104, 329, { .fields = { 32, 240 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 56 } }, 120, 64, 318, { .fields = { 56, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 48 } }, 128, 72, 319, { .fields = { 48, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 136, 88, 314, { .fields = { 8, 152 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 72 } }, -8, -16, 882, { .fields = { 112, 184 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 24, 8 } }, 104, -24, 886, { .fields = { 88, 88 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 8 } }, 112, 40, 887, { .fields = { 24, 40 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 128, 56 } }, 8, -16, 882, { .fields = { 104, 184 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 112, 88 } }, 48, 0, 592, { .fields = { 16, 96 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 32, 88 } }, 16, 8, 582, { .fields = { 88, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 24, 24 } }, 24, 96, 478, { .fields = { 120, 128 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 48, 40 } }, 112, 80, 316, { .fields = { 120, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 40, 48 } }, -88, -96, 583, { .fields = { 8, 48 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 48, 32 } }, -160, -120, 202, { .fields = { 96, 96 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 24, 16 } }, -112, -120, 230, { .fields = { 80, 240 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
};

/// View 5's sprite ranges after the image-strip selector, ending at `SPRITE_BATCH_END`.
///
/// The leading zero-count record keeps the decoded background image.
static SpriteBatch _gAcropolisSanctuaryView5SpriteBatches[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 3, 0 } },
    { 6, 24, 0, 0, { 4, 0 } },
    { 30, 4, 0, 0, { 1, 0 } },
    { 34, 3, 0, 0, { 5, 0 } },
    { 37, 1, 0, 0, { 0, 0 } },
    { 38, 1, 0, 0, { 6, 0 } },
    { 39, ARRAY_SIZE(_gAcropolisSanctuaryView5SpriteSources) - 39, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 6's 32 texture rectangles and sorting depths for cached room sprites.
///
/// Positions and dimensions are pixels; depths are unscaled ordering values.
/// The view's batches partition this array, which stays live with the room overlay.
static SpriteSource _gAcropolisSanctuaryView6SpriteSources[32] = {
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 56, 56 } }, 88, -8, 1250, { .fields = { 40, 80 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 8 } }, 8, 24, 659, { .fields = { 88, 248 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 80 } }, -8, 0, 779, { .fields = { 96, 80 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 16 } }, -16, 64, 776, { .fields = { 120, 240 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 16 } }, 0, 64, 772, { .fields = { 40, 136 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 16 } }, 0, 16, 727, { .fields = { 40, 168 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 16 } }, 16, 16, 619, { .fields = { 40, 152 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 120 } }, 24, 0, 565, { .fields = { 120, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 120 } }, 32, 0, 510, { .fields = { 120, 120 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 56, 32, 473, { .fields = { 64, 224 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 64, 40, 422, { .fields = { 56, 136 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 72, 48, 403, { .fields = { 80, 224 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 80, 48, 389, { .fields = { 72, 224 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 88, 56, 350, { .fields = { 48, 136 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 96, 56, 337, { .fields = { 48, 160 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 104, 64, 325, { .fields = { 56, 160 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 112, 64, 315, { .fields = { 56, 224 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 120, 64, 307, { .fields = { 104, 232 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 128, 72, 275, { .fields = { 112, 232 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 136, 72, 265, { .fields = { 88, 224 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, 144, 72, 259, { .fields = { 88, 136 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 32 } }, 152, 72, 252, { .fields = { 112, 200 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 32 } }, 40, 16, 520, { .fields = { 104, 200 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 32 } }, 48, 24, 492, { .fields = { 96, 224 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 64 } }, -8, 8, 791, { .fields = { 88, 160 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 24, 48 } }, 8, 8, 813, { .fields = { 64, 136 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 112, 40 } }, 32, 8, 816, { .fields = { 104, 184 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 40, 16 } }, 96, 48, 811, { .fields = { 16, 240 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 32, 16 } }, 112, -8, 805, { .fields = { 24, 224 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 88 } }, 16, 32, 575, { .fields = { 104, 112 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 112 } }, 32, 8, 5021, { .fields = { 104, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 112, 80 } }, 48, 8, 455, { .fields = { 120, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
};

/// View 6's sprite ranges after the image-strip selector, ending at `SPRITE_BATCH_END`.
///
/// The leading zero-count record keeps the decoded background image.
static SpriteBatch _gAcropolisSanctuaryView6SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 3, 0 } },
    { 1, 23, 0, 0, { 0, 0 } },
    { 24, 5, 0, 0, { 2, 0 } },
    { 29, ARRAY_SIZE(_gAcropolisSanctuaryView6SpriteSources) - 29, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 7's 24 texture rectangles and sorting depths for cached room sprites.
///
/// Positions and dimensions are pixels; depths are unscaled ordering values.
/// The view's batches partition this array, which stays live with the room overlay.
static SpriteSource _gAcropolisSanctuaryView7SpriteSources[24] = {
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 8 } }, 8, 56, 893, { .fields = { 120, 248 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -24, 80, 735, { .fields = { 80, 184 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -8, 80, 804, { .fields = { 48, 136 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 112 } }, -72, 8, 488, { .fields = { 120, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 16 } }, -80, 16, 451, { .fields = { 64, 128 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 112 } }, -64, 8, 514, { .fields = { 120, 112 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, -56, 16, 618, { .fields = { 88, 104 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 16 } }, -48, 32, 670, { .fields = { 56, 128 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 16 } }, -40, 32, 701, { .fields = { 48, 120 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 16 } }, -32, 32, 726, { .fields = { 72, 128 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, -24, 16, 755, { .fields = { 120, 224 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 88 } }, -16, 8, 808, { .fields = { 112, 104 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, -8, 8, 876, { .fields = { 104, 160 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 56 } }, 0, 8, 875, { .fields = { 104, 104 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 24, 8 } }, -112, 0, 835, { .fields = { 88, 184 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 24 } }, -8, 48, 894, { .fields = { 80, 104 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 40 } }, -120, 24, 817, { .fields = { 96, 104 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 16 } }, -112, 48, 899, { .fields = { 40, 104 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 120, 40 } }, -112, 8, 887, { .fields = { 112, 144 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 144, 64 } }, -160, 8, 760, { .fields = { 104, 192 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 32, 8 } }, -48, 72, 750, { .fields = { 48, 184 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 24, 24 } }, -40, 80, 750, { .fields = { 56, 104 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 16 } }, -160, -8, 636, { .fields = { 80, 128 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 96, 104 } }, -160, 16, 634, { .fields = { 24, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
};

/// View 7's sprite ranges after the image-strip selector, ending at `SPRITE_BATCH_END`.
///
/// The leading zero-count record keeps the decoded background image.
/// The final nonterminal record is an empty range at the end of the source array.
static SpriteBatch _gAcropolisSanctuaryView7SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 0, 0 } },
    { 14, 5, 0, 0, { 3, 0 } },
    { 19, 4, 0, 0, { 2, 0 } },
    { 23, ARRAY_SIZE(_gAcropolisSanctuaryView7SpriteSources) - 23, 0, 0, { 4, 0 } },
    { ARRAY_SIZE(_gAcropolisSanctuaryView7SpriteSources), 0, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 8's 9 texture rectangles and sorting depths for cached room sprites.
///
/// Positions and dimensions are pixels; depths are unscaled ordering values.
/// The view's batches partition this array, which stays live with the room overlay.
static SpriteSource _gAcropolisSanctuaryView8SpriteSources[9] = {
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 32, 56 } }, -80, 24, 614, { .fields = { 96, 112 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 32, 56 } }, -48, 32, 607, { .fields = { 96, 56 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 32, 56 } }, -16, 40, 600, { .fields = { 96, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 8 } }, 80, -64, 766, { .fields = { 88, 128 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 24, 48 } }, -56, -96, 785, { .fields = { 72, 48 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 32, 48 } }, -32, -88, 663, { .fields = { 96, 168 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 32, 48 } }, 0, -80, 651, { .fields = { 64, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 32, 40 } }, 32, -72, 666, { .fields = { 96, 216 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 32 } }, 64, -64, 748, { .fields = { 80, 96 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
};

/// View 8's sprite ranges after the image-strip selector, ending at `SPRITE_BATCH_END`.
///
/// The leading zero-count record keeps the decoded background image.
static SpriteBatch _gAcropolisSanctuaryView8SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 1, 0 } },
    { 3, ARRAY_SIZE(_gAcropolisSanctuaryView8SpriteSources) - 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 9's 4 texture rectangles and sorting depths for cached room sprites.
///
/// Positions and dimensions are pixels; depths are unscaled ordering values.
/// The view's batches partition this array, which stays live with the room overlay.
static SpriteSource _gAcropolisSanctuaryView9SpriteSources[4] = {
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 48, 32 } }, -48, -48, 585, { .fields = { 80, 160 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 48, 32 } }, 0, -48, 591, { .fields = { 80, 192 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 40, 80 } }, -40, -16, 587, { .fields = { 88, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 40, 80 } }, 0, -16, 591, { .fields = { 88, 80 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
};

/// View 9's sprite ranges after the image-strip selector, ending at `SPRITE_BATCH_END`.
///
/// The leading zero-count record keeps the decoded background image.
static SpriteBatch _gAcropolisSanctuaryView9SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gAcropolisSanctuaryView9SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 10's image-strip selection and terminal batch, with no source sprites.
static SpriteBatch _gAcropolisSanctuaryView10SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 11's 8 texture rectangles and sorting depths for cached room sprites.
///
/// Positions and dimensions are pixels; depths are unscaled ordering values.
/// The view's batches partition this array, which stays live with the room overlay.
static SpriteSource _gAcropolisSanctuaryView11SpriteSources[8] = {
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 72, 40 } }, -152, -24, 230, { .fields = { 80, 96 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 56, 16 } }, -128, 16, 239, { .fields = { 96, 136 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 96, 64 } }, -152, 32, 227, { .fields = { 80, 192 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 56, 24 } }, -112, 96, 248, { .fields = { 72, 232 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 80, 96 } }, -56, 24, 302, { .fields = { 48, 136 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 64, 96 } }, 24, 16, 423, { .fields = { 16, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 48, 136 } }, 88, -40, 432, { .fields = { 80, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 24, 96 } }, 136, -8, 394, { .fields = { 24, 96 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
};

/// View 11's sprite ranges after the image-strip selector, ending at `SPRITE_BATCH_END`.
///
/// The leading zero-count record keeps the decoded background image.
static SpriteBatch _gAcropolisSanctuaryView11SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gAcropolisSanctuaryView11SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 12's 15 semitransparent sprites and 17 opaque sprites.
///
/// Scripts alternate the first six and next nine sprites; the final seventeen
/// stay visible. Positions and sizes are pixels; depths are unscaled ordering values.
static SpriteSource _gAcropolisSanctuaryView12SpriteSources[32] = {
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 40, 16 } }, -40, -112, 1294, { .fields = { 88, 88 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 64, 16 } }, -48, -96, 1195, { .fields = { 64, 136 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 72, 16 } }, -48, -80, 1110, { .fields = { 56, 72 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 64, 16 } }, -40, -64, 1036, { .fields = { 64, 104 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 40, 16 } }, -16, -48, 1026, { .fields = { 88, 120 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 24, 16 } }, 8, -32, 1021, { .fields = { 104, 184 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 8 } }, -8, -96, 1373, { .fields = { 120, 248 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 8 } }, 0, -40, 978, { .fields = { 112, 248 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 40, 16 } }, -40, -112, 1329, { .fields = { 88, 168 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 16 } }, -48, -96, 992, { .fields = { 112, 200 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 24 } }, -48, -80, 671, { .fields = { 112, 48 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 16 } }, -32, -64, 913, { .fields = { 112, 152 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 24 } }, 8, -80, 1379, { .fields = { 112, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 16 } }, 16, -56, 1231, { .fields = { 120, 216 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 24 } }, 16, -40, 1095, { .fields = { 112, 24 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 24, 16 } }, -24, -112, 1380, { .fields = { 88, 200 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 24, 16 } }, -8, -96, 1415, { .fields = { 88, 32 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 24, 16 } }, 8, -80, 1388, { .fields = { 88, 48 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 24, 16 } }, -48, -120, 1200, { .fields = { 104, 232 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 16, 16 } }, -48, -104, 1059, { .fields = { 96, 16 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 16, 16 } }, -56, -88, 907, { .fields = { 96, 152 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 32, 8 } }, -56, -72, 671, { .fields = { 80, 216 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 40, 8 } }, -56, -64, 664, { .fields = { 72, 224 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 40, 8 } }, -48, -56, 807, { .fields = { 72, 64 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 48, 8 } }, -40, -48, 832, { .fields = { 56, 184 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 32, 8 } }, -24, -40, 845, { .fields = { 72, 192 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 24, 8 } }, -16, -32, 852, { .fields = { 80, 232 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 8, 16 } }, 16, -64, 1293, { .fields = { 112, 216 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 24, 16 } }, 8, -48, 1164, { .fields = { 88, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 24, 8 } }, 8, -32, 1027, { .fields = { 72, 16 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 32, 8 } }, 0, -24, 904, { .fields = { 72, 240 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 24, 8 } }, 8, -16, 906, { .fields = { 88, 248 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
};

/// View 12's sprite ranges after the image-strip selector, ending at `SPRITE_BATCH_END`.
///
/// The leading zero-count record keeps the decoded background image.
/// Scripts toggle `hidden` on batches 1 and 2; batch 3 remains visible.
static SpriteBatch _gAcropolisSanctuaryView12SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 1, 0 } },
    { 6, 9, 0, 0, { 2, 0 } },
    { 15, ARRAY_SIZE(_gAcropolisSanctuaryView12SpriteSources) - 15, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Five opaque texture rectangles sorted in front of view 13's background.
///
/// Positions and dimensions are pixels about the view origin; UVs are texels
/// and depth is the unscaled sprite sorting value. Cached packets borrow these
/// sources for the loaded room's lifetime.
static SpriteSource _gAcropolisSanctuaryView13SpriteSources[5] = {
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 40, 80 } }, -160, 32, 488, { .fields = { 32, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 40, 56 } }, -120, 32, 499, { .fields = { 88, 184 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 56, 104 } }, -80, -8, 410, { .fields = { 72, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 56, 80 } }, -24, 40, 497, { .fields = { 72, 104 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 40, 24 } }, -64, 96, 305, { .fields = { 48, 184 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
};

/// View 13's complete sprite range after the decoded-background selector.
///
/// The zero-count first record keeps the background strips; the last ends
/// the list without selecting any sprites.
static SpriteBatch _gAcropolisSanctuaryView13SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gAcropolisSanctuaryView13SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Eight opaque texture rectangles sorted in front of view 14's background.
///
/// Positions and dimensions are pixels about the view origin; UVs are texels
/// and depth is the unscaled sprite sorting value. Cached packets borrow these
/// sources for the loaded room's lifetime.
static SpriteSource _gAcropolisSanctuaryView14SpriteSources[8] = {
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 48, 48 } }, -160, 72, 338, { .fields = { 80, 128 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 40, 16 } }, -8, 88, 325, { .fields = { 72, 232 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 16 } }, 32, 80, 325, { .fields = { 112, 232 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 24, 24 } }, 48, 72, 325, { .fields = { 104, 176 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 32, 72 } }, 72, 48, 325, { .fields = { 96, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 56, 56 } }, 104, 64, 350, { .fields = { 72, 72 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 48, 16 } }, -112, 96, 325, { .fields = { 80, 200 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 56, 16 } }, -64, 96, 325, { .fields = { 72, 216 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
};

/// View 14's complete sprite range after the decoded-background selector.
///
/// The zero-count first record keeps the background strips; the last ends
/// the list without selecting any sprites.
static SpriteBatch _gAcropolisSanctuaryView14SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gAcropolisSanctuaryView14SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 15's background-strip selector and terminal record, with no sprites.
///
/// Its address is also retained in the view's empty source slot; that slot
/// supplies no SpriteSource elements and is never dereferenced as sprites.
static SpriteBatch _gAcropolisSanctuaryView15SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 16's 38 source sprites: two translucent alternatives and an opaque tail.
///
/// Sprites 0..3 and 4..22 are script-selected alternatives; sprites 23..37
/// stay visible. Positions and dimensions are pixels about the view origin,
/// UVs are texels and depth is the unscaled sprite sorting value.
static SpriteSource _gAcropolisSanctuaryView16SpriteSources[38] = {
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 64, 112 } }, -48, -104, 674, { .fields = { 64, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 64, 88 } }, 16, -112, 618, { .fields = { 0, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 72, 80 } }, -56, 8, 622, { .fields = { 112, 88 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 72, 112 } }, 16, -24, 566, { .fields = { 56, 112 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 32 } }, 16, -112, 659, { .fields = { 112, 56 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 32, 8 } }, 32, -104, 637, { .fields = { 40, 8 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 8 } }, 56, -96, 616, { .fields = { 40, 248 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 32 } }, 64, -88, 581, { .fields = { 88, 224 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 32 } }, 72, -56, 554, { .fields = { 104, 224 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 16 } }, 72, -24, 544, { .fields = { 72, 128 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 24, 24 } }, 64, -8, 559, { .fields = { 72, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 24 } }, 72, 16, 546, { .fields = { 8, 232 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 48 } }, 80, 40, 506, { .fields = { 112, 168 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 32, 8 } }, -16, -104, 690, { .fields = { 40, 16 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 16 } }, -24, -96, 705, { .fields = { 24, 240 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 24, 32 } }, -48, -88, 716, { .fields = { 64, 224 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 32 } }, -48, -56, 697, { .fields = { 120, 224 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 8, 32 } }, -48, -24, 675, { .fields = { 56, 224 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 16, 72 } }, -56, 8, 651, { .fields = { 24, 168 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 32, 24 } }, -40, 64, 606, { .fields = { 56, 104 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 32, 8 } }, -8, 80, 578, { .fields = { 64, 48 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 24, 40 } }, 24, 48, 562, { .fields = { 88, 104 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_255, { .fields = { 32, 8 } }, 48, 80, 534, { .fields = { 40, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 48, 16 } }, 0, -120, 647, { .fields = { 64, 200 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 32, 24 } }, 48, -120, 599, { .fields = { 64, 176 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 24, 32 } }, 64, -96, 568, { .fields = { 88, 144 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 16, 56 } }, 72, -64, 554, { .fields = { 120, 168 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 24, 24 } }, 72, -8, 534, { .fields = { 72, 24 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 16, 64 } }, 80, 16, 513, { .fields = { 8, 168 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 32, 16 } }, -32, -112, 688, { .fields = { 56, 160 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 32, 16 } }, -48, -96, 699, { .fields = { 56, 144 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 32, 56 } }, -64, -80, 685, { .fields = { 96, 0 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 8, 16 } }, -56, -24, 672, { .fields = { 56, 88 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 16, 80 } }, -64, -8, 644, { .fields = { 40, 168 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 16, 24 } }, -72, 72, 604, { .fields = { 96, 176 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 48, 48 } }, -56, 72, 508, { .fields = { 64, 56 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_896, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 96, 40 } }, -8, 80, 475, { .fields = { 24, 216 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
    { ACROPOLIS_SANCTUARY_SPRITE_PAGE_960, ACROPOLIS_SANCTUARY_SPRITE_CLUT_256, { .fields = { 16, 32 } }, 88, 80, 481, { .fields = { 112, 224 } }, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL, 0 },
};

/// View 16's background selector, alternative ranges and persistent sprite tail.
///
/// Scripts toggle hidden on batches 1 and 2; batch 3 remains visible. Counts
/// cover all 38 sources and the final record ends the list. Retain writable
/// storage because visibility changes through the public view directory.
static SpriteBatch _gAcropolisSanctuaryView16SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 1, 0 } },
    { 4, 19, 0, 0, { 2, 0 } },
    { 23, ARRAY_SIZE(_gAcropolisSanctuaryView16SpriteSources) - 23, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gAcropolisSanctuarySpriteViews[16] = {
    { { .empty = _gAcropolisSanctuaryView1SpriteBatches }, _gAcropolisSanctuaryView1SpriteBatches, NULL },
    { { .elements = _gAcropolisSanctuaryView2SpriteSources }, _gAcropolisSanctuaryView2SpriteBatches, NULL },
    { { .elements = _gAcropolisSanctuaryView3SpriteSources }, _gAcropolisSanctuaryView3SpriteBatches, NULL },
    { { .elements = _gAcropolisSanctuaryView4SpriteSources }, _gAcropolisSanctuaryView4SpriteBatches, NULL },
    { { .elements = _gAcropolisSanctuaryView5SpriteSources }, _gAcropolisSanctuaryView5SpriteBatches, NULL },
    { { .elements = _gAcropolisSanctuaryView6SpriteSources }, _gAcropolisSanctuaryView6SpriteBatches, NULL },
    { { .elements = _gAcropolisSanctuaryView7SpriteSources }, _gAcropolisSanctuaryView7SpriteBatches, NULL },
    { { .elements = _gAcropolisSanctuaryView8SpriteSources }, _gAcropolisSanctuaryView8SpriteBatches, NULL },
    { { .elements = _gAcropolisSanctuaryView9SpriteSources }, _gAcropolisSanctuaryView9SpriteBatches, NULL },
    { { .empty = _gAcropolisSanctuaryView10SpriteBatches }, _gAcropolisSanctuaryView10SpriteBatches, NULL },
    { { .elements = _gAcropolisSanctuaryView11SpriteSources }, _gAcropolisSanctuaryView11SpriteBatches, NULL },
    { { .elements = _gAcropolisSanctuaryView12SpriteSources }, _gAcropolisSanctuaryView12SpriteBatches, NULL },
    { { .elements = _gAcropolisSanctuaryView13SpriteSources }, _gAcropolisSanctuaryView13SpriteBatches, NULL },
    { { .elements = _gAcropolisSanctuaryView14SpriteSources }, _gAcropolisSanctuaryView14SpriteBatches, NULL },
    { { .empty = _gAcropolisSanctuaryView15SpriteBatches }, _gAcropolisSanctuaryView15SpriteBatches, NULL },
    { { .elements = _gAcropolisSanctuaryView16SpriteSources }, _gAcropolisSanctuaryView16SpriteBatches, NULL },
};

ViewCamera gAcropolisSanctuaryViewCameras[16] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 6200, 0x7530, 8000 } }, 589 },
    { { { { 345, 0, 4081 }, { 546, 4059, -46 }, { -4044, 548, 342 } }, { 4961, 1400, 8451 } }, 230 },
    { { { { 559, 0, -4057 }, { -549, 4058, -75 }, { 4020, 554, 554 } }, { 9472, 1403, 8386 } }, 230 },
    { { { { 775, 0, -4021 }, { -623, 4046, -120 }, { 3973, 635, 766 } }, { 9087, 1276, 0x2D59 } }, 230 },
    { { { { 656, 0, 4043 }, { 635, 4045, -103 }, { -3992, 643, 648 } }, { 4960, 1280, 0x2D3A } }, 230 },
    { { { { -795, 0, -4017 }, { 65, 4095, -13 }, { 4017, -66, -795 } }, { 8714, 919, 4418 } }, 230 },
    { { { { -580, 0, 4054 }, { -150, 4093, -21 }, { -4051, -151, -579 } }, { 4402, 920, 4439 } }, 230 },
    { { { { 1227, 0, -3907 }, { -3609, 1569, -1134 }, { 1496, 3783, 470 } }, { 4250, 3534, 8412 } }, 230 },
    { { { { -186, 0, -4091 }, { -25, 4095, 1 }, { 4091, 25, -186 } }, { 6573, 667, 7896 } }, 230 },
    { { { { 1471, 0, 3822 }, { 944, 3969, -363 }, { -3704, 1012, 1425 } }, { 7110, 2190, 9120 } }, 680 },
    { { { { 3484, 0, -2153 }, { -524, 3972, -847 }, { 2089, 996, 3379 } }, { 8712, 1215, 9980 } }, 230 },
    { { { { 2699, 0, 3080 }, { -2257, 2787, 1978 }, { -2096, -3001, 1837 } }, { 8528, 459, 9285 } }, 230 },
    { { { { 4014, 0, -810 }, { -213, 3951, -1058 }, { 782, 1079, 3873 } }, { 5552, 1588, 0x2BBF } }, 230 },
    { { { { 4014, 0, -810 }, { -213, 3951, -1058 }, { 782, 1079, 3873 } }, { 8025, 1652, 6179 } }, 230 },
    { { { { 4071, 0, -446 }, { -158, 3830, -1440 }, { 417, 1449, 3808 } }, { 4926, 1150, 9330 } }, 230 },
    { { { { 1261, 0, -3896 }, { 704, 4028, 228 }, { 3832, -740, 1240 } }, { 0x310A, 4285, 8624 } }, 207 },
};

/// Cue-2 script bases in the loaded type-1 bank; cue 1 uses the next entry.
enum {
    ACROPOLIS_SANCTUARY_FOOTSTEP_CLASS1_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x2D,
    ACROPOLIS_SANCTUARY_FOOTSTEP_CLASS1_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x2F,
    ACROPOLIS_SANCTUARY_FOOTSTEP_CLASS2_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x11,
    ACROPOLIS_SANCTUARY_FOOTSTEP_CLASS2_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x13,
};

/// Surface class 1's walk/run cue pairs; scripted jumps use the walking pair.
static WorldCollisionFootstepSounds _gAcropolisSanctuarySurfaceClass1FootstepSounds = {
    .walk         = ACROPOLIS_SANCTUARY_FOOTSTEP_CLASS1_WALK,
    .run          = ACROPOLIS_SANCTUARY_FOOTSTEP_CLASS1_RUN,
    .scriptedJump = ACROPOLIS_SANCTUARY_FOOTSTEP_CLASS1_WALK,
};

/// Surface class 2's walk/run cue pairs; scripted jumps use the walking pair.
static WorldCollisionFootstepSounds _gAcropolisSanctuarySurfaceClass2FootstepSounds = {
    .walk         = ACROPOLIS_SANCTUARY_FOOTSTEP_CLASS2_WALK,
    .run          = ACROPOLIS_SANCTUARY_FOOTSTEP_CLASS2_RUN,
    .scriptedJump = ACROPOLIS_SANCTUARY_FOOTSTEP_CLASS2_WALK,
};

/// Blocking, impact-enabled surface shared by classes 0 and 3..7, without footsteps.
static WorldCollisionSurfaceProperties _gAcropolisSanctuaryDefaultSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Class 1's blocking, impact-enabled surface with its own footstep cue pairs.
static WorldCollisionSurfaceProperties _gAcropolisSanctuarySurfaceClass1Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gAcropolisSanctuarySurfaceClass1FootstepSounds,
};

/// Class 2's blocking, impact-enabled surface with its own footstep cue pairs.
static WorldCollisionSurfaceProperties _gAcropolisSanctuarySurfaceClass2Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gAcropolisSanctuarySurfaceClass2FootstepSounds,
};

WorldCollisionSurfaceProperties* gAcropolisSanctuarySurfacePropertiesByClass[8] = {
    &_gAcropolisSanctuaryDefaultSurfaceProperties,
    &_gAcropolisSanctuarySurfaceClass1Properties,
    &_gAcropolisSanctuarySurfaceClass2Properties,
    &_gAcropolisSanctuaryDefaultSurfaceProperties,
    &_gAcropolisSanctuaryDefaultSurfaceProperties,
    &_gAcropolisSanctuaryDefaultSurfaceProperties,
    &_gAcropolisSanctuaryDefaultSurfaceProperties,
    &_gAcropolisSanctuaryDefaultSurfaceProperties,
};

enum { ACROPOLIS_SANCTUARY_AREA_UPDATE_SET_MAP_MARK = 1 };

/// Saved Acropolis layout and map-mark updates applied on the first warp-3 arrival.
///
/// Rows carry common layouts and mode-group policies for the forked road,
/// observatory, promenade and roof garden. Applying a row discards that area's
/// saved enemy poses. In save modes 0..3 the leading unconditional rows keep
/// the update walk accepting later rows, so the last layout for each area wins.
/// The list ends at AREA_APPLY_END and is borrowed only during the call.
static const AreaApplyRec _gAcropolisSanctuaryArrivalAreaUpdates[] __attribute__((section(".data"))) = {
    { GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PATIO, 4, AREA_APPLY_MODE_ALWAYS | ACROPOLIS_SANCTUARY_AREA_UPDATE_SET_MAP_MARK },
    { GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_CAFETERIA, 4, AREA_APPLY_MODE_ALWAYS | ACROPOLIS_SANCTUARY_AREA_UPDATE_SET_MAP_MARK },
    { GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_HALLWAY, 4, AREA_APPLY_MODE_ALWAYS | ACROPOLIS_SANCTUARY_AREA_UPDATE_SET_MAP_MARK },
    { GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_FORKED_ROAD, 3, AREA_APPLY_MODE_REPLAY_SCAVENGER | ACROPOLIS_SANCTUARY_AREA_UPDATE_SET_MAP_MARK },
    { GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_FORKED_ROAD, 7, AREA_APPLY_MODE_BOUNTY_NIGHTMARE | ACROPOLIS_SANCTUARY_AREA_UPDATE_SET_MAP_MARK },
    { GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_OBSERVATORY, 2, AREA_APPLY_MODE_REPLAY_SCAVENGER | ACROPOLIS_SANCTUARY_AREA_UPDATE_SET_MAP_MARK },
    { GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_OBSERVATORY, 7, AREA_APPLY_MODE_BOUNTY_NIGHTMARE | ACROPOLIS_SANCTUARY_AREA_UPDATE_SET_MAP_MARK },
    { GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PROMENADE, 2, AREA_APPLY_MODE_REPLAY_SCAVENGER | ACROPOLIS_SANCTUARY_AREA_UPDATE_SET_MAP_MARK },
    { GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_PROMENADE, 7, AREA_APPLY_MODE_BOUNTY_NIGHTMARE | ACROPOLIS_SANCTUARY_AREA_UPDATE_SET_MAP_MARK },
    { GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_ROOF_GARDEN, 7, AREA_APPLY_MODE_BOUNTY_NIGHTMARE | ACROPOLIS_SANCTUARY_AREA_UPDATE_SET_MAP_MARK },
    { AREA_APPLY_END, 0, 0, 0 },
};

static TmdBone _gAcropolisSanctuaryModel090F0Skeleton[3] = {
#include "assets/acropolis_sanctuary_model_090F0_skeleton.inc"
};

static u32 _gAcropolisSanctuaryModel090F0PartVerts[3] = {
#include "assets/acropolis_sanctuary_model_090F0_partVerts.inc"
};

static SVECTOR _gAcropolisSanctuaryModel090F0Verts[56] = {
#include "assets/acropolis_sanctuary_model_090F0_verts.inc"
};

static SVECTOR _gAcropolisSanctuaryModel090F0Normals[6] = {
#include "assets/acropolis_sanctuary_model_090F0_normals.inc"
};

static u32 _gAcropolisSanctuaryModel090F0Stream[215] = {
#include "assets/acropolis_sanctuary_model_090F0_stream.inc"
};

TmdSource gAcropolisSanctuaryModel090F0 = {
    0,
    1768,
    0,
    3,
    _gAcropolisSanctuaryModel090F0PartVerts,
    _gAcropolisSanctuaryModel090F0Verts,
    _gAcropolisSanctuaryModel090F0Normals,
    _gAcropolisSanctuaryModel090F0Skeleton,
    _gAcropolisSanctuaryModel090F0Stream,
};

static TmdBone _gAcropolisSanctuaryModel09584Skeleton[1] = {
#include "assets/acropolis_sanctuary_model_09584_skeleton.inc"
};

static u32 _gAcropolisSanctuaryModel09584PartVerts[1] = {
#include "assets/acropolis_sanctuary_model_09584_partVerts.inc"
};

static SVECTOR _gAcropolisSanctuaryModel09584Verts[19] = {
#include "assets/acropolis_sanctuary_model_09584_verts.inc"
};

static SVECTOR _gAcropolisSanctuaryModel09584Normals[11] = {
#include "assets/acropolis_sanctuary_model_09584_normals.inc"
};

static u32 _gAcropolisSanctuaryModel09584Stream[73] = {
#include "assets/acropolis_sanctuary_model_09584_stream.inc"
};

TmdSource gAcropolisSanctuaryModel09584 = {
    0,
    440,
    0,
    1,
    _gAcropolisSanctuaryModel09584PartVerts,
    _gAcropolisSanctuaryModel09584Verts,
    _gAcropolisSanctuaryModel09584Normals,
    _gAcropolisSanctuaryModel09584Skeleton,
    _gAcropolisSanctuaryModel09584Stream,
};

/// Unread word after the model records; its representation and role are unproven.
static u32 D_acropolis_sanctuary_80186C8C = 0xB000000;

/// Borrowed sanctuary cutscene task published for its event-script cues.
///
/// Published after work allocation and player capture. Cue callbacks require
/// the task and initialized work to remain live. Completion leaves this pointer
/// set while default teardown frees the work; no cue may use it afterwards.
/// A later successful cutscene initialization replaces it.
static Task* _gAcropolisSanctuaryCutsceneTask = NULL;

SVECTOR ActorContact_ScratchPosition = { 0, 0, 0, 0 };

/// Disables the sanctuary's nine weapon-CAP collision triggers.
///
/// Requires the live writable 17-entry trigger array; preserves every other flag.
static inline void _acropolisSanctuaryDisableWeaponTriggers(void)
{
    s32                    disabledTriggerMask;
    WorldCollisionTrigger* trigger6;
    WorldCollisionTrigger* trigger9;
    WorldCollisionTrigger* trigger10;
    WorldCollisionTrigger* trigger11;
    WorldCollisionTrigger* trigger12;
    WorldCollisionTrigger* trigger13;
    WorldCollisionTrigger* trigger14;
    WorldCollisionTrigger* trigger15;
    WorldCollisionTrigger* trigger16;

    disabledTriggerMask = ~WORLD_COLLISION_TRIGGER_ENABLED;
    trigger6            = &_gAcropolisSanctuaryActionTriggers[6];
    trigger9            = &_gAcropolisSanctuaryActionTriggers[9];
    trigger10           = &_gAcropolisSanctuaryActionTriggers[10];
    trigger11           = &_gAcropolisSanctuaryActionTriggers[11];
    trigger12           = &_gAcropolisSanctuaryActionTriggers[12];
    trigger13           = &_gAcropolisSanctuaryActionTriggers[13];
    trigger14           = &_gAcropolisSanctuaryActionTriggers[14];
    trigger15           = &_gAcropolisSanctuaryActionTriggers[15];
    trigger16           = &_gAcropolisSanctuaryActionTriggers[16];

    trigger6->flags  &= disabledTriggerMask;
    trigger9->flags  &= disabledTriggerMask;
    trigger10->flags &= disabledTriggerMask;
    trigger11->flags &= disabledTriggerMask;
    trigger12->flags &= disabledTriggerMask;
    trigger13->flags &= disabledTriggerMask;
    trigger14->flags &= disabledTriggerMask;
    trigger15->flags &= disabledTriggerMask;
    trigger16->flags &= disabledTriggerMask;
}

/// Starts the sanctuary warp-3 arrival scene and disables completed interactions.
///
/// Runs in the room task's active state with live saved flags and collision
/// triggers. First arrival advances bridge progress, applies saved placement
/// updates and sets scene/objective/dialogue progress. Object state 2 at index
/// 28 disables the nine weapon-CAP triggers; this callback never reenables them.
static void _acropolisSanctuaryUpdateRoomProgress(Task* unusedTask)
{
    enum {
        ACROPOLIS_SANCTUARY_SCENE_ARRIVAL_WARP          = 3,
        ACROPOLIS_SANCTUARY_WEAPON_INTERACTION_OBJECT   = 28,
        ACROPOLIS_SANCTUARY_WEAPON_INTERACTION_COMPLETE = 2,
        ACROPOLIS_SANCTUARY_BRIDGE_PROGRESS_ENTERED     = 2,
        ACROPOLIS_SANCTUARY_ARRIVAL_SCENE_EVENT         = 6,
        ACROPOLIS_SANCTUARY_OBSERVATORY_ROUTE_COMPLETE  = 5,
        ACROPOLIS_SANCTUARY_ARRIVAL_OBJECTIVE           = 6,
        ACROPOLIS_SANCTUARY_ARRIVAL_DIALOGUE            = 5,
    };

    if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_BRIDGE_PROGRESS) == 0 && gGameSession->location.loc.warp == ACROPOLIS_SANCTUARY_SCENE_ARRIVAL_WARP) {
        gameFlagSetNibble(GAME_FLAG_ACROPOLIS_BRIDGE_PROGRESS, ACROPOLIS_SANCTUARY_BRIDGE_PROGRESS_ENTERED);
        evsStartScriptWithSkip(_gAcropolisSanctuaryArrivalScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAcropolisSanctuaryArrivalSkipScript);
        areaApplySavedUpdates(_gAcropolisSanctuaryArrivalAreaUpdates);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = ACROPOLIS_SANCTUARY_ARRIVAL_SCENE_EVENT;
        gameFlagSetNibble(GAME_FLAG_OBSERVATORY_ROUTE_PROGRESS, ACROPOLIS_SANCTUARY_OBSERVATORY_ROUTE_COMPLETE);
        gameFlagSetNibble(GAME_FLAG_OBSERVATORY_EXIT_USED, 1);
        gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, ACROPOLIS_SANCTUARY_ARRIVAL_OBJECTIVE);
        gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
        gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, ACROPOLIS_SANCTUARY_ARRIVAL_DIALOGUE);
    }
    if (areaGetCurrentObjectState(ACROPOLIS_SANCTUARY_WEAPON_INTERACTION_OBJECT) == ACROPOLIS_SANCTUARY_WEAPON_INTERACTION_COMPLETE) {
        _acropolisSanctuaryDisableWeaponTriggers();
    }
}

/// Resolves an executed exit to the promenade and latches its first use.
///
/// Receives `ROOM_EVENT_MESSAGE_RESOLVE` with live request/reply records, which
/// may alias. Copies all eight bytes, including in query mode; only execution
/// latches the sanctuary event at 2 and stores object state 2 for index 0x13.
/// Execution selects promenade room 1 before bridge progress, room 2 after it.
/// Returns 1 to continue the transition; other destinations pass through.
static s32 _acropolisSanctuaryResolvePromenadeExit(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        ACROPOLIS_SANCTUARY_EVENT_LATCH_NONE             = 0,
        ACROPOLIS_SANCTUARY_EVENT_LATCH_PROMENADE_EXIT   = 2,
        ACROPOLIS_SANCTUARY_PROMENADE_EXIT_OBJECT_INDEX  = 0x13,
        ACROPOLIS_SANCTUARY_PROMENADE_EXIT_OBJECT_STATE  = 2,
        ACROPOLIS_SANCTUARY_PROMENADE_ROOM_BEFORE_BRIDGE = 1,
        ACROPOLIS_SANCTUARY_PROMENADE_ROOM_AFTER_BRIDGE  = 2,
        ACROPOLIS_SANCTUARY_EXIT_RESOLVED                = 1,
    };
    s32 destinationRoom;

    *reply = *request;
    if (request->areaId == GAME_AREA_ACROPOLIS_PROMENADE) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            if (gameFlagGetNibble(GAME_FLAG_SANCTUARY_EVENT_LATCH) == ACROPOLIS_SANCTUARY_EVENT_LATCH_NONE) {
                gameFlagSetNibble(GAME_FLAG_SANCTUARY_EVENT_LATCH, ACROPOLIS_SANCTUARY_EVENT_LATCH_PROMENADE_EXIT);
                areaSetCurrentObjectState(ACROPOLIS_SANCTUARY_PROMENADE_EXIT_OBJECT_INDEX, ACROPOLIS_SANCTUARY_PROMENADE_EXIT_OBJECT_STATE);
            }
        }
        if (request->areaId == GAME_AREA_ACROPOLIS_PROMENADE && request->queryOnly == ROOM_EVENT_EXECUTE) {
            destinationRoom = gameFlagGetNibble(GAME_FLAG_ACROPOLIS_BRIDGE_PROGRESS);
            if (destinationRoom == 0) {
                destinationRoom = ACROPOLIS_SANCTUARY_PROMENADE_ROOM_BEFORE_BRIDGE;
            } else {
                destinationRoom = ACROPOLIS_SANCTUARY_PROMENADE_ROOM_AFTER_BRIDGE;
            }
            reply->room = destinationRoom;
        }
    }
    return ACROPOLIS_SANCTUARY_EXIT_RESOLVED;
}

/// Refuses every key-item use in the sanctuary, returning 0 without side effects.
///
/// Receives `ACROPOLIS_SANCTUARY_MESSAGE_USE_KEY_ITEM` with an item ID in the
/// first payload word; all parameters are unused. The item menu treats the
/// result as "cannot use now" and retains the item.
static s32 _acropolisSanctuaryRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unused)
{
    enum { ACROPOLIS_SANCTUARY_KEY_ITEM_REFUSED = 0 };

    return ACROPOLIS_SANCTUARY_KEY_ITEM_REFUSED;
}

/// Starts the blocker interaction script for room command 0 while it is uncleared.
///
/// Receives `ROOM_MESSAGE_COMMAND` with an integer selector; the second payload
/// and receiver are unused. Other commands do nothing. Always returns 0.
static s32 _acropolisSanctuaryHandleRoomCommand(Task* task, s32 messageId, s32 command, s32 unused)
{
    enum { ACROPOLIS_SANCTUARY_ROOM_COMMAND_BLOCKER = 0 };

    if (command == ACROPOLIS_SANCTUARY_ROOM_COMMAND_BLOCKER && gameFlagGetNibble(GAME_FLAG_SANCTUARY_BLOCKER_CLEARED) == 0) {
        evsStartScript(_gAcropolisSanctuaryBlockerScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
    }
    return 0;
}

/// Starts the sanctuary cutscene on the first room action 1.
///
/// `DIRECTION_MESSAGE_ROOM_ACTION` borrows the four-byte request until return.
/// Reads only actionId; the argument byte and zero second word are ignored.
/// Latches the event before spawning, so a failed spawn is not retried.
/// Returns 0 for every action and retains no request pointer.
static s32 _acropolisSanctuaryHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum {
        ACROPOLIS_SANCTUARY_ROOM_ACTION_CUTSCENE = 1,
        ACROPOLIS_SANCTUARY_EVENT_LATCH_CUTSCENE = 1,
    };
    if (request->actionId == ACROPOLIS_SANCTUARY_ROOM_ACTION_CUTSCENE && gameFlagGetNibble(GAME_FLAG_SANCTUARY_EVENT_LATCH) == 0) {
        gameFlagSetNibble(GAME_FLAG_SANCTUARY_EVENT_LATCH, ACROPOLIS_SANCTUARY_EVENT_LATCH_CUTSCENE);
        taskSpawnFromTable(&_gAcropolisSanctuaryCutsceneTaskDesc, 0, 0, 0);
    }
    return 0;
}

/// Applies an event script's packed view-sprite batch selection.
///
/// Bits 0..7 hold a 1-based view ID; bits 8..15 select batch 1 when zero or
/// batch 2 otherwise. Higher bits are ignored. The sanctuary scripts pass
/// views 12 and 16, whose live batch lists contain both alternatives.
static void _acropolisSanctuaryApplyScriptSpriteSelection(u32 packedSelection)
{
    _acropolisSanctuarySelectViewSpriteBatch(
        (packedSelection >> ACROPOLIS_SANCTUARY_SCRIPT_SPRITE_BATCH_SHIFT) & ACROPOLIS_SANCTUARY_SCRIPT_SPRITE_FIELD_MASK,
        packedSelection & ACROPOLIS_SANCTUARY_SCRIPT_SPRITE_FIELD_MASK);
}

/// Restores the player weapon animation after the scripted sanctuary poses.
///
/// Requires a live player task and initialized weapon animation bank. Equipped
/// weapon index 2 uses clip 55; the other weapons use clip 7. The persistent
/// request records are stamped with the active bank index before dispatch.
static void _acropolisSanctuaryRestorePlayerWeaponAnimation(void)
{
    enum { ACROPOLIS_SANCTUARY_WEAPON_SPECIAL_RESTORE = 2 };

    if (gPlayerStatus.weapon == ACROPOLIS_SANCTUARY_WEAPON_SPECIAL_RESTORE) {
        playerActorWriteWeaponAnimationBankIndex(&_gAcropolisSanctuaryPlayerSpecialWeaponRestoreRequest.source.index);
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &_gAcropolisSanctuaryPlayerSpecialWeaponRestoreRequest, 0);
    } else {
        playerActorWriteWeaponAnimationBankIndex(&_gAcropolisSanctuaryPlayerWeaponRestoreRequest.source.index);
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &_gAcropolisSanctuaryPlayerWeaponRestoreRequest, 0);
    }
}

/// Registers the sanctuary room task and restores its actor and blocker placement.
///
/// Called in state 0. Advances to state 1, enables placed actor 1 while the
/// blocker has not reached cleared state 1, and restores its pose and position
/// when bridge progress is nonzero. Rebuilds the live blocker collision last.
static void _acropolisSanctuaryInitRoomTask(Task* task)
{
    enum {
        ACROPOLIS_SANCTUARY_BLOCKER_ACTOR_INDEX = 1,
        ACROPOLIS_SANCTUARY_BLOCKER_ACTOR_DRAW  = 1,
        ACROPOLIS_SANCTUARY_BLOCKER_CLEARED     = 1,
    };
    Task* placedActor;

    task->msgTable = _gAcropolisSanctuaryRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = task->state + 1;
    if (gameFlagGetNibble(GAME_FLAG_SANCTUARY_BLOCKER_CLEARED) != ACROPOLIS_SANCTUARY_BLOCKER_CLEARED) {
        placedActor = sceneFindPlacedActor(ACROPOLIS_SANCTUARY_BLOCKER_ACTOR_INDEX);
        sceneSetPlacedActorDrawMode(ACROPOLIS_SANCTUARY_BLOCKER_ACTOR_INDEX, ACROPOLIS_SANCTUARY_BLOCKER_ACTOR_DRAW);
        if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_BRIDGE_PROGRESS) != 0 && placedActor != NULL) {
            TASK_MESSAGE_DISPATCH_POINTER(placedActor, ACTOR_MESSAGE_PLAY_ANIMATION, &_gAcropolisSanctuaryRupertRestoredClip3Request, 0);
            TASK_MESSAGE_DISPATCH_POINTER(placedActor, ACTOR_MESSAGE_PLACE, &_gAcropolisSanctuaryRupertRestoredPlacement, 0);
        }
    }
    _acropolisSanctuaryPlaceBlockerCollision();
}

void acropolisSanctuaryRoomTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = _gAcropolisSanctuaryRoomTaskStates;
    handlers.funcs[task->state](task);
}

/// Installs the cutscene animation set and resets the player's first clip.
///
/// Requires initialized cutscene work. A NULL captured player skips dispatch;
/// otherwise the player task must be live. The stack request is consumed
/// synchronously, while the installed animation-set table survives playback.
/// Enables world collision; the retained 15-frame argument is ignored on reset.
static inline void _acropolisSanctuaryCutsceneInstallPlayerAnimation(Task* task)
{
    enum { ACROPOLIS_SANCTUARY_CUTSCENE_ANIMATION_BLEND_FRAMES = 15 };
    AnimationPlayRequest             request;
    _AcropolisSanctuaryCutsceneWork* work;

    work = task->work;
    if (work->playerTask != NULL) {
        request.source.sets          = _gAcropolisSanctuaryCutscenePlayerAnimationSets;
        request.animationId          = 0;
        request.blend                = ANIMATION_BLEND_RESET;
        request.blendFrames          = ACROPOLIS_SANCTUARY_CUTSCENE_ANIMATION_BLEND_FRAMES;
        request.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
        TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &request, 0);
    }
}

/// Places the player at the sanctuary cutscene mark with zero Euler angles.
///
/// The captured player must be live through synchronous placement dispatch.
/// The mark (-7600, 0, -4400) uses whole world-coordinate units; the unused
/// vector components have no placement meaning.
static inline void _acropolisSanctuaryCutscenePlacePlayer(_AcropolisSanctuaryCutsceneWork* work)
{
    ActorTransform placement;

    placement.pos.vx = -0x1DB0;
    placement.pos.vy = 0;
    placement.pos.vz = -0x1130;
    placement.rot.vx = 0;
    placement.rot.vy = 0;
    placement.rot.vz = 0;
    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, &placement, 0);
}

/// Applies each pending script request for the cutscene's player placement once.
///
/// Requires initialized work and a live captured player in place-player phase.
/// Removes player equipment before installing animation and placing the player,
/// selects saved view 14, then marks the request applied. Initial/hold phases
/// and already-applied requests have no effects.
static inline void _acropolisSanctuaryCutsceneApplyPlacement(Task* task)
{
    enum {
        ACROPOLIS_SANCTUARY_VIEW_CUTSCENE_PLAYER = 14,
    };
    _AcropolisSanctuaryCutsceneWork* work;

    work = task->work;
    switch (work->phase) {
        case ACROPOLIS_SANCTUARY_CUTSCENE_PHASE_INITIAL:
        case ACROPOLIS_SANCTUARY_CUTSCENE_PHASE_HOLD:
            break;
        case ACROPOLIS_SANCTUARY_CUTSCENE_PHASE_PLACE_PLAYER:
            if (work->placementApplied == ACROPOLIS_SANCTUARY_CUTSCENE_PLACEMENT_PENDING) {
                playerActorRemoveEquipment();
                _acropolisSanctuaryCutsceneInstallPlayerAnimation(task);
                _acropolisSanctuaryCutscenePlacePlayer(work);
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = ACROPOLIS_SANCTUARY_VIEW_CUTSCENE_PLAYER;
                work->placementApplied                                     = work->placementApplied + 1;
            }
            break;
    }
}

/// Runs the sanctuary player cutscene, then reloads the roof garden.
///
/// Starts bodyless in state 0 once wheel/display transitions are clear. Owns
/// `_AcropolisSanctuaryCutsceneWork` and publishes the singleton for script cues.
/// Selects the equipped-weapon animation, starts the event/skip pair, then
/// applies placement requests through the cutscene helpers. Event completion
/// selects roof-garden room 1, warp 2 and queues a captured-frame reload.
/// Requires live player and overlay resources through handoff. Allocation
/// failure kills the task but still falls through into setup in the original.
static void _acropolisSanctuaryCutsceneTask(Task* task)
{
    enum {
        ACROPOLIS_SANCTUARY_CUTSCENE_INITIAL             = 0,
        ACROPOLIS_SANCTUARY_CUTSCENE_RUNNING             = 1,
        ACROPOLIS_SANCTUARY_CUTSCENE_WEAPON_BLEND_FRAMES = 15,
        ACROPOLIS_SANCTUARY_PRIMARY_WEAPON_BANK_BASE     = 1,
        ACROPOLIS_SANCTUARY_ALTERNATE_WEAPON_BANK_BASE   = 34,
        ACROPOLIS_SANCTUARY_CUTSCENE_EXIT_WARP           = 2,
        ACROPOLIS_SANCTUARY_CUTSCENE_EXIT_ROOM           = 1,
    };
    AnimationPlayRequest             request;
    _AcropolisSanctuaryCutsceneWork* allocatedWork;
    _AcropolisSanctuaryCutsceneWork* animationWork;
    s32                              weaponBankIndex;
    s32                              equippedWeapon;

    switch (task->state) {
        case ACROPOLIS_SANCTUARY_CUTSCENE_INITIAL:
            if (gAttachmentState.mode != ATTACHMENT_MODE_WHEEL && gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                allocatedWork = memCalloc(sizeof(*allocatedWork), 0);
                task->work    = allocatedWork;
                if (allocatedWork == NULL) {
                    // The original continues into animation setup after this failure.
                    taskKill(task);
                } else {
                    memFillBytes(allocatedWork, 0, sizeof(*allocatedWork));
                    allocatedWork->playerTask        = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                    _gAcropolisSanctuaryCutsceneTask = task;
                }
                animationWork   = task->work;
                equippedWeapon  = gPlayerStatus.weapon;
                weaponBankIndex = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? equippedWeapon + ACROPOLIS_SANCTUARY_PRIMARY_WEAPON_BANK_BASE : equippedWeapon + ACROPOLIS_SANCTUARY_ALTERNATE_WEAPON_BANK_BASE;

                request.source.index         = weaponBankIndex;
                request.animationId          = 1;
                request.blend                = ANIMATION_BLEND_INTERPOLATE;
                request.blendFrames          = ACROPOLIS_SANCTUARY_CUTSCENE_WEAPON_BLEND_FRAMES;
                request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                // Keep the reloaded work alias separate for the animation dispatch.
                TASK_MESSAGE_DISPATCH_POINTER(animationWork->playerTask, ANIMATION_MESSAGE_PLAY, &request, 0);
                sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SANCTUARY, 7), 0, 0);
                evsStartScriptWithSkip(_gAcropolisSanctuaryDepartureScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAcropolisSanctuaryDepartureSkipScript);
                task->state = task->state + 1;
            }
            break;

        case ACROPOLIS_SANCTUARY_CUTSCENE_RUNNING:
            // Event completion takes precedence over pending placement cues.
            if (gGameSession->eventState == 0) {
                sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = GAME_AREA_ACROPOLIS_ROOF_GARDEN;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = GAME_STAGE_ACROPOLIS;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = ACROPOLIS_SANCTUARY_CUTSCENE_EXIT_WARP;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = ACROPOLIS_SANCTUARY_CUTSCENE_EXIT_ROOM;
                gDisplayState.spriteVariant                                 = 1;
                taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
                taskKill(task);
                break;
            }
            _acropolisSanctuaryCutsceneApplyPlacement(task);
            break;
    }
}

/// Accepts a script cue for player placement or the cutscene sound transition.
///
/// The published cutscene task and its work must be live, even for an unknown
/// cue. Cue 0 holds and resets placement, cue 1 arms and resets placement, and
/// cue 2 replaces sanctuary sound script 7 with script 8. Other cues do nothing.
static void _acropolisSanctuaryRequestCutsceneCue(s32 cue)
{
    _AcropolisSanctuaryCutsceneWork* work = _gAcropolisSanctuaryCutsceneTask->work;

    switch (cue) {
        case ACROPOLIS_SANCTUARY_CUTSCENE_CUE_HOLD:
            work->phase            = ACROPOLIS_SANCTUARY_CUTSCENE_PHASE_HOLD;
            work->placementApplied = ACROPOLIS_SANCTUARY_CUTSCENE_PLACEMENT_PENDING;
            return;
        case ACROPOLIS_SANCTUARY_CUTSCENE_CUE_PLACE_PLAYER:
            work->phase            = ACROPOLIS_SANCTUARY_CUTSCENE_PHASE_PLACE_PLAYER;
            work->placementApplied = ACROPOLIS_SANCTUARY_CUTSCENE_PLACEMENT_PENDING;
            return;
        case ACROPOLIS_SANCTUARY_CUTSCENE_CUE_CHANGE_SOUND:
            sndEvtRequestScriptStop(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SANCTUARY, 7), SOUND_SCRIPT_STOP_NO_FADE);
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SANCTUARY, 8), 0, 0);
            return;
    }
}

/// Restores the movable blocker in the room's live collision grid.
///
/// Copies the template's four normal XYZ triples, eight vertex XYZ triples and
/// four complete faces into the live grid's leading records, retaining vector
/// fourth components. Assigns surface class 1, then offsets the vertices by
/// (200, 0, 380) before clearance or (3000, 0, 0) afterwards, in whole room units.
/// Requires a readable template and writable live room arrays through the call.
static void _acropolisSanctuaryPlaceBlockerCollision(void)
{
    enum {
        ACROPOLIS_SANCTUARY_BLOCKER_SURFACE_CLASS = 1,
    };
    WorldCollisionGrid* roomGrid        = &_gAcropolisSanctuaryCollisionGrid;
    WorldCollisionGrid* blockerTemplate = &_gAcropolisSanctuaryBlockerCollisionGrid;
    SVECTOR             placementOffset;
    s32                 elementIndex;

    // Restore the template without copying the unused fourth vector components.
    for (elementIndex = 0; elementIndex < ARRAY_SIZE(_gAcropolisSanctuaryBlockerCollisionGridNormals); elementIndex++) {
        roomGrid->normals[elementIndex].vx            = blockerTemplate->normals[elementIndex].vx;
        roomGrid->normals[elementIndex].vy            = blockerTemplate->normals[elementIndex].vy;
        roomGrid->normals[elementIndex].vz            = blockerTemplate->normals[elementIndex].vz;
        roomGrid->vertices[elementIndex * 2].vx       = blockerTemplate->vertices[elementIndex * 2].vx;
        roomGrid->vertices[elementIndex * 2].vy       = blockerTemplate->vertices[elementIndex * 2].vy;
        roomGrid->vertices[elementIndex * 2].vz       = blockerTemplate->vertices[elementIndex * 2].vz;
        roomGrid->vertices[(elementIndex * 2) + 1].vx = blockerTemplate->vertices[(elementIndex * 2) + 1].vx;
        roomGrid->vertices[(elementIndex * 2) + 1].vy = blockerTemplate->vertices[(elementIndex * 2) + 1].vy;
        roomGrid->vertices[(elementIndex * 2) + 1].vz = blockerTemplate->vertices[(elementIndex * 2) + 1].vz;
        roomGrid->faces[elementIndex]                 = blockerTemplate->faces[elementIndex];
        roomGrid->faces[elementIndex].surfaceClass    = ACROPOLIS_SANCTUARY_BLOCKER_SURFACE_CLASS;
    }

    if (gameFlagGetNibble(GAME_FLAG_SANCTUARY_BLOCKER_CLEARED) == 0) {
        placementOffset.vx = 200;
        placementOffset.vy = 0;
        placementOffset.vz = 380;
    } else {
        placementOffset.vx = 3000;
        placementOffset.vy = 0;
        placementOffset.vz = 0;
    }

    for (elementIndex = 0; elementIndex < ARRAY_SIZE(_gAcropolisSanctuaryBlockerCollisionGridVerts); elementIndex++) {
        roomGrid->vertices[elementIndex].vx += placementOffset.vx;
        roomGrid->vertices[elementIndex].vy += placementOffset.vy;
        roomGrid->vertices[elementIndex].vz += placementOffset.vz;
    }
}

/// Selects one of the current room view's two alternative cached-sprite batches.
///
/// Only each argument's low byte is used. Zero `useSecondBatch` shows batch 1
/// and hides batch 2; nonzero reverses them. `viewId` is 1-based and must select
/// a live view with both records. The current stage and area must be valid and
/// `spriteVariant` must be 1. Visibility is retained in the room's batch list;
/// neither sprite sources nor cached packets are reallocated.
static void _acropolisSanctuarySelectViewSpriteBatch(s32 useSecondBatch, s32 viewId)
{
    GameSession*     session  = gGameSession;
    GameLocationKey* location = &session->location.loc;
    SpriteBatch*     batches;

    batches = gSpriteAreaTables[location->stage - 1][session->spriteVariant - 1].areaViews[location->area - 1][(viewId & ACROPOLIS_SANCTUARY_SCRIPT_SPRITE_FIELD_MASK) - 1].batches;
    if ((useSecondBatch & ACROPOLIS_SANCTUARY_SCRIPT_SPRITE_FIELD_MASK) == 0) {
        batches[1].hidden = 0;
        batches[2].hidden = 1;
    } else {
        batches[1].hidden = 1;
        batches[2].hidden = 0;
    }
}

void acropolisSanctuaryRoomEffectTask(Task* task)
{
    enum {
        ACROPOLIS_SANCTUARY_VIEW_MOSAIC_LATCH_HOLD = 12,
    };
    enum {
        ACROPOLIS_SANCTUARY_ROOM_EFFECT_INITIAL  = 0,
        ACROPOLIS_SANCTUARY_FLAME_DIM_ARGUMENT   = 2 << 8,
        ACROPOLIS_SANCTUARY_FLAME_SMALL_ARGUMENT = 160 << 16,
        ACROPOLIS_SANCTUARY_FLAME_DIM_COUNT      = 6,
    };
    GfxCoord*        roomCoord;
    GameLocationKey* location;
    s32              placementIndex;

    roomCoord = task->extra.coordBody->coord;
    if (task->state == ACROPOLIS_SANCTUARY_ROOM_EFFECT_INITIAL) {
        for (placementIndex = 0; placementIndex < ACROPOLIS_SANCTUARY_FLAME_DIM_COUNT; placementIndex++) {
            effectSpawn(EFFECT_ACROPOLIS_SANCTUARY_FLAME, roomCoord, placementIndex + ACROPOLIS_SANCTUARY_FLAME_DIM_ARGUMENT, &_gAcropolisSanctuaryFlamePositions[placementIndex]);
        }
        for (placementIndex = ACROPOLIS_SANCTUARY_FLAME_DIM_COUNT; placementIndex < ARRAY_SIZE(_gAcropolisSanctuaryFlamePositions); placementIndex++) {
            effectSpawn(EFFECT_ACROPOLIS_SANCTUARY_FLAME, roomCoord, placementIndex + ACROPOLIS_SANCTUARY_FLAME_SMALL_ARGUMENT, &_gAcropolisSanctuaryFlamePositions[placementIndex]);
        }
        task->msgTable = _gAcropolisSanctuaryRoomEffectMessages;
        gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM_EFFECT);
        _gAcropolisSanctuaryMosaicCleanupLatched = ACROPOLIS_SANCTUARY_MOSAIC_CLEANUP_CLEAR;
        task->state                              = task->state + 1;
    }
    location = &gGameSession->location.loc;
    if (location->view == ACROPOLIS_SANCTUARY_VIEW_MOSAIC_LATCH_SET) {
        _gAcropolisSanctuaryMosaicCleanupLatched = ACROPOLIS_SANCTUARY_MOSAIC_CLEANUP_LATCHED;
    } else if (location->view != ACROPOLIS_SANCTUARY_VIEW_MOSAIC_LATCH_HOLD) {
        _gAcropolisSanctuaryMosaicCleanupLatched = ACROPOLIS_SANCTUARY_MOSAIC_CLEANUP_CLEAR;
    }
}

void acropolisSanctuaryMosaicTask(Task* task)
{
    enum { ACROPOLIS_SANCTUARY_MOSAIC_INITIAL = 0 };
    EffectWork*                    mosaicWork;
    GfxCoord*                      mosaicCoord;
    _AcropolisSanctuaryMosaicTile* tile;
    s32                            sizeClass;
    s32                            spawnIndex;
    s32                            tileIndex;

/// Places and spawns the selected mosaic tile in whole coordinate units.
///
/// Captures `tile`, `sizeClass`, `mosaicWork` and `mosaicCoord` in this function.
/// Writes `sizeClass` and the work's temporary offset; evaluates tileIndex once.
/// The captured pointers and argument must have no side effects. Expands to
/// statements and requires a braced call site; placement is copied at spawn.
#define ACROPOLIS_SANCTUARY_SPAWN_MOSAIC_TILE(tileIndex)                                                           \
    sizeClass           = tile->sizeClass;                                                                         \
    mosaicWork->move.vx = 0;                                                                                       \
    mosaicWork->move.vy = ((tile->originV * 1145) >> 7) - _gAcropolisSanctuaryMosaicTileCorners[sizeClass][0].vy;  \
    mosaicWork->move.vz = -((tile->originU * 2147) >> 8) - _gAcropolisSanctuaryMosaicTileCorners[sizeClass][0].vz; \
    effectSpawn(EFFECT_ACROPOLIS_SANCTUARY_MOSAIC_TILE, mosaicCoord, (tileIndex), &mosaicWork->move)

    mosaicWork  = task->spawnArg2.pointer;
    mosaicCoord = task->extra.coordBody->coord;
    if (task->state != ACROPOLIS_SANCTUARY_MOSAIC_INITIAL) {
        effectKillTask(mosaicWork, task);
        return;
    }
    for (spawnIndex = 0; spawnIndex < ARRAY_SIZE(_gAcropolisSanctuaryMosaicTiles); spawnIndex++) {
        tile = &_gAcropolisSanctuaryMosaicTiles[spawnIndex];
        ACROPOLIS_SANCTUARY_SPAWN_MOSAIC_TILE(spawnIndex);
    }
    for (spawnIndex = 0; spawnIndex < ARRAY_SIZE(_gAcropolisSanctuaryMosaicRepeatedTileIndices); spawnIndex++) {
        tileIndex = _gAcropolisSanctuaryMosaicRepeatedTileIndices[spawnIndex];
        tile      = &_gAcropolisSanctuaryMosaicTiles[tileIndex];
        ACROPOLIS_SANCTUARY_SPAWN_MOSAIC_TILE(tileIndex);
    }
    task->state = task->state + 1;

#undef ACROPOLIS_SANCTUARY_SPAWN_MOSAIC_TILE
}

void acropolisSanctuaryMosaicTileTask(Task* task)
{
    enum {
        ACROPOLIS_SANCTUARY_MOSAIC_TILE_SPLIT_AGE = 100,
    };
    EffectWork*                           tileWork;
    GfxCoord*                             tileCoord;
    _AcropolisSanctuaryMosaicTileScratch* tileScratch;
    POLY_FT4*                             tileQuad;
    SVECTOR*                              scratchCorner;
    s32                                   sizeClass;
    s32                                   cornerIndex;
    s32                                   shardCount;

    tileWork  = task->spawnArg2.pointer;
    sizeClass = _gAcropolisSanctuaryMosaicTiles[task->spawnArg1.value].sizeClass;
    tileCoord = task->extra.coordBody->coord;
    actorRenderComposeCoord(tileCoord);
    tileScratch = SCRATCH_STACK_RESERVE_BLOCK(_AcropolisSanctuaryMosaicTileScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    // Transform the size-class corners into narrowed world positions.
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(tileScratch->corners); cornerIndex++) {
        tileScratch->corners[cornerIndex].vx = _gAcropolisSanctuaryMosaicTileCorners[sizeClass][cornerIndex].vx;
        // Byte addressing preserves separate store and GTE operand registers.
        scratchCorner     = (SVECTOR*)((u8*)tileScratch + cornerIndex * sizeof(SVECTOR) + OFFSET_OF(_AcropolisSanctuaryMosaicTileScratch, corners));
        scratchCorner->vy = _gAcropolisSanctuaryMosaicTileCorners[sizeClass][cornerIndex].vy;
        scratchCorner->vz = _gAcropolisSanctuaryMosaicTileCorners[sizeClass][cornerIndex].vz;
        gte_SetRotMatrix(&tileCoord->workm);
        gte_ldv0(&tileScratch->corners[cornerIndex]);
        gte_rtv0();
        gte_stsv(&tileScratch->corners[cornerIndex]);
        tileScratch->corners[cornerIndex].vx += tileCoord->workm.t[0];
        scratchCorner->vy                    += tileCoord->workm.t[1];
        scratchCorner->vz                    += tileCoord->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&tileScratch->corners[0]);
    gte_rtps();
    tileQuad       = gGpuPrimCursor;
    gGpuPrimCursor = tileQuad + 1;
    setPolyFT4(tileQuad);
    gte_stsxy(&tileQuad->x0);
    gte_ldv3(&tileScratch->corners[1], &tileScratch->corners[2], &tileScratch->corners[3]);
    gte_rtpt();
    gte_stsxy3(&tileQuad->x1, &tileQuad->x2, &tileQuad->x3);
    gte_stszotz(&tileScratch->otz);
    if (tileScratch->otz >= ACROPOLIS_SANCTUARY_MOSAIC_MIN_DEPTH) {
        // Seed motion only on an accepted projection at age zero.
        if (tileWork->age == 0) {
            tileWork->scale = _gAcropolisSanctuaryMosaicTiles[task->spawnArg1.value].thrown;
            if (tileWork->scale != ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE) {
                gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                tileWork->move.vx = -((gRandomLcgState >> 16) & 0xFF);
                gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                tileWork->move.vy = 0x40 - ((gRandomLcgState >> 16) & 0x7F);
                gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                tileWork->move.vz = 0x40 - ((gRandomLcgState >> 16) & 0x7F);
                gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                tileWork->pos.vx  = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                tileWork->pos.vy  = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                tileWork->pos.vz  = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                tileWork->angle   = _gAcropolisSanctuaryMosaicTiles[task->spawnArg1.value].holdFrames;
            } else {
                gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                tileWork->move.vx = -((gRandomLcgState >> 16) & 0x1F);
                gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                tileWork->move.vy = (gRandomLcgState >> 16) & 7;
                gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                tileWork->move.vz = 4 - ((gRandomLcgState >> 16) & 7);
                gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                tileWork->pos.vx  = 0x20 - ((gRandomLcgState >> 16) & 0x3F);
                gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                tileWork->pos.vy  = 0x20 - ((gRandomLcgState >> 16) & 0x3F);
                gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                tileWork->pos.vz  = 0x20 - ((gRandomLcgState >> 16) & 0x3F);
                gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                tileWork->angle   = _gAcropolisSanctuaryMosaicTiles[task->spawnArg1.value].holdFrames +
                                  ((gRandomLcgState >> 16) & 7);
            }
        }
        tileQuad->tpage = ACROPOLIS_SANCTUARY_MOSAIC_TEXTURE_PAGE;
        tileQuad->clut  = ACROPOLIS_SANCTUARY_MOSAIC_PALETTE;
        setShadeTex(tileQuad, 1);
        setSemiTrans(tileQuad, 1);
        tileQuad->u0 = _gAcropolisSanctuaryMosaicTiles[task->spawnArg1.value].originU;
        tileQuad->v0 = _gAcropolisSanctuaryMosaicTiles[task->spawnArg1.value].originV;
        tileQuad->u1 = _gAcropolisSanctuaryMosaicTiles[task->spawnArg1.value].originU +
                       _gAcropolisSanctuaryMosaicTiles[task->spawnArg1.value].extentU;
        tileQuad->v1 = _gAcropolisSanctuaryMosaicTiles[task->spawnArg1.value].originV;
        tileQuad->u2 = _gAcropolisSanctuaryMosaicTiles[task->spawnArg1.value].originU;
        tileQuad->v2 = _gAcropolisSanctuaryMosaicTiles[task->spawnArg1.value].originV +
                       _gAcropolisSanctuaryMosaicTiles[task->spawnArg1.value].extentV;
        tileQuad->u3 = _gAcropolisSanctuaryMosaicTiles[task->spawnArg1.value].originU +
                       _gAcropolisSanctuaryMosaicTiles[task->spawnArg1.value].extentU;
        tileQuad->v3 = _gAcropolisSanctuaryMosaicTiles[task->spawnArg1.value].originV +
                       _gAcropolisSanctuaryMosaicTiles[task->spawnArg1.value].extentV;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)tileScratch->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                tileQuad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_AcropolisSanctuaryMosaicTileScratch);
    if (tileWork->angle + ACROPOLIS_SANCTUARY_MOSAIC_DRIFT_FRAMES < tileWork->age) {
        effectKillTask(tileWork, task);
        return;
    }
    // Release held tiles into drift; splitting advances age past their lifetime.
    if (tileWork->angle < tileWork->age) {
        tileCoord->coord.t[0] += tileWork->move.vx;
        tileCoord->coord.t[1] += tileWork->move.vy;
        tileCoord->coord.t[2] += tileWork->move.vz;
        gfxRotMatrixYXZ(&tileCoord->coord, &tileWork->pos, GRAPHICS_ROTATION_COMPOSE);
        tileCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        tileWork->move.vy       = tileWork->move.vy + ACROPOLIS_SANCTUARY_MOSAIC_GRAVITY;
        if (tileWork->scale == ACROPOLIS_SANCTUARY_MOSAIC_TILE_CRUMBLE) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((u16)((gRandomLcgState >> 16) % (u32)ACROPOLIS_SANCTUARY_MOSAIC_SPLIT_PERIOD) == 0 || tileCoord->coord.t[1] >= ACROPOLIS_SANCTUARY_MOSAIC_SPLIT_HEIGHT) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                shardCount      = ((gRandomLcgState >> 16) & 3) + 1;
                for (cornerIndex = 0; cornerIndex < shardCount; cornerIndex++) {
                    effectSpawn(EFFECT_ACROPOLIS_SANCTUARY_MOSAIC_SHARD, tileCoord, task->spawnArg1.value | ACROPOLIS_SANCTUARY_MOSAIC_SHARD_AIRBORNE, NULL);
                }
                tileWork->age = tileWork->age + ACROPOLIS_SANCTUARY_MOSAIC_TILE_SPLIT_AGE;
            }
        }
    }
    if (tileCoord->coord.t[0] < ACROPOLIS_SANCTUARY_MOSAIC_BOUNCE_X && tileCoord->coord.t[1] >= ACROPOLIS_SANCTUARY_MOSAIC_BOUNCE_Y) {
        if (sizeClass != ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            // Retain this temporary for the later shard count to preserve allocation.
            sizeClass = (gRandomLcgState >> 16) & 3;
            if (sizeClass != 0) {
                sizeClass = sizeClass + 1;
                for (cornerIndex = 0; cornerIndex < sizeClass; cornerIndex++) {
                    effectSpawn(EFFECT_ACROPOLIS_SANCTUARY_MOSAIC_SHARD, tileCoord, task->spawnArg1.value, NULL);
                }
                tileWork->age = tileWork->age + ACROPOLIS_SANCTUARY_MOSAIC_TILE_SPLIT_AGE;
            } else {
                tileCoord->coord.t[1] -= tileWork->move.vy * 2;
                tileWork->move.vy      = -(tileWork->move.vy >> 1);
            }
        } else {
            tileCoord->coord.t[1] -= tileWork->move.vy * 2;
            tileWork->move.vy      = -(tileWork->move.vy >> 1);
        }
    }
    if (gGameSession->location.loc.view != ACROPOLIS_SANCTUARY_VIEW_MOSAIC_LATCH_SET && _gAcropolisSanctuaryMosaicCleanupLatched != ACROPOLIS_SANCTUARY_MOSAIC_CLEANUP_CLEAR &&
        (tileCoord->coord.t[0] < ACROPOLIS_SANCTUARY_MOSAIC_CLEAR_X ||
         (tileCoord->coord.t[0] < ACROPOLIS_SANCTUARY_MOSAIC_BOUNCE_X && tileCoord->coord.t[1] >= ACROPOLIS_SANCTUARY_MOSAIC_BOUNCE_Y))) {
        tileWork->age = tileWork->age + ACROPOLIS_SANCTUARY_MOSAIC_DRIFT_FRAMES;
    }
    tileWork->age = tileWork->age + 1;
}

void acropolisSanctuaryMosaicShardTask(Task* task)
{
    enum {
        ACROPOLIS_SANCTUARY_MOSAIC_TILE_INDEX_MASK          = 0xFFF,
        ACROPOLIS_SANCTUARY_MOSAIC_SHARD_DRIFT_MASK         = 0xF,
        ACROPOLIS_SANCTUARY_MOSAIC_SHARD_SIZE_SHIFT         = 16,
        ACROPOLIS_SANCTUARY_MOSAIC_SHARD_SIZE_ONE           = 1 << 12,
        ACROPOLIS_SANCTUARY_MOSAIC_SHARD_SIZE_FRACTION_BITS = 12,
        ACROPOLIS_SANCTUARY_MOSAIC_SHARD_MIN_SPLIT_SIZE     = (ACROPOLIS_SANCTUARY_MOSAIC_SHARD_SIZE_ONE / 4) + 1,
        ACROPOLIS_SANCTUARY_MOSAIC_SHARD_CHILD_SIZE_SHIFT   = ACROPOLIS_SANCTUARY_MOSAIC_SHARD_SIZE_SHIFT - 1,
        ACROPOLIS_SANCTUARY_MOSAIC_SHARD_MAX_BOUNCES        = 2,
    };
    EffectWork*                            shardWork;
    GfxCoord*                              shardCoord;
    _AcropolisSanctuaryMosaicShardScratch* shardScratch;
    POLY_FT3*                              shardTriangle;
    SVECTOR*                               baseCorners;
    s32                                    sizeFactor;
    s32                                    packedSize;
    s32                                    cornerIndex;
    s32                                    shardCount;
    s32                                    childParams;
    SVECTOR*                               scratchCorner;

    shardWork  = task->spawnArg2.pointer;
    shardCoord = task->extra.coordBody->coord;
    if (shardWork->age >= ACROPOLIS_SANCTUARY_MOSAIC_DRIFT_FRAMES + 1 || shardWork->index >= ACROPOLIS_SANCTUARY_MOSAIC_SHARD_MAX_BOUNCES) {
        effectKillTask(shardWork, task);
        return;
    }
    actorRenderComposeCoord(shardCoord);
    shardScratch = SCRATCH_STACK_RESERVE_BLOCK(_AcropolisSanctuaryMosaicShardScratch);
    // Unpack once, preserving signed size and the unsigned zero test.
    if (shardWork->age == 0) {
        shardWork->scale = (task->spawnArg1.value >> ACROPOLIS_SANCTUARY_MOSAIC_SHARD_DRIFT_SHIFT) & ACROPOLIS_SANCTUARY_MOSAIC_SHARD_DRIFT_MASK;
        packedSize       = (s16)(task->spawnArg1.value >> ACROPOLIS_SANCTUARY_MOSAIC_SHARD_SIZE_SHIFT);
        sizeFactor       = ACROPOLIS_SANCTUARY_MOSAIC_SHARD_SIZE_ONE;
        if ((u16)packedSize != 0) {
            sizeFactor = packedSize;
        }
        shardWork->angle      = sizeFactor;
        task->spawnArg1.value = task->spawnArg1.value & ACROPOLIS_SANCTUARY_MOSAIC_TILE_INDEX_MASK;
        if (shardWork->scale != 0) {
            gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            shardWork->move.vx = 8 - ((gRandomLcgState >> 16) & 0xF);
            gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            shardWork->move.vy = (gRandomLcgState >> 16) & 0xF;
            gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            shardWork->move.vz = 8 - ((gRandomLcgState >> 16) & 0xF);
        } else {
            gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            shardWork->move.vx = 8 - ((gRandomLcgState >> 16) & 0xF);
            gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            shardWork->move.vy = -((gRandomLcgState >> 16) & 0x1F);
            gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            shardWork->move.vz = 8 - ((gRandomLcgState >> 16) & 0xF);
        }
        gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        shardWork->pos.vx = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
        gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        shardWork->pos.vy = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
        gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        shardWork->pos.vz = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
    }
    gte_SetTransMatrix(&GsWSMATRIX);
    baseCorners = _gAcropolisSanctuaryMosaicTileCorners[ACROPOLIS_SANCTUARY_MOSAIC_TILE_SMALL];
    // Scale and transform the three shard corners before projecting them together.
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(shardScratch->corners); cornerIndex++) {
        shardScratch->corners[cornerIndex].vx = baseCorners[cornerIndex].vx;
        // Byte addressing preserves separate store and GTE operand registers.
        scratchCorner     = (SVECTOR*)((u8*)shardScratch + cornerIndex * sizeof(SVECTOR) + OFFSET_OF(_AcropolisSanctuaryMosaicShardScratch, corners));
        scratchCorner->vy = baseCorners[cornerIndex].vy;
        scratchCorner->vz = baseCorners[cornerIndex].vz;
        gte_lddp(shardWork->angle);
        gte_ldsv(&shardScratch->corners[cornerIndex]);
        gte_gpf12();
        gte_stsv(&shardScratch->corners[cornerIndex]);
        gte_SetRotMatrix(&shardCoord->workm);
        gte_ldv0(&shardScratch->corners[cornerIndex]);
        gte_rtv0();
        gte_stsv(&shardScratch->corners[cornerIndex]);
        shardScratch->corners[cornerIndex].vx += shardCoord->workm.t[0];
        scratchCorner->vy                     += shardCoord->workm.t[1];
        scratchCorner->vz                     += shardCoord->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv3(&shardScratch->corners[0], &shardScratch->corners[1], &shardScratch->corners[2]);
    gte_rtpt();
    shardTriangle  = gGpuPrimCursor;
    gGpuPrimCursor = shardTriangle + 1;
    setPolyFT3(shardTriangle);
    gte_stsxy3(&shardTriangle->x0, &shardTriangle->x1, &shardTriangle->x2);
    gte_stszotz(&shardScratch->otz);
    if (shardScratch->otz >= ACROPOLIS_SANCTUARY_MOSAIC_MIN_DEPTH) {
        shardTriangle->tpage = ACROPOLIS_SANCTUARY_MOSAIC_TEXTURE_PAGE;
        shardTriangle->clut  = ACROPOLIS_SANCTUARY_MOSAIC_PALETTE;
        setShadeTex(shardTriangle, 1);
        setSemiTrans(shardTriangle, 1);
        shardTriangle->u0 = _gAcropolisSanctuaryMosaicTiles[task->spawnArg1.value].originU;
        shardTriangle->v0 = _gAcropolisSanctuaryMosaicTiles[task->spawnArg1.value].originV;
        shardTriangle->u1 = _gAcropolisSanctuaryMosaicTiles[task->spawnArg1.value].originU +
                            ((_gAcropolisSanctuaryMosaicTiles[task->spawnArg1.value].extentU * shardWork->angle) >> ACROPOLIS_SANCTUARY_MOSAIC_SHARD_SIZE_FRACTION_BITS);
        shardTriangle->v1 = _gAcropolisSanctuaryMosaicTiles[task->spawnArg1.value].originV;
        shardTriangle->u2 = _gAcropolisSanctuaryMosaicTiles[task->spawnArg1.value].originU;
        shardTriangle->v2 = _gAcropolisSanctuaryMosaicTiles[task->spawnArg1.value].originV +
                            ((_gAcropolisSanctuaryMosaicTiles[task->spawnArg1.value].extentV * shardWork->angle) >> ACROPOLIS_SANCTUARY_MOSAIC_SHARD_SIZE_FRACTION_BITS);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)shardScratch->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                shardTriangle);
    }
    shardCoord->coord.t[0] += shardWork->move.vx;
    shardCoord->coord.t[1] += shardWork->move.vy;
    SCRATCH_STACK_RELEASE_BLOCK(_AcropolisSanctuaryMosaicShardScratch);
    shardCoord->coord.t[2] += shardWork->move.vz;
    gfxRotMatrixYXZ(&shardCoord->coord, &shardWork->pos, GRAPHICS_ROTATION_COMPOSE);
    shardCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    shardWork->move.vy       = shardWork->move.vy + ACROPOLIS_SANCTUARY_MOSAIC_GRAVITY;
    // Bounce at the floor region or split into children with half the encoded size.
    if (shardCoord->coord.t[0] < ACROPOLIS_SANCTUARY_MOSAIC_BOUNCE_X && shardCoord->coord.t[1] >= ACROPOLIS_SANCTUARY_MOSAIC_BOUNCE_Y) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        shardCount      = (gRandomLcgState >> 16) & 1;
        if (shardWork->angle >= ACROPOLIS_SANCTUARY_MOSAIC_SHARD_MIN_SPLIT_SIZE && shardCount != 0) {
            shardCount = shardCount + 1;
            for (cornerIndex = 0; cornerIndex < shardCount; cornerIndex++) {
                effectSpawn(EFFECT_ACROPOLIS_SANCTUARY_MOSAIC_SHARD, shardCoord, task->spawnArg1.value | (shardWork->angle << ACROPOLIS_SANCTUARY_MOSAIC_SHARD_CHILD_SIZE_SHIFT), NULL);
            }
            shardWork->age = shardWork->age + ACROPOLIS_SANCTUARY_MOSAIC_DRIFT_FRAMES;
        } else {
            shardCoord->coord.t[1] -= shardWork->move.vy * 2;
            shardWork->move.vy      = -(shardWork->move.vy >> 1);
            shardWork->index        = shardWork->index + 1;
        }
    } else if (shardWork->angle >= ACROPOLIS_SANCTUARY_MOSAIC_SHARD_MIN_SPLIT_SIZE) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((u16)((gRandomLcgState >> 16) % (u32)ACROPOLIS_SANCTUARY_MOSAIC_SPLIT_PERIOD) == 0 || shardCoord->coord.t[1] >= ACROPOLIS_SANCTUARY_MOSAIC_SPLIT_HEIGHT) {
            for (cornerIndex = 0; cornerIndex < 2; cornerIndex++) {
                childParams = (shardWork->angle << ACROPOLIS_SANCTUARY_MOSAIC_SHARD_CHILD_SIZE_SHIFT) | ACROPOLIS_SANCTUARY_MOSAIC_SHARD_AIRBORNE;
                effectSpawn(EFFECT_ACROPOLIS_SANCTUARY_MOSAIC_SHARD, shardCoord, task->spawnArg1.value | childParams, NULL);
            }
            shardWork->age = shardWork->age + ACROPOLIS_SANCTUARY_MOSAIC_DRIFT_FRAMES;
        }
    }
    if (gGameSession->location.loc.view != ACROPOLIS_SANCTUARY_VIEW_MOSAIC_LATCH_SET && _gAcropolisSanctuaryMosaicCleanupLatched != ACROPOLIS_SANCTUARY_MOSAIC_CLEANUP_CLEAR &&
        (shardCoord->coord.t[0] < ACROPOLIS_SANCTUARY_MOSAIC_CLEAR_X ||
         (shardCoord->coord.t[0] < ACROPOLIS_SANCTUARY_MOSAIC_BOUNCE_X && shardCoord->coord.t[1] >= ACROPOLIS_SANCTUARY_MOSAIC_BOUNCE_Y))) {
        shardWork->age = shardWork->age + ACROPOLIS_SANCTUARY_MOSAIC_DRIFT_FRAMES;
    }
    shardWork->age = shardWork->age + 1;
}

void acropolisSanctuaryFlameTask(Task* task)
{
    enum {
        ACROPOLIS_SANCTUARY_FLAME_STATE_INITIAL    = 0,
        ACROPOLIS_SANCTUARY_FLAME_PLACEMENT_MASK   = 0xF,
        ACROPOLIS_SANCTUARY_FLAME_VARIANT_SHIFT    = 8,
        ACROPOLIS_SANCTUARY_FLAME_VARIANT_MASK     = 3,
        ACROPOLIS_SANCTUARY_FLAME_SIZE_SHIFT       = 16,
        ACROPOLIS_SANCTUARY_FLAME_SIZE_MASK        = 0xFFF,
        ACROPOLIS_SANCTUARY_FLAME_SIZE_BITS        = ACROPOLIS_SANCTUARY_FLAME_SIZE_MASK << ACROPOLIS_SANCTUARY_FLAME_SIZE_SHIFT,
        ACROPOLIS_SANCTUARY_FLAME_DEFAULT_SIZE     = 640,
        ACROPOLIS_SANCTUARY_FLAME_MIN_DEPTH        = 17,
        ACROPOLIS_SANCTUARY_FLAME_TEXTURE_PAGE     = getTPage(0, GPU_BLEND_ADD, 704, 0),
        ACROPOLIS_SANCTUARY_FLAME_PALETTE_WORDS    = 16,
        ACROPOLIS_SANCTUARY_FLAME_PALETTE_Y        = 270,
        ACROPOLIS_SANCTUARY_FLAME_TEXTURE_STRIDE_U = 40,
        ACROPOLIS_SANCTUARY_FLAME_TEXTURE_EXTENT   = 39,
    };
    EffectWork*                  flameWork;
    GfxCoord*                    flameCoord;
    RoomGlowSpriteScratch*       flameScratch;
    POLY_FT4*                    flameQuad;
    _AcropolisSanctuaryFlameGrey baseGrey;
    _AcropolisSanctuaryFlameGrey flickerGrey;
    s32                          spawnParams;
    s32                          greyLevel;

    flameWork  = task->spawnArg2.pointer;
    flameCoord = task->extra.coordBody->coord;
    if ((_gAcropolisSanctuaryFlameViewMasks[task->spawnArg1.value & ACROPOLIS_SANCTUARY_FLAME_PLACEMENT_MASK] >> (gGameSession->location.loc.view - 1)) & 1) {
        actorRenderComposeCoord(flameCoord);
        flameScratch = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);
        if (task->state == ACROPOLIS_SANCTUARY_FLAME_STATE_INITIAL) {
            // Initialize on the first visible update, then retain only the placement index.
            baseGrey              = _gAcropolisSanctuaryFlameBaseGrey;
            flickerGrey           = _gAcropolisSanctuaryFlameFlickerGrey;
            spawnParams           = task->spawnArg1.value;
            flameWork->scale      = (spawnParams & ACROPOLIS_SANCTUARY_FLAME_SIZE_BITS)
                                        ? ((spawnParams >> ACROPOLIS_SANCTUARY_FLAME_SIZE_SHIFT) & ACROPOLIS_SANCTUARY_FLAME_SIZE_MASK)
                                        : ACROPOLIS_SANCTUARY_FLAME_DEFAULT_SIZE;
            flameWork->angle      = (task->spawnArg1.value >> ACROPOLIS_SANCTUARY_FLAME_VARIANT_SHIFT) & ACROPOLIS_SANCTUARY_FLAME_VARIANT_MASK;
            task->spawnArg1.value = task->spawnArg1.value & ACROPOLIS_SANCTUARY_FLAME_PLACEMENT_MASK;
            flameWork->period     = baseGrey.grey[flameWork->angle];
            flameWork->step       = flickerGrey.grey[flameWork->angle];
            task->state++;
        }
        // Project the composed centre after narrowing it to signed 16-bit coordinates.
        flameScratch->worldPos.vx = flameCoord->workm.t[0];
        flameScratch->worldPos.vy = flameCoord->workm.t[1];
        flameScratch->worldPos.vz = flameCoord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&flameScratch->worldPos);
        gte_rtps();
        // Visible placements consume a packet even when its projected depth rejects it.
        flameQuad      = gGpuPrimCursor;
        gGpuPrimCursor = flameQuad + 1;
        setPolyFT4(flameQuad);
        gte_stsxy(&flameScratch->screenPos);
        gte_stszotz(&flameScratch->otz);
        if (flameScratch->otz >= ACROPOLIS_SANCTUARY_FLAME_MIN_DEPTH) {
            greyLevel        = (u8)flameWork->period + (gDisplayState.animFrame & 1) * flameWork->step;
            flameQuad->tpage = ACROPOLIS_SANCTUARY_FLAME_TEXTURE_PAGE;
            setSemiTrans(flameQuad, 1);
            setRGB0(flameQuad, greyLevel, greyLevel, greyLevel);
            flameQuad->clut          = getClut(flameWork->angle * ACROPOLIS_SANCTUARY_FLAME_PALETTE_WORDS, ACROPOLIS_SANCTUARY_FLAME_PALETTE_Y);
            flameQuad->u0            = flameWork->angle * ACROPOLIS_SANCTUARY_FLAME_TEXTURE_STRIDE_U;
            flameQuad->v0            = 0;
            flameQuad->u1            = flameWork->angle * ACROPOLIS_SANCTUARY_FLAME_TEXTURE_STRIDE_U + ACROPOLIS_SANCTUARY_FLAME_TEXTURE_EXTENT;
            flameQuad->v1            = 0;
            flameQuad->u2            = flameWork->angle * ACROPOLIS_SANCTUARY_FLAME_TEXTURE_STRIDE_U;
            flameQuad->v2            = ACROPOLIS_SANCTUARY_FLAME_TEXTURE_EXTENT;
            flameQuad->u3            = flameWork->angle * ACROPOLIS_SANCTUARY_FLAME_TEXTURE_STRIDE_U + ACROPOLIS_SANCTUARY_FLAME_TEXTURE_EXTENT;
            flameQuad->v3            = ACROPOLIS_SANCTUARY_FLAME_TEXTURE_EXTENT;
            flameScratch->halfExtent = (flameWork->scale * ACROPOLIS_SANCTUARY_FLAME_TEXTURE_EXTENT) / flameScratch->otz;
            _acropolisSanctuarySetFlameQuadBounds(flameQuad, flameScratch);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)flameScratch->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    flameQuad);
        }
        SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
    }
}

/// Starts the mosaic breakup from the room-effect task's placement coordinate.
///
/// Receives `ACROPOLIS_SANCTUARY_MOSAIC_MESSAGE_START`; both payload words are
/// unused. Requires a live coordinate body and the loaded sanctuary overlay.
/// The spawner snapshots the local offset during dispatch; the spawned task does
/// not follow its retained offset pointer. Always returns 0.
static s32 _acropolisSanctuaryStartMosaic(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    GfxCoord* mosaicCoord  = task->extra.coordBody->coord;
    SVECTOR   mosaicOffset = _gAcropolisSanctuaryMosaicOffset;

    effectSpawn(EFFECT_ACROPOLIS_SANCTUARY_MOSAIC, mosaicCoord, 0, &mosaicOffset);
    return 0;
}

#include "../../shared/actor_contacts_push_contact.inc.c"

#include "../../shared/actor_contacts_push.inc.c"

void acropolisSanctuaryItemVisibilityTask(Task* task)
{
    enum {
        ACROPOLIS_SANCTUARY_ITEM_HIDDEN_VIEW_FIRST  = 11,
        ACROPOLIS_SANCTUARY_ITEM_HIDDEN_VIEW_SECOND = 13,
        ACROPOLIS_SANCTUARY_ITEM_STATE_HIDDEN       = 2,
    };
    Enemy*     placedItem = task->spawnArg2.pointer;
    TmdObject* itemModel  = task->extra.tmd;
    s32        objectState;
    s32        viewIndex;

    objectState = areaGetCurrentObjectState((u8)placedItem->placeKey);
    viewIndex   = viewGetMappedIndex();
    if (viewIndex == ACROPOLIS_SANCTUARY_ITEM_HIDDEN_VIEW_FIRST || viewIndex == ACROPOLIS_SANCTUARY_ITEM_HIDDEN_VIEW_SECOND || objectState == ACROPOLIS_SANCTUARY_ITEM_STATE_HIDDEN) {
        itemModel->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        itemModel->flags    = TMD_OBJECT_FLAGGED_PASS;
        itemModel->otOffset = 0;
    }
}

/// Retains an unreferenced placed-object visibility hook in the sanctuary image.
///
/// Hides the TMD model at object state 2; otherwise enables the flagged draw
/// pass with zero ordering bias. The mapped-view query has no effect. No
/// spawner or table binds this hook, so the presumed `Enemy` spawn payload
/// and its placed-item identity are unconfirmed.
static void _acropolisSanctuaryUpdateUnusedPlacedObjectVisibility(Task* task)
{
    enum {
        ACROPOLIS_SANCTUARY_UNUSED_OBJECT_HIDDEN = 2,
    };
    Enemy*     placedObject;
    TmdObject* objectModel;
    s32        objectState;

    placedObject = task->spawnArg2.pointer;
    objectModel  = task->extra.tmd;
    objectState  = areaGetCurrentObjectState((u8)placedObject->placeKey);
    viewGetMappedIndex();
    if (objectState == ACROPOLIS_SANCTUARY_UNUSED_OBJECT_HIDDEN) {
        objectModel->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        objectModel->flags    = TMD_OBJECT_FLAGGED_PASS;
        objectModel->otOffset = 0;
    }
}
