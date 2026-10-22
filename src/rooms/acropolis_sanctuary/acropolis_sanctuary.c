#include "rooms/acropolis_sanctuary.h"
#include "../../stages/akropolis/event_flags.h"

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
#include "../../shared/actor/actor_contacts.h"

#include "assets/rooms/akropolis/sanctuary/declarations.inc"

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

#define ROOM_PART _gAcropolisSanctuaryMosaicOffset
#include "assets/rooms/akropolis/sanctuary/markers.inc"
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

static void _acropolisSanctuaryApplyScriptSpriteSelection(u32 packedSelection);
static void _acropolisSanctuaryRestorePlayerWeaponAnimation(void);

static s32 _acropolisSanctuaryResolvePromenadeExit(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _acropolisSanctuaryRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unused);
static s32 _acropolisSanctuaryHandleRoomCommand(Task* task, s32 messageId, s32 command, s32 unused);
static s32 _acropolisSanctuaryHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

#include "assets/rooms/akropolis/sanctuary/clips_1.inc"

/// Message routes borrowed by the sanctuary room task while it is live.
///
/// Resolve takes request/reply room events; command and key-item use take
/// integer selectors; room action borrows a direction-action request. The
/// terminal record makes unsupported IDs return zero.
static TaskMessageEntry _gAcropolisSanctuaryRoomMessages[] = {
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
    &gAkropolisSanctuaryPlayerSanctuaryArrival7Clip,
    &gAkropolisSanctuaryPlayerSanctuaryArrival11Clip,
    &gAkropolisSanctuaryPlayerSanctuaryTalkClip,
    &gAkropolisSanctuaryPlayerSanctuaryTalkEndClip,
    &gAkropolisSanctuaryPlayerSanctuaryReplyClip,
    &gAkropolisSanctuaryPlayerSanctuaryListenClip,
    &gAkropolisSanctuaryPlayerSanctuaryArrival26Clip,
    &gAkropolisSanctuaryPlayerSanctuaryArrival27Clip,
    &_gAkropolisCafeteriaRupertArriveClip,
};

