#include "common.h"

#include "decomp/common.h"

#include "actors/actor_135600.h"
#include "actors/actors_shared_801327b4.h"
#include "actors/actors_shared_801327f8.h"
#include "actors/actors_shared_80132f24.h"

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

/// The actor's message table, stored in `Task::field_24`: 0x7D3, 0x7D4 and
/// 0x7D5 against the handlers below.
extern GpMsgEntry D_actor_135600_8013B0F4[];

/// Declared locally rather than taken from `actors_shared_8013231c.h`: the
/// overlay calls it through its message dispatch, which passes a fourth,
/// unused argument.
s32 ActorsShared8013231c(Task* task, s32 msgId, Actor135600PlaceArgs* args, s32 arg3);

/// The 0x7D3 entry of `D_actor_135600_8013B0F4`, applied to the work block at
/// 0x50C in `actor_135600_4`.
s32 func_actor_135600_801330A8(Task* task, s32 msgId, Actor135600AnimPreset* preset, s32 arg3);

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
        poly           = (POLY_F4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(poly + 1);
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
        addPrim((u_long*)(((((u32)depth << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt) - 2, poly);

        tpage          = Gpu_PrimCursor;
        Gpu_PrimCursor = tpage + 1;
        setlen(tpage, 1);
        tpage->code[0] = 0xE1000465;
        addPrim((u_long*)(((((u32)depth << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt) - 2, tpage);
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
    GpCdRec10*            entry1;
    GpCdRec10*            entry2;
    u32                   index1;
    u32                   index2;
    u32                   raw1;
    u32                   raw2;
    u8                    areaByte0;
    u8                    areaByte1;
    Actor135600PlaceArgs  args;
    Actor135600AnimPreset preset;
    GpAreaKey             key;

    work = (Actor135600Work*)Mem_Calloc(0x50C, false);
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
        raw1            = ((GpEnemy*)task->spawnArg2)->field_8;
        sessionKey1     = (GpAreaKey*)&gGameSession->loc;
        key.field_3     = sessionKey1->field_3;
        key.field_2     = sessionKey1->field_2;
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
        keyp1       = &key;
        key.field_1 = sessionKey1->field_1;
        TOUCH_REG(keyp1);
        areaByte0   = gGameSession->loc.view;
        index1      = raw1 >> 12;
        key.field_0 = areaByte0;
        Gp_SyncAreaKeyIndex(keyp1);
        entry1           = (GpCdRec10*)((index1 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model1->field_24 = entry1->field_D;
        model1->field_25 = entry1->field_E;
        if (model1->field_18 != NULL) {
            Tmd_ProcessStream(model1);
            Tmd_ProcessStream(model1);
        }
    }

    spawned = Task_SpawnFromTable(&D_actor_135600_8013B0C4, 2, 0xC, (s32)task);
    if (spawned != NULL) {
        work->field_4FC = spawned;
        model2          = (TmdObject*)spawned->extra;
        raw2            = ((GpEnemy*)task->spawnArg2)->field_8;
        sessionKey2     = (GpAreaKey*)&gGameSession->loc;
        key.field_3     = sessionKey2->field_3;
        key.field_2     = sessionKey2->field_2;
        SOFT_BARRIER();
        keyp2       = &key;
        key.field_1 = sessionKey2->field_1;
        TOUCH_REG(keyp2);
        areaByte1   = gGameSession->loc.view;
        index2      = raw2 >> 12;
        key.field_0 = areaByte1;
        Gp_SyncAreaKeyIndex(keyp2);
        entry2           = (GpCdRec10*)((index2 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model2->field_24 = entry2->field_D;
        model2->field_25 = entry2->field_E;
        if (model2->field_18 != NULL) {
            Tmd_ProcessStream(model2);
            Tmd_ProcessStream(model2);
        }
    }

    spawned = Task_SpawnFromTable(&D_actor_135600_8013B0C4, 3, 8, (s32)task);
    if (spawned != NULL) {
        work->field_504 = spawned;
    }

    ActorsShared80132f24(task);

    args.pos.vx = 0xA6E;
    args.pos.vz = 0x5F0;
    args.pos.vy = 0;
    args.rot.vx = 0;
    args.rot.vy = 0x400;
    args.rot.vz = 0;
    ActorsShared8013231c(task, 0x7D4, &args, 0);

    preset.field_0 = 0;
    preset.field_4 = 2;
    preset.field_8 = 0;
    func_actor_135600_801330A8(task, 0x7D3, &preset, 0);

    func_actor_135600_80133240(task, 0x7D5, 1, 0);

    task->field_24     = D_actor_135600_8013B0F4;
    task->exitCallback = ActorsShared801327b4;
    task->state       += 1;
}

void func_actor_135600_801324D0(Task* arg0)
{
    TmdObject*       ext      = arg0->extra;
    Actor135600Work* work     = (Actor135600Work*)arg0->work;
    TaskFunc         funcs[2] = { (TaskFunc)func_actor_135600_80132DF8, ActorsShared801327f8 };
    VECTOR3          pos;
    GsCOORDINATE2*   coord;
    s32              i;

    if (!(ext->field_C & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)arg0->extra)->field_8[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x300, Gp_State1C->field_8);
        }
    }
    if (D_801153F4 == 0) {
        funcs[work->field_4F8](arg0);
        coord              = ((TmdObject*)arg0->extra)->field_8;
        work->field_4D8   += work->field_4C8;
        work->field_4DC   += work->field_4CC;
        work->field_4E0   += work->field_4D0;
        coord->coord.t[0] += (s16)(work->field_4D8 >> 16);
        coord->coord.t[1] += (s16)(work->field_4DC >> 16);
        coord->coord.t[2] += (s16)(work->field_4E0 >> 16);
        coord->flg         = 0;
        work->field_4D8    = (u16)work->field_4D8;
        work->field_4DC    = (u16)work->field_4DC;
        work->field_4E0    = (u16)work->field_4E0;
        if (work->field_474 != 0) {
            for (i = 1; i < 0x14; i++) {
                Gp_AnimTickIndex((GpAnimCtx*)work, i);
            }
        }
        if (gGameSession->field_4D != 0) {
            ((TmdObject*)arg0->extra)->field_8[1].flg = 0;
            Gp_UpdateCoord(&((TmdObject*)arg0->extra)->field_8[1]);
            func_800D7A9C(ext, (VECTOR*)((TmdObject*)arg0->extra)->field_8[1].workm.t, 0, 3);
        }
        if (work->field_508 >= 0) {
            if (work->field_508 == 0) {
                Tmd_FreeBuffers(ext);
            }
            work->field_508--;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_135600/actor_135600", func_actor_135600_801326E8);

INCLUDE_ASM("actors/nonmatchings/actor_135600/actor_135600", func_actor_135600_8013282C);

INCLUDE_RODATA("actors/nonmatchings/actor_135600/actor_135600", D_actor_135600_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_135600/actor_135600", D_actor_135600_80131E24);

void func_actor_135600_801329E0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135600_80131E24;
    sp.funcs[task->state](task);
}

INCLUDE_RODATA("actors/nonmatchings/actor_135600/actor_135600", D_actor_135600_80131E30);

INCLUDE_RODATA("actors/nonmatchings/actor_135600/actor_135600", D_actor_135600_80131E3C);
