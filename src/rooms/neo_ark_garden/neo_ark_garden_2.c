#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

/// The block the garden's ambience task reaches through `Task::spawnArg2`.
/// Only `soundDelay` is read here; what precedes it belongs to whoever owns the
/// block, and the type's true size is not known.
typedef struct NeoArkGardenAmbience {
    u8  pad_0[0x24];
    s16 soundDelay; // Frames left before the view's loops are re-enqueued; set to 4 on every view change
} NeoArkGardenAmbience;

/// The gameplay-resident light slot the glow writes: `mode` becomes 2 and
/// `data.light` takes the glow's world position and a randomised intensity.
typedef struct {
    s32 mode;
    union {
        GsCOORDINATE2 coord;
        GpObj44       light;
    } data;
} _NeoArkGardenLight;

extern s32                D_80070F70;
extern _NeoArkGardenLight D_80114FF8;
extern s32                D_80115730;
extern s32                D_80115734;
extern s32                D_80115754;
extern u32                Gp_LcgState;

extern SVECTOR D_neo_ark_garden_801813D8;
extern SVECTOR D_neo_ark_garden_801813E0[];

/// Per-channel right shifts turning a glow's brightness into its colour,
/// indexed by the tint in `Task::spawnArg1`.
extern s16 D_neo_ark_garden_80181400[][3];

void func_neo_ark_garden_8017EFB8(SVECTOR* arg0, s16 arg1, s32 arg2);
void func_neo_ark_garden_8017F42C(SVECTOR* arg0);
void func_neo_ark_garden_8017FF0C(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_neo_ark_garden_80180190(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_neo_ark_garden_801805B4(GsCOORDINATE2* arg0, s32 arg1, u8* rgb);
void func_neo_ark_garden_80180AF4(GsCOORDINATE2* coord, s16 size);
void func_neo_ark_garden_80181020(GsCOORDINATE2* arg0, s32 arg1);

/// Garden ambience task tick. On its first tick it installs three effect ids
/// and moves `state` to 1. `spawnArg1` holds the view seen on the previous
/// tick; whenever `Gp_GetViewIndex()` differs from it, `soundDelay` restarts at
/// 4, and once it has run down the current view's pair of 0x550F0003 /
/// 0x550F0004 loops is enqueued every tick. In views 2, 4 and 5 the first such
/// tick with `state` still 1 also plays them once through
/// `SndEvt_EnqueueType6` and moves `state` to 2. Views 2 and 4 additionally
/// roll two 1-in-4 chances per tick, while no event is running, to spawn
/// effect 0x60070 at the first two points of `D_neo_ark_garden_801813E0`;
/// view 4 also updates the last two points, and view 3 draws the marker at
/// `D_neo_ark_garden_801813D8`.
void func_neo_ark_garden_8017EA9C(Task* task)
{
    NeoArkGardenAmbience* work;
    u32                   rnd;

    work = task->spawnArg2;
    if (task->state == 0) {
        task->state = 1;
        D_80115734  = 0x60228;
        D_80115730  = 0x60233;
        D_80115754  = 0x6023E;
    }
    if (task->spawnArg1 != (Gp_GetViewIndex() & 0xFF)) {
        work->soundDelay = 4;
    }
    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
            if (work->soundDelay == 0) {
                if (task->state == 1) {
                    task->state = 2;
                    SndEvt_EnqueueType6(0x550F0003, -8, 0x32);
                    SndEvt_EnqueueType6(0x550F0004, 0, 0x32);
                }
                SndEvt_EnqueueTypeA(0x550F0003, -8, 0x32);
                SndEvt_EnqueueTypeA(0x550F0004, 0, 0x32);
            } else {
                work->soundDelay--;
            }
            if (Gp_State1C->eventState == 0) {
                rnd         = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState = rnd;
                if (((rnd >> 16) & 3) == 0) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x60070, 0, ((Gp_LcgState >> 16) & 0x11FF) | 0x22200,
                                &D_neo_ark_garden_801813E0[0]);
                }
                rnd         = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState = rnd;
                if (((rnd >> 16) & 3) == 0) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x60070, 0, ((Gp_LcgState >> 16) & 0x11FF) | 0x22200,
                                &D_neo_ark_garden_801813E0[1]);
                }
            }
            break;
        case 3:
            if (work->soundDelay == 0) {
                SndEvt_EnqueueTypeA(0x550F0003, -0xF, 0x4C);
                SndEvt_EnqueueTypeA(0x550F0004, -0xE, 0x4C);
            } else {
                work->soundDelay--;
            }
            func_neo_ark_garden_8017EFB8(&D_neo_ark_garden_801813D8, 0x600, 0xC0);
            break;
        case 4:
            if (work->soundDelay == 0) {
                if (task->state == 1) {
                    task->state = 2;
                    SndEvt_EnqueueType6(0x550F0003, -0xC, 0);
                    SndEvt_EnqueueType6(0x550F0004, 0xC, 0);
                }
                SndEvt_EnqueueTypeA(0x550F0003, -0xC, 0);
                SndEvt_EnqueueTypeA(0x550F0004, 0xC, 0);
            } else {
                work->soundDelay--;
            }
            func_neo_ark_garden_8017F42C(&D_neo_ark_garden_801813E0[2]);
            func_neo_ark_garden_8017F42C(&D_neo_ark_garden_801813E0[3]);
            if (Gp_State1C->eventState == 0) {
                rnd         = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState = rnd;
                if (((rnd >> 16) & 3) == 0) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x60070, 0, ((Gp_LcgState >> 16) & 0x11FF) | 0x22200,
                                &D_neo_ark_garden_801813E0[0]);
                }
                rnd         = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState = rnd;
                if (((rnd >> 16) & 3) == 0) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x60070, 0, ((Gp_LcgState >> 16) & 0x11FF) | 0x22200,
                                &D_neo_ark_garden_801813E0[1]);
                }
            }
            break;
        case 5:
            if (work->soundDelay == 0) {
                if (task->state == 1) {
                    task->state = 2;
                    SndEvt_EnqueueType6(0x550F0003, -0xE, 0x40);
                    SndEvt_EnqueueType6(0x550F0004, -0xD, 0x40);
                }
                SndEvt_EnqueueTypeA(0x550F0003, -0xE, 0x40);
                SndEvt_EnqueueTypeA(0x550F0004, -0xD, 0x40);
            } else {
                work->soundDelay--;
            }
            break;
        case 6:
            if (work->soundDelay == 0) {
                SndEvt_EnqueueTypeA(0x550F0003, 0xD, 0x4C);
                SndEvt_EnqueueTypeA(0x550F0004, 0xF, 0x4C);
            } else {
                work->soundDelay--;
            }
            break;
        case 7:
            if (work->soundDelay == 0) {
                SndEvt_EnqueueTypeA(0x550F0003, -0xC, 0);
                SndEvt_EnqueueTypeA(0x550F0004, -0xC, 0);
            } else {
                work->soundDelay--;
            }
            break;
    }
    task->spawnArg1 = Gp_GetViewIndex() & 0xFF;
}

