#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "rooms/dryfield_dilapidated_house.h"
#include "main/tmd.h"

#include <psyq/inline_c.h>

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real() __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

/// Unsigned: the original shifts the register right and then masks, so the
/// shift has to compile to `srl` rather than `sra`.
extern u32 Gp_LcgState;

void func_dryfield_dilapidated_house_80182A18(GsCOORDINATE2* coord, s16 arg1, s16 arg2);
void func_dryfield_dilapidated_house_801832A8(GsCOORDINATE2* coord, s32 arg1, s32 arg2, s32 arg3);
void func_dryfield_dilapidated_house_80182F14(GsCOORDINATE2* coord, s32 arg1, s16 arg2);
void func_dryfield_dilapidated_house_80183728(GsCOORDINATE2* coord, s16 arg1, s32 arg2, s16 arg3);
void func_dryfield_dilapidated_house_801815E8(GsCOORDINATE2* coord, s32 arg1);
void func_dryfield_dilapidated_house_8018142C(Task* task);
void func_dryfield_dilapidated_house_80180738(Task* task, SVECTOR* verts);
void func_dryfield_dilapidated_house_801803A4(Task* task, SVECTOR* verts);
void func_dryfield_dilapidated_house_801823B8(s16 slot, s16 flags);

extern DdhRoomRec D_dryfield_dilapidated_house_8018669C;

/// Steps the task's 0..0x1000 ramp by 0x44, saturating at 0x1000, and feeds the
/// distance still to run (`0x1000 - ramp`) to the room record's matrix/vertex
/// interpolator. Returns the ramp value, which the caller stores into its
/// `DdhCoordWork`.
s32 func_dryfield_dilapidated_house_80180FD8(Task* task)
{
    s32 ramp;

    ramp = task->killCountdown + 0x44;
    if (ramp >= 0x1001) {
        ramp = 0x1000;
    }
    task->killCountdown = ramp;
    func_dryfield_dilapidated_house_80180A0C(task, &D_dryfield_dilapidated_house_8018669C, 0x1000 - ramp);
    return ramp;
}

/// Rebuilds the work block's `mtx` as the identity, then composes it against
/// the parent model's `GsCOORDINATE2` chain: each node's `coord` rotation is
/// multiplied in, and its translation is rotated by the accumulated matrix and
/// added to `mtx.t`. Steps one coordinate record at a time from the head of the
/// parent's array up to the record this task's own `coord` links with `sub`.
void func_dryfield_dilapidated_house_80181028(Task* task)
{
    VECTOR         vec;
    GsCOORDINATE2* coord;
    DdhCoordWork*  work;
    GsCOORDINATE2* node;
    MATRIX*        mtx;

    coord                = (GsCOORDINATE2*)((TmdObject*)task->extra)->field_8;
    work                 = (DdhCoordWork*)task->work;
    node                 = (GsCOORDINATE2*)((TmdObject*)((Task*)task->spawnArg2)->extra)->field_8;
    mtx                  = &work->mtx;
    *(s32*)&work->mtx    = ONE;
    *(s32*)&mtx->m[0][2] = 0;
    *(s32*)&mtx->m[1][1] = ONE;
    *(s32*)&mtx->m[2][0] = 0;
    mtx->m[2][2]         = ONE;
    mtx->t[0]            = 0;
    mtx->t[1]            = 0;
    mtx->t[2]            = 0;
    do {
        ApplyMatrixLV(mtx, (VECTOR*)node->coord.t, &vec);
        mtx->t[0] += vec.vx;
        mtx->t[1] += vec.vy;
        mtx->t[2] += vec.vz;
        MulMatrix0(mtx, &node->coord, mtx);
    } while (node++ != coord->sub);
}

void func_dryfield_dilapidated_house_801810F8(TmdObject* dst, TmdObject* src)
{
    if (!(src->field_C & 0x80)) {
        dst->field_C &= ~0x80;
        return;
    }
    dst->field_C |= 0x80;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_80181134);

