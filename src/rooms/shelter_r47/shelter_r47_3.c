#include "rooms/shelter_r47.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "shelter_r47_private.h"

#include "actors/actor_443500.h"

#include "gameplay/action_prompt.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/player_actor.h"
#include "gameplay/direction_input.h"
#include "gameplay/item_menu.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/view.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag_ids.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/pad_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/ui_types.h"

#include "overlay.h"

#include "rooms/shelter_b1_pod_service_gantry_light_types.h"

#include "rooms/room.h"
#include "rooms/room_common.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/room/room_cutscene.h"
#include "../../shared/room/action_prompt.h"

/// One-based saved view for the map terminal's initial B1 page (37).
///
/// The terminal preserves the previous saved view in its work before selecting
/// this page; closing restores that view. This is a view selector, not a page index.
#define SHELTER_R47_MAP_ENTRY_VIEW 0x25

/// Map width in screen pixels that starts reopening the side panel (181).
///
/// Tested after the map's easing update with >=; this is a map-width
/// threshold, independent of the side panel's fully open width.
#define SHELTER_R47_MAP_PANEL_OPEN_WIDTH_THRESHOLD 0xB5

/// Last timed-view update count before the terminal starts closing.
///
/// The counter starts at zero and increments before a strict > comparison.
/// Closing starts on update 301, including the opening fade in the count;
/// an event skip can close it earlier.
#define SHELTER_R47_MAP_SHOW_LIMIT 300

/// Selection updates before the screen-on sound during player use.
///
/// The signed-byte countdown is set on opening and decremented before testing
/// for zero. The sixth selection update plays the sound, even while CAP
/// playback or unsettled geometry prevents input.
#define SHELTER_R47_MAP_SCREEN_ON_DELAY 6

static void _shelterR47MapTerminalTask(Task* task);
static void _shelterR47MapTerminalPromptTask(Task* task);

static void _shelterR47MapTerminalInitTask(Task* task);
static void _shelterR47MapTerminalSelectHotspotTask(Task* task);
static void _shelterR47MapTerminalHandleHotspotTask(Task* task);
static void _actionPromptMoveCursors(Task* task);
static void _actionPromptDrawCursor(s32 cursorX, s32 cursorY, s32 cursorMode);
static void _actionPromptOutlineRect(const ActionPromptHotspot* hotspot, u8 red, u8 green, u8 blue);
static void _shelterR47MapTerminalBeginFadeOutTask(Task* task);
static void _shelterR47MapTerminalFadeOutTask(Task* task);
static void _shelterR47MapTerminalUpdateAndDraw(Task* task);
static s32  _actionPromptHitTest(ActionPromptHotspot* hotspots, s16 cursorX, s16 cursorY);
static void _shelterR47MapTerminalOpenTask(Task* task);
static void _shelterR47MapTerminalOpenCommandsTask(Task* task);
static void _shelterR47MapTerminalRestoreRoomTask(Task* task);
static void _shelterR47MapTerminalSwitchPageTask(Task* task);
static void _shelterR47MapTerminalReopenPageTask(Task* task);
static void _shelterR47MapTerminalTimedViewTask(Task* task);
static void _actionPromptReset(Task* task);

/// Destinations in the terminal's eleven-entry task state table.
enum {
    SHELTER_R47_MAP_STATE_INIT           = 0,
    SHELTER_R47_MAP_STATE_OPEN           = 1,
    SHELTER_R47_MAP_STATE_SELECT_HOTSPOT = 2,
    SHELTER_R47_MAP_STATE_OPEN_COMMANDS  = 3,
    SHELTER_R47_MAP_STATE_HANDLE_HOTSPOT = 4,
    SHELTER_R47_MAP_STATE_RESTORE_ROOM   = 5,
    SHELTER_R47_MAP_STATE_SWITCH_PAGE    = 6,
    SHELTER_R47_MAP_STATE_REOPEN_PAGE    = 7,
    SHELTER_R47_MAP_STATE_TIMED_VIEW     = 8,
    SHELTER_R47_MAP_STATE_BEGIN_FADE_OUT = 9,
    SHELTER_R47_MAP_STATE_FADE_OUT       = 10,
};

/// Room-bank sounds of the sole warp endpoint.
enum {
    SHELTER_R47_WARP_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, 0xB),
    SHELTER_R47_WARP_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, 0xA),
};

/// Action payloads of the room's trigger table.
///
/// The warp byte packs departure slot 1 and destination slot 2. Stair
/// parameter 0 packs surface class 3 and five steps; control bit 8 descends.
enum {
    SHELTER_R47_TRIGGER_POD_ACCESS_WARP        = 0x12,
    SHELTER_R47_TRIGGER_STAIR_STEPS_CLASS_3    = (3 << 4) | 5,
    SHELTER_R47_TRIGGER_STAIR_DESCEND          = 0x100,
    SHELTER_R47_TRIGGER_ACTION_COMPANION_ENTRY = 1,
    SHELTER_R47_TRIGGER_ACTION_POINT_2_SOUND   = 2,
    SHELTER_R47_TRIGGER_ACTION_BEGIN_TOUR      = 3,
    SHELTER_R47_TRIGGER_ACTION_MAP_OR_FOLLOWUP = 4,
    SHELTER_R47_TRIGGER_ACTION_OPEN_MAP        = 5,
    SHELTER_R47_TRIGGER_ACTION_OPEN_CONSOLE    = 6,
    SHELTER_R47_TRIGGER_COMMAND_CUTSCENE       = 1,
    SHELTER_R47_TRIGGER_COMMAND_REPEAT_ENTRY   = 8,
};

/// Low-nibble map-mark policies in this room's saved-area update list.
enum {
    SHELTER_R47_SAVED_AREA_MARK_CLEAR = 0,
    SHELTER_R47_SAVED_AREA_MARK_SET   = 1,
};

/// Placement/resource key of the tour's Pierce actor.
enum { SHELTER_R47_PIERCE_ENTRY_ID = 143 };

/// Subtractive overlay intensity, in byte levels per task tick.
///
/// The halfword accumulator crosses the byte range before it is clamped.
enum {
    SHELTER_R47_MAP_FADE_BLACK    = 0xFF,
    SHELTER_R47_MAP_FADE_RANGE    = 0x100,
    SHELTER_R47_MAP_FADE_IN_STEP  = 8,
    SHELTER_R47_MAP_FADE_OUT_STEP = 0x10,
};

/// Disc colours and perspective radius scales for this room's view glows.
///
/// Colours are RGB nibbles; disc pixel radii are scale * 64 / (camera Z / 4).
enum {
    SHELTER_R47_GLOW_COLOR_GREY                    = 0x444,
    SHELTER_R47_GLOW_COLOR_COOL_GREY               = 0x344,
    SHELTER_R47_GLOW_DISC_RADIUS_SCALE_SMALL       = 0x100,
    SHELTER_R47_GLOW_DISC_RADIUS_SCALE_MEDIUM      = 0x180,
    SHELTER_R47_GLOW_DISC_RADIUS_SCALE_LARGE       = 0x280,
    SHELTER_R47_GLOW_DISC_RADIUS_SCALE_EXTRA_LARGE = 0x300,
};

/// GPU addresses and colour state of the room-view sprite sources.
///
/// Both pages contain 8-bit textures at VRAM Y 0; their palette is at (0,255).
enum {
    SHELTER_R47_VIEW_SPRITE_PAGE_896      = getTPage(1, GPU_BLEND_AVERAGE, 896, 0),
    SHELTER_R47_VIEW_SPRITE_PAGE_960      = getTPage(1, GPU_BLEND_AVERAGE, 960, 0),
    SHELTER_R47_VIEW_SPRITE_CLUT          = getClut(0, 255),
    SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR = 128,
    SHELTER_R47_VIEW_SPRITE_OPAQUE        = 0,
};

/// Cue-2 bases in the current room's type-1 sound bank.
///
/// Cue 1 adds one; companion playback adds 100 to a base before requesting it.
enum {
    SHELTER_R47_SURFACE_CLASS_1_WALK_SOUND = 0x10000041,
    SHELTER_R47_SURFACE_CLASS_1_RUN_SOUND  = 0x10000043,
    SHELTER_R47_SURFACE_CLASS_2_WALK_SOUND = 0x10000059,
    SHELTER_R47_SURFACE_CLASS_2_RUN_SOUND  = 0x1000005B,
    SHELTER_R47_SURFACE_CLASS_3_WALK_SOUND = 0x10000015,
    SHELTER_R47_SURFACE_CLASS_3_RUN_SOUND  = 0x10000017,
    SHELTER_R47_SURFACE_CLASS_3_JUMP_SOUND = 0x10000019,
};

/// Eleven map-terminal handlers indexed by task state 0..10.
///
/// The dispatcher copies all eleven callback pointers by value before calling
/// one. Slots cover setup, opening, interaction, restoration, page changes and
/// timed/fade exits. The room overlay must remain loaded through dispatch.
static const TaskFuncTable11 _gShelterR47MapTerminalStateHandlers = {
    .funcs = {
        [SHELTER_R47_MAP_STATE_INIT]           = _shelterR47MapTerminalInitTask,
        [SHELTER_R47_MAP_STATE_OPEN]           = _shelterR47MapTerminalOpenTask,
        [SHELTER_R47_MAP_STATE_SELECT_HOTSPOT] = _shelterR47MapTerminalSelectHotspotTask,
        [SHELTER_R47_MAP_STATE_OPEN_COMMANDS]  = _shelterR47MapTerminalOpenCommandsTask,
        [SHELTER_R47_MAP_STATE_HANDLE_HOTSPOT] = _shelterR47MapTerminalHandleHotspotTask,
        [SHELTER_R47_MAP_STATE_RESTORE_ROOM]   = _shelterR47MapTerminalRestoreRoomTask,
        [SHELTER_R47_MAP_STATE_SWITCH_PAGE]    = _shelterR47MapTerminalSwitchPageTask,
        [SHELTER_R47_MAP_STATE_REOPEN_PAGE]    = _shelterR47MapTerminalReopenPageTask,
        [SHELTER_R47_MAP_STATE_TIMED_VIEW]     = _shelterR47MapTerminalTimedViewTask,
        [SHELTER_R47_MAP_STATE_BEGIN_FADE_OUT] = _shelterR47MapTerminalBeginFadeOutTask,
        [SHELTER_R47_MAP_STATE_FADE_OUT]       = _shelterR47MapTerminalFadeOutTask,
    },
};

/// Bodyless cursor-task recipe owned and killed by its map-terminal session.
///
/// Index 0 starts at state 0. Argument 1 selects port 0 with 1, port 1 with 2,
/// and both otherwise. Runs at the terminal's priority with zero metadata;
/// its returned handle is retained in the terminal's spawn argument 2.
static TaskDesc _gShelterR47MapTerminalPromptTaskDesc = {
    .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } },
    .callback = _shelterR47MapTerminalPromptTask,
    .data     = { .value = 0 },
};

TaskDesc gShelterR47MapTerminalTaskDesc = {
    .header   = { .fields = { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } },
    .callback = _shelterR47MapTerminalTask,
    .data     = { .value = 0 },
};

/// Ten fixed world-space glow centres, in signed game-coordinate units.
///
/// View 5 draws all ten, view 13 the pair at 8..9, view 14 entries 0, 6 and
/// 8..9, and view 44 entries 0 and 6. Drawers borrow these points for the call;
/// the pair requires two adjacent elements. Physical fixture identities are unproven.
static SVECTOR _gShelterR47ViewGlowCenters[10] = {
    { 0x3098, -2480, 7430, 0 },
    { 0x38AE, -4330, 6120, 0 },
    { 0x38AE, -4330, 7040, 0 },
    { 0x38AE, -4330, 8130, 0 },
    { 0x38AE, -4330, 9190, 0 },
    { 0x38AE, -4330, 0x2832, 0 },
    { 0x30DE, -2560, 7150, 0 },
    { 0x33FE, -2290, 0x276A, 0 },
    { 0x41FA, -2290, 6520, 0 },
    { 0x4240, -2420, 6930, 0 },
};

u8* gShelterR47ViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gShelterR47ViewCounts[1] = { 46 };

DirectionWarpEntry gShelterR47WarpEntries[1] = {
    {
        .player         = { { .word = 1024 }, 400, 0, 3300 },
        .companion      = { { .word = 1792 }, 2800, 0, 3700 },
        .arrivalSound   = SHELTER_R47_WARP_ARRIVAL_SOUND,
        .departureSound = SHELTER_R47_WARP_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = GAME_FLAG_MAP_MARK_B2_LABORATORY,
    },
};

