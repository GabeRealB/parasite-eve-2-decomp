#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

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
#include "main/tmd.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

extern s32 func_80179B14(RoomEventMsg* in, RoomEventMsg* out);

extern s16 D_80071076;
extern s32 D_8011572C;
extern s32 D_80115750;
extern s32 D_80115758;
extern u8  D_80115690;

extern TaskDesc D_shelter_1f_vehicular_airlock_80182028;

/// The room's message table, which its cap scripts index.
extern GpMsgEntry D_shelter_1f_vehicular_airlock_80182034[];

extern SVECTOR D_shelter_1f_vehicular_airlock_8018205C[];
extern SVECTOR D_shelter_1f_vehicular_airlock_8018206C[];

/// The trail's two ends as offsets from the parent coordinate: `[0]` places
/// the object itself and `[1]` the trail's far end.
extern SVECTOR D_shelter_1f_vehicular_airlock_801820EC[];

/// The far end's offset, `D_shelter_1f_vehicular_airlock_801820EC[1]` reached
/// by its own name, as the task does on every tick after the first.
extern SVECTOR D_shelter_1f_vehicular_airlock_801820F4;

/// Spawn argument of the helper task 0x31 the room's event task starts.
extern GpFadeWork D_shelter_1f_vehicular_airlock_80182AA0;

/// The message and event the message handler latched for the room's event
/// task.
extern RoomEventMsg     D_shelter_1f_vehicular_airlock_80182AA8;
extern s8               D_shelter_1f_vehicular_airlock_80182AB0;
extern RoomLatchedEvent D_shelter_1f_vehicular_airlock_80182AB4;

void func_shelter_1f_vehicular_airlock_8017DC80(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_shelter_1f_vehicular_airlock_8017E468(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_1f_vehicular_airlock_8017E80C(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_shelter_1f_vehicular_airlock_8017EF60(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_shelter_1f_vehicular_airlock_8017F38C(GsCOORDINATE2* arg0, s32 arg1, u8* rgb);
void func_shelter_1f_vehicular_airlock_8017FC10(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, s16 arg2, s16 arg3);
void func_shelter_1f_vehicular_airlock_80180290(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);

/// Sets bit 0x80 of the task's model flags while the 2-bit game flag its spawn
/// argument names reads 2, and clears it otherwise.
void func_shelter_1f_vehicular_airlock_8017D5E4(Task* task)
{
    TmdObject* obj = task->extra;

    if (Gp_GetCurBit2Flag(((RoomFlagModelArg*)task->spawnArg2)->flagId) == 2) {
        obj->flags |= 0x80;
    } else {
        obj->flags &= ~0x80;
    }
}

/// The room's own event task, spawned by its message handler. State 0 runs
/// the latched event's CAP command; state 1 waits for it to finish and, when
/// the event asks for it, starts helper task 0x31; states 2 and 3 play the
/// event's stage sound and wait for it; state 4 writes the latched message's
/// destination into the save data and hands over to task type 0x11.
void func_shelter_1f_vehicular_airlock_8017D644(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_shelter_1f_vehicular_airlock_80182AB4.capCmd, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_shelter_1f_vehicular_airlock_80182AB4.fade != 0) {
                    D_shelter_1f_vehicular_airlock_80182AA0.field_0 = 0;
                    D_shelter_1f_vehicular_airlock_80182AA0.field_1 = 0;
                    D_shelter_1f_vehicular_airlock_80182AA0.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, (s32)&D_shelter_1f_vehicular_airlock_80182AA0);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_shelter_1f_vehicular_airlock_80182AB4.stageSnd != 0) {
                Gp_EnqueueStageSnd6(D_shelter_1f_vehicular_airlock_80182AB4.stageSnd, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_shelter_1f_vehicular_airlock_80182AB4.stageSnd)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_1f_vehicular_airlock_80182AA8.msgId;
            Mc_SaveData.at4.loc.warp = D_shelter_1f_vehicular_airlock_80182AA8.field_2;
            Mc_SaveData.at4.loc.room = D_shelter_1f_vehicular_airlock_80182AA8.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

static __inline__ s32 _shelter1fVehicularAirlockStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event)
{
    D_shelter_1f_vehicular_airlock_80182AB0 = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->field_5 == 0) {
            D_shelter_1f_vehicular_airlock_80182AA8 = *dst;
            D_shelter_1f_vehicular_airlock_80182AB4 = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_shelter_1f_vehicular_airlock_80182028, 0, 0, 0);
            D_shelter_1f_vehicular_airlock_80182AB0 = 1;
        }
        return 2;
    }
    return 1;
}

