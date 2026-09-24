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
#include "main/sound.h"
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

/// Record the destination resolver `func_neo_ark_observatory_8017F44C` reads:
/// `field_0` is the destination area and `field_5` must be 0 for it to act.
typedef struct MapMarkerRec {
    u16  field_0;
    byte pad_2[3];
    u8   field_5;
} MapMarkerRec;

/// Record the destination resolver writes: `field_3` is the destination room.
/// The caller passes the same buffer as the `MapMarkerRec`, so it overwrites
/// the room byte the caller staged after the area and warp.
typedef struct MapMarkerOut {
    byte pad_0[3];
    s8   field_3;
} MapMarkerOut;

/// Departure staged by the message handler `func_neo_ark_observatory_8017F6F8`
/// and carried out by the departure task `func_neo_ark_observatory_8017F588`.
/// `field_0`..`field_3` are the stage, area, warp and room the task commits to
/// the save's location. `field_4` is the halfword the task sends to the player
/// as message 0x3EE, where all ones sends nothing; `field_8` is the sound event
/// it plays and waits out, where 0 plays none.
typedef struct NeoArkObservatoryEventDesc {
    u8   field_0;
    u8   field_1;
    u8   field_2;
    u8   field_3;
    u16  field_4;
    byte pad_6[0x2];
    s32  field_8;
} NeoArkObservatoryEventDesc;
STATIC_ASSERT_SIZEOF(NeoArkObservatoryEventDesc, 0xC);

/// The save's location key, read byte-wise for its view and as one word to
/// test its view and area together.
typedef union _NeoArkObservatorySaveLoc {
    GpAreaKey key;
    s32       head;
} _NeoArkObservatorySaveLoc;

/// Scratchpad block the mirror takes while it rebuilds its coordinate frame.
/// `viewRow` is the view matrix's second row, negated through the GTE for the
/// floor mirror. The other mirrors reflect through a plane: `normal` is the
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

/// Scratch block one band segment is projected in: the four corners in world
/// space, the GTE depth and flag, and the projected corners.
typedef struct _NeoArkObservatoryBandScratch {
    SVECTOR v[4];
    s32     otz;
    s32     flag;
    DVECTOR sxy[4];
} _NeoArkObservatoryBandScratch;

/// Twelve opaque bytes the mesh copy carries across unchanged.
typedef struct _NeoArkObservatoryBlk12 {
    u8 data[12];
} _NeoArkObservatoryBlk12;

/// A small mesh as a pointer table: `field_4` is rotated without translation,
/// `field_8` rotated and translated, and `field_C` copied through. The room
/// keeps a pristine source and a working copy that is rebuilt from it.
typedef struct _NeoArkObservatoryMesh {
    s32                      field_0;
    SVECTOR*                 field_4;
    SVECTOR*                 field_8;
    _NeoArkObservatoryBlk12* field_C;
} _NeoArkObservatoryMesh;

/// A destination resolver: reads the area from its first record and writes the
/// room through the second.
typedef s32 (*_MapMarkerResolve)(MapMarkerRec*, MapMarkerOut*);

extern s16                       D_80071076;
extern _NeoArkObservatorySaveLoc D_8007216C;
extern s8                        D_8007272D;
extern TaskDesc                  D_80137EE4;
extern TaskDesc                  D_80138694;
extern TaskDesc                  D_8013C72C;
extern TaskDesc                  D_8013CAEC;
extern TaskDesc                  D_8013FC58;
extern TaskDesc                  D_80140078;

extern void func_80132220(void);
extern void func_801322F8(void);
extern void func_80179B14(RoomEventMsg* in, RoomEventMsg* out);

/// Index of the mirrored player's coordinate part each held-object reflection
/// is parented to, by `Task::spawnArg1`.
extern u8 D_neo_ark_observatory_80180DB8[];

/// The mirror's task descriptors: entry 0 runs the mirror task itself, entry 1
/// the held-object reflections it spawns.
extern TaskDesc D_neo_ark_observatory_80180DBC[];

/// The departure task's descriptor.
extern TaskDesc D_neo_ark_observatory_80180DD4;

extern TaskDesc D_neo_ark_observatory_80181200;
extern TaskDesc D_neo_ark_observatory_801812C0;

/// Descriptor of the cap-file task `func_neo_ark_observatory_8017FB1C`.
extern TaskDesc D_neo_ark_observatory_801811AC;

/// Messages the room task answers, terminated by id 0x7FFFFFFF.
extern GpMsgEntry D_neo_ark_observatory_801811B8[];

