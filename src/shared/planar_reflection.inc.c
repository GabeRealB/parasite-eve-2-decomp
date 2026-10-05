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

#include "rooms/room_common.h"

/// Scratch block for rebuilding a planar reflection's coordinate frame.
///
/// Reserved on the scratch stack when the view changes, and released before
/// the player's light matrices are copied. A floor mirror uses only
/// `viewYRow`. Any other mirror reflects the view through a plane.
/// `normal` and `planePoint` describe that plane, `refAxis` is the world axis
/// least aligned with the normal, `basis` is the orthonormal frame built from
/// the two, and `reflect` is the reflection matrix copied into the mirror's
/// coordinate frame. That frame's translation is the view translation plus
/// the shift that makes `reflect` fix `planePoint`.
typedef struct {
    SVECTOR viewYRow;      // View matrix Y row, copied and then negated in place (4.12)
    SVECTOR refAxis;       // Unit world axis least aligned with `normal` (4.12; 0x1000 = 1)
    byte    unknown_10[8]; // No recovered access; role unproven
    MATRIX  basis;         // Orthonormal frame whose third column is the plane normal
    MATRIX  reflect;       // Reflection through that plane; only the rotation is used
    SVECTOR normal;        // Plane normal, normalized in place to 4.12
    SVECTOR planePoint;    // Point on the plane, in view-translation units; then rotated by `reflect`
    s16     leastAbs;      // Smallest absolute component of `normal` so far (4.12)
    s16     leastAxis;     // Which component `leastAbs` came from (0 X, 1 Y, 2 Z)
    s16     axisAbs;       // Absolute value of the component being compared (4.12)
    byte    unknown_6E[2]; // No recovered access; role unproven
} _PlanarReflectionFrameScratch;
STATIC_ASSERT_SIZEOF(_PlanarReflectionFrameScratch, 0x70);

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
    tmdBuildBufferHalf(extra);
    tmdBuildBufferHalf(extra);
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
    work->viewRebuildStamp = gGfxViewCoord.composeStamp & GRAPHICS_COORD_STAMP_MASK;
    work->copyPending      = 1;
    work->equippedWeapon   = -1;
    extra->flags          |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->copyPending      = 0;
    work->viewRebuildStamp = -1;
    actor                  = (GameActor*)owner->work;
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

