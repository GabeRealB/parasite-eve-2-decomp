#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/abs.h>

#include "decomp/common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/D4.h"

#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// Work block of the `actor_135600` enemy task, the `memCalloc(0x50C, 0)`
/// result `func_actor_135600_80132234` parks in `Task::work` -- that slot is
/// not a `TaskIdMap` here. The setup handler seeds `field_475` / `field_476`
/// and `field_508` with -1, zeroes the three 16.16 accumulators at
/// `field_4D8..field_4E0`, and parks the three children it spawns in
/// `field_4FC` / `field_500` / `field_504`. The light/colour pair is what
/// `func_actor_135600_80132DDC` points the model at; `target`, `step`, `limit`
/// and `field_4FA` belong to the motion sequence the step handlers of
/// `D_actor_135600_80131E48` run.
typedef struct Actor135600Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0x14];  // the slot array `func_800B3F84` is handed
    /* 0x334 */ byte       poses[0x140]; // pose buffer `func_800B3F84` is handed
    /* 0x474 */ s8         field_474;    // non-zero while the animation slots tick
    /* 0x475 */ s8         field_475;    // current animation id
    /* 0x476 */ s8         field_476;    // current bank index into `D_actor_135600_8013B0C0`
    /* 0x477 */ s8         field_477;    // preset byte the arrival and turn steps pass as `field_4`
    /* 0x478 */ MATRIX     light;
    /* 0x498 */ MATRIX     color;
    /* 0x4B8 */ VECTOR3    target;    // world position the turn-to-face step steers toward
    /* 0x4C4 */ byte       pad_4C4[0x4C8 - 0x4C4];
    /* 0x4C8 */ VECTOR3    step;      // per-frame deltas the accumulators take
    /* 0x4D4 */ byte       pad_4D4[0x4D8 - 0x4D4];
    /* 0x4D8 */ s32        field_4D8; // 16.16 accumulators; only the high half reaches the coordinate
    /* 0x4DC */ s32        field_4DC;
    /* 0x4E0 */ s32        field_4E0;
    /* 0x4E4 */ byte       pad_4E4[0x4E8 - 0x4E4];
    /* 0x4E8 */ SVECTOR    limit;     // per-axis stop threshold; 0x7FFF on all three disables it
    /* 0x4F0 */ u16        field_4F0;
    /* 0x4F2 */ u16        field_4F2; // placement yaw the last step turns to
    /* 0x4F4 */ u16        field_4F4;
    /* 0x4F6 */ byte       pad_4F6[0x4F8 - 0x4F6];
    /* 0x4F8 */ s16        field_4F8; // selects which of the two handlers the tick runs
    /* 0x4FA */ u16        field_4FA; // index into the step-handler table `D_actor_135600_80131E48`
    /* 0x4FC */ Task*      field_4FC;
    /* 0x500 */ Task*      field_500;
    /* 0x504 */ Task*      field_504;
    /* 0x508 */ s32        field_508;
    /* 0x50C */ byte       pad_50C[0];
} Actor135600Work;
STATIC_ASSERT_SIZEOF(Actor135600Work, 0x50C);

/// Animation preset the 0x7D3 handler `func_actor_135600_801330A8` applies.
/// `field_0` is the bank index into `D_actor_135600_8013B0C0`, `field_4` the
/// animation id, `field_8` selects a blended restart once the slots run and
/// `field_C` is handed to that restart. The arrival and turn steps build one on
/// their own stack.
typedef struct Actor135600AnimPreset {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
} Actor135600AnimPreset;
STATIC_ASSERT_SIZEOF(Actor135600AnimPreset, 0x14);

