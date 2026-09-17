#include "common.h"

#include "main/display.h"
#include "rooms/dryfield_breezeway.h"

#include <psyq/libgpu.h>

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_3", func_dryfield_breezeway_8017EB8C);

/// Draws one segment of the breezeway's prompt beam: a raw-textured quad of
/// two `arg0`-rotated edges, eight halfwords wide, whose far edge is `arg1`
/// down the rotated frame from its near one. All five probe points go through
/// `RotTransSV` (so `arg0` has to be a real rotation: the identity matrix the
/// two `Set` calls start from is splatted word-wise and then handed to
/// `RotMatrixZ`), and the quad is carved from `Gpu_PrimCursor` and linked into
/// `Gpu_CurrentOt[0x64]` with the room's tpage 0x8E / clut 0x4000 texture.
///
/// `arg2` is the scan's own position, added to every projected point; the
/// rotated probe at (0, `arg1`, 0) -- the far edge's centre -- lands in `arg3`
/// as the segment's tip, which is the position the caller advances its cursor
/// to. `arg4` carries the near edge: `mode` 0 draws it from the `arg2` origin
/// (the first segment of a beam), anything else from the two corners stored in
/// `arg4`, which the tail of every call overwrites with the far edge -- so a
/// beam that keeps being redrawn starts where the previous segment ended.
///
/// Nothing is written to `arg4`'s corners on a `mode` 0 call beyond that tail,
/// which is what makes the first segment of a beam run from the origin.
void func_dryfield_breezeway_8017F1F4(s16 arg0, s16 arg1, DbwVec* arg2, DbwVec* arg3, DbwBeamEdge* arg4)
{
    SVECTOR     probe;
    DbwVec      tip;
    SVECTOR     near0;
    SVECTOR     near1;
    SVECTOR     far0;
    SVECTOR     far1;
    DbwVec      corner0;
    DbwVec      corner1;
    DbwVec      corner2;
    DbwVec      corner3;
    DbwMatWords matw;
    MATRIX*     mtx;
    long        flag;
    POLY_FT4*   p;

    mtx                  = &matw.mat;
    matw.ident.m00_m01   = 0x1000;
    matw.ident.m02_m10   = 0;
    *(s32*)&mtx->m[1][1] = 0x1000;
    matw.ident.m20_m21   = 0;
    mtx->m[2][2]         = 0x1000;
    matw.mat.t[0]        = 0;
    matw.mat.t[1]        = 0;
    matw.mat.t[2]        = 0;
    RotMatrixZ(arg0, &matw.mat);
    SetRotMatrix(&matw.mat);
    SetTransMatrix(&matw.mat);

    probe.vx = 0;
    probe.vy = arg1;
    probe.vz = 0;
    RotTransSV(&probe, (SVECTOR*)&tip, &flag);
    arg3->vx = arg2->vx + tip.vx;
    arg3->vy = arg2->vy + tip.vy;
    arg3->vz = arg2->vz + tip.vz;

    if (arg4->mode == 0) {
        near0.vx = -4;
        near0.vy = 0;
        near0.vz = 0;
        RotTransSV(&near0, (SVECTOR*)&corner0, &flag);
        near1.vx = 4;
        near1.vy = 0;
        near1.vz = 0;
        RotTransSV(&near1, (SVECTOR*)&corner1, &flag);
    }

    far0.vx = -4;
    far0.vy = arg1;
    far0.vz = 0;
    RotTransSV(&far0, (SVECTOR*)&corner2, &flag);
    far1.vx = 4;
    far1.vy = arg1;
    far1.vz = 0;
    RotTransSV(&far1, (SVECTOR*)&corner3, &flag);

    p              = (POLY_FT4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(p + 1);
    setPolyFT4(p);
    p->tpage = 0x8E;
    p->clut  = 0x4000;

    if (arg4->mode == 0) {
        p->x0 = corner0.vx + arg2->vx;
        p->y0 = corner0.vy + arg2->vy;
        p->x1 = corner1.vx + arg2->vx;
        p->y1 = corner1.vy + arg2->vy;
    } else {
        p->x0 = arg4->fromA.vx;
        p->y0 = arg4->fromA.vy;
        p->x1 = arg4->fromB.vx;
        p->y1 = arg4->fromB.vy;
    }
    p->x2 = corner2.vx + arg2->vx;
    p->y2 = corner2.vy + arg2->vy;
    p->x3 = corner3.vx + arg2->vx;
    p->y3 = corner3.vy + arg2->vy;

    p->u0 = 0;
    p->v0 = 0;
    p->u1 = 0x10;
    p->v1 = 0;
    p->u2 = 0;
    p->v2 = 4;
    p->u3 = 0x10;
    p->v3 = 4;

    setShadeTex(p, 1);
    addPrim(&Gpu_CurrentOt[0x64], p);

    arg4->fromA.vx = corner2.vx + arg2->vx;
    arg4->fromA.vy = corner2.vy + arg2->vy;
    arg4->fromB.vx = corner3.vx + arg2->vx;
    arg4->fromB.vy = corner3.vy + arg2->vy;
}
