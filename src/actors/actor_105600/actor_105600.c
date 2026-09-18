#include "common.h"

#include <psyq/inline_c.h>

#include "actors/actor_105600.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

/* `gte_MulMatrix0` from `psyq/gtemac.h`, except with the real `rtv0` / `rtir`
 * encodings this toolchain assembles correctly. */
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_rtir_real() __asm__ volatile("nop; nop; .word 0x4A49E012")

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_105600_80131EA0;

/// Nonzero parks the actor instead of running its state machine: 1 draws the
/// body where it stands and 2 fades it out.
extern u8 D_801153F4;

/// Frame counts of the actor's animations, indexed by `Actor105600Work.field_694`.
extern s16 D_actor_105600_80136B1C[];

/// The approach cycle's per-state handlers, indexed by `Actor105600Work.field_6A6`.
extern void (*D_actor_105600_80148360[])(Task*);

void Gp_AnimTickIndex(GpAnimCtx* arg0, s32 arg1);
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

void func_actor_105600_80131EC4(Task* arg0);
void func_actor_105600_80133358(Task* arg0);
void func_actor_105600_801334E4(Task* arg0);
void func_actor_105600_801336F0(Task* arg0);

INCLUDE_RODATA("actors/nonmatchings/actor_105600/actor_105600", D_actor_105600_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_105600/actor_105600", func_actor_105600_80131EC4);

INCLUDE_ASM("actors/nonmatchings/actor_105600/actor_105600", func_actor_105600_80132938);

INCLUDE_ASM("actors/nonmatchings/actor_105600/actor_105600", func_actor_105600_80132B1C);

INCLUDE_ASM("actors/nonmatchings/actor_105600/actor_105600", func_actor_105600_80132C58);

INCLUDE_ASM("actors/nonmatchings/actor_105600/actor_105600", func_actor_105600_80133034);

INCLUDE_ASM("actors/nonmatchings/actor_105600/actor_105600", func_actor_105600_8013312C);

INCLUDE_ASM("actors/nonmatchings/actor_105600/actor_105600", func_actor_105600_80133358);

INCLUDE_ASM("actors/nonmatchings/actor_105600/actor_105600", func_actor_105600_801334E4);

INCLUDE_ASM("actors/nonmatchings/actor_105600/actor_105600", func_actor_105600_801336F0);

INCLUDE_ASM("actors/nonmatchings/actor_105600/actor_105600", func_actor_105600_8013386C);

INCLUDE_ASM("actors/nonmatchings/actor_105600/actor_105600", func_actor_105600_80133C3C);

INCLUDE_RODATA("actors/nonmatchings/actor_105600/actor_105600", D_actor_105600_80131EA0);

INCLUDE_ASM("actors/nonmatchings/actor_105600/actor_105600", func_actor_105600_801341C0);

INCLUDE_ASM("actors/nonmatchings/actor_105600/actor_105600", func_actor_105600_80134368);

INCLUDE_ASM("actors/nonmatchings/actor_105600/actor_105600", func_actor_105600_80134770);

/// Places a fresh body block for the actor: allocates the 0xF0-byte work
/// block, builds the root coordinate by rotating the local spawn offset through
/// the parent coordinate and re-aiming it, then links the three collision
/// bodies and their `GpRec18` tables onto the model root and hands the light /
/// colour matrices to its `TmdObject`. The sound cue that marks the placement
/// packs the room/channel bits of the spawn context into
/// `D_actor_105600_80148110`.
void func_actor_105600_80134FD0(GpEnemy* arg0, Task* arg1)
{
    Actor105600FxWork*       work;
    Actor105600PlaceScratch* scratch;
    Actor105600Ctx*          ctx;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           parentCoord;
    TmdObject*               tmd;
    Task*                    parent;
    s32                      sound;
    s32                      pan;

    tmd         = arg1->extra;
    coord       = tmd->field_8;
    parent      = arg1->parent;
    parentCoord = ((TmdObject*)parent->extra)->field_8;
    work        = Mem_Calloc(0xF0, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->idMap   = (TaskIdMap*)work;
    tmd->field_C  = 0;
    scratch       = (Actor105600PlaceScratch*)(*(u8**)G_SCRATCH_HEAD -= 0x38);
    tmd->field_1C = &work->lightMtx;
    tmd->field_20 = &work->colorMtx;

    Gfx_ViewCoord.flg = 0;
    Gp_UpdateCoord(&Gfx_ViewCoord);
    parentCoord->flg = 0;
    Gp_UpdateCoord(parentCoord);
    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &parentCoord->workm, &coord->coord);

    scratch->rot.vx = 0;
    scratch->rot.vy = 0x1F4;
    scratch->rot.vz = 0x64;
    gte_SetRotMatrix(&coord->coord);
    gte_ldv0(&scratch->rot);
    gte_rtv0_real();
    gte_stlvnl(&scratch->pos);
    coord->sub         = &Gfx_ViewCoord;
    coord->coord.t[0] += scratch->pos.vx;
    coord->coord.t[1] += scratch->pos.vy;
    coord->coord.t[2] += scratch->pos.vz;

    scratch->rot.vx = 0x80;
    scratch->rot.vy = 0;
    scratch->rot.vz = 0x10;
    RotMatrix(&scratch->rot, &scratch->mtx);
    gte_SetRotMatrix(&coord->coord);
    gte_ldclmv(&scratch->mtx);
    gte_rtir_real();
    gte_stclmv(&coord->coord);
    gte_ldclmv(&scratch->mtx.m[0][1]);
    gte_rtir_real();
    gte_stclmv(&coord->coord.m[0][1]);
    gte_ldclmv(&scratch->mtx.m[0][2]);
    gte_rtir_real();
    gte_stclmv(&coord->coord.m[0][2]);

    work->field_EE = (D_actor_105600_80147FDC.field_E != 1);

    work->obj40.field_8  = coord;
    work->obj40.field_C  = work->rec60;
    work->obj40.field_10 = 0;
    work->obj40.field_12 = 0;
    work->obj40.field_14 = 0;
    work->obj40.field_18 = Gp_PackPair(&D_actor_105600_80147FDC.pair, 3);
    work->obj40.field_1C = 0x64;
    work->obj40.flags    = 1;
    Gp_LinkObj(3, &work->obj40);
    Gp_InitRec18Table(work->rec60, 1, 0);
    work->obj40.flags |= 0x8000;

    work->obj78.field_8  = coord;
    work->obj78.field_C  = work->rec60;
    work->obj78.field_10 = 0;
    work->obj78.field_12 = 0;
    work->obj78.field_14 = 0;
    work->obj78.field_18 = 0x22B2B;
    work->obj78.field_1C = 0x64;
    work->obj78.flags    = 1;
    Gp_LinkObj(1, &work->obj78);
    work->obj78.flags |= 0x8000;

    work->d4rec.field_0  = 0;
    work->d4rec.field_2  = 0;
    work->d4rec.field_4  = 0;
    work->d4rec.field_8  = 0;
    work->d4rec.field_A  = -0x1F4;
    work->d4rec.field_C  = 0;
    work->d4rec.field_10 = 1;
    work->d4rec.field_12 = 1;
    work->d4rec.field_14 = work->recD0;
    work->obj98.field_C  = (GpRec18*)&work->d4rec;
    work->obj98.field_8  = coord;
    work->obj98.field_10 = 0;
    work->obj98.field_12 = 0;
    work->obj98.field_14 = 0;
    work->obj98.field_18 = 0;
    work->obj98.field_1C = 0;
    work->obj98.flags    = 3;
    Gp_LinkObj(3, &work->obj98);
    Gp_InitRec18Table(work->recD0, 1, 0);
    work->obj98.flags |= 0x4400;

    arg1->state = 1;
    Task_DetachFromParent(arg1);

    coord->flg = 0;
    Gp_UpdateCoord(coord);

    ctx   = arg1->spawnArg2;
    sound = D_actor_105600_80148110 | (((u16)ctx->field_8 >> 0xC) << 8);
    pan   = (s8)Gp_GetObjPan((GpObj38*)coord);
    SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));

    *(u8**)G_SCRATCH_HEAD += 0x38;
}