/// The world translation and Euler angles the 0x7D4 handler
/// `func_actor_135600_801331C4` places the root part at, and the placement the
/// motion-start handler `func_actor_135600_8013282C` walks the actor to.
typedef struct Actor135600PlaceArgs {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor135600PlaceArgs;
STATIC_ASSERT_SIZEOF(Actor135600PlaceArgs, 0x18);

/// Optional start animation for `func_actor_135600_8013282C`: the preset's
/// `field_4` and the `field_477` byte. Absent, the defaults are anim 0xD and 1.
typedef struct Actor135600SpawnAnim {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ u8  field_4;
} Actor135600SpawnAnim;

/// Overlay of the `GsCOORDINATE2` at `TmdObject::coords`, the actor's root
/// part. Offset 0x44 (libgs `param`) holds the Euler angles the placement and
/// turn-to-face handlers write and hand straight to `RotMatrix`.
typedef struct Actor135600Coord {
    /* 0x00 */ s32     flg;
    /* 0x04 */ MATRIX  coord;
    /* 0x24 */ MATRIX  workm;
    /* 0x44 */ SVECTOR rot;
} Actor135600Coord;
STATIC_ASSERT_SIZEOF(Actor135600Coord, 0x4C);

/// Word-wise view of a `MATRIX` used to splat an identity rotation: five
/// aligned stores instead of nine halfword ones, each word holding two adjacent
/// `m[][]` entries.
typedef struct Actor135600MatrixWords {
    /* 0x00 */ s32 m00_m01;
    /* 0x04 */ s32 m02_m10;
    /* 0x08 */ s32 m11_m12;
    /* 0x0C */ s32 m20_m21;
    /* 0x10 */ s16 m22;
} Actor135600MatrixWords;

typedef union Actor135600Matrix {
    MATRIX                 mat;
    Actor135600MatrixWords ident;
} Actor135600Matrix;
STATIC_ASSERT_SIZEOF(Actor135600Matrix, 0x20);

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).
void func_8004BFF8(s16 angle, MATRIX* matrix);

/// Declared locally with a signed animation id, as every caller passes one.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void Gp_UpdateCoord(GsCOORDINATE2* arg0);

/// Non-zero while a non-gameplay mode holds the actors; the tick skips its
/// whole update while it is set.
extern u8 D_801153F4;

/// The marker quad's two vertex pairs, in the actor's local frame: `-4/+4`
/// and `-3/+3` along X, all coplanar in Z.
extern SVECTOR D_actor_135600_8013B060[4];

/// Animation bank table the 0x7D3 handler seeds the slot array from, indexed
/// by the preset's bank index.
extern void* D_actor_135600_8013B0C0[];

/// Child task table the setup handler spawns from: entry 0 is the actor
/// itself, entries 1 and 2 the two part models and entry 3 the marker task.
extern TaskDesc D_actor_135600_8013B0C4;

/// The actor's message table, stored in `Task::msgTable`: 0x7D3, 0x7D4, 0x7D5,
/// 0x7DD and 0x7DB against the handlers below.
extern GpMsgEntry D_actor_135600_8013B0F4[];

void func_actor_135600_80132234(Task* task);
void func_actor_135600_801324D0(Task* task);
void func_actor_135600_801326E8(Task* task);
void func_actor_135600_80132A38(Task* task);
void func_actor_135600_80132AB4(Task* task);
void func_actor_135600_80132B14(Task* task);
void func_actor_135600_80132C18(Task* task);
void func_actor_135600_80132C80(GsCOORDINATE2* coord, MATRIX* mtx, SVECTOR* vec);
void func_actor_135600_80132DBC(Task* task);
void func_actor_135600_80132DDC(Task* task);
void func_actor_135600_80132DF8(Task* task);
void func_actor_135600_80132E00(Task* task);
void func_actor_135600_80132E68(Task* task);
void func_actor_135600_80132F28(Task* task);
void func_actor_135600_80132FA8(Task* task);
s32  func_actor_135600_801330A8(Task* task, s32 msgId, Actor135600AnimPreset* preset, s32 arg3);
s32  func_actor_135600_801331C4(Task* task, s32 msgId, Actor135600PlaceArgs* args, s32 arg3);
s32  func_actor_135600_80133240(Task* task, s32 msgId, s32 mode, s32 arg3);

/// States of the two part tasks (`D_actor_135600_8013B0C4` entries 1 and 2),
/// dispatched by `func_actor_135600_801329E0`: attach to the parent, idle,
/// kill.
const TaskFuncTable3 D_actor_135600_80131E24 = { {
    func_actor_135600_80132A38,
    func_actor_135600_80132AB4,
    taskKill,
} };

/// States of the marker task (entry 3), dispatched by
/// `func_actor_135600_80132ABC`: attach to the parent with an offset, draw the
/// marker, kill.
const TaskFuncTable3 D_actor_135600_80131E30 = { {
    func_actor_135600_80132B14,
    func_actor_135600_80132C18,
    taskKill,
} };

