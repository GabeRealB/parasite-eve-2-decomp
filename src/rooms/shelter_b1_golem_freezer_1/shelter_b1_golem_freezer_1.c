#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

extern s32 D_80070F70;

#define GOLEM_RAND() ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16)

extern void func_80131E70(void);
extern void func_80131E24(void);
extern s32  func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

/// The room's message table, installed on the room task.
extern GpMsgEntry D_shelter_b1_golem_freezer_1_8017E6A8[];

extern s16          D_shelter_b1_golem_freezer_1_8017E6D0;
extern s16          D_shelter_b1_golem_freezer_1_8017E6D2;
extern GpGridParams D_shelter_b1_golem_freezer_1_8017E714;
extern GpGridParams D_shelter_b1_golem_freezer_1_8017E9C0;
extern SVECTOR      D_shelter_b1_golem_freezer_1_8017E738[];
extern SVECTOR      D_shelter_b1_golem_freezer_1_8017E740[];

void func_shelter_b1_golem_freezer_1_8017D744(s32 arg0);
void func_shelter_b1_golem_freezer_1_8017D7CC(GpCoord* arg0, s16* arg1);
void func_shelter_b1_golem_freezer_1_8017DC5C(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b1_golem_freezer_1_8017E254(GpCoord* coord, u16 arg1, s16 arg2, s16 arg3);

/// Message-table handler for message 0x13F1: does nothing and answers 0.
s32 func_shelter_b1_golem_freezer_1_8017D5D0(void)
{
    return 0;
}

/// Message-table handler for message 0x13EE: copies the incoming record onto
/// the outgoing one and passes both on to `func_80179A04`. Always answers 1.
s32 func_shelter_b1_golem_freezer_1_8017D5D8(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    return 1;
}

/// Message-table handler for message 0x13F0: does nothing and answers 0.
s32 func_shelter_b1_golem_freezer_1_8017D61C(void)
{
    return 0;
}

/// Message-table handler for message 0x13EF: when the message's `field_2` is 1
/// and the session's place is 0x15, calls `func_80131E70`. Always answers 0.
s32 func_shelter_b1_golem_freezer_1_8017D624(s32 arg0, s32 arg1, RoomEventMsg* msg)
{
    if (msg->field_2 == 1 && gGameSession->at4.loc.place == 0x15) {
        func_80131E70();
    }
    return 0;
}

/// The room task's first state: installs the room's message table, takes game
/// pointer slot 7, calls `func_80131E24` while the session's place is 0x15,
/// runs `func_shelter_b1_golem_freezer_1_8017D744` and moves on to the next
/// state.
void func_shelter_b1_golem_freezer_1_8017D66C(Task* arg0)
{
    arg0->msgTable = D_shelter_b1_golem_freezer_1_8017E6A8;
    Game_SetPtrSlot(arg0, 7);
    if (gGameSession->at4.loc.place == 0x15) {
        func_80131E24();
    }
    func_shelter_b1_golem_freezer_1_8017D744(0);
    arg0->state = arg0->state + 1;
}

/// The room task's idle state. It reserves a stack frame it never uses.
void func_shelter_b1_golem_freezer_1_8017D6DC(Task* task)
{
    char pad[0x10];
}

/// State handlers of the room task `func_shelter_b1_golem_freezer_1_8017D6EC`
/// runs: its setup, an idle state, and `taskKill`.
const TaskFuncTable3 D_shelter_b1_golem_freezer_1_8017D5C4 = {
    { func_shelter_b1_golem_freezer_1_8017D66C, func_shelter_b1_golem_freezer_1_8017D6DC, taskKill }
};

/// Runs one tick of the room task through the three-state table
/// `D_shelter_b1_golem_freezer_1_8017D5C4`, copying the table onto the stack
/// and calling the entry for the task's current state.
void func_shelter_b1_golem_freezer_1_8017D6EC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_golem_freezer_1_8017D5C4;
    sp.funcs[task->state](task);
}

void func_shelter_b1_golem_freezer_1_8017D744(s32 arg0)
{
    Task* slot   = (Task*)Gp_LookupSlot4(0);
    Task* task   = slot;
    s32   isNull = (slot == NULL);

    if (isNull) {
        task = gameGetPtrSlot(3);
    }
    if (slot != NULL) {
        if (gGameSession->at4.loc.place == 0x15) {
            D_shelter_b1_golem_freezer_1_8017E6D2 = 0;
        } else {
            D_shelter_b1_golem_freezer_1_8017E6D2 = 0x2710;
        }
    } else {
        D_shelter_b1_golem_freezer_1_8017E6D2 = 0x2710;
    }
    func_shelter_b1_golem_freezer_1_8017D7CC(task->extra.tmd->coords, &D_shelter_b1_golem_freezer_1_8017E6D0);
}

