#include "common.h"

#include "gameplay/268.h"
#include "gameplay/4CC.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/tmd.h"
#include "rooms/acropolis_cafeteria.h"

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

void func_acropolis_cafeteria_801827C4(Task* task)
{
    GpItemObj8* obj;
    TmdObject*  tmd;

    obj = (GpItemObj8*)task->spawnArg2;
    tmd = (TmdObject*)task->extra;
    if (Gp_GetCurBit2Flag(obj->field_8) != 2) {
        tmd->field_1C = &D_acropolis_cafeteria_8018D5C0;
        tmd->field_20 = &D_acropolis_cafeteria_8018D5A0;
        tmd->field_C  = 0;
    } else {
        tmd->field_C |= 0x80;
    }
    switch (Gp_GetViewIndex() & 0xFF) {
        case 0xC:
            tmd->field_E = 7;
            break;
        case 0x18:
            tmd->field_E = 4;
            break;
        default:
            tmd->field_E = -2;
            break;
    }
}
void func_acropolis_cafeteria_8018286C(Task* task)
{
    GpItemObj8* obj;
    TmdObject*  tmd;
    s32         flag;

    obj  = (GpItemObj8*)task->spawnArg2;
    tmd  = (TmdObject*)task->extra;
    flag = Gp_GetCurBit2Flag(obj->field_8);
    if ((Gp_GetViewIndex() & 0xFF) != 9) {
        tmd->field_C = 0x80;
        return;
    }
    if (obj->field_8 == 0xA) {
        Gfx_RotMatrixX(&((TmdObject*)task->extra)->field_8->coord, 0x400, 1);
    }
    tmd->field_1C = &D_acropolis_cafeteria_8018D600;
    tmd->field_20 = &D_acropolis_cafeteria_8018D5E0;
    if (flag == 2) {
        tmd->field_C &= 0xFFF7;
        Task_CallExit(task);
    } else {
        tmd->field_C = 8;
        tmd->field_E = 0;
        Tmd_AllocBuffers(tmd);
    }
}
void func_acropolis_cafeteria_80182954(Task* task)
{
    TmdObject* tmd;

    tmd = (TmdObject*)task->extra;
    if ((Gp_GetViewIndex() & 0xFF) != 9) {
        tmd->field_C = 0x80;
        return;
    }
    tmd->field_1C = &D_acropolis_cafeteria_8018D640;
    tmd->field_20 = &D_acropolis_cafeteria_8018D620;
    if (Gp_GetCurBit2Flag(0xA) == 2) {
        tmd->field_C |= 0x80;
    } else {
        tmd->field_C = 8;
        tmd->field_E = 0;
        Tmd_AllocBuffers(tmd);
    }
    Gfx_RotMatrixX(&((TmdObject*)task->extra)->field_8->coord, 0x400, 1);
}
INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_9", func_acropolis_cafeteria_80182A08);
