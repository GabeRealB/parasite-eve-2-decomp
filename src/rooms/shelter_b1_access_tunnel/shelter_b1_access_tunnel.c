#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
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

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

/// Task descriptor `func_shelter_b1_access_tunnel_8017D5FC` spawns when a
/// gated event fires.
extern TaskDesc D_shelter_b1_access_tunnel_8017E704;

/// Task descriptor for the room's event task, spawned when the message handler
/// starts the latched event.
extern TaskDesc D_shelter_b1_access_tunnel_8017E710;

/// Message table `func_shelter_b1_access_tunnel_8017DCBC` installs on its task.
extern GpMsgEntry D_shelter_b1_access_tunnel_8017E71C[];

extern SVECTOR D_shelter_b1_access_tunnel_8017E744[];
extern SVECTOR D_shelter_b1_access_tunnel_8017E7B4[];
extern SVECTOR D_shelter_b1_access_tunnel_8017E7D4[];

extern GpAreaApplyRec D_shelter_b1_access_tunnel_8017FF44;
extern GpFadeWork     D_shelter_b1_access_tunnel_8017FF4C;

/// Copy of the message that fired a gated event, kept for the task
/// `func_shelter_b1_access_tunnel_8017D760` to warp from.
extern RoomEventMsg D_shelter_b1_access_tunnel_8017FF54;

/// Set by `func_shelter_b1_access_tunnel_8017D5FC` when the event it gates has
/// just fired, clear otherwise.
extern u8 D_shelter_b1_access_tunnel_8017FF5C;

extern RoomEventMsg D_shelter_b1_access_tunnel_8017FF64;
extern u8           D_shelter_b1_access_tunnel_8017FF6C;

/// Copy of the request that fired a gated event, whose cap command and voice
/// lines the task `func_shelter_b1_access_tunnel_8017D760` plays.
extern RoomEventReq D_shelter_b1_access_tunnel_8017FF70;

extern RoomLatchedEvent D_shelter_b1_access_tunnel_8017FF90;

void func_shelter_b1_access_tunnel_8017DCBC(Task* task);
void func_shelter_b1_access_tunnel_8017DD00(Task* task);
void func_shelter_b1_access_tunnel_8017DEC0(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Gates an event on a game-flag nibble and a collected item: returns 1 when
/// the nibble already shows the event done, 0 (running the request's refusal
/// cap command) when the item is missing, and 2 when it fires, which unless
/// `msg` is a dry run records the request, sets the nibble and spawns the
/// event task.
s32 func_shelter_b1_access_tunnel_8017D5FC(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                                = req->flagId;
    D_shelter_b1_access_tunnel_8017FF5C = 0;
    neg                                 = flag < 0;
    got                                 = (s16)flag;
    if (neg) {
        flag = -flag;
        got  = GameFlag_GetNibble(flag) == 0;
    } else {
        got = GameFlag_GetNibble(got);
    }
    ret = 1;
    if (got == 0) {
        if (Gp_HasCollectedBit(req->itemId) != 0 || req->itemId == 0) {
            ret = 2;
            if (msg->field_5 == 0) {
                D_shelter_b1_access_tunnel_8017FF54 = *msg;
                D_shelter_b1_access_tunnel_8017FF70 = *req;
                id                                  = req->flagId;
                mode                                = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_shelter_b1_access_tunnel_8017E704, 0, 0, 0);
                D_shelter_b1_access_tunnel_8017FF5C = 1;
                return 2;
            }
            return ret;
        }
        ret = 0;
        if (msg->field_5 == 0) {
            Gp_RunCapCmd1(req->field_4);
            Gp_SetNibbleIf(msg->field_6, 2);
            ret = 0;
        }
        return ret;
    }
    return ret;
}