/// Draws a pulsing red marker at the world-space point `arg0`: projects it
/// through `Gfx_ViewWorldMtx` and, when the GTE flag is non-negative, queues
/// two gouraud `POLY_G4` diamonds and two gouraud `LINE_G3` diagonals around
/// the projected centre. `arg2` is a signed half-extent; the on-screen radius
/// is `(s16)arg2 * 32 / otz`. `arg1` scales `gDisplayState.animFrame` into
/// `rsin`, so the lit vertex pulses as `rsin(...) / 34 + 0x78` on red.
void func_neo_ark_garden_8017EFB8(SVECTOR* arg0, s16 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw13Scratch* block;
    POLY_G4*           prim;
    LINE_G3*           line;
    s32                sine;
    s32                pulse;
    s32                radius;
    s32                i;
    s32                t1;
    s32                t2;
    s32                twice;
    u16                sx;
    u16                sy;

    {
        void**       scratch;
        register u8* tmp asm("v0");

        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        tmp      = head - 0x10;
        block    = (RoomDraw13Scratch*)tmp;
        *scratch = tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        sine          = rsin(gDisplayState.animFrame * arg1);
        radius        = ((s16)arg2 * 32) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        i             = 0;
        pulse         = sine / 34 + 0x78;
        block->radius = radius;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, pulse, 0, 0);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx - (u16)block->radius;
            sx       = block->sx;
            prim->x2 = sx;
            prim->x1 = sx;
            prim->x3 = block->sx + (u16)block->radius;
            sy       = block->sy;
            prim->y3 = sy;
            prim->y2 = sy;
            prim->y0 = sy;
            twice    = i * 2;
            prim->y1 = (block->sy - (u16)block->radius) + (block->radius * twice);
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
            setRGB1(line, pulse, 0, 0);
            setRGB2(line, 0, 0, 0);
            t1       = i * 3 - 1;
            t2       = i + 1;
            line->x0 = block->sx + (block->radius * t1);
            line->y0 = block->sy - (block->radius * t2);
            line->x1 = block->sx;
            line->y1 = block->sy;
            line->x2 = block->sx - (block->radius * t1);
            line->y2 = block->sy + (block->radius * t2);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    line);
            Gp_AddTpageShift((P_TAG*)line, 1, block->otz);
            i = t2;
        } while (i < 2);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
}

