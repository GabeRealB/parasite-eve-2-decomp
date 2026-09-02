#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include <psyq/inline_c.h>
#include <psyq/libgs.h>

extern SVECTOR D_acropolis_cafeteria_80184E80[];
extern SVECTOR D_acropolis_cafeteria_80184E88;

void Room_Draw03(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, s32 arg2, s32 arg3);

void func_acropolis_cafeteria_801803AC(Task* task)
{
    GsCOORDINATE2  coord;
    GsCOORDINATE2* coords;
    GsCOORDINATE2* objCoord;
    GsCOORDINATE2* dst;
    GpEffWork*     work;
    SVECTOR*       vec;
    s32            i;

    coords   = (GsCOORDINATE2*)task->idMap;
    work     = (GpEffWork*)task->spawnArg2;
    objCoord = ((TmdObject*)task->extra)->field_8;

    if (Gp_State1C->field_4 < 2) {
        work->field_22++;
        switch (task->state) {
            case 0:
                coords = (GsCOORDINATE2*)Mem_Calloc(0x500, 0);
                if (coords == NULL) {
                    work->field_22 = 0;
                    return;
                }
                task->idMap          = (TaskIdMap*)coords;
                objCoord->sub        = work->field_8;
                objCoord->coord.t[0] = D_acropolis_cafeteria_80184E80[0].vx;
                objCoord->coord.t[1] = D_acropolis_cafeteria_80184E80[0].vy;
                objCoord->coord.t[2] = D_acropolis_cafeteria_80184E80[0].vz;
                objCoord->flg        = 0;
                Gp_UpdateCoord(objCoord);
                task->state      = 1;
                coord.sub        = work->field_8;
                vec              = &D_acropolis_cafeteria_80184E80[1];
                coord.coord.t[0] = vec->vx;
                coord.coord.t[1] = vec->vy;
                coord.coord.t[2] = vec->vz;
                coord.flg        = 0;
                Gp_UpdateCoord(&coord);
                for (i = 0; i < 8; i++) {
                    dst        = &coords[i];
                    dst->sub   = &Gfx_ViewCoord;
                    dst->workm = objCoord->workm;
                    gte_SetRotMatrix(&objCoord->workm);
                    gte_SetTransMatrix(&objCoord->workm);
                    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &dst->workm, &dst->coord);
                    dst        = &coords[i + 8];
                    dst->sub   = &Gfx_ViewCoord;
                    dst->workm = coord.workm;
                    gte_SetRotMatrix(&coord.workm);
                    gte_SetTransMatrix(&coord.workm);
                    Gp_WorldToLocal(&Gfx_ViewCoord.workm, &dst->workm, &dst->coord);
                }
                break;

            case 1:
                objCoord->flg = 0;
                Gp_UpdateCoord(objCoord);
                coord.sub        = work->field_8;
                coord.coord.t[0] = D_acropolis_cafeteria_80184E88.vx;
                coord.coord.t[1] = D_acropolis_cafeteria_80184E88.vy;
                coord.coord.t[2] = D_acropolis_cafeteria_80184E88.vz;
                coord.flg        = 0;
                Gp_UpdateCoord(&coord);
                dst        = &coords[work->field_22 & 7];
                dst->sub   = &Gfx_ViewCoord;
                dst->workm = objCoord->workm;
                gte_SetRotMatrix(&objCoord->workm);
                gte_SetTransMatrix(&objCoord->workm);
                Gp_WorldToLocal(&Gfx_ViewCoord.workm, &dst->workm, &dst->coord);
                dst        = &coords[(work->field_22 & 7) + 8];
                dst->sub   = &Gfx_ViewCoord;
                dst->workm = coord.workm;
                gte_SetRotMatrix(&coord.workm);
                gte_SetTransMatrix(&coord.workm);
                Gp_WorldToLocal(&Gfx_ViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst      = &coords[i];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                    dst      = &coords[i + 8];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                }
                Room_Draw03(coords, &coords[8], work->field_22 & 7, 0x123);
                if (work->field_22 == task->spawnArg1 && work->field_22 != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}