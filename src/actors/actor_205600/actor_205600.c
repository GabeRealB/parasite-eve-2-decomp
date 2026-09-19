#include "common.h"

#include <psyq/inline_c.h>

#include "actors/actor_205600.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

/* `gte_MulMatrix0` from `psyq/gtemac.h`, except with the real `rtv0` / `rtir`
 * encodings this toolchain assembles correctly. */
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_rtir_real() __asm__ volatile("nop; nop; .word 0x4A49E012")

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_205600_80149EA0;

INCLUDE_RODATA("actors/nonmatchings/actor_205600/actor_205600", D_actor_205600_80149E20);

INCLUDE_ASM("actors/nonmatchings/actor_205600/actor_205600", func_actor_205600_80149EC4);

INCLUDE_ASM("actors/nonmatchings/actor_205600/actor_205600", func_actor_205600_8014A938);

INCLUDE_ASM("actors/nonmatchings/actor_205600/actor_205600", func_actor_205600_8014AB1C);

INCLUDE_ASM("actors/nonmatchings/actor_205600/actor_205600", func_actor_205600_8014AC58);

INCLUDE_ASM("actors/nonmatchings/actor_205600/actor_205600", func_actor_205600_8014B034);

INCLUDE_ASM("actors/nonmatchings/actor_205600/actor_205600", func_actor_205600_8014B12C);

INCLUDE_ASM("actors/nonmatchings/actor_205600/actor_205600", func_actor_205600_8014B358);

INCLUDE_ASM("actors/nonmatchings/actor_205600/actor_205600", func_actor_205600_8014B4E4);

INCLUDE_ASM("actors/nonmatchings/actor_205600/actor_205600", func_actor_205600_8014B6F0);

INCLUDE_ASM("actors/nonmatchings/actor_205600/actor_205600", func_actor_205600_8014B86C);

INCLUDE_ASM("actors/nonmatchings/actor_205600/actor_205600", func_actor_205600_8014BC3C);

INCLUDE_RODATA("actors/nonmatchings/actor_205600/actor_205600", D_actor_205600_80149EA0);

INCLUDE_ASM("actors/nonmatchings/actor_205600/actor_205600", func_actor_205600_8014C1C0);

INCLUDE_ASM("actors/nonmatchings/actor_205600/actor_205600", func_actor_205600_8014C368);

INCLUDE_ASM("actors/nonmatchings/actor_205600/actor_205600", func_actor_205600_8014C770);

/// Places a fresh body block for the actor: allocates the 0xF0-byte work
/// block, builds the root coordinate by rotating the local spawn offset through
/// the parent coordinate and re-aiming it, then links the three collision
/// bodies and their `GpRec18` tables onto the model root and hands the light /
/// colour matrices to its `TmdObject`. The sound cue that marks the placement
/// packs the room/channel bits of the spawn context into
/// `D_actor_205600_80160110`.
void func_actor_205600_8014CFD0(GpEnemy* arg0, Task* arg1)
{
    Actor205600FxWork*       work;
    Actor205600PlaceScratch* scratch;
    Actor205600Ctx*          ctx;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           parentCoord;
    TmdObject*               tmd;
    Task*                    parent;
    s32                      sound;
    s32                      pan;

    tmd         = arg1->extra;
    coord       = tmd->coords;
    parent      = arg1->parent;
    parentCoord = ((TmdObject*)parent->extra)->coords;
    work        = memCalloc(0xF0, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work    = (TaskIdMap*)work;
    tmd->flags    = 0;
    scratch       = (Actor205600PlaceScratch*)(*(u8**)G_SCRATCH_HEAD -= 0x38);
    tmd->lightMtx = &work->lightMtx;
    tmd->colorMtx = &work->colorMtx;

    gGfxViewCoord.flg = 0;
    Gp_UpdateCoord(&gGfxViewCoord);
    parentCoord->flg = 0;
    Gp_UpdateCoord(parentCoord);
    Gp_WorldToLocal(&gGfxViewCoord.workm, &parentCoord->workm, &coord->coord);

    scratch->rot.vx = 0;
    scratch->rot.vy = 0x1F4;
    scratch->rot.vz = 0x64;
    gte_SetRotMatrix(&coord->coord);
    gte_ldv0(&scratch->rot);
    gte_rtv0_real();
    gte_stlvnl(&scratch->pos);
    coord->sub         = &gGfxViewCoord;
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

    work->field_EE = (D_actor_205600_8015FFDC.field_E != 1);

    work->obj40.coord    = coord;
    work->obj40.ctx.recs = work->rec60;
    work->obj40.pos.vx   = 0;
    work->obj40.pos.vy   = 0;
    work->obj40.pos.vz   = 0;
    work->obj40.key      = Gp_PackPair(&D_actor_205600_8015FFDC.pair, 3);
    work->obj40.radius   = 0x64;
    work->obj40.flags    = 1;
    Gp_LinkObj(3, &work->obj40);
    Gp_InitRec18Table(work->rec60, 1, 0);
    work->obj40.flags |= 0x8000;

    work->obj78.coord    = coord;
    work->obj78.ctx.recs = work->rec60;
    work->obj78.pos.vx   = 0;
    work->obj78.pos.vy   = 0;
    work->obj78.pos.vz   = 0;
    work->obj78.key      = 0x22B2B;
    work->obj78.radius   = 0x64;
    work->obj78.flags    = 1;
    Gp_LinkObj(1, &work->obj78);
    work->obj78.flags |= 0x8000;

    work->d4rec.end0.vx    = 0;
    work->d4rec.end0.vy    = 0;
    work->d4rec.end0.vz    = 0;
    work->d4rec.end1.vx    = 0;
    work->d4rec.end1.vy    = -0x1F4;
    work->d4rec.end1.vz    = 0;
    work->d4rec.end0Radius = 1;
    work->d4rec.end1Radius = 1;
    work->d4rec.recs       = work->recD0;
    work->obj98.ctx.d4rec  = &work->d4rec;
    work->obj98.coord      = coord;
    work->obj98.pos.vx     = 0;
    work->obj98.pos.vy     = 0;
    work->obj98.pos.vz     = 0;
    work->obj98.key        = 0;
    work->obj98.radius     = 0;
    work->obj98.flags      = 3;
    Gp_LinkObj(3, &work->obj98);
    Gp_InitRec18Table(work->recD0, 1, 0);
    work->obj98.flags |= 0x4400;

    arg1->state = 1;
    Task_DetachFromParent(arg1);

    coord->flg = 0;
    Gp_UpdateCoord(coord);

    ctx   = arg1->spawnArg2;
    sound = D_actor_205600_80160110 | (((u16)ctx->field_8 >> 0xC) << 8);
    pan   = (s8)Gp_GetObjPan(coord);
    SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));

    *(u8**)G_SCRATCH_HEAD += 0x38;
}

INCLUDE_ASM("actors/nonmatchings/actor_205600/actor_205600", func_actor_205600_8014D410);

INCLUDE_ASM("actors/nonmatchings/actor_205600/actor_205600", func_actor_205600_8014D744);

INCLUDE_ASM("actors/nonmatchings/actor_205600/actor_205600", func_actor_205600_8014DCDC);

INCLUDE_ASM("actors/nonmatchings/actor_205600/actor_205600", func_actor_205600_8014E004);