/// Queues one textured quad (tpage 0xAC, clut 0x43C0, 64x64 texels). The
/// unit corners are scaled by 250 in the y/z plane, rotated by the matrix
/// `Gfx_RotMatrixX` builds from `D_80070F70 << 7`, moved to `arg0`, and
/// projected through `Gfx_ViewWorldMtx`. Nothing is queued when the GTE flag
/// word is negative.
void func_neo_ark_garden_8017F42C(SVECTOR* arg0)
{
    MATRIX         m;
    void**         scratch;
    u8*            head;
    GpQuadScratch* block;
    SVECTOR*       v;
    s32            i;
    GpQuadCorner*  tbl;
    MATRIX*        wm;
    POLY_FT4*      prim;

    Gfx_RotMatrixX(&m, D_80070F70 << 7, 1);
    scratch = (void**)G_SCRATCH_HEAD;
    i       = 0;
    wm      = &m;
    tbl     = D_80111E38;
    head    = (u8*)*scratch - 0x38;
    SOFT_TOUCH_REG(head);
    block    = (GpQuadScratch*)head;
    v        = block->vec;
    *scratch = block;
    do {
        v->vx = 0;
        TOUCH_REG(v);
        v->vy = (s16)tbl->x * 250;
        TOUCH_REG(v);
        v->vz = (s16)tbl->y * 250;
        gte_SetRotMatrix(wm);
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        *(u16*)&v->vx = *(u16*)&v->vx + *(u16*)&arg0->vx;
        tbl++;
        *(u16*)&v->vy = *(u16*)&v->vy + *(u16*)&arg0->vy;
        i++;
        *(u16*)&v->vz = *(u16*)&v->vz + *(u16*)&arg0->vz;
        v++;
    } while (i < 4);

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(&block->vec[0]);
    gte_rtps();
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2D);
        prim->tpage = 0xAC;
        prim->clut  = 0x43C0;
        prim->u0    = 0;
        prim->v0    = 0;
        prim->u1    = 0x3F;
        prim->v1    = 0;
        prim->u2    = 0;
        prim->v2    = 0x3F;
        prim->u3    = 0x3F;
        prim->v3    = 0x3F;
        prim->x0    = *(u16*)&block->sxy0.vx;
        prim->y0    = *(u16*)&block->sxy0.vy;
        prim->x1    = *(u16*)&block->sxy1.vx;
        prim->y1    = *(u16*)&block->sxy1.vy;
        prim->x2    = *(u16*)&block->sxy2.vx;
        prim->y2    = *(u16*)&block->sxy2.vy;
        prim->x3    = *(u16*)&block->sxy3.vx;
        prim->y3    = *(u16*)&block->sxy3.vy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x38;
}

