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

/// The room script task's work block as the prompt-spawning state reads it:
/// `promptKind` is the display mode forwarded to `func_800D4E78`, read signed.
typedef struct RoomUtil21Work {
    /* 0x00 */ byte pad_0[0xE];
    /* 0x0E */ s8   promptKind;
} RoomUtil21Work;

extern TaskDesc D_dryfield_factory_80186E88[];
typedef struct {
    s32  id;
    void (*handler)(Task*);
} FactoryControlMessageEntry;
STATIC_ASSERT_SIZEOF(FactoryControlMessageEntry, 8);

extern FactoryControlMessageEntry D_dryfield_factory_80186EA0[2];
extern OverlayHotspot             D_dryfield_factory_80186EB0[];
extern SVECTOR                    D_dryfield_factory_80186EF8;
extern SVECTOR                    D_dryfield_factory_80186F00;
extern SVECTOR                    D_dryfield_factory_80186F08;

static void func_dryfield_factory_80180A4C(Task* task);
static void func_dryfield_factory_8018182C(Task* task);
static void func_dryfield_factory_80181938(Task* task);
static void func_dryfield_factory_8018196C(Task* task);
static void func_dryfield_factory_801819BC(Task* task);
static void func_dryfield_factory_80181A24(Task* task);
static void func_dryfield_factory_80181AB8(Task* task);

/// State handlers of the room's script task, run by
/// `func_dryfield_factory_8018169C`: set-up, prompt arming, the idle hotspot
/// scan, prompt spawning, the prompt state, the exit and the wait for the
/// message handler's trigger.
static const TaskFuncTable7 D_dryfield_factory_8017D678 = {
    {
        func_dryfield_factory_8018182C,
        func_dryfield_factory_80181938,
        func_dryfield_factory_80180A4C,
        func_dryfield_factory_8018196C,
        func_dryfield_factory_801819BC,
        func_dryfield_factory_80181A24,
        func_dryfield_factory_80181AB8,
    },
};

void func_dryfield_factory_8018169C(Task*);
void func_dryfield_factory_80181718(Task*);
void func_dryfield_factory_80181768(Task*);

TaskDesc D_dryfield_factory_801826B0 = { 0, 32, func_dryfield_factory_8017D85C, { .model = NULL } };

