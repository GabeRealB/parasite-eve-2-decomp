#include "rooms/dryfield_factory.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "dryfield_factory_private.h"

#include "gameplay/action_prompt.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/player_actor.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/item_menu.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
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
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/dryfield_night_factory.h"

#include "rooms/room_common.h"
static void _factoryPanelPrompt(Task* task);

static s32 _actionPromptHitTestDefault(ActionPromptHotspot* hotspots, s16 cursorX, s16 cursorY);
#define ACTION_PROMPT_HIT_TEST _actionPromptHitTestDefault
#include "../../shared/action_prompt.h"
#include "../../shared/glow_draw.h"
#include "../../shared/room_events.h"
/// Selects the daytime factory instance for shared room declarations and code.
///
/// Keep this binding through all factory implementation fragments.
#define DRYFIELD_TIME DRYFIELD_DAY
#include "../../shared/factory_lift.h"

static void _factoryPanelOpenPrompt(Task* task);
static void _factoryPanelRun(Task* task);
static void _factoryPanelArmPrompt(Task* task);
static void _factoryPanelIdle(Task* task);
static void _factoryPanelExit(Task* task);
static void _factoryPanelWaitMove(Task* task);

/// Operator-panel handlers in FACTORY_PANEL_STATE_ order.
///
/// The dispatcher copies all seven callbacks by value before selecting the
/// current task state. Every slot is populated; there is no terminator.
static const TaskFuncTable7 _gFactoryPanelStates = {
    {
        _factoryPanelInit,       // FACTORY_PANEL_STATE_INIT
        _factoryPanelArmPrompt,  // FACTORY_PANEL_STATE_ARM_PROMPT
        _factoryPanelIdle,       // FACTORY_PANEL_STATE_IDLE
        _factoryPanelOpenPrompt, // FACTORY_PANEL_STATE_OPEN_PROMPT
        _factoryPanelPrompt,     // FACTORY_PANEL_STATE_PROMPT
        _factoryPanelExit,       // FACTORY_PANEL_STATE_EXIT
        _factoryPanelWaitMove,   // FACTORY_PANEL_STATE_WAIT_MOVE
    },
};

static void _actionPromptResetDefault(Task* task);

