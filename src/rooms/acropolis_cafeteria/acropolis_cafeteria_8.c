#include "common.h"

#include "gameplay/268.h"
#include "gameplay/4CC.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "rooms/acropolis_cafeteria.h"

#include <psyq/abs.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

extern GsCOORDINATE2 Gfx_ViewCoord;

void func_acropolis_cafeteria_80181E3C(Task* arg0);

extern SVECTOR RoomsShared8017d830Delta;

extern MATRIX D_acropolis_cafeteria_8018D5A0;
extern MATRIX D_acropolis_cafeteria_8018D5C0;
extern MATRIX D_acropolis_cafeteria_8018D5E0;
extern MATRIX D_acropolis_cafeteria_8018D600;
extern MATRIX D_acropolis_cafeteria_8018D620;
extern MATRIX D_acropolis_cafeteria_8018D640;

void func_acropolis_cafeteria_801818DC(Task* task)
{
    TmdObject*                obj;
    GsCOORDINATE2*            coord;
    AcropolisCafeteriaDebris* work;
    GsCOORDINATE2*            player;

    obj   = (TmdObject*)task->extra;
    coord = obj->field_8;
    work  = Mem_Calloc(0xD8, 0);
    if (work == NULL) {
        Task_Kill(task);
        return;
    }
    task->idMap        = (TaskIdMap*)work;
    task->exitCallback = func_acropolis_cafeteria_80181E3C;
    task->state        = task->state + 1;
    Mem_Set(work, 0, 0xD8);
    coord->sub   = &Gfx_ViewCoord;
    coord->flg   = 0;
    obj->field_C = 0;
    RotMatrix(&work->field_C4, &coord->coord);
    work->field_B0     = (rand() & 0xFFF) + 0x3000;
    player             = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
    coord->coord.t[0]  = player->coord.t[0];
    coord->coord.t[1]  = player->coord.t[1] - 0x800;
    coord->coord.t[2]  = player->coord.t[2] + 0x800;
    work->obj.field_C  = work->slots;
    work->obj.field_18 = 0x50000;
    work->obj.field_1C = 0xFA;
    work->obj.field_8  = coord;
    work->obj.field_10 = 0;
    work->obj.field_12 = 0;
    work->obj.field_14 = 0;
    work->obj.flags    = 1;
    Gp_LinkObj(4, &work->obj);
    Gp_InitRec18Table(work->obj.field_C, 6, 0);
    work->obj.flags |= 0x8000;
}

void func_acropolis_cafeteria_80181A3C(Task* task)
{
    MATRIX*                   head;
    AcropolisCafeteriaDebris* work;
    GsCOORDINATE2*            coord;
    SVECTOR*                  direction;
    s32                       speed;

    head                      = *(MATRIX**)G_SCRATCH_HEAD;
    *(MATRIX**)G_SCRATCH_HEAD = head - 1;
    work                      = (AcropolisCafeteriaDebris*)task->idMap;
    coord                     = ((TmdObject*)task->extra)->field_8;
    work->field_B0--;
    coord->flg         = 0;
    coord->coord.t[1] += 0x80;
    Gp_UpdateCoord(coord);
    switch (work->field_D4) {
        case 0:
            if (Gp_FindRec18(work->obj.field_C, 0)) {
                work->field_D4++;
                head[-1]  = coord->coord;
                direction = &work->field_CC;
                Gfx_MatrixCol2(Wip_SysConfig.field_4, direction);
                VectorNormalSS(direction, direction);
                rand();
                speed          = work->field_B0;
                speed        >>= 1;
                speed          = (speed * speed) >> 6;
                work->field_B8 = -0x100;
                work->field_B4 = (work->field_CC.vx * speed) >> 24;
                work->field_BC = (work->field_CC.vz * speed) >> 24;
            }
            break;
        case 1:
            work->field_B8 += 0x10;
            if (work->field_B8 > 0) {
                work->field_B8 = 0;
                work->field_D4++;
            } else {
                work->field_C4.vx += (work->field_B0 >> 6) + (rand() & 0x7F);
                work->field_C4.vy += (work->field_B0 >> 6) + (rand() & 0x7F);
                work->field_C4.vz += (work->field_B0 >> 6) + (rand() & 0x7F);
            }
        case 2:
            work->field_B4 = (work->field_B4 * 6) / 7;
            if (ABS(work->field_B4) < 9) {
                work->field_B4 = 0;
            }
            work->field_BC = (work->field_BC * 6) / 7;
            if (ABS(work->field_BC) < 9) {
                work->field_BC = 0;
            }
            if ((work->field_B4 | work->field_BC) == 0) {
                work->field_D4++;
            }
            coord->coord.t[0] += work->field_B4;
            coord->coord.t[1] += work->field_B8;
            coord->coord.t[2] += work->field_BC;
            break;
        case 3:
            work->field_C4.vx = (work->field_C4.vx * 2) / 3;
            if (ABS(work->field_C4.vx) < 9) {
                work->field_C4.vx = 0;
            }
            work->field_C4.vz = (work->field_C4.vz * 2) / 3;
            if (ABS(work->field_C4.vz) < 9) {
                work->field_C4.vz = 0;
            }
            if (((u16)work->field_C4.vx | (u16)work->field_C4.vz) == 0) {
                work->field_D4 = 0;
            }
            break;
    }
    RotMatrix(&work->field_C4, &coord->coord);
    Gp_ClearRec18Occupied(work->slots);
    *(MATRIX**)G_SCRATCH_HEAD += 1;
}

void func_acropolis_cafeteria_80181E30(Task* arg0)
{
    arg0->state = 3;
}

void func_acropolis_cafeteria_80181E3C(Task* arg0)
{
    Gp_UnlinkObj(arg0->idMap);
    Task_Kill(arg0);
}