INCLUDE_ASM("actors/nonmatchings/actor_105600/actor_105600", func_actor_105600_80135410);

INCLUDE_ASM("actors/nonmatchings/actor_105600/actor_105600", func_actor_105600_80135744);

/// Per-frame tick of the approach cycle: runs the collision and state handlers,
/// drifts the root coordinate forward along its Z axis (and upward while
/// `field_6DE` is below 2), reseeds or ticks the nineteen animation slots, then
/// publishes the body's colour and its ground shadow. `D_801153F4` overrides
/// the whole state machine - 1 draws the body without advancing it and 2 parks
/// it faded out. The same body as `Actor02000_Fn02A34` of `actor_102000`.
void func_actor_105600_80135CDC(GpEnemy* ctx, Task* actor)
{
    VECTOR3          pos;
    Actor105600Ctx*  spawn;
    TmdObject*       model;
    Actor105600Work* moveWork;
    Actor105600Work* animWork;
    Actor105600Work* work;
    Actor105600Work* flagWork;
    GsCOORDINATE2*   moveCoord;
    GsCOORDINATE2*   part;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   root;
    s16              duration;
    s32              i;
    u8               flags;

    work  = (Actor105600Work*)actor->idMap;
    model = (TmdObject*)actor->extra;
    coord = model->field_8;
    switch (D_801153F4) {
        case 0:
            model->field_C    = 0;
            ctx->node.field_4 = 0;
            break;
        case 1:
            goto draw;
        case 2:
            model->field_C    = 0x80;
            ctx->node.field_4 = 1;
            return;
    }

    if (ctx->field_4C != 0) {
        spawn    = (Actor105600Ctx*)actor->spawnArg2;
        flags    = spawn->field_4C;
        flagWork = (Actor105600Work*)actor->idMap;
        if ((flags & 2) && (flagWork->field_6B8 == 0)) {
            spawn->field_4C     = flags & 0xFD;
            flagWork->field_6A6 = 0xA;
            flagWork->field_694 = 0x14;
            flagWork->field_6A8 = 0;
            flagWork->field_6E0 = 1;
        }
    }
    func_actor_105600_80131EC4(actor);
    D_actor_105600_80148360[work->field_6A6](actor);
    if (work->field_69E != 0) {
        func_actor_105600_80133358(actor);
    }
    moveCoord              = ((TmdObject*)actor->extra)->field_8;
    moveWork               = (Actor105600Work*)actor->idMap;
    moveWork->field_678    = moveCoord->coord.t[0];
    moveWork->field_67C    = moveCoord->coord.t[1];
    moveWork->field_680    = moveCoord->coord.t[2];
    moveCoord->coord.t[0] += (s32)(moveCoord->coord.m[0][2] * moveWork->field_69C) >> 0xC;
    if (moveWork->field_6DE < 2) {
        moveCoord->coord.t[1] += 0x80;
    }
    moveCoord->coord.t[2] += (s32)(moveCoord->coord.m[2][2] * moveWork->field_69C) >> 0xC;
    animWork               = (Actor105600Work*)actor->idMap;
    i                      = 1;
    if (animWork->field_694 != animWork->field_696) {
        animWork->field_696 = (s16)(u16)animWork->field_694;
        animWork->field_698 = 0;
        duration            = D_actor_105600_80136B1C[animWork->field_694];
        do {
            func_800B4114(&animWork->ctx, i, animWork->field_694, 0, duration);
            i += 1;
        } while (i < 0x13);
    } else {
        TOUCH_REG(i);
        animWork->field_698 = (u16)animWork->field_698 + i;
        do {
            Gp_AnimTickIndex(&animWork->ctx, i);
            i += 1;
        } while (i < 0x13);
    }
    if (work->field_6B4 != 0) {
        func_actor_105600_801334E4(actor);
    }
    func_actor_105600_801336F0(actor);
    coord->flg                                 = 0;
    ((TmdObject*)actor->extra)->field_8[3].flg = 0;
    Gp_UpdateCoord(coord);
draw:
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor((GpEnemy*)actor->spawnArg2, (VECTOR*)&pos, 0, 0);
    root   = ((TmdObject*)actor->extra)->field_8;
    part   = root + 3;
    pos.vx = part->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = part->workm.t[2];
    Gp_DrawEffGroundQuad(&pos, 0x300, 0x80);
}

INCLUDE_ASM("actors/nonmatchings/actor_105600/actor_105600", func_actor_105600_80136004);
