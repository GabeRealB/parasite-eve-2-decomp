#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

/// Work block of the task that scrolls one full-screen image vertically into
/// another. Its first state allocates it zeroed and sets `speed`; the drawing
/// state advances `offset` and places the seam between the images from it.
typedef struct {
    s32 speed;  ///< 16.16 per-frame step, sized so the scroll completes in the spawn argument's frame count
    s32 offset; ///< 16.16 scroll distance; its integer part is clamped to 240 lines
    s16 timer;  ///< Frames drawn so far; scrolling starts once it reaches 46
} _ShelterB1PodAccessTunnelWork;
STATIC_ASSERT_SIZEOF(_ShelterB1PodAccessTunnelWork, 0xC);

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

extern s8       D_80114C12;
extern u8       D_80115690;
extern s32      D_8011572C;
extern s32      D_80115750;
extern s32      D_80115758;
extern TaskDesc D_801348D8;

/// Descriptor of the event task the message handler spawns.
extern TaskDesc D_shelter_b1_pod_access_tunnel_801810CC;

/// The room's message table.
extern s32 D_shelter_b1_pod_access_tunnel_801810D8;

extern TaskDesc D_shelter_b1_pod_access_tunnel_80181108;
extern u8       D_shelter_b1_pod_access_tunnel_80181120[];
extern TaskDesc D_shelter_b1_pod_access_tunnel_801811C8;
extern TaskDesc D_shelter_b1_pod_access_tunnel_80182D2C;
extern s32      D_shelter_b1_pod_access_tunnel_80182FFC;
extern s32      D_shelter_b1_pod_access_tunnel_8018380C;

/// Points the room task draws its bands between, depending on the view.
extern SVECTOR D_shelter_b1_pod_access_tunnel_801839A4[];
extern SVECTOR D_shelter_b1_pod_access_tunnel_801839E4[];

/// The two points of the twin trail, as offsets from its anchor frame. The
/// second is also reached under its own name.
extern SVECTOR D_shelter_b1_pod_access_tunnel_80183A04[];
extern SVECTOR D_shelter_b1_pod_access_tunnel_80183A0C;

extern GpFadeWork       D_shelter_b1_pod_access_tunnel_80184CFC;
extern RoomEventMsg     D_shelter_b1_pod_access_tunnel_80184D04;
extern u8               D_shelter_b1_pod_access_tunnel_80184D0C;
extern RoomLatchedEvent D_shelter_b1_pod_access_tunnel_80184D10;

void func_shelter_b1_pod_access_tunnel_8017DE10(Task* arg0);
void func_shelter_b1_pod_access_tunnel_8017DED8(Task* task);
void func_shelter_b1_pod_access_tunnel_8017E048(Task* task);
void func_shelter_b1_pod_access_tunnel_8017E5B4(Task* task);
void func_shelter_b1_pod_access_tunnel_8017E66C(s32 tpage, s16 arg1);
void func_shelter_b1_pod_access_tunnel_8017E8F4(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b1_pod_access_tunnel_8017F3DC(GpCoord* coord, s32 arg1, s32 arg2, u8* rgb);
void func_shelter_b1_pod_access_tunnel_8017F808(GpCoord* coord, s32 arg1, u8* rgb);
void func_shelter_b1_pod_access_tunnel_8018008C(GpCoord* arg0, GpCoord* arg1, s16 arg2, s16 arg3);
void func_shelter_b1_pod_access_tunnel_8018070C(GpCoord* coord, s16 arg1, u8* rgb);

/// The room's event task, spawned when the message handler latches an event.
/// State 0 runs the latched event's CAP command; state 1 waits for it to
/// finish and, when the event asks for it, starts helper task 0x31; states 2
/// and 3 play the event's stage sound, if any, and wait for it; state 4 writes
/// the latched message's destination into the save data and hands over to
/// task type 0x11.
void func_shelter_b1_pod_access_tunnel_8017D61C(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_shelter_b1_pod_access_tunnel_80184D10.capCmd, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_shelter_b1_pod_access_tunnel_80184D10.fade != 0) {
                    D_shelter_b1_pod_access_tunnel_80184CFC.field_0 = 0;
                    D_shelter_b1_pod_access_tunnel_80184CFC.field_1 = 0;
                    D_shelter_b1_pod_access_tunnel_80184CFC.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, (s32)&D_shelter_b1_pod_access_tunnel_80184CFC);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_shelter_b1_pod_access_tunnel_80184D10.stageSnd != 0) {
                Gp_EnqueueStageSnd6(D_shelter_b1_pod_access_tunnel_80184D10.stageSnd, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_shelter_b1_pod_access_tunnel_80184D10.stageSnd)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.roomVariant = 1;
            Mc_SaveData.at4.loc.area  = D_shelter_b1_pod_access_tunnel_80184D04.msgId;
            Mc_SaveData.at4.loc.warp  = D_shelter_b1_pod_access_tunnel_80184D04.field_2;
            Mc_SaveData.at4.loc.room  = D_shelter_b1_pod_access_tunnel_80184D04.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

static __inline__ s32 _shelterB1PodAccessTunnelStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event)
{
    D_shelter_b1_pod_access_tunnel_80184D0C = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->field_5 == 0) {
            D_shelter_b1_pod_access_tunnel_80184D04 = *dst;
            D_shelter_b1_pod_access_tunnel_80184D10 = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_shelter_b1_pod_access_tunnel_801810CC, 0, 0, 0);
            D_shelter_b1_pod_access_tunnel_80184D0C = 1;
        }
        return 2;
    }
    return 1;
}

