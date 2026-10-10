#include "rooms/dryfield_night_factory.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "dryfield_night_factory_private.h"

#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/action_prompt.h"
#include "gameplay/actor_render.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/sound.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
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

#include "rooms/dryfield_factory.h"

#include "rooms/room_common.h"
static void _factoryRoomInit(Task* task);
static void _factoryPanelPrompt(Task* task);

static s32 _actionPromptHitTestDefault(ActionPromptHotspot* hotspots, s16 cursorX, s16 cursorY);
#define ACTION_PROMPT_HIT_TEST _actionPromptHitTestDefault
#include "../../shared/action_prompt.h"
#include "../../shared/glow_draw.h"
#include "../../shared/room_events.h"
#include "../../shared/room_variants.h"
/// Selects the nighttime factory instance for shared room declarations and code.
///
/// Keep this binding through all factory implementation fragments.
#define DRYFIELD_TIME DRYFIELD_NIGHT
#include "../../shared/factory_lift.h"

static void _factoryEntryIdle(Task* task);
static void _factoryPanelOpenPrompt(Task* task);
static void _factoryPanelRun(Task* task);
static void _factoryPanelArmPrompt(Task* task);
static void _factoryPanelIdle(Task* task);
static void _factoryPanelExit(Task* task);
static void _factoryPanelWaitMove(Task* task);

static void _actionPromptResetDefault(Task* task);

extern TaskDesc gRoomEventTaskDesc;

static void _factoryPromptTask(Task* task);
static void _factoryPanelMarkMoveSettled(Task* task, s32 messageId, s32 firstArg, s32 secondArg);

static TmdBone _gDryfieldNightFactoryDryfieldFactoryModel06604Skeleton[1] = {
#include "assets/dryfield_factory_model_06604_skeleton.inc"
};

static u32 _gDryfieldNightFactoryDryfieldFactoryModel06604PartVerts[1] = {
#include "assets/dryfield_factory_model_06604_partVerts.inc"
};

static SVECTOR _gDryfieldNightFactoryDryfieldFactoryModel06604Verts[306] = {
#include "assets/dryfield_factory_model_06604_verts.inc"
};

static SVECTOR _gDryfieldNightFactoryDryfieldFactoryModel06604Normals[353] = {
#include "assets/dryfield_factory_model_06604_normals.inc"
};

static u32 _gDryfieldNightFactoryDryfieldFactoryModel06604Stream[2811] = {
#include "assets/dryfield_factory_model_06604_stream.inc"
};

static TmdSource _gDryfieldNightFactoryDryfieldFactoryModel06604 = {
    0,
    19284,
    0,
    1,
    _gDryfieldNightFactoryDryfieldFactoryModel06604PartVerts,
    _gDryfieldNightFactoryDryfieldFactoryModel06604Verts,
    _gDryfieldNightFactoryDryfieldFactoryModel06604Normals,
    _gDryfieldNightFactoryDryfieldFactoryModel06604Skeleton,
    _gDryfieldNightFactoryDryfieldFactoryModel06604Stream,
};

static TmdBone _gDryfieldNightFactoryDryfieldFactoryModel093E4Skeleton[1] = {
#include "assets/dryfield_factory_model_093E4_skeleton.inc"
};

static u32 _gDryfieldNightFactoryDryfieldFactoryModel093E4PartVerts[1] = {
#include "assets/dryfield_factory_model_093E4_partVerts.inc"
};

static SVECTOR _gDryfieldNightFactoryDryfieldFactoryModel093E4Verts[25] = {
#include "assets/dryfield_factory_model_093E4_verts.inc"
};

static SVECTOR _gDryfieldNightFactoryDryfieldFactoryModel093E4Normals[28] = {
#include "assets/dryfield_factory_model_093E4_normals.inc"
};

static u32 _gDryfieldNightFactoryDryfieldFactoryModel093E4Stream[139] = {
#include "assets/dryfield_factory_model_093E4_stream.inc"
};

static TmdSource _gDryfieldNightFactoryDryfieldFactoryModel093E4 = {
    0,
    856,
    0,
    1,
    _gDryfieldNightFactoryDryfieldFactoryModel093E4PartVerts,
    _gDryfieldNightFactoryDryfieldFactoryModel093E4Verts,
    _gDryfieldNightFactoryDryfieldFactoryModel093E4Normals,
    _gDryfieldNightFactoryDryfieldFactoryModel093E4Skeleton,
    _gDryfieldNightFactoryDryfieldFactoryModel093E4Stream,
};

static SVECTOR _gFactoryBarrierTemplateNormals[2] = {
#include "assets/dryfield_night_factory_collision_09660_normals.inc"
};

static SVECTOR _gFactoryBarrierTemplateVerts[8] = {
#include "assets/dryfield_night_factory_collision_09660_verts.inc"
};

static WorldCollisionGridFace _gFactoryBarrierTemplateFaces[2] = {
#include "assets/dryfield_night_factory_collision_09660_faces.inc"
};

static s16 _gFactoryBarrierTemplateCells[4] = {
#include "assets/dryfield_night_factory_collision_09660_cells.inc"
};

#define GRID_CELL(i) (&_gFactoryBarrierTemplateCells[i])
static s16* _gFactoryBarrierTemplateTable[1] = {
#include "assets/dryfield_night_factory_collision_09660_table.inc"
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
#include "assets/dryfield_night_factory_collision_09730_normals.inc"
};

static SVECTOR _gFactoryLiftTemplateVerts[8] = {
#include "assets/dryfield_night_factory_collision_09730_verts.inc"
};

