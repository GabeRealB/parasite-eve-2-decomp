#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/gtemac.h>

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/3688.h"
#include "gameplay/3A34.h"
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

/// Scratchpad block the mirror takes while it rebuilds its coordinate frame.
/// `viewRow` is the view matrix's second row, negated through the GTE for
/// mirror 0. The other mirror reflects through a plane: `normal` is the
/// plane's unit normal, `refAxis` the coordinate axis least aligned with it
/// (picked through `leastAbs` / `leastAxis` / `axisAbs`), `basis` the
/// orthonormal frame built from the two and `reflect` the reflection matrix
/// derived from it. `offset` is the plane's position relative to the view,
/// rotated in place into the reflected frame.
typedef struct {
    SVECTOR viewRow;
    SVECTOR refAxis;
    byte    unknown_10[8];
    MATRIX  basis;
    MATRIX  reflect;
    SVECTOR normal;
    SVECTOR offset;
    s16     leastAbs;
    s16     leastAxis;
    s16     axisAbs;
    byte    unknown_6E[2];
} _MirrorPlaneScratch;

/// Scratchpad block the mirror takes to find where the reflection lands on
/// screen. Two points above and below one of the reflected model's parts are
/// projected through `pos`: `sxyHead` / `otzHead` for the upper point,
/// `sxyFoot` / `otzFoot` for the lower. `left` .. `bottom` is the screen
/// rectangle the reflection quads cover and `texX` the x of the texture page
/// they sample the off-screen copy of the frame from.
typedef struct {
    SVECTOR pos;
    s32     dp;
    s32     flag;
    s32     otzFoot;
    s32     otzHead;
    DVECTOR sxyFoot;
    DVECTOR sxyHead;
    u16     texX;
    s32     left;
    s32     right;
    s32     top;
    s32     bottom;
} _MirrorExtentScratch;

/// Scratch state of an elevator-car task, stored at `Task::work`.
/// `func_acropolis_west_elevator_hall_8017F64C` allocates it with
/// `memCalloc(4, 0)`, so the size below is the allocation and not a guess.
typedef struct {
    /* 0x0 */ s32 field_0;
} AwehElevatorState;

extern s32 D_80070F70;
extern s8  D_8007272D;

/// The save's location key, read as one word to test its view and area
/// together.
extern s32 D_8007216C;

extern TaskDesc   D_acropolis_west_elevator_hall_80184568[];
extern s32        D_acropolis_west_elevator_hall_80184620;
extern s32        D_acropolis_west_elevator_hall_80184890;
extern s32        D_acropolis_west_elevator_hall_801849C8;
extern GpMsgEntry D_acropolis_west_elevator_hall_801849CC[];
extern GpMsgEntry D_acropolis_west_elevator_hall_801849F4[];

/// Index of the mirror model's coordinate part each held-object reflection is
/// parented to, by the reflection's `spawnArg1`.
extern u8 D_acropolis_west_elevator_hall_801802A4[];

/// The mirror's task table: entry 0 runs the mirror itself, entry 1 a
/// held-object reflection.
extern TaskDesc D_acropolis_west_elevator_hall_801802A8[];

/// The lift bay's two 256-entry RGB555 CLUTs and the blend destination:
/// `...80184A04` is the unlit base palette, `...80184C04` the lit one and
/// `...80184E04` the blended result that `...80185004` uploads to VRAM.
extern u16      D_acropolis_west_elevator_hall_80184A04[];
extern u16      D_acropolis_west_elevator_hall_80184C04[];
extern u16      D_acropolis_west_elevator_hall_80184E04[];
extern GpImgRec D_acropolis_west_elevator_hall_80185004[];

/// The hall's two elevator-car tasks, spawned by the room task.
extern Task* D_acropolis_west_elevator_hall_80186AE4[];

void func_acropolis_west_elevator_hall_8017D7B0(Task* task);
void func_acropolis_west_elevator_hall_8017F354(Task* task);
void func_acropolis_west_elevator_hall_8017F568(Task* arg0);
void func_acropolis_west_elevator_hall_8017F64C(Task* task);
void func_acropolis_west_elevator_hall_8017F6F0(Task* task);

/// Scale applied to held-object reflections in slots 2 and up: it mirrors
/// them across X.
const VECTOR D_acropolis_west_elevator_hall_8017D5C4 = { -0x1000, 0x1000, 0x1000, 0 };