s32 func_shelter_1f_vehicular_airlock_8017D7DC(Task* task, s32 msgId, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomLatchedEvent event;

    *out = *in;
    func_80179B14(in, out);
    if (in->msgId == 3) {
        if (GameFlag_GetNibble(0xB2) == 0) {
            if (in->field_5 == 0) {
                Gp_SetNibbleIf(in->field_6, 2);
                Gp_RunCapCmd1(2);
            }
            return 0;
        }
        event.capCmd   = 4;
        event.stageSnd = 0x55020003;
        event.flagId   = 0x15B;
        event.fade     = 0;
        return _shelter1fVehicularAirlockStartEvent(out, &event);
    }
    if (in->msgId == 5) {
        event.capCmd   = 6;
        event.stageSnd = 0x55020001;
        event.flagId   = 0x15A;
        event.fade     = 0;
        return _shelter1fVehicularAirlockStartEvent(out, &event);
    }
    return 1;
}

s32 func_shelter_1f_vehicular_airlock_8017D988(void)
{
    return 0;
}

s32 func_shelter_1f_vehicular_airlock_8017D990(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 3) {
        if (Gp_GetCurBit2Flag(6) == 2 && GameFlag_GetNibble(0x7A) >= 6) {
            arg2 = 5;
        }
        Gp_SpawnIfCapIdle(arg2, 0);
    }
    return 0;
}

s32 func_shelter_1f_vehicular_airlock_8017D9F4(void)
{
    return 0;
}