s32 func_shelter_b1_pod_access_tunnel_8017D7B4(Task* task, s32 msgId, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomLatchedEvent event;

    *out = *in;
    func_80179A04(in, out);
    if (in->msgId == 0x2F) {
        if (GameFlag_GetNibble(0x118) == 2) {
            if (in->field_5 == 0) {
                Gp_RunCapCmd1(8);
            }
            return 2;
        }
        if (GameFlag_GetNibble(0x7A) >= 6) {
            if (in->field_5 == 0) {
                Gp_MsgPlayerWeapon(0);
                Task_SpawnFromTable(&D_shelter_b1_pod_access_tunnel_80181108, 1, 0, 0);
            }
            return 2;
        }
        if (GameFlag_GetNibble(0xB3) == 0) {
            if (in->field_5 == 0) {
                Gp_SetNibbleIf(in->field_6, 2);
                Gp_RunCapCmd1(3);
            }
            return 0;
        }
        if (in->field_5 == 0 && GameFlag_GetNibble(0xD1) == 0 && GameFlag_GetNibble(0x83) == 0) {
            Gp_FillAllyHp();
            GameFlag_SetNibble(0xD1, 1);
            GameFlag_SetNibble(0x4C, 7);
        }
    }
    if (in->msgId == 0x10) {
        if (GameFlag_GetNibble(0x118) == 2) {
            if (in->field_5 == 0) {
                Gp_RunCapCmd1(8);
            }
            return 2;
        }
        if (GameFlag_GetNibble(0xD1) == 2) {
            if (in->field_5 == 0) {
                Gp_RunCapCmd1(4);
            }
            return 2;
        }
        event.capCmd   = 6;
        event.stageSnd = 0x54110001;
        event.flagId   = 0x130;
        event.fade     = 0;
        return _shelterB1PodAccessTunnelStartEvent(out, &event);
    }
    return 1;
}

/// The room task's three states: set-up, idle and exit.
const TaskFuncTable3 D_shelter_b1_pod_access_tunnel_8017D5D8 = {
    { func_shelter_b1_pod_access_tunnel_8017DE10, func_shelter_b1_pod_access_tunnel_8017DED8, taskKill },
};

