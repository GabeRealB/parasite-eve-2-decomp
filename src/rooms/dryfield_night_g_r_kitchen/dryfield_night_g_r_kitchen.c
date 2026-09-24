#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

extern s16 D_80071076;
extern u8  D_801153F4;

/// The event the room's gate `func_dryfield_night_g_r_kitchen_8017D5E8`
/// latched: the incoming message and the request, kept for the event task it
/// spawns from `D_dryfield_night_g_r_kitchen_8017E248`, and the flag the gate
/// sets once it has done so.
extern RoomEventMsg D_dryfield_night_g_r_kitchen_8017EC24;
extern RoomEventReq D_dryfield_night_g_r_kitchen_8017EC30;
extern u8           D_dryfield_night_g_r_kitchen_8017EC2C;
extern TaskDesc     D_dryfield_night_g_r_kitchen_8017E248;

/// The room's message table, `(msgId, handler)` pairs ending at 0x7FFFFFFF,
/// which the entry task installs as its own `Task::msgTable`.
extern GpMsgEntry D_dryfield_night_g_r_kitchen_8017E254[];

/// The two pairs of world points the room's light shafts run between, one
/// pair per `SVECTOR[2]`: the first array holds the two shafts drawn in view
/// 2, the second the two drawn in view 3.
extern SVECTOR D_dryfield_night_g_r_kitchen_8017E27C[];
extern SVECTOR D_dryfield_night_g_r_kitchen_8017E29C[];

/// The room's event gate. A request whose flag nibble is already set (or clear,
/// for a negative `flagId`) answers 1. One whose prerequisite item is missing
/// runs the request's CAP command and answers 0. Otherwise the message and
/// request are latched, the nibble is written, the event task is spawned and
/// the answer is 2. A non-zero `field_5` on the message only reports the
/// answer, with none of the side effects.
s32 func_dryfield_night_g_r_kitchen_8017D5E8(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                                  = req->flagId;
    D_dryfield_night_g_r_kitchen_8017EC2C = 0;
    neg                                   = flag < 0;
    got                                   = (s16)flag;
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
                D_dryfield_night_g_r_kitchen_8017EC24 = *msg;
                D_dryfield_night_g_r_kitchen_8017EC30 = *req;
                id                                    = req->flagId;
                mode                                  = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_dryfield_night_g_r_kitchen_8017E248, 0, 0, 0);
                D_dryfield_night_g_r_kitchen_8017EC2C = 1;
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

