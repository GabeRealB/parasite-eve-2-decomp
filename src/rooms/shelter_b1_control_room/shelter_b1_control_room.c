#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/gtemac.h>
#include "gte.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

/// The mirror's configuration, filled in by `func_shelter_b1_control_room_8017D600`
/// whenever the view moves. `active` other than 1 hides the reflection.
/// `copyPending` set to 1 makes the next frame copy the frame buffer into the
/// off-screen strip at x = `stripX`, after which it is cleared. `mode` 1 is a
/// floor mirror, which reflects the view through its second row and raises it
/// by `normal.vy`; any other mode reflects through the plane `normal` placed at
/// `offset` from the view. `firstLayer` is the first of the three blend layers
/// the reflection quads are drawn in, and a negative value skips drawing them.
/// `subject` is the task whose body the mirror reflects, and a `field_18` of 1
/// as the task starts makes it exit instead.
typedef struct {
    s32     active;
    s32     copyPending;
    s32     firstLayer;
    s32     mode;
    s32     field_10;
    s32     stripX;
    s32     field_18;
    Task*   subject;
    SVECTOR normal;
    SVECTOR offset;
} _MirrorCfg;

/// The mirror task's `Task::work`. `viewFlg` caches
/// `gGfxViewCoord.flg & 0x7FFFFFFF` so the frame is rebuilt only when the view
/// moves. `coord` is the reflected frame the clone's root part hangs from,
/// `light` and `color` the matrices the clone is drawn under, and `clip` the
/// screen rectangle (left, right, top, bottom) the reflection may cover.
typedef struct {
    s32        viewFlg;
    GpCoord    coord;
    MATRIX     light;
    MATRIX     color;
    s16        clip[4];
    byte       unknown_9C[4];
    _MirrorCfg cfg;
} _MirrorWork;

/// Scratchpad block the mirror takes for one frame. `refAxis` is the
/// coordinate axis least aligned with the plane normal (found through
/// `leastAbs` / `axisAbs`, with the axis index in `flag`), `basis` the
/// orthonormal frame built from the two and `reflect` the reflection matrix
/// derived from it, and `offset` the plane offset rotated into that frame.
/// The rest locates the reflection on screen: two points on the reflected body
/// are projected through `pos` into `sxyHead` / `otzHead` and `sxyFoot` /
/// `otzFoot`, `left` .. `bottom` is the rectangle the quads cover and `texX`
/// the x of the texture page they sample the off-screen strip from.
typedef struct {
    SVECTOR pos;
    SVECTOR refAxis;
    MATRIX  basis;
    MATRIX  reflect;
    SVECTOR offset;
    s16     leastAbs;
    byte    unknown_5A[2];
    s16     axisAbs;
    byte    unknown_5E[2];
    DVECTOR sxyFoot;
    DVECTOR sxyHead;
    u16     texX;
    s32     dp;
    s32     flag;
    s32     otzFoot;
    s32     otzHead;
    s32     left;
    s32     right;
    s32     top;
    s32     bottom;
} _MirrorScratch;

extern s32        func_80179A04(RoomEventMsg* in, RoomEventMsg* out);
extern void       func_80131FB8(void);
extern GpMsgEntry D_shelter_b1_control_room_80181B94[];
extern s32        D_80132D70;
extern s32        D_80133088;

void func_shelter_b1_control_room_8017D600(Task* task, _MirrorCfg* cfg);

/// Applies `m` to `v` through the GTE and stores the result in `out`.
static inline void _applyMatrixSV(MATRIX* m, SVECTOR* v, SVECTOR* out)
{
    gte_SetRotMatrix(m);
    gte_ldv0(v);
    gte_rtv0();
    gte_stsv(out);
}

