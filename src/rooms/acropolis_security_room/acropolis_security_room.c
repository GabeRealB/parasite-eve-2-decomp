#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/stdio.h>

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017d830.h"

/// 0xA work block of the security-monitor task, hung off the `Task::work`
/// slot (0x1C) -- that slot is *not* a `TaskIdMap` here, it is the
/// `memCalloc(0xA)` block `func_acropolis_security_room_8017D9DC` allocates.
/// Reach it with `(AsrMonitorWork*)task->work`.
///
/// `cameraId` is the camera the monitor is currently showing, seeded from the
/// `GameFlag_GetNibble(0x2A)` lookup table and offset by 0x7F before being
/// handed to the panel drawer `func_acropolis_security_room_8017E0C4`.
/// `blinkTimer` is the cursor blink counter the overlay drawer
/// `func_acropolis_security_room_8017E37C` advances, `selection` the row the
/// player has highlighted, and `promptKind` the display mode forwarded to
/// `func_800D4E78` when the action prompt is spawned.
typedef struct AsrMonitorWork {
    /* 0x00 */ u16  cameraId;
    /* 0x02 */ s16  blinkTimer;
    /* 0x04 */ s16  selection;
    /* 0x06 */ s8   promptKind;
    /* 0x07 */ s8   field_7;
    /* 0x08 */ s8   field_8;
    /* 0x09 */ byte pad_9[0x1];
} AsrMonitorWork;
STATIC_ASSERT_SIZEOF(AsrMonitorWork, 0xA);

/// Scratch state of the security-room cap script, stored at `Task::work`.
///
/// `func_acropolis_security_room_8017FA18` -- state 0 of the family whose
/// handler table is `D_acropolis_security_room_8017D63C` -- allocates it with
/// `memCalloc(0x10, 0)` and stores it straight into the `Task::work` slot,
/// so the size below is the allocation and not a guess; the same function
/// parks the family's `GpMsgEntry[]` in `Task::msgTable`. The overlay's other
/// two allocators (`memCalloc(0xA)` in `func_acropolis_security_room_8017D9DC`
/// and `memCalloc(4)` in `func_acropolis_security_room_80180368`) belong to
/// other task families and to a different block.
typedef struct AcropolisSecurityRoomState {
    /* 0x0 */ s32   field_0;    // sub-step picked by the previous cap event
    /* 0x4 */ Task* child;      // task this state spawned, polled by Task_PollKill
    /* 0x8 */ u16   frames;     // frames the current state has been running
    /* 0xA */ s16   variant;    // 0 = the pair-4 cap script, 1 = the pair-3 one
    /* 0xC */ s8    promptKind; // display mode forwarded to `func_800D4E78`
    /* 0xD */ byte  pad_D[0x3];
} AcropolisSecurityRoomState;
STATIC_ASSERT_SIZEOF(AcropolisSecurityRoomState, 0x10);

/// Scratch state of the security-room ambience task, stored at `Task::work`.
/// `func_acropolis_security_room_80180368` allocates it with `memCalloc(4, 0)`,
/// so the size below is the allocation and not a guess.
typedef struct {
    /* 0x0 */ u16  fadeStarted; // the looping ambience has already been faded out
    /* 0x2 */ byte pad_2[0x2];
} AsrAmbienceState;

/// One frame of the 128x128 textured quad `func_acropolis_security_room_80180E34`
/// draws: the sprite is centred on (`x`, `y`) with texture page 0xAB, the CLUT
/// `clut << 6` and its top-left texel at (`u`, `v`), the other three corners
/// being that texel plus 0x7F on each axis. The table
/// (`D_acropolis_security_room_80183970`) has four entries and is indexed by
/// the low two bits of the drawing task's `Task::spawnArg1`.
typedef struct AsrSpriteFrame {
    /* 0x0 */ u16  clut;
    /* 0x2 */ u8   u;
    /* 0x3 */ byte pad_3[1];
    /* 0x4 */ u8   v;
    /* 0x5 */ byte pad_5[1];
    /* 0x6 */ u16  x;
    /* 0x8 */ u16  y;
} AsrSpriteFrame;
STATIC_ASSERT_SIZEOF(AsrSpriteFrame, 0xA);

/// 0x14-byte scratch block `func_acropolis_security_room_80180A78` takes from
/// `G_SCRATCH_HEAD` while it draws the security laser. `a` and `b` are the two
/// endpoints of the beam in the emitter's local frame; each is rotated by the
/// emitter's `GpCoord::workm` and then biased by that matrix's
/// translation, so both end up in world space. `otz` receives `SZ3 >> 2` from
/// the `RTPS` of `a` and doubles as the OT slot selector.
typedef struct AsrBeamScratch {
    /* 0x00 */ s32     otz;
    /* 0x04 */ SVECTOR a;
    /* 0x0C */ SVECTOR b;
} AsrBeamScratch;
STATIC_ASSERT_SIZEOF(AsrBeamScratch, 0x14);

/// Projection and radius scratch for the security-room flash effect.
typedef struct AsrFlashScratch {
    /* 0x00 */ s32     otz;
    /* 0x04 */ s32     step;
    /* 0x08 */ SVECTOR v;
    /* 0x10 */ s16     x;
    /* 0x12 */ s16     y;
} AsrFlashScratch;
STATIC_ASSERT_SIZEOF(AsrFlashScratch, 0x14);

/// Local X/Z corner coordinates, scaled by the effect's field_24.
typedef struct AsrQuadCorner {
    /* 0x00 */ s16 x;
    /* 0x02 */ s16 z;
} AsrQuadCorner;
STATIC_ASSERT_SIZEOF(AsrQuadCorner, 0x4);

extern s16 D_80114D08;

/// The tasks `func_acropolis_security_room_8017D77C` and
/// `func_acropolis_security_room_8017D834` spawn and poll until they end;
/// message 0x13F1 is forwarded to the second while it is alive.
extern Task* D_acropolis_security_room_801855A8;
extern Task* D_acropolis_security_room_801855AC;

/// The whole-unit world displacement the last `func_acropolis_security_room_80181C84`
/// call produced.
extern SVECTOR D_acropolis_security_room_801855B0;

/// The room's message table, parked in `Task::msgTable` by its message task.
extern GpMsgEntry D_acropolis_security_room_801825DC[];
extern TaskDesc   D_acropolis_security_room_80182618;
extern TaskDesc   D_acropolis_security_room_8018263C;

/// The security monitor's own hotspot table, hit-tested by
/// `func_acropolis_security_room_8017ECB4`.
extern OverlayHotspot D_acropolis_security_room_80182648[];

/// The five camera ids the security monitor can display, in the order the
/// `GameFlag_GetNibble(0x2A)` nibble indexes them.
extern s16 D_acropolis_security_room_801826B4[];

/// The single-entry `TaskDesc` table the script spawns its child task from:
/// `func_acropolis_security_room_8017F9C8`.
extern TaskDesc D_acropolis_security_room_801826C0[];
/// The script's message table, parked in `Task::msgTable`.
extern GpMsgEntry D_acropolis_security_room_801826CC[];

/// The script's hotspot table, terminated by an entry whose `id` is -1.
extern OverlayHotspot D_acropolis_security_room_801826DC[];

/// The two `TaskDesc`s this room's script spawns from: index 0 is
/// `func_acropolis_security_room_80180368`, index 1 is
/// `func_acropolis_security_room_801804CC`.
extern TaskDesc D_acropolis_security_room_80182700[];

/// The 0x100-entry RGB555 palette every monitor-screen CLUT is blended
/// towards: the "off" colours of the four security-camera feeds.
extern u16 D_acropolis_security_room_80182718[];
/// The four lit palettes, one per camera feed, blended against
/// `D_acropolis_security_room_80182718` by the feed's own brightness.
extern u16 D_acropolis_security_room_80182918[];
extern u16 D_acropolis_security_room_80182B18[];
extern u16 D_acropolis_security_room_80182D18[];
extern u16 D_acropolis_security_room_80182F18[];
/// The four blend results, uploaded to VRAM by
/// `D_acropolis_security_room_80183918`.
extern u16 D_acropolis_security_room_80183118[];
extern u16 D_acropolis_security_room_80183318[];
extern u16 D_acropolis_security_room_80183518[];
extern u16 D_acropolis_security_room_80183718[];
/// The upload records for the four blended CLUTs above.
extern GpImgRec D_acropolis_security_room_80183918[];
/// Camera-lit bitmask for each value of `GameFlag_GetNibble(9)`; bit N is set
/// while feed N is showing something.
extern u16 D_acropolis_security_room_80183968[];

/// The four sprite frames `func_acropolis_security_room_80180E34` picks from.
extern AsrSpriteFrame D_acropolis_security_room_80183970[];

/// Spawn position and effect id of the flash each newly lit feed plays,
/// indexed by feed.
extern SVECTOR D_acropolis_security_room_80183998[];
extern s16     D_acropolis_security_room_801839B8[];

extern AsrQuadCorner D_acropolis_security_room_801839C0[];

/// 0xFF-terminated area-record lists applied as the script ends.
extern GpAreaApplyRec D_acropolis_security_room_80184F50[];
extern GpAreaApplyRec D_acropolis_security_room_80184F78[];
extern GpAreaApplyRec D_acropolis_security_room_80184F7C[];
extern GpAreaApplyRec D_acropolis_security_room_80184F80[];

void func_acropolis_security_room_8017D930(Task* task);
void func_acropolis_security_room_8017D97C(Task* task);
void func_acropolis_security_room_8017D9DC(Task* task);
void func_acropolis_security_room_8017DB30(Task* task);
void func_acropolis_security_room_8017DC7C(Task* task);
void func_acropolis_security_room_8017E0C4(s16 id);
void func_acropolis_security_room_8017E37C(Task* task);
void func_acropolis_security_room_8017E490(Task* task);
void func_acropolis_security_room_8017E8F0(s32 x, s32 y, s32 variant);
void func_acropolis_security_room_8017EA28(Task* task);
void func_acropolis_security_room_8017EA5C(Task* task);
void func_acropolis_security_room_8017EADC(Task* task);
void func_acropolis_security_room_8017EB9C(Task* task);
s32  func_acropolis_security_room_8017ECB4(OverlayHotspot* table, s16 x, s16 y);
void func_acropolis_security_room_8017EDE4(Task* task);
void func_acropolis_security_room_8017EE44(Task* task);
void func_acropolis_security_room_8017F480(Task* task);
void func_acropolis_security_room_8017F8E0(s32 x, s32 y, s32 variant);
void func_acropolis_security_room_8017FA18(Task* task);
void func_acropolis_security_room_8017FB20(Task* task);
void func_acropolis_security_room_8017FB54(Task* task);
void func_acropolis_security_room_8017FBA4(Task* task);
void func_acropolis_security_room_8017FC30(Task* task);
s32  func_acropolis_security_room_8017FCB0(OverlayHotspot* table, s16 x, s16 y);
void func_acropolis_security_room_8017FD64(s32 flags);
void func_acropolis_security_room_8017FE6C(Task* task);
void func_acropolis_security_room_8017FF0C(Task* task);
void func_acropolis_security_room_8017FF84(Task* task);
void func_acropolis_security_room_8017FFD0(Task* task);
void func_acropolis_security_room_80180010(Task* task);
void func_acropolis_security_room_80180030(Task* task);
void func_acropolis_security_room_801800A4(Task* task);
void func_acropolis_security_room_8018014C(Task* task);
void func_acropolis_security_room_801801C4(Task* task);
void func_acropolis_security_room_80180218(Task* task);
void func_acropolis_security_room_80180308(Task* task);
void func_acropolis_security_room_80180A78(Task* task);

/// Message 0x13EE handler: copies the incoming location record onto the
/// outgoing one and answers 1.
s32 func_acropolis_security_room_8017D6AC(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    return 1;
}