/// State 0 of the room's message task: parks the room's message table in
/// `Task::msgTable`, publishes the task in pointer slot 7 and advances to
/// state 1.
void func_shelter_1f_vehicular_airlock_8017D9FC(Task* task)
{
    task->msgTable = D_shelter_1f_vehicular_airlock_80182034;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// State 1 of the room's message task: does nothing.
void func_shelter_1f_vehicular_airlock_8017DA40(Task* task)
{
}

/// The message task's three state handlers: publishing the room's message
/// table, idling and `taskKill`.
const TaskFuncTable3 D_shelter_1f_vehicular_airlock_8017D5D8 = {
    { func_shelter_1f_vehicular_airlock_8017D9FC, func_shelter_1f_vehicular_airlock_8017DA40, taskKill },
};

/// Runs the room's message task through its three states: publishing the
/// room's message table (`func_shelter_1f_vehicular_airlock_8017D9FC`), idling
/// (`func_shelter_1f_vehicular_airlock_8017DA40`) and `taskKill`. The table is
/// copied onto the stack first, so the call goes through a local copy rather
/// than the rodata.
void func_shelter_1f_vehicular_airlock_8017DA48(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_1f_vehicular_airlock_8017D5D8;
    sp.funcs[task->state](task);
}

void func_shelter_1f_vehicular_airlock_8017DAA0(Task* task)
{
    u8       view;
    SVECTOR* p;

    if (task->state == 0) {
        D_80115758  = 0x601D7;
        D_8011572C  = 0x601F3;
        D_80115750  = 0x6020F;
        task->state = 1;
    }

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
            p = D_shelter_1f_vehicular_airlock_8018206C;
            SOFT_TOUCH_REG(p);
            func_shelter_1f_vehicular_airlock_8017DC80(&p[0], 0x200, 0x800, 0x210);
            func_shelter_1f_vehicular_airlock_8017DC80(&p[2], 0x200, 0x800, 0x210);
            func_shelter_1f_vehicular_airlock_8017DC80(&p[6], 0x200, 0, 0x210);
            func_shelter_1f_vehicular_airlock_8017DC80(&p[8], 0x200, 0, 0x210);
            func_shelter_1f_vehicular_airlock_8017E468(&p[12], 0x200, 0x200);
            break;
        case 3:
            p = D_shelter_1f_vehicular_airlock_8018205C;
            func_shelter_1f_vehicular_airlock_8017DC80(&p[0], 0x200, 0x800, 0x210);
            func_shelter_1f_vehicular_airlock_8017DC80(&p[2], 0x200, 0x800, 0x210);
            func_shelter_1f_vehicular_airlock_8017DC80(&p[6], 0x200, 0, 0x210);
            func_shelter_1f_vehicular_airlock_8017DC80(&p[8], 0x200, 0, 0x210);
            func_shelter_1f_vehicular_airlock_8017DC80(&p[12], 0x200, 0, 0x111);
            if (GameFlag_GetNibble(0xB2) == 1) {
                func_shelter_1f_vehicular_airlock_8017E80C(&p[15], 0x804, 0x140, 0x21);
                func_shelter_1f_vehicular_airlock_8017E80C(&p[16], 0xC0, 0x120, 0x210);
                func_shelter_1f_vehicular_airlock_8017E80C(&p[17], -0xC0, 0x120, 0x210);
            } else {
                func_shelter_1f_vehicular_airlock_8017E468(&p[15], 0x180, 0x21);
                func_shelter_1f_vehicular_airlock_8017E468(&p[16], 0x140, 0x210);
                func_shelter_1f_vehicular_airlock_8017E468(&p[17], 0x140, 0x210);
            }
            break;
    }
}

/// Draws a gouraud capsule between the view-space points `arg0[0]` and
/// `arg0[1]`: a half-disc at each end, radius `(s16)arg1 * 64` over the end's
/// OTZ, joined by a band, all rotated by `(s16)arg2`. The inner vertices take
/// the colour coded in `arg3` (red from bits 8-15, green from bits 4-5, blue
/// from bits 0-1), each scaled by a blend byte that alternates with the frame
/// counter, and the outer rim is black. Nothing is drawn when the second
/// point's OTZ is below 0x11.
void func_shelter_1f_vehicular_airlock_8017DC80(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    u8*                head;
    RoomDraw11Scratch* block;
    POLY_G4*           prim;
    POLY_G4*           p;
    SVECTOR*           p1;
    s32                ang;
    s32                t;
    s32                t2;
    s32                t3;
    s32                packed;
    s32                extent;
    s32                r0;
    s32                r1;
    s32                base;
    u8                 blend;
    u8                 r;
    u8                 g;
    u8                 b;

    {
        void** scratch;
        u8*    tmp;

        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        tmp      = head - 0x18;
        *scratch = tmp;
        p1       = arg0 + 1;
        block    = (RoomDraw11Scratch*)tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw11Scratch*)(head - 0x18))->sx0);
    gte_stszotz(&block->otz0);
    gte_ldv0(p1);
    gte_rtps();
    gte_stsxy(&((RoomDraw11Scratch*)(head - 0x18))->sx1);
    gte_stszotz(&((RoomDraw11Scratch*)(head - 0x18))->otz1);
    if (block->otz1 >= 0x11) {
        if (((RoomDraw11Scratch*)(head - 0x18))->otz0 < 0x10) {
            ((RoomDraw11Scratch*)(head - 0x18))->otz0 = 0x10;
        }
        extent    = (s16)arg1 * 64;
        r0        = extent / ((RoomDraw11Scratch*)(head - 0x18))->otz0;
        r1        = extent / block->otz1;
        packed    = arg3 << 16;
        blend     = (((u8)gDisplayState.animFrame & 1) * 8) | 0x20;
        r         = blend * (packed >> 24);
        g         = blend * ((packed >> 20) & 3);
        base      = (s16)arg2;
        b         = blend * (arg3 & 3);
        ang       = 0;
        block->r0 = r0;
        block->r1 = r1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            p        = prim;
            p->r2    = r;
            p->g2    = g;
            prim->b2 = b;
            p->r3    = 0;
            p->g3    = 0;
            p->b3    = 0;
            p->x0    = block->sx0 + ((block->r0 * rsin(base + ang)) >> 12);
            p->y0    = block->sy0 + ((block->r0 * rcos(base + ang)) >> 12);
            t        = ang + 0x200;
            prim->x1 = block->sx0 + ((block->r0 * rsin(base + t)) >> 12);
            prim->y1 = block->sy0 + ((block->r0 * rcos(base + t)) >> 12);
            t2       = ang + 0x400;
            p->x2    = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx0 + ((block->r0 * rsin(base + t2)) >> 12);
            prim->y3 = block->sy0 + ((block->r0 * rcos(base + t2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, r, g, b);
            prim->x0 = block->sx0 + ((block->r0 * rsin(base + (ang * 2))) >> 12);
            prim->y0 = block->sy0 + ((block->r0 * rcos(base + (ang * 2))) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(base + (ang * 2))) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(base + (ang * 2))) >> 12);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
            addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            SCHED_BARRIER();
            t3             = ang - 0x1000;
            prim           = (POLY_G4*)gGpuPrimCursor;
            t              = ang - 0x1000;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx1 + ((block->r1 * rsin(base - t3)) >> 12);
            prim->y0 = block->sy1 + ((block->r1 * rcos(base - t)) >> 12);
            t        = ang - 0xE00;
            prim->x1 = block->sx1 + ((block->r1 * rsin(base - t)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(base - t)) >> 12);
            t        = ang - 0xC00;
            prim->x2 = block->sx1;
            prim->y2 = block->sy1;
            t        = base - t;
            prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
            prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
        } while (ang < 0x800);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