/// States of the actor itself (entry 0), dispatched by
/// `func_actor_135600_80132D64`: setup, per-frame tick, exit.
const TaskFuncTable3 D_actor_135600_80131E3C = { {
    func_actor_135600_80132234,
    func_actor_135600_801324D0,
    func_actor_135600_80132DBC,
} };

/// Step handlers of the motion sequence, indexed by
/// `Actor135600Work::field_4FA`: turn to face `target`, start walking forward,
/// walk until arrival, then turn to the placement yaw.
const TaskFuncTable4 D_actor_135600_80131E48 = { {
    func_actor_135600_80132E68,
    func_actor_135600_80132F28,
    func_actor_135600_801326E8,
    func_actor_135600_80132FA8,
} };

/// The constant local-space offset `func_actor_135600_80132F28` rotates:
/// straight ahead along the part's own +Z.
const VECTOR D_actor_135600_80131E58 = { 0, 0, 0x200000, 0 };

/// Recomputes `coord`'s world matrix (`Gp_UpdateCoord`), composes its parent
/// chain, then projects two offsets along the part's local Z - the near one 10 units
/// out and the far one `arg1 * 0x46 / 0x1000 + 10`, so the pair opens by 70
/// 4096ths of a unit per tick - and returns the signed `ratan2` of the
/// difference between the two projections, the actor's screen-space angle.
/// The pair is drawn as the quad `D_actor_135600_8013B060` describes: the wide
/// vertex pair rotated about the screen origin by that angle and anchored on
/// the near projection, the narrow pair unrotated on the far one, as a
/// semi-transparent `POLY_F4` followed by its texture page, both linked into
/// the ordering table at the far point's depth. Nothing is drawn when that
/// depth is behind the camera. The marker's draw state passes the countdown it
/// runs on as `arg1`.
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

/// Setup state of the actor (entry 0 of `D_actor_135600_80131E3C`). It
/// allocates the 0x50C-byte work block, seeds the -1 sentinels and spawns
/// entries 1 to 3 of `D_actor_135600_8013B0C4` -- the two part models get the
/// texture page and CLUT of the area record the actor's placement key resolves
/// to, and all three are parked in the work block. It then points the model at
/// the work block's light/colour pair, places it at (0xA6E, 0, 0x5F0) yawed
/// 0x400, applies animation 2, shows it (message 0x7D5 mode 1), publishes the
/// message table `D_actor_135600_8013B0F4`, installs the exit callback and
/// steps to the tick state.
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
         * address the way the target does. */
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

/// Per-frame tick of the actor (entry 1 of `D_actor_135600_80131E3C`).
/// Draws the ground shadow under the second part unless the model is hidden,
/// then -- only while `D_801153F4` is clear -- runs the handler `field_4F8`
/// selects, advances the root coordinate by the high halves of the 16.16
/// accumulators fed from `step` (re-zeroing each high half), ticks slots 1 to
/// 19 while `field_474` is set, rebuilds the second part's coordinate and the
/// actor colour while `gGameSession->viewReady` is set, and counts `field_508`
/// down while non-negative, freeing the model's buffers when it reaches zero.
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

/// Dispatcher of the two part tasks: runs their state from
/// `D_actor_135600_80131E24`.
void func_actor_135600_801329E0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135600_80131E24;
    sp.funcs[task->state](task);
}

/// Setup state of a part task (entry 0 of `D_actor_135600_80131E24`): chains
/// the task's root coordinate under the parent's part coordinate the spawn
/// arguments name, inherits the parent's light and colour matrices, reparents the task so it
/// is updated with the parent, and advances to the next state.
void func_actor_135600_80132A38(Task* task)
{
    Task*          parent;
    s32            part;
    TmdObject*     extra;
    TmdObject*     parentExtra;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* dest;

    parent          = (Task*)task->spawnArg2;
    part            = task->spawnArg1;
    extra           = (TmdObject*)task->extra;
    parentExtra     = (TmdObject*)parent->extra;
    coord           = extra->coords;
    dest            = &parentExtra->coords[part];
    coord->flg      = 0;
    coord->sub      = dest;
    extra->lightMtx = parentExtra->lightMtx;
    extra->colorMtx = parentExtra->colorMtx;
    Task_Reparent(parent, task);
    task->state += 1;
}