static WorldCollisionGridFace _gFactoryLiftTemplateFaces[4] = {
#include "assets/dryfield_night_factory_collision_09730_faces.inc"
};

static s16 _gFactoryLiftTemplateCells[10] = {
#include "assets/dryfield_night_factory_collision_09730_cells.inc"
};

#define GRID_CELL(i) (&_gFactoryLiftTemplateCells[i])
static s16* _gFactoryLiftTemplateTable[2] = {
#include "assets/dryfield_night_factory_collision_09730_table.inc"
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
#include "assets/dryfield_night_factory_collision_097FC_normals.inc"
};

static SVECTOR _gFactoryLiftTurnedTemplateVerts[8] = {
#include "assets/dryfield_night_factory_collision_097FC_verts.inc"
};

static WorldCollisionGridFace _gFactoryLiftTurnedTemplateFaces[4] = {
#include "assets/dryfield_night_factory_collision_097FC_faces.inc"
};

static s16 _gFactoryLiftTurnedTemplateCells[8] = {
#include "assets/dryfield_night_factory_collision_097FC_cells.inc"
};

#define GRID_CELL(i) (&_gFactoryLiftTurnedTemplateCells[i])
static s16* _gFactoryLiftTurnedTemplateTable[2] = {
#include "assets/dryfield_night_factory_collision_097FC_table.inc"
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

TaskDesc gFactoryNightSpawnTable[8] = {
    { { { TASK_BODY_NONE, 192 } }, factoryPowerScene, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, factoryLampScene, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, factoryCapScene, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, factoryBarrierTransitionScene, { .value = 0 } },
    { { { TASK_BODY_TMD, 192 } }, factoryLiftRun, { .model = &_gDryfieldNightFactoryDryfieldFactoryModel06604 } },
    { { { TASK_BODY_COORD, 192 } }, factoryBarrierCollision, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, factoryHatchScene, { .value = 0 } },
    { { { TASK_BODY_TMD, 192 } }, factoryHatchRun, { .model = &_gDryfieldNightFactoryDryfieldFactoryModel093E4 } },
};

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

/// Bodyless priority-32 operator-panel session poller and an unspawned end entry.
///
/// Entry 0 borrows the room-owned panel-task slot, spawns the selected panel
/// and waits for its exit request. The factory room must remain loaded.
static TaskDesc _gFactoryPanelSessionDesc[2] = {
    { { { TASK_BODY_NONE, FACTORY_PANEL_SESSION_PRIORITY } }, factoryPanelSpawn, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Room departures, key-item refusal, CAP commands, sound and direction actions.
///
/// Five native-payload callbacks followed by TASK_MESSAGE_TABLE_END. Resolve
/// borrows eight-byte request/reply records, command and sound use integer
/// first words, and the action callback borrows a four-byte request. The room
/// task borrows this table and its code while the night factory is loaded.
static TaskMessageEntry _gFactoryMsgTable[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, roomVariantFactoryMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, factoryIgnoreMessage },
    { ROOM_MESSAGE_COMMAND, factoryCommand },
    { ROOM_MESSAGE_SOUND, factorySoundCommand },
    { DIRECTION_MESSAGE_ROOM_ACTION, factoryRoomAction },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Single bodyless priority-192 cursor task owned by an operator-panel session.
///
/// The panel passes spawnArg1 value 1 for controller port 0 and retains the child
/// task until exit; no table terminator or model resource is stored.
static TaskDesc _gFactoryPanelCursorDesc = { { { TASK_BODY_NONE, FACTORY_TASK_PRIORITY } }, _factoryPromptTask, { .value = 0 } };

TaskDesc gFactoryNightPanelDesc[1] = {
    { { { TASK_BODY_NONE, 192 } }, _factoryPanelRun, { .value = 0 } },
};

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

u8* gDryfieldNightFactoryViewMaps[2] = {
    gViewIdentityMap,
    gViewIdentityMap,
};

WorldCoordRoomLighting gDryfieldNightFactoryRoomLighting[2] = {
    { .lights = &gDryfieldNightFactoryRoomLights, .ambientTable = gDryfieldNightFactoryAmbientByView },
    { .lights = &gDryfieldNightFactoryRoomLights, .ambientTable = gDryfieldNightFactoryAmbientByView },
};

WorldCollisionRoomResources D_dryfield_night_factory_80186F34[2] = {
    { &gDryfieldNightFactoryCollisionGrid, D_dryfield_night_factory_80189CA0, D_dryfield_night_factory_8018A168, NULL },
    { &gDryfieldNightFactoryCollisionGrid, D_dryfield_night_factory_80189CA0, D_dryfield_night_factory_8018A168, NULL },
};

ViewCount D_dryfield_night_factory_80186F54[2] = { 19, 19 };

DirectionWarpEntry D_dryfield_night_factory_80186F58[3] = {
    { { { .word = 3072 }, 5178, 0, 1454 }, { 0, 0, 0, 0 }, { { .word = 768 }, 3952, 0, 1200 }, { 0, 0, 0, 0 }, 0x53170002, 0x53170001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 474 },
    { { { .word = 1024 }, 642, 0, 7493 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 1100, 0, 7060 }, { 0, 0, 0, 0 }, 0x53170004, 0x53170003, 0x53170005, 7, DIRECTION_WARP_FLAG_NONE, 475 },
    { { { .word = 3072 }, 5445, 1, 6866 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 1100, 0, 7060 }, { 0, 0, 0, 0 }, 0x53170014, 0x53170006, DIRECTION_WARP_SOUND_NONE, 6, DIRECTION_WARP_FLAG_FADE_DEPARTURE, 473 },
};

enum {
    DRYFIELD_NIGHT_FACTORY_COLLISION_CELL_COUNT_X = 2,
    DRYFIELD_NIGHT_FACTORY_COLLISION_CELL_COUNT_Z = 4,
};

/// Room-space normals; the barrier owns entries 0..1 and the lift owns 2..5.
static SVECTOR _gDryfieldNightFactoryCollisionGridNormals[28] = {
#include "assets/dryfield_night_factory_collision_0A630_normals.inc"
};

/// Room-space vertices; the barrier owns entries 0..7 and the lift owns 8..15.
static SVECTOR _gDryfieldNightFactoryCollisionGridVerts[170] = {
#include "assets/dryfield_night_factory_collision_0A630_verts.inc"
};

/// Live room faces, with entries 0..5 reserved for the barrier and lift.
static WorldCollisionGridFace _gDryfieldNightFactoryCollisionGridFaces[72] = {
#include "assets/dryfield_night_factory_collision_0A630_faces.inc"
};

/// Cell face-index lists, terminated by `WORLD_COLLISION_GRID_CELL_END`.
static s16 _gDryfieldNightFactoryCollisionGridCells[288] = {
#include "assets/dryfield_night_factory_collision_0A630_cells.inc"
};

/// Binds generated halfword offsets to the live grid's face-index storage.
///
/// Constant offsets lie within the 288-element pool; the argument is evaluated
/// once. This binding ends after the table include.
#define GRID_CELL(i) (&_gDryfieldNightFactoryCollisionGridCells[i])
/// X-major cell pointers, indexed by x * cellCountZ + z.
static s16* _gDryfieldNightFactoryCollisionGridTable[DRYFIELD_NIGHT_FACTORY_COLLISION_CELL_COUNT_X * DRYFIELD_NIGHT_FACTORY_COLLISION_CELL_COUNT_Z] = {
#include "assets/dryfield_night_factory_collision_0A630_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid gDryfieldNightFactoryCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldNightFactoryCollisionGridNormals,
    .vertices    = _gDryfieldNightFactoryCollisionGridVerts,
    .faces       = _gDryfieldNightFactoryCollisionGridFaces,
    .cellFaceIds = _gDryfieldNightFactoryCollisionGridTable,
    .xBias       = 444,
    .zBias       = 222,
    .cellCountX  = DRYFIELD_NIGHT_FACTORY_COLLISION_CELL_COUNT_X,
    .cellCountZ  = DRYFIELD_NIGHT_FACTORY_COLLISION_CELL_COUNT_Z,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldNightFactoryCollisionGridFaces),
};

ViewCamera D_dryfield_night_factory_80187C14[19] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -3057, 0x44F4, -6028 } }, 240 },
    { { { { 1147, 0, -3931 }, { -130, 4093, -37 }, { 3929, 135, 1147 } }, { -214, 1283, -1020 } }, 246 },
    { { { { 981, 0, 3976 }, { 62, 4095, -15 }, { -3976, 64, 981 } }, { -5724, 1309, -1020 } }, 246 },
    { { { { -3926, 0, -1165 }, { -458, 3765, 1545 }, { 1070, 1612, -3609 } }, { -847, 2531, -7624 } }, 257 },
    { { { { 0, 0, 4096 }, { 23, 4095, 0 }, { -4095, 23, 0 } }, { -2274, 1463, -666 } }, 680 },
    { { { { -4095, 0, 24 }, { 12, 3554, 2035 }, { -21, 2035, -3554 } }, { -4773, 2743, -9126 } }, 230 },
    { { { { -3993, 0, -911 }, { -270, 3911, 1183 }, { 870, 1214, -3813 } }, { -1048, 2819, -0x2BC8 } }, 246 },
    { { { { -1035, 0, 3962 }, { 929, 3981, 242 }, { -3852, 960, -1006 } }, { -5890, 2089, -0x284C } }, 240 },
    { { { { -489, 0, -4066 }, { -641, 4044, 77 }, { 4015, 646, -483 } }, { -337, 1837, -9758 } }, 240 },
    { { { { 619, 0, -4048 }, { -727, 4029, -111 }, { 3982, 736, 608 } }, { -4642, 1552, -5446 } }, 257 },
    { { { { 1741, 0, -3707 }, { 404, 4071, 190 }, { 3685, -446, 1731 } }, { -4747, 1512, -9880 } }, 282 },
    { { { { 0, 0, 4096 }, { 23, 4095, 0 }, { -4095, 23, 0 } }, { -2274, 1463, -666 } }, 680 },
    { { { { 1147, 0, -3931 }, { -130, 4093, -37 }, { 3929, 135, 1147 } }, { -214, 1283, -1020 } }, 246 },
    { { { { 981, 0, 3976 }, { 62, 4095, -15 }, { -3976, 64, 981 } }, { -5724, 1309, -1020 } }, 246 },
    { { { { -3926, 0, -1165 }, { -458, 3765, 1545 }, { 1070, 1612, -3609 } }, { -847, 2531, -7624 } }, 257 },
    { { { { -4095, 0, 24 }, { 12, 3554, 2035 }, { -21, 2035, -3554 } }, { -4773, 2743, -9126 } }, 230 },
    { { { { -3993, 0, -911 }, { -270, 3911, 1183 }, { 870, 1214, -3813 } }, { -1048, 2819, -0x2BC8 } }, 246 },
    { { { { -3738, 0, -1672 }, { -680, 3741, 1520 }, { 1528, 1665, -3415 } }, { -2135, 3083, -0x2DB6 } }, 263 },
    { { { { -3738, 0, -1672 }, { -680, 3741, 1520 }, { 1528, 1665, -3415 } }, { -2135, 3083, -0x2DB6 } }, 263 },
};

