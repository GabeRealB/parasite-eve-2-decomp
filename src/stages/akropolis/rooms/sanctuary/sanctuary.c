#include "stages/akropolis/rooms/sanctuary.h"

#include "config.h"
#include "../../event_flags.h"
#include "../../room_states.h"
#include "../../event_flags.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/abs.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_210700.h"

#include "gameplay/actor_presentation.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_transitions.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/companion_load.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/gameflag.h"
#include "gameplay/items.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/event_flags.h"
#include "main/gameflag.h"
#include "main/gfx.h"
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

#include "rooms/room_common.h"
#include "../../../../shared/actor/actor_contacts.h"

#include "assets/rooms/akropolis/sanctuary/declarations.inc"
#include "assets/engine/animation/player_schema.h"

/// Task-owned state for the sanctuary cutscene's script-cued player placement.
typedef struct {
    Task* playerTask;       // Borrowed player task captured at creation
    u16   phase;            // Script phase (0 initial, 1 hold, 2 place player)
    u16   placementApplied; // Player setup status (0 pending, 1 applied)
    u8    field_8[4];       // Cleared at creation; role and field boundaries unproven
} _CutsceneWork;
STATIC_ASSERT_SIZEOF(_CutsceneWork, 0xC);

/// One tile of the sanctuary's mosaic: the piece of the mosaic sheet it shows and how it comes away from the wall.
typedef struct {
    s16 extentU;    // Texels from `originU` to the tile's last column (14 or 29)
    s16 extentV;    // Texels from `originV` to the tile's last row (15 or 31)
    s16 originU;    // Left texel column on the mosaic sheet, 0..165
    s16 originV;    // Top texel row on the mosaic sheet, 0..240
    s16 thrown;     // How the tile leaves (0 works loose slowly and sheds shards, 1 thrown clear)
    s16 holdFrames; // Frames the tile stays in place before it moves; 0 when thrown
    s16 sizeClass;  // Tile size (0 one 15 x 16 texel cell, 1 a 30 x 32 block)
} _MosaicTile;

STATIC_ASSERT_SIZEOF(_MosaicTile, 0xE);

/// Views selected by player placement and the mosaic's retained cleanup latch.
enum {
    VIEW_MOSAIC_LATCH_SET = 16,
};

/// Packed shard arguments, projection resources and mosaic motion limits.
enum {
    MOSAIC_MESSAGE_START     = 3101,
    MOSAIC_MIN_DEPTH         = 17,
    MOSAIC_TEXTURE_PAGE      = getTPage(1, GPU_BLEND_AVERAGE, 768, 0),
    MOSAIC_PALETTE           = getClut(0, 264),
    MOSAIC_GRAVITY           = 3,
    MOSAIC_DRIFT_FRAMES      = 60,
    MOSAIC_SPLIT_PERIOD      = 60U,
    MOSAIC_SPLIT_HEIGHT      = -3071,
    MOSAIC_BOUNCE_X          = -10048,
    MOSAIC_BOUNCE_Y          = -3799,
    MOSAIC_CLEAR_X           = -10432,
    MOSAIC_SHARD_DRIFT_SHIFT = 12,
    MOSAIC_SHARD_AIRBORNE    = 1 << MOSAIC_SHARD_DRIFT_SHIFT,
};

/// Script requests are distinct from the cutscene work's stored phases.
enum {
    CUTSCENE_CUE_HOLD         = 0,
    CUTSCENE_CUE_PLACE_PLAYER = 1,
    CUTSCENE_CUE_CHANGE_SOUND = 2,
};

/// Script-selected phases and placement status of the sanctuary cutscene.
enum {
    CUTSCENE_PHASE_INITIAL      = 0,
    CUTSCENE_PHASE_HOLD         = 1,
    CUTSCENE_PHASE_PLACE_PLAYER = 2,
    CUTSCENE_PLACEMENT_PENDING  = 0,
};

/// Scratch-stack block one mosaic shard's triangle is drawn from.
typedef struct {
    s32     otz;         // SZ3 / 4 of the projection, a quarter of the last corner's depth; draw threshold and ordering-table depth
    SVECTOR corners[3];  // Local corner workspace, then the world positions supplied to the projection
    u8      field_1C[4]; // Reserved with the block but never read or written; role and field boundaries unproven
} _MosaicShardScratch;
STATIC_ASSERT_SIZEOF(_MosaicShardScratch, 0x20);

/// Scratch-stack block one whole mosaic tile's quad is drawn from.
typedef struct {
    s32     otz;         // SZ3 / 4 of the projection, a quarter of the last corner's depth; draw threshold and ordering-table depth
    SVECTOR corners[4];  // Local corner workspace, then the world positions supplied to the projection
    u8      field_24[4]; // Reserved with the block but never read or written; role and field boundaries unproven
} _MosaicTileScratch;
STATIC_ASSERT_SIZEOF(_MosaicTileScratch, 0x28);

/// Gray levels of the sanctuary flame sprite, one byte per variant.
typedef struct {
    u8 gray[3]; // 0..255, written to R, G and B; variants 0 and 1 bright, 2 dim
} _FlameGray;
STATIC_ASSERT_SIZEOF(_FlameGray, 3);

/// `gPlayerStatus.weapon` is the equipped-weapon index the slot-3 msg 0x3E8 record is keyed on,
/// `gDisplayState.pendingMode` and `gAttachmentState.mode` gate the cutscene task's setup (the latter is 1 while the
/// attachment wheel is open) and `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId` picks which of the two
/// weapon-id bases that record uses.

/// Whole-unit X/Y/Z displacement left by the last call of `_actorContactApplyGridPushback`.
extern SVECTOR ActorContact_ScratchPosition;

/// Returns this carrier's persistent last contact-push correction.
static inline SVECTOR* _actorContactGetLastPushStep(void)
{
    return &ActorContact_ScratchPosition;
}

static void _akropolisSanctuaryUpdateRoomProgress(Task* unusedTask);
static void _akropolisSanctuaryInitializeRoomTask(Task* task);
static void _akropolisSanctuaryPlaceBlockerCollision(void);
static void _akropolisSanctuarySelectViewSpriteBatch(s32 useSecondBatch, s32 viewId);

/// Unsigned byte fields in a script's packed view-sprite selection.
enum {
    SCRIPT_SPRITE_FIELD_MASK  = 0xFF,
    SCRIPT_SPRITE_BATCH_SHIFT = 8,
};

/// Sets a flame billboard's square screen bounds around its projected center.
static inline void _akropolisSanctuarySetFlameQuadBounds(POLY_FT4* flameQuad, const RoomGlowSpriteScratch* flameScratch)
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
static const TaskFuncTable3 _gAkropolisSanctuaryRoomTaskStates = {
    .funcs = { _akropolisSanctuaryInitializeRoomTask, _akropolisSanctuaryUpdateRoomProgress, taskKill },
};

#define ROOM_PART _gAkropolisSanctuaryMosaicOffset
#include "assets/rooms/akropolis/sanctuary/markers.inc"

/// Resting flame gray for texture/palette variants 0..2.
static const _FlameGray _gAkropolisSanctuaryFlameBaseGray = { { 0x60, 0x60, 0x10 } };
/// Flame gray added on odd display frames for texture/palette variants 0..2.
static const _FlameGray _gAkropolisSanctuaryFlameFlickerGray = { { 0x10, 0x10, 0x08 } };
/// Unread; its role is unproven.
static const u8 _gAkropolisSanctuaryUnusedByte = 0xF1;

static s32 _akropolisSanctuaryStartMosaic(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg);

static void _akropolisSanctuaryCutsceneTask(Task* task);

static void _akropolisSanctuaryRequestCutsceneCue(s32 cue);

static void _akropolisSanctuaryApplyScriptSpriteSelection(u32 packedSelection);
static void _akropolisSanctuaryRestorePlayerWeaponAnimation(void);

