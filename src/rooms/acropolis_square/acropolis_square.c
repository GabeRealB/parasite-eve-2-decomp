#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/gtemac.h>

#include "decomp/common.h"
#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/text.h"
#include "main/tmd.h"
#include "main/ui.h"
#include "main/wipsys.h"
#include "rooms/room.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_80181228.h"

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

extern UiObjectDesc D_800611E4;
extern UiObject*    D_80067634;
extern s32          D_80070F70;
extern u8           D_80071086;
extern u16          D_80072174;
extern s8           D_80072176;
extern s8           D_80072310;
extern u16          D_80072834;
extern u16          D_80072836;
extern u8           D_80072A93;
extern s32          D_80072A94;
extern s32          D_80072A98;
extern UiObjectDesc D_8010EFA0;
extern s16          D_80114D08;
extern u32          D_80115694;

/// Index of the mirrored player's coordinate part each held-object reflection
/// is parented to, by `Task::spawnArg1`.
extern u8 D_acropolis_square_80183464[];

/// Task descriptor of the held-object reflections the mirror spawns.
extern TaskDesc D_acropolis_square_80183468;

/// Labels of the four menu entries of the play-data menu panel: "Save",
/// "Play Data", "Weapon Data" and "PE Data".
extern u8 D_acropolis_square_80183480[];
extern u8 D_acropolis_square_80183488[];
extern u8 D_acropolis_square_80183494[];
extern u8 D_acropolis_square_801834A0[];

/// Row labels of the play-data statistics panel, one per row
/// `func_acropolis_square_8017F46C` draws.
extern u8 D_acropolis_square_801834A8[];
extern u8 D_acropolis_square_801834D8[];
extern u8 D_acropolis_square_801834B0[];
extern u8 D_acropolis_square_801834B4[];
extern u8 D_acropolis_square_801834BC[];
extern u8 D_acropolis_square_801834C8[];
extern u8 D_acropolis_square_801834E0[];
extern u8 D_acropolis_square_801834E8[];
extern u8 D_acropolis_square_801834F0[];

/// The suffix appended to that panel's count rows.
extern u8 D_acropolis_square_801834F8[];

/// The "%" suffix the room's percentage formatters append.
extern u8 D_acropolis_square_80183500[];

/// Help texts of the statistics panel's nine rows, handed to the UI holder for
/// the selected row.
extern u8 D_acropolis_square_80183504[];
extern u8 D_acropolis_square_80183530[];
extern u8 D_acropolis_square_80183554[];
extern u8 D_acropolis_square_80183584[];
extern u8 D_acropolis_square_801835B8[];
extern u8 D_acropolis_square_801835EC[];
extern u8 D_acropolis_square_80183624[];
extern u8 D_acropolis_square_80183658[];
extern u8 D_acropolis_square_80183690[];

/// The play-data menu panel's list.
extern UiList D_acropolis_square_801836CC;

/// The usage panel's list.
extern UiList D_acropolis_square_801836F4;

/// UI descriptor the play-data panels spawn when they first open.
extern UiObjectDesc D_acropolis_square_80183718;

/// UI descriptors the "Play Data" entry and the two usage entries open.
extern UiObjectDesc D_acropolis_square_80183734;
extern UiObjectDesc D_acropolis_square_80183750;

/// The telephone menu panel's list.
extern UiList D_acropolis_square_8018377C;

/// Task descriptor table of the room's cutscenes: entry 0 is the cutscene
/// runner, spawned with a cutscene record as its argument, and entry 1 the
/// sound task the runner spawns for the scene.
extern TaskDesc D_acropolis_square_801837A0;

/// The room's message table, installed on the room entry task.
extern GpMsgEntry D_acropolis_square_801837C4[];

extern TaskDesc D_acropolis_square_80183808;
extern s32      D_acropolis_square_8018382C;
extern s32      D_acropolis_square_80183830;
extern s32      D_acropolis_square_80183834;
extern s32      D_acropolis_square_8018399C;
extern s32      D_acropolis_square_801838DC;
extern s32      D_acropolis_square_80183A5C;
extern s32      D_acropolis_square_80183B34[];
extern s32      D_acropolis_square_80183B58;
extern s16      D_acropolis_square_80183B68[];
extern s32      D_acropolis_square_80183B98;

/// The area records applied when a scene ends with game-flag nibble 0x7A at 1,
/// nibble 0 at 2 and the save's location at 0x0101 in its upper half.
extern GpAreaApplyRec D_acropolis_square_80188888;

extern s32   D_acropolis_square_80188898;
extern Task* D_acropolis_square_8018889C;
extern s32   D_acropolis_square_801888A0;
extern s32   D_acropolis_square_801888A4;

/// The scene sub-task while it runs, NULL otherwise.
extern Task* D_acropolis_square_801888A8;

/// The cutscene record the room hands entry 0 of `D_acropolis_square_801837A0`.
extern RoomCutsceneRec D_acropolis_square_801888AC;

extern GpCoord D_acropolis_square_801888CC;

void func_acropolis_square_8017D8C8(Task* task);
void func_acropolis_square_801811EC(Task* task);
void func_acropolis_square_80182260(Task* task);
void func_acropolis_square_801822A4(Task* task);

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

/// Sets up the room's mirror: re-attaches the player's own TMD source
/// to this task so the reflection draws the same model, allocates the
/// `RoomMirrorWork` block the reflection's coordinate frame and matrices live
/// in, and hangs the task off the player task so it dies with it.
/// `spawnArg1` must be 0 or 1, and 0 also raises `GameSession::field_4E`. The
/// two child tasks reflect the player's held-object tasks
/// (`GameActor::field_920` / `field_924`).
void func_acropolis_square_8017D714(Task* task)
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
            spawned = Task_SpawnFromTable(&D_acropolis_square_80183468, 1, i, (s32)task);
            if (spawned != NULL) {
                Task_Reparent(child, spawned);
            }
        }
    }
    func_acropolis_square_8017D8C8(task);
}

/// Per-frame state of the mirror task `func_acropolis_square_8017D714` sets up.
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
void func_acropolis_square_8017D8C8(Task* task)
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
                spawned = Task_SpawnFromTable(&D_acropolis_square_80183468, 1, i + 2, (s32)task);
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

/// Scale applied to reflections with `spawnArg1 >= 2`: X negated, Y and Z kept.
const VECTOR D_acropolis_square_8017D5C4 = { -0x1000, 0x1000, 0x1000 };

/// Per-frame callback of a held-object reflection. `Task::spawnArg2` is the
/// mirror task the room set up and the parent is the held-object task being
/// reflected. On the first frame it clones the parent's TMD source, parents the clone's root coordinate to the
/// mirrored player's corresponding part, points the clone at the mirror's
/// light and color matrices and negates the X translation; every frame it
/// republishes the mirror model's draw flags onto the clone.
void func_acropolis_square_8017F24C(Task* task)
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
    mirrorPart  = &mirror->extra.tmd->coords[D_acropolis_square_80183464[task->spawnArg1]];
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
            scale = D_acropolis_square_8017D5C4;
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

/// Mirror task: runs the set-up state, then the per-frame state.
void func_acropolis_square_8017F41C(Task* task)
{
    TaskFunc states[2] = {
        func_acropolis_square_8017D714,
        func_acropolis_square_8017D8C8,
    };

    states[task->state](task);
}