/// Event task: plays the recorded request's cap command and voice lines, then
/// copies the recorded message's area, warp and room into the save location,
/// spawns task 0x11 and ends.
void func_shelter_b1_access_tunnel_8017D760(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_shelter_b1_access_tunnel_8017FF70.field_0);
            if (D_shelter_b1_access_tunnel_8017FF70.field_8 != 0) {
                SndEvt_EnqueueType6(D_shelter_b1_access_tunnel_8017FF70.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_shelter_b1_access_tunnel_8017FF70.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_shelter_b1_access_tunnel_8017FF70.field_C != 0) {
                SndEvt_EnqueueType6(D_shelter_b1_access_tunnel_8017FF70.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_shelter_b1_access_tunnel_8017FF70.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_b1_access_tunnel_8017FF54.msgId;
            Mc_SaveData.at4.loc.warp = D_shelter_b1_access_tunnel_8017FF54.field_2;
            Mc_SaveData.at4.loc.room = (u8)D_shelter_b1_access_tunnel_8017FF54.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// The room's event task, spawned by the message handler when it starts the
/// latched event. It runs the event's CAP command, waits for it to finish and
/// optionally starts helper task 0x31, then plays the event's stage sound and
/// waits for it; finally it writes the latched message's destination into the
/// save location and hands over to task 0x11.
void func_shelter_b1_access_tunnel_8017D8D0(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_shelter_b1_access_tunnel_8017FF90.capCmd, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_shelter_b1_access_tunnel_8017FF90.fade != 0) {
                    D_shelter_b1_access_tunnel_8017FF4C.field_0 = 0;
                    D_shelter_b1_access_tunnel_8017FF4C.field_1 = 0;
                    D_shelter_b1_access_tunnel_8017FF4C.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, (s32)&D_shelter_b1_access_tunnel_8017FF4C);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_shelter_b1_access_tunnel_8017FF90.stageSnd != 0) {
                Gp_EnqueueStageSnd6(D_shelter_b1_access_tunnel_8017FF90.stageSnd, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_shelter_b1_access_tunnel_8017FF90.stageSnd)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_b1_access_tunnel_8017FF64.msgId;
            Mc_SaveData.at4.loc.warp = D_shelter_b1_access_tunnel_8017FF64.field_2;
            Mc_SaveData.at4.loc.room = D_shelter_b1_access_tunnel_8017FF64.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

/// Starts `event` for the outgoing message `dst` unless its flag says it has
/// already happened (answering 1). Otherwise answers 2, and - unless
/// `dst->field_5` asks for a dry run - latches the message and the event,
/// sets the flag and spawns the room's event task.
static __inline__ s32 _accessTunnelStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event)
{
    D_shelter_b1_access_tunnel_8017FF6C = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->field_5 == 0) {
            D_shelter_b1_access_tunnel_8017FF64 = *dst;
            D_shelter_b1_access_tunnel_8017FF90 = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_shelter_b1_access_tunnel_8017E710, 0, 0, 0);
            D_shelter_b1_access_tunnel_8017FF6C = 1;
        }
        return 2;
    }
    return 1;
}

/// Message handler: copies the incoming message to `out` and forwards both to
/// `func_80179A04`. Messages 0x12 and 0x18, while nibble 0x113 is between 1
/// and 3 and this is not a dry run, apply the room's area records and set the
/// nibble to 4. Message 0x15, while nibble 0xE5 is clear, answers 0 and -
/// unless `in->field_5` asks for a dry run - passes `in->field_6` to
/// `Gp_SetNibbleIf` and runs cap command 1. Otherwise message 0x12 goes through
/// the rooms' event gate on flag 0xAD, message 0x14 starts the room event on
/// flag 0x13F, and any other message answers 1.
s32 func_shelter_b1_access_tunnel_8017DA68(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq     req;
    RoomLatchedEvent event;

    *out = *in;
    func_80179A04(in, out);
    if (in->msgId == 0x12 || in->msgId == 0x18) {
        if (in->field_5 == 0 && GameFlag_GetNibble(0x113) > 0 && GameFlag_GetNibble(0x113) < 4) {
            Gp_ApplyAreaRecs(&D_shelter_b1_access_tunnel_8017FF44);
            GameFlag_SetNibble(0x113, 4);
        }
    }
    if (in->msgId == 0x15 && GameFlag_GetNibble(0xE5) == 0) {
        if (in->field_5 == 0) {
            Gp_SetNibbleIf(in->field_6, 2);
            Gp_RunCapCmd1(1);
        }
        return 0;
    }
    if (in->msgId == 0x12) {
        req.field_0 = 3;
        req.field_4 = 1;
        req.field_8 = 0x54130009;
        req.field_C = 0x54130001;
        req.flagId  = 0xAD;
        req.itemId  = 0;
        return func_shelter_b1_access_tunnel_8017D5FC(&req, out);
    }
    if (in->msgId == 0x14) {
        event.capCmd   = 4;
        event.stageSnd = 0x54130005;
        event.flagId   = 0x13F;
        event.fade     = 0;
        return _accessTunnelStartEvent(out, &event);
    }
    return 1;
}

