#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/gtemac.h>

#include "gameplay/1A8.h"
#include "gameplay/3688.h"
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
#include "main/wipsys.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

#define gte_TransposeMatrix(src, dst)     \
    __asm__ volatile("lhu $12,0(%0);"     \
                     "lhu $13,6(%0);"     \
                     "lhu $14,12(%0);"    \
                     "sh $12,0(%1);"      \
                     "sh $13,2(%1);"      \
                     "sh $14,4(%1);"      \
                     "lhu $12,2(%0);"     \
                     "lhu $13,8(%0);"     \
                     "lhu $14,14(%0);"    \
                     "sh $12,6(%1);"      \
                     "sh $13,8(%1);"      \
                     "sh $14,10(%1);"     \
                     "lhu $12,4(%0);"     \
                     "lhu $13,10(%0);"    \
                     "lhu $14,16(%0);"    \
                     "sh $12,12(%1);"     \
                     "sh $13,14(%1);"     \
                     "sh $14,16(%1);"     \
                     :                    \
                     : "r"(src), "r"(dst) \
                     : "$12", "$13", "$14", "memory")

extern s32 D_80070F70;

extern void func_807245E4(void*);
extern void func_80724608(void*, s32, s32, void*);

/// Part of the mirrored player model each held-object reflection hangs off,
/// indexed by `Task::spawnArg1`.
extern u8 D_acropolis_east_elevator_hall_8017FC8C[];

extern s32        D_acropolis_east_elevator_hall_80185C8C;
extern s32        D_acropolis_east_elevator_hall_80185D54;
extern s32        D_acropolis_east_elevator_hall_801860B4;
extern s32        D_acropolis_east_elevator_hall_8018621C;
extern GpMsgEntry D_acropolis_east_elevator_hall_801862F4[];
extern s32        D_acropolis_east_elevator_hall_8018631C;

/// Name word handed to `func_80724608`: `"Player"`, followed by one stray
/// non-zero byte C cannot reproduce, so it stays assembly.
extern char D_acropolis_east_elevator_hall_8017D5E0[];

/// The mirror task's descriptors: entry 0 spawns the mirror itself, entry 1
/// one reflection of a held object.
extern TaskDesc D_acropolis_east_elevator_hall_8017FC90[];

void func_acropolis_east_elevator_hall_8017D7A4(Task* task);
void func_acropolis_east_elevator_hall_8017F478(Task* task);
void func_acropolis_east_elevator_hall_8017F4E8(Task* task);

/// Scale handed to `ScaleMatrix` to flip the reflection across X.
const VECTOR D_acropolis_east_elevator_hall_8017D5C4 = { -0x1000, 0x1000, 0x1000, 0 };

/// State handlers of the room task: set-up, the per-frame tick and `taskKill`.
const TaskFuncTable3 D_acropolis_east_elevator_hall_8017D5D4 = {
    { func_acropolis_east_elevator_hall_8017F478, func_acropolis_east_elevator_hall_8017F4E8, taskKill },
};

/// Rotates `out` in place by `m` through the GTE.
static inline void _rotateOffset(MATRIX* m, SVECTOR* out)
{
    SVECTOR v;

    v = *out;
    gte_SetRotMatrix(m);
    gte_ldv0(&v);
    gte_rtv0();
    gte_stsv(out);
}

