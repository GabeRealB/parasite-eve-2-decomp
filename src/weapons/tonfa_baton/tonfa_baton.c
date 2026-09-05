#include "common.h"

#include <psyq/inline_c.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "weapons/tonfa_baton.h"

void WeaponsShared8011db78(Task* task);

void func_tonfa_baton_8011D1EC(Task* task)
{
    GsCOORDINATE2  local;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* dst;
    GpEffWork*     work;
    SVECTOR*       vec;
    s32            i;
    s32            flags;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->field_8;
    if (Gp_State1C->field_4 != 0) {
        if (Gp_State1C->field_4 >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        work->field_22++;
        switch (task->state) {
            case 0:
                coord->sub        = work->field_8;
                coord->coord.t[0] = D_tonfa_baton_8011E0F0[0].vx;
                coord->coord.t[1] = D_tonfa_baton_8011E0F0[0].vy;
                coord->coord.t[2] = D_tonfa_baton_8011E0F0[0].vz;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                task->state      = 1;
                vec              = &D_tonfa_baton_8011E0F0[1];
                local.sub        = coord;
                local.coord.t[0] = vec->vx;
                local.coord.t[1] = vec->vy;
                local.coord.t[2] = vec->vz;
                local.flg        = 0;
                Gp_UpdateCoord(&local);
                for (i = 0; i < 8; i++) {
                    dst        = &D_tonfa_baton_8012BBEC[i];
                    dst->sub   = &Gfx_ViewCoord;
                    dst->workm = coord->workm;
                    gte_SetRotMatrix(&coord->workm);
                    gte_SetTransMatrix(&coord->workm);
                    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &dst->workm, &dst->coord);
                    dst        = &D_tonfa_baton_8012BE6C[i];
                    dst->sub   = &Gfx_ViewCoord;
                    dst->workm = local.workm;
                    gte_SetRotMatrix(&local.workm);
                    gte_SetTransMatrix(&local.workm);
                    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &dst->workm, &dst->coord);
                }
                flags = 0x13;
                if (task->spawnArg1 == 0) {
                    flags = 1;
                }
                D_tonfa_baton_8012C0EC = flags;
                break;
            case 1:
                coord->flg = 0;
                Gp_UpdateCoord(coord);
                local.sub        = work->field_8;
                local.coord.t[0] = D_tonfa_baton_8011E0F8.vx;
                local.coord.t[1] = D_tonfa_baton_8011E0F8.vy;
                local.coord.t[2] = D_tonfa_baton_8011E0F8.vz;
                local.flg        = 0;
                Gp_UpdateCoord(&local);
                dst        = &D_tonfa_baton_8012BBEC[work->field_22 & 7];
                dst->sub   = &Gfx_ViewCoord;
                dst->workm = coord->workm;
                gte_SetRotMatrix(&coord->workm);
                gte_SetTransMatrix(&coord->workm);
                Gp_WorldToLocal(&Gfx_ViewCoord.workm, &dst->workm, &dst->coord);
                dst        = &D_tonfa_baton_8012BE6C[work->field_22 & 7];
                dst->sub   = &Gfx_ViewCoord;
                dst->workm = local.workm;
                gte_SetRotMatrix(&local.workm);
                gte_SetTransMatrix(&local.workm);
                Gp_WorldToLocal(&Gfx_ViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst      = &D_tonfa_baton_8012BBEC[i];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                    dst      = &D_tonfa_baton_8012BE6C[i];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                }
                func_tonfa_baton_8011D6B0(work->field_22 & 7, D_tonfa_baton_8012C0EC);
                break;
        }
        if (work->field_22 >= 0x1F) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

INCLUDE_ASM("weapons/nonmatchings/tonfa_baton/tonfa_baton", func_tonfa_baton_8011D6B0);

void func_tonfa_baton_8011DA48(Task* arg0)
{
    TmdObject* extra;
    s32*       ptr;

    extra              = (TmdObject*)arg0->extra;
    ptr                = extra->field_8;
    arg0->state        = arg0->state + 1;
    arg0->exitCallback = WeaponsShared8011db78;
    *ptr               = 0;
    extra->field_C     = 0;
}

void func_tonfa_baton_8011DA74(Task* arg0)
{
    TmdObject*  extra;
    TonfaCoord* coord;
    GameActor*  actor;
    s32         mode;

    extra          = (TmdObject*)arg0->extra;
    coord          = (TonfaCoord*)extra->field_8;
    actor          = ((GpActorWork*)Game_GetPtrSlot(3))->actor;
    coord->flg     = 0;
    extra->field_C = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_C;

    coord->coord.t[0] = 0;
    coord->coord.t[1] = 0x60;
    coord->coord.t[2] = 0;

    if (*(u32*)&actor->field_954 != 0x40000) {
        arg0->spawnArg1 = 0;
    }

    mode = arg0->spawnArg1 & 0xF;
    switch (mode) {
        case 0:
            if (coord->angle > 0) {
                coord->angle = coord->angle - 0x100;
            }
            break;
        case 1:
            if (coord->angle < 0x800) {
                coord->angle = coord->angle + 0x1C0;
            }
            break;
    }
    Gfx_RotMatrixZ(&coord->coord, coord->angle, 1);
}

void func_tonfa_baton_8011DB6C(Task* arg0)
{
    arg0->state = 3;
}

INCLUDE_RODATA("weapons/nonmatchings/tonfa_baton/tonfa_baton", D_tonfa_baton_8011D1C0);