TaskDesc D_dryfield_factory_801826BC[2] = {
    { 0, 32, func_dryfield_factory_8017DD00, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

GpMsgEntry D_dryfield_factory_801826D4[6] = {
    { 5102, func_dryfield_factory_8017DB08 },
    { 5105, func_dryfield_factory_8017DDA0 },
    { 5104, func_dryfield_factory_8017DDA8 },
    { 5106, func_dryfield_factory_8017DEA8 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_factory_8017DF14 },
    { 0x7FFFFFFF, NULL },
};

TmdBone D_dryfield_factory_80182704[1] = {
#include "assets/dryfield_factory_model_091F0_skeleton.inc"
};

u32 D_dryfield_factory_80182728[1] = {
#include "assets/dryfield_factory_model_091F0_partVerts.inc"
};

SVECTOR D_dryfield_factory_8018272C[306] = {
#include "assets/dryfield_factory_model_091F0_verts.inc"
};

SVECTOR D_dryfield_factory_801830BC[353] = {
#include "assets/dryfield_factory_model_091F0_normals.inc"
};

u32 D_dryfield_factory_80183BC4[2811] = {
#include "assets/dryfield_factory_model_091F0_stream.inc"
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
#include "assets/dryfield_factory_model_09610_skeleton.inc"
};

u32 D_dryfield_factory_801867F8[1] = {
#include "assets/dryfield_factory_model_09610_partVerts.inc"
};

SVECTOR D_dryfield_factory_801867FC[25] = {
#include "assets/dryfield_factory_model_09610_verts.inc"
};

SVECTOR D_dryfield_factory_801868C4[28] = {
#include "assets/dryfield_factory_model_09610_normals.inc"
};

u32 D_dryfield_factory_801869A4[139] = {
#include "assets/dryfield_factory_model_09610_stream.inc"
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

GpGridFace D_dryfield_factory_80186C44[2] = {
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

GpGridParams D_dryfield_factory_80186C68 = { NULL, D_dryfield_factory_80186BF4, D_dryfield_factory_80186C04, D_dryfield_factory_80186C44, D_dryfield_factory_80186C64, -4464, -3949, 1, 1, 4000, 2 };

SVECTOR D_dryfield_factory_80186C8C[4] = {
#include "assets/dryfield_factory_collision_09778_normals.inc"
};

SVECTOR D_dryfield_factory_80186CAC[8] = {
#include "assets/dryfield_factory_collision_09778_verts.inc"
};

GpGridFace D_dryfield_factory_80186CEC[4] = {
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

GpGridParams D_dryfield_factory_80186D38 = { NULL, D_dryfield_factory_80186C8C, D_dryfield_factory_80186CAC, D_dryfield_factory_80186CEC, D_dryfield_factory_80186D30, 750, 2191, 1, 2, 4000, 4 };

SVECTOR D_dryfield_factory_80186D5C[4] = {
#include "assets/dryfield_factory_collision_09844_normals.inc"
};

SVECTOR D_dryfield_factory_80186D7C[8] = {
#include "assets/dryfield_factory_collision_09844_verts.inc"
};

GpGridFace D_dryfield_factory_80186DBC[4] = {
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

GpGridParams D_dryfield_factory_80186E04 = { NULL, D_dryfield_factory_80186D5C, D_dryfield_factory_80186D7C, D_dryfield_factory_80186DBC, D_dryfield_factory_80186DFC, 750, 1950, 1, 2, 4000, 4 };

TaskDesc D_dryfield_factory_80186E28[8] = {
    { 0, 192, func_dryfield_factory_8017FC18, { .model = NULL } },
    { 0, 192, func_dryfield_factory_801807DC, { .model = NULL } },
    { 0, 192, func_dryfield_factory_80180920, { .model = NULL } },
    { 0, 192, func_dryfield_factory_8017FDDC, { .model = NULL } },
    { TASK_BODY_TMD, 192, func_dryfield_factory_8018072C, { .model = &D_dryfield_factory_801867B0 } },
    { TASK_BODY_COORD, 192, func_dryfield_factory_8018001C, { .model = NULL } },
    { 0, 192, func_dryfield_factory_80180964, { .model = NULL } },
    { TASK_BODY_TMD, 192, func_dryfield_factory_80180784, { .model = &D_dryfield_factory_80186BD0 } },
};

TaskDesc D_dryfield_factory_80186E88[1] = {
    { 0, 192, func_dryfield_factory_80181718, { .model = NULL } },
};

TaskDesc D_dryfield_factory_80186E94[1] = {
    { 0, 192, func_dryfield_factory_8018169C, { .model = NULL } },
};

FactoryControlMessageEntry D_dryfield_factory_80186EA0[2] = {
    { 5107, func_dryfield_factory_80181768 },
    { 0x7FFFFFFF, NULL },
};

OverlayHotspot D_dryfield_factory_80186EB0[6] = {
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
    { &D_dryfield_factory_80187BF8, D_dryfield_factory_80189694, D_dryfield_factory_80189ABC, NULL },
    { &D_dryfield_factory_80187BF8, D_dryfield_factory_80189694, D_dryfield_factory_80189ABC, NULL },
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
    D_8010CAF8,
    D_dryfield_factory_80186F30,
};

GpRoomCoordRec D_dryfield_factory_80186F4C[2] = {
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

static void func_dryfield_factory_80180DE8(Task* task, s16 step);

/// Idle state of the room's script task. It counts down the delay the prompt
/// state armed and, once that is spent and no cap is playing, hit-tests the
/// room's hotspots under the cursor: a confirmed hit copies the hotspot's `id`
/// and `promptKind` into the work block and advances to state 3, and the
/// cancel button leaves for state 5.
static void func_dryfield_factory_80180A4C(Task* task)
{
    RoomActionPrompt*       prompt = D_80114D28;
    OverlayHotspot*         hs     = D_dryfield_factory_80186EB0;
    NightFactoryScriptWork* st     = (NightFactoryScriptWork*)task->work;

    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (st->field_8 != 0) {
        st->field_8 = st->field_8 - 1;
    }
    if ((Gp_CapBusy() != 0) || (st->field_8 != 0)) {
        prompt->mode     = 0;
        prompt->targetId = 0;
        return;
    }
    prompt->targetId = 0x80;
    if (actionPromptHitTest(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
        if (prompt->buttons.slots[0].state == 2) {
            for (; hs->id != -1; hs++) {
                if (hs->hit != 0) {
                    prompt->mode     = 0;
                    prompt->targetId = 0;
                    st->field_C      = hs->id;
                    st->field_E      = hs->promptKind;
                    task->state      = 3;
                    return;
                }
            }
        }
    } else {
        prompt->mode = 1;
    }
    if (prompt->buttons.slots[1].state == 2) {
        task->state = 5;
    }
}

#include "../../shared/action_prompt_outline_rect.inc.c"

static void func_dryfield_factory_80180DE8(Task* task, s16 step)
{
    s32 id;
    s32 state;

    if (GameFlag_GetNibble(0x48) != 0) {
        switch (step) {
            case 0:
                id = 0x53170000;
                if (gGameSession->location.loc.stage == 2) {
                    id = 0x52170000;
                }
                SndEvt_EnqueueType6(id | 9, 0, 0);
                if (!(GameFlag_GetNibble(0x49) & 2)) {
                    GameFlag_SetNibble(0x49, GameFlag_GetNibble(0x49) | 2);
                    if (GameFlag_GetNibble(0x47) == 0) {
                        Mc_SaveData[0].state.at4.loc.view = 0x12;
                    } else {
                        Mc_SaveData[0].state.at4.loc.view = 0x13;
                    }
                    state = 6;
                } else {
                    Gp_StartCapSlot(8, 0, 0);
                    state = 2;
                }
                task->state = state;
                break;
            case 1:
                id = 0x53170000;
                if (gGameSession->location.loc.stage == 2) {
                    id = 0x52170000;
                }
                SndEvt_EnqueueType6(id | 9, 0, 0);
                if (GameFlag_GetNibble(0x49) & 2) {
                    GameFlag_SetNibble(0x49, GameFlag_GetNibble(0x49) & ~2);
                    if (GameFlag_GetNibble(0x47) == 0) {
                        Mc_SaveData[0].state.at4.loc.view = 0x12;
                    } else {
                        Mc_SaveData[0].state.at4.loc.view = 0x13;
                    }
                    state = 6;
                } else {
                    Gp_StartCapSlot(9, 0, 0);
                    state = 2;
                }
                task->state = state;
                break;
            case 2:
                id = 0x53170000;
                if (gGameSession->location.loc.stage == 2) {
                    id = 0x52170000;
                }
                SndEvt_EnqueueType6(id | 9, 0, 0);
                GameFlag_SetNibble(0x49, GameFlag_GetNibble(0x49) ^ 1);
                if (GameFlag_GetNibble(0x47) == 0) {
                    Mc_SaveData[0].state.at4.loc.view = 0x12;
                } else {
                    Mc_SaveData[0].state.at4.loc.view = 0x13;
                }
                state       = 6;
                task->state = state;
                break;
            case 3:
                Gp_StartCapSlot(6, 0, 1);
                state       = 2;
                task->state = state;
                break;
            case 4:
                Gp_StartCapSlot(7, 0, 0);
                state       = 2;
                task->state = state;
                break;
        }
    } else {
        switch (step) {
            case 0:
                id = 0x53170000;
                if (gGameSession->location.loc.stage == 2) {
                    id = 0x52170000;
                }
                SndEvt_EnqueueType6(id | 9, 0, 0);
                Gp_StartCapSlot(8, 0, 0);
                break;
            case 1:
                id = 0x53170000;
                if (gGameSession->location.loc.stage == 2) {
                    id = 0x52170000;
                }
                SndEvt_EnqueueType6(id | 9, 0, 0);
                Gp_StartCapSlot(9, 0, 0);
                break;
            case 2:
                id = 0x53170000;
                if (gGameSession->location.loc.stage == 2) {
                    id = 0x52170000;
                }
                SndEvt_EnqueueType6(id | 9, 0, 0);
                Gp_StartCapSlot(0xA, 0, 0);
                break;
            case 3:
                Gp_StartCapSlot(6, 0, 0);
                break;
            case 4:
                Gp_StartCapSlot(7, 0, 0);
                break;
        }
        task->state = 2;
    }
}

#include "../../shared/action_prompt_move_cursors.inc.c"

#include "../../shared/action_prompt_draw_cursor.inc.c"

/// Sets the skip-link byte on the second sprite command of view 9 for the
/// current room. `arg0` zero skips OT-linking (`field_4` = 1); non-zero draws
/// it. No-op unless `GameSession.location.loc.stage` is 2.
void func_dryfield_factory_80181620(s32 arg0)
{
    GameSession*     g;
    GameLocationKey* sess;
    SpriteBatch*     batches;

    g    = gGameSession;
    sess = &g->location.loc;
    if (sess->stage == 2) {
        batches = Gp_SprtTables[sess->stage - 1][g->spriteVariant - 1].field_0[sess->area - 1][8].field_4;
        if (!(arg0 & 0xFF)) {
            batches[1].hidden = 1;
            return;
        }
        batches[1].hidden = 0;
    }
}

/// Runs the room script task's current state. The seven handlers are copied
/// onto the stack first, so the call goes through a local table rather than
/// through `.rodata`.
void func_dryfield_factory_8018169C(Task* task)
{
    TaskFuncTable7 sp;

    sp = D_dryfield_factory_8017D678;
    sp.funcs[task->state](task);
}

void func_dryfield_factory_80181718(Task* task)
{
    TaskFunc states[2] = { actionPromptReset, actionPromptMoveCursors };

    states[task->state](task);
}

/// Message 0x13F3 handler of the room's script task: raises the one-shot
/// trigger `field_A` in its work block, which the script's wait state consumes.
void func_dryfield_factory_80181768(Task* task)
{
    ((NightFactoryScriptWork*)task->work)->field_A = 1;
}

#include "../../shared/action_prompt_hit_test.inc.c"

/// State 0 of the room's script task: allocates its work block, spawns the
/// child task, publishes the script's message table, picks the global mode
/// byte from game flag 0x48, advances, and clears the hotspot hits while
/// holding the HUD for the cutscene.
static void func_dryfield_factory_8018182C(Task* task)
{
    NightFactoryScriptWork* work;
    OverlayHotspot*         hs;

    work = memCalloc(0x10, 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2.pointer = Task_SpawnFromTable(D_dryfield_factory_80186E88, 0, 1, 0);
    task->work              = work;
    task->msgTable          = D_dryfield_factory_80186EA0;
    if (GameFlag_GetNibble(0x48) == 0) {
        Mc_SaveData[0].state.at4.loc.view = 0xC;
    } else {
        Mc_SaveData[0].state.at4.loc.view = 5;
    }
    task->state++;
    Display_AcquireRef();
    for (hs = D_dryfield_factory_80186EB0; hs->id != -1; hs++) {
        hs->hit = 0;
    }
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    gGameSession->eventState   = 1;
    work->field_8              = 0;
}

/// Arms the action prompt for a hotspot and steps the caller's script on one
/// state: marks the prompt as highlighted (`mode` 1) for the fixed target id
/// 0x80 and resets the on-screen position, which `func_800D4E78` fills in again
/// when the prompt is actually spawned.
static void func_dryfield_factory_80181938(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;

    prompt->targetId    = 0x80;
    prompt->mode        = 1;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}

/// Spawns the action prompt for the script's current step: clears the prompt's
/// highlight state, then re-spawns it at the coordinates the gameplay side left
/// in `D_80114D28` with the display mode this state picked.
static void func_dryfield_factory_8018196C(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;
    RoomUtil21Work*   work   = (RoomUtil21Work*)task->work;

    prompt->mode     = 0;
    prompt->targetId = 0;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}

/// Prompt state of the room's script task: clears the cursor highlight and,
/// while `func_800D4EC0` still reports a prompt on screen, runs the cap step
/// the work block names; otherwise it returns to the idle state 2. Either way
/// it re-arms the idle state's delay.
static void func_dryfield_factory_801819BC(Task* task)
{
    NightFactoryScriptWork* work = (NightFactoryScriptWork*)task->work;

    D_80114D28[0].mode     = 0;
    D_80114D28[0].targetId = 0;
    if (func_800D4EC0() != 0) {
        func_dryfield_factory_80180DE8(task, work->field_C);
    } else {
        task->state = 2;
    }
    work->field_8 = 0xA;
}

static void func_dryfield_factory_80181A24(Task* arg0)
{
    D_80114D08 = 0xA;
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    Gp_MsgAlly3F3(1);
    Display_ReleaseRef();
    gGameSession->eventState          = 0;
    gGameSession->hideHud             = 0;
    gGameSession->cutsceneHold        = 0;
    Mc_SaveData[0].state.at4.loc.view = 3;
    /* Without the barrier GCC fills taskKill's delay slot with the byte store. */
    taskKill((Task*)arg0->spawnArg2.pointer);
    Task_RequestKill(arg0, 0);
}

/// Wait state of the room's script task: once the one-shot trigger `field_A`
/// has been raised by the script's message handler, picks the global mode
/// byte from game flag 0x48, re-arms the idle delay, consumes the trigger and
/// returns the script to its idle state 2.
static void func_dryfield_factory_80181AB8(Task* task)
{
    RoomActionPrompt*       prompt;
    NightFactoryScriptWork* work;

    prompt           = D_80114D28;
    work             = (NightFactoryScriptWork*)task->work;
    prompt->targetId = 0;
    prompt->mode     = 0;
    if (work->field_A != 0) {
        if (GameFlag_GetNibble(0x48) == 0) {
            Mc_SaveData[0].state.at4.loc.view = 0xC;
        } else {
            Mc_SaveData[0].state.at4.loc.view = 5;
        }
        work->field_8 = 0xA;
        work->field_A = 0;
        task->state   = 2;
    }
}

/// Sets the skip-link byte on the second sprite command of view 11 for the
/// current room. `arg0` zero skips OT-linking (`field_4` = 1); non-zero draws
/// it. No-op unless `GameSession.location.loc.stage` is 2.
void func_dryfield_factory_80181B38(s32 arg0)
{
    GameSession*     g;
    GameLocationKey* sess;
    SpriteBatch*     batches;

    g    = gGameSession;
    sess = &g->location.loc;
    if (sess->stage == 2) {
        batches = Gp_SprtTables[sess->stage - 1][g->spriteVariant - 1].field_0[sess->area - 1][10].field_4;
        if (!(arg0 & 0xFF)) {
            batches[1].hidden = 1;
            return;
        }
        batches[1].hidden = 0;
    }
}

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