/// State 0 of the hall's mirror task. Re-attaches the player's own TMD source
/// to this task so the reflection draws the same model, allocates the
/// `RoomMirrorWork` block holding the reflection's coordinate frame and
/// matrices, and reparents the task under the player task so it dies with it.
/// `spawnArg1` must be 0 or 1, otherwise the task kills itself; 0 also raises
/// `GameSession::field_4E`. For each held-object task the player has
/// (`GameActor::field_920` / `field_924`) it spawns a reflection task and
/// hangs it under that held object, then runs the first per-frame update.
void func_acropolis_east_elevator_hall_8017D5F0(Task* task)
{
    Task*           owner;
    GameActor*      actor;
    TmdObject*      extra;
    GpCoord*        parts;
    RoomMirrorWork* work;
    Task*           child;
    Task*           spawned;
    s32             i;

    owner = gameGetPtrSlot(3);
    if (Gp_AttachTmd(task, owner->extra.tmd->source) == NULL) {
        taskKill(task);
        return;
    }
    extra = task->extra.tmd;
    parts = extra->coords;
    if ((u32)task->spawnArg1 >= 2U) {
        taskKill(task);
        return;
    }
    work = memCalloc(sizeof(RoomMirrorWork), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work   = (TaskIdMap*)work;
    extra->tpage = 6;
    tmdProcessStream(extra);
    tmdProcessStream(extra);
    extra->flags    = 0x10;
    extra->otOffset = 0x1F;
    if (task->spawnArg1 == 0) {
        gGameSession->field_4E = 1;
    }
    parts->sub      = &work->coord;
    extra->lightMtx = &work->light;
    extra->colorMtx = &work->color;
    Task_Reparent(owner, task);
    task->state++;
    work->viewFlg   = gGfxViewCoord.flg & 0x7FFFFFFF;
    work->field_4   = 1;
    work->configRev = -1;
    extra->flags   |= 0x80;
    work->field_4   = 0;
    work->viewFlg   = -1;
    actor           = (GameActor*)owner->work;
    for (i = 0; i < 2; i++) {
        child = (&actor->field_920)[i];
        if (child != NULL) {
            spawned = Task_SpawnFromTable(D_acropolis_east_elevator_hall_8017FC90, 1, i, (s32)task);
            if (spawned != NULL) {
                Task_Reparent(child, spawned);
            }
        }
    }
    func_acropolis_east_elevator_hall_8017D7A4(task);
}

/// State 1 of the hall's mirror task, run every frame after
/// `func_acropolis_east_elevator_hall_8017D5F0` has set the mirror up.
///
/// When the player's equipped weapon changes it spawns reflection tasks for
/// the player's two held-object tasks. When the view moves it rebuilds the
/// reflection's coordinate frame: mirror 0 copies the view matrix with its
/// second row negated and applies location-specific corrections, any other
/// mirror reflects through a plane chosen by the current stage, area and view.
/// On the frame after mirror 0 rebuilds, it queues packets that copy the frame
/// buffer into the off-screen strip at x = `width`. In stages 1 and 5 it
/// projects the reflected body to find its screen rectangle and, where that
/// overlaps the mirror's clip rectangle, draws quads sampling that strip;
/// otherwise the reflection is hidden. Every frame it copies the player's pose
/// and light matrices onto the reflection.
void func_acropolis_east_elevator_hall_8017D7A4(Task* task)
{
    RoomMirrorWork*          work;
    PlayerStatus*            status;
    TmdObject*               extra;
    TmdObject*               model;
    Task*                    owner;
    GameActor*               actor;
    Task*                    child;
    Task*                    spawned;
    RoomMirrorPlaneScratch*  plane;
    RoomMirrorExtentScratch* extent;
    GpCoord*                 parts;
    GpCoord*                 refPart;
    DR_AREA*                 drArea;
    DR_STP*                  drStp;
    DR_OFFSET*               drOffset;
    SPRT*                    sprt;
    DR_TPAGE*                tpage;
    TILE*                    tile;
    POLY_FT4*                poly;
    s32                      stage;
    s32                      area;
    s32                      view;
    s32                      width;
    s32                      viewFlg;
    s32                      copyPending;
    s32                      halfWidth;
    s32                      texX;
    s32                      i;
    s32                      layer;
    u32                      j;

    width  = 0x1C0;
    work   = task->work;
    extra  = task->extra.tmd;
    stage  = Mc_SaveData.at4.loc.stage;
    area   = Mc_SaveData.at4.loc.area;
    view   = Mc_SaveData.at4.loc.view;
    status = &Player_Status;
    if (stage == 5) {
        width = 0x140;
    }
    if (work->configRev != status->weapon) {
        actor           = gameGetPtrSlot(3)->work;
        work->configRev = status->weapon;
        for (i = 0; i < 2; i++) {
            child = (&actor->field_918)[i];
            if (child != NULL) {
                spawned = Task_SpawnFromTable(D_acropolis_east_elevator_hall_8017FC90, 1, i + 2, (s32)task);
                if (spawned != NULL) {
                    Task_Reparent(child, spawned);
                }
            }
        }
    }
    extra->flags |= 0x10;
    viewFlg       = gGfxViewCoord.flg & 0x7FFFFFFF;
    if (work->viewFlg != viewFlg) {
        GpCoord* sub;

        work->viewFlg     = viewFlg;
        sub               = gGfxViewCoord.sub;
        work->field_A0[0] = -0xA0;
        work->field_A0[1] = 0xA0;
        work->coord.flg   = 0;
        work->field_A0[2] = -0x78;
        work->field_A0[3] = 0x78;
        plane             = (RoomMirrorPlaneScratch*)SCRATCH_PUSH_BYTES(0x70);
        work->coord.sub   = sub;
        if (task->spawnArg1 == 0) {
            work->field_4     = 1;
            work->coord.coord = gGfxViewCoord.coord;
            plane->viewRow.vx = work->coord.coord.m[1][0];
            plane->viewRow.vy = work->coord.coord.m[1][1];
            plane->viewRow.vz = work->coord.coord.m[1][2];
            gte_lddp(-0x1000);
            gte_ldsv(&plane->viewRow);
            gte_gpf12();
            gte_stsv(&plane->viewRow);
            work->coord.coord.m[1][0] = plane->viewRow.vx;
            work->coord.coord.m[1][1] = plane->viewRow.vy;
            work->coord.coord.m[1][2] = plane->viewRow.vz;
            if (stage == 5) {
                if (area == 7) {
                    if (view >= 6 && view < 12 && gGameSession->at4.loc.room == 2) {
                        work->coord.coord.t[1] += 0x9B;
                        extra->flags           &= ~0x80;
                        work->field_8           = 0;
                    } else {
                        work->field_4 = 0;
                        extra->flags |= 0x80;
                    }
                }
            } else if (area == 1) {
                extra->flags |= 0x80;
                if (view == 9) {
                    work->field_4 = 0;
                }
            } else {
                if (area != 0x11) {
                    work->coord.coord.t[1] += 0x69;
                }
                work->field_8 = 1;
                if ((area == 0x11 && view == 5) || (area == 2 && (view == 7 || view == 5))) {
                    extra->flags |= 0x80;
                } else {
                    extra->flags &= ~0x80;
                }
            }
        } else {
            model         = task->extra.tmd;
            model->flags &= ~0x80;
            if (stage == 1) {
                switch (area) {
                    case 0x11:
                        switch (view) {
                            case 2:
                                plane->normal.vx = -0x1000;
                                plane->normal.vy = 0;
                                plane->normal.vz = 0;
                                VectorNormalSS(&plane->normal, &plane->normal);
                                plane->offset.vx = -0x1518;
                                plane->offset.vy = 0;
                                plane->offset.vz = 0;
                                break;
                            case 3:
                                plane->normal.vx = 0x64;
                                plane->normal.vy = 0;
                                plane->normal.vz = -0x384;
                                VectorNormalSS(&plane->normal, &plane->normal);
                                plane->offset.vx = 0;
                                plane->offset.vy = 0;
                                plane->offset.vz = -0x640;
                                break;
                            case 4:
                                work->field_A0[0] = -0x14;
                                work->field_A0[1] = 0x14;
                                plane->normal.vx  = -0x1000;
                                plane->normal.vy  = 0;
                                plane->normal.vz  = 0;
                                VectorNormalSS(&plane->normal, &plane->normal);
                                plane->offset.vx = 0x1644;
                                plane->offset.vy = 0;
                                plane->offset.vz = 0;
                                break;
                            default:
                                model->flags |= 0x80;
                                break;
                        }
                        break;
                    case 1:
                        switch (view) {
                            case 6:
                                work->field_A0[1] = 0x64;
                                work->field_A0[0] = 0;
                                plane->normal.vx  = -0x1000;
                                plane->normal.vy  = 0;
                                plane->normal.vz  = 0;
                                VectorNormalSS(&plane->normal, &plane->normal);
                                plane->offset.vx = 0x1AF4;
                                plane->offset.vy = 0;
                                plane->offset.vz = 0;
                                model->otOffset  = 0x1F;
                                break;
                            case 7:
                            case 8:
                                plane->normal.vx = 0;
                                plane->normal.vy = 0;
                                plane->normal.vz = 0x1000;
                                VectorNormalSS(&plane->normal, &plane->normal);
                                plane->offset.vx = 0;
                                plane->offset.vy = 0;
                                plane->offset.vz = 0x14B4;
                                break;
                            default:
                                model->flags |= 0x80;
                                break;
                        }
                        break;
                    case 2:
                        switch (view) {
                            case 2:
                            case 5:
                                plane->normal.vx = -0x1000;
                                plane->normal.vy = 0;
                                plane->normal.vz = 0;
                                VectorNormalSS(&plane->normal, &plane->normal);
                                plane->offset.vx = 0x170C;
                                plane->offset.vy = 0;
                                plane->offset.vz = 0;
                                break;
                            case 4:
                                plane->normal.vx = -0x1000;
                                plane->normal.vy = 0;
                                plane->normal.vz = 0;
                                VectorNormalSS(&plane->normal, &plane->normal);
                                plane->offset.vx = -0x1644;
                                plane->offset.vy = 0;
                                plane->offset.vz = 0;
                                break;
                            case 3:
                                plane->normal.vx = -0x64;
                                plane->normal.vy = 0;
                                plane->normal.vz = -0x384;
                                VectorNormalSS(&plane->normal, &plane->normal);
                                plane->offset.vx = 0;
                                plane->offset.vy = 0;
                                plane->offset.vz = -0x640;
                                break;
                            default:
                                model->flags |= 0x80;
                                break;
                        }
                        break;
                    default:
                        model->flags |= 0x80;
                        break;
                }
            } else if (view == 8 || view == 1) {
                plane->normal.vx = -0x1000;
                plane->normal.vy = 0;
                plane->normal.vz = 0;
                VectorNormalSS(&plane->normal, &plane->normal);
                plane->offset.vx = 0xA38;
                plane->offset.vy = 0;
                plane->offset.vz = 0;
            } else {
                model->flags |= 0x80;
            }
            if (!(model->flags & 0x80)) {
                plane->leastAbs = plane->normal.vx;
                if (plane->leastAbs < 0) {
                    plane->leastAbs = -plane->leastAbs;
                }
                plane->leastAxis = 0;
                plane->axisAbs   = plane->normal.vy;
                if (plane->axisAbs < 0) {
                    plane->axisAbs = -plane->axisAbs;
                }
                if (plane->leastAbs > plane->axisAbs) {
                    plane->leastAbs  = plane->axisAbs;
                    plane->leastAxis = 1;
                }
                plane->axisAbs = plane->normal.vz;
                if (plane->axisAbs < 0) {
                    plane->axisAbs = -plane->axisAbs;
                }
                if (plane->leastAbs > plane->axisAbs) {
                    plane->leastAbs  = plane->axisAbs;
                    plane->leastAxis = 2;
                }
                plane->refAxis.vx = 0;
                if (plane->leastAxis == 0) {
                    plane->refAxis.vx = 0x1000;
                }
                plane->refAxis.vy = 0;
                if (plane->leastAxis == 1) {
                    plane->refAxis.vy = 0x1000;
                }
                plane->refAxis.vz = 0;
                if (plane->leastAxis == 2) {
                    plane->refAxis.vz = 0x1000;
                }
                Gfx_OrthonormalBasis(&plane->basis, &plane->normal, &plane->refAxis);
                gte_TransposeMatrix(&plane->basis, &plane->reflect);
                plane->reflect.m[2][0] = -plane->reflect.m[2][0];
                plane->reflect.m[2][1] = -plane->reflect.m[2][1];
                plane->reflect.m[2][2] = -plane->reflect.m[2][2];
                gte_MulMatrix0(&plane->basis, &plane->reflect, &plane->reflect);
                work->coord.coord      = plane->reflect;
                work->coord.coord.t[0] = gGfxViewCoord.coord.t[0] + plane->offset.vx;
                work->coord.coord.t[1] = gGfxViewCoord.coord.t[1] + plane->offset.vy;
                work->coord.coord.t[2] = gGfxViewCoord.coord.t[2] + plane->offset.vz;
                _rotateOffset(&plane->reflect, &plane->offset);
                work->coord.coord.t[0] -= plane->offset.vx;
                work->coord.coord.t[1] -= plane->offset.vy;
                work->coord.coord.t[2] -= plane->offset.vz;
                work->field_8           = 1;
            }
        }
        work->field_C = extra->flags;
        SCRATCH_POP_BYTES(0x70);
    }

    copyPending = work->field_4;
    if (copyPending == 1 && task->spawnArg1 == 0 && !(area == 1 && view == 0xF) && gDisplayState.pendingMode == 0) {
        u16  ofs[2];
        RECT rect;

        work->field_4   = 0;
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
        ofs[0]          = width + 0xA0;
        ofs[1]          = 0x178;
        SetDrawOffset(drOffset, ofs);
        addPrim(&gGpuCurrentOt[0x3FF], drOffset);

        drArea          = (DR_AREA*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(DR_AREA);
        rect.x          = width;
        rect.y          = 0x100;
        rect.w          = 0x140;
        rect.h          = 0xF0;
        SetDrawArea(drArea, &rect);
        addPrim(&gGpuCurrentOt[0x3FF], drArea);
    }

    extra->flags = work->field_C;
    if (!(extra->flags & 0x80) && gGameSession->field_65 == 0) {
        parts   = task->extra.tmd->coords;
        owner   = gameGetPtrSlot(3);
        refPart = &parts[1];
        if (owner != NULL) {
            TmdObject* src       = owner->extra.tmd;
            GpCoord*   srcCoords = src->coords;

            parts->flg = 0;
            j          = 0;
            if (src->partCount != 0) {
                GpCoord* from = (GpCoord*)&srcCoords->coord;
                GpCoord* to   = (GpCoord*)&parts->coord;

                do {
                    *(MATRIX*)to = *(MATRIX*)from;
                    to++;
                    from++;
                } while (++j < src->partCount);
            }
        }
        if (stage == 1 || stage == 5) {
            extent = (RoomMirrorExtentScratch*)SCRATCH_PUSH_BYTES(0x34);
            if (gGameSession->eventState != 0) {
                Gp_UpdateCoord(refPart);
                gte_SetTransMatrix(&refPart->workm);
                gte_SetRotMatrix(&refPart->workm);
                extent->pos.vx = 0;
                extent->pos.vy = -0x3E8;
                extent->pos.vz = 0;
                gte_RotTransPers(&extent->pos, &extent->sxyHead, &extent->dp, &extent->flag, &extent->otzHead);
                extent->pos.vx = 0;
                extent->pos.vy = 0x3E8;
                extent->pos.vz = 0;
                gte_RotTransPers(&extent->pos, &extent->sxyFoot, &extent->dp, &extent->flag, &extent->otzFoot);
            } else {
                Gp_UpdateCoord(parts);
                gte_SetTransMatrix(&parts->workm);
                gte_SetRotMatrix(&parts->workm);
                extent->pos.vx = 0;
                extent->pos.vy = -0x7D0;
                extent->pos.vz = 0;
                gte_RotTransPers(&extent->pos, &extent->sxyHead, &extent->dp, &extent->flag, &extent->otzHead);
                extent->pos.vx = 0;
                extent->pos.vy = 0;
                extent->pos.vz = 0;
                gte_RotTransPers(&extent->pos, &extent->sxyFoot, &extent->dp, &extent->flag, &extent->otzFoot);
            }
            if (extent->sxyFoot.vy > extent->sxyHead.vy) {
                extent->sxyHead.vx = extent->sxyFoot.vy;
                extent->sxyFoot.vy = extent->sxyHead.vy;
                extent->sxyHead.vy = extent->sxyHead.vx;
            }
            extent->sxyFoot.vy -= 0x10;
            extent->sxyHead.vy += 0x10;
            halfWidth           = (extent->sxyHead.vy - extent->sxyFoot.vy) >> 1;
            if (halfWidth >= 0x60) {
                halfWidth = 0x5F;
            }
            if ((*(u32*)&Mc_SaveData.at4.loc & 0xFF00FF) == 0x20005) {
                if (task->spawnArg1 == 0) {
                    halfWidth = 0x5F;
                } else {
                    extent->otzFoot = extent->otzHead + 0xA;
                }
            }
            extent->left = extent->sxyFoot.vx - halfWidth;
            if (extent->left < -0xA0) {
                extent->left = -0xA0;
            }
            extent->right = extent->sxyFoot.vx + halfWidth;
            if (extent->right > 0xA0) {
                extent->right = 0xA0;
            }
            extent->top = extent->sxyFoot.vy;
            if (extent->top < -0x78) {
                extent->top = -0x78;
            }
            extent->bottom = extent->sxyHead.vy;
            if (extent->bottom > 0x78) {
                extent->bottom = 0x78;
            }
            if (extent->top < work->field_A0[3] && work->field_A0[2] < extent->bottom && extent->left < work->field_A0[1] &&
                work->field_A0[0] < extent->right) {
                DR_TPAGE* mode;

                mode            = (DR_TPAGE*)gGpuPrimCursor;
                texX            = extent->left + (u16)(width + 0xA0);
                extent->texX    = texX & 0xFFC0;
                gGpuPrimCursor += sizeof(DR_TPAGE);
                setDrawTPage(mode, 0, 1, 0);
                addPrim(&gGpuCurrentOt[(((extent->otzFoot << gDisplayState.otDepthShift) & 0x3FFF) >> 4) + extra->otOffset - 15],
                        mode);
                for (layer = work->field_8; layer < 3; layer++) {
                    poly            = (POLY_FT4*)gGpuPrimCursor;
                    gGpuPrimCursor += sizeof(POLY_FT4);
                    setPolyFT4(poly);
                    setSemiTrans(poly, 1);
                    if (work->field_8 == 1) {
                        setShadeTex(poly, 0);
                        poly->r0 = poly->g0 = poly->b0 = 0x80;
                    } else {
                        setShadeTex(poly, 1);
                    }
                    poly->x0 = poly->x2 = extent->left;
                    poly->x1 = poly->x3 = extent->right;
                    poly->y0 = poly->y1 = extent->top;
                    poly->y2 = poly->y3 = extent->bottom;
                    poly->tpage         = getTPage(2, layer, extent->texX, 0x100);
                    poly->u0 = poly->u2 = poly->x0 + 0xA0 + width - extent->texX;
                    poly->u1 = poly->u3 = poly->x1 + 0xA0 + width - extent->texX;
                    poly->v0 = poly->v1 = poly->y0 + 0x78;
                    poly->v2 = poly->v3 = poly->y2 + 0x78;
                    addPrim(&gGpuCurrentOt[(((extent->otzFoot << gDisplayState.otDepthShift) & 0x3FFF) >> 4) + extra->otOffset - 15],
                            poly);
                }
                mode            = (DR_TPAGE*)gGpuPrimCursor;
                gGpuPrimCursor += sizeof(DR_TPAGE);
                setDrawTPage(mode, 0, 0, 0);
                addPrim(&gGpuCurrentOt[(((extent->otzFoot << gDisplayState.otDepthShift) & 0x3FFF) >> 4) + extra->otOffset - 15],
                        mode);
            } else {
                extra->flags |= 0x80;
            }
            SCRATCH_POP_BYTES(0x34);
        }
    }

    {
        GpCoord*   ownerParts;
        TmdObject* ownerBody;
        GpCoord*   ownParts;
        MATRIX     mtx;

        ownerParts  = gameGetPtrSlot(3)->extra.tmd->coords;
        ownerBody   = gameGetPtrSlot(3)->extra.tmd;
        ownParts    = task->extra.tmd->coords;
        work->light = *ownerBody->lightMtx;
        work->color = *ownerBody->colorMtx;
        Gp_UpdateCoord(ownParts);
        gte_TransposeMatrix(&ownParts->workm, &mtx);
        gte_MulMatrix0(&ownerParts->workm, &mtx, &mtx);
        gte_MulMatrix0(&work->light, &mtx, &work->light);
    }
}

/// Per-frame callback of a held-object reflection. `Task::spawnArg2` is the
/// hall's mirror task and the parent is the held-object task being reflected.
/// On the first frame it clones the parent's TMD source, parents the clone's
/// root coordinate to the mirrored player's corresponding part, points the
/// clone at the mirror's light and color matrices and negates the X
/// translation, flipping the clone across X when `spawnArg1` is 2 or more;
/// every frame it copies the mirror model's draw flags onto the clone.
void func_acropolis_east_elevator_hall_8017F128(Task* task)
{
    Task*           mirror;
    TmdObject*      mirrorExtra;
    RoomMirrorWork* work;
    GpCoord*        mirrorPart;
    TmdObject*      src;
    GpCoord*        srcParts;
    TmdObject*      extra;
    GpCoord*        parts;
    VECTOR          scale;
    u16             flags;

    if (task->parent == NULL) {
        Task_CallExit(task);
    }
    mirror      = (Task*)task->spawnArg2;
    mirrorPart  = &mirror->extra.tmd->coords[D_acropolis_east_elevator_hall_8017FC8C[task->spawnArg1]];
    work        = (RoomMirrorWork*)mirror->work;
    mirrorExtra = mirror->extra.tmd;
    if (task->state == 0) {
        src      = task->parent->extra.tmd;
        srcParts = src->coords;
        if (Gp_AttachTmd(task, src->source) == NULL) {
            Task_CallExit(task);
            return;
        }
        extra        = task->extra.tmd;
        parts        = extra->coords;
        extra->tpage = src->tpage;
        tmdProcessStream(extra);
        tmdProcessStream(extra);
        extra->flags    = 0x10;
        extra->otOffset = 0x1F;
        parts->sub      = mirrorPart;
        extra->lightMtx = &work->light;
        extra->colorMtx = &work->color;
        if (task->spawnArg1 >= 2) {
            scale = D_acropolis_east_elevator_hall_8017D5C4;
            ScaleMatrix(&parts->coord, &scale);
        }
        parts->coord.t[0] = -srcParts->coord.t[0];
        parts->coord.t[1] = srcParts->coord.t[1];
        parts->coord.t[2] = srcParts->coord.t[2];
        parts->flg        = 0;
        task->state++;
    }
    extra        = task->extra.tmd;
    flags        = mirrorExtra->flags;
    extra->flags = flags;
    if (task->spawnArg1 >= 2) {
        extra->flags = flags & 0xFFEF;
    }
}

/// Runs the hall's mirror task: state 0 sets the mirror up, state 1 is its
/// per-frame update.
void func_acropolis_east_elevator_hall_8017F2F8(Task* task)
{
    TaskFunc states[2] = {
        func_acropolis_east_elevator_hall_8017D5F0,
        func_acropolis_east_elevator_hall_8017D7A4,
    };

    states[task->state](task);
}

/// Message handler that copies the incoming location record onto the
/// outgoing one and answers 1.
s32 func_acropolis_east_elevator_hall_8017F348(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    return 1;
}

/// Message handler that accepts the message and does nothing else.
s32 func_acropolis_east_elevator_hall_8017F370(void)
{
    return 0;
}

s32 func_acropolis_east_elevator_hall_8017F378(Task* task, s32 msgId, GpMsg13EF* arg2, s32 arg3)
{
    if (arg2->field_2 == 0 && GameFlag_GetNibble(0) == 0 && D_acropolis_east_elevator_hall_8018631C == 0) {
        func_800E8634((s32)&D_acropolis_east_elevator_hall_80185D54, 0, (s32)&D_acropolis_east_elevator_hall_801860B4);
        D_acropolis_east_elevator_hall_8018631C = 1;
        GameFlag_SetNibble(0, 1);
        GameFlag_SetNibble(3, 0);
        GameFlag_SetNibble(0x155, 3);
        GameFlag_SetNibble(8, 2);
        func_800E3FAC(0xA2, 2);
    }
    return 0;
}

s32 func_acropolis_east_elevator_hall_8017F420(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 2) {
        func_800E8614((s32)&D_acropolis_east_elevator_hall_8018621C, 0);
    }
    return 0;
}

void func_acropolis_east_elevator_hall_8017F450(void)
{
    Gp_StartCapSlot(0x10, 1, 0);
}

void func_acropolis_east_elevator_hall_8017F478(Task* task)
{
    task->msgTable = D_acropolis_east_elevator_hall_801862F4;
    Game_SetPtrSlot(task, 7);
    Gp_MsgSlot4Chain(0, 1);
    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_acropolis_east_elevator_hall_80185C8C, 0);
    task->state++;
}