/// The event task the gate spawns: runs the latched request's CAP command,
/// plays its two sound events in turn and waits for each to finish, then
/// writes the latched message's destination into the save data and hands over
/// to task type 0x11 to load it.
void func_dryfield_night_g_r_kitchen_8017D74C(Task* task)
{
    switch (task->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_dryfield_night_g_r_kitchen_8017EC30.field_0);
            if (D_dryfield_night_g_r_kitchen_8017EC30.field_8 != 0) {
                SndEvt_EnqueueType6(D_dryfield_night_g_r_kitchen_8017EC30.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_dryfield_night_g_r_kitchen_8017EC30.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_dryfield_night_g_r_kitchen_8017EC30.field_C != 0) {
                SndEvt_EnqueueType6(D_dryfield_night_g_r_kitchen_8017EC30.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_dryfield_night_g_r_kitchen_8017EC30.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_dryfield_night_g_r_kitchen_8017EC24.msgId;
            Mc_SaveData.at4.loc.warp = D_dryfield_night_g_r_kitchen_8017EC24.field_2;
            Mc_SaveData.at4.loc.room = (u8)D_dryfield_night_g_r_kitchen_8017EC24.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// The room's handler for message 0x13F1: answers 0.
s32 func_dryfield_night_g_r_kitchen_8017D8BC(void)
{
    return 0;
}

/// The room's handler for message 0x13EE, the first entry of its message
/// table. It copies the incoming record to `out`; for a record whose first
/// halfword is 0x14 it builds the room's event request -- flag nibble 0x34, no
/// prerequisite item, CAP commands 3 and 3 and two sounds -- and answers what
/// the event gate answers. Everything else answers 1.
s32 func_dryfield_night_g_r_kitchen_8017D8C4(s32 arg0, s32 arg1, RoomEventMsg* in,
                                             RoomEventMsg* out)
{
    RoomEventReq req;

    *out = *in;
    if (in->msgId == 0x14) {
        req.field_0 = 3;
        req.field_4 = 3;
        req.field_8 = 0x53130001;
        req.field_C = 0x53130004;
        req.flagId  = 0x34;
        req.itemId  = 0;
        return func_dryfield_night_g_r_kitchen_8017D5E8(&req, in);
    }
    return 1;
}

/// The room's handler for message 0x13F0: answers 0.
s32 func_dryfield_night_g_r_kitchen_8017D948(void)
{
    return 0;
}

/// The room's handler for message 0x13EF: answers 0.
s32 func_dryfield_night_g_r_kitchen_8017D950(void)
{
    return 0;
}

/// State 0 of the room entry task: installs the room's message table,
/// registers the task in game pointer slot 7 and advances to the idle state.
void func_dryfield_night_g_r_kitchen_8017D958(Task* task)
{
    task->msgTable = D_dryfield_night_g_r_kitchen_8017E254;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// State 1 of the room entry task: idles.
void func_dryfield_night_g_r_kitchen_8017D99C(Task* task)
{
}

/// The room entry task's three states: install the room's message table,
/// idle, and `taskKill`.
const TaskFuncTable3 D_dryfield_night_g_r_kitchen_8017D5DC = {
    { func_dryfield_night_g_r_kitchen_8017D958, func_dryfield_night_g_r_kitchen_8017D99C, taskKill },
};

/// Runs the room entry task's current state from its three-entry table, which
/// it copies onto the stack before the call.
void func_dryfield_night_g_r_kitchen_8017D9A4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_g_r_kitchen_8017D5DC;
    sp.funcs[task->state](task);
}

/// Draws a light shaft between the two world points `arg0[0]` and `arg0[1]`:
/// a fan of gouraud wedges around each projected point, joined by wedges
/// spanning the two, the sweep oriented along the screen-space line between
/// them. Each radius is `(s16)arg1 * 64` over that point's OTZ. Nothing is
/// drawn unless both points project. The lit vertices take a brightness that
/// flickers with the frame counter.
void func_dryfield_night_g_r_kitchen_8017D9FC(SVECTOR* arg0, s32 arg1)
{
    void**             scratch;
    u8*                head;
    SVECTOR*           p1;
    RoomDraw08Scratch* block;
    POLY_G4*           prim;
    DisplayState*      ds;
    s32                raw;
    s32                ang;
    s32                angEnd;
    s32                limit;
    s32                angStart;
    s32                t;
    s32                t2;
    s32                t3;
    s32                conn;
    s32                scaled;
    s32                blend;

    p1      = arg0 + 1;
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u8* tmp asm("v0");
        tmp      = head - 0x1C;
        block    = (RoomDraw08Scratch*)tmp;
        *scratch = tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps_real();
    gte_stsxy(&((RoomDraw08Scratch*)(head - 0x1C))->sx0);
    gte_stflg(&((RoomDraw08Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps_real();
        gte_stsxy(&((RoomDraw08Scratch*)(head - 0x1C))->sx1);
        gte_stflg(&((RoomDraw08Scratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((RoomDraw08Scratch*)(head - 0x1C))->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / ((RoomDraw08Scratch*)(head - 0x1C))->otz0;
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

/// Picks the pair of light shafts `func_dryfield_night_g_r_kitchen_8017D9FC`
/// draws from the current view index (`gGameSession->at4.loc.view`, 2 or 3);
/// any other view draws nothing.
void func_dryfield_night_g_r_kitchen_8017E1E4(void)
{
    u8 view;

    view = gGameSession->at4.loc.view;
    if (view == 2) {
        func_dryfield_night_g_r_kitchen_8017D9FC(&D_dryfield_night_g_r_kitchen_8017E27C[0], 0x100);
        func_dryfield_night_g_r_kitchen_8017D9FC(&D_dryfield_night_g_r_kitchen_8017E27C[2], 0x100);
    } else if (view == 3) {
        func_dryfield_night_g_r_kitchen_8017D9FC(&D_dryfield_night_g_r_kitchen_8017E29C[0], 0x100);
        func_dryfield_night_g_r_kitchen_8017D9FC(&D_dryfield_night_g_r_kitchen_8017E29C[2], 0x100);
    }
}