/// Draws a fan of four gouraud quads around the view-space point `arg0` when
/// its OTZ is at least 0x11: the centre takes the colour coded in `arg2` (red
/// from bits 8-15, green from bits 4-5, blue from bits 0-1), each scaled by a
/// blend byte that alternates with the frame counter, and the rim, at radius
/// `(s16)arg1 * 64` over the OTZ, is black.
void func_shelter_1f_vehicular_airlock_8017E468(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw25Scratch* block;
    POLY_G4*           prim;
    u8*                ds_ptr;
    DisplayState*      ds;
    s32                radius;
    s32                ang;
    s32                t;
    s32                t2;
    s32                packed;
    u8                 blend;
    u8                 r;
    u8                 g;
    u8                 b;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - 0xC);
        SOFT_TOUCH_REG(tmp);
        block = (RoomDraw25Scratch*)tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw25Scratch*)(head - 0xC))->sx);
    gte_stszotz(&block->otz);
    if (((RoomDraw25Scratch*)(head - 0xC))->otz >= 0x11) {
        radius        = ((s16)arg1 * 64) / ((RoomDraw25Scratch*)(head - 0xC))->otz;
        ds_ptr        = (u8*)&gDisplayState;
        packed        = arg2 << 16;
        blend         = ((*(u8*)&((DisplayState*)ds_ptr)->animFrame & 1) * 8) | 0x20;
        r             = blend * (packed >> 24);
        g             = blend * ((packed >> 20) & 3);
        b             = blend * (arg2 & 3);
        ang           = 0;
        ds            = (DisplayState*)ds_ptr;
        block->radius = radius;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x200;
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0xC;
}