WorldCollisionTrigger gShelterR47ViewBoundaryTriggers[6] = {
    { NULL, NULL, NULL, { 4491, -1712, 2957, 0 }, { { -546, -2576, -2563, 0 }, { 546, -2576, 2564, 0 }, { -546, 2576, -2563, 0 }, { 546, 2576, 2564, 0 } }, { 4014, 0, -856, 0 }, { 0, 0, 4096, 0 }, 3674, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4698, -1680, 2971, 0 }, { { 539, -2544, 2477, 0 }, { -539, -2544, -2477, 0 }, { 539, 2544, 2477, 0 }, { -539, 2544, -2477, 0 } }, { -4006, 0, 871, 0 }, { 0, 0, 4096, 0 }, 3584, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3241, -1344, 3296, 0 }, { { -901, -2544, 2605, 0 }, { 901, -2544, -2605, 0 }, { -901, 2544, 2605, 0 }, { 901, 2544, -2605, 0 } }, { -3881, 0, -1343, 0 }, { 0, 0, 4096, 0 }, 3744, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x31E1, -1377, 3168, 0 }, { { 894, -2576, -2563, 0 }, { -894, -2576, 2564, 0 }, { 894, 2576, -2563, 0 }, { -894, 2576, 2564, 0 } }, { 3875, 0, 1351, 0 }, { 0, 0, 4096, 0 }, 3736, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x34F1, -1344, 4864, 0 }, { { 2318, -2576, -3, 0 }, { -2318, -2576, 4, 0 }, { 2318, 2576, -3, 0 }, { -2318, 2576, 4, 0 } }, { 5, 0, 4107, 0 }, { 0, 0, 4096, 0 }, 3463, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3541, -1313, 4961, 0 }, { { -2434, -2576, 13, 0 }, { 2434, -2576, -12, 0 }, { -2434, 2576, 13, 0 }, { 2434, 2576, -12, 0 } }, { -22, 0, -4097, 0 }, { 0, 0, 4096, 0 }, 3537, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger gShelterR47ActionTriggers[13] = {
    { NULL, NULL, NULL, { 304, -48, 3232, 0 }, { { -528, 0, -928, 0 }, { 528, 0, -928, 0 }, { -528, 0, 928, 0 }, { 528, 0, 928, 0 } }, { 0, 4096, 0, 0 }, { 4096, 0, 0, 0 }, 1063, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL, SHELTER_R47_TRIGGER_POD_ACCESS_WARP, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x34A0, -64, 4645, 0 }, { { 928, 0, -528, 0 }, { 928, 0, 528, 0 }, { -928, 0, -528, 0 }, { -928, 0, 528, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 1063, WORLD_COLLISION_TRIGGER_ACTION_FACING, SHELTER_R47_TRIGGER_STAIR_STEPS_CLASS_3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x33F0, -1056, 5440, 0 }, { { 752, 0, -528, 0 }, { 752, 0, 528, 0 }, { -752, 0, -528, 0 }, { -752, 0, 528, 0 } }, { 0, 4098, 0, 0 }, { 201, 0, 4091, 0 }, 918, WORLD_COLLISION_TRIGGER_ACTION_FACING | SHELTER_R47_TRIGGER_STAIR_DESCEND, SHELTER_R47_TRIGGER_STAIR_STEPS_CLASS_3, 128, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7040, -64, 2688, 0 }, { { 928, 0, -2416, 0 }, { 928, 0, 2416, 0 }, { -928, 0, -2416, 0 }, { -928, 0, 2416, 0 } }, { 0, 4099, 0, 0 }, { 201, 0, 4091, 0 }, 2585, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, SHELTER_R47_TRIGGER_ACTION_COMPANION_ENTRY, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3780, -64, 2703, 0 }, { { 1536, 0, -1760, 0 }, { 320, 0, 1760, 0 }, { -320, 0, -1760, 0 }, { -1536, 0, 1760, 0 } }, { 0, 4098, 0, 0 }, { 201, 0, 4091, 0 }, 2332, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, SHELTER_R47_TRIGGER_ACTION_POINT_2_SOUND, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0x369F, -1056, 6319, 0 }, { { 2009, 0, -1407, 0 }, { -1726, 0, 1658, 0 }, { 1727, 0, -1657, 0 }, { -2008, 0, 1408, 0 } }, { 0, 4099, 0, 0 }, { 201, 0, 4091, 0 }, 2442, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, SHELTER_R47_TRIGGER_ACTION_BEGIN_TOUR, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3F80, -1043, 6048, 0 }, { { -528, 0, -928, 0 }, { 528, 0, -928, 0 }, { -528, 0, 928, 0 }, { 528, 0, 928, 0 } }, { 0, 4096, 0, 0 }, { -4017, 0, -799, 0 }, 1063, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_R47_TRIGGER_ACTION_MAP_OR_FOLLOWUP, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3F60, -1056, 7568, 0 }, { { -528, 0, -528, 0 }, { 528, 0, -528, 0 }, { -528, 0, 528, 0 }, { 528, 0, 528, 0 } }, { 0, 4102, 0, 0 }, { -4091, 0, -201, 0 }, 746, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_R47_TRIGGER_ACTION_OPEN_MAP, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3F0F, -1056, 9135, 0 }, { { 49, 0, -954, 0 }, { 923, 0, -15, 0 }, { -922, 0, 17, 0 }, { -47, 0, 955, 0 } }, { 0, 4103, 0, 0 }, { -2897, 0, -2897, 0 }, 955, WORLD_COLLISION_TRIGGER_ACTION_ROOM, SHELTER_R47_TRIGGER_ACTION_OPEN_CONSOLE, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x32E0, -1056, 8096, 0 }, { { -528, 0, -528, 0 }, { 528, 0, -528, 0 }, { -528, 0, 528, 0 }, { 528, 0, 528, 0 } }, { 0, 4102, 0, 0 }, { 4052, 0, 601, 0 }, 746, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_R47_TRIGGER_COMMAND_CUTSCENE, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x32E0, -1056, 6512, 0 }, { { -528, 0, -1056, 0 }, { 528, 0, -1056, 0 }, { -528, 0, 1056, 0 }, { 528, 0, 1056, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 1180, WORLD_COLLISION_TRIGGER_ACTION_CAP, 39, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x36C0, -1056, 9136, 0 }, { { -1584, 0, -464, 0 }, { 1584, 0, -464, 0 }, { -1584, 0, 464, 0 }, { 1584, 0, 464, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, -4096, 0 }, 1649, WORLD_COLLISION_TRIGGER_ACTION_CAP, 40, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7072, -64, 4704, 0 }, { { 2144, 0, -688, 0 }, { 2144, 0, 688, 0 }, { -2144, 0, -688, 0 }, { -2144, 0, 688, 0 } }, { 0, 4104, 0, 0 }, { -201, 0, -4091, 0 }, 2246, WORLD_COLLISION_TRIGGER_ACTION_CAP, SHELTER_R47_TRIGGER_COMMAND_REPEAT_ENTRY, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Pierce's load/spawn resource for placement variant 1, followed by its sentinel.
///
/// File 435 at group base 40 loads actor_443500; placement entry 143 selects
/// descriptor 0. The room layout borrows this list while its overlay is loaded.
static AreaResource _gShelterR47Variant1Resources[2] = {
    { .entryId = SHELTER_R47_PIERCE_ENTRY_ID, .fileNumber = 435, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskTable = gActor443500PierceTaskDescs },
    { .entryId = AREA_PLACEMENT_END },
};

/// Pierce's initial placement for variant 1, followed by the ending record.
///
/// Position is in world game units and yaw is zero. The actor borrows this
/// record for texture offsets: four 64-word pages and six CLUT rows.
static AreaPlacement _gShelterR47Variant1Placements[2] = {
    { .entryId = SHELTER_R47_PIERCE_ENTRY_ID, .x = 0x3E80, .y = -1000, .z = 5910, .texturePageOffset = 4, .clutRowOffset = 6 },
    { .entryId = AREA_PLACEMENT_END },
};

/// Loading-only companion-entry scene resource for placement variant 2.
///
/// File 434 at group base 10 loads actor_143400. It has no spawn descriptors;
/// the second record terminates the list. Storage belongs to the room overlay.
static AreaResource _gShelterR47Variant2Resources[2] = {
    { .entryId = 100, .fileNumber = 434, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR },
    { .entryId = AREA_PLACEMENT_END },
};

/// Empty placement list for variant 2; its resource is used by event scripts.
static AreaPlacement _gShelterR47Variant2Placements[1] = {
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gShelterR47AreaVariants[12] = {
    [1] = { .placements = _gShelterR47Variant1Placements, .resources = _gShelterR47Variant1Resources },
    [2] = { .placements = _gShelterR47Variant2Placements, .resources = _gShelterR47Variant2Resources },
};

SVECTOR gShelterR47CollisionGridNormals[13] = {
#include "assets/shelter_r47_collision_0ACCC_normals.inc"
};

SVECTOR gShelterR47CollisionGridVerts[66] = {
#include "assets/shelter_r47_collision_0ACCC_verts.inc"
};

WorldCollisionGridFace gShelterR47CollisionGridFaces[30] = {
#include "assets/shelter_r47_collision_0ACCC_faces.inc"
};

/// Concatenated cell face-index lists; each nonempty list ends with -1.
static s16 _gShelterR47CollisionGridCells[172] = {
#include "assets/shelter_r47_collision_0ACCC_cells.inc"
};

/// Address of a cell's first face ID in the room's packed face-list pool.
///
/// `halfwordOffset` counts s16 elements, including -1 terminators, rather than
/// grid cells or bytes. Evaluated once; the generated table supplies constant
/// list starts in 0..171. Captures the private pool and yields an s16* that
/// remains valid while the room is loaded. Bind only around the table include;
/// its collisionSource manifest entry selects this identifier for generation.
#define SHELTER_R47_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterR47CollisionGridCells[(halfwordOffset)])
/// X-major pointers for the grid's 5-by-3 cells; NULL entries are empty.
static s16* _gShelterR47CollisionGridTable[15] = {
#include "assets/shelter_r47_collision_0ACCC_table.inc"
};
#undef SHELTER_R47_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gShelterR47CollisionGrid = {
    .viewCoord   = NULL,
    .normals     = gShelterR47CollisionGridNormals,
    .vertices    = gShelterR47CollisionGridVerts,
    .faces       = gShelterR47CollisionGridFaces,
    .cellFaceIds = _gShelterR47CollisionGridTable,
    .xBias       = 1003,
    .zBias       = -1010,
    .cellCountX  = 5,
    .cellCountZ  = 3,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(gShelterR47CollisionGridFaces),
};

ViewCamera gShelterR47ViewCameras[46] = {
    { { { { 1403, 0, 3848 }, { 2757, 2857, -1005 }, { -2684, 2934, 978 } }, { -0x2DC3, 4293, -2180 } }, 207 },
    { { { { 693, 0, 4036 }, { 392, 4076, -67 }, { -4017, 398, 690 } }, { -7410, 1510, -2610 } }, 289 },
    { { { { 1395, 0, -3850 }, { -456, 4067, -165 }, { 3823, 485, 1386 } }, { -2490, 1520, -2540 } }, 207 },
    { { { { -3621, 0, 1914 }, { 765, 3754, 1447 }, { -1754, 1637, -3319 } }, { -0x3D54, 3550, -8260 } }, 289 },
    { { { { 3965, 0, 1025 }, { -3, 4095, 14 }, { -1025, -15, 3965 } }, { -0x3F7A, 2500, -1160 } }, 289 },
    { { { { -4017, 0, -797 }, { -627, 2530, 3159 }, { 492, 3221, -2481 } }, { -6717, 2637, -5072 } }, 329 },
    { { { { 4089, 0, -229 }, { 137, 3281, 2447 }, { 183, -2450, 3276 } }, { -6550, 170, -1790 } }, 143 },
    { { { { -4096, 0, 0 }, { 0, 2262, 3414 }, { 0, 3414, -2262 } }, { -7000, 6570, -9340 } }, 289 },
    { { { { -4095, 0, 0 }, { 0, 2763, 3023 }, { 0, 3023, -2763 } }, { -7000, 2930, -6100 } }, 289 },
    { { { { 3908, 0, 1224 }, { 276, 3990, -882 }, { -1193, 924, 3807 } }, { -8490, 2150, -770 } }, 289 },
    { { { { -1888, 0, 3634 }, { -233, 4087, -121 }, { -3627, -263, -1884 } }, { -9400, 1470, -5110 } }, 541 },
    { { { { 4095, 0, 0 }, { 0, 3947, 1094 }, { 0, -1094, 3947 } }, { -7000, 1530, -4260 } }, 289 },
    { { { { -2903, 0, -2888 }, { -254, 4080, 256 }, { 2877, 361, -2892 } }, { -0x389A, 2490, -8990 } }, 289 },
    { { { { 0, 0, 4096 }, { 1366, 3861, 0 }, { -3861, 1366, 0 } }, { -0x4538, 2880, -6500 } }, 289 },
    { { { { 4052, 0, -595 }, { 68, 4068, 468 }, { 591, -473, 4025 } }, { -0x3A98, 2050, -5600 } }, 541 },
    { { { { 2083, 0, 3526 }, { 535, 4048, -316 }, { -3485, 621, 2058 } }, { -390, 1520, 3400 } }, 282 },
    { { { { 664, 0, 4041 }, { 178, 4091, -29 }, { -4037, 181, 664 } }, { -1310, 1370, 2360 } }, 275 },
    { { { { 862, 0, 4004 }, { 1204, 3906, -259 }, { -3818, 1231, 822 } }, { -6593, 1887, 2833 } }, 246 },
    { { { { -3915, 0, -1201 }, { -30, 4094, 99 }, { 1201, 103, -3914 } }, { -7057, 873, -0x39AE } }, 269 },
    { { { { -458, 0, 4070 }, { 2704, 3060, 304 }, { -3041, 2722, -342 } }, { -5900, 5070, -8460 } }, 289 },
    { { { { 0, 0, -4096 }, { -343, 4081, 0 }, { 4081, 343, 0 } }, { 1531, 1760, -3200 } }, 853 },
    { { { { -4046, 0, -632 }, { 58, 4078, -373 }, { 629, -377, -4029 } }, { -8150, 1150, -5100 } }, 289 },
    { { { { -416, 0, 4074 }, { -259, 4087, -26 }, { -4066, -260, -415 } }, { -9288, 1352, -3496 } }, 541 },
    { { { { -331, 0, -4082 }, { 653, 4043, -53 }, { 4029, -655, -327 } }, { -3800, 800, -3800 } }, 680 },
    { { { { 2885, 0, 2906 }, { 102, 4093, -101 }, { -2905, 143, 2883 } }, { -7132, 1446, -1740 } }, 447 },
    { { { { 3776, 0, 1585 }, { 432, 3940, -1031 }, { -1525, 1118, 3633 } }, { -8348, 1907, -27 } }, 369 },
    { { { { 115, 0, -4094 }, { 87, 4095, 2 }, { 4093, -87, 115 } }, { -6000, 1508, -3300 } }, 447 },
    { { { { 2175, 0, 3470 }, { 1288, 3803, -807 }, { -3222, 1520, 2020 } }, { -9356, 2096, -1713 } }, 447 },
    { { { { -1119, 0, -3940 }, { 554, 4055, -157 }, { 3900, -576, -1108 } }, { -5100, 1200, -3750 } }, 312 },
    { { { { -394, 0, -4076 }, { 954, 3982, -92 }, { 3963, -958, -383 } }, { -4850, 800, -3750 } }, 380 },
    { { { { 3594, 0, -1963 }, { -1509, 2619, -2763 }, { 1255, 3148, 2299 } }, { -5820, 4480, -190 } }, 447 },
    { { { { 3745, 0, 1658 }, { 62, 4093, -140 }, { -1657, 153, 3742 } }, { -8710, 1080, 430 } }, 348 },
    { { { { -995, 0, 3973 }, { 105, 4094, 26 }, { -3971, 108, -995 } }, { -0x2C69, 998, -3892 } }, 358 },
    { { { { 2031, 0, 3556 }, { 1299, 3812, -742 }, { -3310, 1496, 1890 } }, { -7162, 1426, -2106 } }, 447 },
    { { { { -3914, 0, 1205 }, { 1172, 961, 3805 }, { -283, 3981, -918 } }, { -7360, 9500, -6160 } }, 257 },
    { { { { -3916, 0, 1198 }, { 1165, 960, 3807 }, { -281, 3981, -918 } }, { -7364, 9503, -6166 } }, 257 },
    { { { { -4096, 0, 0 }, { 0, 921, 3991 }, { 0, 3991, -921 } }, { -7000, 9483, -6180 } }, 418 },
    { { { { -4096, 0, 0 }, { 0, 921, 3991 }, { 0, 3991, -921 } }, { -7000, 9483, -6180 } }, 418 },
    { { { { -4096, 0, 0 }, { 0, 921, 3991 }, { 0, 3991, -921 } }, { -7000, 9483, -6180 } }, 418 },
    { { { { -4096, 0, 0 }, { 0, 921, 3991 }, { 0, 3991, -921 } }, { -7000, 9483, -6180 } }, 418 },
    { { { { -4096, 0, 0 }, { 0, 921, 3991 }, { 0, 3991, -921 } }, { -7000, 9483, -6180 } }, 418 },
    { { { { 606, 0, 4050 }, { 30, 4095, -4 }, { -4050, 30, 606 } }, { -9523, 1028, -2600 } }, 447 },
    { { { { 1999, 0, 3574 }, { 187, 4090, -104 }, { -3569, 214, 1996 } }, { -9422, 777, -982 } }, 329 },
    { { { { 2809, 0, 2980 }, { 1285, 3695, -1212 }, { -2688, 1766, 2535 } }, { -0x3412, 2770, -6570 } }, 289 },
    { { { { 1178, 0, -3922 }, { 529, 4058, 158 }, { 3887, -552, 1167 } }, { -1433, 500, -1642 } }, 380 },
    { { { { 1676, 0, -3737 }, { -1100, 3914, -493 }, { 3571, 1205, 1601 } }, { -7948, 590, -2568 } }, 329 },
};

/// 23 texture rectangles and sorting depths for mapped room view 1.
///
/// Positions are display-centred pixels; depths are unscaled OT sorting values.
/// The view's batches select source ranges in elements, without a sentinel.
/// Sources and textures remain borrowed while the room overlay is loaded.
static SpriteSource _gShelterR47View1SpriteSources[23] = {
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 72 } }, 128, 48, 386, { .fields = { 96, 72 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 24, 64 } }, 104, 56, 386, { .fields = { 96, 0 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 56 } }, 88, 64, 381, { .fields = { 104, 144 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 24, 48 } }, 64, 72, 376, { .fields = { 80, 144 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 40 } }, 48, 80, 391, { .fields = { 88, 192 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 24, 32 } }, 24, 88, 386, { .fields = { 72, 32 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 24, 24 } }, 0, 96, 380, { .fields = { 48, 32 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 16 } }, -16, 104, 376, { .fields = { 72, 216 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 48 } }, -72, 40, 1250, { .fields = { 112, 208 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 8, 72 } }, -56, 40, 1250, { .fields = { 120, 0 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 8, 64 } }, -48, 56, 1250, { .fields = { 120, 144 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 8, 48 } }, -40, 72, 1250, { .fields = { 104, 200 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 8, 24 } }, -32, 96, 1250, { .fields = { 72, 168 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 40, 24 } }, 16, 32, 1250, { .fields = { 32, 168 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 56, 16 } }, -40, 56, 1250, { .fields = { 8, 232 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 40, 16 } }, 16, 56, 1250, { .fields = { 32, 216 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 24 } }, -32, 72, 1250, { .fields = { 48, 96 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 40, 24 } }, 16, 72, 1250, { .fields = { 64, 232 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 24 } }, -24, 96, 1250, { .fields = { 48, 120 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 24 } }, 48, 8, 1250, { .fields = { 48, 144 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 24 } }, 56, 32, 1250, { .fields = { 56, 192 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 40, 32 } }, 56, 56, 1250, { .fields = { 56, 0 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 64, 32 } }, -48, 24, 1250, { .fields = { 32, 64 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
};

/// Source ranges for mapped room view 1, ending with `SPRITE_BATCH_END`.
///
/// The zero-count first record selects decoded image strips. Subsequent ranges
/// cover every source once. The two `field_6` bytes retain unproven metadata.
static SpriteBatch _gShelterR47View1SpriteBatches[4] = {
    { .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 8, .field_6 = { 1, 0 } },
    { .firstSprite = 8, .spriteCount = ARRAY_SIZE(_gShelterR47View1SpriteSources) - 8, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// 8 texture rectangles and sorting depths for mapped room view 2.
///
/// Positions are display-centred pixels; depths are unscaled OT sorting values.
/// The view's batches select source ranges in elements, without a sentinel.
/// Sources and textures remain borrowed while the room overlay is loaded.
static SpriteSource _gShelterR47View2SpriteSources[8] = {
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 8, 8 } }, -48, 0, 1300, { .fields = { 120, 240 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 48 } }, -104, 0, 1237, { .fields = { 80, 0 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 48 } }, -104, -48, 1237, { .fields = { 80, 48 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 48 } }, -104, -96, 1237, { .fields = { 80, 96 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 48 } }, -56, -48, 1300, { .fields = { 112, 144 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 48 } }, -56, -96, 1300, { .fields = { 112, 192 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 32 } }, -40, -96, 1350, { .fields = { 64, 192 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 8, 48 } }, -56, 0, 1300, { .fields = { 104, 144 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
};

/// Source ranges for mapped room view 2, ending with `SPRITE_BATCH_END`.
///
/// The zero-count first record selects decoded image strips. Subsequent ranges
/// cover every source once. The two `field_6` bytes retain unproven metadata.
static SpriteBatch _gShelterR47View2SpriteBatches[3] = {
    { .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gShelterR47View2SpriteSources), .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// 12 texture rectangles and sorting depths for mapped room view 3.
///
/// Positions are display-centred pixels; depths are unscaled OT sorting values.
/// The view's batches select source ranges in elements, without a sentinel.
/// Sources and textures remain borrowed while the room overlay is loaded.
static SpriteSource _gShelterR47View3SpriteSources[12] = {
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 8, 8 } }, 32, 0, 2300, { .fields = { 104, 88 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 48 } }, 72, -40, 2025, { .fields = { 112, 0 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 16 } }, 32, -56, 2300, { .fields = { 96, 192 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 24, 16 } }, 48, -56, 2125, { .fields = { 88, 72 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 16 } }, 72, -56, 2015, { .fields = { 96, 56 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 40 } }, 32, -40, 2300, { .fields = { 112, 192 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, 72, 737, { .fields = { 80, 144 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 40 } }, 80, 72, 737, { .fields = { 80, 0 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, 24, 737, { .fields = { 80, 96 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 48 } }, 96, 24, 737, { .fields = { 112, 48 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 24, 24 } }, 88, 0, 737, { .fields = { 104, 232 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 16 } }, 112, 8, 737, { .fields = { 96, 40 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
};

/// Source ranges for mapped room view 3, ending with `SPRITE_BATCH_END`.
///
/// The zero-count first record selects decoded image strips. Subsequent ranges
/// cover every source once. The two `field_6` bytes retain unproven metadata.
static SpriteBatch _gShelterR47View3SpriteBatches[4] = {
    { .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 6, .field_6 = { 1, 0 } },
    { .firstSprite = 6, .spriteCount = ARRAY_SIZE(_gShelterR47View3SpriteSources) - 6, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// View 3's right-side clip, restored to full-screen drawing at sorting depth 2500.
///
/// The rectangle uses draw-buffer pixels; the renderer adds the active buffer's
/// VRAM Y origin. The terminal record emits no clip or restore commands.
static SpriteDrawArea _gShelterR47View3DrawAreas[2] = {
    { .clipRect = { 195, 0, 125, 239 }, .restoreDepth = 2500 },
    { .restoreDepth = SPRITE_DRAW_AREA_END },
};

/// 82 texture rectangles and sorting depths for mapped room view 4.
///
/// Positions are display-centred pixels; depths are unscaled OT sorting values.
/// The view's batches select source ranges in elements, without a sentinel.
/// Sources and textures remain borrowed while the room overlay is loaded.
static SpriteSource _gShelterR47View4SpriteSources[82] = {
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 8, 8 } }, -88, 80, 1075, { .fields = { 72, 64 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 32 } }, -112, 88, 1050, { .fields = { 16, 224 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 32 } }, -80, 88, 1050, { .fields = { 0, 32 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 32 } }, -16, 88, 1056, { .fields = { 0, 64 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 32 } }, 16, 88, 1056, { .fields = { 16, 128 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 32 } }, 48, 88, 1062, { .fields = { 16, 160 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 32 } }, 80, 88, 1081, { .fields = { 16, 192 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 32 } }, 112, 88, 1081, { .fields = { 112, 160 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 32 } }, 144, 88, 1081, { .fields = { 0, 128 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 16 } }, -80, 72, 1045, { .fields = { 48, 48 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 16 } }, -48, 72, 1050, { .fields = { 112, 240 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 8 } }, -40, 64, 1075, { .fields = { 56, 136 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 16 } }, -16, 72, 1056, { .fields = { 80, 240 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 24 } }, 16, 64, 1057, { .fields = { 56, 96 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 32 } }, 48, 56, 1087, { .fields = { 112, 96 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 32 } }, 80, 56, 1112, { .fields = { 48, 168 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 8, 24 } }, 72, 32, 1187, { .fields = { 88, 176 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 24 } }, 80, 32, 1187, { .fields = { 56, 168 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 32 } }, 112, 56, 1112, { .fields = { 48, 136 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 24 } }, 144, 64, 1112, { .fields = { 72, 120 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 8 } }, 112, 48, 1237, { .fields = { 56, 128 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 32 } }, -48, 88, 1056, { .fields = { 48, 200 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 32 } }, -48, 8, 1050, { .fields = { 16, 96 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 16 } }, -80, 24, 1037, { .fields = { 40, 64 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 24 } }, -112, 40, 1000, { .fields = { 56, 144 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 24 } }, -112, 64, 1000, { .fields = { 72, 72 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 8, 24 } }, -112, 88, 1000, { .fields = { 56, 232 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 8, 24 } }, -120, 64, 962, { .fields = { 48, 232 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 8, 16 } }, -120, 88, 962, { .fields = { 96, 240 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 24 } }, -128, 40, 962, { .fields = { 88, 120 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 24, 16 } }, -160, 32, 950, { .fields = { 104, 240 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 24, 8 } }, -136, 32, 962, { .fields = { 48, 120 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 24 } }, -80, 40, 1037, { .fields = { 48, 24 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 16 } }, -80, 64, 1037, { .fields = { 48, 240 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 24 } }, -48, 40, 1050, { .fields = { 72, 192 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 24 } }, -48, 64, 1050, { .fields = { 72, 216 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 48 } }, -80, -8, 975, { .fields = { 112, 96 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 24 } }, -64, 8, 962, { .fields = { 64, 232 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 24 } }, -48, 0, 937, { .fields = { 112, 216 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 8, 8 } }, 64, 64, 1450, { .fields = { 88, 208 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, -80, 1450, { .fields = { 80, 144 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 48 } }, 112, -32, 1450, { .fields = { 80, 0 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 32 } }, 112, 16, 1450, { .fields = { 0, 0 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 48 } }, 64, -80, 1450, { .fields = { 80, 48 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 48 } }, 64, -32, 1450, { .fields = { 80, 192 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 48 } }, 64, 16, 1450, { .fields = { 32, 48 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 40 } }, 32, -72, 1445, { .fields = { 48, 96 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 48 } }, 32, -32, 1445, { .fields = { 80, 96 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 48 } }, 32, 16, 1445, { .fields = { 48, 0 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 8 } }, 48, 64, 1445, { .fields = { 56, 80 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 8, 8 } }, 24, 56, 1075, { .fields = { 64, 216 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 8, 8 } }, 80, 56, 1175, { .fields = { 64, 224 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 8, 8 } }, 32, 80, 1175, { .fields = { 64, 200 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 8, 8 } }, 8, 88, 1175, { .fields = { 64, 208 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 24, 24 } }, -16, 72, 1050, { .fields = { 88, 216 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 24, 24 } }, 8, 64, 1062, { .fields = { 88, 152 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 24, 24 } }, 32, 56, 1081, { .fields = { 80, 0 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 24, 24 } }, 56, 56, 1081, { .fields = { 80, 48 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 8 } }, 56, 48, 1081, { .fields = { 56, 88 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 8, 8 } }, 40, 48, 1345, { .fields = { 64, 192 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 8, 8 } }, 16, 56, 1317, { .fields = { 88, 200 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 8, 8 } }, -8, 64, 1295, { .fields = { 120, 152 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 24, 24 } }, -24, 72, 1292, { .fields = { 104, 24 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 24, 24 } }, 0, 64, 1305, { .fields = { 104, 72 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 24, 24 } }, 24, 56, 1330, { .fields = { 96, 192 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 24, 24 } }, 48, 48, 1375, { .fields = { 104, 128 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 8, 8 } }, 32, 48, 1367, { .fields = { 96, 184 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 8, 8 } }, 40, 48, 1387, { .fields = { 88, 144 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 8, 8 } }, 8, 56, 1330, { .fields = { 104, 176 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 8, 8 } }, -16, 64, 1305, { .fields = { 104, 184 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 24, 24 } }, -32, 72, 1300, { .fields = { 104, 48 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 24, 24 } }, -8, 64, 1320, { .fields = { 88, 96 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 24, 24 } }, 16, 56, 1362, { .fields = { 80, 24 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 16 } }, 40, 56, 1375, { .fields = { 80, 240 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 8, 8 } }, 24, 48, 1425, { .fields = { 104, 120 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 8, 8 } }, 32, 48, 1431, { .fields = { 96, 144 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 8, 8 } }, 0, 56, 1382, { .fields = { 96, 176 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 8, 8 } }, -24, 64, 1362, { .fields = { 112, 152 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 24, 24 } }, -40, 72, 1357, { .fields = { 104, 0 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 24, 24 } }, -16, 64, 1375, { .fields = { 120, 192 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_896, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 24, 24 } }, 8, 56, 1412, { .fields = { 56, 0 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 16 } }, 32, 56, 1436, { .fields = { 32, 32 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
};

/// Source ranges for mapped room view 4, ending with `SPRITE_BATCH_END`.
///
/// The zero-count first record selects decoded image strips. Subsequent ranges
/// cover every source once. The two `field_6` bytes retain unproven metadata.
static SpriteBatch _gShelterR47View4SpriteBatches[10] = {
    { .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 22, .field_6 = { 6, 0 } },
    { .firstSprite = 22, .spriteCount = 14, .field_6 = { 1, 0 } },
    { .firstSprite = 36, .spriteCount = 3, .field_6 = { 4, 0 } },
    { .firstSprite = 39, .spriteCount = 11, .field_6 = { 0, 0 } },
    { .firstSprite = 50, .spriteCount = 9, .field_6 = { 5, 0 } },
    { .firstSprite = 59, .spriteCount = 7, .field_6 = { 3, 0 } },
    { .firstSprite = 66, .spriteCount = 8, .field_6 = { 7, 0 } },
    { .firstSprite = 74, .spriteCount = ARRAY_SIZE(_gShelterR47View4SpriteSources) - 74, .field_6 = { 2, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// 15 texture rectangles and sorting depths for mapped room view 5.
///
/// Positions are display-centred pixels; depths are unscaled OT sorting values.
/// The view's batches select source ranges in elements, without a sentinel.
/// Sources and textures remain borrowed while the room overlay is loaded.
static SpriteSource _gShelterR47View5SpriteSources[15] = {
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 8, 8 } }, -48, 32, 925, { .fields = { 120, 248 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 16 } }, -112, 104, 700, { .fields = { 112, 232 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 48 } }, -96, 72, 750, { .fields = { 112, 0 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 48 } }, -80, 40, 875, { .fields = { 112, 48 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 24 } }, -64, 32, 925, { .fields = { 112, 160 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 16 } }, 112, 32, 1000, { .fields = { 64, 32 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 56, 16 } }, 56, 32, 1000, { .fields = { 56, 16 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 16 } }, 24, 32, 1000, { .fields = { 80, 0 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 40, 16 } }, -56, 32, 1000, { .fields = { 72, 80 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 24, 24 } }, -40, 48, 1000, { .fields = { 104, 208 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 24, 32 } }, -40, 72, 1000, { .fields = { 104, 128 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 16 } }, -16, 48, 1000, { .fields = { 80, 64 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 24 } }, 16, 48, 1000, { .fields = { 112, 184 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 16 } }, -16, 72, 1000, { .fields = { 80, 48 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 32 } }, 16, 72, 1000, { .fields = { 112, 96 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
};

/// Source ranges for mapped room view 5, ending with `SPRITE_BATCH_END`.
///
/// The zero-count first record selects decoded image strips. Subsequent ranges
/// cover every source once. The two `field_6` bytes retain unproven metadata.
static SpriteBatch _gShelterR47View5SpriteBatches[4] = {
    { .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 5, .field_6 = { 1, 0 } },
    { .firstSprite = 5, .spriteCount = ARRAY_SIZE(_gShelterR47View5SpriteSources) - 5, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Empty sprite-batch list for mapped room view 6.
///
/// The zero-count first record selects decoded image strips; the final record
/// ends the list. The source slot retains this list's address without sprites.
static SpriteBatch _gShelterR47View6SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Empty sprite-batch list for mapped room view 7.
///
/// The zero-count first record selects decoded image strips; the final record
/// ends the list. The source slot retains this list's address without sprites.
static SpriteBatch _gShelterR47View7SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// 10 texture rectangles and sorting depths for mapped room view 8.
///
/// Positions are display-centred pixels; depths are unscaled OT sorting values.
/// The view's batches select source ranges in elements, without a sentinel.
/// Sources and textures remain borrowed while the room overlay is loaded.
static SpriteSource _gShelterR47View8SpriteSources[10] = {
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 40, 40 } }, -152, -56, 1950, { .fields = { 88, 120 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 40, 40 } }, -112, -56, 1950, { .fields = { 88, 160 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 40, 40 } }, -72, -56, 1900, { .fields = { 88, 200 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 32 } }, -32, -48, 1800, { .fields = { 56, 0 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 32 } }, 0, -48, 1800, { .fields = { 56, 32 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 40, 40 } }, 32, -56, 1900, { .fields = { 88, 80 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 40, 40 } }, 72, -56, 1950, { .fields = { 88, 40 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 40, 40 } }, 112, -56, 1950, { .fields = { 88, 0 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 16 } }, -24, -16, 1800, { .fields = { 96, 240 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 16, 16 } }, 8, -16, 1800, { .fields = { 112, 240 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
};

/// Source ranges for mapped room view 8, ending with `SPRITE_BATCH_END`.
///
/// The zero-count first record keeps decoded image strips. The following range
/// covers every source. Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View8SpriteBatches[3] = {
    { .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gShelterR47View8SpriteSources) },
    { .firstSprite = SPRITE_BATCH_END },
};

/// 24 texture rectangles and sorting depths for mapped room view 9.
///
/// Positions are pixels relative to the draw origin; depths are unscaled OT values.
/// The view's batches select source ranges in elements, without a source sentinel.
/// Sources and textures are borrowed while the room and view resources are loaded.
static SpriteSource _gShelterR47View9SpriteSources[24] = {
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 16 } }, -64, 0, 812, { .fields = { 40, 200 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 24 } }, -112, -8, 800, { .fields = { 48, 128 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 24 } }, -160, -8, 800, { .fields = { 40, 0 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 24, 16 } }, -160, -24, 800, { .fields = { 104, 240 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 16 } }, 16, 0, 812, { .fields = { 56, 240 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 24 } }, 64, -8, 800, { .fields = { 40, 176 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 24 } }, 112, -8, 800, { .fields = { 48, 104 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 24, 16 } }, 136, -24, 800, { .fields = { 32, 232 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 24 } }, -72, 56, 634, { .fields = { 40, 152 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 24 } }, -120, 56, 656, { .fields = { 80, 216 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 16 } }, -152, 56, 827, { .fields = { 48, 216 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 40, 24 } }, -160, 32, 649, { .fields = { 88, 192 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 24 } }, 24, 56, 634, { .fields = { 48, 32 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 24 } }, 72, 56, 656, { .fields = { 48, 80 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 16 } }, 120, 56, 673, { .fields = { 24, 56 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 40, 24 } }, 120, 32, 650, { .fields = { 56, 56 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 32 } }, -64, 0, 570, { .fields = { 96, 64 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 32 } }, -64, 32, 630, { .fields = { 96, 32 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 40, 32 } }, -64, 64, 701, { .fields = { 88, 0 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 24, 8 } }, -48, 96, 801, { .fields = { 64, 24 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 24, 8 } }, 24, 96, 802, { .fields = { 56, 232 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 40, 32 } }, 24, 64, 701, { .fields = { 88, 160 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 32 } }, 32, 32, 631, { .fields = { 96, 128 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 32 } }, 32, 0, 570, { .fields = { 96, 96 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
};

/// Source ranges for mapped room view 9, ending with `SPRITE_BATCH_END`.
///
/// The zero-count first record keeps decoded image strips. Five ranges cover all
/// 24 source sprites; the nonzero `field_6` bytes have no established role.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View9SpriteBatches[7] = {
    { .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 4, .field_6 = { 0, 0 } },
    { .firstSprite = 4, .spriteCount = 4, .field_6 = { 3, 0 } },
    { .firstSprite = 8, .spriteCount = 4, .field_6 = { 2, 0 } },
    { .firstSprite = 12, .spriteCount = 4, .field_6 = { 4, 0 } },
    { .firstSprite = 16, .spriteCount = ARRAY_SIZE(_gShelterR47View9SpriteSources) - 16, .field_6 = { 1, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 10.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View10SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// 2 texture rectangles and sorting depths for mapped room view 11.
///
/// Positions are pixels relative to the draw origin; depths are unscaled OT values.
/// The view's batches select source ranges in elements, without a source sentinel.
/// Sources and textures are borrowed while the room and view resources are loaded.
static SpriteSource _gShelterR47View11SpriteSources[2] = {
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 16 } }, 88, 104, 520, { .fields = { 96, 16 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 16 } }, 120, 104, 511, { .fields = { 96, 0 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
};

/// Source ranges for mapped room view 11, ending with `SPRITE_BATCH_END`.
///
/// The zero-count first record keeps decoded image strips. The following range
/// covers every source. Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View11SpriteBatches[3] = {
    { .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gShelterR47View11SpriteSources) },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 12.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View12SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 13.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View13SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// 14 texture rectangles and sorting depths for mapped room view 14.
///
/// Positions are pixels relative to the draw origin; depths are unscaled OT values.
/// The view's batches select source ranges in elements, without a source sentinel.
/// Sources and textures are borrowed while the room and view resources are loaded.
static SpriteSource _gShelterR47View14SpriteSources[14] = {
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 48 } }, -160, 72, 212, { .fields = { 80, 0 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 48 } }, -112, 72, 212, { .fields = { 80, 48 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 48 } }, -64, 72, 212, { .fields = { 80, 96 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 48 } }, -16, 72, 212, { .fields = { 80, 144 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 48 } }, 32, 72, 212, { .fields = { 80, 192 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 48 } }, 80, 72, 212, { .fields = { 32, 0 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 48 } }, 128, 72, 212, { .fields = { 48, 48 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 40 } }, 80, 32, 212, { .fields = { 32, 144 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 24 } }, 32, 48, 212, { .fields = { 32, 216 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 48 } }, 128, 24, 212, { .fields = { 48, 96 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 24 } }, -16, 48, 212, { .fields = { 0, 48 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 24 } }, -64, 48, 212, { .fields = { 0, 96 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 32 } }, -160, 40, 212, { .fields = { 32, 184 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 24 } }, -112, 48, 212, { .fields = { 0, 72 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
};

/// Source ranges for mapped room view 14, ending with `SPRITE_BATCH_END`.
///
/// The zero-count first record keeps decoded image strips. The following range
/// covers every source. Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View14SpriteBatches[3] = {
    { .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gShelterR47View14SpriteSources) },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 15.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View15SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 16.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View16SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 17.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View17SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 18.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View18SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 19.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View19SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 20.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// The first record retains its authored hidden flag despite its zero count.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View20SpriteBatches[2] = {
    { .spriteCount = 0, .hidden = true },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 21.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View21SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 22.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View22SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 23.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View23SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 24.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View24SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 25.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View25SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 26.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View26SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 27.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View27SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 28.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View28SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 29.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View29SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 30.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View30SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 31.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View31SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 32.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View32SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 33.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View33SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 34.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View34SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 35.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View35SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 36.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View36SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 37.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View37SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 38.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View38SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 39.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View39SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 40.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View40SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 41.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View41SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 42.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View42SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 43.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View43SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 44.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View44SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Nine texture rectangles and sorting depths for mapped room view 45.
///
/// Positions are display-centred pixels; depths are unscaled OT sorting values.
/// The view's batch selects the complete source range in elements, without a
/// sentinel. Sources and textures remain borrowed while the room is loaded.
static SpriteSource _gShelterR47View45SpriteSources[9] = {
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 32 } }, -160, 0, 902, { .fields = { 96, 224 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 32, 48 } }, -160, 72, 915, { .fields = { 96, 176 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 56, 24 } }, -128, 96, 875, { .fields = { 40, 216 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 72, 24 } }, -72, 96, 875, { .fields = { 24, 192 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 40, 72 } }, 120, -96, 625, { .fields = { 88, 0 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 24, 48 } }, 136, -24, 625, { .fields = { 104, 128 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 48, 40 } }, 112, 24, 625, { .fields = { 56, 128 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 88, 56 } }, 72, 64, 625, { .fields = { 40, 72 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
    { SHELTER_R47_VIEW_SPRITE_PAGE_960, SHELTER_R47_VIEW_SPRITE_CLUT, { .fields = { 72, 24 } }, 0, 96, 875, { .fields = { 24, 168 } }, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_NEUTRAL_COLOR, SHELTER_R47_VIEW_SPRITE_OPAQUE },
};

/// Image background and one nine-source batch for mapped room view 45.
///
/// The first zero-count record keeps decoded image strips; the next selects
/// all nine source elements. The terminal record ends the list. Consumers
/// borrow the mutable records while the room overlay is loaded.
static SpriteBatch _gShelterR47View45SpriteBatches[3] = {
    { .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gShelterR47View45SpriteSources) },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Decoded-image background with no source sprites for mapped room view 46.
///
/// The zero-count first record selects image strips; `SPRITE_BATCH_END`
/// ends the list. The view retains this address in `sources.empty`.
/// Consumers borrow the list while the room overlay is loaded.
static SpriteBatch _gShelterR47View46SpriteBatches[2] = {
    { .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

SpriteView gShelterR47SpriteViews[46] = {
    { { .elements = _gShelterR47View1SpriteSources }, _gShelterR47View1SpriteBatches, NULL },
    { { .elements = _gShelterR47View2SpriteSources }, _gShelterR47View2SpriteBatches, NULL },
    { { .elements = _gShelterR47View3SpriteSources }, _gShelterR47View3SpriteBatches, _gShelterR47View3DrawAreas },
    { { .elements = _gShelterR47View4SpriteSources }, _gShelterR47View4SpriteBatches, NULL },
    { { .elements = _gShelterR47View5SpriteSources }, _gShelterR47View5SpriteBatches, NULL },
    { { .empty = _gShelterR47View6SpriteBatches }, _gShelterR47View6SpriteBatches, NULL },
    { { .empty = _gShelterR47View7SpriteBatches }, _gShelterR47View7SpriteBatches, NULL },
    { { .elements = _gShelterR47View8SpriteSources }, _gShelterR47View8SpriteBatches, NULL },
    { { .elements = _gShelterR47View9SpriteSources }, _gShelterR47View9SpriteBatches, NULL },
    { { .empty = _gShelterR47View10SpriteBatches }, _gShelterR47View10SpriteBatches, NULL },
    { { .elements = _gShelterR47View11SpriteSources }, _gShelterR47View11SpriteBatches, NULL },
    { { .empty = _gShelterR47View12SpriteBatches }, _gShelterR47View12SpriteBatches, NULL },
    { { .empty = _gShelterR47View13SpriteBatches }, _gShelterR47View13SpriteBatches, NULL },
    { { .elements = _gShelterR47View14SpriteSources }, _gShelterR47View14SpriteBatches, NULL },
    { { .empty = _gShelterR47View15SpriteBatches }, _gShelterR47View15SpriteBatches, NULL },
    { { .empty = _gShelterR47View16SpriteBatches }, _gShelterR47View16SpriteBatches, NULL },
    { { .empty = _gShelterR47View17SpriteBatches }, _gShelterR47View17SpriteBatches, NULL },
    { { .empty = _gShelterR47View18SpriteBatches }, _gShelterR47View18SpriteBatches, NULL },
    { { .empty = _gShelterR47View19SpriteBatches }, _gShelterR47View19SpriteBatches, NULL },
    { { .empty = _gShelterR47View20SpriteBatches }, _gShelterR47View20SpriteBatches, NULL },
    { { .empty = _gShelterR47View21SpriteBatches }, _gShelterR47View21SpriteBatches, NULL },
    { { .empty = _gShelterR47View22SpriteBatches }, _gShelterR47View22SpriteBatches, NULL },
    { { .empty = _gShelterR47View23SpriteBatches }, _gShelterR47View23SpriteBatches, NULL },
    { { .empty = _gShelterR47View24SpriteBatches }, _gShelterR47View24SpriteBatches, NULL },
    { { .empty = _gShelterR47View25SpriteBatches }, _gShelterR47View25SpriteBatches, NULL },
    { { .empty = _gShelterR47View26SpriteBatches }, _gShelterR47View26SpriteBatches, NULL },
    { { .empty = _gShelterR47View27SpriteBatches }, _gShelterR47View27SpriteBatches, NULL },
    { { .empty = _gShelterR47View28SpriteBatches }, _gShelterR47View28SpriteBatches, NULL },
    { { .empty = _gShelterR47View29SpriteBatches }, _gShelterR47View29SpriteBatches, NULL },
    { { .empty = _gShelterR47View30SpriteBatches }, _gShelterR47View30SpriteBatches, NULL },
    { { .empty = _gShelterR47View31SpriteBatches }, _gShelterR47View31SpriteBatches, NULL },
    { { .empty = _gShelterR47View32SpriteBatches }, _gShelterR47View32SpriteBatches, NULL },
    { { .empty = _gShelterR47View33SpriteBatches }, _gShelterR47View33SpriteBatches, NULL },
    { { .empty = _gShelterR47View34SpriteBatches }, _gShelterR47View34SpriteBatches, NULL },
    { { .empty = _gShelterR47View35SpriteBatches }, _gShelterR47View35SpriteBatches, NULL },
    { { .empty = _gShelterR47View36SpriteBatches }, _gShelterR47View36SpriteBatches, NULL },
    { { .empty = _gShelterR47View37SpriteBatches }, _gShelterR47View37SpriteBatches, NULL },
    { { .empty = _gShelterR47View38SpriteBatches }, _gShelterR47View38SpriteBatches, NULL },
    { { .empty = _gShelterR47View39SpriteBatches }, _gShelterR47View39SpriteBatches, NULL },
    { { .empty = _gShelterR47View40SpriteBatches }, _gShelterR47View40SpriteBatches, NULL },
    { { .empty = _gShelterR47View41SpriteBatches }, _gShelterR47View41SpriteBatches, NULL },
    { { .empty = _gShelterR47View42SpriteBatches }, _gShelterR47View42SpriteBatches, NULL },
    { { .empty = _gShelterR47View43SpriteBatches }, _gShelterR47View43SpriteBatches, NULL },
    { { .empty = _gShelterR47View44SpriteBatches }, _gShelterR47View44SpriteBatches, NULL },
    { { .elements = _gShelterR47View45SpriteSources }, _gShelterR47View45SpriteBatches, NULL },
    { { .empty = _gShelterR47View46SpriteBatches }, _gShelterR47View46SpriteBatches, NULL },
};

/// Nine room-owned point lights, composed and attenuated by gameplay in place.
///
/// Positions and falloff radii use game-coordinate units; RGB uses ONE for 1.0.
/// Each view filter admits every view. Storage stays live with the room overlay.
static WorldCoordPointLight _gShelterR47PointLights[9] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x310C, -2576, 7191 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1756, 3216, 2480 }, { 0, 0 } }, 100, 200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x309F, -2482, 7439 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1019, 3392, 1019 }, { 0, 0 } }, 100, 461 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x38B7, -3643, 7891 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2170, 2209, 2195 }, { 0, 0 } }, 1878, 3562 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4133, -2789, 3232 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2983, 2975, 2968 }, { 0, 0 } }, 1970, 6109 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8491, -3002, 3193 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2615, 2612, 2602 }, { 0, 0 } }, 3836, 6579 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x33FB, -2300, 0x2736 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2435, 3392, 2909 }, { 0, 0 } }, 300, 800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x4074, -2305, 6109 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2059, 4119, 2889 }, { 0, 0 } }, 401, 855 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x41F1, -1881, 7349 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1756, 3216, 2480 }, { 0, 0 } }, 100, 200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x35EA, -1128, 2778 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1944, 1895, 1866 }, { 0, 0 } }, 5201, 9559 },
};