/// Glow effect task. It does nothing while the event state is 1 to 3 and
/// releases its work block once it reaches 4. State 0 parents the coordinate to `GpEffWork::parent`
/// with no rotation at `GpEffWork::pos`. States 1 and 2 grow the brightness
/// to 0xC0 and the size to 0x200 and draw a glow disc tinted through
/// `D_neo_ark_garden_80181400`; state 1 also spawns effect `D_80115730` on a
/// random bone of the player every fourth tick, and state 2 adds a larger
/// half-bright disc on odd ticks. State 3 drifts the coordinate away along a
/// rotated step, draws an expanding ring and fades the glow, releasing the
/// work block once it has faded. State 4 releases it at once.
void func_neo_ark_garden_8017F790(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpEffWork*     spawned;
    MATRIX*        mtx;
    u8             col[4];

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            return;
        }
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    mem->age++;
    switch (arg0->state) {
        case 0:
            coord->sub                   = mem->parent;
            mtx                          = &coord->coord;
            *(s32*)&coord->coord.m[0][0] = 0x1000;
            *(s32*)&mtx->m[0][2]         = 0;
            *(s32*)&mtx->m[1][1]         = 0x1000;
            *(s32*)&mtx->m[2][0]         = 0;
            mtx->m[2][2]                 = 0x1000;
            coord->coord.t[0]            = mem->pos.vx;
            coord->coord.t[1]            = mem->pos.vy;
            coord->coord.t[2]            = mem->pos.vz;
            coord->flg                   = 0;
            Gp_UpdateCoord(coord);
            arg0->state = 1;
            break;
        case 1:
            Gp_UpdateCoord(coord);
            if (!(mem->age & 3)) {
                Task* player = gameGetPtrSlot(3);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                spawned      = Gp_SpawnEff(D_80115730, &((TmdObject*)player->extra)->coords[(((u32)Gp_LcgState >> 16) & 0xF) + 3], (s32)coord, NULL);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->task);
                }
            }
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_neo_ark_garden_80181400[arg0->spawnArg1][0];
            col[1] = mem->scale >> D_neo_ark_garden_80181400[arg0->spawnArg1][1];
            col[2] = mem->scale >> D_neo_ark_garden_80181400[arg0->spawnArg1][2];
            func_neo_ark_garden_801805B4(coord, mem->angle, col);
            break;
        case 2:
            Gp_UpdateCoord(coord);
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_neo_ark_garden_80181400[arg0->spawnArg1][0];
            col[1] = mem->scale >> D_neo_ark_garden_80181400[arg0->spawnArg1][1];
            col[2] = mem->scale >> D_neo_ark_garden_80181400[arg0->spawnArg1][2];
            func_neo_ark_garden_801805B4(coord, mem->angle, col);
            col[0] >>= 1;
            col[1] >>= 1;
            col[2] >>= 1;
            if (mem->age & 1) {
                func_neo_ark_garden_801805B4(coord, (s16)(mem->angle + 0x100), col);
            }
            break;
        case 3:
            Gp_UpdateCoord(coord);
            col[0] = mem->scale >> D_neo_ark_garden_80181400[arg0->spawnArg1][0];
            col[1] = mem->scale >> D_neo_ark_garden_80181400[arg0->spawnArg1][1];
            col[2] = mem->scale >> D_neo_ark_garden_80181400[arg0->spawnArg1][2];
            func_neo_ark_garden_801805B4(coord, mem->angle, col);
            col[0] = mem->scale;
            col[1] = (u16)mem->scale >> 1;
            col[2] = (u16)mem->scale >> 2;
            if (mem->period == 0) {
                mem->move.vy = -0x100;
                mem->move.vz = 0x100;
                mem->move.vx = 0;
                gte_SetRotMatrix(&coord->workm);
                gte_ldv0(&mem->move);
                gte_rtv0();
                gte_stsv(&mem->move);
            }
            mem->period       += 8;
            coord->workm.t[0] += mem->move.vx;
            coord->workm.t[1] += mem->move.vy;
            coord->workm.t[2] += mem->move.vz;
            func_neo_ark_garden_80180190(coord, (s16)(mem->period + 0x80), 0x100, col);
            mem->angle -= 0x10;
            if (mem->scale > 0x10) {
                mem->scale -= 0x10;
                break;
            }
            Gp_ReleaseState1CMem(mem, arg0);
            break;
        case 4:
            Gp_ReleaseState1CMem(mem, arg0);
            break;
    }
}