/// Draws one row of the play-data statistics panel: the row label, then the
/// statistic `arg0->field_8` selects - play time, several save counters, and
/// two percentages printed with two decimals and a "%" suffix. While the row is
/// selected its help text goes to the UI holder.
void func_acropolis_square_8017F46C(DialogPrompt* arg0, UiObject* arg1)
{
    u8  buf[0x20];
    u8* p;

    p = buf;
    if (((arg1->status >> 16) == 1) || (arg1->status == 1)) {
        if (arg0->field_10 == arg0->field_8) {
            u8* tbl[9] = {
                D_acropolis_square_80183504,
                D_acropolis_square_80183530,
                D_acropolis_square_80183554,
                D_acropolis_square_80183584,
                D_acropolis_square_801835B8,
                D_acropolis_square_801835EC,
                D_acropolis_square_80183624,
                D_acropolis_square_80183658,
                D_acropolis_square_80183690,
            };

            Ui_SetHolderParam((s32)tbl[arg0->field_8], 0, 0);
        }
    }

    switch (arg0->field_8) {
        case 0: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_acropolis_square_801834A8);
            Text_FormatTime(p, D_80072174);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 1: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_acropolis_square_801834D8);
            Text_ItoaUnsigned(p, D_80072A93);
            Text_Strcat(p, D_acropolis_square_801834F8);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 2: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_acropolis_square_801834B0);
            Text_ItoaUnsigned(p, D_80072834);
            Text_Strcat(p, D_acropolis_square_801834F8);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 3: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_acropolis_square_801834B4);
            Text_ItoaUnsigned(p, D_80072836);
            Text_Strcat(p, D_acropolis_square_801834F8);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 4: {
            TextDrawReq req;
            s32         y;
            s32         pct;
            s32         len;
            s32         n;
            s32         i;
            u8*         q;

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_acropolis_square_801834BC);
            if (Mc_SaveData.field_6CC == 0) {
                pct = 0;
            } else {
                pct = (Mc_SaveData.field_6CC * 10000) / (Mc_SaveData.field_6CC + Mc_SaveData.field_6CE);
            }
            if (pct < 100) {
                func_8002F44C(p, pct, 3);
            } else {
                Text_ItoaUnsigned(p, pct);
            }
            n   = 2;
            q   = p;
            len = 0;
            while (*q != 0) {
                q++;
                len++;
            }
            if (len < n) {
                n = len;
            }
            n++;
            for (i = 0; i < n; i++) {
                q[1] = q[0];
                q--;
            }
            q[1] = 0x2E;
            Text_Strcat(p, D_acropolis_square_80183500);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 5: {
            TextDrawReq req;
            s32         y;
            s32         pct;
            s32         total;
            s32         cnt;
            s32         len;
            s32         n;
            s32         i;
            u8*         q;

            total          = D_80072834;
            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_acropolis_square_801834C8);
            cnt   = 326;
            total = total + (GameFlag_GetNibble(0x167) + GameFlag_GetNibble(0x168));
            if (total == 0) {
                pct = 0;
            } else {
                pct = (total * 10000) / cnt;
            }
            if (pct < 100) {
                func_8002F44C(p, pct, 3);
            } else {
                Text_ItoaUnsigned(p, pct);
            }
            n   = 2;
            q   = p;
            len = 0;
            while (*q != 0) {
                q++;
                len++;
            }
            if (len < n) {
                n = len;
            }
            n++;
            for (i = 0; i < n; i++) {
                q[1] = q[0];
                q--;
            }
            q[1] = 0x2E;
            Text_Strcat(p, D_acropolis_square_80183500);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            Ui_DrawHBar((UiPanel*)arg1, arg1->field_1C, (s16)arg1->field_1E, arg0->field_1A + 3);
            arg0->field_1A = (u16)arg0->field_1A + 5;
            break;
        }
        case 6: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_acropolis_square_801834E0);
            Text_ItoaUnsigned(p, D_80072176);
            Text_Strcat(p, D_acropolis_square_801834F8);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
            break;
        }
        case 7: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_acropolis_square_801834E8);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, D_80072A94), arg0->field_1C, 3, 2);
            break;
        }
        case 8: {
            TextDrawReq req;
            s32         y;

            req.x          = arg1->baseX + (u16)arg0->field_18;
            y              = arg1->baseY - 6;
            req.y          = (u16)arg0->field_1A + y;
            req.otIndex    = (s16)arg1->drawOrder + 1;
            req.field_8    = arg0->field_1C;
            req.glyphTable = 0;
            req.centerMode = 0;
            req.field_E    = 1;
            func_8002E53C(&req, D_acropolis_square_801834F0);
            Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, Text_ItoaUnsigned(p, D_80072A98), arg0->field_1C, 3, 2);
            break;
        }
    }
}

/// "Play Data", the title of the play-data menu panel
/// `func_acropolis_square_80180CBC` draws.
const char D_acropolis_square_8017D620[] = "Play Data";

/// Text drawn in place of a usage row's percentage once it reaches 100 percent.
const u8 D_acropolis_square_8017D62C[] = "100.0%";

/// Draws one row of a play-data usage panel from the `RoomItemUsage` block at
/// the owner task's `work`: the item's name and icon, its share of all uses as
/// a two-decimal percentage, and a gouraud bar scaled by the row's
/// `barWidths`. While the row is selected the item is previewed, and a confirm
/// opens the item's detail panel.
void func_acropolis_square_8017FC38(DialogPrompt* arg0, UiObject* arg1)
{
    u8             buf[0x20];
    TextDrawReq    req;
    TextDrawReq*   r;
    RoomItemUsage* work;
    POLY_G4*       prim;
    u8*            p;
    u8*            q;
    s32            item;
    s32            value;
    s32            x;
    s32            y;
    s32            color;
    s32            textY;
    s32            limit;
    s32            n;
    s32            len;
    s32            i;
    s32            avail;
    s32            base;
    s32            barW;
    s32            barX;
    s32            rowY;
    s32            one;
    s32            tx;
    s32            ty;

    p     = buf;
    r     = &req;
    x     = arg0->field_18;
    y     = arg0->field_1A;
    work  = (RoomItemUsage*)arg1->owner->work;
    item  = work->itemIds[arg0->field_8];
    value = work->percents[arg0->field_8];
    color = arg0->field_1C;
    if (arg1->mode != 5) {
        req.x          = arg1->baseX + 0x11 + x;
        textY          = arg1->baseY - 6;
        req.y          = textY + y;
        req.otIndex    = (s16)arg1->drawOrder + 1;
        req.field_8    = color;
        req.glyphTable = 0;
        req.centerMode = 0;
        r->field_E     = 1;
        func_8002E53C(r, (u8*)Gp_GetItemText(item, 0, 0));
        func_800CE5D0(arg1, x, y, item);
    }
    limit = 1;
    if (value >= 10000) {
        Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, D_acropolis_square_8017D62C, arg0->field_1C, 3, 2);
    } else {
        for (i = 2; i > 0; i--) {
            limit *= 10;
        }
        if (value < limit) {
            func_8002F44C(p, value, 3);
        } else {
            Text_ItoaUnsigned(p, value);
        }
        n   = 2;
        q   = p;
        len = 0;
        while (*q != 0) {
            q++;
            len++;
        }
        if (len < n) {
            n = len;
        }
        n++;
        for (len = 0; len < n; len++) {
            q[1] = q[0];
            q--;
        }
        q[1] = '.';
        Text_Strcat(p, D_acropolis_square_80183500);
        Text_DrawPrompt(arg1, -arg0->field_18, arg0->field_1A, buf, arg0->field_1C, 3, 2);
    }

    base  = (s16)arg1->field_1C + 0x80;
    avail = (s16)arg1->field_1E - 0x4A;
    barW  = avail - base;
    barW  = (barW * work->barWidths[arg0->field_8]) >> 12;
    rowY  = arg0->field_1A - 0xC;
    barW  = barW + 2;
    barX  = avail - barW;
    if (barW >= 2) {
        prim             = (POLY_G4*)gGpuPrimCursor;
        tx               = arg1->baseX + barX + 1;
        prim->x2         = tx;
        prim->x0         = tx;
        ty               = arg1->baseY;
        gGpuPrimCursor   = prim + 1;
        ty               = ty + rowY;
        ty              += 1;
        *(s32*)&prim->r3 = 0x10000;
        *(s32*)&prim->r1 = 0x10000;
        setlen(prim, 8);
        *(s32*)&prim->r0 = 0x100B0;
        setcode(prim, 0x38);
        *(s32*)&prim->r2 = 0x100B0;
        tx               = (u16)prim->x0 + barW - 1;
        prim->y1         = ty;
        prim->y0         = ty;
        ty              += 8;
        prim->y3         = ty;
        prim->y2         = ty;
        prim->x3         = tx;
        prim->x1         = tx;
        addPrim(gGpuCurrentOt + (s16)arg1->drawOrder + 1, prim);
    }
    one = 1;
    func_80046B34((UiPanel*)arg1, barX, arg0->field_1A - 0xC, barW, 9, 0, one);
    if (((arg1->status >> 16) == one) || (arg1->status == one)) {
        if (arg0->field_10 == arg0->field_8) {
            Gp_SetPreviewItem(item, 0);
            Gp_SetHolderItemText(item);
        }
    }
    if (arg0->field_C == 1) {
        if (Pad_CheckButtons(0, 1, 0x10) != 0) {
            SndEvt_EnqueueType6(3, 0, 0);
            Ui_SpawnFromDesc(&D_8010EFA0, item, 1, 1, arg1);
            arg1->status = 0;
        }
    }
}

