#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

/// The room's message table, handed to its event task in state 0.
extern GpMsgEntry D_shelter_1f_airlock_8017E494[];

/// Ambient effect emitter positions for the airlock, selected by view index.
/// `D_shelter_1f_airlock_8017E4BC` / `_8017E4C4` / `_8017E4D4` are successive
/// labels into one contiguous run of `SVECTOR`s, so the per-view lists overlap.
extern SVECTOR D_shelter_1f_airlock_8017E4BC[];
extern SVECTOR D_shelter_1f_airlock_8017E4C4[];
extern SVECTOR D_shelter_1f_airlock_8017E4D4[];

void func_shelter_1f_airlock_8017D8A8(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_1f_airlock_8017E0F0(SVECTOR* arg0, s32 arg1, s32 arg2);

s32 func_shelter_1f_airlock_8017D5D0(void)
{
    return 0;
}

/// The room's handler for message 0x13EE: copies the incoming save location
/// onto the outgoing one, passes both to `func_80179B14` and returns 1.
s32 func_shelter_1f_airlock_8017D5D8(s32 arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    *out = *in;
    func_80179B14(in, out);
    return 1;
}

s32 func_shelter_1f_airlock_8017D61C(void)
{
    return 0;
}

s32 func_shelter_1f_airlock_8017D624(void)
{
    return 0;
}

/// State 0 of the room's event task: installs the room's message table,
/// publishes the task in pointer slot 7 and advances to state 1.
void func_shelter_1f_airlock_8017D62C(Task* task)
{
    task->msgTable = D_shelter_1f_airlock_8017E494;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// State 1 of the room's event task: does nothing, so the task idles here.
void func_shelter_1f_airlock_8017D670(Task* task)
{
}

/// The event task's three states: install the message table, idle, and kill.
const TaskFuncTable3 D_shelter_1f_airlock_8017D5C4 = {
    {
        func_shelter_1f_airlock_8017D62C,
        func_shelter_1f_airlock_8017D670,
        taskKill,
    },
};

/// The room's event task: runs the handler for its current state, through a
/// stack copy of the state table.
void func_shelter_1f_airlock_8017D678(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_1f_airlock_8017D5C4;
    sp.funcs[task->state](task);
}

void func_shelter_1f_airlock_8017D6D0(void)
{
    switch (Gp_GetViewIndex() & 0xFF) {
        case 3:
            func_shelter_1f_airlock_8017E0F0(&D_shelter_1f_airlock_8017E4C4[0], 0x200, 0x111);
            func_shelter_1f_airlock_8017E0F0(&D_shelter_1f_airlock_8017E4C4[1], 0x200, 0x111);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4C4[3], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4C4[5], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4C4[7], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4C4[9], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4C4[11], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4C4[17], 0x180, 0x1011);
            break;
        case 4:
            func_shelter_1f_airlock_8017E0F0(&D_shelter_1f_airlock_8017E4BC[0], 0x200, 0x111);
            func_shelter_1f_airlock_8017E0F0(&D_shelter_1f_airlock_8017E4BC[1], 0x200, 0x111);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[4], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[6], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[8], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[10], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[12], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[14], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[16], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[18], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[20], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[22], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[24], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[26], 0x180, 0x1011);
            break;
        case 5:
            func_shelter_1f_airlock_8017E0F0(&D_shelter_1f_airlock_8017E4D4[0], 0x200, 0x200);
            break;
    }
}

/// Projects two adjacent positions and draws a colored glow between them.
/// The RGB nibbles in arg2 gain an alternating frame contribution whose shift
/// is selected by bits 12..15. Uses the two-point RoomDraw08Scratch layout.
void func_shelter_1f_airlock_8017D8A8(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    RoomDraw08Scratch* block;
    POLY_G4*           prim;
    DisplayState*      ds;
    SVECTOR*           p1;
    s32                ang;
    s32                t;
    s32                conn;
    s32                t3;
    s32                t2;
    s32                limit;
    s32                angStart;
    s32                packed;
    s32                blend;
    s32                tr;
    s32                tg;
    s32                scaled;
    u8                 r;
    u8                 g;
    u8                 b;

    p1      = arg0 + 1;
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        u8* tmp;
        *scratch = (tmp = head - 0x1C);
        block    = (RoomDraw08Scratch*)tmp;
        SOFT_TOUCH_REG(block);
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw08Scratch*)(head - 0x1C))->sx0);
    gte_stflg(&((RoomDraw08Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&((RoomDraw08Scratch*)(head - 0x1C))->sx1);
        gte_stflg(&((RoomDraw08Scratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((RoomDraw08Scratch*)(head - 0x1C))->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / ((RoomDraw08Scratch*)(head - 0x1C))->otz0;
            block->r1 = scaled / block->otz1;
            ang       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &gDisplayState;
            SOFT_BARRIER();
            ang    = (s16)ang;
            packed = arg2 << 16;
            blend  = (ds->animFrame & 1) << (packed >> 28);
            tr     = (packed >> 20) & 0xF0;
            tg     = (packed >> 16) & 0xF0;
            r      = blend + tr;
            g      = blend + tg;
            b      = blend + ((arg2 & 0xF) << 4);
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
                    t    = ang - angStart;
                    t  <<= 1;
                    conn = angStart + t;
                    TOUCH_REG(conn);
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, r, g, b);
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
                    t3             = ang + 0x800;
                    prim           = (POLY_G4*)gGpuPrimCursor;
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
    SOFT_BARRIER();
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

/// Draws a glow at a world-space point: projects `arg0` through
/// `Gfx_ViewWorldMtx` and, when the resulting OTZ is at least 0x11, queues
/// four gouraud `POLY_G4` wedges around the projected centre, dark at the rim
/// and coloured at the centre. The on-screen radius is `(s16)arg1 * 64 / otz`.
/// The centre colour takes red from bits 8..15 of `arg2` and green and blue
/// from two-bit fields at bits 4 and 0, each scaled by a brightness that
/// alternates with the frame counter.
void func_shelter_1f_airlock_8017E0F0(SVECTOR* arg0, s32 arg1, s32 arg2)
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