/// Effect task that moves the task's coordinate toward the coordinate in
/// `Task::spawnArg1` while no event is running. State 0 takes their world
/// displacement into the coordinate's parent frame and scales it by
/// 0xCC / 0x1000; state 1 adds that step to the coordinate every tick and, on
/// odd ticks, draws a sprite there through `func_neo_ark_garden_8017FF0C` with
/// an advancing phase. The work block is released after 20 ticks, or once the
/// event state reaches 4.
void func_neo_ark_garden_8017FCE8(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* target;
    VECTOR         delta;

    work   = task->spawnArg2;
    coord  = ((TmdObject*)task->extra)->coords;
    target = (GsCOORDINATE2*)task->spawnArg1;
    if (Gp_State1C->eventState == 0) {
        work->field_22++;
        switch (task->state) {
            case 0:
                delta.vx = target->workm.t[0] - coord->workm.t[0];
                delta.vy = target->workm.t[1] - coord->workm.t[1];
                delta.vz = target->workm.t[2] - coord->workm.t[2];
                ApplyTransposeMatrixLV(&coord->workm, &delta, &delta);
                work->field_18 = delta.vx;
                work->field_1A = delta.vy;
                work->field_1C = delta.vz;
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&work->field_18);
                gte_rtv0();
                gte_stsv(&work->field_18);
                gte_lddp(0xCC);
                gte_ldsv(&work->field_18);
                gte_gpf12();
                gte_stsv(&work->field_18);
                task->state = 1;
                break;
            case 1:
                coord->coord.t[0] += (s16)work->field_18;
                coord->coord.t[1] += (s16)work->field_1A;
                coord->coord.t[2] += (s16)work->field_1C;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (work->field_22 & 1) {
                    func_neo_ark_garden_8017FF0C(coord, (s16)++work->field_20, 0x200, 0x80);
                }
                if ((s16)work->field_22 >= 20) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    } else if (Gp_State1C->eventState >= 4) {
        Gp_ReleaseState1CMem(work, task);
    }
}

/// Draws a grey sprite at the coordinate's world position: projects it through
/// `GsWSMATRIX` and, when the GTE flag is non-negative, queues one
/// semi-transparent `POLY_FT4` (tpage 0x2A, clut 0x42CB). `arg1 & 3` picks one
/// of four 24-texel animation frames at u 0x60 + frame * 24, v 0..0x17. `arg2`
/// is a signed half-extent; the on-screen radius is
/// `(s16)arg2 * 23 / (otz + 1)`. `arg3` is the grey level on all three
/// channels.
void func_neo_ark_garden_8017FF0C(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**             scratch;
    u8*                head;
    RoomDraw14Scratch* block;
    POLY_FT4*          prim;
    SVECTOR*           vec;
    DisplayState*      ds;
    s32                tex;
    s32                u0;
    s32                u1;
    s32                sarg;
    s32                t;
    s16                xy;
    u16                vz;

    tex = arg1;
    CLOBBER_REG(a1);
    scratch                                     = (void**)G_SCRATCH_HEAD;
    head                                        = *scratch;
    ((RoomDraw14Scratch*)(head - 0x18))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                       = (RoomDraw14Scratch*)(head - 0x18);
    block->vec.vy                               = *(u16*)&arg0->workm.t[1];
    vz                                          = *(u16*)&arg0->workm.t[2];
    *scratch                                    = block;
    block->vec.vz                               = vz;
    vec                                         = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw14Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw14Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw14Scratch*)(head - 0x18))->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->tpage = 0x2A;
        prim->clut  = 0x42CB;
        t           = (tex & 3) * 24;
        u0          = t + 0x60;
        u1          = t + 0x77;
        SOFT_USE_REG(u1);
        prim->u0 = u0;
        prim->u2 = u0;
        prim->v2 = 0x17;
        prim->v3 = 0x17;
        sarg     = (s16)arg2;
        prim->u1 = u1;
        prim->u3 = u1;
        t        = sarg * 24;
        setRGB0(prim, arg3, arg3, arg3);
        prim->v0      = 0;
        prim->v1      = 0;
        block->radius = (t - sarg) / block->otz;
        xy            = *(u16*)&block->sx - *(u16*)&block->radius;
        prim->x2      = xy;
        prim->x0      = xy;
        xy            = *(u16*)&block->sx + *(u16*)&block->radius;
        prim->x3      = xy;
        prim->x1      = xy;
        xy            = *(u16*)&block->sy - *(u16*)&block->radius;
        prim->y1      = xy;
        prim->y0      = xy;
        xy            = *(u16*)&block->sy + *(u16*)&block->radius;
        prim->y3      = xy;
        prim->y2      = xy;
        ds            = &gDisplayState;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x18;
}