static s32 _akropolisSanctuaryResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _akropolisSanctuaryRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);
static s32 _akropolisSanctuaryHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg);
static s32 _akropolisSanctuaryHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

#include "assets/rooms/akropolis/sanctuary/clips_1.inc"

/// Messages of the room task.
static TaskMessageEntry _gAkropolisSanctuaryRoomMessages[] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _akropolisSanctuaryResolveRoomTransition },
    { ROOM_MESSAGE_COMMAND, _akropolisSanctuaryHandleRoomCommand },
    { ROOM_MESSAGE_USE_KEY_ITEM, _akropolisSanctuaryRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _akropolisSanctuaryHandleRoomAction },
    { TASK_MESSAGE_TABLE_END, NULL },
};

#define ROOM_PART _gAkropolisSanctuaryPlayerSceneStartPlacement
#include "assets/rooms/akropolis/sanctuary/events.inc"
/// Nine animation sets copied into the player's sanctuary scene bank extension.
static AnimationSet* _gAkropolisSanctuaryPlayerSceneAnimationSets[9] = {
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

#define ROOM_PART _gAkropolisSanctuaryPlayerSceneBankExtensionRequest
#include "assets/rooms/akropolis/sanctuary/events.inc"
#include "assets/rooms/akropolis/sanctuary/clips_2.inc"

/// Single-set table installed for the player's sanctuary departure cutscene.
#include "assets/rooms/akropolis/sanctuary/cutscene_player_bank.inc"

#define ROOM_PART _gAkropolisSanctuaryUnusedSceneKey2
#include "assets/rooms/akropolis/sanctuary/events.inc"
/// Bodyless task recipe for the first sanctuary room action 1.
static TaskDesc _gAkropolisSanctuaryCutsceneTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_CONTROL } },
    .callback = _akropolisSanctuaryCutsceneTask,
    .data     = { .value = 0 },
};

#define ROOM_PART _gAkropolisSanctuaryBlockerCollisionGridNormals
#include "assets/rooms/akropolis/sanctuary/geometry.inc"

