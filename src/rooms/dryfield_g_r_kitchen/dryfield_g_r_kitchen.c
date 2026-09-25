#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

extern s16 D_80071076;

/// The event message and request the gate latched for the event task, and the
/// flag saying one was latched this call.
extern RoomEventMsg D_dryfield_g_r_kitchen_8017F55C;
extern RoomEventReq D_dryfield_g_r_kitchen_8017F568;
extern u8           D_dryfield_g_r_kitchen_8017F564;

/// Descriptor of the event task `func_dryfield_g_r_kitchen_8017D74C`.
extern TaskDesc D_dryfield_g_r_kitchen_8017EBB4;

/// The room's message table, installed on the room task by its entry state.
extern GpMsgEntry D_dryfield_g_r_kitchen_8017EBC0[];

/// Endpoints of the two beams drawn in view 2. The code forms this address,
/// but the table starts one entry earlier, so the beams run from `[0]` to
/// `[-1]` and from `[2]` to `[1]`.
extern SVECTOR D_dryfield_g_r_kitchen_8017EBF0[];

/// Endpoints of the two beams drawn in view 3: `[0]` to `[1]` and `[2]` to
/// `[3]`.
extern SVECTOR D_dryfield_g_r_kitchen_8017EC08[];

void func_dryfield_g_r_kitchen_8017D74C(Task* task);
void func_dryfield_g_r_kitchen_8017D958(Task* task);
void func_dryfield_g_r_kitchen_8017D99C(Task* task);
void func_dryfield_g_r_kitchen_8017D9FC(GsCOORDINATE2* arg0, SVECTOR* arg1, SVECTOR* arg2, s32 arg3);
void func_dryfield_g_r_kitchen_8017E27C(GsCOORDINATE2* arg0, SVECTOR* arg1, SVECTOR* arg2, s32 arg3);

/// Event gate for the room's exit. Returns 1 when game-flag nibble
/// `req->flagId` already reads set (clear, for a negative id). Otherwise, when
/// `req->itemId` has been collected or is 0, it returns 2 and - unless
/// `msg->field_5` asks for a dry run - latches `msg` and `req`, sets the
/// nibble and spawns the event task. When the item is missing it returns 0
/// and, outside a dry run, runs cap command `req->field_4`.
s32 func_dryfield_g_r_kitchen_8017D5E8(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                            = req->flagId;
    D_dryfield_g_r_kitchen_8017F564 = 0;
    neg                             = flag < 0;
    got                             = (s16)flag;
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
                D_dryfield_g_r_kitchen_8017F55C = *msg;
                D_dryfield_g_r_kitchen_8017F568 = *req;
                id                              = req->flagId;
                mode                            = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_dryfield_g_r_kitchen_8017EBB4, 0, 0, 0);
                D_dryfield_g_r_kitchen_8017F564 = 1;
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

