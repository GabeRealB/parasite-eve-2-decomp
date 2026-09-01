#include "common.h"

#include <psyq/inline_c.h>

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/tmd.h"
#include "weapons/m4a1_grenade.h"

/// `mvmva 1, 0, 0, 0, 0`: rotate V0 by the rotation matrix and add the
/// translation vector. The `inline_c.h` macro of that name assembles to a
/// different word, so spell the instruction out.
#define gte_rtv0tr_real() __asm__ volatile("nop; nop; .word 0x4A480012")

void WeaponsShared8011de24(Task* task);

INCLUDE_RODATA("weapons/nonmatchings/m4a1_grenade/m4a1_grenade", D_m4a1_grenade_8011D1C0);

INCLUDE_ASM("weapons/nonmatchings/m4a1_grenade/m4a1_grenade", func_m4a1_grenade_8011D1EC);

/// Spawn state: allocates the grenade's work block, places the projectile a
/// little above and in front of the muzzle coordinate, parents it to world,
/// and links its two collision nodes.
void func_m4a1_grenade_8011D654(Task* arg0)
{
    void**           scratch;
    u8*              head;
    SVECTOR*         blk;
    SVECTOR*         vec;
    MATRIX*          mtx;
    TmdObject*       extra;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   muzzle;
    M4a1GrenadeWork* work;

    scratch  = (void**)G_SCRATCH_HEAD;
    extra    = arg0->extra;
    head     = *scratch;
    coord    = extra->field_8;
    blk      = (SVECTOR*)(head - 0x28);
    *scratch = blk;
    muzzle   = coord->sub;
    work     = Mem_Calloc(sizeof(M4a1GrenadeWork), 0);
    vec      = blk;
    if (work == NULL) {
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x28;
        Task_Kill(arg0);
        return;
    }
    arg0->idMap        = (TaskIdMap*)work;
    arg0->exitCallback = WeaponsShared8011de24;
    arg0->state++;
    Mem_Set(work, 0, sizeof(M4a1GrenadeWork));
    blk->vx     = 0;
    blk->vy     = 0x220;
    blk->vz     = 0x28;
    muzzle->flg = 0;
    Gp_UpdateCoord(muzzle);
    coord->workm = muzzle->workm;
    gte_SetRotMatrix(&muzzle->workm);
    gte_SetTransMatrix(&muzzle->workm);
    gte_ldv0(vec);
    gte_rtv0tr_real();
    gte_stlvnl(coord->workm.t);
    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &coord->workm, &coord->coord);
    mtx            = (MATRIX*)(head - 0x20);
    coord->sub     = &Gfx_ViewCoord;
    coord->flg     = 0;
    extra->field_C = 0;
    *mtx           = coord->coord;
    Gfx_RotMatrixX(mtx, -0x400, 0);
    Gfx_MatrixCol2(mtx, &work->dir);
    VectorNormalSS(&work->dir, &work->dir);
    work->field_88     = 0xA0000;
    work->field_8C     = 1;
    work->field_90     = 0;
    work->obj.field_8  = coord;
    work->obj.field_C  = work->rec0;
    work->obj.field_10 = 0;
    work->obj.field_12 = 0;
    work->obj.field_14 = 0;
    work->obj.field_18 = (u16)arg0->spawnArg1 | 0x20000;
    work->obj.field_1C = 0x94;
    work->obj.flags    = 1;
    Gp_LinkObj(1, &work->obj);
    Gp_InitRec18Table(work->obj.field_C, 1, 0);
    work->obj2.field_C   = (GpRec18*)&work->d4rec;
    work->obj2.flags     = 3;
    work->d4rec.field_14 = work->rec1;
    work->obj2.field_8   = coord;
    work->obj2.field_10  = 0;
    work->obj2.field_12  = 0;
    work->obj2.field_14  = 0;
    work->obj2.field_18  = 0;
    work->obj2.field_1C  = 0;
    work->d4rec.field_0  = 0;
    work->d4rec.field_2  = 0;
    work->d4rec.field_4  = 0;
    work->d4rec.field_8  = 0;
    work->d4rec.field_C  = 0;
    work->d4rec.field_10 = 1;
    work->d4rec.field_12 = 1;
    work->obj.flags     |= 0xC400;
    work->d4rec.field_A  = -(work->field_88 >> 10);
    Gp_LinkObj(1, &work->obj2);
    Gp_InitRec18Table(work->d4rec.field_14, 1, 0);
    work->obj2.flags       |= 0x4400;
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x28;
}

INCLUDE_ASM("weapons/nonmatchings/m4a1_grenade/m4a1_grenade", func_m4a1_grenade_8011D994);
