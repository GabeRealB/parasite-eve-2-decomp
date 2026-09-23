#include "common.h"

#include "main/display.h"
#include "main/sound.h"
#include "rooms/dryfield_breezeway.h"

#include <psyq/libgpu.h>

void func_dryfield_breezeway_8017EB8C(Task* task, s16 arg1, s16 arg2)
{
    s32           x;
    s32           ay;
    s32           ty;
    SVECTOR       pos;
    SVECTOR       out;
    SVECTOR       target;
    DbwBeamEdge   edge;
    DbwEventWork* work;
    s32           dist;
    s32           cdist;
    s32           dx;
    s32           dy;
    s16           y;
    s16           sx;
    s16           sy;
    s32           scale;
    s32           px;
    s32           py;
    s32           r;
    s32           d;
    s32           i;
    s16           a;
    s16           angle;
    s16           t;

    RoomActionPrompt* prompt;

    x      = arg1;
    ay     = arg2;
    work   = (DbwEventWork*)task->work;
    prompt = &D_80114D28;
    ty     = ay + 0x50;
    dist   = SquareRoot0(x * x + ty * ty);
    sx     = work->cursorX;
    dx     = sx - prompt->screen.xy.x;
    sy     = work->cursorY;
    dy     = sy - prompt->screen.xy.y;
    cdist  = SquareRoot0(dx * dx + dy * dy);
    if (dist >= 0x70 || ay < -0x4F || cdist > 0x20) {
        prompt->mode   = 1;
        work->cursorY += work->field_52;
        dist           = 0x70;
        if (work->cursorY >= 0x20) {
            work->cursorY  = 0x20;
            work->field_52 = 0;
        } else {
            work->field_52++;
        }
        if (work->field_56 < 8) {
            work->field_58 = work->cursorX;
            if (work->cursorX > 0) {
                work->field_54 = (u16)(work->field_54 - 2) - work->field_56;
            }
            if (work->cursorX < 0) {
                work->field_54 = work->field_56 + (u16)(work->field_54 + 2);
            }
            work->cursorX += (s16)work->field_54 >> work->field_56;
            if ((work->cursorX > 0 && work->field_58 <= 0) || (work->cursorX < 0 && work->field_58 >= 0)) {
                work->field_56++;
            }
        }
    } else {
        prompt->mode   = 2;
        work->field_56 = 3;
        work->field_54 = 0;
        work->field_52 = 0;
        work->field_58 = work->cursorX;
        work->field_5A = work->cursorY;
        work->cursorX += (prompt->screen.xy.x - work->cursorX) >> 2;
        work->cursorY += (prompt->screen.xy.y - work->cursorY) >> 2;
        if (work->cursorX != work->field_58 || work->cursorY != work->field_5A) {
            SndEvt_EnqueueType6(0x5216000D, 0, 0);
        }
    }

    y     = work->cursorY;
    x     = work->cursorX;
    ty    = y + 0x50;
    scale = 0x800 - (ty << 12) / 224;
    scale = 0xE00 - scale;
    sx    = work->cursorX;
    sy    = work->cursorY;
    r     = 0x70 - dist;
    px    = (x * scale / 8) >> 9;
    py    = ((ty * scale / 8) >> 9) + ((r * scale / 8) >> 9);
    py   -= 0x50;

    target.vx = 0;
    target.vy = -0x50;
    target.vz = 0;
    pos.vx    = px;
    pos.vy    = py;
    pos.vz    = 0;
    a         = func_dryfield_breezeway_8017FBEC(0, -0x50, px, py);
    angle     = -((func_dryfield_breezeway_8017FBEC(px, py, x, y) + a) / 2) + 0x800;
    for (i = 0; i < 30; i++) {
        if (i == 0) {
            edge.mode = 0;
        } else {
            edge.mode = 1;
        }
        func_dryfield_breezeway_8017F1F4(angle, 4, (DbwVec*)&pos, (DbwVec*)&out, &edge);
        if (func_dryfield_breezeway_8017FAD0((DbwVec*)&target, (DbwVec*)&out) != 0) {
            break;
        }
        t   = angle + func_dryfield_breezeway_8017FBEC(out.vx, out.vy, 0, -0x50);
        d   = (t << 20) >> 20;
        pos = out;
        if (d > 0x200) {
            angle -= 0x200;
        } else if (d > 0x100) {
            angle -= 0x100;
        } else if (d > 0x80) {
            angle -= 0x80;
        } else if (d < -0x200) {
            angle += 0x200;
        } else if (d < -0x100) {
            angle += 0x100;
        } else if (d < -0x80) {
            angle += 0x80;
        } else {
            angle = -func_dryfield_breezeway_8017FBEC(out.vx, out.vy, 0, -0x50);
        }
    }

    target.vx = sx;
    target.vy = sy;
    target.vz = 0;
    pos.vx    = px;
    pos.vy    = py;
    pos.vz    = 0;
    a         = func_dryfield_breezeway_8017FBEC(0, -0x50, px, py);
    angle     = -((func_dryfield_breezeway_8017FBEC(px, py, sx, sy) + a) / 2);
    for (i = 0; i < 30; i++) {
        func_dryfield_breezeway_8017F1F4(angle, 4, (DbwVec*)&pos, (DbwVec*)&out, &edge);
        if (func_dryfield_breezeway_8017FAD0((DbwVec*)&target, (DbwVec*)&out) != 0) {
            break;
        }
        t   = angle + func_dryfield_breezeway_8017FBEC(out.vx, out.vy, sx, sy);
        d   = (t << 20) >> 20;
        pos = out;
        if (d > 0x200) {
            angle -= 0x200;
        } else if (d > 0x100) {
            angle -= 0x100;
        } else if (d > 0x80) {
            angle -= 0x80;
        } else if (d < -0x200) {
            angle += 0x200;
        } else if (d < -0x100) {
            angle += 0x100;
        } else if (d < -0x80) {
            angle += 0x80;
        } else {
            angle = -func_dryfield_breezeway_8017FBEC(out.vx, out.vy, sx, sy);
        }
    }
    func_dryfield_breezeway_8017FB30(task, out.vx, out.vy);
}

/// Draws one segment of the breezeway's prompt beam: a raw-textured quad of
/// two `arg0`-rotated edges, eight halfwords wide, whose far edge is `arg1`
/// down the rotated frame from its near one. All five probe points go through
/// `RotTransSV` (so `arg0` has to be a real rotation: the identity matrix the
/// two `Set` calls start from is splatted word-wise and then handed to
/// `RotMatrixZ`), and the quad is carved from `gGpuPrimCursor` and linked into
/// `gGpuCurrentOt[0x64]` with the room's tpage 0x8E / clut 0x4000 texture.
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

    p              = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
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
    addPrim(&gGpuCurrentOt[0x64], p);

    arg4->fromA.vx = corner2.vx + arg2->vx;
    arg4->fromA.vy = corner2.vy + arg2->vy;
    arg4->fromB.vx = corner3.vx + arg2->vx;
    arg4->fromB.vy = corner3.vy + arg2->vy;
}