/// Fills in `cfg` for the current area key.
///
/// Every field starts from a default that leaves the mirror inactive, with the
/// player task (`gameGetPtrSlot(3)`) as its subject. Three places turn it on,
/// each for a set of views: area 7 of stage 5 while the session is in room 2,
/// area 0x1E of stages 2 and 3, and area 0x12 of stage 4. In stage 4's area the
/// subject becomes the `Gp_LookupSlot4(0)` task instead, and only when `place`
/// is 0xB; with any other `place`, `field_18` is set so the mirror task exits
/// on its first frame.
///
/// The `do { } while (0)` is not logic. Its loop notes act as a scheduling
/// barrier: without it, the scheduler would move the shared constant 1 down to
/// its first store, below the key reads.
void func_shelter_b1_control_room_8017D600(Task* task, _MirrorCfg* cfg)
{
    s32        stage;
    s32        area;
    s32        view;
    GpAreaKey* key;
    s32        one;

    key = &Mc_SaveData.at4.loc;
    one = 1;
    do {
        stage = key->stage;
        area  = key->area;
        view  = key->view;
    } while (0);
    cfg->stripX      = 0x1C0;
    cfg->active      = 0;
    cfg->copyPending = 0;
    cfg->firstLayer  = 0;
    cfg->mode        = one;
    cfg->offset.vy   = 0;
    cfg->field_10    = one;
    cfg->field_18    = 0;
    switch (stage) {
        case 5:
            if (area == 7 && (u32)(view - 6) < 6 && gGameSession->at4.loc.room == 2) {
                cfg->offset.vy   = 0x9B;
                cfg->active      = 1;
                cfg->copyPending = 1;
            }
            break;
        case 4:
            if (area == 0x12) {
                if (key->place == 0xB) {
                    cfg->field_10 = 0;
                    cfg->subject  = (Task*)Gp_LookupSlot4(0);
                    if (view == 4 || view == 1) {
                        cfg->normal.vz   = -0x1000;
                        cfg->offset.vz   = -0xABE;
                        cfg->mode        = 0;
                        cfg->normal.vx   = 0;
                        cfg->normal.vy   = 0;
                        cfg->offset.vx   = 0;
                        cfg->offset.vy   = 0;
                        cfg->active      = 1;
                        cfg->copyPending = 1;
                        cfg->stripX      = 0x140;
                        cfg->firstLayer  = 1;
                    }
                } else {
                    cfg->field_18 = 1;
                }
            }
            break;
        case 2:
        case 3:
            if (area == 0x1E && (view == 8 || view == 1)) {
                cfg->offset.vx   = 0xA38;
                cfg->firstLayer  = -1;
                cfg->normal.vx   = 0;
                cfg->normal.vz   = 0;
                cfg->offset.vy   = 0;
                cfg->offset.vz   = 0;
                cfg->active      = 1;
                cfg->copyPending = 1;
            }
            break;
    }
    if (cfg->field_10 == 1) {
        cfg->subject = gameGetPtrSlot(3);
    }
}

