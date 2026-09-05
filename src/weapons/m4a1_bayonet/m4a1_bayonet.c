#include "common.h"

#include <psyq/inline_c.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"
#include "weapons/m4a1_bayonet.h"

/// Per-frame task for the M4A1 bayonet's blade trail. Nothing runs once the
/// room is fading (`Gp_State1C->field_4` non-zero); the task is then released
/// as soon as that phase reaches 4. State 0 places the tip frame at
/// `D_m4a1_bayonet_8011DEC8[0]` under the muzzle and the hilt frame at
/// `[1]` under it, then seeds all sixteen trail slots with that pose. State 1
/// re-poses both frames every frame, writes them into trail slot
/// `field_22 & 7`, re-runs the whole ring so the older slots follow their
/// parents, and hands the ribbon to `func_m4a1_bayonet_8011D69C`. The task
/// lives 13 frames.
void func_m4a1_bayonet_8011D1E4(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* slot;
    GsCOORDINATE2  hilt;
    s32            phase;
    SVECTOR*       vec;
    s32            vx;
    s32            vy;
    s32            vz;
    s32            i;
    s32            alive;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->field_8;
    phase = Gp_State1C->field_4;
    if (phase == 0) {
        work->field_22++;
        switch (task->state) {
            case 0:
                coord->sub        = work->field_8;
                coord->coord.t[0] = D_m4a1_bayonet_8011DEC8[0].vx;
                coord->coord.t[1] = D_m4a1_bayonet_8011DEC8[0].vy;
                coord->coord.t[2] = D_m4a1_bayonet_8011DEC8[0].vz;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                task->state = 1;

                vx              = D_m4a1_bayonet_8011DEC8[1].vx;
                vec             = &D_m4a1_bayonet_8011DEC8[1];
                vy              = vec->vy;
                vz              = vec->vz;
                hilt.sub        = coord;
                hilt.flg        = 0;
                hilt.coord.t[0] = vx;
                hilt.coord.t[1] = vy;
                hilt.coord.t[2] = vz;
                Gp_UpdateCoord(&hilt);

                for (i = 0; i < 8; i++) {
                    slot        = &D_m4a1_bayonet_8012D398[i];
                    slot->sub   = &Gfx_ViewCoord;
                    slot->workm = coord->workm;
                    gte_SetRotMatrix(&coord->workm);
                    gte_SetTransMatrix(&coord->workm);
                    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &slot->workm, &slot->coord);

                    slot        = &D_m4a1_bayonet_8012D618[i];
                    slot->sub   = &Gfx_ViewCoord;
                    slot->workm = hilt.workm;
                    gte_SetRotMatrix(&hilt.workm);
                    gte_SetTransMatrix(&hilt.workm);
                    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &slot->workm, &slot->coord);
                }
                break;
            case 1:
                coord->flg = 0;
                Gp_UpdateCoord(coord);

                hilt.sub        = work->field_8;
                hilt.flg        = 0;
                hilt.coord.t[0] = D_m4a1_bayonet_8011DED0.vx;
                hilt.coord.t[1] = D_m4a1_bayonet_8011DED0.vy;
                hilt.coord.t[2] = D_m4a1_bayonet_8011DED0.vz;
                Gp_UpdateCoord(&hilt);

                slot        = &D_m4a1_bayonet_8012D398[work->field_22 & 7];
                slot->sub   = &Gfx_ViewCoord;
                slot->workm = coord->workm;
                gte_SetRotMatrix(&coord->workm);
                gte_SetTransMatrix(&coord->workm);
                Gp_WorldToLocal(&Gfx_ViewCoord.workm, &slot->workm, &slot->coord);

                slot        = &D_m4a1_bayonet_8012D618[work->field_22 & 7];
                slot->sub   = &Gfx_ViewCoord;
                slot->workm = hilt.workm;
                gte_SetRotMatrix(&hilt.workm);
                gte_SetTransMatrix(&hilt.workm);
                Gp_WorldToLocal(&Gfx_ViewCoord.workm, &slot->workm, &slot->coord);

                for (i = 0; i < 8; i++) {
                    slot      = &D_m4a1_bayonet_8012D398[i];
                    slot->flg = 0;
                    Gp_UpdateCoord(slot);
                    slot      = &D_m4a1_bayonet_8012D618[i];
                    slot->flg = 0;
                    Gp_UpdateCoord(slot);
                }
                func_m4a1_bayonet_8011D69C(work->field_22 & 7, 0x112);
                break;
        }
        alive = work->field_22 < 0xD;
    } else {
        alive = phase < 4;
    }
    if (!alive) {
        Gp_ReleaseState1CMem(work, task);
    }
}

INCLUDE_ASM("weapons/nonmatchings/m4a1_bayonet/m4a1_bayonet", func_m4a1_bayonet_8011D69C);

INCLUDE_RODATA("weapons/nonmatchings/m4a1_bayonet/m4a1_bayonet", D_m4a1_bayonet_8011D1C0);

INCLUDE_ASM("weapons/nonmatchings/m4a1_bayonet/m4a1_bayonet", func_m4a1_bayonet_8011DA34);