/// Scratch-stack block for where a planar reflection lands on screen.
///
/// The mirror task in Acropolis and in the Shelter/Neo Ark stage reserves one
/// block, projects two samples through one reflected part, and releases the
/// block before copying the player's light matrices. While a scripted event
/// is running the samples are on part 1, 1000 game units either side of its
/// origin. Otherwise they are on the root part: 2000 units along negative
/// local Y, and the origin. Model space is Y-down, so the negative-Y sample
/// is the upper end of an upright part; it is stored as the head and the
/// other sample as the foot.
///
/// `point` is reused for both projections. Each projection overwrites
/// `depthCue` and `projectionFlags`, and the mirror never reads them. Screen
/// Y is then ordered so `screenFoot` is the upper edge and `screenHead` the
/// lower, and each edge is padded by 16 pixels. The swap uses `screenHead.vx`
/// as its temporary. The quad is centred on the foot sample's screen X. Its
/// half-width is half the padded vertical span, clamped to 95 pixels, and
/// `left` through `bottom` is that rectangle clamped to the 320 by 240 frame.
/// The quads are drawn when the rectangle overlaps the mirror's clip
/// rectangle; a shared edge does not count. Otherwise the reflected model is
/// hidden for the frame.
///
/// `orderingDepthFoot` is the quarter-depth the quads are ordered at. In area
/// 2, view 5, a mirror other than index 0 stores the head sample's depth plus
/// 10 there instead. `texturePageX` is the 64-pixel-aligned VRAM X of the
/// 16-bit page those quads sample the off-screen frame copy from.
typedef struct {
    SVECTOR point;             // Sample in the part's local space; X and Z are 0, and the unused halfword is left unchanged
    s32     depthCue;          // GTE IR0 of the latest projection, with 12 fractional bits; written and never read
    s32     projectionFlags;   // GTE FLAG of the latest projection; written and never tested
    s32     orderingDepthFoot; // Foot sample's SZ3 / 4, or the head depth plus 10; the quads' ordering-table depth
    s32     orderingDepthHead; // Head sample's SZ3 / 4; read only to replace the foot depth
    DVECTOR screenFoot;        // Foot sample's screen pixels; vy is ordered to the upper edge, vx centres the quad
    DVECTOR screenHead;        // Head sample's screen pixels; vy is ordered to the lower edge, vx is the swap temporary
    u16     texturePageX;      // 64-pixel-aligned VRAM X of the 16-bit page the quads sample. Two alignment bytes follow
    s32     left;              // Quad left edge, pixels from screen centre, clamped to -160
    s32     right;             // Quad right edge, pixels from screen centre, clamped to 160
    s32     top;               // Quad top edge, pixels from screen centre, clamped to -120
    s32     bottom;            // Quad bottom edge, pixels from screen centre, clamped to 120
} _PlanarReflectionExtentScratch;
STATIC_ASSERT_SIZEOF(_PlanarReflectionExtentScratch, 0x34);

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
    RoomMirrorWork*                 work;
    PlayerStatus*                   status;
    TmdObject*                      extra;
    TmdObject*                      model;
    Task*                           owner;
    GameActor*                      actor;
    Task*                           child;
    Task*                           spawned;
    _PlanarReflectionFrameScratch*  frame;
    _PlanarReflectionExtentScratch* extent;
    GfxCoord*                       parts;
    GfxCoord*                       refPart;
    DR_AREA*                        drArea;
    DR_STP*                         drStp;
    DR_OFFSET*                      drOffset;
    SPRT*                           sprt;
    DR_TPAGE*                       tpage;
    TILE*                           tile;
    POLY_FT4*                       poly;
    s32                             stage;
    s32                             area;
    s32                             view;
    s32                             width;
    s32                             viewRebuildStamp;
    s32                             copyPending;
    s32                             halfWidth;
    s32                             texX;
    s32                             i;
    s32                             layer;
    u32                             j;

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
    if (work->equippedWeapon != status->weapon) {
        actor                = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
        work->equippedWeapon = status->weapon;
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
    extra->flags    |= TMD_OBJECT_REVERSE_CULLING;
    viewRebuildStamp = gGfxViewCoord.composeStamp & GRAPHICS_COORD_STAMP_MASK;
    if (work->viewRebuildStamp != viewRebuildStamp) {
        GfxCoord* viewParent;

        work->viewRebuildStamp   = viewRebuildStamp;
        viewParent               = gGfxViewCoord.parent;
        work->clipLeft           = -0xA0;
        work->clipRight          = 0xA0;
        work->coord.composeStamp = GRAPHICS_COORD_DIRTY;
        work->clipTop            = -0x78;
        work->clipBottom         = 0x78;
        frame                    = SCRATCH_STACK_RESERVE_BLOCK(_PlanarReflectionFrameScratch);
        work->coord.parent       = viewParent;
        if (task->spawnArg1.value == 0) {
            work->copyPending  = 1;
            work->coord.coord  = gGfxViewCoord.coord;
            frame->viewYRow.vx = work->coord.coord.m[1][0];
            frame->viewYRow.vy = work->coord.coord.m[1][1];
            frame->viewYRow.vz = work->coord.coord.m[1][2];
            gte_lddp(-0x1000);
            gte_ldsv(&frame->viewYRow);
            gte_gpf12();
            gte_stsv(&frame->viewYRow);
            work->coord.coord.m[1][0] = frame->viewYRow.vx;
            work->coord.coord.m[1][1] = frame->viewYRow.vy;
            work->coord.coord.m[1][2] = frame->viewYRow.vz;
            if (stage == 5) {
                if (area == 7) {
                    if (view >= 6 && view < 12 && gGameSession->location.loc.room == 2) {
                        work->coord.coord.t[1] += 0x9B;
                        extra->flags           &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
                        work->firstBlendMode    = GPU_BLEND_AVERAGE;
                    } else {
                        work->copyPending = 0;
                        extra->flags     |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    }
                }
            } else if (area == 1) {
                extra->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                if (view == 9) {
                    work->copyPending = 0;
                }
            } else {
                if (area != 0x11) {
                    work->coord.coord.t[1] += 0x69;
                }
                work->firstBlendMode = GPU_BLEND_ADD;
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
                                frame->normal.vx = -0x1000;
                                frame->normal.vy = 0;
                                frame->normal.vz = 0;
                                VectorNormalSS(&frame->normal, &frame->normal);
                                frame->planePoint.vx = -0x1518;
                                frame->planePoint.vy = 0;
                                frame->planePoint.vz = 0;
                                break;
                            case 3:
                                frame->normal.vx = 0x64;
                                frame->normal.vy = 0;
                                frame->normal.vz = -0x384;
                                VectorNormalSS(&frame->normal, &frame->normal);
                                frame->planePoint.vx = 0;
                                frame->planePoint.vy = 0;
                                frame->planePoint.vz = -0x640;
                                break;
                            case 4:
                                work->clipLeft   = -0x14;
                                work->clipRight  = 0x14;
                                frame->normal.vx = -0x1000;
                                frame->normal.vy = 0;
                                frame->normal.vz = 0;
                                VectorNormalSS(&frame->normal, &frame->normal);
                                frame->planePoint.vx = 0x1644;
                                frame->planePoint.vy = 0;
                                frame->planePoint.vz = 0;
                                break;
                            default:
                                model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                                break;
                        }
                        break;
                    case 1:
                        switch (view) {
                            case 6:
                                work->clipRight  = 0x64;
                                work->clipLeft   = 0;
                                frame->normal.vx = -0x1000;
                                frame->normal.vy = 0;
                                frame->normal.vz = 0;
                                VectorNormalSS(&frame->normal, &frame->normal);
                                frame->planePoint.vx = 0x1AF4;
                                frame->planePoint.vy = 0;
                                frame->planePoint.vz = 0;
                                model->otOffset      = 0x1F;
                                break;
                            case 7:
                            case 8:
                                frame->normal.vx = 0;
                                frame->normal.vy = 0;
                                frame->normal.vz = 0x1000;
                                VectorNormalSS(&frame->normal, &frame->normal);
                                frame->planePoint.vx = 0;
                                frame->planePoint.vy = 0;
                                frame->planePoint.vz = 0x14B4;
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
                                frame->normal.vx = -0x1000;
                                frame->normal.vy = 0;
                                frame->normal.vz = 0;
                                VectorNormalSS(&frame->normal, &frame->normal);
                                frame->planePoint.vx = 0x170C;
                                frame->planePoint.vy = 0;
                                frame->planePoint.vz = 0;
                                break;
                            case 4:
                                frame->normal.vx = -0x1000;
                                frame->normal.vy = 0;
                                frame->normal.vz = 0;
                                VectorNormalSS(&frame->normal, &frame->normal);
                                frame->planePoint.vx = -0x1644;
                                frame->planePoint.vy = 0;
                                frame->planePoint.vz = 0;
                                break;
                            case 3:
                                frame->normal.vx = -0x64;
                                frame->normal.vy = 0;
                                frame->normal.vz = -0x384;
                                VectorNormalSS(&frame->normal, &frame->normal);
                                frame->planePoint.vx = 0;
                                frame->planePoint.vy = 0;
                                frame->planePoint.vz = -0x640;
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
                frame->normal.vx = -0x1000;
                frame->normal.vy = 0;
                frame->normal.vz = 0;
                VectorNormalSS(&frame->normal, &frame->normal);
                frame->planePoint.vx = 0xA38;
                frame->planePoint.vy = 0;
                frame->planePoint.vz = 0;
            } else {
                model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            if (!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
                // The axis least aligned with the normal completes the frame. Translation is the
                // view translation plus the shift that makes the reflection fix planePoint.
                frame->leastAbs = frame->normal.vx;
                if (frame->leastAbs < 0) {
                    frame->leastAbs = -frame->leastAbs;
                }
                frame->leastAxis = 0;
                frame->axisAbs   = frame->normal.vy;
                if (frame->axisAbs < 0) {
                    frame->axisAbs = -frame->axisAbs;
                }
                if (frame->leastAbs > frame->axisAbs) {
                    frame->leastAbs  = frame->axisAbs;
                    frame->leastAxis = 1;
                }
                frame->axisAbs = frame->normal.vz;
                if (frame->axisAbs < 0) {
                    frame->axisAbs = -frame->axisAbs;
                }
                if (frame->leastAbs > frame->axisAbs) {
                    frame->leastAbs  = frame->axisAbs;
                    frame->leastAxis = 2;
                }
                frame->refAxis.vx = 0;
                if (frame->leastAxis == 0) {
                    frame->refAxis.vx = 0x1000;
                }
                frame->refAxis.vy = 0;
                if (frame->leastAxis == 1) {
                    frame->refAxis.vy = 0x1000;
                }
                frame->refAxis.vz = 0;
                if (frame->leastAxis == 2) {
                    frame->refAxis.vz = 0x1000;
                }
                Gfx_OrthonormalBasis(&frame->basis, &frame->normal, &frame->refAxis);
                gte_TransposeMatrix(&frame->basis, &frame->reflect);
                frame->reflect.m[2][0] = -frame->reflect.m[2][0];
                frame->reflect.m[2][1] = -frame->reflect.m[2][1];
                frame->reflect.m[2][2] = -frame->reflect.m[2][2];
                gte_MulMatrix0(&frame->basis, &frame->reflect, &frame->reflect);
                work->coord.coord      = frame->reflect;
                work->coord.coord.t[0] = gGfxViewCoord.coord.t[0] + frame->planePoint.vx;
                work->coord.coord.t[1] = gGfxViewCoord.coord.t[1] + frame->planePoint.vy;
                work->coord.coord.t[2] = gGfxViewCoord.coord.t[2] + frame->planePoint.vz;
                _gfxRotateSv(&frame->reflect, &frame->planePoint);
                work->coord.coord.t[0] -= frame->planePoint.vx;
                work->coord.coord.t[1] -= frame->planePoint.vy;
                work->coord.coord.t[2] -= frame->planePoint.vz;
                work->firstBlendMode    = GPU_BLEND_ADD;
            }
        }
        work->objectFlags = extra->flags;
        SCRATCH_STACK_RELEASE_BLOCK(_PlanarReflectionFrameScratch);
    }

    copyPending = work->copyPending;
    if (copyPending == 1 && task->spawnArg1.value == 0 && !(area == 1 && view == 0xF) && gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
        u16  ofs[2];
        RECT rect;

        work->copyPending = 0;
        drArea            = gGpuPrimCursor;
        gGpuPrimCursor    = (u8*)gGpuPrimCursor + sizeof(DR_AREA);
        rect.x            = 0;
        rect.y            = gDisplayState.drawBuffer * 0x110;
        rect.w            = 0x140;
        rect.h            = 0xF0;
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

    extra->flags = work->objectFlags;
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
            enum {
                /// Local-Y distance from part 1 to each sample during a scripted event, in game coordinates.
                PLANAR_REFLECTION_EVENT_SAMPLE_DISTANCE = 0x3E8,
                /// Local Y of the root part's head sample, in game coordinates. The foot sample is the origin.
                PLANAR_REFLECTION_ROOT_HEAD_OFFSET = -0x7D0,
                /// Pixels added beyond each ordered screen edge before the quad is measured.
                PLANAR_REFLECTION_SCREEN_EDGE_PAD = 0x10,
                /// Quarter-depths added to the head sample when it replaces the foot's ordering depth.
                PLANAR_REFLECTION_ORDERING_DEPTH_BIAS = 0xA,
                /// Clears a VRAM X down to the 64-pixel alignment a 16-bit texture page requires.
                PLANAR_REFLECTION_TEXTURE_PAGE_MASK = 0xFFC0
            };

            extent = SCRATCH_STACK_RESERVE_BLOCK(_PlanarReflectionExtentScratch);
            if (gGameSession->eventState != 0) {
                actorRenderComposeCoord(refPart);
                gte_SetTransMatrix(&refPart->workm);
                gte_SetRotMatrix(&refPart->workm);
                extent->point.vx = 0;
                extent->point.vy = -PLANAR_REFLECTION_EVENT_SAMPLE_DISTANCE;
                extent->point.vz = 0;
                gte_RotTransPers(&extent->point, &extent->screenHead, &extent->depthCue, &extent->projectionFlags,
                                 &extent->orderingDepthHead);
                extent->point.vx = 0;
                extent->point.vy = PLANAR_REFLECTION_EVENT_SAMPLE_DISTANCE;
                extent->point.vz = 0;
                gte_RotTransPers(&extent->point, &extent->screenFoot, &extent->depthCue, &extent->projectionFlags,
                                 &extent->orderingDepthFoot);
            } else {
                actorRenderComposeCoord(parts);
                gte_SetTransMatrix(&parts->workm);
                gte_SetRotMatrix(&parts->workm);
                extent->point.vx = 0;
                extent->point.vy = PLANAR_REFLECTION_ROOT_HEAD_OFFSET;
                extent->point.vz = 0;
                gte_RotTransPers(&extent->point, &extent->screenHead, &extent->depthCue, &extent->projectionFlags,
                                 &extent->orderingDepthHead);
                extent->point.vx = 0;
                extent->point.vy = 0;
                extent->point.vz = 0;
                gte_RotTransPers(&extent->point, &extent->screenFoot, &extent->depthCue, &extent->projectionFlags,
                                 &extent->orderingDepthFoot);
            }
            // Order screen Y so the foot slot is the upper edge. screenHead.vx is the swap temporary.
            if (extent->screenFoot.vy > extent->screenHead.vy) {
                extent->screenHead.vx = extent->screenFoot.vy;
                extent->screenFoot.vy = extent->screenHead.vy;
                extent->screenHead.vy = extent->screenHead.vx;
            }
            extent->screenFoot.vy -= PLANAR_REFLECTION_SCREEN_EDGE_PAD;
            extent->screenHead.vy += PLANAR_REFLECTION_SCREEN_EDGE_PAD;
            // Half the padded vertical span, clamped to 95, is the quad's half-width about the foot's screen X.
            halfWidth = (extent->screenHead.vy - extent->screenFoot.vy) >> 1;
            if (halfWidth >= 0x60) {
                halfWidth = 0x5F;
            }
            // Area 2, view 5: mirror 0 forces that maximum; any other mirror orders from the head sample.
            if ((GAME_LOCATION_WORD(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc) & GAME_LOCATION_AREA_VIEW_MASK) == GAME_LOCATION_KEY(0, 2, 0, 5)) {
                if (task->spawnArg1.value == 0) {
                    halfWidth = 0x5F;
                } else {
                    extent->orderingDepthFoot = extent->orderingDepthHead + PLANAR_REFLECTION_ORDERING_DEPTH_BIAS;
                }
            }
            extent->left = extent->screenFoot.vx - halfWidth;
            if (extent->left < -0xA0) {
                extent->left = -0xA0;
            }
            extent->right = extent->screenFoot.vx + halfWidth;
            if (extent->right > 0xA0) {
                extent->right = 0xA0;
            }
            extent->top = extent->screenFoot.vy;
            if (extent->top < -0x78) {
                extent->top = -0x78;
            }
            extent->bottom = extent->screenHead.vy;
            if (extent->bottom > 0x78) {
                extent->bottom = 0x78;
            }
            if (extent->top < work->clipBottom && work->clipTop < extent->bottom && extent->left < work->clipRight &&
                work->clipLeft < extent->right) {
                DR_TPAGE* mode;

                mode                 = gGpuPrimCursor;
                texX                 = extent->left + (u16)(width + 0xA0);
                extent->texturePageX = texX & PLANAR_REFLECTION_TEXTURE_PAGE_MASK;
                gGpuPrimCursor       = (u8*)gGpuPrimCursor + sizeof(DR_TPAGE);
                setDrawTPage(mode, 0, 1, 0);
                addPrim(&gGpuCurrentOt[(((extent->orderingDepthFoot << gDisplayState.otDepthShift) & 0x3FFF) >> 4) + extra->otOffset - 15],
                        mode);
                for (layer = work->firstBlendMode; layer < GPU_BLEND_ADD_QUARTER; layer++) {
                    poly           = gGpuPrimCursor;
                    gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(POLY_FT4);
                    setPolyFT4(poly);
                    setSemiTrans(poly, 1);
                    if (work->firstBlendMode == GPU_BLEND_ADD) {
                        setShadeTex(poly, 0);
                        poly->r0 = poly->g0 = poly->b0 = 0x80;
                    } else {
                        setShadeTex(poly, 1);
                    }
                    poly->x0 = poly->x2 = extent->left;
                    poly->x1 = poly->x3 = extent->right;
                    poly->y0 = poly->y1 = extent->top;
                    poly->y2 = poly->y3 = extent->bottom;
                    poly->tpage         = getTPage(2, layer, extent->texturePageX, 0x100);
                    poly->u0 = poly->u2 = poly->x0 + 0xA0 + width - extent->texturePageX;
                    poly->u1 = poly->u3 = poly->x1 + 0xA0 + width - extent->texturePageX;
                    poly->v0 = poly->v1 = poly->y0 + 0x78;
                    poly->v2 = poly->v3 = poly->y2 + 0x78;
                    addPrim(&gGpuCurrentOt[(((extent->orderingDepthFoot << gDisplayState.otDepthShift) & 0x3FFF) >> 4) + extra->otOffset - 15],
                            poly);
                }
                mode           = gGpuPrimCursor;
                gGpuPrimCursor = (u8*)gGpuPrimCursor + sizeof(DR_TPAGE);
                setDrawTPage(mode, 0, 0, 0);
                addPrim(&gGpuCurrentOt[(((extent->orderingDepthFoot << gDisplayState.otDepthShift) & 0x3FFF) >> 4) + extra->otOffset - 15],
                        mode);
            } else {
                extra->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            SCRATCH_STACK_RELEASE_BLOCK(_PlanarReflectionExtentScratch);
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
        actorRenderComposeCoord(ownParts);
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
        tmdBuildBufferHalf(extra);
        tmdBuildBufferHalf(extra);
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
