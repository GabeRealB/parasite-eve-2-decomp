#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

extern s16 D_80071076;
extern u8  D_80115690;

/// Descriptor of the room's event task, which the event gate spawns.
extern TaskDesc D_dryfield_night_driveway_8017E678;

/// Descriptor table of two tasks (`func_dryfield_night_driveway_8017DAF4`,
/// `func_dryfield_night_driveway_8017DB8C`), ended by a 0xFFFF entry; the
/// event gate spawns entry 1.
extern TaskDesc D_dryfield_night_driveway_8017F34C[];

/// Blocks the room's tasks hand to `func_800E8614` / `func_800E8634`.
extern s32 D_dryfield_night_driveway_8017F3D4;
extern s32 D_dryfield_night_driveway_8017F54C;
extern s32 D_dryfield_night_driveway_8017F6CC;
extern s32 D_dryfield_night_driveway_8017F998;
extern s32 D_dryfield_night_driveway_8017FB00;

/// Message table the room task installs at `Task::msgTable`.
extern GpMsgEntry D_dryfield_night_driveway_8017F7A4[];

/// Three pairs of beam end points, back to back: the first pair at `B0[0]`,
/// the second at `B0[2]` and the third at `D0`. `D0` is its own symbol because
/// the code reaches the third pair by name while it indexes the array for the
/// second.
extern SVECTOR D_dryfield_night_driveway_801805B0[];
extern SVECTOR D_dryfield_night_driveway_801805D0;

/// Spawn argument of the helper task 0x31 the event task starts.
extern GpFadeWork D_dryfield_night_driveway_80182110;

/// The message and the event the event gate latched for the event task, and
/// the flag it sets when it latches one.
extern RoomEventMsg     D_dryfield_night_driveway_80182118;
extern u8               D_dryfield_night_driveway_80182120;
extern RoomLatchedEvent D_dryfield_night_driveway_80182124;

void func_dryfield_night_driveway_8017DCFC(Task* arg0);
void func_dryfield_night_driveway_8017DD7C(Task* task);