/// The event task the gate spawns: runs the latched request's cap command,
/// plays its two sound ids in turn, each waited out, then warps to the area,
/// warp point and room the latched message names.
void func_dryfield_g_r_kitchen_8017D74C(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_dryfield_g_r_kitchen_8017F568.field_0);
            if (D_dryfield_g_r_kitchen_8017F568.field_8 != 0) {
                SndEvt_EnqueueType6(D_dryfield_g_r_kitchen_8017F568.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_dryfield_g_r_kitchen_8017F568.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_dryfield_g_r_kitchen_8017F568.field_C != 0) {
                SndEvt_EnqueueType6(D_dryfield_g_r_kitchen_8017F568.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_dryfield_g_r_kitchen_8017F568.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_dryfield_g_r_kitchen_8017F55C.msgId;
            Mc_SaveData.at4.loc.warp = D_dryfield_g_r_kitchen_8017F55C.field_2;
            Mc_SaveData.at4.loc.room = (u8)D_dryfield_g_r_kitchen_8017F55C.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// The room task's three-state table, run from a stack copy by
/// `func_dryfield_g_r_kitchen_8017D9A4`: the entry state
/// `func_dryfield_g_r_kitchen_8017D958`, the idle state
/// `func_dryfield_g_r_kitchen_8017D99C`, then `taskKill`.
const TaskFuncTable3 D_dryfield_g_r_kitchen_8017D5DC = {
    { func_dryfield_g_r_kitchen_8017D958, func_dryfield_g_r_kitchen_8017D99C, taskKill },
};

/// Handler for message 0x13F1 in the room's message table: the room takes no
/// action and reports the message as not handled.
s32 func_dryfield_g_r_kitchen_8017D8BC(void)
{
    return 0;
}

/// Handler for message 0x13EE in the room's message table, which filters a
/// warp request: copies `in` to `out`, and for area 0x14 passes the warp
/// through the event gate with the room's own request - nibble 0x34, no item,
/// cap command 3 and the two sound ids 0x52130001 and 0x52130004 - answering
/// with the gate's result. Any other area answers 1.
s32 func_dryfield_g_r_kitchen_8017D8C4(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;
    s32          ret;

    *out = *in;
    if (in->msgId == 0x14) {
        req.field_0 = 3;
        req.field_4 = 3;
        req.field_8 = 0x52130001;
        req.field_C = 0x52130004;
        req.flagId  = 0x34;
        req.itemId  = 0;
        ret         = func_dryfield_g_r_kitchen_8017D5E8(&req, in);
    } else {
        ret = 1;
    }
    return ret;
}

/// Handler for message 0x13F0 in the room's message table: the room takes no
/// action and reports the message as not handled.
s32 func_dryfield_g_r_kitchen_8017D948(void)
{
    return 0;
}

/// Handler for message 0x13EF in the room's message table: the room takes no
/// action and reports the message as not handled.
s32 func_dryfield_g_r_kitchen_8017D950(void)
{
    return 0;
}

/// Entry state of the room task: installs the room's message table, registers
/// the task in pointer slot 7 and advances to the idle state.
void func_dryfield_g_r_kitchen_8017D958(Task* task)
{
    task->msgTable = D_dryfield_g_r_kitchen_8017EBC0;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// Idle state of the room task.
void func_dryfield_g_r_kitchen_8017D99C(Task* task)
{
}

/// The room task: runs the state the task is in from a stack copy of the
/// room's three-state table.
void func_dryfield_g_r_kitchen_8017D9A4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_g_r_kitchen_8017D5DC;
    sp.funcs[task->state](task);
}

/// Draws a flickering tapered beam between `arg1` and `arg2` in `arg0`'s
/// local space. Both points are rotated by the coordinate's `workm`, offset by
/// its translation and projected through `GsWSMATRIX`, using a 0x28-byte block
/// taken from `G_SCRATCH_HEAD`. Nothing is drawn when the far end's `otz` is
/// below 0x11; the near end's is raised to at least 0x10. The ends get the
/// screen radii `(s16)arg3 * 64 / otz`.
///
/// Two passes, a quarter turn apart, each queue three `POLY_G4`s: a wedge of
/// the near end's disc, a quad joining the two ends, and a wedge of the far
/// end's disc walked backwards from a full turn, so the near end covers one
/// half turn and the far end the other. Centre vertices take a grey of 0x20
/// or 0x30 on the parity of `gDisplayState.animFrame`, rim vertices are black.
/// Each primitive goes into the OT bucket of its own end's `otz` with a
/// `Gp_AddTpageShift` tpage.
void func_dryfield_g_r_kitchen_8017D9FC(GsCOORDINATE2* arg0, SVECTOR* arg1, SVECTOR* arg2, s32 arg3)
{
    u8*                head;
    RoomDraw24Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                t2;
    s32                rgb;
    s32                extent;
    s32                r0;
    s32                r1;

    {
        void** scratch;
        u8*    tmp;

        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        tmp      = head - 0x28;
        *scratch = tmp;
        block    = (RoomDraw24Scratch*)tmp;
    }

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&arg0->workm);
    gte_ldv0(arg1);
    gte_rtv0();
    gte_stsv(&((RoomDraw24Scratch*)(head - 0x28))->vec0);
    *(u16*)&block->vec0.vx = *(u16*)&block->vec0.vx + *(u16*)&arg0->workm.t[0];
    *(u16*)&block->vec0.vy = *(u16*)&block->vec0.vy + *(u16*)&arg0->workm.t[1];
    *(u16*)&block->vec0.vz = *(u16*)&block->vec0.vz + *(u16*)&arg0->workm.t[2];

    gte_SetRotMatrix(&arg0->workm);
    gte_ldv0(arg2);
    gte_rtv0();
    gte_stsv(&((RoomDraw24Scratch*)(head - 0x28))->vec1);
    *(u16*)&block->vec1.vx = *(u16*)&block->vec1.vx + *(u16*)&arg0->workm.t[0];
    *(u16*)&block->vec1.vy = *(u16*)&block->vec1.vy + *(u16*)&arg0->workm.t[1];
    *(u16*)&block->vec1.vz = *(u16*)&block->vec1.vz + *(u16*)&arg0->workm.t[2];

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomDraw24Scratch*)(head - 0x28))->vec0);
    gte_rtps();
    gte_stsxy(&((RoomDraw24Scratch*)(head - 0x28))->sx0);
    gte_stszotz(&block->otz0);
    gte_ldv0(&((RoomDraw24Scratch*)(head - 0x28))->vec1);
    gte_rtps();
    gte_stsxy(&((RoomDraw24Scratch*)(head - 0x28))->sx1);
    gte_stszotz(&((RoomDraw24Scratch*)(head - 0x28))->otz1);
    if (block->otz1 >= 0x11) {
        if (((RoomDraw24Scratch*)(head - 0x28))->otz0 < 0x10) {
            ((RoomDraw24Scratch*)(head - 0x28))->otz0 = 0x10;
        }
        extent    = (s16)arg3 * 64;
        r0        = extent / ((RoomDraw24Scratch*)(head - 0x28))->otz0;
        r1        = extent / block->otz1;
        ang       = 0;
        rgb       = (((u8)gDisplayState.animFrame & 1) * 16) | 0x20;
        block->r0 = r0;
        block->r1 = r1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
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
            addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, rgb, rgb, rgb);
            prim->x0 = block->sx0 + ((block->r0 * rsin(ang * 2)) >> 12);
            prim->y0 = block->sy0 + ((block->r0 * rcos(ang * 2)) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(ang * 2)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(ang * 2)) >> 12);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
            addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx1 + ((block->r1 * rsin(0x1000 - ang)) >> 12);
            prim->y0 = block->sy1 + ((block->r1 * rcos(0x1000 - ang)) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(0xE00 - ang)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(0xE00 - ang)) >> 12);
            prim->x2 = block->sx1;
            prim->y2 = block->sy1;
            prim->x3 = block->sx1 + ((block->r1 * rsin(0xC00 - ang)) >> 12);
            prim->y3 = block->sy1 + ((block->r1 * rcos(0xC00 - ang)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
        } while (ang < 0x800);
    }
    SCRATCH_POP_BYTES(0x28);
}