/// Mosaic-start route borrowed by the sanctuary room-effect task.
static TaskMessageEntry _gAkropolisSanctuaryRoomEffectMessages[] = {
    { MOSAIC_MESSAGE_START, _akropolisSanctuaryStartMosaic },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Row selectors shared with each mosaic tile's sizeClass.
enum {
    MOSAIC_TILE_SMALL = 0,
    MOSAIC_TILE_LARGE = 1,
};

/// Authored tile release modes; crumbling tiles add 0..7 random hold frames.
enum {
    MOSAIC_TILE_CRUMBLE = 0,
    MOSAIC_TILE_THROWN  = 1,
};

/// States stored in the mosaic debris cleanup latch.
enum {
    MOSAIC_CLEANUP_CLEAR   = 0,
    MOSAIC_CLEANUP_LATCHED = 1,
};

/// Seventy-two mosaic fragments, indexed by tile and shard spawn IDs 0..71.
static _MosaicTile _gAkropolisSanctuaryMosaicTiles[] = {
    { 29, 31, 0, 0, MOSAIC_TILE_CRUMBLE, 24, MOSAIC_TILE_LARGE },
    { 29, 31, 30, 0, MOSAIC_TILE_CRUMBLE, 16, MOSAIC_TILE_LARGE },
    { 14, 15, 60, 0, MOSAIC_TILE_CRUMBLE, 16, MOSAIC_TILE_SMALL },
    { 14, 15, 75, 0, MOSAIC_TILE_CRUMBLE, 16, MOSAIC_TILE_SMALL },
    { 14, 15, 90, 0, MOSAIC_TILE_CRUMBLE, 16, MOSAIC_TILE_SMALL },
    { 14, 15, 105, 0, MOSAIC_TILE_CRUMBLE, 16, MOSAIC_TILE_SMALL },
    { 29, 31, 120, 0, MOSAIC_TILE_CRUMBLE, 16, MOSAIC_TILE_LARGE },
    { 29, 31, 150, 0, MOSAIC_TILE_CRUMBLE, 24, MOSAIC_TILE_LARGE },
    { 29, 31, 60, 16, MOSAIC_TILE_CRUMBLE, 8, MOSAIC_TILE_LARGE },
    { 29, 31, 90, 16, MOSAIC_TILE_CRUMBLE, 8, MOSAIC_TILE_LARGE },
    { 29, 31, 0, 32, MOSAIC_TILE_CRUMBLE, 16, MOSAIC_TILE_LARGE },
    { 29, 31, 30, 32, MOSAIC_TILE_CRUMBLE, 8, MOSAIC_TILE_LARGE },
    { 29, 31, 120, 32, MOSAIC_TILE_CRUMBLE, 8, MOSAIC_TILE_LARGE },
    { 29, 31, 150, 32, MOSAIC_TILE_CRUMBLE, 16, MOSAIC_TILE_LARGE },
    { 29, 31, 60, 48, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_LARGE },
    { 29, 31, 90, 48, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_LARGE },
    { 29, 31, 0, 64, MOSAIC_TILE_CRUMBLE, 8, MOSAIC_TILE_LARGE },
    { 29, 31, 30, 64, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_LARGE },
    { 29, 31, 120, 64, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_LARGE },
    { 29, 31, 150, 64, MOSAIC_TILE_CRUMBLE, 8, MOSAIC_TILE_LARGE },
    { 14, 15, 60, 80, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_SMALL },
    { 29, 31, 75, 80, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_LARGE },
    { 14, 15, 105, 80, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_SMALL },
    { 14, 15, 0, 96, MOSAIC_TILE_CRUMBLE, 8, MOSAIC_TILE_SMALL },
    { 29, 31, 15, 96, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_LARGE },
    { 14, 15, 45, 96, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_SMALL },
    { 14, 15, 60, 96, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_SMALL },
    { 14, 15, 105, 96, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_SMALL },
    { 14, 15, 120, 96, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_SMALL },
    { 29, 31, 135, 96, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_LARGE },
    { 14, 15, 165, 96, MOSAIC_TILE_CRUMBLE, 8, MOSAIC_TILE_SMALL },
    { 14, 15, 0, 112, MOSAIC_TILE_CRUMBLE, 16, MOSAIC_TILE_SMALL },
    { 29, 31, 45, 112, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_LARGE },
    { 14, 15, 75, 112, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_SMALL },
    { 14, 15, 90, 112, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_SMALL },
    { 29, 31, 105, 112, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_LARGE },
    { 14, 15, 165, 112, MOSAIC_TILE_CRUMBLE, 16, MOSAIC_TILE_SMALL },
    { 14, 15, 0, 128, MOSAIC_TILE_CRUMBLE, 24, MOSAIC_TILE_SMALL },
    { 29, 31, 15, 128, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_LARGE },
    { 14, 15, 75, 128, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_SMALL },
    { 14, 15, 90, 128, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_SMALL },
    { 29, 31, 135, 128, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_LARGE },
    { 14, 15, 165, 128, MOSAIC_TILE_CRUMBLE, 24, MOSAIC_TILE_SMALL },
    { 14, 15, 0, 144, MOSAIC_TILE_CRUMBLE, 16, MOSAIC_TILE_SMALL },
    { 14, 15, 45, 144, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_SMALL },
    { 14, 15, 60, 144, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_SMALL },
    { 29, 31, 75, 144, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_LARGE },
    { 14, 15, 105, 144, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_SMALL },
    { 14, 15, 120, 144, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_SMALL },
    { 14, 15, 165, 144, MOSAIC_TILE_CRUMBLE, 16, MOSAIC_TILE_SMALL },
    { 29, 31, 0, 160, MOSAIC_TILE_CRUMBLE, 8, MOSAIC_TILE_LARGE },
    { 29, 31, 30, 160, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_LARGE },
    { 14, 15, 60, 160, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_SMALL },
    { 14, 15, 105, 160, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_SMALL },
    { 29, 31, 120, 160, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_LARGE },
    { 29, 31, 150, 160, MOSAIC_TILE_CRUMBLE, 8, MOSAIC_TILE_LARGE },
    { 29, 31, 60, 178, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_LARGE },
    { 29, 31, 90, 178, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_LARGE },
    { 29, 31, 0, 192, MOSAIC_TILE_CRUMBLE, 16, MOSAIC_TILE_LARGE },
    { 29, 31, 30, 192, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_LARGE },
    { 29, 31, 120, 192, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_LARGE },
    { 29, 31, 150, 192, MOSAIC_TILE_CRUMBLE, 16, MOSAIC_TILE_LARGE },
    { 29, 31, 60, 208, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_LARGE },
    { 29, 31, 90, 208, MOSAIC_TILE_THROWN, 0, MOSAIC_TILE_LARGE },
    { 29, 31, 0, 224, MOSAIC_TILE_CRUMBLE, 24, MOSAIC_TILE_LARGE },
    { 29, 31, 30, 224, MOSAIC_TILE_CRUMBLE, 8, MOSAIC_TILE_LARGE },
    { 29, 31, 120, 224, MOSAIC_TILE_CRUMBLE, 8, MOSAIC_TILE_LARGE },
    { 29, 31, 150, 224, MOSAIC_TILE_CRUMBLE, 24, MOSAIC_TILE_LARGE },
    { 14, 15, 60, 240, MOSAIC_TILE_CRUMBLE, 8, MOSAIC_TILE_SMALL },
    { 14, 15, 75, 240, MOSAIC_TILE_CRUMBLE, 16, MOSAIC_TILE_SMALL },
    { 14, 15, 90, 240, MOSAIC_TILE_CRUMBLE, 16, MOSAIC_TILE_SMALL },
    { 14, 15, 105, 240, MOSAIC_TILE_CRUMBLE, 8, MOSAIC_TILE_SMALL },
};
/// Centre-relative corners of the mosaic's two tile sizes, in local room units.
static SVECTOR _gAkropolisSanctuaryMosaicTileCorners[2][4] = {
    [MOSAIC_TILE_SMALL] = { { 0, -72, 63, 0 }, { 0, -72, -62, 0 }, { 0, 71, 63, 0 }, { 0, 71, -62, 0 } },
    [MOSAIC_TILE_LARGE] = { { 0, -143, 126, 0 }, { 0, -143, -125, 0 }, { 0, 143, 126, 0 }, { 0, 143, -125, 0 } },
};
/// Sixteen mosaic tile IDs receiving a second whole-tile effect.
static s16 _gAkropolisSanctuaryMosaicRepeatedTileIndices[] = {
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
static s32 _gAkropolisSanctuaryMosaicCleanupLatched = MOSAIC_CLEANUP_CLEAR;

#define ROOM_PART _gAkropolisSanctuaryFlamePositions
#include "assets/rooms/akropolis/sanctuary/markers.inc"
#include "assets/rooms/akropolis/sanctuary/view_masks.inc"
#include "assets/rooms/akropolis/sanctuary/header.inc"
#define ROOM_PART _gAkropolisSanctuaryCollisionGridNormals
#include "assets/rooms/akropolis/sanctuary/geometry.inc"
#include "assets/rooms/akropolis/sanctuary/view_boundaries.inc"
#include "assets/rooms/akropolis/sanctuary/action_triggers.inc"
#include "assets/rooms/akropolis/sanctuary/placements.inc"
#include "assets/rooms/akropolis/sanctuary/lights.inc"
#include "assets/rooms/akropolis/sanctuary/sprites.inc"
#include "assets/rooms/akropolis/sanctuary/cameras.inc"
#include "assets/rooms/akropolis/sanctuary/surfaces.inc"
#include "assets/rooms/akropolis/sanctuary/updates.inc"

#include "assets/rooms/item_box/model.inc"
DEFINE_ALIAS(_gItemBoxModel, gAkropolisSanctuaryItemBoxModel);

#include "assets/rooms/akropolis/sanctuary/red_key_pickup.inc"

/// Unread; its role is unproven.
static u32 _gAkropolisSanctuaryUnusedWord = 0xB000000;

/// Borrowed sanctuary cutscene task published for its event-script cues.
static Task* _gAkropolisSanctuaryCutsceneTask = NULL;

SVECTOR ActorContact_ScratchPosition = { 0, 0, 0, 0 };

/// Disables the sanctuary's nine weapon-CAP collision triggers.
static inline void _akropolisSanctuaryDisableWeaponTriggers(void)
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
    trigger6            = &_gAkropolisSanctuaryActionTriggers[6];
    trigger9            = &_gAkropolisSanctuaryActionTriggers[9];
    trigger10           = &_gAkropolisSanctuaryActionTriggers[10];
    trigger11           = &_gAkropolisSanctuaryActionTriggers[11];
    trigger12           = &_gAkropolisSanctuaryActionTriggers[12];
    trigger13           = &_gAkropolisSanctuaryActionTriggers[13];
    trigger14           = &_gAkropolisSanctuaryActionTriggers[14];
    trigger15           = &_gAkropolisSanctuaryActionTriggers[15];
    trigger16           = &_gAkropolisSanctuaryActionTriggers[16];

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

/// The first arrival from the promenade plays its scene and moves the story on; once object 28 is in state 2, the
/// weapon captions' triggers are disabled.
static void _akropolisSanctuaryUpdateRoomProgress(Task* unusedTask)
{
    enum {
        OBJECT   = 28,
        COMPLETE = 2,
    };

    if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_BRIDGE_PROGRESS) == AKROPOLIS_BRIDGE_NOT_STARTED && gGameSession->location.warp == AKROPOLIS_SANCTUARY_SCENE_ARRIVAL_WARP) {
        gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_BRIDGE_PROGRESS, AKROPOLIS_BRIDGE_SANCTUARY_ENTERED);
        evsStartScriptWithSkip(_gAkropolisSanctuaryArrivalScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAkropolisSanctuaryArrivalSkipScript);
        areaApplySavedUpdates(_gAkropolisSanctuaryArrivalAreaUpdates);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = AKROPOLIS_SANCTUARY_ARRIVAL_SCENE_EVENT;
        gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_SANCTUARY_ROUTE_PROGRESS, AKROPOLIS_SANCTUARY_ROUTE_SANCTUARY_REACHED);
        gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_OBSERVATORY_FORKED_ROAD_RIDE_SEEN, 1);
        gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, EVENT_FLAG_OBJECTIVE_AKROPOLIS_SANCTUARY_ENTERED);
        gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
        gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, AKROPOLIS_STORY_DIALOGUE_ARMORED_ASSAILANT);
    }
    if (areaGetCurrentObjectState(OBJECT) == COMPLETE) {
        _akropolisSanctuaryDisableWeaponTriggers();
    }
}

/// The promenade's door: leaving that way passes the bomb scene by if it has not played; it leads to the promenade's
/// state before or after the sanctuary.
static s32 _akropolisSanctuaryResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        INDEX = 0x13,
        STATE = 2,
    };
    s32 destinationRoom;

    *reply = *request;
    if (request->areaId == AKROPOLIS_ROOM_ID_PROMENADE) {
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SANCTUARY_BOMB_SCENE_STATE) == AKROPOLIS_SANCTUARY_BOMB_SCENE_NOT_PLAYED) {
                gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_SANCTUARY_BOMB_SCENE_STATE, AKROPOLIS_SANCTUARY_BOMB_SCENE_PASSED_BY);
                areaSetCurrentObjectState(INDEX, STATE);
            }
        }
        if (request->areaId == AKROPOLIS_ROOM_ID_PROMENADE && request->queryOnly == ROOM_EVENT_EXECUTE) {
            destinationRoom = gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_BRIDGE_PROGRESS);
            if (destinationRoom == AKROPOLIS_BRIDGE_NOT_STARTED) {
                destinationRoom = AKROPOLIS_ROOM_STATE_PROMENADE_BEFORE_SANCTUARY;
            } else {
                destinationRoom = AKROPOLIS_ROOM_STATE_PROMENADE_AFTER_SANCTUARY;
            }
            reply->roomState = destinationRoom;
        }
    }
    return ROOM_EVENT_DEPART;
}

