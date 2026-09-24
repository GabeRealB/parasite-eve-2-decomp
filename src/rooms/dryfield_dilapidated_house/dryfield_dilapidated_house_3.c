#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/dryfield_dilapidated_house.h"
#include "main/tmd.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#define gte_mvmva_real() __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_rtps_real()  __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real()  __asm__ volatile("nop; nop; .word 0x4A280030")

extern GpMsgEntry D_dryfield_dilapidated_house_80183E8C[];
extern TaskDesc   D_dryfield_dilapidated_house_80183EB4;
extern Task*      D_dryfield_dilapidated_house_80189B7C;
extern s16        D_dryfield_dilapidated_house_80189C98;
extern s32        D_dryfield_dilapidated_house_80186804[16];
extern SVECTOR    D_dryfield_dilapidated_house_80186844[2];
extern DdhRoomRec D_dryfield_dilapidated_house_8018669C;
extern SVECTOR    D_dryfield_dilapidated_house_801866B4[];
extern s8         D_dryfield_dilapidated_house_801866F4[16][4];
extern u8         D_dryfield_dilapidated_house_80186734[24][4];
extern SVECTOR    D_dryfield_dilapidated_house_80186794[2];
extern SVECTOR    D_dryfield_dilapidated_house_801867A4[6];
extern SVECTOR    D_dryfield_dilapidated_house_801867D4[6];
extern TaskDesc   D_dryfield_dilapidated_house_80186854;

void func_dryfield_dilapidated_house_8017F418(SVECTOR* pts, SVECTOR* p3, s32 len, s32 pos, s32* out);