/// Message 0x13F1 handler: forwards the message unchanged to the task
/// `func_acropolis_security_room_8017D834` spawns and keeps in
/// `D_acropolis_security_room_801855AC`, answering 0 while it is not alive.
s32 func_acropolis_security_room_8017D6D4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    Task* target;
    s32   ret;

    target = D_acropolis_security_room_801855AC;
    if (target == NULL) {
        ret = 0;
    } else {
        ret = Gp_DispatchMsg(target, msgId, arg2, arg3);
    }
    return ret;
}

s32 func_acropolis_security_room_8017D708(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 2) {
        Task_SpawnFromTable(&D_acropolis_security_room_80182618, 1, 0, 0);
    }
    return 0;
}

void func_acropolis_security_room_8017D740(Task* arg0, s32 arg1, GpMsg13EF* arg2)
{
    if (arg2->field_2 == 0) {
        Task_SpawnFromTable(&D_acropolis_security_room_80182618, 0, 0, 0);
    }
}
/// State table of the room's message task: register the room's message table,
/// idle, then kill the task.
const TaskFuncTable3 D_acropolis_security_room_8017D5C4 = { {
    func_acropolis_security_room_8017D930,
    func_acropolis_security_room_8017D97C,
    taskKill,
} };

void func_acropolis_security_room_8017D77C(Task* arg0)
{
    s32 sp10;
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            printf("monitor\n");
            D_acropolis_security_room_801855A8 = Task_Spawn(2, 9, 0, 0);
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            arg0->state = arg0->state + 1;
            return;
        case 1:
            if (Task_PollKill(D_acropolis_security_room_801855A8, &sp10) != 0) {
                Gp_MsgPlayerWeapon(1);
                Gp_MsgPlayer3F3(1);
                taskKill(arg0);
            }
            return;
    }
}

/// The debug line `func_acropolis_security_room_8017D834` prints. The two bytes
/// after its terminator are non-zero in the ROM (0x40, 0x11), so the array is
/// declared at 16 bytes to carry them.
static const char PowerSupplyMsg[16] = "power supply\n\0@\021";

void func_acropolis_security_room_8017D834(Task* arg0)
{
    s32 sp10;
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            printf(PowerSupplyMsg);
            D_acropolis_security_room_801855AC = Task_Spawn(2, 0xA, 0, 0);
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(2);
            arg0->state = arg0->state + 1;
            return;
        case 1:
            if (Task_PollKill(D_acropolis_security_room_801855AC, &sp10) != 0) {
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F3, 1, 0);
                Gp_MsgPlayerWeapon(1);
                Gp_MsgPlayer3F3(1);
                D_acropolis_security_room_801855AC = NULL;
                taskKill(arg0);
            }
            return;
    }
}

void func_acropolis_security_room_8017D930(Task* arg0)
{
    arg0->msgTable = D_acropolis_security_room_801825DC;
    Game_SetPtrSlot(arg0, 7);
    arg0->state                        = arg0->state + 1;
    D_acropolis_security_room_801855AC = NULL;
}

void func_acropolis_security_room_8017D97C(Task* task)
{
}

/// States of the security-monitor task, dispatched by
/// `func_acropolis_security_room_8017ED68`: set up the work block, run the
/// camera list, redraw the panel, confirm a camera, and leave the monitor.
const TaskFuncTable7 D_acropolis_security_room_8017D5EC = { {
    func_acropolis_security_room_8017D9DC,
    func_acropolis_security_room_8017EA28,
    func_acropolis_security_room_8017DB30,
    func_acropolis_security_room_8017EA5C,
    func_acropolis_security_room_8017DC7C,
    func_acropolis_security_room_8017EADC,
    func_acropolis_security_room_8017EB9C,
} };

/// Runs the room's message task's current state through a stack copy of its
/// three-entry state table.
void func_acropolis_security_room_8017D984(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_security_room_8017D5C4;
    sp.funcs[task->state](task);
}

/// Entry state of the security-monitor task: allocates the `AsrMonitorWork`
/// block into the `Task::work` slot, spawns the monitor's companion task,
/// seeds `cameraId` from the `GameFlag_GetNibble(0x2A)` camera table, and picks
/// the next state from the `GameFlag_GetNibble(1)` progress nibble (state+1 and
/// prompt kind 8 before chapter 3, state 6 and prompt kind 5 after). Finally it
/// clears every hotspot's `hit` flag so the first hit test starts clean.
void func_acropolis_security_room_8017D9DC(Task* task)
{
    AsrMonitorWork* work;
    OverlayHotspot* hs;
    s16             flag;
    s32             state;
    s16             stateElse;

    work = (AsrMonitorWork*)memCalloc(sizeof(AsrMonitorWork), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2  = Task_SpawnFromTable(&D_acropolis_security_room_8018263C, 0, 1, 0);
    task->work       = (TaskIdMap*)work;
    work->blinkTimer = 0;
    stateElse        = 6;
    flag             = GameFlag_GetNibble(0x2A);
    if ((u16)flag < 5) {
        work->cameraId = D_acropolis_security_room_801826B4[flag];
    } else {
        work->cameraId = D_acropolis_security_room_801826B4[0];
    }
    if (GameFlag_GetNibble(1) < 3) {
        Mc_SaveData.at4.loc.view = 8;
        /* Without this the scheduler hoists the `task->state` load above the
           `Mc_SaveData.at4.loc.view` byte store to fill its load-delay slot. */
        SOFT_BARRIER();
        state = task->state;
        state++;
    } else {
        Mc_SaveData.at4.loc.view = 5;
        state                    = stateElse;
    }
    task->state = state;
    Display_AcquireRef();
    gGameSession->hideHud      = 1;
    gGameSession->cutsceneHold = 1;
    gGameSession->eventState   = 1;
    hs                         = D_acropolis_security_room_80182648;
    if (hs->id != -1) {
        do {
            hs->hit = 0;
            hs++;
        } while (hs->id != -1);
    }
}

/// Runs the hotspot-hit state of the security monitor: redraws the panel and
/// cursor, then hit-tests the action cursor against the room's hotspot table.
/// A miss leaves the prompt highlighted (`mode` 1); a hit with the prompt
/// confirmed (`buttons[0].state` 2) scans the table for the raised entry and hands its
/// `id` / `promptKind` to the work block, advancing to state 3. Otherwise the
/// task advances to state 5 once the prompt has been dismissed.
void func_acropolis_security_room_8017DB30(Task* task)
{
    AsrMonitorWork*   work;
    OverlayHotspot*   hs;
    RoomActionPrompt* prompt;

    hs     = D_acropolis_security_room_80182648;
    prompt = &D_80114D28;
    work   = (AsrMonitorWork*)task->work;
    func_acropolis_security_room_8017E0C4(work->cameraId - 0x7F);
    func_acropolis_security_room_8017E37C(task);
    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (Gp_CapBusy() != 0) {
        prompt->mode     = 0;
        prompt->targetId = 0;
        return;
    }
    prompt->targetId = 0x80;
    if (func_acropolis_security_room_8017ECB4(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
        if ((prompt->buttons[0].state == 2) && (hs->id != -1)) {
            do {
                if (hs->hit != 0) {
                    prompt->mode     = 0;
                    prompt->targetId = 0;
                    work->selection  = hs->id;
                    work->promptKind = hs->promptKind;
                    task->state      = 3;
                    return;
                }
                hs++;
            } while (hs->id != -1);
        }
    } else {
        prompt->mode = 1;
    }
    if (prompt->buttons[1].state == 2) {
        task->state = 5;
    }
}

/// Runs the camera-list state of the security monitor: mirrors the highlighted
/// row into `Mc_SaveData.at4.loc.view` (with a click), scrolls the panel by a page
/// when the row is one of the two 0x8000/0x8001 scroll commands, and fires the
/// two one-shot cap sequences the room gates on the `0xA` game-flag nibble.
/// Then redraws the panel plus cursor overlay and advances to state 2.
void func_acropolis_security_room_8017DC7C(Task* task)
{
    AsrMonitorWork* work;
    McSaveData*     save;
    s32             sfx;
    s16             sel;
    u16             usel;

    work                = (AsrMonitorWork*)task->work;
    D_80114D28.mode     = 0;
    D_80114D28.targetId = 0;
    if (func_800D4EC0() != 0) {
        sel = work->selection;
        if (sel >= 0) {
            save = &Mc_SaveData;
            if (save->at4.loc.view != sel) {
                save->at4.loc.view = work->selection;
                SndEvt_EnqueueType6(0x51060003, 0, 0);
                if ((work->selection == 0xA) && !(GameFlag_GetNibble(0xA) & 2)) {
                    work->field_7 = 1;
                }
            }
        }
        usel = work->selection;
        if (usel == 0x8000) {
            if (((s16)work->cameraId + 0x3E) < 0xFE) {
                work->cameraId += 0x3E;
                sfx             = 0x51060006;
                goto play;
            }
        } else if (usel == 0x8001) {
            if (((s16)work->cameraId - 0x3E) > 0) {
                work->cameraId -= 0x3E;
                sfx             = 0x51060007;
            play:
                SndEvt_EnqueueType6(sfx, 0, 0);
            }
        }
        if (((u8)Mc_SaveData.at4.loc.view == 0xB) && ((s16)work->cameraId != 4) && !(GameFlag_GetNibble(0xA) & 1)) {
            Gp_StartCapSlot(0xD, 0, 0);
            GameFlag_SetNibble(0xA, GameFlag_GetNibble(0xA) | 1);
        }
        if (((u8)Mc_SaveData.at4.loc.view == 0xA) && ((s16)work->cameraId != 4) && (work->field_7 != 0) &&
            (work->field_8 == 0) && (GameFlag_GetNibble(0x102) == 0)) {
            Gp_StartCapSlot(0xC, 0, 0);
            work->field_8 = 1;
        }
    }
    func_acropolis_security_room_8017E0C4(work->cameraId - 0x7F);
    func_acropolis_security_room_8017E37C(task);
    task->state = 2;
}

/// Outlines `rect` on screen in the colour (`r`, `g`, `b`) with four
/// unconnected flat lines -- top, right, bottom and left edge of the rectangle
/// spanning (`x`, `y`) to (`x + w`, `y + h`) -- each linked into
/// `gGpuCurrentOt[3]`. Nothing in the overlay calls it; it is the debug box
/// drawer for the hotspot rectangles.
void func_acropolis_security_room_8017DE80(RoomRect* rect, u8 r, u8 g, u8 b)
{
    LINE_F2* line;

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x;
    line->y0 = rect->y;
    line->x1 = rect->x + rect->w;
    line->y1 = rect->y;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 3, line);

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x + rect->w;
    line->y0 = rect->y;
    line->x1 = rect->x + rect->w;
    line->y1 = rect->y + rect->h;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 3, line);

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x + rect->w;
    line->y0 = rect->y + rect->h;
    line->x1 = rect->x;
    line->y1 = rect->y + rect->h;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 3, line);

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x;
    line->y0 = rect->y + rect->h;
    line->x1 = rect->x;
    line->y1 = rect->y;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 3, line);
}