static void _factoryPromptTask(Task* task);
static void _factoryPanelMarkMoveSettled(Task* task, s32 messageId, s32 firstArg, s32 secondArg);

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskDesc gFactoryPanelSessionDesc[2] = {
    { { { TASK_BODY_NONE, FACTORY_PANEL_SESSION_PRIORITY } }, factoryPanelSpawn, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

TaskMessageEntry gFactoryMsgTable[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, roomVariantFactoryMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, factoryIgnoreMessage },
    { ROOM_MESSAGE_COMMAND, factoryCommand },
    { ROOM_MESSAGE_SOUND, factorySoundCommand },
    { DIRECTION_MESSAGE_ROOM_ACTION, factoryRoomAction },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static TmdBone _gDryfieldFactoryModel06604Skeleton[1] = {
#include "assets/dryfield_factory_model_06604_skeleton.inc"
};

static u32 _gDryfieldFactoryModel06604PartVerts[1] = {
#include "assets/dryfield_factory_model_06604_partVerts.inc"
};

static SVECTOR _gDryfieldFactoryModel06604Verts[306] = {
#include "assets/dryfield_factory_model_06604_verts.inc"
};

static SVECTOR _gDryfieldFactoryModel06604Normals[353] = {
#include "assets/dryfield_factory_model_06604_normals.inc"
};

static u32 _gDryfieldFactoryModel06604Stream[2811] = {
#include "assets/dryfield_factory_model_06604_stream.inc"
};

static TmdSource _gDryfieldFactoryModel06604 = {
    0,
    19284,
    0,
    1,
    _gDryfieldFactoryModel06604PartVerts,
    _gDryfieldFactoryModel06604Verts,
    _gDryfieldFactoryModel06604Normals,
    _gDryfieldFactoryModel06604Skeleton,
    _gDryfieldFactoryModel06604Stream,
};

static TmdBone _gDryfieldFactoryModel093E4Skeleton[1] = {
#include "assets/dryfield_factory_model_093E4_skeleton.inc"
};

static u32 _gDryfieldFactoryModel093E4PartVerts[1] = {
#include "assets/dryfield_factory_model_093E4_partVerts.inc"
};

static SVECTOR _gDryfieldFactoryModel093E4Verts[25] = {
#include "assets/dryfield_factory_model_093E4_verts.inc"
};

static SVECTOR _gDryfieldFactoryModel093E4Normals[28] = {
#include "assets/dryfield_factory_model_093E4_normals.inc"
};

static u32 _gDryfieldFactoryModel093E4Stream[139] = {
#include "assets/dryfield_factory_model_093E4_stream.inc"
};

static TmdSource _gDryfieldFactoryModel093E4 = {
    0,
    856,
    0,
    1,
    _gDryfieldFactoryModel093E4PartVerts,
    _gDryfieldFactoryModel093E4Verts,
    _gDryfieldFactoryModel093E4Normals,
    _gDryfieldFactoryModel093E4Skeleton,
    _gDryfieldFactoryModel093E4Stream,
};

static SVECTOR _gFactoryBarrierTemplateNormals[2] = {
#include "assets/dryfield_factory_collision_096A8_normals.inc"
};

static SVECTOR _gFactoryBarrierTemplateVerts[8] = {
#include "assets/dryfield_factory_collision_096A8_verts.inc"
};

static WorldCollisionGridFace _gFactoryBarrierTemplateFaces[2] = {
#include "assets/dryfield_factory_collision_096A8_faces.inc"
};

static s16 _gFactoryBarrierTemplateCells[4] = {
#include "assets/dryfield_factory_collision_096A8_cells.inc"
};

#define GRID_CELL(i) (&_gFactoryBarrierTemplateCells[i])
static s16* _gFactoryBarrierTemplateTable[1] = {
#include "assets/dryfield_factory_collision_096A8_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid gFactoryBarrierTemplate = {
    .viewCoord   = NULL,
    .normals     = _gFactoryBarrierTemplateNormals,
    .vertices    = _gFactoryBarrierTemplateVerts,
    .faces       = _gFactoryBarrierTemplateFaces,
    .cellFaceIds = _gFactoryBarrierTemplateTable,
    .xBias       = -4464,
    .zBias       = -3949,
    .cellCountX  = 1,
    .cellCountZ  = ARRAY_SIZE(_gFactoryBarrierTemplateTable),
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gFactoryBarrierTemplateFaces),
};

static SVECTOR _gFactoryLiftTemplateNormals[4] = {
#include "assets/dryfield_factory_collision_09778_normals.inc"
};

static SVECTOR _gFactoryLiftTemplateVerts[8] = {
#include "assets/dryfield_factory_collision_09778_verts.inc"
};

static WorldCollisionGridFace _gFactoryLiftTemplateFaces[4] = {
#include "assets/dryfield_factory_collision_09778_faces.inc"
};

static s16 _gFactoryLiftTemplateCells[10] = {
#include "assets/dryfield_factory_collision_09778_cells.inc"
};

#define GRID_CELL(i) (&_gFactoryLiftTemplateCells[i])
static s16* _gFactoryLiftTemplateTable[2] = {
#include "assets/dryfield_factory_collision_09778_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid gFactoryLiftTemplate = {
    .viewCoord   = NULL,
    .normals     = _gFactoryLiftTemplateNormals,
    .vertices    = _gFactoryLiftTemplateVerts,
    .faces       = _gFactoryLiftTemplateFaces,
    .cellFaceIds = _gFactoryLiftTemplateTable,
    .xBias       = 750,
    .zBias       = 2191,
    .cellCountX  = 1,
    .cellCountZ  = ARRAY_SIZE(_gFactoryLiftTemplateTable),
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gFactoryLiftTemplateFaces),
};

static SVECTOR _gFactoryLiftTurnedTemplateNormals[4] = {
#include "assets/dryfield_factory_collision_09844_normals.inc"
};

static SVECTOR _gFactoryLiftTurnedTemplateVerts[8] = {
#include "assets/dryfield_factory_collision_09844_verts.inc"
};

static WorldCollisionGridFace _gFactoryLiftTurnedTemplateFaces[4] = {
#include "assets/dryfield_factory_collision_09844_faces.inc"
};

static s16 _gFactoryLiftTurnedTemplateCells[8] = {
#include "assets/dryfield_factory_collision_09844_cells.inc"
};