/// State 0 of the handler table at `D_dryfield_dilapidated_house_8017D61C`:
/// snapshots the placed model coordinate's matrix into a fresh `DdhModelWork`,
/// seeds its 0x1000 word, marks the model's `TmdObject` hidden (bit 0x80 of
/// `field_C`), re-parents the task that spawned this one under it and advances
/// to state 1.
void func_dryfield_dilapidated_house_8018118C(Task* arg0)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    DdhModelWork*  work;

    obj   = (TmdObject*)arg0->extra;
    coord = obj->field_8;
    work  = (DdhModelWork*)Mem_Malloc(0x24, false);
    if (work == NULL) {
        Task_Kill(arg0);
        return;
    }
    arg0->work     = (TaskIdMap*)work;
    work->field_20 = 0x1000;
    work->mtx      = coord->coord;
    obj->field_C  |= 0x80;
    Task_Reparent((Task*)arg0->spawnArg2, arg0);
    arg0->state += 1;
}

void func_dryfield_dilapidated_house_80181264(Task* arg0)
{
    func_dryfield_dilapidated_house_8017EBB8(arg0);
    func_dryfield_dilapidated_house_8017EE58(arg0);
}

/// Converts one axis of a cubic Bezier segment (control points `p0`..`p3`) into
/// the polynomial coefficients of `B(t)`, stored high order first: `t^3`, `t^2`,
/// `t` and the constant term.
void func_dryfield_dilapidated_house_80181290(s32 p0, s32 p1, s32 p2, s32 p3, SVECTOR* coeff)
{
    coeff->vx  = -p0 + (p1 - p2) * 3 + p3;
    coeff->vy  = (p0 + p2) * 3 - p1 * 6;
    coeff->vz  = (-p0 + p1) * 3;
    coeff->pad = p0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_801812E8);

void func_dryfield_dilapidated_house_80181340(Task* arg0)
{
    GsCOORDINATE2* coord;
    void*          work;

    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    work  = Mem_Malloc(4, false);
    if (work == NULL) {
        Task_Kill(arg0);
        return;
    }
    arg0->work = work;
    coord->sub = (GsCOORDINATE2*)((TmdObject*)((Task*)arg0->spawnArg2)->extra)->field_8;
    Task_Reparent((Task*)arg0->spawnArg2, arg0);
    arg0->exitCallback = func_dryfield_dilapidated_house_8018142C;
    arg0->state       += 1;
}

void func_dryfield_dilapidated_house_801813DC(Task* task)
{
    SVECTOR verts[24];
    s32     sp0;
    s32     sp1;

    func_dryfield_dilapidated_house_8017FAD4(task, verts, &sp0, &sp1);
    func_dryfield_dilapidated_house_8017F568(task, verts, sp0);
    func_dryfield_dilapidated_house_8017F568(task, verts, sp0);
}

void func_dryfield_dilapidated_house_8018142C(Task* arg0)
{
    GsCOORDINATE2* coord;

    coord      = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    coord->sub = &Gfx_ViewCoord;
    Task_Kill(arg0);
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_8018145C);

extern s32 D_dryfield_dilapidated_house_80186804[16];

/// State 0 of the handler table at `D_dryfield_dilapidated_house_8017D634`,
/// dispatched by `func_dryfield_dilapidated_house_8018145C`: fills a fresh
/// `DdhAngleStep` with the shared per-part angle table scaled by this task's spawn
/// arg (each wrapped into the 0x4000 angle period), links the model coordinate
/// this task works on to the parent model's coordinate array, and re-parents the
/// task that spawned this one under it.
void func_dryfield_dilapidated_house_801814B4(Task* arg0)
{
    DdhAngleStep*  work;
    GsCOORDINATE2* coord;
    s32            i;

    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    work  = (DdhAngleStep*)Mem_Malloc(0x40, false);
    if (work == NULL) {
        Task_Kill(arg0);
        return;
    }
    arg0->work = (TaskIdMap*)work;
    for (i = 0; i < 0x10; i++) {
        work->step[i] = (D_dryfield_dilapidated_house_80186804[i] * arg0->spawnArg1) & 0x3FFF;
    }
    coord->sub = (GsCOORDINATE2*)((TmdObject*)((Task*)arg0->spawnArg2)->extra)->field_8;
    Task_Reparent((Task*)arg0->spawnArg2, arg0);
    arg0->state += 1;
}

void func_dryfield_dilapidated_house_80181584(Task* task)
{
    SVECTOR verts[32];

    func_dryfield_dilapidated_house_80180738(task, verts);
    func_dryfield_dilapidated_house_801803A4(task, verts);
}