/// Two live cone lights followed by 756 bytes whose purpose is unproven.
///
/// Only `coneLights` is exposed to room lighting. Gameplay mutates the live
/// transforms and attenuation; the following bytes have no established reader
/// or original object boundaries and remain in their byte representation.
static ShelterB1PodServiceGantrySpotLightStorage _gShelterR47SpotLightStorage = {
    .coneLights = {
        { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { -4003, -2, 873 }, { 869, 266, 4000 }, { -59, 4087, -261 } }, { 3412, -0x5063, 9454 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2764, 2764, 2425 }, { 0, 0 } }, { 872, 3993, -260, 0 }, 0x4E20, 0x7530, 0x2C71 },
        { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { -4092, 0, -222 }, { 215, -909, -3996 }, { -50, -3996, 908 } }, { 8054, 5038, 1813 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 226, 226, 198 }, { 0, 0 } }, { -221, -3988, 907, 0 }, 10, 0x4E20, 0x2C71 },
    },
    .unknown_D8 = {
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x09,
        0x00,
        0x00,
        0x00,
        0x58,
        0xD8,
        0x18,
        0x80,
        0x02,
        0x00,
        0x00,
        0x00,
        0xB8,
        0xDB,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x99,
        0xF4,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xD8,
        0xF8,
        0x00,
        0x00,
        0xB0,
        0xF1,
        0x00,
        0x00,
        0x28,
        0x07,
        0x00,
        0x00,
        0xB0,
        0xF1,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xF0,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xB5,
        0xF3,
        0x00,
        0x00,
        0x00,
        0x00,
        0x8E,
        0xFC,
        0x00,
        0x00,
        0x60,
        0xF0,
        0x00,
        0x00,
        0x69,
        0xF0,
        0x00,
        0x00,
        0x67,
        0xFC,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xA0,
        0x0F,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xA0,
        0x0F,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xC4,
        0x09,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xC4,
        0x09,
        0x00,
        0x00,
        0x70,
        0x17,
        0x00,
        0x00,
        0xC4,
        0x09,
        0x00,
        0x00,
        0x70,
        0x17,
        0xB8,
        0xF2,
        0xC4,
        0x09,
        0x00,
        0x00,
        0x70,
        0x17,
        0xB8,
        0xF2,
        0xDC,
        0x05,
        0x00,
        0x00,
        0x70,
        0x17,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xC4,
        0x09,
        0x00,
        0x00,
        0x40,
        0x1F,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x40,
        0x1F,
        0xB8,
        0xF2,
        0xDC,
        0x05,
        0x00,
        0x00,
        0x36,
        0x31,
        0x00,
        0x00,
        0x1C,
        0x25,
        0x00,
        0x00,
        0x36,
        0x31,
        0x00,
        0x00,
        0xA0,
        0x0F,
        0x00,
        0x00,
        0x36,
        0x31,
        0xB8,
        0xF2,
        0x00,
        0x00,
        0x00,
        0x00,
        0x36,
        0x31,
        0xB8,
        0xF2,
        0x1C,
        0x25,
        0x00,
        0x00,
        0x40,
        0x38,
        0x00,
        0x00,
        0x7C,
        0x15,
        0x00,
        0x00,
        0x40,
        0x38,
        0x00,
        0x00,
        0x7C,
        0x15,
        0x00,
        0x00,
        0x40,
        0x38,
        0xB8,
        0xF2,
        0xC4,
        0x09,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x3E,
        0xB8,
        0xF2,
        0xC7,
        0x24,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xA0,
        0x0F,
        0x00,
        0x00,
        0x40,
        0x1F,
        0x00,
        0x00,
        0x94,
        0x11,
        0x00,
        0x00,
        0x40,
        0x1F,
        0x00,
        0x00,
        0x94,
        0x11,
        0x00,
        0x00,
        0x28,
        0x23,
        0xB8,
        0xF2,
        0xA0,
        0x0F,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x94,
        0x11,
        0x00,
        0x00,
        0x70,
        0x17,
        0xB8,
        0xF2,
        0x94,
        0x11,
        0x00,
        0x00,
        0xA8,
        0x2F,
        0x00,
        0x00,
        0xF8,
        0x11,
        0x00,
        0x00,
        0x9C,
        0xFF,
        0x00,
        0x00,
        0x78,
        0x05,
        0x00,
        0x00,
        0x9C,
        0xFF,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x44,
        0x11,
        0x00,
        0x00,
        0xD4,
        0x30,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xD4,
        0x30,
        0x18,
        0xFC,
        0x04,
        0x15,
        0x00,
        0x00,
        0xA4,
        0x38,
        0x00,
        0x00,
        0x44,
        0x11,
        0x00,
        0x00,
        0xD4,
        0x30,
        0x00,
        0x00,
        0x44,
        0x11,
        0x00,
        0x00,
        0xA4,
        0x38,
        0x50,
        0xFB,
        0x00,
        0x00,
        0x00,
        0x00,
        0xD4,
        0x30,
        0x50,
        0xFB,
        0x04,
        0x15,
        0x00,
        0x00,
        0x74,
        0x40,
        0x00,
        0x00,
        0x7C,
        0x15,
        0x00,
        0x00,
        0x74,
        0x40,
        0x00,
        0x00,
        0x7C,
        0x15,
        0x00,
        0x00,
        0x8C,
        0x3C,
        0xB8,
        0xF2,
        0x1C,
        0x25,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xA4,
        0x38,
        0x00,
        0x00,
        0x78,
        0x05,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x09,
        0x00,
        0x0A,
        0x00,
        0x08,
        0x00,
        0x0B,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x0A,
        0x00,
        0x0C,
        0x00,
        0x0B,
        0x00,
        0x0D,
        0x00,
        0x03,
        0x00,
        0x00,
        0x00,
        0x0D,
        0x00,
        0x0C,
        0x00,
        0x0E,
        0x00,
        0x00,
        0x00,
        0x02,
        0x00,
        0x00,
        0x00,
        0x11,
        0x00,
        0x09,
        0x00,
        0x10,
        0x00,
        0x08,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x1A,
        0x00,
        0x1D,
        0x00,
        0x05,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x1E,
        0x00,
        0x21,
        0x00,
        0x06,
        0x00,
        0x00,
        0x00,
        0x23,
        0x00,
        0x24,
        0x00,
        0x22,
        0x00,
        0x00,
        0x00,
        0x07,
        0x00,
        0x00,
        0x00,
        0x11,
        0x00,
        0x10,
        0x00,
        0x26,
        0x00,
        0x27,
        0x00,
    },
};