/// State handlers of the room task: set-up, the cutscene hand-off and
/// `taskKill`.
const TaskFuncTable3 D_acropolis_west_elevator_hall_8017D5D4 = {
    { func_acropolis_west_elevator_hall_8017F568, func_acropolis_west_elevator_hall_8017F354, taskKill },
};

/// State handlers of an elevator-car task: set-up, travel and `taskKill`.
const TaskFuncTable3 D_acropolis_west_elevator_hall_8017D5E0 = {
    { func_acropolis_west_elevator_hall_8017F64C, func_acropolis_west_elevator_hall_8017F6F0, taskKill },
};

/// Position of the first effect `func_acropolis_west_elevator_hall_8017F7D4`
/// spawns in view 2.
const SVECTOR D_acropolis_west_elevator_hall_8017D5EC = { -0x1518, -0x720, 0xAC, 0 };

/// Position of the effect `func_acropolis_west_elevator_hall_8017F7D4` spawns
/// in view 5.
const SVECTOR D_acropolis_west_elevator_hall_8017D5F4 = { -0x79, -0x876, 0x703, 0 };

/// First state of the hall's mirror task: re-attaches the player's own TMD
/// source to this task so the reflection draws the player's model, allocates
/// the `RoomMirrorWork` block the reflection's coordinate frame and matrices
/// live in, and reparents the task under the player task. `spawnArg1` must be 0
/// or 1, and 0 also raises `GameSession::field_4E`. For each held-object task
/// the player has (`GameActor::field_920` / `field_924`) it spawns a reflection
/// from entry 1 of the mirror's task table and reparents it under that task,
/// then runs the mirror's per-frame update once.
void func_acropolis_west_elevator_hall_8017D5FC(Task* task)
{
    Task*           owner;
    GameActor*      actor;
    TmdObject*      extra;
    GsCOORDINATE2*  parts;
    RoomMirrorWork* work;
    Task*           child;
    Task*           spawned;
    s32             i;

    owner = gameGetPtrSlot(3);
    if (Gp_AttachTmd(task, ((TmdObject*)owner->extra)->source) == NULL) {
        taskKill(task);
        return;
    }
    extra = task->extra;
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
            spawned = Task_SpawnFromTable(D_acropolis_west_elevator_hall_801802A8, 1, i, (s32)task);
            if (spawned != NULL) {
                Task_Reparent(child, spawned);
            }
        }
    }
    func_acropolis_west_elevator_hall_8017D7B0(task);
}

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