// Sprite pages contain 8-bit textures at VRAM Y 0; modulation 128 is neutral.
enum {
    DRYFIELD_NIGHT_FACTORY_SPRITE_TEXTURE_8BIT  = 1,
    DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832     = getTPage(DRYFIELD_NIGHT_FACTORY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0),
    DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896     = getTPage(DRYFIELD_NIGHT_FACTORY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0),
    DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960     = getTPage(DRYFIELD_NIGHT_FACTORY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0),
    DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255  = getClut(0, 255),
    DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_256  = getClut(0, 256),
    DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR = 128,
};

SpriteBatch gDryfieldNightFactoryView1SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

SpriteSource gDryfieldNightFactoryView2SpriteSources[13] = {
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 80 } }, 104, 40, 300, { .fields = { 8, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 80 } }, 88, 40, 700, { .fields = { 40, 80 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 80, 48 } }, -48, -32, 0x3847, { .fields = { 88, 80 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 56 } }, -48, 16, 1058, { .fields = { 72, 200 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, -32, 16, 1021, { .fields = { 16, 128 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -32, 32, 0x3318, { .fields = { 32, 160 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 40, 56 } }, -8, 16, 0x2749, { .fields = { 32, 200 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 56 } }, -24, 8, 1150, { .fields = { 104, 200 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 56 } }, -40, 8, 1140, { .fields = { 88, 200 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 200 } }, -160, -120, 1037, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 200 } }, -136, -120, 1125, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 200 } }, -112, -120, 1162, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 200 } }, -96, -120, 1212, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
};