/// Offset `func_neo_ark_observatory_8017FA98` hands the mesh rebuild; only its
/// `vy` is ever set.
extern SVECTOR D_neo_ark_observatory_80181368;

extern _NeoArkObservatoryMesh D_neo_ark_observatory_80181410;
extern _NeoArkObservatoryMesh D_neo_ark_observatory_80181FA4;
extern SVECTOR                D_neo_ark_observatory_80181434[];
extern SVECTOR                D_neo_ark_observatory_801814E4[];
extern SVECTOR                D_neo_ark_observatory_801814F4[];
extern SVECTOR                D_neo_ark_observatory_801814FC[];
extern SVECTOR                D_neo_ark_observatory_8018150C[];
extern SVECTOR                D_neo_ark_observatory_8018151C[];
extern SVECTOR                D_neo_ark_observatory_80181524[];
extern SVECTOR                D_neo_ark_observatory_80181564[];
extern SVECTOR                D_neo_ark_observatory_80181574[];
extern SVECTOR                D_neo_ark_observatory_8018157C[];

extern GpAreaApplyRec             D_neo_ark_observatory_80187A28;
extern NeoArkObservatoryEventDesc D_neo_ark_observatory_80187A30;
extern s16                        D_neo_ark_observatory_80187A3C;

void func_neo_ark_observatory_8017D8A8(Task* task);
s32  func_neo_ark_observatory_8017F44C(MapMarkerRec* arg0, MapMarkerOut* arg1);
void func_neo_ark_observatory_8017FE34(GsCOORDINATE2* coord, SVECTOR* offset);
void func_neo_ark_observatory_80180534(SVECTOR* v, s32 arg1, s16 arg2, s16 arg3);
void func_neo_ark_observatory_80180A0C(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_neo_ark_observatory_80180DAC(s16 arg0);

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
/// (`GameActor::field_920` / `field_924`). Runs the first per-frame update
/// before returning.
void func_neo_ark_observatory_8017D6F4(Task* task)
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
            spawned = Task_SpawnFromTable(D_neo_ark_observatory_80180DBC, 1, i, (s32)task);
            if (spawned != NULL) {
                Task_Reparent(child, spawned);
            }
        }
    }
    func_neo_ark_observatory_8017D8A8(task);
}