INCLUDE_RODATA("rooms/nonmatchings/acropolis_east_elevator_hall/acropolis_east_elevator_hall", D_acropolis_east_elevator_hall_8017D5E0);

void func_acropolis_east_elevator_hall_8017F4E8(Task* task)
{
    if (gDisplayState.field_112 != 0) {
        func_807245E4(gameGetPtrSlot(3));
        if (gDisplayState.field_112 != 0) {
            func_80724608(gameGetPtrSlot(3), -0x8C, -0x32, &D_acropolis_east_elevator_hall_8017D5E0);
        }
    }
}

/// Runs the room task's current state through a stack copy of the room's
/// three-entry state table.
void func_acropolis_east_elevator_hall_8017F55C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_east_elevator_hall_8017D5D4;
    sp.funcs[task->state](task);
}

/// Position of the first of the six effects
/// `func_acropolis_east_elevator_hall_8017F5B4` spawns in view 2; the other
/// five positions are written into the local copy in turn.
const SVECTOR D_acropolis_east_elevator_hall_8017D5E8 = { 0x1600, -0x964, 0x540, 0 };

void func_acropolis_east_elevator_hall_8017F5B4(Task* task)
{
    GpCoord* coord;

    coord = task->extra.tmd->coords;
    switch (task->state) {
        case 0:
            Task_Spawn(1, 0x25, 0, 0);
            Task_Spawn(1, 0x25, 1, 0);
            task->state++;
            /* fallthrough */
        case 1:
            if ((u8)gGameSession->at4.loc.view == 2) {
                SVECTOR vec = D_acropolis_east_elevator_hall_8017D5E8;

                Gp_SpawnEff(0x60022, coord, 0xC03, &vec);
                vec.vx = 0x1600;
                vec.vy = -0x985;
                vec.vz = 0x55;
                Gp_SpawnEff(0x60022, coord, 0xC03, &vec);
                vec.vx = 0x1600;
                vec.vy = -0xA81;
                vec.vz = -0x1CA;
                Gp_SpawnEff(0x60022, coord, 0x1204, &vec);
                vec.vx = 0x1600;
                vec.vy = -0xA93;
                vec.vz = -0x61C;
                Gp_SpawnEff(0x60022, coord, 0x1204, &vec);
                vec.vx = 0x1600;
                vec.vy = -0x460;
                vec.vz = -0x1A1;
                Gp_SpawnEff(0x60022, coord, 0x1204, &vec);
                vec.vx = 0x1600;
                vec.vy = -0x449;
                vec.vz = -0x635;
                Gp_SpawnEff(0x60022, coord, 0x1204, &vec);
            }
            break;
    }
}