/// Builds the "Play Data" item-usage panel's three parallel arrays from the
/// save's per-item use counters (`Mc_SaveData.weaponUseCounts`, ids 0x80-0x9F).
///
/// Every id whose name is non-empty (a leading 0 or 0xA marks an unused row)
/// and whose counter is non-zero is marked seen and appended to `itemIds`,
/// while the counters are summed. The ids are then insertion-sorted by use
/// count, most-used first. Finally each row gets `percents` - its share of all
/// recorded uses in hundredths of a percent, rounded - and `barWidths`, its
/// counter as a 12-bit fraction of the top row's. Both are scaled down by
/// halving until the top counter fits in 17 bits, so the multiply and the
/// shift cannot overflow.
void func_acropolis_square_80180034(UiList* list, UiObject* obj)
{
    RoomItemUsage* work;
    s32            count;
    s32            total;
    s32            i;
    s32            j;
    s32            k;
    s32            id;
    s32            tmp;
    s32            uses;
    s32            scale;
    s32            top;
    s32            shift;
    s16*           p;
    u8             c;

    count = 0;
    total = 0;
    work  = (RoomItemUsage*)obj->owner->work;
    p     = work->itemIds;

    for (i = 0; i < 0x20; i++) {
        id = i + 0x80;
        c  = *Gp_GetItemText(id, 0, 1);
        if ((c != 0) && (c != 0xA) && (Mc_SaveData.weaponUseCounts[i] > 0)) {
            Gp_SetItemSeenBit(id, 1);
            *p++ = id;
            count++;
            total += Mc_SaveData.weaponUseCounts[i];
        }
    }

    if (count >= 2) {
        for (i = 1; i < count; i++) {
            uses = Mc_SaveData.weaponUseCounts[work->itemIds[i] - 0x80];
            for (j = 0; j < i; j++) {
                if (Mc_SaveData.weaponUseCounts[work->itemIds[j] - 0x80] < uses) {
                    tmp = work->itemIds[i];
                    for (k = i - 1; k >= j; k--) {
                        work->itemIds[k + 1] = work->itemIds[k];
                    }
                    work->itemIds[j] = tmp;
                    break;
                }
            }
        }
    }

    if (count > 0) {
        scale = 0x4E20;
        top   = Mc_SaveData.weaponUseCounts[work->itemIds[0] - 0x80];
        shift = 0xC;
        while (top > 0x1869F) {
            top   >>= 1;
            scale >>= 1;
            total >>= 1;
            shift--;
        }
        for (i = 0; i < count; i++) {
            work->percents[i] =
                (u32)((Mc_SaveData.weaponUseCounts[work->itemIds[i] - 0x80] * scale) / total + 1) >> 1;
            work->barWidths[i] =
                (Mc_SaveData.weaponUseCounts[work->itemIds[i] - 0x80] << shift) / top;
        }
    }

    list->field_4  = count;
    list->field_9  = 0;
    list->field_10 = 0;
}

/// Parasite Energy counterpart of `func_acropolis_square_80180034`:
/// fills the "Play Data" PE-usage panel's
/// `RoomPeUsage` block from the save's per-slot use counters.
///
/// Each of the twelve Parasite Energy slots owns three consecutive ids starting
/// at 0xF, one per level, so slot `i` at level `Mc_SaveData.attachLevels[i]`
/// prints as `i * 3 + 0xF + level - 1` (a slot the player has never levelled
/// keeps the base id). Every slot with a non-zero counter in
/// `Mc_SaveData.attachUseCounts` - which really runs twelve entries wide, past the
/// seven the struct names - is appended and its counter summed. The ids are
/// then insertion-sorted by use count, most-used first, and each row gets
/// `percents`, its share of all recorded uses in hundredths of a percent, and
/// `barWidths`, its counter as a 12-bit fraction of the top row's. Both are
/// scaled down by halving until the top counter fits in 17 bits, so the
/// multiply and the shift cannot overflow.
void func_acropolis_square_80180330(UiList* list, UiObject* obj)
{
    RoomPeUsage* work;
    s16*         p;
    s32          count;
    s32          total;
    s32          i;
    s32          j;
    s32          k;
    s32          id;
    s32          slot;
    s32          uses;
    s32          scale;
    s32          shift;
    s32          top;
    s32          tmp;
    /* Matching only. The original object frames 24 bytes it never touches and
     * materialises GCC's `/3` magic constant before the first loop for a use
     * that no longer survives, so `scratch` buys the frame size and `magic`
     * plus the (instruction-free) SOFT_USE_REG below buy the allocation. */
    s32 magic;
    s16 scratch[12];

    count = 0;
    total = 0;
    i     = 0;
    work  = (RoomPeUsage*)obj->owner->work;
    p     = work->peIds;
    magic = 0x55555556;

    for (; i < 12; i++) {
        if (Mc_SaveData.attachUseCounts[i] > 0) {
            id = i * 3 + 0xF;
            *p = id;
            if (Mc_SaveData.attachLevels[i] != 0) {
                *p = id + (Mc_SaveData.attachLevels[i] - 1u);
            }
            p++;
            count++;
            total += Mc_SaveData.attachUseCounts[i];
        }
    }
    SOFT_USE_REG(magic);

    if (count >= 2) {
        for (i = 1; i < count; i++) {
            slot = (work->peIds[i] - 0xF) / 3;
            uses = Mc_SaveData.attachUseCounts[slot];
            for (j = 0; j < i; j++) {
                slot = (work->peIds[j] - 0xF) / 3;
                if (Mc_SaveData.attachUseCounts[slot] < uses) {
                    tmp = work->peIds[i];
                    for (k = i - 1; k >= j; k--) {
                        work->peIds[k + 1] = work->peIds[k];
                    }
                    work->peIds[j] = tmp;
                    break;
                }
            }
        }
    }

    if (count > 0) {
        scale = 0x4E20;
        slot  = (work->peIds[0] - 0xF) / 3;
        top   = Mc_SaveData.attachUseCounts[slot];
        shift = 0xC;
        while (top > 0x1869F) {
            top   >>= 1;
            scale >>= 1;
            total >>= 1;
            shift--;
        }
        for (i = 0; i < count; i++) {
            slot               = (work->peIds[i] - 0xF) / 3;
            work->percents[i]  = (u32)((Mc_SaveData.attachUseCounts[slot] * scale) / total + 1) >> 1;
            slot               = (work->peIds[i] - 0xF) / 3;
            work->barWidths[i] = (Mc_SaveData.attachUseCounts[slot] << shift) / top;
        }
    }

    list->field_4  = count;
    list->field_9  = 0;
    list->field_10 = 0;
}

/// Titles of the usage panel, one per kind: weapons and Parasite Energy.
const char D_acropolis_square_8017D634[] = "Weapon Data";
const char D_acropolis_square_8017D640[] = "PE Data";