WorldCoordRoomLights gShelterR47RoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gShelterR47PointLights),
    .pointLights           = _gShelterR47PointLights,
    .coneLightCount        = ARRAY_SIZE(_gShelterR47SpotLightStorage.coneLights),
    .coneLights            = _gShelterR47SpotLightStorage.coneLights,
};

/// Surface-class 2 cue bases; scripted jumps reuse the walking pair.
static WorldCollisionFootstepSounds _gShelterR47SurfaceClass2Footsteps = {
    .walk         = SHELTER_R47_SURFACE_CLASS_2_WALK_SOUND,
    .run          = SHELTER_R47_SURFACE_CLASS_2_RUN_SOUND,
    .scriptedJump = SHELTER_R47_SURFACE_CLASS_2_WALK_SOUND,
};

/// Surface-class 3 walk, run and separate scripted-jump cue bases.
static WorldCollisionFootstepSounds _gShelterR47SurfaceClass3Footsteps = {
    .walk         = SHELTER_R47_SURFACE_CLASS_3_WALK_SOUND,
    .run          = SHELTER_R47_SURFACE_CLASS_3_RUN_SOUND,
    .scriptedJump = SHELTER_R47_SURFACE_CLASS_3_JUMP_SOUND,
};

/// Surface-class 1 cue bases; scripted jumps reuse the walking pair.
static WorldCollisionFootstepSounds _gShelterR47SurfaceClass1Footsteps = {
    .walk         = SHELTER_R47_SURFACE_CLASS_1_WALK_SOUND,
    .run          = SHELTER_R47_SURFACE_CLASS_1_RUN_SOUND,
    .scriptedJump = SHELTER_R47_SURFACE_CLASS_1_WALK_SOUND,
};