/// The same tapered light beam as `func_dryfield_g_r_kitchen_8017D9FC`,
/// between two points of `arg0`'s local space, with every angle turned back a
/// quarter turn. The two
/// `RTPS` projections, the drop when the far end's `otz` is below 0x11, the
/// clamp of the near end's `otz` to 0x10 and the radii `(s16)arg3 * 64 / otz`
/// are unchanged.
///
/// The near cap's wedges cover -0x400..0x400, the far cap's the opposite half
/// walked backwards from 0xC00 to 0x400, and the side quads join the two
/// circles at -0x400 and 0x400. The centre colour is 0x10 or 0x20 on the
/// parity of `gDisplayState.animFrame`, one step darker than
/// `func_dryfield_g_r_kitchen_8017D9FC`'s.
void func_dryfield_g_r_kitchen_8017E27C(GsCOORDINATE2* arg0, SVECTOR* arg1, SVECTOR* arg2, s32 arg3)
{
    u8*                head;
    RoomDraw24Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                rgb;
    s32                extent;
    s32                r0;
    s32                r1;

    {
        void** scratch;
        u8*    tmp;

        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        tmp      = head - 0x28;
        *scratch = tmp;
        block    = (RoomDraw24Scratch*)tmp;
    }

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&arg0->workm);
    gte_ldv0(arg1);
    gte_rtv0();
    gte_stsv(&((RoomDraw24Scratch*)(head - 0x28))->vec0);
    *(u16*)&block->vec0.vx = *(u16*)&block->vec0.vx + *(u16*)&arg0->workm.t[0];
    *(u16*)&block->vec0.vy = *(u16*)&block->vec0.vy + *(u16*)&arg0->workm.t[1];
    *(u16*)&block->vec0.vz = *(u16*)&block->vec0.vz + *(u16*)&arg0->workm.t[2];

    gte_SetRotMatrix(&arg0->workm);
    gte_ldv0(arg2);
    gte_rtv0();
    gte_stsv(&((RoomDraw24Scratch*)(head - 0x28))->vec1);
    *(u16*)&block->vec1.vx = *(u16*)&block->vec1.vx + *(u16*)&arg0->workm.t[0];
    *(u16*)&block->vec1.vy = *(u16*)&block->vec1.vy + *(u16*)&arg0->workm.t[1];
    *(u16*)&block->vec1.vz = *(u16*)&block->vec1.vz + *(u16*)&arg0->workm.t[2];

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomDraw24Scratch*)(head - 0x28))->vec0);
    gte_rtps();
    gte_stsxy(&((RoomDraw24Scratch*)(head - 0x28))->sx0);
    gte_stszotz(&block->otz0);
    gte_ldv0(&((RoomDraw24Scratch*)(head - 0x28))->vec1);
    gte_rtps();
    gte_stsxy(&((RoomDraw24Scratch*)(head - 0x28))->sx1);
    gte_stszotz(&((RoomDraw24Scratch*)(head - 0x28))->otz1);
    if (block->otz1 >= 0x11) {
        if (((RoomDraw24Scratch*)(head - 0x28))->otz0 < 0x10) {
            ((RoomDraw24Scratch*)(head - 0x28))->otz0 = 0x10;
        }
        extent    = (s16)arg3 * 64;
        r0        = extent / ((RoomDraw24Scratch*)(head - 0x28))->otz0;
        r1        = extent / block->otz1;
        ang       = 0;
        rgb       = (((u8)gDisplayState.animFrame & 1) * 16) + 0x10;
        block->r0 = r0;
        block->r1 = r1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx0 + ((block->r0 * rsin(ang - 0x400)) >> 12);
            t        = ang - 0x200;
            prim->y0 = block->sy0 + ((block->r0 * rcos(ang - 0x400)) >> 12);
            prim->x1 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
            prim->y1 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
            prim->y3 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
            addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, rgb, rgb, rgb);
            prim->x0 = block->sx0 + ((block->r0 * rsin(ang * 2 - 0x400)) >> 12);
            prim->y0 = block->sy0 + ((block->r0 * rcos(ang * 2 - 0x400)) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(ang * 2 - 0x400)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(ang * 2 - 0x400)) >> 12);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
            addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx1 + ((block->r1 * rsin(0xC00 - ang)) >> 12);
            prim->y0 = block->sy1 + ((block->r1 * rcos(0xC00 - ang)) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(0xA00 - ang)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(0xA00 - ang)) >> 12);
            prim->x2 = block->sx1;
            prim->y2 = block->sy1;
            prim->x3 = block->sx1 + ((block->r1 * rsin(0x800 - ang)) >> 12);
            prim->y3 = block->sy1 + ((block->r1 * rcos(0x800 - ang)) >> 12);
            ang     += 0x400;
            addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
        } while (ang < 0x800);
    }
    SCRATCH_POP_BYTES(0x28);
}