/// Task of the usage panel: draws the weapon or Parasite Energy title
/// (`spawnArg1`), and on its first tick allocates the `RoomItemUsage` /
/// `RoomPeUsage` block, spawns the panel and fills its list. Cancel closes it,
/// and a child panel that closes hands control back.
void func_acropolis_square_80180650(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    Task*     next;
    UiObject* childObj;
    void*     work;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    list          = &D_acropolis_square_801836F4;
    if (task->spawnArg1 == 0) {
        Ui_DrawText((UiPanel*)obj, D_acropolis_square_8017D634);
    } else {
        Ui_DrawText((UiPanel*)obj, D_acropolis_square_8017D640);
    }
    if (task->state == 0) {
        work = memCalloc(0xC4, 0);
        if (work == NULL) {
            return;
        }
        task->work = work;
        Ui_SpawnFromDesc(&D_acropolis_square_80183718, 0, 0, 1, obj);
        if (task->spawnArg1 == 0) {
            func_acropolis_square_80180034(list, obj);
        } else {
            func_acropolis_square_80180330(list, obj);
        }
        Ui_InitList(list, (UiMiniObj*)obj);
        list->field_A = 1;
        Ui_SetListScrollFlag(list, 1);
        task->state += 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    if (obj->status == 1 && Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
        obj->field_2E = 6;
    }
    if (task->firstChild != NULL) {
        child = task->firstChild;
        do {
            childObj = child->spawnArg2;
            next     = child->nextSibling;
            if (childObj->field_2E == -1 || childObj->field_2E == 6) {
                Ui_TeardownTree(childObj, childObj->owner);
                obj->status = 1;
            }
            child = next;
        } while (child != task->firstChild);
    }
}

/// "Telephone", the title of the menu panel `func_acropolis_square_80180804`
/// runs. Two non-zero bytes follow its terminator, so it stays assembly.
extern const char D_acropolis_square_8017D648[];
INCLUDE_RODATA("rooms/nonmatchings/acropolis_square/acropolis_square", D_acropolis_square_8017D648);

/// Task of the telephone menu panel. Until the save allows it (demo scene 1
/// or a clear) it only spawns the generic panel; otherwise it lays out its
/// list and draws the title. Choosing an entry opens the item prompt, and
/// cancel closes the panel.
void func_acropolis_square_80180804(Task* task)
{
    UiObject* obj;
    UiList*   list;
    Task*     child;
    UiObject* childObj;
    s32       ready;
    s32       sel;
    s32       kind;
    s32       mode;
    s32       one;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    ready         = Mc_SaveData.demoScene == 1;
    list          = &D_acropolis_square_8018377C;
    one           = 1;
    if (Mc_SaveData.clearCount > 0) {
        ready = one;
    }
    if (ready == 0) {
        if (task->state == 0) {
            gGameSession->uiOpen = one;
            Ui_SpawnFromDesc(&D_800611E4, 0, 0, 0, obj);
            obj->status   = 0;
            obj->field_4 |= 0x80000000;
            task->state   = task->state + 1;
        }
    } else if (task->state == 0) {
        Ui_LayoutListPanel(list, (UiPanel*)obj);
        obj->status          = one;
        gGameSession->uiOpen = one;
        Ui_SetListScrollFlag(list, 1);
        Gp_ClearPreviewItems();
        D_80067634   = NULL;
        Wip_UiHolder = NULL;
        task->state  = task->state + 1;
    } else {
        /* The literal carries its trailing "\0\1" - the room's rodata has
         * those two bytes right after the string and nothing else claims them. */
        Ui_DrawText((UiPanel*)obj, D_acropolis_square_8017D648);
        Ui_UpdateListNoAnim(list, obj);
    }
    if (obj->field_2E == 6) {
        obj->field_2E = 0;
        Ui_SetState4((Task*)obj, task);
        obj->status = 0;
    }
    if (obj->status == 1 && Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
        if (task->state != 0) {
            SndEvt_EnqueueType6(0x3B, 0, 0);
        }
        gGameSession->uiOpen = 0;
        obj->field_2E        = -1;
        obj->field_2C        = 0x34;
    }
    child = task->firstChild;
    if (child != NULL) {
        childObj = child->spawnArg2;
        sel      = childObj->field_2E;
        switch (sel) {
            case 6:
                if (task->state == 1) {
                    kind = childObj->field_2C;
                    Ui_TeardownTree(childObj, childObj->owner);
                    mode = 0xF;
                    if (kind == 0x33) {
                        mode = 0x11;
                    }
                    Gp_SpawnItemPrompt(obj, mode, 0, 1);
                    if (ready == 0) {
                        task->state = 3;
                    } else {
                        task->state = 2;
                    }
                } else if (task->state == 3) {
                    obj->field_2E = -1;
                    obj->field_2C = 0x34;
                } else {
                    Ui_TeardownTree(childObj, childObj->owner);
                    SndEvt_EnqueueType6(0x3B, 0, 0);
                    Ui_StartCloseAnim((UiPanel*)obj, task);
                    obj->status = 1;
                }
                break;
            case -1:
                if (task->state == 1) {
                    kind = childObj->field_2C;
                    Ui_TeardownTree(childObj, childObj->owner);
                    mode = 0xF;
                    if (kind == 0x33) {
                        mode = 0x11;
                    }
                    Gp_SpawnItemPrompt(obj, mode, 0, 1);
                    if (ready == 0) {
                        task->state = 3;
                    } else {
                        task->state = 2;
                    }
                } else {
                    obj->field_2E = -1;
                    obj->field_2C = 0x34;
                }
                break;
        }
    }
}

/// Task of a prompt panel: on its first tick it becomes the UI holder and
/// installs the save-prompt exit callback; every tick it draws the prompt
/// lines.
void func_acropolis_square_80180AFC(Task* task)
{
    UiObject* obj;

    obj           = task->spawnArg2;
    obj->field_2E = 0;
    if (task->state == 0) {
        Wip_UiHolder       = (WipUiHolder*)obj;
        task->exitCallback = func_acropolis_square_801811EC;
        task->state       += 1;
    }
    Gp_DrawPromptLines(obj, task);
}

/// Inserts a '.' into a digit string so `decimals` characters sit after the
/// point. Walks to the NUL, then shifts the last `min(len, decimals)` bytes
/// one to the right to open a slot. No-op when `decimals <= 0`.
void func_acropolis_square_80180B58(u8* str, s32 decimals)
{
    s32 len;

    len = 0;
    if (decimals > 0) {
        if (*str != 0) {
            do {
                str += 1;
                len += 1;
            } while (*str != 0);
        }
        if (len < decimals) {
            decimals = len;
            SOFT_TOUCH_REG(decimals);
            decimals += 1;
        } else {
            decimals += 1;
        }
        len = 0;
        if (decimals > 0) {
            do {
                len   += 1;
                str[1] = str[0];
                str   -= 1;
            } while (len < decimals);
        }
        str[1] = '.';
    }
}

/// Format `value` as a percentage with `decimals` fractional digits into `buf`:
/// print the integer with at least `decimals + 1` digits when it is small enough
/// (so "5" with two decimals becomes "0.05"), otherwise print it unpadded, then
/// shift the last `decimals` digits right by one and drop a '.' in front of
/// them. Appends "%" and returns `buf`.
u8* func_acropolis_square_80180BC8(u8* buf, s32 value, s32 decimals)
{
    s32 limit;
    s32 i;
    s32 len;
    s32 n;
    u8* p;

    limit = 1;
    for (i = decimals; i > 0; i--) {
        limit *= 10;
    }

    if (value < limit) {
        func_8002F44C(buf, value, decimals + 1);
    } else {
        Text_ItoaUnsigned(buf, value);
    }

    n   = decimals;
    p   = buf;
    len = 0;
    if (n > 0) {
        while (*p != 0) {
            p++;
            len++;
        }
        if (len < n) {
            n = len;
        }
        n++;
        for (len = 0; len < n; len++) {
            p[1] = p[0];
            p--;
        }
        p[1] = '.';
    }

    Text_Strcat(buf, D_acropolis_square_80183500);
    return buf;
}

/// Task of the play-data menu panel: draws "Play Data", and on its first tick
/// spawns the panel and lays out its list. Cancel closes it.
void func_acropolis_square_80180CBC(Task* task)
{
    UiObject* obj;
    UiList*   list;

    list          = &D_acropolis_square_801836CC;
    obj           = task->spawnArg2;
    obj->field_2E = 0;
    Ui_DrawText((UiPanel*)obj, D_acropolis_square_8017D620);
    if (task->state == 0) {
        Ui_SpawnFromDesc(&D_acropolis_square_80183718, 0, 0, 1, obj);
        Ui_LayoutListPanel(list, (UiPanel*)obj);
        obj->field_12 += 5;
        list->field_A  = 1;
        Ui_SetListScrollFlag(list, 1);
        task->state += 1;
    }
    Ui_UpdateListNoAnim(list, obj);
    if (obj->status == 1 && Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
        obj->field_2E = 6;
    }
}

/// Queues a gouraud-shaded rectangle into the current OT one slot past the
/// panel's draw order. Origin is `field_20`/`field_22` plus (`arg1`, `arg2`);
/// `arg3`/`arg4` are width and height. Left vertices take `arg5`, right vertices
/// take `arg6`. A zero color or width < 2 draws nothing.
void func_acropolis_square_80180DAC(UiPanel* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, u32 arg5, s32 arg6)
{
    register s32 dx asm("v1");
    register s32 w asm("t3");
    POLY_G4*     prim;
    s16          x;
    s16          y;

    dx = arg1;
    w  = arg3;
    if ((arg5 != 0) && (w >= 2)) {
        prim           = (POLY_G4*)gGpuPrimCursor;
        x              = arg0->field_20 + dx + 1;
        prim->x2       = x;
        prim->x0       = x;
        y              = arg0->field_22;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 8);
        *(u32*)&prim->r0 = arg5;
        setcode(prim, 0x38);
        *(u32*)&prim->r2 = arg5;
        *(u32*)&prim->r3 = arg6;
        *(u32*)&prim->r1 = arg6;
        y                = y + arg2 + 1;
        x                = (u16)prim->x0 + w - 1;
        prim->y1         = y;
        prim->y0         = y;
        prim->x3         = x;
        prim->x1         = x;
        y                = y + arg4 - 1;
        prim->y3         = y;
        prim->y2         = y;
        addPrim(gGpuCurrentOt + (s16)arg0->field_14 + 1, prim);
    }
}