/// No key item can be used here.
static s32 _akropolisSanctuaryRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Room command 0 plays Rupert's script while he is in the doorway.
static s32 _akropolisSanctuaryHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg)
{
    if (command == AKROPOLIS_SANCTUARY_ROOM_COMMAND_RUPERT && gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SANCTUARY_RUPERT_LEFT) == 0) {
        evsStartScript(_gAkropolisSanctuaryBlockerScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
    }
    return 0;
}

/// Room action 1 starts the bomb scene once.
static s32 _akropolisSanctuaryHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    if (request->actionId == AKROPOLIS_SANCTUARY_ROOM_ACTION_BOMB_SCENE && gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SANCTUARY_BOMB_SCENE_STATE) == AKROPOLIS_SANCTUARY_BOMB_SCENE_NOT_PLAYED) {
        gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_SANCTUARY_BOMB_SCENE_STATE, AKROPOLIS_SANCTUARY_BOMB_SCENE_PLAYED);
        taskSpawnFromTable(&_gAkropolisSanctuaryCutsceneTaskDesc, 0, 0, 0);
    }
    return 0;
}

/// Applies an event script's packed view-sprite batch selection.
static void _akropolisSanctuaryApplyScriptSpriteSelection(u32 packedSelection)
{
    _akropolisSanctuarySelectViewSpriteBatch(
        (packedSelection >> SCRIPT_SPRITE_BATCH_SHIFT) & SCRIPT_SPRITE_FIELD_MASK,
        packedSelection & SCRIPT_SPRITE_FIELD_MASK);
}

/// Restores the player weapon animation after the scripted sanctuary poses.
static void _akropolisSanctuaryRestorePlayerWeaponAnimation(void)
{
    enum { WEAPON_SPECIAL_RESTORE = 2 };

    if (gPlayerStatus.weapon == WEAPON_SPECIAL_RESTORE) {
        playerActorWriteWeaponAnimationBankIndex(&_gAkropolisSanctuaryPlayerSpecialWeaponRestoreRequest.source.index);
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &_gAkropolisSanctuaryPlayerSpecialWeaponRestoreRequest, 0);
    } else {
        playerActorWriteWeaponAnimationBankIndex(&_gAkropolisSanctuaryPlayerWeaponRestoreRequest.source.index);
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &_gAkropolisSanctuaryPlayerWeaponRestoreRequest, 0);
    }
}

/// Installs the room's messages; until Rupert has left, shows him, in his pose after the arrival scene once it has
/// played; places the cage's collision.
static void _akropolisSanctuaryInitializeRoomTask(Task* task)
{
    Task* placedActor;

    task->msgTable = _gAkropolisSanctuaryRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = task->state + 1;
    if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SANCTUARY_RUPERT_LEFT) != 1) {
        placedActor = sceneFindPlacedActor(AKROPOLIS_SANCTUARY_RUPERT_ACTOR);
        sceneSetPlacedActorDrawMode(AKROPOLIS_SANCTUARY_RUPERT_ACTOR, ACTOR_MESSAGE_DRAW_SHOW);
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_BRIDGE_PROGRESS) != AKROPOLIS_BRIDGE_NOT_STARTED && placedActor != NULL) {
            TASK_MESSAGE_DISPATCH_POINTER(placedActor, ACTOR_MESSAGE_PLAY_ANIMATION, &_gAkropolisSanctuaryRupertRestoredClip3Request, 0);
            TASK_MESSAGE_DISPATCH_POINTER(placedActor, ACTOR_MESSAGE_PLACE, &_gAkropolisSanctuaryRupertRestoredPlacement, 0);
        }
    }
    _akropolisSanctuaryPlaceBlockerCollision();
}

void akropolisSanctuaryRoomTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = _gAkropolisSanctuaryRoomTaskStates;
    handlers.funcs[task->state](task);
}

/// Installs the cutscene animation set and resets the player's first clip.
static inline void _akropolisSanctuaryCutsceneInstallPlayerAnimation(Task* task)
{
    enum { CUTSCENE_ANIMATION_BLEND_FRAMES = 15 };
    AnimationPlayRequest request;
    _CutsceneWork*       work;

    work = task->work;
    if (work->playerTask != NULL) {
        request.source.sets          = _gAkropolisSanctuaryCutscenePlayerAnimationSets;
        request.animationId          = 0;
        request.blend                = ANIMATION_BLEND_RESET;
        request.blendFrames          = CUTSCENE_ANIMATION_BLEND_FRAMES;
        request.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
        TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &request, 0);
    }
}

/// Places the player at the sanctuary cutscene mark with zero Euler angles.
static inline void _akropolisSanctuaryCutscenePlacePlayer(_CutsceneWork* work)
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
static inline void _akropolisSanctuaryCutsceneApplyPlacement(Task* task)
{
    enum {
        VIEW_CUTSCENE_PLAYER = 14,
    };
    _CutsceneWork* work;

    work = task->work;
    switch (work->phase) {
        case CUTSCENE_PHASE_INITIAL:
        case CUTSCENE_PHASE_HOLD:
            break;
        case CUTSCENE_PHASE_PLACE_PLAYER:
            if (work->placementApplied == CUTSCENE_PLACEMENT_PENDING) {
                playerActorRemoveEquipment();
                _akropolisSanctuaryCutsceneInstallPlayerAnimation(task);
                _akropolisSanctuaryCutscenePlacePlayer(work);
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = VIEW_CUTSCENE_PLAYER;
                work->placementApplied                                 = work->placementApplied + 1;
            }
            break;
    }
}

/// Runs the sanctuary player cutscene, then reloads the roof garden.
static void _akropolisSanctuaryCutsceneTask(Task* task)
{
    enum {
        INITIAL             = 0,
        RUNNING             = 1,
        WEAPON_BLEND_FRAMES = 15,
        EXIT_WARP           = 2,
        EXIT_ROOM           = 1,
    };
    AnimationPlayRequest request;
    _CutsceneWork*       allocatedWork;
    _CutsceneWork*       animationWork;
    s32                  weaponBankIndex;
    s32                  equippedWeapon;

    switch (task->state) {
        case INITIAL:
            if (gAttachmentState.mode != ATTACHMENT_MODE_WHEEL && gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                allocatedWork = memCalloc(sizeof(*allocatedWork), 0);
                task->work    = allocatedWork;
                if (allocatedWork == NULL) {
                    // The original continues into animation setup after this failure.
                    taskKill(task);
                } else {
                    memFillBytes(allocatedWork, 0, sizeof(*allocatedWork));
                    allocatedWork->playerTask        = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                    _gAkropolisSanctuaryCutsceneTask = task;
                }
                animationWork   = task->work;
                equippedWeapon  = gPlayerStatus.weapon;
                weaponBankIndex = PLAYER_ACTOR_SCENE_WEAPON_BANK(equippedWeapon);

                request.source.index         = weaponBankIndex;
                request.animationId          = PLAYER_ANIMATION_IDLE;
                request.blend                = ANIMATION_BLEND_INTERPOLATE;
                request.blendFrames          = WEAPON_BLEND_FRAMES;
                request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                // Keep the reloaded work alias separate for the animation dispatch.
                TASK_MESSAGE_DISPATCH_POINTER(animationWork->playerTask, ANIMATION_MESSAGE_PLAY, &request, 0);
                sndEvtRequestScriptStart(AKROPOLIS_SANCTUARY_SOUND(AKROPOLIS_SANCTUARY_SOUND_RADIO_CHATTER), 0, 0);
                evsStartScriptWithSkip(_gAkropolisSanctuaryDepartureScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAkropolisSanctuaryDepartureSkipScript);
                task->state = task->state + 1;
            }
            break;

        case RUNNING:
            // Event completion takes precedence over pending placement cues.
            if (gGameSession->eventState == 0) {
                sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = AKROPOLIS_ROOM_ID_ROOF_GARDEN;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage     = GAME_STAGE_AKROPOLIS;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = EXIT_WARP;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = EXIT_ROOM;
                gDisplayState.spriteVariant                                 = 1;
                taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
                taskKill(task);
                break;
            }
            _akropolisSanctuaryCutsceneApplyPlacement(task);
            break;
    }
}

