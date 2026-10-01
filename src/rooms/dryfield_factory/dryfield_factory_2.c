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
#include "../../shared/action_prompt.h"
#include "../../shared/glow_draw.h"
#include "../../shared/room_events.h"
#include "../../shared/factory_lift.h"

extern SVECTOR D_dryfield_factory_80186EF8;
extern SVECTOR D_dryfield_factory_80186F00;
extern SVECTOR D_dryfield_factory_80186F08;

/// State handlers of the room's script task, run by
/// `factoryPanelRun`: set-up, prompt arming, the idle hotspot
/// scan, prompt spawning, the prompt state, the exit and the wait for the
/// message handler's trigger.
static const TaskFuncTable7 _gFactoryPanelStates = {
    {
        factoryPanelInit,
        factoryPanelArmPrompt,
        factoryPanelIdle,
        factoryPanelOpenPrompt,
        factoryPanelPrompt,
        factoryPanelExit,
        factoryPanelWaitMove,
    },
};

void factoryPanelRun(Task*);
void factoryPromptTask(Task*);
void factoryPanelTrigger(Task*);

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskDesc gFactoryPanelSessionDesc[2] = {
    { { { TASK_BODY_NONE, 32 } }, factoryPanelSpawn, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

TaskMessageEntry gFactoryMsgTable[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, factoryResolveWarp },
    { 5105, factoryIgnoreMessage },
    { 5104, factoryCommand },
    { 5106, factorySoundCommand },
    { DIRECTION_MESSAGE_ROOM_ACTION, factoryRoomAction },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TmdBone D_dryfield_factory_80182704[1] = {
#include "assets/dryfield_factory_model_06604_skeleton.inc"
};

u32 D_dryfield_factory_80182728[1] = {
#include "assets/dryfield_factory_model_06604_partVerts.inc"
};

SVECTOR D_dryfield_factory_8018272C[306] = {
#include "assets/dryfield_factory_model_06604_verts.inc"
};

SVECTOR D_dryfield_factory_801830BC[353] = {
#include "assets/dryfield_factory_model_06604_normals.inc"
};

u32 D_dryfield_factory_80183BC4[2811] = {
#include "assets/dryfield_factory_model_06604_stream.inc"
};

TmdSource D_dryfield_factory_801867B0 = {
    0,
    19284,
    0,
    1,
    D_dryfield_factory_80182728,
    D_dryfield_factory_8018272C,
    D_dryfield_factory_801830BC,
    D_dryfield_factory_80182704,
    D_dryfield_factory_80183BC4,
};

TmdBone D_dryfield_factory_801867D4[1] = {
#include "assets/dryfield_factory_model_093E4_skeleton.inc"
};

u32 D_dryfield_factory_801867F8[1] = {
#include "assets/dryfield_factory_model_093E4_partVerts.inc"
};

SVECTOR D_dryfield_factory_801867FC[25] = {
#include "assets/dryfield_factory_model_093E4_verts.inc"
};

SVECTOR D_dryfield_factory_801868C4[28] = {
#include "assets/dryfield_factory_model_093E4_normals.inc"
};

u32 D_dryfield_factory_801869A4[139] = {
#include "assets/dryfield_factory_model_093E4_stream.inc"
};

TmdSource D_dryfield_factory_80186BD0 = {
    0,
    856,
    0,
    1,
    D_dryfield_factory_801867F8,
    D_dryfield_factory_801867FC,
    D_dryfield_factory_801868C4,
    D_dryfield_factory_801867D4,
    D_dryfield_factory_801869A4,
};

SVECTOR D_dryfield_factory_80186BF4[2] = {
#include "assets/dryfield_factory_collision_096A8_normals.inc"
};

SVECTOR D_dryfield_factory_80186C04[8] = {
#include "assets/dryfield_factory_collision_096A8_verts.inc"
};

WorldCollisionGridFace D_dryfield_factory_80186C44[2] = {
#include "assets/dryfield_factory_collision_096A8_faces.inc"
};

s16 D_dryfield_factory_80186C5C[4] = {
#include "assets/dryfield_factory_collision_096A8_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_factory_80186C5C[i])
s16* D_dryfield_factory_80186C64[1] = {
#include "assets/dryfield_factory_collision_096A8_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid gFactoryBarrierTemplate = { NULL, D_dryfield_factory_80186BF4, D_dryfield_factory_80186C04, D_dryfield_factory_80186C44, D_dryfield_factory_80186C64, -4464, -3949, 1, 1, 4000, 2 };

SVECTOR D_dryfield_factory_80186C8C[4] = {
#include "assets/dryfield_factory_collision_09778_normals.inc"
};

SVECTOR D_dryfield_factory_80186CAC[8] = {
#include "assets/dryfield_factory_collision_09778_verts.inc"
};

WorldCollisionGridFace D_dryfield_factory_80186CEC[4] = {
#include "assets/dryfield_factory_collision_09778_faces.inc"
};

s16 D_dryfield_factory_80186D1C[10] = {
#include "assets/dryfield_factory_collision_09778_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_factory_80186D1C[i])
s16* D_dryfield_factory_80186D30[2] = {
#include "assets/dryfield_factory_collision_09778_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid gFactoryLiftTemplate = { NULL, D_dryfield_factory_80186C8C, D_dryfield_factory_80186CAC, D_dryfield_factory_80186CEC, D_dryfield_factory_80186D30, 750, 2191, 1, 2, 4000, 4 };

SVECTOR D_dryfield_factory_80186D5C[4] = {
#include "assets/dryfield_factory_collision_09844_normals.inc"
};

SVECTOR D_dryfield_factory_80186D7C[8] = {
#include "assets/dryfield_factory_collision_09844_verts.inc"
};

WorldCollisionGridFace D_dryfield_factory_80186DBC[4] = {
#include "assets/dryfield_factory_collision_09844_faces.inc"
};

s16 D_dryfield_factory_80186DEC[8] = {
#include "assets/dryfield_factory_collision_09844_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_factory_80186DEC[i])
s16* D_dryfield_factory_80186DFC[2] = {
#include "assets/dryfield_factory_collision_09844_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid gFactoryLiftTurnedTemplate = { NULL, D_dryfield_factory_80186D5C, D_dryfield_factory_80186D7C, D_dryfield_factory_80186DBC, D_dryfield_factory_80186DFC, 750, 1950, 1, 2, 4000, 4 };

TaskDesc gFactoryDaySpawnTable[8] = {
    { { { TASK_BODY_NONE, 192 } }, factoryPowerScene, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, factoryLampScene, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, factoryCapScene, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, factoryWhiteoutScene, { .value = 0 } },
    { { { TASK_BODY_TMD, 192 } }, factoryLiftRun, { .model = &D_dryfield_factory_801867B0 } },
    { { { TASK_BODY_COORD, 192 } }, factoryBarrierCollision, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, factoryHatchScene, { .value = 0 } },
    { { { TASK_BODY_TMD, 192 } }, factoryHatchRun, { .model = &D_dryfield_factory_80186BD0 } },
};

TaskDesc gFactoryPromptDesc[1] = {
    { { { TASK_BODY_NONE, 192 } }, factoryPromptTask, { .value = 0 } },
};

TaskDesc gFactoryDayPanelDesc[1] = {
    { { { TASK_BODY_NONE, 192 } }, factoryPanelRun, { .value = 0 } },
};

FactoryControlMessageEntry gFactoryPanelMsgTable[2] = {
    { 5107, factoryPanelTrigger },
    { TASK_MESSAGE_TABLE_END, NULL },
};

OverlayHotspot gFactoryPanelHotspots[6] = {
    { -68, -63, 16, 16, 0, 1, 0 },
    { -27, -63, 16, 16, 1, 1, 0 },
    { 13, -63, 16, 16, 2, 1, 0 },
    { 46, -80, 34, 32, 3, 0, 0 },
    { -38, 0, 72, 48, 4, 0, 0 },
    { 0, 0, 0, 0, -1, 0, 0 },
};

SVECTOR D_dryfield_factory_80186EF8 = { 395, -1630, 846, 0 };

SVECTOR D_dryfield_factory_80186F00 = { 5910, -1308, 5649, 0 };

SVECTOR D_dryfield_factory_80186F08 = { 5910, -1404, 5649, 0 };

GpRoomObjRec D_dryfield_factory_80186F10[2] = {
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

GpViewCountRec D_dryfield_factory_80186F5C[2] = {
    { { .bytes = { 19, 0 } } },
    { { .bytes = { 19, 0 } } },
};

GpWarpRec D_dryfield_factory_80186F60[3] = {
    { { .words = { 3072, 5178, 0, 1454 } }, { 0, 0, 0, 0 }, { .words = { 1024, 4530, 0, 1200 } }, { 0, 0, 0, 0 }, 0x52170002, 0x52170001, 0, 2, 0, 474 },
    { { .words = { 1024, 642, 0, 7493 } }, { 0, 0, 0, 0 }, { .words = { 1024, 4530, 0, 1200 } }, { 0, 0, 0, 0 }, 0x52170004, 0x52170003, 0x52170005, 7, 0, 475 },
    { { .words = { 3072, 5445, 0, 6866 } }, { 0, 0, 0, 0 }, { .words = { 2048, 5420, 0, 6000 } }, { 0, 0, 0, 0 }, 0x52170014, 0x52170006, 0, 6, 2, 473 },
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

/// Per-frame effect: draws up to three glowing discs at fixed points in the
/// room. The draw set is selected by the stage-visit byte
/// `gGameSession->location.loc.view` taken as a bit index, and each group also gates on a
/// story flag, so a disc only appears on the visits and after the event that
/// the flag records.
void func_dryfield_factory_801825F0(Task* task)
{
    s32 state;

    state = 1 << gGameSession->location.loc.view;
    if (GameFlag_GetNibble(0x48) != 0 && (state & 0x15068) != 0) {
        glowDrawTintedDisc(&D_dryfield_factory_80186EF8, 0x100, 0x3660);
    }
    if (state & 0xF26C4) {
        if (GameFlag_GetNibble(0x4A) == 1) {
            glowDrawTintedDisc(&D_dryfield_factory_80186F00, 0x80, 0x5A00);
        } else if (GameFlag_GetNibble(0x4A) == 2) {
            glowDrawTintedDisc(&D_dryfield_factory_80186F08, 0x80, 0x50A0);
        }
    }
}