/// Draws a gouraud ring at the coordinate's world position: projects it
/// through `GsWSMATRIX` and, when the GTE flag is non-negative, queues sixteen
/// `POLY_G4` segments. One edge of the ring lies at on-screen radius
/// `(s16)arg1 * 64 / (otz + 1)` and is black; the other lies at
/// `(s16)(arg1 + arg2) * 64 / (otz + 1)` and takes the RGB triple `rgb`.
void func_neo_ark_garden_80180190(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomDraw07Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    register void**    scratch asm("a1");
    register s32       saved asm("t1");
    register u8*       head asm("t0");
    register s32       sum asm("a1");
    register s32       otz asm("v0");
    register s32       rOuter asm("a0");
    register s32       rInner asm("v1");
    register u8*       color asm("s4");
    s32                t;
    u16                vz;
    u32                maskLo;
    u32                maskHi;

    saved   = arg1;
    scratch = (void**)G_SCRATCH_HEAD;
    color   = rgb;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                          = *(u16*)&arg0->workm.t[0];
        ((RoomDraw07Scratch*)(head - 0x1C))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomDraw07Scratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    sum           = saved + arg2;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw07Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw07Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw07Scratch*)(head - 0x1C))->otz);
        USE_REG(head);
        otz        = block->otz + 1;
        rOuter     = ((s16)saved * 64) / otz;
        rInner     = (s16)sum * 64;
        block->otz = otz;
        SOFT_BARRIER();
        rInner        = rInner / otz;
        ang           = 0;
        block->rOuter = rOuter;
        block->rInner = rInner;

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

/// Draws a gouraud glow disc at the coordinate's world position: projects it
/// through `GsWSMATRIX` and, when the GTE flag is non-negative, queues eight
/// `POLY_G4` wedges around the projected centre. `arg1` is a signed
/// half-extent; the on-screen radius is `(s16)arg1 * 64 / (otz + 1)`. Only
/// the centre vertex takes the RGB triple `rgb`, so each wedge fades to black.
void func_neo_ark_garden_801805B4(GsCOORDINATE2* arg0, s32 arg1, u8* rgb)
{
    void**             scratch;
    u8*                head;
    RoomDraw10Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    register s32       ang2 asm("s1");
    u16                vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                          = *(u16*)&arg0->workm.t[0];
        ((RoomDraw10Scratch*)(head - 0x18))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x18;
        block = (RoomDraw10Scratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw10Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw10Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw10Scratch*)(head - 0x18))->otz);
        USE_REG(head);
        block->otz++;
        block->step = ((s16)arg1 * 64) / block->otz;
        ang         = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
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
            ang      = ang2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}

/// Burst effect task. Each tick it grows the burst's size by 0x10 and draws
/// an orange glow disc of twice that size and the glow sprite of
/// `func_neo_ark_garden_80180AF4` at the coordinate. While the ring's
/// brightness (`period`) lasts it draws a widening, fading ring instead of
/// fading the burst; after that the burst's brightness (`scale`) fades by
/// 0x18 a tick and the work block is released once it runs out. It does
/// nothing while the event state is 1 to 3 and releases the block once it
/// reaches 4.
void func_neo_ark_garden_80180948(Task* arg0)
{
    u8             rgb[3];
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s16            step;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        mem->age++;
        if (arg0->state == 0) {
            mem->age    = 1;
            mem->scale  = 0xE0;
            mem->angle  = 0x80;
            mem->period = 0xE0;
            mem->step   = 0x80;
            arg0->state = 1;
        }
        Gp_UpdateCoord(coord);
        rgb[0]     = mem->scale;
        rgb[1]     = (u16)mem->scale >> 1;
        rgb[2]     = (u16)mem->scale >> 2;
        step       = mem->angle + 0x10;
        mem->angle = step;
        func_neo_ark_garden_801805B4(coord, (s16)(step * 2), rgb);
        func_neo_ark_garden_80180AF4(coord, mem->angle);
        if (mem->period >= 0x19) {
            rgb[0] = mem->period;
            rgb[1] = (u16)mem->period >> 1;
            rgb[2] = (u16)mem->period >> 2;
            func_neo_ark_garden_80180190(coord, (s16)(mem->step * 3 / 2), 0x60, rgb);
            mem->period -= 0x18;
            mem->step   += 0x30;
            return;
        }
        mem->scale -= 0x18;
        if (mem->scale < 0x18) {
        kill:
            Gp_ReleaseState1CMem(mem, arg0);
        }
    }
}