/// Accepts a script cue for player placement or the cutscene sound transition.
static void _akropolisSanctuaryRequestCutsceneCue(s32 cue)
{
    _CutsceneWork* work = _gAkropolisSanctuaryCutsceneTask->work;

    switch (cue) {
        case CUTSCENE_CUE_HOLD:
            work->phase            = CUTSCENE_PHASE_HOLD;
            work->placementApplied = CUTSCENE_PLACEMENT_PENDING;
            return;
        case CUTSCENE_CUE_PLACE_PLAYER:
            work->phase            = CUTSCENE_PHASE_PLACE_PLAYER;
            work->placementApplied = CUTSCENE_PLACEMENT_PENDING;
            return;
        case CUTSCENE_CUE_CHANGE_SOUND:
            sndEvtRequestScriptStop(AKROPOLIS_SANCTUARY_SOUND(AKROPOLIS_SANCTUARY_SOUND_RADIO_CHATTER), SOUND_SCRIPT_STOP_NO_FADE);
            sndEvtRequestScriptStart(AKROPOLIS_SANCTUARY_SOUND(AKROPOLIS_SANCTUARY_SOUND_RADIO_CLICK), 0, 0);
            return;
    }
}

/// Restores the movable blocker in the room's live collision grid.
static void _akropolisSanctuaryPlaceBlockerCollision(void)
{
    enum {
        BLOCKER_SURFACE_CLASS = 1,
    };
    WorldCollisionGrid* roomGrid        = &_gAkropolisSanctuaryCollisionGrid;
    WorldCollisionGrid* blockerTemplate = &_gAkropolisSanctuaryBlockerCollisionGrid;
    SVECTOR             placementOffset;
    s32                 elementIndex;

    // Restore the template without copying the unused fourth vector components.
    for (elementIndex = 0; elementIndex < ARRAY_SIZE(_gAkropolisSanctuaryBlockerCollisionGridNormals); elementIndex++) {
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
        roomGrid->faces[elementIndex].surfaceClass    = BLOCKER_SURFACE_CLASS;
    }

    if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SANCTUARY_RUPERT_LEFT) == 0) {
        placementOffset.vx = 200;
        placementOffset.vy = 0;
        placementOffset.vz = 380;
    } else {
        placementOffset.vx = 3000;
        placementOffset.vy = 0;
        placementOffset.vz = 0;
    }

    for (elementIndex = 0; elementIndex < ARRAY_SIZE(_gAkropolisSanctuaryBlockerCollisionGridVerts); elementIndex++) {
        roomGrid->vertices[elementIndex].vx += placementOffset.vx;
        roomGrid->vertices[elementIndex].vy += placementOffset.vy;
        roomGrid->vertices[elementIndex].vz += placementOffset.vz;
    }
}

/// Selects one of the current room view's two alternative cached-sprite batches.
static void _akropolisSanctuarySelectViewSpriteBatch(s32 useSecondBatch, s32 viewId)
{
    GameSession*     session  = gGameSession;
    GameLocationKey* location = &session->location;
    SpriteBatch*     batches;

    batches = gSpriteAreaTables[location->stage - 1][session->spriteVariant - 1].areaViews[location->area - 1][(viewId & SCRIPT_SPRITE_FIELD_MASK) - 1].batches;
    if ((useSecondBatch & SCRIPT_SPRITE_FIELD_MASK) == 0) {
        batches[1].hidden = 0;
        batches[2].hidden = 1;
    } else {
        batches[1].hidden = 1;
        batches[2].hidden = 0;
    }
}

void akropolisSanctuaryRoomEffectTask(Task* task)
{
    enum {
        VIEW_MOSAIC_LATCH_HOLD = 12,
    };
    enum {
        ROOM_EFFECT_INITIAL  = 0,
        FLAME_DIM_ARGUMENT   = 2 << 8,
        FLAME_SMALL_ARGUMENT = 160 << 16,
        FLAME_DIM_COUNT      = 6,
    };
    GfxCoord*        roomCoord;
    GameLocationKey* location;
    s32              placementIndex;

    roomCoord = task->extra.coordBody->coord;
    if (task->state == ROOM_EFFECT_INITIAL) {
        for (placementIndex = 0; placementIndex < FLAME_DIM_COUNT; placementIndex++) {
            effectSpawn(EFFECT_AKROPOLIS_SANCTUARY_FLAME, roomCoord, placementIndex + FLAME_DIM_ARGUMENT, &_gAkropolisSanctuaryFlamePositions[placementIndex]);
        }
        for (placementIndex = FLAME_DIM_COUNT; placementIndex < ARRAY_SIZE(_gAkropolisSanctuaryFlamePositions); placementIndex++) {
            effectSpawn(EFFECT_AKROPOLIS_SANCTUARY_FLAME, roomCoord, placementIndex + FLAME_SMALL_ARGUMENT, &_gAkropolisSanctuaryFlamePositions[placementIndex]);
        }
        task->msgTable = _gAkropolisSanctuaryRoomEffectMessages;
        gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM_EFFECT);
        _gAkropolisSanctuaryMosaicCleanupLatched = MOSAIC_CLEANUP_CLEAR;
        task->state                              = task->state + 1;
    }
    location = &gGameSession->location;
    if (location->view == VIEW_MOSAIC_LATCH_SET) {
        _gAkropolisSanctuaryMosaicCleanupLatched = MOSAIC_CLEANUP_LATCHED;
    } else if (location->view != VIEW_MOSAIC_LATCH_HOLD) {
        _gAkropolisSanctuaryMosaicCleanupLatched = MOSAIC_CLEANUP_CLEAR;
    }
}

void akropolisSanctuaryMosaicTask(Task* task)
{
    enum { MOSAIC_INITIAL = 0 };
    EffectWork*  mosaicWork;
    GfxCoord*    mosaicCoord;
    _MosaicTile* tile;
    s32          sizeClass;
    s32          spawnIndex;
    s32          tileIndex;

/// Places a mosaic tile's center from its texture origin and attempts its spawn.
#define SPAWN_MOSAIC_TILE(tileIndex)                                                                                                            \
    {                                                                                                                                           \
        enum {                                                                                                                                  \
            V_TO_Y_SCALE_Q7 = 1145,                                                                                                             \
            V_FRACTION_BITS = 7,                                                                                                                \
            U_TO_Z_SCALE_Q8 = 2147,                                                                                                             \
            U_FRACTION_BITS = 8                                                                                                                 \
        };                                                                                                                                      \
        sizeClass           = tile->sizeClass;                                                                                                  \
        mosaicWork->move.vx = 0;                                                                                                                \
        mosaicWork->move.vy = ((tile->originV * V_TO_Y_SCALE_Q7) >> V_FRACTION_BITS) - _gAkropolisSanctuaryMosaicTileCorners[sizeClass][0].vy;  \
        mosaicWork->move.vz = -((tile->originU * U_TO_Z_SCALE_Q8) >> U_FRACTION_BITS) - _gAkropolisSanctuaryMosaicTileCorners[sizeClass][0].vz; \
        effectSpawn(EFFECT_AKROPOLIS_SANCTUARY_MOSAIC_TILE, mosaicCoord, (tileIndex), &mosaicWork->move);                                       \
    }

    mosaicWork  = task->spawnArg2.pointer;
    mosaicCoord = task->extra.coordBody->coord;
    if (task->state != MOSAIC_INITIAL) {
        effectKillTask(mosaicWork, task);
        return;
    }
    for (spawnIndex = 0; spawnIndex < ARRAY_SIZE(_gAkropolisSanctuaryMosaicTiles); spawnIndex++) {
        tile = &_gAkropolisSanctuaryMosaicTiles[spawnIndex];
        SPAWN_MOSAIC_TILE(spawnIndex);
    }
    for (spawnIndex = 0; spawnIndex < ARRAY_SIZE(_gAkropolisSanctuaryMosaicRepeatedTileIndices); spawnIndex++) {
        tileIndex = _gAkropolisSanctuaryMosaicRepeatedTileIndices[spawnIndex];
        tile      = &_gAkropolisSanctuaryMosaicTiles[tileIndex];
        SPAWN_MOSAIC_TILE(tileIndex);
    }
    task->state = task->state + 1;

#undef SPAWN_MOSAIC_TILE
}