/// Second state of the mirror task, run every frame.
///
/// When the player's weapon changes it spawns a reflection (entry 1 of the
/// mirror's task table, slots 2 and 3) for each of the player's tasks at
/// `GameActor::field_918` / `field_91C`. When the view moves it rebuilds the
/// reflection's coordinate frame: mirror 0 copies the view matrix with its
/// second row negated plus location-specific corrections, the other mirror
/// reflects through a plane chosen by the current stage, area and view. On the
/// frame after mirror 0 rebuilds, it queues packets that copy the frame buffer
/// into an off-screen strip at x = `width`. In stages 1 and 5 it projects the
/// reflected body to find its screen rectangle and, where that overlaps the
/// mirror's clip rectangle, draws quads sampling that strip; otherwise the
/// reflection is hidden. Every frame it copies the player's pose and light
/// matrices onto the reflection.
void func_acropolis_west_elevator_hall_8017D7B0(Task* task)
{
    RoomMirrorWork*       work;
    PlayerStatus*         status;
    TmdObject*            extra;
    TmdObject*            model;
    Task*                 owner;
    GameActor*            actor;
    Task*                 child;
    Task*                 spawned;
    _MirrorPlaneScratch*  plane;
    _MirrorExtentScratch* extent;
    GsCOORDINATE2*        parts;
    GsCOORDINATE2*        refPart;
    DR_AREA*              drArea;
    DR_STP*               drStp;
    DR_OFFSET*            drOffset;
    SPRT*                 sprt;
    DR_TPAGE*             tpage;
    TILE*                 tile;
    POLY_FT4*             poly;
    s32                   stage;
    s32                   area;
    s32                   view;
    s32                   width;
    s32                   viewFlg;
    s32                   copyPending;
    s32                   halfWidth;
    s32                   texX;
    s32                   i;
    s32                   layer;
    u32                   j;

    width  = 0x1C0;
    work   = task->work;
    extra  = task->extra;
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
                spawned = Task_SpawnFromTable(D_acropolis_west_elevator_hall_801802A8, 1, i + 2, (s32)task);
                if (spawned != NULL) {
                    Task_Reparent(child, spawned);
                }
            }
        }
    }
    extra->flags |= 0x10;
    viewFlg       = gGfxViewCoord.flg & 0x7FFFFFFF;
    if (work->viewFlg != viewFlg) {
        GsCOORDINATE2* sub;

        work->viewFlg     = viewFlg;
        sub               = gGfxViewCoord.sub;
        work->field_A0[0] = -0xA0;
        work->field_A0[1] = 0xA0;
        work->coord.flg   = 0;
        work->field_A0[2] = -0x78;
        work->field_A0[3] = 0x78;
        plane             = (_MirrorPlaneScratch*)(*(u8**)G_SCRATCH_HEAD -= 0x70);
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
            model         = task->extra;
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
        work->field_C          = extra->flags;
        *(u8**)G_SCRATCH_HEAD += 0x70;
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
        parts   = ((TmdObject*)task->extra)->coords;
        owner   = gameGetPtrSlot(3);
        refPart = &parts[1];
        if (owner != NULL) {
            TmdObject*     src       = owner->extra;
            GsCOORDINATE2* srcCoords = src->coords;

            parts->flg = 0;
            j          = 0;
            if (src->partCount != 0) {
                GsCOORDINATE2* from = (GsCOORDINATE2*)&srcCoords->coord;
                GsCOORDINATE2* to   = (GsCOORDINATE2*)&parts->coord;

                do {
                    *(MATRIX*)to = *(MATRIX*)from;
                    to++;
                    from++;
                } while (++j < src->partCount);
            }
        }
        if (stage == 1 || stage == 5) {
            extent = (_MirrorExtentScratch*)(*(u8**)G_SCRATCH_HEAD -= 0x34);
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
            if ((D_8007216C & 0xFF00FF) == 0x20005) {
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
            *(u8**)G_SCRATCH_HEAD += 0x34;
        }
    }

    {
        GsCOORDINATE2* ownerParts;
        TmdObject*     ownerBody;
        GsCOORDINATE2* ownParts;
        MATRIX         mtx;

        ownerParts  = ((TmdObject*)gameGetPtrSlot(3)->extra)->coords;
        ownerBody   = gameGetPtrSlot(3)->extra;
        ownParts    = ((TmdObject*)task->extra)->coords;
        work->light = *ownerBody->lightMtx;
        work->color = *ownerBody->colorMtx;
        Gp_UpdateCoord(ownParts);
        gte_TransposeMatrix(&ownParts->workm, &mtx);
        gte_MulMatrix0(&ownerParts->workm, &mtx, &mtx);
        gte_MulMatrix0(&work->light, &mtx, &work->light);
    }
}

/// Per-frame callback of a held-object reflection. `spawnArg2` is the mirror
/// task that spawned it and the parent is the held-object task it reflects;
/// with no parent it exits. On the first frame it clones the parent's TMD
/// source, hangs the clone's root coordinate off the mirror model's part that
/// `D_acropolis_west_elevator_hall_801802A4` names for its slot, points the
/// clone at the mirror's light and color matrices and negates the X
/// translation, scaling slots 2 and up by the rodata vector; every frame it
/// copies the mirror model's draw flags onto the clone.
void func_acropolis_west_elevator_hall_8017F134(Task* task)
{
    Task*           mirror;
    TmdObject*      mirrorExtra;
    RoomMirrorWork* work;
    GsCOORDINATE2*  mirrorPart;
    TmdObject*      src;
    GsCOORDINATE2*  srcParts;
    TmdObject*      extra;
    GsCOORDINATE2*  parts;
    VECTOR          scale;
    u16             flags;

    if (task->parent == NULL) {
        Task_CallExit(task);
    }
    mirror      = (Task*)task->spawnArg2;
    mirrorPart  = &((TmdObject*)mirror->extra)->coords[D_acropolis_west_elevator_hall_801802A4[task->spawnArg1]];
    work        = (RoomMirrorWork*)mirror->work;
    mirrorExtra = mirror->extra;
    if (task->state == 0) {
        src      = task->parent->extra;
        srcParts = src->coords;
        if (Gp_AttachTmd(task, src->source) == NULL) {
            Task_CallExit(task);
            return;
        }
        extra        = task->extra;
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
            scale = D_acropolis_west_elevator_hall_8017D5C4;
            ScaleMatrix(&parts->coord, &scale);
        }
        parts->coord.t[0] = -srcParts->coord.t[0];
        parts->coord.t[1] = srcParts->coord.t[1];
        parts->coord.t[2] = srcParts->coord.t[2];
        parts->flg        = 0;
        task->state++;
    }
    extra        = task->extra;
    flags        = mirrorExtra->flags;
    extra->flags = flags;
    if (task->spawnArg1 >= 2) {
        extra->flags = flags & 0xFFEF;
    }
}