SpriteBatch gDryfieldNightFactoryView2SpriteBatches[6] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 2, .field_6 = { 3, 0 } },
    { .firstSprite = 2, .spriteCount = 5 },
    { .firstSprite = 7, .spriteCount = 2, .field_6 = { 2, 0 } },
    { .firstSprite = 9, .spriteCount = ARRAY_SIZE(gDryfieldNightFactoryView2SpriteSources) - 9, .field_6 = { 1, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

SpriteSource gDryfieldNightFactoryView3SpriteSources[12] = {
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 72, 240 } }, -160, -120, 50, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 88, 88 } }, 72, 32, 0, { .fields = { 24, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 40, 200 } }, 120, -120, 2000, { .fields = { 16, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 200 } }, 64, -120, 2000, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 80 } }, 96, -120, 2000, { .fields = { 88, 88 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, 96, 56, 1150, { .fields = { 32, 200 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, 96, 40, 1157, { .fields = { 80, 240 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, 96, 24, 1155, { .fields = { 56, 240 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, 96, 8, 1153, { .fields = { 32, 240 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, 96, -8, 1147, { .fields = { 32, 224 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, 96, -24, 1151, { .fields = { 8, 200 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, 96, -40, 1150, { .fields = { 104, 240 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
};

SpriteBatch gDryfieldNightFactoryView3SpriteBatches[5] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 1, .field_6 = { 1, 0 } },
    { .firstSprite = 1, .spriteCount = 1, .field_6 = { 2, 0 } },
    { .firstSprite = 2, .spriteCount = ARRAY_SIZE(gDryfieldNightFactoryView3SpriteSources) - 2 },
    { .firstSprite = SPRITE_BATCH_END },
};

SpriteSource gDryfieldNightFactoryView4SpriteSources[82] = {
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -152, 8, 1041, { .fields = { 40, 24 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -152, 32, 1103, { .fields = { 40, 80 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -152, 56, 0, { .fields = { 32, 160 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -16, 8, 0, { .fields = { 8, 160 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, -16, 32, 978, { .fields = { 8, 184 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -16, 48, 965, { .fields = { 40, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -128, 8, 1130, { .fields = { 88, 216 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, -88, 8, 1106, { .fields = { 72, 104 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -56, 8, 0, { .fields = { 88, 144 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, -48, 24, 1031, { .fields = { 120, 96 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, -48, 40, 937, { .fields = { 96, 192 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 136, 8 } }, -152, 64, 0, { .fields = { 32, 248 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -128, 24, 1075, { .fields = { 120, 64 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, -128, 40, 1086, { .fields = { 64, 80 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -88, 24, 1063, { .fields = { 120, 208 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, -88, 40, 1004, { .fields = { 72, 120 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, -160, -72, 1487, { .fields = { 112, 120 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, -160, -48, 1475, { .fields = { 104, 144 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 32 } }, -160, -24, 1488, { .fields = { 40, 96 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 40, 32 } }, -128, -24, 1455, { .fields = { 32, 168 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, -128, -48, 1450, { .fields = { 96, 56 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, -128, -72, 1475, { .fields = { 104, 168 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 40, 32 } }, -96, -72, 0, { .fields = { 16, 128 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -96, -40, 1455, { .fields = { 80, 96 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, -72, -24, 0, { .fields = { 48, 32 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, -64, -24, 1500, { .fields = { 64, 64 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 32 } }, -88, -24, 1450, { .fields = { 56, 32 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 16 } }, -160, -80, 1025, { .fields = { 64, 80 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 16 } }, -104, -80, 954, { .fields = { 80, 24 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 64, 16 } }, -48, -80, 908, { .fields = { 88, 168 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 64, 16 } }, 16, -80, 839, { .fields = { 72, 128 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 16 } }, 80, -80, 795, { .fields = { 104, 192 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 32 } }, 80, -64, 820, { .fields = { 0, 200 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 104 } }, 128, -32, 0, { .fields = { 120, 136 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 72 } }, 120, 0, 0, { .fields = { 104, 136 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 80, -32, 883, { .fields = { 112, 48 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 72, 0, 979, { .fields = { 120, 232 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 16 } }, 72, -16, 873, { .fields = { 32, 216 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 48 } }, 72, -64, 0, { .fields = { 104, 208 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 72, 16, 986, { .fields = { 120, 80 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 112, 32, 0, { .fields = { 72, 168 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 96 } }, 64, -64, 0, { .fields = { 112, 136 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 64, 32, 985, { .fields = { 0, 152 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 64, 48, 986, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 136 } }, 16, -64, 0, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 64, 24 } }, -48, -64, 1014, { .fields = { 32, 192 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 24 } }, -104, -64, 999, { .fields = { 48, 144 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, -152, -64, 1053, { .fields = { 24, 104 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 48 } }, -160, -64, 0, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, -152, -40, 1092, { .fields = { 56, 168 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 32 } }, -104, -40, 1040, { .fields = { 8, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 64, 32 } }, -48, -40, 996, { .fields = { 0, 64 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 64, 24 } }, -48, -8, 1049, { .fields = { 32, 56 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 24 } }, -104, -8, 1107, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, -144, -16, 1150, { .fields = { 8, 128 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, -160, -16, 0, { .fields = { 72, 24 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -160, 8, 0, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -136, 8, 1040, { .fields = { 64, 24 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 32 } }, -128, 8, 1077, { .fields = { 24, 32 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -160, 32, 0, { .fields = { 56, 48 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 88, 8 } }, -160, 40, 0, { .fields = { 72, 224 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 24 } }, -104, 16, 1044, { .fields = { 104, 96 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 64, 24 } }, -48, 16, 1064, { .fields = { 88, 32 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 16 } }, -40, 40, 1039, { .fields = { 96, 112 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -72, 40, 1078, { .fields = { 104, 24 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 120, 24 } }, -160, 48, 0, { .fields = { 40, 232 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 16 } }, -40, 56, 0, { .fields = { 64, 64 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, 104, -40, 844, { .fields = { 32, 232 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, 104, -16, 0, { .fields = { 112, 232 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, 96, 8, 908, { .fields = { 40, 232 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 72, 24, 1284, { .fields = { 56, 248 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, 56, 0, 1350, { .fields = { 32, 96 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 56, -40, 0, { .fields = { 72, 48 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, 64, -40, 1347, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 64, -8, 1255, { .fields = { 72, 88 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, 72, 8, 1250, { .fields = { 80, 240 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 72, -16, 1237, { .fields = { 24, 168 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 72, -40, 1230, { .fields = { 8, 24 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 48 } }, -16, 16, 1083, { .fields = { 80, 192 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -16, -24, 987, { .fields = { 56, 208 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -16, -64, 884, { .fields = { 56, 128 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 56 } }, -16, -120, 792, { .fields = { 80, 136 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
};

SpriteBatch gDryfieldNightFactoryView4SpriteBatches[7] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 16 },
    { .firstSprite = 16, .spriteCount = 11, .field_6 = { 3, 0 } },
    { .firstSprite = 27, .spriteCount = 40, .field_6 = { 2, 0 } },
    { .firstSprite = 67, .spriteCount = 11, .field_6 = { 4, 0 } },
    { .firstSprite = 78, .spriteCount = ARRAY_SIZE(gDryfieldNightFactoryView4SpriteSources) - 78, .field_6 = { 1, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

SpriteBatch gDryfieldNightFactoryView5SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

SpriteSource gDryfieldNightFactoryView6SpriteSources[32] = {
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 96, 32 } }, -160, 88, 425, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 64, 24 } }, -64, 96, 425, { .fields = { 40, 56 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 128, 40 } }, 8, -24, 1416, { .fields = { 0, 168 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 40, 72 } }, 112, -88, 1750, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 40 } }, -48, -56, 1550, { .fields = { 96, 128 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 32 } }, -48, -88, 1432, { .fields = { 24, 208 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 32 } }, -16, -48, 1684, { .fields = { 16, 32 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, 8, -48, 0, { .fields = { 40, 32 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 32 } }, 16, -48, 1750, { .fields = { 40, 128 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -16, -64, 1704, { .fields = { 80, 240 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 112, -120, 1143, { .fields = { 48, 200 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 112, -96, 1197, { .fields = { 64, 112 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 112, -72, 1244, { .fields = { 80, 176 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 112, -48, 1327, { .fields = { 64, 88 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 112, -24, 1301, { .fields = { 32, 176 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 64, -24, 1386, { .fields = { 24, 136 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 64, -48, 1294, { .fields = { 48, 224 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 64, -72, 1229, { .fields = { 40, 32 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 64, -96, 1178, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 64, -120, 1123, { .fields = { 112, 88 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 24 } }, 8, -120, 1123, { .fields = { 104, 64 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 24 } }, 8, -96, 1243, { .fields = { 112, 128 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 24 } }, 8, -72, 1192, { .fields = { 96, 208 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 24 } }, 8, -48, 1302, { .fields = { 72, 152 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 24 } }, 8, -24, 1368, { .fields = { 88, 32 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 40, 32 } }, 120, -56, 1351, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 40, 32 } }, 120, -88, 1216, { .fields = { 48, 32 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 40, 32 } }, 120, -120, 1083, { .fields = { 56, 128 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 32 } }, 120, -24, 1383, { .fields = { 112, 208 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_256, { .fields = { 56, 32 } }, -64, 88, 625, { .fields = { 56, 208 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_256, { .fields = { 96, 56 } }, -160, 64, 500, { .fields = { 32, 72 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_256, { .fields = { 56, 24 } }, -64, 64, 0, { .fields = { 96, 232 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
};

SpriteBatch gDryfieldNightFactoryView6SpriteBatches[7] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 3, .field_6 = { 4, 0 } },
    { .firstSprite = 3, .spriteCount = 7, .field_6 = { 1, 0 } },
    { .firstSprite = 10, .spriteCount = 15, .field_6 = { 3, 0 } },
    { .firstSprite = 25, .spriteCount = 4, .field_6 = { 2, 0 } },
    { .firstSprite = 29, .spriteCount = ARRAY_SIZE(gDryfieldNightFactoryView6SpriteSources) - 29 },
    { .firstSprite = SPRITE_BATCH_END },
};

SpriteSource gDryfieldNightFactoryView7SpriteSources[39] = {
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 56, 96, 500, { .fields = { 24, 160 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, -80, -48, 2377, { .fields = { 0, 112 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, -80, -32, 2425, { .fields = { 0, 56 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, -48, -32, 2450, { .fields = { 0, 72 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -80, -16, 2375, { .fields = { 16, 88 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -56, -16, 2375, { .fields = { 16, 32 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, -32, -16, 2475, { .fields = { 24, 136 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 48 } }, 56, -32, 2250, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, 8, 8, 1935, { .fields = { 8, 16 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -16, 0, 1935, { .fields = { 32, 184 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -40, 0, 1935, { .fields = { 32, 64 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 32 } }, -64, -8, 1935, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 64 } }, -64, -88, 1833, { .fields = { 96, 128 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 64 } }, -32, -88, 1791, { .fields = { 104, 192 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 64 } }, -8, -88, 1791, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 64 } }, 8, -88, 1740, { .fields = { 96, 64 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 48 } }, -64, -24, 1982, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 48 } }, -32, -24, 1911, { .fields = { 72, 144 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 48 } }, -8, -24, 1911, { .fields = { 72, 192 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 48 } }, 8, -24, 1848, { .fields = { 64, 96 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 48 } }, 40, -88, 1710, { .fields = { 88, 192 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 40, 32 } }, 56, -88, 1675, { .fields = { 32, 224 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, 56, -56, 1675, { .fields = { 56, 48 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, 80, -56, 1675, { .fields = { 56, 144 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, 56, -16, 1780, { .fields = { 40, 88 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, 80, -16, 1780, { .fields = { 56, 184 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 48 } }, 16, -88, 1638, { .fields = { 80, 48 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 32 } }, 16, -40, 1719, { .fields = { 40, 128 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 32 } }, 16, -8, 1792, { .fields = { 40, 32 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, -160, 40, 1200, { .fields = { 96, 48 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, -160, 56, 1200, { .fields = { 16, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, -160, 72, 1200, { .fields = { 24, 208 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, -160, 88, 1200, { .fields = { 40, 208 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_256, { .fields = { 16, 8 } }, -160, 104, 1200, { .fields = { 64, 88 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, -144, 48, 1200, { .fields = { 72, 240 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, -144, 64, 1200, { .fields = { 88, 240 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, -144, 80, 1200, { .fields = { 16, 224 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, -144, 96, 1200, { .fields = { 16, 184 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_256, { .fields = { 8, 24 } }, -128, 64, 1200, { .fields = { 32, 112 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
};

SpriteBatch gDryfieldNightFactoryView7SpriteBatches[8] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 1, .field_6 = { 3, 0 } },
    { .firstSprite = 1, .spriteCount = 7 },
    { .firstSprite = 8, .spriteCount = 4, .field_6 = { 5, 0 } },
    { .firstSprite = 12, .spriteCount = 14, .field_6 = { 1, 0 } },
    { .firstSprite = 26, .spriteCount = 3, .field_6 = { 4, 0 } },
    { .firstSprite = 29, .spriteCount = ARRAY_SIZE(gDryfieldNightFactoryView7SpriteSources) - 29, .field_6 = { 2, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

SpriteSource gDryfieldNightFactoryView8SpriteSources[12] = {
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 48 } }, -128, -120, 125, { .fields = { 96, 56 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -120, -72, 125, { .fields = { 104, 248 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 40 } }, -120, -64, 125, { .fields = { 96, 104 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, -112, -24, 125, { .fields = { 96, 32 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -112, -8, 125, { .fields = { 104, 48 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -112, 0, 125, { .fields = { 104, 224 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 32 } }, -104, 24, 125, { .fields = { 112, 144 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 56 } }, -96, 56, 125, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -88, 88, 125, { .fields = { 120, 200 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, -144, 64, 125, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 16 } }, -160, 80, 125, { .fields = { 64, 16 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, -136, 96, 125, { .fields = { 96, 176 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
};

SpriteBatch gDryfieldNightFactoryView8SpriteBatches[3] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(gDryfieldNightFactoryView8SpriteSources) },
    { .firstSprite = SPRITE_BATCH_END },
};

SpriteSource gDryfieldNightFactoryView9SpriteSources[4] = {
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 64 } }, 72, 16, 1062, { .fields = { 112, 128 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 64 } }, 56, 16, 1062, { .fields = { 112, 192 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 64 } }, 40, 16, 1062, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 64 } }, 32, 16, 1062, { .fields = { 120, 64 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
};

SpriteBatch gDryfieldNightFactoryView9SpriteBatches[3] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(gDryfieldNightFactoryView9SpriteSources) },
    { .firstSprite = SPRITE_BATCH_END },
};

SpriteBatch gDryfieldNightFactoryView10SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

SpriteBatch gDryfieldNightFactoryView11SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

SpriteBatch gDryfieldNightFactoryView12SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

SpriteBatch gDryfieldNightFactoryView13SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

SpriteBatch gDryfieldNightFactoryView14SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

SpriteSource gDryfieldNightFactoryView15SpriteSources[78] = {
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -16, 8, 0, { .fields = { 64, 168 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -56, 8, 0, { .fields = { 24, 168 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 136, 8 } }, -152, 64, 0, { .fields = { 88, 136 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -152, 56, 0, { .fields = { 48, 160 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -152, 32, 1103, { .fields = { 40, 112 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, -128, 40, 1086, { .fields = { 24, 24 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, -88, 40, 1004, { .fields = { 32, 216 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, -48, 40, 937, { .fields = { 88, 192 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -16, 48, 965, { .fields = { 88, 168 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, -16, 32, 978, { .fields = { 32, 48 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, -48, 24, 1031, { .fields = { 32, 240 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -88, 24, 1063, { .fields = { 24, 200 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 16 } }, -88, 8, 1106, { .fields = { 88, 240 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -128, 24, 1075, { .fields = { 104, 48 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -152, 8, 1041, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -128, 8, 1130, { .fields = { 112, 168 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, -72, -24, 0, { .fields = { 64, 128 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 40, 32 } }, -96, -72, 0, { .fields = { 48, 168 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, -64, -24, 1500, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -96, -40, 1450, { .fields = { 112, 32 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 32 } }, -88, -24, 1450, { .fields = { 72, 136 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 40, 32 } }, -128, -24, 1450, { .fields = { 40, 96 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 32 } }, -160, -24, 1488, { .fields = { 40, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, -160, -48, 1475, { .fields = { 64, 112 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, -128, -48, 1450, { .fields = { 64, 24 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, -128, -72, 1475, { .fields = { 56, 48 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, -160, -72, 1475, { .fields = { 88, 216 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 136 } }, 16, -64, 0, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 48 } }, 72, -64, 0, { .fields = { 104, 208 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 96 } }, 64, -64, 0, { .fields = { 112, 136 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 120, 24 } }, -160, 48, 0, { .fields = { 0, 144 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 88, 8 } }, -160, 40, 0, { .fields = { 72, 80 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -160, 32, 0, { .fields = { 16, 248 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 24 } }, -160, 8, 0, { .fields = { 64, 192 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, -160, -16, 0, { .fields = { 72, 216 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 112, 32, 0, { .fields = { 88, 136 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 72 } }, 120, 0, 0, { .fields = { 104, 136 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 104 } }, 128, -32, 0, { .fields = { 120, 136 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 64, 48, 986, { .fields = { 120, 96 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 64, 32, 985, { .fields = { 0, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 72, 16, 986, { .fields = { 112, 48 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 72, 0, 979, { .fields = { 112, 64 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 16 } }, 72, -16, 873, { .fields = { 104, 216 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, 80, -32, 883, { .fields = { 120, 120 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 32 } }, 80, -64, 884, { .fields = { 24, 64 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 16 } }, 80, -80, 795, { .fields = { 0, 88 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 64, 16 } }, 16, -80, 839, { .fields = { 104, 104 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 64, 16 } }, -48, -80, 908, { .fields = { 96, 232 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 16 } }, -104, -80, 954, { .fields = { 96, 200 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 16 } }, -160, -80, 1025, { .fields = { 96, 16 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 48 } }, -160, -64, 0, { .fields = { 96, 136 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, -152, -64, 1053, { .fields = { 96, 24 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 24 } }, -104, -64, 999, { .fields = { 96, 120 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 64, 24 } }, -48, -64, 1014, { .fields = { 56, 88 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 64, 32 } }, -48, -40, 996, { .fields = { 16, 32 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 32 } }, -104, -40, 1040, { .fields = { 48, 224 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, -152, -40, 1092, { .fields = { 120, 192 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, -144, -16, 1150, { .fields = { 120, 144 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -136, 8, 1040, { .fields = { 32, 144 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 32 } }, -128, 8, 1077, { .fields = { 40, 128 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 24 } }, -104, -8, 1107, { .fields = { 112, 168 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 64, 24 } }, -48, -8, 1049, { .fields = { 88, 64 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 64, 24 } }, -48, 16, 1064, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 24 } }, -104, 16, 1044, { .fields = { 120, 224 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -72, 40, 1078, { .fields = { 8, 216 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 16 } }, -40, 40, 1039, { .fields = { 8, 184 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 16 } }, -40, 56, 0, { .fields = { 32, 72 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 72, 24, 0, { .fields = { 16, 24 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 72, -40, 1250, { .fields = { 56, 200 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 72, -16, 1250, { .fields = { 8, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, 72, 8, 1250, { .fields = { 64, 240 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 64, -8, 1255, { .fields = { 88, 176 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, 64, -40, 1347, { .fields = { 40, 160 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, 56, 0, 1350, { .fields = { 40, 192 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 56, -40, 0, { .fields = { 96, 184 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, 96, 8, 1250, { .fields = { 24, 120 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, 104, -40, 1250, { .fields = { 112, 232 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, 104, -16, 0, { .fields = { 48, 200 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
};

SpriteBatch gDryfieldNightFactoryView15SpriteBatches[6] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 16, .field_6 = { 3, 0 } },
    { .firstSprite = 16, .spriteCount = 11 },
    { .firstSprite = 27, .spriteCount = 40, .field_6 = { 2, 0 } },
    { .firstSprite = 67, .spriteCount = ARRAY_SIZE(gDryfieldNightFactoryView15SpriteSources) - 67, .field_6 = { 1, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

SpriteSource gDryfieldNightFactoryView16SpriteSources[32] = {
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 72, 24 } }, -72, 96, 757, { .fields = { 56, 24 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 88, 32 } }, -160, 88, 667, { .fields = { 16, 184 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 88, 112 } }, -160, -24, 0, { .fields = { 40, 144 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 72, 120 } }, -72, -24, 0, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 144 } }, 0, -24, 0, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 128, 40 } }, 8, -24, 1416, { .fields = { 112, 104 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 128, 104 } }, 8, 16, 0, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 40, 72 } }, 112, -88, 1750, { .fields = { 0, 104 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 64, 24 } }, -16, -88, 0, { .fields = { 64, 48 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 32 } }, -48, -88, 1432, { .fields = { 40, 144 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 32, 40 } }, -48, -56, 1550, { .fields = { 72, 144 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 32 } }, 16, -48, 1750, { .fields = { 64, 216 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -16, -64, 1704, { .fields = { 16, 240 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, 8, -48, 0, { .fields = { 40, 104 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 32 } }, -16, -48, 1684, { .fields = { 80, 216 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 48 } }, 32, -64, 0, { .fields = { 112, 104 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 64, 72 } }, 48, -88, 0, { .fields = { 104, 176 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 24 } }, 8, -24, 1368, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 24 } }, 8, -48, 1302, { .fields = { 72, 72 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 24 } }, 8, -72, 1192, { .fields = { 56, 120 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 24 } }, 8, -96, 1243, { .fields = { 64, 144 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 56, 24 } }, 8, -120, 1123, { .fields = { 56, 96 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 64, -120, 1123, { .fields = { 0, 24 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 64, -96, 1178, { .fields = { 0, 48 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 64, -72, 1229, { .fields = { 0, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 64, -48, 1294, { .fields = { 72, 120 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 64, -24, 1386, { .fields = { 16, 216 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 112, -24, 1301, { .fields = { 96, 192 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 112, -48, 1311, { .fields = { 96, 216 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 112, -72, 1244, { .fields = { 120, 144 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_896, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 112, -96, 1197, { .fields = { 0, 72 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_832, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, 112, -120, 1143, { .fields = { 96, 168 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
};

SpriteBatch gDryfieldNightFactoryView16SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = 0, .spriteCount = 7, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 1, 0 } },
    { .firstSprite = 7, .spriteCount = 10, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 2, 0 } },
    { .firstSprite = 17, .spriteCount = ARRAY_SIZE(gDryfieldNightFactoryView16SpriteSources) - 17, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

SpriteBatch gDryfieldNightFactoryView17SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

SpriteSource gDryfieldNightFactoryView18And19SpriteSources[] = {
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -56, 40, 1300, { .fields = { 8, 224 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -56, 48, 1750, { .fields = { 56, 216 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 48, 16 } }, -56, 64, 1750, { .fields = { 80, 232 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -48, 8, 1325, { .fields = { 40, 216 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -64, 8, 1325, { .fields = { 48, 248 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -48, 16, 1312, { .fields = { 40, 232 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -64, 16, 1320, { .fields = { 32, 8 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -64, 24, 1322, { .fields = { 40, 240 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -48, 24, 1315, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -48, 32, 1300, { .fields = { 96, 248 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -64, 32, 1362, { .fields = { 80, 248 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -64, 40, 1375, { .fields = { 112, 248 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -48, 40, 1312, { .fields = { 64, 248 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 16 } }, -40, 40, 1275, { .fields = { 104, 216 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -40, 32, 1282, { .fields = { 56, 240 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, -40, 24, 1287, { .fields = { 56, 232 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
    { DRYFIELD_NIGHT_FACTORY_SPRITE_TPAGE_960, DRYFIELD_NIGHT_FACTORY_SPRITE_CLUT_ROW_255, { .fields = { 80, 216 } }, -112, -120, 500, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, DRYFIELD_NIGHT_FACTORY_SPRITE_NEUTRAL_COLOR, 0 },
};

SpriteBatch gDryfieldNightFactoryView18And19SpriteBatches[] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = 0, .spriteCount = 3, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 3, 0 } },
    { .firstSprite = 3, .spriteCount = 10, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = 13, .spriteCount = 3, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 2, 0 } },
    { .firstSprite = 16, .spriteCount = ARRAY_SIZE(gDryfieldNightFactoryView18And19SpriteSources) - 16, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 1, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

/// State handlers of the room entry task: set-up, an empty tick and
/// `taskKill`.
static const TaskFuncTable3 _gFactoryEntryStates = {
    { _factoryRoomInit, _factoryEntryIdle, taskKill },
};

#include "../../shared/factory_room_init.inc.c"

#include "../../shared/factory_resolve_warp.inc.c"

#include "../../shared/factory_panel_spawn.inc.c"

#include "../../shared/factory_ignore_message.inc.c"

#include "../../shared/factory_command.inc.c"

#include "../../shared/factory_sound_command.inc.c"

#include "../../shared/factory_room_action.inc.c"

#include "../../shared/factory_entry_idle.inc.c"

#include "../../shared/factory_entry_task.inc.c"

#include "../../shared/factory_panel_idle.inc.c"

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