/// Installs the nine sanctuary scene sets in the player's equipped-weapon bank.
///
/// Copies nine pointer words to slots 47..55 without starting playback. The
/// request is borrowed synchronously; the room and Rupert animation resources
/// must remain loaded while those copied pointers are used.
static AnimationBankCopyRequest _gAcropolisSanctuaryPlayerSceneBankExtensionRequest = {
    .sets  = _gAcropolisSanctuaryPlayerSceneAnimationSets,
    .count = ARRAY_SIZE(_gAcropolisSanctuaryPlayerSceneAnimationSets),
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
static AnimationPlayRequest _gAcropolisSanctuaryNo9Clip4Request = {
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
static AnimationPlayRequest _gAcropolisSanctuaryRupertClip1Request = {
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
static AnimationPlayRequest _gAcropolisSanctuaryRupertClip2Request = {
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
static AnimationPlayRequest _gAcropolisSanctuaryRupertClip4Request = {
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
static EvsSceneKey _gAcropolisSanctuaryArrivalSceneKey = {
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

#include "assets/rooms/akropolis/sanctuary/clips_2.inc"

/// Single-set table installed for the player's sanctuary departure cutscene.
///
/// Install-and-play borrows this table and starts clip 0. Keep the table and
/// its room animation set loaded through playback and the roof-garden handoff.
#include "assets/rooms/akropolis/sanctuary/acropolis_sanctuary_cutscene_player_bank.inc"

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

#define ROOM_PART _gAcropolisSanctuaryBlockerCollisionGridNormals
#include "assets/rooms/akropolis/sanctuary/geometry.inc"
/// Mosaic-start route borrowed by the sanctuary room-effect task.
///
/// The start message ignores both payload words. The terminal record makes
/// unsupported IDs return zero; this table and callback outlive the receiver.
static TaskMessageEntry _gAcropolisSanctuaryRoomEffectMessages[] = {
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

/// States stored in the mosaic debris cleanup latch.
enum {
    ACROPOLIS_SANCTUARY_MOSAIC_CLEANUP_CLEAR   = 0,
    ACROPOLIS_SANCTUARY_MOSAIC_CLEANUP_LATCHED = 1,
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
static s16 _gAcropolisSanctuaryMosaicRepeatedTileIndices[] = {
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
/// View-driven latch accelerating cleanup of mosaic debris outside view 16.
///
/// Room-effect initialization clears it; view 16 sets it, view 12 retains it,
/// and other views clear it. Tile and shard tasks use it to expire debris past
/// the room's cleanup boundaries. Stored as a word (0 clear, 1 latched).
static s32 _gAcropolisSanctuaryMosaicCleanupLatched = ACROPOLIS_SANCTUARY_MOSAIC_CLEANUP_CLEAR;

/// Map-marker nibbles updated when arriving through the sanctuary's doorways.
enum {
    ACROPOLIS_SANCTUARY_ROOF_GARDEN_MAP_FLAG = 492,
    ACROPOLIS_SANCTUARY_PROMENADE_MAP_FLAG   = 493,
};

/// Resource keys shared with the map overlay's sanctuary placement records.
enum {
    ACROPOLIS_SANCTUARY_RESOURCE_NO9    = 27,
    ACROPOLIS_SANCTUARY_RESOURCE_RUPERT = 102,
};

/// Saved layout selectors; all other slots in the published table are absent.
enum {
    ACROPOLIS_SANCTUARY_AREA_VARIANT_NO9_AND_RUPERT = 1,
    ACROPOLIS_SANCTUARY_AREA_VARIANT_RUPERT         = 2,
};

/// GPU encodings and neutral RGB modulation for the room's authored view sprites.
enum {
    ACROPOLIS_SANCTUARY_SPRITE_PAGE_896      = getTPage(1, GPU_BLEND_AVERAGE, 896, 0),
    ACROPOLIS_SANCTUARY_SPRITE_PAGE_960      = getTPage(1, GPU_BLEND_AVERAGE, 960, 0),
    ACROPOLIS_SANCTUARY_SPRITE_CLUT_255      = getClut(0, 255),
    ACROPOLIS_SANCTUARY_SPRITE_CLUT_256      = getClut(0, 256),
    ACROPOLIS_SANCTUARY_SPRITE_COLOR_NEUTRAL = 128,
};

/// Cue-2 script bases in the loaded type-1 bank; cue 1 uses the next entry.
enum {
    ACROPOLIS_SANCTUARY_FOOTSTEP_CLASS1_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x2D,
    ACROPOLIS_SANCTUARY_FOOTSTEP_CLASS1_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x2F,
    ACROPOLIS_SANCTUARY_FOOTSTEP_CLASS2_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x11,
    ACROPOLIS_SANCTUARY_FOOTSTEP_CLASS2_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x13,
};

enum { ACROPOLIS_SANCTUARY_AREA_UPDATE_SET_MAP_MARK = 1 };

#define ROOM_PART _gAcropolisSanctuaryFlamePositions
#include "assets/rooms/akropolis/sanctuary/markers.inc"
#include "assets/rooms/akropolis/sanctuary/view_masks.inc"
#include "assets/rooms/akropolis/sanctuary/header.inc"
#define ROOM_PART _gAcropolisSanctuaryCollisionGridNormals
#include "assets/rooms/akropolis/sanctuary/geometry.inc"
#include "assets/rooms/akropolis/sanctuary/view_boundaries.inc"
#include "assets/rooms/akropolis/sanctuary/action_triggers.inc"
#include "assets/rooms/akropolis/sanctuary/placements.inc"
#include "assets/rooms/akropolis/sanctuary/lights.inc"
#include "assets/rooms/akropolis/sanctuary/sprites.inc"
#include "assets/rooms/akropolis/sanctuary/cameras.inc"
#include "assets/rooms/akropolis/sanctuary/surfaces.inc"
#include "assets/rooms/akropolis/sanctuary/updates.inc"
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

#include "assets/rooms/akropolis/sanctuary/acropolis_sanctuary_model_09584.inc"

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
    };

    if (gameFlagGetNibble(GAME_FLAG_AKROPOLIS_BRIDGE_PROGRESS) == 0 && gGameSession->location.warp == ACROPOLIS_SANCTUARY_SCENE_ARRIVAL_WARP) {
        gameFlagSetNibble(GAME_FLAG_AKROPOLIS_BRIDGE_PROGRESS, ACROPOLIS_SANCTUARY_BRIDGE_PROGRESS_ENTERED);
        evsStartScriptWithSkip(_gAcropolisSanctuaryArrivalScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAcropolisSanctuaryArrivalSkipScript);
        areaApplySavedUpdates(_gAcropolisSanctuaryArrivalAreaUpdates);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = ACROPOLIS_SANCTUARY_ARRIVAL_SCENE_EVENT;
        gameFlagSetNibble(GAME_FLAG_OBSERVATORY_ROUTE_PROGRESS, ACROPOLIS_SANCTUARY_OBSERVATORY_ROUTE_COMPLETE);
        gameFlagSetNibble(GAME_FLAG_OBSERVATORY_EXIT_USED, 1);
        gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, ACROPOLIS_SANCTUARY_ARRIVAL_OBJECTIVE);
        gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
        gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, AKROPOLIS_STORY_DIALOGUE_ARMORED_ASSAILANT);
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
    if (request->areaId == GAME_AREA_AKROPOLIS_PROMENADE) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            if (gameFlagGetNibble(GAME_FLAG_SANCTUARY_EVENT_LATCH) == ACROPOLIS_SANCTUARY_EVENT_LATCH_NONE) {
                gameFlagSetNibble(GAME_FLAG_SANCTUARY_EVENT_LATCH, ACROPOLIS_SANCTUARY_EVENT_LATCH_PROMENADE_EXIT);
                areaSetCurrentObjectState(ACROPOLIS_SANCTUARY_PROMENADE_EXIT_OBJECT_INDEX, ACROPOLIS_SANCTUARY_PROMENADE_EXIT_OBJECT_STATE);
            }
        }
        if (request->areaId == GAME_AREA_AKROPOLIS_PROMENADE && request->queryOnly == ROOM_EVENT_EXECUTE) {
            destinationRoom = gameFlagGetNibble(GAME_FLAG_AKROPOLIS_BRIDGE_PROGRESS);
            if (destinationRoom == 0) {
                destinationRoom = ACROPOLIS_SANCTUARY_PROMENADE_ROOM_BEFORE_BRIDGE;
            } else {
                destinationRoom = ACROPOLIS_SANCTUARY_PROMENADE_ROOM_AFTER_BRIDGE;
            }
            reply->roomState = destinationRoom;
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
        if (gameFlagGetNibble(GAME_FLAG_AKROPOLIS_BRIDGE_PROGRESS) != 0 && placedActor != NULL) {
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
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = ACROPOLIS_SANCTUARY_VIEW_CUTSCENE_PLAYER;
                work->placementApplied                                 = work->placementApplied + 1;
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
                sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_SANCTUARY, 7), 0, 0);
                evsStartScriptWithSkip(_gAcropolisSanctuaryDepartureScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAcropolisSanctuaryDepartureSkipScript);
                task->state = task->state + 1;
            }
            break;

        case ACROPOLIS_SANCTUARY_CUTSCENE_RUNNING:
            // Event completion takes precedence over pending placement cues.
            if (gGameSession->eventState == 0) {
                sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = GAME_AREA_AKROPOLIS_ROOF_GARDEN;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage     = GAME_STAGE_AKROPOLIS;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = ACROPOLIS_SANCTUARY_CUTSCENE_EXIT_WARP;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = ACROPOLIS_SANCTUARY_CUTSCENE_EXIT_ROOM;
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
            sndEvtRequestScriptStop(SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_SANCTUARY, 7), SOUND_SCRIPT_STOP_NO_FADE);
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_SANCTUARY, 8), 0, 0);
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
    GameLocationKey* location = &session->location;
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
    location = &gGameSession->location;
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

/// Places a mosaic tile's centre from its texture origin and attempts its spawn.
///
/// Captures `tile`, `sizeClass`, `mosaicWork` and `mosaicCoord` in this function.
/// `tileIndex` is 0..71 and must identify `tile`, whose size class is 0 or 1.
/// U/V texels map to -Z/+Y parent-coordinate units at Q8/Q7 scales; subtracting
/// corner 0 changes the origin placement into a centre placement. XYZ narrow
/// to s16 and move.pad is preserved. Writes `sizeClass` and the work's temporary
/// offset; evaluates tileIndex once. Inputs must be stable and side-effect-free.
/// Expands to one block; spawn copies XYZ and borrows the live parent coordinate.
#define ACROPOLIS_SANCTUARY_SPAWN_MOSAIC_TILE(tileIndex)                                                                                                                                              \
    {                                                                                                                                                                                                 \
        enum {                                                                                                                                                                                        \
            ACROPOLIS_SANCTUARY_MOSAIC_V_TO_Y_SCALE_Q7 = 1145,                                                                                                                                        \
            ACROPOLIS_SANCTUARY_MOSAIC_V_FRACTION_BITS = 7,                                                                                                                                           \
            ACROPOLIS_SANCTUARY_MOSAIC_U_TO_Z_SCALE_Q8 = 2147,                                                                                                                                        \
            ACROPOLIS_SANCTUARY_MOSAIC_U_FRACTION_BITS = 8                                                                                                                                            \
        };                                                                                                                                                                                            \
        sizeClass           = tile->sizeClass;                                                                                                                                                        \
        mosaicWork->move.vx = 0;                                                                                                                                                                      \
        mosaicWork->move.vy = ((tile->originV * ACROPOLIS_SANCTUARY_MOSAIC_V_TO_Y_SCALE_Q7) >> ACROPOLIS_SANCTUARY_MOSAIC_V_FRACTION_BITS) - _gAcropolisSanctuaryMosaicTileCorners[sizeClass][0].vy;  \
        mosaicWork->move.vz = -((tile->originU * ACROPOLIS_SANCTUARY_MOSAIC_U_TO_Z_SCALE_Q8) >> ACROPOLIS_SANCTUARY_MOSAIC_U_FRACTION_BITS) - _gAcropolisSanctuaryMosaicTileCorners[sizeClass][0].vz; \
        effectSpawn(EFFECT_ACROPOLIS_SANCTUARY_MOSAIC_TILE, mosaicCoord, (tileIndex), &mosaicWork->move);                                                                                             \
    }

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
    if (gGameSession->location.view != ACROPOLIS_SANCTUARY_VIEW_MOSAIC_LATCH_SET && _gAcropolisSanctuaryMosaicCleanupLatched != ACROPOLIS_SANCTUARY_MOSAIC_CLEANUP_CLEAR &&
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
    if (gGameSession->location.view != ACROPOLIS_SANCTUARY_VIEW_MOSAIC_LATCH_SET && _gAcropolisSanctuaryMosaicCleanupLatched != ACROPOLIS_SANCTUARY_MOSAIC_CLEANUP_CLEAR &&
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
    if ((_gAcropolisSanctuaryFlameViewMasks[task->spawnArg1.value & ACROPOLIS_SANCTUARY_FLAME_PLACEMENT_MASK] >> (gGameSession->location.view - 1)) & 1) {
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

#include "../../shared/actor/actor_contacts_push_contact.inc.c"

#include "../../shared/actor/actor_contacts_push.inc.c"

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