/// Tick state of a part task: nothing to do, the parent drives it.
void func_actor_135600_80132AB4(Task* task)
{
}

/// Dispatcher of the marker task: runs its state from
/// `D_actor_135600_80131E30`.
void func_actor_135600_80132ABC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135600_80131E30;
    sp.funcs[task->state](task);
}

/// Setup state of the marker task (entry 0 of `D_actor_135600_80131E30`):
/// chains its model root under the parent task's part `spawnArg1`,
/// places the part's coordinate at (-150, 80, 0), turns its rotation by 90
/// degrees about Y, inherits the parent's light and colour matrices, and
/// reparents the task so it is updated with the parent. The kill countdown is
/// set to 0x1000, the value the marker's draw state runs on.
void func_actor_135600_80132B14(Task* task)
{
    Actor135600Matrix m;
    MATRIX*           mtx;
    Task*             parent;
    s32               part;
    TmdObject*        extra;
    TmdObject*        parentExtra;
    GsCOORDINATE2*    coord;
    GsCOORDINATE2*    dest;

    parent      = (Task*)task->spawnArg2;
    extra       = (TmdObject*)task->extra;
    part        = task->spawnArg1;
    parentExtra = (TmdObject*)parent->extra;
    coord       = extra->coords;
    dest        = &parentExtra->coords[part];

    coord->coord.t[0] = -0x96;
    coord->coord.t[1] = 0x50;
    coord->coord.t[2] = 0;

    mtx                  = &m.mat;
    m.ident.m00_m01      = 0x1000;
    *(s32*)&mtx->m[0][2] = 0;
    *(s32*)&mtx->m[1][1] = 0x1000;
    *(s32*)&mtx->m[2][0] = 0;
    mtx->m[2][2]         = 0x1000;

    func_8004BFF8(0x400, mtx);
    MulMatrix0(&coord->coord, mtx, &coord->coord);

    coord->sub      = dest;
    coord->flg      = 0;
    extra->lightMtx = parentExtra->lightMtx;
    extra->colorMtx = parentExtra->colorMtx;
    extra->otOffset = 0;
    Task_Reparent(parent, task);
    task->killCountdown = 0x1000;
    task->state        += 1;
}

/// Draw state of the marker task: while `gGameSession->eventState` is set and
/// the kill countdown is still running, draws the marker at the countdown's
/// length, cutting the countdown to 0x800 once the marker's angle reaches
/// 0x1F5.
void func_actor_135600_80132C18(Task* task)
{
    s16 countdown;

    if (gGameSession->eventState != 0) {
        countdown = task->killCountdown;
        if (countdown > 0 && func_actor_135600_80131E68(((TmdObject*)task->extra)->coords, countdown) >= 0x1F5) {
            task->killCountdown = 0x800;
        }
    }
}

/// Walks `coord->sub` up to world (`gGfxViewCoord`), composing each node's
/// `coord` rotation into `mtx` and accumulating the rotated translation into
/// `vec`. The world parent initializes `mtx` to identity and `vec` to zero.
/// The same algorithm as gameplay's `Gp_ComposeParentWorld`, but through the
/// library `ApplyMatrixSV` / `MulMatrix0` rather than the GTE macros.
void func_actor_135600_80132C80(GsCOORDINATE2* coord, MATRIX* mtx, SVECTOR* vec)
{
    SVECTOR tmp;
    MATRIX* m;

    if (coord->sub != &gGfxViewCoord) {
        func_actor_135600_80132C80(coord->sub, mtx, vec);
    } else {
        m                  = mtx;
        *(s32*)m           = 0x1000;
        *(s32*)&m->m[0][2] = 0;
        *(s32*)&m->m[1][1] = 0x1000;
        *(s32*)&m->m[2][0] = 0;
        m->m[2][2]         = 0x1000;
        vec->vx            = 0;
        vec->vy            = 0;
        vec->vz            = 0;
    }

    tmp.vx = *(u16*)&coord->coord.t[0];
    tmp.vy = *(u16*)&coord->coord.t[1];
    tmp.vz = *(u16*)&coord->coord.t[2];
    ApplyMatrixSV(mtx, &tmp, &tmp);
    vec->vx += tmp.vx;
    vec->vy += tmp.vy;
    vec->vz += tmp.vz;
    MulMatrix0(mtx, &coord->coord, mtx);
}