/// Per-frame update of the room's mirror, the task
/// `func_neo_ark_observatory_8017D6F4` sets up.
///
/// When the player's equipped weapon changes it spawns reflection tasks for
/// the player's two held-object tasks. When the view moves it rebuilds the
/// reflection's coordinate frame. Mirror 0 copies the view matrix with its
/// second row negated and applies location-specific corrections. The other
/// mirror reflects through a plane chosen by the current stage, area and view.
/// On the frame after mirror 0 rebuilds, it queues packets that copy the frame
/// buffer into the off-screen strip at x = `width`. In stages 1 and 5 it
/// projects the reflected body to find its screen rectangle and, where that
/// overlaps the mirror's clip rectangle, draws quads sampling that strip. Otherwise the reflection is hidden. Every
/// frame it copies the player's pose and light matrices onto the reflection.
void func_neo_ark_observatory_8017D8A8(Task* task)
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
                spawned = Task_SpawnFromTable(D_neo_ark_observatory_80180DBC, 1, i + 2, (s32)task);
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
            if ((D_8007216C.head & 0xFF00FF) == 0x20005) {
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

/// Scale applied to reflections with `spawnArg1 >= 2`: X negated, Y and Z kept.
const VECTOR D_neo_ark_observatory_8017D5C4 = { -0x1000, 0x1000, 0x1000 };

/// Per-frame callback of a held-object reflection. `Task::spawnArg2` is the
/// mirror task the room set up and the parent is the held-object task being
/// reflected. On the first frame it clones the parent's TMD source, parents the
/// clone's root coordinate to the mirrored player's corresponding part, points
/// the clone at the mirror's light and color matrices and negates the X
/// translation; every frame it republishes the mirror model's draw flags onto
/// the clone.
void func_neo_ark_observatory_8017F22C(Task* task)
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
    mirrorPart  = &((TmdObject*)mirror->extra)->coords[D_neo_ark_observatory_80180DB8[task->spawnArg1]];
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
            scale = D_neo_ark_observatory_8017D5C4;
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

/// Mirror task: runs the set-up state, then the per-frame state.
void func_neo_ark_observatory_8017F3FC(Task* task)
{
    TaskFunc states[2] = {
        func_neo_ark_observatory_8017D6F4,
        func_neo_ark_observatory_8017D8A8,
    };

    states[task->state](task);
}

/// Picks the room a departure into area `arg0->field_0` lands in, for the
/// areas whose room depends on story progress: areas 5, 41 and 45 take a
/// flag nibble plus one, areas 2, 16 and 20 map flag nibbles to fixed rooms.
/// Every other area leaves `arg1->field_3` unchanged. Nothing is done unless
/// `arg0->field_5` is 0.
s32 func_neo_ark_observatory_8017F44C(MapMarkerRec* arg0, MapMarkerOut* arg1)
{
    if (arg0->field_5 == 0) {
        switch (arg0->field_0) {
            case 2:
                if (GameFlag_GetNibble(0x10F) != 0) {
                    arg1->field_3 = 2;
                }
                if (GameFlag_GetNibble(0x11A) >= 2) {
                    arg1->field_3 = 3;
                }
                break;
            case 5:
                arg1->field_3 = GameFlag_GetNibble(0xA4) + 1;
                break;
            case 16:
                if (GameFlag_GetNibble(0x7A) >= 6) {
                    arg1->field_3 = 3;
                }
                break;
            case 20:
                switch (GameFlag_GetNibble(0xF4)) {
                    case 0:
                        arg1->field_3 = 1;
                        break;
                    case 1:
                        arg1->field_3 = 6;
                        break;
                    case 2:
                        arg1->field_3 = 7;
                        break;
                    case 3:
                        arg1->field_3 = 8;
                        break;
                    default:
                        arg1->field_3 = 1;
                        break;
                }
                break;
            case 45:
                arg1->field_3 = GameFlag_GetNibble(0xB7) + 1;
                break;
            case 41:
                arg1->field_3 = GameFlag_GetNibble(0xB6) + 1;
                break;
            case 3:
            case 4:
            case 6:
            case 7:
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
            case 17:
            case 18:
            case 19:
            case 21:
            case 22:
            case 23:
            case 24:
            case 25:
            case 26:
            case 27:
            case 28:
            case 29:
            case 30:
            case 31:
            case 32:
            case 33:
            case 34:
            case 35:
            case 36:
            case 37:
            case 38:
            case 39:
            case 40:
            case 42:
            case 43:
            case 44:
            default:
                break;
        }
    }
    return 1;
}

/// Departure task. State 0 stages the descriptor's halfword into a `GpMsg3EE`
/// record and sends it to the slot-3 game pointer as message 0x3EE - the
/// all-ones halfword is the "nothing staged" marker, and the task skips to
/// state 2 rather than sending it. State 1 polls that same pointer with 0x3F0,
/// states 2 and 3 queue the descriptor's sound event and wait for the voice to
/// go quiet, and each of them advances the state once its call reports 0.
/// State 4 commits the save location the descriptor names, re-spawns the
/// player task as type 0x11 and kills itself.
void func_neo_ark_observatory_8017F588(Task* arg0)
{
    GpMsg3EE msg;
    void*    slot;

    slot = gameGetPtrSlot(3);
    switch (arg0->state) {
        case 0:
            msg.field_12 = D_neo_ark_observatory_80187A30.field_4;
            if (msg.field_12 == -1) {
                arg0->state = 2;
                break;
            }
            Gp_DispatchMsg(slot, 0x3EE, (s32)&msg, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 1:
            if (Gp_DispatchMsg(slot, 0x3F0, 0, 0) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 2:
            if (D_neo_ark_observatory_80187A30.field_8 == 0) {
                arg0->state = 4;
                break;
            }
            SndEvt_EnqueueType6(D_neo_ark_observatory_80187A30.field_8, 0, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 3:
            if (SndVoice_HasActiveId(D_neo_ark_observatory_80187A30.field_8) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 4:
            SndEvt_EnqueueType7((s32)0x80000000, 0);
            D_80071076                = 1;
            Mc_SaveData.at4.loc.stage = D_neo_ark_observatory_80187A30.field_0;
            Mc_SaveData.at4.loc.area  = D_neo_ark_observatory_80187A30.field_1;
            Mc_SaveData.at4.loc.warp  = D_neo_ark_observatory_80187A30.field_2;
            Mc_SaveData.at4.loc.room  = D_neo_ark_observatory_80187A30.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
        default:
            break;
    }
}

/// Copies the area, warp and room of `desc` into a resolver record, lets
/// `resolve` rewrite the record in place, and copies the result back.
static __inline__ void _neoArkObservatoryStageMarker(NeoArkObservatoryEventDesc* desc, _MapMarkerResolve resolve)
{
    MapMarkerRec rec;

    rec.field_0  = desc->field_1;
    rec.pad_2[0] = desc->field_2;
    rec.pad_2[1] = desc->field_3;
    rec.field_5  = 0;
    resolve(&rec, (MapMarkerOut*)&rec);
    desc->field_1 = rec.field_0;
    desc->field_2 = rec.pad_2[0];
    desc->field_3 = rec.pad_2[1];
}

s32 func_neo_ark_observatory_8017F6F8(Task* arg0, s32 arg1, GpMsg13EF* arg2, s32 arg3)
{
    NeoArkObservatoryEventDesc desc;
    _MapMarkerResolve          resolve;
    s32                        temp;

    if (arg2->field_2 == 0xA) {
        if (GameFlag_GetNibble(0xD1) == 2) {
            GameFlag_SetNibble(0x4C, 8);
        }
        if (GameFlag_GetNibble(0xF7) == 0) {
            temp = GameFlag_GetNibble(0xDF);
            if (temp == 1) {
                _MapMarkerResolve resolve;

                GameFlag_SetNibble(0xF7, 1);
                desc.field_0 = 4;
                desc.field_1 = 0x12;
                desc.field_2 = 3;
                desc.field_3 = temp;
                desc.field_8 = 0x55070005;
                desc.field_4 = 0x400;
                resolve      = func_neo_ark_observatory_8017F44C;
                Gp_MsgPlayerWeapon(0);
                _neoArkObservatoryStageMarker(&desc, resolve);
                D_neo_ark_observatory_80187A30 = desc;
                Task_SpawnFromTable(&D_neo_ark_observatory_80180DD4, 0, 0, 0);
                return 0;
            }
        }
        desc.field_0 = 4;
        desc.field_1 = arg2->field_3;
        desc.field_3 = 1;
        desc.field_2 = 4;
        desc.field_8 = 0x55070005;
        desc.field_4 = 0x400;
        resolve      = func_neo_ark_observatory_8017F44C;
        Gp_MsgPlayerWeapon(0);
        _neoArkObservatoryStageMarker(&desc, resolve);
        D_neo_ark_observatory_80187A30 = desc;
        Task_SpawnFromTable(&D_neo_ark_observatory_80180DD4, 0, 0, 0);
    }
    if (arg2->field_2 == 1 && GameFlag_GetNibble(0xD7) == 0) {
        GameFlag_SetNibble(0xD7, 1);
        if (GameFlag_GetNibble(0x83) != 0) {
            func_800E3FAC(0xA2, 0x2C);
            func_800E8634((s32)&D_8013C72C, 0, (s32)&D_8013CAEC);
        } else {
            func_800E3FAC(0xA2, 0x2D);
            GameFlag_SetNibble(0xD1, 3);
            func_800E8634((s32)&D_80137EE4, 0, (s32)&D_80138694);
        }
    }
    if (arg2->field_2 == 2) {
        if (GameFlag_GetNibble(0xE1) == 0) {
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 6);
            GameFlag_SetNibble(0xE1, 1);
            D_8007272D = 0x15;
            func_800E8634((s32)&D_8013FC58, 0, (s32)&D_80140078);
        }
    }
    if (arg2->field_2 == 3 && gameGetPtrSlot(0xA) != NULL && gGameSession->at4.loc.view == 2) {
        func_80132220();
    }
    if (arg2->field_2 == 4 && GameFlag_GetNibble(0xDE) != 0 && GameFlag_GetNibble(0x16E) == 0) {
        GameFlag_SetNibble(0x16E, 1);
        func_800E8634((s32)&D_neo_ark_observatory_80181200, 0, (s32)&D_neo_ark_observatory_801812C0);
    }
    return 0;
}

/// Rebuilds the room's mesh under the model of the slot-0xA task, or of the
/// slot-3 task when there is none. The mesh is offset by `vy` = 0 while a
/// slot-0xA task exists and flag nibble 0xD7 is set, and by 10000 otherwise.
/// `arg0` is unused.
void func_neo_ark_observatory_8017FA98(s32 arg0)
{
    Task* task;
    Task* slotA;

    task  = gameGetPtrSlot(0xA);
    slotA = task;
    if (task == NULL) {
        task = gameGetPtrSlot(3);
    }
    if (slotA != NULL && GameFlag_GetNibble(0xD7) != 0) {
        D_neo_ark_observatory_80181368.vy = 0;
    } else {
        D_neo_ark_observatory_80181368.vy = 0x2710;
    }
    func_neo_ark_observatory_8017FE34(((TmdObject*)task->extra)->coords, &D_neo_ark_observatory_80181368);
}

void func_neo_ark_observatory_8017FB1C(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x300, 0);
            Gp_SpawnIfCapIdle(task->spawnArg1, 0);
            task->state++;
            break;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
            task->state++;
            break;
        case 2:
            Gp_MsgPlayerWeapon(1);
            Gp_ResetCap();
            taskKill(task);
            break;
    }
}

s32 func_neo_ark_observatory_8017FBE0(void)
{
    return 0;
}

/// Room event-script handler: mirrors the incoming message onto the outgoing
/// one and lets `func_80179B14` act on both. Once the observatory has been
/// reached from both routes (nibbles 0xD1 == 3 and 0x4C == 9) and the script
/// raises one of the two arrival ids with no sub-state pending, nibble 0x4C is
/// cleared and the room's area records are applied.
s32 func_neo_ark_observatory_8017FBE8(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179B14(in, out);
    if ((GameFlag_GetNibble(0xD1) == 3) && (GameFlag_GetNibble(0x4C) == 9) &&
        ((in->msgId == 0xA) || (in->msgId == 0x13)) && (in->field_5 == 0)) {
        GameFlag_SetNibble(0x4C, 0);
        Gp_ApplyAreaRecs(&D_neo_ark_observatory_80187A28);
    }
    return 1;
}

s32 func_neo_ark_observatory_8017FCA0(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 1) {
        Gp_MsgPlayerWeapon(0);
        Task_SpawnFromTable(&D_neo_ark_observatory_801811AC, 0, 1, 0);
    }
    return 0;
}

/// Room entry task tick: installs the room's message table, hands the task to
/// slot 7, then advances state.
void func_neo_ark_observatory_8017FCE0(Task* arg0)
{
    arg0->msgTable = D_neo_ark_observatory_801811B8;
    Game_SetPtrSlot(arg0, 7);
    if ((gameGetPtrSlot(0xA) != NULL) && (gGameSession->at4.loc.place == 1)) {
        func_801322F8();
    } else {
        func_neo_ark_observatory_8017FA98(0);
    }
    if (GameFlag_GetNibble(0xE1) != 0) {
        func_neo_ark_observatory_80180DAC(0xA0);
    }
    arg0->state = arg0->state + 1;
}

/// Per-frame tick of the room entry task: sends the ally message 0x3F3 with 2
/// while no event runs and the save's view is not 2, and otherwise with 2 in
/// view 3 and 1 in any other view.
void func_neo_ark_observatory_8017FD7C(Task* task)
{
    s32 var_a0;

    if (gGameSession->eventState == 0) {
        if (D_8007216C.key.view != 2) {
            Gp_MsgAlly3F3(2);
            return;
        }
    }
    var_a0 = 1;
    if (D_8007216C.key.view == 3) {
        var_a0 = 2;
    }
    Gp_MsgAlly3F3(var_a0);
}

/// State handlers of the room entry task `func_neo_ark_observatory_8017FDDC`,
/// indexed by `Task::state`: the set-up tick, the arrival-line tick, and
/// `taskKill`.
const TaskFuncTable3 D_neo_ark_observatory_8017D698 = {
    {
        func_neo_ark_observatory_8017FCE0,
        func_neo_ark_observatory_8017FD7C,
        taskKill,
    },
};

/// Room entry task: dispatches through a stack copy of its state table.
void func_neo_ark_observatory_8017FDDC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_observatory_8017D698;
    sp.funcs[task->state](task);
}