/// Washes the security-monitor panel with the grey level `id` -- the work
/// block's `cameraId` biased by -0x7F -- as a semi-transparent `POLY_F4`
/// covering (-0x66, -0x5F) to (0x6C, 0x3C) in `gGpuCurrentOt[0xC]`, followed by
/// the drawing-mode packet that restores the panel's texture page. A negative
/// `id` uses its magnitude and the other semi-transparency rate (0xE100004A
/// rather than 0xE100002A), which is what makes the "no signal" panel read
/// differently from a live camera. The strip below the panel (y 0x3C to 0x38)
/// is then blacked out with an opaque quad in `gGpuCurrentOt[0xB]`.
void func_acropolis_security_room_8017E0C4(s16 id)
{
    POLY_F4* poly;
    DR_MODE* dr;
    u16      c;

    if (id >= 0) {
        c              = id & 0x7F;
        poly           = (POLY_F4*)gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        setlen(poly, 5);
        setcode(poly, 0x2A);
        poly->r0 = c;
        poly->g0 = c;
        poly->b0 = c;
        poly->x0 = -0x66;
        poly->y0 = -0x5F;
        poly->x1 = 0x6C;
        poly->y1 = -0x5F;
        poly->x2 = -0x66;
        poly->y2 = 0x3C;
        poly->x3 = 0x6C;
        poly->y3 = 0x3C;
        addPrim(gGpuCurrentOt + 0xC, poly);

        dr             = (DR_MODE*)gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        setlen(dr, 1);
        dr->code[0] = 0xE100002A;
        addPrim(gGpuCurrentOt + 0xC, dr);
    } else {
        c              = (~id + 1) & 0xFF;
        poly           = (POLY_F4*)gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        setlen(poly, 5);
        setcode(poly, 0x2A);
        poly->r0 = c;
        poly->g0 = c;
        poly->b0 = c;
        poly->x0 = -0x66;
        poly->y0 = -0x5F;
        poly->x1 = 0x6C;
        poly->y1 = -0x5F;
        poly->x2 = -0x66;
        poly->y2 = 0x3C;
        poly->x3 = 0x6C;
        poly->y3 = 0x3C;
        addPrim(gGpuCurrentOt + 0xC, poly);

        dr             = (DR_MODE*)gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        setlen(dr, 1);
        dr->code[0] = 0xE100004A;
        addPrim(gGpuCurrentOt + 0xC, dr);
    }

    poly           = (POLY_F4*)gGpuPrimCursor;
    gGpuPrimCursor = poly + 1;
    setlen(poly, 5);
    setcode(poly, 0x28);
    poly->r0 = 0;
    poly->g0 = 0;
    poly->b0 = 0;
    poly->x0 = -0x66;
    poly->y0 = 0x3C;
    poly->x1 = 0x6C;
    poly->y1 = 0x3C;
    poly->x2 = -0x66;
    poly->y2 = 0x38;
    poly->x3 = 0x6C;
    poly->y3 = 0x38;
    addPrim(gGpuCurrentOt + 0xB, poly);

    dr             = (DR_MODE*)gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE100000A;
    addPrim(gGpuCurrentOt + 0xB, dr);
}

/// Draws the blinking cursor overlay on top of the monitor panel: a 0x6C-wide
/// grey `TILE` whose top edge and height both track `AsrMonitorWork::blinkTimer`,
/// followed by the drawing-mode packet that restores the panel's texture page.
/// The timer wraps at 0x97, which is what makes the bar sweep and restart.
void func_acropolis_security_room_8017E37C(Task* task)
{
    AsrMonitorWork* work;
    TILE*           tile;
    DR_MODE*        dr;
    s16             y;

    tile           = (TILE*)gGpuPrimCursor;
    work           = (AsrMonitorWork*)task->work;
    gGpuPrimCursor = tile + 1;
    setlen(tile, 3);
    setcode(tile, 0x42);
    tile->r0 = 0x60;
    tile->g0 = 0x60;
    tile->b0 = 0x60;
    tile->x0 = -0x66;
    tile->w  = 0x6C;
    y        = work->blinkTimer - 0x5F;
    tile->h  = y;
    tile->y0 = y;
    addPrim(gGpuCurrentOt + 0xE, tile);
    dr             = (DR_MODE*)gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE100000A;
    addPrim(gGpuCurrentOt + 0xE, dr);
    work->blinkTimer++;
    if (work->blinkTimer >= 0x97) {
        work->blinkTimer = 0;
    }
}

/// Per-frame cursor driver of the security-room action prompt, run as state 1
/// of `func_acropolis_security_room_8017E9D8`. Byte-for-byte the same body as
/// `func_acropolis_security_room_8017F480` in the cap script.
///
/// `Task::spawnArg1` picks which pad ports take part: 1 drives port 0 only,
/// 2 port 1 only, anything else both. For each port it integrates the analog
/// stick (pad status 0x12 reads it linearly, 0x73 squares it for a dead-zone
/// curve) and then the d-pad -- whose four bits select one of eight
/// 1/16-of-a-turn headings fed to `rsin`/`rcos` -- into the prompt's
/// 1/512-pixel position, clamps that to the screen, classifies the confirm
/// (0x40) and cancel (0xA0) buttons into the prompt's two button slots, and
/// finally hands the rounded position to `func_acropolis_security_room_8017E8F0`
/// to draw the cursor. `RoomActionPrompt::targetId` doubles as the cursor speed
/// here and `field_E` as the double-press window: a second press inside that
/// many frames without the cursor having moved reports state 4 instead of 2.
///
/// `step` carries the analog delta first and the d-pad heading afterwards, and
/// `idx` indexes the button slots in `u16` units so that `i` survives as the
/// loop counter.
void func_acropolis_security_room_8017E490(Task* task)
{
    RoomActionPrompt* prompt;
    PadState*         pad;
    s32               port;
    s32               first;
    s32               count;
    s32               status;
    s32               stick;
    s32               step;
    s32               mask;
    s32               speed;
    s32               i;
    s32               idx;
    u16*              statep;
    u16*              heldp;

    switch (task->spawnArg1) {
        case 1:
            first = 0;
            count = 1;
            break;
        case 2:
            first = 1;
            count = 2;
            break;
        default:
            first = 0;
            count = 2;
            break;
    }

    for (port = first; port < count; port++) {
        prompt = &D_80114D28 + port;
        pad    = (PadState*)&Pad_States[port];
        status = pad->status;
        if (status == 0x12) {
            speed            = prompt->targetId;
            step             = ((u16)pad->field_54 << 0x10) >> 0x15;
            prompt->field_0 += step * speed * gDisplayState.frameTicks;
            step             = ((u16)pad->field_56 << 0x10) >> 0x15;
            prompt->field_4 += step * speed * gDisplayState.frameTicks;
        } else if (status == 0x73) {
            stick = pad->field_54;
            step  = (stick * stick) >> 0x15;
            if (stick < 0) {
                step = -step;
            }
            prompt->field_0 += step * prompt->targetId * gDisplayState.frameTicks;
            stick            = pad->field_56;
            step             = (stick * stick) >> 0x15;
            if (stick < 0) {
                step = -step;
            }
            prompt->field_4 += step * prompt->targetId * gDisplayState.frameTicks;
        }

        switch (pad->buttons >> 0xC) {
            case 1:
                step = 0x0;
                break;
            case 3:
                step = 0x200;
                break;
            case 2:
                step = 0x400;
                break;
            case 6:
                step = 0x600;
                break;
            case 4:
                step = 0x800;
                break;
            case 12:
                step = 0xA00;
                break;
            case 8:
                step = 0xC00;
                break;
            case 9:
                step = 0xE00;
                break;
            default:
                step = -1;
                break;
        }

        if (step != -1) {
            prompt->field_4 += (-rcos(step) * prompt->targetId * gDisplayState.frameTicks) >> 9;
            prompt->field_0 += (rsin(step) * prompt->targetId * gDisplayState.frameTicks) >> 9;
        }

        if (prompt->field_0 < -0x14000) {
            prompt->field_0 = -0x14000;
        } else if (prompt->field_0 > 0x13E00) {
            prompt->field_0 = 0x13E00;
        }
        if (prompt->field_4 < -0xDC00) {
            prompt->field_4 = -0xDC00;
        } else if (prompt->field_4 > 0xDC00) {
            prompt->field_4 = 0xDC00;
        }

        statep = &prompt->buttons[0].state;
        heldp  = &prompt->buttons[0].heldFrames;
        idx    = 0;
        for (i = 0; i < 2; i++, statep += 4, idx += 4) {
            mask = (i == 0) ? 0x40 : 0xA0;
            if (Pad_CheckButtons(port, 1, mask) != 0) {
                if (heldp[idx] < prompt->field_E &&
                    ((RoomActionPromptScreen*)(heldp + idx + 1))->packed == prompt->screen.packed) {
                    *statep    = 4;
                    heldp[idx] = prompt->field_E;
                } else {
                    heldp[idx]                                           = 0;
                    ((RoomActionPromptScreen*)(heldp + idx + 1))->packed = prompt->screen.packed;
                    *statep                                              = 2;
                }
            } else if (Pad_CheckButtons(port, 3, mask) != 0) {
                *statep = 3;
            } else if (Pad_CheckButtons(port, 0, mask) != 0) {
                *statep = 1;
            } else {
                *statep = 0;
            }
            heldp[idx] += gDisplayState.frameTicks;
        }

        prompt->screen.xy.x = prompt->field_0 >> 9;
        prompt->screen.xy.y = prompt->field_4 >> 9;
        func_acropolis_security_room_8017E8F0(prompt->screen.xy.x, prompt->screen.xy.y, prompt->mode);
    }
}

/// Queues the security monitor's 16x24 cursor/highlight quad at (`x`, `y`)
/// into the current OT. `variant` picks the palette -- 0x3C87 when it is 2,
/// and 0x3C88 otherwise -- and 0 draws nothing at all. The room carries a
/// second copy of this body at `func_acropolis_security_room_8017F8E0`.
void func_acropolis_security_room_8017E8F0(s32 x, s32 y, s32 variant)
{
    POLY_FT4* prim;
    s16       px;
    s16       py;

    if (variant == 0) {
        return;
    }

    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;

    px       = x - 2;
    prim->x2 = px;
    prim->x0 = px;
    px       = x + 0xE;
    prim->x3 = px;
    prim->x1 = px;
    py       = y - 2;
    prim->y1 = py;
    prim->y0 = py;
    py       = y + 0x15;
    prim->y3 = py;
    prim->y2 = py;

    prim->tpage = 0x1E;
    if (variant == 2) {
        prim->clut = 0x3C87;
    } else {
        prim->clut = 0x3C88;
    }

    setUVWH(prim, 0, 0xE8, 0x10, 0x17);
    setlen(prim, 9);
    setcode(prim, 0x2D);

    addPrim(gGpuCurrentOt, prim);
}

/// Two-state dispatcher whose handler table is built on the stack rather than
/// read from `.data`: state 0 runs `func_acropolis_security_room_8017EDE4` and
/// state 1 runs `func_acropolis_security_room_8017E490`.
void func_acropolis_security_room_8017E9D8(Task* task)
{
    TaskFunc funcs[2] = {
        func_acropolis_security_room_8017EDE4,
        func_acropolis_security_room_8017E490,
    };

    funcs[task->state](task);
}

/// Arms the action prompt for the monitor's hotspot and steps the caller on one
/// state: highlights (`mode` 1) the fixed target id 0x80 and clears the
/// prompt's on-screen position, which `func_800D4E78` fills in again when the
/// prompt is actually spawned. The room carries a second copy of this body at
/// `func_acropolis_security_room_8017FB20`.
void func_acropolis_security_room_8017EA28(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;

    prompt->targetId    = 0x80;
    prompt->mode        = 1;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}

/// Confirms the camera the player picked on the security monitor: clears the
/// action prompt, redraws the panel for the selected camera plus its cursor
/// overlay, spawns the prompt at the panel's coordinates and advances the task.
void func_acropolis_security_room_8017EA5C(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    AsrMonitorWork*   work   = (AsrMonitorWork*)task->work;

    prompt->mode     = 0;
    prompt->targetId = 0;
    func_acropolis_security_room_8017E0C4(work->cameraId - 0x7F);
    func_acropolis_security_room_8017E37C(task);
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}