/// Draws two light beams under the coordinate of the model in `arg0->extra`,
/// picked by the current view `gGameSession->at4.loc.view`: in view 2 the
/// beams of `D_dryfield_g_r_kitchen_8017EBF0` through
/// `func_dryfield_g_r_kitchen_8017D9FC`, in view 3 those of
/// `D_dryfield_g_r_kitchen_8017EC08` through
/// `func_dryfield_g_r_kitchen_8017E27C`. Any other view draws nothing.
void func_dryfield_g_r_kitchen_8017EB04(Task* arg0)
{
    GsCOORDINATE2* coord;

    coord = ((TmdObject*)arg0->extra)->coords;
    if (gGameSession->at4.loc.view == 2) {
        func_dryfield_g_r_kitchen_8017D9FC(coord, &D_dryfield_g_r_kitchen_8017EBF0[0], &D_dryfield_g_r_kitchen_8017EBF0[-1], 0x100);
        func_dryfield_g_r_kitchen_8017D9FC(coord, &D_dryfield_g_r_kitchen_8017EBF0[2], &D_dryfield_g_r_kitchen_8017EBF0[1], 0x100);
    } else if (gGameSession->at4.loc.view == 3) {
        func_dryfield_g_r_kitchen_8017E27C(coord, &D_dryfield_g_r_kitchen_8017EC08[0], &D_dryfield_g_r_kitchen_8017EC08[1], 0x100);
        func_dryfield_g_r_kitchen_8017E27C(coord, &D_dryfield_g_r_kitchen_8017EC08[2], &D_dryfield_g_r_kitchen_8017EC08[3], 0x100);
    }
}