/// Per-frame update of the room's mirror task.
///
/// On the first frame it attaches the subject's TMD source to this task so the
/// clone draws the same model, hangs the clone's root part from the reflected
/// frame and adopts the subject task. When the view moves it rebuilds the
/// reflected frame, and on request it copies the frame buffer into the
/// off-screen strip. While active it copies the subject's pose and matrices
/// onto the clone, projects the clone to find its screen rectangle and, where
/// that overlaps the clip rectangle, draws quads sampling the strip.
void func_shelter_b1_control_room_8017D7B8(Task* task)
{
    _MirrorWork*    work;
    _MirrorCfg*     cfg;
    _MirrorScratch* scratch;
    TmdObject*      model;
    TmdObject*      src;
    TmdObject*      body;
    GpCoord*        parts;
    GpCoord*        refPart;
    GpCoord*        from;
    GpCoord*        to;
    DR_AREA*        drArea;
    DR_STP*         drStp;
    DR_OFFSET*      drOffset;
    SPRT*           sprt;
    DR_TPAGE*       tpage;
    TILE*           tile;
    POLY_FT4*       poly;
    s32             copyPending;
    s32             halfWidth;
    s32             texX;
    s32             texBase;
    GpCoord*        sub;
    s32             layer;
    u16             ofs[2];
    RECT            rect;

    if (task->state == 0) {
        work = memCalloc(0xD0, 0);
        cfg  = &work->cfg;
        if (work == NULL) {
            goto exit;
        }
        task->work = work;
        func_shelter_b1_control_room_8017D600(task, cfg);
        if (cfg->field_18 == 1) {
            goto exit;
        }
        body = cfg->subject->extra.tmd;
        if (Gp_AttachTmd(task, body->source) == NULL) {
        exit:
            Task_CallExit(task);
            return;
        }
        model        = task->extra.tmd;
        parts        = model->coords;
        model->clut  = body->clut;
        model->tpage = body->tpage;
        tmdProcessStream(model);
        tmdProcessStream(model);
        model->otOffset        = 0x16;
        model->flags           = 0x10;
        gGameSession->field_4E = 1;
        parts->sub             = &work->coord;
        model->lightMtx        = &work->light;
        model->colorMtx        = &work->color;
        Task_Reparent(cfg->subject, task);
        work->viewFlg = -1;
        task->state++;
    }

    scratch = (_MirrorScratch*)SCRATCH_PUSH_BYTES(0x8C);
    model   = task->extra.tmd;
    work    = task->work;
    parts   = model->coords;
    cfg     = &work->cfg;
    if (work->viewFlg != (gGfxViewCoord.flg & 0x7FFFFFFF)) {
        work->viewFlg = gGfxViewCoord.flg & 0x7FFFFFFF;
        func_shelter_b1_control_room_8017D600(task, cfg);
        if (work->cfg.active == 1) {
            sub             = gGfxViewCoord.sub;
            work->clip[0]   = -0xA0;
            work->clip[1]   = 0xA0;
            work->clip[2]   = -0x78;
            work->coord.flg = 0;
            work->clip[3]   = 0x78;
            work->coord.sub = sub;
            if (cfg->mode == 1) {
                work->coord.coord          = gGfxViewCoord.coord;
                work->coord.coord.m[1][0] *= -1;
                work->coord.coord.m[1][1] *= -1;
                work->coord.coord.m[1][2] *= -1;
                work->coord.coord.t[1]    += cfg->normal.vy;
            } else {
                scratch->leastAbs = cfg->normal.vx;
                if (scratch->leastAbs < 0) {
                    scratch->leastAbs = -scratch->leastAbs;
                }
                scratch->flag    = 0;
                scratch->axisAbs = cfg->normal.vy;
                if (scratch->axisAbs < 0) {
                    scratch->axisAbs = -scratch->axisAbs;
                }
                if (scratch->leastAbs > scratch->axisAbs) {
                    scratch->leastAbs = scratch->axisAbs;
                    scratch->flag     = 1;
                }
                scratch->axisAbs = cfg->normal.vz;
                if (scratch->axisAbs < 0) {
                    scratch->axisAbs = -scratch->axisAbs;
                }
                if (scratch->leastAbs > scratch->axisAbs) {
                    scratch->leastAbs = scratch->axisAbs;
                    scratch->flag     = 2;
                }
                scratch->refAxis.vx = 0;
                if (scratch->flag == 0) {
                    scratch->refAxis.vx = 0x1000;
                }
                scratch->refAxis.vy = 0;
                if (scratch->flag == 1) {
                    scratch->refAxis.vy = 0x1000;
                }
                scratch->refAxis.vz = 0;
                if (scratch->flag == 2) {
                    scratch->refAxis.vz = 0x1000;
                }
                Gfx_OrthonormalBasis(&scratch->basis, &cfg->normal, &scratch->refAxis);
                gte_TransposeMatrix(&scratch->basis, &scratch->reflect);
                scratch->reflect.m[2][0] = -scratch->reflect.m[2][0];
                scratch->reflect.m[2][1] = -scratch->reflect.m[2][1];
                scratch->reflect.m[2][2] = -scratch->reflect.m[2][2];
                gte_MulMatrix0(&scratch->basis, &scratch->reflect, &scratch->reflect);
                work->coord.coord      = scratch->reflect;
                work->coord.coord.t[0] = gGfxViewCoord.coord.t[0] + cfg->offset.vx;
                work->coord.coord.t[1] = gGfxViewCoord.coord.t[1] + cfg->offset.vy;
                work->coord.coord.t[2] = gGfxViewCoord.coord.t[2] + cfg->offset.vz;
                _applyMatrixSV(&scratch->reflect, &cfg->offset, &scratch->offset);
                work->coord.coord.t[0] -= scratch->offset.vx;
                work->coord.coord.t[1] -= scratch->offset.vy;
                work->coord.coord.t[2] -= scratch->offset.vz;
            }
        }
    }

    copyPending = cfg->copyPending;
    if (copyPending == 1 && gDisplayState.pendingMode == 0) {
        drArea          = (DR_AREA*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(DR_AREA);
        rect.x          = 0;
        rect.y          = gDisplayState.drawBuffer * 0x110;
        rect.w          = 0x140;
        rect.h          = 0xF0;
        SetDrawArea(drArea, &rect);
        addPrim(&gGpuCurrentOt[0x3FF], drArea);

        drStp           = (DR_STP*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(DR_STP);
        SetDrawStp(drStp, 0);
        addPrim(&gGpuCurrentOt[0x3FF], drStp);

        drOffset        = (DR_OFFSET*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(DR_OFFSET);
        ofs[0]          = 0xA0;
        ofs[1]          = gDisplayState.drawBuffer * 0x110 + 0x78;
        SetDrawOffset(drOffset, ofs);
        addPrim(&gGpuCurrentOt[0x3FF], drOffset);

        sprt            = (SPRT*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(SPRT);
        sprt->x0        = -0xA0;
        sprt->y0        = -0x78;
        sprt->w         = 0xA0;
        sprt->h         = 0xF0;
        sprt->u0        = 0;
        sprt->v0        = gDisplayState.drawBuffer << 4;
        setlen(sprt, 4);
        setcode(sprt, 0x65);
        addPrim(&gGpuCurrentOt[0x3FF], sprt);

        tpage           = (DR_TPAGE*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(DR_TPAGE);
        setDrawTPage(tpage, 1, 1, getTPage(2, 0, 0, gDisplayState.drawBuffer << 8));
        addPrim(&gGpuCurrentOt[0x3FF], tpage);

        sprt            = (SPRT*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(SPRT);
        sprt->x0        = 0;
        sprt->y0        = -0x78;
        sprt->w         = 0xA0;
        sprt->h         = 0xF0;
        sprt->u0        = 0x20;
        sprt->v0        = gDisplayState.drawBuffer << 4;
        setlen(sprt, 4);
        setcode(sprt, 0x65);
        addPrim(&gGpuCurrentOt[0x3FF], sprt);

        tpage           = (DR_TPAGE*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(DR_TPAGE);
        setDrawTPage(tpage, 1, 1, getTPage(2, 0, 0x80, gDisplayState.drawBuffer << 8));
        addPrim(&gGpuCurrentOt[0x3FF], tpage);

        tile            = (TILE*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(TILE);
        setlen(tile, 3);
        setcode(tile, 0x60);
        tile->x0 = -0xA0;
        tile->y0 = -0x78;
        tile->r0 = tile->g0 = 2;
        tile->b0            = 2;
        tile->w             = 0x140;
        tile->h             = 0xF0;
        addPrim(&gGpuCurrentOt[0x3FF], tile);

        drStp           = (DR_STP*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(DR_STP);
        SetDrawStp(drStp, 1);
        addPrim(&gGpuCurrentOt[0x3FF], drStp);

        drOffset        = (DR_OFFSET*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(DR_OFFSET);
        ofs[0]          = cfg->stripX + 0xA0;
        ofs[1]          = 0x178;
        SetDrawOffset(drOffset, ofs);
        addPrim(&gGpuCurrentOt[0x3FF], drOffset);

        drArea          = (DR_AREA*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(DR_AREA);
        rect.x          = cfg->stripX;
        rect.y          = 0x100;
        rect.w          = 0x140;
        rect.h          = 0xF0;
        SetDrawArea(drArea, &rect);
        addPrim(&gGpuCurrentOt[0x3FF], drArea);

        cfg->copyPending = 0;
    }

    if (cfg->active == 1) {
        src           = cfg->subject->extra.tmd;
        refPart       = &task->extra.tmd->coords[1];
        from          = src->coords;
        model->flags &= ~0x80;
        work->light   = *src->lightMtx;
        work->color   = *src->colorMtx;
        Gp_UpdateCoord(parts);
        gte_TransposeMatrix(&parts->workm, &scratch->basis);
        gte_MulMatrix0(&from->workm, &scratch->basis, &scratch->basis);
        gte_MulMatrix0(&work->light, &scratch->basis, &work->light);
        parts->flg = 0;
        to         = parts;
        for (layer = 0; layer < (u32)src->partCount; layer++) {
            to->coord = from->coord;
            to++;
            from++;
        }
        if (cfg->firstLayer >= 0) {
            if (gGameSession->eventState != 0) {
                Gp_UpdateCoord(refPart);
                gte_SetTransMatrix(&refPart->workm);
                gte_SetRotMatrix(&refPart->workm);
                scratch->pos.vx = 0;
                scratch->pos.vy = -0x3E8;
                scratch->pos.vz = 0;
                gte_RotTransPers(&scratch->pos, &scratch->sxyHead, &scratch->dp, &scratch->flag, &scratch->otzHead);
                scratch->pos.vx = 0;
                scratch->pos.vy = 0x3E8;
                scratch->pos.vz = 0;
                gte_RotTransPers(&scratch->pos, &scratch->sxyFoot, &scratch->dp, &scratch->flag, &scratch->otzFoot);
            } else {
                Gp_UpdateCoord(parts);
                gte_SetTransMatrix(&parts->workm);
                gte_SetRotMatrix(&parts->workm);
                scratch->pos.vx = 0;
                scratch->pos.vy = -0x7D0;
                scratch->pos.vz = 0;
                gte_RotTransPers(&scratch->pos, &scratch->sxyHead, &scratch->dp, &scratch->flag, &scratch->otzHead);
                scratch->pos.vx = 0;
                scratch->pos.vy = 0;
                scratch->pos.vz = 0;
                gte_RotTransPers(&scratch->pos, &scratch->sxyFoot, &scratch->dp, &scratch->flag, &scratch->otzFoot);
            }
            if (scratch->sxyFoot.vy > scratch->sxyHead.vy) {
                scratch->sxyHead.vx = scratch->sxyFoot.vy;
                scratch->sxyFoot.vy = scratch->sxyHead.vy;
                scratch->sxyHead.vy = scratch->sxyHead.vx;
            }
            scratch->sxyFoot.vy -= 0x10;
            scratch->sxyHead.vy += 0x10;
            halfWidth            = (scratch->sxyHead.vy - scratch->sxyFoot.vy) >> 1;
            if (halfWidth >= 0x60) {
                halfWidth = 0x5F;
            }
            scratch->left = scratch->sxyFoot.vx - halfWidth;
            if (scratch->left < -0xA0) {
                scratch->left = -0xA0;
            }
            scratch->right = scratch->sxyFoot.vx + halfWidth;
            if (scratch->right > 0xA0) {
                scratch->right = 0xA0;
            }
            scratch->top = scratch->sxyFoot.vy;
            if (scratch->top < -0x78) {
                scratch->top = -0x78;
            }
            scratch->bottom = scratch->sxyHead.vy;
            if (scratch->bottom > 0x78) {
                scratch->bottom = 0x78;
            }
            if (scratch->top < work->clip[3] && work->clip[2] < scratch->bottom && scratch->left < work->clip[1] &&
                work->clip[0] < scratch->right) {
                DR_TPAGE* mode;

                mode            = (DR_TPAGE*)gGpuPrimCursor;
                texBase         = cfg->stripX + 0xA0;
                texX            = scratch->left + texBase;
                scratch->texX   = texX & 0xFFC0;
                gGpuPrimCursor += sizeof(DR_TPAGE);
                setDrawTPage(mode, 0, 1, 0);
                addPrim(&gGpuCurrentOt[(((scratch->otzFoot << gDisplayState.otDepthShift) & 0x3FFF) >> 4) + model->otOffset - 15],
                        mode);
                for (layer = cfg->firstLayer; layer < 3; layer++) {
                    poly            = (POLY_FT4*)gGpuPrimCursor;
                    gGpuPrimCursor += sizeof(POLY_FT4);
                    setPolyFT4(poly);
                    setSemiTrans(poly, 1);
                    if (cfg->firstLayer == 1) {
                        setShadeTex(poly, 0);
                        poly->r0 = poly->g0 = poly->b0 = 0x80;
                    } else {
                        setShadeTex(poly, 1);
                    }
                    poly->x0 = poly->x2 = scratch->left;
                    poly->x1 = poly->x3 = scratch->right;
                    poly->y0 = poly->y1 = scratch->top;
                    poly->y2 = poly->y3 = scratch->bottom;
                    poly->tpage         = getTPage(2, layer, scratch->texX, 0x100);
                    poly->u0 = poly->u2 = poly->x0 + 0xA0 + cfg->stripX - scratch->texX;
                    poly->u1 = poly->u3 = poly->x1 + 0xA0 + cfg->stripX - scratch->texX;
                    poly->v0 = poly->v1 = poly->y0 + 0x78;
                    poly->v2 = poly->v3 = poly->y2 + 0x78;
                    addPrim(&gGpuCurrentOt[(((scratch->otzFoot << gDisplayState.otDepthShift) & 0x3FFF) >> 4) + model->otOffset - 15],
                            poly);
                }
                mode            = (DR_TPAGE*)gGpuPrimCursor;
                gGpuPrimCursor += sizeof(DR_TPAGE);
                setDrawTPage(mode, 0, 0, 0);
                addPrim(&gGpuCurrentOt[(((scratch->otzFoot << gDisplayState.otDepthShift) & 0x3FFF) >> 4) + model->otOffset - 15],
                        mode);
            }
        }
    } else {
        model->flags |= 0x80;
    }
    SCRATCH_POP_BYTES(0x8C);
}

s32 func_shelter_b1_control_room_8017ECCC(void)
{
    return 0;
}

s32 func_shelter_b1_control_room_8017ECD4(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    if (in->msgId != 0x13) {
        return 1;
    }
    if (GameFlag_GetNibble(0xAD) != 0) {
        return 1;
    }
    if (in->field_5 != 0) {
        return 0;
    }
    Gp_SetNibbleIf(in->field_6, 2);
    Gp_RunCapCmd1(1);
    return 0;
}

s32 func_shelter_b1_control_room_8017ED68(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 3:
            if (GameFlag_GetNibble(0xF7) != 0) {
                Gp_RunCapCmd1(6);
            } else {
                Gp_RunCapCmd1(3);
            }
            break;
        case 4:
            if (GameFlag_GetNibble(0xF7) != 0) {
                Gp_RunCapCmd1(7);
            } else {
                Gp_RunCapCmd1(4);
            }
            break;
        case 5:
            if (GameFlag_GetNibble(0xF7) != 0) {
                Gp_RunCapCmd1(8);
            } else {
                Gp_RunCapCmd1(5);
            }
            break;
        case 9:
            if (GameFlag_GetNibble(0x7A) == 6) {
                Gp_RunCapCmd1(9);
            }
            break;
    }
    return 0;
}

s32 func_shelter_b1_control_room_8017EE24(void)
{
    return 0;
}

void func_shelter_b1_control_room_8017EE2C(Task* arg0)
{
    arg0->msgTable = D_shelter_b1_control_room_80181B94;
    Game_SetPtrSlot(arg0, 7);
    if (gGameSession->at4.loc.place == 0xB) {
        func_80131FB8();
        if (Mc_SaveData.demoScene != 9) {
            func_800E8634((s32)&D_80132D70, 0, (s32)&D_80133088);
        }
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// Idle state of the room task: does nothing, and only reserves a 0x10-byte
/// stack frame.
void func_shelter_b1_control_room_8017EEBC(Task* task)
{
    char pad[0x10];
}

/// States of the room task `func_shelter_b1_control_room_8017EECC`: the setup
/// state `func_shelter_b1_control_room_8017EE2C`, the idle state
/// `func_shelter_b1_control_room_8017EEBC`, then `taskKill`.
const TaskFuncTable3 D_shelter_b1_control_room_8017D5C4 = {
    { func_shelter_b1_control_room_8017EE2C, func_shelter_b1_control_room_8017EEBC, taskKill },
};

/// The room task: runs the handler for its state from a stack copy of
/// `D_shelter_b1_control_room_8017D5C4`.
void func_shelter_b1_control_room_8017EECC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_control_room_8017D5C4;
    sp.funcs[task->state](task);
}
