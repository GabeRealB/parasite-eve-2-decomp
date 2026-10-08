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

/// State handlers of the room's script task, run by
/// `_factoryPanelRun`: set-up, prompt arming, the idle hotspot
/// scan, prompt spawning, the prompt state, the exit and the wait for the
/// lift movement to settle.
static const TaskFuncTable7 _gFactoryPanelStates = {
    {
        _factoryPanelInit,
        _factoryPanelArmPrompt,
        _factoryPanelIdle,
        _factoryPanelOpenPrompt,
        _factoryPanelPrompt,
        _factoryPanelExit,
        _factoryPanelWaitMove,
    },
};

static void _actionPromptResetDefault(Task* task);

static void _factoryPromptTask(Task* task);
static void _factoryPanelMarkMoveSettled(Task* task, s32 messageId, s32 firstArg, s32 secondArg);

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskDesc gFactoryPanelSessionDesc[2] = {
    { { { TASK_BODY_NONE, 32 } }, factoryPanelSpawn, { .value = 0 } },
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

static SVECTOR _gDryfieldFactoryCollision096A8Normals[2] = {
#include "assets/dryfield_factory_collision_096A8_normals.inc"
};

static SVECTOR _gDryfieldFactoryCollision096A8Verts[8] = {
#include "assets/dryfield_factory_collision_096A8_verts.inc"
};

static WorldCollisionGridFace _gDryfieldFactoryCollision096A8Faces[2] = {
#include "assets/dryfield_factory_collision_096A8_faces.inc"
};

static s16 _gDryfieldFactoryCollision096A8Cells[4] = {
#include "assets/dryfield_factory_collision_096A8_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldFactoryCollision096A8Cells[i])
static s16* _gDryfieldFactoryCollision096A8Table[1] = {
#include "assets/dryfield_factory_collision_096A8_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid gFactoryBarrierTemplate = { NULL, _gDryfieldFactoryCollision096A8Normals, _gDryfieldFactoryCollision096A8Verts, _gDryfieldFactoryCollision096A8Faces, _gDryfieldFactoryCollision096A8Table, -4464, -3949, 1, 1, 4000, 2 };

static SVECTOR _gDryfieldFactoryCollision09778Normals[4] = {
#include "assets/dryfield_factory_collision_09778_normals.inc"
};

static SVECTOR _gDryfieldFactoryCollision09778Verts[8] = {
#include "assets/dryfield_factory_collision_09778_verts.inc"
};

static WorldCollisionGridFace _gDryfieldFactoryCollision09778Faces[4] = {
#include "assets/dryfield_factory_collision_09778_faces.inc"
};

static s16 _gDryfieldFactoryCollision09778Cells[10] = {
#include "assets/dryfield_factory_collision_09778_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldFactoryCollision09778Cells[i])
static s16* _gDryfieldFactoryCollision09778Table[2] = {
#include "assets/dryfield_factory_collision_09778_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid gFactoryLiftTemplate = { NULL, _gDryfieldFactoryCollision09778Normals, _gDryfieldFactoryCollision09778Verts, _gDryfieldFactoryCollision09778Faces, _gDryfieldFactoryCollision09778Table, 750, 2191, 1, 2, 4000, 4 };

static SVECTOR _gDryfieldFactoryCollision09844Normals[4] = {
#include "assets/dryfield_factory_collision_09844_normals.inc"
};

static SVECTOR _gDryfieldFactoryCollision09844Verts[8] = {
#include "assets/dryfield_factory_collision_09844_verts.inc"
};

static WorldCollisionGridFace _gDryfieldFactoryCollision09844Faces[4] = {
#include "assets/dryfield_factory_collision_09844_faces.inc"
};

static s16 _gDryfieldFactoryCollision09844Cells[8] = {
#include "assets/dryfield_factory_collision_09844_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldFactoryCollision09844Cells[i])
static s16* _gDryfieldFactoryCollision09844Table[2] = {
#include "assets/dryfield_factory_collision_09844_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid gFactoryLiftTurnedTemplate = { NULL, _gDryfieldFactoryCollision09844Normals, _gDryfieldFactoryCollision09844Verts, _gDryfieldFactoryCollision09844Faces, _gDryfieldFactoryCollision09844Table, 750, 1950, 1, 2, 4000, 4 };

TaskDesc gFactoryDaySpawnTable[8] = {
    { { { TASK_BODY_NONE, 192 } }, factoryPowerScene, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, factoryLampScene, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, factoryCapScene, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, factoryBarrierTransitionScene, { .value = 0 } },
    { { { TASK_BODY_TMD, 192 } }, factoryLiftRun, { .model = &_gDryfieldFactoryModel06604 } },
    { { { TASK_BODY_COORD, 192 } }, factoryBarrierCollision, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, factoryHatchScene, { .value = 0 } },
    { { { TASK_BODY_TMD, 192 } }, factoryHatchRun, { .model = &_gDryfieldFactoryModel093E4 } },
};

TaskDesc gFactoryPromptDesc[1] = {
    { { { TASK_BODY_NONE, 192 } }, _factoryPromptTask, { .value = 0 } },
};

TaskDesc gFactoryDayPanelDesc[1] = {
    { { { TASK_BODY_NONE, 192 } }, _factoryPanelRun, { .value = 0 } },
};

TaskMessageEntry gFactoryPanelMsgTable[2] = {
    { FACTORY_PANEL_MESSAGE_MOVE_SETTLED, _factoryPanelMarkMoveSettled },
    { TASK_MESSAGE_TABLE_END, NULL },
};

ActionPromptHotspot gFactoryPanelHotspots[6] = {
    { -68, -63, 16, 16, 0, 1, 0 },
    { -27, -63, 16, 16, 1, 1, 0 },
    { 13, -63, 16, 16, 2, 1, 0 },
    { 46, -80, 34, 32, 3, 0, 0 },
    { -38, 0, 72, 48, 4, 0, 0 },
    { 0, 0, 0, 0, ACTION_PROMPT_HOTSPOT_END, 0, 0 },
};

SVECTOR gFactoryGlowPos48 = { 395, -1630, 846, 0 };

SVECTOR gFactoryGlowPos4A1 = { 5910, -1308, 5649, 0 };

SVECTOR gFactoryGlowPos4A2 = { 5910, -1404, 5649, 0 };

WorldCollisionRoomResources D_dryfield_factory_80186F10[2] = {
    { &gFactoryDayGrid, D_dryfield_factory_80189694, D_dryfield_factory_80189ABC, NULL },
    { &gFactoryDayGrid, D_dryfield_factory_80189694, D_dryfield_factory_80189ABC, NULL },
};

u8 D_dryfield_factory_80186F30[20] = {
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

u8* D_dryfield_factory_80186F44[2] = {
    gViewIdentityMap,
    D_dryfield_factory_80186F30,
};

WorldCoordRoomLighting D_dryfield_factory_80186F4C[2] = {
    { D_dryfield_factory_8018A28C, D_dryfield_factory_8018A2A4 },
    { D_dryfield_factory_8018A28C, D_dryfield_factory_8018A2A4 },
};

ViewCount D_dryfield_factory_80186F5C[2] = { 19, 19 };

DirectionWarpEntry D_dryfield_factory_80186F60[3] = {
    { { { .word = 3072 }, 5178, 0, 1454 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 4530, 0, 1200 }, { 0, 0, 0, 0 }, 0x52170002, 0x52170001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 474 },
    { { { .word = 1024 }, 642, 0, 7493 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 4530, 0, 1200 }, { 0, 0, 0, 0 }, 0x52170004, 0x52170003, 0x52170005, 7, DIRECTION_WARP_FLAG_NONE, 475 },
    { { { .word = 3072 }, 5445, 0, 6866 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 5420, 0, 6000 }, { 0, 0, 0, 0 }, 0x52170014, 0x52170006, DIRECTION_WARP_SOUND_NONE, 6, DIRECTION_WARP_FLAG_FADE_DEPARTURE, 473 },
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