void func_shelter_b1_pod_access_tunnel_8017DA74(Task* task)
{
    s32 var_v0;
    s32 room;

    switch (task->state) {
        case 0:
            Gp_RunCapCmd1(GameFlag_GetNibble(0xFC) != 0 ? 5 : 1);
            Gp_StateF0.field_4 = 1;
            goto L_advance;
        case 1:
            var_v0 = Gp_CapBusy();
            goto L_idle;
        case 2:
            if (Gp_GetCapEventKey() != 0xA) {
                if (Gp_GetCapEventKey() == 1) {
                    GameFlag_SetNibble(0x1B6, 2);
                }
                Gp_StateF0.field_4 = 0;
                taskKill(task);
                Gp_MsgPlayerWeapon(1);
                return;
            }
            GameFlag_SetNibble(0x1B6, 0);
            SndEvt_EnqueueType6(0x54110006, 0, 0);
            goto L_advance;
        case 3:
            var_v0 = SndVoice_HasActiveId(0x54110006);
        L_idle:
            if (var_v0 != 0) {
                return;
            }
        L_advance:
            task->state++;
            return;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            Mc_SaveData.at4.loc.area = 0x23;
            Mc_SaveData.at4.loc.warp = 3;
            Mc_SaveData.at4.loc.room = 1;
            room                     = GameFlag_GetNibble(0x118);
            if (room == 2) {
                Mc_SaveData.at4.loc.room = room;
            }
            gDisplayState.roomVariant = 1;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

void func_shelter_b1_pod_access_tunnel_8017DC18(Task* task)
{
    s32 var_v0;

    switch (task->state) {
        case 0:
            Gp_RunCapCmd1(9);
            Gp_StateF0.field_4 = 1;
            goto L_advance;
        case 1:
            var_v0 = Gp_CapBusy();
            goto L_idle;
        case 2:
            if (Gp_GetCapEventKey() != 0xA) {
                Gp_StateF0.field_4 = 0;
                taskKill(task);
                Gp_MsgPlayerWeapon(1);
                return;
            }
            SndEvt_EnqueueType6(0x54110004, 0, 0);
            goto L_advance;
        case 3:
            var_v0 = SndVoice_HasActiveId(0x54110004);
        L_idle:
            if (var_v0 != 0) {
                return;
            }
        L_advance:
            task->state++;
            return;
        case 4:
            GameFlag_SetNibble(0xB4, 1);
            GameFlag_SetNibble(0x1C1, 0);
            Mc_SaveData.sceneEvent    = 0x1C;
            Mc_SaveData.at4.loc.area  = 0x17;
            Mc_SaveData.at4.loc.warp  = 1;
            Mc_SaveData.at4.loc.room  = 1;
            gDisplayState.roomVariant = 1;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

s32 func_shelter_b1_pod_access_tunnel_8017DD68(void)
{
    return 0;
}

s32 func_shelter_b1_pod_access_tunnel_8017DD70(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 1) {
        if (GameFlag_GetNibble(0x12F) != 0) {
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_shelter_b1_pod_access_tunnel_80181108, 0, 0, 0);
        } else {
            GameFlag_SetNibble(0x12F, 1);
            Gp_RunCapCmd1(0xA);
        }
    }
    return 0;
}

s32 func_shelter_b1_pod_access_tunnel_8017DDD8(void)
{
    return 0;
}

s32 func_shelter_b1_pod_access_tunnel_8017DDE0(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 6) {
        SndEvt_EnqueueType6(0x16, 0, 0);
    }
    return 0;
}

void func_shelter_b1_pod_access_tunnel_8017DE10(Task* arg0)
{
    arg0->msgTable = &D_shelter_b1_pod_access_tunnel_801810D8;
    Game_SetPtrSlot(arg0, 7);
    if (GameFlag_GetNibble(0x118) == 1) {
        Task_SpawnFromTable(&D_shelter_b1_pod_access_tunnel_801811C8, 0, 0, 0);
        GameFlag_SetNibble(0x118, 2);
        func_800E3FAC(0xA2, 0x37);
    } else if (GameFlag_GetNibble(0x7E) == 0) {
        func_800E8634((s32)&D_shelter_b1_pod_access_tunnel_80182FFC, 0, (s32)&D_shelter_b1_pod_access_tunnel_8018380C);
        func_800E3FAC(0xA2, 0x1E);
        GameFlag_SetNibble(0x7E, 1);
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// The idle state of the room's task-state table: it does nothing.
void func_shelter_b1_pod_access_tunnel_8017DED8(Task* task)
{
    char pad[0x10];
}

/// Runs the room task through its state table, copied onto the stack first and
/// indexed by the task's state.
void func_shelter_b1_pod_access_tunnel_8017DEE8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_pod_access_tunnel_8017D5D8;
    sp.funcs[task->state](task);
}

/// Two-state task: state 0, unless blocked by `D_80114C12` or `D_80071075`,
/// sends the slot-3 task a `GpAnimArg` built from `D_80073BA9` (msg 0x3E8) and runs
/// `D_shelter_b1_pod_access_tunnel_80181120` through `func_800E8614`; state 1
/// sets `D_8007272D` to 0x1D and kills this task once the session is idle.
void func_shelter_b1_pod_access_tunnel_8017DF40(Task* task)
{
    GpAnimArg rec;
    s32       state;
    s32       weaponId;
    s32       id;

    state = task->state;
    switch (state) {
        case 0:
            if (D_80114C12 != 1 && gDisplayState.pendingMode == 0) {
                weaponId            = Player_Status.weapon;
                id                  = (Mc_SaveData.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
                rec.animBlock.index = id;
                rec.field_4         = 1;
                rec.field_8         = 0;
                rec.field_C         = 0;
                rec.field_10        = 0;
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&rec, 0);
                func_800E8614((s32)D_shelter_b1_pod_access_tunnel_80181120, 0);
                task->state = task->state + 1;
            }
            break;
        case 1:
            if (gGameSession->eventState == 0) {
                Mc_SaveData.sceneEvent = 0x1D;
                Task_RequestKill(task, 0);
            }
            break;
    }
}

/// Draw state of the vertical image scroll. After a delay the seam moves down
/// the screen: the image above it slides in from the top, bottom rows first,
/// while the one below is pushed off the bottom. Each image is drawn as two
/// sprites spanning the screen width. Kills the task once `viewReady` is set
/// or no event is running.
void func_shelter_b1_pod_access_tunnel_8017E048(Task* task)
{
    _ShelterB1PodAccessTunnelWork* work;
    SPRT*                          p;
    s32                            y;

    work = task->work;
    if (gGameSession->viewReady != 0 || gGameSession->eventState == 0) {
        taskKill(task);
        return;
    }
    y = 0;
    if (work->timer++ >= 0x2E) {
        work->offset += work->speed;
        y             = work->offset >> 16;
        if (y > 0xF0) {
            y = 0xF0;
        }
    }

    p              = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(p + 1);
    setSprt(p);
    p->x0 = -0xA0;
    p->y0 = -0x78;
    p->w  = 0x100;
    setRGB0(p, 0x80, 0x80, 0x80);
    p->u0   = 0;
    p->v0   = -0x11 - y;
    p->clut = 0x3FC0;
    p->h    = y + 1;
    addPrim(gGpuCurrentOt + 1023, p);
    func_shelter_b1_pod_access_tunnel_8017E66C(0x340, 0);

    p              = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(p + 1);
    setSprt(p);
    p->x0 = 0x60;
    p->y0 = -0x78;
    p->w  = 0x40;
    setRGB0(p, 0x80, 0x80, 0x80);
    p->u0   = 0;
    p->v0   = -0x11 - y;
    p->clut = 0x3FC0;
    p->h    = y + 1;
    addPrim(gGpuCurrentOt + 1023, p);
    func_shelter_b1_pod_access_tunnel_8017E66C(0x3C0, 0);

    p              = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(p + 1);
    setSprt(p);
    p->x0 = -0xA0;
    p->w  = 0x100;
    setRGB0(p, 0x80, 0x80, 0x80);
    p->u0   = 0;
    p->v0   = 0;
    p->y0   = y - 0x78;
    p->clut = 0x4000;
    p->h    = 0xF0 - y;
    addPrim(gGpuCurrentOt + 1023, p);
    func_shelter_b1_pod_access_tunnel_8017E66C(0x240, 0x100);

    p              = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(p + 1);
    setSprt(p);
    p->x0 = 0x60;
    p->w  = 0x40;
    setRGB0(p, 0x80, 0x80, 0x80);
    p->u0   = 0;
    p->v0   = 0;
    p->y0   = y - 0x78;
    p->clut = 0x4000;
    p->h    = 0xF0 - y;
    addPrim(gGpuCurrentOt + 1023, p);
    func_shelter_b1_pod_access_tunnel_8017E66C(0x2C0, 0x100);
}

/// Queues the replacement of overlay 0x82.
void func_shelter_b1_pod_access_tunnel_8017E39C(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

/// Queues the load of overlay 0x81.
void func_shelter_b1_pod_access_tunnel_8017E3BC(void)
{
    CdCmd_EnqueueOverlay81();
}

/// Restores the stream random-number state.
void func_shelter_b1_pod_access_tunnel_8017E3DC(void)
{
    Gp_RestoreStreamRng();
}

/// Cancels the queued overlay replacement and restarts the CD queue.
void func_shelter_b1_pod_access_tunnel_8017E3FC(void)
{
    CdCmd_CancelReplaceAndActivate();
}

void func_shelter_b1_pod_access_tunnel_8017E41C(s32 arg0)
{
    Task_SpawnFromTable(&D_shelter_b1_pod_access_tunnel_80182D2C, 0, arg0, 0);
}

void func_shelter_b1_pod_access_tunnel_8017E44C(Task* task)
{
    GpViewRec* view;
    VECTOR     vec;

    if (task->killCountdown < task->spawnArg1 && gGameSession->at4.loc.view == 0xB) {
        view   = Gp_GetStageView(&gGameSession->at4.loc);
        vec.vx = 0;
        vec.vy = 0x10;
        vec.vz = 0;
        ApplyTransposeMatrixLV(&view->mtx, &vec, &vec);
        view->mtx.t[0] += vec.vx;
        view->mtx.t[1] += vec.vy;
        view->mtx.t[2] += vec.vz;
        Gp_TrySpawnViewTask((s32)view);
        task->killCountdown++;
        return;
    }
    taskKill(task);
}

void func_shelter_b1_pod_access_tunnel_8017E52C(s32 arg0)
{
    Task_SpawnFromTable(&D_shelter_b1_pod_access_tunnel_80182D2C, 1, arg0, 0);
}

/// The image-scroll task's three states: set-up, scroll and exit.
const TaskFuncTable3 D_shelter_b1_pod_access_tunnel_8017D610 = {
    { func_shelter_b1_pod_access_tunnel_8017E5B4, func_shelter_b1_pod_access_tunnel_8017E048, taskKill },
};

/// Runs the image-scroll task through its state table, copied onto the stack
/// first and indexed by the task's state.
void func_shelter_b1_pod_access_tunnel_8017E55C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_pod_access_tunnel_8017D610;
    sp.funcs[task->state](task);
}

void func_shelter_b1_pod_access_tunnel_8017E5B4(Task* task)
{
    _ShelterB1PodAccessTunnelWork* work;

    if (gGameSession->at4.loc.view != 0xB) {
        taskKill(task);
        return;
    }
    work = memCalloc(sizeof(_ShelterB1PodAccessTunnelWork), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work   = work;
    work->speed  = 0xF00000 / task->spawnArg1;
    task->state += 1;
}

/// Append an 8-bit, ABR-0 `DR_TPAGE` for VRAM origin (`tpage`, `arg1`) to OT
/// slot 1023.
void func_shelter_b1_pod_access_tunnel_8017E66C(s32 tpage, s16 arg1)
{
    DR_TPAGE* p;
    s32       y;

    y              = arg1;
    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setDrawTPage(p, 1, 0, getTPage(1, 0, tpage & 0x3C0, y));
    addPrim(gGpuCurrentOt + 1023, p);
}

void func_shelter_b1_pod_access_tunnel_8017E704(void)
{
    Task_SpawnFromTable(&D_801348D8, 0, 0, 0);
}

void func_shelter_b1_pod_access_tunnel_8017E734(s32 arg0)
{
    Display_InitModeObj(Task_GetDescAt(&D_shelter_b1_pod_access_tunnel_80182D2C, 2U), arg0, 0, 0x100);
}

/// Counts the spawn argument down one per frame; once it goes negative, kills
/// the task and sets the stage's ending flag.
void func_shelter_b1_pod_access_tunnel_8017E778(Task* arg0)
{
    s32 temp_v0;

    temp_v0         = arg0->spawnArg1 - 1;
    arg0->spawnArg1 = temp_v0;
    if (temp_v0 < 0) {
        taskKill(arg0);
        Stage_SetEndingFlag();
    }
}

/// Room callback forwarding to `Gp_PulseState1C`.
void func_shelter_b1_pod_access_tunnel_8017E7B4(void)
{
    Gp_PulseState1C();
}

void func_shelter_b1_pod_access_tunnel_8017E7D4(Task* arg0)
{
    u8 view;

    if (arg0->state == 0) {
        D_80115758  = 0x601CD;
        D_8011572C  = 0x601E9;
        D_80115750  = 0x60205;
        arg0->state = 1;
    }
    view = Gp_GetViewIndex();
    switch (view) {
        case 2: {
            SVECTOR* p = D_shelter_b1_pod_access_tunnel_801839E4;
            func_shelter_b1_pod_access_tunnel_8017E8F4(&p[0], 0x180, 0x111);
            func_shelter_b1_pod_access_tunnel_8017E8F4(&p[2], 0x180, 0x111);
        } break;
        case 3:
        case 7: {
            SVECTOR* p = D_shelter_b1_pod_access_tunnel_801839A4;
            func_shelter_b1_pod_access_tunnel_8017E8F4(&p[0], 0x180, 0x111);
            func_shelter_b1_pod_access_tunnel_8017E8F4(&p[2], 0x180, 0x111);
            func_shelter_b1_pod_access_tunnel_8017E8F4(&p[4], 0x180, 0x111);
            func_shelter_b1_pod_access_tunnel_8017E8F4(&p[6], 0x180, 0x111);
        } break;
        case 4: {
            SVECTOR* p = D_shelter_b1_pod_access_tunnel_801839A4;
            func_shelter_b1_pod_access_tunnel_8017E8F4(&p[0], 0x180, 0x111);
            func_shelter_b1_pod_access_tunnel_8017E8F4(&p[2], 0x180, 0x111);
        } break;
    }
}

/// Draws a gouraud band between the two points `arg0[0]` and `arg0[1]`,
/// projected through `Gfx_ViewWorldMtx`: a half-disc fan at each end and a
/// quad strip joining them, three `POLY_G4`s per 0x400 step of the screen
/// angle between the two centres. `arg1` scales the radii by depth. The lit
/// vertices take the colour packed 4 bits per channel in `arg2`, with the
/// frame counter's low bit blended in so the band flickers.
void func_shelter_b1_pod_access_tunnel_8017E8F4(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**                   scratch;
    u8*                      head;
    OverlayPointPairScratch* block;
    POLY_G4*                 prim;
    DisplayState*            ds;
    SVECTOR*                 p1;
    s32                      ang;
    s32                      t;
    s32                      t3;
    s32                      t2;
    s32                      limit;
    s32                      angStart;
    s32                      packed;
    s32                      blend;
    s32                      tr;
    s32                      tg;
    s32                      scaled;
    s32                      sum;
    u8                       r;
    u8                       g;
    u8                       b;

    p1      = arg0 + 1;
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u8* tmp asm("v0");
        tmp      = head - 0x1C;
        block    = (OverlayPointPairScratch*)tmp;
        *scratch = tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx0);
    gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx1);
        gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((OverlayPointPairScratch*)(head - 0x1C))->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / ((OverlayPointPairScratch*)(head - 0x1C))->otz0;
            block->r1 = scaled / block->otz1;
            ang       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &gDisplayState;
            SCHED_BARRIER();
            ang    = (s16)ang;
            blend  = ((u8)ds->animFrame & 1) * 8;
            packed = arg2 << 16;
            tr     = (packed >> 20) & 0xF0;
            tg     = (packed >> 16) & 0xF0;
            r      = blend | tr;
            g      = blend | tg;
            b      = blend | ((arg2 & 0xF) << 4);
            if (ang < ang + 0x800) {
                angStart = ang;
                limit    = ang + 0x800;
                do {
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
                    t        = ang + 0x200;
                    prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
                    prim->x1 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y1 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    t2       = ang + 0x400;
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx0 + ((block->r0 * rsin(t2)) >> 12);
                    prim->y3 = block->sy0 + ((block->r0 * rcos(t2)) >> 12);
                    addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    prim = (POLY_G4*)gGpuPrimCursor;
                    USE_REG(prim);
                    t   = ang - angStart;
                    t <<= 1;
                    TOUCH_REG(t);
                    sum            = angStart + t;
                    t              = sum;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, r, g, b);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim((u_long*)(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                      (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);
                    SCHED_BARRIER();
                    t3   = ang + 0x800;
                    prim = (POLY_G4*)gGpuPrimCursor;
                    SOFT_BARRIER();
                    t              = t3;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y0 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xA00;
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xC00;
                    prim->x2 = block->sx1;
                    prim->y2 = block->sy1;
                    prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// A flash that swells and then fades. For as many ticks as the spawn argument
/// it brightens and grows two discs and a ring at its frame, all tinted
/// (level, level / 4, level / 2); at full brightness it draws a fade quad, then
/// draws a two-ring billboard that dims by 0x10 a tick and releases its work
/// block once the level falls to 0x10. It pauses while the room's event state
/// is set and releases the block when that state reaches 4.
void func_shelter_b1_pod_access_tunnel_8017F138(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;
    u8         rgb[3];

    work  = task->spawnArg2;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->age++;
        switch (task->state) {
            case 0:
                work->scale = 0;
                work->angle = 0x80;
                work->step  = 0x100 / task->spawnArg1;
                task->state = 1;
                break;
            case 1:
                work->scale += work->step;
                work->angle += work->step;
                task->spawnArg1--;
                rgb[0] = work->scale;
                rgb[1] = work->scale >> 2;
                rgb[2] = work->scale >> 1;
                func_shelter_b1_pod_access_tunnel_8017F808(coord, work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_shelter_b1_pod_access_tunnel_8017F808(coord, (s16)((u16)work->angle * 2), rgb);
                func_shelter_b1_pod_access_tunnel_8017F3DC(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
                if (task->spawnArg1 == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    rgb[0]      = work->scale;
                    rgb[1]      = work->scale >> 2;
                    rgb[2]      = work->scale >> 1;
                    Gp_DrawFadeQuad(rgb, 1);
                }
                break;
            case 2:
                if (work->scale >= 0x11) {
                    rgb[0] = work->scale;
                    rgb[1] = work->scale >> 2;
                    rgb[2] = work->scale >> 1;
                    func_shelter_b1_pod_access_tunnel_8018070C(coord, (s16)(work->angle * 3), rgb);
                    work->scale -= 0x10;
                    work->angle -= 8;
                    break;
                }
                /* fallthrough */
            case 3:
                Gp_ReleaseState1CMem(work, task);
                break;
        }
    }
}

/// Queues a gouraud ring of sixteen quads around the projected world position
/// of `arg0`: black at radius `arg1` and shaded `rgb` at radius `arg1 + arg2`,
/// both scaled by depth. Nothing is drawn when the projection overflows.
void func_shelter_b1_pod_access_tunnel_8017F3DC(GpCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomDraw02Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    register void**    scratch asm("a1");
    register s32       saved asm("t1");
    register u8*       head asm("t0");
    s32                sum;
    s32                otz;
    register s32       rOuter asm("v0");
    s32                rInner;
    u8*                color;
    s32                t;
    u16                vz;
    u16                vx;
    u32                maskLo;
    u32                maskHi;

    saved   = arg1;
    scratch = (void**)G_SCRATCH_HEAD;
    color   = rgb;
    head    = *scratch;
    USE_REG(head);
    vx = (u16)arg0->workm.t[0];
    USE_REG(vx);
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomDraw02Scratch*)tmp;
    }
    block->vec.vx = vx;
    block->vec.vy = (u16)arg0->workm.t[1];
    vz            = (u16)arg0->workm.t[2];
    *scratch      = block;
    sum           = saved + arg2;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomDraw02Scratch*)(head - 0x1C))->vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw02Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw02Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        USE_REG(head);
        otz                                      = ((RoomDraw02Scratch*)(head - 0x1C))->otz + 1;
        rOuter                                   = ((s16)saved * 64) / otz;
        ((RoomDraw02Scratch*)(head - 0x1C))->otz = otz;
        rInner                                   = (s16)sum * 64;
        block->rOuter                            = rOuter;
        rInner                                   = rInner / ((RoomDraw02Scratch*)(head - 0x1C))->otz;
        ang                                      = 0;
        block->rInner                            = rInner;

        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, color[0], color[1], color[2]);
            setRGB3(prim, color[0], color[1], color[2]);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            t        = ang + 0x100;
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            prim->x2 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->rInner * rsin(t)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(t)) >> 12);
            ang      = t;
            maskLo   = 0xFFFFFF;
            maskHi   = 0xFF000000;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            SOFT_USE_REG2(maskLo, maskHi);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// Queues a gouraud disc of eight wedges around the projected world position
