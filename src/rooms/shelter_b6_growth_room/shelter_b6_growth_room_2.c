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
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "rooms/room_common.h"

void func_shelter_b6_growth_room_8017E0A8(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b6_growth_room_8017E448(s16 arg0, s16 arg1);
void func_shelter_b6_growth_room_8017E7F0(GsCOORDINATE2* coord, u16 arg1, s16 arg2, u16 arg3);
void func_shelter_b6_growth_room_8017ED28(GsCOORDINATE2* coord, u16 arg1, s16 arg2, s16 arg3);

extern TaskDesc D_80135E78;
extern s32      D_80070F70;
extern SVECTOR  D_shelter_b6_growth_room_8017F258[];
extern SVECTOR  D_shelter_b6_growth_room_8017F298[];
extern SVECTOR  D_shelter_b6_growth_room_8017F2C8[];
extern SVECTOR  D_shelter_b6_growth_room_8017F300[];

/// The layout template and the live copy the reset below restores from it.
extern GpGridParams D_shelter_b6_growth_room_8017F234;
extern GpGridParams D_shelter_b6_growth_room_8017FAF0;

/// Resets the live layout lists from the template: the four-entry vector list
/// and its 12-byte records, then the eight-entry list, which is afterwards
/// raised by 0x7D0 on y when `arg0` is nonzero.
void func_shelter_b6_growth_room_8017D82C(s32 arg0)
{
    GpGridParams* dst;
    GpGridParams* src;
    SVECTOR       d;
    s32           i;

    dst = &D_shelter_b6_growth_room_8017FAF0;
    src = &D_shelter_b6_growth_room_8017F234;

    for (i = 0; i < 4; i++) {
        dst->field_4[i].vx = src->field_4[i].vx;
        dst->field_4[i].vy = src->field_4[i].vy;
        dst->field_4[i].vz = src->field_4[i].vz;
        dst->field_C[i]    = src->field_C[i];
    }

    for (i = 0; i < 8; i++) {
        dst->field_8[i].vx = src->field_8[i].vx;
        dst->field_8[i].vy = src->field_8[i].vy;
        dst->field_8[i].vz = src->field_8[i].vz;
    }

    if (arg0 == 0) {
        d.vx = 0;
        d.vy = 0;
    } else {
        d.vx = 0;
        d.vy = 0x7D0;
    }
    d.vz = 0;

    for (i = 0; i < 8; i++) {
        dst->field_8[i].vx += d.vx;
        dst->field_8[i].vy += d.vy;
        dst->field_8[i].vz += d.vz;
    }
}

void func_shelter_b6_growth_room_8017D9D8(Task* task)
{
    SVECTOR pos;
    s32     angle;
    s32     i;
    s32     z;

    if (Gp_State1C->eventState == 0) {
        if (task->spawnArg1 < 0x130 && !(D_80070F70 & 7)) {
            task->spawnArg1++;
        }
    }
    if (task->state < 6) {
        task->state = (task->spawnArg1 >> 4) + 1;
    }
    if ((u32)D_80070F70 % (task->state * 2 + 4) == 0) {
        for (i = 0; i < task->state; i++) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            angle       = ((Gp_LcgState >> 16) & 0x7FF) - 0x400;
            pos.vx      = D_shelter_b6_growth_room_8017F258[i + 30].vx + ((rcos(angle) * 1000) >> 12);
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            pos.vy      = -((s32)(Gp_LcgState >> 16) % ((task->spawnArg1 + 1) * 8));
            pos.vz      = D_shelter_b6_growth_room_8017F258[i + 30].vz + ((rsin(angle) * 1000) >> 12);
            Gp_SpawnEff(0x601A1, NULL, 0x106500, &pos);
        }
    }
    if (!(D_80070F70 & 1)) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        pos.vx      = -1000;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        pos.vy      = -1200 - (Gp_LcgState >> 16) % 400;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        z           = (Gp_LcgState >> 16) % 400 + 0xDAC;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        pos.vz      = z + ((Gp_LcgState >> 16) & 1) * 1000;
        Gp_SpawnEff(0x601A2, NULL, 0x183280, &pos);
    }
    func_shelter_b6_growth_room_8017E448(task->spawnArg1, (task->spawnArg1 >> 1) + 0x50);
    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F298[0], 0x180, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F298[1], 0x200, 0x400);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F298[6], 0x180, 0x400);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F298[7], 0x180, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F298[8], 0x180, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F298[9], 0x180, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F298[10], 0x180, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F298[11], 0x180, 0x440);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F298[12], 0x200, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F298[13], 0x200, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F298[14], 0x100, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F298[15], 0x100, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F298[20], 0x200, 0x444);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F298[21], 0x200, 0x444);
            break;
        case 3:
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F258[0], 0x200, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F258[1], 0x200, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F258[2], 0x200, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F258[3], 0x200, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F258[4], 0x200, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F258[5], 0x200, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F258[6], 0x200, 0x400);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F258[7], 0x200, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F258[12], 0x200, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F258[13], 0x200, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F258[24], 0x200, 0x444);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F258[25], 0x200, 0x444);
            break;
        case 4:
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F300[0], 0x200, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F300[1], 0x100, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F300[2], 0x100, 0x44);
            break;
        case 5:
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F258[0], 0x200, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F258[1], 0x200, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F258[2], 0x200, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F258[3], 0x200, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F258[4], 0x200, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F258[5], 0x200, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F258[6], 0x200, 0x400);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F258[7], 0x200, 0x44);
            break;
        case 6:
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F2C8[0], 0x180, 0x400);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F2C8[1], 0x180, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F2C8[2], 0x180, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F2C8[7], 0x200, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F2C8[8], 0x100, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F2C8[9], 0x100, 0x44);
            break;
        case 7:
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F298[0], 0x180, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F298[1], 0x200, 0x400);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F298[2], 0x200, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F298[3], 0x200, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F298[9], 0x180, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F298[10], 0x180, 0x44);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F298[11], 0x180, 0x440);
            func_shelter_b6_growth_room_8017E0A8(&D_shelter_b6_growth_room_8017F298[12], 0x200, 0x44);
            break;
    }
}