/// Blocking collision properties without footstep cues, shared by classes 0 and 4..7.
static WorldCollisionSurfaceProperties _gShelterR47SilentSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Blocking collision properties and footstep cues for surface class 1.
static WorldCollisionSurfaceProperties _gShelterR47SurfaceClass1Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterR47SurfaceClass1Footsteps,
};

/// Blocking collision properties and footstep cues for surface class 2.
static WorldCollisionSurfaceProperties _gShelterR47SurfaceClass2Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterR47SurfaceClass2Footsteps,
};

/// Blocking collision properties and footstep cues for surface class 3.
static WorldCollisionSurfaceProperties _gShelterR47SurfaceClass3Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterR47SurfaceClass3Footsteps,
};

WorldCollisionSurfaceProperties* gShelterR47SurfaceProperties[8] = {
    &_gShelterR47SilentSurfaceProperties,
    &_gShelterR47SurfaceClass1Properties,
    &_gShelterR47SurfaceClass2Properties,
    &_gShelterR47SurfaceClass3Properties,
    &_gShelterR47SilentSurfaceProperties,
    &_gShelterR47SilentSurfaceProperties,
    &_gShelterR47SilentSurfaceProperties,
    &_gShelterR47SilentSurfaceProperties,
};

AreaApplyRec gShelterR47SavedAreaUpdates[21] = {
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ELEVATOR_HALL, 3, AREA_APPLY_MODE_ALWAYS | SHELTER_R47_SAVED_AREA_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY, 3, AREA_APPLY_MODE_ALWAYS | SHELTER_R47_SAVED_AREA_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STOREROOM, 3, AREA_APPLY_MODE_ALWAYS | SHELTER_R47_SAVED_AREA_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_SLEEPING_QUARTERS, 3, AREA_APPLY_MODE_ALWAYS | SHELTER_R47_SAVED_AREA_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, 3, AREA_APPLY_MODE_REPLAY_SCAVENGER | SHELTER_R47_SAVED_AREA_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, 7, AREA_APPLY_MODE_BOUNTY_NIGHTMARE | SHELTER_R47_SAVED_AREA_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL, 4, AREA_APPLY_MODE_REPLAY_SCAVENGER | SHELTER_R47_SAVED_AREA_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL, 7, AREA_APPLY_MODE_BOUNTY_NIGHTMARE | SHELTER_R47_SAVED_AREA_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ACCESS_TUNNEL, 11, AREA_APPLY_MODE_BOUNTY_NIGHTMARE | SHELTER_R47_SAVED_AREA_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY, 2, AREA_APPLY_MODE_ALWAYS | SHELTER_R47_SAVED_AREA_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_OPERATING_ROOM, 3, AREA_APPLY_MODE_ALWAYS | SHELTER_R47_SAVED_AREA_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY, 4, AREA_APPLY_MODE_ALWAYS | SHELTER_R47_SAVED_AREA_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_BREEDING_ROOM, 4, AREA_APPLY_MODE_ALWAYS | SHELTER_R47_SAVED_AREA_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_MAIN_CORRIDOR, 4, AREA_APPLY_MODE_ALWAYS | SHELTER_R47_SAVED_AREA_MARK_CLEAR },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SEPTIC_TANK, 4, AREA_APPLY_MODE_ALWAYS | SHELTER_R47_SAVED_AREA_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL, 4, AREA_APPLY_MODE_REPLAY_SCAVENGER | SHELTER_R47_SAVED_AREA_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL, 7, AREA_APPLY_MODE_BOUNTY_NIGHTMARE | SHELTER_R47_SAVED_AREA_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B3_ELEVATOR_HALL, 2, AREA_APPLY_MODE_ALWAYS | SHELTER_R47_SAVED_AREA_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_LOWER_SEWER, 3, AREA_APPLY_MODE_ALWAYS | SHELTER_R47_SAVED_AREA_MARK_SET },
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_UPPER_SEWER, 3, AREA_APPLY_MODE_ALWAYS | SHELTER_R47_SAVED_AREA_MARK_SET },
    { .stage = AREA_APPLY_END },
};