/// of `arg0`, shaded `rgb` at the centre and black at the rim, of radius
/// `arg1` scaled by depth. Nothing is drawn when the projection overflows.
void func_shelter_b1_pod_access_tunnel_8017F808(GpCoord* arg0, s32 arg1, u8* rgb)
{
    RoomDraw04Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    register void**    scratch asm("a1");
    u8*                head;
    s32                otz;
    s32                radius;
    s32                t;
    s32                t2;
    u16                vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                          = (u16)arg0->workm.t[0];
        ((RoomDraw04Scratch*)(head - 0x18))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x18;
        block = (RoomDraw04Scratch*)tmp;
    }
    block->vec.vy = (u16)arg0->workm.t[1];
    vz            = (u16)arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw04Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw04Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw04Scratch*)(head - 0x18))->otz);
        USE_REG(head);
        otz           = block->otz + 1;
        radius        = ((s16)arg1 * 64) / otz;
        block->otz    = otz;
        block->radius = radius;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            SOFT_USE_REG(t2);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// A twin trail. The first tick allocates sixteen coordinate frames, eight for
/// each trail, and seeds them all from the two points offset from the anchor,
/// so both trails start collapsed. Each later tick re-places the two points,
/// records them in the next slot of each ring of eight and draws the trails
/// between the rings as a beam. The work block is released once the tick count
/// reaches the spawn argument. It idles while the room's event state is 2 or
/// more.
void func_shelter_b1_pod_access_tunnel_8017FB9C(Task* task)
{
    GpCoord    coord;
    GpCoord*   coords;
    GpCoord*   objCoord;
    GpCoord*   dst;
    GpEffWork* work;
    SVECTOR*   vec;
    s32        i;

    coords   = (GpCoord*)task->work;
    work     = (GpEffWork*)task->spawnArg2;
    objCoord = task->extra.tmd->coords;

    if (Gp_State1C->eventState < 2) {
        work->age++;
        switch (task->state) {
            case 0:
                coords = (GpCoord*)memCalloc(0x500, 0);
                if (coords == NULL) {
                    work->age = 0;
                    return;
                }
                task->work           = (TaskIdMap*)coords;
                objCoord->sub        = work->parent;
                objCoord->coord.t[0] = D_shelter_b1_pod_access_tunnel_80183A04[0].vx;
                objCoord->coord.t[1] = D_shelter_b1_pod_access_tunnel_80183A04[0].vy;
                objCoord->coord.t[2] = D_shelter_b1_pod_access_tunnel_80183A04[0].vz;
                objCoord->flg        = 0;
                Gp_UpdateCoord(objCoord);
                task->state      = 1;
                coord.sub        = work->parent;
                vec              = &D_shelter_b1_pod_access_tunnel_80183A04[1];
                coord.coord.t[0] = vec->vx;
                coord.coord.t[1] = vec->vy;
                coord.coord.t[2] = vec->vz;
                coord.flg        = 0;
                Gp_UpdateCoord(&coord);
                for (i = 0; i < 8; i++) {
                    dst        = &coords[i];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = objCoord->workm;
                    gte_SetRotMatrix(&objCoord->workm);
                    gte_SetTransMatrix(&objCoord->workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                    dst        = &coords[i + 8];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = coord.workm;
                    gte_SetRotMatrix(&coord.workm);
                    gte_SetTransMatrix(&coord.workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                }
                break;

            case 1:
                objCoord->flg = 0;
                Gp_UpdateCoord(objCoord);
                coord.sub        = work->parent;
                coord.coord.t[0] = D_shelter_b1_pod_access_tunnel_80183A0C.vx;
                coord.coord.t[1] = D_shelter_b1_pod_access_tunnel_80183A0C.vy;
                coord.coord.t[2] = D_shelter_b1_pod_access_tunnel_80183A0C.vz;
                coord.flg        = 0;
                Gp_UpdateCoord(&coord);
                dst        = &coords[work->age & 7];
                dst->sub   = &gGfxViewCoord;
                dst->workm = objCoord->workm;
                gte_SetRotMatrix(&objCoord->workm);
                gte_SetTransMatrix(&objCoord->workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                dst        = &coords[(work->age & 7) + 8];
                dst->sub   = &gGfxViewCoord;
                dst->workm = coord.workm;
                gte_SetRotMatrix(&coord.workm);
                gte_SetTransMatrix(&coord.workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst      = &coords[i];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                    dst      = &coords[i + 8];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                }
                func_shelter_b1_pod_access_tunnel_8018008C(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1 && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the beam between two rings of eight coordinate frames as seven
/// gouraud quads, walking back from slot `arg2`, each quad joining two adjacent
/// slots of both rings and dimmer the older it is. `arg3` packs the colour as
/// three multipliers, at bits 8, 4 and 0. A quad whose projection overflows is
/// skipped.
void func_shelter_b1_pod_access_tunnel_8018008C(GpCoord* arg0, GpCoord* arg1, s16 arg2, s16 arg3)
{
    RoomDraw03Scratch* blk;
    GpCoord*           a;
    GpCoord*           b;
    POLY_G4*           prim;
    s32                i;
    s32                j;
    s32                i0;
    s32                i1;
    s32                hi;
    s32                lo;
    s32                fade;
    s32                r;
    s32                g;
    s32                bl;
    s32                r2;
    s32                g2;
    s32                b2;

    {
        register u8* tmp asm("v0");

        tmp                = SCRATCH_HEAD(u8) - sizeof(RoomDraw03Scratch);
        blk                = (RoomDraw03Scratch*)tmp;
        SCRATCH_HEAD(void) = tmp;
    }
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    i = 0;
    do {
        j            = arg2 - i;
        i0           = j & 7;
        a            = &arg0[i0];
        blk->v[0].vx = (u16)a->workm.t[0];
        j            = j - 1;
        blk->v[0].vy = (u16)a->workm.t[1];
        i1           = j & 7;
        blk->v[0].vz = (u16)a->workm.t[2];
        b            = &arg1[i0];
        blk->v[1].vx = (u16)b->workm.t[0];
        blk->v[1].vy = (u16)b->workm.t[1];
        blk->v[1].vz = (u16)b->workm.t[2];
        a            = &arg0[i1];
        blk->v[2].vx = (u16)a->workm.t[0];
        blk->v[2].vy = (u16)a->workm.t[1];
        blk->v[2].vz = (u16)a->workm.t[2];
        b            = &arg1[i1];
        blk->v[3].vx = (u16)b->workm.t[0];
        blk->v[3].vy = (u16)b->workm.t[1];
        blk->v[3].vz = (u16)b->workm.t[2];
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        gte_stsxy(&blk->sx0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&blk->sx1, &blk->sx2, &blk->sx3);
        gte_stflg(&blk->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&blk->otz);
            fade           = 0x40 - i * 9;
            hi             = fade & 0xFF;
            r              = hi * (arg3 >> 8);
            g              = hi * ((arg3 >> 4) & 3);
            bl             = hi * (arg3 & 3);
            lo             = (fade - 9) & 0xFF;
            r2             = lo * (arg3 >> 8);
            g2             = lo * ((arg3 >> 4) & 3);
            prim           = (POLY_G4*)gGpuPrimCursor;
            blk->otz       = blk->otz + 1;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 8);
            b2 = lo * (arg3 & 3);
            setcode(prim, 0x38);
            prim->r0 = r;
            prim->r1 = r;
            prim->g0 = g;
            prim->g1 = g;
            prim->b0 = bl;
            prim->b1 = bl;
            prim->r2 = r2;
            prim->r3 = r2;
            prim->g2 = g2;
            prim->g3 = g2;
            prim->b2 = b2;
            prim->b3 = b2;
            prim->x0 = blk->sx0;
            prim->y0 = blk->sy0;
            prim->x1 = blk->sx1;
            prim->y1 = blk->sy1;
            prim->x2 = blk->sx2;
            prim->y2 = blk->sy2;
            prim->x3 = blk->sx3;
            prim->y3 = blk->sy3;
            addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
        i += 1;
    } while (i < 7);
    SCRATCH_POP(RoomDraw03Scratch);
}

/// A spark burst. The first tick spawns its flash effect; then, for a non-zero
/// spawn argument, it sprays randomly jittered sparks each tick, and for zero
/// it draws a fixed ring and one widening by 0x30 a tick, both dimming by 0x20
/// a tick. Either way it releases its work block after seven ticks. It pauses
/// while the room's event state is set and releases the block when that state
/// reaches 4.
void func_shelter_b1_pod_access_tunnel_80180484(Task* task)
{
    GpCoord*   objCoord;
    GpEffWork* work;
    u8         rgb[4];

    objCoord = task->extra.tmd->coords;
    work     = (GpEffWork*)task->spawnArg2;

    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }

    Gp_UpdateCoord(objCoord);
    work->age++;

    switch (task->state) {
        case 0:
            Gp_SpawnEff(0x60076, objCoord, 0x400, NULL);
            if (task->spawnArg1 != 0) {
                Gp_SpawnEff(0x60070, objCoord, 0x80004600, NULL);
                task->state = 1;
            } else {
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                work->scale = 0x100;
                work->angle = 0xC0;
                task->state = 2;
            }
            break;

        case 1:
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x60070, objCoord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x82003400,
                        &work->move);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 2:
            work->angle -= 0x20;
            work->scale += 0x30;
            rgb[0]       = work->angle;
            rgb[1]       = work->angle >> 1;
            rgb[2]       = work->angle >> 2;
            func_shelter_b1_pod_access_tunnel_8017F3DC(objCoord, 0x100, 0x100, rgb);
            func_shelter_b1_pod_access_tunnel_8017F3DC(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Queues a star-shaped glow at the projected world position of `arg0`: a
/// disc of radius `arg1` scaled by depth, shaded half `arg2` at the centre, an
/// inner disc of half that radius at full `arg2`, and four thin rays at right
/// angles, alternately reaching the radius and twice it, all fading to black
/// at the rim. Nothing is drawn when the projection overflows.
void func_shelter_b1_pod_access_tunnel_8018070C(GpCoord* arg0, s16 arg1, u8* arg2)
{
    register RoomBillboardScratch* block asm("s3");
    register POLY_G4*              prim asm("s2");
    register s32                   ang asm("s4");
    register void**                scratch asm("a1");
    register u8*                   head asm("a2");
    s32                            t;
    s32                            t2;
    s32                            u;
    u16                            vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u16 vx asm("v0");
        vx                                             = (u16)arg0->workm.t[0];
        ((RoomBillboardScratch*)(head - 0x1C))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomBillboardScratch*)tmp;
    }
    block->vec.vy = (u16)arg0->workm.t[1];
    vz            = (u16)arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((RoomBillboardScratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomBillboardScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomBillboardScratch*)(head - 0x1C))->otz);
        USE_REG(head);
        block->otz   += 1;
        block->rOuter = (arg1 * 64) / block->otz;
        block->rInner = (arg1 * 8) / block->otz;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 13);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x1C);
}