/// Menu entry "Save": on a confirm while the CD is idle, opens the save panel
/// and moves the owner task to state 1.
void func_acropolis_square_80180EB0(DialogPrompt* prompt, UiObject* obj)
{
    s32 sel;

    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_acropolis_square_80183480, prompt->field_1C, 1, 0);
    sel = prompt->field_C;
    if (sel == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0 && CdCmd_IsIdle() != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        D_80071086 = 0xFF;
        Ui_SpawnFromDesc(&D_800611E4, 1, 0, 0, obj);
        obj->status       = 0;
        obj->field_2E     = 6;
        obj->owner->state = sel;
    }
}

/// Menu entry "Play Data": on a confirm, opens the play-data panel and moves
/// the owner task to state 2.
void func_acropolis_square_80180F94(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_acropolis_square_80183488, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_acropolis_square_80183734, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// Menu entry "Weapon Data": on a confirm, opens the usage panel for weapons
/// and moves the owner task to state 2.
void func_acropolis_square_8018105C(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_acropolis_square_80183494, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_acropolis_square_80183750, 0, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// Menu entry "PE Data": on a confirm, opens the usage panel for Parasite
/// Energy and moves the owner task to state 2.
void func_acropolis_square_80181124(DialogPrompt* prompt, UiObject* obj)
{
    Text_DrawPrompt(obj, prompt->field_18, prompt->field_1A, D_acropolis_square_801834A0, prompt->field_1C, 1, 0);
    if (prompt->field_C == 1 && Pad_CheckButtons(0, 1, Pad_MaskConfirm) != 0) {
        SndEvt_EnqueueType6(0x16, 0, 0);
        Ui_SpawnFromDesc(&D_acropolis_square_80183750, 1, 1, 1, obj);
        obj->field_2E     = 6;
        obj->status       = 0;
        obj->owner->state = 2;
    }
}

/// Task exit callback for the save-prompt UI: if this task still owns
/// `Wip_UiHolder`, clear it, then free the spawned UI object and kill the task.
void func_acropolis_square_801811EC(Task* task)
{
    WipUiHolder* holder;

    holder = task->spawnArg2;
    if (Wip_UiHolder == holder) {
        Wip_UiHolder = NULL;
    }
    Ui_FreeAndKill(task);
}

/// The room's cutscene runner: suppresses the player and ally HUD, loads and
/// starts the scene's caption slot, lets confirm or cancel cut the scene
/// sub-task short, applies the story-flag side effects when the scene ends,
/// and restores everything before killing itself.
void func_acropolis_square_80181228(Task* task)
{
    RoomCutsceneRec* rec;
    s32              killOut;
    s32              flag;
    s32              cmd;
    s32              fadeA;
    s32              fadeB;

    rec = (RoomCutsceneRec*)task->spawnArg2;
    switch (task->state) {
        case 0:
            D_acropolis_square_801888A8 = NULL;
            Gp_MsgPlayerWeapon(0);
            if (Mc_SaveData.companionType == 1) {
                Gp_MsgAllyWeapon(0);
            }
            if (rec->field_0 > 0) {
                D_80115694               = Mc_SaveData.at4.loc.view;
                Mc_SaveData.at4.loc.view = rec->field_0;
            } else {
                D_80115694 = -rec->field_0;
            }
            gGameSession->hideHud    = 1;
            gGameSession->eventState = 1;
            Gp_StateF0.field_4       = 2;
            Gp_MsgPlayer3F3(0);
            Gp_MsgAlly3F3(0);
            if (rec->field_4 != 0) {
                SndEvt_EnqueueType6(rec->field_4, 0, 0);
            }
            task->state++;
            break;
        case 1:
        case 2:
            task->state++;
            break;
        case 3:
            if (rec->field_3 != 0) {
                Gp_CapFile = 0;
                Gp_LoadCapFile(rec->field_3);
                fadeB = 0;
                fadeA = rec->field_14;
                if (fadeA == 0) {
                    fadeA = 0x3C0;
                } else {
                    fadeB = rec->field_16;
                }
                func_800E6D4C(fadeA, fadeB);
            }
            if (rec->field_2 != 0) {
                task->state = 6;
            } else {
                task->state++;
            }
            break;
        case 4:
            D_acropolis_square_801888A8 = Task_SpawnFromTable(&D_acropolis_square_801837A0, 1, 0, rec->field_10);
            Gp_StartCapSlot(rec->field_1, 0, 0x63);
            task->state++;
            break;
        case 5:
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
                SndEvt_EnqueueType7(rec->field_10, 1);
                taskKill(D_acropolis_square_801888A8);
                task->state++;
            } else if (Task_PollKill(D_acropolis_square_801888A8, &killOut) != 0) {
                task->state++;
            }
            break;
        case 6:
            Gp_AbortCap();
            task->state++;
            break;
        case 7:
            if (rec->field_2 == 0) {
                SndEvt_EnqueueType6(rec->field_C, 0, 0);
            }
            flag = GameFlag_GetNibble(0x7A);
            if (flag > 0) {
                if (flag >= 5) {
                    if (flag == 5) {
                        if (GameFlag_GetNibble(0x111) != 0) {
                            if (GameFlag_GetNibble(0x112) == 0) {
                                GameFlag_SetNibble(3, 0);
                                GameFlag_SetNibble(0x155, 9);
                                GameFlag_SetNibble(0x112, 1);
                            }
                        }
                    }
                }
            }
            if (rec->field_1 == 1) {
                Gp_RunCapCmd(GameFlag_GetNibble(0x155) + 0x10, 0);
            } else {
                Gp_RunCapCmd(rec->field_1, 0);
            }
            if (GameFlag_GetNibble(0x7A) == 1) {
                if (GameFlag_GetNibble(0) == 2) {
                    GameFlag_SetNibble(0, 3);
                    GameFlag_SetNibble(0xE, 4);
                    if ((*(u32*)&Mc_SaveData.at4.loc & 0xFFFF0000) == 0x1010000) {
                        Gp_ApplyAreaRecs(&D_acropolis_square_80188888);
                        func_800E3FAC(0xA2, 5);
                    }
                }
            }
            task->state++;
            break;
        case 8:
            if (Gp_CapBusy() == 0) {
                if ((GameFlag_GetNibble(0x155) == 0xE) && (GameFlag_GetNibble(3) == 0)) {
                    GameFlag_SetNibble(3, 1);
                    task->state = 0x14;
                } else {
                    Gp_RunCapCmd1(task->spawnArg1);
                    task->state++;
                }
            }
            break;
        case 9:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 10:
            task->state++;
            break;
        case 11:
            Gp_MsgPlayer3F3(1);
            Gp_MsgAlly3F3(1);
            Mc_SaveData.at4.loc.view = D_80115694;
            task->state++;
            break;
        case 12:
        case 13:
            task->state++;
            break;
        case 14:
            SndEvt_EnqueueType6(rec->field_8, 0, 0);
            Gp_MsgPlayerWeapon(1);
            if (Mc_SaveData.companionType == 1) {
                Gp_MsgAllyWeapon(1);
            }
            gGameSession->hideHud    = 0;
            gGameSession->eventState = 0;
            Gp_StateF0.field_4       = 0;
            if (rec->field_3 != 0) {
                Gp_ResetCap();
            }
            D_80114D08 = 0xA;
            taskKill(task);
            break;
        case 20:
            Gp_RunCapCmd(GameFlag_GetNibble(0x155) + 0x10, 0);
            task->state++;
            break;
        case 21:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 22:
            switch (Gp_GetCapEventKey()) {
                case 11:
                    Gp_RunCapCmd(0x20, 0);
                    task->state++;
                    break;
                case 12:
                    Gp_RunCapCmd(0x21, 0);
                    task->state++;
                    break;
                default:
                    GameFlag_SetNibble(3, 2);
                    task->state = 8;
                    break;
            }
            break;
        case 23:
            if (Gp_CapBusy() == 0) {
                task->state = 0x14;
            }
            break;
    }
}

/// State handlers of the room entry task `func_acropolis_square_80182308`,
/// indexed by `Task::state`: the set-up tick, the idle tick, and `taskKill`.
const TaskFuncTable3 D_acropolis_square_8017D6B4 = {
    {
        func_acropolis_square_80182260,
        func_acropolis_square_801822A4,
        taskKill,
    },
};

s32 func_acropolis_square_80181794(Task* task, s32 msgId, RoomEventMsg* arg2, RoomEventMsg* arg3)
{
    GpAreaKey key; // filled in but never used: the Gp_SetAreaObjId call the
                   // sibling rooms make with it is absent here
    u16 temp_s1;

    key.stage = 1;
    key.area  = 4;
    *arg3     = *arg2;
    if (arg2->msgId == 9) {
        if ((D_acropolis_square_8018382C != 0) && (arg2->field_5 == 0)) {
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 2);
        }
        if (arg2->msgId == 9) {
            if (GameFlag_GetNibble(9) & 1) {
                arg3->field_3 = 2;
            }
        }
        return 1;
    }
    if (arg2->msgId == 2) {
        if ((D_acropolis_square_8018382C != 0) && (arg2->field_5 == 0)) {
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 2);
        }
        if (GameFlag_GetNibble(0) < 2) {
            return 1;
        }
        if ((GameFlag_GetNibble(0) == 2) || (GameFlag_GetNibble(0) >= 3)) {
            if (arg2->field_5 == 0) {
                do {
                    Gp_SetNibbleIf(arg2->field_6, 2);
                    Gp_RunCapCmd1(1);
                } while (0);
            }
            return 0;
        }
    }
    if (arg2->msgId == 0x11) {
        if (GameFlag_GetNibble(0) < 2) {
            return 1;
        }
        if ((GameFlag_GetNibble(0) == 2) || (GameFlag_GetNibble(0) >= 3)) {
            if (arg2->field_5 == 0) {
                do {
                    Gp_SetNibbleIf(arg2->field_6, 2);
                    Gp_RunCapCmd1(1);
                } while (0);
            }
            return 0;
        }
    }
    temp_s1 = arg2->msgId;
    if (temp_s1 == 3) {
        if (arg2->field_5 == 0) {
            if (GameFlag_GetNibble(0) < 2) {
                if (GameFlag_GetNibble(0x21) < 2) {
                    arg3->field_3 = 1;
                } else {
                    arg3->field_3 = 2;
                }
            } else {
                arg3->field_3 = temp_s1;
            }
        }
    }
    return 1;
}
s32 func_acropolis_square_801819BC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    s32 var_a0;

    if (arg2 == 2) {
        if (Mc_SaveData.at4.loc.warp == 7) {
            Mc_SaveData.at4.loc.warp = 1;
        }
        D_acropolis_square_801888AC.field_0  = 9;
        D_acropolis_square_801888AC.field_1  = 1;
        D_acropolis_square_801888AC.field_3  = 1;
        D_acropolis_square_801888AC.field_4  = 0x51010001;
        D_acropolis_square_801888AC.field_8  = 0x51010007;
        D_acropolis_square_801888AC.field_10 = 0x51010006;
        D_acropolis_square_801888AC.field_C  = 0x5101000B;
        D_acropolis_square_801888AC.field_2  = D_acropolis_square_8018382C;
        D_acropolis_square_8018382C          = 0;
        Task_SpawnFromTable(&D_acropolis_square_801837A0, 0, 2, (s32)&D_acropolis_square_801888AC);
    }
    if ((arg2 == 0xE) && (GameFlag_GetNibble(0x124) == 0)) {
        GameFlag_SetNibble(0x124, 1);
        Gp_SpawnIfCapIdle(0xE, 1);
    }
    if ((arg2 == 0x10) && (GameFlag_GetNibble(0x156) == 0)) {
        GameFlag_SetNibble(0x156, 1);
        var_a0 = 0x11;
        if (D_80072310 != 1) {
            var_a0 = 0x10;
        }
        Gp_SpawnIfCapIdle(var_a0, 1);
    }
    return 0;
}
/// Siren task for the square. States 0-2 arm the scene and tick, 3 fires the
/// first siren blast, 4 repeats it every 0x79 frames until the player answers,
/// and 5 waits for the scripted phase to advance before handing the scene off
/// to the slot-5 task and killing itself.
void func_acropolis_square_80181AEC(Task* task)
{
    s32 pan;
    s32 pan2;
    s32 pan3;
    s32 count;
    s32 count2;
    u32 state;

    state = task->state;
    switch (state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            D_acropolis_square_8018382C = 1;
            D_acropolis_square_80188898 = 0;
            func_800E8634((s32)&D_acropolis_square_80183834, 0, (s32)&D_acropolis_square_801838DC);
            goto advance;

        case 3:
            D_acropolis_square_801888CC.coord.t[0] = 0x19AA;
            D_acropolis_square_801888CC.coord.t[1] = -0xF96;
            D_acropolis_square_801888CC.coord.t[2] = 0x8DE;
            D_acropolis_square_801888CC.sub        = &gGfxViewCoord;
            Gp_UpdateCoord(&D_acropolis_square_801888CC);
            pan = Gp_GetObjPan(&D_acropolis_square_801888CC);
            SndEvt_EnqueueType6(
                0x51010009, (s8)pan, (s8)gpGetObjDepth(&D_acropolis_square_801888CC));
            goto advance;

        case 4:
            count                       = D_acropolis_square_80188898 + 1;
            D_acropolis_square_80188898 = count;
            if (count >= 0x79) {
                D_acropolis_square_801888CC.coord.t[0] = 0x19AA;
                D_acropolis_square_801888CC.coord.t[1] = -0xF96;
                D_acropolis_square_801888CC.coord.t[2] = 0x8DE;
                D_acropolis_square_80188898            = 0;
                D_acropolis_square_801888CC.sub        = &gGfxViewCoord;
                Gp_UpdateCoord(&D_acropolis_square_801888CC);
                pan2 = Gp_GetObjPan(&D_acropolis_square_801888CC);
                SndEvt_EnqueueType6(0x51010009, (s8)pan2,
                                    (s8)gpGetObjDepth(&D_acropolis_square_801888CC));
            }
            if (gGameSession->eventState != 0) {
                return;
            }
            Gp_MsgPlayerWeapon(1);
            /* fallthrough */

        case 1:
        case 2:
        advance:
            task->state += 1;
            return;

        case 5:
            if ((u32)(Mc_SaveData.at4.loc.view - 5) >= 3U) {
                if (Mc_SaveData.at4.loc.view == 9) {
                    goto checkArmed;
                }
                goto handOff;
            }
        checkArmed:
            if (D_acropolis_square_8018382C == 0) {
            handOff:
                if (D_acropolis_square_8018382C != 0) {
                    GameFlag_SetNibble(3, 0);
                    GameFlag_SetNibble(0x155, 2);
                    D_acropolis_square_8018382C = 0;
                }
                Gp_DispatchMsg(gameGetPtrSlot(5), 0xC1F, 0, 0);
                SndEvt_EnqueueType7(0x51010009, 1);
                taskKill(task);
                return;
            }
            count2                      = D_acropolis_square_80188898 + 1;
            D_acropolis_square_80188898 = count2;
            if (count2 >= 0x79) {
                D_acropolis_square_801888CC.coord.t[0] = 0x19AA;
                D_acropolis_square_801888CC.coord.t[1] = -0xF96;
                D_acropolis_square_801888CC.coord.t[2] = 0x8DE;
                D_acropolis_square_80188898            = 0;
                D_acropolis_square_801888CC.sub        = &gGfxViewCoord;
                Gp_UpdateCoord(&D_acropolis_square_801888CC);
                pan3 = Gp_GetObjPan(&D_acropolis_square_801888CC);
                SndEvt_EnqueueType6(0x51010009, (s8)pan3,
                                    (s8)gpGetObjDepth(&D_acropolis_square_801888CC));
            }
            break;
    }
}
/// Scrolling backdrop task: three 256x240 sprite strips (the last one half
/// width) tiled across the screen from `D_acropolis_square_801888A0`, each with
/// its own texture page. States 0-3 slide the strip in and hold it for a while,
/// state 4 kills the task; every state still draws.
void func_acropolis_square_80181DD0(Task* task)
{
    SPRT*     p;
    DR_TPAGE* dr;
    s32       x;
    s32       i;
    s32       tpageX;
    s32       count;
    s32       count2;
    s32       pos;

    switch (task->state) {
        case 0:
            D_acropolis_square_801888A0 = -0x140;
            D_acropolis_square_801888A4 = 0;
            task->state                += 1;
            break;

        case 1:
            count                       = D_acropolis_square_801888A4 + 1;
            D_acropolis_square_801888A4 = count;
            if (count >= 0x2E) {
                task->state += 1;
            }
            break;

        case 2:
            pos                         = D_acropolis_square_801888A0 + 1;
            D_acropolis_square_801888A0 = pos;
            if (pos >= 0) {
                D_acropolis_square_801888A4 = 0;
                task->state                += 1;
            }
            break;

        case 3:
            count2                      = D_acropolis_square_801888A4 + 1;
            D_acropolis_square_801888A4 = count2;
            if (count2 >= 0x1F) {
                task->state += 1;
            }
            break;

        case 4:
            Mc_SaveData.at4.loc.view = 0xD;
            taskKill(task);
            break;
    }

    x = D_acropolis_square_801888A0;
    for (i = 0; i < 3; i++) {
        tpageX         = 0x1C0 + i * 0x80;
        p              = (SPRT*)gGpuPrimCursor;
        gGpuPrimCursor = p + 1;
        setlen(p, 4);
        setcode(p, 0x65);
        p->x0 = x - 0xA0;
        p->y0 = -0x78;
        p->u0 = 0;
        p->v0 = 0;
        if (i == 2) {
            p->w = 0x80;
            p->h = 0xF0;
        } else {
            p->w = 0x100;
            p->h = 0xF0;
        }
        p->clut = GetClut(0, 0xFF);
        addPrim(&gGpuCurrentOt[4], p);

        dr             = gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        setDrawTPage(dr, 0, 1, GetTPage(1, 0, tpageX, 0x100));
        addPrim(&gGpuCurrentOt[4], dr);

        x += 0x100;
    }
}