void func_shelter_b1_golem_freezer_1_8017D7CC(GpCoord* coord, s16* arg1)
{
    MATRIX        m;
    long          flag;
    s32           i;
    SVECTOR*      d;
    SVECTOR*      s;
    GpGridParams* dst = &D_shelter_b1_golem_freezer_1_8017E9C0;
    GpGridParams* src = &D_shelter_b1_golem_freezer_1_8017E714;

    i = 0;
    do {
        dst->field_4[i].vx = src->field_4[i].vx;
        dst->field_4[i].vy = src->field_4[i].vy;
        dst->field_4[i].vz = src->field_4[i].vz;
        dst->field_C[i]    = src->field_C[i];
        i++;
    } while (i <= 0);

    for (i = 0; i < 4; i++) {
        dst->field_8[i].vx = src->field_8[i].vx;
        dst->field_8[i].vy = src->field_8[i].vy;
        dst->field_8[i].vz = src->field_8[i].vz;
    }

    m = coord->coord;

    if (arg1 != NULL) {
        m.t[0] += arg1[0];
        m.t[1] += arg1[1];
        m.t[2] += arg1[2];
    }

    d = dst->field_4;
    s = src->field_4;
    i = 0;
    do {
        gte_SetRotMatrix(&m);
        gte_ldv0(s);
        s++;
        gte_rtv0();
        gte_stsv(d);
        d++;
        i++;
    } while (i <= 0);

    gte_SetRotMatrix(&m);
    gte_SetTransMatrix(&m);
    d = dst->field_8;
    s = src->field_8;
    for (i = 0; i < 4; i++) {
        RotTransSV(s++, d++, &flag);
    }
}

void func_shelter_b1_golem_freezer_1_8017DA7C(void)
{
    SVECTOR pos;
    s32     i;
    s32     ang;
    s32     r;

    if (!(D_80070F70 & 3)) {
        for (i = 0; i < 9; i++) {
            ang    = GOLEM_RAND() & 0xFFF;
            r      = (GOLEM_RAND() & 0x3C0) + 0x40;
            pos.vx = D_shelter_b1_golem_freezer_1_8017E738[i + 2].vx + ((r * rcos(ang)) >> 12);
            pos.vy = -(GOLEM_RAND() & 0xFF);
            pos.vz = D_shelter_b1_golem_freezer_1_8017E738[i + 2].vz + ((r * rsin(ang)) >> 12);
            Gp_SpawnEff(0x601A6, NULL, (GOLEM_RAND() & 0x10FF) + 0x85400, &pos);
        }
    }
    switch (Gp_GetViewIndex() & 0xFF) {
        case 3:
            func_shelter_b1_golem_freezer_1_8017DC5C(D_shelter_b1_golem_freezer_1_8017E738, 0x200, 0x421);
            break;
        case 4:
            func_shelter_b1_golem_freezer_1_8017DC5C(D_shelter_b1_golem_freezer_1_8017E738, 0x200, 0x210);
            break;
        case 2:
        case 5:
            func_shelter_b1_golem_freezer_1_8017DC5C(D_shelter_b1_golem_freezer_1_8017E740, 0x200, 0x421);
            break;
    }
}

/// Draws a glow disc at the world-space point `arg0`: projects it through
/// `Gfx_ViewWorldMtx` and, when the GTE flag is non-negative, queues four
/// gouraud `POLY_G4` wedges around the projected centre. `arg1` is a signed
/// half-extent; the on-screen radius is `(s16)arg1 * 64 / otz`. `arg2` packs
/// three RGB nibbles for the centre vertex, OR'd with a flicker of
/// `(animFrame & 1) * 8`; the rim vertices are black.
void func_shelter_b1_golem_freezer_1_8017DC5C(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    register u8*       tmp asm("v0");
    RoomDraw13Scratch* block;
    POLY_G4*           prim;
    DisplayState*      ds;
    s32                ang;
    s32                t;
    s32                t2;
    s32                packed;
    s32                blend;
    s32                tr;
    s32                tg;
    u8                 r;
    u8                 g;
    u8                 b;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    tmp      = head - 0x10;
    block    = (RoomDraw13Scratch*)tmp;
    *scratch = tmp;

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        arg1 = ((s16)arg1 * 64) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        ang  = 0;
        tmp  = (u8*)&gDisplayState;
        SOFT_TOUCH_REG(tmp);
        ds            = (DisplayState*)tmp;
        blend         = (*(u8*)&ds->animFrame & 1) * 8;
        packed        = arg2 << 16;
        tr            = (packed >> 20) & 0xF0;
        tg            = (packed >> 16) & 0xF0;
        r             = blend | tr;
        g             = blend | tg;
        b             = blend | ((arg2 & 0xF) << 4);
        block->radius = arg1;
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
    SCRATCH_POP_BYTES(0x10);
}