/// Dispatcher of the actor itself: runs its state from
/// `D_actor_135600_80131E3C`.
void func_actor_135600_80132D64(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135600_80131E3C;
    sp.funcs[task->state](task);
}

/// Exit state and exit callback of the actor: the `Gp_EnemyTaskExit` teardown.
void func_actor_135600_80132DBC(Task* task)
{
    Gp_EnemyTaskExit(task);
}

/// Points the model's light and colour matrices at the work block's own pair.
void func_actor_135600_80132DDC(Task* task)
{
    TmdObject*       ext;
    Actor135600Work* work;

    ext           = task->extra;
    work          = (Actor135600Work*)task->work;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}

/// Entry 0 of the tick's handler pair, selected by `field_4F8` while no motion
/// sequence runs: does nothing.
void func_actor_135600_80132DF8(Task* arg0)
{
}

/// Entry 1 of the tick's handler pair: runs the step of
/// `D_actor_135600_80131E48` that `field_4FA` selects.
void func_actor_135600_80132E00(Task* task)
{
    Actor135600Work* work;
    TaskFuncTable4   fns;

    work = (Actor135600Work*)task->work;
    fns  = D_actor_135600_80131E48;
    fns.funcs[(s16)work->field_4FA](task);
}

/// Step 0: turns the root part to face `work->target`, taking the yaw of the
/// normalised offset from the part's own translation with `ratan2` and
/// rebuilding the local matrix from that yaw alone, then advances the step.
void func_actor_135600_80132E68(Task* task)
{
    Actor135600Work*  work;
    Actor135600Coord* coord;
    VECTOR            delta;
    SVECTOR           dir;
    SVECTOR           rot;

    work  = (Actor135600Work*)task->work;
    coord = (Actor135600Coord*)((TmdObject*)task->extra)->coords;

    delta.vx = work->target.vx - coord->coord.t[0];
    delta.vy = work->target.vy - coord->coord.t[1];
    delta.vz = work->target.vz - coord->coord.t[2];
    VectorNormalS(&delta, &dir);

    rot.vx = 0;
    rot.vy = ratan2(dir.vx, dir.vz);
    rot.vz = 0;

    coord->rot.vx = rot.vx;
    coord->rot.vy = rot.vy;
    coord->rot.vz = rot.vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg = 0;
    work->field_4FA++;
}

/// Step 1: rotates the forward offset `D_actor_135600_80131E58` through the
/// root part's matrix into `work->step`, opens the per-axis stop threshold to
/// 0x7FFF, which disables it, and advances the step.
void func_actor_135600_80132F28(Task* task)
{
    Actor135600Work* work;
    GsCOORDINATE2*   coord;
    VECTOR           vec;

    coord = ((TmdObject*)task->extra)->coords;
    work  = (Actor135600Work*)task->work;

    vec = D_actor_135600_80131E58;
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->step);
    work->limit.vx = 0x7FFF;
    work->limit.vy = 0x7FFF;
    work->limit.vz = 0x7FFF;
    work->field_4FA++;
}

/// Step 3 of the motion sequence, the turn to the placement yaw.
/// Euler-extracts the root coordinate into `vec`, and while the yaw gap to
/// `work->field_4F2` is at least 0x41 steps `vec.vy` toward it by 0x40, taking
/// the step on an `s32` widening of the extracted yaw; otherwise snaps the yaw
/// to the target, plays the animation with a preset carrying the `field_477`
/// byte and clears `field_4F8` / `field_4FA`, ending the sequence. Either way
/// the root coordinate is rebuilt as the identity matrix rotated by `vec`.
void func_actor_135600_80132FA8(Task* arg0)
{
    Actor135600Work*        work;
    Actor135600MatrixWords* words;
    GsCOORDINATE2*          coord;
    SVECTOR                 vec;
    Actor135600AnimPreset   preset;
    s32                     vy;
    s16                     diff;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor135600Work*)arg0->work;

    Gp_ExtractEuler(&vec, &coord->coord);
    diff = (u16)work->field_4F2 - (u16)vec.vy;
    if (ABS(diff) >= 0x41) {
        vy = vec.vy;
        if (diff < 0) {
            vec.vy = vy - 0x40;
        } else {
            vec.vy = vy + 0x40;
        }
    } else {
        vec.vy          = work->field_4F2;
        preset.field_0  = 0;
        preset.field_4  = work->field_477;
        preset.field_8  = 1;
        preset.field_C  = 5;
        preset.field_10 = 0;
        func_actor_135600_801330A8(arg0, 0x7D3, &preset, 0);
        work->field_4F8 = 0;
        work->field_4FA = 0;
    }

    words          = (Actor135600MatrixWords*)&coord->coord;
    words->m00_m01 = ONE;
    words->m02_m10 = 0;
    words->m11_m12 = ONE;
    words->m20_m21 = 0;
    words->m22     = ONE;
    RotMatrix(&vec, &coord->coord);
    coord->flg = 0;
}