s32 func_shelter_b1_access_tunnel_8017DCA4(void)
{
    return 0;
}

s32 func_shelter_b1_access_tunnel_8017DCAC(void)
{
    return 0;
}

s32 func_shelter_b1_access_tunnel_8017DCB4(void)
{
    return 0;
}

/// The room's three-entry task state table, dispatched by
/// `func_shelter_b1_access_tunnel_8017DD08` from a stack copy.
const TaskFuncTable3 D_shelter_b1_access_tunnel_8017D5F0 = {
    {
        func_shelter_b1_access_tunnel_8017DCBC,
        func_shelter_b1_access_tunnel_8017DD00,
        taskKill,
    },
};

/// Installs the room's message table on `task` and advances it.
void func_shelter_b1_access_tunnel_8017DCBC(Task* task)
{
    task->msgTable = D_shelter_b1_access_tunnel_8017E71C;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

void func_shelter_b1_access_tunnel_8017DD00(Task* task)
{
}

/// Runs the handler for the task's state from the room's state table.
void func_shelter_b1_access_tunnel_8017DD08(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_access_tunnel_8017D5F0;
    sp.funcs[task->state](task);
}

void func_shelter_b1_access_tunnel_8017DD60(void)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 2: {
            SVECTOR* p;
            p = D_shelter_b1_access_tunnel_8017E744;
            func_shelter_b1_access_tunnel_8017DEC0(&p[0], 0x200, 0x334);
            func_shelter_b1_access_tunnel_8017DEC0(&p[2], 0x200, 0x334);
            func_shelter_b1_access_tunnel_8017DEC0(&p[4], 0x180, 0x444);
            break;
        }
        case 3: {
            SVECTOR* p;
            p = D_shelter_b1_access_tunnel_8017E744;
            func_shelter_b1_access_tunnel_8017DEC0(&p[0], 0x200, 0x334);
            func_shelter_b1_access_tunnel_8017DEC0(&p[2], 0x200, 0x334);
            func_shelter_b1_access_tunnel_8017DEC0(&p[4], 0x180, 0x111);
            func_shelter_b1_access_tunnel_8017DEC0(&p[6], 0x180, 0x222);
            func_shelter_b1_access_tunnel_8017DEC0(&p[8], 0x180, 0x333);
            func_shelter_b1_access_tunnel_8017DEC0(&p[10], 0x180, 0x444);
            break;
        }
        case 4: {
            SVECTOR* p;
            p = D_shelter_b1_access_tunnel_8017E7D4;
            func_shelter_b1_access_tunnel_8017DEC0(&p[0], 0x200, 0x343);
            func_shelter_b1_access_tunnel_8017DEC0(&p[-8], 0x180, 0x444);
            func_shelter_b1_access_tunnel_8017DEC0(&p[-6], 0x180, 0x333);
            break;
        }
        case 5: {
            SVECTOR* p;
            p = D_shelter_b1_access_tunnel_8017E7B4;
            func_shelter_b1_access_tunnel_8017DEC0(&p[0], 0x200, 0x344);
            func_shelter_b1_access_tunnel_8017DEC0(&p[2], 0x200, 0x344);
            break;
        }
    }
}

/// Draws a flickering two-point gouraud glow between `arg0[0]` and `arg0[1]`,
/// sized by `arg1` and tinted by the packed nibbles of `arg2`.
void func_shelter_b1_access_tunnel_8017DEC0(SVECTOR* arg0, s32 arg1, s32 arg2)
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
            blend  = (*(u8*)&ds->animFrame & 1) * 8;
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