/// Rebuilds the working mesh from its source under `coord`: the first four
/// vectors are rotated only, the eight after them rotated and translated and,
/// when `offset` is non-NULL, shifted by it afterwards.
void func_neo_ark_observatory_8017FE34(GsCOORDINATE2* coord, SVECTOR* offset)
{
    MATRIX                  m;
    long                    flag;
    s32                     i;
    SVECTOR*                d;
    SVECTOR*                s;
    _NeoArkObservatoryMesh* dst = &D_neo_ark_observatory_80181FA4;
    _NeoArkObservatoryMesh* src = &D_neo_ark_observatory_80181410;

    for (i = 0; i < 4; i++) {
        dst->field_4[i].vx = src->field_4[i].vx;
        dst->field_4[i].vy = src->field_4[i].vy;
        dst->field_4[i].vz = src->field_4[i].vz;
        dst->field_C[i]    = src->field_C[i];
    }

    for (i = 0; i < 8; i++) {
        dst->field_8[i].vx = src->field_8[i].vx;
        dst->field_8[i].vy = src->field_8[i].vy;
        dst->field_8[i].vz = src->field_8[i].vz;
    }

    m = coord->coord;

    d = dst->field_4;
    s = src->field_4;
    for (i = 0; i < 4; i++) {
        gte_SetRotMatrix(&m);
        gte_ldv0(s);
        s++;
        gte_rtv0();
        gte_stsv(d);
        d++;
    }

    gte_SetRotMatrix(&m);
    gte_SetTransMatrix(&m);
    d = dst->field_8;
    s = src->field_8;
    if (offset != NULL) {
        for (i = 0; i < 8; i++) {
            RotTransSV(s, d, &flag);
            s++;
            d->vx += offset->vx;
            d->vy += offset->vy;
            d->vz += offset->vz;
            d++;
        }
    } else {
        for (i = 0; i < 8; i++) {
            RotTransSV(s++, d++, &flag);
        }
    }
}