/// Projects the world-space point `arg0` through `Gfx_ViewWorldMtx` and, if
/// the resulting OTZ is at least 0x11, queues two gouraud `POLY_G4` diamonds
/// and two gouraud `LINE_G3` diagonals around the projected centre. `arg2` is a
/// signed half-extent; the on-screen radius is `(s16)arg2 * 32 / otz`. `arg1`
/// scales the display frame counter into `rsin`, giving a pulse of
/// `rsin(...) / 68 + 0x3C`; `arg3` packs the lit vertex's colour as per-channel
/// multipliers of that pulse, red in bits 8 and up, green in bits 4-5 and blue
/// in bits 0-1.
void func_shelter_1f_vehicular_airlock_8017E80C(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    u8*               head;
    RoomShaftScratch* block;
    POLY_G4*          prim;
    LINE_G3*          line;
    s32               sine;
    u8                pulse;
    s32               r;
    s32               g;
    s32               b;
    s32               radius;
    s32               i;
    s32               t1;
    s32               t2;
    s32               twice;
    u16               sx;
    u16               sy;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - 0x14);
        SOFT_TOUCH_REG(tmp);
        block = (RoomShaftScratch*)tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomShaftScratch*)(head - 0x14))->sx);
    gte_stszotz(&block->otz);
    if (((RoomShaftScratch*)(head - 0x14))->otz >= 0x11) {
        sine             = rsin(gDisplayState.animFrame * (s16)arg1);
        radius           = ((s16)arg2 * 32) / ((RoomShaftScratch*)(head - 0x14))->otz;
        pulse            = sine / 68 + 0x3C;
        r                = pulse * ((s16)arg3 >> 8);
        g                = pulse * (((s16)arg3 >> 4) & 3);
        b                = pulse * (arg3 & 3);
        i                = 0;
        block->halfWidth = radius;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx - (u16)block->halfWidth;
            sx       = block->sx;
            prim->x2 = sx;
            prim->x1 = sx;
            prim->x3 = block->sx + (u16)block->halfWidth;
            sy       = block->sy;
            prim->y3 = sy;
            prim->y2 = sy;
            prim->y0 = sy;
            twice    = i * 2;
            prim->y1 = (block->sy - (u16)block->halfWidth) + (block->halfWidth * twice);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            i++;
        } while (i < 2);

        i = 0;
        do {
            line           = (LINE_G3*)gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            setLineG3(line);
            setRGB0(line, 0, 0, 0);
            setRGB1(line, r, g, b);
            setRGB2(line, 0, 0, 0);
            t1       = i * 3 - 1;
            t2       = i + 1;
            line->x0 = block->sx + (block->halfWidth * t1);
            line->y0 = block->sy - (block->halfWidth * t2);
            line->x1 = block->sx;
            line->y1 = block->sy;
            line->x2 = block->sx - (block->halfWidth * t1);
            line->y2 = block->sy + (block->halfWidth * t2);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    line);
            Gp_AddTpageShift((P_TAG*)line, 1, block->otz);
            i = t2;
        } while (i < 2);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
}

/// Flash effect task on the object's coordinate. Over `spawnArg1` ticks it
/// brightens, drawing two fans and a ring in a red-dominant colour that grows
/// each tick; on the last one it flashes the screen. It then fades out as a
/// shrinking billboard, 0x10 a tick, and releases its work block. It also
/// releases it once the room's event state reaches 4, and is frozen while the
/// event state is non-zero.
void func_shelter_1f_vehicular_airlock_8017ECBC(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    u8             rgb[3];

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
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
                rgb[1] = (u16)work->scale >> 2;
                rgb[2] = (u16)work->scale >> 1;
                func_shelter_1f_vehicular_airlock_8017F38C(coord, (s16)work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_shelter_1f_vehicular_airlock_8017F38C(coord, (s16)((u16)work->angle * 2), rgb);
                func_shelter_1f_vehicular_airlock_8017EF60(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
                if (task->spawnArg1 == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    rgb[0]      = work->scale;
                    rgb[1]      = (u16)work->scale >> 2;
                    rgb[2]      = (u16)work->scale >> 1;
                    Gp_DrawFadeQuad(rgb, 1);
                }
                break;
            case 2:
                if ((s16)work->scale >= 0x11) {
                    rgb[0] = work->scale;
                    rgb[1] = (u16)work->scale >> 2;
                    rgb[2] = (u16)work->scale >> 1;
                    func_shelter_1f_vehicular_airlock_80180290(coord, (s16)((s16)work->angle * 3), rgb);
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

/// Draws a ring of sixteen gouraud quads around the screen position of the
/// coordinate's world translation, unless the projection flags an error. The
/// vertices at radius `(s16)arg1 * 64` over the depth are black and those at
/// `(s16)(arg1 + arg2) * 64` over the depth take `rgb`.
void func_shelter_1f_vehicular_airlock_8017EF60(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb)
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
    vx = *(u16*)&arg0->workm.t[0];
    USE_REG(vx);
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomDraw02Scratch*)tmp;
    }
    block->vec.vx = vx;
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
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
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 12);
            t        = ang + 0x100;
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 12);
            prim->x2 = *(u16*)&block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = *(u16*)&block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = *(u16*)&block->sx + ((block->rInner * rsin(t)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rInner * rcos(t)) >> 12);
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
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