/// Draws a pulsing light shaft at the task's coordinate origin. The origin is
/// projected once through `GsWSMATRIX` (`RTPS`) into a 0x14-byte
/// `G_SCRATCH_HEAD` block; anything with `otz` below 0x11 is dropped.
/// `spawnArg1`'s low byte scales the frame counter `D_80070F70`, and the
/// product's low byte is folded into a 0..0x80 triangle wave that drives the
/// red channel of one corner; its high byte is the shaft length, divided by
/// `otz` so the two `POLY_G4` halves narrow with distance.
void func_acropolis_east_elevator_hall_8017F77C(Task* arg0)
{
    u8*               head;
    u8*               raw;
    RoomShaftScratch* block;
    POLY_G4*          prim;
    GpCoord*          coord;
    void*             mem;
    u16               vz;
    s32               i;
    s32               red;
    s32               pulse;
    s32               level;

    coord = arg0->extra.tmd->coords;
    mem   = arg0->spawnArg2;
    Gp_UpdateCoord(coord);
    head = SCRATCH_HEAD(void);
    raw  = head - 0x14;
    /* `raw` and `block` have to stay separate registers: the ROM computes the
       block address into a scratch register and copies it into the callee-saved
       one the rest of the function uses. */
    SOFT_TOUCH_REG(raw);
    block              = (RoomShaftScratch*)raw;
    block->vec.vx      = (u16)coord->workm.t[0];
    block->vec.vy      = (u16)coord->workm.t[1];
    vz                 = (u16)coord->workm.t[2];
    SCRATCH_HEAD(void) = block;
    block->vec.vz      = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomShaftScratch*)(head - 0x14))->vec);
    gte_rtps();
    gte_stsxy(&((RoomShaftScratch*)(head - 0x14))->sx);
    gte_stszotz(&block->otz);
    if (((RoomShaftScratch*)(head - 0x14))->otz >= 0x11) {
        pulse = D_80070F70 * ((RoomShaftArg*)&arg0->spawnArg1)->phase;
        if (pulse & 0x80) {
            level = 0x80 - (pulse & 0x7F);
        } else {
            level = pulse & 0x7F;
        }
        /* Same split for the ramp: the ROM keeps the triangle result in a
           scratch register and copies it into the callee-saved `red`. */
        red              = level;
        block->halfWidth = (((RoomShaftArg*)&arg0->spawnArg1)->height << 9) / block->otz;
        for (i = 0; i < 2; i++) {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, red, 0, 0);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx - block->halfWidth;
            prim->x1 = prim->x2 = block->sx;
            prim->x3            = block->sx + block->halfWidth;
            prim->y0 = prim->y2 = prim->y3 = block->sy;
            prim->y1                       = (block->sy - block->halfWidth) + block->halfWidth * (i + i);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP_BYTES(0x14);
    Gp_ReleaseState1CMem(mem, arg0);
}