/// Sound task: plays the sound event `spawnArg2` on its first tick and again
/// at tick 0x50, then kills itself at tick 0x78.
void func_acropolis_square_80182048(Task* task)
{
    s32 zero;

    switch (task->state) {
        case 0x50:
        case 0x0:
            zero = 0;
            TOUCH_REG(zero);
            SndEvt_EnqueueType6((s32)task->spawnArg2, zero, zero);
            break;
        case 0x78:
            Task_RequestKill(task, 0);
            return;
    }
    task->state += 1;
}

s32 func_acropolis_square_801820D8(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    if (arg2->field_2 == 0) {
        Gp_SpawnIfCapIdle(5, 0);
    }
    return 0;
}

/// Always returns 0.
s32 func_acropolis_square_80182108(void)
{
    return 0;
}

s32 func_acropolis_square_80182110(s32 arg0, s32 arg1, s32 arg2)
{
    SndEvt_EnqueueType6(D_acropolis_square_80183B34[arg2], 0, 0);
    return 0;
}

void func_acropolis_square_80182148(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_RunCapCmd1(5);
            /* Keeps this arm from being cross-jumped into the identical
               `case 3` arm; emits nothing. */
            SOFT_BARRIER();
            goto advance;
        case 1:
            Mc_SaveData.at4.loc.view = 7;
            goto advance;
        case 3:
            Gp_RunCapCmd1(5);
            goto advance;
        case 6:
            Gp_RunCapCmd1(5);
            Mc_SaveData.at4.loc.view = 8;
            /* fallthrough */
        case 4:
        case 5:
        advance:
            task->state++;
            return;
        case 2:
        case 7:
            GameFlag_SetNibble(0x15, 1);
            taskKill(task);
            return;
    }
}