void akropolisSanctuaryMosaicTileTask(Task* task)
{
    enum {
        MOSAIC_TILE_SPLIT_AGE = 100,
    };
    EffectWork*         tileWork;
    GfxCoord*           tileCoord;
    _MosaicTileScratch* tileScratch;
    POLY_FT4*           tileQuad;
    SVECTOR*            scratchCorner;
    s32                 sizeClass;
    s32                 cornerIndex;
    s32                 shardCount;

    tileWork  = task->spawnArg2.pointer;
    sizeClass = _gAkropolisSanctuaryMosaicTiles[task->spawnArg1.value].sizeClass;
    tileCoord = task->extra.coordBody->coord;
    actorRenderComposeCoord(tileCoord);
    tileScratch = SCRATCH_STACK_RESERVE_BLOCK(_MosaicTileScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    // Transform the size-class corners into narrowed world positions.
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(tileScratch->corners); cornerIndex++) {
        tileScratch->corners[cornerIndex].vx = _gAkropolisSanctuaryMosaicTileCorners[sizeClass][cornerIndex].vx;
        // Byte addressing preserves separate store and GTE operand registers.
        scratchCorner     = (SVECTOR*)((u8*)tileScratch + cornerIndex * sizeof(SVECTOR) + OFFSET_OF(_MosaicTileScratch, corners));
        scratchCorner->vy = _gAkropolisSanctuaryMosaicTileCorners[sizeClass][cornerIndex].vy;
        scratchCorner->vz = _gAkropolisSanctuaryMosaicTileCorners[sizeClass][cornerIndex].vz;
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
    if (tileScratch->otz >= MOSAIC_MIN_DEPTH) {
        // Seed motion only on an accepted projection at age zero.
        if (tileWork->age == 0) {
            tileWork->scale = _gAkropolisSanctuaryMosaicTiles[task->spawnArg1.value].thrown;
            if (tileWork->scale != MOSAIC_TILE_CRUMBLE) {
                RANDOM_LCG_ADVANCE();
                tileWork->move.vx = -(RANDOM_LCG_DRAW() & 0xFF);
                RANDOM_LCG_ADVANCE();
                tileWork->move.vy = 0x40 - (RANDOM_LCG_DRAW() & 0x7F);
                RANDOM_LCG_ADVANCE();
                tileWork->move.vz = 0x40 - (RANDOM_LCG_DRAW() & 0x7F);
                RANDOM_LCG_ADVANCE();
                tileWork->pos.vx = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
                RANDOM_LCG_ADVANCE();
                tileWork->pos.vy = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
                RANDOM_LCG_ADVANCE();
                tileWork->pos.vz = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
                tileWork->angle  = _gAkropolisSanctuaryMosaicTiles[task->spawnArg1.value].holdFrames;
            } else {
                RANDOM_LCG_ADVANCE();
                tileWork->move.vx = -(RANDOM_LCG_DRAW() & 0x1F);
                RANDOM_LCG_ADVANCE();
                tileWork->move.vy = RANDOM_LCG_DRAW() & 7;
                RANDOM_LCG_ADVANCE();
                tileWork->move.vz = 4 - (RANDOM_LCG_DRAW() & 7);
                RANDOM_LCG_ADVANCE();
                tileWork->pos.vx = 0x20 - (RANDOM_LCG_DRAW() & 0x3F);
                RANDOM_LCG_ADVANCE();
                tileWork->pos.vy = 0x20 - (RANDOM_LCG_DRAW() & 0x3F);
                RANDOM_LCG_ADVANCE();
                tileWork->pos.vz = 0x20 - (RANDOM_LCG_DRAW() & 0x3F);
                RANDOM_LCG_ADVANCE();
                tileWork->angle = _gAkropolisSanctuaryMosaicTiles[task->spawnArg1.value].holdFrames +
                                  (RANDOM_LCG_DRAW() & 7);
            }
        }
        tileQuad->tpage = MOSAIC_TEXTURE_PAGE;
        tileQuad->clut  = MOSAIC_PALETTE;
        setShadeTex(tileQuad, 1);
        setSemiTrans(tileQuad, 1);
        tileQuad->u0 = _gAkropolisSanctuaryMosaicTiles[task->spawnArg1.value].originU;
        tileQuad->v0 = _gAkropolisSanctuaryMosaicTiles[task->spawnArg1.value].originV;
        tileQuad->u1 = _gAkropolisSanctuaryMosaicTiles[task->spawnArg1.value].originU +
                       _gAkropolisSanctuaryMosaicTiles[task->spawnArg1.value].extentU;
        tileQuad->v1 = _gAkropolisSanctuaryMosaicTiles[task->spawnArg1.value].originV;
        tileQuad->u2 = _gAkropolisSanctuaryMosaicTiles[task->spawnArg1.value].originU;
        tileQuad->v2 = _gAkropolisSanctuaryMosaicTiles[task->spawnArg1.value].originV +
                       _gAkropolisSanctuaryMosaicTiles[task->spawnArg1.value].extentV;
        tileQuad->u3 = _gAkropolisSanctuaryMosaicTiles[task->spawnArg1.value].originU +
                       _gAkropolisSanctuaryMosaicTiles[task->spawnArg1.value].extentU;
        tileQuad->v3 = _gAkropolisSanctuaryMosaicTiles[task->spawnArg1.value].originV +
                       _gAkropolisSanctuaryMosaicTiles[task->spawnArg1.value].extentV;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(tileScratch->otz),
                tileQuad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_MosaicTileScratch);
    if (tileWork->angle + MOSAIC_DRIFT_FRAMES < tileWork->age) {
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
        tileWork->move.vy       = tileWork->move.vy + MOSAIC_GRAVITY;
        if (tileWork->scale == MOSAIC_TILE_CRUMBLE) {
            RANDOM_LCG_ADVANCE();
            if ((u16)(RANDOM_LCG_DRAW() % (u32)MOSAIC_SPLIT_PERIOD) == 0 || tileCoord->coord.t[1] >= MOSAIC_SPLIT_HEIGHT) {
                RANDOM_LCG_ADVANCE();
                shardCount = (RANDOM_LCG_DRAW() & 3) + 1;
                for (cornerIndex = 0; cornerIndex < shardCount; cornerIndex++) {
                    effectSpawn(EFFECT_AKROPOLIS_SANCTUARY_MOSAIC_SHARD, tileCoord, task->spawnArg1.value | MOSAIC_SHARD_AIRBORNE, NULL);
                }
                tileWork->age = tileWork->age + MOSAIC_TILE_SPLIT_AGE;
            }
        }
    }
    if (tileCoord->coord.t[0] < MOSAIC_BOUNCE_X && tileCoord->coord.t[1] >= MOSAIC_BOUNCE_Y) {
        if (sizeClass != MOSAIC_TILE_SMALL) {
            RANDOM_LCG_ADVANCE();
            // Retain this temporary for the later shard count to preserve allocation.
            sizeClass = RANDOM_LCG_DRAW() & 3;
            if (sizeClass != 0) {
                sizeClass = sizeClass + 1;
                for (cornerIndex = 0; cornerIndex < sizeClass; cornerIndex++) {
                    effectSpawn(EFFECT_AKROPOLIS_SANCTUARY_MOSAIC_SHARD, tileCoord, task->spawnArg1.value, NULL);
                }
                tileWork->age = tileWork->age + MOSAIC_TILE_SPLIT_AGE;
            } else {
                tileCoord->coord.t[1] -= tileWork->move.vy * 2;
                tileWork->move.vy      = -(tileWork->move.vy >> 1);
            }
        } else {
            tileCoord->coord.t[1] -= tileWork->move.vy * 2;
            tileWork->move.vy      = -(tileWork->move.vy >> 1);
        }
    }
    if (gGameSession->location.view != VIEW_MOSAIC_LATCH_SET && _gAkropolisSanctuaryMosaicCleanupLatched != MOSAIC_CLEANUP_CLEAR &&
        (tileCoord->coord.t[0] < MOSAIC_CLEAR_X ||
         (tileCoord->coord.t[0] < MOSAIC_BOUNCE_X && tileCoord->coord.t[1] >= MOSAIC_BOUNCE_Y))) {
        tileWork->age = tileWork->age + MOSAIC_DRIFT_FRAMES;
    }
    tileWork->age = tileWork->age + 1;
}

void akropolisSanctuaryMosaicShardTask(Task* task)
{
    enum {
        TILE_INDEX_MASK          = 0xFFF,
        SHARD_DRIFT_MASK         = 0xF,
        SHARD_SIZE_SHIFT         = 16,
        SHARD_SIZE_ONE           = 1 << 12,
        SHARD_SIZE_FRACTION_BITS = 12,
        SHARD_MIN_SPLIT_SIZE     = (SHARD_SIZE_ONE / 4) + 1,
        SHARD_CHILD_SIZE_SHIFT   = SHARD_SIZE_SHIFT - 1,
        SHARD_MAX_BOUNCES        = 2,
    };
    EffectWork*          shardWork;
    GfxCoord*            shardCoord;
    _MosaicShardScratch* shardScratch;
    POLY_FT3*            shardTriangle;
    SVECTOR*             baseCorners;
    s32                  sizeFactor;
    s32                  packedSize;
    s32                  cornerIndex;
    s32                  shardCount;
    s32                  childParams;
    SVECTOR*             scratchCorner;

    shardWork  = task->spawnArg2.pointer;
    shardCoord = task->extra.coordBody->coord;
    if (shardWork->age >= MOSAIC_DRIFT_FRAMES + 1 || shardWork->index >= SHARD_MAX_BOUNCES) {
        effectKillTask(shardWork, task);
        return;
    }
    actorRenderComposeCoord(shardCoord);
    shardScratch = SCRATCH_STACK_RESERVE_BLOCK(_MosaicShardScratch);
    // Unpack once, preserving signed size and the unsigned zero test.
    if (shardWork->age == 0) {
        shardWork->scale = (task->spawnArg1.value >> MOSAIC_SHARD_DRIFT_SHIFT) & SHARD_DRIFT_MASK;
        packedSize       = (s16)(task->spawnArg1.value >> SHARD_SIZE_SHIFT);
        sizeFactor       = SHARD_SIZE_ONE;
        if ((u16)packedSize != 0) {
            sizeFactor = packedSize;
        }
        shardWork->angle      = sizeFactor;
        task->spawnArg1.value = task->spawnArg1.value & TILE_INDEX_MASK;
        if (shardWork->scale != 0) {
            RANDOM_LCG_ADVANCE();
            shardWork->move.vx = 8 - (RANDOM_LCG_DRAW() & 0xF);
            RANDOM_LCG_ADVANCE();
            shardWork->move.vy = RANDOM_LCG_DRAW() & 0xF;
            RANDOM_LCG_ADVANCE();
            shardWork->move.vz = 8 - (RANDOM_LCG_DRAW() & 0xF);
        } else {
            RANDOM_LCG_ADVANCE();
            shardWork->move.vx = 8 - (RANDOM_LCG_DRAW() & 0xF);
            RANDOM_LCG_ADVANCE();
            shardWork->move.vy = -(RANDOM_LCG_DRAW() & 0x1F);
            RANDOM_LCG_ADVANCE();
            shardWork->move.vz = 8 - (RANDOM_LCG_DRAW() & 0xF);
        }
        RANDOM_LCG_ADVANCE();
        shardWork->pos.vx = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
        RANDOM_LCG_ADVANCE();
        shardWork->pos.vy = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
        RANDOM_LCG_ADVANCE();
        shardWork->pos.vz = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
    }
    gte_SetTransMatrix(&GsWSMATRIX);
    baseCorners = _gAkropolisSanctuaryMosaicTileCorners[MOSAIC_TILE_SMALL];
    // Scale and transform the three shard corners before projecting them together.
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(shardScratch->corners); cornerIndex++) {
        shardScratch->corners[cornerIndex].vx = baseCorners[cornerIndex].vx;
        // Byte addressing preserves separate store and GTE operand registers.
        scratchCorner     = (SVECTOR*)((u8*)shardScratch + cornerIndex * sizeof(SVECTOR) + OFFSET_OF(_MosaicShardScratch, corners));
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
    if (shardScratch->otz >= MOSAIC_MIN_DEPTH) {
        shardTriangle->tpage = MOSAIC_TEXTURE_PAGE;
        shardTriangle->clut  = MOSAIC_PALETTE;
        setShadeTex(shardTriangle, 1);
        setSemiTrans(shardTriangle, 1);
        shardTriangle->u0 = _gAkropolisSanctuaryMosaicTiles[task->spawnArg1.value].originU;
        shardTriangle->v0 = _gAkropolisSanctuaryMosaicTiles[task->spawnArg1.value].originV;
        shardTriangle->u1 = _gAkropolisSanctuaryMosaicTiles[task->spawnArg1.value].originU +
                            ((_gAkropolisSanctuaryMosaicTiles[task->spawnArg1.value].extentU * shardWork->angle) >> SHARD_SIZE_FRACTION_BITS);
        shardTriangle->v1 = _gAkropolisSanctuaryMosaicTiles[task->spawnArg1.value].originV;
        shardTriangle->u2 = _gAkropolisSanctuaryMosaicTiles[task->spawnArg1.value].originU;
        shardTriangle->v2 = _gAkropolisSanctuaryMosaicTiles[task->spawnArg1.value].originV +
                            ((_gAkropolisSanctuaryMosaicTiles[task->spawnArg1.value].extentV * shardWork->angle) >> SHARD_SIZE_FRACTION_BITS);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(shardScratch->otz),
                shardTriangle);
    }
    shardCoord->coord.t[0] += shardWork->move.vx;
    shardCoord->coord.t[1] += shardWork->move.vy;
    SCRATCH_STACK_RELEASE_BLOCK(_MosaicShardScratch);
    shardCoord->coord.t[2] += shardWork->move.vz;
    gfxRotMatrixYXZ(&shardCoord->coord, &shardWork->pos, GRAPHICS_ROTATION_COMPOSE);
    shardCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    shardWork->move.vy       = shardWork->move.vy + MOSAIC_GRAVITY;
    // Bounce at the floor region or split into children with half the encoded size.
    if (shardCoord->coord.t[0] < MOSAIC_BOUNCE_X && shardCoord->coord.t[1] >= MOSAIC_BOUNCE_Y) {
        RANDOM_LCG_ADVANCE();
        shardCount = RANDOM_LCG_DRAW() & 1;
        if (shardWork->angle >= SHARD_MIN_SPLIT_SIZE && shardCount != 0) {
            shardCount = shardCount + 1;
            for (cornerIndex = 0; cornerIndex < shardCount; cornerIndex++) {
                effectSpawn(EFFECT_AKROPOLIS_SANCTUARY_MOSAIC_SHARD, shardCoord, task->spawnArg1.value | (shardWork->angle << SHARD_CHILD_SIZE_SHIFT), NULL);
            }
            shardWork->age = shardWork->age + MOSAIC_DRIFT_FRAMES;
        } else {
            shardCoord->coord.t[1] -= shardWork->move.vy * 2;
            shardWork->move.vy      = -(shardWork->move.vy >> 1);
            shardWork->index        = shardWork->index + 1;
        }
    } else if (shardWork->angle >= SHARD_MIN_SPLIT_SIZE) {
        RANDOM_LCG_ADVANCE();
        if ((u16)(RANDOM_LCG_DRAW() % (u32)MOSAIC_SPLIT_PERIOD) == 0 || shardCoord->coord.t[1] >= MOSAIC_SPLIT_HEIGHT) {
            for (cornerIndex = 0; cornerIndex < 2; cornerIndex++) {
                childParams = (shardWork->angle << SHARD_CHILD_SIZE_SHIFT) | MOSAIC_SHARD_AIRBORNE;
                effectSpawn(EFFECT_AKROPOLIS_SANCTUARY_MOSAIC_SHARD, shardCoord, task->spawnArg1.value | childParams, NULL);
            }
            shardWork->age = shardWork->age + MOSAIC_DRIFT_FRAMES;
        }
    }
    if (gGameSession->location.view != VIEW_MOSAIC_LATCH_SET && _gAkropolisSanctuaryMosaicCleanupLatched != MOSAIC_CLEANUP_CLEAR &&
        (shardCoord->coord.t[0] < MOSAIC_CLEAR_X ||
         (shardCoord->coord.t[0] < MOSAIC_BOUNCE_X && shardCoord->coord.t[1] >= MOSAIC_BOUNCE_Y))) {
        shardWork->age = shardWork->age + MOSAIC_DRIFT_FRAMES;
    }
    shardWork->age = shardWork->age + 1;
}