void func_neo_ark_observatory_80180124(Task* task)
{
    u8 view;

    if (task->state == 0) {
        D_neo_ark_observatory_80187A3C = 0;
        task->state                    = 1;
    }

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
            func_neo_ark_observatory_80180A0C(&D_neo_ark_observatory_801814E4[0], 0x280, 0x444);
            break;
        case 3: {
            SVECTOR* p;
            p = D_neo_ark_observatory_801814F4;
            func_neo_ark_observatory_80180A0C(&p[0], 0x280, 0x444);
            func_neo_ark_observatory_80180A0C(&p[1], 0x280, 0x444);
            break;
        }
        case 4:
        case 16: {
            SVECTOR* p;
            p = D_neo_ark_observatory_801814FC;
            func_neo_ark_observatory_80180A0C(&p[0], 0x280, 0x444);
            func_neo_ark_observatory_80180A0C(&p[1], 0x280, 0x444);
            func_neo_ark_observatory_80180A0C(&p[2], 0x280, 0x333);
            func_neo_ark_observatory_80180A0C(&p[3], 0x280, 0x222);
            break;
        }
        case 5:
        case 17: {
            SVECTOR* p;
            p = D_neo_ark_observatory_8018150C;
            func_neo_ark_observatory_80180A0C(&p[0], 0x280, 0x444);
            func_neo_ark_observatory_80180A0C(&p[1], 0x280, 0x444);
            break;
        }
        case 6:
        case 18:
            func_neo_ark_observatory_80180A0C(&D_neo_ark_observatory_8018151C[0], 0x200, 0x444);
            break;
        case 7: {
            SVECTOR* p;
            p = D_neo_ark_observatory_80181434;
            func_neo_ark_observatory_80180534(&p[0], 0x400, D_neo_ark_observatory_80187A3C, 8);
            func_neo_ark_observatory_80180534(&p[2], 0x400, D_neo_ark_observatory_80187A3C, 0xC);
            func_neo_ark_observatory_80180534(&p[4], 0x400, D_neo_ark_observatory_80187A3C, 8);
        }
            /* fallthrough */
        case 19: {
            SVECTOR* p;
            p = D_neo_ark_observatory_8018151C;
            func_neo_ark_observatory_80180A0C(&p[0], 0x200, 0x444);
            func_neo_ark_observatory_80180A0C(&p[2], 0x200, 0x222);
            func_neo_ark_observatory_80180A0C(&p[6], 0x200, 0x222);
            func_neo_ark_observatory_80180A0C(&p[7], 0x200, 0x333);
            func_neo_ark_observatory_80180A0C(&p[8], 0x200, 0x444);
            func_neo_ark_observatory_80180A0C(&p[9], 0x200, 0x444);
            break;
        }
        case 8:
        case 20: {
            SVECTOR* p;
            p = D_neo_ark_observatory_80181564;
            func_neo_ark_observatory_80180A0C(&p[0], 0x200, 0x444);
            func_neo_ark_observatory_80180A0C(&p[1], 0x200, 0x444);
            func_neo_ark_observatory_80180A0C(&p[2], 0x200, 0x444);
            func_neo_ark_observatory_80180534(&p[-32], 0x400, D_neo_ark_observatory_80187A3C, 8);
            func_neo_ark_observatory_80180534(&p[-30], 0x400, D_neo_ark_observatory_80187A3C, 8);
            func_neo_ark_observatory_80180534(&p[-28], 0x400, D_neo_ark_observatory_80187A3C, 8);
            break;
        }
        case 9: {
            SVECTOR* p;
            p = D_neo_ark_observatory_80181574;
            func_neo_ark_observatory_80180A0C(&p[0], 0x200, 0x444);
            func_neo_ark_observatory_80180A0C(&p[1], 0x200, 0x444);
            func_neo_ark_observatory_80180A0C(&p[2], 0x200, 0x333);
            func_neo_ark_observatory_80180A0C(&p[3], 0x200, 0x222);
            func_neo_ark_observatory_80180A0C(&p[-6], 0x200, 0x222);
            func_neo_ark_observatory_80180A0C(&p[-8], 0x200, 0x333);
            func_neo_ark_observatory_80180534(&p[-28], 0x400, D_neo_ark_observatory_80187A3C, 8);
            func_neo_ark_observatory_80180534(&p[-26], 0x400, D_neo_ark_observatory_80187A3C, 0xC);
            func_neo_ark_observatory_80180534(&p[-24], 0x400, D_neo_ark_observatory_80187A3C, 8);
            break;
        }
        case 10: {
            SVECTOR* p;
            p = D_neo_ark_observatory_80181524;
            func_neo_ark_observatory_80180A0C(&p[0], 0x200, 0x444);
            func_neo_ark_observatory_80180A0C(&p[1], 0x200, 0x444);
            func_neo_ark_observatory_80180A0C(&p[5], 0x200, 0x444);
            func_neo_ark_observatory_80180A0C(&p[6], 0x200, 0x333);
            func_neo_ark_observatory_80180A0C(&p[7], 0x200, 0x222);
            func_neo_ark_observatory_80180534(&p[-12], 0x600, D_neo_ark_observatory_80187A3C, 0x10);
            break;
        }
        case 11: {
            SVECTOR* p;
            p = D_neo_ark_observatory_8018157C;
            func_neo_ark_observatory_80180A0C(&p[0], 0x200, 0x222);
            func_neo_ark_observatory_80180A0C(&p[1], 0x200, 0x333);
            func_neo_ark_observatory_80180A0C(&p[2], 0x200, 0x444);
            func_neo_ark_observatory_80180A0C(&p[-7], 0x200, 0x444);
            func_neo_ark_observatory_80180A0C(&p[-8], 0x200, 0x444);
            func_neo_ark_observatory_80180534(&p[-21], 0x600, D_neo_ark_observatory_80187A3C, 0x10);
            break;
        }
        case 12:
        case 14: {
            SVECTOR* p;
            p = D_neo_ark_observatory_801814E4;
            func_neo_ark_observatory_80180A0C(&p[0], 0x280, 0x444);
            func_neo_ark_observatory_80180A0C(&p[2], 0x280, 0x444);
            break;
        }
        case 21: {
            SVECTOR* p;
            p = D_neo_ark_observatory_80181564;
            func_neo_ark_observatory_80180A0C(&p[0], 0x200, 0x444);
            func_neo_ark_observatory_80180A0C(&p[1], 0x200, 0x444);
            break;
        }
    }
}