void func_shelter_b1_golem_freezer_1_8017DFFC(Task* task)
{
    GpEffWork* work  = task->spawnArg2;
    GpCoord*   coord = task->extra.tmd->coords;
    s32        vz;
    s16        f2a;
    u32        rng2;
    u32        rng3;

    work->age++;
    if (task->state == 0) {
        work->scale = task->spawnArg1 & 0xFFF;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        work->angle = (Gp_LcgState >> 16) & 0xFFF;

        if (task->spawnArg1 & 0xF000) {
            work->period = (task->spawnArg1 >> 12) & 0x7;
        } else {
            work->period = 1;
        }

        work->age   = 0;
        task->state = 1;

        if (task->spawnArg1 & 0xFF0000) {
            f2a = (task->spawnArg1 >> 16) & 0xFF;
        } else {
            f2a = 0x40;
        }

        work->step    = f2a;
        work->move.vy = 0;
        rng2          = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState   = rng2;
        work->move.vx = 0x80 - (((u32)rng2 >> 16) & 0xFF);
        rng3          = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState   = rng3;
        vz            = 0x80 - (((u32)rng3 >> 16) & 0xFF);
        work->move.vz = vz;
        VectorNormalSS(&work->move, &work->move);

        gte_lddp(work->step);
        gte_ldsv(&work->move);
        gte_gpf12();
        gte_stsv(&work->move);
    }

    func_shelter_b1_golem_freezer_1_8017E254(coord, work->index, work->scale, work->angle);

    coord->coord.t[0] += work->move.vx;
    coord->coord.t[1] += work->move.vy;
    coord->coord.t[2] += work->move.vz;
    coord->flg         = 0;

    if ((work->age % work->period) == 0) {
        work->index++;
        if (work->index >= 0xA) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

void func_shelter_b1_golem_freezer_1_8017E254(GpCoord* coord, u16 arg1, s16 arg2, s16 arg3)
{
    void**           scratch;
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              u0;
    s32              v0;
    s32              u1;
    s32              v1;
    s32              ang;
    s32              ang2;
    u16              vz;

    scratch                                   = (void**)G_SCRATCH_HEAD;
    head                                      = *scratch;
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = *(u16*)&coord->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = *(u16*)&coord->workm.t[1];
    vz                                        = *(u16*)&coord->workm.t[2];
    *scratch                                  = block;
    block->vec.vz                             = vz;
    vec                                       = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        if (block->otz >= 0x41) {
            prim           = (POLY_FT4*)gGpuPrimCursor;
            ang            = arg3;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2E);
            setRGB0(prim, 0x50, 0x50, 0x50);
            prim->tpage = 0x2B;
            prim->clut  = 0x43D0;
            u0          = (arg1 % 5) * 0x30;
            v0          = (arg1 / 5) * 0x30;
            u1          = u0 + 0x2F;
            v1          = v0 - 0x51;
            v0          = v0 - 0x80;
            setUV4(prim, u0, v0, u1, v0, u0, v1, u1, v1);
            block->dx = (((arg2 * 47) / block->otz) * rsin(ang)) >> 12;
            block->dy = (((arg2 * 47) / block->otz) * rcos(ang)) >> 12;
            prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
            prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
            prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
            ang2      = ang + 0x400;
            prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
            block->dx = (((arg2 * 47) / block->otz) * rsin(ang2)) >> 12;
            block->dy = (((arg2 * 47) / block->otz) * rcos(ang2)) >> 12;
            prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
            prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
            prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
            prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
        }
    }
    SCRATCH_POP_BYTES(0x1C);
}