Task* gRoomCutsceneSoundTask = NULL;

Task* gShelterR47TerminalTask;

u8 gShelterR47ConsoleButtonFirstUseStarted;

u8 gShelterR47ConsoleRowFirstUseStarted;

u8 gShelterR47MapPreviousFirstUseStarted;

u8 gShelterR47MapNextFirstUseStarted;

RoomCutsceneRec gShelterR47RepeatUseCutscene;

static void _glowDrawDiamond(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);
static void _glowDrawPulsingDisc(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);

/// Opens a map-terminal session and allocates its work and cursor task.
///
/// State 0 takes a `SHELTER_R47_MAP_MODE_*` opening mode in spawn argument 1.
/// The task owns the zeroed primary-heap work; spawn argument 2 becomes its
/// port-0 cursor task, which completion must kill separately. Work allocation
/// failure kills the terminal before acquiring any display or room holds.
/// Requires the room's mutable, sentinel-terminated hotspot tables. Player use
/// grows the quads from zero; timed viewing and the tour begin fully open behind
/// a black overlay. Saves the current view and selects the B1 map view.
static void _shelterR47MapTerminalInitTask(Task* task)
{
    enum { SHELTER_R47_MAP_CURSOR_PORT_0 = 1 };
    ShelterR47MapTerminalWork* work;
    s16                        labelX;
    ActionPromptHotspot*       hotspot;
    s32                        openMode;
    u8                         savedView;
    s16                        mapWidth, mapHeight, panelWidth, panelHeight;

    work = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2.pointer = taskSpawnFromTable(&_gShelterR47MapTerminalPromptTaskDesc, 0, SHELTER_R47_MAP_CURSOR_PORT_0, 0);
    task->work              = work;
    task->state            += 1;
    displayAcquireMenuHold();
    // The tour exposes only navigation; discard hits from an earlier session.
    work->hotspots = task->spawnArg1.value == SHELTER_R47_MAP_MODE_TOUR ? gShelterR47MapTerminalTourHotspots : gShelterR47MapTerminalHotspots;
    // Retained scheduling boundary before walking the stored table pointer.
    do {
    } while (0);
    for (hotspot = work->hotspots; hotspot->id != ACTION_PROMPT_HOTSPOT_END; hotspot++) {
        hotspot->hit = 0;
    }
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    gGameSession->eventState   = 1;
    mapWidth                   = SHELTER_R47_MAP_WIDTH;
    mapHeight                  = SHELTER_R47_MAP_HEIGHT;
    panelWidth                 = SHELTER_R47_MAP_PANEL_WIDTH;
    panelHeight                = SHELTER_R47_MAP_PANEL_HEIGHT;
    labelX                     = SHELTER_R47_MAP_LABEL_X;
    // Retained scheduling boundary keeps the shared opening sizes grouped.
    do {
    } while (0);
    savedView                                              = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view;
    work->labelX                                           = SHELTER_R47_MAP_LABEL_X_AWAY;
    work->mapTargetWidth                                   = mapWidth;
    work->mapTargetHeight                                  = mapHeight;
    work->panelTargetWidth                                 = panelWidth;
    work->panelTargetHeight                                = panelHeight;
    work->labelTargetX                                     = labelX;
    work->savedView                                        = savedView;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = SHELTER_R47_MAP_ENTRY_VIEW;
    if ((openMode = task->spawnArg1.value) == SHELTER_R47_MAP_MODE_TIMED || openMode == SHELTER_R47_MAP_MODE_TOUR) {
        work->fade        = SHELTER_R47_MAP_FADE_BLACK;
        work->mapWidth    = mapWidth;
        work->mapHeight   = mapHeight;
        work->panelWidth  = panelWidth;
        work->panelHeight = panelHeight;
        work->labelX      = labelX;
        fadeDrawOverlay(work->fade, work->fade, work->fade, GPU_BLEND_SUBTRACT);
        work->openMode = openMode;
    }
}

/// Latches a confirmed hotspot and stops its cursor while the command menu opens.
///
/// Borrows the live task, its work, its port-0 prompt and the first confirmed
/// hit-tested entry before the -1 terminator. Copies its ID and command kind;
/// no pointer to the hotspot is retained. Enters command-menu state 3.
static inline void _shelterR47MapTerminalAcceptHotspot(Task* task, ShelterR47MapTerminalWork* work,
                                                       ActionPrompt* prompt, const ActionPromptHotspot* hotspot)
{
    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    work->hotspotId     = hotspot->id;
    work->promptKind    = hotspot->promptKind;
    task->state         = SHELTER_R47_MAP_STATE_OPEN_COMMANDS;
}