#define GRID_CELL(i) (&_gFactoryLiftTurnedTemplateCells[i])
static s16* _gFactoryLiftTurnedTemplateTable[2] = {
#include "assets/dryfield_factory_collision_09844_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid gFactoryLiftTurnedTemplate = {
    .viewCoord   = NULL,
    .normals     = _gFactoryLiftTurnedTemplateNormals,
    .vertices    = _gFactoryLiftTurnedTemplateVerts,
    .faces       = _gFactoryLiftTurnedTemplateFaces,
    .cellFaceIds = _gFactoryLiftTurnedTemplateTable,
    .xBias       = 750,
    .zBias       = 1950,
    .cellCountX  = 1,
    .cellCountZ  = ARRAY_SIZE(_gFactoryLiftTurnedTemplateTable),
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gFactoryLiftTurnedTemplateFaces),
};

TaskDesc gFactoryDaySpawnTable[8] = {
    { { { TASK_BODY_NONE, FACTORY_TASK_PRIORITY } }, factoryPowerScene, { .value = 0 } },
    { { { TASK_BODY_NONE, FACTORY_TASK_PRIORITY } }, factoryLampScene, { .value = 0 } },
    { { { TASK_BODY_NONE, FACTORY_TASK_PRIORITY } }, factoryCapScene, { .value = 0 } },
    { { { TASK_BODY_NONE, FACTORY_TASK_PRIORITY } }, factoryBarrierTransitionScene, { .value = 0 } },
    { { { TASK_BODY_TMD, FACTORY_TASK_PRIORITY } }, factoryLiftRun, { .model = &_gDryfieldFactoryModel06604 } },
    { { { TASK_BODY_COORD, FACTORY_TASK_PRIORITY } }, factoryBarrierCollision, { .value = 0 } },
    { { { TASK_BODY_NONE, FACTORY_TASK_PRIORITY } }, factoryHatchScene, { .value = 0 } },
    { { { TASK_BODY_TMD, FACTORY_TASK_PRIORITY } }, factoryHatchRun, { .model = &_gDryfieldFactoryModel093E4 } },
};

/// Single bodyless priority-192 cursor task owned by an operator-panel session.
///
/// The panel passes spawnArg1 value 1 for controller port 0 and retains the child
/// task until exit; no table terminator or model resource is stored.
static TaskDesc _gFactoryPanelCursorDesc = { { { TASK_BODY_NONE, FACTORY_TASK_PRIORITY } }, _factoryPromptTask, { .value = 0 } };

TaskDesc gFactoryDayPanelDesc = { { { TASK_BODY_NONE, FACTORY_TASK_PRIORITY } }, _factoryPanelRun, { .value = 0 } };