void func_acropolis_square_80182200(s32 arg0)
{
    switch (arg0) { /* irregular */
        case 0:
            D_acropolis_square_8018889C = Task_SpawnFromTable(&D_acropolis_square_80183808, 2, 0, 0);
            return;
        case 1:
            taskKill(D_acropolis_square_8018889C);
            return;
    }
}

/// First state of the room entry task: installs the room's message table,
/// publishes the task in pointer slot 7 and advances the state.
void func_acropolis_square_80182260(Task* task)
{
    task->msgTable = D_acropolis_square_801837C4;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

void func_acropolis_square_801822A4(Task* task)
{
    char pad[0x10];

    if (Mc_SaveData.at4.loc.warp == 7 && D_acropolis_square_80183830 == 0) {
        D_acropolis_square_80183830 = 1;
        Mc_SaveData.sceneEvent      = 2;
        func_800E8634((s32)&D_acropolis_square_8018399C, 0, (s32)&D_acropolis_square_80183A5C);
    }
}

/// Room entry task: runs the state handler `D_acropolis_square_8017D6B4`
/// names for `Task::state`, through a copy of the table taken onto the stack.
void func_acropolis_square_80182308(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_square_8017D6B4;
    sp.funcs[task->state](task);
}

s32 func_acropolis_square_80182360(void)
{
    GpAreaKey key;

    if (GameFlag_GetNibble(0x1F) == 0) {
        GameFlag_SetNibble(0x1F, 1);
        key.stage = 1;
        key.area  = 1;
        Gp_SetAreaObjId(&key, 2, 1);
        gGameSession->eventState = 1;
        Task_SpawnFromTable(&D_acropolis_square_80183808, 0, 0, 0);
        return 0;
    }
    return 1;
}

void func_acropolis_square_801823DC(Task* task)
{
    GpEffWork* work;
    GpCoord*   coord;

    coord = task->extra.tmd->coords;
    work  = task->spawnArg2;
    switch (task->state) { /* irregular */
        case 0:
            task->msgTable = &D_acropolis_square_80183B58;
            Game_SetPtrSlot(task, 5);
            D_acropolis_square_80183B98 = 0;
            Task_Spawn(1, 0x25, 0, 0);
            Task_Spawn(1, 0x25, 1, 0);
            task->state++;
            return;
        case 1:
            if ((0x268 >> ((u8)gGameSession->at4.loc.view - 1)) & 1) {
                work->move.vx = 0x19AA;
                work->move.vy = -0xF96;
                work->move.vz = 0x8DE;
                Gp_SpawnEff(0x60047, coord, D_acropolis_square_80183B98 * 0x10000218 + 0x10E08,
                            &work->move);
            }
            if ((u8)gGameSession->at4.loc.view == 0xE) {
                work->move.vx = 0x18D2;
                work->move.vy = -0x100B;
                work->move.vz = 0x8AB;
                Gp_SpawnEff(0x60047, coord, D_acropolis_square_80183B98 * 0x218 + 0x10010608,
                            &work->move);
            }
            if ((u8)gGameSession->at4.loc.view == 9) {
                work->move.vx = 0x19AA;
                work->move.vy = -0xF96;
                work->move.vz = 0x8E8;
                Gp_SpawnEff(0x60047, coord, D_acropolis_square_80183B98 * 0x118 + 0x80010308,
                            &work->move);
            }
            return;
    }
}

void func_acropolis_square_801825DC(Task* task)
{
    u8*              head;
    u8*              raw;
    RoomGlowScratch* blk;
    POLY_G4*         prim;
    LINE_G3*         line;
    GpCoord*         coord;
    void*            mem;
    u16              vz;
    s32              i;
    s32              pulse;
    s32              level;
    s32              height;
    s16              amp;
    s16              flip;
    s32              ampSi;
    s32              ampHalf;
    u8               red;
    u8               cyan;
    s32              z;
    s32              shift;
    u32              depth;
    u32              tag;
    u_long*          ot;

    coord = task->extra.tmd->coords;
    mem   = task->spawnArg2;
    Gp_UpdateCoord(coord);
    head = SCRATCH_HEAD(void);
    raw  = head - 0x18;
    SOFT_TOUCH_REG(raw);
    blk                = (RoomGlowScratch*)raw;
    blk->vec.vx        = *(u16*)&coord->workm.t[0];
    blk->vec.vy        = *(u16*)&coord->workm.t[1];
    vz                 = *(u16*)&coord->workm.t[2];
    SCRATCH_HEAD(void) = blk;
    blk->vec.vz        = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomGlowScratch*)(head - 0x18))->vec);
    gte_rtps();
    gte_stsxy(&((RoomGlowScratch*)(head - 0x18))->sx);
    gte_stszotz(&blk->otz);
    if (((RoomGlowScratch*)(head - 0x18))->otz >= 0x11) {
        pulse  = D_80070F70;
        pulse *= task->spawnArg1 & 0xFF;
        flip   = (task->spawnArg1 >> 16) & 1;
        if (pulse & 0x80) {
            level = ~pulse & 0x7F;
        } else {
            level = pulse & 0x7F;
        }
        amp   = level * 2;
        level = task->spawnArg1;
        if (level < 0) {
            height      = (level >> 8) & 0xFF;
            blk->rOuter = (height * 0x600) / blk->otz;
            blk->rInner = (height * 0xC0) / blk->otz;
            for (i = 0; i < 0x10; i += 2) {
                ampSi          = amp;
                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, (ampSi * (flip ^ 1)) >> 1, (flip * ampSi) >> 1, (flip * ampSi) >> 1);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->rOuter * D_acropolis_square_80183B68[i + 4]) >> 12);
                prim->y0 = blk->sy + ((blk->rOuter * D_acropolis_square_80183B68[i]) >> 12);
                prim->x1 = blk->sx + ((blk->rOuter * D_acropolis_square_80183B68[i + 5]) >> 12);
                prim->y1 = blk->sy + ((blk->rOuter * D_acropolis_square_80183B68[i + 1]) >> 12);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                prim->x3 = blk->sx + ((blk->rOuter * D_acropolis_square_80183B68[i + 6]) >> 12);
                prim->y3 = blk->sy + ((blk->rOuter * D_acropolis_square_80183B68[i + 2]) >> 12);
                addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);

                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, ampSi * (flip ^ 1), flip * ampSi, flip * ampSi);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->rOuter * D_acropolis_square_80183B68[i + 4]) >> 13);
                prim->y0 = blk->sy + ((blk->rOuter * D_acropolis_square_80183B68[i]) >> 13);
                prim->x1 = blk->sx + ((blk->rOuter * D_acropolis_square_80183B68[i + 5]) >> 13);
                prim->y1 = blk->sy + ((blk->rOuter * D_acropolis_square_80183B68[i + 1]) >> 13);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                prim->x3 = blk->sx + ((blk->rOuter * D_acropolis_square_80183B68[i + 6]) >> 13);
                prim->y3 = blk->sy + ((blk->rOuter * D_acropolis_square_80183B68[i + 2]) >> 13);
                addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
            }
            ampHalf = amp >> 1;
            for (i = 2; i < 0x10; i += 8) {
                do {
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    red  = ampHalf * (flip ^ 1);
                    cyan = flip * ampHalf;
                    setRGB2(prim, red, cyan, cyan);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = blk->sx + ((blk->rInner * D_acropolis_square_80183B68[i]) >> 12);
                    prim->y0 = blk->sy + ((blk->rInner * D_acropolis_square_80183B68[i - 4]) >> 12);
                    prim->x1 = blk->sx + ((blk->rOuter * D_acropolis_square_80183B68[i + 4]) >> 11);
                    prim->y1 = blk->sy + ((blk->rOuter * D_acropolis_square_80183B68[i]) >> 11);
                    prim->x2 = blk->sx;
                    prim->y2 = blk->sy;
                    prim->x3 = blk->sx + ((blk->rInner * D_acropolis_square_80183B68[i + 8]) >> 12);
                    prim->y3 = blk->sy + ((blk->rInner * D_acropolis_square_80183B68[i + 4]) >> 12);
                    shift    = gDisplayState.otDepthShift;
                    depth    = (((u32)blk->otz << shift) >> 2) & 0xFFC;
                    __asm__("" : "+r"(depth) : "r"(shift), "m"(gDisplayState.otDepthShift));
                    setaddr(prim, getaddr((u_long*)(depth + (s32)gGpuCurrentOt)));
                    ot  = (u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt);
                    tag = (*ot & 0xFF000000) | ((u32)prim & 0xFFFFFF);
                    *ot = tag;
                    z   = blk->otz;
                    SOFT_TOUCH_REG(z);
                    SOFT_TOUCH_REG(z);
                    SOFT_TOUCH_REG_USE(z, tag);
                    SOFT_TOUCH_REG_USE(prim, z);
                    Gp_AddTpageShift((P_TAG*)prim, 1, z);

                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, red, cyan, cyan);
                } while (0);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->rInner * D_acropolis_square_80183B68[i + 4]) >> 13);
                prim->y0 = blk->sy + ((blk->rInner * D_acropolis_square_80183B68[i]) >> 13);
                prim->x1 = blk->sx + ((blk->rOuter * D_acropolis_square_80183B68[i + 8]) >> 12);
                prim->y1 = blk->sy + ((blk->rOuter * D_acropolis_square_80183B68[i + 4]) >> 12);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                prim->x3 = blk->sx + ((blk->rInner * D_acropolis_square_80183B68[i + 0xC]) >> 13);
                prim->y3 = blk->sy + ((blk->rInner * D_acropolis_square_80183B68[i + 8]) >> 13);
                addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        prim);
                z = blk->otz;
                __asm__("" : "+r"(z) : "r"(red), "r"(&D_acropolis_square_80183B68[i]));
                __asm__("" : "+r"(prim) : "r"(z), "r"(cyan));
                Gp_AddTpageShift((P_TAG*)prim, 1, z);
            }
        } else {
            blk->rOuter = (((level >> 8) & 0xFF) << 9) / blk->otz;
            for (i = 0; i < 2; i++) {
                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, amp * (flip ^ 1), flip * amp, flip * amp);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx - blk->rOuter;
                prim->x1 = prim->x2 = blk->sx;
                prim->x3            = blk->sx + blk->rOuter;
                prim->y0 = prim->y2 = prim->y3 = blk->sy;
                prim->y1                       = (blk->sy - blk->rOuter) + blk->rOuter * (i + i);
                addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
            }
            if (task->spawnArg1 & 0x10000000) {
                for (i = 0; i < 2; i++) {
                    line           = (LINE_G3*)gGpuPrimCursor;
                    gGpuPrimCursor = line + 1;
                    setLineG3(line);
                    setRGB0(line, 0, 0, 0);
                    setRGB1(line, amp * (flip ^ 1), flip * amp, flip * amp);
                    setRGB2(line, 0, 0, 0);
                    line->x0 = blk->sx + blk->rOuter * (i * 3 - 1);
                    line->y0 = blk->sy - blk->rOuter * (i + 1);
                    line->x1 = blk->sx;
                    line->y1 = blk->sy;
                    line->x2 = blk->sx - blk->rOuter * (i * 3 - 1);
                    line->y2 = blk->sy + blk->rOuter * (i + 1);
                    addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                      (s32)gGpuCurrentOt),
                            line);
                    Gp_AddTpageShift((P_TAG*)line, 1, blk->otz);
                }
            }
        }
    }
    SCRATCH_POP_BYTES(0x18);
    Gp_ReleaseState1CMem(mem, task);
}

s32 func_acropolis_square_8018344C(s32 arg0, s32 arg1, s32 arg2)
{
    D_acropolis_square_80183B98 = arg2;
    return 0;
}

void func_acropolis_square_8018345C(void)
{
}