void akropolisSanctuaryFlameTask(Task* task)
{
    enum {
        STATE_INITIAL    = 0,
        PLACEMENT_MASK   = 0xF,
        VARIANT_SHIFT    = 8,
        VARIANT_MASK     = 3,
        SIZE_SHIFT       = 16,
        SIZE_MASK        = 0xFFF,
        SIZE_BITS        = SIZE_MASK << SIZE_SHIFT,
        DEFAULT_SIZE     = 640,
        MIN_DEPTH        = 17,
        TEXTURE_PAGE     = getTPage(0, GPU_BLEND_ADD, 704, 0),
        PALETTE_WORDS    = 16,
        PALETTE_Y        = 270,
        TEXTURE_STRIDE_U = 40,
        TEXTURE_EXTENT   = 39,
    };
    EffectWork*            flameWork;
    GfxCoord*              flameCoord;
    RoomGlowSpriteScratch* flameScratch;
    POLY_FT4*              flameQuad;
    _FlameGray             baseGray;
    _FlameGray             flickerGray;
    s32                    spawnParams;
    s32                    grayLevel;

    flameWork  = task->spawnArg2.pointer;
    flameCoord = task->extra.coordBody->coord;
    if ((_gAkropolisSanctuaryFlameViewMasks[task->spawnArg1.value & PLACEMENT_MASK] >> (gGameSession->location.view - 1)) & 1) {
        actorRenderComposeCoord(flameCoord);
        flameScratch = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);
        if (task->state == STATE_INITIAL) {
            // Initialize on the first visible update, then retain only the placement index.
            baseGray              = _gAkropolisSanctuaryFlameBaseGray;
            flickerGray           = _gAkropolisSanctuaryFlameFlickerGray;
            spawnParams           = task->spawnArg1.value;
            flameWork->scale      = (spawnParams & SIZE_BITS)
                                        ? ((spawnParams >> SIZE_SHIFT) & SIZE_MASK)
                                        : DEFAULT_SIZE;
            flameWork->angle      = (task->spawnArg1.value >> VARIANT_SHIFT) & VARIANT_MASK;
            task->spawnArg1.value = task->spawnArg1.value & PLACEMENT_MASK;
            flameWork->period     = baseGray.gray[flameWork->angle];
            flameWork->step       = flickerGray.gray[flameWork->angle];
            task->state++;
        }
        // Project the composed center after narrowing it to signed 16-bit coordinates.
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
        if (flameScratch->otz >= MIN_DEPTH) {
            grayLevel        = (u8)flameWork->period + (gDisplayState.animFrame & 1) * flameWork->step;
            flameQuad->tpage = TEXTURE_PAGE;
            setSemiTrans(flameQuad, 1);
            setRGB0(flameQuad, grayLevel, grayLevel, grayLevel);
            flameQuad->clut          = getClut(flameWork->angle * PALETTE_WORDS, PALETTE_Y);
            flameQuad->u0            = flameWork->angle * TEXTURE_STRIDE_U;
            flameQuad->v0            = 0;
            flameQuad->u1            = flameWork->angle * TEXTURE_STRIDE_U + TEXTURE_EXTENT;
            flameQuad->v1            = 0;
            flameQuad->u2            = flameWork->angle * TEXTURE_STRIDE_U;
            flameQuad->v2            = TEXTURE_EXTENT;
            flameQuad->u3            = flameWork->angle * TEXTURE_STRIDE_U + TEXTURE_EXTENT;
            flameQuad->v3            = TEXTURE_EXTENT;
            flameScratch->halfExtent = (flameWork->scale * TEXTURE_EXTENT) / flameScratch->otz;
            _akropolisSanctuarySetFlameQuadBounds(flameQuad, flameScratch);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(flameScratch->otz),
                    flameQuad);
        }
        SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
    }
}