/// Draws a fan of eight gouraud quads around the screen position of the
/// coordinate's world translation, unless the projection flags an error: the
/// centre takes `rgb` and the rim, at radius `(s16)arg1 * 64` over the depth,
/// is black.
void func_shelter_1f_vehicular_airlock_8017F38C(GsCOORDINATE2* arg0, s32 arg1, u8* rgb)
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
        vx                                          = *(u16*)&arg0->workm.t[0];
        ((RoomDraw04Scratch*)(head - 0x18))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x18;
        block = (RoomDraw04Scratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
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
            prim->x0 = *(u16*)&block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = *(u16*)&block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            SOFT_USE_REG(t2);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

/// Trail effect task. On its first tick it allocates two eight-slot histories
/// of view-space coordinates and fills both with the trail's two ends; then
/// every tick it records the current ends in the next slot and draws the band
/// between the histories, until its age reaches `spawnArg1`. It is frozen
/// while the room's event state is 2 or more.
void func_shelter_1f_vehicular_airlock_8017F720(Task* task)
{
    GsCOORDINATE2  coord;
    GsCOORDINATE2* coords;
    GsCOORDINATE2* objCoord;
    GsCOORDINATE2* dst;
    GpEffWork*     work;
    SVECTOR*       vec;
    s32            i;

    coords   = (GsCOORDINATE2*)task->work;
    work     = (GpEffWork*)task->spawnArg2;
    objCoord = ((TmdObject*)task->extra)->coords;

    if (Gp_State1C->eventState < 2) {
        work->age++;
        switch (task->state) {
            case 0:
                coords = (GsCOORDINATE2*)memCalloc(0x500, 0);
                if (coords == NULL) {
                    work->age = 0;
                    return;
                }
                task->work           = (TaskIdMap*)coords;
                objCoord->sub        = work->parent;
                objCoord->coord.t[0] = D_shelter_1f_vehicular_airlock_801820EC[0].vx;
                objCoord->coord.t[1] = D_shelter_1f_vehicular_airlock_801820EC[0].vy;
                objCoord->coord.t[2] = D_shelter_1f_vehicular_airlock_801820EC[0].vz;
                objCoord->flg        = 0;
                Gp_UpdateCoord(objCoord);
                task->state      = 1;
                coord.sub        = work->parent;
                vec              = &D_shelter_1f_vehicular_airlock_801820EC[1];
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
                coord.coord.t[0] = D_shelter_1f_vehicular_airlock_801820F4.vx;
                coord.coord.t[1] = D_shelter_1f_vehicular_airlock_801820F4.vy;
                coord.coord.t[2] = D_shelter_1f_vehicular_airlock_801820F4.vz;
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
                func_shelter_1f_vehicular_airlock_8017FC10(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1 && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the band between two eight-slot coordinate histories as seven gouraud
/// quads, walking back from slot `arg2`; each quad joins two consecutive slots
/// of `arg0` and `arg1`. Brightness falls by 9 per quad from 0x40, and `arg3`
/// scales it per channel: red by `arg3 >> 8`, green by bits 4-5 and blue by
/// bits 0-1. A quad whose projection flags an error is skipped.
void func_shelter_1f_vehicular_airlock_8017FC10(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, s16 arg2, s16 arg3)
{
    RoomDraw03Scratch* blk;
    GsCOORDINATE2*     a;
    GsCOORDINATE2*     b;
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

        tmp                     = (u8*)*(void**)G_SCRATCH_HEAD - sizeof(RoomDraw03Scratch);
        blk                     = (RoomDraw03Scratch*)tmp;
        *(void**)G_SCRATCH_HEAD = tmp;
    }
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    i = 0;
    do {
        j            = arg2 - i;
        i0           = j & 7;
        a            = &arg0[i0];
        blk->v[0].vx = *(u16*)&a->workm.t[0];
        j            = j - 1;
        blk->v[0].vy = *(u16*)&a->workm.t[1];
        i1           = j & 7;
        blk->v[0].vz = *(u16*)&a->workm.t[2];
        b            = &arg1[i0];
        blk->v[1].vx = *(u16*)&b->workm.t[0];
        blk->v[1].vy = *(u16*)&b->workm.t[1];
        blk->v[1].vz = *(u16*)&b->workm.t[2];
        a            = &arg0[i1];
        blk->v[2].vx = *(u16*)&a->workm.t[0];
        blk->v[2].vy = *(u16*)&a->workm.t[1];
        blk->v[2].vz = *(u16*)&a->workm.t[2];
        b            = &arg1[i1];
        blk->v[3].vx = *(u16*)&b->workm.t[0];
        blk->v[3].vy = *(u16*)&b->workm.t[1];
        blk->v[3].vz = *(u16*)&b->workm.t[2];
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
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(RoomDraw03Scratch);
}

/// Burst effect task on the object's coordinate. On its first tick it spawns
/// effect 0x60076, then either (`spawnArg1` non-zero) a spray of 0x60070
/// sparks in random directions for seven ticks, or two 0x6007C effects and
/// seven ticks of a fading, widening ring. It then releases its work block,
/// as it does once the room's event state reaches 4; it is frozen while the
/// event state is non-zero.
void func_shelter_1f_vehicular_airlock_80180008(Task* task)
{
    GsCOORDINATE2* objCoord;
    GpEffWork*     work;
    u8             rgb[4];

    objCoord = ((TmdObject*)task->extra)->coords;
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
            rgb[1]       = (u16)work->angle >> 1;
            rgb[2]       = (u16)work->angle >> 2;
            func_shelter_1f_vehicular_airlock_8017EF60(objCoord, 0x100, 0x100, rgb);
            func_shelter_1f_vehicular_airlock_8017EF60(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Draws a star-shaped glow around the screen position of the coordinate's
/// world translation, unless the projection flags an error: a fan of gouraud
/// wedges at radius `arg1 * 64` over the depth in half of `arg2`'s colour, a
/// second at half that radius in the full colour, and four cross wedges from
/// an inner radius of `arg1 * 8` over the depth. Every rim is black.
void func_shelter_1f_vehicular_airlock_80180290(GsCOORDINATE2* arg0, s16 arg1, u8* arg2)
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
        vx                                             = *(u16*)&arg0->workm.t[0];
        ((RoomBillboardScratch*)(head - 0x1C))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomBillboardScratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
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
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rOuter * rcos(t2)) >> 12);
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
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = *(u16*)&block->sy + ((block->rOuter * rcos(t2)) >> 13);
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
            prim->x0 = *(u16*)&block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = *(u16*)&block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = *(u16*)&block->sy + ((block->rInner * rcos(u)) >> 13);
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
            prim->x0 = *(u16*)&block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = *(u16*)&block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}