void func_acropolis_east_elevator_hall_8017FAAC(Task* arg0)
{
    void**                    scratch;
    u8*                       head;
    register RoomMoteScratch* block asm("v1");
    TILE_1*                   prim;
    GpCoord*                  coord;
    void*                     mem;
    u16                       vz;

    scratch = (void**)G_SCRATCH_HEAD;
    coord   = arg0->extra.tmd->coords;
    mem     = arg0->spawnArg2;
    Gp_UpdateCoord(coord);
    head          = *scratch;
    block         = (RoomMoteScratch*)(head - 0xC);
    block->vec.vx = (u16)coord->workm.t[0];
    block->vec.vy = (u16)coord->workm.t[1];
    vz            = (u16)coord->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomMoteScratch*)(head - 0xC))->vec);
    gte_rtps();
    prim           = (TILE_1*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setTile1(prim);
    gte_stsxy(&prim->x0);
    gte_stszotz(&block->otz);
    if (((RoomMoteScratch*)(head - 0xC))->otz >= 0x11) {
        setRGB0(prim, 0x80, 0x80, 0x80);
        addPrim((u_long*)(((((u32)((RoomMoteScratch*)(head - 0xC))->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
        Gp_AddTpageShift((P_TAG*)prim, 1, ((RoomMoteScratch*)(head - 0xC))->otz);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0xC);
    Gp_ReleaseState1CMem(mem, arg0);
}
