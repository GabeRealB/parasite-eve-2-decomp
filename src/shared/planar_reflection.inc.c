#include "planar_reflection.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/gtemac.h>

#include "gte.h"
#include "types.h"

#include "gameplay/actor_render.h"
#include "gameplay/model_objects.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "rooms/room.h"
#include "rooms/room_common.h"

static void Reflection_InitPlayer(Task* task);
static void Reflection_UpdatePlayer(Task* task);
static void Reflection_HeldObjectTask(Task* task);

#ifndef PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION
#error "Define PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION as 0 or 1"
#elif PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION != 0 && PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION != 1
#error "PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION must be 0 or 1"
#elif PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION
#include "planar_reflection_rodata.inc.c"
#endif

/// State 0 of the hall's mirror task. Re-attaches the player's own TMD source
/// to this task so the reflection draws the same model, allocates the
/// `RoomMirrorWork` block holding the reflection's coordinate frame and
/// matrices, and reparents the task under the player task so it dies with it.
/// `spawnArg1` must be 0 or 1, otherwise the task kills itself; 0 also raises
/// `GameSession::field_4E`. For each held-object task the player has
/// (`GameActor::attachmentTasks`) it spawns a reflection task and
/// hangs it under that held object, then runs the first per-frame update.
static void Reflection_InitPlayer(Task* task)
{
    Task*           owner;
    GameActor*      actor;
    TmdObject*      extra;
    GfxCoord*       parts;
    RoomMirrorWork* work;
    Task*           child;
    Task*           spawned;
    s32             i;

    owner = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (Gp_AttachTmd(task, owner->extra.tmd->source) == NULL) {
        taskKill(task);
        return;
    }
    extra = task->extra.tmd;
    parts = extra->coords;
    if ((u32)task->spawnArg1.value >= 2U) {
        taskKill(task);
        return;
    }
    work = memCalloc(sizeof(RoomMirrorWork), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work               = work;
    extra->texturePageOffset = 6;
    tmdProcessStream(extra);
    tmdProcessStream(extra);
    extra->flags    = TMD_OBJECT_REVERSE_CULLING;
    extra->otOffset = 0x1F;
    if (task->spawnArg1.value == 0) {
        gGameSession->field_4E = 1;
    }
    parts->parent   = &work->coord;
    extra->lightMtx = &work->light;
    extra->colorMtx = &work->color;
    taskReparent(owner, task);
    task->state++;
    work->viewFlg   = gGfxViewCoord.composeStamp & GRAPHICS_COORD_STAMP_MASK;
    work->field_4   = 1;
    work->configRev = -1;
    extra->flags   |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->field_4   = 0;
    work->viewFlg   = -1;
    actor           = (GameActor*)owner->work;
    for (i = 0; i < 2; i++) {
        child = actor->attachmentTasks[i];
        if (child != NULL) {
            spawned = Task_SpawnFromTable(Reflection_GetTasks(), 1, i, task);
            if (spawned != NULL) {
                taskReparent(child, spawned);
            }
        }
    }
    Reflection_UpdatePlayer(task);
}

/// State 1 of the hall's mirror task, run every frame after
/// `Reflection_InitPlayer` has set the mirror up.
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
static void Reflection_UpdatePlayer(Task* task)
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
    GfxCoord*                parts;
    GfxCoord*                refPart;
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
    stage  = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage;
    area   = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area;
    view   = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view;
    status = &gPlayerStatus;
    if (stage == 5) {
        width = 0x140;
    }
    if (work->configRev != status->weapon) {
        actor           = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
        work->configRev = status->weapon;
        for (i = 0; i < 2; i++) {
            child = actor->equipmentTasks[i];
            if (child != NULL) {
                spawned = Task_SpawnFromTable(Reflection_GetTasks(), 1, i + 2, task);
                if (spawned != NULL) {
                    taskReparent(child, spawned);
                }
            }
        }
    }
    extra->flags |= TMD_OBJECT_REVERSE_CULLING;
    viewFlg       = gGfxViewCoord.composeStamp & GRAPHICS_COORD_STAMP_MASK;
    if (work->viewFlg != viewFlg) {
        GfxCoord* viewParent;

        work->viewFlg            = viewFlg;
        viewParent               = gGfxViewCoord.parent;
        work->field_A0[0]        = -0xA0;
        work->field_A0[1]        = 0xA0;
        work->coord.composeStamp = GRAPHICS_COORD_DIRTY;
        work->field_A0[2]        = -0x78;
        work->field_A0[3]        = 0x78;
        plane                    = (RoomMirrorPlaneScratch*)SCRATCH_STACK_RESERVE_BYTES(0x70);
        work->coord.parent       = viewParent;
        if (task->spawnArg1.value == 0) {
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
                    if (view >= 6 && view < 12 && gGameSession->location.loc.room == 2) {
                        work->coord.coord.t[1] += 0x9B;
                        extra->flags           &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
                        work->field_8           = 0;
                    } else {
                        work->field_4 = 0;
                        extra->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    }
                }
            } else if (area == 1) {
                extra->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                if (view == 9) {
                    work->field_4 = 0;
                }
            } else {
                if (area != 0x11) {
                    work->coord.coord.t[1] += 0x69;
                }
                work->field_8 = 1;
                if ((area == 0x11 && view == 5) || (area == 2 && (view == 7 || view == 5))) {
                    extra->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                } else {
                    extra->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
                }
            }
        } else {
            model         = task->extra.tmd;
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
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
                                model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
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
                                model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
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
                                model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                                break;
                        }
                        break;
                    default:
                        model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
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
                model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            if (!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
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
                gfxRotateSv(&plane->reflect, &plane->offset);
                work->coord.coord.t[0] -= plane->offset.vx;
                work->coord.coord.t[1] -= plane->offset.vy;
                work->coord.coord.t[2] -= plane->offset.vz;
                work->field_8           = 1;
            }
        }
        work->field_C = extra->flags;
        SCRATCH_STACK_RELEASE_BYTES(0x70);
    }

    copyPending = work->field_4;
    if (copyPending == 1 && task->spawnArg1.value == 0 && !(area == 1 && view == 0xF) && gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
        u16  ofs[2];
        RECT rect;

        work->field_4  = 0;
        drArea         = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_AREA);
        rect.x         = 0;
        rect.y         = gDisplayState.drawBuffer * 0x110;
        rect.w         = 0x140;
        rect.h         = 0xF0;
        SetDrawArea(drArea, &rect);
        addPrim(&gGpuCurrentOt[0x3FF], drArea);

        drStp          = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_STP);
        SetDrawStp(drStp, 0);
        addPrim(&gGpuCurrentOt[0x3FF], drStp);

        drOffset       = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_OFFSET);
        ofs[0]         = 0xA0;
        ofs[1]         = gDisplayState.drawBuffer * 0x110 + 0x78;
        SetDrawOffset(drOffset, ofs);
        addPrim(&gGpuCurrentOt[0x3FF], drOffset);

        sprt           = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(SPRT);
        sprt->x0       = -0xA0;
        sprt->y0       = -0x78;
        sprt->w        = 0xA0;
        sprt->h        = 0xF0;
        sprt->u0       = 0;
        sprt->v0       = gDisplayState.drawBuffer << 4;
        setlen(sprt, 4);
        setcode(sprt, 0x65);
        addPrim(&gGpuCurrentOt[0x3FF], sprt);

        tpage          = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_TPAGE);
        setDrawTPage(tpage, 1, 1, getTPage(2, 0, 0, gDisplayState.drawBuffer << 8));
        addPrim(&gGpuCurrentOt[0x3FF], tpage);

        sprt           = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(SPRT);
        sprt->x0       = 0;
        sprt->y0       = -0x78;
        sprt->w        = 0xA0;
        sprt->h        = 0xF0;
        sprt->u0       = 0x20;
        sprt->v0       = gDisplayState.drawBuffer << 4;
        setlen(sprt, 4);
        setcode(sprt, 0x65);
        addPrim(&gGpuCurrentOt[0x3FF], sprt);

        tpage          = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_TPAGE);
        setDrawTPage(tpage, 1, 1, getTPage(2, 0, 0x80, gDisplayState.drawBuffer << 8));
        addPrim(&gGpuCurrentOt[0x3FF], tpage);

        tile           = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(TILE);
        setlen(tile, 3);
        setcode(tile, 0x60);
        tile->x0 = -0xA0;
        tile->y0 = -0x78;
        tile->r0 = tile->g0 = 2;
        tile->b0            = 2;
        tile->w             = 0x140;
        tile->h             = 0xF0;
        addPrim(&gGpuCurrentOt[0x3FF], tile);

        drStp          = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_STP);
        SetDrawStp(drStp, 1);
        addPrim(&gGpuCurrentOt[0x3FF], drStp);

        drOffset       = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_OFFSET);
        ofs[0]         = width + 0xA0;
        ofs[1]         = 0x178;
        SetDrawOffset(drOffset, ofs);
        addPrim(&gGpuCurrentOt[0x3FF], drOffset);

        drArea         = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_AREA);
        rect.x         = width;
        rect.y         = 0x100;
        rect.w         = 0x140;
        rect.h         = 0xF0;
        SetDrawArea(drArea, &rect);
        addPrim(&gGpuCurrentOt[0x3FF], drArea);
    }

    extra->flags = work->field_C;
    if (!(extra->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) && gGameSession->sceneUpdatesPaused == 0) {
        parts   = task->extra.tmd->coords;
        owner   = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        refPart = &parts[1];
        if (owner != NULL) {
            TmdObject* src       = owner->extra.tmd;
            GfxCoord*  srcCoords = src->coords;

            parts->composeStamp = GRAPHICS_COORD_DIRTY;
            j                   = 0;
            if (src->partCount != 0) {
                MATRIX* from = &srcCoords->coord;
                MATRIX* to   = &parts->coord;

                do {
                    *to  = *from;
                    to   = &PARENT_OF(to, GfxCoord, coord)[1].coord;
                    from = &PARENT_OF(from, GfxCoord, coord)[1].coord;
                } while (++j < src->partCount);
            }
        }
        if (stage == 1 || stage == 5) {
            extent = (RoomMirrorExtentScratch*)SCRATCH_STACK_RESERVE_BYTES(0x34);
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
            if ((GAME_LOCATION_WORD(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc) & GAME_LOCATION_AREA_VIEW_MASK) == GAME_LOCATION_KEY(0, 2, 0, 5)) {
                if (task->spawnArg1.value == 0) {
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

                mode           = gGpuPrimCursor;
                texX           = extent->left + (u16)(width + 0xA0);
                extent->texX   = texX & 0xFFC0;
                gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_TPAGE);
                setDrawTPage(mode, 0, 1, 0);
                addPrim(&gGpuCurrentOt[(((extent->otzFoot << gDisplayState.otDepthShift) & 0x3FFF) >> 4) + extra->otOffset - 15],
                        mode);
                for (layer = work->field_8; layer < 3; layer++) {
                    poly           = gGpuPrimCursor;
                    gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(POLY_FT4);
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
                mode           = gGpuPrimCursor;
                gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_TPAGE);
                setDrawTPage(mode, 0, 0, 0);
                addPrim(&gGpuCurrentOt[(((extent->otzFoot << gDisplayState.otDepthShift) & 0x3FFF) >> 4) + extra->otOffset - 15],
                        mode);
            } else {
                extra->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            SCRATCH_STACK_RELEASE_BYTES(0x34);
        }
    }

    {
        GfxCoord*  ownerParts;
        TmdObject* ownerBody;
        GfxCoord*  ownParts;
        MATRIX     mtx;

        ownerParts  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
        ownerBody   = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd;
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
static void Reflection_HeldObjectTask(Task* task)
{
    Task*           mirror;
    TmdObject*      mirrorExtra;
    RoomMirrorWork* work;
    GfxCoord*       mirrorPart;
    TmdObject*      src;
    GfxCoord*       srcParts;
    TmdObject*      extra;
    GfxCoord*       parts;
    VECTOR          scale;
    u16             flags;

    if (task->parent == NULL) {
        Task_CallExit(task);
    }
    mirror      = (Task*)task->spawnArg2.pointer;
    mirrorPart  = &mirror->extra.tmd->coords[Reflection_Data_8017FC8C[task->spawnArg1.value]];
    work        = (RoomMirrorWork*)mirror->work;
    mirrorExtra = mirror->extra.tmd;
    if (task->state == 0) {
        src      = task->parent->extra.tmd;
        srcParts = src->coords;
        if (Gp_AttachTmd(task, src->source) == NULL) {
            Task_CallExit(task);
            return;
        }
        extra                    = task->extra.tmd;
        parts                    = extra->coords;
        extra->texturePageOffset = src->texturePageOffset;
        tmdProcessStream(extra);
        tmdProcessStream(extra);
        extra->flags    = TMD_OBJECT_REVERSE_CULLING;
        extra->otOffset = 0x1F;
        parts->parent   = mirrorPart;
        extra->lightMtx = &work->light;
        extra->colorMtx = &work->color;
        if (task->spawnArg1.value >= 2) {
            scale = Reflection_Data_8017D5C4;
            ScaleMatrix(&parts->coord, &scale);
        }
        parts->coord.t[0]   = -srcParts->coord.t[0];
        parts->coord.t[1]   = srcParts->coord.t[1];
        parts->coord.t[2]   = srcParts->coord.t[2];
        parts->composeStamp = GRAPHICS_COORD_DIRTY;
        task->state++;
    }
    extra        = task->extra.tmd;
    flags        = mirrorExtra->flags;
    extra->flags = flags;
    if (task->spawnArg1.value >= 2) {
        extra->flags = flags & 0xFFEF;
    }
}

/// Runs the hall's mirror task: state 0 sets the mirror up, state 1 is its
/// per-frame update.
static inline void Reflection_PlayerTask(Task* task)
{
    TaskFunc states[2] = {
        Reflection_InitPlayer,
        Reflection_UpdatePlayer,
    };

    states[task->state](task);
}