/// Starts the mosaic breakup from the room-effect task's placement coordinate.
static s32 _akropolisSanctuaryStartMosaic(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    GfxCoord* mosaicCoord  = task->extra.coordBody->coord;
    SVECTOR   mosaicOffset = _gAkropolisSanctuaryMosaicOffset;

    effectSpawn(EFFECT_AKROPOLIS_SANCTUARY_MOSAIC, mosaicCoord, 0, &mosaicOffset);
    return 0;
}

#include "../../../../shared/actor/actor_contacts_push_contact.inc.c"

#include "../../../../shared/actor/actor_contacts_push.inc.c"

void akropolisSanctuaryItemVisibilityTask(Task* task)
{
    enum {
        HIDDEN_VIEW_FIRST  = 11,
        HIDDEN_VIEW_SECOND = 13,
        STATE_HIDDEN       = 2,
    };
    Enemy*     placedItem = task->spawnArg2.pointer;
    TmdObject* itemModel  = task->extra.tmd;
    s32        objectState;
    s32        viewIndex;

    objectState = areaGetCurrentObjectState((u8)placedItem->placeKey);
    viewIndex   = viewGetMappedIndex();
    if (viewIndex == HIDDEN_VIEW_FIRST || viewIndex == HIDDEN_VIEW_SECOND || objectState == STATE_HIDDEN) {
        itemModel->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        itemModel->flags    = TMD_OBJECT_FLAGGED_PASS;
        itemModel->otOffset = 0;
    }
}

/// Retains an unreferenced placed-object visibility hook in the sanctuary image.
static void _akropolisSanctuaryUpdateUnusedPlacedObjectVisibility(Task* task)
{
    enum {
        UNUSED_OBJECT_HIDDEN = 2,
    };
    Enemy*     placedObject;
    TmdObject* objectModel;
    s32        objectState;

    placedObject = task->spawnArg2.pointer;
    objectModel  = task->extra.tmd;
    objectState  = areaGetCurrentObjectState((u8)placedObject->placeKey);
    viewGetMappedIndex();
    if (objectState == UNUSED_OBJECT_HIDDEN) {
        objectModel->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        objectModel->flags    = TMD_OBJECT_FLAGGED_PASS;
        objectModel->otOffset = 0;
    }
}