void func_dryfield_dilapidated_house_801815B8(Task* arg0)
{
    GsCOORDINATE2* coord;

    coord      = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    coord->sub = &Gfx_ViewCoord;
    Task_Kill(arg0);
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_801815E8);

/// Near and far trail offsets. `[0]` seeds the object's coordinate on the first
/// frame and `[1]` the second ring; `D_dryfield_dilapidated_house_8018694C` is
/// `[1]` under its own name, because the per-frame path in state 1 rebuilds
/// its address from scratch.
extern SVECTOR D_dryfield_dilapidated_house_80186944[1];
extern SVECTOR D_dryfield_dilapidated_house_8018694C;

/// Eight-slot trail coordinates, one array per end of the pair. Every entry is
/// parented to `Gfx_ViewCoord`.
extern GsCOORDINATE2 D_dryfield_dilapidated_house_80189DE0[8];
extern GsCOORDINATE2 D_dryfield_dilapidated_house_8018A060[8];

/// Per-frame twin trail. State 0 places the object's coordinate at
/// `D_dryfield_dilapidated_house_80186944[0]` and the second ring at `[1]`,
/// then seeds all sixteen trail slots with that pose. State 1 re-poses both
/// frames every frame, writes them into slot `field_22 & 7`, re-runs the whole
/// ring so the older slots follow their parents, and hands the ribbon to
/// `func_dryfield_dilapidated_house_801823B8`. The task frees itself once
/// `field_22` reaches spawn arg 1. It idles whole while `Gp_State1C->field_4`
/// is 2 or more.
void func_dryfield_dilapidated_house_80181F08(Task* task)
{
    GsCOORDINATE2  coord;
    GsCOORDINATE2* objCoord;
    GsCOORDINATE2* dst;
    GpEffWork*     work;
    SVECTOR*       vec;
    s32            i;

    work     = (GpEffWork*)task->spawnArg2;
    objCoord = ((TmdObject*)task->extra)->field_8;

    if (Gp_State1C->field_4 < 2) {
        work->field_22++;
        switch (task->state) {
            case 0:
                objCoord->sub        = work->field_8;
                objCoord->coord.t[0] = D_dryfield_dilapidated_house_80186944[0].vx;
                objCoord->coord.t[1] = D_dryfield_dilapidated_house_80186944[0].vy;
                objCoord->coord.t[2] = D_dryfield_dilapidated_house_80186944[0].vz;
                objCoord->flg        = 0;
                Gp_UpdateCoord(objCoord);
                task->state      = 1;
                coord.sub        = work->field_8;
                vec              = &D_dryfield_dilapidated_house_80186944[1];
                coord.coord.t[0] = vec->vx;
                coord.coord.t[1] = vec->vy;
                coord.coord.t[2] = vec->vz;
                coord.flg        = 0;
                Gp_UpdateCoord(&coord);
                for (i = 0; i < 8; i++) {
                    dst        = &D_dryfield_dilapidated_house_80189DE0[i];
                    dst->sub   = &Gfx_ViewCoord;
                    dst->workm = objCoord->workm;
                    gte_SetRotMatrix(&objCoord->workm);
                    gte_SetTransMatrix(&objCoord->workm);
                    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &dst->workm, &dst->coord);
                    dst        = &D_dryfield_dilapidated_house_8018A060[i];
                    dst->sub   = &Gfx_ViewCoord;
                    dst->workm = coord.workm;
                    gte_SetRotMatrix(&coord.workm);
                    gte_SetTransMatrix(&coord.workm);
                    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &dst->workm, &dst->coord);
                }
                return;

            case 1:
                objCoord->flg = 0;
                Gp_UpdateCoord(objCoord);
                coord.sub        = work->field_8;
                coord.coord.t[0] = D_dryfield_dilapidated_house_8018694C.vx;
                coord.coord.t[1] = D_dryfield_dilapidated_house_8018694C.vy;
                coord.coord.t[2] = D_dryfield_dilapidated_house_8018694C.vz;
                coord.flg        = 0;
                Gp_UpdateCoord(&coord);
                dst        = &D_dryfield_dilapidated_house_80189DE0[work->field_22 & 7];
                dst->sub   = &Gfx_ViewCoord;
                dst->workm = objCoord->workm;
                gte_SetRotMatrix(&objCoord->workm);
                gte_SetTransMatrix(&objCoord->workm);
                Gp_WorldToLocal(&Gfx_ViewCoord.workm, &dst->workm, &dst->coord);
                dst        = &D_dryfield_dilapidated_house_8018A060[work->field_22 & 7];
                dst->sub   = &Gfx_ViewCoord;
                dst->workm = coord.workm;
                gte_SetRotMatrix(&coord.workm);
                gte_SetTransMatrix(&coord.workm);
                Gp_WorldToLocal(&Gfx_ViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst      = &D_dryfield_dilapidated_house_80189DE0[i];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                    dst      = &D_dryfield_dilapidated_house_8018A060[i];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                }
                func_dryfield_dilapidated_house_801823B8(work->field_22 & 7, 0x210);
                if (work->field_22 == task->spawnArg1 && work->field_22 != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the two eight-slot coordinate trails as seven gouraud `POLY_G4`
/// quads, walking backwards from `slot`. Each quad spans `workm.t` of two
/// adjacent slots on `D_dryfield_dilapidated_house_80189DE0` and
/// `D_dryfield_dilapidated_house_8018A060`. Dropped when `gte_stszotz` is
/// closer than 0x11. `flags` is the beam colour, three 2-bit channels at
/// bits 8, 4 and 0 that each multiply the 0x40-9i fade.
void func_dryfield_dilapidated_house_801823B8(s16 slot, s16 flags)
{
    DdhBeamScratch* blk;
    GsCOORDINATE2*  a;
    GsCOORDINATE2*  b;
    POLY_G4*        prim;
    s32             i;
    s32             j;
    s32             i0;
    s32             i1;
    s32             hi;
    s32             lo;
    s32             fade;

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD - sizeof(DdhBeamScratch);
    blk                     = (DdhBeamScratch*)*(void**)G_SCRATCH_HEAD;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 7; i++) {
        j            = slot - i;
        i0           = j & 7;
        i1           = (j - 1) & 7;
        a            = &D_dryfield_dilapidated_house_80189DE0[i0];
        blk->v[0].vx = *(u16*)&a->workm.t[0];
        blk->v[0].vy = *(u16*)&a->workm.t[1];
        b            = &D_dryfield_dilapidated_house_8018A060[i0];
        blk->v[0].vz = *(u16*)&a->workm.t[2];
        blk->v[1].vx = *(u16*)&b->workm.t[0];
        blk->v[1].vy = *(u16*)&b->workm.t[1];
        a            = &D_dryfield_dilapidated_house_80189DE0[i1];
        blk->v[1].vz = *(u16*)&b->workm.t[2];
        blk->v[2].vx = *(u16*)&a->workm.t[0];
        blk->v[2].vy = *(u16*)&a->workm.t[1];
        b            = &D_dryfield_dilapidated_house_8018A060[i1];
        blk->v[2].vz = *(u16*)&a->workm.t[2];
        blk->v[3].vx = *(u16*)&b->workm.t[0];
        blk->v[3].vy = *(u16*)&b->workm.t[1];
        blk->v[3].vz = *(u16*)&b->workm.t[2];
        gte_ldv0(&blk->v[0]);
        gte_rtps_real();
        prim           = (POLY_G4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setPolyG4(prim);
        gte_stsxy(&prim->x0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt_real();
        gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
        gte_stszotz(&blk->otz);
        if (blk->otz >= 0x11) {
            fade = 0x40 - i * 9;
            hi   = fade & 0xFF;
            lo   = (fade - 9) & 0xFF;
            setRGB0(prim, hi * (flags >> 8), hi * ((flags >> 4) & 3), hi * (flags & 3));
            setRGB1(prim, hi * (flags >> 8), hi * ((flags >> 4) & 3), hi * (flags & 3));
            setRGB2(prim, lo * (flags >> 8), lo * ((flags >> 4) & 3), lo * (flags & 3));
            setRGB3(prim, lo * (flags >> 8), lo * ((flags >> 4) & 3), lo * (flags & 3));
            addPrim((u_long*)((((u32)(blk->otz << Display_State.field_128) >> 2) & 0xFFC) +
                              (s32)Gpu_CurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + sizeof(DdhBeamScratch);
}

/// Per-frame state machine of the ``DdhEffWork`` effect family's fade-in
/// handler: state 0 seeds the work block (0xC0 / 0x500 scale and angle, a
/// 12-bit `Gp_LcgState` draw as the third ramp value, a `Gp_SpawnEff` and a
/// fade quad), maps the placed model's own coordinate onto
/// `Gp_RoomCoords[0]` and spawns the ring of `0x60275` flame effects, then
/// re-parents each onto this task. State 1 steps the angle by 0x40 per frame
/// and runs two more draws against the same coordinate. While the
/// `Gp_State1C` fade is armed the frame counter is rolled back and the work
/// block is released as soon as the fade reaches 4 or the angle passes
/// 0x580.
void func_dryfield_dilapidated_house_80182744(Task* task)
{
    DdhEffWork*    work;
    GsCOORDINATE2* coord;
    GpCoord64*     rc;
    GpCoordTail*   tail;
    GpEffWork*     eff;
    u16            tick;
    u16            tick1;
    s16            size;
    s32            angle;
    s32            keep;
    s32            i;
    u8             rgb[3];

    work           = task->spawnArg2;
    coord          = (GsCOORDINATE2*)((TmdObject*)task->extra)->field_8;
    tick           = work->field_22;
    tick1          = tick + 1;
    work->field_22 = tick1;
    rc             = &Gp_RoomCoords[0];
    tail           = (GpCoordTail*)&rc->coord;

    switch (task->state) {
        case 0:
            if (Gp_State1C->field_4 != 0) {
                s32 fade;

                work->field_22 = tick;
                fade           = Gp_State1C->field_4;
                SOFT_USE_REG(fade);
                keep = fade < 4;
                break;
            }
            work->field_24 = 0xC0;
            work->field_26 = 0x500;
            work->field_20 = 0;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_28 = (Gp_LcgState >> 16) & 0xFFF;
            Gp_SpawnEff(0x60274, coord, 0, NULL);
            rgb[0] = 0xFF;
            rgb[1] = 0x7F;
            rgb[2] = 0x3F;
            Gp_DrawFadeQuad(rgb, 1);
            Gp_RoomCoords[0].field_0 = 4;
            tail->field_58           = 0x200;
            tail->field_5C           = 0x2000;
            Gp_LcgState              = Gp_LcgState * 5 + 0x71357911;
            size                     = ((Gp_LcgState >> 16) & 0x700) + 0x800;
            tail->field_50           = size;
            tail->field_52           = size >> 1;
            tail->field_54           = size >> 2;
            tail->coord.coord.t[0]   = coord->coord.t[0];
            tail->coord.coord.t[1]   = coord->coord.t[1];
            tail->coord.coord.t[2]   = coord->coord.t[2];
            rc->coord.flg            = 0;
            i                        = 0;
            func_dryfield_dilapidated_house_801832A8(coord, (s16)work->field_22, work->field_26, work->field_28);
            func_dryfield_dilapidated_house_80182F14(coord, work->field_26, (s16)(u16)work->field_24 >> 1);
            work->field_26 = 0x380;
            do {
                eff = Gp_SpawnEff(0x60275, coord, i, NULL);
                if (eff != NULL) {
                    Task_Reparent(task, eff->field_0);
                }
                i += 0x2AA;
            } while (i < 0x556);
            task->state = 1;
            return;
        case 1:
            if (Gp_State1C->field_4 != 0) {
                work->field_22 = tick;
                keep           = Gp_State1C->field_4 < 4;
                break;
            }
            func_dryfield_dilapidated_house_801832A8(coord, (s16)tick1, work->field_26, work->field_28);
            func_dryfield_dilapidated_house_80182F14(coord, work->field_26, (s16)(u16)work->field_24 >> 1);
            func_dryfield_dilapidated_house_80182F14(coord, (s16)((u16)work->field_26 * 2), (s16)(u16)work->field_24 >> 1);
            angle          = (u16)work->field_26;
            angle         += 0x40;
            work->field_26 = angle;
            SOFT_USE_REG(angle);
            keep = (s16)angle < 0x581;
            break;
        default:
            return;
    }
    if (!keep) {
        Gp_ReleaseState1CMem(work, task);
    }
}

/// Draws the flame column: two 16-vertex rings of radius `arg1` and
/// `arg1 + 0x100` are built in the XY plane (`vz` 0x100 / 0) by `rsin` /
/// `rcos`, rotated by `arg0`'s `workm` and offset by its translation, then
/// each of the 16 segments is projected through `GsWSMATRIX` as one `POLY_G4`.
/// The inner edge carries the unsigned `arg2` ramp `(arg2, arg2 >> 1, arg2 >> 2)`
/// and the outer edge fades to black; a negative `gte_stflg` drops the segment.
/// Same body as `func_pyrokinesis_8012FC34`.
void func_dryfield_dilapidated_house_80182A18(GsCOORDINATE2* arg0, s16 arg1, s16 arg2)
{
    void**         scratch;
    register u8*   head asm("v0");
    GpBandScratch* block;
    SVECTOR*       op;
    POLY_G4*       prim;
    s32            i;
    s32            next;
    s32            ang;
    s16            r0;
    s16            r1;
    u32            ramp;
    u8             red;
    u8             grn;
    u8             blu;

    /* The ramp halves are unsigned: writing them as `(u16)arg2 >> 1` folds the
     * widening into an `andi`, where the ROM shifts the value up and back. */
    ramp     = (u32)arg2 << 16;
    red      = arg2;
    grn      = ramp >> 17;
    blu      = ramp >> 18;
    r1       = arg1 + 0x100;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = (u8*)*scratch - 0x118;
    block    = (GpBandScratch*)head;
    *scratch = head;
    gte_SetTransMatrix(&GsWSMATRIX);
    r0 = arg1;
    for (i = 0; i < 16; i++) {
        ang                = i << 8;
        block->inner[i].vx = (rsin(ang) * r0) >> 12;
        block->inner[i].vy = (rcos(ang) * r0) >> 12;
        block->inner[i].vz = 0x100;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->inner[i]);
        gte_rtv0_real();
        gte_stsv(&block->inner[i]);
        block->inner[i].vx = *(u16*)&block->inner[i].vx + *(u16*)&arg0->workm.t[0];
        block->inner[i].vy = *(u16*)&block->inner[i].vy + *(u16*)&arg0->workm.t[1];
        block->inner[i].vz = *(u16*)&block->inner[i].vz + *(u16*)&arg0->workm.t[2];
        block->outer[i].vx = (rsin(ang) * r1) >> 12;
        op                 = &block->inner[i] + 16;
        op->vy             = (rcos(ang) * r1) >> 12;
        op->vz             = 0;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->outer[i]);
        gte_rtv0_real();
        gte_stsv(&block->outer[i]);
        block->outer[i].vx = *(u16*)&block->outer[i].vx + *(u16*)&arg0->workm.t[0];
        op->vy             = *(u16*)&op->vy + *(u16*)&arg0->workm.t[1];
        op->vz             = *(u16*)&op->vz + *(u16*)&arg0->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        gte_ldv0(&block->inner[i]);
        gte_rtps_real();
        gte_stsxy(&block->sxy0);
        next = (i + 1) & 0xF;
        gte_ldv3(&block->inner[next], &block->outer[i], &block->outer[next]);
        gte_rtpt_real();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz);
            block->otz++;
            prim           = (POLY_G4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setPolyG4(prim);
            setRGB0(prim, red, grn, blu);
            setRGB1(prim, red, grn, blu);
            setRGB2(prim, 0, 0, 0);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sxy0.vx;
            prim->y0 = *(u16*)&block->sxy0.vy;
            prim->x1 = *(u16*)&block->sxy1.vx;
            prim->y1 = *(u16*)&block->sxy1.vy;
            prim->x2 = *(u16*)&block->sxy2.vx;
            prim->y2 = *(u16*)&block->sxy2.vy;
            prim->x3 = *(u16*)&block->sxy3.vx;
            prim->y3 = *(u16*)&block->sxy3.vy;
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) +
                              (s32)Gpu_CurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x118;
}

/// Draws the flame ring: `arg0`'s origin is projected once through
/// `GsWSMATRIX` and eight `POLY_G4` blades are swept around it, each spanning
/// a 0x200 arc of radius `(arg1 * 64) / otz`. Only the third vertex carries
/// colour, the rest of the blade fading to black, and that colour is the
/// `arg2` ramp `(arg2, arg2 >> 1, arg2 >> 2)` - a red-biased fire tint. A
/// negative `gte_stflg` drops the whole ring. Same body as
/// `func_pyrokinesis_80130130`.
void func_dryfield_dilapidated_house_80182F14(GsCOORDINATE2* arg0, s32 arg1, s16 arg2)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    POLY_G4*       prim;
    s32            ang;
    register s32   ang2 asm("s1");
    u16            vz;
    u16            red;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                      = *(u16*)&arg0->workm.t[0];
        ((GpRingScratch*)(head - 0x18))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x18;
        block = (GpRingScratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;
    red           = arg2;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        USE_REG(head);
        block->otz++;
        block->step = ((s16)arg1 * 64) / block->otz;
        ang         = 0;
        do {
            prim           = (POLY_G4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, *(u8*)&red, arg2 >> 1, arg2 >> 2);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->step * rsin(ang)) >> 12);
            prim->y0 = *(u16*)&block->sy + ((block->step * rcos(ang)) >> 12);
            ang2     = ang + 0x100;
            prim->x1 = *(u16*)&block->sx + ((block->step * rsin(ang2)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->step * rcos(ang2)) >> 12);
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            ang2     = ang + 0x200;
            prim->x3 = *(u16*)&block->sx + ((block->step * rsin(ang2)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->step * rcos(ang2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) +
                              (s32)Gpu_CurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            ang = ang2;
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_801832A8);

/// Draws the flame band: two 16-vertex rings of radius `arg1` and
/// `arg1 + arg2` are built in the XZ plane by `rsin` / `rcos`, rotated by
/// `arg0`'s `workm` and offset by its translation, then each of the 16
/// segments is projected through `GsWSMATRIX` as one `POLY_G4`. The inner
/// edge carries the `arg3` ramp `(arg3, arg3 >> 1, arg3 >> 2)` and the outer
/// edge fades to black; a negative `gte_stflg` drops the segment. Same body
/// as `func_pyrokinesis_801312B4`.
void func_dryfield_dilapidated_house_80183728(GsCOORDINATE2* arg0, s16 arg1, s32 arg2, s16 arg3)
{
    void**         scratch;
    register u8*   head asm("v0");
    GpBandScratch* block;
    SVECTOR*       op;
    POLY_G4*       prim;
    s32            i;
    s32            next;
    s32            ang;
    s16            r0;
    s16            r1;

    r1       = arg1 + arg2;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = (u8*)*scratch - 0x118;
    block    = (GpBandScratch*)head;
    *scratch = head;
    gte_SetTransMatrix(&GsWSMATRIX);
    r0 = arg1;
    for (i = 0; i < 16; i++) {
        ang                = i << 8;
        block->inner[i].vx = (rsin(ang) * r0) >> 12;
        block->inner[i].vy = 0;
        block->inner[i].vz = (rcos(ang) * r0) >> 12;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->inner[i]);
        gte_rtv0_real();
        gte_stsv(&block->inner[i]);
        block->inner[i].vx = *(u16*)&block->inner[i].vx + *(u16*)&arg0->workm.t[0];
        block->inner[i].vy = *(u16*)&block->inner[i].vy + *(u16*)&arg0->workm.t[1];
        block->inner[i].vz = *(u16*)&block->inner[i].vz + *(u16*)&arg0->workm.t[2];
        block->outer[i].vx = (rsin(ang) * r1) >> 12;
        op                 = &block->inner[i] + 16;
        op->vy             = 0;
        op->vz             = (rcos(ang) * r1) >> 12;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->outer[i]);
        gte_rtv0_real();
        gte_stsv(&block->outer[i]);
        block->outer[i].vx = *(u16*)&block->outer[i].vx + *(u16*)&arg0->workm.t[0];
        op->vy             = *(u16*)&op->vy + *(u16*)&arg0->workm.t[1];
        op->vz             = *(u16*)&op->vz + *(u16*)&arg0->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        gte_ldv0(&block->inner[i]);
        gte_rtps_real();
        gte_stsxy(&block->sxy0);
        next = (i + 1) & 0xF;
        gte_ldv3(&block->inner[next], &block->outer[i], &block->outer[next]);
        gte_rtpt_real();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz);
            block->otz++;
            prim           = (POLY_G4*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
            setPolyG4(prim);
            setRGB0(prim, *(u8*)&arg3, arg3 >> 1, arg3 >> 2);
            setRGB1(prim, *(u8*)&arg3, arg3 >> 1, arg3 >> 2);
            setRGB2(prim, 0, 0, 0);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sxy0.vx;
            prim->y0 = *(u16*)&block->sxy0.vy;
            prim->x1 = *(u16*)&block->sxy1.vx;
            prim->y1 = *(u16*)&block->sxy1.vy;
            prim->x2 = *(u16*)&block->sxy2.vx;
            prim->y2 = *(u16*)&block->sxy2.vy;
            prim->x3 = *(u16*)&block->sxy3.vx;
            prim->y3 = *(u16*)&block->sxy3.vy;
            addPrim((u_long*)(((((u32)block->otz << Display_State.field_128) >> 2) & 0xFFC) +
                              (s32)Gpu_CurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x118;
}

void func_dryfield_dilapidated_house_80183BF8(Task* arg0)
{
    GsCOORDINATE2* coord;
    s32            mask;

    mask  = 1 << (u8)gGameSession->loc.view;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (mask & 0x84A9C) {
        func_dryfield_dilapidated_house_801815E8(coord, 0);
    }
    if (mask & 0x104B98) {
        func_dryfield_dilapidated_house_801815E8(coord, 8);
    }
    if (mask & 0xA55F8) {
        func_dryfield_dilapidated_house_801815E8(coord, 0x10);
    }
}

/// Per-frame handler that runs the `DdhEffWork` effect block one step further:
/// an early out while `Gp_State1C` is armed. It counts frames in `field_22`,
/// seeds the 0xC0 / 0x100 scale/angle pair on the first frame, feeds the pair to
/// `func_dryfield_dilapidated_house_80182A18` and then steps the scale by -0x10
/// and the angle by +0x40. Once the scale falls below 0x10 - and immediately
/// when the state word has already reached 4 - it releases the work block.
void func_dryfield_dilapidated_house_80183C8C(Task* arg0)
{
    DdhEffWork* mem;
    s16         flag;
    s32         scale;
    s32         angle;

    mem  = arg0->spawnArg2;
    flag = Gp_State1C->field_4;
    if (flag != 0) {
        if (flag >= 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }

    mem->field_22++;
    if (arg0->state == 0) {
        mem->field_24 = 0xC0;
        mem->field_26 = 0x100;
        arg0->state   = 1;
    }
    func_dryfield_dilapidated_house_80182A18(((TmdObject*)arg0->extra)->field_8, mem->field_26, mem->field_24);
    angle         = (u16)mem->field_26;
    scale         = (u16)mem->field_24;
    angle        += 0x40;
    scale        -= 0x10;
    mem->field_24 = scale;
    mem->field_26 = angle;
    if ((s16)scale < 0x10) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

/// Per-frame handler of the effect family whose work block is `DdhEffWork`
/// (`task->spawnArg2`). While the `Gp_State1C` state word at 0x4 is clear it
/// seeds the ramp (0x80 / 0x100) on the first frame and then, every frame,
/// clears the model coordinate's update flag, refreshes the coordinate and feeds
/// the angle/scale pair to `func_dryfield_dilapidated_house_80183728`, stepping
/// the scale by -8 and the angle by +0x80. Once the scale drops below 9 - and
/// immediately when that state word has already reached 4 - it releases the work
/// block through `Gp_ReleaseState1CMem`.
void func_dryfield_dilapidated_house_80183D5C(Task* arg0)
{
    DdhEffWork*    mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s32            scale;
    s32            angle;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (flag != 0) {
        if (flag >= 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }

    if (arg0->state == 0) {
        Gfx_RotMatrixZ(&coord->coord, arg0->spawnArg1, 0);
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        mem->field_24 = 0x80;
        mem->field_26 = 0x100;
        arg0->state   = 1;
    }

    func_dryfield_dilapidated_house_80183728(coord, mem->field_26, 0x100, mem->field_24);
    angle         = (u16)mem->field_26;
    scale         = (u16)mem->field_24;
    angle        += 0x80;
    scale        -= 8;
    mem->field_24 = scale;
    mem->field_26 = angle;
    if ((s16)scale < 9) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}