/// The mirror task's per-frame entry: state 0 sets the mirror up, state 1
/// runs its update.
void func_acropolis_west_elevator_hall_8017F304(Task* task)
{
    TaskFunc states[2] = {
        func_acropolis_west_elevator_hall_8017D5FC,
        func_acropolis_west_elevator_hall_8017D7B0,
    };

    states[task->state](task);
}

/// Runs the one-shot cutscene hand-off for the west elevator hall: once the
/// session reports state 8 == 1 the room spawns its scripted task pair, opens
/// the story flags for the elevator and marks the sequence as running; the
/// second block retires it again when the session goes idle.
///
/// `args` and the scratch block above it are dead here - the dispatch that
/// consumed them is gone - but the compiler still reserves and fills them, so
/// they have to stay for the frame layout to match.
void func_acropolis_west_elevator_hall_8017F354(Task* task)
{
    s32 args[2] = { 0, 4 };
    u8  scratch[0x210];
    u8  sessionState;

    if (D_acropolis_west_elevator_hall_801849C8 == 0) {
        sessionState = gGameSession->at4.loc.warp;
        if (sessionState == 1) {
            D_acropolis_west_elevator_hall_801849C8 = sessionState;
            func_800E8634((s32)&D_acropolis_west_elevator_hall_80184620, 0, (s32)&D_acropolis_west_elevator_hall_80184890);
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 1);
            GameFlag_SetNibble(0x7A, 1);
            func_800E3FAC(0xA2, 1);
        }
    }
    if (D_acropolis_west_elevator_hall_801849C8 == 1 && gGameSession->eventState == 0) {
        D_acropolis_west_elevator_hall_801849C8 = 2;
    }
}

/// Per-frame entry of an elevator-car task: runs the state its `state` field
/// selects from `D_acropolis_west_elevator_hall_8017D5E0` (set-up, travel,
/// then kill).
void func_acropolis_west_elevator_hall_8017F418(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_west_elevator_hall_8017D5E0;
    sp.funcs[task->state](task);
}

/// Sets both of the hall's elevator-car tasks moving forwards, by storing 1 in
/// each task's `spawnArg1` (the per-frame step direction the car task reads).
s32 func_acropolis_west_elevator_hall_8017F470(void)
{
    D_acropolis_west_elevator_hall_80186AE4[0]->spawnArg1 = 1;
    D_acropolis_west_elevator_hall_80186AE4[1]->spawnArg1 = 1;
    return 0;
}

/// Sets both elevator-car tasks moving backwards, by storing -1 in each task's
/// `spawnArg1`.
s32 func_acropolis_west_elevator_hall_8017F498(void)
{
    D_acropolis_west_elevator_hall_80186AE4[0]->spawnArg1 = -1;
    D_acropolis_west_elevator_hall_80186AE4[1]->spawnArg1 = -1;
    return 0;
}

s32 func_acropolis_west_elevator_hall_8017F4C0(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    if (*(u16*)src == 1 && GameFlag_GetNibble(0x21) == 0 && src->field_5 == 0) {
        GameFlag_SetNibble(0x21, 1);
        D_8007272D   = 1;
        dst->field_2 = 7;
    }
    return 1;
}

s32 func_acropolis_west_elevator_hall_8017F560(void)
{
    return 0;
}