/// The 0x7D3 entry of `D_actor_135600_8013B0F4`, also called directly by the
/// setup handler and the arrival and turn steps. Re-seeds the 20-slot array off bank table `D_actor_135600_8013B0C0` when the
/// preset's bank index changes, then stores the animation id and restarts or
/// resets every slot with it and ticks them, latching `field_474`. The
/// animation is reinstalled even when the id is unchanged. Returns 0.
s32 func_actor_135600_801330A8(Task* task, s32 msgId, Actor135600AnimPreset* msg, s32 arg3)
{
    Actor135600Work* work;
    TmdObject*       ext;
    s32              i;

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

/// The 0x7D4 entry of `D_actor_135600_8013B0F4`, also called by the setup
/// handler: drops the translation straight into the root part's local matrix, stores
/// the Euler angles in the coordinate's own `rot` slot and rebuilds the
/// rotation from them; clearing `flg` makes the world matrix be recomputed.
/// Returns 0.
s32 func_actor_135600_801331C4(Task* task, s32 msgId, Actor135600PlaceArgs* args, s32 arg3)
{
    Actor135600Coord* coord;

    coord             = (Actor135600Coord*)((TmdObject*)task->extra)->coords;
    coord->coord.t[0] = args->pos.vx;
    coord->coord.t[1] = args->pos.vy;
    coord->coord.t[2] = args->pos.vz;
    coord->rot.vx     = args->rot.vx;
    coord->rot.vy     = args->rot.vy;
    coord->rot.vz     = args->rot.vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg = 0;
    return 0;
}

/// The 0x7D5 entry of `D_actor_135600_8013B0F4`, the actor's visibility,
/// switched on the word `mode`. Flag 0x80 hides the model (the tick skips the
/// shadow while it is set). Mode 0 hides the model and clears flag 4, 1 shows
/// it, allocates its buffers and clears 4, 2 hides it, sets 4 and starts the
/// `field_508` countdown at 2, and 3 shows it while setting 4. Anything else
/// returns 1 and leaves the flags alone; the handled modes return 0. Either
/// way the resulting flags are copied onto the objects of the three tasks the
/// setup state parked at `field_4FC` / `field_500` / `field_504`.
s32 func_actor_135600_80133240(Task* task, s32 msgId, s32 mode, s32 arg3)
{
    Actor135600Work* work;
    TmdObject*       obj;
    TmdObject*       objA;
    TmdObject*       objB;
    TmdObject*       objC;
    s32              ret;

    work = (Actor135600Work*)task->work;
    obj  = task->extra;
    objB = work->field_500->extra;
    objA = work->field_4FC->extra;
    objC = work->field_504->extra;
    ret  = 0;
    switch (mode) {
        case 0:
            obj->flags |= 0x80;
            obj->flags &= ~4;
            break;
        case 1:
            obj->flags &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~4;
            break;
        case 2:
            obj->flags     |= 0x80;
            work->field_508 = mode;
            obj->flags     |= 4;
            break;
        case 3:
            obj->flags &= ~0x80;
            obj->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    objB->flags = obj->flags;
    objA->flags = obj->flags;
    objC->flags = obj->flags;
    return ret;
}

/// The 0x7DB entry of `D_actor_135600_8013B0F4`: accepts the message and does
/// nothing with it.
s32 func_actor_135600_8013336C(void)
{
    return 0;
}
