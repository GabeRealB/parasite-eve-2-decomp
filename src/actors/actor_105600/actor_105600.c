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

INCLUDE_ASM("actors/nonmatchings/actor_105600/actor_105600", func_actor_105600_80135CDC);

INCLUDE_ASM("actors/nonmatchings/actor_105600/actor_105600", func_actor_105600_80136004);