/// Leaves the security monitor: records which camera was on screen as the
/// `0x2A` nibble (index into `D_acropolis_security_room_801826B4`, 0 if the id
/// is not in the table), restores the room's normal display state and kills the
/// monitor task along with the child task it spawned.
void func_acropolis_security_room_8017EADC(Task* task)
{
    AsrMonitorWork* work;
    s16*            camera;
    s32             index;
    s32             cameraId;

    index      = 0;
    camera     = D_acropolis_security_room_801826B4;
    work       = (AsrMonitorWork*)task->work;
    D_80114D08 = 0xA;
    cameraId   = (s16)work->cameraId;
loop:
    if (cameraId != *camera) {
        index  += 1;
        camera += 1;
        if (index >= 5) {
            GameFlag_SetNibble(0x2A, 0);
            goto done;
        }
        goto loop;
    }
    GameFlag_SetNibble(0x2A, index);
done:
    Mc_SaveData.at4.loc.view = 4;
    Display_ReleaseRef();
    gGameSession->cutsceneHold = 0;
    gGameSession->hideHud      = 0;
    gGameSession->eventState   = 0;
    taskKill((Task*)task->spawnArg2);
    Task_RequestKill(task, 0);
}

/// Idle state of the security monitor: hit-tests the action cursor against the
/// monitor's hotspot table and mirrors the result into the room's action
/// prompt. A hit that the player confirms (`buttons[0].state == 2`) on a raised hotspot
/// clears the prompt and runs cap command 0xE; `buttons[1].state == 2` leaves the
/// monitor by advancing to state 5.
void func_acropolis_security_room_8017EB9C(Task* task)
{
    RoomActionPrompt* prompt  = &D_80114D28;
    OverlayHotspot*   hotspot = D_acropolis_security_room_80182648;

    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (Gp_CapBusy() != 0) {
        prompt->mode     = 0;
        prompt->targetId = 0;
        return;
    }
    prompt->targetId = 0x80;
    if (func_acropolis_security_room_8017ECB4(hotspot, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
        if (prompt->buttons[0].state == 2) {
            for (; hotspot->id != -1; hotspot++) {
                if (hotspot->hit != 0) {
                    prompt->mode     = 0;
                    prompt->targetId = 0;
                    Gp_RunCapCmd(0xE, 0);
                    return;
                }
            }
        }
    } else {
        prompt->mode = 1;
    }
    if (prompt->buttons[1].state == 2) {
        task->state = 5;
    }
}

/// Hit-tests the action cursor at (`x`, `y`) against the 0xFFFF-terminated
/// hotspot table `table`, raising `hit` on every entry whose rectangle
/// contains the point and clearing it on every other one. Returns non-zero if
/// any entry was hit, so `func_acropolis_security_room_8017EB9C` can tell
/// "cursor is over something" from "cursor is over nothing" without rescanning
/// the table. Same body as `func_acropolis_security_room_8017FCB0`.
s32 func_acropolis_security_room_8017ECB4(OverlayHotspot* table, s16 x, s16 y)
{
    s32 hit;

    hit = 0;
    while (table->id != -1) {
        if ((x >= table->x) && ((table->x + table->w) >= x) && (y >= table->y) && ((table->y + table->h) >= y)) {
            table->hit = 1;
            hit        = 1;
        } else {
            table->hit = 0;
        }
        table++;
    }
    return hit;
}

/// The sixteen state handlers of the room's cap script.
const TaskFuncTable16 D_acropolis_security_room_8017D63C = { {
    func_acropolis_security_room_8017FA18,
    func_acropolis_security_room_8017FB20,
    func_acropolis_security_room_8017EE44,
    func_acropolis_security_room_8017FB54,
    func_acropolis_security_room_8017FBA4,
    func_acropolis_security_room_8017FC30,
    func_acropolis_security_room_801800A4,
    func_acropolis_security_room_8018014C,
    func_acropolis_security_room_801801C4,
    func_acropolis_security_room_80180218,
    func_acropolis_security_room_8017FE6C,
    func_acropolis_security_room_8017FF0C,
    func_acropolis_security_room_8017FF84,
    func_acropolis_security_room_8017FFD0,
    func_acropolis_security_room_80180010,
    func_acropolis_security_room_80180030,
} };

/// Runs the security-monitor task's current state. The seven handlers are
/// copied onto the stack first, so the call goes through a local table rather
/// than through `.rodata`.
void func_acropolis_security_room_8017ED68(Task* task)
{
    TaskFuncTable7 sp;

    sp = D_acropolis_security_room_8017D5EC;
    sp.funcs[task->state](task);
}

/// Resets both action-prompt slots before the cursor driver's first frame and
/// steps the caller on one state: clears each slot's leading words and its two
/// trailing shorts, parks the target id at 0x100 with `field_E` at 0xF, and
/// marks the slot as highlighted (`mode` 1). The room carries a second copy of
/// this body at `func_acropolis_security_room_80180308`.
void func_acropolis_security_room_8017EDE4(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    s32               i;

    for (i = 0; i < 2; i++, prompt++) {
        prompt->field_0               = 0;
        prompt->field_4               = 0;
        prompt->targetId              = 0x100;
        prompt->field_E               = 0xF;
        prompt->buttons[0].heldFrames = 0;
        prompt->buttons[1].heldFrames = 0;
        prompt->mode                  = 1;
    }
    task->state = task->state + 1;
}

/// Idle state of the security room's cap script: the same hotspot scan
/// `func_acropolis_security_room_8017EB9C` runs for the monitor, but against
/// the script's own table and with the hit recorded in the script's state
/// block instead of dispatched as a cap command. A confirmed
/// (`buttons[0].state == 2`) hit copies the hotspot's `id` and `promptKind` into the
/// state block and advances to state 3; with nothing under the cursor the
/// pending sub-step is cleared and the prompt merely highlights (`mode` 1).
/// `buttons[1].state == 2` leaves the scan by advancing to state 5.
void func_acropolis_security_room_8017EE44(Task* task)
{
    RoomActionPrompt*           prompt = &D_80114D28;
    OverlayHotspot*             hs     = D_acropolis_security_room_801826DC;
    AcropolisSecurityRoomState* st     = (AcropolisSecurityRoomState*)task->work;

    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (Gp_CapBusy() != 0) {
        prompt->mode     = 0;
        prompt->targetId = 0;
        return;
    }
    prompt->targetId = 0x80;
    if (func_acropolis_security_room_8017FCB0(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
        if (prompt->buttons[0].state == 2) {
            for (; hs->id != -1; hs++) {
                if (hs->hit != 0) {
                    prompt->mode     = 0;
                    prompt->targetId = 0;
                    st->variant      = hs->id;
                    st->promptKind   = hs->promptKind;
                    task->state      = 3;
                    return;
                }
            }
        }
    } else {
        st->field_0  = 0;
        prompt->mode = 1;
    }
    if (prompt->buttons[1].state == 2) {
        task->state = 5;
    }
}

/// Outlines `rect` on screen in (`r`, `g`, `b`) with four unconnected flat
/// `LINE_F2`s -- top, right, bottom and left edge of the rectangle spanning
/// (`x`, `y`) to (`x + w`, `y + h`) -- each linked into `gGpuCurrentOt[1]`.
void func_acropolis_security_room_8017EF78(RoomRect* rect, u8 r, u8 g, u8 b)
{
    LINE_F2* line;

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x;
    line->y0 = rect->y;
    line->x1 = rect->x + rect->w;
    line->y1 = rect->y;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x + rect->w;
    line->y0 = rect->y;
    line->x1 = rect->x + rect->w;
    line->y1 = rect->y + rect->h;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x + rect->w;
    line->y0 = rect->y + rect->h;
    line->x1 = rect->x;
    line->y1 = rect->y + rect->h;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x;
    line->y0 = rect->y + rect->h;
    line->x1 = rect->x;
    line->y1 = rect->y;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);
}

void func_acropolis_security_room_8017F1BC(Task* task)
{
    AcropolisSecurityRoomState* st = (AcropolisSecurityRoomState*)task->work;
    s32                         flag;
    s32                         step;

    flag = GameFlag_GetNibble(9);
    if ((flag == 0) || (flag == 2)) {
        step = st->field_0;
        if (step == 0) {
            Gp_StartCapSlot(3, 1, 0);
        } else if (step == 1) {
            Gp_ClearCollectedBit(0x104);
            SndEvt_EnqueueType6(0x51060001, 0, 0);
            GameFlag_SetNibble(9, GameFlag_GetNibble(9) | 1);
            GameFlag_SetNibble(1, 2);
            func_acropolis_security_room_8017FD64(GameFlag_GetNibble(9) & 0xFF);
            st->field_0 = 0;
            task->state = 6;
            func_800E9BDC(1, 0xF9FF);
            Gp_ApplyAreaRecs(D_acropolis_security_room_80184F80);
            taskKill((Task*)task->spawnArg2);
            return;
        } else {
            Gp_StartCapSlot(3, 1, 2);
        }
    } else if ((flag == 1) || (flag == 3)) {
        if (st->field_0 == 0) {
            Gp_StartCapSlot(3, 1, 1);
        } else {
            Gp_StartCapSlot(3, 1, 3);
        }
    } else {
        return;
    }
    task->state = 2;
}

void func_acropolis_security_room_8017F300(Task* task)
{
    AcropolisSecurityRoomState* st = (AcropolisSecurityRoomState*)task->work;
    s32                         flag;
    s32                         step;

    flag = GameFlag_GetNibble(9);
    if ((flag == 0) || (flag == 1)) {
        step = st->field_0;
        if (step == 0) {
            Gp_StartCapSlot(4, 1, 0);
        } else if (step == 2) {
            Gp_ClearCollectedBit(0x103);
            SndEvt_EnqueueType6(0x51060001, 0, 0);
            GameFlag_SetNibble(9, GameFlag_GetNibble(9) | 2);
            func_acropolis_security_room_8017FD64(GameFlag_GetNibble(9) & 0xFF);
            st->field_0              = 0;
            task->state              = 0xA;
            gGameSession->eventState = 1;
            func_800E9BDC(1, 0xF9FF);
            Gp_ApplyAreaRecs(D_acropolis_security_room_80184F50);
            if (GameFlag_GetNibble(3) < 3) {
                Gp_ApplyAreaRecs(D_acropolis_security_room_80184F78);
            } else {
                Gp_ApplyAreaRecs(D_acropolis_security_room_80184F7C);
            }
            taskKill((Task*)task->spawnArg2);
            return;
        } else {
            Gp_StartCapSlot(4, 1, 2);
            task->state = 2;
            return;
        }
    } else if ((flag == 2) || (flag == 3)) {
        if (st->field_0 == 0) {
            Gp_StartCapSlot(4, 1, 1);
        } else {
            Gp_StartCapSlot(4, 1, 3);
        }
    } else {
        return;
    }
    task->state = 2;
}

/// Per-frame cursor driver of the security-room action prompt, run as state 1
/// of `func_acropolis_security_room_8017F9C8`.
///
/// `Task::spawnArg1` picks which pad ports take part: 1 drives port 0 only,
/// 2 port 1 only, anything else both. For each port it integrates the analog
/// stick (pad status 0x12 reads it linearly, 0x73 squares it for a dead-zone
/// curve) and then the d-pad -- whose four bits select one of eight
/// 1/16-of-a-turn headings fed to `rsin`/`rcos` -- into the prompt's
/// 1/512-pixel position, clamps that to the screen, classifies the confirm
/// (0x40) and cancel (0xA0) buttons into the prompt's two button slots, and
/// finally hands the rounded position to `func_acropolis_security_room_8017F8E0`
/// to draw the cursor. `RoomActionPrompt::targetId` doubles as the cursor speed
/// here and `field_E` as the double-press window: a second press inside that
/// many frames without the cursor having moved reports state 4 instead of 2.
///
/// `step` carries the analog delta first and the d-pad heading afterwards, and
/// `idx` indexes the button slots in `u16` units so that `i` survives as the
/// loop counter.
void func_acropolis_security_room_8017F480(Task* task)
{
    RoomActionPrompt* prompt;
    PadState*         pad;
    s32               port;
    s32               first;
    s32               count;
    s32               status;
    s32               stick;
    s32               step;
    s32               mask;
    s32               speed;
    s32               i;
    s32               idx;
    u16*              statep;
    u16*              heldp;

    switch (task->spawnArg1) {
        case 1:
            first = 0;
            count = 1;
            break;
        case 2:
            first = 1;
            count = 2;
            break;
        default:
            first = 0;
            count = 2;
            break;
    }

    for (port = first; port < count; port++) {
        prompt = &D_80114D28 + port;
        pad    = (PadState*)&Pad_States[port];
        status = pad->status;
        if (status == 0x12) {
            speed            = prompt->targetId;
            step             = ((u16)pad->field_54 << 0x10) >> 0x15;
            prompt->field_0 += step * speed * gDisplayState.frameTicks;
            step             = ((u16)pad->field_56 << 0x10) >> 0x15;
            prompt->field_4 += step * speed * gDisplayState.frameTicks;
        } else if (status == 0x73) {
            stick = pad->field_54;
            step  = (stick * stick) >> 0x15;
            if (stick < 0) {
                step = -step;
            }
            prompt->field_0 += step * prompt->targetId * gDisplayState.frameTicks;
            stick            = pad->field_56;
            step             = (stick * stick) >> 0x15;
            if (stick < 0) {
                step = -step;
            }
            prompt->field_4 += step * prompt->targetId * gDisplayState.frameTicks;
        }

        switch (pad->buttons >> 0xC) {
            case 1:
                step = 0x0;
                break;
            case 3:
                step = 0x200;
                break;
            case 2:
                step = 0x400;
                break;
            case 6:
                step = 0x600;
                break;
            case 4:
                step = 0x800;
                break;
            case 12:
                step = 0xA00;
                break;
            case 8:
                step = 0xC00;
                break;
            case 9:
                step = 0xE00;
                break;
            default:
                step = -1;
                break;
        }

        if (step != -1) {
            prompt->field_4 += (-rcos(step) * prompt->targetId * gDisplayState.frameTicks) >> 9;
            prompt->field_0 += (rsin(step) * prompt->targetId * gDisplayState.frameTicks) >> 9;
        }

        if (prompt->field_0 < -0x14000) {
            prompt->field_0 = -0x14000;
        } else if (prompt->field_0 > 0x13E00) {
            prompt->field_0 = 0x13E00;
        }
        if (prompt->field_4 < -0xDC00) {
            prompt->field_4 = -0xDC00;
        } else if (prompt->field_4 > 0xDC00) {
            prompt->field_4 = 0xDC00;
        }

        statep = &prompt->buttons[0].state;
        heldp  = &prompt->buttons[0].heldFrames;
        idx    = 0;
        for (i = 0; i < 2; i++, statep += 4, idx += 4) {
            mask = (i == 0) ? 0x40 : 0xA0;
            if (Pad_CheckButtons(port, 1, mask) != 0) {
                if (heldp[idx] < prompt->field_E &&
                    ((RoomActionPromptScreen*)(heldp + idx + 1))->packed == prompt->screen.packed) {
                    *statep    = 4;
                    heldp[idx] = prompt->field_E;
                } else {
                    heldp[idx]                                           = 0;
                    ((RoomActionPromptScreen*)(heldp + idx + 1))->packed = prompt->screen.packed;
                    *statep                                              = 2;
                }
            } else if (Pad_CheckButtons(port, 3, mask) != 0) {
                *statep = 3;
            } else if (Pad_CheckButtons(port, 0, mask) != 0) {
                *statep = 1;
            } else {
                *statep = 0;
            }
            heldp[idx] += gDisplayState.frameTicks;
        }

        prompt->screen.xy.x = prompt->field_0 >> 9;
        prompt->screen.xy.y = prompt->field_4 >> 9;
        func_acropolis_security_room_8017F8E0(prompt->screen.xy.x, prompt->screen.xy.y, prompt->mode);
    }
}

/// Queues the security-room's 16x24 cursor/highlight quad at (`x`, `y`) into
/// the current OT. `variant` picks the palette -- 0x3C87 when it is 2, and
/// 0x3C88 otherwise -- and 0 draws nothing at all.
void func_acropolis_security_room_8017F8E0(s32 x, s32 y, s32 variant)
{
    POLY_FT4* prim;
    s16       px;
    s16       py;

    if (variant == 0) {
        return;
    }

    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;

    px       = x - 2;
    prim->x2 = px;
    prim->x0 = px;
    px       = x + 0xE;
    prim->x3 = px;
    prim->x1 = px;
    py       = y - 2;
    prim->y1 = py;
    prim->y0 = py;
    py       = y + 0x15;
    prim->y3 = py;
    prim->y2 = py;

    prim->tpage = 0x1E;
    if (variant == 2) {
        prim->clut = 0x3C87;
    } else {
        prim->clut = 0x3C88;
    }

    setUVWH(prim, 0, 0xE8, 0x10, 0x17);
    setlen(prim, 9);
    setcode(prim, 0x2D);

    addPrim(gGpuCurrentOt, prim);
}

/// Task callback of the descriptor at `D_acropolis_security_room_801826C0`:
/// a two-state dispatcher whose handler table is built on the stack rather
/// than read from `.data`, so state 0 runs
/// `func_acropolis_security_room_80180308` and state 1 runs
/// `func_acropolis_security_room_8017F480`.
void func_acropolis_security_room_8017F9C8(Task* task)
{
    TaskFunc funcs[2] = {
        func_acropolis_security_room_80180308,
        func_acropolis_security_room_8017F480,
    };

    funcs[task->state](task);
}

/// State 0 of the security-room cap script: allocates the 0x10 state block into
/// `Task::work`, spawns the script's child task, publishes the message table
/// and the current pair-flag nibble, takes a display reference and clears every
/// hotspot's `hit` flag before the first cursor scan. A failed allocation kills
/// the task instead.
void func_acropolis_security_room_8017FA18(Task* task)
{
    AcropolisSecurityRoomState* st;
    OverlayHotspot*             hs;

    st = memCalloc(sizeof(AcropolisSecurityRoomState), 0);
    if (st == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2          = Task_SpawnFromTable(D_acropolis_security_room_801826C0, 0, 1, 0);
    task->msgTable           = D_acropolis_security_room_801826CC;
    task->work               = (TaskIdMap*)st;
    Mc_SaveData.at4.loc.view = 6;
    SOFT_BARRIER();
    task->state++;
    st->field_0 = 0;
    st->frames  = 0;
    func_acropolis_security_room_8017FD64(GameFlag_GetNibble(9) & 0xFF);
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    gGameSession->eventState   = 1;
    Display_AcquireRef();
    for (hs = D_acropolis_security_room_801826DC; hs->id != -1; hs++) {
        hs->hit = 0;
    }
}

/// Arms the action prompt for the script's hotspot and steps the caller on one
/// state: highlights (`mode` 1) the fixed target id 0x80 and clears the prompt's
/// on-screen position, which `func_800D4E78` fills in again when the prompt is
/// actually spawned. The room carries a second copy of this body at
/// `func_acropolis_security_room_8017EA28`.
void func_acropolis_security_room_8017FB20(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;

    prompt->targetId    = 0x80;
    prompt->mode        = 1;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}

/// Spawns the action prompt for the script's current step: clears the prompt's
/// highlight state, then re-spawns it at the coordinates the gameplay side left
/// in `D_80114D28` with the display mode this state picked.
void func_acropolis_security_room_8017FB54(Task* task)
{
    RoomActionPrompt*           prompt = &D_80114D28;
    AcropolisSecurityRoomState* st     = (AcropolisSecurityRoomState*)task->work;

    prompt->mode     = 0;
    prompt->targetId = 0;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, st->promptKind);
    task->state = 4;
}

/// Runs one step of whichever cap script this task family is driving, once the
/// gameplay side reports the action prompt has been dismissed: clears the
/// prompt, then hands the task to `func_acropolis_security_room_8017F1BC` or
/// `func_acropolis_security_room_8017F300` and resets the sub-step. While
/// `func_800D4EC0` still reports a prompt up and no sub-step is pending, the
/// task instead parks on state 2. `variant` is never written in this overlay --
/// the state block is calloc'd -- so the `func_acropolis_security_room_8017F300`
/// arm is the one this room actually takes.
void func_acropolis_security_room_8017FBA4(Task* task)
{
    RoomActionPrompt*           prompt = &D_80114D28;
    AcropolisSecurityRoomState* st     = (AcropolisSecurityRoomState*)task->work;

    prompt->mode     = 0;
    prompt->targetId = 0;
    if ((func_800D4EC0() != 0) || (st->field_0 != 0)) {
        if (st->variant == 1) {
            func_acropolis_security_room_8017F1BC(task);
            st->field_0 = 0;
            return;
        }
        func_acropolis_security_room_8017F300(task);
        st->field_0 = 0;
        return;
    }
    task->state = 2;
}

void func_acropolis_security_room_8017FC30(Task* task)
{
    D_80114D08 = 0xA;
    Gp_MsgPlayer3F3(1);
    gGameSession->eventState   = 0;
    gGameSession->hideHud      = 0;
    gGameSession->cutsceneHold = 0;
    Mc_SaveData.at4.loc.view   = 3;
    Display_ReleaseRef();
    taskKill((Task*)task->spawnArg2);
    Task_RequestKill(task, 0);
}

/// Hit-tests the action cursor at (`x`, `y`) against the 0xFFFF-terminated
/// hotspot table `table`, raising `hit` on every entry whose rectangle
/// contains the point and clearing it on every other one. Returns non-zero if
/// any entry was hit, so the caller can tell "cursor is over something" from
/// "cursor is over nothing" without rescanning the table.
s32 func_acropolis_security_room_8017FCB0(OverlayHotspot* table, s16 x, s16 y)
{
    s32 hit;

    hit = 0;
    while (table->id != -1) {
        if ((x >= table->x) && ((table->x + table->w) >= x) && (y >= table->y) && ((table->y + table->h) >= y)) {
            table->hit = 1;
            hit        = 1;
        } else {
            table->hit = 0;
        }
        table++;
    }
    return hit;
}

/// Repaints the two security-monitor sprites for the current state of game
/// flag nibble 9, whose low two bits say which of the two shutters has been
/// opened. The nibble selects, for each of the two sprite commands of view 6
/// in this room's sprite record, whether `Gp_LinkViewSprts` skips linking it
/// (`field_4` non-zero) or draws it.
void func_acropolis_security_room_8017FD64(s32 flags)
{
    GameSession* g    = gGameSession;
    GpAreaKey*   sess = &g->at4.loc;
    GpSprtCmd*   cmd;

    cmd = Gp_SprtTables[sess->stage - 1][g->sprtVariant - 1].field_0[sess->area - 1][5].field_4;
    switch (flags & 0xFF) {
        case 0:
            cmd[1].field_4 = 1;
            cmd[2].field_4 = 1;
            break;
        case 1:
            cmd[1].field_4 = 0;
            cmd[2].field_4 = 1;
            break;
        case 2:
            cmd[1].field_4 = 1;
            cmd[2].field_4 = 0;
            break;
        case 3:
            cmd[1].field_4 = 0;
            cmd[2].field_4 = 0;
            break;
    }
}

/// `GpMsgEntry` handler for message 0x13F1, the "can this key item be used
/// here?" query `Gp_UseKeyItemRow` sends to slot 7. `item` is the key item the
/// player highlighted; the three ids this room accepts each select a sub-step
/// of the cap script, recorded in the state block's `field_0` for
/// `func_acropolis_security_room_8017FA18` to pick up. Any other item stores 0
/// and answers 0, which is the "cannot use that now" reply.
s32 func_acropolis_security_room_8017FE24(Task* task, s32 msgId, s32 item, s32 arg3)
{
    AcropolisSecurityRoomState* st = (AcropolisSecurityRoomState*)task->work;

    if (item == 0x101) {
        st->field_0 = 3;
        return 1;
    }
    if (item == 0x103) {
        st->field_0 = 2;
        return 1;
    }
    if (item == 0x104) {
        st->field_0 = 1;
        return 1;
    }
    st->field_0 = 0;
    return 0;
}

/// Fades the screen to white over 0x40 frames, then steps the caller on one
/// state: `frames` doubles as the fade level here, rising by 4 a frame and
/// driving `Fade_DrawOverlay`'s three colour channels together. At the halfway
/// point (0x80) the door chime is queued; once the level passes 0xFF the
/// counter is reset for the next state and `Mc_SaveData.at4.loc.view` is set to 0x10.
void func_acropolis_security_room_8017FE6C(Task* task)
{
    AcropolisSecurityRoomState* st = (AcropolisSecurityRoomState*)task->work;
    u8                          level;

    level = st->frames;
    Fade_DrawOverlay(level, level, level, 2);
    st->frames = st->frames + 4;
    if (st->frames == 0x80) {
        SndEvt_EnqueueType6(0x51060002, 0, 0);
    }
    if (st->frames >= 0x100) {
        st->frames               = 0;
        Mc_SaveData.at4.loc.view = 0x10;
        /* Without the barrier GCC hoists the `lw` of `task->state` above the
         * byte store, dropping the load-delay `nop`. */
        SOFT_BARRIER();
        task->state = task->state + 1;
    }
}

void func_acropolis_security_room_8017FF0C(Task* task)
{
    AcropolisSecurityRoomState* st = (AcropolisSecurityRoomState*)task->work;

    if (st->frames == 1) {
        st->child   = Task_SpawnFromTable(D_acropolis_security_room_80182700, 0, 0, 0);
        task->state = task->state + 1;
    }
    st->frames = st->frames + 1;
}

void func_acropolis_security_room_8017FF84(Task* task)
{
    s32 killArg;

    if (Task_PollKill(((AcropolisSecurityRoomState*)task->work)->child, &killArg) != 0) {
        task->state = task->state + 1;
    }
}

void func_acropolis_security_room_8017FFD0(Task* arg0)
{
    Gp_MsgPlayer3F3(1);
    Gp_MsgPlayer3F3(0);
    arg0->state = (s32)(arg0->state + 1);
}

void func_acropolis_security_room_80180010(Task* task)
{
    Mc_SaveData.at4.loc.view = 3;
    /* Without the barrier GCC hoists the `lw` of `task->state` above the byte
     * store, dropping the load-delay `nop` and making the body one instruction
     * short. */
    SOFT_BARRIER();
    task->state = task->state + 1;
}

void func_acropolis_security_room_80180030(Task* task)
{
    D_80114D08 = 0xA;
    Gp_MsgPlayer3F3(1);
    Display_ReleaseRef();
    gGameSession->hideHud      = 0;
    gGameSession->cutsceneHold = 0;
    gGameSession->eventState   = 0;
    func_800E9BDC(0, 0xF9FF);
    Task_RequestKill(task, 0);
}

void func_acropolis_security_room_801800A4(Task* task)
{
    AcropolisSecurityRoomState* st = (AcropolisSecurityRoomState*)task->work;
    s32                         level;
    s16                         frames;

    GameFlag_SetNibble(0x1EE, 0);
    level = (u8)st->frames;
    Fade_DrawOverlay(level, level, level, 2);
    frames     = st->frames + 4;
    st->frames = frames;
    if ((u16)frames == 0x80) {
        SndEvt_EnqueueType6(0x51060002, 0, 0);
    }
    if (st->frames >= 0x100) {
        st->frames               = 0;
        Mc_SaveData.at4.loc.view = 0xE;
        /* Same load-delay shape as `func_acropolis_security_room_80180010`:
         * without the barrier GCC hoists the `lw` of `task->state` above the
         * byte store and drops the delay `nop`. */
        SOFT_BARRIER();
        task->state = task->state + 1;
    }
}

void func_acropolis_security_room_8018014C(Task* task)
{
    AcropolisSecurityRoomState* st = (AcropolisSecurityRoomState*)task->work;

    if (st->frames == 1) {
        st->child   = Task_SpawnFromTable(D_acropolis_security_room_80182700, 1, 0, 0);
        task->state = task->state + 1;
    }
    st->frames = st->frames + 1;
}

void func_acropolis_security_room_801801C4(Task* task)
{
    s32 killArg;

    if (Task_PollKill(((AcropolisSecurityRoomState*)task->work)->child, &killArg) != 0) {
        Gp_MsgPlayer3F3(1);
        task->state = task->state + 1;
    }
}

void func_acropolis_security_room_80180218(Task* task)
{
    D_80114D08               = 0xA;
    Mc_SaveData.at4.loc.view = 3;
    Display_ReleaseRef();
    func_800E9BDC(0, 0xF9FF);
    Task_RequestKill(task, 0);
    gGameSession->hideHud      = 0;
    gGameSession->cutsceneHold = 0;
    gGameSession->eventState   = 0;
}

/// Runs the cap script's current state. The sixteen handlers are copied onto
/// the stack first, so the call goes through a local table rather than through
/// `.rodata`.
void func_acropolis_security_room_80180294(Task* task)
{
    TaskFuncTable16 sp;

    sp = D_acropolis_security_room_8017D63C;
    sp.funcs[task->state](task);
}

/// Resets both action-prompt slots before the script's first cursor scan and
/// steps the caller on one state: clears each slot's leading words and its two
/// trailing shorts, parks the target id at 0x100 with `field_E` at 0xF, and
/// marks the slot as highlighted (`mode` 1). The room carries a second copy of
/// this body at `func_acropolis_security_room_8017EDE4`.
void func_acropolis_security_room_80180308(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    s32               i;

    for (i = 0; i < 2; i++, prompt++) {
        prompt->field_0               = 0;
        prompt->field_4               = 0;
        prompt->targetId              = 0x100;
        prompt->field_E               = 0xF;
        prompt->buttons[0].heldFrames = 0;
        prompt->buttons[1].heldFrames = 0;
        prompt->mode                  = 1;
    }
    task->state = task->state + 1;
}

/// First `TaskDesc` of `D_acropolis_security_room_80182700`: starts the room's
/// looping ambience, then rides alongside the cutscene task
/// (`func_acropolis_security_room_801804CC`) until the CD queue reaches its cue
/// or the player skips, fading the loop out exactly once either way, and asks
/// the task system to kill itself.
void func_acropolis_security_room_80180368(Task* task)
{
    CdCmdQueue*       queue;
    s32               state;
    AsrAmbienceState* st;
    AsrAmbienceState* alloc;

    queue = &CdCmd_Queue;
    state = task->state;
    st    = (AsrAmbienceState*)task->work;

    switch (state) {
        case 0:
            goto L_case0;
        case 1:
            goto L_case1;
        case 2:
            goto L_case2;
    }
    return;

L_case0:
    alloc      = (AsrAmbienceState*)memCalloc(sizeof(AsrAmbienceState), 0);
    task->work = (TaskIdMap*)alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    Mem_Set(alloc, 0, sizeof(AsrAmbienceState));
    SndEvt_EnqueueType6(0x51060008, 0, 0);
    goto advance;

L_case1:
    if (queue->field_1EA >= 0x46 && st->fadeStarted == 0) {
        SndEvt_EnqueueType7(0x51060008, 0x14);
        st->fadeStarted = state;
    }
    if (CdCmd_IsIdle() & 0xFFFF) {
        task->state = task->state + 1;
    }
    if (Pad_CheckFlag800() == 0) {
        return;
    }
    if (st->fadeStarted == 0) {
        SndEvt_EnqueueType7(0x51060008, 0x14);
    }
advance:
    task->state = task->state + 1;
    return;

L_case2:
    Task_RequestKill(task, 0);
}

/// Second `TaskDesc` of `D_acropolis_security_room_80182700`: kicks off the
/// streamed cutscene for the security room, waits for the CD queue to go idle
/// (or for the player to skip it), then asks the task system to kill itself.
void func_acropolis_security_room_801804CC(Task* arg0)
{
    u8          slotParam[4];
    CdCmdQueue* queue;
    Task*       task;

    task  = arg0;
    queue = &CdCmd_Queue;
    switch (task->state) {
        case 0:
            goto L_case0;
        case 1:
            goto L_case1;
        case 2:
            goto L_case2;
    }
    return;

L_case0:
    queue->field_1EA = 1;
    slotParam[0]     = Stream_FindSlot(&gGameSession->at4.loc.view, 0, 0);
    CdCmd_Enqueue(0x61, 0, slotParam);
    goto advance;

L_case1:
    if (CdCmd_IsIdle() & 0xFFFF) {
        goto advance;
    }
    if (Pad_CheckFlag800() == 0) {
        return;
    }
advance:
    task->state = task->state + 1;
    return;

L_case2:
    Task_RequestKill(task, 0);
}

/// Per-frame update of the security-room's four monitor feeds: state 0 seeds
/// the four screen CLUTs from the unlit palette, state 1 re-blends each of
/// them towards its lit palette by that feed's brightness and spawns the
/// flash effects. `Task::spawnArg2` is the `GpEffWork` holding the lit-feed
/// bitmask (`index`) and the four per-feed brightnesses
/// (`scale` .. `step`).
void func_acropolis_security_room_801805A4(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    s32        i;

    work  = (GpEffWork*)task->spawnArg2;
    coord = task->extra.tmd->coords;

    switch (task->state) {
        case 0: {
            u16* base = D_acropolis_security_room_80182718;
            u16* pal  = D_acropolis_security_room_80182918;
            u16* out  = D_acropolis_security_room_80183118;

            for (i = 0; i < 0x100; i += 0x10) {
                Gp_BlendRgb555Clut(&pal[i], &base[i], 0, &out[i]);
            }
            pal = D_acropolis_security_room_80182B18;
            out = D_acropolis_security_room_80183318;
            for (i = 0; i < 0x100; i += 0x10) {
                Gp_BlendRgb555Clut(&pal[i], &base[i], 0, &out[i]);
            }
            pal = D_acropolis_security_room_80182D18;
            out = D_acropolis_security_room_80183518;
            for (i = 0; i < 0x100; i += 0x10) {
                Gp_BlendRgb555Clut(&pal[i], &base[i], 0, &out[i]);
            }
            pal = D_acropolis_security_room_80182F18;
            out = D_acropolis_security_room_80183718;
            for (i = 0; i < 0x100; i += 0x10) {
                Gp_BlendRgb555Clut(&pal[i], &base[i], 0, &out[i]);
            }
            Gp_LoadImages(D_acropolis_security_room_80183918);
            task->state = task->state + 1;
            break;
        }

        case 1:
            work->index = D_acropolis_security_room_80183968[GameFlag_GetNibble(9)];
            if ((Gp_GetViewIndex() & 0xFF) == 6) {
                u16* pal  = D_acropolis_security_room_80182918;
                u16* base = D_acropolis_security_room_80182718;
                u16* out  = D_acropolis_security_room_80183118;
                s32  limit;

                // The cap flickers by one step every other frame.
                limit        = 0x1000 - ((gDisplayState.animFrame & 1) << 9);
                work->scale  = (work->index & 1) ? ((work->scale < limit) ? work->scale + 0x200 : limit) : 0;
                work->angle  = (work->index & 2) ? ((work->angle < limit) ? work->angle + 0x200 : limit) : 0;
                work->period = (work->index & 4) ? ((work->period < limit) ? work->period + 0x200 : limit) : 0;
                work->step   = (work->index & 8) ? ((work->step < limit) ? work->step + 0x200 : limit) : 0;

                for (i = 0; i < 0x100; i += 0x10) {
                    Gp_BlendRgb555Clut(&pal[i], &base[i], work->scale, &out[i]);
                }
                pal = D_acropolis_security_room_80182B18;
                out = D_acropolis_security_room_80183318;
                for (i = 0; i < 0x100; i += 0x10) {
                    Gp_BlendRgb555Clut(&pal[i], &base[i], work->angle, &out[i]);
                }
                pal = D_acropolis_security_room_80182D18;
                out = D_acropolis_security_room_80183518;
                for (i = 0; i < 0x100; i += 0x10) {
                    Gp_BlendRgb555Clut(&pal[i], &base[i], work->period, &out[i]);
                }
                pal = D_acropolis_security_room_80182F18;
                out = D_acropolis_security_room_80183718;
                for (i = 0; i < 0x100; i += 0x10) {
                    Gp_BlendRgb555Clut(&pal[i], &base[i], work->step, &out[i]);
                }
                Gp_LoadImages(D_acropolis_security_room_80183918);

                for (i = 0; i < 4; i++) {
                    Gp_SpawnEff(0x60049, coord, i, NULL);
                }
            } else if (((Gp_GetViewIndex() & 0xFF) != 8) && ((Gp_GetViewIndex() & 0xFF) != 0x10)) {
                for (i = 0; i < 4; i++) {
                    if ((work->index >> i) & 1) {
                        Gp_SpawnEff(0x600A0, coord, D_acropolis_security_room_801839B8[i],
                                    &D_acropolis_security_room_80183998[i]);
                    }
                }
            }
            break;
    }

    if (GameFlag_GetNibble(1) < 3) {
        func_acropolis_security_room_80180A78(task);
    }
}

/// Draws the security room's sweeping laser beam: two points in the emitter's
/// local frame are rotated into world space by the emitter coordinate's
/// `workm`, projected through `GsWSMATRIX`, and linked into the current OT as
/// one semi-transparent flat `LINE_F2`. The beam only exists in the two camera
/// views selected by the `0xC` bitmask over `GameSession::at4.loc.view`, its far
/// endpoint sweeps with the frame counter (`gDisplayState.animFrame * 6` folded
/// into a 406-step range), and nothing is queued when the near endpoint
/// projects closer than an OTZ of 0x11.
void func_acropolis_security_room_80180A78(Task* task)
{
    void**          scratch;
    u8*             head;
    AsrBeamScratch* blk;
    GpCoord*        coord;
    LINE_F2*        prim;

    coord = task->extra.tmd->coords;
    if ((0xC >> ((u8)gGameSession->at4.loc.view - 1)) & 1) {
        scratch   = (void**)G_SCRATCH_HEAD;
        head      = *scratch;
        blk       = (AsrBeamScratch*)(head - 0x14);
        blk->a.vx = -0x427;
        blk->a.vy = ((u32)gDisplayState.animFrame * 6) % 406 + 0xF633;
        *scratch  = blk;
        blk->a.vz = 0x9AF;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&((AsrBeamScratch*)(head - 0x14))->a);
        gte_rtv0();
        gte_stsv(&((AsrBeamScratch*)(head - 0x14))->a);
        blk->a.vx = *(u16*)&blk->a.vx + *(u16*)&coord->workm.t[0];
        blk->a.vy = *(u16*)&blk->a.vy + *(u16*)&coord->workm.t[1];
        blk->a.vz = *(u16*)&blk->a.vz + *(u16*)&coord->workm.t[2];
        blk->b.vx = -0x1F0;
        blk->b.vy = ((u32)gDisplayState.animFrame * 6) % 406 + 0xF633;
        blk->b.vz = 0x9AF;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&((AsrBeamScratch*)(head - 0x14))->b);
        gte_rtv0();
        gte_stsv(&((AsrBeamScratch*)(head - 0x14))->b);
        blk->b.vx = *(u16*)&blk->b.vx + *(u16*)&coord->workm.t[0];
        blk->b.vy = *(u16*)&blk->b.vy + *(u16*)&coord->workm.t[1];
        blk->b.vz = *(u16*)&blk->b.vz + *(u16*)&coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&((AsrBeamScratch*)(head - 0x14))->a);
        gte_rtps();
        prim           = (LINE_F2*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setLineF2(prim);
        gte_stsxy(&prim->x0);
        gte_ldv0(&((AsrBeamScratch*)(head - 0x14))->b);
        gte_rtps();
        prim->code |= 2;
        gte_stsxy(&prim->x1);
        gte_stszotz(&blk->otz);
        if (((AsrBeamScratch*)(head - 0x14))->otz > 0x10) {
            setRGB0(prim, 0x10, 0x10, 0x10);
            addPrim((u_long*)(((((u32)((AsrBeamScratch*)(head - 0x14))->otz << gDisplayState.otDepthShift) >> 2) &
                               0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 2, ((AsrBeamScratch*)(head - 0x14))->otz);
        }
        SCRATCH_POP_BYTES(0x14);
    }
}

/// Per-frame draw for the security-room's flash sprite: refreshes the task's
/// coordinate frame, loads it into the GTE, then queues one 128x128 textured
/// quad from `D_acropolis_security_room_80183970` -- picked by the low two bits
/// of `Task::spawnArg1` -- into the current OT before releasing the effect's
/// `Gp_State1C` work block.
void func_acropolis_security_room_80180E34(Task* arg0)
{
    GpEffWork* mem;
    GpCoord*   coord;
    POLY_FT4*  prim;
    s16        x;
    s16        y;
    u16        cx;
    u16        cy;

    coord = arg0->extra.tmd->coords;
    mem   = arg0->spawnArg2;
    Gp_UpdateCoord(coord);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);

    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyFT4(prim);
    mem->scale  = arg0->spawnArg1 & 3;
    prim->tpage = 0xAB;
    prim->code |= 3;
    prim->clut  = D_acropolis_security_room_80183970[mem->scale].clut << 6;
    cx          = D_acropolis_security_room_80183970[mem->scale].x;
    cy          = D_acropolis_security_room_80183970[mem->scale].y;
    prim->u0    = D_acropolis_security_room_80183970[mem->scale].u;
    prim->v0    = D_acropolis_security_room_80183970[mem->scale].v;
    prim->u1    = D_acropolis_security_room_80183970[mem->scale].u + 0x7F;
    prim->v1    = D_acropolis_security_room_80183970[mem->scale].v;
    prim->u2    = D_acropolis_security_room_80183970[mem->scale].u;
    prim->v2    = D_acropolis_security_room_80183970[mem->scale].v + 0x7F;
    prim->u3    = D_acropolis_security_room_80183970[mem->scale].u + 0x7F;
    prim->v3    = D_acropolis_security_room_80183970[mem->scale].v + 0x7F;
    x           = cx - 0x40;
    prim->x2    = x;
    prim->x0    = x;
    x           = cx + 0x3F;
    prim->x3    = x;
    prim->x1    = x;
    y           = cy - 0x40;
    prim->y1    = y;
    prim->y0    = y;
    y           = cy + 0x3F;
    prim->y3    = y;
    prim->y2    = y;
    addPrim((u_long*)(((((u32)0x30 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
    Gp_ReleaseState1CMem(mem, arg0);
}

/// Draws a rotating textured quad and updates its drift until it settles.
void func_acropolis_security_room_80181108(Task* arg0)
{
    RoomQuadScratch* blk;
    GpCoord*         coord;
    GpEffWork*       mem;
    POLY_FT4*        prim;
    s32              i;
    SVECTOR*         sv;
    s32              ty;
    s32              tx;
    s32              tz;

    SCRATCH_PUSH(RoomQuadScratch);
    blk   = SCRATCH_HEAD(RoomQuadScratch);
    coord = arg0->extra.tmd->coords;
    mem   = arg0->spawnArg2;
    Gp_UpdateCoord(coord);

    if (mem->age == 0) {
        mem->scale   = 0x20;
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        mem->period  = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1F0);
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        mem->step    = 0x80 - (((u32)Gp_LcgState >> 16) & 0xF0);
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        mem->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        mem->move.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        mem->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
    }

    for (i = 0; i < 4; i++) {
        /* Spelled as a shifted block rather than `&blk->v[i]`, which is the same
           address: the member form lets CSE share one register with the GTE
           macros' `&blk->v[i]`, and the original keeps two. */
        sv           = ((RoomQuadScratch*)((SVECTOR*)blk + i))->v;
        blk->v[i].vx = D_acropolis_security_room_801839C0[i].x * mem->scale;
        sv->vy       = 0;
        sv->vz       = D_acropolis_security_room_801839C0[i].z * mem->scale;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[i]);
        gte_rtv0();
        gte_stsv(&blk->v[i]);
        blk->v[i].vx = *(u16*)&blk->v[i].vx + *(u16*)&coord->workm.t[0];
        sv->vy       = *(u16*)&sv->vy + *(u16*)&coord->workm.t[1];
        sv->vz       = *(u16*)&sv->vz + *(u16*)&coord->workm.t[2];
    }

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->v[0]);
    gte_rtps();

    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyFT4(prim);
    gte_stsxy(&prim->x0);
    gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
    gte_rtpt();
    prim->u0 = 0;
    prim->v0 = 0;
    prim->u1 = 7;
    prim->v1 = 0;
    prim->u2 = 0;
    prim->v2 = 7;
    prim->u3 = 7;
    prim->v3 = 7;
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stszotz(&blk->otz);
    if (blk->otz > 0x10) {
        prim->tpage = 0x2D;
        prim->clut  = 0x4390;
        prim->code |= 1;
        addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
    }
    SCRATCH_POP(RoomQuadScratch);

    if (mem->index == 0) {
        coord->coord.t[0] += mem->move.vx;
        coord->coord.t[1] += mem->move.vy;
        coord->coord.t[2] += mem->move.vz;
        Gfx_RotMatrixX(&coord->coord, mem->period, 0);
        Gfx_RotMatrixZ(&coord->coord, mem->step, 0);
        coord->flg = 0;

        ty = mem->move.vy;
        if (ty >= 0x1D) {
            ty--;
        } else {
            ty++;
        }
        mem->move.vy = ty;

        tx = mem->move.vx;
        if (tx == 0) {
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->move.vx += (2 - (u16)(((u32)Gp_LcgState >> 16) % 5U)) * 8;
        } else {
            if (tx > 0) {
                tx--;
            } else {
                tx++;
            }
            mem->move.vx = tx;
        }

        tz = mem->move.vz;
        if (tz == 0) {
            mem->move.vz += mem->step % 32;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->move.vz += (2 - (u16)(((u32)Gp_LcgState >> 16) % 5U)) * 8;
        } else {
            if (tz > 0) {
                tz--;
            } else {
                tz++;
            }
            mem->move.vz = tz;
        }

        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        mem->period += (1 - (u16)(((u32)Gp_LcgState >> 16) % 3U)) * 16;
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        mem->step   += (1 - (u16)(((u32)Gp_LcgState >> 16) % 3U)) * 8;
        if (coord->coord.t[1] >= -0x1A3) {
            mem->index = 1;
        }
    }

    mem->age = mem->age + 1;
    if ((u8)gGameSession->at4.loc.view != 0xF) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

void func_acropolis_security_room_801817A4(Task* taskArg)
{
    Task* task;
    void* mem;

    POLY_G4*         quad;
    LINE_G3*         line;
    s32              nextIndex;
    s32              greenBit;
    s32              quadLum;
    s32              lineLum;
    u32              tagMask;
    u32              addrMask;
    s32              tpageMode;
    s32              savedLum;
    s32              rawLum;
    u16              vz;
    s32              direction;
    s32              i;
    s32              redBit;
    s32              product;
    s32              greenProduct;
    u8               lineRed;
    u8               lineGreen;
    s32              redProduct;
    u16              redHalf;
    u16              x;
    u16              y;
    u32              rng;
    u8*              head;
    GpCoord*         coord;
    AsrFlashScratch* scratch;

    task  = taskArg;
    coord = task->extra.tmd->coords;
    mem   = task->spawnArg2;
    Gp_UpdateCoord(coord);
    head = SCRATCH_HEAD(u8);
    {
        u8* tmp;
        tmp = head - 0x14;
        SOFT_TOUCH_REG(tmp);
        scratch = (AsrFlashScratch*)tmp;
    }
    scratch->v.vx      = *(u16*)&coord->workm.t[0];
    scratch->v.vy      = *(u16*)&coord->workm.t[1];
    vz                 = *(u16*)&coord->workm.t[2];
    SCRATCH_HEAD(void) = scratch;
    scratch->v.vz      = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((AsrFlashScratch*)(head - 0x14))->v);
    gte_rtps();
    gte_stsxy(&((AsrFlashScratch*)(head - 0x14))->x);
    gte_stszotz(&scratch->otz);
    if (((AsrFlashScratch*)(head - 0x14))->otz >= 0x11) {
        redBit  = ((s32)task->spawnArg1 >> 1) & 1;
        redHalf = redBit;

        rng      = (Gp_LcgState * 5) + 0x71357911;
        greenBit = (u16)task->spawnArg1 & 1;
        rawLum   = ((rng >> 0x10) & 0x70) + 0x40;
        __asm__("move %0,%1" : "=r"(quadLum) : "r"(rawLum));
        redProduct = quadLum * redBit;
        i          = 0;
        addrMask   = 0xFFFFFF;
        SOFT_TOUCH_REG(addrMask);
        Gp_LcgState = rng;
        __asm__("move %0,%1" : "=r"(savedLum) : "r"(rawLum));
        scratch->step = 0xC00 / ((AsrFlashScratch*)(head - 0x14))->otz;
        do {
            quad           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyG4(quad);
            setRGB0(quad, 0, 0, 0);
            setRGB1(quad, 0, 0, 0);
            setRGB2(quad, redProduct, greenBit * quadLum, 0);
            setRGB3(quad, 0, 0, 0);
            quad->x0  = scratch->x - *(u16*)&scratch->step;
            x         = scratch->x;
            quad->x2  = x;
            quad->x1  = x;
            quad->x3  = scratch->x + *(u16*)&scratch->step;
            y         = scratch->y;
            quad->y3  = y;
            quad->y2  = y;
            quad->y0  = y;
            quad->y1  = (scratch->y - *(u16*)&scratch->step) + (scratch->step * (i << 1));
            tagMask   = 0xFF000000;
            quad->tag = (quad->tag & tagMask) | (*((u_long*)(((((u32)scratch->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt)) & addrMask);
            TOUCH_REG(tagMask);
            __asm__("addiu %0,$0,1" : "=r"(tpageMode) : "r"(i));
            *((u_long*)(((((u32)scratch->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt)) = (*((u_long*)(((((u32)scratch->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt)) & tagMask) | ((u32)quad & addrMask);
            i                                                                                                    += tpageMode;
            Gp_AddTpageShift((P_TAG*)quad, tpageMode, scratch->otz);
            USE_REG(scratch);
            USE_REG(scratch);
            USE_REG(scratch);
            USE_REG(scratch);
            USE_REG(scratch);
            USE_REG(scratch);
            USE_REG(scratch);
        } while (i < 2);
        i       = 0;
        lineLum = savedLum;
        do {
            line           = (LINE_G3*)gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            setLineG3(line);
            setRGB0(line, 0, 0, 0);
            product   = redHalf * lineLum;
            lineRed   = product;
            product   = (greenProduct = greenBit * lineLum);
            lineGreen = product;
            setRGB1(line, lineRed, lineGreen, 0);
            setRGB2(line, 0, 0, 0);
            direction = (i << 1) - 1;
            product   = scratch->step * direction;
            line->x0  = scratch->x + product;
            nextIndex = i + 1;
            line->y0  = scratch->y - (scratch->step * nextIndex);
            line->x1  = scratch->x;
            line->y1  = scratch->y;
            line->x2  = scratch->x - (scratch->step * direction);
            line->y2  = scratch->y + (scratch->step * nextIndex);
            addPrim((u_long*)(((((u32)scratch->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), line);
            Gp_AddTpageShift((P_TAG*)line, 1, scratch->otz);
            i = nextIndex;
            USE_REG(scratch);
            USE_REG(scratch);
            USE_REG(scratch);
            USE_REG(scratch);
        } while (i < 2);
    }
    SCRATCH_POP_BYTES(0x14);
    Gp_ReleaseState1CMem(mem, task);
}

/// Gets a 16.16 X/Y/Z displacement for `rec` from `func_800E0C10` and, when it
/// reports one, adds its X and Z to the coordinate's translation, rounding a
/// fractional part away from zero. The whole-unit displacement is also left in
/// `D_acropolis_security_room_801855B0`. Returns non-zero when the X or Z
/// displacement is non-zero.
s32 func_acropolis_security_room_80181C84(GpCoord* coord, GpRec18* rec, s16 arg2)
{
    void**            scratch;
    u8*               head;
    OverlayDeltaFlag* s;
    register void*    p asm("v1");
    s32               val;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    p        = head - 0x14;
    s        = p;
    *scratch = p;
    s->moved = 0;
    if (func_800E0C10(rec, &s->delta, arg2, NULL) != 0) {
        coord->coord.t[0]                    += ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]                    += s->delta.vz.h.hi;
        D_acropolis_security_room_801855B0.vx = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        D_acropolis_security_room_801855B0.vy = s->delta.vy.w >> 16;
        D_acropolis_security_room_801855B0.vz = s->delta.vz.w >> 16;
        val                                   = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_acropolis_security_room_801855B0.vx++;
            } else {
                coord->coord.t[0]--;
                D_acropolis_security_room_801855B0.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_acropolis_security_room_801855B0.vz++;
            } else {
                coord->coord.t[2]--;
                D_acropolis_security_room_801855B0.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    SCRATCH_POP_BYTES(0x14);
    return s->moved;
}

/// Measures the bearing of each type-1 or type-3 record in `recs` (up to
/// `count`, or the first zero key) from the coordinate's world position,
/// relative to the direction it faces. For a record that has every other such
/// record within a quarter turn of it, moves the coordinate `push` units back
/// along that record's bearing, in X and Z. Returns non-zero if it moved the
/// coordinate; returns 0 at once while `gGameSession->viewReady` is 1.
s32 func_acropolis_security_room_80181E28(GpCoord* coord, GpRec18* recs, s16 count, s16 push)
{
    void**                  scratch;
    void**                  tail;
    u8*                     head;
    OverlayBisectorScratch* st;
    u16                     vz;
    s16                     d;
    s16                     dz;
    s32                     t;
    s32                     hit;

    if (gGameSession->viewReady == 1) {
        return 0;
    }

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u8* tmp asm("v0");
        tmp = head - sizeof(OverlayBisectorScratch);
        st  = (OverlayBisectorScratch*)tmp;
    }
    st->eye.vx = *(u16*)&coord->coord.t[0];
    st->eye.vy = *(u16*)&coord->coord.t[1];
    vz         = *(u16*)&coord->coord.t[2];
    *scratch   = st;
    st->eye.vz = vz;

    overlayToWorld(coord->sub, &st->eye);

    st->aim.vx = 0;
    st->aim.vy = 0;
    st->aim.vz = 0x1000;

    overlayToWorld2(coord, &st->aim);

    for (st->i = 0; st->i < count; st->i++) {
        if (recs[st->i].key == 0) {
            st->angle[st->i] = 0x7FFE;
            break;
        }
        st->kind = recs[st->i].key & 0xFFFF0000;
        if ((st->kind != 0x10000) && (st->kind != 0x30000)) {
            st->angle[st->i] = 0x7FFF;
        } else {
            st->delta.vx     = *(u16*)&recs[st->i].point.vx - *(u16*)&st->eye.vx;
            st->delta.vy     = *(u16*)&recs[st->i].point.vy - *(u16*)&st->eye.vy;
            dz               = *(u16*)&recs[st->i].point.vz - *(u16*)&st->eye.vz;
            st->delta.vz     = dz;
            st->angle[st->i] = ratan2(st->delta.vx, dz);

            st->delta.vx     = *(u16*)&st->aim.vx - *(u16*)&st->eye.vx;
            st->delta.vy     = *(u16*)&st->aim.vy - *(u16*)&st->eye.vy;
            dz               = *(u16*)&st->aim.vz - *(u16*)&st->eye.vz;
            st->delta.vz     = dz;
            st->angle[st->i] = *(u16*)&st->angle[st->i] - ratan2(st->delta.vx, dz);

            d = st->angle[st->i];
            if (st->angle[st->i] < 0) {
            wrapUp1:
                if (d < -0x800) {
                    d += 0x1000;
                    goto wrapUp1;
                }
            } else {
            wrapDown1:
                if (d > 0x800) {
                    d -= 0x1000;
                    goto wrapDown1;
                }
            }
            st->angle[st->i] = d;
        }
    }

    st->hit = 0;
    for (st->i = 0; st->i < count; st->i++) {
        if (st->angle[st->i] == 0x7FFE) {
            break;
        }
        if (st->angle[st->i] == 0x7FFF) {
            continue;
        }
        for (st->j = 0; st->j < count; st->j++) {
            if (st->i == st->j) {
                continue;
            }
            if (st->angle[st->j] == 0x7FFF) {
                continue;
            }
            if (st->angle[st->j] != 0x7FFE) {
                st->diff = (u16)st->angle[st->j] - (u16)st->angle[st->i];
                d        = st->diff;
                if (st->diff < 0) {
                wrapUp2:
                    if (d < -0x800) {
                        d += 0x1000;
                        goto wrapUp2;
                    }
                } else {
                wrapDown2:
                    if (d > 0x800) {
                        d -= 0x1000;
                        goto wrapDown2;
                    }
                }
                t        = d;
                st->diff = t;
                SOFT_BARRIER();
                if (t < 0) {
                    t = -t;
                }
                if (t >= 0x401) {
                    break;
                }
                if (st->angle[st->j] != 0x7FFE) {
                    if (st->j + 1 < count) {
                        continue;
                    }
                }
            }
            st->hit = 1;
            Gfx_RotMatrixY(&st->m,
                           st->angle[st->i] + (s16)ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]),
                           1);
            Gfx_MatrixCol2(&st->m, &st->aim);
            VectorNormalSS(&st->aim, &st->aim);
            gte_lddp(-push);
            gte_ldsv(&st->aim);
            gte_gpf12();
            gte_stsv(&st->delta);
            coord->coord.t[0] += st->delta.vx;
            coord->coord.t[2] += st->delta.vz;
            break;
        }
    }

    tail = (void**)G_SCRATCH_HEAD;
    hit  = st->hit;
    SCRATCH_POP_BYTES_AT(tail, sizeof(OverlayBisectorScratch));
    return hit;
}

/// Per-frame visibility hook for a pick-up prop: the model is drawn with flags
/// 8 at OT offset 0 until the item's 2-bit flag reaches 2, after which it is
/// hidden (flags 0x80).
void func_acropolis_security_room_80182574(Task* task)
{
    GpItemObj8* obj;
    TmdObject*  tmd;
    s32         flag;

    obj  = (GpItemObj8*)task->spawnArg2;
    tmd  = task->extra.tmd;
    flag = Gp_GetCurBit2Flag(obj->field_8);
    Gp_GetViewIndex();
    if (flag == 2) {
        tmd->flags = 0x80;
    } else {
        tmd->flags    = 8;
        tmd->otOffset = 0;
    }
}
