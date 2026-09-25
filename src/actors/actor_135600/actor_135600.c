#include "common.h"

#include "decomp/common.h"

#include "actors/actor_135600.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_135600_80131E24;

/// Child task table the setup handler spawns from: index 1 is the actor's
/// walking model, index 2 the collision one and index 3 a model-less helper.
extern TaskDesc D_actor_135600_8013B0C4;

/// The actor's message table, stored in `Task::msgTable`: 0x7D3, 0x7D4 and
/// 0x7D5 against the handlers below.
extern GpMsgEntry D_actor_135600_8013B0F4[];

/// The 0x7D5 entry of `D_actor_135600_8013B0F4`: the actor's own visibility,
/// switched on the word `mode` rather than on a pointer.
s32 func_actor_135600_80133240(Task* task, s32 msgId, s32 mode, s32 arg3);

/// Composes `coord`'s parent chain into `mtx` and its world position into
/// `vec`, the two halves of the projection this unit's marker effect needs.
/// Defined in `actor_135600_2.c`.
void func_actor_135600_80132C80(GsCOORDINATE2* coord, MATRIX* mtx, SVECTOR* vec);

void Gp_UpdateCoord(GsCOORDINATE2* arg0);

/// The marker quad's two vertex pairs, in the actor's local frame: `-4/+4`
/// and `-3/+3` along X, all coplanar in Z.
extern SVECTOR D_actor_135600_8013B060[4];

