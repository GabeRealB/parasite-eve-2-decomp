#include "common.h"

#include <psyq/inline_c.h>

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/tmd.h"
#include "weapons/gunblade.h"

void func_gunblade_8011D1E4(Task* task)
{
    GsCOORDINATE2  local;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* dst;
    GpEffWork*     work;
    GpEffWork*     eff;
    s32            keep;
    SVECTOR*       vec;
    s32            i;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->field_8;
    if (Gp_State1C->field_4 != 0) {
        keep = Gp_State1C->field_4 < 4;
    } else {
        work->field_22++;
        switch (task->state) {
            case 0:
                coord->sub          = work->field_8;
                coord->coord.t[0]   = D_gunblade_8011E704[0].vx;
                D_gunblade_8012E244 = task;
                coord->coord.t[1]   = D_gunblade_8011E704[0].vy;
                D_gunblade_8012E248 = work;
                coord->coord.t[2]   = D_gunblade_8011E704[0].vz;
                coord->flg          = 0;
                Gp_UpdateCoord(coord);
                task->state      = 1;
                vec              = &D_gunblade_8011E704[1];
                local.sub        = work->field_8;
                local.coord.t[0] = vec->vx;
                local.coord.t[1] = vec->vy;
                local.coord.t[2] = vec->vz;
                local.flg        = 0;
                Gp_UpdateCoord(&local);
                for (i = 0; i < 8; i++) {
                    dst        = &D_gunblade_8012E254[i];
                    dst->sub   = &Gfx_ViewCoord;
                    dst->workm = coord->workm;
                    gte_SetRotMatrix(&coord->workm);
                    gte_SetTransMatrix(&coord->workm);
                    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &dst->workm, &dst->coord);
                    dst        = &D_gunblade_8012E4D4[i];
                    dst->sub   = &Gfx_ViewCoord;
                    dst->workm = local.workm;
                    gte_SetRotMatrix(&local.workm);
                    gte_SetTransMatrix(&local.workm);
                    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &dst->workm, &dst->coord);
                }
                return;
            case 1:
                coord->flg = 0;
                Gp_UpdateCoord(coord);
                local.sub        = work->field_8;
                local.coord.t[0] = D_gunblade_8011E70C.vx;
                local.coord.t[1] = D_gunblade_8011E70C.vy;
                local.coord.t[2] = D_gunblade_8011E70C.vz;
                local.flg        = 0;
                Gp_UpdateCoord(&local);
                dst        = &D_gunblade_8012E254[work->field_22 & 7];
                dst->sub   = &Gfx_ViewCoord;
                dst->workm = coord->workm;
                gte_SetRotMatrix(&coord->workm);
                gte_SetTransMatrix(&coord->workm);
                Gp_WorldToLocal(&Gfx_ViewCoord.workm, &dst->workm, &dst->coord);
                dst        = &D_gunblade_8012E4D4[work->field_22 & 7];
                dst->sub   = &Gfx_ViewCoord;
                dst->workm = local.workm;
                gte_SetRotMatrix(&local.workm);
                gte_SetTransMatrix(&local.workm);
                Gp_WorldToLocal(&Gfx_ViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst      = &D_gunblade_8012E254[i];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                    dst      = &D_gunblade_8012E4D4[i];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                }
                if (work->field_22 < 9) {
                    func_gunblade_8011D70C(work->field_22 & 7, 0x112);
                    return;
                }
                if (work->field_20 == 1) {
                    work->field_20++;
                    eff = Gp_SpawnEff(0x6029A, coord, task->spawnArg1, NULL);
                    if (eff != NULL) {
                        Task_Reparent(task, eff->field_0);
                    }
                }
                func_gunblade_8011D70C(work->field_22 & 7, 0x331);
                keep = work->field_22 < 0xD;
                break;
            default:
                return;
        }
    }
    if (!keep) {
        D_gunblade_8012E248 = NULL;
        Gp_ReleaseState1CMem(work, task);
    }
}

INCLUDE_ASM("weapons/nonmatchings/gunblade/gunblade", func_gunblade_8011D70C);

INCLUDE_ASM("weapons/nonmatchings/gunblade/gunblade", func_gunblade_8011DAA4);

void func_gunblade_8011E008(s32 arg0)
{
    GpEffWork* work = D_gunblade_8012E248;

    if (work != NULL) {
        D_gunblade_8012E244->spawnArg1 = arg0;
        work->field_20++;
    }
}

INCLUDE_RODATA("weapons/nonmatchings/gunblade/gunblade", D_gunblade_8011D1C0);

INCLUDE_ASM("weapons/nonmatchings/gunblade/gunblade", func_gunblade_8011E040);