/// Draws a glowing disc at the world point `arg0`: projected through
/// `Gfx_ViewWorldMtx`, it becomes four `POLY_G4` wedges around the screen
/// position. `arg1` is the radius, scaled by 64 over the depth; `arg2` packs
/// the centre vertex's colour as three 4-bit channels, OR'd with the
/// frame-counter bit.
void func_shelter_b6_growth_room_8017E0A8(SVECTOR* arg0, s32 arg1, s32 arg2)
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
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
}

void func_shelter_b6_growth_room_8017E448(s16 arg0, s16 arg1)
{
    POLY_G4* prim;

    prim           = (POLY_G4*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(prim + 1);
    setPolyG4(prim);
    setRGB0(prim, 0, 0, 0);
    setRGB1(prim, 0, 0, 0);
    setRGB2(prim, arg1, arg1, arg1);
    setRGB3(prim, arg1, arg1, arg1);
    setXY4(prim, -160, 120 - arg0, 160, 120 - arg0, -160, 120, 160, 120);
    addPrim((u_long*)((((u32)(0x40 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
    Gp_AddTpageShift((P_TAG*)prim, 1, 0x40);
}

void func_shelter_b6_growth_room_8017E564(Task* task)
{
    GpEffWork*     work  = task->spawnArg2;
    GsCOORDINATE2* coord = ((TmdObject*)task->extra)->coords;
    s32            vz;
    s32            t;
    s16            f2a;
    u32            rng2;
    u32            rng3;

    work->age++;
    if (task->state == 0) {
        work->scale = (*(u16*)&task->spawnArg1) & 0xFFF;

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
        work->move.vx = 0x80 - ((rng2 >> 16) & 0xFF);
        rng3          = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState   = rng3;
        vz            = 0x80 - ((rng3 >> 16) & 0xFF);
        work->move.vz = vz;
        VectorNormalSS(&work->move, &work->move);

        gte_lddp(work->step);
        gte_ldsv(&work->move);
        gte_gpf12();
        gte_stsv(&work->move);
    }

    if (work->age < work->period * 10 - 16) {
        if (work->angle < 0x40) {
            work->angle += 4;
        }
    } else {
        t           = work->age + 16;
        work->angle = 0x40 - (t - work->period * 10) * 4;
    }

    func_shelter_b6_growth_room_8017E7F0(coord, work->index, work->scale, work->angle);

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

void func_shelter_b6_growth_room_8017E7F0(GsCOORDINATE2* coord, u16 arg1, s16 arg2, u16 arg3)
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
    s16              xy;
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
            gGpuPrimCursor = (u8*)(prim + 1);
            setlen(prim, 9);
            setcode(prim, 0x2C);
            prim->tpage = 0x2C;
            prim->clut  = 0x4386;
            u0          = (arg1 & 1) << 7;
            v0          = (arg1 >> 1) << 5;
            u1          = u0 + 0x7F;
            v1          = v0 + 0x1F;
            setRGB0(prim, arg3, arg3, arg3);
            setUV4(prim, u0, v0, u1, v0, u0, v1, u1, v1);
            setSemiTrans(prim, 1);
            block->dx = (arg2 * 127) / block->otz;
            block->dy = (arg2 * 31) / block->otz;
            xy        = *(u16*)&block->sx - *(u16*)&block->dx;
            prim->x2  = xy;
            prim->x0  = xy;
            xy        = *(u16*)&block->sx + *(u16*)&block->dx;
            prim->x3  = xy;
            prim->x1  = xy;
            xy        = *(u16*)&block->sy - *(u16*)&block->dy;
            prim->y1  = xy;
            prim->y0  = xy;
            xy        = *(u16*)&block->sy + *(u16*)&block->dy;
            prim->y3  = xy;
            prim->y2  = xy;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

void func_shelter_b6_growth_room_8017EAC8(Task* task)
{
    GpEffWork*     work  = task->spawnArg2;
    GsCOORDINATE2* coord = ((TmdObject*)task->extra)->coords;
    s32            vz;
    s16            f2a;
    u32            rng2;
    u32            rng3;

    work->age++;
    if (task->state == 0) {
        work->scale = (*(u16*)&task->spawnArg1) & 0xFFF;
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
        work->move.vx = ((rng2 >> 16) & 0x7F) + 0x40;
        rng3          = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState   = rng3;
        vz            = 0x40 - ((rng3 >> 16) & 0x7F);
        work->move.vz = vz;
        VectorNormalSS(&work->move, &work->move);

        gte_lddp(work->step);
        gte_ldsv(&work->move);
        gte_gpf12();
        gte_stsv(&work->move);
    }

    func_shelter_b6_growth_room_8017ED28(coord, work->index, work->scale, work->angle);

    coord->coord.t[0] += work->move.vx;
    coord->coord.t[1] += work->move.vy;
    coord->coord.t[2] += work->move.vz;
    coord->flg         = 0;
    work->move.vy     += 2;

    if ((work->age % work->period) == 0) {
        work->index++;
        if (work->index >= 0xA) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

void func_shelter_b6_growth_room_8017ED28(GsCOORDINATE2* coord, u16 arg1, s16 arg2, s16 arg3)
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
            gGpuPrimCursor = (u8*)(prim + 1);
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->tpage = 0x2B;
            prim->clut  = 0x4384;
            u0          = (arg1 % 5) * 0x30;
            v0          = (arg1 / 5) * 0x30;
            u1          = u0 + 0x2F;
            v1          = v0 + 0x57;
            v0          = v0 + 0x28;
            setUV4(prim, u0, v0, u1, v0, u0, v1, u1, v1);
            ang       = arg3;
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
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}