void func_dryfield_dilapidated_house_8017EAB4(Task* arg0)
{
    arg0->msgTable = D_dryfield_dilapidated_house_80183E8C;
    Game_SetPtrSlot(arg0, 7);
    if (Gp_LookupSlot4(1) != 0) {
        D_dryfield_dilapidated_house_80189B78 =
            Task_SpawnFromTable(&D_dryfield_dilapidated_house_80183EB4, 0, 0, 0);
    }
    D_dryfield_dilapidated_house_80189C98 = 2;
    D_dryfield_dilapidated_house_80189B7C =
        Task_SpawnFromTable(&D_dryfield_dilapidated_house_80183EB4, 2, 0, 0);
    gGameSession->flowFlags = 0x83;
    arg0->state            += 1;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_3", func_dryfield_dilapidated_house_8017EB60);

/// Projects the eight local-space markers at
/// `D_dryfield_dilapidated_house_801866B4` through the parent task's
/// `DdhCoordWork` matrix and `Gfx_ViewWorldMtx`, then queues two red
/// `LINE_F2`s as an X at each screen point in `gGpuCurrentOt[10]`.
void func_dryfield_dilapidated_house_8017EBB8(Task* task)
{
    struct {
        SVECTOR vec;
        s32     sxy;
        s32     dp;
        s32     flag;
        s32     otz;
    } sc;
    MATRIX*  mtx;
    LINE_F2* line;
    u16      sx;
    s32      sy;
    s16      x0;
    s16      y0;
    s16      x1;
    s16      y1;
    s32      i;

    i   = 0;
    mtx = &((DdhCoordWork*)((Task*)task->spawnArg2)->work)->mtx;
    do {
        sc.vec.vx = D_dryfield_dilapidated_house_801866B4[i].vx;
        sc.vec.vy = D_dryfield_dilapidated_house_801866B4[i].vy;
        sc.vec.vz = D_dryfield_dilapidated_house_801866B4[i].vz;
        gte_SetRotMatrix(mtx);
        gte_ldv0(&sc.vec);
        gte_mvmva_real();
        gte_stsv(&sc.vec);
        sc.vec.vx = *(u16*)&sc.vec.vx + *(u16*)&mtx->t[0];
        sc.vec.vy = *(u16*)&sc.vec.vy + *(u16*)&mtx->t[1];
        sc.vec.vz = *(u16*)&sc.vec.vz + *(u16*)&mtx->t[2];
        gte_SetRotMatrix(&Gfx_ViewWorldMtx);
        gte_SetTransMatrix(&Gfx_ViewWorldMtx);
        gte_ldv0(&sc.vec);
        gte_rtps_real();
        gte_stsxy(&sc.sxy);
        gte_stdp(&sc.dp);
        gte_stflg(&sc.flag);
        gte_stszotz(&sc.otz);
        line           = (LINE_F2*)gGpuPrimCursor;
        sx             = sc.sxy;
        sy             = sc.sxy >> 16;
        gGpuPrimCursor = line + 1;
        x0             = sx - 5;
        y0             = sy - 5;
        x1             = sx + 5;
        setLineF2(line);
        setRGB0(line, 0xFF, 0, 0);
        y1       = sy + 5;
        line->x0 = x0;
        line->y0 = y0;
        line->x1 = x1;
        line->y1 = y1;
        addPrim(gGpuCurrentOt + 10, line);

        line           = (LINE_F2*)gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setRGB0(line, 0xFF, 0, 0);
        i++;
        line->x0 = x1;
        line->y0 = y0;
        line->x1 = x0;
        line->y1 = y1;
        addPrim(gGpuCurrentOt + 10, line);
    } while (i < 8);
}

/// Debug view of the two cubic Bezier segments whose control points start at
/// `D_dryfield_dilapidated_house_801866B4`: samples each at 21 positions,
/// projects every point through the parent task's `DdhCoordWork` matrix and
/// `Gfx_ViewWorldMtx`, and queues a small `LINE_F2` X at it in
/// `gGpuCurrentOt[10]` - green for the first segment, blue for the second.
void func_dryfield_dilapidated_house_8017EE58(Task* task)
{
    SVECTOR  vec;
    DVECTOR  sx;
    DVECTOR  sy;
    s32      out[3];
    s32      sxy;
    s32      dp;
    s32      flag;
    s32      otz;
    MATRIX*  mtx;
    LINE_F2* line;
    s32      i;

    mtx = &((DdhCoordWork*)((Task*)task->spawnArg2)->work)->mtx;
    for (i = 20; i >= 0; i--) {
        func_dryfield_dilapidated_house_8017F418(D_dryfield_dilapidated_house_801866B4, D_dryfield_dilapidated_house_801866B4 + 3, 20, i, out);
        vec.vx = out[0];
        vec.vy = out[1];
        vec.vz = out[2];
        gte_SetRotMatrix(mtx);
        gte_ldv0(&vec);
        gte_mvmva_real();
        gte_stsv(&vec);
        vec.vx = *(u16*)&vec.vx + *(u16*)&mtx->t[0];
        vec.vy = *(u16*)&vec.vy + *(u16*)&mtx->t[1];
        vec.vz = *(u16*)&vec.vz + *(u16*)&mtx->t[2];
        gte_SetRotMatrix(&Gfx_ViewWorldMtx);
        gte_SetTransMatrix(&Gfx_ViewWorldMtx);
        gte_ldv0(&vec);
        gte_rtps_real();
        gte_stsxy(&sxy);
        gte_stdp(&dp);
        gte_stflg(&flag);
        gte_stszotz(&otz);
        sx.vx = sxy;
        sy.vx = sxy >> 16;

        line           = (LINE_F2*)gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setRGB0(line, 0, 0xFF, 0);
        line->x0 = sx.vx - 1;
        line->y0 = sy.vx - 1;
        line->x1 = sx.vx + 1;
        line->y1 = sy.vx + 1;
        addPrim(gGpuCurrentOt + 10, line);

        line           = (LINE_F2*)gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setRGB0(line, 0, 0xFF, 0);
        line->x0 = sx.vx + 1;
        line->y0 = sy.vx - 1;
        line->x1 = sx.vx - 1;
        line->y1 = sy.vx + 1;
        addPrim(gGpuCurrentOt + 10, line);
    }
    for (i = 20; i >= 0; i--) {
        func_dryfield_dilapidated_house_8017F418(D_dryfield_dilapidated_house_801866B4 + 3, D_dryfield_dilapidated_house_801866B4 + 6, 20, i, out);
        vec.vx = out[0];
        vec.vy = out[1];
        vec.vz = out[2];
        gte_SetRotMatrix(mtx);
        gte_ldv0(&vec);
        gte_mvmva_real();
        gte_stsv(&vec);
        vec.vx = *(u16*)&vec.vx + *(u16*)&mtx->t[0];
        vec.vy = *(u16*)&vec.vy + *(u16*)&mtx->t[1];
        vec.vz = *(u16*)&vec.vz + *(u16*)&mtx->t[2];
        gte_SetRotMatrix(&Gfx_ViewWorldMtx);
        gte_SetTransMatrix(&Gfx_ViewWorldMtx);
        gte_ldv0(&vec);
        gte_rtps_real();
        gte_stsxy(&sxy);
        gte_stdp(&dp);
        gte_stflg(&flag);
        gte_stszotz(&otz);
        sx.vx = sxy;
        sy.vx = sxy >> 16;

        line           = (LINE_F2*)gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setRGB0(line, 0, 0, 0xFF);
        line->x0 = sx.vx - 1;
        line->y0 = sy.vx - 1;
        line->x1 = sx.vx + 1;
        line->y1 = sy.vx + 1;
        addPrim(gGpuCurrentOt + 10, line);

        line           = (LINE_F2*)gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setRGB0(line, 0, 0, 0xFF);
        line->x0 = sx.vx + 1;
        line->y0 = sy.vx - 1;
        line->x1 = sx.vx - 1;
        line->y1 = sy.vx + 1;
        addPrim(gGpuCurrentOt + 10, line);
    }
}

/// Evaluates a cubic Bezier segment at frame `pos` of `len`: control points
/// `pts[0..2]` and `p3`, with `t` running from 1 (0xFFFF) down to 0 as `pos`
/// reaches `len`. Writes the X/Y/Z result to `out`.
void func_dryfield_dilapidated_house_8017F418(SVECTOR* pts, SVECTOR* p3, s32 len, s32 pos, s32* out)
{
    SVECTOR  coeff[3];
    SVECTOR* p1;
    SVECTOR* p2;
    s32      t;
    s32      i;
    s32*     o;

    if (len != 0) {
        t  = ((len - pos) * 0xFFFF) / len;
        p1 = &pts[1];
        p2 = &pts[2];
        func_dryfield_dilapidated_house_80181290(pts->vx, p1->vx, p2->vx, p3->vx, &coeff[0]);
        func_dryfield_dilapidated_house_80181290(pts->vy, p1->vy, p2->vy, p3->vy, &coeff[1]);
        func_dryfield_dilapidated_house_80181290(pts->vz, p1->vz, p2->vz, p3->vz, &coeff[2]);
        o = out;
        for (i = 0; i < 3; i++) {
            *o++ = ((((((coeff[i].vx * t) >> 16) + coeff[i].vy) * t >> 16) + coeff[i].vz) * t >> 16) + coeff[i].pad;
        }
    }
}

void func_dryfield_dilapidated_house_8017F568(Task* task, SVECTOR* verts, s32 arg2)
{
    CVECTOR   colors[24];
    s8*       quad;
    CVECTOR*  col;
    POLY_G4*  poly;
    DR_TPAGE* tpage;
    s32       i;
    u16       scale;
    s32       a, b, c, d;

    quad                 = D_dryfield_dilapidated_house_801866F4[0];
    scale                = ((DdhCoordWork*)((Task*)task->spawnArg2)->work)->field_4;
    task->killCountdown += 0x40;
    if (task->killCountdown >= 0x800) {
        task->killCountdown = 0;
    }
    rsin(task->killCountdown);
    for (i = 0; i < 24; i++) {
        colors[i].r = (D_dryfield_dilapidated_house_80186734[i][0] * (s16)scale) >> 12;
        colors[i].g = (D_dryfield_dilapidated_house_80186734[i][1] * (s16)scale) >> 12;
        colors[i].b = (D_dryfield_dilapidated_house_80186734[i][2] * (s16)scale) >> 12;
    }
    col = colors;
    for (i = 0; i < 6; i++) {
        a              = quad[0];
        b              = quad[1];
        c              = quad[2];
        d              = quad[3];
        poly           = (POLY_G4*)gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        setlen(poly, 8);
        poly->code = 0x3A;
        poly->r0   = col[a].r;
        poly->g0   = col[a].g;
        poly->b0   = col[a].b;
        poly->r1   = col[b].r;
        poly->g1   = col[b].g;
        poly->b1   = col[b].b;
        poly->r2   = col[c].r;
        poly->g2   = col[c].g;
        poly->b2   = col[c].b;
        poly->r3   = col[d].r;
        poly->g3   = col[d].g;
        poly->b3   = col[d].b;
        poly->x0   = verts[a].vx;
        poly->y0   = verts[a].vy;
        poly->x1   = verts[b].vx;
        poly->y1   = verts[b].vy;
        poly->x2   = verts[c].vx;
        poly->y2   = verts[c].vy;
        poly->x3   = verts[d].vx;
        poly->y3   = verts[d].vy;
        addPrim((u32*)((((u32)(arg2 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt) + 3, poly);
        tpage          = gGpuPrimCursor;
        gGpuPrimCursor = tpage + 1;
        setlen(tpage, 1);
        tpage->code[0] = 0xE1000425;
        addPrim((u32*)((((u32)(arg2 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt) + 3, tpage);
        quad += 4;
    }
    for (i = 6; i < 16; i++) {
        a              = quad[0];
        b              = quad[1];
        c              = quad[2];
        d              = quad[3];
        poly           = (POLY_G4*)gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        setlen(poly, 8);
        poly->code = 0x3A;
        poly->r0   = col[a].r;
        poly->g0   = col[a].g;
        poly->b0   = col[a].b;
        poly->r1   = col[b].r;
        poly->g1   = col[b].g;
        poly->b1   = col[b].b;
        poly->r2   = col[c].r;
        poly->g2   = col[c].g;
        poly->b2   = col[c].b;
        poly->r3   = col[d].r;
        poly->g3   = col[d].g;
        poly->b3   = col[d].b;
        poly->x0   = verts[a].vx;
        poly->y0   = verts[a].vy;
        poly->x1   = verts[b].vx;
        poly->y1   = verts[b].vy;
        poly->x2   = verts[c].vx;
        poly->y2   = verts[c].vy;
        poly->x3   = verts[d].vx;
        poly->y3   = verts[d].vy;
        addPrim((u32*)((((u32)(arg2 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt) + 3, poly);
        tpage          = gGpuPrimCursor;
        gGpuPrimCursor = tpage + 1;
        setlen(tpage, 1);
        tpage->code[0] = 0xE1000465;
        addPrim((u32*)((((u32)(arg2 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt) + 3, tpage);
        quad += 4;
    }
}

/// Lays out 24 screen-space points in `verts` as four rings of six around two
/// ends of a segment. The ends come from `D_dryfield_dilapidated_house_80186794`,
/// mirrored in x when the task's spawn arg 1 is 1, rotated by the parent task's
/// `DdhCoordWork` matrix; the far end is pulled toward the near one by the
/// parent's `field_4` ramp before both are moved by the matrix translation and
/// projected. Each ring's offsets are rotated to the segment's screen angle and
/// scaled by the projection distance over the last projected depth, which is
/// left in `*arg2` (`*arg3` gets the GTE flags). Rings 0 and 1 use the tables at
/// their natural size, rings 2 and 3 scaled by a factor that pulses with
/// `killCountdown`.
void func_dryfield_dilapidated_house_8017FAD4(Task* task, SVECTOR* verts, s32* arg2, s32* arg3)
{
    SVECTOR        a;
    SVECTOR        b;
    DdhRotMatrix   rot;
    DdhScreenPoint proj[2];
    DdhCoordWork*  work;
    MATRIX*        mtx;
    SVECTOR*       src;
    s16            t;
    s16            r;
    s32            scale;
    DdhMatWords*   words;
    s32            i;
    u16            f;
    u16            x0;
    s32            y0;
    u16            x1;
    s32            y1;
    s32            dx;
    s32            dy;
    s32            side;

    side = task->spawnArg1;
    work = ((Task*)task->spawnArg2)->work;
    mtx  = &work->mtx;
    f    = work->field_4;
    a.vx = D_dryfield_dilapidated_house_80186794[0].vx;
    a.vy = D_dryfield_dilapidated_house_80186794[0].vy;
    a.vz = D_dryfield_dilapidated_house_80186794[0].vz;
    src  = &D_dryfield_dilapidated_house_80186794[1];
    b.vx = src->vx;
    b.vy = src->vy;
    b.vz = src->vz;
    if (side == 1) {
        a.vx *= -1;
        b.vx *= -1;
    }
    gte_SetRotMatrix(mtx);
    gte_ldv0(&a);
    gte_mvmva_real();
    gte_stsv(&a);
    gte_ldv0(&b);
    gte_mvmva_real();
    gte_stsv(&b);
    t     = (s16)f * 0.75 + 1024.0;
    b.vx  = a.vx + (b.vx - a.vx) * t / 4096;
    b.vy  = a.vy + (b.vy - a.vy) * t / 4096;
    b.vz  = a.vz + (b.vz - a.vz) * t / 4096;
    a.vx += mtx->t[0];
    a.vy += mtx->t[1];
    a.vz += mtx->t[2];
    b.vx += mtx->t[0];
    b.vy += mtx->t[1];
    b.vz += mtx->t[2];
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(&a);
    gte_rtps_real();
    gte_stsxy(&proj[0].sxy);
    gte_stdp(&proj[0].depthCue);
    gte_stflg(arg3);
    gte_stszotz(arg2);
    gte_ldv0(&b);
    gte_rtps_real();
    gte_stsxy(&proj[1].sxy);
    gte_stdp(&proj[0].depthCue);
    gte_stflg(arg3);
    gte_stszotz(arg2);
    dy                = proj[0].sxy.vy - proj[1].sxy.vy;
    dx                = proj[1].sxy.vx - proj[0].sxy.vx;
    x1                = proj[1].sxy.vx;
    x0                = proj[0].sxy.vx;
    y0                = proj[0].sxy.vy;
    y1                = proj[1].sxy.vy;
    i                 = ratan2(dx, dy);
    scale             = gDisplayState.screenDistance;
    rot.words.m00_m01 = 0x1000;
    rot.words.m02_m10 = 0;
    words             = &rot.words;
    words->m11_m12    = 0x1000;
    rot.words.m20_m21 = 0;
    words->m22        = 0x1000;
    RotMatrixZ(i, &rot.mat);
    gte_SetRotMatrix(&rot.mat);
    for (i = 0; i < 6; i++) {
        a.vx = D_dryfield_dilapidated_house_801867A4[i].vx * scale / *arg2;
        a.vy = D_dryfield_dilapidated_house_801867A4[i].vy * scale / *arg2;
        gte_ldv0(&a);
        gte_mvmva_real();
        gte_stsv(&b);
        verts[i].vx = b.vx + x0;
        verts[i].vy = b.vy + y0;
    }
    USE_REG(x0);
    for (i = 0; i < 6; i++) {
        a.vx = D_dryfield_dilapidated_house_801867D4[i].vx * scale / *arg2;
        a.vy = D_dryfield_dilapidated_house_801867D4[i].vy * scale / *arg2;
        gte_ldv0(&a);
        gte_mvmva_real();
        gte_stsv(&b);
        verts[i + 6].vx = b.vx + x1;
        verts[i + 6].vy = b.vy + y1;
    }
    r = 4096.0 - rsin(task->killCountdown) * 0.5 + 4096.0;
    for (i = 0; i < 6; i++) {
        a.vx = ((D_dryfield_dilapidated_house_801867A4[i].vx * r) >> 12) * scale / *arg2;
        a.vy = ((D_dryfield_dilapidated_house_801867A4[i].vy * r) >> 12) * scale / *arg2;
        gte_ldv0(&a);
        gte_mvmva_real();
        gte_stsv(&b);
        verts[i + 12].vx = b.vx + x0;
        verts[i + 12].vy = b.vy + y0;
    }
    for (i = 0; i < 6; i++) {
        a.vx = ((D_dryfield_dilapidated_house_801867D4[i].vx * r) >> 12) * scale / *arg2;
        a.vy = ((D_dryfield_dilapidated_house_801867D4[i].vy * r) >> 12) * scale / *arg2;
        gte_ldv0(&a);
        gte_mvmva_real();
        gte_stsv(&b);
        verts[i + 18].vx = b.vx + x1;
        verts[i + 18].vy = b.vy + y1;
    }
}

/// Projects the two 16-vertex rings in `verts` (inner at 0..15, outer at
/// 16..31) and joins them with 16 semi-transparent `POLY_G4`s, wrapping the
/// last quad back to vertex 0. The inner edge is a grey whose level is the
/// parent task's `DdhCoordWork::field_8` clamped to 0x400 and scaled to 0..0xFF;
/// the outer edge is black. Each quad goes into the ordering table four entries
/// past its average depth, preceded by a `DR_TPAGE` selecting blend mode 3.
void func_dryfield_dilapidated_house_801803A4(Task* task, SVECTOR* verts)
{
    s32       sxy[32];
    s32       sz[16];
    CVECTOR   c0;
    CVECTOR   c1;
    POLY_G4*  prim;
    DR_TPAGE* tp;
    s16       level;
    s32       i;
    s32*      xy;
    SVECTOR*  v;
    s32*      p;
    s32*      z;

    v     = verts;
    p     = sxy;
    z     = sz;
    level = ((DdhCoordWork*)((Task*)task->spawnArg2)->work)->field_8;
    SetRotMatrix(&Gfx_ViewWorldMtx);
    SetTransMatrix(&Gfx_ViewWorldMtx);
    for (i = 0; i < 16; i++) {
        gte_ldv3(v, v + 1, v + 16);
        gte_rtpt_real();
        gte_stsxy3(p, p + 1, p + 16);
        gte_stszotz(z);
        p++;
        z++;
        v++;
    }
    if (level > 0x400) {
        level = 0x400;
    }
    c0.r = level * 0xFF / 0x400;
    c0.g = level * 0xFF / 0x400;
    c0.b = 0;
    c1.r = 0;
    c1.g = 0;
    c1.b = 0;
    for (i = 0; i < 16; i++) {
        prim           = (POLY_G4*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(prim + 1);
        setlen(prim, 8);
        setcode(prim, 0x3A);
        prim->r0 = c0.r;
        prim->g0 = c0.g;
        prim->b0 = c0.b;
        prim->r1 = c0.r;
        prim->g1 = c0.g;
        prim->b1 = c0.b;
        prim->r2 = c1.r;
        prim->g2 = c1.g;
        prim->b2 = c1.b;
        prim->r3 = c1.r;
        prim->g3 = c1.g;
        prim->b3 = c1.b;
        // Packed screen words, one per vertex; each `xy` word is two words past
        // the previous one because a colour word sits between them.
        xy = (s32*)&prim->x0;
        if (i < 15) {
            xy[0] = sxy[i];
            xy[2] = sxy[i + 1];
            xy[4] = sxy[i + 16];
            xy[6] = sxy[i + 17];
        } else {
            xy[0] = sxy[15];
            xy[2] = sxy[0];
            xy[4] = sxy[31];
            xy[6] = sxy[16];
        }
        addPrim((u_long*)(((((u32)sz[i] << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt) + 4, prim);
        tp             = (DR_TPAGE*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(tp + 1);
        setlen(tp, 1);
        tp->code[0] = 0xE1000465;
        addPrim((u_long*)(((((u32)sz[i] << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt) + 4, tp);
    }
}

void func_dryfield_dilapidated_house_80180738(Task* task, SVECTOR* verts)
{
    DdhAngleStep* work;
    DdhCoordWork* src;
    MATRIX*       mtx;
    SVECTOR*      ofs;
    SVECTOR*      ofs2;
    SVECTOR       pos[2];
    SVECTOR*      v0;
    SVECTOR*      v1;
    s16           tx;
    s16           ty;
    s16           tz;
    s32           i;
    s32           ang;
    s32           c;
    s32           s;

    v0 = verts;
    v1 = &verts[16];

    work = (DdhAngleStep*)task->work;
    src  = (DdhCoordWork*)((Task*)task->spawnArg2)->work;

    ofs       = D_dryfield_dilapidated_house_80186844;
    ofs2      = D_dryfield_dilapidated_house_80186844 + 1;
    pos[0].vx = ofs->vx;
    pos[0].vy = ofs->vy;
    pos[0].vz = ofs->vz;
    pos[1].vx = ofs2->vx;
    pos[1].vy = ofs2->vy;
    pos[1].vz = ofs2->vz;

    mtx = &src->mtx;

    tx = mtx->t[0];
    ty = mtx->t[1];
    tz = mtx->t[2];

    gte_SetRotMatrix(mtx);

    for (i = 0; i < 16; i++) {
        ang = (i << 12) >> 4;
        c   = rcos(ang);
        s   = rsin(ang);

        v0->vx = pos[0].vx + ((c * 0x96) >> 12);
        v0->vy = pos[0].vy + ((s * 0x4B) >> 12);
        v0->vz = pos[0].vz;

        gte_ldv0(v0);
        gte_mvmva_real();
        gte_stsv(v0);

        v0->vx += tx;
        v0->vy += ty;
        v0->vz += tz;
        v0++;

        v1->vx = pos[1].vx + ((c * 0xFA) >> 12);
        v1->vy = pos[1].vy + ((s * 0x7D) >> 12);
        v1->vz = pos[1].vz + ((rsin(work->step[i] >> 2) * 0x64) >> 12);

        work->step[i] = (work->step[i] + D_dryfield_dilapidated_house_80186804[i]) & 0x3FFF;

        gte_ldv0(v1);
        gte_mvmva_real();
        gte_stsv(v1);

        v1->vx += tx;
        v1->vy += ty;
        v1->vz += tz;
        v1++;
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_3", func_dryfield_dilapidated_house_80180A0C);

void func_dryfield_dilapidated_house_80180B84(Task* task)
{
    Task*          parent;
    TmdObject*     obj;
    TmdObject*     parentObj;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* parentCoord;
    DdhCoordWork*  work;
    DdhRoomRec*    rec;
    TmdSource*     source;
    SVECTOR*       dst;
    SVECTOR*       dst2;
    SVECTOR*       src2;
    SVECTOR*       verts;
    TaskDesc*      table;
    Task*          spawned;
    GsCOORDINATE2* childCoord;
    u16            flags;
    s32            i;

    parent      = (Task*)task->spawnArg2;
    obj         = (TmdObject*)task->extra;
    parentObj   = (TmdObject*)parent->extra;
    coord       = obj->coords;
    parentCoord = parentObj->coords;
    work        = (DdhCoordWork*)Mem_Malloc(0x6C, false);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work    = (TaskIdMap*)work;
    work->field_0 = 0;
    flags         = obj->flags | 0x80;
    obj->flags    = flags;
    if (!(parentObj->flags & 0x80)) {
        obj->flags = flags & 0xFF7F;
    }
    obj->otOffset = 4;
    obj->flags   |= 2;
    parentCoord  += task->spawnArg1;
    coord->flg    = 0;
    coord->sub    = parentCoord;
    obj->lightMtx = parentObj->lightMtx;
    obj->colorMtx = parentObj->colorMtx;
    Task_Reparent(parent, task);

    rec    = &D_dryfield_dilapidated_house_8018669C;
    source = ((TmdObject*)task->extra)->source;
    dst    = rec->field_8;
    dst2   = (SVECTOR*)rec->field_C;
    verts  = source->verts;
    for (i = 0; i < rec->field_10; i++) {
        dst[i].vx = verts[i].vx;
        dst[i].vy = verts[i].vy;
        dst[i].vz = verts[i].vz;
    }
    if (rec->field_4 != 0) {
        src2 = source->normals;
        for (i = 0; i < rec->field_12; i++) {
            dst2[i].vx = src2[i].vx;
            dst2[i].vy = src2[i].vy;
            dst2[i].vz = src2[i].vz;
        }
    }

    func_dryfield_dilapidated_house_80180FD8(task);

    table   = &D_dryfield_dilapidated_house_80186854;
    spawned = Task_SpawnFromTable(table, 3, 9, (s32)task);
    if (spawned != NULL) {
        childCoord        = ((TmdObject*)spawned->extra)->coords;
        childCoord->coord = work->mtx;
    }
    spawned = Task_SpawnFromTable(table, 3, 0x11, (s32)task);
    if (spawned != NULL) {
        childCoord        = ((TmdObject*)spawned->extra)->coords;
        childCoord->coord = work->mtx;
    }
    spawned = Task_SpawnFromTable(table, 2, 0, (s32)task);
    if (spawned != NULL) {
        childCoord        = ((TmdObject*)spawned->extra)->coords;
        childCoord->coord = work->mtx;
    }
    spawned = Task_SpawnFromTable(table, 2, 1, (s32)task);
    if (spawned != NULL) {
        childCoord        = ((TmdObject*)spawned->extra)->coords;
        childCoord->coord = work->mtx;
    }

    task->exitCallback = func_dryfield_dilapidated_house_80180FB8;
    task->state       += 1;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_3", func_dryfield_dilapidated_house_80180F04);

void func_dryfield_dilapidated_house_80180F5C(Task* arg0)
{
    DdhCoordWork* work;
    s32           temp_v0;

    work = (DdhCoordWork*)arg0->work;
    func_dryfield_dilapidated_house_801810F8((TmdObject*)arg0->extra,
                                             (TmdObject*)((Task*)arg0->spawnArg2)->extra);
    func_dryfield_dilapidated_house_80181028(arg0);
    temp_v0       = func_dryfield_dilapidated_house_80180FD8(arg0);
    work->field_0 = temp_v0;
    work->field_8 = temp_v0;
    work->field_4 = temp_v0;
}