/// Lift-completion notification followed by the task-message end marker.
///
/// The panel borrows this table while its work is live. Both payload words
/// are ignored. The handler has no defined result; senders discard it.
static TaskMessageEntry _gFactoryPanelMsgTable[2] = {
    { FACTORY_PANEL_MESSAGE_MOVE_SETTLED, _factoryPanelMarkMoveSettled },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Mutable operator-panel hotspot rectangles in screen-centred pixels.
///
/// Raise, lower and turn open Push menus; the two caption spots open
/// Examine menus. Hit testing writes every entry's hit byte, and confirm
/// latches the first hit. The sixth entry ends the scan by its ID alone.
static ActionPromptHotspot _gFactoryPanelHotspots[6] = {
    { -68, -63, 16, 16, FACTORY_PANEL_CHOICE_RAISE, FACTORY_PANEL_PROMPT_PUSH, 0 },
    { -27, -63, 16, 16, FACTORY_PANEL_CHOICE_LOWER, FACTORY_PANEL_PROMPT_PUSH, 0 },
    { 13, -63, 16, 16, FACTORY_PANEL_CHOICE_TURN, FACTORY_PANEL_PROMPT_PUSH, 0 },
    { 46, -80, 34, 32, FACTORY_PANEL_CHOICE_CAPTION6, FACTORY_PANEL_PROMPT_EXAMINE, 0 },
    { -38, 0, 72, 48, FACTORY_PANEL_CHOICE_CAPTION7, FACTORY_PANEL_PROMPT_EXAMINE, 0 },
    { 0, 0, 0, 0, ACTION_PROMPT_HOTSPOT_END, 0, 0 },
};

/// Power-glow centre in signed whole world-coordinate units.
static SVECTOR _gFactoryPowerGlowPosition = { 395, -1630, 846, 0 };

/// Lamp-glow centre at lamp progress 1, in signed whole world-coordinate units.
static SVECTOR _gFactoryLampFirstGlowPosition = { 5910, -1308, 5649, 0 };

/// Alternate lamp-glow centre at lamp progress 2, in signed whole world-coordinate units.
static SVECTOR _gFactoryLampSecondGlowPosition = { 5910, -1404, 5649, 0 };

WorldCollisionRoomResources D_dryfield_factory_80186F10[2] = {
    { &gDryfieldFactoryCollisionGrid, D_dryfield_factory_80189694, D_dryfield_factory_80189ABC, NULL },
    { &gDryfieldFactoryCollisionGrid, D_dryfield_factory_80189694, D_dryfield_factory_80189ABC, NULL },
};

/// Room-2 logical-view map used after the barrier is cleared.
///
/// Logical views 1..19 map to camera/image/sprite indices 1..19 in a
/// different order. The twentieth stored byte is zero, outside the valid
/// logical-view domain; lookups use the separate count rather than a sentinel.
static u8 _gDryfieldFactoryClearedBarrierViewMap[20] = {
    1,
    13,
    14,
    15,
    5,
    16,
    17,
    8,
    9,
    10,
    11,
    12,
    2,
    3,
    4,
    6,
    7,
    18,
    19,
    0,
};

u8* gDryfieldFactoryRoomViewMaps[2] = {
    gViewIdentityMap,
    _gDryfieldFactoryClearedBarrierViewMap,
};

WorldCoordRoomLighting gDryfieldFactoryRoomLighting[2] = {
    { .lights = &gDryfieldFactoryRoomLights, .ambientTable = gDryfieldFactoryViewAmbientMinima },
    { .lights = &gDryfieldFactoryRoomLights, .ambientTable = gDryfieldFactoryViewAmbientMinima },
};

enum { DRYFIELD_FACTORY_LOGICAL_VIEW_COUNT = 19 };

ViewCount gDryfieldFactoryRoomViewCounts[2] = { DRYFIELD_FACTORY_LOGICAL_VIEW_COUNT, DRYFIELD_FACTORY_LOGICAL_VIEW_COUNT };

/// Map-marker nibbles shared with the opposite endpoint of each factory door.
enum {
    DRYFIELD_FACTORY_DRIVEWAY_MAP_FLAG  = 474,
    DRYFIELD_FACTORY_BREEZEWAY_MAP_FLAG = 475,
    DRYFIELD_FACTORY_GARAGE_MAP_FLAG    = 473
};

DirectionWarpEntry gDryfieldFactoryWarpEntries[3] = {
    {
        .player         = { { .word = 3072 }, 5178, 0, 1454 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 1024 }, 4530, 0, 1200 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 2),
        .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 1),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_FACTORY_DRIVEWAY_MAP_FLAG,
    },
    {
        .player         = { { .word = 1024 }, 642, 0, 7493 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = 1024 }, 4530, 0, 1200 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 4),
        .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 3),
        .blockedSound   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 5),
        .initialView    = 7,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DRYFIELD_FACTORY_BREEZEWAY_MAP_FLAG,
    },
    {
        .player         = { { .word = 3072 }, 5445, 0, 6866 },
        .unknown_10     = { 0, 0, 0, 0 },
        .companion      = { { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, 5420, 0, 6000 },
        .unknown_24     = { 0, 0, 0, 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 0x14),
        .departureSound = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 6),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 6,
        .flags          = DIRECTION_WARP_FLAG_FADE_DEPARTURE,
        .mapFlagId      = DRYFIELD_FACTORY_GARAGE_MAP_FLAG,
    },
};

#include "../../shared/factory_panel_idle.inc.c"

#include "../../shared/action_prompt_outline_rect.inc.c"

#include "../../shared/factory_panel_run_step.inc.c"

#include "../../shared/action_prompt_move_cursors.inc.c"

#include "../../shared/action_prompt_draw_cursor.inc.c"

#include "../../shared/factory_show_view9_sprite.inc.c"

#include "../../shared/factory_panel_run.inc.c"

#include "../../shared/factory_prompt_task.inc.c"

#include "../../shared/factory_panel_trigger.inc.c"

#include "../../shared/action_prompt_hit_test.inc.c"

#include "../../shared/factory_panel_init.inc.c"

#include "../../shared/factory_panel_arm_prompt.inc.c"

#include "../../shared/factory_panel_open_prompt.inc.c"

#include "../../shared/factory_panel_prompt.inc.c"

#include "../../shared/factory_panel_exit.inc.c"

#include "../../shared/factory_panel_wait_move.inc.c"

#include "../../shared/factory_show_view11_sprite.inc.c"

#include "../../shared/action_prompt_reset.inc.c"

#include "../../shared/glow_draw_tinted_disc.inc.c"

#include "../../shared/factory_draw_glows.inc.c"