void func_acropolis_west_elevator_hall_8017F568(Task* arg0)
{
    arg0->msgTable = D_acropolis_west_elevator_hall_801849CC;
    Game_SetPtrSlot(arg0, 7);
    D_acropolis_west_elevator_hall_80186AE4[0] =
        Task_SpawnFromTable(D_acropolis_west_elevator_hall_80184568, 0, 0, -1);
    D_acropolis_west_elevator_hall_80186AE4[1] =
        Task_SpawnFromTable(D_acropolis_west_elevator_hall_80184568, 1, 0, 1);
    arg0->state = (s32)(arg0->state + 1);
}

/// Per-frame entry of the room task: runs the state its `state` field selects
/// from `D_acropolis_west_elevator_hall_8017D5D4` (set-up, the cutscene
/// hand-off, then kill).
void func_acropolis_west_elevator_hall_8017F5F4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_west_elevator_hall_8017D5D4;
    sp.funcs[task->state](task);
}

/// Second state of the elevator task: allocates its scratch block, parks the
/// car model at its starting position and parents it to the room's view
/// coordinate system.
void func_acropolis_west_elevator_hall_8017F64C(Task* task)
{
    TmdObject*         extra;
    GsCOORDINATE2*     coord;
    AwehElevatorState* work;

    extra = (TmdObject*)task->extra;
    coord = extra->coords;
    work  = (AwehElevatorState*)memCalloc(sizeof(AwehElevatorState), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work        = (TaskIdMap*)work;
    work->field_0     = 0;
    extra->flags      = 0;
    coord->sub        = &gGfxViewCoord;
    coord->coord.t[0] = -1000;
    coord->coord.t[1] = -20;
    coord->coord.t[2] = 0x974;
    coord->flg        = 0;
    task->state++;
}

/// Fourth state of the elevator task: drives the car along its shaft from the
/// task's per-frame step, clamps the travel to [0, 0x2D0], and refreshes the
/// model's world matrix and lighting from the resulting position.
void func_acropolis_west_elevator_hall_8017F6F0(Task* task)
{
    VECTOR             pos;
    TmdObject*         extra;
    GsCOORDINATE2*     coord;
    AwehElevatorState* work;

    work  = (AwehElevatorState*)task->work;
    extra = (TmdObject*)task->extra;
    coord = extra->coords;

    work->field_0 += task->spawnArg1 * 0x14;
    if (work->field_0 < 0) {
        work->field_0 = 0;
    }
    if (work->field_0 >= 0x2D1) {
        work->field_0 = 0x2D0;
    }
    coord->coord.t[0] = (work->field_0 * (s32)task->spawnArg2) - 1000;
    if ((u8)gGameSession->at4.loc.view == 5) {
        extra->flags = 0;
    } else {
        extra->flags = 0x80;
    }
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    func_800D7A9C(extra, &pos, 0, 3);
}

/// Third state of the elevator task: on the two session phases that use it,
/// spawns the lift's ambient effects around the room's coordinate system.
void func_acropolis_west_elevator_hall_8017F7D4(Task* task)
{
    SVECTOR        pos;
    SVECTOR        altPos;
    GsCOORDINATE2* coord;

    coord = ((TmdObject*)task->extra)->coords;
    switch (task->state) {
        case 0:
            task->msgTable = D_acropolis_west_elevator_hall_801849F4;
            Game_SetPtrSlot(task, 5);
            Task_Spawn(1, 0x25, 0, 0);
            Task_Spawn(1, 0x25, 1, 0);
            task->state = task->state + 1;
            return;
        case 1:
            if ((u8)gGameSession->at4.loc.view == 2) {
                pos = D_acropolis_west_elevator_hall_8017D5EC;
                Gp_SpawnEff(0x6001F, coord, 0x1804, &pos);
                pos.vx = -0x1800;
                pos.vy = -0x4F0;
                pos.vz = -0x600;
                Gp_SpawnEff(0x6001F, coord, 0x803, &pos);
                pos.vx = -0x1800;
                pos.vy = -0x4F0;
                pos.vz = -0x2C0;
                Gp_SpawnEff(0x6001F, coord, 0x803, &pos);
            }
            if ((u8)gGameSession->at4.loc.view == 5) {
                altPos = D_acropolis_west_elevator_hall_8017D5F4;
                Gp_SpawnEff(0x60025, coord, 0, &altPos);
            }
            return;
    }
}

/// Per-frame update of the lift bay's lighting: ramps `GpEffWork::scale`
/// from 0 to 0x1000 in 0x800 steps, re-blending the bay CLUT towards its lit
/// palette on every step it takes, and latching `angle` once the ramp is
/// full. On every session phase but 5 the CLUT is then blended straight back
/// to the unlit palette and the effect's work object is released, so only
/// phase 5 keeps the lit bay on screen.
void func_acropolis_west_elevator_hall_8017F990(Task* task)
{
    GpEffWork* work;
    s32        i;
    s32        blend;

    work  = (GpEffWork*)task->spawnArg2;
    blend = 0;
    if (work->angle == 0) {
        work->scale = work->scale + 0x800;
        if (work->scale == 0x1000) {
            work->angle = 1;
        }
        blend = 1;
    }

    if (blend != 0) {
        for (i = 0; i < 0x100; i += 0x10) {
            Gp_BlendRgb555Clut(&D_acropolis_west_elevator_hall_80184C04[i],
                               &D_acropolis_west_elevator_hall_80184A04[i], work->scale,
                               &D_acropolis_west_elevator_hall_80184E04[i]);
        }
        Gp_LoadImages(D_acropolis_west_elevator_hall_80185004);
    }

    if ((u8)gGameSession->at4.loc.view != 5) {
        for (i = 0; i < 0x100; i += 0x10) {
            Gp_BlendRgb555Clut(&D_acropolis_west_elevator_hall_80184C04[i],
                               &D_acropolis_west_elevator_hall_80184A04[i], 0,
                               &D_acropolis_west_elevator_hall_80184E04[i]);
        }
        Gp_LoadImages(D_acropolis_west_elevator_hall_80185004);
        Gp_ReleaseState1CMem(work, task);
    }
}

/// Draws one frame of a pair of red-shaded gradient quads and then retires
/// the task. The task coordinate's origin is projected once through
/// `GsWSMATRIX` (`RTPS`) into a 0x14-byte `G_SCRATCH_HEAD` block; anything
/// nearer than `otz` 0x11 is not drawn. The red level pulses with the global
/// counter `D_80070F70` times `spawnArg1`'s low byte, folded into a 0..0x80
/// triangle; `spawnArg1`'s second byte sets the quads' extent, divided by
/// `otz` so they shrink with distance.
void func_acropolis_west_elevator_hall_8017FAE8(Task* arg0)
{
    u8*               head;
    u8*               raw;
    RoomShaftScratch* block;
    POLY_G4*          prim;
    GsCOORDINATE2*    coord;
    void*             mem;
    u16               vz;
    s32               i;
    s32               red;
    s32               pulse;
    s32               level;

    coord = ((TmdObject*)arg0->extra)->coords;
    mem   = arg0->spawnArg2;
    Gp_UpdateCoord(coord);
    head = *(void**)G_SCRATCH_HEAD;
    raw  = head - 0x14;
    /* `raw` and `block` have to stay separate registers: the ROM computes the
       block address into a scratch register and copies it into the callee-saved
       one the rest of the function uses. */
    SOFT_TOUCH_REG(raw);
    block                   = (RoomShaftScratch*)raw;
    block->vec.vx           = *(u16*)&coord->workm.t[0];
    block->vec.vy           = *(u16*)&coord->workm.t[1];
    vz                      = *(u16*)&coord->workm.t[2];
    *(void**)G_SCRATCH_HEAD = block;
    block->vec.vz           = vz;

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
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    Gp_ReleaseState1CMem(mem, arg0);
}

/// Copies 82 scanlines through DR_MOVE packets, applying a horizontal cosine
/// distortion to a 120-pixel-wide strip of the current frame buffer. During
/// alternate 128-frame intervals, each row uses a random divisor for the
/// displacement. The packets share OT slot 0x72, then the task retires.
void func_acropolis_west_elevator_hall_8017FE18(Task* task)
{
    RECT         rect;
    DR_MOVE*     mv;
    void*        mem;
    unsigned int tagMask;
    s32          otOfs;
    s32          byteOfs;
    s32          i;
    s32          base;
    s32          y;
    s32          t;
    s32          v;
    s32          rng;
    s32          phase;
    s32          frame;

    mem  = task->spawnArg2;
    base = gDisplayState.drawBuffer * 0x110 + 0x50;

    otOfs = 0x72;
    for (i = 0; i < 0x52; i++) {
        phase = i * 2;
        frame = gDisplayState.animFrame;
        USE_REG2(frame, phase);
        y = i + base;
        SOFT_TOUCH_REG(y);
        t = 0x800 - rcos((frame + phase) * 16);
        if (gDisplayState.animFrame & 0x80) {
            rng = Gp_LcgState * 5 + 0x71357911;
            {
                s32 quotient = t / (s32)((((u32)rng >> 16) & 0x3F) + 0xC0);
                SOFT_USE_REG(quotient);
                v           = quotient;
                Gp_LcgState = rng;
            }
        } else {
            v = t / 0x100;
        }

        byteOfs        = otOfs << 2;
        v             += 0x50;
        rect.x         = v;
        rect.y         = y;
        rect.w         = 0x78;
        rect.h         = 1;
        tagMask        = 0xFF000000;
        mv             = (DR_MOVE*)gGpuPrimCursor;
        gGpuPrimCursor = mv + 1;
        SetDrawMove(mv, &rect, 0x50, i + base);
        {
            u_long* ot;
            u_long  mask;
            u_long  addrMask = 0xFFFFFF;
            SOFT_USE_REG(addrMask);
            ot   = (u_long*)(byteOfs + (s32)gGpuCurrentOt);
            mask = tagMask;
            SOFT_TOUCH_REG_USE(mask, mv);
            mv->tag = (mv->tag & mask) | getaddr(ot);
            *ot     = (*ot & mask) | ((u_long)mv & addrMask);
        }
    }

    Gp_ReleaseState1CMem(mem, task);
}

/// Draws one frame of the hall's soft light billboard and then retires the
/// task. The effect coordinate is projected through `GsWSMATRIX` with a
/// single `RTPS`, and the resulting screen point becomes the centre of
/// a semi-transparent `POLY_FT4` whose half-extent shrinks with distance
/// (`0x6700 / otz`). Sprites closer than `otz == 0x11` are skipped entirely,
/// which is why the primitive is claimed from `gGpuPrimCursor` before the
/// depth test but only filled in and linked afterwards.
void func_acropolis_west_elevator_hall_8017FFE4(Task* arg0)
{
    void**            scratch;
    u8*               head;
    RoomShaftScratch* block;
    s32*              otzp;
    GsCOORDINATE2*    coord;
    void*             mem;
    POLY_FT4*         prim;
    u16               vz;

    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    mem   = arg0->spawnArg2;
    Gp_UpdateCoord(coord);
    scratch       = (void**)G_SCRATCH_HEAD;
    head          = *scratch;
    block         = (RoomShaftScratch*)(head - 0x14);
    otzp          = &block->otz;
    block->vec.vx = *(u16*)&coord->workm.t[0];
    block->vec.vy = *(u16*)&coord->workm.t[1];
    vz            = *(u16*)&coord->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomShaftScratch*)(head - 0x14))->vec);
    gte_rtps();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((RoomShaftScratch*)(head - 0x14))->sx);
    gte_stszotz(otzp);
    if (((RoomShaftScratch*)(head - 0x14))->otz >= 0x11) {
        prim->tpage      = 0xAB;
        prim->clut       = 0x4380;
        prim->u0         = 0;
        prim->v0         = 0;
        prim->u1         = 0x67;
        prim->v1         = 0;
        prim->u2         = 0;
        prim->v2         = 0x67;
        prim->u3         = 0x67;
        prim->v3         = 0x67;
        prim->code      |= 3;
        block->halfWidth = 0x6700 / ((RoomShaftScratch*)(head - 0x14))->otz;
        prim->x0 = prim->x2 = block->sx - *(u16*)&block->halfWidth;
        prim->x1 = prim->x3 = block->sx + *(u16*)&block->halfWidth;
        prim->y0 = prim->y1 = block->sy - *(u16*)&block->halfWidth;
        prim->y2 = prim->y3 = block->sy + *(u16*)&block->halfWidth;
        addPrim((u_long*)(((((u32)((RoomShaftScratch*)(head - 0x14))->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    Gp_ReleaseState1CMem(mem, arg0);
}

s32 func_acropolis_west_elevator_hall_80180274(void)
{
    Gp_SpawnEff(0x60033, NULL, 0, NULL);
    return 0;
}