/// The room's event task, spawned by the event gate. State 0 runs the latched
/// event's CAP command; state 1 waits for it to finish and, when the event
/// asks for it, starts helper task 0x31; states 2 and 3 play the event's stage
/// sound and wait for it (state 2 skips to 4 when there is none); state 4
/// writes the latched message's destination into the save data and hands over
/// to task type 0x11.
void func_dryfield_night_driveway_8017D608(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_dryfield_night_driveway_80182124.capCmd, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_dryfield_night_driveway_80182124.fade != 0) {
                    D_dryfield_night_driveway_80182110.field_0 = 0;
                    D_dryfield_night_driveway_80182110.field_1 = 0;
                    D_dryfield_night_driveway_80182110.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, (s32)&D_dryfield_night_driveway_80182110);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_dryfield_night_driveway_80182124.stageSnd != 0) {
                Gp_EnqueueStageSnd6(D_dryfield_night_driveway_80182124.stageSnd, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_dryfield_night_driveway_80182124.stageSnd)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_dryfield_night_driveway_80182118.msgId;
            Mc_SaveData.at4.loc.warp = D_dryfield_night_driveway_80182118.field_2;
            Mc_SaveData.at4.loc.room = D_dryfield_night_driveway_80182118.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

/// The room task's three states: set up, idle, kill.
const TaskFuncTable3 D_dryfield_night_driveway_8017D5D8 = {
    { func_dryfield_night_driveway_8017DCFC, func_dryfield_night_driveway_8017DD7C, taskKill },
};

/// Event gate for the driveway. Every message is answered by editing the copy
/// in `out`; the two that matter are message 0x17, which reports whether the
/// road flag is clear and otherwise stages the pending request at
/// `D_dryfield_night_driveway_80182124` for `func_dryfield_night_driveway_8017D608`
/// to replay as a CAP command, and message 0x20, which reports the gate flag and
/// spawns the cutscene task at `D_dryfield_night_driveway_8017F34C`.
s32 func_dryfield_night_driveway_8017D7A0(s32 arg0, s32 arg1, RoomEventMsg* in,
                                          RoomEventMsg* out)
{
    RoomLatchedEvent  req;
    RoomLatchedEvent* p;
    s32               fl;

    *out = *in;
    if (in->msgId == 0x17 && in->field_5 == 0) {
        fl           = GameFlag_GetNibble(0x47) == 0;
        out->field_3 = fl ? 1 : 2;
    }
    if (in->msgId == 0x20 && in->field_5 == 0) {
        fl           = GameFlag_GetNibble(0x51) == 0;
        out->field_3 = fl ? 2 : 1;
        if (GameFlag_GetNibble(0x53) != 0) {
            out->field_3 = out->field_3 + 2;
        }
    }
    if (in->msgId == 2 && GameFlag_GetNibble(0x61) != 0) {
        if (in->field_5 == 0) {
            Gp_RunCapCmd1(6);
            Gp_SetNibbleIf(in->field_6, 2);
        }
        return 2;
    }
    if (in->msgId == 0x20) {
        if (GameFlag_GetNibble(0x3A) != 2) {
            if (in->field_5 == 0) {
                if (gGameSession->at4.loc.stage == 2) {
                    if (gGameSession->at4.loc.place == 1) {
                        if (GameFlag_GetNibble(0x50) == 0) {
                            Task_SpawnFromTable(D_dryfield_night_driveway_8017F34C, 1, 0, 0);
                            return 0;
                        }
                    }
                }
                if (gGameSession->at4.loc.place == 1 && Gp_StateF0.field_0 == gGameSession->at4.loc.place) {
                    return 0;
                }
                Gp_RunCapCmd1(1);
                return 0;
            }
            return 0;
        }
        if (in->field_5 == 0 && GameFlag_GetNibble(0x4B) == 1) {
            GameFlag_SetNibble(0x4B, 2);
        }
    }
    if (in->msgId == 0x17) {
        if (GameFlag_GetNibble(0x30) == 1) {
            if (in->field_5 == 0) {
                Gp_SetNibbleIf(in->field_6, 2);
                Gp_RunCapCmd1(2);
                return 2;
            }
            return 2;
        }
        req.capCmd                         = 9;
        req.stageSnd                       = 0x52190003;
        req.flagId                         = 0x11C;
        req.fade                           = 0;
        p                                  = &req;
        D_dryfield_night_driveway_80182120 = 0;
        if (GameFlag_GetNibble(p->flagId) == 0 || p->flagId == 0) {
            if (out->field_5 == 0) {
                D_dryfield_night_driveway_80182118 = *out;
                D_dryfield_night_driveway_80182124 = req;
                if (p->flagId != 0) {
                    GameFlag_SetNibble(p->flagId, 1);
                }
                Task_SpawnFromTable(&D_dryfield_night_driveway_8017E678, 0, 0, 0);
                D_dryfield_night_driveway_80182120 = 1;
                return 2;
            }
            return 2;
        }
    }
    return 1;
}

/// Task callback: on its first tick it hides the display and hands control to
/// the captioned cutscene; on every later tick it kills the task and clears the
/// collected bit. Either way it advances its own state.
void func_dryfield_night_driveway_8017DAF4(Task* arg0)
{
    if (arg0->state == 0) {
        gGameSession->hideHud = 1;
        D_80115768            = 1;
        SetDispMask(0);
        func_800E3FAC(0xA2, 0x10);
        func_800E8634((s32)&D_dryfield_night_driveway_8017F54C, 0, (s32)&D_dryfield_night_driveway_8017F6CC);
    } else {
        taskKill(arg0);
        Gp_ClearCollectedBit(0x114);
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// Task callback: a four-step script. State 0 queues the weapon message and the
/// captioned command, state 1 waits one tick, state 2 starts the cutscene at
/// `D_dryfield_night_driveway_8017F3D4`, and state 3 - reached by falling out of
/// state 2 - clears area flag 4 for the current location and kills the task once
/// `eventState` is zero.
void func_dryfield_night_driveway_8017DB8C(Task* arg0)
{
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(1);
            arg0->state += 1;
            return;
        case 1:
            arg0->state = 2;
            return;
        case 2:
            func_800E8614((s32)&D_dryfield_night_driveway_8017F3D4, 0);
            arg0->state += 1;
            /* fallthrough */
        case 3:
            if (gGameSession->eventState == 0) {
                Gp_ClearAreaFlag4((GpAreaKey*)&gGameSession->at4.loc);
                taskKill(arg0);
            }
            return;
    }
}

/// Script callback: stores its argument into `Gp_StateF0.field_1A`.
void func_dryfield_night_driveway_8017DC6C(s32 arg0)
{
    Gp_StateF0.field_1A = arg0;
}

/// Script callback: stores its argument into the session's `viewDirty`.
void func_dryfield_night_driveway_8017DC78(s16 arg0)
{
    gGameSession->viewDirty = arg0;
}

/// Script callback: stores its argument into `D_80115768`.
void func_dryfield_night_driveway_8017DC88(u8 arg0)
{
    D_80115768 = arg0;
}

/// Script-event hook: events 8 and 10 each queue their stage sound; every
/// event returns 0.
s32 func_dryfield_night_driveway_8017DC94(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 8:
            Gp_EnqueueStageSnd6(0x52190008, 0, 0);
            break;
        case 10:
            Gp_EnqueueStageSnd6(0x5219000A, 0, 0);
            break;
    }
    return 0;
}

/// Message handlers that answer 0 (messages 0x13F1, 0x13F0 and 0x13EF of the
/// room's message table).
s32 func_dryfield_night_driveway_8017DCE4(void)
{
    return 0;
}

s32 func_dryfield_night_driveway_8017DCEC(void)
{
    return 0;
}

s32 func_dryfield_night_driveway_8017DCF4(void)
{
    return 0;
}

/// First state of the room task: installs the room's message table and takes
/// pointer slot 7; when slot 0xA is set and the room was entered by warp 4, it
/// also hands `D_dryfield_night_driveway_8017FB00` and
/// `D_dryfield_night_driveway_8017F998` to `func_800E8634`. Then advances the
/// state.
void func_dryfield_night_driveway_8017DCFC(Task* arg0)
{
    arg0->msgTable = D_dryfield_night_driveway_8017F7A4;
    Game_SetPtrSlot(arg0, 7);
    if ((gameGetPtrSlot(0xA) != 0) && (gGameSession->at4.loc.warp == 4)) {
        func_800E8634((s32)&D_dryfield_night_driveway_8017FB00, 0, (s32)&D_dryfield_night_driveway_8017F998);
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// Empty task state: the middle entry of the room task's state table. Its only
/// trace is a 0x10-byte stack frame.
void func_dryfield_night_driveway_8017DD7C(Task* task)
{
    char pad[0x10];
}

/// Room task: copies the state table onto the stack and runs the entry for the
/// task's current state.
void func_dryfield_night_driveway_8017DD8C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_driveway_8017D5D8;
    sp.funcs[task->state](task);
}

/// Draws a glowing beam between the two points `arg0[0]` and `arg0[1]`,
/// projected through the view matrix. Each end is a gouraud half-disc of
/// radius `(s16)arg1 * 64 / otz` around its projected point, lit at the centre
/// and dark at the rim, turned to face the other end by the screen-space angle
/// between them; quads join the two discs. The centre brightness flickers
/// between 0x20 and 0x30 with the display frame counter. Nothing is drawn when
/// either projection is invalid. The work block lives on the scratchpad stack.
void func_dryfield_night_driveway_8017DDE4(SVECTOR* arg0, s32 arg1)
{
    void**                   scratch;
    u8*                      head;
    SVECTOR*                 p1;
    OverlayPointPairScratch* block;
    POLY_G4*                 prim;
    DisplayState*            ds;
    s32                      raw;
    s32                      ang;
    s32                      angEnd;
    s32                      limit;
    s32                      angStart;
    s32                      t;
    s32                      t2;
    s32                      t3;
    s32                      conn;
    s32                      scaled;
    s32                      blend;

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
            raw       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &gDisplayState;
            ang       = (s16)raw;
            blend     = ((*(u8*)&ds->animFrame & 1) * 0x10) | 0x20;
            SOFT_BARRIER();
            angEnd = ang + 0x800;
            if (ang < angEnd) {
                angStart = ang;
                limit    = angEnd;
                do {
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, blend, blend, blend);
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

                    conn           = angStart + ((ang - angStart) * 2);
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, blend, blend, blend);
                    setRGB3(prim, blend, blend, blend);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(conn)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(conn)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(conn)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(conn)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim((u_long*)(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                      (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);
                    SCHED_BARRIER();

                    prim           = (POLY_G4*)gGpuPrimCursor;
                    t3             = ang + 0x800;
                    t              = t3;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, blend, blend, blend);
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
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

/// Room draw hook: sets the effect mode to 2, then draws the beams the current
/// view (`gGameSession->at4.loc.view`) shows - views 2 and 9 the first pair, 4
/// and 7 the second, 5 the third, and 3 and 10 both the first and second.
void func_dryfield_night_driveway_8017E5CC(void)
{
    Gp_State1C->roomEffectMode = 2;
    switch (gGameSession->at4.loc.view) {
        case 2:
        case 9:
            func_dryfield_night_driveway_8017DDE4(&D_dryfield_night_driveway_801805B0[0], 0x180);
            break;
        case 4:
        case 7:
            func_dryfield_night_driveway_8017DDE4(&D_dryfield_night_driveway_801805B0[2], 0x180);
            break;
        case 5:
            func_dryfield_night_driveway_8017DDE4(&D_dryfield_night_driveway_801805D0, 0x180);
            break;
        case 3:
        case 10:
            func_dryfield_night_driveway_8017DDE4(&D_dryfield_night_driveway_801805B0[0], 0x180);
            func_dryfield_night_driveway_8017DDE4(&D_dryfield_night_driveway_801805B0[2], 0x180);
            break;
    }
}