/// Draws a glow at the coordinate: two camera-facing textured squares, an
/// inner one of half-extent `size` and an outer one of `size * 3 / 2` (each
/// scaled by 0x37 / otz), and, where the ground beneath the coordinate is
/// found, a flat quad on it through `func_neo_ark_garden_80181020`. It also
/// points the gameplay light slot at the coordinate with a randomly
/// flickering intensity. Nothing is drawn when the GTE flags the projection.
void func_neo_ark_garden_80180AF4(GsCOORDINATE2* coord, s16 size)
{
    GsCOORDINATE2  ground;
    POLY_FT4*      prim;
    s16            outerLeft;
    s16            outerRight;
    s16            outerTop;
    s16            outerBottom;
    s16            intensity;
    s16            left;
    s16            right;
    s16            top;
    s16            bottom;
    s32            outerSize;
    s32            shifted;
    u32            random;
    GpObj44*       light;
    GpRingScratch* block;
    void**         scratch;
    GpRingScratch* alias;
    u16            vy;
    GpRingScratch* sc;

    D_80114FF8.mode           = 2;
    light                     = &D_80114FF8.data.light;
    light->field_58           = 0x300;
    light->field_5C           = 0x3000;
    random                    = (Gp_LcgState * 5) + 0x71357911;
    intensity                 = ((random >> 0x10) & 0x700) + 0x800;
    light->field_50           = intensity;
    shifted                   = intensity << 0x10;
    light->field_52           = (s16)(shifted >> 0x11);
    light->field_54           = (s16)(shifted >> 0x12);
    light->field_18.vx        = (s32)coord->coord.t[0];
    light->field_18.vy        = (s32)coord->coord.t[1];
    light->field_18.vz        = coord->coord.t[2];
    D_80114FF8.data.coord.flg = 0;
    scratch                   = (void**)G_SCRATCH_HEAD;
    block                     = (GpRingScratch*)*scratch - 1;
    block->vec.vx             = *(u16*)&coord->workm.t[0];
    alias                     = block;
    vy                        = *(u16*)&coord->workm.t[1];
    __asm__("move %0,%1" : "=r"(alias) : "r"(alias), "r"(vy), "r"(alias));
    sc          = alias;
    sc->vec.vy  = vy;
    sc->vec.vz  = *(u16*)&coord->workm.t[2];
    Gp_LcgState = random;
    *scratch    = sc;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&sc->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (sc->flag >= 0) {
        gte_stszotz(&block->otz);
        prim                 = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor       = prim + 1;
        ((u8*)&prim->tag)[3] = 9;
        prim->code           = 0x2EU;
        *(u16*)&prim->tpage  = 0x29;
        if (D_80070F70 & 1) {
            prim->r0   = 0xA0;
            prim->g0   = 0x80;
            prim->b0   = 0x60;
            prim->clut = 0x428B;
            setUV4(prim, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
        } else {
            prim->clut = 0x428C;
            setUV4(prim, 0xA8, 0xC8, 0xDF, 0xC8, 0xA8, 0xFF, 0xDF, 0xFF);
            prim->code = (u8)(prim->code | 1);
        }
        sc->step = (s32)((s32)((s16)size * 0x37) / (s32)sc->otz);
        left     = *(u16*)&sc->sx - (u16)sc->step;
        prim->x2 = left;
        prim->x0 = left;
        right    = *(u16*)&sc->sx + (u16)sc->step;
        prim->x3 = right;
        prim->x1 = right;
        top      = *(u16*)&sc->sy - (u16)sc->step;
        prim->y1 = top;
        prim->y0 = top;
        bottom   = *(u16*)&sc->sy + (u16)sc->step;
        prim->y3 = bottom;
        prim->y2 = bottom;
        addPrim(
            (u_long*)((((u32)sc->otz << gDisplayState.otDepthShift) >> 2 & 0xFFC) +
                      (s32)gGpuCurrentOt),
            prim);
        prim                 = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor       = prim + 1;
        ((u8*)&prim->tag)[3] = 9;
        prim->code           = 0x2F;
        prim->tpage          = 0x29;
        prim->clut =
            (s16)(((u32)(((gDisplayState.animFrame & 1) * 0x10) + 0x120) >> 4) |
                  0x4300);
        setUV4(prim, 0x38, 0xC8, 0x6F, 0xC8, 0x38, 0xFF, 0x6F, 0xFF);
        outerSize   = (s16)((s16)size * 3 / 2);
        sc->step    = (s32)((s32)(outerSize * 0x37) / (s32)sc->otz);
        outerLeft   = *(u16*)&sc->sx - (u16)sc->step;
        prim->x2    = outerLeft;
        prim->x0    = outerLeft;
        outerRight  = *(u16*)&sc->sx + (u16)sc->step;
        prim->x3    = outerRight;
        prim->x1    = outerRight;
        outerTop    = *(u16*)&sc->sy - (u16)sc->step;
        prim->y1    = outerTop;
        prim->y0    = outerTop;
        outerBottom = *(u16*)&sc->sy + (u16)sc->step;
        prim->y3    = outerBottom;
        prim->y2    = outerBottom;
        addPrim(
            (u_long*)((((u32)sc->otz << gDisplayState.otDepthShift) >> 2 & 0xFFC) +
                      (s32)gGpuCurrentOt),
            prim);
        if (Gp_TraceGroundCoord(coord, &ground) == 1) {
            func_neo_ark_garden_80181020(&ground, outerSize);
        }
    }
    *(void**)G_SCRATCH_HEAD =
        (u8*)*(void**)G_SCRATCH_HEAD + sizeof(GpRingScratch);
}

/// Draws a flickering flat quad at the coordinate `arg0`: scales the unit quad
/// `D_80111E38` by `arg1` in the x/z plane, rotates it by `Gfx_ViewWorldMtx`
/// with no translation, adds `arg0->workm.t`, and projects the four corners
/// through `GsWSMATRIX`. When the GTE flag is non-negative, queues one
/// semi-transparent `POLY_FT4` (tpage 0x28, clut 0x428C) coloured
/// `(0x30, 0x20, 0x20)`. Odd and even frames of `gDisplayState.animFrame`
/// alternate between two 32-texel columns (u 0xC0..0xDF or 0xE0..0xFF, v
/// 0x38..0x57).
void func_neo_ark_garden_80181020(GsCOORDINATE2* arg0, s32 arg1)
{
    void**         scratch;
    u8*            head;
    GpQuadScratch* block;
    SVECTOR*       v;
    s32            i;
    GpQuadCorner*  tbl;
    POLY_FT4*      prim;
    s32            prod;
    s32            u;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    head    -= 0x38;
    *scratch = head;
    block    = (GpQuadScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    i   = 0;
    v   = block->vec;
    tbl = D_80111E38;
    do {
        prod  = tbl->x * arg1;
        v->vy = 0;
        TOUCH_REG(v);
        v->vx = prod;
        TOUCH_REG(v);
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(&Gfx_ViewWorldMtx);
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        *(u16*)&v->vx = *(u16*)&v->vx + *(u16*)&arg0->workm.t[0];
        tbl++;
        *(u16*)&v->vy = *(u16*)&v->vy + *(u16*)&arg0->workm.t[1];
        i++;
        *(u16*)&v->vz = *(u16*)&v->vz + *(u16*)&arg0->workm.t[2];
        v++;
    } while (i < 4);

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps();
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        setRGB0(prim, 0x30, 0x20, 0x20);
        prim->tpage = 0x28;
        prim->clut  = 0x428C;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v0    = 0x38;
        prim->u0    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v1    = 0x38;
        prim->u1    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v2    = 0x57;
        prim->u2    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v3    = 0x57;
        prim->u3    = u;
        prim->x0    = *(u16*)&block->sxy0.vx;
        prim->y0    = *(u16*)&block->sxy0.vy;
        prim->x1    = *(u16*)&block->sxy1.vx;
        prim->y1    = *(u16*)&block->sxy1.vy;
        prim->x2    = *(u16*)&block->sxy2.vx;
        prim->y2    = *(u16*)&block->sxy2.vy;
        prim->x3    = *(u16*)&block->sxy3.vx;
        prim->y3    = *(u16*)&block->sxy3.vy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x38;
}