/// Selects the first confirmed hotspot while the map terminal is idle.
///
/// State 2 requires live terminal work, a page in 0..4 and the port-0 prompt.
/// Refreshes the display and screen-on countdown before gating input on CAP
/// playback and settled geometry. Confirmation saves the hotspot and command
/// kind for state 3. Player cancel restores the room; tour cancel plays a
/// refusal, and a completed tour starts fade-out after CAP playback ends.
static void _shelterR47MapTerminalSelectHotspotTask(Task* task)
{
    enum {
        SHELTER_R47_MAP_CONFIRM_BUTTON          = 0,
        SHELTER_R47_MAP_CANCEL_BUTTON           = 1,
        SHELTER_R47_MAP_CANCEL_REFUSED_CAP_SLOT = 0x25,
    };
    ActionPrompt*              prompt  = gActionPromptSlots;
    ShelterR47MapTerminalWork* work    = task->work;
    ActionPromptHotspot*       hotspot = work->hotspots;

    if (work->screenOnDelay != 0) {
        if (--work->screenOnDelay == 0) {
            sndEvtRequestScriptStart(SOUND_SHELTER_R47_MAP_TERMINAL_SCREEN_ON, 0, 0);
        }
    }
    _shelterR47MapTerminalUpdateAndDraw(task);
    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if ((capIsBusy() != 0) || (work->holdPrompt != 0)) {
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
        return;
    }
    if (work->openMode == SHELTER_R47_MAP_MODE_TOUR_DONE) {
        task->state = SHELTER_R47_MAP_STATE_BEGIN_FADE_OUT;
        return;
    }
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    if (_actionPromptHitTest(hotspot, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HIGHLIGHTED;
        if (prompt->buttons.slots[SHELTER_R47_MAP_CONFIRM_BUTTON].state == ACTION_PROMPT_BUTTON_PRESSED) {
            for (; hotspot->id != ACTION_PROMPT_HOTSPOT_END; hotspot++) {
                if (hotspot->hit != 0) {
                    _shelterR47MapTerminalAcceptHotspot(task, work, prompt, hotspot);
                    return;
                }
            }
        }
    } else {
        prompt->mode = ACTION_PROMPT_MODE_NORMAL;
    }
    if (prompt->buttons.slots[SHELTER_R47_MAP_CANCEL_BUTTON].state == ACTION_PROMPT_BUTTON_PRESSED) {
        if (work->openMode == SHELTER_R47_MAP_MODE_USE) {
            task->state = SHELTER_R47_MAP_STATE_RESTORE_ROOM;
            return;
        }
        capStartSequenceSlot(SHELTER_R47_MAP_CANCEL_REFUSED_CAP_SLOT, 0, 0);
    }
}

/// Starts closing the current terminal page before selecting an adjacent page.
///
/// `state` must be this task's live map-terminal work, with `hotspotId`
/// identifying the previous or next page. Sets closing targets for both quads
/// and an off-screen target for the label, then enters the state that waits
/// for the map to close before changing `page`. Queues the page-switch sound
/// and stops the terminal loop while retaining the voices' ADSR release
/// settings; the terminal task and work stay live.
static inline void _shelterR47MapTerminalLeavePage(Task* task, ShelterR47MapTerminalWork* state)
{
    enum { SHELTER_R47_MAP_STATE_SWITCH_PAGE = 6 };

    state->labelTargetX      = SHELTER_R47_MAP_LABEL_X_AWAY;
    state->mapTargetWidth    = 0;
    state->mapTargetHeight   = 0;
    state->panelTargetWidth  = 0;
    state->panelTargetHeight = 0;
    task->state              = SHELTER_R47_MAP_STATE_SWITCH_PAGE;
    sndEvtRequestScriptStart(SOUND_SHELTER_R47_MAP_TERMINAL_PAGE_SWITCH, 0, 0);
    sndEvtRequestScriptStop(SOUND_SHELTER_R47_MAP_TERMINAL_LOOP, SOUND_SCRIPT_STOP_KEEP_RELEASE);
}

/// Resolves the terminal's accepted Examine/Push command or cancellation.
///
/// State 4 requires the completed hotspot menu's result and live terminal work.
/// Panel/title actions select a CAP sequence for page 0..4. Navigation first
/// plays its one-off player-use sequence, then closes the page on a later
/// acceptance; the tour refuses previous-page navigation. Cancellation and
/// ordinary actions return to selection. The cursor stays hidden and stopped.
static void _shelterR47MapTerminalHandleHotspotTask(Task* task)
{
    enum {
        SHELTER_R47_MAP_PANEL_B1_CAP_SLOT           = 0x1D,
        SHELTER_R47_MAP_PANEL_B2_CAP_SLOT           = 0x1E,
        SHELTER_R47_MAP_PANEL_B3_CAP_SLOT           = 0x1F,
        SHELTER_R47_MAP_PANEL_NEO_ARK_CAP_SLOT      = 0x20,
        SHELTER_R47_MAP_PANEL_1F_CAP_SLOT           = 0x21,
        SHELTER_R47_MAP_PREVIOUS_REFUSED_CAP_SLOT   = 0x24,
        SHELTER_R47_MAP_TITLE_B1_CAP_SLOT           = 0x2F,
        SHELTER_R47_MAP_TITLE_B2_CAP_SLOT           = 0x30,
        SHELTER_R47_MAP_TITLE_B3_CAP_SLOT           = 0x31,
        SHELTER_R47_MAP_TITLE_NEO_ARK_CAP_SLOT      = 0x32,
        SHELTER_R47_MAP_TITLE_1F_CAP_SLOT           = 0x33,
        SHELTER_R47_MAP_NEXT_FIRST_USE_CAP_SLOT     = 0x34,
        SHELTER_R47_MAP_PREVIOUS_FIRST_USE_CAP_SLOT = 0x35,
    };
    ActionPrompt*              prompt = gActionPromptSlots;
    ShelterR47MapTerminalWork* work   = task->work;

    _shelterR47MapTerminalUpdateAndDraw(task);
    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    if (itemMenuIsHotspotActionConfirmed() != 0) {
        switch (work->hotspotId) {
            case SHELTER_R47_MAP_HOTSPOT_PANEL:
                switch (work->page) {
                    case SHELTER_R47_MAP_PAGE_B1:
                        capStartSequenceSlot(SHELTER_R47_MAP_PANEL_B1_CAP_SLOT, 0, 0);
                        break;
                    case SHELTER_R47_MAP_PAGE_B2:
                        capStartSequenceSlot(SHELTER_R47_MAP_PANEL_B2_CAP_SLOT, 0, 0);
                        break;
                    case SHELTER_R47_MAP_PAGE_B3:
                        capStartSequenceSlot(SHELTER_R47_MAP_PANEL_B3_CAP_SLOT, 0, 0);
                        break;
                    case SHELTER_R47_MAP_PAGE_NEO_ARK:
                        capStartSequenceSlot(SHELTER_R47_MAP_PANEL_NEO_ARK_CAP_SLOT, 0, 0);
                        break;
                    case SHELTER_R47_MAP_PAGE_1F:
                        capStartSequenceSlot(SHELTER_R47_MAP_PANEL_1F_CAP_SLOT, 0, 0);
                        break;
                }
                break;
            case SHELTER_R47_MAP_HOTSPOT_PREV:
                if (work->openMode != SHELTER_R47_MAP_MODE_USE) {
                    capStartSequenceSlot(SHELTER_R47_MAP_PREVIOUS_REFUSED_CAP_SLOT, 0, 0);
                    task->state = SHELTER_R47_MAP_STATE_SELECT_HOTSPOT;
                    return;
                }
                if (gShelterR47MapPreviousFirstUseStarted == false) {
                    capStartSequenceSlot(SHELTER_R47_MAP_PREVIOUS_FIRST_USE_CAP_SLOT, 0, 0);
                    gShelterR47MapPreviousFirstUseStarted = true;
                    task->state                           = SHELTER_R47_MAP_STATE_SELECT_HOTSPOT;
                    return;
                }
                _shelterR47MapTerminalLeavePage(task, work);
                return;
            case SHELTER_R47_MAP_HOTSPOT_NEXT:
                if (work->openMode == SHELTER_R47_MAP_MODE_USE && gShelterR47MapNextFirstUseStarted == false) {
                    capStartSequenceSlot(SHELTER_R47_MAP_NEXT_FIRST_USE_CAP_SLOT, 0, 0);
                    gShelterR47MapNextFirstUseStarted = true;
                    task->state                       = SHELTER_R47_MAP_STATE_SELECT_HOTSPOT;
                    return;
                }
                _shelterR47MapTerminalLeavePage(task, work);
                return;
            case SHELTER_R47_MAP_HOTSPOT_TITLE:
                switch (work->page) {
                    case SHELTER_R47_MAP_PAGE_B1:
                        capStartSequenceSlot(SHELTER_R47_MAP_TITLE_B1_CAP_SLOT, 0, 0);
                        break;
                    case SHELTER_R47_MAP_PAGE_B2:
                        capStartSequenceSlot(SHELTER_R47_MAP_TITLE_B2_CAP_SLOT, 0, 0);
                        break;
                    case SHELTER_R47_MAP_PAGE_B3:
                        capStartSequenceSlot(SHELTER_R47_MAP_TITLE_B3_CAP_SLOT, 0, 0);
                        break;
                    case SHELTER_R47_MAP_PAGE_NEO_ARK:
                        capStartSequenceSlot(SHELTER_R47_MAP_TITLE_NEO_ARK_CAP_SLOT, 0, 0);
                        break;
                    case SHELTER_R47_MAP_PAGE_1F:
                        capStartSequenceSlot(SHELTER_R47_MAP_TITLE_1F_CAP_SLOT, 0, 0);
                        break;
                }
                break;
            default:
                return;
        }
    }
    task->state = SHELTER_R47_MAP_STATE_SELECT_HOTSPOT;
}

/// Selects the additional private outline drawer; its rectangle is read-only.
#undef ACTION_PROMPT_OUTLINE_RECT
#define ACTION_PROMPT_OUTLINE_RECT _actionPromptOutlineRect
#include "../../shared/room/action_prompt_outline_rect.inc.c"
#undef ACTION_PROMPT_OUTLINE_RECT
#define ACTION_PROMPT_OUTLINE_RECT _actionPromptOutlineRectDefault

/// Selects the additional private cursor task, with signature `void(Task*)`.
#undef ACTION_PROMPT_MOVE_CURSORS_TASK
#define ACTION_PROMPT_MOVE_CURSORS_TASK _actionPromptMoveCursors
#undef ACTION_PROMPT_DRAW_CURSOR
/// Selects the second prompt's private drawer for both fragments.
#define ACTION_PROMPT_DRAW_CURSOR _actionPromptDrawCursor
#include "../../shared/room/action_prompt_move_cursors.inc.c"
#undef ACTION_PROMPT_MOVE_CURSORS_TASK
#define ACTION_PROMPT_MOVE_CURSORS_TASK _actionPromptMoveCursorsDefault

#include "../../shared/room/action_prompt_draw_cursor.inc.c"
#undef ACTION_PROMPT_DRAW_CURSOR
#define ACTION_PROMPT_DRAW_CURSOR _actionPromptDrawCursorDefault

/// Draws the final clear terminal frame and starts the closing fade.
///
/// State 9 requires live terminal work and a valid page; state 10 advances
/// the fade on subsequent ticks. This frame still updates geometry and labels.
static void _shelterR47MapTerminalBeginFadeOutTask(Task* task)
{
    ShelterR47MapTerminalWork* work;

    work = task->work;
    shelterR47MapTerminalUpdateAndDrawQuads(task);
    shelterR47MapTerminalDrawPreviousButton();
    shelterR47MapTerminalDrawNextButton();
    shelterR47MapTerminalDrawPageTitle(task, work->page);
    shelterR47MapTerminalDrawPageCaptions(task, work->page);
    work->fade = 0;
    task->state++;
}

/// Releases the closing terminal's holds and cursor, then hands off result zero.
///
/// Requires live work and a cursor task in spawnArg2.pointer. The original
/// spawn mode gates player-control release; the current work mode gates event
/// release. Timed viewing leaves those holds to its event script. Releases
/// model/display/cutscene holds and kills the cursor before requesting result 0.
/// The terminal and work remain live until an exit handler is dispatched.
static inline void _shelterR47MapTerminalCompleteFadeOut(Task* task, const ShelterR47MapTerminalWork* work)
{
    if (task->spawnArg1.value != SHELTER_R47_MAP_MODE_TIMED) {
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
    }
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
    displayReleaseMenuHold();
    if (work->openMode != SHELTER_R47_MAP_MODE_TIMED) {
        gGameSession->eventState = 0;
    }
    gGameSession->cutsceneHold = 0;
    taskKill(task->spawnArg2.pointer);
    taskRequestKill(task, 0);
}

/// Fades the terminal to black and requests completion when fully covered.
///
/// State 10 borrows live terminal work and the cursor task in spawn argument 2.
/// Adds 16 intensity levels per tick, clamps at 255, releases the display hold,
/// kills the cursor and requests result zero. Timed mode leaves control and
/// event-state restoration to the script; other modes release those holds.
/// An exit dispatch later tears down the stopped task and frees its work.
static void _shelterR47MapTerminalFadeOutTask(Task* task)
{
    ShelterR47MapTerminalWork* work;
    u16                        nextFade;
    u8                         fadeLevel;

    work = task->work;
    shelterR47MapTerminalUpdateAndDrawQuads(task);
    shelterR47MapTerminalDrawPreviousButton();
    shelterR47MapTerminalDrawNextButton();
    shelterR47MapTerminalDrawPageTitle(task, work->page);
    shelterR47MapTerminalDrawPageCaptions(task, work->page);
    nextFade   = work->fade + SHELTER_R47_MAP_FADE_OUT_STEP;
    work->fade = nextFade;
    if ((s16)nextFade >= SHELTER_R47_MAP_FADE_RANGE) {
        work->fade = SHELTER_R47_MAP_FADE_BLACK;
        // Requesting completion retains work for the final black overlay below.
        _shelterR47MapTerminalCompleteFadeOut(task, work);
    }
    sndEvtRequestScriptStop(SOUND_SHELTER_R47_MAP_TERMINAL_LOOP, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    fadeLevel = (u8)work->fade;
    fadeDrawOverlay(fadeLevel, fadeLevel, fadeLevel, GPU_BLEND_SUBTRACT);
}

/// Updates the terminal's opening geometry and queues its complete page display.
///
/// Borrows live terminal work with a valid page 0..4. Quads update the opening
/// and marker state before the two buttons; the title then eases label X for
/// both the title and captions. Requires their textures, OT and packet arena
/// to remain live through GPU completion. Does not process input or teardown.
static void _shelterR47MapTerminalUpdateAndDraw(Task* task)
{
    ShelterR47MapTerminalWork* work;

    work = task->work;
    shelterR47MapTerminalUpdateAndDrawQuads(task);
    shelterR47MapTerminalDrawPreviousButton();
    shelterR47MapTerminalDrawNextButton();
    shelterR47MapTerminalDrawPageTitle(task, work->page);
    shelterR47MapTerminalDrawPageCaptions(task, work->page);
}

/// Runs one state of the room's interactive, timed or tour map terminal.
///
/// The bodyless task begins at state 0 with a `SHELTER_R47_MAP_MODE_*` spawn
/// argument. State must stay in 0..10: setup/open, hotspot selection/menu/result,
/// room restoration, page switch/reopen, timed viewing, then fade setup/closing.
/// Each tick copies all eleven callback pointers before dispatch; callbacks
/// may stop or release the task. Keep the room overlay loaded through teardown.
static void _shelterR47MapTerminalTask(Task* task)
{
    TaskFuncTable11 stateHandlers;

    stateHandlers = _gShelterR47MapTerminalStateHandlers;
    stateHandlers.funcs[task->state](task);
}

/// Selects the private hotspot tester for the map-terminal prompt.
///
/// The static instance uses the signature documented in `action_prompt.h`.
#undef ACTION_PROMPT_HIT_TEST
#define ACTION_PROMPT_HIT_TEST _actionPromptHitTest
#include "../../shared/room/action_prompt_hit_test.inc.c"
#undef ACTION_PROMPT_HIT_TEST
#define ACTION_PROMPT_HIT_TEST actionPromptHitTest

/// Starts player input, timed viewing or the tour's opening CAP sequence.
///
/// State 1 requires live work and the port-0 prompt. Timed mode enters state 8
/// behind the initial black overlay. The tour hides the cursor while subtracting
/// eight intensity levels per tick, starting its CAP sequence only on signed
/// underflow; player use sets the screen-on delay and enters selection.
/// Zeroes the sampled screen position on every invocation, including tour fade
/// ticks; the cursor task retains its separate fixed-point motion position.
static void _shelterR47MapTerminalOpenTask(Task* task)
{
    enum { SHELTER_R47_MAP_TOUR_START_CAP_SLOT = 0x12 };
    ShelterR47MapTerminalWork* work;
    ActionPrompt*              prompt = gActionPromptSlots;
    u8                         fadeLevel;

    work = task->work;
    if (work->openMode == SHELTER_R47_MAP_MODE_TIMED) {
        fadeLevel = work->fade;
        fadeDrawOverlay(fadeLevel, fadeLevel, fadeLevel, GPU_BLEND_SUBTRACT);
        task->state         = SHELTER_R47_MAP_STATE_TIMED_VIEW;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    } else if (work->openMode == SHELTER_R47_MAP_MODE_TOUR) {
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
        if ((s16)work->fade > 0) {
            work->fade -= SHELTER_R47_MAP_FADE_IN_STEP;
            if ((s16)work->fade < 0) {
                work->fade = 0;
                capStartSequenceSlot(SHELTER_R47_MAP_TOUR_START_CAP_SLOT, 0, 0);
                task->state++;
            } else {
                fadeLevel = work->fade;
                fadeDrawOverlay(fadeLevel, fadeLevel, fadeLevel, GPU_BLEND_SUBTRACT);
            }
        }
    } else {
        work->screenOnDelay = SHELTER_R47_MAP_SCREEN_ON_DELAY;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
        prompt->mode        = ACTION_PROMPT_MODE_NORMAL;
        task->state++;
    }
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
}

/// Opens the command menu at the selected terminal hotspot's cursor position.
///
/// State 3 requires live work with the latched hotspot and command kind.
/// First-use previous/next buttons offer Examine before their normal Push
/// action. Coordinates are signed pixels from the display centre. Stops and
/// hides the cursor, queues the menu, then enters state 4 to resolve its result.
static void _shelterR47MapTerminalOpenCommandsTask(Task* task)
{
    enum { SHELTER_R47_MAP_COMMAND_EXAMINE = 0 };
    ShelterR47MapTerminalWork* work;
    ActionPrompt*              prompt = gActionPromptSlots;

    work = task->work;
    _shelterR47MapTerminalUpdateAndDraw(task);
    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    if (work->openMode == SHELTER_R47_MAP_MODE_USE) {
        if (work->hotspotId == SHELTER_R47_MAP_HOTSPOT_PREV && gShelterR47MapPreviousFirstUseStarted == false) {
            work->promptKind = SHELTER_R47_MAP_COMMAND_EXAMINE;
        }
        if (work->hotspotId == SHELTER_R47_MAP_HOTSPOT_NEXT && gShelterR47MapNextFirstUseStarted == false) {
            work->promptKind = SHELTER_R47_MAP_COMMAND_EXAMINE;
        }
    }
    itemMenuOpenHotspotCommands(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = SHELTER_R47_MAP_STATE_HANDLE_HOTSPOT;
}

/// Restores play after map-terminal use and hands the stopped task to its caller.
///
/// Requires live map-terminal work and its live cursor task in spawnArg2.
/// Restores the saved view, actor control/drawing and room holds, releases the
/// display hold and stops the loop sound. Kills the cursor before requesting
/// completion with result zero; the polling caller releases this task's work.
static void _shelterR47MapTerminalRestoreRoomTask(Task* task)
{
    enum { SHELTER_R47_MAP_ACTION_DELAY_FRAMES = 10 };
    ShelterR47MapTerminalWork* work;

    work = task->work;
    // Delay a fresh directional interaction while room control is restored.
    gDirectionInteractionRearmUpdates = SHELTER_R47_MAP_ACTION_DELAY_FRAMES;
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
    sndEvtRequestScriptStop(SOUND_SHELTER_R47_MAP_TERMINAL_LOOP, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    displayReleaseMenuHold();
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = work->savedView;
    gGameSession->eventState                               = 0;
    gGameSession->hideHud                                  = 0;
    gGameSession->cutsceneHold                             = 0;
    taskKill(task->spawnArg2.pointer);
    taskRequestKill(task, 0);
}

/// Selects the adjacent floor view after the current map has closed.
///
/// State 6 requires live work with page 0..4 and a previous/next hotspot.
/// Draws the closing geometry until width reaches zero, wraps the page within
/// the five-page range, selects its room view and starts reopening the map and
/// labels. The side panel stays closed until state 7 opens it.
static void _shelterR47MapTerminalSwitchPageTask(Task* task)
{
    ShelterR47MapTerminalWork* work;

    work = task->work;
    _shelterR47MapTerminalUpdateAndDraw(task);
    if (work->mapWidth <= 0) {
        work->mapTargetWidth  = SHELTER_R47_MAP_WIDTH;
        work->mapTargetHeight = SHELTER_R47_MAP_HEIGHT;
        work->labelTargetX    = SHELTER_R47_MAP_LABEL_X;
        switch (work->hotspotId) {
            case SHELTER_R47_MAP_HOTSPOT_PREV:
                work->page--;
                if (work->page < 0) {
                    work->page = SHELTER_R47_MAP_PAGE_1F;
                }
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = gShelterR47MapTerminalPageViews[work->page];
                break;
            case SHELTER_R47_MAP_HOTSPOT_NEXT:
                work->page++;
                if (work->page >= SHELTER_R47_MAP_PAGE_COUNT) {
                    work->page = SHELTER_R47_MAP_PAGE_B1;
                }
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = gShelterR47MapTerminalPageViews[work->page];
                break;
        }
        task->state++;
    }
}

/// Reopens the side panel once the new map is wide enough, then resumes input.
///
/// State 7 requires live work for the newly selected page. Draws every tick;
/// reaching the panel threshold plays the screen-on sound and enters state 2.
/// Selection continues to hide the cursor until the map has fully settled.
static void _shelterR47MapTerminalReopenPageTask(Task* task)
{
    ShelterR47MapTerminalWork* work;

    work = task->work;
    _shelterR47MapTerminalUpdateAndDraw(task);
    if (work->mapWidth >= SHELTER_R47_MAP_PANEL_OPEN_WIDTH_THRESHOLD) {
        sndEvtRequestScriptStart(SOUND_SHELTER_R47_MAP_TERMINAL_SCREEN_ON, 0, 0);
        work->panelTargetWidth  = SHELTER_R47_MAP_PANEL_WIDTH;
        work->panelTargetHeight = SHELTER_R47_MAP_PANEL_HEIGHT;
        task->state             = SHELTER_R47_MAP_STATE_SELECT_HOTSPOT;
    }
}

/// Shows the timed map while fading in, then closes after the limit or a skip.
///
/// State 8 requires live work and a valid page. The signed halfword frame count
/// includes the eight-level-per-tick opening fade; closure begins after 300
/// ticks (on tick 301), or when the event is skipped. The transition frame still
/// draws the terminal and applies its HUD/event holds. CAP playback hides the
/// cursor; this state never enables interactive hotspot commands.
static void _shelterR47MapTerminalTimedViewTask(Task* task)
{
    ShelterR47MapTerminalWork* work;
    ActionPrompt*              prompt = gActionPromptSlots;
    u8                         fadeLevel;

    work = task->work;
    if ((s16)work->fade > 0) {
        work->fade -= SHELTER_R47_MAP_FADE_IN_STEP;
        if ((s16)work->fade < 0) {
            work->fade = 0;
        } else {
            fadeLevel = work->fade;
            fadeDrawOverlay(fadeLevel, fadeLevel, fadeLevel, GPU_BLEND_SUBTRACT);
        }
    }
    work->showFrames++;
    if (work->showFrames > SHELTER_R47_MAP_SHOW_LIMIT || gGameSession->evtSkipped != 0) {
        task->state = SHELTER_R47_MAP_STATE_BEGIN_FADE_OUT;
    }
    _shelterR47MapTerminalUpdateAndDraw(task);
    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (capIsBusy() != 0) {
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    }
}

/// Resets and runs the map terminal's action cursors.
///
/// State must be 0 (reset both ports and advance) or 1 (move and draw cursors).
/// Spawn argument 1 selects port 0 with 1, port 1 with 2 and both otherwise;
/// the terminal spawns port 0. Borrows gameplay's prompts, pad samples, cursor
/// textures and frame arena/OT. Needs no work; the terminal owns its teardown.
static void _shelterR47MapTerminalPromptTask(Task* task)
{
    TaskFunc states[] = {
        _actionPromptReset,
        _actionPromptMoveCursors,
    };

    states[task->state](task);
}

// Bind the additional private reset callback, with signature void(Task*).
#undef ACTION_PROMPT_RESET_TASK
#define ACTION_PROMPT_RESET_TASK _actionPromptReset
#include "../../shared/room/action_prompt_reset.inc.c"
#undef ACTION_PROMPT_RESET_TASK
#define ACTION_PROMPT_RESET_TASK _actionPromptResetDefault

/// Queues the additive, flickering cool-grey disc pair used by this room's view glows.
///
/// Borrows two adjacent world-space centres for the call: element 0 uses the
/// large radius scale, then element 1 uses the medium scale. Requires composed
/// view matrices, an initialized scratch stack and a current ordering table and
/// packet arena with space for eight Gouraud quads plus blend commands. A centre
/// whose projection passes the GTE flag check must have nonzero camera Z / 4,
/// as required by `glowDrawDisc`.
static inline void _shelterR47DrawViewGlowPair(const SVECTOR discCenters[2])
{
    enum {
        SHELTER_R47_GLOW_PAIR_LARGE_CENTER  = 0,
        SHELTER_R47_GLOW_PAIR_MEDIUM_CENTER = 1,
    };

    glowDrawDisc(&discCenters[SHELTER_R47_GLOW_PAIR_LARGE_CENTER], SHELTER_R47_GLOW_DISC_RADIUS_SCALE_LARGE, SHELTER_R47_GLOW_COLOR_COOL_GREY);
    glowDrawDisc(&discCenters[SHELTER_R47_GLOW_PAIR_MEDIUM_CENTER], SHELTER_R47_GLOW_DISC_RADIUS_SCALE_MEDIUM, SHELTER_R47_GLOW_COLOR_COOL_GREY);
}

void shelterR47DrawViewGlowsTask(Task* unusedTask)
{
    enum {
        SHELTER_R47_GLOW_VIEW_FULL          = 5,
        SHELTER_R47_GLOW_VIEW_PAIR          = 13,
        SHELTER_R47_GLOW_VIEW_PARTIAL       = 14,
        SHELTER_R47_GLOW_VIEW_PULSING       = 44,
        SHELTER_R47_GLOW_PULSE_RATE         = 0x60, // 4096 angle units per turn, per animation frame
        SHELTER_R47_GLOW_PULSE_RADIUS_SCALE = 0xA0, // Diamond scale * 32 / depth; disc scale * 64 / depth
    };
    u8 mappedView;

    // Select visible world points by the mapped camera index, preserving packet order.
    mappedView = viewGetMappedIndex();
    switch (mappedView) {
        case SHELTER_R47_GLOW_VIEW_FULL:
            _glowDrawDiamond(&_gShelterR47ViewGlowCenters[0], SHELTER_R47_GLOW_PULSE_RATE, SHELTER_R47_GLOW_PULSE_RADIUS_SCALE);
            glowDrawDisc(&_gShelterR47ViewGlowCenters[1], SHELTER_R47_GLOW_DISC_RADIUS_SCALE_LARGE, SHELTER_R47_GLOW_COLOR_GREY);
            glowDrawDisc(&_gShelterR47ViewGlowCenters[2], SHELTER_R47_GLOW_DISC_RADIUS_SCALE_LARGE, SHELTER_R47_GLOW_COLOR_GREY);
            glowDrawDisc(&_gShelterR47ViewGlowCenters[3], SHELTER_R47_GLOW_DISC_RADIUS_SCALE_LARGE, SHELTER_R47_GLOW_COLOR_GREY);
            glowDrawDisc(&_gShelterR47ViewGlowCenters[4], SHELTER_R47_GLOW_DISC_RADIUS_SCALE_LARGE, SHELTER_R47_GLOW_COLOR_GREY);
            glowDrawDisc(&_gShelterR47ViewGlowCenters[5], SHELTER_R47_GLOW_DISC_RADIUS_SCALE_LARGE, SHELTER_R47_GLOW_COLOR_GREY);
            glowDrawDisc(&_gShelterR47ViewGlowCenters[6], SHELTER_R47_GLOW_DISC_RADIUS_SCALE_SMALL, SHELTER_R47_GLOW_COLOR_COOL_GREY);
            glowDrawDisc(&_gShelterR47ViewGlowCenters[7], SHELTER_R47_GLOW_DISC_RADIUS_SCALE_EXTRA_LARGE, SHELTER_R47_GLOW_COLOR_COOL_GREY);
            _shelterR47DrawViewGlowPair(&_gShelterR47ViewGlowCenters[8]);
            break;
        case SHELTER_R47_GLOW_VIEW_PAIR:
            _shelterR47DrawViewGlowPair(&_gShelterR47ViewGlowCenters[8]);
            break;
        case SHELTER_R47_GLOW_VIEW_PARTIAL:
            _glowDrawDiamond(&_gShelterR47ViewGlowCenters[0], SHELTER_R47_GLOW_PULSE_RATE, SHELTER_R47_GLOW_PULSE_RADIUS_SCALE);
            glowDrawDisc(&_gShelterR47ViewGlowCenters[6], SHELTER_R47_GLOW_DISC_RADIUS_SCALE_SMALL, SHELTER_R47_GLOW_COLOR_COOL_GREY);
            _shelterR47DrawViewGlowPair(&_gShelterR47ViewGlowCenters[8]);
            break;
        case SHELTER_R47_GLOW_VIEW_PULSING:
            _glowDrawPulsingDisc(&_gShelterR47ViewGlowCenters[0], SHELTER_R47_GLOW_PULSE_RATE, SHELTER_R47_GLOW_PULSE_RADIUS_SCALE);
            glowDrawDisc(&_gShelterR47ViewGlowCenters[6], SHELTER_R47_GLOW_DISC_RADIUS_SCALE_SMALL, SHELTER_R47_GLOW_COLOR_COOL_GREY);
            break;
    }
}

#include "../../shared/effects/glow_draw_disc.inc.c"

#include "../../shared/effects/glow_draw_diamond.inc.c"

#include "../../shared/effects/glow_draw_pulsing_disc.inc.c"