/// Draws a rotating ring of gouraud `POLY_G4` segments between two circles in
/// the XZ plane: an inner circle of radius `(s16)arg1 / 2` around `v[0]` and an
/// outer one of radius `(s16)arg1` around `v[1]`. `arg3` segments cover the
/// full turn, starting at a phase that advances with the frame counter. The
/// inner edge is lit at `arg2` plus a small pulse, fading to half at the far
/// corner and to black on the outer edge; nothing is drawn while that level
/// is negative.
void func_neo_ark_observatory_80180534(SVECTOR* v, s32 arg1, s16 arg2, s16 arg3)
{
    _NeoArkObservatoryBandScratch* blk;
    POLY_G4*                       prim;
    SVECTOR*                       outer;
    DisplayState*                  ds;
    s16                            start;
    s16                            step;
    s32                            angle;
    s32                            next;
    s16                            innerRadius;
    s16                            level;

    step        = 0x1000 / arg3;
    outer       = v + 1;
    innerRadius = (s16)arg1 >> 1;
    start       = gDisplayState.animFrame & 0xFFF;
    level       = arg2 + (rsin(gDisplayState.animFrame << 10) >> 10);
    if (level >= 0) {
        *(u8**)G_SCRATCH_HEAD -= sizeof(_NeoArkObservatoryBandScratch);
        blk                    = *(_NeoArkObservatoryBandScratch**)G_SCRATCH_HEAD;
        gte_SetTransMatrix(&Gfx_ViewWorldMtx);
        for (angle = start; angle < start + step * arg3; angle = next) {
            blk->v[0].vx = v->vx + ((rsin(angle) * innerRadius) >> 12);
            blk->v[0].vy = v->vy;
            blk->v[0].vz = v->vz + ((rcos(angle) * innerRadius) >> 12);
            next         = angle + step;
            blk->v[1].vx = v->vx + ((rsin(next) * innerRadius) >> 12);
            blk->v[1].vy = v->vy;
            blk->v[1].vz = v->vz + ((rcos(next) * innerRadius) >> 12);
            blk->v[2].vx = outer->vx + ((rsin(angle) * (s16)arg1) >> 12);
            blk->v[2].vy = outer->vy;
            blk->v[2].vz = outer->vz + ((rcos(angle) * (s16)arg1) >> 12);
            blk->v[3].vx = outer->vx + ((rsin(next) * (s16)arg1) >> 12);
            blk->v[3].vy = outer->vy;
            blk->v[3].vz = outer->vz + ((rcos(next) * (s16)arg1) >> 12);
            gte_SetRotMatrix(&Gfx_ViewWorldMtx);
            gte_ldv0(&blk->v[0]);
            gte_rtps();
            gte_stsxy(&blk->sxy[0]);
            gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
            gte_rtpt();
            gte_stsxy3(&blk->sxy[1], &blk->sxy[2], &blk->sxy[3]);
            gte_stflg(&blk->flag);
            if (blk->flag >= 0) {
                gte_stszotz(&blk->otz);
                blk->otz++;
                ds             = &gDisplayState;
                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = (u8*)(prim + 1);
                setPolyG4(prim);
                setRGB0(prim, level, level, level);
                setRGB1(prim, level >> 1, level >> 1, level >> 1);
                setRGB2(prim, 0, 0, 0);
                setRGB3(prim, 0, 0, 0);
                addPrim((u_long*)((((u32)(blk->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                                  (s32)gGpuCurrentOt),
                        prim);
                prim->x0 = blk->sxy[0].vx;
                prim->y0 = blk->sxy[0].vy;
                prim->x1 = blk->sxy[1].vx;
                prim->y1 = blk->sxy[1].vy;
                prim->x2 = blk->sxy[2].vx;
                prim->y2 = blk->sxy[2].vy;
                prim->x3 = blk->sxy[3].vx;
                prim->y3 = blk->sxy[3].vy;
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
            }
        }
        *(u8**)G_SCRATCH_HEAD += sizeof(_NeoArkObservatoryBandScratch);
    }
}

/// Draws a glow around the world point `arg0`: projects it through
/// `Gfx_ViewWorldMtx` and, when the GTE flag is non-negative, queues four
/// gouraud `POLY_G4` quads that together make a disc around the projected
/// centre, lit at the centre and black at the rim. The on-screen radius is
/// `(s16)arg1 * 64 / otz`. `arg2` packs the centre colour as three RGB
/// nibbles, each OR'd with a flicker bit taken from the frame counter.
void func_neo_ark_observatory_80180A0C(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    register u8*       tmp asm("v0");
    RoomDraw13Scratch* block;
    POLY_G4*           prim;
    DisplayState*      ds;
    s32                ang;
    s32                t;
    s32                t2;
    s32                packed;
    s32                blend;
    s32                tr;
    s32                tg;
    u8                 r;
    u8                 g;
    u8                 b;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    tmp      = head - 0x10;
    block    = (RoomDraw13Scratch*)tmp;
    *scratch = tmp;

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        arg1 = ((s16)arg1 * 64) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        ang  = 0;
        tmp  = (u8*)&gDisplayState;
        SOFT_TOUCH_REG(tmp);
        ds            = (DisplayState*)tmp;
        blend         = (*(u8*)&ds->animFrame & 1) * 8;
        packed        = arg2 << 16;
        tr            = (packed >> 20) & 0xF0;
        tg            = (packed >> 16) & 0xF0;
        r             = blend | tr;
        g             = blend | tg;
        b             = blend | ((arg2 & 0xF) << 4);
        block->radius = arg1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x200;
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
}

void func_neo_ark_observatory_80180DAC(s16 arg0)
{
    D_neo_ark_observatory_80187A3C = arg0;
}