/// Projects the two offsets along the actor's local Z - the near one 10 units
/// out and the far one `arg1 * 0x46 / 0x1000 + 10`, so the pair opens by 70
/// 4096ths of a unit per tick - and returns the signed `ratan2` of the
/// difference between the two projections, the actor's screen-space angle.
/// The pair is drawn as the quad `D_actor_135600_8013B060` describes: the wide
/// vertex pair rotated about the screen origin by that angle and anchored on
/// the near projection, the narrow pair unrotated on the far one, as a
/// semi-transparent `POLY_F4` followed by its texture page, both linked into
/// the ordering table at the far point's depth. Nothing is drawn when that
/// depth is behind the camera.
s32 func_actor_135600_80131E68(GsCOORDINATE2* coord, s32 arg1)
{
    SVECTOR           v0;
    SVECTOR           v1;
    SVECTOR           pos;
    SVECTOR           quad[4];
    Actor135600Matrix m;
    MATRIX*           mtx;
    s32               sxy0;
    s32               p;
    s32               flag;
    s32               sxy1;
    s16               y0;
    s16               y1;
    s32               rot;
    u16               x0;
    u16               x1;
    s32               depth;
    POLY_F4*          poly;
    DR_TPAGE*         tpage;
    s32               i;

    Gp_UpdateCoord(coord);
    mtx = &m.mat;
    func_actor_135600_80132C80(coord, &m.mat, &pos);

    v0.vx = 0;
    v0.vy = 0;
    v0.vz = 0xA;
    ApplyMatrixSV(&m.mat, &v0, &v0);

    v1.vx = 0;
    v1.vy = 0;
    v1.vz = arg1 * 0x46 / 0x1000 + 0xA;
    ApplyMatrixSV(&m.mat, &v1, &v1);

    v0.vx += pos.vx;
    v0.vy += pos.vy;
    v0.vz += pos.vz;
    v1.vx += pos.vx;
    v1.vy += pos.vy;
    v1.vz += pos.vz;

    SetRotMatrix(&Gfx_ViewWorldMtx);
    SetTransMatrix(&Gfx_ViewWorldMtx);

    RotTransPers(&v0, &sxy0, &p, &flag);
    depth = RotTransPers(&v1, &sxy1, &p, &flag);

    x0  = *(u16*)&sxy0;
    x1  = *(u16*)&sxy1;
    y0  = sxy0 >> 16;
    y1  = sxy1 >> 16;
    rot = ratan2(*(s16*)&sxy1 - *(s16*)&sxy0, y0 - y1);

    /* Only the middle diagonal and the last entry go through `mtx`: a store
     * written that way keeps its address in the register `RotMatrixZ` is
     * handed, where the ones naming `m` directly fold to a frame-relative
     * address, and the target has both. */
    m.ident.m00_m01       = 0x1000;
    *(s32*)&m.mat.m[0][2] = 0;
    *(s32*)&mtx->m[1][1]  = 0x1000;
    *(s32*)&m.mat.m[2][0] = 0;
    mtx->m[2][2]          = 0x1000;
    RotMatrixZ(rot, &m.mat);

    for (i = 0; i < 2; i++) {
        ApplyMatrixSV(&m.mat, &D_actor_135600_8013B060[i], &quad[i]);
        quad[i].vx    += x0;
        quad[i].vy    += y0;
        quad[i + 2].vx = D_actor_135600_8013B060[i + 2].vx + x1;
        quad[i + 2].vy = D_actor_135600_8013B060[i + 2].vy + y1;
    }

    if (p >= 0) {
        poly           = (POLY_F4*)gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        setlen(poly, 5);
        setcode(poly, 0x2A);
        setRGB0(poly, 0xFF, 0x40, 0);
        poly->x0 = quad[0].vx;
        poly->y0 = quad[0].vy;
        poly->x1 = quad[1].vx;
        poly->y1 = quad[1].vy;
        poly->x2 = quad[2].vx;
        poly->y2 = quad[2].vy;
        poly->x3 = quad[3].vx;
        poly->y3 = quad[3].vy;
        addPrim((u_long*)(((((u32)depth << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt) - 2, poly);

        tpage          = gGpuPrimCursor;
        gGpuPrimCursor = tpage + 1;
        setlen(tpage, 1);
        tpage->code[0] = 0xE1000465;
        addPrim((u_long*)(((((u32)depth << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt) - 2, tpage);
    }
    return rot;
}

void func_actor_135600_80132234(Task* task)
{
    Actor135600Work*      work;
    Task*                 spawned;
    TmdObject*            model1;
    TmdObject*            model2;
    GpAreaKey*            sessionKey1;
    GpAreaKey*            keyp1;
    GpAreaKey*            sessionKey2;
    GpAreaKey*            keyp2;
    GpAreaPlace*          entry1;
    GpAreaPlace*          entry2;
    u32                   index1;
    u32                   index2;
    u32                   raw1;
    u32                   raw2;
    u8                    areaByte0;
    u8                    areaByte1;
    Actor135600PlaceArgs  args;
    Actor135600AnimPreset preset;
    GpAreaKey             key;

    work = (Actor135600Work*)memCalloc(0x50C, false);
    if (work == NULL) {
        Gp_EnemyTaskExit(task);
        return;
    }
    task->work      = (TaskIdMap*)work;
    work->field_475 = -1;
    work->field_476 = -1;
    work->field_508 = -1;
    work->field_4D8 = 0;
    work->field_4DC = 0;
    work->field_4E0 = 0;

    spawned = Task_SpawnFromTable(&D_actor_135600_8013B0C4, 1, 8, (s32)task);
    if (spawned != NULL) {
        work->field_500 = spawned;
        model1          = (TmdObject*)spawned->extra;
        raw1            = ((GpEnemy*)task->spawnArg2)->placeKey;
        sessionKey1     = (GpAreaKey*)&gGameSession->at4.loc;
        key.stage       = sessionKey1->stage;
        key.area        = sessionKey1->area;
        /* Both calls below hand `key` to Gp_SyncAreaKeyIndex and then to
         * Gp_GetNestedAreaRec. Read as one straight-line block, the two
         * `&key` arguments global-CSE into a single address pseudo that then
         * has to survive the first call, which costs `$s4` and shifts `work`,
         * `model` and the index up a register each; the barrier plus the
         * touched pointer pin the block's shape, and each call recomputes the
         * address the way the target does. The same two files'
         * `Actor401300_TintEffect` and `func_actor_450800_80132160` are the
         * worked examples of the idiom. */
        SOFT_BARRIER();
        keyp1    = &key;
        key.room = sessionKey1->room;
        TOUCH_REG(keyp1);
        areaByte0 = gGameSession->at4.loc.view;
        index1    = raw1 >> 12;
        key.view  = areaByte0;
        Gp_SyncAreaKeyIndex(keyp1);
        entry1        = (GpAreaPlace*)((index1 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model1->tpage = entry1->tpage;
        model1->clut  = entry1->clut;
        if (model1->buffer != NULL) {
            tmdProcessStream(model1);
            tmdProcessStream(model1);
        }
    }

    spawned = Task_SpawnFromTable(&D_actor_135600_8013B0C4, 2, 0xC, (s32)task);
    if (spawned != NULL) {
        work->field_4FC = spawned;
        model2          = (TmdObject*)spawned->extra;
        raw2            = ((GpEnemy*)task->spawnArg2)->placeKey;
        sessionKey2     = (GpAreaKey*)&gGameSession->at4.loc;
        key.stage       = sessionKey2->stage;
        key.area        = sessionKey2->area;
        SOFT_BARRIER();
        keyp2    = &key;
        key.room = sessionKey2->room;
        TOUCH_REG(keyp2);
        areaByte1 = gGameSession->at4.loc.view;
        index2    = raw2 >> 12;
        key.view  = areaByte1;
        Gp_SyncAreaKeyIndex(keyp2);
        entry2        = (GpAreaPlace*)((index2 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model2->tpage = entry2->tpage;
        model2->clut  = entry2->clut;
        if (model2->buffer != NULL) {
            tmdProcessStream(model2);
            tmdProcessStream(model2);
        }
    }

    spawned = Task_SpawnFromTable(&D_actor_135600_8013B0C4, 3, 8, (s32)task);
    if (spawned != NULL) {
        work->field_504 = spawned;
    }

    func_actor_135600_80132DDC(task);

    args.pos.vx = 0xA6E;
    args.pos.vz = 0x5F0;
    args.pos.vy = 0;
    args.rot.vx = 0;
    args.rot.vy = 0x400;
    args.rot.vz = 0;
    func_actor_135600_801331C4(task, 0x7D4, &args, 0);

    preset.field_0 = 0;
    preset.field_4 = 2;
    preset.field_8 = 0;
    func_actor_135600_801330A8(task, 0x7D3, &preset, 0);

    func_actor_135600_80133240(task, 0x7D5, 1, 0);

    task->msgTable     = D_actor_135600_8013B0F4;
    task->exitCallback = func_actor_135600_80132DBC;
    task->state       += 1;
}

void func_actor_135600_801324D0(Task* arg0)
{
    TmdObject*       ext      = arg0->extra;
    Actor135600Work* work     = (Actor135600Work*)arg0->work;
    TaskFunc         funcs[2] = { func_actor_135600_80132DF8, func_actor_135600_80132E00 };
    VECTOR3          pos;
    GsCOORDINATE2*   coord;
    s32              i;

    if (!(ext->flags & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)arg0->extra)->coords[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x300, Gp_State1C->groundShade);
        }
    }
    if (D_801153F4 == 0) {
        funcs[work->field_4F8](arg0);
        coord              = ((TmdObject*)arg0->extra)->coords;
        work->field_4D8   += work->step.vx;
        work->field_4DC   += work->step.vy;
        work->field_4E0   += work->step.vz;
        coord->coord.t[0] += (s16)(work->field_4D8 >> 16);
        coord->coord.t[1] += (s16)(work->field_4DC >> 16);
        coord->coord.t[2] += (s16)(work->field_4E0 >> 16);
        coord->flg         = 0;
        work->field_4D8    = (u16)work->field_4D8;
        work->field_4DC    = (u16)work->field_4DC;
        work->field_4E0    = (u16)work->field_4E0;
        if (work->field_474 != 0) {
            for (i = 1; i < 0x14; i++) {
                Gp_AnimTickIndex(&work->anim, i);
            }
        }
        if (gGameSession->viewReady != 0) {
            ((TmdObject*)arg0->extra)->coords[1].flg = 0;
            Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[1]);
            func_800D7A9C(ext, (VECTOR*)((TmdObject*)arg0->extra)->coords[1].workm.t, 0, 3);
        }
        if (work->field_508 >= 0) {
            if (work->field_508 == 0) {
                Tmd_FreeBuffers(ext);
            }
            work->field_508--;
        }
    }
}

/// Step 2 of the motion sequence, the arrival check. Once the X/Z distances
/// from the root coordinate to `target` stop shrinking below `limit`, plays the
/// animation with a preset carrying the `field_477` byte, clears `step` and
/// advances `field_4FA`; otherwise records the distances as the new `limit`.
void func_actor_135600_801326E8(Task* arg0)
{
    Actor135600Work*      work;
    GsCOORDINATE2*        coord;
    SVECTOR               d;
    s32                   dx;
    s32                   dz;
    Actor135600AnimPreset preset;

    work  = (Actor135600Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (work->target.vx - coord->coord.t[0] >= 0) {
        dx = (u16)work->target.vx - (u16)coord->coord.t[0];
    } else {
        dx = (u16)coord->coord.t[0] - (u16)work->target.vx;
    }
    d.vx = dx;
    if (work->target.vz - coord->coord.t[2] >= 0) {
        dz = (u16)work->target.vz - (u16)coord->coord.t[2];
    } else {
        dz = (u16)coord->coord.t[2] - (u16)work->target.vz;
    }
    d.vz = dz;
    if (d.vx >= work->limit.vx && d.vz >= work->limit.vz) {
        preset.field_0  = 0;
        preset.field_4  = work->field_477;
        preset.field_8  = 1;
        preset.field_C  = 5;
        preset.field_10 = 0;
        func_actor_135600_801330A8(arg0, 0x7D3, &preset, 0);
        work->step.vx = 0;
        work->step.vy = 0;
        work->step.vz = 0;
        work->field_4FA++;
        return;
    }
    work->limit.vx = d.vx < 0 ? -d.vx : d.vx;
    work->limit.vz = d.vz < 0 ? -d.vz : d.vz;
}

/// The 0x7DD entry of `D_actor_135600_8013B0F4`: starts the motion sequence,
/// storing the placement position as `target` and its rotation in
/// `field_4F0..field_4F4`, then applies a start preset -- `anim`'s, or anim 0xD
/// with preset byte 1 when absent -- with the body of
/// `func_actor_135600_801330A8` written out inline. Returns 0.
s32 func_actor_135600_8013282C(Task* task, s32 arg1, Actor135600PlaceArgs* place, Actor135600SpawnAnim* anim)
{
    Actor135600Work*       work;
    Actor135600Work*       w;
    Actor135600AnimPreset  preset;
    Actor135600AnimPreset* msg;
    s32                    i;
    TmdObject*             ext;

    w              = (Actor135600Work*)task->work;
    w->field_4F8   = 1;
    w->field_4FA   = 0;
    w->target.vx   = place->pos.vx;
    w->target.vy   = place->pos.vy;
    w->target.vz   = place->pos.vz;
    w->field_4F0   = place->rot.vx;
    w->field_4F2   = place->rot.vy;
    w->field_4F4   = place->rot.vz;
    preset.field_0 = 0;
    if (anim != NULL) {
        preset.field_4 = anim->field_0;
        w->field_477   = anim->field_4;
    } else {
        preset.field_4 = 0xD;
        w->field_477   = 1;
    }
    preset.field_8  = 1;
    preset.field_C  = 5;
    preset.field_10 = 1;

    msg  = &preset;
    work = (Actor135600Work*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_476) {
        work->field_476 = msg->field_0;
        func_800B3F84(&work->anim, D_actor_135600_8013B0C0[work->field_476], ext, work->poses, work->slots);
    }
    work->field_475 = msg->field_4;
    if (msg->field_8 != 0 && work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            func_800B4114(&work->anim, i, work->field_475, 0, msg->field_C);
        }
    } else {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimResetSlot(&work->anim, i, work->field_475);
        }
    }
    for (i = 1; i < 0x14; i++) {
        Gp_AnimTickIndex(&work->anim, i);
    }
    work->field_474 = 1;
    return 0;
}

INCLUDE_RODATA("actors/nonmatchings/actor_135600/actor_135600", D_actor_135600_80131E24);

void func_actor_135600_801329E0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135600_80131E24;
    sp.funcs[task->state](task);
}

INCLUDE_RODATA("actors/nonmatchings/actor_135600/actor_135600", D_actor_135600_80131E30);

INCLUDE_RODATA("actors/nonmatchings/actor_135600/actor_135600", D_actor_135600_80131E3C);
